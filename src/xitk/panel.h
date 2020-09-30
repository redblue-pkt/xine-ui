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
 * xine panel related stuff
 *
 */

#ifndef PANEL_H
#define PANEL_H

#include "xitk.h"

void panel_update_nextprev_tips (xui_panel_t *panel);
int is_playback_widgets_enabled (xui_panel_t *panel);
void enable_playback_controls (xui_panel_t *panel, int enable);

void panel_show_tips (xui_panel_t *panel);
int panel_get_tips_enable (xui_panel_t *panel);
unsigned long panel_get_tips_timeout (xui_panel_t *panel);
void panel_raise_window(xui_panel_t *panel);
void panel_get_window_position(xui_panel_t *panel, int *px, int *py, int *pw, int *ph);

void panel_deinit (xui_panel_t *panel);
xui_panel_t *panel_init (gGui_t *gui);

void panel_paint (xui_panel_t *panel);

/* NOTE: return 0 (unmapped), 1 (iconified), 2 (visible) */
int panel_is_visible (xui_panel_t *panel);

void panel_change_skins (xui_panel_t *panel, int synthetic);

void panel_add_autoplay_buttons (xui_panel_t *panel);
void panel_add_mixer_control (xui_panel_t *panel);

/* the next 3 are usable both directly and as widget callbacks.
   w may be NULL, data is xui_panel_t *. */
void panel_toggle_visibility (xitk_widget_t *w, void *data);
void panel_toggle_audio_mute(xitk_widget_t *w, void *data, int status);
void panel_snapshot(xitk_widget_t *w, void *data);

void panel_check_pause (xui_panel_t *panel);
void panel_check_mute (xui_panel_t *panel);

void panel_reset_runtime_label (xui_panel_t *panel);
void panel_reset_slider (xui_panel_t *panel);

void panel_update_slider (xui_panel_t *panel, int pos);
void panel_update_channel_display (xui_panel_t *panel);
void panel_update_runtime_display (xui_panel_t *panel);
void panel_update_mrl_display (xui_panel_t *panel);
void panel_update_mixer_display (xui_panel_t *panel);

void panel_set_title (xui_panel_t *panel, char*);

void panel_reparent (xui_panel_t *panel);

#endif
