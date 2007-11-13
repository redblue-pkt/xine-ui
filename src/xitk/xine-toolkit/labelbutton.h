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

#ifndef HAVE_XITK_LABELBUTTON_H
#define HAVE_XITK_LABELBUTTON_H

#include "_xitk.h"

#define CLICK_BUTTON 1
#define RADIO_BUTTON 2

typedef struct {

  ImlibData		 *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *bWidget;
  int                     bType;
  int                     bClicked;

  int                     focus;

  int                     bState;
  int                     bOldState;
  xitk_image_t           *skin;

  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
   
  void                   *userdata;
   
  int                     align;
  int                     label_offset;
  int                     label_visible;
  int                     label_static;

  char                   *label;
  char                   *normcolor;
  char                   *focuscolor;
  char                   *clickcolor;
  char                   *fontname;
  
  /* Only used if (w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER || WIDGET_GROUP_MENU */
  char                   *shortcut_label;
  char                   *shortcut_font;
  int                     shortcut_pos;

} lbutton_private_data_t;

#endif
