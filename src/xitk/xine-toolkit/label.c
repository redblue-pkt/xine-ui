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
#include "font.h"
#include "image.h"
#include "label.h"
#include "widget_types.h"
#include "utils.h"
#include "_xitk.h"

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  label_private_data_t *private_data = (label_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_LABEL) {

    if(private_data->anim_running) {
      void *dummy;
      
      private_data->anim_running = 0;
      pthread_join(private_data->thread, &dummy);
    }

    XITK_FREE(private_data->animated_label);
    XITK_FREE(private_data->fontname);
    XITK_FREE(private_data->skin_element_name);
    if(private_data->font)
      xitk_image_free_image(private_data->imlibdata, &private_data->font);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  label_private_data_t *private_data = 
    (label_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_LABEL) {
    if(sk == FOREGROUND_SKIN && private_data->font) {
      return private_data->font;
    }
  }

  return NULL;
}

/*
 *
 */
char *xitk_label_get_label(xitk_widget_t *w) {
  label_private_data_t *private_data;
  
  if(w->widget_type & WIDGET_TYPE_LABEL) {
    private_data = (label_private_data_t *) w->private_data;
    return private_data->label;
  }

  return NULL;
}

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

    /* non skinable widget */
    if(private_data->skin_element_name == NULL) {
      xitk_font_t   *fs = NULL;
      int            lbear, rbear, wid, asc, des;
      xitk_image_t  *bg;

      /* Clean old */
      XLOCK (private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, private_data->font->image, win, 
		 gc, 0, 0, private_data->font->width, private_data->font->height, l->x, l->y);
      XUNLOCK (private_data->imlibdata->x.disp);

      fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
      xitk_font_set_font(fs, gc);
      xitk_font_string_extent(fs, private_data->label, &lbear, &rbear, &wid, &asc, &des);

      bg = xitk_image_create_image(private_data->imlibdata, l->width, l->height);

      XLOCK (private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, private_data->font->image, bg->image, 
		 gc, 0, 0, private_data->font->width, private_data->font->height, 0, 0);
      XSetForeground(private_data->imlibdata->x.disp, gc, 
		     xitk_get_pixel_color_black(private_data->imlibdata));
      XDrawString(private_data->imlibdata->x.disp, bg->image, gc,
		  2, ((private_data->font->height + asc + des)>>1) - des,
		  private_data->label, strlen(private_data->label));
      XCopyArea (private_data->imlibdata->x.disp, bg->image, win, 
		 gc, 0, 0, private_data->font->width, private_data->font->height, l->x, l->y);
      XUNLOCK (private_data->imlibdata->x.disp);

      xitk_image_free_image(private_data->imlibdata, &bg);
      
      xitk_font_unload_font(fs);

      return;
    }

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
	
	XLOCK (private_data->imlibdata->x.disp);
	XCopyArea (private_data->imlibdata->x.disp, font->image, win, gc, px, py,
		   nCWidth, nCHeight, x_dest, y_dest);
	XUNLOCK (private_data->imlibdata->x.disp);
	
      }
      
      x_dest += nCWidth;
    }

   }
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
      XLOCK (private_data->imlibdata->x.disp);
      XSync(private_data->imlibdata->x.disp, False);
      XUNLOCK (private_data->imlibdata->x.disp);
      
    }

    xitk_usec_sleep(400000);
    
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
      /*
       * xitk_label_animation_loop may be already running due to a call
       * from gui_dndcallback, inside mrl_add_and_play. paint_label will
       * try to lock this mutex and will block. If we don�t unlock it here,
       * we will block too waiting for the join... Deadlock!
       */
      pthread_mutex_unlock(&private_data->mutex);
      pthread_join (private_data->thread, &dummy);
      pthread_mutex_lock(&private_data->mutex);
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
    if(private_data->skin_element_name) {
      XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->font);
      private_data->font        = xitk_image_load_image(private_data->imlibdata, 
							xitk_skin_get_label_skinfont_filename(skonfig, private_data->skin_element_name));
      private_data->char_length = (private_data->font->width/32);
      private_data->char_height = (private_data->font->height/3);
      
      private_data->length      = xitk_skin_get_label_length(skonfig, private_data->skin_element_name);
      private_data->animation   = xitk_skin_get_label_animation(skonfig, private_data->skin_element_name);
      
      l->x                      = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      l->y                      = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      l->width                  = private_data->char_length * private_data->length;
      l->height                 = private_data->char_height;
      l->visible                = xitk_skin_get_visibility(skonfig, private_data->skin_element_name);
      l->enable                 = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
      
      xitk_set_widget_pos(l, l->x, l->y);
    }
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

  return 0;
}

/*
 *
 */
static xitk_widget_t *_xitk_label_create(xitk_skin_config_t *skonfig, xitk_label_widget_t *l,
					 int x, int y, int width, int height,
					 char *skin_element_name, char *fontname,
					 int visible, int enable) {
  xitk_widget_t          *mywidget;
  label_private_data_t   *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  
  private_data = (label_private_data_t *) xitk_xmalloc(sizeof(label_private_data_t));

  private_data->imlibdata      = l->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(l->skin_element_name);

  private_data->lWidget        = mywidget;

  if(skin_element_name == NULL) {
    private_data->font         = xitk_image_create_image(private_data->imlibdata, width, height);
    draw_flat(private_data->imlibdata, private_data->font->image, 
	      private_data->font->width, private_data->font->height);

    private_data->fontname     = strdup(fontname);
    private_data->char_length  = 0;
    private_data->char_height  = 0;
    private_data->length       = width;
  }
  else {
    private_data->font         = xitk_image_load_image(private_data->imlibdata, 
							 xitk_skin_get_label_skinfont_filename(skonfig, private_data->skin_element_name));
  
    private_data->fontname     = NULL;
    private_data->char_length  = (private_data->font->width/32);
    private_data->char_height  = (private_data->font->height/3);
    private_data->length       = xitk_skin_get_label_length(skonfig, private_data->skin_element_name);
  }

  private_data->label          = NULL;
  private_data->animation      = (skin_element_name == NULL) ? 0 : xitk_skin_get_label_animation(skonfig, private_data->skin_element_name);
  private_data->animated_label = NULL;
  private_data->anim_running   = 0;
  private_data->window         = l->window;
  private_data->gc             = l->gc;

  mywidget->private_data       = private_data;

  mywidget->enable             = enable;
  mywidget->running            = 1;
  mywidget->visible            = visible;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->imlibdata          = private_data->imlibdata;
  mywidget->x                  = x;
  mywidget->y                  = y;

  if(skin_element_name == NULL) {
    mywidget->width            = width;
    mywidget->height           = height;
  }
  else {
    mywidget->width            = private_data->char_length * private_data->length;
    mywidget->height           = private_data->char_height;
  }
  mywidget->widget_type        = WIDGET_TYPE_LABEL;
  mywidget->paint              = paint_label;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = NULL;
  mywidget->notify_change_skin = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy     = notify_destroy;
  mywidget->get_skin           = get_skin;
  
  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  pthread_mutex_init(&private_data->mutex, NULL);

  label_setup_label(mywidget, l->label);

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_label_create(xitk_skin_config_t *skonfig, xitk_label_widget_t *l) {

  XITK_CHECK_CONSTITENCY(l);
  
  return _xitk_label_create(skonfig, l, 
			    (xitk_skin_get_coord_x(skonfig, l->skin_element_name)),
			    (xitk_skin_get_coord_y(skonfig, l->skin_element_name)),
			    (xitk_skin_get_label_length(skonfig, l->skin_element_name)),
			    -1,
			    l->skin_element_name, 
			    NULL,
			    (xitk_skin_get_visibility(skonfig, l->skin_element_name)),
			    (xitk_skin_get_enability(skonfig, l->skin_element_name)));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_label_create(xitk_label_widget_t *l,
					int x, int y, int width, int height, char *fontname) {
  XITK_CHECK_CONSTITENCY(l);

  return _xitk_label_create(NULL, l, x, y, width, height, NULL, fontname, 1, 1);
}
