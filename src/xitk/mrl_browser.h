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

#ifndef MRL_BROWSER_H
#define MRL_BROWSER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

typedef struct xui_mrlb_st xui_mrlb_t;

typedef void (*select_cb_t) (xitk_widget_t *w, void *mrlb, int);

void open_mrlbrowser (xitk_widget_t *w, void *gui);
void open_mrlbrowser_from_playlist (xitk_widget_t *w, void *gui);
void destroy_mrl_browser (xui_mrlb_t *mrlb);

void mrl_browser_toggle_visibility (xitk_widget_t *w, void *mrlb);

int mrl_browser_is_running (xui_mrlb_t *mrlb);
int mrl_browser_is_visible (xui_mrlb_t *mrlb);

void mrl_browser_change_skins (xui_mrlb_t *mrlb, int);
void hide_mrl_browser (xui_mrlb_t *mrlb);
void show_mrl_browser (xui_mrlb_t *mrlb);
void set_mrl_browser_transient (xui_mrlb_t *mrlb);
void mrl_browser_show_tips (xui_mrlb_t *mrlb, int enabled, unsigned long timeout);
void mrl_browser_update_tips_timeout (xui_mrlb_t *mrlb, unsigned long timeout);
void mrl_browser_reparent (xui_mrlb_t *mrlb);

#endif
