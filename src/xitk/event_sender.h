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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */

#ifndef EVENT_SENDER_H
#define EVENT_SENDER_H

void event_sender_sticky_cb(void *data, xine_cfg_entry_t *cfg);
void event_sender_panel(void);
void event_sender_end(void);
void event_sender_move(int x, int y);
int event_sender_is_visible(void);
int event_sender_is_running(void);
void event_sender_toggle_visibility(xitk_widget_t *, void *);
void event_sender_raise_window(void);
void event_sender_update_menu_buttons(void);
void event_sender_send(int event);
void event_sender_reparent(void);

#endif
