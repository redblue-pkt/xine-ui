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
#ifndef HAVE_XITK_INTBOX_H
#define HAVE_XITK_INTBOX_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {
  ImlibData               *imlibdata;
  char                    *skin_element_name;

  xitk_widget_t           *input_widget;
  xitk_widget_t           *more_widget;
  xitk_widget_t           *less_widget;

  int                      step;
  int                      value;
  int                      force_value;

  xitk_widget_list_t      *parent_wlist;

  xitk_state_callback_t    callback;
  void                    *userdata;

} intbox_private_data_t;

/*
 * Create a combo box.
 */
/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_intbox_create(xitk_intbox_widget_t *ib,
					 int x, int y, int width, int height, 
					 xitk_widget_t **iw, xitk_widget_t **mw, xitk_widget_t **lw);

void xitk_intbox_set_value(xitk_widget_t *, int);
int xitk_intbox_get_value(xitk_widget_t *);

#endif
