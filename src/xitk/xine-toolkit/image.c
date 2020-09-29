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
#include <X11/Intrinsic.h>
#include <errno.h>

#include <xine/sorted_array.h>

#include "_xitk.h"

#include "utils.h"

typedef struct {
  xitk_widget_t  w;

  char          *skin_element_name;
  xitk_image_t  *skin;
} _image_private_t;

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
unsigned int xitk_get_pixel_color_from_rgb(xitk_t *xitk, int r, int g, int b) {
  XColor       xcolor;
  unsigned int pixcol;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);

  xcolor.flags = DoRed | DoBlue | DoGreen;

  xcolor.red   = r<<8;
  xcolor.green = g<<8;
  xcolor.blue  = b<<8;

  xitk_lock_display (xitk);
  XAllocColor(xitk->display, Imlib_get_colormap(xitk->imlibdata), &xcolor);
  xitk_unlock_display (xitk);

  pixcol = xcolor.pixel;

  return pixcol;
}

/*
 * Some default pixel colors.
 */
unsigned int xitk_get_pixel_color_black(xitk_t *xitk) {
  int user_color = xitk_get_black_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 0, 0, 0);
}
unsigned int xitk_get_pixel_color_white(xitk_t *xitk) {
  int user_color = xitk_get_white_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 255, 255, 255);
}
unsigned int xitk_get_pixel_color_lightgray(xitk_t *xitk) {
  int user_color = xitk_get_focus_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 224, 224, 224);
}
unsigned int xitk_get_pixel_color_gray(xitk_t *xitk) {
  int user_color = xitk_get_background_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 190, 190, 190);
}
unsigned int xitk_get_pixel_color_darkgray(xitk_t *xitk) {
  int user_color = xitk_get_select_color();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 128, 128, 128);
}
unsigned int xitk_get_pixel_color_warning_foreground(xitk_t *xitk) {
  int user_color = xitk_get_warning_foreground();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_black(xitk);
}
unsigned int xitk_get_pixel_color_warning_background(xitk_t *xitk) {
  int user_color = xitk_get_warning_background();
  if(user_color >= 0)
    return (unsigned int) user_color;
  return xitk_get_pixel_color_from_rgb(xitk, 255, 255, 0);
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
  xitk_image_t *image;

  if (!src)
    return;
  image = *src;
  if (!image)
    return;

  image->refs -= 1;
  if (image->refs > 0)
    return;

#ifdef XINE_SARRAY_MODE_UNIQUE
  if (image->wl && image->wl->shared_images) {
    xine_sarray_remove_ptr (image->wl->shared_images, image);
#ifdef DEBUG_SHARED_IMAGE
    printf ("xitk_shared_image: %d uses of %s (%d x %d).\n",
      image->max_refs, image->key, image->width, image->height);
#endif
  }

  if (image->raw) {
    ImlibData *im = image->xitk->imlibdata;
    ABORT_IF_NULL(im);
    xitk_lock_display (image->xitk);
    Imlib_destroy_image (im, image->raw);
    xitk_unlock_display (image->xitk);
  }
#endif

  if (image->mask)
    xitk_image_destroy_xitk_pixmap (image->mask);

  if (image->image)
    xitk_image_destroy_xitk_pixmap (image->image);

  _xitk_image_destroy_pix_font (&image->pix_font);

  XITK_FREE (image);
  *src = NULL;
}

/*
 *
 */
static void xitk_image_xitk_pixmap_destroyer(xitk_pixmap_t *xpix) {
  ABORT_IF_NULL(xpix);

  xitk_lock_display (xpix->xitk);

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

  xitk_unlock_display (xpix->xitk);

  XITK_FREE(xpix);
}

xitk_pixmap_t *xitk_pixmap_create_from_data(xitk_t *xitk, int width, int height, const char *data) {
  xitk_pixmap_t    *xpix;
  XGCValues         gcv;

  xpix = xitk_xmalloc(sizeof(xitk_pixmap_t));
  if (!xpix)
    return NULL;

  xpix->imlibdata = xitk->imlibdata;
  xpix->xitk      = xitk;
  xpix->destroy   = xitk_image_xitk_pixmap_destroyer;
  xpix->width     = width;
  xpix->height    = height;
  xpix->xim       = NULL;
  xpix->shm       = 0;
#ifdef HAVE_SHM
  xpix->shminfo = NULL;
#endif

  gcv.graphics_exposures = False;

  xitk_lock_display (xitk);
  xpix->pixmap = XCreateBitmapFromData (xitk->display, xitk->imlibdata->x.root, data, 40, 40);
  xpix->gc = XCreateGC(xitk->display, xpix->pixmap, GCGraphicsExposures, &gcv);
  xitk_unlock_display (xitk);

  return xpix;
}

Pixmap xitk_pixmap_get_pixmap(xitk_pixmap_t *p) {
  return p->pixmap;
}

int xitk_pixmap_width(xitk_pixmap_t *p) {
  return p->width;
}

int xitk_pixmap_height(xitk_pixmap_t *p) {
  return p->height;
}

int xitk_pixmap_get_pixel(xitk_pixmap_t *p, int x, int y) {
  XImage *xi;
  Pixel pixel;

  xitk_lock_display (p->xitk);
  xi = XGetImage (p->xitk->display, p->pixmap, x, y, 1, 1, AllPlanes, ZPixmap);
  pixel = XGetPixel(xi, 0, 0);
  XDestroyImage(xi);
  xitk_unlock_display (p->xitk);

  return (int) pixel;
}

/*
 *
 */
static xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(xitk_t *xitk, int width, int height, int depth) {
  xitk_pixmap_t    *xpix;
  XGCValues         gcv;
#ifdef HAVE_SHM
  XShmSegmentInfo  *shminfo;
#endif
  ImlibData        *im;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);

  im = xitk->imlibdata;

  xpix            = (xitk_pixmap_t *) xitk_xmalloc(sizeof(xitk_pixmap_t));
  xpix->xitk      = xitk;
  xpix->imlibdata = xitk->imlibdata;
  xpix->destroy   = xitk_image_xitk_pixmap_destroyer;
  xpix->width     = width;
  xpix->height    = height;
  xpix->xim       = NULL;
  xpix->shm       = 0;

  xitk_lock_display (xpix->xitk);

#ifdef HAVE_SHM
  if(xitk_is_use_xshm() == 2) {
    XImage   *xim;
    
    shminfo = (XShmSegmentInfo *) xitk_xmalloc(sizeof(XShmSegmentInfo));
    
    xitk_x_error = 0;
    xitk_install_x_error_handler();
    
    xim = XShmCreateImage (p->xitk->display, im->x.visual, depth, ZPixmap, NULL, shminfo, width, height);
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
    XShmAttach (p->xitk->display, shminfo);
    XSync (p->xitk->display, False);
    
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
    xpix->pixmap = XShmCreatePixmap (p->xitk->display, im->x.base_window, 
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
      gcv.graphics_exposures = False;
      xpix->gc                     = XCreateGC (p->xitk->display, xpix->pixmap, GCGraphicsExposures, &gcv);
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
      xpix->pixmap  = XCreatePixmap (xpix->xitk->display, im->x.base_window, width, height, depth);

      gcv.graphics_exposures = False;
      xpix->gc = XCreateGC (xpix->xitk->display, xpix->pixmap, GCGraphicsExposures, &gcv);
    }
  xitk_unlock_display (xpix->xitk);

  return xpix;
}

xitk_pixmap_t *xitk_image_create_xitk_pixmap(xitk_t *xitk, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(xitk, width, height, xitk->imlibdata->x.depth);
}
xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(xitk_t *xitk, int width, int height) {
  return xitk_image_create_xitk_pixmap_with_depth(xitk, width, height, 1);
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

  xitk_lock_display (p->xitk);
  p = XCreatePixmap (p->xitk->display, im->x.base_window, width, height, 1);
  xitk_unlock_display (p->xitk);

  return p;
}
#endif

void xitk_pixmap_copy_area(xitk_pixmap_t *src, xitk_pixmap_t *dst,
                           int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
  ABORT_IF_NULL(src);
  ABORT_IF_NULL(dst);

  xitk_lock_display (src->xitk);
  XCopyArea (src->xitk->display, src->pixmap, dst->pixmap, dst->gc,
    src_x, src_y, width, height, dst_x, dst_y);
  xitk_unlock_display (src->xitk);
}

/*
 *
 */
void xitk_image_change_image(xitk_image_t *src, xitk_image_t *dest, int width, int height) {
  CHECK_IMAGE(src);
  ABORT_IF_NULL(dest);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);

  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);

  if(src->mask) {
    xitk_image_destroy_xitk_pixmap(src->mask);

    dest->mask = xitk_image_create_xitk_pixmap(src->xitk, width, height);

    xitk_lock_display (src->xitk);
    XCopyArea (src->xitk->display, src->mask->pixmap, dest->mask->pixmap, dest->mask->gc,
      0, 0, width, height, 0, 0);
    xitk_unlock_display (src->xitk);

  }
  else
    dest->mask = NULL;

  if(dest->image)
    xitk_image_destroy_xitk_pixmap(dest->image);

  dest->image = xitk_image_create_xitk_pixmap(src->xitk, width, height);

  xitk_pixmap_copy_area(src->image, dest->image, 0, 0, width, height, 0, 0);

  dest->width = width;
  dest->height = height;
}

/*
 *
 */
xitk_image_t *xitk_image_create_image(xitk_t *xitk, int width, int height) {
  xitk_image_t *i;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);
  ABORT_IF_NOT_COND(width > 0);
  ABORT_IF_NOT_COND(height > 0);

  i = (xitk_image_t *)xitk_xmalloc (sizeof (*i));
  if (!i)
    return NULL;

  i->xitk     = xitk;
  i->image    = xitk_image_create_xitk_pixmap(xitk, width, height);
  if (!i->image) {
    XITK_FREE (i);
    return NULL;
  }
  i->mask     = NULL;
  i->width    = width;
  i->height   = height;
  i->wl       = NULL;
  i->max_refs =
  i->refs     = 1;
  i->key[0]   = 0;

  return i;
}

#ifdef XINE_SARRAY_MODE_UNIQUE
static int _xitk_shared_image_cmp (void *a, void *b) {
  xitk_image_t *d = (xitk_image_t *)a;
  xitk_image_t *e = (xitk_image_t *)b;

  if (d->width < e->width)
    return -1;
  if (d->width > e->width)
    return 1;
  if (d->height < e->height)
    return -1;
  if (d->height > e->height)
    return 1;
  return strcmp (d->key, e->key);
}
#endif

int xitk_shared_image (xitk_widget_list_t *wl, const char *key, int width, int height, xitk_image_t **image) {
  xitk_image_t *i;
  size_t keylen;

  ABORT_IF_NOT_COND (width > 0);
  ABORT_IF_NOT_COND (height > 0);

  if (!image)
    return 0;
  if (!wl || !key) {
    *image = NULL;
    return 0;
  }
  if (!wl->imlibdata) {
    *image = NULL;
    return 0;
  }

  keylen = strlen (key);
  if (keylen > sizeof (i->key) - 1)
    keylen = sizeof (i->key) - 1;
  i = (xitk_image_t *)xitk_xmalloc (sizeof (*i) + keylen);
  if (!i) {
    *image = NULL;
    return 0;
  }
  i->xitk  = wl->xitk;
  i->width = width;
  i->height = height;
  memcpy (i->key, key, keylen);
  i->key[keylen] = 0;

#ifdef XINE_SARRAY_MODE_UNIQUE
  if (!wl->shared_images) {
    wl->shared_images = xine_sarray_new (16, _xitk_shared_image_cmp);
    xine_sarray_set_mode (wl->shared_images, XINE_SARRAY_MODE_UNIQUE);
  }
  if (wl->shared_images) {
    int ai;

    ai = xine_sarray_add (wl->shared_images, i);
    if (ai < 0) {
      XITK_FREE (i);
      i = xine_sarray_get (wl->shared_images, ~ai);
      i->refs += 1;
      if (i->refs > i->max_refs)
        i->max_refs = i->refs;
      *image = i;
      return i->refs;
    }
  }
#endif
  i->image = xitk_image_create_xitk_pixmap (wl->xitk, width, height);
  if (!i->image) {
    XITK_FREE (i);
    return 0;
  }
  i->max_refs = i->refs = 1;
  i->wl = wl;
  i->mask = NULL;
  *image = i;
  return i->refs;
}

void xitk_shared_image_list_delete (xitk_widget_list_t *wl) {
  if (wl && wl->shared_images) {
    int i, max = xine_sarray_size (wl->shared_images);

    for (i = 0; i < max; i++) {
      xitk_image_t *image = xine_sarray_get (wl->shared_images, i);

      image->wl = NULL;
    }

    xine_sarray_delete(wl->shared_images);
    wl->shared_images = NULL;
  }
}

/*
 *
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(xitk_t *xitk,
                                                              const char *fontname,
                                                              int width, int align, const char *str,
                                                              unsigned int foreground,
                                                              unsigned int background) {
  ImlibData      *im;
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

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);
  ABORT_IF_NULL(fontname);
  ABORT_IF_NULL(str);
  ABORT_IF_NOT_COND(width > 0);

  im = xitk->imlibdata;

  xitk_lock_display (xitk);
  gc = XCreateGC (xitk->display, im->x.base_window, None, None);
  xitk_unlock_display (xitk);

  /* Creating an image from an empty string would cause an abort with failed */
  /* condition "width > 0". So we substitute some spaces (one single space   */
  /* may not be enough!). Should only happen in case of error.               */
  if(!*str)
    str = "   ";

  fs = xitk_font_load_font(xitk, fontname);
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

  image = xitk_image_create_image(xitk, width, (height + add_line_spc) * numlines - add_line_spc);
  draw_flat_with_color(image->image, image->width, image->height, background);

  { /* Draw string in image */
    int i, y, x = 0;
    
    xitk_lock_display (xitk);
    XSetForeground (xitk->display, gc, foreground);
    xitk_unlock_display (xitk);
    
    for(y = ascent, i = 0; i < numlines; i++, y += (height + add_line_spc)) {
      xitk_font_string_extent(fs, lines[i], &lbearing, &rbearing, NULL, NULL, NULL); 
      length = rbearing - lbearing;

      if((align == ALIGN_DEFAULT) || (align == ALIGN_LEFT))
        x = 0;
      else if(align == ALIGN_CENTER)
        x = (width - length) >> 1;
      else if(align == ALIGN_RIGHT)
        x = (width - length);
      
      xitk_font_draw_string(fs, image->image, gc,
			    (x - lbearing), y, lines[i], strlen(lines[i]));
			    /*   ^^^^^^^^ Adjust to start of ink */
    }
  }

  xitk_font_unload_font(fs);

  xitk_lock_display (xitk);
  XFreeGC (xitk->display, gc);
  xitk_unlock_display (xitk);

  return image;
}
xitk_image_t *xitk_image_create_image_from_string(xitk_t *xitk,
                                                  const char *fontname,
                                                  int width, int align, const char *str) {

  return xitk_image_create_image_with_colors_from_string(xitk, fontname, width, align, str,
    xitk_get_pixel_color_black(xitk), xitk_get_pixel_color_gray(xitk));
}
/*
 *
 */
void xitk_image_add_mask(xitk_image_t *dest) {

  CHECK_IMAGE(dest);

  if(dest->mask)
    xitk_image_destroy_xitk_pixmap(dest->mask);

  dest->mask = xitk_image_create_xitk_mask_pixmap(dest->xitk, dest->width, dest->height);

  pixmap_fill_rectangle(dest->mask, 0, 0, dest->width, dest->height, 1);
}

void menu_draw_arrow_branch(xitk_image_t *p) {
  int            w;
  int            h;
  XPoint         points[4];
  int            i;
  int            x1, x2, x3;
  int            y1, y2, y3;

  CHECK_IMAGE(p);

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

    pixmap_fill_polygon(p->image, &points[0], 4, xitk_get_pixel_color_black(p->xitk));

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

  CHECK_IMAGE(p);

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

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black (p->xitk));
  xitk_unlock_display (p->xitk);

  for(i = 0; i < 3; i++) {
    
    if(i == 2) {
      for(s = 0; s < nsegments; s++) {
	segments[s].x1++; segments[s].y1++;
	segments[s].x2++; segments[s].y2++;
      }
    }
    
    xitk_lock_display (p->xitk);
    XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, &segments[0], nsegments);
    xitk_unlock_display (p->xitk);

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
  ABORT_IF_NULL(p);

  xitk_lock_display (p->xitk);

  XSetForeground (p->xitk->display, p->gc, color);
  XDrawLine (p->xitk->display, p->pixmap, p->gc, x0, y0, x1, y1);

  xitk_unlock_display (p->xitk);
}

void xitk_image_draw_line(xitk_image_t *i,
                      int x0, int y0, int x1, int y1, unsigned color) {
  pixmap_draw_line(i->image, x0, y0, x1, y1, color);
}

void pixmap_draw_rectangle(xitk_pixmap_t *p, int x, int y, int w, int h, unsigned int color) {
  ABORT_IF_NULL(p);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->gc, color);
  XDrawRectangle (p->xitk->display, p->pixmap, p->gc, x, y, w, h);
  xitk_unlock_display (p->xitk);
}

void pixmap_fill_rectangle(xitk_pixmap_t *p, int x, int y, int w, int h, unsigned int color) {
  ABORT_IF_NULL(p);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->gc, color);
  XFillRectangle (p->xitk->display, p->pixmap, p->gc, x, y, w , h);
  xitk_unlock_display (p->xitk);
}

void xitk_image_fill_rectangle(xitk_image_t *i, int x, int y, int w, int h, unsigned int color) {
  pixmap_fill_rectangle(i->image, x, y, w, h, color);
}

void pixmap_fill_polygon(xitk_pixmap_t *p,
                         XPoint *points, int npoints, unsigned color) {
  ABORT_IF_NULL(p);

  xitk_lock_display (p->xitk);

  XSetForeground (p->xitk->display, p->gc, color);
  XFillPolygon (p->xitk->display, p->pixmap, p->gc, points, npoints, Convex, CoordModeOrigin);

  xitk_unlock_display (p->xitk);
}

void xitk_image_fill_polygon(xitk_image_t *i,
                         XPoint *points, int npoints, unsigned color) {
  pixmap_fill_polygon(i->image, points, npoints, color);
}

/*
 *
 */
static void _draw_rectangular_box (xitk_pixmap_t *p,
  int x, int y, int excstart, int excstop, int width, int height, int type) {
  unsigned int color[2];
  XSegment xs[5], *q;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->xitk);

  color[(type & DRAW_FLATTER) != DRAW_OUTTER] = xitk_get_pixel_color_white (p->xitk);
  color[(type & DRAW_FLATTER) == DRAW_OUTTER] = (type & DRAW_LIGHT)
                                              ? xitk_get_pixel_color_darkgray (p->xitk)
                                              : xitk_get_pixel_color_black (p->xitk);

  /* +---     ----- *              | *
   * |              *              | *
   * |              * -------------+ */
  q = xs;
  if (excstart < excstop) {
    q->x1 = x + 1; q->x2 = x + excstart;        q->y1 = q->y2 = y; q++;
    q->x1 = x + excstop; q->x2 = x + width - 2; q->y1 = q->y2 = y; q++;
  } else {
    q->x1 = x + 1; q->x2 = x + width - 2;       q->y1 = q->y2 = y; q++;
  }
  q->x1 = q->x2 = x; q->y1 = y + 1; q->y2 = y + height - 2; q++;
  if (type & DRAW_DOUBLE) {
    q->x1 = q->x2 = x + width - 2;        q->y1 = y + 2; q->y2 = y + height - 3; q++;
    q->x1 = x + 2; q->x2 = x + width - 3; q->y1 = q->y2 = y + height - 2; q++;
  }
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->gc, color[0]);
  XDrawSegments (p->xitk->display, p->pixmap, p->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);

  /*              | * +---     ----- *
   *              | * |              *
   * -------------+ * |              */
  q = xs;
  q->x1 = q->x2 = x + width - 1;        q->y1 = y + 1; q->y2 = y + height - 2; q++;
  q->x1 = x + 1; q->x2 = x + width - 2; q->y1 = q->y2 = y + height - 1;        q++;
  if (type & DRAW_DOUBLE) {
    if (excstart < excstop) {
      q->x1 = x + 2; q->x2 = x + excstart;        q->y1 = q->y2 = y + 1; q++;
      q->x1 = x + excstop; q->x2 = x + width - 3; q->y1 = q->y2 = y + 1; q++;
    } else {
      q->x1 = x + 2; q->x2 = x + width - 3;       q->y1 = q->y2 = y + 1; q++;
    }
    q->x1 = q->x2 = x + 1; q->y1 = y + 2; q->y2 = y + height - 3; q++;
  }
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->gc, color[1]);
  XDrawSegments (p->xitk->display, p->pixmap, p->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
void draw_rectangular_box (xitk_pixmap_t *p, int x, int y, int width, int height, int type) {
  _draw_rectangular_box (p, x, y, 0, 0, width, height, type);
}
void xitk_image_draw_rectangular_box (xitk_image_t *i, int x, int y, int width, int height, int type) {
  _draw_rectangular_box (i->image, x, y, 0, 0, width, height, type);
}

static void _draw_check_round(xitk_image_t *p, int x, int y, int d, int checked) {
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
  XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, x, y, d, d, (30 * 64), (180 * 64));
  xitk_unlock_display (p->xitk);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_darkgray(p->xitk));
  XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, x, y, d, d, (210 * 64), (180 * 64));
  xitk_unlock_display (p->xitk);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white(p->xitk));
  XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, x + 2, y + 2, d - 4, d - 4, (0 * 64), (360 * 64));
  xitk_unlock_display (p->xitk);

  if(checked) {
    xitk_lock_display (p->xitk);
    XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
    XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, x + 4, y + 4, d - 8, d - 8, (0 * 64), (360 * 64));
    xitk_unlock_display (p->xitk);
  }
}
static void _draw_check_check(xitk_image_t *p, int x, int y, int d, int checked) {
  /* background */
  pixmap_fill_rectangle (p->image, x, y, d, d,
    (checked & 2) ? xitk_get_pixel_color_white (p->xitk) : xitk_get_pixel_color_lightgray (p->xitk));
  /* */
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x, y, x + d, y);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x, y, x, y + d);
  xitk_unlock_display (p->xitk);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_darkgray(p->xitk));
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x, y + d, x + d, y + d);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x + d, y, x + d, y + d);
  xitk_unlock_display (p->xitk);

  if (checked & 1) {
    xitk_lock_display (p->xitk);
    XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
    XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x + (d / 5), (y + ((d / 3) * 2)) - 2, x + (d / 2), y + d - 2);
    XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x + (d / 5)+1, (y + ((d / 3) * 2)) - 2, x + (d / 2) + 1, y + d - 2);
    XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x + (d / 2), y + d - 2, x + d - 2, y+1);
    XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, x + (d / 2) + 1, y + d - 2, x + d - 1, y+1);
    xitk_unlock_display (p->xitk);
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
      _draw_rectangular_box (p->image, 4,               6,     0, 0, 12, h, relief);
      _draw_rectangular_box (p->image, w + 4,           6,     0, 0, 12, h, relief);
      _draw_rectangular_box (p->image, (w * 2) + 4 + 1, 6 + 1, 0, 0, 12, h, nrelief);
    }
    break;
  }
}

/*
 *
 */
static void _draw_three_state(xitk_image_t *p, int style) {
  int           w;
  int           h;
  XSegment      xs[8], *q;

  CHECK_IMAGE(p);

  w = p->width / 3;
  h = p->height;

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray (p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 0, 0, w , h);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_lightgray (p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, w, 0, (w * 2) , h);
  xitk_unlock_display (p->xitk);

  /* +----+----       *      +----       *
   * |    |           *      |           *
   * |    |           *      |           */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = 0 * w; q->x2 = 1 * w; q->y1 = q->y2 = 0; q++;
    q->x1 = q->x2 = 0 * w; q->y1 = 0; q->y2 = h - 1; q++;
  }
  q->x1 = 1 * w; q->x2 = 2 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 1 * w; q->y1 = 0; q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);

  /*     |    |+----  *          |+----  *
   *     |    ||      *          ||      *
   * ----+----+|      *      ----+|      */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 2; q->y1 = 2; q->y2 = h - 3; q++;
    q->x1 = 2; q->x2 = 1 * w - 2; q->y1 = q->y2 = h - 2; q++;
  }
  q->x1 = q->x2 = 2 * w - 2; q->y1 = 2; q->y2 = h - 3; q++;
  q->x1 = 1 * w + 2; q->x2 = 2 * w - 2; q->y1 = q->y2 = h - 2; q++;
  q->x1 = 2 * w + 0; q->x2 = 3 * w + 0; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 2 * w + 0; q->y1 = 0; q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_darkgray (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 2 * w, 0, w - 1, h - 1);
  xitk_unlock_display (p->xitk);

  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 1; q->y1 = 0; q->y2 = h - 1; q++;
    q->x1 = 0; q->x2 = 1 * w - 1; q->y1 = q->y2 = h - 1; q++;
  }
  q->x1 = 2 * w + 1; q->x2 = 3 * w - 1; q->y1 = q->y2 = 1; q++;
  q->x1 = q->x2 = 2 * w + 1; q->y1 = 1; q->y2 = h - 2; q++;
  q->x1 = q->x2 = 2 * w - 1; q->y1 = 0; q->y2 = h; q++;
  q->x1 = 1 * w + 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);

  /*                | *
   *                | *
   *            ----+ */
  q = xs;
  q->x1 = q->x2 = 3 * w - 1; q->y1 = 1; q->y2 = h - 1; q++;
  q->x1 = 2 * w + 1; q->x2 = 3 * w + 0; q->y1 = q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white(p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);

  /* +   ++   ++   + *
   *                 *
   * +   ++   ++   + */
  q = xs;
  q->x1 = q->x2 = 0 * w;            q->y1 = q->y2 = 0; q++;
  q->x1 = 1 * w - 1; q->x2 = 1 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = 2 * w - 1; q->x2 = 2 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 3 * w - 1;        q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 0 * w;            q->y1 = q->y2 = h - 1; q++;
  q->x1 = 1 * w - 1; q->x2 = 1 * w; q->y1 = q->y2 = h - 1; q++;
  q->x1 = 2 * w - 1; q->x2 = 2 * w; q->y1 = q->y2 = h - 1; q++;
  q->x1 = q->x2 = 3 * w - 1;        q->y1 = q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
static void _draw_two_state(xitk_image_t *p, int style) {
  int           w;
  int           h;
  XSegment     xs[3], *q;

  CHECK_IMAGE(p);

  w = p->width / 2;
  h = p->height;

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray(p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 0, 0, w - 1, h - 1);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_lightgray(p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, w, 0, (w * 2) - 1 , h - 1);
  xitk_unlock_display (p->xitk);

  /* +-----+----- *       +----- *
   * |     |      *       |      *
   * |     |      *       |      */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = 0;        q++;
    q->x1 = q->x2 = 0;            q->y1 = 0; q->y2 = h - 1; q++;
  } else {
    q->x1 = w; q->x2 = 2 * w - 1; q->y1 = q->y2 = 0; q++;
  }
  q->x1 = q->x2 = w; q->y1 = 0; q->y2 = h - 1; q++;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);

  /*      |     | *            | *
   *      |     | *            | *
   * -----+-----+ *       -----+ */
  q = xs;
  q->x1 = q->x2 = 2 * w - 1;      q->y1 = 0; q->y2 = h - 1; q++;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 1;    q->y1 = 0; q->y2 = h - 1; q++;
    q->x1 = 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1;    q++;
  } else {
    q->x1 = 1 * w; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1; q++;
  }
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, q - xs);
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
static void _draw_relief(xitk_pixmap_t *p, int w, int h, int type) {

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->imlibdata);

  pixmap_fill_rectangle(p, 0, 0, w, h, xitk_get_pixel_color_gray(p->xitk));

  if (((type & DRAW_FLATTER) == DRAW_OUTTER) || ((type & DRAW_FLATTER) == DRAW_INNER))
    _draw_rectangular_box (p, 0, 0, 0, 0, w, h, type);
}

void draw_checkbox_check(xitk_image_t *p) {
  int  style = xitk_get_checkstyle_feature();

  pixmap_fill_rectangle(p->image, 0, 0, p->width, p->height, xitk_get_pixel_color_gray(p->xitk));

  switch(style) {
    
  case CHECK_STYLE_CHECK:
    if (p->width == p->height * 4) {
      int w = p->width / 4;
      _draw_check_check (p, w * 0, 0, p->height, 0);
      _draw_check_check (p, w * 1, 0, p->height, 1);
      _draw_check_check (p, w * 2, 0, p->height, 2);
      _draw_check_check (p, w * 3, 0, p->height, 3);
    } else {
      int w = p->width / 3;
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
void draw_paddle_three_state (xitk_image_t *p, int width, int height) {
  int           w, h, gap, m, dir;
  XSegment      xs[9];
  XRectangle    xr[3];

  CHECK_IMAGE(p);

  dir = width > height;
  w = p->width / 3;
  if ((width > 0) && (width <= w))
    w = width;
  h = p->height;
  if ((height > 0) && (height <= h))
    h = height;

  gap = (w < 11) || (h < 11) ? 1 : 2;

  /* ------------
   * |  ||  ||  |
   * ------------ */
  /* Draw mask */
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->mask->gc, 0);
  XFillRectangle (p->xitk->display, p->mask->pixmap, p->mask->gc, 0, 0, p->width, p->height);
  xitk_unlock_display (p->xitk);
  xr[0].x = 0 * w + 1; xr[0].y = 1; xr[0].width = w - 2; xr[0].height = h - 2;
  xr[1].x = 1 * w + 1; xr[1].y = 1; xr[1].width = w - 2; xr[1].height = h - 2;
  xr[2].x = 2 * w + 1; xr[2].y = 1; xr[2].width = w - 2; xr[2].height = h - 2;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->mask->gc, 1);
  XFillRectangles (p->xitk->display, p->mask->pixmap, p->mask->gc, xr, 3);
  xitk_unlock_display (p->xitk);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray (p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 0 * w, 0, w, h);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_lightgray (p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 1 * w, 0, w, h);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_darkgray (p->xitk));
  XFillRectangle (p->xitk->display, p->image->pixmap, p->image->gc, 2 * w, 0, w, h);
  xitk_unlock_display (p->xitk);
  /* +---+---
   * |   |       |
   *          ---+ */
  xs[0].x1 = 0 * w + gap + 1; xs[0].x2 = 1 * w - gap - 2; xs[0].y1 = xs[0].y2 = gap;
  xs[1].x1 = 1 * w + gap + 1; xs[1].x2 = 2 * w - gap - 2; xs[1].y1 = xs[1].y2 = gap;
  xs[2].x1 = xs[2].x2 = 0 * w + gap;     xs[2].y1 = gap + 1; xs[2].y2 = h - gap - 2;
  xs[3].x1 = xs[3].x2 = 1 * w + gap;     xs[3].y1 = gap + 1; xs[3].y2 = h - gap - 2;
  xs[4].x1 = xs[4].x2 = 3 * w - gap - 1; xs[4].y1 = gap + 1; xs[4].y2 = h - gap - 2;
  xs[5].x1 = 2 * w + gap + 1; xs[5].x2 = 3 * w - gap - 2; xs[5].y1 = xs[5].y2 = h - gap - 1;
  if (!dir) {
    /*   -    -    -   */
    m = (h - 1) >> 1;
    xs[6].x1 = 0 * w + gap + 3; xs[6].x2 = 1 * w - gap - 4; xs[6].y1 = xs[6].y2 = m;
    xs[7].x1 = 1 * w + gap + 3; xs[7].x2 = 2 * w - gap - 4; xs[7].y1 = xs[7].y2 = m;
    xs[8].x1 = 2 * w + gap + 3; xs[8].x2 = 3 * w - gap - 4; xs[8].y1 = xs[8].y2 = m;
  } else {
    /*   |    |    |   */
    m = (w - 1) >> 1;
    xs[6].x1 = xs[6].x2 = 0 * w + m; xs[6].y1 = gap + 3; xs[6].y2 = h - gap - 4;
    xs[7].x1 = xs[7].x2 = 1 * w + m; xs[7].y1 = gap + 3; xs[7].y2 = h - gap - 4;
    xs[8].x1 = xs[8].x2 = 2 * w + m; xs[8].y1 = gap + 3; xs[8].y2 = h - gap - 4;
  }
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, 9);
  xitk_unlock_display (p->xitk);
  /*         +---
   *    |   ||
   * ---+---+    */
  xs[0].x1 = 2 * w + gap + 1; xs[0].x2 = 3 * w - gap - 2; xs[0].y1 = xs[0].y2 = gap;
  xs[1].x1 = xs[1].x2 = 1 * w - gap - 1; xs[1].y1 = gap + 1; xs[1].y2 = h - gap - 2;
  xs[2].x1 = xs[2].x2 = 2 * w - gap - 1; xs[2].y1 = gap + 1; xs[2].y2 = h - gap - 2;
  xs[3].x1 = xs[3].x2 = 2 * w + gap;     xs[3].y1 = gap + 1; xs[3].y2 = h - gap - 2;
  xs[4].x1 = 0 * w + gap + 1; xs[4].x2 = 1 * w - gap - 2; xs[4].y1 = xs[4].y2 = h - gap - 1;
  xs[5].x1 = 1 * w + gap + 1; xs[5].x2 = 2 * w - gap - 2; xs[5].y1 = xs[5].y2 = h - gap - 1;
  if (!dir) {
    /*   -    -    -   */
    m = ((h - 1) >> 1) + 1;
    xs[6].x1 = 0 * w + gap + 3; xs[6].x2 = 1 * w - gap - 4; xs[6].y1 = xs[6].y2 = m;
    xs[7].x1 = 1 * w + gap + 3; xs[7].x2 = 2 * w - gap - 4; xs[7].y1 = xs[7].y2 = m;
    xs[8].x1 = 2 * w + gap + 3; xs[8].x2 = 3 * w - gap - 4; xs[8].y1 = xs[8].y2 = m;
  } else {
    /*   |    |    |   */
    m = ((w - 1) >> 1) + 1;
    xs[6].x1 = xs[6].x2 = 0 * w + m; xs[6].y1 = gap + 3; xs[6].y2 = h - gap - 4;
    xs[7].x1 = xs[7].x2 = 1 * w + m; xs[7].y1 = gap + 3; xs[7].y2 = h - gap - 4;
    xs[8].x1 = xs[8].x2 = 2 * w + m; xs[8].y1 = gap + 3; xs[8].y2 = h - gap - 4;
  }
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black (p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, 9);
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
void draw_paddle_three_state_vertical(xitk_image_t *p) {
  draw_paddle_three_state (p, 0, p->height);
}
void draw_paddle_three_state_horizontal(xitk_image_t *p) {
  draw_paddle_three_state (p, p->width / 3, 0);
}

/*
 *
 */
void draw_inner(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_INNER);
}
#ifdef YET_UNUSED
void draw_inner_light(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_INNER | DRAW_LIGHT);
}
#endif

/*
 *
 */
void draw_outter(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_OUTTER);
}
#ifdef YET_UNUSED
void draw_outter_light(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_OUTTER | DRAW_LIGHT);
}
#endif

void xitk_image_draw_outter(xitk_image_t *i, int w, int h) {
  _draw_relief(i->image, w, h, DRAW_OUTTER);
}

/*
 *
 */
void draw_flat(xitk_pixmap_t *p, int w, int h) {
  _draw_relief(p, w, h, DRAW_FLATTER | DRAW_LIGHT);
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
  xitk_t        *xitk;
  xitk_font_t   *fs = NULL;
  int            yoff = 0, xstart = 0, xstop = 0;
  int            ascent = 0, descent = 0, lbearing = 0, rbearing = 0;
  int            titlelen = 0;
  const char    *titlebuf = NULL;
  char           buf[BUFSIZ];

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->xitk);

  xitk = p->xitk;

  if(title) {
    int maxinkwidth = (w - 12);

    titlelen = strlen(title);
    titlebuf = title;

    fs = xitk_font_load_font(xitk, (fontname ? fontname : DEFAULT_FONT_12));
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
      if (titlelen > ((int)sizeof (buf) - dotslen - 1)) /* Should never happen, */
	titlelen = (sizeof(buf) - dotslen - 1);  /* just to be sure ...  */
      strlcpy(buf, title, titlelen);
      strcat(buf, dots);
      titlelen += dotslen;
      titlebuf = buf;
    }
  }

  /* Dont draw frame box under frame title */
  if(title) {
    yoff = (ascent >> 1) + 1; /* Roughly v-center outline to ascent part of glyphs */
    xstart = 4 - 1;
    xstop = (rbearing - lbearing) + 8;
  }

  _draw_rectangular_box (p, x, y + yoff, xstart, xstop, w, h - yoff, style | DRAW_DOUBLE | DRAW_LIGHT);

  if(title) {
    xitk_pixmap_draw_string(p, fs,  (x - lbearing + 6), (y + ascent), titlebuf, titlelen,
                            xitk_get_pixel_color_black(p->xitk));
    xitk_font_unload_font(fs);
  }

}

void xitk_pixmap_draw_string(xitk_pixmap_t *p, xitk_font_t *xtfs,
                             int x, int y, const char *text,
                             size_t nbytes, int color) {
  xitk_t *xitk;

  ABORT_IF_NULL(p);
  ABORT_IF_NULL(p->xitk);

  xitk = p->xitk;

  xitk_font_set_font(xtfs, p->gc);
  xitk_lock_display (xitk);
  XSetForeground (xitk->display, p->gc, color);
  xitk_font_draw_string(xtfs, p, p->gc, x, y, text, nbytes);
  xitk_unlock_display (xitk);
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

void xitk_image_draw_inner_frame(xitk_image_t *i, const char *title, const char *fontname,
                                  int x, int y, int w, int h) {
  _draw_frame(i->image, title, fontname, DRAW_INNER, x, y, w, h);
}
void xitk_image_draw_outter_frame(xitk_image_t *i, const char *title, const char *fontname,
                                  int x, int y, int w, int h) {
  _draw_frame(i->image, title, fontname, DRAW_OUTTER, x, y, w, h);
}

/*
 *
 */
void draw_tab(xitk_image_t *p) {
  int           w, h;
  XSegment      xs[10];
  XRectangle    xr[2];

  CHECK_IMAGE(p);

  w = p->width / 3;
  h = p->height;

  xr[0].x = 0 * w; xr[0].width = 2 * w; xr[0].y = 0; xr[0].height = 5;
  xr[1].x = 2 * w; xr[1].width = 1 * w; xr[1].y = 0; xr[1].height = h;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray (p->xitk));
  XFillRectangles (p->xitk->display, p->image->pixmap, p->image->gc, xr, 2);
  xitk_unlock_display (p->xitk);

  xr[0].x = 0 * w + 1; xr[0].width = w - 2; xr[0].y = 4; xr[0].height = h - 4;
  xr[1].x = 1 * w + 1; xr[1].width = w - 2; xr[1].y = 0; xr[1].height = h;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_lightgray (p->xitk));
  XFillRectangles (p->xitk->display, p->image->pixmap, p->image->gc, xr, 2);
  xitk_unlock_display (p->xitk);

  /*          *  /-----  *  /-----  *
   *  /-----  * |        * |        *
   * |        * |        * |        *
   * +------- * +------- * |        */
  xs[0].x1 = 1 * w + 2; xs[0].x2 = 2 * w - 3; xs[0].y1 = xs[0].y2 = 0;
  xs[1].x1 = xs[1].x2 = 1 * w + 1;            xs[1].y1 = xs[1].y2 = 1;
  xs[2].x1 = 2 * w + 2; xs[2].x2 = 3 * w - 3; xs[2].y1 = xs[2].y2 = 0;
  xs[3].x1 = xs[3].x2 = 2 * w + 1;            xs[3].y1 = xs[3].y2 = 1;
  xs[4].x1 = 0 * w + 2; xs[4].x2 = 1 * w - 3; xs[4].y1 = xs[4].y2 = 3;
  xs[5].x1 = xs[5].x2 = 0 * w + 1;            xs[5].y1 = xs[5].y2 = 4;
  xs[6].x1 = xs[6].x2 = 0 * w;                xs[6].y1 = 5; xs[6].y2 = h - 1;
  xs[7].x1 = xs[7].x2 = 1 * w;                xs[7].y1 = 2; xs[7].y2 = h - 1;
  xs[8].x1 = xs[8].x2 = 2 * w;                xs[8].y1 = 2; xs[8].y2 = h - 1;
  xs[9].x1 = 0 * w;     xs[9].x2 = 2 * w - 1; xs[9].y1 = xs[9].y2 = h - 1;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white(p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, 10);
  xitk_unlock_display (p->xitk);

  /*          *          *          *
   *          *        | *        | *
   *        | *        | *        | *
   *        | *        | *        | */
  xs[0].x1 = xs[0].x2 = 1 * w - 1;            xs[0].y1 = 5; xs[0].y2 = h - 1;
  xs[1].x1 = xs[1].x2 = 2 * w - 1;            xs[1].y1 = 2; xs[1].y2 = h - 1;
  xs[2].x1 = xs[2].x2 = 3 * w - 1;            xs[2].y1 = 2; xs[2].y2 = h - 1;
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
  XDrawSegments (p->xitk->display, p->image->pixmap, p->image->gc, xs, 3);
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
void draw_paddle_rotate(xitk_image_t *p) {
  int           w;
  int           h;
  unsigned int  ccolor, fcolor, ncolor;

  CHECK_IMAGE(p);

  w = p->width/3;
  h = p->height;
  ncolor = xitk_get_pixel_color_darkgray(p->xitk);
  fcolor = xitk_get_pixel_color_warning_background(p->xitk);
  ccolor = xitk_get_pixel_color_lightgray(p->xitk);

  {
    int x, i;
    unsigned int bg_colors[3] = { ncolor, fcolor, ccolor };

    pixmap_fill_rectangle(p->mask, 0, 0, w * 3, h, 0);

    for(x = 0, i = 0; i < 3; i++) {
      xitk_lock_display (p->xitk);
      XSetForeground (p->xitk->display, p->mask->gc, 1);
      XFillArc (p->xitk->display, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc (p->xitk->display, p->mask->pixmap, p->mask->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      xitk_unlock_display (p->xitk);
      
      xitk_lock_display (p->xitk);
      XSetForeground (p->xitk->display, p->image->gc, bg_colors[i]);
      XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XDrawArc (p->xitk->display, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
      XDrawArc (p->xitk->display, p->image->pixmap, p->image->gc, x, 0, w-1, h-1, (0 * 64), (360 * 64));
      xitk_unlock_display (p->xitk);

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

  CHECK_IMAGE(p);

  w = p->width;
  h = p->height;

  xitk_lock_display (p->xitk);

  /* Draw mask */
  XSetForeground (p->xitk->display, p->mask->gc, 0);
  XFillRectangle (p->xitk->display, p->mask->pixmap, p->mask->gc, 0, 0, w , h);

  XSetForeground (p->xitk->display, p->mask->gc, 1);
  XFillArc (p->xitk->display, p->mask->pixmap, p->mask->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));
  xitk_unlock_display (p->xitk);

  /* */
  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_gray(p->xitk));
  XFillArc (p->xitk->display, p->image->pixmap, p->image->gc, 0, 0, w-1, h-1, (0 * 64), (360 * 64));

  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_white(p->xitk));
  //  XDrawArc (p->xitk->display, p->image, p->image->gc, 0, 0, w-1, h-1, (30 * 64), (180 * 64));
  XDrawArc (p->xitk->display, p->image->pixmap, p->image->gc, 1, 1, w-2, h-2, (30 * 64), (180 * 64));
  xitk_unlock_display (p->xitk);

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_darkgray(p->xitk));
  //  XDrawArc (p->xitk->display, p->image, p->image->gc, 0, 0, w-1, h-1, (210 * 64), (180 * 64));
  XDrawArc (p->xitk->display, p->image->pixmap, p->image->gc, 1, 1, w-3, h-3, (210 * 64), (180 * 64));
  xitk_unlock_display (p->xitk);
}

/*
 *
 */
void draw_button_plus(xitk_image_t *p) {
  int           w, h;
  CHECK_IMAGE(p);

  draw_button_minus(p);

  w = p->width / 3;
  h = p->height;

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
 
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, (w >> 1) - 1, 2, (w >> 1) - 1, h - 4);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, w + (w >> 1) - 1, 2, w + (w >> 1) - 1, h - 4);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, (w * 2) + (w >> 1), 3, (w * 2) + (w >> 1), h - 3);

  xitk_unlock_display (p->xitk);
}

/*
 *
 */
void draw_button_minus(xitk_image_t *p) {
  int           w, h;

  CHECK_IMAGE(p);

  w = p->width / 3;
  h = p->height;

  xitk_lock_display (p->xitk);
  XSetForeground (p->xitk->display, p->image->gc, xitk_get_pixel_color_black(p->xitk));
 
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, 2, (h >> 1) - 1, w - 4, (h >> 1) - 1);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, w + 2, (h >> 1) - 1, (w * 2) - 4, (h >> 1) - 1);
  XDrawLine (p->xitk->display, p->image->pixmap, p->image->gc, (w * 2) + 3, h >> 1, (w * 3) - 3, h >> 1);

  xitk_unlock_display (p->xitk);
}

/*
 *
 */

static xitk_image_t *_image_new(xitk_t *xitk, ImlibImage *img) {
  xitk_image_t  *i;
  ImlibData     *im = xitk->imlibdata;

  i = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  if (!i) {
    xitk_lock_display (xitk);
    Imlib_destroy_image(im, img);
    xitk_unlock_display (xitk);
    return NULL;
  }
  i->xitk   = xitk;
  i->raw    = img;
  i->width  = img->rgb_width;
  i->height = img->rgb_height;
  i->max_refs =
  i->refs     = 1;
  i->wl       = NULL;
  i->key[0]   = 0;
  return i;
}

xitk_image_t *xitk_image_decode_raw(xitk_t *xitk, const void *data, size_t size) {
  ImlibData *im;
  ImlibImage *img;

  if (size < 4)
    return NULL;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);
  ABORT_IF_NULL(data);

  im = xitk->imlibdata;

  xitk_lock_display (xitk);
  img = Imlib_decode_image(im, data, size);
  xitk_unlock_display (xitk);

  if (!img) {
    XITK_WARNING("%s(): couldn't decode image\n", __FUNCTION__);
    return NULL;
  }

  return _image_new(xitk, img);
}

int xitk_image_render(xitk_image_t *i, int width, int height) {
  ImlibImage *img;
  xitk_t *xitk;
  int ret;

  ABORT_IF_NULL(i);
  ABORT_IF_NULL(i->xitk);
  ABORT_IF_NULL(i->xitk->imlibdata);
  ABORT_IF_NULL(i->raw);

  xitk = i->xitk;
  img = i->raw;

  xitk_lock_display (xitk);
  ret = Imlib_render (xitk->imlibdata, img, width, height);
  xitk_unlock_display (xitk);

  if (!ret) {
    XITK_WARNING("%s(): couldn't render image\n", __FUNCTION__);
    return -1;
  }

  i->image         = xitk_image_create_xitk_pixmap(xitk, width, height);
  i->pix_font      = NULL;
  xitk_lock_display (xitk);
  i->image->pixmap = Imlib_copy_image(xitk->imlibdata, img);
  xitk_unlock_display (xitk);

  if(img->shape_mask) {
    i->mask          = xitk_image_create_xitk_mask_pixmap(xitk, width, height);
    xitk_lock_display (xitk);
    i->mask->pixmap  = Imlib_copy_mask(xitk->imlibdata, img);
    xitk_unlock_display (xitk);
  }
  else {
    i->mask = NULL;
  }

  i->width  = width;
  i->height = height;

  xitk_lock_display (xitk);
  Imlib_destroy_image(i->xitk->imlibdata, i->raw);
  xitk_unlock_display (xitk);

  i->raw = NULL;

  return 0;
}

xitk_image_t *xitk_image_load_image(xitk_t *xitk, const char *image) {
  ImlibImage    *img = NULL;
  xitk_image_t  *i;
  ImlibData     *im;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);

  im = xitk->imlibdata;

  if(image == NULL) {
    XITK_WARNING("image name is NULL\n");
    return NULL;
  }

  xitk_lock_display (xitk);
  img = Imlib_load_image(im, image);
  xitk_unlock_display (xitk);
  if(!img) {
    XITK_WARNING("%s(): couldn't find image %s\n", __FUNCTION__, image);
    return NULL;
  }

  i = _image_new(xitk, img);
  if (!i)
    return NULL;

  if (xitk_image_render(i, img->rgb_width, img->rgb_height) < 0)
    xitk_image_free_image(&i);

  return i;
}

static GC _xitk_image_temp_gc (xitk_widget_list_t *wl) {
  GC lgc;

  if (wl) {
    if (wl->temp_gc && (wl->origin_gc == wl->gc)) {
      lgc = wl->temp_gc;
    } else {
      xitk_lock_display (wl->xitk);
      if (wl->temp_gc)
        XFreeGC (wl->xitk->display, wl->temp_gc);
      wl->temp_gc = lgc = XCreateGC  (wl->xitk->display, wl->win, None, None);
      if (!wl->temp_gc) {
        xitk_unlock_display (wl->xitk);
        return NULL;
      }
      wl->origin_gc = wl->gc;
      XCopyGC (wl->xitk->display, wl->gc, (1 << GCLastBit) - 1, lgc);
      xitk_unlock_display (wl->xitk);
    }
  } else {
    xitk_lock_display (wl->xitk);
    lgc = XCreateGC (wl->xitk->display, wl->win, None, None);
    xitk_unlock_display (wl->xitk);
    if (!lgc)
      return NULL;
  }
  return lgc;
}

void xitk_part_image_copy (xitk_widget_list_t *wl, xitk_part_image_t *from, xitk_part_image_t *to,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
  GC lgc;

  if (!wl || !from || !to)
    return;
  if (!from->image || !to->image)
    return;
  if (!from->image->image || !to->image->image)
    return;

  lgc = _xitk_image_temp_gc (wl);
  if (!lgc)
    return;

  xitk_lock_display (wl->xitk);
  XCopyArea (wl->xitk->display, from->image->image->pixmap, to->image->image->pixmap, lgc,
    from->x + src_x, from->y + src_y, width, height, to->x + dst_x, to->y + dst_y);
  if (!wl)
    XFreeGC (wl->xitk->display, lgc);
  xitk_unlock_display (wl->xitk);
}

void xitk_part_image_draw (xitk_widget_list_t *wl, xitk_part_image_t *origin, xitk_part_image_t *copy,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
  GC lgc;

  if (!wl || !origin)
    return;
  if (!origin->image)
    return;

  if (copy) {
    if (!copy->image)
      return;
  } else {
    copy = origin;
  }

  lgc = _xitk_image_temp_gc (wl);
  if (!lgc)
    return;

  xitk_lock_display (wl->xitk);
  if (origin->image->mask && origin->image->mask->pixmap) {
    /* NOTE: clip origin always refers to the full source image,
     * even with partial draws. */
    XSetClipOrigin (wl->xitk->display, lgc,
      dst_x - origin->x - src_x, dst_y - origin->y - src_y);
    XSetClipMask (wl->xitk->display, lgc, origin->image->mask->pixmap);
    XCopyArea (wl->xitk->display, copy->image->image->pixmap, wl->win, lgc,
      copy->x + src_x, copy->y + src_y, width, height, dst_x, dst_y);
    XSetClipMask (wl->xitk->display, lgc, None);
  } else {
    XCopyArea (wl->xitk->display, copy->image->image->pixmap, wl->win, lgc,
      copy->x + src_x, copy->y + src_y, width, height, dst_x, dst_y);
  }
  if (!wl)
    XFreeGC (wl->xitk->display, lgc);
  xitk_unlock_display (wl->xitk);
}

void xitk_image_draw_image (xitk_widget_list_t *wl, xitk_image_t *img,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y, int sync) {
  GC lgc;

  if (!img->image)
    return;

  lgc = _xitk_image_temp_gc (wl);
  if (!lgc)
    return;

  xitk_lock_display (wl->xitk);
  if (img->mask && img->mask->pixmap) {
    /* NOTE: clip origin always refers to the full source image,
     * even with partial draws. */
    XSetClipOrigin (wl->xitk->display, lgc, dst_x - src_x, dst_y - src_y);
    XSetClipMask (wl->xitk->display, lgc, img->mask->pixmap);
    XCopyArea (wl->xitk->display, img->image->pixmap, wl->win, lgc, src_x, src_y, width, height, dst_x, dst_y);
    XSetClipMask (wl->xitk->display, lgc, None);
  } else {
    XCopyArea (wl->xitk->display, img->image->pixmap, wl->win, lgc, src_x, src_y, width, height, dst_x, dst_y);
  }
  if (sync)
    XSync (wl->xitk->display, False);
  if (!wl)
    XFreeGC (wl->xitk->display, lgc);
  xitk_unlock_display (wl->xitk);
}

int xitk_image_width(xitk_image_t *i) {
  return i->width;
}

int xitk_image_height(xitk_image_t *i) {
  return i->height;
}

/*
 * ********************************************************************************
 *                              Widget specific part
 * ********************************************************************************
 */

/*
 *
 */
static void _notify_destroy (_image_private_t *wp) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    if (!wp->skin_element_name)
      xitk_image_free_image (&(wp->skin));
    XITK_FREE (wp->skin_element_name);
  }
}

/*
 *
 */
static int _notify_inside (_image_private_t *wp, int x, int y) {
  (void)wp;
  (void)x;
  (void)y;
  return 0;
}

/*
 *
 */
static xitk_image_t *_get_skin (_image_private_t *wp, int sk) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    if ((sk == BACKGROUND_SKIN) && wp->skin)
      return wp->skin;
  }
  return NULL;
}

/*
 *
 */
static void _paint_image (_image_private_t *wp, widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.image.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (wp->w.visible == 1)) {
    xitk_image_draw_image (wp->w.wl, wp->skin,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y, 0);
  }
}

/*
 *
 */
static void _notify_change_skin (_image_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (wp->w.visible == 1))) {
    if (wp->skin_element_name) {
      const xitk_skin_element_info_t *info;

      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, wp->skin_element_name);
      if (info) {
        wp->skin = info->pixmap_img.image;
        wp->w.x       = info->x;
        wp->w.y       = info->y;
        wp->w.width   = wp->skin->width;
        wp->w.height  = wp->skin->height;
      }
      xitk_skin_unlock(skonfig);
      
      xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    }
  }
}

static int _notify_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _image_private_t *wp;
  int retval = 0;

  xitk_container (wp, w, w);
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = wp->w.x;
      event->y = wp->w.y;
      event->width = wp->w.width;
      event->height = wp->w.height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      _paint_image (wp, event);
      break;
    case WIDGET_EVENT_INSIDE:
      result->value = _notify_inside (wp, event->x, event->y);
      retval = 1;
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _notify_change_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _notify_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _get_skin (wp, event->skin_layer);
        retval = 1;
      }
      break;
    default: ;
  }
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_image_create (xitk_widget_list_t *wl,
					  xitk_image_widget_t *im,
					  int x, int y,
					  const char *skin_element_name,
					  xitk_image_t *skin) {
  _image_private_t *wp;

  ABORT_IF_NULL(wl);

  wp = (_image_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->skin_element_name = (skin_element_name == NULL) ? NULL : strdup (im->skin_element_name);

  wp->skin              = skin;

  wp->w.enable          = 0;
  wp->w.visible         = 0;
  wp->w.x               = x;
  wp->w.y               = y;
  wp->w.width           = wp->skin->width;
  wp->w.height          = wp->skin->height;
  wp->w.type            = WIDGET_TYPE_IMAGE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = _notify_event;

  return &wp->w;
}

xitk_widget_t *xitk_image_create (xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_image_widget_t *im) {
  const xitk_skin_element_info_t *info;

  XITK_CHECK_CONSTITENCY(im);

  info = xitk_skin_get_info (skonfig, im->skin_element_name);
  if (!info)
    return NULL;
  return _xitk_image_create (wl, im, info->x, info->y, im->skin_element_name, info->pixmap_img.image);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
					 xitk_image_widget_t *im, 
					 xitk_image_t *image, int x, int y) {
  XITK_CHECK_CONSTITENCY(im);

  return _xitk_image_create (wl, im, x, y, NULL, image);
}

int xitk_image_quality (xitk_t *xitk, int qual) {
  return Imlib_gfx_quality (xitk->imlibdata, qual);
}

