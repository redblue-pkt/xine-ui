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

#include <xine.h>
#include <xine/video_out_x11.h>

#include "gui_main.h"

extern gGlob_t *gGui;

typedef struct
{
  int          flags;
  int          functions;
  int          decorations;
  int          input_mode;
  int          status;
} MWMHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5

void gui_setup_video_window (int video_width, int video_height, int *dest_x, int *dest_y,
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

  XLockDisplay (display);

  gGui->video_width = video_width;
  gGui->video_height = video_height;
  *dest_x = 0;
  *dest_y = 0;

  if (gGui->fullscreen_req) {

    *dest_width  = gGui->fullscreen_width;
    *dest_height = gGui->fullscreen_height;

    if (gGui->video_win) {

      if (gGui->fullscreen_mode) {
	XUnlockDisplay (gGui->display);
	return;
      }

      XDestroyWindow(gGui->display, gGui->video_win);
      gGui->video_win = 0;

    }

    gGui->fullscreen_mode = 1;

    /*
     * open fullscreen window
     */

    attr.background_pixel  = gGui->black;

    gGui->video_win = XCreateWindow (gGui->display, 
				     RootWindow (display, DefaultScreen(gGui->display)), 
				     0, 0, gGui->fullscreen_width, gGui->fullscreen_height, 
				     0, gGui->depth, CopyFromParent, gGui->vinfo.visual,
				     CWBackPixel, &attr);

    if (this->xclasshint != NULL)
      XSetClassHint(this->display, this->window, this->xclasshint);

    /*
     * wm, no borders please
     */
    
    prop = XInternAtom(this->display, "_MOTIF_WM_HINTS", False);
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0;
    XChangeProperty(this->display, this->window, prop, prop, 32,
		    PropModeReplace, (unsigned char *) &mwmhints,
		    PROP_MWM_HINTS_ELEMENTS);
    XSetTransientForHint(this->display, this->window, None);
    XRaiseWindow(this->display, this->window);

  } else {

    *dest_width  = gGui->video_width;
    *dest_height = gGui->video_height;

    if (this->window) {

      if (this->fullscreen_mode) {
	XDestroyWindow(this->display, this->window);
	this->window = 0;
      } else {
	
	XResizeWindow (this->display, this->window, 
		       this->video_width, this->video_height);

	XUnlockDisplay (this->display);
	
	return;
	
      }
    }

    this->fullscreen_mode = 0;

    hint.x = 0;
    hint.y = 0;
    hint.width  = this->video_width;
    hint.height = this->video_height;
    hint.flags  = PPosition | PSize;

    /*
    theCmap   = XCreateColormap(display, RootWindow(display,gXv.screen), 
    gXv.vinfo.visual, AllocNone); */
  
    attr.background_pixel  = this->black.pixel;
    attr.border_pixel      = 1;
    /* attr.colormap          = theCmap; */
    

    this->window = XCreateWindow(this->display, RootWindow(this->display, this->screen),
				 hint.x, hint.y, hint.width, hint.height, 4, 
				 this->depth, CopyFromParent, this->vinfo.visual,
				 CWBackPixel | CWBorderPixel , &attr);

    if (this->xclasshint != NULL)
      XSetClassHint(this->display, this->window, this->xclasshint);

    
    /* Tell other applications about this window */

    XSetStandardProperties(this->display, this->window, window_title, window_title, 
			   None, NULL, 0, &hint);

  }
  
  XSelectInput(this->display, this->window, StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(this->display, this->window, wm_hint);
    XFree(wm_hint);
  }

  /* FIXME
  wm_delete_window = XInternAtom(this->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(this->display, this->window, &wm_delete_window, 1);
  */

  /* Map window. */
  
  XMapRaised(this->display, this->window);
  
  /* Wait for map. */

  do  {
    XMaskEvent(this->display, 
	       StructureNotifyMask, 
	       &xev) ;
  } while (xev.type != MapNotify || xev.xmap.event != this->window);

  XFlush(this->display);
  XSync(this->display, False);
  
  this->gc = XCreateGC(this->display, this->window, 0L, &xgcv);

  if (this->fullscreen_mode) {
    XSetInputFocus (this->display, this->window, RevertToNone, CurrentTime);
    XMoveWindow (this->display, this->window, 0, 0);
  }

  XUnlockDisplay (this->display);

  /* drag and drop FIXME: implement */

  /* 
  if(!gXv.xdnd)
    gXv.xdnd = (DND_struct_t *) malloc(sizeof(DND_struct_t));
  
  gui_init_dnd(gXv.xdnd);
  gui_dnd_set_callback (gXv.xdnd, gui_dndcallback);
  gui_make_window_dnd_aware (gXv.xdnd, gXv.window);
  */

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
void gui_set_cursor_visibility(int show_cursor) {
  XDefineCursor(gGui->display, gGui->video_win, gGui->cursor[show_cursor]);
}

/* hide/show video window */
void gui_set_video_window_visibility(int show_window) {
  if(show_window == 1)
    XMapRaised (gGui->display, gGui->video_win);
  else
    XUnmapWindow (gGui->display, gGui->video_win);
}
