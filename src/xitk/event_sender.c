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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "event.h"
#include "actions.h"
#include "panel.h"
#include "errors.h"
#include "utils.h"
#include "i18n.h"

#include "xitk.h"

#define WINDOW_WIDTH        240
#define WINDOW_HEIGHT       296

extern gGui_t          *gGui;

static char            *eventerfontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

typedef struct {
  xitk_window_t        *xwin;

  int                   x;
  int                   y;
  int                   move_forced;

  xitk_widget_list_t   *widget_list;

  struct {
    xitk_widget_t      *left;
    xitk_widget_t      *right;
    xitk_widget_t      *up;
    xitk_widget_t      *down;
    xitk_widget_t      *select;
  } navigator;

  struct {
    xitk_widget_t      *menu[7];
  } menus;
  
  struct {
    xitk_widget_t      *prev;
    xitk_widget_t      *next;
  } angles;

  struct {
    xitk_widget_t      *number[11];
  } numbers;


  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

} _eventer_t;

static _eventer_t    *eventer = NULL;

void event_sender_sticky_cb(void *data, xine_cfg_entry_t *cfg) {
  
  if(!eventer)
    gGui->eventer_sticky = cfg->num_value;
  else {
    int old_sticky_value = gGui->eventer_sticky;
    gGui->eventer_sticky = cfg->num_value;
    
    if((!old_sticky_value) && gGui->eventer_sticky) {
      int  px, py, pw, ph;
      xitk_get_window_position(gGui->display, gGui->panel_window, &px, &py, &pw, &ph);
      eventer->x = px + pw;
      eventer->y = py;
      eventer->move_forced++;
      xitk_window_move_window(gGui->imlib_data, eventer->xwin, eventer->x, eventer->y);
    }
  }

}

static void event_sender_store_new_position(int x, int y, int w, int h) {

  if(gGui->eventer_sticky) {
    if(eventer->move_forced == 0) {
      if(panel_is_visible()) {
	eventer->move_forced++;
	xitk_window_move_window(gGui->imlib_data, eventer->xwin, eventer->x, eventer->y);
      }
    }
    else
      eventer->move_forced--;
  }
  else {
    eventer->x = x;
    eventer->y = y;
    config_update_num ("gui.eventer_x", x);
    config_update_num ("gui.eventer_y", y);
  }
  
}

/* Send given event to xine engine */
static void event_sender_send(int event) {
  xine_event_t   xine_event;

  xine_event.type = event;
  
  if(xine_event.type != 0)
    xine_event_send(gGui->stream, &xine_event);
}

static void event_sender_up(xitk_widget_t *w, void *data) {
  event_sender_send(XINE_EVENT_INPUT_UP);
}

static void event_sender_down(xitk_widget_t *w, void *data) {
  event_sender_send(XINE_EVENT_INPUT_DOWN);
}

static void event_sender_left(xitk_widget_t *w, void *data) {
  event_sender_send(XINE_EVENT_INPUT_LEFT);
}

static void event_sender_right(xitk_widget_t *w, void *data) {
  event_sender_send(XINE_EVENT_INPUT_RIGHT);
}

static void event_sender_select(xitk_widget_t *w, void *data) {
  event_sender_send(XINE_EVENT_INPUT_SELECT);
}

static void event_sender_menu(xitk_widget_t *w, void *data) {
  int event     = (int) data;
  int events[7] = {
    XINE_EVENT_INPUT_MENU1, XINE_EVENT_INPUT_MENU2, XINE_EVENT_INPUT_MENU3,
    XINE_EVENT_INPUT_MENU4, XINE_EVENT_INPUT_MENU5, XINE_EVENT_INPUT_MENU6,
    XINE_EVENT_INPUT_MENU7
  };

  event_sender_send(events[event]);
}

static void event_sender_num(xitk_widget_t *w, void *data) {
  int event      = (int) data;
  int events[11] = { 
    XINE_EVENT_INPUT_NUMBER_0, XINE_EVENT_INPUT_NUMBER_1, XINE_EVENT_INPUT_NUMBER_2,
    XINE_EVENT_INPUT_NUMBER_3, XINE_EVENT_INPUT_NUMBER_4, XINE_EVENT_INPUT_NUMBER_5,
    XINE_EVENT_INPUT_NUMBER_6, XINE_EVENT_INPUT_NUMBER_7, XINE_EVENT_INPUT_NUMBER_8,
    XINE_EVENT_INPUT_NUMBER_9, XINE_EVENT_INPUT_NUMBER_10_ADD
  };

  event_sender_send(events[event]);
}

static void event_sender_angle(xitk_widget_t *w, void *data) {
  int event      = (int) data;
  int events[11] = { 
    XINE_EVENT_INPUT_ANGLE_NEXT, XINE_EVENT_INPUT_ANGLE_PREVIOUS
  };

  event_sender_send(events[event]);
}

void event_sender_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;

  eventer->running = 0;
  eventer->visible = 0;

  if((xitk_get_window_info(eventer->widget_key, &wi))) {
    config_update_num ("gui.eventer_x", wi.x);
    config_update_num ("gui.eventer_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  xitk_unregister_event_handler(&eventer->widget_key);

  XLockDisplay(gGui->display);
  XUnmapWindow(gGui->display, xitk_window_get_window(eventer->xwin));
  XUnlockDisplay(gGui->display);

  xitk_destroy_widgets(eventer->widget_list);

  XLockDisplay(gGui->display);
  XDestroyWindow(gGui->display, xitk_window_get_window(eventer->xwin));
  XUnlockDisplay(gGui->display);

  eventer->xwin = None;
  xitk_list_free(eventer->widget_list->l);
  
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, eventer->widget_list->gc);
  XUnlockDisplay(gGui->display);

  free(eventer->widget_list);

  free(eventer);
  eventer = NULL;
}

int event_sender_is_visible(void) {
  
  if(eventer != NULL)
    return eventer->visible;
  
  return 0;
}

int event_sender_is_running(void) {
  
  if(eventer != NULL)
    return eventer->running;
  
  return 0;
}

void event_sender_toggle_visibility(xitk_widget_t *w, void *data) {
  if(eventer != NULL) {
    if (eventer->visible && eventer->running) {
      eventer->visible = 0;
      xitk_hide_widgets(eventer->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, xitk_window_get_window(eventer->xwin));
      XUnlockDisplay(gGui->display);
    } else {
      if(eventer->running) {
	eventer->visible = 1;
	xitk_show_widgets(eventer->widget_list);
	XLockDisplay(gGui->display);
	XMapRaised(gGui->display, xitk_window_get_window(eventer->xwin)); 
	XSetTransientForHint (gGui->display, 
			      xitk_window_get_window(eventer->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(eventer->xwin));
      }
    }
  }
}

void event_sender_raise_window(void) {
  if(eventer != NULL) {
    if(eventer->xwin) {
      if(eventer->visible && eventer->running) {
	if(eventer->running) {
	  XLockDisplay(gGui->display);
	  XMapRaised(gGui->display, xitk_window_get_window(eventer->xwin));
	  eventer->visible = 1;
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(eventer->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(eventer->xwin));
	}
      } else {
	XLockDisplay(gGui->display);
	XUnmapWindow (gGui->display, xitk_window_get_window(eventer->xwin));
	XUnlockDisplay(gGui->display);
	eventer->visible = 0;
      }
    }
  }
}

void event_sender_move(int x, int y) {

  if(eventer != NULL) {
    if(gGui->eventer_sticky) {
      eventer->x = x;
      eventer->y = y;
      config_update_num ("gui.eventer_x", x);
      config_update_num ("gui.eventer_y", y);
      xitk_window_move_window(gGui->imlib_data, eventer->xwin, x, y);
    }
  }
}

static void event_sender_end(xitk_widget_t *w, void *data) {
  event_sender_exit(NULL, NULL);
}

void event_sender_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  int                        x, y, i;

  /* this shouldn't happen */
  if(eventer != NULL) {
    if(eventer->xwin)
      return;
  }
  
  eventer = (_eventer_t *) xine_xmalloc(sizeof(_eventer_t));

  eventer->x = xine_config_register_num (gGui->xine, "gui.eventer_x", 
					 100,
					 CONFIG_NO_DESC,
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_BEG,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
  eventer->y = xine_config_register_num (gGui->xine, "gui.eventer_y",
					 100,
					 CONFIG_NO_DESC,
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_BEG,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
  
    if(gGui->eventer_sticky) {
    int  px, py, pw, ph;
    xitk_get_window_position(gGui->display, gGui->panel_window, &px, &py, &pw, &ph);
    eventer->x = px + pw;
    eventer->y = py;
  }

  /* Create window */
  eventer->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						   _("event sender"), eventer->x, eventer->y,
						   WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);

  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(eventer->xwin)), None, None);
  
  eventer->widget_list                = xitk_widget_list_new();
  eventer->widget_list->l             = xitk_list_new ();
  eventer->widget_list->win           = (xitk_window_get_window(eventer->xwin));
  eventer->widget_list->gc            = gc;

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  x = 85;
  y = 30;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Up");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_up; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l,  
	   (eventer->navigator.up = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   eventerfontname)));
  
  x -= 70;
  y += 40;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Left");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_left; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->navigator.left = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   eventerfontname)));

  x += 75;
  y += 5;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Select");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_select; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->navigator.select = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 60, 30,
					   "Black", "Black", "White", 
					   eventerfontname)));

  x += 65;
  y -= 5;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Right");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_right; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->navigator.right = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   eventerfontname)));

  x -= 70;
  y += 40;
 
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Down");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_down; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->navigator.down = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   eventerfontname)));

  x = 15;
  y = 120;

  for(i = 0; i < 7; i++) {
    char number[7];

    memset(&number, 0, sizeof(number));
    sprintf(number, _("Menu %d"), (i + 1));
    lb.button_type       = CLICK_BUTTON;
    lb.label             = number;
    lb.align             = LABEL_ALIGN_CENTER;
    lb.callback          = event_sender_menu; 
    lb.state_callback    = NULL;
    lb.userdata          = (void *)i;
    lb.skin_element_name = NULL;
    xitk_list_append_content(eventer->widget_list->l, 
	     (eventer->menus.menu[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 60, 23,
					     "Black", "Black", "White", eventerfontname)));
    y += 23;
  }

  x = 165; 
  y = 189;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Angle -");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_angle; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)0;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->angles.prev = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 60, 23,
					   "Black", "Black", "White", eventerfontname)));
  y += 23;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Angle +");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_angle; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)1;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
	   (eventer->angles.next = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 60, 23,
					   "Black", "Black", "White", eventerfontname)));


    

  x = 85 + 46;
  y = 189;

  for(i = 9; i >= 0; i--) {
    char number[2];
    
    memset(&number, 0, sizeof(number));
    sprintf(number, "%d", i);

    lb.button_type       = CLICK_BUTTON;
    lb.label             = number;
    lb.align             = LABEL_ALIGN_CENTER;
    lb.callback          = event_sender_num;
    lb.state_callback    = NULL;
    lb.userdata          = (void *)i;
    lb.skin_element_name = NULL;

    if(!i)
      x -= 46;

    xitk_list_append_content(eventer->widget_list->l, 
	     (eventer->numbers.number[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 23, 23,
					     "Black", "Black", "White", eventerfontname)));

    if(!((i - 1) % 3)) {
      y += 23;
      x += 46;
    }
    else
      x -= 23;

  }

  x += 46;

  {
    char number[5];
    
    i = 10;
    
    memset(&number, 0, sizeof(number));
    sprintf(number, "+ %d", i);
    
    lb.button_type       = CLICK_BUTTON;
    lb.label             = number;
    lb.align             = LABEL_ALIGN_CENTER;
    lb.callback          = event_sender_num;
    lb.state_callback    = NULL;
    lb.userdata          = (void *)i;
    lb.skin_element_name = NULL;
    
    xitk_list_append_content(eventer->widget_list->l, 
	     (eventer->numbers.number[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 46, 23,
					     "Black", "Black", "White", eventerfontname)));
  }

  x = WINDOW_WIDTH - 75;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = event_sender_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(eventer->widget_list->l, 
   xitk_noskin_labelbutton_create(eventer->widget_list, 
				  &lb, x, WINDOW_HEIGHT - (23 + 15), 60, 23,
				  "Black", "Black", "White", eventerfontname));
  
  XMapRaised(gGui->display, xitk_window_get_window(eventer->xwin));
  XUnlockDisplay (gGui->display);

  eventer->widget_key = xitk_register_event_handler("eventer", 
						    (xitk_window_get_window(eventer->xwin)),
						    NULL,
						    event_sender_store_new_position,
						    NULL,
						    eventer->widget_list,
						    NULL);
  

  eventer->visible = 1;
  eventer->running = 1;
  eventer->move_forced = 0;
  event_sender_raise_window();


  XLockDisplay (gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(eventer->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay (gGui->display);

}
