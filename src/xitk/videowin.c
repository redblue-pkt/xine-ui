/* 
 * Copyright (C) 2000-2004 the xine project
 * 
 * This file is part of xine, a free video player.
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
 * video window handling functions
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XTESTEXTENSION
#include <X11/extensions/XTest.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

#define EST_KEEP_VALID  10	  /* #frames to allow for changing fps */
#define EST_MAX_JITTER  0.01	  /* maximum jitter to detect valid fps */
#define EST_MAX_DIFF    0.01      /* maximum diff to detect valid fps */
#define ABS(x) ((x)>0?(x):-(x))

extern gGui_t           *gGui;

/* Video window private structure */
typedef struct {
  xitk_widget_list_t    *wl;
  Cursor                 cursor[3];       /* Cursor pointers                       */
  char                   window_title[1024];
  int                    current_cursor;  /* arrow or hand */
  int                    cursor_visible;
  int                    cursor_timer;
  Visual	        *visual;          /* Visual for video window               */
  Colormap	         colormap;        /* Colormap for video window		   */
  XClassHint            *xclasshint;
  XClassHint            *xclasshint_fullscreen;
  XClassHint            *xclasshint_borderless;
  GC                     gc;

  int                    video_width;     /* size of currently displayed video     */
  int                    video_height;

  int                    frame_width;     /* frame size, from xine-lib */
  int                    frame_height;

  double                 video_duration;  /* frame duratrion in seconds */
  double                 video_average;   /* average frame duration in seconds */
  double                 use_duration;    /* duration used for tv mode selection */
  int                    video_duration_valid; /* is use_duration trustable? */
  int                    win_width;       /* size of non-fullscreen window         */
  int                    win_height;
  int                    output_width;    /* output video window width/height      */
  int                    output_height;
  float                  mag;

  int                    stream_resize_window; /* Boolean, 1 if new stream resize output window */
  int                    zoom_small_stream; /* Boolean, 1 to double size small streams */

  int                    fullscreen_mode; /* bitfield:                                      */
  int                    fullscreen_req;  /* WINDOWED_MODE, FULLSCR_MODE or FULLSCR_XI_MODE */
  int                    fullscreen_width;
  int                    fullscreen_height;

  int                    xinerama_fullscreen_x; /* will contain paramaters for very 
						   fullscreen in xinerama mode */
  int                    xinerama_fullscreen_y;
  int                    xinerama_fullscreen_width;
  int                    xinerama_fullscreen_height;

  int                    visible_width;   /* Size of currently visible portion of screen */
  int                    visible_height;  /* May differ from fullscreen_* e.g. for TV mode */
  double                 visible_aspect;  /* Pixel ratio of currently visible screen */

  int                    using_xinerama;
#ifdef HAVE_XINERAMA
  XineramaScreenInfo    *xinerama;   /* pointer to xinerama struct, or NULL */
  int                    xinerama_cnt;    /* number of screens in Xinerama */
#endif

  int                    xwin;            /* current X location */
  int                    ywin;            /* current Y location */
  int                    old_xwin;
  int                    old_ywin;

  int                    desktopWidth;    /* desktop width */
  int                    desktopHeight;   /* desktop height */
  int                    depth;
  int                    show;
  int                    borderless;      /* borderless window (for windowed mode)? */

  Bool                   have_xtest;
#ifdef HAVE_XTESTEXTENSION
  KeyCode                kc_shift_l;      /* Fake key to send */
#endif

  XWMHints              *wm_hint;

  xitk_register_key_t    widget_key;
  xitk_register_key_t    old_widget_key;

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension stuff */
  XF86VidModeModeInfo**  XF86_modelines;
  int                    XF86_modelines_count;
#endif

  int                    hide_on_start; /* user use '-H' arg, don't map 
					   video window the first time */

  struct timeval         click_time;

  pthread_t              second_display_thread;
  int                    second_display_running;
  
} gVw_t;

static gVw_t            *gVw;
static void video_window_handle_event (XEvent *event, void *data);
static void video_window_adapt_size (void);

static void _video_window_resize_cb(void *data, xine_cfg_entry_t *cfg) {
  gVw->stream_resize_window = cfg->num_value;
}
static void _video_window_zoom_small_cb(void *data, xine_cfg_entry_t *cfg) {
  gVw->zoom_small_stream = cfg->num_value;
}

static Bool have_xtestextention(void) {  
  Bool xtestext = False;
#ifdef HAVE_XTESTEXTENSION
  int dummy1 = 0, dummy2 = 0, dummy3 = 0, dummy4 = 0;
  
  XLockDisplay(gGui->video_display);
  xtestext = XTestQueryExtension(gGui->video_display, &dummy1, &dummy2, &dummy3, &dummy4);
  XUnlockDisplay(gGui->video_display);
#endif
  
  return xtestext;
}

static void _set_window_title(void) {
  XmbSetWMProperties(gGui->video_display, gGui->video_window, gVw->window_title, gVw->window_title, NULL, 0, NULL, NULL, NULL);
  XSync(gGui->video_display, False);
}

/* 
 * very small X event loop for the second display
 */
static void *second_display_loop (void *dummy) {
  
  while(gVw->second_display_running) {
    XEvent   xevent;
    int      got_event;

    xine_usec_sleep(20000);
    
    do {
        XLockDisplay(gGui->video_display);
        got_event = XCheckMaskEvent(gGui->video_display, INPUT_MOTION, &xevent);
        XUnlockDisplay(gGui->video_display);
        
        if( got_event && gGui->stream ) {
          video_window_handle_event(&xevent, NULL);
        }
    } while (got_event);
  }
  
  pthread_exit(NULL);
}


static void video_window_find_visual (Visual **visual_return, int *depth_return) {
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
    vinfo_tmpl.screen = gGui->video_screen;
    vinfo_tmpl.class  = (gGui->prefered_visual_class != -1
			 ? gGui->prefered_visual_class : TrueColor);
    vinfo = XGetVisualInfo(gGui->video_display,
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
    vinfo = XGetVisualInfo(gGui->video_display,
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

    XGetWindowAttributes(gGui->video_display, (DefaultRootWindow(gGui->video_display)), &attribs);

    depth = attribs.depth;
  
    if (XMatchVisualInfo(gGui->video_display, gGui->video_screen, depth, TrueColor, &vinfo)) {
      visual = vinfo.visual;
    } else {
      printf (_("gui_main: couldn't find true color visual.\n"));

      depth = DefaultDepth (gGui->video_display, gGui->video_screen);
      visual = DefaultVisual (gGui->video_display, gGui->video_screen); 
    }
  }

  if (depth_return != NULL)
    *depth_return = depth;
  if (visual_return != NULL)
    *visual_return = visual;
}


/*
 * Let the video driver override the selected visual
 */
void video_window_select_visual (void) {
  XVisualInfo *vinfo = (XVisualInfo *) -1;

  if (gGui->vo_port && gGui->video_display == gGui->display) {

    xine_gui_send_vo_data(gGui->stream, XINE_GUI_SEND_SELECT_VISUAL, &vinfo);

    if (vinfo != (XVisualInfo *) -1) {
      if (! vinfo) {
        fprintf (stderr, _("videowin: output driver cannot select a working visual\n"));
        exit (1);
      }
      gGui->visual = vinfo->visual;
      gGui->depth  = vinfo->depth;
    }
    if (gGui->visual != gVw->visual) {
      printf (_("videowin: output driver overrides selected visual to visual id 0x%lx\n"), gGui->visual->visualid);
      XLockDisplay (gGui->display);
      gui_init_imlib (gGui->visual);
      XUnlockDisplay (gGui->display);
      video_window_adapt_size ();
    }
  }
}

/*
 * will modify/create video output window based on
 *
 * - fullscreen flags
 * - win_width/win_height
 *
 * will set
 * output_width/output_height
 * visible_width/visible_height/visible_aspect
 */
static void video_window_adapt_size (void) { 
  XSizeHints            hint;
  XWMHints             *wm_hint;
  XSetWindowAttributes  attr;
  Atom                  prop;
  Atom                  wm_delete_window;
  MWMHints              mwmhints;
  XEvent                xev;
  XGCValues             xgcv;
  Window                old_video_window = None;
  int                   border_width;

  XLockDisplay (gGui->video_display);

  if(gGui->use_root_window) { /* Using root window, but not really */

    gVw->xwin = gVw->ywin = 0;
    gVw->output_width    = gVw->fullscreen_width;
    gVw->output_height   = gVw->fullscreen_height;
    gVw->visible_width   = gVw->fullscreen_width;
    gVw->visible_height  = gVw->fullscreen_height;
    gVw->visible_aspect  = gGui->pixel_aspect = 1.0;

    if(gGui->video_window == None) {
      XGCValues   gcv;
      Window      wparent;
      Window      rootwindow = None;

      gVw->fullscreen_mode = FULLSCR_MODE;
      gVw->visual          = gGui->visual;
      gVw->depth           = gGui->depth;
      gVw->colormap        = gGui->colormap;

      if(gGui->video_display != gGui->display)
        video_window_find_visual (&gVw->visual, &gVw->depth);
      
      /* This couldn't happen, but we're paranoid ;-) */
      if((rootwindow = xitk_get_desktop_root_window(gGui->video_display, 
						    gGui->video_screen, &wparent)) == None)
	rootwindow = DefaultRootWindow(gGui->video_display);

      attr.override_redirect = True;
      attr.background_pixel  = gGui->black.pixel;
      
      border_width = 0;
      
      gGui->video_window = XCreateWindow(gGui->video_display, rootwindow,
					 0, 0, gVw->fullscreen_width, gVw->fullscreen_height, 
					 border_width, 
					 CopyFromParent, CopyFromParent, CopyFromParent, 
					 CWBackPixel | CWOverrideRedirect, &attr);
      
      if(gGui->video_display == gGui->display)
        xitk_widget_list_set(gVw->wl, WIDGET_LIST_WINDOW, (void *) gGui->video_window);

      if(gGui->vo_port)
	xine_gui_send_vo_data(gGui->stream, 
			      XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);

      XSelectInput(gGui->video_display, gGui->video_window, ExposureMask);
      
      _set_window_title();
      
      gcv.foreground         = gGui->black.pixel;
      gcv.background         = gGui->black.pixel;
      gcv.graphics_exposures = False;
      gVw->gc = XCreateGC(gGui->video_display, gGui->video_window, 
			  GCForeground | GCBackground | GCGraphicsExposures, &gcv);

      if(gGui->video_display == gGui->display)
        xitk_widget_list_set(gVw->wl, WIDGET_LIST_GC, gVw->gc);

      hint.flags  = USSize | USPosition | PPosition | PSize;
      hint.x      = 0;
      hint.y      = 0;
      hint.width  = gVw->fullscreen_width;
      hint.height = gVw->fullscreen_height;
      XSetNormalHints(gGui->video_display, gGui->video_window, &hint);
      
      XClearWindow(gGui->video_display, gGui->video_window);
      
      XMapWindow(gGui->video_display, gGui->video_window);
      
      XLowerWindow(gGui->video_display, gGui->video_window);

      gVw->old_widget_key = gVw->widget_key;

      XUnlockDisplay (gGui->video_display);

      if(gGui->video_display == gGui->display)
        gVw->widget_key = xitk_register_event_handler("video_window", 
						    gGui->video_window, 
						    video_window_handle_event,
						    NULL,
						    gui_dndcallback,
						    NULL, NULL);
    
      return;
    }
    
    XUnlockDisplay (gGui->video_display);
    
    return;
  }


#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension
   * In case a fullscreen request is received or if already in fullscreen, the
   * appropriate modeline will be looked up and used.
   */
  if(( (!(gVw->fullscreen_req & WINDOWED_MODE)) || (!(gVw->fullscreen_mode & WINDOWED_MODE)))
     && (gVw->XF86_modelines_count > 1)) {
    int search = 0;
    
    /* skipping first entry because it is the current modeline */
    for(search = 1; search < gVw->XF86_modelines_count; search++) {
       if(gVw->XF86_modelines[search]->hdisplay >= gVw->video_width)
	 break;
    }

    /*
     * in case we have a request for a resolution higher than any available
     * ones we take the highest currently available.
     */
    if((!(gVw->fullscreen_mode & WINDOWED_MODE)) && (search >= gVw->XF86_modelines_count))
       search = 0;
       
    /* just switching to a different modeline if necessary */
    if(!(search >= gVw->XF86_modelines_count)) {
       if(XF86VidModeSwitchToMode(gGui->video_display, XDefaultScreen(gGui->video_display), gVw->XF86_modelines[search])) {
          double res_h, res_v;
	  
	  gGui->XF86VidMode_fullscreen = 1;	  
	  gVw->fullscreen_width        = gVw->XF86_modelines[search]->hdisplay;
	  gVw->fullscreen_height       = gVw->XF86_modelines[search]->vdisplay;
	  
	  /* update pixel aspect */
	  res_h = (DisplayWidth  (gGui->video_display, gGui->video_screen)*1000 
		   / DisplayWidthMM (gGui->video_display, gGui->video_screen));
	  res_v = (DisplayHeight (gGui->video_display, gGui->video_screen)*1000
		   / DisplayHeightMM (gGui->video_display, gGui->video_screen));
  
	  gGui->pixel_aspect    = res_v / res_h;
#ifdef DEBUG
	  printf("pixel_aspect: %f\n", gGui->pixel_aspect);
#endif

	  // TODO
	  /*
	   * just in case the mouse pointer is off the visible area, move it
	   * to the middle of the video window
	   */
	  XWarpPointer(gGui->video_display, None, gGui->video_window, 0, 0, 0, 0, gVw->fullscreen_width/2, gVw->fullscreen_height/2);
	  
	  XF86VidModeSetViewPort(gGui->video_display, XDefaultScreen(gGui->video_display), 0, 0);
          
	  /*
	   * if this is true, we are back at the original resolution, so there
	   * is no need to further worry about anything.
	   */
	  if((!(gVw->fullscreen_mode & WINDOWED_MODE)) && (search == 0))
	    gGui->XF86VidMode_fullscreen = 0;
       } else {
	  xine_error(_("XF86VidMode Extension: modeline switching failed.\n"));
       }
    }
  }
#endif/* HAVE_XF86VIDMODE */

#ifdef HAVE_XINERAMA
  if (gVw->xinerama) {
    int           i;
    int           knowLocation = 0;
    Window        root_win, dummy_win;
    int           x_mouse,y_mouse;
    int           dummy_x,dummy_y;
    unsigned int  dummy_opts;

    /* someday this could also use the centre of the window as the
     * test point I guess.  Right now it's the upper-left.
     */
    if (gGui->video_window != None) {
      if (gVw->xwin >= 0 && gVw->ywin >= 0 &&
	  gVw->xwin < gVw->desktopWidth && gVw->ywin < gVw->desktopHeight) {
	knowLocation = 1;
      }
    }
    
    if (gVw->fullscreen_req & FULLSCR_XI_MODE) {
      hint.x = gVw->xinerama_fullscreen_x;
      hint.y = gVw->xinerama_fullscreen_y;
      hint.width  = gVw->xinerama_fullscreen_width;
      hint.height = gVw->xinerama_fullscreen_height;
      gVw->fullscreen_width = hint.width;
      gVw->fullscreen_height = hint.height;
    } 
    else {
      /* Get mouse cursor position */
      XQueryPointer(gGui->video_display, RootWindow(gGui->video_display, gGui->video_screen), 
		    &root_win, &dummy_win, &x_mouse, &y_mouse, &dummy_x, &dummy_y, &dummy_opts);

      for (i = 0; i < gVw->xinerama_cnt; i++) {
	if (
	    (knowLocation == 1 &&
	     gVw->xwin >= gVw->xinerama[i].x_org &&
	     gVw->ywin >= gVw->xinerama[i].y_org &&
	     gVw->xwin < gVw->xinerama[i].x_org+gVw->xinerama[i].width &&
	     gVw->ywin < gVw->xinerama[i].y_org+gVw->xinerama[i].height) ||
	    (knowLocation == 0 &&
	     x_mouse >= gVw->xinerama[i].x_org &&
	     y_mouse >= gVw->xinerama[i].y_org &&
	     x_mouse < gVw->xinerama[i].x_org+gVw->xinerama[i].width &&
	     y_mouse < gVw->xinerama[i].y_org+gVw->xinerama[i].height)) {
	  /*  gVw->xinerama[i].screen_number ==
	      XScreenNumberOfScreen(XDefaultScreenOfDisplay(gGui->video_display)))) {*/
	  hint.x = gVw->xinerama[i].x_org;
	  hint.y = gVw->xinerama[i].y_org;
	  
	  if(knowLocation == 0) {
	    gVw->old_xwin = hint.x;
	    gVw->old_ywin = hint.y;
	  }

	  if (!(gVw->fullscreen_req & WINDOWED_MODE)) {
	    hint.width  = gVw->xinerama[i].width;
	    hint.height = gVw->xinerama[i].height;
	    gVw->fullscreen_width = hint.width;
	    gVw->fullscreen_height = hint.height;
	  } 
	  else {
	    hint.width  = gVw->video_width;
	    hint.height = gVw->video_height;
	  }
	  break;
	}
      }
    }
  } 
  else {
    hint.x = 0;
    hint.y = 0;
    if(!(gVw->fullscreen_req & WINDOWED_MODE)) {
      hint.width  = gVw->fullscreen_width;
      hint.height = gVw->fullscreen_height;
    } else {
      hint.width  = gVw->win_width;
      hint.height = gVw->win_height;
    }
  }
#else /* HAVE_XINERAMA */
  hint.x = 0;
  hint.y = 0;   /* for now -- could change later */
#endif /* HAVE_XINERAMA */
  
  gVw->visible_width  = gVw->fullscreen_width;
  gVw->visible_height = gVw->fullscreen_height;
  gVw->visible_aspect = gGui->pixel_aspect;

  /* Retrieve size/aspect from tvout backend, if it should be set */
  if(gGui->tvout) {
    tvout_get_size_and_aspect(gGui->tvout,
			      &gVw->visible_width, &gVw->visible_height,
			      &gVw->visible_aspect);
    tvout_get_size_and_aspect(gGui->tvout,
			      &hint.width, &hint.height, NULL);    
    tvout_set_fullscreen_mode(gGui->tvout, 
			      !(gVw->fullscreen_req & WINDOWED_MODE) ? 1 : 0,
			      gVw->visible_width, gVw->visible_height);
  }

#ifdef HAVE_XINERAMA
  /* ask for xinerama fullscreen mode */
  if (gVw->xinerama && (gVw->fullscreen_req & FULLSCR_XI_MODE)) {

    if (gGui->video_window) {
      int dummy;

      if ((gVw->fullscreen_mode & FULLSCR_XI_MODE) && gGui->visual == gVw->visual) {
        if (gVw->visible_width != gVw->output_width || gVw->visible_height != gVw->output_height) {
          /*
           * resizing the video window may be necessary if the modeline or tv mode has
           * just been switched
           */
          XMoveResizeWindow (gGui->video_display, gGui->video_window, hint.x, hint.y,
					  gVw->visible_width, gVw->visible_height);
          gVw->output_width    = gVw->visible_width;
          gVw->output_height   = gVw->visible_height;
        }
        gVw->fullscreen_mode = gVw->fullscreen_req;
        XUnlockDisplay (gGui->video_display);

        return;
      }

      xitk_get_window_position(gGui->video_display, gGui->video_window,
      			       &gVw->old_xwin, &gVw->old_ywin, &dummy, &dummy);

      if(gGui->video_display == gGui->display)
        xitk_unregister_event_handler(&gVw->old_widget_key);
      old_video_window = gGui->video_window;
    }

    gVw->fullscreen_mode = gVw->fullscreen_req;
    gVw->visual   = gGui->visual;
    gVw->depth    = gGui->depth;
    gVw->colormap = gGui->colormap;
      
    if(gGui->video_display != gGui->display)
      video_window_find_visual (&gVw->visual, &gVw->depth);

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = gGui->black.pixel;
    attr.colormap	   = gVw->colormap;

    border_width           = 0;

    gGui->video_window =
      XCreateWindow (gGui->video_display, DefaultRootWindow(gGui->video_display),
		     hint.x, hint.y, gVw->visible_width, gVw->visible_height,
		     border_width, gVw->depth, InputOutput,
		     gVw->visual,
		     CWBackPixel | CWBorderPixel | CWColormap, &attr);

    if(gGui->video_display == gGui->display)
      xitk_widget_list_set(gVw->wl, WIDGET_LIST_WINDOW, (void *) gGui->video_window);

    if(gGui->vo_port)
      xine_gui_send_vo_data(gGui->stream,
			    XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);

    if (gVw->xclasshint_fullscreen != NULL)
      XSetClassHint(gGui->video_display, gGui->video_window, gVw->xclasshint_fullscreen);

    hint.win_gravity = StaticGravity;
    hint.flags  = PPosition | PSize | PWinGravity;

    _set_window_title();

    XSetWMNormalHints (gGui->video_display, gGui->video_window, &hint);

    XSetWMHints(gGui->video_display, gGui->video_window, gVw->wm_hint);

    gVw->output_width    = hint.width;
    gVw->output_height   = hint.height;

    /*
     * wm, no borders please
     */
    prop = XInternAtom(gGui->video_display, "_MOTIF_WM_HINTS", False);
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0;
    XChangeProperty(gGui->video_display, gGui->video_window, prop, prop, 32,
		    PropModeReplace, (unsigned char *) &mwmhints,
		    PROP_MWM_HINTS_ELEMENTS);

  } else
#endif /* HAVE_XINERAMA */
  if (!(gVw->fullscreen_req & WINDOWED_MODE)) {

    if (gGui->video_window) {
      int dummy;

      if ((!(gVw->fullscreen_mode & WINDOWED_MODE)) && (gGui->visual == gVw->visual)) {
//#ifdef HAVE_XF86VIDMODE
//	if(gVw->XF86_modelines_count > 1) {
	if ((gVw->visible_width != gVw->output_width) 
	    || (gVw->visible_height != gVw->output_height)) {
	   /*
	    * resizing the video window may be necessary if the modeline or tv mode has
	    * just been switched
	    */
	   XResizeWindow (gGui->video_display, gGui->video_window,
			  gVw->visible_width, gVw->visible_height);
	   gVw->output_width    = gVw->visible_width;
	   gVw->output_height   = gVw->visible_height;
	}
//#endif
        gVw->fullscreen_mode = gVw->fullscreen_req;
	XUnlockDisplay (gGui->video_display);
	
	return;
      }
      
      xitk_get_window_position(gGui->video_display, gGui->video_window,
      			       &gVw->old_xwin, &gVw->old_ywin, &dummy, &dummy);
      
      if(gGui->video_display == gGui->display)
        xitk_unregister_event_handler(&gVw->old_widget_key);
      old_video_window = gGui->video_window;
    }

    gVw->fullscreen_mode = gVw->fullscreen_req;
    gVw->visual   = gGui->visual;
    gVw->depth    = gGui->depth;
    gVw->colormap = gGui->colormap;
      
    if(gGui->video_display != gGui->display)
      video_window_find_visual (&gVw->visual, &gVw->depth);

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = gGui->black.pixel;
    attr.colormap	   = gVw->colormap;

    border_width           = 0;

    gGui->video_window = 
      XCreateWindow (gGui->video_display, DefaultRootWindow(gGui->video_display), 
		     hint.x, hint.y, gVw->visible_width, gVw->visible_height, 
		     border_width, gVw->depth, InputOutput,
		     gVw->visual,
		     CWBackPixel | CWBorderPixel | CWColormap, &attr);

    if(gGui->video_display == gGui->display)
      xitk_widget_list_set(gVw->wl, WIDGET_LIST_WINDOW, (void *) gGui->video_window);
    
    if(gGui->vo_port)
      xine_gui_send_vo_data(gGui->stream,
			    XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);
    
    if (gVw->xclasshint_fullscreen != NULL)
      XSetClassHint(gGui->video_display, gGui->video_window, gVw->xclasshint_fullscreen);

#ifndef HAVE_XINERAMA
    hint.x      = 0;
    hint.y      = 0;
    hint.width  = gVw->visible_width;
    hint.height = gVw->visible_height;
#endif
    hint.win_gravity = StaticGravity;
    hint.flags  = PPosition | PSize | PWinGravity;
    
    _set_window_title();
    
    XSetWMNormalHints (gGui->video_display, gGui->video_window, &hint);
        
    XSetWMHints(gGui->video_display, gGui->video_window, gVw->wm_hint);

    gVw->output_width    = hint.width;
    gVw->output_height   = hint.height;
    
    /*
     * wm, no borders please
     */
    prop = XInternAtom(gGui->video_display, "_MOTIF_WM_HINTS", False);
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0;
    XChangeProperty(gGui->video_display, gGui->video_window, prop, prop, 32,
		    PropModeReplace, (unsigned char *) &mwmhints,
		    PROP_MWM_HINTS_ELEMENTS);

  } 
  else {
       
#ifndef HAVE_XINERAMA
    hint.x           = 0;
    hint.y           = 0;
    hint.width       = gVw->win_width;
    hint.height      = gVw->win_height;
#endif
    hint.flags       = PPosition | PSize;
    
    /*
     * user sets window geom, move back to original location.
     */
    if(gVw->stream_resize_window == 0) {
      hint.x           = gVw->old_xwin;
      hint.y           = gVw->old_ywin;
    }
    
    gVw->output_width  = hint.width;
    gVw->output_height = hint.height;

    if (gGui->video_window) {

      if ((!(gVw->fullscreen_mode & WINDOWED_MODE)) || (gVw->visual != gGui->visual)) {
#ifdef HAVE_XF86VIDMODE
	/*
	 * toggling from fullscreen to window mode - time to switch back to
	 * the original modeline
	 */
	if(gVw->XF86_modelines_count > 1) {
	   double res_h, res_v;
	   
	   XF86VidModeSwitchToMode(gGui->video_display, XDefaultScreen(gGui->video_display), gVw->XF86_modelines[0]);
	   XF86VidModeSetViewPort(gGui->video_display, XDefaultScreen(gGui->video_display), 0, 0);

	   gGui->XF86VidMode_fullscreen = 0;
       
	   gVw->fullscreen_width  = gVw->XF86_modelines[0]->hdisplay;
	   gVw->fullscreen_height = gVw->XF86_modelines[0]->vdisplay;
	   
	   /* update pixel aspect */
	   res_h = (DisplayWidth  (gGui->video_display, gGui->video_screen)*1000 
		    / DisplayWidthMM (gGui->video_display, gGui->video_screen));
	   res_v = (DisplayHeight (gGui->video_display, gGui->video_screen)*1000
		    / DisplayHeightMM (gGui->video_display, gGui->video_screen));
  
	   gGui->pixel_aspect    = res_v / res_h;
#ifdef DEBUG
	   printf("pixel_aspect: %f\n", gGui->pixel_aspect);
#endif
	}
#endif

        if(gGui->video_display == gGui->display)
          xitk_unregister_event_handler(&gVw->old_widget_key);
	old_video_window = gGui->video_window;
      }
      else {
	
	/* Update window size hints with the new size */
	XSetNormalHints(gGui->video_display, gGui->video_window, &hint);
	
	XResizeWindow (gGui->video_display, gGui->video_window, 
		       gVw->win_width, gVw->win_height);
	
	XUnlockDisplay (gGui->video_display);
	
	return;	
      }
    }

    gVw->fullscreen_mode   = WINDOWED_MODE;
    gVw->visual            = gGui->visual;
    gVw->depth             = gGui->depth;
    gVw->colormap          = gGui->colormap;
      
    if(gGui->video_display != gGui->display)
      video_window_find_visual (&gVw->visual, &gVw->depth);

    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = gGui->black.pixel;
    attr.colormap	   = gVw->colormap;

    if(gVw->borderless)
      border_width = 0;
    else
      border_width = 4;

    gGui->video_window =
      XCreateWindow(gGui->video_display, DefaultRootWindow(gGui->video_display),
		    hint.x, hint.y, hint.width, hint.height, border_width, 
		    gVw->depth, InputOutput, gVw->visual,
		    CWBackPixel | CWBorderPixel | CWColormap, &attr);
    
    if(gGui->video_display == gGui->display)
      xitk_widget_list_set(gVw->wl, WIDGET_LIST_WINDOW, (void *) gGui->video_window);

    if(gGui->vo_port)
      xine_gui_send_vo_data(gGui->stream, XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);
    
    if(gVw->borderless) {
      if (gVw->xclasshint_borderless != NULL)
	XSetClassHint(gGui->video_display, gGui->video_window, gVw->xclasshint_borderless);
    }
    else {

      if(gVw->xclasshint != NULL)
      XSetClassHint(gGui->video_display, gGui->video_window, gVw->xclasshint);
    }

    _set_window_title();

    XSetWMNormalHints (gGui->video_display, gGui->video_window, &hint);

    XSetWMHints(gGui->video_display, gGui->video_window, gVw->wm_hint);

    if(gVw->borderless) {
      prop = XInternAtom(gGui->video_display, "_MOTIF_WM_HINTS", False);
      mwmhints.flags = MWM_HINTS_DECORATIONS;
      mwmhints.decorations = 0;
      XChangeProperty(gGui->video_display, gGui->video_window, prop, prop, 32,
		      PropModeReplace, (unsigned char *) &mwmhints,
		      PROP_MWM_HINTS_ELEMENTS);
    }
  }
  
  XSelectInput(gGui->video_display, gGui->video_window, INPUT_MOTION | KeymapStateMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap = gGui->icon;
    wm_hint->flags = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->video_display, gGui->video_window, wm_hint);
    XFree(wm_hint);
  }

  wm_delete_window = XInternAtom(gGui->video_display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(gGui->video_display, gGui->video_window, &wm_delete_window, 1);

  if(gVw->hide_on_start == 1) {
    gVw->hide_on_start = -1;
    gVw->show = 0;
  }
  else {
    /* Map window. */

    if((gGui->always_layer_above || 
	((!(gVw->fullscreen_mode & WINDOWED_MODE)) && is_layer_above())) && 
       !wm_not_ewmh_only()) {
      xitk_set_layer_above(gGui->video_window);
    }
        
    XRaiseWindow(gGui->video_display, gGui->video_window);
    XMapWindow(gGui->video_display, gGui->video_window);
    
    /* Wait for map. */
    do  {
      XMaskEvent(gGui->video_display, 
		 StructureNotifyMask, 
		 &xev) ;
    } while (xev.type != MapNotify || xev.xmap.event != gGui->video_window);
        
    if((gGui->always_layer_above || 
	((!(gVw->fullscreen_mode & WINDOWED_MODE)) && is_layer_above())) && 
       wm_not_ewmh_only()) {
      xitk_set_layer_above(gGui->video_window);
    }
    
    /* inform the window manager that we are fullscreen. This info musn't be set for xinerama-fullscreen,
       otherwise there are 2 different window size for one fullscreen mode ! (kwin doesn't accept this) */
    if( !(gVw->fullscreen_mode & WINDOWED_MODE)
     && !(gVw->fullscreen_mode & FULLSCR_XI_MODE)
     && wm_not_ewmh_only())
      xitk_set_ewmh_fullscreen(gGui->video_window);
    
  }
  
  XSync(gGui->video_display, False);

  if(gVw->gc != None) 
    XFreeGC(gGui->video_display, gVw->gc);

  gVw->gc = XCreateGC(gGui->video_display, gGui->video_window, 0L, &xgcv);
  if(gGui->video_display == gGui->display)
    xitk_widget_list_set(gVw->wl, WIDGET_LIST_GC, gVw->gc);
      
  if ((!(gVw->fullscreen_mode & WINDOWED_MODE))) {
    /* Waiting for visibility, avoid X error on some cases */

    try_to_set_input_focus(gGui->video_window);

#ifdef HAVE_XINERAMA
    if(gVw->xinerama)
      XMoveWindow (gGui->video_display, gGui->video_window, hint.x, hint.y);
    else
      XMoveWindow (gGui->video_display, gGui->video_window, 0, 0);
#else
    XMoveWindow (gGui->video_display, gGui->video_window, 0, 0);
#endif
  }

  /* Update WM_TRANSIENT_FOR hints on other windows for the new video_window */
  if ((gGui->panel_window != None) && (!gGui->use_root_window))
    XSetTransientForHint(gGui->video_display, gGui->panel_window,gGui->video_window);

  /* The old window should be destroyed now */
  if(old_video_window != None) {
    XDestroyWindow(gGui->video_display, old_video_window);
     
    if(gGui->cursor_grabbed)
       XGrabPointer(gGui->video_display, gGui->video_window, 1, 
		    None, GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
  }

  XUnlockDisplay (gGui->video_display);

  gVw->old_widget_key = gVw->widget_key;

  if(gGui->video_display == gGui->display)
    gVw->widget_key = xitk_register_event_handler("video_window", 
						gGui->video_window, 
						video_window_handle_event,
						NULL,
						gui_dndcallback,
						NULL, NULL);

  /* take care about window decoration/pos */
  {
    Window tmp_win;
    
    XLockDisplay (gGui->video_display);
    XTranslateCoordinates(gGui->video_display, gGui->video_window, DefaultRootWindow(gGui->video_display), 
			  0, 0, &gVw->xwin, &gVw->ywin, &tmp_win);
    XUnlockDisplay (gGui->video_display);
  }

}

static float get_default_mag(int video_width, int video_height) {
  if(gVw->zoom_small_stream && video_width < 300 && video_height < 300 )
    return 2.0;
  else
    return 1.0;
}

/*
 *
 */
void video_window_dest_size_cb (void *data,
				int video_width, int video_height,
				double video_pixel_aspect,
				int *dest_width, int *dest_height,
				double *dest_pixel_aspect)  {
  
  gVw->frame_width = video_width;
  gVw->frame_height = video_height;

  /* correct size with video_pixel_aspect */
  if (video_pixel_aspect >= gGui->pixel_aspect)
    video_width  = video_width * video_pixel_aspect / gGui->pixel_aspect + .5;
  else
    video_height = video_height * gGui->pixel_aspect / video_pixel_aspect + .5;

  if(gVw->stream_resize_window && (gVw->fullscreen_mode & WINDOWED_MODE)) {

    if(gVw->video_width != video_width || gVw->video_height != video_height) {
      
      if((video_width > 0) && (video_height > 0)) {
        double mag = get_default_mag( video_width, video_height );

        /* FIXME: this is supposed to give the same results as if a 
         * video_window_set_mag(mag) was called. Since video_window_adapt_size()
         * check several other details (like border, xinerama, etc) this
         * may produce wrong values in some cases. (?)
         */  
        *dest_width  = (int) ((float) video_width * mag);
        *dest_height = (int) ((float) video_height * mag);
        *dest_pixel_aspect = gGui->pixel_aspect;
        return;
      }
    }
  }
  
  if (!(gVw->fullscreen_mode & WINDOWED_MODE)) {
    *dest_width  = gVw->visible_width;
    *dest_height = gVw->visible_height;
    *dest_pixel_aspect = gVw->visible_aspect;
  } else {
    *dest_width  = gVw->output_width;
    *dest_height = gVw->output_height;
    *dest_pixel_aspect = gGui->pixel_aspect;
  }
}

/*
 *
 */
void video_window_frame_output_cb (void *data,
				   int video_width, int video_height,
				   double video_pixel_aspect,
				   int *dest_x, int *dest_y, 
				   int *dest_width, int *dest_height,
				   double *dest_pixel_aspect,
				   int *win_x, int *win_y) {

  gVw->frame_width = video_width;
  gVw->frame_height = video_height;

  /* correct size with video_pixel_aspect */
  if (video_pixel_aspect >= gGui->pixel_aspect)
    video_width  = video_width * video_pixel_aspect / gGui->pixel_aspect + .5;
  else
    video_height = video_height * gGui->pixel_aspect / video_pixel_aspect + .5;

  /* Please do NOT remove, support will be added soon! */
#if 0
  double jitter;
  gVw->video_duration = video_duration;
  gVw->video_average  = 0.5 * gVw->video_average + 0.5 video_duration;
  jitter = ABS (video_duration - gVw->video_average) / gVw->video_average;
  if (jitter > EST_MAX_JITTER) {
    if (gVw->duration_valid > -EST_KEEP_VALID)
      gVw->duration_valid--;
  } else {
    if (gVw->duration_valid < EST_KEEP_VALID)
      gVw->duration_valid++;
    if (ABS (video_duration - gVw->use_duration) / video_duration > EST_MAX_DIFF)
      gVw->use_duration = video_duration;
  }
#endif

  if(!gVw->stream_resize_window) {
    gVw->video_width  = video_width;
    gVw->video_height = video_height;
  }
  else {
    if(gVw->video_width != video_width || gVw->video_height != video_height ) {
      
      gVw->video_width  = video_width;
      gVw->video_height = video_height;
      
      if(video_width > 0 && video_height > 0)
	video_window_set_mag(get_default_mag( video_width, video_height ));
    }
  }

  *dest_x = 0;
  *dest_y = 0;

  if (!(gVw->fullscreen_mode & WINDOWED_MODE)) {
    *dest_width  = gVw->visible_width;
    *dest_height = gVw->visible_height;
    *dest_pixel_aspect = gVw->visible_aspect;
    /* TODO: check video size/fps/ar if tv mode and call video_window_adapt_size if necessary */
  } else {
    *dest_width  = gVw->output_width;
    *dest_height = gVw->output_height;
    *dest_pixel_aspect = gGui->pixel_aspect;
  }

  *win_x = (gVw->xwin < 0) ? 0 : gVw->xwin;
  *win_y = (gVw->ywin < 0) ? 0 : gVw->ywin;

}

/*
 *
 */
void video_window_set_fullscreen_mode (int req_fullscreen) {
  
  if(!(gVw->fullscreen_mode & req_fullscreen)) {

#ifdef HAVE_XINERAMA
    if((req_fullscreen & FULLSCR_XI_MODE) && (!gVw->xinerama)) {
      if(gVw->fullscreen_mode & FULLSCR_MODE)
	gVw->fullscreen_req = WINDOWED_MODE;
      else
	gVw->fullscreen_req = FULLSCR_MODE;
    }
    else
#endif
      gVw->fullscreen_req = req_fullscreen;

  }
  else {

    if((req_fullscreen & FULLSCR_MODE) && (gVw->fullscreen_mode & FULLSCR_MODE))
      gVw->fullscreen_req = WINDOWED_MODE;
#ifdef HAVE_XINERAMA
    else if((req_fullscreen & FULLSCR_XI_MODE) && (gVw->fullscreen_mode & FULLSCR_XI_MODE))
      gVw->fullscreen_req = WINDOWED_MODE;
#endif

  }

  video_window_adapt_size ();
}

/*
 *
 */
int video_window_get_fullscreen_mode (void) {
  return gVw->fullscreen_mode;
}

#if 0
/*
 * set/reset xine in xinerama fullscreen
 * ie: try to expend display on further screens
 */
void video_window_set_xinerama_fullscreen_mode(int req_fullscreen) {
  gVw->fullscreen_req = req_fullscreen;

  video_window_adapt_size ();
}

/*
 *
 */
int video_window_get_xinerama_fullscreen_mode(void) {
  return gVw->fullscreen_mode;
}
#endif

/*
 * Set cursor
 */
void video_window_set_cursor(int cursor) {

  if(cursor) {
    gVw->current_cursor = cursor;
    
    if(gVw->cursor_visible) {
      gVw->cursor_timer = 0;
      XLockDisplay (gGui->video_display);
      XDefineCursor(gGui->video_display, gGui->video_window, 
		    gVw->cursor[gVw->current_cursor]);
      XSync(gGui->video_display, False);
      XUnlockDisplay (gGui->video_display);
    }
  }

}

/*
 * hide/show cursor in video window
 */
void video_window_set_cursor_visibility(int show_cursor) {

  if(gGui->use_root_window)
    return;
  
  gVw->cursor_visible = show_cursor;

  if(show_cursor)
    gVw->cursor_timer = 0;

  XLockDisplay (gGui->video_display);
  XDefineCursor(gGui->video_display, gGui->video_window, 
		gVw->cursor[show_cursor ? gVw->current_cursor : show_cursor]);
  XSync(gGui->video_display, False);
  XUnlockDisplay (gGui->video_display);
}

/* 
 * Get cursor visiblity (boolean) 
 */
int video_window_is_cursor_visible(void) {
  return gVw->cursor_visible;
}

int video_window_get_cursor_timer(void) {
  return gVw->cursor_timer;
}

void video_window_set_cursor_timer(int timer) {
  gVw->cursor_timer = timer;
}

/* 
 * hide/show video window 
 */
void video_window_set_visibility(int show_window) {

  if(gGui->use_root_window)
    return;

  xine_gui_send_vo_data(gGui->stream, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *)show_window);
  
  gVw->show = show_window;

  XLockDisplay(gGui->video_display);
  
  if(gVw->show == 1) {

    if((gGui->always_layer_above || 
	(((!(gVw->fullscreen_mode & WINDOWED_MODE)) && is_layer_above()) && 
       (gVw->hide_on_start == 0))) && (!wm_not_ewmh_only())) {
      xitk_set_layer_above(gGui->video_window);
    }
    
    XRaiseWindow(gGui->video_display, gGui->video_window);
    XMapWindow(gGui->video_display, gGui->video_window);
    
    if((gGui->always_layer_above || 
	(((!(gVw->fullscreen_mode & WINDOWED_MODE)) && is_layer_above()) && 
       (gVw->hide_on_start == 0))) && (wm_not_ewmh_only())) {
      xitk_set_layer_above(gGui->video_window);
    }

  }
  else
    XUnmapWindow (gGui->video_display, gGui->video_window);
  
  XUnlockDisplay (gGui->video_display);
  
  /* User used '-H', now he want to show video window */
  if(gVw->hide_on_start == -1)
    gVw->hide_on_start = 0;

}

/*
 *
 */
int video_window_is_visible (void) {
  return gVw->show;
}

/*
 * check if screen_number is in the list
 */
static int screen_is_in_xinerama_fullscreen_list (const char *list, int screen_number) {
  const char *buffer;
  int         dummy;

  buffer = list;

  do {
    if((sscanf(buffer,"%d", &dummy) == 1) && (screen_number == dummy))
      return 1;
  } while((buffer = strchr(buffer,' ')) && (++buffer < (list + strlen(list))));

  return 0;
}

/*
 *
 */
static unsigned char bm_no_data[] = { 0,0,0,0, 0,0,0,0 };
void video_window_init (window_attributes_t *window_attribute, int hide_on_start) {
  Pixmap                bm_no;
  int i;
#ifdef HAVE_XINERAMA
  int                   screens;
  int                   dummy_a, dummy_b;
  XineramaScreenInfo   *screeninfo = NULL;
  const char           *screens_list;
#endif
#ifdef HAVE_XF86VIDMODE
  int                   dummy_query_event, dummy_query_error;
#endif

  gVw = (gVw_t *) xine_xmalloc(sizeof(gVw_t));
  
  if(gGui->video_display == gGui->display) {
    gVw->wl                 = xitk_widget_list_new();
    xitk_widget_list_set(gVw->wl, WIDGET_LIST_LIST, (xitk_list_new()));
  }
  
  gVw->fullscreen_req     = WINDOWED_MODE;
  gVw->fullscreen_mode    = WINDOWED_MODE;
  gGui->video_window      = None;
  gVw->show               = 1;
  gVw->widget_key         = 
    gVw->old_widget_key   = 0;
  gVw->gc		  = None;
  gVw->borderless         = window_attribute->borderless;
  gVw->have_xtest         = have_xtestextention();
  gVw->hide_on_start      = hide_on_start;

  gVw->depth	          = gGui->depth;
  gVw->visual		  = gGui->visual;
  gVw->colormap		  = gGui->colormap;
  /* Currently, there no plugin loaded so far, but that might change */
  video_window_select_visual ();
  
  if(gGui->video_display != gGui->display)
    video_window_find_visual (&gVw->visual, &gVw->depth);
  
  gVw->xwin               = window_attribute->x;
  gVw->ywin               = window_attribute->y;

  XLockDisplay (gGui->video_display);
  gVw->desktopWidth       = DisplayWidth(gGui->video_display, gGui->video_screen);
  gVw->desktopHeight      = DisplayHeight(gGui->video_display, gGui->video_screen);

#ifdef HAVE_XTESTEXTENSION
  gVw->kc_shift_l         = XKeysymToKeycode(gGui->video_display, XK_Shift_L);
#endif
  
  snprintf(gVw->window_title, sizeof(gVw->window_title), "%s", "xine");
  
  gettimeofday(&gVw->click_time, 0);

  gVw->using_xinerama     = 0;
#ifdef HAVE_XINERAMA
  gVw->xinerama		  = NULL;
  gVw->xinerama_cnt	  = 0;
  /* Spark
   * some Xinerama stuff
   * I want to figure out what fullscreen means for this setup
   */

  if ((XineramaQueryExtension (gGui->video_display, &dummy_a, &dummy_b)) 
      && (screeninfo = XineramaQueryScreens(gGui->video_display, &screens))) {
    /* Xinerama Detected */
#ifdef DEBUG
    printf ("videowin: display is using xinerama with %d screens\n", screens);
    printf ("videowin: going to assume we are using the first screen.\n");
    printf ("videowin: size of the first screen is %dx%d.\n", 
	     screeninfo[0].width, screeninfo[0].height);
#endif
    if (XineramaIsActive(gGui->video_display)) {
      gVw->using_xinerama = 1;
      gVw->fullscreen_width  = screeninfo[0].width;
      gVw->fullscreen_height = screeninfo[0].height;
      gVw->xinerama = screeninfo;
      gVw->xinerama_cnt = screens;

      screens_list = 
	xine_config_register_string (gGui->xine, "gui.xinerama_use_screens",
				     "0 1",
				     _("Screens to use in order to do a very fullscreen in xinerama mode. (example 0 2 3)"),
				     _("Example, if you want the display to expand on screen 0, 2 and 3, enter 0 2 3"),
				     CONFIG_LEVEL_EXP,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);

      if((sscanf(screens_list,"%d",&dummy_a) == 1) && (dummy_a >= 0) && (dummy_a < screens)) {

        /* try to calculate the best maximum size for xinerama fullscreen */
        gVw->xinerama_fullscreen_x = screeninfo[dummy_a].x_org;
        gVw->xinerama_fullscreen_y = screeninfo[dummy_a].y_org;
        gVw->xinerama_fullscreen_width = screeninfo[dummy_a].width;
        gVw->xinerama_fullscreen_height = screeninfo[dummy_a].height;

        i = dummy_a;
        while(i < screens) {
	  
          if(screen_is_in_xinerama_fullscreen_list(screens_list, i)) {
            if(screeninfo[i].x_org < gVw->xinerama_fullscreen_x)
	      gVw->xinerama_fullscreen_x = screeninfo[i].x_org;
            if(screeninfo[i].y_org < gVw->xinerama_fullscreen_y)
	      gVw->xinerama_fullscreen_y = screeninfo[i].y_org;
          }
	  
          i++;
        }
	
        i = dummy_a;
        while(i < screens) {

          if(screen_is_in_xinerama_fullscreen_list(screens_list, i)) {
            if((screeninfo[i].width + screeninfo[i].x_org) > 
	       (gVw->xinerama_fullscreen_x + gVw->xinerama_fullscreen_width)) {
	      gVw->xinerama_fullscreen_width = 
		screeninfo[i].width + screeninfo[i].x_org - gVw->xinerama_fullscreen_x;
	    }

            if((screeninfo[i].height + screeninfo[i].x_org) > 
	       (gVw->xinerama_fullscreen_y + gVw->xinerama_fullscreen_height)) {
	      gVw->xinerama_fullscreen_height = 
		screeninfo[i].height + screeninfo[i].y_org - gVw->xinerama_fullscreen_y;
	    }
          }

          i++;
        }
      } else {
        /* we can't find screens to use, so we use screen 0 */
        gVw->xinerama_fullscreen_x      = screeninfo[0].x_org;
        gVw->xinerama_fullscreen_y      = screeninfo[0].y_org;
        gVw->xinerama_fullscreen_width  = screeninfo[0].width;
        gVw->xinerama_fullscreen_height = screeninfo[0].height;
      }

      dummy_a = xine_config_register_num(gGui->xine,
					 "gui.xinerama_fullscreen_x",
					 -8192,
					 _("x coordinate for xinerama fullscreen (-8192 = autodetect)"),
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_EXP,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
      if(dummy_a > -8192)
	gVw->xinerama_fullscreen_x = dummy_a;

      dummy_a = xine_config_register_num(gGui->xine,
					 "gui.xinerama_fullscreen_y",
					 -8192,
					 _("y coordinate for xinerama fullscreen (-8192 = autodetect)"),
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_EXP,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
      if(dummy_a > -8192) 
	gVw->xinerama_fullscreen_y = dummy_a;

      dummy_a = xine_config_register_num(gGui->xine,
					 "gui.xinerama_fullscreen_width",
					 -8192,
					 _("width for xinerama fullscreen (-8192 = autodetect)"),
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_EXP,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
      if(dummy_a > -8192) 
	gVw->xinerama_fullscreen_width = dummy_a;
      
      dummy_a = xine_config_register_num(gGui->xine,
					 "gui.xinerama_fullscreen_height",
					 -8192,
					 _("height for xinerama fullscreen (-8192 = autodetect)"),
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_EXP,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
      if(dummy_a > -8192) 
	gVw->xinerama_fullscreen_height = dummy_a;
      
#ifdef DEBUG
      printf("videowin: Xinerama fullscreen parameters: X_origin=%d Y_origin=%d Width=%d Height=%d\n",gVw->xinerama_fullscreen_x,gVw->xinerama_fullscreen_y,gVw->xinerama_fullscreen_width,gVw->xinerama_fullscreen_height);
#endif
    } 
    else {
      gVw->fullscreen_width           = DisplayWidth  (gGui->video_display, gGui->video_screen);
      gVw->fullscreen_height          = DisplayHeight (gGui->video_display, gGui->video_screen);
      gVw->xinerama_fullscreen_x      = 0;
      gVw->xinerama_fullscreen_y      = 0;
      gVw->xinerama_fullscreen_width  = gVw->fullscreen_width;
      gVw->xinerama_fullscreen_height = gVw->fullscreen_height;
    }

  } else 
#endif
  {
    /* no Xinerama */
    if(gGui->verbosity) 
      printf ("Display is not using Xinerama.\n");
    gVw->fullscreen_width  = DisplayWidth (gGui->video_display, gGui->video_screen);
    gVw->fullscreen_height = DisplayHeight (gGui->video_display, gGui->video_screen);
  }
  gVw->visible_width  = gVw->fullscreen_width;
  gVw->visible_height = gVw->fullscreen_height;

  /* create xclass hint for video window */

  if ((gVw->xclasshint = XAllocClassHint()) != NULL) {
    gVw->xclasshint->res_name  = _("xine Video Window");
    gVw->xclasshint->res_class = "xine";
  }
  if ((gVw->xclasshint_fullscreen = XAllocClassHint()) != NULL) {
    gVw->xclasshint_fullscreen->res_name  = _("xine Video Fullscreen Window");
    gVw->xclasshint_fullscreen->res_class = "xine";
  }
  if ((gVw->xclasshint_borderless = XAllocClassHint()) != NULL) {
    gVw->xclasshint_borderless->res_name  = _("xine Video Borderless Window");
    gVw->xclasshint_borderless->res_class = "xine";
  }

  /* 
   * create cursors
   */

  bm_no                     = XCreateBitmapFromData(gGui->video_display, 
						    DefaultRootWindow(gGui->video_display), 
						    bm_no_data, 8, 8);
  gVw->cursor[0]            = XCreatePixmapCursor(gGui->video_display, bm_no, bm_no,
						  &gGui->black, &gGui->black, 0, 0);
  gVw->cursor[CURSOR_ARROW] = XCreateFontCursor(gGui->video_display, XC_left_ptr);
  gVw->cursor[CURSOR_HAND]  = XCreateFontCursor(gGui->video_display, XC_hand2);
  gVw->current_cursor       = CURSOR_ARROW;
  gVw->cursor_timer         = 0;

  /*
   * wm hints
   */

  gVw->wm_hint = XAllocWMHints();
  if (!gVw->wm_hint) {
    printf (_("XAllocWMHints() failed\n"));
    exit (1);
  }

  gVw->wm_hint->input         = True;
  gVw->wm_hint->initial_state = NormalState;
  gVw->wm_hint->icon_pixmap   = gGui->icon;
  gVw->wm_hint->flags         = InputHint | StateHint | IconPixmapHint;

  XUnlockDisplay (gGui->video_display);
      
  gVw->stream_resize_window = 
    xine_config_register_bool(gGui->xine, 
			      "gui.stream_resize_window", 
			      1,
			      _("New stream sizes resize output window"),
			      CONFIG_NO_HELP,
			      CONFIG_LEVEL_ADV,
			      _video_window_resize_cb,
			      CONFIG_NO_DATA);
  
  gVw->zoom_small_stream = 
    xine_config_register_bool(gGui->xine,
			      "gui.zoom_small_stream", 
			      0,
			      _("Double size for small streams (require stream_resize_window)"),
			      CONFIG_NO_HELP,
			      CONFIG_LEVEL_ADV,
			      _video_window_zoom_small_cb,
			      CONFIG_NO_DATA);
  
  if((window_attribute->width > 0) && (window_attribute->height > 0)) {
    gVw->video_width  = window_attribute->width;
    gVw->video_height = window_attribute->height;
    /* 
     * Force to keep window size.
     * I don't update the config file, i think this window geometry
     * user defined can be temporary.
     */
    gVw->stream_resize_window = 0;
  }
  else {
    gVw->video_width  = 768;
    gVw->video_height = 480;
  }

#ifdef HAVE_XF86VIDMODE
  if(xine_config_register_bool(gGui->xine, "gui.use_xvidext", 
			       0,
			       _("use XVidModeExtension when switching to fullscreen"),
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_EXP,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA)) {
    /* 
     * without the "stream resizes window" behavior, the XVidMode support
     * won't work correctly, so we force it for each session the user wants
     * to have XVideMode on...
     * 
     * FIXME: maybe display a warning message or so?!
     */
    gVw->stream_resize_window = 1;
    
    XLockDisplay (gGui->video_display);
    
    if(XF86VidModeQueryExtension(gGui->video_display, &dummy_query_event, &dummy_query_error)) {
      XF86VidModeModeInfo* XF86_modelines_swap;
      int                  mode, major, minor, sort_x, sort_y;
      
      XF86VidModeQueryVersion(gGui->video_display, &major, &minor);
      printf(_("XF86VidMode Extension (%d.%d) detected, trying to use it.\n"), major, minor);
      
      if(XF86VidModeGetAllModeLines(gGui->video_display, 
				    XDefaultScreen(gGui->video_display),
				    &(gVw->XF86_modelines_count), &(gVw->XF86_modelines))) {
	printf(_("XF86VidMode Extension: %d modelines found.\n"), gVw->XF86_modelines_count);
	
	/* first, kick off unsupported modes */
	for(mode = 1; mode < gVw->XF86_modelines_count; mode++) {
	  
	  if(!XF86VidModeValidateModeLine(gGui->video_display, gGui->video_screen, 
					  gVw->XF86_modelines[mode])) {
	    int wrong_mode;
	    
	    printf(_("XF86VidModeModeLine %dx%d isn't valid: discarded.\n"), 
		   gVw->XF86_modelines[mode]->hdisplay,
		   gVw->XF86_modelines[mode]->vdisplay);
	    
	    for(wrong_mode = mode; wrong_mode < gVw->XF86_modelines_count; wrong_mode++)
	      gVw->XF86_modelines[wrong_mode] = gVw->XF86_modelines[wrong_mode + 1];
	    
	    gVw->XF86_modelines[wrong_mode] = NULL;
	    gVw->XF86_modelines_count--;
	    mode--;
	  }
	}
	
	/*
	 * sorting modelines, skipping first entry because it is the current
	 * modeline in use - this is important so we know to which modeline
	 * we have to switch to when toggling fullscreen mode.
	 */
	for(sort_x = 1; sort_x < gVw->XF86_modelines_count; sort_x++) {

	  for(sort_y = sort_x+1; sort_y < gVw->XF86_modelines_count; sort_y++) {

	    if(gVw->XF86_modelines[sort_x]->hdisplay > gVw->XF86_modelines[sort_y]->hdisplay) {
	      XF86_modelines_swap = gVw->XF86_modelines[sort_y];
	      gVw->XF86_modelines[sort_y] = gVw->XF86_modelines[sort_x];
	      gVw->XF86_modelines[sort_x] = XF86_modelines_swap;
	    }
	  }
	}
      } else {
	gVw->XF86_modelines_count = 0;
	printf(_("XF86VidMode Extension: could not get list of available modelines. Failed.\n"));
      }
    } else {
      printf(_("XF86VidMode Extension: initialization failed, not using it.\n"));
    }
    XUnlockDisplay (gGui->video_display);
  }
  else
    gVw->XF86_modelines_count = 0;
  
#endif

  video_window_set_mag(1.0);

  /*
   * for plugins that aren't really bind to the window, it's necessary that the
   * gVw->xwin and gVw->ywin variables are set to *real* values, otherwise the
   * overlay will be displayed somewhere outside the window
   */
  if(gGui->video_window) {
    Window tmp_win;
    
    XLockDisplay (gGui->video_display);
    if((window_attribute->x > -8192) && (window_attribute->y > -8192)) {
      gVw->xwin = gVw->old_xwin = window_attribute->x;
      gVw->ywin = gVw->old_ywin = window_attribute->y;
      
      XMoveResizeWindow (gGui->video_display, gGui->video_window, 
			 gVw->xwin, gVw->ywin, gVw->video_width, gVw->video_height);
  
    } 
    else {
      
      XTranslateCoordinates(gGui->video_display, gGui->video_window, DefaultRootWindow(gGui->video_display), 
			    0, 0, &gVw->xwin, &gVw->ywin, &tmp_win);
    }
    XUnlockDisplay (gGui->video_display);

  }
  
  if( gGui->video_display != gGui->display ) {
    gVw->second_display_running = 1;
    pthread_create(&gVw->second_display_thread, NULL, second_display_loop, NULL);
  }

}


/*
 * Necessary cleanup
 */
void video_window_exit (void) {

#ifdef HAVE_XF86VIDMODE
  /* Restore original VidMode */
  if(gGui->XF86VidMode_fullscreen) {
    XLockDisplay(gGui->video_display);
    XF86VidModeSwitchToMode(gGui->video_display, XDefaultScreen(gGui->video_display), gVw->XF86_modelines[0]);
    XF86VidModeSetViewPort(gGui->video_display, XDefaultScreen(gGui->video_display), 0, 0);
    XUnlockDisplay(gGui->video_display);
  }
#endif

  gVw->second_display_running = 0;
  if(gGui->use_root_window || gGui->video_display != gGui->display) {
    XExposeEvent event;
    
    XLockDisplay(gGui->video_display);
    XClearWindow(gGui->video_display, gGui->video_window);
    event.type       = Expose;
    event.send_event = True;
    event.display    = gGui->video_display;
    event.window     = gGui->video_window;
    event.x          = 0;
    event.y          = 0;
    event.width      = gVw->video_width;
    event.height     = gVw->video_height;
    XSendEvent(gGui->video_display, gGui->video_window, False, Expose, (XEvent *) &event);
    XUnlockDisplay(gGui->video_display);
  }
    
  if(gGui->video_display == gGui->display)
    xitk_unregister_event_handler(&gVw->widget_key);
  else
    pthread_join(gVw->second_display_thread, NULL);
}


/*
 * Translate screen coordinates to video coordinates
 */
static int video_window_translate_point(int gui_x, int gui_y,
					int *video_x, int *video_y) {
  x11_rectangle_t rect;
  int             xwin, ywin;
  unsigned int    wwin, hwin, bwin, dwin;
  float           xf,yf;
  float           scale, width_scale, height_scale,aspect;
  Window          rootwin;

  rect.x = gui_x;
  rect.y = gui_y;
  rect.w = 0;
  rect.h = 0;

  if (xine_gui_send_vo_data(gGui->stream, 
			    XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect) != -1) {
    /* driver implements gui->video coordinate space translation, use it */
    *video_x = rect.x;
    *video_y = rect.y;
    return 1;
  }

  /* Driver cannot convert gui->video space, fall back to old code... */

  XLockDisplay(gGui->video_display);
  if(XGetGeometry(gGui->video_display, gGui->video_window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) == BadDrawable) {
    XUnlockDisplay(gGui->video_display);
    return 0;
  }
  XUnlockDisplay(gGui->video_display);
  
  /* Scale co-ordinate to image dimensions. */
  height_scale = (float)gVw->video_height / (float)hwin;
  width_scale  = (float)gVw->video_width / (float)wwin;
  aspect       = (float)gVw->video_width / (float)gVw->video_height;
  if (((float)wwin / (float)hwin) < aspect) {
    scale    = width_scale;
    xf       = (float)gui_x * scale;
    yf       = (float)gui_y * scale;
    /* wwin=wwin * scale; */
    hwin     = hwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x = xf * 1.25 / aspect;
    *video_y = yf - ((hwin - gVw->video_height) / 2);
    /* printf("wscale:a=%f, s=%f, x=%d, y=%d\n",aspect, scale,*video_x,*video_y);  */
  } else {
    scale    = height_scale;
    xf       = (float)gui_x * scale;
    yf       = (float)gui_y * scale;
    wwin     = wwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x = (xf - ((wwin - gVw->video_width) /2)) * 1.25 / aspect;
    *video_y = yf;
    /* printf("hscale:a=%f s=%f x=%d, y=%d\n",aspect,scale,*video_x,*video_y);  */
  }

  return 1;
}

/*
 * Set/Get magnification.
 */
void video_window_set_mag(float mag) {
  
  if((!(gVw->fullscreen_mode & WINDOWED_MODE))
#ifdef HAVE_XF86VIDMODE
     && !(gVw->XF86_modelines_count > 1)
#endif
     )
    return;
  
  gVw->mag        = mag;
  gVw->win_width  = (int) ((float) gVw->video_width) * gVw->mag;
  gVw->win_height = (int) ((float) gVw->video_height) * gVw->mag;
  
  video_window_adapt_size ();
}

float video_window_get_mag (void) {
  
  /* compute current mag */
  gVw->mag = (((float) gVw->output_width / (float) gVw->video_width ) + 
	      ((float) gVw->output_height / (float) gVw->video_height )) * .5;
  return gVw->mag;
}

/*
 * Change displayed logo, if selected skin want to customize it.
 */
void video_window_update_logo(void) {
  xine_cfg_entry_t  cfg_entry;
  char                *skin_logo;
  int                  cfg_err_result;
  
  cfg_err_result = xine_config_lookup_entry(gGui->xine, "gui.logo_mrl", &cfg_entry);
  skin_logo = xitk_skin_get_logo(gGui->skin_config);
  
  if(skin_logo) {
    
    if((cfg_err_result) && cfg_entry.str_value) {
      /* Old and new logo are same, don't reload */
      if(!strcmp(cfg_entry.str_value, skin_logo))
	goto __done;
    }
    
    config_update_string("gui.logo_mrl", skin_logo);
    goto __play_logo_now;
    
  }
  else { /* Skin don't use logo feature, set to xine's default */
    
    /* 
     * Back to default logo only on a skin 
     * change, not at the first skin loading.
     **/
    if((cfg_err_result) && (strcmp(cfg_entry.str_value, XINE_LOGO_MRL))) {
      config_update_string("gui.logo_mrl", XINE_LOGO_MRL);

    __play_logo_now:
      
      sleep(1);
      
      if(gGui->logo_mode) {
	if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
	  gGui->ignore_next = 1;
	  xine_stop(gGui->stream);
	  gGui->ignore_next = 0; 
	}
	if(gGui->display_logo) {
	  if((!xine_open(gGui->stream, gGui->logo_mrl)) 
	     || (!xine_play(gGui->stream, 0, 0))) {
	    gui_handle_xine_error(gGui->stream, (char *)gGui->logo_mrl);
	    goto __done;
	  }
	}
	gGui->logo_mode = 1;
      }
    }
  }
  
 __done:
  gGui->logo_has_changed--;
}
void video_window_change_skins(void) {
  gGui->logo_has_changed++;
}

/*
 *
 */
static void video_window_handle_event (XEvent *event, void *data) {

  switch(event->type) {

  case DestroyNotify:
    if(gGui->video_window == event->xany.window)
      gui_exit(NULL, NULL);
    break;

  case KeyPress:
    gui_handle_event(event, data);
    break;

  case MotionNotify: {
    XMotionEvent *mevent = (XMotionEvent *) event;
    int x, y;

    /* printf("Mouse event:mx=%d my=%d\n",mevent->x, mevent->y); */
    
    if(!gGui->cursor_visible) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }

    if (video_window_translate_point(mevent->x, mevent->y, &x, &y)) {
      xine_event_t event;
      xine_input_data_t input;

      event.type        = XINE_EVENT_INPUT_MOUSE_MOVE;
      event.stream      = gGui->stream;
      event.data        = &input;
      event.data_length = sizeof(input);
      gettimeofday(&event.tv, NULL);
      input.button      = 0; /*  No buttons, just motion. */
      input.x           = x;
      input.y           = y;
      xine_event_send(gGui->stream, &event);
    }
  }
  break;

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    int x, y;

    if(!gGui->cursor_visible) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }

    if (bevent->button == Button3 && gGui->display == gGui->video_display)
      video_window_menu(gVw->wl);
    else if (bevent->button == Button2)
      panel_toggle_visibility(NULL, NULL);
    else if (bevent->button == Button1) {
      struct timeval  old_click_time, tm_diff;
      long int        click_diff;
      
      timercpy(&gVw->click_time, &old_click_time);
      gettimeofday(&gVw->click_time, 0);
      
      if (video_window_translate_point(bevent->x, bevent->y, &x, &y)) {
	xine_input_data_t input;
	xine_event_t      event;
	
	event.type        = XINE_EVENT_INPUT_MOUSE_BUTTON;
	event.stream      = gGui->stream;
	event.data        = &input;
	event.data_length = sizeof(input);
	timercpy(&old_click_time, &event.tv);
	input.button      = 1;
	input.x           = x;
	input.y           = y;
	xine_event_send(gGui->stream, &event);
      }

      timersub(&gVw->click_time, &old_click_time, &tm_diff);
      click_diff = (tm_diff.tv_sec * 1000) + (tm_diff.tv_usec / 1000.0);
      
      if(click_diff < (xitk_get_timer_dbl_click())) {
	gui_execute_action_id(ACTID_TOGGLE_FULLSCREEN);
	gVw->click_time.tv_sec -= (xitk_get_timer_dbl_click() / 1000.0);
	gVw->click_time.tv_usec -= (xitk_get_timer_dbl_click() * 1000.0);
      }
      
    }
  }
  break;

  case ButtonRelease:
    gui_handle_event(event, data);
    break;

  case Expose: {
    XExposeEvent * xev = (XExposeEvent *) event;

    if (xev->count == 0) {

      if(event->xany.window == gGui->video_window) {
	xine_gui_send_vo_data(gGui->stream, XINE_GUI_SEND_EXPOSE_EVENT, event);
      }
    }
  }
  break;

  case ConfigureNotify:
    if(event->xany.window == gGui->video_window) {
      XConfigureEvent *cev = (XConfigureEvent *) event;
      Window tmp_win;

      gVw->output_width  = cev->width;
      gVw->output_height = cev->height;

      if ((cev->x == 0) && (cev->y == 0)) {
        XLockDisplay(cev->display);
        XTranslateCoordinates(cev->display, cev->window, DefaultRootWindow(cev->display),
                              0, 0, &gVw->xwin, &gVw->ywin, &tmp_win);
        XUnlockDisplay(cev->display);
      }
      else {
        gVw->xwin = cev->x;
        gVw->ywin = cev->y;
      }
    }
    break;
    
  }

}

void video_window_reset_ssaver(void) {
  XLockDisplay(gGui->video_display);

#ifdef HAVE_XTESTEXTENSION
  if(gVw->have_xtest == True) {
    XTestFakeKeyEvent(gGui->video_display, gVw->kc_shift_l, True, CurrentTime);
    XTestFakeKeyEvent(gGui->video_display, gVw->kc_shift_l, False, CurrentTime);
    XSync(gGui->video_display, False);
  }
  else 
#endif
    {
      XResetScreenSaver(gGui->video_display);
    }

  XUnlockDisplay(gGui->video_display);
}

void video_window_get_frame_size(int *w, int *h) {
  if(w)
    *w = gVw->frame_width;
  if(h)
    *h = gVw->frame_height;
  if (!gVw->frame_width && !gVw->frame_height) {
    /* fall back to meta info */
    *w = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    *h = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
  }
}

void video_window_get_visible_size(int *w, int *h) {
  if(w)
    *w = gVw->visible_width;
  if(h)
    *h = gVw->visible_height;
}

void video_window_get_output_size(int *w, int *h) {
  if(w)
    *w = gVw->output_width;
  if(h)
    *h = gVw->output_height;
}

void video_window_set_mrl(char *mrl) {
  if(mrl && strlen(mrl)) {
    
    snprintf(gVw->window_title, sizeof(gVw->window_title), "%s: %s", "xine", mrl);
    
    XLockDisplay(gGui->video_display);
    _set_window_title();
    XUnlockDisplay(gGui->video_display);
  }
}
