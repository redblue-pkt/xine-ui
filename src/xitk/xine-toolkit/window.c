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

xitk_t *xitk_window_get_xitk (xitk_window_t *w) {
  return w ? w->xitk : NULL;
}

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

void xitk_window_set_window_icon (xitk_window_t *w, xitk_image_t *icon) {

  if (w == NULL)
    return;

  xitk_set_window_icon (w->xitk->display, w->window, icon->beimg->id1);
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

void xitk_window_set_wm_window_type (xitk_window_t *w, xitk_wm_window_type_t type) {
  if (w)
    xitk_set_wm_window_type (w->xitk, w->window, type);
}

void xitk_window_set_transient_for(xitk_window_t *w, Window win) {
  xitk_lock_display (w->xitk);
  XSetTransientForHint(w->xitk->display, w->window, win);
  xitk_unlock_display (w->xitk);
}

void xitk_window_set_transient_for_win(xitk_window_t *w, xitk_window_t *xwin) {
  xitk_window_set_transient_for(w, xwin->window);
}

void xitk_window_raise_window (xitk_window_t *xwin) {
  if (xwin && xwin->bewin)
    xwin->bewin->raise (xwin->bewin);
}

void xitk_window_show_window (xitk_window_t *xwin, int raise) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_STATE, XITK_WS_NORMAL},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
    if (raise)
      xwin->bewin->raise (xwin->bewin);
  }
}

void xitk_window_iconify_window (xitk_window_t *xwin) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_STATE, XITK_WS_ICONIFIED},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
  }
}

void xitk_window_hide_window (xitk_window_t *xwin) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_STATE, XITK_WS_HIDDEN},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
  }
}

void xitk_window_clear_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XClearWindow(w->xitk->display, w->window);
  xitk_unlock_display (w->xitk);
}

void xitk_window_reparent_window (xitk_window_t *xwin, xitk_window_t *parent, int x, int y) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_PARENT, (uintptr_t)parent},
      {XITK_TAG_X, x},
      {XITK_TAG_Y, y},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
  }
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
void xitk_window_get_window_position (xitk_window_t *xwin, int *x, int *y, int *w, int *h) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_X, 0},
      {XITK_TAG_Y, 0},
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->get_props (xwin->bewin, tags);
    if (x)
      *x = tags[0].value;
    if (y)
      *y = tags[1].value;
    if (w)
      *w = tags[2].value;
    if (h)
      *h = tags[3].value;
  }
}

/*
 * Center a window in root window.
 */
void xitk_window_move_window (xitk_window_t *xwin, int x, int y) {
  if (xwin && xwin->bewin) {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_X, x},
      {XITK_TAG_Y, y},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
  }
}

/*
 * Center a window in root window.
 */
void xitk_window_center_window (xitk_window_t *xwin) {
  Window rootwin;
  int x, y;
  unsigned int w, h, b, d;

  if (!xwin)
    return;
  if (!xwin->xitk || !xwin->bewin)
    return;

  xitk_lock_display (xwin->xitk);
  if (XGetGeometry (xwin->xitk->display, xwin->xitk->imlibdata->x.root, &rootwin,
    &x, &y, &w, &h, &b, &d) != BadDrawable) {
    int xx, yy;

    xitk_unlock_display (xwin->xitk);
    xx = (w / 2) - (xwin->width / 2);
    yy = (h / 2) - (xwin->height / 2);
    xitk_window_move_window (xwin, xx, yy);
    return;
  }
  xitk_unlock_display (xwin->xitk);
}

/*
 * Create a simple (empty) window.
 */
xitk_window_t *xitk_window_create_window_ext (xitk_t *xitk, int x, int y, int width, int height,
    const char *title, const char *res_name, const char *res_class,
    int override_redirect, int layer_above, xitk_image_t *icon, xitk_image_t *bg_image) {
  xitk_window_t         *xwin;

  if ((xitk == NULL) || (xitk->imlibdata == NULL))
    return NULL;
  if (!bg_image && ((width <= 0 || height <= 0)))
    return NULL;

  if (!title)
    title = "xiTK Window";

  xwin                  = (xitk_window_t *) xitk_xmalloc(sizeof(xitk_window_t));
  xwin->xitk            = xitk;
  xwin->bewin           = NULL;
  xwin->bg_image        = bg_image;
  xwin->win_parent      = NULL;

  {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_X, x},
      {XITK_TAG_Y, y},
      {XITK_TAG_IMAGE, xwin->bg_image ? (uintptr_t)xwin->bg_image->beimg : 0},
      {XITK_TAG_TITLE, (uintptr_t)title},
      {XITK_TAG_ICON, (uintptr_t)icon},
      {XITK_TAG_OVERRIDE_REDIRECT, override_redirect},
      {XITK_TAG_LAYER_ABOVE, layer_above},
      {XITK_TAG_RES_NAME, (uintptr_t)res_name},
      {XITK_TAG_RES_CLASS, (uintptr_t)res_class},
      {XITK_TAG_WIDTH, width},
      {XITK_TAG_HEIGHT, height},
      {XITK_TAG_END, 0}
    };
    xwin->bewin = xitk->d->window_new (xitk->d, tags);
    if (xwin->bewin) {
      xwin->bewin->get_props (xwin->bewin, tags + 9);
      xwin->width = tags[9].value;
      xwin->height = tags[10].value;
      xwin->bewin->data = xwin;
      xwin->window = xwin->bewin->id;
      xitk_image_ref (xwin->bg_image);
      return xwin;
    }
  }
  XITK_FREE (xwin);
  return NULL;
}

xitk_window_t *xitk_window_create_window(xitk_t *xitk, int x, int y, int width, int height) {
  return xitk_window_create_window_ext(xitk, x, y, width, height,
                                       NULL, NULL, NULL, 0, 0, NULL, NULL);
}

/*
 * Create a simple painted window.
 */
xitk_window_t *xitk_window_create_simple_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                                    const char *title, const char *res_name, const char *res_class,
                                                    int override_redirect, int layer_above, xitk_image_t *icon) {
  xitk_window_t *xwin;
  xitk_image_t *bg_image;

  if (!xitk || (width <= 0) || (height <= 0))
    return NULL;
  bg_image = xitk_image_new (xitk, NULL, 0, width, height);
  xitk_image_draw_outter (bg_image, width, height);
  xwin = xitk_window_create_window_ext (xitk, x, y, width, height, title,
    res_name, res_class, override_redirect, layer_above, icon, bg_image);
  if (!xwin) {
    xitk_image_free_image (&bg_image);
    return NULL;
  }
  return xwin;
}

xitk_window_t *xitk_window_create_simple_window(xitk_t *xitk, int x, int y, int width, int height) {
  return xitk_window_create_simple_window_ext(xitk, x, y, width, height, NULL, NULL, NULL, 0, 0, NULL);
}

xitk_widget_list_t *xitk_window_widget_list (xitk_window_t *xwin) {
  if (xwin->widget_list)
    return xwin->widget_list;

  xwin->widget_list = xitk_widget_list_get (xwin->xitk, xwin->window);
  if (!xwin->widget_list)
    return NULL;

  xwin->widget_list->xwin = xwin;
  return xwin->widget_list;
}

/*
 * Create a simple, with title bar, window.
 */
xitk_window_t *xitk_window_create_dialog_window(xitk_t *xitk, const char *title,
						int x, int y, int width, int height) {
  xitk_window_t *xwin;
  xitk_image_t *bar, *pix_bg;
  unsigned int   colorblack, colorwhite, colorgray, colordgray;
  xitk_font_t   *fs = NULL;
  int            lbear, rbear, wid, asc, des;
  int            bar_style = xitk_get_cfg_num (xitk, XITK_BAR_STYLE);

  if (title == NULL)
    return NULL;

  xwin = xitk_window_create_simple_window(xitk, x, y, width, height);
  if (!xwin)
    return NULL;

  xitk_window_set_window_title(xwin, title);

  bar = xitk_image_new (xitk, NULL, 0, width, TITLE_BAR_HEIGHT);
  pix_bg = xitk_image_new (xitk, NULL, 0, width, height);

  fs = xitk_font_load_font(xitk, DEFAULT_BOLD_FONT_12);
  xitk_image_set_font (bar, fs);
  xitk_font_string_extent(fs, (title && strlen(title)) ? title : "Window", &lbear, &rbear, &wid, &asc, &des);

  xitk_image_copy (xwin->bg_image, pix_bg);

  colorblack = xitk_get_cfg_num (xitk, XITK_BLACK_COLOR);
  colorwhite = xitk_get_cfg_num (xitk, XITK_WHITE_COLOR);
  colorgray = xitk_get_cfg_num (xitk, XITK_BG_COLOR);
  colordgray = xitk_get_cfg_num (xitk, XITK_SELECT_COLOR);

 /* Draw window title bar background */
  if(bar_style) {
    int s, bl = 255;
    unsigned int colorblue;

    colorblue = xitk_color_db_get (xitk, bl);
    xitk_lock_display (xitk);
    for(s = 0; s <= TITLE_BAR_HEIGHT; s++, bl -= 8) {
      xitk_image_draw_line (bar, 0, s, width, s, colorblue);
      colorblue = xitk_color_db_get (xitk, bl);
    }
    xitk_unlock_display (xitk);
  }
  else {
    int s;
    unsigned int c, cd;

    cd = xitk_color_db_get (xitk, (115 << 16) + (12 << 8) + 206);
    c = xitk_color_db_get (xitk, (135 << 16) + (97 << 8) + 168);

    xitk_image_fill_rectangle (bar, 0, 0, width, TITLE_BAR_HEIGHT, colorgray);
    xitk_image_draw_rectangular_box (bar, 2, 2, width - 5, TITLE_BAR_HEIGHT - 4, DRAW_INNER);

    xitk_lock_display (xitk);
    for(s = 6; s <= (TITLE_BAR_HEIGHT - 6); s += 3) {
      xitk_image_draw_line (bar, 5, s, (width - 8), s, c);
      xitk_image_draw_line (bar, 5, s+1, (width - 8), s + 1, cd);
    }

    xitk_image_fill_rectangle (bar, ((width - wid) - TITLE_BAR_HEIGHT) - 10, 6,
      wid + 20, TITLE_BAR_HEIGHT - 1 - 8, colorgray);
    xitk_unlock_display (xitk);
  }

  xitk_lock_display (xitk);
  xitk_image_draw_line (bar, 0, 0, width, 0, colorwhite);
  xitk_image_draw_line (bar, 0, 0, 0, TITLE_BAR_HEIGHT - 1, colorwhite);
  xitk_unlock_display (xitk);

  xitk_lock_display (xitk);

  xitk_image_draw_line (bar, width - 1, 0, width - 1, TITLE_BAR_HEIGHT - 1, colorblack);
  xitk_image_draw_line (bar, 2, TITLE_BAR_HEIGHT - 1, width - 2, TITLE_BAR_HEIGHT - 1, colorblack);

  xitk_image_draw_line (bar, width - 2, 2, width - 2, TITLE_BAR_HEIGHT - 1, colordgray);

  xitk_image_draw_line (pix_bg, width - 1, 0, width - 1, height - 1, colorblack);
  xitk_image_draw_line (pix_bg, 0, height - 1, width - 1, height - 1, colorblack);

  xitk_image_draw_line (pix_bg, width - 2, 0, width - 2, height - 2, colordgray);
  xitk_image_draw_line (pix_bg, 2, height - 2, width - 2, height - 2, colordgray);

  xitk_unlock_display (xitk);

  xitk_image_draw_string (bar, (width - wid) - TITLE_BAR_HEIGHT, ((TITLE_BAR_HEIGHT + asc + des) >> 1) - des,
    title, strlen (title), bar_style ? colorwhite : xitk_color_db_get (xitk, (85 << 16) + (12 << 8) + 135));

  xitk_image_set_font (bar, NULL);
  xitk_font_unload_font(fs);

  xitk_image_copy_rect (bar, pix_bg, 0, 0, width, TITLE_BAR_HEIGHT, 0, 0);

  xitk_window_set_background_image (xwin, pix_bg);

  xitk_image_free_image (&bar);
  xitk_image_free_image (&pix_bg);

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
  XSetWMNormalHints (w->xitk->display, w->window, &hint);
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
 * Apply (draw) window background.
 */
void xitk_window_apply_background (xitk_window_t *xwin) {
  if (!xwin)
    return;
  if (!xwin->bewin || !xwin->bg_image)
    return;
  {
    xitk_tagitem_t tags[2] = {
      {XITK_TAG_IMAGE, (uintptr_t)xwin->bg_image},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->set_props (xwin->bewin, tags);
  }
}

xitk_image_t *xitk_window_get_background_image (xitk_window_t *w) {
  return w ? w->bg_image : NULL;
}

int xitk_window_set_background_image (xitk_window_t *xwin, xitk_image_t *bg) {
  if (xwin && xwin->bewin) {
    xitk_image_t *old_bg = xwin->bg_image;
    xitk_tagitem_t tags[] = {
      {XITK_TAG_IMAGE, (uintptr_t)bg->beimg},
      {XITK_TAG_END, 0}
    };
    xwin->bg_image = bg;
    xitk_image_ref (bg);
    xitk_image_free_image (&old_bg);
    return xwin->bewin->set_props (xwin->bewin, tags);
  } else {
    return 0;
  }
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

void xitk_window_destroy_window (xitk_window_t *xwin) {
  if (!xwin)
    return;
  if (xwin->widget_list) {
    xwin->widget_list->xwin = NULL;
    XITK_WIDGET_LIST_FREE (xwin->widget_list);
    xwin->widget_list = NULL;
  }
  if (xwin->bewin)
    xwin->bewin->_delete (&xwin->bewin);
  xitk_image_free_image (&xwin->bg_image);
  XITK_FREE (xwin);
}

void xitk_x11_destroy_window_wrapper (xitk_window_t **p) {
  if (!p)
    return;
  xitk_window_destroy_window (*p);
  *p = NULL;
}

xitk_window_t *xitk_x11_wrap_window (xitk_t *xitk, Window window) {
  xitk_window_t *xwin;

  if (!xitk || (window == None))
    return NULL;
  if (!xitk->d)
    return NULL;

  xwin = xitk_xmalloc (sizeof (*xwin));
  if (!xwin)
    return NULL;

  {
    xitk_tagitem_t tags[2] = {
      {XITK_TAG_WRAP, window},
      {XITK_TAG_END, 0}
    };
    xwin->bewin = xitk->d->window_new (xitk->d, tags);
  }
  if (!xwin->bewin) {
    XITK_FREE (xwin);
    return NULL;
  }
  xwin->xitk = xitk;
  xwin->widget_list = NULL;
  xwin->bg_image = NULL;
  xwin->window = window;
  {
    xitk_tagitem_t tags[3] = {
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->get_props (xwin->bewin, tags);
    xwin->width = tags[0].value;
    xwin->height = tags[1].value;
  }
  return xwin;
}

