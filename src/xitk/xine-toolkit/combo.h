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
#ifndef HAVE_XITK_COMBO_H
#define HAVE_XITK_COMBO_H

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
  const char                      **entries;
  int                              layer_above;
  xitk_state_callback_t            callback;
  void                            *userdata;
  xitk_register_key_t             *parent_wkey;
} xitk_combo_widget_t;

/** */
xitk_widget_t *xitk_combo_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_combo_widget_t *c,
  xitk_widget_t **lw, xitk_widget_t **bw);
/** */
xitk_widget_t *xitk_noskin_combo_create (xitk_widget_list_t *wl,
  xitk_combo_widget_t *c, int x, int y, int width, xitk_widget_t **lw, xitk_widget_t **bw);
/** */
int xitk_combo_get_current_selected (xitk_widget_t *w);
/** */
const char *xitk_combo_get_current_entry_selected (xitk_widget_t *w);
/** */
void xitk_combo_set_select (xitk_widget_t *w, int select);
/** */
void xitk_combo_update_list (xitk_widget_t *w, const char *const *const list, int len);
/** */
void xitk_combo_update_pos (xitk_widget_t *w);
/** */
void xitk_combo_rollunroll (xitk_widget_t *w);
/** */
void xitk_combo_callback_exec (xitk_widget_t *w);
/** */
xitk_widget_t *xitk_combo_get_label_widget (xitk_widget_t *w);

#endif
