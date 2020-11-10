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
#ifndef MENUS_H
#define MENUS_H

#include "xitk.h"

void video_window_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y);
void audio_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y);
void spu_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y);
void playlist_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y, int selected);
void control_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y);

#endif
