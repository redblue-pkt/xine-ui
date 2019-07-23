/* 
 * Copyright (C) 2000-2009 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

/* sys/time.h does not define timersub() on all platforms... */
#ifndef timersub
# define timersub(a, b, result)                                               \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
#endif

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
 * cleanup the str string, take care about '
 */
char *atoa(char *str);

/*
 * Create directories recursively
 */
int mkdir_safe(const char *path);

/*
 * return 0/1 regarding to string (1/0, true/false, yes/no, on/off).
 */
int get_bool_value(const char *val);


const char *get_last_double_semicolon(const char *str);
int is_ipv6_last_double_semicolon(const char *str);

int is_downloadable(const char *filename);
int is_a_dir(const char *filename);
int is_a_file(const char *filename);

#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size);
#endif

char *xitk_vasprintf(const char *fmt, va_list ap)  __attribute__ ((format (printf, 1, 0)));
char *xitk_asprintf(const char *fmt, ...)  __attribute__ ((format (printf, 1, 2)));

#endif
