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

extern gGui_t             *gGui;

#define PLAYB_PLAY         1
#define PLAYB_STOP         2
#define PLAYB_PAUSE        3
#define PLAYB_NEXT         4
#define PLAYB_PREV         5
#define PLAYB_SPEEDM       6
#define PLAYB_SPEEDL       7

#define PLAYL_LOAD         8
#define PLAYL_SAVE         9
#define PLAYL_EDIT        10
#define PLAYL_NO_LOOP     11
#define PLAYL_LOOP        12
#define PLAYL_REPEAT      13
#define PLAYL_SHUFFLE     14
#define PLAYL_SHUF_PLUS   15

#define AUDIO_MUTE        16

#define VIDEO_FULLSCR     17
#define VIDEO_2X          18
#define VIDEO_1X          19
#define VIDEO__5X         20

#define SETS_SETUP        21
#define SETS_KEYMAP       22
#define SETS_VIDEO        23
#define SETS_LOGS         24

static void menu_panel_visibility(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);
}
static void menu_file_selector(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_FILESELECTOR);
}
static void menu_mrl_browser(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  gui_execute_action_id(ACTID_MRLBROWSER);
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
  case PLAYL_SAVE:
    printf("TODO\n");
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
      mediamark_add_entry(autoplay_mrls[j], autoplay_mrls[j], NULL, 0, -1);
    
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
  }
}
static void menu_audio_ctrl(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  int ctrl = (int) data;

  switch(ctrl) {
  case AUDIO_MUTE:
    gui_execute_action_id(ACTID_MUTE);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
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

  case SETS_LOGS:
    gui_execute_action_id(ACTID_VIEWLOG);
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
    { "Open/File",
      NULL,
      menu_file_selector,        NULL                                                        },
    { "Open/Mrl Browser",
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
    { "Playlist/Load",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_LOAD                                                },
    { "Playlist/Save",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_SAVE                                                },
    { "Playlist/Editor",
      NULL,
      menu_playlist_ctrl, (void *) PLAYL_EDIT                                                },
    { "Playlist/Get from...",
      "<branch>",
      NULL, NULL                                                                             },
    /*
      autoplay input plugin idents      
    */
    { "SEP",  
      "<separator>",
      NULL, NULL                                                                             },
    { "Audio Mute",
      gGui->mixer.mute ? "<checked>":"<check>",
      menu_audio_ctrl, (void *) AUDIO_MUTE                                                   },
    { "SEP",
      "<separator>",
      NULL, NULL                                                                             },
    { "Aspect ratio",
      "<branch>",
      NULL, NULL                                                                             },
    { "Aspect ratio/Automatic",
      (aspect == XINE_VO_ASPECT_AUTO) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_AUTO                                              },
    { "Aspect ratio/Square",
      (aspect == XINE_VO_ASPECT_SQUARE) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_SQUARE                                            },
    { "Aspect ratio/4:3",
      (aspect == XINE_VO_ASPECT_4_3) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_4_3                                               },
    { "Aspect ratio/Anamorphic",
      (aspect == XINE_VO_ASPECT_ANAMORPHIC) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_ANAMORPHIC                                        },
    { "Aspect ratio/DVB",
      (aspect == XINE_VO_ASPECT_DVB) ? "<checked>" : "<check>",
      menu_aspect, (void *) XINE_VO_ASPECT_DVB                                               },
    { "Fullscreen\\/Window",
      video_window_get_fullscreen_mode() ? "<checked>" : "<check>",
      menu_video_ctrl, (void *) VIDEO_FULLSCR                                                },
    { "200%",
      (video_window_get_mag() == 2.0) ? "<checked>":"<check>",
      menu_video_ctrl, (void *) VIDEO_2X                                                     },
    { "100%",
      (video_window_get_mag() == 1.0) ? "<checked>":"<check>",
      menu_video_ctrl, (void *) VIDEO_1X                                                     },
    { "50%",
      (video_window_get_mag() ==  .5) ? "<checked>":"<check>",
      menu_video_ctrl, (void *) VIDEO__5X                                                    },
    { "SEP", 
      "<separator>",
      NULL, NULL                                                                             },
    { "Settings",
      "<branch>",
      NULL, NULL                                                                             },
    { "Settings/Setup",
      NULL,
      menu_settings, (void *) SETS_SETUP                                                     },
    { "Settings/Keymap Editor",
      NULL,
      menu_settings, (void *) SETS_KEYMAP                                                    },
    { "Settings/Video",
      NULL,
      menu_settings, (void *) SETS_VIDEO                                                     },
    { "Settings/Logs",
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
  
  w = xitk_noskin_menu_create(wl, &menu, x, y);

  {
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

  xitk_menu_show_menu(w);
}
