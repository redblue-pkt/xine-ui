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
#ifndef HAVE_XITK_WINDOW_H
#define HAVE_XITK_WINDOW_H

#include "_xitk.h"

#define XITK_WINDOW_ANSWER_UNKNOWN 0
#define XITK_WINDOW_ANSWER_OK      1
#define XITK_WINDOW_ANSWER_YES     2
#define XITK_WINDOW_ANSWER_NO      3
#define XITK_WINDOW_ANSWER_CANCEL  4

/*
 * Is window visible.
 */
int xitk_is_window_visible(Display *display, Window window);

/*
 * Is window is size match with given args
 */
int xitk_is_window_size(Display *display, Window window, int width, int height);

/*
 * Set/Change window title.
 */
void xitk_set_window_title(Display *display, Window window, char *title);
void xitk_window_set_window_title(ImlibData *im, xitk_window_t *w, char *title);

/*
 * Get (safely) window pos.
 */
void xitk_get_window_position(Display *display, Window window, 
			      int *x, int *y, int *width, int *height);
void xitk_window_get_window_position(ImlibData *im, xitk_window_t *w, 
				     int *x, int *y, int *width, int *height);
/*
 * Move window to xy coords.
 */
void xitk_window_move_window(ImlibData *im, xitk_window_t *w, int x, int y);

/*
 * Center a window in root window.
 */
void xitk_window_center_window(ImlibData *im, xitk_window_t *w);

/*
 * Create an empty window.
 */
xitk_window_t *xitk_window_create_window(ImlibData *im, int x, int y, int width, int height);

/*
 * Create a simple and painted window.
 */
xitk_window_t *xitk_window_create_simple_window(ImlibData *im, int x, int y, int width, int height);

/*
 * Create a simple window dialog.
 */
xitk_window_t *xitk_window_create_dialog_window(ImlibData *im, char *title, int x, int y, int width, int height);

/*
 * Destroy a window.
 */
void xitk_window_destroy_window(ImlibData *im, xitk_window_t *w);

/*
 * Return window (X) id.
 */
Window xitk_window_get_window(xitk_window_t *w);

/*
 * Return background pixmap of a window.
 */
Pixmap xitk_window_get_background(xitk_window_t *w);

/*
 * Redraw window background.
 */
void xitk_window_apply_background(ImlibData *im, xitk_window_t *w);

/*
 * Change window background with 'bg' (store and redraw).
 */
int xitk_window_change_background(ImlibData *im, xitk_window_t *w, Pixmap bg, int width, int height);

/*
 * Return window width and height of a given window object.
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height);

/*
 *
 */
xitk_window_t *xitk_window_dialog_one_button_with_width(ImlibData *im, char *title, char *button_label,
							xitk_state_callback_t cb, void *userdata, 
							int window_width, int align, char *message, ...);
/*
 * Display an OK window (sized), containing the message.
 */
xitk_window_t *xitk_window_dialog_ok_with_width(ImlibData *im, char *title,
						xitk_state_callback_t cb, void *userdata, 
						int window_width, int align, char *message, ...);

/*
 *
 */
xitk_window_t *xitk_window_dialog_two_buttons_with_width(ImlibData *im, char *title,
							 char *button1_label, char *button2_label,
							 xitk_state_callback_t cb1, 
							 xitk_state_callback_t cb2, 
							 void *userdata, 
							 int window_width, int align, char *message, ...);
/*
 * Display a question window with 'yes' and 'no' buttons.
 */
xitk_window_t *xitk_window_dialog_yesno_with_width(ImlibData *im, char *title,
						   xitk_state_callback_t ycb, 
						   xitk_state_callback_t ncb, 
						   void *userdata, 
						   int window_width, int align, char *message, ...);

/*
 *
 */
xitk_window_t *xitk_window_dialog_three_buttons_with_width(ImlibData *im, char *title,
							   char *button1_label,
							   char *button2_label,
							   char *button3_label,
							   xitk_state_callback_t cb1, 
							   xitk_state_callback_t cb2, 
							   xitk_state_callback_t cb3, 
							   void *userdata, 
							   int window_width, int align, char *message, ...);
/*
 * Display a question window with 'yes', 'no' and 'cancel' buttons.
 */
xitk_window_t *xitk_window_dialog_yesnocancel_with_width(ImlibData *im, char *title,
							 xitk_state_callback_t ycb, 
							 xitk_state_callback_t ncb, 
							 xitk_state_callback_t ccb, 
							 void *userdata, 
							 int window_width, int align, char *message, ...);

#endif
