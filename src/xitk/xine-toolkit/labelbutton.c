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
#include "font.h"
#include "widget_types.h"

#include "_xitk.h"

#define CLICK     1
#define FOCUS     2
#define NORMAL    3

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  lbutton_private_data_t *private_data = (lbutton_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_LABELBUTTON) {
    XITK_FREE(private_data->label);
    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->skin);
    XITK_FREE(private_data->normcolor);
    XITK_FREE(private_data->focuscolor);
    XITK_FREE(private_data->clickcolor);
    XITK_FREE(private_data->fontname);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(sk == FOREGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *lb, int x, int y) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(lb->visible) {
      xitk_image_t *skin = private_data->skin;
      
      return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, lb, skin->mask, x, y);
    }
    else 
      return 0;
  }

  return 1;
}

/*
 * Draw the string in pixmap pix, then return it
 */
static Pixmap create_labelofbutton(xitk_widget_t *lb, 
				   Window win, GC gc, Pixmap pix, 
				   int xsize, int ysize, 
				   char *label, int state) {
  lbutton_private_data_t  *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  xitk_font_t             *fs = NULL;
  int                      lbear, rbear, width, asc, des;
  int                      xoff = 0, yoff = 0, DefaultColor = -1;
  unsigned int             fg;
  XColor                   color;
  xitk_color_names_t      *gColor = NULL;

  color.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */
  if(private_data->fontname)
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
  
  if(fs == NULL) 
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, xitk_get_system_font());

  if(fs == NULL)
    XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);
  
  xitk_font_set_font(fs, gc);
  xitk_font_string_extent(fs, label, &lbear, &rbear, &width, &asc, &des);

  /*  Some colors configurations */
  switch(state) {
  case CLICK:
    xoff = -4;
    yoff = 1;
    if(!strcasecmp(private_data->clickcolor, "Default")) {
      DefaultColor = 255;
    }
    else {
      gColor = xitk_get_color_name(private_data->clickcolor);
    }
    break;
    
  case FOCUS:
    if(!strcasecmp(private_data->focuscolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = xitk_get_color_name(private_data->focuscolor);
    }
    break;
    
  case NORMAL:
    if(!strcasecmp(private_data->normcolor, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = xitk_get_color_name(private_data->normcolor);
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

  XLOCK(private_data->imlibdata->x.disp);

  XAllocColor(private_data->imlibdata->x.disp,
	      Imlib_get_colormap(private_data->imlibdata), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->imlibdata->x.disp, gc, fg);

  /*  Put text in the right place */
  if(private_data->align == LABEL_ALIGN_CENTER) {
    XDrawString(private_data->imlibdata->x.disp, pix, gc, 
		(xsize-(width+xoff))>>1, ((ysize+asc+des+yoff)>>1)-des, 
		label, strlen(label));
  }
  else if(private_data->align == LABEL_ALIGN_LEFT) {
    XDrawString(private_data->imlibdata->x.disp, pix, gc, 
		((state != CLICK) ? 1 : 5), ((ysize+asc+des+yoff)>>1)-des, 
		label, strlen(label));
  }
  else if(private_data->align == LABEL_ALIGN_RIGHT) {
    XDrawString(private_data->imlibdata->x.disp, pix, gc, 
    		xsize - (width + ((state != CLICK) ? 5 : 1)),
    		((ysize+asc+des+yoff)>>1)-des, 
    		label, strlen(label));
  }
  
  XUNLOCK(private_data->imlibdata->x.disp);

  xitk_font_unload_font(fs);

  if(gColor) {
    XITK_FREE(gColor->colorname);
    XITK_FREE(gColor);
  }
    
  return pix;
}

/*
 * Paint the button with correct background pixmap
 */
static void paint_labelbutton (xitk_widget_t *lb, Window win, GC gc) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  int          button_width, state = 0;
  GC           lgc;
  xitk_image_t *skin;
  Pixmap       btn, bgtmp;
  XWindowAttributes attr;

  if ((lb->widget_type & WIDGET_TYPE_LABELBUTTON) && lb->visible) {
    
    XLOCK(private_data->imlibdata->x.disp);

    XGetWindowAttributes(private_data->imlibdata->x.disp, win, &attr);
    
    skin = private_data->skin;
    
    lgc = XCreateGC(private_data->imlibdata->x.disp, win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, lgc);
    
    if (skin->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, lb->x, lb->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask);
    }

    button_width = skin->width / 3;
    bgtmp = XCreatePixmap(private_data->imlibdata->x.disp, skin->image,
			  button_width, skin->height, attr.depth);
    
    if (private_data->bArmed) {
      if (private_data->bClicked) {
	state = CLICK;
	XCopyArea (private_data->imlibdata->x.disp, skin->image, 
		   bgtmp, gc, 2*button_width, 0,
		   button_width, skin->height, 0, 0);
      }
      else {
	if(!private_data->bState || private_data->bType == CLICK_BUTTON) {
	  state = FOCUS;
	  XCopyArea (private_data->imlibdata->x.disp, skin->image,
		     bgtmp, gc, button_width, 0,
		     button_width, skin->height, 0, 0);
	}
	else {
	  if(private_data->bType == RADIO_BUTTON) {
	    state = CLICK;
	    XCopyArea (private_data->imlibdata->x.disp, skin->image,
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
	  XCopyArea (private_data->imlibdata->x.disp, skin->image, bgtmp, gc, 0, 0,
		     button_width, skin->height, 0, 0);
	}
	else {
	  state = CLICK;
	  XCopyArea (private_data->imlibdata->x.disp, skin->image, 
		     bgtmp, gc, 2*button_width, 0,
		     button_width, skin->height, 0, 0);
	}
      }
      else {
	state = NORMAL;
	XCopyArea (private_data->imlibdata->x.disp, skin->image, bgtmp, gc, 0, 0,
		   button_width, skin->height, 0, 0);
      }
    }
    
    btn = create_labelofbutton(lb, win, gc, bgtmp,
			       button_width, skin->height, 
			       private_data->label, state);

    XCopyArea (private_data->imlibdata->x.disp, btn, win, lgc, 0, 0,
	       button_width, skin->height, lb->x, lb->y);

    XFreePixmap(private_data->imlibdata->x.disp, bgtmp);

    XFreeGC(private_data->imlibdata->x.disp, lgc);

    XUNLOCK(private_data->imlibdata->x.disp);
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
static int notify_click_labelbutton (xitk_widget_list_t *wl, xitk_widget_t *lb, 
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
int xitk_labelbutton_change_label(xitk_widget_list_t *wl, 
				  xitk_widget_t *lb, char *newlabel) {
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
char *xitk_labelbutton_get_label(xitk_widget_t *lb) {
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
static int notify_focus_labelbutton (xitk_widget_list_t *wl, 
				     xitk_widget_t *lb, int bEntered) {
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
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *lb, xitk_skin_config_t *skonfig) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(private_data->skin_element_name) {
      XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->skin);
      private_data->skin       = xitk_image_load_image(private_data->imlibdata, 
						       xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
      XITK_FREE(private_data->normcolor);
      private_data->normcolor  = strdup(xitk_skin_get_label_color(skonfig, private_data->skin_element_name));
      XITK_FREE(private_data->focuscolor);
      private_data->focuscolor = strdup(xitk_skin_get_label_color_focus(skonfig, private_data->skin_element_name));
      XITK_FREE(private_data->clickcolor);
      private_data->clickcolor = strdup(xitk_skin_get_label_color_click(skonfig, private_data->skin_element_name));
      XITK_FREE(private_data->fontname);
      private_data->fontname   = strdup(xitk_skin_get_label_fontname(skonfig, private_data->skin_element_name));
      lb->x                    = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      lb->y                    = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      lb->width                = private_data->skin->width/3;
      lb->height               = private_data->skin->height;
      
      xitk_set_widget_pos(lb, lb->x, lb->y);
    }
  }
}

/*
 * Return state (ON/OFF) if button is radio button, otherwise -1
 */
int xitk_labelbutton_get_state(xitk_widget_t *lb) {
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;
  
  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(private_data->bType == RADIO_BUTTON)
      return private_data->bState;
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify click label button on something (%d) "
	     "that is not a label button\n", lb->widget_type);
#endif
  
  return -1;
}

/*
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment(xitk_widget_t *lb, int align) {
  lbutton_private_data_t *private_data = (lbutton_private_data_t *) lb->private_data;

  if(lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    private_data->align = align;
  }
}

/*
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment(xitk_widget_t *lb) {
  lbutton_private_data_t *private_data = (lbutton_private_data_t *) lb->private_data;

  if(lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    return private_data->align;
  }
  
  return -1;
}

/*
 * Set radio button to state 'state'
 */
void xitk_labelbutton_set_state(xitk_widget_t *lb, int state, Window win, GC gc) {
  int clk, arm;
  lbutton_private_data_t *private_data = 
    (lbutton_private_data_t *) lb->private_data;

  if (lb->widget_type & WIDGET_TYPE_LABELBUTTON) {
    if(private_data->bType == RADIO_BUTTON) {
      if(xitk_labelbutton_get_state(lb) != state) {
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
static xitk_widget_t *_xitk_labelbutton_create (xitk_skin_config_t *skonfig, 
						xitk_labelbutton_widget_t *b,
						int x, int y, 
						char *skin_element_name, xitk_image_t *skin,
						char *ncolor, char *fcolor, char *ccolor,
						char *fontname, char *label) {
  xitk_widget_t               *mywidget;
  lbutton_private_data_t *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  
  private_data = (lbutton_private_data_t *) xitk_xmalloc (sizeof (lbutton_private_data_t));

  private_data->imlibdata         = b->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(b->skin_element_name);

  private_data->bWidget           = mywidget;
  private_data->bType             = b->button_type;
  private_data->bClicked          = 0;
  private_data->bArmed            = 0;
  private_data->bState            = 0;
  private_data->bOldState         = 0;

  private_data->callback          = b->callback;
  private_data->state_callback    = b->state_callback;
  private_data->userdata          = b->userdata;
  private_data->label             = strdup((label)?label:"");

  private_data->skin              = skin;
  private_data->normcolor         = strdup(ncolor);
  private_data->focuscolor        = strdup(fcolor);
  private_data->clickcolor        = strdup(ccolor);
  private_data->fontname          = strdup(fontname);

  private_data->align             = b->align;

  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width/3;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_LABELBUTTON;
  mywidget->paint                 = paint_labelbutton;
  mywidget->notify_click          = notify_click_labelbutton;
  mywidget->notify_focus          = notify_focus_labelbutton;
  mywidget->notify_keyevent       = NULL;
  mywidget->notify_inside         = notify_inside;
  mywidget->notify_change_skin    = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy        = notify_destroy;
  mywidget->get_skin              = get_skin;

  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_labelbutton_create (xitk_skin_config_t *skonfig, xitk_labelbutton_widget_t *b) {

  XITK_CHECK_CONSTITENCY(b);

  return _xitk_labelbutton_create(skonfig, b, 
		  (xitk_skin_get_coord_x(skonfig, b->skin_element_name)),
		  (xitk_skin_get_coord_y(skonfig, b->skin_element_name)),
		   b->skin_element_name,
		  (xitk_image_load_image(b->imlibdata, xitk_skin_get_skin_filename(skonfig, b->skin_element_name))),
		  (xitk_skin_get_label_color(skonfig, b->skin_element_name)),
		  (xitk_skin_get_label_color_focus(skonfig, b->skin_element_name)),
		  (xitk_skin_get_label_color_click(skonfig, b->skin_element_name)),
		  (xitk_skin_get_label_fontname(skonfig, b->skin_element_name)),
		  (((xitk_skin_get_label_printable(skonfig, b->skin_element_name) == 1)
		   ? b->label : NULL)));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_labelbutton_widget_t *b,
					       int x, int y, int width, int height,
					       char *ncolor, char *fcolor, char *ccolor, 
					       char *fname) {
  xitk_image_t  *i;
  
  XITK_CHECK_CONSTITENCY(b);

  i = xitk_image_create_image(b->imlibdata, width * 3, height);
  draw_bevel_three_state(b->imlibdata, i);

  return _xitk_labelbutton_create(NULL, b, x, y, NULL, i, ncolor, fcolor, ccolor, fname, b->label);
}
