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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <errno.h>

#include "_xitk.h"

#include "utils.h"

#define CHECK_IMAGE(p)                          \
  do {                                          \
    ABORT_IF_NULL((p));                         \
    ABORT_IF_NULL((p)->image);                  \
    ABORT_IF_NULL((p)->image->imlibdata);       \
  } while (0)

int xitk_x_error = 0;

/*
 * Get a pixel color from rgb values.
 */
unsigned int xitk_get_pixel_color_from_rgb(ImlibData *im, int r, int g, int b) {
  XColor       xcolor;
  unsigned int pixcol;

  ABORT_IF_NULL(im);
  
  xcolor.flags = DoRed | DoBlue | DoGreen;
  
  xcolor.red   = r<<8;
  xcolor.green = g<<8;
  xcolor.blue  = b<<8;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XAllocColor(im->x.disp, Imlib_get_colormap(im), &xcolor);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

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
  return xitk_get_pixel_color_from_rgb(im, 190, 190, 190);
}
unsigned int xitk_get_pixel_color_darkgray(ImlibData *im) {
  int user_color = xitk_get_select_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(im, 128, 128, 128);
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

static int _xitk_pix_font_find_char (xitk_pix_font_t *pf, xitk_point_t *found, int this_char) {
  int range, n = 0;

  for (range = 0; pf->unicode_ranges[range].first > 0; range++) {
    if ((this_char >= pf->unicode_ranges[range].first) && (this_char <= pf->unicode_ranges[range].last))
      break;
    n += pf->unicode_ranges[range].last - pf->unicode_ranges[range].first + 1;
  }

  if (pf->unicode_ranges[range].first <= 0) {
    *found = pf->unknown;
    return 0;
  }

  n += this_char - pf->unicode_ranges[range].first;
  found->x = (n % pf->chars_per_row) * pf->char_width;
  found->y = (n / pf->chars_per_row) * pf->char_height;
  return 1;
}

void xitk_image_set_pix_font (xitk_image_t *image, const char *format) {
  xitk_pix_font_t *pf;
  int range, total;

  if (!image || !format)
    return;
  if (image->pix_font)
    return;
  image->pix_font = pf = malloc (sizeof (*pf));
  if (!pf)
    return;

  pf->width = image->width;
  pf->height = image->height;

  range = 0;
  total = 0;
  do {
    const uint8_t *p = (const uint8_t *)format;
    int v;
    uint8_t z;
    if (!p)
      break;
    while (1) {
      while (*p && (*p != '('))
        p++;
      if (!(*p))
        break;
      p++;
      if ((*p ^ '0') < 10)
        break;
    }
    if (!(*p))
      break;

    v = 0;
    while ((z = (*p ^ '0')) < 10)
      v = v * 10u + z, p++;
    if (v <= 0)
      break;
    pf->chars_per_row = v;
    if (*p != '!')
      break;
    p++;

    while (1) {
      pf->unicode_ranges[range].last = 0;
      v = 0;
      while ((z = (*p ^ '0')) < 10)
        v = v * 10u + z, p++;
      pf->unicode_ranges[range].first = v;
      if (*p != '-')
        break;
      p++;
      v = 0;
      while ((z = (*p ^ '0')) < 10)
        v = v * 10u + z, p++;
      pf->unicode_ranges[range].last = v;
      if (pf->unicode_ranges[range].last < pf->unicode_ranges[range].first)
        pf->unicode_ranges[range].last = pf->unicode_ranges[range].first;
      total += pf->unicode_ranges[range].last - pf->unicode_ranges[range].first + 1;
      range++;
      if (range >= XITK_MAX_UNICODE_RANGES)
        break;
      if (*p != '!')
        break;
      p++;
    }
  } while (0);

  if (range == 0) {
    pf->chars_per_row = 32;
    pf->unicode_ranges[0].first = 32;
    pf->unicode_ranges[0].last = 127;
    total = 127 - 32 + 1;
    range = 1;
  }

  pf->char_width = pf->width / pf->chars_per_row;
  pf->chars_total = total;
  total = (total + pf->chars_per_row - 1) / pf->chars_per_row;
  pf->char_height = pf->height / total;
  pf->unicode_ranges[range].first = 0;
  pf->unicode_ranges[range].last = 0;

  pf->unknown.x = 0;
  pf->unknown.y = 0;
  _xitk_pix_font_find_char (pf, &pf->unknown, 127);
  _xitk_pix_font_find_char (pf, &pf->space, ' ');
  _xitk_pix_font_find_char (pf, &pf->asterisk, '*');
}

static void _xitk_image_destroy_pix_font (xitk_pix_font_t **pix_font) {
  free (*pix_font);
  *pix_font = NULL;
}

/*
 *
 */
void xitk_image_free_image(xitk_image_t **src) {

  ABORT_IF_NULL(*src);

  if((*src)->mask)
    xitk_image_destroy_xitk_pixmap((*src)->mask);
  
  if((*src)->image)
    xitk_image_destroy_xitk_pixmap((*src)->image);

  _xitk_image_destroy_pix_font (&(*src)->pix_font);

  XITK_FREE((*src));
  *src = NULL;
}

/*
 *
 */
static void xitk_image_xitk_pixmap_destroyer(xitk_pixmap_t *xpix) {
  ABORT_IF_NULL(xpix);

  XLOCK (xpix->imlibdata->x.x_lock_display, xpix->imlibdata->x.disp);
  
  if(xpix->pixmap != None)
    XFreePixmap(xpix->imlibdata->x.disp, xpix->pixmap);
  
  XFreeGC(xpix->imlibdata->x.disp, xpix->gc);
  XSync(xpix->imlibdata->x.disp, False);

#ifdef HAVE_SHM
  if(xpix->shm) {
    XShmSegmentInfo *shminfo = xpix->shminfo;

    XShmDetach(xpix->imlibdata->x.disp, shminfo);
    
    if(xpix->xim)
      XDestroyImage(xpix->xim);
    
    if(shmdt(shminfo->shmaddr) < 0)
      XITK_WARNING("shmdt() failed: '%s'\n", strerror(errno));

    if(shmctl(shminfo->shmid, IPC_RMID, 0) < 0)
      XITK_WARNING("shmctl() failed: '%s'\n", strerror(errno));
    

    free(shminfo);
  }
#endif
  
  XUNLOCK (xpix->imlibdata->x.x_unlock_display, xpix->imlibdata->x.disp);
  
  XITK_FREE(xpix);
}

/*
 *
 */
xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(ImlibData *im, int width, int height, int depth) {
  xitk_pixmap_t    *xpix;
#ifdef HAVE_SHM
  XShmSegmentInfo  *shminfo;
#endif

  ABORT_IF_NULL(im);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);
  
  xpix            = (xitk_pixmap_t *) xitk_xmalloc(sizeof(xitk_pixmap_t));
  xpix->imlibdata = im;
  xpix->destroy   = xitk_image_xitk_pixmap_destroyer;
  xpix->width     = width;
  xpix->height    = height;
  xpix->xim       = NULL;
  xpix->shm       = 0;
  
  XLOCK (im->x.x_lock_display, im->x.disp);

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

    xpix->xim    = xim;
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
      xpix->shm                    = 1;
      xpix->shminfo                = shminfo;
      xpix->gcv.graphics_exposures = False;
      xpix->gc                     = XCreateGC(im->x.disp, xpix->pixmap, GCGraphicsExposures, &xpix->gcv);
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
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return xpix;
}

xitk_pixmap_t *xitk_image_create_xitk_pixmap(ImlibData *im, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(im, width, height, im->x.depth);
}
xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(ImlibData *im, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(im, width, height, 1);
}

void xitk_image_destroy_xitk_pixmap(xitk_pixmap_t *p) {
  ABORT_IF_NULL(p);
  p->destroy(p);
}

#if 0
/*
 *
 */
Pixmap xitk_image_create_mask_pixmap(ImlibData *im, int width, int height) {
  Pixmap p;
  
  ABORT_IF_NULL(im);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  p = XCreatePixmap(im->x.disp, im->x.base_window, width, height, 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return p;
}
#endif

/*
 *
 */
void xitk_image_change_image(xitk_image_t *src, xitk_image_t *dest, int width, int height) {
  ImlibData *im;

  CHECK_IMAGE(src);
  ABORT_IF_NULL(dest);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);

  im = src->image->imlibdata;

  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);
  
  if(src->mask) {
    xitk_image_destroy_xitk_pixmap(src->mask);

    dest->mask = xitk_image_create_xitk_pixmap(im, width, height);

    XLOCK (im->x.x_lock_display, im->x.disp);
    XCopyArea(im->x.disp, src->mask->pixmap, dest->mask->pixmap, dest->mask->gc,
	      0, 0, width, height, 0, 0);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);

  }
  else
    dest->mask = NULL;
  
  if(dest->image)
    xitk_image_destroy_xitk_pixmap(dest->image);
  
  dest->image = xitk_image_create_xitk_pixmap(im, width, height);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XCopyArea(im->x.disp, src->image->pixmap, dest->image->pixmap, dest->image->gc,
	    0, 0, width, height, 0, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  dest->width = width;
  dest->height = height;
}

/*
 *
 */
xitk_image_t *xitk_image_create_image(ImlibData *im, int width, int height) {
  xitk_image_t *i;

  ABORT_IF_NULL(im);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);

  i         = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  i->mask   = NULL;
  i->image  = xitk_image_create_xitk_pixmap(im, width, height);
  i->width  = width;
  i->height = height;

  return i;
}

/*
 *
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(ImlibData *im, 
                                                              const char *fontname,
                                                              int width, int align, const char *str,
                                                              unsigned int foreground,
                                                              unsigned int background) {
  xitk_image_t   *image;
  xitk_font_t    *fs;
  GC              gc;
  int             length, height, lbearing, rbearing, ascent, descent, linel, linew, wlinew, lastws;
  int             maxw = 0;
  const char     *p;
  char           *bp;
  char           *lines[256];
  int             numlines = 0;
  char            buf[BUFSIZ]; /* Could be allocated dynamically for bigger texts */

  int             add_line_spc = 2;

  ABORT_IF_NULL(im);
  ABORT_IF_NULL(fontname);
  ABORT_IF_NULL(str);
  ABORT_IF_NOT_COND(width > 0);

  XLOCK (im->x.x_lock_display, im->x.disp);
  gc = XCreateGC(im->x.disp, im->x.base_window, None, None);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  /* Creating an image from an empty string would cause an abort with failed */
  /* condition "width > 0". So we substitute some spaces (one single space   */
  /* may not be enough!). Should only happen in case of error.               */
  if(!*str)
    str = "   ";

  fs = xitk_font_load_font(im->x.disp, fontname);
  xitk_font_set_font(fs, gc);
  xitk_font_string_extent(fs, str, NULL, NULL, NULL, &ascent, &descent);
  height = ascent + descent;

  p      = str;
  bp     = buf;
  wlinew = linew = lastws = linel = 0;

  /*
   * Create string image using exactly the ink width
   * without left and right spacing measures
   */

  while((*p!='\0') && (bp + linel) < (buf + BUFSIZ - TABULATION_SIZE - 2)) {
    
    switch(*p) {
      
    case '\t':
      {
	int a;
	
	if((linel == 0) || (bp[linel - 1] != ' ')) {
	  lastws = linel;
	  wlinew = linew; /* width if wrapped */
	}
	
	a = TABULATION_SIZE - (linel % TABULATION_SIZE);
	
        while(a--)
          bp[linel++] = ' ';
      }
      break;

      case '\a':
      case '\b':
      case '\f':
      case '\r':
      case '\v':
        break; /* Ignore those */
	
    case '\n':
      lines[numlines++] = bp;
      bp                = bp + linel + 1;
      
      wlinew = lastws = linel = 0;
      
      if(linew > maxw)
	maxw = linew;
      break;
      
    case ' ':
      if((linel == 0) || (bp[linel-1] != ' ')) {
	lastws = linel;
	wlinew = linew; /* width if wrapped */
      }

      /* fall through */
    default:
      bp[linel++] = *p;

      if(!lastws)
	wlinew = linew;
      break;
    }

    bp[linel] = '\0'; /* terminate the string for reading with xitk_font_string_extent or strlen */

    xitk_font_string_extent(fs, bp, &lbearing, &rbearing, NULL, NULL, NULL); 
    if((linew = rbearing - lbearing) > width) {
      
      if(lastws == 0) { /* if we haven't found a whitespace */
        bp[linel]         = bp[linel-1]; /* Move that last character down */
        bp[linel-1]       = 0;
        lines[numlines++] = bp;
        bp                += linel;
        linel             = 1;
        bp[linel]         = 0;
      }
      else {
        char *nextword = (bp + lastws);
        int   wordlen;
	
        while(*nextword == ' ')
          nextword++;
	
        wordlen           = (bp + linel) - nextword;
        bp[lastws]        = '\0';
        lines[numlines++] = bp;
        bp                = bp + lastws + 1;
	
        if(bp != nextword)
          memmove(bp, nextword, wordlen + 1);
	
        linel = wordlen;
        lastws = 0;
      }
      
      if(wlinew > maxw)
        maxw = wlinew;
      
      xitk_font_string_extent(fs, bp, &lbearing, &rbearing, NULL, NULL, NULL); 
      linew = rbearing - lbearing;
    }
    
    p++;
  }
  
  if(linel) { /* In case last chars aren't stored */
    lines[numlines++] = bp;
    
    if(linew > maxw)
      maxw = linew;
  }
  
  /* If default resp. left aligned, we may shrink the image */
  if((align == ALIGN_DEFAULT) || (align == ALIGN_LEFT))
    width = MIN(maxw, width);
  
  image = xitk_image_create_image(im, width, (height + add_line_spc) * numlines - add_line_spc);
  draw_flat_with_color(image->image, image->width, image->height, background);
  
  { /* Draw string in image */
    int i, y, x = 0;
    
    XLOCK (im->x.x_lock_display, im->x.disp);
    XSetForeground(im->x.disp, gc, foreground);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    
    for(y = ascent, i = 0; i < numlines; i++, y += (height + add_line_spc)) {
      xitk_font_string_extent(fs, lines[i], &lbearing, &rbearing, NULL, NULL, NULL); 
      length = rbearing - lbearing;

      if((align == ALIGN_DEFAULT) || (align == ALIGN_LEFT))
        x = 0;
      else if(align == ALIGN_CENTER)
        x = (width - length) >> 1;
      else if(align == ALIGN_RIGHT)
        x = (width - length);
      
      xitk_font_draw_string(fs, image->image->pixmap, gc, 
			    (x - lbearing), y, lines[i], strlen(lines[i]));
			    /*   ^^^^^^^^ Adjust to start of ink */
    }
  }

  xitk_font_unload_font(fs);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XFreeGC(im->x.disp, gc);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return image;
}
xitk_image_t *xitk_image_create_image_from_string(ImlibData *im,
                                                  const char *fontname,
                                                  int width, int align, const char *str) {

  return xitk_image_create_image_with_colors_from_string(im,fontname, width, align, str,
							 xitk_get_pixel_color_black(im),
							 xitk_get_pixel_color_gray(im));
}
/*
 *
 */
void xitk_image_add_mask(xitk_image_t *dest) {
  ImlibData *im;

  CHECK_IMAGE(dest);

  im = dest->image->imlibdata;

  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);

  dest->mask = xitk_image_create_xitk_mask_pixmap(im, dest->width, dest->height);

  pixmap_fill_rectangle(dest->mask, 0, 0, dest->width, dest->height, 1);
}

void menu_draw_arrow_branch(xitk_image_t *p) {
  int            w;
  int            h;
  XPoint         points[4];
  int            i;
  int            x1, x2, x3;
  int            y1, y2, y3;
  ImlibData     *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;
  
  x1 = (w - 5);
  y1 = (h / 2); 

  x2 = (w - 10);
  y2 = ((h / 2) + 5);

  x3 = (w - 10);
  y3 = ((h / 2) - 5);

  for(i = 0; i < 3; i++) {
    
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

    pixmap_fill_polygon(p->image, &points[0], 4, xitk_get_pixel_color_black(im));

    x1 += w;
    x2 += w;
    x3 += w;
  }

}

/*
 *
 */
static void _draw_arrow(xitk_image_t *p, int direction) {
  int            w;
  int            h;
  XSegment      *segments;
  int            nsegments;
  int            i, s;
  int            x1, x2, dx;
  int            y1, y2, dy;
  ImlibData     *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;

  /*
   * XFillPolygon doesn't yield equally shaped arbitrary sized small triangles
   * because of its filling algorithm (see also fill-rule in XCreateGC(3X11)
   * as for which pixels are drawn on the boundary).
   * So we handcraft them using XDrawSegments applying Bresenham's algorithm.
   */

  /* Coords of the enclosing rectangle for the triangle:   */
  /* Pay attention to integer precision loss and calculate */
  /* carefully to obtain symmetrical and centered shapes.  */
  x1 = ((w - 1) / 2) - (w / 4);
  x2 = ((w - 1) / 2) + (w / 4);
  y1 = ((h - 1) / 2) - (h / 4);
  y2 = ((h - 1) / 2) + (h / 4);

  dx = (x2 - x1 + 1);
  dy = (y2 - y1 + 1);

  if(direction == DIRECTION_UP || direction == DIRECTION_DOWN) {
    int y, iy, dd;

    nsegments = dy;
    segments = (XSegment *) calloc(nsegments, sizeof(XSegment));

    if(direction == DIRECTION_DOWN) {
      y = y1; iy = 1;
    }
    else {
      y = y2; iy = -1;
    }
    dx = (dx + 1) / 2;
    dd = 0;
    for(s = 0; s < nsegments; s++) {
      segments[s].y1 = y; segments[s].x1 = x1;
      segments[s].y2 = y; segments[s].x2 = x2;
      y += iy;
      if(dy >= dx) {
	if((dd += dx) >= dy) {
	  x1++; x2--;
	  dd -= dy;
	}
      }
      else {
	do {
	  x1++; x2--;
	} while((dd += dy) < dx);
	dd -= dx;
      }
    }
  }
  else if(direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT) {
    int x, ix, dd;

    nsegments = dx;
    segments = (XSegment *) calloc(nsegments, sizeof(XSegment));

    if(direction == DIRECTION_RIGHT) {
      x = x1; ix = 1;
    }
    else {
      x = x2; ix = -1;
    }
    dy = (dy + 1) / 2;
    dd = 0;
    for(s = 0; s < nsegments; s++) {
      segments[s].x1 = x; segments[s].y1 = y1;
      segments[s].x2 = x; segments[s].y2 = y2;
      x += ix;
      if(dx >= dy) {
	if((dd += dy) >= dx) {
	  y1++; y2--;
	  dd -= dx;
	}
      }
      else {
	do {
	  y1++; y2--;
	} while((dd += dx) < dy);
	dd -= dy;
      }
    }
  }
  else {
    XITK_WARNING("direction '%d' is unhandled.\n", direction);
    return;
  }

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  for(i = 0; i < 3; i++) {
    
    if(i == 2) {
      for(s = 0; s < nsegments; s++) {
	segments[s].x1++; segments[s].y1++;
	segments[s].x2++; segments[s].y2++;
      }
    }
    
    XLOCK (im->x.x_lock_display, im->x.disp);
    XDrawSegments(im->x.disp, p->image->pixmap, p->image->gc, 
		  &segments[0], nsegments);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);

    for(s = 0; s < nsegments; s++) {
      segments[s].x1 += w;
      segments[s].x2 += w;
    }
  }

  free(segments);
}

/*
 *
 */
void draw_arrow_up(xitk_image_t *p) {
  _draw_arrow(p, DIRECTION_UP);
}
void draw_arrow_down(xitk_image_t *p) {
  _draw_arrow(p, DIRECTION_DOWN);
}
void draw_arrow_left(xitk_image_t *p) {
  _draw_arrow(p, DIRECTION_LEFT);
}
void draw_arrow_right(xitk_image_t *p) {
  _draw_arrow(p, DIRECTION_RIGHT);
}

/*
 *
 */


void pixmap_draw_line(xitk_pixmap_t *p,
                      int x0, int y0, int x1, int y1, unsigned color) {
  ImlibData *im = p->imlibdata;

  ABORT_IF_NULL(im);
  ABORT_IF_NULL(p);

  XLOCK (im->x.x_lock_display, im->x.disp);

  XSetForeground (im->x.disp, p->gc, color);
  XDrawLine (im->x.disp, p->pixmap, p->gc, x0, y0, x1, y1);

  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

void pixmap_fill_rectangle(xitk_pixmap_t *p, int x, int y, int w, int h, unsigned int color) {
  ImlibData *im = p->imlibdata;

  ABORT_IF_NULL(im);
  ABORT_IF_NULL(p);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->gc, color);
  XFillRectangle(im->x.disp, p->pixmap, p->gc, x, y, w , h);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

void pixmap_fill_polygon(xitk_pixmap_t *p,
                         XPoint *points, int npoints, unsigned color) {
  ImlibData *im = p->imlibdata;

  ABORT_IF_NULL(im);
  ABORT_IF_NULL(p);

  XLOCK (im->x.x_lock_display, im->x.disp);

  XSetForeground (im->x.disp, p->gc, color);
  XFillPolygon (im->x.disp, p->pixmap, p->gc, points, npoints, Convex, CoordModeOrigin);

  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box(xitk_pixmap_t *p,
				  int x, int y, int excstart, int excstop,
				  int width, int height, int relief) {
  ImlibData *im;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  im = p->imlibdata;

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + excstart), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + excstop), y, (x + width - 1), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + 1), (y + height), (x + width), (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box_light(xitk_pixmap_t *p,
					int x, int y, int excstart, int excstop,
					int width, int height, int relief) {
  ImlibData *im;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  im = p->imlibdata;

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_darkgray(im));

  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + excstart), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + excstop), y, (x + width - 1), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_darkgray(im));
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_white(im));
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + 1), (y + height), (x + width), (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
static void _draw_rectangular_box_with_colors(xitk_pixmap_t *p,
					      int x, int y, int width, int height, 
					      unsigned int lcolor, unsigned int dcolor,
					      int relief) {
  ImlibData *im;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  im = p->imlibdata;

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, lcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, dcolor);

  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, (x + width - 1), y);
  XDrawLine(im->x.disp, p->pixmap, p->gc, x, y, x, (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(relief == DRAW_OUTTER)
    XSetForeground(im->x.disp, p->gc, dcolor);
  else if(relief == DRAW_INNER)
    XSetForeground(im->x.disp, p->gc, lcolor);
  
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + width), y, (x + width), (y + height));
  XDrawLine(im->x.disp, p->pixmap, p->gc, (x + 1), (y + height), (x + width), (y + height));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
void draw_rectangular_inner_box(xitk_pixmap_t *p,
				int x, int y, int width, int height) {
  _draw_rectangular_box(p, x, y, 0, 0, width, height, DRAW_INNER);
}
void draw_rectangular_outter_box(xitk_pixmap_t *p,
				 int x, int y, int width, int height) {
  _draw_rectangular_box(p, x, y, 0, 0, width, height, DRAW_OUTTER);
}
void draw_rectangular_inner_box_light(xitk_pixmap_t *p,
				      int x, int y, int width, int height) {
  _draw_rectangular_box_light(p, x, y, 0, 0, width, height, DRAW_INNER);
}
void draw_rectangular_outter_box_light(xitk_pixmap_t *p,
				       int x, int y, int width, int height) {
  _draw_rectangular_box_light(p, x, y, 0, 0, width, height, DRAW_OUTTER);
}

static void _draw_check_round(xitk_image_t *p, int x, int y, int d, int checked) {
  ImlibData *im = p->image->imlibdata;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x, y, d, d, (30 * 64), (180 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x, y, d, d, (210 * 64), (180 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x + 2, y + 2, d - 4, d - 4, (0 * 64), (360 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  if(checked) {
    XLOCK (im->x.x_lock_display, im->x.disp);
    XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
    XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x + 4, y + 4, d - 8, d - 8, (0 * 64), (360 * 64));
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
}
static void _draw_check_check(xitk_image_t *p, int x, int y, int d, int checked) {
  ImlibData     *im = p->image->imlibdata;

  /* background */
  pixmap_fill_rectangle(p->image, x, y, d, d, xitk_get_pixel_color_lightgray(im));
  /* */
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x, y, x + d, y);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x, y, x, y + d);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x, y + d, x + d, y + d);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x + d, y, x + d, y + d);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  if(checked) {
    XLOCK (im->x.x_lock_display, im->x.disp);
    XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x + (d / 5), (y + ((d / 3) * 2)) - 2, x + (d / 2), y + d - 2);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x + (d / 5)+1, (y + ((d / 3) * 2)) - 2, x + (d / 2) + 1, y + d - 2);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x + (d / 2), y + d - 2, x + d - 2, y+1);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, x + (d / 2) + 1, y + d - 2, x + d - 1, y+1);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
  
}

static void draw_check_three_state_round_style(xitk_image_t *p, int x, int y, int d, int w, int checked) {
  int i;

  CHECK_IMAGE(p);

  for(i = 0; i < 3; i++) {
    if(i == 2) {
      x++;
      y++;
    }

    _draw_check_round(p, x, y, d, checked);
    x += w;
  }
}

static void draw_check_three_state_check_style(xitk_image_t *p, int x, int y, int d, int w, int checked) {
  int i;

  CHECK_IMAGE(p);

  for(i = 0; i < 3; i++) {
    if(i == 2) {
      x++;
      y++;
    }

    _draw_check_check(p, x, y, d, checked);
    x += w;
  }
}

void menu_draw_check(xitk_image_t *p, int checked) {
  int  style = xitk_get_checkstyle_feature();

  CHECK_IMAGE(p);

  switch(style) {
    
  case CHECK_STYLE_CHECK:
    draw_check_three_state_check_style(p, 4, 4, p->height - 8, p->width / 3, checked);
    break;
    
  case CHECK_STYLE_ROUND:
    draw_check_three_state_round_style(p, 4, 4, p->height - 8, p->width / 3, checked);
    break;
    
  case CHECK_STYLE_OLD:
  default: 
    {
      int      relief = (checked) ? DRAW_INNER : DRAW_OUTTER;
      int      nrelief = (checked) ? DRAW_OUTTER : DRAW_INNER;
      int      w, h;
      
      w = p->width / 3;
      h = p->height - 12;
      _draw_rectangular_box(p->image, 4,               6,     0, 0, 12, h, relief);
      _draw_rectangular_box(p->image, w + 4,           6,     0, 0, 12, h, relief);
      _draw_rectangular_box(p->image, (w * 2) + 4 + 1, 6 + 1, 0, 0, 12, h, nrelief);
    }
    break;
  }
}

/*
 *
 */
static void _draw_three_state(xitk_image_t *p, int style) {
  ImlibData    *im;
  int           w;
  int           h;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w , h);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) , h);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, (h - 1));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 2), 2, (w - 2), (h - 3));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2, (h - 2), w-2, (h - 2));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2*w - 2,     2, 2*w - 2, h - 3);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,     w+2, h - 2, 2*w - 2, h - 2);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,   w * 2,     0,   w * 3,     0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc,   w * 2,     0,   w * 2, h - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
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
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 3) - 1, 1, (w * 3) - 1, (h - 1));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 1, (h - 1), (w * 3), (h - 1));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
static void _draw_two_state(xitk_image_t *p, int style) {
  int           w;
  int           h;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 2;
  h = p->height;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w - 1, h - 1);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) - 1 , h - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w, 0);
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, 0, (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2), 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, (h - 1));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  if(style == STYLE_BEVEL) {
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 1), 0 , (w - 1), (h - 1));
    XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0 ,(h - 1), w , (h - 1));
  }
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, (h - 1),(w * 2), (h - 1));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
static void _draw_relief(xitk_pixmap_t *p, int w, int h, int relief, int light) {
  ImlibData *im;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  im = p->imlibdata;

  pixmap_fill_rectangle(p, 0, 0, w, h, xitk_get_pixel_color_gray(im));

  if(relief != DRAW_FLATTER) {
    
    _draw_rectangular_box_with_colors(p, 0, 0, w-1, h-1,
				      xitk_get_pixel_color_white(im),
				      ((light)
				       ? xitk_get_pixel_color_darkgray(im)
				       : xitk_get_pixel_color_black(im)),
				      relief);
  }

}

void draw_checkbox_check(xitk_image_t *p) {
  ImlibData *im = p->image->imlibdata;
  int  style = xitk_get_checkstyle_feature();

  pixmap_fill_rectangle(p->image, 0, 0, p->width, p->height, xitk_get_pixel_color_gray(im));

  switch(style) {
    
  case CHECK_STYLE_CHECK:
    {
      int w;
      
      w = p->width / 3;
      _draw_check_check(p, 0, 0, p->height, 0);
      _draw_check_check(p, w, 0, p->height, 0);
      _draw_check_check(p, w * 2, 0, p->height, 1);
    }
    break;
    
  case CHECK_STYLE_ROUND:
    {
      int w;
      
      w = p->width / 3;
      _draw_check_round(p, 0, 0, p->height, 0);
      _draw_check_round(p, w, 0, p->height, 0);
      _draw_check_round(p, w * 2, 0, p->height, 1);
    }
    break;
    
  case CHECK_STYLE_OLD:
  default: 
    _draw_three_state(p, STYLE_BEVEL);
    break;
  }
}

/*
 *
 */
void draw_flat_three_state(xitk_image_t *p) {
  _draw_three_state(p, STYLE_FLAT);
}

/*
 *
 */
void draw_bevel_three_state(xitk_image_t *p) {
  _draw_three_state(p, STYLE_BEVEL);
}

/*
 *
 */
void draw_bevel_two_state(xitk_image_t *p) {
  _draw_two_state(p, STYLE_BEVEL);
}

/*
 *
 */
static void _draw_paddle_three_state(xitk_image_t *p, int direction) {
  int           w;
  int           h;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;

  XLOCK (im->x.x_lock_display, im->x.disp);
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
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  pixmap_fill_rectangle(p->image, 2, 2, w - 4, (h - 1) - 2, xitk_get_pixel_color_gray(im));
  pixmap_fill_rectangle(p->image, w + 2, 2, (w * 2) - 4, (h - 1) - 2, xitk_get_pixel_color_lightgray(im));
  pixmap_fill_rectangle(p->image, (w * 2) + 2, 2, ((w * 3) - 1) - 4, (h - 1) - 2, xitk_get_pixel_color_darkgray(im));

  _draw_rectangular_box_with_colors(p->image, 2, 2, (w-1)-4, (h-1)-4,
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(p->image, w+2, 2, (w-1)-4, (h-1)-4,
				    xitk_get_pixel_color_white(im),
				    xitk_get_pixel_color_black(im),
				    DRAW_OUTTER);
  _draw_rectangular_box_with_colors(p->image, (w*2)+2, 2, (w-1)-4, (h-1)-4,
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
      XITK_WARNING("direction '%d' is unhandled.\n",direction);
      return;
    }

    for(i = 0; i < 3; i++, offset += w) {
      if(i == 2) { xx++; yy++; }
      draw_rectangular_outter_box(p->image, xx + offset, yy, ww, hh);
    }
  }
  
}

/*
 *
 */
void draw_paddle_three_state_vertical(xitk_image_t *p) {
  _draw_paddle_three_state(p, DIRECTION_UP);
}
void draw_paddle_three_state_horizontal(xitk_image_t *p) {
  _draw_paddle_three_state(p, DIRECTION_LEFT);
}

/*
 *
 */
void draw_inner(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_INNER, 0);
}
void draw_inner_light(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_INNER, 1);
}

/*
 *
 */
void draw_outter(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_OUTTER, 0);
}
void draw_outter_light(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_OUTTER, 1);
}

/*
 *
 */
void draw_flat(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_FLATTER, 1);
}

/*
 *
 */
void draw_flat_with_color(xitk_pixmap_t *p, int w, int h, unsigned int color) {

  pixmap_fill_rectangle(p, 0, 0, w, h, color);
}

/*
 * Draw a frame outline with embedded title.
 */
static void _draw_frame(xitk_pixmap_t *p,
                        const char *title, const char *fontname,
                        int style, int x, int y, int w, int h) {
  ImlibData     *im;
  xitk_font_t   *fs = NULL;
  int            sty[2];
  int            yoff = 0, xstart = 0, xstop = 0;
  int            ascent = 0, descent = 0, lbearing = 0, rbearing = 0;
  int            titlelen = 0;
  const char    *titlebuf = NULL;
  char           buf[BUFSIZ];

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  im = p->imlibdata;

  if(title) {
    int maxinkwidth = (w - 12);

    titlelen = strlen(title);
    titlebuf = title;

    fs = xitk_font_load_font(im->x.disp, (fontname ? fontname : DEFAULT_FONT_12));
    xitk_font_set_font(fs, p->gc);
    xitk_font_text_extent(fs, title, titlelen, &lbearing, &rbearing, NULL, &ascent, &descent);

    /* Limit title to frame width */
    if((rbearing - lbearing) > maxinkwidth) {
      char  dots[]  = "...";
      int   dotslen = strlen(dots);
      int   dotsrbearing;
      int   titlewidth;

      /* Cut title, append dots */
      xitk_font_text_extent(fs, dots, dotslen, NULL, &dotsrbearing, NULL, NULL, NULL);
      do {
	titlelen--;
	xitk_font_text_extent(fs, title, titlelen, NULL, NULL, &titlewidth, NULL, NULL);
	rbearing = titlewidth + dotsrbearing;
      } while((rbearing - lbearing) > maxinkwidth);
      { /* Cut possible incomplete multibyte character at the end */
	int titlewidth1;
	while((titlelen > 0) &&
	      (xitk_font_text_extent(fs, title, (titlelen - 1), NULL, NULL, &titlewidth1, NULL, NULL),
	       titlewidth1 == titlewidth))
	  titlelen--;
      }
      if(titlelen > (sizeof(buf) - dotslen - 1)) /* Should never happen, */
	titlelen = (sizeof(buf) - dotslen - 1);  /* just to be sure ...  */
      strlcpy(buf, title, titlelen);
      strcat(buf, dots);
      titlelen += dotslen;
      titlebuf = buf;
    }
  }

  sty[0] = (style == DRAW_INNER) ? DRAW_INNER : DRAW_OUTTER;
  sty[1] = (style == DRAW_INNER) ? DRAW_OUTTER : DRAW_INNER;

  /* Dont draw frame box under frame title */
  if(title) {
    yoff = (ascent >> 1) + 1; /* Roughly v-center outline to ascent part of glyphs */
    xstart = 4 - 1;
    xstop = (rbearing - lbearing) + 8;
  }

  _draw_rectangular_box_light(p, x, (y + yoff),
			      xstart, xstop,
			      w, (h - yoff), sty[0]);
  
  if(title)
    xstart--, xstop--;
  
  _draw_rectangular_box_light(p, (x + 1), ((y + yoff) + 1),
			      xstart, xstop,
			      (w - 2), ((h - yoff) - 2), sty[1]);
  
  if(title) {
    XLOCK (im->x.x_lock_display, im->x.disp);
    XSetForeground(im->x.disp, p->gc, xitk_get_pixel_color_black(im));
    xitk_font_draw_string(fs, p->pixmap, p->gc, (x - lbearing + 6), (y + ascent), titlebuf, titlelen);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    
    xitk_font_unload_font(fs);
  }

}

/*
 *
 */
void draw_inner_frame(xitk_pixmap_t *p,
                      const char *title, const char *fontname,
                      int x, int y, int w, int h) {
  _draw_frame(p, title, fontname, DRAW_INNER, x, y, w, h);
}
void draw_outter_frame(xitk_pixmap_t *p,
                       const char *title, const char *fontname,
                       int x, int y, int w, int h) {
  _draw_frame(p, title, fontname, DRAW_OUTTER, x, y, w, h);
}

/*
 *
 */
void draw_tab(xitk_image_t *p) {
  ImlibData    *im;
  int           w, h;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, (w * 3), h);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_lightgray(im));
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, (w - 1), h);
  XFillRectangle(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w - 1), h);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, w, 3);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, 3, 0, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, (w * 2) - 1, 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w, 0, w, h);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 3, 0, (w * 3) - 1, 0);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2), 3, (w * 2), (h - 1));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2), 3, (w * 2) + 3, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w - 1), 3, (w - 1), h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) - 1, 0, (w * 2) - 1, h);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 3) - 1, 0, (w * 3) - 1, h);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 0, (h - 1), (w * 2) - 1, (h - 1));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
void draw_paddle_rotate(xitk_image_t *p) {
  int           w;
  int           h;
  unsigned int  ccolor, fcolor, ncolor;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width/3;
  h = p->height;
  ncolor = xitk_get_pixel_color_darkgray(im);
  fcolor = xitk_get_pixel_color_warning_background(im);
  ccolor = xitk_get_pixel_color_lightgray(im);
  
  {
    int x, i;
    unsigned int bg_colors[3] = { ncolor, fcolor, ccolor };

    pixmap_fill_rectangle(p->mask, 0, 0, w * 3, h, 0);

    for(x = 0, i = 0; i < 3; i++) {
      XLOCK (im->x.x_lock_display, im->x.disp);
      XSetForeground(im->x.disp, p->mask->gc, 1);
      XFillArc(im->x.disp, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc(im->x.disp, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XUNLOCK (im->x.x_unlock_display, im->x.disp);
      
      XLOCK (im->x.x_lock_display, im->x.disp);
      XSetForeground(im->x.disp, p->image->gc, bg_colors[i]);
      XFillArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
      XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XUNLOCK (im->x.x_unlock_display, im->x.disp);

      x += w;
    }
  }
}

/*
 *
 */
void draw_rotate_button(xitk_image_t *p) {
  int           w;
  int           h;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width;
  h = p->height;

  XLOCK (im->x.x_lock_display, im->x.disp);

  /* Draw mask */
  XSetForeground(im->x.disp, p->mask->gc, 0);
  XFillRectangle(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, w , h);
  
  XSetForeground(im->x.disp, p->mask->gc, 1);
  XFillArc(im->x.disp, p->mask->pixmap, p->mask->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  /* */
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_gray(im));
  XFillArc(im->x.disp, p->image->pixmap, p->image->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));

  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_white(im));
  //  XDrawArc(im->x.disp, p->image, p->image->gc, 0, 0, w-1, h-1, (30 * 64), (180 * 64));
  XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, 1, 1, w-2, h-2, (30 * 64), (180 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_darkgray(im));
  //  XDrawArc(im->x.disp, p->image, p->image->gc, 0, 0, w-1, h-1, (210 * 64), (180 * 64));
  XDrawArc(im->x.disp, p->image->pixmap, p->image->gc, 1, 1, w-3, h-3, (210 * 64), (180 * 64));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
void draw_button_plus(xitk_image_t *p) {
  int           w, h;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  draw_button_minus(p);
  
  w = p->width / 3;
  h = p->height;
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
 
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w >> 1) - 1, 2, (w >> 1) - 1, h - 4);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w + (w >> 1) - 1, 2, w + (w >> 1) - 1, h - 4);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + (w >> 1), 3, (w * 2) + (w >> 1), h - 3);

  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
void draw_button_minus(xitk_image_t *p) {
  int           w, h;
  ImlibData    *im;

  CHECK_IMAGE(p);

  im = p->image->imlibdata;

  w = p->width / 3;
  h = p->height;
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, p->image->gc, xitk_get_pixel_color_black(im));
 
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, 2, (h >> 1) - 1, w - 4, (h >> 1) - 1);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, w + 2, (h >> 1) - 1, (w * 2) - 4, (h >> 1) - 1);
  XDrawLine(im->x.disp, p->image->pixmap, p->image->gc, (w * 2) + 3, h >> 1, (w * 3) - 3, h >> 1);
  
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 *
 */
xitk_image_t *xitk_image_load_image(ImlibData *im, const char *image) {
  ImlibImage    *img = NULL;
  xitk_image_t  *i;

  ABORT_IF_NULL(im);

  if(image == NULL) {
    XITK_WARNING("image name is NULL\n");
    return NULL;
  }

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(!(img = Imlib_load_image(im, (char *)image))) {
    XITK_WARNING("%s(): couldn't find image %s\n", __FUNCTION__, image);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    return NULL;
  }
  
  Imlib_render (im, img, img->rgb_width, img->rgb_height);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  i = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  i->image         = xitk_image_create_xitk_pixmap(im, img->rgb_width, img->rgb_height);
  i->pix_font      = NULL;
  XLOCK (im->x.x_lock_display, im->x.disp);
  i->image->pixmap = Imlib_copy_image(im, img);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  if(img->shape_mask) {
    i->mask          = xitk_image_create_xitk_mask_pixmap(im, img->rgb_width, img->rgb_height);
    XLOCK (im->x.x_lock_display, im->x.disp);
    i->mask->pixmap  = Imlib_copy_mask(im, img);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
  else {
    i->mask = NULL;
  }
    
  i->width         = img->rgb_width;
  i->height        = img->rgb_height;
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  Imlib_destroy_image(im, img);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  return i;
} 

void xitk_image_draw_image (xitk_widget_list_t *wl, xitk_image_t *img,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
  ImlibData *im;
  GC lgc;

  if (!img->image)
    return;

  im = img->image->imlibdata;

  if (wl) {
    if (wl->temp_gc && (wl->origin_gc == wl->gc)) {
      lgc = wl->temp_gc;
    } else {
      XLOCK (im->x.x_lock_display, im->x.disp);
      if (wl->temp_gc)
        XFreeGC (im->x.disp, wl->temp_gc);
      wl->temp_gc = lgc = XCreateGC (im->x.disp, wl->win, None, None);
      if (!wl->temp_gc) {
        XUNLOCK (im->x.x_unlock_display, im->x.disp);
        return;
      }
      wl->origin_gc = wl->gc;
      XCopyGC (im->x.disp, wl->gc, (1 << GCLastBit) - 1, lgc);
      XUNLOCK (im->x.x_unlock_display, im->x.disp);
    }
  } else {
    XLOCK (im->x.x_lock_display, im->x.disp);
    lgc = XCreateGC (im->x.disp, wl->win, None, None);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    if (!lgc)
      return;
  }

  XLOCK (im->x.x_lock_display, im->x.disp);
  if (img->mask && img->mask->pixmap) {
    /* NOTE: clip origin always refers to the full source image,
     * even with partial draws. */
    XSetClipOrigin (im->x.disp, lgc, dst_x - src_x, dst_y - src_y);
    XSetClipMask (im->x.disp, lgc, img->mask->pixmap);
    XCopyArea (im->x.disp, img->image->pixmap, wl->win, lgc, src_x, src_y, width, height, dst_x, dst_y);
    XSetClipMask (im->x.disp, lgc, None);
  } else {
    XCopyArea (im->x.disp, img->image->pixmap, wl->win, lgc, src_x, src_y, width, height, dst_x, dst_y);
  }
  if (!wl)
    XFreeGC (im->x.disp, lgc);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}


/*
 * ********************************************************************************
 *                              Widget specific part
 * ********************************************************************************
 */

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  image_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    private_data = (image_private_data_t *) w->private_data;

    if(!private_data->skin_element_name)
      xitk_image_free_image(&(private_data->skin));

    XITK_FREE(private_data->skin_element_name);
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
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
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
static void paint_image (xitk_widget_t *w, widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.image.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && w->visible == 1)) {
    image_private_data_t *private_data = (image_private_data_t *) w->private_data;

    xitk_image_draw_image(w->wl, private_data->skin,
        event->x - w->x, event->y - w->y,
        event->width, event->height,
        event->x, event->y);
  }
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  image_private_data_t *private_data;
  
  if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && w->visible == 1)) {
    private_data = (image_private_data_t *) w->private_data;

    if(private_data->skin_element_name) {
      
      xitk_skin_lock(skonfig);

      private_data->skin = xitk_skin_get_image(skonfig,
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

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;

  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    event->x = w->x;
    event->y = w->y;
    event->width = w->width;
    event->height = w->height;
    /* fall through */
  case WIDGET_EVENT_PARTIAL_PAINT:
    paint_image (w, event);
    break;
  case WIDGET_EVENT_INSIDE:
    result->value = notify_inside(w, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_GET_SKIN:
    if(result) {
      result->image = get_skin(w, event->skin_layer);
      retval = 1;
    }
    break;
  }
  
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_image_create (xitk_widget_list_t *wl,
					  xitk_skin_config_t *skonfig, 
					  xitk_image_widget_t *im,
					  int x, int y,
					  const char *skin_element_name,
					  xitk_image_t *skin) {
  xitk_widget_t              *mywidget;
  image_private_data_t       *private_data;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (image_private_data_t *) xitk_xmalloc (sizeof (image_private_data_t));

  private_data->imlibdata         = wl->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(im->skin_element_name);

  private_data->bWidget           = mywidget;
  private_data->skin              = skin;

  mywidget->private_data          = private_data;

  mywidget->wl                    = wl;

  mywidget->enable                = 0;
  mywidget->running               = 1;
  mywidget->visible               = 0;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width;
  mywidget->height                = private_data->skin->height;
  mywidget->type                  = WIDGET_TYPE_IMAGE | WIDGET_PARTIAL_PAINTABLE;
  mywidget->event                 = notify_event;
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
			    (xitk_skin_get_image(skonfig,
						 xitk_skin_get_skin_filename(skonfig, im->skin_element_name))));
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
