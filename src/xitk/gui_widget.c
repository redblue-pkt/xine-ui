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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "gui_widget.h"
#include "gui_list.h"
extern uint32_t xine_debug;

int paint_widget_list (widget_list_t *wl) {
  widget_t *mywidget;

  mywidget = (widget_t *) gui_list_first_content (wl->l);
  while (mywidget && wl && wl->l && wl->win && wl->gc) {

    if(mywidget->paint)
      (mywidget->paint) (mywidget, wl->win, wl->gc);

    mywidget = (widget_t *) gui_list_next_content (wl->l); 
  }
  return 1;
}

int is_inside_widget (widget_t *widget, int x, int y) {
  return ((x >= widget->x) && (x <= (widget->x + widget->width))
	  && (y >= widget->y) && (y <= (widget->y + widget->height)));
}

widget_t *get_widget_at (widget_list_t *wl, int x, int y) {
  widget_t *mywidget;

  mywidget = (widget_t *) gui_list_first_content (wl->l);
  while (mywidget) {
    if (is_inside_widget (mywidget, x, y))
      return mywidget;

    mywidget = (widget_t *) gui_list_next_content (wl->l);
  }
  return NULL;
}

void motion_notify_widget_list (widget_list_t *wl,
				int x, int y) {

  int bRepaint = 0;
  widget_t *mywidget = get_widget_at (wl, x, y);

  if (mywidget != wl->focusedWidget) {
      if (wl->focusedWidget) {
	if (wl->focusedWidget->notify_focus 
	    && wl->focusedWidget->enable == WIDGET_ENABLE)
	  bRepaint |= (wl->focusedWidget->notify_focus) (wl, wl->focusedWidget, FOCUS_LOST);
      }
      
      wl->focusedWidget = mywidget;
      
      if (mywidget) {
	if (mywidget->notify_focus && mywidget->enable == WIDGET_ENABLE)
	  bRepaint |= (mywidget->notify_focus) (wl, mywidget, FOCUS_RECEIVED);
      }
    }

  if (bRepaint)
    paint_widget_list (wl);
}

int click_notify_widget_list (widget_list_t *wl, 
			       int x, int y, int bUp) {

  int bRepaint = 0;
  widget_t *mywidget = get_widget_at (wl, x, y);

  motion_notify_widget_list (wl, x, y);

  if (!bUp) {
      
    wl->pressedWidget = mywidget;
    
    if (mywidget) {
      if (mywidget->notify_click && mywidget->enable == WIDGET_ENABLE)
	bRepaint |= (mywidget->notify_click) (wl, mywidget, LBUTTON_DOWN, x, y);
    }
  } else {
    if (wl->pressedWidget) {
      if (wl->pressedWidget->notify_click 
	  && wl->pressedWidget->enable == WIDGET_ENABLE)
	bRepaint |= (wl->pressedWidget->notify_click) (wl, wl->pressedWidget, LBUTTON_UP, x, y);
    }
  }
  
  return((bRepaint == 1));
  /*
  if (bRepaint && wl && wl->win && wl->gc) {
    paint_widget_list (wl);
    return 1;
  } else
    return 0;
  */
}

int widget_get_width(widget_t *lb) {
  
  return lb->width;  
}

int widget_get_height(widget_t *lb) {
  
  return lb->height;  
}

int widget_enabled(widget_t *w) {

  return (w->enable == WIDGET_ENABLE);
}

void widget_enable(widget_t *w) {

  w->enable = WIDGET_ENABLE;
}

void widget_disable(widget_t *w) {

  w->enable = ~WIDGET_ENABLE;
}
