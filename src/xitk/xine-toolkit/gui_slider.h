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

#ifndef HAVE_GUI_SLIDER_H
#define HAVE_GUI_SLIDER_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "gui_widget.h"

/*  To handle a vertical slider */
#define VSLIDER 1
/*  To handle an horizontal slider */
#define HSLIDER 2

typedef struct {
  Display    *display;

  widget_t   *sWidget;
  int         sType;
  int         bClicked;
  int         bArmed;
  int 	      pos;
  int         max;
  int         step;
  int         min;
  int         realmin;
  int         realmax;

  gui_image_t *paddle_skin;
  gui_image_t *bg_skin;

  /* callback function (active_widget, user_data, current_position) */
  void        (*mfunction) (widget_t *, void *, int);
  void       *muser_data;
			  
  void        (*function) (widget_t *, void *, int);
  void       *user_data;

  int  button_width;
  float ratio;

} slider_private_data_t;

/* *************************************************************** */

/**
 * Create a slider
 */
widget_t *create_slider(Display *display, ImlibData *idata,
			int type, int x, int y, int min, int max, 
			int step, const char *bg, const char *paddle, 
/* cb for motion      */void *fm, void *udm,
/* cb for btn release */void *f, void *ud) ;

/**
 * Get current position of paddle.
 */
int slider_get_pos(widget_t *);

/**
 * Set position of paddle.
 */
void slider_set_pos(widget_list_t *, widget_t *, int);

/**
 * Set min value of slider.
 */
void slider_set_min(widget_t *, int);

/**
 * Set max value of slider.
 */
void slider_set_max(widget_t *, int);

/**
 * Get min value of slider.
 */
int slider_get_min(widget_t *);

/**
 * Get max value of slider.
 */
int slider_get_max(widget_t *);

/**
 * Set position to 0 and redraw the widget.
 */
void slider_reset(widget_list_t *, widget_t *);

/**
 * Set position to max and redraw the widget.
 */
void slider_set_to_max(widget_list_t *, widget_t *);

/**
 * Increment by step the paddle position
 */
void slider_make_step(widget_list_t *, widget_t *);

/**
 * Decrement by step the paddle position.
 */
void slider_make_backstep(widget_list_t *, widget_t *);

#endif
