/* 
 * Copyright (C) 2000-2002 the xine project
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
#ifndef KBINDINGS_H
#define KBINDINGS_H

#include <X11/Xlib.h>

#include "xitk.h"

/* Opaque structure */
typedef struct kbinding_s kbinding_t;
typedef struct kbinding_entry_s kbinding_entry_t;

/* 
 * This bit is turned on in the enum below to indicate there is 
 * a corresponding definition xine-lib's events.h 
 */
#define ACTID_IS_INPUT_EVENT 0x400

typedef enum {
  ACTID_NOKEY = 0,
  ACTID_WINDOWREDUCE,
  ACTID_WINDOWENLARGE,
  ACTID_WINDOW100,
  ACTID_WINDOW200,
  ACTID_WINDOW50,
  ACTID_SPU_NEXT,
  ACTID_SPU_PRIOR,
  ACTID_CONTROLSHOW,
  ACTID_TOGGLE_WINOUT_VISIBLITY,
  ACTID_AUDIOCHAN_NEXT,
  ACTID_AUDIOCHAN_PRIOR,
  ACTID_PLAYLIST,
  ACTID_PAUSE,
  ACTID_TOGGLE_VISIBLITY,
  ACTID_TOGGLE_FULLSCREEN,
  ACTID_TOGGLE_ASPECT_RATIO,
  ACTID_STREAM_INFOS,
  ACTID_TOGGLE_INTERLEAVE,
  ACTID_QUIT,
  ACTID_PLAY,
  ACTID_STOP,
  ACTID_MRL_NEXT,
  ACTID_MRL_PRIOR,
  ACTID_EVENT_SENDER,
  ACTID_EJECT,
  ACTID_SET_CURPOS,
  ACTID_SET_CURPOS_0,
  ACTID_SET_CURPOS_10,
  ACTID_SET_CURPOS_20,
  ACTID_SET_CURPOS_30,
  ACTID_SET_CURPOS_40,
  ACTID_SET_CURPOS_50,
  ACTID_SET_CURPOS_60,
  ACTID_SET_CURPOS_70,
  ACTID_SET_CURPOS_80,
  ACTID_SET_CURPOS_90,
  ACTID_SEEK_REL_m,
  ACTID_SEEK_REL_p,
  ACTID_SEEK_REL_m60,
  ACTID_SEEK_REL_p60,
  ACTID_SEEK_REL_m15,
  ACTID_SEEK_REL_p15,
  ACTID_SEEK_REL_m30,
  ACTID_SEEK_REL_p30,
  ACTID_SEEK_REL_m7,
  ACTID_SEEK_REL_p7,
  ACTID_MRLBROWSER,
  ACTID_MUTE,
  ACTID_AV_SYNC_p3600,
  ACTID_AV_SYNC_m3600,
  ACTID_AV_SYNC_RESET,
  ACTID_SPEED_FAST,
  ACTID_SPEED_SLOW,
  ACTID_SPEED_RESET,
  ACTID_pVOLUME,
  ACTID_mVOLUME,
  ACTID_SNAPSHOT,
  ACTID_ZOOM_1_1,
  ACTID_GRAB_POINTER,
  ACTID_ZOOM_IN,
  ACTID_ZOOM_OUT,
  ACTID_ZOOM_X_IN,
  ACTID_ZOOM_X_OUT,
  ACTID_ZOOM_Y_IN,
  ACTID_ZOOM_Y_OUT,
  ACTID_ZOOM_RESET,
  ACTID_TOGGLE_TVMODE,
  ACTID_SETUP,
  ACTID_VIEWLOG,
  ACTID_KBEDIT,
  ACTID_DPMSSTANDBY,
  ACTID_MRLIDENTTOGGLE,
  ACTID_SCANPLAYLIST,

  /*
   * The below events map one-to-one with definitions in xine-lib's events.h 
   */
  ACTID_EVENT_MENU1         = XINE_EVENT_INPUT_MENU1          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU2         = XINE_EVENT_INPUT_MENU2          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU3         = XINE_EVENT_INPUT_MENU3          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU4         = XINE_EVENT_INPUT_MENU4          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU5         = XINE_EVENT_INPUT_MENU5          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU6         = XINE_EVENT_INPUT_MENU6          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_MENU7         = XINE_EVENT_INPUT_MENU7          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_UP            = XINE_EVENT_INPUT_UP             | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_DOWN          = XINE_EVENT_INPUT_DOWN           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_LEFT          = XINE_EVENT_INPUT_LEFT           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_RIGHT         = XINE_EVENT_INPUT_RIGHT          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_PRIOR         = XINE_EVENT_INPUT_PREVIOUS       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NEXT          = XINE_EVENT_INPUT_NEXT           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_ANGLE_NEXT    = XINE_EVENT_INPUT_ANGLE_NEXT     | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_ANGLE_PRIOR   = XINE_EVENT_INPUT_ANGLE_PREVIOUS | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_SELECT        = XINE_EVENT_INPUT_SELECT         | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_0      = XINE_EVENT_INPUT_NUMBER_0       | ACTID_IS_INPUT_EVENT,  
  ACTID_EVENT_NUMBER_1      = XINE_EVENT_INPUT_NUMBER_1       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_2      = XINE_EVENT_INPUT_NUMBER_2       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_3      = XINE_EVENT_INPUT_NUMBER_3       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_4      = XINE_EVENT_INPUT_NUMBER_4       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_5      = XINE_EVENT_INPUT_NUMBER_5       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_6      = XINE_EVENT_INPUT_NUMBER_6       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_7      = XINE_EVENT_INPUT_NUMBER_7       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_8      = XINE_EVENT_INPUT_NUMBER_8       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_9      = XINE_EVENT_INPUT_NUMBER_9       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_NUMBER_10_ADD = XINE_EVENT_INPUT_NUMBER_10_ADD  | ACTID_IS_INPUT_EVENT
  /*
   * End of numeric mapping.
   */
} action_id_t;
  
kbinding_t *kbindings_init_kbinding(void);
void kbindings_save_kbinding(kbinding_t *);
void kbindings_reset_kbinding(kbinding_t **);
void kbindings_free_kbinding(kbinding_t **);
void kbindings_display_current_bindings(kbinding_t *);
void kbindings_display_default_lirc_bindings(void);
void kbindings_display_default_bindings(void);
kbinding_entry_t *kbindings_lookup_action(kbinding_t *, const char *);
void kbindings_handle_kbinding(kbinding_t *, XEvent *);
action_id_t kbindings_get_action_id(kbinding_entry_t *);

void kbedit_window(void);
void kbedit_exit(xitk_widget_t *, void *);
int kbedit_is_visible(void);
int kbedit_is_running(void);
void kbedit_toggle_visibility(xitk_widget_t *, void *);
void kbedit_raise_window(void);

#endif
