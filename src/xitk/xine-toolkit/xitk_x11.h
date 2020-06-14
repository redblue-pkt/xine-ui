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

void xitk_x11_find_visual(Display *display, int screen, VisualID prefered_visual_id, int prefered_visual_class,
                          Visual **visual_out, int *depth_out);

#endif /* _XITK_X11_H_ */
