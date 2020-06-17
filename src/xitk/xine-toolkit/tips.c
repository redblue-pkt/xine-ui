/* 
 * Copyright (C) 2000-2020 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
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
#include <sys/time.h>
#include <errno.h>
#include <X11/Xlib.h>

#include "_xitk.h"

#include "tips.h"

struct xitk_tips_s {
  Display             *display;

  xitk_widget_t       *widget, *new_widget;
  int                  visible;
  int                  running;
  int                  prewait;

  pthread_t            thread;
  pthread_mutex_t      mutex;
  pthread_cond_t       cond;
};

static struct timespec _compute_interval(unsigned int millisecs) {
  struct timespec ts;
#if _POSIX_TIMERS > 0
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t ttimer = (uint64_t)ts.tv_sec*1000 + ts.tv_nsec/1000000 + millisecs;
  ts.tv_sec = ttimer/1000;
  ts.tv_nsec = (ttimer%1000)*1000000;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t ttimer = (uint64_t)tv.tv_sec*1000 + tv.tv_usec/1000 + millisecs;
  ts.tv_sec = ttimer/1000;
  ts.tv_nsec = (ttimer%1000)*1000000;
#endif
  return ts;
}


static __attribute__((noreturn)) void *_tips_loop_thread(void *data) {

  xitk_tips_t *tips = data;

  tips->running = 1;
  pthread_mutex_lock(&tips->mutex);
  
  while (tips->running) {
    struct timespec      ts;

    /* Wait for a new tip to show */
    if (!tips->new_widget) {
      while (tips->running) {
	ts = _compute_interval(1000);
        if (pthread_cond_timedwait(&tips->cond, &tips->mutex, &ts) != ETIMEDOUT)
	  break;
      }
    }

    /* Exit if we're quitting */
    if (!tips->running)
      break;

    /* Start over if nothing */
    if (!tips->new_widget)
      continue;
    
    tips->prewait = 1;

    ts = _compute_interval(500);
    
    pthread_cond_timedwait (&tips->cond, &tips->mutex, &ts);
    
    /* Start over if interrupted */
    if (!tips->prewait)
      continue;

    tips->prewait = 0;

    tips->widget = tips->new_widget;
    tips->new_widget = NULL;
    
    if (tips->widget && (tips->widget->tips_timeout > 0) && tips->widget->tips_string && strlen(tips->widget->tips_string)) {
      xitk_t              *xitk =tips->widget->wl->xitk;
      int                  x, y, w, h;
      xitk_window_t       *xwin;
      xitk_image_t        *image;
      xitk_font_t         *fs;
      unsigned int         cfore, cback;
      struct timespec      ts;

      int                  disp_w = xitk_get_display_width();
      int                  disp_h = xitk_get_display_height();

      int                  x_margin = 12, y_margin = 6;
      int                  bottom_gap = 16; /* To avoid mouse cursor overlaying tips on bottom of widget */

      tips->visible = 1;

      /* Get parent window position */
      xitk_get_window_position(tips->display, tips->widget->wl->win, &x, &y, NULL, NULL);
      
      x += tips->widget->x;
      y += tips->widget->y;
      
      fs = xitk_font_load_font(xitk, DEFAULT_FONT_10);
      xitk_font_set_font(fs, tips->widget->wl->gc);

      xitk_font_unload_font(fs);
      
      cfore = xitk_get_pixel_color_black(tips->widget->wl->imlibdata);
      cback = xitk_get_pixel_color_lightgray(tips->widget->wl->imlibdata);
      
      /* Note: disp_w/3 is max. width, returned image with ALIGN_LEFT will be as small as possible */
      image = xitk_image_create_image_with_colors_from_string(tips->widget->wl->imlibdata, DEFAULT_FONT_10,
							      (disp_w/3), ALIGN_LEFT, 
                                                              tips->widget->tips_string, cfore, cback);
      
      /* Create the tips window, horizontally centered from parent widget */
      /* If necessary, adjust position to display it fully on screen      */
      w = image->width + x_margin;
      h = image->height + y_margin;
      x -= ((w - tips->widget->width) >> 1);
      y += (tips->widget->height + bottom_gap);
      if(x > disp_w - w)
	x = disp_w - w;
      else if(x < 0)
	x = 0;
      if(y > disp_h - h)
	/* 1 px dist to widget prevents odd behavior of mouse pointer when  */
	/* pointer is moved slowly from widget to tips, at least under FVWM */
	/*                                           v                      */
        y -= (tips->widget->height + h + bottom_gap + 1);
      /* No further alternative to y-position the tips (just either below or above widget) */
      xwin = xitk_window_create_simple_window_ext(xitk, x, y, w, h,
                                                  NULL, NULL, NULL, 1, 0, NULL);
      {
	xitk_pixmap_t *bg;
	int            width, height;
	GC             gc;
	
	xitk_window_get_window_size(xwin, &width, &height);
        bg = xitk_image_create_xitk_pixmap(tips->widget->wl->imlibdata, width, height);
	
        XLOCK (xitk_x_lock_display, tips->display);
        gc = XCreateGC(tips->display, tips->widget->wl->imlibdata->x.base_window, None, None);
        XCopyArea(tips->display, (xitk_window_get_background(xwin)), bg->pixmap, gc, 0, 0, width, height, 0, 0);
        XUNLOCK (xitk_x_unlock_display, tips->display);

        XLOCK (xitk_x_lock_display, tips->display);
        XSetForeground(tips->display, gc, cfore);
        XDrawRectangle(tips->display, bg->pixmap, gc, 0, 0, width - 1, height - 1);
        XUNLOCK (xitk_x_unlock_display, tips->display);

        XLOCK (xitk_x_lock_display, tips->display);
        XSetForeground(tips->display, gc, cback);
        XFillRectangle(tips->display, bg->pixmap, gc, 1, 1, width - 2, height - 2);
        XCopyArea(tips->display, image->image->pixmap, bg->pixmap,
		  gc, 0, 0, image->width, image->height, (width - image->width)>>1, (height - image->height)>>1);
        XUNLOCK (xitk_x_unlock_display, tips->display);

        xitk_window_set_background(xwin, bg);

        XLOCK (xitk_x_lock_display, tips->display);
        XFreeGC(tips->display, gc);
        XUNLOCK (xitk_x_unlock_display, tips->display);

        xitk_image_free_image(&image);
      }

      xitk_window_show_window(xwin, 1);

      ts = _compute_interval(tips->widget->tips_timeout);

      pthread_cond_timedwait(&tips->cond, &tips->mutex, &ts);

      xitk_window_destroy_window(xwin);
      
      /* We are flushing here, otherwise tips window will stay displayed */
      XLOCK (xitk_x_lock_display, tips->display);
      XSync(tips->display, False);
      XUNLOCK (xitk_x_unlock_display, tips->display);

      tips->visible = 0;
    }

  }

  pthread_mutex_unlock(&tips->mutex);

  pthread_exit(NULL);
}

/*
 *
 */
xitk_tips_t *xitk_tips_init(Display *disp) {

  xitk_tips_t *tips = xitk_xmalloc(sizeof(*tips));

    pthread_attr_t       pth_attrs;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    struct sched_param   pth_params;
#endif

    tips->visible    = 0;
    tips->display    = disp;
    tips->widget     = NULL;
    tips->new_widget = NULL;
    tips->prewait    = 0;

    pthread_mutex_init(&tips->mutex, NULL);
    pthread_cond_init(&tips->cond, NULL);

    pthread_attr_init(&pth_attrs);

#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
#endif
  
    pthread_create(&tips->thread, &pth_attrs, _tips_loop_thread, tips);

  return tips;
}

/*
 *
 */
void xitk_tips_stop(xitk_tips_t *tips) {

  if (!tips)
    return;

  tips->running = 0;
  
  pthread_mutex_lock(&tips->mutex);
  tips->new_widget = NULL;
  tips->prewait = 0;
  pthread_cond_signal(&tips->cond);
  pthread_mutex_unlock(&tips->mutex);

  /* Wait until tips are hidden to avoid a race condition causing a segfault when */
  /* the parent widget is already destroyed before it's referenced during hiding. */
  while (tips->visible)
    xitk_usec_sleep(5000);

  pthread_join(tips->thread, NULL);
}

void xitk_tips_deinit(xitk_tips_t **ptips) {
  xitk_tips_t *tips = *ptips;

  if (!tips)
    return;

  *ptips = NULL;

  pthread_mutex_destroy(&tips->mutex);
  pthread_cond_destroy(&tips->cond);

  free(tips);
}

static void _tips_update_widget(xitk_tips_t *tips, xitk_widget_t *w)
{
  pthread_mutex_lock(&tips->mutex);
  if (tips->running) {
    tips->new_widget = w;
    tips->prewait = 0;
    pthread_cond_signal(&tips->cond);
  }
  pthread_mutex_unlock(&tips->mutex);
}

/*
 *
 */
void xitk_tips_hide_tips(xitk_tips_t *tips) {

  _tips_update_widget(tips, NULL);

  /* Wait until tips are hidden to avoid a race condition causing a segfault when */
  /* the parent widget is already destroyed before it's referenced during hiding. */
  while (tips->visible)
    xitk_usec_sleep(5000);
}

/*
 *
 */
int xitk_tips_show_widget_tips(xitk_tips_t *tips, xitk_widget_t *w) {

  /* Don't show when window invisible. This call may occur directly after iconifying window. */
  if(!xitk_is_window_visible(w->wl->imlibdata->x.disp, w->wl->win))
    return 0;

  _tips_update_widget(tips, w);

  return 1;
}

/*
 *
 */
void xitk_tips_set_timeout(xitk_widget_t *w, unsigned long timeout) {

  if(w == NULL)
    return;
  
  w->tips_timeout = timeout;
  if(w->type & (WIDGET_GROUP | WIDGET_GROUP_WIDGET)) {
    widget_event_t  event;
    
    event.type         = WIDGET_EVENT_TIPS_TIMEOUT;
    event.tips_timeout = timeout;
    (void) w->event(w, &event, NULL);
  }
}

/*
 *
 */
void xitk_tips_set_tips(xitk_widget_t *w, const char *str) {

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
    else if ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
      xitk_widget_t *widget = xitk_combo_get_label_widget(w);
      
      xitk_tips_set_tips(widget, str);
    }
  }
  
  /* No timeout, set it to default */
  if(!w->tips_timeout)
    xitk_tips_set_timeout(w, xitk_get_tips_timeout());

}

