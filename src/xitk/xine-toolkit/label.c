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
#include <stdlib.h>
#include <string.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "label.h"
#include "widget_types.h"

/*
 *
 */
static void paint_label (widget_t *l,  Window win, GC gc) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;
  gui_image_t *font = (gui_image_t *) private_data->font;
  int x_dest, y_dest, nCWidth, nCHeight, nLen, i;
  
  x_dest = l->x;
  y_dest = l->y;
  
  if (l->widget_type & WIDGET_TYPE_LABEL) {
    
    nCWidth = font->width / 32;
    nCHeight = font->height / 3;
    nLen = strlen (private_data->label);
    
    for (i=0; i<private_data->length; i++) {
      int c=0;
      
      if ((i<nLen) && (private_data->label[i] >= 32))
	c = private_data->label[i]-32;
      
      if (c>=0) {
	int px, py;
	
	px = (c % 32) * nCWidth;
	py = (c / 32) * nCHeight;
	
	XLOCK (private_data->display);
	XCopyArea (private_data->display, font->image, win, gc, px, py,
		   nCWidth, nCHeight, x_dest, y_dest);
	XUNLOCK (private_data->display);
	
      }
      
      x_dest += nCWidth;
    }
    
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "paint labal on something (%d) that "
	     "is not a label\n", l->widget_type);
#endif

  /* XSync(private_data->display, False); */
}

/*
 *
 */
int label_change_label (widget_list_t *wl, widget_t *l, const char *newlabel) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;

  if(l->widget_type & WIDGET_TYPE_LABEL) {
    if((private_data->label = 
	(const char *) realloc((char *) private_data->label, 
			       strlen(newlabel)+1)) != NULL) {
      strcpy((char*)private_data->label, (char*)newlabel);
      l->width = (private_data->char_length * strlen(newlabel));
    }
    paint_label(l, wl->win, wl->gc);
    return 1;
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "notify focus label button on something (%d) "
	     "that is not a label button\n", l->widget_type);
#endif

  return 0;
}

/*
 *
 */
widget_t *label_create (Display *display, ImlibData *idata,
			int x, int y, int length, 
			const char *label, char *font) {
  widget_t              *mywidget;
  label_private_data_t *private_data;

  mywidget = (widget_t *) gui_xmalloc(sizeof(widget_t));

  private_data = (label_private_data_t *) 
    gui_xmalloc(sizeof(label_private_data_t));

  private_data->display     = display;

  private_data->lWidget     = mywidget;
  private_data->font        = gui_load_image(idata, font);
  private_data->char_length = (private_data->font->width/32);
  private_data->char_height = (private_data->font->height/3);
  private_data->length      = length;
  private_data->label       = strdup(label);

  mywidget->private_data    = private_data;
  mywidget->enable          = 1;
  mywidget->x               = x;
  mywidget->y               = y;
  mywidget->width           = (private_data->char_length * strlen(label));
  mywidget->height          = private_data->char_height;
  mywidget->widget_type     = WIDGET_TYPE_LABEL;
  mywidget->paint           = paint_label;
  mywidget->notify_click    = NULL;
  mywidget->notify_focus    = NULL;
  
  return mywidget;
}
