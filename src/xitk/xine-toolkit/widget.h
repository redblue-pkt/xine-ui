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

#ifndef HAVE_XITK_WIDGET_H
#define HAVE_XITK_WIDGET_H

#include "dlist.h"

#include <X11/Xlib.h>

#include <xine/sorted_array.h>

#include "xitk/Imlib-light/Imlib.h"

typedef enum {
  FOCUS_LOST = 0,
  FOCUS_RECEIVED,
  FOCUS_MOUSE_IN,
  FOCUS_MOUSE_OUT
} widget_focus_t;

#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1


#define FOREGROUND_SKIN 1
#define BACKGROUND_SKIN 2

#define WIDGET_EVENT_PAINT           1
#define WIDGET_EVENT_CLICK           2
#define WIDGET_EVENT_FOCUS           3
#define WIDGET_EVENT_KEY             4
#define WIDGET_EVENT_INSIDE          5
#define WIDGET_EVENT_CHANGE_SKIN     6
#define WIDGET_EVENT_ENABLE          7
#define WIDGET_EVENT_GET_SKIN        8
#define WIDGET_EVENT_DESTROY         9
#define WIDGET_EVENT_TIPS_TIMEOUT   10
#define WIDGET_EVENT_CLIP_READY     11
#define WIDGET_EVENT_PARTIAL_PAINT  12

typedef struct {
  int                   type; /* See WIDGET_EVENT_x */
  int                   x, y, width, height;

  int                   button_pressed;
  int                   button;
  int                   modifier;   /* modifier key state (EVENT_CLICK, EVENT_KEY_EVENT) */
  widget_focus_t        focus;

  unsigned long         tips_timeout;

  const char           *string;

  xitk_skin_config_t   *skonfig;
  int                   skin_layer;
} widget_event_t;

typedef struct {
  int                   value;
  xitk_image_t         *image;
} widget_event_result_t;

typedef struct xitk_widget_rel_s {
  xitk_dnode_t node;
  xitk_dlist_t list;
  struct xitk_widget_rel_s *group;
  xitk_widget_t *w;
} xitk_widget_rel_t;

/* return 1 if event_result is filled, otherwise 0 */
typedef int (*widget_event_notify_t)(xitk_widget_t *, widget_event_t *, widget_event_result_t *);

struct xitk_widget_s {
  xitk_dnode_t           node;

  xitk_widget_list_t    *wl;
  xitk_widget_rel_t      parent, focus_redirect;

  int                    x;
  int                    y;
  int                    width;
  int                    height;

  uint32_t               type;
  int                    enable;
  int                    running;
  int                    visible;
  widget_focus_t         have_focus;

  widget_event_notify_t  event;

  unsigned long          tips_timeout;
  char                  *tips_string;

  void                  *private_data;

  struct {
    int                  enable;
    int                  visible;
    int                  running;
  }                      state;
};

struct xitk_widget_list_s {
  xitk_dnode_t                node;

  xitk_t                     *xitk;
  ImlibData                  *imlibdata;

  xitk_dlist_t                list;

  xitk_widget_t              *widget_focused;
  xitk_widget_t              *widget_under_mouse;
  xitk_widget_t              *widget_pressed;

  xine_sarray_t              *shared_images;

  xitk_window_t              *xwin;
  Window                      win;
};

xitk_widget_t *xitk_widget_new (xitk_widget_list_t *wl, size_t size);
void xitk_widget_set_parent (xitk_widget_t *w, xitk_widget_t *parent);

/* ****************************************************************** */

#if !defined(__GNUC__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define	__FUNCTION__	__func__
#endif


/*
 * small utility function to debug xlock races
 */

/*  #define DEBUG_XLOCK */
#ifdef DEBUG_XLOCK

#if 1

static int displ;
#define XLOCK(FUNC,DISP) do {                                                 \
    int i;                                                                    \
    displ++;                                                                  \
    for(i = 0; i < displ; i++) printf("%d",i);                                \
    printf(">%s: %s(%d) XLock\n",                                             \
           __FILE__, __FUNCTION__, __LINE__);                                 \
    fflush(stdout);                                                           \
    (FUNC)(DISP);                                                             \
    printf(" %s: %s(%d) got the lock\n",                                      \
           __FILE__, __FUNCTION__, __LINE__);                                 \
  } while (0)

#define XUNLOCK(FUNC,DISP) do {                                               \
    int i;                                                                    \
    for(i = 0; i < displ; i++) printf("%d",i);                                \
    displ--;                                                                  \
    printf("<%s: %s(%d) XUnlockDisplay\n",                                    \
           __FILE__, __FUNCTION__, __LINE__);                                 \
    fflush(stdout);                                                           \
    (FUNC)(DISP);                                                             \
  } while (0)

#else

#define XLOCK(FUNC,DISP) do {                                                 \
    printf("%s: %s(%d) XLockDisplay (%d)\n",                                  \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
    fflush(stdout);                                                           \
    (FUNC)(DISP);                                                             \
    printf("%s: %s(%d) got the lock (%d)\n",                                  \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
  } while (0)

#define XUNLOCK(FUNC,DISP) do {                                               \
    printf("%s: %s(%d) XUnlockDisplay (%d)\n",                                \
           __FILE__, __FUNCTION__, __LINE__, DISP);                           \
    fflush(stdout);                                                           \
    (FUNC)(DISP);                                                             \
  } while (0)

#endif

#else

#define XLOCK(FUNC,DISP) (FUNC)(DISP)
#define XUNLOCK(FUNC,DISP) (FUNC)(DISP)

#endif

/**
 *
 */
void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward, int modifier);
void xitk_set_focus_to_wl (xitk_widget_list_t *wl);

#endif
