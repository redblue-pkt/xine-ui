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

#ifndef HAVE_GUI_LBUTTON_H
#define HAVE_GUI_LBUTTON_H

#include "gui_main.h"
#include "gui_widget.h"

#define CLICK_BUTTON 1
#define RADIO_BUTTON 2

widget_t *create_label_button (int x, int y, int btype, const char *label, 
			       void* f, void* ud, const char *skin,
 			       const char *normcolor, const char *focuscolor, 
			       const char *clickcolor);
int labelbutton_change_label(widget_list_t *wl, widget_t *, const char *);
const char *labelbutton_get_label(widget_t *);
int labelbutton_get_state(widget_t *);
void labelbutton_set_state(widget_t *, int, Window, GC);


typedef struct lbutton_private_data_s {
  widget_t    *bWidget;
  int          bType;
  int          bClicked;
  int          bArmed;
  int          bState;
  int          bOldState;
  gui_image_t *skin;

  /* callback functions (active_widget, user_data [, state]) */
  void         (*function) (widget_t *, void *);
  void         (*rbfunction) (widget_t *, void *, int);
  void         *user_data;

  const char   *label;
  const char   *normcolor;
  const char   *focuscolor;
  const char   *clickcolor;

} lbutton_private_data_t;

#endif
