/* 
 * Copyright (C) 2000-2019 the xine project
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
  char                   skin_element_name[64];

  xitk_widget_t          *lWidget;

  xitk_pixmap_t          *labelpix;

  xitk_pix_font_t        *pix_font;

  int                     length;      /* length in char */
  xitk_image_t           *font;
  char                   *fontname;
  char                   *label;
  size_t                  label_len;

  xitk_simple_callback_t  callback;
  void                   *userdata;

  int                     animation;
  int                     anim_step;
  int                     anim_timer;
  int                     anim_offset;

  int                     label_visible;

  int                     anim_running;
  pthread_t               anim_thread;
  pthread_mutex_t         change_mutex;
} label_private_data_t;

#endif

