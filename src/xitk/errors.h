/*
 * Copyright (C) 2000-2019 the xine project
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

void xine_error (gGui_t *gui, const char *message, ...) __attribute__ ((format (printf, 2, 3)));

void xine_error_with_more (gGui_t *gui, const char *message, ...) __attribute__ ((format (printf, 2, 3)));

void xine_info (gGui_t *gui, const char *message, ...) __attribute__ ((format (printf, 2, 3)));

void gui_handle_xine_error (gGui_t *gui, xine_stream_t *stream, const char *mrl);

void too_slow_window (gGui_t *gui);

#endif

