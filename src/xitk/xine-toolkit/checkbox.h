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
#ifndef HAVE_XITK_CHECKBOX_H
#define HAVE_XITK_CHECKBOX_H

typedef struct {
  int                               magic;
  xitk_state_callback_t             callback;
  void                             *userdata;
  const char                       *skin_element_name;
} xitk_checkbox_widget_t;

/** Create a checkbox. */
xitk_widget_t *xitk_checkbox_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cp);
/** Same as above, without skinable feature. */
xitk_widget_t *xitk_noskin_checkbox_create (xitk_widget_list_t *wl,
    xitk_checkbox_widget_t *cb, int x, int y, int width, int height);
/** get state of checkbox "widget". */
int xitk_checkbox_get_state (xitk_widget_t *w);
/** Set state of checkbox. */
void xitk_checkbox_set_state (xitk_widget_t *w, int state);

#endif
