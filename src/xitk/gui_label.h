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

#ifndef HAVE_GUI_LABEL_H
#define HAVE_GUI_LABEL_H

#include <X11/Xlib.h>
#include "Imlib.h"
#include "gui_widget.h"

widget_t *create_label (Display *display, ImlibData *idata,
			int x, int y, int length, const char *label, char *bg);
int label_change_label (widget_list_t *wl, widget_t *l, const char *newlabel);

typedef struct label_private_data_s {
  Display       *display;

  widget_t      *lWidget;

  int            char_length; /* length of 1 char */
  int            char_height; /* height of 1 char */

  int            length;      /* length in char */
  gui_image_t   *font;
  const char    *label;

} label_private_data_t;

#endif



