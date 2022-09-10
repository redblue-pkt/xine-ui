/*
 * Copyright (C) 2000-2022 the xine project
 *
 * This file is part of xine, a unix video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * xine panel related stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "common.h"
#include "panel.h"
#include "control.h"
#include "event_sender.h"
#include "menus.h"
#include "mrl_browser.h"
#include "playlist.h"
#include "setup.h"
#include "snapshot.h"
#include "stream_infos.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/slider.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/button_list.h"
#include "xine-toolkit/skin.h"

typedef enum {
  /* keep order */
  _W_prev = 0,
  _W_stop,
  _W_play,
  _W_pause,
  _W_next,
  _W_slider_play,
  /* /keep order */
  _W_eject,
  _W_slider_mixer,
  _W_mute,
  _W_LAST
} _W_t;

struct xui_panel_st {
  gGui_t               *gui;
  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;

  int                   x;
  int                   y;
  int                   skin_on_change;

  xitk_widget_t        *title_label;
  xitk_widget_t        *runtime_label;
  int                   runtime_mode;

  xitk_widget_t        *w[_W_LAST];

  struct {
    unsigned int        slider_play_time;
    int                 enabled;
  } playback_widgets;

  struct {
    int                 enable;
    unsigned long       timeout;
  } tips;

  int                   visible;
  char                  runtime[20];
  xitk_widget_t        *audiochan_label;
  xitk_widget_t        *autoplay_buttons;
  xitk_widget_t        *spuid_label;
  xitk_register_key_t   widget_key;

  struct {
    pthread_mutex_t     mutex;
    pthread_cond_t      wake;
    pthread_t           thread;
    int                 run;
    stream_infos_ident_t *ident;
    xitk_vers_string_t  istr;
  }                     sl;

  /* private vars to avoid useless updates */
  unsigned int          shown_time;
  unsigned int          shown_length;

  int                   logo_synthetic;
};

void panel_update_nextprev_tips (xui_panel_t *panel) {

  if (panel->gui->skip_by_chapter) {
    xitk_set_widget_tips(panel->w[_W_prev], _("Play previous chapter or mrl"));
    xitk_set_widget_tips(panel->w[_W_next], _("Play next chapter or mrl"));
  }
  else {
    xitk_set_widget_tips(panel->w[_W_prev], _("Play previous entry from playlist"));
    xitk_set_widget_tips(panel->w[_W_next], _("Play next entry from playlist"));
  }
}

int is_playback_widgets_enabled (xui_panel_t *panel) {
  return (panel->playback_widgets.enabled == 1);
}

void enable_playback_controls (xui_panel_t *panel, int enable) {
  if (!panel)
    return;
  panel->playback_widgets.enabled = enable;
  /* prev, stop, play, pause, next, slider_play */
  xitk_widgets_state (panel->w + _W_prev, 6, XITK_WIDGET_STATE_ENABLE, enable ? ~0u : 0);
}

/*
 *
 */
static void panel_show_tips (xui_panel_t *panel) {
  xitk_set_tips_timeout (panel->gui->xitk, panel->tips.enable ? panel->tips.timeout : XITK_TIPS_TIMEOUT_OFF);
}

/* Somewhat paranoia conditionals (Hans, YOU're paranoid ;-) )*/
/*
int panel_get_tips_enable (xui_panel_t *panel) {
  return (panel) ? panel->tips.enable : 0;
}
unsigned long panel_get_tips_timeout (xui_panel_t *panel) {
  return (panel) ? panel->tips.timeout : 0;
}
*/

/*
 * Config callbacks.
 */
static void panel_enable_tips_cb(void *data, xine_cfg_entry_t *cfg) {
  xui_panel_t *panel = data;
  panel->tips.enable = cfg->num_value;
  panel_show_tips (panel);
}
static void panel_timeout_tips_cb(void *data, xine_cfg_entry_t *cfg) {
  xui_panel_t *panel = data;
  panel->tips.timeout = (unsigned long) cfg->num_value;
  panel_show_tips (panel);
}

/*
 * Toolkit event handler will call this function with new
 * coords of panel window.
 */
static void panel_store_new_position (void *data, const xitk_rect_t *wr) {
  xui_panel_t *panel = data;

  if(panel->skin_on_change == 0) {

    panel->x = wr->x;
    panel->y = wr->y;
    /*
    config_update_num (panel->gui->xine, "gui.panel_x", wr->x);
    config_update_num (panel->gui->xine, "gui.panel_y", wr->y); */

    event_sender_move (panel->gui, wr->x + wr->width, wr->y);
  }
  else
    panel->skin_on_change--;

}

static void _panel_exit (xui_panel_t *panel) {

#ifdef HAVE_XINE_CONFIG_UNREGISTER_CALLBACKS
  xine_config_unregister_callbacks (panel->gui->xine, NULL, NULL, panel, sizeof (*panel));
#endif
  gui_save_window_pos (panel->gui, "panel", panel->widget_key);
  xitk_unregister_event_handler (panel->gui->xitk, &panel->widget_key);

  pthread_mutex_lock (&panel->sl.mutex);
  panel->sl.run = 0;
  pthread_cond_signal (&panel->sl.wake);
  pthread_mutex_unlock (&panel->sl.mutex);
  pthread_join (panel->sl.thread, NULL);
  pthread_cond_destroy (&panel->sl.wake);
  pthread_mutex_destroy (&panel->sl.mutex);

  panel->title_label = NULL;
  xitk_window_destroy_window (panel->xwin);
  panel->xwin = NULL;
  /* xitk_dlist_init (&panel->widget_list->list); */

  panel->gui->panel = NULL;
  free (panel);
}

/*
 * Change displayed logo, if selected skin want to customize it.
 */
static void _update_logo (xui_panel_t *panel) {
  xine_cfg_entry_t     cfg_entry;
  const char          *skin_logo;
  int                  cfg_err_result;

  cfg_err_result = xine_config_lookup_entry (panel->gui->xine, "gui.logo_mrl", &cfg_entry);
  skin_logo = xitk_skin_get_logo (panel->gui->skin_config);

  if(skin_logo) {

    if((cfg_err_result) && cfg_entry.str_value) {
      /* Old and new logo are same, don't reload */
      if(!strcmp(cfg_entry.str_value, skin_logo))
	goto __done;
    }

    config_update_string (panel->gui->xine, "gui.logo_mrl", skin_logo);
    goto __play_logo_now;

  }
  else { /* Skin don't use logo feature, set to xine's default */

    /*
     * Back to default logo only on a skin
     * change, not at the first skin loading.
     **/
#ifdef XINE_LOGO2_MRL
#  define USE_XINE_LOGO_MRL XINE_LOGO2_MRL
#else
#  define USE_XINE_LOGO_MRL XINE_LOGO_MRL
#endif
    if (panel->logo_synthetic && (cfg_err_result) && (strcmp (cfg_entry.str_value, USE_XINE_LOGO_MRL))) {
        config_update_string (panel->gui->xine, "gui.logo_mrl", USE_XINE_LOGO_MRL);

    __play_logo_now:

      sleep(1);

      if (panel->gui->logo_mode) {
        if (xine_get_status (panel->gui->stream) == XINE_STATUS_PLAY) {
          panel->gui->ignore_next = 1;
          xine_stop (panel->gui->stream);
          panel->gui->ignore_next = 0;
	}
        if (panel->gui->display_logo) {
          if ((!xine_open (panel->gui->stream, panel->gui->logo_mrl))
            || (!xine_play (panel->gui->stream, 0, 0))) {
            gui_handle_xine_error (panel->gui, panel->gui->stream, panel->gui->logo_mrl);
	    goto __done;
	  }
	}
        panel->gui->logo_mode = 1;
      }
    }
  }

 __done:
  panel->gui->logo_has_changed--;
}

/*
 * Change the current skin.
 */
void panel_change_skins (xui_panel_t *panel, int synthetic) {
  xitk_image_t *bg_image;

  panel->logo_synthetic = !!synthetic;
  panel->gui->logo_has_changed++;

  panel->skin_on_change++;

  xitk_hide_widgets(panel->widget_list);
  xitk_skin_lock (panel->gui->skin_config);

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (panel->gui->skin_config, "BackGround");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    gui_msg (panel->gui, XUI_MSG_ERROR, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
    exit(-1);
  }

  xitk_window_set_background_image (panel->xwin, bg_image);

  video_window_set_transient_for (panel->gui->vwin, panel->xwin);

  if (panel_is_visible (panel) > 1)
    raise_window (panel->gui, panel->xwin, 1, 1);

  xitk_skin_unlock (panel->gui->skin_config);

  xitk_change_skins_widget_list (panel->widget_list, panel->gui->skin_config);

  enable_playback_controls (panel, panel->playback_widgets.enabled);
  xitk_paint_widget_list(panel->widget_list);

  event_sender_move (panel->gui, panel->x + xitk_image_width (bg_image), panel->y);
}

/*
 * Update runtime displayed informations.
 */
static void secs2str (char *s, unsigned int secs) {
  /* Avoid slow snprintf (). Yes this does not support south asian */
  /* alternative digits, but neither does the widget bitmap font.  */
  /* But wait -- what if you modify letters.png now ?              */
  if (secs >= 24 * 60 * 60) { /* [  dd::hh] */
    unsigned int days;
    secs /= 3600u;
    days = secs / 24u;
    secs %= 24u;
    memcpy (s, "    ::  ", 8);
    s[8] = 0;
    s[7] = (secs % 10u) + '0';
    s[6] = (secs / 10u) + '0';
    s[3] = (days % 10u) + '0';
    days /= 10u;
    if (days)
      s[2] = days + '0';
  } else { /* [hh:mm:ss] */
    s[8] = 0;
    s[7] = (secs % 10u) + '0';
    secs /= 10u;
    s[6] = (secs % 6u) + '0';
    secs /= 6u;
    s[5] = ':';
    s[4] = (secs % 10u) + '0';
    secs /= 10u;
    s[3] = (secs % 6u) + '0';
    secs /= 6u;
    s[2] = ':';
    s[1] = (secs % 10u) + '0';
    s[0] = (secs / 10u) + '0';
  }
}

int panel_update_runtime_display (xui_panel_t *panel) {
  int seconds, pos, length;
  unsigned int step = 500;
  char timestr[16];

  if (panel_is_visible (panel) < 2)
    return step;

  if (!gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, &seconds, &length)) {

    if ((panel->gui->stream_length.pos || panel->gui->stream_length.time) && panel->gui->stream_length.length) {
      pos     = panel->gui->stream_length.pos;
      seconds = panel->gui->stream_length.time;
      length  = panel->gui->stream_length.length;
    }
    else
      return step;

  }

  do {
    int speed;
    unsigned int msecs = seconds, secs;
    if (!(pos || msecs)) { /* are you sure there is something to play? */
      panel->shown_time = ~3;
      strcpy (timestr, "--:--:--");
      break;
    }
    if (panel->runtime_mode) { /* show remaining time */
      if (msecs > (unsigned int)length) {
        panel->shown_time = ~3;
        strcpy (timestr, "  :  :  ");
        break;
      }
      msecs = length - msecs;
    }
    if (msecs == panel->shown_time) {
      timestr[0] = 0;
      break;
    }
    panel->shown_time = msecs;
    speed = xine_get_param (panel->gui->stream, XINE_PARAM_FINE_SPEED);
    if (speed > XINE_FINE_SPEED_NORMAL * 3 / 4) {
      /* find a stream step that is a multiple of, and plays roughly, 0.5s. */
      step = 500u * (((unsigned int)speed + (XINE_FINE_SPEED_NORMAL >> 1)) / XINE_FINE_SPEED_NORMAL);
      /* allow up to 4 updates/s on fast play. */
      if (step > 1000)
        step >>= 1;
      /* round display value. */
      secs = (msecs + 250u) / 1000u;
      /* preferred next stop stream time. */
      step = panel->runtime_mode
           ? msecs + step - (msecs + (step >> 1)) / step * step
           : step + (msecs + (step >> 1)) / step * step - msecs;
      /* and in real time. */
      step = step * XINE_FINE_SPEED_NORMAL / speed;
      secs2str (timestr, secs);
      /* printf ("xine-ui: msecs=%u, step=%u.\n", msecs, step); */
    } else {
      secs   = msecs / 1000u;
      msecs %= 1000u;
      secs2str (timestr, secs);
      /* millisecond information is useful when editing. since we only update */
      /* twice a second, show this in slow mode only. */
      timestr[12] = 0;
      timestr[11] = (msecs % 10u) + '0';
      msecs /= 10u;
      timestr[10] = (msecs % 10u) + '0';
      timestr[9] = (msecs / 10u) + '0';
      timestr[8] = '.';
    }
  } while (0);

  if ((unsigned int)length != panel->shown_length) {
    char buffer[256];
    const char *ts = _("Total time: ");
    size_t tl = strlen (ts);
    if (tl > sizeof (buffer) - 9)
      tl = sizeof (buffer) - 9;
    memcpy (buffer, ts, tl);
    panel->shown_length = length;
    if (length > 0)
      secs2str (buffer + tl, length / 1000);
    else
      strcpy (buffer + tl, "??:??:??");
    xitk_set_widget_tips (panel->runtime_label, buffer);
  }

  if (timestr[0])
    xitk_label_change_label (panel->runtime_label, timestr);

  return step;
}

static void panel_check_mute (xui_panel_t *panel) {
  xitk_button_set_state (panel->w[_W_mute], panel->gui->mixer.mute);
}

/*
 * Update slider thread.
 */
static void *slider_loop (void *data) {
  xui_panel_t *panel = data;
  int screensaver_timer = 0, lastmsecs = -1, i = -1, step = 0;
  struct timespec wtime = {0, 0};

  xitk_vers_string_init (&panel->sl.istr, NULL, 0);
  panel->sl.ident = stream_infos_ident_new ();

  while (1) {
    i++;
    if (step > 0) {
      wtime.tv_nsec += step * 1000000;
      if (wtime.tv_nsec >= 1000000000) {
        wtime.tv_nsec -= 1000000000;
        wtime.tv_sec += 1;
      }
      pthread_mutex_lock (&panel->sl.mutex);
      if (!panel->sl.run) {
        pthread_mutex_unlock (&panel->sl.mutex);
        break;
      }
      pthread_cond_timedwait (&panel->sl.wake, &panel->sl.mutex, &wtime);
      pthread_mutex_unlock (&panel->sl.mutex);
    }

#if _POSIX_TIMERS > 0
    clock_gettime (CLOCK_REALTIME, &wtime);
#else
    {
      struct timeval tv = {0, 0};
      gettimeofday (&tv, NULL);
      wtime.tv_sec = tv.tv_sec;
      wtime.tv_nsec = tv.tv_usec * 1000;
    }
#endif
    step = 500;

    if (panel->gui->stream) {

      int status = xine_get_status (panel->gui->stream);
      int speed  = xine_get_param (panel->gui->stream, XINE_PARAM_SPEED);

      int pos = 0, msecs = 0;

      if(status == XINE_STATUS_PLAY) {
        int seeking;
        pthread_mutex_lock (&panel->gui->seek_mutex);
        seeking = panel->gui->seek_running;
        pthread_mutex_unlock (&panel->gui->seek_mutex);
        if (!seeking && gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, &msecs, NULL)) {

          /* pthread_mutex_lock (&panel->gui->xe_mutex); */

          if (panel->gui->playlist.num && panel->gui->playlist.cur >= 0 && panel->gui->playlist.mmk &&
            panel->gui->playlist.mmk[panel->gui->playlist.cur] && panel->gui->mmk.end != -1) {
            if ((msecs / 1000) >= panel->gui->playlist.mmk[panel->gui->playlist.cur]->end) {
              panel->gui->ignore_next = 0;
              gui_playlist_start_next (panel->gui);
              /* pthread_mutex_unlock (&panel->gui->xe_mutex); */
              continue;
	    }
	  }

          if (panel->gui->playlist.num) {
            if ((xine_get_stream_info (panel->gui->stream, XINE_STREAM_INFO_SEEKABLE))) {
	      if(!xitk_is_widget_enabled(panel->w[_W_slider_play]))
                xitk_widgets_state (panel->w + _W_slider_play, 1, XITK_WIDGET_STATE_ENABLE, ~0u);
	    }
	    else {
	      if(xitk_is_widget_enabled(panel->w[_W_slider_play])) {
		xitk_slider_reset(panel->w[_W_slider_play]);
                xitk_widgets_state (panel->w + _W_slider_play, 1, XITK_WIDGET_STATE_ENABLE, 0);
	      }
	    }
	  }

          /* pthread_mutex_unlock (&panel->gui->xe_mutex); */
	}
	else
	  pos = -1;

      }

      if(!(i % 2)) {
        osd_update (panel->gui);

        if (panel->gui->mrl_overrided) {
          panel->gui->mrl_overrided--;
          if (panel->gui->mrl_overrided == 0)
            panel_update_mrl_display (panel);
	}
      }

      if ((status == XINE_STATUS_PLAY) &&
        ((speed != XINE_SPEED_PAUSE) || (msecs != lastmsecs))) {

        if (stream_infos_ident_get (panel->sl.ident, &panel->sl.istr, panel->gui->stream)) {
          if (!pthread_mutex_trylock (&panel->gui->mmk_mutex)) {
            if (panel->sl.istr.s && panel->sl.istr.s[0]) {
              if (gui_playlist_set_str_val (panel->gui, panel->sl.istr.s, MMK_VAL_IDENT, GUI_MMK_CURRENT)) {
                mediamark_t *m = &panel->gui->mmk;
                mediamark_set_str_val (&m, panel->sl.istr.s, MMK_VAL_IDENT);
                gui_playlist_unlock (panel->gui);
                video_window_set_mrl (panel->gui->vwin, panel->sl.istr.s);
                playlist_mrlident_toggle (panel->gui);
                panel_update_mrl_display (panel);
              } else {
                gui_playlist_unlock (panel->gui);
              }
            } else {
              video_window_set_mrl (panel->gui->vwin, panel->gui->mmk.mrl);
              gui_playlist_unlock (panel->gui);
            }
          }
        }

        {
          if (video_window_is_visible (panel->gui->vwin) > 1) {
            if (panel->gui->ssaver_timeout) {
              if (!(i % 2))
                screensaver_timer++;
              if (screensaver_timer >= panel->gui->ssaver_timeout)
                screensaver_timer = video_window_reset_ssaver (panel->gui->vwin);
            }
          }
        }

        if (panel->gui->logo_mode == 0) {

          if (panel_is_visible (panel) > 1) {

            step = panel_update_runtime_display (panel);

	    if(xitk_is_widget_enabled(panel->w[_W_slider_play])) {
	      if(pos >= 0)
		xitk_slider_set_pos(panel->w[_W_slider_play], pos);
	    }

	    if(!(i % 20)) {
	      int level = 0;

              panel_update_channel_display (panel);

              if ((panel->gui->mixer.method == SOUND_CARD_MIXER) &&
                (panel->gui->mixer.caps & MIXER_CAP_VOL)) {
                level = panel->gui->mixer.volume_level =
                  xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_VOLUME);
	      }
              else if (panel->gui->mixer.method == SOFTWARE_MIXER) {
                level = panel->gui->mixer.amp_level =
                  xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);
	      }

	      xitk_slider_set_pos (panel->w[_W_slider_mixer], level);
              panel_check_mute (panel);

	      i = 0;
	    }

	  }

          if (stream_infos_is_visible (panel->gui->streaminfo) && panel->gui->stream_info_auto_update)
            stream_infos_update_infos (panel->gui->streaminfo);

	}
      }
      lastmsecs = msecs;
    }

    if (panel->gui->cursor_visible) {
      if(!(i % 2))
	video_window_set_cursor_timer (panel->gui->vwin, video_window_get_cursor_timer (panel->gui->vwin) + 1);

      if (video_window_get_cursor_timer (panel->gui->vwin) >= 5) {
        panel->gui->cursor_visible = !panel->gui->cursor_visible;
        video_window_set_cursor_visibility (panel->gui->vwin, panel->gui->cursor_visible);
      }
    }

    if (panel->gui->logo_has_changed)
      _update_logo (panel);
  }

  stream_infos_ident_delete (&panel->sl.ident);
  xitk_vers_string_deinit (&panel->sl.istr);

  return NULL;
}

static int _panel_get_visibility (xui_panel_t *panel) {
  uint32_t flags = xitk_window_flags (panel->xwin, 0, 0);

  panel->visible = (flags & XITK_WINF_VISIBLE) ? 2 : (flags & XITK_WINF_ICONIFIED) ? 1 : 0;
  return panel->visible;
}

/*
 * Boolean about panel visibility.
 */
int panel_is_visible (xui_panel_t *panel) {
  if (!panel)
    return 0;
  return _panel_get_visibility (panel);
}

/*
 * Show/Hide panel window.
 */
static void _panel_toggle_visibility (xui_panel_t *panel) {

  _panel_get_visibility (panel);
  if ((panel->visible > 1) && !video_window_is_separate_display (panel->gui->vwin)) {
    int vv = video_window_is_visible (panel->gui->vwin);

    if ((vv > 0) && !panel->gui->use_root_window) {
      panel->visible = 0;
      xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
      xitk_hide_widgets (panel->widget_list);
    } else {
      panel->visible = 1;
      xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_ICONIFIED);
    }
    video_window_grab_input_focus(panel->gui->vwin);

  } else {

    panel->visible = 2;
    panel->gui->nongui_error_msg = NULL;
    xitk_show_widgets (panel->widget_list);
    xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    if (panel->gui->cursor_grabbed)
      video_window_ungrab_pointer(panel->gui->vwin);

    if ((video_window_get_fullscreen_mode (panel->gui->vwin) & (WINDOWED_MODE | FULLSCR_MODE))
        && (!((video_window_get_fullscreen_mode (panel->gui->vwin)) & FULLSCR_XI_MODE))) {
      xitk_rect_t wr = {0, 0, 0, 0};
      int desktopw, desktoph;

      xitk_window_get_window_position (panel->xwin, &wr);

      xitk_get_display_size (panel->gui->xitk, &desktopw, &desktoph);

      if (((wr.x + wr.width) <= 0) || ((wr.y + wr.height) <= 0) || (wr.x >= desktopw) || (wr.y >= desktoph)) {
        wr.x = (desktopw - wr.width) >> 1;
        wr.y = (desktoph - wr.height) >> 1;
        panel_store_new_position (panel, &wr);
        wr.width = wr.height = XITK_INT_KEEP;
        xitk_window_move_resize (panel->xwin, &wr);
      }
    }

    if (panel->gui->logo_mode == 0) {
      int pos;

      if(xitk_is_widget_enabled(panel->w[_W_slider_play])) {
        if (gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, NULL, NULL))
	  xitk_slider_set_pos(panel->w[_W_slider_play], pos);
        panel_update_runtime_display (panel);
      }

      panel_update_mrl_display (panel);
    }

  }
}

void panel_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;
  if (!panel)
    return;
  _panel_toggle_visibility (panel);
  config_update_num (panel->gui->xine, "gui.panel_visible", panel->visible > 1);
}

void panel_raise_window (xui_panel_t *panel) {
  if (panel_is_visible (panel) > 1)
    xitk_window_raise_window (panel->xwin);
}

void panel_get_window_position (xui_panel_t *panel, xitk_rect_t *r) {
  if (panel)
    xitk_window_get_window_position (panel->xwin, r);
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause (xui_panel_t *panel) {

  if (!panel)
    return;

  xitk_button_set_state (panel->w[_W_pause],
    (((xine_get_status (panel->gui->stream) == XINE_STATUS_PLAY) &&
      (xine_get_param (panel->gui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)) ? 1 : 0));
}

static void panel_reset_runtime_label (xui_panel_t *panel) {
  xitk_label_change_label (panel->runtime_label, panel->runtime_mode == 0 ? "00:00:00" : "..:..:..");
}

static void _panel_change_display_mode(xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;
  panel->gui->is_display_mrl = !panel->gui->is_display_mrl;
  panel_update_mrl_display (panel);
  playlist_mrlident_toggle (panel->gui);
}

static void _panel_change_time_label(xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;
  panel->runtime_mode = !panel->runtime_mode;
  panel_update_runtime_display (panel);
}

/*
 * Reset the slider of panel window (set to 0).
 */
void panel_reset_slider (xui_panel_t *panel) {
  if (!panel)
    return;
  xitk_slider_reset(panel->w[_W_slider_play]);
  panel_reset_runtime_label (panel);
}

void panel_update_slider (xui_panel_t *panel, int pos) {
  if (!panel)
    return;
  xitk_slider_set_pos(panel->w[_W_slider_play], pos);
}

/*
 * Update audio/spu channel displayed informations.
 */
void panel_update_channel_display (xui_panel_t *panel) {
  int   channel;
  char  buffer[XINE_LANG_MAX];
  const char *lang = NULL;

  if (!panel)
    return;

  memset(&buffer, 0, sizeof(buffer));
  channel = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  switch (channel) {
  case -2:
    lang = "off";
    break;

  case -1:
    if (!xine_get_audio_lang (panel->gui->stream, channel, &buffer[0]))
      lang = "auto";
    else
      lang = buffer;
    break;

  default:
    if (!xine_get_audio_lang (panel->gui->stream, channel, &buffer[0]))
      snprintf(buffer, sizeof(buffer), "%3d", channel);
    lang = buffer;
    break;
  }
  xitk_label_change_label (panel->audiochan_label, lang);

  memset(&buffer, 0, sizeof(buffer));
  channel = xine_get_param (panel->gui->stream, XINE_PARAM_SPU_CHANNEL);
  switch (channel) {
  case -2:
    lang = "off";
    break;

  case -1:
    if (!xine_get_spu_lang (panel->gui->stream, channel, &buffer[0]))
      lang = "auto";
    else
      lang = buffer;
    break;

  default:
    if (!xine_get_spu_lang (panel->gui->stream, channel, &buffer[0]))
      snprintf(buffer, sizeof(buffer), "%3d", channel);
    lang = buffer;
    break;
  }
  xitk_label_change_label (panel->spuid_label, lang);
}

static void panel_audio_lang_list(xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;
  int x, y;
  xitk_rect_t wr = {0, 0, 0, 0};

  (void)w;
  xitk_window_get_window_position (panel->xwin, &wr);
  xitk_get_widget_pos(panel->audiochan_label, &x, &y);
  x += wr.x;
  y += (wr.y + xitk_get_widget_height(panel->audiochan_label));
  audio_lang_menu (panel->gui, panel->widget_list, x, y);
}

static void panel_spu_lang_list(xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;
  xitk_rect_t wr = {0, 0, 0, 0};
  int x, y;

  (void)w;
  xitk_window_get_window_position (panel->xwin, &wr);
  xitk_get_widget_pos(panel->spuid_label, &x, &y);
  x += wr.x;
  y += (wr.y + xitk_get_widget_height(panel->spuid_label));
  spu_lang_menu (panel->gui, panel->widget_list, x, y);
}

/*
 * Update displayed MRL according to the current one.
 */
void panel_update_mrl_display (xui_panel_t *panel) {
  if (panel) {
    gui_playlist_lock (panel->gui);
    panel_set_title (panel, (panel->gui->is_display_mrl) ? panel->gui->mmk.mrl : panel->gui->mmk.ident);
    gui_playlist_unlock (panel->gui);
  }
}

void panel_update_mixer_display (xui_panel_t *panel) {
  if (panel) {
    gGui_t *gui = panel->gui;
    int max = 100, vol = 0;

    switch(gui->mixer.method) {
    case SOUND_CARD_MIXER:
      max = 100;
      vol = gui->mixer.volume_level;
      break;
    case SOFTWARE_MIXER:
      max = 200;
      vol = gui->mixer.amp_level;
      break;
    }

    xitk_slider_set_range (panel->w[_W_slider_mixer], 0, max, max / 50);
    xitk_slider_set_pos (panel->w[_W_slider_mixer], vol);
    xitk_button_set_state (panel->w[_W_mute], gui->mixer.mute);
  }
}

/*
 *
 */
void panel_toggle_audio_mute(xitk_widget_t *w, void *data, int state) {
  xui_panel_t *panel = data;

  (void)w;
  if (panel->gui->mixer.method == SOFTWARE_MIXER) {
    panel->gui->mixer.mute = state;
    xine_set_param (panel->gui->stream, XINE_PARAM_AUDIO_AMP_MUTE, panel->gui->mixer.mute);
    osd_display_info (panel->gui, _("Amp: %s"), panel->gui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  else if (panel->gui->mixer.caps & MIXER_CAP_MUTE) {
    panel->gui->mixer.mute = state;
    xine_set_param (panel->gui->stream, XINE_PARAM_AUDIO_MUTE, panel->gui->mixer.mute);
    osd_display_info (panel->gui, _("Audio: %s"), panel->gui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  panel_check_mute (panel);
}

/*
 *  Use internal xine-lib framegrabber function
 *  to snapshot current frame.
 */
static void panel_snapshot_error(void *data, char *message) {
  gGui_t *gui = data;
  gui_msg (gui, XUI_MSG_ERROR, "%s", message);
}
static void panel_snapshot_info(void *data, char *message) {
  gGui_t *gui = data;
  gui_msg (gui, XUI_MSG_INFO, "%s", message);
}
void panel_snapshot (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;
  gui_playlist_lock (panel->gui);
  create_snapshot (panel->gui, panel->gui->mmk.mrl, panel_snapshot_error, panel_snapshot_info, panel->gui);
  gui_playlist_unlock (panel->gui);
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_play_cb (xitk_widget_t *w, void *data, int pos) {
  xui_panel_t *panel = data;
  unsigned int stime;
  struct timeval tv = {0, 0};

  (void)w;
  /* Update ui gfx smoothly, at most 20 times a second (~ 5 times less than before).
   * There is hardly a visible difference, but a lot less X action - especially
   * with KDE window thumbnails on. Slider thread will fix the final position. */
  gettimeofday (&tv, NULL);
  stime = tv.tv_usec / (1000000 / 20) + tv.tv_sec * 20;
  /* printf ("panel_slider_play_cb: timeslot #%u.\n", stime); */
  if (xitk_is_widget_enabled (panel->w[_W_slider_play])) {
    gui_set_current_position (panel->gui, pos);
    if (stime != panel->playback_widgets.slider_play_time) {
      panel->playback_widgets.slider_play_time = stime;
      if (xine_get_status (panel->gui->stream) != XINE_STATUS_PLAY) {
        panel_reset_slider (panel);
      } else {
        /*
        int pos;
        if (gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, NULL, NULL))
          xitk_slider_set_pos (panel->w[_W_slider_play], pos); */
        panel_update_runtime_display (panel);
      }
    }
  }
  panel_check_pause (panel);
}

static void panel_slider_mixer_cb (xitk_widget_t *w, void *data, int pos) {
  xui_panel_t *panel = data;

  (void)w;
  if (panel->gui->mixer.method == SOUND_CARD_MIXER)
    gui_set_audio_vol (panel->gui, pos);
  else if (panel->gui->mixer.method == SOFTWARE_MIXER)
    gui_set_amp_level (panel->gui, pos);
}

/*
 * Handle X events here.
 */
static int panel_event (void *data, const xitk_be_event_t *e) {
  xui_panel_t *panel = data;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      gui_exit (NULL, panel->gui);
      return 1;
    case XITK_EV_BUTTON_DOWN:
      if (e->code == 3) {
        video_window_menu (panel->gui, panel->widget_list, e->w, e->h);
        return 1;
      }
      break;
    case XITK_EV_POS_SIZE:
      event_sender_move (panel->gui, e->x + e->w, e->y);
      return 1;
    case XITK_EV_KEY_DOWN:
      if ((e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_MENU)) {
        video_window_menu (panel->gui, panel->widget_list, e->w, e->h);
        return 1;
      }
      break;
    default: ;
  }
  return gui_handle_be_event (panel->gui, e);
}

/*
 * Create buttons from information if input plugins autoplay featured.
 * We couldn't do this into panel_init(), this function is
 * called before xine engine initialization.
 */
void panel_add_autoplay_buttons (xui_panel_t *panel) {
  const char * const *autoplay_plugins;

  if (!panel)
    return;

  autoplay_plugins = xine_get_autoplay_input_plugin_ids (panel->gui->xine);
  if (autoplay_plugins) {
    const char *tips[64];
    unsigned int i;

    for (i = 0; autoplay_plugins[i]; i++) {
      if (i >= sizeof (tips) / sizeof (tips[0]))
        break;
      tips[i] = xine_get_input_plugin_description (panel->gui->xine, autoplay_plugins[i]);
    }
    panel->autoplay_buttons = xitk_button_list_new (
      panel->widget_list,
      panel->gui->skin_config, "AutoPlayGUI",
      playlist_scan_input, panel->gui,
      autoplay_plugins,
      tips, XITK_TIPS_TIMEOUT_AUTO, 0);
    if (panel->autoplay_buttons)
      xitk_add_widget (panel->widget_list, panel->autoplay_buttons, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (panel->autoplay_buttons, _("More sources..."));
  }

  /* show panel (see panel_init ()) */
  {
    int visible = xine_config_register_bool (panel->gui->xine, "gui.panel_visible", 1,
      _("gui panel visibility"),
      CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);

    panel->visible = (!panel->gui->no_gui) ? visible : 0;
  }
  /*  The user don't want panel on startup */
  if (panel->visible && (actions_on_start (panel->gui->actions_on_start, ACTID_TOGGLE_VISIBLITY))) {
    panel->visible = 0;
    config_update_num (panel->gui->xine, "gui.panel_visible", 0);
  }
  if (panel->gui->use_root_window || video_window_is_separate_display (panel->gui->vwin))
    panel->visible = 1;
  if (panel->visible) {
    panel->visible = 2;
    reparent_window (panel->gui, panel->xwin);
    /* already called by reparent_window ()
    xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    xitk_window_raise_window (panel->xwin);
    xitk_window_try_to_set_input_focus (panel->xwin);
    */
  } else {
    if ((video_window_is_visible (panel->gui->vwin) > 0) && !panel->gui->use_root_window) {
      xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
      xitk_hide_widgets (panel->widget_list);
    } else {
      xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_ICONIFIED);
    }
  }
  panel->gui->cursor_visible = 1;
  video_window_set_cursor_visibility (panel->gui->vwin, 1);
}

/*
 * Check if there a mixer control available,
 */
void panel_add_mixer_control (xui_panel_t *panel) {

  if (!panel)
    return;
  panel->gui->mixer.caps = MIXER_CAP_NOTHING;

  if (panel->gui->ao_port) {
    if ((xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_VOLUME)) != -1)
      panel->gui->mixer.caps |= MIXER_CAP_VOL;
    if ((xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_MUTE)) != -1)
      panel->gui->mixer.caps |= MIXER_CAP_MUTE;
  }

  if (((panel->gui->mixer.method == SOUND_CARD_MIXER) && panel->gui->mixer.caps & MIXER_CAP_VOL) ||
     (panel->gui->mixer.method == SOFTWARE_MIXER)) {
    int level = 0;

    xitk_widgets_state (panel->w + _W_slider_mixer, 1, XITK_WIDGET_STATE_ENABLE, ~0u);

    if (panel->gui->mixer.method == SOUND_CARD_MIXER)
      level = panel->gui->mixer.volume_level = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_VOLUME);
    else if (panel->gui->mixer.method == SOFTWARE_MIXER)
      level = panel->gui->mixer.amp_level = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);

    xitk_slider_set_pos (panel->w[_W_slider_mixer], level);
  }

  if (panel->gui->mixer.caps & MIXER_CAP_MUTE) {
    xitk_widgets_state (panel->w + _W_mute, 1, XITK_WIDGET_STATE_ENABLE, ~0u);
    panel->gui->mixer.mute = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_MUTE);
    xitk_button_set_state (panel->w[_W_mute], panel->gui->mixer.mute);
  }

  /* Tips should be available only if widgets are enabled */
  if (panel->gui->mixer.caps & MIXER_CAP_VOL)
    xitk_set_widget_tips (panel->w[_W_slider_mixer], _("Volume control"));
  if (panel->gui->mixer.caps & MIXER_CAP_MUTE)
    xitk_set_widget_tips (panel->w[_W_mute], _("Mute toggle"));
}

void panel_deinit (xui_panel_t *panel) {
  if (!panel)
    return;

  _panel_get_visibility (panel);
  config_update_num (panel->gui->xine, "gui.panel_visible", panel->visible > 1);
  if (panel->visible > 1) {
    panel->visible = 0;
    xitk_window_flags (panel->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
    xitk_hide_widgets (panel->widget_list);
  }
  _panel_exit (panel);
}

void panel_paint (xui_panel_t *panel) {
  if (panel_is_visible (panel) > 1)
    xitk_paint_widget_list (panel->widget_list);
}


/*
 * Create the panel window, and fit all widgets in.
 */
xui_panel_t *panel_init (gGui_t *gui) {
  xui_panel_t              *panel;
  char                     *title = _("xine Panel");
  xitk_image_t             *bg_image;
  int                       width, height;

  if (!gui)
    return NULL;

  panel = calloc (1, sizeof (*panel));
  if (!panel)
    return NULL;

  panel->gui = gui;

  panel->shown_length = ~3;
  panel->shown_time = ~3;

  panel->playback_widgets.enabled = -1;

  panel->skin_on_change = 0;

  /*
   * load bg image before opening window, so we can determine it's size
   */
  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (panel->gui->skin_config, "BackGround");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    gui_msg (panel->gui, XUI_MSG_ERROR, _("panel: couldn't find image for background\n"));
    free(panel);
    return NULL;
  }
  width = xitk_image_width(bg_image);
  height = xitk_image_height(bg_image);

  /*
   * open the panel window
   */

  panel->x = 200;
  panel->y = 100;
  gui_load_window_pos (panel->gui, "panel", &panel->x, &panel->y);

  panel->xwin = xitk_window_create_window_ext (gui->xitk, panel->x, panel->y, width, height, title,
    title, "xine", 0, is_layer_above (panel->gui), panel->gui->icon, bg_image);
  if (!panel->xwin) {
    gui_msg (panel->gui, XUI_MSG_ERROR, _("panel: couldn't create window\n"));
    xitk_image_free_image(&bg_image);
    free(panel);
    return NULL;
  }
  xitk_window_set_role (panel->xwin, XITK_WR_VICE);

  /*
   * Widget-list
   */

  panel->widget_list = xitk_window_widget_list(panel->xwin);

  {
    xitk_widget_t *w;
    xitk_button_widget_t b;

    XITK_WIDGET_INIT (&b);
    b.state_callback    = NULL;

    b.userdata          = GUI_PREV (panel->gui);

    b.skin_element_name = "Prev";
    b.callback          = gui_nextprev_mrl;
    panel->w[_W_prev] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_prev], XITK_WIDGET_STATE_KEEP);

    b.skin_element_name = "AudioPrev";
    b.callback          = gui_nextprev_audio_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Previous audio channel"));

    b.skin_element_name = "SpuPrev";
    b.callback          = gui_nextprev_spu_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Previous SPU channel"));

    b.skin_element_name = "PlaySlow";
    b.callback          = gui_nextprev_speed;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Slow motion"));

    b.userdata          = GUI_NEXT (panel->gui);

    b.skin_element_name = "Next";
    b.callback          = gui_nextprev_mrl;
    panel->w[_W_next] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_next], XITK_WIDGET_STATE_KEEP);

    b.skin_element_name = "AudioNext";
    b.callback          = gui_nextprev_audio_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Next audio channel"));

    b.skin_element_name = "SpuNext";
    b.callback          = gui_nextprev_spu_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Next SPU channel"));

    b.skin_element_name = "PlayFast";
    b.callback          = gui_nextprev_speed;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Fast motion"));

    b.userdata          = panel->gui;

    b.skin_element_name = "Stop";
    b.callback          = gui_stop;
    panel->w[_W_stop] =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_stop], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (panel->w[_W_stop], _("Stop playback"));

    b.skin_element_name = "Play";
    b.callback          = gui_play;
    panel->w[_W_play] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_play], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (panel->w[_W_play], _("Play selected entry"));

    b.skin_element_name = "Eject";
    b.callback          = gui_eject;
    panel->w[_W_eject] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_eject], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (panel->w[_W_eject], _("Eject current medium"));

    b.skin_element_name = "Exit";
    b.callback          = gui_exit;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Quit"));

    b.skin_element_name = "Setup";
    b.callback          = gui_setup_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Setup window"));

    b.skin_element_name = "Nav";
    b.callback          = gui_event_sender_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Navigator"));

    b.skin_element_name = "FullScreen";
    b.callback          = gui_set_fullscreen_mode;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Fullscreen/Window mode"));

    b.skin_element_name = "CtlBtn";
    b.callback          = gui_control_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Control"));

    b.skin_element_name = "AudioCtlBtn";
    b.callback          = gui_acontrol_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Audio control"));

    b.skin_element_name = "MrlBtn";
    b.callback          = gui_mrlbrowser_show;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Open Location"));

    b.skin_element_name = "PlBtn";
    b.callback          = gui_playlist_show;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Playlist"));

    b.userdata          = panel;

    b.skin_element_name = "Close";
    b.callback          = panel_toggle_visibility;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Hide GUI"));

    b.skin_element_name = "Snapshot";
    b.callback          = panel_snapshot;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (w, _("Take a snapshot"));
  }

  /*
   * Init to default, otherwise if panel is hide
   * at startup, label is empty 'till it's updated
   */
  panel->runtime_mode   = 0;
  panel_reset_runtime_label (panel);

  {
    xitk_label_widget_t lbl;
    XITK_WIDGET_INIT (&lbl);
    lbl.userdata          = panel;

    lbl.skin_element_name = "TimeLabel";
    lbl.label             = "00:00:00";
    lbl.callback          = _panel_change_time_label;
    panel->runtime_label = xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->runtime_label, XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (panel->runtime_label, _("Total time: --:--:--"));

    lbl.label             = "";

    lbl.skin_element_name = "TitleLabel";
    lbl.callback          = _panel_change_display_mode;
    panel->title_label =  xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->title_label, XITK_WIDGET_STATE_KEEP);

    lbl.skin_element_name = "AudioLabel";
    lbl.callback          = panel_audio_lang_list;
    panel->audiochan_label = xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->audiochan_label, XITK_WIDGET_STATE_KEEP);

    lbl.skin_element_name = "SpuLabel";
    lbl.callback          = panel_spu_lang_list;
    panel->spuid_label =  xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->spuid_label, XITK_WIDGET_STATE_KEEP);
  }

  {
    xitk_slider_widget_t sl;
    XITK_WIDGET_INIT (&sl);
    sl.motion_userdata   = panel;
    sl.callback          = NULL;
    sl.userdata          = NULL;
    sl.min               = 0;

    sl.skin_element_name = "SliderPlay";
    sl.max               = 65535;
    sl.step              = sl.max / 50;
    sl.motion_callback   = panel_slider_play_cb;
    panel->w[_W_slider_play] = xitk_slider_create (panel->widget_list, panel->gui->skin_config, &sl);
    xitk_add_widget (panel->widget_list, panel->w[_W_slider_play],
      (panel->gui->playlist.num ? XITK_WIDGET_STATE_ENABLE : 0) | XITK_WIDGET_STATE_VISIBLE);
    xitk_widget_mode (panel->w[_W_slider_play], WIDGET_KEEP_FOCUS, 0);
    xitk_set_widget_tips (panel->w[_W_slider_play], _("Stream playback position slider"));
    xitk_slider_reset (panel->w[_W_slider_play]);

    sl.skin_element_name = "SliderVol";
    sl.max               = (panel->gui->mixer.method == SOUND_CARD_MIXER) ? 100 : 200;
    sl.step              = sl.max / 20;
    sl.motion_callback   = panel_slider_mixer_cb;
    panel->w[_W_slider_mixer] = xitk_slider_create (panel->widget_list, panel->gui->skin_config, &sl);
    xitk_add_widget (panel->widget_list, panel->w[_W_slider_mixer], XITK_WIDGET_STATE_VISIBLE);
    xitk_slider_reset (panel->w[_W_slider_mixer]);
  }

  {
    xitk_button_widget_t b;
    XITK_WIDGET_INIT (&b);

    b.callback          = NULL;

    b.skin_element_name = "Pause";
    b.state_callback    = gui_pause;
    b.userdata          = panel->gui;
    panel->w[_W_pause] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_pause],
      XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE | XITK_WIDGET_STATE_IMMEDIATE);
    xitk_set_widget_tips (panel->w[_W_pause], _("Pause/Resume playback"));

    b.skin_element_name = "Mute";
    b.state_callback    = panel_toggle_audio_mute;
    b.userdata          = panel;
    panel->w[_W_mute] = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->w[_W_mute], XITK_WIDGET_STATE_VISIBLE);
  }

  panel->tips.enable = xine_config_register_bool (panel->gui->xine, "gui.tips_visible", 1,
    _("gui tips visibility"),
    _("If disabled, no tooltips are shown."),
    CONFIG_LEVEL_ADV, panel_enable_tips_cb, panel);
  panel->tips.timeout = (unsigned long)xine_config_register_num (panel->gui->xine, "gui.tips_timeout", 5000,
    _("Tips timeout (ms)"),
    _("Persistence time of tooltips, in milliseconds."),
    CONFIG_LEVEL_ADV, panel_timeout_tips_cb, panel);
  panel_show_tips (panel);

  panel_update_mrl_display (panel);
  panel_update_nextprev_tips (panel);

  /* NOTE: panel and video window follow a complex visibility logic.
   * defer this to panel_add_autoplay_buttons () when video win is stable. */
  panel->visible = 0;
  /* NOTE: do this _before_ xitk_window_flags (, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE), and make sure to get that initial expose event
   * handled in xitk.c - otherwise some widgets may not show until focused. */
  xitk_window_flags (panel->xwin,
    XITK_WINF_TASKBAR | XITK_WINF_PAGER | XITK_WINF_DND, XITK_WINF_TASKBAR | XITK_WINF_PAGER | XITK_WINF_DND);
  panel->widget_key = xitk_be_register_event_handler ("panel", panel->xwin, panel_event, panel, NULL, NULL);

  pthread_mutex_init (&panel->sl.mutex, NULL);
  pthread_cond_init (&panel->sl.wake, NULL);
  pthread_mutex_lock (&panel->sl.mutex);
  panel->sl.run = 1;
  pthread_mutex_unlock (&panel->sl.mutex);
  {
    pthread_attr_t       pth_attrs;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    struct sched_param   pth_params;
#endif

    pthread_attr_init(&pth_attrs);

    /* this won't work on linux, freebsd 5.0 */
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
#endif

    pthread_create (&panel->sl.thread, &pth_attrs, slider_loop, panel);
    pthread_attr_destroy (&pth_attrs);
  }

  panel->gui->panel = panel;
  return panel;
}

void panel_set_title (xui_panel_t *panel, char *title) {
  if(panel && panel->title_label)
    xitk_label_change_label(panel->title_label, title);
}

