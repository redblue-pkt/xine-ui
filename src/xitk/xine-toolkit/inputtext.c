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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "inputtext.h"
#include "widget_types.h"

#include "_xitk.h"

#define NORMAL    1
#define FOCUS     2

/*
 * Return a pixmap with text drawed.
 */
static Pixmap create_labelofinputtext(widget_t *it, 
				      Window win, GC gc, Pixmap pix, 
				      int xsize, int ysize, 
				      char *label, int state) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *plabel = label;
  XFontStruct *fs;
  XCharStruct cs;
  int dir, as, des, len, yoff = 0, DefaultColor = -1;
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
  case NORMAL:
    if(!strcasecmp(private_data->normal_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->normal_color)) {
	  break;
	}
      } 
    }
    break;

  case FOCUS:
    if(!strcasecmp(private_data->focused_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->focused_color)) {
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

  XAllocColor(private_data->display,
	      DefaultColormap(private_data->display, 0), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->display, gc, fg);

  if((private_data->cursor_pos - private_data->disp_offset)
     > private_data->max_visible) {
    private_data->disp_offset += private_data->max_visible - 1;
  }
  else if(private_data->disp_offset > private_data->cursor_pos) {
    private_data->disp_offset -= private_data->max_visible;
    if(private_data->disp_offset < 0)
      private_data->disp_offset = 0;
  }

  if(private_data->disp_offset)
    plabel += private_data->disp_offset;

  /*  Put text in the right place */
  XDrawString(private_data->display, pix, gc, 
	      2, ((ysize+as+des+yoff)>>1)-des, 
	      plabel, strlen(plabel));

  /* Draw cursor pointer */
  if(private_data->cursor_pos >= 0) {
    XDrawLine(private_data->display, pix, gc, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 2
	      , 2, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 2,
	      ysize - 3);
    XDrawLine(private_data->display, pix, gc,
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 1,
	      2, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 3,
	      2);
    XDrawLine(private_data->display, pix, gc, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 1,
	      ysize - 3, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 3,
	      ysize - 3);
  }
  
  XFreeFont(private_data->display, fs);

  return pix;
}

/*
 * Paint the input text box.
 */
static void paint_inputtext(widget_t *it, Window win, GC gc) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int          button_width, state = 0;
  gui_image_t *skin;
  Pixmap       btn, bgtmp;
  XWindowAttributes attr;

  XGetWindowAttributes(private_data->display, win, &attr);

  skin = private_data->skin;

  button_width = skin->width/2;

  bgtmp = XCreatePixmap(private_data->display, skin->image,
			button_width, skin->height, attr.depth);

  XLOCK(private_data->display);

  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    
    if(private_data->have_focus) {
      state = FOCUS;
      XCopyArea (private_data->display, skin->image,
		 bgtmp, gc, button_width, 0,
		 button_width, skin->height, 0, 0);
    }
    else {
      state = NORMAL;
      XCopyArea (private_data->display, skin->image,
		 bgtmp, gc, 0, 0,
		 button_width, skin->height, 0, 0);
    }
    

    btn = create_labelofinputtext(it, win, gc, bgtmp,
				  button_width, skin->height, 
				  private_data->text, state);
    
    XCopyArea (private_data->display, btn, win, gc, 0, 0,
	       button_width, skin->height, it->x, it->y);


    XFreePixmap(private_data->display, bgtmp);

  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif
  
  XUNLOCK(private_data->display);
}

/*
 * Handle click events.
 */
static int notify_click_inputtext(widget_list_t *wl, widget_t *it, 
				   int bUp, int x, int y) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int pos;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {

    if(it->have_focus == FOCUS_LOST) {
      it->have_focus = private_data->have_focus = FOCUS_RECEIVED;
    }

    pos = x - it->x;
    pos /= 6;

    if(private_data->text) {
      if(pos > strlen(private_data->text))
	pos = strlen(private_data->text);
    }
    else
      pos = 0;

    private_data->cursor_pos = pos;

    paint_inputtext(it, wl->win, wl->gc);

  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify focus input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif

  return 1;
}

/*
 * Handle motion on input text box.
 */
static int notify_focus_inputtext(widget_list_t *wl, 
				   widget_t *it, int bEntered) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    private_data->have_focus = bEntered;
    if(bEntered == FOCUS_LOST) {
      private_data->cursor_pos = -1;
    }
    else {
      private_data->cursor_pos = strlen(private_data->text);
    }
    paint_inputtext(it, wl->win, wl->gc);

  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "notify focus input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif

  return 1;
}

/*
 * Handle keyboard event in input text box.
 */
static void notify_keyevent_inputtext(widget_list_t *wl,
				      widget_t *it, XEvent *xev) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    XKeyEvent   keyevent = xev->xkey;
    KeySym      key;
    char        buf[256];
    int         len;
    
    XLOCK(private_data->display);
    len = XLookupString(&keyevent, buf, sizeof(buf), &key, NULL);
    XUNLOCK(private_data->display);

    buf[len] = '\0';
    
    if(key == XK_Delete) {
      char *oldtext, *newtext;
      char *p, *pp;
      int offset;
      
      if(strlen(private_data->text) > 1) {
	if((private_data->cursor_pos > 0)
	   && (private_data->cursor_pos < strlen(private_data->text))) {

	  oldtext = strdup(private_data->text);
	  newtext = (char *) gui_xmalloc(strlen(oldtext));

	  offset = 0;
	  p = oldtext;
	  pp = newtext;
	  
	  while(offset < private_data->cursor_pos) {
	    *pp = *p;
	    p++;
	    pp++;
	    offset++;
	  }
	  p++;
	  
	  while(*p != '\0') {
	    *pp = *p;
	    offset++;
	    p++;
	    pp++;
	  } 

	  *pp = 0;

	  free(private_data->text);
	  private_data->text = strdup(newtext);

	  paint_inputtext(it, wl->win, wl->gc);

	  free(oldtext);
	  free(newtext);
	}
      }
      else {
	sprintf(private_data->text, "%s", "");
	paint_inputtext(it, wl->win, wl->gc);
      }
	
    }
    else if(key == XK_BackSpace) {
      char *oldtext, *newtext;
      char *p, *pp;
      int offset;

      if(strlen(private_data->text) > 1) {
	if(private_data->cursor_pos > 0) {

	  oldtext = strdup(private_data->text);
	  newtext = (char *) gui_xmalloc(strlen(oldtext));

	  private_data->cursor_pos--;
	  
	  offset = 0;
	  p = oldtext;
	  pp = newtext;
	  
	  while(offset < private_data->cursor_pos) {
	    *pp = *p;
	    p++;
	    pp++;
	    offset++;
	  }
	  p++;
	  
	  while(*p != '\0') {
	    *pp = *p;
	    offset++;
	    p++;
	    pp++;
	  } 
	  *pp = 0;
	    
	  free(private_data->text);
	  private_data->text = strdup(newtext);

	  paint_inputtext(it, wl->win, wl->gc);

	  free(oldtext);
	  free(newtext);
	}
      }
      else {
	sprintf(private_data->text, "%s", "");
	private_data->cursor_pos = 0;
	paint_inputtext(it, wl->win, wl->gc);
      }
	
    }
    else if(key == XK_Left) {
      if(private_data->cursor_pos > 0) {
	private_data->cursor_pos--;
	paint_inputtext(it, wl->win, wl->gc);
      }
    }
    else if(key == XK_Right) {
      if(private_data->cursor_pos < strlen(private_data->text)) {
	private_data->cursor_pos++;
	paint_inputtext(it, wl->win, wl->gc);
      }
    }
    else if((key == XK_Return) || (key == XK_KP_Enter)) {
      private_data->cursor_pos = -1;
      it->have_focus = private_data->have_focus = FOCUS_LOST;
      paint_inputtext(it, wl->win, wl->gc);

      if(strlen(private_data->text) > 0) {
	if(private_data->callback)
	  private_data->callback(it, 
				 private_data->userdata, private_data->text);
      }
    }
    else if((key == XK_Escape) || (key == XK_Tab)) {
      private_data->cursor_pos = -1;
      it->have_focus = private_data->have_focus = FOCUS_LOST;
      paint_inputtext(it, wl->win, wl->gc);
    }
    else if((buf[0] != 0) && (buf[1] == 0))  {
      char *oldtext;
      int pos;
      
      if((private_data->text != NULL) 
	 && (strlen(private_data->text) < private_data->max_length)) {

	oldtext = strdup(private_data->text);
	
	if(strlen(private_data->text) <= private_data->cursor_pos)
	  pos = strlen(private_data->text);
	else
	  pos = private_data->cursor_pos;

	free(private_data->text);
	private_data->text = (char *) gui_xmalloc(strlen(oldtext) + 3);

	sprintf(private_data->text, "%s", oldtext);
	sprintf(&private_data->text[pos], "%c%s%c",
		buf[0], &oldtext[pos], 0);

	private_data->cursor_pos++;
	paint_inputtext(it, wl->win, wl->gc);
	free(oldtext);
      }
      
    }
    else {
      //      printf("got unhandled key = [%ld]\n", key);
    }
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify key event input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif

}

/*
 * Return the text of widget.
 */
char *inputttext_get_text(widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    return private_data->text;
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "change input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif
  
  return NULL;
}

/*
 * Change and redisplay the text of widget.
 */
void inputtext_change_text(widget_list_t *wl, widget_t *it, const char *text) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    free(private_data->text);
    private_data->text = strdup((text != NULL)?text:"");
    private_data->cursor_pos = -1;
    paint_inputtext(it, wl->win, wl->gc);
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "change input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif
}

/*
 * Create input text box
 */
widget_t *inputtext_create (xitk_inputtext_t *it) {
  widget_t                  *mywidget;
  inputtext_private_data_t  *private_data;
  
  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));
  
  private_data                = (inputtext_private_data_t *) 
    gui_xmalloc (sizeof(inputtext_private_data_t));
  
  private_data->display       = it->display;
  
  private_data->iWidget       = mywidget;
  private_data->text          = strdup((it->text != NULL)?it->text:"");
  
  
  private_data->max_length    = it->max_length;
  private_data->cursor_pos    = -1;

  private_data->skin          = gui_load_image(it->imlibdata, it->skin_filename);

  private_data->max_visible   = (private_data->skin->width/2) / 6;
  private_data->disp_offset   = 0;

  private_data->callback      = it->callback;
  private_data->userdata      = it->userdata;

  private_data->normal_color  = strdup(it->normal_color);
  private_data->focused_color = strdup(it->focused_color);

  mywidget->private_data      = private_data;

  mywidget->enable            = 1;
  mywidget->have_focus        = FOCUS_LOST;
  mywidget->x                 = it->x;
  mywidget->y                 = it->y;

  mywidget->width             = private_data->skin->width/2;
  mywidget->height            = private_data->skin->height;
  mywidget->widget_type       = WIDGET_TYPE_INPUTTEXT;

  mywidget->paint             = paint_inputtext;
  mywidget->notify_click      = notify_click_inputtext;
  mywidget->notify_focus      = notify_focus_inputtext;
  mywidget->notify_keyevent   = notify_keyevent_inputtext;

  return mywidget;
}
