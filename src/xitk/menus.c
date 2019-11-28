/* 
 * Copyright (C) 2000-2019 the xine project
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

#include "common.h"

#define PLAYL_LOAD              10
#define PLAYL_SAVE              11
#define PLAYL_EDIT              12
#define PLAYL_NO_LOOP           13
#define PLAYL_LOOP              14
#define PLAYL_REPEAT            15
#define PLAYL_SHUFFLE           16
#define PLAYL_SHUF_PLUS         17
#define PLAYL_CTRL_STOP         18

#define AUDIO_MUTE              20
#define AUDIO_INCRE_VOL         21
#define AUDIO_DECRE_VOL         22
#define AUDIO_PPROCESS          23
#define AUDIO_PPROCESS_ENABLE   24

#define IS_CHANNEL_CHECKED(C, N) (C == N) ? "<checked>" : "<check>"

static char *menu_get_shortcut (gGui_t *gui, char *action) {
  char *shortcut = kbindings_get_shortcut(gui->kbindings, action);
#ifdef DEBUG
  if(!shortcut) {
    fprintf(stderr, "Action '%s' is invalid\n", action);
    abort();
  }
#endif
  return (shortcut ? strdup(shortcut) : NULL);
}

static void menu_control_reset(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  (void)w;
  (void)me;
  control_reset (data);
}

static void menu_open_mrlbrowser(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;

  (void)me;
  open_mrlbrowser_from_playlist (w, gui);
}

static void menu_scan_infos(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_scan_for_infos();
}
static void menu_scan_infos_selected(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_scan_for_infos_selected();
}
static void menu_playlist_mmk_editor(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_mmk_editor();
}
static void menu_playlist_delete_all(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_delete_all(w, data);
}
static void menu_playlist_delete_current(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_delete_current(w, data);
}
static void menu_playlist_play_current(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_play_current(w, data);
}
static void menu_playlist_move_updown(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  playlist_move_current_updown(w, data);
}

static void menu_menus_selection(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;

  (void)w;
  event_sender_send (me->user_id);
}

static void menu_gui_action (xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  (void)w;
  gui_execute_action_id (gui, me->user_id);
}

static void menu_playlist_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  int ctrl = me->user_id;

  switch(ctrl) {

  case PLAYL_LOAD:
    playlist_load_playlist(NULL, NULL);
    break;
    
  case PLAYL_SAVE:
    playlist_save_playlist(NULL, NULL);
    break;

  case PLAYL_EDIT:
    gui_execute_action_id (gui, ACTID_PLAYLIST);
    break;

  case PLAYL_NO_LOOP:
    gui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
    osd_display_info(_("Playlist: no loop."));
    break;

  case PLAYL_LOOP:
    gui->playlist.loop = PLAYLIST_LOOP_LOOP;
    osd_display_info(_("Playlist: loop."));
    break;

  case PLAYL_REPEAT:
    gui->playlist.loop = PLAYLIST_LOOP_REPEAT;
    osd_display_info(_("Playlist: entry repeat."));
    break;

  case PLAYL_SHUFFLE:
    gui->playlist.loop = PLAYLIST_LOOP_SHUFFLE;
    osd_display_info(_("Playlist: shuffle."));
    break;

  case PLAYL_SHUF_PLUS:
    gui->playlist.loop = PLAYLIST_LOOP_SHUF_PLUS;
    osd_display_info(_("Playlist: shuffle forever."));
    break;

  case PLAYL_CTRL_STOP:
    gui_execute_action_id (gui, ACTID_PLAYLIST_STOP);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}
static void menu_playlist_from(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  int       num_mrls;
  const char * const *autoplay_mrls = xine_get_autoplay_mrls (gui->xine, me->menu, &num_mrls);

  (void)w;
  (void)me;
  if(autoplay_mrls) {
    int j;
    
    /* Flush playlist in newbie mode */
    if(gui->smart_mode) {
      mediamark_free_mediamarks();
      playlist_update_playlist();
    }
    
    for (j = 0; j < num_mrls; j++)
      mediamark_append_entry(autoplay_mrls[j], autoplay_mrls[j], NULL, 0, -1, 0, 0);
    
    gui->playlist.cur = gui->playlist.num ? 0 : -1;
    
    if(gui->playlist.cur == 0)
      gui_set_current_mmk(mediamark_get_current_mmk());
    
    /* 
     * If we're in newbie mode, start playback immediately
     * (even ignoring if we're currently playing something
     */
    if(gui->smart_mode) {
      if(xine_get_status(gui->stream) == XINE_STATUS_PLAY)
        gui_stop (NULL, gui);
      gui_play (NULL, gui);
    }

    enable_playback_controls (gui->panel, (gui->playlist.num > 0));
  }
}

static void menu_audio_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  int ctrl = me->user_id;

  (void)w;
  switch(ctrl) {
  case AUDIO_MUTE:
    gui_execute_action_id (gui, ACTID_MUTE);
    break;

  case AUDIO_INCRE_VOL:
    if((gui->mixer.caps & MIXER_CAP_VOL) && (gui->mixer.volume_level < 100)) {
      
      gui->mixer.volume_level += 10;
      
      if(gui->mixer.volume_level > 100)
	gui->mixer.volume_level = 100;
      
      xine_set_param(gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
      panel_update_mixer_display (gui->panel);
      osd_draw_bar(_("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
    }
    break;

  case AUDIO_DECRE_VOL:
    if((gui->mixer.caps & MIXER_CAP_VOL) && (gui->mixer.volume_level > 0)) {
      
      gui->mixer.volume_level -= 10;
      
      if(gui->mixer.volume_level < 0)
	gui->mixer.volume_level = 0;

      xine_set_param(gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
      panel_update_mixer_display (gui->panel);
      osd_draw_bar(_("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
    }
    break;

  case AUDIO_PPROCESS:
    gui_execute_action_id (gui, ACTID_APP);
    break;

  case AUDIO_PPROCESS_ENABLE:
    gui_execute_action_id (gui, ACTID_APP_ENABLE);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}

static void menu_audio_viz(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int viz = (int)(intptr_t) data;
  
  config_update_num("gui.post_audio_plugin", viz);
}

static void menu_audio_chan(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  int channel = me->user_id;

  (void)w;
  gui_direct_change_audio_channel (NULL, gui, channel);
}

static void menu_spu_chan(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int channel = (int)(intptr_t) data;

  gui_direct_change_spu_channel(NULL, NULL, channel);
}
static void menu_aspect(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gGui_t *gui = data;
  int aspect = me->user_id;

  (void)w;
  gui_toggle_aspect (aspect);
}

void video_window_menu (gGui_t *gui, xitk_widget_list_t *wl) {
  int                  aspect = xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO);
  int                  x, y;
  xitk_menu_widget_t   menu;
  char                 buffer[2048];
  xitk_widget_t       *w;
  float                xmag, ymag;
#ifdef HAVE_XINERAMA
  int                  fullscr_mode = (FULLSCR_MODE | FULLSCR_XI_MODE);
#else
  int                  fullscr_mode = FULLSCR_MODE;
#endif
  xitk_menu_entry_t    menu_entries[] = {
    { NULL ,
      NULL,
      "<title>",      
      NULL, NULL, 0},
    { _("Show controls"),
      menu_get_shortcut (gui, "ToggleVisibility"),
      panel_is_visible (gui->panel) ? "<checked>" : "<check>",  
      menu_gui_action, gui, ACTID_TOGGLE_VISIBLITY},
    { _("Show video window"),
      menu_get_shortcut (gui, "ToggleWindowVisibility"),
      video_window_is_visible (gui->vwin) ? "<checked>" : "<check>",  
      menu_gui_action, gui, ACTID_TOGGLE_WINOUT_VISIBLITY },
    { _("Fullscreen"),
      menu_get_shortcut (gui, "ToggleFullscreen"),
      (video_window_get_fullscreen_mode (gui->vwin) & fullscr_mode) ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_TOGGLE_FULLSCREEN },
    { "SEP",  
      NULL,
      "<separator>",  
      NULL, NULL, 0},
    { _("Open"),
      NULL,
      "<branch>",   
      NULL, NULL, 0},
    { _("Open/File..."),
      menu_get_shortcut (gui, "FileSelector"),
      NULL,
      menu_gui_action, gui, ACTID_FILESELECTOR},
    { _("Open/Playlist..."),
      NULL,
      NULL,
      menu_playlist_ctrl, gui, PLAYL_LOAD },
    { _("Open/Location..."),
      menu_get_shortcut (gui, "MrlBrowser"),
      NULL,
      menu_gui_action, gui, ACTID_MRLBROWSER},
    { _("Playback"),
      NULL,
      "<Branch>",
      NULL, NULL, 0},
    { _("Playback/Play"),
      menu_get_shortcut (gui, "Play"),
      NULL,
      menu_gui_action, gui, ACTID_PLAY },
    { _("Playback/Stop"),
      menu_get_shortcut (gui, "Stop"),
      NULL,
      menu_gui_action, gui, ACTID_STOP },
    { _("Playback/Pause"),
      menu_get_shortcut (gui,"Pause"),
      NULL,
      menu_gui_action, gui, ACTID_PAUSE },
    { _("Playback/SEP"),
      NULL,
      "<separator>",  
      NULL,  NULL, 0},
    { _("Playback/Next MRL"),
      menu_get_shortcut (gui, "NextMrl"),
      NULL,
      menu_gui_action, gui, ACTID_MRL_NEXT },
    { _("Playback/Previous MRL"),
      menu_get_shortcut (gui, "PriorMrl"),
      NULL,
      menu_gui_action, gui, ACTID_MRL_PRIOR },
    { _("Playback/SEP"),
      NULL,
      "<separator>",  
      NULL,  NULL, 0},
    { _("Playback/Increase Speed"),
      menu_get_shortcut (gui, "SpeedFaster"),
      NULL,
      menu_gui_action, gui, ACTID_SPEED_FAST },
    { _("Playback/Decrease Speed"),
      menu_get_shortcut (gui, "SpeedSlower"),
      NULL,
      menu_gui_action, gui, ACTID_SPEED_SLOW },
    { _("Playlist"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Playlist/Get from"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Playlist/Load..."),
      NULL,
      NULL,
      menu_playlist_ctrl, gui, PLAYL_LOAD },
    { _("Playlist/Editor..."),
      menu_get_shortcut (gui, "PlaylistEditor"),
      NULL,
      menu_playlist_ctrl, gui, PLAYL_EDIT },
    { _("Playlist/SEP"),  
      NULL,
      "<separator>",  
      NULL, NULL, 0},
    { _("Playlist/Loop modes"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Playlist/Loop modes/Disabled"),
      NULL,
      (gui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, gui, PLAYL_NO_LOOP },
    { _("Playlist/Loop modes/Loop"),
      NULL,
      (gui->playlist.loop == PLAYLIST_LOOP_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, gui, PLAYL_LOOP },
    { _("Playlist/Loop modes/Repeat Selection"),
      NULL,
      (gui->playlist.loop == PLAYLIST_LOOP_REPEAT) ? "<checked>" : "<check>",
      menu_playlist_ctrl, gui, PLAYL_REPEAT },
    { _("Playlist/Loop modes/Shuffle"),
      NULL,
      (gui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) ? "<checked>" : "<check>",
      menu_playlist_ctrl, gui, PLAYL_SHUFFLE },
    { _("Playlist/Loop modes/Non-stop Shuffle"),
      NULL,
      (gui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS) ? "<checked>" : "<check>",
      menu_playlist_ctrl, gui, PLAYL_SHUF_PLUS },
    { _("Playlist/Continue Playback"),
      menu_get_shortcut (gui, "PlaylistStop"),
      (gui->playlist.control & PLAYLIST_CONTROL_STOP) ? "<check>" : "<checked>",
      menu_playlist_ctrl, gui, PLAYL_CTRL_STOP },
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Menus"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Menus/Navigation..."),
      menu_get_shortcut (gui, "EventSenderShow"),
      NULL,
      menu_gui_action, gui, ACTID_EVENT_SENDER},
    { _("Menus/SEP"),  
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Stream"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Stream/Information..."),
      menu_get_shortcut (gui, "StreamInfosShow"),
      NULL,
      menu_gui_action, gui, ACTID_STREAM_INFOS},
    { _("Stream/Information (OSD)"),
      menu_get_shortcut (gui, "OSDStreamInfos"),
      NULL,
      menu_gui_action, gui, ACTID_OSD_SINFOS},
    { _("Video"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Video/Deinterlace"),
      menu_get_shortcut (gui, "ToggleInterleave"),
      (gui->deinterlace_enable) ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_TOGGLE_INTERLEAVE },
    { _("Video/SEP"),
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Video/Aspect ratio"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Video/Aspect ratio/Automatic"),
      NULL,
      (aspect == XINE_VO_ASPECT_AUTO) ? "<checked>" : "<check>",
      menu_aspect, gui, XINE_VO_ASPECT_AUTO},
    { _("Video/Aspect ratio/Square"),
      NULL,
      (aspect == XINE_VO_ASPECT_SQUARE) ? "<checked>" : "<check>",
      menu_aspect, gui, XINE_VO_ASPECT_SQUARE},
    { _("Video/Aspect ratio/4:3"),
      NULL,
      (aspect == XINE_VO_ASPECT_4_3) ? "<checked>" : "<check>",
      menu_aspect, gui, XINE_VO_ASPECT_4_3},
    { _("Video/Aspect ratio/Anamorphic"),
      NULL,
      (aspect == XINE_VO_ASPECT_ANAMORPHIC) ? "<checked>" : "<check>",
      menu_aspect, gui, XINE_VO_ASPECT_ANAMORPHIC},
    { _("Video/Aspect ratio/DVB"),
      NULL,
      (aspect == XINE_VO_ASPECT_DVB) ? "<checked>" : "<check>",
      menu_aspect, gui, XINE_VO_ASPECT_DVB},
    { _("Video/200%"),
      menu_get_shortcut (gui, "Window200"),
      (video_window_get_mag (gui->vwin, &xmag, &ymag),
        (xmag == 2.0f && ymag == 2.0f)) ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_WINDOW200 },
    { _("Video/100%"),
      menu_get_shortcut (gui, "Window100"),
      (video_window_get_mag (gui->vwin, &xmag, &ymag),
        (xmag == 1.0f && ymag == 1.0f)) ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_WINDOW100 },
    { _("Video/50%"),
      menu_get_shortcut (gui, "Window50"),
      (video_window_get_mag (gui->vwin, &xmag, &ymag),
        (xmag == 0.5f && ymag == 0.5f)) ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_WINDOW50 },
    { _("Video/SEP"),
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Video/Postprocess"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Video/Postprocess/Chain Reaction..."),
      menu_get_shortcut (gui, "VPProcessShow"),
      NULL,
      menu_gui_action, gui, ACTID_VPP },
    { _("Video/Postprocess/Enable Postprocessing"),
      menu_get_shortcut (gui, "VPProcessEnable"),
      gui->post_video_enable ? "<checked>" : "<check>",
      menu_gui_action, gui, ACTID_VPP_ENABLE },
    { _("Audio"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Audio/Volume"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Audio/Volume/Mute"),
      menu_get_shortcut (gui, "Mute"),
      gui->mixer.mute ? "<checked>" : "<check>",
      menu_audio_ctrl, gui, AUDIO_MUTE},
    { _("Audio/Volume/Increase 10%"),
      NULL,
      NULL,
      menu_audio_ctrl, gui, AUDIO_INCRE_VOL},
    { _("Audio/Volume/Decrease 10%"),
      NULL,
      NULL,
      menu_audio_ctrl, gui, AUDIO_DECRE_VOL},
    { _("Audio/Channel"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Audio/Visualization"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Audio/SEP"),
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Audio/Postprocess"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Audio/Postprocess/Chain Reaction..."),
      NULL /* menu_get_shortcut (gui, "APProcessShow") */,
      NULL,
      menu_audio_ctrl, gui, AUDIO_PPROCESS},
    { _("Audio/Postprocess/Enable Postprocessing"),
      NULL /* menu_get_shortcut (gui, "APProcessEnable") */,
      gui->post_audio_enable ? "<checked>" : "<check>",
      menu_audio_ctrl, gui, AUDIO_PPROCESS_ENABLE},
    { _("Subtitle"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Subtitle/Channel"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Settings"),
      NULL,
      "<branch>",
      NULL, NULL, 0},
    { _("Settings/Setup..."),
      menu_get_shortcut (gui, "SetupShow"),
      NULL,
      menu_gui_action, gui, ACTID_SETUP},
#ifdef HAVE_CURL
    { _("Settings/Skin Downloader..."),
      menu_get_shortcut (gui, "SkinDownload"),
      NULL,
      menu_gui_action, gui, ACTID_SKINDOWNLOAD},
#endif
    { _("Settings/Keymap Editor..."),
      menu_get_shortcut (gui, "KeyBindingEditor"),
      NULL,
      menu_gui_action, gui, ACTID_KBEDIT},
    { _("Settings/Video..."),
      menu_get_shortcut (gui, "ControlShow"),
      NULL,
      menu_gui_action, gui, ACTID_CONTROLSHOW},
    { _("Settings/TV Analog..."),
      menu_get_shortcut (gui, "TVAnalogShow"),
      NULL,
      menu_gui_action, gui, ACTID_TVANALOG},
    { "SEP",
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Help..."),
      menu_get_shortcut (gui, "HelpShow"),
      NULL,
      menu_gui_action, gui, ACTID_HELP_SHOW},
    { _("Logs..."),
      menu_get_shortcut (gui, "ViewlogShow"),
      NULL,
      menu_gui_action, gui, ACTID_VIEWLOG},
    { "SEP",
      NULL,
      "<separator>",
      NULL, NULL, 0},
    { _("Quit"),
      menu_get_shortcut (gui, "Quit"),
      NULL,
      menu_gui_action, gui, ACTID_QUIT},
    { NULL,
      NULL,
      NULL,
      NULL, NULL, 0}
  };
  xitk_menu_entry_t *cur_entry;

  if(gui->no_gui)
    return;

  gui->nongui_error_msg = NULL;
  
  snprintf(buffer, sizeof(buffer), _("xine %s"), VERSION);
  menu_entries[0].menu = buffer;

  XITK_WIDGET_INIT(&menu, gui->imlib_data);
  
  gui->x_lock_display (gui->video_display);
  (void) xitk_get_mouse_coords(gui->video_display, 
			       gui->video_window, NULL, NULL, &x, &y);
  gui->x_unlock_display (gui->video_display);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  cur_entry = &menu_entries[0];
  while(cur_entry->menu) {
    free(cur_entry->shortcut);
    cur_entry++;
  }

  /* Subtitle loader */
  if(gui->playlist.num) {
    xitk_menu_entry_t   menu_entry;
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Open/Subtitle...");
    menu_entry.cb        = menu_gui_action;
    menu_entry.user_data = gui;
    menu_entry.user_id   = ACTID_SUBSELECT;
    xitk_menu_add_entry(w, &menu_entry);

    /* Save Playlist */
    menu_entry.menu      = _("Playlist/Save...");
    menu_entry.cb        = menu_playlist_ctrl;
    menu_entry.user_data = gui;
    menu_entry.user_id   = PLAYL_SAVE;
    xitk_menu_add_entry(w, &menu_entry);
  }
  
  { /* Autoplay plugins */
    xitk_menu_entry_t   menu_entry;
    char                buffer[2048];
    char               *location = _("Playlist/Get from");
    const char *const  *plug_ids = xine_get_autoplay_input_plugin_ids (gui->xine);
    const char         *plug_id;
    
    plug_id = *plug_ids++;
    while(plug_id) {

      memset(&buffer, 0, sizeof(buffer));
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      snprintf(buffer, sizeof(buffer), "%s/%s", location, plug_id);
      
      menu_entry.menu      = buffer;
      menu_entry.cb        = menu_playlist_from;
      menu_entry.user_data = gui;
      xitk_menu_add_entry(w, &menu_entry);
      
      plug_id = *plug_ids++;
    }
  }

  { /* Audio Viz */
    xitk_menu_entry_t   menu_entry;
    const char        **viz_names = post_get_audio_plugins_names();

    if(viz_names && *viz_names) {
      int                 i = 0;
      char               *location = _("Audio/Visualization");
      char                buffer[2048];

      while(viz_names[i]) {
	memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	snprintf(buffer, sizeof(buffer), "%s/%s", location, viz_names[i]);
	menu_entry.menu      = buffer;
	menu_entry.type      = IS_CHANNEL_CHECKED(i, gui->visual_anim.post_plugin_num);
	menu_entry.cb        = menu_audio_viz;
	menu_entry.user_data = (void *)(intptr_t) i;
	xitk_menu_add_entry(w, &menu_entry);
	i++;
      }

    }
    else {
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
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
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Audio/Channel/Off");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = gui;
    menu_entry.user_id   = -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Audio/Channel/Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = gui;
    menu_entry.user_id   = -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_audio_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
            menu_entry.user_data = gui;
            menu_entry.user_id   = i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      snprintf(buffer, sizeof(buffer), "%s/%s", location, (get_language_from_iso639_1(langbuf)));
      menu_entry.menu      = buffer;
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_audio_chan;
      menu_entry.user_data = gui;
      menu_entry.user_id   = i;
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
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Subtitle/Channel/Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_spu_lang(gui->stream, i, &langbuf[0])) {
	
	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *)(intptr_t) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      snprintf(buffer, sizeof(buffer), "%s/%s", location, (get_language_from_iso639_1(langbuf)));
      menu_entry.menu      = buffer;
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_spu_chan;
      menu_entry.user_data = (void *)(intptr_t) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }
  
  { /* Menus access */
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

    pthread_mutex_lock (&gui->mmk_mutex);
    if (gui->mmk.mrl) {
      if (!strncmp(gui->mmk.mrl, "bd:/", 4)) {
        first_entry = 14;
      } else if (!strncmp(gui->mmk.mrl, "dvd:/", 5)) {
        first_entry = 7;
      } else if (!strncmp(gui->mmk.mrl, "dvdnav:/", 8)) {
        first_entry = 7;
      }
    }
    pthread_mutex_unlock (&gui->mmk_mutex);

    for(i = 0; i < 7; i++) {
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

      menu_entry.menu = xitk_asprintf("%s/%s", menus_str, gettext(menu_entries[first_entry + i]));
      menu_entry.cb        = menu_menus_selection;
      menu_entry.user_data = gui;
      menu_entry.user_id   = XINE_EVENT_INPUT_MENU1 + i;

      if (menu_entry.menu)
        xitk_menu_add_entry(w, &menu_entry);
      free(menu_entry.menu);
    }
  }

  /* Mediamark */
  if(xine_get_status(gui->stream) != XINE_STATUS_STOP) {
    xitk_menu_entry_t   menu_entry;

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Playback/SEP");
    menu_entry.type      = "<separator>";
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

    menu_entry.menu = xitk_asprintf("%s/%s", _("Playback"), _("Add Mediamark"));
    menu_entry.shortcut  = menu_get_shortcut (gui, "AddMediamark");
    menu_entry.cb        = menu_gui_action;
    menu_entry.user_data = gui;
    menu_entry.user_id   = ACTID_ADDMEDIAMARK;

    if (menu_entry.menu)
      xitk_menu_add_entry(w, &menu_entry);

    free(menu_entry.shortcut);
    free(menu_entry.menu);
  }

  xitk_menu_show_menu(w);
}

void audio_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , NULL, "<title>",      NULL, NULL, 0 },
    { NULL,  NULL, NULL,           NULL, NULL, 0 }
  };

  menu_entries[0].menu = _("Audio");
  
  XITK_WIDGET_INIT(&menu, gui->imlib_data);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* Audio channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Off");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = gui;
    menu_entry.user_id   = -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = gui;
    menu_entry.user_id   = -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char  langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_audio_lang(gui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
            menu_entry.user_data = gui;
            menu_entry.user_id   = i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      menu_entry.menu      = (char *) get_language_from_iso639_1(langbuf);
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_audio_chan;
      menu_entry.user_data = gui;
      menu_entry.user_id   = i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void spu_lang_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , NULL, "<title>",      NULL, NULL, 0 },
    { NULL,  NULL, NULL,           NULL, NULL, 0 }
  };

  menu_entries[0].menu = _("Subtitle");
  
  XITK_WIDGET_INIT(&menu, gui->imlib_data);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* SPU channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Off");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    menu_entry.menu      = _("Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_spu_lang(gui->stream, i, &langbuf[0])) {
	
	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *)(intptr_t) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      menu_entry.menu      = (char *) get_language_from_iso639_1(langbuf);
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_spu_chan;
      menu_entry.user_data = (void *)(intptr_t) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void playlist_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y, int selected) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  xitk_menu_entry_t    menu_entries_nosel[] = {
    { NULL ,           NULL,          "<title>",     NULL,                         NULL,                    0 },
    { _("Scan"),       menu_get_shortcut (gui, "ScanPlaylistInfo"),
                                      NULL,          menu_scan_infos,              NULL,                    0 },
    { _("Add"),        NULL,          NULL,          menu_open_mrlbrowser,         gui,                     0 },
    { NULL,            NULL,          NULL,          NULL,                         NULL,                    0 }
  };
  xitk_menu_entry_t    menu_entries_sel[] = {
    { NULL ,           NULL,          "<title>",     NULL,                         NULL,                    0 },
    { _("Play"),       NULL,          NULL,          menu_playlist_play_current,   NULL,                    0 },
    { "SEP",           NULL,          "<separator>", NULL,                         NULL,                    0 },
    { _("Scan"),       NULL,          NULL,          menu_scan_infos_selected,     NULL,                    0 },
    { _("Add"),        NULL,          NULL,          menu_open_mrlbrowser,         gui,                     0 },
    { _("Edit"),       menu_get_shortcut (gui, "MediamarkEditor"),
                                      NULL,          menu_playlist_mmk_editor,     NULL,                    0 },
    { _("Delete"),     NULL,          NULL,          menu_playlist_delete_current, NULL,                    0 },
    { _("Delete All"), NULL,          NULL,          menu_playlist_delete_all,     NULL,                    0 },
    { "SEP",           NULL,          "<separator>", NULL,                         NULL,                    0 },
    { _("Move Up"),    NULL,          NULL,          menu_playlist_move_updown,    (void *) DIRECTION_UP,   0 },
    { _("Move Down"),  NULL,          NULL,          menu_playlist_move_updown,    (void *) DIRECTION_DOWN, 0 },
    { NULL,            NULL,          NULL,          NULL,                         NULL,                    0 }
  };
  xitk_menu_entry_t *cur_entry;

  
  XITK_WIDGET_INIT(&menu, gui->imlib_data);
  
  if(selected) {
    menu_entries_sel[0].menu = _("Playlist");
    menu.menu_tree           = &menu_entries_sel[0];
  }
  else {
    menu_entries_nosel[0].menu = _("Playlist");
    menu.menu_tree             = &menu_entries_nosel[0];
  }
  
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;

  w = xitk_noskin_menu_create(wl, &menu, x, y);

  cur_entry = &menu_entries_sel[0];
  while(cur_entry->menu) {
    free(cur_entry->shortcut);
    cur_entry++;
  }
  
  if(!selected && gui->playlist.num) {
    xitk_menu_entry_t   menu_entry;
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Delete All");
    menu_entry.cb        = menu_playlist_delete_all;
    xitk_menu_add_entry(w, &menu_entry);
  }

  xitk_menu_show_menu(w);
}

void control_menu (gGui_t *gui, xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL,                      NULL, "<title>", NULL,               NULL, 0      },
    { _("Reset video settings"), NULL, NULL,      menu_control_reset, gui->vctrl, 0},
    { NULL,                      NULL, NULL,      NULL,               NULL, 0      }
  };
  
  XITK_WIDGET_INIT(&menu, gui->imlib_data);
  
  menu_entries[0].menu   = _("Video Control");
  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;

  w = xitk_noskin_menu_create(wl, &menu, x, y);
  
  xitk_menu_show_menu(w);
}
