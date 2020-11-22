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
 * xitk X11 functions
 *
 */

#ifndef _XITK_X11_H_
#define _XITK_X11_H_

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <X11/Xlib.h>

typedef struct xitk_x11_s xitk_x11_t;

xitk_x11_t *xitk_x11_new (xitk_t *xitk);
void xitk_x11_delete (xitk_x11_t *xitk_x11);

extern void (*xitk_x_lock_display) (Display *display);
extern void (*xitk_x_unlock_display) (Display *display);

/*
 *
 */

#define MWM_HINTS_DECORATIONS       (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS     5
typedef struct _mwmhints {
  unsigned long                     flags;
  unsigned long                     functions;
  unsigned long                     decorations;
  long                              input_mode;
  unsigned long                     status;
} MWMHints;

/*
 *
 */

#define INPUT_MOTION (ExposureMask | ButtonPressMask | ButtonReleaseMask |    \
                      KeyPressMask | KeyReleaseMask | ButtonMotionMask |      \
                      StructureNotifyMask | PropertyChangeMask |              \
                      LeaveWindowMask | EnterWindowMask | PointerMotionMask)


/*
 * X11 helpers
 */

void xitk_x11_find_visual(Display *display, int screen, const char *prefered_visual,
                          Visual **visual_out, int *depth_out);

void xitk_x11_xrm_parse(const char *xrm_class_name,
                        char **geometry, int *borderless,
                        char **prefered_visual, int *install_colormap);

int xitk_x11_parse_geometry(const char *geomstr, int *x, int *y, int *w, int *h);

/*
 *
 */

void xitk_x11_set_ewmh_fullscreen(Display *display, Window window, int enable);

unsigned char *xitk_x11_get_wm_name (Display *display, Window win, Atom atom, Atom type_utf8);
uint32_t xitk_x11_check_wm (Display *display, int verbose);

/*
 *
 */

void xitk_x11_try_to_set_input_focus(Display *display, Window window);
void xitk_x11_get_window_position(Display *display, Window window, int *x, int *y, int *width, int *height);
int  xitk_x11_is_window_iconified(Display *display, Window window);
int  xitk_x11_is_window_visible(Display *display, Window window);
Window xitk_get_desktop_root_window(Display *display, int screen, Window *clientparent);

/*
 * access to xitk X11
 */

#include "xitk.h"

void        xitk_x11_select_visual(xitk_t *, Visual *gui_visual);

Display    *xitk_x11_get_display(xitk_t *);
Visual     *xitk_x11_get_visual(xitk_t *);
int         xitk_x11_get_depth(xitk_t *);
Colormap    xitk_x11_get_colormap(xitk_t *);

xitk_window_t *xitk_x11_wrap_window(xitk_t *, Window window);
void xitk_x11_destroy_window_wrapper(xitk_window_t **);

#endif /* _XITK_X11_H_ */
