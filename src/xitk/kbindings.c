/* 
 * Copyright (C) 2000-2003 the xine project
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

extern gGui_t                 *gGui;

#undef TRACE_KBINDINGS

#define  KBEDIT_NOOP           0
#define  KBEDIT_ALIASING       1
#define  KBEDIT_EDITING        2

typedef struct {
  kbinding_t           *kbt;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;
  int                   running;
  int                   visible;

  xitk_widget_t        *browser;
  kbinding_entry_t     *ksel;
  
  xitk_widget_t        *alias;
  xitk_widget_t        *edit;
  xitk_widget_t        *delete;
  xitk_widget_t        *save;
  xitk_widget_t        *done;
  xitk_widget_t        *grab;

  xitk_widget_t        *comment;
  xitk_widget_t        *key;

  xitk_widget_t        *ctrl;
  xitk_widget_t        *meta;
  xitk_widget_t        *mod3;
  xitk_widget_t        *mod4;
  xitk_widget_t        *mod5;

  int                   num_entries;
  char                **entries;

  int                   grabbing;
  int                   action_wanted; /* See KBEDIT_ defines */

  xitk_register_key_t   kreg;
} _kbedit_t;

static _kbedit_t    *kbedit = NULL;
static char         *fontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
static char         *br_fontname = "-misc-fixed-medium-r-normal-*-10-*-*-*-*-*-*-*";
#define FONT_HEIGHT_MODEL "azertyuiopqsdfghjklmwxcvbnAZERTYUIOPQSDFGHJKLMWXCVBN&й(-и_за)=№~#{[|`\\^@]}%"

#define WINDOW_WIDTH        520
#define WINDOW_HEIGHT       440
#define MAX_DISP_ENTRIES    11

/*
 * Handled key modifier.
 */
#define KEYMOD_NOMOD           0x00000000
#define KEYMOD_CONTROL         0x00000001
#define KEYMOD_META            0x00000002
#define KEYMOD_MOD3            0x00000010
#define KEYMOD_MOD4            0x00000020
#define KEYMOD_MOD5            0x00000040

#define DEFAULT_DISPLAY_MODE   1
#define LIRC_DISPLAY_MODE      2
/*
 * Key binding entry struct.
 */
struct kbinding_entry_s {
  char             *comment;     /* Comment automatically added in xbinding_display*() outputs */
  char             *action;      /* Human readable action, used in config file too */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  char             *key;         /* key binding */
  int               modifier;    /* Modifier key of binding (can be OR'ed) */
  int               is_alias;    /* is made from an alias entry ? */
};

#define MAX_ENTRIES 255
struct kbinding_s {
  int               num_entries;
  kbinding_entry_t *entry[MAX_ENTRIES];
};

/*
  Remap file entries syntax are:
  ...  
  WindowReduce {
      key = less
      modifier = none
  }

  Alias {
      entry = WindowReduce
      key = w
      modifier = control
  }
  ...
  WindowReduce action key is <less>, with no modifier usage.
  There is an alias: C-w (control w> keystroke will have same
                     action than WindowReduce.

  modifier entry is optional.
  modifier key can be multiple:
 ...
  WindowReduce {
      key = less
      modifier = control, meta
  }
  ...
  Here, WindowReduce action keystroke will be CM-<

  Key modifier name are:
    none, control, ctrl, meta, alt, mod3, mod4, mod5.

  shift,lock and numlock modifier are voluntary not handled.

*/
/* Same as above, used on remapping */
typedef struct {
  char             *alias;
  char             *action;
  char             *key;
  char             *modifier;
  int               is_alias;
} user_kbinding_t;

/*
 * keybinding file object.
 */
typedef struct {
  FILE             *fd;
  char             *bindingfile;
  char             *ln;
  char              buf[256];
} kbinding_file_t;


/* 
 * Default key mapping table.
 */
static kbinding_entry_t default_binding_table[] = {
  { "start playback",
    "Play",                   ACTID_PLAY                    , "Return",   KEYMOD_NOMOD   , 0 },
  { "playback pause toggle",
    "Pause",                  ACTID_PAUSE                   , "space",    KEYMOD_NOMOD   , 0 },
  { "stop playback",
    "Stop",                   ACTID_STOP                    , "S",        KEYMOD_NOMOD   , 0 },
  { "take a snapshot",
    "Snapshot",               ACTID_SNAPSHOT                , "t",        KEYMOD_NOMOD   , 0 },
  { "eject the current medium",
    "Eject",                  ACTID_EJECT                   , "e",        KEYMOD_NOMOD   , 0 },
  { "select and play next MRL in the playlist",
    "NextMrl",                ACTID_MRL_NEXT                , "Next",     KEYMOD_NOMOD   , 0 },
  { "select and play previous MRL in the playlist",
    "PriorMrl",               ACTID_MRL_PRIOR               , "Prior",    KEYMOD_NOMOD   , 0 },
  { "loop mode toggle",
    "ToggleLoopMode",         ACTID_LOOPMODE	            , "l",	  KEYMOD_NOMOD   , 0 },
  { "stop playback after played stream",
    "PlaylistStop",           ACTID_PLAYLIST_STOP           , "l",	  KEYMOD_CONTROL , 0 },
  { "scan playlist to grab stream infos",
    "ScanPlaylistInfo",       ACTID_SCANPLAYLIST            , "s",        KEYMOD_CONTROL , 0 },
  { "add a mediamark from current playback",
    "AddMediamark",           ACTID_ADDMEDIAMARK            , "a",        KEYMOD_CONTROL , 0 },
  { "edit selected mediamark",
    "MediamarkEditor",        ACTID_MMKEDITOR               , "e",        KEYMOD_CONTROL , 0 },
  { "set position to -60 seconds in current stream",
    "SeekRelative-60",        ACTID_SEEK_REL_m60            , "Left",     KEYMOD_NOMOD   , 0 }, 
  { "set position to +60 seconds in current stream",
    "SeekRelative+60",        ACTID_SEEK_REL_p60            , "Right",    KEYMOD_NOMOD   , 0 },
  { "set position to -30 seconds in current stream",
    "SeekRelative-30",        ACTID_SEEK_REL_m30            , "Left",     KEYMOD_META    , 0 }, 
  { "set position to +30 seconds in current stream",
    "SeekRelative+30",        ACTID_SEEK_REL_p30            , "Right",    KEYMOD_META    , 0 },
  { "set position to -15 seconds in current stream",
    "SeekRelative-15",        ACTID_SEEK_REL_m15            , "Left",     KEYMOD_CONTROL , 0 },
  { "set position to +15 seconds in current stream",
    "SeekRelative+15",        ACTID_SEEK_REL_p15            , "Right",    KEYMOD_CONTROL , 0 },
  { "set position to -7 seconds in current stream",
    "SeekRelative-7",         ACTID_SEEK_REL_m7             , "Left",     KEYMOD_MOD3    , 0 }, 
  { "set position to +7 seconds in current stream",
    "SeekRelative+7",         ACTID_SEEK_REL_p7             , "Right",    KEYMOD_MOD3    , 0 },
  { "set position to beginning of current stream",
    "SetPosition0%",          ACTID_SET_CURPOS_0            , "0",        KEYMOD_CONTROL , 0 },
  { "set position to 10% of current stream",
    "SetPosition10%",         ACTID_SET_CURPOS_10           , "1",        KEYMOD_CONTROL , 0 },
  { "set position to 20% of current stream",
    "SetPosition20%",         ACTID_SET_CURPOS_20           , "2",        KEYMOD_CONTROL , 0 },
  { "set position to 30% of current stream",
    "SetPosition30%",         ACTID_SET_CURPOS_30           , "3",        KEYMOD_CONTROL , 0 },
  { "set position to 40% of current stream",
    "SetPosition40%",         ACTID_SET_CURPOS_40           , "4",        KEYMOD_CONTROL , 0 },
  { "set position to 50% of current stream",
    "SetPosition50%",         ACTID_SET_CURPOS_50           , "5",        KEYMOD_CONTROL , 0 },
  { "set position to 60% of current stream",
    "SetPosition60%",         ACTID_SET_CURPOS_60           , "6",        KEYMOD_CONTROL , 0 },
  { "set position to 70% of current stream",
    "SetPosition70%",         ACTID_SET_CURPOS_70           , "7",        KEYMOD_CONTROL , 0 },
  { "set position to 80% of current stream",
    "SetPosition80%",         ACTID_SET_CURPOS_80           , "8",        KEYMOD_CONTROL , 0 },
  { "set position to 90% of current stream",
    "SetPosition90%",         ACTID_SET_CURPOS_90           , "9",        KEYMOD_CONTROL , 0 },
  { "increment playback speed",
    "SpeedFaster",            ACTID_SPEED_FAST              , "Up",       KEYMOD_NOMOD   , 0 },
  { "decrement playback speed",
    "SpeedSlower",            ACTID_SPEED_SLOW              , "Down",     KEYMOD_NOMOD   , 0 },
  { "reset playback speed",
    "SpeedReset",             ACTID_SPEED_RESET             , "Down",     KEYMOD_META    , 0 },
  { "increment audio volume",
    "Volume+",                ACTID_pVOLUME                 , "V",        KEYMOD_NOMOD   , 0 },
  { "decrement audio volume",
    "Volume-",                ACTID_mVOLUME                 , "v",        KEYMOD_NOMOD   , 0 },
  { "increment amplification level",
    "Amp+",                   ACTID_pAMP                    , "V",        KEYMOD_CONTROL , 0 },
  { "decrement amplification level",
    "Amp-",                   ACTID_mAMP                    , "v",        KEYMOD_CONTROL , 0 },
  { "reset amplification to default value",
    "ResetAmp",               ACTID_AMP_RESET               , "A",        KEYMOD_CONTROL , 0 },
  { "audio muting toggle",
    "Mute",                   ACTID_MUTE                    , "m",        KEYMOD_CONTROL , 0 },
  { "select next audio channel",
    "AudioChannelNext",       ACTID_AUDIOCHAN_NEXT          , "plus",     KEYMOD_NOMOD   , 0 },
  { "select previous audio channel",
    "AudioChannelPrior",      ACTID_AUDIOCHAN_PRIOR         , "minus",    KEYMOD_NOMOD   , 0 },
  { "select next sub picture (subtitle) channel",
    "SpuNext",                ACTID_SPU_NEXT                , "period",   KEYMOD_NOMOD   , 0 },
  { "select previous sub picture (subtitle) channel",
    "SpuPrior",               ACTID_SPU_PRIOR               , "comma",    KEYMOD_NOMOD   , 0 },
  { "interlaced mode toggle",
    "ToggleInterleave",       ACTID_TOGGLE_INTERLEAVE       , "i",        KEYMOD_NOMOD   , 0 },
  { "cycle aspect ratio values",
    "ToggleAspectRatio",      ACTID_TOGGLE_ASPECT_RATIO     , "a",        KEYMOD_NOMOD   , 0 },
  { "reduce the output window size by factor 1.2",
    "WindowReduce",           ACTID_WINDOWREDUCE            , "less",     KEYMOD_NOMOD   , 0 },
  { "enlarge the output window size by factor 1.2",
    "WindowEnlarge",          ACTID_WINDOWENLARGE           , "greater",  KEYMOD_NOMOD   , 0 },
  { "set video output window to 50%",
    "Window50",               ACTID_WINDOW50                , "1",        KEYMOD_META    , 0 },
  { "set video output window to 100%",
    "Window100",              ACTID_WINDOW100               , "2",        KEYMOD_META    , 0 },
  { "set video output window to 200%",
    "Window200",              ACTID_WINDOW200               , "3",        KEYMOD_META    , 0 },
  { "zoom in",
    "ZoomIn",                 ACTID_ZOOM_IN                 , "z",        KEYMOD_NOMOD   , 0 },
  { "zoom out",
    "ZoomOut",                ACTID_ZOOM_OUT                , "Z",        KEYMOD_NOMOD   , 0 },
  { "zoom in horizontally",
    "ZoomInX",                ACTID_ZOOM_X_IN               , "z",        KEYMOD_CONTROL , 0 },
  { "zoom out horizontally",
    "ZoomOutX",               ACTID_ZOOM_X_OUT              , "Z",        KEYMOD_CONTROL , 0 },
  { "zoom in vertically",
    "ZoomInY",                ACTID_ZOOM_Y_IN               , "z",        KEYMOD_META    , 0 },
  { "zoom out vertically",
    "ZoomOutY",               ACTID_ZOOM_Y_OUT              , "Z",        KEYMOD_META    , 0 },
  { "reset zooming",
    "ZoomReset",              ACTID_ZOOM_RESET              , "z",        KEYMOD_CONTROL | KEYMOD_META , 0 },
  { "resize output window to stream size",
    "Zoom1:1",                ACTID_ZOOM_1_1                , "s",        KEYMOD_NOMOD   , 0 },
  { "fullscreen toggle",
    "ToggleFullscreen",       ACTID_TOGGLE_FULLSCREEN       , "f",        KEYMOD_NOMOD   , 0 },
#ifdef HAVE_XINERAMA
  { "Xinerama fullscreen toggle",
    "ToggleXineramaFullscr",  ACTID_TOGGLE_XINERAMA_FULLSCREEN 
                                                            , "F",        KEYMOD_NOMOD   , 0 },
#endif
  { "jump to media Menu",
    "Menu",                   ACTID_EVENT_MENU1             , "Escape",   KEYMOD_NOMOD   , 0 },
  { "jump to Title Menu",
    "TitleMenu",              ACTID_EVENT_MENU2             , "F1",       KEYMOD_NOMOD   , 0 },
  { "jump to Root Menu",
    "RootMenu",               ACTID_EVENT_MENU3             , "F2",       KEYMOD_NOMOD   , 0 },
  { "jump to Subpicture Menu",
    "SubpictureMenu",         ACTID_EVENT_MENU4             , "F3",       KEYMOD_NOMOD   , 0 },
  { "jump to Audio Menu",
    "AudioMenu",              ACTID_EVENT_MENU5             , "F4",       KEYMOD_NOMOD   , 0 },
  { "jump to Angle Menu",
    "AngleMenu",              ACTID_EVENT_MENU6             , "F5",       KEYMOD_NOMOD   , 0 },
  { "jump to Part Menu",
    "PartMenu",               ACTID_EVENT_MENU7             , "F6",       KEYMOD_NOMOD   , 0 },
  { "menu navigate up",
    "EventUp",                ACTID_EVENT_UP                , "KP_Up",    KEYMOD_NOMOD   , 0 },
  { "menu navigate down",
    "EventDown",              ACTID_EVENT_DOWN              , "KP_Down",  KEYMOD_NOMOD   , 0 },
  { "menu navigate left",
    "EventLeft",              ACTID_EVENT_LEFT              , "KP_Left",  KEYMOD_NOMOD   , 0 },
  { "menu navigate right",
    "EventRight",             ACTID_EVENT_RIGHT             , "KP_Right", KEYMOD_NOMOD   , 0 },
  { "menu select",
    "EventSelect",            ACTID_EVENT_SELECT            , "KP_Enter", KEYMOD_NOMOD   , 0 },
  { "jump to next chapter",
    "EventNext",              ACTID_EVENT_NEXT              , "KP_Next",  KEYMOD_NOMOD   , 0 },
  { "jump to previous chapter",
    "EventPrior",             ACTID_EVENT_PRIOR             , "KP_Prior", KEYMOD_NOMOD   , 0 },
  { "select next angle",
    "EventAngleNext",         ACTID_EVENT_ANGLE_NEXT        , "KP_Home",  KEYMOD_NOMOD   , 0 },
  { "select previous angle",
    "EventAnglePrior",        ACTID_EVENT_ANGLE_PRIOR       , "KP_End",   KEYMOD_NOMOD   , 0 },
  { "visibility toggle of help window",
    "HelpShow",               ACTID_HELP_SHOW               , "h",        KEYMOD_META    , 0 },
  { "visibility toggle of video post effect window",
    "VPProcessShow",          ACTID_VPP                     , "P",        KEYMOD_META    , 0 },
  { "toggle post effect usage",
    "VPProcessEnable",        ACTID_VPP_ENABLE              , "P",        KEYMOD_CONTROL | KEYMOD_META , 0 },
  { "visibility toggle of output window",
    "ToggleWindowVisibility", ACTID_TOGGLE_WINOUT_VISIBLITY , "h",        KEYMOD_NOMOD   , 0 },
  { "visibility toggle of UI windows",
    "ToggleVisiblity",        ACTID_TOGGLE_VISIBLITY        , "g",        KEYMOD_NOMOD   , 0 },
  { "visibility toggle of control window",
    "ControlShow",            ACTID_CONTROLSHOW             , "c",        KEYMOD_META    , 0 },
  { "visibility toggle of mrl browser window",
    "MrlBrowser",             ACTID_MRLBROWSER              , "m",        KEYMOD_META    , 0 },
  { "visibility toggle of playlist editor window",
    "PlaylistEditor",         ACTID_PLAYLIST                , "p",        KEYMOD_META    , 0 },
  { "visibility toggle of the setup window",
    "SetupShow",              ACTID_SETUP                   , "s",        KEYMOD_META    , 0 },
  { "visibility toggle of the event sender window",
    "EventSenderShow",        ACTID_EVENT_SENDER            , "e",        KEYMOD_META    , 0 },
  { "visibility toggle of analog TV window",
    "TVAnalogShow",           ACTID_TVANALOG                , "t",        KEYMOD_META    , 0 },
  { "visibility toggle of log viewer",
    "ViewlogShow",            ACTID_VIEWLOG	            , "l",	  KEYMOD_META    , 0 },
  { "visibility toggle of stream info window",
    "StreamInfosShow",        ACTID_STREAM_INFOS            , "i",        KEYMOD_META    , 0 },
  { "display stream information using OSD",
    "OSDStreamInfos",         ACTID_OSD_SINFOS              , "i",        KEYMOD_CONTROL , 0 },
  { "enter key binding editor",
    "KeyBindingEditor",       ACTID_KBEDIT	            , "k",	  KEYMOD_META    , 0 },
  { "open file selector",
    "FileSelector",           ACTID_FILESELECTOR            , "o",        KEYMOD_CONTROL , 0 },
  { "select a subtitle file",
    "SubSelector",            ACTID_SUBSELECT               , "S",        KEYMOD_CONTROL , 0 },
#ifdef HAVE_CURL
  { "download a skin from the skin server",
    "SkinDownload",           ACTID_SKINDOWNLOAD            , "d",        KEYMOD_CONTROL , 0 },
#endif
  { "display MRL/Ident toggle",
    "MrlIdentToggle",         ACTID_MRLIDENTTOGGLE          , "t",        KEYMOD_CONTROL , 0 },
  { "grab pointer toggle",
    "GrabPointer",            ACTID_GRAB_POINTER            , "Insert",   KEYMOD_NOMOD   , 0 },
  { "enter the number 0",
    "Number0",                ACTID_EVENT_NUMBER_0          , "0",        KEYMOD_NOMOD   , 0 },
  { "enter the number 1",
    "Number1",                ACTID_EVENT_NUMBER_1          , "1",        KEYMOD_NOMOD   , 0 },
  { "enter the number 2",
    "Number2",                ACTID_EVENT_NUMBER_2          , "2",        KEYMOD_NOMOD   , 0 },
  { "enter the number 3",
    "Number3",                ACTID_EVENT_NUMBER_3          , "3",        KEYMOD_NOMOD   , 0 },
  { "enter the number 4",
    "Number4",                ACTID_EVENT_NUMBER_4          , "4",        KEYMOD_NOMOD   , 0 },
  { "enter the number 5",
    "Number5",                ACTID_EVENT_NUMBER_5          , "5",        KEYMOD_NOMOD   , 0 },
  { "enter the number 6",
    "Number6",                ACTID_EVENT_NUMBER_6          , "6",        KEYMOD_NOMOD   , 0 },
  { "enter the number 7",
    "Number7",                ACTID_EVENT_NUMBER_7          , "7",        KEYMOD_NOMOD   , 0 },
  { "enter the number 8",
    "Number8",                ACTID_EVENT_NUMBER_8          , "8",        KEYMOD_NOMOD   , 0 },
  { "enter the number 9",
    "Number9",                ACTID_EVENT_NUMBER_9          , "9",        KEYMOD_NOMOD   , 0 },
  { "add 10 to the next entered number",
    "Number10add",            ACTID_EVENT_NUMBER_10_ADD     , "plus",     KEYMOD_MOD3    , 0 },
  { "set position in current stream to numeric percentage",
    "SetPosition%",           ACTID_SET_CURPOS              , "slash",    KEYMOD_NOMOD   , 0 },
  { "set position forward by numeric argument in current stream",
    "SeekRelative+",          ACTID_SEEK_REL_p              , "Up",       KEYMOD_META    , 0 }, 
  { "set position back by numeric argument in current stream",
    "SeekRelative-",          ACTID_SEEK_REL_m              , "Up",       KEYMOD_MOD3    , 0 }, 
  { "change audio video syncing (delay video)",
    "AudioVideoDecay+",       ACTID_AV_SYNC_p3600           , "m",        KEYMOD_NOMOD   , 0 },
  { "change audio video syncing (delay audio)",
    "AudioVideoDecay-",       ACTID_AV_SYNC_m3600           , "n",        KEYMOD_NOMOD   , 0 },
  { "reset audio video syncing offset",
    "AudioVideoDecayReset",   ACTID_AV_SYNC_RESET           , "Home",     KEYMOD_NOMOD   , 0 },
  { "change subtitle syncing (delay video)",
    "SpuVideoDecay+",         ACTID_SV_SYNC_p               , "M",        KEYMOD_NOMOD   , 0 },
  { "change subtitle syncing (delay subtitles)",
    "SpuVideoDecay-",         ACTID_SV_SYNC_m               , "N",        KEYMOD_NOMOD   , 0 },
  { "reset subtitle syncing offset",
    "SpuVideoDecayReset",     ACTID_SV_SYNC_RESET           , "End",      KEYMOD_NOMOD   , 0 },
  { "toggle TV modes (on the DXR3)",
    "ToggleTVmode",           ACTID_TOGGLE_TVMODE	    , "o",	  KEYMOD_CONTROL | KEYMOD_META , 0 },
  { "switch Monitor to DPMS standby mode",
    "DPMSStandby",            ACTID_DPMSSTANDBY             , "d",        KEYMOD_NOMOD   , 0 },
  { "increase hue by 10",
    "HueControl+",            ACTID_HUECONTROLp             , "VOID",     KEYMOD_NOMOD   , 0 },
  { "decrease hue by 10",
    "HueControl-",            ACTID_HUECONTROLm             , "VOID",     KEYMOD_NOMOD   , 0 },
  { "increase saturation by 10",
    "SaturationControl+",     ACTID_SATURATIONCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 },
  { "decrease saturation by 10",
    "SaturationControl-",     ACTID_SATURATIONCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 },
  { "increase brightness by 10",
    "BrightnessControl+",     ACTID_BRIGHTNESSCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 },
  { "decrease brightness by 10",
    "BrightnessControl-",     ACTID_BRIGHTNESSCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 },
  { "increase contrast by 10",
    "ContrastControl+",       ACTID_CONTRASTCONTROLp        , "VOID",     KEYMOD_NOMOD   , 0 },
  { "decrease contrast by 10",
    "ContrastControl-",       ACTID_CONTRASTCONTROLm        , "VOID",     KEYMOD_NOMOD   , 0 },
  { "quit the program",
    "Quit",                   ACTID_QUIT                    , "q",        KEYMOD_NOMOD   , 0 },
  { 0,
    0,                        0,                              0,          0              , 0 }
};

/*
 * Check if there some redundant entries in key binding table kbt.
 */
static void _kbinding_reset_cb(xitk_widget_t *w, void *data, int button) {
  kbinding_t *kbt = (kbinding_t *) data;
  kbindings_reset_kbinding(&kbt);
}
static void _kbinding_editor_cb(xitk_widget_t *w, void *data, int button) {
  kbedit_window();
}
static void _kbindings_check_redundancy(kbinding_t *kbt) {
  int            i, j, found = -1;
  xitk_window_t *xw;
      
  if(kbt == NULL)
    return;
  
  for(i = 0; kbt->entry[i]->action != NULL; i++) {
    for(j = 0; kbt->entry[j]->action != NULL; j++) {
      if(i != j && j != found) {
	if((!strcmp(kbt->entry[i]->key, kbt->entry[j]->key)) &&
	   (kbt->entry[i]->modifier == kbt->entry[j]->modifier) && 
	   (strcasecmp(kbt->entry[i]->key, "void"))) {
	  char  buffer[4096];
	  
	  found = i;
	  
	  memset(&buffer, 0, sizeof(buffer));
	  sprintf(buffer, _("Key bindings of '%s' and '%s' are the same.\n\n"
			    "What do you want to do with current key bindings?\n"),
		  kbt->entry[i]->action, kbt->entry[j]->action);
	  
	  xw = xitk_window_dialog_two_buttons_with_width(gGui->imlib_data,
							 _("Keybindings error!"),
							 _("Reset"),
							 _("Editor"),
							 _kbinding_reset_cb, _kbinding_editor_cb, 
							 (void *) kbt, 400, ALIGN_CENTER,
							 buffer);
	  XLockDisplay(gGui->display);
	  if(!gGui->use_root_window)
	    XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
	  XSync(gGui->display, False);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(xw));
	}
      }
    }
  }
  
}

/*
 * Free a keybindings object.
 */
static void _kbindings_free_bindings(kbinding_t *kbt) {
  kbinding_entry_t **k;
  int                i;
  
  if(kbt == NULL)
    return;

  k = kbt->entry;
  for(i = (kbt->num_entries - 1); i >= 0; i--) {
    SAFE_FREE(k[i]->comment);
    SAFE_FREE(k[i]->action);
    SAFE_FREE(k[i]->key);
    free(k[i]);
  }
  SAFE_FREE(kbt);
}

/*
 * Return position in str of char 'c'. -1 if not found
 */
static int _is_there_char(const char *str, int c) {
  char *p;

  if(str)
    if((p = strrchr(str, c)) != NULL) {
      return (p - str);
    }
  
  return -1;
}

/*
 * Return >= 0 if it's begin of section, otherwise -1
 */
static int _kbindings_begin_section(kbinding_file_t *kbdf) {

  ABORT_IF_NULL(kbdf);

  return _is_there_char(kbdf->ln, '{');
}

/*
 * Return >= 0 if it's end of section, otherwise -1
 */
static int _kbindings_end_section(kbinding_file_t *kbdf) {

  ABORT_IF_NULL(kbdf);

  return _is_there_char(kbdf->ln, '}');
}

/*
 * Cleanup end of line.
 */
static void _kbindings_clean_eol(kbinding_file_t *kbdf) {
  char *p;

  ABORT_IF_NULL(kbdf);

  p = kbdf->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      p++;
    }

    while(p > kbdf->ln) {
      --p;
      
      if(*p == ' ' || *p == '\t') 
	*p = '\0';
      else
	break;
    }
  }
}

/*
 * Read the next line of the key binding file.
 */
static void _kbindings_get_next_line(kbinding_file_t *kbdf) {

  ABORT_IF_NULL(kbdf);
  ABORT_IF_NULL(kbdf->fd);
  
 __get_next_line:

  kbdf->ln = fgets(kbdf->buf, 255, kbdf->fd);

  while(kbdf->ln && (*kbdf->ln == ' ' || *kbdf->ln == '\t')) ++kbdf->ln;

  if(kbdf->ln) {
    if((strncmp(kbdf->ln, "//", 2) == 0) ||
       (strncmp(kbdf->ln, "/*", 2) == 0) || /**/
       (strncmp(kbdf->ln, ";", 1) == 0) ||
       (strncmp(kbdf->ln, "#", 1) == 0) ||
       (*kbdf->ln == '\0')) {
      goto __get_next_line;
    }

  }

  _kbindings_clean_eol(kbdf);
}

/*
 * move forward p pointer till non space or tab.
 */
static void _kbindings_set_pos_to_next_char(char **p) {

  ABORT_IF_NULL(*p);

  while(*(*p) != '\0' && (*(*p) == ' ' || *(*p) == '\t')) ++(*p);
}

/*
 * Position p pointer to value.
 */
static void _kbindings_set_pos_to_value(char **p) {
  
  ABORT_IF_NULL(*p);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Add an entry in key binding table kbt. Called when an Alias entry
 * is found.
 */
static void _kbindings_add_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  kbinding_entry_t  *k;
  int                modifier;

  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(ukb);

  k = kbindings_lookup_action(kbt, ukb->alias);
  if(k) {
    
    modifier = k->modifier;
    
    if(ukb->modifier) {
      char *p;
      
      modifier = KEYMOD_NOMOD;

      while((p = xine_strsep(&ukb->modifier, ",")) != NULL) {
	
	_kbindings_set_pos_to_next_char(&p);
	
	if(p) {
	  
	  if(!strcasecmp(p, "none"))
	    modifier = KEYMOD_NOMOD;
	  else if(!strcasecmp(p, "control"))
	    modifier |= KEYMOD_CONTROL;
	  else if(!strcasecmp(p, "ctrl"))
	    modifier |= KEYMOD_CONTROL;
	  else if(!strcasecmp(p, "meta"))
	    modifier |= KEYMOD_META;
	  else if(!strcasecmp(p, "alt"))
	    modifier |= KEYMOD_META;
	  else if(!strcasecmp(p, "mod3"))
	    modifier |= KEYMOD_MOD3;
	  else if(!strcasecmp(p, "mod4"))
	    modifier |= KEYMOD_MOD4;
	  else if(!strcasecmp(p, "mod5"))
	    modifier |= KEYMOD_MOD5;
	}
      }
    }

    /*
     * Add new entry (struct memory already allocated)
     */

    kbt->entry[kbt->num_entries - 1]->is_alias  = 1;
    kbt->entry[kbt->num_entries - 1]->comment   = strdup(k->comment);
    kbt->entry[kbt->num_entries - 1]->action    = strdup(k->action);
    kbt->entry[kbt->num_entries - 1]->action_id = k->action_id;
    kbt->entry[kbt->num_entries - 1]->key       = strdup(ukb->key);
    kbt->entry[kbt->num_entries - 1]->modifier  = modifier;
    
    /*
     * NULL terminate array.
     */
    kbt->entry[kbt->num_entries] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_t));
    kbt->entry[kbt->num_entries]->is_alias  = 0;
    kbt->entry[kbt->num_entries]->comment   = NULL;
    kbt->entry[kbt->num_entries]->action    = NULL;
    kbt->entry[kbt->num_entries]->action_id = 0;
    kbt->entry[kbt->num_entries]->key       = NULL;
    kbt->entry[kbt->num_entries]->modifier  = 0;
  
    kbt->num_entries++;

  }
}

/*
 * Change keystroke of modifier in entry.
 */
static void _kbindings_replace_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  int i;

  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(ukb);

  for(i = 0; kbt->entry[i]->action != NULL; i++) {

    if(!strcmp(kbt->entry[i]->action, ukb->action)) {
      SAFE_FREE(kbt->entry[i]->key);
      kbt->entry[i]->key = strdup(ukb->key);

      if(ukb->modifier) {
	char *p;
	int   modifier = KEYMOD_NOMOD;
	
	while((p = xine_strsep(&ukb->modifier, ",")) != NULL) {

	  _kbindings_set_pos_to_next_char(&p);
	  
	  if(p) {
	    
	    if(!strcasecmp(p, "none"))
	      modifier = KEYMOD_NOMOD;
	    else if(!strcasecmp(p, "control"))
	      modifier |= KEYMOD_CONTROL;
	    else if(!strcasecmp(p, "ctrl"))
	      modifier |= KEYMOD_CONTROL;
	    else if(!strcasecmp(p, "meta"))
	      modifier |= KEYMOD_META;
	    else if(!strcasecmp(p, "alt"))
	      modifier |= KEYMOD_META;
	    else if(!strcasecmp(p, "mod3"))
	      modifier |= KEYMOD_MOD3;
	    else if(!strcasecmp(p, "mod4"))
	      modifier |= KEYMOD_MOD4;
	    else if(!strcasecmp(p, "mod5"))
	      modifier |= KEYMOD_MOD5;
	  }
	}
	kbt->entry[i]->modifier = modifier;	
      }
      return;
    }
  }
}

/*
 * Read key remap section (try to).
 */
static void _kbindings_parse_section(kbinding_t *kbt, kbinding_file_t *kbdf) {
  int               brace_offset;
  user_kbinding_t   ukb;
  char              *p;
  
  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(kbdf);

  if((kbdf->ln != NULL) && (*kbdf->ln != '\0')) {
    int found = 0;
    
    memset(&ukb, 0, sizeof(ukb));
    found = 1;
  
    p = kbdf->ln;
    
    if((brace_offset = _kbindings_begin_section(kbdf)) >= 0) {
      *(kbdf->ln + brace_offset) = '\0';
      _kbindings_clean_eol(kbdf);
      
      ukb.action   = strdup(kbdf->ln);
      ukb.is_alias = 0;

      while(_kbindings_end_section(kbdf) < 0) {
	
	_kbindings_get_next_line(kbdf);
	p = kbdf->ln;
	if(kbdf->ln != NULL) {
	  if(!strncasecmp(kbdf->ln, "modifier", 8)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb.modifier = strdup(p);
	  }
	  if(!strncasecmp(kbdf->ln, "entry", 5)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb.alias = strdup(p);
	  }
	  else if(!strncasecmp(kbdf->ln, "key", 3)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb.key = strdup(p);
	  }	  
	}
	else
	  break;
      }

      if(found && ukb.alias && ukb.action && ukb.key) {
	_kbindings_add_entry(kbt, &ukb);
      }
      else if(found && ukb.action && ukb.key) {
	_kbindings_replace_entry(kbt, &ukb);
      }
      
    }

    SAFE_FREE(ukb.alias);
    SAFE_FREE(ukb.action);
    SAFE_FREE(ukb.key);
    SAFE_FREE(ukb.modifier);
  }
}

/*
 * Read and parse remap key binding file, if available.
 */
static void _kbinding_load_config(kbinding_t *kbt, char *file) {
  kbinding_file_t *kbdf;
  
  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(file);

  kbdf = (kbinding_file_t *) xine_xmalloc(sizeof(kbinding_file_t));
  kbdf->bindingfile = strdup(file);

  if((kbdf->fd = fopen(kbdf->bindingfile, "r")) != NULL) {
    
    _kbindings_get_next_line(kbdf);

    while(kbdf->ln != NULL) {

      if(_kbindings_begin_section(kbdf)) {
	_kbindings_parse_section(kbt, kbdf);
      }
      
      _kbindings_get_next_line(kbdf);
    }
    fclose(kbdf->fd);
  }

  SAFE_FREE(kbdf->bindingfile);
  SAFE_FREE(kbdf);
}

static void _kbindings_display_kbindings_to_stream(kbinding_t *kbt, int mode, FILE *stream) {
  char               buf[256];
  kbinding_entry_t **k;
  int          i;

  if(kbt == NULL) {
    xine_error(_("OOCH: key binding table is NULL.\n"));
    return;
  }

  switch(mode) {
  case LIRC_DISPLAY_MODE:
    fprintf(stream, "##\n# xine key bindings.\n");
    fprintf(stream, "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);
    
    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      if(!k[i]->is_alias) {
	sprintf(buf, "# %s\n", k[i]->comment);
	sprintf(buf, "%sbegin\n", buf);
	sprintf(buf, "%s\tremote = xxxxx\n", buf);
	sprintf(buf, "%s\tbutton = xxxxx\n", buf);
	sprintf(buf, "%s\tprog   = xine\n", buf);
	sprintf(buf, "%s\trepeat = 0\n", buf);
	sprintf(buf, "%s\tconfig = %s\n", buf, k[i]->action);
	sprintf(buf, "%send\n\n", buf);
	fprintf(stream, buf);
	memset(&buf, 0, sizeof(buf));
      }
    }
    fprintf(stream, "##\n# End of xine key bindings.\n##\n");
    break;
    
  case DEFAULT_DISPLAY_MODE:
  default:
    fprintf(stream, "##\n# xine key bindings.\n");
    fprintf(stream, "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);

    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      sprintf(buf, "# %s\n", k[i]->comment);

      if(k[i]->is_alias) {
	sprintf(buf, "%sAlias {\n", buf);
	sprintf(buf, "%s\tentry = %s\n", buf, k[i]->action);
      }
      else
	sprintf(buf, "%s%s {\n", buf, k[i]->action);

      sprintf(buf, "%s\tkey = %s\n", buf, k[i]->key);
      sprintf(buf, "%s%s", buf, "\tmodifier = ");
      if(k[i]->modifier == KEYMOD_NOMOD)
	sprintf(buf, "%s%s, ", buf, "none");
      if(k[i]->modifier & KEYMOD_CONTROL)
	sprintf(buf, "%s%s, ", buf, "control");
      if(k[i]->modifier & KEYMOD_META)
	sprintf(buf, "%s%s, ", buf, "meta");
      if(k[i]->modifier & KEYMOD_MOD3)
	sprintf(buf, "%s%s, ", buf, "mod3");
      if(k[i]->modifier & KEYMOD_MOD4)
	sprintf(buf, "%s%s, ", buf, "mod4");
      if(k[i]->modifier & KEYMOD_MOD5)
	sprintf(buf, "%s%s, ", buf, "mod5");
      buf[strlen(buf) - 2] = '\n';
      buf[strlen(buf) - 1] = '\0';
      sprintf(buf, "%s%s", buf, "}\n\n");
      fprintf(stream, "%s", buf);
      memset(&buf, 0, sizeof(buf));
    }
    fprintf(stream, "##\n# End of xine key bindings.\n##\n");
    break;
  }
}
/*
 * Display all key bindings from kbt key binding table.
 */
static void _kbindings_display_kbindings(kbinding_t *kbt, int mode) {
  _kbindings_display_kbindings_to_stream(kbt, mode, stdout);  
}

/*
 * Convert a modifier to key binding modifier style.
 */
static void kbindings_convert_modifier(int mod, int *modifier) {
  *modifier = KEYMOD_NOMOD;
  
  ABORT_IF_NULL(modifier);

  if(mod & MODIFIER_NOMOD)
    *modifier = KEYMOD_NOMOD;
  if(mod & MODIFIER_CTRL)
    *modifier |= KEYMOD_CONTROL;
  if(mod & MODIFIER_META)
    *modifier |= KEYMOD_META;
  if(mod & MODIFIER_MOD3)
    *modifier |= KEYMOD_MOD3;
  if(mod & MODIFIER_MOD4)
    *modifier |= KEYMOD_MOD4;
  if(mod & MODIFIER_MOD5)
    *modifier |= KEYMOD_MOD5;
}

/*
 * Create a new key binding table from (static) default one.
 */
static kbinding_t *_kbindings_init_to_default(void) {
  kbinding_t  *kbt;
  int          i;
 
  kbt = (kbinding_t *) xine_xmalloc(sizeof(kbinding_t));

  for(i = 0; default_binding_table[i].action != NULL; i++) {
    kbt->entry[i] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_entry_t));
    kbt->entry[i]->comment = strdup(default_binding_table[i].comment);
    kbt->entry[i]->action = strdup(default_binding_table[i].action);
    kbt->entry[i]->action_id = default_binding_table[i].action_id;
    kbt->entry[i]->key = strdup(default_binding_table[i].key);
    kbt->entry[i]->modifier = default_binding_table[i].modifier;
  }
  kbt->entry[i] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_entry_t));
  kbt->entry[i]->comment = NULL;
  kbt->entry[i]->action = NULL;
  kbt->entry[i]->action_id = 0;
  kbt->entry[i]->key = NULL;
  kbt->entry[i]->modifier = 0;
  
  kbt->num_entries = i+1;

  return kbt;
}

/*
 * Initialize a key binding table from default, then try
 * to remap this one with (system/user) remap files.
 */
kbinding_t *kbindings_init_kbinding(void) {
  kbinding_t *kbt = NULL;

  kbt = _kbindings_init_to_default();
  
  _kbinding_load_config(kbt, gGui->keymap_file);
  
  /* Just to check is there redundant entries, and inform user */
  _kbindings_check_redundancy(kbt);

  return kbt;
}

/*
 * Save current key binding table to keymap file.
 */
void kbindings_save_kbinding(kbinding_t *kbt) {
  FILE   *f;
  
  if((f = fopen(gGui->keymap_file, "w+")) == NULL) {
    return;
  }
  else {
    _kbindings_display_kbindings_to_stream(kbt, DEFAULT_DISPLAY_MODE, f);  
    fclose(f);
  }
}

/*
 * Free key binding table kbt, then set it to default table.
 */
void kbindings_reset_kbinding(kbinding_t **kbt) {
  kbinding_t *k;
  
  ABORT_IF_NULL(*kbt);
  
  _kbindings_free_bindings(*kbt);
  k = _kbindings_init_to_default();
  *kbt = k;
}

/*
 * Freeing key binding table, then NULLify it.
 */
void kbindings_free_kbinding(kbinding_t **kbt) {

  ABORT_IF_NULL(*kbt);

  _kbindings_free_bindings(*kbt);
  *kbt = NULL;
}

/*
 * Return a duplicated table from kbt.
 */
kbinding_t *kbindings_duplicate_kbindings(kbinding_t *kbt) {
  int         i;
  kbinding_t *k;
  
  ABORT_IF_NULL(kbt);
  
  k = (kbinding_t *) xine_xmalloc(sizeof(kbinding_t));
  
  for(i = 0; kbt->entry[i]->action != NULL; i++) {
    k->entry[i]            = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_entry_t));
    k->entry[i]->comment   = strdup(kbt->entry[i]->comment);
    k->entry[i]->action    = strdup(kbt->entry[i]->action);
    k->entry[i]->action_id = kbt->entry[i]->action_id;
    k->entry[i]->key       = strdup(kbt->entry[i]->key);
    k->entry[i]->modifier  = kbt->entry[i]->modifier;
    k->entry[i]->is_alias  = kbt->entry[i]->is_alias;
  }

  k->entry[i]            = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_t));
  k->entry[i]->comment   = NULL;
  k->entry[i]->action    = NULL;
  k->entry[i]->action_id = 0;
  k->entry[i]->key       = NULL;
  k->entry[i]->modifier  = 0;
  k->entry[i]->is_alias  = 0;
  k->num_entries         = i + 1;
  
  return k;
}

/*
 * This could be used to create a default key binding file
 * with 'xine --keymap > $HOME/.xine_keymap'
 */
void kbindings_display_default_bindings(void) {
  kbinding_t *kbt = NULL;
  
  kbt = _kbindings_init_to_default();
  
  _kbindings_display_kbindings(kbt, DEFAULT_DISPLAY_MODE);
  _kbindings_free_bindings(kbt);
}

/*
 * This could be used to create a default key binding file
 * with 'xine --keymap=lirc > $HOME/.lircrc'
 */
void kbindings_display_default_lirc_bindings(void) {
  kbinding_t *kbt = NULL;
  
  kbt = _kbindings_init_to_default();
  
  _kbindings_display_kbindings(kbt, LIRC_DISPLAY_MODE);
  _kbindings_free_bindings(kbt);
}

/*
 * This could be used to create a key binding file from key binding 
 * object k, with 'xine --keymap > $HOME/.xine_keymap'
 */
void kbindings_display_current_bindings(kbinding_t *kbt) {

  ABORT_IF_NULL(kbt);

  _kbindings_display_kbindings(kbt, DEFAULT_DISPLAY_MODE);
}

/*
 * Return action id from key binding kbt entry.
 */
action_id_t kbindings_get_action_id(kbinding_entry_t *kbt) {

  if(kbt == NULL)
    return ACTID_NOKEY;

  return kbt->action_id;
}

/*
 * Return a key binding entry (if available) matching with action string.
 */
kbinding_entry_t *kbindings_lookup_action(kbinding_t *kbt, const char *action) {
  kbinding_entry_t  *k;
  int                i;

  if((action == NULL) || (kbt == NULL))
    return NULL;

  /* CHECKME: Not case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if(!strcasecmp(k->action, action))
      return k;
  }

  /* Unknown action */
  return NULL;
}

/*
 * Try to find and entry in key binding table matching with key and modifier value.
 */
static kbinding_entry_t *kbindings_lookup_binding(kbinding_t *kbt, const char *key, int modifier) {
  kbinding_entry_t *k;
  int               i;

  if((key == NULL) || (kbt == NULL))
    return NULL;


#ifdef TRACE_KBINDINGS
  printf("Looking for: '%s' [", key);
  if(modifier == KEYMOD_NOMOD)
    printf("none, ");
  if(modifier & KEYMOD_CONTROL)
    printf("control, ");
  if(modifier & KEYMOD_META)
    printf("meta, ");
  if(modifier & KEYMOD_MOD3)
    printf("mod3, ");
  if(modifier & KEYMOD_MOD4)
    printf("mod4, ");
  if(modifier & KEYMOD_MOD5)
    printf("mod5, ");
  printf("\b\b]\n");
#endif

  /* Be case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if((!(strcmp(k->key, key))) && (modifier == k->modifier))
      return k;
  }

  /* Not case sensitive */
  /*
  for(i = 0, k = kbt[0]; kbt[i]->action != NULL; i++, k = kbt[i]) {
    if((!(strcasecmp(k->key, key))) && (modifier == k->modifier))
      return k;
  }
  */
  /* Last chance */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if((!(strcmp(k->key, key))) && (k->modifier == KEYMOD_NOMOD))
      return k;
  }

  /* Keybinding unknown */
  return NULL;
}

/*
 * Handle key event from an XEvent.
 */
void kbindings_handle_kbinding(kbinding_t *kbt, XEvent *event) {

  if((kbt == NULL) || (event == NULL))
    return;

  switch(event->type) {
    
  case ButtonRelease: {
    kbinding_entry_t    *k;
    char                 xbutton[256];
    int                  mod, modifier;

    (void) xitk_get_key_modifier(event, &mod);
    kbindings_convert_modifier(mod, &modifier);
    
    memset(&xbutton, 0, sizeof(xbutton));
    snprintf(xbutton, 255, "XButton_%d", event->xbutton.button);

#ifdef TRACE_KBINDINGS
    printf("ButtonRelease: %s, modifier: %d\n", xbutton, modifier);
#endif    

    k = kbindings_lookup_binding(kbt, xbutton, modifier);
    
    if(k) {
      gui_execute_action_id(k->action_id);
    }
#if 0  /* DEBUG */
    else
      printf("%s unhandled\n", kbuf);
#endif    
  }
  break;

  case KeyPress: {
    KeySym               mkey;
    int                  mod, modifier;
    kbinding_entry_t    *k;

    (void) xitk_get_key_modifier(event, &mod);
    kbindings_convert_modifier(mod, &modifier);
    mkey = xitk_get_key_pressed(event);

#ifdef TRACE_KBINDINGS
    printf("KeyPress: %s, modifier: %d\n", (XKeysymToString(mkey)), modifier);
#endif    
    k = kbindings_lookup_binding(kbt, XKeysymToString(mkey), modifier);
    XUnlockDisplay (gGui->display);
    
    if(k)
      gui_execute_action_id(k->action_id);
#if 0  /* DEBUG */
    else
      printf("%s unhandled\n", kbuf);
#endif
    
  }
  break;
  }
}

/*
 * ***** Key Binding Editor ******
 */
/*
 * return 1 if key binding editor is ON
 */
int kbedit_is_running(void) {

  if(kbedit != NULL)
    return kbedit->running;

  return 0;
}

/*
 * Return 1 if setup panel is visible
 */
int kbedit_is_visible(void) {

  if(kbedit != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(kbedit->xwin));
    else
      return kbedit->visible;
  }

  return 0;
}

/*
 * Raise kbedit->xwin
 */
void kbedit_raise_window(void) {
  
  if(kbedit != NULL)
    raise_window(xitk_window_get_window(kbedit->xwin), kbedit->visible, kbedit->running);
}

/*
 * Hide/show the kbedit panel
 */
void kbedit_toggle_visibility (xitk_widget_t *w, void *data) {
  if(kbedit != NULL)
    toggle_window(xitk_window_get_window(kbedit->xwin), kbedit->widget_list,
		  &kbedit->visible, kbedit->running);
}

static void kbedit_create_browser_entries(void) {
  int i, j;
  
  if(kbedit->num_entries) {
    for(i = 0; i < kbedit->num_entries; i++)
      free((char *)kbedit->entries[i]);
    free((char **)kbedit->entries);

  }

  kbedit->entries = (char **) xine_xmalloc(sizeof(char *) * (kbedit->kbt->num_entries + 1));
  kbedit->num_entries = (kbedit->kbt->num_entries - 1);
  
  for(i = 0; i < kbedit->num_entries; i++) {
    char buf[256];
    char shortcut[256];

    memset(&buf, 0, sizeof(buf));
    memset(&shortcut, 0, sizeof(shortcut));
    
    if(kbedit->kbt->entry[i]->is_alias)
      sprintf(buf, "@{%s}", kbedit->kbt->entry[i]->comment);
    else
      sprintf(buf, "%s", kbedit->kbt->entry[i]->comment);
    
    sprintf(shortcut, "%2s", "[ ");
    
    if(kbedit->kbt->entry[i]->modifier != KEYMOD_NOMOD) {
      
      if(kbedit->kbt->entry[i]->modifier & KEYMOD_CONTROL)
	sprintf(shortcut, "%s%c", shortcut, 'C');
      if(kbedit->kbt->entry[i]->modifier & KEYMOD_META)
	sprintf(shortcut, "%s%c", shortcut, 'M');
      if(kbedit->kbt->entry[i]->modifier & KEYMOD_MOD3)
	sprintf(shortcut, "%s%2s", shortcut, "M3");
      if(kbedit->kbt->entry[i]->modifier & KEYMOD_MOD4)
	sprintf(shortcut, "%s%2s", shortcut, "M4");
      if(kbedit->kbt->entry[i]->modifier & KEYMOD_MOD5)
	sprintf(shortcut, "%s%2s", shortcut, "M5");

      sprintf(shortcut, "%s%c", shortcut, '-');
    }

    sprintf(shortcut, "%s%s ]", shortcut, kbedit->kbt->entry[i]->key);

    /* Right align shortcuts */
    j = 78 - (strlen(buf) + strlen(shortcut));
    while((j--))
      sprintf(buf, "%s%c", buf, ' ');
    
    sprintf(buf, "%s%s", buf, shortcut);
    
    kbedit->entries[i] = strdup(buf);
  }
  kbedit->entries[i] = NULL;
}

static void kbedit_display_kbinding(char *action, kbinding_entry_t *kbe) {

  if(action && kbe) {

    xitk_label_change_label(kbedit->comment, action);
    xitk_label_change_label(kbedit->key, kbe->key);
    
    xitk_checkbox_set_state(kbedit->ctrl, (kbe->modifier & KEYMOD_CONTROL) ? 1 : 0);
    xitk_checkbox_set_state(kbedit->meta, (kbe->modifier & KEYMOD_META) ? 1 : 0);
    xitk_checkbox_set_state(kbedit->mod3, (kbe->modifier & KEYMOD_MOD3) ? 1 : 0);
    xitk_checkbox_set_state(kbedit->mod4, (kbe->modifier & KEYMOD_MOD4) ? 1 : 0);
    xitk_checkbox_set_state(kbedit->mod5, (kbe->modifier & KEYMOD_MOD5) ? 1 : 0);
  }
}

/*
 *
 */
static void kbedit_select(int s) {

  xitk_enable_and_show_widget(kbedit->alias);
  xitk_enable_and_show_widget(kbedit->edit);
  xitk_enable_and_show_widget(kbedit->delete);
  
  kbedit->ksel = kbedit->kbt->entry[s];
  kbedit_display_kbinding(kbedit->entries[s], kbedit->kbt->entry[s]);
}

/*
 *
 */
static void kbedit_unset(void) {

  if(xitk_labelbutton_get_state(kbedit->alias))
    xitk_labelbutton_set_state(kbedit->alias, 0);
  
  if(xitk_labelbutton_get_state(kbedit->edit))
    xitk_labelbutton_set_state(kbedit->edit, 0);
  
  xitk_disable_widget(kbedit->alias);
  xitk_disable_widget(kbedit->edit);
  xitk_disable_widget(kbedit->delete);
  xitk_disable_widget(kbedit->grab);
  kbedit->ksel = NULL;

  xitk_label_change_label(kbedit->comment, _("Nothing selected"));
  xitk_label_change_label(kbedit->key, _("None"));
  
  xitk_checkbox_set_state(kbedit->ctrl, 0);
  xitk_checkbox_set_state(kbedit->meta, 0);
  xitk_checkbox_set_state(kbedit->mod3, 0);
  xitk_checkbox_set_state(kbedit->mod4, 0);
  xitk_checkbox_set_state(kbedit->mod5, 0);
}

/*
 * Check for redundancy.
 * return: -2 on failure (null pointer passed)
 *         -1 on success
 *         >=0 if a redundant entry found (bkt array entry num).
 */
static int bkedit_check_redundancy(kbinding_t *kbt, kbinding_entry_t *kbe) {
  int ret = -1;
  
  if(kbt && kbe) {
    int i;
    
    for(i = 0; kbt->entry[i]->action != NULL; i++) {
      if((!strcmp(kbt->entry[i]->key, kbe->key)) &&
	 (kbt->entry[i]->modifier == kbe->modifier) &&
	 (strcasecmp(kbt->entry[i]->key, "void"))) {
	return i;
      }
    }
  }
  else
    ret = -2;
  
  return ret;
}

/*
 *
 */
void kbedit_exit(xitk_widget_t *w, void *data) {
  
  if(kbedit) {
    window_info_t wi;
    
    kbedit->running = 0;
    kbedit->visible = 0;
    
    if((xitk_get_window_info(kbedit->kreg, &wi))) {
      config_update_num("gui.kbedit_x", wi.x);
      config_update_num("gui.kbedit_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&kbedit->kreg);
    
    xitk_destroy_widgets(kbedit->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, kbedit->xwin);
    
    kbedit->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(kbedit->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(kbedit->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(kbedit->widget_list);
    
    {
      int i;
      
      for(i = 0; i < kbedit->num_entries; i++)
	free((char *)kbedit->entries[i]);
      
      free((char **)kbedit->entries);
    }
    
    kbindings_free_kbinding(&kbedit->kbt);
    
    free(kbedit);
    kbedit = NULL;
  }
}

/*
 *
 */
static void kbedit_sel(xitk_widget_t *w, void *data, int s) {

  if(s >= 0)
    kbedit_select(s);
}

/*
 * Create an alias from the selected entry.
 */
static void kbedit_alias(xitk_widget_t *w, void *data, int state) {

  xitk_labelbutton_set_state(kbedit->edit, 0);

  if(state) {
    xitk_enable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_ALIASING;
  }
  else {
    xitk_disable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_NOOP;
  }
}

/*
 * Change shortcut, should take care about reduncancy.
 */
static void kbedit_edit(xitk_widget_t *w, void *data, int state) {

  xitk_labelbutton_set_state(kbedit->alias, 0);

  if(state) {
    xitk_enable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_EDITING;
  }
  else {
    xitk_disable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_NOOP;
  }

}

/*
 * Remove selected entry, but alias ones only.
 */
static void kbedit_delete(xitk_widget_t *w, void *data) {
  int s = xitk_browser_get_current_selected(kbedit->browser);

  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);
  
  /* We can delete alias entries, only */
  if(s >= 0) {
    if(kbedit->kbt->entry[s]->is_alias) {
      int i;
      xitk_browser_release_all_buttons(kbedit->browser);
      
      free(kbedit->kbt->entry[s]->comment);
      free(kbedit->kbt->entry[s]->action);
      free(kbedit->kbt->entry[s]->key);
      free(kbedit->kbt->entry[s]);
      
      for(i = s; s < kbedit->num_entries; s++)
	kbedit->kbt->entry[s] = kbedit->kbt->entry[s + 1];
      
      kbedit->kbt->num_entries--;
      
      kbedit_create_browser_entries();
      
      xitk_browser_update_list(kbedit->browser, 
			       (const char* const*) kbedit->entries, kbedit->num_entries, 0);
    }
    else {
      xine_error(_("You can only delete alias entries."));
    }
  }
}

/*
 * Reset to xine's default table.
 */
static void kbedit_reset(xitk_widget_t *w, void *data) {

  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);

  kbindings_reset_kbinding(&kbedit->kbt);
  kbedit_create_browser_entries();
  xitk_browser_update_list(kbedit->browser, 
			   (const char* const*) kbedit->entries, kbedit->num_entries, 0);
}

/*
 * Save keymap file, then set global table to hacked one 
 */
static void kbedit_save(xitk_widget_t *w, void *data) {
  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);
  
  kbindings_free_kbinding(&gGui->kbindings);
  gGui->kbindings = kbindings_duplicate_kbindings(kbedit->kbt);
  kbindings_save_kbinding(gGui->kbindings);
}

/*
 * Forget and dismiss kbeditor 
 */
static void kbedit_done(xitk_widget_t *w, void *data) {
  kbedit_exit(NULL, NULL);
}

static void kbedit_accept_yes(xitk_widget_t *w, void *data, int state) {
  kbinding_entry_t *kbe = (kbinding_entry_t *) data;
  
  switch(kbedit->action_wanted) {
    
  case KBEDIT_ALIASING:
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->comment = strdup(kbedit->ksel->comment);
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->action = strdup(kbedit->ksel->action);
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->action_id = kbedit->ksel->action_id;
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->key = strdup(kbe->key);
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->modifier = kbe->modifier;
    kbedit->kbt->entry[kbedit->kbt->num_entries - 1]->is_alias = 1;
    
    kbedit->kbt->entry[kbedit->kbt->num_entries] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_t));
    kbedit->kbt->entry[kbedit->kbt->num_entries]->comment = NULL;
    kbedit->kbt->entry[kbedit->kbt->num_entries]->action = NULL;
    kbedit->kbt->entry[kbedit->kbt->num_entries]->action_id = 0;
    kbedit->kbt->entry[kbedit->kbt->num_entries]->key = NULL;
    kbedit->kbt->entry[kbedit->kbt->num_entries]->modifier = 0;
    kbedit->kbt->entry[kbedit->kbt->num_entries]->is_alias = 0;
    
    kbedit->kbt->num_entries++;
    
    kbedit_create_browser_entries();
    xitk_browser_update_list(kbedit->browser, 
			     (const char* const*) kbedit->entries, kbedit->num_entries, 0);
    break;
    
  case KBEDIT_EDITING:
    kbedit->ksel->key = (char *) realloc(kbedit->ksel->key, sizeof(char *) * (strlen(kbe->key) + 1));
    sprintf(kbedit->ksel->key, "%s", kbe->key);
    kbedit->ksel->modifier = kbe->modifier;
    
    kbedit_create_browser_entries();
    xitk_browser_update_list(kbedit->browser, 
			     (const char* const*) kbedit->entries, kbedit->num_entries, 0);
    break;
  }

  kbedit->action_wanted = KBEDIT_NOOP;
  
  SAFE_FREE(kbe->comment);
  SAFE_FREE(kbe->action);
  SAFE_FREE(kbe->key);
  SAFE_FREE(kbe);
  kbedit_unset();
}

static void kbedit_accept_no(xitk_widget_t *w, void *data, int state) {
  kbinding_entry_t *kbe = (kbinding_entry_t *) data;
  
  kbedit->action_wanted = KBEDIT_NOOP;
  
  xitk_browser_update_list(kbedit->browser, 
			   (const char* const*) kbedit->entries, kbedit->num_entries, 0);
  SAFE_FREE(kbe->comment);
  SAFE_FREE(kbe->action);
  SAFE_FREE(kbe->key);
  SAFE_FREE(kbe);
  kbedit_unset();
}

/*
 * Grab key binding.
 */
static void kbedit_grab(xitk_widget_t *w, void *data) {
  char              *olbl;
  char              *action;
  XEvent             xev;
  int                mod, modifier;
  xitk_window_t     *xwin;
  kbinding_entry_t  *kbe;
  int                redundant;
  
  /* We are already grabbing keybinding */
  if(kbedit->grabbing)
    return;

  xine_strdupa(olbl, (xitk_labelbutton_get_label(kbedit->grab)));
 
  kbedit->grabbing = 1;
  
  kbe = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_entry_t));
  kbe->comment   = strdup(kbedit->ksel->comment);
  kbe->action    = strdup(kbedit->ksel->action);
  kbe->action_id = kbedit->ksel->action_id;
  kbe->is_alias  = kbedit->ksel->is_alias;
  
  xitk_labelbutton_change_label(kbedit->grab, _("Press Keyboard Keys..."));
  XLockDisplay(gGui->display);
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);

  {
    int x, y, w, h;

    
    xitk_get_window_position(gGui->display, 
			     (xitk_window_get_window(kbedit->xwin)), &x, &y, &w, &h);
        
    xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
					    _("Event receiver window"), x, y, w, h);

    set_window_states_start((xitk_window_get_window(xwin)));
  }
  
  XLockDisplay(gGui->display);
  XRaiseWindow(gGui->display, (xitk_window_get_window(xwin)));
  XMapWindow(gGui->display, (xitk_window_get_window(xwin)));
  XUnlockDisplay(gGui->display);

  try_to_set_input_focus(xitk_window_get_window(xwin));

  do {
    XMaskEvent(gGui->display, ButtonReleaseMask | KeyReleaseMask, &xev);
    XMapRaised(gGui->display, (xitk_window_get_window(xwin)));
  } while ((xev.type != KeyRelease && xev.type != ButtonRelease) ||
	   xev.xmap.event != xitk_window_get_window(xwin));
  
  (void) xitk_get_key_modifier(&xev, &mod);
  kbindings_convert_modifier(mod, &modifier);

  kbe->modifier = modifier;
  
  if(xev.type == KeyRelease) {  
    XKeyEvent mykeyevent;
    KeySym    mykey;
    char      kbuf[256];
    int       len;
    
    mykeyevent = xev.xkey;
   
    XLockDisplay (gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    kbe->key = strdup((XKeysymToString(mykey)));
    XUnlockDisplay (gGui->display);

  }
  else if(xev.type == ButtonRelease) {
    char  xbutton[256];
    
    memset(&xbutton, 0, sizeof(xbutton));
    snprintf(xbutton, 255, "XButton_%d", xev.xbutton.button);
    kbe->key = strdup(xbutton);
    kbe->modifier &= 0xFFFFFFEF;
  }
  
  xitk_labelbutton_change_label(kbedit->grab, olbl);

  xitk_window_destroy_window(gGui->imlib_data, xwin);
  
  XLockDisplay(gGui->display);
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);

  kbedit->grabbing = 0;
  
  if((redundant = bkedit_check_redundancy(kbedit->kbt, kbe)) == -1) {
    char shortcut[256];
    
    xine_strdupa(action, (xitk_label_get_label(kbedit->comment)));
    kbedit_display_kbinding(action, kbe);
    
    memset(&shortcut, 0, sizeof(shortcut));
    sprintf(shortcut, "%2s", "[ ");
    
    if(kbe->modifier != KEYMOD_NOMOD) {
      
      if(kbe->modifier & KEYMOD_CONTROL)
	sprintf(shortcut, "%s%c", shortcut, 'C');
      if(kbe->modifier & KEYMOD_META)
	sprintf(shortcut, "%s%c", shortcut, 'M');
      if(kbe->modifier & KEYMOD_MOD3)
	sprintf(shortcut, "%s%2s", shortcut, "M3");
      if(kbe->modifier & KEYMOD_MOD4)
	sprintf(shortcut, "%s%2s", shortcut, "M4");
      if(kbe->modifier & KEYMOD_MOD5)
	sprintf(shortcut, "%s%2s", shortcut, "M5");

      sprintf(shortcut, "%s%c", shortcut, '-');
    }
    
    sprintf(shortcut, "%s%s ]", shortcut, kbe->key);

    /* Ask if uses want/wont store new shorcut */
    xitk_window_dialog_yesno_with_width(gGui->imlib_data, _("Accept?"),
					kbedit_accept_yes, 
					kbedit_accept_no, 
					(void *) kbe, 400, ALIGN_CENTER,
					_("Store %s as\n'%s' key binding ?."),
					shortcut, kbedit->ksel->comment);

  }
  else {
    /* error, redundant */
    if(redundant >= 0) {
      xine_error(_("This key bindings is redundant with action:\n\"%s\".\n"),
		 kbedit->kbt->entry[redundant]->comment);
    }
  }

  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);
}

/*
 *
 */
static void kbedit_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {
    
  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      kbedit_exit(NULL, NULL);
    break;

  case KeyRelease: {
    xitk_widget_t *w;

    if((!kbedit) || (kbedit && !kbedit->widget_list))
      return;
    
    w = xitk_get_focused_widget(kbedit->widget_list);

    if((w && (w != kbedit->done) && (w != kbedit->save))
       && (xitk_browser_get_current_selected(kbedit->browser) < 0)) {
      kbedit_unset();
    }
  }
  break;

  case ButtonRelease: {
    xitk_widget_t *w;

    if((!kbedit) || (kbedit && !kbedit->widget_list))
      return;

    w = xitk_get_focused_widget(kbedit->widget_list);
    
    if((w && (w != kbedit->done) && (w != kbedit->save))
       && (xitk_browser_get_current_selected(kbedit->browser) < 0)) {
      kbedit_unset();
    }

  }
  break;
  
  }
}

void kbedit_reparent(void) {
  if(kbedit)
    reparent_window((xitk_window_get_window(kbedit->xwin)));
}

/*
 *
 */
void kbedit_window(void) {
  int                        x, y;
  GC                         gc;
  xitk_pixmap_t             *bg;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        l;
  xitk_browser_widget_t      br;
  xitk_checkbox_widget_t     cb;
  int                        btnw = 80;
  int                        lbear, rbear, wid, asc, des;
  xitk_font_t               *fs;
  xitk_widget_t             *w;
  
  x = xine_config_register_num(gGui->xine, "gui.kbedit_x", 
			       100, 
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  y = xine_config_register_num(gGui->xine, "gui.kbedit_y",
			       100,
			       CONFIG_NO_DESC,
			       CONFIG_NO_HELP,
			       CONFIG_LEVEL_DEB,
			       CONFIG_NO_CB,
			       CONFIG_NO_DATA);
  
  kbedit = (_kbedit_t *) xine_xmalloc(sizeof(_kbedit_t));

  kbedit->kbt           = kbindings_duplicate_kbindings(gGui->kbindings);
  kbedit->action_wanted = KBEDIT_NOOP;
  kbedit->xwin          = xitk_window_create_dialog_window(gGui->imlib_data,
							   _("key binding editor"), 
							   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  set_window_states_start((xitk_window_get_window(kbedit->xwin)));

  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(kbedit->xwin)), None, None);
  XUnlockDisplay (gGui->display);

  kbedit->widget_list      = xitk_widget_list_new();

  xitk_widget_list_set(kbedit->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(kbedit->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(kbedit->xwin)));
  xitk_widget_list_set(kbedit->widget_list, WIDGET_LIST_GC, gc);
  
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(kbedit->xwin)), bg->pixmap,
	    bg->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay (gGui->display);
  
  x = 15;
  y = 35;
  
  draw_rectangular_inner_box(gGui->imlib_data, bg, x, y,
			     (WINDOW_WIDTH - 30), (20 * (MAX_DISP_ENTRIES + 1)) - 2);

  y = y + (20 * (MAX_DISP_ENTRIES + 1)) + 45;
  draw_outter_frame(gGui->imlib_data, bg, 
		    _("Binding action"), fontname, 
		    x, y, 
		    (WINDOW_WIDTH - 30), 45);

  y += 50;
  draw_outter_frame(gGui->imlib_data, bg, 
		    _("Key"), fontname, 
		    x, y, 
		    120, 45);

  draw_outter_frame(gGui->imlib_data, bg, 
		    _("Modifiers"), fontname, 
		    x + 130, y, 
		    (WINDOW_WIDTH - (x + 130) - 15), 45);


  xitk_window_change_background(gGui->imlib_data, kbedit->xwin, bg->pixmap,
				WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_image_destroy_xitk_pixmap(bg);

  kbedit_create_browser_entries();

  y = 35;

  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  
  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = kbedit->num_entries;
  br.browser.entries               = (const char* const*)kbedit->entries;
  br.callback                      = kbedit_sel;
  br.dbl_click_callback            = NULL;
  br.parent_wlist                  = kbedit->widget_list;
  br.userdata                      = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
			   (kbedit->browser = 
			    xitk_noskin_browser_create(kbedit->widget_list, &br,
						       (XITK_WIDGET_LIST_GC(kbedit->widget_list)),
						       x + 1, y + 1, 
						       WINDOW_WIDTH - 30 - 2 - 16, 20,
						       16, br_fontname)));
  xitk_enable_and_show_widget(kbedit->browser);

  xitk_browser_set_alignment(kbedit->browser, ALIGN_LEFT);
  xitk_browser_update_list(kbedit->browser, 
			   (const char* const*) kbedit->entries, kbedit->num_entries, 0);

  y = (WINDOW_HEIGHT - 160);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  
  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Alias");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = kbedit_alias;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->alias = 
		    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
						   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->alias);
  
  x += btnw + 2;

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Edit");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = kbedit_edit;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->edit = 
		    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
						   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->edit);
  
  x += btnw + 2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Delete");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_delete; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->delete = 
		    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
						   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->delete);

  x += btnw + 2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_save; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->save = 
		    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
						   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->save);

  x += btnw + 2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Reset");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_reset; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
				       "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(w);

  x += btnw + 2;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Done");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_done; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->done = 
		    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, btnw, 23,
						   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->done);

  x = 15;
  y += 30;
  
  XITK_WIDGET_INIT(&l, gGui->imlib_data);

  fs = xitk_font_load_font(gGui->display, fontname);
  xitk_font_set_font(fs, (XITK_WIDGET_LIST_GC(kbedit->widget_list)));
  xitk_font_string_extent(fs, FONT_HEIGHT_MODEL, &lbear, &rbear, &wid, &asc, &des);
  xitk_font_unload_font(fs);
  
  y += (asc+des) + 2;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = FONT_HEIGHT_MODEL;
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->comment = 
		    xitk_noskin_label_create(kbedit->widget_list, &l, x + 1 + 10, y, 
					     WINDOW_WIDTH - 50 - 2, (asc+des), fontname)));
  xitk_enable_and_show_widget(kbedit->comment);

  y += 50;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = "THE Key";
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
		   (kbedit->key = xitk_noskin_label_create(kbedit->widget_list, &l, x + 10, y, 
							   100, (asc+des), fontname)));
  xitk_enable_and_show_widget(kbedit->key);
  
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  x += 130 + 10;

  cb.callback          = NULL;
  cb.userdata          = NULL;
  cb.skin_element_name = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(kbedit->widget_list)),
			    (kbedit->ctrl = 
			     xitk_noskin_checkbox_create(kbedit->widget_list, &cb, x, y, 10, 10)));
  xitk_show_widget(kbedit->ctrl);
  xitk_disable_widget(kbedit->ctrl);


  x += 15;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = _("ctrl");
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_label_create(kbedit->widget_list, &l, x, y - (((asc+des) - 10)>>1), 
				 40, (asc+des), fontname)));
  xitk_enable_and_show_widget(w);

  x += 55;

  cb.callback          = NULL;
  cb.userdata          = NULL;
  cb.skin_element_name = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(kbedit->widget_list)),
			    (kbedit->meta = 
			     xitk_noskin_checkbox_create(kbedit->widget_list, &cb, x, y, 10, 10)));
  xitk_show_widget(kbedit->meta);
  xitk_disable_widget(kbedit->meta);


  x += 15;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = _("meta");
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_label_create(kbedit->widget_list, &l, x, y - (((asc+des) - 10)>>1), 
				 40, (asc+des), fontname)));
  xitk_enable_and_show_widget(w);

  x += 55;

  cb.callback          = NULL;
  cb.userdata          = NULL;
  cb.skin_element_name = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(kbedit->widget_list)),
			    (kbedit->mod3 = 
			     xitk_noskin_checkbox_create(kbedit->widget_list, &cb, x, y, 10, 10)));
  xitk_show_widget(kbedit->mod3);
  xitk_disable_widget(kbedit->mod3);


  x += 15;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = _("mod3");
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_label_create(kbedit->widget_list, &l, x, y - (((asc+des) - 10)>>1), 
				 40, (asc+des), fontname)));
  xitk_enable_and_show_widget(w);

  x += 55;

  cb.callback          = NULL;
  cb.userdata          = NULL;
  cb.skin_element_name = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(kbedit->widget_list)),
			    (kbedit->mod4 = 
			     xitk_noskin_checkbox_create(kbedit->widget_list, &cb, x, y, 10, 10)));
  xitk_show_widget(kbedit->mod4);
  xitk_disable_widget(kbedit->mod4);


  x += 15;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = _("mod4");
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_label_create(kbedit->widget_list, &l, x, y - (((asc+des) - 10)>>1), 
				 40, (asc+des), fontname)));
  xitk_enable_and_show_widget(w);

  x += 55;

  cb.callback          = NULL;
  cb.userdata          = NULL;
  cb.skin_element_name = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(kbedit->widget_list)),
			    (kbedit->mod5 = 
			     xitk_noskin_checkbox_create(kbedit->widget_list, &cb, x, y, 10, 10)));
  xitk_show_widget(kbedit->mod5);
  xitk_disable_widget(kbedit->mod5);


  x += 15;

  l.window            = (XITK_WIDGET_LIST_WINDOW(kbedit->widget_list));
  l.gc                = (XITK_WIDGET_LIST_GC(kbedit->widget_list));
  l.skin_element_name = NULL;
  l.label             = _("mod5");
  l.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
   (w = xitk_noskin_label_create(kbedit->widget_list, &l, x, y - (((asc+des) - 10)>>1), 
				 40, (asc+des), fontname)));
  xitk_enable_and_show_widget(w);

  x = 15;
  y += 30;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Grab");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_grab; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(kbedit->widget_list)), 
	   (kbedit->grab = 
	    xitk_noskin_labelbutton_create(kbedit->widget_list, &lb, x, y, WINDOW_WIDTH - 30, 23,
					   "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(kbedit->grab);

  kbedit_unset();
  
  kbedit->kreg = xitk_register_event_handler("kbedit", 
					     (xitk_window_get_window(kbedit->xwin)),
					     kbedit_handle_event,
					     NULL,
					     NULL,
					     kbedit->widget_list,
					     NULL);
  

  kbedit->visible = 1;
  kbedit->running = 1;
  kbedit_raise_window();

  try_to_set_input_focus(xitk_window_get_window(kbedit->xwin));
}

#ifdef KBINDINGS_MAN

static gGui_t  *gGui;

static void layer_above_video(Window w) {
}
static void raise_window(Window window, int visible, int running) {
}
static void xine_error(char *message, ...) {
}
static void gui_execute_action_id(action_id_t actid) {
}
static void toggle_window(Window window, xitk_widget_list_t *widget_list, int *visible, int running) {
}
static void config_update_num(char *key, int value) {
}
static void try_to_set_input_focus(Window window) {
}

int main(int argc, char **argv) {
  int   i;

  gGui              = (gGui_t *) xine_xmalloc(sizeof(gGui_t));
  gGui->keymap_file = (char *) xine_xmalloc(XITK_PATH_MAX + XITK_NAME_MAX);
  sprintf(gGui->keymap_file, "%s/%s/%s", xine_get_homedir(), ".xine", "keymap");
  
  if((gGui->kbindings = kbindings_init_kbinding())) {
    kbinding_entry_t **k = gGui->kbindings->entry;
    
    for(i = 0; k[i]->action != NULL; i++) {
      
      if(k[i]->is_alias || (!strcasecmp(k[i]->key, "VOID")))
	continue;
      
      printf(".IP \"");
      
      if(k[i]->modifier != KEYMOD_NOMOD) {
	char buf[256];
	
	memset(&buf, 0, sizeof(buf));
	
	if(k[i]->modifier & KEYMOD_CONTROL)
	  sprintf(buf, "%s", "\\fBC\\fP");
	if(k[i]->modifier & KEYMOD_META) {
	  if(strlen(buf))
	    sprintf(buf, "%s%s", buf, "\\-");
	  sprintf(buf, "%s%s", buf, "\\fBM\\fP");
	}
	if(k[i]->modifier & KEYMOD_MOD3) {
	  if(strlen(buf))
	    sprintf(buf, "%s%s", buf, "\\-");
	  sprintf(buf, "%s%s", buf, "\\fBM3\\fP");
	}
	if(k[i]->modifier & KEYMOD_MOD4) {
	  if(strlen(buf))
	    sprintf(buf, "%s%s", buf, "\\-");
	  sprintf(buf, "%s%s", buf, "\\fBM4\\fP");
	}
	if(k[i]->modifier & KEYMOD_MOD5) {
	  if(strlen(buf))
	    sprintf(buf, "%s%s", buf, "\\-");
	  sprintf(buf, "%s%s", buf, "\\fBM5\\fP");
	}
	printf("%s\\-", buf);
      }
      
      printf("\\fB");
      if(strlen(k[i]->key) > 1)
	printf("\\<");
      
      if(!strncmp(k[i]->key, "KP_", 3))
	printf("Keypad %s", (k[i]->key + 3));
      else
	printf("%s", k[i]->key);
      
      if(strlen(k[i]->key) > 1)
	printf("\\>");
      printf("\\fP");
      
      printf("\"\n");
      
      printf("%c%s\n", (toupper(k[i]->comment[0])), (k[i]->comment + 1));
    }
    
    kbindings_free_kbinding(&gGui->kbindings);
  }

  free(gGui->keymap_file);
  free(gGui);

  return 0;
}

#endif
