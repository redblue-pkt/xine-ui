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

#include "common.h"

#define STEP_SIZE 256

/*
 * global variables
 */
extern gGui_t          *gGui;
extern _panel_t        *panel;

static pid_t            xine_pid;

/* Icon data */
static unsigned char xine_bits[] = {
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

static char *exp_levels[] = {
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

static char *visual_anim_style[] = {
  "None",
  "Post Plugin",
  "Stream Animation",
  NULL
};

static char *mixer_control_method[] = {
  "Sound card",
  "Software",
  NULL
};

void dummy_config_cb(void *data, xine_cfg_entry_t *cfg) {
  /* It exist to avoid "restart" window message in setup window */
}
static void auto_vo_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->auto_vo_visibility = cfg->num_value;
}
static void auto_panel_visibility_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->auto_panel_visibility = cfg->num_value;
}
static void skip_by_chapter_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->skip_by_chapter = cfg->num_value;
  panel_update_nextprev_tips();
}
static void ssaver_timeout_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->ssaver_timeout = cfg->num_value;
}

static void visual_anim_cb(void *data, xine_cfg_entry_t *cfg) {
  
  if(((gGui->visual_anim.enabled == 2) && (cfg->num_value != 2)) && gGui->visual_anim.running)
     visual_anim_stop();
  
  gGui->visual_anim.enabled = cfg->num_value;
}

static void stream_info_auto_update_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->stream_info_auto_update = cfg->num_value;
  stream_infos_toggle_auto_update();
}

/*
 * Layer above callbacks
 */
static void layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->layer_above = cfg->num_value;
}
static void always_layer_above_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->always_layer_above = cfg->num_value;
}

/*
 * Callback for snapshots saving location.
 */
static void snapshot_loc_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->snapshot_location = cfg->str_value;
}

/*
 * Callback for skin server
 */
static void skin_server_url_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->skin_server_url = cfg->str_value;
}

/*
 * OSD cbs
 */
static void osd_enabled_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->osd.enabled = cfg->num_value;
}
static void osd_use_unscaled_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->osd.use_unscaled = cfg->num_value;
}
static void osd_timeout_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->osd.timeout = cfg->num_value;
}

static void smart_mode_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->smart_mode = cfg->num_value;
}

static void play_anyway_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->play_anyway = cfg->num_value;
}

static void exp_level_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->experience_level = (cfg->num_value * 10);
}

static void audio_mixer_method_cb(void *data, xine_cfg_entry_t *cfg) {
  int max = 100, vol = 0;
  
  gGui->mixer.method = cfg->num_value;

  switch(gGui->mixer.method) {
  case SOUND_CARD_MIXER:
    max = 100;
    vol = gGui->mixer.volume_level;
    break;
  case SOFTWARE_MIXER:
    max = 200;
    vol = gGui->mixer.amp_level;
    break;
  }

  xitk_slider_set_max(panel->mixer.slider, max);
  xitk_slider_set_pos(panel->mixer.slider, vol);
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
}
static void gui_signal_handler (int sig, void *data) {
  pid_t     cur_pid = getppid();
  
  switch (sig) {

  case SIGHUP:
    if(cur_pid == xine_pid) {
      printf("SIGHUP received: re-read config file\n");
      config_reset();
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
      
      config_save();
      
      action.sa_handler = dummy_sighandler;
      sigemptyset(&(action.sa_mask));
      action.sa_flags = 0;
      if(sigaction(SIGALRM, &action, NULL) != 0) {
	fprintf(stderr, "sigaction(SIGALRM) failed: %s\n", strerror(errno));
      }
      alarm(5);
      
      xine_stop(gGui->stream);
    }
    break;
  }

}

/*
 *
 */
void gui_execute_action_id(action_id_t action) {
  xine_event_t   xine_event;
  
  if(action & ACTID_IS_INPUT_EVENT) {
    if((action >= ACTID_EVENT_NUMBER_0) && (action <= ACTID_EVENT_NUMBER_9)) {

      if(!gGui->numeric.set) {
	gGui->numeric.set = 1;
	gGui->numeric.arg = 0;
      }
      
      if(((gGui->numeric.arg * 10) + (action - ACTID_EVENT_NUMBER_0)) <= INT_MAX) {
	gGui->numeric.arg *= 10;
	gGui->numeric.arg += (action - ACTID_EVENT_NUMBER_0);
      }
      else
	fprintf(stderr, "WARNING: Input number canceled, avoid overflow\n");

    } 
    else if(action == ACTID_EVENT_NUMBER_10_ADD) {

      if(!gGui->numeric.set) {
	gGui->numeric.set = 1;
	gGui->numeric.arg = 0;
      }
      
      if((gGui->numeric.arg + 10) <= INT_MAX)
	gGui->numeric.arg += 10;
      else
	fprintf(stderr, "WARNING: Input number canceled, avoid overflow\n");

    }
    else {
      gGui->numeric.set = 0;
      gGui->numeric.arg = 0;
    }

    /* events for advanced input plugins. */
    xine_event.type        = action & ~ACTID_IS_INPUT_EVENT;
    xine_event.data_length = 0;
    xine_event.data        = NULL;
    xine_event.stream      = gGui->stream;
    gettimeofday(&xine_event.tv, NULL);
    
    xine_event_send(gGui->stream, &xine_event);
    return;
  }

  switch(action) {
    
  case ACTID_WINDOWREDUCE:
    video_window_set_mag (0.8 * video_window_get_mag());
    break;

  case ACTID_WINDOWENLARGE:
    video_window_set_mag (1.2 * (video_window_get_mag() + 0.002));
    break;

  case ACTID_ZOOM_1_1:
  case ACTID_WINDOW100:
    video_window_set_mag (1.0);
    osd_display_info(_("Zoom: 1:1"));
    break;

  case ACTID_WINDOW200:
    video_window_set_mag (2.0);
    osd_display_info(_("Zoom: 200%%"));
    break;

  case ACTID_WINDOW50:
    video_window_set_mag (0.5);
    osd_display_info(_("Zoom: 50%%"));
    break;

  case ACTID_SPU_NEXT:
    if(!gGui->numeric.set)
      gui_change_spu_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_spu_channel(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_SPU_PRIOR:
    if(!gGui->numeric.set)
      gui_change_spu_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_spu_channel(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
    
  case ACTID_CONTROLSHOW:
    gui_control_show(NULL, NULL);
    break;
    
  case ACTID_TOGGLE_WINOUT_VISIBLITY:
    gui_toggle_visibility (NULL, NULL);
    break;
    
  case ACTID_AUDIOCHAN_NEXT:
    if(!gGui->numeric.set)
      gui_change_audio_channel(NULL, (void*)GUI_NEXT);
    else
      gui_direct_change_audio_channel(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_AUDIOCHAN_PRIOR:
    if(!gGui->numeric.set)
      gui_change_audio_channel(NULL, (void*)GUI_PREV);
    else
      gui_direct_change_audio_channel(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
    
  case ACTID_PAUSE:
    gui_pause (NULL, (void*)(1), 0); 
    break;

  case ACTID_PLAYLIST:
    gui_playlist_show(NULL, NULL);
    break;
      
  case ACTID_TOGGLE_VISIBLITY:
    panel_toggle_visibility (NULL, NULL);
    break;

  case ACTID_TOGGLE_FULLSCREEN:
    gui_set_fullscreen_mode(NULL, NULL);
    break;

#ifdef HAVE_XINERAMA
  case ACTID_TOGGLE_XINERAMA_FULLSCREEN:
    gui_set_xinerama_fullscreen_mode();
    break;
#endif

  case ACTID_TOGGLE_ASPECT_RATIO:
    gui_toggle_aspect(-1);
    break;

  case ACTID_STREAM_INFOS:
    gui_stream_infos_show(NULL, NULL);
    break;

  case ACTID_TOGGLE_INTERLEAVE:
    gui_toggle_interlaced();
    break;

  case ACTID_QUIT:
    gui_exit(NULL, NULL);
    break;

  case ACTID_PLAY:
    gui_play(NULL, NULL);
    break;

  case ACTID_STOP:
    gui_stop(NULL, NULL);
    break;

  case ACTID_EVENT_SENDER:
    gui_event_sender_show(NULL, NULL);
    break;
    
  case ACTID_MRL_NEXT:
    if(!gGui->numeric.set)
      gui_nextprev(NULL, (void*)GUI_NEXT);
    else
      gui_direct_nextprev(NULL, (void*)GUI_NEXT, gGui->numeric.arg);
    break;
    
  case ACTID_MRL_PRIOR:
    if (!gGui->numeric.set)
      gui_nextprev(NULL, (void*)GUI_PREV);
    else
      gui_direct_nextprev(NULL, (void*)GUI_PREV, gGui->numeric.arg);
    break;
      
  case ACTID_SETUP:
    gui_setup_show(NULL, NULL);
    break;

  case ACTID_EJECT:
    gui_eject(NULL, NULL);
    break;

  case ACTID_SET_CURPOS:
    /* Number is a percentage */
    gGui->numeric.arg %= 100; /* range [0..100] */
    gui_set_current_position((65534 * gGui->numeric.arg) / 100);
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

  case ACTID_SET_CURPOS_0:
    gui_set_current_position (0);
    break;

  case ACTID_SEEK_REL_m:
    if(gGui->numeric.set) {
      gGui->numeric.arg = -gGui->numeric.arg;
      gui_seek_relative (gGui->numeric.arg);
    }
  break;

  case ACTID_SEEK_REL_p:
    if(gGui->numeric.set)
      gui_seek_relative (gGui->numeric.arg);
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
    panel_toggle_audio_mute(NULL, NULL, !gGui->mixer.mute);
    break;
    
  case ACTID_AV_SYNC_m3600:
    {
      int av_offset = (xine_get_param(gGui->stream, XINE_PARAM_AV_OFFSET) - 3600);

      gGui->mmk.av_offset = av_offset;
      if (gGui->playlist.cur < gGui->playlist.num)
        gGui->playlist.mmk[gGui->playlist.cur]->av_offset = av_offset;
      xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, av_offset);
      osd_display_info(_("A/V offset: %d"), av_offset);
    }
    break;
    
  case ACTID_AV_SYNC_p3600:
    {
      int av_offset = (xine_get_param(gGui->stream, XINE_PARAM_AV_OFFSET) + 3600);
      
      gGui->mmk.av_offset = av_offset;
      if (gGui->playlist.cur < gGui->playlist.num)
        gGui->playlist.mmk[gGui->playlist.cur]->av_offset = av_offset;
      xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, av_offset);
      osd_display_info(_("A/V offset: %d"), av_offset);
    }
    break;

  case ACTID_AV_SYNC_RESET:
    xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, 0);
    osd_display_info(_("A/V Offset: reset."));
    break;

  case ACTID_SV_SYNC_p:
    {
      int spu_offset = xine_get_param(gGui->stream, XINE_PARAM_SPU_OFFSET) + 3600;
      
      xine_set_param(gGui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
      osd_display_info(_("SPU Offset: %d"), spu_offset);
    }
    break;

  case ACTID_SV_SYNC_m:
    {
      int spu_offset = xine_get_param(gGui->stream, XINE_PARAM_SPU_OFFSET) - 3600;
      
      xine_set_param(gGui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
      osd_display_info(_("SPU Offset: %d"), spu_offset);
    }
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
    panel_snapshot(NULL, NULL);
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
    if(!gGui->cursor_grabbed) {
      if(!panel_is_visible()) {
	XLockDisplay(gGui->display);
	XGrabPointer(gGui->display, gGui->video_window, 1, None, 
		     GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
	XUnlockDisplay(gGui->display);
      }
      
      gGui->cursor_grabbed = 1;
    }
    else {
      XLockDisplay(gGui->display);
      XUngrabPointer(gGui->display, CurrentTime);
      XUnlockDisplay(gGui->display);
      gGui->cursor_grabbed = 0;
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

  case ACTID_DPMSSTANDBY:
    xine_system(0, "xset dpms force standby");
    break;

  case ACTID_MRLIDENTTOGGLE:
    gGui->is_display_mrl = !gGui->is_display_mrl;
    panel_update_mrl_display();
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
    gGui->playlist.loop++;
    if(gGui->playlist.loop == PLAYLIST_LOOP_MODES_NUM)
      gGui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
    
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
    break;
    
  case ACTID_ADDMEDIAMARK:
    gui_add_mediamark();
    break;

  case ACTID_SKINDOWNLOAD:
    download_skin(gGui->skin_server_url);
    break;

  case ACTID_OSD_SINFOS:
    osd_stream_infos();
    break;

  case ACTID_FILESELECTOR:
    gui_file_selector();
    break;

  case ACTID_HUECONTROLp:
    if(gGui->video_settings.hue <= (65535 - STEP_SIZE)) {
      config_update_num("gui.vo_hue", gGui->video_settings.hue + STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_HUE, gGui->video_settings.hue);
      osd_draw_bar(_("Hue"), 0, 65535, gGui->video_settings.hue, OSD_BAR_STEPPER);
    }
    break;
  case ACTID_HUECONTROLm:
    if(gGui->video_settings.hue >= STEP_SIZE) {
      config_update_num("gui.vo_hue", gGui->video_settings.hue - STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_HUE, gGui->video_settings.hue);
      osd_draw_bar(_("Hue"), 0, 65535, gGui->video_settings.hue, OSD_BAR_STEPPER);
    }
    break;

  case ACTID_SATURATIONCONTROLp:
    if(gGui->video_settings.saturation <= (65535 - STEP_SIZE)) {
      config_update_num("gui.vo_saturation", gGui->video_settings.saturation + STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_SATURATION, gGui->video_settings.saturation);
      osd_draw_bar(_("Saturation"), 0, 65535, gGui->video_settings.saturation, OSD_BAR_STEPPER);
    }
    break;
  case ACTID_SATURATIONCONTROLm:
    if(gGui->video_settings.saturation >= STEP_SIZE) {
      config_update_num("gui.vo_saturation", gGui->video_settings.saturation - STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_SATURATION, gGui->video_settings.saturation);
      osd_draw_bar(_("Saturation"), 0, 65535, gGui->video_settings.saturation, OSD_BAR_STEPPER);
    }
    break;

  case ACTID_BRIGHTNESSCONTROLp:
    if(gGui->video_settings.brightness <= (65535 - STEP_SIZE)) {
      config_update_num("gui.vo_brightness", gGui->video_settings.brightness + STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_BRIGHTNESS, gGui->video_settings.brightness);
      osd_draw_bar(_("Brightness"), 0, 65535, gGui->video_settings.brightness, OSD_BAR_STEPPER);
    }
    break;
  case ACTID_BRIGHTNESSCONTROLm:
    if(gGui->video_settings.brightness >= STEP_SIZE) {
      config_update_num("gui.vo_brightness", gGui->video_settings.brightness - STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_BRIGHTNESS, gGui->video_settings.brightness);
      osd_draw_bar(_("Brightness"), 0, 65535, gGui->video_settings.brightness, OSD_BAR_STEPPER);
    }
    break;

  case ACTID_CONTRASTCONTROLp:
    if(gGui->video_settings.contrast <= (65535 - STEP_SIZE)) {
      config_update_num("gui.vo_contrast", gGui->video_settings.contrast + STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_CONTRAST, gGui->video_settings.contrast);
      osd_draw_bar(_("Contrast"), 0, 65535, gGui->video_settings.contrast, OSD_BAR_STEPPER);
    }
    break;
  case ACTID_CONTRASTCONTROLm:
    if(gGui->video_settings.contrast >= STEP_SIZE) {
      config_update_num("gui.vo_contrast", gGui->video_settings.contrast - STEP_SIZE);
      control_set_image_prop(XINE_PARAM_VO_CONTRAST, gGui->video_settings.contrast);
      osd_draw_bar(_("Contrast"), 0, 65535, gGui->video_settings.contrast, OSD_BAR_STEPPER);
    }
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
    if(xine_get_status(gGui->stream) != XINE_STATUS_STOP) {
      if(gGui->playlist.control & PLAYLIST_CONTROL_STOP)
	gGui->playlist.control &= !PLAYLIST_CONTROL_STOP;
      else
	gGui->playlist.control |= PLAYLIST_CONTROL_STOP;
      osd_display_info(_("Playlist: %s"), 
		       (gGui->playlist.control & PLAYLIST_CONTROL_STOP) ? _("Stop") : _("Continue"));
    }
    break;
    
  default:
    break;
  }

  /* Some sort of function was done given. Clear numeric argument. */
  gGui->numeric.set = 0;
  gGui->numeric.arg = 0;

}	    

/*
 * top-level event handler
 */
void gui_handle_event (XEvent *event, void *data) {

  switch(event->type) {

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case DestroyNotify:
    if(event->xany.window == gGui->panel_window
       || event->xany.window == gGui->video_window) {
      xine_exit (gGui->xine);
      gGui->running = 0;
    }
    break;
    
  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    /* printf ("ButtonPress\n"); */

    /* printf ("button: %d\n",bevent->button); */

    if ((bevent->button == 3) && (bevent->window == gGui->video_window))
      panel_toggle_visibility (NULL, NULL);
    
  }
  break;

  case ButtonRelease:
  case KeyPress:
    kbindings_handle_kbinding(gGui->kbindings, event);
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
 * Start playback to next entry in playlist (or stop the engine, then display logo).
 */
void gui_playlist_start_next(void) {

  if (gGui->ignore_next)
    return;

  panel_reset_slider ();
  
  if(gGui->playlist.control & PLAYLIST_CONTROL_STOP) {
    gGui->playlist.control &= !PLAYLIST_CONTROL_STOP;
    gui_display_logo();
    return;
  }
  
  switch(gGui->playlist.loop) {
    
  case PLAYLIST_LOOP_NO_LOOP:
  case PLAYLIST_LOOP_LOOP:
    gGui->playlist.cur++;
    
    if(gGui->playlist.cur < gGui->playlist.num) {
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				 gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset)) {

	if((gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) && 
	   mediamark_all_played() && (gGui->actions_on_start[0] == ACTID_QUIT))
	  gui_exit(NULL, NULL);

	gui_display_logo();
      }
    }
    else {
      
      if(gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP) {
	if(gGui->actions_on_start[0] == ACTID_QUIT)
	  gui_exit(NULL, NULL);

	gGui->playlist.cur--;
	mediamark_reset_played_state();
	gui_display_logo();
      }
      else if(gGui->playlist.loop == PLAYLIST_LOOP_LOOP) {
	gGui->playlist.cur = 0;
	gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0,
				   gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset)) {
	  if(mediamark_all_played() && (gGui->actions_on_start[0] == ACTID_QUIT))
	    gui_exit(NULL, NULL);
	  gui_display_logo();
	}
      }

    }
    break;
    
  case PLAYLIST_LOOP_REPEAT:
    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
			       gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset))
      gui_display_logo();
    break;

  case PLAYLIST_LOOP_SHUFFLE:
  case PLAYLIST_LOOP_SHUF_PLUS:
    if(!mediamark_all_played()) {
      
    __shuffle_restart:
      gGui->playlist.cur = mediamark_get_shuffle_next();
      
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				 gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset))
	gui_display_logo();
    }
    else {
      mediamark_reset_played_state();

      if(gGui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS)
	goto __shuffle_restart;
      else if(gGui->actions_on_start[0] == ACTID_QUIT)
      	gui_exit(NULL, NULL);
      
      gui_display_logo();
    }
    break;
        
  }
  
  if(is_playback_widgets_enabled() && (!gGui->playlist.num)) {
    gui_set_current_mrl(NULL);
    enable_playback_controls(0);
  }

}

static void gui_find_visual (Visual **visual_return, int *depth_return) {
  XWindowAttributes  attribs;
  XVisualInfo	    *vinfo;
  XVisualInfo	     vinfo_tmpl;
  int		     num_visuals;
  int		     depth = 0;
  Visual	    *visual = NULL;

  if (gGui->prefered_visual_id == None) {
    /*
     * List all available TrueColor visuals, pick the best one for xine.
     * We prefer visuals of depth 15/16 (fast).  Depth 24/32 may be OK, 
     * but could be slow.
     */
    vinfo_tmpl.screen = gGui->screen;
    vinfo_tmpl.class  = (gGui->prefered_visual_class != -1
			 ? gGui->prefered_visual_class : TrueColor);
    vinfo = XGetVisualInfo(gGui->display,
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
    vinfo_tmpl.visualid = gGui->prefered_visual_id;
    vinfo = XGetVisualInfo(gGui->display,
			   VisualIDMask, &vinfo_tmpl, 
			   &num_visuals);
    if (vinfo == NULL) {
      printf(_("gui_main: selected visual %#lx does not exist, trying default visual\n"),
	     (long) gGui->prefered_visual_id);
    } else {
      depth = vinfo[0].depth;
      visual = vinfo[0].visual;
      XFree(vinfo);
    }
  }

  if (depth == 0) {
    XVisualInfo vinfo;

    XGetWindowAttributes(gGui->display, (DefaultRootWindow(gGui->display)), &attribs);

    depth = attribs.depth;
  
    if (XMatchVisualInfo(gGui->display, gGui->screen, depth, TrueColor, &vinfo)) {
      visual = vinfo.visual;
    } else {
      printf (_("gui_main: couldn't find true color visual.\n"));

      depth = DefaultDepth (gGui->display, gGui->screen);
      visual = DefaultVisual (gGui->display, gGui->screen); 
    }
  }

  if (depth_return != NULL)
    *depth_return = depth;
  if (visual_return != NULL)
    *visual_return = visual;
}

void gui_deinit(void) {
  xitk_unregister_event_handler(&gGui->widget_key);
}

/*
 * Initialize the GUI
 */
void gui_init (int nfiles, char *filenames[], window_attributes_t *window_attribute) {
  int    i;
  char  *server;

  /*
   * init playlist
   */
  for (i = 0; i < nfiles; i++) {
    char *sub = NULL;
    
    /* grab recursively all files from dir */
    if(is_a_dir(filenames[i])) {
      if(filenames[i][strlen(filenames[i]) - 1] == '/')
	filenames[i][strlen(filenames[i]) - 1] = '\0'; 
      
      mediamark_collect_from_directory(filenames[i]);
    }
    else {
      if((sub = (char *)get_last_double_semicolon(filenames[i])) != NULL) {
	if(is_ipv6_last_double_semicolon(filenames[i]))
	  sub = NULL;
	else {
	  *sub = 0;
	  sub += 2;
	}
      }
      
      mediamark_add_entry((const char *)filenames[i], 
			  (const char *)filenames[i], sub, 0, -1, 0, 0);
    }
  }
  
  if((gGui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) || 
     (gGui->playlist.loop == PLAYLIST_LOOP_SHUF_PLUS))
    gGui->playlist.cur = mediamark_get_shuffle_next();
  
  gGui->is_display_mrl = 0;
  gGui->mrl_overrided  = 0;
  gGui->new_pos        = -1;

  /*
   * X / imlib stuff
   */

  if (!XInitThreads ()) {
    printf (_("\nXInitThreads failed - looks like you don't have a "
	    "thread-safe xlib.\n"));
    exit (1);
  } 
  
  if((gGui->display = XOpenDisplay((getenv("DISPLAY")))) == NULL) {
    fprintf(stderr, _("Cannot open display\n"));
    exit(1);
  }

  if (xine_config_register_bool (gGui->xine, "gui.xsynchronize", 
				 0,
				 _("Synchronized X protocol (debug)"), 
				 CONFIG_NO_HELP,
				 CONFIG_LEVEL_ADV,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA)) {

    XLockDisplay(gGui->display);
    XSynchronize (gGui->display, True);
    XUnlockDisplay(gGui->display);
    fprintf (stderr, _("Warning! Synchronized X activated - this is way slow...\n"));
  }

  gGui->layer_above = 
    xine_config_register_bool (gGui->xine, "gui.layer_above", 0,
			       _("Windows stacking"),
			       _("Use wm layer property to place window on top."), 
			       CONFIG_LEVEL_ADV,
			       layer_above_cb,
			       CONFIG_NO_DATA);

  gGui->always_layer_above = 
    xine_config_register_bool (gGui->xine, "gui.always_layer_above", 0,
			       _("Windows stacking (more)"),
			       _("Use WM layer property to place windows on top."), 
			       CONFIG_LEVEL_ADV,
			       always_layer_above_cb,
			       CONFIG_NO_DATA);

  if(gGui->always_layer_above && (!gGui->layer_above))
    config_update_num("gui.layer_above", 1);

  gGui->snapshot_location = 
    (char *)xine_config_register_string (gGui->xine, "gui.snapshotdir", 
					 (char *) (xine_get_homedir()),
					 _("Snapshot location"),
					 _("Where snapshots will be saved."),
					 CONFIG_LEVEL_BEG,
					 snapshot_loc_cb,
					 CONFIG_NO_DATA);
  
  gGui->ssaver_timeout =
    xine_config_register_num (gGui->xine, "gui.screensaver_timeout", 10,
			      _("Screensaver wakeup"),
			      _("Time between two screensaver fake events, 0 to disable."),
			      CONFIG_LEVEL_ADV,
			      ssaver_timeout_cb,
			      CONFIG_NO_DATA);
  
  gGui->skip_by_chapter = 
    xine_config_register_bool (gGui->xine, "gui.skip_by_chapter", 1,
			       _("Chapter hopping"),
			       _("Play next|previous chapter instead of mrl (dvdnav)"), 
			       CONFIG_LEVEL_ADV,
			       skip_by_chapter_cb, 
			       CONFIG_NO_DATA);
  
  gGui->auto_vo_visibility = 
    xine_config_register_bool (gGui->xine, "gui.auto_video_output_visibility", 0,
			       _("Visibility behavior of output window"),
			       _("Hide video output window if there is no video in the stream"), 
			       CONFIG_LEVEL_ADV,
			       auto_vo_visibility_cb, 
			       CONFIG_NO_DATA);

  gGui->auto_panel_visibility = 
    xine_config_register_bool (gGui->xine, "gui.auto_panel_visibility", 0,
			       _("Visiblility behavior of panel"),
			       _("Automatically show/hide panel window, according to auto_video_output_visibility"), 
			       CONFIG_LEVEL_ADV,
			       auto_panel_visibility_cb,
			       CONFIG_NO_DATA); 
 
  gGui->eventer_sticky = 
    xine_config_register_bool(gGui->xine, "gui.eventer_sticky", 
			      1,
			      _("Event sender behavior"),
			      _("Event sender window stick to main panel"), 
			      CONFIG_LEVEL_ADV,
			      event_sender_sticky_cb,
			      CONFIG_NO_DATA);

  gGui->visual_anim.enabled = 
    xine_config_register_enum(gGui->xine, "gui.visual_anim", 
			      1, /* Post plugin */
			      visual_anim_style,
			      _("Visual animation style"),
			      _("Display some video animations when "
				"current stream is audio only (eg: mp3)."), 
			      CONFIG_LEVEL_BEG,
			      visual_anim_cb,
			      CONFIG_NO_DATA);

  gGui->stream_info_auto_update = 
    xine_config_register_bool(gGui->xine, "gui.sinfo_auto_update", 
			      0,
			      _("Stream information"),
			      _("Update stream information (in stream info window) "
				"each half seconds."), 
			      CONFIG_LEVEL_ADV,
			      stream_info_auto_update_cb,
			      CONFIG_NO_DATA);
  

  server = 
    (char *)xine_config_register_string (gGui->xine, "gui.skin_server_url", 
					 SKIN_SERVER_URL,
					 _("Skin Server Url"),
					 _("From where we can get skins."),
					 CONFIG_LEVEL_ADV,
					 skin_server_url_cb,
					 CONFIG_NO_DATA);
  
  config_update_string("gui.skin_server_url", 
		       gGui->skin_server_url ? gGui->skin_server_url : server);
  
  gGui->osd.enabled = 
    xine_config_register_bool(gGui->xine, "gui.osd_enabled", 
			      1,
			      _("Enable OSD support"),
			      _("Enabling OSD permit to display some status/informations "
				"in output window."), 
			      CONFIG_LEVEL_BEG,
			      osd_enabled_cb,
			      CONFIG_NO_DATA);

  gGui->osd.use_unscaled = 
    xine_config_register_bool(gGui->xine, "gui.osd_use_unscaled", 
			      1,
			      _("Use unscaled OSD"),
			      _("Use unscaled (full screen resolution) OSD if possible"), 
			      CONFIG_LEVEL_ADV,
			      osd_use_unscaled_cb,
			      CONFIG_NO_DATA);

  gGui->osd.timeout = 
    xine_config_register_num(gGui->xine, "gui.osd_timeout", 
			      3,
			      _("Dismiss OSD time (s)"),
			      _("Persistence time of OSD visual, in seconds."),
			      CONFIG_LEVEL_BEG,
			      osd_timeout_cb,
			      CONFIG_NO_DATA);

  gGui->smart_mode = 
    xine_config_register_bool(gGui->xine, "gui.smart_mode", 
			      1,
			      _("Change xine's behavior for unexperienced user"), 
			      _("In this mode, xine take some decisions to simplify user's life."),
			      CONFIG_LEVEL_BEG,
			      smart_mode_cb,
			      CONFIG_NO_DATA);

  gGui->play_anyway = 
    xine_config_register_bool(gGui->xine, "gui.play_anyway", 
			      0,
			      _("Ask user for playback with unsupported codec"),
			      _("If xine don't support audio or video codec of current stream "
				"the user will be asked if the stream should be played or not"), 
			      CONFIG_LEVEL_BEG,
			      play_anyway_cb,
			      CONFIG_NO_DATA);

  gGui->experience_level =
    (xine_config_register_enum(gGui->xine, "gui.experience_level", 
			       0, exp_levels,
			       _("Configuration experience level"),
			       _("Level of user's experience, this will show more or less "
				 "configuration options."), 
			       CONFIG_LEVEL_BEG,
			       exp_level_cb, 
			       CONFIG_NO_DATA)) * 10;

  gGui->mixer.amp_level = xine_config_register_range(gGui->xine, "gui.amp_level", 
						     100, 0, 200,
						     _("Amplification level"),
						     NULL,
						     CONFIG_LEVEL_DEB,
						     dummy_config_cb, 
						     CONFIG_NO_DATA);

  gGui->splash = 
    gGui->splash ? (xine_config_register_bool(gGui->xine, "gui.splash", 
					      1,
					      _("Display splash screen"),
					      _("If enabled, xine will display its splash screen"), 
					      CONFIG_LEVEL_BEG,
					      dummy_config_cb,
					      CONFIG_NO_DATA)) : 0;
  
  gGui->mixer.method = 
    xine_config_register_enum(gGui->xine, "gui.audio_mixer_method", 
			      SOUND_CARD_MIXER, mixer_control_method,
			      _("Audio mixer control method"),
			      _("Which method used to control audio volume."), 
			      CONFIG_LEVEL_ADV,
			      audio_mixer_method_cb,
			      CONFIG_NO_DATA);

  gGui->numeric.set = 0;
  gGui->numeric.arg = 0;
  
  XLockDisplay (gGui->display);

  gGui->screen = DefaultScreen(gGui->display);

  /* Some infos */
  if(gGui->verbosity) {
    dump_cpu_infos();
    dump_xfree_info(gGui->display, gGui->screen, (gGui->verbosity >= XINE_VERBOSITY_DEBUG) ? 1 : 0);
  }

  gui_find_visual(&gGui->visual, &gGui->depth);

  gui_init_imlib (gGui->visual);

  XUnlockDisplay (gGui->display);

  /*
   * create and map panel and video window
   */
  xine_pid = getppid();
  
  xitk_init(gGui->display, (gGui->verbosity) ? 1 : 0);
  
  preinit_skins_support();
  
  if(gGui->splash)
    splash_create();

  init_skins_support();

  gGui->on_quit = 0;
  gGui->running = 1;
  
  video_window_init (window_attribute, 
		     ((actions_on_start(gGui->actions_on_start, 
				       ACTID_TOGGLE_WINOUT_VISIBLITY)) ? 1 : 0));

  /* kbinding might open an error dialog (double keymapping), which produces a segfault,
   * when done before the video_window_init(). */
  gGui->kbindings = kbindings_init_kbinding();

  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());

  panel_init ();
}

void gui_init_imlib (Visual *vis) {
  XColor                dummy;
  ImlibInitParams	imlib_init;

  /*
   * This routine isn't re-entrant. I cannot find a Imlib_cleanup either.
   * However, we have to reinitialize Imlib if we have to change the visual.
   * This will be a (small) memory leak.
   */
  imlib_init.flags = PARAMS_VISUALID;
  imlib_init.visualid = vis->visualid;
  if (gGui->install_colormap && (vis->class & 1)) {
      /*
       * We're using a visual with changable colors / colormaps
       * (According to the comment in X11/X.h, an odd display class
       * has changable colors), and the user requested to install a
       * private colormap for xine.  Allocate a fresh colormap for
       * Imlib and Xine.
       */
      Colormap cm;
      cm = XCreateColormap(gGui->display, 
			   RootWindow(gGui->display, gGui->screen),
			   vis, AllocNone);

      imlib_init.cmap = cm;
      imlib_init.flags |= PARAMS_COLORMAP;
  }
  gGui->imlib_data = Imlib_init_with_params (gGui->display, &imlib_init);
  if (gGui->imlib_data == NULL) {
    fprintf(stderr, _("Unable to initialize Imlib\n"));
    exit(1);
  }

  gGui->colormap = Imlib_get_colormap (gGui->imlib_data);

  XAllocNamedColor(gGui->display, gGui->colormap,
		   "black", &gGui->black, &dummy);

  /*
   * create an icon pixmap
   */
  
  gGui->icon = XCreateBitmapFromData (gGui->display, 
				      gGui->imlib_data->x.root,
				      xine_bits, 40, 40);
}

/*
 *
 */
typedef struct {
  int      start;
  char   **session_opts;
} _startup_t;

static void on_start(void *data) {
  _startup_t *startup = (_startup_t *) data;

  splash_destroy();

  if((!startup->start && !gGui->playlist.num) || (!startup->start && gGui->playlist.num)) {

    gui_display_logo();
    
    do {
      xine_usec_sleep(5000);
    } while(gGui->logo_mode != 1);

  }

  /* Crapy Workaround:
   * The lib seems unable to display OSD until the stream got 
   * any video data.
   */
  if(startup->start || (!startup->start && !gGui->display_logo)) {
    (void) xine_open(gGui->stream, (const char *) XINE_BLACK_STREAM);
    
    if(!startup->start && !gGui->display_logo)
      xine_play(gGui->stream, 0, 0);
    
    gGui->ignore_next = 1;
    xine_stop(gGui->stream);
    gGui->ignore_next = 0;
  }
  
  if(startup->session_opts) {
    int i = 0;
    int dummy_session;
    
    while(startup->session_opts[i])
      session_handle_subopt(startup->session_opts[i++], &dummy_session);
    
  }
  
  if(startup->start)
    gui_execute_action_id(ACTID_PLAY);
  
}

void gui_run(char **session_opts) {
  _startup_t  startup;
  int         i, auto_start = 0;
  
  video_window_change_skins();
  panel_add_autoplay_buttons();
  panel_add_mixer_control();
  panel_update_channel_display () ;
  panel_update_mrl_display ();
  panel_update_runtime_display();

  /* Register config entries related to video control settings */
  control_config_register();

  /* autoscan playlist  */
  if(gGui->autoscan_plugin != NULL) {
    const char *const *autoscan_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    int         i;

    if(autoscan_plugins) {

      for(i = 0; autoscan_plugins[i] != NULL; ++i) {

	if(!strcasecmp(autoscan_plugins[i], gGui->autoscan_plugin)) {
	  int    num_mrls, j;
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine,
							 gGui->autoscan_plugin,
							 &num_mrls);
	  
	  if(autoplay_mrls) {
	    for (j = 0; j < num_mrls; j++)
	      mediamark_add_entry((const char *)autoplay_mrls[j],
				  (const char *)autoplay_mrls[j], NULL, 0, -1, 0, 0);
	   
	    gGui->playlist.cur = 0;
	    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	  }
	}    
      }
    }
  }  
  
  enable_playback_controls((gGui->playlist.num > 0));

  /* We can't handle signals here, xitk handle this, so
   * give a function callback for this.
   */
  xitk_register_signal_handler(gui_signal_handler, NULL);

  /*
   * event loop
   */

#ifdef HAVE_LIRC
  if(gGui->lirc_enable)
    lirc_start();
#endif
  
  if(gGui->stdctl_enable)
    stdctl_start();

  /*  global event handler */
  gGui->widget_key = xitk_register_event_handler("NO WINDOW", None,
						 gui_handle_event, 
						 NULL,
						 gui_dndcallback, 
						 NULL, NULL);
  
  start_remote_server();
  init_session();

  if(gGui->tvout) {
    int w, h;
    
    video_window_get_visible_size(&w, &h);
    tvout_set_fullscreen_mode(gGui->tvout, 
			      ((video_window_get_fullscreen_mode() & WINDOWED_MODE) ? 0 : 1), w, h);
  }
  
  if(gGui->actions_on_start[0] != ACTID_NOKEY) {

    /* Popup setup window if there is no config file */
    if(actions_on_start(gGui->actions_on_start, ACTID_SETUP)) {
      config_save();
      gui_execute_action_id(ACTID_SETUP);
    }
    
    /*  The user wants to hide video window  */
    if(actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)) {
      if(!panel_is_visible())
	gui_execute_action_id(ACTID_TOGGLE_VISIBLITY);
      
      /* DXR3 case */
      if(video_window_is_visible())
	video_window_set_visibility(!(video_window_is_visible()));
      else
	xine_gui_send_vo_data(gGui->stream,
			      XINE_GUI_SEND_VIDEOWIN_VISIBLE,
			      (int *)0);
      
    }

    /* The user wants to see in fullscreen mode  */
    for (i = actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_FULLSCREEN); i > 0; i--)
      gui_execute_action_id(ACTID_TOGGLE_FULLSCREEN);

#ifdef HAVE_XINERAMA
    /* The user wants to see in xinerama fullscreen mode  */
    for (i = actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_XINERAMA_FULLSCREEN); i > 0; i--)
      gui_execute_action_id(ACTID_TOGGLE_XINERAMA_FULLSCREEN);
#endif

    /* User load a playlist on startup */
    if(actions_on_start(gGui->actions_on_start, ACTID_PLAYLIST)) {
      mediamark_load_mediamarks(gGui->playlist.on_start);
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      if((!is_playback_widgets_enabled()) && gGui->playlist.num)
	enable_playback_controls(1);
    }

    if(actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_INTERLEAVE))
      gui_toggle_interlaced();
    
    /*  The user request "play on start" */
    if(actions_on_start(gGui->actions_on_start, ACTID_PLAY)) {
      if((mediamark_get_current_mrl()) != NULL) {
	auto_start = 1;
      }
    }

    /* Flush actions on start */
    if(actions_on_start(gGui->actions_on_start, ACTID_QUIT)) {
      gGui->actions_on_start[0] = ACTID_QUIT;
      gGui->actions_on_start[1] = ACTID_NOKEY;
    }
  }

  startup.start        = auto_start;
  startup.session_opts = session_opts;

  xitk_run(on_start, (void *)&startup);

  /* save playlist */
  if(gGui->playlist.mmk && gGui->playlist.num) {
    char buffer[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    sprintf(buffer, "%s/.xine/xine-ui_old_playlist.tox", xine_get_homedir());
    mediamark_save_mediamarks(buffer);
  }

  gGui->running = 0;
  deinit_session();

  kbindings_save_kbinding(gGui->kbindings);
  kbindings_free_kbinding(&gGui->kbindings);

  XCloseDisplay(gGui->display);
}
