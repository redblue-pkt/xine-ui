/* 
 * Copyright (C) 2000-2003 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */
#ifndef MENUS_H
#define MENUS_H

#include "xitk.h"

void video_window_menu(xitk_widget_list_t *wl);
void audio_lang_menu(xitk_widget_list_t *wl, int x, int y);
void spu_lang_menu(xitk_widget_list_t *wl, int x, int y);
void playlist_menu(xitk_widget_list_t *wl, int x, int y, int selected);

#endif
