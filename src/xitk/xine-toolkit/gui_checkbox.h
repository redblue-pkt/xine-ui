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
#ifndef HAVE_GUI_CHECKBOX_H
#define HAVE_GUI_CHECKBOX_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "gui_widget.h"

typedef struct {
  Display     *display;
  widget_t    *cWidget;
  int          cClicked;
  int          cArmed;
  int          cState;
  gui_image_t *skin;

  /* callback function (active_widget, user_data, check state) */
  void         (*function) (widget_t *, void *, int);
  void         *user_data;

} checkbox_private_data_t;

/* ****************************************************************** */

/**
 * Create a checkbox.
 */
widget_t *create_checkbox (Display *display, ImlibData *idata,
			   int x, int y, void* f, void* ud, const char *skin) ;
/**
 * get state of checkbox "widget".
 */
int checkbox_get_state(widget_t *);
/**
 * Set state of checkbox .
 */
void checkbox_set_state(widget_t *, int, Window, GC);

#endif
