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
#ifndef HAVE_XITK_MENU_H
#define HAVE_XITK_MENU_H

#include "dlist.h"

typedef struct xitk_menu_entry_s xitk_menu_entry_t;
typedef void (*xitk_menu_callback_t)(xitk_widget_t *, xitk_menu_entry_t *, void *);
typedef enum {
  XITK_MENU_ENTRY_END = 0,
  XITK_MENU_ENTRY_PLAIN,
  XITK_MENU_ENTRY_SEPARATOR,
  XITK_MENU_ENTRY_BRANCH,
  XITK_MENU_ENTRY_CHECK,
  XITK_MENU_ENTRY_CHECKED,
  XITK_MENU_ENTRY_TITLE,
  XITK_MENU_ENTRY_LAST
} xitk_menu_entry_type_t;

struct xitk_menu_entry_s {
  xitk_menu_entry_type_t type;
  int                    user_id;  /** << callback private */
  const char            *menu;
  const char            *shortcut; /** << displayed (can be NULL) */
};

typedef struct {
  int                      magic;
  const char              *skin_element_name;
  const xitk_menu_entry_t *menu_tree; /** << terminated by type == XITK_MENU_ENTRY_END */
  xitk_menu_callback_t     cb;
  void                    *user_data;
} xitk_menu_widget_t;

xitk_widget_t *xitk_noskin_menu_create (xitk_widget_list_t *wl, xitk_menu_widget_t *m, int x, int y);
void xitk_menu_show_menu (xitk_widget_t *w);
void xitk_menu_add_entry (xitk_widget_t *w, const xitk_menu_entry_t *me);

#endif
