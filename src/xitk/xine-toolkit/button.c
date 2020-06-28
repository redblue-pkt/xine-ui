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

typedef struct {
  xitk_widget_t    w;

  char            *skin_element_name;
  xitk_widget_t   *bWidget;
  int              bClicked;
  int              focus;
  xitk_part_image_t skin;

  /* callback function (active_widget, user_data) */
  xitk_simple_callback_t  callback;
  void                   *userdata;
} _button_private_t;

/*
 *
 */
static void _notify_destroy (_button_private_t *wp) {
  if (!wp->skin_element_name)
    xitk_image_free_image (&wp->skin.image);
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static xitk_image_t *_get_skin (_button_private_t *wp, int sk) {
  if ((sk == FOREGROUND_SKIN) && wp->skin.image)
    return wp->skin.image;
  return NULL;
}

/*
 *
 */
static int _notify_inside (_button_private_t *wp, int x, int y) {
  if (wp->w.visible == 1) {
    xitk_image_t *skin = wp->skin.image;

    if (skin->mask)
      return xitk_is_cursor_out_mask (&wp->w, skin->mask, x + wp->skin.x, y + wp->skin.y);
    else 
      return 1;
  }
  return 0;
}

/**
 *
 */
static void _paint_button (_button_private_t *wp, widget_event_t *event) {
  int mode;

#ifdef XITK_PAINT_DEBUG
  printf ("xitk.button.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (wp->w.visible) {
    mode = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
         ? (wp->bClicked ? 2 : 1)
         : 0;
    xitk_part_image_draw (wp->w.wl, &wp->skin, NULL,
      mode * wp->w.width + event->x - wp->w.x, event->y - wp->w.y,
      event->width, event->height,
      event->x, event->y);
  }
}

/*
 *
 */
static void _notify_change_skin (_button_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name) {
    xitk_skin_lock (skonfig);
    xitk_skin_get_part_image (skonfig, &wp->skin, xitk_skin_get_skin_filename (skonfig, wp->skin_element_name));
    wp->w.x         = xitk_skin_get_coord_x (skonfig, wp->skin_element_name);
    wp->w.y         = xitk_skin_get_coord_y (skonfig, wp->skin_element_name);
    wp->w.width     = wp->skin.width / 3;
    wp->w.height    = wp->skin.height;
    wp->w.visible   = (xitk_skin_get_visibility (skonfig, wp->skin_element_name)) ? 1 : -1;
    wp->w.enable    = xitk_skin_get_enability (skonfig, wp->skin_element_name);
    xitk_skin_unlock (skonfig);
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}

/*
 *
 */
static int _notify_click_button (_button_private_t *wp, int button, int bUp, int x, int y) {
  int ret = 0;

  (void)x;
  (void)y;
  if (button == Button1) {
    widget_event_t event;

    wp->bClicked = !bUp;
    event.x = wp->w.x;
    event.y = wp->w.y;
    event.width = wp->w.width;
    event.height = wp->w.height;
    _paint_button (wp, &event);

    if (bUp && (wp->focus == FOCUS_RECEIVED)) {
      if (wp->callback)
        wp->callback (&wp->w, wp->userdata);
    }
    ret = 1;
  }
  return ret;
}

/*
 *
 */
static int _notify_focus_button (_button_private_t *wp, int focus) {
  wp->focus = focus;
  return 1;
}

static int notify_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _button_private_t *wp = (_button_private_t *)w;
  int retval = 0;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON) {
    switch (event->type) {
      case WIDGET_EVENT_PAINT:
        event->x = wp->w.x;
        event->y = wp->w.y;
        event->width = wp->w.width;
        event->height = wp->w.height;
        /* fall through */
      case WIDGET_EVENT_PARTIAL_PAINT:
        _paint_button (wp, event);
        break;
      case WIDGET_EVENT_CLICK:
        result->value = _notify_click_button (wp, event->button,
          event->button_pressed, event->x, event->y);
        retval = 1;
        break;
      case WIDGET_EVENT_FOCUS:
        _notify_focus_button (wp, event->focus);
        break;
      case WIDGET_EVENT_INSIDE:
        result->value = _notify_inside (wp, event->x, event->y);
        retval = 1;
        break;
      case WIDGET_EVENT_CHANGE_SKIN:
        _notify_change_skin (wp, event->skonfig);
        break;
      case WIDGET_EVENT_DESTROY:
        _notify_destroy (wp);
        break;
      case WIDGET_EVENT_GET_SKIN:
        if (result) {
          result->image = _get_skin (wp, event->skin_layer);
          retval = 1;
        }
        break;
    }
  }
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_button_create (_button_private_t *wp, xitk_button_widget_t *b) {
  
  ABORT_IF_NULL (wp->w.wl);
  ABORT_IF_NULL (wp->w.wl->imlibdata);

  wp->bWidget           = &wp->w;
  wp->bClicked          = 0;
  wp->focus             = FOCUS_LOST;

  wp->callback          = b->callback;
  wp->userdata          = b->userdata;
  
  wp->w.private_data    = wp;

  wp->w.running         = 1;
  wp->w.have_focus      = FOCUS_LOST;
  wp->w.width           = wp->skin.width / 3;
  wp->w.height          = wp->skin.height;
  wp->w.type            = WIDGET_TYPE_BUTTON | WIDGET_CLICKABLE | WIDGET_FOCUSABLE
                        | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = notify_event;
  wp->w.tips_timeout    = 0;
  wp->w.tips_string     = NULL;

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_button_create (xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_button_widget_t *b) {
  _button_private_t *wp;
  
  XITK_CHECK_CONSTITENCY(b);
  wp = (_button_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  wp->w.wl = wl;
  wp->skin_element_name = b->skin_element_name ? strdup (b->skin_element_name) : NULL;
  xitk_skin_get_part_image (skonfig, &wp->skin, xitk_skin_get_skin_filename (skonfig, b->skin_element_name));
  wp->w.x       = xitk_skin_get_coord_x (skonfig, b->skin_element_name);
  wp->w.y       = xitk_skin_get_coord_y (skonfig, b->skin_element_name);
  wp->w.enable  = xitk_skin_get_enability (skonfig, b->skin_element_name);
  wp->w.visible = xitk_skin_get_visibility (skonfig, b->skin_element_name) ? 1 : -1;

  return _xitk_button_create (wp, b);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_button_create (xitk_widget_list_t *wl,
					  xitk_button_widget_t *b,
					  int x, int y, int width, int height) {
  static const char *noskin_names[] = {
    "XITK_NOSKIN_LEFT",
    "XITK_NOSKIN_RIGHT",
    "XITK_NOSKIN_UP",
    "XITK_NOSKIN_DOWN",
    "XITK_NOSKIN_PLUS",
    "XITK_NOSKIN_MINUS"
  };
  _button_private_t *wp;
  unsigned int u = sizeof (noskin_names) / sizeof (noskin_names[0]);
  xitk_image_t *i;

  XITK_CHECK_CONSTITENCY(b);
  wp = (_button_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  if (b->skin_element_name) {
    for (u = 0; u < sizeof (noskin_names) / sizeof (noskin_names[0]); u++) {
      if (!strcmp (b->skin_element_name, noskin_names[u]))
        break;
    }
    b->skin_element_name = NULL;
  }
  if (u == sizeof (noskin_names) / sizeof (noskin_names[0])) {
    i = xitk_image_create_image (wl->xitk, width * 3, height);
    draw_bevel_three_state (i);
  } else {
    if (xitk_shared_image (wl, noskin_names[u], width * 3, height, &i) == 1) {
      draw_bevel_three_state (i);
      switch (u) {
        case 0:
          draw_arrow_left (i);
          break;
        case 1:
          draw_arrow_right (i);
          break;
        case 2:
          draw_arrow_up (i);
          break;
        case 3:
          draw_arrow_down (i);
          break;
        case 4:
          draw_button_plus (i);
          break;
        case 5:
          draw_button_minus (i);
          break;
        default: ;
      }
    }
  }

  wp->w.wl = wl;
  wp->w.x = x;
  wp->w.y = y;
  wp->skin_element_name = b->skin_element_name ? strdup (b->skin_element_name) : NULL;
  wp->skin.image = i;
  wp->skin.x        = 0;
  wp->skin.y        = 0;
  wp->skin.width    = i->width;
  wp->skin.height   = i->height;
  wp->w.enable = 0;
  wp->w.visible = 0;

  return _xitk_button_create (wp, b);
}
