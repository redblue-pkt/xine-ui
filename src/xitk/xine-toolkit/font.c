/* 
 * Copyright (C) 2000-2003 the xine project
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"

/*
 *
 */
xitk_font_t *xitk_font_load_font(Display *display, char *font) {
  xitk_font_t *xtfs;

  assert((display != NULL) && (font != NULL));

  xtfs = (xitk_font_t *) xitk_xmalloc(sizeof(xitk_font_t));

  XLOCK(display);
  xtfs->font = XLoadQueryFont(display, font);
  XUNLOCK(display);
  
  if(xtfs->font == NULL) {
    char *fname = xitk_get_default_font();
    if(fname) {
      XLOCK(display);
      xtfs->font = XLoadQueryFont(display, fname);
      XUNLOCK(display);
    }

    if(xtfs->font == NULL) {
      fname = xitk_get_system_font();
      XLOCK(display);
      xtfs->font = XLoadQueryFont(display, fname);
      XUNLOCK(display);
    }
    
    if(xtfs->font == NULL) {
      XITK_WARNING("%s(): XLoadQueryFont() failed\n", __FUNCTION__);
      free(xtfs);
      return NULL;
    }
  }
  
  xtfs->display = display;
  
  return xtfs;
}

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs) {

  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL));

  XLOCK(xtfs->display);
  XFreeFont(xtfs->display, xtfs->font);
  XUNLOCK(xtfs->display);
  free(xtfs);
}

/*
 *
 */
Font xitk_font_get_font_id(xitk_font_t *xtfs) {
  
  assert((xtfs != NULL) && (xtfs->font != NULL));

  return xtfs->font->fid;
}

/*
 *
 */
int xitk_font_is_font_8(xitk_font_t *xtfs) {

  assert((xtfs != NULL) && (xtfs->font != NULL));

  return ((xtfs->font->min_byte1 == 0) && (xtfs->font->max_byte1 == 0));
}

/*
 *
 */
int xitk_font_is_font_16(xitk_font_t *xtfs) {

  assert((xtfs != NULL) && (xtfs->font != NULL));

  return ((xtfs->font->min_byte1 != 0) || (xtfs->font->max_byte1 != 0));
}

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int length) {
  int len;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    len = XTextWidth (xtfs->font, c, length);
  else
    len = XTextWidth16 (xtfs->font, (XChar2b *)c, length);

  XUNLOCK(xtfs->display);
  
  return len;
}

/*
 *
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c) {
  
  return (xitk_font_get_text_width(xtfs, c, strlen(c)));
}

/*
 *
 */
int xitk_font_get_char_width(xitk_font_t *xtfs, unsigned char *c) {
  unsigned int  ch = (*c & 0xff);
  int           width;
  XCharStruct  *chars;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (c != NULL));

  if(xitk_font_is_font_8(xtfs) && 
     ((ch >= xtfs->font->min_char_or_byte2) && (ch <= xtfs->font->max_char_or_byte2))) {

    chars = xtfs->font->per_char;

    if(chars)
      width = chars[ch - xtfs->font->min_char_or_byte2].width;
    else
      width = xtfs->font->min_bounds.width;

  }
  else {
    width = xitk_font_get_text_width(xtfs, c, 1);
  }
  
  return width;
}

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int length) {
  int lbearing, rbearing, width, ascent, descent;
  int height;
  
  xitk_font_text_extent(xtfs, c, length, &lbearing, &rbearing, &width, &ascent, &descent);
 
  height = ascent + descent;
 
  return height;
}

/*
 *
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c) {

  return (xitk_font_get_text_height(xtfs, c, strlen(c)));
}

/*
 *
 */
int xitk_font_get_char_height(xitk_font_t *xtfs, unsigned char *c) {

  return (xitk_font_get_text_height(xtfs, c, 1));
}

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int length,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  XCharStruct ov;
  int         dir;
  int         fascent, fdescent;

  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    XTextExtents(xtfs->font, c, length, &dir, &fascent, &fdescent, &ov);
  else
    XTextExtents16(xtfs->font, (XChar2b *)c, (length / 2), &dir, &fascent, &fdescent, &ov);

  XUNLOCK(xtfs->display);

  if(lbearing)
    *lbearing = ov.lbearing;
  if(rbearing)
    *rbearing = ov.rbearing;
  if(width)
    *width    = ov.width;
  if(ascent)
    *ascent  = xtfs->font->ascent;
  if(descent)
    *descent = xtfs->font->descent;
#if 0
  if(ascent)
    *ascent   = ov.ascent;
  if(descent)
    *descent  = ov.descent;
#endif
}

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			     int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {

  xitk_font_text_extent(xtfs, c, strlen(c), lbearing, rbearing, width, ascent, descent);
}

/*
 *
 */
int xitk_font_get_ascent(xitk_font_t *xtfs, const char *c) {
  int lbearing, rbearing, width, ascent, descent;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return ascent;
}

/*
 *
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c) {
  int lbearing, rbearing, width, ascent, descent;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return descent;
}

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc) {

  assert((xtfs != NULL) && (xtfs->display != NULL));

  XLOCK(xtfs->display);
  XSetFont(xtfs->display, gc, xitk_font_get_font_id(xtfs));
  XUNLOCK(xtfs->display);
}
