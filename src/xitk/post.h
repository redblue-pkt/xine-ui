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
 */

#ifndef POST_H
#define POST_H

#include <xine.h>

typedef struct {
  xine_post_t    *post;
  char           *name;
} post_element_t;

typedef struct post_win_s post_win_t;

typedef enum {
  POST_AUDIO = 0,
  POST_VIDEO
} post_type_t;

typedef struct {
  gGui_t              *gui;
  xine_stream_t       *stream;
  post_win_t          *win;
  post_element_t     **elements;
  int                  num_elements;
  post_type_t          type;
  union {
    struct {
      char           **audio_vis_plugins;
      int              ignore_visual_anim;
    }                  audio;
    struct {
      const char      *deinterlace_plugin;
      post_element_t **deinterlace_elements;
      int              num_deinterlace_elements;
    }                  video;
  }                    info;
} post_info_t;

const char * const *post_get_audio_plugins_names (gGui_t *gui);
void post_init (gGui_t *gui);
void post_rewire_visual_anim (gGui_t *gui);
int post_rewire_audio_port_to_stream (gGui_t *gui, xine_stream_t *stream);
int post_rewire_audio_post_to_stream (gGui_t *gui, xine_stream_t *stream);
void post_deinit (gGui_t *gui);

void pplugin_end (post_info_t *info);
int pplugin_is_visible (post_info_t *info);
int pplugin_is_running (post_info_t *info);
void pplugin_toggle_visibility (xitk_widget_t *w, void *info);
void pplugin_raise_window (post_info_t *info);
void pplugin_update_enable_button (post_info_t *info);
void pplugin_panel (post_info_t *info);
void pplugin_parse_and_store_post (post_info_t *info, const char *post);
void pplugin_rewire_from_posts_window (post_info_t *info);
void pplugin_rewire_posts (post_info_t *info);
int pplugin_is_post_selected (post_info_t *info);
void pplugin_reparent (post_info_t *info);

void post_deinterlace_init (gGui_t *gui, const char *deinterlace_post);
void post_deinterlace (gGui_t *gui);

#endif
