/* 
 * Copyright (C) 2000-2019 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * video window handling functions
 */

#ifndef VIDEOWIN_H
#define VIDEOWIN_H

typedef struct xui_vwin_st xui_vwin_t;

typedef struct {
  int   x;
  int   y;
  int   width;
  int   height;
  int   borderless;
} window_attributes_t;

#define WINDOWED_MODE              0x00000001
#define FULLSCR_MODE               0x00000002
#define FULLSCR_XI_MODE            0x00000004

#define CURSOR_ARROW               1
#define CURSOR_HAND                2

xui_vwin_t *video_window_init (gGui_t *gui, window_attributes_t *window_attribute, int hide_on_start);
void video_window_exit (xui_vwin_t *vwin);

void video_window_select_visual (xui_vwin_t *vwin);

void video_window_frame_output_cb (void *vwin,
				   int video_width, int video_height,
				   double video_pixel_aspect,
				   int *dest_x, int *dest_y, 
				   int *dest_width, int *dest_height,
				   double *dest_pixel_aspect,
				   int *win_x, int *win_y);

void video_window_dest_size_cb (void *vwin,
				int video_width, int video_height,
				double video_pixel_aspect,
				int *dest_width, int *dest_height,
				double *dest_pixel_aspect);

/* set/check fullscreen mode */
void video_window_set_fullscreen_mode (xui_vwin_t *vwin, int req_fullscreen);
int video_window_get_fullscreen_mode (xui_vwin_t *vwin);

/* Set cursor */
void video_window_set_cursor (xui_vwin_t *vwin, int cursor);
/* hide/show cursor in video window*/
void video_window_set_cursor_visibility (xui_vwin_t *vwin, int show_cursor);
/* Get cursor visiblity (boolean) */
int video_window_is_cursor_visible (xui_vwin_t *vwin);
int video_window_get_cursor_timer (xui_vwin_t *vwin);
void video_window_set_cursor_timer (xui_vwin_t *vwin, int timer);

/* hide/show video window */
void video_window_set_visibility (xui_vwin_t *vwin, int show_window);
int video_window_is_visible (xui_vwin_t *vwin);

int video_window_set_mag (xui_vwin_t *vwin, float xmag, float ymag);
void video_window_get_mag (xui_vwin_t *vwin, float *xmag, float *ymag);

void video_window_update_logo (xui_vwin_t *vwin);
void video_window_change_skins (xui_vwin_t *vwin, int);

long int video_window_get_ssaver_idle (xui_vwin_t *vwin);
long int video_window_reset_ssaver (xui_vwin_t *vwin);

void video_window_get_frame_size (xui_vwin_t *vwin, int *w, int *h);
void video_window_get_visible_size (xui_vwin_t *vwin, int *w, int *h);
void video_window_get_output_size (xui_vwin_t *vwin, int *w, int *h);

void video_window_set_mrl (xui_vwin_t *vwin, char *mrl);

void video_window_toggle_border (xui_vwin_t *vwin);

void video_window_set_transient_for (xui_vwin_t *vwin, Window w);

/* call this with 1 before and with 0 after accessing the video window directly */
void video_window_lock (xui_vwin_t *vwin, int lock_or_unlock);

#endif
