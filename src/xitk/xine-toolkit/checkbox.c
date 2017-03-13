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
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

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
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

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
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

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

/*
 *
 */
static void paint_checkbox (xitk_widget_t *w) {
  checkbox_private_data_t *private_data;
  GC                       lgc;
  int                      checkbox_width;
  xitk_image_t            *skin;
  
  if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX) && w->visible == 1)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    skin           = private_data->skin;
    checkbox_width = skin->width / 3;
    
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
      if (private_data->cClicked || private_data->cState) { // focused, clicked or checked
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		   w->wl->win, lgc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
      else { // focused, unchecked
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap,
		   w->wl->win, lgc, checkbox_width, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
    } 
    else {
      if(private_data->cState) { // unfocused, checked
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		   w->wl->win, lgc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
      else { // unfocused, unchecked
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, w->wl->win, lgc, 0, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
    }
    XUNLOCK (private_data->imlibdata->x.disp);

    XLOCK (private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, lgc);
    XUNLOCK(private_data->imlibdata->x.disp);
  }

}

/*
 *
 */
static int notify_click_checkbox (xitk_widget_t *w, int button, int cUp, int x, int y) {
  checkbox_private_data_t *private_data;
  int                      ret = 0;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    if(button == Button1) {
      private_data = (checkbox_private_data_t *) w->private_data;
      
      private_data->cClicked = !cUp;
      if (cUp && (private_data->focus == FOCUS_RECEIVED)) {
	private_data->cState = !private_data->cState;
	
	if(private_data->callback) {
	  private_data->callback(private_data->cWidget, 
				 private_data->userdata,
				 private_data->cState);
	}
      }
      
      paint_checkbox(w);
      ret = 1;
    }
  }

  return ret;
}

/*
 *
 */
static int notify_focus_checkbox (xitk_widget_t *w, int focus) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    private_data->focus = focus;
  }
  return 1;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    
    if(private_data->skin_element_name) {
      xitk_skin_lock(skonfig);
      private_data->skin = xitk_skin_get_image(skonfig,
					       xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
      w->x               = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      w->y               = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->width           = private_data->skin->width/3;
      w->height          = private_data->skin->height;
      w->visible         = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1: -1;
      w->enable          = xitk_skin_get_enability(skonfig, private_data->skin_element_name);

      xitk_skin_unlock(skonfig);

      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint_checkbox(w);
    break;
  case WIDGET_EVENT_CLICK:
    result->value = notify_click_checkbox(w, event->button,
					  event->button_pressed, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_FOCUS:
    notify_focus_checkbox(w, event->focus);
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

void xitk_checkbox_callback_exec(xitk_widget_t *w) {
  checkbox_private_data_t *private_data;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    if(private_data->callback) {
      private_data->callback(private_data->cWidget, 
			     private_data->userdata,
			     private_data->cState);
    }
  }
}

/*
 *
 */
int xitk_checkbox_get_state(xitk_widget_t *w) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    return private_data->cState;
  }
  
  return 0;
}

/*
 *
 */
void xitk_checkbox_set_state(xitk_widget_t *w, int state) {
  checkbox_private_data_t *private_data;
  int                      clk, focus;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    if(xitk_checkbox_get_state(w) != state) {
      
      focus = private_data->focus;
      clk = private_data->cClicked;
      
      private_data->focus = FOCUS_RECEIVED;
      private_data->cClicked = 1;
      private_data->cState = state;

      paint_checkbox(w);

      private_data->focus = focus;
      private_data->cClicked = clk;

      paint_checkbox(w);
    }
  }

}

/*
 *
 */
static xitk_widget_t *_xitk_checkbox_create(xitk_widget_list_t *wl,
					    xitk_skin_config_t *skonfig, 
					    xitk_checkbox_widget_t *cb, int x, int y, 
                                            const char *skin_element_name, xitk_image_t *skin,
					    int visible, int enable) {
  xitk_widget_t *mywidget;
  checkbox_private_data_t *private_data;

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (checkbox_private_data_t *) 
    xitk_xmalloc (sizeof (checkbox_private_data_t));

  private_data->imlibdata         = cb->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(cb->skin_element_name);

  private_data->cWidget           = mywidget;
  private_data->cClicked          = 0;
  private_data->cState            = 0;
  private_data->focus             = FOCUS_LOST;

  private_data->skin              = skin;
  private_data->callback          = cb->callback;
  private_data->userdata          = cb->userdata;

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
  mywidget->type                  = WIDGET_TYPE_CHECKBOX | WIDGET_CLICKABLE | WIDGET_FOCUSABLE | WIDGET_KEYABLE;
  mywidget->event                 = notify_event;
  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;
  
  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_checkbox_create (xitk_widget_list_t *wl,
				     xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cb) {
  
  XITK_CHECK_CONSTITENCY(cb);

  return _xitk_checkbox_create(wl, skonfig, cb,
			       (xitk_skin_get_coord_x(skonfig, cb->skin_element_name)),
			       (xitk_skin_get_coord_y(skonfig, cb->skin_element_name)),
			       cb->skin_element_name,
			       (xitk_skin_get_image(skonfig,
						    xitk_skin_get_skin_filename(skonfig, cb->skin_element_name))),
			       (xitk_skin_get_visibility(skonfig, cb->skin_element_name)) ? 1 : -1,
			       xitk_skin_get_enability(skonfig, cb->skin_element_name));

}

/*
 *
 */
xitk_widget_t *xitk_noskin_checkbox_create(xitk_widget_list_t *wl,
					   xitk_checkbox_widget_t *cb,
					   int x, int y, int width, int height) {
  xitk_image_t  *i;
  
  XITK_CHECK_CONSTITENCY(cb);

  i = xitk_image_create_image(cb->imlibdata, width * 3, height);
  draw_checkbox_check(cb->imlibdata, i);
  
  return _xitk_checkbox_create(wl, NULL, cb, x, y, NULL, i, 0, 0);
}
