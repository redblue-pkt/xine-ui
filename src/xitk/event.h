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
#include <xine.h>

#include "Imlib-light/Imlib.h"
#include "kbindings.h"
#include "xitk.h"
#include "i18n.h"

#ifdef __GNUC__
#define perr(FMT,ARGS...) {fprintf(stderr, FMT, ##ARGS);fflush(stderr);}
#else	/* C99 version: */
#define perr(...)	  {fprintf(stderr, __VA_ARGS__);fflush(stderr);}
#endif

#ifdef __GNUC__
#define xine_error(FMT,ARGS...) {xitk_window_dialog_error(gGui->imlib_data, FMT, ##ARGS);sleep(15);}
#define xine_info(FMT,ARGS...)  xitk_window_dialog_info(gGui->imlib_data, FMT, ##ARGS)
#else	/* C99 version: */
#define xine_error(...)	        xitk_window_dialog_error(gGui->imlib_data, __VA_ARGS__)
#define xine_info(FMT,ARGS...)  xitk_window_dialog_info(gGui->imlib_data,  __VA_ARGS__)
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
  action_id_t          actions_on_start[16];
  char                *autoscan_plugin;

  /* basic X11 stuff */
  Display             *display;
  int                  screen;
  int		       depth;
  Visual	      *visual;
  XColor               black;
  Pixmap               icon;

  int		       prefered_visual_class;
  VisualID	       prefered_visual_id;
  int		       install_colormap;

  xitk_skin_config_t  *skin_config;
  xitk_dnd_t           xdnd;

  ImlibData           *imlib_data;

  Window               video_window; 
  ImlibImage          *video_window_logo_image;
  xitk_image_t         video_window_logo_pixmap;
  int                  cursor_visible;
  int                  cursor_grabbed;

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

#ifdef HAVE_XF86VIDMODE
  int                  XF86VidMode_fullscreen;
#endif

  struct {
    int                caps;
    int                volume_mixer;
    int                volume_level;
    int                mute;
  } mixer;

  xitk_register_key_t  widget_key;

  int                  layer_above;
  int                  reparent_hack;
  
  kbinding_t          *kbindings;
  
} gGui_t;


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
 * Configuration file convenience functions
 */

void config_save(void);
void config_reset(void);

char *gui_get_skindir(void);
char *gui_get_configfile(void);

void gui_init (int nfiles, char *filenames[]);

void gui_run (void);

void gui_status_callback (int nStatus) ;

void gui_dndcallback (char *filename) ;

void gui_execute_action_id(action_id_t);

void gui_handle_event (XEvent *event, void *data);

char *gui_next_mrl_callback (void) ;

void gui_branched_callback (void) ;

void gui_handle_xine_error(void);
#endif


