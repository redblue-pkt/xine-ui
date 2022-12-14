/*
 * Copyright (C) 2000-2021 the xine project
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

#include "_xitk.h"

#include "tips.h"
#include "default_font.h"

typedef enum {
  TIPS_IDLE = 0,
  TIPS_DELAY,
  TIPS_SHOW,
  TIPS_WAIT,
  TIPS_HIDE,
  TIPS_QUIT,
  TIPS_LAST
} _tips_state_t;

static const _tips_state_t _tips_next[TIPS_LAST] = {
  [TIPS_IDLE] = TIPS_IDLE,
  [TIPS_DELAY] = TIPS_SHOW,
  [TIPS_SHOW] = TIPS_WAIT,
  [TIPS_WAIT] = TIPS_HIDE,
  [TIPS_HIDE] = TIPS_IDLE,
  [TIPS_QUIT] = TIPS_QUIT
};

static const int _tips_wait[TIPS_LAST] = {
  [TIPS_IDLE] = 8000,
  [TIPS_DELAY] = 500,
  [TIPS_SHOW] = 0,
  [TIPS_WAIT] = -1,
  [TIPS_HIDE] = 0,
  [TIPS_QUIT] = 0
};

struct xitk_tips_s {
  xitk_widget_t       *widget, *new_widget;
  _tips_state_t        state;
  int                  num_waiters;

  pthread_t            thread;
  pthread_mutex_t      mutex;
  pthread_cond_t       in, out;
};

static void _compute_interval (struct timespec *ts, unsigned int millisecs) {
#if _POSIX_TIMERS > 0
  clock_gettime (CLOCK_REALTIME, ts);
#else
  struct timeval tv;
  gettimeofday (&tv, NULL);
  ts->tv_sec = tv.tv_sec;
  ts->tv_nsec = tv.tv_usec * 1000;
#endif
  ts->tv_sec += millisecs / 1000;
  ts->tv_nsec += (millisecs % 1000) * 1000000;
  if (ts->tv_nsec >= 1000000000) {
    ts->tv_nsec -= 1000000000;
    ts->tv_sec += 1;
  }
}

static void *_tips_loop_thread (void *data) {
  xitk_tips_t *tips = data;
  xitk_t *xitk = NULL;
  xitk_image_t *image = NULL;
  xitk_window_t *xwin = NULL;
  _tips_state_t state = TIPS_IDLE;
  unsigned int timeout = 0;

  pthread_mutex_lock (&tips->mutex);

  while (state != TIPS_QUIT) {
    int wait;

    tips->state = state;
    wait = _tips_wait[state];
    if (wait < 0)
      wait = tips->widget ? timeout : 0;
    if (wait > 0) {
      struct timespec ts;
      _compute_interval (&ts, wait);
      pthread_cond_timedwait (&tips->in, &tips->mutex, &ts);
    }
    if (tips->num_waiters > 0) {
      tips->num_waiters &= 0xffff;
      state = (state == TIPS_WAIT) || (state == TIPS_HIDE) ? TIPS_HIDE : TIPS_IDLE;
    }

    switch (state) {
      case TIPS_SHOW:
        if (tips->widget && tips->widget->tips_timeout && tips->widget->tips_string && tips->widget->tips_string[0]) {
          xitk_rect_t r = {0, 0, 0, 0};
          unsigned int cfore, cback;
          int disp_w, disp_h;
          int x_margin = 12, y_margin = 6;
          int bottom_gap = 16; /* To avoid mouse cursor overlaying tips on bottom of widget */

          xitk = tips->widget->wl->xitk;
          xitk_get_display_size (xitk, &disp_w, &disp_h);

          /* Get parent window position */
          xitk_window_get_window_position (tips->widget->wl->xwin, &r);

          r.x += tips->widget->x;
          r.y += tips->widget->y;

          cfore = xitk_get_cfg_num (xitk, XITK_BLACK_COLOR);
          cback = xitk_get_cfg_num (xitk, XITK_FOCUS_COLOR);

          /* Note: disp_w/3 is max. width, returned image with ALIGN_LEFT will be as small as possible */
          image = xitk_image_create_image_with_colors_from_string (xitk,
            DEFAULT_FONT_10, disp_w / 3, x_margin >> 1, y_margin >> 1, ALIGN_LEFT, tips->widget->tips_string, cfore, cback);
          if (!image) {
            state = TIPS_IDLE;
            tips->widget = tips->new_widget = NULL;
            break;
          }
          r.width = image->width;
          r.height = image->height;

          /* Tips may be extensive help texts that user wants more time to read.
           * We used to implement this by per widget timeout values, but this
           * was never really used like that (only as an enable flag).
           * Instead, take the xitk value as basis for small texts, and hold
           * larger area tips up to 8 times longer. This also saves us the need
           * to broadcast config changes to all widgets. This should be fine since
           * tips are killed when the widget goes away or loses focus. */
          if (tips->widget->tips_timeout == XITK_TIPS_TIMEOUT_AUTO) {
            timeout = xitk->tips_timeout * (r.height - 8) / 11;
            if (timeout < xitk->tips_timeout)
              timeout = xitk->tips_timeout;
            else if (timeout > xitk->tips_timeout * 8)
              timeout = xitk->tips_timeout * 8;
          } else {
            timeout = tips->widget->tips_timeout;
          }

          /* Create the tips window, horizontally centered from parent widget */
          /* If necessary, adjust position to display it fully on screen      */
          xitk_image_draw_rectangle (image, 0, 0, r.width, r.height, cfore);
          r.x -= (r.width - tips->widget->width) >> 1;
          r.y += tips->widget->height + bottom_gap;
          if (r.x > disp_w - r.width)
            r.x = disp_w - r.width;
          else if (r.x < 0)
            r.x = 0;
          if (r.y > disp_h - r.height)
            /* 1 px dist to widget prevents odd behavior of mouse pointer when  */
            /* pointer is moved slowly from widget to tips, at least under FVWM */
            /*                                           v                      */
            r.y -= tips->widget->height + r.height + bottom_gap + 1;
          /* No further alternative to y-position the tips (just either below or above widget) */
          xwin = xitk_window_create_window_ext (xitk, r.x, r.y, r.width, r.height, "tips", NULL, NULL, 1, 0, NULL, image);
          if (!xwin) {
            xitk_image_free_image (&image);
            state = TIPS_IDLE;
            tips->widget = tips->new_widget = NULL;
            break;
          }
          pthread_mutex_unlock (&tips->mutex);
          xitk_window_set_role (xwin, XITK_WR_SUBMENU);
          xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
          pthread_mutex_lock (&tips->mutex);
          state = TIPS_WAIT;
        } else {
          state = TIPS_IDLE;
          tips->widget = tips->new_widget = NULL;
        }
        break;

      case TIPS_HIDE:
        if (xwin) {
          xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
          xitk_window_destroy_window (xwin);
          xwin = NULL;
          xitk_image_free_image (&image);
        }
        state = TIPS_IDLE;
        /* fall through */
      case TIPS_IDLE:
        tips->widget = NULL;
        if (tips->new_widget == (xitk_widget_t *)-1) {
          state = TIPS_QUIT;
        } else if (tips->new_widget) {
          state = TIPS_DELAY;
          tips->widget = tips->new_widget;
          tips->new_widget = NULL;
        }
        break;

      default:
        state = _tips_next[state];
    }
    if (tips->num_waiters > 0)
      pthread_cond_broadcast (&tips->out);
  }

  tips->state = TIPS_QUIT;
  tips->widget = tips->new_widget = NULL;
  pthread_mutex_unlock (&tips->mutex);

  return NULL;
}

/*
 *
 */
xitk_tips_t *xitk_tips_new (void) {
  xitk_tips_t *tips;

  tips = xitk_xmalloc (sizeof (*tips));
  if (!tips)
    return NULL;

  tips->state      = TIPS_IDLE;
  tips->num_waiters = 0;
  tips->widget     = NULL;
  tips->new_widget = NULL;

  pthread_mutex_init (&tips->mutex, NULL);
  pthread_cond_init (&tips->in, NULL);
  pthread_cond_init (&tips->out, NULL);

  {
    pthread_attr_t       pth_attrs;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    struct sched_param   pth_params;
#endif
    int r;
    pthread_attr_init (&pth_attrs);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
    pthread_attr_getschedparam (&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min (SCHED_OTHER);
    pthread_attr_setschedparam (&pth_attrs, &pth_params);
#endif
    r = pthread_create (&tips->thread, &pth_attrs, _tips_loop_thread, tips);
    pthread_attr_destroy (&pth_attrs);
    if (!r)
      return tips;
  }

  pthread_cond_destroy (&tips->out);
  pthread_cond_destroy (&tips->in);
  pthread_mutex_destroy (&tips->mutex);
  free (tips);
  return NULL;
}

void xitk_tips_delete (xitk_tips_t **ptips) {
  xitk_tips_t *tips = *ptips;

  if (!tips)
    return;

  *ptips = NULL;

  pthread_mutex_lock (&tips->mutex);
  tips->num_waiters += 0x10000;
  tips->new_widget = (xitk_widget_t *)-1;
  pthread_cond_signal (&tips->in);
  pthread_mutex_unlock (&tips->mutex);

  pthread_join (tips->thread, NULL);

  pthread_cond_destroy (&tips->out);
  pthread_cond_destroy (&tips->in);
  pthread_mutex_destroy (&tips->mutex);

  free (tips);
}

int xitk_tips_show_widget_tips (xitk_tips_t *tips, xitk_widget_t *w) {
  xitk_widget_t *v = NULL;

  if (!tips)
    return 1;
  /* Don't show when window invisible. This call may occur directly after iconifying window. */
  if (w && !(xitk_window_flags (w->wl->xwin, 0, 0) & XITK_WINF_VISIBLE))
    return 0;

  do {
    if (!w)
      break;
    if (!w->tips_timeout || !w->tips_string)
      break;
    if (!w->tips_string[0])
      break;
    v = w;
  } while (0);
  pthread_mutex_lock (&tips->mutex);
  if ((tips->state != TIPS_QUIT) && (v != tips->widget)) {
    tips->new_widget = v;
    tips->num_waiters += 0x10001;
    pthread_cond_signal (&tips->in);
    /* Wait until tips are hidden to avoid a race condition causing a segfault when */
    /* the parent widget is already destroyed before it's referenced during hiding. */
    while (tips->state == TIPS_WAIT)
      pthread_cond_wait (&tips->out, &tips->mutex);
    tips->num_waiters -= 1;
  }
  pthread_mutex_unlock (&tips->mutex);
  return 1;
}

