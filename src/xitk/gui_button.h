/* 
 * Copyright (C) 2000 the xine project
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

#ifndef HAVE_GUI_BUTTON_H
#define HAVE_GUI_BUTTON_H

#include "gui_main.h"
#include "gui_widget.h"

widget_t *create_button (int x, int y, void* f, void* ud, const char *skin) ;

typedef struct button_private_data_s {
  widget_t    *bWidget;
  int          bClicked;
  int          bArmed;
  gui_image_t *skin;

  /* callback function (active_widget, user_data) */
  void         (*function) (widget_t *, void *);
  void         *user_data;

} button_private_data_t;

#endif
