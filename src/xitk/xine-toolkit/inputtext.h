/* 
 * Copyright (C) 2000-2001 the xine project
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

#ifndef HAVE_GUI_INPUTTEXT_H
#define HAVE_GUI_INPUTTEXT_H

#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"

#include "_xitk.h"

typedef struct {
  Display                *display;

  widget_t               *iWidget;

  gui_image_t            *skin;

  xitk_string_callback_t callback;
  void                   *userdata;

  char                   *text;
  const char             *fontname;
  const char             *normal_color;
  const char             *focused_color;

  int                     have_focus;
  int                     max_length;
  int                     max_visible;
  int                     disp_offset;
  int                     cursor_pos;

} inputtext_private_data_t;

/* ***************************************************************** */

widget_t *inputtext_create(xitk_inputtext_t *it);
char *inputttext_get_text(widget_t *it);
void inputtext_change_text(widget_list_t *wl, widget_t *it, const char *text);

#endif
