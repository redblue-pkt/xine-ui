/*
 * Copyright (C) 2000-2021 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <strings.h>
#include <xine/sorted_array.h>

#include "dump.h"

#include "common.h"
#include "kbindings_common.h"
#include "kbindings.h"
#include "videowin.h"
#include "actions.h"

/* refs string layout: [int32_t] (char)
 * [(-)(-)(count)(refs)] [(t)(e)(x)(t)] [( )(h)(e)(r)] [(e)(\0)(\0)(\0)] [0]
 *           ((char *)ptr)-^                     ((int32_t *)ptr + count)-^ */

typedef union {
  int32_t i[32];
  char s[4 * 32];
} refs_dummy_t;

static ATTR_INLINE_ALL_STRINGOPS char *refs_set_dummy (refs_dummy_t *dummy, const char *text) {
  size_t l = strlen (text) + 1;
  if (l > sizeof (*dummy) - 8)
    l = sizeof (*dummy) - 8;
  dummy->i[(4 + l - 1) >> 2] = 0;
  dummy->i[(4 + l - 1 + 4) >> 2] = 0;
  memcpy (dummy->s + 4, text, l);
  dummy->s[2] = (l + 3) >> 2;
  return dummy->s + 4;
}

static ATTR_INLINE_ALL_STRINGOPS char *refs_strdup (const char *text) {
  size_t l, s;
  char *m;

  if (!text)
    return NULL;
  l = strlen (text) + 1;
  s = (4 + l + 4 + 3) & ~3u;
  m = malloc (s);
  if (!m)
    return NULL;
  memset (m + s - 8, 0, 8);
  m += 4;
  m[-2] = (s - 8) >> 2;
  m[-1] = 1;
  memcpy (m, text, l);
  return m;
}
  
static char *refs_ref (char *refs) {
  if (!refs)
    return NULL;
  refs[-1] += 1;
  return refs;
}

static void refs_unref (char **refs) {
  char *m;
  if (!refs)
    return;
  m = *refs;
  if (!m)
    return;
  if (--(m[-1]) == 0)
    free (m - 4);
  *refs = NULL;
}
  
/*
 * Default key mapping table.
 */
static const struct {
  const char       *comment;     /* Comment automatically added in xbinding_display*() outputs */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  const char        action[24];  /* Human readable action, used in config file too. 4 aligned. */
  const char        key[10];      /* key binding */
  uint8_t           modifier;    /* Modifier key of binding (can be OR'ed) */
  uint8_t           is_alias : 1;/* is made from an alias entry ? */
  uint8_t           is_gui : 1;
}  default_binding_table[] = {
  { N_("start playback"),
    ACTID_PLAY,                      "Play",                   "Return",   KEYMOD_NOMOD,    0, 0},
  { N_("playback pause toggle"),
    ACTID_PAUSE,                     "Pause",                  "space",    KEYMOD_NOMOD,    0, 0},
  { N_("stop playback"),
    ACTID_STOP,                      "Stop",                   "S",        KEYMOD_NOMOD,    0, 0},
  { N_("close stream"),
    ACTID_CLOSE,                     "Close",                  "C",        KEYMOD_NOMOD,    0, 0},
  { N_("take a snapshot"),
    ACTID_SNAPSHOT,                  "Snapshot",               "t",        KEYMOD_NOMOD,    0, 0},
  { N_("eject the current medium"),
    ACTID_EJECT,                     "Eject",                  "e",        KEYMOD_NOMOD,    0, 0},
  { N_("select and play next MRL in the playlist"),
    ACTID_MRL_NEXT,                  "NextMrl",                "Next",     KEYMOD_NOMOD,    0, 0},
  { N_("select and play previous MRL in the playlist"),
    ACTID_MRL_PRIOR,                 "PriorMrl",               "Prior",    KEYMOD_NOMOD,    0, 0},
  { N_("select and play MRL in the playlist"),
    ACTID_MRL_SELECT,                "SelectMrl",              "Select",   KEYMOD_NOMOD,    0, 0},
  { N_("loop mode toggle"),
    ACTID_LOOPMODE,                  "ToggleLoopMode",         "l",        KEYMOD_NOMOD,    0, 0},
  { N_("stop playback after played stream"),
    ACTID_PLAYLIST_STOP,             "PlaylistStop",           "l",        KEYMOD_CONTROL,  0, 0},
  { N_("scan playlist to grab stream infos"),
    ACTID_SCANPLAYLIST,              "ScanPlaylistInfo",       "s",        KEYMOD_CONTROL,  0, 0},
  { N_("add a mediamark from current playback"),
    ACTID_ADDMEDIAMARK,              "AddMediamark",           "a",        KEYMOD_CONTROL,  0, 0},
  { N_("edit selected mediamark"),
    ACTID_MMKEDITOR,                 "MediamarkEditor",        "e",        KEYMOD_CONTROL,  0, 1},
  { N_("set position to -60 seconds in current stream"),
    ACTID_SEEK_REL_m60,              "SeekRelative-60",        "Left",     KEYMOD_NOMOD,    0, 0},
  { N_("set position to +60 seconds in current stream"),
    ACTID_SEEK_REL_p60,              "SeekRelative+60",        "Right",    KEYMOD_NOMOD,    0, 0},
  { N_("set position to -30 seconds in current stream"),
    ACTID_SEEK_REL_m30,              "SeekRelative-30",        "Left",     KEYMOD_META,     0, 0},
  { N_("set position to +30 seconds in current stream"),
    ACTID_SEEK_REL_p30,              "SeekRelative+30",        "Right",    KEYMOD_META,     0, 0},
  { N_("set position to -15 seconds in current stream"),
    ACTID_SEEK_REL_m15,              "SeekRelative-15",        "Left",     KEYMOD_CONTROL,  0, 0},
  { N_("set position to +15 seconds in current stream"),
    ACTID_SEEK_REL_p15,              "SeekRelative+15",        "Right",    KEYMOD_CONTROL,  0, 0},
  { N_("set position to -7 seconds in current stream"),
    ACTID_SEEK_REL_m7,               "SeekRelative-7",         "Left",     KEYMOD_MOD3,     0, 0},
  { N_("set position to +7 seconds in current stream"),
    ACTID_SEEK_REL_p7,               "SeekRelative+7",         "Right",    KEYMOD_MOD3,     0, 0},
  { N_("set position to beginning of current stream"),
    ACTID_SET_CURPOS_0,              "SetPosition0%",          "0",        KEYMOD_CONTROL,  0, 0},
  /* NOTE: these used to be "... to 10% of ..." etc. but msgmerge seems not to like such "c-format". */
  { N_("set position to 1/10 of current stream"),
    ACTID_SET_CURPOS_10,             "SetPosition10%",         "1",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 2/10 of current stream"),
    ACTID_SET_CURPOS_20,             "SetPosition20%",         "2",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 3/10 of current stream"),
    ACTID_SET_CURPOS_30,             "SetPosition30%",         "3",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 4/10 of current stream"),
    ACTID_SET_CURPOS_40,             "SetPosition40%",         "4",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 5/10 of current stream"),
    ACTID_SET_CURPOS_50,             "SetPosition50%",         "5",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 6/10 of current stream"),
    ACTID_SET_CURPOS_60,             "SetPosition60%",         "6",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 7/10 of current stream"),
    ACTID_SET_CURPOS_70,             "SetPosition70%",         "7",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 8/10 of current stream"),
    ACTID_SET_CURPOS_80,             "SetPosition80%",         "8",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to 9/10 of current stream"),
    ACTID_SET_CURPOS_90,             "SetPosition90%",         "9",        KEYMOD_CONTROL,  0, 0},
  { N_("set position to end of current stream"),
    ACTID_SET_CURPOS_100,            "SetPosition100%",        "End",      KEYMOD_CONTROL,  0, 0},
  { N_("increment playback speed"),
    ACTID_SPEED_FAST,                "SpeedFaster",            "Up",       KEYMOD_NOMOD,    0, 0},
  { N_("decrement playback speed"),
    ACTID_SPEED_SLOW,                "SpeedSlower",            "Down",     KEYMOD_NOMOD,    0, 0},
  { N_("reset playback speed"),
    ACTID_SPEED_RESET,               "SpeedReset",             "Down",     KEYMOD_META,     0, 0},
  { N_("increment audio volume"),
    ACTID_pVOLUME,                   "Volume+",                "V",        KEYMOD_NOMOD,    0, 0},
  { N_("decrement audio volume"),
    ACTID_mVOLUME,                   "Volume-",                "v",        KEYMOD_NOMOD,    0, 0},
  { N_("increment amplification level"),
    ACTID_pAMP,                      "Amp+",                   "V",        KEYMOD_CONTROL,  0, 0},
  { N_("decrement amplification level"),
    ACTID_mAMP,                      "Amp-",                   "v",        KEYMOD_CONTROL,  0, 0},
  { N_("reset amplification to default value"),
    ACTID_AMP_RESET,                 "ResetAmp",               "A",        KEYMOD_CONTROL,  0, 0},
  { N_("audio muting toggle"),
    ACTID_MUTE,                      "Mute",                   "m",        KEYMOD_CONTROL,  0, 0},
  { N_("select next audio channel"),
    ACTID_AUDIOCHAN_NEXT,            "AudioChannelNext",       "plus",     KEYMOD_NOMOD,    0, 0},
  { N_("select previous audio channel"),
    ACTID_AUDIOCHAN_PRIOR,           "AudioChannelPrior",      "minus",    KEYMOD_NOMOD,    0, 0},
  { N_("visibility toggle of audio post effect window"),
    ACTID_APP,                       "APProcessShow",          "VOID",     KEYMOD_NOMOD,    0, 1},
  { N_("toggle post effect usage"),
    ACTID_APP_ENABLE,                "APProcessEnable",        "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("select next sub picture (subtitle) channel"),
    ACTID_SPU_NEXT,                  "SpuNext",                "period",   KEYMOD_NOMOD,    0, 0},
  { N_("select previous sub picture (subtitle) channel"),
    ACTID_SPU_PRIOR,                 "SpuPrior",               "comma",    KEYMOD_NOMOD,    0, 0},
  { N_("interlaced mode toggle"),
    ACTID_TOGGLE_INTERLEAVE,         "ToggleInterleave",       "i",        KEYMOD_NOMOD,    0, 0},
  { N_("cycle aspect ratio values"),
    ACTID_TOGGLE_ASPECT_RATIO,       "ToggleAspectRatio",      "a",        KEYMOD_NOMOD,    0, 0},
  { N_("reduce the output window size by factor 1.2"),
    ACTID_WINDOWREDUCE,              "WindowReduce",           "less",     KEYMOD_NOMOD,    0, 0},
  { N_("enlarge the output window size by factor 1.2"),
    ACTID_WINDOWENLARGE,             "WindowEnlarge",          "greater",  KEYMOD_NOMOD,    0, 0},
  { N_("set video output window to 50%"),
    ACTID_WINDOW50,                  "Window50",               "1",        KEYMOD_META,     0, 0},
  { N_("set video output window to 100%"),
    ACTID_WINDOW100,                 "Window100",              "2",        KEYMOD_META,     0, 0},
  { N_("set video output window to 200%"),
    ACTID_WINDOW200,                 "Window200",              "3",        KEYMOD_META,     0, 0},
  { N_("zoom in"),
    ACTID_ZOOM_IN,                   "ZoomIn",                 "z",        KEYMOD_NOMOD,    0, 0},
  { N_("zoom out"),
    ACTID_ZOOM_OUT,                  "ZoomOut",                "Z",        KEYMOD_NOMOD,    0, 0},
  { N_("zoom in horizontally"),
    ACTID_ZOOM_X_IN,                 "ZoomInX",                "z",        KEYMOD_CONTROL,  0, 0},
  { N_("zoom out horizontally"),
    ACTID_ZOOM_X_OUT,                "ZoomOutX",               "Z",        KEYMOD_CONTROL,  0, 0},
  { N_("zoom in vertically"),
    ACTID_ZOOM_Y_IN,                 "ZoomInY",                "z",        KEYMOD_META,     0, 0},
  { N_("zoom out vertically"),
    ACTID_ZOOM_Y_OUT,                "ZoomOutY",               "Z",        KEYMOD_META,     0, 0},
  { N_("reset zooming"),
    ACTID_ZOOM_RESET,                "ZoomReset",              "z",        KEYMOD_CONTROL | KEYMOD_META, 0, 0},
  { N_("resize output window to stream size"),
    ACTID_ZOOM_1_1,                  "Zoom1:1",                "s",        KEYMOD_NOMOD,    0, 0},
  { N_("fullscreen toggle"),
    ACTID_TOGGLE_FULLSCREEN,         "ToggleFullscreen",       "f",        KEYMOD_NOMOD,    0, 0},
#ifdef HAVE_XINERAMA
  { N_("Xinerama fullscreen toggle"),
    ACTID_TOGGLE_XINERAMA_FULLSCREEN,"ToggleXineramaFullscr",  "F",        KEYMOD_NOMOD,    0, 0},
#endif
  { N_("jump to media Menu"),
    ACTID_EVENT_MENU1,               "Menu",                   "Escape",   KEYMOD_NOMOD,    0, 0},
  { N_("jump to Title Menu"),
    ACTID_EVENT_MENU2,               "TitleMenu",              "F1",       KEYMOD_NOMOD,    0, 0},
  { N_("jump to Root Menu"),
    ACTID_EVENT_MENU3,               "RootMenu",               "F2",       KEYMOD_NOMOD,    0, 0},
  { N_("jump to Subpicture Menu"),
    ACTID_EVENT_MENU4,               "SubpictureMenu",         "F3",       KEYMOD_NOMOD,    0, 0},
  { N_("jump to Audio Menu"),
    ACTID_EVENT_MENU5,               "AudioMenu",              "F4",       KEYMOD_NOMOD,    0, 0},
  { N_("jump to Angle Menu"),
    ACTID_EVENT_MENU6,               "AngleMenu",              "F5",       KEYMOD_NOMOD,    0, 0},
  { N_("jump to Part Menu"),
    ACTID_EVENT_MENU7,               "PartMenu",               "F6",       KEYMOD_NOMOD,    0, 0},
  { N_("menu navigate up"),
    ACTID_EVENT_UP,                  "EventUp",                "KP_Up",    KEYMOD_NOMOD,    0, 0},
  { N_("menu navigate down"),
    ACTID_EVENT_DOWN,                "EventDown",              "KP_Down",  KEYMOD_NOMOD,    0, 0},
  { N_("menu navigate left"),
    ACTID_EVENT_LEFT,                "EventLeft",              "KP_Left",  KEYMOD_NOMOD,    0, 0},
  { N_("menu navigate right"),
    ACTID_EVENT_RIGHT,               "EventRight",             "KP_Right", KEYMOD_NOMOD,    0, 0},
  { N_("menu select"),
    ACTID_EVENT_SELECT,              "EventSelect",            "KP_Enter", KEYMOD_NOMOD,    0, 0},
  { N_("jump to next chapter"),
    ACTID_EVENT_NEXT,                "EventNext",              "KP_Next",  KEYMOD_NOMOD,    0, 0},
  { N_("jump to previous chapter"),
    ACTID_EVENT_PRIOR,               "EventPrior",             "KP_Prior", KEYMOD_NOMOD,    0, 0},
  { N_("select next angle"),
    ACTID_EVENT_ANGLE_NEXT,          "EventAngleNext",         "KP_Home",  KEYMOD_NOMOD,    0, 0},
  { N_("select previous angle"),
    ACTID_EVENT_ANGLE_PRIOR,         "EventAnglePrior",        "KP_End",   KEYMOD_NOMOD,    0, 0},
  { N_("visibility toggle of help window"),
    ACTID_HELP_SHOW,                 "HelpShow",               "h",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of video post effect window"),
    ACTID_VPP,                       "VPProcessShow",          "P",        KEYMOD_META,     0, 1},
  { N_("toggle post effect usage"),
    ACTID_VPP_ENABLE,                "VPProcessEnable",        "P",        KEYMOD_CONTROL | KEYMOD_META, 0, 0},
  { N_("visibility toggle of output window"),
    ACTID_TOGGLE_WINOUT_VISIBLITY,   "ToggleWindowVisibility", "h",        KEYMOD_NOMOD,    0, 1},
  { N_("bordered window toggle of output window"),
    ACTID_TOGGLE_WINOUT_BORDER,      "ToggleWindowBorder",     "b",        KEYMOD_NOMOD,    0, 1},
  { N_("visibility toggle of UI windows"),
    ACTID_TOGGLE_VISIBLITY,          "ToggleVisibility",       "g",        KEYMOD_NOMOD,    0, 1},
  { N_("visibility toggle of control window"),
    ACTID_CONTROLSHOW,               "ControlShow",            "c",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of audio control window"),
    ACTID_ACONTROLSHOW,              "AControlShow",           "a",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of mrl browser window"),
    ACTID_MRLBROWSER,                "MrlBrowser",             "m",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of playlist editor window"),
    ACTID_PLAYLIST,                  "PlaylistEditor",         "p",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of the setup window"),
    ACTID_SETUP,                     "SetupShow",              "s",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of the event sender window"),
    ACTID_EVENT_SENDER,              "EventSenderShow",        "e",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of analog TV window"),
    ACTID_TVANALOG,                  "TVAnalogShow",           "t",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of log viewer"),
    ACTID_VIEWLOG,                   "ViewlogShow",            "l",        KEYMOD_META,     0, 1},
  { N_("visibility toggle of stream info window"),
    ACTID_STREAM_INFOS,              "StreamInfosShow",        "i",        KEYMOD_META,     0, 1},
  { N_("display stream information using OSD"),
    ACTID_OSD_SINFOS,                "OSDStreamInfos",         "i",        KEYMOD_CONTROL,  0, 0},
  { N_("display information using OSD"),
    ACTID_OSD_WTEXT,                 "OSDWriteText",           "VOID",     KEYMOD_CONTROL,  0, 0},
  { N_("show OSD menu"),
    ACTID_OSD_MENU,                  "OSDMenu",                "O",        KEYMOD_NOMOD,    0, 0},
  { N_("enter key binding editor"),
    ACTID_KBEDIT,                    "KeyBindingEditor",       "k",        KEYMOD_META,     0, 1},
  { N_("enable key bindings (not useful to bind a key to it!)"),
    ACTID_KBENABLE,                  "KeyBindingsEnable",      "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("open file selector"),
    ACTID_FILESELECTOR,              "FileSelector",           "o",        KEYMOD_CONTROL,  0, 1},
  { N_("select a subtitle file"),
    ACTID_SUBSELECT,                 "SubSelector",            "S",        KEYMOD_CONTROL,  0, 1},
#ifdef HAVE_CURL
  { N_("download a skin from the skin server"),
    ACTID_SKINDOWNLOAD,              "SkinDownload",           "d",        KEYMOD_CONTROL,  0, 1},
#endif
  { N_("display MRL/Ident toggle"),
    ACTID_MRLIDENTTOGGLE,            "MrlIdentToggle",         "t",        KEYMOD_CONTROL,  0, 0},
  { N_("grab pointer toggle"),
    ACTID_GRAB_POINTER,              "GrabPointer",            "Insert",   KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 0"),
    ACTID_EVENT_NUMBER_0,            "Number0",                "0",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 1"),
    ACTID_EVENT_NUMBER_1,            "Number1",                "1",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 2"),
    ACTID_EVENT_NUMBER_2,            "Number2",                "2",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 3"),
    ACTID_EVENT_NUMBER_3,            "Number3",                "3",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 4"),
    ACTID_EVENT_NUMBER_4,            "Number4",                "4",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 5"),
    ACTID_EVENT_NUMBER_5,            "Number5",                "5",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 6"),
    ACTID_EVENT_NUMBER_6,            "Number6",                "6",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 7"),
    ACTID_EVENT_NUMBER_7,            "Number7",                "7",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 8"),
    ACTID_EVENT_NUMBER_8,            "Number8",                "8",        KEYMOD_NOMOD,    0, 0},
  { N_("enter the number 9"),
    ACTID_EVENT_NUMBER_9,            "Number9",                "9",        KEYMOD_NOMOD,    0, 0},
  { N_("add 10 to the next entered number"),
    ACTID_EVENT_NUMBER_10_ADD,       "Number10add",            "plus",     KEYMOD_MOD3,     0, 0},
  { N_("set position in current stream to numeric percentage"),
    ACTID_SET_CURPOS,                "SetPosition%",           "slash",    KEYMOD_NOMOD,    0, 0},
  { N_("set position forward by numeric argument in current stream"),
    ACTID_SEEK_REL_p,                "SeekRelative+",          "Up",       KEYMOD_META,     0, 0},
  { N_("set position back by numeric argument in current stream"),
    ACTID_SEEK_REL_m,                "SeekRelative-",          "Up",       KEYMOD_MOD3,     0, 0},
  { N_("change audio video syncing (delay video)"),
    ACTID_AV_SYNC_p3600,             "AudioVideoDecay+",       "m",        KEYMOD_NOMOD,    0, 0},
  { N_("change audio video syncing (delay audio)"),
    ACTID_AV_SYNC_m3600,             "AudioVideoDecay-",       "n",        KEYMOD_NOMOD,    0, 0},
  { N_("reset audio video syncing offset"),
    ACTID_AV_SYNC_RESET,             "AudioVideoDecayReset",   "Home",     KEYMOD_NOMOD,    0, 0},
  { N_("change subtitle syncing (delay video)"),
    ACTID_SV_SYNC_p,                 "SpuVideoDecay+",         "M",        KEYMOD_NOMOD,    0, 0},
  { N_("change subtitle syncing (delay subtitles)"),
    ACTID_SV_SYNC_m,                 "SpuVideoDecay-",         "N",        KEYMOD_NOMOD,    0, 0},
  { N_("reset subtitle syncing offset"),
    ACTID_SV_SYNC_RESET,             "SpuVideoDecayReset",     "End",      KEYMOD_NOMOD,    0, 0},
  { N_("toggle TV modes (on the DXR3)"),
    ACTID_TOGGLE_TVMODE,             "ToggleTVmode",           "o",        KEYMOD_CONTROL | KEYMOD_META, 0, 0},
  { N_("switch Monitor to DPMS standby mode"),
    ACTID_DPMSSTANDBY,               "DPMSStandby",            "d",        KEYMOD_NOMOD,    0, 0},
  { N_("increase hue by 10"),
    ACTID_HUECONTROLp,               "HueControl+",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease hue by 10"),
    ACTID_HUECONTROLm,               "HueControl-",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase saturation by 10"),
    ACTID_SATURATIONCONTROLp,        "SaturationControl+",     "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease saturation by 10"),
    ACTID_SATURATIONCONTROLm,        "SaturationControl-",     "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase brightness by 10"),
    ACTID_BRIGHTNESSCONTROLp,        "BrightnessControl+",     "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease brightness by 10"),
    ACTID_BRIGHTNESSCONTROLm,        "BrightnessControl-",     "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase contrast by 10"),
    ACTID_CONTRASTCONTROLp,          "ContrastControl+",       "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease contrast by 10"),
    ACTID_CONTRASTCONTROLm,          "ContrastControl-",       "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase gamma by 10"),
    ACTID_GAMMACONTROLp,             "GammaControl+",          "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease gamma by 10"),
    ACTID_GAMMACONTROLm,             "GammaControl-",          "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase sharpness by 10"),
    ACTID_SHARPNESSCONTROLp,         "SharpnessControl+",      "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease sharpness by 10"),
    ACTID_SHARPNESSCONTROLm,         "SharpnessControl-",      "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("increase noise reduction by 10"),
    ACTID_NOISEREDUCTIONCONTROLp,    "NoiseReductionControl+", "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("decrease noise reduction by 10"),
    ACTID_NOISEREDUCTIONCONTROLm,    "NoiseReductionControl-", "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("quit the program"),
    ACTID_QUIT,                      "Quit",                   "q",        KEYMOD_NOMOD,    0, 0},
  { N_("input_pvr: set input"),
    ACTID_PVR_SETINPUT,              "PVRSetInput",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("input_pvr: set frequency"),
    ACTID_PVR_SETFREQUENCY,          "PVRSetFrequency",        "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("input_pvr: mark the start of a new stream section"),
    ACTID_PVR_SETMARK,               "PVRSetMark",             "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("input_pvr: set the name for the current stream section"),
    ACTID_PVR_SETNAME,               "PVRSetName",             "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("input_pvr: save the stream section"),
    ACTID_PVR_SAVE,                  "PVRSave",                "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("Open playlist"),
    ACTID_PLAYLIST_OPEN,             "PlaylistOpen",           "VOID",     KEYMOD_NOMOD,    0, 0},
#ifdef ENABLE_VDR_KEYS
  { N_("VDR Red button"),
    ACTID_EVENT_VDR_RED,             "VDRButtonRed",           "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Green button"),
    ACTID_EVENT_VDR_GREEN,           "VDRButtonGreen",         "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Yellow button"),
    ACTID_EVENT_VDR_YELLOW,          "VDRButtonYellow",        "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Blue button"),
    ACTID_EVENT_VDR_BLUE,            "VDRButtonBlue",          "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Play"),
    ACTID_EVENT_VDR_PLAY,            "VDRPlay",                "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Pause"),
    ACTID_EVENT_VDR_PAUSE,           "VDRPause",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Stop"),
    ACTID_EVENT_VDR_STOP,            "VDRStop",                "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Record"),
    ACTID_EVENT_VDR_RECORD,          "VDRRecord",              "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Fast forward"),
    ACTID_EVENT_VDR_FASTFWD,         "VDRFastFwd",             "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Fast rewind"),
    ACTID_EVENT_VDR_FASTREW,         "VDRFastRew",             "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Power"),
    ACTID_EVENT_VDR_POWER,           "VDRPower",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Channel +"),
    ACTID_EVENT_VDR_CHANNELPLUS,     "VDRChannelPlus",         "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Channel -"),
    ACTID_EVENT_VDR_CHANNELMINUS,    "VDRChannelMinus",        "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Schedule menu"),
    ACTID_EVENT_VDR_SCHEDULE,        "VDRSchedule",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Channel menu"),
    ACTID_EVENT_VDR_CHANNELS,        "VDRChannels",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Timers menu"),
    ACTID_EVENT_VDR_TIMERS,          "VDRTimers",              "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Recordings menu"),
    ACTID_EVENT_VDR_RECORDINGS,      "VDRRecordings",          "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Setup menu"),
    ACTID_EVENT_VDR_SETUP,           "VDRSetup",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Command menu"),
    ACTID_EVENT_VDR_COMMANDS,        "VDRCommands",            "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Command back"),
    ACTID_EVENT_VDR_BACK,            "VDRBack",                "VOID",     KEYMOD_NOMOD,    0, 0},
#ifdef XINE_EVENT_VDR_USER0 /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR User command 0"),
    ACTID_EVENT_VDR_USER0,           "VDRUser0",               "VOID",     KEYMOD_NOMOD,    0, 0},
#endif
  { N_("VDR User command 1"),
    ACTID_EVENT_VDR_USER1,           "VDRUser1",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 2"),
    ACTID_EVENT_VDR_USER2,           "VDRUser2",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 3"),
    ACTID_EVENT_VDR_USER3,           "VDRUser3",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 4"),
    ACTID_EVENT_VDR_USER4,           "VDRUser4",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 5"),
    ACTID_EVENT_VDR_USER5,           "VDRUser5",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 6"),
    ACTID_EVENT_VDR_USER6,           "VDRUser6",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 7"),
    ACTID_EVENT_VDR_USER7,           "VDRUser7",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 8"),
    ACTID_EVENT_VDR_USER8,           "VDRUser8",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR User command 9"),
    ACTID_EVENT_VDR_USER9,           "VDRUser9",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Volume +"),
    ACTID_EVENT_VDR_VOLPLUS,         "VDRVolumePlus",          "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Volume -"),
    ACTID_EVENT_VDR_VOLMINUS,        "VDRVolumeMinus",         "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Mute audio"),
    ACTID_EVENT_VDR_MUTE,            "VDRMute",                "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Audio menu"),
    ACTID_EVENT_VDR_AUDIO,           "VDRAudio",               "VOID",     KEYMOD_NOMOD,    0, 0},
  { N_("VDR Command info"),
    ACTID_EVENT_VDR_INFO,            "VDRInfo",                "VOID",     KEYMOD_NOMOD,    0, 0},
#ifdef XINE_EVENT_VDR_CHANNELPREVIOUS /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR Previous channel"),
    ACTID_EVENT_VDR_CHANNELPREVIOUS, "VDRChannelPrevious",     "VOID",     KEYMOD_NOMOD,    0, 0},
#endif
#ifdef XINE_EVENT_VDR_SUBTITLES /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR Subtiles menu"),
    ACTID_EVENT_VDR_SUBTITLES,       "VDRSubtitles",           "VOID",     KEYMOD_NOMOD,    0, 0}
#endif
#endif
};

#define KBT_NUM_BASE (sizeof (default_binding_table) / sizeof (default_binding_table[0]))
#define _kbt_entry(kbt,index) ((unsigned int)index < KBT_NUM_BASE ? kbt->base + index : kbt->alias[index - KBT_NUM_BASE])

struct kbinding_s {
  gGui_t           *gui;
  int               num_entries;
  xine_sarray_t    *action_index, *key_index;
  kbinding_entry_t *last, base[KBT_NUM_BASE], *alias[MAX_ENTRIES - KBT_NUM_BASE];
};

static int _kbindings_action_cmp (void *a, void *b) {
  kbinding_entry_t *d = (kbinding_entry_t *)a;
  kbinding_entry_t *e = (kbinding_entry_t *)b;
  const int32_t *v1 = (const int32_t *)(const void *)d->action;
  const int32_t *v2 = (const int32_t *)(const void *)e->action;
  int f;
  /* 'A' == 'a, sizeof (default_binding_table[0].action) == 6 * 4 */
  f = (v1[0] | 0x20202020) - (v2[0] | 0x20202020);
  if (f)
    return f;
  f = (v1[1] | 0x20202020) - (v2[1] | 0x20202020);
  if (f)
    return f;
  f = (v1[2] | 0x20202020) - (v2[2] | 0x20202020);
  if (f)
    return f;
  f = (v1[3] | 0x20202020) - (v2[3] | 0x20202020);
  if (f)
    return f;
  f = (v1[4] | 0x20202020) - (v2[4] | 0x20202020);
  if (f)
    return f;
  f = (v1[5] | 0x20202020) - (v2[5] | 0x20202020);
  if (f)
    return f;
  return (int)d->is_alias - (int)e->is_alias;
}

static int _kbindings_key_cmp (void *a, void *b) {
  /* yes this depends on string length and mmachine endian,
   * but it is still a atable sort. */
  kbinding_entry_t *d = (kbinding_entry_t *)a;
  kbinding_entry_t *e = (kbinding_entry_t *)b;
  /* "A" != "a" */
  const int32_t *v1 = (const int32_t *)(const void *)d->key;
  const int32_t *v2 = (const int32_t *)(const void *)e->key;
  int32_t *s = (int32_t *)(void *)d->key + d->key[-2];
  int f;
  /* a layout violation that safely stops our fast single test loop. */
  *s = 0x00ffff00;
  while (*v1 == *v2)
    v1++, v2++;
  *s = 0;
  f = *v1 - *v2;
  if (f)
    return f;
  return (int)d->modifier - (int)e->modifier;
}

static void kbindings_index_add (kbinding_t *kbt, kbinding_entry_t *entry) {
  if (entry->action)
    xine_sarray_add (kbt->action_index, entry);
  if (entry->key && strcasecmp (entry->key, "void"))
    xine_sarray_add (kbt->key_index, entry);
}

static void kbindings_index_remove (kbinding_t *kbt, kbinding_entry_t *entry) {
  kbt->last = NULL;
#ifdef XINE_SARRAY_MODE_DEFAULT
  xine_sarray_remove_ptr (kbt->action_index, entry);
  xine_sarray_remove_ptr (kbt->key_index, entry);
#else
  int i;
  i = xine_sarray_binary_search (kbt->action_index, entry);
  if (i >= 0)
    i = xine_sarray_remove (kbt->action_index, i);
  i = xine_sarray_binary_search (kbt->key_index, entry);
  if (i >= 0)
    i = xine_sarray_remove (kbt->key_index, i);
#endif
}

static const uint8_t _tab_char[256] = {
  [0]    = 1,
  ['\t'] = 2,
  [' ']  = 2,
  [',']  = 4
};

typedef union {
  uint8_t b[4];
  uint32_t w;
} _v4_t;

static int _kbindings_modifier_from_string (const char *s) {
  /* NOTE: for [static] const, gcc references actual read only mem.
   * however, this here optimizes to a plain immediate value.
   * tested with gcc -S ;-) */
  _v4_t _none = {{'n', 'o', 'n', 'e'}};
  _v4_t _ctrl = {{'c', 't', 'r', 'l'}};
  _v4_t _meta = {{'m', 'e', 't', 'a'}};
  _v4_t _mod3 = {{'m', 'o', 'd', '3'}};
  _v4_t _mod4 = {{'m', 'o', 'd', '4'}};
  _v4_t _mod5 = {{'m', 'o', 'd', '5'}};
  _v4_t _cont = {{'c', 'o', 'n', 't'}};
  _v4_t _rol_ = {{'r', 'o', 'l', 0}};
  _v4_t _alt_ = {{'a', 'l', 't', 0}};
  int modifier = KEYMOD_NOMOD;
  const uint8_t *p = (const uint8_t *)s, *b, *e;
  while (1) {
    size_t l;
    while (_tab_char[*p] & 2) /* \t ' ' */
      p++;
    if (!*p)
      break;
    b = p;
    do {
      while (!(_tab_char[*p] & (1 | 2 | 4))) /* \0 \t ' ' , */
        p++;
      e = p;
      while (_tab_char[*p] & 2) /* \t ' ' */
        p++;
    } while (!(_tab_char[*p] & (1 | 4))); /* \0 , */
    l = e - b;
    if (l == 4) {
      uint32_t v;
      memcpy (&v, b, 4);
      v |= 0x20202020;
      if (v == _none.w) {
        modifier = KEYMOD_NOMOD;
      } else if (v == _ctrl.w) {
        modifier |= KEYMOD_CONTROL;
      } else if (v == _meta.w) {
        modifier |= KEYMOD_META;
      } else if (v == _mod3.w) {
        modifier |= KEYMOD_MOD3;
      } else if (v == _mod4.w) {
        modifier |= KEYMOD_MOD4;
      } else if (v == _mod5.w) {
        modifier |= KEYMOD_MOD5;
      }
    } else if (l == 7) {
      _v4_t v;
      memcpy (&v.w, b, 4);
      v.w |= 0x20202020;
      if (v.w == _cont.w) {
        memcpy (&v.w, b + 4, 4);
        v.w |= 0x20202020;
        v.b[3] = 0;
        if (v.w == _rol_.w)
          modifier |= KEYMOD_CONTROL;
      }
    } else if (l == 3) {
      _v4_t v;
      memcpy (&v.w, b, 4);
      v.w |= 0x20202020;
      v.b[3] = 0;
      if (v.w == _alt_.w)
        modifier |= KEYMOD_META;
    }
    if (!*p)
      break;
    p++;
  }
  return modifier;
}

/*
 * Return a key binding entry (if available) matching with action string.
 */
static ATTR_INLINE_ALL_STRINGOPS kbinding_entry_t *_kbindings_lookup_action (kbinding_t *kbt, const char *action) {
  union {
    int32_t i[6];
    char s[6 * 4];
  } buf;
  kbinding_entry_t dummy;
  size_t s = strlen (action) + 1;
  int i;

  if (s > sizeof (buf))
    s = sizeof (buf);
  for (i = s >> 2; i < (int)sizeof (buf) / 4; i++)
    buf.i[i] = 0;
  memcpy (buf.s, action, s);
  dummy.action = buf.s;
  dummy.is_alias = 0;
  i = xine_sarray_binary_search (kbt->action_index, &dummy);
  if (i < 0)
    return NULL;
  return (kbinding_entry_t *)xine_sarray_get (kbt->action_index, i);
}

const kbinding_entry_t *kbindings_lookup_action (kbinding_t *kbt, const char *action) {
  if((action == NULL) || (kbt == NULL))
    return NULL;
  return _kbindings_lookup_action (kbt, action);
}

/*
 * Read and parse remap key binding file, if available.
 */
static void _kbinding_load_config(kbinding_t *kbt, const char *name) {
  size_t fsize = 1 << 20;
  char *fbuf;
  xitk_cfg_parse_t *tree;
  int i;

  fbuf = xitk_cfg_load (name, &fsize);
  if (!fbuf)
    return;

  tree = xitk_cfg_parse (fbuf, XITK_CFG_PARSE_CASE);
  if (!tree) {
    xitk_cfg_unload (fbuf);
    return;
  }

  for (i = tree[0].first_child; i; i = tree[i].next) {
    const char *sname = fbuf + tree[i].key, *entry = "", *key = "", *mod = "";
    int j;

    for (j = tree[i].first_child; j; j = tree[j].next) {
      const char *ename = fbuf + tree[j].key, *eval = fbuf + tree[j].value;
      int d;

      d = strcmp (ename, "key");
      if (d < 0) {
        if (!strcmp (ename, "entry")) 
          entry = eval;
      } else if (d == 0) {
        key = eval;
      } else {
        if (!strcmp (ename, "modifier"))
          mod = eval;
      }
    }
    if (!key[0])
      continue;
    if (!strcmp (sname, "Alias")) {
      kbinding_entry_t *k;
      int modifier;

      if (!entry[0])
        continue;
      /* This should only happen if the keymap file was edited manually. */
      if (kbt->num_entries >= MAX_ENTRIES) {
        fprintf (stderr, _("xine-ui: Too many Alias entries in keymap file, entry ignored.\n"));
        continue;
      }
      k = _kbindings_lookup_action (kbt, entry);
      if (k) {
        kbinding_entry_t *n = (kbinding_entry_t *)malloc (sizeof (*n));
        if (n) {
          modifier = mod[0] ? _kbindings_modifier_from_string (mod) : k->modifier;
          /* Add new entry */
          n->action     = k->action;
          n->action_id  = k->action_id;
          n->index      = kbt->num_entries;
          n->modifier   = modifier;
          n->is_alias   = 1;
          n->is_gui     = k->is_gui;
          n->is_default = 0;
          n->comment    = k->comment;
          n->key        = refs_strdup (key);
          kbindings_index_add (kbt, n);
          kbt->alias[kbt->num_entries - KBT_NUM_BASE] = n;
          kbt->num_entries++;
        }
      }
    } else {
      kbinding_entry_t *e = _kbindings_lookup_action (kbt, sname);
      if (e) {
        int modifier = _kbindings_modifier_from_string (mod);
        if (strcmp (e->key, key) || (e->modifier != modifier)) {
          kbindings_index_remove (kbt, e);
          refs_unref (&e->key);
          e->key = refs_strdup (key);
          e->modifier = modifier;
          e->is_default = 0;
          if (e->index < KBT_NUM_BASE)
            e->is_gui = default_binding_table[e->index].is_gui;
          kbindings_index_add (kbt, e);
        }
      }
    }
  }

  xitk_cfg_unparse (tree);
  xitk_cfg_unload (fbuf);
}
  
/*
 * Check if there some redundant entries in key binding table kbt.
 */
static void _kbinding_done (void *data, int state) {
  kbinding_t *kbt = data;
  if (state == 1)
    kbindings_reset_kbinding (kbt);
  else if (state == 2)
    kbedit_window (kbt->gui);
}

static void _kbindings_check_redundancy(kbinding_t *kbt) {
  kbinding_entry_t *e1;
  int n, i, found = 0;
  size_t msglen = 0, dnalen = 0;
  char *kmsg = NULL;
  const char *dna = NULL;

  if(kbt == NULL)
    return;

  n = xine_sarray_size (kbt->key_index);
  e1 = xine_sarray_get (kbt->key_index, 0);
  for (i = 1; i < n; i++) {
    kbinding_entry_t *e2 = xine_sarray_get (kbt->key_index, i);
    if (!_kbindings_key_cmp (e1, e2)) {
      char *p;
      const char *action1 = e1->action, *action2 = e2->action;
      size_t alen1 = strlen (action1), alen2 = strlen (action2);
      found++;
      if (!kmsg) {
        const char *header = _("The following key bindings pairs are identical:\n\n");
        size_t hlen = strlen (header);
        dna = _("and");
        dnalen = strlen (dna);
        kmsg = malloc (hlen + alen1 + 1 + dnalen + 1 + alen2 + 2);
        if (!kmsg)
          break;
        p = kmsg;
        memcpy (p, header, hlen); p += hlen;
      } else {
        char *nmsg = realloc (kmsg, msglen + 2 + alen1 + 1 + dnalen + 1 + alen2 + 2);
        if (!nmsg)
          break;
        kmsg = nmsg;
        p = kmsg + msglen;
        memcpy (p, ", ", 2); p += 2;
      }
      memcpy (p, action1, alen1); p += alen1;
      *p++ = ' ';
      memcpy (p, dna, dnalen); p += dnalen;
      *p++ = ' ';
      memcpy (p, action2, alen2 + 1); p += alen2;
      msglen = p - kmsg;
    }
    e1 = e2;
  }

  if (found) {
    if (kbt->gui && kbt->gui->xitk) {
      xitk_register_key_t key;
      const char *footer = _(".\n\nWhat do you want to do ?\n");
      size_t flen = strlen (footer);
      char *nmsg = realloc (kmsg, msglen + flen + 1);
      if (nmsg) {
        kmsg = nmsg;
        memcpy (kmsg + msglen, footer, flen + 1);
      }
      dump_error (kmsg);
      key = xitk_window_dialog_3 (kbt->gui->xitk, NULL,
        get_layer_above_video (kbt->gui), 450, _("Keybindings error!"), _kbinding_done, kbt,
        _("Reset"), _("Editor"), _("Cancel"), NULL, 0, ALIGN_CENTER, "%s", kmsg);
      video_window_set_transient_for (kbt->gui->vwin, xitk_get_window (kbt->gui->xitk, key));
    } else {
      memcpy (kmsg + msglen, "\n", 2);
      printf ("%s", kmsg);
    }
  }

  free(kmsg);
}

/*
 * Initialize a key binding table from default, then try
 * to remap this one with (system/user) remap files.
 */

kbinding_t *kbindings_init_kbinding (gGui_t *gui, const char *keymap_file) {
  kbinding_t *kbt;
  int i;

  kbt = (kbinding_t *)malloc (sizeof (*kbt));
  if (!kbt)
    return NULL;

  kbt->gui = gui;
  kbt->last = NULL;
  kbt->action_index = xine_sarray_new (MAX_ENTRIES, _kbindings_action_cmp);
  kbt->key_index = xine_sarray_new (MAX_ENTRIES, _kbindings_key_cmp);
  if (!kbt->action_index || !kbt->key_index) {
    xine_sarray_delete (kbt->action_index);
    xine_sarray_delete (kbt->key_index);
    free (kbt);
    return NULL;
  }

  /*
   * FIXME: This should be a compile time check.
   */
  /* Check for space to hold the default table plus a number of user defined aliases. */
  if (KBT_NUM_BASE > MAX_ENTRIES) {
    fprintf(stderr, "%s(%d):\n"
		    "  Too many entries in default_binding_table[].\n"
		    "  Increase MAX_ENTRIES to at least %zd.\n",
	    __XINE_FUNCTION__, __LINE__, KBT_NUM_BASE + 100);
    abort();
  }

  for (i = 0; i < (int)KBT_NUM_BASE; i++) {
    kbinding_entry_t *n = kbt->base + i;
    n->action     = default_binding_table[i].action;
    n->action_id  = default_binding_table[i].action_id;
    n->modifier   = default_binding_table[i].modifier;
    n->index      = i;
    n->is_alias   = default_binding_table[i].is_alias;
    n->is_gui     = default_binding_table[i].is_gui;
    n->is_default = 1;
    n->comment    = gettext (default_binding_table[i].comment);
    n->key        = refs_strdup (default_binding_table[i].key);
    kbindings_index_add (kbt, n);
  }
  kbt->num_entries = i;

  if (keymap_file)
    _kbinding_load_config (kbt, keymap_file);

  /* Just to check is there redundant entries, and inform user */
  _kbindings_check_redundancy(kbt);

  return kbt;
}

/*
 * Free a keybindings object.
 */
void kbindings_free_kbinding (kbinding_t **_kbt) {
  kbinding_t *kbt;
  int i;

  if (!_kbt)
    return;
  kbt = *_kbt;
  if (!kbt)
    return;

  kbt->last = NULL;
  xine_sarray_clear (kbt->action_index);
  xine_sarray_clear (kbt->key_index);

  for (i = kbt->num_entries - 1; i >= (int)KBT_NUM_BASE; i--) {
    kbinding_entry_t *e = kbt->alias[i - KBT_NUM_BASE];
    if (e) {
      e->action = NULL;
      e->comment = NULL;
      refs_unref (&e->key);
      kbt->alias[i - KBT_NUM_BASE] = NULL;
      free (e);
    }
  }
  for (; i >= 0; i--) {
    kbinding_entry_t *e = kbt->base + i;
    if (e) {
      e->action = NULL;
      e->comment = NULL;
      refs_unref (&e->key);
    }
  }
  xine_sarray_delete (kbt->key_index);
  xine_sarray_delete (kbt->action_index);
  free (kbt);
  *_kbt = NULL;
}

/*
 * Return a duplicated table from kbt.
 */
kbinding_t *_kbindings_duplicate_kbindings (kbinding_t *kbt) {
  int         i;
  kbinding_t *k;

  ABORT_IF_NULL(kbt);

  k = (kbinding_t *)calloc (1, sizeof (*k));
  if (!k)
    return NULL;

  k->gui = kbt->gui;
  k->last = NULL;
  k->action_index = xine_sarray_new (MAX_ENTRIES, _kbindings_action_cmp);
  k->key_index = xine_sarray_new (MAX_ENTRIES, _kbindings_key_cmp);
  if (!k->action_index || !k->key_index) {
    xine_sarray_delete (k->action_index);
    xine_sarray_delete (k->key_index);
    free (k);
    return NULL;
  }

  for (i = 0; i < (int)KBT_NUM_BASE; i++) {
    kbinding_entry_t *n = k->base + i, *o = kbt->base + i;
    n->action     = o->action;
    n->action_id  = o->action_id;
    n->modifier   = o->modifier;
    n->index      = i;
    n->is_alias   = o->is_alias;
    n->is_gui     = o->is_gui;
    n->is_default = o->is_default;
    n->comment    = o->comment;
    n->key        = refs_ref (o->key);
    kbindings_index_add (k, n);
  }
  for (; i < kbt->num_entries; i++) {
    kbinding_entry_t *o = kbt->alias[i - KBT_NUM_BASE];
    kbinding_entry_t *n = (kbinding_entry_t *)malloc (sizeof (*n));
    if (!n)
      break;
    n->action     = o->action;
    n->action_id  = o->action_id;
    n->modifier   = o->modifier;
    n->index      = i;
    n->is_alias   = o->is_alias;
    n->is_gui     = o->is_gui;
    n->is_default = o->is_default;
    n->comment    = o->comment;
    n->key        = refs_ref (o->key);
    kbindings_index_add (k, n);
    k->alias[i - KBT_NUM_BASE] = n;
  }
  k->num_entries = i;

  return k;
}

kbinding_entry_t *kbindings_find_key (kbinding_t *kbt, const char *key, int modifier) {
  kbinding_entry_t *e;

  if (!kbt || !key)
    return NULL;

  e = kbt->last;
  if (e && !strcmp (e->key, key) && (e->modifier == modifier))
    return e;

  {
    refs_dummy_t dummy;
    kbinding_entry_t e2 = {
      .key = refs_set_dummy (&dummy, key),
      .modifier = modifier
    };
    int i = xine_sarray_binary_search (kbt->key_index, &e2);
    if (i >= 0) {
      kbt->last = e = xine_sarray_get (kbt->key_index, i);
      return e;
    }
  }
  return NULL;
}

int kbindings_entry_set (kbinding_t *kbt, int index, int modifier, const char *key) {
  kbinding_entry_t *e;

  /* paranoia */
  if ((index < 0) || (index >= kbt->num_entries))
    return -3;
  e = _kbt_entry (kbt, index);
  if (!e)
    return -3;

  if (!strcasecmp (key, "VOID")) {
    /* delete */
    if (!strcasecmp (e->key, "VOID"))
      return -2;
    if (index >= (int)KBT_NUM_BASE) {
      /* remove alias */
      kbindings_index_remove (kbt, e);
      e->action = NULL;
      e->comment = NULL;
      refs_unref (&e->key);
      free (e);
      kbt->num_entries--;
      for (; index < kbt->num_entries; index++) {
        kbt->alias[index - KBT_NUM_BASE] = kbt->alias[index - KBT_NUM_BASE + 1];
        kbt->alias[index - KBT_NUM_BASE]->index = index;
      }
      return -1;
    } else {
      kbinding_entry_t dummy = {
        .action = e->action,
        .is_alias = 1
      };
      int ai = xine_sarray_binary_search (kbt->action_index, &dummy);
      if (ai >= 0) {
        /* turn alias into new base entry */
        kbinding_entry_t *a = xine_sarray_get (kbt->action_index, ai);
        ai = a->index;
        if ((ai >= (int)KBT_NUM_BASE) && (ai < kbt->num_entries)) {
          kbindings_index_remove (kbt, e);
          kbindings_index_remove (kbt, a);
          e->modifier = a->modifier;
          refs_unref (&e->key);
          e->key = a->key;
          a->key = NULL;
          e->comment = a->comment;
          a->comment = NULL;
          free (a);
          kbt->num_entries--;
          for (; ai < kbt->num_entries; ai++) {
            kbt->alias[ai - KBT_NUM_BASE] = kbt->alias[ai - KBT_NUM_BASE + 1];
            kbt->alias[ai - KBT_NUM_BASE]->index = ai;
          }
          kbindings_index_add (kbt, e);
          e->is_default = (!strcmp (e->key, default_binding_table[index].key)
                        && (e->modifier == default_binding_table[index].modifier));
          return -1;
        }
      }
      /* keep base entry, just reset */
      kbindings_index_remove (kbt, e);
      refs_unref (&e->key);
      e->key = refs_strdup ("VOID");
      e->modifier = 0;
      kbindings_index_add (kbt, e);
      e->is_default = (!strcmp (e->key, default_binding_table[index].key)
                    && (e->modifier == default_binding_table[index].modifier));
      return -1;
    }
  } else {
    if (!strcmp (e->key, key) && (e->modifier == modifier)) {
      /* no change */
      return -2;
    }
    {
      refs_dummy_t dummy;
      kbinding_entry_t e2 = {
        .key = refs_set_dummy (&dummy, key),
        .modifier = modifier
      };
      int i = xine_sarray_binary_search (kbt->key_index, &e2);
      if (i >= 0) {
        /* key already in use */
        e = xine_sarray_get (kbt->key_index, i);
        i = e->index;
        return ((i < kbt->num_entries) && (_kbt_entry (kbt, i) == e)) ? i : -3;
      }
    }
    /* set new key */
    kbindings_index_remove (kbt, e);
    refs_unref (&e->key);
    e->key = refs_strdup (key);
    e->modifier = modifier;
    kbindings_index_add (kbt, e);
    if (index < (int)KBT_NUM_BASE)
      e->is_default = (!strcmp (e->key, default_binding_table[index].key)
                    && (e->modifier == default_binding_table[index].modifier));
    return -1;
  }
}

int kbindings_alias_add (kbinding_t *kbt, int index, int modifier, const char *key) {
  kbinding_entry_t *e;

  /* paranoia */
  if ((index < 0) || (index >= kbt->num_entries))
    return -3;
  e = _kbt_entry (kbt, index);
  if (!e)
    return -3;
  if (kbt->num_entries >= MAX_ENTRIES)
    return -4;

  if (!strcasecmp (key, "VOID"))
    return -2;
  {
    refs_dummy_t dummy;
    kbinding_entry_t e2 = {
      .key = refs_set_dummy (&dummy, key),
      .modifier = modifier
    };
    int i = xine_sarray_binary_search (kbt->key_index, &e2);
    if (i >= 0) {
      /* key already in use */
      e = xine_sarray_get (kbt->key_index, i);
      i = e->index;
      return ((i < kbt->num_entries) && (_kbt_entry (kbt, i) == e)) ? i : -3;
    }
  }
  {
    /* set new alias */
    kbinding_entry_t *a = malloc (sizeof (*a));
    if (!a)
      return -3;
    a->action = e->action;
    a->action_id = e->action_id;
    a->modifier = modifier;
    a->is_alias = 1;
    a->is_gui = e->is_gui;
    a->key = refs_strdup (key);
    a->comment = e->comment;
    a->index = kbt->num_entries;
    kbt->alias[kbt->num_entries - KBT_NUM_BASE] = a;
    kbt->num_entries++;
    kbindings_index_add (kbt, a);
    return -1;
  }
}

const kbinding_entry_t *_kbindings_get_entry (kbinding_t *kbt, int index) {
  if (!kbt)
    return NULL;
  if (index < (int)KBT_NUM_BASE)
    return index < 0 ? NULL : kbt->base + index;
  else
    return index >= kbt->num_entries ? NULL : kbt->alias[index - KBT_NUM_BASE];
}

int _kbindings_get_num_entries (kbinding_t *kbt) {
  return kbt ? kbt->num_entries : 0;
}

int kbindings_reset (kbinding_t *kbt, int index) {
  if (!kbt)
    return -3;
  if (index < 0) {
    for (index = 0; index < (int)KBT_NUM_BASE; index++) {
      kbt->base[index].is_default = 1;
      kbt->base[index].modifier = default_binding_table[index].modifier;
      if (strcmp (kbt->base[index].key, default_binding_table[index].key)) {
        refs_unref (&kbt->base[index].key);
        kbt->base[index].key = refs_strdup (default_binding_table[index].key);
      }
    }
    for (; index < kbt->num_entries; index++) {
      kbinding_entry_t *entry = kbt->alias[index - KBT_NUM_BASE];
      kbt->alias[index - KBT_NUM_BASE] = NULL;
      entry->action = NULL;
      entry->comment = NULL;
      refs_unref (&entry->key);
      free (entry);
    }
    kbt->num_entries = KBT_NUM_BASE;
    return -1;
  }
  if (index < (int)KBT_NUM_BASE)
    return kbindings_entry_set (kbt, index, default_binding_table[index].modifier, default_binding_table[index].key);
  return kbindings_entry_set (kbt, index, 0, "VOID");
}
