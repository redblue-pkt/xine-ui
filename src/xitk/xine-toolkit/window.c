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
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "Imlib-light/Imlib.h"
#include "labelbutton.h"
#include "image.h"
#include "font.h"
#include "window.h"
#include "widget.h"
#include "widget_types.h"

#include "_xitk.h"

#define DIALOG_TYPE_UNKNOWN        0
#define DIALOG_TYPE_OK             1
#define DIALOG_TYPE_YESNO          2
#define DIALOG_TYPE_YESNOCANCEL    3
#define DIALOG_TYPE_BUTTONLESS     4

static void _xitk_window_destroy_window(xitk_widget_t *, void *);

#define TITLE_BAR_HEIGHT 20

/*
 * Is window is size match with given args
 */
int xitk_is_window_visible(Display *display, Window window) {
  XWindowAttributes  wattr;
  Status             status;
  
  if((display == NULL) || (window == None))
    return -1;
  
  XLOCK(display);
  status = XGetWindowAttributes(display, window, &wattr);
  XUNLOCK(display);
  
  if((status != BadDrawable) && (status != BadWindow) && (wattr.map_state == IsViewable))
    return 1;
  
  return 0;
}

/*
 * Is window is size match with given args
 */
int xitk_is_window_size(Display *display, Window window, int width, int height) {
  XWindowAttributes  wattr;
  
  if((display == NULL) || (window == None))
    return -1;
  
  XLOCK(display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    XUNLOCK(display);
    return -1;
  }
  XUNLOCK(display);
  
  if((wattr.width == width) && (wattr.height == height))
    return 1;
  
  return 0;
}

/*
 * Set/Change window title.
 */
void xitk_set_window_title(Display *display, Window window, char *title) {

  if((display == NULL) || (window == None) || (title == NULL))
    return;

  XLOCK(display);
  XmbSetWMProperties(display, window, title, title, NULL, 0, NULL, NULL, NULL);
  XUNLOCK(display);
}

/*
 * Set/Change window title.
 */
void xitk_window_set_window_title(ImlibData *im, xitk_window_t *w, char *title) {

  if((im == NULL) || (w == NULL) || (title == NULL))
    return;

  xitk_set_window_title(im->x.disp, w->window, title);
}

/*
 * Get (safely) window pos.
 */
void xitk_get_window_position(Display *display, Window window, 
			      int *x, int *y, int *width, int *height) {
  XWindowAttributes  wattr;
  Window             wdummy;
  int                xx = 0, yy = 0;

  if((display == NULL) || (window == None))
    return;

  XLOCK(display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    wattr.width = wattr.height = 0;
    goto __failure;    
  }
  
  (void) XTranslateCoordinates (display, window, wattr.root, 
				-wattr.border_width, -wattr.border_width,
                                &xx, &yy, &wdummy);
  
 __failure:
  
  XUNLOCK(display);
  
  if(x)
    *x = xx;
  if(y)
    *y = yy;
  if(width)
    *width = wattr.width;
  if(height)
    *height = wattr.height;
}

/*
 * Get (safely) window pos.
 */
void xitk_window_get_window_position(ImlibData *im, xitk_window_t *w, 
				     int *x, int *y, int *width, int *height) {
  
  if((im == NULL) || (w == NULL))
    return;

  xitk_get_window_position(im->x.disp, w->window, x, y, width, height);
}

/*
 * Center a window in root window.
 */
void xitk_window_move_window(ImlibData *im, xitk_window_t *w, int x, int y) {
    
  if((im == NULL) || (w == NULL))
    return;

  XLOCK(im->x.disp);
  XMoveResizeWindow (im->x.disp, w->window, x, y, w->width, w->height);
  XUNLOCK(im->x.disp);

}

/*
 * Center a window in root window.
 */
void xitk_window_center_window(ImlibData *im, xitk_window_t *w) {
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;
  int           xx = 0, yy = 0;
    
  if((im == NULL) || (w == NULL))
    return;

  XLOCK(im->x.disp);
  if(XGetGeometry(im->x.disp, im->x.root, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    xx = (wwin / 2) - (w->width / 2);
    yy = (hwin / 2) - (w->height / 2);
  }

  XMoveResizeWindow (im->x.disp, w->window, xx, yy, w->width, w->height);
  XUNLOCK(im->x.disp);
}

/*
 * Create a simple (empty) window.
 */
xitk_window_t *xitk_window_create_window(ImlibData *im, int x, int y, int width, int height) {
  xitk_window_t         *xwin;
  char                   title[] = {"Xitk Window"};
  XSizeHints             hint;
  XWMHints              *wm_hint;
  XSetWindowAttributes   attr;
  Atom                   prop, XA_WIN_LAYER;
  XColor                 black, dummy;
  MWMHints               mwmhints;
  XClassHint            *xclasshint;
  long                   data[1];

  if((im == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin             = (xitk_window_t *) xitk_xmalloc(sizeof(xitk_window_t));
  xwin->background = NULL;
  xwin->width      = width;
  xwin->height     = height;
  xwin->parent     = NULL;
  
  memset(&hint, 0, sizeof(hint));
  hint.x               = x;
  hint.y               = y;
  hint.width           = width;
  hint.base_width      = width;
  hint.min_width       = width;
  hint.height          = height;
  hint.base_height     = height;
  hint.min_height      = height;
  hint.win_gravity     = NorthWestGravity;
  hint.flags           = PWinGravity | PBaseSize | PMinSize | USSize | USPosition;
  
  XLOCK(im->x.disp);
  XAllocNamedColor(im->x.disp, Imlib_get_colormap(im), "black", &black, &dummy);
  XUNLOCK(im->x.disp);

  attr.override_redirect = False;
  attr.background_pixel  = black.pixel;
  attr.border_pixel      = black.pixel;
  attr.colormap          = Imlib_get_colormap(im);
  attr.win_gravity       = NorthWestGravity;

  XLOCK(im->x.disp);
  xwin->window = XCreateWindow(im->x.disp, im->x.root, hint.x, hint.y, hint.width, hint.height,
			       0, im->x.depth,  InputOutput, im->x.visual,
			       CWBackPixel | CWBorderPixel | CWColormap
			       | CWOverrideRedirect | CWWinGravity ,
			       &attr);
  
  XSetStandardProperties(im->x.disp, xwin->window, title, title, None, NULL, 0, &hint);

  XSelectInput(im->x.disp, xwin->window, INPUT_MOTION | KeymapStateMask);
  
  XA_WIN_LAYER = XInternAtom(im->x.disp, "_WIN_LAYER", False);
  
  data[0] = 10;
  XChangeProperty(im->x.disp, xwin->window, XA_WIN_LAYER,
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		  1);
  
  prop = XInternAtom(im->x.disp, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(im->x.disp, xwin->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Window";
    xclasshint->res_class = "Xitk";
    XSetClassHint(im->x.disp, xwin->window, xclasshint);
    XFree(xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags         = InputHint | StateHint;
    XSetWMHints(im->x.disp, xwin->window, wm_hint);
    XFree(wm_hint);
  }

  XUNLOCK(im->x.disp);

  return xwin;
}

/*
 * Create a simple painted window.
 */
xitk_window_t *xitk_window_create_simple_window(ImlibData *im, int x, int y, int width, int height) {
  xitk_window_t *xwin;
  
  if((im == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin = xitk_window_create_window(im, x, y, width, height);
  xwin->width = width;
  xwin->height = height;
  
  xwin->background = xitk_image_create_xitk_pixmap(im, width, height);
  draw_outter(im, xwin->background, width, height);
  xitk_window_apply_background(im, xwin);

  xitk_window_move_window(im, xwin, x, y);

  return xwin;
}

/*
 * Create a simple, with title bar, window.
 */
xitk_window_t *xitk_window_create_dialog_window(ImlibData *im, char *title, 
						int x, int y, int width, int height) {
  xitk_window_t *xwin;
  xitk_pixmap_t  *bar, *pix_bg;
  unsigned int   colorblack, colorwhite, colorgray, colordgray;
  xitk_font_t   *fs = NULL;
  int            lbear, rbear, wid, asc, des;
  int            bar_style = xitk_get_barstyle_feature();
  
  if((im == NULL) || (title == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin = xitk_window_create_simple_window(im, x, y, width, height);

  xitk_window_set_window_title(im, xwin, title);

  bar = xitk_image_create_xitk_pixmap(im, width, TITLE_BAR_HEIGHT);
  pix_bg = xitk_image_create_xitk_pixmap(im, width, height);

  fs = xitk_font_load_font(im->x.disp, DEFAULT_BOLD_FONT_12);
  xitk_font_set_font(fs, bar->gc);
  xitk_font_string_extent(fs, title, &lbear, &rbear, &wid, &asc, &des);

  XLOCK(im->x.disp);
  XCopyArea(im->x.disp, xwin->background->pixmap, pix_bg->pixmap, xwin->background->gc,
	    0, 0, width, height, 0, 0);
  XUNLOCK(im->x.disp);

  colorblack = xitk_get_pixel_color_black(im);
  colorwhite = xitk_get_pixel_color_white(im);
  colorgray = xitk_get_pixel_color_gray(im);
  colordgray = xitk_get_pixel_color_darkgray(im);

 /* Draw window title bar background */
  if(bar_style) {
    int s, bl = 255;
    unsigned int colorblue;

    colorblue = xitk_get_pixel_color_from_rgb(im, 0, 0, bl);
    XLOCK(im->x.disp);
    for(s = 0; s <= TITLE_BAR_HEIGHT; s++, bl -= 8) {
      XSetForeground(im->x.disp, bar->gc, colorblue);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, s, width, s);
      colorblue = xitk_get_pixel_color_from_rgb(im, 0, 0, bl);
    }
    XUNLOCK(im->x.disp);
  }
  else {
    int s;
    unsigned int c, cd;

    cd = xitk_get_pixel_color_from_rgb(im, 115, 12, 206);
    c = xitk_get_pixel_color_from_rgb(im, 135, 97, 168);

    draw_flat_with_color(im, bar, width, TITLE_BAR_HEIGHT, colorgray);
    draw_rectangular_inner_box(im, bar, 2, 2, width - 6, (TITLE_BAR_HEIGHT - 1 - 4));
    
    XLOCK(im->x.disp);
    for(s = 6; s <= (TITLE_BAR_HEIGHT - 6); s += 3) {
      XSetForeground(im->x.disp, bar->gc, c);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 5, s, (width - 8), s);
      XSetForeground(im->x.disp, bar->gc, cd);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 5, s+1, (width - 8), s+1);
    }
    
    XSetForeground(im->x.disp, bar->gc, colorgray);
    XFillRectangle(im->x.disp, bar->pixmap, bar->gc, 
		   ((width - wid) - TITLE_BAR_HEIGHT) - 10, 6, 
		   wid + 20, TITLE_BAR_HEIGHT - 1 - 8);
    XUNLOCK(im->x.disp);
  }
  
  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorwhite);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, 0, width, 0);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, 0, 0, TITLE_BAR_HEIGHT - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorblack);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, width - 1, 0, width - 1, TITLE_BAR_HEIGHT - 1);

  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 2, TITLE_BAR_HEIGHT - 1, width - 2, TITLE_BAR_HEIGHT - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colordgray);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, width - 2, 2, width - 2, TITLE_BAR_HEIGHT - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorblack);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, width - 1, 0, width - 1, height - 1);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, 0, height - 1, width - 1, height - 1);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colordgray);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, width - 2, 0, width - 2, height - 2);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, 2, height - 2, width - 2, height - 2);
  XUNLOCK(im->x.disp);

  XLOCK(im->x.disp);
  if(bar_style)
    XSetForeground(im->x.disp, bar->gc, colorwhite);
  else
    XSetForeground(im->x.disp, bar->gc, (xitk_get_pixel_color_from_rgb(im, 85, 12, 135)));
  
  XDrawString(im->x.disp, bar->pixmap, bar->gc, 
	      (width - wid) - TITLE_BAR_HEIGHT, ((TITLE_BAR_HEIGHT+asc+des) >> 1) - des, 
	      title, strlen(title));
  XUNLOCK(im->x.disp);

  xitk_font_unload_font(fs);

  XLOCK(im->x.disp);
  XCopyArea(im->x.disp, bar->pixmap, pix_bg->pixmap, bar->gc, 0, 0, width, TITLE_BAR_HEIGHT, 0, 0);
  XUNLOCK(im->x.disp);
  
  xitk_window_change_background(im, xwin, pix_bg->pixmap, width, height);
  
  xitk_image_destroy_xitk_pixmap(bar);
  xitk_image_destroy_xitk_pixmap(pix_bg);

  return xwin;
}

/*
 * Get window sizes.
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height) {

  if(w == NULL)
    return;

  *width = w->width;
  *height = w->height;
}

/*
 * Get window (X) id.
 */
Window xitk_window_get_window(xitk_window_t *w) {

  if(w == NULL)
    return None;

  return w->window;
}

/*
 * Return window background pixmap.
 */
Pixmap xitk_window_get_background(xitk_window_t *w) {

  if(w == NULL)
    return None;

  return w->background->pixmap;
}

/*
 * Apply (draw) window background.
 */
void xitk_window_apply_background(ImlibData *im, xitk_window_t *w) {

  if((im == NULL) || (w == NULL))
    return;

  XLOCK(im->x.disp);
  XSetWindowBackgroundPixmap(im->x.disp, w->window, w->background->pixmap);
  XClearWindow(im->x.disp, w->window);
  XUNLOCK(im->x.disp);
}

/*
 * Change window background with 'bg', then draw it.
 */
int xitk_window_change_background(ImlibData *im,
				  xitk_window_t *w, Pixmap bg, int width, int height) {
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;

  if((im == NULL) || (w == NULL) || (bg == None) || (width == 0 || height == 0))
    return 0;

  xitk_image_destroy_xitk_pixmap(w->background);
  w->background = xitk_image_create_xitk_pixmap(im, width, height);
  
  XLOCK(im->x.disp);
  if(XGetGeometry(im->x.disp, w->window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    XResizeWindow (im->x.disp, w->window, wwin, hwin);
  }
  else
    return 0;

  XCopyArea(im->x.disp, bg, w->background->pixmap, w->background->gc, 0, 0, width, height, 0, 0);
  XUNLOCK(im->x.disp);

  xitk_window_apply_background(im, w);

  return 1;
}

/*
 * Local XEvent handling.
 */
static void _window_handle_event(XEvent *event, void *data) {
  xitk_dialog_t *wd = (xitk_dialog_t *)data;
  
  switch(event->type) {

  case Expose:
    if(wd->widget_list) {
      wd->widget_list->widget_focused = wd->default_button;
      if (wd->widget_list->widget_focused->notify_focus 
	  && wd->widget_list->widget_focused->enable == WIDGET_ENABLE) {
	xitk_widget_t *w = wd->widget_list->widget_focused;
	
	(void) (w->notify_focus) (wd->widget_list, w, FOCUS_RECEIVED);
	w->have_focus = FOCUS_RECEIVED;
	
	if(w->paint)
	  (w->paint) (w, wd->widget_list->win, wd->widget_list->gc);
      }
    }
    break;
    
  case MappingNotify:
    XLOCK(wd->imlibdata->x.disp);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUNLOCK(wd->imlibdata->x.disp);
    break;

  case KeyPress: {
    XKeyEvent  mykeyevent;
    KeySym     mykey;
    char       kbuf[256];
    int        len;
    
    mykeyevent = event->xkey;
    
    XLOCK(wd->imlibdata->x.disp);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUNLOCK(wd->imlibdata->x.disp);
    
    switch (mykey) {

    case XK_Return:
      if(wd->default_button)
	_xitk_window_destroy_window(wd->default_button, (void *)wd);
      break;
      
    case XK_Escape:
      if((wd->type == DIALOG_TYPE_YESNO) || (wd->type == DIALOG_TYPE_YESNOCANCEL)) {
	if(wd->default_button)
	  _xitk_window_destroy_window(wd->default_button, (void *)wd);
      }
      else {
	if(wd->type != DIALOG_TYPE_BUTTONLESS)
	  _xitk_window_destroy_window(NULL, (void *)wd);
      }
      break;

    }
    
  }
  break;    
  }
}

void xitk_window_destroy_window(ImlibData *im, xitk_window_t *w) {

  XLOCK(im->x.disp);
  XUnmapWindow(im->x.disp, w->window);
  XUNLOCK(im->x.disp);

  if(w->background)
    xitk_image_destroy_xitk_pixmap(w->background);

  w->width = -1;
  w->height = -1;
  XLOCK(im->x.disp);
  XDestroyWindow(im->x.disp, w->window);
  XUNLOCK(im->x.disp);

  XITK_FREE(w);
}

void xitk_window_dialog_destroy(xitk_window_t *w) {
  xitk_dialog_t *wd = w->parent;

  if(wd) {
    xitk_window_destroy_window(wd->imlibdata, wd->xwin);
    xitk_unregister_event_handler(&wd->key);
    
    if(wd->widget_list) {
      xitk_destroy_widgets(wd->widget_list);
      XLOCK(wd->imlibdata->x.disp);
      XFreeGC(wd->imlibdata->x.disp, wd->widget_list->gc);
      XUNLOCK(wd->imlibdata->x.disp);
      
      xitk_list_free(wd->widget_list->l);
      
      XITK_FREE(wd->widget_list);
    }
    XITK_FREE(wd);
  }  
}

/*
 * Unmap and free window components.
 */
static void _xitk_window_destroy_window(xitk_widget_t *w, void *data) {
  xitk_dialog_t *wd = (xitk_dialog_t *)data;

  xitk_window_destroy_window(wd->imlibdata, wd->xwin);
  xitk_unregister_event_handler(&wd->key);

  switch(wd->type) {

  case DIALOG_TYPE_OK:
    if(wd->yescallback)
      wd->yescallback(NULL, wd->userdata, XITK_WINDOW_ANSWER_OK);
    break;
    
  case DIALOG_TYPE_YESNO:
  case DIALOG_TYPE_YESNOCANCEL:
    if(w == wd->wyes) {
      if(wd->yescallback)
	wd->yescallback(NULL, wd->userdata, XITK_WINDOW_ANSWER_YES);
    }
    else if(w == wd->wno) {
      if(wd->nocallback)
	wd->nocallback(NULL, wd->userdata, XITK_WINDOW_ANSWER_NO);
    }
    else if(w == wd->wcancel) {
      if(wd->cancelcallback)
	wd->cancelcallback(NULL, wd->userdata, XITK_WINDOW_ANSWER_CANCEL);
    }
    break;
    
  case DIALOG_TYPE_BUTTONLESS:
    /* NOOP */
    break;

  default:
    XITK_WARNING("window dialog type unknown: %d\n", wd->type);
    break;

  }

  if(wd->widget_list) {
    xitk_destroy_widgets(wd->widget_list);
    
    XLOCK(wd->imlibdata->x.disp);
    XFreeGC(wd->imlibdata->x.disp, wd->widget_list->gc);
    XUNLOCK(wd->imlibdata->x.disp);
    
    xitk_list_free(wd->widget_list->l);
    
    XITK_FREE(wd->widget_list);
  }
  
  XITK_FREE(wd);
  
}

xitk_window_t *xitk_window_dialog_button_free_with_width(ImlibData *im, char *title,
							 int window_width, int align, char *message, ...) {
  xitk_dialog_t              *wd;
  int                         windoww = window_width, windowh;
  xitk_image_t               *i;

  if((im == NULL) || (window_width == 0) || (message == NULL))
    return NULL;

  wd = (xitk_dialog_t *) xitk_xmalloc(sizeof(xitk_dialog_t));

  {
    va_list   args;
    char     *buf;
    int       n, size = 100;
    
    if((buf = xitk_xmalloc(size)) == NULL) 
      return NULL;
    
    while(1) {

      va_start(args, message);
      n = vsnprintf(buf, size, message, args);
      va_end(args);
      
      if(n > -1 && n < size)
	break;
      
      if(n > -1)
	size = n + 1;
      else
	size *= 2;

      if((buf = realloc(buf, size)) == NULL)
	return NULL;
    }
    
    i = xitk_image_create_image_from_string(im, DEFAULT_FONT_12, windoww - 40, align, buf);
    XITK_FREE(buf);
  }
  
  windowh = (i->height) + (TITLE_BAR_HEIGHT + 40);

  wd->imlibdata = im;
  wd->type = DIALOG_TYPE_BUTTONLESS;
  wd->wyes = wd->wno = wd->wcancel = wd->default_button = NULL;
  wd->widget_list = NULL;
  wd->xwin = xitk_window_create_dialog_window(im, ((title != NULL) ? title : "Notice"),
					      0, 0, windoww, windowh);
  wd->xwin->parent = wd;
  
  xitk_window_center_window(im, wd->xwin);
  
  wd->widget_list = NULL;

  /* Draw text area */
  {
    xitk_pixmap_t  *bg;
    int             width, height;
    GC              gc;
    
    xitk_window_get_window_size(wd->xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(im, width, height);
    
    XLOCK(im->x.disp);
    gc = XCreateGC(im->x.disp, (xitk_window_get_background(wd->xwin)), None, None);
    XCopyArea(im->x.disp, (xitk_window_get_background(wd->xwin)), bg->pixmap,
	      gc, 0, 0, width, height, 0, 0);
    XCopyArea(im->x.disp, i->image->pixmap, bg->pixmap,
	      i->image->gc, 0, 0, i->width, i->height, 20, (TITLE_BAR_HEIGHT + 20));
    XUNLOCK(im->x.disp);

    xitk_window_change_background(im, wd->xwin, bg->pixmap, width, height);

    xitk_image_destroy_xitk_pixmap(bg);

    XLOCK(im->x.disp);
    XFreeGC(im->x.disp, gc);
    XUNLOCK(im->x.disp);

    xitk_image_free_image(im, &i);

  }

  XLOCK(im->x.disp);
  XMapRaised(im->x.disp, (xitk_window_get_window(wd->xwin)));
  XUNLOCK(im->x.disp);

  wd->key = xitk_register_event_handler("xitk_nobtn", 
					(xitk_window_get_window(wd->xwin)),
					_window_handle_event,
					NULL,
					NULL,
					NULL,
					(void *)wd);
  
  return wd->xwin;
}
/*
 * Create a window error, containing an error message.
 */
xitk_window_t *xitk_window_dialog_one_button_with_width(ImlibData *im, char *title, char *button_label,
							xitk_state_callback_t cb, void *userdata, 
							int window_width, int align, char *message, ...) {
  xitk_dialog_t              *wd;
  xitk_labelbutton_widget_t   lb;
  int                         windoww = window_width, windowh;
  xitk_image_t               *i;
  int                         bwidth = 100, bx, by;

  if((im == NULL) || (window_width == 0) || (message == NULL))
    return NULL;

  wd = (xitk_dialog_t *) xitk_xmalloc(sizeof(xitk_dialog_t));

  {
    va_list   args;
    char     *buf;
    int       n, size = 100;
    
    if((buf = xitk_xmalloc(size)) == NULL) 
      return NULL;
    
    while(1) {

      va_start(args, message);
      n = vsnprintf(buf, size, message, args);
      va_end(args);
      
      if(n > -1 && n < size)
	break;
      
      if(n > -1)
	size = n + 1;
      else
	size *= 2;

      if((buf = realloc(buf, size)) == NULL)
	return NULL;
    }
    
    i = xitk_image_create_image_from_string(im, DEFAULT_FONT_12, windoww - 40, align, buf);
    XITK_FREE(buf);
  }
  
  windowh = (i->height + 50) + (TITLE_BAR_HEIGHT + 40);

  wd->imlibdata = im;
  wd->type = DIALOG_TYPE_OK;
  wd->yescallback = cb;
  wd->userdata = userdata;
  wd->xwin = xitk_window_create_dialog_window(im, ((title != NULL) ? title : "Notice"),
					      0, 0, windoww, windowh);
  wd->xwin->parent = wd;

  xitk_window_center_window(im, wd->xwin);
  
  wd->widget_list                = xitk_widget_list_new();
  wd->widget_list->l             = xitk_list_new ();
  wd->widget_list->win           = (xitk_window_get_window(wd->xwin));

  XLOCK(wd->imlibdata->x.disp);
  wd->widget_list->gc            = (XCreateGC(im->x.disp, (xitk_window_get_window(wd->xwin)),
					      None, None));
  XUNLOCK(wd->imlibdata->x.disp);
				
  /* OK button */
  if(bwidth > windoww)
    bwidth = (windoww - 4);
  
  bx = ((windoww - bwidth) / 2);
  by = windowh - 50;

  XITK_WIDGET_INIT(&lb, im);
  lb.button_type       = CLICK_BUTTON;
  lb.label             = button_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wyes =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));
  wd->default_button = wd->wyes;

  /* Draw text area */
  {
    xitk_pixmap_t  *bg;
    int             width, height;
    GC              gc;
    
    xitk_window_get_window_size(wd->xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(im, width, height);
    
    XLOCK(im->x.disp);
    gc = XCreateGC(im->x.disp, (xitk_window_get_background(wd->xwin)), None, None);
    XCopyArea(im->x.disp, (xitk_window_get_background(wd->xwin)), bg->pixmap,
	      gc, 0, 0, width, height, 0, 0);
    XCopyArea(im->x.disp, i->image->pixmap, bg->pixmap,
	      i->image->gc, 0, 0, i->width, i->height, 20, (TITLE_BAR_HEIGHT + 20));
    XUNLOCK(im->x.disp);

    xitk_window_change_background(im, wd->xwin, bg->pixmap, width, height);

    xitk_image_destroy_xitk_pixmap(bg);

    XLOCK(im->x.disp);
    XFreeGC(im->x.disp, gc);
    XUNLOCK(im->x.disp);

    xitk_image_free_image(im, &i);

  }

  XLOCK(im->x.disp);
  XMapRaised(im->x.disp, (xitk_window_get_window(wd->xwin)));
  XUNLOCK(im->x.disp);

  wd->key = xitk_register_event_handler("xitk_1btn", 
					(xitk_window_get_window(wd->xwin)),
					_window_handle_event,
					NULL,
					NULL,
					wd->widget_list,
					(void *)wd);

  return wd->xwin;
}

xitk_window_t *xitk_window_dialog_ok_with_width(ImlibData *im, char *title,
						xitk_state_callback_t cb, void *userdata, 
						int window_width, int align, char *message, ...) {
  
  va_list        args;
  char          *buf;
  int            n, size = 100;
  xitk_window_t *xw = NULL;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return NULL;
  
  while(1) {

    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return NULL;
  }
  
  {
    char buf2[(strlen(buf) * 2) + 1];
    xitk_subst_special_chars(buf, buf2);
    xw = xitk_window_dialog_one_button_with_width(im, title, _("OK"), cb, userdata, window_width,
						  align, buf2);
  }
  XITK_FREE(buf);
  return xw;
}

/*
 * Create an interactive window, containing 'yes', 'no', 'cancel' buttons.
 */
xitk_window_t *xitk_window_dialog_two_buttons_with_width(ImlibData *im, char *title,
							 char *button1_label, char *button2_label,
							 xitk_state_callback_t cb1, 
							 xitk_state_callback_t cb2, 
							 void *userdata, 
							 int window_width, int align, char *message, ...) {
  xitk_dialog_t              *wd;
  xitk_labelbutton_widget_t   lb;
  int                         windoww = window_width, windowh;
  xitk_image_t               *i;
  int                         bwidth = 100, bx1, bx2, by;

  if((im == NULL) || (window_width == 0) || (message == NULL))
    return NULL;

  wd = (xitk_dialog_t *) xitk_xmalloc(sizeof(xitk_dialog_t));

  {
    va_list   args;
    char     *buf;
    int       n, size = 100;
    
    if((buf = xitk_xmalloc(size)) == NULL) 
      return NULL;
    
    while(1) {
      
      va_start(args, message);
      n = vsnprintf(buf, size, message, args);
      va_end(args);
      
      if(n > -1 && n < size)
	break;
      
      if(n > -1)
	size = n + 1;
      else
	size *= 2;

      if((buf = realloc(buf, size)) == NULL)
	return NULL;
    }
    
    i = xitk_image_create_image_from_string(im, DEFAULT_FONT_12, windoww - 40, align, buf);
    XITK_FREE(buf);
  }

  windowh = (i->height + 50) + (TITLE_BAR_HEIGHT + 40);

  wd->imlibdata = im;
  wd->type = DIALOG_TYPE_YESNO;
  wd->yescallback = cb1;
  wd->nocallback = cb2;
  wd->userdata = userdata;
  wd->xwin = xitk_window_create_dialog_window(im, ((title != NULL) ? title : _("Question?")), 
					      0, 0, windoww, windowh);  
  wd->xwin->parent = wd;
  xitk_window_center_window(im, wd->xwin);
  
  wd->widget_list                = xitk_widget_list_new();
  wd->widget_list->l             = xitk_list_new ();
  wd->widget_list->win           = (xitk_window_get_window(wd->xwin));
  XLOCK(wd->imlibdata->x.disp);
  wd->widget_list->gc            = (XCreateGC(im->x.disp, (xitk_window_get_window(wd->xwin)),
					      None, None));
  XUNLOCK(wd->imlibdata->x.disp);
				
  /* Buttons */
  if((bwidth * 2) > windoww)
    bwidth = (windoww / 2) - 8;

  bx1 = ((windoww - (bwidth * 2)) / 3);
  bx2 = (bx1 + bwidth) + bx1;
  by = windowh - 50;

  XITK_WIDGET_INIT(&lb, im);
  lb.button_type       = CLICK_BUTTON;
  lb.label             = button1_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wyes =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx1, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = button2_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wno =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx2, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));

  wd->default_button = wd->wno;

  /* Draw text area */
  {
    xitk_pixmap_t *bg;
    int            width, height;
    GC             gc;
    
    xitk_window_get_window_size(wd->xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(im, width, height);
    
    XLOCK(im->x.disp);
    gc = XCreateGC(im->x.disp, (xitk_window_get_background(wd->xwin)), None, None);
    XCopyArea(im->x.disp, (xitk_window_get_background(wd->xwin)), bg->pixmap,
	      gc, 0, 0, width, height, 0, 0);
    XCopyArea(im->x.disp, i->image->pixmap, bg->pixmap,
	      i->image->gc, 0, 0, i->width, i->height, 20, (TITLE_BAR_HEIGHT + 20));
    XUNLOCK(im->x.disp);

    xitk_window_change_background(im, wd->xwin, bg->pixmap, width, height);

    xitk_image_destroy_xitk_pixmap(bg);

    XLOCK(im->x.disp);
    XFreeGC(im->x.disp, gc);
    XUNLOCK(im->x.disp);

    xitk_image_free_image(im, &i);

  }

  XLOCK(im->x.disp);
  XMapRaised(im->x.disp, (xitk_window_get_window(wd->xwin)));
  XUNLOCK(im->x.disp);

  wd->key = xitk_register_event_handler("xitk_2btns", 
					(xitk_window_get_window(wd->xwin)),
					_window_handle_event,
					NULL,
					NULL,
					wd->widget_list,
					(void *)wd);
  return wd->xwin;
}

/*
 *
 */
xitk_window_t *xitk_window_dialog_yesno_with_width(ImlibData *im, char *title,
						   xitk_state_callback_t ycb, 
						   xitk_state_callback_t ncb, 
						   void *userdata, 
						   int window_width, int align, char *message, ...) {
  va_list        args;
  char          *buf;
  int            n, size = 100;
  xitk_window_t *xw = NULL;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return NULL;
  
  while(1) {
    
    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return NULL;
  }
  
  {
    char buf2[(strlen(buf) * 2) + 1];
    
    xitk_subst_special_chars(buf, buf2);
    xw = xitk_window_dialog_two_buttons_with_width(im, title, _("Yes"), _("No"), 
						   ycb, ncb, userdata, window_width, align, buf2);
  }

  XITK_FREE(buf);
  return xw;
}

/*
 * Create an interactive window, containing 'yes', 'no', 'cancel' buttons.
 */
xitk_window_t *xitk_window_dialog_three_buttons_with_width(ImlibData *im, char *title,
							   char *button1_label,
							   char *button2_label,
							   char *button3_label,
							   xitk_state_callback_t cb1, 
							   xitk_state_callback_t cb2, 
							   xitk_state_callback_t cb3, 
							   void *userdata, 
							   int window_width, int align, char *message, ...) {
  xitk_dialog_t              *wd;
  xitk_labelbutton_widget_t   lb;
  int                         windoww = window_width, windowh;
  xitk_image_t               *i;
  int                         bwidth = 100, bx1, bx2, bx3, by;

  if((im == NULL) || (window_width == 0) || (message == NULL))
    return NULL;

  wd = (xitk_dialog_t *) xitk_xmalloc(sizeof(xitk_dialog_t));

  {
    va_list   args;
    char     *buf;
    int       n, size = 100;
    
    if((buf = xitk_xmalloc(size)) == NULL) 
      return NULL;
    
    while(1) {
      
      va_start(args, message);
      n = vsnprintf(buf, size, message, args);
      va_end(args);
      
      if(n > -1 && n < size)
	break;
      
      if(n > -1)
	size = n + 1;
      else
	size *= 2;

      if((buf = realloc(buf, size)) == NULL)
	return NULL;
    }
    
    i = xitk_image_create_image_from_string(im, DEFAULT_FONT_12, windoww - 40, align, buf);
    XITK_FREE(buf);
  }

  windowh = (i->height + 50) + (TITLE_BAR_HEIGHT + 40);

  wd->imlibdata      = im;
  wd->type           = DIALOG_TYPE_YESNOCANCEL;
  wd->yescallback    = cb1;
  wd->nocallback     = cb2;
  wd->cancelcallback = cb3;
  wd->userdata       = userdata;
  wd->xwin           = xitk_window_create_dialog_window(im, ((title != NULL) 
							     ? title : _("Question?")), 
							0, 0, windoww, windowh);  
  wd->xwin->parent = wd;

  xitk_window_center_window(im, wd->xwin);
  
  wd->widget_list                = xitk_widget_list_new();
  wd->widget_list->l             = xitk_list_new ();
  wd->widget_list->win           = (xitk_window_get_window(wd->xwin));
  XLOCK(wd->imlibdata->x.disp);
  wd->widget_list->gc            = (XCreateGC(im->x.disp, (xitk_window_get_window(wd->xwin)),
					      None, None));
  XUNLOCK(wd->imlibdata->x.disp);
				
  /* Buttons */
  if((bwidth * 3) > windoww)
    bwidth = (windoww / 3) - 12;

  bx1 = ((windoww - (bwidth * 3)) / 4);
  bx2 = (bx1 + bwidth) + bx1;
  bx3 = (bx2 + bwidth) + bx1;
  by = windowh - 50;

  XITK_WIDGET_INIT(&lb, im);
  lb.button_type       = CLICK_BUTTON;
  lb.label             = button1_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wyes =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx1, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = button2_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wno =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx2, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = button3_label;
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = _xitk_window_destroy_window;
  lb.state_callback    = NULL;
  lb.userdata          = (void*)wd;
  lb.skin_element_name = NULL;
  xitk_list_append_content(wd->widget_list->l, 
	   (wd->wcancel =
	    xitk_noskin_labelbutton_create(wd->widget_list, &lb,
					   bx3, by,
					   bwidth, 30,
					   "Black", "Black", "White", DEFAULT_BOLD_FONT_12)));

  wd->default_button = wd->wcancel;

  /* Draw text area */
  {
    xitk_pixmap_t *bg;
    int            width, height;
    GC             gc;
    
    xitk_window_get_window_size(wd->xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(im, width, height);
    
    XLOCK(im->x.disp);
    gc = XCreateGC(im->x.disp, (xitk_window_get_background(wd->xwin)), None, None);
    XCopyArea(im->x.disp, (xitk_window_get_background(wd->xwin)), bg->pixmap,
	      gc, 0, 0, width, height, 0, 0);
    XCopyArea(im->x.disp, i->image->pixmap, bg->pixmap,
	      i->image->gc, 0, 0, i->width, i->height, 20, (TITLE_BAR_HEIGHT + 20));
    XUNLOCK(im->x.disp);

    xitk_window_change_background(im, wd->xwin, bg->pixmap, width, height);

    xitk_image_destroy_xitk_pixmap(bg);
    XLOCK(im->x.disp);
    XFreeGC(im->x.disp, gc);
    XUNLOCK(im->x.disp);

    xitk_image_free_image(im, &i);

  }

  XLOCK(im->x.disp);
  XMapRaised(im->x.disp, (xitk_window_get_window(wd->xwin)));
  XUNLOCK(im->x.disp);

  wd->key = xitk_register_event_handler("xitk_3btns", 
					(xitk_window_get_window(wd->xwin)),
					_window_handle_event,
					NULL,
					NULL,
					wd->widget_list,
					(void *)wd);

  return wd->xwin;
}

xitk_window_t *xitk_window_dialog_yesnocancel_with_width(ImlibData *im, char *title,
							 xitk_state_callback_t ycb, 
							 xitk_state_callback_t ncb, 
							 xitk_state_callback_t ccb, 
							 void *userdata, 
							 int window_width, int align, char *message, ...) {
  va_list        args;
  char          *buf;
  int            n, size = 100;
  xitk_window_t *xw = NULL;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return NULL;
  
  while(1) {
    
    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return NULL;
  }
  
  {
    char buf2[(strlen(buf) * 2) + 1];
    
    xitk_subst_special_chars(buf, buf2);
    xw = xitk_window_dialog_three_buttons_with_width(im, title, _("Yes"), _("No"), _("Cancel"),
						     ycb, ncb, ccb, userdata, window_width, align, buf2);
  }
  XITK_FREE(buf);
  return xw;
}
