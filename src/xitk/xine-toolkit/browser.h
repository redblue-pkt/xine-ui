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

#ifndef HAVE_XITK_BROWSER_H
#define HAVE_XITK_BROWSER_H

#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

#define DEFAULT_DBL_CLICK_TIME 200

typedef struct {

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *bWidget;
  xitk_widget_t          *item_tree[BROWSER_MAX_ENTRIES];
  xitk_widget_list_t     *parent_wlist;

  xitk_skin_config_t     *skonfig;

  char                  **content;
  int                     max_length;
  int                     list_length;
  int                     current_start;

  xitk_simple_callback_t  callback;
  void                   *userdata;

  int                     dbl_click_time;
  xitk_state_callback_t   dbl_click_callback;

  int                     last_button_clicked;
  int                     current_button_clicked;
  struct timeval          click_time;

} browser_private_data_t;

/* ****************************************************************** */

/**
 * Create the list browser
 */
xitk_widget_t *xitk_browser_create(xitk_skin_config_t *skonfig, xitk_browser_widget_t *b);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_browser_create(xitk_browser_widget_t *br, GC gc, 
					  int x, int y, 
					  int itemw, int itemh, int slidw, char *fontname);
/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start);
/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, char **list, int len, int start);
/**
 * Return the current selected button (if not, return -1)
 */
int xitk_browser_get_current_selected(xitk_widget_t *w);
/**
 * Select the item 'select' in list
 */
void xitk_browser_set_select(xitk_widget_t *w, int select);
/**
 * Release all enabled buttons
 */
void xitk_browser_release_all_buttons(xitk_widget_t *w);
/**
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w);

void xitk_browser_step_up(xitk_widget_t *w, void *data);
void xitk_browser_step_down(xitk_widget_t *w, void *data);

/**
 * Change browser labels alignment
 */
void xitk_browser_set_alignment(xitk_widget_t *w, int align);

#endif
