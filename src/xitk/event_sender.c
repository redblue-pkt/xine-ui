/*
 * Copyright (C) 2000-2017 the xine project
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "common.h"

#define WINDOW_WIDTH    250
#define WINDOW_HEIGHT   221


typedef struct {
  xitk_window_t        *xwin;

  int                   x;
  int                   y;

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

/* Initialized below */
static char *menu_items_default[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char *menu_items_dvd[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char *menu_items_bd[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

void event_sender_sticky_cb(void *data, xine_cfg_entry_t *cfg) {
  int old_sticky_value = gGui->eventer_sticky;
  
  gGui->eventer_sticky = cfg->num_value;
  if(eventer) {
    if((!old_sticky_value) && gGui->eventer_sticky) {
      int  px, py, pw;
      xitk_get_window_position(gGui->display, gGui->panel_window, &px, &py, &pw, NULL);
      eventer->x = px + pw;
      eventer->y = py;
      xitk_window_move_window(gGui->imlib_data, eventer->xwin, eventer->x, eventer->y);
    }
  }
}

static void event_sender_store_new_position(int x, int y, int w, int h) {

  if(eventer && !gGui->eventer_sticky) {
    eventer->x = x;
    eventer->y = y;
    config_update_num ("gui.eventer_x", x);
    config_update_num ("gui.eventer_y", y);
  }
}

void event_sender_show_tips(int enabled, unsigned long timeout) {
  
  if(eventer) {
    if(enabled)
      xitk_set_widgets_tips_timeout(eventer->widget_list, timeout);
    else
      xitk_disable_widgets_tips(eventer->widget_list);
  }
}

void event_sender_update_tips_timeout(unsigned long timeout) {
  if(eventer)
    xitk_set_widgets_tips_timeout(eventer->widget_list, timeout);
}

/* Send given event to xine engine */
void event_sender_send (int event) {
  xine_event_t   xine_event;

  if (!event)
    return;

  xine_event.type        = event;
  xine_event.data_length = 0;
  xine_event.data        = NULL;
  xine_event.stream      = gGui->stream;
  gettimeofday(&xine_event.tv, NULL);
  xine_event_send (gGui->stream, &xine_event);
}

static void event_sender_send2 (xitk_widget_t *w, void *data) {
  int event = (long int)data;
  xine_event_t   xine_event;

  xine_event.type        = event;
  xine_event.data_length = 0;
  xine_event.data        = NULL;
  xine_event.stream      = gGui->stream;
  gettimeofday(&xine_event.tv, NULL);
  xine_event_send (gGui->stream, &xine_event);
}

static void event_sender_exit(xitk_widget_t *, void *);

static void event_sender_handle_event(XEvent *event, void *data) {
  xine_event_t  xine_event;

  switch(event->type) {

  case ButtonRelease:
    /*
     * If we tried to move sticky window, move it back to stored position.
     */
    if(eventer && gGui->eventer_sticky) {
      if(panel_is_visible()) {
	int  x, y;
	xitk_window_get_window_position(gGui->imlib_data, eventer->xwin, &x, &y, NULL, NULL);
	if((x != eventer->x) || (y != eventer->y))
	  xitk_window_move_window(gGui->imlib_data, eventer->xwin, eventer->x, eventer->y);
      }
    }
    return;

  case KeyPress:
    {
      XKeyEvent      mykeyevent;
      KeySym         key;
      char           kbuf[256];

      mykeyevent = event->xkey;
      
      XLockDisplay(gGui->display);
      XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &key, NULL);
      XUnlockDisplay(gGui->display);

      switch(key) {
      case XK_Up:
        xine_event.type = XINE_EVENT_INPUT_UP;
	break;
      case XK_Down:
        xine_event.type = XINE_EVENT_INPUT_DOWN;
	break;
      case XK_Left:
        xine_event.type = XINE_EVENT_INPUT_LEFT;
	break;
      case XK_Right:
        xine_event.type = XINE_EVENT_INPUT_RIGHT;
	break;

      case XK_Return:
      case XK_KP_Enter:
      case XK_ISO_Enter:
        xine_event.type = XINE_EVENT_INPUT_SELECT;
	break;
	
      case XK_0:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_0;
	break;
      case XK_1:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_1;
	break;
      case XK_2:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_2;
	break;
      case XK_3:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_3;
	break;
      case XK_4:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_4;
	break;
      case XK_5:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_5;
	break;
      case XK_6:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_6;
	break;
      case XK_7:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_7;
	break;
      case XK_8:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_8;
	break;
      case XK_9:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_9;
	break;

      case XK_plus:
      case XK_KP_Add:
        xine_event.type = XINE_EVENT_INPUT_NUMBER_10_ADD;
	break;

      case XK_Escape:
	event_sender_exit(NULL, NULL);
	return;

      default:
	gui_handle_event(event, data);
	return;
      }
    }
    break;

    default:
      return;
  }

  xine_event.data_length = 0;
  xine_event.data        = NULL;
  xine_event.stream      = gGui->stream;
  gettimeofday(&xine_event.tv, NULL);
  xine_event_send (gGui->stream, &xine_event);
}

void event_sender_update_menu_buttons(void) {

  if(eventer) {
    int i;
    char **lbls = menu_items_default;
    
    if((!strncmp(gGui->mmk.mrl, "dvd:/", 5)) || (!strncmp(gGui->mmk.mrl, "dvdnav:/", 8)))
      lbls = menu_items_dvd;
    if(!strncmp(gGui->mmk.mrl, "bd:/", 4))
      lbls = menu_items_bd;
    
    for(i = 0; i < 7; i++)
      xitk_labelbutton_change_label(eventer->menus.menu[i], lbls[i]);

  }
}

static void event_sender_exit(xitk_widget_t *w, void *data) {

  if(eventer) {
    window_info_t wi;
    
    eventer->running = 0;
    eventer->visible = 0;
    
    if((xitk_get_window_info(eventer->widget_key, &wi))) {
      config_update_num ("gui.eventer_x", wi.x);
      config_update_num ("gui.eventer_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&eventer->widget_key);
    
    xitk_destroy_widgets(eventer->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, eventer->xwin);
    
    eventer->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(eventer->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(eventer->widget_list)));
    XUnlockDisplay(gGui->display);
    
    XITK_WIDGET_LIST_FREE(eventer->widget_list);
    
    free(eventer);
    eventer = NULL;

    try_to_set_input_focus(gGui->video_window);
  }
}

int event_sender_is_visible(void) {
  
  if(eventer != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(eventer->xwin));
    else
      return eventer->visible && xitk_is_window_visible(gGui->display, xitk_window_get_window(eventer->xwin));
  }  

  return 0;
}

int event_sender_is_running(void) {
  
  if(eventer != NULL)
    return eventer->running;
  
  return 0;
}

void event_sender_raise_window(void) {
  if(eventer != NULL)
    raise_window(xitk_window_get_window(eventer->xwin), eventer->visible, eventer->running);
}

void event_sender_toggle_visibility(xitk_widget_t *w, void *data) {
  if(eventer != NULL)
    toggle_window(xitk_window_get_window(eventer->xwin), eventer->widget_list,
		  &eventer->visible, eventer->running);
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

void event_sender_end(void) {
  event_sender_exit(NULL, NULL);
}

void event_sender_reparent(void) {
  if(eventer)
    reparent_window((xitk_window_get_window(eventer->xwin)));
}

void event_sender_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  int                        x, y, i;
  xitk_widget_t             *w;

  eventer = (_eventer_t *) calloc(1, sizeof(_eventer_t));
  
  eventer->x = xine_config_register_num (__xineui_global_xine_instance, "gui.eventer_x", 
					 80,
					 CONFIG_NO_DESC,
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_DEB,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
  eventer->y = xine_config_register_num (__xineui_global_xine_instance, "gui.eventer_y",
					 80,
					 CONFIG_NO_DESC,
					 CONFIG_NO_HELP,
					 CONFIG_LEVEL_DEB,
					 CONFIG_NO_CB,
					 CONFIG_NO_DATA);
  
  if(gGui->eventer_sticky && panel_is_visible()) {
    int  px, py, pw;
    xitk_get_window_position(gGui->display, gGui->panel_window, &px, &py, &pw, NULL);
    eventer->x = px + pw;
    eventer->y = py;
  }

  /* Create window */
  eventer->xwin = xitk_window_create_simple_window(gGui->imlib_data, eventer->x, eventer->y, WINDOW_WIDTH, WINDOW_HEIGHT);

  set_window_states_start((xitk_window_get_window(eventer->xwin)));

  XLockDisplay (gGui->display);
  XStoreName(gGui->display, (xitk_window_get_window(eventer->xwin)), _("xine Event Sender"));
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(eventer->xwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  eventer->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(eventer->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(eventer->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(eventer->xwin)));
  xitk_widget_list_set(eventer->widget_list, WIDGET_LIST_GC, gc);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_CENTER;
  lb.callback          = event_sender_send2;
  lb.state_callback    = NULL;
  lb.skin_element_name = NULL;

#define I2PTR(n) (void *)((long int)(n))

  x = 80 + 5 + 5;
  y = 5 + 23 * 3 + 5 - 40;

  lb.label             = _("Up");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_UP);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)),
	   (eventer->navigator.up = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   hboldfontname)));
  xitk_enable_and_show_widget(eventer->navigator.up);

  x -= 70;
  y += 40;

  lb.label             = _("Left");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_LEFT);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->navigator.left = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   hboldfontname)));
  xitk_enable_and_show_widget(eventer->navigator.left);

  x += 75;
  y += 5;

  lb.label             = _("Select");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_SELECT);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->navigator.select = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 60, 30,
					   "Black", "Black", "White", 
					   hboldfontname)));
  xitk_enable_and_show_widget(eventer->navigator.select);

  x += 65;
  y -= 5;

  lb.label             = _("Right");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_RIGHT);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->navigator.right = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   hboldfontname)));
  xitk_enable_and_show_widget(eventer->navigator.right);

  x -= 70;
  y += 40;
 
  lb.label             = _("Down");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_DOWN);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->navigator.down = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 70, 40,
					   "Black", "Black", "White", 
					   hboldfontname)));
  xitk_enable_and_show_widget(eventer->navigator.down);

  x = 23 * 2 + 5 + (80 - 23 * 3) / 2 + 1; /* (+1 to round up) */
  y = 5 + 23 * 3 + 5 + 40 + 5;

  for(i = 9; i >= 0; i--) {
    char number[2];
    
    snprintf(number, sizeof(number), "%d", i);

    lb.label             = number;
    lb.userdata          = I2PTR (XINE_EVENT_INPUT_NUMBER_0 + i);

    if(!i)
      x -= 46;

    xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	     (eventer->numbers.number[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 23, 23,
					     "Black", "Black", "White", hboldfontname)));
    xitk_enable_and_show_widget(eventer->numbers.number[i]);
  
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
    
    snprintf(number, sizeof(number), "+ %d", i);
    
    lb.label             = number;
    lb.userdata          = I2PTR (XINE_EVENT_INPUT_NUMBER_10_ADD);
    
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	     (eventer->numbers.number[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 46, 23,
					     "Black", "Black", "White", hboldfontname)));
    xitk_enable_and_show_widget(eventer->numbers.number[i]);
  }

  x = WINDOW_WIDTH - 80 - 5;
  y = 5 + 23 * 3 + 5 + 40 + 5;

  lb.label             = _("Angle +");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_ANGLE_NEXT);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->angles.next = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 80, 23,
					   "Black", "Black", "White", hboldfontname)));
  xitk_enable_and_show_widget(eventer->angles.next);

  y += 23;

  lb.label             = _("Angle -");
  lb.userdata          = I2PTR (XINE_EVENT_INPUT_ANGLE_PREVIOUS);
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	   (eventer->angles.prev = 
	    xitk_noskin_labelbutton_create(eventer->widget_list, 
					   &lb, x, y, 80, 23,
					   "Black", "Black", "White", hboldfontname)));
  xitk_enable_and_show_widget(eventer->angles.prev);


  /* Initialize menu labels */
  menu_items_default[0] = _("Menu 1");
  menu_items_default[1] = _("Menu 2");
  menu_items_default[2] = _("Menu 3");
  menu_items_default[3] = _("Menu 4");
  menu_items_default[4] = _("Menu 5");
  menu_items_default[5] = _("Menu 6");
  menu_items_default[6] = _("Menu 7");
 
  menu_items_dvd[0]     = _("Menu toggle");
  menu_items_dvd[1]     = _("Title");
  menu_items_dvd[2]     = _("Root");
  menu_items_dvd[3]     = _("Subpicture");
  menu_items_dvd[4]     = _("Audio");
  menu_items_dvd[5]     = _("Angle");
  menu_items_dvd[6]     = _("Part");

  menu_items_bd[0] = _("Top Menu");
  menu_items_bd[1] = _("Popup Menu");
  menu_items_bd[2] = _("Menu 3");
  menu_items_bd[3] = _("Menu 4");
  menu_items_bd[4] = _("Menu 5");
  menu_items_bd[5] = _("Menu 6");
  menu_items_bd[6] = _("Menu 7");

  x = 5;
  y = 5;

  for(i = 0; i < 7; i++) {
    lb.label             = "";
    lb.userdata          = I2PTR (XINE_EVENT_INPUT_MENU1 + i);
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
	     (eventer->menus.menu[i] = 
	      xitk_noskin_labelbutton_create(eventer->widget_list, 
					     &lb, x, y, 80, 23,
					     "Black", "Black", "White", hboldfontname)));
    xitk_enable_and_show_widget(eventer->menus.menu[i]);

    if(i == 2) {
      x += 80;
      y -= (23 * 3);
    }
    if(i == 3) {
      x += 80;
      y -= 23;
    }

    y += 23;
  }
  event_sender_update_menu_buttons();
  
  x = WINDOW_WIDTH - 70 - 5;
  y = WINDOW_HEIGHT - 23 - 5;

  lb.label             = _("Close");
  lb.callback          = event_sender_exit;
  lb.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(eventer->widget_list)), 
   (w = xitk_noskin_labelbutton_create(eventer->widget_list, 
				       &lb, x, y, 70, 23,
				       "Black", "Black", "White", hboldfontname)));
  xitk_enable_and_show_widget(w);
  event_sender_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());

  eventer->widget_key = xitk_register_event_handler("eventer", 
						    (xitk_window_get_window(eventer->xwin)),
						    event_sender_handle_event,
						    event_sender_store_new_position,
						    NULL,
						    eventer->widget_list,
						    NULL);
  

  eventer->visible = 1;
  eventer->running = 1;
  event_sender_raise_window();

  try_to_set_input_focus(xitk_window_get_window(eventer->xwin));
}
