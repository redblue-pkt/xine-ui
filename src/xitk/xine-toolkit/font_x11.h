
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

#ifndef _HAVE_XITK_FONT_X11_H
#define _HAVE_XITK_FONT_X11_H

#include "xitk.h"

#include <X11/Xlib.h>

typedef struct xitk_x11_font_s xitk_x11_font_t;

xitk_x11_font_t *xitk_x11_font_create(xitk_t *, Display *, const char *font);
void xitk_x11_font_destroy(Display *, xitk_x11_font_t **);

void xitk_x11_font_text_extent(Display *, xitk_x11_font_t *, const char *c, size_t nbytes,
                               int *lbearing, int *rbearing, int *width, int *ascent, int *descent);
void xitk_x11_font_draw_string(Display *, xitk_x11_font_t *, Drawable img,
                               int x, int y, const char *text, size_t nbytes, uint32_t color);


#endif /* _HAVE_XITK_FONT_X11_H */
