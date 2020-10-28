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
 * Public part of xiTK backend.
 */

#ifndef XITK_BACKEND_H
#define XITK_BACKEND_H

#include <stdint.h>
#include "dlist.h"

typedef struct xitk_backend_s xitk_backend_t;
typedef struct xitk_be_display_s xitk_be_display_t;
typedef struct xitk_be_image_s xitk_be_image_t;
typedef struct xitk_be_window_s xitk_be_window_t;

typedef enum {
  XITK_BE_MASK_NO = 0, /* default */
  XITK_BE_MASK_YES,    /* implicit when copying from masked image */
  XITK_BE_MASK_SHARED  /* if supported by backend, otherwise same as YES */
} xitk_be_mask_t;

typedef enum {
  XITK_TAG_END = 0, /* 0 */
  XITK_TAG_SKIP,    /* num_items */
  XITK_TAG_INSERT,  /* xitk_tagitem_t * */
  XITK_TAG_JUMP,    /* xitk_tagitem_t * */
  XITK_TAG_BUFSIZE, /* size_t. > 0 enables string copying. */
#define XITK_XY_CENTER 0x7fffffff
  XITK_TAG_X,       /* int */
  XITK_TAG_Y,       /* int */
  XITK_TAG_WIDTH,   /* int */
  XITK_TAG_HEIGHT,  /* int */
  XITK_TAG_MODE,    /* int */
  XITK_TAG_LAYER_ABOVE,  /* int */
  XITK_TAG_FG,      /* uint32_t */
  XITK_TAG_BG,      /* uint32_t */
  XITK_TAG_IMAGE,   /* xitk_be_image_t * */
  XITK_TAG_WINDOW,  /* xitk_be_window_t * */
  XITK_TAG_ICON,    /* xitk_be_image_t * */
  XITK_TAG_WRAP,    /* existing window id */
  XITK_TAG_RAW,     /* const char * */
  XITK_TAG_PARENT,  /* xitk_be_window_t * */
  XITK_TAG_TRANSIENT_FOR,  /* xitk_be_window_t * */
  XITK_TAG_STATE,   /* xitk_be_window_state_t * */
  XITK_TAG_BORDER,  /* bool */
  XITK_TAG_OVERRIDE_REDIRECT,  /* bool */
  XITK_TAG_MASK,    /* xitk_be_mask_t */
  XITK_TAG_FILEBUF, /* const char * */
  XITK_TAG_FILESIZE,/* size_t */
  XITK_TAG_NAME,    /* (const) char * */
  XITK_TAG_FILENAME,/* (const) char * */
  XITK_TAG_FONTNAME,/* (const) char * */
  XITK_TAG_TITLE,   /* (const) char * */
  XITK_TAG_RES_NAME,/* (const) char * */
  XITK_TAG_RES_CLASS,/* (const) char * */
  XITK_TAG_LAST
} xitk_tag_type_t;

typedef struct {
  xitk_tag_type_t type;
  uintptr_t value;
} xitk_tagitem_t;

/* generic helper, not to be implemented by backend. */
int xitk_tags_get (const xitk_tagitem_t *from, xitk_tagitem_t *to);

typedef struct {
  uint16_t x1, y1, x2, y2;
} xitk_be_line_t;
typedef struct {
  uint16_t x, y, w, h;
} xitk_be_rect_t;

typedef enum {
  XITK_BE_TYPE_X11 = 1,
  XITK_BE_TYPE_WAYLAND = 2
} xitk_be_type_t;

typedef enum {
  XITK_EV_ANY = 0,
  XITK_EV_NEW_WIN,
  XITK_EV_DEL_WIN,
  XITK_EV_SHOW,
  XITK_EV_HIDE,
  XITK_EV_REPARENT,
  XITK_EV_FOCUS,
  XITK_EV_UNFOCUS,
  XITK_EV_ENTER,
  XITK_EV_LEAVE,
  XITK_EV_EXPOSE,
  XITK_EV_POS_SIZE,
  XITK_EV_MOVE,
  XITK_EV_BUTTON_DOWN,
  XITK_EV_BUTTON_UP,
  XITK_EV_KEY_DOWN,
  XITK_EV_KEY_UP,
  XITK_EV_CLIP_READY,
  XITK_EV_DND,
  XITK_EV_LAST
} xitk_be_event_type_t;

typedef struct {
  xitk_be_event_type_t type;
  uint32_t code, qual, from_peer;
  uintptr_t id;
  xitk_be_window_t *window, *parent;
  int x, y, w, h, more, time;
  const char *utf8;
} xitk_be_event_t;

struct xitk_be_image_s {
  xitk_dnode_t node;
  uint32_t magic;
  xitk_be_type_t type;
  xitk_be_display_t *display;
  uintptr_t id1, id2;
  /* for appliction use */
  void *data;

  void (*_delete) (xitk_be_image_t **image);

  int  (*get_props) (xitk_be_image_t *image, xitk_tagitem_t *taglist);
  int  (*pixel_is_visible) (xitk_be_image_t *image, int x, int y);

  int  (*set_props) (xitk_be_image_t *image, const xitk_tagitem_t *taglist);
  void (*draw_lines) (xitk_be_image_t *image, const xitk_be_line_t *lines, int num_lines, uint32_t color, int mask);
  void (*fill_rects) (xitk_be_image_t *image, const xitk_be_rect_t *rects, int num_rects, uint32_t color, int mask);
  void (*copy_rect)  (xitk_be_image_t *image, xitk_be_image_t *from, int x1, int y1, int w, int h, int x2, int y2);
  void (*draw_arc)   (xitk_be_image_t *image, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
    uint16_t a1, uint16_t a2, uint32_t color, int mask);
  void (*fill_arc)   (xitk_be_image_t *image, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
    uint16_t a1, uint16_t a2, uint32_t color, int mask);
  void (*fill_polygon) (xitk_be_image_t *image, const xitk_point_t *points, int num_points, uint32_t color, int mask);
  void (*draw_text)  (xitk_be_image_t *image, const char *text, size_t bytes, int x, int y);
};

typedef enum {
  XITK_WS_NONE = 0,
  XITK_WS_HIDDEN,
  XITK_WS_NORMAL,
  XITK_WS_MAXIMIZED,
  XITK_WS_ICONIFIED
} xitk_be_window_state_t;

struct xitk_be_window_s {
  xitk_dnode_t node;
  uint32_t magic;
  xitk_be_type_t type;
  xitk_be_display_t *display;
  uintptr_t id;
  /* for appliction use */
  void *data;

  void (*_delete)  (xitk_be_window_t **win);

  int  (*get_props)     (xitk_be_window_t *win, xitk_tagitem_t *taglist);
  int  (*get_clip_utf8) (xitk_be_window_t *win, char **buf, int max_len);

  int  (*set_props)     (xitk_be_window_t *win, const xitk_tagitem_t *taglist);
  void (*raise)         (xitk_be_window_t *win);
  int  (*set_clip_utf8) (xitk_be_window_t *win, const char *buf, int len);
  void (*copy_rect)     (xitk_be_window_t *win, xitk_be_image_t *from, int x1, int y1, int w, int h, int x2, int y2, int sync);
};

struct xitk_be_display_s {
  xitk_dnode_t node;
  uint32_t magic;
  xitk_be_type_t type;
  xitk_backend_t *be;
  uintptr_t id;
  /* for appliction use */
  void *data;

  xitk_dlist_t images, windows;

  void (*close)  (xitk_be_display_t **d);
  void (*lock)   (xitk_be_display_t *d);
  void (*unlock) (xitk_be_display_t *d);

  /* win may be NULL, type may be XITK_EV_ANY.
   * the pointers in event stay valid until next next_event () or close (). */
  int (*next_event) (xitk_be_display_t *d, xitk_be_event_t *event, xitk_be_window_t *win, xitk_be_event_type_t type, int timeout);
  const char *(*event_name) (xitk_be_display_t *d, xitk_be_event_t *event);

  struct {
    int  (*_new)    (xitk_be_display_t *d, xitk_color_info_t *info);
    void (*_delete) (xitk_be_display_t *d, uint32_t value);
  } color;

  xitk_be_image_t  *(*image_new)  (xitk_be_display_t *d, const xitk_tagitem_t *taglist);
  xitk_be_window_t *(*window_new) (xitk_be_display_t *d, const xitk_tagitem_t *taglist);
};

xitk_backend_t *xitk_backend_new (xitk_t *xitk, int verbosity);

struct xitk_backend_s {
  xitk_dnode_t node;
  uint32_t magic;
  xitk_be_type_t type;
  xitk_t *xitk;
  int verbosity;

  xitk_dlist_t displays;

  void (*_delete) (xitk_backend_t **be);
  xitk_be_display_t *(*open_display) (xitk_backend_t *be, const char *name, int use_lock, int use_sync);
};
#endif
