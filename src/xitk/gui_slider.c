/* 
 * Copyright (C) 2000 the xine project
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
#include <pthread.h>
#include "xine.h"
#include "gui_widget.h"
#include "gui_slider.h"
#include "gui_widget_types.h"
#include "gui_main.h"
#include "utils.h"

extern gGlob_t         *gGlob;
extern uint32_t         xine_debug;

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
       else                                                                \
         fprintf(stderr, "Unknown slider type (%d)\n",                     \
                 private_data->sType);                                     \
                                                                           \
       if(private_data->pos > private_data->max) {                         \
         fprintf(stderr, "slider POS > MAX!.\n");                          \
         private_data->pos = private_data->max;                            \
       }                                                                   \
       if(private_data->pos < private_data->min) {                         \
         fprintf(stderr, "slider POS < MIN!.\n");                          \
         private_data->pos = private_data->min;                            \
       }                                                                   \
     }

/* ------------------------------------------------------------------------- */
/*
 * Draw widget
 */
void paint_slider (widget_t *sl, Window win, GC gc) {
  float tmp = 0L;
  int button_width, button_height;	
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  gui_image_t *bg = (gui_image_t *) private_data->bg_skin;
  gui_image_t *paddle = (gui_image_t *) private_data->paddle_skin;
  
  XLockDisplay (gGlob->gDisplay);

  if(private_data->pos > private_data->max
     || private_data->pos < private_data->min)
    return;

  button_width = private_data->button_width;
  button_height = private_data->paddle_skin->height;
  
  if(private_data->sType == HSLIDER)
    tmp = sl->width - button_width;
  if(private_data->sType == VSLIDER)
    tmp = sl->height - button_height;

  tmp /= private_data->max;
  tmp *= private_data->pos;

  XCopyArea (gGlob->gDisplay, bg->image, win, gc, 0, 0,
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
	XCopyArea (gGlob->gDisplay, paddle->image, win, gc, 2*button_width, 0,
		   button_width, paddle->height, x, y);
	
      } else {
	XCopyArea (gGlob->gDisplay, paddle->image, win, gc, button_width, 0,
		   button_width, paddle->height, x, y);
      }
    } else {
      XCopyArea (gGlob->gDisplay, paddle->image, win, gc, 0, 0,
		 button_width, paddle->height, x, y);
    }
    XSetForeground(gGlob->gDisplay, gc, gui_color.white.pixel);
    
    XFlush (gGlob->gDisplay);
    
  } 
  else
    fprintf (stderr, "paint slider on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  
  XUnlockDisplay (gGlob->gDisplay);

}
/* ------------------------------------------------------------------------- */
/*
 * Got click
 */
int notify_click_slider (widget_list_t *wl, 
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
      if(private_data->mfunction) {
	if(private_data->realmin < 0)
	  retpos = (private_data->realmin + private_data->pos);
	else
	  retpos = private_data->pos;
	private_data->mfunction (private_data->sWidget,
				 private_data->muser_data,
				 retpos);
      }

      /*
       * Loop of death ;-)
       */
      do {
	XNextEvent (gGlob->gDisplay, &sliderevent) ;
	
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
	  
 	  if(private_data->mfunction) {
	    private_data->mfunction (private_data->sWidget,
				     private_data->muser_data,
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
	    if(private_data->function) {
	      private_data->function (private_data->sWidget,
				      private_data->user_data,
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
	    if(private_data->function && private_data->mfunction) {
	      private_data->function (private_data->sWidget,
				      private_data->user_data,
				      private_data->pos);
	    }
	  }
	  break;
	}
      } while (sliderevent.type != ButtonRelease); 
    }
    paint_widget_list (wl);
  }
  else
    fprintf (stderr, "notify click slider on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  return 1;
}
/* ------------------------------------------------------------------------- */
/*
 * Got focus
 */
int notify_focus_slider (widget_list_t *wl, widget_t *sl, int bEntered) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;

  if (sl->widget_type & WIDGET_TYPE_SLIDER)
    private_data->bArmed = bEntered;    
  else
    fprintf (stderr, "notify slider button on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  return 1;
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf (stderr, "slider_make_step on something (%d) "
	     "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf (stderr, "slider_make_step on something (%d) "
	     "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
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
  } else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
/*
 * Return the MIN value
 */
int slider_get_min(widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return private_data->realmin;
  } 
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  return -1;
}
/* ------------------------------------------------------------------------- */
/*
 * Return the MAX value
 */
int slider_get_max(widget_t *sl) {
  slider_private_data_t *private_data = 
    (slider_private_data_t *) sl->private_data;
  
  if (sl->widget_type & WIDGET_TYPE_SLIDER) {
    return private_data->max;
  } 
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  return -1;
}
/* ------------------------------------------------------------------------- */
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
      else
	fprintf(stderr, "Unknown slider type (%d)\n", 
		private_data->sType);
    }
  } 
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf(stderr, "slider_set_min on something (%d) "
	    "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf (stderr, "slider_get_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
  return -1;
}
/* ------------------------------------------------------------------------- */
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
  else
    fprintf (stderr, "slider_set_pos on something (%d) "
	     "that is not a slider\n", sl->widget_type);
}
/* ------------------------------------------------------------------------- */
/*
 * Create the widget
 */
widget_t *create_slider (int type, int x, int y, int min, int max, 
			 int step, const char *bg, const char *paddle,
			 void *fm, void *udm, void *f, void *ud) {
  widget_t                *mywidget;
  slider_private_data_t   *private_data;

  mywidget = (widget_t *) xmalloc (sizeof(widget_t));
  private_data = (slider_private_data_t *) 
    xmalloc (sizeof (slider_private_data_t));
  
  private_data->sWidget      = mywidget;
  private_data->sType        = type;
  private_data->bClicked     = 0;
  private_data->bArmed       = 0;
  private_data->min          = min;

  if(max <= min) 
    private_data->max        = min+1;
  else
    private_data->max        = max;

  if(min < 0) {
    private_data->realmin = min;
    private_data->min = 0;
    private_data->realmax = max;
    private_data->max = max - min;
  }
  else
    private_data->realmin = min;
  
  private_data->pos          = 0;
  private_data->step         = step;
  private_data->paddle_skin  = gui_load_image(paddle);
  private_data->button_width = private_data->paddle_skin->width / 3;

  private_data->bg_skin      = gui_load_image(bg);

  if(type == HSLIDER)
    private_data->ratio      = (float)(private_data->max - private_data->min)/private_data->bg_skin->width;
  else if(type == VSLIDER)
    private_data->ratio      = (float)(private_data->max - private_data->min)/private_data->bg_skin->height;
  else
    fprintf(stderr, "Unknown slider type (%d)\n", type);

  private_data->mfunction    = fm;
  private_data->muser_data   = udm;
  private_data->function     = f;
  private_data->user_data    = ud;

  mywidget->private_data     = private_data;

  mywidget->enable           = 1;

  mywidget->x                = x;
  mywidget->y                = y;

  mywidget->width            = private_data->bg_skin->width;
  mywidget->height           = private_data->bg_skin->height;
  mywidget->widget_type      = WIDGET_TYPE_SLIDER;
  mywidget->paint            = paint_slider;
  mywidget->notify_click     = notify_click_slider;
  mywidget->notify_focus     = notify_focus_slider;

  return mywidget;
}
/* ------------------------------------------------------------------------- */
