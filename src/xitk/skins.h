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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */

#ifndef SKINS_H
#define SKINS_H

typedef struct {
  const char *pathname;
  const char *skin;
  int         number;
} skins_locations_t;

skins_locations_t **get_available_skins(void);
int get_available_skins_num(void);
void preinit_skins_support(void);
void init_skins_support(void);
void select_new_skin(int selected);
void download_skin(char *url);
const char *skin_get_current_skin_dir(void);

#endif
