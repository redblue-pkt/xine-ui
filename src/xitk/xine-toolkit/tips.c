/* 
 * Copyright (C) 2000-2004 the xine project
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
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "_xitk.h"

typedef struct {
  Display             *display;
  pthread_t            thread;

  xitk_widget_t       *widget;
  int                  visible;
  int                  running;

  pthread_mutex_t      mutex;
  
  pthread_mutex_t      new_mutex;
  pthread_cond_t       new_cond;
  
  pthread_mutex_t      timer_mutex;
  pthread_cond_t       timer_cond;

  int                  prewait;
  pthread_mutex_t      prewait_mutex;
  pthread_cond_t       prewait_cond;
} _tips_t;

static _tips_t tips;

static void _tips_handle_event(XEvent *event, void *data) {
  switch(event->type) {
  case MotionNotify:
  case ButtonRelease:
  case ButtonPress:
    xitk_tips_hide_tips();
    break;
  }
}

static void *_tips_loop_thread(void *data) {

  tips.running = 1;
  
  while(tips.running) {
    struct timeval       tv;
    struct timespec      ts;
    int                  result = 1;

    pthread_mutex_lock(&tips.new_mutex);
    pthread_cond_wait(&tips.new_cond, &tips.new_mutex);
    pthread_mutex_unlock(&tips.new_mutex);
    
    pthread_mutex_lock(&tips.prewait_mutex);
    
    tips.prewait = 1;
    
    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = (tv.tv_usec + 300000) * 1000;      
    
    result = pthread_cond_timedwait(&tips.prewait_cond, &tips.prewait_mutex, &ts);
    tips.prewait = 0;
    
    if(!result) {
      tips.widget = NULL;
      pthread_mutex_unlock(&tips.prewait_mutex);
      continue;
    }
    
    pthread_mutex_unlock(&tips.prewait_mutex);

    if(tips.widget && (tips.widget->tips_timeout > 0) && tips.widget->tips_string && strlen(tips.widget->tips_string)) {
      int                  x, y, string_length;
      xitk_window_t       *xwin;
      xitk_register_key_t  key;
      xitk_image_t        *image;
      xitk_font_t         *fs;
      XWindowAttributes    wattr;
      Status               status;
      unsigned int         cwarnfore, cwarnback;
      struct timeval       tv;
      struct timespec      ts;

      tips.visible = 1;

      /* Get parent window position */
      xitk_get_window_position(tips.display, tips.widget->wl->win, &x, &y, NULL, NULL);
      
      x += tips.widget->x;
      y += (tips.widget->y + tips.widget->height);
      
      fs = xitk_font_load_font(tips.display, DEFAULT_FONT_10);
      xitk_font_set_font(fs, tips.widget->wl->gc);

      string_length = MIN((xitk_font_get_string_length(fs, tips.widget->tips_string)), (xitk_get_display_width()/3));

      xitk_font_unload_font(fs);
      
      cwarnfore = xitk_get_pixel_color_warning_foreground(tips.widget->imlibdata);
      cwarnback = xitk_get_pixel_color_warning_background(tips.widget->imlibdata);
      
      image = xitk_image_create_image_with_colors_from_string(tips.widget->imlibdata, DEFAULT_FONT_10,
							      string_length + 1, ALIGN_LEFT, 
							      tips.widget->tips_string, cwarnfore, cwarnback);
      
      /* Create the tips window, horizontaly centered from parent widget */
      xwin = xitk_window_create_simple_window(tips.widget->imlibdata, x - (((image->width + 10) >> 1) 
								      - (tips.widget->width >> 1)), y, 
					      image->width + 10, image->height + 10);
      
      /* WM should ignore tips windows */
      {
	XSetWindowAttributes tp_attr;
	
	tp_attr.override_redirect = True;
	
	XLOCK(tips.display);
	XChangeWindowAttributes(tips.display, (xitk_window_get_window(xwin)), CWOverrideRedirect, &tp_attr);
	XUNLOCK(tips.display);
	
      }
      
      {
	xitk_pixmap_t *bg;
	int            width, height;
	GC             gc;
	
	xitk_window_get_window_size(xwin, &width, &height);
	bg = xitk_image_create_xitk_pixmap(tips.widget->imlibdata, width, height);
	
	XLOCK(tips.display);
	gc = XCreateGC(tips.display, tips.widget->imlibdata->x.base_window, None, None);
	XCopyArea(tips.display, (xitk_window_get_background(xwin)), bg->pixmap, gc, 0, 0, width, height, 0, 0);
	XUNLOCK(tips.display);
	
	XLOCK(tips.display);
	XSetForeground(tips.display, gc, cwarnfore);
	XDrawRectangle(tips.display, bg->pixmap, gc, 0, 0, width - 1, height - 1);
	XUNLOCK(tips.display);
	
	XLOCK(tips.display);
	XSetForeground(tips.display, gc, cwarnback);
	XFillRectangle(tips.display, bg->pixmap, gc, 1, 1, width - 2, height - 2);
	XCopyArea(tips.display, image->image->pixmap, bg->pixmap,
		  gc, 0, 0, image->width, image->height, (width - image->width)>>1, ((height - image->height)>>1) + 1);
	XUNLOCK(tips.display);
	
	xitk_window_change_background(tips.widget->imlibdata, xwin, bg->pixmap, width, height);
	
	xitk_image_destroy_xitk_pixmap(bg);
	
	XLOCK(tips.display);
	XFreeGC(tips.display, gc);
	XUNLOCK(tips.display);
	
	xitk_image_free_image(tips.widget->imlibdata, &image);
      }
      
      XLOCK(tips.display);
      status = XGetWindowAttributes(tips.display, tips.widget->wl->win, &wattr);
      XMapRaised(tips.display, (xitk_window_get_window(xwin)));
      XUNLOCK(tips.display);
      
      if((status != BadDrawable) && (status != BadWindow) && (wattr.map_state == IsViewable)) {
	XLOCK(tips.display);
	XSetInputFocus(tips.display, tips.widget->wl->win, RevertToParent, CurrentTime);
	XUNLOCK(tips.display);
      }
      
      key = xitk_register_event_handler("xitk tips", 
					(xitk_window_get_window(xwin)),
					_tips_handle_event,
					NULL,
					NULL,
					NULL,
					NULL);
            
      gettimeofday(&tv, NULL);
      ts.tv_sec  = tv.tv_sec;
      ts.tv_nsec = (tv.tv_usec + (tips.widget->tips_timeout * 10000)) * 1000;      

      pthread_mutex_lock(&tips.timer_mutex);
      pthread_cond_timedwait(&tips.timer_cond, &tips.timer_mutex, &ts);

      xitk_unregister_event_handler(&key);
      xitk_window_destroy_window(tips.widget->imlibdata, xwin);
      
      /* We are flushing here, otherwise tips window will stay displayed */
      XLOCK(tips.display);
      XSync(tips.display, False);
      XUNLOCK(tips.display);

      tips.widget = NULL;
      
      tips.visible = 0;

      pthread_mutex_unlock(&tips.timer_mutex);
    }

  }

  pthread_exit(NULL);
}

/*
 *
 */
void xitk_tips_init(Display *disp) {
  
  if(!tips.running) {
    pthread_attr_t       pth_attrs;
#if ! defined (__OpenBSD__)
    struct sched_param   pth_params;
#endif

    tips.visible = 0;
    tips.display = disp;
    tips.widget  = NULL;
    tips.prewait = 0;

    pthread_mutex_init(&tips.mutex, NULL);
    pthread_mutex_init(&tips.new_mutex, NULL);
    pthread_mutex_init(&tips.timer_mutex, NULL);
    pthread_mutex_init(&tips.prewait_mutex, NULL);

    pthread_cond_init(&tips.new_cond, NULL);
    pthread_cond_init(&tips.timer_cond, NULL);
    pthread_cond_init(&tips.prewait_cond, NULL);
    
    pthread_attr_init(&pth_attrs);
#if ! defined (__OpenBSD__)
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
#endif
  
    pthread_create(&tips.thread, &pth_attrs, _tips_loop_thread, NULL);
  }
}

/*
 *
 */
void xitk_tips_deinit(void) {
  tips.running = 0;
  
  pthread_mutex_lock(&tips.mutex);
  if(tips.prewait)
    pthread_cond_signal(&tips.prewait_cond);
  else if(tips.visible)
    pthread_cond_signal(&tips.timer_cond);
  else
    pthread_cond_signal(&tips.new_cond);
  
  pthread_mutex_lock(&tips.timer_mutex);

  pthread_join(tips.thread, NULL);

  pthread_mutex_unlock(&tips.mutex);
  pthread_mutex_unlock(&tips.timer_mutex);

  pthread_mutex_destroy(&tips.mutex);
  pthread_mutex_destroy(&tips.new_mutex);
  pthread_mutex_destroy(&tips.timer_mutex);
  pthread_mutex_destroy(&tips.prewait_mutex);
  
  pthread_cond_destroy(&tips.new_cond);
  pthread_cond_destroy(&tips.timer_cond);
  pthread_cond_destroy(&tips.prewait_cond);
}

/*
 *
 */
void xitk_tips_hide_tips(void) {
  pthread_mutex_lock(&tips.mutex);
  if(tips.running) {
    if(tips.widget && tips.visible)
      pthread_cond_signal(&tips.timer_cond);
  }
  pthread_mutex_unlock(&tips.mutex);
}

/*
 *
 */
void xitk_tips_show_widget_tips(xitk_widget_t *w) {
  pthread_mutex_lock(&tips.mutex);
  if(tips.running) {
    
    if(tips.prewait)
      pthread_cond_signal(&tips.prewait_cond);
    else if(tips.widget && tips.visible)
      pthread_cond_signal(&tips.timer_cond);
    
    if((tips.widget != w) && (tips.widget = w))
      pthread_cond_signal(&tips.new_cond);
  }
  pthread_mutex_unlock(&tips.mutex);
}

/*
 *
 */
void xitk_tips_set_timeout(xitk_widget_t *w, unsigned long timeout) {

  if(w == NULL)
    return;
  
  w->tips_timeout = timeout;
}

/*
 *
 */
void xitk_tips_set_tips(xitk_widget_t *w, char *str) {

  if((w == NULL) || (str == NULL))
    return;
  
  XITK_FREE(w->tips_string);
  w->tips_string = strdup(str);

  /* Special GROUP widget case */
  if(w->type & WIDGET_GROUP) {
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX) {
      xitk_widget_t *widget = xitk_intbox_get_input_widget(w);

      xitk_tips_set_tips(widget, str);
    }
    else if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_DOUBLEBOX) {
      xitk_widget_t *widget = xitk_doublebox_get_input_widget(w);
      
      xitk_tips_set_tips(widget, str);
    }
    else if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) {
      xitk_widget_t *widget = xitk_combo_get_label_widget(w);
      
      xitk_tips_set_tips(widget, str);
    }
  }
  
  /* No timeout, set it to default */
  if(!w->tips_timeout)
    xitk_tips_set_timeout(w, TIPS_TIMEOUT);

}
