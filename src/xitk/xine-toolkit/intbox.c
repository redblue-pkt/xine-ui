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

#include <stdint.h>
#include <string.h>

#include "_xitk.h"
#include "intbox.h"
#include "inputtext.h"
#include "button.h"
#include "slider.h"
#include "default_font.h"

typedef enum {
  _W_input = 0,
  _W_more,
  _W_less,
  _W_slider,
  _W_LAST
} _W_t;

typedef struct {
  xitk_widget_t        w;

  xitk_intbox_widget_t info;
  char                    skin_element_name[64];

  int                  input_width, slider_width, pm_width;
  xitk_slider_hv_t     hv;

  xitk_widget_t       *iw[_W_LAST];
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
  if (!wp->iw[_W_input])
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
  if (strcmp ((const char *)q, xitk_inputtext_get_text (wp->iw[_W_input])))
    xitk_inputtext_change_text (wp->iw[_W_input], (const char *)q);
}

static void _ib_set_slider (_intbox_private_t *wp) {
  if (!wp->iw[_W_slider])
    return;
  wp->hv.h.pos = wp->info.value - wp->info.min;
  wp->hv.h.step = 1;
  wp->hv.h.visible = 1;
  wp->hv.h.max = wp->info.max - wp->info.min + 1;
  wp->hv.v.pos = 0;
  wp->hv.v.step = 1;
  wp->hv.v.visible = 0;
  wp->hv.v.max = 0;
  xitk_slider_hv_sync (wp->iw[_W_slider], &wp->hv,
    (wp->w.state & XITK_WIDGET_STATE_VISIBLE) ? XITK_SLIDER_SYNC_SET_AND_PAINT : XITK_SLIDER_SYNC_SET);
}

static void _ib_enability (_intbox_private_t *wp) {
  xitk_widgets_state (wp->iw + _W_input, 4, XITK_WIDGET_STATE_ENABLE, (wp->w.state & XITK_WIDGET_STATE_ENABLE));
}

/*
 *
 */
static void _ib_destroy (_intbox_private_t *wp) {
  xitk_widgets_delete (wp->iw + _W_input, 4);
}

/*
 *
 */
static void _ib_paint (_intbox_private_t *wp) {
  unsigned int show = 0;
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    xitk_set_widget_pos (wp->iw[_W_input], wp->w.x, wp->w.y);
    xitk_set_widget_pos (wp->iw[_W_more], wp->w.x + wp->input_width, wp->w.y);
    xitk_set_widget_pos (wp->iw[_W_less], wp->w.x + wp->input_width, wp->w.y + wp->w.height - wp->pm_width);
    xitk_set_widget_pos (wp->iw[_W_slider], wp->w.x + wp->input_width + 2, wp->w.y);
    show = ~0u;
  }
  xitk_widgets_state (wp->iw + _W_input, 4, XITK_WIDGET_STATE_VISIBLE, show);
}

/*
 *
 */
static void _ib_new_skin (_intbox_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name[0]) {
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
  if (wp->iw[_W_input])
    xitk_set_widget_tips_and_timeout (wp->iw[_W_input], wp->w.tips_string, timeout);
  if (wp->iw[_W_slider])
    xitk_set_widget_tips_and_timeout (wp->iw[_W_slider], wp->w.tips_string, timeout);
}

static int notify_event (xitk_widget_t *w, const widget_event_t *event) {
  _intbox_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INTBOX)
    return 0;

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
  _intbox_private_t *wp;

  xitk_container (wp, w, w);
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
  _intbox_private_t *wp;

  xitk_container (wp, w, w);
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
  xitk_slider_hv_sync (wp->iw[_W_slider], &wp->hv, XITK_SLIDER_SYNC_GET);
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
  wp->w.state   &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->w.type     = WIDGET_GROUP | WIDGET_TYPE_INTBOX;
  wp->w.event    = notify_event;

  wp->info = *ib;
  wp->skin_element_name[0] = 0;

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
  wp->iw[_W_input] = xitk_noskin_inputtext_create (wl, &inp,
    x, y, wp->input_width, wp->w.height, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, DEFAULT_FONT_10);
  if (wp->iw[_W_input]) {
    xitk_dlist_add_tail (&wl->list, &wp->iw[_W_input]->node);
    wp->iw[_W_input]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
  }
  _ib_set_text (wp);

  if (wp->pm_width) {
    xitk_button_widget_t b;

    XITK_WIDGET_INIT (&b);

    b.state_callback    = NULL;
    b.userdata          = wp;

    b.skin_element_name = "XITK_NOSKIN_PLUS";
    b.callback          = intbox_plus;
    wp->iw[_W_more] = xitk_noskin_button_create (wl, &b,
      x + wp->input_width, y, wp->pm_width, wp->pm_width);
    if (wp->iw[_W_more]) {
      xitk_dlist_add_tail (&wl->list, &wp->iw[_W_more]->node);
      wp->iw[_W_more]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
    }

    b.skin_element_name = "XITK_NOSKIN_MINUS";
    b.callback          = intbox_minus;
    wp->iw[_W_less] = xitk_noskin_button_create (wl, &b,
      x + wp->input_width, y + wp->w.height - wp->pm_width, wp->pm_width, wp->pm_width);
    if (wp->iw[_W_less]) {
      xitk_dlist_add_tail (&wl->list, &wp->iw[_W_less]->node);
      wp->iw[_W_less]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
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
    wp->iw[_W_slider] = xitk_noskin_slider_create (wl, &sl,
        x + wp->input_width + 2, y, wp->slider_width, wp->w.height, XITK_HVSLIDER);
    if (wp->iw[_W_slider]) {
      xitk_dlist_add_tail (&wl->list, &wp->iw[_W_slider]->node);
      wp->iw[_W_slider]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_INTBOX;
    }
    _ib_set_slider (wp);
  }

  xitk_widget_set_focus_redirect (&wp->w, wp->iw[_W_input]);

  return &wp->w;
}
