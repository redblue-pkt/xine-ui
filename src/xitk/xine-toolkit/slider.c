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
#include <math.h>

#include "_xitk.h"
#include "slider.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
  xitk_widget_t           w;

  xitk_short_string_t     skin_element_name;

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
    int                   x, y, w, h;
  }                       paddle_drawn_here;
} _slider_private_t;

static int _tabs_min (int a, int b) {
  int d = b - a;
  return a + (d & (d >> (8 * sizeof (d) - 1)));
}

static int _tabs_max (int a, int b) {
  int d = a - b;
  return a - (d & (d >> (8 * sizeof (d) - 1)));
}

static void _slider_update_skin (_slider_private_t *wp) {
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

static int _slider_report_xy (_slider_private_t *wp, int x, int y) {
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
static void _slider_update_value (_slider_private_t *wp, float value) {
  float range;

  if (value < wp->lower)
    wp->value = wp->lower;
  else if (value > wp->upper)
    wp->value = wp->upper;
  else
    wp->value = value;

  range = wp->upper - wp->lower;

  wp->percentage = (wp->value - wp->lower) / range;
  wp->angle = 7.0 * M_PI / 6.0 - (wp->value - wp->lower) * 4.0 * M_PI / 3.0 / (wp->upper - wp->lower);
}

/*
 *
 */
static void _slider_update_minmax (_slider_private_t *wp, float min, float max) {
  if(min > max) {
    XITK_WARNING("%s@%d: slider min value > max value !\n", __FILE__, __LINE__);
    return;
  }

  wp->upper = (min == max) ? max + 1 : max;
  wp->lower = min;
  _slider_update_value (wp, wp->value);
}

/*
 *
 */
static void _slider_update (_slider_private_t *wp, int x, int y) {

  if (wp->sType == XITK_RSLIDER) {
    int   xc, yc;
    float old_value;

    xc = wp->bg_skin.width / 2;
    yc = wp->bg_skin.height / 2;

    old_value = wp->value;
    wp->angle = atan2 (yc - y, x - xc);

    if (wp->angle < -M_PI / 2.)
      wp->angle += 2 * M_PI;

    if (wp->angle < -M_PI / 6)
      wp->angle = -M_PI / 6;

    if (wp->angle > 7. * M_PI / 6.)
      wp->angle = 7. * M_PI / 6.;

    wp->value = wp->lower + (7. * M_PI / 6 - wp->angle) * (wp->upper - wp->lower) / (4. * M_PI / 3.);

    if (wp->value != old_value) {
      float new_value = wp->value;

      if (new_value < wp->lower)
        new_value = wp->lower;

      if (new_value > wp->upper)
        new_value = wp->upper;

      _slider_update_value (wp, new_value);

    }
  }
  else {
    float width, height;
    float old_value, new_value = 0.0;

    old_value = wp->value;

    width = (float)wp->bg_skin.width;
    height = (float)wp->bg_skin.height;

    if (wp->bar_mode == 1) {

      if(x < 0)
	x = 0;
      if(x > width)
	x = width;

      if(y < 0)
	y = 0;
      if(y > height)
	y = height;

      if (wp->sType == XITK_HSLIDER)
	new_value = (x * .01) / (width * .01);
      else if (wp->sType == XITK_VSLIDER)
	new_value = ((height - y) * .01) / (height * .01);

    }
    else {
      int pwidth, pheight;

      pwidth = wp->button_width;
      pheight = wp->paddle_skin.height;

      if (wp->sType == XITK_HSLIDER) {
	x -= pwidth >> 1;

	if(x < 0)
	  x = 0;
	if(x > (width - pwidth))
	  x = width - pwidth;

	if(y < 0)
	  y = 0;
	if(y > height)
	  y = height;

      }
      else { /* XITK_VSLIDER */

	if(x < 0)
	  x = 0;
	if(x > width)
	  x = width;

	y += pheight >> 1;

	if(y < 0)
	  y = 0;
	if(y > (height + pheight))
	  y = height + pheight ;
      }

      if (wp->sType == XITK_HSLIDER)
	new_value = (x * .01) / ((width - pwidth) * .01);
      else if (wp->sType == XITK_VSLIDER)
	new_value = ((height - y) * .01) / ((height - pheight) * .01);
    }

    wp->value = wp->lower + (new_value * (wp->upper - wp->lower));

    if (wp->value != old_value) {
      float new_value = wp->value;

      if (new_value < wp->lower)
        new_value = wp->lower;

      if (new_value > wp->upper)
        new_value = wp->upper;

      _slider_update_value (wp, new_value);
    }
  }
}

/*
 *
 */
static void _notify_destroy (_slider_private_t *wp) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->skin_element_name.s) {
      xitk_image_free_image (&(wp->paddle_skin.image));
      xitk_image_free_image (&(wp->bg_skin.image));
    }
    xitk_short_string_deinit (&wp->skin_element_name);
  }
}

/*
 *
 */
static xitk_image_t *_get_skin (_slider_private_t *wp, int sk) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if ((sk == FOREGROUND_SKIN) && (wp->paddle_skin.image))
      return wp->paddle_skin.image;
    else if ((sk == BACKGROUND_SKIN) && (wp->bg_skin.image))
      return wp->bg_skin.image;
  }
  return NULL;
}

/*
 *
 */
static int _notify_inside (_slider_private_t *wp, int x, int y) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (wp->w.visible == 1) {
      if (wp->bar_mode == 1) {
        return xitk_image_inside (wp->paddle_skin.image, wp->paddle_skin.x + x - wp->w.x, wp->paddle_skin.y + y - wp->w.y);
      } else {
        return xitk_image_inside (wp->bg_skin.image, wp->bg_skin.x + x - wp->w.x, wp->bg_skin.y + y - wp->w.y);
      }
    } else
      return 0;
  }
  return 1;
}

/*
 * Draw widget
 */
static void _paint_slider (_slider_private_t *wp, widget_event_t *event) {
  xitk_image_t           *bg;
  xitk_image_t           *paddle;

  if (wp && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) && (wp->w.visible == 1)) && wp->bg_skin.width) {
    int    srcx1, src_w, destx1, srcy1, src_h, desty1;
    int    xcenter, ycenter;
    int    paddle_x, paddle_width, paddle_height;
    double angle;

    bg = wp->bg_skin.image;
    paddle = wp->paddle_skin.image;
    if (!paddle || !bg)
      return;
    srcx1 = src_w = destx1 = srcy1 = src_h = desty1 = 0;

    if (wp->sType == XITK_HVSLIDER) {
      paddle_width = wp->hv_w;
      paddle_height = wp->hv_h;
    } else {
      paddle_width = wp->button_width;
      paddle_height = wp->paddle_skin.height;
    }
    paddle_x = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
             ? (wp->bClicked ? 2 : 1)
             : 0;
    paddle_x *= paddle_width;

    if (!event) {
      if (wp->paddle_drawn_here.w > 0) {
        /* incremental */
        if (wp->bar_mode) {
          /* nasty little shortcut ;-) */
          if (wp->sType == XITK_HSLIDER) {
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
          } else if (wp->sType == XITK_VSLIDER) {
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
          }
          return;
        } else {
          xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
            wp->paddle_drawn_here.x, wp->paddle_drawn_here.y,
            wp->paddle_drawn_here.w, wp->paddle_drawn_here.h,
            wp->w.x + wp->paddle_drawn_here.x, wp->w.y + wp->paddle_drawn_here.y);
        }
      } else {
        /* full */
        xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL, 0, 0, bg->width, bg->height, wp->w.x, wp->w.y);
      }
    } else {
      /* partial */
      xitk_part_image_draw (wp->w.wl, &wp->bg_skin, NULL,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y);
    }

    if (wp->w.enable == WIDGET_ENABLE) {

      if (wp->sType == XITK_HVSLIDER) {
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
        srcx1 = paddle_x;
        srcy1 = 0;
        src_w = wp->hv_w;
        src_h = wp->hv_h;
        destx1 = wp->w.x + x;
        desty1 = wp->w.y + y;

      } else if (wp->sType == XITK_RSLIDER) {

        xcenter = (wp->bg_skin.width >> 1) + wp->w.x;
        ycenter = (wp->bg_skin.height >> 1) + wp->w.y;
        angle   = wp->angle;

	if(angle < M_PI / -2)
	  angle = angle + M_PI * 2;

        destx1 = (int) (0.5 + xcenter + wp->radius * cos (angle)) - (paddle_width / 2);
        desty1 = (int) (0.5 + ycenter - wp->radius * sin (angle)) - (paddle_height / 2);

        srcx1 = paddle_x;
	srcy1 = 0;
	src_w = paddle_width;
	src_h = paddle_height;

      } else {

        if (wp->bar_mode == 1) {
          if (wp->sType == XITK_VSLIDER) {
            int pixpos = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.height);
            pixpos = paddle_height - pixpos;
            srcy1  = pixpos;
            src_w  = paddle_width;
            src_h  = paddle_height - pixpos;
            destx1 = wp->w.x;
            desty1 = wp->w.y + pixpos;
          } else if (wp->sType == XITK_HSLIDER) {
            int pixpos = (int)wp->value / ((wp->upper - wp->lower) / wp->bg_skin.width);
            srcy1  = 0;
            src_w  = pixpos;
            src_h  = paddle_height;
            destx1 = wp->w.x;
            desty1 = wp->w.y;
	  }
        } else {
          if (wp->sType == XITK_HSLIDER) {
            destx1 = wp->w.x + ((wp->w.width - paddle_width) * .01) * (wp->percentage * 100.0) + 0.5;
            desty1 = wp->w.y;
          } else if (wp->sType == XITK_VSLIDER) {
            destx1 = wp->w.x;
            desty1 = wp->w.y + wp->bg_skin.height - paddle_height
                   - ((wp->w.height - paddle_height) * .01) * (wp->percentage * 100.0) + 0.5;
          }
	  srcy1  = 0;
          src_w  = paddle_width;
          src_h  = paddle_height;
	}

        srcx1 = paddle_x;
      }

      wp->paddle_drawn_here.x = destx1 - wp->w.x;
      wp->paddle_drawn_here.y = desty1 - wp->w.y;
      wp->paddle_drawn_here.w = src_w;
      wp->paddle_drawn_here.h = src_h;
      if (event) {
        int x1, x2, y1, y2;

        x1 = _tabs_max (destx1, event->x);
        x2 = _tabs_min (destx1 + src_w, event->x + event->width);
        y1 = _tabs_max (desty1, event->y);
        y2 = _tabs_min (desty1 + src_h, event->y + event->height);
        if ((x1 < x2) && (y1 < y2))
          xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
            srcx1 + x1 - destx1, srcy1 + y1 - desty1,
            x2 - x1, y2 - y1, x1, y1);
      } else {
        xitk_part_image_draw (wp->w.wl, &wp->paddle_skin, NULL,
          srcx1, srcy1, src_w, src_h, destx1, desty1);
      }
    }
  }

}

static void _xitk_slider_get_skin (_slider_private_t *wp, xitk_skin_config_t *skonfig) {
  const xitk_skin_element_info_t *s = xitk_skin_get_info (skonfig, wp->skin_element_name.s);
  /* always be there for the application. if this skin does not use us, disable user side. */
  if (s && s->pixmap_img.image && s->slider_pixmap_pad_img.image) {
    wp->w.x         = s->x;
    wp->w.y         = s->y;
    wp->w.enable    = s->enability;
    wp->w.visible   = s->visibility ? 1 : -1;
    wp->sType       = s->slider_type;
    wp->radius      = s->slider_radius;
    wp->bg_skin     = s->pixmap_img;
    wp->paddle_skin = s->slider_pixmap_pad_img;
  } else {
    wp->w.x         = 0;
    wp->w.y         = 0;
    wp->w.enable    = 0;
    wp->w.visible   = -1;
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
}

/*
 *
 */
static void _notify_change_skin (_slider_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (wp->skin_element_name.s) {
      xitk_skin_lock (skonfig);
      _xitk_slider_get_skin (wp, skonfig);
      xitk_skin_unlock (skonfig);
      wp->hv_w = wp->paddle_skin.width / 3;
      wp->hv_h = wp->paddle_skin.height;
      _slider_update_skin (wp);
      wp->button_width    = wp->paddle_skin.width / 3;
      wp->bar_mode = 0;
      if (wp->sType == XITK_HSLIDER) {
        if (wp->button_width == wp->bg_skin.width)
          wp->bar_mode = 1;
      }
      else if (wp->sType == XITK_VSLIDER) {
        if (wp->paddle_skin.height == wp->bg_skin.height)
          wp->bar_mode = 1;
      }
      wp->w.width   = wp->bg_skin.width;
      wp->w.height  = wp->bg_skin.height;
      xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    }
  }
}

/*
 * Got click
 */
static int _notify_click_slider (_slider_private_t *wp, int button, int bUp, int x, int y) {
  int                    ret = 0;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (button == 1) {
      if (wp->focus == FOCUS_RECEIVED) {
        if (wp->sType == XITK_HVSLIDER) {
          /* if this thing really moves x and y, use xitk_slider_hv_sync (). */
          int moved = _slider_report_xy (wp, x, y);
          int new_value = wp->hv_max_y > 0 ? (int)wp->upper - wp->hv_info.v.pos : wp->hv_info.h.pos;
          if (wp->bClicked == bUp)
            wp->bClicked = !bUp;
          if (bUp == 0) {
            _paint_slider (wp, NULL);
            /* Exec motion callback function (if available) */
            if (moved && wp->motion_callback)
              wp->motion_callback (&wp->w, wp->motion_userdata, new_value);
          } else if (bUp == 1) {
            wp->bClicked = 0;
            _paint_slider (wp, NULL);
            if (wp->callback)
              wp->callback (&wp->w, wp->userdata, new_value);
          }
        } else {
          int old_value = (int)wp->value;

          _slider_update (wp, (x - wp->w.x), (y - wp->w.y));
          if (wp->bClicked == bUp)
            wp->bClicked = !bUp;
          if (bUp == 0) {
            if (old_value != (int)wp->value)
              _slider_update_value (wp, wp->value);
            _paint_slider (wp, NULL);
            /* Exec motion callback function (if available) */
            if (old_value != (int)wp->value) {
              if (wp->motion_callback)
                wp->motion_callback (&wp->w, wp->motion_userdata, (int)wp->value);
            }
          } else if (bUp == 1) {
            wp->bClicked = 0;
            _slider_update_value (wp, wp->value);
            _paint_slider (wp, NULL);
            if (wp->callback)
              wp->callback (&wp->w, wp->userdata, (int)wp->value);
          }
        }
      }
      ret = 1;
    }
  }

  return ret;
}

/*
 * Got focus
 */
static int _notify_focus_slider (_slider_private_t *wp, int focus) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked)
      wp->focus = focus;
  }
  return 1;
}

static int _slider_key (_slider_private_t *wp, const char *string, int modifier) {
  int v, dx, dy;

  if ((wp->w.enable != WIDGET_ENABLE) || !string)
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

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _slider_private_t *wp;
  int retval = 0;

  xitk_container (wp, w, w);
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = wp->w.x;
      event->y = wp->w.y;
      event->width = wp->w.width;
      event->height = wp->w.height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      _paint_slider (wp, event);
      break;
    case WIDGET_EVENT_KEY:
      return _slider_key (wp, event->string, event->modifier);
    case WIDGET_EVENT_CLICK:
      result->value = _notify_click_slider (wp, event->button, event->button_pressed, event->x, event->y);
      retval = 1;
      break;
    case WIDGET_EVENT_FOCUS:
      _notify_focus_slider (wp, event->focus);
      break;
    case WIDGET_EVENT_INSIDE:
      result->value = _notify_inside (wp, event->x, event->y);
      retval = 1;
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _notify_change_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _notify_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _get_skin (wp, event->skin_layer);
        retval = 1;
      }
      break;
    default: ;
  }
  return retval;
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
    _slider_update_minmax (wp, (float)((min == wp->upper) ? min - 1 : min), wp->upper);
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
    _slider_update_minmax (wp, wp->lower, (float)((max == wp->lower) ? max + 1 : max));
}

/*
 * Set pos to 0 and redraw the widget.
 */
void xitk_slider_reset (xitk_widget_t *w) {
  _slider_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    _slider_update_value (wp, 0.0);
    wp->bClicked = 0;
    _paint_slider (wp, NULL);
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
      _slider_update_value (wp, wp->upper);
      _paint_slider (wp, NULL);
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
            _paint_slider (wp, NULL);
          }
        } else {
          if (wp->hv_info.h.pos != pos) {
            wp->hv_info.h.pos = pos;
            _paint_slider (wp, NULL);
          }
        }
        if ((pos >= (int)wp->lower) && (pos <= (int)wp->upper))
          wp->value = pos;
      } else {
        float value = (float) pos;

        if ((value >= wp->lower) && (value <= wp->upper)) {
          _slider_update_value (wp, value);
          _paint_slider (wp, NULL);
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
        _slider_update_skin (wp);
      }
    }

    wp->hv_info = *info;
    wp->lower = 0;
    wp->upper = wp->hv_max_y > 0
              ? (wp->hv_info.v.visible >= wp->hv_info.v.max ? 0 : wp->hv_info.v.max - wp->hv_info.v.visible)
              : (wp->hv_info.h.visible >= wp->hv_info.h.max ? 0 : wp->hv_info.h.max - wp->hv_info.h.visible);
    wp->step = wp->hv_max_y > 0 ? wp->hv_info.v.step : wp->hv_info.h.step;
    if (mode == XITK_SLIDER_SYNC_SET_AND_PAINT)
      _paint_slider (wp, NULL);
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
      _slider_update_skin (wp);
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

  wp->motion_callback    = s->motion_callback;
  wp->motion_userdata    = s->motion_userdata;
  wp->callback           = s->callback;
  wp->userdata           = s->userdata;

  wp->w.width            = wp->bg_skin.width;
  wp->w.height           = wp->bg_skin.height;
  wp->w.type             = WIDGET_TYPE_SLIDER | WIDGET_FOCUSABLE | WIDGET_TABABLE | WIDGET_CLICKABLE
                         | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event            = notify_event;

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
  _xitk_slider_get_skin (wp, skonfig);

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
  wp->w.enable = 0;
  wp->w.visible = 0;
  wp->w.x = x;
  wp->w.y = y;

  return _xitk_slider_create (wp, s);
}
