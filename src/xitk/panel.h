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
 * xine panel related stuff
 *
 */

#ifndef PANEL_H
#define PANEL_H

#include <pthread.h>
#include "xitk.h"

typedef struct {
  xitk_widget_list_t   *widget_list;
  
  xitk_widget_t        *title_label;
  xitk_widget_t        *runtime_label;
  xitk_widget_t        *slider_play;

  struct {
    xitk_widget_t        *slider;
    xitk_widget_t        *mute;
  } mixer;

  struct {
    int                   enable;
    unsigned int          timeout;
  } tips;

  xitk_widget_t        *checkbox_pause;
  int                   visible;
  char                  runtime[20];
  char                  audiochan[20];
  xitk_widget_t        *audiochan_label;
  xitk_widget_t        *autoplay_plugins[64];
  char                  spuid[20];
  xitk_widget_t        *spuid_label;
  ImlibImage           *bg_image;
  xitk_register_key_t   widget_key;
  pthread_t             slider_thread;
} _panel_t;

void panel_show_tips(void);
int panel_get_tips_enable(void);
unsigned long panel_get_tips_timeout(void);

void panel_init (void);

void panel_change_skins(void);

void panel_add_autoplay_buttons(void);

void panel_add_mixer_control(void);

int panel_is_visible(void);

void panel_toggle_visibility (xitk_widget_t *w, void *data);

void panel_toggle_audio_mute(xitk_widget_t *w, void *data, int status);

void panel_execute_xineshot(xitk_widget_t *w, void *data);

void panel_snapshot(xitk_widget_t *w, void *data);

void panel_check_pause(void);

void panel_check_mute(void);

void panel_reset_slider (void);

void panel_update_channel_display (void);

void panel_update_runtime_display(void);

void panel_set_no_mrl(void);

void panel_update_mrl_display (void);

void panel_layer_above(int);

void panel_set_title(char*);

#endif
