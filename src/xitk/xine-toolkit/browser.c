/*
 * Copyright (C) 2000-2021 the xine project
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

#define MAX_VISIBLE 32

typedef enum {
  /* keep order */
  _W_up = 0,/* Position of button up in item_tree */
  _W_vert,  /* Position of slider in item_tree */
  _W_down,  /* Position of button down in item_tree */
  /* /keep order */
  /* keep order */
  _W_left,
  _W_hor,
  _W_right,
  /* keep order */
  _W_items, /* Position of first item button in item_tree */
  _W_LAST = _W_items + MAX_VISIBLE 
} _W_t;

typedef struct _browser_private_s {
  xitk_widget_t           w;
  xitk_skin_config_t     *skonfig;
  xitk_short_string_t     skin_element_name;
  struct {
    const char * const   *names;
    const char * const   *shortcuts;
    int                   width;
    int                   num;
    int                   snum;
    int                   last_over;
    int                   selected;
  }                       items;
  struct {
    int                   x, y;
    int                   start, num, max;
    int                   width, x0, xmax, dx;
    int                   ymax;
    uint8_t               i2v[MAX_VISIBLE];
    uint8_t               v2i[MAX_VISIBLE];
    xitk_short_string_t   fontname;
    xitk_widget_t        *btns[_W_LAST];
    struct _browser_private_s *blist[MAX_VISIBLE];
  }                       visible;

  int                     slider_width, with_hslider, with_vslider;

  xitk_ext_state_callback_t   callback;
  void                   *userdata;

  xitk_ext_state_callback_t   dbl_click_callback;

  struct timeval          click_time;
} _browser_private_t;

static void _browser_set_items (_browser_private_t *wp, const char * const *names, const char * const *shortcuts, int num) {

  wp->items.last_over = -1;

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
    if (!wp->visible.fontname.s[0])
      break;
    fs = xitk_font_load_font (wp->w.wl->xitk, wp->visible.fontname.s);
    if (!fs)
      break;
    wp->items.width = xitk_font_get_string_length (fs, wp->items.names[max_index]);
    if (shortcuts && (max_index < wp->items.snum))
      wp->items.width += 10 + xitk_font_get_string_length (fs, wp->items.shortcuts[max_index]);
    xitk_font_unload_font (fs);
  } while (0);
}

static void _browser_set_hslider (_browser_private_t *wp, int reset) {
  int dw;

  wp->visible.width = xitk_get_widget_width (wp->visible.btns[_W_items]) - 4;

  if (!wp->with_hslider) {
    wp->visible.xmax = 0;
    wp->visible.x0 = 0;
    return;
  }

  dw = wp->items.width - wp->visible.width;

  if (dw > 0) {
    int pos, align = xitk_labelbutton_get_alignment (wp->visible.btns[_W_items]);

    xitk_widgets_state (wp->visible.btns + _W_left, 3, XITK_WIDGET_STATE_ENABLE, ~0u);

    wp->visible.xmax = dw;
    wp->visible.x0 = align == ALIGN_CENTER ? (dw >> 1)
                   : align == ALIGN_RIGHT ? dw
                   : 0;

    pos = reset ? wp->visible.x0 : xitk_slider_get_pos (wp->visible.btns[_W_hor]);
    if (wp->skin_element_name.s) {
      xitk_slider_set_min (wp->visible.btns[_W_hor], 0);
      xitk_slider_set_max (wp->visible.btns[_W_hor], dw);
      if (pos > dw)
        pos = dw;
      else if (pos < 0)
        pos = 0;
      xitk_slider_set_pos (wp->visible.btns[_W_hor], pos);
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
      xitk_slider_hv_sync (wp->visible.btns[_W_hor], &si, XITK_SLIDER_SYNC_SET_AND_PAINT);
    }
    wp->visible.dx = pos - wp->visible.x0;
  } else {
    wp->visible.xmax = 0;
    wp->visible.x0 = 0;

    xitk_widgets_state (wp->visible.btns + _W_left, 3, XITK_WIDGET_STATE_ENABLE, 0);

    xitk_slider_set_max (wp->visible.btns[_W_hor], 1);
    xitk_slider_set_min (wp->visible.btns[_W_hor], 0);
    xitk_slider_reset (wp->visible.btns[_W_hor]);
  }
}

static void _browser_set_vslider (_browser_private_t *wp) {
  wp->visible.ymax = wp->items.num - wp->visible.max;
  unsigned int able;

  if (!wp->with_vslider) {
    if (wp->visible.ymax < 0)
      wp->visible.ymax = 0;
    return;
  }

  if (wp->visible.ymax <= 0) {
    wp->visible.ymax = 0;
    able = 0;
  } else {
    able = ~0u;
  }
  if (wp->skin_element_name.s) {
    xitk_slider_set_min (wp->visible.btns[_W_vert], 0);
    xitk_slider_set_max (wp->visible.btns[_W_vert], wp->visible.ymax);
    xitk_widgets_state (wp->visible.btns + _W_up, 3, XITK_WIDGET_STATE_ENABLE, able);
    xitk_slider_set_pos (wp->visible.btns[_W_vert], wp->visible.ymax - wp->visible.start);
  } else {
    xitk_slider_hv_t si;
    xitk_widgets_state (wp->visible.btns + _W_up, 3, XITK_WIDGET_STATE_ENABLE, able);
    si.h.pos = 0;
    si.h.visible = 0;
    si.h.step = 0;
    si.h.max = 0;
    si.v.pos = wp->visible.start;
    si.v.visible = wp->visible.num;
    si.v.step = 1;
    si.v.max = wp->items.num;
    xitk_slider_hv_sync (wp->visible.btns[_W_vert], &si, XITK_SLIDER_SYNC_SET_AND_PAINT);
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
    xitk_widgets_state (wp->visible.btns + _W_items, wp->visible.num, XITK_WIDGET_STATE_ENABLE, ~0u);
    xitk_widgets_state (wp->visible.btns + _W_items + wp->visible.num, wp->visible.max - wp->visible.num,
      XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE, 0);
    for (n = wp->visible.num; n < wp->visible.max; n++) {
      xitk_labelbutton_change_label (wp->visible.btns[n + _W_items], "");
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[n + _W_items], "", -1, NULL);
      xitk_labelbutton_set_state (wp->visible.btns[n + _W_items], 0);
    }
    xitk_widgets_state (wp->visible.btns + _W_items + wp->visible.num, wp->visible.max - wp->visible.num,
      XITK_WIDGET_STATE_VISIBLE, ~0u);
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

static int _browser_select (_browser_private_t *wp, int item) {
  int v;
  if (item == wp->items.selected)
    return 0;
  v = _browser_item_2_visible (wp, wp->items.selected);
  if (v >= 0)
    xitk_labelbutton_set_state (wp->visible.btns[v + _W_items], 0);
  v = _browser_item_2_visible (wp, item);
  if (v >= 0)
    xitk_labelbutton_set_state (wp->visible.btns[v + _W_items], 1);
  wp->items.selected = item;
  return 1;
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
    struct timeval tv;
    gettimeofday (&tv, NULL);
    if (num == wp->items.selected) {
      if (xitk_is_dbl_click (wp->w.wl->xitk, &wp->click_time, &tv)) {
        if (wp->dbl_click_callback) {
          wp->click_time = tv;
          _browser_select (wp, state ? num : -1);
          wp->dbl_click_callback (&wp->w, wp->userdata, num, modifier);
          return;
        }
      }
    }
    wp->click_time = tv;
    _browser_select (wp, state ? num : -1);
    if (wp->callback)
      wp->callback (&wp->w, wp->userdata, state ? num : -num - 1, modifier);
  }
}

static void _browser_hide_set_pos (_browser_private_t *wp) {
  int h = xitk_get_widget_height (wp->visible.btns[_W_items]) + (wp->skin_element_name.s ? 1 : 0);
  int i, y = wp->visible.y;
  for (i = 0; i < wp->visible.max; i++) {
    int v = wp->visible.i2v[i];
    xitk_widgets_state (wp->visible.btns + _W_items + v, 1, XITK_WIDGET_STATE_VISIBLE, 0);
    xitk_set_widget_pos (wp->visible.btns[v + _W_items], wp->visible.x, y);
    y += h;
  }
}

static void _browser_show (_browser_private_t *wp) {
  uint32_t state[MAX_VISIBLE], t;
  int i;

  t = wp->w.state & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  for (i = 0; i < wp->visible.num; i++) {
    int v = wp->visible.i2v[i];
    if (wp->visible.xmax)
      xitk_labelbutton_set_label_offset (wp->visible.btns[v + _W_items], -wp->visible.dx);
    state[v] = t;
  }
  t &= ~XITK_WIDGET_STATE_ENABLE;
  for (; i < wp->visible.max; i++) {
    int v = wp->visible.i2v[i];
    state[v] = t;
  }
  if ((wp->items.selected >= wp->visible.start) && (wp->items.selected < wp->visible.start + wp->visible.num))
    state[wp->visible.i2v[wp->items.selected - wp->visible.start]] |= XITK_WIDGET_STATE_ON;
  if ((wp->items.last_over >= wp->visible.start) && (wp->items.last_over < wp->visible.start + wp->visible.num))
    state[wp->visible.i2v[wp->items.last_over - wp->visible.start]] |= wp->w.state & XITK_WIDGET_STATE_FOCUS;

  for (i = 0; i < wp->visible.max; i++)
    xitk_widgets_state (wp->visible.btns + _W_items + i, 1,
      XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE | XITK_WIDGET_STATE_ON | XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS,
      state[i]);
}

static void _browser_set_label (_browser_private_t *wp, int start, int num) {
  int i;
  for (i = 0; i < num; i++) {
    int v = wp->visible.i2v[start - wp->visible.start];
    xitk_labelbutton_change_label (wp->visible.btns[v + _W_items], wp->items.names[start]);
    if (start < wp->items.snum)
      xitk_labelbutton_change_shortcut_label (wp->visible.btns[v + _W_items],
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

static int _browser_get_focus (_browser_private_t *wp) {
  int i;
  for (i = 0; i < wp->visible.num; i++) {
    if (wp->visible.btns[_W_items + i] && (wp->visible.btns[_W_items + i]->state & XITK_WIDGET_STATE_FOCUS))
      return i;
  }
  return -1;
}

static int _browser_get_current_item (_browser_private_t *wp, int *have_focus) {
  int i = _browser_get_focus (wp);
  if (i < 0) {
    *have_focus = 0;
    return wp->visible.start + (wp->visible.num >> 1);
  } else {
    *have_focus = 1;
    return _browser_visible_2_item (wp, i);
  }
}

static void _browser_paint (_browser_private_t *wp) {
  unsigned int d = wp->w.state ^ wp->w.shown_state, t;

  if (!d)
    return;

  if (d & XITK_WIDGET_STATE_FOCUS) {
    if (wp->w.state & XITK_WIDGET_STATE_FOCUS) {
      int i = wp->items.last_over >= 0 ? wp->items.last_over
            : wp->items.selected >= 0 ? wp->items.selected : 0;

      if ((i < wp->visible.start) || (i >= wp->visible.start + wp->visible.num))
        i = wp->visible.start + (wp->visible.num >> 1);
      wp->items.last_over = i;
    } else {
      wp->items.last_over = _browser_visible_2_item (wp, _browser_get_focus (wp));
    }
  }

  t = wp->w.state & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_widgets_state (wp->visible.btns, _W_items,
    XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE | XITK_WIDGET_STATE_FOCUS, t);
  _browser_show (wp);
  wp->w.shown_state = wp->w.state;
}

static void _browser_item_btns (_browser_private_t *wp, const xitk_skin_element_info_t *info) {
  int x, y, h, n, keep;
  xitk_dnode_t *prev;

  n = info ? info->browser_entries : 0;
  if (n > MAX_VISIBLE)
    n = MAX_VISIBLE;

  keep = n < wp->visible.max ? n : wp->visible.max;
  if (n < wp->visible.max)
    xitk_widgets_delete (wp->visible.btns + _W_items + n, wp->visible.max - n);
  wp->visible.max = n;

  _browser_vtab_init (wp);
  wp->visible.start = 0;
  wp->visible.num = n < wp->items.num ? n : wp->items.num;

  x = info ? info->x : 0;
  y = info ? info->y : 0;
  wp->w.x = wp->visible.x = x;
  wp->w.y = wp->visible.y = y;

  if (keep > 0) {
    int i, j = wp->visible.num < keep ? wp->visible.num : keep;

    h = xitk_get_widget_height (wp->visible.btns[_W_items]) + 1;
    for (i = 0; i < j; i++) {
      xitk_widget_t *w = wp->visible.btns[_W_items + i];

      xitk_set_widget_pos (w, x, y);
      xitk_widgets_state (&w, 1, XITK_WIDGET_STATE_ENABLE, ~0u);
      xitk_labelbutton_change_label (w, wp->items.names[i]);
      if (wp->items.shortcuts)
        xitk_labelbutton_change_shortcut_label (w, i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
      y += h;
    }
    for (; i < keep; i++) {
      xitk_widget_t *w = wp->visible.btns[_W_items + i];

      xitk_set_widget_pos (w, x, y);
      xitk_widgets_state (&w, 1, XITK_WIDGET_STATE_ENABLE, 0);
      xitk_labelbutton_change_label (w, "");
      if (wp->items.shortcuts)
        xitk_labelbutton_change_shortcut_label (w, i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
      y += h;
    }
    prev = &wp->visible.btns[_W_items + i - 1]->node;
  } else {
    h = 0;
    prev = wp->w.wl->list.tail.prev;
  }

  if (keep < n) {
    xitk_labelbutton_widget_t lb;
    int i;
    XITK_WIDGET_INIT (&lb);

    lb.button_type       = RADIO_BUTTON;
    lb.align             = ALIGN_DEFAULT;
    lb.state_callback    = browser_select;
    lb.skin_element_name = wp->skin_element_name.s;
    lb.callback          = NULL;

    for (i = keep; i < n; i++) {
      xitk_widget_t *w;

      wp->visible.blist[i] = wp;
      lb.label    = i < wp->items.num ? wp->items.names[i] : "";
      lb.userdata = wp->visible.blist + i;
      wp->visible.btns[_W_items + i] = w = xitk_labelbutton_create (wp->w.wl, wp->skonfig, &lb);
      if (!w)
        break;
      xitk_dnode_insert_after (prev, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
      w->type &= ~WIDGET_TABABLE;
      xitk_widget_set_parent (w, &wp->w);
      if (i >= wp->visible.num)
        xitk_widgets_state (&w, 1, XITK_WIDGET_STATE_ENABLE, 0);
      xitk_set_widget_pos (w, x, y);
      if (h <= 0)
        h = xitk_get_widget_height (w) + 1;
      y += h;
      if (wp->items.shortcuts)
        xitk_labelbutton_change_shortcut_label (w,
          i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
      prev = &w->node;
    }
    wp->visible.max = i;
  }

  /* set hull */
  {
    int x1 = 0x7fffffff, x2 = 0, y1 = 0x7fffffff, y2 = 0, i;
    for (i = 0; i < _W_items + wp->visible.max; i++) {
      xitk_widget_t *w = wp->visible.btns[i];
      if (!w)
        continue;
      if (w->width <= 0)
        continue;
      x1 = xitk_min (x1, w->x);
      x2 = xitk_max (x2, w->x + w->width);
      y1 = xitk_min (y1, w->y);
      y2 = xitk_max (y2, w->y + w->height);
    }
    wp->w.x = x1;
    wp->w.width = x2 - x1;
    wp->w.y = y1;
    wp->w.height = y2 - y1;
  }
}

static void _browser_new_skin (_browser_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name.s) {
    const xitk_skin_element_info_t *info;

    xitk_skin_lock (skonfig);
    info = xitk_skin_get_info (skonfig, wp->skin_element_name.s);

    wp->skonfig = skonfig;
    xitk_widget_state_from_info (&wp->w, info);

    xitk_short_string_set (&wp->visible.fontname, info ? info->label_fontname : NULL);
    _browser_item_btns (wp, info);
    xitk_skin_unlock (skonfig);

    _browser_set_items (wp, wp->items.names, wp->items.shortcuts, wp->items.num);
    _browser_set_hslider (wp, 1);
    _browser_set_btns (wp);
    _browser_move (wp, 0);
    _browser_set_vslider (wp);
  }
}

static void _browser_notify_destroy (_browser_private_t *wp) {
  xitk_short_string_deinit (&wp->visible.fontname);
  xitk_short_string_deinit (&wp->skin_element_name);
}

static void _browser_enability (_browser_private_t *wp) {
  if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
    if (wp->items.num > wp->visible.max)
      xitk_widgets_state (wp->visible.btns + _W_up, 3, XITK_WIDGET_STATE_ENABLE, ~0u);
    if (wp->visible.xmax)
      xitk_widgets_state (wp->visible.btns + _W_left, 3, XITK_WIDGET_STATE_ENABLE, ~0u);
    xitk_widgets_state (wp->visible.btns + _W_items, wp->visible.num, XITK_WIDGET_STATE_ENABLE, ~0u);
  } else {
    xitk_widgets_state (wp->visible.btns, _W_items + wp->visible.max, XITK_WIDGET_STATE_ENABLE, 0);
  }
}

/*
 * Jump to entry in list which match with the alphanum char key.
 */
static int _browser_warp_jump (_browser_private_t *wp, const char *key, int modifier) {
  int i, v;
  size_t klen;

  if ((modifier & ~MODIFIER_NUML) == MODIFIER_NOMOD) {
    v = _browser_get_current_item (wp, &i);
    klen = strlen (key);
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
        return 1;
    }
  } else if ((modifier & ~MODIFIER_NUML) == MODIFIER_SHIFT) {
    v = _browser_get_current_item (wp, &i);
    klen = strlen (key);
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
        return 1;
    }
  } else {
    return 0;
  }

  wp->items.last_over = i;
  _browser_move (wp, i - (wp->visible.max >> 1) - wp->visible.start);
  _browser_set_vslider (wp);

  return 1;
}

static void _browser_hslidmove (_browser_private_t *wp, int pos) {
  if (wp->visible.xmax) {
    if (pos != wp->visible.dx) {
      int i;
      wp->visible.dx = pos;
      xitk_widgets_state (wp->visible.btns + _W_items, wp->visible.num, XITK_WIDGET_STATE_VISIBLE, 0);
      for (i = 0; i < wp->visible.num; i++)
        xitk_labelbutton_set_label_offset (wp->visible.btns[i + _W_items], -pos);
      xitk_widgets_state (wp->visible.btns + _W_items, wp->visible.num, XITK_WIDGET_STATE_VISIBLE, ~0u);
    }
  } else {
    xitk_slider_reset (wp->visible.btns[_W_hor]);
  }
}


static int _browser_key (_browser_private_t *wp, const char *string, int modifier) {
  if (!(wp->w.state & XITK_WIDGET_STATE_ENABLE) || !string)
    return 0;

  if (string[0] != XITK_CTRL_KEY_PREFIX)
    return _browser_warp_jump (wp, string, modifier);

  if ((modifier & ~MODIFIER_NUML) != MODIFIER_NOMOD)
    return 0;

  switch (string[1]) {
    int v, f;
    case XITK_KEY_LEFT:
      _browser_hslidmove (wp, xitk_slider_make_backstep (wp->visible.btns[_W_hor]) - wp->visible.x0);
      return 1;
    case XITK_KEY_RIGHT:
      _browser_hslidmove (wp, xitk_slider_make_step (wp->visible.btns[_W_hor]) - wp->visible.x0);
      return 1;

    case XITK_KEY_HOME:
      _browser_get_current_item (wp, &f);
      v = 0;
      goto _key_move;
    case XITK_KEY_END:
      _browser_get_current_item (wp, &f);
      v = wp->items.num - 1;
      if (v < 0)
        v = 0;
      goto _key_move;

    case XITK_KEY_PREV:
      v = _browser_get_current_item (wp, &f) - wp->visible.num;
      if (v < 0)
        v = wp->visible.start;
      goto _key_move;
    case XITK_KEY_NEXT:
      v = _browser_get_current_item (wp, &f) + wp->visible.num;
      if (v >= wp->items.num) {
        v = wp->items.num - 1;
        if (v < 0)
          v = 0;
      }
      goto _key_move;

    case XITK_KEY_UP:
      v = _browser_get_current_item (wp, &f) - 1;
      if (v < 0)
        v = wp->visible.start;
      goto _key_move;
    case XITK_KEY_DOWN:
      v = _browser_get_current_item (wp, &f) + 1;
      if (v >= wp->items.num) {
        v = wp->items.num - 1;
        if (v < 0)
          v = 0;
      }
    _key_move:
      wp->items.last_over = v;
      _browser_move (wp, v - (wp->visible.max >> 1) - wp->visible.start);
      _browser_set_vslider (wp);
      return 1;

    case XITK_MOUSE_WHEEL_UP:
      v = -1;
      goto _mouse_move;
    case XITK_MOUSE_WHEEL_DOWN:
      v = 1;
    _mouse_move:
      wp->items.last_over = -1;
      _browser_move (wp, v);
      _browser_set_vslider (wp);
      return 1;

    default: ;
  }
  return 0;
}

static int browser_notify_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _browser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp || !event)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return 0;
  (void)result;
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _browser_paint (wp);
      break;
    case WIDGET_EVENT_KEY:
      return _browser_key (wp, event->string, event->modifier);
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
  _browser_private_t *wp;

  xitk_container (wp, w, w);
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
  _browser_private_t *wp;

  xitk_container (wp, w, w);
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
  _browser_private_t *wp;
  int i;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  for (i = 0; i < wp->visible.max; i++)
    xitk_labelbutton_set_alignment (wp->visible.btns[i + _W_items], align);
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
  _browser_private_t *wp;

  xitk_container (wp, w, w);
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
    xitk_labelbutton_set_state (wp->visible.btns[i + _W_items], 0);
}

void xitk_browser_release_all_buttons(xitk_widget_t *w) {
  _browser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
  _xitk_browser_release_all_buttons (wp);
}

/**
 * Select the item 'select' in list
 */
void xitk_browser_set_select (xitk_widget_t *w, int item) {
  _browser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  gettimeofday (&wp->click_time, NULL);
  _browser_select (wp, item);
  if (wp->items.selected >= 0) {
    _browser_move (wp, wp->items.selected - (wp->visible.max >> 1) - wp->visible.start);
    _browser_set_vslider (wp);
  }
}

/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, const char *const *list, const char *const *shortcut, int len, int start) {
  _browser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;

  _browser_set_items (wp, list, shortcut, len);
  _browser_set_hslider (wp, 1);
  _browser_set_btns (wp);
  wp->visible.start = -MAX_VISIBLE;
  _browser_move (wp, start - wp->visible.start);
  _browser_set_vslider (wp);
  if (wp->items.selected >= wp->items.num)
    _browser_select (wp, -1);
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

  _browser_slidmove (wp, xitk_slider_make_step (wp->visible.btns[_W_vert]));
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

  _browser_slidmove (wp, xitk_slider_make_backstep (wp->visible.btns[_W_vert]));
}

static void browser_left(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;

  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  _browser_hslidmove (wp, xitk_slider_make_backstep (wp->visible.btns[_W_hor]) - wp->visible.x0);
}

static void browser_right(xitk_widget_t *w, void *data) {
  _browser_private_t *wp = (_browser_private_t *)data;

  if (!w || !wp)
    return;
  if (!(w->type & WIDGET_GROUP_BROWSER))
    return;

  _browser_hslidmove (wp, xitk_slider_make_step (wp->visible.btns[_W_hor]) - wp->visible.x0);
}

/**
 * Create the list browser
 */
static xitk_widget_t *_xitk_browser_create (_browser_private_t *wp, const xitk_browser_widget_t *br) {
  int i;

  for (i = 0; i < _W_items; i++) {
    xitk_widget_t *w = wp->visible.btns[i];
    if (w) {
      xitk_dlist_add_tail (&wp->w.wl->list, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
      w->type &= ~WIDGET_TABABLE;
      xitk_widget_set_parent (w, &wp->w);
    }
  }

  gettimeofday (&wp->click_time, NULL);

  wp->dbl_click_callback = br->dbl_click_callback;

  wp->callback = br->callback;
  wp->userdata = br->userdata;

  wp->visible.xmax = 0;
  wp->visible.dx = 0;

  wp->items.last_over = -1;
  wp->items.selected = -1;

  _browser_vtab_init (wp);
  _browser_set_hslider (wp, 1);
  _browser_set_vslider (wp);

  wp->w.type = WIDGET_TABABLE | WIDGET_KEYABLE | WIDGET_GROUP | WIDGET_TYPE_BROWSER;
  wp->w.event = browser_notify_event;
  _browser_show (wp);

  return &wp->w;
}

/**
 * Create the list browser
 */
xitk_widget_t *xitk_browser_create(xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_browser_widget_t *br) {
  _browser_private_t *wp;
  xitk_button_widget_t           b;
  xitk_slider_widget_t           sl;
  const xitk_skin_element_info_t *info;

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(br);

  wp = (_browser_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&b);
  XITK_WIDGET_INIT(&sl);

  wp->w.width = 0;
  wp->w.height = 0;

  /* sliders are there as part of skin design. */
  wp->slider_width = 1;
  wp->with_hslider = 1;
  wp->with_vslider = 1;

  xitk_short_string_init (&wp->skin_element_name);
  xitk_short_string_set (&wp->skin_element_name, br->browser.skin_element_name);
  info = xitk_skin_get_info (skonfig, wp->skin_element_name.s);
  xitk_short_string_init (&wp->visible.fontname);
  xitk_short_string_set (&wp->visible.fontname, info ? info->label_fontname : NULL);
  _browser_set_items (wp, br->browser.entries, NULL, br->browser.num_entries);
  wp->skonfig = skonfig;
  wp->visible.max = 0;

  b.state_callback    = NULL;
  b.userdata          = wp;

  b.skin_element_name = br->arrow_up.skin_element_name;
  b.callback          = browser_up;
  wp->visible.btns[_W_up] = xitk_button_create (wp->w.wl, skonfig, &b);

  sl.min             = 0;
  sl.max             = 1;
  sl.step            = 1;
  sl.userdata        =
  sl.motion_userdata = wp;

  sl.skin_element_name = br->slider.skin_element_name;
  sl.callback          =
  sl.motion_callback   = browser_slidmove;
  wp->visible.btns[_W_vert] = xitk_slider_create (wl, skonfig, &sl);

  b.skin_element_name = br->arrow_dn.skin_element_name;
  b.callback          = browser_down;
  wp->visible.btns[_W_down] = xitk_button_create (wl, skonfig, &b);

  b.skin_element_name = br->arrow_left.skin_element_name;
  b.callback          = browser_left;
  wp->visible.btns[_W_left] = xitk_button_create (wl, skonfig, &b);

  sl.skin_element_name = br->slider_h.skin_element_name;
  sl.callback          =
  sl.motion_callback   = browser_hslidmove;
  wp->visible.btns[_W_hor] = xitk_slider_create (wl, skonfig, &sl);

  b.skin_element_name = br->arrow_right.skin_element_name;
  b.callback          = browser_right;
  wp->visible.btns[_W_right] = xitk_button_create (wl, skonfig, &b);

  _browser_item_btns (wp, info);

  xitk_widget_state_from_info (&wp->w, info);
  return _xitk_browser_create (wp, br);
}


/*
 *
 */
xitk_widget_t *xitk_noskin_browser_create (xitk_widget_list_t *wl, const xitk_browser_widget_t *br,
  int x, int y, int itemw, int itemh, int slidw, const char *fontname) {
  _browser_private_t *wp;
  xitk_button_widget_t b;
  xitk_slider_widget_t sl;
  int                  sw = slidw < 0 ? -slidw : slidw;
  int                  sh = (itemh >> 1) > sw ? (itemh >> 1) : sw;
  int                  iw;

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(br);

  wp = (_browser_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&b);
  XITK_WIDGET_INIT(&sl);

  wp->w.x = wp->visible.x = x;
  wp->w.y = wp->visible.y = y;
  wp->skin_element_name.s = NULL;
  xitk_short_string_init (&wp->visible.fontname);
  xitk_short_string_set (&wp->visible.fontname, fontname);

  _browser_set_items (wp, br->browser.entries, NULL, br->browser.num_entries);

  wp->visible.max = br->browser.max_displayed_entries <= MAX_VISIBLE ? br->browser.max_displayed_entries : MAX_VISIBLE;
  wp->slider_width = slidw;
  if (wp->slider_width < 0) {
    wp->w.width = itemw;
    wp->w.height = itemh * wp->visible.max;
    wp->with_hslider = wp->items.width + 4 > itemw;
    if (wp->with_hslider)
      wp->visible.max -= 1;
    if (wp->visible.max < wp->items.num) {
      iw = itemw + wp->slider_width;
      wp->with_vslider = 1;
      if (!wp->with_hslider && (wp->items.width + 4 > iw)) {
        wp->with_hslider = 1;
        wp->visible.max -= 1;
      }
      wp->visible.num = wp->visible.max;
    } else {
      iw = itemw;
      wp->with_vslider = 0;
      wp->visible.num = wp->items.num;
    }
  } else {
    iw = itemw;
    wp->with_hslider = wp->with_vslider = (wp->slider_width > 0);
    wp->visible.num = wp->visible.max < wp->items.num ? wp->visible.max : wp->items.num;
    wp->w.width = itemw + slidw;
    wp->w.height = itemh * wp->visible.max + slidw;
  }
 
  wp->visible.start = 0;

  {
    xitk_labelbutton_widget_t lb;
    int ix = x, iy = y, i;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = RADIO_BUTTON;
    lb.align             = ALIGN_LEFT;
    lb.callback          = NULL;
    lb.state_callback    = browser_select;
    lb.skin_element_name = "XITK_NOSKIN_FLAT";

    for (i = 0; i < wp->visible.max; i++) {
      xitk_widget_t *w;

      wp->visible.blist[i] = wp;
      lb.label             = i < wp->items.num ? wp->items.names[i] : "";
      lb.userdata          = wp->visible.blist + i;
      wp->visible.btns[i + _W_items] = w = xitk_noskin_labelbutton_create (wl, &lb,
        ix, iy, iw, itemh, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, fontname);
      if (!w)
        break;
      xitk_dlist_add_tail (&wl->list, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_BROWSER;
      w->type &= ~WIDGET_TABABLE;
      xitk_widget_set_parent (w, &wp->w);
      if (i >= wp->visible.num)
        xitk_widgets_state (&w, 1, XITK_WIDGET_STATE_ENABLE, 0);
      xitk_set_widget_pos (w, ix, iy);
      if (wp->items.shortcuts)
        xitk_labelbutton_change_shortcut_label (w,
          i < wp->items.snum ? wp->items.shortcuts[i] : "", 0, NULL);
      iy += itemh;
    }
    wp->visible.max = i;
  }

  b.state_callback    = NULL;
  b.userdata          = wp;

  sl.min               = 0;
  sl.max               = 1;
  sl.step              = 1;
  sl.skin_element_name = NULL;
  sl.userdata          =
  sl.motion_userdata   = wp;

  if (wp->with_vslider) {
    b.skin_element_name = "XITK_NOSKIN_UP";
    b.callback          = browser_up;
    wp->visible.btns[_W_up] = xitk_noskin_button_create (wl, &b, x + iw, y + 0, sw, sh);

    sl.callback        =
    sl.motion_callback = browser_slidmove;
    wp->visible.btns[_W_vert] = xitk_noskin_slider_create (wl, &sl,
      x + iw, y + sh, sw, itemh * wp->visible.max - sh * 2, XITK_HVSLIDER);

    b.skin_element_name = "XITK_NOSKIN_DOWN";
    b.callback          = browser_down;
    wp->visible.btns[_W_down] = xitk_noskin_button_create (wl, &b,
      x + iw, y + itemh * wp->visible.max - sh, sw, sh);
  } else {
    wp->visible.btns[_W_up] = NULL;
    wp->visible.btns[_W_vert] = NULL;
    wp->visible.btns[_W_down] = NULL;
  }

  if (wp->with_hslider) {
    b.skin_element_name = "XITK_NOSKIN_LEFT";
    b.callback          = browser_left;
    wp->visible.btns[_W_left] = xitk_noskin_button_create (wl, &b,
      x, y + itemh * wp->visible.max, sh, sw);

    sl.callback        =
    sl.motion_callback = browser_hslidmove;
    wp->visible.btns[_W_hor] = xitk_noskin_slider_create (wl, &sl,
      x + sh, y + itemh * wp->visible.max, iw - sh * 2, sw, XITK_HVSLIDER);

    b.skin_element_name = "XITK_NOSKIN_RIGHT";
    b.callback          = browser_right;
    wp->visible.btns[_W_right] = xitk_noskin_button_create (wl, &b,
      x + iw - sh, y + itemh * wp->visible.max, sh, sw);
  } else {
    wp->visible.btns[_W_left] = NULL;
    wp->visible.btns[_W_hor] = NULL;
    wp->visible.btns[_W_right] = NULL;
  }

  wp->w.state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->skin_element_name.s = NULL;
  wp->skonfig = NULL;
  return _xitk_browser_create (wp, br);
}
