/*
 * Copyright (C) 2002-2003 Stefan Holst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
 *
 * $Id$
 *
 * utilities
 */

#ifndef HAVE_UTILS_H
#define HAVE_UTILS_H

/*
 * multi purpose scheduler
 */

void start_scheduler(void);
void stop_scheduler(void);

/*
 * will execute cb(data) after <delay> ms.
 * returns job id.
 */

int schedule_job(int delay, void (*cb)(void *data), void *data);

/*
 * cancel pending job width given job id.
 */

void cancel_job(int job_id);

void lock_job_mutex(void);
void unlock_job_mutex(void);

/*
 * heap management
 */

/* semipublic functions*/
void *_gen_malloc   ( size_t, const char * , char *, int );
void *_gen_free     ( void * );
void *_gen_realloc  ( void *, size_t, char *, int );
void *_gen_strdup   ( const char *, char *, int );

/*
 * allocs a new heap object of given type
 */

#define ho_new(type)  (_gen_malloc(sizeof(type),NULL,__FILE__,__LINE__))
#define ho_new_tagged(type,text)  (_gen_malloc(sizeof(type),text,__FILE__,__LINE__))

/*
 * resizes a heap object
 */

#define ho_resize(hObj, size) (hObj=_gen_realloc((hObj), (size_t)(size),__FILE__,__LINE__))

/*
 * frees a heap object
 */

#define ho_free(hObj) (hObj=_gen_free(hObj))

/*
 * allocs a new string of given size
 */

#define ho_newstring(wSize) (_gen_malloc((size_t)(wSize),"string",__FILE__,__LINE__))

/*
 * like strdup, but returns a valid heap object
 */

#define ho_strdup(lpSrc) (_gen_strdup(lpSrc,__FILE__,__LINE__))

/*
 * test wether a heap pointer is valid
 */

int ho_verify(void *ho);

/*
 * prints info about all allocated data chunks to stdout
 */

void heapstat(void);


#if 0
#define _LPV(hObj) *(void*)&hObj
/*--- Array interface macros ---*/
#define NEWARRAY(lpArray, wSize) \
  (_LPV(lpArray)=FmNew((SIZET)(sizeof(*(lpArray))*(wSize)), \
  NULL,szSRCFILE,__LINE__))
#endif

#endif
