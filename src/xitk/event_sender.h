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

#ifndef EVENT_SENDER_H
#define EVENT_SENDER_H

void event_sender_sticky_cb (void *gui, xine_cfg_entry_t *cfg);
void event_sender_panel (gGui_t *gui);
void event_sender_end (gGui_t *gui);
void event_sender_move (gGui_t *gui, int x, int y);
int event_sender_is_visible (gGui_t *gui);
void event_sender_toggle_visibility (gGui_t *gui);
void event_sender_update_menu_buttons (gGui_t *gui);
void event_sender_send (gGui_t *gui, int event);
void event_sender_reparent (gGui_t *gui);
void event_sender_show_tips (gGui_t *gui, unsigned long timeout);
//void event_sender_update_tips_timeout (gGui_t *gui, unsigned long timeout);

#endif
