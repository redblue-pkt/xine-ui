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

#ifndef HAVE_XITK_MRLBROWSER_H
#define HAVE_XITK_MRLBROWSER_H

#include <xine.h>

#include "browser.h"

typedef void (*xitk_mrl_callback_t)(xitk_widget_t *, void *, xine_mrl_t *);

typedef struct {
  char                             *name;
  char                             *ending;
} xitk_mrlbrowser_filter_t;

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
  xitk_image_t                     *icon;
  void                            (*reparent_window) (void *rw_data, xitk_window_t *xwin);
  void                             *rw_data;
  int                               layer_above;

  int                               x;
  int                               y;
  char                             *window_title;
  char                             *resource_name;
  char                             *resource_class;

  struct {
    char                           *cur_origin;
    const char                     *skin_element_name;
  } origin;

  xitk_key_event_callback_t         key_cb;
  void                             *key_cb_data;
  xitk_dnd_callback_t               dndcallback;

  struct {
    char                           *caption;
    const char                     *skin_element_name;
    xitk_mrl_callback_t             callback;
    void                           *data;
  } select;

  struct {
    const char                     *skin_element_name;
    xitk_mrl_callback_t             callback;
    void                           *data;
  } play;

  struct {
    char                           *caption;
    const char                     *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t          callback;
    void                           *data;
  } kill;

  const char *const                *ip_availables;

  struct {

    struct {
      const char                   *skin_element_name;
    } button;

    struct {
      const char                   *label_str;
      const char                   *skin_element_name;
    } label;

  } ip_name;

  xine_t                           *xine;

  xitk_browser_widget_t             browser;

  xitk_mrlbrowser_filter_t        **mrl_filters;

  struct {
    const char                     *skin_element_name;
  } combo;

} xitk_mrlbrowser_widget_t;


/** */
xitk_widget_t *xitk_mrlbrowser_create (xitk_t *xitk, xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb);
/** */
void xitk_mrlbrowser_change_skins (xitk_widget_t *w, xitk_skin_config_t *skonfig);
/** */
int xitk_mrlbrowser_is_running (xitk_widget_t *w);
/** */
int xitk_mrlbrowser_is_visible (xitk_widget_t *w);
/** */
void xitk_mrlbrowser_hide (xitk_widget_t *w);
/** */
void xitk_mrlbrowser_show (xitk_widget_t *w);
/** */
int xitk_mrlbrowser_get_window_info (xitk_widget_t *w, window_info_t *inf);
/** */
xitk_window_t *xitk_mrlbrowser_get_window (xitk_widget_t *w);
/** */
void xitk_mrlbrowser_set_tips_timeout (xitk_widget_t *w, int enabled, unsigned long timeout);

#endif
