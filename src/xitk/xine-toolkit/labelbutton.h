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

#ifndef HAVE_XITK_LABELBUTTON_H
#define HAVE_XITK_LABELBUTTON_H

#define CLICK_BUTTON 1
#define RADIO_BUTTON 2
#define TAB_BUTTON   3

typedef struct {
  int                        magic;
  int                        button_type;
  int                        align;
  const char                *label;
  xitk_state_callback_t      callback;
  xitk_ext_state_callback_t  state_callback;
  void                      *userdata;
  const char                *skin_element_name;
} xitk_labelbutton_widget_t;

/** Create a labeled button. */
xitk_widget_t *xitk_labelbutton_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_labelbutton_widget_t *b);
/** */
xitk_widget_t *xitk_info_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, const xitk_skin_element_info_t *info);
/** */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b,
  int x, int y, int width, int height,
  const char *ncolor, const char *fcolor, const char *ccolor,
  const char *fname);
/** Change label of button 'widget'. */
int xitk_labelbutton_change_label (xitk_widget_t *w, const char *new_label);
int xitk_labelbutton_change_shortcut_label (xitk_widget_t *w, const char *string, int x_pos, const char *);
/** * Return label of button 'widget'. */
const char *xitk_labelbutton_get_label (xitk_widget_t *w);
const char *xitk_labelbutton_get_shortcut_label (xitk_widget_t *w);
/** * Get state of button 'widget'. */
int xitk_labelbutton_get_state (xitk_widget_t *w);
/** Set state of button 'widget'. */
void xitk_labelbutton_set_state (xitk_widget_t *, int selected);
/** Return used font name */
char *xitk_labelbutton_get_fontname (xitk_widget_t *w);
/** Set label button alignment */
void xitk_labelbutton_set_alignment (xitk_widget_t *w, int align);
/** Get label button alignment */
int xitk_labelbutton_get_alignment (xitk_widget_t *w);
/** */
void xitk_labelbutton_set_label_offset (xitk_widget_t *w, int dx);
int xitk_labelbutton_get_label_offset (xitk_widget_t *w);

#endif
