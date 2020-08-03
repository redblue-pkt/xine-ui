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
#ifndef HAVE_XITK_DOUBLEBOX_H
#define HAVE_XITK_DOUBLEBOX_H

typedef struct {
  int                              magic;

  const char                      *skin_element_name;
  
  double                           value;
  double                           step;

  xitk_state_double_callback_t      callback;
  void                             *userdata;
} xitk_doublebox_widget_t;

xitk_widget_t *xitk_noskin_doublebox_create (xitk_widget_list_t *wl,
  xitk_doublebox_widget_t *ib, int x, int y, int width, int height);
void xitk_doublebox_set_value (xitk_widget_t *w, double value);
double xitk_doublebox_get_value (xitk_widget_t *w);

#endif
