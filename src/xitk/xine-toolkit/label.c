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
  int x_dest, y_dest, nCWidth, nCHeight, len, i;
  char *label_to_display;

  if ((l->widget_type & WIDGET_TYPE_LABEL) && l->visible) {

    pthread_mutex_lock(&private_data->mutex);
    if (private_data->anim_running) {
      label_to_display = &private_data->animated_label[private_data->anim_offset];
      len = private_data->length;
    } else {
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
void *label_animation_loop(void *data) {
  label_private_data_t *private_data = (label_private_data_t *)data;
  widget_t             *w = private_data->lWidget;

  do {
    
    if(w->visible) {

      private_data->anim_offset++;
      if (private_data->anim_offset>(strlen(private_data->label) + 4))
	private_data->anim_offset = 0;
      
      paint_label(private_data->lWidget, private_data->window, private_data->gc);
      
      /* We can't wait here, otherwise the rolling effect is really jerky */
      XLOCK (private_data->display);
      XSync(private_data->display, False);
      XUNLOCK (private_data->display);
      
    }

#if HAVE_NANOSLEEP
    /* nanosleep is prefered on solaris, because it's mt-safe */
    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 400000000;
      nanosleep(&ts, NULL);
    }
#else
    usleep(400000);
#endif
    
  } while(w->running && private_data->anim_running);
  
  pthread_exit(NULL);
}

static void label_setup_label (widget_t *l, char *label_) {
  label_private_data_t *private_data = 
    (label_private_data_t *) l->private_data;

  int label_len;

  pthread_mutex_lock(&private_data->mutex);

  if (private_data->label) {
    free(private_data->label);
    private_data->label = NULL;
  }

  if (private_data->animated_label) {
    free(private_data->animated_label);
    private_data->animated_label = NULL;
  }

  label_len = strlen(label_);
  private_data->label = malloc (label_len +1);
  strncpy (private_data->label, label_, label_len);
  private_data->label[label_len] = 0;

  if (private_data->animation) {

    if (private_data->anim_running) {
      void *dummy;
      private_data->anim_running = 0;
      pthread_join (private_data->thread, &dummy);
    }

    if (label_len > private_data->length) {
      pthread_attr_t         pth_attrs;
      struct sched_param     pth_params;

      private_data->anim_running = 1;

      private_data->animated_label = (char *) malloc(2 * strlen(label_) + 12);

      sprintf(private_data->animated_label, "%s *** %s *** ", label_, label_) ;
      
      private_data->anim_offset = 0;

      pthread_attr_init(&pth_attrs);

      /* this won't work on linux, freebsd 5.0 */
      pthread_attr_getschedparam(&pth_attrs, &pth_params);
      pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
      pthread_attr_setschedparam(&pth_attrs, &pth_params);

      pthread_create(&private_data->thread, &pth_attrs, 
		     label_animation_loop, (void *)private_data);

      pthread_attr_destroy(&pth_attrs);
    }
  }
  
  pthread_mutex_unlock(&private_data->mutex);
}

/*
 *
 */
int label_change_label (widget_list_t *wl, widget_t *l, const char *newlabel) {

  if(l->widget_type & WIDGET_TYPE_LABEL) {

    label_setup_label (l, newlabel);
    
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
widget_t *label_create (xitk_label_t *l) {
  widget_t              *mywidget;
  label_private_data_t  *private_data;

  mywidget = (widget_t *) gui_xmalloc(sizeof(widget_t));
  
  private_data = (label_private_data_t *) 
    gui_xmalloc(sizeof(label_private_data_t));

  private_data->display        = l->display;

  private_data->lWidget        = mywidget;
  private_data->font           = gui_load_image(l->imlibdata, l->font);
  private_data->char_length    = (private_data->font->width/32);
  private_data->char_height    = (private_data->font->height/3);
  private_data->length         = l->length;
  private_data->label          = NULL;
  private_data->animation      = l->animation;
  private_data->animated_label = NULL;
  private_data->anim_running   = 0;
  private_data->window         = l->window;
  private_data->gc             = l->gc;

  mywidget->private_data       = private_data;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->x                  = l->x;
  mywidget->y                  = l->y;
  mywidget->width              = private_data->char_length * private_data->length;
  mywidget->height             = private_data->char_height;
  mywidget->widget_type        = WIDGET_TYPE_LABEL;
  mywidget->paint              = paint_label;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  
  pthread_mutex_init (&private_data->mutex, NULL);

  label_setup_label (mywidget, l->label);

  return mywidget;
}



