/* 
 * Copyright (C) 2000 the xine project
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

#include <xine.h>
#include <xine/video_out_x11.h>

#include "xitk.h"

#include "event.h"
#include "videowin.h"

extern gGui_t *gGui;

XVisualInfo    gvw_vinfo;
Cursor         gvw_cursor[2]; /* Cursor pointers     */
XClassHint    *gvw_xclasshint;
GC             gvw_gc;
int            gvw_video_width, gvw_video_height; /* size of currently displayed video */
int            gvw_fullscreen_mode; /* are we currently in fullscreen mode?  */
int            gvw_fullscreen_req;  /* ==1 => gui_setup_video_window will switch to fullscreen mode */
int            gvw_fullscreen_width, gvw_fullscreen_height;
int            gvw_completion_type;
int            gvw_depth;
int            gvw_show;
XWMHints      *gvw_wm_hint;

static DND_struct_t    xdnd_videowin;

void video_window_set_fullscreen (int req_fullscreen) {

  x11_rectangle_t area;

  gvw_fullscreen_req = req_fullscreen;

  video_window_adapt_size (gvw_video_width, gvw_video_height, 
			   &area.x, &area.y, &area.w, &area.h);

  gGui->vo_driver->gui_data_exchange (gGui->vo_driver, GUI_DATA_EX_DEST_POS_SIZE_CHANGED, &area);

}

int video_window_is_fullscreen () {
  return gvw_fullscreen_mode;
}

void video_window_calc_dest_size (int video_width, int video_height,
				  int *dest_width, int *dest_height)  {

  if (gvw_fullscreen_mode) {

    *dest_width  = gvw_fullscreen_width;
    *dest_height = gvw_fullscreen_height;
    
  } else {

    *dest_width  = video_width;
    *dest_height = video_height;

  }
}

void video_window_adapt_size (int video_width, int video_height, int *dest_x, int *dest_y,
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

  XLockDisplay (gGui->display);

  gvw_video_width = video_width;
  gvw_video_height = video_height;
  *dest_x = 0;
  *dest_y = 0;

  if (gvw_fullscreen_req) {

    *dest_width  = gvw_fullscreen_width;
    *dest_height = gvw_fullscreen_height;

    if (gGui->video_window) {

      if (gvw_fullscreen_mode) {
	XUnlockDisplay (gGui->display);
	return;
      }

      XDestroyWindow(gGui->display, gGui->video_window);
      gGui->video_window = 0;

    }

    gvw_fullscreen_mode = 1;

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black.pixel;

    gGui->video_window = XCreateWindow (gGui->display, 
				     RootWindow (gGui->display, DefaultScreen(gGui->display)), 
				     0, 0, gvw_fullscreen_width, gvw_fullscreen_height, 
				     0, gvw_depth, CopyFromParent, gvw_vinfo.visual,
				     CWBackPixel, &attr);

    if (gvw_xclasshint != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gvw_xclasshint);

    XSetWMHints(gGui->display, gGui->video_window, gvw_wm_hint);

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
    
    dnd_make_window_aware (&xdnd_videowin, gGui->video_window);

  } else {

    *dest_width  = gvw_video_width;
    *dest_height = gvw_video_height;

    if (gGui->video_window) {

      if (gvw_fullscreen_mode) {
	XDestroyWindow(gGui->display, gGui->video_window);
	gGui->video_window = 0;
      } else {
	
	XResizeWindow (gGui->display, gGui->video_window, 
		       gvw_video_width, gvw_video_height);

	XUnlockDisplay (gGui->display);
	
	return;
	
      }
    }

    gvw_fullscreen_mode = 0;

    hint.x = 0;
    hint.y = 0;
    hint.width  = gvw_video_width;
    hint.height = gvw_video_height;
    hint.flags  = PPosition | PSize;

    /*
    theCmap   = XCreateColormap(display, RootWindow(display,gXv.screen), 
    gXv.vinfo.visual, AllocNone); */
  
    attr.background_pixel  = gGui->black.pixel;
    attr.border_pixel      = 1;
    /* attr.colormap          = theCmap; */
    

    gGui->video_window = XCreateWindow(gGui->display, RootWindow(gGui->display, gGui->screen),
				 hint.x, hint.y, hint.width, hint.height, 4, 
				 gvw_depth, CopyFromParent, gvw_vinfo.visual,
				 CWBackPixel | CWBorderPixel , &attr);

    if (gvw_xclasshint != NULL)
      XSetClassHint(gGui->display, gGui->video_window, gvw_xclasshint);

    XSetWMHints(gGui->display, gGui->video_window, gvw_wm_hint);

    /*
     * drag and drop
     */
    
    dnd_make_window_aware (&xdnd_videowin, gGui->video_window);
      
    /* Tell other applications about gGui window */

    XSetStandardProperties(gGui->display, gGui->video_window, window_title, window_title, 
			   None, NULL, 0, &hint);

  }
  
  XSelectInput(gGui->display, gGui->video_window, StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask);

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
  
  gvw_gc = XCreateGC(gGui->display, gGui->video_window, 0L, &xgcv);

  if (gvw_fullscreen_mode) {
    XSetInputFocus (gGui->display, gGui->video_window, RevertToNone, CurrentTime);
    XMoveWindow (gGui->display, gGui->video_window, 0, 0);
  }

  XUnlockDisplay (gGui->display);

  dnd_init_dnd(gGui->display, &xdnd_videowin);
  dnd_set_callback (&xdnd_videowin, gui_dndcallback);
  dnd_make_window_aware (&xdnd_videowin, gGui->video_window);

  /*
   * make cursor disappear
   */

  /* FIXME: implement in a clean way

    Cursor not already created.
  if(gXv.current_cursor == -1) {
    create_cursor_xv(theCmap);
    gXv.current_cursor = SHOW_CURSOR;
  };
  */
}

/* hide/show cursor in video window*/
void video_window_set_cursor_visibility(int show_cursor) {
  XDefineCursor(gGui->display, gGui->video_window, gvw_cursor[show_cursor]);
}

/* hide/show video window */
void video_window_set_visible(int show_window) {

  gvw_show = show_window;

  if (gvw_show == 1)
    XMapRaised (gGui->display, gGui->video_window);
  else
    XUnmapWindow (gGui->display, gGui->video_window);
}

int video_window_is_visible () {
  return gvw_show;
}

static unsigned char bm_no_data[] = { 0,0,0,0, 0,0,0,0 };

void video_window_init () {

  XWindowAttributes  attribs;
  Pixmap             bm_no;
  int                x,y,w,h;

  gvw_fullscreen_req  = 0;
  gvw_fullscreen_mode = 0;
  gGui->video_window  = 0;
  gvw_show            = 1;

  XLockDisplay (gGui->display);

  XGetWindowAttributes(gGui->display, DefaultRootWindow(gGui->display), &attribs);

  gvw_depth = attribs.depth;
  
  if (gvw_depth != 15 && gvw_depth != 16 && gvw_depth != 24 && gvw_depth != 32)  {
    /* The root window may be 8bit but there might still be
     * visuals with other bit depths. For example this is the 
     * case on Sun/Solaris machines.
     */
    gvw_depth = 24;
  }

  if (!XMatchVisualInfo(gGui->display, gGui->screen, gvw_depth, TrueColor, &gvw_vinfo)) {
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
      gvw_fullscreen_width  = screeninfo[0].width;
      gvw_fullscreen_height = screeninfo[0].height;
    } else {
      gvw_fullscreen_width  = DisplayWidth  (gGui->display, gGui->screen);
      gvw_fullscreen_height = DisplayHeight (gGui->display, gGui->screen);
    }

  } else 
#endif
  {
    /* no Xinerama */
    printf ("Display is not using Xinerama.\n");
    gvw_fullscreen_width  = DisplayWidth (gGui->display, gGui->screen);
    gvw_fullscreen_height = DisplayHeight (gGui->display, gGui->screen);
  } 

  /* create xclass hint for video window */

  if ((gvw_xclasshint = XAllocClassHint()) != NULL) {
    gvw_xclasshint->res_name = "Xine Video Window";
    gvw_xclasshint->res_class = "Xine";
  }

  /* 
   * create cursors
   */

  XLockDisplay (gGui->display);

  bm_no = XCreateBitmapFromData(gGui->display, DefaultRootWindow(gGui->display), bm_no_data, 8, 8);
  gvw_cursor[0] = XCreatePixmapCursor(gGui->display, bm_no, bm_no,
				      &gGui->black, &gGui->black, 0, 0);
  gvw_cursor[1] = XCreateFontCursor(gGui->display, XC_left_ptr);

  /*
   * wm hints
   */

  gvw_wm_hint = XAllocWMHints();
  if (!gvw_wm_hint) {
    printf ("XAllocWMHints failed\n");
    exit (1);
  }

  gvw_wm_hint->input         = True;
  gvw_wm_hint->initial_state = NormalState;
  gvw_wm_hint->icon_pixmap   = gGui->icon;
  gvw_wm_hint->flags         = InputHint | StateHint | IconPixmapHint;

  XUnlockDisplay (gGui->display);

  video_window_adapt_size (768, 480, &x, &y, &w, &h);

}

