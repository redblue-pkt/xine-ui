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

static char *refs_strdup (const char *text) {
  size_t l;
  char *m;

  if (!text)
    return NULL;
  l = strlen (text) + 1;
  m = malloc (4 + l);
  if (!m)
    return NULL;
  m += 4;
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
  
/* used on remapping */
typedef struct {
  const char *alias, *action, *key;
  char *modifier;
  int is_alias, is_gui;
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
static const struct {
  const char       *comment;     /* Comment automatically added in xbinding_display*() outputs */
  const char       *action;      /* Human readable action, used in config file too */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  const char        key[9];      /* key binding */
  uint8_t           modifier;    /* Modifier key of binding (can be OR'ed) */
  uint8_t           is_alias : 1;/* is made from an alias entry ? */
  uint8_t           is_gui : 1;
}  default_binding_table[] = {
  { N_("start playback"),
    "Play",                   ACTID_PLAY                    , "Return",   KEYMOD_NOMOD   , 0 , 0},
  { N_("playback pause toggle"),
    "Pause",                  ACTID_PAUSE                   , "space",    KEYMOD_NOMOD   , 0 , 0},
  { N_("stop playback"),
    "Stop",                   ACTID_STOP                    , "S",        KEYMOD_NOMOD   , 0 , 0},
  { N_("close stream"),
    "Close",                  ACTID_CLOSE                   , "C",        KEYMOD_NOMOD   , 0 , 0},
  { N_("take a snapshot"),
    "Snapshot",               ACTID_SNAPSHOT                , "t",        KEYMOD_NOMOD   , 0 , 0},
  { N_("eject the current medium"),
    "Eject",                  ACTID_EJECT                   , "e",        KEYMOD_NOMOD   , 0 , 0},
  { N_("select and play next MRL in the playlist"),
    "NextMrl",                ACTID_MRL_NEXT                , "Next",     KEYMOD_NOMOD   , 0 , 0},
  { N_("select and play previous MRL in the playlist"),
    "PriorMrl",               ACTID_MRL_PRIOR               , "Prior",    KEYMOD_NOMOD   , 0 , 0},
  { N_("select and play MRL in the playlist"),
    "SelectMrl",              ACTID_MRL_SELECT              , "Select",   KEYMOD_NOMOD   , 0 , 0},
  { N_("loop mode toggle"),
    "ToggleLoopMode",         ACTID_LOOPMODE	            , "l",	  KEYMOD_NOMOD   , 0 , 0},
  { N_("stop playback after played stream"),
    "PlaylistStop",           ACTID_PLAYLIST_STOP           , "l",	  KEYMOD_CONTROL , 0 , 0},
  { N_("scan playlist to grab stream infos"),
    "ScanPlaylistInfo",       ACTID_SCANPLAYLIST            , "s",        KEYMOD_CONTROL , 0 , 0},
  { N_("add a mediamark from current playback"),
    "AddMediamark",           ACTID_ADDMEDIAMARK            , "a",        KEYMOD_CONTROL , 0 , 0},
  { N_("edit selected mediamark"),
    "MediamarkEditor",        ACTID_MMKEDITOR               , "e",        KEYMOD_CONTROL , 0 , 1},
  { N_("set position to -60 seconds in current stream"),
    "SeekRelative-60",        ACTID_SEEK_REL_m60            , "Left",     KEYMOD_NOMOD   , 0 , 0},
  { N_("set position to +60 seconds in current stream"),
    "SeekRelative+60",        ACTID_SEEK_REL_p60            , "Right",    KEYMOD_NOMOD   , 0 , 0},
  { N_("set position to -30 seconds in current stream"),
    "SeekRelative-30",        ACTID_SEEK_REL_m30            , "Left",     KEYMOD_META    , 0 , 0},
  { N_("set position to +30 seconds in current stream"),
    "SeekRelative+30",        ACTID_SEEK_REL_p30            , "Right",    KEYMOD_META    , 0 , 0},
  { N_("set position to -15 seconds in current stream"),
    "SeekRelative-15",        ACTID_SEEK_REL_m15            , "Left",     KEYMOD_CONTROL , 0 , 0},
  { N_("set position to +15 seconds in current stream"),
    "SeekRelative+15",        ACTID_SEEK_REL_p15            , "Right",    KEYMOD_CONTROL , 0 , 0},
  { N_("set position to -7 seconds in current stream"),
    "SeekRelative-7",         ACTID_SEEK_REL_m7             , "Left",     KEYMOD_MOD3    , 0 , 0},
  { N_("set position to +7 seconds in current stream"),
    "SeekRelative+7",         ACTID_SEEK_REL_p7             , "Right",    KEYMOD_MOD3    , 0 , 0},
  { N_("set position to beginning of current stream"),
    "SetPosition0%",          ACTID_SET_CURPOS_0            , "0",        KEYMOD_CONTROL , 0 , 0},
  /* NOTE: these used to be "... to 10% of ..." etc. but msgmerge seems not to like such "c-format". */
  { N_("set position to 1/10 of current stream"),
    "SetPosition10%",         ACTID_SET_CURPOS_10           , "1",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 2/10 of current stream"),
    "SetPosition20%",         ACTID_SET_CURPOS_20           , "2",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 3/10 of current stream"),
    "SetPosition30%",         ACTID_SET_CURPOS_30           , "3",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 4/10 of current stream"),
    "SetPosition40%",         ACTID_SET_CURPOS_40           , "4",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 5/10 of current stream"),
    "SetPosition50%",         ACTID_SET_CURPOS_50           , "5",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 6/10 of current stream"),
    "SetPosition60%",         ACTID_SET_CURPOS_60           , "6",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 7/10 of current stream"),
    "SetPosition70%",         ACTID_SET_CURPOS_70           , "7",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 8/10 of current stream"),
    "SetPosition80%",         ACTID_SET_CURPOS_80           , "8",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to 9/10 of current stream"),
    "SetPosition90%",         ACTID_SET_CURPOS_90           , "9",        KEYMOD_CONTROL , 0 , 0},
  { N_("set position to end of current stream"),
    "SetPosition100%",        ACTID_SET_CURPOS_100          , "End",      KEYMOD_CONTROL , 0 , 0},
  { N_("increment playback speed"),
    "SpeedFaster",            ACTID_SPEED_FAST              , "Up",       KEYMOD_NOMOD   , 0 , 0},
  { N_("decrement playback speed"),
    "SpeedSlower",            ACTID_SPEED_SLOW              , "Down",     KEYMOD_NOMOD   , 0 , 0},
  { N_("reset playback speed"),
    "SpeedReset",             ACTID_SPEED_RESET             , "Down",     KEYMOD_META    , 0 , 0},
  { N_("increment audio volume"),
    "Volume+",                ACTID_pVOLUME                 , "V",        KEYMOD_NOMOD   , 0 , 0},
  { N_("decrement audio volume"),
    "Volume-",                ACTID_mVOLUME                 , "v",        KEYMOD_NOMOD   , 0 , 0},
  { N_("increment amplification level"),
    "Amp+",                   ACTID_pAMP                    , "V",        KEYMOD_CONTROL , 0 , 0},
  { N_("decrement amplification level"),
    "Amp-",                   ACTID_mAMP                    , "v",        KEYMOD_CONTROL , 0 , 0},
  { N_("reset amplification to default value"),
    "ResetAmp",               ACTID_AMP_RESET               , "A",        KEYMOD_CONTROL , 0 , 0},
  { N_("audio muting toggle"),
    "Mute",                   ACTID_MUTE                    , "m",        KEYMOD_CONTROL , 0 , 0},
  { N_("select next audio channel"),
    "AudioChannelNext",       ACTID_AUDIOCHAN_NEXT          , "plus",     KEYMOD_NOMOD   , 0 , 0},
  { N_("select previous audio channel"),
    "AudioChannelPrior",      ACTID_AUDIOCHAN_PRIOR         , "minus",    KEYMOD_NOMOD   , 0 , 0},
  { N_("visibility toggle of audio post effect window"),
    "APProcessShow",          ACTID_APP                     , "VOID",     KEYMOD_NOMOD   , 0 , 1},
  { N_("toggle post effect usage"),
    "APProcessEnable",        ACTID_APP_ENABLE              , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("select next sub picture (subtitle) channel"),
    "SpuNext",                ACTID_SPU_NEXT                , "period",   KEYMOD_NOMOD   , 0 , 0},
  { N_("select previous sub picture (subtitle) channel"),
    "SpuPrior",               ACTID_SPU_PRIOR               , "comma",    KEYMOD_NOMOD   , 0 , 0},
  { N_("interlaced mode toggle"),
    "ToggleInterleave",       ACTID_TOGGLE_INTERLEAVE       , "i",        KEYMOD_NOMOD   , 0 , 0},
  { N_("cycle aspect ratio values"),
    "ToggleAspectRatio",      ACTID_TOGGLE_ASPECT_RATIO     , "a",        KEYMOD_NOMOD   , 0 , 0},
  { N_("reduce the output window size by factor 1.2"),
    "WindowReduce",           ACTID_WINDOWREDUCE            , "less",     KEYMOD_NOMOD   , 0 , 0},
  { N_("enlarge the output window size by factor 1.2"),
    "WindowEnlarge",          ACTID_WINDOWENLARGE           , "greater",  KEYMOD_NOMOD   , 0 , 0},
  { N_("set video output window to 50%"),
    "Window50",               ACTID_WINDOW50                , "1",        KEYMOD_META    , 0 , 0},
  { N_("set video output window to 100%"),
    "Window100",              ACTID_WINDOW100               , "2",        KEYMOD_META    , 0 , 0},
  { N_("set video output window to 200%"),
    "Window200",              ACTID_WINDOW200               , "3",        KEYMOD_META    , 0 , 0},
  { N_("zoom in"),
    "ZoomIn",                 ACTID_ZOOM_IN                 , "z",        KEYMOD_NOMOD   , 0 , 0},
  { N_("zoom out"),
    "ZoomOut",                ACTID_ZOOM_OUT                , "Z",        KEYMOD_NOMOD   , 0 , 0},
  { N_("zoom in horizontally"),
    "ZoomInX",                ACTID_ZOOM_X_IN               , "z",        KEYMOD_CONTROL , 0 , 0},
  { N_("zoom out horizontally"),
    "ZoomOutX",               ACTID_ZOOM_X_OUT              , "Z",        KEYMOD_CONTROL , 0 , 0},
  { N_("zoom in vertically"),
    "ZoomInY",                ACTID_ZOOM_Y_IN               , "z",        KEYMOD_META    , 0 , 0},
  { N_("zoom out vertically"),
    "ZoomOutY",               ACTID_ZOOM_Y_OUT              , "Z",        KEYMOD_META    , 0 , 0},
  { N_("reset zooming"),
    "ZoomReset",              ACTID_ZOOM_RESET              , "z",        KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { N_("resize output window to stream size"),
    "Zoom1:1",                ACTID_ZOOM_1_1                , "s",        KEYMOD_NOMOD   , 0 , 0},
  { N_("fullscreen toggle"),
    "ToggleFullscreen",       ACTID_TOGGLE_FULLSCREEN       , "f",        KEYMOD_NOMOD   , 0 , 0},
#ifdef HAVE_XINERAMA
  { N_("Xinerama fullscreen toggle"),
    "ToggleXineramaFullscr",  ACTID_TOGGLE_XINERAMA_FULLSCREEN
                                                            , "F",        KEYMOD_NOMOD   , 0 , 0},
#endif
  { N_("jump to media Menu"),
    "Menu",                   ACTID_EVENT_MENU1             , "Escape",   KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Title Menu"),
    "TitleMenu",              ACTID_EVENT_MENU2             , "F1",       KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Root Menu"),
    "RootMenu",               ACTID_EVENT_MENU3             , "F2",       KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Subpicture Menu"),
    "SubpictureMenu",         ACTID_EVENT_MENU4             , "F3",       KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Audio Menu"),
    "AudioMenu",              ACTID_EVENT_MENU5             , "F4",       KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Angle Menu"),
    "AngleMenu",              ACTID_EVENT_MENU6             , "F5",       KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to Part Menu"),
    "PartMenu",               ACTID_EVENT_MENU7             , "F6",       KEYMOD_NOMOD   , 0 , 0},
  { N_("menu navigate up"),
    "EventUp",                ACTID_EVENT_UP                , "KP_Up",    KEYMOD_NOMOD   , 0 , 0},
  { N_("menu navigate down"),
    "EventDown",              ACTID_EVENT_DOWN              , "KP_Down",  KEYMOD_NOMOD   , 0 , 0},
  { N_("menu navigate left"),
    "EventLeft",              ACTID_EVENT_LEFT              , "KP_Left",  KEYMOD_NOMOD   , 0 , 0},
  { N_("menu navigate right"),
    "EventRight",             ACTID_EVENT_RIGHT             , "KP_Right", KEYMOD_NOMOD   , 0 , 0},
  { N_("menu select"),
    "EventSelect",            ACTID_EVENT_SELECT            , "KP_Enter", KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to next chapter"),
    "EventNext",              ACTID_EVENT_NEXT              , "KP_Next",  KEYMOD_NOMOD   , 0 , 0},
  { N_("jump to previous chapter"),
    "EventPrior",             ACTID_EVENT_PRIOR             , "KP_Prior", KEYMOD_NOMOD   , 0 , 0},
  { N_("select next angle"),
    "EventAngleNext",         ACTID_EVENT_ANGLE_NEXT        , "KP_Home",  KEYMOD_NOMOD   , 0 , 0},
  { N_("select previous angle"),
    "EventAnglePrior",        ACTID_EVENT_ANGLE_PRIOR       , "KP_End",   KEYMOD_NOMOD   , 0 , 0},
  { N_("visibility toggle of help window"),
    "HelpShow",               ACTID_HELP_SHOW               , "h",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of video post effect window"),
    "VPProcessShow",          ACTID_VPP                     , "P",        KEYMOD_META    , 0 , 1},
  { N_("toggle post effect usage"),
    "VPProcessEnable",        ACTID_VPP_ENABLE              , "P",        KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { N_("visibility toggle of output window"),
    "ToggleWindowVisibility", ACTID_TOGGLE_WINOUT_VISIBLITY , "h",        KEYMOD_NOMOD   , 0 , 1},
  { N_("bordered window toggle of output window"),
    "ToggleWindowBorder",     ACTID_TOGGLE_WINOUT_BORDER    , "b",        KEYMOD_NOMOD   , 0 , 1},
  { N_("visibility toggle of UI windows"),
    "ToggleVisibility",       ACTID_TOGGLE_VISIBLITY        , "g",        KEYMOD_NOMOD   , 0 , 1},
  { N_("visibility toggle of control window"),
    "ControlShow",            ACTID_CONTROLSHOW             , "c",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of mrl browser window"),
    "MrlBrowser",             ACTID_MRLBROWSER              , "m",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of playlist editor window"),
    "PlaylistEditor",         ACTID_PLAYLIST                , "p",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of the setup window"),
    "SetupShow",              ACTID_SETUP                   , "s",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of the event sender window"),
    "EventSenderShow",        ACTID_EVENT_SENDER            , "e",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of analog TV window"),
    "TVAnalogShow",           ACTID_TVANALOG                , "t",        KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of log viewer"),
    "ViewlogShow",            ACTID_VIEWLOG	            , "l",	  KEYMOD_META    , 0 , 1},
  { N_("visibility toggle of stream info window"),
    "StreamInfosShow",        ACTID_STREAM_INFOS            , "i",        KEYMOD_META    , 0 , 1},
  { N_("display stream information using OSD"),
    "OSDStreamInfos",         ACTID_OSD_SINFOS              , "i",        KEYMOD_CONTROL , 0 , 0},
  { N_("display information using OSD"),
    "OSDWriteText",           ACTID_OSD_WTEXT               , "VOID",     KEYMOD_CONTROL , 0 , 0},
  { N_("show OSD menu"),
    "OSDMenu",                ACTID_OSD_MENU                , "O",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter key binding editor"),
    "KeyBindingEditor",       ACTID_KBEDIT	            , "k",	  KEYMOD_META    , 0 , 1},
  { N_("enable key bindings (not useful to bind a key to it!)"),
    "KeyBindingsEnable",      ACTID_KBENABLE	            , "VOID",	  KEYMOD_NOMOD   , 0 , 0},
  { N_("open file selector"),
    "FileSelector",           ACTID_FILESELECTOR            , "o",        KEYMOD_CONTROL , 0 , 1},
  { N_("select a subtitle file"),
    "SubSelector",            ACTID_SUBSELECT               , "S",        KEYMOD_CONTROL , 0 , 1},
#ifdef HAVE_CURL
  { N_("download a skin from the skin server"),
    "SkinDownload",           ACTID_SKINDOWNLOAD            , "d",        KEYMOD_CONTROL , 0 , 1},
#endif
  { N_("display MRL/Ident toggle"),
    "MrlIdentToggle",         ACTID_MRLIDENTTOGGLE          , "t",        KEYMOD_CONTROL , 0 , 0},
  { N_("grab pointer toggle"),
    "GrabPointer",            ACTID_GRAB_POINTER            , "Insert",   KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 0"),
    "Number0",                ACTID_EVENT_NUMBER_0          , "0",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 1"),
    "Number1",                ACTID_EVENT_NUMBER_1          , "1",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 2"),
    "Number2",                ACTID_EVENT_NUMBER_2          , "2",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 3"),
    "Number3",                ACTID_EVENT_NUMBER_3          , "3",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 4"),
    "Number4",                ACTID_EVENT_NUMBER_4          , "4",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 5"),
    "Number5",                ACTID_EVENT_NUMBER_5          , "5",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 6"),
    "Number6",                ACTID_EVENT_NUMBER_6          , "6",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 7"),
    "Number7",                ACTID_EVENT_NUMBER_7          , "7",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 8"),
    "Number8",                ACTID_EVENT_NUMBER_8          , "8",        KEYMOD_NOMOD   , 0 , 0},
  { N_("enter the number 9"),
    "Number9",                ACTID_EVENT_NUMBER_9          , "9",        KEYMOD_NOMOD   , 0 , 0},
  { N_("add 10 to the next entered number"),
    "Number10add",            ACTID_EVENT_NUMBER_10_ADD     , "plus",     KEYMOD_MOD3    , 0 , 0},
  { N_("set position in current stream to numeric percentage"),
    "SetPosition%",           ACTID_SET_CURPOS              , "slash",    KEYMOD_NOMOD   , 0 , 0},
  { N_("set position forward by numeric argument in current stream"),
    "SeekRelative+",          ACTID_SEEK_REL_p              , "Up",       KEYMOD_META    , 0 , 0},
  { N_("set position back by numeric argument in current stream"),
    "SeekRelative-",          ACTID_SEEK_REL_m              , "Up",       KEYMOD_MOD3    , 0 , 0},
  { N_("change audio video syncing (delay video)"),
    "AudioVideoDecay+",       ACTID_AV_SYNC_p3600           , "m",        KEYMOD_NOMOD   , 0 , 0},
  { N_("change audio video syncing (delay audio)"),
    "AudioVideoDecay-",       ACTID_AV_SYNC_m3600           , "n",        KEYMOD_NOMOD   , 0 , 0},
  { N_("reset audio video syncing offset"),
    "AudioVideoDecayReset",   ACTID_AV_SYNC_RESET           , "Home",     KEYMOD_NOMOD   , 0 , 0},
  { N_("change subtitle syncing (delay video)"),
    "SpuVideoDecay+",         ACTID_SV_SYNC_p               , "M",        KEYMOD_NOMOD   , 0 , 0},
  { N_("change subtitle syncing (delay subtitles)"),
    "SpuVideoDecay-",         ACTID_SV_SYNC_m               , "N",        KEYMOD_NOMOD   , 0 , 0},
  { N_("reset subtitle syncing offset"),
    "SpuVideoDecayReset",     ACTID_SV_SYNC_RESET           , "End",      KEYMOD_NOMOD   , 0 , 0},
  { N_("toggle TV modes (on the DXR3)"),
    "ToggleTVmode",           ACTID_TOGGLE_TVMODE	    , "o",	  KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { N_("switch Monitor to DPMS standby mode"),
    "DPMSStandby",            ACTID_DPMSSTANDBY             , "d",        KEYMOD_NOMOD   , 0 , 0},
  { N_("increase hue by 10"),
    "HueControl+",            ACTID_HUECONTROLp             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease hue by 10"),
    "HueControl-",            ACTID_HUECONTROLm             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase saturation by 10"),
    "SaturationControl+",     ACTID_SATURATIONCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease saturation by 10"),
    "SaturationControl-",     ACTID_SATURATIONCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase brightness by 10"),
    "BrightnessControl+",     ACTID_BRIGHTNESSCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease brightness by 10"),
    "BrightnessControl-",     ACTID_BRIGHTNESSCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase contrast by 10"),
    "ContrastControl+",       ACTID_CONTRASTCONTROLp        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease contrast by 10"),
    "ContrastControl-",       ACTID_CONTRASTCONTROLm        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase gamma by 10"),
    "GammaControl+",          ACTID_GAMMACONTROLp           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease gamma by 10"),
    "GammaControl-",          ACTID_GAMMACONTROLm           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase sharpness by 10"),
    "SharpnessControl+",      ACTID_SHARPNESSCONTROLp       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease sharpness by 10"),
    "SharpnessControl-",      ACTID_SHARPNESSCONTROLm       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("increase noise reduction by 10"),
    "NoiseReductionControl+", ACTID_NOISEREDUCTIONCONTROLp  , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("decrease noise reduction by 10"),
    "NoiseReductionControl-", ACTID_NOISEREDUCTIONCONTROLm  , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("quit the program"),
    "Quit",                   ACTID_QUIT                    , "q",        KEYMOD_NOMOD   , 0 , 0},
  { N_("input_pvr: set input"),
    "PVRSetInput",            ACTID_PVR_SETINPUT            , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("input_pvr: set frequency"),
    "PVRSetFrequency",        ACTID_PVR_SETFREQUENCY        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("input_pvr: mark the start of a new stream section"),
    "PVRSetMark",             ACTID_PVR_SETMARK             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("input_pvr: set the name for the current stream section"),
    "PVRSetName",             ACTID_PVR_SETNAME             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("input_pvr: save the stream section"),
    "PVRSave",                ACTID_PVR_SAVE                , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("Open playlist"),
    "PlaylistOpen",           ACTID_PLAYLIST_OPEN           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef ENABLE_VDR_KEYS
  { N_("VDR Red button"),
    "VDRButtonRed",           ACTID_EVENT_VDR_RED            , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Green button"),
    "VDRButtonGreen",         ACTID_EVENT_VDR_GREEN          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Yellow button"),
    "VDRButtonYellow",        ACTID_EVENT_VDR_YELLOW         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Blue button"),
    "VDRButtonBlue",          ACTID_EVENT_VDR_BLUE           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Play"),
    "VDRPlay",                ACTID_EVENT_VDR_PLAY           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Pause"),
    "VDRPause",               ACTID_EVENT_VDR_PAUSE          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Stop"),
    "VDRStop",                ACTID_EVENT_VDR_STOP           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Record"),
    "VDRRecord",              ACTID_EVENT_VDR_RECORD         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Fast forward"),
    "VDRFastFwd",             ACTID_EVENT_VDR_FASTFWD        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Fast rewind"),
    "VDRFastRew",             ACTID_EVENT_VDR_FASTREW        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Power"),
    "VDRPower",               ACTID_EVENT_VDR_POWER          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Channel +"),
    "VDRChannelPlus",         ACTID_EVENT_VDR_CHANNELPLUS    , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Channel -"),
    "VDRChannelMinus",        ACTID_EVENT_VDR_CHANNELMINUS   , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Schedule menu"),
    "VDRSchedule",            ACTID_EVENT_VDR_SCHEDULE       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Channel menu"),
    "VDRChannels",            ACTID_EVENT_VDR_CHANNELS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Timers menu"),
    "VDRTimers",              ACTID_EVENT_VDR_TIMERS         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Recordings menu"),
    "VDRRecordings",          ACTID_EVENT_VDR_RECORDINGS     , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Setup menu"),
    "VDRSetup",               ACTID_EVENT_VDR_SETUP          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Command menu"),
    "VDRCommands",            ACTID_EVENT_VDR_COMMANDS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Command back"),
    "VDRBack",                ACTID_EVENT_VDR_BACK           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef XINE_EVENT_VDR_USER0 /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR User command 0"),
    "VDRUser0",               ACTID_EVENT_VDR_USER0          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#endif
  { N_("VDR User command 1"),
    "VDRUser1",               ACTID_EVENT_VDR_USER1          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 2"),
    "VDRUser2",               ACTID_EVENT_VDR_USER2          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 3"),
    "VDRUser3",               ACTID_EVENT_VDR_USER3          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 4"),
    "VDRUser4",               ACTID_EVENT_VDR_USER4          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 5"),
    "VDRUser5",               ACTID_EVENT_VDR_USER5          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 6"),
    "VDRUser6",               ACTID_EVENT_VDR_USER6          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 7"),
    "VDRUser7",               ACTID_EVENT_VDR_USER7          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 8"),
    "VDRUser8",               ACTID_EVENT_VDR_USER8          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR User command 9"),
    "VDRUser9",               ACTID_EVENT_VDR_USER9          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Volume +"),
    "VDRVolumePlus",          ACTID_EVENT_VDR_VOLPLUS        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Volume -"),
    "VDRVolumeMinus",         ACTID_EVENT_VDR_VOLMINUS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Mute audio"),
    "VDRMute",                ACTID_EVENT_VDR_MUTE           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Audio menu"),
    "VDRAudio",               ACTID_EVENT_VDR_AUDIO          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { N_("VDR Command info"),
    "VDRInfo",                ACTID_EVENT_VDR_INFO           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef XINE_EVENT_VDR_CHANNELPREVIOUS /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR Previous channel"),
    "VDRChannelPrevious",     ACTID_EVENT_VDR_CHANNELPREVIOUS, "VOID",     KEYMOD_NOMOD   , 0 , 0},
#endif
#ifdef XINE_EVENT_VDR_SUBTITLES /* #ifdef is precaution for backward compatibility at the moment */
  { N_("VDR Subtiles menu"),
    "VDRSubtitles",           ACTID_EVENT_VDR_SUBTITLES      , "VOID",     KEYMOD_NOMOD   , 0 , 0}
#endif
#endif
};

#define KBT_NUM_BASE (sizeof (default_binding_table) / sizeof (default_binding_table[0]))
#define _kbt_entry(kbt,index) ((unsigned int)index < KBT_NUM_BASE ? kbt->base + index : kbt->alias[index - KBT_NUM_BASE])

struct kbinding_s {
  int               num_entries;
  xine_sarray_t    *action_index, *key_index;
  kbinding_entry_t *last, base[KBT_NUM_BASE], *alias[MAX_ENTRIES - KBT_NUM_BASE];
};

static int _kbindings_action_cmp (void *a, void *b) {
  kbinding_entry_t *d = (kbinding_entry_t *)a;
  kbinding_entry_t *e = (kbinding_entry_t *)b;
  int f = strcasecmp (d->action, e->action);
  if (f)
    return f;
  return (int)d->is_alias - (int)e->is_alias;
}

static int _kbindings_key_cmp (void *a, void *b) {
  kbinding_entry_t *d = (kbinding_entry_t *)a;
  kbinding_entry_t *e = (kbinding_entry_t *)b;
  int f = strcmp (d->key, e->key); /* "A" != "a" */
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

kbinding_t *_kbindings_init_to_default(void) {
  kbinding_t  *kbt;
  int i;

  kbt = (kbinding_t *)malloc (sizeof (*kbt));
  if (!kbt)
    return NULL;

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
    n->action    = default_binding_table[i].action;
    n->action_id = default_binding_table[i].action_id;
    n->modifier  = default_binding_table[i].modifier;
    n->index     = i;
    n->is_alias  = default_binding_table[i].is_alias;
    n->is_gui    = default_binding_table[i].is_gui;
    n->comment   = refs_strdup (gettext (default_binding_table[i].comment));
    n->key       = refs_strdup (default_binding_table[i].key);
    kbindings_index_add (kbt, n);
  }
  kbt->num_entries = i;

  return kbt;
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

static int _kbindings_modifier_from_string (char *s) {
  int modifier = KEYMOD_NOMOD;
  if (s) {
    char *p, *t = s;
    while ((p = xine_strsep (&t, ",")) != NULL) {
      _kbindings_set_pos_to_next_char (&p);
      if (p) {
        if (!strcasecmp (p, "none"))
          modifier = KEYMOD_NOMOD;
        else if (!strcasecmp(p, "control"))
          modifier |= KEYMOD_CONTROL;
        else if (!strcasecmp(p, "ctrl"))
          modifier |= KEYMOD_CONTROL;
        else if (!strcasecmp(p, "meta"))
          modifier |= KEYMOD_META;
        else if (!strcasecmp(p, "alt"))
          modifier |= KEYMOD_META;
        else if (!strcasecmp(p, "mod3"))
          modifier |= KEYMOD_MOD3;
        else if (!strcasecmp(p, "mod4"))
          modifier |= KEYMOD_MOD4;
        else if (!strcasecmp(p, "mod5"))
          modifier |= KEYMOD_MOD5;
      }
    }
  }
  return modifier;
}

/*
 * Add an entry in key binding table kbt. Called when an Alias entry
 * is found.
 */
static void _kbindings_add_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  kbinding_entry_t  *k;
  int                modifier;

  /* This should only happen if the keymap file was edited manually. */
  if(kbt->num_entries >= MAX_ENTRIES) {
    fprintf(stderr, _("xine-ui: Too many Alias entries in keymap file, entry ignored.\n"));
    return;
  }

  k = kbindings_lookup_action (kbt, ukb->alias);
  if (k) {
    kbinding_entry_t *n = (kbinding_entry_t *)malloc (sizeof (*n));
    if (n) {
      modifier = ukb->modifier ? _kbindings_modifier_from_string (ukb->modifier) : k->modifier;

      /* Add new entry */
      n->action    = k->action;
      n->action_id = k->action_id;
      n->index     = kbt->num_entries;
      n->modifier  = modifier;
      n->is_alias  = 1;
      n->is_gui    = k->is_gui;
      n->comment   = refs_ref (k->comment);
      n->key       = refs_strdup (ukb->key);
      kbindings_index_add (kbt, n);
      kbt->alias[kbt->num_entries - KBT_NUM_BASE] = n;
      kbt->num_entries++;
    }
  }
}

/*
 * Change keystroke of modifier in entry.
 */
static void _kbindings_replace_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  kbinding_entry_t *e = kbindings_lookup_action (kbt, ukb->action);
  if (e) {
    int modifier = _kbindings_modifier_from_string (ukb->modifier);
    if (strcmp (e->key, ukb->key) || (e->modifier != modifier)) {
      kbindings_index_remove (kbt, e);
      refs_unref (&e->key);
      e->key = refs_strdup (ukb->key);
      e->modifier = modifier;
      if (e->index < KBT_NUM_BASE)
        e->is_gui = default_binding_table[e->index].is_gui;
      kbindings_index_add (kbt, e);
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

  if((kbdf->ln != NULL) && (*kbdf->ln != '\0')) {
    int found = 0;

    memset (&ukb, 0, sizeof (ukb));
    found = 1;

    p = kbdf->ln;

    if((brace_offset = _kbindings_begin_section(kbdf)) >= 0) {
      char buf[2048], *b1 = buf, *be = buf + sizeof (buf);
      *(kbdf->ln + brace_offset) = '\0';
      _kbindings_clean_eol(kbdf);
      ukb.action = b1;
      b1 += strlcpy (b1, kbdf->ln, be - b1);
      if (b1 > be)
        b1 = be;
      ukb.is_alias = 0;

      while(_kbindings_end_section(kbdf) < 0) {

	_kbindings_get_next_line(kbdf);
	p = kbdf->ln;
	if(kbdf->ln != NULL) {
	  if(!strncasecmp(kbdf->ln, "modifier", 8)) {
	    _kbindings_set_pos_to_value(&p);
            ukb.modifier = b1;
            b1 += strlcpy (b1, p, be - b1) + 1;
            if (b1 > be)
              b1 = be;
	  }
	  if(!strncasecmp(kbdf->ln, "entry", 5)) {
	    _kbindings_set_pos_to_value(&p);
            ukb.alias = b1;
            b1 += strlcpy (b1, p, be - b1) + 1;
            if (b1 > be)
              b1 = be;
	  }
	  else if(!strncasecmp(kbdf->ln, "key", 3)) {
	    _kbindings_set_pos_to_value(&p);
            ukb.key = b1;
            b1 += strlcpy (b1, p, be - b1) + 1;
            if (b1 > be)
              b1 = be;
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
  }
}

/*
 * Read and parse remap key binding file, if available.
 */
static void _kbinding_load_config(kbinding_t *kbt, char *file) {
  kbinding_file_t *kbdf;

  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(file);

  kbdf = (kbinding_file_t *) calloc(1, sizeof(kbinding_file_t));
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

/*
 * Check if there some redundant entries in key binding table kbt.
 */
#ifndef KBINDINGS_MAN
static void _kbinding_done (void *data, int state) {
  kbinding_t *kbt = data;
  if (state == 1)
    kbindings_reset_kbinding (kbt);
  else if (state == 2)
    kbedit_window (gGui);
}
#endif

static void _kbindings_check_redundancy(kbinding_t *kbt) {
  gGui_t *gui = gGui;
  kbinding_entry_t *e1;
  int n, i, found = 0;
  char *kmsg = NULL, *dna = NULL;

  if(kbt == NULL)
    return;

  n = xine_sarray_size (kbt->key_index);
  e1 = xine_sarray_get (kbt->key_index, 0);
  for (i = 1; i < n; i++) {
    kbinding_entry_t *e2 = xine_sarray_get (kbt->key_index, i);
    if (!_kbindings_key_cmp (e1, e2)) {
      const char *action1, *action2;
      found++;
      action1 = e1->action;
      action2 = e2->action;
      if (!kmsg) {
        const char *header = _("The following key bindings pairs are identical:\n\n");
        dna = _("and");
        kmsg = xitk_asprintf ("%s%s%c%s%c%s", header, action1,' ', dna, ' ', action2);
      } else {
        size_t len1 = strlen (kmsg);
        size_t len2 = strlen (action1) + 1 + strlen (dna) + 1 + strlen (action2) + 2;
        kmsg = (char *)realloc (kmsg, len1 + len2 + 1);
        sprintf (kmsg + len1, "%s%s%c%s%c%s", ", ", action1, ' ', dna, ' ', action2);
      }
    }
    e1 = e2;
  }

#ifndef KBINDINGS_MAN
  if(found) {
    char          *footer = _(".\n\nWhat do you want to do ?\n");

    kmsg = (char *) realloc(kmsg, strlen(kmsg) + strlen(footer) + 1);
    strlcat(kmsg, footer, strlen(kmsg) + strlen(footer) + 1);

    dump_error(kmsg);

    xitk_register_key_t key =
    xitk_window_dialog_3 (gui->xitk,
      NULL,
      get_layer_above_video (gui), 450, _("Keybindings error!"), _kbinding_done, kbt,
      _("Reset"), _("Editor"), _("Cancel"), NULL, 0, ALIGN_CENTER, "%s", kmsg);
    video_window_set_transient_for (gui->vwin, xitk_get_window (gui->xitk, key));
  }
#endif

  free(kmsg);
}

/*
 * Initialize a key binding table from default, then try
 * to remap this one with (system/user) remap files.
 */
kbinding_t *kbindings_init_kbinding(void) {
  gGui_t *gui = gGui;
  kbinding_t *kbt = NULL;

  kbt = _kbindings_init_to_default();

  _kbinding_load_config(kbt, gui->keymap_file);

  /* Just to check is there redundant entries, and inform user */
  _kbindings_check_redundancy(kbt);

  gui->kbindings_enabled = 1;

  return kbt;
}

/*
 * Free a keybindings object.
 */
void _kbindings_free_bindings(kbinding_t *kbt) {
  int i;

  if (!kbt)
    return;

  kbt->last = NULL;
  xine_sarray_clear (kbt->action_index);
  xine_sarray_clear (kbt->key_index);

  for (i = kbt->num_entries - 1; i >= (int)KBT_NUM_BASE; i--) {
    kbinding_entry_t *e = kbt->alias[i - KBT_NUM_BASE];
    if (e) {
      e->action = NULL;
      refs_unref (&e->comment);
      refs_unref (&e->key);
      kbt->alias[i - KBT_NUM_BASE] = NULL;
      free (e);
    }
  }
  for (; i >= 0; i--) {
    kbinding_entry_t *e = kbt->base + i;
    if (e) {
      e->action = NULL;
      refs_unref (&e->comment);
      refs_unref (&e->key);
    }
  }
  xine_sarray_delete (kbt->key_index);
  xine_sarray_delete (kbt->action_index);
  SAFE_FREE(kbt);
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
 * Return a key binding entry (if available) matching with action string.
 */
kbinding_entry_t *kbindings_lookup_action(kbinding_t *kbt, const char *action) {
  if((action == NULL) || (kbt == NULL))
    return NULL;

  {
    kbinding_entry_t dummy = {
      .action = (char *)action, /* will not be written to */
      .is_alias = 0
    };
    int i = xine_sarray_binary_search (kbt->action_index, &dummy);
    if (i < 0)
      return NULL;
    return (kbinding_entry_t *)xine_sarray_get (kbt->action_index, i);
  }
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
    n->action    = o->action;
    n->action_id = o->action_id;
    n->modifier  = o->modifier;
    n->index     = i;
    n->is_alias  = o->is_alias;
    n->is_gui    = o->is_gui;
    n->comment   = refs_ref (o->comment);
    n->key       = refs_ref (o->key);
    kbindings_index_add (k, n);
  }
  for (; i < kbt->num_entries; i++) {
    kbinding_entry_t *o = kbt->alias[i - KBT_NUM_BASE];
    kbinding_entry_t *n = (kbinding_entry_t *)malloc (sizeof (*n));
    if (!n)
      break;
    n->action    = o->action;
    n->action_id = o->action_id;
    n->modifier  = o->modifier;
    n->index     = i;
    n->is_alias  = o->is_alias;
    n->is_gui    = o->is_gui;
    n->comment   = refs_ref (o->comment);
    n->key       = refs_ref (o->key);
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
    kbinding_entry_t dummy = {
      .key = (char *)key, /* will not be written to */
      .modifier = modifier
    };
    int i = xine_sarray_binary_search (kbt->key_index, &dummy);
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
      refs_unref (&e->key);
      refs_unref (&e->comment);
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
          refs_unref (&e->comment);
          e->comment = a->comment;
          a->comment = NULL;
          free (a);
          kbt->num_entries--;
          for (; ai < kbt->num_entries; ai++) {
            kbt->alias[ai - KBT_NUM_BASE] = kbt->alias[ai - KBT_NUM_BASE + 1];
            kbt->alias[ai - KBT_NUM_BASE]->index = ai;
          }
          kbindings_index_add (kbt, e);
          return -1;
        }
      }
      /* keep base entry, just reset */
      kbindings_index_remove (kbt, e);
      refs_unref (&e->key);
      e->key = refs_strdup ("VOID");
      e->modifier = 0;
      kbindings_index_add (kbt, e);
      return -1;
    }
  } else {
    if (!strcmp (e->key, key) && (e->modifier == modifier)) {
      /* no change */
      return -2;
    }
    {
      kbinding_entry_t dummy = {
        .key = (char *)key, /* will not be written to */
        .modifier = modifier
      };
      int i = xine_sarray_binary_search (kbt->key_index, &dummy);
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
    kbinding_entry_t dummy = {
      .key = (char *)key, /* will not be written to */
      .modifier = modifier
    };
    int i = xine_sarray_binary_search (kbt->key_index, &dummy);
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
    a->comment = refs_ref (e->comment);
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
      refs_unref (&entry->key);
      refs_unref (&entry->comment);
      free (entry);
    }
    kbt->num_entries = KBT_NUM_BASE;
    return -1;
  }
  if (index < (int)KBT_NUM_BASE)
    return kbindings_entry_set (kbt, index, default_binding_table[index].modifier, default_binding_table[index].key);
  return kbindings_entry_set (kbt, index, 0, "VOID");
}
