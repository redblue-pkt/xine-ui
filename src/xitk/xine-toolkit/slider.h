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

#ifndef HAVE_XITK_SLIDER_H
#define HAVE_XITK_SLIDER_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

/*  vertical slider type */
#define XITK_VSLIDER 1
/*  horizontal slider type */
#define XITK_HSLIDER 2
/*  rotate button *slider* type */
#define XITK_RSLIDER 3

typedef struct {
  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *sWidget;
  int                     sType;
  int                     bClicked;
  int                     bArmed;


  float                   angle;
  float                   percentage;

  float                   upper;
  float                   value;
  float                   lower;
  int                     step;

  int                     radius;

  xitk_image_t           *paddle_skin;
  xitk_image_t           *bg_skin;

  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
			  
  xitk_state_callback_t   callback;
  void                   *userdata;

  int                     button_width;
  float                   ratio;

  int                     paddle_cover_bg;

} slider_private_data_t;

/* *************************************************************** */

/**
 * Create a slider
 */
xitk_widget_t *xitk_slider_create(xitk_skin_config_t *skonfig, xitk_slider_widget_t *s);

/**
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_slider_create (xitk_slider_widget_t *s,
					  int x, int y, int width, int height, int type);
/**
 * Get current position of paddle.
 */
int xitk_slider_get_pos(xitk_widget_t *);

/**
 * Set position of paddle.
 */
void xitk_slider_set_pos(xitk_widget_list_t *, xitk_widget_t *, int);

/**
 * Set min value of slider.
 */
void xitk_slider_set_min(xitk_widget_t *, int);

/**
 * Set max value of slider.
 */
void xitk_slider_set_max(xitk_widget_t *, int);

/**
 * Get min value of slider.
 */
int xitk_slider_get_min(xitk_widget_t *);

/**
 * Get max value of slider.
 */
int xitk_slider_get_max(xitk_widget_t *);

/**
 * Set position to 0 and redraw the widget.
 */
void xitk_slider_reset(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Set position to max and redraw the widget.
 */
void xitk_slider_set_to_max(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Increment by step the paddle position
 */
void xitk_slider_make_step(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Decrement by step the paddle position.
 */
void xitk_slider_make_backstep(xitk_widget_list_t *, xitk_widget_t *);

#endif
