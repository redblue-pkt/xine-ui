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
 * gui inititalization and top-level event handling stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"

#include "event.h"
#include "parseskin.h"
#include "playlist.h"
#include "control.h"
#include "lirc.h"
#include "videowin.h"
#include "panel.h"
#include "actions.h"
#include <xine/video_out_x11.h>
#include "xine.h"
#include "utils.h"

#include "xscreensaver-remote.h"

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

/* Screensavers parameters */
typedef struct {

  /* XFree86 one */
  struct {
    int           timeout;
    int           interval;
    int           prefer_blanking;
    int           allow_exposures;
  } screensaver;

  /* Jamie's Zawinski xscreensaver */
  struct {
    int           was_running;
  } xscreensaver;

} screen_savers_t;

static screen_savers_t    ssavers;

/**
 * Configuration file lookup/set functions
 */
char *config_lookup_str(char *key, char *def) {

  return(gGui->config->lookup_str(gGui->config, key, def));
}

int config_lookup_int(char *key, int def) {

  return(gGui->config->lookup_int(gGui->config, key, def));
}

void config_set_str(char *key, char *value) {

  if(key) {
    gGui->config->set_str(gGui->config, key, value);
    config_save();
  }
}

void config_set_int(char *key, int value) {
  
  if(key) {
    gGui->config->set_int(gGui->config, key, value);
    config_save();
  }
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
static void gui_signal_handler (int sig) {
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
  }
}

/*
 * top-level event handler
 */
void gui_handle_event (XEvent *event, void *data) {
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;

  switch(event->type) {

  case MappingNotify:
    /* printf ("MappingNotify\n");*/
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case DestroyNotify:
    /*  printf ("DestroyNotify\n");  */
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

    if ((bevent->button==3) && (bevent->window == gGui->video_window)) {
      panel_toggle_visibility (NULL, NULL);
      break;
    }
    
  }
  break;

  case KeyPress:
    mykeyevent = event->xkey;

    /* printf ("KeyPress (state : %d, keycode: %d)\n", mykeyevent.state, mykeyevent.keycode);  */
      
    XLockDisplay (gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay (gGui->display);

    switch (mykey) {
    
    case XK_less:
    case XK_greater: {
      int x, y;
      unsigned int w, h, b, d;
      Window rootwin;
      
      XLockDisplay (gGui->display);
      
      if(XGetGeometry(gGui->display, 
		      gGui->video_window, &rootwin, 
		      &x, &y, &w, &h, &b, &d) != BadDrawable) {
      }

      if(mykey == XK_less) {
	w /= 1.2;
	h /= 1.2;
      }
      else {
	w *= 1.2;
	h *= 1.2;
      }
      
      XResizeWindow (gGui->display, gGui->video_window, w, h);
      XUnlockDisplay(gGui->display);

    }
      break;

    case XK_period:
      gui_change_spu_channel(NULL, (void*)GUI_NEXT);
      break;
      
    case XK_comma:
      gui_change_spu_channel(NULL, (void*)GUI_PREV);
      break;

    case XK_c:
    case XK_C:
      gui_control_show(NULL, NULL, 0);
      break;

    case XK_h:
    case XK_H:
      if(panel_is_visible())
	video_window_set_visibility(!video_window_is_visible());
      break;
      
    case XK_plus:
    case XK_KP_Add:
      gui_change_audio_channel(NULL, (void*)GUI_NEXT);
      break;
      
    case XK_minus:
    case XK_KP_Subtract:
      gui_change_audio_channel(NULL, (void*)GUI_PREV);
      break;
      
    case XK_space:
    case XK_P:
    case XK_p:
      gui_pause (NULL, (void*)(1), 0); 
      break;
      
    case XK_g:
    case XK_G:
      panel_toggle_visibility (NULL, NULL);
      break;

    case XK_f:
    case XK_F:
      gui_toggle_fullscreen(NULL, NULL);
      break;

    case XK_a:
    case XK_A:
      gui_toggle_aspect();
      break;

    case XK_q:
    case XK_Q:
      gui_exit(NULL, NULL);
      break;

    case XK_Return:
    case XK_KP_Enter:
      gui_play(NULL, NULL);
      break;

    case XK_Control_L:
    case XK_Control_R:
      if(!panel_is_visible())
	video_window_set_cursor_visibility(!video_window_is_cursor_visible());
      break;

    case XK_Next:
      gui_nextprev(NULL, (void*)GUI_NEXT);
      break;

    case XK_Prior:
      gui_nextprev(NULL, (void*)GUI_PREV);
      break;
      
    case XK_e:
    case XK_E:
      gui_eject(NULL, NULL);
      break;

    case XK_b:
    case XK_B:
      open_filebrowser(NULL, NULL);
      break;

    case XK_1:
    case XK_KP_1:
      gui_set_current_position (6553);
      break;

    case XK_2:
    case XK_KP_2:
      gui_set_current_position (13107);
      break;

    case XK_3:
    case XK_KP_3:
      gui_set_current_position (19660);
      break;

    case XK_4:
    case XK_KP_4:
      gui_set_current_position (26214);
      break;

    case XK_5:
    case XK_KP_5:
      gui_set_current_position (32767);
      break;

    case XK_6:
    case XK_KP_6:
      gui_set_current_position (39321);
      break;

    case XK_7:
    case XK_KP_7:
      gui_set_current_position (45874);
      break;

    case XK_8:
    case XK_KP_8:
      gui_set_current_position (52428);
      break;

    case XK_9:
    case XK_KP_9:
      gui_set_current_position (58981);
      break;

    case XK_0:
    case XK_KP_0:
      gui_set_current_position (0);
      break;
      /* FIXME
    case XK_Left:
      metronom_set_av_offset (metronom_get_av_offset () - 3600);
      break;
    case XK_Right:
      metronom_set_av_offset (metronom_get_av_offset () + 3600);
      break;
    case XK_Home:
      metronom_set_av_offset (0);
      break;
      */
    }

    break;

  case ConfigureNotify:
    /* FIXME: FIXED
    xine_window_handle_event(gGui->xine, (void *)event);
    */

    /* printf ("ConfigureNotify\n"); */
    /*  background */
    /* CHECKME
    XLOCK ();
    Imlib_apply_image(gGui->imlib_data, gGui->gui_bg_image, gGui->gui_panel_win);
    XUNLOCK ();
    paint_widget_list (gui_widget_list);
    XLOCK ();
    XSync(gGui->display, False);
    XUNLOCK ();
    */
    break;

  case ClientMessage:

    dnd_process_client_message (&gGui->xdnd, event);

    break;
    /*
  default:
    printf("Got event: %i\n", event->type);
    */
  }

}

/*
 * Callback function called by Xine engine.
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
      xine_play (gGui->xine, gGui->filename, 0 );
    } else {
      
      if(gGui->autoplay_options & QUIT_ON_STOP)
	gui_exit(NULL, NULL);
      
      video_window_show_logo();
      gGui->playlist_cur--;
    }
  }
}

/*
 * Initialize the GUI
 */
#define SEND_KEVENT(key) {                                                    \
     static XEvent startevent;                                                \
     startevent.type = KeyPress;                                              \
     startevent.xkey.type = KeyPress;                                         \
     startevent.xkey.send_event = True;                                       \
     startevent.xkey.display = gGui->display;                                 \
     startevent.xkey.window = gGui->panel_window;                             \
     startevent.xkey.keycode = XKeysymToKeycode(gGui->display, key);          \
     XSendEvent(gGui->display, gGui->panel_window, True, KeyPressMask,        \
                &startevent);                                                 \
   }
#ifndef	NAME_MAX
#define	NAME_MAX	20
#endif
void gui_init (int nfiles, char *filenames[]) {

  int                   i;
  XColor                dummy;
  char                 *display_name = ":0.0";
  char                  buffer[PATH_MAX + NAME_MAX + 1]; /* Enought ?? ;-) */

  /*
   * init playlist
   */

  for (i=0; i<nfiles; i++)
    gGui->playlist[i] = filenames[i];
  gGui->playlist_num = nfiles; 
  gGui->playlist_cur = 0;

  if (nfiles)
    strcpy(gGui->filename, gGui->playlist [gGui->playlist_cur]);
  else 
    sprintf(gGui->filename, "DROP A FILE ON XINE");

  /*
   * X / imlib stuff
   */

  if (!XInitThreads ()) {
    printf ("\nXInitThreads failed - looks like you don't have a "
	    "thread-safe xlib.\n");
    exit (1);
  } 

  
  if(getenv("DISPLAY"))
    display_name = getenv("DISPLAY");
  
  if((gGui->display = XOpenDisplay(display_name)) == NULL) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  XLockDisplay (gGui->display);

  gGui->screen = DefaultScreen(gGui->display);
  gGui->imlib_data = Imlib_init (gGui->display);

  /* 
   * Create logo image displayed into video window from
   * the official Xine logo.
   */
  sprintf(buffer, "%s/xine_logo.png", XINE_SKINDIR);
  if((gGui->video_window_logo_image= 
      Imlib_load_image(gGui->imlib_data, buffer)) == NULL) {
    fprintf(stderr, "Unable to load %s logo\n", buffer);
    exit(1);
  }

  Imlib_render(gGui->imlib_data, gGui->video_window_logo_image,
	       gGui->video_window_logo_image->rgb_width,
	       gGui->video_window_logo_image->rgb_height);

  gGui->video_window_logo_pixmap.image = 
    Imlib_move_image(gGui->imlib_data, gGui->video_window_logo_image);

  gGui->video_window_logo_pixmap.width = 
    gGui->video_window_logo_image->rgb_width;

  gGui->video_window_logo_pixmap.height = 
    gGui->video_window_logo_image->rgb_height;

  XAllocNamedColor(gGui->display, 
		   DefaultColormap(gGui->display, gGui->screen), 
		   "black", &gGui->black, &dummy);

  /*
   * create an icon pixmap
   */
  
  gGui->icon = XCreateBitmapFromData (gGui->display, 
				      DefaultRootWindow(gGui->display),
				      xine_bits, 40, 40);


  XUnlockDisplay (gGui->display);

  /*
   * create and map panel and video window
   */

  xine_pid = getppid();

  widget_init(gGui->display);

  gGui->running = 1;

  video_window_init ();
  
  panel_init ();

  /*
   * Store original screensaver parameters and sets new ones
   */
  XGetScreenSaver(gGui->display ,&ssavers.screensaver.timeout, 
		  &ssavers.screensaver.interval,
		  &ssavers.screensaver.prefer_blanking, 
		  &ssavers.screensaver.allow_exposures);
  
  if((XSetScreenSaver(gGui->display, 0, 0, 
		      DontPreferBlanking, DontAllowExposures)) == BadValue) {
    fprintf(stderr, "XSetScreenSaver() failed: %s\n", strerror(errno));
  }
  
  /*
   * XScreenSaver specific.
   */
  xscreensaver_remote_init(gGui->display);
  ssavers.xscreensaver.was_running = is_xscreensaver_running(gGui->display);

  if(ssavers.xscreensaver.was_running == 1) {
    if(xscreensaver_kill_server(gGui->display) < 0)
      ssavers.xscreensaver.was_running = 0;
  }
  
}

/*
 *
 */
void gui_run (void) {
  
  struct sigaction      action;

  panel_add_autoplay_buttons();

  panel_update_channel_display () ;
  panel_update_mrl_display ();

  /*  The user request "play on start" */
  if(gGui->autoplay_options & PLAY_ON_START) {
    /* probe DVD && VCD  */
    int i = 0;
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    
    while(autoplay_plugins[i] != NULL) {
      
      if(gGui->autoplay_options & PLAY_FROM_DVD)

	if(!strcasecmp(autoplay_plugins[i], "DVD")) {
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine, "DVD");
	  int j = 0;
	  
	  while(autoplay_mrls[j]) {
	    gGui->playlist[gGui->playlist_num + j] = autoplay_mrls[j];
	    j++;
	  }
	  gGui->playlist_num += j;
	  gGui->playlist_cur = 0;
	  gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	  
	}

      if(gGui->autoplay_options & PLAY_FROM_VCD)
	if(!strcasecmp(autoplay_plugins[i], "VCD")) {
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine, "VCD");
	  int j = 0;
	    
	  while(autoplay_mrls[j]) {
	    gGui->playlist[gGui->playlist_num + j] = autoplay_mrls[j];
	    j++;
	  }
	  gGui->playlist_num += j;
	  gGui->playlist_cur = 0;
	  gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	}

      i++;
    }
    
    /*  The user wants to hide control panel  */
    if(panel_is_visible() && (gGui->autoplay_options & HIDEGUI_ON_START))
      SEND_KEVENT(XK_G);
    
    /*  The user wants to see in fullscreen mode  */
    if(gGui->autoplay_options & FULL_ON_START)
      SEND_KEVENT(XK_F);
    
    if(gGui->playlist[0] != NULL)
      SEND_KEVENT(XK_Return);

    gGui->autoplay_options |= PLAYED_ON_START;    
  }

  /* install sighandler */
  action.sa_handler = gui_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGHUP, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGHUP) failed: %s\n", strerror(errno));
  }
  action.sa_handler = gui_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR1, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGUSR1) failed: %s\n", strerror(errno));
  }
  action.sa_handler = gui_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR2, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGUSR2) failed: %s\n", strerror(errno));
  }

  /*
   * event loop
   */

#ifdef HAVE_LIRC
  if(!no_lirc) {
    init_lirc();
  }
#endif

  //global event handler
  gGui->widget_key = widget_register_event_handler("NO WINDOW", None,
						   gui_handle_event, 
						   NULL,
						   gui_dndcallback, 
						   NULL, NULL);
  
  widget_run();

  gGui->running = 0;
  
  /*
   * Restore screensaver parameters
   */
  if((XSetScreenSaver(gGui->display, ssavers.screensaver.timeout, 
		      ssavers.screensaver.interval, 
		      ssavers.screensaver.prefer_blanking, 
		      ssavers.screensaver.allow_exposures)) == BadValue) {
    fprintf(stderr, "XSetScreenSaver() failed: %s\n", strerror(errno));
  }

  /*
   * Restart XScreenSaver.
   */
  if(ssavers.xscreensaver.was_running == 1)
    xscreensaver_start_server();
}
