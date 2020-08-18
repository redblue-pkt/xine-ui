/* 
 * Copyright (C) 2000-2020 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * all the various actions for the gui (play, stop, open, pause...)
 *
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#include "common.h"

/* see gui_nextprev below. */
#define GUI_NEXT(gui)  ((gui)->nextprev + 2)
#define GUI_PREV(gui)  ((gui)->nextprev + 0)
#define GUI_RESET(gui) ((gui)->nextprev + 1)

/* windows */
void gui_reparent_all_windows (gGui_t *gui);
void raise_window(xitk_window_t *xwin, int visible, int running);
void toggle_window(xitk_window_t *xwin, xitk_widget_list_t *widget_list, int *visible, int running);
void layer_above_video(xitk_window_t *xwin);
int get_layer_above_video (gGui_t *gui);
int is_layer_above(void);
void wait_for_window_visible(xitk_window_t *xwin);
void gui_change_skin(xitk_widget_t *w, void *data);
void gui_playlist_show (xitk_widget_t *w, void *gui);
void gui_mrlbrowser_show (xitk_widget_t *w, void *gui);
void gui_control_show (xitk_widget_t *w, void *gui);
void gui_mrl_browser_show(xitk_widget_t *w, void *data);
void gui_setup_show (xitk_widget_t *w, void *gui);
void gui_event_sender_show (xitk_widget_t *w, void *gui);
void gui_viewlog_show (xitk_widget_t *w, void *gui);
void gui_kbedit_show (xitk_widget_t *w, void *gui);
void gui_help_show (xitk_widget_t *w, void *gui);
void gui_stream_infos_show (xitk_widget_t *w, void *gui);
void gui_tvset_show (xitk_widget_t *w, void *gui);
void gui_vpp_show (xitk_widget_t *w, void *gui);
void gui_app_show (xitk_widget_t *w, void *gui);

/* playlist */
void gui_display_logo (gGui_t *gui);
int gui_xine_play (gGui_t *gui, xine_stream_t *stream, int start_pos, int start_time, int update_mmk);
int gui_open_and_play_alternates(mediamark_t *mmk, const char *sub);
int gui_xine_open_and_play(char *mrl, char *sub, int start_pos, 
			   int start_time, int av_offset, int spu_offset, int report_error);
void gui_dndcallback (const char *filename);
void gui_step_mrl (gGui_t *gui, int by);
void gui_nextprev_mrl (xitk_widget_t *w, void *gui_nextprev);
void gui_set_current_mmk(mediamark_t *mmk);
void gui_set_current_mmk_by_index(int idx);
void gui_add_mediamark(void);
void gui_file_selector(void);

/* main ctrl */
void gui_exit (xitk_widget_t *w, void *gui);
void gui_exit_2 (gGui_t *gui); /** << internal use */
void gui_play (xitk_widget_t *w, void *gui);
void gui_stop (xitk_widget_t *w, void *gui);
void gui_close (xitk_widget_t *w, void *gui);
void gui_pause (xitk_widget_t *w, void *gui, int state);
void gui_eject (xitk_widget_t *w, void *gui);
void gui_nextprev_speed (xitk_widget_t *w, void *gui_nextprev);
void gui_set_current_position (int pos);
void gui_seek_relative (int off_sec) ;
int gui_xine_get_pos_length (gGui_t *gui, xine_stream_t *stream, int *pos, int *time, int *length);

/* video */
void gui_toggle_visibility (xitk_widget_t *w, void *gui);
void gui_toggle_border (xitk_widget_t *w, void *gui);
void gui_set_fullscreen_mode (xitk_widget_t *w, void *gui);
#ifdef HAVE_XINERAMA
void gui_set_xinerama_fullscreen_mode(void);
#endif
void gui_toggle_aspect (gGui_t *gui, int aspect);
void gui_toggle_interlaced (gGui_t *gui);
void gui_vpp_enable (gGui_t *gui);
void gui_change_zoom(int zoom_dx, int zoom_dy);
void gui_reset_zoom(void);
void gui_toggle_tvmode(void);

/* audio */
void gui_direct_change_audio_channel (xitk_widget_t *w, void *gui, int value);
void gui_nextprev_audio_channel (xitk_widget_t *w, void *gui_nextprev);
void change_audio_vol(int value);
void gui_increase_audio_volume(void);
void gui_decrease_audio_volume(void);
void gui_app_enable (gGui_t *gui);
void change_amp_vol(int value);
void gui_increase_amp_level(void);
void gui_decrease_amp_level(void);
void gui_reset_amp_level(void);

/* subtitle */
void gui_direct_change_spu_channel (xitk_widget_t *w, void *gui, int value);
void gui_nextprev_spu_channel (xitk_widget_t *w, void *gui_nextprev);
void gui_select_sub(void);

/* anim */
void visual_anim_init(void);
void visual_anim_done(void);
void visual_anim_add_animation(char *mrl);
void visual_anim_play(void);
void visual_anim_play_next(void);
void visual_anim_stop(void);

#endif
