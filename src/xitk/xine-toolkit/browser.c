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
#include "inputtext.h"
#include "font.h"

#include "_xitk.h"

typedef struct {
  xitk_widget_t    *itemlist;
  int               sel;
} btnlist_t;


#define WBUP    0  /*  Position of button up in item_tree  */
#define WSLID   1  /*  Position of slider in item_tree  */
#define WBDN    2  /*  Position of button down in item_tree  */
#define WBLF    3
#define WSLIDH  4
#define WBRT    5
#define WBSTART 6  /*  Position of first item button in item_tree */

#define EXTRA_BTNS 50

static void notify_change_skin(xitk_widget_list_t *, xitk_widget_t *, xitk_skin_config_t *);
/*
 *
 */
static void enability(xitk_widget_t *w) {
  browser_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    int i;
    int max;
    
    private_data = (browser_private_data_t *) w->private_data;

    max = private_data->max_length;
    max += (private_data->skin_element_name) ? EXTRA_BTNS : WBSTART;
   
    if(w->enable == WIDGET_ENABLE) {
      for(i = WBUP; i < max; i++)
	xitk_enable_widget(private_data->item_tree[i]);
    }
    else {
      for(i = WBUP; i < max; i++)
	xitk_disable_widget(private_data->item_tree[i]);
    }
  }
}

static void paint(xitk_widget_t *w, Window win, GC gc) {
  browser_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    int i;
    
    private_data = (browser_private_data_t *) w->private_data;

    if((w->visible == 1)) {
      if(!private_data->skin_element_name) {
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
	if(private_data->need_h_slider) {
#warning NEED

	}
	
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBUP]);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WSLID]);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBDN]);

	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBLF]);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WSLIDH]);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[WBRT]);

	if(private_data->need_h_slider) {
#warning NEED
	}

	for(i = WBSTART; i < (private_data->max_length+WBSTART); i++) {
	  (void) xitk_set_widget_pos(private_data->item_tree[i], x, iy);
	  xitk_show_widget(private_data->parent_wlist, private_data->item_tree[i]);
	  iy += itemh;
	}
      }
    }
    else {
      for(i = 0; i < (private_data->max_length+WBSTART); i++) {
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
  browser_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    xitk_skin_lock(skonfig);
    
    if(private_data->skin_element_name) {
      int x, y, i = 0;
      
      x          = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      y          = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->visible = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1 : -1;
      w->enable  = xitk_skin_get_enability(skonfig, private_data->skin_element_name);

      private_data->max_length = xitk_skin_get_browser_entries(skonfig, 
							       private_data->skin_element_name);
      
      for(i = WBSTART; i < (EXTRA_BTNS + WBSTART); i++) {
	xitk_disable_widget(private_data->item_tree[i]);
	xitk_hide_widget(private_data->parent_wlist, private_data->item_tree[i]);
      }

      for(i = WBSTART; i < private_data->max_length + WBSTART; i++) {
	(void) xitk_set_widget_pos(private_data->item_tree[i], x, y);
	y += xitk_get_widget_height(private_data->item_tree[i]) + 1;
	xitk_enable_widget(private_data->item_tree[i]);
	xitk_show_widget(private_data->parent_wlist, private_data->item_tree[i]);
      }

      xitk_browser_rebuild_browser(w, 0);
    }
    xitk_skin_unlock(skonfig);
  }
}

/**
 * Return the number of displayed entries
 */
int xitk_browser_get_num_entries(xitk_widget_t *w) {
  browser_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (browser_private_data_t *) w->private_data;
    return private_data->max_length;
  }

  return -1;
}

/**
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w) {
  browser_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (browser_private_data_t *) w->private_data;
    return private_data->current_start;
  }

  return -1;
}

/**
 * Change browser labels alignment
 */
void xitk_browser_set_alignment(xitk_widget_t *w, int align) {
  browser_private_data_t *private_data;
  int i;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    for(i = 0; i < private_data->max_length; i++) {
      xitk_labelbutton_set_alignment(private_data->item_tree[i+WBSTART], align);
    }
    
    xitk_browser_rebuild_browser(w, -1);
  }
}

/**
 * Return the current selected button (if not, return -1)
 */
int xitk_browser_get_current_selected(xitk_widget_t *w) {
  browser_private_data_t *private_data;
  int                     i;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
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
  browser_private_data_t *private_data;
  int                     i;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
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
  browser_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    xitk_browser_release_all_buttons(w);
    
    xitk_labelbutton_set_state(private_data->item_tree[select+WBSTART], 1, 
			       private_data->parent_wlist->win, private_data->parent_wlist->gc);
  }
}

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  int                     i, j, max;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    xitk_browser_release_all_buttons(w);

    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    j = (private_data->list_length > (private_data->max_length - 1) ? 
	 (private_data->list_length - 1) : 1);

    if(j <= (private_data->max_length - 1))
      max = 0;
    else if(j > (private_data->max_length - 1))
      max = j - (private_data->max_length - 1);
    
    if(((start >= 0) && max) || ((start == 0) && (max == 0)))
      private_data->current_start = start;

    private_data->need_h_slider = 0;
    private_data->labels_offset = 0;

    for(i = 0; i < private_data->max_length; i++) {
      xitk_labelbutton_set_label_offset(private_data->item_tree[i + WBSTART], 0);
    }
    
    for(i = 0; i < private_data->max_length; i++) {
      if (((private_data->current_start + i) < private_data->list_length) 
	  && (private_data->content[private_data->current_start + i] != NULL)) {
	int item_width = xitk_get_widget_width(private_data->item_tree[WBSTART]);
	char *label_font = xitk_labelbutton_get_fontname(private_data->item_tree[WBSTART]);
	
	xitk_labelbutton_change_label(wl, private_data->item_tree[i + WBSTART], 
				      private_data->content[private_data->current_start + i]);
	
	if(label_font) {
	  xitk_font_t *fs = NULL;
	  int          label_width;
	  
	  fs = xitk_font_load_font(private_data->imlibdata->x.disp, label_font);
	  xitk_font_set_font(fs, wl->gc);
	  label_width = xitk_font_get_string_length(fs, private_data->content[private_data->current_start + i]);
	  xitk_font_unload_font(fs);
	  
	  if(label_width > (item_width - 2)) {
	    int diff_width = (label_width - (item_width - 2));
	    
	    private_data->need_h_slider = 1;
	    
	    if(private_data->labels_offset < diff_width)
	      private_data->labels_offset = diff_width;
	    
	  }
	  XITK_FREE(label_font);
	}
      }
      else {
	xitk_labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], "");
      }
    }

    if(private_data->need_h_slider) {
      int align = xitk_labelbutton_get_alignment(private_data->item_tree[WBSTART]);

      switch(align) {
      case LABEL_ALIGN_CENTER:
	xitk_slider_set_min(private_data->item_tree[WSLIDH], -(private_data->labels_offset>>1));
	xitk_slider_set_max(private_data->item_tree[WSLIDH], (private_data->labels_offset>>1));
	break;
      case LABEL_ALIGN_LEFT:
	xitk_slider_set_min(private_data->item_tree[WSLIDH], 0);
	xitk_slider_set_max(private_data->item_tree[WSLIDH], private_data->labels_offset);
	break;
      case LABEL_ALIGN_RIGHT:
	xitk_slider_set_min(private_data->item_tree[WSLIDH], (-private_data->labels_offset) - 3);
	xitk_slider_set_max(private_data->item_tree[WSLIDH], 0);
	break;
      }
    }
    else {
      xitk_slider_set_max(private_data->item_tree[WSLIDH], 1);
      xitk_slider_set_min(private_data->item_tree[WSLIDH], 0);
    }
    xitk_slider_reset(wl, private_data->item_tree[WSLIDH]);
     
    xitk_slider_set_max(private_data->item_tree[WSLID], max);
    
    if(start == 0)
      xitk_slider_set_to_max(wl, private_data->item_tree[WSLID]);
    else if(max)
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
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    private_data->content = list;
    private_data->list_length = len;
    xitk_browser_rebuild_browser(w, start);
  }
}  

/**
 * Handle slider movments
 */
static void browser_slidmove(xitk_widget_t *w, void *data, int pos) {
  browser_private_data_t *private_data;
  int                     realpos;
  
  if(w && ((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {

    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
    
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

static void browser_hslidmove(xitk_widget_t *w, void *data, int pos) {
  browser_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
    xitk_widget_list_t  *wl;
    int                  i;
    
    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    if(private_data->labels_offset) {

      for(i = 0; i < private_data->max_length; i++) {
	if (((private_data->current_start + i) < private_data->list_length)
	    && (private_data->content[private_data->current_start + i] != NULL)) {

	  xitk_labelbutton_set_label_offset(private_data->item_tree[i + WBSTART], -pos);
	  
	  xitk_labelbutton_change_label(wl, private_data->item_tree[i + WBSTART], 
					private_data->content[private_data->current_start + i]);
	  
	}
	else
	  xitk_labelbutton_change_label(wl, private_data->item_tree[i+WBSTART], "");
	
      }
    }
    else
      xitk_slider_reset(wl, private_data->item_tree[WSLIDH]);

    XITK_FREE(wl);

  }
}

/**
 * slide up
 */
static void browser_up(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
 
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;

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
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;

  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;
    
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
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_backstep(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));

    XITK_FREE(wl);
  }
}

void xitk_browser_page_up(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    int min, max, pos, npos, vis;

    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;

    min = xitk_slider_get_min(private_data->item_tree[WSLID]);
    max = xitk_slider_get_max(private_data->item_tree[WSLID]);
    npos = pos = xitk_slider_get_pos(private_data->item_tree[WSLID]);
    vis = private_data->max_length;

    if((pos - vis) >= min)
      npos -= vis;
    else
      npos = min;

    xitk_slider_set_pos(wl, private_data->item_tree[WSLID], npos);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));

    XITK_FREE(wl);
  }
}
void xitk_browser_page_down(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    int min, max, pos, npos, vis;

    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;

    min = xitk_slider_get_min(private_data->item_tree[WSLID]);
    max = xitk_slider_get_max(private_data->item_tree[WSLID]);
    npos = pos = xitk_slider_get_pos(private_data->item_tree[WSLID]);
    vis = private_data->max_length;

    if((pos + vis) <= max)
      npos += vis;
    else
      npos = max;

    xitk_slider_set_pos(wl, private_data->item_tree[WSLID], npos);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));
    
    XITK_FREE(wl);
  }
}

/**
 * slide Down (extern).
 */
void xitk_browser_step_down(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;

    xitk_slider_make_step(wl, private_data->item_tree[WSLID]);
    browser_slidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLID]));

    XITK_FREE(wl);
  }
}


/**
 * slide left (extern).
 */
void xitk_browser_step_left(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_backstep(wl, private_data->item_tree[WSLIDH]);
    browser_hslidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLIDH]));

    XITK_FREE(wl);
  }
}
/**
 * slide right (extern).
 */
void xitk_browser_step_right(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
 
  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {
    
    private_data = (browser_private_data_t *) w->private_data;
    
    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;

    xitk_slider_make_step(wl, private_data->item_tree[WSLIDH]);
    browser_hslidmove(w, w, xitk_slider_get_pos(private_data->item_tree[WSLIDH]));

    XITK_FREE(wl);
  }
}

static void browser_left(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;

    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_backstep(wl, private_data->item_tree[WSLIDH]);
    browser_hslidmove(w, data, xitk_slider_get_pos(private_data->
						   item_tree[WSLIDH]));
    XITK_FREE(wl);
  }
}
static void browser_right(xitk_widget_t *w, void *data) {
  browser_private_data_t *private_data;
  xitk_widget_list_t     *wl;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)) {
    private_data = (browser_private_data_t *) ((xitk_widget_t*)data)->private_data;

    wl = (xitk_widget_list_t*) xitk_xmalloc(sizeof(xitk_widget_list_t));
    wl->win = private_data->parent_wlist->win;
    wl->gc = private_data->parent_wlist->gc;
    
    xitk_slider_make_step(wl, private_data->item_tree[WSLIDH]);
    browser_hslidmove(w, data, xitk_slider_get_pos(private_data->
						   item_tree[WSLIDH]));
    XITK_FREE(wl);
  }
}

/**
 * Handle list selections
 */
static void browser_select(xitk_widget_t *w, void *data, int state) {
  browser_private_data_t *private_data;
  int                     i, btn_selected;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) { 
    
    private_data = (browser_private_data_t*) ((btnlist_t*)data)->itemlist->private_data;
    
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
	private_data->jumped = i;

	if(private_data->callback)
	  private_data->callback(((btnlist_t*)data)->itemlist, private_data->userdata, i);
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
	      private_data->dbl_click_callback(((btnlist_t*)data)->itemlist/*w*/, 
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
	    private_data->dbl_click_callback(((btnlist_t*)data)->itemlist/*w*/, 
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

/*
 *
 */
xitk_widget_t *xitk_browser_get_browser(xitk_widget_list_t *wl, xitk_widget_t *w) {
  xitk_widget_t *widget = NULL;
  
  if(wl && w && ((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {

    widget = (xitk_widget_t *) xitk_list_first_content(wl->l);
    while(widget && (widget != w)) {
      widget = (xitk_widget_t *) xitk_list_next_content(wl->l);
    }
    while(widget && (!(((widget->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
		       (widget->widget_type & WIDGET_GROUP_WIDGET)))) {
      widget = (xitk_widget_t *) xitk_list_next_content(wl->l);
    }
  }

  return widget;
}

/*
 * Jump to entry in list which match with the alphanum char key.
 */
void xitk_browser_warp_jump(xitk_widget_t *w, char *key, int modifier) {
  browser_private_data_t *private_data;

  if(w && (((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER) &&
	   (w->widget_type & WIDGET_GROUP_WIDGET))) {

    private_data = (browser_private_data_t *) w->private_data;
    
    if(((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) || (modifier & MODIFIER_SHIFT)) {
      int jumped;
      int start, end, mark;
      int focus;

      /* Jump wasn't happend yet */
      if(private_data->jumped == -1)
	jumped = 0;
      else {
	if((private_data->jumped + 1) < private_data->list_length)
	  jumped = private_data->jumped + 1;
	else
	  jumped = 0;
      }

      start = jumped;
      end = ((start - 1) > 0) ? (start - 1) : 0;
      mark = private_data->list_length;
      
      for(; start < mark; start++) {
	
	if(!strncasecmp(private_data->content[start], key, strlen(key))) {
	  
	  private_data->jumped = start;

	  focus = 0;
	  if((private_data->list_length - start) <= private_data->max_length) {

	    if(private_data->list_length >= private_data->max_length) {
	      focus = private_data->max_length - (private_data->list_length - start);
	      start = (private_data->list_length - private_data->max_length);
	    }
	    else
	      focus = start;

	  }
	  
	  xitk_browser_rebuild_browser(w, start);

	  if(private_data->parent_wlist->widget_focused) {
	    
	    if(private_data->parent_wlist->widget_focused->notify_focus && 
	       private_data->parent_wlist->widget_focused->enable == WIDGET_ENABLE) {
	      
	      (void) (private_data->parent_wlist->widget_focused->notify_focus) (private_data->parent_wlist, private_data->parent_wlist->widget_focused, FOCUS_LOST);
	      
	      private_data->parent_wlist->widget_focused->have_focus = FOCUS_LOST;
	    }
	    
	    if(private_data->parent_wlist->widget_focused->paint)
	      (private_data->parent_wlist->widget_focused->paint) (private_data->parent_wlist->widget_focused, private_data->parent_wlist->win, private_data->parent_wlist->gc);

	  }
	  
	  xitk_browser_set_select(w, focus);
	  
	  private_data->parent_wlist->widget_focused = private_data->item_tree[WBSTART + focus];
	  
	  if(private_data->parent_wlist->widget_focused->notify_focus && private_data->parent_wlist->widget_focused->enable == WIDGET_ENABLE) {
	    
	    (void) (private_data->parent_wlist->widget_focused->notify_focus) (private_data->parent_wlist, private_data->parent_wlist->widget_focused, FOCUS_RECEIVED);
	    
	    private_data->parent_wlist->widget_focused->have_focus = FOCUS_RECEIVED;
	    
	  }
	  
	  if(private_data->parent_wlist->widget_focused->paint)
	    (private_data->parent_wlist->widget_focused->paint) (private_data->parent_wlist->widget_focused, private_data->parent_wlist->win, private_data->parent_wlist->gc);
	  
	  return;
	}

	if(((start + 1) == mark) && end) {
	  mark = end;
	  start = end = -1;
	}
      }

      private_data->jumped = -1;
    }
  }
}

/**
 * Create the list browser
 */
static xitk_widget_t *_xitk_browser_create(xitk_widget_list_t *wl,
					   xitk_skin_config_t *skonfig, xitk_browser_widget_t *br,
					   int x, int y, int width, int height,
					   char *skin_element_name,
					   xitk_widget_t *mywidget,
					   browser_private_data_t *_private_data,
					   int visible, int enable) {
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
  private_data->jumped               = -1;
  private_data->content              = br->browser.entries;
  private_data->list_length          = br->browser.num_entries;
  private_data->max_length           = br->browser.max_displayed_entries;

  private_data->dbl_click_time       = xitk_get_timer_dbl_click();

  if(br->dbl_click_callback)
    private_data->dbl_click_callback = br->dbl_click_callback;
  else
    private_data->dbl_click_callback = NULL;

  private_data->current_button_clicked = -1;
  private_data->last_button_clicked    = -1;

  private_data->parent_wlist           = br->parent_wlist;
  
  private_data->callback               = br->callback;
  private_data->userdata               = br->userdata;

  private_data->need_h_slider          = 0;
  private_data->labels_offset          = 0;
  
  mywidget->private_data               = private_data;

  mywidget->widget_list                = wl;

  mywidget->enable                     = enable;
  mywidget->running                    = 1;
  mywidget->visible                    = visible;
  mywidget->have_focus                 = FOCUS_LOST;
  
  mywidget->imlibdata                  = private_data->imlibdata;
  mywidget->x                          = x;
  mywidget->y                          = y;
  mywidget->width                      = width;
  mywidget->height                     = height;
  
  mywidget->widget_type                = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_BROWSER;
  mywidget->paint                      = paint;
  mywidget->notify_click               = NULL;
  mywidget->notify_focus               = NULL;
  mywidget->notify_keyevent            = NULL;
  mywidget->notify_change_skin         = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_inside              = NULL;
  mywidget->notify_destroy             = NULL;
  mywidget->get_skin                   = NULL;
  mywidget->notify_enable              = enability;

  mywidget->tips_timeout               = 0;
  mywidget->tips_string                = NULL;

  return mywidget;
}

/**
 * Create the list browser
 */
xitk_widget_t *xitk_browser_create(xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_browser_widget_t *br) {
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
  private_data->skonfig = skonfig;

  
  br->browser.max_displayed_entries = xitk_skin_get_browser_entries(skonfig, 
								    br->browser.skin_element_name);
  
  {

    int x, y, i;
    btnlist_t *bt;
    
    x = xitk_skin_get_coord_x(skonfig, br->browser.skin_element_name);
    y = xitk_skin_get_coord_y(skonfig, br->browser.skin_element_name);
    
    for(i = WBSTART; i < (EXTRA_BTNS + WBSTART); i++) {
      
      bt = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      bt->itemlist = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
      bt->itemlist = mywidget;
      bt->sel = i;
      
      lb.button_type       = RADIO_BUTTON;
      lb.label             = "";
      lb.callback          = NULL;
      lb.state_callback    = browser_select;
      lb.userdata          = (void *)(bt);
      lb.skin_element_name = br->browser.skin_element_name;
      xitk_list_append_content(br->parent_wlist->l, 
			       (private_data->item_tree[i] = 
				xitk_labelbutton_create (br->parent_wlist, skonfig, &lb)));
      private_data->item_tree[i]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
      
      xitk_disable_widget(private_data->item_tree[i]);
      xitk_hide_widget(br->parent_wlist, private_data->item_tree[i]);
      
      (void) xitk_set_widget_pos(private_data->item_tree[i], x, y);
      
      y += xitk_get_widget_height(private_data->item_tree[i]) + 1;
    }
    
    for(i = WBSTART; i < (br->browser.max_displayed_entries + WBSTART); i++) {
      xitk_enable_widget(private_data->item_tree[i]);
      xitk_show_widget(br->parent_wlist, private_data->item_tree[i]);
    }

  }
  
  b.skin_element_name = br->arrow_up.skin_element_name;
  b.callback          = browser_up;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			   (private_data->item_tree[WBUP] = 
			    xitk_button_create(br->parent_wlist, skonfig, &b)));
  private_data->item_tree[WBUP]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
  
  sl.min                      = 0;
  sl.max                      = (br->browser.num_entries > (br->browser.max_displayed_entries-1) ? br->browser.num_entries-1 : 0);
  sl.step                     = 1;
  sl.skin_element_name        = br->slider.skin_element_name;
  sl.callback                 = browser_slidmove;
  sl.userdata                 = (void*)mywidget;
  sl.motion_callback          = browser_slidmove;
  sl.motion_userdata          = (void*)mywidget;
  xitk_list_append_content(br->parent_wlist->l,
	  (private_data->item_tree[WSLID] = xitk_slider_create(br->parent_wlist, skonfig, &sl)));
  private_data->item_tree[WSLID]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  xitk_slider_set_to_max(br->parent_wlist, private_data->item_tree[WSLID]);
  
  b.skin_element_name = br->arrow_dn.skin_element_name;
  b.callback          = browser_down;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			  private_data->item_tree[WBDN] = 
			   xitk_button_create (br->parent_wlist, skonfig, &b));
  private_data->item_tree[WBDN]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
  
  b.skin_element_name = br->arrow_left.skin_element_name;
  b.callback          = browser_left;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			   (private_data->item_tree[WBLF] = 
			    xitk_button_create(br->parent_wlist, skonfig, &b)));
  private_data->item_tree[WBLF]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
  
  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = br->slider_h.skin_element_name;
  sl.callback                 = browser_hslidmove;
  sl.userdata                 = (void*)mywidget;
  sl.motion_callback          = browser_hslidmove;
  sl.motion_userdata          = (void*)mywidget;
  xitk_list_append_content(br->parent_wlist->l,
	  (private_data->item_tree[WSLIDH] = xitk_slider_create(br->parent_wlist, skonfig, &sl)));
  private_data->item_tree[WSLIDH]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  xitk_slider_reset(br->parent_wlist, private_data->item_tree[WSLIDH]);
  
  b.skin_element_name = br->arrow_right.skin_element_name;
  b.callback          = browser_right;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			  private_data->item_tree[WBRT] = 
			   xitk_button_create (br->parent_wlist, skonfig, &b));
  private_data->item_tree[WBRT]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  return _xitk_browser_create(wl, skonfig, br, 
			      0, 0, 0, 0, br->browser.skin_element_name, mywidget, private_data,
			      (xitk_skin_get_visibility(skonfig, br->browser.skin_element_name)) ? 1 : -1,
			      xitk_skin_get_enability(skonfig, br->browser.skin_element_name));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_browser_create(xitk_widget_list_t *wl,
					  xitk_browser_widget_t *br, GC gc, int x, int y, 
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
  private_data->skonfig = NULL;

  {
    int            ix = x, iy = y, i;
    btnlist_t     *bt;
    xitk_image_t  *wimage;

    for(i = WBSTART; i < (br->browser.max_displayed_entries+WBSTART); i++) {

      bt = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      bt->itemlist = mywidget;
      bt->sel = i;
      
      lb.button_type       = RADIO_BUTTON;
      lb.label             = "";
      lb.align             = LABEL_ALIGN_LEFT;
      lb.callback          = NULL;
      lb.state_callback    = browser_select;
      lb.userdata          = (void *)(bt);
      lb.skin_element_name = NULL;
      xitk_list_append_content(br->parent_wlist->l, 
			       (private_data->item_tree[i] = 
				xitk_noskin_labelbutton_create (br->parent_wlist, &lb,
								ix, iy,
								itemw, itemh,
								"Black", "Black", "White", 
								fontname)));
      
      private_data->item_tree[i]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

      wimage = xitk_get_widget_foreground_skin(private_data->item_tree[i]);
      if(wimage)
	draw_flat_three_state(br->imlibdata, wimage);				

      (void) xitk_set_widget_pos(private_data->item_tree[i], ix, iy);
      
      iy += itemh;
   
    }
  }

  b.skin_element_name = NULL;
  b.callback          = browser_up;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
			   (private_data->item_tree[WBUP] = 
			    xitk_noskin_button_create(br->parent_wlist, &b,
						      x + itemw, y + 0, slidw, btnh)));
  private_data->item_tree[WBUP]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

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
	    xitk_noskin_slider_create(br->parent_wlist, &sl,
				      x + itemw, y + btnh, slidw, 
				      (itemh * br->browser.max_displayed_entries) - (btnh * 2),
				      XITK_VSLIDER)));
  private_data->item_tree[WSLID]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
			   
  xitk_slider_set_to_max(br->parent_wlist, private_data->item_tree[WSLID]);
  
  b.skin_element_name = NULL;
  b.callback          = browser_down;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
	   (private_data->item_tree[WBDN] = 
	    xitk_noskin_button_create(br->parent_wlist, &b,
				      x + itemw, 
				      y + ((itemh * br->browser.max_displayed_entries) - btnh), 
				      slidw, btnh)));
  private_data->item_tree[WBDN]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  { /* Draw arrow in button */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->item_tree[WBDN]);
    
    if(wimage)
      draw_arrow_down(b.imlibdata, wimage);

  }
  
  b.skin_element_name = NULL;
  b.callback          = browser_left;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
	   (private_data->item_tree[WBLF] = 
	    xitk_noskin_button_create(br->parent_wlist, &b,
				      x, 
				      y + (itemh * br->browser.max_displayed_entries), 
				      slidw, btnh)));
  private_data->item_tree[WBLF]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  { /* Draw arrow in button */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->item_tree[WBLF]);
    
    if(wimage)
      draw_arrow_left(b.imlibdata, wimage);

  }

  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = browser_hslidmove;
  sl.userdata                 = (void*)mywidget;
  sl.motion_callback          = browser_hslidmove;
  sl.motion_userdata          = (void*)mywidget;
  xitk_list_append_content(br->parent_wlist->l,
	   (private_data->item_tree[WSLIDH] = 
	    xitk_noskin_slider_create(br->parent_wlist, &sl,
				      x + btnh, 
				      y + (itemh * br->browser.max_displayed_entries), 
				      (itemw - (btnh * 2)),
				      slidw,
				      XITK_HSLIDER)));
  private_data->item_tree[WSLIDH]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
			   
  xitk_slider_reset(br->parent_wlist, private_data->item_tree[WSLIDH]);
  
  b.skin_element_name = NULL;
  b.callback          = browser_right;
  b.userdata          = (void *)mywidget;
  xitk_list_append_content(br->parent_wlist->l, 
	   (private_data->item_tree[WBRT] = 
	    xitk_noskin_button_create(br->parent_wlist, &b,
				      x + (itemw - btnh), 
				      y + (itemh * br->browser.max_displayed_entries), 
				      slidw, btnh)));
  private_data->item_tree[WBRT]->widget_type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;

  { /* Draw arrow in button */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(private_data->item_tree[WBRT]);
    
    if(wimage)
      draw_arrow_right(b.imlibdata, wimage);

  }

  return _xitk_browser_create(wl, NULL, br, x, y, 
			      (itemw + slidw), (itemh * br->browser.max_displayed_entries) + slidw, 
			      NULL, mywidget, private_data, 1, 1);
}
