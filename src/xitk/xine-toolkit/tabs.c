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

#include <stdio.h>

#include "_xitk.h"


static void tabs_arrange(xitk_widget_t *);

/*
 *
 */
static void enability(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_TABS) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    tabs_private_data_t *private_data = (tabs_private_data_t *) w->private_data;
    int                  i;

    if(w->enable == WIDGET_ENABLE) {
      xitk_enable_and_show_widget(private_data->left);
      xitk_enable_and_show_widget(private_data->right);
      for(i = 0; i < private_data->num_entries; i++)
	xitk_enable_widget(private_data->tabs[i]);
    }
    else {
      xitk_disable_widget(private_data->left);
      xitk_disable_widget(private_data->right);
      for(i = 0; i < private_data->num_entries; i++) {
	xitk_disable_widget(private_data->tabs[i]);
      }
    }
  }
}
  
static void notify_destroy(xitk_widget_t *w) {
  tabs_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_TABS) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    int i;
    
    private_data = (tabs_private_data_t *) w->private_data;

    for(i = 0; i <= private_data->num_entries; i++)
      XITK_FREE(private_data->bt[i]);

    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static void tabs_arrange(xitk_widget_t *w) {
  tabs_private_data_t  *private_data;

  if(w && ((((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_TABS) &&
	    (w->type & WIDGET_GROUP_WIDGET)) && w->visible == 1)) {
    int i = 0, width, x;
    
    private_data = (tabs_private_data_t*) w->private_data;
    
    if(private_data->offset != private_data->old_offset) {
      
      for(i = 0; i < private_data->num_entries; i++)
	xitk_hide_widget(private_data->tabs[i]);
      
      i = private_data->offset;
      width = 0;
      x = private_data->x;

      do {      
	if((width + xitk_get_widget_width(private_data->tabs[i])) <= private_data->width - 40) {
	  xitk_set_widget_pos(private_data->tabs[i], x, private_data->y);
	  width += xitk_get_widget_width(private_data->tabs[i]);
	  x += xitk_get_widget_width(private_data->tabs[i]);
	  xitk_show_widget(private_data->tabs[i]);
	}
	else 
	  break;
	
	i++;
	
      } while((i < private_data->num_entries) && (width < (private_data->width - 40)));
      private_data->gap_widthstart = width;
    }
    /*
     * Fill gap
     */
    if((((private_data->width - 40) - private_data->gap_widthstart) * 3) > 0) {
      xitk_image_t *p;
      GC            gc;
      XGCValues     gcv;
      
      p = xitk_image_create_image(private_data->imlibdata, 
				  ((private_data->width - 40) - private_data->gap_widthstart) * 3,
				  private_data->bheight>>1);
      
      if(p) {
	gcv.graphics_exposures = False;

        draw_flat(p->image, p->width, p->height);
	
        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	gc = XCreateGC(private_data->imlibdata->x.disp, 
		       p->image->pixmap, GCGraphicsExposures, &gcv);
	XCopyArea(private_data->imlibdata->x.disp, 
		  p->image->pixmap, private_data->parent_wlist->win, gc, 
		  0, 0, p->width/3, p->height, 
		  private_data->x + private_data->gap_widthstart, private_data->y);
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	
	draw_tab(private_data->imlibdata, p);
	
        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	XCopyArea(private_data->imlibdata->x.disp, 
		  p->image->pixmap, private_data->parent_wlist->win, gc, 
		  0, 0, p->width/3, p->height, 
		  private_data->x + private_data->gap_widthstart, private_data->y + p->height);
	XFreeGC(private_data->imlibdata->x.disp, gc);
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

        xitk_image_free_image(&p);
      }
    }

    if(private_data->offset != private_data->old_offset) {
      if(i < private_data->num_entries)
	xitk_start_widget(private_data->right);
      else
	xitk_stop_widget(private_data->right);
      
      if(private_data->offset == 0)
	xitk_stop_widget(private_data->left);
      else
	xitk_start_widget(private_data->left);
    }

    private_data->old_offset = private_data->offset;
  } 
}

/*
 *
 */
static void paint(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_TABS) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

    if(w->visible == 1) {
      tabs_arrange(w);
    }
  }
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint(w);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_ENABLE:
    enability(w);
    break;
  }
  
  return retval;
}

/*
 *
 */
static void tabs_select(xitk_widget_t *w, void *data, int select) {
  tabs_private_data_t *private_data = (tabs_private_data_t*) ((btnlist_t*)data)->itemlist->private_data;

  if(select) {
    private_data->old_selected = private_data->selected;
    private_data->selected = (int)((btnlist_t*)data)->sel;

    xitk_labelbutton_set_state(private_data->tabs[private_data->old_selected], 0);


    //    tabs_arrange(private_data->widget);
    if(private_data->callback)
      private_data->callback(private_data->widget, private_data->userdata, private_data->selected);
  }
  else {
    xitk_labelbutton_set_state(private_data->tabs[private_data->selected], 1);
  }
  
}

/*
 *
 */
static void tabs_select_prev(xitk_widget_t *w, void *data) {
  xitk_widget_t *t = (xitk_widget_t *)data;
  tabs_private_data_t *private_data = (tabs_private_data_t*)t->private_data;

  if(private_data->offset > 0) {
    private_data->old_offset = private_data->offset;
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
    private_data->old_offset = private_data->offset;
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
  
  if(w && (((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_TABS) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
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
    
  if(((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_TABS) &&
     (w->type & WIDGET_GROUP_WIDGET)) {

    private_data = (tabs_private_data_t*)w->private_data;
    return (private_data->selected);
  }


  return -1;
}

/*
 *
 */
const char *xitk_tabs_get_current_tab_selected(xitk_widget_t *w) {
  tabs_private_data_t *private_data;

  if(!w) {
    XITK_WARNING("%s(): widget is NULL\n", __FUNCTION__);
    return NULL;
  }

  if(((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_TABS) &&
     (w->type & WIDGET_GROUP_WIDGET)) {

    private_data = (tabs_private_data_t*)w->private_data;
    return ((xitk_labelbutton_get_label(private_data->tabs[private_data->selected])));
  }
  
  return NULL;
}

/*
 *
 */
xitk_widget_t *xitk_noskin_tabs_create(xitk_widget_list_t *wl,
				       xitk_tabs_widget_t *t, 
                                       int x, int y, int width,
                                       const char *fontname) {
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
      
    fs = xitk_font_load_font(t->imlibdata->x.disp, fontname);

    xitk_font_set_font(fs, t->parent_wlist->gc);
    fheight = xitk_font_get_string_height(fs, " ");

    XITK_WIDGET_INIT(&lb, t->imlibdata);
    XITK_WIDGET_INIT(&b, t->imlibdata);

    private_data->bheight = fheight + 18;

    for(i = 0; i < t->num_entries; i++) {

      private_data->bt[i]           = (btnlist_t *) xitk_xmalloc(sizeof(btnlist_t));
      private_data->bt[i]->itemlist = mywidget;
      private_data->bt[i]->sel      = i;

      fwidth = xitk_font_get_string_length(fs, t->entries[i]);

      lb.skin_element_name = NULL;
      lb.button_type       = RADIO_BUTTON;
      lb.align             = ALIGN_CENTER;
      lb.label             = t->entries[i];
      lb.callback          = NULL;
      lb.state_callback    = tabs_select;
      lb.userdata          = (void *) (private_data->bt[i]);
      private_data->tabs[i] = xitk_noskin_labelbutton_create (t->parent_wlist, &lb, xx, y, fwidth + 20, 
        private_data->bheight, "Black", "Black", "Black", fontname);
      xitk_dlist_add_tail (&t->parent_wlist->list, &private_data->tabs[i]->node);
      private_data->tabs[i]->type |= WIDGET_GROUP | WIDGET_GROUP_TABS;
      xx += fwidth + 20;

      xitk_hide_widget(private_data->tabs[i]);
      draw_tab(t->imlibdata, (xitk_get_widget_foreground_skin(private_data->tabs[i])));
      
    }

    /* 
       Add left/rigth arrows 
    */
    {
      xitk_image_t  *wimage;

      b.skin_element_name = NULL;
      b.callback          = tabs_select_prev;
      b.userdata          = (void *)mywidget;
      private_data->left = xitk_noskin_button_create (t->parent_wlist, &b, (private_data->x + width) - 40,
        (y-1) + (private_data->bheight - 20), 20, 20);
      xitk_dlist_add_tail (&t->parent_wlist->list, &private_data->left->node);
      private_data->left->type |= WIDGET_GROUP | WIDGET_GROUP_TABS;
      
      wimage = xitk_get_widget_foreground_skin(private_data->left);
      if(wimage)
	draw_arrow_left(t->imlibdata, wimage);

      xx += 20;
      b.skin_element_name = NULL;
      b.callback          = tabs_select_next;
      b.userdata          = (void *)mywidget;
      private_data->right = xitk_noskin_button_create (t->parent_wlist, &b, (private_data->x + width) - 20,
        (y-1) + (private_data->bheight - 20), 20, 20);
        xitk_dlist_add_tail (&t->parent_wlist->list, &private_data->right->node);
      private_data->right->type |= WIDGET_GROUP | WIDGET_GROUP_TABS;

      wimage = xitk_get_widget_foreground_skin(private_data->right);
      if(wimage)
	draw_arrow_right(t->imlibdata, wimage);

    }

    private_data->old_selected = private_data->selected = 0;
    private_data->offset = 0;
    private_data->old_offset = -2;
    
    xitk_font_unload_font(fs);
  }  

  mywidget->private_data          = private_data;

  mywidget->wl                    = wl;

  mywidget->enable                = 1;
  mywidget->running               = 0;
  mywidget->visible               = 0;

  mywidget->have_focus            = FOCUS_LOST; 
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x = mywidget->y       = 0;
  mywidget->width                 = private_data->width;
  mywidget->height                = private_data->bheight;
  mywidget->type                  = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_TABS;
  mywidget->event                 = notify_event;
  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  xitk_labelbutton_set_state(private_data->tabs[private_data->selected], 1);

  return mywidget;
}
