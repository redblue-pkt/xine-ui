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
#ifndef HAVE_XITK_COMBO_H
#define HAVE_XITK_COMBO_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

typedef struct {

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_window_t          *xwin;
  GC                      gc;

  Window                  parent_win;
  GC                      parent_gc;
  xitk_register_key_t    *parent_wkey;

  xitk_widget_t          *combo_widget;
  xitk_widget_t          *label_widget;
  xitk_widget_t          *button_widget;
  xitk_widget_t          *browser_widget;

  char                  **entries;
  int                     num_entries;
  int                     selected;

  xitk_register_key_t     widget_key;
  xitk_widget_list_t     *widget_list;
  xitk_widget_list_t     *parent_wlist;

  xitk_state_callback_t   callback;
  void                   *userdata;
  int                     visible;

} combo_private_data_t;

/*
 * Create a combo box.
 */
xitk_widget_t *xitk_create_combo(xitk_skin_config_t *skonfig, xitk_slider_widget_t *s,
				 xitk_widget_t **lw, xitk_widget_t **bw);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_combo_create(xitk_combo_widget_t *c, int x, int y, int width,
					xitk_widget_t **lw, xitk_widget_t **bw);

/*
 * Return current selected entry (offset in entries).
 */
int xitk_combo_get_current_selected(xitk_widget_t *w);

/*
 * Return current selected entry (text).
 */
char *xitk_combo_get_current_entry_selected(xitk_widget_t *w);

/*
 * Choose a new selected entry in list (also update the label).
 */
void xitk_combo_set_select(xitk_widget_list_t *wl, xitk_widget_t *w, int select);

/*
 * Update combo list/num entries.
 */
void xitk_combo_update_list(xitk_widget_t *w, char **list, int len);

/*
 * Update combo window pos to parent label pos
 */
void xitk_combo_update_pos(xitk_widget_t *w);

#endif
