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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "button.h"
#include "widget_types.h"

/**
 *
 */
static void paint_button (widget_t *b,  Window win, GC gc) {
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  int          button_width;
  gui_image_t *skin;

  skin = private_data->skin;

  button_width = skin->width / 3;
  
  XLockDisplay (private_data->display);

  if (b->widget_type & WIDGET_TYPE_BUTTON) {
    
    if (private_data->bArmed) {
      if (private_data->bClicked) {
	XCopyArea (private_data->display, skin->image,  
		   win, gc, 2*button_width, 0,
		   button_width, skin->height, b->x, b->y);
	
      } else {
	XCopyArea (private_data->display, skin->image,  
		   win, gc, button_width, 0,
		   button_width, skin->height, b->x, b->y);
      }
    } else {
      XCopyArea (private_data->display, skin->image,  win, gc, 0, 0,
		 button_width, skin->height, b->x, b->y);
    }

  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint button on something (%d) that is not a button\n",
	     b->widget_type);
#endif
  
  XUnlockDisplay (private_data->display);
  XSync (private_data->display, False);
}

/*
 *
 */
static int notify_click_button (widget_list_t *wl, 
				widget_t *b,int bUp, int x, int y){
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  int bRepaint = 0;
  
  if (b->widget_type & WIDGET_TYPE_BUTTON) {
    private_data->bClicked = !bUp;
    
    if (bUp && private_data->bArmed) {
      bRepaint = paint_widget_list (wl);
      if(private_data->function) {
	private_data->function (private_data->bWidget, 
				private_data->user_data);
      }
    }

    if(!bRepaint) 
      paint_widget_list (wl);
    
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click button on something (%d) that is not"
	     " a button\n", b->widget_type);
#endif

  return 1;
}

/*
 *
 */
static int notify_focus_button (widget_list_t *wl, widget_t *b, int bEntered) {
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  
  if (b->widget_type & WIDGET_TYPE_BUTTON) {

    private_data->bArmed = bEntered;
    
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify focus button on something (%d) that is not"
	     " a button\n", b->widget_type);
#endif
  
  return 1;
}

/*
 *
 */
widget_t *create_button (Display *display, ImlibData *idata,
			 int x, int y, void* f, void* ud, const char *skin) {
  widget_t              *mywidget;
  button_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));

  private_data = (button_private_data_t *) gui_xmalloc(sizeof(button_private_data_t));

  private_data->display   = display;

  private_data->bWidget   = mywidget;
  private_data->bClicked  = 0;
  private_data->bArmed    = 0;
  private_data->skin      = gui_load_image(idata, skin);
  private_data->function  = f;
  private_data->user_data = ud;


  mywidget->private_data = private_data;

  mywidget->enable       = 1;

  mywidget->x            = x;
  mywidget->y            = y;
  mywidget->width        = private_data->skin->width/3;
  mywidget->height       = private_data->skin->height;
  mywidget->widget_type  = WIDGET_TYPE_BUTTON;
  mywidget->paint        = paint_button;
  mywidget->notify_click = notify_click_button;
  mywidget->notify_focus = notify_focus_button;

  return mywidget;
}
