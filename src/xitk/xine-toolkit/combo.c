/* 
 * Copyright (C) 2000-2019 the xine project
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>

#include "_xitk.h"

static void _combo_rollunroll(xitk_widget_t *w, void *data, int state);

/*
 *
 */
static void enability(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    if(w->enable == WIDGET_ENABLE) {
      xitk_enable_widget(private_data->label_widget);
      xitk_enable_widget(private_data->button_widget);
    }
    else {

      if(private_data->visible) {
	xitk_checkbox_set_state(private_data->button_widget, 0);
	_combo_rollunroll(private_data->button_widget, (void *)w, 0);
      }


      xitk_disable_widget(private_data->label_widget);
      xitk_disable_widget(private_data->button_widget);
    }

  }
}

static void notify_destroy(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;
    
    if(private_data->visible)
      _combo_rollunroll(private_data->button_widget, (void *)w, 0);

    xitk_destroy_widgets(private_data->widget_list);

    xitk_unregister_event_handler(&private_data->widget_key);
    xitk_window_destroy_window(private_data->imlibdata, private_data->xwin);

    XLOCK(private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, private_data->widget_list->gc);
    XUNLOCK(private_data->imlibdata->x.disp);

    xitk_widget_list_defferred_destroy (private_data->widget_list);

    XITK_FREE(private_data->skin_element_name);
    free(private_data);
  }
}

/*
 *
 */
static void paint(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    if(private_data->visible == 1 && (w->visible < 1)) {
      xitk_checkbox_set_state(private_data->button_widget, 0);
      _combo_rollunroll(private_data->button_widget, (void *)w, 0);
    }
    if(w->visible == 1) {
      int bx, lw;

      lw = xitk_get_widget_width(private_data->label_widget);
      xitk_set_widget_pos(private_data->label_widget, w->x, w->y);
      bx = w->x + lw;
      xitk_set_widget_pos(private_data->button_widget, bx, w->y);

      xitk_show_widget(private_data->label_widget);
      xitk_show_widget(private_data->button_widget);
    }
    else {
      xitk_hide_widget(private_data->label_widget);
      xitk_hide_widget(private_data->button_widget);
    }
  }
}

/*
 *
 */
void xitk_combo_callback_exec(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *)w->private_data;
    
    if(private_data->callback)
      private_data->callback(private_data->combo_widget, 
			     private_data->userdata, private_data->selected);

  }
}

/*
 * Called on select action.
 */
static void combo_select(xitk_widget_t *w, void *data, int selected) {

  if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
    xitk_widget_t        *c = (xitk_widget_t *) ((browser_private_data_t *)w->private_data)->userdata;
    combo_private_data_t *private_data = (combo_private_data_t *)c->private_data;
    
    private_data->selected = selected;
    
    xitk_label_change_label(private_data->label_widget, private_data->entries[selected]);
    
    XLOCK(private_data->imlibdata->x.disp);
    XUnmapWindow(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
    XUNLOCK(private_data->imlibdata->x.disp);
    
    private_data->visible = 0;
    
    xitk_browser_release_all_buttons(private_data->browser_widget);
    xitk_browser_update_list(private_data->browser_widget, 
			     (const char* const*)private_data->entries, NULL,
			     private_data->num_entries, 0);
    
    xitk_checkbox_set_state(private_data->button_widget, 0);

    if(private_data->callback)
      private_data->callback(private_data->combo_widget, private_data->userdata, selected);
  
  }
}

/*
 * Handle Xevents here.
 */
static void _combo_handle_event(XEvent *event, void *data) {
  combo_private_data_t *private_data = (combo_private_data_t *)data;

  switch(event->type) {
    
  case ButtonRelease:
    /*
     * If we try to move the combo window,
     * move it back to right position (under label*
     */
    if(private_data && private_data->visible) { 
      int  x, y;
      xitk_window_get_window_position(private_data->combo_widget->imlibdata, 
				      private_data->xwin, &x, &y, NULL, NULL);
      if((x != private_data->win_x) || (y != private_data->win_y))
	xitk_combo_update_pos(private_data->combo_widget);
    }
    break;
  }    
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    if(private_data->skin_element_name) {
      int x, y;

      xitk_skin_lock(skonfig);
      
      w->visible = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1 : -1;
      w->enable  = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
      
      xitk_set_widget_pos(w, w->x, w->y);
      xitk_get_widget_pos(private_data->label_widget, &x, &y);
      
      w->x = x;
      w->y = y;

      x += xitk_get_widget_width(private_data->label_widget);
      
      (void) xitk_set_widget_pos(private_data->button_widget, x, y);

      xitk_skin_unlock(skonfig);
    }
  }
}

static void tips_timeout(xitk_widget_t *w, unsigned long timeout) {
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;
    
    private_data->combo_widget->tips_timeout = timeout;
    private_data->label_widget->tips_timeout = timeout;
    private_data->button_widget->tips_timeout = timeout;
    private_data->browser_widget->tips_timeout = timeout;
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
  case WIDGET_EVENT_ENABLE:
    enability(w);
    break;
  case WIDGET_EVENT_TIPS_TIMEOUT:
    tips_timeout(w, event->tips_timeout);
    break;
  }
  
  return retval;
}

/*
 *
 */
static void _combo_rollunroll(xitk_widget_t *w, void *data, int state) {
  xitk_widget_t        *combo = (xitk_widget_t *)data;
  combo_private_data_t *private_data = (combo_private_data_t *)combo->private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX))) {
    
    if(state && private_data->visible == 0) {
      private_data->visible = 1;
      xitk_combo_update_pos(combo);
    }
    else {
      private_data->visible = 0;

      XLOCK(private_data->imlibdata->x.disp);
      XUnmapWindow(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
      XUNLOCK(private_data->imlibdata->x.disp);
    }
  }      
}

/*
 *
 */
static void _combo_rollunroll_from_lbl(xitk_widget_t *w, void *data) {
  xitk_widget_t        *combo = (xitk_widget_t *)data;
  combo_private_data_t *private_data = (combo_private_data_t *)combo->private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))) {
    int state;
    
    state = !xitk_checkbox_get_state(private_data->button_widget);
    
    xitk_checkbox_set_state(private_data->button_widget, state);

    if(state && private_data->visible == 0) {
      w->wl->widget_focused = private_data->button_widget;
      private_data->visible = 1;
      xitk_combo_update_pos(combo);
    }
    else {
      private_data->visible = 0;
      
      XLOCK(private_data->imlibdata->x.disp);
      XUnmapWindow(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
      XUNLOCK(private_data->imlibdata->x.disp);
    }
    
  }      
}

/*
 * ************************* END OF PRIVATES *******************************
 */

int xitk_combo_is_same_parent(xitk_widget_t *w1, xitk_widget_t *w2) {

  if((w1 && w2) && ((w1->type & WIDGET_GROUP_COMBO) && (w1->type & WIDGET_GROUP_COMBO))) {
    
    if(w1 == w2)
      return 1;
    
    if(w1->wl == w2->wl) {
      xitk_widget_list_t  *wl = w1->wl;
      xitk_widget_t       *w, *wt;
      
      w = (xitk_widget_t *)wl->list.head.next;
      while (w->node.next && ((w != w1) && (w != w2))) {
        w = (xitk_widget_t *)w->node.next;
      }

      if (w->node.next) {

	wt = (w == w1) ? w2 : w1;
	
	if((w->type & WIDGET_GROUP_COMBO) && 
	   ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
          w = (xitk_widget_t *)w->node.next;
          if (w->node.next)
            w = (xitk_widget_t *)w->node.next;
	}
	else if((w->type & WIDGET_GROUP_COMBO) && 
		((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
          w = (xitk_widget_t *)w->node.next;
	}
	
        if (w->node.next && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET)) {
	  combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;
	
	  if((wt == private_data->label_widget) || (wt == private_data->button_widget))
	    return 1;
	  
	}

      }
    }
  }
  return 0;  
}

/*
 *
 */
void xitk_combo_rollunroll(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO))) {

    if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX) {
      int state = !xitk_checkbox_get_state(w);
      xitk_checkbox_callback_exec(w);
      xitk_checkbox_set_state(w, state);
    }
  }
  else if(w->type & WIDGET_GROUP_WIDGET) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;
    int state = !xitk_checkbox_get_state(private_data->button_widget);
      xitk_checkbox_callback_exec(private_data->button_widget);
      xitk_checkbox_set_state(private_data->button_widget, state);
  }
}

  /*
   *
   */
void xitk_combo_set_select(xitk_widget_t *w, int select) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *)w->private_data;
    
    if(private_data->entries && private_data->entries[select]) {
      private_data->selected = select;
      xitk_label_change_label(private_data->label_widget, private_data->entries[select]);
    }

  }
}

/*
 *
 */
void xitk_combo_update_pos(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t  *private_data = (combo_private_data_t *)w->private_data;
    int                    xx = 0, yy = 0;
    window_info_t          wi;
    XSizeHints             hint;
    
    if(private_data->visible) {
      if((xitk_get_window_info(*(private_data->parent_wkey), &wi))) {
	private_data->win_x = wi.x;
	private_data->win_y = wi.y;
	WINDOW_INFO_ZERO(&wi);
      }
      
      xitk_get_widget_pos(private_data->label_widget, &xx, &yy);
      
      yy += xitk_get_widget_height(private_data->label_widget);
      private_data->win_x += xx;
      private_data->win_y += yy;
      
      hint.x = private_data->win_x;
      hint.y = private_data->win_y;
      hint.flags = PPosition;

      XLOCK(private_data->imlibdata->x.disp);
      XSetWMNormalHints (private_data->imlibdata->x.disp,
			 xitk_window_get_window(private_data->xwin),
			 &hint);
      XMoveWindow(private_data->imlibdata->x.disp, 
		  (xitk_window_get_window(private_data->xwin)), 
		  private_data->win_x, private_data->win_y);
      XMapRaised(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
      XSync(private_data->imlibdata->x.disp, False);
      XUNLOCK(private_data->imlibdata->x.disp);

      while(!xitk_is_window_visible(private_data->imlibdata->x.disp,
				    (xitk_window_get_window(private_data->xwin))))
	xitk_usec_sleep(5000);
      
      XLOCK(private_data->imlibdata->x.disp);
      XSetInputFocus(private_data->imlibdata->x.disp, 
		     (xitk_window_get_window(private_data->xwin)), RevertToParent, CurrentTime);
      XUNLOCK(private_data->imlibdata->x.disp);
      
      /* No widget focused, give focus to the first one */
      if(private_data->widget_list->widget_focused == NULL)
	xitk_set_focus_to_next_widget(private_data->widget_list, 0);

    }
  }
}

/*
 *
 */
int xitk_combo_get_current_selected(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    return private_data->selected;
  }

  return -1;    
}

/*
 * 
 */
const char *xitk_combo_get_current_entry_selected(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    if(private_data->entries && private_data->selected >= 0)
      return(private_data->entries[private_data->selected]);
  }

  return NULL;    
}

/*
 *
 */
void xitk_combo_update_list(xitk_widget_t *w, const char *const *const list, int len) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;

    private_data->entries     = list;
    private_data->num_entries = len;
    private_data->selected    = -1;
    
    xitk_browser_update_list(private_data->browser_widget, 
			     (const char* const*)private_data->entries, NULL,
			     private_data->num_entries, 0);
  }
}

/*
 *
 */
xitk_widget_t *xitk_combo_get_label_widget(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    combo_private_data_t *private_data = (combo_private_data_t *) w->private_data;
    
    return private_data->label_widget;
  }

  return NULL;
}

/*
 *
 */
static xitk_widget_t *_xitk_combo_create(xitk_widget_list_t *wl,
					 xitk_skin_config_t *skonfig,
                                         xitk_combo_widget_t *c, const char *skin_element_name,
					 xitk_widget_t *mywidget, 
					 combo_private_data_t *private_data,
					 int visible, int enable) {
  Atom                        XA_WIN_LAYER;
  long                        data[1];
  const char                **entries = c->entries;
  unsigned int                itemw, itemh = 20;
  unsigned int                slidw = 12;
  xitk_browser_widget_t       browser;
  XClassHint                  xclasshint;
  Status                      status;
  
  XITK_WIDGET_INIT(&browser, c->imlibdata);

  itemw = xitk_get_widget_width(private_data->label_widget);
  itemw += xitk_get_widget_width(private_data->button_widget);
  itemw -= 2; /* space for border */

  private_data->imlibdata                = c->imlibdata;
  private_data->skin_element_name        = (skin_element_name == NULL) ? NULL : strdup(skin_element_name);
  private_data->entries                  = c->entries;
  private_data->combo_widget             = mywidget;
  private_data->parent_wlist             = c->parent_wlist;
  private_data->parent_wkey              = c->parent_wkey;
  private_data->callback                 = c->callback;
  private_data->userdata                 = c->userdata;
  
  private_data->selected = -1;
  {
    int i = 0;
    
    while(entries[i] != NULL) {
      i++;
    }
    
    private_data->num_entries = i;
  }
  
  if(private_data->num_entries) {
    xitk_label_change_label(private_data->label_widget, entries[0]);
    private_data->selected = 0;
  }
  
  private_data->xwin = xitk_window_create_simple_window(c->imlibdata, 0, 0,
							(itemw + 2), (itemh * 5) + slidw + 2);
  XLOCK(c->imlibdata->x.disp);

  {
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    XLOCK (c->imlibdata->x.disp);
    XChangeWindowAttributes (c->imlibdata->x.disp,
			     (xitk_window_get_window(private_data->xwin)),
			     CWOverrideRedirect, &attr);
    XUNLOCK (c->imlibdata->x.disp);
  }

  if(c->layer_above) {
    XA_WIN_LAYER = XInternAtom(c->imlibdata->x.disp, "_WIN_LAYER", False);
    
    data[0] = 10;
    XChangeProperty(c->imlibdata->x.disp, 
		    (xitk_window_get_window(private_data->xwin)), XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }

  XSetTransientForHint (c->imlibdata->x.disp,
			(xitk_window_get_window(private_data->xwin)), private_data->parent_wlist->win);
  
  /* Change default classhint to new one. */
  if((status = XGetClassHint(c->imlibdata->x.disp,
		     (xitk_window_get_window(private_data->xwin)), &xclasshint)) != BadWindow) {
    XFree(xclasshint.res_name);
    XFree(xclasshint.res_class);
    xclasshint.res_name  = (char *)"Xitk Combo";
    xclasshint.res_class = (char *)"Xitk";
    XSetClassHint(c->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)), &xclasshint);
  }

  private_data->gc = XCreateGC(c->imlibdata->x.disp, 
			       (xitk_window_get_window(private_data->xwin)), None, None);

  XUNLOCK(c->imlibdata->x.disp);

  private_data->widget_list                = xitk_widget_list_new() ;
  xitk_dlist_init (&private_data->widget_list->list);
  private_data->widget_list->win           = (xitk_window_get_window(private_data->xwin));
  private_data->widget_list->gc            = private_data->gc;
  
  /* Browser */
  browser.imlibdata                     = private_data->imlibdata;
  browser.arrow_up.skin_element_name    = NULL;
  browser.slider.skin_element_name      = NULL;
  browser.arrow_dn.skin_element_name    = NULL;
  browser.browser.skin_element_name     = NULL;
  browser.browser.max_displayed_entries = 5;
  browser.browser.num_entries           = private_data->num_entries;
  browser.browser.entries               = (const char* const*)private_data->entries;
  browser.callback                      = combo_select;
  browser.dbl_click_callback            = NULL;
  browser.parent_wlist                  = private_data->widget_list;
  browser.userdata                      = (void*)mywidget;
  private_data->browser_widget = xitk_noskin_browser_create (private_data->widget_list, &browser,
    private_data->gc, 1, 1, (itemw - slidw), itemh, slidw, DEFAULT_FONT_10);
  xitk_dlist_add_tail (&private_data->widget_list->list, &private_data->browser_widget->node);
  xitk_enable_and_show_widget(private_data->browser_widget);
  private_data->browser_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;
  
  xitk_browser_update_list(private_data->browser_widget, 
			   (const char* const*)private_data->entries, NULL,
			   private_data->num_entries, 0);
  
  private_data->widget_key = 
    xitk_register_event_handler("xitk combo",
				(xitk_window_get_window(private_data->xwin)), 
				_combo_handle_event,
				NULL,
				NULL,
				private_data->widget_list,
				(void *) private_data);

  private_data->visible  = 0;

  mywidget->private_data = private_data;
  mywidget->wl           = wl;
  mywidget->enable       = enable;
  mywidget->running      = 1;
  mywidget->visible      = visible;
  mywidget->have_focus   = FOCUS_LOST;

  mywidget->imlibdata    = private_data->imlibdata;
  //  mywidget->x = mywidget->y = mywidget->width = mywidget->height = 0;
  mywidget->type         = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
  mywidget->event        = notify_event;
  mywidget->tips_timeout = 0;
  mywidget->tips_string  = NULL;

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_combo_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_combo_widget_t *c,
				 xitk_widget_t **lw, xitk_widget_t **bw) {
  xitk_widget_t              *mywidget;
  combo_private_data_t       *private_data;
  xitk_checkbox_widget_t      cb;
  xitk_label_widget_t         lbl;
  
  XITK_CHECK_CONSTITENCY(c);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  XITK_WIDGET_INIT(&cb, c->imlibdata);
  XITK_WIDGET_INIT(&lbl, c->imlibdata);

  private_data = (combo_private_data_t *) xitk_xmalloc (sizeof(combo_private_data_t));

  /* Create label and button (skinable) */
  lbl.label             = "";
  lbl.skin_element_name = c->skin_element_name;
  lbl.window            = c->parent_wlist->win;
  lbl.gc                = c->parent_wlist->gc;
  lbl.callback          = _combo_rollunroll_from_lbl;
  lbl.userdata          = (void *)mywidget;
  private_data->label_widget = xitk_label_create (c->parent_wlist, skonfig, &lbl);
  xitk_dlist_add_tail (&c->parent_wlist->list, &private_data->label_widget->node);
  private_data->label_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

  cb.skin_element_name = c->skin_element_name;
  cb.callback          = _combo_rollunroll;
  cb.userdata          = (void *)mywidget;
  private_data->button_widget = xitk_checkbox_create (c->parent_wlist, skonfig, &cb);
  xitk_dlist_add_tail (&c->parent_wlist->list, &private_data->button_widget->node);
  private_data->button_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

  if(lw)
    *lw = private_data->label_widget;
  if(bw)
    *bw = private_data->button_widget;
  
  {
    int x, y;
    
    xitk_get_widget_pos(private_data->label_widget, &x, &y);

    mywidget->x = x;
    mywidget->y = y;

    x += xitk_get_widget_width(private_data->label_widget);
    
    (void) xitk_set_widget_pos(private_data->button_widget, x, y);
  }
  return _xitk_combo_create(wl, skonfig, c, c->skin_element_name, mywidget, private_data,
			    (xitk_skin_get_visibility(skonfig, c->skin_element_name)) ? 1 : -1,
			    xitk_skin_get_enability(skonfig, c->skin_element_name));
}

/*
 *  ******************************************************************************
 */
xitk_widget_t *xitk_noskin_combo_create(xitk_widget_list_t *wl,
					xitk_combo_widget_t *c,
					int x, int y, int width, 
					xitk_widget_t **lw, xitk_widget_t **bw) {
  xitk_widget_t              *mywidget;
  combo_private_data_t       *private_data;
  xitk_checkbox_widget_t      cb;
  xitk_label_widget_t         lbl;

  XITK_CHECK_CONSTITENCY(c);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  XITK_WIDGET_INIT(&cb, c->imlibdata);
  XITK_WIDGET_INIT(&lbl, c->imlibdata);

  private_data = (combo_private_data_t *) xitk_xmalloc (sizeof(combo_private_data_t));

  /* Create label and button (skinable) */
  {
    xitk_font_t    *fs;
    int             height;
    
    fs = xitk_font_load_font(c->imlibdata->x.disp, DEFAULT_FONT_10);
    xitk_font_set_font(fs, c->parent_wlist->gc);
    height = xitk_font_get_string_height(fs, " ") + 4;
    xitk_font_unload_font(fs);


    lbl.window            = c->parent_wlist->win;
    lbl.gc                = c->parent_wlist->gc;
    lbl.skin_element_name = NULL;
    lbl.label             = "";
    lbl.callback          = _combo_rollunroll_from_lbl;
    lbl.userdata          = (void *)mywidget;
    private_data->label_widget = xitk_noskin_label_create (c->parent_wlist, &lbl,
      x, y, (width - height), height, DEFAULT_FONT_10);
    xitk_dlist_add_tail (&c->parent_wlist->list, &private_data->label_widget->node);
    private_data->label_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

    cb.skin_element_name = NULL;
    cb.callback          = _combo_rollunroll;
    cb.userdata          = (void *)mywidget;

    private_data->button_widget = xitk_noskin_checkbox_create (c->parent_wlist, &cb,
      x + (width - height), y, height, height);
    xitk_dlist_add_tail (&c->parent_wlist->list, &private_data->button_widget->node);
    private_data->button_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;
  
    if(lw)
      *lw = private_data->label_widget;
    if(bw)
      *bw = private_data->button_widget;
    
    mywidget->x = x;
    mywidget->y = y;
    mywidget->width = width;
    mywidget->height = height;
    
    {
      xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->label_widget);
      
      if(wimage)
	draw_rectangular_inner_box(c->imlibdata, wimage->image, 0, 0, wimage->width - 1, wimage->height - 1);

      wimage = xitk_get_widget_foreground_skin(private_data->button_widget);
      
      if(wimage) {
	draw_bevel_three_state(c->imlibdata, wimage);
	draw_arrow_down(c->imlibdata, wimage);
      }
      
    }

  }

  return _xitk_combo_create(wl, NULL, c, NULL, mywidget, private_data, 0, 0);
}
