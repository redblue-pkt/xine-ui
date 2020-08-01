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

#ifndef HAVE_XITK_LABEL_H
#define HAVE_XITK_LABEL_H

typedef struct {
  int                     magic;
  const char             *label;
  const char             *skin_element_name;
  xitk_simple_callback_t  callback;
  void                   *userdata;
} xitk_label_widget_t;

/** * Create a label widget. */
xitk_widget_t *xitk_label_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_label_widget_t *l);
/** */
xitk_widget_t *xitk_noskin_label_create (xitk_widget_list_t *wl,
  const xitk_label_widget_t *l, int x, int y, int width, int height,
  const char *font_name);
/** Change label of widget 'widget'. */
int xitk_label_change_label (xitk_widget_t *w, const char *newlabel);
/** Get label. */
const char *xitk_label_get_label (xitk_widget_t *w);

#endif

