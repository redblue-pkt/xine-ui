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
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include "_xitk.h"
#include "xitk.h"
#include "default_font.h"

#include "xitk_x11.h"

#define TITLE_BAR_HEIGHT 20

static Window _xitk_window_get_window (xitk_window_t *w) {
  return w->window;
}

void xitk_window_set_input_focus (xitk_window_t *w) {
  xitk_lock_display (w->xitk);
  XSetInputFocus(xitk_x11_get_display(w->xitk), w->window, RevertToParent, CurrentTime);
  xitk_unlock_display (w->xitk);
}

void xitk_window_try_to_set_input_focus(xitk_window_t *w) {

  if (w == NULL)
    return;

  return xitk_x11_try_to_set_input_focus(xitk_x11_get_display(w->xitk), w->window);
}

void xitk_window_set_parent_window(xitk_window_t *xwin, xitk_window_t *parent) {

  if(xwin)
    xwin->win_parent = parent;
}

/*
 *
 */

void xitk_window_define_window_cursor (xitk_window_t *xwin, xitk_cursors_t cursor) {
  xitk_tagitem_t tags[] = {
    {XITK_TAG_CURSOR, (uintptr_t)cursor},
    {XITK_TAG_END, 0}
  };

  if (!xwin)
    return;

  xwin->bewin->set_props (xwin->bewin, tags);
}

void xitk_window_restore_window_cursor (xitk_window_t *xwin) {
  xitk_tagitem_t tags[] = {
    {XITK_TAG_CURSOR, ~(uintptr_t)0},
    {XITK_TAG_END, 0}
  };

  if (!xwin)
    return;

  xwin->bewin->set_props (xwin->bewin, tags);
}

/*
 * Is window is size match with given args
 */

void xitk_window_set_window_title (xitk_window_t *xwin, const char *title) {
  xitk_tagitem_t tags[] = {
    {XITK_TAG_TITLE, (uintptr_t)NULL},
    {XITK_TAG_END, 0}
  };
  const char *t;
 
  if (!xwin || !title)
    return;
  if (!xwin->bewin)
    return;
 
  xwin->bewin->get_props (xwin->bewin, tags);
  t = (const char *)tags[0].value;
  if (t && !strcmp (t, title))
    return;
 
  tags[0].value = (uintptr_t)title;
  xwin->bewin->set_props (xwin->bewin, tags);
}
 
/*
 *
 */

void xitk_window_set_window_icon (xitk_window_t *w, xitk_image_t *icon) {

  xitk_tagitem_t tags[] = {
    {XITK_TAG_ICON, (uintptr_t)icon},
    {XITK_TAG_END, 0}
  };

  if (w == NULL)
    return;

  w->bewin->set_props (w->bewin, tags);
}

void xitk_window_set_window_layer(xitk_window_t *w, int layer) {

  if (w == NULL)
    return;

  xitk_x11_set_window_layer(w->xitk, w->window, layer);
}

/*
 *
 */

void xitk_window_set_window_class(xitk_window_t *w, const char *res_name, const char *res_class) {
  xitk_tagitem_t tags[] = {
    {XITK_TAG_RES_NAME, (uintptr_t)res_name},
    {XITK_TAG_RES_CLASS, (uintptr_t)res_class},
    {XITK_TAG_END, 0}
  };

  if (w == NULL)
    return;

  w->bewin->set_props (w->bewin, tags);
}

void xitk_window_set_wm_window_type (xitk_window_t *xwin, xitk_wm_window_type_t type) {
  if (xwin) {
    xitk_set_wm_window_type (xwin->xitk, xwin->window, type);
    xwin->type = type;
  }
}

void xitk_window_set_transient_for_win(xitk_window_t *w, xitk_window_t *xwin) {
  xitk_lock_display (w->xitk);
  XSetTransientForHint(xitk_x11_get_display(w->xitk), w->window, xwin->window);
  xitk_unlock_display (w->xitk);
}

void xitk_window_raise_window (xitk_window_t *xwin) {
  if (xwin && xwin->bewin)
    xwin->bewin->raise (xwin->bewin);
}

void xitk_window_clear_window(xitk_window_t *w)
{
  xitk_lock_display (w->xitk);
  XClearWindow(xitk_x11_get_display(w->xitk), w->window);
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
  Display *display;
  int x, y;
  unsigned int w, h, b, d;

  if (!xwin)
    return;
  if (!xwin->xitk || !xwin->bewin)
    return;

  xitk_lock_display (xwin->xitk);
  display = xitk_x11_get_display(xwin->xitk);
  if (XGetGeometry (display, DefaultRootWindow(display), &rootwin,
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

  if (xitk == NULL)
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
  /* will be set by xitk_window_update_tree (). */
  xwin->type            = WINDOW_TYPE_END;
  xwin->role            = XITK_WR_HELPER;

  {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_X, x},
      {XITK_TAG_Y, y},
      {XITK_TAG_IMAGE, xwin->bg_image ? (uintptr_t)xwin->bg_image->beimg : 0},
      {XITK_TAG_TITLE, (uintptr_t)title},
      {XITK_TAG_ICON, (uintptr_t)icon},
      {XITK_TAG_WIN_FLAGS, (XITK_WINF_OVERRIDE_REDIRECT << 16) | (override_redirect ? XITK_WINF_OVERRIDE_REDIRECT : 0)},
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
      xwin->flags = tags[11].value;
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

  xwin->widget_list = xitk_widget_list_get (xwin->xitk, xwin);

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
    for(s = 0; s <= TITLE_BAR_HEIGHT; s++, bl -= 8) {
      xitk_image_draw_line (bar, 0, s, width, s, colorblue);
      colorblue = xitk_color_db_get (xitk, bl);
    }
  }
  else {
    int s;
    unsigned int c, cd;

    cd = xitk_color_db_get (xitk, (115 << 16) + (12 << 8) + 206);
    c = xitk_color_db_get (xitk, (135 << 16) + (97 << 8) + 168);

    xitk_image_fill_rectangle (bar, 0, 0, width, TITLE_BAR_HEIGHT, colorgray);
    xitk_image_draw_rectangular_box (bar, 2, 2, width - 5, TITLE_BAR_HEIGHT - 4, DRAW_INNER);

    for(s = 6; s <= (TITLE_BAR_HEIGHT - 6); s += 3) {
      xitk_image_draw_line (bar, 5, s, (width - 8), s, c);
      xitk_image_draw_line (bar, 5, s+1, (width - 8), s + 1, cd);
    }

    xitk_image_fill_rectangle (bar, ((width - wid) - TITLE_BAR_HEIGHT) - 10, 6,
      wid + 20, TITLE_BAR_HEIGHT - 1 - 8, colorgray);
  }

  xitk_image_draw_line (bar, 0, 0, width, 0, colorwhite);
  xitk_image_draw_line (bar, 0, 0, 0, TITLE_BAR_HEIGHT - 1, colorwhite);


  xitk_image_draw_line (bar, width - 1, 0, width - 1, TITLE_BAR_HEIGHT - 1, colorblack);
  xitk_image_draw_line (bar, 2, TITLE_BAR_HEIGHT - 1, width - 2, TITLE_BAR_HEIGHT - 1, colorblack);

  xitk_image_draw_line (bar, width - 2, 2, width - 2, TITLE_BAR_HEIGHT - 1, colordgray);

  xitk_image_draw_line (pix_bg, width - 1, 0, width - 1, height - 1, colorblack);
  xitk_image_draw_line (pix_bg, 0, height - 1, width - 1, height - 1, colorblack);

  xitk_image_draw_line (pix_bg, width - 2, 0, width - 2, height - 2, colordgray);
  xitk_image_draw_line (pix_bg, 2, height - 2, width - 2, height - 2, colordgray);


  xitk_image_draw_string (bar, fs, (width - wid) - TITLE_BAR_HEIGHT, ((TITLE_BAR_HEIGHT + asc + des) >> 1) - des,
    title, strlen (title), bar_style ? colorwhite : xitk_color_db_get (xitk, (85 << 16) + (12 << 8) + 135));

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
  xwin->bewin->data = xwin;
  xwin->bg_image = NULL;
  xwin->win_parent = NULL;

  /* will be set by xitk_window_update_tree (). */
  xwin->type = WINDOW_TYPE_END;
  xwin->role = XITK_WR_HELPER;

  xwin->window = window;
  xwin->widget_list = xitk_widget_list_get (xwin->xitk, xwin);

  {
    xitk_tagitem_t tags[] = {
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
      {XITK_TAG_WIN_FLAGS, 0},
      {XITK_TAG_END, 0}
    };
    xwin->bewin->get_props (xwin->bewin, tags);
    xwin->width = tags[0].value;
    xwin->height = tags[1].value;
    xwin->flags = tags[2].value;
  }
  return xwin;
}

uint32_t xitk_window_flags (xitk_window_t *xwin, uint32_t mask, uint32_t value) {
  xitk_tagitem_t tags[] = {
    {XITK_TAG_WIN_FLAGS, 0},
    {XITK_TAG_END, 0}
  };

  if (!xwin)
    return 0;
  if (!xwin->bewin)
    return 0;

  tags[0].value = (mask << 16) | (value & 0xffff);
  if ((mask & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) && (xwin->role != XITK_WR_SUBMENU)) {
    xitk_window_update_tree (xwin, tags[0].value);
  } else {
    xwin->bewin->set_props (xwin->bewin, tags);
  }
  xwin->bewin->get_props (xwin->bewin, tags);
  xwin->flags = tags[0].value;


  return tags[0].value & 0xffff;
}

void xitk_window_set_role (xitk_window_t *xwin, xitk_window_role_t role) {
  if (!xwin)
    return;
  if (xwin->role == role)
    return;
  xwin->role = role;
  xitk_window_update_tree (xwin, 0);
}

