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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "font.h"
#include "image.h"
#include "widget_types.h"
#include "_xitk.h"

/*
 * Get a pixel color from rgb values.
 */
unsigned int xitk_get_pixel_color_from_rgb(ImlibData *im, int r, int g, int b) {
  XColor xcolor;
  unsigned int pixcol;

  assert(im);
  
  xcolor.flags = DoRed | DoBlue | DoGreen;
  
  XLOCK(im->x.disp);
  
  xcolor.red   = r<<8;
  xcolor.green = g<<8;
  xcolor.blue  = b<<8;
  XAllocColor(im->x.disp, Imlib_get_colormap(im), &xcolor);
  pixcol = xcolor.pixel;
  
  XUNLOCK(im->x.disp);

  return pixcol;
}

/*
 * Some default pixel colors.
 */
unsigned int xitk_get_pixel_color_black(ImlibData *im) {
  return xitk_get_pixel_color_from_rgb(im, 0, 0, 0);
}
unsigned int xitk_get_pixel_color_white(ImlibData *im) {
  return xitk_get_pixel_color_from_rgb(im, 255, 255, 255);
}
unsigned int xitk_get_pixel_color_lightgray(ImlibData *im) {
  return xitk_get_pixel_color_from_rgb(im, 224, 224, 224);
}
unsigned int xitk_get_pixel_color_gray(ImlibData *im) {
  return xitk_get_pixel_color_from_rgb(im, 192, 192, 192);
}
unsigned int xitk_get_pixel_color_darkgray(ImlibData *im) {
  return xitk_get_pixel_color_from_rgb(im, 160, 160, 160);
}

/*
 *
 */
void xitk_image_free_image(ImlibData *im, xitk_image_t **src) {

  assert(im && *src);

  if((*src)->mask != None) {
    XLOCK(im->x.disp);
    XFreePixmap(im->x.disp, (*src)->mask);
    XUNLOCK(im->x.disp);
  }
  
  if((*src)->image != None) {
    XLOCK(im->x.disp);
    XFreePixmap(im->x.disp, (*src)->image);
    XUNLOCK(im->x.disp);
  }

  XITK_FREE((*src));
  *src = NULL;
}

/*
 *
 */
Pixmap xitk_image_create_pixmap(ImlibData *im, int width, int height) {
  Pixmap p;
  
  assert(im && width && height);
  
  XLOCK(im->x.disp);
  p = XCreatePixmap(im->x.disp, im->x.base_window, width, height, im->x.depth);
  XUNLOCK(im->x.disp);

  return p;
}
/*
 *
 */

Pixmap xitk_image_create_mask_pixmap(ImlibData *im, int width, int height) {
  Pixmap p;
  
  assert(im && width && height);
  
  XLOCK(im->x.disp);
  p = XCreatePixmap(im->x.disp, im->x.base_window, width, height, 1);
  XUNLOCK(im->x.disp);

  return p;
}

/*
 *
 */
void xitk_image_change_image(ImlibData *im, 
			     xitk_image_t *src, xitk_image_t *dest, int width, int height) {
  GC         gc;
  XGCValues  gcv;

  assert(im && src && dest && width && height);
  
  if(dest->mask != None) {
    XLOCK(im->x.disp);
    XFreePixmap(im->x.disp, dest->mask);
    XUNLOCK(im->x.disp);
  }
  
  if(src->mask != None) {
    dest->mask = xitk_image_create_pixmap(im, width, height);
    XLOCK(im->x.disp);
    gcv.graphics_exposures = False;
    gc = XCreateGC(im->x.disp, dest->mask, GCGraphicsExposures, &gcv);
    XCopyArea(im->x.disp, src->mask, dest->mask, gc, 0, 0, width, height, 0, 0);
    XFreeGC(im->x.disp, gc);
    XUNLOCK(im->x.disp);
  }
  else
    dest->mask = None;
  
  if(dest->image != None) {
    XLOCK(im->x.disp);
    XFreePixmap(im->x.disp, dest->image);
    XUNLOCK(im->x.disp);
  }
  
  dest->image = xitk_image_create_pixmap(im, width, height);

  XLOCK(im->x.disp);
  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, dest->image, GCGraphicsExposures, &gcv);
  XCopyArea(im->x.disp, src->image, dest->image, gc, 0, 0, width, height, 0, 0);
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);
  
  dest->width = width;
  dest->height = height;
}

/*
 *
 */
xitk_image_t *xitk_image_create_image(ImlibData *im, int width, int height) {
  xitk_image_t *i;

  assert(im && width && height);

  i = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  i->mask = None;
  i->image = xitk_image_create_pixmap(im, width, height);
  i->width = width;
  i->height = height;

  return i;
}

/*
 * Should be rewrite.
 */
xitk_image_t *xitk_image_create_image_from_string(ImlibData *im, 
						  char *fontname, char *str, int width) {
  xitk_image_t   *image;
  xitk_font_t    *fs;
  GC              gc;
  int             length, height, descent;
  int             force_parsing = 0;

  assert(im && fontname && str && width);

  XLOCK(im->x.disp);
  gc = XCreateGC(im->x.disp, im->x.base_window, None, None);
  XUNLOCK(im->x.disp);
  
  fs = xitk_font_load_font(im->x.disp, fontname);
  xitk_font_set_font(fs, gc);
  length = xitk_font_get_string_length(fs, str);
  height = xitk_font_get_string_height(fs, str);
  descent = xitk_font_get_descent(fs, str);

  if(strrchr(str, '\n'))
    force_parsing = 1;

  if((length > width) || force_parsing) {
    int     llen = 0;
    char   *lines[256];
    char   *p = str;
    char   *o = str;

    while(*p) {
      char buf2[1024];

      while(*o == ' ' || *o == '\t') p = ++o;

      memset(&buf2, 0, sizeof(buf2));
      snprintf(buf2, (p - o), "%s", o);

      if(xitk_font_get_string_length(fs, buf2) > width) {
	char *pp = p + 1;

	do {
	  pp--;
	  while(*pp != ' ' && *pp != '\t' && pp > o)  pp--; 

	  if(*pp == '\n') {
	    memset(&buf2, 0, sizeof(buf2));
	    snprintf(buf2, (pp - o) + 1, "%s", o);
	    buf2[(pp - o) + 1] = '\0';
	  }
	  else {
	    memset(&buf2, 0, sizeof(buf2));
	    snprintf(buf2, (pp - o) + 1, "%s", o);
	  }

	} while((xitk_font_get_string_length(fs, buf2) > width) || (*pp == '\0'));

	if(pp == o) {
	  lines[llen++] = strdup(o);
	  goto create_image;
	}

	memset(&buf2, 0, sizeof(buf2));
	snprintf(buf2, (pp - o) + 1, "%s", o);
	lines[llen++] = strdup(buf2);

	o = p = pp+1;

      }
      else {

	p++;
	if(*p == '\n') {
	  memset(&buf2, 0, sizeof(buf2));
	  snprintf(buf2, (p - o) + 1, "%s", o);
	  buf2[(p - o) + 1] = '\0';
	  if(buf2[0] == '\n')
	    memset(&buf2, 0, sizeof(buf2));
	  lines[llen++] = strdup(buf2);
	  o = p + 1;
	  p++;
	}
	
      }

    }
    if(*o != '\0')
      lines[llen++] = strdup(o);
    
  create_image:
    
    image = xitk_image_create_image(im, width, (height * llen) + (3 * llen));
    draw_flat(im, image->image, width, (height * llen) + (3 * llen));
    
    /* Color, DrawString */  
    {
      int            i, j;
      
      XLOCK(im->x.disp);
      
      XSetForeground(im->x.disp, gc, xitk_get_pixel_color_black(im));
      j = height;
      for(i = 0; i < llen; i++, j += (height + 3)) {
	XDrawString(im->x.disp, image->image, gc, 0, (j - descent), lines[i], strlen(lines[i]));
      }
      XUNLOCK(im->x.disp);
    }

   
    { /* Free lines */
      int i;
      for(i = 0; i<llen;i++)
	XITK_FREE(lines[i]);
    }

  }
  else {
    height = xitk_font_get_string_height(fs, str);
    image = xitk_image_create_image(im, width, height);
    draw_flat(im, image->image, width, height);

    XLOCK(im->x.disp);
    XSetForeground(im->x.disp, gc, xitk_get_pixel_color_black(im));
    XDrawString(im->x.disp, image->image, gc, 0, (height - descent), str, strlen(str));
    XUNLOCK(im->x.disp);
  }
  
  xitk_font_unload_font(fs);

  XLOCK(im->x.disp);
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);

  return image;
}

/*
 *
 */
void xitk_image_add_mask(ImlibData *im, xitk_image_t *dest) {
  GC            gc;
  XGCValues     gcv;

  assert(im && dest);
  
  if(dest->mask != None) {
    XLOCK(im->x.disp);
    XFreePixmap(im->x.disp, dest->mask);
    XUNLOCK(im->x.disp);
  }
  
  dest->mask = xitk_image_create_mask_pixmap(im, dest->width, dest->height);

  XLOCK(im->x.disp);
  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, dest->mask, GCGraphicsExposures, &gcv);
  XSetForeground(im->x.disp, gc, 1);
  XFillRectangle(im->x.disp, dest->mask, gc, 0, 0, dest->width, dest->height);
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_arrow(ImlibData *im, xitk_image_t *p, int direction) {
  GC             gc;
  XGCValues      gcv;
  int            w = p->width / 3;
  int            h = p->height;
  XPoint         points[4];
  int            i, offset = 0;
  short          x1, x2, x3;
  short          y1, y2, y3;

  assert(im && p);
  
  XLOCK(im->x.disp);
  
  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p->image, GCGraphicsExposures, &gcv);
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_black(im));
  XUNLOCK(im->x.disp);

  for(i = 0; i < 3; i++) {
    
    if(direction == DIRECTION_UP) {
      x1 = (w / 2) + offset;
      y1 = (h / 4);
      
      x2 = ((w / 4) * 3) + offset;
      y2 = ((h / 4) * 3);
      
      x3 = (w / 4) + offset;
      y3 = ((h / 4) * 3); 
    }
    else if(direction == DIRECTION_DOWN) {
      x1 = (w / 2) + offset;
      y1 = ((h / 4) * 3);

      x2 = (w / 4) + offset;
      y2 = (h / 4);
      
      x3 = ((w / 4) * 3) + offset;
      y3 = (h / 4); 
    }
    else {
      XITK_WARNING("%s(): direction '%d' is unhandled.\n", __FUNCTION__, direction);
      return;
    }
    
    if(i == 2) {
      x1++; x2++; x3++;
      y1++; y2++; y3++;
    }
    
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    points[2].x = x3;
    points[2].y = y3;
    points[3].x = x1;
    points[3].y = y1;
    
    offset += w;

    XLOCK(im->x.disp);
    XFillPolygon(im->x.disp, p->image, gc, &points[0], 4, Complex, CoordModeOrigin);
    XUNLOCK(im->x.disp);
  }

  XLOCK(im->x.disp);
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_arrow_up(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_UP);
}

/*
 *
 */
void draw_arrow_down(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_DOWN);
}

/*
 *
 */
static void _draw_rectangular_box(ImlibData *im, Pixmap p, 
				  int x, int y, int width, int height, int relief) {
  GC            gc;
  XGCValues     gcv;

  assert(im && p && width && height);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p, GCGraphicsExposures, &gcv);
  
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, gc, xitk_get_pixel_color_white(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, gc, xitk_get_pixel_color_darkgray(im));

  XDrawLine(im->x.disp, p, gc, x, y, (x + width), y);
  XDrawLine(im->x.disp, p, gc, x, y, x, (y + height));
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, gc, xitk_get_pixel_color_darkgray(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, gc, xitk_get_pixel_color_white(im));
  
  XDrawLine(im->x.disp, p, gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p, gc, x, (y + height), (x + width), (y + height));

  XFreeGC(im->x.disp, gc);

  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box_with_colors(ImlibData *im, Pixmap p, 
					      int x, int y, int width, int height, 
					      unsigned int lcolor, unsigned int dcolor,
					      int relief) {
  GC            gc;
  XGCValues     gcv;

  assert(im && p && width && height);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p, GCGraphicsExposures, &gcv);
  
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, gc, lcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, gc, dcolor);

  XDrawLine(im->x.disp, p, gc, x, y, (x + width), y);
  XDrawLine(im->x.disp, p, gc, x, y, x, (y + height));

  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, gc, dcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, gc, lcolor);
  
  XDrawLine(im->x.disp, p, gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p, gc, x, (y + height), (x + width), (y + height));

  XFreeGC(im->x.disp, gc);

  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_rectangular_inner_box(ImlibData *im, Pixmap p, int x, int y, int width, int height) {
  _draw_rectangular_box(im, p, x, y, width, height, DRAW_INNER);
}
void draw_rectangular_outter_box(ImlibData *im, Pixmap p, int x, int y, int width, int height) {
  _draw_rectangular_box(im, p, x, y, width, height, DRAW_OUTTER);
}

/*
 *
 */
static void _draw_three_state(ImlibData *im, xitk_image_t *p, int style) {
  GC            gc;
  XGCValues     gcv;
  int           w = p->width / 3;
  int           h = p->height;

  assert(im && p);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p->image, GCGraphicsExposures, &gcv);
  
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image, gc, 0, 0, w , h);
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image, gc, w, 0, (w * 2) , h);

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image, gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image, gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image, gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image, gc, w, 0, w, (h - 1));
  
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_darkgray(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image, gc, (w - 1), 0, (w - 1), (h - 1));
    XDrawLine(im->x.disp, p->image, gc, 0, (h - 1), w, (h - 1));
  }
  XDrawLine(im->x.disp, p->image, gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image, gc, w, (h - 1), (w * 2), (h - 1));

  XFillRectangle(im->x.disp, p->image, gc, w * 2 , 0, (w - 1), (h - 1));
 
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_black(im));
  XDrawLine(im->x.disp, p->image, gc, (w * 2), 0, (w * 3), 0);
  XDrawLine(im->x.disp, p->image, gc, (w * 2), 0, (w * 2), (h - 1));

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image, gc, (w * 3) - 1, 0, (w * 3) - 1, (h - 1));
  XDrawLine(im->x.disp, p->image, gc, (w * 2), (h - 1), (w * 3), (h - 1));

  XFreeGC(im->x.disp, gc);

  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_two_state(ImlibData *im, xitk_image_t *p, int style) {
  GC            gc;
  XGCValues     gcv;
  int           w = p->width / 2;
  int           h = p->height;

  assert(im && p);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p->image, GCGraphicsExposures, &gcv);
  
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image, gc, 0, 0, w - 1, h - 1);
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image, gc, w, 0, (w * 2) - 1 , h - 1);

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_black(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image, gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image, gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image, gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image, gc, w, 0, w, (h - 1));

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image, gc, (w - 1), 0 , (w - 1), (h - 1));
    XDrawLine(im->x.disp, p->image, gc, 0 ,(h - 1), w , (h - 1));
  }
  XDrawLine(im->x.disp, p->image, gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image, gc, w, (h - 1),(w * 2), (h - 1));

  XFreeGC(im->x.disp, gc);

  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_relief(ImlibData *im, Pixmap p, int w, int h, int relief) {
  GC            gc;
  XGCValues     gcv;
  
  assert(im && p);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p, GCGraphicsExposures, &gcv);
  
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p, gc, 0, 0, w , h);
  
  if(relief != DRAW_FLATTER) {

    if(relief == DRAW_OUTTER)
      _draw_rectangular_box_with_colors(im, p, 0, 0, w-1, h-1, 
					xitk_get_pixel_color_white(im),	
					xitk_get_pixel_color_darkgray(im),
					relief);
    else if(relief == DRAW_INNER)
      _draw_rectangular_box_with_colors(im, p, 0, 0, w-1, h-1, 
					xitk_get_pixel_color_white(im),
					xitk_get_pixel_color_black(im),	
					relief);
  }

  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_flat_three_state(ImlibData *im, xitk_image_t *p) {
  _draw_three_state(im, p, STYLE_FLAT);
}

/*
 *
 */
void draw_bevel_three_state(ImlibData *im, xitk_image_t *p) {
  _draw_three_state(im, p, STYLE_BEVEL);
}

/*
 *
 */
void draw_bevel_two_state(ImlibData *im, xitk_image_t *p) {
  _draw_two_state(im, p, STYLE_BEVEL);
}

/*
 *
 */
static void _draw_paddle_three_state(ImlibData *im, xitk_image_t *p, int direction) {
  GC            gc, mgc;
  XGCValues     gcv;
  int           w = p->width / 3;
  int           h = p->height;

  assert(im && p);

  XLOCK(im->x.disp);

  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p->image, GCGraphicsExposures, &gcv);

  gcv.graphics_exposures = False;
  mgc = XCreateGC(im->x.disp, p->mask, GCGraphicsExposures, &gcv);

  /* Draw mask */
  XSetForeground(im->x.disp, mgc, 0);
  /* Top */
  XDrawLine(im->x.disp, p->mask, mgc, 0, 0, (w - 1), 0);
  XDrawLine(im->x.disp, p->mask, mgc, 0, 1, (w - 1), 1);
  /* Bottom */
  XDrawLine(im->x.disp, p->mask, mgc, 0, (h - 1), (w - 1), (h - 1));
  XDrawLine(im->x.disp, p->mask, mgc, 0, (h - 1) - 1, (w - 1), (h - 1) - 1);
  /* Left */
  XDrawLine(im->x.disp, p->mask, mgc, 0, 0, 0, (h - 1));
  XDrawLine(im->x.disp, p->mask, mgc, 1, 0, 1, (h - 1));
  /* Right */
  XDrawLine(im->x.disp, p->mask, mgc, (w - 1), 0, (w - 1), (h - 1));
  XDrawLine(im->x.disp, p->mask, mgc, (w - 1) - 1, 0, (w - 1) - 1, (h - 1));
  
  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image, gc, 2, 2, w - 4, (h - 1) - 2);

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image, gc, w + 2, 2, (w * 2) - 4, (h - 1) - 2);

  XSetForeground(im->x.disp, gc, xitk_get_pixel_color_darkgray(im));
  XFillRectangle(im->x.disp, p->image, gc, (w * 2) + 2, 2, ((w * 3) - 1) - 4, (h - 1) - 2);
  
  _draw_rectangular_box_with_colors(im, p->image, 2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_darkgray(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(im, p->image, w+2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_darkgray(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(im, p->image, (w*2)+2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_INNER);
  
  { /* Enlighting paddle */
    int xx, yy, ww, hh;
    int i, offset = 0;

    if(direction == DIRECTION_UP) {
      xx = 4; yy = ((h-1)>>1); ww = (w-1) - 8; hh = 1;
    }
    else if(direction == DIRECTION_LEFT) {
      xx = ((w-1)>>1); yy = 4; ww = 1; hh = (h-1) - 8;
    }
    else {
      XITK_WARNING("%s(): direction '%d' is unhandled.\n", __FUNCTION__, direction);
      return;
    }

    for(i = 0; i < 3; i++, offset += w) {
      if(i == 2) { xx++; yy++; }
      draw_rectangular_outter_box(im, p->image, xx + offset, yy, ww, hh);
    }
  }
  
  XFreeGC(im->x.disp, gc);
  XFreeGC(im->x.disp, mgc);

  XUNLOCK(im->x.disp);
}
/*
 *
 */
void draw_paddle_three_state_vertical(ImlibData *im, xitk_image_t *p) {
  _draw_paddle_three_state(im, p, DIRECTION_UP);
}
void draw_paddle_three_state_horizontal(ImlibData *im, xitk_image_t *p) {
  _draw_paddle_three_state(im, p, DIRECTION_LEFT);
}

/*
 *
 */
void draw_inner(ImlibData *im, Pixmap p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_INNER);
}

/*
 *
 */
void draw_outter(ImlibData *im, Pixmap p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_OUTTER);
}

/*
 *
 */
void draw_flat(ImlibData *im, Pixmap p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_FLATTER);
}

/*
 *
 */
void draw_flat_with_color(ImlibData *im, Pixmap p, int w, int h, unsigned int color) {
  GC            gc;
  XGCValues     gcv;
  
  assert(im && (p != None));

  XLOCK(im->x.disp);
  
  gcv.graphics_exposures = False;
  gc = XCreateGC(im->x.disp, p, GCGraphicsExposures, &gcv);
  
  XSetForeground(im->x.disp, gc, color);
  XFillRectangle(im->x.disp, p, gc, 0, 0, w , h);
  
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
xitk_image_t *xitk_image_load_image(ImlibData *im, char *image) {
  ImlibImage    *img = NULL;
  xitk_image_t  *i;

  assert(im);

  if(image == NULL) {
    XITK_WARNING("%s(): image name is NULL\n", __FUNCTION__);
    return NULL;
  }


  i = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  
  XLOCK(im->x.disp);
  if(!(img = Imlib_load_image(im, (char *)image))) {
    XITK_DIE("%s(): couldn't find image %s\n", __FUNCTION__, image);
  }
  
  Imlib_render (im, img, img->rgb_width, img->rgb_height);
  XUNLOCK(im->x.disp);
  
  i->image  = Imlib_copy_image(im, img);
  i->mask   = Imlib_copy_mask(im, img);
  i->width  = img->rgb_width;
  i->height = img->rgb_height;
  
  XLOCK(im->x.disp);
  Imlib_destroy_image(im, img);
  XUNLOCK(im->x.disp);
  
  return i;
} 

/*
 * ********************************************************************************
 *                              Widget specific part
 * ********************************************************************************
 */

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  image_private_data_t *private_data = (image_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_IMAGE) {
    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->skin);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  image_private_data_t *private_data = (image_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_IMAGE) {
    if(sk == BACKGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static void paint_image (xitk_widget_t *i, Window win, GC gc) {
  xitk_image_t *skin;
  GC lgc;
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;
  
  if ((i->widget_type & WIDGET_TYPE_IMAGE) && i->visible) {

    skin = private_data->skin;
    
    XLOCK (private_data->imlibdata->x.disp);

    lgc = XCreateGC(private_data->imlibdata->x.disp, win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, lgc);
    
    if (skin->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, i->x, i->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask);
    }
    
    XCopyArea (private_data->imlibdata->x.disp, skin->image, win, lgc, 0, 0,
	       skin->width, skin->height, i->x, i->y);
    
    XFreeGC(private_data->imlibdata->x.disp, lgc);

    XUNLOCK (private_data->imlibdata->x.disp);
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "paint image on something (%d) "
	     "that is not an image\n", i->widget_type);
#endif
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *i, xitk_skin_config_t *skonfig) {
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;
  
  if ((i->widget_type & WIDGET_TYPE_IMAGE) && i->visible) {
    
    XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->skin);
    private_data->skin = xitk_image_load_image(private_data->imlibdata,
					       xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
    
    i->x               = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
    i->y               = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
    i->width           = private_data->skin->width;
    i->height          = private_data->skin->height;
    
    xitk_set_widget_pos(i, i->x, i->y);
  }
}

/*
 *
 */
xitk_widget_t *xitk_image_create (xitk_skin_config_t *skonfig, xitk_image_widget_t *im) {
  xitk_widget_t              *mywidget;
  image_private_data_t *private_data;

  XITK_CHECK_CONSTITENCY(im);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (image_private_data_t *) xitk_xmalloc (sizeof (image_private_data_t));

  private_data->imlibdata         = im->imlibdata;
  private_data->skin_element_name = strdup(im->skin_element_name);


  private_data->bWidget           = mywidget;
  private_data->skin              = xitk_image_load_image(private_data->imlibdata,
							  xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->x                     = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
  mywidget->y                     = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
  mywidget->width                 = private_data->skin->width;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_IMAGE;
  mywidget->paint                 = paint_image;
  mywidget->notify_click          = NULL;
  mywidget->notify_focus          = NULL;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_change_skin    = notify_change_skin;
  mywidget->notify_destroy        = notify_destroy;
  mywidget->get_skin              = get_skin;

  return mywidget;
}
