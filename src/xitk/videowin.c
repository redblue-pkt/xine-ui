/* 
 * Copyright (C) 2000-2002 the xine project
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

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XTESTEXTENSION
#include <X11/extensions/XTest.h>
#endif

#include <xine.h>
#include <xine/xineutils.h>

#include "Imlib-light/Imlib.h"

#include "event.h"
#include "videowin.h"
#include "panel.h"
#include "actions.h"
#include "errors.h"
#include "i18n.h"
#include "xitk.h"

#define EST_KEEP_VALID  10	  /* #frames to allow for changing fps */
#define EST_MAX_JITTER  0.01	  /* maximum jitter to detect valid fps */
#define EST_MAX_DIFF    0.01      /* maximum diff to detect valid fps */
#define ABS(x) ((x)>0?(x):-(x))

extern gGui_t *gGui;

/* Video window private structure */
typedef struct {
  Cursor         cursor[2];       /* Cursor pointers                       */
  int            cursor_visible;
  Visual	*visual;	  /* Visual for video window               */
  Colormap	 colormap;	  /* Colormap for video window		   */
  XClassHint    *xclasshint;
  XClassHint    *xclasshint_fullscreen;
  XClassHint    *xclasshint_borderless;
  GC             gc;

  int            video_width;     /* size of currently displayed video     */
  int            video_height;
  double         video_duration;  /* frame duratrion in seconds */
  double         video_average;   /* average frame duration in seconds */
  double         use_duration;    /* duration used for tv mode selection */
  int            video_duration_valid; /* is use_duration trustable? */
  int            win_width;       /* size of non-fullscreen window         */
  int            win_height;
  int            output_width;    /* output video window width/height      */
  int            output_height;
  float          mag;

  int            stream_resize_window; /* Boolean, 1 if new stream resize output window */
  int            zoom_small_stream; /* Boolean, 1 to double size small streams */

  int            fullscreen_mode; /* 0: regular  1: fullscreen  2: TV mode */
  int            fullscreen_req;  /* ==1..2 => video_window will 
				   * switch to fullscreen mode             */
  int            fullscreen_width;
  int            fullscreen_height;

  int            visible_width;   /* Size of currently visible portion of screen */
  int            visible_height;  /* May differ from fullscreen_* e.g. for TV mode */
  double         visible_aspect;  /* Pixel ratio of currently vissible screen */

  int            using_xinerama;
#ifdef HAVE_XINERAMA
  XineramaScreenInfo *xinerama;   /* pointer to xinerama struct, or NULL */
  int            xinerama_cnt;    /* number of screens in Xinerama */
#endif

  int            xwin;            /* current X location */
  int            ywin;            /* current Y location */
  int            old_xwin;
  int            old_ywin;

  int            desktopWidth;    /* desktop width */
  int            desktopHeight;   /* desktop height */
  int            completion_type;
  int            depth;
  int            show;
  int            borderless;      /* borderless window (for windowed mode)? */

  Bool           have_xtest;
#ifdef HAVE_XTESTEXTENSION
  KeyCode        kc_shift_l;      /* Fake key to send */
#endif

  XWMHints      *wm_hint;

  xitk_register_key_t    widget_key;
  xitk_register_key_t    old_widget_key;

  int            completion_event;

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension stuff */
  XF86VidModeModeInfo** XF86_modelines;
  int                   XF86_modelines_count;
#endif

  int            hide_on_start; /* user use '-H' arg, don't map video window the first time */

} gVw_t;

static gVw_t    *gVw;

#ifndef XShmGetEventBase
extern int XShmGetEventBase(Display *);
#endif

static void video_window_handle_event (XEvent *event, void *data);
static void video_window_adapt_size (void);

static void _video_window_resize_cb(void *data, xine_cfg_entry_t *cfg) {
  gVw->stream_resize_window = cfg->num_value;
}

static void _video_window_zoom_small_cb(void *data, xine_cfg_entry_t *cfg) {
  gVw->zoom_small_stream = cfg->num_value;
}


static Bool have_xtestextention(void) {  
#ifdef HAVE_XTESTEXTENSION
  int dummy1, dummy2, dummy3, dummy4;
  
  return (XTestQueryExtension(gGui->display, &dummy1, &dummy2, &dummy3, &dummy4));
#endif
  return False;
}

/*
 * Let the video driver override the selected visual
 */
void video_window_select_visual (void) {
  XVisualInfo *vinfo = (XVisualInfo *) -1;

  XLockDisplay (gGui->display);
  if (gGui->vo_driver) {

    xine_gui_send_vo_data(gGui->xine, XINE_GUI_SEND_SELECT_VISUAL, &vinfo);

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
      gui_init_imlib (gGui->visual);
      video_window_adapt_size ();
    }
  }
  XUnlockDisplay (gGui->display);
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

  static char          *window_title;
  XSizeHints            hint;
  XWMHints             *wm_hint;
  XSetWindowAttributes  attr;
  Atom                  prop;
  Atom                  wm_delete_window;
  static Atom           XA_WIN_LAYER = None;
  MWMHints              mwmhints;
  XEvent                xev;
  XGCValues             xgcv;
  Window                old_video_window = None;
  long                  propvalue[1];
  int                   border_width;

/*  printf("window_adapt:vw=%d, vh=%d, dx=%d, dy=%d, dw=%d, dh=%d\n",
 *           video_width,
 *           video_height,
 *           dest_x,
 *           dest_y,
 *           dest_width,
 *           dest_height); 
 */

  xine_strdupa(window_title, _("xine video output"));

  XLockDisplay (gGui->display);

  if(gGui->use_root_window) { /* Using root window, but not really */

    gVw->output_width    = gVw->fullscreen_width;
    gVw->output_height   = gVw->fullscreen_height;
    
    if(gGui->video_window == None) {
      XGCValues   gcv;
      
      gVw->fullscreen_mode = 1;
      gVw->visual          = gGui->visual;
      gVw->depth           = gGui->depth;
      gVw->colormap        = gGui->colormap;
      
      attr.override_redirect = True;
      attr.background_pixel  = gGui->black.pixel;
      
      border_width = 0;
      
      gGui->video_window = XCreateWindow(gGui->display, DefaultRootWindow(gGui->display),
					 0, 0, gVw->fullscreen_width, gVw->fullscreen_height, 
					 border_width, 
					 CopyFromParent, CopyFromParent, CopyFromParent, 
					 CWBackPixel | CWOverrideRedirect, &attr);
      
      if(gGui->vo_driver)
	xine_gui_send_vo_data(gGui->xine, 
			      XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);

      XSelectInput(gGui->display, gGui->video_window, ExposureMask);
      
      XSetStandardProperties(gGui->display, gGui->video_window, 
			     window_title, window_title, None, NULL, 0, 0);
      
      
      gcv.foreground         = gGui->black.pixel;
      gcv.background         = gGui->black.pixel;
      gcv.graphics_exposures = False;
      gVw->gc = XCreateGC(gGui->display, gGui->video_window, 
			  GCForeground | GCBackground | GCGraphicsExposures, &gcv);

      hint.flags  = USSize | USPosition | PPosition | PSize;
      hint.x      = 0;
      hint.y      = 0;
      hint.width  = gVw->fullscreen_width;
      hint.height = gVw->fullscreen_height;
      XSetNormalHints(gGui->display, gGui->video_window, &hint);
      
      XClearWindow(gGui->display, gGui->video_window);
      
      XMapWindow(gGui->display, gGui->video_window);
      
      XLowerWindow(gGui->display, gGui->video_window);
      
      gVw->old_widget_key = gVw->widget_key;

      XUnlockDisplay (gGui->display);

      gVw->widget_key = xitk_register_event_handler("video_window", 
						    gGui->video_window, 
						    video_window_handle_event,
						    NULL,
						    gui_dndcallback,
						    NULL, NULL);
      return;
    }
    
    XUnlockDisplay (gGui->display);
    
    return;
  }

  switch (gVw->fullscreen_req) {
  case 0:
  case 1:
#warning FIXME NEWAPI
#if 0
    if(gGui->xine)
      xine_tvmode_switch2 (gGui->xine, 
			  0, gVw->video_width, gVw->video_height, gVw->video_duration);
#endif
    break;
  case 2:
#warning FIXME NEWAPI
#if 0
    if(gGui->xine)
      if (xine_tvmode_switch2 (gGui->xine,
			      1, gVw->video_width, gVw->video_height, gVw->video_duration) != 1)
	gVw->fullscreen_req = 0;
#endif
    break;
  default:
#warning FIXME NEWAPI
#if 0
    if(gGui->xine)
      xine_tvmode_switch2 (gGui->xine, 
			  0, gVw->video_width, gVw->video_height, gVw->video_duration);
#endif
    gVw->fullscreen_req = 0;
  }

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension
   * In case a fullscreen request is received or if already in fullscreen, the
   * appropriate modeline will be looked up and used.
   */
  if((gVw->fullscreen_req || gVw->fullscreen_mode) && gVw->XF86_modelines_count > 1) {
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
    if(gVw->fullscreen_mode && search >= gVw->XF86_modelines_count)
       search = 0;
       
    /* just switching to a different modeline if necessary */
    if(!(search >= gVw->XF86_modelines_count)) {
       if(XF86VidModeSwitchToMode(gGui->display, XDefaultScreen(gGui->display), gVw->XF86_modelines[search])) {
          double res_h, res_v;
	  
	  gGui->XF86VidMode_fullscreen = 1;	  
	  gVw->fullscreen_width        = gVw->XF86_modelines[search]->hdisplay;
	  gVw->fullscreen_height       = gVw->XF86_modelines[search]->vdisplay;
	  
	  /* update pixel aspect */
	  res_h = (DisplayWidth  (gGui->display, gGui->screen)*1000 
		   / DisplayWidthMM (gGui->display, gGui->screen));
	  res_v = (DisplayHeight (gGui->display, gGui->screen)*1000
		   / DisplayHeightMM (gGui->display, gGui->screen));
  
	  gGui->pixel_aspect    = res_h / res_v;
#ifdef DEBUG
	  printf("pixel_aspect: %f\n", gGui->pixel_aspect);
#endif

	  // TODO
	  /*
	   * just in case the mouse pointer is off the visible area, move it
	   * to the middle of the video window
	   */
	  XWarpPointer(gGui->display, None, gGui->video_window, 0, 0, 0, 0, gVw->fullscreen_width/2, gVw->fullscreen_height/2);
	  
	  XF86VidModeSetViewPort(gGui->display, XDefaultScreen(gGui->display), 0, 0);
          
	  /*
	   * if this is true, we are back at the original resolution, so there
	   * is no need to further worry about anything.
	   */
	  if(gVw->fullscreen_mode && search == 0)
	    gGui->XF86VidMode_fullscreen = 0;
       } else {
	  xine_error(_("XF86VidMode Extension: modeline switching failed.\n"));
       }
    }
  }
#endif

#ifdef HAVE_XINERAMA
  if (gVw->xinerama) {
    int i;
    int knowLocation = 0;
    
    /* someday this could also use the centre of the window as the
     * test point I guess.  Right now it's the upper-left.
     */
    if (gGui->video_window != None) {
      if (gVw->xwin >= 0 && gVw->ywin >= 0 &&
	  gVw->xwin < gVw->desktopWidth && gVw->ywin < gVw->desktopHeight) {
	knowLocation = 1;
      }
    }
    
    for (i = 0; i < gVw->xinerama_cnt; i++) {
      if (
	  (knowLocation == 1 &&
	   gVw->xwin >= gVw->xinerama[i].x_org &&
	   gVw->ywin >= gVw->xinerama[i].y_org &&
	   gVw->xwin <= gVw->xinerama[i].x_org+gVw->xinerama[i].width &&
	   gVw->ywin <= gVw->xinerama[i].y_org+gVw->xinerama[i].height) ||
	  (knowLocation == 0 &&
	   gVw->xinerama[i].screen_number == 
	   XScreenNumberOfScreen(XDefaultScreenOfDisplay(gGui->display)))) {
	hint.x = gVw->xinerama[i].x_org;
	hint.y = gVw->xinerama[i].y_org;
	if (gVw->fullscreen_req) {
	  hint.width  = gVw->xinerama[i].width;
	  hint.height = gVw->xinerama[i].height;
	  gVw->fullscreen_width = hint.width;
	  gVw->fullscreen_height = hint.height;
	} else {
	  hint.width  = gVw->video_width;
	  hint.height = gVw->video_height;
	}
	break;
      }
    }
  } else {
    hint.x = 0;
    hint.y = 0;
    if (gVw->fullscreen_req) {
      hint.width  = gVw->fullscreen_width;
      hint.height = gVw->fullscreen_height;
    } else {
      hint.width  = gVw->win_width;
      hint.height = gVw->win_height;
    }
  }
#else
  hint.x = 0;
  hint.y = 0;   /* for now -- could change later */
#endif
  
  gVw->visible_width  = gVw->fullscreen_width;
  gVw->visible_height = gVw->fullscreen_height;
  gVw->visible_aspect = gGui->pixel_aspect;

  if(gGui->xine) {
#warning FIXME NEWAPI
#if 0
    xine_tvmode_size2 (gGui->xine, &gVw->visible_width, &gVw->visible_height, &gVw->visible_aspect, NULL);
    xine_tvmode_size2 (gGui->xine, &hint.width, &hint.height, NULL, NULL);
#endif
  }
  
  if (gVw->fullscreen_req) {

    if (gGui->video_window) {
      int dummy;

      if (gVw->fullscreen_mode && gGui->visual == gVw->visual) {
//#ifdef HAVE_XF86VIDMODE
//	if(gVw->XF86_modelines_count > 1) {
	if (gVw->visible_width != gVw->output_width || gVw->visible_height != gVw->output_height) {
	   /*
	    * resizing the video window may be necessary if the modeline or tv mode has
	    * just been switched
	    */
	   XResizeWindow (gGui->display, gGui->video_window,
			  gVw->visible_width, gVw->visible_height);
	   gVw->output_width    = gVw->visible_width;
	   gVw->output_height   = gVw->visible_height;
	}
//#endif
        gVw->fullscreen_mode = gVw->fullscreen_req;
	XUnlockDisplay (gGui->display);
	
	return;
      }
      
      xitk_get_window_position(gGui->display, gGui->video_window,
      			       &gVw->old_xwin, &gVw->old_ywin, &dummy, &dummy);
      
      xitk_unregister_event_handler(&gVw->old_widget_key);
      old_video_window = gGui->video_window;
    }

    gVw->fullscreen_mode = gVw->fullscreen_req;
    gVw->visual   = gGui->visual;
    gVw->depth    = gGui->depth;
    gVw->colormap = gGui->colormap;

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = gGui->black.pixel;
    attr.colormap	   = gVw->colormap;

    border_width           = 0;

    gGui->video_window = 
      XCreateWindow (gGui->display, gGui->imlib_data->x.root, 
		     hint.x, hint.y, gVw->visible_width, gVw->visible_height, 
		     border_width, gVw->depth, InputOutput,
		     gVw->visual,
		     CWBackPixel | CWBorderPixel | CWColormap, &attr);

    if(gGui->vo_driver)
      xine_gui_send_vo_data(gGui->xine,
			    XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);
    
    if (gVw->xclasshint_fullscreen != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint_fullscreen);

#ifndef HAVE_XINERAMA
    hint.x      = 0;
    hint.y      = 0;
    hint.width  = gVw->visible_width;
    hint.height = gVw->visible_height;
#endif
    hint.win_gravity = StaticGravity;
    hint.flags  = PPosition | PSize | PWinGravity;
    
    XSetStandardProperties(gGui->display, gGui->video_window, 
 			   window_title, window_title, None, NULL, 0, 0);
    
    XSetWMNormalHints (gGui->display, gGui->video_window, &hint);
        
    XSetWMHints(gGui->display, gGui->video_window, gVw->wm_hint);

    gVw->output_width    = hint.width;
    gVw->output_height   = hint.height;
    
    /*
     * layer above most other things, like gnome panel
     * WIN_LAYER_ABOVE_DOCK  = 10
     *
     */
    if(gGui->layer_above) {
      if( XA_WIN_LAYER == None )
	XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);
      
      propvalue[0] = 10;
      XChangeProperty(gGui->display, gGui->video_window, XA_WIN_LAYER,
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		      1);
    }

    /*
     * wm, no borders please
     */
    
    prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", False);
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0;
    XChangeProperty(gGui->display, gGui->video_window, prop, prop, 32,
		    PropModeReplace, (unsigned char *) &mwmhints,
		    PROP_MWM_HINTS_ELEMENTS);

    XSetTransientForHint(gGui->display, gGui->video_window, None);
    XRaiseWindow(gGui->display, gGui->video_window);

  } else {

#ifndef HAVE_XINERAMA
    hint.x           = 0;
    hint.y           = 0;
    hint.width       = gVw->win_width;
    hint.height      = gVw->win_height;
#endif
    hint.flags       = PPosition | PSize;
    
    /*
     * user sets window geom, move back to original location.
     * This probably break something with Xinerama, but i can't
     * test it.
     */
    if((gVw->using_xinerama == 0) && 
       (gVw->stream_resize_window == 0)) {
      hint.x           = gVw->old_xwin;
      hint.y           = gVw->old_ywin;
    }
    
    gVw->output_width  = hint.width;
    gVw->output_height = hint.height;

    if (gGui->video_window) {

      if (gVw->fullscreen_mode || gVw->visual != gGui->visual) {
#ifdef HAVE_XF86VIDMODE
	/*
	 * toggling from fullscreen to window mode - time to switch back to
	 * the original modeline
	 */
	if(gVw->XF86_modelines_count > 1) {
	   double res_h, res_v;
	   
	   XF86VidModeSwitchToMode(gGui->display, XDefaultScreen(gGui->display), gVw->XF86_modelines[0]);
	   XF86VidModeSetViewPort(gGui->display, XDefaultScreen(gGui->display), 0, 0);

	   gGui->XF86VidMode_fullscreen = 0;
       
	   gVw->fullscreen_width  = gVw->XF86_modelines[0]->hdisplay;
	   gVw->fullscreen_height = gVw->XF86_modelines[0]->vdisplay;
	   
	   /* update pixel aspect */
	   res_h = (DisplayWidth  (gGui->display, gGui->screen)*1000 
		    / DisplayWidthMM (gGui->display, gGui->screen));
	   res_v = (DisplayHeight (gGui->display, gGui->screen)*1000
		    / DisplayHeightMM (gGui->display, gGui->screen));
  
	   gGui->pixel_aspect    = res_h / res_v;
#ifdef DEBUG
	   printf("pixel_aspect: %f\n", gGui->pixel_aspect);
#endif
	}
#endif

	xitk_unregister_event_handler(&gVw->old_widget_key);
	old_video_window = gGui->video_window;
      }
      else {
	
	/* Update window size hints with the new size */
	XSetNormalHints(gGui->display, gGui->video_window, &hint);
	
	XResizeWindow (gGui->display, gGui->video_window, 
		       gVw->win_width, gVw->win_height);
	
	XUnlockDisplay (gGui->display);
	
	return;
	
      }
    }

    gVw->fullscreen_mode = 0;
    gVw->visual   = gGui->visual;
    gVw->depth    = gGui->depth;
    gVw->colormap = gGui->colormap;

    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = gGui->black.pixel;
    attr.colormap	   = gVw->colormap;

    if(gVw->borderless)
      border_width = 0;
    else
      border_width = 4;

    gGui->video_window =
      XCreateWindow(gGui->display, gGui->imlib_data->x.root,
		    hint.x, hint.y, hint.width, hint.height, border_width, 
		    gVw->depth, InputOutput, gVw->visual,
		    CWBackPixel | CWBorderPixel | CWColormap, &attr);
    
    if(gGui->vo_driver)
      xine_gui_send_vo_data(gGui->xine, XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)gGui->video_window);
    
    if(gVw->borderless) {
      if (gVw->xclasshint_borderless != NULL)
	XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint_borderless);
    }
    else {
      if (gVw->xclasshint != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint);
    }

    XSetStandardProperties(gGui->display, gGui->video_window, 
			   window_title, window_title, None, NULL, 0, 0);

    XSetWMNormalHints (gGui->display, gGui->video_window, &hint);

    XSetWMHints(gGui->display, gGui->video_window, gVw->wm_hint);

    if(gVw->borderless) {
      prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", False);
      mwmhints.flags = MWM_HINTS_DECORATIONS;
      mwmhints.decorations = 0;
      XChangeProperty(gGui->display, gGui->video_window, prop, prop, 32,
		      PropModeReplace, (unsigned char *) &mwmhints,
		      PROP_MWM_HINTS_ELEMENTS);
    }
  }
  
  XSelectInput(gGui->display, gGui->video_window, INPUT_MOTION | KeymapStateMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap = gGui->icon;
    wm_hint->flags = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->display, gGui->video_window, wm_hint);
    XFree(wm_hint);
  }

  wm_delete_window = XInternAtom(gGui->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(gGui->display, gGui->video_window, &wm_delete_window, 1);

  if(gVw->hide_on_start == 1) {
    gVw->hide_on_start = -1;
    gVw->show = 0;
  }
  else {
    /* Map window. */
    
    XMapRaised(gGui->display, gGui->video_window);
    
    /* Wait for map. */
    
    do  {
      XMaskEvent(gGui->display, 
		 StructureNotifyMask, 
		 &xev) ;
      
    } while (xev.type != MapNotify || xev.xmap.event != gGui->video_window);
  }

  XSync(gGui->display, False);

  if (gVw->gc != None) 
    XFreeGC(gGui->display, gVw->gc);

  gVw->gc = XCreateGC(gGui->display, gGui->video_window, 0L, &xgcv);
  
  if (gVw->fullscreen_mode) {
    /* Waiting for visibility, avoid X error on some cases */
    while(!xitk_is_window_visible(gGui->display, gGui->video_window))
      xine_usec_sleep(5000);

    XSetInputFocus (gGui->display, 
		    gGui->video_window, RevertToNone, CurrentTime);
#ifdef HAVE_XINERAMA
    XMoveWindow (gGui->display, gGui->video_window, hint.x, hint.y);
#else
    XMoveWindow (gGui->display, gGui->video_window, 0, 0);
#endif
  }

  /* Update WM_TRANSIENT_FOR hints on other windows for the new video_window */
  if (gGui->panel_window != None) {
    XSetTransientForHint(gGui->display, gGui->panel_window,
			 gGui->video_window);
  }
  /* FIXME: update TransientForHint on control panel and mrl browser... */

  /* The old window should be destroyed now */
  if(old_video_window != None) {
    XDestroyWindow(gGui->display, old_video_window);
     
    if(gGui->cursor_grabbed)
       XGrabPointer(gGui->display, gGui->video_window, 1, 
		    None, GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
  }

  gVw->old_widget_key = gVw->widget_key;

  XUnlockDisplay (gGui->display);

  gVw->widget_key = xitk_register_event_handler("video_window", 
						gGui->video_window, 
						video_window_handle_event,
						NULL,
						gui_dndcallback,
						NULL, NULL);

  /* take care about window decoration/pos */
  {
    Window tmp_win;
    
    XLockDisplay (gGui->display);
    XTranslateCoordinates(gGui->display, gGui->video_window, DefaultRootWindow(gGui->display), 
			  0, 0, &gVw->xwin, &gVw->ywin, &tmp_win);
    XUnlockDisplay (gGui->display);
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
  
  /* correct size with video_pixel_aspect */
  if (video_pixel_aspect >= gGui->pixel_aspect)
    video_width  = video_width * video_pixel_aspect / gGui->pixel_aspect + .5;
  else
    video_height = video_height * gGui->pixel_aspect / video_pixel_aspect + .5;

  if(gVw->stream_resize_window && !gVw->fullscreen_mode) {

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
  
  if (gVw->fullscreen_mode) {
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

  if (gVw->fullscreen_mode) {
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
  gVw->fullscreen_req = req_fullscreen;

  video_window_adapt_size ();
}

/*
 *
 */
int video_window_get_fullscreen_mode (void) {
  return gVw->fullscreen_mode;
}

/*
 * hide/show cursor in video window
 */
void video_window_set_cursor_visibility(int show_cursor) {

  if(gGui->use_root_window)
    return;
  
  gVw->cursor_visible = show_cursor;

  XLockDisplay (gGui->display);
  XDefineCursor(gGui->display, gGui->video_window, 
		gVw->cursor[show_cursor]);
  XSync(gGui->display, False);
  XUnlockDisplay (gGui->display);
}

/* 
 * Get cursor visiblity (boolean) 
 */
int video_window_is_cursor_visible(void) {
  return gVw->cursor_visible;
}

/* 
 * hide/show video window 
 */
void video_window_set_visibility(int show_window) {
  static Atom  XA_WIN_LAYER = None;
  long         data[1];
  
  if(gGui->use_root_window)
    return;

  xine_gui_send_vo_data(gGui->xine, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (int *)show_window);
  
  gVw->show = show_window;

  if (gVw->show == 1) {
    XLockDisplay (gGui->display);

    /*
     * layer above most other things, like gnome panel
     * WIN_LAYER_ABOVE_DOCK  = 10
     *
     */

    if(gGui->layer_above && (gVw->hide_on_start == 0)) {
      if( XA_WIN_LAYER == None )
	XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);
      
      data[0] = 10;
      XChangeProperty(gGui->display, gGui->video_window, XA_WIN_LAYER,
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		      1);
    }
    
    XMapRaised (gGui->display, gGui->video_window);
    XUnlockDisplay (gGui->display);
  }
  else {
    XLockDisplay (gGui->display);
    XUnmapWindow (gGui->display, gGui->video_window);
    XUnlockDisplay (gGui->display);
  }
  
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
 *
 */
static unsigned char bm_no_data[] = { 0,0,0,0, 0,0,0,0 };
void video_window_init (window_attributes_t *window_attribute, int hide_on_start) {

  Pixmap                bm_no;
#ifdef HAVE_XINERAMA
  int                   screens;
  int                   dummy_a, dummy_b;
  XineramaScreenInfo   *screeninfo = NULL;
#endif
#ifdef HAVE_XF86VIDMODE
  int                   dummy_query_event, dummy_query_error;
#endif

  gVw = (gVw_t *) xine_xmalloc(sizeof(gVw_t));

  gVw->fullscreen_req     = 0;
  gVw->fullscreen_mode    = 0;
  gGui->video_window      = None;
  gVw->show               = 1;
  gVw->widget_key         = 
    gVw->old_widget_key   = 0;
  gVw->gc		  = None;
  gVw->borderless         = window_attribute->borderless;
  gVw->have_xtest         = have_xtestextention();
  gVw->hide_on_start      = hide_on_start;

  XLockDisplay (gGui->display);

  gVw->depth	          = gGui->depth;
  gVw->visual		  = gGui->visual;
  gVw->colormap		  = gGui->colormap;
  /* Currently, there no plugin loaded so far, but that might change */
  video_window_select_visual ();
  gVw->xwin               = window_attribute->x;
  gVw->ywin               = window_attribute->y;
  gVw->desktopWidth       = DisplayWidth(gGui->display, gGui->screen);
  gVw->desktopHeight      = DisplayHeight(gGui->display, gGui->screen);

#ifdef HAVE_XTESTEXTENSION
  gVw->kc_shift_l         = XKeysymToKeycode(gGui->display, XK_Shift_L);
#endif

  gVw->using_xinerama     = 0;
#ifdef HAVE_XINERAMA
  gVw->xinerama		  = NULL;
  gVw->xinerama_cnt	  = 0;
  /* Spark
   * some Xinerama stuff
   * I want to figure out what fullscreen means for this setup
   */

  if ((XineramaQueryExtension (gGui->display, &dummy_a, &dummy_b)) 
      && (screeninfo = XineramaQueryScreens(gGui->display, &screens))) {
    /* Xinerama Detected */
#ifdef DEBUG
    printf ("videowin: display is using xinerama with %d screens\n", screens);
    printf ("videowin: going to assume we are using the first screen.\n");
    printf ("videowin: size of the first screen is %dx%d.\n", 
	     screeninfo[0].width, screeninfo[0].height);
#endif
    if (XineramaIsActive(gGui->display)) {
      gVw->using_xinerama = 1;
      gVw->fullscreen_width  = screeninfo[0].width;
      gVw->fullscreen_height = screeninfo[0].height;
      gVw->xinerama = screeninfo;
      gVw->xinerama_cnt = screens;
    } else {
      gVw->fullscreen_width  = DisplayWidth  (gGui->display, gGui->screen);
      gVw->fullscreen_height = DisplayHeight (gGui->display, gGui->screen);
    }

  } else 
#endif
  {
    /* no Xinerama */
    printf ("Display is not using Xinerama.\n");
    gVw->fullscreen_width  = DisplayWidth (gGui->display, gGui->screen);
    gVw->fullscreen_height = DisplayHeight (gGui->display, gGui->screen);
  } 
  gVw->visible_width  = gVw->fullscreen_width;
  gVw->visible_height = gVw->fullscreen_height;

  /* create xclass hint for video window */

  if ((gVw->xclasshint = XAllocClassHint()) != NULL) {
    gVw->xclasshint->res_name = _("Xine Video Window");
    gVw->xclasshint->res_class = "Xine";
  }
  if ((gVw->xclasshint_fullscreen = XAllocClassHint()) != NULL) {
    gVw->xclasshint_fullscreen->res_name = _("Xine Video Fullscreen Window");
    gVw->xclasshint_fullscreen->res_class = "Xine";
  }
  if ((gVw->xclasshint_borderless = XAllocClassHint()) != NULL) {
    gVw->xclasshint_borderless->res_name = _("Xine Video Borderless Window");
    gVw->xclasshint_borderless->res_class = "Xine";
  }

  /* 
   * create cursors
   */

  bm_no = XCreateBitmapFromData(gGui->display, 
				gGui->imlib_data->x.root, 
				bm_no_data, 8, 8);
  gVw->cursor[0] = XCreatePixmapCursor(gGui->display, bm_no, bm_no,
				      &gGui->black, &gGui->black, 0, 0);
  gVw->cursor[1] = XCreateFontCursor(gGui->display, XC_left_ptr);

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

  /*
   * completion event
   */

  if (XShmQueryExtension (gGui->display) == True) {
    gVw->completion_event = XShmGetEventBase (gGui->display) + ShmCompletion;
  } else {
    gVw->completion_event = -1;
  }

  XUnlockDisplay (gGui->display);

  gVw->stream_resize_window = 
    xine_config_register_bool(gGui->xine, 
			      "gui.stream_resize_window", 
			      0,
			      _("New stream sizes resize output window"),
			      CONFIG_NO_HELP,
			      CONFIG_LEVEL_BEG,
			      _video_window_resize_cb,
			      CONFIG_NO_DATA);
  
  gVw->zoom_small_stream = 
    xine_config_register_bool(gGui->xine,
			      "gui.zoom_small_stream", 
			      0,
			      _("Double size for small streams (require stream_resize_window)"),
			      CONFIG_NO_HELP,
			      CONFIG_LEVEL_BEG,
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
  XLockDisplay (gGui->display);
  
  if(xine_config_register_bool(gGui->xine, "gui.use_xvidext", 
			       0,
			       _("use XVidModeExtension when switching to fullscreen"),
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_BEG,
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
     
    if(XF86VidModeQueryExtension(gGui->display, &dummy_query_event, &dummy_query_error)) {
      XF86VidModeModeInfo* XF86_modelines_swap;
      int                  major, minor, sort_x, sort_y;
      
      XF86VidModeQueryVersion(gGui->display, &major, &minor);
      printf(_("XF86VidMode Extension (%d.%d) detected, trying to use it.\n"), major, minor);
      
      if(XF86VidModeGetAllModeLines(gGui->display, XDefaultScreen(gGui->display), &(gVw->XF86_modelines_count), &(gVw->XF86_modelines))) {
	printf(_("XF86VidMode Extension: %d modelines found.\n"), gVw->XF86_modelines_count);
	
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
  }
   else
     gVw->XF86_modelines_count = 0;

  XUnlockDisplay (gGui->display);
#endif

  video_window_set_mag(1.0);

  /*
   * for plugins that aren't really bind to the window, it's necessary that the
   * gVw->xwin and gVw->ywin variables are set to *real* values, otherwise the
   * overlay will be displayed somewhere outside the window
   */
  if(gGui->video_window) {
    Window tmp_win;
    
    if((window_attribute->x > -8192) && (window_attribute->y > -8192)) {
      gVw->xwin = gVw->old_xwin = window_attribute->x;
      gVw->ywin = gVw->old_ywin = window_attribute->y;
      
      XLockDisplay (gGui->display);
      XMoveResizeWindow (gGui->display, gGui->video_window, 
			 gVw->xwin, gVw->ywin, gVw->video_width, gVw->video_height);
      XUnlockDisplay (gGui->display);
  
    }

    XLockDisplay (gGui->display);
    XTranslateCoordinates(gGui->display, gGui->video_window, DefaultRootWindow(gGui->display), 
			  0, 0, &gVw->xwin, &gVw->ywin, &tmp_win);
    XUnlockDisplay (gGui->display);
    
  }
}


/*
 * Necessary cleanup
 */
void video_window_exit (void) {
#warning FIXME NEWAPI
#if 0
  xine_tvmode_exit2 (gGui->xine);
#endif
}


/*
 * Translate screen coordinates to video coordinates
 */
static int video_window_translate_point(int gui_x, int gui_y,
					int *video_x, int *video_y)
{
  x11_rectangle_t rect;
  int xwin, ywin;
  unsigned int wwin, hwin, bwin, dwin;
  float xf,yf;
  float scale, width_scale, height_scale,aspect;
  Window rootwin;

  rect.x = gui_x;
  rect.y = gui_y;
  rect.w = 0;
  rect.h = 0;

  if (xine_gui_send_vo_data(gGui->xine, 
			    XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect) != -1) {
    /* driver implements gui->video coordinate space translation, use it */
    *video_x = rect.x;
    *video_y = rect.y;
    return 1;
  }

  /* Driver cannot convert gui->video space, fall back to old code... */

  if(XGetGeometry(gGui->display, gGui->video_window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) == BadDrawable)
    return 0;

  /* Scale co-ordinate to image dimensions. */
  height_scale=(float)gVw->video_height/(float)hwin;
  width_scale=(float)gVw->video_width/(float)wwin;
  aspect=(float)gVw->video_width/(float)gVw->video_height;
  if (((float)wwin/(float)hwin)<aspect) {
    scale=width_scale;
    xf=(float)gui_x * scale;
    yf=(float)gui_y * scale;
    /* wwin=wwin * scale; */
    hwin=hwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x=xf * 1.25 / aspect;
    *video_y=yf-((hwin-gVw->video_height)/2);
    /* printf("wscale:a=%f, s=%f, x=%d, y=%d\n",aspect, scale,*video_x,*video_y);  */
  } else {
    scale=height_scale;
    xf=(float)gui_x * scale;
    yf=(float)gui_y * scale;
    wwin=wwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x=(xf-((wwin-gVw->video_width)/2)) * 1.25 / aspect;
    *video_y=yf;
    /* printf("hscale:a=%f s=%f x=%d, y=%d\n",aspect,scale,*video_x,*video_y);  */
  }

  return 1;
}

/*
 * Set/Get magnification.
 */
void video_window_set_mag(float mag) {

  if(gVw->fullscreen_mode
#ifdef HAVE_XF86VIDMODE
     && !(gVw->XF86_modelines_count > 1)
#endif
     )
    return;

  gVw->mag = mag;
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
void video_window_change_skins(void) {
  xine_cfg_entry_t cfg_entry;
  char             *skin_logo;
  static int        sk_changed = 0;
  int cfg_err_result;
  memset(&cfg_entry, 0, sizeof(xine_cfg_entry_t));
  cfg_err_result = xine_config_lookup_entry(gGui->xine, "misc.logo_mrl", &cfg_entry);
  skin_logo = xitk_skin_get_logo(gGui->skin_config);
  
  if(skin_logo) {
    
    if((cfg_err_result) && cfg_entry.str_value) {
      /* Old and new logo are same, don't reload */
      if(!strcmp(cfg_entry.str_value, skin_logo))
	return;
    }
    
    config_update_string("misc.logo_mrl", skin_logo);
  }
  else { /* Skin don't use logo feature, set to xine's default */
    
    /* 
     * Back to default logo only on a skin 
     * change, not at the first skin loading.
     **/
    if((cfg_err_result) && sk_changed)
      config_update_string("misc.logo_mrl", XINE_LOGO_MRL);
  }

  sk_changed++;
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
    
    if((!gGui->cursor_visible) 
#ifdef HAVE_XTESTEXTENSION
       && (event->xkey.keycode != gVw->kc_shift_l)
#endif
       ) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }
    
    gui_handle_event(event, data);
    break;
    
  case MotionNotify: {
    XMotionEvent *mevent = (XMotionEvent *) event;
    xine_input_event_t xine_event;
    int x, y;

    /* printf("Mouse event:mx=%d my=%d\n",mevent->x, mevent->y); */
    
    if(!gGui->cursor_visible) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }

    if (video_window_translate_point(mevent->x, mevent->y, &x, &y)) {
      xine_event.event.type = XINE_EVENT_MOUSE_MOVE;
      xine_event.button = 0; /*  No buttons, just motion. */
      xine_event.x = x;
      xine_event.y = y;
      xine_send_event(gGui->xine, (xine_event_t*)(&xine_event));
    }
  }
  break;

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    xine_input_event_t xine_event;
    int x, y;

    if(!gGui->cursor_visible) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }

    if (bevent->button == Button3)
      panel_toggle_visibility(NULL, NULL);

    if (bevent->button == Button1) {
      if (video_window_translate_point(bevent->x, bevent->y, &x, &y)) {
	xine_event.event.type = XINE_EVENT_MOUSE_BUTTON;
	xine_event.button = 1;
	xine_event.x = x;
	xine_event.y = y;
	xine_send_event(gGui->xine, (xine_event_t*)(&xine_event));
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
	xine_gui_send_vo_data(gGui->xine, XINE_GUI_SEND_EXPOSE_EVENT, event);
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

  if (event->type == gVw->completion_event) 
    xine_gui_send_vo_data(gGui->xine, XINE_GUI_SEND_COMPLETION_EVENT, (void *)event);

}

void video_window_reset_ssaver(void) {
#ifdef HAVE_XTESTEXTENSION
  if(gVw->have_xtest == True) {
    XLockDisplay(gGui->display);
    XTestFakeKeyEvent(gGui->display, gVw->kc_shift_l, True, CurrentTime);
    XTestFakeKeyEvent(gGui->display, gVw->kc_shift_l, False, CurrentTime);
    XSync(gGui->display, False);
    XUnlockDisplay(gGui->display);
  }
  else 
#endif
    {
      XLockDisplay(gGui->display);
      XResetScreenSaver(gGui->display);
      XUnlockDisplay(gGui->display);
    }
}
