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

#ifndef STREAM_INFOS_H
#define STREAM_INFOS_H

#include <stdint.h>
#include <xine.h>

#include "xitk.h"

const char *get_fourcc_string(char *dst, size_t dst_size, uint32_t f);

char *stream_infos_get_ident_from_stream(xine_stream_t *stream);

void stream_infos_panel (gGui_t *gui);
void stream_infos_end (xui_sinfo_t *sinfo);
int stream_infos_is_visible (xui_sinfo_t *sinfo);
void stream_infos_toggle_visibility (xitk_widget_t *w, void *sinfo);
void stream_infos_raise_window (xui_sinfo_t *sinfo);
void stream_infos_update_infos (xui_sinfo_t *sinfo);
void stream_infos_toggle_auto_update (xui_sinfo_t *sinfo);
void stream_infos_reparent (xui_sinfo_t *sinfo);

#endif
