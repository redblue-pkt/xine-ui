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

#define CLICK     1
#define FOCUS     2
#define NORMAL    3

/*
 * Draw the string in pixmap pix, then return it
 */
static Pixmap create_labelofbutton(widget_t *lb, 
				   Window win, GC gc, Pixmap pix, 
				   int xsize, int ysize, 
				   const char *label, int state) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  XFontStruct *fs;
  XCharStruct cs;
  int dir, as, des, len, xoff = 0, yoff = 0, DefaultColor = -1;
  unsigned int fg;
  XColor color;
  gui_color_names_t *gColors;

  gColors = gui_get_color_names();

  color.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */
  fs = XLoadQueryFont(private_data->display, "fixed");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "times-roman");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "*times-medium-r*");
  XTextExtents(fs, label, strlen(label), &dir, &as, &des, &cs);
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
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->clickcolor)) {
	  break;
	}
      } 
    }
    break;
    
  case FOCUS:
    if(!strcasecmp(private_data->clickcolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->focuscolor)) {
	  break;
	}
      } 
    }
    break;
    
  case NORMAL:
    if(!strcasecmp(private_data->clickcolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->normcolor)) {
	  break;
	}
      } 
    }
    break;
  }
  
  if(gColors->colorname == NULL || DefaultColor != -1) {
    color.red = color.blue = color.green = DefaultColor<<8;
  }
  else {
    color.red = gColors->red<<8; 
    color.blue = gColors->blue<<8;
    color.green = gColors->green<<8;
  }

  XAllocColor(private_data->display, DefaultColormap(private_data->display, 0), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->display, gc, fg);

  /*  Put text in the right place */
  XDrawString(private_data->display, pix, gc, 
	      (xsize-(len+xoff))>>1, ((ysize+as+des+yoff)>>1)-des, 
	      label, strlen(label));
  
  XFreeFont(private_data->display, fs);

  return pix;
}

/*
 * Paint the button with correct background pixmap
 */
static void paint_labelbutton (widget_t *lb, Window win, GC gc) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  int          button_width, state = 0;
  gui_image_t *skin;
  Pixmap       btn, bgtmp;
  XWindowAttributes attr;

  XGetWindowAttributes(private_data->display, win, &attr);

  skin = private_data->skin;

  button_width = skin->width / 3;
  bgtmp = XCreatePixmap(private_data->display, skin->image,
			button_width, skin->height, attr.depth);

  XLockDisplay(private_data->display);

  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    
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
    XCopyArea (private_data->display, btn, win, gc, 0, 0,
	       button_width, skin->height, lb->x, lb->y);

    XFreePixmap(private_data->display, bgtmp);

  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif
  
  XUnlockDisplay (private_data->display);
  XSync (private_data->display, False);
}

/*
 * Handle click events
 */
static int notify_click_labelbutton (widget_list_t *wl, widget_t *lb, 
				     int bUp, int x, int y) {
  int bRepaint = 0;
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    private_data->bClicked = !bUp;
    private_data->bOldState = private_data->bState;
    
    if (bUp && private_data->bArmed) {
      private_data->bState = !private_data->bState;
      bRepaint = paint_widget_list (wl);
      if(private_data->bType == RADIO_BUTTON) {
	if(private_data->rbfunction) {
	  private_data->rbfunction (private_data->bWidget, 
				    private_data->user_data,
				    private_data->bState);
	}
      }
      else if(private_data->bType == CLICK_BUTTON) {
	if(private_data->function) {
	  private_data->function (private_data->bWidget, 
				  private_data->user_data);
	}
      }
    }
    
    if(!bRepaint) 
      paint_widget_list (wl);
    
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
			     widget_t *lb, const char *newlabel) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if((private_data->label = 
	(const char *) realloc((char *) private_data->label, 
			       strlen(newlabel)+1)) != NULL) {
      strcpy((char*)private_data->label, (char*)newlabel);
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
const char *labelbutton_get_label(widget_t *lb) {
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
widget_t *create_label_button (Display *display, ImlibData *idata,
			       int x, int y, int btype, const char *label, 
			       void* f, void* ud, const char *skin,
 			       const char *normcolor, const char *focuscolor, 
			       const char *clickcolor) {
  widget_t               *mywidget;
  lbutton_private_data_t *private_data;
  
  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));
  
  private_data = (lbutton_private_data_t *) 
    gui_xmalloc (sizeof (lbutton_private_data_t));

  private_data->display    = display;

  private_data->bWidget    = mywidget;
  private_data->bType      = btype;
  private_data->bClicked   = 0;
  private_data->bArmed     = 0;
  private_data->bState     = 0;
  private_data->bOldState  = 0;
  private_data->skin       = gui_load_image(idata, skin);
  private_data->function   = f;
  private_data->rbfunction = f;
  private_data->user_data  = ud;
  private_data->label      = (label ? strdup(label) : "");
  private_data->normcolor  = strdup(normcolor);
  private_data->focuscolor = strdup(focuscolor);
  private_data->clickcolor = strdup(clickcolor);

  mywidget->private_data   = private_data;

  mywidget->enable         = 1;
  mywidget->x              = x;
  mywidget->y              = y;
  mywidget->width          = private_data->skin->width/3;
  mywidget->height         = private_data->skin->height;
  mywidget->widget_type    = WIDGET_TYPE_LABELBUTTON;
  mywidget->paint          = paint_labelbutton;
  mywidget->notify_click   = notify_click_labelbutton;
  mywidget->notify_focus   = notify_focus_labelbutton;

  return mywidget;
}
