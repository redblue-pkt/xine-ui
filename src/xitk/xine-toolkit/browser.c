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

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "widget_types.h"
#include "browser.h"
#include "list.h"
#include "button.h"
#include "labelbutton.h"
#include "slider.h"

#define WBUP    0  /*  Position of button up in item_tree  */
#define WSLID   1  /*  Position of slider in item_tree  */
#define WBDN    2  /*  Position of button down in item_tree  */
#define WBSTART 3  /*  Position of first item button in item_tree */

/**
 * Return the real number of first displayed in list
 */
int browser_get_current_start(widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    return private_data->current_start;
  }

  return -1;
}

/**
 * Return the current selected button (if not, return -1)
 */
int browser_get_current_selected(widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  int i;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    for(i = 0; i < private_data->max_length; i++) {
      if(labelbutton_get_state(private_data->item_tree[i+WBSTART]))
	return (private_data->current_start + i);
    }
  }

  return -1;    
}

/**
 * Release all enabled buttons
 */
void browser_release_all_buttons(widget_t *w) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  int i;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    for(i = 0; i < private_data->max_length; i++) {
      labelbutton_set_state(private_data->item_tree[i+WBSTART], 0, 
			    private_data->win,
			    private_data->gc);
    }
  }
}

/**
 * Select the item 'select' in list
 */
void browser_set_select(widget_t *w, int select) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    browser_release_all_buttons(w);
    
    labelbutton_set_state(private_data->item_tree[select+WBSTART], 1, 
			  private_data->win, private_data->gc);
  }
}

/**
 * Redraw buttons/slider
 */
void browser_rebuild_browser(widget_t *w, int start) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) w->private_data;
  widget_list_t* wl;
  int i, j;

  if(w->widget_type & WIDGET_TYPE_BROWSER) {
    browser_release_all_buttons(w);
    
    if(start >= 0)
      private_data->current_start = start;
    
    wl = (widget_list_t*) gui_xmalloc(sizeof(widget_list_t));
    wl->win = private_data->win;
    wl->gc = private_data->gc;
    
    for(i = 0; i < private_data->max_length; i++) {
      if (((private_data->current_start+i)<private_data->list_length) 
	  && (private_data->content[private_data->current_start+i] != NULL)) {
	labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], 
				 private_data->content[private_data->
						      current_start+i]);
      }
      else {
	labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], "");
      }
    }
    j = (private_data->list_length > (private_data->max_length-1) ? 
	 (private_data->list_length-1) : 1);
    slider_set_max(private_data->item_tree[WSLID], j);
    
    if(start == 0)
      slider_set_to_max(wl, private_data->item_tree[WSLID]);
    else if(j>1)
      slider_set_pos(wl, private_data->item_tree[WSLID], 
		     (slider_get_max(private_data->item_tree[WSLID]) 
		      - slider_get_min(private_data->item_tree[WSLID]) 
		      - (private_data->current_start)));
    
    free(wl);
  }
}

/**
 * Update the list, and rebuild button list
 */
void browser_update_list(widget_t *w, char **list, int len, int start) {
  browser_private_data_t *private_data;

  if(w) {
    if(w->widget_type & WIDGET_TYPE_BROWSER) {
      private_data = (browser_private_data_t *) w->private_data;
      private_data->content = list;
      private_data->list_length = len;
      private_data->current_start = start;
      browser_rebuild_browser(w, start);
    }
  }
}  

/**
 * Handle slider movments
 */
static void browser_slidmove(widget_t *w, void *data, int pos) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((widget_t*)data)->private_data;
  int realpos;
  
  
  if(w->widget_type & (WIDGET_TYPE_SLIDER | WIDGET_TYPE_BUTTON)) {
    if(browser_get_current_selected(((widget_t*)data)) > -1)
      browser_release_all_buttons(((widget_t*)data));
    
    realpos = 
      slider_get_max(private_data->item_tree[WSLID]) 
      - slider_get_min(private_data->item_tree[WSLID]) 
      - pos;
    
    if(private_data->list_length > (private_data->max_length-1))
      browser_rebuild_browser(((widget_t*)data), realpos);
    else
      browser_rebuild_browser(((widget_t*)data), -1);
  }
}

/**
 * slide up
 */
static void browser_up(widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((widget_t*)data)->private_data;
  widget_list_t* wl;

  if(w->widget_type & WIDGET_TYPE_BUTTON) {
    wl = (widget_list_t*) gui_xmalloc(sizeof(widget_list_t));
    wl->win = private_data->win;
    wl->gc = private_data->gc;
    
    slider_make_step(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, data, slider_get_pos(private_data->
						item_tree[WSLID]));
    free(wl);
  }
}

/**
 * slide down
 */
static void browser_down(widget_t *w, void *data) {
  browser_private_data_t *private_data = 
    (browser_private_data_t *) ((widget_t*)data)->private_data;
  widget_list_t* wl;

  if(w->widget_type & WIDGET_TYPE_BUTTON) {
    wl = (widget_list_t*) gui_xmalloc(sizeof(widget_list_t));
    wl->win = private_data->win;
    wl->gc = private_data->gc;
    
    slider_make_backstep(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, data, slider_get_pos(private_data->
						item_tree[WSLID]));
    free(wl);
  }
}

/**
 * Handle list selections
 */
static void browser_select(widget_t *w, void *data, int state) {
  int i;
  browser_private_data_t *private_data = 
    (browser_private_data_t*) ((btnlist_t*)data)->itemlist->private_data;

  if(w->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(!strcasecmp((labelbutton_get_label(private_data->
					  item_tree[(int)((btnlist_t*)data)->
						   sel])), "")) {
      labelbutton_set_state((widget_t*)(private_data->
					item_tree[(int)((btnlist_t*)data)->
						 sel]),
			    0, private_data->win, private_data->gc);
    }
    
    for(i = WBSTART; i < private_data->max_length+WBSTART; i++) {
      if((labelbutton_get_state(private_data->item_tree[i]))
	 && (i != ((btnlist_t*)data)->sel)) {
	labelbutton_set_state(private_data->item_tree[i], 0, 
			    private_data->win, private_data->gc);
      }
    }
    
    if((i = browser_get_current_selected(((btnlist_t*)data)->itemlist)) > -1) {
      // Callback call
      if(private_data->function)
	private_data->function(((btnlist_t*)data)->itemlist, (void*)(i));
    }
  }
  
}

/**
 * Create the list browser
 */
widget_t *create_browser(Display *display, ImlibData *idata,
			 widget_list_t *thelist,
			 browser_placements_t *bp) {
  widget_t *mywidget;
  browser_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc(sizeof(widget_t));

  private_data = 
    (browser_private_data_t *) gui_xmalloc(sizeof(browser_private_data_t));

  private_data->bWidget = mywidget;
  private_data->content = bp->browser.entries;
  private_data->list_length = bp->browser.num_entries;
  private_data->max_length = bp->browser.max_displayed_entries;

  gui_list_append_content(thelist->l, 
	  (private_data->item_tree[WBUP] = 
	   create_button(display, idata, 
			 bp->arrow_up.x, bp->arrow_up.y, 
			 browser_up, (void*)mywidget, 
			 bp->arrow_up.skinfile)));
  
  gui_list_append_content(thelist->l,
	  (private_data->item_tree[WSLID] = 
	   create_slider(display, idata, VSLIDER,
			 bp->slider.x, bp->slider.y, 0,
			 (bp->browser.num_entries > (bp->browser.max_displayed_entries-1)
			  ? bp->browser.num_entries-1 : 0), 
			 1, bp->slider.skinfile, bp->paddle.skinfile,
			 browser_slidmove, (void*)mywidget,
			 browser_slidmove, (void*)mywidget)));
  
  slider_set_to_max(thelist, private_data->item_tree[WSLID]);
  
  gui_list_append_content(thelist->l, 
	  (private_data->item_tree[WBDN] = 
	   create_button (display, idata, 
			  bp->arrow_dn.x, bp->arrow_dn.y, 
			  browser_down, 
			  (void*)mywidget, bp->arrow_dn.skinfile)));
  
  {
    int x, y, i;
    btnlist_t *bt;
    
    x = bp->browser.x;
    y = bp->browser.y;
    
    for(i = WBSTART; i < bp->browser.max_displayed_entries+WBSTART; i++) {
      
      bt = (btnlist_t *) gui_xmalloc(sizeof(btnlist_t));
      bt->itemlist = (widget_t *) gui_xmalloc(sizeof(widget_t));
      bt->itemlist = mywidget;
      bt->sel = i;
      
      gui_list_append_content(thelist->l, 
		      (private_data->item_tree[i] =
		       create_label_button (display, idata, 
					    x, y,
					    RADIO_BUTTON, 
					    "",
					    browser_select, 
					    (void*)(bt), 
					    bp->browser.skinfile,
					    bp->browser.norm_color,
					    bp->browser.focused_color, 
					    bp->browser.clicked_color)));
      
      y += widget_get_height(private_data->item_tree[i]) + 1;
    }
  }
  
  private_data->win = thelist->win;
  private_data->gc = thelist->gc;
  
  private_data->function = bp->callback;
  private_data->user_data = bp->user_data;
  
  mywidget->private_data = private_data;

  mywidget->enable = 1;
  
  mywidget->x = mywidget->x = mywidget->width = mywidget->height = 0;    
  
  mywidget->widget_type = WIDGET_TYPE_BROWSER | WIDGET_TYPE_GROUP;
  mywidget->paint = NULL;
  mywidget->notify_click = NULL;
  mywidget->notify_focus = NULL;

  return mywidget;
}
