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
 * gui initialization and top-level event handling stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xmd.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <zlib.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>

#include "common.h"

#ifdef HAVE_LIRC
extern int no_lirc;
#endif

extern int errno;

/*
 * global variables
 */
extern gGui_t          *gGui;

static pid_t            xine_pid;

/* Icon data */
static unsigned char xine_bits[] = {
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xff, 0xff, 0xff,
   0x8f, 0xf1, 0xff, 0xff, 0xff, 0x8f, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f,
   0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00,
   0x00, 0x88, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00,
   0x88, 0x11, 0x91, 0x79, 0xf8, 0x88, 0x1f, 0x9b, 0x99, 0x0c, 0xf8, 0x1f,
   0x8e, 0x99, 0x7d, 0xf8, 0x11, 0x8e, 0x99, 0x7d, 0x88, 0x11, 0x9b, 0x99,
   0x0d, 0x88, 0x1f, 0x91, 0x99, 0xf9, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00,
   0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f,
   0x00, 0x00, 0x00, 0xf8, 0xf1, 0xff, 0xff, 0xff, 0x8f, 0xf1, 0xff, 0xff,
   0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8
};

static void auto_vo_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->auto_vo_visibility = cfg->num_value;
}
static void auto_panel_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->auto_panel_visibility = cfg->num_value;
}
static void skip_by_chapter_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->skip_by_chapter = cfg->num_value;
  panel_update_nextprev_tips();
}
static void ssaver_timeout_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->ssaver_timeout = cfg->num_value;
}

static void visual_anim_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->visual_anim.enabled = cfg->num_value;

  if((!gGui->visual_anim.enabled) && gGui->visual_anim.running)
    visual_anim_stop();
}

static void stream_info_auto_update_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->stream_info_auto_update = cfg->num_value;
  stream_infos_toggle_auto_update();
}

/*
 * Layer above callbacks
 */
static void layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->layer_above = cfg->num_value;
}
static void always_layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->always_layer_above = cfg->num_value;
}

/*
 * Callback for snapshots saving location.
 */
static void snapshot_loc_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->snapshot_location = cfg->str_value;
}

int actions_on_start(action_id_t actions[], action_id_t a) {
  int i = 0, num = 0;
  while(actions[i] != ACTID_NOKEY) {
    if(actions[i] == a)
      num++;
    i++;
  }
  return num;
}

/*
 *
 */
static void dummy_sighandler(int dummy) {
}
static void gui_signal_handler (int sig, void *data) {
  pid_t     cur_pid = getppid();
  
  switch (sig) {

  case SIGHUP:
    if(cur_pid == xine_pid) {
      printf("SIGHUP received: re-read config file\n");
      config_reset();
    }
    break;

  case SIGUSR1:
    if(cur_pid == xine_pid) {
      printf("SIGUSR1 received\n");
    }
    break;

  case SIGUSR2:
    if(cur_pid == xine_pid) {
      printf("SIGUSR2 received\n");
    }
    break;

  case SIGINT:
  case SIGTERM:
    if(cur_pid == xine_pid) {
      struct sigaction action;
      
      config_save();
      
      action.sa_handler = dummy_sighandler;
      sigemptyset(&(action.sa_mask));
      action.sa_flags = 0;
      if(sigaction(SIGALRM, &action, NULL) != 0) {
	fprintf(stderr, "sigaction(SIGALRM) failed: %s\n", strerror(errno));
      }
      alarm(5);
      
      xine_stop(gGui->stream);
    }
    break;
  }

}

/*
 *
 */
void gui_execute_action_id(action_id_t action) {
  xine_event_t   xine_event;
  
  if(action & ACTID_IS_INPUT_EVENT) {
    
    if((action >= ACTID_EVENT_NUMBER_0) && (action <= ACTID_EVENT_NUMBER_9)) {

      if(!gGui->numeric.set) {
	gGui->numeric.set = 1;
	gGui->numeric.arg = 0;
      }
      
      if(((gGui->numeric.arg * 10) + (action - ACTID_EVENT_NUMBER_0)) <= INT_MAX) {
	gGui->numeric.arg *= 10;
	gGui->numeric.arg += (action - ACTID_EVENT_NUMBER_0);
      }
      else
	printf("WARNING: Input number canceled, avoid overflow\n");

    } 
    else if(action == ACTID_EVENT_NUMBER_10_ADD) {

      if(!gGui->numeric.set) {
	gGui->numeric.set = 1;
	gGui->numeric.arg = 0;
      }
      
      if((gGui->numeric.arg + 10) <= INT_MAX)
	gGui->numeric.arg += 10;
      else
	printf("WARNING: Input number canceled, avoid overflow\n");

    }
    else {
      gGui->numeric.set = 0;
      gGui->numeric.arg = 0;
    }

    /* events for advanced input plugins. */
    xine_event.type = action & ~ACTID_IS_INPUT_EVENT;
    xine_event.data_length = 0;
    xine_event.data = NULL;
    xine_event_send(gGui->stream, &xine_event);
    return;
  }

  switch(action) {
    
  case ACTID_WINDOWREDUCE:
    video_window_set_mag (0.8 * video_window_get_mag());
    break;

  case ACTID_WINDOWENLARGE:
    video_window_set_mag (1.2 * video_window_get_mag());
    break;

  case ACTID_ZOOM_1_1:
  case ACTID_WINDOW100:
    video_window_set_mag (1.0);
    break;

  case ACTID_WINDOW200:
    video_window_set_mag (2.0);
    break;

  case ACTID_WINDOW50:
    video_window_set_mag (0.5);
    break;

  case ACTID_SPU_NEXT:
    if(!gGui->numeric.set)
      gui_change_spu_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_spu_channel(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_SPU_PRIOR:
    if(!gGui->numeric.set)
      gui_change_spu_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_spu_channel(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
    
  case ACTID_CONTROLSHOW:
    gui_control_show(NULL, NULL);
    break;
    
  case ACTID_TOGGLE_WINOUT_VISIBLITY:
    gui_toggle_visibility (NULL, NULL);
    break;
    
  case ACTID_AUDIOCHAN_NEXT:
    if(!gGui->numeric.set)
      gui_change_audio_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_audio_channel(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_AUDIOCHAN_PRIOR:
    if(!gGui->numeric.set)
      gui_change_audio_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_audio_channel(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
    
  case ACTID_PAUSE:
    gui_pause (NULL, (void*)(1), 0); 
    break;

  case ACTID_PLAYLIST:
    gui_playlist_show(NULL, NULL);
    break;
      
  case ACTID_TOGGLE_VISIBLITY:
    panel_toggle_visibility (NULL, NULL);
    break;

  case ACTID_TOGGLE_FULLSCREEN:
    gui_set_fullscreen_mode(NULL, NULL);
    break;

  case ACTID_TOGGLE_ASPECT_RATIO:
    gui_toggle_aspect();
    break;

  case ACTID_STREAM_INFOS:
    gui_stream_infos_show(NULL, NULL);
    break;

  case ACTID_TOGGLE_INTERLEAVE:
    gui_toggle_interlaced();
    break;

  case ACTID_QUIT:
    gui_exit(NULL, NULL);
    break;

  case ACTID_PLAY:
    gui_play(NULL, NULL);
    break;

  case ACTID_STOP:
    gui_stop(NULL, NULL);
    break;

  case ACTID_EVENT_SENDER:
    gui_event_sender_show(NULL, NULL);
    break;
    
  case ACTID_MRL_NEXT:
    if(!gGui->numeric.set)
      gui_nextprev(NULL, (void*)GUI_NEXT);
    else
      gui_direct_nextprev(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_MRL_PRIOR:
    if (!gGui->numeric.set)
      gui_nextprev(NULL, (void*)GUI_PREV);
    else
      gui_direct_nextprev(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
      
  case ACTID_SETUP:
    gui_setup_show(NULL, NULL);
    break;

  case ACTID_EJECT:
    gui_eject(NULL, NULL);
    break;

  case ACTID_SET_CURPOS:
    /* Number is a percentage */
    gGui->numeric.arg %= 100; /* range [0..100] */
    gui_set_current_position((65534 * gGui->numeric.arg) / 100);
    break;

  case ACTID_SET_CURPOS_10:
    gui_set_current_position (6553);
    break;

  case ACTID_SET_CURPOS_20:
    gui_set_current_position (13107);
    break;

  case ACTID_SET_CURPOS_30:
    gui_set_current_position (19660);
    break;

  case ACTID_SET_CURPOS_40:
    gui_set_current_position (26214);
    break;

  case ACTID_SET_CURPOS_50:
    gui_set_current_position (32767);
    break;
    
  case ACTID_SET_CURPOS_60:
    gui_set_current_position (39321);
    break;

  case ACTID_SET_CURPOS_70:
    gui_set_current_position (45874);
    break;
    
  case ACTID_SET_CURPOS_80:
    gui_set_current_position (52428);
    break;
    
  case ACTID_SET_CURPOS_90:
    gui_set_current_position (58981);
    break;

  case ACTID_SET_CURPOS_0:
    gui_set_current_position (0);
    break;

  case ACTID_SEEK_REL_m:
    if(gGui->numeric.set) {
      gGui->numeric.arg = -gGui->numeric.arg;
      gui_seek_relative (gGui->numeric.arg);
    }
  break;

  case ACTID_SEEK_REL_p:
    if(gGui->numeric.set)
      gui_seek_relative (gGui->numeric.arg);
    break;

  case ACTID_SEEK_REL_m60:
    gui_seek_relative (-60);
    break;

  case ACTID_SEEK_REL_m15:
    gui_seek_relative (-15);
    break;

  case ACTID_SEEK_REL_p60:
    gui_seek_relative (60);
    break;
    
  case ACTID_SEEK_REL_p15:
    gui_seek_relative (15);
    break;
    
  case ACTID_SEEK_REL_m30:
    gui_seek_relative (-30);
    break;
    
  case ACTID_SEEK_REL_m7:
    gui_seek_relative (-7);
    break;
    
  case ACTID_SEEK_REL_p30:
    gui_seek_relative (30);
    break;
    
  case ACTID_SEEK_REL_p7:
    gui_seek_relative (7);
    break;

  case ACTID_MRLBROWSER:
    gui_mrlbrowser_show(NULL, NULL);
    break;
    
  case ACTID_MUTE:
    panel_toggle_audio_mute(NULL, NULL, !gGui->mixer.mute);
    break;
    
  case ACTID_AV_SYNC_m3600:
    xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, 
		   (xine_get_param(gGui->stream, XINE_PARAM_AV_OFFSET)) - 3600);
    break;
    
  case ACTID_AV_SYNC_p3600:
    xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET,
		   (xine_get_param(gGui->stream, XINE_PARAM_AV_OFFSET)) + 3600);
    break;

  case ACTID_AV_SYNC_RESET:
    xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, 0);
    break;

  case ACTID_SPEED_FAST:
    gui_change_speed_playback(NULL, (void*)GUI_PREV);
    break;
    
  case ACTID_SPEED_SLOW:
    gui_change_speed_playback(NULL, (void*)GUI_NEXT);
    break;

  case ACTID_SPEED_RESET:
    gui_change_speed_playback(NULL, (void*)GUI_RESET);
    break;

  case ACTID_pVOLUME:
    gui_increase_audio_volume();
    break;

  case ACTID_mVOLUME:
    gui_decrease_audio_volume();
    break;

  case ACTID_SNAPSHOT:
    panel_snapshot(NULL, NULL);
    break;
    
  case ACTID_ZOOM_IN:
    gui_change_zoom(1,1);
    break;

  case ACTID_ZOOM_OUT:
    gui_change_zoom(-1,-1);
    break;

  case ACTID_ZOOM_X_IN:
    gui_change_zoom(1,0);
    break;
  
  case ACTID_ZOOM_X_OUT:
    gui_change_zoom(-1,0);
    break;

  case ACTID_ZOOM_Y_IN:
    gui_change_zoom(0,1);
    break;
  
  case ACTID_ZOOM_Y_OUT:
    gui_change_zoom(0,-1);
    break;
  
  case ACTID_ZOOM_RESET:
    gui_reset_zoom();
    break;
    
  case ACTID_GRAB_POINTER:
    if(!gGui->cursor_grabbed) {
      if(!panel_is_visible()) {
	XLockDisplay(gGui->display);
	XGrabPointer(gGui->display, gGui->video_window, 1, None, 
		     GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
	XUnlockDisplay(gGui->display);
      }
      
      gGui->cursor_grabbed = 1;
    }
    else {
      XLockDisplay(gGui->display);
      XUngrabPointer(gGui->display, CurrentTime);
      XUnlockDisplay(gGui->display);
      gGui->cursor_grabbed = 0;
    }
    break;
    
  case ACTID_TOGGLE_TVMODE:
    gui_toggle_tvmode();
    break;

  case ACTID_VIEWLOG:
    gui_viewlog_show(NULL, NULL);
    break;

  case ACTID_KBEDIT:
    gui_kbedit_show(NULL, NULL);
    break;

  case ACTID_DPMSSTANDBY:
    xine_system(0, "xset dpms force standby");
    break;

  case ACTID_MRLIDENTTOGGLE:
    gGui->is_display_mrl = !gGui->is_display_mrl;
    panel_update_mrl_display();
    playlist_mrlident_toggle();
    break;
    
  case ACTID_SCANPLAYLIST:
    playlist_scan_for_infos();
    break;

  case ACTID_MMKEDITOR:
    playlist_mmk_editor();
    break;

  case ACTID_LOOPMODE:
    gGui->playlist.loop++;
    if(gGui->playlist.loop > PLAYLIST_LOOP_SHUFFLE)
      gGui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
#if 1
    printf("Playlist loop mode:");
    switch(gGui->playlist.loop) {
    case PLAYLIST_LOOP_NO_LOOP:
      printf("PLAYLIST_LOOP_NO_LOOP");
      break;
    case PLAYLIST_LOOP_LOOP:
      printf("PLAYLIST_LOOP_LOOP");
      break;
    case PLAYLIST_LOOP_REPEAT:
      printf("PLAYLIST_LOOP_REPEAT");
      break;
    case PLAYLIST_LOOP_SHUFFLE:
      printf("PLAYLIST_LOOP_SHUFFLE");
      break;
    }
    printf("\n");
#endif
    break;
    

  default:
    break;
  }

  /* Some sort of function was done given. Clear numeric argument. */
  gGui->numeric.set = 0;
  gGui->numeric.arg = 0;

}	    

/*
 * top-level event handler
 */
void gui_handle_event (XEvent *event, void *data) {

  switch(event->type) {

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case DestroyNotify:
    if(event->xany.window == gGui->panel_window
       || event->xany.window == gGui->video_window) {
      xine_exit (gGui->xine);
      gGui->running = 0;
    }
    break;
    
  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    /* printf ("ButtonPress\n"); */

    /* printf ("button: %d\n",bevent->button); */

    if ((bevent->button == 3) && (bevent->window == gGui->video_window))
      panel_toggle_visibility (NULL, NULL);
    
  }
  break;

  case ButtonRelease:
  case KeyPress:
    kbindings_handle_kbinding(gGui->kbindings, event);
    break;
    
  case ConfigureNotify:
    break;

  case ClientMessage:
    xitk_process_client_dnd_message (&gGui->xdnd, event);
    break;

    /*
      default:
      printf("Got event: %i\n", event->type);
    */
  }

}

/*
 * Start playback to next entry in playlist (or stop the engine, then display logo).
 */
void gui_playlist_start_next(void) {

  if (gGui->ignore_next)
    return;

  panel_reset_slider ();
  
  switch(gGui->playlist.loop) {
    
  case PLAYLIST_LOOP_NO_LOOP:
  case PLAYLIST_LOOP_LOOP:
    gGui->playlist.cur++;
    
    if(gGui->playlist.cur < gGui->playlist.num) {
      gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	gui_display_logo();
    }
    else {
      
      if(gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) {
	if(gGui->actions_on_start[0] == ACTID_QUIT)
	  gui_exit(NULL, NULL);

	gGui->playlist.cur--;
	mediamark_reset_played_state();
	gui_display_logo();
      }
      else if(gGui->playlist.loop == PLAYLIST_LOOP_LOOP) {
	gGui->playlist.cur = 0;
	gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
	gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	  gui_display_logo();
      }

    }
    break;
    
  case PLAYLIST_LOOP_REPEAT:
    gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
      gui_display_logo();
    break;

  case PLAYLIST_LOOP_SHUFFLE:
    if(!mediamark_all_played()) {
      if(gGui->playlist.num >= 3) {
	int    next;
	float  num = (float) gGui->playlist.num;
	
	srandom((unsigned int)time(NULL));
	do {
	  next = (int) (num * random() / RAND_MAX);
	} while((next == gGui->playlist.cur) && (gGui->playlist.mmk[next]->played == 1));
	
	gGui->playlist.cur = next;
      }
      else
	gGui->playlist.cur = !gGui->playlist.cur;
      
      gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      if(!gui_xine_open_and_play(gGui->mmk.mrl, 0, gGui->mmk.start))
	gui_display_logo();
    }
    else {
      mediamark_reset_played_state();
      gui_display_logo();
    }
    
    break;
        
  }
  
  if(is_playback_widgets_enabled() && (!gGui->playlist.num) && (!gGui->mmk.mrl)) {
    gui_set_current_mrl(NULL);
    enable_playback_controls(0);
  }
  
}

static void gui_find_visual (Visual **visual_return, int *depth_return) {
  XWindowAttributes  attribs;
  XVisualInfo	    *vinfo;
  XVisualInfo	     vinfo_tmpl;
  int		     num_visuals;
  int		     depth = 0;
  Visual	    *visual = NULL;

  if (gGui->prefered_visual_id == None) {
    /*
     * List all available TrueColor visuals, pick the best one for xine.
     * We prefer visuals of depth 15/16 (fast).  Depth 24/32 may be OK, 
     * but could be slow.
     */
    vinfo_tmpl.screen = gGui->screen;
    vinfo_tmpl.class  = (gGui->prefered_visual_class != -1
			 ? gGui->prefered_visual_class : TrueColor);
    vinfo = XGetVisualInfo(gGui->display,
			   VisualScreenMask | VisualClassMask,
			   &vinfo_tmpl, &num_visuals);
    if (vinfo != NULL) {
      int i, pref;
      int best_visual_index = -1;
      int best_visual = -1;

      for (i = 0; i < num_visuals; i++) {
	if (vinfo[i].depth == 15 || vinfo[i].depth == 16)
	  pref = 3;
	else if (vinfo[i].depth > 16)
	  pref = 2;
	else
	  pref = 1;
	
	if (pref > best_visual) {
	  best_visual = pref;
	  best_visual_index = i;
	}  
      }
      
      if (best_visual_index != -1) {
	depth = vinfo[best_visual_index].depth;
	visual = vinfo[best_visual_index].visual;
      }
      
      XFree(vinfo);
    }
  } else {
    /*
     * Use the visual specified by the user.
     */
    vinfo_tmpl.visualid = gGui->prefered_visual_id;
    vinfo = XGetVisualInfo(gGui->display,
			   VisualIDMask, &vinfo_tmpl, 
			   &num_visuals);
    if (vinfo == NULL) {
      printf(_("gui_main: selected visual %#lx does not exist, trying default visual\n"),
	     (long) gGui->prefered_visual_id);
    } else {
      depth = vinfo[0].depth;
      visual = vinfo[0].visual;
      XFree(vinfo);
    }
  }

  if (depth == 0) {
    XVisualInfo vinfo;

    XGetWindowAttributes(gGui->display, (DefaultRootWindow(gGui->display)), &attribs);

    depth = attribs.depth;
  
    if (XMatchVisualInfo(gGui->display, gGui->screen, depth, TrueColor, &vinfo)) {
      visual = vinfo.visual;
    } else {
      printf (_("gui_main: couldn't find true color visual.\n"));

      depth = DefaultDepth (gGui->display, gGui->screen);
      visual = DefaultVisual (gGui->display, gGui->screen); 
    }
  }

  if (depth_return != NULL)
    *depth_return = depth;
  if (visual_return != NULL)
    *visual_return = visual;
}

/*
 * Initialize the GUI
 */
void gui_init (int nfiles, char *filenames[], window_attributes_t *window_attribute) {
  int    i;
  char  *display_name = ":0.0";

  /*
   * init playlist
   */
  for (i = 0; i < nfiles; i++)
    mediamark_add_entry((const char *)filenames[i], (const char *)filenames[i], 0, -1);
  
  gGui->playlist.cur = 0;
  gGui->is_display_mrl = 0;
  gGui->mrl_overrided = 0;

  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());

  /*
   * X / imlib stuff
   */

  if (!XInitThreads ()) {
    printf (_("\nXInitThreads failed - looks like you don't have a "
	    "thread-safe xlib.\n"));
    exit (1);
  } 
  
  if(getenv("DISPLAY"))
    display_name = getenv("DISPLAY");
  
  if((gGui->display = XOpenDisplay(display_name)) == NULL) {
    fprintf(stderr, _("Cannot open display\n"));
    exit(1);
  }

  if (xine_config_register_bool (gGui->xine, "gui.xsynchronize", 
				 0,
				 _("synchronized X protocol (debug)"), 
				 CONFIG_NO_HELP,
				 CONFIG_LEVEL_EXP,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA)) {

    XSynchronize (gGui->display, True);
    fprintf (stderr, _("Warning! Synchronized X activated - this is way slow...\n"));
  }

  gGui->layer_above = 
    xine_config_register_bool (gGui->xine, "gui.layer_above", 1,
			       _("Windows stacking"),
			       _("Use wm layer property to place window on top."), 
			       CONFIG_LEVEL_EXP,
			       layer_above_cb,
			       CONFIG_NO_DATA);

  gGui->always_layer_above = 
    xine_config_register_bool (gGui->xine, "gui.always_layer_above", 0,
			       _("Windows stacking (more)"),
			       _("Use WM layer property to place windows on top."), 
			       CONFIG_LEVEL_EXP,
			       always_layer_above_cb,
			       CONFIG_NO_DATA);

  if(gGui->always_layer_above && (!gGui->layer_above))
    config_update_num("gui.layer_above", 1);

  gGui->snapshot_location = 
    (char *)xine_config_register_string (gGui->xine, "gui.snapshotdir", 
					 (char *) (xine_get_homedir()),
					 _("Snapshot location"),
					 _("Where snapshots will be saved."),
					 CONFIG_LEVEL_EXP,
					 snapshot_loc_cb,
					 CONFIG_NO_DATA);
  
  gGui->ssaver_timeout =
    xine_config_register_num (gGui->xine, "gui.screensaver_timeout", 10,
			      _("Screensaver wakeup"),
			      _("Time between two screensaver fake events, 0 to disable."),
			      CONFIG_LEVEL_EXP,
			      ssaver_timeout_cb,
			      CONFIG_NO_DATA);
  
  gGui->skip_by_chapter = 
    xine_config_register_bool (gGui->xine, "gui.skip_by_chapter", 1,
			       _("Chapter hoping"),
			       _("Play next|previous chapter instead of mrl (dvdnav)"), 
			       CONFIG_LEVEL_EXP,
			       skip_by_chapter_cb, 
			       CONFIG_NO_DATA);
  
  gGui->auto_vo_visibility = 
    xine_config_register_bool (gGui->xine, "gui.auto_video_output_visibility", 0,
			       _("Visiblity behavoir of output window"),
			       _("Show/hide video output window regarding to the stream type"), 
			       CONFIG_LEVEL_EXP,
			       auto_vo_visibility_cb, 
			       CONFIG_NO_DATA);

  gGui->auto_panel_visibility = 
    xine_config_register_bool (gGui->xine, "gui.auto_panel_visibility", 0,
			       _("Visiblility behavior of panel"),
			       _("Automatically show/hide panel window, according to auto_video_output_visibility"), 
			       CONFIG_LEVEL_EXP,
			       auto_panel_visibility_cb,
			       CONFIG_NO_DATA); 
 
  gGui->eventer_sticky = 
    xine_config_register_bool(gGui->xine, "gui.eventer_sticky", 
			      1,
			      _("Event sender behavior"),
			      _("Event sender window stick to main panel"), 
			      CONFIG_LEVEL_EXP,
			      event_sender_sticky_cb,
			      CONFIG_NO_DATA);

  gGui->visual_anim.enabled = 
    xine_config_register_bool(gGui->xine, "gui.visual_anim", 
			      1,
			      _("Visual animation"),
			      _("Display some video animations when "
				"current stream is audio only (eg: mp3)."), 
			      CONFIG_LEVEL_EXP,
			      visual_anim_cb,
			      CONFIG_NO_DATA);

  gGui->stream_info_auto_update = 
    xine_config_register_bool(gGui->xine, "gui.sinfo_auto_update", 
			      0,
			      _("Stream informations"),
			      _("Update stream informations (in stream infos window) "
				"each half seconds."), 
			      CONFIG_LEVEL_EXP,
			      stream_info_auto_update_cb,
			      CONFIG_NO_DATA);
  
  gGui->numeric.set = 0;
  gGui->numeric.arg = 0;

  XLockDisplay (gGui->display);

  gGui->screen = DefaultScreen(gGui->display);

  /* Some infos */
  printf(_("XServer Vendor: %s. Release: %d,\n"), 
	 XServerVendor(gGui->display), XVendorRelease(gGui->display));
  printf(_("        Protocol Version: %d, Revision: %d,\n"), 
	 XProtocolVersion(gGui->display), XProtocolRevision(gGui->display));
  printf(_("        Available Screen(s): %d, using %d\n")
	 , XScreenCount(gGui->display), gGui->screen);
  printf(_("        Depth: %d.\n"),
	 XDisplayPlanes(gGui->display, gGui->screen));
#ifdef HAVE_SHM
  if(XShmQueryExtension(gGui->display)) {
    int major, minor, ignore;
    Bool pixmaps;
    
    if(XQueryExtension(gGui->display, "MIT-SHM", &ignore, &ignore, &ignore)) {
      if(XShmQueryVersion(gGui->display, &major, &minor, &pixmaps ) == True) {
	printf(_("        XShmQueryVersion: %d.%d.\n"), major, minor);
      }
    }
  }
#endif
  
  gui_find_visual(&gGui->visual, &gGui->depth);

  gui_init_imlib (gGui->visual);

  XUnlockDisplay (gGui->display);

  /*
   * create and map panel and video window
   */
  xine_pid = getppid();
  
  xitk_init(gGui->display);
  
  init_skins_support();

  gGui->kbindings = kbindings_init_kbinding();

  gGui->running = 1;

  video_window_init (window_attribute, 
		     ((actions_on_start(gGui->actions_on_start, 
				       ACTID_TOGGLE_WINOUT_VISIBLITY)) ? 1 : 0));

  panel_init ();

}

void gui_init_imlib (Visual *vis) {
  XColor                dummy;
  ImlibInitParams	imlib_init;

  /*
   * This routine isn't re-entrant. I cannot find a Imlib_cleanup either.
   * However, we have to reinitialize Imlib if we have to change the visual.
   * This will be a (small) memory leak.
   */
  imlib_init.flags = PARAMS_VISUALID;
  imlib_init.visualid = vis->visualid;
  if (gGui->install_colormap && (vis->class & 1)) {
      /*
       * We're using a visual with changable colors / colormaps
       * (According to the comment in X11/X.h, an odd display class
       * has changable colors), and the user requested to install a
       * private colormap for xine.  Allocate a fresh colormap for
       * Imlib and Xine.
       */
      Colormap cm;
      cm = XCreateColormap(gGui->display, 
			   RootWindow(gGui->display, gGui->screen),
			   vis, AllocNone);

      imlib_init.cmap = cm;
      imlib_init.flags |= PARAMS_COLORMAP;
  }
  gGui->imlib_data = Imlib_init_with_params (gGui->display, &imlib_init);
  if (gGui->imlib_data == NULL) {
    fprintf(stderr, _("Unable to initialize Imlib\n"));
    exit(1);
  }

  gGui->colormap = Imlib_get_colormap (gGui->imlib_data);

  XAllocNamedColor(gGui->display, gGui->colormap,
		   "black", &gGui->black, &dummy);

  /*
   * create an icon pixmap
   */
  
  gGui->icon = XCreateBitmapFromData (gGui->display, 
				      gGui->imlib_data->x.root,
				      xine_bits, 40, 40);
}


/*
 *
 */
void gui_run (void) {
  int i;
  
  video_window_change_skins();
  panel_add_autoplay_buttons();
  panel_add_mixer_control();
  panel_update_channel_display () ;
  panel_update_mrl_display ();
  panel_update_runtime_display();

  gui_display_logo();

  /* autoscan playlist  */
  if(gGui->autoscan_plugin != NULL) {
    const char *const *autoscan_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    int         i;

    if(autoscan_plugins) {

      for(i=0; autoscan_plugins[i] != NULL; ++i) {

	if(!strcasecmp(autoscan_plugins[i], gGui->autoscan_plugin)) {
	  int                num_mrls, j;
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine,
							 gGui->autoscan_plugin,
							 &num_mrls);
	  
	  if(autoplay_mrls) {
	    for (j = 0; j < num_mrls; j++)
	      mediamark_add_entry((const char *)autoplay_mrls[j],
				  (const char *)autoplay_mrls[j], 0, -1);
	   
	    gGui->playlist.cur = 0;
	    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	  }
	}    
      }
    }
  }  

  enable_playback_controls((gGui->playlist.num > 0));
  
  if(gGui->actions_on_start[0] != ACTID_NOKEY) {

    /* Popup setup window if there is no config file */
    if(actions_on_start(gGui->actions_on_start, ACTID_SETUP))
      gui_execute_action_id(ACTID_SETUP);
    
    /*  The user wants to hide control panel  */
    if(panel_is_visible() && (actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_VISIBLITY)))
      gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);

    /*  The user wants to hide video window  */
    if(actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)) {
      if(!panel_is_visible())
	gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);

      xine_gui_send_vo_data(gGui->stream,
			    XINE_GUI_SEND_VIDEOWIN_VISIBLE,
			    (int *)0);
    }

    /*  The user wants to see in fullscreen mode  */
    for (i = actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_FULLSCREEN); i > 0; i--)
      gui_execute_action_id(ACTID_TOGGLE_FULLSCREEN);
    
    /*  The user request "play on start" */
    if(actions_on_start(gGui->actions_on_start, ACTID_PLAY))
      
      if((mediamark_get_current_mrl()) != NULL)
	gui_execute_action_id(ACTID_PLAY);
    
    /* Flush actions on start */
    if(actions_on_start(gGui->actions_on_start, ACTID_QUIT)) {
      gGui->actions_on_start[0] = ACTID_QUIT;
      gGui->actions_on_start[1] = ACTID_NOKEY;
    }
  }

  /* We can't handle signals here, xitk handle this, so
   * give a function callback for this.
   */
  xitk_register_signal_handler(gui_signal_handler, NULL);

  /*
   * event loop
   */

#ifdef HAVE_LIRC
  if(!no_lirc) {
    init_lirc();
  }
#endif

  /*  global event handler */
  gGui->widget_key = xitk_register_event_handler("NO WINDOW", None,
						 gui_handle_event, 
						 NULL,
						 gui_dndcallback, 
						 NULL, NULL);
  
  start_remote_server();

  /* Need for tvmode */
  video_window_set_fullscreen_mode((video_window_get_fullscreen_mode()));

  xitk_run();

  gGui->running = 0;

  kbindings_save_kbinding(gGui->kbindings);
  kbindings_free_kbinding(&gGui->kbindings);
}
