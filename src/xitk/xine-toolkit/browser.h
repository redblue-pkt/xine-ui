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

#ifndef HAVE_XITK_BROWSER_H
#define HAVE_XITK_BROWSER_H

typedef struct {
  int                               magic;

  struct {
    const char                     *skin_element_name;
  } arrow_up;
  
  struct {
    const char                     *skin_element_name;
  } slider;

  struct {
    const char                     *skin_element_name;
  } arrow_dn;

  struct {
    const char                     *skin_element_name;
  } arrow_left;
  
  struct {
    const char                     *skin_element_name;
  } slider_h;

  struct {
    const char                     *skin_element_name;
  } arrow_right;

  struct {
    const char                     *skin_element_name;
    int                             max_displayed_entries;
    int                             num_entries;
    const char *const              *entries;
  } browser;
  
  xitk_ext_state_callback_t         dbl_click_callback;

  xitk_ext_state_callback_t         callback;
  void                             *userdata;

} xitk_browser_widget_t;


/** Create the list browser */
xitk_widget_t *xitk_browser_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_browser_widget_t *b);
xitk_widget_t *xitk_noskin_browser_create (xitk_widget_list_t *wl,
  xitk_browser_widget_t *br,
  int x, int y, int itemw, int itemh, int slidw, const char *font_name);
/** Redraw buttons/slider */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start);
/** Update the list, and rebuild button list */
void xitk_browser_update_list (xitk_widget_t *w, const char *const *list, const char *const *shortcut, int len, int start);
/** Return the current selected button (if not, return -1) */
int xitk_browser_get_current_selected (xitk_widget_t *w);
/** Select the item 'select' in list */
void xitk_browser_set_select (xitk_widget_t *w, int select);
/** Release all enabled buttons */
void xitk_browser_release_all_buttons (xitk_widget_t *w);
/** Return the number of displayed entries */
int xitk_browser_get_num_entries (xitk_widget_t *w);
/** Return the real number of first displayed in list */
int xitk_browser_get_current_start (xitk_widget_t *w);
/** Change browser labels alignment */
void xitk_browser_set_alignment (xitk_widget_t *w, int align);

#endif
