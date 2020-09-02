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
#include "checkbox.h"

typedef struct {
  xitk_widget_t          w;

  char                  *skin_element_name;
  xitk_widget_t         *cWidget;
  int                    cClicked;
  int                    focus;
  int                    cState;
  int                    num_gfx;
  xitk_part_image_t      skin;

  xitk_state_callback_t  callback;
  void                  *userdata;

} _checkbox_private_t;

/*
 *
 */
static void _checkbox_destroy (_checkbox_private_t *wp) {
  if (!wp->skin_element_name)
    xitk_image_free_image (&wp->skin.image);
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static xitk_image_t *_checkbox_get_skin (_checkbox_private_t *wp, int sk) {
  if ((sk == FOREGROUND_SKIN) && wp->skin.image) {
    return wp->skin.image;
  }
  return NULL;
}

/*
 *
 */
static int _checkbox_inside (_checkbox_private_t *wp, int x, int y) {
  if (wp->w.visible == 1) {
    xitk_image_t *skin = wp->skin.image;
      
    if(skin->mask)
      return xitk_is_cursor_out_mask (&wp->w, skin->mask, x + wp->skin.x, y + wp->skin.y);
  } else
    return 0;
  return 1;
}

/*
 *
 */
static void _checkbox_paint (_checkbox_private_t *wp) {
  if (wp->w.visible == 1) {
    static const uint8_t i[] = {0, 2, 0, 2, 1, 2, 2, 2,  0, 1, 0, 1, 2, 3, 2, 3};
    int checkbox_width = wp->skin.width / wp->num_gfx;
    int mode = (wp->num_gfx == 4 ? 8 : 0)
             + ((wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN) ? 4 : 0)
             + (wp->cClicked ? 2 : 0) + (wp->cState ? 1 : 0);
    xitk_part_image_draw (wp->w.wl, &wp->skin, NULL,
      i[mode] * checkbox_width, 0, checkbox_width, wp->skin.height, wp->w.x, wp->w.y);
  }
}

/*
 *
 */
static int _checkbox_click (_checkbox_private_t *wp, int button, int cUp, int x, int y) {
  int ret = 0;

  (void)x;
  (void)y;
  if (button == Button1) {
    wp->cClicked = !cUp;
    if (cUp && (wp->focus == FOCUS_RECEIVED)) {
      wp->cState = !wp->cState;
      _checkbox_paint (wp);
      if (wp->callback)
        wp->callback (wp->cWidget, wp->userdata, wp->cState);
    } else {
      _checkbox_paint (wp);
    }
    ret = 1;
  }
  return ret;
}

static int _checkbox_key (_checkbox_private_t *wp, const char *s, int modifier) {
  static const char k[] = {
    XITK_CTRL_KEY_PREFIX, XITK_KEY_RETURN,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_NUMPAD_ENTER,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_ISO_ENTER,
    ' ', 0
  };
  int i, n = sizeof (k) / sizeof (k[0]);

  if (wp->focus != FOCUS_RECEIVED)
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

  wp->cClicked = 0;
  if (wp->focus == FOCUS_RECEIVED) {
    wp->cState = !wp->cState;
    _checkbox_paint (wp);
    if (wp->callback)
      wp->callback (wp->cWidget, wp->userdata, wp->cState);
  } else {
    _checkbox_paint (wp);
  }
  return 1;
}

/*
 *
 */
static int _checkbox_focus (_checkbox_private_t *wp, int focus) {
  wp->focus = focus;
  return 1;
}

/*
 *
 */
static void _checkbox_read_skin (_checkbox_private_t *wp, xitk_skin_config_t *skonfig) {
  const xitk_skin_element_info_t *s = xitk_skin_get_info (skonfig, wp->skin_element_name);
  if (s) {
    wp->w.x        = s->x;
    wp->w.y        = s->y;
    wp->w.visible  = s->visibility ? 1 : -1;
    wp->w.enable   = s->enability;
    wp->skin       = s->pixmap_img;
  }
}

static void _checkbox_new_skin (_checkbox_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name) {
    xitk_skin_lock(skonfig);
    _checkbox_read_skin (wp, skonfig);
    xitk_skin_unlock(skonfig);
    wp->w.width    = wp->skin.width / 3;
    wp->w.height   = wp->skin.height;
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}

static int checkbox_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _checkbox_private_t *wp = (_checkbox_private_t *)w;

  if (!event || !wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_CHECKBOX)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _checkbox_paint (wp);
      break;
    case WIDGET_EVENT_CLICK: {
      int r = _checkbox_click (wp, event->button, event->button_pressed, event->x, event->y);
      if (result) {
        result->value = r;
        return 1;
      }
      break;
    }
    case WIDGET_EVENT_KEY:
      return _checkbox_key (wp, event->string, event->modifier);
    case WIDGET_EVENT_FOCUS:
      _checkbox_focus (wp, event->focus);
      break;
    case WIDGET_EVENT_INSIDE:
      if (result) {
        result->value = _checkbox_inside (wp, event->x, event->y);
        return 1;
      }
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _checkbox_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _checkbox_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _checkbox_get_skin (wp, event->skin_layer);
        return 1;
      }
      break;
    default: ;
  }
  return 0;
}

void xitk_checkbox_callback_exec(xitk_widget_t *w) {
  _checkbox_private_t *wp = (_checkbox_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    if (wp->callback)
      wp->callback (wp->cWidget, wp->userdata, wp->cState);
  }
}

/*
 *
 */
int xitk_checkbox_get_state(xitk_widget_t *w) {
  _checkbox_private_t *wp = (_checkbox_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    return wp->cState;
  }
  return 0;
}

/*
 *
 */
void xitk_checkbox_set_state(xitk_widget_t *w, int state) {
  _checkbox_private_t *wp = (_checkbox_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) {
    int clk, focus;

    if (xitk_checkbox_get_state (&wp->w) != state) {
      focus = wp->focus;
      clk = wp->cClicked;
      
      wp->focus = FOCUS_RECEIVED;
      wp->cClicked = 1;
      wp->cState = state;

      _checkbox_paint (wp);

      wp->focus = focus;
      wp->cClicked = clk;

      _checkbox_paint (wp);
    }
  }
}

/*
 *
 */
static xitk_widget_t *_xitk_checkbox_create (_checkbox_private_t *wp, xitk_checkbox_widget_t *cb) {
  ABORT_IF_NULL (wp->w.wl);

  wp->cWidget         = &wp->w;
  wp->cClicked        = 0;
  wp->cState          = 0;
  wp->focus           = FOCUS_LOST;

  wp->callback        = cb->callback;
  wp->userdata        = cb->userdata;

  wp->w.private_data  = wp;

  wp->w.width    = wp->skin.width / 3;
  wp->w.height   = wp->skin.height;

  wp->w.running       = 1;
  wp->w.have_focus    = FOCUS_LOST;
  wp->w.type          = WIDGET_TYPE_CHECKBOX | WIDGET_CLICKABLE | WIDGET_FOCUSABLE | WIDGET_TABABLE | WIDGET_KEYABLE;
  wp->w.event         = checkbox_event;
  wp->w.tips_timeout  = 0;
  wp->w.tips_string   = NULL;
  
  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_checkbox_create (xitk_widget_list_t *wl,
				     xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cb) {
  _checkbox_private_t *wp;
  
  XITK_CHECK_CONSTITENCY(cb);

  wp = (_checkbox_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  wp->w.wl       = wl;
  wp->skin_element_name = cb->skin_element_name == NULL ? NULL : strdup (cb->skin_element_name);
  _checkbox_read_skin (wp, skonfig);
  wp->num_gfx = 3;

  return _xitk_checkbox_create (wp, cb);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_checkbox_create(xitk_widget_list_t *wl,
					   xitk_checkbox_widget_t *cb,
					   int x, int y, int width, int height) {
  static const char *noskin_names[] = {
    "XITK_NOSKIN_LEFT",
    "XITK_NOSKIN_RIGHT",
    "XITK_NOSKIN_UP",
    "XITK_NOSKIN_DOWN",
    "XITK_NOSKIN_PLUS",
    "XITK_NOSKIN_MINUS",
    "XITK_NOSKIN_CHECK"
  };
  _checkbox_private_t *wp;
  unsigned int u = sizeof (noskin_names) / sizeof (noskin_names[0]);
  xitk_image_t  *i;
  
  XITK_CHECK_CONSTITENCY(cb);

  wp = (_checkbox_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  if (cb->skin_element_name) {
    for (u = 0; u < sizeof (noskin_names) / sizeof (noskin_names[0]); u++) {
      if (!strcmp (cb->skin_element_name, noskin_names[u]))
        break;
    }
    cb->skin_element_name = NULL;
  }
  if (u == sizeof (noskin_names) / sizeof (noskin_names[0])) {
    /* default is not shared, caller may want to paint it over. */
    wp->num_gfx = 3;
    i = xitk_image_create_image (wl->xitk, width * 3, height);
    draw_checkbox_check (i);
  } else {
    wp->num_gfx = u == 6 ? 4 : 3;
    if (xitk_shared_image (wl, noskin_names[u], width * wp->num_gfx, height, &i) == 1) {
      switch (u) {
        case 0:
          draw_bevel_three_state (i);
          draw_arrow_left (i);
          break;
        case 1:
          draw_bevel_three_state (i);
          draw_arrow_right (i);
          break;
        case 2:
          draw_bevel_three_state (i);
          draw_arrow_up (i);
          break;
        case 3:
          draw_bevel_three_state (i);
          draw_arrow_down (i);
          break;
        case 4:
          draw_bevel_three_state (i);
          draw_button_plus (i);
          break;
        case 5:
          draw_bevel_three_state (i);
          draw_button_minus (i);
          break;
        case 6:
          draw_checkbox_check (i);
          break;
        default: ;
      }
    }
  }

  wp->w.wl       = wl;
  wp->skin_element_name = NULL;
  wp->skin.image = i;
  wp->skin.x        = 0;
  wp->skin.y        = 0;
  wp->skin.width    = i->width;
  wp->skin.height   = i->height;
  wp->w.x        = x;
  wp->w.y        = y;
  wp->w.visible  = 0;
  wp->w.enable   = 0;
  return _xitk_checkbox_create (wp, cb);
}
