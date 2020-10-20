/*
 * Copyright (C) 2000-2020 the xine project
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
#include "control.h"
#include "videowin.h"
#include "file_browser.h"
#include "skins.h"
#include "tvout.h"
#include "stdctl.h"
#include "download.h"
#include "errors.h"
#include "oxine/oxine.h"

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

void gui_reparent_all_windows (gGui_t *gui) {
  int                    i;
  static const struct {
    int                 (*visible)(void);
    void                (*reparent)(void);
  } _reparent[] = {
    { tvset_is_visible,         tvset_reparent },
    { NULL,                     NULL}
  };

  if (playlist_is_visible (gui))
    playlist_reparent (gui);
  if (panel_is_visible (gui->panel) > 1)
    panel_reparent (gui->panel);
  if (setup_is_visible (gui->setup))
    setup_reparent (gui->setup);
  if (mrl_browser_is_visible (gui->mrlb))
    mrl_browser_reparent (gui->mrlb);
  if (event_sender_is_visible (gui))
    event_sender_reparent (gui);
  if (help_is_visible (gui->help))
    help_reparent (gui->help);
  if (viewlog_is_visible (gui->viewlog))
    viewlog_reparent (gui->viewlog);
  if (kbedit_is_visible (gui->keyedit))
    kbedit_reparent (gui->keyedit);
  if (pplugin_is_visible (&gui->post_audio))
    pplugin_reparent (&gui->post_audio);
  if (pplugin_is_visible (&gui->post_video))
    pplugin_reparent (&gui->post_video);
  if (stream_infos_is_visible (gui->streaminfo))
    stream_infos_reparent (gui->streaminfo);
  control_reparent (gui->vctrl);

  for(i = 0; _reparent[i].visible; i++) {
    if(_reparent[i].visible())
      _reparent[i].reparent();
  }

}

void wait_for_window_visible(xitk_window_t *xwin) {
  int t = 0;

  while ((!xitk_window_is_window_visible(xwin)) && (++t < 3))
    xine_usec_sleep(5000);
}

void reparent_window (gGui_t *gui, xitk_window_t *xwin) {
  if (!gui || !xwin)
    return;
  if ((!(video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE)) && !(xitk_get_wm_type (gui->xitk) & WM_TYPE_EWMH_COMP)) {
    /* Don't unmap this window, because on re-mapping, it will not be visible until
     * its ancestor, the video window, is visible. That's not what's intended. */
    xitk_window_set_wm_window_type (xwin, video_window_is_visible (gui->vwin) < 2 ? WINDOW_TYPE_NORMAL : WINDOW_TYPE_NONE);
    xitk_window_show_window (xwin, 1);
    xitk_window_try_to_set_input_focus (xwin);
    video_window_set_transient_for (gui->vwin, xwin);
  } else {
    xitk_window_show_window (xwin, 1);
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
      if (xitk_window_is_window_visible(xwin))
        xitk_window_iconify_window(xwin);
      else
        xitk_window_show_window(xwin, 0);
    }
    else if (!xitk_window_is_window_visible(xwin)) {
      /* Obviously user has iconified the window, let it be */
    }
    else {
      *visible = 0;
      xitk_hide_widgets(widget_list);
      xitk_window_hide_window(xwin);
    }
  }
  else {
    if(running) {
      *visible = 1;
      xitk_show_widgets(widget_list);

      xitk_window_show_window(xwin, 1);
      video_window_set_transient_for (gui->vwin, xwin);

      wait_for_window_visible(xwin);
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

  if(gui->visual_anim.running)
    visual_anim_stop();

  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -1);
  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, -1);
  panel_update_channel_display (gui->panel);

  if(gui->display_logo)
    (void) gui_xine_open_and_play((char *)gui->logo_mrl, NULL, 0, 0, 0, 0, 1);

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

      if(update_mmk && ((ident = stream_infos_get_ident_from_stream(stream)) != NULL)) {
        pthread_mutex_lock (&gui->mmk_mutex);
	free(gui->mmk.ident);
	free(gui->playlist.mmk[gui->playlist.cur]->ident);

	gui->mmk.ident = strdup(ident);
	gui->playlist.mmk[gui->playlist.cur]->ident = strdup(ident);
        pthread_mutex_unlock (&gui->mmk_mutex);

        video_window_set_mrl (gui->vwin, ident);
        playlist_mrlident_toggle (gui);
	panel_update_mrl_display (gui->panel);
	free(ident);
      }

      if(has_video) {

	if((gui->visual_anim.enabled == 2) && gui->visual_anim.running)
	  visual_anim_stop();

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
	  if(!gui->visual_anim.running)
	    visual_anim_play();
	}
	else
	  gui->visual_anim.running = 2;
      }

      xine_usec_sleep(100);
      if(!already_playing)
	osd_update_status();
    }
  }

  return ret;
}

static void set_mmk(mediamark_t *mmk) {
  gGui_t *gui = gGui;

  pthread_mutex_lock (&gui->mmk_mutex);
  free(gui->mmk.mrl);
  free(gui->mmk.ident);
  free(gui->mmk.sub);
  if(mediamark_have_alternates(&(gui->mmk)))
    mediamark_free_alternates(&(gui->mmk));

  if(mmk) {
    gui->mmk.mrl           = strdup(mmk->mrl);
    gui->mmk.ident         = strdup(((mmk->ident) ? mmk->ident : mmk->mrl));
    gui->mmk.sub           = mmk->sub ? strdup(mmk->sub): NULL;
    gui->mmk.start         = mmk->start;
    gui->mmk.end           = mmk->end;
    gui->mmk.av_offset     = mmk->av_offset;
    gui->mmk.spu_offset    = mmk->spu_offset;
    gui->mmk.got_alternate = mmk->got_alternate;
    mediamark_duplicate_alternates(mmk, &(gui->mmk));
  } else {
    /* TRANSLATORS: only ASCII characters (skin) */
    gui->mmk.mrl           = strdup(pgettext("skin", "There is no MRL."));
    gui->mmk.ident         = strdup ("xine-ui version " VERSION);
    gui->mmk.sub           = NULL;
    gui->mmk.start         = 0;
    gui->mmk.end           = -1;
    gui->mmk.av_offset     = 0;
    gui->mmk.spu_offset    = 0;
    gui->mmk.got_alternate = 0;
    gui->mmk.alternates    = NULL;
    gui->mmk.cur_alt       = NULL;
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}

static void mmk_set_update(void) {
  gGui_t *gui = gGui;

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
      xine_error (gui, "%s%s%s", buffer ? buffer : "", v_info ? v_info : "", a_info ? a_info : "");
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

int gui_xine_open_and_play(char *_mrl, char *_sub, int start_pos,
			   int start_time, int av_offset, int spu_offset, int report_error) {
  gGui_t *gui = gGui;
  char *mrl = _mrl;
  int ret;

  if (gui->verbosity)
    printf("%s():\n\tmrl: '%s',\n\tsub '%s',\n\tstart_pos %d, start_time %d, av_offset %d, spu_offset %d.\n",
	   __func__, _mrl, (_sub) ? _sub : "NONE", start_pos, start_time, av_offset, spu_offset);

  if(!strncasecmp(mrl, "cfg:/", 5)) {
    config_mrl(mrl);
    gui_playlist_start_next (gui);
    return 1;
  }
  else if(/*(!strncasecmp(mrl, "ftp://", 6)) ||*/ (!strncasecmp(mrl, "dload:/", 7)))  {
    char        *url = mrl;
    download_t   download;

    if(!strncasecmp(mrl, "dload:/", 7))
      url = _mrl + 7;

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
	  char  *sub = NULL;
	  int    start, end;

	  char *newmrl = strdup(fullfilename);
	  char *ident = strdup(gui->playlist.mmk[gui->playlist.cur]->ident);
	  if(gui->playlist.mmk[gui->playlist.cur]->sub)
	    sub = strdup(gui->playlist.mmk[gui->playlist.cur]->sub);
	  start = gui->playlist.mmk[gui->playlist.cur]->start;
	  end = gui->playlist.mmk[gui->playlist.cur]->end;

	  fwrite(download.buf, download.size, 1, fd);
	  fflush(fd);
	  fclose(fd);

	  mediamark_replace_entry(&gui->playlist.mmk[gui->playlist.cur],
				  newmrl, ident, sub, start, end, 0, 0);
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  mrl = gui->mmk.mrl;

	  free(newmrl);
	  free(ident);
	  free(sub);
	}
      }
    }

    free(download.buf);
    free(download.error);

  }

  if(mrl_look_like_playlist(mrl)) {
    if(mediamark_concat_mediamarks(mrl)) {
      gui_set_current_mmk(mediamark_get_current_mmk());
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

int gui_open_and_play_alternates(mediamark_t *mmk, const char *sub) {
  gGui_t *gui = gGui;
  char *alt;

  if(!(alt = mediamark_get_current_alternate_mrl(mmk)))
    alt = mediamark_get_first_alternate_mrl(mmk);

  do {

    if(gui_xine_open_and_play(alt, gui->mmk.sub, 0, 0, 0, 0, 0))
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
  tvset_end();
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

#ifdef HAVE_XF86VIDMODE
  /* just in case a different modeline than the original one is running,
   * toggle back to window mode which automatically causes a switch back to
   * the original modeline
   */
  if(gui->XF86VidMode_fullscreen)
    video_window_set_fullscreen_mode (gui->vwin, WINDOWED_MODE);
  //     gui_set_fullscreen_mode (NULL, gui);
#endif

  osd_deinit();

  config_update_num("gui.amp_level", gui->mixer.amp_level);
  xine_config_save (gui->xine, __xineui_global_config_file);

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

#ifdef HAVE_LIRC
  if(__xineui_global_lirc_enable)
    lirc_stop();
#endif

  if (gui->stdctl_enable)
    stdctl_stop (gui);

  xitk_skin_unload_config(gui->skin_config);

  mediamark_free_mediamarks();

  if (gui->icon)
    xitk_image_destroy_xitk_pixmap(gui->icon);
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
      xine_error (gui, _("No MRL (input stream) specified"));
      return;
    }

    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
			       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset,
			       !mediamark_have_alternates(&(gui->mmk)))) {

      if(!mediamark_have_alternates(&(gui->mmk)) ||
	 !gui_open_and_play_alternates(&(gui->mmk), gui->mmk.sub)) {

	if(mediamark_all_played() && (gui->actions_on_start[0] == ACTID_QUIT)) {
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
      osd_update_status();
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

  mediamark_reset_played_state();
  if(gui->visual_anim.running) {
    xine_stop(gui->visual_anim.stream);
    if(gui->visual_anim.enabled == 2)
      gui->visual_anim.running = 0;
  }

  osd_hide_sinfo();
  osd_hide_bar();
  osd_hide_info();
  osd_update_status();
  panel_reset_slider (gui->panel);
  panel_check_pause (gui->panel);
  panel_update_runtime_display (gui->panel);

  if (is_playback_widgets_enabled (gui->panel)) {
    if(!gui->playlist.num) {
      gui_set_current_mmk(NULL);
      enable_playback_controls (gui->panel, 0);
    }
    else if(gui->playlist.num && (strcmp((mediamark_get_current_mrl()), gui->mmk.mrl))) {
      gui_set_current_mmk(mediamark_get_current_mmk());
    }
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

  mediamark_reset_played_state();
  if(gui->visual_anim.running) {
    xine_stop(gui->visual_anim.stream);
    if(gui->visual_anim.enabled == 2)
      gui->visual_anim.running = 0;
  }

  osd_hide_sinfo();
  osd_hide_bar();
  osd_hide_info();
  osd_update_status();
  panel_reset_slider (gui->panel);
  panel_check_pause (gui->panel);
  panel_update_runtime_display (gui->panel);

  if (is_playback_widgets_enabled (gui->panel)) {
    if(!gui->playlist.num) {
      gui_set_current_mmk(NULL);
      enable_playback_controls (gui->panel, 0);
    }
    else if(gui->playlist.num && (strcmp((mediamark_get_current_mrl()), gui->mmk.mrl))) {
      gui_set_current_mmk(mediamark_get_current_mmk());
    }
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
  osd_update_status();
}

void gui_eject (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  int i;

  (void)w;
  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY)
    gui_stop (NULL, gui);

  if(xine_eject(gui->stream)) {

    if(gui->playlist.num) {
      mediamark_t **mmk = NULL;
      char         *mrl;
      size_t        tok_len = 0;
      int           new_num = 0;

      const char *const cur_mrl = mediamark_get_current_mrl();
      /*
       * If it's an mrl (____://) remove all of them in playlist
       */
      if ( (mrl = strstr((cur_mrl), ":/")) )
	tok_len = (mrl - cur_mrl) + 2;

      if(tok_len != 0) {
	/*
	 * Store all of not maching entries
	 */
	for(i = 0; i < gui->playlist.num; i++) {
	  if(strncasecmp(gui->playlist.mmk[i]->mrl, cur_mrl, tok_len)) {

	    mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (new_num + 2));

	    (void) mediamark_store_mmk(&mmk[new_num],
				       gui->playlist.mmk[i]->mrl,
				       gui->playlist.mmk[i]->ident,
				       gui->playlist.mmk[i]->sub,
				       gui->playlist.mmk[i]->start,
				       gui->playlist.mmk[i]->end,
				       gui->playlist.mmk[i]->av_offset,
				       gui->playlist.mmk[i]->spu_offset);
	    new_num++;
	  }
	}

	/*
	 * flip playlists.
	 */
	mediamark_free_mediamarks();
	if(mmk)
	  gui->playlist.mmk = mmk;

	if(!(gui->playlist.num = new_num))
	  gui->playlist.cur = -1;
	else if(new_num)
	  gui->playlist.cur = 0;
      }
      else {
      __remove_current_mrl:
	/*
	 * Remove only the current MRL
	 */
        pthread_mutex_lock(&gui->mmk_mutex);
        mediamark_delete_entry(gui->playlist.cur);
	if(gui->playlist.cur)
	  gui->playlist.cur--;
        pthread_mutex_unlock(&gui->mmk_mutex);
      }

      if (is_playback_widgets_enabled (gui->panel) && (!gui->playlist.num))
        enable_playback_controls (gui->panel, 0);

    }

    gui_set_current_mmk(mediamark_get_current_mmk());
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

void gui_toggle_visibility (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui->use_root_window) {
    int visible = video_window_is_visible (gui->vwin) < 2;

    video_window_set_visibility (gui->vwin, visible);

    /* We need to reparent all visible windows because of redirect tweaking */
    gui_reparent_all_windows (gui);

    /* (re)start/stop visual animation */
    if (video_window_is_visible (gui->vwin) > 1) {
      if(gui->visual_anim.enabled && (gui->visual_anim.running == 2))
	visual_anim_play();
    }
    else {

      if(gui->visual_anim.running) {
	visual_anim_stop();
	gui->visual_anim.running = 2;
      }
    }
  }
}

void gui_toggle_border (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  video_window_toggle_border (gui->vwin);
}

int gui_hide_show_all (gGui_t *gui, int flags_mask, int flags_visible) {
  int v, s;

  if (!gui)
    return 0;

  v = (panel_is_visible (gui->panel) > 1) << 0;
  s = (v ^ flags_visible) & flags_mask;
  if (s & (1 << 0)) {
    /* this will call us again on the remaining windows, ... */
    panel_toggle_visibility (NULL, gui->panel);
    /* ... so stop right here. */
    return v;
  }

  v |= (mrl_browser_is_visible (gui->mrlb) != 0) << 1;
  v |= (playlist_is_visible (gui) != 0) << 2;
  v |= (control_status (gui->vctrl) == 3) << 3;
  v |= (setup_is_visible (gui->setup) != 0) << 4;
  v |= (viewlog_is_visible (gui->viewlog) != 0) << 5;
  v |= (kbedit_is_visible (gui->keyedit) != 0) << 6;
  v |= (event_sender_is_visible (gui) != 0) << 7;
  v |= (stream_infos_is_visible (gui->streaminfo) != 0) << 8;
  v |= (tvset_is_visible () != 0) << 9;
  v |= (pplugin_is_visible (&gui->post_video) != 0) << 10;
  v |= (pplugin_is_visible (&gui->post_audio) != 0) << 11;
  v |= (help_is_visible (gui->help) != 0) << 12;

  s = (v ^ flags_visible) & flags_mask;
  if (s & (1 << 1))
    mrl_browser_toggle_visibility (NULL, gui->mrlb);
  if (s & (1 << 2))
    playlist_toggle_visibility (gui);
  if (s & (1 << 3))
    control_toggle_visibility (gui->vctrl);
  if (s & (1 << 4))
    setup_toggle_visibility (gui->setup);
  if (s & (1 << 5))
    viewlog_toggle_visibility (gui->viewlog);
  if (s & (1 << 6))
    kbedit_toggle_visibility (NULL, gui->keyedit);
  if (s & (1 << 7))
    event_sender_toggle_visibility (gui);
  if (s & (1 << 8))
    stream_infos_toggle_visibility (NULL, gui->streaminfo);
  if (s & (1 << 9))
    tvset_toggle_visibility (NULL, NULL);
  if (s & (1 << 10))
    pplugin_toggle_visibility (NULL, &gui->post_video);
  if (s & (1 << 11))
    pplugin_toggle_visibility (NULL, &gui->post_audio);
  if (s & (1 << 12))
    help_toggle_visibility (gui->help);

  return v;
}

static void set_fullscreen_mode(int fullscreen_mode) {
  gGui_t *gui = gGui;
  int flags;

  if ((video_window_is_visible (gui->vwin) < 2) || gui->use_root_window)
    return;

  flags = gui_hide_show_all (gui, ~0, 0);

  video_window_set_fullscreen_mode (gui->vwin, fullscreen_mode);
  /* Drawable has changed, update cursor visiblity */
  if(!gui->cursor_visible)
    video_window_set_cursor_visibility (gui->vwin, gui->cursor_visible);

  gui_hide_show_all (gui, flags, ~0);
}

void gui_set_fullscreen_mode (xitk_widget_t *w, void *data) {
  (void)w;
  (void)data;
  set_fullscreen_mode(FULLSCR_MODE);
}

#ifdef HAVE_XINERAMA
void gui_set_xinerama_fullscreen_mode(void) {
  set_fullscreen_mode(FULLSCR_XI_MODE);
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

  osd_display_info(_("Aspect ratio: %s"),
		   ratios[xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO)]);

  panel_raise_window(gui->panel);
}

void gui_toggle_interlaced (gGui_t *gui) {
  if (!gui)
    return;
  gui->deinterlace_enable = !gui->deinterlace_enable;
  osd_display_info(_("Deinterlace: %s"), (gui->deinterlace_enable) ? _("enabled") : _("disabled"));
  post_deinterlace (gui);
  panel_raise_window(gui->panel);
}

void gui_direct_change_audio_channel (xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = data;
  (void)w;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, value);
  panel_update_channel_display (gui->panel);
  osd_display_audio_lang();
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
  osd_display_spu_lang();
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
  osd_update_status();
}

static void *gui_seek_thread (void *data) {
  gGui_t *gui = data;
  int update_mmk = 0, ret;

  pthread_detach (pthread_self ());

  do {
    {
      const char *mrl = mediamark_get_current_mrl ();
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

    if ( gui->playlist.num &&
        (gui->playlist.cur >= 0) &&
         gui->playlist.mmk &&
         gui->playlist.mmk[gui->playlist.cur] &&
        !strcmp (gui->playlist.mmk[gui->playlist.cur]->mrl, gui->mmk.mrl))
      update_mmk = 1;

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
          osd_stream_position (pos);
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
              osd_stream_position (gui->stream_length.pos);
            }
            gui->ignore_next = 0;
          }

        }
      }
    }

    gui->ignore_next = 0;
    osd_hide_status ();
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

void gui_set_current_position (int pos) {
  gGui_t *gui = gGui;

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

void gui_seek_relative (int off_sec) {
  gGui_t *gui = gGui;

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

void gui_dndcallback(const char *filename) {
  gGui_t *gui = gGui;
  int   more_than_one = -2;
  char *mrl           = filename ? strdup(filename) : NULL;

  if(mrl) {
    char  buffer[strlen(mrl) + 10];
    char  buffer2[strlen(mrl) + 10];
    char *p;

    memset(&buffer, 0, sizeof(buffer));
    memset(&buffer2, 0, sizeof(buffer2));

    pthread_mutex_lock(&gui->mmk_mutex);

    if((strlen(mrl) > 6) &&
       (!strncmp(mrl, "file:", 5))) {

      if((p = strstr(mrl, ":/")) != NULL) {
	struct stat pstat;

	p += 2;

	if(*(p + 1) == '/')
	  p++;

      __second_stat:

	if((stat(p, &pstat)) == 0) {
	  if(is_a_dir(p)) {
	    if(*(p + (strlen(p) - 1)) == '/')
	      *(p + (strlen(p) - 1)) = '\0';

	    mediamark_collect_from_directory(p);
	    more_than_one = gui->playlist.cur;
	    goto __do_play;
	  }
	  else
	    snprintf(buffer, sizeof(buffer), "file:/%s", p);
	}
	else {
	  snprintf(buffer2, sizeof(buffer2), "/%s", p);

	  /* file don't exist, add it anyway */
	  if((stat(buffer2, &pstat)) == -1)
	    strlcpy(buffer, mrl, sizeof(buffer));
	  else {
	    if(is_a_dir(buffer2)) {

	      if(buffer2[strlen(buffer2) - 1] == '/')
		buffer2[strlen(buffer2) - 1] = '\0';

	      mediamark_collect_from_directory(buffer2);
	      more_than_one = gui->playlist.cur;
	      goto __do_play;
	    }
	    else
	      snprintf(buffer, sizeof(buffer), "file:/%s", buffer2);
	  }

	}
      }
      else {
	p = mrl + 5;
	goto __second_stat;
      }
    }
    else
      strlcpy(buffer, mrl, sizeof(buffer));

    if(is_a_dir(buffer)) {
      if(buffer[strlen(buffer) - 1] == '/')
	buffer[strlen(buffer) - 1] = '\0';

      mediamark_collect_from_directory(buffer);
      more_than_one = gui->playlist.cur;
    }
    else {
      char *ident;

      /* If possible, use only base name as identifier to better fit into the display */
      if((ident = strrchr(buffer, '/')) && ident[1])
	ident++;
      else
	ident = buffer;

      if(mrl_look_like_playlist(buffer)) {
	int cur = gui->playlist.cur;

	more_than_one = (gui->playlist.cur - 1);
	if(mediamark_concat_mediamarks(buffer))
	  gui->playlist.cur = cur;
	else
	  mediamark_append_entry(buffer, ident, NULL, 0, -1, 0, 0);
      }
      else
	mediamark_append_entry(buffer, ident, NULL, 0, -1, 0, 0);

    }

  __do_play:

    playlist_update_playlist (gui);

    if(!(gui->playlist.control & PLAYLIST_CONTROL_IGNORE)) {

      if((xine_get_status(gui->stream) == XINE_STATUS_STOP) || gui->logo_mode) {
	if((more_than_one > -2) && ((more_than_one + 1) < gui->playlist.num))
	  gui->playlist.cur = more_than_one + 1;
	else
	  gui->playlist.cur = gui->playlist.num - 1;

        set_mmk(mediamark_get_current_mmk());
        mmk_set_update();
	if(gui->smart_mode)
          gui_play (NULL, gui);

      }
    }

    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    pthread_mutex_unlock(&gui->mmk_mutex);
  }

  free(mrl);
}

void gui_step_mrl (gGui_t *gui, int by) {
  int i, by_chapter;
  mediamark_t *mmk;

  if (!gui)
    return;
  mmk = mediamark_get_current_mmk ();
  by_chapter = (gui->skip_by_chapter &&
		(xine_get_stream_info(gui->stream, XINE_STREAM_INFO_HAS_CHAPTERS))) ? 1 : 0;

  if(mmk && mediamark_got_alternate(mmk))
    mediamark_unset_got_alternate(mmk);

  if (by > 0) { /* next */

    osd_hide();

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
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
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

    osd_hide();
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
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
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
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
				       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
              gui_display_logo (gui);
	  }
	  else
	    gui->playlist.cur = 0;

	}
	else {
          int newcur = (gui->playlist.cur - by) + gui->playlist.num;

	  gui->playlist.cur = newcur;
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0,
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
  int by;
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

void gui_set_current_mmk(mediamark_t *mmk) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&gui->mmk_mutex);
  set_mmk(mmk);
  pthread_mutex_unlock(&gui->mmk_mutex);

  mmk_set_update();
}

void gui_set_current_mmk_by_index(int idx) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&gui->mmk_mutex);
  set_mmk(mediamark_get_mmk_by_index(idx));
  gui->playlist.cur = idx;
  pthread_mutex_unlock(&gui->mmk_mutex);

  mmk_set_update();
}

void gui_control_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  if (gui)
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

void gui_tvset_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (!gui)
    return;
  if (tvset_is_running() && !tvset_is_visible())
    tvset_toggle_visibility(NULL, NULL);
  else if(!tvset_is_running())
    tvset_panel();
  else {
    if(gui->use_root_window)
      tvset_toggle_visibility(NULL, NULL);
    else
      tvset_end();
  }
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
    osd_display_info(_("Video post plugins: %s."), (gui->post_video_enable) ? _("enabled") : _("disabled"));
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
    layer = xitk_get_layer_level();
  }
  else {
    if (is_layer_above (gui))
      layer = xitk_get_layer_level();
    else
      /* FIXME: never happens? */ layer = 4;
  }

  xitk_window_set_window_layer(xwin, layer);
}

int get_layer_above_video (gGui_t *gui) {
  if (!(gui->always_layer_above || gui->layer_above))
    return 0;
  if ((!(video_window_get_fullscreen_mode (gui->vwin) & WINDOWED_MODE)) && video_window_is_visible (gui->vwin) > 1)
    return xitk_get_layer_level ();
  if (gui->always_layer_above || gui->layer_above)
    return xitk_get_layer_level ();
  return 4;
}

void change_amp_vol(int value) {
  gGui_t *gui = gGui;
  if(value < 0)
    value = 0;
  if(value > 200)
    value = 200;
  gui->mixer.amp_level = value;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL, gui->mixer.amp_level);
  panel_update_mixer_display (gui->panel);
  osd_draw_bar(_("Amplification Level"), 0, 200, gui->mixer.amp_level, OSD_BAR_STEPPER);
}
void gui_increase_amp_level(void) {
  gGui_t *gui = gGui;
  change_amp_vol((gui->mixer.amp_level + 1));
}
void gui_decrease_amp_level(void) {
  gGui_t *gui = gGui;
  change_amp_vol((gui->mixer.amp_level - 1));
}
void gui_reset_amp_level(void) {
  change_amp_vol(100);
}

void change_audio_vol(int value) {
  gGui_t *gui = gGui;
  if(value < 0)
    value = 0;
  if(value > 100)
    value = 100;
  gui->mixer.volume_level = value;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
  panel_update_mixer_display (gui->panel);
  osd_draw_bar(_("Audio Volume"), 0, 100, gui->mixer.volume_level, OSD_BAR_STEPPER);
}
void gui_increase_audio_volume(void) {
  gGui_t *gui = gGui;
  if((gui->mixer.method == SOUND_CARD_MIXER) && (gui->mixer.caps & MIXER_CAP_VOL))
    change_audio_vol((gui->mixer.volume_level + 1));
  else if(gui->mixer.method == SOFTWARE_MIXER)
    gui_increase_amp_level();
}
void gui_decrease_audio_volume(void) {
  gGui_t *gui = gGui;
  if((gui->mixer.method == SOUND_CARD_MIXER) && (gui->mixer.caps & MIXER_CAP_VOL))
    change_audio_vol((gui->mixer.volume_level - 1));
  else if(gui->mixer.method == SOFTWARE_MIXER)
    gui_decrease_amp_level();
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
    osd_display_info (_("Audio post plugins: %s."), (gui->post_audio_enable) ? _("enabled") : _("disabled"));
    pplugin_update_enable_button (&gui->post_audio);
    if (pplugin_is_visible (&gui->post_audio))
      pplugin_rewire_from_posts_window (&gui->post_audio);
    else
      pplugin_rewire_posts (&gui->post_audio);
  }
}

void gui_change_zoom(int zoom_dx, int zoom_dy) {
  gGui_t *gui = gGui;

  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_X,
		 xine_get_param(gui->stream, XINE_PARAM_VO_ZOOM_X) + zoom_dx);
  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_Y,
		 xine_get_param(gui->stream, XINE_PARAM_VO_ZOOM_Y) + zoom_dy);

  panel_raise_window(gui->panel);
}

/*
 * Reset zooming by recall aspect ratio.
 */
void gui_reset_zoom(void) {
  gGui_t *gui = gGui;

  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_X, 100);
  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_Y, 100);

  panel_raise_window(gui->panel);
}

/*
 * Toggle TV Modes on the dxr3
 */
void gui_toggle_tvmode(void) {
  gGui_t *gui = gGui;

  xine_set_param(gui->stream, XINE_PARAM_VO_TVMODE,
		 xine_get_param(gui->stream, XINE_PARAM_VO_TVMODE) + 1);

  osd_display_info(_("TV Mode: %d"), xine_get_param(gui->stream, XINE_PARAM_VO_TVMODE));
}

void gui_add_mediamark(void) {
  gGui_t *gui = gGui;

  if((gui->logo_mode == 0) && (xine_get_status(gui->stream) == XINE_STATUS_PLAY)) {
    int secs;

    if (gui_xine_get_pos_length (gui, gui->stream, NULL, &secs, NULL)) {
      secs /= 1000;

      mediamark_append_entry(gui->mmk.mrl, gui->mmk.ident,
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
      config_update_string("media.files.origin_path", gui->curdir);
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
    config_update_string("media.files.origin_path", gui->curdir);
  }
  free(cur_dir);

  /* Get the file path/name */
  if(((file = filebrowser_get_full_filename(fb)) != NULL) && strlen(file)) {
    int first  = gui->playlist.num;
    char *ident;

    /* Get the name only to use as an identifier in the playlist display */
    ident = filebrowser_get_current_filename(fb);

    /* If the file has an extension which could be a playlist, attempt to append
       it to the current list as a list; otherwise, append it as an ordinary file. */
    if(mrl_look_like_playlist(file)) {
      if(!mediamark_concat_mediamarks(file))
        mediamark_append_entry(file, ident, NULL, 0, -1, 0, 0);
    }
    else
      mediamark_append_entry(file, ident, NULL, 0, -1, 0, 0);

    playlist_update_playlist (gui);
    free(ident);

    /* Enable controls on display */
    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    /* If an MRL is not being played, select the first file appended. If in "smart mode" start
       playing the entry.  If a an MRL is currently being played, let it continue normally */
    if((first != gui->playlist.num) &&
      (gui->logo_mode || (xine_get_status(gui->stream) == XINE_STATUS_STOP))) {
      gui->playlist.cur = first;
      if(gui->smart_mode) {
        gui_set_current_mmk(mediamark_get_current_mmk());
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
  char **files;
  char  *path = filebrowser_get_current_dir(fb);

  /* Update the configuration with the current path */
  if(path && strlen(path)) {
    strlcpy(gui->curdir, path, sizeof(gui->curdir));
    config_update_string("media.files.origin_path", gui->curdir);
  }

  /* Get all of the file names in the current directory as an array of pointers to strings */
  if((files = filebrowser_get_all_files(fb)) != NULL) {
    int i = 0;

    if(path && strlen(path)) {
      char pathname[XITK_PATH_MAX + 1 + 1]; /* +1 for trailing '/' */
      char fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 2];
      int  first = gui->playlist.num; /* current count of entries in playlist */

      /* If the path is anything other than "/", append a slash to it so that it can
         be concatenated with the file name */
      if(strcasecmp(path, "/"))
        snprintf(pathname, sizeof(pathname), "%s/", path);
      else
	strlcpy(pathname, path, sizeof(pathname));

      /* For each file, concatenate the path with the name and append it to the playlist */
      while(files[i]) {
        snprintf(fullfilename, sizeof(fullfilename), "%s%s", pathname, files[i]);

        /* If the file has an extension which could be a playlist, attempt to append
           it to the current list as a list; otherwise, append it as an ordinary file. */
        if(mrl_look_like_playlist(fullfilename)) {
          if(!mediamark_concat_mediamarks(fullfilename))
            mediamark_append_entry(fullfilename, files[i], NULL, 0, -1, 0, 0);
        }
        else
          mediamark_append_entry(fullfilename, files[i], NULL, 0, -1, 0, 0);

        i++;
      } /* End while */

      playlist_update_playlist (gui);

      /* Enable playback controls on display */
      if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
        enable_playback_controls (gui->panel, 1);

      /* If an MRL is not being played, select the first file appended. If in "smart mode" start
         playing the entry.  If an MRL is currently being played, let it continue normally */
      if((first != gui->playlist.num) && (xine_get_status(gui->stream) == XINE_STATUS_STOP)) {
        gui->playlist.cur = first;
        if(gui->smart_mode) {
          gui_set_current_mmk(mediamark_get_current_mmk());
          gui_play (NULL, gui);
        }
      }
    } /* If valid path */

    i = 0;
    while(files[i])
      free(files[i++]);

    free(files);
  } /* If valid file list */

  free(path);

  gui->load_stream = NULL;
}


void gui_file_selector(void) {
  gGui_t *gui = gGui;
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
    gui->load_stream = create_filebrowser(_("Stream(s) Loading"), gui->curdir, hidden_file_cb, &cbb[0], &cbb[1], &cbb[2]);
  }
}

static void subselector_callback (filebrowser_t *fb, void *userdata) {
  gGui_t *gui = userdata;
  char *file;
  int ret;

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    if(file) {
      mediamark_t *mmk = mediamark_clone_mmk(mediamark_get_current_mmk());

      if(mmk) {
	mediamark_replace_entry(&gui->playlist.mmk[gui->playlist.cur], mmk->mrl, mmk->ident,
				file, mmk->start, mmk->end, mmk->av_offset, mmk->spu_offset);
	mediamark_free_mmk(&mmk);

	mmk = mediamark_get_current_mmk();
	gui_set_current_mmk(mmk);

        playlist_mrlident_toggle (gui);

	if(xine_get_status(gui->stream) == XINE_STATUS_PLAY) {
	  int curpos;
	  xine_close (gui->spu_stream);

	  gui_messages_off (gui);
	  ret = xine_open(gui->spu_stream, mmk->sub);
	  gui_messages_on (gui);
	  if (ret)
	    xine_stream_master_slave(gui->stream,
				     gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);

          if (gui_xine_get_pos_length (gui, gui->stream, &curpos, NULL, NULL)) {
	    xine_stop(gui->stream);
	    gui_set_current_position(curpos);
	  }
	}
      }
    }
    free(file);
  }

  gui->load_sub = NULL;
}

void gui_select_sub(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num) {
    if (gui->load_sub)
      filebrowser_raise_window (gui->load_sub);
    else {
      filebrowser_callback_button_t  cbb[2];
      mediamark_t *mmk;

      mmk = mediamark_get_current_mmk();

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

        gui->load_sub = create_filebrowser(_("Pick a subtitle file"), open_path, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
	free(open_path);
      }
    }
  }
}

/*
 *
 */
void visual_anim_init(void) {
  gGui_t *gui = gGui;
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
void visual_anim_done(void) {
  gGui_t *gui = gGui;
  int i;

  for (i = 0; i < gui->visual_anim.num_mrls; i++) free(gui->visual_anim.mrls[i]);
  free(gui->visual_anim.mrls);
}
void visual_anim_add_animation(char *mrl) {
  gGui_t *gui = gGui;
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
void visual_anim_play(void) {
  gGui_t *gui = gGui;
  if(gui->visual_anim.enabled == 2) {
    if (!visual_anim_open_and_play (gui->visual_anim.stream,
      gui->visual_anim.mrls[gui->visual_anim.current]))
      gui_handle_xine_error (gui, gui->visual_anim.stream,
        gui->visual_anim.mrls[gui->visual_anim.current]);
    gui->visual_anim.running = 1;
  }
}
void visual_anim_play_next(void) {
  gGui_t *gui = gGui;
  if(gui->visual_anim.enabled == 2) {
    gui->visual_anim.current++;
    if(gui->visual_anim.mrls[gui->visual_anim.current] == NULL)
      gui->visual_anim.current = 0;
    visual_anim_play();
  }
}
void visual_anim_stop(void) {
  gGui_t *gui = gGui;
  if(gui->visual_anim.enabled == 2) {
    xine_stop(gui->visual_anim.stream);
    gui->visual_anim.running = 0;
  }
}

