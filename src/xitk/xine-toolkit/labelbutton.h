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

#ifndef HAVE_XITK_LABELBUTTON_H
#define HAVE_XITK_LABELBUTTON_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
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

} lbutton_private_data_t;

/* ***************************************************************** */

xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
					       xitk_labelbutton_widget_t *b,
					       int x, int y, int width, int height,
					       char *ncolor, char *fcolor, char *ccolor, 
					       char *fname);
/**
  * Create a labeled button.
  */
xitk_widget_t *xitk_labelbutton_create (xitk_widget_list_t *wl,
					xitk_skin_config_t *skonfig, xitk_labelbutton_widget_t *b);

/**
 * Change label of button 'widget'.
 */
int xitk_labelbutton_change_label(xitk_widget_list_t *wl, xitk_widget_t *, char *);

/**
 * Return label of button 'widget'.
 */
char *xitk_labelbutton_get_label(xitk_widget_t *);

/**
 * Get state of button 'widget'.
 */
int xitk_labelbutton_get_state(xitk_widget_t *);

/**
 * Set state of button "widget'.
 */
void xitk_labelbutton_set_state(xitk_widget_t *, int, Window, GC);

/*
 * Return used font name
 */
char *xitk_labelbutton_get_fontname(xitk_widget_t *);

/**
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment(xitk_widget_t *, int);

/**
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment(xitk_widget_t *);

/**
 *
 */
void xitk_labelbutton_set_label_offset(xitk_widget_t *, int);
int xitk_labelbutton_get_label_offset(xitk_widget_t *);

#endif
