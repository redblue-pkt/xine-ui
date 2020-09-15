/* 
 * Copyright (C) 2000-2020 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */

#ifndef HAVE_XITK_IMAGE_H
#define HAVE_XITK_IMAGE_H

#include "_xitk.h"

#define STYLE_FLAT     1
#define STYLE_BEVEL    2

#define ALIGN_LEFT    1
#define ALIGN_CENTER  2
#define ALIGN_RIGHT   3
#define ALIGN_DEFAULT (ALIGN_LEFT)

#define TABULATION_SIZE 6 /* number of chars inserted in place of a tabulation */

void draw_checkbox_check(xitk_image_t *p);
void draw_paddle_three_state (xitk_image_t *p, int width, int height);
int xitk_shared_image (xitk_widget_list_t *wl, const char *key, int width, int height, xitk_image_t **image);
void xitk_shared_image_list_delete (xitk_widget_list_t *wl);

#endif
