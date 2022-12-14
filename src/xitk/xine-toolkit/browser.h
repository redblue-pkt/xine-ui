/*
 * Copyright (C) 2000-2021 the xine project
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
  /** If the "selected" argument is >= 0: user has selected new item (selected).
   *  Otherwise: user has explicitly _un_selected old item (1 - selected). */
  xitk_ext_state_callback_t         callback;
  void                             *userdata;

} xitk_browser_widget_t;


/** Create the list browser */
xitk_widget_t *xitk_browser_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_browser_widget_t *b);
/** Create the list browser with generic skin.
 *  If slider width is < 0, sliders and move buttons appear if needed only
 *  within (itemw) x (itemh * br->browser.max_displayed_entries).
 *  If slider width is 0, movement will be keyboard and mouse wheel only. */
xitk_widget_t *xitk_noskin_browser_create (xitk_widget_list_t *wl,
  const xitk_browser_widget_t *br,
  int x, int y, int itemw, int itemh, int slidw, const char *font_name);
/** Update the list, and rebuild button list */
void xitk_browser_update_list (xitk_widget_t *w, const char *const *list, const char *const *shortcut, int len, int start);
/** Return the current selected button (if not, return -1) */
#define xitk_browser_get_current_selected(_w) xitk_widget_select (_w, XITK_INT_KEEP)
/** Select the item 'select' in list */
#define xitk_browser_set_select(_w,index) xitk_widget_select (_w, index)
/** Release all enabled buttons */
#define xitk_browser_release_all_buttons(_w) xitk_browser_set_select (_w, -1)
/** Return the number of displayed entries */
int xitk_browser_get_num_entries (xitk_widget_t *w);
/** Return the real number of first displayed in list */
int xitk_browser_get_current_start (xitk_widget_t *w);
/** Change browser labels alignment */
void xitk_browser_set_alignment (xitk_widget_t *w, int align);

#endif
