/* 
 * Copyright (C) 2000-2002 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 * implementation of all the various actions for the gui (play, stop, open, pause...)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <X11/Xlib.h>
#include <xine/video_out_x11.h>

#include "event.h"
#include "control.h"
#include "actions.h"
#include "lirc.h"
#include "panel.h"
#include "playlist.h"
#include "videowin.h"
#include "mrl_browser.h"
#include "setup.h"
#include "viewlog.h"
#include "errors.h"
#include "i18n.h"

#include "xitk.h"

extern gGui_t          *gGui;
extern _panel_t        *panel;

/*
 * Callback-functions for widget-button click
 */
void gui_exit (xitk_widget_t *w, void *data) {
  
  video_window_exit ();

#ifdef HAVE_XF86VIDMODE
  /* just in case a different modeline than the original one is running,
   * toggle back to window mode which automatically causes a switch back to
   * the original modeline
   */
  if(gGui->XF86VidMode_fullscreen)
     video_window_set_fullscreen_mode (0);
//     gui_set_fullscreen_mode(NULL,NULL);
#endif
   
  destroy_mrl_browser();
  control_exit(NULL, NULL);
  pl_exit(NULL, NULL);

  config_save();

  xine_exit(gGui->xine); 

  gGui->running = 0;

#ifdef HAVE_LIRC
  if(gGui->lirc_enable)
    deinit_lirc();
#endif

  xitk_stop();
  /* 
   * This prevent xine waiting till the end of time for an
   * XEvent when lirc (and futur other control ways) is used to quit .
   */
  gui_send_expose_to_window(gGui->video_window);
  xitk_skin_unload_config(gGui->skin_config);
}

void gui_play (xitk_widget_t *w, void *data) {

  if (xine_get_status (gGui->xine) != XINE_PLAY) {

    if (!strncmp (gGui->filename, "xine-ui version", 15)) {
      xine_error (_("No MRL (input stream) specified"));
      return;
    }

    if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
      gui_handle_xine_error();
  } else {
    xine_set_speed(gGui->xine, SPEED_NORMAL);
  }

  panel_check_pause();
}

void gui_stop (xitk_widget_t *w, void *data) {

  gGui->ignore_status = 1;
  xine_stop (gGui->xine);
  gGui->ignore_status = 0; 
  panel_reset_slider ();
  panel_check_pause();
  panel_update_runtime_display();
}

void gui_pause (xitk_widget_t *w, void *data, int state) {
  
  if (xine_get_speed (gGui->xine) != SPEED_PAUSE)
    xine_set_speed(gGui->xine, SPEED_PAUSE);
  else
    xine_set_speed(gGui->xine, SPEED_NORMAL);
  panel_check_pause();
}

void gui_eject(xitk_widget_t *w, void *data) {
  char *tmp_playlist[MAX_PLAYLIST_LENGTH];
  int i, new_num = 0;
  
  if (xine_eject(gGui->xine)) {

    if(gGui->playlist_num) {
      char  *tok = NULL;
      char  *mrl;
      int    len;
      /*
       * If it's an mrl (____://) remove all of them in playlist
       */
      mrl = strstr(gGui->playlist[gGui->playlist_cur], "://");
      if(mrl) {
	len = (mrl - gGui->playlist[gGui->playlist_cur]) + 4;
	tok = (char *) alloca(len + 1);
	memset(tok, 0, len + 1);
  	snprintf(tok, len, "%s", gGui->playlist[gGui->playlist_cur]);
      }

      if(tok != NULL) {
	/* 
	 * Store all of not maching entries
	 */
	for(i=0; i < gGui->playlist_num; i++) {
	  if(strncasecmp(gGui->playlist[i], tok, strlen(tok))) {
	    tmp_playlist[new_num] = gGui->playlist[i];
	    new_num++;
	  }
	}
	/*
	 * Create new _cleaned_ playlist
	 */
	memset(&gGui->playlist, 0, sizeof(gGui->playlist));
	for(i=0; i<new_num; i++)
	  gGui->playlist[i] = tmp_playlist[i];
	
	gGui->playlist_num = new_num;
	if(new_num)
	  gGui->playlist_cur = 0;
      }
      else {
	/*
	 * Remove only the current MRL
	 */
	for(i = gGui->playlist_cur; i < gGui->playlist_num; i++)
	  gGui->playlist[i] = gGui->playlist[i+1];
	
	gGui->playlist_num--;
	if(gGui->playlist_cur) gGui->playlist_cur--;
      }
    }
    
    gui_set_current_mrl(gGui->playlist [gGui->playlist_cur]);
    pl_update_playlist();
  }
}

void gui_toggle_visibility(xitk_widget_t *w, void *data) {

  if(panel_is_visible()) {
    video_window_set_visibility(!(video_window_is_visible()));
    XMapRaised (gGui->display, gGui->panel_window); 

    if(gGui->reparent_hack) {
      panel_toggle_visibility(NULL, NULL);
      panel_toggle_visibility(NULL, NULL);
    }
    else
      layer_above_video(gGui->panel_window);
  }
}

void gui_set_fullscreen_mode(xitk_widget_t *w, void *data) {

  if(!(video_window_is_visible()))
    video_window_set_visibility(1);
  
  video_window_set_fullscreen_mode (video_window_get_fullscreen_mode () + 1);
  
  /* Drawable has changed, update cursor visiblity */
  if(!gGui->cursor_visible) {
    video_window_set_cursor_visibility(gGui->cursor_visible);
  }
  
  if (panel_is_visible())  {
    XMapRaised (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    
    if(gGui->reparent_hack) {
      panel_toggle_visibility(NULL, NULL);
      panel_toggle_visibility(NULL, NULL);
    }
    else
      layer_above_video(gGui->panel_window);
  }

  if(mrl_browser_is_visible()) {
    show_mrl_browser();
    set_mrl_browser_transient();
  }

  if(pl_is_visible())
    pl_raise_window();
  
  if(control_is_visible())
    control_raise_window();
  
  if(setup_is_visible())
    setup_raise_window();

  if(viewlog_is_visible())
    viewlog_raise_window();

  if(kbedit_is_visible())
    kbedit_raise_window();
}

void gui_toggle_aspect(void) {
  
  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_ASPECT_RATIO,
				 gGui->vo_driver->get_property (gGui->vo_driver, VO_PROP_ASPECT_RATIO) + 1);
  
  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
  }
}

void gui_set_aspect_ratio(int ratio_code) {
  
  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_ASPECT_RATIO, ratio_code);
  
  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
  }
}

void gui_toggle_interlaced(void) {

  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_INTERLACED,
                                 1-gGui->vo_driver->get_property (gGui->vo_driver, VO_PROP_INTERLACED));

  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display,
                          gGui->panel_window, gGui->video_window);
  }
}

void gui_change_audio_channel(xitk_widget_t *w, void *data) {
  
  if(((int)data) == GUI_NEXT) {
    xine_select_audio_channel(gGui->xine,
			      (xine_get_audio_selection(gGui->xine) + 1));
  }
  else if(((int)data) == GUI_PREV) {
    xine_select_audio_channel(gGui->xine, 
			      (xine_get_audio_selection(gGui->xine) - 1));
  }
  
  panel_update_channel_display ();
}

void gui_change_spu_channel(xitk_widget_t *w, void *data) {
  
  if(((int)data) == GUI_NEXT) {
    xine_select_spu_channel(gGui->xine, 
			    (xine_get_spu_channel(gGui->xine) + 1));
  }
  else if(((int)data) == GUI_PREV) {
    xine_select_spu_channel(gGui->xine, 
			    (xine_get_spu_channel(gGui->xine) - 1));
  }

  panel_update_channel_display ();
  
}

void gui_change_speed_playback(xitk_widget_t *w, void *data) {

  if(((int)data) == GUI_NEXT) {
    if (xine_get_speed (gGui->xine) > SPEED_PAUSE)
      xine_set_speed (gGui->xine, xine_get_speed (gGui->xine)/2);
  }
  else if(((int)data) == GUI_PREV) {
    if (xine_get_speed (gGui->xine) < SPEED_FAST_4) {
      if (xine_get_speed (gGui->xine) > SPEED_PAUSE)
	xine_set_speed (gGui->xine, xine_get_speed (gGui->xine)*2);
      else
	xine_set_speed (gGui->xine, SPEED_SLOW_4);
    }
  }
  else if(((int)data) == GUI_RESET) {
    xine_set_speed (gGui->xine, SPEED_NORMAL);
  }
  
}

void gui_set_current_position (int pos) {

  if((xine_is_stream_seekable (gGui->xine)) == 0)
     return;

  gGui->ignore_status = 1;
  if(!xine_play (gGui->xine, gGui->filename, pos, 0))
    gui_handle_xine_error();
  
  gGui->ignore_status = 0;
  panel_check_pause();
}

void gui_seek_relative (int off_sec) {

  int sec;

  if (!xine_is_stream_seekable (gGui->xine)) 
    return;

  gGui->ignore_status = 1;

  sec = xine_get_current_time (gGui->xine);
  if ((sec + off_sec) < 0) 
    sec = 0;
  else
    sec += off_sec;

  if(!xine_play (gGui->xine, gGui->filename, 0, sec))
    gui_handle_xine_error();

  gGui->ignore_status = 0;
  panel_check_pause();
}

void gui_dndcallback (char *filename) {

  if(filename) {
    gGui->playlist_cur = gGui->playlist_num++;
    gGui->playlist[gGui->playlist_cur] = strdup(filename);

    if((xine_get_status(gGui->xine) == XINE_STOP)) {
      gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
      /* IS IT REALLY A GOOD IDEA ?? gui_play (NULL, NULL);  */
    }

    pl_update_playlist();
  }
}

void gui_nextprev(xitk_widget_t *w, void *data) {

  if(((int)data) == GUI_NEXT) {
    gGui->ignore_status = 1;
    xine_stop (gGui->xine);
    gGui->ignore_status = 0;
    gui_status_callback (XINE_STOP);
  }
  else if(((int)data) == GUI_PREV) {
    gGui->ignore_status = 1;
    xine_stop (gGui->xine);
    gGui->playlist_cur--;
    if ((gGui->playlist_cur>=0) && (gGui->playlist_cur < gGui->playlist_num)) {
      gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
      if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
	gui_handle_xine_error();

    } else {
      gGui->playlist_cur = 0;
    }
    gGui->ignore_status = 0;
  }

  panel_check_pause();
}

void gui_playlist_show(xitk_widget_t *w, void *data) {

  if(!pl_is_running()) {
    playlist_editor();
  }
  else {
    if(pl_is_visible())
      pl_exit(NULL, NULL);
    else
      pl_toggle_visibility(NULL, NULL);
  }

}

void gui_mrlbrowser_show(xitk_widget_t *w, void *data) {

  if(!mrl_browser_is_running()) {
    open_mrlbrowser(NULL, NULL);
  }
  else {

    if(!mrl_browser_is_visible()) {
      show_mrl_browser();
      set_mrl_browser_transient();
    }
    else {
      destroy_mrl_browser();
    }

  }

}

void gui_set_current_mrl(char *mrl) {

  if(mrl)
    strcpy(gGui->filename, mrl);
  else
    panel_set_no_mrl();
  
  panel_update_mrl_display ();
}

char *gui_get_next_mrl () {
  if (gGui->playlist_cur < gGui->playlist_num-1) {
    return gGui->playlist[gGui->playlist_cur + 1];
  } else
    return NULL;
}

/*
void gui_notify_demux_branched () {
  gGui->gui_playlist_cur ++;
  strcpy(gGui->gui_filename, gGui->gui_playlist [gGui->gui_playlist_cur]);
  label_change_label (gui_widget_list, gui_title_label, gGui->gui_filename);
}
*/

void gui_control_show(xitk_widget_t *w, void *data) {

  if(control_is_running() && !control_is_visible())
    control_toggle_visibility(NULL, NULL);
  else if(!control_is_running())
    control_panel();
  else
    control_exit(NULL, NULL);
}

void gui_setup_show(xitk_widget_t *w, void *data) {

  if (setup_is_running() && !setup_is_visible())
    setup_toggle_visibility(NULL, NULL);
  else if(!setup_is_running())
    setup_panel();
  else
    setup_exit(NULL, NULL);
}

void gui_viewlog_show(xitk_widget_t *w, void *data) {

  if (viewlog_is_running() && !viewlog_is_visible())
    viewlog_toggle_visibility(NULL, NULL);
  else if(!viewlog_is_running())
    viewlog_window();
  else
    viewlog_exit(NULL, NULL);
}

void gui_kbedit_show(xitk_widget_t *w, void *data) {

  if (kbedit_is_running() && !kbedit_is_visible())
    kbedit_toggle_visibility(NULL, NULL);
  else if(!kbedit_is_running())
    kbedit_window();
  else
    kbedit_exit(NULL, NULL);
}

/*
 * set window layer property to something above GNOME (and KDE?) panel
 * (reset to normal if do_raise == 0)
 */
void layer_above_video(Window w) {

  static Atom XA_WIN_LAYER = None;
  XEvent xev;

  if(!gGui->layer_above)
    return;
  
  if( XA_WIN_LAYER == None )
    XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);

  xev.type = ClientMessage;
  xev.xclient.type = ClientMessage;
  xev.xclient.window = w;
  xev.xclient.message_type = XA_WIN_LAYER;
  xev.xclient.format = 32;

  /* top layer if video is fullscreen, otherwise normal layer */
  if (video_window_get_fullscreen_mode() && video_window_is_visible()) 
    xev.xclient.data.l[0] = (long) 10;
  else
    xev.xclient.data.l[0] = (long) 4;

  xev.xclient.data.l[1] = 0;

  XSendEvent(gGui->display, gGui->imlib_data->x.root, False,
	     SubstructureNotifyMask, (XEvent*) &xev);
}

void gui_increase_audio_volume(void) {

  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
    if(gGui->mixer.volume_level < 100) {
      gGui->mixer.volume_level++;
      xine_set_audio_property(gGui->xine, gGui->mixer.volume_mixer, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
    }
  }
}

void gui_decrease_audio_volume(void) {
  
  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
    if(gGui->mixer.volume_level > 0) {
      gGui->mixer.volume_level--;
      xine_set_audio_property(gGui->xine, gGui->mixer.volume_mixer, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
    }
  }
}

void gui_change_zoom(int zoom_d) {

  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_ZOOM_FACTOR,
				 gGui->vo_driver->get_property (gGui->vo_driver, VO_PROP_ZOOM_FACTOR) + zoom_d);
  
  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
  }
}

/*
 * Reset zooming by recall aspect ratio.
 */
void gui_reset_zoom(void) {
  
  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_ZOOM_FACTOR, 100);
  
  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
  }
}

/* 
 * Toggle TV Modes on the dxr3
 */
void gui_toggle_tvmode(void) {

  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_TVMODE,
  				 gGui->vo_driver->get_property (gGui->vo_driver, VO_PROP_ASPECT_RATIO));
  				 
}

/*
 * Send an Expose event to given window.
 */
void gui_send_expose_to_window(Window window) {
  XEvent xev;

  if(window == None)
    return;

  xev.xany.type          = Expose;
  xev.xexpose.type       = Expose;
  xev.xexpose.send_event = True;
  xev.xexpose.display    = gGui->display;
  xev.xexpose.window     = window;
  xev.xexpose.count      = 0;
  
  XLockDisplay(gGui->display);
  if(!XSendEvent(gGui->display, window, False, ExposureMask, &xev)) {
    fprintf(stderr, "XSendEvent(display, 0x%x ...) failed.\n", (unsigned int) window);
  }
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);
  
}
