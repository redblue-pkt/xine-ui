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
#ifndef HAVE_XITK_FONT_H
#define HAVE_XITK_FONT_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"

/*
 *
 */
xitk_font_t *xitk_font_load_font(Display *display, char *font);

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs);

/*
 *
 */
Font xitk_font_get_font_id(xitk_font_t *xtfs);

/*
 *
 */
int xitk_font_is_font_8(xitk_font_t *xtfs);

/*
 *
 */
int xitk_font_is_font_16(xitk_font_t *xtfs);

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int length);

/*
 *
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c);

/*
 *
 */
int xitk_font_get_char_width(xitk_font_t *xtfs, unsigned char *c);

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int length);

/*
 *
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c);

/*
 *
 */
int xitk_font_get_char_height(xitk_font_t *xtfs, unsigned char *c);

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int length,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			    int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc);

#endif
