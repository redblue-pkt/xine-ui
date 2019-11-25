/* 
 * Copyright (C) 2000-2017 the xine project
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
#ifndef HAVE_XITK_FONT_H
#define HAVE_XITK_FONT_H

#include "_xitk.h"

#define DEFAULT_FONT_10      "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*"
#define DEFAULT_FONT_12      "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*"
#define DEFAULT_FONT_14      "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*"
#define DEFAULT_BOLD_FONT_10 "-*-helvetica-bold-r-*-*-10-*-*-*-*-*-*-*"
#define DEFAULT_BOLD_FONT_12 "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*"
#define DEFAULT_BOLD_FONT_14 "-*-helvetica-bold-r-*-*-14-*-*-*-*-*-*-*"
#define FONT_HEIGHT_MODEL    "azertyuiopqsdfghjklmwxcvbnAZERTYUIOPQSDFGHJKLMWXCVBN&�(-�_��)=�~#{[|`\\^@]}%"

/*
 * init font cache subsystem
 */
void xitk_font_cache_init(void);

/*
 * destroy font cache subsystem
 */
void xitk_font_cache_done(void);

/*
 * load font from font name. Return NULL on failure.
 */
xitk_font_t *xitk_font_load_font(Display *display, const char *font);

/*
 * Draw the string with given font.
 */
void xitk_font_draw_string(xitk_font_t *xtfs, Pixmap pix, GC gc, 
			   int x, int y, const char *text, 
			   size_t nbytes);

/*
 * Unload (free memory) font.
 */
void xitk_font_unload_font(xitk_font_t *xtfs);

/*
 * Return width (in pixel) of a string with length length.
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int length);

/*
 * Return the string length (in pixel).
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c);

/*
 * Return the char width (in pixel),
 * if nbytes != NULL returns there size of multibyte character in bytes.
 */
int xitk_font_get_char_width(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);

/*
 * Return the height (in pixel) of string, with length length.
 * if nbytes != NULL returns there size of multibyte character in bytes.
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes);

/*
 * Return string height (in pixel) of string.
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c);

/*
 * Return char height (in pixel).
 */
int xitk_font_get_char_height(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);

/*
 * Get text extents of string length length.
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int length,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 * Get string extents.
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			    int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 * Get ascent height (in pixel) of string.
 */
int xitk_font_get_ascent(xitk_font_t *xtfs, const char *c);

/*
 * Get descent height (in pixel) of string.
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c);

/*
 * Set font to GC.
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc);

#endif
