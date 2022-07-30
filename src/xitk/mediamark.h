/*
 * Copyright (C) 2000-2022 the xine project
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

typedef enum {
  MMK_VAL_IDENT = 0,
  MMK_VAL_MRL,
  MMK_VAL_SUB
} mmk_val_t;

int mediamark_set_str_val (mediamark_t **mmk, const char *value, mmk_val_t what);

#define mediamark_have_alternates(_mmk) ((_mmk)->alternates != NULL)
void mediamark_free_alternates(mediamark_t *mmk);
char *mediamark_get_first_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_next_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_current_alternate_mrl(mediamark_t *mmk);
void mediamark_append_alternate_mrl(mediamark_t *mmk, const char *mrl);
void mediamark_duplicate_alternates (const mediamark_t *s_mmk, mediamark_t *d_mmk);
int mediamark_got_alternate(mediamark_t *mmk);
void mediamark_set_got_alternate(mediamark_t *mmk);
void mediamark_unset_got_alternate(mediamark_t *mmk);

int mediamark_free_mmk(mediamark_t **mmk);
int mediamark_store_mmk(mediamark_t **mmk, const char *mrl, const char *ident, const char *sub, int start, int end, int av_offset, int spu_offset);
mediamark_t *mediamark_clone_mmk(mediamark_t *mmk);
#define mediamark_append_entry(_gui,_mrl,_ident,_sub,_start,_end,_av_offset,_spu_offset) \
  mediamark_insert_entry (_gui, -1, _mrl, _ident, _sub, _start, _end, _av_offset, _spu_offset)
int mediamark_insert_entry (gGui_t *gui, int index, const char *mrl, const char *ident,
  const char *sub, int start, int end, int av_offset, int spu_offset);
void mediamark_free_mediamarks (gGui_t *gui);
void mediamark_delete_entry (gGui_t *gui, int offset);
void mediamark_reset_played_state (gGui_t *gui);
int mediamark_all_played (gGui_t *gui);
int mediamark_get_shuffle_next (gGui_t *gui);
int mediamark_get_entry_from_id (gGui_t *gui, const char *ident);

mediamark_t *mediamark_get_current_mmk (gGui_t *gui);
const char *mediamark_get_current_mrl (gGui_t *gui);
const char *mediamark_get_current_ident (gGui_t *gui);
const char *mediamark_get_current_sub (gGui_t *gui);
mediamark_t *mediamark_get_mmk_by_index (gGui_t *gui, int index);

int mediamark_concat_mediamarks (gGui_t *gui, const char *filename);
void mediamark_load_mediamarks (gGui_t *gui, const char *filename);
void mediamark_save_mediamarks (gGui_t *gui, const char *filename);

int mrl_look_like_playlist (const char *mrl);
int mrl_look_like_file(char *mrl);
void mediamark_collect_from_directory (gGui_t *gui, char *filepathname);

void mmk_edit_mediamark (gGui_t *gui, mediamark_t **mmk, apply_callback_t callback, void *data);
void mmk_editor_toggle_visibility (gGui_t *gui);
void mmk_editor_raise_window (gGui_t *gui);
void mmk_editor_end (gGui_t *gui);
void mmk_editor_set_mmk (gGui_t *gui, mediamark_t **mmk);

#endif
