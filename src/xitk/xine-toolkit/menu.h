/* 
 * Copyright (C) 2000-2003 the xine project
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
 */
#ifndef HAVE_XITK_MENU_H
#define HAVE_XITK_MENU_H

#include "_xitk.h"

typedef struct {
  xitk_widget_list_t      wl;
  Display                *display;
  ImlibData              *im;
  xitk_window_t          *xwin;
  xitk_register_key_t     key;
  xitk_widget_t          *widget;
} menu_window_t;

typedef struct menu_node_s menu_node_t;
struct menu_node_s {
  menu_node_t            *prev;
  xitk_menu_entry_t      *menu_entry;
  xitk_widget_t          *widget;
  menu_window_t          *menu_window;
  menu_node_t            *branch;
  menu_node_t            *next;
};

typedef struct {
  menu_node_t            *first;
  menu_node_t            *cur;
  menu_node_t            *last;
} menu_tree_t;

typedef struct {
  ImlibData              *imlibdata;
  xitk_widget_t          *widget;
  char                   *skin_element_name;
  menu_tree_t            *mtree;
  menu_node_t            *curbranch;
  xitk_list_t            *menu_windows;
  xitk_widget_list_t     *parent_wlist;
  int                     x, y;
} menu_private_data_t;

#endif

