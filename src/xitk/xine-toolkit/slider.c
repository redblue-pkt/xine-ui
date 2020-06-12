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
#include <math.h>

#include "_xitk.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
  xitk_widget_t           w;

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *sWidget;
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

  xitk_image_t           *paddle_skin;
  xitk_image_t           *bg_skin;

  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
			  
  xitk_state_callback_t   callback;
  void                   *userdata;

  int                     button_width;
  float                   ratio;

  int                     paddle_cover_bg;

  xitk_slider_hv_t        hv_info;
  int                     hv_w, hv_h;
  int                     hv_x, hv_y, hv_max_x, hv_max_y;
} _slider_private_t;

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
    
    xc = wp->bg_skin->width / 2;
    yc = wp->bg_skin->height / 2;
    
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

    width = (float)wp->bg_skin->width;
    height = (float)wp->bg_skin->height;
    
    if (wp->paddle_cover_bg == 1) {

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
      pheight = wp->paddle_skin->height;
      
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
    if (!wp->skin_element_name) {
      xitk_image_free_image (&(wp->paddle_skin));
      xitk_image_free_image (&(wp->bg_skin));
    }
    XITK_FREE (wp->skin_element_name);
  }
}

/*
 *
 */
static xitk_image_t *_get_skin (_slider_private_t *wp, int sk) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if ((sk == FOREGROUND_SKIN) && (wp->paddle_skin))
      return wp->paddle_skin;
    else if ((sk == BACKGROUND_SKIN) && (wp->bg_skin))
      return wp->bg_skin;
  }
  return NULL;
}

/*
 *
 */
static int _notify_inside (_slider_private_t *wp, int x, int y) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (wp->w.visible == 1) {
      xitk_image_t *skin;
      
      if (wp->paddle_cover_bg == 1)
        skin = wp->paddle_skin;
      else
        skin = wp->bg_skin;

      if(skin->mask)
        return xitk_is_cursor_out_mask (&wp->w, skin->mask->pixmap, x, y);
    }
    else
      return 0;
  }
  return 1;
}

/*
 * Draw widget
 */
static void _paint_slider (_slider_private_t *wp) {
  int                     button_width, button_height;	
  xitk_image_t           *bg;
  xitk_image_t           *paddle;
  
  if (wp && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) && (wp->w.visible == 1))) {
    int    x, y, srcx1, srcx2, destx1, srcy1, srcy2, desty1;
    int    xcenter, ycenter;
    int    paddle_width;
    int    paddle_height;
    double angle;
    
    bg = (xitk_image_t *)wp->bg_skin;
    paddle = (xitk_image_t *)wp->paddle_skin;
    if (!paddle || !bg)
      return;
    x = y = srcx1 = srcx2 = destx1 = srcy1 = srcy2 = desty1 = 0;

    xitk_image_draw_image (wp->w.wl, bg, 0, 0, bg->width, bg->height, wp->w.x, wp->w.y);

    if (wp->w.enable == WIDGET_ENABLE) {

      if (wp->sType == XITK_HVSLIDER) {
        int dx;

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
        dx = (wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)
           ? (wp->bClicked ? 2 : 1)
           : 0;
        srcx1 = dx * wp->hv_w;
        srcy1 = 0;
        srcx2 = wp->hv_w;
        srcy2 = wp->hv_h;
        destx1 = wp->w.x + x;
        desty1 = wp->w.y + y;

      } else if (wp->sType == XITK_RSLIDER) {
        button_width = wp->bg_skin->width;
        button_height = wp->bg_skin->height;

        xcenter       = (bg->width /2) + wp->w.x;
	ycenter       = (bg->width /2) + wp->w.y;
	paddle_width  = paddle->width / 3;
	paddle_height = paddle->height;
        angle         = wp->angle;

	if(angle < M_PI / -2)
	  angle = angle + M_PI * 2;

        x = (int) (0.5 + xcenter + wp->radius * cos (angle)) - (paddle_width / 2);
        y = (int) (0.5 + ycenter - wp->radius * sin (angle)) - (paddle_height / 2);

	srcx1 = 0;

        if ((wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)) {
	  srcx1 += paddle_width;
          if (wp->bClicked)
	    srcx1 += paddle_width;
	}

	srcy1 = 0;
	srcx2 = paddle_width;
	srcy2 = paddle_height;
	destx1 = x;
	desty1 = y;

      }
      else {
	int   dir_area;
	float tmp;

	button_width = wp->button_width;
	button_height = paddle->height;

        dir_area = (float)(wp->sType == XITK_HSLIDER) ?
          wp->w.width - button_width : wp->w.height - button_height;

        tmp = (dir_area * .01) * (wp->percentage * 100.0);

        if (wp->sType == XITK_HSLIDER) {
          x = rint (wp->w.x + tmp);
          y = wp->w.y;
        } else if (wp->sType == XITK_VSLIDER) {
          x = wp->w.x;
          y = rint (wp->w.y + wp->bg_skin->height - button_height - tmp);
        }

        if (wp->paddle_cover_bg == 1) {
	  int pixpos;
	  int direction;

          x = wp->w.x;
          y = wp->w.y;

          direction = (wp->sType == XITK_HSLIDER) ? wp->bg_skin->width : wp->bg_skin->height;

          pixpos = (int)wp->value / ((wp->upper - wp->lower) / direction);

          if (wp->sType == XITK_VSLIDER) {
	    pixpos = button_height - pixpos;
	    srcx1  = 0;
	    srcy1  = pixpos;
	    srcx2  = button_width;
	    srcy2  = paddle->height - pixpos;
            destx1 = wp->w.x;
            desty1 = wp->w.y + pixpos;
	  }
          else if (wp->sType == XITK_HSLIDER) {
	    srcx1  = 0;
	    srcy1  = 0;
	    srcx2  = pixpos;
	    srcy2  = paddle->height;
            destx1 = wp->w.x;
            desty1 = wp->w.y;
	  }
	}
	else {
	  srcy1  = 0;
	  srcx2  = button_width;
	  srcy2  = paddle->height;
          destx1 = x;
          desty1 = y;
	}

        if ((wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)) {
          if (wp->bClicked)
	    srcx1 = 2*button_width;
	  else
	    srcx1 = button_width;
	}

      }

      xitk_image_draw_image (wp->w.wl, paddle, srcx1, srcy1, srcx2, srcy2, destx1, desty1);
    }
  }

}

/*
 *
 */
static void _notify_change_skin (_slider_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (wp->skin_element_name) {
      xitk_skin_lock (skonfig);

      wp->paddle_skin     = xitk_skin_get_image (skonfig, xitk_skin_get_slider_skin_filename (skonfig, wp->skin_element_name));
      if (!wp->paddle_skin) {
        xitk_skin_unlock (skonfig);
        return;
      }
      wp->hv_w = wp->paddle_skin->width / 3;
      wp->hv_h = wp->paddle_skin->height;
      _slider_update_skin (wp);
      wp->button_width    = wp->paddle_skin->width / 3;
      wp->bg_skin         = xitk_skin_get_image (skonfig, xitk_skin_get_skin_filename (skonfig, wp->skin_element_name));
      wp->sType           = xitk_skin_get_slider_type (skonfig, wp->skin_element_name);
      wp->paddle_cover_bg = 0;
      
      if (wp->sType == XITK_HSLIDER) {
        if (wp->button_width == wp->bg_skin->width)
          wp->paddle_cover_bg = 1;
      }
      else if (wp->sType == XITK_VSLIDER) {
        if (wp->paddle_skin->height == wp->bg_skin->height)
          wp->paddle_cover_bg = 1;
      }
      
      wp->radius = xitk_skin_get_slider_radius (skonfig, wp->skin_element_name);

      wp->w.x       = xitk_skin_get_coord_x (skonfig, wp->skin_element_name);
      wp->w.y       = xitk_skin_get_coord_y (skonfig, wp->skin_element_name);
      wp->w.width   = wp->bg_skin->width;
      wp->w.height  = wp->bg_skin->height;
      wp->w.visible = (xitk_skin_get_visibility (skonfig, wp->skin_element_name)) ? 1 : -1;
      wp->w.enable  = xitk_skin_get_enability (skonfig, wp->skin_element_name);
      
      xitk_skin_unlock (skonfig);

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
    if(button == Button1) {
      if (wp->focus == FOCUS_RECEIVED) {
        if (wp->sType == XITK_HVSLIDER) {
          /* if this thing really moves x and y, use xitk_slider_hv_sync (). */
          int new_value = _slider_report_xy (wp, x, y)
                        ? (wp->hv_max_y > 0 ? (int)wp->upper - wp->hv_info.v.pos : wp->hv_info.h.pos)
                        : -1;

          if (wp->bClicked == bUp)
            wp->bClicked = !bUp;
          if (bUp == 0) {
            _paint_slider (wp);
            /* Exec motion callback function (if available) */
            if ((new_value >= 0) && wp->motion_callback)
              wp->motion_callback (wp->sWidget, wp->motion_userdata, new_value);
          } else if (bUp == 1) {
            wp->bClicked = 0;
            _paint_slider (wp);
            if (wp->callback)
              wp->callback (wp->sWidget, wp->userdata, new_value);
          }
        } else {
          int old_value = (int)wp->value;

          _slider_update (wp, (x - wp->w.x), (y - wp->w.y));
          if (wp->bClicked == bUp)
            wp->bClicked = !bUp;
          if (bUp == 0) {
            if (old_value != (int)wp->value)
              _slider_update_value (wp, wp->value);
            _paint_slider (wp);
            /* Exec motion callback function (if available) */
            if (old_value != (int)wp->value) {
              if (wp->motion_callback)
                wp->motion_callback (wp->sWidget, wp->motion_userdata, (int)wp->value);
            }
          } else if (bUp == 1) {
            wp->bClicked = 0;
            _slider_update_value (wp, wp->value);
            _paint_slider (wp);
            if (wp->callback)
              wp->callback (wp->sWidget, wp->userdata, (int)wp->value);
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

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _slider_private_t *wp = (_slider_private_t *)w;
  int retval = 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _paint_slider (wp);
      break;
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
void xitk_slider_make_step(xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      if (((xitk_slider_get_pos (&wp->w)) + wp->step) <= xitk_slider_get_max (&wp->w))
        xitk_slider_set_pos (&wp->w, xitk_slider_get_pos (&wp->w) + wp->step);
    }
  } 
}

/*
 * Decrement position
 */
void xitk_slider_make_backstep(xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      if (xitk_slider_get_pos (&wp->w) - wp->step >= xitk_slider_get_min (&wp->w))
        xitk_slider_set_pos (&wp->w, xitk_slider_get_pos (&wp->w) - wp->step);
    }
  } 
}

/*
 * Set value MIN.
 */
void xitk_slider_set_min (xitk_widget_t *w, int min) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    _slider_update_minmax (wp, (float)((min == wp->upper) ? min - 1 : min), wp->upper);
}

/*
 * Return the MIN value
 */
int xitk_slider_get_min (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)
    return (int)wp->lower;
  return -1;
}

/*
 * Return the MAX value
 */
int xitk_slider_get_max (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    return (int)wp->upper;
  return -1;
}

/*
 * Set value MAX
 */
void xitk_slider_set_max (xitk_widget_t *w, int max) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
    _slider_update_minmax (wp, wp->lower, (float)((max == wp->lower) ? max + 1 : max));
}

/*
 * Set pos to 0 and redraw the widget.
 */
void xitk_slider_reset (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    _slider_update_value (wp, 0.0);
    wp->bClicked = 0;
    _paint_slider (wp);
  }
}

/*
 * Set pos to max and redraw the widget.
 */
void xitk_slider_set_to_max (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      _slider_update_value (wp, wp->upper);
      _paint_slider (wp);
    }
  }
}

/*
 * Return current position.
 */
int xitk_slider_get_pos (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
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
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    if (!wp->bClicked) {
      if (wp->sType == XITK_HVSLIDER) {
        if (wp->hv_max_y > 0) {
          pos = (int)wp->upper - pos;
          if (wp->hv_info.v.pos != pos) {
            wp->hv_info.v.pos = pos;
            _paint_slider (wp);
          }
        } else {
          if (wp->hv_info.h.pos != pos) {
            wp->hv_info.h.pos = pos;
            _paint_slider (wp);
          }
        }
        if ((pos >= (int)wp->lower) && (pos <= (int)wp->upper))
          wp->value = pos;
      } else {
        float value = (float) pos;

        if ((value >= wp->lower) && (value <= wp->upper)) {
          _slider_update_value (wp, value);
          _paint_slider (wp);
        }
      }
    }
  }
}

/*
 * Call callback for current position
 */
void xitk_slider_callback_exec (xitk_widget_t *w) {
  _slider_private_t *wp = (_slider_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    int value = wp->sType == XITK_HVSLIDER
              ? (wp->hv_max_y > 0 ? (int)wp->upper - wp->hv_info.v.pos : wp->hv_info.h.pos)
              : wp->value;

    if (wp->callback)
      wp->callback (wp->sWidget, wp->userdata, value);
    else if (wp->motion_callback)
        wp->motion_callback (wp->sWidget, wp->motion_userdata, value);
  }
}

void xitk_slider_hv_sync (xitk_widget_t *w, xitk_slider_hv_t *info, xitk_slider_sync_t mode) {
  _slider_private_t *wp = (_slider_private_t *)w;

  if (!wp || !info)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_SLIDER)
    return;
  if (wp->sType != XITK_HVSLIDER)
    return;

  if (mode == XITK_SLIDER_SYNC_GET) {
    *info = wp->hv_info;
  } else if ((mode == XITK_SLIDER_SYNC_SET) || (mode == XITK_SLIDER_SYNC_SET_AND_PAINT)) {
    if (!wp->skin_element_name &&
      ((info->h.visible != wp->hv_info.h.visible) || (info->h.max != wp->hv_info.h.max) ||
       (info->v.visible != wp->hv_info.v.visible) || (info->v.max != wp->hv_info.v.max))) {
      int hv_w, hv_h;

      if ((info->h.visible >= info->h.max) || (info->h.max <= 0)) {
        hv_w = wp->w.width;
      } else {
        hv_w = wp->w.width * info->v.visible / info->v.max;
        if (hv_w < 10) {
          hv_w = 10;
          if (hv_w > wp->w.width)
            hv_w = wp->w.width;
        }
      }
      if ((info->v.visible >= info->v.max) || (info->v.max <= 0)) {
        hv_h = wp->w.height;
      } else {
        hv_h = wp->w.height * info->v.visible / info->v.max;
        if (hv_h < 10) {
          hv_h = 10;
          if (hv_h > wp->w.height)
            hv_h = wp->w.height;
        }
      }
      if ((hv_w != wp->hv_w) || (hv_h != wp->hv_h)) {
        draw_paddle_three_state (wp->paddle_skin, hv_w, hv_h);
        wp->hv_w = hv_w;
        wp->hv_h = hv_h;
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
      _paint_slider (wp);
  }
}

/*
 * Create the widget
 */
static xitk_widget_t *_xitk_slider_create(xitk_widget_list_t *wl,
					  xitk_skin_config_t *skonfig, xitk_slider_widget_t *s,
					  int x, int y, const char *skin_element_name,
					  xitk_image_t *bg_skin, xitk_image_t *pad_skin,
					  int stype, int radius, int visible, int enable) {
  _slider_private_t *wp;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  wp = (_slider_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  wp->imlibdata          = wl->imlibdata;
  wp->skin_element_name  = skin_element_name ? strdup (s->skin_element_name) : NULL;

  wp->sWidget            = &wp->w;
  wp->sType              = stype;
  wp->bClicked           = 0;
  wp->focus              = FOCUS_LOST;

  wp->angle              = 0.0;

  wp->upper              = (float)((s->min == s->max) ? s->max + 1 : s->max);
  wp->lower              = (float)s->min;

  wp->value              = 0.0;
  wp->percentage         = 0.0;
  wp->radius             = radius;

  wp->step               = s->step;

  wp->paddle_skin        = pad_skin;
  wp->button_width       = wp->paddle_skin->width / 3;

  wp->bg_skin            = bg_skin;

  wp->paddle_cover_bg    = 0;
  if (wp->sType == XITK_HSLIDER) {
    if (wp->button_width == wp->bg_skin->width)
      wp->paddle_cover_bg = 1;
  } else if (wp->sType == XITK_VSLIDER) {
    if (wp->paddle_skin->height == wp->bg_skin->height)
      wp->paddle_cover_bg = 1;
  } else if (wp->sType == XITK_HVSLIDER) {
    if (wp->skin_element_name) {
      wp->hv_w = wp->paddle_skin->width / 3;
      wp->hv_h = wp->paddle_skin->height;
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

  wp->motion_callback    = s->motion_callback;
  wp->motion_userdata    = s->motion_userdata;
  wp->callback           = s->callback;
  wp->userdata           = s->userdata;

  wp->w.private_data     = wp;

  wp->w.wl               = wl;

  wp->w.enable           = enable;
  wp->w.running          = 1;
  wp->w.visible          = visible;
  wp->w.have_focus       = FOCUS_LOST;
  wp->w.x                = x;
  wp->w.y                = y;
  wp->w.width            = wp->bg_skin->width;
  wp->w.height           = wp->bg_skin->height;
  wp->w.type             = WIDGET_TYPE_SLIDER | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEYABLE;
  wp->w.event            = notify_event;
  wp->w.tips_timeout     = 0;
  wp->w.tips_string      = NULL;

  return &wp->w;
}

/*
 * Create the widget
 */
xitk_widget_t *xitk_slider_create(xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_slider_widget_t *s) {

  XITK_CHECK_CONSTITENCY(s);

  xitk_image_t *bg_skin = xitk_skin_get_image(skonfig,
      xitk_skin_get_skin_filename(skonfig, s->skin_element_name));
  xitk_image_t *pad_skin = xitk_skin_get_image(skonfig,
      xitk_skin_get_slider_skin_filename(skonfig, s->skin_element_name));
  if (!bg_skin || !pad_skin)
    return NULL;

  return _xitk_slider_create(wl, skonfig, s,
			     (xitk_skin_get_coord_x(skonfig, s->skin_element_name)),
			     (xitk_skin_get_coord_y(skonfig, s->skin_element_name)),
			     s->skin_element_name,
			     bg_skin, pad_skin,
			     (xitk_skin_get_slider_type(skonfig, s->skin_element_name)),
			     (xitk_skin_get_slider_radius(skonfig, s->skin_element_name)),
			     ((xitk_skin_get_visibility(skonfig, s->skin_element_name)) ? 1 : -1),
			     (xitk_skin_get_enability(skonfig, s->skin_element_name)));
}

/*
 * Create the widget (not skined).
 */
xitk_widget_t *xitk_noskin_slider_create(xitk_widget_list_t *wl,
					 xitk_slider_widget_t *s,
					 int x, int y, int width, int height, int type) {
  xitk_image_t   *b, *p;
  int             radius;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_CHECK_CONSTITENCY(s);
  
  if (type == XITK_VSLIDER) {
    p = xitk_image_create_image (wl->imlibdata, width * 3, height / 5);
    xitk_image_add_mask (p);
    draw_paddle_three_state_vertical (p);
  } else if (type == XITK_HSLIDER) {
    p = xitk_image_create_image(wl->imlibdata, (width / 5) * 3, height);
    xitk_image_add_mask (p);
    draw_paddle_three_state_horizontal (p);
  } else if (type == XITK_RSLIDER) {
    int w;
    
    w = ((((width + height) >> 1) >> 1) / 10) * 3;
    p = xitk_image_create_image(wl->imlibdata, (w * 3), w);
    xitk_image_add_mask (p);
    draw_paddle_rotate (p);
  } else if (type == XITK_HVSLIDER) {
    p = xitk_image_create_image (wl->imlibdata, width * 3, height);
    xitk_image_add_mask (p);
    /* defer init to xitk_slider_hv_sync (). */
  } else {
    XITK_DIE ("Slider type unhandled.\n");
  }
  
  b = xitk_image_create_image(wl->imlibdata, width, height);
  xitk_image_add_mask(b);
  if ((type == XITK_HSLIDER) || (type == XITK_VSLIDER) || (type == XITK_HVSLIDER))
    draw_inner(b->image, width, height);
  else if(type == XITK_RSLIDER) {
    draw_rotate_button(b);
  }
  
  radius = (b->height >> 1) - (p->height);

  return _xitk_slider_create(wl,NULL, s, x, y, NULL, b, p, type, radius, 0, 0);
}
