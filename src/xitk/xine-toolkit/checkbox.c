/* 
 * Copyright (C) 2000-2002 the xine project
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
static void notify_destroy(xitk_widget_t *w, void *data) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->skin);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
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
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    if((w->visible == 1)) {
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
static void paint_checkbox (xitk_widget_t *w, Window win, GC gc) {
  checkbox_private_data_t *private_data;
  GC                       lgc;
  int                      checkbox_width;
  xitk_image_t            *skin;
  
  if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX) && (w->visible == 1))) {
    private_data = (checkbox_private_data_t *) w->private_data;

    skin           = private_data->skin;
    checkbox_width = skin->width / 3;
    
    XLOCK (private_data->imlibdata->x.disp);
    lgc = XCreateGC(private_data->imlibdata->x.disp, win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, lgc);
    XUNLOCK (private_data->imlibdata->x.disp);
    
    if (skin->mask) {
      XLOCK (private_data->imlibdata->x.disp);
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, w->x, w->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask->pixmap);
      XUNLOCK (private_data->imlibdata->x.disp);
    }

    XLOCK (private_data->imlibdata->x.disp);
    if ((private_data->focus == FOCUS_RECEIVED) || (private_data->focus == FOCUS_MOUSE_IN)) {
      if (private_data->cClicked) { //click
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		   win, lgc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
      else {
	if(!private_data->cState) //focus
	  XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		     win, lgc, checkbox_width, 0,
		     checkbox_width, skin->height, w->x, w->y);
      }
    } 
    else {
      if(private_data->cState) { //click
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		   win, lgc, 2*checkbox_width, 0,
		   checkbox_width, skin->height, w->x, w->y);
      }
      else { //normal
	XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, win, lgc, 0, 0,
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
static int notify_click_checkbox (xitk_widget_list_t *wl, 
				  xitk_widget_t *w, int cUp, int x, int y) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    
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
    
    paint_checkbox(w, wl->win, wl->gc);
  }

  return 1;
}

/*
 *
 */
static int notify_focus_checkbox (xitk_widget_list_t *wl, xitk_widget_t *w, int focus) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    private_data->focus = focus;
  }
  return 1;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    
    if(private_data->skin_element_name) {
      xitk_skin_lock(skonfig);
      xitk_image_free_image(private_data->imlibdata, &private_data->skin);
      private_data->skin = xitk_image_load_image(private_data->imlibdata,
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

/*
 *
 */
int xitk_checkbox_get_state(xitk_widget_t *w) {
  checkbox_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;
    return private_data->cState;
  }
  
  return 0;
}

/*
 *
 */
void xitk_checkbox_set_state(xitk_widget_t *w, int state, Window win, GC gc) {
  checkbox_private_data_t *private_data;
  int                      clk, focus;

  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    private_data = (checkbox_private_data_t *) w->private_data;

    if(xitk_checkbox_get_state(w) != state) {
      
      focus = private_data->focus;
      clk = private_data->cClicked;
      
      private_data->focus = FOCUS_RECEIVED;
      private_data->cClicked = 1;
      private_data->cState = state;

      paint_checkbox(w, win, gc);

      private_data->focus = focus;
      private_data->cClicked = clk;

      paint_checkbox(w, win, gc);
    }
  }

}

/*
 *
 */
static xitk_widget_t *_xitk_checkbox_create(xitk_widget_list_t *wl,
					    xitk_skin_config_t *skonfig, 
					    xitk_checkbox_widget_t *cb, int x, int y, 
					    char *skin_element_name, xitk_image_t *skin,
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

  mywidget->widget_list           = wl;

  mywidget->enable                = enable;
  mywidget->running               = 1;
  mywidget->visible               = visible;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width/3;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_CHECKBOX;
  mywidget->paint                 = paint_checkbox;
  mywidget->notify_click          = notify_click_checkbox;
  mywidget->notify_focus          = notify_focus_checkbox;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_inside         = notify_inside;
  mywidget->notify_change_skin    = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy        = notify_destroy;
  mywidget->get_skin              = get_skin;
  mywidget->notify_enable         = NULL;

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
			       (xitk_image_load_image(cb->imlibdata,
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
  draw_bevel_three_state(cb->imlibdata, i);
  
  return _xitk_checkbox_create(wl, NULL, cb, x, y, NULL, i, 1, 1);
}
