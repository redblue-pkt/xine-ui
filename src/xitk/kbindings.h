/* 
 * Copyright (C) 2000-2020 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */
#ifndef KBINDINGS_H
#define KBINDINGS_H

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
  ACTID_TOGGLE_WINOUT_BORDER,
  ACTID_AUDIOCHAN_NEXT,
  ACTID_AUDIOCHAN_PRIOR,
  ACTID_PLAYLIST,
  ACTID_PAUSE,
  ACTID_TOGGLE_VISIBLITY,
  ACTID_TOGGLE_FULLSCREEN,
#ifdef HAVE_XINERAMA
  ACTID_TOGGLE_XINERAMA_FULLSCREEN,
#endif
  ACTID_TOGGLE_ASPECT_RATIO,
  ACTID_STREAM_INFOS,
  ACTID_TOGGLE_INTERLEAVE,
  ACTID_QUIT,
  ACTID_PLAY,
  ACTID_STOP,
  ACTID_CLOSE,
  ACTID_MRL_NEXT,
  ACTID_MRL_PRIOR,
  ACTID_MRL_SELECT,
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
  ACTID_SET_CURPOS_100,
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
  ACTID_pAMP,
  ACTID_mAMP,
  ACTID_AMP_RESET,
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
  ACTID_TVANALOG,
  ACTID_SETUP,
  ACTID_VIEWLOG,
  ACTID_KBEDIT,
  ACTID_KBENABLE,
  ACTID_DPMSSTANDBY,
  ACTID_MRLIDENTTOGGLE,
  ACTID_SCANPLAYLIST,
  ACTID_MMKEDITOR,
  ACTID_LOOPMODE,
  ACTID_ADDMEDIAMARK,
  ACTID_SKINDOWNLOAD,
  ACTID_OSD_SINFOS,
  ACTID_OSD_WTEXT,
  ACTID_FILESELECTOR,
  ACTID_SUBSELECT,
  ACTID_SV_SYNC_p,
  ACTID_SV_SYNC_m,
  ACTID_SV_SYNC_RESET,
  ACTID_HUECONTROLp,
  ACTID_HUECONTROLm,
  ACTID_SATURATIONCONTROLp,
  ACTID_SATURATIONCONTROLm,
  ACTID_BRIGHTNESSCONTROLp,
  ACTID_BRIGHTNESSCONTROLm,
  ACTID_CONTRASTCONTROLp,
  ACTID_CONTRASTCONTROLm,
  ACTID_GAMMACONTROLp,
  ACTID_GAMMACONTROLm,
  ACTID_SHARPNESSCONTROLp,
  ACTID_SHARPNESSCONTROLm,
  ACTID_NOISEREDUCTIONCONTROLp,
  ACTID_NOISEREDUCTIONCONTROLm,
  ACTID_VPP,
  ACTID_VPP_ENABLE,
  ACTID_HELP_SHOW,
  ACTID_PLAYLIST_STOP,
  ACTID_OSD_MENU,
  ACTID_APP,
  ACTID_APP_ENABLE,
  ACTID_PVR_SETINPUT,
  ACTID_PVR_SETFREQUENCY,
  ACTID_PVR_SETMARK,
  ACTID_PVR_SETNAME,
  ACTID_PVR_SAVE,
  ACTID_PLAYLIST_OPEN,

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
  ACTID_EVENT_NUMBER_10_ADD = XINE_EVENT_INPUT_NUMBER_10_ADD  | ACTID_IS_INPUT_EVENT,
#ifdef ENABLE_VDR_KEYS
  ACTID_EVENT_VDR_RED              = XINE_EVENT_VDR_RED              | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_GREEN            = XINE_EVENT_VDR_GREEN            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_YELLOW           = XINE_EVENT_VDR_YELLOW           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_BLUE             = XINE_EVENT_VDR_BLUE             | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_PLAY             = XINE_EVENT_VDR_PLAY             | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_PAUSE            = XINE_EVENT_VDR_PAUSE            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_STOP             = XINE_EVENT_VDR_STOP             | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_RECORD           = XINE_EVENT_VDR_RECORD           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_FASTFWD          = XINE_EVENT_VDR_FASTFWD          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_FASTREW          = XINE_EVENT_VDR_FASTREW          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_POWER            = XINE_EVENT_VDR_POWER            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_CHANNELPLUS      = XINE_EVENT_VDR_CHANNELPLUS      | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_CHANNELMINUS     = XINE_EVENT_VDR_CHANNELMINUS     | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_SCHEDULE         = XINE_EVENT_VDR_SCHEDULE         | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_CHANNELS         = XINE_EVENT_VDR_CHANNELS         | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_TIMERS           = XINE_EVENT_VDR_TIMERS           | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_RECORDINGS       = XINE_EVENT_VDR_RECORDINGS       | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_SETUP            = XINE_EVENT_VDR_SETUP            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_COMMANDS         = XINE_EVENT_VDR_COMMANDS         | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_BACK             = XINE_EVENT_VDR_BACK             | ACTID_IS_INPUT_EVENT,
#ifdef XINE_EVENT_VDR_USER0 /* #ifdef is precaution for backward compatibility at the moment */
  ACTID_EVENT_VDR_USER0            = XINE_EVENT_VDR_USER0            | ACTID_IS_INPUT_EVENT,
#endif
  ACTID_EVENT_VDR_USER1            = XINE_EVENT_VDR_USER1            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER2            = XINE_EVENT_VDR_USER2            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER3            = XINE_EVENT_VDR_USER3            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER4            = XINE_EVENT_VDR_USER4            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER5            = XINE_EVENT_VDR_USER5            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER6            = XINE_EVENT_VDR_USER6            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER7            = XINE_EVENT_VDR_USER7            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER8            = XINE_EVENT_VDR_USER8            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_USER9            = XINE_EVENT_VDR_USER9            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_VOLPLUS          = XINE_EVENT_VDR_VOLPLUS          | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_VOLMINUS         = XINE_EVENT_VDR_VOLMINUS         | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_MUTE             = XINE_EVENT_VDR_MUTE             | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_AUDIO            = XINE_EVENT_VDR_AUDIO            | ACTID_IS_INPUT_EVENT,
  ACTID_EVENT_VDR_INFO             = XINE_EVENT_VDR_INFO             | ACTID_IS_INPUT_EVENT,
#ifdef XINE_EVENT_VDR_CHANNELPREVIOUS /* #ifdef is precaution for backward compatibility at the moment */
  ACTID_EVENT_VDR_CHANNELPREVIOUS  = XINE_EVENT_VDR_CHANNELPREVIOUS  | ACTID_IS_INPUT_EVENT,
#endif
#ifdef XINE_EVENT_VDR_SUBTITLES /* #ifdef is precaution for backward compatibility at the moment */
  ACTID_EVENT_VDR_SUBTITLES        = XINE_EVENT_VDR_SUBTITLES        | ACTID_IS_INPUT_EVENT,
#endif
#endif

  /*
   * End of numeric mapping.
   */
} action_id_t;
  
kbinding_t *kbindings_init_kbinding(void);
void kbindings_save_kbinding(kbinding_t *);
void kbindings_reset_kbinding(kbinding_t *);
void kbindings_free_kbinding(kbinding_t **);
void kbindings_display_current_bindings(kbinding_t *);
void kbindings_display_default_lirc_bindings(void);
void kbindings_display_default_bindings(void);
kbinding_entry_t *kbindings_lookup_action(kbinding_t *, const char *);
void kbindings_handle_kbinding(kbinding_t *kbt, KeySym keysym, int keycode, int modifier, int button);
action_id_t kbindings_get_action_id(kbinding_entry_t *);

/* return bytes written (without terminating nul).
 * if result >= buf_size, output was truncated. Does not return space required!
 * if result == 0, entry was not found.
 */
size_t kbindings_get_shortcut(kbinding_t *, const char *, char *buf, size_t buf_size);

void kbedit_window (gGui_t *gui);
void kbedit_end (xui_keyedit_t *kbedit);
int kbedit_is_visible (xui_keyedit_t *kbedit);
void kbedit_toggle_visibility (xitk_widget_t *w, void *kbedit);
void kbedit_raise_window (xui_keyedit_t *kbedit);
void kbedit_reparent (xui_keyedit_t *kbedit);

#endif
