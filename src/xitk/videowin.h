/* 
 * Copyright (C) 2000-2003 the xine project
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

#define WINDOWED_MODE              0x00000001
#define FULLSCR_MODE               0x00000002
#ifdef HAVE_XINERAMA
#define FULLSCR_XI_MODE            0x00000004
#endif

#define CURSOR_ARROW               1
#define CURSOR_HAND                2

void video_window_init (window_attributes_t *window_attribute, int hide_on_start);

void video_window_exit (void);

void video_window_select_visual (void);

void video_window_frame_output_cb (void *this,
				   int video_width, int video_height,
				   double video_pixel_aspect,
				   int *dest_x, int *dest_y, 
				   int *dest_width, int *dest_height,
				   double *dest_pixel_aspect,
				   int *win_x, int *win_y);

void video_window_dest_size_cb (void *this,
				int video_width, int video_height,
				double video_pixel_aspect,
				int *dest_width, int *dest_height,
				double *dest_pixel_aspect);

/* set/check fullscreen mode */
void video_window_set_fullscreen_mode (int req_fullscreen);
int video_window_get_fullscreen_mode (void);

/* Set cursor */
void video_window_set_cursor(int cursor);
/* hide/show cursor in video window*/
void video_window_set_cursor_visibility(int show_cursor);
/* Get cursor visiblity (boolean) */
int video_window_is_cursor_visible(void);
int video_window_get_cursor_timer(void);
void video_window_set_cursor_timer(int timer);

/* hide/show video window */
void video_window_set_visibility(int show_window);
int video_window_is_visible (void);

void video_window_set_mag (float mag);
float video_window_get_mag (void);

void video_window_update_logo(void);
void video_window_change_skins(void);

void video_window_reset_ssaver(void);

void video_window_get_frame_size(int *w, int *h);

void video_window_get_visible_size(int *w, int *h);
#endif
