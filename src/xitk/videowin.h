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
 * video window handling functions
 */

#ifndef VIDEOWIN_H
#define VIDEOWIN_H

void video_window_init (void);

void video_window_select_visual (void);

void video_window_adapt_size (void *this,
			      int video_width, int video_height, 
			      int *dest_x, int *dest_y,
			      int *dest_width, int *dest_height);

void video_window_calc_dest_size (void *this,
				  int video_width, int video_height,
				  int *dest_width, int *dest_height);

/* set/get fullscreen mode */
void video_window_set_fullscreen (int req_fullscreen);
int video_window_is_fullscreen (void);

/* hide/show cursor in video window*/
void video_window_set_cursor_visibility(int show_cursor);
/* Get cursor visiblity (boolean) */
int video_window_is_cursor_visible(void);

/* hide/show video window */
void video_window_set_visibility(int show_window);
int video_window_is_visible (void);

#endif
