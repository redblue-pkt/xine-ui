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
 * implementation of all the various actions for the gui (play, stop, open, pause...)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <strings.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <xine/xineutils.h>

#include "common.h"
#include "actions.h"
#include "event.h"
#include "stream_infos.h"
#include "tvset.h"
#include "playlist.h"
#include "panel.h"
#include "setup.h"
#include "mrl_browser.h"
#include "event_sender.h"
#include "help.h"
#include "viewlog.h"
#include "acontrol.h"
#include "control.h"
#include "videowin.h"
#include "file_browser.h"
#include "skins.h"
#include "tvout.h"
#include "stdctl.h"
#include "download.h"
#include "errors.h"
#include "lirc.h"
#include "oxine/oxine.h"
#include "xine-toolkit/skin.h"

static gGui_t *_gui_get_nextprev (void *data, int *value) {
  gGui_t **p = data, *gui;
  if (!p)
    return NULL;
  gui = *p;
  if ((p < gui->nextprev) || (p >= gui->nextprev + sizeof (gui->nextprev) / sizeof (gui->nextprev[0])))
    return NULL;
  *value = p - gui->nextprev - 1;
  return gui;
}

static void gui_messages_off (gGui_t *gui) {
  pthread_mutex_lock (&gui->no_messages.mutex);
  ++gui->no_messages.level;
  gui->no_messages.until.tv_sec = sizeof (gui->no_messages.until.tv_sec) > 4 ? ((~(uint64_t)0) >> 1) : 0x7fffffff;
  pthread_mutex_unlock (&gui->no_messages.mutex);
}

static void gui_messages_on (gGui_t *gui) {
  pthread_mutex_lock (&gui->no_messages.mutex);
  if (--gui->no_messages.level == 0)
    gettimeofday (&gui->no_messages.until, NULL);
  pthread_mutex_unlock (&gui->no_messages.mutex);
}

void reparent_window (gGui_t *gui, xitk_window_t *xwin) {
  if (!gui || !xwin)
    return;
  if ((!(video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE)) && !(xitk_get_wm_type (gui->xitk) & WM_TYPE_EWMH_COMP)) {
    /* Don't unmap this window, because on re-mapping, it will not be visible until
     * its ancestor, the video window, is visible. That's not what's intended. */
    xitk_window_set_wm_window_type (xwin, video_window_is_visible (gui->vwin) < 2 ? WINDOW_TYPE_NORMAL : WINDOW_TYPE_NONE);
    xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    xitk_window_raise_window (xwin);
    xitk_window_try_to_set_input_focus (xwin);
    video_window_set_transient_for (gui->vwin, xwin);
  } else {
    xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    xitk_window_raise_window (xwin);
    video_window_set_transient_for (gui->vwin, xwin);
    layer_above_video (gui, xwin);
  }
}

void raise_window (gGui_t *gui, xitk_window_t *xwin, int visible, int running) {
  if (gui && xwin && visible && running) {
    reparent_window(gui, xwin);
    layer_above_video (gui, xwin);
  }
}

void toggle_window (gGui_t *gui, xitk_window_t *xwin, xitk_widget_list_t *widget_list, int *visible, int running) {
  if (!gui || !xwin)
    return;

  if((*visible) && running) {

    if(gui->use_root_window) {
      if ((xitk_window_flags (xwin, 0, 0) & XITK_WINF_VISIBLE))
        xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_ICONIFIED);
      else
        xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    }
    else if (!(xitk_window_flags (xwin, 0, 0) & XITK_WINF_VISIBLE)) {
      /* Obviously user has iconified the window, let it be */
    }
    else {
      *visible = 0;
      xitk_hide_widgets(widget_list);
      xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
    }
  }
  else {
    if(running) {
      *visible = 1;
      xitk_show_widgets(widget_list);

      xitk_window_flags (xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);

      layer_above_video (gui, xwin);
    }
  }
}

int gui_xine_get_pos_length (gGui_t *gui, xine_stream_t *stream, int *ppos, int *ptime, int *plength) {
  do {
    if (!gui)
      break;
    if (!stream)
      break;

    if (stream == gui->stream) {
      pthread_mutex_lock (&gui->seek_mutex);
      if (gui->seek_running) {
        /* xine_play (), xine_get_* () etc. all grab stream frontend lock.
         * Dont wait for seek thread, just use its values. */
        if (ppos)
          *ppos = gui->stream_length.pos;
        if (ptime)
          *ptime = gui->stream_length.time;
        if (plength)
          *plength = gui->stream_length.length;
        pthread_mutex_unlock (&gui->seek_mutex);
        return 1;
      }
      pthread_mutex_unlock (&gui->seek_mutex);
    }

    if (xine_get_status (stream) == XINE_STATUS_PLAY) {
      int lpos = 0, ltime = 0, llength = 0;
      int ret, t = 0;
      while (((ret = xine_get_pos_length (stream, &lpos, &ltime, &llength)) == 0) && (++t < 10) && (!gui->on_quit)) {
        printf ("gui_xine_get_pos_length: wait 100 ms (%d).\n", t);
        xine_usec_sleep (100000); /* wait before trying again */
      }
      if (ret == 0)
        break;
      if (stream == gui->stream) {
        gui->stream_length.pos = lpos;
        gui->stream_length.time = ltime;
        gui->stream_length.length = llength;
      }
      if (ppos)
        *ppos = lpos;
      if (ptime)
        *ptime = ltime;
      if (plength)
        *plength = llength;
      return ret;
    }
  } while (0);

  if (ppos)
    *ppos = 0;
  if (ptime)
    *ptime = 0;
  if (plength)
    *plength = 0;
  return 0;
}

/*
 *
 */
void gui_display_logo (gGui_t *gui) {
  pthread_mutex_lock(&gui->logo_mutex);

  gui->logo_mode = 2;

  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY) {
    gui->ignore_next = 1;
    xine_stop(gui->stream);
    gui->ignore_next = 0;
  }

  if (gui->visual_anim.running)
    visual_anim_stop (gui);

  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -1);
  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, -1);
  panel_update_channel_display (gui->panel);

  if(gui->display_logo)
    (void) gui_xine_open_and_play (gui, (char *)gui->logo_mrl, NULL, 0, 0, 0, 0, 1);

  gui->logo_mode = 1;

  panel_reset_slider (gui->panel);
  if (stream_infos_is_visible (gui->streaminfo))
    stream_infos_update_infos (gui->streaminfo);

  pthread_mutex_unlock(&gui->logo_mutex);
}

static int _gui_xine_play (gGui_t *gui, xine_stream_t *stream, int start_pos, int start_ms, int update_mmk) {
  int      ret;
  int      has_video;
  int      already_playing = (gui->logo_mode == 0);

  if(gui->visual_anim.post_changed && (xine_get_status(stream) == XINE_STATUS_STOP)) {
    post_rewire_visual_anim (gui);
    gui->visual_anim.post_changed = 0;
  }

  has_video = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);
  if (has_video)
    has_video = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_VIDEO);

  if((has_video && gui->visual_anim.enabled == 1) && gui->visual_anim.running) {

    if (post_rewire_audio_port_to_stream (gui, stream))
      gui->visual_anim.running = 0;

  } else if (!has_video && (gui->visual_anim.enabled == 1) &&
	     (gui->visual_anim.running == 0) && gui->visual_anim.post_output_element.post) {

    if (post_rewire_audio_post_to_stream (gui, stream))
      gui->visual_anim.running = 1;

  }

  if ((ret = xine_play (stream, start_pos, start_ms)) == 0)
    gui_handle_xine_error (gui, stream, NULL);
  else {
    char *ident;

    pthread_mutex_lock(&gui->logo_mutex);
    if(gui->logo_mode != 2)
      gui->logo_mode = 0;
    pthread_mutex_unlock(&gui->logo_mutex);

    if(gui->logo_mode == 0) {

      if (stream_infos_is_visible (gui->streaminfo))
	stream_infos_update_infos (gui->streaminfo);

      if (update_mmk && ((ident = stream_infos_get_ident_from_stream (stream)) != NULL)) {
        gui_playlist_set_str_val (gui, ident, MMK_VAL_IDENT, GUI_MMK_CURRENT);
        gui_current_set_index (gui, GUI_MMK_CURRENT);

        video_window_set_mrl (gui->vwin, ident);
        playlist_mrlident_toggle (gui);
	panel_update_mrl_display (gui->panel);
	free(ident);
      }

      if(has_video) {

	if((gui->visual_anim.enabled == 2) && gui->visual_anim.running)
	  visual_anim_stop (gui);

	if(gui->auto_vo_visibility) {

          if (video_window_is_visible (gui->vwin) < 2)
            video_window_set_visibility (gui->vwin, 1);

	}

        if (gui->auto_panel_visibility && video_window_is_visible (gui->vwin) > 1 &&
          panel_is_visible (gui->panel) > 1)
          panel_toggle_visibility (NULL, gui->panel);

      }
      else {

	if(gui->auto_vo_visibility) {

          if (panel_is_visible (gui->panel) < 2)
            panel_toggle_visibility (NULL, gui->panel);

          if (video_window_is_visible (gui->vwin) > 1)
            video_window_set_visibility (gui->vwin, 0);

	}

        if (gui->auto_panel_visibility && video_window_is_visible (gui->vwin) > 1 &&
          panel_is_visible (gui->panel) < 2)
          panel_toggle_visibility (NULL, gui->panel);

        if (video_window_is_visible (gui->vwin) > 1) {
          if (!gui->visual_anim.running)
            visual_anim_play (gui);
	}
	else
	  gui->visual_anim.running = 2;
      }

      xine_usec_sleep(100);
      if(!already_playing)
        osd_update_status (gui);
    }
  }

  return ret;
}

void gui_pl_updated (gGui_t *gui) {

  video_window_set_mrl (gui->vwin, gui->mmk.ident);
  event_sender_update_menu_buttons (gui);
  panel_update_mrl_display (gui->panel);
  playlist_update_focused_entry (gui);

  gui->playlist.ref_append = gui->playlist.cur;
}

static void _start_anyway_done (void *data, int state) {
  gGui_t *gui = data;
  gui->play_data.running = 0;

  if (state == 2)
    _gui_xine_play (gui, gui->play_data.stream, gui->play_data.start_pos,
      gui->play_data.start_time_in_secs * 1000, gui->play_data.update_mmk);
  else
    gui_playlist_start_next (gui);
}

int gui_xine_play (gGui_t *gui, xine_stream_t *stream, int start_pos, int start_time_in_secs, int update_mmk) {
  int has_video, has_audio, v_unhandled = 0, a_unhandled = 0;
  uint32_t video_handled, audio_handled;

  if (gui->play_data.running)
    return 0;

  has_video     = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);
  video_handled = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HANDLED);
  has_audio     = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_AUDIO);
  audio_handled = xine_get_stream_info(stream, XINE_STREAM_INFO_AUDIO_HANDLED);

  if (has_video)
    has_video = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_VIDEO);
  if (has_audio)
    has_audio = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_AUDIO);

  v_unhandled = (has_video && (!video_handled));
  a_unhandled = (has_audio && (!audio_handled));
  if(v_unhandled || a_unhandled) {
    char *buffer = NULL;
    char *v_info = NULL;
    char *a_info = NULL;

    if(v_unhandled && a_unhandled) {
      buffer = xitk_asprintf(_("The stream '%s' isn't supported by xine:\n\n"),
                             (stream == gui->stream) ? gui->mmk.mrl : gui->visual_anim.mrls[gui->visual_anim.current]);
    }
    else {
      buffer = xitk_asprintf(_("The stream '%s' uses an unsupported codec:\n\n"),
                             (stream == gui->stream) ? gui->mmk.mrl : gui->visual_anim.mrls[gui->visual_anim.current]);
    }

    if(v_unhandled) {
      const char *minfo;
      uint32_t    vfcc;
      char        tmp[32];

      minfo = xine_get_meta_info(stream, XINE_META_INFO_VIDEOCODEC);
      vfcc = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_FOURCC);
      v_info = xitk_asprintf(_("Video Codec: %s (%s)\n"),
                             (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"),
                             (get_fourcc_string(tmp, sizeof(tmp), vfcc)));
    }

    if(a_unhandled) {
      const char *minfo;
      uint32_t    afcc;
      char        tmp[32];

      minfo = xine_get_meta_info(stream, XINE_META_INFO_AUDIOCODEC);
      afcc = xine_get_stream_info(stream, XINE_STREAM_INFO_AUDIO_FOURCC);
      a_info = xitk_asprintf(_("Audio Codec: %s (%s)\n"),
                             (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"),
                             (get_fourcc_string(tmp, sizeof(tmp), afcc)));
    }


    if(v_unhandled && a_unhandled) {
      gui_msg (gui, XUI_MSG_ERROR, "%s%s%s", buffer ? buffer : "", v_info ? v_info : "", a_info ? a_info : "");
      free(buffer); free(v_info); free(a_info);
      return 0;
    }

    if(!gui->play_anyway) {

      gui->play_data.stream             = stream;
      gui->play_data.start_pos          = start_pos;
      gui->play_data.start_time_in_secs = start_time_in_secs;
      gui->play_data.update_mmk         = update_mmk;
      gui->play_data.running            = 1;

      xitk_register_key_t key =
      xitk_window_dialog_3 (gui->xitk,
        NULL,
        get_layer_above_video (gui), 400, _("Start Playback ?"), _start_anyway_done, gui,
        NULL, XITK_LABEL_YES, XITK_LABEL_NO, NULL, 0, ALIGN_CENTER,
        "%s%s%s%s", buffer ? buffer : "", v_info ? v_info : "", a_info ? a_info : "", _("\nStart playback anyway ?\n"));
      free(buffer); free(v_info); free(a_info);

      video_window_set_transient_for (gui->vwin, xitk_get_window (gui->xitk, key));

      /* Doesn't work so well yet
         use gui->play_data.running hack for a while
         xitk_window_dialog_set_modal(xw);
      */

      return 1;
    }

    free(buffer); free(v_info); free(a_info);
  }

  return _gui_xine_play (gui, stream, start_pos, start_time_in_secs * 1000, update_mmk);
}

int gui_xine_open_and_play (gGui_t *gui, char *_mrl, char *_sub, int start_pos,
			   int start_time, int av_offset, int spu_offset, int report_error) {
  char *mrl = _mrl;
  int ret;

  if (gui->verbosity)
    printf("%s():\n\tmrl: '%s',\n\tsub '%s',\n\tstart_pos %d, start_time %d, av_offset %d, spu_offset %d.\n",
	   __func__, _mrl, (_sub) ? _sub : "NONE", start_pos, start_time, av_offset, spu_offset);

  if(!strncasecmp(mrl, "cfg:/", 5)) {
    config_mrl (gui->xine, mrl);
    gui_playlist_start_next (gui);
    return 1;
  }
  else if(/*(!strncasecmp(mrl, "ftp://", 6)) ||*/ (!strncasecmp(mrl, "dload:/", 7)))  {
    char        *url = mrl;
    download_t   download;

    if(!strncasecmp(mrl, "dload:/", 7))
      url = _mrl + 7;

    download.gui    = gui;
    download.buf    = NULL;
    download.error  = NULL;
    download.size   = 0;
    download.status = 0;

    if((network_download(url, &download))) {
      char *filename;

      filename = strrchr(url, '/');
      if(filename && filename[1]) { /* we have a filename */
	char  fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 2];
	FILE *fd;

	filename++;
	snprintf(fullfilename, sizeof(fullfilename), "%s/%s", xine_get_homedir(), filename);

	if((fd = fopen(fullfilename, "w+b")) != NULL) {
	  fwrite(download.buf, download.size, 1, fd);
	  fflush(fd);
	  fclose(fd);

          mediamark_set_str_val (&gui->playlist.mmk[gui->playlist.cur], fullfilename, MMK_VAL_MRL);
          gui_current_set_index (gui, GUI_MMK_CURRENT);
	  mrl = gui->mmk.mrl;
        }
      }
    }

    free(download.buf);
    free(download.error);

  }

  if (mrl_look_like_playlist (gui, mrl)) {
    if (gui_playlist_add_item (gui, mrl, 1, GUI_ITEM_TYPE_AUTO, 0)) {
      gui_current_set_index (gui, GUI_MMK_CURRENT);
      mrl        = gui->mmk.mrl;
      start_pos  = 0;
      start_time = gui->mmk.start;
      av_offset  = gui->mmk.av_offset;
      spu_offset = gui->mmk.spu_offset;
      playlist_update_playlist (gui);
    }
  }

  gui_messages_off (gui);
  ret = xine_open(gui->stream, (const char *) mrl);
  gui_messages_on (gui);
  if (!ret) {

#ifdef XINE_PARAM_GAPLESS_SWITCH
    if( xine_check_version(1,1,1) )
      xine_set_param(gui->stream, XINE_PARAM_GAPLESS_SWITCH, 0);
#endif

    if(!strcmp(mrl, gui->mmk.mrl))
      gui->playlist.mmk[gui->playlist.cur]->played = 1;

    if(report_error)
      gui_handle_xine_error (gui, gui->stream, mrl);
    return 0;
  }

  if(_sub) {

    gui_messages_off (gui);
    ret = xine_open(gui->spu_stream, _sub);
    gui_messages_on (gui);
    if (ret)
      xine_stream_master_slave(gui->stream,
			       gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
  }
  else
    xine_close (gui->spu_stream);

  xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, av_offset);
  xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);

  if (!gui_xine_play (gui, gui->stream, start_pos, start_time, 1)) {
    if(!strcmp(mrl, gui->mmk.mrl))
      gui->playlist.mmk[gui->playlist.cur]->played = 1;
    return 0;
  }

  if(!strcmp(mrl, gui->mmk.mrl))
    gui->playlist.mmk[gui->playlist.cur]->played = 1;

  gui_xine_get_pos_length (gui, gui->stream, NULL, NULL, NULL);

  if (gui->stdctl_enable)
    stdctl_playing (gui, mrl);

  return 1;
}

int gui_open_and_play_alternates (gGui_t *gui, mediamark_t *mmk, const char *sub) {
  char *alt;

  if(!(alt = mediamark_get_current_alternate_mrl(mmk)))
    alt = mediamark_get_first_alternate_mrl(mmk);

  do {

    if (gui_xine_open_and_play (gui, alt, gui->mmk.sub, 0, 0, 0, 0, 0))
      return 1;

  } while((alt = mediamark_get_next_alternate_mrl(&(gui->mmk))));
  return 0;
}

/*
 * Callback-functions for widget-button click
 */
void gui_exit (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  /* NOTE: This runs as xitk callback, with xitk lock held.
   * Do not wait for threads that try to use xitk here -- see gui_exit_2 () below. */
  (void)w;

  if (gui->verbosity)
    printf ("xine_ui: gui_exit ().\n");

  oxine_exit();

  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY) {
    gui->ignore_next = 1;

    if(gui->visual_anim.running) {
      xine_post_out_t * audio_source;

      xine_stop(gui->visual_anim.stream);

      while(xine_get_status(gui->visual_anim.stream) == XINE_STATUS_PLAY)
	xine_usec_sleep(50000);

      audio_source = xine_get_audio_source(gui->stream);
      (void) xine_post_wire_audio_port(audio_source, gui->ao_port);
    }

    xine_stop (gui->stream);
    while(xine_get_status(gui->stream) == XINE_STATUS_PLAY)
      xine_usec_sleep(50000);

    gui->ignore_next = 0;
  }

  gui->on_quit = 1;
  xitk_stop (gui->xitk);
}

void gui_exit_2 (gGui_t *gui) {

  if (gui->verbosity)
    printf ("xine_ui: gui_exit_2 ().\n");

  /* paranoia: xitk internal error exit (eg. parent console close */
  if (!gui->on_quit)
    gui_exit (NULL, gui);

  /* shut down event queue threads */
  /* do it first, events access gui elements ... */
  xine_event_dispose_queue(gui->event_queue);
  xine_event_dispose_queue(gui->visual_anim.event_queue);
  gui->event_queue = gui->visual_anim.event_queue = NULL;

  panel_deinit (gui->panel);
  destroy_mrl_browser (gui->mrlb);
  control_deinit (gui->vctrl);

  gui_deinit (gui);

  playlist_deinit (gui);

  setup_end (gui->setup);
  viewlog_end (gui->viewlog);
  kbedit_end (gui->keyedit);
  event_sender_end (gui);
  stream_infos_end (gui->streaminfo);
  tvset_end (gui->tvset);
  pplugin_end (&gui->post_audio);
  pplugin_end (&gui->post_video);
  help_end (gui->help);
#ifdef HAVE_TAR
  skin_download_end (gui->skdloader);
#endif

  if (gui->load_stream) {
    filebrowser_end (gui->load_stream);
    gui->load_stream = NULL;
  }
  if (gui->load_sub) {
    filebrowser_end (gui->load_sub);
    gui->load_sub = NULL;
  }

  if (video_window_is_visible (gui->vwin) > 1)
    video_window_set_visibility (gui->vwin, 0);

  tvout_deinit(gui->tvout);

  osd_deinit (gui);

  config_update_num (gui->xine, "gui.amp_level", gui->mixer.amp_level);
  xine_config_save (gui->xine, gui->cfg_file);

  xine_close(gui->stream);
  xine_close(gui->visual_anim.stream);
  xine_close (gui->spu_stream);

  /* we are going to dispose this stream, so make sure slider_loop
   * won't use it anymore (otherwise -> segfault on exit).
   */
  gui->running = 0;

  if(gui->visual_anim.post_output_element.post)
    xine_post_dispose (gui->xine, gui->visual_anim.post_output_element.post);
  gui->visual_anim.post_output_element.post = NULL;

  /* unwire post plugins before closing streams */
  post_deinit (gui);

  xine_dispose(gui->stream);
  xine_dispose(gui->visual_anim.stream);
  xine_dispose (gui->spu_stream);
  gui->stream = gui->visual_anim.stream = gui->spu_stream = NULL;

  if(gui->vo_port)
    xine_close_video_driver (gui->xine, gui->vo_port);
  if(gui->vo_none)
    xine_close_video_driver (gui->xine, gui->vo_none);
  gui->vo_port = gui->vo_none = NULL;

  if(gui->ao_port)
    xine_close_audio_driver (gui->xine, gui->ao_port);
  if(gui->ao_none)
    xine_close_audio_driver (gui->xine, gui->ao_none);
  gui->ao_port = gui->ao_none = NULL;

  video_window_exit (gui->vwin);
  gui->vwin = NULL;

  if (gui->lirc_enable)
    lirc_stop (gui);

  if (gui->stdctl_enable)
    stdctl_stop (gui);

  xitk_skin_unload_config(gui->skin_config);

  gui_playlist_free (gui);
  gui_current_free (gui);

  if (gui->icon)
    xitk_image_free_image (&gui->icon);
}

void gui_play (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if((!gui->playlist.num) && (xine_get_status(gui->stream) != XINE_STATUS_PLAY))
    return;

  video_window_reset_ssaver (gui->vwin);

  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY) {
    if(gui->logo_mode != 0) {
      gui->ignore_next = 1;
      xine_stop(gui->stream);
      gui->ignore_next = 0;
    }
  }

  if(xine_get_status(gui->stream) != XINE_STATUS_PLAY) {

    if (!strncmp (gui->mmk.ident, "xine-ui version", 15)) {
      gui_msg (gui, XUI_MSG_ERROR, _("No MRL (input stream) specified"));
      return;
    }

    if (!gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
			       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset,
			       !mediamark_have_alternates(&(gui->mmk)))) {

      if(!mediamark_have_alternates(&(gui->mmk)) ||
        !gui_open_and_play_alternates (gui, &(gui->mmk), gui->mmk.sub)) {

	if (mediamark_all_played (gui) && (gui->actions_on_start[0] == ACTID_QUIT)) {
          gui_exit (NULL, gui);
	  return;
	}
        gui_display_logo (gui);
      }
    }
  }
  else {
    int oldspeed = xine_get_param(gui->stream, XINE_PARAM_SPEED);

    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
    if(oldspeed != XINE_SPEED_NORMAL)
      osd_update_status (gui);
  }

  panel_check_pause (gui->panel);
}

void gui_stop (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  gui->ignore_next = 1;
  xine_stop (gui->stream);
  gui->ignore_next = 0;

  if(!(gui->playlist.control & PLAYLIST_CONTROL_STOP_PERSIST))
    gui->playlist.control &= ~PLAYLIST_CONTROL_STOP;

  gui->stream_length.pos = gui->stream_length.time = gui->stream_length.length = 0;

  mediamark_reset_played_state (gui);
  if(gui->visual_anim.running) {
    xine_stop(gui->visual_anim.stream);
    if(gui->visual_anim.enabled == 2)
      gui->visual_anim.running = 0;
  }

  osd_hide_sinfo (gui);
  osd_hide_bar (gui);
  osd_hide_info (gui);
  osd_update_status (gui);
  panel_reset_slider (gui->panel);
  panel_check_pause (gui->panel);
  panel_update_runtime_display (gui->panel);

  if (is_playback_widgets_enabled (gui->panel)) {
    if (gui_current_set_index (gui, GUI_MMK_CURRENT) == GUI_MMK_NONE)
      enable_playback_controls (gui->panel, 0);
  }
  gui_display_logo (gui);
}

void gui_close (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  gui->ignore_next = 1;
  xine_stop (gui->stream);
  gui->ignore_next = 0;

  if(!(gui->playlist.control & PLAYLIST_CONTROL_STOP_PERSIST))
    gui->playlist.control &= ~PLAYLIST_CONTROL_STOP;

  gui->stream_length.pos = gui->stream_length.time = gui->stream_length.length = 0;

  mediamark_reset_played_state (gui);
  if(gui->visual_anim.running) {
    xine_stop(gui->visual_anim.stream);
    if(gui->visual_anim.enabled == 2)
      gui->visual_anim.running = 0;
  }

  osd_hide_sinfo (gui);
  osd_hide_bar (gui);
  osd_hide_info (gui);
  osd_update_status (gui);
  panel_reset_slider (gui->panel);
  panel_check_pause (gui->panel);
  panel_update_runtime_display (gui->panel);

  if (is_playback_widgets_enabled (gui->panel)) {
    if (gui_current_set_index (gui, GUI_MMK_CURRENT) == GUI_MMK_NONE)
      enable_playback_controls (gui->panel, 0);
  }
  gui->ignore_next = 1;
  xine_close (gui->stream);
  gui->ignore_next = 0;
}

void gui_pause (xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = data;
  int speed = xine_get_param(gui->stream, XINE_PARAM_SPEED);

  (void)w;
  (void)state;
  if(speed != XINE_SPEED_PAUSE) {
    gui->last_playback_speed = speed;
    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    panel_update_runtime_display (gui->panel);
  }
  else {
    xine_set_param (gui->stream, XINE_PARAM_SPEED, gui->last_playback_speed);
    video_window_reset_ssaver (gui->vwin);
  }

  panel_check_pause (gui->panel);
  /* Give xine engine some time before updating OSD, otherwise the */
  /* time disp may be empty when switching to XINE_SPEED_PAUSE.    */
  xine_usec_sleep(10000);
  osd_update_status (gui);
}

void gui_eject (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY)
    gui_stop (NULL, gui);

  if(xine_eject(gui->stream)) {

    if(gui->playlist.num) {
      char         *mrl;
      size_t        tok_len = 0;

      const char *const cur_mrl = mediamark_get_current_mrl (gui);
      /*
       * If it's an mrl (____://) remove all of them in playlist
       */
      if ( (mrl = strstr((cur_mrl), ":/")) )
	tok_len = (mrl - cur_mrl) + 2;

      if (tok_len != 0) {
        mediamark_t **a, **b, **e;
        gui_playlist_lock (gui);
        /* remove maching entries */
        for (a = b = gui->playlist.mmk, e = a + gui->playlist.num; a < e; a++) {
          if (!strncasecmp ((*a)->mrl, cur_mrl, tok_len))
            mediamark_free (a);
          else
            *b++ = *a;
        }
        gui->playlist.num = b - gui->playlist.mmk;
        gui->playlist.cur = gui->playlist.num ? 0 : -1;
        for (; b < e; b++)
          *b = NULL;
        gui_playlist_unlock (gui);
      }
      else {
      __remove_current_mrl:
	/*
	 * Remove only the current MRL
	 */
        gui_playlist_remove (gui, GUI_MMK_CURRENT);
      }

      if (is_playback_widgets_enabled (gui->panel) && (!gui->playlist.num))
        enable_playback_controls (gui->panel, 0);

    }

    gui_current_set_index (gui, GUI_MMK_CURRENT);
    playlist_update_playlist (gui);
  }
  else {
    /* Remove current mrl */
    if(gui->playlist.num)
      goto __remove_current_mrl;
  }

  if(!gui->playlist.num)
    gui->playlist.cur = -1;

}

void gui_toggle_visibility (gGui_t *gui) {

  if (!gui->use_root_window) {
    int visible = video_window_is_visible (gui->vwin) < 2;

    video_window_set_visibility (gui->vwin, visible);

    /* (re)start/stop visual animation */
    if (video_window_is_visible (gui->vwin) > 1) {
      if (gui->visual_anim.enabled && (gui->visual_anim.running == 2))
        visual_anim_play (gui);
    }
    else {

      if (gui->visual_anim.running) {
        visual_anim_stop (gui);
	gui->visual_anim.running = 2;
      }
    }
  }
}

void gui_toggle_border (gGui_t *gui) {
  video_window_toggle_border (gui->vwin);
}

static void set_fullscreen_mode (gGui_t *gui, int fullscreen_mode) {
  if ((video_window_is_visible (gui->vwin) < 2) || gui->use_root_window)
    return;

  video_window_set_fullscreen_mode (gui->vwin, fullscreen_mode);
  /* Drawable has changed, update cursor visiblity */
  if(!gui->cursor_visible)
    video_window_set_cursor_visibility (gui->vwin, gui->cursor_visible);
}

void gui_set_fullscreen_mode (xitk_widget_t *w, void *data) {
  (void)w;
  gGui_t *gui = (gGui_t *)data;
  set_fullscreen_mode (gui, FULLSCR_MODE);
}

#ifdef HAVE_XINERAMA
void gui_set_xinerama_fullscreen_mode (gGui_t *gui) {
  set_fullscreen_mode (gui, FULLSCR_XI_MODE);
}
#endif

void gui_toggle_aspect (gGui_t *gui, int aspect) {
  static const char * const ratios[XINE_VO_ASPECT_NUM_RATIOS + 1] = {
    "Auto",
    "Square",
    "4:3",
    "Anamorphic",
    "DVB",
    NULL
  };

  if (!gui)
    return;
  if(aspect == -1)
    aspect = xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO) + 1;

  xine_set_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO, aspect);

  osd_display_info (gui, _("Aspect ratio: %s"),
    ratios [xine_get_param (gui->stream, XINE_PARAM_VO_ASPECT_RATIO)]);

  panel_raise_window(gui->panel);
}

void gui_toggle_interlaced (gGui_t *gui) {
  if (!gui)
    return;
  gui->deinterlace_enable = !gui->deinterlace_enable;
  osd_display_info (gui, _("Deinterlace: %s"), (gui->deinterlace_enable) ? _("enabled") : _("disabled"));
  post_deinterlace (gui);
  panel_raise_window(gui->panel);
}

void gui_direct_change_audio_channel (xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = data;
  (void)w;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, value);
  panel_update_channel_display (gui->panel);
  osd_display_audio_lang (gui);
}

void gui_nextprev_audio_channel (xitk_widget_t *w, void *data) {
  int channel;
  gGui_t *gui = _gui_get_nextprev (data, &channel);

  if (!gui)
    return;
  channel += xine_get_param (gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  gui_direct_change_audio_channel (w, gui, channel);
}

void gui_direct_change_spu_channel(xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = data;
  (void)w;
  if (!gui)
    return;
  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, value);
  panel_update_channel_display (gui->panel);
  osd_display_spu_lang (gui);
}

void gui_nextprev_spu_channel (xitk_widget_t *w, void *data) {
  int channel;
  gGui_t *gui = _gui_get_nextprev (data, &channel);
  int maxchannel;

  (void)w;
  if (!gui)
    return;
  channel += xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);
  maxchannel = xine_get_stream_info(gui->stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);

  if (xine_get_status(gui->spu_stream) != XINE_STATUS_IDLE) /* if we have a slave SPU channel, take it into account */
    maxchannel += xine_get_stream_info(gui->spu_stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);

  /* XINE_STREAM_INFO_MAX_SPU_CHANNEL actually returns the number of available spu channels, i.e.
   * 0 means no SPUs, 1 means 1 SPU channel, etc. */
  --maxchannel;

  if (channel > maxchannel)
    channel = -2; /* -2 == off, -1 == auto, 0 == 1st channel */
  else if (channel < -2)
    channel = maxchannel;

  gui_direct_change_spu_channel (w, gui, channel);
}

void gui_nextprev_speed (xitk_widget_t *w, void *data) {
  int speed, d;
  gGui_t *gui = _gui_get_nextprev (data, &d);

  (void)w;
  if (!gui)
    return;
  if (d < 0) {
    speed = xine_get_param (gui->stream, XINE_PARAM_SPEED);
    if (speed > XINE_SPEED_PAUSE) {
      xine_set_param (gui->stream, XINE_PARAM_SPEED, (speed /= 2));
    }
#ifdef XINE_PARAM_VO_SINGLE_STEP
    else {
      xine_set_param (gui->stream, XINE_PARAM_VO_SINGLE_STEP, 1);
      panel_update_runtime_display (gui->panel);
    }
#endif
  } else if (d > 0) {
    speed = xine_get_param (gui->stream, XINE_PARAM_SPEED);
    if (speed < XINE_SPEED_FAST_4) {
      if (speed > XINE_SPEED_PAUSE) {
        xine_set_param (gui->stream, XINE_PARAM_SPEED, (speed *= 2));
      } else {
        xine_set_param (gui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_SLOW_4));
        video_window_reset_ssaver (gui->vwin);
      }
    }
  } else {
    xine_set_param (gui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_NORMAL));
  }

  if (speed != XINE_SPEED_PAUSE)
    gui->last_playback_speed = speed;

  panel_check_pause (gui->panel);
  /* Give xine engine some time before updating OSD, otherwise the        */
  /* time disp may be empty when switching to speeds < XINE_SPEED_NORMAL. */
  xine_usec_sleep(10000);
  osd_update_status (gui);
}

static void *gui_seek_thread (void *data) {
  gGui_t *gui = data;
  int update_mmk = 0, ret;

  pthread_detach (pthread_self ());

  do {
    {
      const char *mrl = mediamark_get_current_mrl (gui);
      if (gui->logo_mode && mrl) {
        gui_messages_off (gui);
        ret = xine_open (gui->stream, mrl);
        gui_messages_on (gui);
        if (!ret) {
          gui_handle_xine_error (gui, gui->stream, mrl);
          break;
        }
      }
    }

    if (!xine_get_stream_info (gui->stream, XINE_STREAM_INFO_SEEKABLE) || (gui->ignore_next == 1))
      break;

    if (xine_get_status (gui->stream) != XINE_STATUS_PLAY) {
      gui_messages_off (gui);
      xine_open (gui->stream, gui->mmk.mrl);
      gui_messages_on (gui);

      if (gui->mmk.sub) {
        gui_messages_off (gui);
        ret = xine_open (gui->spu_stream, gui->mmk.sub);
        gui_messages_on (gui);
        if (ret)
          xine_stream_master_slave (gui->stream, gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
      } else {
        xine_close (gui->spu_stream);
      }
    }

    gui->ignore_next = 1;

    gui_playlist_lock (gui);
    if ( gui->playlist.num &&
        (gui->playlist.cur >= 0) &&
         gui->playlist.mmk &&
         gui->playlist.mmk[gui->playlist.cur] &&
        !strcmp (gui->playlist.mmk[gui->playlist.cur]->mrl, gui->mmk.mrl))
      update_mmk = 1;
    gui_playlist_unlock (gui);

    {
      int pos = -1;

      while (1) {
        pthread_mutex_lock (&gui->seek_mutex);
        if ((pos == gui->seek_pos) && (gui->seek_timestep == 0)) {
          gui->seek_running = 0;
          gui->seek_pos = -1;
          pthread_mutex_unlock (&gui->seek_mutex);
          break;
        }
        if (pos != gui->seek_pos) {

          pos = gui->seek_pos;
          pthread_mutex_unlock (&gui->seek_mutex);
          /* panel_update_slider (gui->panel, pos); */
          osd_stream_position (gui, pos);
          _gui_xine_play (gui, gui->stream, pos, 0, update_mmk);
          xine_get_pos_length (gui->stream,
            &gui->stream_length.pos, &gui->stream_length.time, &gui->stream_length.length);
          panel_update_runtime_display (gui->panel);

        } else {

          int cur_time;
          int timestep = gui->seek_timestep;
          gui->seek_timestep = 0;
          pthread_mutex_unlock (&gui->seek_mutex);
          if ( xine_get_stream_info (gui->stream, XINE_STREAM_INFO_SEEKABLE) &&
              (xine_get_status (gui->stream) == XINE_STATUS_PLAY) &&
               xine_get_pos_length (gui->stream, NULL, &cur_time, NULL)) {
            gui->ignore_next = 1;
            cur_time += timestep;
            if (cur_time < 0)
              cur_time = 0;
            _gui_xine_play (gui, gui->stream, 0, cur_time, 1);
            if (xine_get_pos_length (gui->stream,
              &gui->stream_length.pos, &gui->stream_length.time, &gui->stream_length.length)) {
              panel_update_slider (gui->panel, gui->stream_length.pos);
              osd_stream_position (gui, gui->stream_length.pos);
            }
            gui->ignore_next = 0;
          }

        }
      }
    }

    gui->ignore_next = 0;
    osd_hide_status (gui);
    panel_check_pause (gui->panel);

    return NULL;
  } while (0);

  pthread_mutex_lock (&gui->seek_mutex);
  gui->seek_running = 0;
  gui->seek_pos = -1;
  gui->seek_timestep = 0;
  pthread_mutex_unlock (&gui->seek_mutex);
  return NULL;
}

void gui_set_current_position (gGui_t *gui, int pos) {
  pthread_mutex_lock (&gui->seek_mutex);
  if (gui->seek_running == 0) {
    pthread_t pth;
    int err;
    gui->seek_running = 1;
    gui->seek_pos = pos;
    pthread_mutex_unlock (&gui->seek_mutex);
    err = pthread_create (&pth, NULL, gui_seek_thread, gui);
    if (err) {
      printf (_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror (err));
      pthread_mutex_lock (&gui->seek_mutex);
      gui->seek_running = 0;
      gui->seek_pos = -1;
      gui->seek_timestep = 0;
      pthread_mutex_unlock (&gui->seek_mutex);
    }
  } else {
    gui->seek_pos = pos;
    pthread_mutex_unlock (&gui->seek_mutex);
  }
}

void gui_seek_relative (gGui_t *gui, int off_sec) {
  if (off_sec == 0)
    return;

  pthread_mutex_lock (&gui->seek_mutex);
  if (gui->seek_running == 0) {
    pthread_t pth;
    int err;
    gui->seek_running = 1;
    gui->seek_timestep = off_sec * 1000;
    pthread_mutex_unlock (&gui->seek_mutex);
    err = pthread_create (&pth, NULL, gui_seek_thread, gui);
    if (err) {
      printf (_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror (err));
      pthread_mutex_lock (&gui->seek_mutex);
      gui->seek_running = 0;
      gui->seek_pos = -1;
      gui->seek_timestep = 0;
      pthread_mutex_unlock (&gui->seek_mutex);
    }
  } else {
    gui->seek_timestep = off_sec * 1000;
    pthread_mutex_unlock (&gui->seek_mutex);
  }
}

void gui_dndcallback (void *_gui, const char *filename) {
  gGui_t *gui = _gui;
  int n;

  if (!filename)
    return;

  n = gui_playlist_add_item (gui, filename, GUI_MAX_DIR_LEVELS, GUI_ITEM_TYPE_AUTO, 0);
  if (!n)
    return;

  playlist_update_playlist (gui);

  if (!(gui->playlist.control & PLAYLIST_CONTROL_IGNORE)) {
    if ((xine_get_status (gui->stream) == XINE_STATUS_STOP) || gui->logo_mode) {
      gui_playlist_lock (gui);
      gui->playlist.cur = gui->playlist.num - n;
      gui_playlist_unlock (gui);
      gui_current_set_index (gui, GUI_MMK_CURRENT);
      gui_pl_updated (gui);
      if (gui->smart_mode)
        gui_play (NULL, gui);
    }
  }

  if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
    enable_playback_controls (gui->panel, 1);
}

void gui_step_mrl (gGui_t *gui, int by) {
  int i, by_chapter;
  mediamark_t *mmk;

  if (!gui)
    return;
  mmk = mediamark_get_current_mmk (gui);
  by_chapter = (gui->skip_by_chapter &&
		(xine_get_stream_info(gui->stream, XINE_STREAM_INFO_HAS_CHAPTERS))) ? 1 : 0;

  if(mmk && mediamark_got_alternate(mmk))
    mediamark_unset_got_alternate(mmk);

  if (by > 0) { /* next */

    osd_hide (gui);

    if(by_chapter) {

      for(i = 0; i < by; i++)
        gui_execute_action_id (gui, ACTID_EVENT_NEXT);

    }
    else {

      switch(gui->playlist.loop) {

      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gui->ignore_next = 0;
        gui_playlist_start_next (gui);
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
        if ((gui->playlist.cur + by) < gui->playlist.num) {
          gui->playlist.cur += by - (gui->playlist.loop == PLAYLIST_LOOP_NO_LOOP);
	  gui->ignore_next = 0;
          gui_playlist_start_next (gui);
	}
	break;

      case PLAYLIST_LOOP_LOOP:
        if ((gui->playlist.cur + by) > gui->playlist.num) {
	  int newcur = by - (gui->playlist.num - gui->playlist.cur);

	  gui->ignore_next = 1;
	  gui->playlist.cur = newcur;
          gui_current_set_index (gui, GUI_MMK_CURRENT);
          if (!gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
				     gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
            gui_display_logo (gui);

	  gui->ignore_next = 0;
	}
	else {
          gui->playlist.cur += by - 1;
	  gui->ignore_next = 0;
          gui_playlist_start_next (gui);
	}
	break;
      }
    }
  }
  else if (by < 0) { /* prev */

    osd_hide (gui);
    by = -by;
    if(by_chapter) {
      for (i = 0; i < by; i++)
        gui_execute_action_id (gui, ACTID_EVENT_PRIOR);
    }
    else {
      switch(gui->playlist.loop) {
      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gui->ignore_next = 0;
        gui_playlist_start_next (gui);
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
        if ((gui->playlist.cur - by) >= 0) {
	  gui->ignore_next = 1;
          gui->playlist.cur -= by;
	  if((gui->playlist.cur < gui->playlist.num)) {
            gui_current_set_index (gui, GUI_MMK_CURRENT);
            if (!gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
				       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
              gui_display_logo (gui);
	  }
	  else
	    gui->playlist.cur = 0;
	  gui->ignore_next = 0;
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	gui->ignore_next = 1;

        if ((gui->playlist.cur - by) >= 0) {
          gui->playlist.cur -= by;
	  if((gui->playlist.cur < gui->playlist.num)) {
            gui_current_set_index (gui, GUI_MMK_CURRENT);
            if (!gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
				       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
              gui_display_logo (gui);
	  }
	  else
	    gui->playlist.cur = 0;

	}
	else {
          int newcur = (gui->playlist.cur - by) + gui->playlist.num;

	  gui->playlist.cur = newcur;
          gui_current_set_index (gui, GUI_MMK_CURRENT);
          if (!gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
				     gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
            gui_display_logo (gui);
	}

	gui->ignore_next = 0;
	break;
      }
    }
  }

  panel_check_pause (gui->panel);
}

void gui_nextprev_mrl (xitk_widget_t *w, void *data) {
  int by = 0;
  gGui_t *gui = _gui_get_nextprev (data, &by);

  (void)w;
  gui_step_mrl (gui, by);
}

void gui_playlist_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!gui->plwin)
    playlist_editor (gui);
  else if (!playlist_is_visible (gui) || gui->use_root_window)
    playlist_toggle_visibility (gui);
  else
    playlist_exit (gui);
}

void gui_mrlbrowser_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  gui->nongui_error_msg = NULL;

  if (!mrl_browser_is_running (gui->mrlb)) {
    open_mrlbrowser (NULL, gui);
  }
  else {

    if (!mrl_browser_is_visible (gui->mrlb)) {
      show_mrl_browser (gui->mrlb);
    }
    else {
      if(gui->use_root_window)
        show_mrl_browser (gui->mrlb);
      else
        destroy_mrl_browser (gui->mrlb);
    }

  }

}

void gui_acontrol_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  if (!gui->actrl)
    acontrol_init (gui);
  else
    acontrol_toggle_window (w, gui->actrl);
}

void gui_control_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  if (!gui->vctrl)
    control_init (gui);
  else
    control_toggle_window (w, gui->vctrl);
}

void gui_setup_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!gui->setup)
    setup_panel (gui);
  else if (!setup_is_visible (gui->setup) || gui->use_root_window)
    setup_toggle_visibility (gui->setup);
  else
    setup_end (gui->setup);
}

void gui_event_sender_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!gui->eventer)
    event_sender_panel (gui);
  else if (!event_sender_is_visible (gui) || gui->use_root_window)
    event_sender_toggle_visibility (gui);
  else
    event_sender_end (gui);
}

void gui_stream_infos_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!gui->streaminfo)
    stream_infos_panel (gui);
  else if (gui->use_root_window || !stream_infos_is_visible (gui->streaminfo))
    stream_infos_toggle_visibility (NULL, gui->streaminfo);
  else
    stream_infos_end (gui->streaminfo);
}

void gui_tvset_show (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!gui->tvset)
    tvset_panel (gui);
  else if (gui->use_root_window || !tvset_is_visible (gui->tvset))
    tvset_toggle_visibility (NULL, gui);
  else
    tvset_end (gui->tvset);
}

void gui_vpp_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (!pplugin_is_running (&gui->post_video))
    pplugin_panel (&gui->post_video);
  else if (!pplugin_is_visible (&gui->post_video) || gui->use_root_window)
    pplugin_toggle_visibility (NULL, &gui->post_video);
  else
    pplugin_end (&gui->post_video);
}

void gui_vpp_enable (gGui_t *gui) {
  if (!gui)
    return;
  if (pplugin_is_post_selected (&gui->post_video)) {
    gui->post_video_enable = !gui->post_video_enable;
    osd_display_info (gui, _("Video post plugins: %s."), (gui->post_video_enable) ? _("enabled") : _("disabled"));
    pplugin_update_enable_button (&gui->post_video);
    if (pplugin_is_visible (&gui->post_video))
      pplugin_rewire_from_posts_window (&gui->post_video);
    else
      pplugin_rewire_posts (&gui->post_video);
  }
}

void gui_viewlog_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  (void)w;
  if (!gui->viewlog)
    viewlog_panel (gui);
  else if (!viewlog_is_visible (gui->viewlog) || gui->use_root_window)
    viewlog_toggle_visibility (gui->viewlog);
  else
    viewlog_end (gui->viewlog);
}

void gui_kbedit_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  (void)w;
  if (!gui->keyedit)
    kbedit_window (gui);
  else if (!kbedit_is_visible (gui->keyedit) || gui->use_root_window)
    kbedit_toggle_visibility (NULL, gui->keyedit);
  else
    kbedit_end (gui->keyedit);
}

void gui_help_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  (void)w;
  if (!gui->help) {
    help_panel (gui);
  } else if (!help_is_visible (gui->help) || gui->use_root_window) {
    help_toggle_visibility (gui->help);
  } else {
    help_end (gui->help);
  }
}

/*
 * Return 1 if layer should be set
 */
int is_layer_above (gGui_t *gui) {
  if (!gui)
    return 0;
  return (gui->always_layer_above || gui->layer_above) ? 1 : 0;
}
/*
 * set window layer property to something above GNOME (and KDE?) panel
 * (reset to normal if do_raise == 0)
 */
void layer_above_video (gGui_t *gui, xitk_window_t *xwin) {
  int layer = 10;

  if (!gui || !is_layer_above (gui))
    return;

  if ((!(video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE)) && video_window_is_visible (gui->vwin) > 1) {
    layer = xitk_get_layer_level(gui->xitk);
  }
  else {
    if (is_layer_above (gui))
      layer = xitk_get_layer_level(gui->xitk);
    else
      /* FIXME: never happens? */ layer = 4;
  }

  xitk_window_set_window_layer(xwin, layer);
}

int get_layer_above_video (gGui_t *gui) {
  if (!(gui->always_layer_above || gui->layer_above))
    return 0;
  if ((!(video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE)) && video_window_is_visible (gui->vwin) > 1)
    return xitk_get_layer_level (gui->xitk);
  if (gui->always_layer_above || gui->layer_above)
    return xitk_get_layer_level (gui->xitk);
  return 4;
}

void gui_set_amp_level (gGui_t *gui, int value) {
  if(value < 0)
    value = 0;
  if(value > 200)
    value = 200;
  if (gui->mixer.amp_level == value)
    return;
  gui->mixer.amp_level = value;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL, gui->mixer.amp_level);
  panel_update_mixer_display (gui->panel);
  acontrol_update_mixer_display (gui->actrl);
  osd_draw_bar (gui, _("Amplification Level"), 0, 200, gui->mixer.amp_level, OSD_BAR_STEPPER);
}

void gui_set_audio_vol (gGui_t *gui, int value) {
  if(value < 0)
    value = 0;
  if(value > 100)
    value = 100;
  if (gui->mixer.volume_level == value)
    return;
  gui->mixer.volume_level = value;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
  panel_update_mixer_display (gui->panel);
  osd_draw_bar (gui, _("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
}

void gui_app_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  if (!gui)
    return;
  (void)w;
  if (!pplugin_is_running (&gui->post_audio))
    pplugin_panel (&gui->post_audio);
  else if (!pplugin_is_visible (&gui->post_audio) || gui->use_root_window)
    pplugin_toggle_visibility (NULL, &gui->post_audio);
  else
    pplugin_end (&gui->post_audio);
}

void gui_app_enable (gGui_t *gui) {
  if (!gui)
    return;
  if (pplugin_is_post_selected (&gui->post_audio)) {
    gui->post_audio_enable = !gui->post_audio_enable;
    osd_display_info (gui, _("Audio post plugins: %s."), (gui->post_audio_enable) ? _("enabled") : _("disabled"));
    pplugin_update_enable_button (&gui->post_audio);
    if (pplugin_is_visible (&gui->post_audio))
      pplugin_rewire_from_posts_window (&gui->post_audio);
    else
      pplugin_rewire_posts (&gui->post_audio);
  }
}

void gui_change_zoom (gGui_t *gui, int dx, int dy) {
  if ((dx == 0) && (dy == 0))
    dx = dy = 100; /* Reset zooming by recall aspect ratio. */
  else
    dx += xine_get_param (gui->stream, XINE_PARAM_VO_ZOOM_X),
    dy += xine_get_param (gui->stream, XINE_PARAM_VO_ZOOM_Y);
  xine_set_param (gui->stream, XINE_PARAM_VO_ZOOM_X, dx);
  xine_set_param (gui->stream, XINE_PARAM_VO_ZOOM_Y, dy);
  panel_raise_window (gui->panel);
}

/*
 * Toggle TV Modes on the dxr3
 */
void gui_toggle_tvmode (gGui_t *gui) {
  xine_set_param(gui->stream, XINE_PARAM_VO_TVMODE,
		 xine_get_param(gui->stream, XINE_PARAM_VO_TVMODE) + 1);

  osd_display_info (gui, _("TV Mode: %d"), xine_get_param(gui->stream, XINE_PARAM_VO_TVMODE));
}

void gui_add_mediamark (gGui_t *gui) {
  if((gui->logo_mode == 0) && (xine_get_status(gui->stream) == XINE_STATUS_PLAY)) {
    int secs;

    if (gui_xine_get_pos_length (gui, gui->stream, NULL, &secs, NULL)) {
      secs /= 1000;

      gui_playlist_append (gui, gui->mmk.mrl, gui->mmk.ident,
			     gui->mmk.sub, secs, -1, gui->mmk.av_offset, gui->mmk.spu_offset);
      playlist_update_playlist (gui);
    }
  }
}

static void fileselector_cancel_callback (filebrowser_t *fb, void *userdata) {
  gGui_t *gui = userdata;
  char *cur_dir = filebrowser_get_current_dir(fb);

  if (fb == gui->load_stream) {
    if(cur_dir && strlen(cur_dir)) {
      strlcpy(gui->curdir, cur_dir, sizeof(gui->curdir));
      config_update_string (gui->xine, "media.files.origin_path", gui->curdir);
    }
    gui->load_stream = NULL;
  }
  else if (fb == gui->load_sub)
    gui->load_sub = NULL;

  free(cur_dir);
}


/* Callback function for file select button or double-click in file browser.
   Append selected file to the current playlist */
static void fileselector_callback (filebrowser_t *fb, void *userdata) {
  gGui_t *gui = userdata;
  char *file;
  char *cur_dir = filebrowser_get_current_dir(fb);

  /* Upate configuration with the selected directory path */
  if(cur_dir && strlen(cur_dir)) {
    strlcpy(gui->curdir, cur_dir, sizeof(gui->curdir));
    config_update_string (gui->xine, "media.files.origin_path", gui->curdir);
  }
  free(cur_dir);

  /* Get the file path/name */
  file = filebrowser_get_full_filename (fb);
  if (file && file[0]) {
    int first  = gui->playlist.num;

    /* If the file has an extension which could be a playlist, attempt to append
       it to the current list as a list; otherwise, append it as an ordinary file. */
    gui_playlist_add_item (gui, file, 1, GUI_ITEM_TYPE_AUTO, 0);

    playlist_update_playlist (gui);

    /* Enable controls on display */
    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    /* If an MRL is not being played, select the first file appended. If in "smart mode" start
       playing the entry.  If a an MRL is currently being played, let it continue normally */
    if((first != gui->playlist.num) &&
      (gui->logo_mode || (xine_get_status(gui->stream) == XINE_STATUS_STOP))) {
      gui->playlist.cur = first;
      if(gui->smart_mode) {
        gui_current_set_index (gui, GUI_MMK_CURRENT);
        gui_play (NULL, gui);
      }
    }
  } /* If valid file name */

  free(file);
  gui->load_stream = NULL;
}


/* Callback function for "Select All" button in file browser. Append all files in the
   currently selected directory to the current playlist. */
static void fileselector_all_callback (filebrowser_t *fb, void *userdata) {
  gGui_t *gui = userdata;
  char *path = filebrowser_get_current_dir (fb);

  if (path && path[0]) {
    char **files;
    /* Update the configuration with the current path */
    strlcpy (gui->curdir, path, sizeof (gui->curdir));
    config_update_string (gui->xine, "media.files.origin_path", gui->curdir);
    /* Get all of the file names in the current directory as an array of pointers to strings */
    files = filebrowser_get_all_files (fb);
    if (files && files[0]) {
      char buf[2048], *add, *e = buf + sizeof (buf);
      int i, first = gui->playlist.num; /* current count of entries in playlist */

      add = buf + strlcpy (buf, path, e - buf - 2);
      if (add > e - 3)
        add = e - 3;
      if (add[-1] != '/')
        *add++ = '/';
      for (i = 0; files[i]; i++) {
        strlcpy (add, files[i], e - add);
        free (files[i]);
        /* If the file has an extension which could be a playlist, attempt to append
           it to the current list as a list; otherwise, append it as an ordinary file. */
        gui_playlist_add_item (gui, buf, 1, GUI_ITEM_TYPE_AUTO, 0);
      }

      playlist_update_playlist (gui);

      /* Enable playback controls on display */
      if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
        enable_playback_controls (gui->panel, 1);

      /* If an MRL is not being played, select the first file appended. If in "smart mode" start
         playing the entry.  If an MRL is currently being played, let it continue normally */
      if((first != gui->playlist.num) && (xine_get_status(gui->stream) == XINE_STATUS_STOP)) {
        gui->playlist.cur = first;
        if(gui->smart_mode) {
          gui_current_set_index (gui, GUI_MMK_CURRENT);
          gui_play (NULL, gui);
        }
      }
    }
    free (files);
  }
  free (path);

  gui->load_stream = NULL;
}


void gui_file_selector (gGui_t *gui) {
  filebrowser_callback_button_t  cbb[3];

  gui->nongui_error_msg = NULL;

  if (gui->load_stream)
    filebrowser_raise_window (gui->load_stream);
  else {
    cbb[0].label = _("Select");
    cbb[0].callback = fileselector_callback;
    cbb[0].userdata = gui;
    cbb[0].need_a_file = 0;
    cbb[1].label = _("Select all");
    cbb[1].callback = fileselector_all_callback;
    cbb[1].userdata = gui;
    cbb[1].need_a_file = 0;
    cbb[2].callback = fileselector_cancel_callback;
    cbb[2].userdata = gui;
    cbb[2].need_a_file = 0;
    gui->load_stream = create_filebrowser (gui, _("Stream(s) Loading"), gui->curdir, hidden_file_cb, gui, &cbb[0], &cbb[1], &cbb[2]);
  }
}

static void subselector_callback (filebrowser_t *fb, void *userdata) {
  gGui_t *gui = userdata;
  char *file;
  int ret;

  file = filebrowser_get_full_filename (fb);
  if (file) {
    mediamark_set_str_val (&gui->playlist.mmk[gui->playlist.cur], file, MMK_VAL_SUB);
    gui_current_set_index (gui, GUI_MMK_CURRENT);
    playlist_mrlident_toggle (gui);

    if (xine_get_status (gui->stream) == XINE_STATUS_PLAY) {
      int curpos;
      xine_close (gui->spu_stream);
      gui_messages_off (gui);
      ret = xine_open (gui->spu_stream, file);
      gui_messages_on (gui);
      if (ret)
        xine_stream_master_slave (gui->stream,
          gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
      if (gui_xine_get_pos_length (gui, gui->stream, &curpos, NULL, NULL)) {
        xine_stop (gui->stream);
        gui_set_current_position (gui, curpos);
      }
    }
    free(file);
  }

  gui->load_sub = NULL;
}

void gui_select_sub (gGui_t *gui) {
  if(gui->playlist.num) {
    if (gui->load_sub)
      filebrowser_raise_window (gui->load_sub);
    else {
      filebrowser_callback_button_t  cbb[2];
      mediamark_t *mmk;

      mmk = mediamark_get_current_mmk (gui);

      if(mmk) {
	char *path, *open_path;

	cbb[0].label = _("Select");
	cbb[0].callback = subselector_callback;
        cbb[0].userdata = gui;
	cbb[0].need_a_file = 1;
	cbb[1].callback = fileselector_cancel_callback;
        cbb[1].userdata = gui;
	cbb[1].need_a_file = 0;

	path = mmk->sub ? mmk->sub : mmk->mrl;

	if(mrl_look_like_file(path)) {
	  char *p;

	  open_path = strdup(path);

	  if(!strncasecmp(path, "file:", 5))
	    path += 5;

	  p = strrchr(open_path, '/');
	  if (p && strlen(p))
	    *p = '\0';
	}
	else
	  open_path = strdup(gui->curdir);

        gui->load_sub = create_filebrowser (gui, _("Pick a subtitle file"), open_path, hidden_file_cb, gui, &cbb[0], NULL, &cbb[1]);
	free(open_path);
      }
    }
  }
}

/*
 *
 */
void visual_anim_init (gGui_t *gui) {
  char *buffer;

  buffer = xitk_asprintf("%s/%s", XINE_VISDIR, "default.mpv");

  gui->visual_anim.mrls = (char **) malloc(sizeof(char *) * 3);
  gui->visual_anim.num_mrls = 0;

  if (buffer) {
    gui->visual_anim.mrls[gui->visual_anim.num_mrls++]   = buffer;
    gui->visual_anim.mrls[gui->visual_anim.num_mrls]     = NULL;
    gui->visual_anim.mrls[gui->visual_anim.num_mrls + 1] = NULL;
  }
}
void visual_anim_done (gGui_t *gui) {
  int i;

  for (i = 0; i < gui->visual_anim.num_mrls; i++) free(gui->visual_anim.mrls[i]);
  free(gui->visual_anim.mrls);
}
void visual_anim_add_animation (gGui_t *gui, char *mrl) {
  gui->visual_anim.mrls = (char **) realloc(gui->visual_anim.mrls, sizeof(char *) *
					     ((gui->visual_anim.num_mrls + 1) + 2));

  gui->visual_anim.mrls[gui->visual_anim.num_mrls++]   = strdup(mrl);
  gui->visual_anim.mrls[gui->visual_anim.num_mrls]     = NULL;
  gui->visual_anim.mrls[gui->visual_anim.num_mrls + 1] = NULL;
}
static int visual_anim_open_and_play(xine_stream_t *stream, const char *mrl) {
  if((!xine_open(stream, mrl)) || (!xine_play(stream, 0, 0)))
    return 0;

  return 1;
}
void visual_anim_play (gGui_t *gui) {
  if(gui->visual_anim.enabled == 2) {
    if (!visual_anim_open_and_play (gui->visual_anim.stream,
      gui->visual_anim.mrls[gui->visual_anim.current]))
      gui_handle_xine_error (gui, gui->visual_anim.stream,
        gui->visual_anim.mrls[gui->visual_anim.current]);
    gui->visual_anim.running = 1;
  }
}
void visual_anim_play_next (gGui_t *gui) {
  if(gui->visual_anim.enabled == 2) {
    gui->visual_anim.current++;
    if(gui->visual_anim.mrls[gui->visual_anim.current] == NULL)
      gui->visual_anim.current = 0;
    visual_anim_play (gui);
  }
}
void visual_anim_stop (gGui_t *gui) {
  if(gui->visual_anim.enabled == 2) {
    xine_stop(gui->visual_anim.stream);
    gui->visual_anim.running = 0;
  }
}
