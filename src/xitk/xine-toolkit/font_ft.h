
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

#ifndef _HAVE_XITK_FONT_FT_H
#define _HAVE_XITK_FONT_FT_H

#include <stdint.h>
#include <stddef.h>

typedef struct xitk_ft_font_s xitk_ft_font_t;
typedef struct xitk_ft_font_factory_s xitk_ft_font_factory_t;

xitk_ft_font_factory_t *xitk_ft_font_factory_create(void);
void xitk_ft_font_factory_destroy(xitk_ft_font_factory_t **);

xitk_ft_font_t *xitk_ft_font_create(xitk_ft_font_factory_t *, const char *font);
void xitk_ft_font_destroy(xitk_ft_font_t **);

void xitk_ft_font_text_extent(xitk_ft_font_t *, const char *c, size_t nbytes,
                               int *lbearing, int *rbearing, int *width, int *ascent, int *descent);
void xitk_ft_font_draw_string(xitk_ft_font_t *, uint32_t *dst,
                              int x, int y, int stride, int max_h,
                              const char *text, size_t nbytes, uint32_t color);

#endif /* _HAVE_XITK_FONT_FT_H */
