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

#ifndef __MEDIAMARK_H__
#define __MEDIAMARK_H__

#include "common.h"

typedef void (*apply_callback_t)(void *data);

typedef struct alternate_s alternate_t;
struct alternate_s {
  char                     *mrl;
  alternate_t              *next;
};

typedef struct {
  char                     *ident;
  char                     *mrl;
  char                     *sub;
  int                       start;  /*  0..x (secs)                     */
  int                       end;    /* -1 == <till the end> else (secs) */
  int                       av_offset;
  int                       spu_offset;

  int                       played; /* used with shuffle loop mode */

  int                       got_alternate;
  alternate_t              *cur_alt;
  alternate_t              *alternates;
} mediamark_t;


int mediamark_have_alternates(mediamark_t *mmk);
void mediamark_free_alternates(mediamark_t *mmk);
char *mediamark_get_first_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_next_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_current_alternate_mrl(mediamark_t *mmk);
void mediamark_append_alternate_mrl(mediamark_t *mmk, const char *mrl);
void mediamark_duplicate_alternates(mediamark_t *s_mmk, mediamark_t *d_mmk);
int mediamark_got_alternate(mediamark_t *mmk);
void mediamark_set_got_alternate(mediamark_t *mmk);
void mediamark_unset_got_alternate(mediamark_t *mmk);

int mediamark_free_mmk(mediamark_t **mmk);
int mediamark_store_mmk(mediamark_t **mmk, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
mediamark_t *mediamark_clone_mmk(mediamark_t *mmk);
void mediamark_insert_entry(int index, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_append_entry(const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_free_mediamarks(void);
void mediamark_replace_entry(mediamark_t **mmk, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_free_entry(int offset);
void mediamark_reset_played_state(void);
int mediamark_all_played(void);
int mediamark_get_shuffle_next(void);
int mediamark_get_entry_from_id(const char *ident);

mediamark_t *mediamark_get_current_mmk(void);
const char *mediamark_get_current_mrl(void);
const char *mediamark_get_current_ident(void);
const char *mediamark_get_current_sub(void);
mediamark_t *mediamark_get_mmk_by_index(int index);

int mediamark_concat_mediamarks(const char *filename);
void mediamark_load_mediamarks(const char *filename);
void mediamark_save_mediamarks(const char *filename);

int mrl_look_like_playlist(char *mrl);
int mrl_look_like_file(char *mrl);
void mediamark_collect_from_directory(char *filepathname);

void mmk_editor_update_tips_timeout(unsigned long timeout);
void mmk_editor_show_tips(int enabled, unsigned long timeout);
void mmk_edit_mediamark(mediamark_t **mmk, apply_callback_t callback, void *data);
int mmk_editor_is_visible(void);
int mmk_editor_is_running(void);
void mmk_editor_toggle_visibility(void);
void mmk_editor_raise_window(void);
void mmk_editor_end(void);
void mmkeditor_set_mmk(mediamark_t **mmk);

#endif
