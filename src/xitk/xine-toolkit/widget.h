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

#ifndef HAVE_XITK_WIDGET_H
#define HAVE_XITK_WIDGET_H

#include <inttypes.h> 
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "Imlib-light/Imlib.h"

#include "list.h"
#include "skin.h"
#include "widget_types.h"

#define FOCUS_RECEIVED 1
#define FOCUS_LOST     0
#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1


#define FOREGROUND_SKIN 1
#define BACKGROUND_SKIN 2


#define BROWSER_MAX_ENTRIES 65535

typedef int xitk_register_key_t;

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
} xitk_color_names_t;

typedef struct xitk_image_s {
  Pixmap                      image;
  Pixmap                      mask;
  int                         width;
  int                         height;
} xitk_image_t;

typedef struct {
  XColor                      red;
  XColor                      blue;
  XColor                      green;
  XColor                      white;
  XColor                      black;
  XColor                      tmp;
} xitk_color_t;

typedef struct {
  int                         enabled;
  int                         offset_x;
  int                         offset_y;
} xitk_move_t;

typedef struct {
  struct {
    int                       x;
    int                       y;
  } pos;
  xitk_image_t               *image;
} xitk_image_and_pos_t;

struct xitk_widget_list_s;

struct xitk_widget_s;

typedef void (*widget_paint_callback_t)(struct xitk_widget_s *, Window, GC);

typedef int (*widget_click_callback_t) (struct xitk_widget_list_s *, struct xitk_widget_s *, int, int, int);

typedef int (*widget_focus_callback_t)(struct xitk_widget_list_s *, struct xitk_widget_s *, int);

typedef void (*widget_keyevent_callback_t)(struct xitk_widget_list_s *, struct xitk_widget_s *, XEvent *);

typedef int (*widget_inside_callback_t)(struct xitk_widget_s *, int, int);

typedef void (*widget_change_skin_callback_t)(struct xitk_widget_list_s *, struct xitk_widget_s *, xitk_skin_config_t *);

typedef xitk_image_t *(*widget_get_skin_t)(struct xitk_widget_s *, int);

typedef void (*widget_destroy_t)(struct xitk_widget_s *, void *);

typedef struct xitk_widget_s {
  ImlibData                      *imlibdata;

  int                             x;
  int                             y;
  int                             width;
  int                             height;

  int                             enable;
  int                             have_focus;
  int                             running;
  int                             visible;

  widget_paint_callback_t         paint;

  /* notify callback return value : 1 => repaint necessary 0=> do nothing */
                                       /*   parameter: up (1) or down (0) */
  widget_click_callback_t         notify_click;
                                       /*            entered (1) left (0) */
  widget_focus_callback_t         notify_focus;

  widget_keyevent_callback_t      notify_keyevent;

  widget_inside_callback_t        notify_inside;

  widget_change_skin_callback_t   notify_change_skin;

  widget_destroy_t                notify_destroy;

  widget_get_skin_t               get_skin;

  pthread_t                       tips_thread;
  int                             tips_timeout;
  char                           *tips_string;

  void                           *private_data;
  uint32_t                        widget_type;
} xitk_widget_t;

typedef struct xitk_widget_list_s {

  xitk_list_t                *l;

  xitk_widget_t              *focusedWidget;
  xitk_widget_t              *pressedWidget;

  Window                      win;
  GC                          gc;
} xitk_widget_list_t;

/* ****************************************************************** */

/**
 * Allocate an clean memory area of size "size".
 */
void *xitk_xmalloc(size_t size);

/**
 * return pointer to the xitk_color_names struct.
 */
xitk_color_names_t *xitk_get_color_names(void);

/**
 *
 */
xitk_color_names_t *xitk_get_color_name(char *color);

/**
 * Free color object.
 */
void xitk_free_color_name(xitk_color_names_t *color);

/**
 * (re)Paint a widget list.
 */
int xitk_paint_widget_list (xitk_widget_list_t *wl);

/*
 *
 */
void xitk_change_skins_widget_list(xitk_widget_list_t *wl, xitk_skin_config_t *skonfig);

/**
 * Check if cursor is in a mask pixmap or not. Return 1 if the cursor
 * is in visible area (or mask don't exist), and 0 if the pointed
 * area is not visible.
 */
int xitk_is_cursor_out_mask(Display *display, xitk_widget_t *w, Pixmap mask, int x, int y);

/**
 * Boolean function, if x and y coords is in widget.
 */
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y);

/**
 * Return widget from widget list 'wl' localted at x,y coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int bUp) ;

/**
 *
 */
void xitk_send_key_event(xitk_widget_list_t *, xitk_widget_t *, XEvent *);

/**
 * Return the focused widget.
 */
xitk_widget_t *xitk_get_focused_widget(xitk_widget_list_t *);

/**
 * Return the pressed widget.
 */
xitk_widget_t *xitk_get_pressed_widget(xitk_widget_list_t *);

/**
 * Return width (in pixel) of widget.
 */
int xitk_get_widget_width(xitk_widget_t *);

/**
 * Return height (in pixel) of widget.
 */
int xitk_get_widget_height(xitk_widget_t *);

/*
 * Set position of a widget.
 */
int xitk_set_widget_pos(xitk_widget_t *w, int x, int y);

/*
 * Get position of a widget.
 */
int xitk_get_widget_pos(xitk_widget_t *w, int *x, int *y);

/*
 * Boolean, return 1 if widget 'w' have focus.
 */
int xitk_is_widget_focused(xitk_widget_t *);

/**
 * Boolean, enable state of widget.
 */
int xitk_is_widget_enabled(xitk_widget_t *);

/**
 * Enable widget.
 */
void xitk_enable_widget(xitk_widget_t *);

/**
 * Disable widget.
 */
void xitk_disable_widget(xitk_widget_t *);

/**
 *
 */
void xitk_free_widget(xitk_widget_t *w);

/**
 * Destroy and free widget.
 */
void xitk_destroy_widget(xitk_widget_list_t *wl, xitk_widget_t *w);

/**
 * Destroy widgets from widget list.
 */
void xitk_destroy_widgets(xitk_widget_list_t *wl);

/**
 * Stop widget.
 */
void xitk_stop_widget(xitk_widget_t *w);

/**
 * Start widget.
 */
void xitk_start_widget(xitk_widget_t *w);

/**
 * Stop each (if widget handle it) widgets of widget list.
 */
void xitk_stop_widgets(xitk_widget_list_t *);

/**
 * Show a widget.
 */
void xitk_show_widget(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Set widgets of widget list visible.
 */
void xitk_show_widgets(xitk_widget_list_t *);

/**
 * Hide a widget.
 */
void xitk_hide_widget(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Set widgets of widget list not visible.
 */
void xitk_hide_widgets(xitk_widget_list_t *);

/**
 *
 */
xitk_image_t *xitk_get_widget_foreground_skin(xitk_widget_t *w);

/**
 *
 */
xitk_image_t *xitk_get_widget_background_skin(xitk_widget_t *w);

/**
 *
 */
void xitk_set_widget_tips(xitk_widget_t *w, char *str);

/**
 *
 */
void xitk_set_widget_tips_default(xitk_widget_t *w, char *str);

/**
 *
 */
void xitk_set_widget_tips_and_timeout(xitk_widget_t *w, char *str, unsigned int timeout);

/**
 *
 */
void xitk_set_widgets_tips_timeout(xitk_widget_list_t *wl, unsigned long timeout);

/**
 *
 */
void xitk_enable_widget_tips(xitk_widget_t *w);

/**
 *
 */
void xitk_disable_widget_tips(xitk_widget_t *w);

/**
 *
 */
void xitk_disable_widgets_tips(xitk_widget_list_t *wl);

/**
 *
 */
void xitk_enable_widgets_tips(xitk_widget_list_t *wl);

/**
 *
 */
void xitk_set_widget_tips_timeout(xitk_widget_t *w, unsigned long timeout);

#ifndef __GNUC__
#define	__FUNCTION__	__func__
#endif


/*
 * small utility function to debug xlock races
 */

/*  #define DEBUG_XLOCK */
#ifdef DEBUG_XLOCK

#if 1

static int displ;
#define XLOCK(DISP) {                                                         \
    int i;                                                                    \
    displ++;                                                                  \
    for(i = 0; i < displ; i++) printf("%d",i);                                \
    printf(">%s: %s(%d) XLock\n",                                             \
           __FILE__, __FUNCTION__, __LINE__);                                 \
    fflush(stdout);                                                           \
    XLockDisplay(DISP);                                                       \
    printf(" %s: %s(%d) got the lock\n",                                      \
           __FILE__, __FUNCTION__, __LINE__);                                 \
  }

#define XUNLOCK(DISP) {                                                       \
    int i;                                                                    \
    for(i = 0; i < displ; i++) printf("%d",i);                                \
    displ--;                                                                  \
    printf("<%s: %s(%d) XUnlockDisplay\n",                                    \
           __FILE__, __FUNCTION__, __LINE__);                                 \
    fflush(stdout);                                                           \
    XUnlockDisplay(DISP);                                                     \
  }

#else

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

#endif

#else

#define XLOCK(DISP) {                                                         \
    XLockDisplay(DISP);                                                       \
  }

#define XUNLOCK(DISP) {                                                       \
    XUnlockDisplay(DISP);                                                     \
  }

#endif

#endif

