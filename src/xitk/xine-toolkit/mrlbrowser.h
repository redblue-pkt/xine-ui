/* 
 * Copyright (C) 2000-2002 the xine project
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

#ifndef HAVE_XITK_MRLBROWSER_H
#define HAVE_XITK_MRLBROWSER_H

#ifdef NEED_MRLBROWSER

#include <limits.h>
#include <X11/Xlib.h>
#include <xine.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "dnd.h"
#include "browser.h"

#define MAXFILES      65535

typedef struct {
  mrl_t                    *mrls[MAXFILES];
  char                     *mrls_disp[MAXFILES];
} mrl_contents_t;

typedef struct {

  xitk_widget_t            *fbWidget; /*  My widget */

  ImlibData                *imlibdata;
  char                     *skin_element_name;
  char                     *skin_element_name_ip;

  xitk_register_key_t       widget_key;

  Window                    window; /* file browser window */
  
  ImlibImage               *bg_image;
  xitk_widget_list_t       *widget_list; /* File browser widget list */
  
  xine_t                   *xine;

  mrl_contents_t           *mc;
  int                       mrls_num;

  char                     *last_mrl_source;

  xitk_widget_t            *widget_origin; /* Current directory widget */
  char                      current_origin[XITK_PATH_MAX + 1]; /* Current directory */

  int                       running; /* Boolean status */
  int                       visible; /* Boolean status */

  xitk_widget_t            *mrlb_list; /*  Browser list widget */
  xitk_widget_t            *autodir_plugins[64];

  xitk_mrl_callback_t       add_callback;
  xitk_mrl_callback_t       play_callback;
  xitk_simple_callback_t    kill_callback;
  xitk_simple_callback_t    ip_callback;

} mrlbrowser_private_data_t;

xitk_widget_t *xitk_mrlbrowser_create(xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb);
int xitk_mrlbrowser_is_running(xitk_widget_t *w);
int xitk_mrlbrowser_is_visible(xitk_widget_t *w);
void xitk_mrlbrowser_hide(xitk_widget_t *w);
void xitk_mrlbrowser_show(xitk_widget_t *w);
void xitk_mrlbrowser_set_transient(xitk_widget_t *w, Window window);
void xitk_mrlbrowser_destroy(xitk_widget_t *w);
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);
void xitk_mrlbrowser_set_tips_timeout(xitk_widget_t *w, int enabled, unsigned long timeout);

#endif

#endif
