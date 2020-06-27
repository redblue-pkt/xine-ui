/* 
 * Copyright (C) 2000-2020 the xine project
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


typedef struct _tabs_private_s {
  xitk_widget_t           w;

  char                   *skin_element_name;
  xitk_widget_t          *widget;
  xitk_image_t           *gap;
  xitk_widget_t          *left;
  xitk_widget_t          *right;

  int                     x, y, width;
  int                     gap_x, gap_y, gap_w, gap_h;

  int                     num_entries;
  char                  **entries;

  xitk_widget_t          *tabs[MAX_TABS];
  struct _tabs_private_s *ref[MAX_TABS];

  int                     selected;
  int                     start, stop;

  int                     bheight;

  xitk_state_callback_t  callback;
  void                   *userdata;

} _tabs_private_t;

static int _tabs_min (int a, int b) {
  int d = b - a;
  return a + (d & (d >> (8 * sizeof (d) - 1)));
}

static int _tabs_max (int a, int b) {
  int d = a - b;
  return a - (d & (d >> (8 * sizeof (d) - 1)));
}

/*
 *
 */
static void _tabs_enability (_tabs_private_t *wp) {
  int i;

  if (wp->w.enable == WIDGET_ENABLE) {
    xitk_enable_and_show_widget (wp->left);
    xitk_enable_and_show_widget (wp->right);
    for (i = 0; i < wp->num_entries; i++)
      xitk_enable_and_show_widget (wp->tabs[i]);
  } else {
    xitk_disable_widget (wp->left);
    xitk_disable_widget (wp->right);
    for (i = 0; i < wp->num_entries; i++)
      xitk_disable_widget (wp->tabs[i]);
  }
}
  
static void _notify_destroy (_tabs_private_t *wp) {
  if (wp->gap)
    xitk_image_free_image (&wp->gap);
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static void _tabs_set_gap (_tabs_private_t *wp, int x, int width) {
  wp->gap_x = x;
  if (width > 0) {
    wp->gap_w = width;
    if (wp->gap) {
      int w;
      /* HACK */
      w = wp->gap->image->width;
      wp->gap->image->width = width * 3;
      draw_tab (wp->gap);
      wp->gap->image->width = w;
    }
  } else {
    wp->gap_w = 0;
  }
  if (wp->start <= 0)
    xitk_stop_widget (wp->left);
  else
    xitk_start_widget (wp->left);
  if (wp->stop < wp->num_entries)
    xitk_start_widget (wp->right);
  else
    xitk_stop_widget (wp->right);
}

static void _tabs_arrange_left (_tabs_private_t *wp, int start, int paint) {
  int i, width, x;

  if ((start < 0) || (start >= wp->num_entries) || (start == wp->start))
    return;

  if (wp->start < 0)
    wp->start = 0;
  for (i = wp->start; i < wp->stop; i++)
    xitk_disable_and_hide_widget (wp->tabs[i]);

  wp->start = start;
  for (width = wp->width - 40, x = wp->x, i = wp->start; i < wp->num_entries; i++) {
    int w = xitk_get_widget_width (wp->tabs[i]);

    if (width < w)
      break;
    xitk_set_widget_pos (wp->tabs[i], x, wp->y);
    if (paint)
      xitk_enable_and_show_widget (wp->tabs[i]);
    else
      xitk_enable_widget (wp->tabs[i]);
    width -= w;
    x += w;
  }
  wp->stop = i;

  _tabs_set_gap (wp, x, width);
}
  
static void _tabs_arrange_item (_tabs_private_t *wp, int item, int paint) {
  if ((item < 0) || (item >= wp->num_entries))
    return;

  if (item < wp->start) {
    _tabs_arrange_left (wp, item, paint);
  } else if (item >= wp->stop) {
    int width = wp->width - 40, i = wp->num_entries;

    while ((i > 0) && (width >= 0))
      width -= xitk_get_widget_width (wp->tabs[--i]);
    if (width < 0)
      i += 1;
    _tabs_arrange_left (wp, i, paint);
  }
}

/*
 *
 */
static void _tabs_paint (_tabs_private_t *wp, widget_event_t *event) {
  do {
    int x1, x2, y1, y2;

    if (wp->w.visible != 1)
      break;

    x1 = _tabs_max (event->x, wp->gap_x);
    x2 = _tabs_min (event->x + event->width, wp->gap_x + wp->gap_w);
    if (x1 >= x2)
      break;

    y1 = _tabs_max (event->y, wp->gap_y);
    y2 = _tabs_min (event->y + event->height, wp->gap_y + wp->gap_h);
    if (y1 >= y2)
      break;

    xitk_image_draw_image (wp->w.wl, wp->gap,
      x1 - wp->gap_x, y1 - wp->gap_y, x2 - x1, y2 - y1, x1, y1);
  } while (0);
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _tabs_private_t *wp = (_tabs_private_t *)w;
  int retval = 0;

  (void)result;
  if (!wp || !event)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_TABS)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = wp->w.x;
      event->y = wp->w.y;
      event->width = wp->w.width;
      event->height = wp->w.height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      _tabs_paint (wp, event);
      break;
    case WIDGET_EVENT_DESTROY:
      _notify_destroy (wp);
      break;
    case WIDGET_EVENT_ENABLE:
      _tabs_enability (wp);
      break;
    default: ;
  }
  return retval;
}

/*
 *
 */
static void tabs_select(xitk_widget_t *w, void *data, int select, int modifier) {
  _tabs_private_t **ref = (_tabs_private_t **)data, *wp = *ref;

  (void)w;
  (void)modifier;
  if (select) {
    xitk_labelbutton_set_state (wp->tabs[wp->selected], 0);
    wp->selected = ref - wp->ref;
    if (wp->callback)
      wp->callback (wp->widget, wp->userdata, wp->selected);
  } else {
    xitk_labelbutton_set_state (wp->tabs[wp->selected], 1);
  }
}

/*
 *
 */
static void _tabs_shift_left (xitk_widget_t *w, void *data) {
  _tabs_private_t *wp = (_tabs_private_t *)data;

  (void)w;
  _tabs_arrange_left (wp, wp->start - 1, wp->w.visible);
}

/*
 *
 */
static void _tabs_shift_right (xitk_widget_t *w, void *data) {
  _tabs_private_t *wp = (_tabs_private_t *)data;

  (void)w;
  _tabs_arrange_left (wp, wp->start + 1, wp->w.visible);
}

/*
 *
 */
void xitk_tabs_set_current_selected(xitk_widget_t *w, int select) {
  _tabs_private_t *wp = (_tabs_private_t *)w;
  
  if (wp && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_TABS)) {
    if ((select >= 0) && (select < wp->num_entries)) {
      xitk_labelbutton_set_state (wp->tabs[wp->selected], 0);
      wp->selected = select;
      _tabs_arrange_item (wp, wp->selected, wp->w.visible);
      xitk_labelbutton_set_state (wp->tabs[wp->selected], 1);
    }
  }
}

/*
 *
 */
int xitk_tabs_get_current_selected(xitk_widget_t *w) {
  _tabs_private_t *wp = (_tabs_private_t *)w;
  
  if (wp && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_TABS))
    return wp->selected;
  return -1;
}

/*
 *
 */
const char *xitk_tabs_get_current_tab_selected(xitk_widget_t *w) {
  _tabs_private_t *wp = (_tabs_private_t *)w;
  
  if (wp && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_TABS))
    return xitk_labelbutton_get_label (wp->tabs[wp->selected]);
  return NULL;
}

/*
 *
 */
xitk_widget_t *xitk_noskin_tabs_create(xitk_widget_list_t *wl,
				       xitk_tabs_widget_t *t, 
                                       int x, int y, int width,
                                       const char *fontname) {
  _tabs_private_t *wp;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_CHECK_CONSTITENCY(t);
  
  if((t->entries == NULL) || (t->num_entries == 0))
    XITK_DIE("%s(): entries should be non NULL.\n", __FUNCTION__);

  wp = (_tabs_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;
  
  wp->widget      = &wp->w;
  wp->entries     = t->entries;
  wp->num_entries = t->num_entries;
  wp->x           = x;
  wp->y           = y;
  wp->width       = width;
  wp->callback    = t->callback;
  wp->userdata    = t->userdata;
    
  wp->skin_element_name = (t->skin_element_name == NULL) ? NULL : strdup (t->skin_element_name);
  
  {
    xitk_font_t               *fs;
    int                        fwidth, fheight, i;
    xitk_labelbutton_widget_t  lb;
    xitk_button_widget_t       b;
    int                        xx = x;
      
    fs = xitk_font_load_font(wl->xitk, fontname);

    xitk_font_set_font(fs, wl->gc);
    fheight = xitk_font_get_string_height(fs, " ");

    XITK_WIDGET_INIT(&lb);
    XITK_WIDGET_INIT(&b);

    wp->bheight = fheight + 18;

    for (i = 0; i < wp->num_entries; i++) {

      wp->ref[i]          = wp;

      fwidth = xitk_font_get_string_length(fs, t->entries[i]);

      lb.skin_element_name = NULL;
      lb.button_type       = TAB_BUTTON;
      lb.align             = ALIGN_CENTER;
      lb.label             = t->entries[i];
      lb.callback          = NULL;
      lb.state_callback    = tabs_select;
      lb.userdata          = (void *)&wp->ref[i];
      if ((wp->tabs[i] = xitk_noskin_labelbutton_create (wl, &lb,
        xx, y, fwidth + 20, wp->bheight, "Black", "Black", "Black", fontname))) {
        wp->tabs[i]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_TABS;
        xitk_dlist_add_tail (&wl->list, &wp->tabs[i]->node);
        xitk_hide_widget (wp->tabs[i]);
      }
      xx += fwidth + 20;
    }

    wp->gap_w = wp->width - 40;
    wp->gap_h = wp->bheight >> 1;
    wp->gap_y = wp->y + wp->gap_h;
    wp->gap = xitk_image_create_image (wl->xitk, wp->gap_w * 3, wp->gap_h);

    /* 
       Add left/rigth arrows 
    */
    {
      xitk_image_t  *wimage;

      b.skin_element_name = NULL;
      b.callback          = _tabs_shift_left;
      b.userdata          = (void *)wp;
      if ((wp->left = xitk_noskin_button_create (wl, &b,
        wp->x + width - 40, y - 1 + wp->bheight - 20, 20, 20))) {
        wp->left->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_TABS;
        xitk_dlist_add_tail (&wl->list, &wp->left->node);
        wimage = xitk_get_widget_foreground_skin (wp->left);
        if (wimage)
          draw_arrow_left (wimage);
      }

      xx += 20;
      b.skin_element_name = NULL;
      b.callback          = _tabs_shift_right;
      b.userdata          = (void *)wp;
      if ((wp->right = xitk_noskin_button_create (wl, &b,
        wp->x + width - 20, y - 1 + wp->bheight - 20, 20, 20))) {
        wp->right->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_TABS;
        xitk_dlist_add_tail (&wl->list, &wp->right->node);
        wimage = xitk_get_widget_foreground_skin (wp->right);
        if (wimage)
        draw_arrow_right (wimage);
      }
    }

    wp->selected = 0;
    wp->start = -2;
    wp->stop = wp->num_entries;
    _tabs_arrange_left (wp, 0, 0);
    
    xitk_font_unload_font(fs);
  }  

  wp->w.private_data          = wp;

  wp->w.wl                    = wl;

  wp->w.enable                = 1;
  wp->w.running               = 0;
  wp->w.visible               = 0;

  wp->w.have_focus            = FOCUS_LOST; 
  wp->w.x                     = x;
  wp->w.y                     = y;
  wp->w.width                 = wp->width;
  wp->w.height                = wp->bheight;
  wp->w.type                  = WIDGET_GROUP | WIDGET_TYPE_TABS | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event                 = notify_event;
  wp->w.tips_timeout          = 0;
  wp->w.tips_string           = NULL;

  xitk_labelbutton_set_state (wp->tabs[wp->selected], 1);

  return &wp->w;
}

