/* 
 * Copyright (C) 2000-2004 the xine project
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
 * $Id$
 *
 */

#ifndef HAVE_XITK_BROWSER_H
#define HAVE_XITK_BROWSER_H

#include "_xitk.h"

#define BROWSER_MAX_ENTRIES 65535

typedef struct {

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *bWidget;
  xitk_widget_t          *item_tree[BROWSER_MAX_ENTRIES];

  btnlist_t              *bt[BROWSER_MAX_ENTRIES];

  xitk_widget_list_t     *parent_wlist;

  xitk_skin_config_t     *skonfig;

  int                     jumped;

  char                  **content;
  char                  **shortcuts_content;
  int                     max_length;
  int                     list_length;
  int                     current_start;

  xitk_state_callback_t   callback;
  void                   *userdata;

  int                     dbl_click_time;
  xitk_state_callback_t   dbl_click_callback;

  int                     last_button_clicked;
  int                     current_button_clicked;
  struct timeval          click_time;

  int                     need_h_slider;
  int                     labels_offset;

} browser_private_data_t;

#endif
