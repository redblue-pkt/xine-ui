/*
 * Copyright (C) 2000-2019 the xine project
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
#ifndef HAVE_XITK_RECODE_H
#define HAVE_XITK_RECODE_H

typedef struct xitk_recode_s xitk_recode_t;

typedef struct {
  const char *src;    /** << text to recode (no \0 needed) */
  size_t      ssize;  /** << bytes to recode */
  char       *buf;    /** << optional application supplied, or NULL */
  size_t      bsize;  /** << bytes room in buf */
  const char *res;    /** << can be src, buf, or malloc'ed. no \0. */
  size_t      rsize;  /** << bytes in res */
} xitk_recode_string_t;

#ifdef HAVE_ICONV

/**
 * prepare recoding
 *   - when one of encoding is NULL, is prepared no conversion, strings will
 *     be strdup()ed
 *   - when encoding is "", it's got from system
 */
xitk_recode_t *xitk_recode_init (const char *src_encoding, const char *dst_encoding, int threadsafe);

/**
 * recode string 'src' into 'dst' according to prepared 'xr'
 */
char *xitk_recode(xitk_recode_t *xr, const char *src);

/**
 * destroy recoding
 */
void xitk_recode_done(xitk_recode_t *xr);

void xitk_recode2_do (xitk_recode_t *xr, xitk_recode_string_t *s);
void xitk_recode2_done (xitk_recode_t *xr, xitk_recode_string_t *s);

#else

/* If we're not using iconv(), just define everything as no-op, and
 * xitk_recode as strdup.
 */

#define xitk_recode_init(src_encoding, dst_encoding, threadsafe) NULL
#define xitk_recode(xr, src) strdup(src)
#define xitk_recode_done(xr)
#define xitk_recode2_do(xr, rs) do {(rs)->res = (rs)->src; (rs)->rsize = (rs)->ssize;} while (0)
#define xitk_recode2_done(xr, rs) (rs)->res = NULL

#endif

#endif
