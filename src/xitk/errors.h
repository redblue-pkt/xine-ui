/*
 * Copyright (C) 2000-2021 the xine project
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

#define XUI_MSG_INFO  0 /** << just for user info */
#define XUI_MSG_WARN  1 /** << warning */
#define XUI_MSG_ERROR 2 /** << fail */
#define XUI_MSG_MORE  8 /** << add "more..." button */
void gui_msg (gGui_t *gui, unsigned int flags, const char *message, ...) __attribute__ ((format (printf, 3, 4)));

void gui_handle_xine_error (gGui_t *gui, xine_stream_t *stream, const char *mrl);

void too_slow_window (gGui_t *gui);

#endif

