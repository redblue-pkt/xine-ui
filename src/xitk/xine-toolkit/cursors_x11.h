/*
 * Copyright (C) 2004-2021 the xine project
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
 *
 */
#ifndef HAVE_XITK_CURSORS_H
#define HAVE_XITK_CURSORS_H

#include <X11/Xlib.h>

#include "xitk.h"

typedef struct xitk_x11_cursors_s xitk_x11_cursors_t;

xitk_x11_cursors_t *xitk_x11_cursors_init (Display *display, int xitk_cursors, int lock, int verbosity);
void xitk_x11_cursors_deinit(xitk_x11_cursors_t **);

void xitk_x11_cursors_define_window_cursor(xitk_x11_cursors_t *, Window window, xitk_cursors_t cursor);
void xitk_x11_cursors_restore_window_cursor(xitk_x11_cursors_t *, Window window);

#endif
