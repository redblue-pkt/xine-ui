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
#ifndef HAVE_XITK_CHECKBOX_H
#define HAVE_XITK_CHECKBOX_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {
  Display               *display;
  ImlibData             *imlibdata;
  char                  *skin_element_name;
  xitk_widget_t         *cWidget;
  int                    cClicked;
  int                    cArmed;
  int                    cState;
  xitk_image_t           *skin;

  xitk_state_callback_t  callback;
  void                  *userdata;

} checkbox_private_data_t;

/* ****************************************************************** */

/**
 * Create a checkbox.
 */
xitk_widget_t *xitk_checkbox_create (xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cb);

/**
 * get state of checkbox "widget".
 */
int xitk_checkbox_get_state(xitk_widget_t *);

/**
 * Set state of checkbox .
 */
void xitk_checkbox_set_state(xitk_widget_t *, int, Window, GC);

#endif
