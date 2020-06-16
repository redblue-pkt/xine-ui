/* 
 * Copyright (C) 2000-2017 the xine project
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
 */

#ifndef EVENT_H
#define EVENT_H

#include <X11/Xlib.h>

#include "common.h"

typedef struct {
  int    num_files;
  char **filenames;

  /* X11 */
  int            window_id;
  int            borderless;
  /*const*/char *geometry;
  /*const*/char *prefered_visual;
  int            install_colormap;
} gui_init_params_t;

int hidden_file_cb(int action, int value);
void dummy_config_cb(void *data, xine_cfg_entry_t *cfg);
int actions_on_start(action_id_t actions[], action_id_t a);
void gui_deinit (gGui_t *gui);
void gui_init (gGui_t *gui, gui_init_params_t *params);
void gui_run (gGui_t *gui, char **session_opts);
int gui_playlist_play (gGui_t *gui, int idx);
void gui_playlist_start_next (gGui_t *gui);
void gui_execute_action_id (gGui_t *gui, action_id_t id);
void gui_handle_event (XEvent *event, void *gui);

int wm_not_ewmh_only(void);
#endif
