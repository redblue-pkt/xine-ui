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
 * video window handling functions
 */

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>

#include <xine.h>
#include <xine/video_out_x11.h>

#include "xine.h"

#include "xitk.h"

#include "utils.h"
#include "Imlib-light/Imlib.h"

#include "event.h"
#include "videowin.h"

extern gGui_t *gGui;

/* Video window private structure */
typedef struct {
  XVisualInfo    vinfo;
  Cursor         cursor[2];       /* Cursor pointers                       */
  int            cursor_visible;
  XClassHint    *xclasshint;
  GC             gc;
  int            video_width;     /* size of currently displayed video     */
  int            video_height;
  int            fullscreen_mode; /* are we currently in fullscreen mode?  */
  int            fullscreen_req;  /* ==1 => video_window will 
				   * switch to fullscreen mode             */
  int            fullscreen_width;
  int            fullscreen_height;
  int            completion_type;
  int            depth;
  int            show;
  XWMHints      *wm_hint;
  DND_struct_t   xdnd;

  int            completion_event;

} gVw_t;

static gVw_t    *gVw;

/*
 *
 */
void video_window_draw_logo(void) {
  ImlibImage *resized_image;
  int xwin, ywin, tmp;
  unsigned int wwin, hwin, bwin, dwin;
  double ratio = 1;
  Window rootwin;
  
  XLockDisplay (gGui->display);
  
  XClearWindow (gGui->display, gGui->video_window); 

  if(XGetGeometry(gGui->display, gGui->video_window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    tmp = (wwin / 100) * 86;
    ratio = (tmp < gGui->video_window_logo_pixmap.width && tmp >= 1) ? 
      (ratio = (double)tmp / (double)gGui->video_window_logo_pixmap.width) : 1;
  }
  
  resized_image = Imlib_clone_image(gGui->imlib_data, 
				    gGui->video_window_logo_image);
  Imlib_render (gGui->imlib_data, resized_image, 
		(int)gGui->video_window_logo_pixmap.width * ratio, 
		(int)gGui->video_window_logo_pixmap.height * ratio);

  XCopyArea (gGui->display, resized_image->pixmap, gGui->video_window, 
	     gVw->gc, 0, 0,
	     resized_image->width, resized_image->height, 
	     (wwin - resized_image->width) / 2, 
	     (hwin - resized_image->height) / 2);

  
  XFlush(gGui->display);
  XSync(gGui->display, False);
  
  Imlib_destroy_image(gGui->imlib_data, resized_image);

  XUnlockDisplay (gGui->display);
}

void video_window_hide_logo(void) {

  XUnmapWindow (gGui->display, gGui->video_window);
  XMapWindow (gGui->display, gGui->video_window);
  /*
  gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
				      GUI_DATA_EX_DRAWABLE_CHANGED, 
				      (void*)gGui->video_window);
				      */
}

/*
 *
 */
void video_window_set_fullscreen (int req_fullscreen) {

  x11_rectangle_t area;

  gVw->fullscreen_req = req_fullscreen;

  video_window_adapt_size (gVw->video_width, gVw->video_height, 
			   &area.x, &area.y, &area.w, &area.h);

  gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
				      GUI_DATA_EX_DEST_POS_SIZE_CHANGED, 
				      &area);
}

/*
 *
 */
int video_window_is_fullscreen (void) {
  return gVw->fullscreen_mode;
}

/*
 *
 */
void video_window_calc_dest_size (int video_width, int video_height,
				  int *dest_width, int *dest_height)  {

  if (gVw->fullscreen_mode) {

    *dest_width  = gVw->fullscreen_width;
    *dest_height = gVw->fullscreen_height;
    
  } else {

    *dest_width  = video_width;
    *dest_height = video_height;

  }
}

/*
 *
 */
void video_window_adapt_size (int video_width, int video_height, 
			      int *dest_x, int *dest_y,
			      int *dest_width, int *dest_height) {

  static char          *window_title = "xine video output";
  XSizeHints            hint;
  XWMHints             *wm_hint;
  XSetWindowAttributes  attr;
  Atom                  prop;
  Atom                  wm_delete_window;
  MWMHints              mwmhints;
  XEvent                xev;
  XGCValues             xgcv;
  Window                old_video_window = None;

  XLockDisplay (gGui->display);

  gVw->video_width = video_width;
  gVw->video_height = video_height;
  *dest_x = 0;
  *dest_y = 0;

  if (gVw->fullscreen_req) {

    *dest_width  = gVw->fullscreen_width;
    *dest_height = gVw->fullscreen_height;

    if (gGui->video_window) {

      if (gVw->fullscreen_mode) {
	XUnlockDisplay (gGui->display);
	return;
      }

      old_video_window = gGui->video_window;
    }

    gVw->fullscreen_mode = 1;

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black.pixel;
    
    gGui->video_window = 
      XCreateWindow (gGui->display, 
		     RootWindow (gGui->display, DefaultScreen(gGui->display)), 
		     0, 0, gVw->fullscreen_width, 
		     gVw->fullscreen_height, 
		     0, gVw->depth, CopyFromParent, 
		     gVw->vinfo.visual,
		     CWBackPixel, &attr);
    
    if(gGui->vo_driver)
      gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					  GUI_DATA_EX_DRAWABLE_CHANGED, 
					  (void*)gGui->video_window);
    
    if (gVw->xclasshint != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint);

    XSetWMHints(gGui->display, gGui->video_window, gVw->wm_hint);

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

    /*
     * drag and drop
     */
    
    dnd_make_window_aware (&gVw->xdnd, gGui->video_window);

  } else {

    *dest_width  = gVw->video_width;
    *dest_height = gVw->video_height;

    if (gGui->video_window) {

      if (gVw->fullscreen_mode)
	old_video_window = gGui->video_window;
      else {
	
	XResizeWindow (gGui->display, gGui->video_window, 
		       gVw->video_width, gVw->video_height);

	XUnlockDisplay (gGui->display);
	
	return;
	
      }
    }

    gVw->fullscreen_mode = 0;

    hint.x = 0;
    hint.y = 0;
    hint.width  = gVw->video_width;
    hint.height = gVw->video_height;
    hint.flags  = PPosition | PSize;

    /*
    theCmap   = XCreateColormap(display, RootWindow(display,gXv.screen), 
    gXv.vinfo.visual, AllocNone); */
  
    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = 1;
    /* attr.colormap          = theCmap; */
    

    gGui->video_window = 
      XCreateWindow(gGui->display, RootWindow(gGui->display, gGui->screen),
		    hint.x, hint.y, hint.width, hint.height, 4, 
		    gVw->depth, CopyFromParent, gVw->vinfo.visual,
		    CWBackPixel | CWBorderPixel , &attr);
    
    if(gGui->vo_driver)
      gGui->vo_driver->gui_data_exchange (gGui->vo_driver,
					  GUI_DATA_EX_DRAWABLE_CHANGED, 
					  (void*)gGui->video_window);
    
    if (gVw->xclasshint != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gVw->xclasshint);

    XSetWMHints(gGui->display, gGui->video_window, gVw->wm_hint);

    /*
     * drag and drop
     */
    
    dnd_make_window_aware (&gVw->xdnd, gGui->video_window);
      
    /* Tell other applications about gGui window */

    XSetStandardProperties(gGui->display, gGui->video_window, 
			   window_title, window_title, None, NULL, 0, &hint);
  }
  
  
  XSelectInput(gGui->display, gGui->video_window, 
	       StructureNotifyMask | ExposureMask | 
	       KeyPressMask | ButtonPressMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
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

  XFlush(gGui->display);
  XSync(gGui->display, False);
  
  gVw->gc = XCreateGC(gGui->display, gGui->video_window, 0L, &xgcv);

  if (gVw->fullscreen_mode) {
    XSetInputFocus (gGui->display, 
		    gGui->video_window, RevertToNone, CurrentTime);
    XMoveWindow (gGui->display, gGui->video_window, 0, 0);
  }

  XUnlockDisplay (gGui->display);

  dnd_init_dnd(gGui->display, &gVw->xdnd);
  dnd_set_callback (&gVw->xdnd, gui_dndcallback);
  dnd_make_window_aware (&gVw->xdnd, gGui->video_window);

  /* The old window should be destroyed now */
  if(old_video_window != None)
    XDestroyWindow(gGui->display, old_video_window);
}

/*
 * hide/show cursor in video window
 */
void video_window_set_cursor_visibility(int show_cursor) {
  gVw->cursor_visible = show_cursor;
  XDefineCursor(gGui->display, gGui->video_window, 
		gVw->cursor[gVw->cursor_visible]);
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

  gVw->show = show_window;

  if (gVw->show == 1)
    XMapRaised (gGui->display, gGui->video_window);
  else
    XUnmapWindow (gGui->display, gGui->video_window);
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
void video_window_init (void) {

  XWindowAttributes  attribs;
  Pixmap             bm_no;
  int                x,y,w,h;


  gVw = (gVw_t *) xmalloc(sizeof(gVw_t));

  gVw->fullscreen_req  = 0;
  gVw->fullscreen_mode = 0;
  gGui->video_window   = 0;
  gVw->show            = 1;

  XLockDisplay (gGui->display);

  XGetWindowAttributes(gGui->display, 
		       DefaultRootWindow(gGui->display), &attribs);

  gVw->depth = attribs.depth;
  
  if (gVw->depth != 15 
      && gVw->depth != 16 
      && gVw->depth != 24 
      && gVw->depth != 32)  {
    /* The root window may be 8bit but there might still be
     * visuals with other bit depths. For example this is the 
     * case on Sun/Solaris machines.
     */
    gVw->depth = 24;
  }

  if (!XMatchVisualInfo(gGui->display, 
			gGui->screen, gVw->depth, TrueColor, &gVw->vinfo)) {
    printf ("gui_main: couldn't find truecolor visual for video window.\n");
    exit (1);
  }
  
  
#ifdef HAVE_XINERAMA
  /* Spark
   * some Xinerama stuff
   * I want to figure out what fullscreen means for this setup
   */

  if ((XineramaQueryExtension (gGui->display, &dummy_a, &dummy_b)) 
      && (screeninfo = XineramaQueryScreens(gGui->display, &screens))) {
    /* Xinerama Detected */
#ifdef DEBUG
    printf ("Display is using Xinerama with %d screens\n", screens);
    printf (" going to assume we are using the first screen.\n");
    printf (" size of the first screen is %dx%d.\n", 
	     screeninfo[0].width, screeninfo[0].height);
#endif
    if (XineramaIsActive(gGui->display)) {
      gVw->fullscreen_width  = screeninfo[0].width;
      gVw->fullscreen_height = screeninfo[0].height;
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

  /* 
   * create cursors
   */

  bm_no = XCreateBitmapFromData(gGui->display, 
				DefaultRootWindow(gGui->display), 
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

  XUnlockDisplay (gGui->display);

  video_window_adapt_size (768, 480, &x, &y, &w, &h);
  video_window_draw_logo();

}

/*
 *
 */
void video_window_handle_event (XEvent *event) {
  
  switch(event->type) {

  case Expose:
    if(event->xany.window == gGui->video_window) {
      if(xine_get_status(gGui->xine) == XINE_STOP)
	video_window_draw_logo();
      else {

	gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					    GUI_DATA_EX_EXPOSE_EVENT, 
					    event);
      }
    }
    break;

  case ConfigureNotify:
  case VisibilityNotify:
    if(event->xany.window == gGui->video_window) {
      if(xine_get_status(gGui->xine) != XINE_STOP) {
	XConfigureEvent *cev = (XConfigureEvent *) event;
	x11_rectangle_t area;

	area.x = 0;
	area.y = 0;
	area.w = cev->width;
	area.h = cev->height;
	
	gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					    GUI_DATA_EX_DEST_POS_SIZE_CHANGED, 
					    &area);
	
	gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					    GUI_DATA_EX_EXPOSE_EVENT, 
					    event);
      }
    }
    break;
    
  case ClientMessage:
    dnd_process_client_message (&gVw->xdnd, event);
    break;
  }

  if (event->type == gVw->completion_event) 
    gGui->vo_driver->gui_data_exchange (gGui->vo_driver, 
					GUI_DATA_EX_COMPLETION_EVENT, 
					event);

}
