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
#include "_xitk.h"

/*
 *
 */
static void paint_label (widget_t *l,  Window win, GC gc) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;
  gui_image_t *font = (gui_image_t *) private_data->font;
  int x_dest, y_dest, nCWidth, nCHeight, nLen, i;
  char *label_to_display;

  if (l->widget_type & WIDGET_TYPE_LABEL) {

    if(private_data->animation) {
      pthread_mutex_lock(&private_data->mutex);
      label_to_display = private_data->animated_label;
      pthread_mutex_unlock(&private_data->mutex);
    }
    else
      label_to_display = private_data->label;
      
    x_dest = l->x;
    y_dest = l->y;
  
    nCWidth = font->width / 32;
    nCHeight = font->height / 3;
    nLen = strlen (label_to_display);
    
    for (i=0; i<private_data->length; i++) {
      int c=0;
      
      if ((i<nLen) && (label_to_display[i] >= 32))
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
int label_change_label (widget_list_t *wl, widget_t *l, const char *newlabel) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;

  if(l->widget_type & WIDGET_TYPE_LABEL) {
    pthread_mutex_lock(&private_data->mutex);

    if((private_data->label = (char *) 
	realloc(private_data->label, strlen(newlabel)+1)) != NULL) {

      strcpy((char*)private_data->label, (char*)newlabel);
      l->width = (private_data->char_length * strlen(newlabel));
    }

    pthread_mutex_unlock(&private_data->mutex);
    
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
void *label_animation_loop(void *data) {
  label_private_data_t *private_data = (label_private_data_t *)data;
  widget_t             *w = private_data->lWidget;
  char                 *label;
  char                 *disp_label = NULL;
  char                 *receiver = NULL;
  char                 *p;
  char                 *pp;
  int                  offset = 0;
  int                  len = 0;
  int                  length;

  pthread_detach(pthread_self());
  
  do {
    
    pthread_mutex_lock(&private_data->mutex);
    label = strdup(private_data->label);
    length = private_data->length;
    pthread_mutex_unlock(&private_data->mutex);

    if(disp_label == NULL)
      disp_label = (char *) gui_xmalloc(strlen(label) + 8);
    else
      disp_label = (char *) realloc(disp_label, strlen(label) + 8);
    
    if(receiver == NULL)
      receiver = (char *) 
	malloc((sizeof(char *) * length) + 1);
    else
      receiver = (char *) 
	realloc(receiver, (sizeof(char *) * length) + 1);
    
    memset(receiver, 0, length + 1);
    
    if(strlen(label)) {
      sprintf(disp_label, "%s  ***  ", label);
      
      p = disp_label;
      p += offset;
      
      snprintf(receiver, length, "%s", p);
      
      len = strlen(receiver);
      
      if(len < length) {
	
	p = disp_label;
	pp = receiver;
	pp += len;
	
	while(len < length) {
	  
	  *pp = *p;
	  
	  if(*(p + 1) == '\0')
	    p = disp_label;
	  else
	    p++;
	  
	  pp++;
	  len++;
	}
      }
      
      offset++;
      
      if(offset > (strlen(disp_label)-1))
	offset = 0;
    }
    
    pthread_mutex_lock(&private_data->mutex);
    if(private_data->animated_label)
      private_data->animated_label = (char *) 
	realloc(private_data->animated_label, strlen(receiver) + 1);
    else
      private_data->animated_label = (char *) 
	gui_xmalloc(strlen(receiver) + 1);
    
    sprintf(private_data->animated_label, "%s", receiver);
    
    pthread_mutex_unlock(&private_data->mutex);
    
    paint_label(private_data->lWidget, private_data->window, private_data->gc);
    
    usleep(200000);
    
  } while(w->running);
  
  pthread_exit(NULL);
}

/*
 *
 */
widget_t *label_create (xitk_label_t *l) {
  widget_t              *mywidget;
  label_private_data_t  *private_data;
  pthread_attr_t         pth_attrs;
  struct sched_param     pth_params;

  mywidget = (widget_t *) gui_xmalloc(sizeof(widget_t));
  
  private_data = (label_private_data_t *) 
    gui_xmalloc(sizeof(label_private_data_t));

  private_data->display        = l->display;

  private_data->lWidget        = mywidget;
  private_data->font           = gui_load_image(l->imlibdata, l->font);
  private_data->char_length    = (private_data->font->width/32);
  private_data->char_height    = (private_data->font->height/3);
  private_data->length         = l->length;
  private_data->label          = (char *)(l->label ? strdup(l->label) : "");
  private_data->animation      = l->animation;
  private_data->animated_label = strdup(((private_data->animation) 
					 ? private_data->label : ""));

  if(private_data->animation) {
    private_data->window       = l->window;
    private_data->gc           = l->gc;
  }

  mywidget->private_data       = private_data;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->x                  = l->x;
  mywidget->y                  = l->y;
  mywidget->width              = (private_data->char_length 
				  * strlen(private_data->label));
  mywidget->height             = private_data->char_height;
  mywidget->widget_type        = WIDGET_TYPE_LABEL;
  mywidget->paint              = paint_label;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  
  pthread_mutex_init (&private_data->mutex, NULL);

  if(private_data->animation) {

    pthread_attr_init(&pth_attrs);

    pthread_attr_getschedparam(&pth_attrs, &pth_params);

    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);

    pthread_attr_setschedparam(&pth_attrs, &pth_params);

    pthread_create(&private_data->thread, &pth_attrs, 
		   label_animation_loop, (void *)private_data);

    pthread_attr_destroy(&pth_attrs);
  }

  return mywidget;
}
