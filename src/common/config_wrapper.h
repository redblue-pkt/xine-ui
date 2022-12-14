/*
 * Copyright (C) 2000-2021 the xine project
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

#ifndef CONFIG_WRAPPER_H
#define CONFIG_WRAPPER_H

int config_update_string (xine_t *xine, const char *key, const char *string);
#define config_update_enum(_xine,_key,_value) config_update_num (_xine, _key, _value)
#define config_update_bool(_xine,_key,_value) config_update_num (_xine, _key, (value) > 0)
int config_update_num (xine_t *xine, const char *key, int value);
int config_mrl (xine_t *xine, const char *mrl);

#endif
