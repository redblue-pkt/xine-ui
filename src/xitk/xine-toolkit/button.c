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

#include "_xitk.h"
#include "button.h"

typedef struct {
  xitk_widget_t       w;

  xitk_short_string_t skin_element_name;
  int                 s5;
  xitk_part_image_t   skin;

  /* callback function (active_widget, user_data) */
  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
  void                   *userdata;
} _button_private_t;

/*
 *
 */
static void _button_destroy (_button_private_t *wp) {
  if (!wp->skin_element_name.s)
    xitk_image_free_image (&wp->skin.image);
  xitk_short_string_deinit (&wp->skin_element_name);
}

/*
 *
 */
static xitk_image_t *_button_get_skin (_button_private_t *wp, int sk) {
  if ((sk == FOREGROUND_SKIN) && wp->skin.image)
    return wp->skin.image;
  return NULL;
}

/*
 *
 */
static int _button_inside (_button_private_t *wp, int x, int y) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    xitk_image_t *skin = wp->skin.image;

    return xitk_image_inside (skin, x + wp->skin.x - wp->w.x, y + wp->skin.y - wp->w.y);
  }
  return 0;
}

/**
 *
 */
static void _button_paint (_button_private_t *wp, widget_event_t *event) {
  int f = 0;
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.button.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if ((~wp->w.state & wp->w.shown_state & XITK_WIDGET_STATE_FOCUS) && (wp->w.type & WIDGET_GROUP_COMBO)) {
    f = 1;
    wp->w.state &= ~(XITK_WIDGET_STATE_ON | XITK_WIDGET_STATE_CLICK);
  }

  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    xitk_img_state_t state = xitk_image_find_state (wp->s5 ? XITK_IMG_STATE_DISABLED_SELECTED : XITK_IMG_STATE_SELECTED,
      (wp->w.state & XITK_WIDGET_STATE_ENABLE), (wp->w.state & (XITK_WIDGET_STATE_FOCUS | XITK_WIDGET_STATE_MOUSE)),
      (wp->w.state & XITK_WIDGET_STATE_CLICK), (wp->w.state & XITK_WIDGET_STATE_ON));

    xitk_part_image_draw (wp->w.wl, &wp->skin, NULL,
      (int)state * wp->w.width + event->x - wp->w.x, event->y - wp->w.y,
      event->width, event->height,
      event->x, event->y);
  }

  if (f && wp->state_callback)
    wp->state_callback (&wp->w, wp->userdata, !!(wp->w.state & XITK_WIDGET_STATE_ON));
}

static void _button_read_skin (_button_private_t *wp, xitk_skin_config_t *skonfig) {
  const xitk_skin_element_info_t *s = xitk_skin_get_info (skonfig, wp->skin_element_name.s);

  xitk_widget_state_from_info (&wp->w, s);
  if (s) {
    wp->skin      = s->pixmap_img;
    wp->w.x       = s->x;
    wp->w.y       = s->y;
  } else {
    wp->skin.x      = 0;
    wp->skin.y      = 0;
    wp->skin.width  = 0;
    wp->skin.height = 0;
    wp->skin.image  = NULL;
    wp->w.x         = 0;
    wp->w.y         = 0;
  }
}

/*
 *
 */
static void _button_new_skin (_button_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name.s) {
    xitk_skin_lock (skonfig);
    _button_read_skin (wp, skonfig);
    xitk_skin_unlock (skonfig);
    wp->w.width   = wp->skin.width / 3;
    wp->w.height  = wp->skin.height;
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}

/*
 *
 */
static int _button_click (_button_private_t *wp, int button, int bUp, int x, int y) {
  (void)x;
  (void)y;
  if (button == 1) {
    widget_event_t event;

    if (bUp && (wp->w.state & XITK_WIDGET_STATE_FOCUS) && wp->state_callback)
      wp->w.state ^= XITK_WIDGET_STATE_ON;

    wp->w.state &= ~XITK_WIDGET_STATE_CLICK;
    wp->w.state |= bUp ? 0 : XITK_WIDGET_STATE_CLICK;
    event.x = wp->w.x;
    event.y = wp->w.y;
    event.width = wp->w.width;
    event.height = wp->w.height;
    _button_paint (wp, &event);
    wp->w.shown_state = wp->w.state;

    if (bUp && (wp->w.state & XITK_WIDGET_STATE_FOCUS)) {
      if (wp->state_callback)
        wp->state_callback (&wp->w, wp->userdata, !!(wp->w.state & XITK_WIDGET_STATE_ON));
      else if (wp->callback)
        wp->callback (&wp->w, wp->userdata);
    }
    return 1;
  }
  return 0;
}

static int _button_key (_button_private_t *wp, const char *s, int modifier, int key_up) {
  static const char k[] = {
    XITK_CTRL_KEY_PREFIX, XITK_KEY_RETURN,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_NUMPAD_ENTER,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_ISO_ENTER,
    ' ', 0
  };
  widget_event_t event;
  int i, n = sizeof (k) / sizeof (k[0]);

  if (!(wp->w.state & XITK_WIDGET_STATE_FOCUS))
    return 0;
  if (!s)
    return 0;

  if (modifier & ~(MODIFIER_SHIFT | MODIFIER_NUML))
    return 0;

  for (i = 0; i < n; i += 2) {
    if (!memcmp (s, k + i, 2))
      break;
  }
  if (i >= n)
    return 0;

  wp->w.state &= ~XITK_WIDGET_STATE_CLICK;
  if (wp->state_callback) {
    if (key_up)
      return 0;
    wp->w.state ^= XITK_WIDGET_STATE_ON;
  } else {
    wp->w.state |= key_up ? 0 : XITK_WIDGET_STATE_CLICK;
  }

  event.x = wp->w.x;
  event.y = wp->w.y;
  event.width = wp->w.width;
  event.height = wp->w.height;
  _button_paint (wp, &event);
  wp->w.shown_state = wp->w.state;

  if (wp->w.state & XITK_WIDGET_STATE_FOCUS) {
    if (wp->state_callback)
      wp->state_callback (&wp->w, wp->userdata, !!(wp->w.state & XITK_WIDGET_STATE_ON));
    else if (key_up && wp->callback)
      wp->callback (&wp->w, wp->userdata);
  }
  return 1;
}

static int button_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _button_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp || ! event)
    return 0;
  if (((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BUTTON)
    && ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_CHECKBOX))
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _button_paint (wp, event);
      break;
    case WIDGET_EVENT_CLICK:
      return _button_click (wp, event->button, event->button_pressed, event->x, event->y);
    case WIDGET_EVENT_KEY:
      return _button_key (wp, event->string, event->modifier, !event->button_pressed);
    case WIDGET_EVENT_INSIDE:
      return _button_inside (wp, event->x, event->y) ? 1 : 2;
    case WIDGET_EVENT_CHANGE_SKIN:
      _button_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _button_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _button_get_skin (wp, event->skin_layer);
        return 1;
      }
      break;
    default: ;
  }
  return 0;
}

/*
 *
 */
static xitk_widget_t *_xitk_button_create (_button_private_t *wp, const xitk_button_widget_t *b) {
  ABORT_IF_NULL (wp->w.wl);

  wp->callback          = b->callback;
  wp->state_callback    = b->state_callback;
  wp->userdata          = b->userdata;

  wp->w.width           = wp->skin.width / (wp->s5 ? 6 : 3);
  wp->w.height          = wp->skin.height;
  wp->w.type            = (wp->state_callback ? WIDGET_TYPE_CHECKBOX : WIDGET_TYPE_BUTTON)
                        | WIDGET_CLICKABLE | WIDGET_FOCUSABLE | WIDGET_TABABLE
                        | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = button_event;

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_button_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_button_widget_t *b) {
  _button_private_t *wp;

  XITK_CHECK_CONSTITENCY(b);
  wp = (_button_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->s5 = 0;
  xitk_short_string_init (&wp->skin_element_name);
  xitk_short_string_set (&wp->skin_element_name, b->skin_element_name);
  _button_read_skin (wp, skonfig);

  return _xitk_button_create (wp, b);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_button_create (xitk_widget_list_t *wl,
  const xitk_button_widget_t *b, int x, int y, int width, int height) {
  static const char *noskin_names[] = {
    "XITK_NOSKIN_LEFT",
    "XITK_NOSKIN_RIGHT",
    "XITK_NOSKIN_UP",
    "XITK_NOSKIN_DOWN",
    "XITK_NOSKIN_PLUS",
    "XITK_NOSKIN_MINUS",
    "XITK_NOSKIN_CHECK"
  };
  _button_private_t *wp;
  xitk_button_widget_t _b;
  unsigned int u = sizeof (noskin_names) / sizeof (noskin_names[0]);
  xitk_image_t *i;

  XITK_CHECK_CONSTITENCY(b);
  wp = (_button_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  _b = *b;
  if (_b.skin_element_name) {
    for (u = 0; u < sizeof (noskin_names) / sizeof (noskin_names[0]); u++) {
      if (!strcmp (_b.skin_element_name, noskin_names[u]))
        break;
    }
    _b.skin_element_name = NULL;
  }
  if (u == sizeof (noskin_names) / sizeof (noskin_names[0])) {
    i = xitk_image_new (wl->xitk, NULL, 0, width * 3, height);
    xitk_image_draw_bevel_three_state (i);
    wp->s5 = 0;
  } else {
    wp->s5 = 1;
    if (xitk_shared_image (wl, noskin_names[u], width * 6, height, &i) == 1) {
      i->last_state = XITK_IMG_STATE_DISABLED_SELECTED;
      if (u != 6)
        xitk_image_draw_bevel_three_state (i);
      switch (u) {
        case 0:
          xitk_image_draw_arrow_left (i);
          break;
        case 1:
          xitk_image_draw_arrow_right (i);
          break;
        case 2:
          xitk_image_draw_arrow_up (i);
          break;
        case 3:
          xitk_image_draw_arrow_down (i);
          break;
        case 4:
          xitk_image_draw_button_plus (i);
          break;
        case 5:
          xitk_image_draw_button_minus (i);
          break;
        case 6:
          xitk_image_draw_checkbox_check (i);
          break;
        default: ;
      }
    }
  }

  wp->w.x = x;
  wp->w.y = y;
  wp->skin_element_name.s = NULL;
  wp->skin.image = i;
  wp->skin.x        = 0;
  wp->skin.y        = 0;
  wp->skin.width    = i->width;
  wp->skin.height   = i->height;
  wp->w.state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  return _xitk_button_create (wp, &_b);
}
