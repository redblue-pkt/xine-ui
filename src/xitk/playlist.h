/* 
 * Copyright (C) 2000 the xine project
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

#ifndef PLAYLIST_H
#define PLAYLIST_H

void playlist_editor(void);
void playlist_handle_event(XEvent *);
void pl_exit(widget_t *, void *);
int pl_is_running(void);
void pl_toggle_visibility(widget_t *, void *);
void pl_raise_window(void);
void pl_scan_input(widget_t *, void *);
void pl_update_playlist(void);

#endif
