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
  mrl_t                    *mrls[MAXFILES];
  char                     *mrls_disp[MAXFILES];
} mrl_contents_t;

typedef struct {

  widget_t                 *fbWidget; /*  My widget */

  Display                  *display; /* Current display */

  widgetkey_t               widget_key;

  Window                    window; /* file browser window */
  
  ImlibImage               *bg_image;
  widget_list_t            *widget_list; /* File browser widget list */
  
  xine_t                   *xine;

  mrl_contents_t           *mc;
  int                       mrls_num;

  char                     *last_mrl_source;

  widget_t                 *widget_origin; /* Current directory widget */
  char                      current_origin[PATH_MAX + 1]; /* Current directory */

  int                       running; /* Boolean status */
  int                       visible; /* Boolean status */

  widget_t                 *mrlb_list; /*  Browser list widget */

  xitk_mrl_callback_t      add_callback;
  xitk_mrl_callback_t      play_callback;
  xitk_simple_callback_t   kill_callback;
  xitk_simple_callback_t   ip_callback;

} mrlbrowser_private_data_t;

widget_t *mrlbrowser_create(xitk_mrlbrowser_t *mb);
int mrlbrowser_is_running(widget_t *w);
int mrlbrowser_is_visible(widget_t *w);
void mrlbrowser_hide(widget_t *w);
void mrlbrowser_show(widget_t *w);
void mrlbrowser_set_transient(widget_t *w, Window window);
void mrlbrowser_destroy(widget_t *w);
int mrlbrowser_get_window_info(widget_t *w, window_info_t *inf);

#endif

#endif
