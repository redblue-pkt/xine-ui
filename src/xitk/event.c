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

#include "Imlib-light/Imlib.h"

#include "event.h"
#include "playlist.h"
#include "control.h"
#include "lirc.h"
#include "videowin.h"
#include "panel.h"
#include "actions.h"
#include <xine/video_out_x11.h>
#include <xine.h>
#include <xine/xineutils.h>
#include "mrl_browser.h"
#include "skins.h"
#include "errors.h"
#include "network.h"
#include "i18n.h"

#include "xitk.h"

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

static void ssaver_timeout_cb(void *data, cfg_entry_t *cfg) {
  gGui->ssaver_timeout = cfg->num_value;
}

/*
 * Callback for snapshots saving location.
 */
static void snapshot_loc_cb(void *data, cfg_entry_t *cfg) {
  gGui->snapshot_location = cfg->str_value;
}

static int actions_on_start(action_id_t actions[], action_id_t a) {
  int i = 0, num = 0;
  while(actions[i] != ACTID_NOKEY) {
    if(actions[i] == a)
      num++;
    i++;
  }
  return num;
}

void config_save(void) {

  gGui->config->save(gGui->config);
}

void config_reset(void) {

  gGui->config->read(gGui->config, gGui->configfile);
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
      
      xine_stop(gGui->xine);
    }
    break;
  }

}

/*
 *
 */
void gui_execute_action_id(action_id_t action) {
  xine_event_t   xine_event;
  static long int numeric_arg = 0;  /* Saves accumulated numeric value */

  if(action & ACTID_IS_INPUT_EVENT) {

    if (action >= ACTID_EVENT_NUMBER_0 && action <= ACTID_EVENT_NUMBER_9) {
      numeric_arg *= 10;
      numeric_arg += (action - ACTID_EVENT_NUMBER_0);
    } else if (action == ACTID_EVENT_NUMBER_10_ADD) {
      numeric_arg += 10;
    } else numeric_arg = 0;
    
    /* events for advanced input plugins. */
    xine_event.type = action & ~ACTID_IS_INPUT_EVENT;
    xine_send_event(gGui->xine, &xine_event);
    return;
  }

  if(action & ACTID_IS_INPUT_EVENT) {
    /* events for advanced input plugins. */
    xine_event.type = action & ~ACTID_IS_INPUT_EVENT;
    xine_send_event(gGui->xine, &xine_event);
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

  case ACTID_SPU_NEXT: {
      int i;
      if (numeric_arg == 0) numeric_arg = 1;
      for (i=1; i<=numeric_arg; i++)       
	gui_change_spu_channel(NULL, (void*)GUI_NEXT);
    }
    break;
    
  case ACTID_SPU_PRIOR: {
      int i;
      if (numeric_arg == 0) numeric_arg = 1;
      for (i=1; i<=numeric_arg; i++)       
	gui_change_spu_channel(NULL, (void*)GUI_PREV);
    }
    break;
    
  case ACTID_CONTROLSHOW:
    gui_control_show(NULL, NULL);
    break;
    
  case ACTID_TOGGLE_WINOUT_VISIBLITY:
    gui_toggle_visibility (NULL, NULL);
    break;
      
  case ACTID_AUDIOCHAN_NEXT: {
      int i;
      if (numeric_arg == 0) numeric_arg = 1;
      for (i=1; i<=numeric_arg; i++)       
	gui_change_audio_channel(NULL, (void*)GUI_NEXT);
    }
      break;
      
  case ACTID_AUDIOCHAN_PRIOR: {
      int i;
      if (numeric_arg == 0) numeric_arg = 1;
      for (i=1; i<=numeric_arg; i++)       
	gui_change_audio_channel(NULL, (void*)GUI_PREV);
  }
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

  case ACTID_SETUP:
    gui_setup_show(NULL, NULL);
    break;

  case ACTID_MRL_NEXT: {
    int i;
    if (numeric_arg == 0) numeric_arg = 1;
    for (i=1; i<=numeric_arg; i++)       
      gui_nextprev(NULL, (void*)GUI_NEXT);
  }
    break;

  case ACTID_MRL_PRIOR: {
    int i;
    if (numeric_arg == 0) numeric_arg = 1;
    for (i=1; i<=numeric_arg; i++)       
      gui_nextprev(NULL, (void*)GUI_PREV);
  }
    break;
      
  case ACTID_EJECT:
    gui_eject(NULL, NULL);
    break;

  case ACTID_SET_CURPOS:
    /* Number is a percentage */
    gui_set_current_position((65534 * numeric_arg)/100);
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
    numeric_arg = -numeric_arg;
  case ACTID_SEEK_REL_p:
    if (numeric_arg != 0) gui_seek_relative (numeric_arg);
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
    xine_set_av_offset (gGui->xine, xine_get_av_offset (gGui->xine) - 3600);
    break;

  case ACTID_AV_SYNC_p3600:
    xine_set_av_offset (gGui->xine, xine_get_av_offset (gGui->xine) + 3600);
    break;

  case ACTID_AV_SYNC_RESET:
    xine_set_av_offset (gGui->xine, 0);
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
    gui_change_zoom(1);
    break;

  case ACTID_ZOOM_OUT:
    gui_change_zoom(-1);
    break;

  case ACTID_ZOOM_RESET:
    gui_reset_zoom();
    break;
    
  case ACTID_GRAB_POINTER:
    if(!gGui->cursor_grabbed) {
      if(!panel_is_visible())
	XGrabPointer(gGui->display, gGui->video_window, 1, None, 
		     GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
      
      gGui->cursor_grabbed = 1;
    }
    else {
      XUngrabPointer(gGui->display, CurrentTime);
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

  default:
    break;
  }

  /* Some sort of function was done given. Clear numeric argument. */
  numeric_arg = 0;

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
 * Callback functions called by Xine engine.
 */

void gui_status_callback (int nStatus) {

  if (gGui->ignore_status)
    return;

  /* printf ("gui status callback : %d\n", nStatus); */
  
  if (nStatus == XINE_STOP) {
    gGui->playlist_cur++;
    panel_reset_slider ();

    if (gGui->playlist_cur < gGui->playlist_num) {
      gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
      if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
	gui_handle_xine_error();

    } else {
      
      if(gGui->actions_on_start[0] == ACTID_QUIT)
	gui_exit(NULL, NULL);
      
      gGui->playlist_cur--;
    }
  }
}

char *gui_next_mrl_callback (void) {

  if (gGui->playlist_cur >= (gGui->playlist_num-1)) 
    return NULL;

  return gGui->playlist[gGui->playlist_cur+1];
}

void gui_branched_callback (void) {

  if (gGui->playlist_cur < (gGui->playlist_num-1)) {
  
    gGui->playlist_cur++;
    panel_reset_slider ();

    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
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
  int                   i;
  char                 *display_name = ":0.0";

  /*
   * init playlist
   */

  for (i = 0; i < nfiles; i++)
    gGui->playlist[i] = filenames[i];

  gGui->playlist_num = nfiles; 
  gGui->playlist_cur = 0;

  if (nfiles)
    strcpy(gGui->filename, gGui->playlist [gGui->playlist_cur]);
  else 
    panel_set_no_mrl();

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

  if (gGui->config->register_bool (gGui->config, "gui.xsynchronize", 0,
				   _("synchronized X protocol (debug)"), NULL, NULL, NULL)) {
    XSynchronize (gGui->display, True);
    fprintf (stderr, _("Warning! Synchronized X activated - this is way slow...\n"));
  }

  gGui->layer_above = 
    gGui->config->register_bool (gGui->config, "gui.layer_above", 1,
				 _("use wm layer property to place window on top"), 
				 NULL, NULL, NULL);
  
  gGui->snapshot_location = 
    gGui->config->register_string (gGui->config, "gui.snapshotdir", 
				   (char *) (xine_get_homedir()),
				   _("where snapshots will be saved"),
				   NULL, snapshot_loc_cb, NULL);
  
  gGui->ssaver_timeout =
    gGui->config->register_num (gGui->config, "gui.screensaver_timeout", 10,
				_("time between two screensaver fake events, 0 to disable"),
				NULL, ssaver_timeout_cb, NULL);
  
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

  video_window_init (window_attribute);

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

  /* autoscan playlist  */
  if(gGui->autoscan_plugin != NULL) {
    char **autoscan_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    
    int i;

    if(autoscan_plugins) {

      for(i=0; autoscan_plugins[i] != NULL; ++i) {

	if(!strcasecmp(autoscan_plugins[i], gGui->autoscan_plugin)) {
	  int num_mrls;
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine,
							 gGui->autoscan_plugin,
							 &num_mrls);
	  int j;
	  
	  if(autoplay_mrls) {
	    for (j=0; j<num_mrls; j++) {
	      gGui->playlist[gGui->playlist_num + j] = autoplay_mrls[j];
	    }
	    gGui->playlist_num += j;
	    gGui->playlist_cur = 0;
	    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	  }
	}    
      }
    }
  }  

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
      gui_execute_action_id(ACTID_TOGGLE_WINOUT_VISIBLITY);
    }

    /*  The user wants to see in fullscreen mode  */
    for (i = actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_FULLSCREEN); i > 0; i--)
      gui_execute_action_id(ACTID_TOGGLE_FULLSCREEN);
    
    /*  The user request "play on start" */
    if(actions_on_start(gGui->actions_on_start, ACTID_PLAY))
      
      if(gGui->playlist[0] != NULL)
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

  xitk_run();

  gGui->running = 0;

  kbindings_save_kbinding(gGui->kbindings);
  kbindings_free_kbinding(&gGui->kbindings);
}
