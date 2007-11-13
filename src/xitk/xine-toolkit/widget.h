/* 
 * Copyright (C) 2000-2004 the xine project
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


#define WIDGET_EVENT_PAINT           1
#define WIDGET_EVENT_CLICK           2
#define WIDGET_EVENT_FOCUS           3
#define WIDGET_EVENT_KEY_EVENT       4
#define WIDGET_EVENT_INSIDE          5
#define WIDGET_EVENT_CHANGE_SKIN     6
#define WIDGET_EVENT_ENABLE          7
#define WIDGET_EVENT_GET_SKIN        8
#define WIDGET_EVENT_DESTROY         9
#define WIDGET_EVENT_TIPS_TIMEOUT   10

typedef struct {
  int                   type; /* See WIDGET_EVENT_x */
  int                   x;
  int                   y;
  
  int                   button_pressed;
  int                   button;
  
  int                   focus;
  
  unsigned long         tips_timeout;

  XEvent               *xevent;

  xitk_skin_config_t   *skonfig;
  int                   skin_layer;
} widget_event_t;

typedef struct {
  int                   value;
  xitk_image_t         *image;
} widget_event_result_t;

/* return 1 if event_result is filled, otherwise 0 */
typedef int (*widget_event_notify_t)(xitk_widget_t *, widget_event_t *, widget_event_result_t *);

struct xitk_widget_s {
  ImlibData                      *imlibdata;

  xitk_widget_list_t             *wl;

  int                             x;
  int                             y;
  int                             width;
  int                             height;

  int                             enable;
  int                             have_focus;
  int                             running;
  int                             visible;

  widget_event_notify_t           event;

  unsigned long                   tips_timeout;
  char                           *tips_string;

  void                           *private_data;
  uint32_t                        type;
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
 *
 */
void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward);

#endif

