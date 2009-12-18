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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
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
  int                           direction;

  /* browser */
  int                           browser_entries;
  
  /* slider */
  char                         *pixmap_pad;
  int                           slider_type;
  int                           radius;
  
  /* label */
  int                           staticity;
  int                           print;
  int                           align;
  int                           animation;
  int                           animation_step;
  unsigned long                 animation_timer;
  int                           length;
  char                         *pixmap_font;
  char                         *color;
  char                         *color_focus;
  char                         *color_click;
  char                         *font;

  int                          max_buttons;
} xitk_skin_element_t;

typedef struct cache_entry_s cache_entry_t;
struct cache_entry_s {
  xitk_image_t   *image;
  char           *filename;
  cache_entry_t  *next;
};

struct xitk_skin_config_s {
  ImlibData                    *im;
  FILE                         *fd;
  char                         *path;
  char                         *skinfile;
  char                         *ln;
  char                          buf[256];

  cache_entry_t                *cache;
  
  char                         *name;
  int                           version;
  char                         *author;
  char                         *date;
  char                         *url;
  char                         *logo;
  char                         *animation;

  char                         *load_command;
  char                         *unload_command;
  
  xitk_skin_element_t          *first, *last;
  xitk_skin_element_t          *celement;
};

#endif
