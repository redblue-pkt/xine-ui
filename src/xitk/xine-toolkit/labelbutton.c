/* 
 * Copyright (C) 2000-2019 the xine project
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
#include <stdlib.h>
#include <string.h>

#include "_xitk.h"

#define CLICK     1
#define FOCUS     2
#define NORMAL    3

static size_t _strlcpy (char *d, const char *s, size_t l) {
  size_t n;
  if (!s)
    s = "";
  n = strlen (s);
  if (l > n + 1)
    l = n + 1;
  memcpy (d, s, l);
  d[l - 1] = 0;
  return n;
}

static void _set_label (lbutton_private_data_t *private_data, const char *label) {
  size_t n;
  if (private_data->label != private_data->lbuf)
    free (private_data->label);
  if (!label)
    label = "";
  n = strlen (label) + 1;
  if (n <= sizeof (private_data->lbuf)) {
    memcpy (private_data->lbuf, label, n);
    private_data->label = private_data->lbuf;
  } else {
    private_data->label = strdup (label);
  }
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;

    if (private_data->skin_element_name[0] == '\x01')
      xitk_image_free_image(private_data->imlibdata, &(private_data->skin));

    if (private_data->label != private_data->lbuf) {
      XITK_FREE (private_data->label);
    }
    XITK_FREE(private_data->shortcut_label);
    XITK_FREE(private_data->shortcut_font);
    XITK_FREE(private_data->fontname);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  lbutton_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;

    if(sk == FOREGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *w, int x, int y) {
  lbutton_private_data_t *private_data;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;

    if(w->visible == 1) {
      xitk_image_t *skin = private_data->skin;
      
      if(skin->mask)
	return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, w, skin->mask->pixmap, x, y);
    }
    else 
      return 0;
  }

  return 1;
}

/*
 * Draw the string in pixmap pix, then return it
 */
static void create_labelofbutton(xitk_widget_t *lb, 
				 Window win, GC gc, Pixmap pix, 
				 int xsize, int ysize, 
				 char *label, char *shortcut_label, int shortcut_pos, int state) {
  lbutton_private_data_t  *private_data = (lbutton_private_data_t *) lb->private_data;
  xitk_font_t             *fs = NULL;
  int                      lbear, rbear, width, asc, des;
  int                      xoff = 0, yoff = 0, DefaultColor = -1;
  unsigned int             fg = 0;
  unsigned int             origin = 0;
  XColor                   xcolor;
  xitk_color_names_t      *color = NULL;

  xcolor.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */
  if(private_data->fontname)
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
  
  if(fs == NULL) 
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, xitk_get_system_font());

  if(fs == NULL)
    XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);
  
  xitk_font_set_font(fs, gc);
  xitk_font_string_extent(fs, (label && strlen(label)) ? label : "Button", &lbear, &rbear, &width, &asc, &des);

  if((state == CLICK) && (private_data->label_static == 0)) {
    xoff = -4;
    yoff = 1;
  }

  if ((private_data->skin_element_name[0] & ~1)
    || (!(private_data->skin_element_name[0] & ~1) && ((fg = xitk_get_black_color()) == (unsigned int)-1))) {
    
    /*  Some colors configurations */
    switch(state) {
    case CLICK:
      if(!strcasecmp(private_data->clickcolor, "Default")) {
	DefaultColor = 255;
      }
      else {
	color = xitk_get_color_name(private_data->clickcolor);
      }
      break;
      
    case FOCUS:
      if(!strcasecmp(private_data->focuscolor, "Default")) {
	DefaultColor = 0;
      }
      else {
	color = xitk_get_color_name(private_data->focuscolor);
      }
      break;
      
    case NORMAL:
      if(!strcasecmp(private_data->normcolor, "Default")) {
	DefaultColor = 0;
      }
      else {
	color = xitk_get_color_name(private_data->normcolor);
      }
      break;
    }
    
    if(color == NULL || DefaultColor != -1) {
      xcolor.red = xcolor.blue = xcolor.green = DefaultColor<<8;
    }
    else {
      xcolor.red = color->red<<8; 
      xcolor.blue = color->blue<<8;
      xcolor.green = color->green<<8;
    }
    
    XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
    XAllocColor(private_data->imlibdata->x.disp,
    Imlib_get_colormap(private_data->imlibdata), &xcolor);
    XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

    fg = xcolor.pixel;
  }
  
  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  XSetForeground(private_data->imlibdata->x.disp, gc, fg);
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

  origin = ((ysize+asc+des+yoff)>>1)-des;
  
  /*  Put text in the right place */
  if(private_data->align == ALIGN_CENTER) {
    xitk_font_draw_string(fs, pix, gc, 
			  ((xsize-(width+xoff))>>1) + private_data->label_offset, 
			  origin, label, strlen(label));
  }
  else if(private_data->align == ALIGN_LEFT) {
    xitk_font_draw_string(fs, pix, gc, 
			  (((state != CLICK) ? 1 : 5)) + private_data->label_offset, 
			  origin, label, strlen(label));
    
    /* shortcut is only permited if alignement is set to left */
    if(strlen(shortcut_label) && shortcut_pos >= 0) {
      xitk_font_t *short_font;
      if (private_data->shortcut_font)
	short_font = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->shortcut_font);
      else
        short_font = fs;
      xitk_font_draw_string(short_font, pix, gc, 
			    (((state != CLICK) ? 1 : 5)) + shortcut_pos, 
			    origin, shortcut_label, strlen(shortcut_label));
      if (short_font != fs)
	xitk_font_unload_font(short_font);
    }
    
  }
  else if(private_data->align == ALIGN_RIGHT) {
    xitk_font_draw_string(fs, pix, gc, 
			  (xsize - (width + ((state != CLICK) ? 5 : 1))) + private_data->label_offset, 
			  origin, label, strlen(label));
  }

  xitk_font_unload_font(fs);

  if(color)
    xitk_free_color_name(color);
}

/*
 * Paint the button with correct background pixmap
 */
static void paint_labelbutton (xitk_widget_t *w) {
  lbutton_private_data_t *private_data;
  int               button_width, state = 0;
  GC                lgc;
  xitk_image_t     *skin;
  XWindowAttributes attr;
  xitk_pixmap_t    *btn;

  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {

    private_data = (lbutton_private_data_t *) w->private_data;

    if(w->visible == 1) {
      
      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XGetWindowAttributes(private_data->imlibdata->x.disp, w->wl->win, &attr);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
      
      skin = private_data->skin;
      
      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      lgc = XCreateGC(private_data->imlibdata->x.disp, w->wl->win, None, None);
      XCopyGC(private_data->imlibdata->x.disp, w->wl->gc, (1 << GCLastBit) - 1, lgc);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	    
      if (skin->mask) {
        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	XSetClipOrigin(private_data->imlibdata->x.disp, lgc, w->x, w->y);
	XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask->pixmap);
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
      }
      
      button_width = skin->width / 3;
      btn = xitk_image_create_xitk_pixmap(private_data->imlibdata, button_width, skin->height);
      
      if ((private_data->focus == FOCUS_RECEIVED) || (private_data->focus == FOCUS_MOUSE_IN)) {
	if (private_data->bClicked) {
	  state = CLICK;
          XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	  XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		     btn->pixmap, w->wl->gc, 2*button_width, 0,
		     button_width, skin->height, 0, 0);
          XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	}
	else {
	  if(!private_data->bState || private_data->bType == CLICK_BUTTON) {
	    state = FOCUS;
            XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	    XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap,
		       btn->pixmap, w->wl->gc, button_width, 0,
		       button_width, skin->height, 0, 0);
            XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	  }
	  else {
	    if(private_data->bType == RADIO_BUTTON) {
	      state = CLICK;
              XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	      XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap,
			 btn->pixmap, w->wl->gc, 2*button_width, 0,
			 button_width, skin->height, 0, 0);
              XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	    }
	  }
	}
      }
      else {
        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	if(private_data->bState && private_data->bType == RADIO_BUTTON) {
	  if(private_data->bOldState == 1 && private_data->bClicked == 1) {
	    state = NORMAL;
	    XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, btn->pixmap, 
		       w->wl->gc, 0, 0, button_width, skin->height, 0, 0);
	  }
	  else {
	    state = CLICK;
	    XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, 
		       btn->pixmap, w->wl->gc, 2*button_width, 0,
		       button_width, skin->height, 0, 0);
	  }
	}
	else {
	  state = NORMAL;
	  XCopyArea (private_data->imlibdata->x.disp, skin->image->pixmap, btn->pixmap, 
		     w->wl->gc, 0, 0, button_width, skin->height, 0, 0);
	}
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
      }
      
      if(private_data->label_visible) {
	create_labelofbutton(w, w->wl->win, w->wl->gc, btn->pixmap,
			     button_width, skin->height, 
			     private_data->label, private_data->shortcut_label, private_data->shortcut_pos, state);
      }
      
      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, btn->pixmap, w->wl->win, lgc, 0, 0,
		 button_width, skin->height, w->x, w->y);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

      xitk_image_destroy_xitk_pixmap(btn);

      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XFreeGC(private_data->imlibdata->x.disp, lgc);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
    }
  }
}

/*
 * Handle click events
 */
static int notify_click_labelbutton (xitk_widget_t *w, int button, int bUp, int x, int y) {
  lbutton_private_data_t *private_data;
  int                     ret = 0;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    if(button == Button1) {
      private_data = (lbutton_private_data_t *) w->private_data;
      
      private_data->bClicked = !bUp;
      private_data->bOldState = private_data->bState;
      
      if (bUp && (private_data->focus == FOCUS_RECEIVED)) {
	private_data->bState = !private_data->bState;
	paint_labelbutton(w);
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
	paint_labelbutton(w);
      
      ret = 1;
    }
  }

  return ret;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_label (xitk_widget_t *w, const char *newlabel) {
  lbutton_private_data_t *private_data;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    _set_label (private_data, newlabel);
    paint_labelbutton(w);
    return 1;
  }

  return 0;
}

/*
 * Return the current button label
 */
const char *xitk_labelbutton_get_label(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;

  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    return private_data->label;
  }

  return NULL;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_shortcut_label(xitk_widget_t *w, const char *newlabel, int pos, const char *newfont) {
  lbutton_private_data_t *private_data;

  if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) && 
	    (((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_MENU) || ((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER)))) {
    private_data = (lbutton_private_data_t *) w->private_data;

    if((private_data->shortcut_label = (char *) realloc(private_data->shortcut_label, strlen(newlabel) + 1)) != NULL)
      strcpy(private_data->shortcut_label, newlabel);

    if(newfont &&
	(private_data->shortcut_font = (char *) realloc(private_data->shortcut_font, strlen(newfont) + 1)) != NULL)
      strcpy(private_data->shortcut_font, newfont);

    if(strlen(private_data->shortcut_label)) {
      if(pos >= 0)
	private_data->shortcut_pos = pos;
      else
	private_data->shortcut_pos = -1;
    }
    
    paint_labelbutton(w);
    
    return 1;
  }

  return 0;
}

/*
 * Return the current button label
 */
const char *xitk_labelbutton_get_shortcut_label(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;

  if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) &&
	    (((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_MENU) || ((w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER)))) {
    private_data = (lbutton_private_data_t *) w->private_data;
    return private_data->shortcut_label;
  }

  return NULL;
}

/*
 * Handle focus on button
 */
static int notify_focus_labelbutton (xitk_widget_t *w, int focus) {
  lbutton_private_data_t *private_data;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    private_data->focus = focus;
  }

  return 1;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  lbutton_private_data_t *private_data;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    
    if (private_data->skin_element_name[0] & ~1) {
      const xitk_skin_element_info_t *info;
      
      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, private_data->skin_element_name);
      XITK_FREE (private_data->fontname);
      if (info) {
        private_data->skin = info->pixmap_img;
        _strlcpy (private_data->normcolor,  info->label_color,       sizeof (private_data->normcolor));
        _strlcpy (private_data->focuscolor, info->label_color_focus, sizeof (private_data->focuscolor));
        _strlcpy (private_data->clickcolor, info->label_color_click, sizeof (private_data->clickcolor));
        private_data->fontname      = strdup (info->label_fontname);
        private_data->label_visible = info->label_printable;
        private_data->label_static  = info->label_staticity;
        private_data->align         = info->label_alignment;
        w->x      = info->x;
        w->y      = info->y;
        w->width  = private_data->skin->width / 3;
        w->height = private_data->skin->height;
        w->visible = info->visibility ? 1 : -1;
        w->enable = info->enability;
      }
      xitk_skin_unlock (skonfig);

      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}


static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint_labelbutton(w);
    break;
  case WIDGET_EVENT_CLICK:
    result->value = notify_click_labelbutton(w, event->button,
					     event->button_pressed, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_FOCUS:
    notify_focus_labelbutton(w, event->focus);
    break;
  case WIDGET_EVENT_INSIDE:
    result->value = notify_inside(w, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_GET_SKIN:
    if(result) {
      result->image = get_skin(w, event->skin_layer);
      retval = 1;
    }
    break;
  }
  
  return retval;
}

/*
 * Return state (ON/OFF) if button is radio button, otherwise -1
 */
int xitk_labelbutton_get_state(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;

    if(private_data->bType == RADIO_BUTTON)
      return private_data->bState;
  }
  
  return -1;
}

/*
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment(xitk_widget_t *w, int align) {
  lbutton_private_data_t *private_data;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    private_data->align = align;
  }
}

/*
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    return private_data->align;
  }
  
  return -1;
}

/*
 * Return used font name
 */
char *xitk_labelbutton_get_fontname(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    if(private_data->fontname == NULL)
      return NULL;
    return (strdup(private_data->fontname));
  }
  
  return NULL;
}

/*
 *
 */
void xitk_labelbutton_set_label_offset(xitk_widget_t *w, int offset) {
  lbutton_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    private_data->label_offset = offset;
  }
}
int xitk_labelbutton_get_label_offset(xitk_widget_t *w) {
  lbutton_private_data_t *private_data;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    private_data = (lbutton_private_data_t *) w->private_data;
    return private_data->label_offset;
  }
  return 0;
}

/*
 * Set radio button to state 'state'
 */
void xitk_labelbutton_set_state(xitk_widget_t *w, int state) {
  lbutton_private_data_t *private_data;
  int                     clk, focus;
  
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    
    private_data = (lbutton_private_data_t *) w->private_data;
    
    if(private_data->bType == RADIO_BUTTON) {
      if(xitk_labelbutton_get_state(w) != state) {
	focus = private_data->focus;
	clk = private_data->bClicked;
	
	private_data->focus = FOCUS_RECEIVED;
	private_data->bClicked = 1;
	private_data->bOldState = private_data->bState;
	private_data->bState = state;

	paint_labelbutton(w);
	
	private_data->focus = focus;
	private_data->bClicked = clk;
	
	paint_labelbutton(w);
      }
    }
  }

}

void *labelbutton_get_user_data(xitk_widget_t *w) {
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    lbutton_private_data_t *private_data = (lbutton_private_data_t *) w->private_data;
    
    return private_data->userdata;
  }

  return NULL;
}
void xitk_labelbutton_callback_exec(xitk_widget_t *w) {

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    lbutton_private_data_t *private_data = (lbutton_private_data_t *) w->private_data;
    
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
}

/*
 * Create the labeled button
 */
xitk_widget_t *xitk_info_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, const xitk_skin_element_info_t *info) {
  xitk_widget_t           *mywidget;
  lbutton_private_data_t *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  if (!mywidget)
    return NULL;
  private_data = (lbutton_private_data_t *) xitk_xmalloc (sizeof (lbutton_private_data_t));
  if (!private_data) {
    free (mywidget);
    return NULL;
  }

  private_data->imlibdata         = b->imlibdata;

  private_data->bWidget           = mywidget;
  private_data->bType             = b->button_type;
  private_data->bClicked          = 0;
  private_data->focus             = FOCUS_LOST;
  private_data->bState            = 0;
  private_data->bOldState         = 0;

  private_data->callback          = b->callback;
  private_data->state_callback    = b->state_callback;
  private_data->userdata          = b->userdata;

  private_data->label = private_data->lbuf;
  _set_label (private_data, b->label);

  private_data->shortcut_label    = strdup("");
  private_data->shortcut_font     = strdup (info->label_fontname);
  private_data->shortcut_pos      = -1;
  private_data->label_visible     = info->label_printable;
  private_data->label_static      = info->label_staticity;

  private_data->skin              = info->pixmap_img;

  if (info->pixmap_name && (info->pixmap_name[0] == '\x01') && (info->pixmap_name[1] == 0)) {
    private_data->skin_element_name[0] = '\x01';
    private_data->skin_element_name[1] = 0;
  } else {
    _strlcpy (private_data->skin_element_name, b->skin_element_name, sizeof (private_data->skin_element_name));
  }
  _strlcpy (private_data->normcolor, info->label_color, sizeof (private_data->normcolor));
  _strlcpy (private_data->focuscolor, info->label_color_focus, sizeof (private_data->focuscolor));
  _strlcpy (private_data->clickcolor, info->label_color_click, sizeof (private_data->clickcolor));
  private_data->fontname          = strdup (info->label_fontname);

  private_data->label_offset      = 0;
  private_data->align             = info->label_alignment;

  mywidget->private_data          = private_data;

  mywidget->wl                    = wl;

  mywidget->enable                = info->enability;
  mywidget->running               = 1;
  mywidget->visible               = info->visibility;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->imlibdata             = private_data->imlibdata;
  mywidget->x                     = info->x;
  mywidget->y                     = info->y;
  mywidget->width                 = private_data->skin->width/3;
  mywidget->height                = private_data->skin->height;
  mywidget->type                  = WIDGET_TYPE_LABELBUTTON | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEYABLE;
  mywidget->event                 = notify_event;
  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_labelbutton_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_labelbutton_widget_t *b) {
  const xitk_skin_element_info_t *pinfo;
  xitk_skin_element_info_t info;

  XITK_CHECK_CONSTITENCY (b);
  pinfo = xitk_skin_get_info (skonfig, b->skin_element_name);
  if (pinfo) {
    if (!pinfo->visibility) {
      info = *pinfo;
      info.visibility = 1;
      pinfo = &info;
    }
  } else {
    memset (&info, 0, sizeof (info));
    info.label_alignment = b->align;
    info.visibility = -1;
    pinfo = &info;
  }

  return xitk_info_labelbutton_create (wl, b, pinfo);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, int x, int y, int width, int height,
  const char *ncolor, const char *fcolor, const char *ccolor, const char *fname) {
  xitk_skin_element_info_t info;
  
  XITK_CHECK_CONSTITENCY(b);
  info.x                 = x;
  info.y                 = y;
  info.label_alignment   = b->align;
  info.label_printable   = 1;
  info.label_staticity   = 0;
  info.visibility        = 0;
  info.enability         = 0;
  info.label_color       = (char *)ncolor;
  info.label_color_focus = (char *)fcolor;
  info.label_color_click = (char *)ccolor;
  info.label_fontname    = (char *)fname;
  info.pixmap_name       = (char *)"\x01";
  info.pixmap_img        = xitk_image_create_image (b->imlibdata, width * 3, height);
  if (info.pixmap_img)
    draw_bevel_three_state (b->imlibdata, info.pixmap_img);

  return xitk_info_labelbutton_create (wl, b, &info);
}

