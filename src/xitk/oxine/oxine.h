/*
 * Copyright (C) 2002 Stefan Holst
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 *
 * oxine data structures
 */

#ifndef HAVE_OXINE_H
#define HAVE_OXINE_H

#include <stdlib.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include <pthread.h>
#include <xine.h>

#include "otk.h"
#include "odk.h"

/* I assume that we can have only an osd window  *
 * displayed                                     */

#define OXINE_MODE_PLAY_MENU (1)
#define OXINE_MODE_INFO_MENU (2)
#define OXINE_MODE_NORMAL (3)
#define OXINE_MODE_MAINMENU (4)

typedef struct oxine_s oxine_t;
typedef struct oxine_window_s oxine_window_t;

struct oxine_s {

  xine_t             *xine;

  otk_t              *otk;
  odk_t              *odk;
  otk_widget_t       *main_window;
  otk_widget_t       *pauseplay;


  int                mode;
  int                cd_is_mounted;
  int                cd_in_use;
  /* int                need_draw; */

  const char         *cd_mountpoint;
  const char         *cd_device;

  int                mm_sort_type;

  void (*main_menu_cb) (void *data);

  void (*reentry) (void *data);
  void               *reentry_data;

  char               *pos_str;

  /*
   * stream info blending
   */
  char               *lines[3];
  otk_widget_t       *info_window;
  int                 info_window_job;
  void (*media_info_close_cb)(void *data);

      
};

void oxine_init(void);

void oxine_exit(void);

void oxine_menu(void);

int oxine_action_event(int xine_event_type);

int oxine_mouse_event(int xine_event_type, int x, int y);

void oxine_adapt(void);

#endif
