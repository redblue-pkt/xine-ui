/* 
 * Copyright (C) 2000-2002 the xine project
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

#ifndef HAVE_XITK_BUTTON_H
#define HAVE_XITK_BUTTON_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {
  ImlibData              *imlibdata;
  char                   *skin_element_name;
  xitk_widget_t          *bWidget;
  int                     bClicked;
  int                     focus;
  xitk_image_t            *skin;

  /* callback function (active_widget, user_data) */
  xitk_simple_callback_t  callback;
  void                   *userdata;

} button_private_data_t;

/* ********************************************************** */

/**
 * Create a button
 */
xitk_widget_t *xitk_button_create (xitk_skin_config_t *skonfig, xitk_button_widget_t *b);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_button_create (xitk_button_widget_t *b,
					  int x, int y, int width, int height);
#endif
