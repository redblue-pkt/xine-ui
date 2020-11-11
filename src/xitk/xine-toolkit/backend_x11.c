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
 * xiTK X11 backend.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#define MWM_HINTS_DECORATIONS       (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS     5
typedef struct _mwmhints {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long          input_mode;
  unsigned long status;
} MWMHints;

#include <xine/sorted_array.h>

#include "_xitk.h"
#include "xitkintl.h"
#include "backend.h"
#include "dump_x11.h"
#include "dnd.h"

#define _XITK_X11_BE_MAGIC      (('x' << 24) | ('1' << 16) | ('1' << 8) | 'b')
#define _XITK_X11_IMAGE_MAGIC   (('x' << 24) | ('1' << 16) | ('1' << 8) | 'i')
#define _XITK_X11_WINDOW_MAGIC  (('x' << 24) | ('1' << 16) | ('1' << 8) | 'w')
#define _XITK_X11_DISPLAY_MAGIC (('x' << 24) | ('1' << 16) | ('1' << 8) | 'd')

typedef struct xitk_x11_image_s xitk_x11_image_t;
typedef struct xitk_x11_window_s xitk_x11_window_t;
typedef struct xitk_x11_display_s xitk_x11_display_t;
typedef struct xitk_x11_backend_s xitk_x11_backend_t;

struct xitk_x11_backend_s {
  xitk_backend_t be;

  pthread_mutex_t mutex;
  int refs;
};

typedef enum {
  XITK_A_WIN_LAYER = 0,
  XITK_A__MOTIF_WM_HINTS,
  XITK_A_WM_CHANGE_STATE,
  XITK_A_WM_STATE,
  XITK_A_WM_DELETE_WINDOW,
  XITK_A_WM_PROTOCOLS,
  XITK_A__NET_WM_NAME,
  XITK_A__NET_WM_WINDOW_TYPE,
  XITK_A__NET_WM_WINDOW_TYPE_DESKTOP,
  XITK_A__NET_WM_WINDOW_TYPE_DOCK,
  XITK_A__NET_WM_WINDOW_TYPE_TOOLBAR,
  XITK_A__NET_WM_WINDOW_TYPE_MENU,
  XITK_A__NET_WM_WINDOW_TYPE_UTILITY,
  XITK_A__NET_WM_WINDOW_TYPE_SPLASH,
  XITK_A__NET_WM_WINDOW_TYPE_DIALOG,
  XITK_A__NET_WM_WINDOW_TYPE_NORMAL,
  XITK_A__NET_WM_STATE,
  XITK_A__NET_WM_STATE_MODAL,
  XITK_A__NET_WM_STATE_STICKY,
  XITK_A__NET_WM_STATE_MAXIMIZED_VERT,
  XITK_A__NET_WM_STATE_MAXIMIZED_HORZ,
  XITK_A__NET_WM_STATE_SHADED,
  XITK_A__NET_WM_STATE_SKIP_TASKBAR,
  XITK_A__NET_WM_STATE_SKIP_PAGER,
  XITK_A__NET_WM_STATE_HIDDEN,
  XITK_A__NET_WM_STATE_FULLSCREEN,
  XITK_A__NET_WM_STATE_ABOVE,
  XITK_A__NET_WM_STATE_BELOW,
  XITK_A__NET_WM_STATE_DEMANDS_ATTENTION,
  XITK_A_LAST
} xitk_x11_atoms_t;

typedef enum {
  XITK_A_null = 0,
  XITK_A_atom,
  XITK_A_timestamp,
  XITK_A_integer,
  XITK_A_c_string,
  XITK_A_string,
  XITK_A_utf8_string,
  XITK_A_text,
  XITK_A_filename,
  XITK_A_net_address,
  XITK_A_window,
  XITK_A_clipboard,
  XITK_A_length,
  XITK_A_targets,
  XITK_A_multiple,
  XITK_A__NET_SUPPORTING_WM_CHECK,
  XITK_A_clip_end
} xitk_x11_clip_atom_t;

typedef enum {
  XITK_X11_WT_BUFSIZE = 0,
  XITK_X11_WT_X,
  XITK_X11_WT_Y,
  XITK_X11_WT_W,
  XITK_X11_WT_H,
  XITK_X11_WT_IMAGE,
  XITK_X11_WT_PARENT,
  XITK_X11_WT_TRANSIENT_FOR,
  XITK_X11_WT_WRAP,
  XITK_X11_WT_ICON,
  XITK_X11_WT_WIN_FLAGS,
  XITK_X11_WT_LAYER_ABOVE,
  XITK_X11_WT_TITLE,
  XITK_X11_WT_RES_NAME,
  XITK_X11_WT_RES_CLASS,
  XITK_X11_WT_END,
  XITK_X11_WT_LAST
} xitk_x11_wt_t;

static const xitk_tagitem_t _xitk_x11_window_defaults[XITK_X11_WT_LAST] = {
  [XITK_X11_WT_BUFSIZE]       = {XITK_TAG_BUFSIZE, 256},
  [XITK_X11_WT_X]             = {XITK_TAG_X, 0},
  [XITK_X11_WT_Y]             = {XITK_TAG_Y, 0},
  [XITK_X11_WT_W]             = {XITK_TAG_WIDTH, 320},
  [XITK_X11_WT_H]             = {XITK_TAG_HEIGHT, 240},
  [XITK_X11_WT_IMAGE]         = {XITK_TAG_IMAGE, (uintptr_t)NULL},
  [XITK_X11_WT_PARENT]        = {XITK_TAG_PARENT, (uintptr_t)NULL},
  [XITK_X11_WT_TRANSIENT_FOR] = {XITK_TAG_TRANSIENT_FOR, (uintptr_t)NULL},
  [XITK_X11_WT_WRAP]          = {XITK_TAG_WRAP, None},
  [XITK_X11_WT_ICON]          = {XITK_TAG_ICON, (uintptr_t)NULL},
  [XITK_X11_WT_WIN_FLAGS]     = {XITK_TAG_WIN_FLAGS, 0},
  [XITK_X11_WT_LAYER_ABOVE]   = {XITK_TAG_LAYER_ABOVE, 0},
  [XITK_X11_WT_TITLE]         = {XITK_TAG_TITLE, 0},
  [XITK_X11_WT_RES_NAME]      = {XITK_TAG_RES_NAME, (uintptr_t)NULL},
  [XITK_X11_WT_RES_CLASS]     = {XITK_TAG_RES_CLASS, (uintptr_t)NULL},
  [XITK_X11_WT_END]           = {XITK_TAG_END, 0}
};

struct xitk_x11_window_s {
  xitk_be_window_t w;

  xitk_x11_display_t *d;
  xitk_dnd_t *dnd;
  GC gc;
  xitk_tagitem_t props[XITK_X11_WT_LAST];
  char title[256];
};

typedef enum {
  XITK_X11_IT_W = 0,
  XITK_X11_IT_H,
  XITK_X11_IT_MASK,
  XITK_X11_IT_FILENAME,
  XITK_X11_IT_FILEBUF,
  XITK_X11_IT_FILESIZE,
  XITK_X11_IT_RAW,
  XITK_X11_IT_END,
  XITK_X11_IT_LAST
} xitk_x11_it_t;

static const xitk_tagitem_t _xitk_x11_image_defaults[XITK_X11_IT_LAST] = {
  [XITK_X11_IT_W] = {XITK_TAG_WIDTH, 0},
  [XITK_X11_IT_H] = {XITK_TAG_HEIGHT, 0},
  [XITK_X11_IT_MASK] = {XITK_TAG_MASK, 0},
  [XITK_X11_IT_FILENAME] = {XITK_TAG_FILENAME, (uintptr_t)NULL},
  [XITK_X11_IT_FILEBUF] = {XITK_TAG_FILEBUF, (uintptr_t)NULL},
  [XITK_X11_IT_FILESIZE] = {XITK_TAG_FILESIZE, 0},
  [XITK_X11_IT_RAW] = {XITK_TAG_RAW, (uintptr_t)NULL},
  [XITK_X11_IT_END] = {XITK_TAG_END, 0}
};

struct xitk_x11_image_s {
  xitk_be_image_t img;

  xitk_x11_display_t *d;
  struct {
    int x, y;
    Pixmap id;
  } shared_mask;
  xitk_tagitem_t props[XITK_X11_IT_LAST];
};

struct xitk_x11_display_s {
  xitk_be_display_t d;

  xitk_x11_backend_t *be;
  pthread_mutex_t mutex;
  int refs;

  int fd;
  Display *display;

  xitk_x11_window_t *dnd_win;

  xitk_dlist_t free_windows;
  xitk_x11_window_t windows[32];

  int default_screen;
  Atom atoms[XITK_A_LAST];

  struct {
    char   *text;
    int     text_len;
    Window  window_in, window_out;
    Atom    req, prop, target, dummy;
    Time    own_time;
    Atom    atoms[XITK_A_clip_end];
  } clipboard;

  GC gc1, gc2;

  xine_sarray_t *ctrl_keysyms1;
  uint8_t        ctrl_keysyms2[XITK_KEY_LASTCODE];

  XImage *testpix;

  char utf8[2048], name[256];
};

static void _xitk_x11_backend_delete (xitk_x11_backend_t *be) {
  pthread_mutex_unlock (&be->mutex);
  pthread_mutex_destroy (&be->mutex);
  free (be);
}

static int _xitk_x11_backend_unref (xitk_x11_backend_t *be) {
  if (--be->refs != 0)
    return 1;
  _xitk_x11_backend_delete (be);
  return 0;
}

static void _xitk_x11_clipboard_deinit (xitk_x11_display_t *d) {
  free (d->clipboard.text);
  d->clipboard.text = NULL;
  d->clipboard.text_len = 0;
}

static void _xitk_x11_display_delete (xitk_x11_display_t *d) {
  xitk_x11_backend_t *be = d->be;

  _xitk_x11_clipboard_deinit (d);

  pthread_mutex_lock (&be->mutex);
  xitk_dnode_remove (&d->d.node);
  pthread_mutex_unlock (&be->mutex);

  xine_sarray_delete (d->ctrl_keysyms1);
  d->ctrl_keysyms1 = NULL;

  if (d->testpix) {
    XDestroyImage (d->testpix);
    d->testpix = NULL;
  }

  if (d->gc1 || d->gc2) {
    d->d.lock (&d->d);
    if (d->gc1) {
        XFreeGC (d->display, d->gc1);
        d->gc1 = NULL;
    }
    if (d->gc2) {
        XFreeGC (d->display, d->gc2);
        d->gc2 = NULL;
    }
    d->d.unlock (&d->d);
  }

  XCloseDisplay (d->display);
  d->display = NULL;
  d->fd = -1;

  pthread_mutex_destroy (&d->mutex);

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.display.delete (%p).\n", (void *)d);

  d->be = NULL;

  pthread_mutex_lock (&be->mutex);
  if (_xitk_x11_backend_unref (be))
    pthread_mutex_unlock (&be->mutex);
  free (d);
}

static int _xitk_x11_display_unref (xitk_x11_display_t *d) {
  if (--d->refs != 0) {
    return 1;
  }
  pthread_mutex_unlock (&d->mutex);
  _xitk_x11_display_delete (d);
  return 0;
}

static void xitk_x11_display_close (xitk_be_display_t **_d) {
  xitk_x11_display_t *d;

  if (!_d)
    return;
  xitk_container (d, *_d, d);
  *_d = NULL;
  pthread_mutex_lock (&d->mutex);
  if (_xitk_x11_display_unref (d))
    pthread_mutex_unlock (&d->mutex);
}

static void _xitk_x11_display_gc (xitk_x11_display_t *d, xitk_x11_image_t *img) {
  XGCValues gcv;

  if (!d->gc1) {
    gcv.graphics_exposures = False;
    d->gc1 = XCreateGC (d->display, img->img.id1, GCGraphicsExposures, &gcv);
  }
  if (!d->gc2 && (img->img.id2 != None)) {
    gcv.graphics_exposures = False;
    d->gc2 = XCreateGC (d->display, img->img.id2, GCGraphicsExposures, &gcv);
  }
}

static void _xitk_x11_image_add_mask (xitk_x11_image_t *img) {
  xitk_x11_display_t *d = img->d;

  img->img.id2 = XCreatePixmap (d->display, img->img.id1, img->props[XITK_X11_IT_W].value,
    img->props[XITK_X11_IT_H].value, 1);
  if (img->img.id2 != None) {
    img->props[XITK_X11_IT_MASK].value = XITK_BE_MASK_YES;
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, 1);
    XFillRectangle (d->display, img->img.id2, d->gc2, 0, 0, img->props[XITK_X11_IT_W].value,
      img->props[XITK_X11_IT_H].value);
  }
}

static void xitk_x11_image_draw_lines (xitk_be_image_t *_image, const xitk_be_line_t *lines, int num_lines, uint32_t color, int mask) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;
  XSegment xs[40];
  int i;

  if (!lines || (num_lines <= 0))
    return;
  xitk_container (img, _image, img);
  if (!img)
    return;
  d = img->d;
  if (num_lines > (int)(sizeof (xs) / sizeof (xs[0])))
    num_lines = sizeof (xs) / sizeof (xs[0]);
  for (i = 0; i < num_lines; i++) {
    xs[i].x1 = lines[i].x1;
    xs[i].y1 = lines[i].y1;
    xs[i].x2 = lines[i].x2;
    xs[i].y2 = lines[i].y2;
  }

  if (mask) {
    if (img->img.id2 == None)
      _xitk_x11_image_add_mask (img);
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, color);
    XDrawSegments (d->display, img->img.id2, d->gc2, xs, num_lines);
  } else {
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc1, color);
    XDrawSegments (d->display, img->img.id1, d->gc1, xs, num_lines);
  }
}
  
static void xitk_x11_image_fill_rects (xitk_be_image_t *_image, const xitk_be_rect_t *rects, int num_rects, uint32_t color, int mask) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;
  XRectangle xr[40];
  int i;

  if (!rects || (num_rects <= 0))
    return;
  xitk_container (img, _image, img);
  if (!img)
    return;
  d = img->d;
  if (num_rects > (int)(sizeof (xr) / sizeof (xr[0])))
    num_rects = sizeof (xr) / sizeof (xr[0]);
  for (i = 0; i < num_rects; i++) {
    xr[i].x = rects[i].x;
    xr[i].y = rects[i].y;
    xr[i].width = rects[i].w;
    xr[i].height = rects[i].h;
  }

  if (mask) {
    if (img->img.id2 == None)
      _xitk_x11_image_add_mask (img);
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, color);
    XFillRectangles (d->display, img->img.id2, d->gc2, xr, num_rects);
  } else {
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc1, color);
    XFillRectangles (d->display, img->img.id1, d->gc1, xr, num_rects);
  }
}
  
static void xitk_x11_image_draw_arc (xitk_be_image_t *_img, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
  uint16_t a1, uint16_t a2, uint32_t color, int mask) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;

  xitk_container (img, _img, img);
  if (!img)
    return;
  d = img->d;

  if (mask) {
    if (img->img.id2 == None)
      _xitk_x11_image_add_mask (img);
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, color);
    XDrawArc (d->display, img->img.id2, d->gc2, x, y, w, h, a1, a2);
  } else {
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc1, color);
    XDrawArc (d->display, img->img.id1, d->gc1, x, y, w, h, a1, a2);
  }
}

static void xitk_x11_image_fill_arc (xitk_be_image_t *_img, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
  uint16_t a1, uint16_t a2, uint32_t color, int mask) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;

  xitk_container (img, _img, img);
  if (!img)
    return;
  d = img->d;

  if (mask) {
    if (img->img.id2 == None)
      _xitk_x11_image_add_mask (img);
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, color);
    XFillArc (d->display, img->img.id2, d->gc2, x, y, w, h, a1, a2);
  } else {
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc1, color);
    XFillArc (d->display, img->img.id1, d->gc1, x, y, w, h, a1, a2);
  }
}

static void xitk_x11_image_fill_polygon (xitk_be_image_t *_image,
  const xitk_point_t *points, int num_points, uint32_t color, int mask) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;
  XPoint xs[40];
  int i;

  if (!points || (num_points <= 0))
    return;
  xitk_container (img, _image, img);
  if (!img)
    return;
  d = img->d;
  if (num_points > (int)(sizeof (xs) / sizeof (xs[0])))
    num_points = sizeof (xs) / sizeof (xs[0]);
  for (i = 0; i < num_points; i++) {
    xs[i].x = points[i].x;
    xs[i].y = points[i].y;
  }

  if (mask) {
    if (img->img.id2 == None)
      _xitk_x11_image_add_mask (img);
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc2, color);
    XFillPolygon (d->display, img->img.id2, d->gc2, xs, num_points, Convex, CoordModeOrigin);
  } else {
    _xitk_x11_display_gc (d, img);
    XSetForeground (d->display, d->gc1, color);
    XFillPolygon (d->display, img->img.id1, d->gc1, xs, num_points, Convex, CoordModeOrigin);
  }
}
  
static int xitk_x11_image_set_props (xitk_be_image_t *_image, const xitk_tagitem_t *taglist) {
  (void)_image;
  (void)taglist;
  return 0;
}

static int xitk_x11_image_get_props (xitk_be_image_t *_image, xitk_tagitem_t *taglist) {
  xitk_x11_image_t *img;

  xitk_container (img, _image, img);
  if (!img)
    return 0;
  return xitk_tags_get (img->props, taglist);
}

static void xitk_x11_image_delete (xitk_be_image_t **_image) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;

  if (!_image)
    return;
  xitk_container (img, *_image, img);
  if (!img)
    return;
  *_image = NULL;
  d = img->d;
  if ((img->img.id1 != None) || (img->img.id2 != None)) {
    d->d.lock (&d->d);
    if (img->img.id2 != None) {
      XFreePixmap (d->display, img->img.id2);
      img->img.id2 = None;
    }
    if (img->img.id1 != None) {
      XFreePixmap (d->display, img->img.id1);
      img->img.id1 = None;
    }
    d->d.unlock (&d->d);
  }
  pthread_mutex_lock (&d->mutex);
  xitk_dnode_remove (&img->img.node);
  if (_xitk_x11_display_unref (d))
    pthread_mutex_unlock (&d->mutex);
  free (img);
}

static void xitk_x11_image_copy_rect (xitk_be_image_t *_image, xitk_be_image_t *_from,
  int x1, int y1, int w, int h, int x2, int y2) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img, *from;

  xitk_container (img, _image, img);
  xitk_container (from, _from, img);
  if (!img || !from)
    return;
  d = img->d;

  {
    int v;

    v = img->props[XITK_X11_IT_W].value;
    if (x2 < 0) {
      w += x2;
      x2 = 0;
    }
    if (x2 >= v)
      return;
    v = img->props[XITK_X11_IT_H].value;
    if (y2 < 0) {
      h += y2;
      y2 = 0;
    }
    if (y2 >= v)
      return;
    v = from->props[XITK_X11_IT_W].value;
    if (x1 < 0) {
      w += x1;
      x1 = 0;
    }
    if (x1 >= v)
      return;
    v = from->props[XITK_X11_IT_H].value;
    if (y1 < 0) {
      h += y1;
      y1 = 0;
    }
    if (y1 >= v)
      return;
    if ((w <= 0) || (h <= 0))
      return;
  }

  if ((img->img.id2 == None) && (from->img.id2 != None)) {
    if ((w != (int)img->props[XITK_X11_IT_W].value) || (h != (int)img->props[XITK_X11_IT_H].value)) {
      _xitk_x11_image_add_mask (img);
      img->shared_mask.id = None;
      img->shared_mask.x = 0;
      img->shared_mask.y = 0;
    } else {
      img->props[XITK_X11_IT_MASK].value = XITK_BE_MASK_SHARED;
      img->shared_mask.id = from->img.id2;
      img->shared_mask.x = x1;
      img->shared_mask.y = y1;
    }
  }

  _xitk_x11_display_gc (d, img);
  if ((img->img.id2 != None) && (from->img.id2 != None))
    XCopyArea (d->display, from->img.id2, img->img.id2, d->gc2, x1, y1, w, h, x2, y2);
  XCopyArea (d->display, from->img.id1, img->img.id1, d->gc1, x1, y1, w, h, x2, y2);
}

static int xitk_x11_pixel_is_visible (xitk_be_image_t *_img, int x, int y) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;
  uint32_t pixel;

  xitk_container (img, _img, img);
  if (!img)
    return 0;
  d = img->d;
  if ((x < 0) || (x >= (int)img->props[XITK_X11_IT_W].value)
    || (y < 0) || (y >= (int)img->props[XITK_X11_IT_H].value))
    return 0;
  if (img->img.id2 == None)
    return 1;

  d->d.lock (&d->d);
  if (d->testpix)
    XGetSubImage (d->display, img->img.id2, x, y, 1, 1, AllPlanes, ZPixmap, d->testpix, 0, 0);
  else
    d->testpix = XGetImage (d->display, img->img.id2, x, y, 1, 1, AllPlanes, ZPixmap);
  d->d.unlock (&d->d);
  pixel = XGetPixel (d->testpix, 0, 0);

  return pixel;
}

static xitk_be_image_t *xitk_x11_image_new (xitk_be_display_t *_d, const xitk_tagitem_t *taglist) {
  xitk_x11_display_t *d;
  xitk_x11_image_t *img;

  xitk_container (d, _d, d);
  img = calloc (1, sizeof (*img));
  if (!img)
    return NULL;

  memcpy (img->props, _xitk_x11_image_defaults, sizeof (img->props));
  xitk_tags_get (taglist, img->props);

  do {
    ImlibData *ild = d->be->be.xitk->imlibdata;

    if (!ild)
      break;

    xitk_dnode_init (&img->img.node);
    img->img.magic      = _XITK_X11_IMAGE_MAGIC;
    img->img.type       = XITK_BE_TYPE_X11;
    img->img.id1        = None;
    img->img.id2        = None;
    img->img.display    = &d->d;
    img->img.data       = NULL;
    img->img._delete    = xitk_x11_image_delete;
    img->img.get_props  = xitk_x11_image_get_props;
    img->img.pixel_is_visible = xitk_x11_pixel_is_visible;
    img->img.set_props  = xitk_x11_image_set_props;
    img->img.draw_lines = xitk_x11_image_draw_lines;
    img->img.fill_rects = xitk_x11_image_fill_rects;
    img->img.draw_arc   = xitk_x11_image_draw_arc;
    img->img.fill_arc   = xitk_x11_image_fill_arc;
    img->img.fill_polygon = xitk_x11_image_fill_polygon;
#if 0
    img->img.draw_text  = xitk_x11_image_draw_text;
#endif
    img->img.copy_rect  = xitk_x11_image_copy_rect;
    img->d              = d;
    img->shared_mask.id = None;
    img->shared_mask.x  = 0;
    img->shared_mask.y  = 0;

    if (img->props[XITK_X11_IT_FILENAME].value ||
       (img->props[XITK_X11_IT_FILEBUF].value && img->props[XITK_X11_IT_FILESIZE].value)) {
      ImlibImage *ili;
      int w, h, mx, my;

      if (img->props[XITK_X11_IT_FILENAME].value) {
        d->d.lock (&d->d);
        ili = Imlib_load_image (ild, (const char *)img->props[XITK_X11_IT_FILENAME].value);
        d->d.unlock (&d->d);
        if (!ili) {
          XITK_WARNING ("%s: couldn't load image %s\n", __FUNCTION__, (const char *)img->props[XITK_X11_IT_FILENAME].value);
          break;
        }
      } else {
        d->d.lock (&d->d);
        ili = Imlib_decode_image (ild, (const char *)img->props[XITK_X11_IT_FILEBUF].value, img->props[XITK_X11_IT_FILESIZE].value);
        d->d.unlock (&d->d);
        if (!ili) {
          XITK_WARNING ("%s: couldn't decode image\n", __FUNCTION__);
          break;
        }
      }
      w = ili->rgb_width;
      h = ili->rgb_height;
      if ((w <= 0) || (h <= 0))
        break;
      mx = img->props[XITK_X11_IT_W].value > 0 ? img->props[XITK_X11_IT_W].value * 0x10000 / w : 0x10000;
      my = img->props[XITK_X11_IT_H].value > 0 ? img->props[XITK_X11_IT_H].value * 0x10000 / h : 0x10000;
      if (my < mx)
        mx = my;
      w = (w * mx) >> 16;
      h = (h * mx) >> 16;
      if ((w <= 0) || (h <= 0))
        break;
      d->d.lock (&d->d);
      if (!Imlib_render (ild, ili, w, h)) {
        Imlib_destroy_image (ild, ili);
        d->d.unlock (&d->d);
        XITK_WARNING ("%s: couldn't render image %s\n", __FUNCTION__, (const char *)img->props[XITK_X11_IT_FILENAME].value);
        break;
      }
      img->img.id1 = Imlib_copy_image (ild, ili);
      img->img.id2 = ili->shape_mask ? Imlib_copy_mask (ild, ili) : None;
      Imlib_destroy_image (ild, ili);
      d->d.unlock (&d->d);
      img->props[XITK_X11_IT_W].value = w;
      img->props[XITK_X11_IT_H].value = h;
      img->props[XITK_X11_IT_MASK].value = (img->img.id2 != None) ? XITK_BE_MASK_YES : XITK_BE_MASK_NO;

    } else if ((img->props[XITK_X11_IT_W].value > 0) && (img->props[XITK_X11_IT_H].value > 0)) {

      if (img->props[XITK_X11_IT_RAW].value) {
        d->d.lock (&d->d);
        img->img.id1 = XCreateBitmapFromData (d->display, ild->x.base_window,
          (const char *)img->props[XITK_X11_IT_RAW].value,
          img->props[XITK_X11_IT_W].value, img->props[XITK_X11_IT_H].value);
        d->d.unlock (&d->d);
        img->img.id2 = None;
        img->props[XITK_X11_IT_MASK].value = XITK_BE_MASK_NO;
      } else {
        d->d.lock (&d->d);
        if (img->props[XITK_X11_IT_MASK].value == XITK_BE_MASK_YES)
          _xitk_x11_image_add_mask (img);
        else
          img->img.id2 = None;
        img->img.id1 = XCreatePixmap (d->display, ild->x.base_window, img->props[XITK_X11_IT_W].value,
          img->props[XITK_X11_IT_H].value, ild->x.depth);
        d->d.unlock (&d->d);
      }

    } else {
      break;
    }

    img->props[XITK_X11_IT_RAW].value = (uintptr_t)NULL;
    img->props[XITK_X11_IT_FILENAME].value = (uintptr_t)NULL;
    pthread_mutex_lock (&d->mutex);
    d->refs += 1;
    xitk_dlist_add_tail (&d->d.images, &img->img.node);
    pthread_mutex_unlock (&d->mutex);
    return &img->img;
  } while (0);

  free (img);
  return NULL;
}

static void xitk_x11_display_lock (xitk_be_display_t *_d) {
  xitk_x11_display_t *d;

  xitk_container (d, _d, d);
  XLockDisplay (d->display);
}

static void xitk_x11_display_unlock (xitk_be_display_t *_d) {
  xitk_x11_display_t *d;

  xitk_container (d, _d, d);
  XUnlockDisplay (d->display);
}

static void xitk_x11_display_nolock (xitk_be_display_t *_d) {
  (void)_d;
}

static void _xitk_x11_clipboard_unregister_window (xitk_x11_display_t *d, Window win) {
  if (d->clipboard.window_out == win)
    d->clipboard.window_out = None;
  if (d->clipboard.window_in == win)
    d->clipboard.window_in = None;
}

static void _xitk_x11_clipboard_init (xitk_x11_display_t *d) {
  static const char * const atom_names[XITK_A_clip_end] = {
    [XITK_A_null]        = "NULL",
    [XITK_A_atom]        = "ATOM",
    [XITK_A_timestamp]   = "TIMESTAMP",
    [XITK_A_integer]     = "INTEGER",
    [XITK_A_c_string]    = "C_STRING",
    [XITK_A_string]      = "STRING",
    [XITK_A_utf8_string] = "UTF8_STRING",
    [XITK_A_text]        = "TEXT",
    [XITK_A_filename]    = "FILENAME",
    [XITK_A_net_address] = "NET_ADDRESS",
    [XITK_A_window]      = "WINDOW",
    [XITK_A_clipboard]   = "CLIPBOARD",
    [XITK_A_length]      = "LENGTH",
    [XITK_A_targets]     = "TARGETS",
    [XITK_A_multiple]    = "MULTIPLE",
    [XITK_A__NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK"
  };

  d->clipboard.text = NULL;
  d->clipboard.text_len = 0;
  d->clipboard.window_in = None;
  d->clipboard.window_out = None;
  d->clipboard.own_time = CurrentTime;

  d->d.lock (&d->d);
  XInternAtoms (d->display, (char **)atom_names, XITK_A_clip_end, True, d->clipboard.atoms);
  d->clipboard.dummy = XInternAtom (d->display, "_XITK_CLIP", False);
  d->d.unlock (&d->d);
  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.clipboard: "
      "null=%d atom=%d timestamp=%d integer=%d c_string=%d string=%d utf8_string=%d text=%d "
      "filename=%d net_address=%d window=%d clipboard=%d length=%d targets=%d multiple=%d dummy=%d.\n",
      (int)d->clipboard.atoms[XITK_A_null],
      (int)d->clipboard.atoms[XITK_A_atom],
      (int)d->clipboard.atoms[XITK_A_timestamp],
      (int)d->clipboard.atoms[XITK_A_integer],
      (int)d->clipboard.atoms[XITK_A_c_string],
      (int)d->clipboard.atoms[XITK_A_string],
      (int)d->clipboard.atoms[XITK_A_utf8_string],
      (int)d->clipboard.atoms[XITK_A_text],
      (int)d->clipboard.atoms[XITK_A_filename],
      (int)d->clipboard.atoms[XITK_A_net_address],
      (int)d->clipboard.atoms[XITK_A_window],
      (int)d->clipboard.atoms[XITK_A_clipboard],
      (int)d->clipboard.atoms[XITK_A_length],
      (int)d->clipboard.atoms[XITK_A_targets],
      (int)d->clipboard.atoms[XITK_A_multiple],
      (int)d->clipboard.dummy);
}

static int _xitk_x11_clipboard_event (xitk_x11_display_t *d, XEvent *event) {
  if (event->xproperty.window == None)
    return 0;
  if (event->type == PropertyNotify) {
    if (event->xproperty.atom == d->clipboard.dummy) {
      if (event->xproperty.window == d->clipboard.window_out) {
        /* set #2. */
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.clipboard: set #2: own clipboard @ time=%ld.\n", (long int)event->xproperty.time);
        d->clipboard.own_time = event->xproperty.time;
        d->d.lock (&d->d);
        XSetSelectionOwner (d->display, d->clipboard.atoms[XITK_A_clipboard],
          d->clipboard.window_out, d->clipboard.own_time);
        d->d.unlock (&d->d);
      } else if (event->xproperty.window == d->clipboard.window_in) {
        /* get #2. */
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.clipboard: get #2 time=%ld.\n", (long int)event->xproperty.time);
        if (event->xproperty.state == PropertyNewValue) {
          do {
            Atom actual_type;
            int actual_format = 0;
            unsigned long nitems, bytes_after;
            unsigned char *prop;

            d->d.lock (&d->d);
            XGetWindowProperty (d->display, d->clipboard.window_in,
              d->clipboard.dummy, 0, 0, False, d->clipboard.atoms[XITK_A_utf8_string],
              &actual_type, &actual_format, &nitems, &bytes_after, &prop);
            XFree (prop);
            if ((actual_type != d->clipboard.atoms[XITK_A_utf8_string]) || (actual_format != 8))
              break;
            d->clipboard.text = malloc ((bytes_after + 1 + 3) & ~3);
            if (!d->clipboard.text)
              break;
            d->clipboard.text_len = bytes_after;
            XGetWindowProperty (d->display, d->clipboard.window_in,
              d->clipboard.dummy, 0, (d->clipboard.text_len + 3) >> 2,
              True, d->clipboard.atoms[XITK_A_utf8_string],
              &actual_type, &actual_format, &nitems, &bytes_after, &prop);
            if (!prop)
              break;
            memcpy (d->clipboard.text, prop, d->clipboard.text_len);
            XFree (prop);
            d->clipboard.text[d->clipboard.text_len] = 0;
          } while (0);
          d->d.unlock (&d->d);
          if (d->clipboard.text_len > 0) {
            return 2;
          }
        }
        d->clipboard.window_in = None;
      }
      return 1;
    }
  } else if (event->type == SelectionClear) {
    if ((event->xselectionclear.window == d->clipboard.window_out)
     && (event->xselectionclear.selection == d->clipboard.atoms[XITK_A_clipboard])) {
      if (d->be->be.verbosity >= 2)
        printf ("xitk.x11.clipboard: lost.\n");
      d->clipboard.window_out = None;
    }
    return 1;
  } else if (event->type == SelectionNotify) {
  } else if (event->type == SelectionRequest) {
    if ((event->xselectionrequest.owner == d->clipboard.window_out)
     && (event->xselectionrequest.selection == d->clipboard.atoms[XITK_A_clipboard])) {
      if (d->be->be.verbosity >= 2) {
        char *tname, *pname;
        d->d.lock (&d->d);
        tname = XGetAtomName (d->display, event->xselectionrequest.target);
        pname = XGetAtomName (d->display, event->xselectionrequest.property);
        d->d.unlock (&d->d);
        printf ("xitk.x11.clipboard: serve #1 requestor=%d target=%d (%s) property=%d (%s).\n",
          (int)event->xselectionrequest.requestor,
          (int)event->xselectionrequest.target, tname,
          (int)event->xselectionrequest.property, pname);
        XFree (pname);
        XFree (tname);
      }
      d->clipboard.req    = event->xselectionrequest.requestor;
      d->clipboard.target = event->xselectionrequest.target;
      d->clipboard.prop   = event->xselectionrequest.property;
      if (d->clipboard.target == d->clipboard.atoms[XITK_A_targets]) {
        int r;
        Atom atoms[] = {
          d->clipboard.atoms[XITK_A_string], d->clipboard.atoms[XITK_A_utf8_string],
          d->clipboard.atoms[XITK_A_length], d->clipboard.atoms[XITK_A_timestamp]
        };
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.clipboard: serve #2: reporting %d (STRING) %d (UTF8_STRING) %d (LENGTH) %d (TIMESTAMP).\n",
            (int)d->clipboard.atoms[XITK_A_string], (int)d->clipboard.atoms[XITK_A_utf8_string],
            (int)d->clipboard.atoms[XITK_A_length], (int)d->clipboard.atoms[XITK_A_timestamp]);
        d->d.lock (&d->d);
        r = XChangeProperty (d->display, d->clipboard.req, d->clipboard.prop,
          d->clipboard.atoms[XITK_A_atom], 32, PropModeReplace,
          (const unsigned char *)atoms, sizeof (atoms) / sizeof (atoms[0]));
        d->d.unlock (&d->d);
        if (r) {
          if (d->be->be.verbosity >= 2)
            printf ("xitk.x11.clipboard: serve #2: reporting OK.\n");
        }
      } else if ((d->clipboard.target == d->clipboard.atoms[XITK_A_string])
        || (d->clipboard.target == d->clipboard.atoms[XITK_A_utf8_string])) {
        int r;
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.clipboard: serve #3: sending %d bytes.\n", d->clipboard.text_len + 1);
        d->d.lock (&d->d);
        r = XChangeProperty (d->display, d->clipboard.req, d->clipboard.prop,
          d->clipboard.atoms[XITK_A_utf8_string], 8, PropModeReplace,
          (const unsigned char *)d->clipboard.text, d->clipboard.text_len + 1);
        d->d.unlock (&d->d);
        if (r) {
          if (d->be->be.verbosity >= 2)
            printf ("xitk.x11.clipboard: serve #3 len=%d OK.\n", d->clipboard.text_len);
        }
      }
      {
        XEvent event2;
        event2.xany.type = SelectionNotify;
        event2.xselection.requestor = event->xselectionrequest.requestor;
        event2.xselection.target = event->xselectionrequest.target;
        event2.xselection.property = event->xselectionrequest.property;
        event2.xselection.time = event->xselectionrequest.time;
        d->d.lock (&d->d);
        XSendEvent (d->display, event->xselectionrequest.requestor, False, 0, &event2);
        d->d.unlock (&d->d);
      }
      return 1;
    }
  }
  return 0;
}

static int xitk_x11_window_set_utf8 (xitk_be_window_t *_win, const char *text, int text_len) {
  xitk_x11_window_t *win;
  xitk_x11_display_t *d;
  Window xw;

  xitk_container (win, _win, w);
  if (!win)
    return 0;
  d = win->d;
  xw = win->w.id;

  free (d->clipboard.text);
  d->clipboard.text = NULL;
  d->clipboard.text_len = 0;
  if (!text || (text_len <= 0))
    return 0;
  d->clipboard.text = malloc (text_len + 1);
  if (!d->clipboard.text)
    return 0;
  memcpy (d->clipboard.text, text, text_len);
  d->clipboard.text[text_len] = 0;
  d->clipboard.text_len = text_len;

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.clipboard: set #1.\n");
  d->clipboard.window_out = xw;
  d->d.lock (&d->d);
  /* set #1: HACK: get current server time. */
  XChangeProperty (d->display, xw,
    d->clipboard.dummy, d->clipboard.atoms[XITK_A_utf8_string], 8, PropModeAppend, NULL, 0);
  d->d.unlock (&d->d);

  return text_len;
}

static int xitk_x11_window_get_utf8 (xitk_be_window_t *_win, char **text, int max_len) {
  xitk_x11_window_t *win;
  xitk_x11_display_t *d;
  Window xw;
  int l;

  xitk_container (win, _win, w);
  if (!win)
    return 0;
  d = win->d;
  xw = win->w.id;

  if (d->clipboard.window_in != xw) {

    if (d->clipboard.window_out == None) {
      free (d->clipboard.text);
      d->clipboard.text = NULL;
      d->clipboard.text_len = 0;
      d->clipboard.window_in = xw;
      /* get #1. */
      if (d->be->be.verbosity >= 2)
        printf ("xitk.x11.clipboard: get #1.\n");
      d->d.lock (&d->d);
      XConvertSelection (d->display, d->clipboard.atoms[XITK_A_clipboard],
        d->clipboard.atoms[XITK_A_utf8_string], d->clipboard.dummy, win->w.id, CurrentTime);
      d->d.unlock (&d->d);
      return -1;
    }
    if (d->be->be.verbosity >= 2)
      printf ("xitk.x11.clipboard: get #1 from self: %d bytes.\n", d->clipboard.text_len);
  } else {
    d->clipboard.window_in = None;
    if (d->be->be.verbosity >= 2)
      printf ("xitk.x11.clipboard: get #3: %d bytes.\n", d->clipboard.text_len);
  }

  if (!d->clipboard.text || (d->clipboard.text_len <= 0))
    return 0;
  l = d->clipboard.text_len;
  if (l > max_len)
    l = max_len;
  if (l <= 0)
    return 0;
  if (!text)
    return l;
  if (*text)
    memcpy (*text, d->clipboard.text, l);
  else
    *text = d->clipboard.text;
  return l;
}

static const int _xitk_x11_event_type[XITK_EV_LAST] = {
  [XITK_EV_NEW_WIN] = CreateNotify,
  [XITK_EV_DEL_WIN] = DestroyNotify,
  [XITK_EV_SHOW] = MapNotify,
  [XITK_EV_HIDE] = UnmapNotify,
  [XITK_EV_REPARENT] = ReparentNotify,
  [XITK_EV_FOCUS] = FocusIn,
  [XITK_EV_UNFOCUS] = FocusOut,
  [XITK_EV_ENTER] = EnterNotify,
  [XITK_EV_LEAVE] = LeaveNotify,
  [XITK_EV_EXPOSE] = Expose,
  [XITK_EV_POS_SIZE] = ConfigureNotify,
  [XITK_EV_MOVE] = MotionNotify,
  [XITK_EV_BUTTON_DOWN] = ButtonPress,
  [XITK_EV_BUTTON_UP] = ButtonRelease,
  [XITK_EV_KEY_DOWN] = KeyPress,
  [XITK_EV_KEY_UP] = KeyRelease,
  [XITK_EV_CLIP_READY] = 0
};

static uint32_t _xitk_x11_get_modifier (uint32_t state) {
  static const uint32_t tab[] = {
    ShiftMask,    MODIFIER_SHIFT,
    LockMask,     MODIFIER_LOCK,
    ControlMask,  MODIFIER_CTRL,
    Mod1Mask,     MODIFIER_META,
    Mod2Mask,     MODIFIER_NUML,
    Mod3Mask,     MODIFIER_MOD3,
    Mod4Mask,     MODIFIER_MOD4,
    Mod5Mask,     MODIFIER_MOD5,
    Button1Mask,  MODIFIER_BUTTON1,
    Button2Mask,  MODIFIER_BUTTON2,
    Button3Mask,  MODIFIER_BUTTON3,
    Button4Mask,  MODIFIER_BUTTON4,
    Button5Mask,  MODIFIER_BUTTON5
  };
  uint32_t res = MODIFIER_NOMOD, i;

  if (state & XK_Multi_key)
    state = (state | XK_Multi_key) & 0xFF;
  for (i = 0; i < sizeof (tab) / sizeof (tab[0]); i += 2)
    if (state & tab[i])
      res |= tab[i + 1];
  return res;
}

static int _xitk_x11_ctrl_keysyms_cmp (void *a, void *b) {
  const unsigned int d = (const unsigned int)(uintptr_t)a;
  const unsigned int e = (const unsigned int)(uintptr_t)b;
  return d < e ? -1 : d > e ? 1 : 0;
}

static int _xitk_x11_keyevent_2_string (xitk_x11_display_t *d, XEvent *event, KeySym *ksym, char *buf, int bsize) {
  int i, len;

  *ksym = XK_VoidSymbol;
  d->d.lock (&d->d);
  /* ksym = XLookupKeysym (&event->xkey, 0); */
  len = XLookupString (&event->xkey, buf, bsize - 1, ksym, NULL);
  d->d.unlock (&d->d);

  i = xine_sarray_binary_search (d->ctrl_keysyms1, (void *)(uintptr_t)(*ksym));
  if (i >= 0) {
    buf[0] = XITK_CTRL_KEY_PREFIX;
    buf[1] = d->ctrl_keysyms2[i];
    len = 2;
  }

  if (len < 0)
    len = 0;
  buf[len] = 0;

  return len;
}

static void _xitk_x11_window_debug_flags (const char *s1, const char *s2, uint32_t flags) {
  char buf[2000], *b = buf, *e = b + sizeof (buf);

  b += strlcpy (b, "xitk.x11.window.flags.", e - b);
  b += strlcpy (b, s1, e - b);
  b += strlcpy (b, " (", e - b);
  b += strlcpy (b, s2, e - b);
  b += strlcpy (b, "): ", e - b);
  if (flags & XITK_WINF_VISIBLE)
    b += strlcpy (b, "visible ", e - b);
  if (flags & XITK_WINF_ICONIFIED)
    b += strlcpy (b, "iconified ", e - b);
  if (flags & XITK_WINF_DECORATED)
    b += strlcpy (b, "decorated ", e - b);
  if (flags & XITK_WINF_TASKBAR)
    b += strlcpy (b, "taskbar ", e - b);
  if (flags & XITK_WINF_PAGER)
    b += strlcpy (b, "pager ", e - b);
  if (flags & XITK_WINF_MAX_X)
    b += strlcpy (b, "max_x ", e - b);
  if (flags & XITK_WINF_MAX_Y)
    b += strlcpy (b, "max_y ", e - b);
  if (flags & XITK_WINF_FULLSCREEN)
    b += strlcpy (b, "fullscreen ", e - b);
  if (flags & XITK_WINF_FOCUS)
    b += strlcpy (b, "focus ", e - b);
  if (flags & XITK_WINF_OVERRIDE_REDIRECT)
    b += strlcpy (b, "override_redirect ", e - b);
  if (flags & XITK_WINF_FIXED_POS)
    b += strlcpy (b, "fixed_pos ", e - b);
  if (flags & XITK_WINF_FENCED_IN)
    b += strlcpy (b, "fenced_in ", e - b);
  b += strlcpy (b, ".\n", e - b);
  printf ("%s", buf);
}

static uint32_t _xitk_x11_window_merge_flags (uint32_t f1, uint32_t f2) {
  uint32_t mask, res;

  mask = f2 & 0xffff0000;
  res = f1 | mask;
  mask >>= 16;
  res = (f1 & ~mask) | (f2 & mask);
  return res;
}

static void _xitk_x11_window_flags (xitk_x11_window_t *win, uint32_t mask_and_value) {
  xitk_x11_display_t *d = win->d;
  XWindowAttributes attr;
  Atom type, buf[32];
  int fmt;
  unsigned long nitem, rest;
  uint32_t oldflags, newflags, diff, mask, have;

  mask = mask_and_value >> 16;
  mask_and_value &= 0xffff;
  have = 0;
  oldflags = win->props[XITK_X11_WT_WIN_FLAGS].value;

  attr.root = None;
  if (XGetWindowAttributes (d->display, win->w.id, &attr)) {
    oldflags &= ~(XITK_WINF_VISIBLE | XITK_WINF_OVERRIDE_REDIRECT);
    have |= XITK_WINF_VISIBLE | XITK_WINF_OVERRIDE_REDIRECT;
/*  win->props[XITK_X11_WT_X].value = attr.x;
    win->props[XITK_X11_WT_Y].value = attr.y; */
    win->props[XITK_X11_WT_W].value = attr.width;
    win->props[XITK_X11_WT_H].value = attr.height;
    oldflags |= attr.map_state == IsUnmapped ? 0 : XITK_WINF_VISIBLE;
    oldflags |= attr.override_redirect == False ? 0 : XITK_WINF_OVERRIDE_REDIRECT;
  }

  if (XGetWindowProperty (d->display, win->w.id, d->atoms[XITK_A__NET_WM_STATE], 0, sizeof (buf),
    False, d->atoms[XITK_A_atom], &type, &fmt, &nitem, &rest, (unsigned char **)buf)) {
    uint32_t u;

    oldflags &= ~(XITK_WINF_FULLSCREEN | XITK_WINF_MAX_X | XITK_WINF_MAX_Y);
    oldflags |= XITK_WINF_TASKBAR | XITK_WINF_PAGER;
    for (u = 0; u < nitem; u++) {
      if (buf[u] == d->atoms[XITK_A__NET_WM_STATE_FULLSCREEN]) {
        oldflags |= XITK_WINF_FULLSCREEN;
      } else if (buf[u] == d->atoms[XITK_A__NET_WM_STATE_SKIP_TASKBAR]) {
        oldflags &= ~XITK_WINF_TASKBAR;
      } else if (buf[u] == d->atoms[XITK_A__NET_WM_STATE_SKIP_PAGER]) {
        oldflags &= ~XITK_WINF_PAGER;
      } else if (buf[u] == d->atoms[XITK_A__NET_WM_STATE_MAXIMIZED_HORZ]) {
        oldflags |= XITK_WINF_MAX_X;
      } else if (buf[u] == d->atoms[XITK_A__NET_WM_STATE_MAXIMIZED_VERT]) {
        oldflags |= XITK_WINF_MAX_Y;
      }
    }
  } else {
    oldflags |= XITK_WINF_TASKBAR | XITK_WINF_PAGER;
  }
  have |= XITK_WINF_FULLSCREEN | XITK_WINF_TASKBAR | XITK_WINF_PAGER | XITK_WINF_MAX_X | XITK_WINF_MAX_Y;

  have |= XITK_WINF_ICONIFIED | XITK_WINF_DECORATED | XITK_WINF_FIXED_POS | XITK_WINF_FENCED_IN;
  if (d->be->be.verbosity >= 2)
    _xitk_x11_window_debug_flags ("before", win->title, oldflags);

  newflags = (oldflags & ~mask) | (mask_and_value & mask);
  diff = newflags ^ oldflags;
  if (diff) {
    d->d.lock (&d->d);
    if (diff & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) {
      if (diff & newflags & XITK_WINF_VISIBLE)
        XMapWindow (d->display, win->w.id);
      else if ((newflags & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) == XITK_WINF_ICONIFIED)
        XIconifyWindow (d->display, win->w.id, d->be->be.xitk->imlibdata->x.screen);
      else
        XUnmapWindow (d->display, win->w.id);
      XSync (d->display, False);
    }
    if (diff & XITK_WINF_DECORATED) {
        MWMHints mwmhints;

      memset (&mwmhints, 0, sizeof (mwmhints));
      mwmhints.flags = MWM_HINTS_DECORATIONS;
      mwmhints.decorations = (newflags & XITK_WINF_DECORATED) ? 1 : 0;
      XChangeProperty (d->display, win->w.id, d->atoms[XITK_A__MOTIF_WM_HINTS], d->atoms[XITK_A__MOTIF_WM_HINTS], 32,
        PropModeReplace, (unsigned char *) &mwmhints, PROP_MWM_HINTS_ELEMENTS);
    }
    if (diff & XITK_WINF_FULLSCREEN) {
        XEvent msg;

        msg.xclient.type = ClientMessage;
        msg.xclient.serial = 0;
        msg.xclient.send_event = 1;
        msg.xclient.window = win->w.id;
        msg.xclient.message_type = d->atoms[XITK_A__NET_WM_STATE];
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (newflags & XITK_WINF_FULLSCREEN) ? 1 : 0;
        msg.xclient.data.l[1] = d->atoms[XITK_A__NET_WM_STATE_FULLSCREEN];
        msg.xclient.data.l[2] = None;
        msg.xclient.data.l[3] = 1; /* from plain spplication */
        msg.xclient.data.l[4] = 0;
        XSendEvent (d->display, attr.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
    }
    if (diff & XITK_WINF_TASKBAR) {
        XEvent msg;

        msg.xclient.type = ClientMessage;
        msg.xclient.serial = 0;
        msg.xclient.send_event = 1;
        msg.xclient.window = win->w.id;
        msg.xclient.message_type = d->atoms[XITK_A__NET_WM_STATE];
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (newflags & XITK_WINF_TASKBAR) ? 0 : 1;
        msg.xclient.data.l[1] = d->atoms[XITK_A__NET_WM_STATE_SKIP_TASKBAR];
        msg.xclient.data.l[2] = None;
        msg.xclient.data.l[3] = 1; /* from plain spplication */
        msg.xclient.data.l[4] = 0;
        XSendEvent (d->display, attr.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
    }
    if (diff & XITK_WINF_PAGER) {
        XEvent msg;

        msg.xclient.type = ClientMessage;
        msg.xclient.serial = 0;
        msg.xclient.send_event = 1;
        msg.xclient.window = win->w.id;
        msg.xclient.message_type = d->atoms[XITK_A__NET_WM_STATE];
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (newflags & XITK_WINF_PAGER) ? 0 : 1;
        msg.xclient.data.l[1] = d->atoms[XITK_A__NET_WM_STATE_SKIP_PAGER];
        msg.xclient.data.l[2] = None;
        msg.xclient.data.l[3] = 1; /* from plain spplication */
        msg.xclient.data.l[4] = 0;
        XSendEvent (d->display, attr.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
    }
    if (diff & XITK_WINF_MAX_X) {
        XEvent msg;

        msg.xclient.type = ClientMessage;
        msg.xclient.serial = 0;
        msg.xclient.send_event = 1;
        msg.xclient.window = win->w.id;
        msg.xclient.message_type = d->atoms[XITK_A__NET_WM_STATE];
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (newflags & XITK_WINF_MAX_X) ? 1 : 0;
        msg.xclient.data.l[1] = d->atoms[XITK_A__NET_WM_STATE_MAXIMIZED_HORZ];
        msg.xclient.data.l[2] = None;
        if ((diff & XITK_WINF_MAX_Y) && !(((newflags << 1) ^ newflags) & XITK_WINF_MAX_Y)) {
          diff &= ~XITK_WINF_MAX_Y;
          msg.xclient.data.l[2] = d->atoms[XITK_A__NET_WM_STATE_MAXIMIZED_VERT];
        }
        msg.xclient.data.l[3] = 1; /* from plain spplication */
        msg.xclient.data.l[4] = 0;
        XSendEvent (d->display, attr.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
    }
    if (diff & XITK_WINF_MAX_Y) {
        XEvent msg;

        msg.xclient.type = ClientMessage;
        msg.xclient.serial = 0;
        msg.xclient.send_event = 1;
        msg.xclient.window = win->w.id;
        msg.xclient.message_type = d->atoms[XITK_A__NET_WM_STATE];
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (newflags & XITK_WINF_MAX_Y) ? 1 : 0;
        msg.xclient.data.l[1] = d->atoms[XITK_A__NET_WM_STATE_MAXIMIZED_VERT];
        msg.xclient.data.l[2] = None;
        msg.xclient.data.l[3] = 1; /* from plain spplication */
        msg.xclient.data.l[4] = 0;
        XSendEvent (d->display, attr.root, False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
    }
    if (diff & XITK_WINF_FOCUS) {
      if (newflags & XITK_WINF_FOCUS)
        XSetInputFocus (d->display, win->w.id, RevertToParent, CurrentTime);
    }
    XSync (d->display, False);
    d->d.unlock (&d->d);
    if (diff & XITK_WINF_DND) {
      if (newflags & XITK_WINF_DND) {
        win->dnd = xitk_dnd_new (d->display, d->d.lock == xitk_x11_display_lock, d->be->be.verbosity);
        xitk_dnd_make_window_aware (win->dnd, win->w.id);
      } else {
        pthread_mutex_lock (&d->mutex);
        if (d->dnd_win == win)
          d->dnd_win = NULL;
        pthread_mutex_unlock (&d->mutex);
        xitk_dnd_delete (win->dnd);
        win->dnd = NULL;
      }
    }
  }

  if (d->be->be.verbosity >= 2)
    _xitk_x11_window_debug_flags ("after", win->title, newflags);
  win->props[XITK_X11_WT_WIN_FLAGS].value = newflags | (have << 16);
}

static int xitk_x11_window_get_props (xitk_be_window_t *_win, xitk_tagitem_t *taglist) {
  xitk_x11_window_t *win;

  xitk_container (win, _win, w);
  if (!win)
    return 0;
  return xitk_tags_get (win->props, taglist);
}

static int xitk_x11_window_set_props (xitk_be_window_t *_win, const xitk_tagitem_t *taglist) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;
  xitk_x11_image_t *img;
  xitk_tagitem_t props[XITK_X11_WT_LAST];
  XSizeHints hint;
  int res;

  xitk_container (win, _win, w);
  if (!win)
    return 0;
  d = win->d;
  if (win->w.id == None) {
    if (d->be->be.verbosity >= 1)
      printf ("xitk.x11.window.set_props (%s): ERROR: window closed already.\n",
        (const char *)win->props[XITK_X11_WT_TITLE].value);
    return 0;
  }

  if (win->props[XITK_X11_WT_WRAP].value != None) {
    XWindowAttributes attr;
    attr.root = None;
    if (XGetWindowAttributes (d->display, win->w.id, &attr)) {
/*    win->props[XITK_X11_WT_X].value = attr.x;
      win->props[XITK_X11_WT_Y].value = attr.y; */
      win->props[XITK_X11_WT_W].value = attr.width;
      win->props[XITK_X11_WT_H].value = attr.height;
    }
  }
  memcpy (props, win->props, sizeof (props));
  props[XITK_X11_WT_IMAGE].value = (uintptr_t)NULL;
  props[XITK_X11_WT_PARENT].value = ~(uintptr_t)NULL;
  res = xitk_tags_get (taglist, props);

  /* always do this on request. */
  if (props[XITK_X11_WT_PARENT].value != ~(uintptr_t)NULL) {
    xitk_x11_window_t *pw = (xitk_x11_window_t *)props[XITK_X11_WT_PARENT].value;

    win->props[XITK_X11_WT_X].value = props[XITK_X11_WT_X].value;
    win->props[XITK_X11_WT_Y].value = props[XITK_X11_WT_Y].value;
    win->props[XITK_X11_WT_PARENT].value = props[XITK_X11_WT_PARENT].value;
    d->d.lock (&d->d);
    XReparentWindow (d->display, win->w.id, pw ? pw->w.id : d->be->be.xitk->imlibdata->x.root,
      win->props[XITK_X11_WT_X].value, win->props[XITK_X11_WT_Y].value);
    d->d.unlock (&d->d);
  }

  img = NULL;
  hint.flags = 0;
  if (props[XITK_X11_WT_IMAGE].value) {
    /* setting the same image again means refresh. */
    img = (xitk_x11_image_t *)props[XITK_X11_WT_IMAGE].value;
    win->props[XITK_X11_WT_IMAGE].value = (uintptr_t)img;
    props[XITK_X11_WT_W].value = img->props[XITK_X11_IT_W].value;
    props[XITK_X11_WT_H].value = img->props[XITK_X11_IT_H].value;
  }
  if  ((props[XITK_X11_WT_X].value != win->props[XITK_X11_WT_X].value)
    || (props[XITK_X11_WT_Y].value != win->props[XITK_X11_WT_Y].value)) {
    if (win->props[XITK_X11_WT_WRAP].value != None) {
      XWindowAttributes attr;
      attr.root = None;
      if (XGetWindowAttributes (d->display, win->w.id, &attr)) {
/*      win->props[XITK_X11_WT_X].value = attr.x;
        win->props[XITK_X11_WT_Y].value = attr.y; */
        win->props[XITK_X11_WT_W].value = attr.width;
        win->props[XITK_X11_WT_H].value = attr.height;
      }
    }
    hint.x = win->props[XITK_X11_WT_X].value = props[XITK_X11_WT_X].value;
    hint.y = win->props[XITK_X11_WT_Y].value = props[XITK_X11_WT_Y].value;
    hint.flags |= PPosition;
  }
  if  ((props[XITK_X11_WT_W].value != win->props[XITK_X11_WT_W].value)
    || (props[XITK_X11_WT_H].value != win->props[XITK_X11_WT_H].value)) {
    if ((props[XITK_X11_WT_W].value <= 0) || (props[XITK_X11_WT_H].value <= 0)) {
      if (d->be->be.verbosity > 0)
        printf ("xitk.x11.window.resize (%p, %d, %d) ignored.\n",
          (void *)win, (int)props[XITK_X11_WT_W].value, (int)props[XITK_X11_WT_H].value);
    } else {
      hint.width = win->props[XITK_X11_WT_W].value = props[XITK_X11_WT_W].value;
      hint.height = win->props[XITK_X11_WT_H].value = props[XITK_X11_WT_H].value;
      hint.flags |= PSize;
    }
  }
  if (hint.flags || img) {
    d->d.lock (&d->d);
    if (hint.flags) {
      XSetWMNormalHints (d->display, win->w.id, &hint);
      XMoveResizeWindow (d->display, win->w.id,
        win->props[XITK_X11_WT_X].value, win->props[XITK_X11_WT_Y].value,
        win->props[XITK_X11_WT_W].value, win->props[XITK_X11_WT_H].value);
    }
    if (img) {
      XSetWindowBackgroundPixmap (d->display, win->w.id, img->img.id1);
      XShapeCombineMask (d->display, win->w.id, ShapeBounding, 0, 0, img->img.id2, ShapeSet);
      XClearWindow (d->display, win->w.id);
    }
    XSync (d->display, False);
    d->d.unlock (&d->d);
  }

  if (props[XITK_X11_WT_TRANSIENT_FOR].value != win->props[XITK_X11_WT_TRANSIENT_FOR].value) {
    xitk_x11_window_t *tw = (xitk_x11_window_t *)props[XITK_X11_WT_TRANSIENT_FOR].value;

    d->d.lock (&d->d);
    XSetTransientForHint (d->display, win->w.id, tw ? tw->w.id : None);
    d->d.unlock (&d->d);
    win->props[XITK_X11_WT_TRANSIENT_FOR].value = props[XITK_X11_WT_TRANSIENT_FOR].value;
  }

  if ((props[XITK_X11_WT_WIN_FLAGS].value ^ win->props[XITK_X11_WT_WIN_FLAGS].value)
    & (props[XITK_X11_WT_WIN_FLAGS].value >> 16))
    _xitk_x11_window_flags (win, props[XITK_X11_WT_WIN_FLAGS].value);

  return res;
}

static void xitk_x11_window_copy_rect (xitk_be_window_t *_win, xitk_be_image_t *_from,
  int x1, int y1, int w, int h, int x2, int y2, int sync) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;
  xitk_x11_image_t *from;

  xitk_container (win, _win, w);
  xitk_container (from, _from, img);
  if (!win || !from)
    return;
  d = win->d;
  if (win->w.id == None) {
    if (d->be->be.verbosity >= 1)
      printf ("xitk.x11.window.copy_rect (%s): ERROR: window closed already.\n",
        (const char *)win->props[XITK_X11_WT_TITLE].value);
    return;
  }

  {
    int v;

    v = win->props[XITK_X11_WT_W].value;
    if (x2 < 0) {
      w += x2;
      x2 = 0;
    }
    if (x2 >= v)
      return;
    v = win->props[XITK_X11_WT_H].value;
    if (y2 < 0) {
      h += y2;
      y2 = 0;
    }
    if (y2 >= v)
      return;
    v = from->props[XITK_X11_IT_W].value;
    if (x1 < 0) {
      w += x1;
      x1 = 0;
    }
    if (x1 >= v)
      return;
    v = from->props[XITK_X11_IT_H].value;
    if (y1 < 0) {
      h += y1;
      y1 = 0;
    }
    if (y1 >= v)
      return;
    if ((w <= 0) || (h <= 0))
      return;
  }

  /* NOTE: clip origin always refers to the full source image,
   * even with partial draws. */
  if (from->img.id2 != None) {
    XSetClipOrigin (d->display, d->gc1, x2 - x1, y2 - y1);
    XSetClipMask (d->display, d->gc1, from->img.id2);
    XCopyArea (d->display, from->img.id1, win->w.id, d->gc1, x1, y1, w, h, x2, y2);
    XSetClipMask (d->display, d->gc1, None);
  } else if (from->shared_mask.id != None) {
    XSetClipOrigin (d->display, d->gc1, x2 - from->shared_mask.x - x1, y2 - from->shared_mask.y - y1);
    XSetClipMask (d->display, d->gc1, from->shared_mask.id);
    XCopyArea (d->display, from->img.id1, win->w.id, d->gc1, x1, y1, w, h, x2, y2);
    XSetClipMask (d->display, d->gc1, None);
  } else {
    XCopyArea (d->display, from->img.id1, win->w.id, d->gc1, x1, y1, w, h, x2, y2);
  }
  if (sync)
    XSync (d->display, False);
}

static void xitk_x11_window_raise (xitk_be_window_t *_win) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;

  xitk_container (win, _win, w);
  if (!win)
    return;
  d = win->d;
  if (win->w.id != None) {
    d->d.lock (&d->d);
    XRaiseWindow (d->display, win->w.id);
    d->d.unlock (&d->d);
  }
}

static void xitk_x11_window_delete (xitk_be_window_t **_win) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;

  if (!_win)
    return;
  xitk_container (win, *_win, w);
  if (!win)
    return;
  *_win = NULL;
  d = win->d;
  if ((win->w.id != None) || win->gc) {
    d->d.lock (&d->d);
    if ((win->w.id != None) && (win->props[XITK_X11_WT_WRAP].value == None)) {
      XUnmapWindow (d->display, win->w.id);
      XDestroyWindow (d->display, win->w.id);
    }
    win->w.id = None;
    if (win->gc) {
      XFreeGC (d->display, win->gc);
      win->gc = NULL;
    }
    d->d.unlock (&d->d);
  }

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.window.delete (%s, %p).\n", win && win->props[XITK_X11_WT_TITLE].value
      ? (const char *)win->props[XITK_X11_WT_TITLE].value : "<unknown>", (void *)win);

  pthread_mutex_lock (&d->mutex);
  xitk_dnode_remove (&win->w.node);
  d->dnd_win = NULL;
  xitk_dnd_delete (win->dnd);
  win->dnd = NULL;
  _xitk_x11_clipboard_unregister_window (d, win->w.id);
  xitk_dlist_add_tail (&d->free_windows, &win->w.node);
  if (_xitk_x11_display_unref (d))
    pthread_mutex_unlock (&d->mutex);
}

static xitk_be_window_t *xitk_x11_window_new (xitk_be_display_t *_d, const xitk_tagitem_t *taglist) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;
  uint32_t want_flags;

  xitk_container (d, _d, d);
  if (!d)
    return NULL;

  pthread_mutex_lock (&d->mutex);
  if (!d->free_windows.head.next->next) {
    pthread_mutex_unlock (&d->mutex);
    return NULL;
  }
  win = (xitk_x11_window_t *)d->free_windows.head.next;
  xitk_dnode_remove (&win->w.node);
  pthread_mutex_unlock (&d->mutex);

  win->w.display = &d->d;
  win->d = d;

  memcpy (win->props, _xitk_x11_window_defaults, sizeof (win->props));
  win->props[XITK_X11_WT_TITLE].value = (uintptr_t)win->title;
  strcpy (win->title, "xiTK Window");
  xitk_tags_get (taglist, win->props);

  want_flags = _xitk_x11_window_merge_flags (
    win->props[XITK_X11_WT_WRAP].value != None ? 0 : 0xffff0000,
    win->props[XITK_X11_WT_WIN_FLAGS].value);
  win->props[XITK_X11_WT_WIN_FLAGS].value = XITK_WINF_DECORATED;

  do {
    long        data[1];
    XClassHint *xclasshint;
    XWMHints   *wm_hint;

    if (win->props[XITK_X11_WT_WRAP].value != None) {
      Window        rootwin;
      int           xwin, ywin;
      unsigned int  wwin, hwin, bwin, dwin;

      win->w.id = win->props[XITK_X11_WT_WRAP].value;
      d->d.lock (&d->d);
      if (XGetGeometry (d->display, win->w.id, &rootwin,
        &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
        win->props[XITK_X11_WT_X].value = xwin;
        win->props[XITK_X11_WT_Y].value = ywin;
        win->props[XITK_X11_WT_W].value = wwin;
        win->props[XITK_X11_WT_H].value = hwin;
      } else {
        d->d.unlock (&d->d);
        break;
      }
      d->d.unlock (&d->d);
    } else {
      XSetWindowAttributes   attr;
      XSizeHints             hint;

      if (win->props[XITK_X11_WT_IMAGE].value) {
        xitk_x11_image_t *img = (xitk_x11_image_t *)win->props[XITK_X11_WT_IMAGE].value;

        win->props[XITK_X11_WT_W].value = img->props[XITK_X11_IT_W].value;
        win->props[XITK_X11_WT_H].value = img->props[XITK_X11_IT_H].value;
      }

      if  ((win->props[XITK_X11_WT_X].value == XITK_XY_CENTER)
        || (win->props[XITK_X11_WT_Y].value == XITK_XY_CENTER)) {
        int x = -1, y;

        if (win->props[XITK_X11_WT_TRANSIENT_FOR].value) {
          xitk_x11_window_t *tw = (xitk_x11_window_t *)win->props[XITK_X11_WT_TRANSIENT_FOR].value;

          x = tw->props[XITK_X11_WT_X].value + (tw->props[XITK_X11_WT_W].value >> 1);
          y = tw->props[XITK_X11_WT_Y].value + (tw->props[XITK_X11_WT_H].value >> 1);
        } else {
          Window        rootwin;
          int           xwin, ywin;
          unsigned int  wwin, hwin, bwin, dwin;

          d->d.lock (&d->d);
          if (XGetGeometry (d->display, d->be->be.xitk->imlibdata->x.root, &rootwin,
            &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
            x = wwin >> 1;
            y = hwin >> 1;
          }
          d->d.unlock (&d->d);
        }
        if (x >= 0) {
          if (win->props[XITK_X11_WT_X].value == XITK_XY_CENTER)
            win->props[XITK_X11_WT_X].value = x - (win->props[XITK_X11_WT_W].value >> 1);
          if (win->props[XITK_X11_WT_Y].value == XITK_XY_CENTER)
            win->props[XITK_X11_WT_Y].value = y - (win->props[XITK_X11_WT_H].value >> 1);
        }
      }

      memset (&hint, 0, sizeof (hint));
      hint.x               = win->props[XITK_X11_WT_X].value;
      hint.y               = win->props[XITK_X11_WT_Y].value;
      hint.width           =
      hint.base_width      =
      hint.min_width       =
      hint.max_width       = win->props[XITK_X11_WT_W].value;
      hint.height          =
      hint.base_height     =
      hint.min_height      =
      hint.max_height      = win->props[XITK_X11_WT_H].value;
      hint.win_gravity     = NorthWestGravity;
      hint.flags           = PWinGravity | PBaseSize | PMinSize | PMaxSize | USSize | USPosition;

      /* applied this here already.
       * BTW. this is not very useful with kwin4 because
       * 1. window is always on top of the world,
       * 2. window may be moved over the screen border, and
       * 3. while window has focus, alt-tab reports "no windows available". */
      attr.override_redirect = (want_flags & XITK_WINF_OVERRIDE_REDIRECT) ? True : False;
      win->props[XITK_X11_WT_WIN_FLAGS].value |= want_flags & XITK_WINF_OVERRIDE_REDIRECT;
      want_flags &= ~(XITK_WINF_OVERRIDE_REDIRECT << 16);

      attr.background_pixel  =
      attr.border_pixel      = xitk_get_cfg_num (d->be->be.xitk, XITK_BLACK_COLOR);
      attr.colormap          = Imlib_get_colormap (d->be->be.xitk->imlibdata);
      attr.win_gravity       = NorthWestGravity;

      attr.event_mask =
        KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
        EnterWindowMask | LeaveWindowMask | PointerMotionMask | ButtonMotionMask |
        KeymapStateMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask |
        SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask | ColormapChangeMask;

      d->d.lock (&d->d);
      win->w.id = XCreateWindow (d->display, d->be->be.xitk->imlibdata->x.root,
        hint.x, hint.y, hint.width, hint.height, 0, d->be->be.xitk->imlibdata->x.depth,
        InputOutput, d->be->be.xitk->imlibdata->x.visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect | CWWinGravity | CWEventMask, &attr);
      if (win->w.id == None) {
        d->d.unlock (&d->d);
        break;
      }
      XmbSetWMProperties (d->display, win->w.id, win->title, win->title, NULL, 0, &hint, NULL, NULL);
    
      data[0] = 10;
      XChangeProperty (d->display, win->w.id, d->atoms[XITK_A_WIN_LAYER], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
    }

    XSelectInput (d->display, win->w.id, INPUT_MOTION | KeymapStateMask);

    XSetWMProtocols (d->display, win->w.id, &d->atoms[XITK_A_WM_DELETE_WINDOW], 1);

    if ((xclasshint = XAllocClassHint ()) != NULL) {
      xclasshint->res_name  = ((char *)win->props[XITK_X11_WT_RES_NAME].value
                            ? (char *)win->props[XITK_X11_WT_RES_NAME].value : "Xine Window");
      xclasshint->res_class = ((char *)win->props[XITK_X11_WT_RES_CLASS].value
                            ? (char *)win->props[XITK_X11_WT_RES_CLASS].value : "Xitk");
      XSetClassHint (d->display, win->w.id, xclasshint);
      XFree (xclasshint);
    }

    if (win->props[XITK_X11_WT_IMAGE].value) {
      xitk_x11_image_t *img = (xitk_x11_image_t *)win->props[XITK_X11_WT_IMAGE].value;

      XSetWindowBackgroundPixmap (d->display, win->w.id, img->img.id1);
      XShapeCombineMask (d->display, win->w.id, ShapeBounding, 0, 0, img->img.id2, ShapeSet);
      XClearWindow (d->display, win->w.id);
    }

    if (win->props[XITK_X11_WT_TRANSIENT_FOR].value) {
      xitk_x11_window_t *tw = (xitk_x11_window_t *)win->props[XITK_X11_WT_TRANSIENT_FOR].value;

      XSetTransientForHint (d->display, win->w.id, tw->w.id);
    }

    if (win->props[XITK_X11_WT_WRAP].value == None) {
      xitk_x11_image_t *icon = (xitk_x11_image_t *)win->props[XITK_X11_WT_ICON].value;
      if ((wm_hint = XAllocWMHints ()) != NULL) {
        wm_hint->input         = True;
        wm_hint->initial_state = NormalState;
        wm_hint->icon_pixmap   = icon ? icon->img.id1 : None;
        wm_hint->icon_mask     = icon ? icon->img.id2 : None;
        wm_hint->flags         = InputHint | StateHint | (icon ? (IconPixmapHint | IconMaskHint) : 0);
        XSetWMHints (d->display, win->w.id, wm_hint);
        XFree (wm_hint);
      }
    }

    _xitk_x11_window_flags (win, want_flags);

    win->gc = XCreateGC (d->display, win->w.id, 0, NULL);
    d->d.unlock (&d->d);
#if 0
    if (layer_above)
      xitk_window_set_window_layer (xwin, layer_above);
#endif
    win->w.magic = _XITK_X11_WINDOW_MAGIC;
    win->w.type = XITK_BE_TYPE_X11;
    win->w.data = NULL;
    win->w._delete = xitk_x11_window_delete;
    win->w.get_props = xitk_x11_window_get_props;
    win->w.get_clip_utf8 = xitk_x11_window_get_utf8;
    win->w.set_props = xitk_x11_window_set_props;
    win->w.set_clip_utf8 = xitk_x11_window_set_utf8;
    win->w.copy_rect = xitk_x11_window_copy_rect;
    win->w.raise = xitk_x11_window_raise;

    pthread_mutex_lock (&d->mutex);
    d->refs += 1;
    xitk_dlist_add_tail (&d->d.windows, &win->w.node);
    pthread_mutex_unlock (&d->mutex);

    if (d->be->be.verbosity >= 2)
      printf ("xitk.x11.window.new (%p) = %p.\n", (void *)d, (void *)win);

    return &win->w;
  } while (0);

  pthread_mutex_lock (&d->mutex);
  xitk_dlist_add_head (&d->d.windows, &win->w.node);
  pthread_mutex_unlock (&d->mutex);

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.window.new (%p) failed.\n", (void *)d);

  return NULL;
}

static int xitk_x11_next_event (xitk_be_display_t *_d, xitk_be_event_t *event,
  xitk_be_window_t *_win, xitk_be_event_type_t type, int timeout) {
  xitk_x11_display_t *d;
  xitk_x11_window_t *win;

  XEvent xevent;
  KeySym ksym;
  Window w11;

  xitk_container (d, _d, d);
  xitk_container (win, _win, w);
  if (!d || !event)
    return 0;

  if (d->dnd_win) {
    pthread_mutex_lock (&d->mutex);
    if (d->dnd_win && (!win || (win == d->dnd_win)) && ((type == XITK_EV_ANY) || (type == XITK_EV_DND))) {
      int r;
      win = d->dnd_win;
      r = xitk_dnd_client_message (win->dnd, &xevent, d->utf8, sizeof (d->utf8));
      if (r >= 2) {
        event->type = XITK_EV_DND;
        event->window = &win->w;
        event->parent = NULL;
        event->code = 0;
        event->sym = 0;
        event->qual = 0;
        event->id = 0;
        event->x = 0;
        event->y = 0;
        event->w = 0;
        event->h = 0;
        event->time = 0;
        event->more = strlen (d->utf8);
        event->utf8 = d->utf8;
        pthread_mutex_unlock (&d->mutex);
        return 1;
      }
      d->dnd_win = NULL;
    }
    pthread_mutex_unlock (&d->mutex);
  }

  while (1) {
    Status res;

    d->d.lock (&d->d);
    if (win) {
      if ((type > XITK_EV_ANY) && (type < XITK_EV_CLIP_READY)) {
        res = XCheckTypedWindowEvent (d->display, win->w.id, _xitk_x11_event_type[type], &xevent);
      } else {
        res = XCheckWindowEvent (d->display, win->w.id, ~0, &xevent);
      }
    } else {
      if ((type > XITK_EV_ANY) && (type < XITK_EV_CLIP_READY)) {
        res = XCheckTypedEvent (d->display, _xitk_x11_event_type[type], &xevent);
      } else {
        /* NOTE: res = XCheckMaskEvent (d->display, ~0, &xevent); would be the natural choice here.
         * however, that seems to filter out some interesting stuff. */
        res = XPending (d->display);
        if (res != False)
          XNextEvent (d->display, &xevent);
      }
    }
    d->d.unlock (&d->d);
    if (res != False)
      break;

    if (timeout > 0) {
      fd_set fdset;
      struct timeval tv;

      FD_ZERO (&fdset);
      FD_SET (d->fd, &fdset);
      tv.tv_sec  = 0;
      tv.tv_usec = timeout * 1000;
      if (select (d->fd + 1, &fdset, 0, 0, &tv) <= 0)
        return 0;
    } else {
      return 0;
    }
  }

  w11 = xevent.xany.type == CreateNotify ? xevent.xcreatewindow.window
      : xevent.xany.type == DestroyNotify ? xevent.xdestroywindow.window
      : xevent.xany.type == MapNotify ? xevent.xmap.window
      : xevent.xany.type == UnmapNotify ? xevent.xunmap.window
      : xevent.xany.type == ConfigureNotify ? xevent.xconfigure.window
      : xevent.xany.window;
  if (!win) {
    pthread_mutex_lock (&d->mutex);
    for (win = (xitk_x11_window_t *)d->d.windows.head.next; win->w.node.next; win = (xitk_x11_window_t *)win->w.node.next)
      if (win->w.id == w11)
        break;
    pthread_mutex_unlock (&d->mutex);
    if (!win->w.node.next)
      win = NULL;
  }
  event->id = w11;
  event->from_peer = (xevent.xany.send_event != False);
  event->code = 0;
  event->sym = 0;
  event->more = 0;
  event->time = 0;
  event->window = &win->w;
  event->parent = NULL;
  event->utf8 = NULL;

  switch (xevent.xany.type) {
    case KeyPress:
      event->type = XITK_EV_KEY_DOWN;
      goto _key_rest;

    case KeyRelease:
      event->type = XITK_EV_KEY_UP;
    _key_rest:
      event->qual = _xitk_x11_get_modifier (xevent.xkey.state);
      event->code = xevent.xkey.keycode;
      event->x = xevent.xkey.x;
      event->y = xevent.xkey.y;
      event->w = xevent.xkey.x_root;
      event->h = xevent.xkey.y_root;
      event->time = xevent.xkey.time;
      event->more = _xitk_x11_keyevent_2_string (d, &xevent, &ksym, d->utf8, sizeof (d->utf8));
      event->sym = ksym;
      event->utf8 = d->utf8;
      break;

    case ButtonPress:
      event->type = XITK_EV_BUTTON_DOWN;
      goto _button_rest;

    case ButtonRelease:
      event->type = XITK_EV_BUTTON_UP;
    _button_rest:
      event->qual = _xitk_x11_get_modifier (xevent.xbutton.state);
      event->code = xevent.xbutton.button;
      event->x = xevent.xbutton.x;
      event->y = xevent.xbutton.y;
      event->w = xevent.xbutton.x_root;
      event->h = xevent.xbutton.y_root;
      event->time = xevent.xbutton.time;
      break;

    case MotionNotify:
      event->type = XITK_EV_MOVE;
      event->qual = _xitk_x11_get_modifier (xevent.xmotion.state);
      event->x = xevent.xmotion.x;
      event->y = xevent.xmotion.y;
      event->w = xevent.xmotion.x_root;
      event->h = xevent.xmotion.y_root;
      event->time = xevent.xmotion.time;
      break;

    case EnterNotify:
      event->type = XITK_EV_ENTER;
      goto _el_rest;

    case LeaveNotify:
      event->type = XITK_EV_LEAVE;
    _el_rest:
      event->qual = _xitk_x11_get_modifier (xevent.xcrossing.state);
      event->x = xevent.xcrossing.x;
      event->y = xevent.xcrossing.y;
      event->w = xevent.xcrossing.x_root;
      event->h = xevent.xcrossing.y_root;
      event->code = xevent.xcrossing.mode == NotifyNormal ? 0
                  : xevent.xcrossing.mode == NotifyGrab ? 1 : 2;
      event->time = xevent.xcrossing.time;
      break;

    case FocusIn:
      if (win)
        win->props[XITK_X11_WT_WIN_FLAGS].value |= XITK_WINF_FOCUS;
      event->type = XITK_EV_FOCUS;
      event->qual = 0;
      event->x = 0;
      event->y = 0;
      event->w = 0;
      event->h = 0;
      break;

    case FocusOut:
      if (win)
        win->props[XITK_X11_WT_WIN_FLAGS].value &= ~XITK_WINF_FOCUS;
      event->type = XITK_EV_UNFOCUS;
      event->qual = 0;
      event->x = 0;
      event->y = 0;
      event->w = 0;
      event->h = 0;
      break;

    case KeymapNotify:
      break;

    case Expose:
      event->type = XITK_EV_EXPOSE;
      event->qual = 0;
      event->x = xevent.xexpose.x;
      event->y = xevent.xexpose.y;
      event->w = xevent.xexpose.width;
      event->h = xevent.xexpose.height;
      event->more = xevent.xexpose.count;
      break;

    case CreateNotify:
      event->type = XITK_EV_NEW_WIN;
      event->qual = 0;
      event->x = xevent.xcreatewindow.x;
      event->y = xevent.xcreatewindow.y;
      event->w = xevent.xcreatewindow.width;
      event->h = xevent.xcreatewindow.height;
      if (win) {
        win->props[XITK_X11_WT_X].value = xevent.xcreatewindow.x;
        win->props[XITK_X11_WT_Y].value = xevent.xcreatewindow.y;
        win->props[XITK_X11_WT_W].value = xevent.xcreatewindow.width;
        win->props[XITK_X11_WT_H].value = xevent.xcreatewindow.height;
      }
      break;

    case DestroyNotify:
      event->type = XITK_EV_DEL_WIN;
      if (!win)
        goto _ev_zero;
      if (!event->from_peer) {
        event->code = 1;
        _xitk_x11_clipboard_unregister_window (d, win->w.id);
        win->w.id = None;
      } else {
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.window.close_request (%s).\n", win->props[XITK_X11_WT_TITLE].value ?
            (const char *)win->props[XITK_X11_WT_TITLE].value : "<unknown>");
        event->type = XITK_EV_DEL_WIN;
        event->code = 0;
      }
      goto _ev_zero;

    case UnmapNotify:
      event->type = XITK_EV_HIDE;
      goto _ev_zero;

    case MapNotify:
      event->type = XITK_EV_SHOW;
    _ev_zero:
      event->qual = 0;
      event->x = 0;
      event->y = 0;
      event->w = 0;
      event->h = 0;
      break;

    case ReparentNotify:
      event->type = XITK_EV_REPARENT;
      event->qual = 0;
      {
        xitk_x11_window_t *pwin;

        pthread_mutex_lock (&d->mutex);
        for (pwin = (xitk_x11_window_t *)d->d.windows.head.next; pwin->w.node.next; pwin = (xitk_x11_window_t *)pwin->w.node.next)
          if (pwin->w.id == xevent.xreparent.parent)
        break;
        pthread_mutex_unlock (&d->mutex);
        if (!pwin->w.node.next)
          pwin = NULL;
        event->parent = &pwin->w;
      }
      event->x = xevent.xconfigure.x;
      event->y = xevent.xconfigure.y;
      event->w = 0;
      event->h = 0;
      break;

    case ConfigureNotify:
      event->type = XITK_EV_POS_SIZE;
      event->qual = 0;
      event->x = xevent.xconfigure.x;
      event->y = xevent.xconfigure.y;
      event->w = xevent.xconfigure.width;
      event->h = xevent.xconfigure.height;
      if (win) {
        win->props[XITK_X11_WT_X].value = xevent.xconfigure.x;
        win->props[XITK_X11_WT_Y].value = xevent.xconfigure.y;
        win->props[XITK_X11_WT_W].value = xevent.xconfigure.width;
        win->props[XITK_X11_WT_H].value = xevent.xconfigure.height;
      }
      break;

    case PropertyNotify:
      if (d->be->be.verbosity >= 2) {
        char *name;
        d->d.lock (&d->d);
        name = XGetAtomName (d->display, xevent.xproperty.atom);
        d->d.unlock (&d->d);
        printf ("xitk.x11.window.property.change (%s, %s).\n",
          win && win->props[XITK_X11_WT_TITLE].value ? (const char *)win->props[XITK_X11_WT_TITLE].value : "<unknown>",
        name ? name : "<unknown>");
        XFree (name);
      }
      if (win && (xevent.xproperty.atom == d->atoms[XITK_A_WM_STATE])) {
        Atom actual_type;
        int actual_format = 0, res;
        unsigned long nitems, bytes_after;
        int *prop = NULL;

        d->d.lock (&d->d);
        res = XGetWindowProperty (d->display, win->w.id, xevent.xproperty.atom, 0, 4, False,
          d->clipboard.atoms[XITK_A_integer], &actual_type, &actual_format, &nitems, &bytes_after,
          (unsigned char **)&prop);
        d->d.unlock (&d->d);
        if ((res != False) && (nitems >= 1)) {
          win->props[XITK_X11_WT_WIN_FLAGS].value &= ~(XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED);
          win->props[XITK_X11_WT_WIN_FLAGS].value |=
            *prop == NormalState ? XITK_WINF_VISIBLE : *prop == IconicState ? XITK_WINF_ICONIFIED : 0;
        }
        XFree (prop);
      }
      /* fall through */
    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
      if (win) {
        int r = _xitk_x11_clipboard_event (d, &xevent);
        if (r > 0) {
          if (r == 1)
            return 0;
          event->type = XITK_EV_CLIP_READY;
          event->qual = 0;
          event->x = 0;
          event->y = 0;
          event->w = 0;
          event->h = 0;
          break;
        }
      }
      /* fall through */
    case ClientMessage:
      if (win && win->dnd) {
        int r = xitk_dnd_client_message (win->dnd, &xevent, d->utf8, sizeof (d->utf8));
        if (r >= 2) {
          pthread_mutex_lock (&d->mutex);
          d->dnd_win = win;
          pthread_mutex_unlock (&d->mutex);
          event->type = XITK_EV_DND;
          event->more = strlen (d->utf8);
          event->utf8 = d->utf8;
          event->qual = 0;
          event->id = 0;
          event->x = 0;
          event->y = 0;
          event->w = 0;
          event->h = 0;
          break;
        }
        if (r == 1)
        return 0;
      }
      if (win && (xevent.xclient.message_type == d->atoms[XITK_A_WM_PROTOCOLS])
        && ((Atom)xevent.xclient.data.l[0] == d->atoms[XITK_A_WM_DELETE_WINDOW])) {
        if (d->be->be.verbosity >= 2)
          printf ("xitk.x11.window.close_request (%s).\n", win->props[XITK_X11_WT_TITLE].value ?
            (const char *)win->props[XITK_X11_WT_TITLE].value : "<unknown>");
        event->type = XITK_EV_DEL_WIN;
        event->code = 0;
        goto _ev_zero;
      }
      return 0;

    case MappingNotify:
      d->d.lock (&d->d);
      XRefreshKeyboardMapping (&xevent.xmapping);
      d->d.unlock (&d->d);
      return 0;

    case VisibilityNotify:
    case GraphicsExpose:
    case NoExpose:
    case MapRequest:
    case ColormapNotify:
    case GravityNotify:
    case ResizeRequest:
    case CirculateNotify:
    case CirculateRequest:
    case ConfigureRequest:
    case GenericEvent:
    default: return 0;
  }
  return 1;
}

static const char *xitk_x11_event_name (xitk_be_display_t *_d, const xitk_be_event_t *event) {
  xitk_x11_display_t *d;
  const char *s = NULL;

  xitk_container (d, _d, d);
  if (!d || !event)
    return NULL;
  if ((event->type == XITK_EV_KEY_DOWN) || (event->type == XITK_EV_KEY_UP)) {
    s = XKeysymToString (event->sym);
    if (s)
      strlcpy (d->name, s, sizeof (d->name));
    else
      sprintf (d->name, "XKey_%u", (unsigned int)event->code);
    return d->name;
  }
  if ((event->type == XITK_EV_BUTTON_DOWN) || (event->type == XITK_EV_BUTTON_UP)) {
    sprintf (d->name, "XButton_%d", (unsigned int)event->code);
    return d->name;
  }
  return NULL;
}

static int xitk_x11_color_new (xitk_be_display_t *_d, xitk_color_info_t *info) {
  xitk_x11_display_t *d;
  XColor xcolor;
  uint32_t v;

  xitk_container (d, _d, d);
  if (!d)
    return 0;

  xcolor.flags = DoRed | DoBlue | DoGreen;
  v = info->want >> 16;
  xcolor.red   = (v << 8) + v;
  v = (info->want >> 8) & 0xff;
  xcolor.green = (v << 8) + v;
  v = info->want & 0xff;
  xcolor.blue  = (v << 8) + v;

  d->d.lock (&d->d);
  XAllocColor (d->display, Imlib_get_colormap (d->be->be.xitk->imlibdata), &xcolor);
  d->d.unlock (&d->d);

  info->value = xcolor.pixel;
  info->r = xcolor.red;
  info->g = xcolor.green;
  info->b = xcolor.blue;
  info->a = ~0;

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.color.new (0x%0x) = 0x%0x.\n", (unsigned int)info->want, (unsigned int)xcolor.pixel);
  return 1;
}

static void xitk_x11_color_delete  (xitk_be_display_t *_d, uint32_t value) {
  xitk_x11_display_t *d;
  unsigned long v = value;

  xitk_container (d, _d, d);
  if (!d)
    return;

  d->d.lock (&d->d);
  XFreeColors (d->display, Imlib_get_colormap (d->be->be.xitk->imlibdata), &v, 1, 0);
  d->d.unlock (&d->d);

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.color.delete (0x%0x).\n", (unsigned int)value);
}

static xitk_be_display_t *xitk_x11_open_display (xitk_backend_t *_be, const char *name, int use_lock, int use_sync) {
  static const unsigned int ctrl_syms[XITK_KEY_LASTCODE] = {
    0,
    XK_Escape,
    XK_Return,
    XK_KP_Enter,
    XK_ISO_Enter,
    XK_Left,
    XK_Right,
    XK_Up,
    XK_Down,
    XK_Home,
    XK_End,
    XK_Page_Up,
    XK_Page_Down,
    XK_Tab,
    XK_KP_Tab,
    XK_ISO_Left_Tab,
    XK_Insert,
    XK_Delete,
    XK_BackSpace,
    XK_Print,
    XK_Scroll_Lock,
    XK_Pause,
    XK_F1,
    XK_F2,
    XK_F3,
    XK_F4,
    XK_F5,
    XK_F6,
    XK_F7,
    XK_F8,
    XK_F9,
    XK_F10,
    XK_F11,
    XK_F12,
    XK_Prior,
    XK_Next,
    XK_Cancel,
    XK_Menu,
    XK_Help,
    0xffffffff,
    0xffffffff
  };
  xitk_x11_backend_t *be;
  xitk_x11_display_t *d;
  Display *display;

  if (!_be)
    return NULL;
  xitk_container (be, _be, be);

  if (!name)
    name = getenv ("DISPLAY");
  display = XOpenDisplay (name);
  if (!display) {
    printf (_("Cannot open display\n"));
    return NULL;
  }
  if (use_sync) {
    XSynchronize (display, True);
    printf (_("Warning! Synchronized X activated - this is very slow...\n"));
  }

  d = calloc (1, sizeof (*d));
  if (!d) {
    XCloseDisplay (display);
    return NULL;
  }
  d->d.be = &be->be;
  d->be = be;
  d->display = display;
  d->d.id = (uintptr_t)display;
  d->fd = XConnectionNumber (display);

  d->testpix = NULL;

  d->default_screen = DefaultScreen (display);
  d->gc1 = d->gc2 = NULL;
  xitk_dnode_init (&d->d.node);
  xitk_dlist_init (&d->d.images);
  xitk_dlist_init (&d->d.windows);
  xitk_dlist_init (&d->free_windows);
  {
    uint32_t u;

    for (u = 0; u < sizeof (d->windows) / sizeof (d->windows[0]); u++) {
      xitk_x11_window_t *win = d->windows + u;

      xitk_dnode_init (&win->w.node);
      xitk_dlist_add_tail (&d->free_windows, &win->w.node);
    }
  }
  d->dnd_win = NULL;
  d->d.magic = _XITK_X11_DISPLAY_MAGIC;
  d->d.type = XITK_BE_TYPE_X11;
  d->d.id = (uintptr_t)d->display;
  d->refs = 1;
  pthread_mutex_init (&d->mutex, NULL);

  d->ctrl_keysyms1 = xine_sarray_new (XITK_KEY_LASTCODE, _xitk_x11_ctrl_keysyms_cmp);
  if (d->ctrl_keysyms1) {
    int i;
    for (i = 0; i < XITK_KEY_LASTCODE; i++)
      xine_sarray_add (d->ctrl_keysyms1, (void *)(uintptr_t)ctrl_syms[i]);
    for (i = 0; i < XITK_KEY_LASTCODE; i++) {
      int j = xine_sarray_binary_search (d->ctrl_keysyms1, (void *)(uintptr_t)ctrl_syms[i]);
      if (j)
        d->ctrl_keysyms2[j] = i;
    }
  }

  if (use_lock) {
    d->d.lock   = xitk_x11_display_lock;
    d->d.unlock = xitk_x11_display_unlock;
  } else {
    d->d.lock   = xitk_x11_display_nolock;
    d->d.unlock = xitk_x11_display_nolock;
  }

  _xitk_x11_clipboard_init (d);

  {
    static const char * const names[XITK_A_LAST] = {
      [XITK_A_WIN_LAYER]       = "_WIN_LAYER",
      [XITK_A__MOTIF_WM_HINTS] = "_MOTIF_WM_HINTS",
      [XITK_A_WM_CHANGE_STATE] = "WM_CHANGE_STATE",
      [XITK_A_WM_STATE]        = "WM_STATE",
      [XITK_A_WM_DELETE_WINDOW]= "WM_DELETE_WINDOW",
      [XITK_A_WM_PROTOCOLS]    = "WM_PROTOCOLS",
      [XITK_A__NET_WM_NAME]    = "_NET_WM_NAME",

      [XITK_A__NET_WM_WINDOW_TYPE]         = "_NET_WM_WINDOW_TYPE",
      [XITK_A__NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
      [XITK_A__NET_WM_WINDOW_TYPE_DOCK]    = "_NET_WM_WINDOW_TYPE_DOCK",
      [XITK_A__NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
      [XITK_A__NET_WM_WINDOW_TYPE_MENU]    = "_NET_WM_WINDOW_TYPE_MENU",
      [XITK_A__NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
      [XITK_A__NET_WM_WINDOW_TYPE_SPLASH]  = "_NET_WM_WINDOW_TYPE_SPLASH",
      [XITK_A__NET_WM_WINDOW_TYPE_DIALOG]  = "_NET_WM_WINDOW_TYPE_DIALOG",
      [XITK_A__NET_WM_WINDOW_TYPE_NORMAL]  = "_NET_WM_WINDOW_TYPE_NORMAL",

      [XITK_A__NET_WM_STATE]               = "_NET_WM_STATE",
      [XITK_A__NET_WM_STATE_MODAL]         = "_NET_WM_STATE_MODAL",
      [XITK_A__NET_WM_STATE_STICKY]        = "_NET_WM_STATE_STICKY",
      [XITK_A__NET_WM_STATE_MAXIMIZED_VERT]= "_NET_WM_STATE_MAXIMIZED_VERT",
      [XITK_A__NET_WM_STATE_MAXIMIZED_HORZ]= "_NET_WM_STATE_MAXIMIZED_HORZ",
      [XITK_A__NET_WM_STATE_SHADED]        = "_NET_WM_STATE_SHADED",
      [XITK_A__NET_WM_STATE_SKIP_TASKBAR]  = "_NET_WM_STATE_SKIP_TASKBAR",
      [XITK_A__NET_WM_STATE_SKIP_PAGER]    = "_NET_WM_STATE_SKIP_PAGER",
      [XITK_A__NET_WM_STATE_HIDDEN]        = "_NET_WM_STATE_HIDDEN",
      [XITK_A__NET_WM_STATE_FULLSCREEN]    = "_NET_WM_STATE_FULLSCREEN",
      [XITK_A__NET_WM_STATE_ABOVE]         = "_NET_WM_STATE_ABOVE",
      [XITK_A__NET_WM_STATE_BELOW]         = "_NET_WM_STATE_BELOW",
      [XITK_A__NET_WM_STATE_DEMANDS_ATTENTION] = "_NET_WM_STATE_DEMANDS_ATTENTION",
    };
    d->d.lock (&d->d);
    XInternAtoms (d->display, (char **)names, XITK_A_LAST, False, d->atoms);
    d->d.unlock (&d->d);
  }

  d->d.close = xitk_x11_display_close;
  d->d.next_event = xitk_x11_next_event;
  d->d.event_name = xitk_x11_event_name;
  d->d.image_new  = xitk_x11_image_new;
  d->d.window_new = xitk_x11_window_new;
  d->d.color._new = xitk_x11_color_new;
  d->d.color._delete = xitk_x11_color_delete;

  pthread_mutex_lock (&be->mutex);
  be->refs += 1;
  xitk_dlist_add_tail (&be->be.displays, &d->d.node);
  pthread_mutex_unlock (&be->mutex);

  if (d->be->be.verbosity > 1)
    dump_xfree_info (d->display, d->default_screen, d->be->be.verbosity);

  if (d->be->be.verbosity >= 2)
    printf ("xitk.x11.display.new (%p) = %p.\n", (void *)be, (void *)d);

  return &d->d;
}

static void xitk_x11_backend_delete (xitk_backend_t **_be) {
  xitk_x11_backend_t *be;

  if (!_be)
    return;
  xitk_container (be, *_be, be);
  if (!be)
    return;
  if (be->be.verbosity >= 2)
    printf ("xitk.x11.delete (%p).\n", (void *)be);
  pthread_mutex_lock (&be->mutex);
  if (_xitk_x11_backend_unref (be))
    pthread_mutex_unlock (&be->mutex);
  *_be = NULL;
}


xitk_backend_t *xitk_backend_new (xitk_t *xitk, int verbosity) {
  xitk_x11_backend_t *be;

  if (!xitk)
    return NULL;

  if (XInitThreads () == False) {
    if (verbosity > 0)
      printf (_("\nXInitThreads failed - looks like you don't have a thread-safe xlib.\n"));
    return NULL;
  }

  be = calloc (1, sizeof (*be));
  if (!be)
    return NULL;

  xitk_dnode_init (&be->be.node);
  xitk_dlist_init (&be->be.displays);
  be->be.magic = _XITK_X11_BE_MAGIC;
  be->be.type = XITK_BE_TYPE_X11;
  be->be.xitk = xitk;
  be->be.verbosity = verbosity;

  be->refs = 1;
  pthread_mutex_init (&be->mutex, NULL);

  be->be._delete = xitk_x11_backend_delete;
  be->be.open_display = xitk_x11_open_display;

  if (be->be.verbosity >= 2)
    printf ("xitk.x11.new () = %p.\n", (void *)be);

  return &be->be;
}
