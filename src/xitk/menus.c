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

#include "common.h"

extern gGui_t                  *gGui;
extern _panel_t                *panel;

#define PLAYB_PLAY              1
#define PLAYB_STOP              2
#define PLAYB_PAUSE             3
#define PLAYB_NEXT              4
#define PLAYB_PREV              5
#define PLAYB_SPEEDM            6
#define PLAYB_SPEEDL            7

#define PLAYL_LOAD              10
#define PLAYL_SAVE              11
#define PLAYL_EDIT              12
#define PLAYL_NO_LOOP           13
#define PLAYL_LOOP              14
#define PLAYL_REPEAT            15
#define PLAYL_SHUFFLE           16
#define PLAYL_SHUF_PLUS         17

#define AUDIO_MUTE              20
#define AUDIO_INCRE_VOL         21
#define AUDIO_DECRE_VOL         22

#define VIDEO_FULLSCR           30
#define VIDEO_2X                31
#define VIDEO_1X                32
#define VIDEO__5X               33
#define VIDEO_INTERLEAVE        34
#define VIDEO_PPROCESS          35
#define VIDEO_PPROCESS_ENABLE   36

#define SETS_SETUP              40
#define SETS_KEYMAP             41
#define SETS_VIDEO              42
#define SETS_LOGS               43
#define SETS_SKINDL             44
#define SETS_TVANALOG           45

#define STREAM_OSDI             50
#define STREAM_WINI             51

#define IS_CHANNEL_CHECKED(C, N) (C == N) ? "<checked>" : "<check>"


static void menu_menus_selection(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int event     = (int) data;
  int events[7] = {
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

  switch(ctrl) {

  case PLAYB_PLAY:
    gui_execute_action_id(ACTID_PLAY);
    break;
    
  case PLAYB_STOP:
    gui_execute_action_id(ACTID_STOP);
    break;
    
  case PLAYB_PAUSE:
    gui_execute_action_id(ACTID_PAUSE);
    break;

  case PLAYB_NEXT:
    gui_execute_action_id(ACTID_MRL_NEXT);
    break;

  case PLAYB_PREV:
    gui_execute_action_id(ACTID_MRL_PRIOR);
    break;

  case PLAYB_SPEEDM:
    gui_execute_action_id(ACTID_SPEED_FAST);
    break;

  case PLAYB_SPEEDL:
    gui_execute_action_id(ACTID_SPEED_SLOW);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
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

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}
static void menu_playlist_from(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int       num_mrls;
  char    **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine, me->menu, &num_mrls);

  if(autoplay_mrls) {
    int j;
    
    /* Flush playlist in newbie mode */
    if(gGui->smart_mode) {
      mediamark_free_mediamarks();
      gGui->playlist.cur = 0;
      playlist_update_playlist();
    }
    
    if(!gGui->playlist.num)
      gGui->playlist.cur = 0;
    
    for (j = 0; j < num_mrls; j++)
      mediamark_add_entry(autoplay_mrls[j], autoplay_mrls[j], NULL, 0, -1, 0);
    
    if(gGui->playlist.cur == 0)
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    
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
  int ctrl = (int) data;

  switch(ctrl) {

  case VIDEO_FULLSCR:
    gui_execute_action_id(ACTID_TOGGLE_FULLSCREEN);
    break;
    
  case VIDEO_2X:
    gui_execute_action_id(ACTID_WINDOW200);
    break;
    
  case VIDEO_1X:
    gui_execute_action_id(ACTID_WINDOW100);
    break;
    
  case VIDEO__5X:
    gui_execute_action_id(ACTID_WINDOW50);
    break;
    
  case VIDEO_INTERLEAVE:
    gui_execute_action_id(ACTID_TOGGLE_INTERLEAVE);
    break;

  case VIDEO_PPROCESS:
    gui_execute_action_id(ACTID_VPP);
    break;

  case VIDEO_PPROCESS_ENABLE:
    gui_execute_action_id(ACTID_VPP_ENABLE);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}
static void menu_settings(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int sets = (int) data;

  switch(sets) {

  case SETS_SETUP:
    gui_execute_action_id(ACTID_SETUP);
    break;
    
  case SETS_KEYMAP:
    gui_execute_action_id(ACTID_KBEDIT);
    break;
    
  case SETS_VIDEO:
    gui_execute_action_id(ACTID_CONTROLSHOW);
    break;

  case SETS_TVANALOG:
    gui_execute_action_id(ACTID_TVANALOG);
    break;

  case SETS_LOGS:
    gui_execute_action_id(ACTID_VIEWLOG);
    break;

  case SETS_SKINDL:
    gui_execute_action_id(ACTID_SKINDOWNLOAD);
    break;
    
  default:
    printf("%s(): unknown setting %d\n", __XINE_FUNCTION__, sets);
    break;
  }
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
  xitk_menu_entry_t    menu_entries[] = {
    { NULL ,
      "<title>",      
      NULL, NULL                                                                             },
    { "GUI visiblity",
      panel_is_visible() ? "<checked>" : "<check>",  
      menu_panel_visibility, NULL                                                            },
    { "SEP",  
      "<separator>",  
      NULL,  NULL                                                                            },
    { "Open",
      "<branch>",   
      NULL, NULL                                                                             },
    { "Open/File...",
      NULL,
      menu_file_selector,        NULL                                                        },
    { "Open/Playlist...",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_LOAD                                                },
    { "Open/Location...",
      NULL,
      menu_mrl_browser, NULL                                                                 },
    { "Playback",
      "<Branch>",
      NULL, NULL                                                                             },
    { "Playback/Play",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PLAY                                                },
    { "Playback/Stop",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_STOP                                                },
    { "Playback/Pause",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PAUSE                                               },
    { "Playback/Next MRL",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_NEXT                                                },
    { "Playback/Previous MRL",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_PREV                                                },
    { "Playback/Increase Speed",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_SPEEDM                                              },
    { "Playback/Decrease Speed",
      NULL,
      menu_playback_ctrl, (void *) PLAYB_SPEEDL                                              },
    { "Playlist",
      "<branch>",
      NULL, NULL                                                                             },
    { "Playlist/Loop modes",
      "<branch>",
      NULL, NULL                                                                             },
    { "Playlist/Loop modes/Disabled",
      (gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_NO_LOOP                                             },
    { "Playlist/Loop modes/Loop",
      (gGui->playlist.loop == PLAYLIST_LOOP_LOOP) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_LOOP                                                },
    { "Playlist/Loop modes/Repeat Selection",
      (gGui->playlist.loop == PLAYLIST_LOOP_REPEAT) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_REPEAT                                              },
    { "Playlist/Loop modes/Shuffle",
      (gGui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_SHUFFLE                                             },
    { "Playlist/Loop modes/Non-stop Shuffle",
      (gGui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS) ? "<checked>" : "<check>",
      menu_playlist_ctrl, (void *) PLAYL_SHUF_PLUS                                           },
    { "Playlist/SEP",  
      "<separator>",  
      NULL, NULL                                                                             },
    { "Playlist/Get from...",
      "<branch>",
      NULL, NULL                                                                             },
    /*
      autoplay input plugin idents      
    */
    { "Playlist/Load...",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_LOAD                                                },
    { "Playlist/Editor...",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_EDIT                                                },
    { "SEP",  
      "<separator>",
      NULL, NULL                                                                             },
    { "Menus",
      "<branch>",
      NULL, NULL                                                                             },
    { "SEP",  
      "<separator>",
      NULL, NULL                                                                             },
    { "Audio",
      "<branch>",
      NULL, NULL                                                                             },
    { "Audio/Volume",
      "<branch>",
      NULL, NULL                                                                             },
    { "Audio/Volume/Mute",
      gGui->mixer.mute ? "<checked>" : "<check>",
      menu_audio_ctrl, (void *) AUDIO_MUTE                                                   },
    { "Audio/Volume/Increase 10%",
      NULL,
      menu_audio_ctrl, (void *) AUDIO_INCRE_VOL                                              },
    { "Audio/Volume/Decrease 10%",
      NULL,
      menu_audio_ctrl, (void *) AUDIO_DECRE_VOL                                              },
    { "Audio/Channel",
      "<branch>",
      NULL, NULL                                                                             },
    { "Audio/Visualization",
      "<branch>",
      NULL, NULL                                                                             },
    /*
      audio channels
    */
    { "SEP",  
      "<separator>",
      NULL, NULL                                                                             },
    { "Fullscreen\\/Window",
      video_window_get_fullscreen_mode() ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_FULLSCR                                                },
    { "Video",
      "<branch>",
      NULL, NULL                                                                             },
    { "Video/Deinterlace",
      (xine_get_param(gGui->stream, XINE_PARAM_VO_DEINTERLACE)) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_INTERLEAVE                                             },
    { "Video/SEP",
      "<separator>",
      NULL, NULL                                                                             },
    { "Video/Postprocess",
      "<branch>",
      NULL, NULL                                                                             },
    { "Video/Postprocess/Chain Reaction...",
      NULL,
      menu_video_ctrl, (void *) VIDEO_PPROCESS                                               },
    { "Video/Postprocess/Enable",
      gGui->post_enable ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_PPROCESS_ENABLE                                        },
    { "Video/SEP",
      "<separator>",
      NULL, NULL                                                                             },
    { "Video/Aspect ratio",
      "<branch>",
      NULL, NULL                                                                             },
    { "Video/Aspect ratio/Automatic",
      (aspect == XINE_VO_ASPECT_AUTO) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_AUTO                                              },
    { "Video/Aspect ratio/Square",
      (aspect == XINE_VO_ASPECT_SQUARE) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_SQUARE                                            },
    { "Video/Aspect ratio/4:3",
      (aspect == XINE_VO_ASPECT_4_3) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_4_3                                               },
    { "Video/Aspect ratio/Anamorphic",
      (aspect == XINE_VO_ASPECT_ANAMORPHIC) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_ANAMORPHIC                                        },
    { "Video/Aspect ratio/DVB",
      (aspect == XINE_VO_ASPECT_DVB) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_DVB                                               },
    { "Video/200%",
      (video_window_get_mag() == 2.0) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_2X                                                     },
    { "Video/100%",
      (video_window_get_mag() == 1.0) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_1X                                                     },
    { "Video/50%",
      (video_window_get_mag() ==  .5) ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO__5X                                                    },
    { "SEP", 
      "<separator>",
      NULL, NULL                                                                             },
    { "Subtitle",
      "<branch>",
      NULL, NULL                                                                             },
    { "Subtitle/Channel",
      "<branch>",
      NULL, NULL                                                                             },
    /*
      spu channels
    */
    { "SEP",
      "<separator>",
      NULL, NULL                                                                             },
    { "Stream",
      "<branch>",
      NULL, NULL                                                                             },
    { "Stream/OSD Information",
      NULL,
      menu_stream, (void *) STREAM_OSDI                                                      },
    { "Stream/Window Information...",
      NULL,
      menu_stream, (void *) STREAM_WINI                                                      },
    { "SEP",  
      "<separator>",
      NULL, NULL                                                                             },
    { "Settings",
      "<branch>",
      NULL, NULL                                                                             },
    { "Settings/Setup...",
      NULL,
      menu_settings, (void *) SETS_SETUP                                                     },
#ifdef HAVE_CURL
    { "Settings/Skin Downloader...",
      NULL,
      menu_settings, (void *) SETS_SKINDL                                                    },
#endif
    { "Settings/Keymap Editor...",
      NULL,
      menu_settings, (void *) SETS_KEYMAP                                                    },
    { "Settings/Video...",
      NULL,
      menu_settings, (void *) SETS_VIDEO                                                     },
    { "Settings/TV Analog...",
      NULL,
      menu_settings, (void *) SETS_TVANALOG                                                  },
    { "Settings/Logs...",
      NULL,
      menu_settings, (void *) SETS_LOGS                                                      },
    { "SEP",
      "<separator>",
      NULL, NULL                                                                             },
    { "Quit",
      NULL,
      menu_quit, NULL                                                                        },
    { NULL,
      NULL,
      NULL, NULL                                                                             }
  };

  sprintf(buffer, _("xine %s"), VERSION);
  menu_entries[0].menu = buffer;

  XITK_WIDGET_INIT(&menu, gGui->imlib_data);
  (void) xitk_get_mouse_coords(gGui->display, 
			       gGui->video_window, NULL, NULL, &x, &y);
  
  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  w = xitk_noskin_menu_create(wl, &menu, x + 1, y + 1);

  /* Subtitle loader */
  if(gGui->playlist.num) {
    xitk_menu_entry_t   menu_entry;
    
    menu_entry.menu      = "Open/Subtitle...";
    menu_entry.type      = NULL;
    menu_entry.cb        = menu_subtitle_selector;
    menu_entry.user_data = NULL;
    xitk_menu_add_entry(w, &menu_entry);

    /* Save Playlist */
    menu_entry.menu      = "Playlist/Save...";
    menu_entry.type      = NULL;
    menu_entry.cb        = menu_playlist_ctrl;
    menu_entry.user_data = (void *) PLAYL_SAVE;
    xitk_menu_add_entry(w, &menu_entry);
  }
  
  { /* Autoplay plugins */
    xitk_menu_entry_t   menu_entry;
    char                buffer[2048];
    char               *location = "Playlist/Get from...";
    const char *const  *plug_ids = xine_get_autoplay_input_plugin_ids(gGui->xine);
    const char         *plug_id;
    
    plug_id = *plug_ids++;
    while(plug_id) {

      memset(&buffer, 0, sizeof(buffer));
      sprintf(buffer, "%s/%s", location, plug_id);
      
      menu_entry.menu      = buffer;
      menu_entry.type      = NULL;
      menu_entry.cb        = menu_playlist_from;
      menu_entry.user_data = NULL;
      xitk_menu_add_entry(w, &menu_entry);
      
      plug_id = *plug_ids++;
    }
  }

  { /* Audio Viz */
    xitk_menu_entry_t   menu_entry;
    const char        **viz_names = post_get_audio_plugins_names();

    if(viz_names && *viz_names) {
      int                 i = 0;
      char               *location = "Audio/Visualization";
      char                buffer[2048];

      while(viz_names[i]) {
	sprintf(buffer, "%s/%s", location, viz_names[i]);
	menu_entry.menu      = buffer;
	menu_entry.type      = IS_CHANNEL_CHECKED(i, gGui->visual_anim.post_plugin_num);
	menu_entry.cb        = menu_audio_viz;
	menu_entry.user_data = (void *) i;
	xitk_menu_add_entry(w, &menu_entry);
	i++;
      }

    }
    else {
      menu_entry.menu      = "Audio/Visualization/None";
      menu_entry.type      = NULL;
      menu_entry.cb        = NULL;
      menu_entry.user_data = NULL;
      xitk_menu_add_entry(w, &menu_entry);
    }
    
  }

  { /* Audio channels */
    xitk_menu_entry_t   menu_entry;
    int                 i;
    char               *location = "Audio/Channel";
    char                buffer[2048];
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    
    menu_entry.menu      = "Audio/Channel/Off";
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    menu_entry.menu      = "Audio/Channel/Auto";
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
	    sprintf(buffer, "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      sprintf(buffer, "%s/%s", location, (get_language_from_iso639_1(langbuf)));
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
    char               *location = "Subtitle/Channel";
    char                buffer[2048];
    int                 channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);
    
    menu_entry.menu      = "Subtitle/Channel/Off";
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    menu_entry.menu      = "Subtitle/Channel/Auto";
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
	    sprintf(buffer, "%s/%d", location, i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      sprintf(buffer, "%s/%s", location, (get_language_from_iso639_1(langbuf)));
      menu_entry.menu      = buffer;
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_spu_chan;
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }
  
  { /* Menus access */
    xitk_menu_entry_t   menu_entry;
    char                buffer[2048];
    char               *location = "Menus";
    char               *default_menu[8] = {
      "Menu 1", "Menu 2", "Menu 3", "Menu 4", "Menu 5", "Menu 6", "Menu 7", NULL
    };
    char               *dvd_menu[8] = {
      "Menu toggle", "Title", "Root", "Subpicture", "Audio", "Angle", "Part", NULL
    };
    char              **menu = default_menu;
    int                 i;
    
    if((!strncmp(gGui->mmk.mrl, "dvd:/", 5)) || (!strncmp(gGui->mmk.mrl, "dvdnav:/", 8)))
      menu = dvd_menu;
    
    for(i = 0; i < 7; i++) {
      sprintf(buffer, "%s/%s", location, menu[i]);
      
      menu_entry.menu      = buffer;
      menu_entry.type      = NULL;
      menu_entry.cb        = menu_menus_selection;
      menu_entry.user_data = (void *)i;
      xitk_menu_add_entry(w, &menu_entry);
    }
  }

  xitk_menu_show_menu(w);
}

void audio_lang_menu(xitk_widget_list_t *wl, int x, int y) {
  xitk_menu_widget_t   menu;
  char                 buffer[2048];
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , "<title>",      NULL, NULL },
    { "SEP", "<separator>",  NULL, NULL },
    { NULL,  NULL,           NULL, NULL }
  };

  sprintf(buffer, "%s", _("Audio"));
  menu_entries[0].menu = buffer;
  
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
    
    menu_entry.menu      = "Off";
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_audio_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    menu_entry.menu      = "Auto";
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
	    sprintf(buffer, "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_audio_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
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
  char                 buffer[2048];
  xitk_widget_t       *w;
  xitk_menu_entry_t    menu_entries[] = {
    { NULL , "<title>",      NULL, NULL },
    { "SEP", "<separator>",  NULL, NULL },
    { NULL,  NULL,           NULL, NULL }
  };

  sprintf(buffer, "%s", _("Subtitle"));
  menu_entries[0].menu = buffer;
  
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
    
    menu_entry.menu      = "Off";
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -2);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -2;
    xitk_menu_add_entry(w, &menu_entry);
    
    menu_entry.menu      = "Auto";
    menu_entry.type      = IS_CHANNEL_CHECKED(channel, -1);
    menu_entry.cb        = menu_spu_chan;
    menu_entry.user_data = (void *) -1;
    xitk_menu_add_entry(w, &menu_entry);
    
    for(i = 0; i < 32; i++) {
      char   langbuf[32];
      
      memset(&langbuf, 0, sizeof(langbuf));
      
      if(!xine_get_spu_lang(gGui->stream, i, &langbuf[0])) {
	
	if(i == 0) {
	  for(i = 0; i < 15; i++) {
	    sprintf(buffer, "%d", i);
	    menu_entry.menu      = buffer;
	    menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
	    menu_entry.cb        = menu_spu_chan;
	    menu_entry.user_data = (void *) i;
	    xitk_menu_add_entry(w, &menu_entry);
	  }
	}

	break;
      }
      
      menu_entry.menu      = (char *) get_language_from_iso639_1(langbuf);
      menu_entry.type      = IS_CHANNEL_CHECKED(channel, i);
      menu_entry.cb        = menu_spu_chan;
      menu_entry.user_data = (void *) i;
      xitk_menu_add_entry(w, &menu_entry);
    }

  }

  xitk_menu_show_menu(w);
}
