/* 
 * Copyright (C) 2000-2001 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 *
 */

#ifndef __XITK_H_
#define __XITK_H_

#include <X11/Xlib.h>

#include "dnd.h"
#include "widget.h"

/* Duplicate s to d timeval values */
#define timercpy(s, d) {                                                      \
      (d)->tv_sec = (s)->tv_sec;                                              \
      (d)->tv_usec = (s)->tv_usec;                                            \
    }

#define WINDOW_INFO_ZERO(w) {                                                 \
      if((w)->name)                                                           \
	free((w)->name);                                                      \
      (w)->window = None;                                                     \
      (w)->name   = NULL;                                                     \
      (w)->x      = 0;                                                        \
      (w)->y      = 0;                                                        \
      (w)->height = 0;                                                        \
      (w)->width  = 0;                                                        \
    }

typedef struct {
  Window    window;
  char     *name;
  int       x;
  int       y;
  int       height;
  int       width;
} window_info_t;

#ifndef _XITK_C_

typedef void (*widget_cb_event_t)(XEvent *event, void *user_data);
typedef void (*widget_cb_newpos_t)(int, int, int, int);

widget_list_t *widget_list_new (void);
widgetkey_t widget_register_event_handler(char *name, Window window,
					  widget_cb_event_t cb,
					  widget_cb_newpos_t pos_cb,
					  dnd_callback_t dnd_cb,
					  widget_list_t *wl,
					  void *user_data);
void widget_unregister_event_handler(widgetkey_t *key);
int widget_get_window_info(widgetkey_t key, window_info_t *winf);

#endif
#endif

