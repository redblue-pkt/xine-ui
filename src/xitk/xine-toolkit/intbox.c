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

#include <stdint.h>
#include <string.h>

#include "_xitk.h"
#include "intbox.h"
#include "inputtext.h"
#include "button.h"
#include "slider.h"

typedef struct {
  xitk_widget_t        w;

  xitk_intbox_widget_t info;
  char                *skin_element_name;

  int                  input_width, slider_width, pm_width;
  xitk_slider_hv_t     hv;

  xitk_widget_t       *input_widget;
  xitk_widget_t       *more_widget;
  xitk_widget_t       *less_widget;
  xitk_widget_t       *slider_widget;
} _intbox_private_t;

static const uint8_t _ib_unhex[256] = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,255,255,255,255,255,255,
  255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static const uint8_t _ib_hex[16] = "0123456789abcdef";

static int _ib_from_string (const char *s) {
  const uint8_t *p = (const uint8_t *)s;
  int minus = 0;
  uint32_t v = 0;
  if (*p == '-') {
    p++;
    minus = 1;
  }
  if (p[0] == '#') {
    uint8_t z;
    p++;
    while (!((z = _ib_unhex[p[0]]) & 0x80)) {
      v = (v << 4) | z;
      p++;
    }
  } else if ((p[0] == '0') && ((p[1] & 0xdf) == 'X')) {
    uint8_t z;
    p += 2;
    while (!((z = _ib_unhex[p[0]]) & 0x80)) {
      v = (v << 4) | z;
      p++;
    }
  } else {
    uint8_t z;
    while ((z = p[0] ^ '0') < 10) {
      v = 10u * v + z;
      p++;
    }
  }
  return minus ? -(int)v : (int)v;
}

static void _ib_set_text (_intbox_private_t *wp) {
  uint8_t b[32], *q = b + sizeof (b);
  if (!wp->input_widget)
    return;
  *--q = 0;
  if (wp->info.fmt == INTBOX_FMT_DECIMAL) {
    uint32_t v = wp->info.value < 0 ? -wp->info.value : wp->info.value;
    do {
      *--q = '0' + (v % 10u);
      v /= 10u;
    } while (v);
    if (wp->info.value < 0)
      *--q = '-';
  } else {
    uint32_t v = wp->info.value;
    do {
      *--q = _ib_hex[v & 0x0f];
      v >>= 4;
    } while (v);
    if (wp->info.fmt == INTBOX_FMT_0x) {
      *--q = 'x';
      *--q = '0';
    } else if (wp->info.fmt == INTBOX_FMT_HASH) {
      *--q = '#';
    }
  }
  if (strcmp ((const char *)q, xitk_inputtext_get_text (wp->input_widget)))
    xitk_inputtext_change_text (wp->input_widget, (const char *)q);
}

static void _ib_set_slider (_intbox_private_t *wp) {
  if (!wp->slider_widget)
    return;
  wp->hv.h.pos = wp->info.value - wp->info.min;
  wp->hv.h.step = 1;
  wp->hv.h.visible = 1;
  wp->hv.h.max = wp->info.max - wp->info.min + 1;
  wp->hv.v.pos = 0;
  wp->hv.v.step = 1;
  wp->hv.v.visible = 0;
  wp->hv.v.max = 0;
  xitk_slider_hv_sync (wp->slider_widget, &wp->hv,
    wp->w.visible ? XITK_SLIDER_SYNC_SET_AND_PAINT : XITK_SLIDER_SYNC_SET);
}

static void _ib_enability (_intbox_private_t *wp) {
  if (wp->w.enable == WIDGET_ENABLE) {
    xitk_enable_widget (wp->input_widget);
    if (wp->more_widget)
      xitk_enable_widget (wp->more_widget);
    if (wp->less_widget)
      xitk_enable_widget (wp->less_widget);
    if (wp->slider_widget)
      xitk_enable_widget (wp->slider_widget);
  } else {
    xitk_disable_widget (wp->input_widget);
    if (wp->more_widget)
      xitk_disable_widget (wp->more_widget);
    if (wp->less_widget)
      xitk_disable_widget (wp->less_widget);
    if (wp->slider_widget)
      xitk_disable_widget (wp->slider_widget);
  }
}

/*
 *
 */
static void _ib_destroy (_intbox_private_t *wp) {
  if (wp->input_widget) {
    xitk_destroy_widget (wp->input_widget);
    wp->input_widget = NULL;
  }
  if (wp->more_widget) {
    xitk_destroy_widget (wp->more_widget);
    wp->more_widget = NULL;
  }
  if (wp->less_widget) {
    xitk_destroy_widget (wp->less_widget);
    wp->less_widget = NULL;
  }
  if (wp->slider_widget) {
    xitk_destroy_widget (wp->slider_widget);
    wp->slider_widget = NULL;
  }
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static void _ib_paint (_intbox_private_t *wp) {
  if (wp->w.visible == 1) {
    if (wp->input_widget) {
      xitk_set_widget_pos (wp->input_widget, wp->w.x, wp->w.y);
      xitk_show_widget (wp->input_widget);
    }
    if (wp->more_widget) {
      xitk_set_widget_pos (wp->more_widget, wp->w.x + wp->input_width, wp->w.y);
      xitk_show_widget (wp->more_widget);
    }
    if (wp->less_widget) {
      xitk_set_widget_pos (wp->less_widget, wp->w.x + wp->input_width, wp->w.y + wp->w.height - wp->pm_width);
      xitk_show_widget (wp->less_widget);
    }
    if (wp->slider_widget) {
      xitk_set_widget_pos (wp->slider_widget, wp->w.x + wp->input_width + 2, wp->w.y);
      xitk_show_widget (wp->slider_widget);
    }
  } else {
    if (wp->input_widget)
      xitk_hide_widget (wp->input_widget);
    if (wp->more_widget)
      xitk_hide_widget (wp->more_widget);
    if (wp->less_widget)
      xitk_hide_widget (wp->less_widget);
    if (wp->slider_widget)
      xitk_hide_widget (wp->slider_widget);
  }
}

/*
 *
 */
static void _ib_new_skin (_intbox_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->info.skin_element_name) {
#if 0
    int x, y;
    xitk_skin_lock (skonfig);
    /* visibility && enability */
    xitk_set_widget_pos (c, c->x, c->y);
    xitk_get_widget_pos (wp->label_widget, &x, &y);
    x += xitk_get_widget_width (wp->label_widget);
    xitk_set_widget_pos (wp->button_widget, x, y);
    xitk_skin_unlock (skonfig);
#else
    (void)wp;
    (void)skonfig;
#endif
  }
}

static void _ib_tips_timeout (_intbox_private_t *wp, unsigned long timeout) {
  if (wp->input_widget)
    xitk_set_widget_tips_and_timeout (wp->input_widget, wp->w.tips_string, timeout);
  if (wp->slider_widget)
    xitk_set_widget_tips_and_timeout (wp->slider_widget, wp->w.tips_string, timeout);
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _intbox_private_t *wp = (_intbox_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return 0;
  (void)result;
  
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _ib_paint (wp);
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _ib_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _ib_destroy (wp);
      break;
    case WIDGET_EVENT_ENABLE:
      _ib_enability (wp);
      break;
    case WIDGET_EVENT_TIPS_TIMEOUT:
      _ib_tips_timeout (wp, event->tips_timeout);
      break;
    default: ;
  }
  return 0;
}

/*
 *
 */
static void intbox_it (xitk_widget_t *x, void *data, const char *string) {
  _intbox_private_t *wp = (_intbox_private_t *)data;
  int v;

  if (!wp || !string)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return;

  (void)x;
  v = _ib_from_string (string);
  if (v > wp->info.max)
    v = wp->info.max;
  else if (v < wp->info.min)
    v = wp->info.min;
  if (v != wp->info.value) {
    wp->info.value = v;
    _ib_set_text (wp);
    _ib_set_slider (wp);
    if (wp->info.callback)
      wp->info.callback (&wp->w, wp->info.userdata, wp->info.value);
  } else {
    _ib_set_text (wp);
  }
}

/*
 *
 */
void xitk_intbox_set_value (xitk_widget_t *w, int value) {
  _intbox_private_t *wp = (_intbox_private_t *)w;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return;

  if (value > wp->info.max)
    value = wp->info.max;
  else if (value < wp->info.min)
    value = wp->info.min;
  if (value != wp->info.value) {
    wp->info.value = value;
    _ib_set_text (wp);
    _ib_set_slider (wp);
  }
}

/*
 *
 */
int xitk_intbox_get_value(xitk_widget_t *w) {
  _intbox_private_t *wp = (_intbox_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return 0;

  return wp->info.value;
}

/*
 *
 */
static void intbox_minus(xitk_widget_t *x, void *data) {
  _intbox_private_t *wp = (_intbox_private_t *)data;
  int v;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return;

  (void)x;
  v = wp->info.value - wp->info.step;
  if (v < wp->info.min)
    v = wp->info.min;
  if (v != wp->info.value) {
    wp->info.value = v;
    _ib_set_text (wp);
    _ib_set_slider (wp);
    if (wp->info.callback)
      wp->info.callback (&wp->w, wp->info.userdata, wp->info.value);
  }
}

/*
 *
 */
static void intbox_plus(xitk_widget_t *x, void *data) {
  _intbox_private_t *wp = (_intbox_private_t *)data;
  int v;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return;

  (void)x;
  v = wp->info.value + wp->info.step;
  if (v > wp->info.max)
    v = wp->info.max;
  if (v != wp->info.value) {
    wp->info.value = v;
    _ib_set_text (wp);
    _ib_set_slider (wp);
    if (wp->info.callback)
      wp->info.callback (&wp->w, wp->info.userdata, wp->info.value);
  }
}

static void intbox_sl (xitk_widget_t *x, void *data, int pos) {
  _intbox_private_t *wp = (_intbox_private_t *)data;
  int v;
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return;

  (void)x;
  (void)pos;
  xitk_slider_hv_sync (wp->slider_widget, &wp->hv, XITK_SLIDER_SYNC_GET);
  v = wp->info.min + wp->hv.h.pos;
  if (v != wp->info.value) {
    wp->info.value = v;
    _ib_set_text (wp);
    if (wp->info.callback)
      wp->info.callback (&wp->w, wp->info.userdata, wp->info.value);
  }
}

/*
 *
 */
xitk_widget_t *xitk_noskin_intbox_create (xitk_widget_list_t *wl,
  xitk_intbox_widget_t *ib, int x, int y, int width, int height) {
  _intbox_private_t *wp;
  xitk_inputtext_widget_t inp;
  char buf[256];

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(ib);

  wp = (_intbox_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&inp);

  wp->w.x        = x;
  wp->w.y        = y;
  wp->w.width    = width;
  wp->w.height   = height;
  wp->w.enable   = 0;
  wp->w.visible  = 0;
  wp->w.type     = WIDGET_GROUP | WIDGET_TYPE_INTBOX;
  wp->w.event    = notify_event;

  wp->info = *ib;
  wp->skin_element_name = wp->info.skin_element_name ? strdup (wp->info.skin_element_name) : NULL;

  if (wp->info.min >= wp->info.max) {
    int v = ~(unsigned int)0 >> 1;
    wp->info.max = v;
    wp->info.min = -v - 1;
  }
  if (wp->info.value > wp->info.max)
    wp->info.value = wp->info.max;
  else if (wp->info.value < wp->info.min)
    wp->info.value = wp->info.min;

  /* Create inputtext and buttons (not skinable) */
  if (wp->w.width >= 7 * wp->w.height) {
    wp->input_width = (5 * wp->w.height) >> 1;
    wp->slider_width = wp->w.width - wp->input_width - 2;
    wp->pm_width = 0;
  } else {
    wp->pm_width = wp->w.height >> 1;
    wp->input_width = wp->w.width - wp->pm_width;
    wp->slider_width = 0;
  }

  inp.skin_element_name = NULL;
  inp.text              = buf;
  inp.max_length        = 32;
  inp.callback          = intbox_it;
  inp.userdata          = (void *)wp;
  wp->input_widget = xitk_noskin_inputtext_create (wl, &inp,
    x, y, wp->input_width, wp->w.height, "Black", "Black", DEFAULT_FONT_10);
  if (wp->input_widget) {
    xitk_dlist_add_tail (&wl->list, &wp->input_widget->node);
    wp->input_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
  }
  _ib_set_text (wp);

  if (wp->pm_width) {
    xitk_button_widget_t b;

    XITK_WIDGET_INIT (&b);

    b.skin_element_name = "XITK_NOSKIN_PLUS";
    b.callback          = intbox_plus;
    b.userdata          = (void *)wp;
    wp->more_widget = xitk_noskin_button_create (wl, &b,
      x + wp->input_width, y, wp->pm_width, wp->pm_width);
    if (wp->more_widget) {
      xitk_dlist_add_tail (&wl->list, &wp->more_widget->node);
      wp->more_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
    }

    b.skin_element_name = "XITK_NOSKIN_MINUS";
    b.callback          = intbox_minus;
    b.userdata          = (void *)wp;
    wp->less_widget = xitk_noskin_button_create (wl, &b,
      x + wp->input_width, y + wp->w.height - wp->pm_width, wp->pm_width, wp->pm_width);
    if (wp->less_widget) {
      xitk_dlist_add_tail (&wl->list, &wp->less_widget->node);
      wp->less_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
    }
  }

  if (wp->slider_width) {
    xitk_slider_widget_t sl;

    XITK_WIDGET_INIT (&sl);
    sl.min = sl.max      = 0;
    sl.step              = 1;
    sl.skin_element_name = NULL;
    sl.callback          = intbox_sl;
    sl.userdata          = (void *)wp;
    sl.motion_callback   = intbox_sl;
    sl.motion_userdata   = (void *)wp;
    wp->slider_widget = xitk_noskin_slider_create (wl, &sl,
        x + wp->input_width + 2, y, wp->slider_width, wp->w.height, XITK_HVSLIDER);
    if (wp->slider_widget) {
      xitk_dlist_add_tail (&wl->list, &wp->slider_widget->node);
      wp->slider_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
    }
    _ib_set_slider (wp);
  }

  xitk_widget_set_focus_redirect (&wp->w, wp->input_widget);

  return &wp->w;
}
