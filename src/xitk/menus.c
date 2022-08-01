/*
 * Copyright (C) 2000-2022 the xine project
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

#include <stdio.h>

#include "lang.h"

#include "common.h"
#include "menus.h"
#include "control.h"
#include "event_sender.h"
#include "mrl_browser.h"
#include "panel.h"
#include "playlist.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "xine-toolkit/menu.h"

#define PLAYL_LOAD               0
#define PLAYL_SAVE               1
#define PLAYL_EDIT               2
#define PLAYL_NO_LOOP            3
#define PLAYL_LOOP               4
#define PLAYL_REPEAT             5
#define PLAYL_SHUFFLE            6
#define PLAYL_SHUF_PLUS          7
#define PLAYL_CTRL_STOP          8
#define PLAYL_SCAN               9
#define PLAYL_MMK_EDIT          10
#define PLAYL_DEL_1             11
#define PLAYL_DEL_ALL           12
#define PLAYL_UP                13
#define PLAYL_DOWN              14
#define PLAYL_GET_FROM          15
#define PLAYL_PLAY_CUR          16
#define PLAYL_OPEN_MRLB         17
#define PLAYL_SCAN_SEL          18


#define AUDIO_MUTE               0
#define AUDIO_INCRE_VOL          1
#define AUDIO_DECRE_VOL          2
#define AUDIO_PPROCESS           3
#define AUDIO_PPROCESS_ENABLE    4

#define CTRL_RESET               0

#define _MENU_GUI_ACTION_BASE   (0 << 24)
#define _MENU_ASPECT_BASE       (1 << 24)
#define _MENU_AUDIO_CMD_BASE    (2 << 24)
#define _MENU_AUDIO_CHAN_BASE   ((3 << 24) + 2)
#define _MENU_AUDIO_VIZ_BASE    (4 << 24)
#define _MENU_SUBT_CHAN_BASE    ((5 << 24) + 2)
#define _MENU_PLAYL_CMD_BASE    (6 << 24)
#define _MENU_CTRL_CMD_BASE     (7 << 24)
#define _MENU_NAV_BASE          (8 << 24)

typedef struct {
  char *write, *end, buf[1024];
} menu_text_buf_t;

static void _menu_set_shortcuts (gGui_t *gui, menu_text_buf_t *tbuf, xitk_menu_entry_t *m, int n) {
  xitk_menu_entry_t *e;

  for (e = m; n > 0; e++, n--) {
    size_t sz;

    if (tbuf->write + 1 >= tbuf->end) {
      if (gui->verbosity >= 1)
        printf ("gui.menu: text buffer overflow.\n");
      break;
    }
    if (!e->shortcut)
      continue;
    sz = kbindings_get_shortcut (gui->kbindings, e->shortcut, tbuf->write, tbuf->end - tbuf->write, gui->shortcut_style);
    if (!sz) {
      /* no shortcut set (VOID) */
      e->shortcut = NULL;
    } else if (tbuf->write + sz == tbuf->end) {
      if (gui->verbosity >= 2)
        printf ("gui.menu: shortcut for action \"%s\" was truncated.\n", e->shortcut);
      e->shortcut = tbuf->write;
    } else {
      e->shortcut = tbuf->write;
    }
    tbuf->write += sz + 1;
  }
  for (; n > 0; e++, n--)
    e->shortcut = NULL;
}

static void _menu_action (xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;

  (void)w;
  switch (me->user_id >> 24) {

    case (_MENU_GUI_ACTION_BASE >> 24):
      gui_execute_action_id (gui, me->user_id - _MENU_GUI_ACTION_BASE);
      break;

    case (_MENU_ASPECT_BASE >> 24):
      gui_toggle_aspect (gui, me->user_id - _MENU_ASPECT_BASE);
      break;

    case (_MENU_AUDIO_CMD_BASE >> 24):
      switch (me->user_id - _MENU_AUDIO_CMD_BASE) {
        case AUDIO_MUTE:
          gui_execute_action_id (gui, ACTID_MUTE);
          break;
        case AUDIO_INCRE_VOL:
          if ((gui->mixer.caps & MIXER_CAP_VOL) && (gui->mixer.volume_level < 100)) {
            gui->mixer.volume_level += 10;
            if (gui->mixer.volume_level > 100)
              gui->mixer.volume_level = 100;
            xine_set_param (gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
            panel_update_mixer_display (gui->panel);
            osd_draw_bar (gui, _("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
          }
          break;
        case AUDIO_DECRE_VOL:
          if ((gui->mixer.caps & MIXER_CAP_VOL) && (gui->mixer.volume_level > 0)) {
            gui->mixer.volume_level -= 10;
            if (gui->mixer.volume_level < 0)
              gui->mixer.volume_level = 0;
            xine_set_param (gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
            panel_update_mixer_display (gui->panel);
            osd_draw_bar (gui, _("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
          }
          break;
        case AUDIO_PPROCESS:
          gui_execute_action_id (gui, ACTID_APP);
          break;
        case AUDIO_PPROCESS_ENABLE:
          gui_execute_action_id (gui, ACTID_APP_ENABLE);
          break;
        default:
          printf("%s(): unknown control %d\n", __XINE_FUNCTION__, me->user_id - _MENU_CTRL_CMD_BASE);
          break;
      }
      break;

    case (_MENU_AUDIO_CHAN_BASE >> 24):
      gui_direct_change_audio_channel (w, gui, me->user_id - _MENU_AUDIO_CHAN_BASE);
      break;

    case (_MENU_AUDIO_VIZ_BASE >> 24):
      config_update_num (gui->xine, "gui.post_audio_plugin", me->user_id - _MENU_AUDIO_VIZ_BASE);
      break;

    case (_MENU_SUBT_CHAN_BASE >> 24):
      gui_direct_change_spu_channel (w, gui, me->user_id - _MENU_SUBT_CHAN_BASE);
      break;

    case (_MENU_PLAYL_CMD_BASE >> 24):
      switch (me->user_id - _MENU_PLAYL_CMD_BASE) {
        case PLAYL_LOAD:
          playlist_load_playlist (gui);
          break;
        case PLAYL_SAVE:
          playlist_save_playlist (gui);
          break;
        case PLAYL_EDIT:
          gui_execute_action_id (gui, ACTID_PLAYLIST);
          break;
        case PLAYL_NO_LOOP:
          gui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
          osd_display_info (gui, _("Playlist: no loop."));
          break;
        case PLAYL_LOOP:
          gui->playlist.loop = PLAYLIST_LOOP_LOOP;
          osd_display_info (gui, _("Playlist: loop."));
          break;
        case PLAYL_REPEAT:
          gui->playlist.loop = PLAYLIST_LOOP_REPEAT;
          osd_display_info (gui, _("Playlist: entry repeat."));
          break;
        case PLAYL_SHUFFLE:
          gui->playlist.loop = PLAYLIST_LOOP_SHUFFLE;
          osd_display_info (gui, _("Playlist: shuffle."));
          break;
        case PLAYL_SHUF_PLUS:
          gui->playlist.loop = PLAYLIST_LOOP_SHUF_PLUS;
          osd_display_info (gui, _("Playlist: shuffle forever."));
          break;
        case PLAYL_CTRL_STOP:
          gui_execute_action_id (gui, ACTID_PLAYLIST_STOP);
          break;
        case PLAYL_SCAN:
          playlist_scan_for_infos (gui);
          break;
        case PLAYL_MMK_EDIT:
          playlist_mmk_editor (gui);
          break;
        case PLAYL_DEL_1:
          playlist_delete_current (gui);
          break;
        case PLAYL_DEL_ALL:
          playlist_delete_all (gui);
          break;
        case PLAYL_UP:
          playlist_move_current_up (gui);
          break;
        case PLAYL_DOWN:
          playlist_move_current_down (gui);
          break;
        case PLAYL_GET_FROM:
          {
            int num_mrls;
            const char * const *autoplay_mrls = xine_get_autoplay_mrls (gui->xine, me->menu, &num_mrls);
            if (autoplay_mrls) {
              int j;
              /* Flush playlist in newbie mode */
              if (gui->smart_mode) {
                gui_playlist_free (gui);
                playlist_update_playlist (gui);
              }
              for (j = 0; j < num_mrls; j++)
                gui_playlist_append (gui, autoplay_mrls[j], autoplay_mrls[j], NULL, 0, -1, 0, 0);
              gui->playlist.cur = gui->playlist.num ? 0 : -1;
              if (gui->playlist.cur == 0)
                gui_current_set_index (gui, GUI_MMK_CURRENT);
              /* If we're in newbie mode, start playback immediately
               * (even ignoring if we're currently playing something */
              if (gui->smart_mode) {
                if (xine_get_status (gui->stream) == XINE_STATUS_PLAY)
                  gui_stop (NULL, gui);
                gui_play (NULL, gui);
              }
              enable_playback_controls (gui->panel, (gui->playlist.num > 0));
            }
          }
          break;
        case PLAYL_PLAY_CUR:
          playlist_play_current (gui);
          break;
        case PLAYL_OPEN_MRLB:
          open_mrlbrowser_from_playlist (w, gui);
          break;
        case PLAYL_SCAN_SEL:
          playlist_scan_for_infos_selected (gui);
          break;
        default:
          printf ("%s(): unknown control %d\n", __XINE_FUNCTION__, me->user_id - _MENU_PLAYL_CMD_BASE);
      }
      break;

    case (_MENU_CTRL_CMD_BASE >> 24):
      switch (me->user_id - _MENU_CTRL_CMD_BASE) {
        case CTRL_RESET:
          control_reset (gui->vctrl);
          break;
        default: ;
      }
      break;

    case (_MENU_NAV_BASE >> 24):
      event_sender_send (gui, me->user_id - _MENU_NAV_BASE);
      break;

    default: ;
  }
}

void video_window_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  int                  aspect = xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO);
  xitk_menu_widget_t   menu;
  char                 title[256];
  xitk_widget_t       *w;
  float                xmag, ymag;
#ifdef HAVE_XINERAMA
  int                  fullscr_mode = (FULLSCR_MODE | FULLSCR_XI_MODE);
#else
  int                  fullscr_mode = FULLSCR_MODE;
#endif
  menu_text_buf_t      tbuf = {tbuf.buf, tbuf.buf + sizeof (tbuf.buf) - 1, {0}};
  xitk_menu_entry_t    menu_entries[] = {
    { XITK_MENU_ENTRY_TITLE, 0,
      NULL, NULL},
    { panel_is_visible (gui->panel) > 1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_TOGGLE_VISIBLITY,
      _("Show controls"), "ToggleVisibility"},
    { video_window_is_visible (gui->vwin) > 1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_TOGGLE_WINOUT_VISIBLITY,
      _("Show video window"), "ToggleWindowVisibility"},
    { (video_window_get_fullscreen_mode (gui->vwin) & fullscr_mode) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_TOGGLE_FULLSCREEN,
      _("Fullscreen"), "ToggleFullscreen"},
    { video_window_get_border_mode (gui->vwin) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_TOGGLE_WINOUT_BORDER,
      _("Window frame"), "ToggleWindowBorder"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Open"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_FILESELECTOR,
      _("Open/File..."), "FileSelector"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_PLAYL_CMD_BASE + PLAYL_LOAD,
      _("Open/Playlist..."), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_MRLBROWSER,
      _("Open/Location..."), "MrlBrowser"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Playback"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_PLAY,
      _("Playback/Play"), "Play"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_STOP,
      _("Playback/Stop"), "Stop"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_PAUSE,
      _("Playback/Pause"), "Pause"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Playback/SEP"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_MRL_NEXT,
      _("Playback/Next MRL"), "NextMrl"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_MRL_PRIOR,
      _("Playback/Previous MRL"), "PriorMrl"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Playback/SEP"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_SPEED_FAST,
      _("Playback/Increase Speed"), "SpeedFaster"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_SPEED_SLOW,
      _("Playback/Decrease Speed"), "SpeedSlower"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Playlist"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Playlist/Get from"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_PLAYL_CMD_BASE + PLAYL_LOAD,
      _("Playlist/Load..."), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_PLAYL_CMD_BASE + PLAYL_EDIT,
      _("Playlist/Editor..."), "PlaylistEditor"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Playlist/SEP"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Playlist/Loop modes"), "ToggleLoopMode"},
    { (gui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_NO_LOOP,
      _("Playlist/Loop modes/Disabled"), NULL},
    { (gui->playlist.loop == PLAYLIST_LOOP_LOOP) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_LOOP,
      _("Playlist/Loop modes/Loop"), NULL},
    { (gui->playlist.loop == PLAYLIST_LOOP_REPEAT) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_REPEAT,
      _("Playlist/Loop modes/Repeat Selection"), NULL},
    { (gui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_SHUFFLE,
      _("Playlist/Loop modes/Shuffle"), NULL},
    { (gui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_SHUF_PLUS,
      _("Playlist/Loop modes/Non-stop Shuffle"), NULL},
    { (gui->playlist.control & PLAYLIST_CONTROL_STOP) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_PLAYL_CMD_BASE + PLAYL_CTRL_STOP,
      _("Playlist/Continue Playback"), "PlaylistStop"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Menus"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_EVENT_SENDER,
      _("Menus/Navigation..."), "EventSenderShow"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Menus/SEP"), NULL},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Stream"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_STREAM_INFOS,
      _("Stream/Information..."), "StreamInfosShow"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_OSD_SINFOS,
      _("Stream/Information (OSD)"), "OSDStreamInfos"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Video"), NULL},
    { gui->deinterlace_enable ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_TOGGLE_INTERLEAVE,
      _("Video/Deinterlace"), "ToggleInterleave"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Video/SEP"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Video/Aspect ratio"), "ToggleAspectRatio"},
    { (aspect == XINE_VO_ASPECT_AUTO) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_ASPECT_BASE + XINE_VO_ASPECT_AUTO,
      _("Video/Aspect ratio/Automatic"), NULL},
    { (aspect == XINE_VO_ASPECT_SQUARE) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_ASPECT_BASE + XINE_VO_ASPECT_SQUARE,
      _("Video/Aspect ratio/Square"), NULL},
    { (aspect == XINE_VO_ASPECT_4_3) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_ASPECT_BASE + XINE_VO_ASPECT_4_3,
      _("Video/Aspect ratio/4:3"), NULL},
    { (aspect == XINE_VO_ASPECT_ANAMORPHIC) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_ASPECT_BASE + XINE_VO_ASPECT_ANAMORPHIC,
      _("Video/Aspect ratio/Anamorphic"), NULL},
    { (aspect == XINE_VO_ASPECT_DVB) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_ASPECT_BASE + XINE_VO_ASPECT_DVB,
      _("Video/Aspect ratio/DVB"), NULL},
    { (video_window_get_mag (gui->vwin, &xmag, &ymag), (xmag == 2.0f && ymag == 2.0f))
        ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_WINDOW200,
      _("Video/200%"), "Window200"},
    { (video_window_get_mag (gui->vwin, &xmag, &ymag), (xmag == 1.0f && ymag == 1.0f))
        ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_WINDOW100,
      _("Video/100%"), "Window100"},
    { (video_window_get_mag (gui->vwin, &xmag, &ymag), (xmag == 0.5f && ymag == 0.5f))
        ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_WINDOW50,
      _("Video/50%"), "Window50"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Video/SEP"), NULL},
    { (gui->transform.flags & 1) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_FLIP_H,
      _("Video/mirrored"), "FlipH"},
    { (gui->transform.flags & 2) ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_FLIP_V,
      _("Video/upside down"), "FlipV"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Video/SEP"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Video/Postprocess"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_VPP,
      _("Video/Postprocess/Chain Reaction..."), "VPProcessShow"},
    { gui->post_video_enable ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_GUI_ACTION_BASE + ACTID_VPP_ENABLE,
      _("Video/Postprocess/Enable Postprocessing"), "VPProcessEnable"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Audio"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Audio/Volume"), NULL},
    { gui->mixer.mute ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_AUDIO_CMD_BASE + AUDIO_MUTE,
      _("Audio/Volume/Mute"), "Mute"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_AUDIO_CMD_BASE + AUDIO_INCRE_VOL,
      _("Audio/Volume/Increase 10%"), "Volume+"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_AUDIO_CMD_BASE + AUDIO_DECRE_VOL,
      _("Audio/Volume/Decrease 10%"), "Volume-"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Audio/Channel"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Audio/Visualization"), NULL},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      _("Audio/SEP"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Audio/Postprocess"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_AUDIO_CMD_BASE + AUDIO_PPROCESS,
      _("Audio/Postprocess/Chain Reaction..."), "APProcessShow"},
    { gui->post_audio_enable ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK,
      _MENU_AUDIO_CMD_BASE + AUDIO_PPROCESS_ENABLE,
      _("Audio/Postprocess/Enable Postprocessing"), "APProcessEnable"},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Subtitle"), NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Subtitle/Channel"), NULL},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_BRANCH, 0,
      _("Settings"), NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_SETUP,
      _("Settings/Setup..."), "SetupShow"},
#ifdef HAVE_CURL
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_SKINDOWNLOAD,
      _("Settings/Skin Downloader..."), "SkinDownload"},
#endif
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_KBEDIT,
      _("Settings/Keymap Editor..."), "KeyBindingEditor"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_CONTROLSHOW,
      _("Settings/Video..."), "ControlShow"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_ACONTROLSHOW,
      _("Settings/Audio..."), "AControlShow"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_TVANALOG,
      _("Settings/TV Analog..."), "TVAnalogShow"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_HELP_SHOW,
      _("Help..."), "HelpShow"},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_VIEWLOG,
      _("Logs..."), "ViewlogShow"},
    { XITK_MENU_ENTRY_SEPARATOR, 0,
      "SEP", NULL},
    { XITK_MENU_ENTRY_PLAIN, _MENU_GUI_ACTION_BASE + ACTID_QUIT,
      _("Quit"), "Quit"},
    { XITK_MENU_ENTRY_END, 0,
      NULL, NULL}
  };

  if(gui->no_gui)
    return;

  _menu_set_shortcuts (gui, &tbuf, menu_entries, sizeof (menu_entries) / sizeof (menu_entries[0]) - 1);

  gui->nongui_error_msg = NULL;

  snprintf (title, sizeof (title), _("xine %s"), VERSION);
  menu_entries[0].menu = title;

  XITK_WIDGET_INIT(&menu);

  menu.menu_tree         = &menu_entries[0];
  menu.skin_element_name = NULL;
  menu.cb        = _menu_action;
  menu.user_data = gui;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  /* Subtitle loader */
  if(gui->playlist.num) {
    xitk_menu_entry_t   menu_entry;

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.type      = XITK_MENU_ENTRY_PLAIN;
    menu_entry.user_id   = _MENU_GUI_ACTION_BASE + ACTID_SUBSELECT;
    menu_entry.menu      = _("Open/Subtitle...");
    xitk_menu_add_entry(w, &menu_entry);

    /* Save Playlist */
    menu_entry.user_id   = _MENU_PLAYL_CMD_BASE + PLAYL_SAVE;
    menu_entry.menu      = _("Playlist/Save...");
    xitk_menu_add_entry(w, &menu_entry);
  }

  { /* Autoplay plugins */
    xitk_menu_entry_t   menu_entry;
    char                buffer[2048];
    char               *location = _("Playlist/Get from");
    const char *const  *plug_ids = xine_get_autoplay_input_plugin_ids (gui->xine);
    const char         *plug_id;

    memset (&menu_entry, 0, sizeof (menu_entry));
    menu_entry.type = XITK_MENU_ENTRY_PLAIN;
    menu_entry.user_id = _MENU_PLAYL_CMD_BASE + PLAYL_GET_FROM;
    plug_id = *plug_ids++;
    while (plug_id) {
      buffer[0] = 0;
      snprintf (buffer, sizeof (buffer), "%s/%s", location, plug_id);
      menu_entry.menu      = buffer;
      xitk_menu_add_entry(w, &menu_entry);
      plug_id = *plug_ids++;
    }
  }

  { /* Audio Viz */
    xitk_menu_entry_t   menu_entry;
    const char * const *viz_names = post_get_audio_plugins_names (gui);

    memset (&menu_entry, 0, sizeof (menu_entry));
    menu_entry.type = XITK_MENU_ENTRY_PLAIN;
    if (viz_names && *viz_names) {
      int                 i = 0;
      char               *location = _("Audio/Visualization");
      char                buffer[2048];

      while (viz_names[i]) {
        snprintf (buffer, sizeof (buffer), "%s/%s", location, viz_names[i]);
        menu_entry.menu      = buffer;
        menu_entry.type      = i == gui->visual_anim.post_plugin_num ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
        menu_entry.user_id   = _MENU_AUDIO_VIZ_BASE + i;
        xitk_menu_add_entry(w, &menu_entry);
        i++;
      }

    } else {
      menu_entry.menu      = _("Audio/Visualization/None");
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  { /* Audio channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char               *location = _("Audio/Channel");
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

    memset (&menu_entry, 0, sizeof (menu_entry));
    menu_entry.menu      = _("Audio/Channel/Off");
    menu_entry.type      = channel == -2 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE - 2;
    xitk_menu_add_entry(w, &menu_entry);

    menu_entry.menu      = _("Audio/Channel/Auto");
    menu_entry.type      = channel ==  -1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE - 1;
    xitk_menu_add_entry(w, &menu_entry);

    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];

      memset(&langbuf, 0, sizeof(langbuf));

      if(!xine_get_audio_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
            menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
            menu_entry.user_id   = i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }

      snprintf(buffer, sizeof(buffer), "%s/%s", location, (get_language_from_iso639_1(langbuf)));
      menu_entry.menu      = buffer;
      menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
      menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE + i;
      xitk_menu_add_entry(w, &menu_entry);
    }
  }

  { /* SPU channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char               *location = _("Subtitle/Channel");
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Subtitle/Channel/Off");
    menu_entry.type      = channel == -2 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_SUBT_CHAN_BASE - 2;
    xitk_menu_add_entry(w, &menu_entry);

    menu_entry.menu      = _("Subtitle/Channel/Auto");
    menu_entry.type      = channel == -1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_SUBT_CHAN_BASE - 1;
    xitk_menu_add_entry(w, &menu_entry);

    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];

      memset(&langbuf, 0, sizeof(langbuf));

      if(!xine_get_spu_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
            menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
            menu_entry.user_id   = _MENU_SUBT_CHAN_BASE + i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }

      snprintf(buffer, sizeof(buffer), "%s/%s", location, (get_language_from_iso639_1(langbuf)));
      menu_entry.menu      = buffer;
      menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
      menu_entry.user_id   = _MENU_SUBT_CHAN_BASE + i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  { /* Menus access */
    static const char menu_shortcuts[7][16] = {
      "Menu", "TitleMenu", "RootMenu", "SubpictureMenu", "AudioMenu", "AngleMenu", "PartMenu"
    };
    static const char menu_entries[21][16] = {
      /* Default menu */
      N_("Menu 1"), N_("Menu 2"), N_("Menu 3"), N_("Menu 4"),
      N_("Menu 5"), N_("Menu 6"), N_("Menu 7"),

      /* DVD menu */
      N_("Menu toggle"), N_("Title"), N_("Root"), N_("Subpicture"),
      N_("Audio"), N_("Angle"), N_("Part"),

      /* BluRay menu */
      N_("Top Menu"), N_("Popup Menu"), N_("Menu 3"), N_("Menu 4"),
      N_("Menu 5"), N_("Menu 6"), N_("Menu 7"),
    };

    xitk_menu_entry_t   menu_entry;
    int                 i, first_entry = 0;
    const char *const menus_str = _("Menus");

    gui_playlist_lock (gui);
    if (gui->mmk.mrl) {
      if (!strncmp(gui->mmk.mrl, "bd:/", 4)) {
        first_entry = 14;
      } else if (!strncmp(gui->mmk.mrl, "dvd:/", 5)) {
        first_entry = 7;
      } else if (!strncmp(gui->mmk.mrl, "dvdnav:/", 8)) {
        first_entry = 7;
      }
    }
    gui_playlist_unlock (gui);

    memset (&menu_entry, 0, sizeof (xitk_menu_entry_t));
    menu_entry.type = XITK_MENU_ENTRY_PLAIN;
    for (i = 0; i < 7; i++) {
      char buf[1024];
      snprintf (buf, sizeof (buf), "%s/%s", menus_str, gettext (menu_entries [first_entry + i]));
      menu_entry.menu = buf;
      menu_entry.user_id = _MENU_NAV_BASE + XINE_EVENT_INPUT_MENU1 + i;
      menu_entry.shortcut = menu_shortcuts[i];
      _menu_set_shortcuts (gui, &tbuf, &menu_entry, 1);
      xitk_menu_add_entry (w, &menu_entry);
    }
  }

  /* Mediamark */
  if (xine_get_status (gui->stream) != XINE_STATUS_STOP) {
    xitk_menu_entry_t menu_entry;
    char buf[1024], *p = buf, *e = buf + sizeof (buf);
    p += strlcpy (p, _("Playback"), e - p);
    if (p >= e)
      p = e - 1;
    *p++ = '/';
    /* p += */ strlcpy (p, _("Add Mediamark"), e - p);
    memset (&menu_entry, 0, sizeof (menu_entry));
    menu_entry.type      = XITK_MENU_ENTRY_SEPARATOR;
    menu_entry.menu      = _("Playback/SEP");
    xitk_menu_add_entry (w, &menu_entry);
    menu_entry.menu      = buf;
    menu_entry.type      = XITK_MENU_ENTRY_PLAIN;
    menu_entry.shortcut  = "AddMediamark";
    menu_entry.user_id   = _MENU_GUI_ACTION_BASE + ACTID_ADDMEDIAMARK;
    _menu_set_shortcuts (gui, &tbuf, &menu_entry, 1);
    xitk_menu_add_entry (w, &menu_entry);
  }

  xitk_menu_show_menu(w);
}

void audio_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { XITK_MENU_ENTRY_TITLE, 0, _("Audio"), NULL },
    { XITK_MENU_ENTRY_END,   0, NULL,       NULL }
  };

  XITK_WIDGET_INIT(&menu);

  menu.menu_tree         = &menu_entries[0];
  menu.skin_element_name = NULL;
  menu.cb        = _menu_action;
  menu.user_data = gui;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* Audio channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

    menu_entry.menu      = _("Off");
    menu_entry.type      = channel == -2 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE - 2;
    xitk_menu_add_entry(w, &menu_entry);

    menu_entry.menu      = _("Auto");
    menu_entry.type      = channel ==  -1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE - 1;
    xitk_menu_add_entry(w, &menu_entry);

    for(i = 0; i < 32; i++) {
      char  langbuf[XINE_LANG_MAX];

      memset(&langbuf, 0, sizeof(langbuf));

      if(!xine_get_audio_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
            menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
            menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE + i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }

      menu_entry.menu      = get_language_from_iso639_1 (langbuf);
      menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
      menu_entry.user_id   = _MENU_AUDIO_CHAN_BASE + i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void spu_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { XITK_MENU_ENTRY_TITLE, 0, _("Subtitle"), NULL },
    { XITK_MENU_ENTRY_END,   0, NULL,          NULL }
  };

  XITK_WIDGET_INIT(&menu);

  menu.menu_tree         = &menu_entries[0];
  menu.skin_element_name = NULL;
  menu.cb        = _menu_action;
  menu.user_data = gui;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* SPU channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

    menu_entry.menu      = _("Off");
    menu_entry.type      = channel == -2 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_SUBT_CHAN_BASE - 2;
    xitk_menu_add_entry(w, &menu_entry);

    menu_entry.menu      = _("Auto");
    menu_entry.type      = channel == -1 ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
    menu_entry.user_id   = _MENU_SUBT_CHAN_BASE - 1;
    xitk_menu_add_entry(w, &menu_entry);

    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];

      memset(&langbuf, 0, sizeof(langbuf));

      if(!xine_get_spu_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
            menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
            menu_entry.user_id   = _MENU_SUBT_CHAN_BASE + i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }

      menu_entry.menu      = get_language_from_iso639_1 (langbuf);
      menu_entry.type      = channel == i ? XITK_MENU_ENTRY_CHECKED : XITK_MENU_ENTRY_CHECK;
      menu_entry.user_id   = _MENU_SUBT_CHAN_BASE + i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void playlist_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y, int selected) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  menu_text_buf_t      tbuf = {tbuf.buf, tbuf.buf + sizeof (tbuf.buf) - 1, {0}};
  xitk_menu_entry_t    menu_entries_nosel[] = {
    { XITK_MENU_ENTRY_TITLE, 0,                                      NULL,      NULL               },
    { XITK_MENU_ENTRY_PLAIN, _MENU_PLAYL_CMD_BASE + PLAYL_SCAN,      _("Scan"), "ScanPlaylistInfo" },
    { XITK_MENU_ENTRY_PLAIN, _MENU_PLAYL_CMD_BASE + PLAYL_OPEN_MRLB, _("Add"),  NULL               },
    { XITK_MENU_ENTRY_END,   0,                                      NULL,      NULL               }
  };
  xitk_menu_entry_t    menu_entries_sel[] = {
    { XITK_MENU_ENTRY_TITLE,     0,                                     NULL,            NULL              },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_PLAY_CUR, _("Play"),       NULL              },
    { XITK_MENU_ENTRY_SEPARATOR, 0,                                     "SEP",           NULL              },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_SCAN_SEL, _("Scan"),       "ScanPlaylistInfo"},
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_OPEN_MRLB,_("Add"),        NULL              },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_MMK_EDIT, _("Edit"),       "MediamarkEditor" },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_DEL_1,    _("Delete"),     NULL,             },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_DEL_ALL,  _("Delete All"), NULL,             },
    { XITK_MENU_ENTRY_SEPARATOR, 0,                                     "SEP",           NULL              },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_UP,       _("Move Up"),    NULL              },
    { XITK_MENU_ENTRY_PLAIN,     _MENU_PLAYL_CMD_BASE + PLAYL_DOWN,     _("Move Down"),  NULL              },
    { XITK_MENU_ENTRY_END,       0,                                     NULL,            NULL              }
  };
  _menu_set_shortcuts (gui, &tbuf, menu_entries_nosel, sizeof (menu_entries_nosel) / sizeof (menu_entries_nosel[0]) - 1);
  _menu_set_shortcuts (gui, &tbuf, menu_entries_sel, sizeof (menu_entries_sel) / sizeof (menu_entries_sel[0]) - 1);

  XITK_WIDGET_INIT(&menu);

  if(selected) {
    menu_entries_sel[0].menu = _("Playlist");
    menu.menu_tree           = &menu_entries_sel[0];
  }
  else {
    menu_entries_nosel[0].menu = _("Playlist");
    menu.menu_tree             = &menu_entries_nosel[0];
  }

  menu.skin_element_name = NULL;
  menu.cb        = _menu_action;
  menu.user_data = gui;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  if(!selected && gui->playlist.num) {
    xitk_menu_entry_t   menu_entry;

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.type      = XITK_MENU_ENTRY_PLAIN;
    menu_entry.menu      = _("Delete All");
    menu_entry.user_id   = _MENU_PLAYL_CMD_BASE + PLAYL_DEL_ALL;
    xitk_menu_add_entry(w, &menu_entry);
  }

  xitk_menu_show_menu(w);
}

void control_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  xitk_menu_entry_t    menu_entries[] = {
    { XITK_MENU_ENTRY_TITLE, 0, NULL, NULL },
    { XITK_MENU_ENTRY_PLAIN, _MENU_CTRL_CMD_BASE + CTRL_RESET, _("Reset video settings"), NULL },
    { XITK_MENU_ENTRY_END,   0, NULL, NULL }
  };

  XITK_WIDGET_INIT(&menu);

  menu_entries[0].menu   = _("Video Control");
  menu.menu_tree         = &menu_entries[0];
  menu.skin_element_name = NULL;
  menu.cb                = _menu_action;
  menu.user_data         = gui;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  xitk_menu_show_menu(w);
}
