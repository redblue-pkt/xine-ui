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
xitk_image_t *xitk_load_image(ImlibData *idata, char *image) {
  ImlibImage *img = NULL;
  xitk_image_t *i;

  i = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
  
  if(!(img = Imlib_load_image(idata, (char *)image))) {
    XITK_DIE("%s(): couldn't find image %s\n", __FUNCTION__, image);
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
static void paint_image (xitk_widget_t *i, Window win, GC gc) {
  xitk_image_t *skin;
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
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *i, xitk_skin_config_t *skonfig) {
  image_private_data_t *private_data = 
    (image_private_data_t *) i->private_data;
  
  if ((i->widget_type & WIDGET_TYPE_IMAGE) && i->visible) {
    
    XITK_FREE_XITK_IMAGE(private_data->display, private_data->skin);
    private_data->skin = xitk_load_image(private_data->imlibdata,
					 xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
    
    i->x               = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
    i->y               = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
    i->width           = private_data->skin->width;
    i->height          = private_data->skin->height;
    
    xitk_set_widget_pos(i, i->x, i->y);
  }
}

/*
 *
 */
xitk_widget_t *xitk_image_create (xitk_skin_config_t *skonfig, xitk_image_widget_t *im) {
  xitk_widget_t              *mywidget;
  image_private_data_t *private_data;

  XITK_CHECK_CONSTITENCY(im);

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (image_private_data_t *) 
    xitk_xmalloc (sizeof (image_private_data_t));

  private_data->display           = im->display;
  private_data->imlibdata         = im->imlibdata;
  private_data->skin_element_name = strdup(im->skin_element_name);


  private_data->bWidget           = mywidget;
  private_data->skin              = xitk_load_image(private_data->imlibdata,
						    xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->x                     = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
  mywidget->y                     = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
  mywidget->width                 = private_data->skin->width;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_IMAGE;
  mywidget->paint                 = paint_image;
  mywidget->notify_click          = NULL;
  mywidget->notify_focus          = NULL;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_change_skin    = notify_change_skin;

  return mywidget;
}
