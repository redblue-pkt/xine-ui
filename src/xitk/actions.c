/* 
 * Copyright (C) 2000-2001 the xine project
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

#include <X11/Xlib.h>
#include <xine/video_out_x11.h>

#include "xitk.h"

#include "event.h"
#include "control.h"
#include "actions.h"
#include "lirc.h"
#include "panel.h"
#include "playlist.h"
#include "videowin.h"
#include "mrl_browser.h"

extern gGui_t          *gGui;

/*
 * Callback-functions for widget-button click
 */
void gui_exit (widget_t *w, void *data) {

  printf("xine-panel: EXIT\n");
  
  config_save();

  xine_exit(gGui->xine); 

  gGui->running = 0;

#ifdef HAVE_LIRC
  if(gGui->lirc_enable) {
    deinit_lirc();
  }
#endif
  widget_stop();
}

void gui_play (widget_t *w, void *data) {

  fprintf(stderr, "xine-panel: PLAY\n");

  if (xine_get_status (gGui->xine) != XINE_PLAY) {
    video_window_hide_logo();
    xine_play (gGui->xine, gGui->filename, 0, 0 );
  } else {
    xine_set_speed(gGui->xine, SPEED_NORMAL);
  }

  panel_check_pause();
}

void gui_stop (widget_t *w, void *data) {

  gGui->ignore_status = 1;
  xine_stop (gGui->xine);
  video_window_show_logo();
  gGui->ignore_status = 0; 
  panel_reset_slider ();
  panel_check_pause();
}

void gui_pause (widget_t *w, void *data, int state) {
  
  if (xine_get_speed (gGui->xine) != SPEED_PAUSE)
    xine_set_speed(gGui->xine, SPEED_PAUSE);
  else
    xine_set_speed(gGui->xine, SPEED_NORMAL);
  panel_check_pause();
}

void gui_eject(widget_t *w, void *data) {
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

void gui_toggle_fullscreen(widget_t *w, void *data) {

  video_window_set_fullscreen (!video_window_is_fullscreen ());

  /* Drawable has changed, update cursor visiblity */
  if(!gGui->cursor_visible) {
    video_window_set_cursor_visibility(gGui->cursor_visible);
  }
  
  if (panel_is_visible())  {
    pl_raise_window();
    control_raise_window();
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
  }
  
  if(mrl_browser_is_visible()) {
    show_mrl_browser();
    set_mrl_browser_transient();
  }
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

void gui_toggle_interlaced(void) {

  gGui->vo_driver->set_property (gGui->vo_driver, VO_PROP_INTERLACED,
                                 gGui->vo_driver->get_property (gGui->vo_driver, VO_PROP_INTERLACED) + 1);

  if (panel_is_visible())  {
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display,
                          gGui->panel_window, gGui->video_window);
  }
}

void gui_change_audio_channel(widget_t *w, void *data) {
  
  if(((int)data) == GUI_NEXT) {
    xine_select_audio_channel(gGui->xine,
			      (xine_get_audio_channel(gGui->xine) + 1));
  }
  else if(((int)data) == GUI_PREV) {
    if(xine_get_audio_channel(gGui->xine))
      xine_select_audio_channel(gGui->xine, 
				(xine_get_audio_channel(gGui->xine) - 1));
  }
  
  panel_update_channel_display ();
}

void gui_change_spu_channel(widget_t *w, void *data) {
  
  if(((int)data) == GUI_NEXT) {
    xine_select_spu_channel(gGui->xine, 
			    (xine_get_spu_channel(gGui->xine) + 1));
  }
  else if(((int)data) == GUI_PREV) {
    if(xine_get_spu_channel(gGui->xine) >= 0) {
      /* 
       * Subtitle will be disabled; so hide it otherwise 
       * the latest still be showed forever.
       */
      /* FIXME
      if((xine_get_spu_channel(gGui->xine) - 1) == -1)
      	vo_hide_spu();
      */

      xine_select_spu_channel(gGui->xine, 
			      (xine_get_spu_channel(gGui->xine) - 1));
    }
  }

  panel_update_channel_display ();
  
}

void gui_set_current_position (int pos) {

  gGui->ignore_status = 1;
  xine_play (gGui->xine, gGui->filename, pos, 0);
  gGui->ignore_status = 0;
  panel_check_pause();
}

void gui_seek_relative (int off_sec) {

  int sec;

  gGui->ignore_status = 1;

  sec = xine_get_current_time (gGui->xine);
  if ((sec + off_sec) < 0) 
    sec = 0;
  else
    sec += off_sec;

  xine_play (gGui->xine, gGui->filename, 0, sec);

  gGui->ignore_status = 0;
  panel_check_pause();
}

void gui_dndcallback (char *filename) {

  //  printf("%s() add %s\n", __FUNCTION__, filename);

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

void gui_nextprev(widget_t *w, void *data) {

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
      xine_play (gGui->xine, gGui->filename, 0, 0 );
    } else {
      video_window_show_logo();
      gGui->playlist_cur = 0;
    }
    gGui->ignore_status = 0;
  }

  panel_check_pause();
}

void gui_playlist_show(widget_t *w, void *data) {

  if(!pl_is_running()) {
    playlist_editor();
  }
  else {
    pl_exit(NULL, NULL);
  }
}

void gui_mrlbrowser_show(widget_t *w, void *data) {

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
    sprintf(gGui->filename, "DROP A FILE ON XINE");  
  
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

void gui_control_show(widget_t *w, void *data) {

  if(control_is_running() && !control_is_visible())
    control_toggle_panel_visibility(NULL, NULL);
  else if(!control_is_running())
    control_panel();
  else
    control_exit(NULL, NULL);
}
