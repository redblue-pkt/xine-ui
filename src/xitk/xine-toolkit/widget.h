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

#define FOCUS_LOST      0
#define FOCUS_RECEIVED  1
#define FOCUS_MOUSE_IN  3
#define FOCUS_MOUSE_OUT 4

#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1


#define FOREGROUND_SKIN 1
#define BACKGROUND_SKIN 2


typedef struct {
  XColor                      red;
  XColor                      blue;
  XColor                      green;
  XColor                      white;
  XColor                      black;
  XColor                      tmp;
} xitk_color_t;

typedef void (*widget_paint_callback_t)(xitk_widget_t *, Window, GC);

typedef int (*widget_click_callback_t) (xitk_widget_list_t *, struct xitk_widget_s *, int, int, int);

typedef int (*widget_focus_callback_t)(xitk_widget_list_t *, xitk_widget_t *, int);

typedef void (*widget_keyevent_callback_t)(xitk_widget_list_t *, xitk_widget_t *, XEvent *);

typedef int (*widget_inside_callback_t)(xitk_widget_t *, int, int);

typedef void (*widget_change_skin_callback_t)(xitk_widget_list_t *, xitk_widget_t *, xitk_skin_config_t *);

typedef void (*widget_enable_callback_t)(xitk_widget_t *);

typedef xitk_image_t *(*widget_get_skin_t)(xitk_widget_t *, int);

typedef void (*widget_destroy_t)(xitk_widget_t *, void *);

struct xitk_widget_s {
  ImlibData                      *imlibdata;

  xitk_widget_list_t             *widget_list;

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

  widget_enable_callback_t        notify_enable;

  pthread_t                       tips_thread;
  int                             tips_timeout;
  char                           *tips_string;

  void                           *private_data;
  uint32_t                        widget_type;
};

struct xitk_widget_list_s {

  xitk_list_t                *l;

  xitk_widget_t              *widget_focused;
  xitk_widget_t              *widget_under_mouse;
  xitk_widget_t              *widget_pressed;

  Window                      win;
  GC                          gc;
};

/* ****************************************************************** */

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
void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward);

#endif

