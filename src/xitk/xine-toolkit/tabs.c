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
#include "image.h"
#include "labelbutton.h"
#include "button.h"
#include "tabs.h"
#include "font.h"
#include "widget_types.h"

#include "_xitk.h"


typedef struct {
  xitk_widget_t    *itemlist;
  int               sel;
} btnlist_t;

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  tabs_private_data_t *private_data;

  if(w->widget_type & WIDGET_TYPE_TABS) {
    int i;
    private_data = (tabs_private_data_t *) w->private_data;

    XITK_FREE(private_data->skin_element_name);
    for(i = 0; i <= private_data->num_entries; i++) {
      xitk_destroy_widget(private_data->parent_wlist, private_data->tabs[i]);
    }
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static void tabs_arrange(xitk_widget_t *w) {
  tabs_private_data_t  *private_data;

  if((w->widget_type & WIDGET_TYPE_TABS) && (w->visible)) {
    int i, width, x;

    private_data = (tabs_private_data_t*) w->private_data;

    for(i = 0; i < private_data->num_entries; i++)
      xitk_hide_widget(private_data->parent_wlist, private_data->tabs[i]);

    i = private_data->offset;
    width = 0;
    x = private_data->x;

    do {      
      if((width + xitk_get_widget_width(private_data->tabs[i])) <= private_data->width - 40) {
	xitk_set_widget_pos(private_data->tabs[i], x, private_data->y);
	width += xitk_get_widget_width(private_data->tabs[i]);
	x += xitk_get_widget_width(private_data->tabs[i]);
	xitk_show_widget(private_data->parent_wlist, private_data->tabs[i]);
      }
      else 
	break;
      
      i++;

    } while((i < private_data->num_entries) && (width < (private_data->width - 40)));

    /*
     * Fill gap
     */
    {
      xitk_image_t *p;
      GC            gc;
      XGCValues     gcv;

      p = xitk_image_create_image(private_data->imlibdata, 
				  ((private_data->width - 40) - width) * 3, private_data->bheight);

      if(p) {
	draw_tab(private_data->imlibdata, p);

	XLOCK(private_data->imlibdata->x.disp);
	gcv.graphics_exposures = False;
	gc = XCreateGC(private_data->imlibdata->x.disp, p->image, GCGraphicsExposures, &gcv);
	XCopyArea(private_data->imlibdata->x.disp, p->image, private_data->parent_wlist->win, gc, 
		  0, 0, p->width/3, p->height, private_data->x + width, private_data->y);
	XFreeGC(private_data->imlibdata->x.disp, gc);
	XUNLOCK(private_data->imlibdata->x.disp);
	xitk_image_free_image(private_data->imlibdata, &p);
      }
    }
    if(i < private_data->num_entries)
      xitk_start_widget(private_data->right);
    else
      xitk_stop_widget(private_data->right);

    if(private_data->offset == 0)
      xitk_stop_widget(private_data->left);
    else
      xitk_start_widget(private_data->left);
      

  }

}

/*
 *
 */
static void paint(xitk_widget_t *w, Window win, GC gc) {

  if(w->widget_type & WIDGET_TYPE_TABS) {
    if(w->visible) {
      tabs_arrange(w);
    }
  }
}

/*
 *
 */
static void tabs_select(xitk_widget_t *w, void *data, int select) {
  tabs_private_data_t *private_data = (tabs_private_data_t*) ((btnlist_t*)data)->itemlist->private_data;

  if(select) {
    private_data->old_selected = private_data->selected;
    private_data->selected = (int)((btnlist_t*)data)->sel;

    xitk_labelbutton_set_state(private_data->tabs[private_data->old_selected], 0,
			       private_data->parent_wlist->win, private_data->parent_wlist->gc);


    tabs_arrange(private_data->widget);
    if(private_data->callback)
      private_data->callback(private_data->widget, private_data->userdata, private_data->selected);
  }
  else {
    xitk_labelbutton_set_state(private_data->tabs[private_data->selected], 1,
			       private_data->parent_wlist->win, private_data->parent_wlist->gc);
  }
  
}

/*
 *
 */
static void tabs_select_prev(xitk_widget_t *w, void *data) {
  xitk_widget_t *t = (xitk_widget_t *)data;
  tabs_private_data_t *private_data = (tabs_private_data_t*)t->private_data;

  if(private_data->offset > 0) {
    private_data->offset--;
    tabs_arrange(private_data->widget);
  }
}

/*
 *
 */
static void tabs_select_next(xitk_widget_t *w, void *data) {
  xitk_widget_t *t = (xitk_widget_t *)data;
  tabs_private_data_t *private_data = (tabs_private_data_t*)t->private_data;

  if(private_data->offset < (private_data->num_entries - 1)) {
    private_data->offset++;
    tabs_arrange(private_data->widget);
  }
}

/*
 *
 */
void xitk_tabs_set_current_selected(xitk_widget_t *w, int select) {
  tabs_private_data_t *private_data;
  
  if(!w)
    XITK_WARNING("%s(): widget is NULL\n", __FUNCTION__);
  
  if(w->widget_type & WIDGET_TYPE_TABS) {
    private_data = (tabs_private_data_t*)w->private_data;
    
    if(select <= private_data->num_entries) {
      private_data->old_selected = private_data->selected;
      private_data->selected = select;
      tabs_arrange(w);
    }

  }
}

/*
 *
 */
int xitk_tabs_get_current_selected(xitk_widget_t *w) {
  tabs_private_data_t *private_data;

  if(!w) {
    XITK_WARNING("%s(): widget is NULL\n", __FUNCTION__);
    return -1;
  }
    
  if(w->widget_type & WIDGET_TYPE_TABS) {
    private_data = (tabs_private_data_t*)w->private_data;
    return (private_data->selected);
  }


  return -1;
}

/*
 *
 */
char *xitk_tabs_get_current_tab_selected(xitk_widget_t *w) {
  tabs_private_data_t *private_data;

  if(!w) {
    XITK_WARNING("%s(): widget is NULL\n", __FUNCTION__);
    return NULL;
  }

  if(w->widget_type & WIDGET_TYPE_TABS) {
    private_data = (tabs_private_data_t*)w->private_data;
    return ((xitk_labelbutton_get_label(private_data->tabs[private_data->selected])));
  }
  
  return NULL;
}

/*
 *
 */
xitk_widget_t *xitk_noskin_tabs_create(xitk_tabs_widget_t *t, int x, int y, int width) {
  xitk_widget_t         *mywidget;
  tabs_private_data_t   *private_data;
  
  XITK_CHECK_CONSTITENCY(t);
  
  if((t->entries == NULL) || (t->num_entries == 0))
    XITK_DIE("%s(): entries should be non NULL.\n", __FUNCTION__);

  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  private_data = (tabs_private_data_t *) xitk_xmalloc(sizeof(tabs_private_data_t));
  
  private_data->imlibdata   = t->imlibdata;
  private_data->widget      = mywidget;

  private_data->entries     = t->entries;
  private_data->num_entries = t->num_entries;

  private_data->parent_wlist = t->parent_wlist;

  private_data->x           = x;
  private_data->y           = y;
  private_data->width       = width;

  private_data->callback    = t->callback;
  private_data->userdata    = t->userdata;
    
  private_data->skin_element_name = (t->skin_element_name == NULL) ? NULL : strdup(t->skin_element_name);
  
  {
    xitk_font_t               *fs;
    int                        fwidth, fheight, i;
    xitk_labelbutton_widget_t  lb;
    xitk_button_widget_t       b;
    int                        xx = x;
    btnlist_t                 *bt;
      
    fs = xitk_font_load_font(t->imlibdata->x.disp, DEFAULT_FONT_12);

    xitk_font_set_font(fs, t->parent_wlist->gc);
    fheight = xitk_font_get_string_height(fs, "HEIGHT");

    XITK_WIDGET_INIT(&lb, t->imlibdata);
    XITK_WIDGET_INIT(&b, t->imlibdata);

    private_data->bheight = fheight * 3;

    for(i = 0; i < t->num_entries; i++) {

      bt = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      bt->itemlist = mywidget;
      bt->sel = i;

      fwidth = xitk_font_get_string_length(fs, t->entries[i]);

      lb.skin_element_name = NULL;
      lb.button_type       = RADIO_BUTTON;
      lb.label             = t->entries[i];
      lb.callback          = NULL;
      lb.state_callback    = tabs_select;
      lb.userdata          = (void *)bt;
      xitk_list_append_content (t->parent_wlist->l, 
				(private_data->tabs[i] = 
				 xitk_noskin_labelbutton_create (&lb, xx, y, fwidth + 20, 
								 private_data->bheight, 
								 "Black", "Black", "Black",
								 DEFAULT_FONT_12)));
      xx += fwidth + 20;

      xitk_hide_widget(private_data->parent_wlist, private_data->tabs[i]);
      draw_tab(t->imlibdata, (xitk_get_widget_foreground_skin(private_data->tabs[i])));
      
    }

    /* 
       Add left/rigth arrows 
       FIXME: draw arrows
    */
    {
      xitk_image_t  *wimage;

      b.skin_element_name = NULL;
      b.callback          = tabs_select_prev;
      b.userdata          = (void *)mywidget;
      xitk_list_append_content(t->parent_wlist->l,
       (private_data->left = xitk_noskin_button_create(&b, (private_data->x + width) - 40, 
						       y + (private_data->bheight - 20), 20, 20)));
      
      wimage = xitk_get_widget_foreground_skin(private_data->left);
      if(wimage)
	draw_arrow_left(t->imlibdata, wimage);

      xx += 20;
      b.skin_element_name = NULL;
      b.callback          = tabs_select_next;
      b.userdata          = (void *)mywidget;
      xitk_list_append_content(t->parent_wlist->l,
       (private_data->right = xitk_noskin_button_create(&b, (private_data->x + width) - 20,
							y + (private_data->bheight - 20), 20, 20)));

      wimage = xitk_get_widget_foreground_skin(private_data->right);
      if(wimage)
	draw_arrow_right(t->imlibdata, wimage);

    }

    private_data->old_selected = private_data->selected = 0;
    private_data->offset = 0;
    
    xitk_font_unload_font(fs);
  }  

  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;

  mywidget->have_focus            = FOCUS_LOST; 
  mywidget->x = mywidget->y = mywidget->width = mywidget->height = 0;
  mywidget->widget_type           = WIDGET_TYPE_TABS | WIDGET_TYPE_GROUP;
  mywidget->paint                 = paint;
  mywidget->notify_click          = NULL;
  mywidget->notify_focus          = NULL;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_inside         = NULL;
  mywidget->notify_change_skin    = NULL;
  mywidget->notify_destroy        = NULL;//notify_destroy;
  mywidget->get_skin              = NULL;

  xitk_labelbutton_set_state(private_data->tabs[private_data->selected], 1,
  			     private_data->parent_wlist->win, private_data->parent_wlist->gc);
    

  return mywidget;
}

