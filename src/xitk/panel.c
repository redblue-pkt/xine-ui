/*
 * Copyright (C) 2000-2019 the xine project
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

#include "common.h"
#include "xine-toolkit/button_list.h"

typedef struct {
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
  xitk_button_list_t   *autoplay_buttons;
  xitk_widget_t        *spuid_label;
  ImlibImage           *bg_image;
  xitk_register_key_t   widget_key;
  pthread_t             slider_thread;

  /* private vars to avoid useless updates */
  unsigned int          shown_time;
  unsigned int          shown_length;

} _panel_t;

_panel_t          *panel = NULL;

void panel_update_nextprev_tips(void) {

  if(gGui->skip_by_chapter) { 
    xitk_set_widget_tips(panel->playback_widgets.prev, _("Play previous chapter or mrl")); 
    xitk_set_widget_tips(panel->playback_widgets.next, _("Play next chapter or mrl")); 
  }
  else {
    xitk_set_widget_tips(panel->playback_widgets.prev, _("Play previous entry from playlist"));
    xitk_set_widget_tips(panel->playback_widgets.next, _("Play next entry from playlist")); 
  }
}

int is_playback_widgets_enabled(void) {
  return (panel->playback_widgets.enabled == 1);
}

void enable_playback_controls(int enable) {
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
void panel_show_tips(void) {
  
  if(panel->tips.enable)
    xitk_set_widgets_tips_timeout(panel->widget_list, panel->tips.timeout);
  else
    xitk_disable_widgets_tips(panel->widget_list);

  playlist_show_tips(panel->tips.enable, panel->tips.timeout);
  control_show_tips(panel->tips.enable, panel->tips.timeout);
  mrl_browser_show_tips(panel->tips.enable, panel->tips.timeout);
  event_sender_show_tips(panel->tips.enable, panel->tips.timeout);
  mmk_editor_show_tips(panel->tips.enable, panel->tips.timeout);
  setup_show_tips(panel->tips.enable, panel->tips.timeout);
}

/* Somewhat paranoia conditionals (Hans, YOU're paranoid ;-) )*/
int panel_get_tips_enable(void) {
  return (panel) ? panel->tips.enable : 0;
}
unsigned long panel_get_tips_timeout(void) {
  return (panel) ? panel->tips.timeout : 0;
}

/*
 * Config callbacks.
 */
static void panel_enable_tips_cb(void *data, xine_cfg_entry_t *cfg) {
  panel->tips.enable = cfg->num_value;
  panel_show_tips();
}
static void panel_timeout_tips_cb(void *data, xine_cfg_entry_t *cfg) {
  panel->tips.timeout = (unsigned long) cfg->num_value;

  panel_show_tips();
}

/*
 * Toolkit event handler will call this function with new
 * coords of panel window.
 */
static void panel_store_new_position(int x, int y, int w, int h) {

  if(panel->skin_on_change == 0) {
    
    panel->x = x;
    panel->y = y;
    
    config_update_num ("gui.panel_x", x); 
    config_update_num ("gui.panel_y", y);
    
    if(event_sender_is_running())
      event_sender_move(x+w, y);

  }
  else
    panel->skin_on_change--;

}

static void panel_exit(xitk_widget_t *w, void *data) {

  if(panel) {
    window_info_t wi;
    
    panel->visible = 0;

    if((xitk_get_window_info(panel->widget_key, &wi))) {
      config_update_num ("gui.panel_x", wi.x);
      config_update_num ("gui.panel_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler(&panel->widget_key);

    pthread_join(panel->slider_thread, NULL);

    XLockDisplay(gGui->display);
    XUnmapWindow(gGui->display, gGui->panel_window);
    XUnlockDisplay(gGui->display);

    panel->title_label = 0;
    xitk_destroy_widgets(panel->widget_list);
    xitk_button_list_delete (panel->autoplay_buttons);

    XLockDisplay(gGui->display);
    XDestroyWindow(gGui->display, gGui->panel_window);
    Imlib_destroy_image(gGui->imlib_data, panel->bg_image);
    XUnlockDisplay(gGui->display);

    gGui->panel_window = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(panel->widget_list)));

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(panel->widget_list)));
    XUnlockDisplay(gGui->display);

    XITK_WIDGET_LIST_FREE(panel->widget_list);
    
    free(panel);
    panel = NULL;  
  }
}

/*
 * Change the current skin.
 */
void panel_change_skins(int synthetic) {
  ImlibImage   *new_img, *old_img;
  XSizeHints    hint;
  
  panel->skin_on_change++;

  xitk_skin_lock(gGui->skin_config);
  xitk_hide_widgets(panel->widget_list);

  XLockDisplay(gGui->display);
  
  if(!(new_img = Imlib_load_image(gGui->imlib_data,
				  xitk_skin_get_skin_filename(gGui->skin_config, "BackGround")))) {
    xine_error(_("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
    exit(-1);
  }
  
  hint.width  = new_img->rgb_width;
  hint.height = new_img->rgb_height;
  hint.flags  = PPosition | PSize;
  XSetWMNormalHints(gGui->display, gGui->panel_window, &hint);
  
  XResizeWindow (gGui->display, gGui->panel_window,
		 (unsigned int)new_img->rgb_width,
		 (unsigned int)new_img->rgb_height);
  
  XUnlockDisplay(gGui->display);

  while(!xitk_is_window_size(gGui->display, gGui->panel_window, 
			     new_img->rgb_width, new_img->rgb_height)) {
    xine_usec_sleep(10000);
  }
  
  old_img = panel->bg_image;
  panel->bg_image = new_img;

  video_window_set_transient_for (gGui->panel_window);

  XLockDisplay(gGui->display);

  Imlib_destroy_image(gGui->imlib_data, old_img);
  Imlib_apply_image(gGui->imlib_data, new_img, gGui->panel_window);
  
  XUnlockDisplay(gGui->display);

  if(panel_is_visible())
    raise_window(gGui->panel_window, 1, 1);

  xitk_skin_unlock(gGui->skin_config);
  
  xitk_change_skins_widget_list(panel->widget_list, gGui->skin_config);

  /*
   * Update position of dynamic buttons.
   */
  xitk_button_list_new_skin (panel->autoplay_buttons, gGui->skin_config);

  enable_playback_controls(panel->playback_widgets.enabled);
  xitk_paint_widget_list(panel->widget_list);

  event_sender_move(panel->x + hint.width, panel->y);
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
void panel_update_runtime_display(void) {
  int seconds, pos, length;
  char timestr[16];

  if(!panel_is_visible())
    return;

  if(!gui_xine_get_pos_length(gGui->stream, &pos, &seconds, &length)) {

    if((gGui->stream_length.pos || gGui->stream_length.time) && gGui->stream_length.length) {
      pos     = gGui->stream_length.pos;
      seconds = gGui->stream_length.time;
      length  = gGui->stream_length.length;
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
    if (xine_get_param (gGui->stream, XINE_PARAM_SPEED) > XINE_SPEED_NORMAL / 2)
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
static __attribute__((noreturn)) void *slider_loop(void *dummy) {
  int screensaver_timer = 0;
  int lastmsecs = -1;
  int i = 0;
  
  while(gGui->on_quit == 0) {

    if(gGui->stream) {
      
      int status = xine_get_status (gGui->stream);
      int speed  = xine_get_param (gGui->stream, XINE_PARAM_SPEED);
      
      int pos = 0, msecs = 0;

      if(status == XINE_STATUS_PLAY) {
        if (gui_xine_get_pos_length (gGui->stream, &pos, &msecs, NULL)) {

	  pthread_mutex_lock(&gGui->xe_mutex);

	  if(gGui->playlist.num && gGui->playlist.cur >= 0 && gGui->playlist.mmk &&
	     gGui->playlist.mmk[gGui->playlist.cur] && gGui->mmk.end != -1) {
            if ((msecs / 1000) >= gGui->playlist.mmk[gGui->playlist.cur]->end) {
	      gGui->ignore_next = 0;
	      gui_playlist_start_next();
	      pthread_mutex_unlock(&gGui->xe_mutex);
	      goto __next_iteration;
	    }
	  }
	  
	  if(gGui->playlist.num) {
	    if((xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE))) {
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

	  pthread_mutex_unlock(&gGui->xe_mutex);
	}
	else
	  pos = -1;
	
      }
      
      if(!(i % 2)) {
	osd_update();
	
	if(gGui->mrl_overrided) {
	  gGui->mrl_overrided--;
	  if(gGui->mrl_overrided == 0)
	    panel_update_mrl_display();
	}
      }
      
      if ((status == XINE_STATUS_PLAY) &&
        ((speed != XINE_SPEED_PAUSE) || (msecs != lastmsecs))) {
	char *ident = stream_infos_get_ident_from_stream(gGui->stream);
	
	if(ident) {
	  if(gGui->playlist.num && gGui->playlist.cur >= 0 && gGui->playlist.mmk &&
	     gGui->playlist.mmk[gGui->playlist.cur]) {
	    if(strcmp(gGui->mmk.ident, ident)) {
	      free(gGui->mmk.ident);
	      free(gGui->playlist.mmk[gGui->playlist.cur]->ident);
	      
	      gGui->mmk.ident = strdup(ident);
	      gGui->playlist.mmk[gGui->playlist.cur]->ident = strdup(ident);
	      
	      video_window_set_mrl(gGui->mmk.ident);
	      playlist_mrlident_toggle();
	      panel_update_mrl_display();
	    }
	  }
	  free(ident);
	}
	else
	  video_window_set_mrl((char *)gGui->mmk.mrl);
	
        {
          int iconified;
          video_window_lock (1);
          iconified = xitk_is_window_iconified (gGui->video_display, gGui->video_window);
          video_window_lock (0);
          if (!iconified) {
            if (gGui->ssaver_timeout) {
              if (!(i % 2))
                screensaver_timer++;
              if (screensaver_timer >= gGui->ssaver_timeout)
                screensaver_timer = video_window_reset_ssaver ();
            }
          }
        }

	if(gGui->logo_mode == 0) {
	  
	  if(panel_is_visible()) {

	    panel_update_runtime_display();
	    
	    if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
	      if(pos >= 0)
		xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
	    }
	    
	    if(!(i % 20)) {
	      int level = 0;
	      
	      panel_update_channel_display();
	      
	      if((gGui->mixer.method == SOUND_CARD_MIXER) &&
		 (gGui->mixer.caps & MIXER_CAP_VOL)) {
		level = gGui->mixer.volume_level = 
		  xine_get_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME);
	      }
	      else if(gGui->mixer.method == SOFTWARE_MIXER) {
		level = gGui->mixer.amp_level = 
		  xine_get_param(gGui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);
	      }
	      
	      xitk_slider_set_pos(panel->mixer.slider, level);
	      panel_check_mute();
	      
	      i = 0;
	    }
	    
	  }
	  
	  if(stream_infos_is_visible() && gGui->stream_info_auto_update)
	    stream_infos_update_infos();

	}
      }
      lastmsecs = msecs;
    }
    
    if(gGui->cursor_visible) {
      if(!(i % 2))
	video_window_set_cursor_timer(video_window_get_cursor_timer() + 1);
      
      if(video_window_get_cursor_timer() >= 5) {
	gGui->cursor_visible = !gGui->cursor_visible;
	video_window_set_cursor_visibility(gGui->cursor_visible);
      }
    }
    
    if(gGui->logo_has_changed)
      video_window_update_logo();
    
  __next_iteration:
    xine_usec_sleep(500000.0);
    i++;
  }

  pthread_exit(NULL);
}

/*
 * Boolean about panel visibility.
 */
int panel_is_visible(void) {

  if(panel) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, gGui->panel_window);
    else
      return panel->visible && xitk_is_window_visible(gGui->display, gGui->panel_window);
  }

  return 0;
}

/*
 * Show/Hide panel window.
 */
static void _panel_toggle_visibility (xitk_widget_t *w, void *data) {
  int visible = xitk_is_window_visible(gGui->display, gGui->panel_window);

  if(((!panel->visible || !visible) && !playlist_is_visible()) || (visible && playlist_is_visible()))
    playlist_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !control_is_visible()) || (visible && control_is_visible()))
    control_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !mrl_browser_is_visible()) || (visible && mrl_browser_is_visible()))
    mrl_browser_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !setup_is_visible()) || (visible && setup_is_visible()))
    setup_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !viewlog_is_visible()) || (visible && viewlog_is_visible()))
    viewlog_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !kbedit_is_visible()) || (visible && kbedit_is_visible()))
    kbedit_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !event_sender_is_visible()) || (visible && event_sender_is_visible()))
    event_sender_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !stream_infos_is_visible()) || (visible && stream_infos_is_visible()))
    stream_infos_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !tvset_is_visible()) || (visible && tvset_is_visible()))
    tvset_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !vpplugin_is_visible()) || (visible && vpplugin_is_visible()))
    vpplugin_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !applugin_is_visible()) || (visible && applugin_is_visible()))
    applugin_toggle_visibility(NULL, NULL);

  if(((!panel->visible || !visible) && !help_is_visible()) || (visible && help_is_visible()))
    help_toggle_visibility(NULL, NULL);

  if (panel->visible && gGui->video_display == gGui->display) {
    
    XLockDisplay(gGui->display);
    
    if (video_window_is_visible ()) {
      if(gGui->use_root_window) { /* Using root window */
	if(visible)
	  XIconifyWindow(gGui->display, gGui->panel_window, gGui->screen);
	else
	  XMapWindow(gGui->display, gGui->panel_window);
      }
      else {
	panel->visible = 0;
	XUnmapWindow (gGui->display, gGui->panel_window);
	XSync(gGui->display, False);
	XUnlockDisplay(gGui->display);
	xitk_hide_widgets(panel->widget_list);
	XLockDisplay(gGui->display);
      }
    }
    else {
      if(visible)
	XIconifyWindow(gGui->display, gGui->panel_window, (XDefaultScreen(gGui->display)));
      else
	XMapWindow(gGui->display, gGui->panel_window);
    }

    XUnlockDisplay(gGui->display);

    {
      Window want;
      int t;
      video_window_lock (1);
      want = gGui->video_window;
      XLockDisplay (gGui->video_display);
      if (gGui->cursor_grabbed)
        XGrabPointer (gGui->video_display, want,
          1, None, GrabModeAsync, GrabModeAsync, want, None, CurrentTime);
      if (video_window_is_visible ()) {
        /* Give focus to video output window */
        XSetInputFocus (gGui->video_display, want, RevertToParent, CurrentTime);
        XSync (gGui->video_display, False);
        XUnlockDisplay (gGui->video_display);
        video_window_lock (0);
        /* check after 5/15/30/50/75/105/140 ms */
        for (t = 5000; t < 40000; t += 5000) {
          Window got;
          int revert;
          xine_usec_sleep (t);
          XLockDisplay (gGui->video_display);
          XGetInputFocus (gGui->video_display, &got, &revert);
          XUnlockDisplay (gGui->video_display);
          if (got == want)
            break;
        }
      } else {
        XUnlockDisplay (gGui->video_display);
        video_window_lock (0);
      }
    }

  }
  else {

    panel->visible = 1;
    gGui->nongui_error_msg = NULL;
    xitk_show_widgets(panel->widget_list);
    
    XLockDisplay(gGui->display);
    
    XRaiseWindow(gGui->display, gGui->panel_window); 
    XMapWindow(gGui->display, gGui->panel_window); 
    XUnlockDisplay (gGui->display);
    video_window_set_transient_for (gGui->panel_window);

    wait_for_window_visible(gGui->display, gGui->panel_window);
    layer_above_video(gGui->panel_window);
     
    if(gGui->cursor_grabbed) {
      XLockDisplay (gGui->display);
      XUngrabPointer(gGui->display, CurrentTime);
      XUnlockDisplay (gGui->display);
    }
    
#if defined(HAVE_XINERAMA) || defined(HAVE_XF86VIDMODE)
    if(
#ifdef HAVE_XINERAMA
       (((video_window_get_fullscreen_mode()) & (WINDOWED_MODE | FULLSCR_MODE)) &&
	(!((video_window_get_fullscreen_mode()) & FULLSCR_XI_MODE)))
#ifdef HAVE_XF86VIDMODE
       ||
#endif
#endif
#ifdef HAVE_XF86VIDMODE
       (gGui->XF86VidMode_fullscreen)
#endif
       ) {
      int x, y, w, h, desktopw, desktoph;
      
      xitk_get_window_position(gGui->display, gGui->panel_window, &x, &y, &w, &h);
      
      XLockDisplay(gGui->display);
      desktopw = DisplayWidth(gGui->display, gGui->screen);
      desktoph = DisplayHeight(gGui->display, gGui->screen);
      XUnlockDisplay(gGui->display);
      
      if(((x + w) <= 0) || ((y + h) <= 0) || (x >= desktopw) || (y >= desktoph)) {
	int newx, newy;
	
	newx = (desktopw - w) >> 1;
	newy = (desktoph - h) >> 1;
	
	XLockDisplay(gGui->display);
	XMoveWindow(gGui->display, gGui->panel_window, newx, newy);
	XUnlockDisplay(gGui->display);
	
	panel_store_new_position(newx, newy, w, h);
      }
    }
#endif
    
    if(gGui->logo_mode == 0) {
      int pos;
      
      if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
	if(gui_xine_get_pos_length(gGui->stream, &pos, NULL, NULL))
	  xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
	panel_update_runtime_display();
      }

      panel_update_mrl_display();
    }
    
  }
}

void panel_toggle_visibility (xitk_widget_t *w, void *data) {
  _panel_toggle_visibility(w, data);
  config_update_num ("gui.panel_visible", panel->visible);
}

void panel_check_mute(void) {
  xitk_checkbox_set_state(panel->mixer.mute, gGui->mixer.mute);
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause(void) {
  
  xitk_checkbox_set_state(panel->playback_widgets.pause, 
	  (((xine_get_status(gGui->stream) == XINE_STATUS_PLAY) && 
	    (xine_get_param(gGui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)) ? 1 : 0));
  
}

void panel_reset_runtime_label(void) {
  if(panel->runtime_mode == 0)
    xitk_label_change_label (panel->runtime_label, "00:00:00"); 
  else
    xitk_label_change_label (panel->runtime_label, "..:..:.."); 
}

static void _panel_change_display_mode(xitk_widget_t *w, void *data) {
  gGui->is_display_mrl = !gGui->is_display_mrl;
  panel_update_mrl_display();
  playlist_mrlident_toggle();
}
static void _panel_change_time_label(xitk_widget_t *w, void *data) {
  panel->runtime_mode = !panel->runtime_mode;
  panel_update_runtime_display();
}

/*
 * Reset the slider of panel window (set to 0).
 */
void panel_reset_slider (void) {
  xitk_slider_reset(panel->playback_widgets.slider_play);
  panel_reset_runtime_label();
}

void panel_update_slider (int pos) {
  xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
}

/*
 * Update audio/spu channel displayed informations.
 */
void panel_update_channel_display (void) {
  int   channel;
  char  buffer[XINE_LANG_MAX];
  char *lang = NULL;

  memset(&buffer, 0, sizeof(buffer));
  channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  switch (channel) {
  case -2:
    lang = "off";
    break;

  case -1:
    if(!xine_get_audio_lang (gGui->stream, channel, &buffer[0]))
      lang = "auto";
    else
      lang = buffer;
    break;

  default:
    if(!xine_get_audio_lang (gGui->stream, channel, &buffer[0]))
      snprintf(buffer, sizeof(buffer), "%3d", channel);
    lang = buffer;
    break;
  }
  xitk_label_change_label (panel->audiochan_label, lang);

  memset(&buffer, 0, sizeof(buffer));
  channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);
  switch (channel) {
  case -2:
    lang = "off";
    break;

  case -1:
    if(!xine_get_spu_lang (gGui->stream, channel, &buffer[0]))
      lang = "auto";
    else
      lang = buffer;
    break;

  default:
    if(!xine_get_spu_lang (gGui->stream, channel, &buffer[0]))
      snprintf(buffer, sizeof(buffer), "%3d", channel);
    lang = buffer;
    break;
  }
  xitk_label_change_label (panel->spuid_label, lang);
}

static void panel_audio_lang_list(xitk_widget_t *w, void *data) {
  int x, y;
  int wx, wy;

  xitk_get_window_position(gGui->display, gGui->panel_window, &wx, &wy, NULL, NULL);
  xitk_get_widget_pos(panel->audiochan_label, &x, &y);
  x += wx;
  y += (wy + xitk_get_widget_height(panel->audiochan_label));
  audio_lang_menu(panel->widget_list, x, y);
}

static void panel_spu_lang_list(xitk_widget_t *w, void *data) {
  int x, y;
  int wx, wy;

  xitk_get_window_position(gGui->display, gGui->panel_window, &wx, &wy, NULL, NULL);
  xitk_get_widget_pos(panel->spuid_label, &x, &y);
  x += wx;
  y += (wy + xitk_get_widget_height(panel->spuid_label));
  spu_lang_menu(panel->widget_list, x, y);
}
/*
 * Update displayed MRL according to the current one.
 */
void panel_update_mrl_display (void) {
  panel_set_title((gGui->is_display_mrl) ? gGui->mmk.mrl : gGui->mmk.ident);
}

void panel_update_mixer_display(void) {
  if (panel) {
    gGui_t *gui = gGui;
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

  if(gGui->mixer.method == SOFTWARE_MIXER) {
    gGui->mixer.mute = state;
    xine_set_param(gGui->stream, XINE_PARAM_AUDIO_AMP_MUTE, gGui->mixer.mute);
    osd_display_info(_("Amp: %s"), gGui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  else if(gGui->mixer.caps & MIXER_CAP_MUTE) {
    gGui->mixer.mute = state;
    xine_set_param(gGui->stream, XINE_PARAM_AUDIO_MUTE, gGui->mixer.mute);
    osd_display_info(_("Audio: %s"), gGui->mixer.mute ? _("Muted") : _("Unmuted"));
  }
  panel_check_mute();
}

/*
 *  Use internal xine-lib framegrabber function
 *  to snapshot current frame.
 */
static void panel_snapshot_error(void *data, char *message) {
  xine_error("%s", message);
}
static void panel_snapshot_info(void *data, char *message) {
  xine_info("%s", message);
}
void panel_snapshot(xitk_widget_t *w, void *data) {
  create_snapshot(gGui->mmk.mrl, panel_snapshot_error, panel_snapshot_info, NULL);
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_cb(xitk_widget_t *w, void *data, int pos) {

  if(w == panel->playback_widgets.slider_play) {
    if(xitk_is_widget_enabled(panel->playback_widgets.slider_play)) {
      gui_set_current_position (pos);
      if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY) {
	panel_reset_slider();
      }
      else {
	int pos;
	
	if(gui_xine_get_pos_length(gGui->stream, &pos, NULL, NULL))
	  xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
	
	panel_update_runtime_display();
      }
    }
  }
  else if(w == panel->mixer.slider) {

    if(gGui->mixer.method == SOUND_CARD_MIXER)
      change_audio_vol(pos);
    else if(gGui->mixer.method == SOFTWARE_MIXER)
      change_amp_vol(pos);
    
  }
  
  panel_check_pause();
}

/*
 * Handle X events here.
 */
static void panel_handle_event(XEvent *event, void *data) {

  switch(event->type) {
  case DestroyNotify:
    if(gGui->panel_window == event->xany.window)
      gui_exit(NULL, NULL);
    break;

  case ButtonPress:
    {
      XButtonEvent *bevent = (XButtonEvent *) event;

      if(bevent->button == Button3)
	video_window_menu(panel->widget_list);
    }
    break;
    
  case KeyPress:
  case ButtonRelease:
    gui_handle_event(event, data);
    break;

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case MapNotify:
    /* When panel becomes visible by any means,               */
    /* all other hidden GUI windows shall also become visible */
    if(!playlist_is_visible())
      playlist_toggle_visibility(NULL, NULL);
    if(!control_is_visible())
      control_toggle_visibility(NULL, NULL);
    if(!mrl_browser_is_visible())
      mrl_browser_toggle_visibility(NULL, NULL);
    if(!setup_is_visible())
      setup_toggle_visibility(NULL, NULL);
    if(!viewlog_is_visible())
      viewlog_toggle_visibility(NULL, NULL);
    if(!kbedit_is_visible())
      kbedit_toggle_visibility(NULL, NULL);
    if(!event_sender_is_visible())
      event_sender_toggle_visibility(NULL, NULL);
    if(!stream_infos_is_visible())
      stream_infos_toggle_visibility(NULL, NULL);
    if(!tvset_is_visible())
      tvset_toggle_visibility(NULL, NULL);
    if(!vpplugin_is_visible())
      vpplugin_toggle_visibility(NULL, NULL);
    if(!applugin_is_visible())
      applugin_toggle_visibility(NULL, NULL);
    if(!help_is_visible())
      help_toggle_visibility(NULL, NULL);
    break;
  }
  
}

/*
 * Create buttons from information if input plugins autoplay featured. 
 * We couldn't do this into panel_init(), this function is
 * called before xine engine initialization.
 */
void panel_add_autoplay_buttons (void) {
  char *tips[64];
  const char * const *autoplay_plugins = xine_get_autoplay_input_plugin_ids (__xineui_global_xine_instance);
  unsigned int i;

  if (!autoplay_plugins)
    return;

  for (i = 0; autoplay_plugins[i]; i++) {
    if (i >= sizeof (tips) / sizeof (tips[0]))
      break;
    tips[i] = (char *)xine_get_input_plugin_description (__xineui_global_xine_instance, autoplay_plugins[i]);
  }

  panel->autoplay_buttons = xitk_button_list_new (
    gGui->imlib_data, panel->widget_list,
    gGui->skin_config, "AutoPlayGUI",
    playlist_scan_input, NULL,
    (char **)autoplay_plugins,
    tips, panel->tips.timeout, 0);
}

/*
 * Check if there a mixer control available,
 */
void panel_add_mixer_control(void) {
  
  gGui->mixer.caps = MIXER_CAP_NOTHING;

  if(gGui->ao_port) {
    if((xine_get_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME)) != -1)
      gGui->mixer.caps |= MIXER_CAP_VOL;
    if((xine_get_param(gGui->stream, XINE_PARAM_AUDIO_MUTE)) != -1)
      gGui->mixer.caps |= MIXER_CAP_MUTE;
  }
  
  if(((gGui->mixer.method == SOUND_CARD_MIXER) && gGui->mixer.caps & MIXER_CAP_VOL) ||
     (gGui->mixer.method == SOFTWARE_MIXER)) {
    int level = 0;

    xitk_enable_widget(panel->mixer.slider);
    
    if(gGui->mixer.method == SOUND_CARD_MIXER)
      level = gGui->mixer.volume_level = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME);
    else if(gGui->mixer.method == SOFTWARE_MIXER)
      level = gGui->mixer.amp_level = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);
    
    xitk_slider_set_pos(panel->mixer.slider, level);
  }

  if(gGui->mixer.caps & MIXER_CAP_MUTE) {
    xitk_enable_widget(panel->mixer.mute);
    gGui->mixer.mute = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_MUTE);
    xitk_checkbox_set_state(panel->mixer.mute, gGui->mixer.mute);
  }

  /* Tips should be available only if widgets are enabled */
  if(gGui->mixer.caps & MIXER_CAP_VOL)
    xitk_set_widget_tips_and_timeout(panel->mixer.slider, _("Volume control"), panel->tips.timeout);
  if(gGui->mixer.caps & MIXER_CAP_MUTE)
    xitk_set_widget_tips_and_timeout(panel->mixer.mute, _("Mute toggle"), panel->tips.timeout);

  if(!panel->tips.enable) {
    xitk_disable_widget_tips(panel->mixer.slider);
    xitk_disable_widget_tips(panel->mixer.mute);
  }

}

void panel_deinit(void) {
  if(panel_is_visible())
    _panel_toggle_visibility(NULL, NULL);
  panel_exit(NULL, NULL);
}

void panel_paint(void) {
  if (panel_is_visible())
    xitk_paint_widget_list (panel->widget_list);
}

void panel_reparent(void) {
  if(panel) {
    reparent_window(gGui->panel_window);
  }
}

/*
 * Create the panel window, and fit all widgets in.
 */
void panel_init (void) {
  GC                        gc;
  XSizeHints                hint;
  XWMHints                 *wm_hint;
  XSetWindowAttributes      attr;
  char                     *title = _("xine Panel");
  Atom                      prop;
  MWMHints                  mwmhints;
  XClassHint               *xclasshint;
  xitk_button_widget_t      b;
  xitk_checkbox_widget_t    cb;
  xitk_label_widget_t       lbl;
  xitk_slider_widget_t      sl;
  xitk_widget_t            *w;
  
  XITK_WIDGET_INIT(&b, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  panel = (_panel_t *) calloc(1, sizeof(_panel_t));

  panel->shown_length = ~3;
  panel->shown_time = ~3;

  panel->playback_widgets.enabled = -1;

  panel->skin_on_change = 0;

  XLockDisplay (gGui->display);
  
  /*
   * load bg image before opening window, so we can determine it's size
   */
  if (!(panel->bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 xitk_skin_get_skin_filename(gGui->skin_config, "BackGround")))) {
    xine_error(_("panel: couldn't find image for background\n"));
    exit(-1);
  }

  XUnlockDisplay(gGui->display);

  /*
   * open the panel window
   */

  panel->x = hint.x = xine_config_register_num (__xineui_global_xine_instance, "gui.panel_x", 
						200,
						CONFIG_NO_DESC,
						CONFIG_NO_HELP,
						CONFIG_LEVEL_DEB,
						CONFIG_NO_CB,
						CONFIG_NO_DATA);
  panel->y = hint.y = xine_config_register_num (__xineui_global_xine_instance, "gui.panel_y",
						100,
						CONFIG_NO_DESC,
						CONFIG_NO_HELP,
						CONFIG_LEVEL_DEB,
						CONFIG_NO_CB,
						CONFIG_NO_DATA);

  hint.width = panel->bg_image->rgb_width;
  hint.height = panel->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = False;
  attr.background_pixel  = gGui->black.pixel;
  /*
   * XXX:multivis
   * To avoid BadMatch errors on XCreateWindow:
   * If the parent and the new window have different depths, we must supply either
   * a BorderPixmap or a BorderPixel.
   * If the parent and the new window use different visuals, we must supply a
   * Colormap
   */
  attr.border_pixel      = gGui->black.pixel;

  XLockDisplay(gGui->display);
  attr.colormap          = Imlib_get_colormap(gGui->imlib_data);
  /*  
      printf ("imlib_data: %d visual : %d\n",gGui->imlib_data,gGui->imlib_data->x.visual);
      printf ("w : %d h : %d\n",hint.width, hint.height);
  */
  
  gGui->panel_window = XCreateWindow (gGui->display, 
				      gGui->imlib_data->x.root, 
				      hint.x, hint.y,
				      hint.width, hint.height, 0, 
				      gGui->imlib_data->x.depth,
				      InputOutput,
				      gGui->imlib_data->x.visual,
				      CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
				      &attr);

  {
    /* prevent window manager from killing us through exit() when user closes panel.
       That wont work with OpenGL* video out because libGL does install an exit handler
       that calls X functions - while video out loop still tries the same -> deadlock */
    Atom wm_delete_window = XInternAtom(gGui->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(gGui->display, gGui->panel_window, &wm_delete_window, 1);
  }

  XmbSetWMProperties(gGui->display, gGui->panel_window, title, title, NULL, 0,
                     &hint, NULL, NULL);

  XSelectInput(gGui->display, gGui->panel_window, INPUT_MOTION | KeymapStateMask);
  XUnlockDisplay(gGui->display);
  
  /*
   * The following is more or less a hack to keep the panel window visible
   * with and without focus in windowed and fullscreen mode.
   * Different WMs and even different versions behave different.
   * There is no guarantee that it works with all WMs/versions.
   */
  if(!video_window_is_visible()) {
    if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
      xitk_unset_wm_window_type(gGui->panel_window, WINDOW_TYPE_TOOLBAR);
    xitk_set_wm_window_type(gGui->panel_window, WINDOW_TYPE_NORMAL);
  } else {
    xitk_unset_wm_window_type(gGui->panel_window, WINDOW_TYPE_NORMAL);
    if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
      xitk_set_wm_window_type(gGui->panel_window, WINDOW_TYPE_TOOLBAR);
  }
  
  if(is_layer_above())
    xitk_set_layer_above(gGui->panel_window);
  
  /*
   * wm, no border please
   */

  memset(&mwmhints, 0, sizeof(mwmhints));
  XLockDisplay(gGui->display);
  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, gGui->panel_window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XUnlockDisplay (gGui->display);
  video_window_set_transient_for (gGui->panel_window);
  XLockDisplay (gGui->display);

  /* 
   * set wm properties 
   */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = title;
    xclasshint->res_class = "xine";
    XSetClassHint(gGui->display, gGui->panel_window, xclasshint);
    XFree(xclasshint);
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap   = gGui->icon;
    wm_hint->flags         = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->display, gGui->panel_window, wm_hint);
    XFree(wm_hint);
  }

  /*
   * set background image
   */
  
  gc = XCreateGC(gGui->display, gGui->panel_window, 0, 0);

  Imlib_apply_image(gGui->imlib_data, panel->bg_image, gGui->panel_window);
  XUnlockDisplay(gGui->display);

  /*
   * Widget-list
   */

  panel->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(panel->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(panel->widget_list, WIDGET_LIST_WINDOW, (void *) gGui->panel_window);
  xitk_widget_list_set(panel->widget_list, WIDGET_LIST_GC, gc);
 
  lbl.window    = (XITK_WIDGET_LIST_WINDOW(panel->widget_list));
  lbl.gc        = (XITK_WIDGET_LIST_GC(panel->widget_list));

  /* Prev button */
  b.skin_element_name = "Prev";
  b.callback          = gui_nextprev;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.prev = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));

  /*  Stop button */
  b.skin_element_name = "Stop";
  b.callback          = gui_stop;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.stop = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(panel->playback_widgets.stop, _("Stop playback"));
  
  /*  Play button */
  b.skin_element_name = "Play";
  b.callback          = gui_play;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.play = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(panel->playback_widgets.play, _("Play selected entry"));

  /*  Pause button */
  cb.skin_element_name = "Pause";
  cb.callback          = gui_pause;
  cb.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.pause = xitk_checkbox_create(panel->widget_list, gGui->skin_config, &cb)));
  xitk_set_widget_tips(panel->playback_widgets.pause, _("Pause/Resume playback"));
  
  /*  Next button */
  b.skin_element_name = "Next";
  b.callback          = gui_nextprev;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.next = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));

  /*  Eject button */
  b.skin_element_name = "Eject";
  b.callback          = gui_eject;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
   (panel->playback_widgets.eject = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(panel->playback_widgets.eject, _("Eject current medium"));

  /*  Exit button */
  b.skin_element_name = "Exit";
  b.callback          = gui_exit;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)),
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Quit"));
 
  /*  Setup button */
  b.skin_element_name = "Setup";
  b.callback          = gui_setup_show;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Setup window"));

  /*  Event sender button */
  b.skin_element_name = "Nav";
  b.callback          = gui_event_sender_show;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Navigator"));
  
  /*  Close button */
  b.skin_element_name = "Close";
  b.callback          = panel_toggle_visibility;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)),
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Hide GUI"));

  /*  Fullscreen button */
  b.skin_element_name = "FullScreen";
  b.callback          = gui_set_fullscreen_mode;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Fullscreen/Window mode"));

  /*  Next audio channel */
  b.skin_element_name = "AudioNext";
  b.callback          = gui_change_audio_channel;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Next audio channel"));

  /*  Prev audio channel */
  b.skin_element_name = "AudioPrev";
  b.callback          = gui_change_audio_channel;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Previous audio channel"));

  /*  Prev spuid */
  b.skin_element_name = "SpuNext";
  b.callback          = gui_change_spu_channel;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Next SPU channel"));

  /*  Next spuid */
  b.skin_element_name = "SpuPrev";
  b.callback          = gui_change_spu_channel;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Previous SPU channel"));

  /*  Label title */
  lbl.skin_element_name = "TitleLabel";
  lbl.label             = "";
  lbl.callback          = _panel_change_display_mode;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->title_label = xitk_label_create (panel->widget_list, gGui->skin_config, &lbl)));

  /*  Runtime label */
  lbl.skin_element_name = "TimeLabel";
  lbl.label             = "00:00:00";
  lbl.callback          = _panel_change_time_label;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->runtime_label = xitk_label_create (panel->widget_list, gGui->skin_config, &lbl)));
  xitk_set_widget_tips(panel->runtime_label, _("Total time: --:--:--"));
  /* 
   * Init to default, otherwise if panel is hide
   * at startup, label is empty 'till it's updated
   */
  panel->runtime_mode   = 0;
  panel_reset_runtime_label();

  /*  Audio channel label */
  lbl.skin_element_name = "AudioLabel";
  lbl.label             = "";
  lbl.callback          = panel_audio_lang_list;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->audiochan_label = xitk_label_create (panel->widget_list, gGui->skin_config, &lbl)));

  /*  Spuid label */
  lbl.skin_element_name = "SpuLabel";
  lbl.label             = "";
  lbl.callback          = panel_spu_lang_list;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->spuid_label = xitk_label_create (panel->widget_list, gGui->skin_config, &lbl)));

  /*  slider seek */
  sl.skin_element_name = "SliderPlay";
  sl.min               = 0;
  sl.max               = 65535;
  sl.step              = 1;
  sl.callback          = NULL;
  sl.userdata          = NULL;
  sl.motion_callback   = panel_slider_cb;
  sl.motion_userdata   = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
    (panel->playback_widgets.slider_play = xitk_slider_create(panel->widget_list, gGui->skin_config, &sl)));
  
  xitk_widget_keyable(panel->playback_widgets.slider_play, 0);

  xitk_set_widget_tips(panel->playback_widgets.slider_play, _("Stream playback position slider"));
  xitk_slider_reset(panel->playback_widgets.slider_play);
  
  if(!gGui->playlist.num)
    xitk_disable_widget(panel->playback_widgets.slider_play);
  
  /* Mixer volume slider */
  sl.skin_element_name = "SliderVol";
  sl.min               = 0;
  sl.max               = (gGui->mixer.method == SOUND_CARD_MIXER) ? 100 : 200;
  sl.step              = 1;
  sl.callback          = NULL;
  sl.userdata          = NULL;
  sl.motion_callback   = panel_slider_cb;
  sl.motion_userdata   = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->mixer.slider = xitk_slider_create(panel->widget_list, gGui->skin_config, &sl)));
  xitk_slider_reset(panel->mixer.slider);
  xitk_disable_widget(panel->mixer.slider);

  /*  Mute toggle */
  cb.skin_element_name = "Mute";
  cb.callback          = panel_toggle_audio_mute;
  cb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
	   (panel->mixer.mute = xitk_checkbox_create (panel->widget_list, gGui->skin_config, &cb)));
  xitk_disable_widget(panel->mixer.mute);

  /* Snapshot */
  b.skin_element_name = "Snapshot";
  b.callback          = panel_snapshot;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Take a snapshot"));

  /*  Playback speed slow */
  b.skin_element_name = "PlaySlow";
  b.callback          = gui_change_speed_playback;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Slow motion"));

  /*  Playback speed fast */
  b.skin_element_name = "PlayFast";
  b.callback          = gui_change_speed_playback;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Fast motion"));
  
  /*  Playlist button */
  b.skin_element_name = "PlBtn";
  b.callback          = gui_playlist_show;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Playlist"));
  
  /*  Control button */
  b.skin_element_name = "CtlBtn";
  b.callback          = gui_control_show;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Control"));

  /*  Mrl button */
  b.skin_element_name = "MrlBtn";
  b.callback          = gui_mrlbrowser_show;
  b.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(panel->widget_list)), 
			   (w = xitk_button_create(panel->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Open Location"));
  

  panel->tips.enable = xine_config_register_bool(__xineui_global_xine_instance, "gui.tips_visible", 
						 1,
						 _("gui tips visibility"), 
						 _("If disabled, no tooltips are shown."), 
						 CONFIG_LEVEL_ADV,
						 panel_enable_tips_cb,
						 CONFIG_NO_DATA);
  panel->tips.timeout = (unsigned long) xine_config_register_num(__xineui_global_xine_instance, "gui.tips_timeout", 
								 5000,
								 _("Tips timeout (ms)"), 
								 _("Persistence time of tooltips, in milliseconds."), 
								 CONFIG_LEVEL_ADV,
								 panel_timeout_tips_cb, 
								 CONFIG_NO_DATA);

  panel_update_mrl_display();
  panel_update_nextprev_tips();

  /* 
   * show panel 
   */
  {
    int visible = xine_config_register_bool(__xineui_global_xine_instance, "gui.panel_visible", 
					    1,
					    _("gui panel visibility"),
					    CONFIG_NO_HELP,
					    CONFIG_LEVEL_DEB,
					    CONFIG_NO_CB,
					    CONFIG_NO_DATA);

      panel->visible = (!gGui->no_gui) ? visible : 0;
  }
  
  /*  The user don't want panel on startup */
  if(panel->visible && (actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_VISIBLITY))) {
    panel->visible = !panel->visible;
    config_update_num ("gui.panel_visible", panel->visible);
  }
  
  if((gGui->use_root_window || gGui->video_display != gGui->display) && (!panel->visible))
    panel->visible = 1;
  
  if (panel->visible) {
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, gGui->panel_window); 
    XMapWindow(gGui->display, gGui->panel_window); 
    XUnlockDisplay(gGui->display);
  }
  else
    xitk_hide_widgets(panel->widget_list);
  
  
  panel->widget_key = xitk_register_event_handler("panel", 
						  gGui->panel_window, 
						  panel_handle_event,
						  panel_store_new_position,
						  gui_dndcallback,
						  panel->widget_list,
						  NULL);

  gGui->cursor_visible = 1;
  video_window_set_cursor_visibility(gGui->cursor_visible);
  
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
    
    pthread_create(&panel->slider_thread, &pth_attrs, slider_loop, NULL);
  }

  if(panel->visible)
    try_to_set_input_focus(gGui->panel_window);
}

void panel_set_title(char *title) {
  if(panel && panel->title_label)
    xitk_label_change_label(panel->title_label, title);
}

