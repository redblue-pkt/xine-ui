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
 * all the various actions for the gui (play, stop, open, pause...)
 *
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#include "xitk.h"

#define GUI_NEXT     1
#define GUI_PREV     2
#define GUI_RESET    3

void gui_exit (xitk_widget_t *w, void *data);

void gui_play (xitk_widget_t *w, void *data);

void gui_stop (xitk_widget_t *w, void *data);

void gui_pause (xitk_widget_t *w, void *data, int state) ;

void gui_eject(xitk_widget_t *w, void *data);

void gui_toggle_visibility(xitk_widget_t *w, void *data);

void gui_set_fullscreen_mode(xitk_widget_t *w, void *data);

void gui_toggle_aspect(void);

void gui_toggle_interlaced(void);

void gui_change_audio_channel(xitk_widget_t *w, void *data);

void gui_change_spu_channel(xitk_widget_t *w, void *data);

void gui_change_speed_playback(xitk_widget_t *w, void *data);

void gui_set_current_position (int pos);

void gui_seek_relative (int off_sec) ;

void gui_dndcallback (char *filename);

void gui_change_skin(xitk_widget_t *w, void *data);

void gui_nextprev(xitk_widget_t *w, void *data);

void gui_playlist_show(xitk_widget_t *w, void *data);

void gui_mrlbrowser_show(xitk_widget_t *w, void *data);

void gui_set_current_mrl(char *mrl);

char *gui_get_next_mrl (void);

void gui_control_show(xitk_widget_t *w, void *data);

void gui_mrl_browser_show(xitk_widget_t *w, void *data);

void gui_setup_show(xitk_widget_t *w, void *data);

void gui_viewlog_show(xitk_widget_t *w, void *data);

void gui_kbedit_show(xitk_widget_t *w, void *data);

void layer_above_video(Window w);

void gui_increase_audio_volume(void);

void gui_decrease_audio_volume(void);

void gui_change_zoom(int zoom_d);

void gui_reset_zoom(void);

void gui_toggle_tvmode(void);

void gui_send_expose_to_window(Window window);
#endif
