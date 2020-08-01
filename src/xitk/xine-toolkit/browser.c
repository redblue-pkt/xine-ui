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
#include <sys/time.h>
#include <unistd.h>

#include "_xitk.h"
#include "browser.h"
#include "labelbutton.h"
#include "button.h"
#include "slider.h"

#define WBUP    0  /*  Position of button up in item_tree  */
#define WSLID   1  /*  Position of slider in item_tree  */
#define WBDN    2  /*  Position of button down in item_tree  */
#define WBLF    3
#define WSLIDH  4
#define WBRT    5
#define WBSTART 6  /*  Position of first item button in item_tree */

#define MAX_VISIBLE 32

typedef struct _browser_private_s {
  xitk_widget_t           w;
  xitk_skin_config_t     *skonfig;
  char                   *skin_element_name;
  struct {
    const char * const   *names;
    const char * const   *shortcuts;
    int                   width;
    int                   num;
    int                   snum;
    int                   over;
    int                   last_over;
    int                   selected;
  }                       items;
  struct {
    int                   start, num, max;
    int                   width, x0, xmax, dx;
    int                   ymax;
    uint8_t               i2v[MAX_VISIBLE];
    uint8_t               v2i[MAX_VISIBLE];
    char                 *fontname;
    xitk_widget_t        *btns[WBSTART + MAX_VISIBLE];
    struct _browser_private_s *blist[MAX_VISIBLE];
  }                       visible;

  xitk_ext_state_callback_t   callback;
  void                   *userdata;

  int                     dbl_click_time;
  xitk_ext_state_callback_t   dbl_click_callback;

  struct timeval          click_time;
} _browser_private_t;

static void _browser_set_items (_browser_private_t *wp, const char * const *names, const char * const *shortcuts, int num) {
  if (!names || (num <= 0)) {
    wp->items.names = NULL;
    wp->items.width = 0;
    wp->items.num = 0;
    return;
  }

  do {
    xitk_font_t *fs;
    int max_len = 0, max_index = 0, i = 0;

    if (shortcuts) {
      for (; i < num; i++) {
        int len;
        if (!names[i] || !shortcuts[i])
          break;
        len = strlen (names[i]) + strlen (shortcuts[i]);
        if (len > max_len) {
          max_len = len;
          max_index = i;
        }
      }
    }
    wp->items.snum = i;
    for (; i < num; i++) {
      int len;
      if (!names[i])
        break;
      len = strlen (names[i]);
      if (len > max_len) {
        max_len = len;
        max_index = i;
      }
    }
    wp->items.num = i;
    wp->items.names = names;
    wp->items.shortcuts = shortcuts;

    wp->items.width = 0;
    if (!wp->visible.fontname)
      break;
    fs = xitk_font_load_font (wp->w.wl->xitk, wp->visible.fontname);
    if (!fs)
      break;
    xitk_font_set_font (fs, wp->w.wl->gc);
    wp->items.width = xitk_font_get_string_length (fs, wp->items.names[max_index]);
    if (shortcuts && (max_index < wp->items.snum))
      wp->items.width += 10 + xitk_font_get_string_length (fs, wp->items.shortcuts[max_index]);
    /* xitk_font_set_font (NULL, wp->w.wl->gc); */
    xitk_font_unload_font (fs);
  } while (0);
}

static void _browser_set_hslider (_browser_private_t *wp, int reset) {
  int dw;
  wp->visible.width = xitk_get_widget_width (wp->visible.btns[WBSTART]) - 4;
  dw = wp->items.width - wp->visible.width;

  if (dw > 0) {
    int pos, align = xitk_labelbutton_get_alignment (wp->visible.btns[WBSTART]);

    xitk_enable_widget (wp->visible.btns[WSLIDH]);
    xitk_enable_widget (wp->visible.btns[WBLF]);
    xitk_enable_widget (wp->visible.btns[WBRT]);

    wp->visible.xmax = dw;
    wp->visible.x0 = align == ALIGN_CENTER ? (dw >> 1)
                   : align == ALIGN_RIGHT ? dw
                   : 0;

    pos = reset ? wp->visible.x0 : xitk_slider_get_pos (wp->visible.btns[WSLIDH]);
    if (wp->skin_element_name) {
      xitk_slider_set_min (wp->visible.btns[WSLIDH], 0);
      xitk_slider_set_max (wp->visible.btns[WSLIDH], dw);
      if (pos > dw)
        pos = dw;
      else if (pos < 0)
        pos = 0;
      xitk_slider_set_pos (wp->visible.btns[WSLIDH], pos);
    } else {
      xitk_slider_hv_t si;
      si.h.pos = pos;
      si.h.visible = wp->visible.width;
      si.h.step = 10;
      si.h.max = wp->items.width;
      si.v.pos = 0;
      si.v.visible = 0;
      si.v.step = 0;
      si.v.max = 0;
      xitk_slider_hv_sync (wp->visible.btns[WSLIDH], &si, XITK_SLIDER_SYNC_SET_AND_PAINT);
    }
    wp->visible.dx = pos - wp->visible.x0;
  } else {
    wp->visible.xmax = 0;
    wp->visible.x0 = 0;

    xitk_disable_widget (wp->visible.btns[WBLF]);
    xitk_disable_widget (wp->visible.btns[WSLIDH]);
    xitk_disable_widget (wp->visible.btns[WBRT]);

    xitk_slider_set_max (wp->visible.btns[WSLIDH], 1);
    xitk_slider_set_min (wp->visible.btns[WSLIDH], 0);
    xitk_slider_reset (wp->visible.btns[WSLIDH]);
  }
}

static void _browser_set_vslider (_browser_private_t *wp) {
  wp->visible.ymax = wp->items.num - wp->visible.max;

  if (wp->skin_element_name) {
    xitk_slider_set_min (wp->visible.btns[WSLID], 0);
    if (wp->visible.ymax <= 0) {
      wp->visible.ymax = 0;
      xitk_slider_set_max (wp->visible.btns[WSLID], 0);
      xitk_disable_widget (wp->visible.btns[WBUP]);
      xitk_disable_widget (wp->visible.btns[WSLID]);
      xitk_disable_widget (wp->visible.btns[WBDN]);
    } else {
      xitk_slider_set_max (wp->visible.btns[WSLID], wp->visible.ymax);
      xitk_enable_widget (wp->visible.btns[WBUP]);
      xitk_enable_widget (wp->visible.btns[WSLID]);
      xitk_enable_widget (wp->visible.btns[WBDN]);
    }
    if (wp->skin_element_name)
      xitk_slider_set_pos (wp->visible.btns[WSLID], wp->visible.ymax- wp->visible.start);
  } else {
    xitk_slider_hv_t si;
    if (wp->visible.ymax > 0) {
      xitk_enable_widget (wp->visible.btns[WBUP]);
      xitk_enable_widget (wp->visible.btns[WSLID]);
      xitk_enable_widget (wp->visible.btns[WBDN]);
    } else {
      wp->visible.ymax = 0;
      xitk_disable_widget (wp->visible.btns[WBUP]);
      xitk_disable_widget (wp->visible.btns[WSLID]);
      xitk_disable_widget (wp->visible.btns[WBDN]);
    }
    si.h.pos = 0;
    si.h.visible = 0;
    si.h.step = 0;
    si.h.max = 0;
    si.v.pos = wp->visible.start;
    si.v.visible = wp->visible.num;
    si.v.step = 1;
    si.v.max = wp->items.num;
    xitk_slider_hv_sync (wp->visible.btns[WSLID], &si, XITK_SLIDER_SYNC_SET_AND_PAINT);
  }
}

static void _browser_vtab_init (_browser_private_t *wp) {
  int i;
  for (i = 0; i < MAX_VISIBLE; i++)
    wp->visible.i2v[i] = wp->visible.v2i[i] = i;
}

static void _browser_vtab_move (_browser_private_t *wp, int by) {
  int i;
  for (i = 0; i < wp->visible.max; i++) {
    int v = wp->visible.i2v[i];
    v += by;
    if (v < 0)
      v += wp->visible.max;
    else if (v >= wp->visible.max)
      v -= wp->visible.max;
    wp->visible.i2v[i] = v;
    wp->visible.v2i[v] = i;
  }
}

static void _browser_set_btns (_browser_private_t *wp) {
  int n = wp->items.num < wp->visible.max ? wp->items.num : wp->visible.max;
  if (n != wp->visible.num) {
    wp->visible.num = n;
    _browser_vtab_init (wp);
    for (n = 0; n < wp->visible.num; n++)
      xitk_enable_widget (wp->visible.btns[n + WBSTART]);
    for (; n < wp->visible.max; n++) {
      xitk_disable_and_hide_widget (wp->visible.btns[n + WBSTART]);
      xitk_labelbutton_change_label (wp->visible.btns[n + WBSTART], "");
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[n + WBSTART], "", -1, NULL);
      xitk_show_widget (wp->visible.btns[n + WBSTART]);
    }
  }
}

static int _browser_item_2_visible (_browser_private_t *wp, int item) {
  item -= wp->visible.start;
  if ((item < 0) || (item >= wp->visible.num))
    return -1;
  return wp->visible.i2v[item];
}
  
static int _browser_visible_2_item (_browser_private_t *wp, int visible) {
  if ((visible < 0) || (visible >= wp->visible.max))
    return -1;
  return wp->visible.v2i[visible] + wp->visible.start;
}

static void _browser_over (_browser_private_t *wp, int item, int paint) {
  if (item == wp->items.over)
    return;
#if 0
  if (paint) {
    int v = _browser_item_2_visible (wp, wp->items.over);
    if (v >= 0)
      xitk_labelbutton_set_state (wp->visible.btns[v + WBSTART], item == wp->items.selected ? 2 : 0);
    v = _browser_item_2_visible (wp, item);
    if ((v >= 0) && (item != wp->items.selected))
      xitk_labelbutton_set_state (wp->visible.btns[v + WBSTART], 1);
  }
#else
  (void)paint;
#endif
  wp->items.over = item;
}
  
static int _browser_select (_browser_private_t *wp, int item) {
  int v;
  if (item == wp->items.selected)
    return 0;
  v = _browser_item_2_visible (wp, wp->items.selected);
  if (v >= 0)
    xitk_labelbutton_set_state (wp->visible.btns[v + WBSTART], 0);
  v = _browser_item_2_visible (wp, item);
  if (v >= 0)
    xitk_labelbutton_set_state (wp->visible.btns[v + WBSTART], 1);
  wp->items.selected = item;
  return 1;
}

static void _browser_hide_set_pos (_browser_private_t *wp) {
  int h = xitk_get_widget_height (wp->visible.btns[WBSTART]) + (wp->skin_element_name ? 1 : 0);
  int i, y = wp->w.y;
  for (i = 0; i < wp->visible.max; i++) {
    int v = wp->visible.i2v[i];
    xitk_hide_widget (wp->visible.btns[v + WBSTART]);
    xitk_set_widget_pos (wp->visible.btns[v + WBSTART], wp->w.x, y);
    y += h;
  }
}

static void _browser_show (_browser_private_t *wp) {
  int i;
  for (i = 0; i < wp->visible.num; i++) {
    int v = wp->visible.i2v[i];
    if (wp->visible.xmax)
      xitk_labelbutton_set_label_offset (wp->visible.btns[v + WBSTART], -wp->visible.dx);
    xitk_show_widget (wp->visible.btns[v + WBSTART]);
    xitk_labelbutton_set_state (wp->visible.btns[v + WBSTART], i == wp->items.selected - wp->visible.start);
  }
  for (; i < wp->visible.max; i++) {
    int v = wp->visible.i2v[i];
    xitk_show_widget (wp->visible.btns[v + WBSTART]);
  }
}

static void _browser_set_label (_browser_private_t *wp, int start, int num) {
  int i;
  for (i = 0; i < num; i++) {
    int v = wp->visible.i2v[start - wp->visible.start];
    xitk_labelbutton_change_label (wp->visible.btns[v + WBSTART], wp->items.names[start]);
    if (start < wp->items.snum)
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[v + WBSTART],
        wp->items.shortcuts[start], 0, NULL);
    start += 1;
  }
}

static void _browser_move (_browser_private_t *wp, int by) {
  int max, npos, nset, nnum;

  max = wp->items.num - wp->visible.max;
  npos = wp->visible.start + by;
  if (npos > max)
    npos = max;
  if (npos < 0)
    npos = 0;

  by = npos - wp->visible.start;
  wp->visible.start = npos;
  nset = wp->visible.start;
  nnum = wp->visible.num;
  if (by < 0) {
    if (by > -wp->visible.max) {
      nnum = -by;
      _browser_vtab_move (wp, by);
    }
  } else if (by > 0) {
    if (by < wp->visible.max) {
      nset += wp->visible.max - by;
      nnum = by;
      _browser_vtab_move (wp, by);
    }
  }

  _browser_hide_set_pos (wp);
  _browser_set_label (wp, nset, nnum);
  _browser_show (wp);
}

static void _browser_paint (_browser_private_t *wp) {
  if (wp->w.visible == 1) {
    int i;
    for (i = 0; i < wp->visible.max + WBSTART; i++)
      xitk_show_widget (wp->visible.btns[i]);
  } else {
    int i;
    for (i = 0; i < wp->visible.max + WBSTART; i++)
      xitk_hide_widget (wp->visible.btns[i]);
  }
}

static void _browser_focus (_browser_private_t *wp, int type) {
  if (type == FOCUS_LOST) {
    wp->w.have_focus = FOCUS_LOST;
    wp->items.last_over = wp->items.over;
    _browser_over (wp, -1, 1);
  } else if (type == FOCUS_RECEIVED) {
    int item = wp->items.last_over >= 0 ? wp->items.last_over
             : wp->items.selected >= 0 ? wp->items.selected : 0;
    wp->w.have_focus = FOCUS_RECEIVED;
    _browser_over (wp, item, 1);
  }
}

static void _browser_item_btns (_browser_private_t *wp, const xitk_skin_element_info_t *info);

static void _browser_new_skin (_browser_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name) {
    const xitk_skin_element_info_t *info;

    xitk_skin_lock (skonfig);
    info = xitk_skin_get_info (skonfig, wp->skin_element_name);

    wp->skonfig = skonfig;
    wp->w.visible = info ? (info->visibility ? 1 : -1) : 0;
    wp->w.enable  = info ? info->enability : 0;

    free (wp->visible.fontname);
    wp->visible.fontname = info && info->label_fontname ? strdup (info->label_fontname) : NULL;
    _browser_item_btns (wp, info);
    _browser_set_items (wp, wp->items.names, wp->items.shortcuts, wp->items.num);
    _browser_set_hslider (wp, 1);
    _browser_set_btns (wp);
    _browser_move (wp, 0);
    _browser_set_vslider (wp);
    xitk_skin_unlock (skonfig);
  }
}

static void _browser_notify_destroy (_browser_private_t *wp) {
  XITK_FREE (wp->visible.fontname);
  XITK_FREE (wp->skin_element_name);
}

static void _browser_enability (_browser_private_t *wp) {
  if (wp->w.enable == WIDGET_ENABLE) {
    int i;
    if (wp->items.num > wp->visible.max) {
      xitk_enable_widget (wp->visible.btns[WBUP]);
      xitk_enable_widget (wp->visible.btns[WSLID]);
      xitk_enable_widget (wp->visible.btns[WBDN]);
    }
    if (wp->visible.xmax) {
      xitk_enable_widget (wp->visible.btns[WBLF]);
      xitk_enable_widget (wp->visible.btns[WSLIDH]);
      xitk_enable_widget (wp->visible.btns[WBRT]);
    }
    for (i = 0; i < wp->visible.num; i++)
      xitk_enable_widget (wp->visible.btns[i + WBSTART]);
  } else {
    int i;
    for (i = 0; i < wp->visible.max + WBSTART; i++)
      xitk_disable_widget (wp->visible.btns[i]);
  }
}

static int browser_notify_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _browser_private_t *wp = (_browser_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return 0;
  (void)result;
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _browser_paint (wp);
      break;
    case WIDGET_EVENT_FOCUS:
      _browser_focus (wp, event->focus);
      break;
    case WIDGET_EVENT_DESTROY:
      _browser_notify_destroy (wp);
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _browser_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_ENABLE:
      _browser_enability (wp);
      break;
    default: ;
  }
  return 0;
}

/**
 * Return the number of displayed entries
 */
int xitk_browser_get_num_entries(xitk_widget_t *w) {
  _browser_private_t *wp = (_browser_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return 0;
  return wp->visible.max;
}

/**
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w) {
  _browser_private_t *wp = (_browser_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return 0;
  return wp->visible.start;
}

/**
 * Change browser labels alignment
 */
void xitk_browser_set_alignment(xitk_widget_t *w, int align) {
  _browser_private_t *wp = (_browser_private_t *)w;
  int i;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
    
  for (i = 0; i < wp->visible.max; i++)
    xitk_labelbutton_set_alignment (wp->visible.btns[i + WBSTART], align);
  _browser_set_hslider (wp, 1);
  _browser_move (wp, 0);
}

/**
 * Return the current selected button (if not, return -1)
 */
static int _xitk_browser_get_current_selected (_browser_private_t *wp) {
  return wp->items.selected;
}
int xitk_browser_get_current_selected(xitk_widget_t *w) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  if (!wp)
    return -1;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return -1;
  return _xitk_browser_get_current_selected (wp);
}

/**
 * Release all enabled buttons
 */
static void _xitk_browser_release_all_buttons (_browser_private_t *wp) {
  int i;
  
  for (i = 0; i < wp->visible.max; i++)
    xitk_labelbutton_set_state (wp->visible.btns[i + WBSTART], 0);
}

void xitk_browser_release_all_buttons(xitk_widget_t *w) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
  _xitk_browser_release_all_buttons (wp);
}

/**
 * Select the item 'select' in list
 */
void xitk_browser_set_select(xitk_widget_t *w, int item) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  gettimeofday (&wp->click_time, NULL);
  _browser_select (wp, item);
}

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start) {
  _browser_private_t *wp = (_browser_private_t *)w;

  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
  _browser_move (wp, start - wp->visible.start);
  _browser_set_hslider (wp, 0);
  _browser_set_vslider (wp);
}

/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, const char *const *list, const char *const *shortcut, int len, int start) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  _browser_set_items (wp, list, shortcut, len);
  _browser_set_hslider (wp, 1);
  _browser_set_btns (wp);
  _browser_move (wp, start - wp->visible.start);
  _browser_set_vslider (wp);
}

/**
 * Handle slider movments
 */
static void _browser_slidmove (_browser_private_t *wp, int pos) {
  int dy = wp->visible.ymax - pos - wp->visible.start;
  if (dy) {
    if (_xitk_browser_get_current_selected (wp) >= 0)
      _xitk_browser_release_all_buttons (wp);
    _browser_move (wp, dy);
  }
}
static void browser_slidmove(xitk_widget_t *w, void *data, int pos) {
  _browser_private_t *wp = (_browser_private_t *)data;

  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;
  _browser_slidmove (wp, pos);
}

static void _browser_hslidmove (_browser_private_t *wp, int pos) {
  if (wp->visible.xmax) {
    if (pos != wp->visible.dx) {
      int i;
      wp->visible.dx = pos;
      for (i = 0; i < wp->visible.num; i++) {
        xitk_hide_widget (wp->visible.btns[i + WBSTART]);
        xitk_labelbutton_set_label_offset (wp->visible.btns[i + WBSTART], -pos);
        xitk_show_widget (wp->visible.btns[i + WBSTART]);
      }
    }
  } else {
    xitk_slider_reset (wp->visible.btns[WSLIDH]);
  }
}
static void browser_hslidmove(xitk_widget_t *w, void *data, int pos) {
  _browser_private_t *wp = (_browser_private_t *)data;
    
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;
  _browser_hslidmove (wp, pos - wp->visible.x0);
}

/**
 * slide up
 */
static void browser_up(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;
 
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  xitk_slider_make_step (wp->visible.btns[WSLID]);
  _browser_slidmove (wp, xitk_slider_get_pos(wp->visible.btns[WSLID]));
}

/**
 * slide down
 */
static void browser_down(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;

  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  xitk_slider_make_backstep (wp->visible.btns[WSLID]);
  _browser_slidmove (wp, xitk_slider_get_pos(wp->visible.btns[WSLID]));
}

/**
 * slide up (extern).
 */
void xitk_browser_step_up(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)w;

  (void)data;
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  xitk_slider_make_backstep (wp->visible.btns[WSLID]);
  _browser_slidmove (wp, xitk_slider_get_pos(wp->visible.btns[WSLID]));
}

void xitk_browser_page_up(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;
 
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  _browser_move (wp, -wp->visible.max);
  _browser_set_vslider (wp);
}

void xitk_browser_page_down(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;
 
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  _browser_move (wp, -wp->visible.max);
  _browser_set_vslider (wp);
}

/**
 * slide Down (extern).
 */
void xitk_browser_step_down(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)w;

  (void)data;
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
    
  xitk_slider_make_step (wp->visible.btns[WSLID]);
  _browser_slidmove (wp, xitk_slider_get_pos(wp->visible.btns[WSLID]));
}


/**
 * slide left (extern).
 */
void xitk_browser_step_left(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  (void)data;
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
    
  xitk_slider_make_backstep (wp->visible.btns[WSLIDH]);
  _browser_hslidmove (wp, xitk_slider_get_pos (wp->visible.btns[WSLIDH]) - wp->visible.x0);
}

/**
 * slide right (extern).
 */
void xitk_browser_step_right(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)w;
  
  (void)data;
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
    
  xitk_slider_make_step (wp->visible.btns[WSLIDH]);
  _browser_hslidmove (wp, xitk_slider_get_pos (wp->visible.btns[WSLIDH]) - wp->visible.x0);
}

static void browser_left(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;
 
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;
  
  xitk_slider_make_backstep (wp->visible.btns[WSLIDH]);
  _browser_hslidmove (wp, xitk_slider_get_pos (wp->visible.btns[WSLIDH]) - wp->visible.x0);
}

static void browser_right(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;
 
  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;
  
  xitk_slider_make_step (wp->visible.btns[WSLIDH]);
  _browser_hslidmove (wp, xitk_slider_get_pos (wp->visible.btns[WSLIDH]) - wp->visible.x0);
}

/**
 * Handle list selections
 */
static void browser_select(xitk_widget_t *w, void *data, int state, int modifier) {
  _browser_private_t **entry = (_browser_private_t **)data, *wp;
  int num;

  if (!w || !entry)
    return;
  if ((w->type & (WIDGET_GROUP_BROWSER | WIDGET_TYPE_MASK)) != (WIDGET_GROUP_BROWSER | WIDGET_TYPE_LABELBUTTON))
    return;

  wp = *entry;
  num = entry - wp->visible.blist;
  num = _browser_visible_2_item (wp, num);
  if (num < 0)
    return;
  {
    int done = 0;
    struct timeval tv;
    gettimeofday (&tv, NULL);
    if (num == wp->items.selected) {
      int difftime = (tv.tv_sec - wp->click_time.tv_sec) * 1000;
      difftime += (tv.tv_usec - wp->click_time.tv_usec) / 1000;
      if (difftime < wp->dbl_click_time) {
        if (wp->dbl_click_callback) {
          wp->dbl_click_callback (&wp->w, wp->userdata, num, modifier);
          done = 1;
        }
      }
    }
    _browser_select (wp, state ? num : -1);
    if (!done && state && wp->callback)
      wp->callback (&wp->w, wp->userdata, num, modifier);
    wp->click_time = tv;
  }
}

/*
 *
 */
xitk_widget_t *xitk_browser_get_browser (xitk_widget_t *w) {
  if (!w)
    return NULL;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return NULL;
  return w->parent;
}

static int _browser_get_focus (_browser_private_t *wp) {
  int i;
  for (i = 0; i < wp->visible.num; i++) {
    if (wp->visible.btns[i + WBSTART] == wp->w.wl->widget_focused)
      return i;
  }
  return -1;
}

static void _browser_set_focus (_browser_private_t *wp, int visible) {
  if (wp->w.wl->widget_focused != wp->visible.btns[visible + WBSTART]) {
    widget_event_t event;
    if (wp->w.wl->widget_focused) {
      if ((wp->w.wl->widget_focused->type & WIDGET_FOCUSABLE) &&
        (wp->w.wl->widget_focused->enable == WIDGET_ENABLE)) {
        event.type = WIDGET_EVENT_FOCUS;
        event.focus = FOCUS_LOST;
        wp->w.wl->widget_focused->event (wp->w.wl->widget_focused, &event, NULL);
        wp->w.wl->widget_focused->have_focus = FOCUS_LOST;
      }
      event.type = WIDGET_EVENT_PAINT;
      wp->w.wl->widget_focused->event (wp->w.wl->widget_focused, &event, NULL);
    }
    wp->w.wl->widget_focused = wp->visible.btns[visible + WBSTART];
    if (wp->w.wl->widget_focused) {
      if ((wp->w.wl->widget_focused->type & WIDGET_FOCUSABLE) &&
        (wp->w.wl->widget_focused->enable == WIDGET_ENABLE)) {
        event.type = WIDGET_EVENT_FOCUS;
        event.focus = FOCUS_RECEIVED;
        wp->w.wl->widget_focused->event (wp->w.wl->widget_focused, &event, NULL);
        wp->w.wl->widget_focused->have_focus = FOCUS_RECEIVED;
      }
      event.type = WIDGET_EVENT_PAINT;
      wp->w.wl->widget_focused->event (wp->w.wl->widget_focused, &event, NULL);
    }
  }
}

/*
 * Jump to entry in list which match with the alphanum char key.
 */
void xitk_browser_warp_jump(xitk_widget_t *w, const char *key, int modifier) {
  _browser_private_t *wp = (_browser_private_t *)w;
  int i, v;

  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  v = _browser_visible_2_item (wp, _browser_get_focus (wp));
  if (v < 0)
    v = wp->visible.start;
  
  if ((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
    size_t klen = strlen (key);
    for (i = v + 1; i < wp->items.num; i++) {
      if (!strncasecmp (wp->items.names[i], key, klen))
        break;
    }
    if (i >= wp->items.num) {
      for (i = 0; i < v; i++) {
        if (!strncasecmp (wp->items.names[i], key, klen))
          break;
      }
      if (i >= v)
        return;
    }
  } else if (modifier & MODIFIER_SHIFT) {
    size_t klen = strlen (key);
    for (i = v - 1; i >= 0; i--) {
      if (!strncasecmp (wp->items.names[i], key, klen))
        break;
    }
    if (i < 0) {
      for (i = wp->items.num - 1; i > v; i--) {
        if (!strncasecmp (wp->items.names[i], key, klen))
          break;
      }
      if (i <= v)
        return;
    }
  } else {
    return;
  }
  _browser_move (wp, i - (wp->visible.max >> 1) - wp->visible.start);
  _browser_set_vslider (wp);
  v = _browser_item_2_visible (wp, i);
  _browser_set_focus (wp, v);
}

/**
 * Create the list browser
 */
static xitk_widget_t *_xitk_browser_create (_browser_private_t *wp, xitk_browser_widget_t *br) {
  wp->dbl_click_time = xitk_get_timer_dbl_click ();
  gettimeofday (&wp->click_time, NULL);

  wp->dbl_click_callback = br->dbl_click_callback;

  wp->callback = br->callback;
  wp->userdata = br->userdata;

  wp->visible.xmax = 0;
  wp->visible.dx = 0;

  wp->items.over = -1;
  wp->items.last_over = -1;
  wp->items.selected = -1;

  _browser_vtab_init (wp);
  _browser_set_hslider (wp, 1);
  _browser_set_vslider (wp);

  wp->w.running = 1;
  wp->w.have_focus = FOCUS_LOST;

  wp->w.type = WIDGET_FOCUSABLE /* | WIDGET_KEYABLE */ | WIDGET_GROUP | WIDGET_TYPE_BROWSER;
  wp->w.event = browser_notify_event;
  wp->w.tips_timeout = 0;
  wp->w.tips_string = NULL;
  _browser_show (wp);

  return &wp->w;
}

static void _browser_item_btns (_browser_private_t *wp, const xitk_skin_element_info_t *info) {
  int i, x, y, n;
  int keep, drop;
  xitk_dnode_t *prev;
  xitk_labelbutton_widget_t lb;

  n = info ? info->browser_entries : 0;
  if (n > MAX_VISIBLE)
    n = MAX_VISIBLE;
  keep = n < wp->visible.max ? n : wp->visible.max;
  drop = wp->visible.max;

  _browser_vtab_init (wp);
  wp->visible.start = 0;
  wp->visible.num = n < wp->items.num ? n : wp->items.num;
  wp->visible.max = n;

  x = info ? info->x : 0;
  y = info ? info->y : 0;
  wp->w.x = x;
  wp->w.y = y;
  prev = wp->w.wl->list.tail.prev;
  XITK_WIDGET_INIT (&lb);


  for (i = 0; i < keep; i++) {
    xitk_set_widget_pos (wp->visible.btns[i + WBSTART], x, y);
    if (i >= wp->visible.num)
      xitk_disable_widget (wp->visible.btns[i + WBSTART]);
    else
      xitk_enable_widget (wp->visible.btns[i + WBSTART]);
    xitk_labelbutton_change_label (wp->visible.btns[i + WBSTART], i < wp->items.num ? wp->items.names[i] : "");
    if (wp->items.shortcuts)
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[i + WBSTART],
        i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
    y += xitk_get_widget_height (wp->visible.btns[i + WBSTART]) + 1;
    prev = &wp->visible.btns[i + WBSTART]->node;
  }

  for (; i < n; i++) {
    wp->visible.blist[i] = wp;
    lb.button_type       = RADIO_BUTTON;
    lb.align             = ALIGN_DEFAULT;
    lb.label             = i < wp->items.num ? wp->items.names[i] : "";
    lb.callback          = NULL;
    lb.state_callback    = browser_select;
    lb.userdata          = wp->visible.blist + i;
    lb.skin_element_name = wp->skin_element_name;
    wp->visible.btns[i + WBSTART] = xitk_labelbutton_create (wp->w.wl, wp->skonfig, &lb);
    if (!wp->visible.btns[i + WBSTART])
      break;
    xitk_dnode_insert_after (prev, &wp->visible.btns[i + WBSTART]->node);
    wp->visible.btns[i + WBSTART]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[i + WBSTART]->parent = &wp->w;
    if (i >= wp->visible.num)
      xitk_disable_widget (wp->visible.btns[i + WBSTART]);
    xitk_set_widget_pos (wp->visible.btns[i + WBSTART], x, y);
    y += xitk_get_widget_height (wp->visible.btns[i + WBSTART]) + 1;
    if (wp->items.shortcuts)
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[i + WBSTART],
        i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
    prev = &wp->visible.btns[i + WBSTART]->node;
  }
  n = i;

  for (; i < drop; i++) {
    xitk_destroy_widget (wp->visible.btns[i + WBSTART]);
    wp->visible.btns[i + WBSTART] = NULL;
  }
}

/**
 * Create the list browser
 */
xitk_widget_t *xitk_browser_create(xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_browser_widget_t *br) {
  _browser_private_t *wp;
  xitk_button_widget_t           b;
  xitk_slider_widget_t           sl;
  const xitk_skin_element_info_t *info;

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(br);

  wp = calloc (1, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&b);
  XITK_WIDGET_INIT(&sl);

  wp->w.width = 0;
  wp->w.height = 0;
  wp->w.wl = wl;

  wp->skin_element_name = br->browser.skin_element_name ? strdup (br->browser.skin_element_name) : NULL;
  info = xitk_skin_get_info (skonfig, wp->skin_element_name);
  wp->visible.fontname = info && info->label_fontname ? strdup (info->label_fontname) : NULL;
  _browser_set_items (wp, br->browser.entries, NULL, br->browser.num_entries);
  wp->skonfig = skonfig;
  wp->visible.max = 0;
  _browser_item_btns (wp, info);
  
  b.skin_element_name = br->arrow_up.skin_element_name;
  b.callback          = browser_up;
  b.userdata          = wp;
  wp->visible.btns[WBUP] = xitk_button_create (wp->w.wl, skonfig, &b);
  if (wp->visible.btns[WBUP]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBUP]->node);
    wp->visible.btns[WBUP]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBUP]->parent = &wp->w;
  }
  
  sl.min                      = 0;
  sl.max                      = (br->browser.num_entries > (br->browser.max_displayed_entries-1) ? br->browser.num_entries-1 : 0);
  sl.step                     = 1;
  sl.skin_element_name        = br->slider.skin_element_name;
  sl.callback                 = browser_slidmove;
  sl.userdata                 = wp;
  sl.motion_callback          = browser_slidmove;
  sl.motion_userdata          = wp;
  wp->visible.btns[WSLID] = xitk_slider_create (wl, skonfig, &sl);
  if (wp->visible.btns[WSLID]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WSLID]->node);
    wp->visible.btns[WSLID]->type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WSLID]->parent = &wp->w;
    xitk_slider_set_to_max (wp->visible.btns[WSLID]);
  }

  b.skin_element_name = br->arrow_dn.skin_element_name;
  b.callback          = browser_down;
  b.userdata          = wp;
  wp->visible.btns[WBDN] = xitk_button_create (wl, skonfig, &b);
  if (wp->visible.btns[WBDN]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBDN]->node);
    wp->visible.btns[WBDN]->type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBDN]->parent = &wp->w;
  }
  
  b.skin_element_name = br->arrow_left.skin_element_name;
  b.callback          = browser_left;
  b.userdata          = wp;
  wp->visible.btns[WBLF] = xitk_button_create (wl, skonfig, &b);
  if (wp->visible.btns[WBLF]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBLF]->node);
    wp->visible.btns[WBLF]->type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBLF]->parent = &wp->w;
  }
  
  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = br->slider_h.skin_element_name;
  sl.callback                 = browser_hslidmove;
  sl.userdata                 = wp;
  sl.motion_callback          = browser_hslidmove;
  sl.motion_userdata          = wp;
  wp->visible.btns[WSLIDH] = xitk_slider_create (wl, skonfig, &sl);
  if (wp->visible.btns[WSLIDH]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WSLIDH]->node);
    wp->visible.btns[WSLIDH]->type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WSLIDH]->parent = &wp->w;
    xitk_slider_reset (wp->visible.btns[WSLIDH]);
  }

  b.skin_element_name = br->arrow_right.skin_element_name;
  b.callback          = browser_right;
  b.userdata          = wp;
  wp->visible.btns[WBRT] = xitk_button_create (wl, skonfig, &b);
  if (wp->visible.btns[WBRT]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBRT]->node);
    wp->visible.btns[WBRT]->type |= WIDGET_GROUP | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBRT]->parent = &wp->w;
  }

  wp->w.visible = info ? (info->visibility ? 1 : -1) : 0;
  wp->w.enable = info ? info->enability : 0;
  return _xitk_browser_create (wp, br);
}


/*
 *
 */
xitk_widget_t *xitk_noskin_browser_create (xitk_widget_list_t *wl,
  xitk_browser_widget_t *br, int x, int y, int itemw, int itemh, int slidw, const char *fontname) {
  _browser_private_t *wp;
  xitk_button_widget_t        b;
  xitk_labelbutton_widget_t   lb;
  xitk_slider_widget_t        sl;
  int                         btns = (itemh >> 1) > slidw ? (itemh >> 1) : slidw;

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(br);

  wp = calloc (1, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&b);
  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&sl);

  wp->w.x = x;
  wp->w.y = y;
  wp->w.wl = wl;
  wp->skin_element_name = NULL;
  wp->visible.fontname = fontname ? strdup (fontname) : NULL;
  _browser_set_items (wp, br->browser.entries, NULL, br->browser.num_entries);
  wp->visible.max = br->browser.max_displayed_entries <= MAX_VISIBLE ? br->browser.max_displayed_entries : MAX_VISIBLE;
  wp->visible.start = 0;
  wp->visible.num = wp->visible.max < wp->items.num ? wp->visible.max : wp->items.num;
  wp->w.width = itemw + slidw;
  wp->w.height = itemh * wp->visible.max + slidw;

  {
    int ix = x, iy = y, i;
    for (i = 0; i < wp->visible.max; i++) {
      xitk_image_t  *wimage;
      wp->visible.blist[i] = wp;
      lb.button_type       = RADIO_BUTTON;
      lb.label             = i < wp->items.num ? wp->items.names[i] : "";
      lb.align             = ALIGN_LEFT;
      lb.callback          = NULL;
      lb.state_callback    = browser_select;
      lb.userdata          = wp->visible.blist + i;
      lb.skin_element_name = NULL;
      wp->visible.btns[i + WBSTART] = xitk_noskin_labelbutton_create (wl, &lb,
        ix, iy, itemw, itemh, "Black", "Black", "White", fontname);
      if (!wp->visible.btns[i + WBSTART])
        break;
      xitk_dlist_add_tail (&wl->list, &wp->visible.btns[i + WBSTART]->node);
      wp->visible.btns[i + WBSTART]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
      wp->visible.btns[i + WBSTART]->parent = &wp->w;
      if (i >= wp->visible.num)
        xitk_disable_widget (wp->visible.btns[i + WBSTART]);
      wimage = xitk_get_widget_foreground_skin (wp->visible.btns[i + WBSTART]);
      if (wimage)
        draw_flat_three_state (wimage);
      xitk_set_widget_pos (wp->visible.btns[i + WBSTART], ix, iy);
      if (wp->items.shortcuts)
        xitk_labelbutton_change_shortcut_label (wp->visible.btns[i + WBSTART],
          i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
      iy += itemh;
    }
    wp->visible.max = i;
  }

  b.skin_element_name = "XITK_NOSKIN_UP";
  b.callback          = browser_up;
  b.userdata          = wp;
  wp->visible.btns[WBUP] = xitk_noskin_button_create (wl, &b, x + itemw, y + 0, slidw, btns);
  if (wp->visible.btns[WBUP]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBUP]->node);
    wp->visible.btns[WBUP]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBUP]->parent = &wp->w;
  }

  sl.min                  = 0;
  sl.max                  = br->browser.num_entries > br->browser.max_displayed_entries - 1 ? br->browser.num_entries - 1 : 0;
  sl.step                 = 1;
  sl.skin_element_name    = NULL;
  sl.callback             = browser_slidmove;
  sl.userdata             = wp;
  sl.motion_callback      = browser_slidmove;
  sl.motion_userdata      = wp;
  wp->visible.btns[WSLID] = xitk_noskin_slider_create (wl, &sl,
    x + itemw, y + btns, slidw, itemh * wp->visible.max - btns * 2, XITK_HVSLIDER);
  if (wp->visible.btns[WSLID]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WSLID]->node);
    wp->visible.btns[WSLID]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WSLID]->parent = &wp->w;
    xitk_slider_set_to_max (wp->visible.btns[WSLID]);
  }
  
  b.skin_element_name = "XITK_NOSKIN_DOWN";
  b.callback          = browser_down;
  b.userdata          = wp;
  wp->visible.btns[WBDN] = xitk_noskin_button_create (wl, &b,
    x + itemw, y + itemh * wp->visible.max - btns, slidw, btns);
  if (wp->visible.btns[WBDN]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBDN]->node);
    wp->visible.btns[WBDN]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBDN]->parent = &wp->w;
  }

  b.skin_element_name = "XITK_NOSKIN_LEFT";
  b.callback          = browser_left;
  b.userdata          = wp;
  wp->visible.btns[WBLF] = xitk_noskin_button_create (wl, &b,
    x, y + itemh * wp->visible.max, btns, slidw);
  if (wp->visible.btns[WBLF]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBLF]->node);
    wp->visible.btns[WBLF]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBLF]->parent = &wp->w;
  }

  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = browser_hslidmove;
  sl.userdata                 = wp;
  sl.motion_callback          = browser_hslidmove;
  sl.motion_userdata          = wp;
  wp->visible.btns[WSLIDH] = xitk_noskin_slider_create (wl, &sl,
    x + btns, y + itemh * wp->visible.max, itemw - btns * 2, slidw, XITK_HVSLIDER);
  if (wp->visible.btns[WSLIDH]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WSLIDH]->node);
    wp->visible.btns[WSLIDH]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WSLIDH]->parent = &wp->w;
    xitk_slider_reset (wp->visible.btns[WSLIDH]);
  }
  
  b.skin_element_name = "XITK_NOSKIN_RIGHT";
  b.callback          = browser_right;
  b.userdata          = wp;
  wp->visible.btns[WBRT] = xitk_noskin_button_create (wl, &b,
    x + itemw - btns, y + itemh * wp->visible.max, btns, slidw);
  if (wp->visible.btns[WBRT]) {
    xitk_dlist_add_tail (&wl->list, &wp->visible.btns[WBRT]->node);
    wp->visible.btns[WBRT]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
    wp->visible.btns[WBRT]->parent = &wp->w;
  }

  wp->w.visible = 0;
  wp->w.enable = 0;
  wp->skin_element_name = NULL;
  wp->skonfig = NULL;
  return _xitk_browser_create (wp, br);
}

