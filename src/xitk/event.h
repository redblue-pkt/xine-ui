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
 */

#ifndef EVENT_H
#define EVENT_H

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"
#include "xine.h"
#include "xitk.h"

#ifdef __GNUC__
#define perr(FMT,ARGS...) {fprintf(stderr, FMT, ##ARGS);fflush(stderr);}
#else	/* C99 version: */
#define perr(...)	  {fprintf(stderr, __VA_ARGS__);fflush(stderr);}
#endif

#define MAX_PLAYLIST_LENGTH  1024

typedef struct {
  /* xine engine configuration */
  config_values_t     *config;
  
  /* our video out driver */
  vo_driver_t         *vo_driver;

  /* xine engine instance */
  xine_t              *xine;

  /* xine lib/gui configuration filename */
  char                *configfile;

  /* stuff like FULL_ON_START, QUIT_ON_STOP */
  int                  autoplay_options;

  /* basic X11 stuff */
  Display             *display;
  int                  screen;
  XColor               black;
  Pixmap               icon;

  xitk_dnd_t           xdnd;

  ImlibData           *imlib_data;

  Window               video_window; 
  ImlibImage          *video_window_logo_image;
  gui_image_t          video_window_logo_pixmap;
  int                  cursor_visible;

  Window               panel_window;

  uint32_t             debug_level;

  char                 filename[1024];

  /* gui playlist */
  char                *playlist[MAX_PLAYLIST_LENGTH];
  int                  playlist_num;
  int                  playlist_cur;

  int                  running;
  int                  ignore_status;

#ifdef HAVE_LIRC
  int                  lirc_enable;
#endif

  widgetkey_t          widget_key;

} gGui_t;


char *gui_get_skindir(const char file[]);

/*
 * flags for autoplay options
 */

#define PLAY_ON_START      0x00000001
#define PLAYED_ON_START    0x00000002
#define QUIT_ON_STOP       0x00000004
#define FULL_ON_START      0x00000008
#define HIDEGUI_ON_START   0x00000010
#define PLAY_FROM_DVD      0x00000020
#define PLAY_FROM_VCD      0x00000040


/*
 * Configuration file lookup/set functions
 */
char *config_lookup_str(char *, char *);
int config_lookup_int(char *, int);
void config_set_str(char *, char *);
void config_set_int(char *, int);
void config_save(void);
void config_reset(void);


void gui_init (int nfiles, char *filenames[]);

void gui_run ();

void gui_status_callback (int nStatus) ;

void gui_dndcallback (char *filename) ;

void gui_handle_event (XEvent *event, void *data);

char *gui_next_mrl_callback () ;

void gui_branched_callback () ;

#endif


