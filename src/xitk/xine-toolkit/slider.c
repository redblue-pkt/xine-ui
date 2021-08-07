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
#include <math.h>

#include "_xitk.h"
#include "slider.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct _slider_private_s {
  xitk_widget_t           w;

  xitk_short_string_t     skin_element_name;

  void                    (*paint) (struct _slider_private_s *wp, widget_event_t *event);
  int                     sType;
  int                     bClicked;
  int                     focus;

  float                   angle;
  float                   percentage;

  float                   upper;
  float                   value;
  float                   lower;
  int                     step;

  int                     radius;

  xitk_part_image_t       paddle_skin;
  xitk_part_image_t       bg_skin;

  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;

  xitk_state_callback_t   callback;
  void                   *userdata;

  int                     button_width;
  float                   ratio;

  int                     bar_mode; /** << 1 (bar |####---|), 0 (pointer |---#----|) */

  xitk_slider_hv_t        hv_info;
  int                     hv_w, hv_h;
  int                     hv_x, hv_y, hv_max_x, hv_max_y;

  struct {
    int                   fx, fy, x, y, w, h;
  }                       paddle_drawn_here;
} _slider_private_t;

static void _xitk_slider_update_skin (_slider_private_t *wp) {
  wp->hv_max_x = wp->w.width - wp->hv_w;
  if (wp->hv_max_x < 0)
    wp->hv_max_x = 0;
  if (wp->hv_x > wp->hv_max_x)
    wp->hv_x = wp->hv_max_x;
  wp->hv_max_y = wp->w.height - wp->hv_h;
  if (wp->hv_max_y < 0)
    wp->hv_max_y = 0;
  if (wp->hv_y > wp->hv_max_y)
    wp->hv_y = wp->hv_max_y;
}

static int _xitk_slider_report_xy (_slider_private_t *wp, int x, int y) {
  x = wp->hv_max_x > 0
    ? ((x - wp->w.x - (wp->hv_w >> 1)) * (wp->hv_info.h.max - wp->hv_info.h.visible) + (wp->hv_max_x >> 1)) / wp->hv_max_x
    : 0;
  if (x > wp->hv_info.h.max - wp->hv_info.h.visible)
    x = wp->hv_info.h.max - wp->hv_info.h.visible;
  if (x < 0)
    x = 0;
  y = wp->hv_max_y > 0
    ? ((y - wp->w.y - (wp->hv_h >> 1)) * (wp->hv_info.v.max - wp->hv_info.v.visible) + (wp->hv_max_y >> 1)) / wp->hv_max_y
    : 0;
  if (y > wp->hv_info.v.max - wp->hv_info.v.visible)
    y = wp->hv_info.v.max - wp->hv_info.v.visible;
  if (y < 0)
    y = 0;
  if ((x != wp->hv_info.h.pos) || (y != wp->hv_info.v.pos)) {
    wp->hv_info.h.pos = x;
    wp->hv_info.v.pos = y;
    return 1;
  }
  return 0;
}

/*
 *
 */
static int _xitk_slider_update_value (_slider_private_t *wp, float value) {
  float range;

  if (value < wp->lower)
    value = wp->lower;
  else if (value > wp->upper)
    value = wp->upper;
  if (value == wp->value)
    return 0;

  wp->value = value;
  range = wp->upper - wp->lower;
  wp->percentage = (wp->value - wp->lower) / range;
  wp->angle = 7.0 * M_PI / 6.0 - (wp->value - wp->lower) * 4.0 * M_PI / 3.0 / range;
  return 1;
}

/*
 *
 */
static void _xitk_slider_update_minmax (_slider_private_t *wp, float min, float max) {
  if(min > max) {
    XITK_WARNING("%s@%d: slider min value > max value !\n", __FILE__, __LINE__);
    return;
  }

  wp->upper = (min == max) ? max + 1 : max;
  wp->lower = min;
  _xitk_slider_update_value (wp, wp->value);
}

/*
 *
 */
static int _xitk_slider_update_xy (_slider_private_t *wp, int x, int y) {
  float value;

  if (wp->sType == XITK_RSLIDER) {

    int xc = wp->bg_skin.width >> 1, yc = wp->bg_skin.height >> 1;
    double angle = atan2 (yc - y, x - xc);

    if (angle < -M_PI / 2.)
      angle += 2 * M_PI;
    if (angle < -M_PI / 6)
      angle = -M_PI / 6;
    if (angle > 7. * M_PI / 6.)
      angle = 7. * M_PI / 6.;
    value = (7. * M_PI / 6 - angle) / (4. * M_PI / 3.);

  } else if (wp->sType == XITK_HSLIDER) {

    int width = wp->bg_skin.width;

    if (!wp->bar_mode) {
      x -= wp->button_width >> 1;
      width -= wp->button_width;
    }
    value = (float)x / (float)width;

  } else { /* wp->sType == XITK_VSLIDER */

    int height = wp->bg_skin.height;

    if (!wp->bar_mode) {
      y -= wp->paddle_skin.height >> 1;
      height -= wp->paddle_skin.height;
    }
    value = (float)(height - y) / (float)height;

  }

  return _xitk_slider_update_value (wp, wp->lower + (value * (wp->upper - wp->lower)));
}

/*
 *
 */
static void _xitk_slider_destroy (_slider_private_t *wp) {
  if (!wp->skin_element_name.s) {
    xitk_image_free_image (&(wp->paddle_skin.image));
    xitk_image_free_image (&(wp->bg_skin.image));
  }
  xitk_short_string_deinit (&wp->skin_element_name);
}

/*
 *
 */
static xitk_image_t *_xitk_slider_get_skin (_slider_private_t *wp, int sk) {
  if (sk == FOREGROUND_SKIN)
    return wp->paddle_skin.image;
  if (sk == BACKGROUND_SKIN)
    return wp->bg_skin.image;
  return NULL;
}

/*
 *
 */
static int _xitk_slider_inside (_slider_private_t *wp, int x, int y) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    if (wp->bar_mode == 1) {
      return xitk_image_inside (wp->paddle_skin.image, wp->paddle_skin.x + x - wp->w.x, wp->paddle_skin.y + y - wp->w.y);
    } else {
      return xitk_image_inside (wp->bg_skin.image, wp->bg_skin.x + x - wp->w.x, wp->bg_skin.y + y - wp->w.y);
    }
  }
  return 0;
}

/*
 * Draw widget
 */
static void _xitk_slider_paint_n (_slider_private_t *wp, widget_event_t *event) {
  (void)wp;
  (void)event;
}

static void _xitk_slider_paint_p (_slider_private_t *wp, widget_event_t *event) {
  int destx = wp->w.x + wp->paddle_drawn_here.x;
  int desty = wp->w.y + wp->paddle_drawn_here.y;

  if (event) {
    int x1 = xitk_max (destx, event->x);
    int x2 = xitk_min (destx + wp->paddle_drawn_here.w, event->x + event->width);
    int y1 = xitk_max (desty, event->y);
    int y2 = xitk_min (desty + wp->paddle_drawn_here.h, event->y + event->height);

    if ((x1 < x2) && (y1 < y2))
      xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
        wp->paddle_drawn_here.fx + x1 - destx, wp->paddle_drawn_here.fy + y1 - desty,
        x2 - x1, y2 - y1, x1, y1);
  } else {
    xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
      wp->paddle_drawn_here.fx, wp->paddle_drawn_here.fy,
      wp->paddle_drawn_here.w, wp->paddle_drawn_here.h, destx, desty);
  }
}

static void _xitk_slider_paint_hv (_slider_private_t *wp, widget_event_t *event) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    int    paddle_x, paddle_width;

    paddle_width = wp->hv_w;
    paddle_x = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
             ? (wp->bClicked ? 2 : 1)
             : 0;
    paddle_x *= paddle_width;

    if (!event) {
      if (wp->paddle_drawn_here.w > 0) {
        /* incremental */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
          wp->paddle_drawn_here.x, wp->paddle_drawn_here.y,
          wp->paddle_drawn_here.w, wp->paddle_drawn_here.h,
          wp->w.x + wp->paddle_drawn_here.x, wp->w.y + wp->paddle_drawn_here.y);
      } else {
        /* full */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL, 0, 0, wp->bg_skin.width, wp->bg_skin.height, wp->w.x, wp->w.y);
      }
    } else {
      /* partial */
      xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y);
    }

    if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
      {
        int x, y;
        x = wp->hv_info.h.max - wp->hv_info.h.visible;
        if (x > 0) {
          x = wp->hv_info.h.pos * (wp->w.width - wp->hv_w) / x;
          if (x > wp->hv_max_x)
            x = wp->hv_max_x;
          if (x < 0)
            x = 0;
        } else {
          x = 0;
        }
        y = wp->hv_info.v.max - wp->hv_info.v.visible;
        if (y > 0) {
          y = wp->hv_info.v.pos * (wp->w.height - wp->hv_h) / y;
          if (y > wp->hv_max_y)
            y = wp->hv_max_y;
          if (y < 0)
            y = 0;
        } else {
          y = 0;
        }
        wp->hv_x = x;
        wp->hv_y = y;
        wp->paddle_drawn_here.fx = paddle_x;
        wp->paddle_drawn_here.fy = 0;
        wp->paddle_drawn_here.w = wp->hv_w;
        wp->paddle_drawn_here.h = wp->hv_h;
        wp->paddle_drawn_here.x = x;
        wp->paddle_drawn_here.y = y;
      }

      _xitk_slider_paint_p (wp, event);
    }
  } else {
    /* no paddle just background when disabled. */
    wp->paddle_drawn_here.fx = -1;
    wp->paddle_drawn_here.x = 0;
    wp->paddle_drawn_here.y = 0;
    wp->paddle_drawn_here.w = 0;
    wp->paddle_drawn_here.h = 0;
  }
}

static void _xitk_slider_paint_r (_slider_private_t *wp, widget_event_t *event) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    int    paddle_x, paddle_width, paddle_height;

    paddle_width = wp->button_width;
    paddle_height = wp->paddle_skin.height;
    paddle_x = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
             ? (wp->bClicked ? 2 : 1)
             : 0;
    paddle_x *= paddle_width;

    if (!event) {
      if (wp->paddle_drawn_here.w > 0) {
        /* incremental */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
          wp->paddle_drawn_here.x, wp->paddle_drawn_here.y,
          wp->paddle_drawn_here.w, wp->paddle_drawn_here.h,
          wp->w.x + wp->paddle_drawn_here.x, wp->w.y + wp->paddle_drawn_here.y);
      } else {
        /* full */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL, 0, 0, wp->bg_skin.width, wp->bg_skin.height, wp->w.x, wp->w.y);
      }
    } else {
      /* partial */
      xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y);
    }

    if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
      int xcenter = wp->bg_skin.width >> 1;
      int ycenter = wp->bg_skin.height >> 1;
      double angle = wp->angle;
      if (angle < M_PI / -2)
        angle = angle + M_PI * 2;
      wp->paddle_drawn_here.x = (int) (0.5 + xcenter + wp->radius * cos (angle)) - (paddle_width / 2);
      wp->paddle_drawn_here.y = (int) (0.5 + ycenter - wp->radius * sin (angle)) - (paddle_height / 2);
      wp->paddle_drawn_here.fx = paddle_x;
      wp->paddle_drawn_here.fy = 0;
      wp->paddle_drawn_here.w = paddle_width;
      wp->paddle_drawn_here.h = paddle_height;
      _xitk_slider_paint_p (wp, event);
    }
  } else {
    /* no paddle just background when disabled. */
    wp->paddle_drawn_here.fx = -1;
    wp->paddle_drawn_here.x = 0;
    wp->paddle_drawn_here.y = 0;
    wp->paddle_drawn_here.w = 0;
    wp->paddle_drawn_here.h = 0;
  }
}

static void _xitk_slider_paint_h (_slider_private_t *wp, widget_event_t *event) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    int    paddle_x, paddle_width, paddle_height;

    paddle_width = wp->button_width;
    paddle_height = wp->paddle_skin.height;
    paddle_x = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
             ? (wp->bClicked ? 2 : 1)
             : 0;
    paddle_x *= paddle_width;

    if (!event) {
      if (wp->paddle_drawn_here.w > 0) {
        /* incremental */
        if (wp->bar_mode && (wp->paddle_drawn_here.fx == paddle_x)) {
          /* nasty little shortcut ;-) */
          int oldpos = wp->paddle_drawn_here.w;
          int newpos = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.width);

          if (newpos < oldpos) {
            xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
              newpos, 0, oldpos - newpos, wp->bg_skin.height, wp->w.x + newpos, wp->w.y);
          } else if (newpos > oldpos) {
            xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
              paddle_x + oldpos, 0, newpos - oldpos, paddle_height, wp->w.x + oldpos, wp->w.y);
          }
          wp->paddle_drawn_here.w = newpos;
          return;
        } else {
          xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
            wp->paddle_drawn_here.x, wp->paddle_drawn_here.y,
            wp->paddle_drawn_here.w, wp->paddle_drawn_here.h,
            wp->w.x + wp->paddle_drawn_here.x, wp->w.y + wp->paddle_drawn_here.y);
        }
      } else {
        /* full */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL, 0, 0, wp->bg_skin.width, wp->bg_skin.height, wp->w.x, wp->w.y);
      }
    } else {
      /* partial */
      xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y);
    }

    if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
      if (wp->bar_mode == 1) {
        wp->paddle_drawn_here.w  = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.width);
        wp->paddle_drawn_here.x = 0;
      } else {
        wp->paddle_drawn_here.x = ((wp->w.width - paddle_width) * .01) * (wp->percentage * 100.0) + 0.5;
        wp->paddle_drawn_here.w  = paddle_width;
      }
      wp->paddle_drawn_here.fx = paddle_x;
      wp->paddle_drawn_here.fy = 0;
      wp->paddle_drawn_here.h = paddle_height;
      wp->paddle_drawn_here.y = 0;
      _xitk_slider_paint_p (wp, event);
    }
  } else {
    /* no paddle just background when disabled. */
    wp->paddle_drawn_here.fx = -1;
    wp->paddle_drawn_here.x = 0;
    wp->paddle_drawn_here.y = 0;
    wp->paddle_drawn_here.w = 0;
    wp->paddle_drawn_here.h = 0;
  }
}

static void _xitk_slider_paint_v (_slider_private_t *wp, widget_event_t *event) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    int    paddle_x, paddle_width, paddle_height;

    paddle_width = wp->button_width;
    paddle_height = wp->paddle_skin.height;
    paddle_x = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
             ? (wp->bClicked ? 2 : 1)
             : 0;
    paddle_x *= paddle_width;

    if (!event) {
      if (wp->paddle_drawn_here.w > 0) {
        /* incremental */
        if (wp->bar_mode && (wp->paddle_drawn_here.fx == paddle_x)) {
          /* nasty little shortcut ;-) */
          int oldpos = wp->paddle_drawn_here.y;
          int newpos = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.height);

          newpos = wp->bg_skin.height - newpos;
          if (newpos > oldpos) {
            xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
              0, oldpos, wp->bg_skin.width, newpos - oldpos, wp->w.x, wp->w.y + oldpos);
          } else if (newpos < oldpos) {
            xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
              paddle_x, newpos, paddle_width, oldpos - newpos, wp->w.x, wp->w.y + newpos);
          }
          wp->paddle_drawn_here.y = newpos;
          wp->paddle_drawn_here.h = paddle_height - newpos;
          return;
        } else {
          xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
            wp->paddle_drawn_here.x, wp->paddle_drawn_here.y,
            wp->paddle_drawn_here.w, wp->paddle_drawn_here.h,
            wp->w.x + wp->paddle_drawn_here.x, wp->w.y + wp->paddle_drawn_here.y);
        }
      } else {
        /* full */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL, 0, 0, wp->bg_skin.width, wp->bg_skin.height, wp->w.x, wp->w.y);
      }
    } else {
      /* partial */
      xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y);
    }

    if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
      if (wp->bar_mode == 1) {
        wp->paddle_drawn_here.h  = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.height);
        wp->paddle_drawn_here.y =
        wp->paddle_drawn_here.fy = paddle_height - wp->paddle_drawn_here.h;
      } else {
        wp->paddle_drawn_here.y = wp->bg_skin.height - paddle_height
          - ((wp->w.height - paddle_height) * .01) * (wp->percentage * 100.0) + 0.5;
        wp->paddle_drawn_here.fy = 0;
        wp->paddle_drawn_here.h = paddle_height;
      }
      wp->paddle_drawn_here.fx = paddle_x;
      wp->paddle_drawn_here.w = paddle_width;
      wp->paddle_drawn_here.x = 0;
      _xitk_slider_paint_p (wp, event);
    }
  } else {
    /* no paddle just background when disabled. */
    wp->paddle_drawn_here.fx = -1;
    wp->paddle_drawn_here.x = 0;
    wp->paddle_drawn_here.y = 0;
    wp->paddle_drawn_here.w = 0;
    wp->paddle_drawn_here.h = 0;
  }
}

static void _xitk_slider_set_paint (_slider_private_t *wp) {
  if (!wp->bg_skin.width || !wp->paddle_skin.image || !wp->bg_skin.image)
    wp->paint = _xitk_slider_paint_n;
  else if (wp->sType == XITK_HVSLIDER)
    wp->paint = _xitk_slider_paint_hv;
  else if (wp->sType == XITK_RSLIDER)
    wp->paint = _xitk_slider_paint_r;
  else if (wp->sType == XITK_HSLIDER)
    wp->paint = _xitk_slider_paint_h;
  else if (wp->sType == XITK_VSLIDER)
    wp->paint = _xitk_slider_paint_v;
  else
    wp->paint = _xitk_slider_paint_n;
}

static void _xitk_slider_set_skin (_slider_private_t *wp, xitk_skin_config_t *skonfig) {
  const xitk_skin_element_info_t *s = xitk_skin_get_info (skonfig, wp->skin_element_name.s);
  /* always be there for the application. if this skin does not use us, disable user side. */
  wp->paddle_drawn_here.fx = -1;
  if (s && s->pixmap_img.image && s->slider_pixmap_pad_img.image) {
    wp->w.x         = s->x;
    wp->w.y         = s->y;
    xitk_widget_state_from_info (&wp->w, s);
    wp->sType       = s->slider_type;
    wp->radius      = s->slider_radius;
    wp->bg_skin     = s->pixmap_img;
    wp->paddle_skin = s->slider_pixmap_pad_img;
  } else {
    wp->w.x         = 0;
    wp->w.y         = 0;
    wp->w.state    &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    wp->sType       = XITK_VSLIDER;
    wp->radius      = 8;
    wp->bg_skin.x      = 0;
    wp->bg_skin.y      = 0;
    wp->bg_skin.width  = 0;
    wp->bg_skin.height = 0;
    wp->bg_skin.image  = NULL;
    wp->paddle_skin.x      = 0;
    wp->paddle_skin.y      = 0;
    wp->paddle_skin.width  = 0;
    wp->paddle_skin.height = 0;
    wp->paddle_skin.image  = NULL;
  }
  _xitk_slider_set_paint (wp);
}

/*
 *
 */
static void _xitk_slider_new_skin (_slider_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name.s) {
    xitk_skin_lock (skonfig);
    _xitk_slider_set_skin (wp, skonfig);
    xitk_skin_unlock (skonfig);
    wp->hv_w = wp->paddle_skin.width / 3;
    wp->hv_h = wp->paddle_skin.height;
    _xitk_slider_update_skin (wp);
    wp->button_width = wp->paddle_skin.width / 3;
    wp->bar_mode = 0;
    if (wp->sType == XITK_HSLIDER) {
      if (wp->button_width == wp->bg_skin.width)
        wp->bar_mode = 1;
    } else if (wp->sType == XITK_VSLIDER) {
      if (wp->paddle_skin.height == wp->bg_skin.height)
        wp->bar_mode = 1;
    }
    wp->w.width   = wp->bg_skin.width;
    wp->w.height  = wp->bg_skin.height;
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}

/*
 * Got click
 */
static int _xitk_slider_click (_slider_private_t *wp, int button, int bUp, int x, int y) {
  if (button == 1) {
    if (wp->focus == FOCUS_RECEIVED) {
      int moved, value;

      if (wp->sType == XITK_HVSLIDER) {
        /* if this thing really moves x and y, use xitk_slider_hv_sync (). */
        moved = _xitk_slider_report_xy (wp, x, y);
        value = wp->hv_max_y > 0 ? (int)wp->upper - wp->hv_info.v.pos : wp->hv_info.h.pos;
      } else {
        moved = _xitk_slider_update_xy (wp, (x - wp->w.x), (y - wp->w.y));
        value = wp->value;
      }

      if (bUp == 0) {
        if (moved || !wp->bClicked) {
          wp->bClicked = 1;
          wp->paint (wp, NULL);
        }
        /* Exec motion callback function (if available) */
        if (moved && wp->motion_callback)
          wp->motion_callback (&wp->w, wp->motion_userdata, value);
      } else if (bUp == 1) {
        if (moved || wp->bClicked) {
          wp->bClicked = 0;
          wp->paint (wp, NULL);
        }
        if (wp->callback)
          wp->callback (&wp->w, wp->userdata, value);
      }
    }
    return 1;
  }
  return 0;
}

/*
 * Got focus
 */
static int _xitk_slider_focus (_slider_private_t *wp, int focus) {
  if (!wp->bClicked)
    wp->focus = focus;
  return 1;
}

static int _xitk_slider_key (_slider_private_t *wp, const char *string, int modifier) {
  int v, dx, dy;

  if (!(wp->w.state & XITK_WIDGET_STATE_ENABLE) || !string)
    return 0;

  if (string[0] != XITK_CTRL_KEY_PREFIX)
    return 0;
  if ((modifier & ~MODIFIER_NUML) != MODIFIER_NOMOD)
    return 0;

  dx = dy = 0;
  switch (string[1]) {
    case XITK_KEY_LEFT: dx = -1; break;
    case XITK_KEY_RIGHT: dx = 1; break;
    case XITK_KEY_HOME: dx = dy = -3; break;
    case XITK_KEY_END: dx = dy = 3; break;
    case XITK_KEY_PREV: dx = dy = -2; break;
    case XITK_KEY_NEXT: dx = dy = 2; break;
    case XITK_KEY_UP: dy = -1; break;
    case XITK_KEY_DOWN: dy = 1; break;
    case XITK_MOUSE_WHEEL_UP: dx = dy = -1; break;
    case XITK_MOUSE_WHEEL_DOWN: dx = dy = 1; break;
    default: return 0;
  }

  v = 0;
  if (wp->sType == XITK_HSLIDER)
    v = dx;
  else if (wp->sType == XITK_VSLIDER)
    v = -dy;
  else if (wp->sType == XITK_RSLIDER)
    v = dy ? -dy : dx;

  if (v) {
    int nv;
    if (v == -3) {
      nv = xitk_slider_get_min (&wp->w);
    } else if (v == 3) {
      nv = xitk_slider_get_max (&wp->w);
    } else if (v < 0) {
      int m = xitk_slider_get_min (&wp->w);
      v = xitk_slider_get_pos (&wp->w);
      nv = v - wp->step;
      if (nv < m)
        nv = m;
    } else {
      int m = xitk_slider_get_max (&wp->w);
      v = xitk_slider_get_pos (&wp->w);
      nv = v + wp->step;
      if (nv > m)
        nv = m;
    }
    if (nv != v) {
      xitk_slider_set_pos (&wp->w, nv);
      if (wp->callback)
        wp->callback (&wp->w, wp->userdata, nv);
      else if (wp->motion_callback)
        wp->motion_callback (&wp->w, wp->motion_userdata, nv);
    }
    return 1;
  }

  if (wp->sType == XITK_HVSLIDER) {
    xitk_slider_hv_t info;
    if (wp->hv_max_y > 0) {
      if ((dx != -3) && (dx != 3))
        dx = 0;
    } else {
      if ((dy != -3) && (dy != 3))
        dy = 0;
    }
    if (!(dx | dy))
      return 0;
    info = wp->hv_info;
    info.h.pos = dx == -3 ? 0
               : dx == -2 ? info.h.pos - info.h.visible
               : dx == -1 ? info.h.pos - info.h.step
               : dx ==  1 ? info.h.pos + info.h.step
               : dx ==  2 ? info.h.pos + info.h.visible
               : dx ==  3 ? info.h.max - info.h.visible
               : info.h.pos;
    if (info.h.pos > info.h.max - info.h.visible)
      info.h.pos = info.h.max - info.h.visible;
    if (info.h.pos < 0)
      info.h.pos = 0;
    dx = wp->hv_info.h.pos;
    info.v.pos = dy == -3 ? 0
               : dy == -2 ? info.v.pos - info.v.visible
               : dy == -1 ? info.v.pos - info.v.step
               : dy ==  1 ? info.v.pos + info.v.step
               : dy ==  2 ? info.v.pos + info.v.visible
               : dy ==  3 ? info.v.max - info.v.visible
               : info.v.pos;
    dy = wp->hv_info.v.pos;
    if (info.v.pos > info.v.max - info.v.visible)
      info.v.pos = info.v.max - info.v.visible;
    if (info.v.pos < 0)
      info.v.pos = 0;
    xitk_slider_hv_sync (&wp->w, &info, XITK_SLIDER_SYNC_SET_AND_PAINT);
    if ((dx != wp->hv_info.h.pos) || (dy != wp->hv_info.v.pos)) {
      v = wp->hv_max_y > 0 ? wp->hv_info.v.max - wp->hv_info.v.visible - wp->hv_info.v.pos : wp->hv_info.h.pos;
      if (wp->callback)
        wp->callback (&wp->w, wp->userdata, v);
      else if (wp->motion_callback)
        wp->motion_callback (&wp->w, wp->motion_userdata, v);
    }
    return 1;
  }

  return 0;
}

static int xitk_slider_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp || !event)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_SLIDER)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      wp->paint (wp, event);
      return 0;
    case WIDGET_EVENT_KEY:
      return _xitk_slider_key (wp, event->string, event->modifier);
    case WIDGET_EVENT_CLICK:
      return _xitk_slider_click (wp, event->button, event->button_pressed, event->x, event->y);
    case WIDGET_EVENT_FOCUS:
      _xitk_slider_focus (wp, event->focus);
      return 0;
    case WIDGET_EVENT_INSIDE:
      return _xitk_slider_inside (wp, event->x, event->y) ? 1 : 2;
    case WIDGET_EVENT_CHANGE_SKIN:
      _xitk_slider_new_skin (wp, event->skonfig);
      return 0;
    case WIDGET_EVENT_DESTROY:
      _xitk_slider_destroy (wp);
      return 0;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _xitk_slider_get_skin (wp, event->skin_layer);
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

/*
 * Increment position
 */
int xitk_slider_make_step(xitk_widget_t *w) {
  _slider_private_t *wp;
  int v = 0;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    v = xitk_slider_get_pos (&wp->w);
    if (!wp->bClicked) {
      int m = xitk_slider_get_max (&wp->w);
      if (v < m) {
        v += wp->step;
        if (v > m)
          v = m;
        xitk_slider_set_pos (&wp->w, v);
      }
    }
  }
  return v;
}

/*
 * Decrement position
 */
int xitk_slider_make_backstep(xitk_widget_t *w) {
  _slider_private_t *wp;
  int v = 0;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    v = xitk_slider_get_pos (&wp->w);
    if (!wp->bClicked) {
      int m = xitk_slider_get_min (&wp->w);
      if (v > m) {
        v -= wp->step;
        if (v < m)
          v = m;
        xitk_slider_set_pos (&wp->w, v);
      }
    }
  }
  return v;
}

/*
 * Set value MIN.
 */
void xitk_slider_set_min (xitk_widget_t *w, int min) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    _xitk_slider_update_minmax (wp, (float)((min == wp->upper) ? min - 1 : min), wp->upper);
}

/*
 * Return the MIN value
 */
int xitk_slider_get_min (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)
    return (int)wp->lower;
  return -1;
}

/*
 * Return the MAX value
 */
int xitk_slider_get_max (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    return (int)wp->upper;
  return -1;
}

/*
 * Set value MAX
 */
void xitk_slider_set_max (xitk_widget_t *w, int max) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    _xitk_slider_update_minmax (wp, wp->lower, (float)((max == wp->lower) ? max + 1 : max));
}

/*
 * Set pos to 0 and redraw the widget.
 */
void xitk_slider_reset (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    _xitk_slider_update_value (wp, 0.0);
    wp->bClicked = 0;
    wp->paint (wp, NULL);
  }
}

/*
 * Set pos to max and redraw the widget.
 */
void xitk_slider_set_to_max (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      _xitk_slider_update_value (wp, wp->upper);
      wp->paint (wp, NULL);
    }
  }
}

/*
 * Return current position.
 */
int xitk_slider_get_pos (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    int value = wp->sType == XITK_HVSLIDER
              ? (wp->hv_max_y > 0 ? (int)wp->upper - wp->hv_info.v.pos : wp->hv_info.h.pos)
              : wp->value;
    return value;
  }
  return -1;
}

/*
 * Set position.
 */
void xitk_slider_set_pos (xitk_widget_t *w, int pos) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      if (wp->sType == XITK_HVSLIDER) {
        if (wp->hv_max_y > 0) {
          pos = (int)wp->upper - pos;
          if (wp->hv_info.v.pos != pos) {
            wp->hv_info.v.pos = pos;
            wp->paint (wp, NULL);
          }
        } else {
          if (wp->hv_info.h.pos != pos) {
            wp->hv_info.h.pos = pos;
            wp->paint (wp, NULL);
          }
        }
        if ((pos >= (int)wp->lower) && (pos <= (int)wp->upper))
          wp->value = pos;
      } else {
        float value = (float) pos;

        if ((value >= wp->lower) && (value <= wp->upper)) {
          _xitk_slider_update_value (wp, value);
          wp->paint (wp, NULL);
        }
      }
    }
  }
}

void xitk_slider_hv_sync (xitk_widget_t *w, xitk_slider_hv_t *info, xitk_slider_sync_t mode) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp || !info)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_SLIDER)
    return;
  if (wp->sType != XITK_HVSLIDER)
    return;

  if (mode == XITK_SLIDER_SYNC_GET) {
    *info = wp->hv_info;
  } else if ((mode == XITK_SLIDER_SYNC_SET) || (mode == XITK_SLIDER_SYNC_SET_AND_PAINT)) {
    if (!wp->skin_element_name.s &&
      ((info->h.visible != wp->hv_info.h.visible) || (info->h.max != wp->hv_info.h.max) ||
       (info->v.visible != wp->hv_info.v.visible) || (info->v.max != wp->hv_info.v.max))) {
      int hv_w, hv_h;
      int no_h = (info->h.visible >= info->h.max) || (info->h.max <= 0);
      int no_v = (info->v.visible >= info->v.max) || (info->v.max <= 0);

      if (no_h) {
        hv_w = wp->w.width;
      } else {
        int minw = no_v ? (wp->w.height * 5) >> 2 : 10;
        hv_w = wp->w.width * info->h.visible / info->h.max;
        if (hv_w < minw) {
          hv_w = minw;
          if (hv_w > wp->w.width)
            hv_w = wp->w.width;
        }
      }
      if (no_v) {
        hv_h = wp->w.height;
      } else {
        int minh = no_h ? (wp->w.width * 5) >> 2 : 10;
        hv_h = wp->w.height * info->v.visible / info->v.max;
        if (hv_h < minh) {
          hv_h = minh;
          if (hv_h > wp->w.height)
            hv_h = wp->w.height;
        }
      }
      if ((hv_w != wp->hv_w) || (hv_h != wp->hv_h)) {
        xitk_image_draw_paddle_three_state (wp->paddle_skin.image, hv_w, hv_h);
        wp->hv_w = hv_w;
        wp->hv_h = hv_h;
        wp->paddle_skin.width = hv_w;
        wp->paddle_skin.height = hv_h;
        _xitk_slider_update_skin (wp);
      }
    }

    wp->hv_info = *info;
    wp->lower = 0;
    wp->upper = wp->hv_max_y > 0
              ? (wp->hv_info.v.visible >= wp->hv_info.v.max ? 0 : wp->hv_info.v.max - wp->hv_info.v.visible)
              : (wp->hv_info.h.visible >= wp->hv_info.h.max ? 0 : wp->hv_info.h.max - wp->hv_info.h.visible);
    wp->step = wp->hv_max_y > 0 ? wp->hv_info.v.step : wp->hv_info.h.step;
    if (mode == XITK_SLIDER_SYNC_SET_AND_PAINT)
      wp->paint (wp, NULL);
  }
}

/*
 * Create the widget
 */
static xitk_widget_t *_xitk_slider_create (_slider_private_t *wp, xitk_slider_widget_t *s) {
  wp->bClicked           = 0;
  wp->focus              = FOCUS_LOST;

  wp->angle              = 0.0;

  wp->upper              = (float)((s->min == s->max) ? s->max + 1 : s->max);
  wp->lower              = (float)s->min;

  wp->value              = 0.0;
  wp->percentage         = 0.0;

  wp->step               = s->step;

  wp->button_width       = wp->paddle_skin.width / 3;

  wp->bar_mode    = 0;
  if (wp->sType == XITK_HSLIDER) {
    if (wp->button_width == wp->bg_skin.width)
      wp->bar_mode = 1;
  } else if (wp->sType == XITK_VSLIDER) {
    if (wp->paddle_skin.height == wp->bg_skin.height)
      wp->bar_mode = 1;
  } else if (wp->sType == XITK_HVSLIDER) {
    if (wp->skin_element_name.s) {
      wp->hv_w = wp->paddle_skin.width / 3;
      wp->hv_h = wp->paddle_skin.height;
      _xitk_slider_update_skin (wp);
    }
  }
  wp->hv_info.h.pos      = -1;
  wp->hv_info.h.step     = 0;
  wp->hv_info.h.visible  = -1;
  wp->hv_info.h.max      = -1;
  wp->hv_info.v.pos      = -1;
  wp->hv_info.v.step     = 0;
  wp->hv_info.v.visible  = -1;
  wp->hv_info.v.max      = -1;
  wp->hv_w               = 0;
  wp->hv_h               = 0;
  wp->paddle_drawn_here.x = 0;
  wp->paddle_drawn_here.y = 0;
  wp->paddle_drawn_here.w = 0;
  wp->paddle_drawn_here.h = 0;
  wp->paddle_drawn_here.fx = -1;

  wp->motion_callback    = s->motion_callback;
  wp->motion_userdata    = s->motion_userdata;
  wp->callback           = s->callback;
  wp->userdata           = s->userdata;

  wp->w.width            = wp->bg_skin.width;
  wp->w.height           = wp->bg_skin.height;
  wp->w.type             = WIDGET_TYPE_SLIDER | WIDGET_FOCUSABLE | WIDGET_TABABLE | WIDGET_CLICKABLE
                         | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event            = xitk_slider_event;

  return &wp->w;
}

/*
 * Create the widget
 */
xitk_widget_t *xitk_slider_create(xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_slider_widget_t *s) {
  _slider_private_t *wp;

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(s);

  wp = (_slider_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  xitk_short_string_init (&wp->skin_element_name);
  xitk_short_string_set (&wp->skin_element_name, s->skin_element_name);
  _xitk_slider_set_skin (wp, skonfig);

  return _xitk_slider_create (wp, s);
}

/*
 * Create the widget (not skined).
 */
xitk_widget_t *xitk_noskin_slider_create(xitk_widget_list_t *wl,
					 xitk_slider_widget_t *s,
					 int x, int y, int width, int height, int type) {
  _slider_private_t *wp;

  ABORT_IF_NULL(wl);

  XITK_CHECK_CONSTITENCY(s);

  wp = (_slider_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->skin_element_name.s = NULL;

  if (type == XITK_VSLIDER) {
    wp->paddle_skin.image = xitk_image_new (wl->xitk, NULL, 0, width * 3, height / 5);
    xitk_image_draw_paddle_three_state_vertical (wp->paddle_skin.image);
  } else if (type == XITK_HSLIDER) {
    wp->paddle_skin.image = xitk_image_new (wl->xitk, NULL, 0, (width / 5) * 3, height);
    xitk_image_draw_paddle_three_state_horizontal (wp->paddle_skin.image);
  } else if (type == XITK_RSLIDER) {
    int w;

    w = ((((width + height) >> 1) >> 1) / 10) * 3;
    wp->paddle_skin.image = xitk_image_new (wl->xitk, NULL, 0, (w * 3), w);
    xitk_image_draw_paddle_rotate (wp->paddle_skin.image);
  } else if (type == XITK_HVSLIDER) {
    wp->paddle_skin.image = xitk_image_new (wl->xitk, NULL, 0, width * 3, height);
    /* defer init to xitk_slider_hv_sync (). */
  } else {
    XITK_DIE ("Slider type unhandled.\n");
  }
  wp->paddle_skin.x = 0;
  wp->paddle_skin.y = 0;
  wp->paddle_skin.width = wp->paddle_skin.image->width;
  wp->paddle_skin.height = wp->paddle_skin.image->height;

  wp->bg_skin.image = xitk_image_new (wl->xitk, NULL, 0, width, height);
  if ((type == XITK_HSLIDER) || (type == XITK_VSLIDER) || (type == XITK_HVSLIDER))
    xitk_image_draw_inner (wp->bg_skin.image, width, height);
  else if (type == XITK_RSLIDER) {
    xitk_image_draw_rotate_button (wp->bg_skin.image);
  }
  wp->bg_skin.x = 0;
  wp->bg_skin.y = 0;
  wp->bg_skin.width = wp->bg_skin.image->width;
  wp->bg_skin.height = wp->bg_skin.image->height;

  wp->radius = (wp->bg_skin.height >> 1) - (wp->paddle_skin.height);

  wp->sType = type;
  _xitk_slider_set_paint (wp);
  wp->w.state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->w.x = x;
  wp->w.y = y;

  return _xitk_slider_create (wp, s);
}

