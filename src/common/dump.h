/* 
 * Copyright (C) 2000-2008 the xine project
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

#ifndef XINEUI_DUMP_H
#define XINEUI_DUMP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

void dump_host_info(void);
#ifdef HAVE_X11
void dump_cpu_infos(void);
void dump_xfree_info(Display *display, int screen, int complete);
#endif

void dump_error(const char *msg);
void dump_info(const char *msg);

#endif
