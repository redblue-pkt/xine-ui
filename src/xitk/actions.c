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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <X11/Xlib.h>
#include <xine/xineutils.h>
#include <pthread.h>

#include "common.h"
#include "oxine/oxine.h"

filebrowser_t          *load_stream = NULL, *load_sub = NULL;

static pthread_mutex_t new_pos_mutex =  PTHREAD_MUTEX_INITIALIZER;

static int             last_playback_speed = XINE_SPEED_NORMAL;

void reparent_all_windows(void) {
  int                    i;
  static const struct {
    int                 (*visible)(void);
    void                (*reparent)(void);
  } _reparent[] = {
    { mrl_browser_is_visible,   mrl_browser_reparent },
    { playlist_is_visible,      playlist_reparent },
    { control_is_visible,       control_reparent },
    { setup_is_visible,         setup_reparent },
    { viewlog_is_visible,       viewlog_reparent },
    { kbedit_is_visible,        kbedit_reparent },
    { event_sender_is_visible,  event_sender_reparent },
    { stream_infos_is_visible,  stream_infos_reparent },
    { tvset_is_visible,         tvset_reparent },
    { vpplugin_is_visible,      vpplugin_reparent },
    { applugin_is_visible,      applugin_reparent },
    { help_is_visible,          help_reparent },
    { NULL,                     NULL}
  };
  
  if (panel_is_visible (gGui->panel))
    panel_reparent (gGui->panel);

  for(i = 0; _reparent[i].visible; i++) {
    if(_reparent[i].visible())
      _reparent[i].reparent();
  }

}

void wait_for_window_visible(Display *display, Window window) {
  int t = 0;

  while((!xitk_is_window_visible(display, window)) && (++t < 3))
    xine_usec_sleep(5000);
}

void raise_window(Window window, int visible, int running) {
  if(window) {
    if(visible && running) {
      reparent_window(window);
      layer_above_video(window);
    }
  }
}

void toggle_window(Window window, xitk_widget_list_t *widget_list, int *visible, int running) {
  gGui_t *gui = gGui;
  if(window && (*visible) && running) {

    XLockDisplay(gui->display);
    if(gui->use_root_window) {
      if(xitk_is_window_visible(gui->display, window))
        XIconifyWindow(gui->display, window, gui->screen);
      else
	XMapWindow(gui->display, window);
    }
    else if(!xitk_is_window_visible(gui->display, window)) {
      /* Obviously user has iconified the window, let it be */
    }
    else {
      *visible = 0;
      XUnlockDisplay(gui->display);
      xitk_hide_widgets(widget_list);
      XLockDisplay(gui->display);
      XUnmapWindow(gui->display, window);
    }
    XUnlockDisplay(gui->display);

  }
  else {
    if(running) {
      *visible = 1;
      xitk_show_widgets(widget_list);

      XLockDisplay(gui->display);
      XRaiseWindow(gui->display, window);
      XMapWindow(gui->display, window);
      XUnlockDisplay(gui->display);
      video_window_set_transient_for (gui->vwin, window);

      wait_for_window_visible(gui->display, window);
      layer_above_video(window);
    }
  }
}

int gui_xine_get_pos_length(xine_stream_t *stream, int *pos, int *time, int *length) {
  gGui_t *gui = gGui;
  int t = 0, ret = 0;
  int lpos, ltime, llength;
  
  if(pthread_mutex_trylock(&gui->xe_mutex))
    return 0;
  
  if(stream && (xine_get_status(stream) == XINE_STATUS_PLAY)) {
    while(((ret = xine_get_pos_length(stream, &lpos, &ltime, &llength)) == 0) && (++t < 10) && (!gui->on_quit))
      xine_usec_sleep(100000); /* wait before trying again */
  }
  
  if(ret == 0) {
    lpos = ltime = llength = 0;
  }
  
  if(pos)
    *pos    = lpos;
  if(time)
    *time   = ltime;
  if(length)
    *length = llength;
  
  if((ret != 0) && (stream == gui->stream)) {
    gui->stream_length.pos = lpos;
    gui->stream_length.time = ltime;
    gui->stream_length.length = llength;
  }

  pthread_mutex_unlock(&gui->xe_mutex);
  return ret;
}

/*
 *
 */
void try_to_set_input_focus(Window window) {
  gGui_t *gui = gGui;

  wait_for_window_visible(gui->display, window);
  
  if(xitk_is_window_visible(gui->display, window)) {
    int    retry = 0;
    Window focused_win;

    do {
      int revert;

      XLockDisplay (gui->display);
      XSetInputFocus(gui->display, window, RevertToParent, CurrentTime);
      XSync(gui->display, False);
      XUnlockDisplay (gui->display);

      /* Retry until the WM was mercyful to give us the focus (but not indefinitely) */
      xine_usec_sleep(5000);
      XLockDisplay (gui->display);
      XGetInputFocus(gui->display, &focused_win, &revert);
      XUnlockDisplay (gui->display);

    } while((focused_win != window) && (retry++ < 30));
  }
}

/*
 *
 */
void gui_display_logo(void) {
  gGui_t *gui = gGui;

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
  if(stream_infos_is_visible())
    stream_infos_update_infos();
  
  pthread_mutex_unlock(&gui->logo_mutex);
}

static int _gui_xine_play(xine_stream_t *stream, 
			  int start_pos, int start_time_in_secs, int update_mmk) {
  gGui_t *gui = gGui;
  int      ret;
  int      has_video;
  int      already_playing = (gui->logo_mode == 0);
  
  if(gui->visual_anim.post_changed && (xine_get_status(stream) == XINE_STATUS_STOP)) {
    post_rewire_visual_anim();
    gui->visual_anim.post_changed = 0;
  }
  
  if(start_time_in_secs)
    start_time_in_secs *= 1000;
  
  has_video = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);
  if (has_video)
    has_video = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_VIDEO);

  if((has_video && gui->visual_anim.enabled == 1) && gui->visual_anim.running) {

    if(post_rewire_audio_port_to_stream(stream))
      gui->visual_anim.running = 0;

  } else if (!has_video && (gui->visual_anim.enabled == 1) && 
	     (gui->visual_anim.running == 0) && gui->visual_anim.post_output_element.post) {

    if(post_rewire_audio_post_to_stream(stream))
      gui->visual_anim.running = 1;

  }

  if((ret = xine_play(stream, start_pos, start_time_in_secs)) == 0)
    gui_handle_xine_error(stream, NULL);
  else {
    char *ident;

    pthread_mutex_lock(&gui->logo_mutex);
    if(gui->logo_mode != 2)
      gui->logo_mode = 0;
    pthread_mutex_unlock(&gui->logo_mutex);
    
    if(gui->logo_mode == 0) {
     
      if(stream_infos_is_visible())
	stream_infos_update_infos();
      
      if(update_mmk && ((ident = stream_infos_get_ident_from_stream(stream)) != NULL)) {
	free(gui->mmk.ident);
	free(gui->playlist.mmk[gui->playlist.cur]->ident);
	
	gui->mmk.ident = strdup(ident);
	gui->playlist.mmk[gui->playlist.cur]->ident = strdup(ident);
	
        video_window_set_mrl (gui->vwin, gui->mmk.ident);
	playlist_mrlident_toggle();
	panel_update_mrl_display (gui->panel);
	free(ident);
      }

      if(has_video) {
	
	if((gui->visual_anim.enabled == 2) && gui->visual_anim.running)
	  visual_anim_stop();
	
	if(gui->auto_vo_visibility) {
	  
          if (!video_window_is_visible (gui->vwin))
            video_window_set_visibility (gui->vwin, 1);
	  
	}

        if (gui->auto_panel_visibility && video_window_is_visible (gui->vwin) &&
          panel_is_visible (gui->panel))
          panel_toggle_visibility (NULL, gui->panel);
	
      }
      else {
	
	if(gui->auto_vo_visibility) {
	  
          if (!panel_is_visible (gui->panel))
            panel_toggle_visibility (NULL, gui->panel);

          if (video_window_is_visible (gui->vwin))
            video_window_set_visibility (gui->vwin, 0);
	    
	}

        if (gui->auto_panel_visibility && video_window_is_visible (gui->vwin) && 
          !panel_is_visible (gui->panel))
          panel_toggle_visibility (NULL, gui->panel);
	  
        if (video_window_is_visible (gui->vwin)) {
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

typedef struct {
  xine_stream_t *stream;
  int            start_pos;
  int            start_time_in_secs;
  int            update_mmk;
  int            running;
} play_data_t;
static play_data_t play_data;

static void start_anyway_yesno(xitk_widget_t *w, void *data, int button) {
  play_data.running = 0;

  if(button == XITK_WINDOW_ANSWER_YES)
    _gui_xine_play(play_data.stream, 
		   play_data.start_pos, play_data.start_time_in_secs, play_data.update_mmk);
  else
    gui_playlist_start_next();

}

static void set_mmk(mediamark_t *mmk) {
  gGui_t *gui = gGui;
  
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
  }
  else {
    char buffer[1024];
    
    snprintf(buffer, sizeof(buffer), "xine-ui version %s", VERSION);
    
    /* TRANSLATORS: only ASCII characters (skin) */
    gui->mmk.mrl           = strdup(pgettext("skin", "There is no MRL."));
    gui->mmk.ident         = strdup(buffer);
    gui->mmk.sub           = NULL;
    gui->mmk.start         = 0;
    gui->mmk.end           = -1;
    gui->mmk.av_offset     = 0;
    gui->mmk.spu_offset    = 0;
    gui->mmk.got_alternate = 0;
    gui->mmk.alternates    = NULL;
    gui->mmk.cur_alt       = NULL;
  }
}

static void mmk_set_update(void) {
  gGui_t *gui = gGui;

  video_window_set_mrl (gui->vwin, gui->mmk.ident);
  event_sender_update_menu_buttons();
  panel_update_mrl_display (gui->panel);
  playlist_update_focused_entry();
  
  gui->playlist.ref_append = gui->playlist.cur;
}

int gui_xine_play(xine_stream_t *stream, int start_pos, int start_time_in_secs, int update_mmk) {
  gGui_t *gui = gGui;
  int has_video, has_audio, v_unhandled = 0, a_unhandled = 0;
  uint32_t video_handled, audio_handled;
  
  if(play_data.running)
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
      char        tmp[8];

      minfo = xine_get_meta_info(stream, XINE_META_INFO_VIDEOCODEC);
      vfcc = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_FOURCC);
      v_info = xitk_asprintf(_("Video Codec: %s (%s)\n"),
                             (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"),
                             (get_fourcc_string(tmp, sizeof(tmp), vfcc)));
    }
    
    if(a_unhandled) {
      const char *minfo;
      uint32_t    afcc;
      char        tmp[8];

      minfo = xine_get_meta_info(stream, XINE_META_INFO_AUDIOCODEC);
      afcc = xine_get_stream_info(stream, XINE_STREAM_INFO_AUDIO_FOURCC);
      a_info = xitk_asprintf(_("Audio Codec: %s (%s)\n"),
                             (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"),
                             (get_fourcc_string(tmp, sizeof(tmp), afcc)));
    }
    

    if(v_unhandled && a_unhandled) {
      xine_error("%s%s%s", buffer ? buffer : "", v_info ? v_info : "", a_info ? a_info : "");
      free(buffer); free(v_info); free(a_info);
      return 0;
    }

    if(!gui->play_anyway) {
      xitk_window_t *xw;

      play_data.stream             = stream;
      play_data.start_pos          = start_pos;
      play_data.start_time_in_secs = start_time_in_secs;
      play_data.update_mmk         = update_mmk;
      play_data.running            = 1;
      
      xw = xitk_window_dialog_yesno_with_width(gui->imlib_data, _("Start Playback ?"), 
					       start_anyway_yesno, start_anyway_yesno, 
					       NULL, 400, ALIGN_CENTER,
					       "%s%s%s%s", buffer ? buffer : "",
					       v_info ? v_info : "", a_info ? a_info : "",
					       _("\nStart playback anyway ?\n"));
      free(buffer); free(v_info); free(a_info);

      XLockDisplay(gui->display);
      XSync(gui->display, False);
      XUnlockDisplay(gui->display);
      video_window_set_transient_for (gui->vwin, xitk_window_get_window (xw));
      layer_above_video(xitk_window_get_window(xw));
      
      /* Doesn't work so well yet 
	 use play_data.running hack for a while
	 xitk_window_dialog_set_modal(xw);
      */
      
      return 1;
    }

    free(buffer); free(v_info); free(a_info);
  }

  return _gui_xine_play(stream, start_pos, start_time_in_secs, update_mmk);
}

int gui_xine_open_and_play(char *_mrl, char *_sub, int start_pos, 
			   int start_time, int av_offset, int spu_offset, int report_error) {
  gGui_t *gui = gGui;
  char *mrl = _mrl;
  int ret;
  
  if(__xineui_global_verbosity)
    printf("%s():\n\tmrl: '%s',\n\tsub '%s',\n\tstart_pos %d, start_time %d, av_offset %d, spu_offset %d.\n",
	   __func__, _mrl, (_sub) ? _sub : "NONE", start_pos, start_time, av_offset, spu_offset);
  
  if(!strncasecmp(mrl, "cfg:/", 5)) {
    config_mrl(mrl);
    gui_playlist_start_next();
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
      playlist_update_playlist();
    }
  }

  gui->suppress_messages++;
  ret = xine_open(gui->stream, (const char *) mrl);
  gui->suppress_messages--;
  if (!ret) {

#ifdef XINE_PARAM_GAPLESS_SWITCH
    if( xine_check_version(1,1,1) )
      xine_set_param(gui->stream, XINE_PARAM_GAPLESS_SWITCH, 0);
#endif

    if(!strcmp(mrl, gui->mmk.mrl))
      gui->playlist.mmk[gui->playlist.cur]->played = 1;
    
    if(report_error)
      gui_handle_xine_error(gui->stream, mrl);
    return 0;
  }
  
  if(_sub) {
    
    gui->suppress_messages++;
    ret = xine_open(gui->spu_stream, _sub);
    gui->suppress_messages--;
    if (ret)
      xine_stream_master_slave(gui->stream, 
			       gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
  }
  else
    xine_close (gui->spu_stream);
  
  xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, av_offset);
  xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
  
  if(!gui_xine_play(gui->stream, start_pos, start_time, 1)) {
    if(!strcmp(mrl, gui->mmk.mrl))
      gui->playlist.mmk[gui->playlist.cur]->played = 1;
    return 0;
  }

  if(!strcmp(mrl, gui->mmk.mrl))
    gui->playlist.mmk[gui->playlist.cur]->played = 1;

  gui_xine_get_pos_length(gui->stream, NULL, NULL, NULL);
  
  if (gui->stdctl_enable)
    stdctl_playing(mrl);

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
  gGui_t *gui = gGui;

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

  /* shut down event queue threads */
  /* do it first, events access gui elements ... */
  xine_event_dispose_queue(gui->event_queue);
  xine_event_dispose_queue(gui->visual_anim.event_queue);
  gui->event_queue = gui->visual_anim.event_queue = NULL;

  gui_deinit();

  panel_deinit (gui->panel);
  playlist_deinit();
  mrl_browser_deinit();
  control_deinit();
  
  setup_end();
  viewlog_end();
  kbedit_end();
  event_sender_end();
  stream_infos_end();
  tvset_end();
  vpplugin_end();
  applugin_end();
  help_end();
#ifdef HAVE_TAR
  download_skin_end();
#endif
  
  if(load_stream)
    filebrowser_end(load_stream);
  if(load_sub)
    filebrowser_end(load_sub);

  if (video_window_is_visible (gui->vwin))
    video_window_set_visibility (gui->vwin, 0);
  
  tvout_deinit(gui->tvout);

#ifdef HAVE_XF86VIDMODE
  /* just in case a different modeline than the original one is running,
   * toggle back to window mode which automatically causes a switch back to
   * the original modeline
   */
  if(gui->XF86VidMode_fullscreen)
    video_window_set_fullscreen_mode (gui->vwin, WINDOWED_MODE);
  //     gui_set_fullscreen_mode(NULL,NULL);
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
  post_deinit ();
  post_deinterlace_deinit ();

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
  
  if(gui->stdctl_enable) 
    stdctl_stop();

  /* this will prevent a race condition that may lead to a lock:
   * xitk_stop() makes gui_run() in event.c return from xitk_run()
   * and do some house cleaning which includes closing displays.
   * However, we're not done using displays and we must prevent that
   * from happening until we're finished.
   */
  XLockDisplay(gui->video_display);
  if( gui->video_display != gui->display )
    XLockDisplay(gui->display);
  xitk_stop();
  /* 
   * This prevent xine waiting till the end of time for an
   * XEvent when lirc (and futur other control ways) is used to quit .
   */
  if( gui->video_display == gui->display )
    gui_send_expose_to_window(gui->video_window);
 
  xitk_skin_unload_config(gui->skin_config);
  XUnlockDisplay(gui->video_display);
  if( gui->video_display != gui->display )
    XUnlockDisplay(gui->display);
}

void gui_play (xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

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
    
    if (!strncmp(gui->mmk.ident, "xine-ui version", 15)) {
      xine_error (_("No MRL (input stream) specified"));
      return;
    }
    
    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0, 
			       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset,
			       !mediamark_have_alternates(&(gui->mmk)))) {

      if(!mediamark_have_alternates(&(gui->mmk)) ||
	 !gui_open_and_play_alternates(&(gui->mmk), gui->mmk.sub)) {
	
	if(mediamark_all_played() && (gui->actions_on_start[0] == ACTID_QUIT)) {
	  gui_exit(NULL, NULL);
	  return;
	}
	gui_display_logo();
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
  gGui_t *gui = gGui;

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
  gui_display_logo();
}

void gui_close (xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

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
  gGui_t *gui = gGui;
  int speed = xine_get_param(gui->stream, XINE_PARAM_SPEED);

  if(speed != XINE_SPEED_PAUSE) {
    last_playback_speed = speed;
    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    panel_update_runtime_display (gui->panel);
  }
  else {
    xine_set_param(gui->stream, XINE_PARAM_SPEED, last_playback_speed);
    video_window_reset_ssaver (gui->vwin);
  }
  
  panel_check_pause (gui->panel);
  /* Give xine engine some time before updating OSD, otherwise the */
  /* time disp may be empty when switching to XINE_SPEED_PAUSE.    */
  xine_usec_sleep(10000);
  osd_update_status();
}

void gui_eject(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int i;
  
  if(xine_get_status(gui->stream) == XINE_STATUS_PLAY)
    gui_stop(NULL, NULL);
  
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
	mediamark_free_entry(gui->playlist.cur);
	
	for(i = gui->playlist.cur; i < gui->playlist.num; i++)
	  gui->playlist.mmk[i] = gui->playlist.mmk[i + 1];
	
	gui->playlist.mmk = (mediamark_t **) realloc(gui->playlist.mmk, sizeof(mediamark_t *) * (gui->playlist.num + 2));

	gui->playlist.mmk[gui->playlist.num] = NULL;
	
	if(gui->playlist.cur)
	  gui->playlist.cur--;
      }

      if (is_playback_widgets_enabled (gui->panel) && (!gui->playlist.num))
        enable_playback_controls (gui->panel, 0);

    }
    
    gui_set_current_mmk(mediamark_get_current_mmk());
    playlist_update_playlist();
  }
  else {
    /* Remove current mrl */
    if(gui->playlist.num)
      goto __remove_current_mrl;
  }

  if(!gui->playlist.num)
    gui->playlist.cur = -1;

}

void gui_toggle_visibility(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if (panel_is_visible (gui->panel) && (!gui->use_root_window)) {
    int visible = !video_window_is_visible (gui->vwin);

    video_window_set_visibility (gui->vwin, visible);


    /* We need to reparent all visible windows because of redirect tweaking */
    if(!visible) { /* Show the panel in taskbar */
      int x = 0, y = 0;

      xitk_get_window_position(gui->display, gui->panel_window, &x, &y, NULL, NULL);
      XLockDisplay(gui->display);
      XReparentWindow(gui->display, gui->panel_window, gui->imlib_data->x.root, x, y);
      XUnlockDisplay(gui->display);
    }
    reparent_all_windows();

    /* (re)start/stop visual animation */
    if (video_window_is_visible (gui->vwin)) {
      layer_above_video(gui->panel_window);
      
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

void gui_toggle_border(xitk_widget_t *w, void *data) {
  video_window_toggle_border (gGui->vwin);
}

static void set_fullscreen_mode(int fullscreen_mode) {
  gGui_t *gui = gGui;
  int panel        = panel_is_visible (gui->panel);
  int mrl_browser  = mrl_browser_is_visible();
  int playlist     = playlist_is_visible();
  int control      = control_is_visible();
  int setup        = setup_is_visible();
  int viewlog      = viewlog_is_visible();
  int kbedit       = kbedit_is_visible();
  int event_sender = event_sender_is_visible();
  int stream_infos = stream_infos_is_visible();
  int tvset        = tvset_is_visible();
  int vpplugin     = vpplugin_is_visible();
  int applugin     = applugin_is_visible();
  int help         = help_is_visible();

  if ((!(video_window_is_visible (gui->vwin))) || gui->use_root_window)
    return;

  if(panel)
    panel_toggle_visibility (NULL, gui->panel);
  else {
    if(mrl_browser)
      mrl_browser_toggle_visibility(NULL, NULL);
    if(playlist)
      playlist_toggle_visibility(NULL, NULL);
    if(control)
      control_toggle_visibility(NULL, NULL);
    if(setup)
      setup_toggle_visibility(NULL, NULL);
    if(viewlog)
      viewlog_toggle_visibility(NULL, NULL);
    if(kbedit)
      kbedit_toggle_visibility(NULL, NULL);
    if(event_sender)
      event_sender_toggle_visibility(NULL, NULL);
    if(stream_infos)
      stream_infos_toggle_visibility(NULL, NULL);
    if(tvset)
      tvset_toggle_visibility(NULL, NULL);
    if(vpplugin)
      vpplugin_toggle_visibility(NULL, NULL);
    if(applugin)
      applugin_toggle_visibility(NULL, NULL);
    if(help)
      help_toggle_visibility(NULL, NULL);
  }
  
  video_window_set_fullscreen_mode (gui->vwin, fullscreen_mode);
  
  /* Drawable has changed, update cursor visiblity */
  if(!gui->cursor_visible)
    video_window_set_cursor_visibility (gui->vwin, gui->cursor_visible);
  
  if(panel)
    panel_toggle_visibility (NULL, gui->panel);
  else {
    if(mrl_browser)
      mrl_browser_toggle_visibility(NULL, NULL);
    if(playlist)
      playlist_toggle_visibility(NULL, NULL);
    if(control)
      control_toggle_visibility(NULL, NULL);
    if(setup)
      setup_toggle_visibility(NULL, NULL);
    if(viewlog)
      viewlog_toggle_visibility(NULL, NULL);
    if(kbedit)
      kbedit_toggle_visibility(NULL, NULL);
    if(event_sender)
      event_sender_toggle_visibility(NULL, NULL);
    if(stream_infos)
      stream_infos_toggle_visibility(NULL, NULL);
    if(tvset)
      tvset_toggle_visibility(NULL, NULL);
    if(vpplugin)
      vpplugin_toggle_visibility(NULL, NULL);
    if(applugin)
      applugin_toggle_visibility(NULL, NULL);
    if(help)
      help_toggle_visibility(NULL, NULL);
  }

}

void gui_set_fullscreen_mode(xitk_widget_t *w, void *data) {
  set_fullscreen_mode(FULLSCR_MODE);
}

#ifdef HAVE_XINERAMA
void gui_set_xinerama_fullscreen_mode(void) {
  set_fullscreen_mode(FULLSCR_XI_MODE);
}
#endif

void gui_toggle_aspect(int aspect) {
  gGui_t *gui = gGui;
  static const char * const ratios[XINE_VO_ASPECT_NUM_RATIOS + 1] = {
    "Auto",
    "Square",
    "4:3",
    "Anamorphic",
    "DVB",
    NULL
  };

  if(aspect == -1)
    aspect = xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO) + 1;
  
  xine_set_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO, aspect);
  
  osd_display_info(_("Aspect ratio: %s"), 
		   ratios[xine_get_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO)]);
  
  if (panel_is_visible (gui->panel))  {
    XLockDisplay(gui->display);
    XRaiseWindow(gui->display, gui->panel_window);
    XUnlockDisplay(gui->display);
    video_window_set_transient_for (gui->vwin, gui->panel_window);
  }
}

void gui_toggle_interlaced(void) {
  gGui_t *gui = gGui;
  gui->deinterlace_enable = !gui->deinterlace_enable;
  osd_display_info(_("Deinterlace: %s"), (gui->deinterlace_enable) ? _("enabled") : _("disabled"));
  post_deinterlace();
  
  if (panel_is_visible (gui->panel))  {
    XLockDisplay(gui->display);
    XRaiseWindow(gui->display, gui->panel_window);
    XUnlockDisplay(gui->display);
    video_window_set_transient_for (gui->vwin, gui->panel_window);
  }
}

void gui_direct_change_audio_channel(xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = gGui;
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, value);
  panel_update_channel_display (gui->panel);
  osd_display_audio_lang();
}

void gui_change_audio_channel(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int dir = (int)(intptr_t)data;
  int channel;
  
  channel = xine_get_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  
  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;
  
  gui_direct_change_audio_channel(w, data, channel);
}

void gui_direct_change_spu_channel(xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = gGui;
  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, value);
  panel_update_channel_display (gui->panel);
  osd_display_spu_lang();
}

void gui_change_spu_channel(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int dir = (int)(intptr_t)data;
  int channel;
  int maxchannel;
  
  channel = xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);
  maxchannel = xine_get_stream_info(gui->stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);

  if (xine_get_status(gui->spu_stream) != XINE_STATUS_IDLE) /* if we have a slave SPU channel, take it into account */
    maxchannel += xine_get_stream_info(gui->spu_stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);

  /* XINE_STREAM_INFO_MAX_SPU_CHANNEL actually returns the number of available spu channels, i.e. 
   * 0 means no SPUs, 1 means 1 SPU channel, etc. */
  --maxchannel;

  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;

  if (channel > maxchannel)
    channel = -2; /* -2 == off, -1 == auto, 0 == 1st channel */
  else if (channel < -2)
    channel = maxchannel;

  gui_direct_change_spu_channel(w, data, channel);
}

void gui_change_speed_playback(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int speed = xine_get_param(gui->stream, XINE_PARAM_SPEED);

  if(((intptr_t)data) == GUI_NEXT) {
    if (speed > XINE_SPEED_PAUSE) {
      xine_set_param(gui->stream, XINE_PARAM_SPEED, (speed /= 2));
    }
#ifdef XINE_PARAM_VO_SINGLE_STEP
    else {
      xine_set_param (gui->stream, XINE_PARAM_VO_SINGLE_STEP, 1);
      panel_update_runtime_display (gui->panel);
    }
#endif
  }
  else if(((intptr_t)data) == GUI_PREV) {
    if(speed < XINE_SPEED_FAST_4) {
      if(speed > XINE_SPEED_PAUSE)
	xine_set_param(gui->stream, XINE_PARAM_SPEED, (speed *= 2));
      else {
	xine_set_param(gui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_SLOW_4));
	video_window_reset_ssaver (gui->vwin);
      }
    }
  }
  else if(((intptr_t)data) == GUI_RESET) {
    xine_set_param(gui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_NORMAL));
  }
  if(speed != XINE_SPEED_PAUSE)
    last_playback_speed = speed;

  panel_check_pause (gui->panel);
  /* Give xine engine some time before updating OSD, otherwise the        */
  /* time disp may be empty when switching to speeds < XINE_SPEED_NORMAL. */
  xine_usec_sleep(10000);
  osd_update_status();
}

static __attribute__((noreturn)) void *_gui_set_current_position(void *data) {
  gGui_t *gui = gGui;
  int  pos = (int)(intptr_t) data;
  int  update_mmk = 0, ret;
  
  pthread_detach(pthread_self());

  if(pthread_mutex_trylock(&gui->xe_mutex)) {
    pthread_exit(NULL);
  }
  
  if(gui->logo_mode && (mediamark_get_current_mrl())) {
    gui->suppress_messages++;
    ret = xine_open(gui->stream, (mediamark_get_current_mrl()));
    gui->suppress_messages--;
    if (!ret) {
      gui_handle_xine_error(gui->stream, (char *)(mediamark_get_current_mrl()));
      pthread_mutex_unlock(&gui->xe_mutex);
      pthread_exit(NULL);
    }
  }

  if(((xine_get_stream_info(gui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gui->ignore_next == 1)) {
    pthread_mutex_unlock(&gui->xe_mutex);
    pthread_exit(NULL);
  }
    
  if(xine_get_status(gui->stream) != XINE_STATUS_PLAY) {
    gui->suppress_messages++;
    xine_open(gui->stream, gui->mmk.mrl);
    gui->suppress_messages--;

    if(gui->mmk.sub) {
      gui->suppress_messages++;
      ret = xine_open(gui->spu_stream, gui->mmk.sub);
      gui->suppress_messages--;
      if (ret)
	xine_stream_master_slave(gui->stream, 
				 gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
    }
    else
      xine_close (gui->spu_stream);
  }
  
  gui->ignore_next = 1;
  
  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur] &&
     (!strcmp(gui->playlist.mmk[gui->playlist.cur]->mrl, gui->mmk.mrl)))
    update_mmk = 1;
  
  pthread_mutex_lock(&new_pos_mutex);
  gui->new_pos = pos;
  pthread_mutex_unlock(&new_pos_mutex);

  do {
    int opos;
    
    pthread_mutex_lock(&new_pos_mutex);
    opos = gui->new_pos;
    pthread_mutex_unlock(&new_pos_mutex);

    panel_update_slider (gui->panel, pos);
    osd_stream_position(pos);
    
    (void) gui_xine_play(gui->stream, pos, 0, update_mmk);
    
    xine_get_pos_length(gui->stream,
			&(gui->stream_length.pos),
			&(gui->stream_length.time), 
			&(gui->stream_length.length));
    panel_update_runtime_display (gui->panel);
    
    pthread_mutex_lock(&new_pos_mutex);
    if(opos == gui->new_pos)
      gui->new_pos = -1;
    
    pos = gui->new_pos;
    pthread_mutex_unlock(&new_pos_mutex);
    
  } while(pos != -1);
  
  gui->ignore_next = 0;
  osd_hide_status();
  panel_check_pause (gui->panel);

  pthread_mutex_unlock(&gui->xe_mutex);
  pthread_exit(NULL);
}

static __attribute__((noreturn)) void *_gui_seek_relative(void *data) {
  gGui_t *gui = gGui;
  int off_sec = (int)(intptr_t)data;
  int sec, pos;
  
  pthread_detach(pthread_self());
  
  pthread_mutex_lock(&new_pos_mutex);
  gui->new_pos = -1;
  pthread_mutex_unlock(&new_pos_mutex);

  if(!gui_xine_get_pos_length(gui->stream, NULL, &sec, NULL)) {
    pthread_exit(NULL);
  }
  
  if(pthread_mutex_trylock(&gui->xe_mutex)) {
    pthread_exit(NULL);
  }

  if(((xine_get_stream_info(gui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gui->ignore_next == 1)) {
    pthread_mutex_unlock(&gui->xe_mutex);
    pthread_exit(NULL);
  }
  
  if(xine_get_status(gui->stream) != XINE_STATUS_PLAY) {
    pthread_mutex_unlock(&gui->xe_mutex);
    pthread_exit(NULL);
  }
  
  gui->ignore_next = 1;
  
  sec /= 1000;

  if((sec + off_sec) < 0)
    sec = 0;
  else
    sec += off_sec;

  (void) gui_xine_play(gui->stream, 0, sec, 1);
  
  pthread_mutex_unlock(&gui->xe_mutex);

  if(gui_xine_get_pos_length(gui->stream, &pos, NULL, NULL))
    osd_stream_position(pos);
  
  gui->ignore_next = 0;
  osd_hide_status();
  panel_check_pause (gui->panel);

  pthread_exit(NULL);
}

void gui_set_current_position (int pos) {
  gGui_t *gui = gGui;
  int        err;
  pthread_t  pth;

  pthread_mutex_lock(&new_pos_mutex);
  if(gui->new_pos == -1) {
    pthread_mutex_unlock(&new_pos_mutex);
    if((err = pthread_create(&pth, NULL, _gui_set_current_position, (void *)(intptr_t)pos)) != 0) {
      printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
      abort();
    }
  }
  else {
    gui->new_pos = pos;
    pthread_mutex_unlock(&new_pos_mutex);
  }
}

void gui_seek_relative (int off_sec) {
  int        err;
  pthread_t  pth;
  
  if((err = pthread_create(&pth, NULL, _gui_seek_relative, (void *)(intptr_t)off_sec)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }
}

void gui_dndcallback(char *filename) {
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

    playlist_update_playlist();

    if(!(gui->playlist.control & PLAYLIST_CONTROL_IGNORE)) {

      if((xine_get_status(gui->stream) == XINE_STATUS_STOP) || gui->logo_mode) {
	if((more_than_one > -2) && ((more_than_one + 1) < gui->playlist.num))
	  gui->playlist.cur = more_than_one + 1;
	else
	  gui->playlist.cur = gui->playlist.num - 1;

        set_mmk(mediamark_get_current_mmk());
        mmk_set_update();
	if(gui->smart_mode)
	  gui_play(NULL, NULL);

      }
    }
    
    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    pthread_mutex_unlock(&gui->mmk_mutex);
  }

  free(mrl);
}

void gui_direct_nextprev(xitk_widget_t *w, void *data, int value) {
  gGui_t *gui = gGui;
  int            i, by_chapter;
  mediamark_t   *mmk = mediamark_get_current_mmk();

  by_chapter = (gui->skip_by_chapter &&
		(xine_get_stream_info(gui->stream, XINE_STREAM_INFO_HAS_CHAPTERS))) ? 1 : 0;
  
  if(mmk && mediamark_got_alternate(mmk))
    mediamark_unset_got_alternate(mmk);

  if(((intptr_t)data) == GUI_NEXT) {

    osd_hide();

    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_NEXT);

    }
    else {

      switch(gui->playlist.loop) {
      
      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gui->playlist.cur + value) < gui->playlist.num) {

	  if(gui->playlist.loop == PLAYLIST_LOOP_NO_LOOP)
	    gui->playlist.cur += (value - 1);
	  else
	    gui->playlist.cur += value;
	  
	  gui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	if((gui->playlist.cur + value) > gui->playlist.num) {
	  int newcur = value - (gui->playlist.num - gui->playlist.cur);
	  
	  gui->ignore_next = 1;
	  gui->playlist.cur = newcur;
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0, 
				     gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
	    gui_display_logo();

	  gui->ignore_next = 0;
	}
	else {
	  gui->playlist.cur += (value - 1);
	  gui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;
      }
    }
  }
  else if(((intptr_t)data) == GUI_PREV) {

    osd_hide();
    
    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_PRIOR);

    }
    else {

      switch(gui->playlist.loop) {
      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gui->playlist.cur - value) >= 0) {
	  
	  gui->ignore_next = 1;
	  gui->playlist.cur -= value;
	  
	  if((gui->playlist.cur < gui->playlist.num)) {
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0, 
				       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
	      gui_display_logo();

	  }
	  else
	    gui->playlist.cur = 0;
	  
	  gui->ignore_next = 0;
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	gui->ignore_next = 1;

	if((gui->playlist.cur - value) >= 0) {

	  gui->playlist.cur -= value;
	  
	  if((gui->playlist.cur < gui->playlist.num)) {
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0, 
				       gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
	      gui_display_logo();
	  }
	  else
	    gui->playlist.cur = 0;
	  
	}
	else {
	  int newcur = (gui->playlist.cur - value) + gui->playlist.num;
	  
	  gui->playlist.cur = newcur;
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gui->mmk.mrl, gui->mmk.sub, 0, 
				     gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1))
	    gui_display_logo();
	}

	gui->ignore_next = 0;
	break;
      }
    }
  }

  panel_check_pause (gui->panel);
}
  
void gui_nextprev(xitk_widget_t *w, void *data) {
  gui_direct_nextprev(w, data, 1);
}

void gui_playlist_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if(!playlist_is_running()) {
    playlist_editor();
  }
  else {
    if(playlist_is_visible())
      if(gui->use_root_window)
	playlist_toggle_visibility(NULL, NULL);
      else
	playlist_exit(NULL, NULL);
    else
      playlist_toggle_visibility(NULL, NULL);
  }

}

void gui_mrlbrowser_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  gui->nongui_error_msg = NULL;

  if(!mrl_browser_is_running()) {
    open_mrlbrowser(NULL, NULL);
  }
  else {

    if(!mrl_browser_is_visible()) {
      show_mrl_browser();
      if(!gui->use_root_window)
	set_mrl_browser_transient();
    }
    else {
      if(gui->use_root_window)
	show_mrl_browser();
      else
	destroy_mrl_browser();
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
  gGui_t *gui = gGui;

  if(control_is_running() && !control_is_visible())
    control_toggle_visibility(NULL, NULL);
  else if(!control_is_running())
    control_panel();
  else {
    if(gui->use_root_window)
      control_toggle_visibility(NULL, NULL);
    else
      control_exit(NULL, NULL);
  }
}

void gui_setup_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if (setup_is_running() && !setup_is_visible())
    setup_toggle_visibility(NULL, NULL);
  else if(!setup_is_running())
    setup_panel();
  else {
    if(gui->use_root_window)
      setup_toggle_visibility(NULL, NULL);
    else
      setup_end();
  }
}

void gui_event_sender_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  
  if (event_sender_is_running() && !event_sender_is_visible())
    event_sender_toggle_visibility(NULL, NULL);
  else if(!event_sender_is_running())
    event_sender_panel();
  else {
    if(gui->use_root_window)
      event_sender_toggle_visibility(NULL, NULL);
    else
      event_sender_end();
  }
}

void gui_stream_infos_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  
  if (stream_infos_is_running() && !stream_infos_is_visible())
    stream_infos_toggle_visibility(NULL, NULL);
  else if(!stream_infos_is_running())
    stream_infos_panel();
  else {
    if(gui->use_root_window)
      stream_infos_toggle_visibility(NULL, NULL);
    else
      stream_infos_end();
  }
}

void gui_tvset_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  
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
  gGui_t *gui = gGui;
  
  if (vpplugin_is_running() && !vpplugin_is_visible())
    vpplugin_toggle_visibility(NULL, NULL);
  else if(!vpplugin_is_running())
    vpplugin_panel();
  else {
    if(gui->use_root_window)
      vpplugin_toggle_visibility(NULL, NULL);
    else
      vpplugin_end();
  }
}

void gui_vpp_enable(void) {
  gGui_t *gui = gGui;

  if(vpplugin_is_post_selected()) {
    gui->post_video_enable = !gui->post_video_enable;
    osd_display_info(_("Video post plugins: %s."), (gui->post_video_enable) ? _("enabled") : _("disabled"));
    vpplugin_update_enable_button();
    if(vpplugin_is_visible())
      vpplugin_rewire_from_posts_window();
    else
      vpplugin_rewire_posts();
  }
}

void gui_viewlog_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if (viewlog_is_running() && !viewlog_is_visible())
    viewlog_toggle_visibility(NULL, NULL);
  else if(!viewlog_is_running())
    viewlog_panel();
  else {
    if(gui->use_root_window)
      viewlog_toggle_visibility(NULL, NULL);
    else
      viewlog_end();
  }
}

void gui_kbedit_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if (kbedit_is_running() && !kbedit_is_visible())
    kbedit_toggle_visibility(NULL, NULL);
  else if(!kbedit_is_running())
    kbedit_window();
  else {
    if(gui->use_root_window)
      kbedit_toggle_visibility(NULL, NULL);
    else
      kbedit_end();
  }
}

void gui_help_show(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if (help_is_running() && !help_is_visible())
    help_toggle_visibility(NULL, NULL);
  else if(!help_is_running())
    help_panel();
  else {
    if(gui->use_root_window)
      help_toggle_visibility(NULL, NULL);
    else
      help_end();
  }
}

/*
 * Return 1 if layer should be set
 */
int is_layer_above(void) {
  gGui_t *gui = gGui;
  
  return (gui->always_layer_above || gui->layer_above) ? 1 : 0;
}
/*
 * set window layer property to something above GNOME (and KDE?) panel
 * (reset to normal if do_raise == 0)
 */
void layer_above_video(Window w) {
  int layer = 10;
  
  if(!(is_layer_above()))
    return;
  
  if ((!(video_window_get_fullscreen_mode (gGui->vwin) & WINDOWED_MODE)) && video_window_is_visible (gGui->vwin)) {
    layer = xitk_get_layer_level();
  }
  else {
    if(is_layer_above())
      layer = xitk_get_layer_level();
    else
      layer = 4;
  }
  
  xitk_set_window_layer(w, layer);
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
  gGui_t *gui = gGui;
  
  if (applugin_is_running() && !applugin_is_visible())
    applugin_toggle_visibility(NULL, NULL);
  else if(!applugin_is_running())
    applugin_panel();
  else {
    if(gui->use_root_window)
      applugin_toggle_visibility(NULL, NULL);
    else
      applugin_end();
  }
}

void gui_app_enable(void) {
  gGui_t *gui = gGui;

  if(applugin_is_post_selected()) {
    gui->post_audio_enable = !gui->post_audio_enable;
    osd_display_info(_("Audio post plugins: %s."), (gui->post_audio_enable) ? _("enabled") : _("disabled"));
    applugin_update_enable_button();
    if(applugin_is_visible())
      applugin_rewire_from_posts_window();
    else
      applugin_rewire_posts();
  }
}

void gui_change_zoom(int zoom_dx, int zoom_dy) {
  gGui_t *gui = gGui;

  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_X,
		 xine_get_param(gui->stream, XINE_PARAM_VO_ZOOM_X) + zoom_dx);
  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_Y,
		 xine_get_param(gui->stream, XINE_PARAM_VO_ZOOM_Y) + zoom_dy);
  
  if (panel_is_visible (gui->panel))  {
    XLockDisplay(gui->display);
    XRaiseWindow(gui->display, gui->panel_window);
    XUnlockDisplay(gui->display);
    video_window_set_transient_for (gui->vwin, gui->panel_window);
  }
}

/*
 * Reset zooming by recall aspect ratio.
 */
void gui_reset_zoom(void) {
  gGui_t *gui = gGui;

  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_X, 100);
  xine_set_param(gui->stream, XINE_PARAM_VO_ZOOM_Y, 100);
  
  if (panel_is_visible (gui->panel))  {
    XLockDisplay(gui->display);
    XRaiseWindow(gui->display, gui->panel_window);
    XUnlockDisplay(gui->display);
    video_window_set_transient_for (gui->vwin, gui->panel_window);
  }
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

/*
 * Send an Expose event to given window.
 */
void gui_send_expose_to_window(Window window) {
  gGui_t *gui = gGui;
  XEvent xev;

  if(window == None)
    return;

  xev.xany.type          = Expose;
  xev.xexpose.type       = Expose;
  xev.xexpose.send_event = True;
  xev.xexpose.display    = gui->display;
  xev.xexpose.window     = window;
  xev.xexpose.count      = 0;
  
  XLockDisplay(gui->display);
  if(!XSendEvent(gui->display, window, False, ExposureMask, &xev)) {
    fprintf(stderr, _("XSendEvent(display, 0x%x ...) failed.\n"), (unsigned int) window);
  }
  XSync(gui->display, False);
  XUnlockDisplay(gui->display);
  
}

void gui_add_mediamark(void) {
  gGui_t *gui = gGui;
  
  if((gui->logo_mode == 0) && (xine_get_status(gui->stream) == XINE_STATUS_PLAY)) {
    int secs;

    if(gui_xine_get_pos_length(gui->stream, NULL, &secs, NULL)) {
      secs /= 1000;
      
      mediamark_append_entry(gui->mmk.mrl, gui->mmk.ident, 
			     gui->mmk.sub, secs, -1, gui->mmk.av_offset, gui->mmk.spu_offset);
      playlist_update_playlist();
    }
  }
}

static void fileselector_cancel_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
  char *cur_dir = filebrowser_get_current_dir(fb);

  if(fb == load_stream) {
    if(cur_dir && strlen(cur_dir)) {
      strlcpy(gui->curdir, cur_dir, sizeof(gui->curdir));
      config_update_string("media.files.origin_path", gui->curdir);
    }
    load_stream = NULL;
  }
  else if(fb == load_sub)
    load_sub = NULL;

  free(cur_dir);
}


/* Callback function for file select button or double-click in file browser.
   Append selected file to the current playlist */
static void fileselector_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
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

    playlist_update_playlist();
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
        gui_play(NULL, NULL);
      }
    }
  } /* If valid file name */

  free(file);
  load_stream = NULL;
}


/* Callback function for "Select All" button in file browser. Append all files in the
   currently selected directory to the current playlist. */
static void fileselector_all_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
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
      
      playlist_update_playlist();

      /* Enable playback controls on display */
      if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
        enable_playback_controls (gui->panel, 1);

      /* If an MRL is not being played, select the first file appended. If in "smart mode" start
         playing the entry.  If an MRL is currently being played, let it continue normally */
      if((first != gui->playlist.num) && (xine_get_status(gui->stream) == XINE_STATUS_STOP)) {
        gui->playlist.cur = first;
        if(gui->smart_mode) {
          gui_set_current_mmk(mediamark_get_current_mmk());
          gui_play(NULL, NULL);
        }
      }
    } /* If valid path */

    i = 0;
    while(files[i])
      free(files[i++]);
    
    free(files);
  } /* If valid file list */

  free(path);

  load_stream = NULL;
}


void gui_file_selector(void) {
  gGui_t *gui = gGui;
  filebrowser_callback_button_t  cbb[3];

  gui->nongui_error_msg = NULL;

  if(load_stream)
    filebrowser_raise_window(load_stream);
  else {
    cbb[0].label = _("Select");
    cbb[0].callback = fileselector_callback;
    cbb[0].need_a_file = 0;
    cbb[1].label = _("Select all");
    cbb[1].callback = fileselector_all_callback;
    cbb[1].need_a_file = 0;
    cbb[2].callback = fileselector_cancel_callback;
    cbb[2].need_a_file = 0;
    load_stream = create_filebrowser(_("Stream(s) Loading"), gui->curdir, hidden_file_cb, &cbb[0], &cbb[1], &cbb[2]);
  }
}

static void subselector_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
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
	
	playlist_mrlident_toggle();
	
	if(xine_get_status(gui->stream) == XINE_STATUS_PLAY) {
	  int curpos;
	  xine_close (gui->spu_stream);

	  gui->suppress_messages++;
	  ret = xine_open(gui->spu_stream, mmk->sub);
	  gui->suppress_messages--;
	  if (ret)
	    xine_stream_master_slave(gui->stream, 
				     gui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
	  
	  if(gui_xine_get_pos_length(gui->stream, &curpos, NULL, NULL)) {
	    xine_stop(gui->stream);
	    gui_set_current_position(curpos);
	  }
	}
      }
    }
    free(file);
  }

  load_sub = NULL;
}

void gui_select_sub(void) {
  gGui_t *gui = gGui;
  
  if(gui->playlist.num) {
    if(load_sub)
      filebrowser_raise_window(load_sub);
    else {
      filebrowser_callback_button_t  cbb[2];
      mediamark_t *mmk;
      
      mmk = mediamark_get_current_mmk();
      
      if(mmk) {
	char *path, *open_path;
	
	cbb[0].label = _("Select");
	cbb[0].callback = subselector_callback;
	cbb[0].need_a_file = 1;
	cbb[1].callback = fileselector_cancel_callback;
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
	
	load_sub = create_filebrowser(_("Pick a subtitle file"), open_path, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
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
    if(!visual_anim_open_and_play(gui->visual_anim.stream, 
				  gui->visual_anim.mrls[gui->visual_anim.current]))
      gui_handle_xine_error(gui->visual_anim.stream, 
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
