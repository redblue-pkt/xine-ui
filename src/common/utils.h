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
#ifndef _COMMON_UTILS_H
#define _COMMON_UTILS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

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
int xine_system(int dont_run_as_root, char *command);

/*
 * cleanup the str string, take care about '
 */
char *atoa(char *str);

/*
 * Create directories recursively
 */
int mkdir_safe(char *path);

/*
 * return 0/1 regarding to string (1/0, true/false, yes/no, on/off).
 */
int get_bool_value(const char *val);


const char *get_last_double_semicolon(const char *str);
int is_ipv6_last_double_semicolon(const char *str);

int is_downloadable(char *filename);
int is_a_dir(char *filename);
int is_a_file(char *filename);

void dump_host_info(void);
#ifdef HAVE_X11
void dump_cpu_infos(void);
void dump_xfree_info(Display *display, int screen, int complete);
#endif

#endif
