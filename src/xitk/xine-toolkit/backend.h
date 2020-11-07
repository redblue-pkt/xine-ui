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
 * Public part of xiTK backend.
 */

#ifndef _XITK_H_
#  error include xitk.h before backend.h!
#endif

#ifndef XITK_BACKEND_H
#define XITK_BACKEND_H

#include <stdint.h>

#define XITK_CTRL_KEY_PREFIX 1
typedef enum {
  XITK_KEY_ESCAPE = 1,
  XITK_KEY_RETURN,
  XITK_KEY_NUMPAD_ENTER,
  XITK_KEY_ISO_ENTER,
  XITK_KEY_LEFT,
  XITK_KEY_RIGHT,
  XITK_KEY_UP,
  XITK_KEY_DOWN,
  XITK_KEY_HOME,
  XITK_KEY_END,
  XITK_KEY_PAGE_UP,
  XITK_KEY_PAGE_DOWN,
  XITK_KEY_TAB,
  XITK_KEY_KP_TAB,
  XITK_KEY_ISO_LEFT_TAB,
  XITK_KEY_INSERT,
  XITK_KEY_DELETE,
  XITK_KEY_BACKSPACE,
  XITK_KEY_PRINT,
  XITK_KEY_ROLL,
  XITK_KEY_PAUSE,
  XITK_KEY_F1,
  XITK_KEY_F2,
  XITK_KEY_F3,
  XITK_KEY_F4,
  XITK_KEY_F5,
  XITK_KEY_F6,
  XITK_KEY_F7,
  XITK_KEY_F8,
  XITK_KEY_F9,
  XITK_KEY_F10,
  XITK_KEY_F11,
  XITK_KEY_F12,
  XITK_KEY_PREV,
  XITK_KEY_NEXT,
  XITK_KEY_ABORT,
  XITK_KEY_MENU,
  XITK_KEY_HELP,

  XITK_MOUSE_WHEEL_UP,
  XITK_MOUSE_WHEEL_DOWN,

  XITK_KEY_LASTCODE
} xitk_ctrl_key_t;

typedef struct xitk_be_window_s xitk_be_window_t;

typedef enum {
  XITK_EV_ANY = 0,
  XITK_EV_NEW_WIN,
  XITK_EV_DEL_WIN, /** << code == 0 (user request), 1 (actual close). */
  XITK_EV_SHOW,
  XITK_EV_HIDE,
  XITK_EV_REPARENT,
  XITK_EV_FOCUS,
  XITK_EV_UNFOCUS,
  XITK_EV_ENTER,
  XITK_EV_LEAVE,
  XITK_EV_EXPOSE,
  XITK_EV_POS_SIZE,
  XITK_EV_MOVE,
  XITK_EV_BUTTON_DOWN,
  XITK_EV_BUTTON_UP,
  XITK_EV_KEY_DOWN,
  XITK_EV_KEY_UP,
  XITK_EV_CLIP_READY,
  XITK_EV_DND,
  XITK_EV_LAST
} xitk_be_event_type_t;

typedef struct {
  xitk_be_event_type_t type;
  uint32_t code; /** << backend internal key code or button # */
  uint32_t sym; /** << backend internal key symbol */
  uint32_t qual; /** << shift, ctrl, ... */
  uint32_t from_peer; /** sent by another client */
  uintptr_t id;  /** << backend internal value */
  xitk_be_window_t *window, *parent;
  int x, y;
  int w, h; /** << alias root window relative x, y with XITK_EV_[MOVE,BUTTON_*,KEY_*]. */
  int more; /** << utf8 byte length, or count of remaining XITK_EV_EXPOSE */
  int time; /** << milliseconds */
  const char *utf8; /** << may be XITK_CTRL_KEY_PREFIX, xitk_ctrl_key_t, 0 */
} xitk_be_event_t;

/** for use in event handler */
const char *xitk_be_event_name (const xitk_be_event_t *event);

/** return 1 if event was handled finally. */
typedef int (xitk_be_event_handler_t) (void *data, const xitk_be_event_t *event);

xitk_register_key_t xitk_be_register_event_handler (const char *name, xitk_window_t *xwin,
  xitk_widget_list_t *wl,
  xitk_be_event_handler_t *event_handler, void *eh_data,
  void (*destructor) (void *data), void *destr_data);

#endif
