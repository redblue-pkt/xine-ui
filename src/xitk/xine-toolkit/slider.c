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
  float                  range;

  if(value < private_data->lower)
    private_data->value = private_data->lower;
  else if(value > private_data->upper)
    private_data->value = private_data->upper;
  else
    private_data->value = value;

  range = (private_data->upper + -private_data->lower);
  
  private_data->percentage = (private_data->value + -private_data->lower) / range;
  private_data->angle = 7.*M_PI/6. - (private_data->value - private_data->lower) * 
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
  
  private_data->upper = (min == max) ? max + 1 : max;
  private_data->lower = min;
  slider_update_value(w, private_data->value);
}

/*
 *
 */
static void slider_update(xitk_widget_t *w, int x, int y) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;
  
  if(private_data->sType == XITK_RSLIDER) {
    int   xc, yc;
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
    
    if(private_data->paddle_cover_bg == 1) {

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

    }
    else {
      int pwidth, pheight;
    
      pwidth = private_data->button_width;
      pheight = private_data->paddle_skin->height;
      
      if(private_data->sType == XITK_HSLIDER) {
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

      if(private_data->sType == XITK_HSLIDER)
	new_value = (x * .01) / ((width - pwidth) * .01);
      else if(private_data->sType == XITK_VSLIDER)
	new_value = ((height - y) * .01) / ((height - pheight) * .01);
    }
    
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
  slider_private_data_t *private_data;

  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;

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
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    
    private_data = (slider_private_data_t *) w->private_data;
    
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
static int notify_inside(xitk_widget_t *w, int x, int y) {
  slider_private_data_t *private_data = (slider_private_data_t *) w->private_data;

  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    
    private_data = (slider_private_data_t *) w->private_data;
    
    if((w->visible == 1)) {
      xitk_image_t *skin;
      
      if(private_data->paddle_cover_bg == 1)
	skin = private_data->paddle_skin;
      else
	skin = private_data->bg_skin;

      if(skin->mask)
	return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, w, skin->mask->pixmap, x, y);
    }
    else
      return 0;
  }

  return 1;
}

/*
 * Draw widget
 */
static void paint_slider(xitk_widget_t *w, Window win, GC gc) {
  int                     button_width, button_height;	
  slider_private_data_t  *private_data;
  GC                      bgc, pgc;
  xitk_image_t           *bg;
  xitk_image_t           *paddle;
  
  if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) && (w->visible == 1))) {
    int    x, y, srcx1, srcx2, destx1, srcy1, srcy2, desty1;
    int    xcenter, ycenter;
    int    paddle_width;
    int    paddle_height;
    double angle;
    
    private_data = (slider_private_data_t *) w->private_data;
    bg = (xitk_image_t *) private_data->bg_skin;
    paddle = (xitk_image_t *) private_data->paddle_skin;

    x = y = srcx1 = srcx2 = destx1 = srcy1 = srcy2 = desty1 = 0;
        
    XLOCK (private_data->imlibdata->x.disp);

    bgc = XCreateGC(private_data->imlibdata->x.disp, bg->image->pixmap, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, bgc);
    pgc = XCreateGC(private_data->imlibdata->x.disp, paddle->image->pixmap, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, pgc);
      
    if(bg->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, bgc, w->x, w->y);
      XSetClipMask(private_data->imlibdata->x.disp, bgc, bg->mask->pixmap);
    }
    
    XCopyArea(private_data->imlibdata->x.disp, bg->image->pixmap, win, bgc, 0, 0,
	      bg->width, bg->height, w->x, w->y);
      
    XUNLOCK(private_data->imlibdata->x.disp);

    if(private_data->sType == XITK_RSLIDER) {
      
      button_width = private_data->bg_skin->width;
      button_height = private_data->bg_skin->height;

      xcenter       = (bg->width /2) + w->x;
      ycenter       = (bg->width /2) + w->y;
      paddle_width  = paddle->width / 3;
      paddle_height = paddle->height;
      angle         = private_data->angle;
      
      if(angle < M_PI / -2)
	angle = angle + M_PI * 2;
      
      x = (int) (0.5 + xcenter + private_data->radius * cos(angle)) - (paddle_width / 2);
      y = (int) (0.5 + ycenter - private_data->radius * sin(angle)) - (paddle_height / 2);

      srcx1 = 0;
      
      if((private_data->focus == FOCUS_RECEIVED) || (private_data->focus == FOCUS_MOUSE_IN)) {
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
	w->width - button_width : w->height - button_height;
	
      tmp = (dir_area * .01) * (private_data->percentage * 100.0);
      
      if(private_data->sType == XITK_HSLIDER) {
	x = rint(w->x + tmp);
	y = w->y;
      }
      else if(private_data->sType == XITK_VSLIDER) {
	x = w->x;
	y = rint(w->y + private_data->bg_skin->height - button_height - tmp);
      }
      
      if(private_data->paddle_cover_bg == 1) {
	int pixpos;
	int direction;
	
	x = w->x;
	y = w->y;
	
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
	  destx1 = w->x;
	  desty1 = w->y + pixpos;
	}
	else if(private_data->sType == XITK_HSLIDER) {
	  srcx1  = 0;
	  srcy1  = 0;
	  srcx2  = pixpos;
	  srcy2  = paddle->height;
	  destx1 = w->x;
	  desty1 = w->y;
	}
      }
      else {
	srcy1  = 0;
	srcx2  = button_width;
	srcy2  = paddle->height;
	destx1 = x;
	desty1 = y;
      }
      
      if((private_data->focus == FOCUS_RECEIVED) || (private_data->focus == FOCUS_MOUSE_IN)) {
	if(private_data->bClicked)
	  srcx1 = 2*button_width;
	else
	  srcx1 = button_width;
      }

    }
    
    XLOCK(private_data->imlibdata->x.disp);
    if(paddle->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, pgc, x, y);
      XSetClipMask(private_data->imlibdata->x.disp, pgc, paddle->mask->pixmap);
    }
    
    XCopyArea(private_data->imlibdata->x.disp, paddle->image->pixmap, win, pgc,
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
			       xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    
    private_data = (slider_private_data_t *) w->private_data;
    
    if(private_data->skin_element_name) {

      xitk_skin_lock(skonfig);

      xitk_image_free_image(private_data->imlibdata, &private_data->paddle_skin);
      private_data->paddle_skin     = xitk_image_load_image(private_data->imlibdata, xitk_skin_get_slider_skin_filename(skonfig, private_data->skin_element_name));
      private_data->button_width    = private_data->paddle_skin->width / 3;
      xitk_image_free_image(private_data->imlibdata, &private_data->bg_skin);
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

      w->x       = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      w->y       = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      w->width   = private_data->bg_skin->width;
      w->height  = private_data->bg_skin->height;
      w->visible = (xitk_skin_get_visibility(skonfig, private_data->skin_element_name)) ? 1 : -1;
      w->enable  = xitk_skin_get_enability(skonfig, private_data->skin_element_name);
      
      xitk_skin_unlock(skonfig);

      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

/*
 * Got click
 */
static int notify_click_slider(xitk_widget_list_t *wl, xitk_widget_t *w, int bUp, int x, int y) {
  slider_private_data_t *private_data;
    
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    
    private_data = (slider_private_data_t *) w->private_data;    
    
    slider_update(w, (x - w->x), (y - w->y));
    
    private_data->bClicked = !bUp;
    
    paint_slider(w, wl->win, wl->gc);
    
    if((bUp == 0) && (private_data->focus == FOCUS_RECEIVED)) {
      XEvent sliderevent;
      int new_value = (int) private_data->value;
      int old_value = !new_value;
      
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

	XNextEvent(private_data->imlibdata->x.disp, &sliderevent) ;

	switch(sliderevent.type) {
	  
	case MotionNotify: {

	  while(XCheckMaskEvent (private_data->imlibdata->x.disp, ButtonMotionMask,
				 &sliderevent));

	  slider_update(w, (sliderevent.xbutton.x - w->x), (sliderevent.xbutton.y - w->y));	
	  new_value = (int) private_data->value;

	  if(new_value != old_value) {
	    old_value = new_value;
	    paint_slider(w, wl->win, wl->gc);
	  }
	  
 	  if(private_data->motion_callback) {
	    private_data->motion_callback(private_data->sWidget,
					  private_data->motion_userdata,
					  (int) private_data->value);
	  }

	}
	break;

	case ButtonRelease: {
	  private_data->bClicked = 0;
	  
	  xitk_slider_set_pos(wl, w, private_data->value);
	  
	  if(private_data->callback) {
	    private_data->callback(private_data->sWidget,
				   private_data->userdata,
				   (int) private_data->value);
	  }
	}
	break;

	default:
	  xitk_xevent_notify (&sliderevent);
 	  break;
	}
	
      } while(sliderevent.type != ButtonRelease); 
    }

  }

  return 1;
}

/*
 * Got focus
 */
static int notify_focus_slider(xitk_widget_list_t *wl, xitk_widget_t *w, int focus) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    private_data->focus = focus;
  }

  return 1;
}

/*
 * Increment position
 */
void xitk_slider_make_step(xitk_widget_list_t *wl, xitk_widget_t *w) {
  slider_private_data_t *private_data;

  if (w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    if(!private_data->bClicked) {
      if(((xitk_slider_get_pos(w)) + private_data->step) <= (xitk_slider_get_max(w)))
	xitk_slider_set_pos(wl, w, xitk_slider_get_pos(w) + private_data->step);
    }
  } 

}

/*
 * Decrement position
 */
void xitk_slider_make_backstep(xitk_widget_list_t *wl, xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    if(!private_data->bClicked) {
      if(((xitk_slider_get_pos(w)) - private_data->step) >= (xitk_slider_get_min(w)))
	xitk_slider_set_pos(wl, w, xitk_slider_get_pos(w) - private_data->step);
    }
    
  } 

}

/*
 * Set value MIN.
 */
void xitk_slider_set_min(xitk_widget_t *w, int min) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    slider_update_minmax(w, (float)((min == private_data->upper) 
				    ? min - 1 : min), private_data->upper);
  }
}

/*
 * Return the MIN value
 */
int xitk_slider_get_min(xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) {
    private_data = (slider_private_data_t *) w->private_data;
    return (int) private_data->lower;
  } 

  return -1;
}

/*
 * Return the MAX value
 */
int xitk_slider_get_max(xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    return (int) private_data->upper;
  } 

  return -1;
}

/*
 * Set value MAX
 */
void xitk_slider_set_max(xitk_widget_t *w, int max) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    slider_update_minmax(w, private_data->lower, (float)((max == private_data->lower) 
							 ? max + 1 : max));
  } 
}

/*
 * Set pos to 0 and redraw the widget.
 */
void xitk_slider_reset(xitk_widget_list_t *wl, xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    slider_update_value(w, 0.0);
    private_data->bClicked = 0;
    paint_slider(w, wl->win, wl->gc);
  }
}

/*
 * Set pos to max and redraw the widget.
 */
void xitk_slider_set_to_max(xitk_widget_list_t *wl, xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    if(!private_data->bClicked) {
      slider_update_value(w, private_data->upper);
      paint_slider(w, wl->win, wl->gc);
    }
  }
}

/*
 * Return current position.
 */
int xitk_slider_get_pos(xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;
    return (int) private_data->value;
  } 

  return -1;
}

/*
 * Set position.
 */
void xitk_slider_set_pos(xitk_widget_list_t *wl, xitk_widget_t *w, int pos) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    float value = (float) pos;
    
    private_data = (slider_private_data_t *) w->private_data;

    if(!private_data->bClicked) {
      if(value >= private_data->lower && value <= private_data->upper) {
	slider_update_value(w, value);
	paint_slider(w, wl->win, wl->gc);
      }
    }
    
  } 
}

/*
 * Call callback for current position
 */
void xitk_slider_callback_exec(xitk_widget_t *w) {
  slider_private_data_t *private_data;
  
  if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    private_data = (slider_private_data_t *) w->private_data;

    if(private_data->callback) {
      private_data->callback(private_data->sWidget,
			     private_data->userdata,
			     (int) private_data->value);
    }
    else {

      if(private_data->motion_callback) {
	private_data->motion_callback(private_data->sWidget,
				      private_data->motion_userdata,
				      (int) private_data->value);
      }

    }
  }
}

/*
 * Create the widget
 */
static xitk_widget_t *_xitk_slider_create(xitk_widget_list_t *wl,
					  xitk_skin_config_t *skonfig, xitk_slider_widget_t *s,
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
  private_data->focus                    = FOCUS_LOST;

  private_data->angle                    = 0.0;

  private_data->upper                    = (float)((s->min == s->max) ? s->max + 1 : s->max);
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

  mywidget->widget_list                  = wl;

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
  mywidget->notify_enable                = NULL;

  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;

  return mywidget;
}

/*
 * Create the widget
 */
xitk_widget_t *xitk_slider_create(xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_slider_widget_t *s) {

  XITK_CHECK_CONSTITENCY(s);
  
  return _xitk_slider_create(wl, skonfig, s,
			     (xitk_skin_get_coord_x(skonfig, s->skin_element_name)),
			     (xitk_skin_get_coord_y(skonfig, s->skin_element_name)),
			     s->skin_element_name,
			     (xitk_image_load_image(s->imlibdata, 
						    xitk_skin_get_skin_filename(skonfig, s->skin_element_name))),
			     (xitk_image_load_image(s->imlibdata, 
						    xitk_skin_get_slider_skin_filename(skonfig, s->skin_element_name))),
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

  return _xitk_slider_create(wl,NULL, s, x, y, NULL, b, p, type, radius, 1, 1);
}
