/*
 * Copyright (C) 2000-2022 the xine project
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
#ifndef _COMMON_UTILS_H
#define _COMMON_UTILS_H

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#ifndef NAME_MAX
#define _NAME_MAX          256
#else
#define _NAME_MAX          NAME_MAX
#endif

#ifndef PATH_MAX
#define _PATH_MAX          768
#else
#define _PATH_MAX          PATH_MAX
#endif

/*
 * Execute a shell command.
 */
int xine_system(int dont_run_as_root, const char *command);

/*
 * trim/reduce quoting and whitespace, return new strlen ().
 */
size_t str_unquote (char *str);

/*
 * Create directories recursively, return 0 (created), EEXIST (found), or Exxx (failed).
 */
int mkdir_safe (const char *path);

/*
 * return 0/1 regarding to string (1/0, true/false, yes/no, on/off).
 */
int get_bool_value(const char *val);

#ifdef NEED_DOUBLE_SEMICOLON
const char *get_last_double_semicolon(const char *str);
int is_ipv6_last_double_semicolon(const char *str);
#endif

int is_downloadable(const char *filename);
int is_a_dir(const char *filename);
int is_a_file(const char *filename);

#ifndef HAVE_STRLCPY
size_t strlcpy (char *dst, const char *src, size_t size) ATTR_INLINE_ALL_STRINGOPS;
#endif

char *xitk_vasprintf(const char *fmt, va_list ap)  __attribute__ ((format (printf, 1, 0)));
char *xitk_asprintf(const char *fmt, ...)  __attribute__ ((format (printf, 1, 2)));

#endif
