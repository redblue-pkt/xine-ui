/* 
 * Copyright (C) 2000-2006 the xine project
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
 * $Id$
 *
 */
#ifndef _LIBCOMMON__H
#define _LIBCOMMON__H

#include "utils.h"

#ifndef HAVE_GETLINE
ssize_t getline(char *lineptr, size_t *n, FILE *f);
#endif

#ifndef HAVE_GETSUBOPT
int getsubopt(char **optionp, char * const *keylistp, char **valuep);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

#endif
