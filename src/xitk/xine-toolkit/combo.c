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
#include <X11/Xutil.h>
#include <stdio.h>

#include "combo.h"
#include "browser.h"
#include "image.h"
#include "label.h"
#include "button.h"
#include "labelbutton.h"
#include "checkbox.h"
#include "font.h"
#include "window.h"
#include "_xitk.h"

static void _combo_rollunroll(xitk_widget_t *w, void *data, int state);

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  combo_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {

    private_data = (combo_private_data_t *) w->private_data;
    
    if(private_data->visible)
      _combo_rollunroll(NULL, (void *)w, 0);

    xitk_destroy_widgets(private_data->widget_list);
    xitk_list_free(private_data->widget_list->l);

    xitk_unregister_event_handler(&private_data->widget_key);
    xitk_window_destroy_window(private_data->imlibdata, private_data->xwin);

    XLOCK(private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, private_data->widget_list->gc);
    XUNLOCK(private_data->imlibdata->x.disp);

    XITK_FREE(private_data->widget_list);

    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static void paint(xitk_widget_t *w, Window win, GC gc) {
  combo_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {

    private_data = (combo_private_data_t *) w->private_data;

    if(private_data->visible == 1 && (w->visible < 1)) {
      xitk_checkbox_set_state(private_data->button_widget, 0,
			      private_data->parent_wlist->win, private_data->parent_wlist->gc);
      _combo_rollunroll(NULL, (void *)w, 0);
    }
    if((w->visible == 1)) {
      int bx, lw;

      lw = xitk_get_widget_width(private_data->label_widget);
      xitk_set_widget_pos(private_data->label_widget, w->x, w->y);
      bx = w->x + lw;
      xitk_set_widget_pos(private_data->button_widget, bx, w->y);

      xitk_show_widget(private_data->parent_wlist, private_data->label_widget);
      xitk_show_widget(private_data->parent_wlist, private_data->button_widget);
    }
    else {
      xitk_hide_widget(private_data->parent_wlist, private_data->label_widget);
      xitk_hide_widget(private_data->parent_wlist, private_data->button_widget);
    }
  }
}

/*
 * Called on select action.
 */
static void combo_select(xitk_widget_t *w, void *data) {
  combo_private_data_t *private_data;
  xitk_widget_t        *c;
  int                   selected;

  if(w && ((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
    c = (xitk_widget_t *) ((browser_private_data_t *)w->private_data)->userdata;
    private_data = (combo_private_data_t *)c->private_data;
    selected = (int)data;
    
    private_data->selected = selected;
    
    xitk_label_change_label(private_data->parent_wlist, 
			    private_data->label_widget, private_data->entries[selected]);
    
    XLOCK(private_data->imlibdata->x.disp);
    XUnmapWindow(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
    XUNLOCK(private_data->imlibdata->x.disp);
    
    private_data->visible = 0;
    
    xitk_browser_release_all_buttons(private_data->browser_widget);
    xitk_browser_update_list(private_data->browser_widget, 
			     private_data->entries, private_data->num_entries, 0);
    
    xitk_checkbox_set_state(private_data->button_widget, 0, 
			    private_data->widget_list->win, private_data->widget_list->gc);

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
    if(private_data->visible) { 
      if(!xitk_is_inside_widget(private_data->browser_widget, 
				event->xbutton.x, event->xbutton.y)) {
	xitk_combo_update_pos(private_data->combo_widget);
      }
    }
    break;
  }
    
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  combo_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (combo_private_data_t *) w->private_data;

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

/*
 *
 */
static void _combo_rollunroll(xitk_widget_t *w, void *data, int state) {
  xitk_widget_t        *combo = (xitk_widget_t *)data;
  combo_private_data_t *private_data = (combo_private_data_t *)combo->private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX))) {
    
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
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))) {
    int state;
    
    state = !xitk_checkbox_get_state(private_data->button_widget);
    
    xitk_checkbox_set_state(private_data->button_widget, state, 
			    private_data->parent_wlist->win, private_data->parent_wlist->gc);
    
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
 * ************************* END OF PRIVATES *******************************
 */

/*
 *
 */
void xitk_combo_set_select(xitk_widget_list_t *wl, xitk_widget_t *w, int select) {
  combo_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (combo_private_data_t *)w->private_data;
    
    if(private_data->entries && private_data->entries[select]) {
      private_data->selected = select;
      xitk_label_change_label(wl, private_data->label_widget, private_data->entries[select]);
    }

  }
}

/*
 *
 */
void xitk_combo_update_pos(xitk_widget_t *w) {
  combo_private_data_t  *private_data;
  int                    x = 0, xx = 0, y = 0, yy = 0;
  window_info_t          wi;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (combo_private_data_t *)w->private_data;
    
    if(private_data->visible) {
      if((xitk_get_window_info(*(private_data->parent_wkey), &wi))) {
	x = wi.x;
	y = wi.y;
	WINDOW_INFO_ZERO(&wi);
      }
      
      xitk_get_widget_pos(private_data->label_widget, &xx, &yy);
      
      yy += xitk_get_widget_height(private_data->label_widget);
      x += xx;
      y += yy;
      
      XLOCK(private_data->imlibdata->x.disp);
      XMoveWindow(private_data->imlibdata->x.disp, 
		  (xitk_window_get_window(private_data->xwin)), x, y);
      XMapRaised(private_data->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)));
      XSync(private_data->imlibdata->x.disp, False);
      XUNLOCK(private_data->imlibdata->x.disp);

      while(!xitk_is_window_visible(private_data->imlibdata->x.disp,
				    (xitk_window_get_window(private_data->xwin))))
	xitk_usec_sleep(5000);
      
      XSetInputFocus(private_data->imlibdata->x.disp, 
		     (xitk_window_get_window(private_data->xwin)), RevertToParent, CurrentTime);
      
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
  combo_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (combo_private_data_t *) w->private_data;
    return private_data->selected;
  }

  return -1;    
}

/*
 * 
 */
char *xitk_combo_get_current_entry_selected(xitk_widget_t *w) {
  combo_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (combo_private_data_t *) w->private_data;

    if(private_data->entries && private_data->selected >= 0)
      return(private_data->entries[private_data->selected]);
  }

  return NULL;    
}

/*
 *
 */
void xitk_combo_update_list(xitk_widget_t *w, char **list, int len) {
  combo_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
 
    private_data              = (combo_private_data_t *) w->private_data;
    private_data->entries     = list;
    private_data->num_entries = len;
    private_data->selected    = -1;
    
    xitk_browser_update_list(private_data->browser_widget, 
			     private_data->entries, private_data->num_entries, 0);
  }
}

/*
 *
 */
static xitk_widget_t *_xitk_combo_create(xitk_widget_list_t *wl,
					 xitk_skin_config_t *skonfig,
					 xitk_combo_widget_t *c, char *skin_element_name,
					 xitk_widget_t *mywidget, 
					 combo_private_data_t *private_data,
					 int visible, int enable) {
  Atom                        XA_WIN_LAYER;
  long                        data[1];
  char                      **entries = c->entries;
  unsigned int                itemw, itemh = 20;
  unsigned int                slidw = 12;
  xitk_browser_widget_t       browser;
  XClassHint                  xclasshint;
  Status                      status;
  
  XITK_WIDGET_INIT(&browser, c->imlibdata);

  itemw = xitk_get_widget_width(private_data->label_widget);
  itemw += xitk_get_widget_width(private_data->button_widget);

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
    xitk_label_change_label(c->parent_wlist, private_data->label_widget, entries[0]);
    private_data->selected = 0;
  }
  
  private_data->xwin = xitk_window_create_simple_window(c->imlibdata, 0, 0,
							(itemw + 2), (itemh * 5) + slidw + 2);
  XLOCK(c->imlibdata->x.disp);

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
    xclasshint.res_name  = "Xitk Combo";
    xclasshint.res_class = "Xitk";
    XSetClassHint(c->imlibdata->x.disp, (xitk_window_get_window(private_data->xwin)), &xclasshint);
  }

  private_data->gc = XCreateGC(c->imlibdata->x.disp, 
			       (xitk_window_get_window(private_data->xwin)), None, None);

  XUNLOCK(c->imlibdata->x.disp);

  private_data->widget_list                = xitk_widget_list_new() ;
  private_data->widget_list->l             = xitk_list_new ();
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
  browser.browser.entries               = private_data->entries;
  browser.callback                      = combo_select;
  browser.dbl_click_callback            = NULL;
  browser.parent_wlist                  = private_data->widget_list;
  browser.userdata                      = (void*)mywidget;
  xitk_list_append_content (private_data->widget_list->l, 
			    (private_data->browser_widget = 
			     xitk_noskin_browser_create(private_data->widget_list, &browser,
							private_data->gc, 1, 1, 
							(itemw - slidw), itemh, slidw,
							DEFAULT_FONT_10)));
  private_data->browser_widget->widget_type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;
  
  xitk_browser_update_list(private_data->browser_widget, 
			   private_data->entries, private_data->num_entries, 0);
  
  private_data->widget_key = 
    xitk_register_event_handler("xitk combo",
				(xitk_window_get_window(private_data->xwin)), 
				_combo_handle_event,
				NULL,
				NULL,
				private_data->widget_list,
				(void *) private_data);

  private_data->visible                  = 0;

  mywidget->private_data                 = private_data;
  mywidget->widget_list                  = wl;

  mywidget->enable                       = enable;
  mywidget->running                      = 1;
  mywidget->visible                      = visible;
  mywidget->have_focus                   = FOCUS_LOST;


  mywidget->imlibdata                    = private_data->imlibdata;
  //  mywidget->x = mywidget->y = mywidget->width = mywidget->height = 0;
  
  mywidget->widget_type                  = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
  mywidget->paint                        = paint;
  mywidget->notify_click                 = NULL;
  mywidget->notify_focus                 = NULL;
  mywidget->notify_keyevent              = NULL;
  mywidget->notify_inside                = NULL;
  mywidget->notify_change_skin           = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy               = notify_destroy;
  mywidget->get_skin                     = NULL;

  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;

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
  xitk_list_append_content(c->parent_wlist->l,
			   (private_data->label_widget = 
			    xitk_label_create(c->parent_wlist, skonfig, &lbl)));
  private_data->label_widget->widget_type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

  cb.skin_element_name = c->skin_element_name;
  cb.callback          = _combo_rollunroll;
  cb.userdata          = (void *)mywidget;
  xitk_list_append_content(c->parent_wlist->l, 
			   (private_data->button_widget = 
			    xitk_checkbox_create(c->parent_wlist, skonfig, &cb)));
  private_data->button_widget->widget_type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

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
    height = xitk_font_get_string_height(fs, FONT_HEIGHT_MODEL);
    xitk_font_unload_font(fs);


    lbl.window            = c->parent_wlist->win;
    lbl.gc                = c->parent_wlist->gc;
    lbl.skin_element_name = NULL;
    lbl.label             = "";
    lbl.callback          = _combo_rollunroll_from_lbl;
    lbl.userdata          = (void *)mywidget;
    xitk_list_append_content(c->parent_wlist->l, 
			     (private_data->label_widget = 
			      xitk_noskin_label_create(c->parent_wlist, &lbl,
						       x, y, (width - height), (height + 4), DEFAULT_FONT_10)));
    private_data->label_widget->widget_type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;

    cb.skin_element_name = NULL;
    cb.callback          = _combo_rollunroll;
    cb.userdata          = (void *)mywidget;

    xitk_list_append_content(c->parent_wlist->l, 
			     (private_data->button_widget = 
			      xitk_noskin_checkbox_create(c->parent_wlist, &cb,
							  x + (width - height), y,
							  (height + 4), (height + 4))));
    private_data->button_widget->widget_type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;
  
    if(lw)
      *lw = private_data->label_widget;
    if(bw)
      *bw = private_data->button_widget;
    
    mywidget->x = x;
    mywidget->y = y;
    mywidget->width = (width - height) + (height + 4);
    mywidget->height = (height + 4);
    
    {
      xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->label_widget);
      
      if(wimage) {
	draw_rectangular_inner_box(c->imlibdata, wimage->image, 0, 0, wimage->width - 1, wimage->height - 1);
      }
      
      wimage = xitk_get_widget_foreground_skin(private_data->button_widget);
      
      if(wimage) {
	draw_arrow_down(c->imlibdata, wimage);
      }
      
    }

  }

  return _xitk_combo_create(wl, NULL, c, NULL, mywidget, private_data, 1, 1);
}
