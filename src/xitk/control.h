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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */

#ifndef CONTROL_H
#define CONTROL_H

void control_config_register(void);

void control_reset(void);
void control_deinit(void);
void control_panel(void);
void control_change_skins(int);
void control_exit(xitk_widget_t *, void *);
int control_is_visible(void);
int control_is_running(void);
void control_toggle_visibility(xitk_widget_t *, void *);
void control_raise_window(void);
void control_show_tips(int enabled, unsigned long timeout);
void control_update_tips_timeout(unsigned long timeout);
void control_set_image_prop(int prop, int value);
void control_reparent(void);

#endif
