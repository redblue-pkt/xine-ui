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

#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <stdio.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/sorted_array.h>

typedef struct gGui_st gGui_t;

#include "xitk.h"

typedef struct xui_keyedit_s xui_keyedit_t;
typedef struct xui_vwin_st xui_vwin_t;
typedef struct xui_actrl_st xui_actrl_t;
typedef struct xui_vctrl_st xui_vctrl_t;
typedef struct xui_event_sender_st xui_event_sender_t;
typedef struct xui_help_s xui_help_t;
typedef struct xui_mrlb_st xui_mrlb_t;
typedef struct xui_panel_st xui_panel_t;
typedef struct xui_mmkedit_s xui_mmkedit_t;
typedef struct xui_playlist_st xui_playlist_t;
typedef struct xui_setup_st xui_setup_t;
typedef struct xui_viewlog_s xui_viewlog_t;
typedef struct filebrowser_s filebrowser_t;
typedef struct xui_tvset_s xui_tvset_t;
typedef struct xui_tvout_s xui_tvout_t;
typedef struct xui_skdloader_s xui_skdloader_t;
typedef struct xui_sinfo_s xui_sinfo_t;
typedef struct xui_lirc_s xui_lirc_t;

#include "config_wrapper.h"
#include "i18n.h"
#include "post.h"
#include "osd.h"
#include "kbindings.h"
#include "mediamark.h"

#include "libcommon.h"

#ifdef HAVE_ORBIT
#include "../corba/xine-server.h"
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
typedef enum {
  PLAYLIST_LOOP_NO_LOOP = 0, /* no loop (default) */
  PLAYLIST_LOOP_LOOP,        /* loop the whole playlist */
  PLAYLIST_LOOP_REPEAT,      /* loop the current mrl */
  PLAYLIST_LOOP_SHUFFLE,     /* random selection in playlist */
  PLAYLIST_LOOP_SHUF_PLUS,   /* random selection in playlist, never ending */
  PLAYLIST_LOOP_MODES_NUM
} playlist_loop_t;

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

typedef struct skins_locations_s skins_locations_t;

typedef enum {
  XUI_EXTS_ANY = 0,
  XUI_EXTS_MEDIA,
  XUI_EXTS_AUDIO,
  XUI_EXTS_VIDEO,
  XUI_EXTS_IMAGE,
  XUI_EXTS_SUBTITLE,
  XUI_EXTS_PLAYLIST,
  XUI_EXTS_LAST
} xui_exts_t;

struct gGui_st {
  struct gGui_st           *nextprev[3];

  char                     *cfg_file;
  xine_t                   *xine;
  xine_video_port_t        *vo_port;
  xine_video_port_t        *vo_none;
  xine_audio_port_t        *ao_none;

  int                       verbosity;
  int                       post_video_enable;
  int                       post_audio_enable;
  int                       deinterlace_enable;
  int                       lirc_enable;

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

  post_info_t               post_audio, post_video;

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

  xitk_t                   *xitk;

  /* basic X11 stuff */
  xitk_image_t             *icon;

  xitk_skin_config_t       *skin_config;
  char                     *skin_server_url;

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

    int                     max;        /** << number of entries that fit .mmk */
    int                     num;        /** << number of entries set in .mmk */
    int                     cur;        /** << current entry in playlist */
    int                     ref_append; /** << append mrl reference entry to this position */
    int                     loop;       /** << see PLAYLIST_LOOP_* */
    int                     control;    /** << see PLAYLIST_CONTROL_* */

    xine_sarray_t          *exts[XUI_EXTS_LAST];
  } playlist;


  int                       on_quit;
  int                       running;
  int                       ignore_next;

  int                       stdctl_enable;

  struct {
    int                     caps; /* MIXER_CAP_x */
    int                     volume_level;
    int                     mute;
    int                     amp_level;
    int                     method;
  } mixer;

  struct {
    const char             *msg[4];
    int                     flags;
  } transform;

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

  void                     (*nongui_error_msg)(const char *text);

  FILE                      *orig_stdout; /* original stdout at startup        */
                                          /* before an evtl. later redirection */

  /* suspend event messages */
  struct {
    pthread_mutex_t          mutex;
    struct timeval           until;
    int                      level;
  } no_messages;

  xitk_window_t             *splash_win;
  xui_panel_t               *panel;
  xui_vwin_t                *vwin;
  xui_setup_t               *setup;
  xui_mrlb_t                *mrlb;
  xui_actrl_t               *actrl;
  xui_vctrl_t               *vctrl;
  xui_event_sender_t        *eventer;
  xui_skdloader_t           *skdloader;
  xui_help_t                *help;
  xui_viewlog_t             *viewlog;
  xui_keyedit_t             *keyedit;
  xui_playlist_t            *plwin;
  xui_mmkedit_t             *mmkedit;
  xui_sinfo_t               *streaminfo;
  xui_tvset_t               *tvset;
  xui_tvout_t               *tvout;
  xui_lirc_t                *lirc;
  filebrowser_t             *pl_load;
  filebrowser_t             *pl_save;

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

  struct {
    int                      (*get_names)(gGui_t *gui, const char **names, int max);
    xine_sarray_t           *avail;
    skins_locations_t       *default_skin;
    skins_locations_t       *current_skin;
    int                      num;
    int                      change_config_entry;
  } skins;

  struct {
    int                      fd;
    pthread_t                thread;
    FILE                    *fbk;
  } stdctl;
};

extern gGui_t *gGui;

void set_window_type_start(gGui_t *gui, xitk_window_t *xwin);
#define set_window_type_start(gui, xwin) \
  xitk_window_set_wm_window_type (xwin, video_window_is_visible ((gui)->vwin) < 2 ? WINDOW_TYPE_NORMAL : WINDOW_TYPE_NONE)

void set_window_states_start(gGui_t *gui, xitk_window_t *xwin);
#define set_window_states_start(gui, xwin)                                \
  do {                                                                    \
    set_window_type_start((gui), (xwin));                                 \
    xitk_window_set_window_class((xwin), NULL, "xine");                   \
    xitk_window_set_window_icon((xwin), (gui)->icon);                     \
  } while(0)

void reparent_window(gGui_t *gui, xitk_window_t *xwin);


void gui_load_window_pos (gGui_t *gui, const char *name, int *x, int *y);
void gui_save_window_pos (gGui_t *gui, const char *name, xitk_register_key_t key);

void gui_playlist_init (gGui_t *gui);
void gui_playlist_lock (gGui_t *gui);
#define gui_playlist_lock(_gui) pthread_mutex_lock (&(_gui)->mmk_mutex)
void gui_playlist_unlock (gGui_t *gui);
#define gui_playlist_unlock(_gui) pthread_mutex_unlock (&(_gui)->mmk_mutex)
void gui_playlist_deinit (gGui_t *gui);

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
