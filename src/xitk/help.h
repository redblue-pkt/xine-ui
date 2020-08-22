/* 
 * Copyright (C) 2000-2009 the xine project
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

#ifndef HELP_H
#define HELP_H

typedef struct xui_help_s xui_help_t;

void help_panel (gGui_t *gui);
void help_end (xui_help_t *help);
int help_is_visible (xui_help_t *help);
int help_is_running (xui_help_t *help);
void help_toggle_visibility (xitk_widget_t *w, void *help);
void help_raise_window (xui_help_t *help);
void help_reparent (xui_help_t *help);

#endif
