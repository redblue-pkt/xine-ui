/* 
 * Copyright (C) 2000-2003 the xine project
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
#include <X11/Xmd.h>

#include "common.h"

extern gGui_t            *gGui;
static xitk_window_t     *xwin = NULL;

void splash_create(void) {
  xitk_image_t *xim;
  
  if((xim = xitk_image_load_image(gGui->imlib_data, XINE_SPLASH))) {
    int  x, y;
    
    XLockDisplay(gGui->display);
    x = (((DisplayWidth(gGui->display, gGui->screen))) >> 1) - (xim->width >> 1);
    y = (((DisplayHeight(gGui->display, gGui->screen))) >> 1) - (xim->height >> 1);
    XUnlockDisplay(gGui->display);
    
    xwin = xitk_window_create_simple_window(gGui->imlib_data, 
					    x, y, xim->width, xim->height);
    
    xitk_window_change_background(gGui->imlib_data, xwin, 
				  xim->image->pixmap, xim->width, xim->height);
    
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, xitk_window_get_window(xwin)); 
    XMapWindow(gGui->display, xitk_window_get_window(xwin));
    XUnlockDisplay(gGui->display);

    xitk_set_layer_above(xitk_window_get_window(xwin));
    xitk_image_free_image(gGui->imlib_data, &xim);
  }
}

void splash_destroy(void) {

  if(xwin) {
    XLockDisplay(gGui->display);
    XUnmapWindow(gGui->display, xitk_window_get_window(xwin)); 
    XUnlockDisplay(gGui->display);
    
    xitk_window_destroy_window(gGui->imlib_data, xwin);
    xwin = NULL;
  }
}
