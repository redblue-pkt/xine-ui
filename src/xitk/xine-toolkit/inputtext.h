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

#ifndef HAVE_XITK_INPUTTEXT_H
#define HAVE_XITK_INPUTTEXT_H

#include "_xitk.h"

typedef struct {

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *iWidget;

  xitk_image_t           *skin;

  int                     cursor_focus;

  xitk_string_callback_t  callback;
  void                   *userdata;

  char                   *fontname;
  char                   *normal_color;
  char                   *focused_color;

  int                     have_focus;

  struct {
    char                 *buf;
    xitk_pixmap_t        *temp_pixmap;
    GC                    temp_gc;
    /* next 2 _without_ trailing 0. */
    int                   size;
    int                   used;
    int                   draw_start;
    int                   draw_stop;
    int                   cursor_pos;
    int                   dirty;
    int                   box_start;
    int                   box_width;
    int                   shift;
    int                   width;
  } text;

  int                     max_length;
} inputtext_private_data_t;

#endif

