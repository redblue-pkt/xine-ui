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
 * gui initialization and top-level event handling stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xmd.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <zlib.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

/* input_pvr functionality needs this */
#define XINE_ENABLE_EXPERIMENTAL_FEATURES

#include "common.h"
#include "oxine/oxine.h"

/*
 * global variables
 */
static pid_t            xine_pid;

/* Icon data */
static const unsigned char icon_datas[] = {
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xff, 0xff, 0xff,
   0x8f, 0xf1, 0xff, 0xff, 0xff, 0x8f, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f,
   0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00,
   0x00, 0x88, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00,
   0x88, 0x11, 0x91, 0x79, 0xf8, 0x88, 0x1f, 0x9b, 0x99, 0x0c, 0xf8, 0x1f,
   0x8e, 0x99, 0x7d, 0xf8, 0x11, 0x8e, 0x99, 0x7d, 0x88, 0x11, 0x9b, 0x99,
   0x0d, 0x88, 0x1f, 0x91, 0x99, 0xf9, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x11, 0x00, 0x00, 0x00,
   0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0x1f,
   0x00, 0x00, 0x00, 0xf8, 0xf1, 0xff, 0xff, 0xff, 0x8f, 0xf1, 0xff, 0xff,
   0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0x11, 0x00, 0x00, 0x00, 0x88, 0x11, 0x00, 0x00, 0x00, 0x88, 0x1f, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0xf8
};

static const char *const exp_levels[] = {
  "Beginner",
  "Advanced",
  "Expert",
  "Master of the known universe",
#ifdef DEBUG
  "Debugger",
#else  
  NULL,
#endif
  NULL
};

static const char *const visual_anim_style[] = {
  "None",
  "Post Plugin",
  "Stream Animation",
  NULL
};

static const char *const mixer_control_method[] = {
  "Sound card",
  "Software",
  NULL
};

static const char *const shortcut_style[] = {
  "Windows style",
  "Emacs style",
  NULL
};

int hidden_file_cb(int action, int value) {
  xine_cfg_entry_t  cfg_entry;
  int               retval = 0;
  
  if(xine_config_lookup_entry (gGui->xine, "media.files.show_hidden_files", &cfg_entry)) {
    if(action)
      config_update_bool("media.files.show_hidden_files", value);
    else
      retval = cfg_entry.num_value;
  }

  return retval;
}

void dummy_config_cb(void *data, xine_cfg_entry_t *cfg) {
  /* It exist to avoid "restart" window message in setup window */
  (void)data;
  (void)cfg;
}
static void auto_vo_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->auto_vo_visibility = cfg->num_value;
}
static void auto_panel_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->auto_panel_visibility = cfg->num_value;
}
static void skip_by_chapter_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->skip_by_chapter = cfg->num_value;
  panel_update_nextprev_tips (gui->panel);
}
static void ssaver_timeout_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->ssaver_timeout = cfg->num_value;
}

static void visual_anim_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  
  if(gui->visual_anim.enabled == cfg->num_value)
    return;

  if((gui->visual_anim.enabled) && (cfg->num_value == 0) && gui->visual_anim.running)
    visual_anim_stop();
  
  if(gui->visual_anim.enabled && gui->visual_anim.running) {
    if((gui->visual_anim.enabled == 1) && (cfg->num_value != 1)) {
      if(post_rewire_audio_port_to_stream(gui->stream))
	gui->visual_anim.running = 0;
    }
    if((gui->visual_anim.enabled == 2) && (cfg->num_value != 2)) {
      visual_anim_stop();
      gui->visual_anim.running = 0;
    }
  }
  
  gui->visual_anim.enabled = cfg->num_value;
  
  if(gui->visual_anim.enabled) {
    int  has_video = xine_get_stream_info(gui->stream, XINE_STREAM_INFO_HAS_VIDEO);
    
    if(has_video)
      has_video = !xine_get_stream_info(gui->stream, XINE_STREAM_INFO_IGNORE_VIDEO);
    
    if(!has_video && !gui->visual_anim.running) {
      if(gui->visual_anim.enabled == 1) {
	if(gui->visual_anim.post_output_element.post) {
	  if(post_rewire_audio_post_to_stream(gui->stream))
	    gui->visual_anim.running = 1;
	}
      }
      else if(gui->visual_anim.enabled == 2) {
	visual_anim_play();
	gui->visual_anim.running = 1;
      }
    }
  }
}

static void stream_info_auto_update_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->stream_info_auto_update = cfg->num_value;
  stream_infos_toggle_auto_update();
}

/*
 * Layer above callbacks
 */
static void layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->layer_above = cfg->num_value;
}
static void always_layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->always_layer_above = cfg->num_value;
}

/*
 * Callback for snapshots saving location.
 */
static void snapshot_loc_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->snapshot_location = cfg->str_value;
}

/*
 * Callback for skin server
 */
static void skin_server_url_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->skin_server_url = cfg->str_value;
}

/*
 * OSD cbs
 */
static void osd_enabled_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->osd.enabled = cfg->num_value;
}
static void osd_use_unscaled_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->osd.use_unscaled = cfg->num_value;
}
static void osd_timeout_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->osd.timeout = cfg->num_value;
}

static void smart_mode_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->smart_mode = cfg->num_value;
}

static void play_anyway_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->play_anyway = cfg->num_value;
}

static void exp_level_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->experience_level = (cfg->num_value * 10);
}

static void audio_mixer_method_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->mixer.method = cfg->num_value;
  panel_update_mixer_display (gui->panel);
}

static void shortcut_style_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->shortcut_style = cfg->num_value;
}

int wm_not_ewmh_only(void) {
  int wm_type = xitk_get_wm_type();
  
  if((wm_type & WM_TYPE_GNOME_COMP) && !(wm_type & WM_TYPE_EWMH_COMP))
    return 0;
  else if(wm_type & WM_TYPE_EWMH_COMP)
    return 1;
  
  return 0;
}

int actions_on_start(action_id_t actions[], action_id_t a) {
  int i = 0, num = 0;
  while(actions[i] != ACTID_NOKEY) {
    if(actions[i] == a)
      num++;
    i++;
  }
  return num;
}

/*
 *
 */
static void dummy_sighandler(int dummy) {
  (void)dummy;
}

static void gui_signal_handler (int sig, void *data) {
  pid_t     cur_pid = getppid();

  (void)data;
  switch (sig) {

  case SIGHUP:
    if(cur_pid == xine_pid) {
      printf("SIGHUP received: re-read config file\n");
      xine_config_reset (gGui->xine);
      xine_config_load (gGui->xine, __xineui_global_config_file);
    }
    break;

  case SIGUSR1:
    if(cur_pid == xine_pid) {
      printf("SIGUSR1 received\n");
    }
    break;

  case SIGUSR2:
    if(cur_pid == xine_pid) {
      printf("SIGUSR2 received\n");
    }
    break;

  case SIGINT:
  case SIGTERM:
    if(cur_pid == xine_pid) {
      struct sigaction action;

      xine_config_save (gGui->xine, __xineui_global_config_file);
      
      action.sa_handler = dummy_sighandler;
      sigemptyset(&(action.sa_mask));
      action.sa_flags = 0;
      if(sigaction(SIGALRM, &action, NULL) != 0) {
	fprintf(stderr, "sigaction(SIGALRM) failed: %s\n", strerror(errno));
      }
      alarm(5);
      
      /* We should call xine_stop() here, but since the */
      /* xine functions are not signal-safe, we cant.   */

      /*xine_stop(gui->stream);*/
    }
    break;
  }

}


/*
 * convert pts value to string
 */
static const char *pts2str(int64_t pts) {
  static char  buf[40];
  int64_t      min;
  double       sec;
  int          ds;
  int          sign;

  if((sign = (pts < 0)) != 0)
    pts = -pts;

  min = pts / (90000 * 60);
  sec = (double)pts / 90000 - 60 * min;
  ds  = sec / 10;
  sec -= 10 * ds;

  snprintf(buf, sizeof(buf), "%s%02" PRIi64 ":%d%.2f (%" PRIi64 " pts)", sign ? "-" : "", min, ds, sec, pts);

  return buf;
}


/*
 *
 */
void gui_execute_action_id (gGui_t *gui, action_id_t action) {
  xine_event_t   xine_event;
  int narg = -1;
  char *sarg = NULL;

  if (gui->event_reject)
    return;

  pthread_mutex_lock (&gui->event_mutex);

  if (gui->event_reject) {
    pthread_mutex_unlock (&gui->event_mutex);
    return;
  }

  gui->event_pending++;
  
  if(action & ACTID_IS_INPUT_EVENT) {

    /* Note: In the following overflow checks, we must test against INT_MAX */
    /* carefully. Otherwise, the comparison term may overflow itself and    */
    /* detecting the overflow condition will fail (never true or true by    */
    /* chance, depending on expression and rearranging by the compiler).    */

    if((action >= ACTID_EVENT_NUMBER_0) && (action <= ACTID_EVENT_NUMBER_9)) {
      int n = action - ACTID_EVENT_NUMBER_0;

      if (!gui->numeric.set) {
        gui->numeric.set = 1;
        gui->numeric.arg = n;
      }
      else if (gui->numeric.arg <= ((INT_MAX - n) / 10)) {
        gui->numeric.arg *= 10;
        gui->numeric.arg += n;
      }
      else
        fprintf (stderr, "xine-ui: Input number overflow, using %d\n", gui->numeric.arg);

    } 
    else if(action == ACTID_EVENT_NUMBER_10_ADD) {

      if (!gui->numeric.set) {
        gui->numeric.set = 1;
        gui->numeric.arg = 10;
      }
      else if (gui->numeric.arg <= (INT_MAX - 10))
        gui->numeric.arg += 10;
      else
        fprintf (stderr, "xine-ui: Input number overflow, using %d\n", gui->numeric.arg);

    }
    else {
      gui->numeric.set = 0;
      gui->numeric.arg = 0;
    }

    pthread_mutex_unlock (&gui->event_mutex);

    /* check if osd menu like this event */
    if (oxine_action_event (action & ~ACTID_IS_INPUT_EVENT)) {
      pthread_mutex_lock (&gui->event_mutex);
      gui->event_pending--;
      if ((gui->event_pending <= 0) && gui->event_reject)
        pthread_cond_signal (&gui->event_safe);
      pthread_mutex_unlock (&gui->event_mutex);
      return;
    }
    
    /* events for advanced input plugins. */
    xine_event.type        = action & ~ACTID_IS_INPUT_EVENT;
    xine_event.data_length = 0;
    xine_event.data        = NULL;
    xine_event.stream      = gui->stream;
    gettimeofday(&xine_event.tv, NULL);
    
    xine_event_send(gui->stream, &xine_event);
    pthread_mutex_lock (&gui->event_mutex);
    gui->event_pending--;
    if ((gui->event_pending <= 0) && gui->event_reject)
      pthread_cond_signal (&gui->event_safe);
    pthread_mutex_unlock (&gui->event_mutex);
    return;
  }

  if (gui->numeric.set)
    narg = gui->numeric.arg;
  gui->numeric.set = 0;
  gui->numeric.arg = 0;

  if (gui->alphanum.set) {
    if ((action == ACTID_OSD_WTEXT) || (action == ACTID_PVR_SETNAME) || (action == ACTID_PLAYLIST_OPEN))
      sarg = strdup (gui->alphanum.arg ? gui->alphanum.arg : "");
  }
  gui->alphanum.set = 0;
  gui->alphanum.arg = "";

  pthread_mutex_unlock (&gui->event_mutex);

  switch(action) {
    
  case ACTID_WINDOWREDUCE:
    {
      int output_width = 0, output_height = 0;

      /* Limit size to a practical minimum.                         */
      /* Too small window is hard to view or to hit with the mouse. */

      video_window_get_output_size (gui->vwin, &output_width, &output_height);
      if(output_width > 50 && output_height > 50) {
	float	xmag, ymag;

        video_window_get_mag (gui->vwin, &xmag, &ymag);
        video_window_set_mag (gui->vwin, xmag * (1/1.2f), ymag * (1/1.2f));
      }
    }
    break;

  case ACTID_WINDOWENLARGE:
    {
      int output_width = 1, output_height = 1, window_width = 0, window_height = 0;

      /* Limit size to a practical maximum.                                        */
      /* Too large window can grab system resources up to a virtual dead lock.     */
      /* Compare output size to window size as a WM (like fvwm) can be configured  */
      /* to impose limits on window size which may not be reflected back to output */
      /* size (no further ConfigureNotify after size limit is exceeded),           */
      /* so we'll not end up in endless magnification.                             */

      video_window_get_output_size (gui->vwin, &output_width, &output_height);
      xitk_get_window_position(gui->video_display, gui->video_window,
			       NULL, NULL, &window_width, &window_height);
      if(output_width < 5000 && output_height < 5000 &&
	 output_width <= window_width && output_height <= window_height) {
	float	xmag, ymag;

        video_window_get_mag (gui->vwin, &xmag, &ymag);
        video_window_set_mag (gui->vwin, xmag * 1.2f, ymag * 1.2f);
      }
    }
    break;

  case ACTID_ZOOM_1_1:
  case ACTID_WINDOW100:
    if (video_window_set_mag (gui->vwin, 1.0f, 1.0f))
      osd_display_info(_("Zoom: 1:1"));
    break;

  case ACTID_WINDOW200:
    if (video_window_set_mag (gui->vwin, 2.0f, 2.0f))
      osd_display_info(_("Zoom: 200%%"));
    break;

  case ACTID_WINDOW50:
    if (video_window_set_mag (gui->vwin, 0.5f, 0.5f))
      osd_display_info(_("Zoom: 50%%"));
    break;

  case ACTID_SPU_NEXT:
    if (narg < 0)
      gui_change_spu_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_spu_channel (NULL, (void*)GUI_NEXT, narg);
    break;
    
  case ACTID_SPU_PRIOR:
    if (narg < 0)
      gui_change_spu_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_spu_channel (NULL, (void*)GUI_PREV, narg);
    break;
    
  case ACTID_CONTROLSHOW:
    gui_control_show (NULL, gui->vctrl);
    break;
    
  case ACTID_TOGGLE_WINOUT_VISIBLITY:
    gui_toggle_visibility (NULL, gui);
    break;
    
  case ACTID_TOGGLE_WINOUT_BORDER:
    gui_toggle_border (NULL, gui);
    break;

  case ACTID_AUDIOCHAN_NEXT:
    if (narg < 0)
      gui_change_audio_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_audio_channel (NULL, gui, narg);
    break;
    
  case ACTID_AUDIOCHAN_PRIOR:
    if (narg < 0)
      gui_change_audio_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_audio_channel (NULL, gui, narg);
    break;
    
  case ACTID_PAUSE:
    gui_pause (NULL, gui, 0);
    break;

  case ACTID_PLAYLIST:
    gui_playlist_show(NULL, NULL);
    break;
      
  case ACTID_TOGGLE_VISIBLITY:
    panel_toggle_visibility (NULL, gui->panel);
    break;

  case ACTID_TOGGLE_FULLSCREEN:
    if (narg >= 0) {
      int fullscreen = video_window_get_fullscreen_mode (gui->vwin) & FULLSCR_MODE;
      if ((narg && !fullscreen) || (!narg && fullscreen))
        gui_set_fullscreen_mode (NULL, gui);
    } else
      gui_set_fullscreen_mode (NULL, gui);
    break;

#ifdef HAVE_XINERAMA
  case ACTID_TOGGLE_XINERAMA_FULLSCREEN:
    gui_set_xinerama_fullscreen_mode();
    break;
#endif

  case ACTID_TOGGLE_ASPECT_RATIO:
    if (narg < 0)
      gui_toggle_aspect(-1);
    else
      gui_toggle_aspect (narg);
    break;

  case ACTID_STREAM_INFOS:
    gui_stream_infos_show(NULL, NULL);
    break;

  case ACTID_TOGGLE_INTERLEAVE:
    gui_toggle_interlaced();
    break;

  case ACTID_QUIT:
    pthread_mutex_lock (&gui->event_mutex);
    gui->event_pending--;
    if ((gui->event_pending <= 0) && gui->event_reject)
      pthread_cond_signal (&gui->event_safe);
    pthread_mutex_unlock (&gui->event_mutex);
    gui_exit (NULL, gui);
    break;

  case ACTID_PLAY:
    gui_play (NULL, gui);
    break;

  case ACTID_STOP:
    gui_stop (NULL, gui);
    break;

  case ACTID_CLOSE:
    gui_close (NULL, gui);
    break;

  case ACTID_EVENT_SENDER:
    gui_event_sender_show(NULL, NULL);
    break;
    
  case ACTID_MRL_NEXT:
    if (narg < 0)
      gui_nextprev(NULL, (void*)GUI_NEXT);
    else
      gui_direct_nextprev (NULL, (void*)GUI_NEXT, narg);
    break;
    
  case ACTID_MRL_PRIOR:
    if (narg < 0)
      gui_nextprev(NULL, (void*)GUI_PREV);
    else
      gui_direct_nextprev (NULL, (void*)GUI_PREV, narg);
    break;

  case ACTID_MRL_SELECT:
    if (narg < 0)
      gui_playlist_play (gui, 0);
    else
      gui_playlist_play (gui, narg);
    break;
      
  case ACTID_SETUP:
    gui_setup_show(NULL, NULL);
    break;

  case ACTID_EJECT:
    gui_eject (NULL, gui);
    break;

  case ACTID_SET_CURPOS:
    if (narg >= 0) {
      /* Number is a percentage, range [0..100] */
      if (narg > 100)
        narg = 100;
      gui_set_current_position ((65535 * narg) / 100);
    }
    break;

  case ACTID_SET_CURPOS_0:
    gui_set_current_position (0);
    break;

  case ACTID_SET_CURPOS_10:
    gui_set_current_position (6553);
    break;

  case ACTID_SET_CURPOS_20:
    gui_set_current_position (13107);
    break;

  case ACTID_SET_CURPOS_30:
    gui_set_current_position (19660);
    break;

  case ACTID_SET_CURPOS_40:
    gui_set_current_position (26214);
    break;

  case ACTID_SET_CURPOS_50:
    gui_set_current_position (32767);
    break;
    
  case ACTID_SET_CURPOS_60:
    gui_set_current_position (39321);
    break;

  case ACTID_SET_CURPOS_70:
    gui_set_current_position (45874);
    break;
    
  case ACTID_SET_CURPOS_80:
    gui_set_current_position (52428);
    break;
    
  case ACTID_SET_CURPOS_90:
    gui_set_current_position (58981);
    break;

  case ACTID_SET_CURPOS_100:
    gui_set_current_position (65535);
    break;

  case ACTID_SEEK_REL_m:
    if (narg >= 0) {
      gui_seek_relative (-narg);
    }
    break;

  case ACTID_SEEK_REL_p:
    if (narg >= 0)
      gui_seek_relative (narg);
    break;

  case ACTID_SEEK_REL_m60:
    gui_seek_relative (-60);
    break;

  case ACTID_SEEK_REL_m15:
    gui_seek_relative (-15);
    break;

  case ACTID_SEEK_REL_p60:
    gui_seek_relative (60);
    break;
    
  case ACTID_SEEK_REL_p15:
    gui_seek_relative (15);
    break;
    
  case ACTID_SEEK_REL_m30:
    gui_seek_relative (-30);
    break;
    
  case ACTID_SEEK_REL_m7:
    gui_seek_relative (-7);
    break;
    
  case ACTID_SEEK_REL_p30:
    gui_seek_relative (30);
    break;
    
  case ACTID_SEEK_REL_p7:
    gui_seek_relative (7);
    break;

  case ACTID_MRLBROWSER:
    gui_mrlbrowser_show(NULL, NULL);
    break;
    
  case ACTID_MUTE:
    panel_toggle_audio_mute (NULL, gui->panel, !gui->mixer.mute);
    break;
    
  case ACTID_AV_SYNC_m3600:
    {
      int av_offset = (xine_get_param(gui->stream, XINE_PARAM_AV_OFFSET) - 3600);

      pthread_mutex_lock (&gui->event_mutex);
      gui->mmk.av_offset = av_offset;
      if (gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
	  gui->playlist.mmk[gui->playlist.cur])
	if (gui->playlist.cur < gui->playlist.num)
	  gui->playlist.mmk[gui->playlist.cur]->av_offset = av_offset;
      pthread_mutex_unlock (&gui->event_mutex);
      xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, av_offset);
      osd_display_info(_("A/V offset: %s"), pts2str(av_offset));
    }
    break;
    
  case ACTID_AV_SYNC_p3600:
    {
      int av_offset = (xine_get_param(gui->stream, XINE_PARAM_AV_OFFSET) + 3600);
      
      pthread_mutex_lock (&gui->event_mutex);
      gui->mmk.av_offset = av_offset;
      if (gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
	  gui->playlist.mmk[gui->playlist.cur])
	if (gui->playlist.cur < gui->playlist.num)
	  gui->playlist.mmk[gui->playlist.cur]->av_offset = av_offset;
      pthread_mutex_unlock (&gui->event_mutex);
      xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, av_offset);
      osd_display_info(_("A/V offset: %s"), pts2str(av_offset));
    }
    break;

  case ACTID_AV_SYNC_RESET:
    pthread_mutex_lock (&gui->event_mutex);
    gui->mmk.av_offset = 0;
    if (gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
	gui->playlist.mmk[gui->playlist.cur])
      if (gui->playlist.cur < gui->playlist.num)
	gui->playlist.mmk[gui->playlist.cur]->av_offset = 0;
    pthread_mutex_unlock (&gui->event_mutex);
    xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, 0);
    osd_display_info(_("A/V Offset: reset."));
    break;

  case ACTID_SV_SYNC_p:
    {
      int spu_offset = xine_get_param(gui->stream, XINE_PARAM_SPU_OFFSET) + 3600;
      
      xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
      
      osd_display_info(_("SPU Offset: %s"), pts2str(spu_offset));
    }
    break;

  case ACTID_SV_SYNC_m:
    {
      int spu_offset = xine_get_param(gui->stream, XINE_PARAM_SPU_OFFSET) - 3600;
      
      xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
      
      osd_display_info(_("SPU Offset: %s"), pts2str(spu_offset));
    }
    break;
    
  case ACTID_SV_SYNC_RESET:
    xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, 0);
    osd_display_info(_("SPU Offset: reset."));
    break;

  case ACTID_SPEED_FAST:
    gui_change_speed_playback(NULL, (void*)GUI_PREV);
    break;
    
  case ACTID_SPEED_SLOW:
    gui_change_speed_playback(NULL, (void*)GUI_NEXT);
    break;

  case ACTID_SPEED_RESET:
    gui_change_speed_playback(NULL, (void*)GUI_RESET);
    break;

  case ACTID_pVOLUME:
    gui_increase_audio_volume();
    break;

  case ACTID_mVOLUME:
    gui_decrease_audio_volume();
    break;

  case ACTID_APP:
    gui_app_show(NULL, NULL);
    break;

  case ACTID_APP_ENABLE:
    gui_app_enable();
    break;

  case ACTID_pAMP:
    gui_increase_amp_level();
    break;

  case ACTID_mAMP:
    gui_decrease_amp_level();
    break;

  case ACTID_AMP_RESET:
    gui_reset_amp_level();
    break;

  case ACTID_SNAPSHOT:
    panel_snapshot (NULL, gui->panel);
    break;
    
  case ACTID_ZOOM_IN:
    gui_change_zoom(1,1);
    break;

  case ACTID_ZOOM_OUT:
    gui_change_zoom(-1,-1);
    break;

  case ACTID_ZOOM_X_IN:
    gui_change_zoom(1,0);
    break;
  
  case ACTID_ZOOM_X_OUT:
    gui_change_zoom(-1,0);
    break;

  case ACTID_ZOOM_Y_IN:
    gui_change_zoom(0,1);
    break;
  
  case ACTID_ZOOM_Y_OUT:
    gui_change_zoom(0,-1);
    break;
  
  case ACTID_ZOOM_RESET:
    gui_reset_zoom();
    break;
    
  case ACTID_GRAB_POINTER:
    if(!gui->cursor_grabbed) {
      if (!panel_is_visible (gui->panel)) {
	gui->x_lock_display (gui->video_display);
	XGrabPointer(gui->video_display, gui->video_window, 1, None, 
		     GrabModeAsync, GrabModeAsync, gui->video_window, None, CurrentTime);
	gui->x_unlock_display (gui->video_display);
      }
      
      gui->cursor_grabbed = 1;
    }
    else {
      gui->x_lock_display (gui->display);
      XUngrabPointer(gui->display, CurrentTime);
      gui->x_unlock_display (gui->display);
      gui->cursor_grabbed = 0;
    }
    break;
    
  case ACTID_TOGGLE_TVMODE:
    gui_toggle_tvmode();
    break;

  case ACTID_TVANALOG:
    gui_tvset_show(NULL, NULL);
    break;

  case ACTID_VIEWLOG:
    gui_viewlog_show(NULL, NULL);
    break;

  case ACTID_KBEDIT:
    gui_kbedit_show(NULL, NULL);
    break;

  case ACTID_KBENABLE:
    if (narg >= 0)
      gui->kbindings_enabled = narg;
    else
      gui->kbindings_enabled = 1;
    break;

  case ACTID_DPMSSTANDBY:
    xine_system(0, "xset dpms force standby");
    break;

  case ACTID_MRLIDENTTOGGLE:
    gui->is_display_mrl = !gui->is_display_mrl;
    panel_update_mrl_display (gui->panel);
    playlist_mrlident_toggle();
    break;
    
  case ACTID_SCANPLAYLIST:
    playlist_scan_for_infos();
    break;

  case ACTID_MMKEDITOR:
    playlist_mmk_editor();
    break;

  case ACTID_SUBSELECT:
    gui_select_sub();
    break;

  case ACTID_LOOPMODE:
    if (narg < 0)
      gui->playlist.loop++;
    else
      gui->playlist.loop = narg;
    if(gui->playlist.loop >= PLAYLIST_LOOP_MODES_NUM ||
       gui->playlist.loop <  PLAYLIST_LOOP_NO_LOOP)
      gui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
    
    switch(gui->playlist.loop) {
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
    break;
    
  case ACTID_ADDMEDIAMARK:
    gui_add_mediamark();
    break;

#ifdef HAVE_TAR
  case ACTID_SKINDOWNLOAD:
    download_skin(gui->skin_server_url);
    break;
#endif

  case ACTID_OSD_SINFOS:
    osd_stream_infos();
    break;

  case ACTID_OSD_WTEXT:
    if (sarg) {
      osd_display_info ("%s", sarg);
    } else {
      osd_display_info(_("No text to display!"));
    }
    break;

  case ACTID_OSD_MENU:
    oxine_menu();
    break;

  case ACTID_FILESELECTOR:
    gui_file_selector();
    break;

  case ACTID_HUECONTROLp:
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_HUE);
    break;
  case ACTID_HUECONTROLm:
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_HUE);
    break;

  case ACTID_SATURATIONCONTROLp:
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_SATURATION);
    break;
  case ACTID_SATURATIONCONTROLm:
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_SATURATION);
    break;

  case ACTID_BRIGHTNESSCONTROLp:
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_BRIGHTNESS);
    break;
  case ACTID_BRIGHTNESSCONTROLm:
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_BRIGHTNESS);
    break;

  case ACTID_CONTRASTCONTROLp:
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_CONTRAST);
    break;
  case ACTID_CONTRASTCONTROLm:
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_CONTRAST);
    break;

  case ACTID_GAMMACONTROLp:
#ifdef XINE_PARAM_VO_GAMMA
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_GAMMA);
#endif
    break;
  case ACTID_GAMMACONTROLm:
#ifdef XINE_PARAM_VO_GAMMA
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_GAMMA);
#endif
    break;

  case ACTID_SHARPNESSCONTROLp:
#ifdef XINE_PARAM_VO_SHARPNESS
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_SHARPNESS);
#endif
    break;
  case ACTID_SHARPNESSCONTROLm:
#ifdef XINE_PARAM_VO_SHARPNESS
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_SHARPNESS);
#endif
    break;

  case ACTID_NOISEREDUCTIONCONTROLp:
#ifdef XINE_PARAM_VO_NOISE_REDUCTION
    control_inc_image_prop (gui->vctrl, XINE_PARAM_VO_NOISE_REDUCTION);
#endif
    break;
  case ACTID_NOISEREDUCTIONCONTROLm:
#ifdef XINE_PARAM_VO_NOISE_REDUCTION
    control_dec_image_prop (gui->vctrl, XINE_PARAM_VO_NOISE_REDUCTION);
#endif
    break;

  case ACTID_VPP:
    gui_vpp_show(NULL, NULL);
    break;

  case ACTID_VPP_ENABLE:
    gui_vpp_enable();
    break;

  case ACTID_HELP_SHOW:
    gui_help_show(NULL, NULL);
    break;

  case ACTID_PLAYLIST_STOP:
    if (narg < 0) {
      gui->playlist.control ^= PLAYLIST_CONTROL_STOP;
      gui->playlist.control &= ~PLAYLIST_CONTROL_STOP_PERSIST;
    }
    else {
      if (narg == 0)
	gui->playlist.control &= ~PLAYLIST_CONTROL_STOP;
      else
	gui->playlist.control |= PLAYLIST_CONTROL_STOP;
      gui->playlist.control |= PLAYLIST_CONTROL_STOP_PERSIST;
    }
    osd_display_info(_("Playlist: %s"), 
		     (gui->playlist.control & PLAYLIST_CONTROL_STOP) ?
		     (gui->playlist.control & PLAYLIST_CONTROL_STOP_PERSIST) ?
		     _("Stop (persistent)") :
		     _("Stop") :
		     _("Continue"));
    break;

  case ACTID_PVR_SETINPUT:

    /* Parameter (integer, required): input
    */
    if (narg >= 0) {
      xine_event_t         xine_event;
      xine_set_v4l2_data_t ev_data;

      /* set input */
      ev_data.input = narg;

      /* do not change tuning */
      ev_data.channel = -1;
      ev_data.frequency = -1;

      /* do not set session id */
      ev_data.session_id = -1;

      /* send event */
      xine_event.type = XINE_EVENT_SET_V4L2;
      xine_event.data_length = sizeof(xine_set_v4l2_data_t);
      xine_event.data = &ev_data;
      xine_event.stream = gui->stream;
      xine_event_send(gui->stream, &xine_event);
    }
    break;

  case ACTID_PVR_SETFREQUENCY:

    /* Parameter (integer, required): frequency
    */
    if (narg >= 0) {
      xine_event_t         xine_event;
      xine_set_v4l2_data_t ev_data;

      /* do not change input */
      ev_data.input = -1;

      /* change tuning */
      ev_data.channel = -1;
      ev_data.frequency = narg;

      /* do not set session id */
      ev_data.session_id = -1;

      /* send event */
      xine_event.type = XINE_EVENT_SET_V4L2;
      xine_event.data_length = sizeof(xine_set_v4l2_data_t);
      xine_event.data = &ev_data;
      xine_event.stream = gui->stream;
      xine_event_send(gui->stream, &xine_event);
    }
    break;

  case ACTID_PVR_SETMARK:

    /* Parameter (integer, required): session_id

       Mark the start of a section in the pvr stream for later saving
       The 'id' can be used to set different marks in the stream. If
       an existing mark is provided, then the mark for that section
       is moved to the current position.
    */
    if (narg >= 0) {
      xine_event_t         xine_event;
      xine_set_v4l2_data_t ev_data;

      /* do not change input */
      ev_data.input = -1;

      /* do not change tuning */
      ev_data.channel = -1;
      ev_data.frequency = -1;

      /* set session id */
      ev_data.session_id = narg;

      /* send event */
      xine_event.type = XINE_EVENT_SET_V4L2;
      xine_event.data_length = sizeof(xine_set_v4l2_data_t);
      xine_event.data = &ev_data;
      xine_event.stream = gui->stream;
      xine_event_send(gui->stream, &xine_event);
    }
    break;

  case ACTID_PVR_SETNAME:

    /* Parameter (string, required): name

       Set the name of the current section in the pvr stream. The name
       is used when the section is saved permanently.
    */
    if (sarg) {
      xine_event_t         xine_event;
      xine_pvr_save_data_t ev_data;

      /* only set name */
      ev_data.mode = -1;
      ev_data.id = -1;

      /* name of the show, max 255 is hardcoded in xine.h */
      strlcpy (ev_data.name, sarg, sizeof(ev_data.name));

      /* send event */
      xine_event.type = XINE_EVENT_PVR_SAVE;
      xine_event.data = &ev_data;
      xine_event.data_length = sizeof(xine_pvr_save_data_t);
      xine_event.stream = gui->stream;
      xine_event_send(gui->stream, &xine_event);
    }
    break;

  case ACTID_PVR_SAVE:

    /* Parameter (integer, required): mode

       Save the pvr stream with mode:
         mode = 0 : save from now on
         mode = 1 : save from last mark
         mode = 2 : save entire stream
    */
    if (narg >= 0) {
      xine_event_t         xine_event;
      xine_pvr_save_data_t ev_data;
      int                  mode = narg;

      /* set mode [0..2] */
      if(mode < 0)
	mode = 0;
      if(mode > 2)
	mode = 2;
      ev_data.mode = mode;

      /* do not set id and name */
      ev_data.id = 0;
      ev_data.name[0] = '\0';

      /* send event */
      xine_event.type = XINE_EVENT_PVR_SAVE;
      xine_event.data = &ev_data;
      xine_event.data_length = sizeof(xine_pvr_save_data_t);
      xine_event.stream = gui->stream;
      xine_event_send(gui->stream, &xine_event);
    }
    break;

  case ACTID_PLAYLIST_OPEN:
    if (sarg) {
        mediamark_load_mediamarks (sarg);
        gui_set_current_mmk(mediamark_get_current_mmk());
        playlist_update_playlist();
        if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
          enable_playback_controls (gui->panel, 1);
    }
    break;

  default:
    break;
  }

  free (sarg);
  pthread_mutex_lock (&gui->event_mutex);
  gui->event_pending--;
  if ((gui->event_pending <= 0) && gui->event_reject)
    pthread_cond_signal (&gui->event_safe);
  pthread_mutex_unlock (&gui->event_mutex);
}

/*
 * top-level event handler
 */
void gui_handle_event (XEvent *event, void *data) {
  gGui_t *gui = data;

  switch(event->type) {

  case MappingNotify:
    gui->x_lock_display (gui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    gui->x_unlock_display (gui->display);
    break;

  case DestroyNotify:
    if(event->xany.window == gui->panel_window
       || event->xany.window == gui->video_window) {
      xine_exit (gui->xine);
      gui->running = 0;
    }
    break;
    
  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    /* printf ("ButtonPress\n"); */

    /* printf ("button: %d\n",bevent->button); */

    if ((bevent->button == 3) && (bevent->window == gui->video_window))
      panel_toggle_visibility (NULL, gui->panel);
    
  }
  break;

  case KeyPress:
    if(gui->stdctl_enable)
      stdctl_keypress(event->xkey);
    /* no break */
  case ButtonRelease:
    kbindings_handle_kbinding(gui->kbindings, event);
    break;
    
  case ConfigureNotify:
    break;

    /*
      default:
      printf("Got event: %i\n", event->type);
    */
  }

}

/*
 * Start playback of an entry in playlist
 */
int gui_playlist_play (gGui_t *gui, int idx) {
  int ret = 1;

  osd_hide();
  panel_reset_slider (gui->panel);

  if(idx >= gui->playlist.num)
    return 0;
  gui_set_current_mmk_by_index(idx);

  pthread_mutex_lock(&gui->mmk_mutex);
  if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
			     gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset,
			     !mediamark_have_alternates(&(gui->mmk))) &&
     (!mediamark_have_alternates(&(gui->mmk)) ||
      !gui_open_and_play_alternates(&(gui->mmk), gui->mmk.sub)))
    ret = 0;
  pthread_mutex_unlock(&gui->mmk_mutex);
  return ret;
}

/*
 * Start playback to next entry in playlist (or stop the engine, then display logo).
 */
void gui_playlist_start_next (gGui_t *gui) {

  if (gui->ignore_next)
    return;

  osd_hide();
  panel_reset_slider (gui->panel);

  if(gui->playlist.control & PLAYLIST_CONTROL_STOP) {
    if(!(gui->playlist.control & PLAYLIST_CONTROL_STOP_PERSIST))
      gui->playlist.control &= ~PLAYLIST_CONTROL_STOP;
    gui_display_logo();
    return;
  }

  if (is_playback_widgets_enabled (gui->panel) && (!gui->playlist.num)) {
    gui_set_current_mmk(NULL);
    enable_playback_controls (gui->panel, 0);
    gui_display_logo();
    return;
  }
  
  switch(gui->playlist.loop) {

  case PLAYLIST_LOOP_NO_LOOP:
  case PLAYLIST_LOOP_LOOP:
    gui->playlist.cur++;
    
    if(gui->playlist.cur >= gui->playlist.num) {
      if(gui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) {
	gui->playlist.cur--;
	mediamark_reset_played_state();

	if(gui->actions_on_start[0] == ACTID_QUIT)
          gui_exit (NULL, gui);
	else
	  gui_display_logo();
	return;
      }
      else if(gui->playlist.loop == PLAYLIST_LOOP_LOOP) {
	gui->playlist.cur = 0;
      }
    }
    break;
    
  case PLAYLIST_LOOP_REPEAT:
    break;

  case PLAYLIST_LOOP_SHUFFLE:
  case PLAYLIST_LOOP_SHUF_PLUS:
    if(!mediamark_all_played()) {
      
    __shuffle_restart:
      gui->playlist.cur = mediamark_get_shuffle_next();
    }
    else {
      mediamark_reset_played_state();

      if(gui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS)
	goto __shuffle_restart;
      else if(gui->actions_on_start[0] == ACTID_QUIT)
        gui_exit (NULL, gui);
      else
	gui_display_logo();
      return;    
    }
    break;
  }

  if (gui_playlist_play (gui, gui->playlist.cur))
    return;

  switch(gui->playlist.loop) {

  case PLAYLIST_LOOP_NO_LOOP:
    if(mediamark_all_played() && (gui->actions_on_start[0] == ACTID_QUIT)) {
      gui_exit (NULL, gui);
      return;
    }
    break;

  case PLAYLIST_LOOP_LOOP:
  case PLAYLIST_LOOP_REPEAT:
    break;
    
  case PLAYLIST_LOOP_SHUFFLE:
  case PLAYLIST_LOOP_SHUF_PLUS:
    if(!mediamark_all_played())
      goto __shuffle_restart;
    break;
  }
  gui_display_logo();
}

static void gui_find_visual (Visual **visual_return, int *depth_return) {
  gGui_t *gui = gGui;
  XWindowAttributes  attribs;
  XVisualInfo	    *vinfo;
  XVisualInfo	     vinfo_tmpl;
  int		     num_visuals;
  int		     depth = 0;
  Visual	    *visual = NULL;

  if (gui->prefered_visual_id == None) {
    /*
     * List all available TrueColor visuals, pick the best one for xine.
     * We prefer visuals of depth 15/16 (fast).  Depth 24/32 may be OK, 
     * but could be slow.
     */
    vinfo_tmpl.screen = gui->screen;
    vinfo_tmpl.class  = (gui->prefered_visual_class != -1
			 ? gui->prefered_visual_class : TrueColor);
    vinfo = XGetVisualInfo(gui->display,
			   VisualScreenMask | VisualClassMask,
			   &vinfo_tmpl, &num_visuals);
    if (vinfo != NULL) {
      int i, pref;
      int best_visual_index = -1;
      int best_visual = -1;

      for (i = 0; i < num_visuals; i++) {
	if (vinfo[i].depth == 15 || vinfo[i].depth == 16)
	  pref = 3;
	else if (vinfo[i].depth > 16)
	  pref = 2;
	else
	  pref = 1;
	
	if (pref > best_visual) {
	  best_visual = pref;
	  best_visual_index = i;
	}  
      }
      
      if (best_visual_index != -1) {
	depth = vinfo[best_visual_index].depth;
	visual = vinfo[best_visual_index].visual;
      }
      
      XFree(vinfo);
    }
  } else {
    /*
     * Use the visual specified by the user.
     */
    vinfo_tmpl.visualid = gui->prefered_visual_id;
    vinfo = XGetVisualInfo(gui->display,
			   VisualIDMask, &vinfo_tmpl, 
			   &num_visuals);
    if (vinfo == NULL) {
      printf(_("gui_main: selected visual %#lx does not exist, trying default visual\n"),
	     (long) gui->prefered_visual_id);
    } else {
      depth = vinfo[0].depth;
      visual = vinfo[0].visual;
      XFree(vinfo);
    }
  }

  if (depth == 0) {
    XVisualInfo vinfo;

    XGetWindowAttributes(gui->display, (DefaultRootWindow(gui->display)), &attribs);

    depth = attribs.depth;
  
    if (XMatchVisualInfo(gui->display, gui->screen, depth, TrueColor, &vinfo)) {
      visual = vinfo.visual;
    } else {
      printf (_("gui_main: couldn't find true color visual.\n"));

      depth = DefaultDepth (gui->display, gui->screen);
      visual = DefaultVisual (gui->display, gui->screen); 
    }
  }

  if (depth_return != NULL)
    *depth_return = depth;
  if (visual_return != NULL)
    *visual_return = visual;
}

void gui_deinit (gGui_t *gui) {
#ifdef HAVE_XINE_CONFIG_UNREGISTER_CALLBACKS
  xine_config_unregister_callbacks (gui->xine, NULL, NULL, gui, sizeof (*gui));
#endif
  pthread_mutex_lock (&gui->event_mutex);
  gui->event_reject = 1;
  while (gui->event_pending > 0)
    pthread_cond_wait (&gui->event_safe, &gui->event_mutex);
  pthread_mutex_unlock (&gui->event_mutex);

  xitk_unregister_event_handler(&gui->widget_key);
}

/*
 * Initialize the GUI
 */

static void gui_dummy_un_lock_display (Display *display) {
  (void)display;
}

void gui_init (gGui_t *gui, int nfiles, char *filenames[], window_attributes_t *window_attribute) {
  int    i;
  char  *server;
  char  *video_display_name;
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&gui->event_mutex, &attr);
  pthread_cond_init (&gui->event_safe, NULL);

  gui->event_reject = 1;
  gui->event_pending = 0;

  /*
   * init playlist
   */
  for (i = 0; i < nfiles; i++) {
    char *file = filenames[i];

    /* grab recursively all files from dir */
    if(is_a_dir(file)) {
      
      if(file[strlen(file) - 1] == '/')
	file[strlen(file) - 1] = '\0'; 

      mediamark_collect_from_directory(file);
    }
    else {

      if(mrl_look_like_playlist(file))
	(void) mediamark_concat_mediamarks(file);
      else {
	char *sub = NULL;

	if((sub = (char *)get_last_double_semicolon(file)) != NULL) {
	  if(is_ipv6_last_double_semicolon(file))
	    sub = NULL;
	  else {
	    *sub = 0;
	    sub += 2;
	  }
	}
	
	mediamark_append_entry((const char *)file, (const char *)file, sub, 0, -1, 0, 0);
      }
    }
  }

  gui->playlist.cur = gui->playlist.num ? 0 : -1;
  
  if((gui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) || 
     (gui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS))
    gui->playlist.cur = mediamark_get_shuffle_next();
  
  gui->is_display_mrl = 0;
  gui->mrl_overrided  = 0;
  /* gui->new_pos        = -1; */

  /*
   * X / imlib stuff
   */

  if (xine_config_register_bool (gui->xine, "gui.use_XLockDisplay", 1,
        _("Enable extra XLib thread safety."),
        _("This is needed for some very old XLib/XCB versions.\n"
          "Otherwise, it may boost or brake performance - just try out."),
        CONFIG_LEVEL_ADV, NULL, NULL)) {
    gui->x_lock_display = XLockDisplay;
    gui->x_unlock_display = XUnlockDisplay;
  } else {
    gui->x_lock_display =
    gui->x_unlock_display = gui_dummy_un_lock_display;
  }

  if (!XInitThreads ()) {
    printf (_("\nXInitThreads failed - looks like you don't have a thread-safe xlib.\n"));
    exit(1);
  }
  
  if((gui->display = XOpenDisplay((getenv("DISPLAY")))) == NULL) {
    fprintf(stderr, _("Cannot open display\n"));
    exit(1);
  }

  video_display_name = 
    (char *)xine_config_register_string (gui->xine, "gui.video_display", 
					 "",
					 _("Name of video display"),
					 _("Use this setting to configure to which "
                                           "display xine will show videos. When left blank, "
                                           "the main display will be used. Setting this "                  "option to something like ':0.1' or ':1' makes "
                                           "possible to display video and control on different "
                                           "screens (very useful for TV presentations)."),
					 CONFIG_LEVEL_ADV,
					 NULL,
					 CONFIG_NO_DATA);
  
  gui->video_display = NULL;
  if ( strlen(video_display_name) ) {
  
    if ((gui->video_display = XOpenDisplay(video_display_name)) == NULL )
      fprintf(stderr, _("Cannot open display '%s' for video. Falling back to primary display.\n"),
              video_display_name);
  }
  
  if ( gui->video_display == NULL ) {
    gui->video_display = gui->display;
  }

  if (xine_config_register_bool (gui->xine, "gui.xsynchronize", 
				 0,
				 _("Synchronized X protocol (debug)"), 
				 _("Do X transactions in synchronous mode. "
				   "Very slow, use only for debugging!"), 
				 CONFIG_LEVEL_ADV,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA)) {

    gui->x_lock_display (gui->display);
    XSynchronize (gui->display, True);
    gui->x_unlock_display (gui->display);
    fprintf (stderr, _("Warning! Synchronized X activated - this is very slow...\n"));
  }

  gui->layer_above = 
    xine_config_register_bool (gui->xine, "gui.layer_above", 0,
			       _("Windows stacking"),
			       _("Use WM layer property to place windows on top\n"
				 "except for the video window in non-fullscreen mode."), 
			       CONFIG_LEVEL_ADV,
			       layer_above_cb,
			       gGui);

  gui->always_layer_above = 
    xine_config_register_bool (gui->xine, "gui.always_layer_above", 0,
			       _("Windows stacking (more)"),
			       _("Use WM layer property to place windows on top\n"
				 "for all windows (surpasses the 'layer_above' setting)."), 
			       CONFIG_LEVEL_ADV,
			       always_layer_above_cb,
			       gGui);

  gui->snapshot_location = 
    (char *)xine_config_register_string (gui->xine, "gui.snapshotdir", 
					 (char *) (xine_get_homedir()),
					 _("Snapshot location"),
					 _("Where snapshots will be saved."),
					 CONFIG_LEVEL_BEG,
					 snapshot_loc_cb,
					 gGui);
  
  gui->ssaver_timeout =
    xine_config_register_num (gui->xine, "gui.screensaver_timeout", 10,
			      _("Screensaver reset interval (s)"),
			      _("Time, in seconds, between two faked events to keep a screensaver quiet, 0 to disable."),
			      CONFIG_LEVEL_ADV,
			      ssaver_timeout_cb,
			      gGui);
  
  gui->skip_by_chapter = 
    xine_config_register_bool (gui->xine, "gui.skip_by_chapter", 1,
			       _("Chapter hopping"),
			       _("Play next|previous chapter instead of mrl (dvdnav)"), 
			       CONFIG_LEVEL_ADV,
			       skip_by_chapter_cb, 
			       gGui);
  
  gui->auto_vo_visibility = 
    xine_config_register_bool (gui->xine, "gui.auto_video_output_visibility", 0,
			       _("Visibility behavior of output window"),
			       _("Hide video output window if there is no video in the stream"), 
			       CONFIG_LEVEL_ADV,
			       auto_vo_visibility_cb, 
			       gGui);

  gui->auto_panel_visibility = 
    xine_config_register_bool (gui->xine, "gui.auto_panel_visibility", 0,
			       _("Visiblility behavior of panel"),
			       _("Automatically show/hide panel window, according to auto_video_output_visibility"), 
			       CONFIG_LEVEL_ADV,
			       auto_panel_visibility_cb,
			       gGui); 
 
  gui->eventer_sticky = 
    xine_config_register_bool (gui->xine, "gui.eventer_sticky", 
			      1,
			      _("Event sender behavior"),
			      _("Event sender window stick to main panel"), 
			      CONFIG_LEVEL_ADV,
			      event_sender_sticky_cb,
			      gGui);

  gui->visual_anim.enabled = 
    xine_config_register_enum (gui->xine, "gui.visual_anim", 
			      1, /* Post plugin */
			      (char**)visual_anim_style,
			      _("Visual animation style"),
			      _("Display some video animations when "
				"current stream is audio only (eg: mp3)."), 
			      CONFIG_LEVEL_BEG,
			      visual_anim_cb,
			      gGui);

  gui->stream_info_auto_update = 
    xine_config_register_bool (gui->xine, "gui.sinfo_auto_update", 
			      0,
			      _("Stream information"),
			      _("Update stream information (in stream info window) "
				"each half seconds."), 
			      CONFIG_LEVEL_ADV,
			      stream_info_auto_update_cb,
			      gGui);
  

  server = 
    (char *)xine_config_register_string (gui->xine, "gui.skin_server_url", 
					 SKIN_SERVER_URL,
					 _("Skin Server Url"),
					 _("From where we can get skins."),
					 CONFIG_LEVEL_ADV,
					 skin_server_url_cb,
					 gGui);
  
  config_update_string("gui.skin_server_url", 
		       gui->skin_server_url ? gui->skin_server_url : server);
  
  gui->osd.enabled = 
    xine_config_register_bool (gui->xine, "gui.osd_enabled", 
			      1,
			      _("Enable OSD support"),
			      _("Enabling OSD permit to display some status/informations "
				"in output window."), 
			      CONFIG_LEVEL_BEG,
			      osd_enabled_cb,
			      gGui);

  gui->osd.use_unscaled = 
    xine_config_register_bool (gui->xine, "gui.osd_use_unscaled", 
			      1,
			      _("Use unscaled OSD"),
			      _("Use unscaled (full screen resolution) OSD if possible"), 
			      CONFIG_LEVEL_ADV,
			      osd_use_unscaled_cb,
			      gGui);

  gui->osd.timeout = 
    xine_config_register_num (gui->xine, "gui.osd_timeout", 
			      3,
			      _("Dismiss OSD time (s)"),
			      _("Persistence time of OSD visual, in seconds."),
			      CONFIG_LEVEL_BEG,
			      osd_timeout_cb,
			      gGui);

  gui->smart_mode = 
    xine_config_register_bool (gui->xine, "gui.smart_mode", 
			      1,
			      _("Change xine's behavior for unexperienced user"), 
			      _("In this mode, xine take some decisions to simplify user's life."),
			      CONFIG_LEVEL_BEG,
			      smart_mode_cb,
			      gGui);

  gui->play_anyway = 
    xine_config_register_bool (gui->xine, "gui.play_anyway", 
			      0,
			      _("Ask user for playback with unsupported codec"),
			      _("If xine don't support audio or video codec of current stream "
				"the user will be asked if the stream should be played or not"), 
			      CONFIG_LEVEL_BEG,
			      play_anyway_cb,
			      gGui);

  gui->experience_level =
    (xine_config_register_enum (gui->xine, "gui.experience_level", 
			       0, (char**)exp_levels,
			       _("Configuration experience level"),
			       _("Level of user's experience, this will show more or less "
				 "configuration options."), 
			       CONFIG_LEVEL_BEG,
			       exp_level_cb, 
			       gGui)) * 10;

  gui->mixer.amp_level = xine_config_register_range (gui->xine, "gui.amp_level", 
						     100, 0, 200,
						     _("Amplification level"),
						     NULL,
						     CONFIG_LEVEL_DEB,
						     dummy_config_cb, 
						     gGui);

  gui->splash = 
    gui->splash ? (xine_config_register_bool (gui->xine, "gui.splash", 
					      1,
					      _("Display splash screen"),
					      _("If enabled, xine will display its splash screen"), 
					      CONFIG_LEVEL_BEG,
					      dummy_config_cb,
					      gGui)) : 0;
  
  gui->mixer.method = 
    xine_config_register_enum (gui->xine, "gui.audio_mixer_method", 
			      SOUND_CARD_MIXER, (char**)mixer_control_method,
			      _("Audio mixer control method"),
			      _("Which method used to control audio volume."), 
			      CONFIG_LEVEL_ADV,
			      audio_mixer_method_cb,
			      gGui);

  gui->shortcut_style = 
    xine_config_register_enum (gui->xine, "gui.shortcut_style", 
			      0, (char**)shortcut_style,
			      _("Menu shortcut style"),
			      _("Shortcut representation in menu, 'Ctrl,Alt' or 'C,M'."), 
			      CONFIG_LEVEL_ADV,
			      shortcut_style_cb,
			      gGui);

  gui->numeric.set = 0;
  gui->numeric.arg = 0;
  gui->alphanum.set = 0;
  gui->alphanum.arg = "";
  
  gui->x_lock_display (gui->display);

  gui->screen = DefaultScreen(gui->display);
  
  gui->x_lock_display (gui->video_display);
  gui->video_screen = DefaultScreen(gui->video_display);
  gui->x_unlock_display (gui->video_display);

  

  /* Some infos */
  if(__xineui_global_verbosity) {
    dump_host_info();
    dump_cpu_infos();
    dump_xfree_info(gui->display, gui->screen, (__xineui_global_verbosity >= XINE_VERBOSITY_DEBUG) ? 1 : 0);
  }

  gui_find_visual(&gui->visual, &gui->depth);

  gui_init_imlib (gui, gui->visual);

  gui->x_unlock_display (gui->display);

  /*
   * create and map panel and video window
   */
  xine_pid = getppid();
  
  xitk_init (gui->display, gui->black, gui->x_lock_display, gui->x_unlock_display, (__xineui_global_verbosity) ? 1 : 0);
  
  preinit_skins_support();
  
  if(gui->splash)
    splash_create();

  init_skins_support();

  gui->on_quit = 0;
  gui->running = 1;
  
  video_window_init (gui, window_attribute,
    ((actions_on_start(gui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)) ? 1 : 0));

  /* kbinding might open an error dialog (double keymapping), which produces a segfault,
   * when done before the video_window_init(). */
  gui->kbindings = kbindings_init_kbinding();

  gui_set_current_mmk(mediamark_get_current_mmk());

  panel_init (gui);
  gui->event_reject = 0;
}

void gui_init_imlib (gGui_t *gui, Visual *vis) {
  XColor                dummy;
  ImlibInitParams	imlib_init;

  /*
   * This routine isn't re-entrant. I cannot find a Imlib_cleanup either.
   * However, we have to reinitialize Imlib if we have to change the visual.
   * This will be a (small) memory leak.
   */
  imlib_init.flags = PARAMS_VISUALID;
  imlib_init.visualid = vis->visualid;
  if (gui->install_colormap && (vis->class & 1)) {
      /*
       * We're using a visual with changable colors / colormaps
       * (According to the comment in X11/X.h, an odd display class
       * has changable colors), and the user requested to install a
       * private colormap for xine.  Allocate a fresh colormap for
       * Imlib and Xine.
       */
      Colormap cm;
      cm = XCreateColormap(gui->display, 
			   RootWindow(gui->display, gui->screen),
			   vis, AllocNone);

      imlib_init.cmap = cm;
      imlib_init.flags |= PARAMS_COLORMAP;
  }
  gui->imlib_data = Imlib_init_with_params (gui->display, &imlib_init);
  if (gui->imlib_data == NULL) {
    fprintf(stderr, _("Unable to initialize Imlib\n"));
    exit(1);
  }

  gui->imlib_data->x.x_lock_display = gui->x_lock_display;
  gui->imlib_data->x.x_unlock_display = gui->x_unlock_display;

  gui->colormap = Imlib_get_colormap (gui->imlib_data);

  XAllocNamedColor(gui->display, gui->colormap,
		   "black", &gui->black, &dummy);

  /*
   * create an icon pixmap
   */
  
  gui->icon = XCreateBitmapFromData (gui->display, 
				      gui->imlib_data->x.root,
				      (const char *)icon_datas, 40, 40);
}

/*
 *
 */
typedef struct {
  int      start;
  char   **session_opts;
} _startup_t;

static void on_start(void *data) {
  gGui_t *gui = gGui;
  _startup_t *startup = (_startup_t *) data;

  splash_destroy();

  if(!startup->start) {
    
    gui_display_logo();
    
    do {
      xine_usec_sleep(5000);
    } while(gui->logo_mode != 1);

  }

  if(startup->session_opts) {
    int i = 0;
    int dummy_session;
    
    while(startup->session_opts[i])
      (void) session_handle_subopt(startup->session_opts[i++], NULL, &dummy_session);
    
  }
  
  if(startup->start)
    gui_execute_action_id (gui, ACTID_PLAY);
  
}

static void on_stop (void *data) {
  gGui_t *gui = data;
  gui_exit_2 (gui);
}

void gui_run(char **session_opts) {
  gGui_t *gui = gGui;
  _startup_t  startup;
  int         i, auto_start = 0;
  
  video_window_change_skins (gui->vwin, 0);
  panel_add_autoplay_buttons (gui->panel);
  panel_show_tips (gui->panel);
  panel_add_mixer_control (gui->panel);
  panel_update_channel_display (gui->panel);
  panel_update_mrl_display (gui->panel);
  panel_update_runtime_display (gui->panel);

  /* Register config entries related to video control settings */
  control_init (gui);

  /* autoscan playlist  */
  if(gui->autoscan_plugin != NULL) {
    const char *const *autoscan_plugins = xine_get_autoplay_input_plugin_ids (gui->xine);
    int         i;

    if(autoscan_plugins) {

      for(i = 0; autoscan_plugins[i] != NULL; ++i) {

	if(!strcasecmp(autoscan_plugins[i], gui->autoscan_plugin)) {
	  int    num_mrls, j;
	  const char * const *autoplay_mrls = xine_get_autoplay_mrls (gui->xine,
							 gui->autoscan_plugin,
							 &num_mrls);
	  
	  if(autoplay_mrls) {
	    for (j = 0; j < num_mrls; j++)
	      mediamark_append_entry(autoplay_mrls[j],
				     autoplay_mrls[j], NULL, 0, -1, 0, 0);
	   
	    gui->playlist.cur = 0;
	    gui_set_current_mmk(mediamark_get_current_mmk());
	  }
	}    
      }
    }
  }  
  
  enable_playback_controls (gui->panel, (gui->playlist.num > 0));

  /* We can't handle signals here, xitk handle this, so
   * give a function callback for this.
   */
  xitk_register_signal_handler(gui_signal_handler, NULL);

  /*
   * event loop
   */

#ifdef HAVE_LIRC
  if(__xineui_global_lirc_enable)
    lirc_start();
#endif
  
  if(gui->stdctl_enable)
    stdctl_start();

  /*  global event handler */
  gui->widget_key = xitk_register_event_handler ("NO WINDOW", None,
    gui_handle_event, NULL, gui_dndcallback, NULL, gui);
  
#ifdef HAVE_READLINE
  start_remote_server();
#endif
  init_session();

  if(gui->tvout) {
    int w, h;
    
    video_window_get_visible_size (gui->vwin, &w, &h);
    tvout_set_fullscreen_mode (gui->tvout, 
      ((video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE) ? 0 : 1), w, h);
  }
  
  if(gui->actions_on_start[0] != ACTID_NOKEY) {

    /* Popup setup window if there is no config file */
    if(actions_on_start(gui->actions_on_start, ACTID_SETUP)) {
      xine_config_save (gui->xine, __xineui_global_config_file);
      gui_execute_action_id (gui, ACTID_SETUP);
    }
    
    /*  The user wants to hide video window  */
    if(actions_on_start(gui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)) {
      if (!panel_is_visible (gui->panel))
        gui_execute_action_id (gui, ACTID_TOGGLE_VISIBLITY);
      
      /* DXR3 case */
      if (video_window_is_visible (gui->vwin))
        video_window_set_visibility (gui->vwin, 0);
      else
	xine_port_send_gui_data(gui->vo_port,
			      XINE_GUI_SEND_VIDEOWIN_VISIBLE,
			      (int *)0);
      
    }

    /* The user wants to see in fullscreen mode  */
    for (i = actions_on_start(gui->actions_on_start, ACTID_TOGGLE_FULLSCREEN); i > 0; i--)
      gui_execute_action_id (gui, ACTID_TOGGLE_FULLSCREEN);

#ifdef HAVE_XINERAMA
    /* The user wants to see in xinerama fullscreen mode  */
    for (i = actions_on_start(gui->actions_on_start, ACTID_TOGGLE_XINERAMA_FULLSCREEN); i > 0; i--)
      gui_execute_action_id (gui, ACTID_TOGGLE_XINERAMA_FULLSCREEN);
#endif

    /* User load a playlist on startup */
    if(actions_on_start(gui->actions_on_start, ACTID_PLAYLIST)) {
      gui_set_current_mmk(mediamark_get_current_mmk());

      if(gui->playlist.num) {
	gui->playlist.cur = 0;
        if (!is_playback_widgets_enabled (gui->panel))
          enable_playback_controls (gui->panel, 1);
      }
    }

    if(actions_on_start(gui->actions_on_start, ACTID_TOGGLE_INTERLEAVE))
      gui_toggle_interlaced();
    
    /*  The user request "play on start" */
    if(actions_on_start(gui->actions_on_start, ACTID_PLAY)) {
      if((mediamark_get_current_mrl()) != NULL) {
	auto_start = 1;
      }
    }

    /* Flush actions on start */
    if(actions_on_start(gui->actions_on_start, ACTID_QUIT)) {
      gui->actions_on_start[0] = ACTID_QUIT;
      gui->actions_on_start[1] = ACTID_NOKEY;
    }
  }

  startup.start        = auto_start;
  startup.session_opts = session_opts;

  oxine_init();
  
  xitk_run (on_start, &startup, on_stop, gui);

  /* save playlist */
  if(gui->playlist.mmk && gui->playlist.num) {
    char buffer[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    snprintf(buffer, sizeof(buffer), "%s/.xine/xine-ui_old_playlist.tox", xine_get_homedir());
    mediamark_save_mediamarks(buffer);
  }

  gui->running = 0;
  deinit_session();

  kbindings_save_kbinding(gui->kbindings);
  kbindings_free_kbinding(&gui->kbindings);
}

