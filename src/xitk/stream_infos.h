/* 
 * Copyright (C) 2000-2002 the xine project
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

#ifndef STREAM_INFOS_H
#define STREAM_INFOS_H

void stream_infos_panel(void);
void stream_infos_exit(xitk_widget_t *, void *);
int stream_infos_is_visible(void);
int stream_infos_is_running(void);
void stream_infos_toggle_visibility(xitk_widget_t *, void *);
void stream_infos_raise_window(void);
void stream_infos_update_infos(void);

#endif
