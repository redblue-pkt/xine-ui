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
#include <string.h>

#include "_xitk.h"
#include "tabs.h"
#include "labelbutton.h"
#include "button.h"

typedef struct _tabs_private_s {
  xitk_widget_t           w;

  xitk_widget_t          *left;
  xitk_widget_t          *right;

  int                     x, y, width, all_width;
  int                     lgap, rgap;

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

static void _notify_destroy (_tabs_private_t *wp) {
  (void)wp;
}

static void _tabs_arrange (_tabs_private_t *wp, int start, int paint) {
  int width;

  if ((start < 0) || (start >= wp->num_entries) || (start == wp->start))
    return;

  if (wp->start < 0)
    wp->start = 0;
  {
    int i;
    for (i = wp->start; i < wp->stop; i++)
      xitk_disable_and_hide_widget (wp->tabs[i]);
  }

  wp->start = start;
  wp->stop = wp->num_entries;
  wp->lgap = 0;
  wp->rgap = 0;
  width = wp->all_width;

  if (width > wp->w.width) {
    int i, width = wp->w.width - 40;
    for (i = wp->start; i < wp->stop; i++) {
      int w = xitk_get_widget_width (wp->tabs[i]);
      width -= w;
      if (width < 0) {
        width += w;
        break;
      }
    }
    wp->stop = i;
    if (wp->start > 0) {
      if (wp->stop < wp->num_entries) { /* middle */
        wp->lgap = width >> 1;
        wp->rgap = width - wp->lgap;
      } else { /* right */
        wp->lgap = width;
      }
    } else { /* left */
      wp->rgap = width;
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

  {
    int i, x = wp->w.x + wp->lgap;
    if (wp->start > 0) {
      int w = xitk_get_widget_width (wp->tabs[wp->start - 1]);
      xitk_set_widget_pos (wp->tabs[wp->start - 1], x - w, wp->y);
    }
    for (i = wp->start; i < wp->stop; i++) {
      int w = xitk_get_widget_width (wp->tabs[i]);
      xitk_set_widget_pos (wp->tabs[i], x, wp->y);
      x += w;
      if (paint)
        xitk_enable_and_show_widget (wp->tabs[i]);
      else
        xitk_enable_widget (wp->tabs[i]);
    }
    if (wp->stop < wp->num_entries)
      xitk_set_widget_pos (wp->tabs[wp->stop], x, wp->y);
  }
}
  
static void _tabs_arrange_item (_tabs_private_t *wp, int item, int paint) {
  if ((item < 0) || (item >= wp->num_entries))
    return;

  if (item < wp->start) {
    _tabs_arrange (wp, item, paint);
  } else if (item >= wp->stop) {
    int width = wp->width - 40, i = wp->num_entries;

    while ((i > 0) && (width >= 0))
      width -= xitk_get_widget_width (wp->tabs[--i]);
    if (width < 0)
      i += 1;
    _tabs_arrange (wp, i, paint);
  }
}

static void _tabs_paint (_tabs_private_t *wp, widget_event_t *event) {
  widget_event_t ne;
  int x1, x2, y1, y2;

  if (wp->lgap > 0) do {
    x1 = _tabs_max (event->x, wp->w.x);
    x2 = _tabs_min (event->x + event->width, wp->w.x + wp->lgap);
    if (x1 >= x2)
      break;
    y1 = _tabs_max (event->y, wp->w.y);
    y2 = _tabs_min (event->y + event->height, wp->w.y + wp->w.height);
    if (y1 >= y2)
      break;
    ne.x = x1;
    ne.y = y1;
    ne.width = x2 - x1;
    ne.height = y2 - y1;
    ne.type = WIDGET_EVENT_PARTIAL_PAINT;
    wp->tabs[wp->start - 1]->visible = 1;
    wp->tabs[wp->start - 1]->event (wp->tabs[wp->start - 1], &ne, NULL);
    wp->tabs[wp->start - 1]->visible = 0;
  } while (0);

  if (wp->rgap > 0) do {
    x1 = _tabs_max (event->x, wp->w.x + wp->w.width - 40 - wp->rgap);
    x2 = _tabs_min (event->x + event->width, wp->w.x + wp->w.width - 40);
    if (x1 >= x2)
      break;
    y1 = _tabs_max (event->y, wp->w.y);
    y2 = _tabs_min (event->y + event->height, wp->w.y + wp->w.height);
    if (y1 >= y2)
      break;
    ne.x = x1;
    ne.y = y1;
    ne.width = x2 - x1;
    ne.height = y2 - y1;
    ne.type = WIDGET_EVENT_PARTIAL_PAINT;
    wp->tabs[wp->stop]->visible = 1;
    wp->tabs[wp->stop]->event (wp->tabs[wp->stop], &ne, NULL);
    wp->tabs[wp->stop]->visible = 0;
  } while (0);
}

/*
 *
 */
static void _tabs_enability (_tabs_private_t *wp) {
  if (wp->w.enable == WIDGET_ENABLE) {
    widget_event_t ne;
    int i;
    xitk_enable_and_show_widget (wp->left);
    xitk_enable_and_show_widget (wp->right);
    for (i = wp->start; i < wp->stop; i++)
      xitk_enable_and_show_widget (wp->tabs[i]);
    ne.x = wp->w.x;
    ne.y = wp->w.y;
    ne.width = wp->w.width;
    ne.height = wp->w.height;
    _tabs_paint (wp, &ne);
  } else {
    int i;
    xitk_disable_widget (wp->left);
    xitk_disable_widget (wp->right);
    for (i = wp->start; i < wp->stop; i++)
      xitk_disable_widget (wp->tabs[i]);
  }
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
      wp->callback (&wp->w, wp->userdata, wp->selected);
  } else {
    xitk_labelbutton_set_state (wp->tabs[wp->selected], 1);
  }
}

/*
 *
 */
static void _tabs_shift_left (xitk_widget_t *w, void *data) {
  _tabs_private_t *wp = (_tabs_private_t *)data;
  widget_event_t ne;

  (void)w;
  if (wp->start <= 0)
    return;
  _tabs_arrange (wp, wp->start - 1, wp->w.visible);
  ne.x = wp->w.x;
  ne.y = wp->w.y;
  ne.width = wp->w.width;
  ne.height = wp->w.height;
  _tabs_paint (wp, &ne);
}

/*
 *
 */
static void _tabs_shift_right (xitk_widget_t *w, void *data) {
  _tabs_private_t *wp = (_tabs_private_t *)data;
  widget_event_t ne;

  (void)w;
  if (wp->stop >= wp->num_entries)
    return;
  _tabs_arrange (wp, wp->start + 1, wp->w.visible);
  ne.x = wp->w.x;
  ne.y = wp->w.y;
  ne.width = wp->w.width;
  ne.height = wp->w.height;
  _tabs_paint (wp, &ne);
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

  XITK_CHECK_CONSTITENCY(t);
  
  if((t->entries == NULL) || (t->num_entries == 0))
    XITK_DIE("%s(): entries should be non NULL.\n", __FUNCTION__);

  wp = (_tabs_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;
  
  wp->entries     = t->entries;
  wp->num_entries = t->num_entries;
  wp->x           = x;
  wp->y           = y;
  wp->width       = width;
  wp->callback    = t->callback;
  wp->userdata    = t->userdata;

  wp->w.running   = 0;
  wp->w.visible   = 0;
  wp->w.x         = x;
  wp->w.y         = y;
  wp->w.width     = wp->width;
  wp->w.type      = WIDGET_GROUP | WIDGET_TYPE_TABS | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event     = notify_event;

  {
    xitk_font_t               *fs;
    int                        fwidth, fheight, i;
    xitk_labelbutton_widget_t  lb;
    xitk_button_widget_t       b;
    int                        xx = x;
      
    fs = xitk_font_load_font(wl->xitk, fontname);

    xitk_font_set_font(fs, wl->gc);
    fheight = xitk_font_get_string_height(fs, " ");
    wp->w.height = wp->bheight = fheight + 18;

    XITK_WIDGET_INIT(&lb);
    XITK_WIDGET_INIT(&b);

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
        xitk_disable_and_hide_widget (wp->tabs[i]);
      }
      xx += fwidth + 20;
    }
    wp->all_width = xx - x;

    /* 
       Add left/rigth arrows 
    */
    if (wp->all_width > wp->w.width) {
      b.skin_element_name = "XITK_NOSKIN_LEFT";
      b.callback          = _tabs_shift_left;
      b.userdata          = (void *)wp;
      if ((wp->left = xitk_noskin_button_create (wl, &b,
        wp->x + width - 40, y - 1 + wp->bheight - 20, 20, 20))) {
        wp->left->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_TABS;
        xitk_dlist_add_tail (&wl->list, &wp->left->node);
      }

      xx += 20;
      b.skin_element_name = "XITK_NOSKIN_RIGHT";
      b.callback          = _tabs_shift_right;
      b.userdata          = (void *)wp;
      if ((wp->right = xitk_noskin_button_create (wl, &b,
        wp->x + width - 20, y - 1 + wp->bheight - 20, 20, 20))) {
        wp->right->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_TABS;
        xitk_dlist_add_tail (&wl->list, &wp->right->node);
      }
    } else {
      wp->left = NULL;
      wp->right = NULL;
    }

    wp->selected = 0;
    wp->start = -2;
    wp->stop = wp->num_entries;
    _tabs_arrange (wp, 0, 0);
    
    xitk_font_unload_font(fs);
  }  

  xitk_labelbutton_set_state (wp->tabs[wp->selected], 1);

  return &wp->w;
}
