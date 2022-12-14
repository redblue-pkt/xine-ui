/*
 * Copyright (C) 2000-2021 the xine project
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

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
} xitk_image_widget_t;

/** Create an image widget type. */
xitk_widget_t *xitk_image_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_image_widget_t *im);

/** Same as above, without skin. */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
  const xitk_image_widget_t *im, xitk_image_t *image, int x, int y);

#endif
