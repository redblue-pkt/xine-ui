/* 
 * Copyright (C) 2000-2001 the xine project
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
 * debug print and profiling functions
 *
 */
 
#ifndef HAVE_MONITOR_H
#define HAVE_MONITOR_H

#include <inttypes.h>

extern uint32_t xine_debug;

#define VERBOSE        (xine_debug & 0x8000>>1)
#define METRONOM       (xine_debug & 0x8000>>2)
#define AUDIO          (xine_debug & 0x8000>>3)
#define DEMUX          (xine_debug & 0x8000>>4)
#define INPUT          (xine_debug & 0x8000>>5)
#define VIDEO          (xine_debug & 0x8000>>6)
#define VPTS           (xine_debug & 0x8000>>7)
#define MPEG           (xine_debug & 0x8000>>8)
#define VAVI           (xine_debug & 0x8000>>9)
#define AC3            (xine_debug & 0x8000>>10)
#define LOOP           (xine_debug & 0x8000>>11)
#define GUI            (xine_debug & 0x8000>>12)

#ifdef  __GNUC__
#define perr(FMT,ARGS...) {fprintf(stderr, FMT, ##ARGS);fflush(stderr);}
#else   /* C99 version: */
#define perr(...)         {fprintf(stderr, __VA_ARGS__);fflush(stderr);}
#endif

#ifdef DEBUG

/*
 * Debug stuff
 */

//#define perr(FMT,ARGS...) {fprintf(stderr, FMT, ##ARGS);fflush(stderr);}

#ifdef  __GNUC__
#define xprintf(LVL, FMT, ARGS...) {                                          \
                                     if(LVL) {                                \
                                       printf(FMT, ##ARGS);		      \
                                     }                                        \
                                   }
#else	/* C99 version: */
#define xprintf(LVL, ...) {						      \
                                     if(LVL) {                                \
                                       printf(__VA_ARGS__);		      \
                                     }                                        \
                                   }
/*
 * profiling
 */

void profiler_init ();

void profiler_set_label (int id, char *label);

void profiler_start_count (int id);

void profiler_stop_count (int id);

void profiler_print_results ();

#else /* no DEBUG, release version */

//#define perr(FMT,ARGS...) 

#ifdef  __GNUC__
#define xprintf(LVL, FMT, ARGS...) 
#else   /* C99 version: */
#define xprintf(LVL, ...) 
#endif

#define profiler_init()
#define profiler_set_label(id, label)
#define profiler_start_count(id)
#define profiler_stop_count(id)
#define profiler_print_results()

#endif /* DEBUG*/

#endif /* HAVE_MONITOR_H */
