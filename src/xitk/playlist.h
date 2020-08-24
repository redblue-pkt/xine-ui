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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "xitk.h"

typedef struct xui_playlist_st xui_playlist_t;

void playlist_editor (gGui_t *gui);
void playlist_change_skins (gGui_t *gui, int synth);
void playlist_reparent (gGui_t *gui);
void playlist_get_input_focus (gGui_t *gui);
void playlist_raise_window (gGui_t *gui);
int playlist_is_visible (gGui_t *gui);
void playlist_toggle_visibility (gGui_t *gui);
void playlist_play_current (gGui_t *gui);
void playlist_delete_current (gGui_t *gui);
void playlist_delete_all (gGui_t *gui);
void playlist_move_current_up (gGui_t *gui);
void playlist_move_current_down (gGui_t *gui);
void playlist_load_playlist (gGui_t *gui);
void playlist_save_playlist (gGui_t *gui);
void playlist_exit (gGui_t *gui);
void playlist_deinit (gGui_t *gui);

void playlist_update_playlist (gGui_t *gui);
void playlist_update_focused_entry (gGui_t *gui);
void playlist_delete_entry (gGui_t *gui, int j);

void playlist_mmk_editor (gGui_t *gui);
void playlist_scan_for_infos_selected (gGui_t *gui);
void playlist_scan_for_infos (gGui_t *gui);
void playlist_show_tips (gGui_t *gui, int enabled, unsigned long timeout);
//void playlist_update_tips_timeout (gGui_t *gui, unsigned long timeout);
void playlist_mrlident_toggle (gGui_t *gui);

void playlist_scan_input (xitk_widget_t *w, void *gui, int state);
#endif
