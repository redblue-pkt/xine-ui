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
#include <pthread.h>
#include <unistd.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "label.h"
#include "widget_types.h"
#include "utils.h"
#include "_xitk.h"

/*
 *
 */
static void paint_label(xitk_widget_t *l, Window win, GC gc) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;
  xitk_image_t          *font = (xitk_image_t *) private_data->font;
  int                   x_dest, y_dest, nCWidth, nCHeight, len, i;
  char                 *label_to_display;

  if ((l->widget_type & WIDGET_TYPE_LABEL) && l->visible) {

    pthread_mutex_lock(&private_data->mutex);
    if (private_data->anim_running) {
      label_to_display = 
	&private_data->animated_label[private_data->anim_offset];
      len = private_data->length;
    } 
    else {
      label_to_display = private_data->label;
      len = strlen(label_to_display);
    }
    pthread_mutex_unlock(&private_data->mutex);

    x_dest = l->x;
    y_dest = l->y;
  
    nCWidth = font->width / 32;
    nCHeight = font->height / 3;
    
    for (i=0; i<private_data->length; i++) {
      int c=0;
      
      if ((i<len) && (label_to_display[i] >= 32))
	c = label_to_display[i]-32;
      
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
}

/*
 *
 */
void *xitk_label_animation_loop(void *data) {
  label_private_data_t *private_data = (label_private_data_t *)data;
  xitk_widget_t             *w = private_data->lWidget;

  do {
    
    if(w->visible) {

      private_data->anim_offset++;
      if (private_data->anim_offset>(strlen(private_data->label) + 4))
	private_data->anim_offset = 0;
      
      paint_label(private_data->lWidget,
		  private_data->window, private_data->gc);
      
      /* We can't wait here, otherwise the rolling effect is really jerky */
      XLOCK (private_data->display);
      XSync(private_data->display, False);
      XUNLOCK (private_data->display);
      
    }

    xine_usec_sleep(400000);
    
  } while(w->running && private_data->anim_running);
  
  pthread_exit(NULL);
}

/*
 *
 */
static void label_setup_label(xitk_widget_t *l, char *label_) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;

  int label_len;

  pthread_mutex_lock(&private_data->mutex);

  if (private_data->label) {
    XITK_FREE(private_data->label);
    private_data->label = NULL;
  }

  if (private_data->animated_label) {
    XITK_FREE(private_data->animated_label);
    private_data->animated_label = NULL;
  }

  label_len = strlen(label_);

  private_data->label = strdup((label_ != NULL) ? label_ : "");

  /*
  private_data->label = (char *) xitk_xmalloc(label_len + 1);
  strncpy (private_data->label, label_, label_len);
  private_data->label[label_len] = 0;
  */

  if (private_data->animation) {

    if (private_data->anim_running) {
      void *dummy;

      private_data->anim_running = 0;
      pthread_join (private_data->thread, &dummy);
    }

    if (label_len > private_data->length) {
      pthread_attr_t       pth_attrs;
      struct sched_param   pth_params;

      private_data->anim_running = 1;

      private_data->animated_label = (char *) 
	xitk_xmalloc(2 * label_len + 11);

      sprintf(private_data->animated_label, "%s *** %s *** ", label_, label_) ;
      
      private_data->anim_offset = 0;

      pthread_attr_init(&pth_attrs);

      /* this won't work on linux, freebsd 5.0 */
      pthread_attr_getschedparam(&pth_attrs, &pth_params);
      pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
      pthread_attr_setschedparam(&pth_attrs, &pth_params);

      pthread_create(&private_data->thread, &pth_attrs, 
		     xitk_label_animation_loop, (void *)private_data);

      pthread_attr_destroy(&pth_attrs);
    }
  }
  
  pthread_mutex_unlock(&private_data->mutex);
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *l, xitk_skin_config_t *skonfig) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;
  
  if(l->widget_type & WIDGET_TYPE_LABEL) {
    
    XITK_FREE_XITK_IMAGE(private_data->display, private_data->font);
    private_data->font        = xitk_load_image(private_data->imlibdata, 
						   xitk_skin_get_label_skinfont_filename(skonfig, private_data->skin_element_name));
    private_data->char_length = (private_data->font->width/32);
    private_data->char_height = (private_data->font->height/3);
    
    private_data->length      = xitk_skin_get_label_length(skonfig, private_data->skin_element_name);
    private_data->animation   = xitk_skin_get_label_animation(skonfig, private_data->skin_element_name);
    
    l->x                      = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
    l->y                      = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
    l->width                  = private_data->char_length * private_data->length;
    l->height                 = private_data->char_height;

    xitk_set_widget_pos(l, l->x, l->y);
  }
}

/*
 *
 */
int xitk_label_change_label (xitk_widget_list_t *wl, xitk_widget_t *l, char *newlabel) {

  if(l->widget_type & WIDGET_TYPE_LABEL) {

    label_setup_label(l, newlabel);
    
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
xitk_widget_t *xitk_label_create(xitk_skin_config_t *skonfig, xitk_label_widget_t *l) {
  xitk_widget_t              *mywidget;
  label_private_data_t  *private_data;
  
  XITK_CHECK_CONSTITENCY(l);

  //  if(!l->skin_element_name) XITK_DIE("skin element name is required.\n");

  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  
  private_data = (label_private_data_t *) 
    xitk_xmalloc(sizeof(label_private_data_t));

  private_data->display        = l->display;
  private_data->imlibdata      = l->imlibdata;
  private_data->skin_element_name = strdup(l->skin_element_name);

  private_data->lWidget        = mywidget;

  private_data->font           = xitk_load_image(private_data->imlibdata, 
						 xitk_skin_get_label_skinfont_filename(skonfig, private_data->skin_element_name));
  private_data->char_length    = (private_data->font->width/32);
  private_data->char_height    = (private_data->font->height/3);

  private_data->length         = xitk_skin_get_label_length(skonfig, private_data->skin_element_name);
  private_data->label          = NULL;
  private_data->animation      = xitk_skin_get_label_animation(skonfig, private_data->skin_element_name);
  private_data->animated_label = NULL;
  private_data->anim_running   = 0;
  private_data->window         = l->window;
  private_data->gc             = l->gc;

  mywidget->private_data       = private_data;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->x                  = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
  mywidget->y                  = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
  mywidget->width              = private_data->char_length * private_data->length;
  mywidget->height             = private_data->char_height;
  mywidget->widget_type        = WIDGET_TYPE_LABEL;
  mywidget->paint              = paint_label;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = NULL;
  mywidget->notify_change_skin = notify_change_skin;
  
  pthread_mutex_init(&private_data->mutex, NULL);

  label_setup_label(mywidget, l->label);

  return mywidget;
}
