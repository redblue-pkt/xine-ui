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
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "widget.h"
#include "window.h"
#include "font.h"
#include "image.h"
#include "tips.h"
#include "_xitk.h"

typedef struct {
  pthread_t             thread;
  pthread_mutex_t       mutex;
  xitk_widget_t        *w;
  xitk_widget_list_t   *wl;
  xitk_window_t        *xwin;
  xitk_register_key_t   key;
} tips_private_t;

static tips_private_t *disptips;

/*
 *
 */
static void *xitk_tips_destroy_thread(void *data) {
  tips_private_t *tp = (tips_private_t *)data;
  
  disptips = tp;

  pthread_detach(pthread_self());

  pthread_mutex_init(&tp->mutex, NULL);
  
  /* Waiting enought time to read the tips */
  xitk_usec_sleep(1500000);

  pthread_mutex_lock(&tp->mutex);

  /* Kill tips window */
  xitk_window_destroy_window(tp->w->imlibdata, tp->xwin);

  /* We are flushing here, otherwise tips window will stay displayed */
  XLOCK(tp->w->imlibdata->x.disp);
  XFlush(tp->w->imlibdata->x.disp);
  XUNLOCK(tp->w->imlibdata->x.disp);

  pthread_mutex_unlock(&tp->mutex);
  pthread_mutex_destroy(&tp->mutex);

  XITK_FREE(tp);

  disptips = NULL;

  pthread_exit(NULL);
}

/*
 *
 */
static void *xitk_tips_thread(void *data) {
  tips_private_t     *tp = (tips_private_t *)data;
  int                 x, y, string_length;
  xitk_image_t       *i;
  xitk_font_t        *fs;
  XWindowAttributes   wattr;
  Status              status;
  unsigned int        cyellow, cblack;

  pthread_detach(pthread_self());

  /* Wait timeout */
  xitk_usec_sleep((tp->w->tips_timeout * 1000));

  /* Get parent window position */
  XLOCK(tp->w->imlibdata->x.disp);
  status = XGetWindowAttributes(tp->w->imlibdata->x.disp, tp->wl->win, &wattr);
  XUNLOCK(tp->w->imlibdata->x.disp);
  
  x = tp->w->x;
  y = (tp->w->y + tp->w->height);

  if((status != BadDrawable) && (status != BadWindow)) {
    x += wattr.x;
    y += wattr.y;
  }
  
  fs = xitk_font_load_font(tp->w->imlibdata->x.disp, DEFAULT_FONT_10);
  xitk_font_set_font(fs, tp->wl->gc);
  string_length = xitk_font_get_string_length(fs, tp->w->tips_string);
  xitk_font_unload_font(fs);

  cblack  = xitk_get_pixel_color_black(tp->w->imlibdata);
  cyellow = xitk_get_pixel_color_from_rgb(tp->w->imlibdata, 255, 255, 0);

  i =  xitk_image_create_image_with_colors_from_string(tp->w->imlibdata, DEFAULT_FONT_10,
						       string_length + 1, ALIGN_LEFT, 
						       tp->w->tips_string, cblack, cyellow);
  
  tp->xwin = xitk_window_create_simple_window(tp->w->imlibdata, x, y, 
					      i->width + 10, i->height + 10);
  
  /* Draw the text area */
  {
    Pixmap   bg;
    int      width, height;
    GC       gc;
    
    xitk_window_get_window_size(tp->xwin, &width, &height);
    bg = xitk_image_create_pixmap(tp->w->imlibdata, width, height);
    
    XLOCK(tp->w->imlibdata->x.disp);
    gc = XCreateGC(tp->w->imlibdata->x.disp, tp->w->imlibdata->x.base_window, None, None);
    XCopyArea(tp->w->imlibdata->x.disp, (xitk_window_get_background(tp->xwin)), bg,
	      gc, 0, 0, width, height, 0, 0);
    XSetForeground(tp->w->imlibdata->x.disp, gc, cyellow);
    XFillRectangle(tp->w->imlibdata->x.disp, bg, gc, 1, 1, width - 2, height - 2);
    XCopyArea(tp->w->imlibdata->x.disp, i->image, bg,
	      gc, 0, 0, i->width, i->height, (width - i->width)>>1, ((height - i->height)>>1) + 1);
    XUNLOCK(tp->w->imlibdata->x.disp);
    
    xitk_window_change_background(tp->w->imlibdata, tp->xwin, bg, width, height);
    
    XLOCK(tp->w->imlibdata->x.disp);
    XFreePixmap(tp->w->imlibdata->x.disp, bg);
    XFreeGC(tp->w->imlibdata->x.disp, gc);
    XUNLOCK(tp->w->imlibdata->x.disp);
    
    xitk_image_free_image(tp->w->imlibdata, &i);
    
  }
  
  XLOCK(tp->w->imlibdata->x.disp);
  XMapRaised(tp->w->imlibdata->x.disp, (xitk_window_get_window(tp->xwin)));

  if(wattr.map_state == IsViewable)
    XSetInputFocus(tp->w->imlibdata->x.disp, tp->wl->win, RevertToNone, CurrentTime);
  
  XUNLOCK(tp->w->imlibdata->x.disp);
  
  tp->key = xitk_register_event_handler("xitk tips", 
					(xitk_window_get_window(tp->xwin)),
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);

  /* Create a thread which will destroy the tips window */  
  {
    pthread_attr_t       pth_attrs;
    struct sched_param   pth_params;
    
    pthread_attr_init(&pth_attrs);
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
    
    pthread_create(&tp->thread, &pth_attrs, xitk_tips_destroy_thread, (void *)tp);
  }
  
  pthread_exit(NULL);
}

/*
 *
 */
void xitk_tips_create(xitk_widget_t *w, xitk_widget_list_t *wl) {
  pthread_attr_t       pth_attrs;
  struct sched_param   pth_params;
  tips_private_t      *tp;
  
  if(!w)
    return;
  
  /* If there a current tips displayed, hide it */
  if(disptips != NULL) {

    pthread_mutex_lock(&disptips->mutex);

    pthread_cancel(disptips->thread);
    
    xitk_window_destroy_window(disptips->w->imlibdata, disptips->xwin);
    
    /* We are flushing here, otherwise tips window will stay displayed */
    XLOCK(disptips->w->imlibdata->x.disp);
    XFlush(disptips->w->imlibdata->x.disp);
    XUNLOCK(disptips->w->imlibdata->x.disp);
    
    pthread_mutex_unlock(&disptips->mutex);
    pthread_mutex_destroy(&disptips->mutex);

    XITK_FREE(disptips);
    disptips = NULL;
  }
  
  if((w->tips_string != NULL) && w->tips_timeout) {

    tp     = (tips_private_t *) xitk_xmalloc(sizeof(tips_private_t));
    tp->w  = w;
    tp->wl = wl;
        
    pthread_attr_init(&pth_attrs);
    
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
    
    pthread_create(&w->tips_thread, &pth_attrs, xitk_tips_thread, (void *)tp);
  }
  //  else 
  //    w->tips_thread = 0;
}

/*
 *
 */
void xitk_tips_tips_kill(xitk_widget_t *w) {
  
  if(!w)
    return;

  if(w->tips_thread) {
    pthread_cancel(w->tips_thread);
  }
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

  /* No timeout, set it to default */
  if(!w->tips_timeout)
    xitk_tips_set_timeout(w, TIPS_TIMEOUT);

}
