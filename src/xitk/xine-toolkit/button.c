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

#include "_xitk.h"

/*
 *
 */
static int notify_inside(xitk_widget_t *b, int x, int y) {
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;

  if((b->widget_type & WIDGET_TYPE_BUTTON) && b->visible) {
    xitk_image_t *skin = private_data->skin;

    return xitk_is_cursor_out_mask(private_data->display, b, skin->mask, x, y);
  }

  return 1;
}

/**
 *
 */
static void paint_button (xitk_widget_t *b, Window win, GC gc) {
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  GC                  lgc;
  int                 button_width;
  xitk_image_t        *skin;

  if((b->widget_type & WIDGET_TYPE_BUTTON) && b->visible) {

    skin         = private_data->skin;
    button_width = skin->width / 3;

    XLOCK (private_data->display);
        
    lgc = XCreateGC(private_data->display, win, None, None);
    XCopyGC(private_data->display, gc, (1 << GCLastBit) - 1, lgc);

    if (skin->mask) {
      XSetClipOrigin(private_data->display, lgc, b->x, b->y);
      XSetClipMask(private_data->display, lgc, skin->mask);
    }

    if(private_data->bArmed) {
      if(private_data->bClicked) {
	XCopyArea (private_data->display, skin->image,  
		   win, lgc, 2*button_width, 0,
		   button_width, skin->height, b->x, b->y);
	
      } else {
	XCopyArea (private_data->display, skin->image,  
		   win, lgc, button_width, 0,
		   button_width, skin->height, b->x, b->y);
      }
    } else {
      XCopyArea (private_data->display, skin->image, 
		 win, lgc, 0, 0,
		 button_width, skin->height, b->x, b->y);
    }

    XFreeGC(private_data->display, lgc);

    XUNLOCK (private_data->display);
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint button on something (%d) that is not a button\n",
	     b->widget_type);
#endif
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *b, xitk_skin_config_t *skonfig) {
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  
  if(b->widget_type & WIDGET_TYPE_BUTTON) {
    
    XITK_FREE_XITK_IMAGE(private_data->display, private_data->skin);
    private_data->skin              = xitk_load_image(private_data->imlibdata,
						      (xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name)));
    b->x                            = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
    b->y                            = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
    b->width                        = private_data->skin->width/3;
    b->height                       = private_data->skin->height;

    xitk_set_widget_pos(b, b->x, b->y);
  }
}

/*
 *
 */
static int notify_click_button (xitk_widget_list_t *wl, 
				xitk_widget_t *b,int bUp, int x, int y){
  button_private_data_t *private_data = 
    (button_private_data_t *) b->private_data;
  
  if (b->widget_type & WIDGET_TYPE_BUTTON) {
    private_data->bClicked = !bUp;
    
    paint_button(b, wl->win, wl->gc);

    if (bUp && private_data->bArmed) {
      if(private_data->callback) {
	private_data->callback(private_data->bWidget, 
			       private_data->userdata);
      }
    }

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
static int notify_focus_button (xitk_widget_list_t *wl, xitk_widget_t *b, int bEntered) {
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
xitk_widget_t *xitk_button_create (xitk_skin_config_t *skonfig, xitk_button_widget_t *b) {
  xitk_widget_t          *mywidget;
  button_private_data_t  *private_data;
  
  XITK_CHECK_CONSTITENCY(b);

  //  if(!b->skin_element_name) XITK_DIE("skin element name is required.\n");
  
  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  
  private_data = (button_private_data_t *) xitk_xmalloc(sizeof(button_private_data_t));
  
  private_data->display           = b->display;
  private_data->imlibdata         = b->imlibdata;
  
  private_data->bWidget           = mywidget;
  private_data->bClicked          = 0;
  private_data->bArmed            = 0;

  private_data->skin_element_name = strdup(b->skin_element_name);
  private_data->skin              = xitk_load_image(private_data->imlibdata,
						    (xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name)));
  
  private_data->callback          = b->callback;
  private_data->userdata          = b->userdata;
  
  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST; 

  mywidget->x                     = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
  mywidget->y                     = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
  
  mywidget->width                 = private_data->skin->width/3;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_BUTTON;
  mywidget->paint                 = paint_button;
  mywidget->notify_click          = notify_click_button;
  mywidget->notify_focus          = notify_focus_button;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_inside         = notify_inside;
  mywidget->notify_change_skin    = notify_change_skin;

  return mywidget;
}
