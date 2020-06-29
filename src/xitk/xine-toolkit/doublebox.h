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
#ifndef HAVE_XITK_DOUBLEBOX_H
#define HAVE_XITK_DOUBLEBOX_H

#include "_xitk.h"

typedef struct {
  char                           *skin_element_name;

  xitk_widget_t                  *input_widget;
  xitk_widget_t                  *more_widget;
  xitk_widget_t                  *less_widget;

  double                          step;
  double                          value;
  int                             force_value;

  xitk_state_double_callback_t    callback;
  void                           *userdata;

} doublebox_private_data_t;

#endif
