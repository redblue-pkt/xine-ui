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
#ifndef HAVE_XITK_TABS_H
#define HAVE_XITK_TABS_H

#define MAX_TABS 256

typedef struct {
  int                              magic;
  const char                      *skin_element_name;
  int                              num_entries;
  char                           **entries;
  xitk_state_callback_t            callback;
  void                            *userdata;
} xitk_tabs_widget_t;

/** */
xitk_widget_t *xitk_noskin_tabs_create (xitk_widget_list_t *wl,
  xitk_tabs_widget_t *t, int x, int y, int width, const char *font_name);
/** */
int xitk_tabs_get_current_selected (xitk_widget_t *w);
/** */
const char *xitk_tabs_get_current_tab_selected (xitk_widget_t *w);
/** */
void xitk_tabs_set_current_selected (xitk_widget_t *w, int select);

#endif

