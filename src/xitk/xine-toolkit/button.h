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

#ifndef HAVE_XITK_BUTTON_H
#define HAVE_XITK_BUTTON_H

typedef struct {
  int                     magic;
  const char             *skin_element_name; /** << supports special names "XITK_NOSKIN_{LEFT|RIGHT|UP|DOWN|PLUS|MINUS|CHECK}". */
  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback; /** << if set, enables checkbox behaviour. */
  void                   *userdata;
} xitk_button_widget_t;

/** Create a button */
xitk_widget_t *xitk_button_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig, const xitk_button_widget_t *b);
xitk_widget_t *xitk_noskin_button_create (xitk_widget_list_t *wl, const xitk_button_widget_t *b,
  int x, int y, int width, int height);
/** get/set on/off state */
int xitk_button_get_state (xitk_widget_t *w);
int xitk_button_set_state (xitk_widget_t *w, int state);

#endif
