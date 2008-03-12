/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */

#ifndef SKINS_INTERNAL_H
#define SKINS_INTERNAL_H

#define DEFAULT_SKIN        "xinetic"
#define SKIN_IFACE_VERSION  5

#undef SKIN_DEBUG

extern skins_locations_t **skins_avail;
extern int                 skins_avail_num;
extern char               **skin_names;

int get_skin_offset(const char *skin);
void skin_change_cb(void *data, xine_cfg_entry_t *cfg);

#endif
