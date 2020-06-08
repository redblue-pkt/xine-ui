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

#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <stdio.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "Imlib-light/Imlib.h"

typedef struct gGui_st gGui_t;

#include "xitk.h"

#include "kbindings.h"
#include "videowin.h"
#include "mediamark.h"
#include "actions.h"
#include "config_wrapper.h"
#include "control.h"
#include "errors.h"
#include "event.h"
#include "event_sender.h"
#include "help.h"
#include "i18n.h"
#include "lang.h"
#ifdef HAVE_LIRC
#include "lirc.h"
#endif
#include "stdctl.h"
#include "menus.h"
#include "mrl_browser.h"
#include "network.h"
#include "panel.h"
#include "playlist.h"
#include "session.h"
#include "setup.h"
#include "skins.h"
#include "snapshot.h"
#include "splash.h"
#include "stream_infos.h"
#include "tvset.h"
#include "viewlog.h"
#include "download.h"
#include "osd.h"
#include "file_browser.h"
#include "post.h"
#include "tvout.h"

#include "libcommon.h"
#include "globals.h"
#include "dump.h"

#ifdef HAVE_ORBIT 
#include "../corba/xine-server.h"
#endif

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>
#endif

/*
 * config related constants
 */
#define CONFIG_LEVEL_BEG         0 /* => beginner */
#define CONFIG_LEVEL_ADV        10 /* advanced user */
#define CONFIG_LEVEL_EXP        20 /* expert */
#define CONFIG_LEVEL_MAS        30 /* motku */
#define CONFIG_LEVEL_DEB        40 /* debugger (only available in debug mode) */

#define CONFIG_NO_DESC          NULL
#define CONFIG_NO_HELP          NULL
#define CONFIG_NO_CB            NULL
#define CONFIG_NO_DATA          NULL

/* Sound mixer capabilities */
#define MIXER_CAP_NOTHING       0x00000000
#define MIXER_CAP_VOL           0x00000001
#define MIXER_CAP_MUTE          0x00000002

/* mixer control method */
#define SOUND_CARD_MIXER        0
#define SOFTWARE_MIXER          1

/* Playlist loop modes */
#define PLAYLIST_LOOP_NO_LOOP   0 /* no loop (default) */
#define PLAYLIST_LOOP_LOOP      1 /* loop the whole playlist */
#define PLAYLIST_LOOP_REPEAT    2 /* loop the current mrl */
#define PLAYLIST_LOOP_SHUFFLE   3 /* random selection in playlist */
#define PLAYLIST_LOOP_SHUF_PLUS 4 /* random selection in playlist, never ending */
#define PLAYLIST_LOOP_MODES_NUM 5

#define PLAYLIST_CONTROL_STOP         0x00000001 /* Don't start next entry in playlist */
#define PLAYLIST_CONTROL_STOP_PERSIST 0x00000002 /* Don't automatically reset PLAYLIST_CONTROL_STOP */
#define PLAYLIST_CONTROL_IGNORE       0x00000004 /* Ignore some playlist adding action */

#define SAFE_FREE(x)            do {           \
                                    free((x)); \
                                    x = NULL;  \
                                } while(0)

#define ABORT_IF_NULL(p)                                                                      \
  do {                                                                                        \
    if((p) == NULL) {                                                                         \
      fprintf(stderr, "%s(%d): '%s' is NULL. Aborting.\n",  __XINE_FUNCTION__, __LINE__, #p); \
      abort();                                                                                \
    }                                                                                         \
  } while(0)

/* Duplicate s to d timeval values */
#define timercpy(s, d) do {                                                   \
      (d)->tv_sec = (s)->tv_sec;                                              \
      (d)->tv_usec = (s)->tv_usec;                                            \
} while(0)

#ifndef timersub
#define timersub(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
#endif


/* Our default location for skin downloads */
#define SKIN_SERVER_URL         "http://xine.sourceforge.net/skins/skins.slx"

#define fontname       "-*-helvetica-medium-r-*-*-11-*-*-*-*-*-*-*"
#define boldfontname   "-*-helvetica-bold-r-*-*-11-*-*-*-*-*-*-*"
#define hboldfontname  "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*"
#define lfontname      "-*-helvetica-bold-r-*-*-11-*-*-*-*-*-*-*"
#define tabsfontname   "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*"
#define br_fontname    "-misc-fixed-medium-r-normal-*-11-*-*-*-*-*-*-*"
#define btnfontname    "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*"

/* easy pseudo widgets for *_toggle_* calls. not all windows support them yet. */
#define XUI_W_TOGGLE NULL
#define XUI_W_ON ((xitk_widget_t *)1)
#define XUI_W_OFF ((xitk_widget_t *)2)

struct gGui_st {
  xine_t                   *xine;
  xine_video_port_t        *vo_port;
  xine_video_port_t        *vo_none;
  xine_audio_port_t        *ao_none;

  tvout_t                  *tvout;

  post_element_t          **post_video_elements;
  int                       post_video_elements_num;
  int                       post_video_enable;
  post_element_t          **post_audio_elements;
  int                       post_audio_elements_num;
  int                       post_audio_enable;

  char                     *deinterlace_plugin;
  post_element_t          **deinterlace_elements;
  int                       deinterlace_elements_num;
  int                       deinterlace_enable;

  xine_audio_port_t        *ao_port;

  xine_stream_t            *stream;

  struct {
    int                     pos;
    int                     time;
    int                     length;
  } stream_length;

  int                       subtitle_autoload;
  xine_stream_t            *spu_stream;

  int                       broadcast_port;

  /* Seek through xine_play () may be slow. We defer it to a separate thread. */
  pthread_mutex_t           seek_mutex;
  /* 0 = no, 1 = yes, 2 = shutdown */
  int                       seek_running;
  int                       seek_pos;
  int                       seek_timestep;

  xine_event_queue_t       *event_queue;

  int                       smart_mode;

  /* Visual stuff (like animation in video window while audio only playback) */
  struct {
    xine_stream_t          *stream;
    xine_event_queue_t     *event_queue;
    int                     running;
    int                     current;
    int                     enabled; /* 0, 1:vpost, 2:vanim */
    
    int                     num_mrls;
    char                  **mrls;
    
    int                     post_plugin_num;
    int                     post_changed;
    post_element_t          post_output_element;    
  } visual_anim;
  
  struct {
    int                     enabled;
    int                     use_unscaled;
    int                     unscaled_available;
    int                     timeout;

    osd_object_t            sinfo;
    osd_object_t            bar;
    osd_object_t            status;
    osd_object_t            info;

  } osd;

  int                       experience_level;

  int                       logo_mode;
  const char               *logo_mrl;
  pthread_mutex_t           logo_mutex;
  int                       logo_has_changed;
  int                       display_logo;

  /* stuff like ACTID_x */
#define MAX_ACTIONS_ON_START 32
  action_id_t               actions_on_start[MAX_ACTIONS_ON_START+1];
  char                     *autoscan_plugin;

  /* basic X11 stuff */
  void                    (*x_lock_display) (Display *display);
  void                    (*x_unlock_display) (Display *display);
  Display                  *display;
  int                       screen;
  int		            depth;
  Visual	           *visual;
  XColor                    black;
  Pixmap                    icon;
  Colormap                  colormap;

  VisualID	            prefered_visual_id;
  int		            prefered_visual_class;
  int		            install_colormap;

  xitk_skin_config_t       *skin_config;
  char                     *skin_server_url;

  ImlibData                *imlib_data;

  Display                  *video_display;
  Window                    video_window; 
  int                       cursor_visible;
  int                       cursor_grabbed;

  uint32_t                  debug_level;

  int                       is_display_mrl;

  int                       mrl_overrided;

  /* Current mediamark */
  mediamark_t               mmk;
  /* Recursive mutex, protecting .mmk and .playlist. */
  pthread_mutex_t           mmk_mutex;
  
  /* playlist */
  struct {
    mediamark_t           **mmk;
    
    int                     num;                   /* number of entries in playlist */
    int                     cur;                   /* current entry in playlist */
    int                     ref_append;            /* append mrl reference entry to this position */
    int                     loop;                  /* current loop mode (see PLAYLIST_LOOP_* */
    int                     control;               /* see PLAYLIST_CONTROL_* */
  } playlist;


  int                       on_quit;
  int                       running;
  int                       ignore_next;

  int                       stdctl_enable;
  
#ifdef HAVE_XF86VIDMODE
  int                       XF86VidMode_fullscreen;
#endif

  struct {
    int                     caps; /* MIXER_CAP_x */
    int                     volume_level;
    int                     mute;
    int                     amp_level;
    int                     method;
  } mixer;

  xitk_register_key_t       widget_key;

  int                       layer_above;
  int                       always_layer_above;
  
  int                       network;
  int                       network_port;
  
  int                       use_root_window;

  int                       ssaver_enabled;
  int                       ssaver_timeout;

  int                       skip_by_chapter;

  int                       auto_vo_visibility;
  int                       auto_panel_visibility;

  const char               *snapshot_location;
  
  char                      *keymap_file;
  kbinding_t                *kbindings;
  int                        shortcut_style;
  int                        kbindings_enabled;

  /* event handling */
  struct {
    int                      set;
    int                      arg;
  } numeric;
  struct {
    int                      set;
    char                    *arg;
  } alphanum;
  int                        event_reject;
  int                        event_pending;
  pthread_mutex_t            event_mutex;
  pthread_cond_t             event_safe;

  int                        eventer_sticky;
  int                        stream_info_auto_update;

  char                       curdir[XITK_PATH_MAX];

  int                        play_anyway;

  int                        splash;

  pthread_mutex_t            download_mutex;

  FILE                      *report;

  int                        no_gui;
  
  int                        no_mouse;
  int                        wid;

  void                     (*nongui_error_msg)(const char *text);

  FILE                      *orig_stdout; /* original stdout at startup        */
                                          /* before an evtl. later redirection */

  /* suspend event messages */
  struct {
    pthread_mutex_t          mutex;
    struct timeval           until;
    int                      level;
  } no_messages;

  xui_panel_t               *panel;
  xui_vwin_t                *vwin;
  xui_setup_t               *setup;
  xui_mrlb_t                *mrlb;
  xui_vctrl_t               *vctrl;

  /* actions.c */
  struct {
    xine_stream_t           *stream;
    int                      start_pos;
    int                      start_time_in_secs;
    int                      update_mmk;
    int                      running;
  } play_data;
  filebrowser_t             *load_stream;
  filebrowser_t             *load_sub;
  int                        last_playback_speed;

  /* main.c */
  char                     **video_driver_ids;
  char                     **audio_driver_ids;
  window_attributes_t        window_attribute;
};

extern gGui_t *gGui;

void set_window_states_start(xitk_window_t *xwin);
#define set_window_states_start(xwin)                                     \
  do {                                                                    \
    if(!video_window_is_visible(gGui->vwin))                              \
      xitk_window_set_wm_window_type((xwin), WINDOW_TYPE_NORMAL);         \
    else                                                                  \
      xitk_window_unset_wm_window_type((xwin), WINDOW_TYPE_NORMAL);       \
    xitk_window_set_window_class((xwin), NULL, "xine"); \
    xitk_window_set_window_icon((xwin), gGui->icon);    \
  } while(0)

void reparent_window(xitk_window_t *xwin);
#define reparent_window(xwin)                                                            \
  do {                                                                                   \
    if((!(video_window_get_fullscreen_mode(gGui->vwin) & WINDOWED_MODE)) && !wm_not_ewmh_only()) { \
      /* Don't unmap this window, because on re-mapping, it will not be visible until    \
	 its ancestor, the video window, is visible. That's not what's intended.         \
         XUnmapWindow(gGui->display, (window));                                          \
      */								                 \
      if(!video_window_is_visible(gGui->vwin))                                           \
        xitk_window_set_wm_window_type((xwin), WINDOW_TYPE_NORMAL);                      \
      else                                                                               \
        xitk_window_unset_wm_window_type((xwin), WINDOW_TYPE_NORMAL);                    \
      xitk_window_show_window((xwin), 1);                                                \
      xitk_window_try_to_set_input_focus((xwin));                                        \
      video_window_set_transient_for(gGui->vwin, (xwin));                                \
    }                                                                                    \
    else {                                                                               \
      xitk_window_show_window((xwin), 1);                                                \
      video_window_set_transient_for(gGui->vwin, (xwin));                                \
      layer_above_video((xwin));                                                         \
    }                                                                                    \
  } while(0)

#ifdef HAVE_XML_PARSER_REENTRANT
# define xml_parser_init_R(X,D,L,M) X = xml_parser_init_r ((D), (L), (M))
# define xml_parser_build_tree_R(X,T) xml_parser_build_tree_r ((X), (T))
# define xml_parser_finalize_R(X) xml_parser_finalize_r ((X))
#else
# define xml_parser_init_R(X,D,L,M) xml_parser_init ((D), (L), (M))
# define xml_parser_build_tree_R(X,T) xml_parser_build_tree ((T))
# define xml_parser_finalize_R(X) do {} while (0)
#endif

#endif
