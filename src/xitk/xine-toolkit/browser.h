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

#ifndef HAVE_GUI_BROWSER_H
#define HAVE_GUI_BROWSER_H

#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"

typedef struct {
  widget_t    *itemlist;
  int          sel;
} btnlist_t;

typedef struct {

  Display       *display;

  widget_t      *bWidget;

  widget_t      *item_tree[BROWSER_MAX_ENTRIES];
  Window         win;
  GC             gc;

  char         **content;
  int            max_length;
  int            list_length;
  int            current_start;

  void           (*function) (widget_t *, void *);
  void          *user_data;

} browser_private_data_t;

/* ****************************************************************** */

/**
 * Create the list browser
 */
widget_t *create_browser(Display *display, ImlibData *idata,
			 /* The receiver list */
			 widget_list_t *thelist, 
			 /* X, Y, skin for slider up button */
			 int upX, int upY, char *upSK,
			 /* X, Y, backgrnd, foregrnd skin for slider */
			 int slX, int slY, char *slSKB, char *slSKF,
			 /* X, Y, skin for slider up button */
			 int dnX, int dnY, char *dnSK,
			 /* X, Y for item buttons */
			 int btnX, int btnY, 
			 /* normal/focus/click colors, skin for item buttons */
			 char *btnN, char *btnF, char *btnC, char *btnSK,
			 /* max item displayed, length of content, content */
			 int maxlength, int listlength, char **content,
			 /* callback and data to pass to callback */
			 void *selCB, void *data);

/**
 * Redraw buttons/slider
 */
void browser_rebuild_browser(widget_t *w, int start);
/**
 * Update the list, and rebuild button list
 */
void browser_update_list(widget_t *w, char **list, int len, int start);
/**
 * Return the current selected button (if not, return -1)
 */
int browser_get_current_selected(widget_t *w);
/**
 * Select the item 'select' in list
 */
void browser_set_select(widget_t *w, int select);
/**
 * Release all enabled buttons
 */
void browser_release_all_buttons(widget_t *w);
/**
 * Return the real number of first displayed in list
 */
int browser_get_current_start(widget_t *w);

#endif
