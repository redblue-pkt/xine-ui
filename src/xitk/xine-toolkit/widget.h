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

#ifndef HAVE_XITK_WIDGET_H
#define HAVE_XITK_WIDGET_H

#include "dlist.h"

#include <xine/sorted_array.h>

#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1

typedef struct {
  enum {
    WIDGET_EVENT_NONE = 0,
    WIDGET_EVENT_PAINT,        /** << check for state change, paint specified portion, or whole widget if that is not supported */
    WIDGET_EVENT_CLICK,        /** << return 0 (not hit), 1 (hit) */
    WIDGET_EVENT_KEY,          /** << return 0 (pass again to further candidates), 1 (handled finally here) */
    WIDGET_EVENT_INSIDE,       /** << return 0 (unsupported??), 1 (yes), 2 (no) */
    WIDGET_EVENT_CHANGE_SKIN,
    WIDGET_EVENT_ENABLE,
    WIDGET_EVENT_GET_SKIN,     /** << return 0 (failed), 1 (result.image filled in) */
    WIDGET_EVENT_DESTROY,
    WIDGET_EVENT_TIPS_TIMEOUT,
    WIDGET_EVENT_CLIP_READY
  }                     type;

  int                   x, y;            /** << PAINT, CLICK, INSIDE */
  int                   width, height;   /** << PAINT */

  int                   button_pressed;  /** << CLICK, KEY */
  int                   button;          /** << CLICK */

  int                   modifier;        /** << CLICK, KEY */

  unsigned long         tips_timeout;    /** << TIPS_TIMEOUT */

  const char           *string;          /** << KEY */

  xitk_skin_config_t   *skonfig;         /** << CHANGE_SKIN */

  enum {
    FOREGROUND_SKIN = 1,
    BACKGROUND_SKIN
  }                     skin_layer;      /** << GET_SKIN */
} widget_event_t;

typedef struct {
  int                   value;
  xitk_image_t         *image;
} widget_event_result_t;

typedef struct xitk_widget_rel_s {
  xitk_dnode_t node;
  xitk_dlist_t list;
  struct xitk_widget_rel_s *group;
  xitk_widget_t *w;
} xitk_widget_rel_t;

/* return 1 if event_result is filled, otherwise 0 */
typedef int (*widget_event_notify_t)(xitk_widget_t *, widget_event_t *, widget_event_result_t *);

struct xitk_widget_s {
  xitk_dnode_t           node;

  xitk_widget_list_t    *wl;
  xitk_widget_rel_t      parent, focus_redirect;

  int                    x;
  int                    y;
  int                    width; /** << 0 if not used by current skin */
  int                    height;

  uint32_t               type;

  /* XITK_WIDGET_STATE_* */
#define XITK_WIDGET_STATE_MOUSE 0x10000
#define XITK_WIDGET_STATE_FOCUS 0x20000
  uint32_t               state;
  /* xitk_show_widgets () restores the state before xitk_hide_widgets (). */
  uint32_t               saved_state;
  /* detect state changes, and avoid double paint. */
#define XITK_WIDGET_STATE_UNSET 0x80000000
  uint32_t               shown_state;

  widget_event_notify_t  event;

  unsigned long          tips_timeout;
  char                  *tips_string;

  void                  *private_data;
};

struct xitk_widget_list_s {
  xitk_dnode_t    node;

  xitk_t         *xitk;
  xitk_dlist_t    list;
  xitk_widget_t  *widget_focused, *widget_under_mouse, *widget_pressed;
  struct {
    int           x, y;
  }               mouse;
  unsigned int    qual;
  xine_sarray_t  *shared_images;
  xitk_window_t  *xwin;
};

xitk_widget_t *xitk_widget_new (xitk_widget_list_t *wl, size_t size);
void xitk_widget_set_parent (xitk_widget_t *w, xitk_widget_t *parent);
void xitk_destroy_widgets (xitk_widget_list_t *wl);

xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y);
#ifdef YET_UNUSED
int xitk_is_widget_focused (xitk_widget_t *w);
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y);
xitk_widget_t *xitk_get_pressed_widget (xitk_widget_list_t *);
xitk_image_t *xitk_get_widget_background_skin (xitk_widget_t *w);
#endif

/**
 *
 */
void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward, int modifier);

void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y, unsigned int state);
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp, int modifier);

#endif
