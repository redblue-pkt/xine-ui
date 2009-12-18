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
 */

#ifndef ERRORS_H
#define ERRORS_H

#include <stdarg.h>

void xine_error(char *message, ...) __attribute__ ((format (printf, 1, 2)));

void xine_error_with_more(char *message, ...) __attribute__ ((format (printf, 1, 2)));

void xine_info(char *message, ...) __attribute__ ((format (printf, 1, 2)));

void gui_handle_xine_error(xine_stream_t *stream, char *mrl);

void too_slow_window(void);

#endif

