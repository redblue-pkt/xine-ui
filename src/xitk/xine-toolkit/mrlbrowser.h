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

#ifndef HAVE_MRLBROWSER_H
#define HAVE_MRLBROWSER_H

#ifdef NEED_MRLBROWSER

#include <limits.h>
#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "dnd.h"
#include "browser.h"

#include "xine.h"

#define MAXFILES      65535

#ifndef NAME_MAX
#define NAME_MAX 256
#endif
#ifndef PATH_MAX
#define PATH_MAX 768
#endif

typedef struct {
  mrl_t                  *mrls[MAXFILES];
  char                   *mrls_disp[MAXFILES];
} mrl_contents_t;

typedef struct {
  
  int                     x;
  int                     y;
  char                   *window_title;
  char                   *bg_skinfile;
  char                   *resource_name;
  char                   *resource_class;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
    int                   max_length;
    char                 *cur_origin;
  } origin;
  
  dnd_callback_t          dndcallback;

  struct {
    int                   x;
    int                   y;
    char                 *caption;
    char                 *skin_filename;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
    void                (*callback) (widget_t *widget, void *data, mrl_t *);
  } select;

  struct {
    int                   x;
    int                   y;
    char                 *caption;
    char                 *skin_filename;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
  } dismiss;

  struct {
    void                (*callback) (widget_t *widget, void *data);
  } kill;

  char                  **ip_availables;
  
  struct {

    struct {
      int                 x;
      int                 y;
      char               *skin_filename;
      char               *normal_color;
      char               *focused_color;
      char               *clicked_color;
    } button;

    struct {
      int                 x;
      int                 y;
      char               *skin_filename;
      char               *label_str;
    } label;

  } ip_name;
  
  xine_t                 *xine;

  browser_placements_t   *br_placement;

} mrlbrowser_placements_t;

typedef struct {

  widget_t               *fbWidget; /*  My widget */

  Display                *display; /* Current display */

  widgetkey_t             widget_key;

  Window                  window; /* file browser window */
  
  ImlibImage             *bg_image;
  widget_list_t          *widget_list; /* File browser widget list */
  
  xine_t                 *xine;

  mrl_contents_t         *mc;
  int                     mrls_num;

  char                   *last_mrl_source;

  widget_t               *widget_origin; /* Current directory widget */
  char                    current_origin[PATH_MAX + 1]; /* Current directory */

  int                     running; /* Boolean status */
  int                     visible; /* Boolean status */

  widget_t               *mrlb_list; /*  Browser list widget */

  void                  (*add_callback) (widget_t *widget, void *data, mrl_t *mrl);
  void                  (*kill_callback) (widget_t *widget, void *data);

  void                  (*ip_callback) (widget_t *widget, void *data);

} mrlbrowser_private_data_t;

widget_t *mrlbrowser_create(Display *display, ImlibData *idata,
			    Window window_trans,
			    mrlbrowser_placements_t *fbp);

int mrlbrowser_is_running(widget_t *w);
int mrlbrowser_is_visible(widget_t *w);
void mrlbrowser_hide(widget_t *w);
void mrlbrowser_show(widget_t *w);
void mrlbrowser_set_transient(widget_t *w, Window window);
void mrlbrowser_destroy(widget_t *w);

#endif

#endif
