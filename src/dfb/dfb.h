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
 * DirectFB output
 * Rich Wareham <richwareham@users.sourceforge.net>
 *
 */

/*** Common #includes and #defines ***/

#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <directfb.h>       

#include <xine.h>
#include <xine/xineutils.h>

#define FONT FONTDIR "/cetus.ttf"
#define POINTER FONTDIR "/pointer2.png"

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif
#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }

#ifndef __DFB_H
#define __DFB_H

/*** types ***/

typedef struct {
  xine_t                  *xine;
  int                      ignore_status;
  const xine_vo_driver_t  *vo_driver;
  const xine_ao_driver_t  *ao_driver;
  char                    *mrl[1024];
  int                      num_mrls;
  int                      current_mrl;
  int                      running;
  int                      auto_quit;

  char                    *audio_driver_id;
  char                    *video_driver_id;
  int                      audio_channel;

  IDirectFB               *dfb;
  IDirectFBDisplayLayer   *layer;
  IDirectFBDisplayLayer   *video_layer;
  IDirectFBSurface        *primary;
  IDirectFBSurface        *bg_surface;
  IDirectFBWindow         *main_window;
  IDirectFBSurface        *main_surface;
  IDirectFBSurface        *pointer;

  IDirectFBFont           *font;
  int                      fontheight;
  DFBDisplayLayerConfig    layer_config;
 
  int                      screen_width;
  int                      screen_height;

  struct {
    int                    enable;
    int                    caps;
    int                    volume_mixer;
    int                    volume_level;
    int                    mute;
  } mixer;

  int                      debug_messages;
} dfbxine_t;

typedef struct {
  IDirectFB *dfb;
  IDirectFBDisplayLayer *layer;
} dfb_visual_info_t;

/*** Global variables ***/

extern dfbxine_t dfbxine; /* Defined in main.c */

/*** functions ***/

/* Handle command line params. Returns 0 on failure */
int do_command_line(int argc, char **argv);

/*** initialise all the DirectFB stuff Returns 0 on failure ***/
int init_dfb();

#endif /* __DFB_H */

