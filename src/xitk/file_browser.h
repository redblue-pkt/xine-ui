/* 
 * Copyright (C) 2000-2001 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */

#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#ifdef NEED_FILEBROWSER

typedef void (*select_cb_t) (widget_t *, void *);
typedef void (*add_cb_t) (widget_t *widget, void *data, const char *);

void file_browser(add_cb_t add_cb, select_cb_t sel_cb, dnd_callback_t dnd_cb);
void destroy_file_browser(void);
int file_browser_is_running(void);
int file_browser_is_visible(void);
void file_browser_toggle_visibility(void);
void hide_file_browser(void);
void show_file_browser(void);
void set_file_browser_transient(void);

#endif

#endif
