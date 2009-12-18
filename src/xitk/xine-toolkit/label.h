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
 */

#ifndef HAVE_XITK_LABEL_H
#define HAVE_XITK_LABEL_H

#include "_xitk.h"

typedef struct {
  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *lWidget;

  int                     char_length; /* length of 1 char */
  int                     char_height; /* height of 1 char */

  xitk_pixmap_t          *labelpix;

  int                     length;      /* length in char */
  xitk_image_t           *font;
  char                   *fontname;
  char                   *label;

  xitk_simple_callback_t  callback;
  void                   *userdata;

  int                     animation;
  int                     anim_step;
  int                     anim_timer;
  int                     anim_running;
  int                     anim_offset;

  int                     label_visible;

  pthread_t               thread;
  pthread_mutex_t         paint_mutex;
  pthread_mutex_t         change_mutex;

} label_private_data_t;

#endif

