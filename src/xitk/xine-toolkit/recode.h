/* 
 * Copyright (C) 2000-2008 the xine project
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

/* Unless building recode.c, make xitk_recode_t an opaque type.
 */

#ifndef BUILD_RECODE_C
typedef void xitk_recode_t;
#endif

#ifdef HAVE_ICONV

/**
 * prepare recoding
 *   - when one of encoding is NULL, is prepared no conversion, strings will
 *     be strdup()ed
 *   - when encoding is "", it's got from system
 */
xitk_recode_t *xitk_recode_init(const char *src_encoding, const char *dst_encoding);

/**
 * recode string 'src' into 'dst' according to prepared 'xr'
 */
char *xitk_recode(xitk_recode_t *xr, const char *src);

/**
 * destroy recoding
 */
void xitk_recode_done(xitk_recode_t *xr);

#else

/* If we're not using iconv(), just define everything as no-op, and
 * xitk_recode as strdup.
 */

#define xitk_recode_init(src_encoding, dst_encoding) NULL
#define xitk_recode(xr, src) strdup(src)
#define xitk_recode_done(xr)

#endif

#endif
