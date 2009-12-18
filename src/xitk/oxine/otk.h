/*
 * Copyright (C) 2002 Stefan Holst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
 *
 * otk - the osd toolkit
 */

#ifndef HAVE_OTK_H
#define HAVE_OTK_H

#include "odk.h"
#include "list.h"
#include "oxine_event.h"

/*
 * alignments for text
 */

#define OTK_ALIGN_LEFT     0x001
#define OTK_ALIGN_CENTER   0x002
#define OTK_ALIGN_RIGHT    0x004
#define OTK_ALIGN_TOP      0x008
#define OTK_ALIGN_VCENTER  0x010
#define OTK_ALIGN_BOTTOM   0x020

/*
 * opaque data types
 */

typedef struct otk_s otk_t;
typedef struct otk_widget_s otk_widget_t;

/*
 * user contributed callbacks
 */

typedef void (*otk_button_cb_t) (void *user_data);
typedef void (*otk_list_cb_t) (void *user_data, void *entry);
typedef void (*otk_selector_cb_t) (void *user_data, int entry);
typedef int  (*otk_slider_cb_t) (odk_t *odk);
typedef void (*otk_button_uc_t) (void *data);

/*
 * initialisation
 */

otk_t *otk_init (odk_t *odk);

/*
 * creating widgets
 */

otk_widget_t *otk_window_new (otk_t *this, const char *title, int x, int y,
			      int w, int h);

otk_widget_t *otk_button_new (otk_widget_t *win, int x, int y,
			      int w, int h, char *text,
			      otk_button_cb_t cb,
			      void *user_data);

otk_widget_t *otk_button_grid_new (char *text,
			      otk_button_cb_t cb,
			      void *user_data);

otk_widget_t *otk_label_new (otk_widget_t *win, int x, int y, int alignment, 
                             char *text);

otk_widget_t *otk_list_new (otk_widget_t *win, int x, int y, int w, int h, 
			  otk_list_cb_t cb,
			  void *user_data);
otk_widget_t *otk_slider_new (otk_widget_t *win, int x, int y, int w, int h, otk_slider_cb_t cb);
otk_widget_t *otk_slider_grid_new (otk_slider_cb_t cb);

otk_widget_t *otk_selector_new(otk_widget_t *win, int x, int y, 
                           int w, int h, const char *const *items, int num,
			   otk_selector_cb_t cb,
			   void *cb_data);

otk_widget_t *otk_selector_grid_new (const char *const *items, int num,
			      otk_selector_cb_t cb,
			      void *cb_data);

otk_widget_t *otk_layout_new(otk_widget_t *win, int x, int y, int w, int h, int rows, int columns);

otk_widget_t *otk_scrollbar_new (otk_widget_t *win, int x, int y,
			        int w, int h,
			        otk_button_cb_t cb_up, otk_button_cb_t cb_down,
			        void *user_data);


/*
 * scrollbar widget functions
 */

void otk_scrollbar_set(otk_widget_t *this, int pos_start, int pos_end);

/*
 * selector widget functions
 */

void otk_selector_set(otk_widget_t *this, int pos);

/*
 * label widget functions
 */

void otk_label_set_font_size(otk_widget_t *this, int font_size);

/*
 * button widget functions
 */

void otk_button_uc_set(otk_widget_t *wid, otk_button_uc_t uc, void *data);

/*
 * layout widget functions
 */

/* layout widget use widget's x,y,w,h to set positions and sizes :
   x = start row : y = start column
   w = columns to be uses : h = rows to be used
*/

void otk_layout_add_widget(otk_widget_t *layout, otk_widget_t *widget, int srow, int scol, int frow, int fcol);

/*
 * list widget functions
 */

void otk_add_listentry(otk_widget_t *this, char *text, void *data, int pos);
void otk_remove_listentries(otk_widget_t *this);
int otk_list_get_pos(otk_widget_t *this);
void otk_list_set_pos(otk_widget_t *this, int newpos);

/* returns number of last selected entry (first entry in list is 1) */
int otk_list_get_entry_number(otk_widget_t *this);

/*
 * update drawing area
 */

void otk_draw_all (otk_t *otk);

/* on 1, update this widget continously (every 100 ms) */
void otk_set_update(otk_widget_t *this, int update);

/*
 * set focus to a specific widget (causes buttons lighting up)
 */

void otk_set_focus (otk_widget_t *widget);

/*
 * frees given widget and all its childs
 */

void otk_destroy_widget (otk_widget_t *widget);

/*
 * frees all widgets
 */

void otk_clear (otk_t *otk);

/*
 * frees otk struct
 */

void otk_free (otk_t *otk);

/*
 * event stuff
 */

void otk_set_event_handler(otk_t *otk, int (*cb)(void *data, oxine_event_t *ev), void *data);

int otk_send_event(otk_t *otk, oxine_event_t *event);

#endif

