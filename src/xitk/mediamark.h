/*
** Copyright (C) 2002 Daniel Caujolle-Bert <segfault@club-internet.fr>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef __MEDIAMARK_H__
#define __MEDIAMARK_H__

#include "common.h"

typedef struct {
  char                     *ident;
  char                     *mrl;
  int                       start; /*  0..x (secs)                     */
  int                       end;   /* -1 == <till the end> else (secs) */
} mediamark_t;

int mediamark_store_mmk(mediamark_t **mmk, const char *mrl, const char *ident, int start, int end);
void mediamark_add_entry(const char *mrl, const char *ident, int start, int end);
void mediamark_free_mediamarks(void);
void mediamark_replace_entry(mediamark_t **mmk, const char *mrl, const char *ident, int start, int end);
void mediamark_free_entry(int offset);
int mediamark_get_entry_from_id(const char *ident);

const mediamark_t *mediamark_get_current_mmk();
const char *mediamark_get_current_mrl(void);
const char *mediamark_get_current_ident(void);

void mediamark_load_mediamarks(const char *filename);
void mediamark_save_mediamarks(const char *filename);

#endif
