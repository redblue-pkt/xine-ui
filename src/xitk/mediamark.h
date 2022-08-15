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

  enum {
    MMK_TYPE_FILE = 0,
    MMK_TYPE_NET
  }                         type;
  enum {
    MMK_FROM_USER = 0,
    MMK_FROM_PLAYLIST,
    MMK_FROM_DIR
  }                         from;

  int                       played; /* used with shuffle loop mode */

  int                       got_alternate;
  alternate_t              *cur_alt;
  alternate_t              *alternates;
} mediamark_t;

typedef enum {
  MMK_VAL_IDENT = 0,
  MMK_VAL_MRL,
  MMK_VAL_SUB,
  MMK_VAL_ADD_ALTER
} mmk_val_t;

/** you can read mediamark_t yourself. to modify, use these. */
/** next 3 return changed. */
int mediamark_copy (mediamark_t **to, const mediamark_t *from);
int mediamark_set_str_val (mediamark_t **mmk, const char *value, mmk_val_t what);
int mediamark_free (mediamark_t **mmk);

#define mediamark_have_alternates(_mmk) ((_mmk)->alternates != NULL)
void mediamark_free_alternates(mediamark_t *mmk);
char *mediamark_get_first_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_next_alternate_mrl(mediamark_t *mmk);
char *mediamark_get_current_alternate_mrl(mediamark_t *mmk);
void mediamark_append_alternate_mrl(mediamark_t *mmk, const char *mrl);
void mediamark_duplicate_alternates (const mediamark_t *s_mmk, mediamark_t *d_mmk);
int mediamark_got_alternate(mediamark_t *mmk);
void mediamark_unset_got_alternate(mediamark_t *mmk);


/** gui currently played item. */
#define GUI_MMK_NONE -1
#define GUI_MMK_CURRENT -2
/** returns the real index used. */
int gui_current_set_index (gGui_t *gui, int idx);
void gui_current_free (gGui_t *gui);


/** gui playlist stuff. */
void gui_playlist_load (gGui_t *gui, const char *filename);
#define GUI_MAX_DIR_LEVELS 8
/** recursively scan this dir for playable files or add this file. return found count.
 *  a negative max_levels will add _all_ files, even hidden and unknown ext ones.
 *  a zero max_levels forbids scanning dirs and playlist files. */
int gui_playlist_add_dir (gGui_t *gui, const char *filepathname, int max_levels);
/** add refs from this playlist file. */
int gui_playlist_add_file (gGui_t *gui, const char *filename);
/** add 1 entry manually. */
#define gui_playlist_append(_gui,_mrl,_ident,_sub,_start,_end,_av_offset,_spu_offset) \
  gui_playlist_insert (_gui, -1, _mrl, _ident, _sub, _start, _end, _av_offset, _spu_offset)
int gui_playlist_insert (gGui_t *gui, int index, const char *mrl, const char *ident,
  const char *sub, int start, int end, int av_offset, int spu_offset);
/** move n entries starting at index by diff. return new start index. */
int gui_playlist_move (gGui_t *gui, int index, int n, int diff);
/** remove 1 entry manually. return new entries count. */
int gui_playlist_remove (gGui_t *gui, int index);
/** returns the real index used. */
int gui_playlist_set_str_val (gGui_t *gui, const char *value, mmk_val_t what, int idx);
mediamark_t *mediamark_get_current_mmk (gGui_t *gui);
const char *mediamark_get_current_mrl (gGui_t *gui);
const char *mediamark_get_current_ident (gGui_t *gui);
mediamark_t *mediamark_get_mmk_by_index (gGui_t *gui, int index);
void gui_playlist_save (gGui_t *gui, const char *filename);
void gui_playlist_free (gGui_t *gui);

void mediamark_reset_played_state (gGui_t *gui);
int mediamark_all_played (gGui_t *gui);
int mediamark_get_shuffle_next (gGui_t *gui);
int mediamark_get_entry_from_id (gGui_t *gui, const char *ident);

/** example: https://vids.anywhere.net/ready/to/rumble/trailer.mp4?again=1#start=0:02:55
 *  buf[0]:
 *    pad[8]
 *  start:
 *    https
 *  protend:
 *    ://
 *  host:
 *    vids.anywhere.net
 *  root:
 *    /ready/to/rumble/
 *  lastpart:
 *    trailer.
 *  ext:
 *    mp4
 *  args:
 *    ?again=1
 *  info:
 *    #start=0:02:55
 *  end:
 *    \0, free[n]
 *  max:
 *    pad[8]
 *  buf[sizeof (buf)]
 */
typedef struct {
  char *start, *protend, *host, *root, *lastpart, *ext, *args, *info, *end, *max;
  char buf[2048];
} mrl_buf_t;
void mrl_buf_init (mrl_buf_t *mrlb);
/** base here is just a hint for interpreting ? and #. can be NULL. */
int mrl_buf_set (mrl_buf_t *mrlb, mrl_buf_t *base, const char *name);
/** for security, this will drop info for network type base. */
void mrl_buf_merge (mrl_buf_t *to, mrl_buf_t *base, mrl_buf_t *name);
int mrl_buf_is_file (mrl_buf_t *mrlb);

size_t mrl_get_lowercase_prot (char *buf, size_t bsize, const char *mrl);
int mrl_look_like_playlist (const char *mrl);
int mrl_look_like_file (const char *mrl);


/** mediamark editor window. */
void mmk_edit_mediamark (gGui_t *gui, mediamark_t **mmk, apply_callback_t callback, void *data);
void mmk_editor_toggle_visibility (gGui_t *gui);
void mmk_editor_raise_window (gGui_t *gui);
void mmk_editor_end (gGui_t *gui);
void mmk_editor_set_mmk (gGui_t *gui, mediamark_t **mmk);

#endif
