/* 
 * Copyright (C) 2000-2001 the xine project
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
#ifndef HAVE_XITK_SKIN_H
#define HAVE_XITK_SKIN_H

#include <pthread.h>

typedef struct xitk_skin_element_s {
  struct xitk_skin_element_s   *prev;
  struct xitk_skin_element_s   *next;

  char                         *section;

  int                           enable;
  int                           visible;
  
  char                         *pixmap;
  int                           x;
  int                           y;

  /* slider */
  char                         *pixmap_pad;
  int                           slider_type;
  
  /* label */
  int                           print;
  int                           align;
  int                           animation;
  int                           length;
  char                         *pixmap_font;
  char                         *color;
  char                         *color_focus;
  char                         *color_click;
  char                         *font;
} xitk_skin_element_t;

typedef struct {
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

  pthread_mutex_t               mutex;
  char                         *load_command;
  char                         *unload_command;

  xitk_skin_element_t          *first, *last;
  xitk_skin_element_t          *celement;
} xitk_skin_config_t;

#endif
