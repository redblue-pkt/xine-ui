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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <X11/Xlib.h>
#include <xine/xineutils.h>
#include <pthread.h>

#include "common.h"

extern gGui_t          *gGui;
extern _panel_t        *panel;

static pthread_t        seek_thread;

/*
 *
 */
void gui_display_logo(void) {

  gGui->logo_mode = 2;
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
    gGui->ignore_next = 1;
    xine_stop(gGui->stream);
    gGui->ignore_next = 0; 
  }

  if(gGui->visual_anim.running)
    visual_anim_stop();

  (void) gui_xine_open_and_play((char *)gGui->logo_mrl, 0, 0);
  gGui->logo_mode = 1;
  panel_reset_slider();
  if(stream_infos_is_visible())
    stream_infos_update_infos();
}

int gui_xine_play(xine_stream_t *stream, int start_pos, int start_time_in_secs) {
  int ret;
  
  if(start_time_in_secs)
    start_time_in_secs *= 1000;

  if((ret = xine_play(stream, start_pos, start_time_in_secs)) == 0) {
    gui_handle_xine_error(stream);
  }
  else {
    char *ident;
    
    if(gGui->logo_mode != 2)
      gGui->logo_mode = 0;
    
    if(gGui->logo_mode == 0) {
      int has_video = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);
      
      if(stream_infos_is_visible())
	stream_infos_update_infos();
      
      if((ident = stream_infos_get_ident_from_stream(stream)) != NULL) {
	
	if(gGui->mmk.ident)
	  free(gGui->mmk.ident);
	if(gGui->playlist.mmk[gGui->playlist.cur]->ident)
	  free(gGui->playlist.mmk[gGui->playlist.cur]->ident);
	
	gGui->mmk.ident = strdup(ident);
	gGui->playlist.mmk[gGui->playlist.cur]->ident = strdup(ident);
	
	playlist_mrlident_toggle();
	panel_update_mrl_display();
	free(ident);
      }

      if(has_video) {
	
	if(gGui->visual_anim.running)
	  visual_anim_stop();
	
	if(gGui->auto_vo_visibility) {
	  
	  if(!video_window_is_visible())
	    video_window_set_visibility(1);
	  
	  if(gGui->auto_panel_visibility && (panel_is_visible()))
	    panel_toggle_visibility(NULL, NULL);
	  
	}
	else {
	  
	  if(gGui->auto_panel_visibility && (panel_is_visible()))
	    panel_toggle_visibility(NULL, NULL);
	  
	}
	
      }
      else {
	
	if(gGui->auto_vo_visibility) {
	  
	  if(gGui->auto_panel_visibility) {
	    
	    if(video_window_is_visible())
	      video_window_set_visibility(0);
	    
	    if(!panel_is_visible())
	      panel_toggle_visibility(NULL, NULL);
	  }
	  else {

	    if((panel_is_visible()) && (video_window_is_visible()))
	      video_window_set_visibility(0);

	  }
	}
	else {

	  if(gGui->auto_panel_visibility && (video_window_is_visible()) && (!panel_is_visible()))
	    panel_toggle_visibility(NULL, NULL);
	  
	  if(video_window_is_visible()) {
	    if(!gGui->visual_anim.running)
	      visual_anim_play();
	  }
	  else
	    gGui->visual_anim.running = 2;
	  
	}
      }
    }
  }
  
  return ret;
}

int gui_xine_open_and_play(char *mrl, int start_pos, int start_time) {
  
  if(!xine_open(gGui->stream, (const char *)mrl)) {
    gui_handle_xine_error(gGui->stream);
    return 0;
  }
  if(!gui_xine_play(gGui->stream, start_pos, start_time)) {
    return 0;
  }

  return 1;
}

/*
 * Callback-functions for widget-button click
 */
void gui_exit (xitk_widget_t *w, void *data) {
  
  gui_stop(NULL, NULL);
  
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
  playlist_exit(NULL, NULL);

  config_save();

  xine_close(gGui->stream);
  xine_close(gGui->visual_anim.stream);

  xine_event_dispose_queue(gGui->event_queue);
  xine_event_dispose_queue(gGui->visual_anim.event_queue);

  xine_dispose(gGui->stream);
  /* xine_dispose(gGui->visual_anim.stream); */

  if(gGui->vo_port)
    xine_close_video_driver(gGui->xine, gGui->vo_port);

  if(gGui->ao_port)
    xine_close_audio_driver(gGui->xine, gGui->ao_port);

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

  video_window_reset_ssaver();
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
    if(gGui->logo_mode != 0) {
      gGui->ignore_next = 1;
      xine_stop(gGui->stream);
      gGui->ignore_next = 0; 
    }
  }
  
  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY) {

    if (!strncmp(gGui->mmk.ident, "xine-ui version", 15)) {
      xine_error (_("No MRL (input stream) specified"));
      return;
    }
    
    gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
    if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
      gui_display_logo();

  } 
  else {
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
    visual_anim_pause();
  }
  
  panel_check_pause();
}

void gui_stop (xitk_widget_t *w, void *data) {

  gGui->ignore_next = 1;
  xine_stop (gGui->stream);
  gGui->ignore_next = 0;

  mediamark_reset_played_state();

  if(gGui->visual_anim.running) {
    xine_stop(gGui->visual_anim.stream);
    gGui->visual_anim.running = 0;
  }

  panel_reset_slider ();
  panel_check_pause();
  panel_update_runtime_display();

  if(is_playback_widgets_enabled() && (!gGui->playlist.num) && (!gGui->mmk.mrl)) {
    gui_set_current_mrl(NULL);
    enable_playback_controls(0);
  }
  gui_display_logo();
}

void gui_pause (xitk_widget_t *w, void *data, int state) {
  
  if(xine_get_param(gGui->stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
  else
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);

  panel_check_pause();
  visual_anim_pause();
}

void gui_eject(xitk_widget_t *w, void *data) {
  int i;
  
  if(xine_eject(gGui->stream)) {
    
    if(gGui->playlist.num) {
      mediamark_t **mmk = NULL;
      char         *tok = NULL;
      char         *mrl;
      int           len;
      int           new_num = 0;
      /*
       * If it's an mrl (____://) remove all of them in playlist
       */
      mrl = strstr((mediamark_get_current_mrl()), ":/");
      if(mrl) {
	char *cur_mrl = (char *)mediamark_get_current_mrl();
	
	len = (mrl - cur_mrl) + 3;
	tok = (char *) alloca(len + 1);
	memset(tok, 0, len + 1);
  	snprintf(tok, len, "%s", cur_mrl);
      }

      if(tok != NULL) {
	/* 
	 * Store all of not maching entries
	 */
	for(i = 0; i < gGui->playlist.num; i++) {
	  if(strncasecmp(gGui->playlist.mmk[i]->mrl, tok, strlen(tok))) {
	    
	    if(!new_num)
	      mmk = (mediamark_t **) xine_xmalloc(sizeof(mediamark_t *) * 2);
	    else { 
	      if(new_num > 1) {
		mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (new_num + 1));
	      }
	    }
	    
	    (void) mediamark_store_mmk(&mmk[new_num], 
				       gGui->playlist.mmk[i]->mrl, gGui->playlist.mmk[i]->mrl,
				       gGui->playlist.mmk[i]->start, gGui->playlist.mmk[i]->end);
	    new_num++;
	  }
	}

	/*
	 * flip playlists.
	 */
	mediamark_free_mediamarks();
	if(mmk)
	  gGui->playlist.mmk = mmk;

	gGui->playlist.num = new_num;
	
	if(new_num)
	  gGui->playlist.cur = 0;
     }
      else {
	/*
	 * Remove only the current MRL
	 */
	mediamark_free_entry(gGui->playlist.cur);

	for(i = gGui->playlist.cur; i < gGui->playlist.num; i++)
	  gGui->playlist.mmk[i] = gGui->playlist.mmk[i + 1];
	
	gGui->playlist.mmk = (mediamark_t **) 
	  realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 2));

	gGui->playlist.mmk[gGui->playlist.num] = NULL;

	if(gGui->playlist.cur)
	  gGui->playlist.cur--;

      }

      if(is_playback_widgets_enabled() && (!gGui->playlist.num)) {
	enable_playback_controls(0);
	gui_display_logo();
      }

    }
    
    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    playlist_update_playlist();
  }
}

void gui_toggle_visibility(xitk_widget_t *w, void *data) {

  if(panel_is_visible()) {
    video_window_set_visibility(!(video_window_is_visible()));
    XLockDisplay(gGui->display);
    XMapRaised (gGui->display, gGui->panel_window); 
    XUnlockDisplay(gGui->display);

    if(gGui->reparent_hack) {
      panel_toggle_visibility(NULL, NULL);
      panel_toggle_visibility(NULL, NULL);
    }
    else
      layer_above_video(gGui->panel_window);

    /* (re)start/stop visual animation */
    if(video_window_is_visible()) {
      if(gGui->visual_anim.enabled && (gGui->visual_anim.running == 2))
	visual_anim_play();
    }
    else {
      if(gGui->visual_anim.running) {
	visual_anim_stop();
	gGui->visual_anim.running = 2;
      }
    }

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
    XLockDisplay(gGui->display);
    XMapRaised (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);

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

  if(playlist_is_visible())
    playlist_raise_window();
  
  if(control_is_visible())
    control_raise_window();
  
  if(setup_is_visible())
    setup_raise_window();

  if(viewlog_is_visible())
    viewlog_raise_window();

  if(kbedit_is_visible())
    kbedit_raise_window();

  if(event_sender_is_visible())
    event_sender_raise_window();

  if(stream_infos_is_visible())
    stream_infos_raise_window();
}

void gui_toggle_aspect(void) {
  
  xine_set_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO, 
		 (xine_get_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO)) + 1);

  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
    
  }
}

void gui_toggle_interlaced(void) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_DEINTERLACE, 
		 1 - (xine_get_param(gGui->stream, XINE_PARAM_VO_DEINTERLACE)));

  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display,
                          gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);

  }
}

void gui_direct_change_audio_channel(xitk_widget_t *w, void *data, int value) {
  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, value);
  panel_update_channel_display ();
}

void gui_change_audio_channel(xitk_widget_t *w, void *data) {
  int dir = (int)data;
  int channel;
  
  channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  
  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;
  
  gui_direct_change_audio_channel(w, data, channel);
}

void gui_direct_change_spu_channel(xitk_widget_t *w, void *data, int value) {
  xine_set_param(gGui->stream, XINE_PARAM_SPU_CHANNEL, value);
  panel_update_channel_display ();
}

void gui_change_spu_channel(xitk_widget_t *w, void *data) {
  int dir = (int)data;
  int channel;
  
  channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);

  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;
  
  gui_direct_change_spu_channel(w, data, channel);
}

void gui_change_speed_playback(xitk_widget_t *w, void *data) {

  if(((int)data) == GUI_NEXT) {
    if (xine_get_param(gGui->stream, XINE_PARAM_SPEED) > XINE_SPEED_PAUSE)
      xine_set_param (gGui->stream, XINE_PARAM_SPEED, 
		      (xine_get_param(gGui->stream, XINE_PARAM_SPEED)) / 2);
  }
  else if(((int)data) == GUI_PREV) {
    if (xine_get_param (gGui->stream, XINE_PARAM_SPEED) < XINE_SPEED_FAST_4) {
      if (xine_get_param (gGui->stream, XINE_PARAM_SPEED) > XINE_SPEED_PAUSE)
	xine_set_param (gGui->stream, XINE_PARAM_SPEED, 
			(xine_get_param(gGui->stream, XINE_PARAM_SPEED)) * 2);
      else
	xine_set_param (gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
    }
  }
  else if(((int)data) == GUI_RESET) {
    xine_set_param (gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
  }
  
}

void *gui_set_current_position_thread(void *data) {
  int pos = (int)data;

  pthread_detach(pthread_self());
  
  (void) gui_xine_play(gGui->stream, pos, 0);
  
  gGui->ignore_next = 0;
  panel_check_pause();

  pthread_exit(NULL);
  return NULL;
}

void *gui_seek_relative_thread(void *data) {
  int off_sec = (int)data;
  int sec;
  
  pthread_detach(pthread_self());
  
  xine_get_pos_length(gGui->stream, NULL, &sec, NULL);
  sec /= 1000;

  if((sec + off_sec) < 0)
    sec = 0;
  else
    sec += off_sec;

  (void) gui_xine_play(gGui->stream, 0, sec);
  
  gGui->ignore_next = 0;
  panel_check_pause();

  pthread_exit(NULL);
  return NULL;
}

void gui_set_current_position (int pos) {
  int err;
  
  if(gGui->logo_mode && (mediamark_get_current_mrl())) {
    if(!xine_open(gGui->stream, (mediamark_get_current_mrl()))) {
      gui_handle_xine_error(gGui->stream);
      return;
    }
  }

  if(((xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gGui->ignore_next == 1))
    return;
    
  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY)
    xine_open(gGui->stream, gGui->mmk.mrl);
  
  gGui->ignore_next = 1;
  
  if ((err = pthread_create(&seek_thread,
			    NULL, gui_set_current_position_thread, (void *)pos)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }
}

void gui_seek_relative (int off_sec) {
  int err;
  
  if(((xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gGui->ignore_next == 1))
    return;
  
  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY)
    return;
  
  gGui->ignore_next = 1;
  
  if ((err = pthread_create(&seek_thread,
			    NULL, gui_seek_relative_thread, (void *)off_sec)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }
}

void gui_dndcallback (char *filename) {

  if(filename) {
    char  buffer[strlen(filename) + 10];
    char  buffer2[strlen(filename) + 10];
    char *p;

    memset(&buffer, 0, sizeof(buffer));
    memset(&buffer2, 0, sizeof(buffer2));

    if((strlen(filename) > 6) && 
       (!strncmp(filename, "file:", 5))) {
      
      if((p = strstr(filename, ":/")) != NULL) {
	struct stat pstat;
	
	p += 2;

	if(*(p + 1) == '/')
	  p++;

      __second_stat:

	if((stat(p, &pstat)) == 0)
	  sprintf(buffer, "file:/%s", p);
	else {
	  
	  sprintf(buffer2, "/%s", p);

	  /* file don't exist, add it anyway */
	  if((stat(buffer2, &pstat)) == -1)
	    sprintf(buffer, "%s", filename);
	  else
	    sprintf(buffer, "file:/%s", buffer2);
	  
	}
      }
      else {
	p = filename + 5;
	goto __second_stat;
      }
    }
    else
      sprintf(buffer, "%s", filename);
    
    mediamark_add_entry(buffer, buffer, 0, -1);
    gGui->playlist.cur = (gGui->playlist.num - 1);
    
    if((xine_get_status(gGui->stream) == XINE_STATUS_STOP) || gGui->logo_mode)
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    
    playlist_update_playlist();

    if((!is_playback_widgets_enabled()) && gGui->playlist.num)
      enable_playback_controls(1);
  }
}

void gui_direct_nextprev(xitk_widget_t *w, void *data, int value) {
  int i;
  int by_chapter;

  by_chapter = (gGui->skip_by_chapter &&
		(xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_HAS_CHAPTERS))) ? 1 : 0;
  
  if(((int)data) == GUI_NEXT) {

    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_NEXT);

    }
    else {

      switch(gGui->playlist.loop) {
      
      case PLAYLIST_LOOP_SHUFFLE:
	gGui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gGui->playlist.cur + value) < gGui->playlist.num) {

	  if(gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP)
	    gGui->playlist.cur += (value - 1);
	  else
	    gGui->playlist.cur += value;
	  
	  gGui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	if((gGui->playlist.cur + value) > gGui->playlist.num) {
	  int newcur = value - (gGui->playlist.num - gGui->playlist.cur);
	  
	  gGui->ignore_next = 1;
	  gGui->playlist.cur = newcur;
	  gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
	  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	    gui_display_logo();

	  gGui->ignore_next = 0;
	}
	else {
	  gGui->playlist.cur += (value - 1);
	  gGui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;
      }
    }
  }
  else if(((int)data) == GUI_PREV) {
    
    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_PRIOR);

    }
    else {

      switch(gGui->playlist.loop) {
      case PLAYLIST_LOOP_SHUFFLE:
	gGui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gGui->playlist.cur - value) >= 0) {
	  
	  gGui->ignore_next = 1;
	  gGui->playlist.cur -= value;
	  
	  if((gGui->playlist.cur < gGui->playlist.num)) {
	    gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
	    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	      gui_display_logo();

	  }
	  else
	    gGui->playlist.cur = 0;
	  
	  gGui->ignore_next = 0;
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	gGui->ignore_next = 1;

	if((gGui->playlist.cur - value) >= 0) {

	  gGui->playlist.cur -= value;
	  
	  if((gGui->playlist.cur < gGui->playlist.num)) {
	    gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
	    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	      gui_display_logo();
	  }
	  else
	    gGui->playlist.cur = 0;
	  
	}
	else {
	  int newcur = (gGui->playlist.cur - value) + gGui->playlist.num;
	  
	  gGui->playlist.cur = newcur;
	  gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
	  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	    gui_display_logo();
	}

	gGui->ignore_next = 0;
	break;
      }
    }
  }

  panel_check_pause();
}
  
void gui_nextprev(xitk_widget_t *w, void *data) {
  gui_direct_nextprev(w, data, 1);
}

void gui_playlist_show(xitk_widget_t *w, void *data) {

  if(!playlist_is_running()) {
    playlist_editor();
  }
  else {
    if(playlist_is_visible())
      playlist_exit(NULL, NULL);
    else
      playlist_toggle_visibility(NULL, NULL);
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

void gui_set_current_mrl(mediamark_t *mmk) {

  if(gGui->mmk.mrl)
    free(gGui->mmk.mrl);
  if(gGui->mmk.ident)
    free(gGui->mmk.ident);
  
  if(mmk) {
    gGui->mmk.mrl = strdup(mmk->mrl);
    gGui->mmk.ident = strdup(((mmk->ident) ? mmk->ident : mmk->mrl));
    gGui->mmk.start = mmk->start;
    gGui->mmk.end = mmk->end;
  }
  else {
    char buffer[1024];
    
    memset(&buffer, 0, sizeof(buffer));
    sprintf(buffer, "xine-ui version %s", VERSION);
    
    gGui->mmk.mrl = strdup(_("There is no mrl."));
    gGui->mmk.ident = strdup(buffer);
    gGui->mmk.start = 0;
    gGui->mmk.end = -1;
  }

  panel_update_mrl_display ();
  playlist_update_focused_entry();
}

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

void gui_event_sender_show(xitk_widget_t *w, void *data) {
  
  if (event_sender_is_running() && !event_sender_is_visible())
    event_sender_toggle_visibility(NULL, NULL);
  else if(!event_sender_is_running())
    event_sender_panel();
  else
    event_sender_exit(NULL, NULL);
}

void gui_stream_infos_show(xitk_widget_t *w, void *data) {
  
  if (stream_infos_is_running() && !stream_infos_is_visible())
    stream_infos_toggle_visibility(NULL, NULL);
  else if(!stream_infos_is_running())
    stream_infos_panel();
  else
    stream_infos_end();
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
 * Return 1 if layer should be set
 */
int is_layer_above(void) {
  
  if(gGui->always_layer_above || gGui->layer_above) {
    if(!gGui->layer_above)
      config_update_num("gui.layer_above", 1);
    return 1;
  }
  
  return 0;
}
/*
 * set window layer property to something above GNOME (and KDE?) panel
 * (reset to normal if do_raise == 0)
 */
void layer_above_video(Window w) {

  static Atom XA_WIN_LAYER = None;
  XEvent xev;

  if(!(is_layer_above()))
    return;
  
  if(XA_WIN_LAYER == None) {
    XLockDisplay(gGui->display);
    XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);
    XUnlockDisplay(gGui->display);
  }

  xev.type                 = ClientMessage;
  xev.xclient.type         = ClientMessage;
  xev.xclient.window       = w;
  xev.xclient.message_type = XA_WIN_LAYER;
  xev.xclient.format       = 32;

  /* top layer if video is fullscreen, otherwise normal layer */
  if (video_window_get_fullscreen_mode() && video_window_is_visible()) {
    xev.xclient.data.l[0] = (long) 10;
  }
  else {
    if(is_layer_above())
      xev.xclient.data.l[0] = (long) 10;
    else
      xev.xclient.data.l[0] = (long) 4;
  }

  xev.xclient.data.l[1] = 0;

  XLockDisplay(gGui->display);
  XSendEvent(gGui->display, gGui->imlib_data->x.root, False,
	     SubstructureNotifyMask, (XEvent*) &xev);
  XUnlockDisplay(gGui->display);

}

void gui_increase_audio_volume(void) {

  if(gGui->mixer.caps & MIXER_CAP_VOL) { 
    if(gGui->mixer.volume_level < 100) {
      gGui->mixer.volume_level++;
      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
    }
  }
}

void gui_decrease_audio_volume(void) {

  if(gGui->mixer.caps & MIXER_CAP_VOL) { 
    if(gGui->mixer.volume_level > 0) {
      gGui->mixer.volume_level--;
      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
    }
  }
}

void gui_change_zoom(int zoom_dx, int zoom_dy) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_X,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_ZOOM_X) + zoom_dx);
  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y) + zoom_dy);
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
}

/*
 * Reset zooming by recall aspect ratio.
 */
void gui_reset_zoom(void) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_X, 100);
  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y, 100);
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow (gGui->display, gGui->panel_window);
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
}

/* 
 * Toggle TV Modes on the dxr3
 */
void gui_toggle_tvmode(void) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_TVMODE,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_TVMODE) + 1);
  				 
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
    fprintf(stderr, _("XSendEvent(display, 0x%x ...) failed.\n"), (unsigned int) window);
  }
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);
  
}

/*
 *
 */
void visual_anim_init(void) {
  char buffer[4096];
  
  memset(&buffer, 0, sizeof(buffer));
  sprintf(buffer, "%s/%s", XINE_VISDIR, "default.avi");
  
  gGui->visual_anim.mrls = (char **) xine_xmalloc(sizeof(char *) * 3);
  gGui->visual_anim.num_mrls = 0;
  
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++]   = strdup(buffer);
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]     = NULL;
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls + 1] = NULL;
}
void visual_anim_add_animation(char *mrl) {
  gGui->visual_anim.mrls = (char **) realloc(gGui->visual_anim.mrls, sizeof(char *) * 
					     ((gGui->visual_anim.num_mrls + 1) + 2));
  
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++]   = strdup(mrl);
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]     = NULL;
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls + 1] = NULL;
}
static int visual_anim_open_and_play(xine_stream_t *stream, const char *mrl) {
  if((!xine_open(stream, mrl)) || (!xine_play(stream, 0, 0)))
    return 0;

  return 1;
}
void visual_anim_play(void) {
  if(gGui->visual_anim.enabled) {
    if(!visual_anim_open_and_play(gGui->visual_anim.stream, 
				  gGui->visual_anim.mrls[gGui->visual_anim.current]))
      gui_handle_xine_error(gGui->visual_anim.stream);
    gGui->visual_anim.running = 1;
  }
}
void visual_anim_play_next(void) {
  if(gGui->visual_anim.enabled) {
    gGui->visual_anim.current++;
    if(gGui->visual_anim.mrls[gGui->visual_anim.current] == NULL)
      gGui->visual_anim.current = 0;
    visual_anim_play();
  }
}
void visual_anim_pause(void) {
  if(gGui->visual_anim.enabled) {
    if(xine_get_param(gGui->visual_anim.stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
      xine_set_param(gGui->visual_anim.stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    else
      xine_set_param(gGui->visual_anim.stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
  }
}
void visual_anim_stop(void) {
  if(gGui->visual_anim.enabled) {
    xine_stop(gGui->visual_anim.stream);
    gGui->visual_anim.running = 0;
  }
}
