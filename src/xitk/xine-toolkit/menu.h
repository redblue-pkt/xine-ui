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
#ifndef HAVE_XITK_MENU_H
#define HAVE_XITK_MENU_H

#include "dlist.h"

typedef struct xitk_menu_entry_s xitk_menu_entry_t;
typedef void (*xitk_menu_callback_t)(xitk_widget_t *, xitk_menu_entry_t *, void *);

struct xitk_menu_entry_s {
  char                             *menu;
  char                             *shortcut; /* displayed (can be NULL) */
  char                             *type;     /* NULL, <separator>, <branch>, <check>, <checked> */
  xitk_menu_callback_t              cb;
  void                             *user_data;
  int                               user_id;
};

typedef struct {
  int                              magic;
  const char                      *skin_element_name;
  xitk_menu_entry_t               *menu_tree; /* NULL terminated */

} xitk_menu_widget_t;

xitk_widget_t *xitk_noskin_menu_create (xitk_widget_list_t *wl, xitk_menu_widget_t *m, int x, int y);
void xitk_menu_show_menu (xitk_widget_t *w);
void xitk_menu_add_entry (xitk_widget_t *w, xitk_menu_entry_t *me);
xitk_widget_t *xitk_menu_get_menu (xitk_widget_t *w);
void xitk_menu_destroy_sub_branchs (xitk_widget_t *w);
void xitk_menu_destroy_branch (xitk_widget_t *w);
int xitk_menu_show_sub_branchs (xitk_widget_t *w);

#endif
