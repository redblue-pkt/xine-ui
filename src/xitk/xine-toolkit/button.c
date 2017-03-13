/* 
 * Copyright (C) 2000-2014 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "_xitk.h"

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  button_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (button_private_data_t *) w->private_data;
    
    if(!private_data->skin_element_name)
      xitk_image_free_image(private_data->imlibdata, &(private_data->skin));

    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  button_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (button_private_data_t *) w->private_data;
    
    if(sk == FOREGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *w, int x, int y) {
  button_private_data_t *private_data;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (button_private_data_t *) w->private_data;

    if(w->visible == 1) {
      xitk_image_t *skin = private_data->skin;
      
      if(skin->mask)
	return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, w, skin->mask->pixmap, x, y);
    }
    else 
      return 0;
  }
  
  return 1;
}

/**
 *
 */
static void paint_button (xitk_widget_t *w) {
  button_private_data_t *private_data;
  GC                     lgc;
  int                    button_width;
  xitk_image_t          *skin;

  if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON) && w->visible == 1)) {
    
    private_data = (button_private_data_t *) w->private_data;
    skin         = private_data->skin;
    button_width = skin->width / 3;

    XLOCK (private_data->imlibdata->x.disp);
    lgc = XCreateGC(private_data->imlibdata->x.disp, w->wl->win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, w->wl->gc, (1 << GCLastBit) - 1, lgc);
    XUNLOCK (private_data->imlibdata->x.disp);

    if (skin->mask) {
      XLOCK (private_data->imlibdata->x.disp);
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, w->x, w->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask->pixmap);
      XUNLOCK (private_data->imlibdata->x.disp);
    }

    XLOCK (private_data->imlibdata->x.disp);
    if ((private_data->focus == FOCUS_RECEIVED) || (private_data->focus == FOCUS_MOUSE_IN)) {
      if(private_data->bClicked) {
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap,  
		   w->wl->win, lgc, 2*button_width, 0,
		   button_width, skin->height, w->x, w->y);
      } 
      else {
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap,  
		   w->wl->win, lgc, button_width, 0,
		   button_width, skin->height, w->x, w->y);
      }
    } 
    else {
      XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		 w->wl->win, lgc, 0, 0,
		 button_width, skin->height, w->x, w->y);
    }
    XUNLOCK (private_data->imlibdata->x.disp);

    XLOCK (private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, lgc);
    XUNLOCK (private_data->imlibdata->x.disp);
  } 

}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  button_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (button_private_data_t *) w->private_data;
    
    if(private_data->skin_element_name) {
      xitk_skin_lock(skonfig);
      private_data->skin              = xitk_skin_get_image(skonfig, 
							    (xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name)));
      w->x                            = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      w->y                            = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->width                        = private_data->skin->width/3;
      w->height                       = private_data->skin->height;
      
      w->visible                      = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1 : -1;
      w->enable                       = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
    
      xitk_skin_unlock(skonfig);
      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

/*
 *
 */
static int notify_click_button (xitk_widget_t *w, int button, int bUp, int x, int y) {
  button_private_data_t *private_data;
  int                    ret = 0;
    
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    if(button == Button1) {
      private_data = (button_private_data_t *) w->private_data;
      private_data->bClicked = !bUp;
      
      paint_button(w);
      
      if (bUp && (private_data->focus == FOCUS_RECEIVED)) {
	if(private_data->callback) {
	  private_data->callback(private_data->bWidget, 
				 private_data->userdata);
	}
      }
      ret = 1;
    }
  }

  return ret;
}

/*
 *
 */
static int notify_focus_button (xitk_widget_t *w, int focus) {
  button_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (button_private_data_t *) w->private_data;
    private_data->focus = focus;
  } 
  
  return 1;
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;

  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint_button(w);
    break;
  case WIDGET_EVENT_CLICK:
    result->value = notify_click_button(w, event->button, 
					event->button_pressed, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_FOCUS:
    notify_focus_button(w, event->focus);
    break;
  case WIDGET_EVENT_INSIDE:
    result->value = notify_inside(w, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_GET_SKIN:
    if(result) {
      result->image = get_skin(w, event->skin_layer);
      retval = 1;
    }
    break;
  }
  
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_button_create (xitk_widget_list_t *wl,
					   xitk_skin_config_t *skonfig, xitk_button_widget_t *b,
					   int x, int y, 
                                           const char *skin_element_name, xitk_image_t *skin,
					   int visible, int enable) {
  xitk_widget_t          *mywidget;
  button_private_data_t  *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  
  private_data = (button_private_data_t *) xitk_xmalloc(sizeof(button_private_data_t));
  
  private_data->imlibdata         = b->imlibdata;
  
  private_data->bWidget           = mywidget;
  private_data->bClicked          = 0;
  private_data->focus             = FOCUS_LOST;

  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(b->skin_element_name);
  private_data->skin              = skin;
  
  private_data->callback          = b->callback;
  private_data->userdata          = b->userdata;
  
  mywidget->private_data          = private_data;

  mywidget->wl                    = wl;

  mywidget->enable                = enable;
  mywidget->running               = 1;
  mywidget->visible               = visible;
  mywidget->have_focus            = FOCUS_LOST; 
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width/3;
  mywidget->height                = private_data->skin->height;
  mywidget->type                  = WIDGET_TYPE_BUTTON | WIDGET_CLICKABLE | WIDGET_FOCUSABLE | WIDGET_KEYABLE;
  mywidget->event                 = notify_event;
  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_button_create (xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_button_widget_t *b) {
  
  XITK_CHECK_CONSTITENCY(b);

  return _xitk_button_create(wl, skonfig, b, 
			     (xitk_skin_get_coord_x(skonfig, b->skin_element_name)),
			     (xitk_skin_get_coord_y(skonfig, b->skin_element_name)),
			     b->skin_element_name,
			     xitk_skin_get_image(skonfig,
						 (xitk_skin_get_skin_filename(skonfig, b->skin_element_name))),
			     (xitk_skin_get_visibility(skonfig, b->skin_element_name)) ? 1 : -1,
			     xitk_skin_get_enability(skonfig, b->skin_element_name));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_button_create (xitk_widget_list_t *wl,
					  xitk_button_widget_t *b,
					  int x, int y, int width, int height) {
  xitk_image_t *i;

  XITK_CHECK_CONSTITENCY(b);

  i = xitk_image_create_image(b->imlibdata, width * 3, height);
  draw_bevel_three_state(b->imlibdata, i);

  return _xitk_button_create(wl, NULL, b, x, y, NULL, i, 0, 0);
}
