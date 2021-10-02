/*
 * Copyright (C) 2000-2021 the xine project
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

#ifndef ACONTROL_H
#define ACONTROL_H

xui_actrl_t *acontrol_init (gGui_t *gui);
void acontrol_deinit (xui_actrl_t *actrl);

/* supports XUI_W_* */
void acontrol_toggle_window (xitk_widget_t *w, void *actrl);

/* off (0), keyboard only (1), hidden window (2), visible window (3= */
/* int control_status (xui_vctrl_t *vctrl); */

void acontrol_reset (xui_actrl_t *actrl);

void acontrol_toggle_visibility (xui_actrl_t *actrl);
void acontrol_change_skins (xui_actrl_t *actrl, int);
void acontrol_raise_window (xui_actrl_t *actrl);

#endif
