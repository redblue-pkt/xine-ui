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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <X11/Xlib.h>

#include "_xitk.h"

/*
 * Get a pixel color from rgb values.
 */
unsigned int xitk_get_pixel_color_from_rgb(ImlibData *im, int r, int g, int b) {
  XColor       xcolor;
  unsigned int pixcol;

  assert(im);
  
  xcolor.flags = DoRed | DoBlue | DoGreen;
  
  xcolor.red   = r<<8;
  xcolor.green = g<<8;
  xcolor.blue  = b<<8;

  XLOCK(im->x.disp);
  XAllocColor(im->x.disp, Imlib_get_colormap(im), &xcolor);
  XUNLOCK(im->x.disp);

  pixcol = xcolor.pixel;

  return pixcol;
}

/*
 * Some default pixel colors.
 */
unsigned int xitk_get_pixel_color_black(ImlibData *im) {
  int user_color = xitk_get_black_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 0, 0, 0);
}
unsigned int xitk_get_pixel_color_white(ImlibData *im) {
  int user_color = xitk_get_white_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 255, 255, 255);
}
unsigned int xitk_get_pixel_color_lightgray(ImlibData *im) {
  int user_color = xitk_get_focus_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 224, 224, 224);
}
unsigned int xitk_get_pixel_color_gray(ImlibData *im) {
  int user_color = xitk_get_background_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 192, 192, 192);
}
unsigned int xitk_get_pixel_color_darkgray(ImlibData *im) {
  int user_color = xitk_get_select_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 160, 160, 160);
}
unsigned int xitk_get_pixel_color_warning_foreground(ImlibData *im) {
  int user_color = xitk_get_warning_foreground();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_black(im);
}
unsigned int xitk_get_pixel_color_warning_background(ImlibData *im) {
  int user_color = xitk_get_warning_background();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 255, 255, 0);
}

/*
 *
 */
void xitk_image_free_image(ImlibData *im, xitk_image_t **src) {

  assert(im && *src);

  if((*src)->mask)
    xitk_image_destroy_xitk_pixmap((*src)->mask);
  
  if((*src)->image)
    xitk_image_destroy_xitk_pixmap((*src)->image);

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
static void xitk_image_xitk_pixmap_destroyer(xitk_pixmap_t *xpix) {

  assert(xpix);

  XLOCK(xpix->imlibdata->x.disp);

#ifdef HAVE_SHM
  if(xpix->shm)
    XShmDetach(xpix->imlibdata->x.disp, xpix->shminfo);

  if(xpix->xim)
    XDestroyImage(xpix->xim);

  if(xpix->shm) {
    shmdt(xpix->shminfo->shmaddr);
    free(xpix->shminfo);
  }
#endif
  
  if(xpix->pixmap != None)
    XFreePixmap(xpix->imlibdata->x.disp, xpix->pixmap);
  
  XFreeGC(xpix->imlibdata->x.disp, xpix->gc);
  
  XUNLOCK(xpix->imlibdata->x.disp);
  
  XITK_FREE(xpix);
}

/*
 *
 */
xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(ImlibData *im, 
							int width, int height, int depth) {
  xitk_pixmap_t    *xpix;
#ifdef HAVE_SHM
  XShmSegmentInfo  *shminfo;
#endif
  
  assert(im && width && height);
  
  xpix = (xitk_pixmap_t *) xitk_xmalloc(sizeof(xitk_pixmap_t));
  xpix->imlibdata = im;
  xpix->destroy   = xitk_image_xitk_pixmap_destroyer;
  xpix->width     = width;
  xpix->height    = height;
  xpix->xim       = NULL;
  xpix->shm       = 0;
  
  XLOCK(im->x.disp);

#ifdef HAVE_SHM
  if(xitk_is_use_xshm() == 2) {
    XImage   *xim;
    
    shminfo = (XShmSegmentInfo *) xitk_xmalloc(sizeof(XShmSegmentInfo));
    
    xitk_x_error = 0;
    xitk_install_x_error_handler();
    
    xim = XShmCreateImage(im->x.disp, im->x.visual, depth, ZPixmap, NULL, shminfo, width, height);
    if(!xim) {
      XITK_WARNING("XShmCreateImage() failed.\n");
      free(shminfo);
      xitk_uninstall_x_error_handler();
      goto __noxshm_pixmap;
    }
    
    shminfo->shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0777);
    if(shminfo->shmid < 0) {
      XITK_WARNING("shmget() failed.\n");
      XDestroyImage(xim);
      free(shminfo);
      xitk_uninstall_x_error_handler();
      goto __noxshm_pixmap;
    }
    
    shminfo->shmaddr  = xim->data = shmat(shminfo->shmid, 0, 0);
    if(xim->data == (char *) -1) {
      XITK_WARNING("shmmat() failed.\n");
      XDestroyImage(xim);
      shmctl(shminfo->shmid, IPC_RMID, 0);
      free(shminfo);
      xitk_uninstall_x_error_handler();
      goto __noxshm_pixmap;
    }
    
    shminfo->readOnly = False;
    XShmAttach(im->x.disp, shminfo);
    XSync(im->x.disp, False);
    
    if(xitk_x_error) {
      XITK_WARNING("XShmAttach() failed.\n");
      XDestroyImage(xim);
      shmdt(shminfo->shmaddr);
      shmctl(shminfo->shmid, IPC_RMID, 0);
      free(shminfo);
      xitk_uninstall_x_error_handler();
      goto __noxshm_pixmap;
    }

    xpix->xim = xim;
    xpix->pixmap = XShmCreatePixmap(im->x.disp, im->x.base_window, 
				    shminfo->shmaddr, shminfo, width, height, depth);

    if(!xpix->pixmap) {
      XITK_WARNING("XShmCreatePixmap() failed.\n");
      XShmDetach(xpix->imlibdata->x.disp, xpix->shminfo);
      XDestroyImage(xim);
      shmdt(shminfo->shmaddr);
      shmctl(shminfo->shmid, IPC_RMID, 0);
      free(shminfo);
      xitk_uninstall_x_error_handler();
      goto __noxshm_pixmap;
    }
    else {
      xpix->shm = 1;
      //      XDestroyImage(xim);
      xpix->shminfo = shminfo;
      xpix->gcv.graphics_exposures = False;
      xpix->gc = XCreateGC(im->x.disp, xpix->pixmap, GCGraphicsExposures, &xpix->gcv);
      shmctl(shminfo->shmid, IPC_RMID, 0);

      //      XShmPutImage(im->x.disp, xpix->pixmap, xpix->gc, xpix->xim, 
      //		   0, 0, 0, 0, width, height, False);
      //      XSync(im->x.disp, False);
      
      xitk_uninstall_x_error_handler();
    }
  }
  else
#endif
    {
#ifdef HAVE_SHM /* Just to make GCC happy */
    __noxshm_pixmap:
#endif
      xpix->shm     = 0;
#ifdef HAVE_SHM
      xpix->shminfo = NULL;
#endif
      xpix->pixmap  = XCreatePixmap(im->x.disp, im->x.base_window, width, height, depth);

      xpix->gcv.graphics_exposures = False;
      xpix->gc = XCreateGC(im->x.disp, xpix->pixmap, GCGraphicsExposures, &xpix->gcv);
    }
  XUNLOCK(im->x.disp);  

  return xpix;
}

xitk_pixmap_t *xitk_image_create_xitk_pixmap(ImlibData *im, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(im, width, height, im->x.depth);
}
xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(ImlibData *im, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(im, width, height, 1);
}

void xitk_image_destroy_xitk_pixmap(xitk_pixmap_t *p) {
  assert(p);
  p->destroy(p);
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
  assert(im && src && dest && width && height);
  
  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);
  
  if(src->mask) {
    xitk_image_destroy_xitk_pixmap(src->mask);

    dest->mask = xitk_image_create_xitk_pixmap(im, width, height);

    XLOCK(im->x.disp);
    XCopyArea(im->x.disp, src->mask->pixmap, dest->mask->pixmap, dest->mask->gc,
	      0, 0, width, height, 0, 0);
    XUNLOCK(im->x.disp);

  }
  else
    dest->mask = NULL;
  
  if(dest->image)
    xitk_image_destroy_xitk_pixmap(dest->image);
  
  dest->image = xitk_image_create_xitk_pixmap(im, width, height);

  XLOCK(im->x.disp);
  XCopyArea(im->x.disp, src->image->pixmap, dest->image->pixmap, dest->image->gc,
	    0, 0, width, height, 0, 0);
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
  i->mask = NULL;
  i->image = xitk_image_create_xitk_pixmap(im, width, height);
  i->width = width;
  i->height = height;

  return i;
}


/*
 *
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(ImlibData *im, 
							      char *fontname, 
							      int width, int align, char *str,
							      unsigned int foreground,
							      unsigned int background) {
  xitk_image_t   *image;
  xitk_font_t    *fs;
  GC              gc;
  int             length, height, descent;
  char           *po, *p;
  char           *lines[256];
  int             numlines = 0;
  char            bufsubs[BUFSIZ], buf[BUFSIZ];

  assert(im && fontname && str && width);

  XLOCK(im->x.disp);
  gc = XCreateGC(im->x.disp, im->x.base_window, None, None);
  XUNLOCK(im->x.disp);
  
  fs = xitk_font_load_font(im->x.disp, fontname);
  xitk_font_set_font(fs, gc);
  length = xitk_font_get_string_length(fs, str);
  height = xitk_font_get_string_height(fs, str);
  descent = xitk_font_get_descent(fs, str);

  memset(&bufsubs, 0, sizeof(bufsubs));
  memset(&buf, 0, sizeof(buf));
  p = str;

  while(*p != '\0') {
    switch(*p) {
      
    case '\t':
      sprintf(bufsubs, "%s%s", bufsubs, "      ");
      break;
      
    default:
      sprintf(bufsubs, "%s%c", bufsubs, *p);
      break;
    }
    p++;
  }

  po = p = &bufsubs[0];

  while(*p != '\0') {

    switch(*p) {
      
      /* Ignore */
    case '\a':
    case '\b':
    case '\f':
    case '\r':
    case '\v':
      break;
      
    case '\n':
      lines[numlines++] = strdup(buf);
      po = p + 1;
      memset(&buf, 0, sizeof(buf));
      break;

    default:
      sprintf(buf, "%s%c", buf, *p);
      break;
    }

    if(xitk_font_get_string_length(fs, buf) >= width) {
      char buf2[BUFSIZ];
      char *ps;

      memset(&buf2, 0, sizeof(buf2));
      /* step back */
      if((ps = strrchr(buf, ' ')) != NULL) { /* Ok, there a space here */
	ps = p;
	while(*ps != ' ') ps--;
	snprintf(buf2, (ps - po)+1, "%s", buf);
	lines[numlines++] = strdup(buf2);
	p = po = ps;
      }
      else { /* cut to previous char */
	snprintf(buf2, (p - po), "%s", buf);
	sprintf(buf2, "%s%c", buf2, '-');
	lines[numlines++] = strdup(buf2);
	po = (p - 1);
	p -= 2;
      }
      memset(&buf, 0, sizeof(buf));
    }

    p++;
  }

  /* Last chars aren't stored */
  if(strlen(buf))
    lines[numlines++] = strdup(buf);
  
  image = xitk_image_create_image(im, width, ((height * numlines) + (3 * numlines)));
  draw_flat_with_color(im, image->image, image->width, image->height, background);
  
  { /* Draw string in image */
    int i, j, x = 0;

    XLOCK(im->x.disp);
    XSetForeground(im->x.disp, gc, foreground);
    XUNLOCK(im->x.disp);
    
    j = height;
    for(i = 0; i < numlines; i++, j += (height + 3)) {
      length = xitk_font_get_string_length(fs, lines[i]);

      if((align == ALIGN_DEFAULT) || (align == ALIGN_LEFT))
	x = 0;
      else if(align == ALIGN_CENTER)
	x = (width - length)>>1;
      else if(align == ALIGN_RIGHT)
	x = (width - length);
      
      XLOCK(im->x.disp);
      XDrawString(im->x.disp, image->image->pixmap, gc, 
		  x, (j - descent), lines[i], strlen(lines[i]));
      XUNLOCK(im->x.disp);
	
      XITK_FREE(lines[i]);
    }
  }

  xitk_font_unload_font(fs);

  XLOCK(im->x.disp);
  XFreeGC(im->x.disp, gc);
  XUNLOCK(im->x.disp);

  return image;
}
xitk_image_t *xitk_image_create_image_from_string(ImlibData *im, 
						  char *fontname, 
						  int width, int align, char *str) {
  
  return xitk_image_create_image_with_colors_from_string(im,fontname, width, align, str,
							 xitk_get_pixel_color_black(im),
							 xitk_get_pixel_color_gray(im));
}
/*
 *
 */
void xitk_image_add_mask(ImlibData *im, xitk_image_t *dest) {
  assert(im && dest);
  
  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);

  dest->mask = xitk_image_create_xitk_mask_pixmap(im, dest->width, dest->height);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, dest->mask->gc, 1);
  XFillRectangle(im->x.disp, dest->mask->pixmap, dest->mask->gc, 0, 0, dest->width, dest->height);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_arrow(ImlibData *im, xitk_image_t *p, int direction) {
  int            w;
  int            h;
  XPoint         points[4];
  int            i, offset = 0;
  short          x1, x2, x3;
  short          y1, y2, y3;

  assert(im && p);
  
  w = p->width / 3;
  h = p->height;

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
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
    else if(direction == DIRECTION_LEFT) {
      x1 = ((w / 4) * 3) + offset;
      y1 = (h / 4);

      x2 = ((w / 4) * 3) + offset;
      y2 = ((h / 4) * 3);
      
      x3 = (w / 4) + offset;
      y3 = (h / 2); 
    }
    else if(direction == DIRECTION_RIGHT) {
      x1 = (w / 4) + offset;
      y1 = (h / 4);

      x2 = ((w / 4) * 3) + offset;
      y2 = (h / 2);
      
      x3 = (w / 4) + offset;
      y3 = ((h / 4) * 3); 
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
    XFillPolygon(im->x.disp, p->image->pixmap, p->image->gc, 
		 &points[0], 4, Complex, CoordModeOrigin);
    XUNLOCK(im->x.disp);
  }

}

/*
 *
 */
void draw_arrow_up(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_UP);
}
void draw_arrow_down(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_DOWN);
}
void draw_arrow_left(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_LEFT);
}
void draw_arrow_right(ImlibData *im, xitk_image_t *p) {
  _draw_arrow(im, p, DIRECTION_RIGHT);
}

/*
 *
 */
static void _draw_rectangular_box(ImlibData *im, xitk_pixmap_t *p, 
				  int x, int y, int excstart, int excstop,
				  int width, int height, int relief) {
  assert(im && p && width && height);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + excstart), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + excstop), y, (x + width), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, (y + height), (x + width), (y + height));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box_light(ImlibData *im, xitk_pixmap_t *p, 
					int x, int y, int excstart, int excstop,
					int width, int height, int relief) {
  assert(im && p && width && height);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_darkgray(im));

  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + excstart), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + excstop), y, (x + width), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_darkgray(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, (y + height), (x + width), (y + height));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box_with_colors(ImlibData *im, xitk_pixmap_t *p, 
					      int x, int y, int width, int height, 
					      unsigned int lcolor, unsigned int dcolor,
					      int relief) {
  assert(im && p && width && height);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, lcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, dcolor);

  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + width), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, dcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, lcolor);
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, (y + height), (x + width), (y + height));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_rectangular_inner_box(ImlibData *im, xitk_pixmap_t *p, 
				int x, int y, int width, int height) {
  _draw_rectangular_box(im, p, x, y, 0, 0, width, height, DRAW_INNER);
}
void draw_rectangular_outter_box(ImlibData *im, xitk_pixmap_t *p, 
				 int x, int y, int width, int height) {
  _draw_rectangular_box(im, p, x, y, 0, 0, width, height, DRAW_OUTTER);
}

/*
 *
 */
static void _draw_three_state(ImlibData *im, xitk_image_t *p, int style) {
  int           w;
  int           h;

  assert(im && p);

  w = p->width / 3;
  h = p->height;

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w , h);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) , h);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, (h - 1));
  XUNLOCK(im->x.disp);
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 2), 2, (w - 2), (h - 3));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2, (h - 2), w-2, (h - 2));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2*w - 2,     2, 2*w - 2, h - 3);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,     w+2, h - 2, 2*w - 2, h - 2);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,   w * 2,     0,   w * 3,     0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,   w * 2,     0,   w * 2, h - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w * 2 , 0, (w - 1), (h - 1));
 
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 1), 0, (w - 1), (h - 1));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, (h - 1), w-1, (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2)+1, 1, (w * 3)-1, 1);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2)+1, 1, (w * 2)+1, (h - 2));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, (h - 1), (w * 2)-1, (h - 1));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 3) - 1, 1, (w * 3) - 1, (h - 1));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 1, (h - 1), (w * 3), (h - 1));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_two_state(ImlibData *im, xitk_image_t *p, int style) {
  int           w;
  int           h;

  assert(im && p);

  w = p->width / 2;
  h = p->height;

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w - 1, h - 1);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) - 1 , h - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, (h - 1));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 1), 0 , (w - 1), (h - 1));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0 ,(h - 1), w , (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, (h - 1),(w * 2), (h - 1));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_relief(ImlibData *im, xitk_pixmap_t *p, int w, int h, int relief, int light) {
  assert(im && p);
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->pixmap, p->gc, 0, 0, w , h);
  XUNLOCK(im->x.disp);
  
  if(relief != DRAW_FLATTER) {
    
    if(relief == DRAW_OUTTER)
      _draw_rectangular_box_with_colors(im, p, 0, 0, w-1, h-1, 
					xitk_get_pixel_color_white(im),	
					((light) 
					 ? xitk_get_pixel_color_darkgray(im)
					 : xitk_get_pixel_color_black(im)),
					relief);
    else if(relief == DRAW_INNER)
      _draw_rectangular_box_with_colors(im, p, 0, 0, w-1, h-1, 
					xitk_get_pixel_color_white(im),
					xitk_get_pixel_color_black(im),	
					relief);
  }

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
  int           w;
  int           h;

  assert(im && p);

  w = p->width / 3;
  h = p->height;

  XLOCK(im->x.disp);
  /* Draw mask */
  XSetForeground(im->x.disp, p->mask->gc, 0);
  /* Top */
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, (w - 1), 0);
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 1, (w - 1), 1);
  /* Bottom */
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 0, (h - 1), (w - 1), (h - 1));
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 0, (h - 1) - 1, (w - 1), (h - 1) - 1);
  /* Left */
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, 0, (h - 1));
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, 1, 0, 1, (h - 1));
  /* Right */
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, (w - 1), 0, (w - 1), (h - 1));
  XDrawLine(im->x.disp, p->mask->pixmap, p->mask->gc, (w - 1) - 1, 0, (w - 1) - 1, (h - 1));
  XUNLOCK(im->x.disp);
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 2, 2, w - 4, (h - 1) - 2);

  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w + 2, 2, (w * 2) - 4, (h - 1) - 2);

  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 2, 2, ((w * 3) - 1) - 4, (h - 1) - 2);
  XUNLOCK(im->x.disp);
  
  _draw_rectangular_box_with_colors(im, p->image, 2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(im, p->image, w+2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(im, p->image, (w*2)+2, 2, (w-1)-4, (h-1)-4, 
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_INNER);
  
  { /* Enlightening paddle */
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
void draw_inner(ImlibData *im, xitk_pixmap_t *p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_INNER, 0);
}
void draw_inner_light(ImlibData *im, xitk_pixmap_t *p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_INNER, 1);
}

/*
 *
 */
void draw_outter(ImlibData *im, xitk_pixmap_t *p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_OUTTER, 0);
}
void draw_outter_light(ImlibData *im, xitk_pixmap_t *p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_OUTTER, 1);
}

/*
 *
 */
void draw_flat(ImlibData *im, xitk_pixmap_t *p, int w, int h) {
  _draw_relief(im, p, w, h, DRAW_FLATTER, 1);
}

/*
 *
 */
void draw_flat_with_color(ImlibData *im, xitk_pixmap_t *p, int w, int h, unsigned int color) {
  assert(im && p);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->gc, color);
  XFillRectangle(im->x.disp, p->pixmap, p->gc, 0, 0, w , h);
  XUNLOCK(im->x.disp);
}

/*
 *
 */
static void _draw_frame(ImlibData *im, xitk_pixmap_t *p, 
			char *title, char *fontname, int style, int x, int y, int w, int h) {
  xitk_font_t   *fs = NULL;
  int            sty[2];
  int            yoff = 0, xstart = 0, xstop = 0, fheight = 0, fwidth = 0;
  char           buf[BUFSIZ];

  assert(im && p);
  
  if(title) {

    fs = xitk_font_load_font(im->x.disp, (fontname ? fontname : DEFAULT_FONT_12));
    xitk_font_set_font(fs, p->gc);
    fheight = xitk_font_get_string_height(fs, title);
    fwidth = xitk_font_get_string_length(fs, title);

    if(fwidth >= (w - 12)) {
      int nchar = 0;

      memset(&buf, 0, sizeof(buf));
      /* Limit the title to width of frame */
      do {
	nchar++;
	snprintf(buf, nchar, "%s", title);
      } while(xitk_font_get_string_length(fs, buf) < (w - 12));
      /* Cut title, add three dots a the end */
      nchar -= 4;
      snprintf(buf, nchar, "%s", title);
      sprintf(buf, "%s%s", buf, "...");
    }
    else
      sprintf(buf, "%s", title);

    fwidth = xitk_font_get_string_length(fs, buf);
  }

  sty[0] = (style == DRAW_INNER) ? DRAW_INNER : DRAW_OUTTER;
  sty[1] = (style == DRAW_INNER) ? DRAW_OUTTER : DRAW_INNER;

  /* Dont draw frame box under frame title */
  if(title) {
    yoff = (fheight>>1);
    xstart = 3;
    xstop = fwidth + 12;
  }
  _draw_rectangular_box_light(im, p, x, (y - yoff), 
			      xstart, xstop,
			      w, (h - yoff), sty[0]);
  
  if(title)
    xstart--;
  
  _draw_rectangular_box_light(im, p, (x + 1), ((y - yoff) + 1), 
			      xstart, xstop,
			      (w - 2), ((h - yoff) - 2), sty[1]);
  
  if(title) {
    XLOCK(im->x.disp);
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
    XDrawString(im->x.disp, p->pixmap, p->gc, (x + 6), y, buf, strlen(buf));
    XUNLOCK(im->x.disp);
    
    xitk_font_unload_font(fs);
  }

}

/*
 *
 */
void draw_inner_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		      int x, int y, int w, int h) {
  _draw_frame(im, p, title, fontname, DRAW_INNER, x, y, w, h);
}
void draw_outter_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		       int x, int y, int w, int h) {
  _draw_frame(im, p, title, fontname, DRAW_OUTTER, x, y, w, h);
}

/*
 *
 */
void draw_tab(ImlibData *im, xitk_image_t *p) {
  int           w, h;

  assert(im && p);

  w = p->width / 3;
  h = p->height;

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, (w * 3), h);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, (w - 1), h);
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w - 1), h);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, w, 3);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, 0, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) - 1, 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, h);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 3, 0, (w * 3) - 1, 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2), 3, (w * 2), (h - 1));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2), 3, (w * 2) + 3, 0);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 1), 3, (w - 1), h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 3) - 1, 0, (w * 3) - 1, h);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, (h - 1), (w * 2) - 1, (h - 1));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_paddle_rotate(ImlibData *im, xitk_image_t *p) {
  int           w;
  int           h;
  unsigned int  ccolor, fcolor, ncolor;

  assert(im && p);
  
  w = p->width/3;
  h = p->height;
  ncolor = xitk_get_pixel_color_darkgray(im);
  fcolor = xitk_get_pixel_color_warning_background(im);
  ccolor = xitk_get_pixel_color_lightgray(im);
  
  {
    int x, i;
    unsigned int bg_colors[3] = { ncolor, fcolor, ccolor };
    
    XLOCK(im->x.disp);
    XSetForeground(im->x.disp, p->mask->gc, 0);
    XFillRectangle(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, w * 3 , h);
    XSetForeground(im->x.disp, p->mask->gc, 1);
    XUNLOCK(im->x.disp);
  
    for(x = 0, i = 0; i < 3; i++) {
      XLOCK(im->x.disp);
      XSetForeground(im->x.disp, p->mask->gc, 1);
      XFillArc(im->x.disp, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc(im->x.disp, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XUNLOCK(im->x.disp);
      
      XLOCK(im->x.disp);
      XSetForeground(im->x.disp, p->image->gc, bg_colors[i]);
      XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
      XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XUNLOCK(im->x.disp);

      x += w;
    }
  }
}

/*
 *
 */
void draw_rotate_button(ImlibData *im, xitk_image_t *p) {
  int           w;
  int           h;
  
  assert(im && p);
  
  w = p->width;
  h = p->height;

  XLOCK(im->x.disp);

  /* Draw mask */
  XSetForeground(im->x.disp, p->mask->gc, 0);
  XFillRectangle(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, w , h);
  
  XSetForeground(im->x.disp, p->mask->gc, 1);
  XFillArc(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));
  XUNLOCK(im->x.disp);
  
  /* */
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillArc(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));

  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  //  XDrawArc(im->x.disp, p->image, p->image->gc, 0, 0, w-1, h-1, (30 * 64), (180 * 64));
  XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, 1, 1, w-2, h-2, (30 * 64), (180 * 64));
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  //  XDrawArc(im->x.disp, p->image, p->image->gc, 0, 0, w-1, h-1, (210 * 64), (180 * 64));
  XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, 1, 1, w-3, h-3, (210 * 64), (180 * 64));
  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_button_plus(ImlibData *im, xitk_image_t *p) {
  int           w, h;
  
  assert(im && p);

  draw_button_minus(im, p);
  
  w = p->width / 3;
  h = p->height;
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
 
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w >> 1) - 1, 2, (w >> 1) - 1, h - 4);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w + (w >> 1) - 1, 2, w + (w >> 1) - 1, h - 4);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + (w >> 1), 3, (w * 2) + (w >> 1), h - 3);

  XUNLOCK(im->x.disp);
}

/*
 *
 */
void draw_button_minus(ImlibData *im, xitk_image_t *p) {
  int           w, h;
  
  assert(im && p);
  
  w = p->width / 3;
  h = p->height;
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
 
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2, (h >> 1) - 1, w - 4, (h >> 1) - 1);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w + 2, (h >> 1) - 1, (w * 2) - 4, (h >> 1) - 1);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 3, h >> 1, (w * 3) - 3, h >> 1);
  
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
  
  i->image         = xitk_image_create_xitk_pixmap(im, img->rgb_width, img->rgb_height);
  XLOCK(im->x.disp);
  i->image->pixmap = Imlib_copy_image(im, img);
  XUNLOCK(im->x.disp);

  i->mask          = xitk_image_create_xitk_mask_pixmap(im, img->rgb_width, img->rgb_height);
  XLOCK(im->x.disp);
  i->mask->pixmap  = Imlib_copy_mask(im, img);
  XUNLOCK(im->x.disp);

  i->width         = img->rgb_width;
  i->height        = img->rgb_height;
  
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
  image_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    private_data = (image_private_data_t *) w->private_data;
    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->skin);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static int notify_inside(xitk_widget_t *w, int x, int y) {
  return 0;
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  image_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    private_data = (image_private_data_t *) w->private_data;
    if(sk == BACKGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static void paint_image (xitk_widget_t *w, Window win, GC gc) {
  image_private_data_t *private_data;
  xitk_image_t         *skin;
  GC                    lgc;
  
  if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (w->visible == 1))) {
    private_data = (image_private_data_t *) w->private_data;

    skin = private_data->skin;
    
    XLOCK (private_data->imlibdata->x.disp);
    lgc = XCreateGC(private_data->imlibdata->x.disp, win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, lgc);
    XUNLOCK (private_data->imlibdata->x.disp);
    
    if (skin->mask) {
      XLOCK (private_data->imlibdata->x.disp);
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, w->x, w->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask->pixmap);
      XUNLOCK (private_data->imlibdata->x.disp);
    }
    
    XLOCK (private_data->imlibdata->x.disp);
    XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, win, lgc, 0, 0,
	       skin->width, skin->height, w->x, w->y);
    
    XFreeGC(private_data->imlibdata->x.disp, lgc);
    XUNLOCK (private_data->imlibdata->x.disp);
  }

}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl,
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  image_private_data_t *private_data;
  
  if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (w->visible == 1))) {
    private_data = (image_private_data_t *) w->private_data;

    if(private_data->skin_element_name) {
      
      xitk_skin_lock(skonfig);

      xitk_image_free_image(private_data->imlibdata, &private_data->skin);
      private_data->skin = xitk_image_load_image(private_data->imlibdata,
						 xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
      
      w->x               = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      w->y               = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->width           = private_data->skin->width;
      w->height          = private_data->skin->height;

      xitk_skin_unlock(skonfig);
      
      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

/*
 *
 */
static xitk_widget_t *_xitk_image_create (xitk_widget_list_t *wl,
					  xitk_skin_config_t *skonfig, 
					  xitk_image_widget_t *im,
					  int x, int y,
					  char *skin_element_name,
					  xitk_image_t *skin) {
  xitk_widget_t              *mywidget;
  image_private_data_t       *private_data;

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (image_private_data_t *) xitk_xmalloc (sizeof (image_private_data_t));

  private_data->imlibdata         = im->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(im->skin_element_name);

  private_data->bWidget           = mywidget;
  private_data->skin              = skin;

  mywidget->private_data          = private_data;

  mywidget->widget_list           = wl;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_IMAGE;
  mywidget->paint                 = paint_image;
  mywidget->notify_click          = NULL;
  mywidget->notify_focus          = NULL;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_inside         = notify_inside;
  mywidget->notify_change_skin    = notify_change_skin;
  mywidget->notify_destroy        = notify_destroy;
  mywidget->get_skin              = get_skin;

  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  return mywidget;
}

xitk_widget_t *xitk_image_create (xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_image_widget_t *im) {

  XITK_CHECK_CONSTITENCY(im);

  return _xitk_image_create(wl, skonfig, im, 
			    (xitk_skin_get_coord_x(skonfig, im->skin_element_name)),
			    (xitk_skin_get_coord_y(skonfig, im->skin_element_name)),
			    im->skin_element_name,
			    (xitk_image_load_image(im->imlibdata,
						   xitk_skin_get_skin_filename(skonfig, 
								       im->skin_element_name))));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
					 xitk_image_widget_t *im, 
					 xitk_image_t *image, int x, int y) {
  XITK_CHECK_CONSTITENCY(im);

  return _xitk_image_create(wl, NULL, im, x, y, NULL, image);
}
