/*
 * Copyright (C) 2000-2004 the xine project
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

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "Imlib-light/Imlib.h"

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
#include "lirc.h"
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

#define PLAYLIST_CONTROL_STOP   0x00000001 /* Don't start next entry in playlist */
#define PLAYLIST_CONTROL_IGNORE 0x00000002 /* Ignore some playlist adding action */

#define SAFE_FREE(x)            do {           \
                                  if((x)) {    \
                                    free((x)); \
                                    x = NULL;  \
                                  }            \
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

typedef struct {
  xine_video_port_t        *vo_port;
  xine_video_port_t        *vo_none;
  xine_audio_port_t        *ao_none;

  tvout_t                  *tvout;

  post_element_t          **post_elements;
  int                       post_elements_num;
  int                       post_enable;

  char                     *deinterlace_plugin;
  post_element_t          **deinterlace_elements;
  int                       deinterlace_elements_num;
  int                       deinterlace_enable;

  struct {
    int                     hue;
    int                     brightness;
    int                     saturation;
    int                     contrast;
  } video_settings;

  xine_audio_port_t        *ao_port;

  xine_stream_t            *stream;

  struct {
    int                     pos;
    int                     time;
    int                     length;
  } stream_length;

  int                       subtitle_autoload;
  xine_stream_t            *spu_stream;

  xine_t                   *xine;
  int                       verbosity;
  int                       broadcast_port;

  pthread_mutex_t           xe_mutex;
  int                       new_pos;

  xine_event_queue_t       *event_queue;

  int                       smart_mode;

  /* Visual stuff (like animation in video window while audio only playback) */
  struct {
    xine_stream_t          *stream;
    xine_event_queue_t     *event_queue;
    int                     running;
    int                     current;
    int                     enabled; /* 0, 1:vpost, 2:vanim */
    
    char                  **mrls;
    int                     num_mrls;
    
    int                     post_plugin_num;
    xine_post_t            *post_output;
    int                     post_changed;
    
  } visual_anim;
  
  struct {
    int                     enabled;
    int                     use_unscaled;
    int                     unscaled_available;
    int                     timeout;

    xine_osd_t             *sinfo;
    int                     sinfo_visible;

    xine_osd_t             *bar[2];
    int                     bar_visible;

    xine_osd_t             *status;
    int                     status_visible;

    xine_osd_t             *info;
    int                     info_visible;

  } osd;

  /* xine lib/gui configuration filename */
  char                     *configfile;
  int                       experience_level;

  const char               *logo_mrl;
  int                       logo_mode;
  int                       logo_has_changed;
  int                       display_logo;

  /* stuff like ACTID_x */
  action_id_t               actions_on_start[16];
  char                     *autoscan_plugin;

  /* basic X11 stuff */
  Display                  *display;
  int                       screen;
  int		            depth;
  Visual	           *visual;
  XColor                    black;
  Pixmap                    icon;
  Colormap                  colormap;
  double                    pixel_aspect;

  int		            prefered_visual_class;
  VisualID	            prefered_visual_id;
  int		            install_colormap;

  xitk_skin_config_t       *skin_config;
  char                     *skin_server_url;

  ImlibData                *imlib_data;

  Window                    video_window; 
  int                       cursor_visible;
  int                       cursor_grabbed;

  Window                    panel_window;

  uint32_t                  debug_level;

  int                       is_display_mrl;

  int                       mrl_overrided;

  /* Current mediamark */
  mediamark_t               mmk;
  
  /* playlist */
  struct {
    mediamark_t           **mmk;
    
    int                     num;                   /* number of entries in playlist */
    int                     cur;                   /* current entry in playlist */
    int                     loop;                  /* current loop mode (see PLAYLIST_LOOP_* */
    int                     control;               /* see PLAYLIST_CONTROL_* */

    char                   *on_start;
  } playlist;


  int                       on_quit;
  int                       running;
  int                       ignore_next;

#ifdef HAVE_LIRC
  int                       lirc_enable;
#endif
  
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
  
  int                       use_root_window;

  const char               *snapshot_location;
  
  int                       ssaver_timeout;

  int                       skip_by_chapter;

  int                       auto_vo_visibility;
  int                       auto_panel_visibility;

  char                      *keymap_file;
  kbinding_t                *kbindings;
  int                        shortcut_style;

  struct {
    int                      set;
    int                      arg;
  } numeric;
  
  int                        eventer_sticky;
  int                        stream_info_auto_update;

  char                       curdir[XITK_PATH_MAX];

  int                        play_anyway;

  int                        got_reference_stream;

  int                        splash;

  pthread_mutex_t            download_mutex;

  FILE                      *report;

} gGui_t;

void event_listener(void *user_data, const xine_event_t *event);

void set_window_states_start(Window window);
#define set_window_states_start(window)                                   \
  do {                                                                    \
    if(!video_window_is_visible())                                        \
      xitk_set_wm_window_type((window), WINDOW_TYPE_NORMAL);              \
    else                                                                  \
      xitk_unset_wm_window_type((window), WINDOW_TYPE_NORMAL);            \
    change_class_name((window));                                          \
    change_icon((window));                                                \
  } while(0)

void reparent_window(Window window);
#define reparent_window(window)                                                          \
  do {                                                                                   \
    if((!(video_window_get_fullscreen_mode() & WINDOWED_MODE)) && !wm_not_ewmh_only()) { \
      XLockDisplay(gGui->display);                                                       \
      XUnmapWindow(gGui->display, (window));                                             \
      if(!video_window_is_visible())                                                     \
        xitk_set_wm_window_type((window), WINDOW_TYPE_NORMAL);                           \
      else                                                                               \
        xitk_unset_wm_window_type((window), WINDOW_TYPE_NORMAL);                         \
      XRaiseWindow(gGui->display, (window));                                             \
      XMapWindow(gGui->display, (window));                                               \
      try_to_set_input_focus((window));                                                  \
      if(!gGui->use_root_window)                                                         \
        XSetTransientForHint (gGui->display, (window), gGui->video_window);              \
      XUnlockDisplay(gGui->display);                                                     \
    }                                                                                    \
    else {                                                                               \
      XLockDisplay(gGui->display);                                                       \
      XRaiseWindow(gGui->display, window);                                               \
      XMapWindow(gGui->display, window);                                                 \
      if(!gGui->use_root_window)                                                         \
        XSetTransientForHint (gGui->display, window, gGui->video_window);                \
      XUnlockDisplay(gGui->display);                                                     \
      layer_above_video(window);                                                         \
    }                                                                                    \
  } while(0)

void change_class_name(Window window);
#define change_class_name(window)                                         \
  do {                                                                    \
    XClassHint  xclasshint;                                               \
    XLockDisplay (gGui->display);                                         \
    if((XGetClassHint(gGui->display, (window), &xclasshint)) != 0) {      \
      XClassHint   nxclasshint;                                           \
      nxclasshint.res_name = xclasshint.res_name;                         \
      nxclasshint.res_class = "xine";                                     \
      XSetClassHint(gGui->display, window, &nxclasshint);                 \
      XFree(xclasshint.res_name);                                         \
      XFree(xclasshint.res_class);                                        \
    }                                                                     \
    XUnlockDisplay (gGui->display);                                       \
  } while(0)

void change_icon(Window window);
#define change_icon(window)                                               \
  do {                                                                    \
    XWMHints *wmhints;                                                    \
    XLockDisplay(gGui->display);                                          \
    if((wmhints = XAllocWMHints())) {                                     \
      wmhints->icon_pixmap   = gGui->icon;                                \
      wmhints->flags         = IconPixmapHint;                            \
      XSetWMHints(gGui->display, (window), wmhints);                      \
      XFree(wmhints);                                                     \
    }                                                                     \
    XUnlockDisplay(gGui->display);                                        \
  } while(0)

#endif
