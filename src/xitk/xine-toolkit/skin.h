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
#ifndef HAVE_XITK_SKIN_H
#define HAVE_XITK_SKIN_H

#include <pthread.h>
#include <xine/sorted_array.h>

typedef struct {
  /* this needs to stay first. */
  char                          section[64];

  xitk_skin_element_info_t      info;
} xitk_skin_element_t;

struct xitk_skin_config_s {
  xitk_t                       *xitk;
  FILE                         *fd;
  char                         *path;
  char                         *skinfile;
  char                         *ln;
  char                          buf[256];

  char                         *name;
  int                           version;
  char                         *author;
  char                         *date;
  char                         *url;
  char                         *logo;
  char                         *animation;

  char                         *load_command;
  char                         *unload_command;

  xine_sarray_t                *elements;
  xitk_skin_element_t          *celement;

  xine_sarray_t                *imgs;

  pthread_mutex_t               skin_mutex;
};

#endif
