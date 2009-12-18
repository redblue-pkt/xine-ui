/* 
 * Copyright (C) 2008 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "globals.h"

/**
 * @brief Global xine instance
 */
xine_t *__xineui_global_xine_instance;

/**
 * @brief Configuration file name
 */
char *__xineui_global_config_file;

/**
 * @brief Verbosity level for the xine fronend
 */
int __xineui_global_verbosity;

#ifdef HAVE_LIRC
/**
 * @brief Boolean flag to check if LIRC support is enabled at runtime.
 */
int __xineui_global_lirc_enable;
#endif
