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

#ifndef HAVE_XITK_SLIDER_H
#define HAVE_XITK_SLIDER_H

/*  vertical slider type */
#define XITK_VSLIDER 1
/*  horizontal slider type */
#define XITK_HSLIDER 2
/*  rotate button *slider* type */
#define XITK_RSLIDER 3
/*  hor and/or vert slider type */
#define XITK_HVSLIDER 4

typedef struct {
  int                    magic;
  int                    min;
  int                    max;
  int                    step;
  const char            *skin_element_name;
  xitk_state_callback_t  callback;
  void                  *userdata;
  xitk_state_callback_t  motion_callback;
  void                  *motion_userdata;
} xitk_slider_widget_t;

typedef struct {
  struct {
    int pos, step, visible, max;
  } h, v;
} xitk_slider_hv_t;

typedef enum {
  XITK_SLIDER_SYNC_GET = 0,
  XITK_SLIDER_SYNC_SET,
  XITK_SLIDER_SYNC_SET_AND_PAINT
} xitk_slider_sync_t;

void xitk_slider_hv_sync (xitk_widget_t *w, xitk_slider_hv_t *info, xitk_slider_sync_t mode);

/** Create a slider */
xitk_widget_t *xitk_slider_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig, const xitk_slider_widget_t *sl);
xitk_widget_t *xitk_noskin_slider_create (xitk_widget_list_t *wl, const xitk_slider_widget_t *s,
  int x, int y, int width, int height, int type);

#define XITK_SLIDER_MIN 0x7ffffff0
#define XITK_SLIDER_MAX 0x7ffffff1
#define XITK_SLIDER_PLUS 0x7ffffff2
#define XITK_SLIDER_MINUS 0x7ffffff3
/** Get current position of paddle. */
#define xitk_slider_get_pos(_w) xitk_widget_select (_w, XITK_INT_KEEP)
/** Set position of paddle. */
#define xitk_slider_set_pos(_w, _pos) xitk_widget_select (_w, _pos)
/** Set min, max, step value of slider. */
void xitk_slider_set_range (xitk_widget_t *w, int min, int max, int step);
/** Set position to 0 and redraw the widget. */
#define xitk_slider_reset(_w) xitk_widget_select (_w, 0)
/** Set position to max and redraw the widget. */
#define xitk_slider_set_to_max(_w) xitk_widget_select (_w, XITK_SLIDER_MAX)
/** Increment by step the paddle position */
#define xitk_slider_make_step(_w) xitk_widget_select (_w, XITK_SLIDER_PLUS)
/** Decrement by step the paddle position. */
#define xitk_slider_make_backstep(_w) xitk_widget_select (_w, XITK_SLIDER_MINUS)

#endif
