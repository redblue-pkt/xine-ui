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
#include <math.h>

#include "Imlib-light/Imlib.h"
#include "image.h"
#include "widget.h"
#include "slider.h"
#include "widget_types.h"
#include "_xitk.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 *
 */
static void slider_update_value(xitk_widget_t *w, float value) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;
  float range;

  if(value < private_data->lower)
    private_data->value = private_data->lower;
  else if(value > private_data->upper)
    private_data->value = private_data->upper;
  else
    private_data->value = value;

  range = (private_data->upper + -private_data->lower);
  
  private_data->percentage = (value + -private_data->lower) / range;
  private_data->angle = 7.*M_PI/6. - (value - private_data->lower) * 
    4.*M_PI/3. / (private_data->upper - private_data->lower);
  
}

/*
 *
 */
static void slider_update_minmax(xitk_widget_t *w, float min, float max) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;

  if(min > max) {
    XITK_WARNING("%s@%d: slider min value > max value !\n", __FILE__, __LINE__);
    return;
  }
  
  private_data->upper = max;
  private_data->lower = min;
  slider_update_value(w, private_data->value);
}

/*
 *
 */
static void slider_update(xitk_widget_t *w, int x, int y) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;
  
  if(private_data->sType == XITK_RSLIDER) {
    int xc, yc;
    float old_value;
    
    xc = private_data->bg_skin->width / 2;
    yc = private_data->bg_skin->height / 2;
    
    old_value = private_data->value;
    private_data->angle = atan2(yc - y, x - xc);
    
    if(private_data->angle < -M_PI / 2.)
      private_data->angle += 2 * M_PI;
    
    if(private_data->angle < -M_PI / 6)
      private_data->angle = -M_PI / 6;
    
    if(private_data->angle > 7. * M_PI / 6.)
      private_data->angle = 7. * M_PI / 6.;
    
    private_data->value = private_data->lower + (7. * M_PI / 6 - private_data->angle) *
      (private_data->upper - private_data->lower) / (4. * M_PI / 3.);
    
    if(private_data->value != old_value) {
      float new_value;
      
      new_value = private_data->value;
      
      if(new_value < private_data->lower)
	new_value = private_data->lower;
      
      if(new_value > private_data->upper)
	new_value = private_data->upper;
      
      slider_update_value(w, new_value);
      
    }
  }
  else {
    float width, height;
    float old_value, new_value = 0.0;
    
    old_value = private_data->value;

    width = (float)private_data->bg_skin->width;
    height = (float)private_data->bg_skin->height;

    if(x < 0)
      x = 0;
    if(x > width)
      x = width;
    if(y < 0)
      y = 0;
    if(y > height)
      y = height;
    
    if(private_data->sType == XITK_HSLIDER)
      new_value = (x * .01) / (width * .01);
    else if(private_data->sType == XITK_VSLIDER)
      new_value = ((height - y) * .01) / (height * .01);
    
    private_data->value = private_data->lower + 
      (new_value * (private_data->upper - private_data->lower));
    
    if(private_data->value != old_value) {
      float new_value;
      
      new_value = private_data->value;
      
      if (new_value < private_data->lower)
	new_value = private_data->lower;
      
      if (new_value > private_data->upper)
	new_value = private_data->upper;
      
      slider_update_value(w, new_value);
    }
  }
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_SLIDER) {
    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->paddle_skin);
    xitk_image_free_image(private_data->imlibdata, &private_data->bg_skin);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_SLIDER) {
    if(sk == FOREGROUND_SKIN && private_data->paddle_skin) {
      return private_data->paddle_skin;
    }
    else if(sk == BACKGROUND_SKIN && private_data->bg_skin) {
      return private_data->bg_skin;
    }
  }

  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *sl, int x, int y) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if(sl->widget_type & WIDGET_TYPE_SLIDER) {
    if(sl->visible) {
      xitk_image_t *skin;
      
      if(private_data->paddle_cover_bg == 1)
	skin = private_data->paddle_skin;
      else
	skin = private_data->bg_skin;

      return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, sl, skin->mask, x, y);
    }
    else
      return 0;
  }

  return 1;
}

/*
 * Draw widget
 */
static void paint_slider (xitk_widget_t *sl, Window win, GC gc) {
  int                     button_width, button_height;	
  slider_private_data_t  *private_data = (slider_private_data_t *) sl->private_data;
  GC                      bgc, pgc;
  xitk_image_t           *bg = (xitk_image_t *) private_data->bg_skin;
  xitk_image_t           *paddle = (xitk_image_t *) private_data->paddle_skin;
  
  if ((sl->widget_type & WIDGET_TYPE_SLIDER) && sl->visible) {
    int x, y, srcx1, srcx2, destx1, srcy1, srcy2, desty1;
    int    xcenter, ycenter;
    int    paddle_width;
    int    paddle_height;
    double angle;
    
    XLOCK (private_data->imlibdata->x.disp);

    x = y = srcx1 = srcx2 = destx1 = srcy1 = srcy2 = desty1 = 0;
        
    bgc = XCreateGC(private_data->imlibdata->x.disp, bg->image, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, bgc);
    pgc = XCreateGC(private_data->imlibdata->x.disp, paddle->image, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, pgc);
      
    if (bg->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, bgc, sl->x, sl->y);
      XSetClipMask(private_data->imlibdata->x.disp, bgc, bg->mask);
    }
    
    XCopyArea (private_data->imlibdata->x.disp, bg->image, win, bgc, 0, 0,
	       bg->width, bg->height, sl->x, sl->y);
      
    
    if(private_data->sType == XITK_RSLIDER) {
      
      button_width = private_data->bg_skin->width;
      button_height = private_data->bg_skin->height;

      xcenter       = (bg->width /2) + sl->x;
      ycenter       = (bg->width /2) + sl->y;
      paddle_width  = paddle->width / 3;
      paddle_height = paddle->height;
      angle         = private_data->angle;
      
      if(angle < M_PI / -2)
	angle = angle + M_PI * 2;
      
      x = (int) (0.5 + xcenter + private_data->radius * cos(angle)) - (paddle_width / 2);
      y = (int) (0.5 + ycenter - private_data->radius * sin(angle)) - (paddle_height / 2);

      srcx1 = 0;
      
      if(private_data->bArmed) {
	srcx1 += paddle_width;
	if(private_data->bClicked)
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

      button_width = private_data->button_width;
      button_height = paddle->height;
      
      dir_area = (float)(private_data->sType == XITK_HSLIDER) ? 
	sl->width - button_width : sl->height - button_height;
	
      tmp = (dir_area * .01) * (private_data->percentage * 100.0);
      
      if(private_data->sType == XITK_HSLIDER) {
	x = rint(sl->x + tmp);
	y = sl->y;
      }
      else if(private_data->sType == XITK_VSLIDER) {
	x = sl->x;
	y = rint(sl->y + private_data->bg_skin->height - button_height - tmp);
      }
      
      if(private_data->paddle_cover_bg == 1) {
	int pixpos;
	int direction;
	
	x = sl->x;
	y = sl->y;
	
	direction = (private_data->sType == XITK_HSLIDER) ? 
	  private_data->bg_skin->width : private_data->bg_skin->height;
	
	pixpos = (int)private_data->value / 
	  ((private_data->upper - private_data->lower) / direction);
	
	if(private_data->sType == XITK_VSLIDER) {
	  pixpos = button_height - pixpos;
	  srcx1  = 0;
	  srcy1  = pixpos;
	  srcx2  = button_width;
	  srcy2  = paddle->height - pixpos;
	  destx1 = sl->x;
	  desty1 = sl->y + pixpos;
	}
	else if(private_data->sType == XITK_HSLIDER) {
	  srcx1  = 0;
	  srcy1  = 0;
	  srcx2  = pixpos;
	  srcy2  = paddle->height;
	  destx1 = sl->x;
	  desty1 = sl->y;
	}
      }
      else {
	srcy1  = 0;
	srcx2  = button_width;
	srcy2  = paddle->height;
	destx1 = x;
	desty1 = y;
      }
      
      if(private_data->bArmed) {
	if(private_data->bClicked)
	  srcx1 = 2*button_width;
	else
	  srcx1 = button_width;
      }

    }
    
    if(paddle->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, pgc, x, y);
      XSetClipMask(private_data->imlibdata->x.disp, pgc, paddle->mask);
    }
    
    XCopyArea(private_data->imlibdata->x.disp, paddle->image, win, pgc,
	      srcx1, srcy1, srcx2, srcy2, destx1, desty1);
    
    XFreeGC(private_data->imlibdata->x.disp, pgc);
    XFreeGC(private_data->imlibdata->x.disp, bgc);
    XUNLOCK(private_data->imlibdata->x.disp);
  }

}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *sl, xitk_skin_config_t *skonfig) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if(sl->widget_type & WIDGET_TYPE_SLIDER) {
    if(private_data->skin_element_name) {

      xitk_skin_lock(skonfig);

      XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->paddle_skin);
      private_data->paddle_skin     = xitk_image_load_image(private_data->imlibdata, xitk_skin_get_slider_skin_filename(skonfig, private_data->skin_element_name));
      private_data->button_width    = private_data->paddle_skin->width / 3;
      XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->bg_skin);
      private_data->bg_skin         = xitk_image_load_image(private_data->imlibdata, xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
      private_data->sType = xitk_skin_get_slider_type(skonfig, private_data->skin_element_name);
      private_data->paddle_cover_bg = 0;
      
      if(private_data->sType == XITK_HSLIDER) {
	if(private_data->button_width == private_data->bg_skin->width)
	  private_data->paddle_cover_bg = 1;
      }
      else if(private_data->sType == XITK_VSLIDER) {
	if(private_data->paddle_skin->height == private_data->bg_skin->height)
	  private_data->paddle_cover_bg = 1;
      }
      
      private_data->radius = xitk_skin_get_slider_radius(skonfig, private_data->skin_element_name);

      sl->x       = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      sl->y       = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      sl->width   = private_data->bg_skin->width;
      sl->height  = private_data->bg_skin->height;
      sl->visible = xitk_skin_get_visibility(skonfig, private_data->skin_element_name);
      sl->enable  = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
      
      xitk_skin_unlock(skonfig);

      xitk_set_widget_pos(sl, sl->x, sl->y);
    }
  }
}

/*
 * Got click
 */
static int notify_click_slider (xitk_widget_list_t *wl, 
				xitk_widget_t *sl, int bUp, int x, int y) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    
    slider_update(sl, (x - sl->x), (y - sl->y));
    
    private_data->bClicked = !bUp;
    
    paint_slider(sl, wl->win, wl->gc);
    
    if(bUp==0 && private_data->bArmed) {
      XEvent sliderevent;

      /*
       * Exec motion callback function (if available)
       */

      if(private_data->motion_callback) {
	private_data->motion_callback(private_data->sWidget,
				      private_data->motion_userdata,
				      (int) private_data->value);
      }

      /*
       * Loop of death ;-)
       */
      do {

	XNextEvent (private_data->imlibdata->x.disp, &sliderevent) ;

	switch(sliderevent.type) {
	  
	case MotionNotify: {

	  while (XCheckMaskEvent (private_data->imlibdata->x.disp, ButtonMotionMask,
				  &sliderevent));

	  slider_update(sl, (sliderevent.xbutton.x - sl->x), (sliderevent.xbutton.y - sl->y));

	  paint_slider(sl, wl->win, wl->gc);

 	  if(private_data->motion_callback) {
	    private_data->motion_callback(private_data->sWidget,
					  private_data->motion_userdata,
					  (int) private_data->value);
	  }

	}
	break;

	case ButtonRelease:
	  private_data->bClicked = 0;

	  paint_slider(sl, wl->win, wl->gc);

	  if(private_data->callback) {
	    private_data->callback(private_data->sWidget,
				   private_data->userdata,
				   (int) private_data->value);
	  }

	  break;

	default:
	  xitk_xevent_notify (&sliderevent);
 	  break;
	}
	
      } while (sliderevent.type != ButtonRelease); 
    }

  }

  return 1;
}

/*
 * Got focus
 */
static int notify_focus_slider (xitk_widget_list_t *wl,
			 xitk_widget_t *sl, int bEntered) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER)
    private_data->bArmed = bEntered;    

  return 1;
}

/*
 * Increment position
 */
void xitk_slider_make_step(xitk_widget_list_t *wl, xitk_widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER && !private_data->bClicked) {
    
    if(((xitk_slider_get_pos(sl)) + private_data->step) <= (xitk_slider_get_max(sl)))
      xitk_slider_set_pos(wl, sl, xitk_slider_get_pos(sl) + private_data->step);

  } 

}

/*
 * Decrement position
 */
void xitk_slider_make_backstep(xitk_widget_list_t *wl, xitk_widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER && !private_data->bClicked) {
    
    if(((xitk_slider_get_pos(sl)) - private_data->step) >= (xitk_slider_get_min(sl)))
      xitk_slider_set_pos(wl, sl, xitk_slider_get_pos(sl) - private_data->step);

  } 

}

/*
 * Set value MIN.
 */
void xitk_slider_set_min(xitk_widget_t *sl, int min) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    slider_update_minmax(sl, (float)min, private_data->upper);
  }
}

/*
 * Return the MIN value
 */
int xitk_slider_get_min(xitk_widget_t *sl) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return (int) private_data->lower;
  } 

  return -1;
}

/*
 * Return the MAX value
 */
int xitk_slider_get_max(xitk_widget_t *sl) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return (int) private_data->upper;
  } 

  return -1;
}

/*
 * Set value MAX
 */
void xitk_slider_set_max(xitk_widget_t *sl, int max) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    slider_update_minmax(sl, private_data->lower, (float)max);
  } 
}

/*
 * Set pos to 0 and redraw the widget.
 */
void xitk_slider_reset(xitk_widget_list_t *wl, xitk_widget_t *sl) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    slider_update_value(sl, 0.0);
    private_data->bClicked = 0;
    paint_slider(sl, wl->win, wl->gc);
  }
}

/*
 * Set pos to max and redraw the widget.
 */
void xitk_slider_set_to_max(xitk_widget_list_t *wl, xitk_widget_t *sl) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER && !private_data->bClicked) {
    slider_update_value(sl, private_data->upper);
    paint_slider(sl, wl->win, wl->gc);
  }

}

/*
 * Return current position.
 */
int xitk_slider_get_pos(xitk_widget_t *sl) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return (int) private_data->value;
  } 

  return -1;
}

/*
 * Set position.
 */
void xitk_slider_set_pos(xitk_widget_list_t *wl, xitk_widget_t *sl, int pos) {
  slider_private_data_t *private_data = (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER && !private_data->bClicked) {
    float value = (float) pos;
    
    if (value >= private_data->lower && value <= private_data->upper) {
      slider_update_value(sl, value);
      paint_slider(sl, wl->win, wl->gc);
    }
    
  } 
}

/*
 * Create the widget
 */
static xitk_widget_t *_xitk_slider_create (xitk_skin_config_t *skonfig, xitk_slider_widget_t *s,
					   int x, int y, char *skin_element_name,
					   xitk_image_t *bg_skin, xitk_image_t *pad_skin,
					   int stype, int radius, int visible, int enable) {
  xitk_widget_t           *mywidget;
  slider_private_data_t   *private_data;

  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));

  private_data = (slider_private_data_t *) xitk_xmalloc (sizeof (slider_private_data_t));

  private_data->imlibdata                = s->imlibdata;
  private_data->skin_element_name        = (skin_element_name == NULL) ? NULL : strdup(s->skin_element_name);

  private_data->sWidget                  = mywidget;
  private_data->sType                    = stype;
  private_data->bClicked                 = 0;
  private_data->bArmed                   = 0;

  private_data->angle                    = 0.0;

  private_data->upper                    = (float)s->max;
  private_data->lower                    = (float)s->min;

  private_data->value                    = 0.0;
  private_data->percentage               = 0.0;
  private_data->radius                   = radius;

  private_data->step                     = s->step;

  private_data->paddle_skin              = pad_skin;
  private_data->button_width             = private_data->paddle_skin->width / 3;
  
  private_data->bg_skin                  = bg_skin;

  private_data->paddle_cover_bg          = 0;
  if(private_data->sType == XITK_HSLIDER) {
    if(private_data->button_width == private_data->bg_skin->width)
      private_data->paddle_cover_bg = 1;
  }
  else if(private_data->sType == XITK_VSLIDER) {
    if(private_data->paddle_skin->height == private_data->bg_skin->height)
      private_data->paddle_cover_bg = 1;
  }

  private_data->motion_callback          = s->motion_callback;
  private_data->motion_userdata          = s->motion_userdata;
  private_data->callback                 = s->callback;
  private_data->userdata                 = s->userdata;

  mywidget->private_data                 = private_data;

  mywidget->enable                       = enable;
  mywidget->running                      = 1;
  mywidget->visible                      = visible;
  mywidget->have_focus                   = FOCUS_LOST;
  mywidget->imlibdata                    = private_data->imlibdata;
  mywidget->x                            = x;
  mywidget->y                            = y;
  mywidget->width                        = private_data->bg_skin->width;
  mywidget->height                       = private_data->bg_skin->height;
  mywidget->widget_type                  = WIDGET_TYPE_SLIDER;
  mywidget->paint                        = paint_slider;
  mywidget->notify_click                 = notify_click_slider;
  mywidget->notify_focus                 = notify_focus_slider;
  mywidget->notify_keyevent              = NULL;
  mywidget->notify_inside                = notify_inside;
  mywidget->notify_change_skin           = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy               = notify_destroy;
  mywidget->get_skin                     = get_skin;

  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;

  return mywidget;
}

/*
 * Create the widget
 */
xitk_widget_t *xitk_slider_create (xitk_skin_config_t *skonfig, xitk_slider_widget_t *s) {

  XITK_CHECK_CONSTITENCY(s);
  
  return _xitk_slider_create(skonfig, s,
			     (xitk_skin_get_coord_x(skonfig, s->skin_element_name)),
			     (xitk_skin_get_coord_y(skonfig, s->skin_element_name)),
			     s->skin_element_name,
			     (xitk_image_load_image(s->imlibdata, 
						    xitk_skin_get_skin_filename(skonfig, s->skin_element_name))),
			     (xitk_image_load_image(s->imlibdata, 
						    xitk_skin_get_slider_skin_filename(skonfig, s->skin_element_name))),
			     (xitk_skin_get_slider_type(skonfig, s->skin_element_name)),
			     (xitk_skin_get_slider_radius(skonfig, s->skin_element_name)),
			     (xitk_skin_get_visibility(skonfig, s->skin_element_name)),
			     (xitk_skin_get_enability(skonfig, s->skin_element_name)));
}

/*
 * Create the widget (not skined).
 */
xitk_widget_t *xitk_noskin_slider_create (xitk_slider_widget_t *s,
					  int x, int y, int width, int height, int type) {
  xitk_image_t   *b, *p;
  int             radius;

  XITK_CHECK_CONSTITENCY(s);
  
  if(type == XITK_VSLIDER)
    p = xitk_image_create_image(s->imlibdata, width * 3, height / 5);
  else if(type == XITK_HSLIDER)
    p = xitk_image_create_image(s->imlibdata, (width / 5) * 3, height);
  else if(type == XITK_RSLIDER) {
    int w;
    
    w = ((((width + height) >> 1) >> 1) / 10) * 3;
    p = xitk_image_create_image(s->imlibdata, (w * 3), w);
  }
  else
    XITK_DIE("Slider type unhandled.\n");
  
  xitk_image_add_mask(s->imlibdata, p);
  if(type == XITK_VSLIDER)
    draw_paddle_three_state_vertical(s->imlibdata, p);
  else if(type == XITK_HSLIDER)
    draw_paddle_three_state_horizontal(s->imlibdata, p);
  else if(type == XITK_RSLIDER) {
    draw_paddle_rotate(s->imlibdata, p);
  }
  
  b = xitk_image_create_image(s->imlibdata, width, height);
  xitk_image_add_mask(s->imlibdata, b);
  if((type == XITK_HSLIDER) || (type == XITK_VSLIDER))
    draw_inner(s->imlibdata, b->image, width, height);
  else if(type == XITK_RSLIDER) {
    draw_rotate_button(s->imlibdata, b);
  }
  
  radius = (b->height >> 1) - (p->height);

  return _xitk_slider_create(NULL, s, x, y, NULL, b, p, type, radius, 1, 1);
}
