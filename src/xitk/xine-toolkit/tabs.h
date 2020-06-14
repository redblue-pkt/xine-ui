/* 
 * Copyright (C) 2000-2009 the xine project
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

#include "_xitk.h"

#define MAX_TABS 256

typedef struct {
  char                   *skin_element_name;
  xitk_widget_t          *widget;
  xitk_widget_t          *left;
  xitk_widget_t          *right;

  int                     x, y, width;

  int                     num_entries;
  char                  **entries;

  xitk_widget_t          *tabs[MAX_TABS];
  btnlist_t              *bt[MAX_TABS];

  int                     selected;
  int                     old_selected;

  int                     offset;
  int                     old_offset;
  int                     bheight;
  int                     gap_widthstart;

  xitk_state_callback_t  callback;
  void                   *userdata;

} tabs_private_data_t;

#endif
