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

#ifndef CONTROL_H
#define CONTROL_H

typedef struct xui_vctrl_st xui_vctrl_t;

xui_vctrl_t *control_init (gGui_t *gui);
void control_deinit (xui_vctrl_t *vctrl);

/* supports XUI_W_* */
void control_toggle_window (xitk_widget_t *w, void *vctrl);

/* off (0), keyboard only (1), hidden window (2), visible window (3= */
int control_status (xui_vctrl_t *vctrl);

void control_reset (xui_vctrl_t *vctrl);

void control_toggle_visibility (xui_vctrl_t *vctrl);
void control_change_skins (xui_vctrl_t *vctrl, int);
void control_raise_window (xui_vctrl_t *vctrl);
void control_show_tips (xui_vctrl_t *vctrl, int enabled, unsigned long timeout);
//void control_update_tips_timeout (xui_vctrl_t *vctrl, unsigned long timeout);
void control_inc_image_prop (xui_vctrl_t *vctrl, int prop);
void control_dec_image_prop (xui_vctrl_t *vctrl, int prop);
void control_reparent (xui_vctrl_t *vctrl);

#endif
