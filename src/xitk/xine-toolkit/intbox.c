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

#include <X11/Xlib.h>
#include <stdio.h>

#include "_xitk.h"

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (intbox_private_data_t *) w->private_data;
    
    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static void paint(xitk_widget_t *w) {
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (intbox_private_data_t *) w->private_data;
    
    if((w->visible == 1)) {
      int bx, ih, iw;

      iw = xitk_get_widget_width(private_data->input_widget);
      ih = xitk_get_widget_height(private_data->input_widget);
      xitk_set_widget_pos(private_data->input_widget, w->x, w->y);
      bx = w->x + iw;
      xitk_set_widget_pos(private_data->more_widget, bx, w->y);
      xitk_set_widget_pos(private_data->less_widget, bx, (w->y + (ih>>1)));

      xitk_show_widget(private_data->input_widget);
      xitk_show_widget(private_data->more_widget);
      xitk_show_widget(private_data->less_widget);
    }
    else {
      xitk_hide_widget(private_data->input_widget);
      xitk_hide_widget(private_data->more_widget);
      xitk_hide_widget(private_data->less_widget);
    }
  }
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    private_data = (intbox_private_data_t *) w->private_data;

    if(private_data->skin_element_name) {
      /*      
      int x, y;

      xitk_skin_lock(skonfig);

      // visibility && enability

      xitk_set_widget_pos(c, c->x, c->y);
      xitk_get_widget_pos(private_data->label_widget, &x, &y);
      x += xitk_get_widget_width(private_data->label_widget);
      
      (void) xitk_set_widget_pos(private_data->button_widget, x, y);

      xitk_skin_unlock(skonfig);
      */
    }
  }
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint(w);
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  }
  
  return retval;
}

/*
 *
 */
static void intbox_change_value(xitk_widget_t *x, void *data, char *string) {
  xitk_widget_t         *w = (xitk_widget_t *)data;
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    char  buf[256];
    
    private_data = (intbox_private_data_t *)w->private_data;
    private_data->value = strtol(string, &string, 10);
    
    memset(&buf, 0, sizeof(buf));
    snprintf(buf, 256, "%d", private_data->value);
    xitk_inputtext_change_text(private_data->input_widget, buf);
    if(private_data->force_value == 0)
      if(private_data->callback)
	private_data->callback(w, private_data->userdata, private_data->value);
  }
}

/*
 *
 */
void xitk_intbox_set_value(xitk_widget_t *w, int value) {
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    char buf[256];
    
    private_data = (intbox_private_data_t *) w->private_data;

    memset(&buf, 0, sizeof(buf));
    snprintf(buf, 256, "%d", value);
    private_data->force_value = 1;
    intbox_change_value(NULL, (void*)w, buf);
    private_data->force_value = 0;
  }
}

/*
 *
 */
int xitk_intbox_get_value(xitk_widget_t *w) {
  intbox_private_data_t *private_data;

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    char *strval;

    private_data = (intbox_private_data_t *)w->private_data;
    strval = xitk_inputtext_get_text(private_data->input_widget);
    private_data->value = strtol(strval, &strval, 10);
    
    return private_data->value;
  }
  return 0;
}

/*
 *
 */
static void intbox_stepdown(xitk_widget_t *x, void *data) {
  xitk_widget_t *w = (xitk_widget_t *) data;
  intbox_private_data_t *private_data;

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (intbox_private_data_t *)w->private_data;
    private_data->value -= private_data->step;
    xitk_intbox_set_value(w, private_data->value);
    if(private_data->callback)
      private_data->callback(w, private_data->userdata, private_data->value);
  }
}

/*
 *
 */
static void intbox_stepup(xitk_widget_t *x, void *data) {
  xitk_widget_t *w = (xitk_widget_t *) data;
  intbox_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (intbox_private_data_t *)w->private_data;
    private_data->value += private_data->step;
    xitk_intbox_set_value(w, private_data->value);
    if(private_data->callback)
      private_data->callback(w, private_data->userdata, private_data->value);
  }
}

/*
 *
 */
static xitk_widget_t *_xitk_intbox_create(xitk_widget_list_t *wl,
					  xitk_skin_config_t *skonfig,
					  xitk_intbox_widget_t *ib, char *skin_element_name,
					  xitk_widget_t *mywidget, 
					  intbox_private_data_t *private_data,
					  int visible, int enable) {
  
  private_data->imlibdata                = ib->imlibdata;
  private_data->skin_element_name        = (skin_element_name == NULL) ? NULL : strdup(skin_element_name);
  private_data->parent_wlist             = ib->parent_wlist;
  private_data->callback                 = ib->callback;
  private_data->userdata                 = ib->userdata;
  private_data->step                     = ib->step;
  private_data->value                    = ib->value;
  private_data->force_value              = 0;

  mywidget->private_data                 = private_data;

  mywidget->wl                           = wl;

  mywidget->enable                       = enable;
  mywidget->running                      = 1;
  mywidget->visible                      = visible;
  mywidget->have_focus                   = FOCUS_LOST;
  
  mywidget->imlibdata                    = private_data->imlibdata;

  mywidget->type                         = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_INTBOX;
  mywidget->event                        = notify_event;
  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_noskin_intbox_create(xitk_widget_list_t *wl,
					 xitk_intbox_widget_t *ib,
					 int x, int y, int width, int height, 
					 xitk_widget_t **iw, xitk_widget_t **mw, xitk_widget_t **lw) {
  xitk_widget_t              *mywidget;
  intbox_private_data_t      *private_data;
  xitk_button_widget_t        b;
  xitk_inputtext_widget_t     inp;

  XITK_CHECK_CONSTITENCY(ib);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  XITK_WIDGET_INIT(&b, ib->imlibdata);
  XITK_WIDGET_INIT(&inp, ib->imlibdata);

  private_data = (intbox_private_data_t *) xitk_xmalloc(sizeof(intbox_private_data_t));
  
  /* Create inputtext and buttons (not skinable) */
  {
    char          buf[256];
    xitk_image_t *wimage;

    memset(&buf, 0, sizeof(buf));
    snprintf(buf, 256, "%d", ib->value);

    inp.skin_element_name = NULL;
    inp.text              = buf; 
    inp.max_length        = 16;
    inp.callback          = intbox_change_value;
    inp.userdata          = (void *)mywidget;
    xitk_list_append_content(ib->parent_wlist->l, 
	     (private_data->input_widget = 
	      xitk_noskin_inputtext_create(ib->parent_wlist, &inp,
					   x, y, (width - 10), height,
					   "Black", "Black", DEFAULT_FONT_10)));
    private_data->input_widget->type |= WIDGET_GROUP | WIDGET_GROUP_INTBOX;
    
    b.skin_element_name = NULL;
    b.callback          = intbox_stepup;
    b.userdata          = (void *)mywidget;
    xitk_list_append_content(ib->parent_wlist->l, 
	     (private_data->more_widget = 
	      xitk_noskin_button_create(ib->parent_wlist, &b,
					(x + width) - (height>>1), y, 
					(height>>1), (height>>1))));
    private_data->more_widget->type |= WIDGET_GROUP | WIDGET_GROUP_INTBOX;

    b.skin_element_name = NULL;
    b.callback          = intbox_stepdown;
    b.userdata          = (void *)mywidget;
    xitk_list_append_content(ib->parent_wlist->l, 
	     (private_data->less_widget = 
	      xitk_noskin_button_create(ib->parent_wlist, &b,
					(x + width) - (height>>1), (y + (height>>1)),
					(height>>1), (height>>1))));
    private_data->less_widget->type |= WIDGET_GROUP | WIDGET_GROUP_INTBOX;

    /* Draw '+' and '-' in buttons */
    wimage = xitk_get_widget_foreground_skin(private_data->more_widget);
    
    if(wimage)
      draw_button_plus(ib->imlibdata, wimage);

    wimage = xitk_get_widget_foreground_skin(private_data->less_widget);
    
    if(wimage)
      draw_button_minus(ib->imlibdata, wimage);

  }

  if(iw)
    *iw = private_data->input_widget;
  if(mw)
    *mw = private_data->more_widget;
  if(lw)
    *lw = private_data->less_widget;
  
  mywidget->x = x;
  mywidget->y = y;
  mywidget->width = width;
  mywidget->height = height;
  
  return _xitk_intbox_create(wl, NULL, ib, NULL, mywidget, private_data, 1, 1);
}
