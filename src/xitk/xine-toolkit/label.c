/* 
 * Copyright (C) 2000-2002 the xine project
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

static void _create_label_pixmap(xitk_widget_t *w) {
  label_private_data_t  *private_data = (label_private_data_t *) w->private_data;
  xitk_image_t          *font         = (xitk_image_t *) private_data->font;
  int                    pixwidth;
  char                  *_label;
  int                    x_dest, i;
  int                    len;
  
  private_data->anim_offset = 0;

  if(private_data->animation) {
    _label = (char *) alloca((strlen(private_data->label) * 2) + 5 + 1);
    
    if((strlen(private_data->label)) > private_data->length)
      sprintf(_label, "%s *** %s", private_data->label, private_data->label);
    else
      sprintf(_label, "%s", private_data->label);
    
  }
  else
    xitk_strdupa(_label, private_data->label);
  
  len = strlen(_label);
  pixwidth = private_data->char_length * 
    ((private_data->length * ((len / private_data->length) + 1)) + 5);

  private_data->labelpix = xitk_image_create_xitk_pixmap(private_data->imlibdata,
							 (pixwidth) ? pixwidth : 1, 
							 private_data->char_height);

  x_dest = 0;
    
  for (i = 0; i < pixwidth; i++) {
    int c = 0;
    
    if ((i < len) && (_label[i] >= 32))
      c = _label[i] - 32;
    
    if (c >= 0) {
      int px, py;
      
      px = (c % 32) * private_data->char_length;
      py = (c / 32) * private_data->char_height;
      
      XLOCK(private_data->imlibdata->x.disp);
      XCopyArea(private_data->imlibdata->x.disp, font->image->pixmap, 
		private_data->labelpix->pixmap, private_data->labelpix->gc, px, py,
		private_data->char_length, private_data->char_height, x_dest, 0);
      XUNLOCK(private_data->imlibdata->x.disp);
     
    }
    
    x_dest += private_data->char_length;
  }

}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  label_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    private_data = (label_private_data_t *) w->private_data;
    
    private_data->on_change = 1;
    
    if(private_data->anim_running) {
      void *dummy;
      
      private_data->anim_running = 0;
      pthread_join(private_data->thread, &dummy);
    }
    
    if (private_data->labelpix) {
      xitk_image_destroy_xitk_pixmap(private_data->labelpix);
      private_data->labelpix = NULL;
    }
    
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
  label_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    private_data = (label_private_data_t *) w->private_data;

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
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    private_data = (label_private_data_t *) w->private_data;
    return private_data->label;
  }

  return NULL;
}

/*
 *
 */
static void paint_label(xitk_widget_t *w, Window win, GC gc) {
  label_private_data_t  *private_data;
  xitk_image_t          *font;

  if (w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL) && (w->visible == 1))) {
    
    private_data = (label_private_data_t *) w->private_data;
    font = (xitk_image_t *) private_data->font;

    /* non skinable widget */
    if(private_data->skin_element_name == NULL) {
      xitk_font_t   *fs = NULL;
      int            lbear, rbear, wid, asc, des;
      xitk_image_t  *bg;

      /* Clean old */
      XLOCK (private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, private_data->font->image->pixmap, win, 
		 private_data->font->image->gc, 
		 0, 0, private_data->font->width, private_data->font->height, w->x, w->y);
      XUNLOCK (private_data->imlibdata->x.disp);

      fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
      xitk_font_set_font(fs, private_data->font->image->gc);
      xitk_font_string_extent(fs, private_data->label, &lbear, &rbear, &wid, &asc, &des);

      bg = xitk_image_create_image(private_data->imlibdata, w->width, w->height);

      XLOCK (private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, 
		 private_data->font->image->pixmap, bg->image->pixmap, 
		 private_data->font->image->gc, 
		 0, 0, private_data->font->width, private_data->font->height, 0, 0);
      XSetForeground(private_data->imlibdata->x.disp, private_data->font->image->gc, 
		     xitk_get_pixel_color_black(private_data->imlibdata));
      XDrawString(private_data->imlibdata->x.disp, bg->image->pixmap, private_data->font->image->gc,
		  2, ((private_data->font->height + asc + des)>>1) - des,
		  private_data->label, strlen(private_data->label));
      XCopyArea (private_data->imlibdata->x.disp, bg->image->pixmap, win, 
		 private_data->font->image->gc, 0, 0, private_data->font->width, private_data->font->height, w->x, w->y);
      XUNLOCK (private_data->imlibdata->x.disp);

      xitk_image_free_image(private_data->imlibdata, &bg);
      
      xitk_font_unload_font(fs);
      
      return;
    }
    else {
      int width = private_data->char_length * private_data->length;
      
      XLOCK(private_data->imlibdata->x.disp);
      XCopyArea(private_data->imlibdata->x.disp, private_data->labelpix->pixmap, win, private_data->font->image->gc, 
		private_data->anim_offset, 0, 
		width, private_data->char_height, 
		w->x, w->y);
      XUNLOCK(private_data->imlibdata->x.disp);
      
    }
  }
}

/*
 *
 */
void *xitk_label_animation_loop(void *data) {
  label_private_data_t *private_data = (label_private_data_t *)data;
  xitk_widget_t        *w            = private_data->lWidget;
  unsigned long         t_anim       = xitk_get_timer_label_animation();
  
  do {
    
    if((w->visible == 1)) {
      
      private_data->anim_offset++;

      if (private_data->anim_offset >
	  (private_data->char_length * (strlen(private_data->label) + 5)))
	private_data->anim_offset = 1;

      /* 
       * Label will change sooner, don't try to paint it till the change,
       * otherwise a deadlock will happened.
       */
      if(!private_data->on_change) {
	paint_label(private_data->lWidget,
		    private_data->window, private_data->font->image->gc);
	
	/* We can't wait here, otherwise the rolling effect is really jerky */
	XLOCK (private_data->imlibdata->x.disp);
	XSync(private_data->imlibdata->x.disp, False);
	XUNLOCK (private_data->imlibdata->x.disp);
      }
    }
    
    xitk_usec_sleep(t_anim);
    
  } while(w->running && private_data->anim_running);
  
  pthread_exit(NULL);
}

/*
 *
 */
static void label_setup_label(xitk_widget_t *w, char *label_) {
  label_private_data_t *private_data = (label_private_data_t *) w->private_data;
  int label_len;
  
  /* Inform animation thread to not paint the label */
  private_data->on_change = 1;
  
  if (private_data->label) {
    XITK_FREE(private_data->label);
  }
  
  if (private_data->labelpix) {
    xitk_image_destroy_xitk_pixmap(private_data->labelpix);
    private_data->labelpix = NULL;
  }

  label_len = strlen(label_);

  private_data->label = strdup((label_ != NULL) ? label_ : "");

  if(private_data->skin_element_name != NULL)
    _create_label_pixmap(w);
  else
    private_data->anim_offset = 0;

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
  
  private_data->on_change = 0;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  label_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    private_data = (label_private_data_t *) w->private_data;
    
    if(private_data->skin_element_name) {
      
      xitk_skin_lock(skonfig);

      xitk_image_free_image(private_data->imlibdata, &private_data->font);
      private_data->font        = xitk_image_load_image(private_data->imlibdata, 
							xitk_skin_get_label_skinfont_filename(skonfig, private_data->skin_element_name));
      private_data->char_length = (private_data->font->width/32);
      private_data->char_height = (private_data->font->height/3);
      
      private_data->length      = xitk_skin_get_label_length(skonfig, private_data->skin_element_name);
      private_data->animation   = xitk_skin_get_label_animation(skonfig, private_data->skin_element_name);
      
      w->x                      = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      w->y                      = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->width                  = private_data->char_length * private_data->length;
      w->height                 = private_data->char_height;
      w->visible                = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1 : -1;
      w->enable                 = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
      
      xitk_skin_unlock(skonfig);

      if(private_data->skin_element_name != NULL)
	_create_label_pixmap(w);

      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

/*
 *
 */
int xitk_label_change_label(xitk_widget_list_t *wl, xitk_widget_t *w, char *newlabel) {

  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    
    label_setup_label(w, newlabel);
    
    paint_label(w, wl->win, wl->gc);

    return 1;
  }

  return 0;
}

/*
 *
 */
static int notify_click_label(xitk_widget_list_t *wl, xitk_widget_t *w, int bUp, int x, int y) {
  label_private_data_t *private_data;
  
  if (w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    private_data = (label_private_data_t *) w->private_data;
    
    if(private_data->callback) {
      if(bUp)
	private_data->callback(private_data->lWidget, private_data->userdata);
      return 1;
    }

  }
  
  return 0;
}

/*
 *
 */
static xitk_widget_t *_xitk_label_create(xitk_widget_list_t *wl,
					 xitk_skin_config_t *skonfig, xitk_label_widget_t *l,
					 int x, int y, int width, int height,
					 char *skin_element_name, char *fontname,
					 int visible, int enable) {
  xitk_widget_t          *mywidget;
  label_private_data_t   *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  
  private_data = (label_private_data_t *) xitk_xmalloc(sizeof(label_private_data_t));

  private_data->imlibdata      = l->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(l->skin_element_name);
  private_data->callback       = l->callback;
  private_data->userdata       = l->userdata;

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
  private_data->anim_running   = 0;
  private_data->window         = l->window;
  private_data->gc             = l->gc;

  private_data->labelpix       = NULL;

  mywidget->private_data       = private_data;

  mywidget->widget_list        = wl;

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
  mywidget->notify_click       = notify_click_label;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = NULL;
  mywidget->notify_change_skin = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy     = notify_destroy;
  mywidget->get_skin           = get_skin;
  
  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  label_setup_label(mywidget, l->label);

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_label_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_label_widget_t *l) {

  XITK_CHECK_CONSTITENCY(l);
  
  return _xitk_label_create(wl, skonfig, l, 
			    (xitk_skin_get_coord_x(skonfig, l->skin_element_name)),
			    (xitk_skin_get_coord_y(skonfig, l->skin_element_name)),
			    (xitk_skin_get_label_length(skonfig, l->skin_element_name)),
			    -1,
			    l->skin_element_name, 
			    NULL,
			    ((xitk_skin_get_visibility(skonfig, l->skin_element_name)) ? 1 : -1),
			    (xitk_skin_get_enability(skonfig, l->skin_element_name)));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_label_create(xitk_widget_list_t *wl,
					xitk_label_widget_t *l,
					int x, int y, int width, int height, char *fontname) {
  XITK_CHECK_CONSTITENCY(l);

  return _xitk_label_create(wl, NULL, l, x, y, width, height, NULL, fontname, 1, 1);
}
