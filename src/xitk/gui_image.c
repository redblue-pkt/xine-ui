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

#include "Imlib.h"
#include "gui_widget.h"
#include "gui_image.h"
#include "gui_widget_types.h"

gui_image_t *gui_load_image(ImlibData *idata, const char *image) {
  ImlibImage *img = NULL;
  gui_image_t *i;

  i = (gui_image_t *) gui_xmalloc(sizeof(gui_image_t));
  
  if( !( img = Imlib_load_image(idata, (char *)image) ) ) {
    fprintf(stderr, "xine-panel: couldn't find image %s\n", image);
    exit(-1);
  }
  
  Imlib_render (idata, img, img->rgb_width, img->rgb_height);
  
  i->image  = Imlib_copy_image(idata, img);
  i->width  = img->rgb_width;
  i->height = img->rgb_height;
  
  return i;
} 

void paint_image (widget_t *i,  Window win, GC gc) {
  gui_image_t *skin;
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;

  XLockDisplay (private_data->display);

  skin = private_data->skin;

  if (i->widget_type & WIDGET_TYPE_IMAGE) {
    XCopyArea (private_data->display, skin->image, win, gc, 0, 0,
	       skin->width, skin->height, i->x, i->y);
    
    XFlush (private_data->display);

  } else
    fprintf (stderr, "paint image on something (%d) "
	     "that is not an image\n", i->widget_type);
  
  XUnlockDisplay (private_data->display);
}

widget_t *create_image (Display *display, ImlibData *idata,
			int x, int y, const char *skin) {
  widget_t              *mywidget;
  image_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));

  private_data = (image_private_data_t *) 
    gui_xmalloc (sizeof (image_private_data_t));

  private_data->display   = display;

  private_data->bWidget   = mywidget;
  private_data->skin      = gui_load_image(idata, skin);

  mywidget->private_data  = private_data;

  mywidget->enable        = 1;
  mywidget->x             = x;
  mywidget->y             = y;
  mywidget->width         = private_data->skin->width;
  mywidget->height        = private_data->skin->height;
  mywidget->widget_type   = WIDGET_TYPE_IMAGE;
  mywidget->paint         = paint_image;
  mywidget->notify_click  = NULL;
  mywidget->notify_focus  = NULL;

  return mywidget;
}

