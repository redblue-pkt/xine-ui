/*
 * Copyright (C) 2000-2020 the xine project
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
#include "xine-toolkit/checkbox.h"

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

  struct {
    int                 enabled;
    xitk_widget_t      *prev;
    xitk_widget_t      *stop;
    xitk_widget_t      *play;
    xitk_widget_t      *pause;
    xitk_widget_t      *next;
    xitk_widget_t      *eject;
    xitk_widget_t      *slider_play;
    unsigned int        slider_play_time;
  } playback_widgets;

  struct {
    xitk_widget_t      *slider;
    xitk_widget_t      *mute;
  } mixer;

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
  pthread_t             slider_thread;

  /* private vars to avoid useless updates */
  unsigned int          shown_time;
  unsigned int          shown_length;

};

void panel_update_nextprev_tips (xui_panel_t *panel) {

  if (panel->gui->skip_by_chapter) { 
    xitk_set_widget_tips(panel->playback_widgets.prev, _("Play previous chapter or mrl")); 
    xitk_set_widget_tips(panel->playback_widgets.next, _("Play next chapter or mrl")); 
  }
  else {
    xitk_set_widget_tips(panel->playback_widgets.prev, _("Play previous entry from playlist"));
    xitk_set_widget_tips(panel->playback_widgets.next, _("Play next entry from playlist")); 
  }
}

int is_playback_widgets_enabled (xui_panel_t *panel) {
  return (panel->playback_widgets.enabled == 1);
}

void enable_playback_controls (xui_panel_t *panel, int enable) {
  void (*enability)(xitk_widget_t *) = NULL;
  
  panel->playback_widgets.enabled = enable;
  
  if(enable)
    enability = xitk_enable_widget;
  else
    enability = xitk_disable_widget;
  
  enability(panel->playback_widgets.prev);
  enability(panel->playback_widgets.stop);
  enability(panel->playback_widgets.play);
  enability(panel->playback_widgets.pause);
  enability(panel->playback_widgets.next);
  enability(panel->playback_widgets.slider_play);
}

/*
 *
 */
void panel_show_tips (xui_panel_t *panel) {
  
  if(panel->tips.enable)
    xitk_set_widgets_tips_timeout(panel->widget_list, panel->tips.timeout);
  else
    xitk_disable_widgets_tips(panel->widget_list);

  playlist_show_tips (panel->gui, panel->tips.enable, panel->tips.timeout);
  control_show_tips (panel->gui->vctrl, panel->tips.enable, panel->tips.timeout);
  mrl_browser_show_tips (panel->gui->mrlb, panel->tips.enable, panel->tips.timeout);
  event_sender_show_tips (panel->gui, panel->tips.enable ? panel->tips.timeout : 0);
  mmk_editor_show_tips(panel->tips.enable, panel->tips.timeout);
  setup_show_tips (panel->gui->setup, panel->tips.enable, panel->tips.timeout);
}

/* Somewhat paranoia conditionals (Hans, YOU're paranoid ;-) )*/
int panel_get_tips_enable (xui_panel_t *panel) {
  return (panel) ? panel->tips.enable : 0;
}
unsigned long panel_get_tips_timeout (xui_panel_t *panel) {
  return (panel) ? panel->tips.timeout : 0;
}

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
static void panel_store_new_position (void *data, int x, int y, int w, int h) {
  xui_panel_t *panel = data;

  (void)h;

  if(panel->skin_on_change == 0) {
    
    panel->x = x;
    panel->y = y;
    
    config_update_num ("gui.panel_x", x); 
    config_update_num ("gui.panel_y", y);
    
    event_sender_move (panel->gui, x + w, y);
  }
  else
    panel->skin_on_change--;

}

static void panel_exit (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;

  if(panel) {
    window_info_t wi;
    
    panel->visible = 0;

#ifdef HAVE_XINE_CONFIG_UNREGISTER_CALLBACKS
    xine_config_unregister_callbacks (panel->gui->xine, NULL, NULL, panel, sizeof (*panel));
#endif

    if((xitk_get_window_info(panel->widget_key, &wi))) {
      config_update_num ("gui.panel_x", wi.x);
      config_update_num ("gui.panel_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler(&panel->widget_key);

    pthread_join(panel->slider_thread, NULL);

    panel->title_label = NULL;

    xitk_window_destroy_window(panel->xwin);
    panel->xwin = NULL;

    /* xitk_dlist_init (&panel->widget_list->list); */

    panel->gui->panel = NULL;
    free (panel);
  }
}

/*
 * Change the current skin.
 */
void panel_change_skins (xui_panel_t *panel, int synthetic) {
  xitk_image_t *bg_image;
  int           new_width, new_height;

  (void)synthetic;

  panel->skin_on_change++;

  xitk_hide_widgets(panel->widget_list);
  xitk_skin_lock (panel->gui->skin_config);

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (panel->gui->skin_config, "BackGround");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    xine_error (panel->gui, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
    exit(-1);
  }
  new_width = xitk_image_width(bg_image);
  new_height = xitk_image_height(bg_image);

  xitk_window_resize_window (panel->xwin, new_width, new_height);
  xitk_window_change_background_with_image(panel->xwin, bg_image, new_width, new_height);

  video_window_set_transient_for (panel->gui->vwin, panel->xwin);

  if (panel_is_visible (panel))
    raise_window (panel->gui, panel->xwin, 1, 1);

  xitk_skin_unlock (panel->gui->skin_config);
  
  xitk_change_skins_widget_list (panel->widget_list, panel->gui->skin_config);

  enable_playback_controls (panel, panel->playback_widgets.enabled);
  xitk_paint_widget_list(panel->widget_list);

  event_sender_move (panel->gui, panel->x + new_width, panel->y);
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
void panel_update_runtime_display (xui_panel_t *panel) {
  int seconds, pos, length;
  char timestr[16];

  if (!panel_is_visible (panel))
    return;

  if (!gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, &seconds, &length)) {

    if ((panel->gui->stream_length.pos || panel->gui->stream_length.time) && panel->gui->stream_length.length) {
      pos     = panel->gui->stream_length.pos;
      seconds = panel->gui->stream_length.time;
      length  = panel->gui->stream_length.length;
    }
    else
      return;

  }

  do {
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
    secs   = msecs / 1000u;
    msecs %= 1000u;
    secs2str (timestr, secs);
    if (xine_get_param (panel->gui->stream, XINE_PARAM_SPEED) > XINE_SPEED_NORMAL / 2)
      break;
    /* millisecond information is useful when editing. since we only update */
    /* twice a second, show this in slow mode only. */
    timestr[12] = 0;
    timestr[11] = (msecs % 10u) + '0';
    msecs /= 10u;
    timestr[10] = (msecs % 10u) + '0';
    timestr[9] = (msecs / 10u) + '0';
    timestr[8] = '.';
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
}

/*
 * Update slider thread.
 */
static __attribute__((noreturn)) void *slider_loop (void *data) {
  xui_panel_t *panel = data;
  int screensaver_timer = 0;
  int lastmsecs = -1;
  int i = 0;
  
  while (panel->gui->on_quit == 0) {

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
	      goto __next_iteration;
	    }
	  }
	  
          if (panel->gui->playlist.num) {
            if ((xine_get_stream_info (panel->gui->stream, XINE_STREAM_INFO_SEEKABLE))) {
	      if(!xitk_is_widget_enabled(panel->playback_widgets.slider_play))
		xitk_enable_widget(panel->playback_widgets.slider_play);
	    }
	    else {
	      if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
		xitk_slider_reset(panel->playback_widgets.slider_play);
		xitk_disable_widget(panel->playback_widgets.slider_play);
	      }
	    }
	  }

          /* pthread_mutex_unlock (&panel->gui->xe_mutex); */
	}
	else
	  pos = -1;
	
      }
      
      if(!(i % 2)) {
	osd_update();
	
        if (panel->gui->mrl_overrided) {
          panel->gui->mrl_overrided--;
          if (panel->gui->mrl_overrided == 0)
            panel_update_mrl_display (panel);
	}
      }
      
      if ((status == XINE_STATUS_PLAY) &&
        ((speed != XINE_SPEED_PAUSE) || (msecs != lastmsecs))) {
        char *ident = stream_infos_get_ident_from_stream (panel->gui->stream);

        if (!pthread_mutex_trylock (&panel->gui->mmk_mutex)) {
          if (ident) {
            if (panel->gui->playlist.num &&
                (panel->gui->playlist.cur >= 0) &&
                panel->gui->playlist.mmk &&
                panel->gui->playlist.mmk[panel->gui->playlist.cur] &&
                (strcmp (panel->gui->mmk.ident, ident))) {
              free (panel->gui->mmk.ident);
              panel->gui->mmk.ident = strdup (ident);
              free (panel->gui->playlist.mmk[panel->gui->playlist.cur]->ident);
              panel->gui->playlist.mmk[panel->gui->playlist.cur]->ident = strdup (ident);
              pthread_mutex_unlock (&panel->gui->mmk_mutex);
              video_window_set_mrl (panel->gui->vwin, ident);
              playlist_mrlident_toggle (panel->gui);
              panel_update_mrl_display (panel);
            } else {
              pthread_mutex_unlock (&panel->gui->mmk_mutex);
            }
          } else {
            video_window_set_mrl (panel->gui->vwin, (char *)panel->gui->mmk.mrl);
            pthread_mutex_unlock (&panel->gui->mmk_mutex);
          }
        }
        free (ident);

        {
          if (!video_window_is_window_iconified (panel->gui->vwin)) {
            if (panel->gui->ssaver_timeout) {
              if (!(i % 2))
                screensaver_timer++;
              if (screensaver_timer >= panel->gui->ssaver_timeout)
                screensaver_timer = video_window_reset_ssaver (panel->gui->vwin);
            }
          }
        }

        if (panel->gui->logo_mode == 0) {

          if (panel_is_visible (panel)) {

            panel_update_runtime_display (panel);

	    if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
	      if(pos >= 0)
		xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
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

	      xitk_slider_set_pos(panel->mixer.slider, level);
              panel_check_mute (panel);

	      i = 0;
	    }

	  }

          if (stream_infos_is_visible() && panel->gui->stream_info_auto_update)
	    stream_infos_update_infos();

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
      video_window_update_logo (panel->gui->vwin);
    
  __next_iteration:
    xine_usec_sleep(500000.0);
    i++;
  }

  pthread_exit(NULL);
}

/*
 * Boolean about panel visibility.
 */
int panel_is_visible (xui_panel_t *panel) {

  if(panel) {
    if (panel->gui->use_root_window)
      return xitk_window_is_window_visible (panel->xwin);
    else
      return panel->visible && xitk_window_is_window_visible (panel->xwin);
  }

  return 0;
}

/*
 * Show/Hide panel window.
 */
static void _panel_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;
  int visible = xitk_window_is_window_visible (panel->xwin);

  (void)w;

  if (panel->visible && !video_window_is_separate_display(panel->gui->vwin)) {
    
    if (video_window_is_visible (panel->gui->vwin)) {
      if (panel->gui->use_root_window) { /* Using root window */
        if (visible)
          xitk_window_iconify_window(panel->xwin);
        else
          xitk_window_show_window(panel->xwin, 0);
      }
      else {
	panel->visible = 0;
        xitk_window_hide_window(panel->xwin);
	xitk_hide_widgets(panel->widget_list);
      }
    }
    else {
      if(visible)
        xitk_window_iconify_window(panel->xwin);
      else
        xitk_window_show_window(panel->xwin, 0);
    }

    video_window_grab_input_focus(panel->gui->vwin);
  }
  else {

    panel->visible = 1;
    panel->gui->nongui_error_msg = NULL;
    xitk_show_widgets(panel->widget_list);

    xitk_window_show_window(panel->xwin, 1);
    video_window_set_transient_for (panel->gui->vwin, panel->xwin);

    wait_for_window_visible (panel->xwin);
    layer_above_video (panel->gui, panel->xwin);
    /* FIXME: when switching to fullscreen with old kwin, compositor off,
     * panel stays hidden  50% of cases. It reappears after a mouse click
     * anywhere. Other open xine windows seem not to have this issue.
     * And no, video window _does_ report viewable. Waiting half a second
     * after switch does help, and so does this HACK. */
    xitk_window_raise_window (panel->xwin);
     
    if (panel->gui->cursor_grabbed) {
      xitk_ungrab_pointer();
    }
    
#if defined(HAVE_XINERAMA) || defined(HAVE_XF86VIDMODE)
    if(
#ifdef HAVE_XINERAMA
       (((video_window_get_fullscreen_mode (panel->gui->vwin)) & (WINDOWED_MODE | FULLSCR_MODE)) &&
	(!((video_window_get_fullscreen_mode (panel->gui->vwin)) & FULLSCR_XI_MODE)))
#ifdef HAVE_XF86VIDMODE
       ||
#endif
#endif
#ifdef HAVE_XF86VIDMODE
       (panel->gui->XF86VidMode_fullscreen)
#endif
       ) {
      int x, y, w, h, desktopw, desktoph;
      
      xitk_window_get_window_position (panel->xwin, &x, &y, &w, &h);

      desktopw = xitk_get_display_width();
      desktoph = xitk_get_display_height();

      if(((x + w) <= 0) || ((y + h) <= 0) || (x >= desktopw) || (y >= desktoph)) {
	int newx, newy;
	
	newx = (desktopw - w) >> 1;
	newy = (desktoph - h) >> 1;

        xitk_window_move_window(panel->xwin, newx, newy);

        panel_store_new_position (panel, newx, newy, w, h);
      }
    }
#endif
    
    if (panel->gui->logo_mode == 0) {
      int pos;
      
      if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
        if (gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, NULL, NULL))
	  xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
        panel_update_runtime_display (panel);
      }

      panel_update_mrl_display (panel);
    }
    
  }

  gui_hide_show_all (panel->gui, ~1, panel->visible ? ~0 : 0);
}

void panel_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;
  _panel_toggle_visibility(w, data);
  config_update_num ("gui.panel_visible", panel->visible);
}

void panel_raise_window(xui_panel_t *panel)
{
  if (panel_is_visible (panel))  {
    xitk_window_raise_window(panel->xwin);
    video_window_set_transient_for (panel->gui->vwin, panel->xwin);
  }
}

void panel_get_window_position(xui_panel_t *panel, int *px, int *py, int *pw, int *ph)
{
  xitk_window_get_window_position(panel->xwin, px, py, pw, ph);
}

void panel_check_mute (xui_panel_t *panel) {
  xitk_checkbox_set_state (panel->mixer.mute, panel->gui->mixer.mute);
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause (xui_panel_t *panel) {
  
  xitk_checkbox_set_state(panel->playback_widgets.pause, 
    (((xine_get_status (panel->gui->stream) == XINE_STATUS_PLAY) && 
      (xine_get_param (panel->gui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)) ? 1 : 0));
}

void panel_reset_runtime_label (xui_panel_t *panel) {
  if(panel->runtime_mode == 0)
    xitk_label_change_label (panel->runtime_label, "00:00:00"); 
  else
    xitk_label_change_label (panel->runtime_label, "..:..:.."); 
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
  xitk_slider_reset(panel->playback_widgets.slider_play);
  panel_reset_runtime_label (panel);
}

void panel_update_slider (xui_panel_t *panel, int pos) {
  xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
}

/*
 * Update audio/spu channel displayed informations.
 */
void panel_update_channel_display (xui_panel_t *panel) {
  int   channel;
  char  buffer[XINE_LANG_MAX];
  const char *lang = NULL;

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
  int wx, wy;

  (void)w;
  xitk_window_get_window_position (panel->xwin, &wx, &wy, NULL, NULL);
  xitk_get_widget_pos(panel->audiochan_label, &x, &y);
  x += wx;
  y += (wy + xitk_get_widget_height(panel->audiochan_label));
  audio_lang_menu (panel->gui, panel->widget_list, x, y);
}

static void panel_spu_lang_list(xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;
  int x, y;
  int wx, wy;

  (void)w;
  xitk_window_get_window_position (panel->xwin, &wx, &wy, NULL, NULL);
  xitk_get_widget_pos(panel->spuid_label, &x, &y);
  x += wx;
  y += (wy + xitk_get_widget_height(panel->spuid_label));
  spu_lang_menu (panel->gui, panel->widget_list, x, y);
}

/*
 * Update displayed MRL according to the current one.
 */
void panel_update_mrl_display (xui_panel_t *panel) {
  if (panel) {
    pthread_mutex_lock (&panel->gui->mmk_mutex);
    panel_set_title (panel, (panel->gui->is_display_mrl) ? panel->gui->mmk.mrl : panel->gui->mmk.ident);
    pthread_mutex_unlock (&panel->gui->mmk_mutex);
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

    xitk_slider_set_max(panel->mixer.slider, max);
    xitk_slider_set_pos(panel->mixer.slider, vol);
    xitk_checkbox_set_state(panel->mixer.mute, gui->mixer.mute);
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
    osd_display_info(_("Amp: %s"), panel->gui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  else if (panel->gui->mixer.caps & MIXER_CAP_MUTE) {
    panel->gui->mixer.mute = state;
    xine_set_param (panel->gui->stream, XINE_PARAM_AUDIO_MUTE, panel->gui->mixer.mute);
    osd_display_info(_("Audio: %s"), panel->gui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  panel_check_mute (panel);
}

/*
 *  Use internal xine-lib framegrabber function
 *  to snapshot current frame.
 */
static void panel_snapshot_error(void *data, char *message) {
  gGui_t *gui = data;
  xine_error (gui, "%s", message);
}
static void panel_snapshot_info(void *data, char *message) {
  gGui_t *gui = data;
  xine_info (gui, "%s", message);
}
void panel_snapshot (xitk_widget_t *w, void *data) {
  xui_panel_t *panel = data;

  (void)w;
  pthread_mutex_lock (&panel->gui->mmk_mutex);
  create_snapshot (panel->gui->mmk.mrl, panel_snapshot_error, panel_snapshot_info, panel->gui);
  pthread_mutex_unlock (&panel->gui->mmk_mutex);
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_cb(xitk_widget_t *w, void *data, int pos) {
  xui_panel_t *panel = data;

  if(w == panel->playback_widgets.slider_play) {
    unsigned int stime;
    struct timeval tv = {0, 0};

    /* Update ui gfx smoothly, at most 20 times a second (~ 5 times less than before).
     * There is hardly a visible difference, but a lot less X action - especially
     * with KDE window thumbnails on. Slider thread will fix the final position. */
    gettimeofday (&tv, NULL);
    stime = tv.tv_usec / (1000000 / 20) + tv.tv_sec * 20;
    /* printf ("panel_slider_cb: timeslot #%u.\n", stime); */
    if (xitk_is_widget_enabled (panel->playback_widgets.slider_play)) {
      gui_set_current_position (pos);
      if (stime != panel->playback_widgets.slider_play_time) {
        panel->playback_widgets.slider_play_time = stime;
        if (xine_get_status (panel->gui->stream) != XINE_STATUS_PLAY) {
          panel_reset_slider (panel);
        } else {
          /*
          int pos;
          if (gui_xine_get_pos_length (panel->gui, panel->gui->stream, &pos, NULL, NULL))
            xitk_slider_set_pos (panel->playback_widgets.slider_play, pos); */
          panel_update_runtime_display (panel);
        }
      }
    }
  }
  else if(w == panel->mixer.slider) {

    if (panel->gui->mixer.method == SOUND_CARD_MIXER)
      change_audio_vol(pos);
    else if (panel->gui->mixer.method == SOFTWARE_MIXER)
      change_amp_vol(pos);
    
  }
  
  panel_check_pause (panel);
}

/*
 * Handle X events here.
 */
static void panel_handle_destroy_notify(void *data) {
  xui_panel_t *panel = data;

  gui_exit (NULL, panel->gui);
}

static void panel_handle_map_notify(void *data) {
  xui_panel_t *panel = data;

  /* When panel becomes visible by any means,               */
  /* all other hidden GUI windows shall also become visible */
  gui_hide_show_all (panel->gui, ~1, ~0);
}

static void panel_handle_button_event(void *data, const xitk_button_event_t *be) {
  xui_panel_t *panel = data;

  if (be->event == XITK_BUTTON_PRESS) {
    if (be->button == Button3) {
      video_window_menu (panel->gui, panel->widget_list);
    }
  }
  if (be->event == XITK_BUTTON_RELEASE) {
    gui_handle_button_event (panel->gui, be);
  }
}

static void panel_handle_key_event(void *data, const xitk_key_event_t *ke) {
  xui_panel_t *panel = data;
  gui_handle_key_event (panel->gui, ke);
}

static const xitk_event_cbs_t panel_event_cbs = {
  .key_cb            = panel_handle_key_event,
  .btn_cb            = panel_handle_button_event,
  .map_notify_cb     = panel_handle_map_notify,
  .destroy_notify_cb = panel_handle_destroy_notify,

  .pos_cb            = panel_store_new_position,
  .dnd_cb            = gui_dndcallback,
};

/*
 * Create buttons from information if input plugins autoplay featured. 
 * We couldn't do this into panel_init(), this function is
 * called before xine engine initialization.
 */
void panel_add_autoplay_buttons (xui_panel_t *panel) {
  const char *tips[64];
  const char * const *autoplay_plugins = xine_get_autoplay_input_plugin_ids (panel->gui->xine);
  unsigned int i;

  if (!autoplay_plugins)
    return;

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
    tips, panel->tips.timeout, 0);
  if (panel->autoplay_buttons)
    xitk_add_widget (panel->widget_list, panel->autoplay_buttons);
}

/*
 * Check if there a mixer control available,
 */
void panel_add_mixer_control (xui_panel_t *panel) {
  
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

    xitk_enable_widget(panel->mixer.slider);
    
    if (panel->gui->mixer.method == SOUND_CARD_MIXER)
      level = panel->gui->mixer.volume_level = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_VOLUME);
    else if (panel->gui->mixer.method == SOFTWARE_MIXER)
      level = panel->gui->mixer.amp_level = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);
    
    xitk_slider_set_pos(panel->mixer.slider, level);
  }

  if (panel->gui->mixer.caps & MIXER_CAP_MUTE) {
    xitk_enable_widget(panel->mixer.mute);
    panel->gui->mixer.mute = xine_get_param (panel->gui->stream, XINE_PARAM_AUDIO_MUTE);
    xitk_checkbox_set_state (panel->mixer.mute, panel->gui->mixer.mute);
  }

  /* Tips should be available only if widgets are enabled */
  if (panel->gui->mixer.caps & MIXER_CAP_VOL)
    xitk_set_widget_tips_and_timeout(panel->mixer.slider, _("Volume control"), panel->tips.timeout);
  if (panel->gui->mixer.caps & MIXER_CAP_MUTE)
    xitk_set_widget_tips_and_timeout(panel->mixer.mute, _("Mute toggle"), panel->tips.timeout);

  if(!panel->tips.enable) {
    xitk_disable_widget_tips(panel->mixer.slider);
    xitk_disable_widget_tips(panel->mixer.mute);
  }

}

void panel_deinit (xui_panel_t *panel) {
  if (panel_is_visible (panel))
    _panel_toggle_visibility (NULL, panel);
  panel_exit (NULL, panel);
}

void panel_paint (xui_panel_t *panel) {
  if (panel_is_visible (panel))
    xitk_paint_widget_list (panel->widget_list);
}

void panel_reparent (xui_panel_t *panel) {
  if (panel) {

    if (!video_window_is_visible (panel->gui->vwin)) { /* Show the panel in taskbar */
      int x = 0, y = 0;

      panel_get_window_position(panel, &x, &y, NULL, NULL);
      xitk_window_reparent_window(panel->xwin, NULL, x, y);
    }

    reparent_window (panel->gui, panel->xwin);

    if (video_window_is_visible (panel->gui->vwin)) {
      layer_above_video (panel->gui, panel->xwin);
    }
  }
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
    xine_error (panel->gui, _("panel: couldn't find image for background\n"));
    exit(-1);
  }
  width = xitk_image_width(bg_image);
  height = xitk_image_height(bg_image);

  /*
   * open the panel window
   */

  panel->x = xine_config_register_num (panel->gui->xine, "gui.panel_x", 200,
    CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
  panel->y = xine_config_register_num (panel->gui->xine, "gui.panel_y", 100,
    CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);

  panel->xwin = xitk_window_create_simple_window_ext(gui->xitk, panel->x, panel->y, width, height, title,
    title, "xine", 0, is_layer_above (panel->gui), panel->gui->icon);
  /*
   * The following is more or less a hack to keep the panel window visible
   * with and without focus in windowed and fullscreen mode.
   * Different WMs and even different versions behave different.
   * There is no guarantee that it works with all WMs/versions.
   */
  if (!video_window_is_visible (panel->gui->vwin)) {
    if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
      xitk_window_unset_wm_window_type (panel->xwin, WINDOW_TYPE_TOOLBAR);
    xitk_window_set_wm_window_type (panel->xwin, WINDOW_TYPE_NORMAL);
  } else {
    xitk_window_unset_wm_window_type (panel->xwin, WINDOW_TYPE_NORMAL);
    if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
      xitk_window_set_wm_window_type (panel->xwin, WINDOW_TYPE_TOOLBAR);
  }
  
  video_window_set_transient_for (panel->gui->vwin, panel->xwin);

  /*
   * set background image
   */

  xitk_window_change_background_with_image(panel->xwin, bg_image, width, height);

  /*
   * Widget-list
   */

  panel->widget_list = xitk_window_widget_list(panel->xwin);

  {
    xitk_widget_t *w;
    xitk_button_widget_t b;
    XITK_WIDGET_INIT (&b);

    b.userdata          = GUI_PREV (panel->gui);

    b.skin_element_name = "Prev";
    b.callback          = gui_nextprev_mrl;
    panel->playback_widgets.prev = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.prev);

    b.skin_element_name = "AudioPrev";
    b.callback          = gui_nextprev_audio_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Previous audio channel"));

    b.skin_element_name = "SpuPrev";
    b.callback          = gui_nextprev_spu_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Previous SPU channel"));

    b.skin_element_name = "PlaySlow";
    b.callback          = gui_nextprev_speed;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Slow motion"));

    b.userdata          = GUI_NEXT (panel->gui);

    b.skin_element_name = "Next";
    b.callback          = gui_nextprev_mrl;
    panel->playback_widgets.next = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.next);

    b.skin_element_name = "AudioNext";
    b.callback          = gui_nextprev_audio_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Next audio channel"));

    b.skin_element_name = "SpuNext";
    b.callback          = gui_nextprev_spu_channel;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Next SPU channel"));

    b.skin_element_name = "PlayFast";
    b.callback          = gui_nextprev_speed;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Fast motion"));

    b.userdata          = panel->gui;

    b.skin_element_name = "Stop";
    b.callback          = gui_stop;
    panel->playback_widgets.stop =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.stop);
    xitk_set_widget_tips (panel->playback_widgets.stop, _("Stop playback"));

    b.skin_element_name = "Play";
    b.callback          = gui_play;
    panel->playback_widgets.play = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.play);
    xitk_set_widget_tips (panel->playback_widgets.play, _("Play selected entry"));

    b.skin_element_name = "Eject";
    b.callback          = gui_eject;
    panel->playback_widgets.eject = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.eject);
    xitk_set_widget_tips (panel->playback_widgets.eject, _("Eject current medium"));

    b.skin_element_name = "Exit";
    b.callback          = gui_exit;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Quit"));

    b.skin_element_name = "Setup";
    b.callback          = gui_setup_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Setup window"));

    b.skin_element_name = "Nav";
    b.callback          = gui_event_sender_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Navigator"));

    b.skin_element_name = "FullScreen";
    b.callback          = gui_set_fullscreen_mode;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Fullscreen/Window mode"));

    b.skin_element_name = "CtlBtn";
    b.callback          = gui_control_show;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Control"));

    b.skin_element_name = "MrlBtn";
    b.callback          = gui_mrlbrowser_show;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Open Location"));

    b.skin_element_name = "PlBtn";
    b.callback          = gui_playlist_show;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Playlist"));

    b.userdata          = panel;

    b.skin_element_name = "Close";
    b.callback          = panel_toggle_visibility;
    w =  xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
    xitk_set_widget_tips (w, _("Hide GUI"));

    b.skin_element_name = "Snapshot";
    b.callback          = panel_snapshot;
    w = xitk_button_create (panel->widget_list, panel->gui->skin_config, &b);
    xitk_add_widget (panel->widget_list, w);
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
    xitk_add_widget (panel->widget_list, panel->runtime_label);
    xitk_set_widget_tips (panel->runtime_label, _("Total time: --:--:--"));

    lbl.label             = "";

    lbl.skin_element_name = "TitleLabel";
    lbl.callback          = _panel_change_display_mode;
    panel->title_label =  xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->title_label);

    lbl.skin_element_name = "AudioLabel";
    lbl.callback          = panel_audio_lang_list;
    panel->audiochan_label = xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->audiochan_label);

    lbl.skin_element_name = "SpuLabel";
    lbl.callback          = panel_spu_lang_list;
    panel->spuid_label =  xitk_label_create (panel->widget_list, panel->gui->skin_config, &lbl);
    xitk_add_widget (panel->widget_list, panel->spuid_label);
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
    sl.motion_callback   = panel_slider_cb;
    panel->playback_widgets.slider_play = xitk_slider_create (panel->widget_list, panel->gui->skin_config, &sl);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.slider_play);
    xitk_widget_mode (panel->playback_widgets.slider_play, WIDGET_KEEP_FOCUS, 0);
    xitk_set_widget_tips (panel->playback_widgets.slider_play, _("Stream playback position slider"));
    xitk_slider_reset (panel->playback_widgets.slider_play);
    if (!panel->gui->playlist.num)
      xitk_disable_widget (panel->playback_widgets.slider_play);

    sl.skin_element_name = "SliderVol";
    sl.max               = (panel->gui->mixer.method == SOUND_CARD_MIXER) ? 100 : 200;
    sl.step              = sl.max / 20;
    sl.motion_callback   = panel_slider_cb;
    panel->mixer.slider = xitk_slider_create (panel->widget_list, panel->gui->skin_config, &sl);
    xitk_add_widget (panel->widget_list, panel->mixer.slider);
    xitk_slider_reset (panel->mixer.slider);
    xitk_disable_widget (panel->mixer.slider);
  }

  {
    xitk_checkbox_widget_t cb;
    XITK_WIDGET_INIT (&cb);

    cb.skin_element_name = "Pause";
    cb.callback          = gui_pause;
    cb.userdata          = panel->gui;
    panel->playback_widgets.pause = xitk_checkbox_create (panel->widget_list, panel->gui->skin_config, &cb);
    xitk_add_widget (panel->widget_list, panel->playback_widgets.pause);
    xitk_set_widget_tips (panel->playback_widgets.pause, _("Pause/Resume playback"));

    cb.skin_element_name = "Mute";
    cb.callback          = panel_toggle_audio_mute;
    cb.userdata          = panel;
    panel->mixer.mute = xitk_checkbox_create (panel->widget_list, panel->gui->skin_config, &cb);
    xitk_add_widget (panel->widget_list, panel->mixer.mute);
    xitk_disable_widget (panel->mixer.mute);
  }

  panel->tips.enable = xine_config_register_bool (panel->gui->xine, "gui.tips_visible", 1,
    _("gui tips visibility"),
    _("If disabled, no tooltips are shown."),
    CONFIG_LEVEL_ADV, panel_enable_tips_cb, panel);
  panel->tips.timeout = (unsigned long)xine_config_register_num (panel->gui->xine, "gui.tips_timeout", 5000,
    _("Tips timeout (ms)"),
    _("Persistence time of tooltips, in milliseconds."),
    CONFIG_LEVEL_ADV, panel_timeout_tips_cb, panel);

  panel_update_mrl_display (panel);
  panel_update_nextprev_tips (panel);

  /* 
   * show panel 
   */
  {
    int visible = xine_config_register_bool (panel->gui->xine, "gui.panel_visible", 1,
      _("gui panel visibility"),
      CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);

    panel->visible = (!panel->gui->no_gui) ? visible : 0;
  }
  
  /*  The user don't want panel on startup */
  if (panel->visible && (actions_on_start (panel->gui->actions_on_start, ACTID_TOGGLE_VISIBLITY))) {
    panel->visible = 0;
    config_update_num ("gui.panel_visible", 0);
  }

  if ((panel->gui->use_root_window || video_window_is_separate_display(panel->gui->vwin)) && (!panel->visible))
    panel->visible = 1;
  
  if (panel->visible)
    xitk_window_show_window (panel->xwin, 1);
  else
    xitk_hide_widgets (panel->widget_list);
  
  panel->widget_key = xitk_window_register_event_handler ("panel", panel->xwin, &panel_event_cbs, panel);

  panel->gui->cursor_visible = 1;
  video_window_set_cursor_visibility (panel->gui->vwin, 1);
  
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
    
    pthread_create (&panel->slider_thread, &pth_attrs, slider_loop, panel);
    pthread_attr_destroy (&pth_attrs);
  }

  reparent_window (panel->gui, panel->xwin);
  if (panel->visible)
    xitk_window_try_to_set_input_focus(panel->xwin);

  panel->gui->panel = panel;
  return panel;
}

void panel_set_title (xui_panel_t *panel, char *title) {
  if(panel && panel->title_label)
    xitk_label_change_label(panel->title_label, title);
}

