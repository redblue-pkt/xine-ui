/* 
 * Copyright (C) 2000-2002 the xine project
 * 
 * This file is part of xine, a free video player.
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

typedef struct {
  int   x;
  int   y;
  int   width;
  int   height;
  int   borderless;
} window_attributes_t;

void video_window_init (window_attributes_t *window_attribute);

void video_window_exit (void);

void video_window_select_visual (void);

void video_window_frame_output_cb (void *this,
				   int video_width, int video_height, 
				   int *dest_x, int *dest_y,
				   int *dest_width, int *dest_height,
				   int *win_x, int *win_y);

void video_window_dest_size_cb (void *this,
				int video_width, int video_height,
				int *dest_width, int *dest_height);

/* set/check fullscreen mode */
void video_window_set_fullscreen_mode (int req_fullscreen);
int video_window_get_fullscreen_mode (void);

/* hide/show cursor in video window*/
void video_window_set_cursor_visibility(int show_cursor);
/* Get cursor visiblity (boolean) */
int video_window_is_cursor_visible(void);

/* hide/show video window */
void video_window_set_visibility(int show_window);
int video_window_is_visible (void);

void video_window_set_mag (float mag);
float video_window_get_mag (void);

void video_window_change_skins(void);

void video_window_reset_ssaver(void);

#endif
