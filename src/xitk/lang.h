/* 
 * Copyright (C) 2000-2009 the xine project
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

#ifndef LANG_H
#define LANG_H

/* This is just an alpha sort order. */
typedef enum {
  NONE     = 0,
  ENGLISH,
  FRENCH,
  GERMAN,
  PORTUGUESE,
  SPANISH,
  POLISH,
  UKRAINIAN,
  CZECH,
  NORWEGIAN_BOKMAL
} lang_code_t;

typedef struct {
  char         lang[20];
  lang_code_t  code;
  char         ext[8];
  char         doc_encoding[16];
} langs_t;

const langs_t *get_lang(void);

const char *get_language_from_iso639_1(const char *two_letters);

#endif
