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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

void playlist_mmk_editor(void);
void playlist_scan_for_infos(void);
void playlist_show_tips(int enabled, unsigned long timeout);
void playlist_mrlident_toggle(void);
void playlist_update_playlist(void);
void playlist_exit(xitk_widget_t *, void *);
int playlist_is_running(void);
int playlist_is_visible(void);
void playlist_scan_input(xitk_widget_t *, void *);
void playlist_raise_window(void);
void playlist_toggle_visibility(xitk_widget_t *, void *);
void playlist_update_focused_entry(void);
void playlist_change_skins(void);
void playlist_delete_all(xitk_widget_t *w, void *data);
void playlist_editor(void);

#endif
