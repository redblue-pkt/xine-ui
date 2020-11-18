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

#ifndef _HAVE_XITK_DND_H
#define _HAVE_XITK_DND_H

#include "_xitk.h"

#include <X11/Xlib.h>

/*
 * *** DND
 */

typedef struct xitk_dnd_s xitk_dnd_t;

xitk_dnd_t *xitk_dnd_new (Display *display, int lock, int verbosity);
void xitk_dnd_delete (xitk_dnd_t *xdnd);
int xitk_dnd_make_window_aware (xitk_dnd_t *dnd, Window w);
/** return 0 (not handled), 1 (handled), 2 (got string). */
int xitk_dnd_client_message (xitk_dnd_t *xdnd, XEvent *event, char *buf, size_t bsize);

#endif
