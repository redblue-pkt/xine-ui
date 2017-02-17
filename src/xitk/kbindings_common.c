/* 
 * Copyright (C) 2000-2014 the xine project
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

#include "common.h"
#include "kbindings_common.h"

/* 
 * Default key mapping table.
 */
static const kbinding_entry_t default_binding_table[] = {
  { "start playback",
    "Play",                   ACTID_PLAY                    , "Return",   KEYMOD_NOMOD   , 0 , 0},
  { "playback pause toggle",
    "Pause",                  ACTID_PAUSE                   , "space",    KEYMOD_NOMOD   , 0 , 0},
  { "stop playback",
    "Stop",                   ACTID_STOP                    , "S",        KEYMOD_NOMOD   , 0 , 0},
  { "close stream",
    "Close",                  ACTID_CLOSE                   , "C",        KEYMOD_NOMOD   , 0 , 0},
  { "take a snapshot",
    "Snapshot",               ACTID_SNAPSHOT                , "t",        KEYMOD_NOMOD   , 0 , 0},
  { "eject the current medium",
    "Eject",                  ACTID_EJECT                   , "e",        KEYMOD_NOMOD   , 0 , 0},
  { "select and play next MRL in the playlist",
    "NextMrl",                ACTID_MRL_NEXT                , "Next",     KEYMOD_NOMOD   , 0 , 0},
  { "select and play previous MRL in the playlist",
    "PriorMrl",               ACTID_MRL_PRIOR               , "Prior",    KEYMOD_NOMOD   , 0 , 0},
  { "select and play MRL in the playlist",
    "SelectMrl",              ACTID_MRL_SELECT              , "Select",   KEYMOD_NOMOD   , 0 , 0},
  { "loop mode toggle",
    "ToggleLoopMode",         ACTID_LOOPMODE	            , "l",	  KEYMOD_NOMOD   , 0 , 0},
  { "stop playback after played stream",
    "PlaylistStop",           ACTID_PLAYLIST_STOP           , "l",	  KEYMOD_CONTROL , 0 , 0},
  { "scan playlist to grab stream infos",
    "ScanPlaylistInfo",       ACTID_SCANPLAYLIST            , "s",        KEYMOD_CONTROL , 0 , 0},
  { "add a mediamark from current playback",
    "AddMediamark",           ACTID_ADDMEDIAMARK            , "a",        KEYMOD_CONTROL , 0 , 0},
  { "edit selected mediamark",
    "MediamarkEditor",        ACTID_MMKEDITOR               , "e",        KEYMOD_CONTROL , 0 , 1},
  { "set position to -60 seconds in current stream",
    "SeekRelative-60",        ACTID_SEEK_REL_m60            , "Left",     KEYMOD_NOMOD   , 0 , 0}, 
  { "set position to +60 seconds in current stream",
    "SeekRelative+60",        ACTID_SEEK_REL_p60            , "Right",    KEYMOD_NOMOD   , 0 , 0},
  { "set position to -30 seconds in current stream",
    "SeekRelative-30",        ACTID_SEEK_REL_m30            , "Left",     KEYMOD_META    , 0 , 0}, 
  { "set position to +30 seconds in current stream",
    "SeekRelative+30",        ACTID_SEEK_REL_p30            , "Right",    KEYMOD_META    , 0 , 0},
  { "set position to -15 seconds in current stream",
    "SeekRelative-15",        ACTID_SEEK_REL_m15            , "Left",     KEYMOD_CONTROL , 0 , 0},
  { "set position to +15 seconds in current stream",
    "SeekRelative+15",        ACTID_SEEK_REL_p15            , "Right",    KEYMOD_CONTROL , 0 , 0},
  { "set position to -7 seconds in current stream",
    "SeekRelative-7",         ACTID_SEEK_REL_m7             , "Left",     KEYMOD_MOD3    , 0 , 0}, 
  { "set position to +7 seconds in current stream",
    "SeekRelative+7",         ACTID_SEEK_REL_p7             , "Right",    KEYMOD_MOD3    , 0 , 0},
  { "set position to beginning of current stream",
    "SetPosition0%",          ACTID_SET_CURPOS_0            , "0",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 10% of current stream",
    "SetPosition10%",         ACTID_SET_CURPOS_10           , "1",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 20% of current stream",
    "SetPosition20%",         ACTID_SET_CURPOS_20           , "2",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 30% of current stream",
    "SetPosition30%",         ACTID_SET_CURPOS_30           , "3",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 40% of current stream",
    "SetPosition40%",         ACTID_SET_CURPOS_40           , "4",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 50% of current stream",
    "SetPosition50%",         ACTID_SET_CURPOS_50           , "5",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 60% of current stream",
    "SetPosition60%",         ACTID_SET_CURPOS_60           , "6",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 70% of current stream",
    "SetPosition70%",         ACTID_SET_CURPOS_70           , "7",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 80% of current stream",
    "SetPosition80%",         ACTID_SET_CURPOS_80           , "8",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 90% of current stream",
    "SetPosition90%",         ACTID_SET_CURPOS_90           , "9",        KEYMOD_CONTROL , 0 , 0},
  { "set position to 100% of current stream",
    "SetPosition100%",        ACTID_SET_CURPOS_100          , "End",      KEYMOD_CONTROL , 0 , 0},
  { "increment playback speed",
    "SpeedFaster",            ACTID_SPEED_FAST              , "Up",       KEYMOD_NOMOD   , 0 , 0},
  { "decrement playback speed",
    "SpeedSlower",            ACTID_SPEED_SLOW              , "Down",     KEYMOD_NOMOD   , 0 , 0},
  { "reset playback speed",
    "SpeedReset",             ACTID_SPEED_RESET             , "Down",     KEYMOD_META    , 0 , 0},
  { "increment audio volume",
    "Volume+",                ACTID_pVOLUME                 , "V",        KEYMOD_NOMOD   , 0 , 0},
  { "decrement audio volume",
    "Volume-",                ACTID_mVOLUME                 , "v",        KEYMOD_NOMOD   , 0 , 0},
  { "increment amplification level",
    "Amp+",                   ACTID_pAMP                    , "V",        KEYMOD_CONTROL , 0 , 0},
  { "decrement amplification level",
    "Amp-",                   ACTID_mAMP                    , "v",        KEYMOD_CONTROL , 0 , 0},
  { "reset amplification to default value",
    "ResetAmp",               ACTID_AMP_RESET               , "A",        KEYMOD_CONTROL , 0 , 0},
  { "audio muting toggle",
    "Mute",                   ACTID_MUTE                    , "m",        KEYMOD_CONTROL , 0 , 0},
  { "select next audio channel",
    "AudioChannelNext",       ACTID_AUDIOCHAN_NEXT          , "plus",     KEYMOD_NOMOD   , 0 , 0},
  { "select previous audio channel",
    "AudioChannelPrior",      ACTID_AUDIOCHAN_PRIOR         , "minus",    KEYMOD_NOMOD   , 0 , 0},
  { "visibility toggle of audio post effect window",
    "APProcessShow",          ACTID_APP                     , "VOID",     KEYMOD_NOMOD   , 0 , 1},
  { "toggle post effect usage",
    "APProcessEnable",        ACTID_APP_ENABLE              , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "select next sub picture (subtitle) channel",
    "SpuNext",                ACTID_SPU_NEXT                , "period",   KEYMOD_NOMOD   , 0 , 0},
  { "select previous sub picture (subtitle) channel",
    "SpuPrior",               ACTID_SPU_PRIOR               , "comma",    KEYMOD_NOMOD   , 0 , 0},
  { "interlaced mode toggle",
    "ToggleInterleave",       ACTID_TOGGLE_INTERLEAVE       , "i",        KEYMOD_NOMOD   , 0 , 0},
  { "cycle aspect ratio values",
    "ToggleAspectRatio",      ACTID_TOGGLE_ASPECT_RATIO     , "a",        KEYMOD_NOMOD   , 0 , 0},
  { "reduce the output window size by factor 1.2",
    "WindowReduce",           ACTID_WINDOWREDUCE            , "less",     KEYMOD_NOMOD   , 0 , 0},
  { "enlarge the output window size by factor 1.2",
    "WindowEnlarge",          ACTID_WINDOWENLARGE           , "greater",  KEYMOD_NOMOD   , 0 , 0},
  { "set video output window to 50%",
    "Window50",               ACTID_WINDOW50                , "1",        KEYMOD_META    , 0 , 0},
  { "set video output window to 100%",
    "Window100",              ACTID_WINDOW100               , "2",        KEYMOD_META    , 0 , 0},
  { "set video output window to 200%",
    "Window200",              ACTID_WINDOW200               , "3",        KEYMOD_META    , 0 , 0},
  { "zoom in",
    "ZoomIn",                 ACTID_ZOOM_IN                 , "z",        KEYMOD_NOMOD   , 0 , 0},
  { "zoom out",
    "ZoomOut",                ACTID_ZOOM_OUT                , "Z",        KEYMOD_NOMOD   , 0 , 0},
  { "zoom in horizontally",
    "ZoomInX",                ACTID_ZOOM_X_IN               , "z",        KEYMOD_CONTROL , 0 , 0},
  { "zoom out horizontally",
    "ZoomOutX",               ACTID_ZOOM_X_OUT              , "Z",        KEYMOD_CONTROL , 0 , 0},
  { "zoom in vertically",
    "ZoomInY",                ACTID_ZOOM_Y_IN               , "z",        KEYMOD_META    , 0 , 0},
  { "zoom out vertically",
    "ZoomOutY",               ACTID_ZOOM_Y_OUT              , "Z",        KEYMOD_META    , 0 , 0},
  { "reset zooming",
    "ZoomReset",              ACTID_ZOOM_RESET              , "z",        KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { "resize output window to stream size",
    "Zoom1:1",                ACTID_ZOOM_1_1                , "s",        KEYMOD_NOMOD   , 0 , 0},
  { "fullscreen toggle",
    "ToggleFullscreen",       ACTID_TOGGLE_FULLSCREEN       , "f",        KEYMOD_NOMOD   , 0 , 0},
#ifdef HAVE_XINERAMA
  { "Xinerama fullscreen toggle",
    "ToggleXineramaFullscr",  ACTID_TOGGLE_XINERAMA_FULLSCREEN 
                                                            , "F",        KEYMOD_NOMOD   , 0 , 0},
#endif
  { "jump to media Menu",
    "Menu",                   ACTID_EVENT_MENU1             , "Escape",   KEYMOD_NOMOD   , 0 , 0},
  { "jump to Title Menu",
    "TitleMenu",              ACTID_EVENT_MENU2             , "F1",       KEYMOD_NOMOD   , 0 , 0},
  { "jump to Root Menu",
    "RootMenu",               ACTID_EVENT_MENU3             , "F2",       KEYMOD_NOMOD   , 0 , 0},
  { "jump to Subpicture Menu",
    "SubpictureMenu",         ACTID_EVENT_MENU4             , "F3",       KEYMOD_NOMOD   , 0 , 0},
  { "jump to Audio Menu",
    "AudioMenu",              ACTID_EVENT_MENU5             , "F4",       KEYMOD_NOMOD   , 0 , 0},
  { "jump to Angle Menu",
    "AngleMenu",              ACTID_EVENT_MENU6             , "F5",       KEYMOD_NOMOD   , 0 , 0},
  { "jump to Part Menu",
    "PartMenu",               ACTID_EVENT_MENU7             , "F6",       KEYMOD_NOMOD   , 0 , 0},
  { "menu navigate up",
    "EventUp",                ACTID_EVENT_UP                , "KP_Up",    KEYMOD_NOMOD   , 0 , 0},
  { "menu navigate down",
    "EventDown",              ACTID_EVENT_DOWN              , "KP_Down",  KEYMOD_NOMOD   , 0 , 0},
  { "menu navigate left",
    "EventLeft",              ACTID_EVENT_LEFT              , "KP_Left",  KEYMOD_NOMOD   , 0 , 0},
  { "menu navigate right",
    "EventRight",             ACTID_EVENT_RIGHT             , "KP_Right", KEYMOD_NOMOD   , 0 , 0},
  { "menu select",
    "EventSelect",            ACTID_EVENT_SELECT            , "KP_Enter", KEYMOD_NOMOD   , 0 , 0},
  { "jump to next chapter",
    "EventNext",              ACTID_EVENT_NEXT              , "KP_Next",  KEYMOD_NOMOD   , 0 , 0},
  { "jump to previous chapter",
    "EventPrior",             ACTID_EVENT_PRIOR             , "KP_Prior", KEYMOD_NOMOD   , 0 , 0},
  { "select next angle",
    "EventAngleNext",         ACTID_EVENT_ANGLE_NEXT        , "KP_Home",  KEYMOD_NOMOD   , 0 , 0},
  { "select previous angle",
    "EventAnglePrior",        ACTID_EVENT_ANGLE_PRIOR       , "KP_End",   KEYMOD_NOMOD   , 0 , 0},
  { "visibility toggle of help window",
    "HelpShow",               ACTID_HELP_SHOW               , "h",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of video post effect window",
    "VPProcessShow",          ACTID_VPP                     , "P",        KEYMOD_META    , 0 , 1},
  { "toggle post effect usage",
    "VPProcessEnable",        ACTID_VPP_ENABLE              , "P",        KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { "visibility toggle of output window",
    "ToggleWindowVisibility", ACTID_TOGGLE_WINOUT_VISIBLITY , "h",        KEYMOD_NOMOD   , 0 , 1},
  { "bordered window toggle of output window",
    "ToggleWindowBorder",     ACTID_TOGGLE_WINOUT_BORDER    , "b",        KEYMOD_NOMOD   , 0 , 1},
  { "visibility toggle of UI windows",
    "ToggleVisibility",       ACTID_TOGGLE_VISIBLITY        , "g",        KEYMOD_NOMOD   , 0 , 1},
  { "visibility toggle of control window",
    "ControlShow",            ACTID_CONTROLSHOW             , "c",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of mrl browser window",
    "MrlBrowser",             ACTID_MRLBROWSER              , "m",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of playlist editor window",
    "PlaylistEditor",         ACTID_PLAYLIST                , "p",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of the setup window",
    "SetupShow",              ACTID_SETUP                   , "s",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of the event sender window",
    "EventSenderShow",        ACTID_EVENT_SENDER            , "e",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of analog TV window",
    "TVAnalogShow",           ACTID_TVANALOG                , "t",        KEYMOD_META    , 0 , 1},
  { "visibility toggle of log viewer",
    "ViewlogShow",            ACTID_VIEWLOG	            , "l",	  KEYMOD_META    , 0 , 1},
  { "visibility toggle of stream info window",
    "StreamInfosShow",        ACTID_STREAM_INFOS            , "i",        KEYMOD_META    , 0 , 1},
  { "display stream information using OSD",
    "OSDStreamInfos",         ACTID_OSD_SINFOS              , "i",        KEYMOD_CONTROL , 0 , 0},
  { "display information using OSD",
    "OSDWriteText",           ACTID_OSD_WTEXT               , "VOID",     KEYMOD_CONTROL , 0 , 0},
  { "show OSD menu",
    "OSDMenu",                ACTID_OSD_MENU                , "O",        KEYMOD_NOMOD   , 0 , 0},
  { "enter key binding editor",
    "KeyBindingEditor",       ACTID_KBEDIT	            , "k",	  KEYMOD_META    , 0 , 1},
  { "enable key bindings (not useful to bind a key to it!)",
    "KeyBindingsEnable",      ACTID_KBENABLE	            , "VOID",	  KEYMOD_NOMOD   , 0 , 0},
  { "open file selector",
    "FileSelector",           ACTID_FILESELECTOR            , "o",        KEYMOD_CONTROL , 0 , 1},
  { "select a subtitle file",
    "SubSelector",            ACTID_SUBSELECT               , "S",        KEYMOD_CONTROL , 0 , 1},
#ifdef HAVE_CURL
  { "download a skin from the skin server",
    "SkinDownload",           ACTID_SKINDOWNLOAD            , "d",        KEYMOD_CONTROL , 0 , 1},
#endif
  { "display MRL/Ident toggle",
    "MrlIdentToggle",         ACTID_MRLIDENTTOGGLE          , "t",        KEYMOD_CONTROL , 0 , 0},
  { "grab pointer toggle",
    "GrabPointer",            ACTID_GRAB_POINTER            , "Insert",   KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 0",
    "Number0",                ACTID_EVENT_NUMBER_0          , "0",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 1",
    "Number1",                ACTID_EVENT_NUMBER_1          , "1",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 2",
    "Number2",                ACTID_EVENT_NUMBER_2          , "2",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 3",
    "Number3",                ACTID_EVENT_NUMBER_3          , "3",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 4",
    "Number4",                ACTID_EVENT_NUMBER_4          , "4",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 5",
    "Number5",                ACTID_EVENT_NUMBER_5          , "5",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 6",
    "Number6",                ACTID_EVENT_NUMBER_6          , "6",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 7",
    "Number7",                ACTID_EVENT_NUMBER_7          , "7",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 8",
    "Number8",                ACTID_EVENT_NUMBER_8          , "8",        KEYMOD_NOMOD   , 0 , 0},
  { "enter the number 9",
    "Number9",                ACTID_EVENT_NUMBER_9          , "9",        KEYMOD_NOMOD   , 0 , 0},
  { "add 10 to the next entered number",
    "Number10add",            ACTID_EVENT_NUMBER_10_ADD     , "plus",     KEYMOD_MOD3    , 0 , 0},
  { "set position in current stream to numeric percentage",
    "SetPosition%",           ACTID_SET_CURPOS              , "slash",    KEYMOD_NOMOD   , 0 , 0},
  { "set position forward by numeric argument in current stream",
    "SeekRelative+",          ACTID_SEEK_REL_p              , "Up",       KEYMOD_META    , 0 , 0}, 
  { "set position back by numeric argument in current stream",
    "SeekRelative-",          ACTID_SEEK_REL_m              , "Up",       KEYMOD_MOD3    , 0 , 0}, 
  { "change audio video syncing (delay video)",
    "AudioVideoDecay+",       ACTID_AV_SYNC_p3600           , "m",        KEYMOD_NOMOD   , 0 , 0},
  { "change audio video syncing (delay audio)",
    "AudioVideoDecay-",       ACTID_AV_SYNC_m3600           , "n",        KEYMOD_NOMOD   , 0 , 0},
  { "reset audio video syncing offset",
    "AudioVideoDecayReset",   ACTID_AV_SYNC_RESET           , "Home",     KEYMOD_NOMOD   , 0 , 0},
  { "change subtitle syncing (delay video)",
    "SpuVideoDecay+",         ACTID_SV_SYNC_p               , "M",        KEYMOD_NOMOD   , 0 , 0},
  { "change subtitle syncing (delay subtitles)",
    "SpuVideoDecay-",         ACTID_SV_SYNC_m               , "N",        KEYMOD_NOMOD   , 0 , 0},
  { "reset subtitle syncing offset",
    "SpuVideoDecayReset",     ACTID_SV_SYNC_RESET           , "End",      KEYMOD_NOMOD   , 0 , 0},
  { "toggle TV modes (on the DXR3)",
    "ToggleTVmode",           ACTID_TOGGLE_TVMODE	    , "o",	  KEYMOD_CONTROL | KEYMOD_META , 0 , 0},
  { "switch Monitor to DPMS standby mode",
    "DPMSStandby",            ACTID_DPMSSTANDBY             , "d",        KEYMOD_NOMOD   , 0 , 0},
  { "increase hue by 10",
    "HueControl+",            ACTID_HUECONTROLp             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease hue by 10",
    "HueControl-",            ACTID_HUECONTROLm             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase saturation by 10",
    "SaturationControl+",     ACTID_SATURATIONCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease saturation by 10",
    "SaturationControl-",     ACTID_SATURATIONCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase brightness by 10",
    "BrightnessControl+",     ACTID_BRIGHTNESSCONTROLp      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease brightness by 10",
    "BrightnessControl-",     ACTID_BRIGHTNESSCONTROLm      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase contrast by 10",
    "ContrastControl+",       ACTID_CONTRASTCONTROLp        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease contrast by 10",
    "ContrastControl-",       ACTID_CONTRASTCONTROLm        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase gamma by 10",
    "GammaControl+",          ACTID_GAMMACONTROLp           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease gamma by 10",
    "GammaControl-",          ACTID_GAMMACONTROLm           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase sharpness by 10",
    "SharpnessControl+",      ACTID_SHARPNESSCONTROLp       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease sharpness by 10",
    "SharpnessControl-",      ACTID_SHARPNESSCONTROLm       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "increase noise reduction by 10",
    "NoiseReductionControl+", ACTID_NOISEREDUCTIONCONTROLp  , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "decrease noise reduction by 10",
    "NoiseReductionControl-", ACTID_NOISEREDUCTIONCONTROLm  , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "quit the program",
    "Quit",                   ACTID_QUIT                    , "q",        KEYMOD_NOMOD   , 0 , 0},
  { "input_pvr: set input",
    "PVRSetInput",            ACTID_PVR_SETINPUT            , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "input_pvr: set frequency",
    "PVRSetFrequency",        ACTID_PVR_SETFREQUENCY        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "input_pvr: mark the start of a new stream section",
    "PVRSetMark",             ACTID_PVR_SETMARK             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "input_pvr: set the name for the current stream section",
    "PVRSetName",             ACTID_PVR_SETNAME             , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "input_pvr: save the stream section",
    "PVRSave",                ACTID_PVR_SAVE                , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "Open playlist",
    "PlaylistOpen",           ACTID_PLAYLIST_OPEN           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef ENABLE_VDR_KEYS
  { "VDR Red button",
    "VDRButtonRed",           ACTID_EVENT_VDR_RED            , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Green button",
    "VDRButtonGreen",         ACTID_EVENT_VDR_GREEN          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Yellow button",
    "VDRButtonYellow",        ACTID_EVENT_VDR_YELLOW         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Blue button",
    "VDRButtonBlue",          ACTID_EVENT_VDR_BLUE           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Play",
    "VDRPlay",                ACTID_EVENT_VDR_PLAY           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Pause",
    "VDRPause",               ACTID_EVENT_VDR_PAUSE          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Stop",
    "VDRStop",                ACTID_EVENT_VDR_STOP           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Record",
    "VDRRecord",              ACTID_EVENT_VDR_RECORD         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Fast forward",
    "VDRFastFwd",             ACTID_EVENT_VDR_FASTFWD        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Fast rewind",
    "VDRFastRew",             ACTID_EVENT_VDR_FASTREW        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Power",
    "VDRPower",               ACTID_EVENT_VDR_POWER          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Channel +",
    "VDRChannelPlus",         ACTID_EVENT_VDR_CHANNELPLUS    , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Channel -",
    "VDRChannelMinus",        ACTID_EVENT_VDR_CHANNELMINUS   , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Schedule menu",
    "VDRSchedule",            ACTID_EVENT_VDR_SCHEDULE       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Channel menu",
    "VDRChannels",            ACTID_EVENT_VDR_CHANNELS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Timers menu",
    "VDRTimers",              ACTID_EVENT_VDR_TIMERS         , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Recordings menu",
    "VDRRecordings",          ACTID_EVENT_VDR_RECORDINGS     , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Setup menu",
    "VDRSetup",               ACTID_EVENT_VDR_SETUP          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Command menu",
    "VDRCommands",            ACTID_EVENT_VDR_COMMANDS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Command back",
    "VDRBack",                ACTID_EVENT_VDR_BACK           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef XINE_EVENT_VDR_USER0 /* #ifdef is precaution for backward compatibility at the moment */
  { "VDR User command 0",
    "VDRUser0",               ACTID_EVENT_VDR_USER0          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#endif
  { "VDR User command 1",
    "VDRUser1",               ACTID_EVENT_VDR_USER1          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 2",
    "VDRUser2",               ACTID_EVENT_VDR_USER2          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 3",
    "VDRUser3",               ACTID_EVENT_VDR_USER3          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 4",
    "VDRUser4",               ACTID_EVENT_VDR_USER4          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 5",
    "VDRUser5",               ACTID_EVENT_VDR_USER5          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 6",
    "VDRUser6",               ACTID_EVENT_VDR_USER6          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 7",
    "VDRUser7",               ACTID_EVENT_VDR_USER7          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 8",
    "VDRUser8",               ACTID_EVENT_VDR_USER8          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR User command 9",
    "VDRUser9",               ACTID_EVENT_VDR_USER9          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Volume +",
    "VDRVolumePlus",          ACTID_EVENT_VDR_VOLPLUS        , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Volume -",
    "VDRVolumeMinus",         ACTID_EVENT_VDR_VOLMINUS       , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Mute audio",
    "VDRMute",                ACTID_EVENT_VDR_MUTE           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Audio menu",
    "VDRAudio",               ACTID_EVENT_VDR_AUDIO          , "VOID",     KEYMOD_NOMOD   , 0 , 0},
  { "VDR Command info",
    "VDRInfo",                ACTID_EVENT_VDR_INFO           , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#ifdef XINE_EVENT_VDR_CHANNELPREVIOUS /* #ifdef is precaution for backward compatibility at the moment */
  { "VDR Previous channel",
    "VDRChannelPrevious",     ACTID_EVENT_VDR_CHANNELPREVIOUS, "VOID",     KEYMOD_NOMOD   , 0 , 0},
#endif
#ifdef XINE_EVENT_VDR_SUBTITLES /* #ifdef is precaution for backward compatibility at the moment */
  { "VDR Subtiles menu",
    "VDRSubtitles",           ACTID_EVENT_VDR_SUBTITLES      , "VOID",     KEYMOD_NOMOD   , 0 , 0},
#endif
#endif
  { 0,
    0,                        0                              , 0,          0              , 0 , 0}
};


static int _kbinding_get_is_gui_from_default(char *action) {
  int i;
  
  for(i = 0; default_binding_table[i].action != NULL; i++) {
    if(!strcmp(default_binding_table[i].action, action))
      return default_binding_table[i].is_gui;
  }
  return 0;
}

/*
 * Create a new key binding table from (static) default one.
 */
void _kbindings_init_to_default_no_kbt(kbinding_t *kbt) {
  int          i;

  /*
   * FIXME: This should be a compile time check.
   */
  /* Check for space to hold the default table plus a number of user defined aliases. */
  if(sizeof(default_binding_table)/sizeof(default_binding_table[0])+100 > MAX_ENTRIES) {
    fprintf(stderr, "%s(%d):\n"
		    "  Too many entries in default_binding_table[].\n"
		    "  Increase MAX_ENTRIES to at least %zd.\n",
	    __XINE_FUNCTION__, __LINE__,
	    sizeof(default_binding_table)/sizeof(default_binding_table[0])+100);
    abort();
  }

  for(i = 0; default_binding_table[i].action != NULL; i++) {
    kbt->entry[i] = (kbinding_entry_t *) malloc(sizeof(kbinding_entry_t));
    kbt->entry[i]->comment = strdup(default_binding_table[i].comment);
    kbt->entry[i]->action = strdup(default_binding_table[i].action);
    kbt->entry[i]->action_id = default_binding_table[i].action_id;
    kbt->entry[i]->key = strdup(default_binding_table[i].key);
    kbt->entry[i]->modifier = default_binding_table[i].modifier;
    kbt->entry[i]->is_alias = default_binding_table[i].is_alias;
    kbt->entry[i]->is_gui = default_binding_table[i].is_gui;
  }
  kbt->entry[i] = (kbinding_entry_t *) malloc(sizeof(kbinding_entry_t));
  kbt->entry[i]->comment = NULL;
  kbt->entry[i]->action = NULL;
  kbt->entry[i]->action_id = 0;
  kbt->entry[i]->key = NULL;
  kbt->entry[i]->modifier = 0;
  kbt->entry[i]->is_alias = 0;
  kbt->entry[i]->is_gui = 0;

  kbt->num_entries = i + 1; /* Count includes the terminating null entry! */
}

kbinding_t *_kbindings_init_to_default(void) {
  kbinding_t  *kbt;
 
  kbt = (kbinding_t *) malloc(sizeof(kbinding_t));
  _kbindings_init_to_default_no_kbt(kbt);

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

/*
 * Add an entry in key binding table kbt. Called when an Alias entry
 * is found.
 */
static void _kbindings_add_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  kbinding_entry_t  *k;
  int                modifier;

  ABORT_IF_NULL(kbt);
  ABORT_IF_NULL(ukb);

  /* This should only happen if the keymap file was edited manually. */
  if(kbt->num_entries >= MAX_ENTRIES) {
    fprintf(stderr, _("xine-ui: Too many Alias entries in keymap file, entry ignored.\n"));
    return;
  }

  k = kbindings_lookup_action(kbt, ukb->alias);
  if(k) {
    
    modifier = k->modifier;
    
    if(ukb->modifier) {
      char *p, *ukb_modifier_ptr;
      
      modifier = KEYMOD_NOMOD;

      ukb_modifier_ptr = ukb->modifier;
      while((p = xine_strsep(&ukb_modifier_ptr, ",")) != NULL) {
	
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
    kbt->entry[kbt->num_entries - 1]->is_gui    = k->is_gui;
    kbt->entry[kbt->num_entries - 1]->is_alias  = 1;
    kbt->entry[kbt->num_entries - 1]->comment   = strdup(k->comment);
    kbt->entry[kbt->num_entries - 1]->action    = strdup(k->action);
    kbt->entry[kbt->num_entries - 1]->action_id = k->action_id;
    kbt->entry[kbt->num_entries - 1]->key       = strdup(ukb->key);
    kbt->entry[kbt->num_entries - 1]->modifier  = modifier;
    
    /*
     * NULL terminate array.
     */
    kbt->entry[kbt->num_entries] = (kbinding_entry_t *) calloc(1, sizeof(kbinding_t));
    kbt->entry[kbt->num_entries]->is_gui    = 0;
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
      
      kbt->entry[i]->is_gui = _kbinding_get_is_gui_from_default(ukb->action);

      if(ukb->modifier) {
	char *p, *ukb_modifier_ptr;
	int   modifier = KEYMOD_NOMOD;

	ukb_modifier_ptr = ukb->modifier;
	while((p = xine_strsep(&ukb_modifier_ptr, ",")) != NULL) {

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
static void _kbinding_reset_cb(xitk_widget_t *w, void *data, int button) {
  kbinding_t *kbt = (kbinding_t *) data;
  kbindings_reset_kbinding(kbt);
}
static void _kbinding_editor_cb(xitk_widget_t *w, void *data, int button) {
  kbedit_window();
}
#endif

static void _kbindings_check_redundancy(kbinding_t *kbt) {
  int            i, j, found = 0;
  char          *kmsg = NULL;
      
  if(kbt == NULL)
    return;
  
  for(i = 0; kbt->entry[i]->action != NULL; i++) {
    for(j = 0; kbt->entry[j]->action != NULL; j++) {
      if(i != j && j != found) {
	if((!strcmp(kbt->entry[i]->key, kbt->entry[j]->key)) &&
	   (kbt->entry[i]->modifier == kbt->entry[j]->modifier) && 
	   (strcasecmp(kbt->entry[i]->key, "void"))) {
	  char *action1, *action2;
	  char *dna = _("and");
	  int  len;
	  
	  found++;
	  
	  action1 = kbt->entry[i]->action;
	  action2 = kbt->entry[j]->action;

	  len = strlen(action1) + 1 + strlen(dna) + 1 + strlen(action2);

	  if(!kmsg) {
	    char *header = _("The following key bindings pairs are identical:\n\n");
	    len += strlen(header);
	    kmsg = (char *) malloc(len + 1);
	    sprintf(kmsg, "%s%s%c%s%c%s", header, action1,' ', dna, ' ', action2);
	  }
	  else {
	    len += 2;
	    kmsg = (char *) realloc(kmsg, strlen(kmsg) + len + 1);
	    sprintf(kmsg+strlen(kmsg), "%s%s%c%s%c%s", ", ", action1, ' ', dna, ' ', action2);
	  }
	}
      }
    }
  }

#ifndef KBINDINGS_MAN
  if(found) {
    xitk_window_t *xw;
    char          *footer = _(".\n\nWhat do you want to do ?\n");
    
    kmsg = (char *) realloc(kmsg, strlen(kmsg) + strlen(footer) + 1);
    strlcat(kmsg, footer, strlen(kmsg) + strlen(footer) + 1);

    dump_error(kmsg);

    xw = xitk_window_dialog_three_buttons_with_width(gGui->imlib_data,
						     _("Keybindings error!"),
						     _("Reset"), _("Editor"), _("Cancel"),
						     _kbinding_reset_cb, _kbinding_editor_cb, NULL,
						     (void *) kbt, 450, ALIGN_CENTER,
						     "%s", kmsg);
    XLockDisplay(gGui->display);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
    XSync(gGui->display, False);
    XUnlockDisplay(gGui->display);
    layer_above_video(xitk_window_get_window(xw));
  }
#endif

  free(kmsg);
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

  gGui->kbindings_enabled = 1;

  return kbt;
}

/*
 * Free a keybindings object.
 */
void _kbindings_free_bindings_no_kbt(kbinding_t *kbt) {
  kbinding_entry_t **k;
  
  if(kbt == NULL)
    return;
  
  if((k = kbt->entry)) {
    int i = kbt->num_entries - 1;
    
    if(i && (i < MAX_ENTRIES)) {
      for(; i >= 0; i--) {
	SAFE_FREE(k[i]->comment);
	SAFE_FREE(k[i]->action);
	SAFE_FREE(k[i]->key);
	free(k[i]);
      }
    }
  }
}

void _kbindings_free_bindings(kbinding_t *kbt) {
  _kbindings_free_bindings_no_kbt(kbt);
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
  kbinding_entry_t  *kret = NULL, *k;
  int                i;

  if((action == NULL) || (kbt == NULL))
    return NULL;

  /* CHECKME: Not case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if(!strcasecmp(k->action, action)) {
      kret = k;
      break;
    }
  }
  
  /* Unknown action */
  return kret;
}
