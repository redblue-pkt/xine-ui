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
#include <stdlib.h>
#include <string.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "labelbutton.h"
#include "widget_types.h"

#include "_xitk.h"

#define CLICK     1
#define FOCUS     2
#define NORMAL    3

/*
 *
 */
static int notify_inside(widget_t *lb, int x, int y) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if ((lb->widget_type & WIDGET_TYPE_LABELBUTTON) && lb->visible) {
    gui_image_t *skin = private_data->skin;
    
    return widget_is_cursor_out_mask(private_data->display, lb, skin->mask, x, y);
  }

  return 1;
}

/*
 * Draw the string in pixmap pix, then return it
 */
static Pixmap create_labelofbutton(widget_t *lb, 
				   Window win, GC gc, Pixmap pix, 
				   int xsize, int ysize, 
				   char *label, int state) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  XFontStruct *fs = NULL;
  XCharStruct cs;
  int dir, as, des, len, xoff = 0, yoff = 0, DefaultColor = -1;
  unsigned int fg;
  XColor color;
  gui_color_names_t *gColor = NULL;

  color.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */
  if(private_data->fontname)
    fs = XLoadQueryFont(private_data->display, private_data->fontname);

  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "fixed");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "times-roman");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "*times-medium-r*");

  if(fs == NULL) {
    fprintf(stderr, "%s()@%d: XLoadQueryFont() returned NULL!. Exiting\n",
	    __FUNCTION__, __LINE__);
    exit(1);
  }

  XLOCK(private_data->display);
  XSetFont(private_data->display, gc, fs->fid);

  XTextExtents(fs, label, strlen(label), &dir, &as, &des, &cs);
  XUNLOCK(private_data->display);

  len = cs.width;

  /*  Some colors configurations */
  switch(state) {
  case CLICK:
    xoff = -4;
    yoff = 1;
    if(!strcasecmp(private_data->clickcolor, "Default")) {
      DefaultColor = 255;
    }
    else {
      gColor = gui_get_color_name(private_data->clickcolor);
    }
    break;
    
  case FOCUS:
    if(!strcasecmp(private_data->focuscolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = gui_get_color_name(private_data->focuscolor);
    }
    break;
    
  case NORMAL:
    if(!strcasecmp(private_data->normcolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = gui_get_color_name(private_data->normcolor);
    }
    break;
  }

  if(gColor == NULL || DefaultColor != -1) {
    color.red = color.blue = color.green = DefaultColor<<8;
  }
  else {
    color.red = gColor->red<<8; 
    color.blue = gColor->blue<<8;
    color.green = gColor->green<<8;
  }

  XLOCK(private_data->display);

  XAllocColor(private_data->display,
	      Imlib_get_colormap(private_data->imlibdata), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->display, gc, fg);

  /*  Put text in the right place */
  XDrawString(private_data->display, pix, gc, 
	      (xsize-(len+xoff))>>1, ((ysize+as+des+yoff)>>1)-des, 
	      label, strlen(label));
  
  XFreeFont(private_data->display, fs);

  XUNLOCK(private_data->display);

  if(gColor) {
    free(gColor->colorname);
    free(gColor);
  }
    
  return pix;
}

/*
 * Paint the button with correct background pixmap
 */
static void paint_labelbutton (widget_t *lb, Window win, GC gc) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  int          button_width, state = 0;
  GC           lgc;
  gui_image_t *skin;
  Pixmap       btn, bgtmp;
  XWindowAttributes attr;

  if ((lb->widget_type & WIDGET_TYPE_LABELBUTTON) && lb->visible) {
    
    XLOCK(private_data->display);

    XGetWindowAttributes(private_data->display, win, &attr);
    
    skin = private_data->skin;
    
    lgc = XCreateGC(private_data->display, win, None, None);
    XCopyGC(private_data->display, gc, (1 << GCLastBit) - 1, lgc);
    
    if (skin->mask) {
      XSetClipOrigin(private_data->display, lgc, lb->x, lb->y);
      XSetClipMask(private_data->display, lgc, skin->mask);
    }

    button_width = skin->width / 3;
    bgtmp = XCreatePixmap(private_data->display, skin->image,
			  button_width, skin->height, attr.depth);
    
    if (private_data->bArmed) {
      if (private_data->bClicked) {
	state = CLICK;
	XCopyArea (private_data->display, skin->image, 
		   bgtmp, gc, 2*button_width, 0,
		   button_width, skin->height, 0, 0);
      }
      else {
	if(!private_data->bState || private_data->bType == CLICK_BUTTON) {
	  state = FOCUS;
	  XCopyArea (private_data->display, skin->image,
		     bgtmp, gc, button_width, 0,
		     button_width, skin->height, 0, 0);
	}
	else {
	  if(private_data->bType == RADIO_BUTTON) {
	    state = CLICK;
	    XCopyArea (private_data->display, skin->image,
		       bgtmp, gc, 2*button_width, 0,
		       button_width, skin->height, 0, 0);
	  }
	}
      }
    }
    else {
      if(private_data->bState && private_data->bType == RADIO_BUTTON) {
	if(private_data->bOldState == 1 && private_data->bClicked == 1) {
	  state = NORMAL;
	  XCopyArea (private_data->display, skin->image, bgtmp, gc, 0, 0,
		     button_width, skin->height, 0, 0);
	}
	else {
	  state = CLICK;
	  XCopyArea (private_data->display, skin->image, 
		     bgtmp, gc, 2*button_width, 0,
		     button_width, skin->height, 0, 0);
	}
      }
      else {
	state = NORMAL;
	XCopyArea (private_data->display, skin->image, bgtmp, gc, 0, 0,
		   button_width, skin->height, 0, 0);
      }
    }
    
    btn = create_labelofbutton(lb, win, gc, bgtmp,
			       button_width, skin->height, 
			       private_data->label, state);

    XCopyArea (private_data->display, btn, win, lgc, 0, 0,
	       button_width, skin->height, lb->x, lb->y);

    XFreePixmap(private_data->display, bgtmp);

    XFreeGC(private_data->display, lgc);

    XUNLOCK(private_data->display);
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif
}

/*
 * Handle click events
 */
static int notify_click_labelbutton (widget_list_t *wl, widget_t *lb, 
				     int bUp, int x, int y) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    private_data->bClicked = !bUp;
    private_data->bOldState = private_data->bState;
    
    if (bUp && private_data->bArmed) {
      private_data->bState = !private_data->bState;
      paint_labelbutton(lb, wl->win, wl->gc);
      if(private_data->bType == RADIO_BUTTON) {
	if(private_data->state_callback) {
	  private_data->state_callback(private_data->bWidget, 
				       private_data->userdata,
				       private_data->bState);
	}
      }
      else if(private_data->bType == CLICK_BUTTON) {
	if(private_data->callback) {
	  private_data->callback(private_data->bWidget, 
				 private_data->userdata);
	}
      }
    }
    else
      paint_labelbutton(lb, wl->win, wl->gc);

    
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "notify click label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif

  return 1;
}

/*
 * Changing button caption
 */
int labelbutton_change_label(widget_list_t *wl, 
			     widget_t *lb, char *newlabel) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if((private_data->label = (char *) 
	realloc(private_data->label, 
		strlen(newlabel)+1)) != NULL) {
      strcpy(private_data->label, newlabel);
    }
    paint_labelbutton(lb, wl->win, wl->gc);
    return 1;
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "notify focus label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif

  return 0;
}

/*
 * Return the current button label
 */
char *labelbutton_get_label(widget_t *lb) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;

  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    return private_data->label;
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "notify focus label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif

  return NULL;
}

/*
 * Handle motion on button
 */
static int notify_focus_labelbutton (widget_list_t *wl, 
				     widget_t *lb, int bEntered) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    private_data->bArmed = bEntered;
  }
#ifdef DEBUG_GUI
 else
    fprintf (stderr, "notify focus label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif

  return 1;
}

/*
 * Return state (ON/OFF) if button is radio button, otherwise -1
 */
int labelbutton_get_state(widget_t *lb) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & ~WIDGET_TYPE_LABELBUTTON) {
#ifdef DEBUG_GUI
    fprintf (stderr, "notify click label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif
  }

  if(private_data->bType == RADIO_BUTTON)
    return private_data->bState;
  
  return -1;
}

/*
 * Set radio button to state 'state'
 */
void labelbutton_set_state(widget_t *lb, int state, Window win, GC gc) {
  int clk, arm;
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;

  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(private_data->bType == RADIO_BUTTON) {
      if(labelbutton_get_state(lb) != state) {
	arm = private_data->bArmed;
	clk = private_data->bClicked;
	
	private_data->bArmed = 1;
	private_data->bClicked = 1;
	private_data->bOldState = private_data->bState;
	private_data->bState = state;

	paint_labelbutton(lb, win, gc);
	
	private_data->bArmed = arm;
	private_data->bClicked = clk;
	
	paint_labelbutton(lb, win, gc);
      }
    }
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif
}

/*
 * Create the labeled button
 */
widget_t *label_button_create (xitk_labelbutton_t *b) {
  widget_t               *mywidget;
  lbutton_private_data_t *private_data;
  
  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));
  
  private_data = (lbutton_private_data_t *) 
    gui_xmalloc (sizeof (lbutton_private_data_t));

  private_data->display        = b->display;

  private_data->bWidget        = mywidget;
  private_data->bType          = b->button_type;
  private_data->bClicked       = 0;
  private_data->bArmed         = 0;
  private_data->bState         = 0;
  private_data->bOldState      = 0;
  private_data->imlibdata      = b->imlibdata;
  private_data->skin           = gui_load_image(b->imlibdata, b->skin);
  private_data->callback       = b->callback;
  private_data->state_callback = b->state_callback;
  private_data->userdata       = b->userdata;
  private_data->label          = (b->label ? strdup(b->label) : "");
  private_data->normcolor      = strdup(b->normcolor);
  private_data->focuscolor     = strdup(b->focuscolor);
  private_data->clickcolor     = strdup(b->clickcolor);
  private_data->fontname       = (b->fontname ? strdup(b->fontname) : NULL);

  mywidget->private_data       = private_data;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->x                  = b->x;
  mywidget->y                  = b->y;
  mywidget->width              = private_data->skin->width/3;
  mywidget->height             = private_data->skin->height;
  mywidget->widget_type        = WIDGET_TYPE_LABELBUTTON;
  mywidget->paint              = paint_labelbutton;
  mywidget->notify_click       = notify_click_labelbutton;
  mywidget->notify_focus       = notify_focus_labelbutton;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = notify_inside;

  return mywidget;
}
