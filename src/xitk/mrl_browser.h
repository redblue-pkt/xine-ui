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

#ifndef MRL_BROWSER_H
#define MRL_BROWSER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

typedef void (*select_cb_t) (xitk_widget_t *, void *, int);

void mrl_browser(xitk_mrl_callback_t add_cb, xitk_mrl_callback_t add_and_play_cb,
		 select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb);
void mrl_browser_change_skins(void);
void open_mrlbrowser(xitk_widget_t *w, void *data);
void open_mrlbrowser_from_playlist(xitk_widget_t *w, void *data);
void destroy_mrl_browser(void);
int mrl_browser_is_running(void);
int mrl_browser_is_visible(void);
void mrl_browser_toggle_visibility(xitk_widget_t *w, void *data);
void hide_mrl_browser(void);
void show_mrl_browser(void);
void set_mrl_browser_transient(void);
void mrl_browser_show_tips(int enabled, unsigned long timeout);

#endif
