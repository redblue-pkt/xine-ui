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

#include "Imlib-light/Imlib.h"
#include "image.h"
#include "widget.h"
#include "slider.h"
#include "widget_types.h"
#include "_xitk.h"

#ifdef DEBUG_GUI
#define COMPUTE_COORDS(X,Y)                                                \
     {                                                                     \
       if(private_data->sType == HSLIDER) {                                \
         private_data->pos = (int) ((X - sl->x) * private_data->ratio);    \
       }                                                                   \
       else if(private_data->sType == VSLIDER) {                           \
         private_data->pos = (int) ((private_data->bg_skin->height         \
				- (Y - sl->y))                             \
                                * private_data->ratio);                    \
       }                                                                   \
       else {                                                              \
         fprintf(stderr, "Unknown slider type (%d)\n",                     \
                 private_data->sType);                                     \
       }                                                                   \
       if(private_data->pos > private_data->max) {                         \
         fprintf(stderr, "slider POS > MAX!.\n");                          \
         private_data->pos = private_data->max;                            \
       }                                                                   \
       if(private_data->pos < private_data->min) {                         \
         fprintf(stderr, "slider POS < MIN!.\n");                          \
         private_data->pos = private_data->min;                            \
       }                                                                   \
     }
#else
#define COMPUTE_COORDS(X,Y)                                                \
     {                                                                     \
       if(private_data->sType == HSLIDER) {                                \
         private_data->pos = (int) ((X - sl->x) * private_data->ratio);    \
       }                                                                   \
       else if(private_data->sType == VSLIDER) {                           \
         private_data->pos = (int) ((private_data->bg_skin->height         \
				- (Y - sl->y))                             \
                                * private_data->ratio);                    \
       }                                                                   \
       if(private_data->pos > private_data->max) {                         \
         private_data->pos = private_data->max;                            \
       }                                                                   \
       if(private_data->pos < private_data->min) {                         \
         private_data->pos = private_data->min;                            \
       }                                                                   \
     }
#endif

/*
 * Draw widget
 */
static void paint_slider (widget_t *sl, Window win, GC gc) {
  float                  tmp = 0L;
  int                    button_width, button_height;	
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  gui_image_t           *bg = (gui_image_t *) private_data->bg_skin;
  gui_image_t           *paddle = (gui_image_t *) private_data->paddle_skin;
  gui_color_t            gui_color;
  
  if(private_data->pos > private_data->max
     || private_data->pos < private_data->min)
    return;

  XLOCK (private_data->display);

  button_width = private_data->button_width;
  button_height = private_data->paddle_skin->height;
  
  if(private_data->sType == HSLIDER)
    tmp = sl->width - button_width;
  if(private_data->sType == VSLIDER)
    tmp = sl->height - button_height;

  tmp /= private_data->max;
  tmp *= private_data->pos;

  XCopyArea (private_data->display, bg->image, win, gc, 0, 0,
	     bg->width, bg->height, sl->x, sl->y);
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    int x=0, y=0;
    if(private_data->sType == HSLIDER) {
      x = rint(sl->x + tmp);
      y = sl->y;
    }
    else if(private_data->sType == VSLIDER) {
      x = sl->x;
      y = rint(sl->y + private_data->bg_skin->height - button_height - tmp);
    }
    if (private_data->bArmed) {
      if (private_data->bClicked) {
	XCopyArea (private_data->display, paddle->image, 
		   win, gc, 2*button_width, 0,
		   button_width, paddle->height, x, y);
	
      } else {
	XCopyArea (private_data->display, paddle->image, 
		   win, gc, button_width, 0,
		   button_width, paddle->height, x, y);
      }
    } else {
      XCopyArea (private_data->display, paddle->image, win, gc, 0, 0,
		 button_width, paddle->height, x, y);
    }
    XSetForeground(private_data->display, gc, gui_color.white.pixel);
    
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint slider on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif

    
  XUNLOCK (private_data->display);
  /* XSync (private_data->display, False); */
}

/*
 * Got click
 */
static int notify_click_slider (widget_list_t *wl, 
				widget_t *sl, int bUp, int x, int y) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  int original_pos = private_data->pos;
  int retpos;

  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    
    COMPUTE_COORDS(x, y);

    private_data->bClicked = !bUp;
    
    paint_slider(sl, wl->win, wl->gc);
    
    if(bUp==0 && private_data->bArmed) {
      XEvent sliderevent;

      /*
       * Exec motion callback function (if available)
       */
      if(private_data->motion_callback) {
	if(private_data->realmin < 0)
	  retpos = (private_data->realmin + private_data->pos);
	else
	  retpos = private_data->pos;
	private_data->motion_callback(private_data->sWidget,
				      private_data->motion_userdata,
				      retpos);
      }

      /*
       * Loop of death ;-)
       */
      do {
	XNextEvent (private_data->display, &sliderevent) ;
	
	switch(sliderevent.type) {
	  
	case MotionNotify:
	  
	  COMPUTE_COORDS(sliderevent.xbutton.x, sliderevent.xbutton.y);
	  
	  paint_slider(sl, wl->win, wl->gc);
	 
	  /*
	   * Callback exec on all of motion events
	   */
	  if(private_data->realmin < 0) {
	    retpos = (private_data->realmin + private_data->pos);
	  }
	  else
	    retpos = private_data->pos;
	  
 	  if(private_data->motion_callback) {
	    private_data->motion_callback(private_data->sWidget,
					  private_data->motion_userdata,
					  retpos);
	  }
	  break;

	case ButtonRelease:
	  private_data->bClicked = 0;
	  /*
	   * Perform callback exec if button release is 
	   * in widget area.
	   */
	  if(is_inside_widget(sl, sliderevent.xbutton.x, 
			      sliderevent.xbutton.y)) {
	    if(private_data->realmin < 0)
	      retpos = (private_data->realmin + private_data->pos);
	    else
	      retpos = private_data->pos;
	    if(private_data->callback) {
	      private_data->callback(private_data->sWidget,
				     private_data->userdata,
				     retpos);
	    }
	  }
	  else {
	    /*
	     * Restoring original position of paddle.
	     */
	    private_data->bArmed = 0;
	    private_data->pos = original_pos;
	    paint_slider(sl, wl->win, wl->gc);
	    if(private_data->callback && private_data->motion_callback) {
	      private_data->callback(private_data->sWidget,
				     private_data->userdata,
				     private_data->pos);
	    }
	  }
	  break;
	}
      } while (sliderevent.type != ButtonRelease); 
    }
    paint_widget_list (wl);
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click slider on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif

  return 1;
}

/*
 * Got focus
 */
static int notify_focus_slider (widget_list_t *wl,
			 widget_t *sl, int bEntered) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER)
    private_data->bArmed = bEntered;    
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify slider button on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif

  return 1;
}

/*
 * Increment position
 */
void slider_make_step(widget_list_t *wl, widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER
      && !private_data->bClicked) {
    if((slider_get_pos(sl) + private_data->step) <= private_data->max)
      slider_set_pos(wl, sl, slider_get_pos(sl) + private_data->step);
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "slider_make_step on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Decrement position
 */
void slider_make_backstep(widget_list_t *wl, widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER
      && !private_data->bClicked) {
    if((slider_get_pos(sl) - private_data->step) >= private_data->min)
      slider_set_pos(wl, sl, slider_get_pos(sl) - private_data->step);
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "slider_make_step on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Set value MIN.
 */
void slider_set_min(widget_t *sl, int min) {
  slider_private_data_t *private_data =
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    if(min < private_data->max) {
      private_data->min = min;
      if(private_data->sType == HSLIDER)
	private_data->ratio = (float) 
	  (private_data->max-private_data->min)/private_data->bg_skin->width;
      else if(private_data->sType == VSLIDER)
	private_data->ratio = (float)
	  (private_data->max-private_data->min)/private_data->bg_skin->height;
      else
	fprintf(stderr, "Unknown slider type (%d)\n", 
		private_data->sType);
    }
  }
#ifdef DEBUG_GUI
 else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Return the MIN value
 */
int slider_get_min(widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return private_data->realmin;
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif
  return -1;
}

/*
 * Return the MAX value
 */
int slider_get_max(widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return private_data->max;
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif
  return -1;
}

/*
 * Set value MAX
 */
void slider_set_max(widget_t *sl, int max) {
  slider_private_data_t *private_data =
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    if(max > private_data->min) {
      private_data->max = max;
      if(private_data->sType == HSLIDER)
	private_data->ratio = (float) 
	  (private_data->max-private_data->min)/private_data->bg_skin->width;
      else if(private_data->sType == VSLIDER)
	private_data->ratio = (float)
	  (private_data->max-private_data->min)/private_data->bg_skin->height;
#ifdef DEBUG_GUI
      else
	fprintf(stderr, "Unknown slider type (%d)\n", 
		private_data->sType);
#endif
    }
  } 
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Set pos to 0 and redraw the widget.
 */
void slider_reset(widget_list_t *wl, widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER
      && !private_data->bClicked) {
    if(private_data->realmin < 0)
      private_data->pos = (0 - private_data->realmin);
    else
      private_data->pos = 0;

    paint_slider(sl, wl->win, wl->gc);
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Set pos to max and redraw the widget.
 */
void slider_set_to_max(widget_list_t *wl, widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER
      && !private_data->bClicked) {
    if(private_data->realmin < 0)
      private_data->pos = (private_data->max - private_data->realmin);
    else
      private_data->pos = private_data->max;

    paint_slider(sl, wl->win, wl->gc);
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
#endif
}

/*
 * Return current position.
 */
int slider_get_pos(widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    if(private_data->realmin< 0)
      return (private_data->realmin + private_data->pos);
    else
      return private_data->pos;
  } 
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
#endif
  return -1;
}

/*
 * Set position.
 */
void slider_set_pos(widget_list_t *wl, widget_t *sl, int pos) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER
      && !private_data->bClicked) {
    if (pos >= private_data->realmin && pos <= private_data->max) {
      if(private_data->realmin < 0) {
	private_data->pos = pos - private_data->realmin;
      }
      else
	private_data->pos = pos;

      paint_slider(sl, wl->win, wl->gc);
    }
    else
      slider_reset(wl, sl);    
  } 
#ifdef DEBUG_GUI
  else {
    if(!(sl->widget_type & WIDGET_TYPE_SLIDER))
      fprintf (stderr, "slider_set_pos on something (%d) "
	       "that is not a slider\n", sl->widget_type);
  }
#endif
}

/*
 * Create the widget
 */
widget_t *slider_create (xitk_slider_t *s) {
  widget_t                *mywidget;
  slider_private_data_t   *private_data;

  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));
  private_data = (slider_private_data_t *) 
    gui_xmalloc (sizeof (slider_private_data_t));

  private_data->display         = s->display;
  
  private_data->sWidget         = mywidget;
  private_data->sType           = s->slider_type;
  private_data->bClicked        = 0;
  private_data->bArmed          = 0;
  private_data->min             = s->min;

  if(s->max <= s->min) 
    private_data->max           = s->min + 1;
  else
    private_data->max           = s->max;

  if(s->min < 0) {
    private_data->realmin       = s->min;
    private_data->min           = 0;
    private_data->realmax       = s->max;
    private_data->max           = s->max - s->min;
  }
  else
    private_data->realmin       = s->min;
  
  private_data->pos             = 0;
  private_data->step            = s->step;
  private_data->paddle_skin     = gui_load_image(s->imlibdata, s->paddle_skin);
  private_data->button_width    = private_data->paddle_skin->width / 3;

  private_data->bg_skin         = gui_load_image(s->imlibdata, s->background_skin);

  if(s->slider_type == HSLIDER)
    private_data->ratio         = (float)(private_data->max - private_data->min)/private_data->bg_skin->width;
  else if(s->slider_type == VSLIDER)
    private_data->ratio         = (float)(private_data->max - private_data->min)/private_data->bg_skin->height;
  else
    fprintf(stderr, "Unknown slider type (%d)\n", s->slider_type);

  private_data->motion_callback = s->motion_callback;
  private_data->motion_userdata = s->motion_userdata;
  private_data->callback        = s->callback;
  private_data->userdata        = s->userdata;

  mywidget->private_data        = private_data;

  mywidget->enable              = 1;
  mywidget->have_focus          = FOCUS_LOST;
  mywidget->x                   = s->x;
  mywidget->y                   = s->y;
  mywidget->width               = private_data->bg_skin->width;
  mywidget->height              = private_data->bg_skin->height;
  mywidget->widget_type         = WIDGET_TYPE_SLIDER;
  mywidget->paint               = paint_slider;
  mywidget->notify_click        = notify_click_slider;
  mywidget->notify_focus        = notify_focus_slider;
  mywidget->notify_keyevent     = NULL;

  return mywidget;
}
