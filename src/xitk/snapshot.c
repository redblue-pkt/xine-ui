/*
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
 * Image snapshot related stuff
 *
 * This feature was originally created for and is dedicated to
 *     Vicki Gynn <vicki@anvil.org>
 *
 * Copyright 2001 (c) Andrew Meredith <andrew@anvil.org>
 *
 * Creation Date: Wed 10 Oct 2001
 *
 *
 * The author wishes to express his gratitude to the following:
 *
 *  Guenter Bartsch <bartscgr@t-online.de> for the xine-lib access function.
 *
 *  Harm van der Heijden <harm@etpmod.phys.tue.nl> for spotting the author's
 *    brain crackingly stupid typo that broke the whole thing for a while.
 *    He also pointed out the yuy2toyv12() function.
 *
 *  Billy Biggs <vektor@dumbterm.net> for the YV12 colour conversion formula
 *    (see below)
 *
 *  James Courtier-Dutton <James@superbug.demon.co.uk> for educating the author
 *    in the details of YUV style data formats.
 *
 *  Thomas Östreich 
 *    for the yuy2toyv12() function (see below)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <pthread.h>

#include "xitk.h"
#include "event.h"
#include <xine/xineutils.h>

#include "xine.h"
#include <xine/xineutils.h>

#include "png.h"
#include "pngconf.h"

#define PIXSZ 3
#define BIT_DEPTH 8

extern gGui_t  *gGui;

/* internal function use to scale yuv data */
typedef void (*scale_line_func_t) (uint8_t *source, uint8_t *dest,
                                   int width, int step);

/* Holdall structure */

struct prvt_image_s {
  int width;
  int height;
  int ratio_code;
  int format;
  uint8_t *y, *u, *v, *yuy2;

  int u_width, v_width;
  int u_height, v_height;

  png_bytepp rgb;

  scale_line_func_t scale_line;
  unsigned long scale_factor;

  FILE *fp;
  char *file_name;
  png_structp struct_ptr;
  png_infop info_ptr;
};

/*
 *  This function was pinched from filter_yuy2tov12.c, part of
 *  transcode, a linux video stream processing tool
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  Thanks Thomas
 *      
 */
static void yuy2toyv12( struct prvt_image_s *image )
{

    int i,j,w2;

    /* I420 */
    uint8_t *y = image->y;
    uint8_t *u = image->u;
    uint8_t *v = image->v;

    uint8_t *input = image->yuy2;
    
    int width  = image->width;
    int height = image->height;

    w2 = width/2;

    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	*(y++) = *(input++);
	*(u++) = *(input++);
	*(y++) = *(input++);
	*(v++) = *(input++);
      }
      
      /* down sampling */
      
      for (j=0; j<w2; j++) {
	/* skip every second line for U and V */
	*(y++) = *(input++);
	input++;
	*(y++) = *(input++);
	input++;
      }
    }
}

/*
 *   Function to construct image filename
 *
 *   Note .. static elements prevent
 *   filename clashes on fast repeats.
 *
 *   With thanks to the xawtv project
 *   from where it was pinched.
 */

char* snap_filename(char *base, char *ext)
{
    static time_t last = 0;
    static int count = 0;
    static char *filename = NULL;
    
    time_t now;
    struct tm* tm;
    char timestamp[32];
    
    time(&now);
    tm = localtime(&now);
    
    if (last != now)
	count = 0;
    last = now;
    count++;
    
    if (filename != NULL)
	free(filename);	

    filename = (char *) xine_xmalloc(strlen(base) + strlen(ext) + 32);
    
    strftime(timestamp,31,"%Y%m%d-%H%M%S",tm);
    sprintf(filename,"%s-%s-%d.%s",
	    base,timestamp,count,ext);
    return filename;
}

/*
 * Scale line with no horizontal scaling. For NTSC mpeg2 dvd input in
 * 4:3 output format (720x480 -> 720x540)
 */
static void scale_line_1_1 (uint8_t *source, uint8_t *dest,
			    int width, int step) {

  memcpy(dest, source, width);
}

/*
 * Interpolates 64 output pixels from 45 source pixels using shifts.
 * Useful for scaling a PAL mpeg2 dvd input source to 1024x768
 * fullscreen resolution, or to 16:9 format on a monitor using square
 * pixels.
 * (720 x 576 ==> 1024 x 576)
 */
static void scale_line_45_64 (uint8_t *source, uint8_t *dest,
			     int width, int step) {

  int p1, p2;

  while ((width -= 64) >= 0) {
    p1 = source[0];
    p2 = source[1];
    dest[0] = p1;
    dest[1] = (1*p1 + 3*p2) >> 2;
    p1 = source[2];
    dest[2] = (5*p2 + 3*p1) >> 3;
    p2 = source[3];
    dest[3] = (7*p1 + 1*p2) >> 3;
    dest[4] = (1*p1 + 3*p2) >> 2;
    p1 = source[4];
    dest[5] = (1*p2 + 1*p1) >> 1;
    p2 = source[5];
    dest[6] = (3*p1 + 1*p2) >> 2;
    dest[7] = (1*p1 + 7*p2) >> 3;
    p1 = source[6];
    dest[8] = (3*p2 + 5*p1) >> 3;
    p2 = source[7];
    dest[9] = (5*p1 + 3*p2) >> 3;
    p1 = source[8];
    dest[10] = p2;
    dest[11] = (1*p2 + 3*p1) >> 2;
    p2 = source[9];
    dest[12] = (5*p1 + 3*p2) >> 3;
    p1 = source[10];
    dest[13] = (7*p2 + 1*p1) >> 3;
    dest[14] = (1*p2 + 7*p1) >> 3;
    p2 = source[11];
    dest[15] = (1*p1 + 1*p2) >> 1;
    p1 = source[12];
    dest[16] = (3*p2 + 1*p1) >> 2;
    dest[17] = p1;
    p2 = source[13];
    dest[18] = (3*p1 + 5*p2) >> 3;
    p1 = source[14];
    dest[19] = (5*p2 + 3*p1) >> 3;
    p2 = source[15];
    dest[20] = p1;
    dest[21] = (1*p1 + 3*p2) >> 2;
    p1 = source[16];
    dest[22] = (1*p2 + 1*p1) >> 1;
    p2 = source[17];
    dest[23] = (7*p1 + 1*p2) >> 3;
    dest[24] = (1*p1 + 7*p2) >> 3;
    p1 = source[18];
    dest[25] = (3*p2 + 5*p1) >> 3;
    p2 = source[19];
    dest[26] = (3*p1 + 1*p2) >> 2;
    dest[27] = p2;
    p1 = source[20];
    dest[28] = (3*p2 + 5*p1) >> 3;
    p2 = source[21];
    dest[29] = (5*p1 + 3*p2) >> 3;
    p1 = source[22];
    dest[30] = (7*p2 + 1*p1) >> 3;
    dest[31] = (1*p2 + 3*p1) >> 2;
    p2 = source[23];
    dest[32] = (1*p1 + 1*p2) >> 1;
    p1 = source[24];
    dest[33] = (3*p2 + 1*p1) >> 2;
    dest[34] = (1*p2 + 7*p1) >> 3;
    p2 = source[25];
    dest[35] = (3*p1 + 5*p2) >> 3;
    p1 = source[26];
    dest[36] = (3*p2 + 1*p1) >> 2;
    p2 = source[27];
    dest[37] = p1;
    dest[38] = (1*p1 + 3*p2) >> 2;
    p1 = source[28];
    dest[39] = (5*p2 + 3*p1) >> 3;
    p2 = source[29];
    dest[40] = (7*p1 + 1*p2) >> 3;
    dest[41] = (1*p1 + 7*p2) >> 3;
    p1 = source[30];
    dest[42] = (1*p2 + 1*p1) >> 1;
    p2 = source[31];
    dest[43] = (3*p1 + 1*p2) >> 2;
    dest[44] = (1*p1 + 7*p2) >> 3;
    p1 = source[32];
    dest[45] = (3*p2 + 5*p1) >> 3;
    p2 = source[33];
    dest[46] = (5*p1 + 3*p2) >> 3;
    p1 = source[34];
    dest[47] = p2;
    dest[48] = (1*p2 + 3*p1) >> 2;
    p2 = source[35];
    dest[49] = (1*p1 + 1*p2) >> 1;
    p1 = source[36];
    dest[50] = (7*p2 + 1*p1) >> 3;
    dest[51] = (1*p2 + 7*p1) >> 3;
    p2 = source[37];
    dest[52] = (1*p1 + 1*p2) >> 1;
    p1 = source[38];
    dest[53] = (3*p2 + 1*p1) >> 2;
    dest[54] = p1;
    p2 = source[39];
    dest[55] = (3*p1 + 5*p2) >> 3;
    p1 = source[40];
    dest[56] = (5*p2 + 3*p1) >> 3;
    p2 = source[41];
    dest[57] = (7*p1 + 1*p2) >> 3;
    dest[58] = (1*p1 + 3*p2) >> 2;
    p1 = source[42];
    dest[59] = (1*p2 + 1*p1) >> 1;
    p2 = source[43];
    dest[60] = (7*p1 + 1*p2) >> 3;
    dest[61] = (1*p1 + 7*p2) >> 3;
    p1 = source[44];
    dest[62] = (3*p2 + 5*p1) >> 3;
    p2 = source[45];
    dest[63] = (3*p1 + 1*p2) >> 2;
    source += 45;
    dest += 64;
  }

  if ((width += 64) <= 0) goto done;
  *dest++ = source[0];
  if (--width <= 0) goto done;
  *dest++ = (1*source[0] + 3*source[1]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (5*source[1] + 3*source[2]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[2] + 1*source[3]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[2] + 3*source[3]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[3] + 1*source[4]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (3*source[4] + 1*source[5]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[4] + 7*source[5]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[5] + 5*source[6]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[6] + 3*source[7]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = source[7];
  if (--width <= 0) goto done;
  *dest++ = (1*source[7] + 3*source[8]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (5*source[8] + 3*source[9]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[9] + 1*source[10]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[9] + 7*source[10]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[10] + 1*source[11]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (3*source[11] + 1*source[12]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = source[12];
  if (--width <= 0) goto done;
  *dest++ = (3*source[12] + 5*source[13]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[13] + 3*source[14]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = source[14];
  if (--width <= 0) goto done;
  *dest++ = (1*source[14] + 3*source[15]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[15] + 1*source[16]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (7*source[16] + 1*source[17]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[16] + 7*source[17]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[17] + 5*source[18]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[18] + 1*source[19]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = source[19];
  if (--width <= 0) goto done;
  *dest++ = (3*source[19] + 5*source[20]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[20] + 3*source[21]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[21] + 1*source[22]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[21] + 3*source[22]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[22] + 1*source[23]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (3*source[23] + 1*source[24]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[23] + 7*source[24]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[24] + 5*source[25]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[25] + 1*source[26]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = source[26];
  if (--width <= 0) goto done;
  *dest++ = (1*source[26] + 3*source[27]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (5*source[27] + 3*source[28]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[28] + 1*source[29]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[28] + 7*source[29]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[29] + 1*source[30]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (3*source[30] + 1*source[31]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[30] + 7*source[31]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[31] + 5*source[32]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[32] + 3*source[33]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = source[33];
  if (--width <= 0) goto done;
  *dest++ = (1*source[33] + 3*source[34]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[34] + 1*source[35]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (7*source[35] + 1*source[36]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[35] + 7*source[36]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[36] + 1*source[37]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (3*source[37] + 1*source[38]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = source[38];
  if (--width <= 0) goto done;
  *dest++ = (3*source[38] + 5*source[39]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[39] + 3*source[40]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[40] + 1*source[41]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[40] + 3*source[41]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[41] + 1*source[42]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (7*source[42] + 1*source[43]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[42] + 7*source[43]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[43] + 5*source[44]) >> 3;
 done: ;

}

/*
 * Interpolates 16 output pixels from 15 source pixels using shifts.
 * Useful for scaling a PAL mpeg2 dvd input source to 4:3 format on
 * a monitor using square pixels.
 * (720 x 576 ==> 768 x 576)
 */
static void scale_line_15_16 (uint8_t *source, uint8_t *dest,
			      int width, int step) {

  int p1, p2;

  while ((width -= 16) >= 0) {
    p1 = source[0];
    dest[0] = p1;
    p2 = source[1];
    dest[1] = (1*p1 + 7*p2) >> 3;
    p1 = source[2];
    dest[2] = (1*p2 + 7*p1) >> 3;
    p2 = source[3];
    dest[3] = (1*p1 + 3*p2) >> 2;
    p1 = source[4];
    dest[4] = (1*p2 + 3*p1) >> 2;
    p2 = source[5];
    dest[5] = (3*p1 + 5*p2) >> 3;
    p1 = source[6];
    dest[6] = (3*p2 + 5*p1) >> 3;
    p2 = source[7];
    dest[7] = (1*p1 + 1*p1) >> 1;
    p1 = source[8];
    dest[8] = (1*p2 + 1*p1) >> 1;
    p2 = source[9];
    dest[9] = (5*p1 + 3*p2) >> 3;
    p1 = source[10];
    dest[10] = (5*p2 + 3*p1) >> 3;
    p2 = source[11];
    dest[11] = (3*p1 + 1*p2) >> 2;
    p1 = source[12];
    dest[12] = (3*p2 + 1*p1) >> 2;
    p2 = source[13];
    dest[13] = (7*p1 + 1*p2) >> 3;
    p1 = source[14];
    dest[14] = (7*p2 + 1*p1) >> 3;
    dest[15] = p1;
    source += 15;
    dest += 16;
  }

  if ((width += 16) <= 0) goto done;
  *dest++ = source[0];
  if (--width <= 0) goto done;
  *dest++ = (1*source[0] + 7*source[1]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[1] + 7*source[2]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[2] + 3*source[3]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (1*source[3] + 3*source[4]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (3*source[4] + 5*source[5]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[5] + 5*source[6]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (1*source[6] + 1*source[7]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (1*source[7] + 1*source[8]) >> 1;
  if (--width <= 0) goto done;
  *dest++ = (5*source[8] + 3*source[9]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (5*source[9] + 3*source[10]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (3*source[10] + 1*source[11]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (3*source[11] + 1*source[12]) >> 2;
  if (--width <= 0) goto done;
  *dest++ = (7*source[12] + 1*source[13]) >> 3;
  if (--width <= 0) goto done;
  *dest++ = (7*source[13] + 1*source[14]) >> 3;
 done: ;
}

int scale_image( struct prvt_image_s *image )
{
  int i;

  int step = 1; /* unused variable for the scale functions */

  /* pointers for post-scaled line buffer */
  uint8_t *n_y = 0;
  uint8_t *n_u = 0;
  uint8_t *n_v = 0;

  /* pointers into pre-scaled line buffers */
  uint8_t *oy_p = image->y;
  uint8_t *ou_p = image->u;
  uint8_t *ov_p = image->v;

  /* pointers into post-scaled line buffers */
  uint8_t *ny_p = 0;
  uint8_t *nu_p = 0;
  uint8_t *nv_p = 0;

  /* old line widths */
  int oy_width = image->width;
  int ou_width = image->u_width;
  int ov_width = image->v_width;

  /* new line widths NB scale factor is factored by 32768 for rounding */
  int ny_width = (oy_width * image->scale_factor) / 32768;
  int nu_width = (ou_width * image->scale_factor) / 32768;
  int nv_width = (ov_width * image->scale_factor) / 32768;

  /* allocate new buffer space space for post-scaled line buffers */
  n_y = (uint8_t*)png_malloc( image->struct_ptr, ny_width * image->height );
  if (n_y == 0) return( 0 );
  n_u = (uint8_t*)png_malloc( image->struct_ptr, nu_width * image->u_height );
  if (n_u == 0) return( 0 );
  n_v = (uint8_t*)png_malloc( image->struct_ptr, nv_width * image->v_height );
  if (n_v == 0) return( 0 );

  /* set post-scaled line buffer progress pointers */
  ny_p = n_y;
  nu_p = n_u;
  nv_p = n_v;

  /* Do the scaling */

  for ( i=0; i<image->height; ++i ) {
    image->scale_line( oy_p, ny_p, ny_width, step );
    oy_p += oy_width;
    ny_p += ny_width;
  }

  for ( i=0; i<image->u_height; ++i ) {
    image->scale_line( ou_p, nu_p, nu_width, step );
    ou_p += ou_width;
    nu_p += nu_width;
  }

  for ( i=0; i<image->v_height; ++i ) {
    image->scale_line( ov_p, nv_p, nv_width, step );
    ov_p += ov_width;
    nv_p += nv_width;
  }

  /* switch to post-scaled data and widths */
  image->y = n_y;
  image->u = n_u;
  image->v = n_v;
  image->width = ny_width;
  image->u_width = nu_width;
  image->v_width = nv_width;

  printf("  Post scaled\n    Width %d %d %d\n    Height %d %d %d\n",
    image->width, image->u_width, image->v_width,
    image->height, image->u_height, image->v_height );

  return( 1 );
}

/*
 *  This function is a fudge .. a hack.
 *
 *  It is in place purely to get snapshots going for YUY2 data
 *  longer term there needs to be a bit of a reshuffle to account
 *  for the two fundamentally different YUV formats. Things would
 *  have been different had I known how YUY2 was done before designing
 *  the flow. Teach me to make assumptions I guess.
 *
 *  So .. this function converts the YUY2 image to YV12. The downside
 *  being that as YV12 has half as many chroma rows as YUY2, there is
 *  a loss of image quality.
 */

static int yuy2_fudge( struct prvt_image_s *image )
{
  image->yuy2 = image->y;

  image->y = png_malloc( image->struct_ptr, image->height*image->width );
  if ( image->y == NULL ) return( 0 );
  memset( image->y, 0, image->height*image->width );

  image->u = png_malloc( image->struct_ptr, image->u_height*image->u_width );
  if ( image->u == NULL ) return( 0 );
  memset( image->u, 0, image->u_height*image->u_width );

  image->v = png_malloc( image->struct_ptr, image->v_height*image->v_width );
  if ( image->v == NULL ) return( 0 );
  memset( image->v, 0, image->v_height*image->v_width );

  yuy2toyv12( image );

  image->yuy2 = NULL;

  return( 1 );
}

/*
 *  RGB allocation
 */

static int rgb_alloc( struct prvt_image_s *image )
{
  int i;

  image->rgb = png_malloc( image->struct_ptr, image->height*sizeof(png_bytep));
  if ( image->rgb == NULL ) return( 0 );

  memset( image->rgb, 0, image->height );

  for ( i=0; i<image->height; i++) {
    image->rgb[i]=png_malloc( image->struct_ptr, image->width*PIXSZ);
    if ( image->rgb[i] == NULL ) return( 0 );
    memset( image->rgb[i], 0, image->width * PIXSZ );
  }

  return( 1 );
}

static void rgb_free( struct prvt_image_s *image )
{
  int i;

  if (image->rgb == 0) return;

  for ( i=0; i<image->height; ++i ) {
    if (image->rgb[i]) png_free ( image->struct_ptr, image->rgb[i]);
    image->rgb[i] = NULL;
  }

  if (image->rgb) png_free(  image->struct_ptr, image->rgb );
  image->rgb = 0;
}

/*
 *  Handler functions for image structure
 */

static int prvt_image_alloc( struct prvt_image_s **image )
{
  char *filename;

  *image = (struct prvt_image_s*)malloc( sizeof( struct prvt_image_s ) );

  if (*image == NULL) return( 0 );
    
  memset( *image, 0, sizeof( struct prvt_image_s ) );

  filename = (char *) alloca(strlen(xine_get_homedir()) + 10);
  sprintf(filename, "%s/%s", xine_get_homedir(), "xinesnap");

  (*image)->file_name = snap_filename( filename, "png" );

  return( 1 );
}

static void prvt_image_free( struct prvt_image_s **image )
{
  struct prvt_image_s *image_p = *image;

  /*
   * Do not free image_p->file_name
   *   It looks after itself
   */

  rgb_free ( image_p );

  if (image_p->info_ptr)   png_destroy_info_struct (  image_p->struct_ptr, &image_p->info_ptr );
  if (image_p->struct_ptr) png_destroy_write_struct( &image_p->struct_ptr, (png_infopp)NULL );
  if (image_p->fp)         fclose( image_p->fp );
}

static int clip_8_bit( int val )
{
	if (val < 0) {
	  val = 0;
	}
	else {
	  if (val > 255) val = 255;
	}
	return( val );
}

/*
 *   Create rgb data from yv12
 */
static void yv12_2_rgb( struct prvt_image_s *image )
{
  int i, j;

  int y, u, v;
  int r, g, b;

  int sub_i_u;
  int sub_i_v;

  int sub_j_u;
  int sub_j_v;

  for ( i=0; i<image->height; ++i )
  {
    /* calculate u & v rows */
    sub_i_u = ((i * image->u_height) / image->height );
    sub_i_v = ((i * image->v_height) / image->height );

    for ( j=0; j<image->width; ++j )
    {
      /* calculate u & v columns */
      sub_j_u = ((j * image->u_width) / image->width );
      sub_j_v = ((j * image->v_width) / image->width );

/***************************************************
 *
 *  Colour conversion from 
 *    http://www.inforamp.net/~poynton/notes/colour_and_gamma/ColorFAQ.html#RTFToC30
 *
 *  Thanks to Billy Biggs <vektor@dumbterm.net>
 *  for the pointer and the following conversion.
 *
 *   R' = [ 1.1644         0    1.5960 ]   ([ Y' ]   [  16 ])
 *   G' = [ 1.1644   -0.3918   -0.8130 ] * ([ Cb ] - [ 128 ])
 *   B' = [ 1.1644    2.0172         0 ]   ([ Cr ]   [ 128 ])
 *
 *  Where in Xine the above values are represented as
 *
 *   Y' == image->y
 *   Cb == image->u
 *   Cr == image->v
 *
 ***************************************************/

      y = image->y[ (i*image->width) + j ] - 16;
      u = image->u[ (sub_i_u*image->u_width) + sub_j_u ] - 128;
      v = image->v[ (sub_i_v*image->v_width) + sub_j_v ] - 128;

      r = (1.1644 * y)                + (1.5960 * v);
      g = (1.1644 * y) - (0.3918 * u) - (0.8130 * v);
      b = (1.1644 * y) + (2.0172 * u);

      r = clip_8_bit( r );
      g = clip_8_bit( g );
      b = clip_8_bit( b );

      image->rgb[i][ (j * PIXSZ) + 0 ] = r;
      image->rgb[i][ (j * PIXSZ) + 1 ] = g;
      image->rgb[i][ (j * PIXSZ) + 2 ] = b;
    }
  }
}

/*
 *   Error functions for use as callbacks by the png libraries
 */

static void user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
  xine_error("Error: %s\n", error_msg);
}

static void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
  xine_error("Warning: %s\n", warning_msg);
}

/*
 *
 */
static void write_row_callback( png_structp png_ptr, png_uint_32 row, int pass)
{
}

/*
 *  External function
 */

void create_snapshot ( gGui_t *gGui )
{
  int err = 0;
  struct prvt_image_s *image;

#ifdef DEBUG
  static int	   prof_scale_image = -1;
  static int	   prof_yuv2rgb     = -1;
  static int	   prof_png         = -1;

  if (prof_scale_image == -1)
    prof_scale_image = xine_profiler_allocate_slot ("snapshot scale image");
  if (prof_yuv2rgb == -1)
    prof_yuv2rgb = xine_profiler_allocate_slot ("snapshot yuv to rgb");
  if (prof_png == -1)
    prof_png = xine_profiler_allocate_slot ("snapshot convert to png");
#endif /* DEBUG */

  if ( ! prvt_image_alloc( &image ) )
  {
    printf("  prvt_image_alloc failed\n");
    return;
  }

  err = xine_get_current_frame( gGui->xine,
          &image->width, &image->height, &image->ratio_code,
          &image->format, &image->y, &image->u, &image->v);

  if (err == 0) {
    printf("   Framegrabber failed\n");
    prvt_image_free( &image );
    return;
  }

  /* the dxr3 driver does not allocate yuv buffers */
  if (! image->y) { /* image->u and image->v are always 0 for YUY2 */
    printf("snapshot.c: not supported for this video out driver\n");
    prvt_image_free( &image );
    return;
  }

  printf("  width:  %d\n", image->width );
  printf("  height: %d\n", image->height );
  printf("  ratio:  " );

  switch ( image->ratio_code ) {
    case XINE_ASPECT_RATIO_SQUARE:
      printf( "XINE_ASPECT_RATIO_SQUARE\n" ); 
      image->scale_line = scale_line_1_1;
      image->scale_factor = ( 32768 * 1 ) / 1;
      break;

    case XINE_ASPECT_RATIO_4_3:
      printf( "XINE_ASPECT_RATIO_4_3\n" ); 
      image->scale_line = scale_line_15_16;
      image->scale_factor = ( 32768 * 16 ) / 15;
      break;

    case XINE_ASPECT_RATIO_ANAMORPHIC: 
      printf( "XINE_ASPECT_RATIO_ANAMORPHIC\n" ); 
      image->scale_line = scale_line_45_64;
      image->scale_factor = ( 32768 * 64 ) / 45;
      break;

    case XINE_ASPECT_RATIO_211_1:      
      printf( "XINE_ASPECT_RATIO_211_1\n" ); 
      image->scale_line = scale_line_45_64;
      image->scale_factor = ( 32768 * 64 ) / 45;
      break;

    case XINE_ASPECT_RATIO_DONT_TOUCH: 
      printf( "XINE_ASPECT_RATIO_DONT_TOUCH\n" ); 
      image->scale_line = scale_line_1_1;
      image->scale_factor = ( 32768 * 1 ) / 1;
      break;
    default:                
      /* the mpeg standard has a few that we don't know about */
      printf( "snapshot.c: warning: unknown aspect ratio. will assume 1:1\n" ); 
      image->scale_line = scale_line_1_1;
      image->scale_factor = ( 32768 * 1 ) / 1;
      break;
  }

  printf("  format: " );
  
  switch ( image->format ) {
    case XINE_IMGFMT_YV12:
      printf( "XINE_IMGFMT_YV12\n" ); 
      image->u_width  = ((image->width+1)/2);
      image->v_width  = ((image->width+1)/2);
      image->u_height = ((image->height+1)/2);
      image->v_height = ((image->height+1)/2);
      break;

    case XINE_IMGFMT_YUY2: 
      printf( "XINE_IMGFMT_YUY2\n" );
      image->u_width  = ((image->width+1)/2);
      image->v_width  = ((image->width+1)/2);
      image->u_height = ((image->height+1)/2);
      image->v_height = ((image->height+1)/2);
      break;

    default:                
      printf( "Unknown\nError: Format Code %d Unknown\n", image->format ); 
      printf( "  ** Please report this error to andrew@anvil.org **\n" );
      prvt_image_free( &image );
      return;
  }

  /**/

  if ( (image->fp = fopen(image->file_name, "wb")) == NULL ) {
    xine_error(_("File open failed (%s)\n"), image->file_name);
    prvt_image_free( &image );
    return;
  }

  /**/

  printf("  Setup png_write_struct\n" );

  image->struct_ptr = png_create_write_struct (
    PNG_LIBPNG_VER_STRING,
    (png_voidp)0,
    user_error_fn,
    user_warning_fn);

  if (image->struct_ptr == NULL) {
    printf( "  png_create_write_struct() failed\n" );
    prvt_image_free( &image );
    return;
  }

  /**/

  printf("  Setup png_info_struct\n" );

  image->info_ptr = png_create_info_struct(image->struct_ptr);

  if (image->info_ptr == NULL) {
    printf( "  png_create_info_struct() failed\n" );
    prvt_image_free( &image );
    return;
  }

#ifdef PNG_SETJMP_SUPPORTED	/* libpng 1.0.5 has no png_jmpbuf */
  /*
   *  Set up long jump callback for PNG parser
   */

  printf("  Setup long jump\n" );
#if defined(PNG_INFO_IMAGE_SUPPORTED)
  if (setjmp(png_jmpbuf(image->struct_ptr)))
#else
  if (setjmp(image->struct_ptr->jmpbuf))
#endif
  {
    printf( "  PNG Parsing failed\n" );
    prvt_image_free( &image );
    return;
  }
#endif

  /*
   *  If YUY2 convert to YV12
   */
  if ( image->format == XINE_IMGFMT_YUY2 ) {
    printf("  Convert YUY2 to YV12\n" );
    if ( yuy2_fudge( image ) == 0 ) {
      printf("  Error: yuy2_fudge failed\n");
      return;
    }
  }

  /*
   *  Scale YUV data
   */
  printf("  Scale YUV Image data\n" );

#ifdef DEBUG
  xine_profiler_start_count (prof_scale_image);
#endif /* DEBUG */

  scale_image ( image );

#ifdef DEBUG
  xine_profiler_stop_count (prof_scale_image);
#endif /* DEBUG */

  /*
   *  Allocate RGB data structures within image
   */
  printf("  Allocate RGB data space\n" );

  if ( !rgb_alloc( image ) )
  {
    printf( "  rgb_alloc() failed\n" );
    prvt_image_free( &image );
    return;
  }

  /*
   *  Move data from yuv to rgb
   */
  printf("  Reformat YUV Image data to RGB\n" );

#ifdef DEBUG
  xine_profiler_start_count (prof_yuv2rgb);
#endif /* DEBUG */

  yv12_2_rgb( image );

#ifdef DEBUG
  xine_profiler_stop_count (prof_yuv2rgb);
#endif /* DEBUG */

  /**/

  printf("  png_set_filter\n" );

#ifdef DEBUG
  xine_profiler_start_count (prof_png);
#endif /* DEBUG */

  png_set_filter( image->struct_ptr, 0, PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE );

  printf("  png_init_io\n" );
  png_init_io(image->struct_ptr, image->fp);
  
  printf("  png_set_write_status_fn\n" );
  png_set_write_status_fn(image->struct_ptr, write_row_callback);

  /**/

  printf("  png_set_IHDR\n" );
  png_set_IHDR( 
    image->struct_ptr, 
    image->info_ptr, 
    image->width, 
    image->height,
    BIT_DEPTH, 
    PNG_COLOR_TYPE_RGB, 
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT, 
    PNG_FILTER_TYPE_DEFAULT);

  /**/

#if defined(PNG_INFO_IMAGE_SUPPORTED)
  printf("  png_set_rows\n" );
  png_set_rows ( image->struct_ptr, image->info_ptr, image->rgb );
  printf("  png_write_png\n" );
  png_write_png( image->struct_ptr, image->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
#else
  png_write_info(image->struct_ptr, image->info_ptr);
  png_write_image(image->struct_ptr, image->rgb);
  png_write_end(image->struct_ptr, NULL);
#endif

  /**/
  xine_info(_("File '%s' written.\n"), image->file_name);

  printf("  prvt_image_free\n" );
  prvt_image_free( &image );

#ifdef DEBUG
  xine_profiler_stop_count (prof_png);
#endif /* DEBUG */

  return;
}
