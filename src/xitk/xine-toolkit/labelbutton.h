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

#ifndef HAVE_GUI_LBUTTON_H
#define HAVE_GUI_LBUTTON_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

#define CLICK_BUTTON 1
#define RADIO_BUTTON 2

typedef struct {

  Display                *display;
  widget_t               *bWidget;
  int                     bType;
  int                     bClicked;
  int                     bArmed;
  int                     bState;
  int                     bOldState;
  gui_image_t            *skin;
  ImlibData		 *imlibdata;

  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
   
  void                   *userdata;
   
  char                   *label;
  char                   *normcolor;
  char                   *focuscolor;
  char                   *clickcolor;
  char                   *fontname;

} lbutton_private_data_t;

/* ***************************************************************** */

 /**
  * Create a labeled button.
  */
widget_t *label_button_create (xitk_labelbutton_t *b);

/**
 * Change label of button 'widget'.
 */
int labelbutton_change_label(widget_list_t *wl, widget_t *, char *);

/**
 * Return label of button 'widget'.
 */
char *labelbutton_get_label(widget_t *);

/**
 * Get state of button 'widget'.
 */
int labelbutton_get_state(widget_t *);

/**
 * Set state of button "widget'.
 */
void labelbutton_set_state(widget_t *, int, Window, GC);

#endif
