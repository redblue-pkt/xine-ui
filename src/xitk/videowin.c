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
#include <X11/extensions/XShm.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include <xine.h>
#include <xine/video_out_x11.h>
#include <xine/xineutils.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"

#include "event.h"
#include "videowin.h"
#include "panel.h"
#include "actions.h"
#include "errors.h"

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

  int            already_exposed;

  int            video_width;     /* size of currently displayed video     */
  int            video_height;
  int            win_width;       /* size of non-fullscreen window         */
  int            win_height;
  int            output_width;    /* output video window width/height      */
  int            output_height;
  float          mag;

  int            fullscreen_mode; /* are we currently in fullscreen mode?  */
  int            fullscreen_req;  /* ==1 => video_window will 
				   * switch to fullscreen mode             */
  int            fullscreen_width;
  int            fullscreen_height;
#ifdef HAVE_XINERAMA
  XineramaScreenInfo *xinerama;   /* pointer to xinerama struct, or NULL */
  int            xinerama_cnt;    /* number of screens in Xinerama */
#endif
  int            xwin;            /* current X location */
  int            ywin;            /* current Y location */
  int            desktopWidth;    /* desktop width */
  int            desktopHeight;   /* desktop height */
  int            completion_type;
  int            depth;
  int            show;
  int            borderless;      /* borderless window (for windowed mode)? */

  XWMHints      *wm_hint;

  xitk_register_key_t    widget_key;
  xitk_register_key_t    old_widget_key;

  int            completion_event;

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension stuff */
  XF86VidModeModeInfo** XF86_modelines;
  int                   XF86_modelines_count;
#endif

} gVw_t;

static gVw_t    *gVw;

#ifndef XShmGetEventBase
extern int XShmGetEventBase(Display *);
#endif

static void video_window_handle_event (XEvent *event, void *data);
static void video_window_adapt_size (void);

/*
 * Let the video driver override the selected visual
 */
void video_window_select_visual (void) {
  XVisualInfo *vinfo = (XVisualInfo *) -1;

  XLockDisplay (gGui->display);
  if (gGui->vo_driver) {
    gGui->vo_driver->gui_data_exchange (gGui->vo_driver, GUI_SELECT_VISUAL,
                                        &vinfo);
    if (vinfo != (XVisualInfo *) -1) {
      if (! vinfo) {
        fprintf (stderr, "videowin: output driver cannot select a working visual\n");
        exit (1);
      }
      gGui->visual = vinfo->visual;
      gGui->depth  = vinfo->depth;
    }
    if (gGui->visual != gVw->visual) {
      printf ("videowin: output driver overrides selected visual to visual id 0x%lx\n", gGui->visual->visualid);
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
 */
static void video_window_adapt_size (void) { 

  static char          *window_title = "xine video output";
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
/* fprintf (stderr, "***** XCreateWindow 1 visual %p id 0x%x depth %d\n", gVw->visual, gVw->visual->visualid, gVw->depth); */
      
      if(gGui->vo_driver)
	gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					    GUI_DATA_EX_DRAWABLE_CHANGED, 
					    (void*)gGui->video_window);
      
      XSetStandardProperties(gGui->display, gGui->video_window, 
			     window_title, window_title, None, NULL, 0, 0);
      

      gcv.foreground         = gGui->black.pixel;
      gcv.background         = gGui->black.pixel;
      gcv.graphics_exposures = False;
      gVw->gc = XCreateGC(gGui->display, gGui->video_window, 
			  GCForeground | GCBackground | GCGraphicsExposures, &gcv);
      
      XSetStandardProperties(gGui->display, gGui->video_window, 
			     window_title, window_title, None, NULL, 0, 0);
      
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

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension
   * In case a fullscreen request is received or if already in fullscreen, the
   * appropriate modeline will be looked up and used.
   */
  if(gVw->fullscreen_req || gVw->fullscreen_mode) {
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
    if(!(gVw->XF86_modelines_count <= 1) && !(search >= gVw->XF86_modelines_count)) {
       if(XF86VidModeSwitchToMode(gGui->display, XDefaultScreen(gGui->display), gVw->XF86_modelines[search])) {
	  XF86VidModeSetViewPort(gGui->display, XDefaultScreen(gGui->display), 0, 0);
          
	  gGui->XF86VidMode_fullscreen = 1;
	  
	  gVw->fullscreen_width  = gVw->XF86_modelines[search]->hdisplay;
	  gVw->fullscreen_height = gVw->XF86_modelines[search]->vdisplay;
	  
	  /*
	   * just in case the mouse pointer is off the visible area, move it
	   * to the middle of the video window
	   */
	  XWarpPointer(gGui->display, None, gGui->video_window, 0, 0, 0, 0, gVw->fullscreen_width/2, gVw->fullscreen_height/2);
	  
	  /*
	   * if this is true, we are back at the original resolution, so there
	   * is no need to further worry about anything.
	   */
	  if(gVw->fullscreen_mode && search == 0)
	    gGui->XF86VidMode_fullscreen = 0;
       } else {
	  xine_error("XF86VidMode Extension: modeline switching failed.\n");
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
  
  if (gVw->fullscreen_req) {

    if (gGui->video_window) {

      if (gVw->fullscreen_mode && gGui->visual == gVw->visual) {
#ifdef HAVE_XF86VIDMODE
	/*
	 * resizing the video window may be necessary if the modeline has
	 * just been switched
	 */
	XResizeWindow (gGui->display, gGui->video_window,
		       gVw->fullscreen_width, gVw->fullscreen_height);
	gVw->output_width    = gVw->fullscreen_width;
	gVw->output_height   = gVw->fullscreen_height;
#endif
	XUnlockDisplay (gGui->display);
	
	return;
      }

      xitk_unregister_event_handler(&gVw->old_widget_key);
      old_video_window = gGui->video_window;
    }

    gVw->fullscreen_mode = 1;
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
		     hint.x, hint.y, gVw->fullscreen_width, gVw->fullscreen_height, 
		     border_width, gVw->depth, InputOutput,
		     gVw->visual,
		     CWBackPixel | CWBorderPixel | CWColormap, &attr);

    if(gGui->vo_driver)
      gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					  GUI_DATA_EX_DRAWABLE_CHANGED, 
					  (void*)gGui->video_window);
    
    if (gVw->xclasshint_fullscreen != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint_fullscreen);

#ifndef HAVE_XINERAMA
    hint.x      = 0;
    hint.y      = 0;
    hint.width  = gVw->fullscreen_width;
    hint.height = gVw->fullscreen_height;
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
	   XF86VidModeSwitchToMode(gGui->display, XDefaultScreen(gGui->display), gVw->XF86_modelines[0]);
	   XF86VidModeSetViewPort(gGui->display, XDefaultScreen(gGui->display), 0, 0);

	   gGui->XF86VidMode_fullscreen = 0;
       
	   gVw->fullscreen_width  = gVw->XF86_modelines[0]->hdisplay;
	   gVw->fullscreen_height = gVw->XF86_modelines[0]->vdisplay;
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
      gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					  GUI_DATA_EX_DRAWABLE_CHANGED, 
					  (void*)gGui->video_window);
    
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

  /* Map window. */
  XMapRaised(gGui->display, gGui->video_window);
  
  /* Wait for map. */

  do  {
    XMaskEvent(gGui->display, 
	       StructureNotifyMask, 
	       &xev) ;
  } while (xev.type != MapNotify || xev.xmap.event != gGui->video_window);

  XSync(gGui->display, False);

  if (gVw->gc != None) XFreeGC(gGui->display, gVw->gc);
  gVw->gc = XCreateGC(gGui->display, gGui->video_window, 0L, &xgcv);
  
  if (gVw->fullscreen_mode) {
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
       XGrabPointer(gGui->display, gGui->video_window, 1, None, GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
  }

  gVw->old_widget_key = gVw->widget_key;

  XUnlockDisplay (gGui->display);

  gVw->widget_key = xitk_register_event_handler("video_window", 
						gGui->video_window, 
						video_window_handle_event,
						NULL,
						gui_dndcallback,
						NULL, NULL);
  
}

/*
 *
 */
void video_window_dest_size_cb (void *data,
				int video_width, int video_height,
				int *dest_width, int *dest_height)  {

  if (gVw->fullscreen_mode) {
    *dest_width  = gVw->fullscreen_width;
    *dest_height = gVw->fullscreen_height;
  } else {
    *dest_width  = gVw->output_width;
    *dest_height = gVw->output_height;
  }
}

/*
 *
 */
void video_window_frame_output_cb (void *data,
				   int video_width, int video_height,
				   int *dest_x, int *dest_y, 
				   int *dest_width, int *dest_height,
				   int *win_x, int *win_y) {
  
  gVw->video_width  = video_width;
  gVw->video_height = video_height;

  *dest_x = 0;
  *dest_y = 0;

  if (gVw->fullscreen_mode) {
    *dest_width  = gVw->fullscreen_width;
    *dest_height = gVw->fullscreen_height;
  } else {
    *dest_width  = gVw->output_width;
    *dest_height = gVw->output_height;
  }

  *win_x = (gVw->xwin < 0) ? 0 : gVw->xwin;
  *win_y = (gVw->ywin < 0) ? 0 : gVw->ywin;

}

/*
 *
 */
void video_window_set_fullscreen (int req_fullscreen) {
  gVw->fullscreen_req = req_fullscreen;
  
  video_window_adapt_size ();
}

/*
 *
 */
int video_window_is_fullscreen (void) {
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

  gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
				      GUI_DATA_EX_VIDEOWIN_VISIBLE, 
				      (int *)show_window);
  
  gVw->show = show_window;

  if (gVw->show == 1) {
    XLockDisplay (gGui->display);

    /*
     * layer above most other things, like gnome panel
     * WIN_LAYER_ABOVE_DOCK  = 10
     *
     */
    if(gGui->layer_above) {
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
void video_window_init (window_attributes_t *window_attribute) {

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
  gVw->already_exposed    = 0;
  gVw->borderless         = window_attribute->borderless;

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

  /* create xclass hint for video window */

  if ((gVw->xclasshint = XAllocClassHint()) != NULL) {
    gVw->xclasshint->res_name = "Xine Video Window";
    gVw->xclasshint->res_class = "Xine";
  }
  if ((gVw->xclasshint_fullscreen = XAllocClassHint()) != NULL) {
    gVw->xclasshint_fullscreen->res_name = "Xine Video Fullscreen Window";
    gVw->xclasshint_fullscreen->res_class = "Xine";
  }
  if ((gVw->xclasshint_borderless = XAllocClassHint()) != NULL) {
    gVw->xclasshint_borderless->res_name = "Xine Video Borderless Window";
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
    printf ("XAllocWMHints failed\n");
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

#ifdef HAVE_XF86VIDMODE
  if (gGui->config->register_bool (gGui->config, "gui.use_xvidect", 0,
				   "use XVidModeExtension when switching to fullscreen",
				   NULL, NULL, NULL)) {
    if(XF86VidModeQueryExtension(gGui->display, &dummy_query_event, &dummy_query_error)) {
      XF86VidModeModeInfo* XF86_modelines_swap;
      int                  major, minor, sort_x, sort_y;
      
      XF86VidModeQueryVersion(gGui->display, &major, &minor);
      printf("XF86VidMode Extension (%d.%d) detected, trying to use it.\n", major, minor);
      
      if(XF86VidModeGetAllModeLines(gGui->display, XDefaultScreen(gGui->display), &(gVw->XF86_modelines_count), &(gVw->XF86_modelines))) {
	printf("XF86VidMode Extension: %d modelines found.\n", gVw->XF86_modelines_count);
	
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
	printf("XF86VidMode Extension: could not get list of available modelines. Failed.\n");
      }
    } else {
      printf("XF86VidMode Extension: initialization failed, not using it.\n");
    }
  }
#endif

  XUnlockDisplay (gGui->display);

  if((window_attribute->width > 0) && (window_attribute->height > 0)) {
    gVw->video_width  = window_attribute->width;
    gVw->video_height = window_attribute->height;
  }
  else {
    gVw->video_width  = 768;
    gVw->video_height = 480;
  }
  
  video_window_set_mag(1.0);

  /*
   * for plugins that aren't really bind to the window, it's necessary that the
   * gVw->xwin and gVw->ywin variables are set to right values, otherwise the
   * overlay will be displayed somewhere outside the window
   */
  if(gGui->video_window) {
    Window tmp_win;
    
    if((window_attribute->x > -8192) && (window_attribute->y > -8192)) {
      gVw->xwin = window_attribute->x;
      gVw->ywin = window_attribute->y;
      
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

  if (gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					  GUI_DATA_EX_TRANSLATE_GUI_TO_VIDEO, 
					  (void*)&rect) != -1) {
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
    //wwin=wwin * scale;
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
  cfg_entry_t *cfg_entry;
  char        *new_vo_logo;

  cfg_entry   = gGui->config->lookup_entry(gGui->config, "video.logo_file");
  new_vo_logo = xitk_skin_get_skin_filename(gGui->skin_config, "VOLogo");
  
  if(new_vo_logo) {

    if(cfg_entry && cfg_entry->str_value) {
      /* Old and new logo are same, don't reload */
      if(!strcmp(cfg_entry->str_value, new_vo_logo))
	return;
    }
    
    gGui->config->update_string(gGui->config, "video.logo_file", new_vo_logo);
  }
  else {
    char  default_logo[2048];
    
    if(cfg_entry) {
      memset(&default_logo, 0, sizeof(default_logo));
      snprintf(default_logo, 2048, "%s/xine_logo.zyuy2", XINE_SKINDIR);
      gGui->config->update_string(gGui->config, "video.logo_file", default_logo);
    }
  }
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

    if(!gGui->cursor_visible) {
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

  case Expose: {
    XExposeEvent * xev = (XExposeEvent *) event;

    /* Ensure we load the right logo */
    if(!gVw->already_exposed) {
      video_window_change_skins();
      gVw->already_exposed++;
    }


    if (xev->count == 0) {

      if(event->xany.window == gGui->video_window) {
	gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					    GUI_DATA_EX_EXPOSE_EVENT, 
					    event);
      }
    }
  }
  break;

  case ConfigureNotify:
    if(event->xany.window == gGui->video_window) {
      XConfigureEvent *cev = (XConfigureEvent *) event;

      gVw->output_width  = cev->width;
      gVw->output_height = cev->height;

      gVw->xwin = cev->x;
      gVw->ywin = cev->y;

    }
    break;
    
  }

  if (event->type == gVw->completion_event) 
    gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					GUI_DATA_EX_COMPLETION_EVENT, 
					event);

}
