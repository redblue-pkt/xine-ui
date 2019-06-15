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

#ifndef CONTROL_H
#define CONTROL_H

typedef struct xui_vctrl_st xui_vctrl_t;

void control_reset (xui_vctrl_t *vctrl);
void control_deinit (xui_vctrl_t *vctrl);
xui_vctrl_t *control_init (gGui_t *gui);

void control_exit (xitk_widget_t *w, void *vctrl);
void control_toggle_visibility (xitk_widget_t *w, void *vctrl);

int control_is_visible (xui_vctrl_t *vctrl);
int control_is_running (xui_vctrl_t *vctrl);

void control_change_skins (xui_vctrl_t *vctrl, int);
void control_raise_window (xui_vctrl_t *vctrl);
void control_show_tips (xui_vctrl_t *vctrl, int enabled, unsigned long timeout);
void control_update_tips_timeout (xui_vctrl_t *vctrl, unsigned long timeout);
void control_inc_image_prop (xui_vctrl_t *vctrl, int prop);
void control_dec_image_prop (xui_vctrl_t *vctrl, int prop);
void control_reparent (xui_vctrl_t *vctrl);

#endif

