/* 
 * Copyright (C) 2000 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <pthread.h>
#include "xine.h"
#include "gui_widget.h"
#include "gui_image.h"
#include "gui_widget_types.h"
#include "gui_main.h"
#include "utils.h"

extern Display         *gDisplay;
extern pthread_mutex_t  gXLock;
extern uint32_t         xine_debug;

void paint_image (widget_t *i,  Window win, GC gc) {
  gui_image_t *skin;
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;

  XLOCK ();

  skin = private_data->skin;

  if (i->widget_type & WIDGET_TYPE_IMAGE) {
    XCopyArea (gDisplay, skin->image, win, gc, 0, 0,
	       skin->width, skin->height, i->x, i->y);
    
    XFlush (gDisplay);

  } else
    xprintf (VERBOSE|GUI, "paint image on something (%d) "
	     "that is not an image\n", i->widget_type);
  
  XUNLOCK ();
}

widget_t *create_image (int x, int y, const char *skin) {
  widget_t              *mywidget;
  image_private_data_t *private_data;

  mywidget = (widget_t *) xmalloc (sizeof(widget_t));

  private_data = (image_private_data_t *) 
    xmalloc (sizeof (image_private_data_t));

  private_data->bWidget   = mywidget;
  private_data->skin      = gui_load_image(skin);

  mywidget->private_data = private_data;

  mywidget->enable       = 1;
  mywidget->x            = x;
  mywidget->y            = y;
  mywidget->width        = private_data->skin->width;
  mywidget->height       = private_data->skin->height;
  mywidget->widget_type  = WIDGET_TYPE_IMAGE;
  mywidget->paint        = paint_image;
  mywidget->notify_click = NULL;
  mywidget->notify_focus = NULL;

  return mywidget;
}

