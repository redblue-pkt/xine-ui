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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "widget_types.h"
#include "_xitk.h"
/*
 *
 */
gui_image_t *gui_load_image(ImlibData *idata, char *image) {
  ImlibImage *img = NULL;
  gui_image_t *i;

  i = (gui_image_t *) gui_xmalloc(sizeof(gui_image_t));
  
  if(!(img = Imlib_load_image(idata, (char *)image))) {
    fprintf(stderr, "xine-panel: couldn't find image %s\n", image);
    exit(-1);
  }
  
  Imlib_render (idata, img, img->rgb_width, img->rgb_height);
  
  i->image  = Imlib_copy_image(idata, img);
  i->mask   = Imlib_copy_mask(idata, img);
  i->width  = img->rgb_width;
  i->height = img->rgb_height;
  
  Imlib_destroy_image (idata, img);
  
  return i;
} 

/*
 *
 */
static void paint_image (widget_t *i, Window win, GC gc) {
  gui_image_t *skin;
  GC lgc;
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;
  
  if ((i->widget_type & WIDGET_TYPE_IMAGE) && i->visible) {

    skin = private_data->skin;
    
    XLOCK (private_data->display);

    lgc = XCreateGC(private_data->display, win, 0, 0);
    XCopyGC(private_data->display, gc, (1 << GCLastBit) - 1, lgc);
    
    if (skin->mask) {
      XSetClipOrigin(private_data->display, lgc, i->x, i->y);
      XSetClipMask(private_data->display, lgc, skin->mask);
    }
    
    XCopyArea (private_data->display, skin->image, win, lgc, 0, 0,
	       skin->width, skin->height, i->x, i->y);
    
    XFreeGC(private_data->display, lgc);

    XUNLOCK (private_data->display);
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "paint image on something (%d) "
	     "that is not an image\n", i->widget_type);
#endif
}

/*
 *
 */
widget_t *image_create (xitk_image_t *im) {
  widget_t              *mywidget;
  image_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));

  private_data = (image_private_data_t *) 
    gui_xmalloc (sizeof (image_private_data_t));

  private_data->display   = im->display;

  private_data->bWidget   = mywidget;
  private_data->skin      = gui_load_image(im->imlibdata, im->skin);

  mywidget->private_data  = private_data;

  mywidget->enable          = 1;
  mywidget->running         = 1;
  mywidget->visible         = 1;
  mywidget->have_focus      = FOCUS_LOST;
  mywidget->x               = im->x;
  mywidget->y               = im->y;
  mywidget->width           = private_data->skin->width;
  mywidget->height          = private_data->skin->height;
  mywidget->widget_type     = WIDGET_TYPE_IMAGE;
  mywidget->paint           = paint_image;
  mywidget->notify_click    = NULL;
  mywidget->notify_focus    = NULL;
  mywidget->notify_keyevent = NULL;

  return mywidget;
}
