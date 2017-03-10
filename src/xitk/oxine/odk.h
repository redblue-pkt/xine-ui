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
 * odk abstracts from xine and other software / hardware interfaces
 */

/*
 * the features of odk are:
 *
 * - framesize independent (virtual) grid (currently 800x600)
 * - color palette management and generation of text palettes
 * - text alignments
 */
 
#ifndef HAVE_ODK_H
#define HAVE_ODK_H

#include <xine.h>
#include <pthread.h>
#include "oxine_event.h"

/*
 * alignments for text
 */

#define ODK_ALIGN_LEFT     0x001
#define ODK_ALIGN_CENTER   0x002
#define ODK_ALIGN_RIGHT    0x004
#define ODK_ALIGN_TOP      0x008
#define ODK_ALIGN_VCENTER  0x010
#define ODK_ALIGN_BOTTOM   0x020

#define PIXMAP_SIMPLE_ARROW_UP 1
#define PIXMAP_SIMPLE_ARROW_DOWN 2

/*
 * stream speeds
 */

#define ODK_SPEED_PAUSE    0
#define ODK_SPEED_SLOW_4   1
#define ODK_SPEED_SLOW_2   2
#define ODK_SPEED_NORMAL   4
#define ODK_SPEED_FAST_2   8
#define ODK_SPEED_FAST_4   16

/*
 * opaque odk data type
 */

typedef struct odk_s odk_t;

/*
 * initializes xine streams, create a drawable and returns a new odk object.
 * the given xine struct must have been initialized.
 */

odk_t *odk_init(void);

/*
 * draw primitives
 */

void odk_draw_point(odk_t *odk, int x, int y, int color);
void odk_draw_line(odk_t *odk, int x1, int y1, int x2, int y2, int color);
void odk_draw_rect(odk_t *odk, int x1, int y1, int x2, int y2, int filled, int color);
void odk_draw_text(odk_t *odk, int x, int y, const char *text, int alignment, int color);

/*
 * show and hide an osd
 */

void odk_show(odk_t *odk);
void odk_hide(odk_t *odk);

/*
 * clears osd
 */

void odk_clear(odk_t *odk);

/*
 * simple pixmaps
 */ 

uint8_t *odk_get_pixmap(int type);
void odk_draw_bitmap(odk_t *odk, uint8_t *bitmap, int x1, int y1, int width, int height,
		     uint8_t *palette_map);


/*
 * closes streams, osd and frees resources and odk object
 */

void odk_free(odk_t *odk);

/*
 * returns sizes of text if the text would have been drawn with current font
 */

void odk_get_text_size(odk_t *odk, const char *text, int *width, int *height);

/*
 * change current font
 */

void odk_set_font(odk_t *odk, const char *font, int size);

/*
 * allocates 10 colors from palette and returns the index to the first
 * color
 */

int odk_alloc_text_palette(odk_t *odk, uint32_t fg_color, uint8_t fg_trans,
                           uint32_t bg_color, uint8_t bg_trans,
                           uint32_t bo_color, uint8_t bo_trans);

/*
 * returns index to given color.
 */

int odk_get_color(odk_t *odk, uint32_t color, uint8_t trans);

/*
 * use these functions to register your callbacks for different events
 */

void odk_set_event_handler(odk_t *odk, int (*cb)(void *data, oxine_event_t *ev), void *data);

int odk_send_event(odk_t *odk, oxine_event_t *event);

/*
 * stops a running stream (if there is one),
 * opens a new stream, returns 0 on success.
 */

int odk_open_and_play(odk_t *odk, const char *mrl);
void odk_enqueue(odk_t *odk, const char *mrl);

/*
 * play, stop, pause, seek a stream
 */

void odk_play(odk_t *odk);
void odk_stop(odk_t *odk);
void odk_set_speed(odk_t *odk, uint32_t speed);
uint32_t odk_get_speed(odk_t *odk);
void odk_seek(odk_t *odk, int how);
void odk_eject(odk_t *odk);

/*
 * Stream info
 */ 

char *odk_get_meta_info(odk_t *odk, int info);
int odk_get_seek(odk_t *odk);    /* 1..100 */
int odk_get_pos_length(odk_t *odk, int *pos, int *time, int *length);
int odk_get_pos_length_high(odk_t *odk, int *pos, int *time, int *length);
const char *odk_get_mrl(odk_t *odk);

/*
 * error handling
 */

int odk_get_error(odk_t *odk);

void odk_user_color(odk_t *odk, const char *name, uint32_t *color, uint8_t *trans);

#endif
