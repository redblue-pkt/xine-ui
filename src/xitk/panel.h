/* 
 * Copyright (C) 2000-2016 the xine project
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

void panel_update_nextprev_tips(void);
int is_playback_widgets_enabled(void);
void enable_playback_controls(int enable);

void panel_show_tips(void);
int panel_get_tips_enable(void);
unsigned long panel_get_tips_timeout(void);

void panel_deinit(void);
void panel_init (void);

void panel_paint(void);

void panel_change_skins(int);

void panel_add_autoplay_buttons(void);

void panel_add_mixer_control(void);

int panel_is_visible(void);

void panel_toggle_visibility (xitk_widget_t *w, void *data);

void panel_toggle_audio_mute(xitk_widget_t *w, void *data, int status);

void panel_execute_xineshot(xitk_widget_t *w, void *data);

void panel_snapshot(xitk_widget_t *w, void *data);

void panel_check_pause(void);

void panel_check_mute(void);

void panel_reset_runtime_label(void);

void panel_reset_slider (void);

void panel_update_slider(int pos);

void panel_update_channel_display (void);

void panel_update_runtime_display(void);

void panel_update_mrl_display (void);

void panel_update_mixer_display(void);

void panel_set_title(char*);

void panel_reparent(void);

#endif
