/* 
 * Copyright (C) 2000-2003 the xine project
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

#ifndef POST_H
#define POST_H

#include <xine.h>

typedef struct {
  xine_post_t    *post;
  char           *name;
} post_element_t;

const char **post_get_audio_plugins_names(void);
void post_init(void);
void post_rewire_visual_anim(void);
int post_rewire_audio_port_to_stream(xine_stream_t *stream);
int post_rewire_audio_post_to_stream(xine_stream_t *stream);

void pplugin_end(void);
int pplugin_is_visible(void);
int pplugin_is_running(void);
void pplugin_toggle_visibility(xitk_widget_t *w, void *data);
void pplugin_raise_window(void);
void pplugin_update_enable_button(void);
void pplugin_panel(void);
void pplugin_parse_and_store_post(const char *post);
void pplugin_rewire_from_posts_window(void);
void pplugin_rewire_posts(void);
int pplugin_is_post_selected(void);

void post_deinterlace_init(const char *deinterlace_post);
void post_deinterlace(void);

#endif
