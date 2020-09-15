/* 
 * Copyright (C) 2000-2020 the xine project
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
#include <string.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include "_xitk.h"
#include "xitk.h"

#include "xitk_x11.h"

#define TITLE_BAR_HEIGHT 20

struct xitk_window_s {
  xitk_t                   *xitk;
  Window                    window;
  xitk_window_t            *win_parent;
  xitk_pixmap_t            *background;
  xitk_pixmap_t            *background_mask;
  int                       width;
  int                       height;

  xitk_widget_list_t       *widget_list;
};

static Window _xitk_window_get_window (xitk_window_t *w) {
  return w->window;
}

void xitk_window_set_input_focus (xitk_window_t *w) {
  xitk_lock_display (w->xitk);
  XSetInputFocus(w->xitk->display, w->window, RevertToParent, CurrentTime);
  xitk_unlock_display (w->xitk);
}

static int _xitk_window_set_focus (Display *display, Window window) {
  int t = 0;

  while ((!xitk_is_window_visible (display, window)) && (++t < 3))
    xitk_usec_sleep (5000);
  
  if (xitk_is_window_visible (display, window)) {
    xitk_x_lock_display (display);
    XSetInputFocus (display, window, RevertToParent, CurrentTime);
    xitk_x_unlock_display (display);
    return 0;
  }

  return -1;
}

static int _xitk_window_has_focus(Display *display, Window window) {
  Window focused_win;
  int revert;

  xitk_x_lock_display (display);
  XGetInputFocus(display, &focused_win, &revert);
  xitk_x_unlock_display (display);

  return (focused_win == window);
}

void xitk_try_to_set_input_focus(Display *display, Window window) {
  int retry = 0;

  if (_xitk_window_set_focus(display, window) < 0)
    return;

  do {

    /* Retry until the WM was mercyful to give us the focus (but not indefinitely) */

    xitk_x_lock_display (display);
    XSync(display, False);
    xitk_x_unlock_display (display);

    if (_xitk_window_has_focus(display, window))
      break;

    xitk_usec_sleep(5000);

    if (_xitk_window_set_focus(display, window) < 0)
      break;

  } while (retry++ < 30);
}

void xitk_window_try_to_set_input_focus(xitk_window_t *w) {

  if (w == NULL)
    return;

  return xitk_try_to_set_input_focus(w->xitk->display, w->window);
}

void xitk_window_set_parent_window(xitk_window_t *xwin, xitk_window_t *parent) {

  if(xwin)
    xwin->win_parent = parent;
}

/*
 *
 */

void xitk_window_define_window_cursor(xitk_window_t *w, xitk_cursors_t cursor) {
  xitk_cursors_define_window_cursor (w->xitk->display, w->window, cursor);
}

void xitk_window_restore_window_cursor(xitk_window_t *w) {
  xitk_cursors_restore_window_cursor (w->xitk->display, w->window);
}

void xitk_window_unset_wm_window_type(xitk_window_t *w, xitk_wm_window_type_t type) {
  if (w)
    xitk_unset_wm_window_type(w->window, type);
}

void xitk_window_set_wm_window_type(xitk_window_t *w, xitk_wm_window_type_t type) {
  if (w)
    xitk_set_wm_window_type(w->window, type);
}

/*
 *
 */

xitk_register_key_t xitk_window_register_event_handler(const char *name, xitk_window_t *w,
                                                       const xitk_event_cbs_t *cbs, void *user_data) {
  return xitk_register_event_handler_ext(name, w, cbs, user_data, w->widget_list);
}

/*
 *
 */

int xitk_window_grab_input(xitk_window_t *w, KeySym *keysym,
                           unsigned int *keycode, int *modifier, int *button) {
  XEvent xev;
  long mask = 0;

  if (!keysym && !keycode && !button)
    return -1;
  if (!w)
    return -1;

  if (button)
    mask |= (ButtonPressMask | ButtonReleaseMask);
  if (keysym || keycode)
    mask |= (KeyPressMask | KeyReleaseMask);

  if (keysym)
    *keysym = XK_VoidSymbol;
  if (keycode)
    *keycode = 0;
  if (modifier)
    *modifier = MODIFIER_NOMOD;
  if (button)
    *button = -1;

  do {
    /* Although only release events are evaluated, we must also grab the corresponding press */
    /* events to hide them from the other GUI windows and prevent unexpected side effects.   */
    xitk_lock_display (w->xitk);
    XMaskEvent(w->xitk->display, mask, &xev);
    xitk_unlock_display (w->xitk);
    if (xev.xany.window != xitk_window_get_window(w))
      return -1;
  } while (xev.type != KeyRelease && xev.type != ButtonRelease);

  switch (xev.type) {
    case ButtonRelease:
      if (modifier)
        xitk_get_key_modifier(&xev, modifier);
      if (button)
        *button = xev.xbutton.button;
      return 0;
    case KeyRelease:
      if (modifier)
        xitk_get_key_modifier(&xev, modifier);
      if (keysym)
        *keysym = xitk_get_key_pressed(&xev);
      if (keycode)
        *keycode = xev.xkey.keycode;
      return 0;
    default:
      break;
  }
  return -1;
}

/*
 *
 */
int xitk_is_window_iconified(Display *display, Window window) {
  unsigned char *prop_return = NULL;
  unsigned long  nitems_return;
  unsigned long  bytes_after_return;
  int            format_return;
  Atom           type_return, atom;
  int            retval = 0;
  
  xitk_x_lock_display (display);
  atom = XInternAtom(display, "WM_STATE", False);
  XGetWindowProperty (display, window, atom, 0, 0x7fffffff, False,
		      atom, &type_return, &format_return, &nitems_return, &bytes_after_return, &prop_return);
  
  if(prop_return) {
    if (prop_return[0] == IconicState)
      retval = 1;
    XFree(prop_return);
  }
  xitk_x_unlock_display (display);
 
  return retval;
}

/*
 * Is window is size match with given args
 */
int xitk_is_window_visible(Display *display, Window window) {
  XWindowAttributes  wattr;
  Status             status;
  
  if((display == NULL) || (window == None))
    return -1;
  
  xitk_x_lock_display (display);
  status = XGetWindowAttributes(display, window, &wattr);
  xitk_x_unlock_display (display);
  
  if((status != BadDrawable) && (status != BadWindow) && (wattr.map_state == IsViewable))
    return 1;
  
  return 0;
}

int xitk_window_is_window_visible(xitk_window_t *w) {

  if (w == NULL)
    return 0;

  return xitk_is_window_visible(w->xitk->display, w->window);
}

/*
 * Is window is size match with given args
 */
static int xitk_is_window_size(Display *display, Window window, int width, int height) {
  XWindowAttributes  wattr;
  
  if((display == NULL) || (window == None))
    return -1;
  
  xitk_x_lock_display (display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    xitk_x_unlock_display (display);
    return -1;
  }
  xitk_x_unlock_display (display);
  
  if((wattr.width == width) && (wattr.height == height))
    return 1;
  
  return 0;
}

/*
 * Set/Change window title.
 */
static void xitk_set_window_title(Display *display, Window window, const char *title) {

  if((display == NULL) || (window == None) || (title == NULL))
    return;

  xitk_x_lock_display (display);
  XmbSetWMProperties(display, window, title, title, NULL, 0, NULL, NULL, NULL);
  xitk_x_unlock_display (display);
}

/*
 * Set/Change window title.
 */
void xitk_window_set_window_title(xitk_window_t *w, const char *title) {

  if ((w == NULL) || (title == NULL))
    return;

  xitk_set_window_title(w->xitk->display, w->window, title);
}

/*
 *
 */

static void xitk_set_window_icon(Display *display, Window window, Pixmap icon) {

  XWMHints *wmhints;

  if ((display == NULL) || (window == None)) /* icon == None is valid */
    return;

  xitk_x_lock_display (display);

  wmhints = XAllocWMHints();
  if (wmhints) {
    wmhints->icon_pixmap   = icon;
    wmhints->flags         = IconPixmapHint;
    XSetWMHints(display, (window), wmhints);
    XFree(wmhints);
  }
  xitk_x_unlock_display (display);
}

void xitk_window_set_window_icon(xitk_window_t *w, xitk_pixmap_t *icon) {

  if (w == NULL)
    return;

  xitk_set_window_icon(w->xitk->display, w->window, xitk_pixmap_get_pixmap(icon));
}

void xitk_window_set_layer_above(xitk_window_t *w) {

  if (w == NULL)
    return;

  xitk_set_layer_above(w->window);
}

void xitk_window_set_window_layer(xitk_window_t *w, int layer) {

  if (w == NULL)
    return;

  xitk_set_window_layer(w->window, layer);
}

/*
 *
 */

static void xitk_set_window_class(Display *display, Window window, const char *res_name, const char *res_class) {

  XClassHint xclasshint, new_xclasshint;

  if ((display == NULL) || (window == None))
    return;

  xitk_x_lock_display (display);

  if ((XGetClassHint(display, (window), &xclasshint)) != 0) {
    new_xclasshint.res_name  = res_name  ? (char*)res_name  : xclasshint.res_name;
    new_xclasshint.res_class = res_class ? (char*)res_class : xclasshint.res_class;
    XSetClassHint(display, window, &new_xclasshint);
    XFree(xclasshint.res_name);
    XFree(xclasshint.res_class);
  }

  xitk_x_unlock_display (display);
}

void xitk_window_set_window_class(xitk_window_t *w, const char *res_name, const char *res_class) {

  if (w == NULL)
    return;

  xitk_set_window_class(w->xitk->display, w->window, res_name, res_class);
}

void xitk_window_set_transient_for(xitk_window_t *w, Window win) {
  xitk_lock_display (w->xitk);
  XSetTransientForHint(w->xitk->display, w->window, win);
  xitk_unlock_display (w->xitk);
}

void xitk_window_set_transient_for_win(xitk_window_t *w, xitk_window_t *xwin) {
  xitk_window_set_transient_for(w, xwin->window);
}

void xitk_window_raise_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XRaiseWindow(w->xitk->display, w->window);
  xitk_unlock_display (w->xitk);
}

void xitk_window_show_window(xitk_window_t *w, int raise)
{
  xitk_lock_display (w->xitk);
  if (raise)
    XRaiseWindow(w->xitk->display, w->window);
  XMapWindow(w->xitk->display, w->window);
  XSync(w->xitk->display, False);
  xitk_unlock_display (w->xitk);
}

void xitk_window_iconify_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XIconifyWindow(w->xitk->display, w->window, w->xitk->imlibdata->x.screen);
  xitk_unlock_display (w->xitk);
}

void xitk_window_hide_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XUnmapWindow(w->xitk->display, w->window);
  xitk_unlock_display (w->xitk);
}

void xitk_window_clear_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XClearWindow(w->xitk->display, w->window);
  xitk_unlock_display (w->xitk);
}

void xitk_window_reparent_window(xitk_window_t *w, xitk_window_t *parent, int x, int y)
{
  xitk_lock_display (w->xitk);
  XReparentWindow(w->xitk->display, w->window,
                  parent ? parent->window : w->xitk->imlibdata->x.root, x, y);
  xitk_unlock_display (w->xitk);
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

  xitk_x_lock_display (display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    wattr.width = wattr.height = 0;
    goto __failure;    
  }
  
  (void) XTranslateCoordinates (display, window, wattr.root, 
				-wattr.border_width, -wattr.border_width,
                                &xx, &yy, &wdummy);
  
 __failure:
  
  xitk_x_unlock_display (display);
  
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
void xitk_window_get_window_position(xitk_window_t *w, int *x, int *y, int *width, int *height) {

  if (w == NULL)
    return;

  xitk_get_window_position(w->xitk->display, w->window, x, y, width, height);
}

/*
 * Center a window in root window.
 */
void xitk_window_move_window(xitk_window_t *w, int x, int y) {

  if (w == NULL)
    return;

  xitk_lock_display (w->xitk);
  XMoveResizeWindow (w->xitk->display, w->window, x, y, w->width, w->height);
  xitk_unlock_display (w->xitk);

}

/*
 * Center a window in root window.
 */
void xitk_window_center_window(xitk_window_t *w) {
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;
  int           xx = 0, yy = 0;

  if (w == NULL)
    return;

  xitk_lock_display (w->xitk);
  if(XGetGeometry(w->xitk->display, w->xitk->imlibdata->x.root, &rootwin,
                  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {

    xx = (wwin / 2) - (w->width / 2);
    yy = (hwin / 2) - (w->height / 2);
  }

  XMoveResizeWindow (w->xitk->display, w->window, xx, yy, w->width, w->height);
  xitk_unlock_display (w->xitk);
}

/*
 * Create a simple (empty) window.
 */
xitk_window_t *xitk_window_create_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                             const char *title, const char *res_name, const char *res_class,
                                             int override_redirect, int layer_above, xitk_pixmap_t *icon) {
  xitk_window_t         *xwin;
  XSizeHints             hint;
  XWMHints              *wm_hint;
  XSetWindowAttributes   attr;
  Atom                   prop, XA_WIN_LAYER, XA_DELETE_WINDOW;
  XColor                 black, dummy;
  MWMHints               mwmhints;
  XClassHint            *xclasshint;
  long                   data[1];

  if((xitk == NULL) || (xitk->imlibdata == NULL) || (width == 0 || height == 0))
    return NULL;

  if (!title)
    title = "xiTK Window";

  xwin                  = (xitk_window_t *) xitk_xmalloc(sizeof(xitk_window_t));
  xwin->xitk            = xitk;
  xwin->win_parent      = NULL;
  xwin->background      = NULL;
  xwin->background_mask = NULL;
  xwin->width           = width;
  xwin->height          = height;

  memset(&hint, 0, sizeof(hint));
  hint.x               = x;
  hint.y               = y;
  hint.width           = width;
  hint.base_width      = width;
  hint.min_width       = width;
  hint.max_width       = width;
  hint.height          = height;
  hint.base_height     = height;
  hint.min_height      = height;
  hint.max_height      = height;
  hint.win_gravity     = NorthWestGravity;
  hint.flags           = PWinGravity | PBaseSize | PMinSize | PMaxSize | USSize | USPosition;
  
  xitk_lock_display (xitk);
  XAllocNamedColor(xitk->display, Imlib_get_colormap(xitk->imlibdata), "black", &black, &dummy);
  xitk_unlock_display (xitk);

  attr.override_redirect = override_redirect ? True : False;
  attr.background_pixel  = black.pixel;
  attr.border_pixel      = black.pixel;
  attr.colormap          = Imlib_get_colormap(xitk->imlibdata);
  attr.win_gravity       = NorthWestGravity;

  xitk_lock_display (xitk);
  xwin->window = XCreateWindow(xitk->display, xitk->imlibdata->x.root, hint.x, hint.y, hint.width, hint.height,
                               0, xitk->imlibdata->x.depth,  InputOutput, xitk->imlibdata->x.visual,
			       CWBackPixel | CWBorderPixel | CWColormap
			       | CWOverrideRedirect | CWWinGravity ,
			       &attr);
  
  XmbSetWMProperties(xitk->display, xwin->window, title, title, NULL, 0, &hint, NULL, NULL);

  XSelectInput(xitk->display, xwin->window, INPUT_MOTION | KeymapStateMask);

  XA_WIN_LAYER = XInternAtom(xitk->display, "_WIN_LAYER", False);
  
  data[0] = 10;
  XChangeProperty(xitk->display, xwin->window, XA_WIN_LAYER,
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		  1);

  memset(&mwmhints, 0, sizeof(mwmhints));
  prop = XInternAtom(xitk->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(xitk->display, xwin->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XA_DELETE_WINDOW = XInternAtom(xitk->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(xitk->display, xwin->window, &XA_DELETE_WINDOW, 1);

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name  = (char*)(res_name ? res_name : "Xine Window");
    xclasshint->res_class = (char*)(res_class ? res_class : "Xitk");
    XSetClassHint(xitk->display, xwin->window, xclasshint);
    XFree(xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags         = InputHint | StateHint;
    XSetWMHints(xitk->display, xwin->window, wm_hint);
    XFree(wm_hint);
  }

  if (icon)
    xitk_window_set_window_icon(xwin, icon);

  xitk_unlock_display (xitk);

  if (layer_above)
    xitk_window_set_window_layer (xwin, layer_above);

  return xwin;
}

xitk_window_t *xitk_window_create_window(xitk_t *xitk, int x, int y, int width, int height) {
  return xitk_window_create_window_ext(xitk, x, y, width, height,
                                       NULL, NULL, NULL, 0, 0, NULL);
}

/*
 * Create a simple painted window.
 */
xitk_window_t *xitk_window_create_simple_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                                    const char *title, const char *res_name, const char *res_class,
                                                    int override_redirect, int layer_above, xitk_pixmap_t *icon) {
  xitk_window_t *xwin;

  xwin = xitk_window_create_window_ext(xitk, x, y, width, height, title,
                                       res_name, res_class, override_redirect, layer_above, icon);
  if (!xwin)
    return NULL;

  xwin->width = width;
  xwin->height = height;
  
  xwin->background = xitk_image_create_xitk_pixmap(xitk, width, height);
  draw_outter(xwin->background, width, height);
  xitk_window_apply_background(xwin);

  xitk_window_move_window(xwin, x, y);

  return xwin;
}
xitk_window_t *xitk_window_create_simple_window(xitk_t *xitk, int x, int y, int width, int height) {
  return xitk_window_create_simple_window_ext(xitk, x, y, width, height, NULL, NULL, NULL, 0, 0, NULL);
}

xitk_widget_list_t *xitk_window_widget_list(xitk_window_t *w)
{
  if (w->widget_list)
    return w->widget_list;

  w->widget_list = xitk_widget_list_new (w->xitk);
  if (!w->widget_list)
    return NULL;

  xitk_lock_display (w->xitk);
  w->widget_list->gc = XCreateGC (w->xitk->display, w->window, None, None);
  xitk_unlock_display (w->xitk);

  w->widget_list->win = w->window;

  return w->widget_list;
}

/*
 * Create a simple, with title bar, window.
 */
xitk_window_t *xitk_window_create_dialog_window(xitk_t *xitk, const char *title,
						int x, int y, int width, int height) {
  xitk_window_t *xwin;
  xitk_pixmap_t  *bar, *pix_bg;
  unsigned int   colorblack, colorwhite, colorgray, colordgray;
  xitk_font_t   *fs = NULL;
  int            lbear, rbear, wid, asc, des;
  int            bar_style = xitk_get_barstyle_feature();

  if (title == NULL)
    return NULL;

  xwin = xitk_window_create_simple_window(xitk, x, y, width, height);
  if (!xwin)
    return NULL;

  xitk_window_set_window_title(xwin, title);

  bar = xitk_image_create_xitk_pixmap(xitk, width, TITLE_BAR_HEIGHT);
  pix_bg = xitk_image_create_xitk_pixmap(xitk, width, height);

  fs = xitk_font_load_font(xitk, DEFAULT_BOLD_FONT_12);
  xitk_font_set_font(fs, bar->gc);
  xitk_font_string_extent(fs, (title && strlen(title)) ? title : "Window", &lbear, &rbear, &wid, &asc, &des);

  xitk_lock_display (xitk);
  XCopyArea(xitk->display, xwin->background->pixmap, pix_bg->pixmap, xwin->background->gc,
	    0, 0, width, height, 0, 0);
  xitk_unlock_display (xitk);

  colorblack = xitk_get_pixel_color_black(xitk);
  colorwhite = xitk_get_pixel_color_white(xitk);
  colorgray = xitk_get_pixel_color_gray(xitk);
  colordgray = xitk_get_pixel_color_darkgray(xitk);

 /* Draw window title bar background */
  if(bar_style) {
    int s, bl = 255;
    unsigned int colorblue;

    colorblue = xitk_get_pixel_color_from_rgb(xitk, 0, 0, bl);
    xitk_lock_display (xitk);
    for(s = 0; s <= TITLE_BAR_HEIGHT; s++, bl -= 8) {
      XSetForeground(xitk->display, bar->gc, colorblue);
      XDrawLine(xitk->display, bar->pixmap, bar->gc, 0, s, width, s);
      colorblue = xitk_get_pixel_color_from_rgb(xitk, 0, 0, bl);
    }
    xitk_unlock_display (xitk);
  }
  else {
    int s;
    unsigned int c, cd;

    cd = xitk_get_pixel_color_from_rgb(xitk, 115, 12, 206);
    c = xitk_get_pixel_color_from_rgb(xitk, 135, 97, 168);

    draw_flat_with_color(bar, width, TITLE_BAR_HEIGHT, colorgray);
    draw_rectangular_inner_box(bar, 2, 2, width - 5, TITLE_BAR_HEIGHT - 4);
    
    xitk_lock_display (xitk);
    for(s = 6; s <= (TITLE_BAR_HEIGHT - 6); s += 3) {
      XSetForeground(xitk->display, bar->gc, c);
      XDrawLine(xitk->display, bar->pixmap, bar->gc, 5, s, (width - 8), s);
      XSetForeground(xitk->display, bar->gc, cd);
      XDrawLine(xitk->display, bar->pixmap, bar->gc, 5, s+1, (width - 8), s+1);
    }
    
    XSetForeground(xitk->display, bar->gc, colorgray);
    XFillRectangle(xitk->display, bar->pixmap, bar->gc,
		   ((width - wid) - TITLE_BAR_HEIGHT) - 10, 6, 
		   wid + 20, TITLE_BAR_HEIGHT - 1 - 8);
    xitk_unlock_display (xitk);
  }
  
  xitk_lock_display (xitk);
  XSetForeground(xitk->display, bar->gc, colorwhite);
  XDrawLine(xitk->display, bar->pixmap, bar->gc, 0, 0, width, 0);
  XDrawLine(xitk->display, bar->pixmap, bar->gc, 0, 0, 0, TITLE_BAR_HEIGHT - 1);
  xitk_unlock_display (xitk);

  xitk_lock_display (xitk);

  XSetForeground(xitk->display, bar->gc, colorblack);
  XDrawLine(xitk->display, bar->pixmap, bar->gc, width - 1, 0, width - 1, TITLE_BAR_HEIGHT - 1);
  XDrawLine(xitk->display, bar->pixmap, bar->gc, 2, TITLE_BAR_HEIGHT - 1, width - 2, TITLE_BAR_HEIGHT - 1);

  XSetForeground(xitk->display, bar->gc, colordgray);
  XDrawLine(xitk->display, bar->pixmap, bar->gc, width - 2, 2, width - 2, TITLE_BAR_HEIGHT - 1);

  XSetForeground(xitk->display, bar->gc, colorblack);
  XDrawLine(xitk->display, pix_bg->pixmap, bar->gc, width - 1, 0, width - 1, height - 1);
  XDrawLine(xitk->display, pix_bg->pixmap, bar->gc, 0, height - 1, width - 1, height - 1);

  XSetForeground(xitk->display, bar->gc, colordgray);
  XDrawLine(xitk->display, pix_bg->pixmap, bar->gc, width - 2, 0, width - 2, height - 2);
  XDrawLine(xitk->display, pix_bg->pixmap, bar->gc, 2, height - 2, width - 2, height - 2);

  xitk_unlock_display (xitk);

  xitk_lock_display (xitk);
  if(bar_style)
    XSetForeground(xitk->display, bar->gc, colorwhite);
  else
    XSetForeground(xitk->display, bar->gc, (xitk_get_pixel_color_from_rgb(xitk, 85, 12, 135)));
  xitk_unlock_display (xitk);

  xitk_font_draw_string(fs, bar, bar->gc,
			(width - wid) - TITLE_BAR_HEIGHT, ((TITLE_BAR_HEIGHT+asc+des) >> 1) - des, title, strlen(title));

  xitk_font_unload_font(fs);

  xitk_lock_display (xitk);
  XCopyArea(xitk->display, bar->pixmap, pix_bg->pixmap, bar->gc, 0, 0, width, TITLE_BAR_HEIGHT, 0, 0);
  xitk_unlock_display (xitk);
  
  xitk_window_change_background(xwin, pix_bg->pixmap, width, height);
  
  xitk_image_destroy_xitk_pixmap(bar);
  xitk_image_destroy_xitk_pixmap(pix_bg);

  return xwin;
}

/*
 * Get window sizes.
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height) {

  if (w == NULL) {
    *width = 0;
    *height = 0;
    return;
  }

  *width = w->width;
  *height = w->height;
}

void xitk_window_resize_window(xitk_window_t *w, int width, int height) {
  XSizeHints   hint;
  int          t = 0;

  if (w == NULL)
    return;

  w->width = width;
  w->height = height;

  xitk_lock_display (w->xitk);

  hint.width  = width;
  hint.height = height;
  hint.flags  = PPosition | PSize;
  XSetWMNormalHints(w->xitk->display, w->window, &hint);
  XResizeWindow (w->xitk->display, w->window, width, height);
  XSync(w->xitk->display, False);

  xitk_unlock_display (w->xitk);

  while (t < 10 && !xitk_is_window_size(w->xitk->display, w->window, w->width, w->height)) {
    xitk_usec_sleep(10000);
  }
}

/*
 * Get window (X) id.
 */
Window xitk_window_get_window(xitk_window_t *w) {
  return w ? _xitk_window_get_window (w) : None;
}

/*
 * Return window background pixmap.
 */
Pixmap xitk_window_get_background(xitk_window_t *w) {

  if(w == NULL)
    return None;

  return w->background->pixmap;
}

xitk_pixmap_t *xitk_window_get_background_pixmap(xitk_window_t *w) {

  xitk_pixmap_t *pixmap;
  int width, height;

  if(w == NULL)
    return None;

  xitk_window_get_window_size(w, &width, &height);
  pixmap = xitk_image_create_xitk_pixmap(w->xitk, width, height);

  xitk_lock_display (w->xitk);
  XCopyArea(w->xitk->display, xitk_window_get_background(w), pixmap->pixmap,
            pixmap->gc, 0, 0, width, height, 0, 0);
  xitk_unlock_display (w->xitk);

  return pixmap;
}

/*
 * Return window background pixmap.
 */
#ifdef YET_UNUSED
Pixmap xitk_window_get_background_mask(xitk_window_t *w) {

  if(w == NULL)
    return None;

  if(w->background_mask == NULL)
    return None;
  
  return w->background->pixmap;
}
#endif

/*
 * Apply (draw) window background.
 */
void xitk_window_apply_background(xitk_window_t *w) {

  if (w == NULL)
    return;

  xitk_lock_display (w->xitk);

  XSetWindowBackgroundPixmap(w->xitk->display, w->window, w->background->pixmap);

  if(w->background_mask)
    XShapeCombineMask(w->xitk->display, w->window, ShapeBounding, 0, 0, w->background_mask->pixmap, ShapeSet);
  else
    XShapeCombineMask(w->xitk->display, w->window, ShapeBounding, 0, 0, 0, ShapeSet);

  XClearWindow(w->xitk->display, w->window);
  xitk_unlock_display (w->xitk);
}

/*
 * Change window background with 'bg', then draw it.
 */
int xitk_window_change_background(xitk_window_t *w, Pixmap bg, int width, int height) {

  xitk_pixmap_t *pixmap;

  if ((w == NULL) || (bg == None) || (width == 0 || height == 0))
    return 0;

  pixmap = xitk_image_create_xitk_pixmap(w->xitk, width, height);

  xitk_lock_display (w->xitk);
  XCopyArea(w->xitk->display, bg, pixmap->pixmap, pixmap->gc, 0, 0, width, height, 0, 0);
  xitk_unlock_display (w->xitk);

  return xitk_window_set_background(w, pixmap);
}

int xitk_window_set_background(xitk_window_t *w, xitk_pixmap_t *bg) {

  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;

  if ((w == NULL) || (bg == NULL))
    return 0;

  xitk_image_destroy_xitk_pixmap(w->background);
  if(w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->background      = bg;
  w->background_mask = NULL;

  xitk_lock_display (w->xitk);
  if(XGetGeometry(w->xitk->display, w->window, &rootwin,
                  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {

    XResizeWindow (w->xitk->display, w->window, wwin, hwin);
  }
  else {
    xitk_unlock_display (w->xitk);
    return 0;
  }

  xitk_unlock_display (w->xitk);

  xitk_window_apply_background(w);

  return 1;
}

/*
 * Change window background with img, then draw it.
 */
int xitk_window_change_background_with_image(xitk_window_t *w, xitk_image_t *img, int width, int height) {
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;

  if ((w == NULL) || (img == NULL) || (width == 0 || height == 0))
    return 0;

  if (w->background)
    xitk_image_destroy_xitk_pixmap(w->background);
  if(w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->background      = xitk_image_create_xitk_pixmap(w->xitk, width, height);
  w->background_mask = NULL;

  if(img->mask)
    w->background_mask = xitk_image_create_xitk_mask_pixmap(w->xitk, width, height);

  xitk_lock_display (w->xitk);
  if(XGetGeometry(w->xitk->display, w->window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    XResizeWindow (w->xitk->display, w->window, wwin, hwin);
  }
  else {
    xitk_unlock_display (w->xitk);
    return 0;
  }
 
  XCopyArea(w->xitk->display, img->image->pixmap, w->background->pixmap, w->background->gc, 0, 0, width, height, 0, 0);
  if(w->background_mask)
    XCopyArea(w->xitk->display, img->mask->pixmap, w->background_mask->pixmap, w->background_mask->gc, 0, 0, width, height, 0, 0);

  xitk_unlock_display (w->xitk);
  xitk_window_apply_background(w);

  return 1;
}

#ifdef YET_UNUSED
void xitk_window_set_modal(xitk_window_t *w) {
  xitk_modal_window(w->window);  
}
void xitk_window_dialog_set_modal(xitk_window_t *w) {
  xitk_dialog_t *wd = w->dialog;
  xitk_window_set_modal(wd->xwin);
}
#endif

static void _destroy_common(xitk_window_t *w, int destroy_window) {
  if (w->widget_list)
    xitk_destroy_widgets(w->widget_list);

  xitk_clipboard_unregister_window (w->window);

  if (destroy_window) {
    xitk_lock_display (w->xitk);
    XUnmapWindow(w->xitk->display, w->window);
    xitk_unlock_display (w->xitk);
  }

  if (w->background)
    xitk_image_destroy_xitk_pixmap(w->background);
  if (w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->width = -1;
  w->height = -1;

  //xitk_unmodal_window(w->window);

  if (destroy_window) {
    xitk_lock_display (w->xitk);
    XDestroyWindow(w->xitk->display, w->window);

    if (w->win_parent && xitk_window_is_window_visible(w->win_parent))
      XSetInputFocus(w->xitk->display, w->win_parent->window, RevertToParent, CurrentTime);
    xitk_unlock_display (w->xitk);
  }

  if (w->widget_list)
    XITK_WIDGET_LIST_FREE(w->widget_list);

  XITK_FREE(w);
}

void xitk_window_destroy_window(xitk_window_t *w) {

  _destroy_common(w, 1);
}

void xitk_x11_destroy_window_wrapper(xitk_window_t **p) {
  xitk_window_t *w = *p;

  if (!w)
    return;
  *p = NULL;

  _destroy_common(w, 0);
}

xitk_window_t *xitk_x11_wrap_window(xitk_t *xitk, Window window) {
  xitk_window_t *xwin;

  xwin = calloc(1, sizeof(*xwin));
  if (!xwin)
    return NULL;

  xwin->xitk   = xitk;
  xwin->window = window;

  xitk_get_window_position(xitk->display, window,
                           NULL, NULL, &xwin->width, &xwin->height);

  return xwin;
}
