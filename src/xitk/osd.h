/*
 * Copyright (C) 2000-2004 the xine project
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

#ifndef __OSD_H__
#define __OSD_H__

#include <stdarg.h>

typedef struct {
  xine_osd_t   *osd[2];
  int           visible;
  int           have_text;
  int           x, y;
  int           w, h;
} osd_object_t;

#define OSD_BAR_PROGRESS 1
#define OSD_BAR_POS      2
#define OSD_BAR_POS2     3
#define OSD_BAR_STEPPER  4

void osd_init(void);
void osd_hide_sinfo(void);
void osd_hide_bar(void);
void osd_hide_status(void);
void osd_hide_info(void);
void osd_hide(void);
void osd_deinit(void);
void osd_update(void);
void osd_display_spu_lang(void);
void osd_display_audio_lang(void);
void osd_stream_infos(void);
void osd_update_status(void);
void osd_stream_position(int pos);
void osd_display_info(char *info, ...) __attribute__ ((format (printf, 1, 2)));

/* see OSD_BAR_* */
void osd_draw_bar(char *title, int min, int max, int val, int type);

void osd_update_osd(void);

#endif
