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
#include <sys/time.h>
#include <unistd.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "widget_types.h"
#include "image.h"
#include "browser.h"
#include "list.h"
#include "button.h"
#include "labelbutton.h"
#include "slider.h"

#include "_xitk.h"

typedef struct {
  xitk_widget_t    *itemlist;
  int               sel;
} btnlist_t;


#define WBUP    0  /*  Position of button up in item_tree  */
#define WSLID   1  /*  Position of slider in item_tree  */
#define WBDN    2  /*  Position of button down in item_tree  */
#define WBSTART 3  /*  Position of first item button in item_tree */

/*
 *
 */
static void paint(xitk_widget_t *w, Window win, GC gc) {
  browser_private_data_t *private_data = (browser_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    int i;

    if(w->visible) {
      int x     = w->x;
      int y     = w->y;
      int itemw = xitk_get_widget_width(private_data->item_tree[WBSTART]);
      int itemh = xitk_get_widget_height(private_data->item_tree[WBSTART]);
      int iy    = w->y;
      int bh;
      
      (void) xitk_set_widget_pos(private_data->item_tree[WBUP], x + itemw, y);
      bh = xitk_get_widget_height(private_data->item_tree[WBUP]);
      (void) xitk_set_widget_pos(private_data->item_tree[WSLID], x + itemw, y + bh);
      bh += xitk_get_widget_height(private_data->item_tree[WSLID]);
      (void) xitk_set_widget_pos(private_data->item_tree[WBDN], x + itemw, y + bh);
      
      xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBUP]);
      xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WSLID]);
      xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBDN]);
      
      for(i = WBSTART; i < private_data->max_length+WBSTART; i++) {
	(void) xitk_set_widget_pos(private_data->item_tree[i], x, iy);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[i]);
	iy += itemh;
      }
      
    }
    else {
      for(i = 0; i < private_data->max_length+WBSTART; i++) {
	xitk_hide_widget(private_data->parent_wlist, private_data->item_tree[i]);
      }
    }
  }
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    if(private_data->skin_element_name) {
      int x, y, i = 0;
      
      x = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      y = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      
      for(i = WBSTART; i < private_data->max_length+WBSTART; i++) {
	
	(void) xitk_set_widget_pos(private_data->item_tree[i], x, y);
	
	y += xitk_get_widget_height(private_data->item_tree[i]) + 1;
	
      }
    }
  }
}

/**
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    return private_data->current_start;
  }

  return -1;
}

/**
 * Change browser labels alignment
 */
void xitk_browser_set_alignment(xitk_widget_t *w, int align) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  int i;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    for(i = 0; i < private_data->max_length; i++) {
      xitk_labelbutton_set_alignment(private_data->item_tree[i+WBSTART], align);
    }
  }
}

/**
 * Return the current selected button (if not, return -1)
 */
int xitk_browser_get_current_selected(xitk_widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  int i;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    for(i = 0; i < private_data->max_length; i++) {
      if(xitk_labelbutton_get_state(private_data->item_tree[i+WBSTART]))
	return (private_data->current_start + i);
    }
  }

  return -1;    
}

/**
 * Release all enabled buttons
 */
void xitk_browser_release_all_buttons(xitk_widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  int i;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    for(i = 0; i < private_data->max_length; i++) {
      xitk_labelbutton_set_state(private_data->item_tree[i+WBSTART], 0, 
				 private_data->parent_wlist->win,
				 private_data->parent_wlist->gc);
    }
  }
}

/**
 * Select the item 'select' in list
 */
void xitk_browser_set_select(xitk_widget_t *w, int select) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    xitk_browser_release_all_buttons(w);
    
    xitk_labelbutton_set_state(private_data->item_tree[select+WBSTART], 1, 
			       private_data->parent_wlist->win, private_data->parent_wlist->gc);
  }
}

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  xitk_widget_list_t* wl;
  int i, j;

  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    xitk_browser_release_all_buttons(w);
    
    if(start >= 0)
      private_data->current_start = start;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    for(i = 0; i < private_data->max_length; i++) {
      if (((private_data->current_start+i)<private_data->list_length) 
	  && (private_data->content[private_data->current_start+i] != NULL)) {
	xitk_labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], 
				      private_data->content[private_data->
							   current_start+i]);
      }
      else {
	xitk_labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], "");
      }
    }
    j = (private_data->list_length > (private_data->max_length-1) ? 
	 (private_data->list_length-1) : 1);
    xitk_slider_set_max(private_data->item_tree[WSLID], j);
    
    if(start == 0)
      xitk_slider_set_to_max(wl, private_data->item_tree[WSLID]);
    else if(j>1)
      xitk_slider_set_pos(wl, private_data->item_tree[WSLID], 
			  (xitk_slider_get_max(private_data->item_tree[WSLID]) 
			   - xitk_slider_get_min(private_data->item_tree[WSLID]) 
			   - (private_data->current_start)));
    
    XITK_FREE(wl);
  }
}

/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, char **list, int len, int start) {
  browser_private_data_t *private_data;

  if(w) {
    if(w->widget_type & WIDGET_TYPE_BROWSER) {
      private_data = (browser_private_data_t *) w->private_data;
      private_data->content = list;
      private_data->list_length = len;
      private_data->current_start = start;
      xitk_browser_rebuild_browser(w, start);
    }
  }
}  

/**
 * Handle slider movments
 */
static void browser_slidmove(xitk_widget_t *w, void *data, int pos) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
  int realpos;
  
  
  if(w->widget_type & (WIDGET_TYPE_SLIDER | WIDGET_TYPE_BUTTON | WIDGET_TYPE_BROWSER)) {
    if(xitk_browser_get_current_selected(((xitk_widget_t*)data)) > -1)
      xitk_browser_release_all_buttons(((xitk_widget_t*)data));
    
    realpos = 
      xitk_slider_get_max(private_data->item_tree[WSLID]) 
      - xitk_slider_get_min(private_data->item_tree[WSLID]) 
      - pos;
    
    if(private_data->list_length > (private_data->max_length-1))
      xitk_browser_rebuild_browser(((xitk_widget_t*)data), realpos);
    else
      xitk_browser_rebuild_browser(((xitk_widget_t*)data), -1);
  }
}

/**
 * slide up
 */
static void browser_up(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
  xitk_widget_list_t* wl;

  if(w->widget_type & WIDGET_TYPE_BUTTON) {
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_step(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, data, xitk_slider_get_pos(private_data->
						  item_tree[WSLID]));
    XITK_FREE(wl);
  }
}

/**
 * slide down
 */
static void browser_down(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
  xitk_widget_list_t *wl;

  if(w->widget_type & WIDGET_TYPE_BUTTON) {
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_backstep(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, data, xitk_slider_get_pos(private_data->
						  item_tree[WSLID]));
    XITK_FREE(wl);
  }
}

/**
 * slide up (extern).
 */
void xitk_browser_step_up(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  xitk_widget_list_t *wl;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_backstep(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));

    XITK_FREE(wl);
  }
}

/**
 * slide Down (extern).
 */
void xitk_browser_step_down(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  xitk_widget_list_t* wl;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;

    xitk_slider_make_step(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));

    XITK_FREE(wl);
  }
}


/**
 * Handle list selections
 */
static void browser_select(xitk_widget_t *w, void *data, int state) {
  int i, btn_selected;
  browser_private_data_t *private_data = 
    (browser_private_data_t*) ((btnlist_t*)data)->itemlist->private_data;

  if(w->widget_type & WIDGET_TYPE_LABELBUTTON) { 
    if(private_data->dbl_click_callback || private_data->callback) {
      if(!strcasecmp((xitk_labelbutton_get_label(private_data->
						 item_tree[(int)((btnlist_t*)data)->
							  sel])), "")) {
	xitk_labelbutton_set_state((xitk_widget_t*)(private_data->
						    item_tree[(int)((btnlist_t*)data)->
							     sel]),
				   0, private_data->parent_wlist->win, 
				   private_data->parent_wlist->gc);
      }
      
      for(i = WBSTART; i < private_data->max_length+WBSTART; i++) {
	if((xitk_labelbutton_get_state(private_data->item_tree[i]))
	   && (i != ((btnlist_t*)data)->sel)) {
	  xitk_labelbutton_set_state(private_data->item_tree[i], 0, 
				     private_data->parent_wlist->win, 
				     private_data->parent_wlist->gc);
	}
      }
      
      if((i = xitk_browser_get_current_selected(((btnlist_t*)data)->itemlist)) > -1) {
	// Callback call
	if(private_data->callback)
	  private_data->callback(((btnlist_t*)data)->itemlist, (void*)(i));
      }
      
      
      /* A button is currently selected */
      if((btn_selected = 
	  xitk_browser_get_current_selected(((btnlist_t*)data)->itemlist)) > -1) {
	
	private_data->current_button_clicked = btn_selected;
	
	if(private_data->last_button_clicked == private_data->current_button_clicked) {
	  struct timeval old_click_time, tm_diff;
	  long int click_diff;
	  
	  private_data->last_button_clicked = -1;
	  
	  timercpy(&private_data->click_time, &old_click_time);
	  gettimeofday(&private_data->click_time, 0);
	  timersub(&private_data->click_time, &old_click_time, &tm_diff);
	  click_diff = (tm_diff.tv_sec * 1000) + (tm_diff.tv_usec / 1000.0);
	  
	  /* Ok, double click occur, call cb */
	  if(click_diff < private_data->dbl_click_time) {
	    if(private_data->dbl_click_callback)
	      private_data->dbl_click_callback(w, 
					       private_data->userdata, 
					       private_data->current_button_clicked);
	  }
	  
	}
	else {
	  private_data->last_button_clicked = private_data->current_button_clicked;
	}
	
	gettimeofday(&private_data->click_time, 0);
	
	/*
	  if(private_data->last_button_clicked == -1)
	  private_data->last_button_clicked = private_data->current_button_clicked;
	*/
	
      }
      else if((xitk_browser_get_current_selected(((btnlist_t*)data)->itemlist) < 0)
	      && (private_data->current_button_clicked > -1)) {
	struct timeval old_click_time, tm_diff;
	long int click_diff;
	
	timercpy(&private_data->click_time, &old_click_time);
	gettimeofday(&private_data->click_time, 0);
	timersub(&private_data->click_time, &old_click_time, &tm_diff);
	click_diff = (tm_diff.tv_sec * 1000) + (tm_diff.tv_usec / 1000.0);
	
	/* Ok, double click occur, call cb */
	if(click_diff < private_data->dbl_click_time) {
	  if(private_data->dbl_click_callback)
	    private_data->dbl_click_callback(w, 
					     private_data->userdata,
					     private_data->current_button_clicked);
	}
	private_data->last_button_clicked = private_data->current_button_clicked;
	private_data->current_button_clicked = -1;
      }
      
      gettimeofday(&private_data->click_time, 0);
    }
    else {
      /* If there no callback, release selected button */
      if((btn_selected = xitk_browser_get_current_selected(((btnlist_t*)data)->itemlist)) > -1) {
	xitk_labelbutton_set_state(private_data->item_tree[(int)((btnlist_t*)data)->sel], 0, 
				   private_data->parent_wlist->win, 
				   private_data->parent_wlist->gc);
      }
    }
  }
}

/**
 * Create the list browser
 */
static xitk_widget_t *_xitk_browser_create(xitk_skin_config_t *skonfig, xitk_browser_widget_t *br,
					   int x, int y, int width, int height,
					   char *skin_element_name,
					   xitk_widget_t *mywidget,
					   browser_private_data_t *_private_data) {
  browser_private_data_t        *private_data = _private_data;
  xitk_button_widget_t           b;
  xitk_labelbutton_widget_t      lb;
  xitk_slider_widget_t           sl;

  XITK_CHECK_CONSTITENCY(br);

  XITK_WIDGET_INIT(&b, br->imlibdata);
  XITK_WIDGET_INIT(&lb, br->imlibdata);
  XITK_WIDGET_INIT(&sl, br->imlibdata);

  private_data->bWidget              = mywidget;
  private_data->imlibdata            = br->imlibdata;
  private_data->skin_element_name    = (skin_element_name == NULL) ? NULL : strdup(br->browser.skin_element_name);
  private_data->content              = br->browser.entries;
  private_data->list_length          = br->browser.num_entries;
  private_data->max_length           = br->browser.max_displayed_entries;

  if(br->dbl_click_time)
    private_data->dbl_click_time     = br->dbl_click_time;
  else
    private_data->dbl_click_time     = DEFAULT_DBL_CLICK_TIME;

  if(br->dbl_click_callback)
    private_data->dbl_click_callback = br->dbl_click_callback;
  else
    private_data->dbl_click_callback = NULL;

  private_data->current_button_clicked = -1;
  private_data->last_button_clicked    = -1;

  private_data->parent_wlist           = br->parent_wlist;
  
  private_data->callback               = br->callback;
  private_data->userdata               = br->userdata;
  
  mywidget->private_data               = private_data;

  mywidget->enable                     = 1;
  mywidget->running                    = 1;
  mywidget->visible                    = 1;
  mywidget->have_focus                 = FOCUS_LOST;
  
  mywidget->x                          = x;
  mywidget->y                          = y;
  mywidget->width                      = width;
  mywidget->height                     = height;
  
  mywidget->widget_type                = WIDGET_TYPE_BROWSER | WIDGET_TYPE_GROUP;
  mywidget->paint                      = paint;
  mywidget->notify_click               = NULL;
  mywidget->notify_focus               = NULL;
  mywidget->notify_keyevent            = NULL;
  mywidget->notify_change_skin         = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_inside              = NULL;
  mywidget->notify_destroy             = NULL;
  mywidget->get_skin                   = NULL;

  return mywidget;
}

/**
 * Create the list browser
 */
xitk_widget_t *xitk_browser_create(xitk_skin_config_t *skonfig, xitk_browser_widget_t *br) {
  xitk_widget_t                 *mywidget;
  browser_private_data_t        *private_data;
  xitk_button_widget_t           b;
  xitk_labelbutton_widget_t      lb;
  xitk_slider_widget_t           sl;

  XITK_CHECK_CONSTITENCY(br);

  XITK_WIDGET_INIT(&b, br->imlibdata);
  XITK_WIDGET_INIT(&lb, br->imlibdata);
  XITK_WIDGET_INIT(&sl, br->imlibdata);

  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  private_data = (browser_private_data_t *) xitk_xmalloc(sizeof(browser_private_data_t));

  b.skin_element_name = br->arrow_up.skin_element_name;
  b.callback          = browser_up;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			   (private_data->item_tree[WBUP] = xitk_button_create(skonfig, &b)));
  
  sl.min                      = 0;
  sl.max                      = (br->browser.num_entries > (br->browser.max_displayed_entries-1) ? br->browser.num_entries-1 : 0);
  sl.step                     = 1;
  sl.skin_element_name        = br->slider.skin_element_name;
  sl.callback                 = browser_slidmove;
  sl.userdata                 = (void*)mywidget;
  sl.motion_callback          = browser_slidmove;
  sl.motion_userdata          = (void*)mywidget;
  xitk_list_append_content(br->parent_wlist->l,
	  (private_data->item_tree[WSLID] = xitk_slider_create(skonfig, &sl)));
  
  xitk_slider_set_to_max(br->parent_wlist, private_data->item_tree[WSLID]);
  
  b.skin_element_name = br->arrow_dn.skin_element_name;
  b.callback          = browser_down;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			  private_data->item_tree[WBDN] = xitk_button_create (skonfig, &b));
  
  {

    int x, y, i;
    btnlist_t *bt;
    
    x = xitk_skin_get_coord_x(skonfig, br->browser.skin_element_name);
    y = xitk_skin_get_coord_y(skonfig, br->browser.skin_element_name);
    
    for(i = WBSTART; i < br->browser.max_displayed_entries+WBSTART; i++) {
      
      bt = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      bt->itemlist = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
      bt->itemlist = mywidget;
      bt->sel = i;
      
      lb.button_type       = RADIO_BUTTON;
      lb.label             = "";
      lb.align             = LABEL_ALIGN_CENTER;
      lb.callback          = NULL;
      lb.state_callback    = browser_select;
      lb.userdata          = (void *)(bt);
      lb.skin_element_name = br->browser.skin_element_name;
      xitk_list_append_content(br->parent_wlist->l, 
			       (private_data->item_tree[i] = 
				xitk_labelbutton_create (skonfig, &lb)));
      
      (void) xitk_set_widget_pos(private_data->item_tree[i], x, y);

      y += xitk_get_widget_height(private_data->item_tree[i]) + 1;
    }
  }
  
  return _xitk_browser_create(skonfig, br, 
			      0, 0, 0, 0, br->browser.skin_element_name, mywidget, private_data);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_browser_create(xitk_browser_widget_t *br, GC gc, int x, int y, 
					  int itemw, int itemh, int slidw, char *fontname) {
  xitk_widget_t              *mywidget;
  browser_private_data_t     *private_data;
  xitk_button_widget_t        b;
  xitk_labelbutton_widget_t   lb;
  xitk_slider_widget_t        sl;
  int                         btnh = (itemh - (itemh /2)) < slidw ? slidw : (itemh - (itemh /2));

  XITK_CHECK_CONSTITENCY(br);
  XITK_WIDGET_INIT(&b, br->imlibdata);
  XITK_WIDGET_INIT(&lb, br->imlibdata);
  XITK_WIDGET_INIT(&sl, br->imlibdata);

  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  private_data = (browser_private_data_t *) xitk_xmalloc(sizeof(browser_private_data_t));

  b.skin_element_name = NULL;
  b.callback          = browser_up;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			   (private_data->item_tree[WBUP] = 
			    xitk_noskin_button_create(&b,
						      x + itemw, y + 0, slidw, btnh)));
  
  { /* Draw arrow in button */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->item_tree[WBUP]);
    
    if(wimage)
      draw_arrow_up(b.imlibdata, wimage);

  }

  sl.min                      = 0;
  sl.max                      = (br->browser.num_entries > (br->browser.max_displayed_entries-1) ? br->browser.num_entries-1 : 0);
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = browser_slidmove;
  sl.userdata                 = (void*)mywidget;
  sl.motion_callback          = browser_slidmove;
  sl.motion_userdata          = (void*)mywidget;
  xitk_list_append_content(br->parent_wlist->l,
	   (private_data->item_tree[WSLID] = 
	    xitk_noskin_slider_create(&sl,
				      x + itemw, y + btnh, slidw, 
				      (itemh * br->browser.max_displayed_entries) - (btnh * 2),
				      XITK_VSLIDER)));
			   
  xitk_slider_set_to_max(br->parent_wlist, private_data->item_tree[WSLID]);
  
  b.skin_element_name = NULL;
  b.callback          = browser_down;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
	   (private_data->item_tree[WBDN] = 
	    xitk_noskin_button_create(&b,
				      x + itemw, 
				      y + ((itemh * br->browser.max_displayed_entries) - btnh), 
				      slidw, btnh)));
  
  { /* Draw arrow in button */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->item_tree[WBDN]);
    
    if(wimage)
      draw_arrow_down(b.imlibdata, wimage);

  }
  {
    int            ix = x, iy = y, i;
    btnlist_t     *bt;
    xitk_image_t  *wimage;

    for(i = WBSTART; i < br->browser.max_displayed_entries+WBSTART; i++) {
      
      bt = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      bt->itemlist = mywidget;
      bt->sel = i;
      
      lb.button_type       = RADIO_BUTTON;
      lb.label             = "";
      lb.align             = LABEL_ALIGN_CENTER;
      lb.callback          = NULL;
      lb.state_callback    = browser_select;
      lb.userdata          = (void *)(bt);
      lb.skin_element_name = NULL;
      xitk_list_append_content(br->parent_wlist->l, 
			       (private_data->item_tree[i] = 
				xitk_noskin_labelbutton_create (&lb,
								ix, iy,
								itemw, itemh,
								"Black", "Black", "White", 
								fontname)));
      
      wimage = xitk_get_widget_foreground_skin(private_data->item_tree[i]);
      if(wimage)
	draw_flat_three_state(br->imlibdata, wimage);				

      (void) xitk_set_widget_pos(private_data->item_tree[i], ix, iy);

      iy += itemh;
    }
  }
  
  return _xitk_browser_create(NULL, br, x, y, 
			      (itemw + slidw), (itemh * br->browser.max_displayed_entries), 
			      NULL, mywidget, private_data);
}
