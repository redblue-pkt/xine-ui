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

#ifndef __MEDIAMARK_H__
#define __MEDIAMARK_H__

#include "common.h"

typedef void (*apply_callback_t)(void *data);

typedef struct {
  char                     *ident;
  char                     *mrl;
  char                     *sub;
  int                       start;  /*  0..x (secs)                     */
  int                       end;    /* -1 == <till the end> else (secs) */
  int                       av_offset;
  int                       spu_offset;
  int                       played; /* used with shuffle loop mode */
} mediamark_t;

int mediamark_free_mmk(mediamark_t **mmk);
int mediamark_store_mmk(mediamark_t **mmk, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
mediamark_t *mediamark_clone_mmk(mediamark_t *mmk);
void mediamark_add_entry(const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_free_mediamarks(void);
void mediamark_replace_entry(mediamark_t **mmk, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_free_entry(int offset);
void mediamark_reset_played_state(void);
int mediamark_all_played(void);
int mediamark_get_shuffle_next(void);
int mediamark_get_entry_from_id(const char *ident);

const mediamark_t *mediamark_get_current_mmk();
const char *mediamark_get_current_mrl(void);
const char *mediamark_get_current_ident(void);
const char *mediamark_get_current_sub(void);

int mediamark_concat_mediamarks(const char *filename);
void mediamark_load_mediamarks(const char *filename);
void mediamark_save_mediamarks(const char *filename);

int mrl_looks_playlist(char *mrl);

void mmk_edit_mediamark(mediamark_t **mmk, apply_callback_t callback, void *data);
int mmk_editor_is_visible(void);
int mmk_editor_is_running(void);
void mmk_editor_toggle_visibility(void);
void mmk_editor_raise_window(void);
void mmk_editor_end(void);
void mmkeditor_set_mmk(mediamark_t **mmk);

#endif
