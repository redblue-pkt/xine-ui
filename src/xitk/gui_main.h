/* 
 * Copyright (C) 2000 the xine project
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

#ifndef GUI_MAIN_H
#define GUI_MAIN_H

#include <Imlib.h>
#include <X11/Xlib.h>
#ifdef HAVE_LIRC
#include "lirc/lirc_client.h"
#endif
#include "xine.h"
#include "gui_widget.h"
#include "gui_dnd.h"

#define perr(FMT,ARGS...) fprintf(stderr, FMT, ##ARGS);fflush(stderr)

#define MAX_PLAYLIST_LENGTH  1024

typedef struct {
  /* Xine lib engine configuration */
  config_values_t     *gConfig;
  
  /* Xine lib instance */
  xine_t              *gXine;

  /* Xine lib/gui configuration filename */
  char                *gConfigFilename;

  /* stuff like FULL_ON_START, QUIT_ON_STOP */
  int                  autoplay_options;

  Display             *gDisplay;

  Window               gVideoWin; /* video output window */

  Window               gui_panel_win;
  DND_struct_t        *xdnd_panel_win;

  uint32_t             debug_level;

  ImlibData           *gImlib_data;
  ImlibImage          *gui_bg_image;     /* background image */

#ifdef HAVE_LIRC
  struct lirc_config  *xlirc_config;
  static int           lirc_fd;
  static int           lirc_enable = 0;
  static pthread_t     lirc_thread;
#endif

  char                 gui_filename[1024];

  /* gui playlist */
  char                *gui_playlist[MAX_PLAYLIST_LENGTH];
  int                  gui_playlist_num;
  int                  gui_playlist_cur;



} gGlob_t;


char *gui_get_skindir(const char file[]);

typedef struct gui_move_s {
    int enabled;
    int offset_x;
    int offset_y;
} gui_move_t;

typedef struct gui_color_s {
    XColor red;
    XColor blue;
    XColor green;
    XColor white;
    XColor black;
    XColor tmp;
} gui_color_t;
gui_color_t gui_color;

typedef struct gui_image_s {
  Pixmap image;
  int width;
  int height;
} gui_image_t;

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct _mwmhints {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t  input_mode;
  uint32_t status;
} MWMHints;

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


gui_image_t *gui_load_image(const char *image);

void gui_start (int nfiles, char *filenames[]);

void gui_status_callback (int nStatus) ;

void gui_dndcallback (char *filename) ;

void gui_set_current_mrl(const char *mrl);

void gui_play(widget_t *, void *);

void gui_stop (widget_t *, void *);

int  is_gui_panel_visible(void);

/*
 * seamless branching support:
 */

char *gui_get_next_mrl (void);

void gui_notify_demux_branched (void);

#endif

