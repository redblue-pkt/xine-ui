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
#include "checkbox.h"
#include "widget_types.h"
#include "_xitk.h"

/*
 *
 */
static void paint_checkbox (widget_t *c, Window win, GC gc) {
  checkbox_private_data_t *private_data = 
    (checkbox_private_data_t *) c->private_data;
  int          checkbox_width;
  gui_image_t *skin;
  
  if ((c->widget_type & WIDGET_TYPE_CHECKBOX) && c->visible) {
    
    skin           = private_data->skin;
    checkbox_width = skin->width / 3;
    
    XLOCK (private_data->display);
    
    if (private_data->cArmed) {
      if (private_data->cClicked) { //click
	XCopyArea (private_data->display, skin->image, 
		   win, gc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, c->x, c->y);
      }
      else {
	if(!private_data->cState) //focus
	  XCopyArea (private_data->display, skin->image, 
		     win, gc, checkbox_width, 0,
		     checkbox_width, skin->height, c->x, c->y);
      }
    } else {
      if(private_data->cState) //click
	XCopyArea (private_data->display, skin->image, 
		   win, gc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, c->x, c->y);
      else  //normal
	XCopyArea (private_data->display, skin->image,  win, gc, 0, 0,
		   checkbox_width, skin->height, c->x, c->y);
    }

    XUNLOCK (private_data->display);
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint checkbox something (%d) "
	     "that is not a checkbox\n", c->widget_type);
  
#endif
}

/*
 *
 */
static int notify_click_checkbox (widget_list_t *wl, widget_t *c, 
				  int cUp, int x, int y) {
  checkbox_private_data_t *private_data = 
    (checkbox_private_data_t *) c->private_data;
  int bRepaint = 0;
  
  if (c->widget_type & WIDGET_TYPE_CHECKBOX) {

    private_data->cClicked = !cUp;
    if (cUp && private_data->cArmed) {
      private_data->cState = !private_data->cState;
      bRepaint = paint_widget_list (wl);
      if(private_data->callback) {
	private_data->callback(private_data->cWidget, 
			       private_data->userdata,
			       private_data->cState);
      }
    }
    if(!bRepaint) 
      paint_widget_list (wl);
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click checkbox on something (%d) "
	     "that is not a checkbox\n", c->widget_type);
#endif

  return 1;
}

/*
 *
 */
static int notify_focus_checkbox (widget_list_t *wl, 
				  widget_t *c, int cEntered) {
  checkbox_private_data_t *private_data = 
    (checkbox_private_data_t *) c->private_data;
  
  if (c->widget_type & WIDGET_TYPE_CHECKBOX)
    private_data->cArmed = cEntered;
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify focus checkbox on something (%d) "
	     "that is not a checkbox\n", c->widget_type);
#endif

  return 1;
}

/*
 *
 */
int checkbox_get_state(widget_t *c) {
  checkbox_private_data_t *private_data = 
    (checkbox_private_data_t *) c->private_data;
  
  if (c->widget_type & ~WIDGET_TYPE_CHECKBOX) {
#ifdef DEBUG_GUI
    fprintf (stderr, "notify click checkbox on something (%d) "
	     "that is not a checkbox\n", c->widget_type);
#endif
  }

  return private_data->cState;
}

/*
 *
 */
void checkbox_set_state(widget_t *c, int state, Window win, GC gc) {
  checkbox_private_data_t *private_data = 
    (checkbox_private_data_t *) c->private_data;
  int clk, arm;

  if (c->widget_type & WIDGET_TYPE_CHECKBOX) {
    if(checkbox_get_state(c) != state) {
      arm = private_data->cArmed;
      clk = private_data->cClicked;

      private_data->cArmed = 1;
      private_data->cClicked = 1;
      private_data->cState = state;

      paint_checkbox(c, win, gc);

      private_data->cArmed = arm;
      private_data->cClicked = clk;

      paint_checkbox(c, win, gc);
    }
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click checkbox on something (%d) "
	     "that is not a checkbox\n", c->widget_type);
#endif
}

/*
 *
 */
widget_t *checkbox_create (xitk_checkbox_t *cb) {
  widget_t *mywidget;
  checkbox_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));

  private_data = (checkbox_private_data_t *) 
    gui_xmalloc (sizeof (checkbox_private_data_t));

  private_data->display   = cb->display;

  private_data->cWidget   = mywidget;
  private_data->cClicked  = 0;
  private_data->cState    = 0;
  private_data->cArmed    = 0;
  private_data->skin      = gui_load_image(cb->imlibdata, cb->skin);
  private_data->callback  = cb->callback;
  private_data->userdata  = cb->userdata;

  mywidget->private_data = private_data;

  mywidget->enable          = 1;
  mywidget->running         = 1;
  mywidget->visible         = 1;
  mywidget->have_focus      = FOCUS_LOST;
  mywidget->x               = cb->x;
  mywidget->y               = cb->y;
  mywidget->width           = private_data->skin->width/3;
  mywidget->height          = private_data->skin->height;
  mywidget->widget_type     = WIDGET_TYPE_CHECKBOX;
  mywidget->paint           = paint_checkbox;
  mywidget->notify_click    = notify_click_checkbox;
  mywidget->notify_focus    = notify_focus_checkbox;
  mywidget->notify_keyevent = NULL;

  return mywidget;
}
