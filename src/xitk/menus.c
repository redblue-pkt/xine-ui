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

#define AUDIO_MUTE        11

#define VIDEO_FULLSCR     12
#define VIDEO_2X          13
#define VIDEO_1X          14
#define VIDEO__5X         15

#define SETS_SETUP        16
#define SETS_KEYMAP       17
#define SETS_VIDEO        18
#define SETS_LOGS         19

static void menu_loopmode(xitk_widget_t *w, void *data) {
  int loop = (int) data;

  gGui->playlist.loop = loop;

  switch(gGui->playlist.loop) {
  case PLAYLIST_LOOP_NO_LOOP:
    osd_display_info(_("Playlist: no loop."));
    break;
  case PLAYLIST_LOOP_LOOP:
    osd_display_info(_("Playlist: loop."));
    break;
  case PLAYLIST_LOOP_REPEAT:
    osd_display_info(_("Playlist: entry repeat."));
    break;
  case PLAYLIST_LOOP_SHUFFLE:
    osd_display_info(_("Playlist: shuffle."));
    break;
  case PLAYLIST_LOOP_SHUF_PLUS:
    osd_display_info(_("Playlist: shuffle forever."));
    break;
  }
}

static void menu_panel_visibility(xitk_widget_t *w, void *data) {
  gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);
}
static void menu_file_selector(xitk_widget_t *w, void *data) {
  gui_execute_action_id(ACTID_FILESELECTOR);
}
static void menu_mrl_browser(xitk_widget_t *w, void *data) {
  gui_execute_action_id(ACTID_MRLBROWSER);
}
static void menu_playback_ctrl(xitk_widget_t *w, void *data) {
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
static void menu_playlist_ctrl(xitk_widget_t *w, void *data) {
  int ctrl = (int) data;

  switch(ctrl) {

  case PLAYL_LOAD:
  case PLAYL_SAVE:
    printf("TODO\n");
    break;

  case PLAYL_EDIT:
    gui_execute_action_id(ACTID_PLAYLIST);
    break;

  default:
    printf("%s(): unknown control %d\n", __XINE_FUNCTION__, ctrl);
    break;
  }
}
static void menu_audio_ctrl(xitk_widget_t *w, void *data) {
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
static void menu_video_ctrl(xitk_widget_t *w, void *data) {
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
static void menu_settings(xitk_widget_t *w, void *data) {
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
static void menu_quit(xitk_widget_t *w, void *data) {
  gui_execute_action_id(ACTID_QUIT);
}

void video_window_menu(xitk_widget_list_t *wl) {
  int                  x, y;
  xitk_menu_widget_t   menu;
  char                 buffer[2048];
  xitk_menu_entry_t    menu_entries[] = {
    { NULL ,                            "<title>",      
      NULL,                                                              NULL                   },
    { "GUI visiblity",                  panel_is_visible() ? "<checked>" : "<check>",  
      menu_panel_visibility,                                             NULL                   },
    { "SEP",                            "<separator>",              
      NULL,                                                              NULL                   },
    { "Open",                           "<branch>",   
      NULL,                                                              NULL                   },
    { "Open/File",                      NULL,
      menu_file_selector,                                                NULL                   },
    { "Open/Mrl Browser",               NULL,
      menu_mrl_browser,                                                  NULL                   },
    { "Playback",                       "<Branch>",
      NULL,                                                              NULL                   },
    { "Playback/Play",                  NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_PLAY    },
    { "Playback/Stop",                  NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_STOP    },
    { "Playback/Pause",                 NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_PAUSE   },
    { "Playback/Next MRL",              NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_NEXT    },
    { "Playback/Previous MRL",          NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_PREV    },
    { "Playback/Increase Speed",        NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_SPEEDM  },
    { "Playback/Decrease Speed",        NULL,
      menu_playback_ctrl,                                                (void *) PLAYB_SPEEDL  },
    { "Playlist",                       "<branch>",
      NULL,                                                              NULL                   },
    { "Playlist/Loop modes",            "<branch>",
      NULL,                                                              NULL                   },
    { "Playlist/Loop modes/FILLME",     NULL,
      NULL,                                                              NULL                   },
    { "Playlist/SEP",                   "<separator>",
      NULL,                                                              NULL                   },
    { "Playlist/Load",                  NULL,
      menu_playlist_ctrl,                                                (void *) PLAYL_LOAD    },
    { "Playlist/Save",                  NULL,
      menu_playlist_ctrl,                                                (void *) PLAYL_SAVE    },
    { "Playlist/Editor",                NULL,
      menu_playlist_ctrl,                                                (void *) PLAYL_EDIT    },
    { "Playlist/Get from...",           "<branch>",
      NULL,                                                              NULL                   },
    { "Playlist/Get from.../Dummy",     NULL,
      NULL,                                                              NULL                   },
    { "SEP",                            "<separator>",              
      NULL,                                                              NULL                   },
    { "Audio Mute",                     gGui->mixer.mute ? "<checked>":"<check>",
      menu_audio_ctrl,                                                   (void *) AUDIO_MUTE    },
    { "SEP",                            "<separator>",              
      NULL,                                                              NULL                   },
    { "Fullscreen\\/Window",            video_window_get_fullscreen_mode() ? "<checked>" : "<check>",
      menu_video_ctrl,                                                   (void *) VIDEO_FULLSCR },
    { "200%",                           (video_window_get_mag() == 2.0) ? "<checked>":"<check>",
      menu_video_ctrl,                                                   (void *) VIDEO_2X      },
    { "100%",                           (video_window_get_mag() == 1.0) ? "<checked>":"<check>",
      menu_video_ctrl,                                                   (void *) VIDEO_1X      },
    { "50%",                            (video_window_get_mag() ==  .5) ? "<checked>":"<check>",
      menu_video_ctrl,                                                   (void *) VIDEO__5X     },
    { "SEP",                            "<separator>",              
      NULL,                                                              NULL                   },
    { "Settings",                       "<branch>",
      NULL,                                                              NULL                   },
    { "Settings/Setup",                 NULL,
      menu_settings,                                                     (void *) SETS_SETUP    },
    { "Settings/Keymap Editor",         NULL,
      menu_settings,                                                     (void *) SETS_KEYMAP   },
    // ASPECT
    { "Settings/Video",                 NULL,
      menu_settings,                                                     (void *) SETS_VIDEO    },
    { "Settings/Logs",                  NULL,
      menu_settings,                                                     (void *) SETS_LOGS     },
    { "SEP",                            "<separator>",              
      NULL,                                                              NULL                   },
    { "Quit",                           NULL,
      menu_quit,                                                         NULL                   },
    { NULL,                             NULL,
      NULL,                                                              NULL                   }
  };

  sprintf(buffer, _("xine %s"), VERSION);
  menu_entries[0].menu = buffer;

  XITK_WIDGET_INIT(&menu, gGui->imlib_data);
  (void) xitk_get_mouse_coords(gGui->display, 
			       gGui->video_window, NULL, NULL, &x, &y);

  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = wl;
  menu.skin_element_name = NULL;
  
  (void *) xitk_noskin_menu_create(wl, &menu, x, y);
  


}
