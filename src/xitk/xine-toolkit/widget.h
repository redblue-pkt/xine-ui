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

#ifndef HAVE_WIDGET_H
#define HAVE_WIDGET_H

#include <inttypes.h> 
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "list.h"
#include "widget_types.h"

#define FOCUS_RECEIVED 1
#define FOCUS_LOST     0
#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1

#define BROWSER_MAX_ENTRIES 65535

typedef int widgetkey_t;

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct {
  uint32_t                    flags;
  uint32_t                    functions;
  uint32_t                    decorations;
  int32_t                     input_mode;
  uint32_t                    status;
} MWMHints;

typedef struct {
  int                         red;
  int                         green;
  int                         blue;
  char                       *colorname;
} gui_color_names_t;

typedef struct {
  Pixmap                      image;
  int                         width;
  int                         height;
} gui_image_t;

typedef struct {
  XColor                      red;
  XColor                      blue;
  XColor                      green;
  XColor                      white;
  XColor                      black;
  XColor                      tmp;
} gui_color_t;

typedef struct {
  int                         enabled;
  int                         offset_x;
  int                         offset_y;
} gui_move_t;

struct widget_list_s;

struct widget_s;

typedef void (*widget_paint_callback_t)(struct widget_s *, Window, GC);

typedef int (*widget_click_callback_t) (struct widget_list_s *, struct widget_s *, int, int, int);

typedef int (*widget_focus_callback_t)(struct widget_list_s *, struct widget_s *, int);

typedef void (*widget_keyevent_callback_t)(struct widget_list_s *, struct widget_s *, XEvent *);

typedef struct widget_s {
  int                        x;
  int                        y;
  int                        width;
  int                        height;

  int                        enable;
  int                        have_focus;

  widget_paint_callback_t    paint;

  /* notify callback return value : 1 => repaint necessary 0=> do nothing */
                                       /*   parameter: up (1) or down (0) */
  widget_click_callback_t    notify_click;
                                       /*            entered (1) left (0) */
  widget_focus_callback_t    notify_focus;

  widget_keyevent_callback_t notify_keyevent;

  void                      *private_data;
  uint32_t                   widget_type;
} widget_t;

typedef struct widget_list_s {

  gui_list_t                 *l;

  widget_t                   *focusedWidget;
  widget_t                   *pressedWidget;

  Window                      win;
  GC                          gc;
} widget_list_t;

/* ****************************************************************** */

/**
 * Allocate an clean memory area of size "size".
 */
void *gui_xmalloc(size_t size);

/**
 * return pointer to the gui_color_names struct.
 */
gui_color_names_t *gui_get_color_names(void);

/**
 * (re)Paint a widget list.
 */
int paint_widget_list (widget_list_t *wl) ;

/**
 * Boolean function, if x and y coords is in widget.
 */
int is_inside_widget (widget_t *widget, int x, int y) ;

/**
 * Return widget from widget list 'wl' localted at x,y coords.
 */
widget_t *get_widget_at (widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void motion_notify_widget_list (widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int click_notify_widget_list (widget_list_t *wl, int x, int y, int bUp) ;

/**
 *
 */
void widget_send_key_event(widget_list_t *, widget_t *, XEvent *);

/**
 * Return width (in pixel) of widget.
 */
int widget_get_width(widget_t *);

/**
 * Return height (in pixel) of widget.
 */
int widget_get_height(widget_t *);

/*
 * Boolean, return 1 if widget 'w' have focus.
 */
int widget_have_focus(widget_t *);

/**
 * Boolean, enable state of widget.
 */
int widget_enabled(widget_t *);

/**
 * Enable widget.
 */
void widget_enable(widget_t *);

/**
 * Disable widget.
 */
void widget_disable(widget_t *);

/*
 * small utility function to debug xlock races
 */

/* #define DEBUG_XLOCK */
#ifdef DEBUG_XLOCK

#define XLOCK(DISP) {                                                         \
    printf("%s: %s(%d) XLockDisplay (%d)\n",                                  \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
    fflush(stdout);                                                           \
    XLockDisplay(DISP);                                                       \
    printf("%s: %s(%d) got the lock (%d)\n",                                  \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
  }

#define XUNLOCK(DISP) {                                                       \
    printf("%s: %s(%d) XUnlockDisplay (%d)\n",                                \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
    fflush(stdout);                                                           \
    XUnlockDisplay(DISP);                                                     \
  }

#else

#define XLOCK(DISP) {                                                         \
    XLockDisplay(DISP);                                                       \
  }

#define XUNLOCK(DISP) {                                                       \
    XUnlockDisplay(DISP);                                                     \
  }

#endif

#endif

