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
#ifndef HAVE_XITK_WINDOW_H
#define HAVE_XITK_WINDOW_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "_xitk.h"

#define XITK_WINDOW_ANSWER_UNKNOWN 0
#define XITK_WINDOW_ANSWER_OK      1
#define XITK_WINDOW_ANSWER_YES     2
#define XITK_WINDOW_ANSWER_NO      3
#define XITK_WINDOW_ANSWER_CANCEL  4

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
 * Display an error window (sized), containing the message.
 */
void xitk_window_dialog_error_with_width(ImlibData *im, char *title,
					 xitk_state_callback_t cb, void *userdata, 
					 int window_width, char *message);

/*
 * Same as above, with static width.
 */
void xitk_window_dialog_error(ImlibData *im, char *title,
			      xitk_state_callback_t cb, void *userdata, char *message);

/*
 * Display a question window with 'yes' and 'no' buttons.
 */
void xitk_window_dialog_yesno_with_width(ImlibData *im, char *title,
					 xitk_state_callback_t ycb, 
					 xitk_state_callback_t ncb, 
					 void *userdata, int window_width, char *message);

void xitk_window_dialog_yesno(ImlibData *im, char *title,
			      xitk_state_callback_t ycb, 
			      xitk_state_callback_t ncb, 
			      void *userdata, char *message);

/*
 * Display a question window with 'yes', 'no' and 'cancel' buttons.
 */
void xitk_window_dialog_yesnocancel_with_width(ImlibData *im, char *title,
					       xitk_state_callback_t ycb, 
					       xitk_state_callback_t ncb, 
					       xitk_state_callback_t ccb, 
					       void *userdata, int window_width, char *message);

void xitk_window_dialog_yesnocancel(ImlibData *im, char *title,
				    xitk_state_callback_t ycb, 
				    xitk_state_callback_t ncb, 
				    xitk_state_callback_t ccb, 
				    void *userdata, char *message);

#endif
