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

#ifndef CONFIG_WRAPPER_H
#define CONFIG_WRAPPER_H

void config_update_range(char *key, int min, int max);
void config_update_string(char *key, char *string);
void config_update_enum(char *key, int value);
void config_update_bool(char *key, int value);
void config_update_num(char *key, int value);
void config_load(void);
void config_save(void);
void config_reset(void);
void config_mrl(const char *mrl);

#endif
