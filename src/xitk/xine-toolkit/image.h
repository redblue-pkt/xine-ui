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

#ifndef HAVE_XITK_IMAGE_H
#define HAVE_XITK_IMAGE_H

#include <X11/Xlib.h>

#define STYLE_FLAT     1
#define STYLE_BEVEL    2

#define DIRECTION_UP     1
#define DIRECTION_DOWN   2
#define DIRECTION_LEFT   3
#define DIRECTION_RIGHT  4

#define DRAW_INNER     1
#define DRAW_OUTTER    2
#define DRAW_FLATTER   3

#define ALIGN_LEFT    1
#define ALIGN_CENTER  2
#define ALIGN_RIGHT   3
#define ALIGN_DEFAULT (ALIGN_LEFT)

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {
  ImlibData            *imlibdata;
  char                 *skin_element_name;
  xitk_widget_t        *bWidget;
  xitk_image_t         *skin;
} image_private_data_t;

/* *************************************************************** */

/*
 * Return pixel value of RGB values.
 */
unsigned int xitk_get_pixel_color_from_rgb(ImlibData *im, int r, int g, int b);

/*
 * Some presets of colors.
 */
unsigned int xitk_get_pixel_color_black(ImlibData *im);
unsigned int xitk_get_pixel_color_white(ImlibData *im);
unsigned int xitk_get_pixel_color_lightgray(ImlibData *im);
unsigned int xitk_get_pixel_color_gray(ImlibData *im);
unsigned int xitk_get_pixel_color_darkgray(ImlibData *im);
unsigned int xitk_get_pixel_color_warning_foreground(ImlibData *im);
unsigned int xitk_get_pixel_color_warning_background(ImlibData *im);

/*
 * Create an image object.
 */
xitk_image_t *xitk_image_create_image(ImlibData *im, int width, int height);
xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(ImlibData *im, 
							int width, int height, int depth);
xitk_pixmap_t *xitk_image_create_xitk_pixmap(ImlibData *im, int width, int height);
xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(ImlibData *im, int width, int height);
void xitk_image_destroy_xitk_pixmap(xitk_pixmap_t *p);

/*
 *
 */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
					 xitk_image_widget_t *im, 
					 xitk_image_t *image, int x, int y);

/*
 * Free an image object.
 */
void xitk_image_free_image(ImlibData *im, xitk_image_t **src);

/*
 * Change image object from src.
 */
void xitk_image_change_image(ImlibData *im, 
			     xitk_image_t *src, xitk_image_t *dest, int width, int height);

/*
 * Create a pixmap.
 */
Pixmap xitk_image_create_pixmap(ImlibData *idata, int width, int height);

/*
 * Add a mask to image object.
 */
void xitk_image_add_mask(ImlibData *im, xitk_image_t *dest);

/**
 * Load image and return a xitk_image_t data type.
 */
xitk_image_t *xitk_image_load_image(ImlibData *idata, char *image);

/**
 * Create an image widget type.
 */
xitk_widget_t *xitk_image_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_image_widget_t *im);

/*
 * Create and image object from a string, width is limited.
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(ImlibData *im, 
							      char *fontname, 
							      int width, int align, char *str,
							      unsigned int foreground,
							      unsigned int background);
xitk_image_t *xitk_image_create_image_from_string(ImlibData *im, 
						  char *fontname, 
						  int width, int align, char *str);

/*
 * Draw in image object a three state pixmap (first state is flat styled).
 */
void draw_flat_three_state(ImlibData *im, xitk_image_t *p);

/*
 * Draw in image object a three state pixmap.
 */
void draw_bevel_three_state(ImlibData *im, xitk_image_t *p);

/*
 * Draw in image object a two state pixmap.
 */
void draw_bevel_two_state(ImlibData *im, xitk_image_t *p);

/*
 * Drawing some paddle (for slider).
 */
void draw_paddle_bevel_three_state(ImlibData *im, xitk_image_t *p);
void draw_paddle_three_state_vertical(ImlibData *im, xitk_image_t *p);
void draw_paddle_three_state_horizontal(ImlibData *im, xitk_image_t *p);

/*
 * Draw an inner box.
 */
void draw_inner(ImlibData *im, xitk_pixmap_t *p, int w, int h);
void draw_inner_light(ImlibData *im, xitk_pixmap_t *p, int w, int h);

/*
 * Draw and outter box.
 */
void draw_outter(ImlibData *im, xitk_pixmap_t *p, int w, int h);
void draw_outter_light(ImlibData *im, xitk_pixmap_t *p, int w, int h);

/*
 * Draw a flat box.
 */
void draw_flat(ImlibData *im, xitk_pixmap_t *p, int w, int h);

/*
 * Draw a flat box with specified color.
 */
void draw_flat_with_color(ImlibData *im, xitk_pixmap_t *p, int w, int h, unsigned int color);

/*
 * Draw and arrow (direction is UP).
 */
void draw_arrow_up(ImlibData *im, xitk_image_t *p);

/*
 * Draw and arrow (direction is DOWN).
 */
void draw_arrow_down(ImlibData *im, xitk_image_t *p);

/*
 * Draw and arrow (direction is LEFT).
 */
void draw_arrow_left(ImlibData *im, xitk_image_t *p);

/*
 * Draw and arrow (direction is RIGHT).
 */
void draw_arrow_right(ImlibData *im, xitk_image_t *p);

/*
 * Draw a rectangular inner box (only the relief effect).
 */
void draw_rectangular_inner_box(ImlibData *im, xitk_pixmap_t *p, 
				int x, int y, int width, int height);

/*
 * Draw a rectangular outter box (only the relief effect).
 */
void draw_rectangular_outter_box(ImlibData *im, xitk_pixmap_t *p, 
				 int x, int y, int width, int height);

/*
 *
 */
void draw_inner_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		      int x, int y, int w, int h);
void draw_outter_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		       int x, int y, int w, int h);

void draw_tab(ImlibData *im, xitk_image_t *p);

void draw_paddle_rotate(ImlibData *im, xitk_image_t *p);
void draw_rotate_button(ImlibData *im, xitk_image_t *p);

void draw_button_plus(ImlibData *im, xitk_image_t *p);
void draw_button_minus(ImlibData *im, xitk_image_t *p);
#endif
