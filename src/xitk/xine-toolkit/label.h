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

#ifndef HAVE_XITK_LABEL_H
#define HAVE_XITK_LABEL_H

#include <X11/Xlib.h>
#include <pthread.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {
  ImlibData        *imlibdata;
  char             *skin_element_name;
  GC                gc;
  Window            window;

  xitk_widget_t    *lWidget;


  int               char_length; /* length of 1 char */
  int               char_height; /* height of 1 char */

  int               length;      /* length in char */
  xitk_image_t     *font;
  char             *fontname;
  char             *label;

  int               animation;
  char             *animated_label;
  int               anim_running;
  int               anim_offset;

  pthread_t         thread;
  pthread_mutex_t   mutex;

} label_private_data_t;

/* ************************************************************** */

/**
 * Create a label widget.
 */
xitk_widget_t *xitk_label_create (xitk_skin_config_t *skonfig, xitk_label_widget_t *l);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_label_create(xitk_label_widget_t *l,
					int x, int y, int width, int height, char *fontname);

/**
 * Change label of widget 'widget'.
 */
int xitk_label_change_label (xitk_widget_list_t *wl, xitk_widget_t *l, char *newlabel);

#endif

