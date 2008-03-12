/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "common.h"

extern _panel_t                *panel;

#define PLAYB_PLAY              1
#define PLAYB_STOP              2
#define PLAYB_PAUSE             3
#define PLAYB_NEXT              4
#define PLAYB_PREV              5
#define PLAYB_SPEEDM            6
#define PLAYB_SPEEDL            7
#define PLAYB_ADDMMK            8

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

#define VIDEO_MIN               30
#define VIDEO_FULLSCR           30
#define VIDEO_2X                31
#define VIDEO_1X                32
#define VIDEO__5X               33
#define VIDEO_INTERLEAVE        34
#define VIDEO_PPROCESS          35
#define VIDEO_PPROCESS_ENABLE   36
#define VIDEO_TOGGLE            37

#define SETS_MIN                40
#define SETS_SETUP              40
#define SETS_KEYMAP             41
#define SETS_VIDEO              42
#define SETS_LOGS               43
#define SETS_SKINDL             44
#define SETS_TVANALOG           45

#define STREAM_OSDI             50
#define STREAM_WINI             51

#define IS_CHANNEL_CHECKED(C, N) (C == N) ? "<checked>" : "<check>"

static char *menu_get_shortcut(char *action) {
  char *shortcut = kbindings_get_shortcut(gGui->kbindings, action);
#ifdef DEBUG
  if(!shortcut) {
    fprintf(stderr, "Action '%s' is invalid\n", action);
    abort();
  }
#endif
  return (shortcut ? strdup(shortcut) : NULL);
}

static void menu_control_reset(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  control_reset();
}
static void menu_open_mrlbrowser(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  open_mrlbrowser_from_playlist(w, data);
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
static void menu_event_sender(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_EVENT_SENDER);
}
static void menu_menus_selection(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int event     = (int) data;
  static const int events[7] = {
    XINE_EVENT_INPUT_MENU1, XINE_EVENT_INPUT_MENU2, XINE_EVENT_INPUT_MENU3,
    XINE_EVENT_INPUT_MENU4, XINE_EVENT_INPUT_MENU5, XINE_EVENT_INPUT_MENU6,
    XINE_EVENT_INPUT_MENU7
  };
  
  event_sender_send(events[event]);
}
static void menu_panel_visibility(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);
}
static void menu_file_selector(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_FILESELECTOR);
}
static void menu_mrl_browser(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_MRLBROWSER);
}
static void menu_subtitle_selector(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_SUBSELECT);
}
static void menu_playback_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int ctrl = (int) data;

  static const int actions[] = {
    ACTID_PLAY,
    ACTID_STOP,
    ACTID_PAUSE,
    ACTID_MRL_NEXT,
    ACTID_MRL_PRIOR,
    ACTID_SPEED_FAST,
    ACTID_SPEED_SLOW,
    ACTID_ADDMEDIAMARK
  };

  if ( ctrl >= sizeof(actions)/sizeof(actions[0]) ) {
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    return;
  }

  gui_execute_action_id(actions[ctrl]);
}

static void menu_playlist_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int ctrl = (int) data;

  switch(ctrl) {

  case PLAYL_LOAD:
    playlist_load_playlist(NULL, NULL);
    break;
    
  case PLAYL_SAVE:
    playlist_save_playlist(NULL, NULL);
    break;

  case PLAYL_EDIT:
    gui_execute_action_id(ACTID_PLAYLIST);
    break;

  case PLAYL_NO_LOOP:
    gGui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
    osd_display_info(_("Playlist: no loop."));
    break;

  case PLAYL_LOOP:
    gGui->playlist.loop = PLAYLIST_LOOP_LOOP;
    osd_display_info(_("Playlist: loop."));
    break;

  case PLAYL_REPEAT:
    gGui->playlist.loop = PLAYLIST_LOOP_REPEAT;
    osd_display_info(_("Playlist: entry repeat."));
    break;

  case PLAYL_SHUFFLE:
    gGui->playlist.loop = PLAYLIST_LOOP_SHUFFLE;
    osd_display_info(_("Playlist: shuffle."));
    break;

  case PLAYL_SHUF_PLUS:
    gGui->playlist.loop = PLAYLIST_LOOP_SHUF_PLUS;
    osd_display_info(_("Playlist: shuffle forever."));
    break;

  case PLAYL_CTRL_STOP:
    gui_execute_action_id(ACTID_PLAYLIST_STOP);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}
static void menu_playlist_from(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int       num_mrls;
  char    **autoplay_mrls = xine_get_autoplay_mrls (__xineui_global_xine_instance, me->menu, &num_mrls);

  if(autoplay_mrls) {
    int j;
    
    /* Flush playlist in newbie mode */
    if(gGui->smart_mode) {
      mediamark_free_mediamarks();
      playlist_update_playlist();
    }
    
    for (j = 0; j < num_mrls; j++)
      mediamark_append_entry(autoplay_mrls[j], autoplay_mrls[j], NULL, 0, -1, 0, 0);
    
    gGui->playlist.cur = gGui->playlist.num ? 0 : -1;
    
    if(gGui->playlist.cur == 0)
      gui_set_current_mmk(mediamark_get_current_mmk());
    
    /* 
     * If we're in newbie mode, start playback immediately
     * (even ignoring if we're currently playing something
     */
    if(gGui->smart_mode) {
      if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY)
	gui_stop(NULL, NULL);
      gui_play(NULL, NULL);
    }

    enable_playback_controls((gGui->playlist.num > 0));
  }
}
static void menu_stream(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int info = (int) data;

  switch(info) {
  case STREAM_OSDI:
    gui_execute_action_id(ACTID_OSD_SINFOS);
    break;
    
  case STREAM_WINI:
    gui_execute_action_id(ACTID_STREAM_INFOS);
    break;
  }
}
static void menu_audio_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int ctrl = (int) data;

  switch(ctrl) {
  case AUDIO_MUTE:
    gui_execute_action_id(ACTID_MUTE);
    break;

  case AUDIO_INCRE_VOL:
    if((gGui->mixer.caps & MIXER_CAP_VOL) && (gGui->mixer.volume_level < 100)) {
      
      gGui->mixer.volume_level += 10;
      
      if(gGui->mixer.volume_level > 100)
	gGui->mixer.volume_level = 100;
      
      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->mixer.slider, gGui->mixer.volume_level);
      osd_draw_bar(_("Audio Volume"), 0, 100, gGui->mixer.volume_level, OSD_BAR_STEPPER);
    }
    break;

  case AUDIO_DECRE_VOL:
    if((gGui->mixer.caps & MIXER_CAP_VOL) && (gGui->mixer.volume_level > 0)) {
      
      gGui->mixer.volume_level -= 10;
      
      if(gGui->mixer.volume_level < 0)
	gGui->mixer.volume_level = 0;

      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
      xitk_slider_set_pos(panel->mixer.slider, gGui->mixer.volume_level);
      osd_draw_bar(_("Audio Volume"), 0, 100, gGui->mixer.volume_level, OSD_BAR_STEPPER);
    }
    break;

  case AUDIO_PPROCESS:
    gui_execute_action_id(ACTID_APP);
    break;

  case AUDIO_PPROCESS_ENABLE:
    gui_execute_action_id(ACTID_APP_ENABLE);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}

static void menu_audio_viz(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int viz = (int) data;
  
  config_update_num("gui.post_audio_plugin", viz);
}
static void menu_audio_chan(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int channel = (int) data;

  gui_direct_change_audio_channel(NULL, NULL, channel);
}
static void menu_spu_chan(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int channel = (int) data;

  gui_direct_change_spu_channel(NULL, NULL, channel);
}
static void menu_aspect(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int aspect = (int) data;
  
  gui_toggle_aspect(aspect);
}
static void menu_video_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int ctrl = ((int) data) - VIDEO_MIN;

  static const int actions[] = {
    ACTID_TOGGLE_FULLSCREEN,
    ACTID_WINDOW200,
    ACTID_WINDOW100,
    ACTID_WINDOW50,
    ACTID_TOGGLE_INTERLEAVE,
    ACTID_VPP,
    ACTID_VPP_ENABLE,
    ACTID_TOGGLE_WINOUT_VISIBLITY
  };

  if ( ctrl >= sizeof(actions)/sizeof(actions[0]) ) {
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl+VIDEO_MIN);
    return;
  }

  gui_execute_action_id(actions[ctrl]);
}
static void menu_settings(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int sets = ((int) data) - SETS_MIN;

  static const int actions[] = {
    ACTID_SETUP,
    ACTID_KBEDIT,
    ACTID_CONTROLSHOW,
    ACTID_TVANALOG,
    ACTID_VIEWLOG,
    ACTID_SKINDOWNLOAD
  };

  if ( sets >= sizeof(actions)/sizeof(actions[0]) ) {
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, sets+SETS_MIN);
    return;
  }

  gui_execute_action_id(actions[sets]);
}
static void menu_help(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_HELP_SHOW);
}
static void menu_quit(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_QUIT);
}

void video_window_menu(xitk_widget_list_t *wl) {
  int                  aspect = xine_get_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO);
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
      NULL, NULL                                                                             },
    { _("Show controls"),
      menu_get_shortcut("ToggleVisibility"),
      panel_is_visible() ? "<checked>" : "<check>",  
      menu_panel_visibility, NULL                                                            },
    { _("Show video window"),
      menu_get_shortcut("ToggleWindowVisibility"),
      video_window_is_visible() ? "<checked>" : "<check>",  
      menu_video_ctrl, (void *) VIDEO_TOGGLE                                                 },
    { _("Fullscreen"),
      menu_get_shortcut("ToggleFullscreen"),
      (video_window_get_fullscreen_mode() & fullscr_mode) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_FULLSCR                                                },
    { "SEP",  
      NULL,
      "<separator>",  
      NULL,  NULL                                                                            },
    { _("Open"),
      NULL,
      "<branch>",   
      NULL, NULL                                                                             },
    { _("Open/File..."),
      menu_get_shortcut("FileSelector"),
      NULL,
      menu_file_selector,        NULL                                                        },
    { _("Open/Playlist..."),
      NULL,
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_LOAD                                                },
    { _("Open/Location..."),
      menu_get_shortcut("MrlBrowser"),
      NULL,
      menu_mrl_browser, NULL                                                                 },
    { _("Playback"),
      NULL,
      "<Branch>",
      NULL, NULL                                                                             },
    { _("Playback/Play"),
      menu_get_shortcut("Play"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PLAY                                                },
    { _("Playback/Stop"),
      menu_get_shortcut("Stop"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_STOP                                                },
    { _("Playback/Pause"),
      menu_get_shortcut("Pause"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PAUSE                                               },
    { _("Playback/SEP"),
      NULL,
      "<separator>",  
      NULL,  NULL                                                                            },
    { _("Playback/Next MRL"),
      menu_get_shortcut("NextMrl"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_NEXT                                                },
    { _("Playback/Previous MRL"),
      menu_get_shortcut("PriorMrl"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PREV                                                },
    { _("Playback/SEP"),
      NULL,
      "<separator>",  
      NULL,  NULL                                                                            },
    { _("Playback/Increase Speed"),
      menu_get_shortcut("SpeedFaster"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_SPEEDM                                              },
    { _("Playback/Decrease Speed"),
      menu_get_shortcut("SpeedSlower"),
      NULL,
      menu_playback_ctrl, (void *) PLAYB_SPEEDL                                              },
    { _("Playlist"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Playlist/Get from"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Playlist/Load..."),
      NULL,
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_LOAD                                                },
    { _("Playlist/Editor..."),
      menu_get_shortcut("PlaylistEditor"),
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_EDIT                                                },
    { _("Playlist/SEP"),  
      NULL,
      "<separator>",  
      NULL, NULL                                                                             },
    { _("Playlist/Loop modes"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Playlist/Loop modes/Disabled"),
      NULL,
      (gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_NO_LOOP                                             },
    { _("Playlist/Loop modes/Loop"),
      NULL,
      (gGui->playlist.loop == PLAYLIST_LOOP_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_LOOP                                                },
    { _("Playlist/Loop modes/Repeat Selection"),
      NULL,
      (gGui->playlist.loop == PLAYLIST_LOOP_REPEAT) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_REPEAT                                              },
    { _("Playlist/Loop modes/Shuffle"),
      NULL,
      (gGui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_SHUFFLE                                             },
    { _("Playlist/Loop modes/Non-stop Shuffle"),
      NULL,
      (gGui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_SHUF_PLUS                                           },
    { _("Playlist/Continue Playback"),
      menu_get_shortcut("PlaylistStop"),
      (gGui->playlist.control & PLAYLIST_CONTROL_STOP) ? "<check>" : "<checked>",
      menu_playlist_ctrl, (void *) PLAYL_CTRL_STOP                                           },
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Menus"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Menus/Navigation..."),
      menu_get_shortcut("EventSenderShow"),
      NULL,
      menu_event_sender, NULL                                                                },
    { _("Menus/SEP"),  
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Stream"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Stream/Information..."),
      menu_get_shortcut("StreamInfosShow"),
      NULL,
      menu_stream, (void *) STREAM_WINI                                                      },
    { _("Stream/Information (OSD)"),
      menu_get_shortcut("OSDStreamInfos"),
      NULL,
      menu_stream, (void *) STREAM_OSDI                                                      },
    { _("Video"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Video/Deinterlace"),
      menu_get_shortcut("ToggleInterleave"),
      (gGui->deinterlace_enable) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_INTERLEAVE                                             },
    { _("Video/SEP"),
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Video/Aspect ratio"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Video/Aspect ratio/Automatic"),
      NULL,
      (aspect == XINE_VO_ASPECT_AUTO) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_AUTO                                              },
    { _("Video/Aspect ratio/Square"),
      NULL,
      (aspect == XINE_VO_ASPECT_SQUARE) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_SQUARE                                            },
    { _("Video/Aspect ratio/4:3"),
      NULL,
      (aspect == XINE_VO_ASPECT_4_3) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_4_3                                               },
    { _("Video/Aspect ratio/Anamorphic"),
      NULL,
      (aspect == XINE_VO_ASPECT_ANAMORPHIC) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_ANAMORPHIC                                        },
    { _("Video/Aspect ratio/DVB"),
      NULL,
      (aspect == XINE_VO_ASPECT_DVB) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_DVB                                               },
    { _("Video/200%"),
      menu_get_shortcut("Window200"),
      (video_window_get_mag(&xmag, &ymag),
        (xmag == 2.0f && ymag == 2.0f)) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_2X                                                     },
    { _("Video/100%"),
      menu_get_shortcut("Window100"),
      (video_window_get_mag(&xmag, &ymag),
        (xmag == 1.0f && ymag == 1.0f)) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_1X                                                     },
    { _("Video/50%"),
      menu_get_shortcut("Window50"),
      (video_window_get_mag(&xmag, &ymag),
        (xmag == 0.5f && ymag == 0.5f)) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO__5X                                                    },
    { _("Video/SEP"),
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Video/Postprocess"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Video/Postprocess/Chain Reaction..."),
      menu_get_shortcut("VPProcessShow"),
      NULL,
      menu_video_ctrl, (void *) VIDEO_PPROCESS                                               },
    { _("Video/Postprocess/Enable Postprocessing"),
      menu_get_shortcut("VPProcessEnable"),
      gGui->post_video_enable ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_PPROCESS_ENABLE                                        },
    { _("Audio"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Audio/Volume"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Audio/Volume/Mute"),
      menu_get_shortcut("Mute"),
      gGui->mixer.mute ? "<checked>" : "<check>",
      menu_audio_ctrl, (void *) AUDIO_MUTE                                                   },
    { _("Audio/Volume/Increase 10%"),
      NULL,
      NULL,
      menu_audio_ctrl, (void *) AUDIO_INCRE_VOL                                              },
    { _("Audio/Volume/Decrease 10%"),
      NULL,
      NULL,
      menu_audio_ctrl, (void *) AUDIO_DECRE_VOL                                              },
    { _("Audio/Channel"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Audio/Visualization"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Audio/SEP"),
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Audio/Postprocess"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Audio/Postprocess/Chain Reaction..."),
      NULL /* menu_get_shortcut("APProcessShow") */,
      NULL,
      menu_audio_ctrl, (void *) AUDIO_PPROCESS                                               },
    { _("Audio/Postprocess/Enable Postprocessing"),
      NULL /* menu_get_shortcut("APProcessEnable") */,
      gGui->post_audio_enable ? "<checked>" : "<check>",
      menu_audio_ctrl, (void *) AUDIO_PPROCESS_ENABLE                                        },
    { _("Subtitle"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Subtitle/Channel"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { "SEP",  
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Settings"),
      NULL,
      "<branch>",
      NULL, NULL                                                                             },
    { _("Settings/Setup..."),
      menu_get_shortcut("SetupShow"),
      NULL,
      menu_settings, (void *) SETS_SETUP                                                     },
#ifdef HAVE_CURL
    { _("Settings/Skin Downloader..."),
      menu_get_shortcut("SkinDownload"),
      NULL,
      menu_settings, (void *) SETS_SKINDL                                                    },
#endif
    { _("Settings/Keymap Editor..."),
      menu_get_shortcut("KeyBindingEditor"),
      NULL,
      menu_settings, (void *) SETS_KEYMAP                                                    },
    { _("Settings/Video..."),
      menu_get_shortcut("ControlShow"),
      NULL,
      menu_settings, (void *) SETS_VIDEO                                                     },
    { _("Settings/TV Analog..."),
      menu_get_shortcut("TVAnalogShow"),
      NULL,
      menu_settings, (void *) SETS_TVANALOG                                                  },
    { "SEP",
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Help..."),
      menu_get_shortcut("HelpShow"),
      NULL,
      menu_help, NULL                                                                        },
    { _("Logs..."),
      menu_get_shortcut("ViewlogShow"),
      NULL,
      menu_settings, (void *) SETS_LOGS                                                      },
    { "SEP",
      NULL,
      "<separator>",
      NULL, NULL                                                                             },
    { _("Quit"),
      menu_get_shortcut("Quit"),
      NULL,
      menu_quit, NULL                                                                        },
    { NULL,
      NULL,
      NULL,
      NULL, NULL                                                                             }
  };
  xitk_menu_entry_t *cur_entry;

  if(gGui->no_gui)
    return;

  gGui->nongui_error_msg = NULL;
  
  snprintf(buffer, sizeof(buffer), _("xine %s"), VERSION);
  menu_entries[0].menu = buffer;

  XITK_WIDGET_INIT(&menu, gGui->imlib_data);
  
  XLockDisplay(gGui->video_display);
  (void) xitk_get_mouse_coords(gGui->video_display, 
			       gGui->video_window, NULL, NULL, &x, &y);
  XUnlockDisplay(gGui->video_display);

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
  if(gGui->playlist.num) {
    xitk_menu_entry_t   menu_entry;
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Open/Subtitle...");
    menu_entry.cb        = menu_subtitle_selector;
    xitk_menu_add_entry(w, &menu_entry);

    /* Save Playlist */
    menu_entry.menu      = _("Playlist/Save...");
    menu_entry.cb        = menu_playlist_ctrl;
    menu_entry.user_data = (void *) PLAYL_SAVE;
    xitk_menu_add_entry(w, &menu_entry);
  }
  
  { /* Autoplay plugins */
    xitk_menu_entry_t   menu_entry;
    char                buffer[2048];
    char               *location = _("Playlist/Get from");
    const char *const  *plug_ids = xine_get_autoplay_input_plugin_ids(__xineui_global_xine_instance);
    const char         *plug_id;
    
    plug_id = *plug_ids++;
    while(plug_id) {

      memset(&buffer, 0, sizeof(buffer));
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      snprintf(buffer, sizeof(buffer), "%s/%s", location, plug_id);
      
      menu_entry.menu      = buffer;
      menu_entry.cb        = menu_playlist_from;
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
	menu_entry.type      = IS_CHANNEL_CHECKED(i, gGui->visual_anim.post_plugin_num);
	menu_entry.cb        = menu_audio_viz;
	menu_entry.user_data = (void *) i;
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
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Audio/Channel/Off");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Audio/Channel/Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char   langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_audio_lang(gGui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
	    menu_entry.user_data = (void *) i;
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
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  { /* SPU channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char               *location = _("Subtitle/Channel");
    char                buffer[2048];
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);
    
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
      
      if(!xine_get_spu_lang(gGui->stream, i, &langbuf[0])) {
	
	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *) i;
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
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }
  
  { /* Menus access */
    static const char menu_entries[14][16] = {
      /* Default menu */
      N_("Menu 1"), N_("Menu 2"), N_("Menu 3"), N_("Menu 4"),
      N_("Menu 5"), N_("Menu 6"), N_("Menu 7"),

      /* DVD menu */
      N_("Menu toggle"), N_("Title"), N_("Root"), N_("Subpicture"), 
      N_("Audio"), N_("Angle"), N_("Part"),
    };

    xitk_menu_entry_t   menu_entry;
    int                 i, j;
    const char *const menus_str = _("Menus");
    
    if((!strncmp(gGui->mmk.mrl, "dvd:/", 5)) || (!strncmp(gGui->mmk.mrl, "dvdnav:/", 8)))
      j = 7; /* Start from the DVD menu */
    
    for(i = 0; i < 7; i++) {
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

      asprintf(&menu_entry.menu, "%s/%s", menus_str, gettext(menu_entries[i]));
      menu_entry.cb        = menu_menus_selection;
      menu_entry.user_data = (void *)i;
      xitk_menu_add_entry(w, &menu_entry);
      free(menu_entry.menu);
    }
  }

  /* Mediamark */
  if(xine_get_status(gGui->stream) != XINE_STATUS_STOP) {
    xitk_menu_entry_t   menu_entry;

    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Playback/SEP");
    menu_entry.type      = "<separator>";
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));

    asprintf(&menu_entry.menu, "%s/%s", _("Playback"), _("Add Mediamark"));
    menu_entry.shortcut  = menu_get_shortcut("AddMediamark");
    menu_entry.cb        = menu_playback_ctrl;
    menu_entry.user_data = (void *) PLAYB_ADDMMK;
    xitk_menu_add_entry(w, &menu_entry);
    
    free(menu_entry.shortcut);
    free(menu_entry.menu);
  }

  xitk_menu_show_menu(w);
}

void audio_lang_menu(xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , NULL, "<title>",      NULL, NULL },
    { NULL,  NULL, NULL,           NULL, NULL }
  };

  menu_entries[0].menu = _("Audio");
  
  XITK_WIDGET_INIT(&menu, gGui->imlib_data);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* Audio channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Off");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Auto");
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char  langbuf[XINE_LANG_MAX];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_audio_lang(gGui->stream, i, &langbuf[0])) {

	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      menu_entry.menu      = (char *) get_language_from_iso639_1(langbuf);
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_audio_chan;
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void spu_lang_menu(xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , NULL, "<title>",      NULL, NULL },
    { NULL,  NULL, NULL,           NULL, NULL }
  };

  menu_entries[0].menu = _("Subtitle");
  
  XITK_WIDGET_INIT(&menu, gGui->imlib_data);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  { /* SPU channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char                buffer[2048];
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);
    
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
      
      if(!xine_get_spu_lang(gGui->stream, i, &langbuf[0])) {
	
	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
	    snprintf(buffer, sizeof(buffer), "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
      menu_entry.menu      = (char *) get_language_from_iso639_1(langbuf);
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_spu_chan;
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}

void playlist_menu(xitk_widget_list_t *wl, int x, int y, int selected) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  xitk_menu_entry_t    menu_entries_nosel[] = {
    { NULL ,           NULL,          "<title>",     NULL,                         NULL                    },
    { _("Scan"),       menu_get_shortcut("ScanPlaylistInfo"),
                                      NULL,          menu_scan_infos,              NULL                    },
    { _("Add"),        NULL,          NULL,          menu_open_mrlbrowser,         NULL                    },
    { NULL,            NULL,          NULL,          NULL,                         NULL                    }
  };
  xitk_menu_entry_t    menu_entries_sel[] = {
    { NULL ,           NULL,          "<title>",     NULL,                         NULL                    },
    { _("Play"),       NULL,          NULL,          menu_playlist_play_current,   NULL                    },
    { "SEP",           NULL,          "<separator>", NULL,                         NULL                    },
    { _("Scan"),       NULL,          NULL,          menu_scan_infos_selected,     NULL                    },
    { _("Add"),        NULL,          NULL,          menu_open_mrlbrowser,         NULL                    },
    { _("Edit"),       menu_get_shortcut("MediamarkEditor"),
                                      NULL,          menu_playlist_mmk_editor,     NULL                    },
    { _("Delete"),     NULL,          NULL,          menu_playlist_delete_current, NULL                    },
    { _("Delete All"), NULL,          NULL,          menu_playlist_delete_all,     NULL                    },
    { "SEP",           NULL,          "<separator>", NULL,                         NULL                    },
    { _("Move Up"),    NULL,          NULL,          menu_playlist_move_updown,    (void *) DIRECTION_UP   },
    { _("Move Down"),  NULL,          NULL,          menu_playlist_move_updown,    (void *) DIRECTION_DOWN },
    { NULL,            NULL,          NULL,          NULL,                         NULL                    }
  };
  xitk_menu_entry_t *cur_entry;

  
  XITK_WIDGET_INIT(&menu, gGui->imlib_data);
  
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
  
  if(!selected && gGui->playlist.num) {
    xitk_menu_entry_t   menu_entry;
    
    memset(&menu_entry, 0, sizeof(xitk_menu_entry_t));
    menu_entry.menu      = _("Delete All");
    menu_entry.cb        = menu_playlist_delete_all;
    xitk_menu_add_entry(w, &menu_entry);
  }

  xitk_menu_show_menu(w);
}

void control_menu(xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  xitk_widget_t       *w = NULL;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL ,           NULL,          "<title>",     NULL,                         NULL                    },
    { _("Reset video settings"),   
                       NULL,          NULL,          menu_control_reset,           NULL                    },
    { NULL,            NULL,          NULL,          NULL,                         NULL                    }
  };
  
  XITK_WIDGET_INIT(&menu, gGui->imlib_data);
  
  menu_entries[0].menu   = _("Video Control");
  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;

  w = xitk_noskin_menu_create(wl, &menu, x, y);
  
  xitk_menu_show_menu(w);
}
