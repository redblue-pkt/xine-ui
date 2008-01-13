/* 
 * Copyright (C) 2000-2007 the xine project
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
 * $Id$
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

extern gGui_t          *gGui;
extern _panel_t        *panel;
filebrowser_t          *load_stream = NULL, *load_sub = NULL;

static pthread_mutex_t new_pos_mutex =  PTHREAD_MUTEX_INITIALIZER;

static int             last_playback_speed = XINE_SPEED_NORMAL;

void reparent_all_windows(void) {
  int                    i;
  typedef struct {
    int                 (*visible)(void);
    void                (*reparent)(void);
  } reparent_t;
  reparent_t _reparent[] = {
    { panel_is_visible,         panel_reparent },
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
  if(window && (*visible) && running) {

    XLockDisplay(gGui->display);
    if(gGui->use_root_window) {
      if(xitk_is_window_visible(gGui->display, window))
        XIconifyWindow(gGui->display, window, gGui->screen);
      else
	XMapWindow(gGui->display, window);
    }
    else if(!xitk_is_window_visible(gGui->display, window)) {
      /* Obviously user has iconified the window, let it be */
    }
    else {
      *visible = 0;
      XUnlockDisplay(gGui->display);
      xitk_hide_widgets(widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow(gGui->display, window);
    }
    XUnlockDisplay(gGui->display);

  }
  else {
    if(running) {
      *visible = 1;
      xitk_show_widgets(widget_list);

      XLockDisplay(gGui->display);
      XRaiseWindow(gGui->display, window);
      XMapWindow(gGui->display, window);
      if(!gGui->use_root_window && gGui->video_display == gGui->display)
        XSetTransientForHint (gGui->display, window, gGui->video_window);
      XUnlockDisplay(gGui->display);

      wait_for_window_visible(gGui->display, window);
      layer_above_video(window);
    }
  }
}

int gui_xine_get_pos_length(xine_stream_t *stream, int *pos, int *time, int *length) {
  int t = 0, ret = 0;
  int lpos, ltime, llength;
  
  if(pthread_mutex_trylock(&gGui->xe_mutex))
    return 0;
  
  if(stream && (xine_get_status(stream) == XINE_STATUS_PLAY)) {
    while(((ret = xine_get_pos_length(stream, &lpos, &ltime, &llength)) == 0) && (++t < 10) && (!gGui->on_quit))
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
  
  if((ret != 0) && (stream == gGui->stream)) {
    gGui->stream_length.pos = lpos;
    gGui->stream_length.time = ltime;
    gGui->stream_length.length = llength;
  }

  pthread_mutex_unlock(&gGui->xe_mutex);
  return ret;
}

/*
 *
 */
void try_to_set_input_focus(Window window) {

  wait_for_window_visible(gGui->display, window);
  
  if(xitk_is_window_visible(gGui->display, window)) {
    int    retry = 0;
    Window focused_win;

    do {
      int revert;

      XLockDisplay (gGui->display);
      XSetInputFocus(gGui->display, window, RevertToParent, CurrentTime);
      XSync(gGui->display, False);
      XUnlockDisplay (gGui->display);

      /* Retry until the WM was mercyful to give us the focus (but not indefinitely) */
      xine_usec_sleep(5000);
      XLockDisplay (gGui->display);
      XGetInputFocus(gGui->display, &focused_win, &revert);
      XUnlockDisplay (gGui->display);

    } while((focused_win != window) && (retry++ < 30));
  }
}

/*
 *
 */
void gui_display_logo(void) {

  pthread_mutex_lock(&gGui->logo_mutex);
  
  gGui->logo_mode = 2;
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
    gGui->ignore_next = 1;
    xine_stop(gGui->stream);
    gGui->ignore_next = 0; 
  }

  if(gGui->visual_anim.running)
    visual_anim_stop();

  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -1);
  xine_set_param(gGui->stream, XINE_PARAM_SPU_CHANNEL, -1);
  panel_update_channel_display();

  if(gGui->display_logo)
    (void) gui_xine_open_and_play((char *)gGui->logo_mrl, NULL, 0, 0, 0, 0, 1);

  gGui->logo_mode = 1;
  
  panel_reset_slider();
  if(stream_infos_is_visible())
    stream_infos_update_infos();
  
  pthread_mutex_unlock(&gGui->logo_mutex);
}

static int _gui_xine_play(xine_stream_t *stream, 
			  int start_pos, int start_time_in_secs, int update_mmk) {
  int      ret;
  int      has_video;
  int      already_playing = (gGui->logo_mode == 0);
  
  if(gGui->visual_anim.post_changed && (xine_get_status(stream) == XINE_STATUS_STOP)) {
    post_rewire_visual_anim();
    gGui->visual_anim.post_changed = 0;
  }
  
  if(start_time_in_secs)
    start_time_in_secs *= 1000;
  
  has_video = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);
  if (has_video)
    has_video = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_VIDEO);

  if((has_video && gGui->visual_anim.enabled == 1) && gGui->visual_anim.running) {

    if(post_rewire_audio_port_to_stream(stream))
      gGui->visual_anim.running = 0;

  } else if (!has_video && (gGui->visual_anim.enabled == 1) && 
	     (gGui->visual_anim.running == 0) && gGui->visual_anim.post_output_element.post) {

    if(post_rewire_audio_post_to_stream(stream))
      gGui->visual_anim.running = 1;

  }

  if((ret = xine_play(stream, start_pos, start_time_in_secs)) == 0)
    gui_handle_xine_error(stream, NULL);
  else {
    char *ident;

    pthread_mutex_lock(&gGui->logo_mutex);
    if(gGui->logo_mode != 2)
      gGui->logo_mode = 0;
    pthread_mutex_unlock(&gGui->logo_mutex);
    
    if(gGui->logo_mode == 0) {
     
      if(stream_infos_is_visible())
	stream_infos_update_infos();
      
      if(update_mmk && ((ident = stream_infos_get_ident_from_stream(stream)) != NULL)) {
	if(gGui->mmk.ident)
	  free(gGui->mmk.ident);
	if(gGui->playlist.mmk[gGui->playlist.cur]->ident)
	  free(gGui->playlist.mmk[gGui->playlist.cur]->ident);
	
	gGui->mmk.ident = strdup(ident);
	gGui->playlist.mmk[gGui->playlist.cur]->ident = strdup(ident);
	
	video_window_set_mrl(gGui->mmk.ident);
	playlist_mrlident_toggle();
	panel_update_mrl_display();
	free(ident);
      }

      if(has_video) {
	
	if((gGui->visual_anim.enabled == 2) && gGui->visual_anim.running)
	  visual_anim_stop();
	
	if(gGui->auto_vo_visibility) {
	  
	  if(!video_window_is_visible())
	    video_window_set_visibility(1);
	  
	}

	if(gGui->auto_panel_visibility && video_window_is_visible() &&
	   panel_is_visible() )
	  panel_toggle_visibility(NULL, NULL);
	
      }
      else {
	
	if(gGui->auto_vo_visibility) {
	  
	  if(!panel_is_visible())
	    panel_toggle_visibility(NULL, NULL);

	  if(video_window_is_visible())
	    video_window_set_visibility(0);
	    
	}

	if(gGui->auto_panel_visibility && video_window_is_visible() && 
	  !panel_is_visible() )
	  panel_toggle_visibility(NULL, NULL);
	  
	if(video_window_is_visible()) {
	  if(!gGui->visual_anim.running)
	    visual_anim_play();
	}
	else
	  gGui->visual_anim.running = 2;
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

int gui_xine_play(xine_stream_t *stream, int start_pos, int start_time_in_secs, int update_mmk) {
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
  
  if((v_unhandled = (has_video && (!video_handled))) 
     || (a_unhandled = (has_audio && (!audio_handled)))) {
    char *buffer = NULL;
    char *v_info = NULL;
    char *a_info = NULL;
    
    if(v_unhandled && a_unhandled) {
      asprintf(&buffer, _("The stream '%s' isn't supported by xine:\n\n"),
	       (stream == gGui->stream) ? gGui->mmk.mrl : gGui->visual_anim.mrls[gGui->visual_anim.current]);
    }
    else {
      asprintf(&buffer, _("The stream '%s' uses an unsupported codec:\n\n"),
	       (stream == gGui->stream) ? gGui->mmk.mrl : gGui->visual_anim.mrls[gGui->visual_anim.current]);
    }
    
    if(v_unhandled) {
      const char *minfo;
      uint32_t    vfcc;
      
      minfo = xine_get_meta_info(stream, XINE_META_INFO_VIDEOCODEC);
      vfcc = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_FOURCC);
      asprintf(&v_info, _("Video Codec: %s (%s)\n"),
	      (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"), 
	      (get_fourcc_string(vfcc)));
    }
    
    if(a_unhandled) {
      const char *minfo;
      uint32_t    afcc;
      
      minfo = xine_get_meta_info(stream, XINE_META_INFO_AUDIOCODEC);
      afcc = xine_get_stream_info(stream, XINE_STREAM_INFO_AUDIO_FOURCC);
      asprintf(&a_info,  _("Audio Codec: %s (%s)\n"),
	      (minfo && strlen(minfo)) ? (char *) minfo : _("Unavailable"), 
	      (get_fourcc_string(afcc)));
    }
    

    if(v_unhandled && a_unhandled) {
      xine_error("%s%s%s", buffer, v_info, a_info);
      free(buffer); free(v_info); free(a_info);
      return 0;
    }

    if(!gGui->play_anyway) {
      xitk_window_t *xw;

      play_data.stream             = stream;
      play_data.start_pos          = start_pos;
      play_data.start_time_in_secs = start_time_in_secs;
      play_data.update_mmk         = update_mmk;
      play_data.running            = 1;
      
      xw = xitk_window_dialog_yesno_with_width(gGui->imlib_data, _("Start Playback ?"), 
					       start_anyway_yesno, start_anyway_yesno, 
					       NULL, 400, ALIGN_CENTER,
					       "%s%s%s%s", buffer,
					       v_info, a_info,
					       _("\nStart playback anyway ?\n"));
      free(buffer); free(v_info); free(a_info);

      XLockDisplay(gGui->display);
      if(!gGui->use_root_window && gGui->video_display == gGui->display)
	XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
      XSync(gGui->display, False);
      XUnlockDisplay(gGui->display);
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
  char *mrl = _mrl;
  
  if(gGui->verbosity)
    printf("%s():\n\tmrl: '%s',\n\tsub '%s',\n\tstart_pos %d, start_time %d, av_offset %d, spu_offset %d.\n",
	   __func__, _mrl, (_sub) ? _sub : "NONE", start_pos, start_time, av_offset, spu_offset);
  
  if(!strncasecmp(mrl, "cfg:/", 5)) {
    config_mrl(mrl);
    gui_playlist_start_next();
    return 1;
  }
  else if((!strncasecmp(mrl, "ftp://", 6)) || (!strncasecmp(mrl, "dload:/", 7)))  {
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
	  char *ident = strdup(gGui->playlist.mmk[gGui->playlist.cur]->ident);
	  if(gGui->playlist.mmk[gGui->playlist.cur]->sub)
	    sub = strdup(gGui->playlist.mmk[gGui->playlist.cur]->sub);
	  start = gGui->playlist.mmk[gGui->playlist.cur]->start;
	  end = gGui->playlist.mmk[gGui->playlist.cur]->end;
	  
	  fwrite(download.buf, download.size, 1, fd);
	  fflush(fd);
	  fclose(fd);

	  mediamark_replace_entry(&gGui->playlist.mmk[gGui->playlist.cur], 
				  newmrl, ident, sub, start, end, 0, 0);
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  mrl = gGui->mmk.mrl;

	  free(newmrl);
	  free(ident);
	  free(sub);
	}
      }
    }

    if(download.buf)
      free(download.buf);
    if(download.error)
      free(download.error);

  }

  if(mrl_look_like_playlist(mrl)) {
    if(mediamark_concat_mediamarks(mrl)) {
      gui_set_current_mmk(mediamark_get_current_mmk());
      mrl        = gGui->mmk.mrl;
      start_pos  = 0;
      start_time = gGui->mmk.start;
      av_offset  = gGui->mmk.av_offset;
      spu_offset = gGui->mmk.spu_offset;
      playlist_update_playlist();
    }
  }

  if(!xine_open(gGui->stream, (const char *) mrl)) {

#ifdef XINE_PARAM_GAPLESS_SWITCH
    if( xine_check_version(1,1,1) )
      xine_set_param(gGui->stream, XINE_PARAM_GAPLESS_SWITCH, 0);
#endif

    if(!strcmp(mrl, gGui->mmk.mrl))
      gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
    
    if(report_error)
      gui_handle_xine_error(gGui->stream, mrl);
    return 0;
  }
  
  if(_sub) {
    
    if(xine_open(gGui->spu_stream, _sub))
      xine_stream_master_slave(gGui->stream, 
			       gGui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
  }
  else
    xine_close (gGui->spu_stream);
  
  xine_set_param(gGui->stream, XINE_PARAM_AV_OFFSET, av_offset);
  xine_set_param(gGui->stream, XINE_PARAM_SPU_OFFSET, spu_offset);
  
  if(!gui_xine_play(gGui->stream, start_pos, start_time, 1)) {
    if(!strcmp(mrl, gGui->mmk.mrl))
      gGui->playlist.mmk[gGui->playlist.cur]->played = 1;
    return 0;
  }

  if(!strcmp(mrl, gGui->mmk.mrl))
    gGui->playlist.mmk[gGui->playlist.cur]->played = 1;

  gui_xine_get_pos_length(gGui->stream, NULL, NULL, NULL);
  
  if (gGui->stdctl_enable)
    stdctl_playing(mrl);

  return 1;
}

int gui_open_and_play_alternates(mediamark_t *mmk, const char *sub) {
  char *alt;
  
  if(!(alt = mediamark_get_current_alternate_mrl(mmk)))
    alt = mediamark_get_first_alternate_mrl(mmk);
  
  do {

    if(gui_xine_open_and_play(alt, gGui->mmk.sub, 0, 0, 0, 0, 0))
      return 1;

  } while((alt = mediamark_get_next_alternate_mrl(&(gGui->mmk))));
  return 0;
}

/*
 * Callback-functions for widget-button click
 */
void gui_exit (xitk_widget_t *w, void *data) {

  oxine_exit();

  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
    gGui->ignore_next = 1;

    if(gGui->visual_anim.running) {
      xine_post_out_t * audio_source;

      xine_stop(gGui->visual_anim.stream);

      while(xine_get_status(gGui->visual_anim.stream) == XINE_STATUS_PLAY)
      	xine_usec_sleep(50000);
      
      audio_source = xine_get_audio_source(gGui->stream);
      (void) xine_post_wire_audio_port(audio_source, gGui->ao_port);
    }
    
    xine_stop (gGui->stream);
    gGui->ignore_next = 0;
  }
  
  gGui->on_quit = 1;

  panel_deinit();
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
  download_skin_end();
  
  if(load_stream)
    filebrowser_end(load_stream);
  if(load_sub)
    filebrowser_end(load_sub);

  gui_deinit();

  if(video_window_is_visible())
    video_window_set_visibility(0);
  
  tvout_deinit(gGui->tvout);
  video_window_exit();

#ifdef HAVE_XF86VIDMODE
  /* just in case a different modeline than the original one is running,
   * toggle back to window mode which automatically causes a switch back to
   * the original modeline
   */
  if(gGui->XF86VidMode_fullscreen)
    video_window_set_fullscreen_mode(WINDOWED_MODE);
  //     gui_set_fullscreen_mode(NULL,NULL);
#endif
   
  osd_deinit();

  config_update_num("gui.amp_level", gGui->mixer.amp_level);
  config_save();
  
  /* Restore old audio volume */
  if(gGui->ao_port && (gGui->mixer.method == SOUND_CARD_MIXER))
    xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.original_level);
  
  xine_close(gGui->stream);
  xine_close(gGui->visual_anim.stream);

  xine_event_dispose_queue(gGui->event_queue);
  xine_event_dispose_queue(gGui->visual_anim.event_queue);

  /* we are going to dispose this stream, so make sure slider_loop 
   * won't use it anymore (otherwise -> segfault on exit).
   */
  gGui->running = 0;

  if(gGui->visual_anim.post_output_element.post)
    xine_post_dispose(gGui->xine, gGui->visual_anim.post_output_element.post);

  xine_dispose(gGui->stream);
  /* xine_dispose(gGui->visual_anim.stream); */

  if(gGui->vo_port)
    xine_close_video_driver(gGui->xine, gGui->vo_port);
  if(gGui->vo_none)
    xine_close_video_driver(gGui->xine, gGui->vo_none);

  if(gGui->ao_port)
    xine_close_audio_driver(gGui->xine, gGui->ao_port);
  if(gGui->ao_none)
    xine_close_audio_driver(gGui->xine, gGui->ao_none);

  xine_exit(gGui->xine); 

#ifdef HAVE_LIRC
  if(gGui->lirc_enable)
    lirc_stop();
#endif
  
  if(gGui->stdctl_enable) 
    stdctl_stop();
  
  xitk_stop();
  /* 
   * This prevent xine waiting till the end of time for an
   * XEvent when lirc (and futur other control ways) is used to quit .
   */
  if( gGui->video_display == gGui->display )
    gui_send_expose_to_window(gGui->video_window);
  xitk_skin_unload_config(gGui->skin_config);
}

void gui_play (xitk_widget_t *w, void *data) {

  if((!gGui->playlist.num) && (xine_get_status(gGui->stream) != XINE_STATUS_PLAY))
    return;

  video_window_reset_ssaver();
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
    if(gGui->logo_mode != 0) {
      gGui->ignore_next = 1;
      xine_stop(gGui->stream);
      gGui->ignore_next = 0; 
    }
  }

  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY) {
    
    if (!strncmp(gGui->mmk.ident, "xine-ui version", 15)) {
      xine_error (_("No MRL (input stream) specified"));
      return;
    }
    
    if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
			       gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset,
			       !mediamark_have_alternates(&(gGui->mmk)))) {

      if(!mediamark_have_alternates(&(gGui->mmk)) ||
	 !gui_open_and_play_alternates(&(gGui->mmk), gGui->mmk.sub)) {
	
	if(mediamark_all_played() && (gGui->actions_on_start[0] == ACTID_QUIT))
	  gui_exit(NULL, NULL);
	gui_display_logo();
      }
    }
  }
  else {
    int oldspeed = xine_get_param(gGui->stream, XINE_PARAM_SPEED);

    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
    if(oldspeed != XINE_SPEED_NORMAL)
      osd_update_status();
  }
  
  panel_check_pause();
}

void gui_stop (xitk_widget_t *w, void *data) {

  gGui->ignore_next = 1;
  xine_stop (gGui->stream);
  gGui->ignore_next = 0;

  if(!(gGui->playlist.control & PLAYLIST_CONTROL_STOP_PERSIST))
    gGui->playlist.control &= ~PLAYLIST_CONTROL_STOP;

  gGui->stream_length.pos = gGui->stream_length.time = gGui->stream_length.length = 0;
  
  mediamark_reset_played_state();
  if(gGui->visual_anim.running) {
    xine_stop(gGui->visual_anim.stream);
    if(gGui->visual_anim.enabled == 2)
      gGui->visual_anim.running = 0;
  }

  osd_hide_sinfo();
  osd_hide_bar();
  osd_hide_info();
  osd_update_status();
  panel_reset_slider ();
  panel_check_pause();
  panel_update_runtime_display();

  if(is_playback_widgets_enabled()) {
    if(!gGui->playlist.num) {
      gui_set_current_mmk(NULL);
      enable_playback_controls(0);
    }
    else if(gGui->playlist.num && (strcmp((mediamark_get_current_mrl()), gGui->mmk.mrl))) {
      gui_set_current_mmk(mediamark_get_current_mmk());
    }
  }
  gui_display_logo();
}

void gui_pause (xitk_widget_t *w, void *data, int state) {
  int speed = xine_get_param(gGui->stream, XINE_PARAM_SPEED);

  if(speed != XINE_SPEED_PAUSE) {
    last_playback_speed = speed;
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
  }
  else {
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, last_playback_speed);
    video_window_reset_ssaver();
  }
  
  panel_check_pause();
  /* Give xine engine some time before updating OSD, otherwise the */
  /* time disp may be empty when switching to XINE_SPEED_PAUSE.    */
  xine_usec_sleep(10000);
  osd_update_status();
}

void gui_eject(xitk_widget_t *w, void *data) {
  int i;
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY)
    gui_stop(NULL, NULL);
  
  if(xine_eject(gGui->stream)) {

    if(gGui->playlist.num) {
      mediamark_t **mmk = NULL;
      char         *tok = NULL;
      char         *mrl;
      int           len;
      int           new_num = 0;
      /*
       * If it's an mrl (____://) remove all of them in playlist
       */
      mrl = strstr((mediamark_get_current_mrl()), ":/");
      if(mrl) {
	char *cur_mrl = (char *)mediamark_get_current_mrl();
	
	len = (mrl - cur_mrl) + 3;
	tok = (char *) alloca(len);
  	snprintf(tok, len, "%s", cur_mrl);
      }

      if(tok != NULL) {
	/* 
	 * Store all of not maching entries
	 */
	for(i = 0; i < gGui->playlist.num; i++) {
	  if(strncasecmp(gGui->playlist.mmk[i]->mrl, tok, strlen(tok))) {
	    
	    mmk = (mediamark_t **) realloc(mmk, sizeof(mediamark_t *) * (new_num + 2));
	    
	    (void) mediamark_store_mmk(&mmk[new_num], 
				       gGui->playlist.mmk[i]->mrl,
				       gGui->playlist.mmk[i]->ident,
				       gGui->playlist.mmk[i]->sub,
				       gGui->playlist.mmk[i]->start, 
				       gGui->playlist.mmk[i]->end,
				       gGui->playlist.mmk[i]->av_offset,
				       gGui->playlist.mmk[i]->spu_offset);
	    new_num++;
	  }
	}

	/*
	 * flip playlists.
	 */
	mediamark_free_mediamarks();
	if(mmk)
	  gGui->playlist.mmk = mmk;
	
	if(!(gGui->playlist.num = new_num))
	  gGui->playlist.cur = -1;
	else if(new_num)
	  gGui->playlist.cur = 0;
      }
      else {
      __remove_current_mrl:
	/*
	 * Remove only the current MRL
	 */
	mediamark_free_entry(gGui->playlist.cur);
	
	for(i = gGui->playlist.cur; i < gGui->playlist.num; i++)
	  gGui->playlist.mmk[i] = gGui->playlist.mmk[i + 1];
	
	gGui->playlist.mmk = (mediamark_t **) realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 2));

	gGui->playlist.mmk[gGui->playlist.num] = NULL;
	
	if(gGui->playlist.cur)
	  gGui->playlist.cur--;
      }

      if(is_playback_widgets_enabled() && (!gGui->playlist.num))
	enable_playback_controls(0);

    }
    
    gui_set_current_mmk(mediamark_get_current_mmk());
    playlist_update_playlist();
  }
  else {
    /* Remove current mrl */
    if(gGui->playlist.num)
      goto __remove_current_mrl;
  }

  if(!gGui->playlist.num)
    gGui->playlist.cur = -1;

}

void gui_toggle_visibility(xitk_widget_t *w, void *data) {

  if(panel_is_visible() && (!gGui->use_root_window)) {
    int visible = !video_window_is_visible();

    video_window_set_visibility(visible);


    /* We need to reparent all visible windows because of redirect tweaking */
    if(!visible) { /* Show the panel in taskbar */
      int x = 0, y = 0;

      xitk_get_window_position(gGui->display, gGui->panel_window, &x, &y, NULL, NULL);
      XLockDisplay(gGui->display);
      XReparentWindow(gGui->display, gGui->panel_window, gGui->imlib_data->x.root, x, y);
      XUnlockDisplay(gGui->display);
    }
    reparent_all_windows();

    /* (re)start/stop visual animation */
    if(video_window_is_visible()) {
      layer_above_video(gGui->panel_window);
      
      if(gGui->visual_anim.enabled && (gGui->visual_anim.running == 2))
	visual_anim_play();
    }
    else {

      if(gGui->visual_anim.running) {
	visual_anim_stop();
	gGui->visual_anim.running = 2;
      }
    }
  }
}

void gui_toggle_border(xitk_widget_t *w, void *data) {
  video_window_toggle_border();
}

static void set_fullscreen_mode(int fullscreen_mode) {
  int panel        = panel_is_visible();
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

  if((!(video_window_is_visible())) || gGui->use_root_window)
    return;

  if(panel)
    panel_toggle_visibility(NULL, NULL);
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
  
  video_window_set_fullscreen_mode(fullscreen_mode);
  
  /* Drawable has changed, update cursor visiblity */
  if(!gGui->cursor_visible)
    video_window_set_cursor_visibility(gGui->cursor_visible);
  
  if(panel)
    panel_toggle_visibility(NULL, NULL);
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
  static char *ratios[XINE_VO_ASPECT_NUM_RATIOS + 1] = {
    "Auto",
    "Square",
    "4:3",
    "Anamorphic",
    "DVB",
    NULL
  };

  if(aspect == -1)
    aspect = xine_get_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO) + 1;
  
  xine_set_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO, aspect);
  
  osd_display_info(_("Aspect ratio: %s"), 
		   ratios[xine_get_param(gGui->stream, XINE_PARAM_VO_ASPECT_RATIO)]);
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, gGui->panel_window);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
    
  }
}

void gui_toggle_interlaced(void) {
  gGui->deinterlace_enable = !gGui->deinterlace_enable;
  osd_display_info(_("Deinterlace: %s"), (gGui->deinterlace_enable) ? _("enabled") : _("disabled"));
  post_deinterlace();
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, gGui->panel_window);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
}

void gui_direct_change_audio_channel(xitk_widget_t *w, void *data, int value) {
  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, value);
  panel_update_channel_display();
  osd_display_audio_lang();
}

void gui_change_audio_channel(xitk_widget_t *w, void *data) {
  int dir = (int)data;
  int channel;
  
  channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  
  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;
  
  gui_direct_change_audio_channel(w, data, channel);
}

void gui_direct_change_spu_channel(xitk_widget_t *w, void *data, int value) {
  xine_set_param(gGui->stream, XINE_PARAM_SPU_CHANNEL, value);
  panel_update_channel_display();
  osd_display_spu_lang();
}

void gui_change_spu_channel(xitk_widget_t *w, void *data) {
  int dir = (int)data;
  int channel;
  
  channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);

  if(dir == GUI_NEXT)
    channel++;
  else if(dir == GUI_PREV)
    channel--;
  
  gui_direct_change_spu_channel(w, data, channel);
}

void gui_change_speed_playback(xitk_widget_t *w, void *data) {
  int speed = xine_get_param(gGui->stream, XINE_PARAM_SPEED);

  if(((int)data) == GUI_NEXT) {
    if(speed > XINE_SPEED_PAUSE)
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, (speed /= 2));
  }
  else if(((int)data) == GUI_PREV) {
    if(speed < XINE_SPEED_FAST_4) {
      if(speed > XINE_SPEED_PAUSE)
	xine_set_param(gGui->stream, XINE_PARAM_SPEED, (speed *= 2));
      else {
	xine_set_param(gGui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_SLOW_4));
	video_window_reset_ssaver();
      }
    }
  }
  else if(((int)data) == GUI_RESET) {
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, (speed = XINE_SPEED_NORMAL));
  }
  if(speed != XINE_SPEED_PAUSE)
    last_playback_speed = speed;

  panel_check_pause();
  /* Give xine engine some time before updating OSD, otherwise the        */
  /* time disp may be empty when switching to speeds < XINE_SPEED_NORMAL. */
  xine_usec_sleep(10000);
  osd_update_status();
}

static void *_gui_set_current_position(void *data) {
  int  pos = (int) data;
  int  update_mmk = 0;
  
  pthread_detach(pthread_self());

  if(pthread_mutex_trylock(&gGui->xe_mutex)) {
    pthread_exit(NULL);
    return NULL;
  }
  
  pthread_mutex_lock(&new_pos_mutex);
  gGui->new_pos = pos;
  pthread_mutex_unlock(&new_pos_mutex);
  
  if(gGui->logo_mode && (mediamark_get_current_mrl())) {
    if(!xine_open(gGui->stream, (mediamark_get_current_mrl()))) {
      gui_handle_xine_error(gGui->stream, (char *)(mediamark_get_current_mrl()));
      pthread_mutex_unlock(&gGui->xe_mutex);
      pthread_exit(NULL);
      return NULL;
    }
  }

  if(((xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gGui->ignore_next == 1)) {
    pthread_mutex_unlock(&gGui->xe_mutex);
    pthread_exit(NULL);
    return NULL;
  }
    
  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY) {
    xine_open(gGui->stream, gGui->mmk.mrl);

    if(gGui->mmk.sub) {
      if(xine_open(gGui->spu_stream, gGui->mmk.sub))
	xine_stream_master_slave(gGui->stream, 
				 gGui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
    }
    else
      xine_close (gGui->spu_stream);
  }
  
  gGui->ignore_next = 1;
  
  if(gGui->playlist.num && gGui->playlist.cur >= 0 && gGui->playlist.mmk &&
     gGui->playlist.mmk[gGui->playlist.cur] &&
     (!strcmp(gGui->playlist.mmk[gGui->playlist.cur]->mrl, gGui->mmk.mrl)))
    update_mmk = 1;
  
  do {
    int opos;
    
    pthread_mutex_lock(&new_pos_mutex);
    opos = gGui->new_pos;
    pthread_mutex_unlock(&new_pos_mutex);
    
    xitk_slider_set_pos(panel->playback_widgets.slider_play, pos);
    osd_stream_position(pos);
    
    (void) gui_xine_play(gGui->stream, pos, 0, update_mmk);
    
    xine_get_pos_length(gGui->stream,
			&(gGui->stream_length.pos),
			&(gGui->stream_length.time), 
			&(gGui->stream_length.length));
    panel_update_runtime_display();
    
    pthread_mutex_lock(&new_pos_mutex);
    if(opos == gGui->new_pos)
      gGui->new_pos = -1;
    
    pos = gGui->new_pos;
    pthread_mutex_unlock(&new_pos_mutex);
    
  } while(pos != -1);
  
  gGui->ignore_next = 0;
  osd_hide_status();
  panel_check_pause();

  pthread_mutex_unlock(&gGui->xe_mutex);
  pthread_exit(NULL);
  return NULL;
}

static void *_gui_seek_relative(void *data) {
  int off_sec = (int)data;
  int sec, pos;
  
  pthread_detach(pthread_self());
  
  pthread_mutex_lock(&new_pos_mutex);
  gGui->new_pos = -1;
  pthread_mutex_unlock(&new_pos_mutex);

  if(!gui_xine_get_pos_length(gGui->stream, NULL, &sec, NULL)) {
    pthread_exit(NULL);
    return NULL;
  }
  
  if(pthread_mutex_trylock(&gGui->xe_mutex)) {
    pthread_exit(NULL);
    return NULL;
  }

  if(((xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0) || 
     (gGui->ignore_next == 1)) {
    pthread_mutex_unlock(&gGui->xe_mutex);
    pthread_exit(NULL);
    return NULL;
  }
  
  if(xine_get_status(gGui->stream) != XINE_STATUS_PLAY) {
    pthread_mutex_unlock(&gGui->xe_mutex);
    pthread_exit(NULL);
    return NULL;
  }
  
  gGui->ignore_next = 1;
  
  sec /= 1000;

  if((sec + off_sec) < 0)
    sec = 0;
  else
    sec += off_sec;

  (void) gui_xine_play(gGui->stream, 0, sec, 1);
  
  pthread_mutex_unlock(&gGui->xe_mutex);

  if(gui_xine_get_pos_length(gGui->stream, &pos, NULL, NULL))
    osd_stream_position(pos);
  
  gGui->ignore_next = 0;
  osd_hide_status();
  panel_check_pause();

  pthread_exit(NULL);
  return NULL;
}

void gui_set_current_position (int pos) {
  int        err;
  pthread_t  pth;

  if(gGui->new_pos == -1) {
    
    if((err = pthread_create(&pth, NULL, _gui_set_current_position, (void *)pos)) != 0) {
      printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
      abort();
    }
  }
  else {
    pthread_mutex_lock(&new_pos_mutex);
    gGui->new_pos = pos;
    pthread_mutex_unlock(&new_pos_mutex);
  }

}

void gui_seek_relative (int off_sec) {
  int        err;
  pthread_t  pth;
  
  if((err = pthread_create(&pth, NULL, _gui_seek_relative, (void *)off_sec)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }
}

static void *_gui_dndcallback(void *data) {
  int   more_than_one = -2;
  char *mrl           = (char *) data;

  pthread_detach(pthread_self());

  if(mrl) {
    char  buffer[strlen(mrl) + 10];
    char  buffer2[strlen(mrl) + 10];
    char *p;

    memset(&buffer, 0, sizeof(buffer));
    memset(&buffer2, 0, sizeof(buffer2));

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
	    more_than_one = gGui->playlist.cur;
	    goto __do_play;
	  }
	  else
	    snprintf(buffer, sizeof(buffer), "file:/%s", p);
	}
	else {
	  snprintf(buffer2, sizeof(buffer2), "/%s", p);
	  
	  /* file don't exist, add it anyway */
	  if((stat(buffer2, &pstat)) == -1)
	    snprintf(buffer, sizeof(buffer), "%s", mrl);
	  else {
	    if(is_a_dir(buffer2)) {
	      
	      if(buffer2[strlen(buffer2) - 1] == '/')
		buffer2[strlen(buffer2) - 1] = '\0'; 
	      
	      mediamark_collect_from_directory(buffer2);
	      more_than_one = gGui->playlist.cur;
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
      snprintf(buffer, sizeof(buffer), "%s", mrl);
    
    if(is_a_dir(buffer)) {
      if(buffer[strlen(buffer) - 1] == '/')
	buffer[strlen(buffer) - 1] = '\0'; 
      
      mediamark_collect_from_directory(buffer);
      more_than_one = gGui->playlist.cur;
    }
    else {
      char *ident;

      /* If possible, use only base name as identifier to better fit into the display */
      if((ident = strrchr(buffer, '/')) && ident[1])
	ident++;
      else
	ident = buffer;
      
      if(mrl_look_like_playlist(buffer)) {
	int cur = gGui->playlist.cur;
	
	more_than_one = (gGui->playlist.cur - 1);
	if(mediamark_concat_mediamarks(buffer))
	  gGui->playlist.cur = cur;
	else
	  mediamark_append_entry(buffer, ident, NULL, 0, -1, 0, 0);
      }
      else
	mediamark_append_entry(buffer, ident, NULL, 0, -1, 0, 0);

    }
    
  __do_play:

    playlist_update_playlist();

    if(!(gGui->playlist.control & PLAYLIST_CONTROL_IGNORE)) {

      if((xine_get_status(gGui->stream) == XINE_STATUS_STOP) || gGui->logo_mode) {
	if((more_than_one > -2) && ((more_than_one + 1) < gGui->playlist.num))
	  gGui->playlist.cur = more_than_one + 1;
	else
	  gGui->playlist.cur = gGui->playlist.num - 1;

	gui_set_current_mmk(mediamark_get_current_mmk());
	if(gGui->smart_mode)
	  gui_play(NULL, NULL);

      }
    }
    
    if((!is_playback_widgets_enabled()) && gGui->playlist.num)
      enable_playback_controls(1);
  }

  free(mrl);

  pthread_exit(NULL);
}

void gui_dndcallback(char *mrl) {
  int         err;
  pthread_t   pth;
  char       *_mrl = (mrl ? strdup(mrl) : NULL);

  if((err = pthread_create(&pth, NULL, _gui_dndcallback, (void *)_mrl)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }

}

void gui_direct_nextprev(xitk_widget_t *w, void *data, int value) {
  int            i, by_chapter;
  mediamark_t   *mmk = mediamark_get_current_mmk();

  by_chapter = (gGui->skip_by_chapter &&
		(xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_HAS_CHAPTERS))) ? 1 : 0;
  
  if(mmk && mediamark_got_alternate(mmk))
    mediamark_unset_got_alternate(mmk);

  if(((int)data) == GUI_NEXT) {

    osd_hide();

    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_NEXT);

    }
    else {

      switch(gGui->playlist.loop) {
      
      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gGui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gGui->playlist.cur + value) < gGui->playlist.num) {

	  if(gGui->playlist.loop == PLAYLIST_LOOP_NO_LOOP)
	    gGui->playlist.cur += (value - 1);
	  else
	    gGui->playlist.cur += value;
	  
	  gGui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	if((gGui->playlist.cur + value) > gGui->playlist.num) {
	  int newcur = value - (gGui->playlist.num - gGui->playlist.cur);
	  
	  gGui->ignore_next = 1;
	  gGui->playlist.cur = newcur;
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				     gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 1))
	    gui_display_logo();

	  gGui->ignore_next = 0;
	}
	else {
	  gGui->playlist.cur += (value - 1);
	  gGui->ignore_next = 0;
	  gui_playlist_start_next();
	}
	break;
      }
    }
  }
  else if(((int)data) == GUI_PREV) {

    osd_hide();
    
    if(by_chapter) {

      for(i = 0; i < value; i++)
	gui_execute_action_id(ACTID_EVENT_PRIOR);

    }
    else {

      switch(gGui->playlist.loop) {
      case PLAYLIST_LOOP_SHUFFLE:
      case PLAYLIST_LOOP_SHUF_PLUS:
	gGui->ignore_next = 0;
	gui_playlist_start_next();
	break;

      case PLAYLIST_LOOP_NO_LOOP:
      case PLAYLIST_LOOP_REPEAT:
	if((gGui->playlist.cur - value) >= 0) {
	  
	  gGui->ignore_next = 1;
	  gGui->playlist.cur -= value;
	  
	  if((gGui->playlist.cur < gGui->playlist.num)) {
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				       gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 1))
	      gui_display_logo();

	  }
	  else
	    gGui->playlist.cur = 0;
	  
	  gGui->ignore_next = 0;
	}
	break;

      case PLAYLIST_LOOP_LOOP:
	gGui->ignore_next = 1;

	if((gGui->playlist.cur - value) >= 0) {

	  gGui->playlist.cur -= value;
	  
	  if((gGui->playlist.cur < gGui->playlist.num)) {
	    gui_set_current_mmk(mediamark_get_current_mmk());
	    if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				       gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 1))
	      gui_display_logo();
	  }
	  else
	    gGui->playlist.cur = 0;
	  
	}
	else {
	  int newcur = (gGui->playlist.cur - value) + gGui->playlist.num;
	  
	  gGui->playlist.cur = newcur;
	  gui_set_current_mmk(mediamark_get_current_mmk());
	  if(!gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0, 
				     gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 1))
	    gui_display_logo();
	}

	gGui->ignore_next = 0;
	break;
      }
    }
  }

  panel_check_pause();
}
  
void gui_nextprev(xitk_widget_t *w, void *data) {
  gui_direct_nextprev(w, data, 1);
}

void gui_playlist_show(xitk_widget_t *w, void *data) {

  if(!playlist_is_running()) {
    playlist_editor();
  }
  else {
    if(playlist_is_visible())
      if(gGui->use_root_window)
	playlist_toggle_visibility(NULL, NULL);
      else
	playlist_exit(NULL, NULL);
    else
      playlist_toggle_visibility(NULL, NULL);
  }

}

void gui_mrlbrowser_show(xitk_widget_t *w, void *data) {

  gGui->nongui_error_msg = NULL;

  if(!mrl_browser_is_running()) {
    open_mrlbrowser(NULL, NULL);
  }
  else {

    if(!mrl_browser_is_visible()) {
      show_mrl_browser();
      if(!gGui->use_root_window)
	set_mrl_browser_transient();
    }
    else {
      if(gGui->use_root_window)
	show_mrl_browser();
      else
	destroy_mrl_browser();
    }

  }

}

static void set_mmk(mediamark_t *mmk) {
  
  if(gGui->mmk.mrl)
    free(gGui->mmk.mrl);
  if(gGui->mmk.ident)
    free(gGui->mmk.ident);
  if(gGui->mmk.sub)
    free(gGui->mmk.sub);
  if(mediamark_have_alternates(&(gGui->mmk)))
    mediamark_free_alternates(&(gGui->mmk));
  
  if(mmk) {
    gGui->mmk.mrl           = strdup(mmk->mrl);
    gGui->mmk.ident         = strdup(((mmk->ident) ? mmk->ident : mmk->mrl));
    gGui->mmk.sub           = mmk->sub ? strdup(mmk->sub): NULL;
    gGui->mmk.start         = mmk->start;
    gGui->mmk.end           = mmk->end;
    gGui->mmk.av_offset     = mmk->av_offset;
    gGui->mmk.spu_offset    = mmk->spu_offset;
    gGui->mmk.got_alternate = mmk->got_alternate;
    mediamark_duplicate_alternates(mmk, &(gGui->mmk));
  }
  else {
    char buffer[1024];
    
    snprintf(buffer, sizeof(buffer), "xine-ui version %s", VERSION);
    
    gGui->mmk.mrl           = strdup(_("There is no MRL."));
    gGui->mmk.ident         = strdup(buffer);
    gGui->mmk.sub           = NULL;
    gGui->mmk.start         = 0;
    gGui->mmk.end           = -1;
    gGui->mmk.av_offset     = 0;
    gGui->mmk.spu_offset    = 0;
    gGui->mmk.got_alternate = 0;
    gGui->mmk.alternates    = NULL;
    gGui->mmk.cur_alt       = NULL;
  }
}

static void mmk_set_update(void) {

  video_window_set_mrl(gGui->mmk.ident);
  event_sender_update_menu_buttons();
  panel_update_mrl_display();
  playlist_update_focused_entry();
  
  gGui->playlist.ref_append = gGui->playlist.cur;
}

void gui_set_current_mmk(mediamark_t *mmk) {

  pthread_mutex_lock(&gGui->mmk_mutex);
  set_mmk(mmk);
  pthread_mutex_unlock(&gGui->mmk_mutex);

  mmk_set_update();
}

void gui_set_current_mmk_by_index(int idx) {

  pthread_mutex_lock(&gGui->mmk_mutex);
  set_mmk(mediamark_get_mmk_by_index(idx));
  gGui->playlist.cur = idx;
  pthread_mutex_unlock(&gGui->mmk_mutex);

  mmk_set_update();
}

void gui_control_show(xitk_widget_t *w, void *data) {

  if(control_is_running() && !control_is_visible())
    control_toggle_visibility(NULL, NULL);
  else if(!control_is_running())
    control_panel();
  else {
    if(gGui->use_root_window)
      control_toggle_visibility(NULL, NULL);
    else
      control_exit(NULL, NULL);
  }
}

void gui_setup_show(xitk_widget_t *w, void *data) {

  if (setup_is_running() && !setup_is_visible())
    setup_toggle_visibility(NULL, NULL);
  else if(!setup_is_running())
    setup_panel();
  else {
    if(gGui->use_root_window)
      setup_toggle_visibility(NULL, NULL);
    else
      setup_end();
  }
}

void gui_event_sender_show(xitk_widget_t *w, void *data) {
  
  if (event_sender_is_running() && !event_sender_is_visible())
    event_sender_toggle_visibility(NULL, NULL);
  else if(!event_sender_is_running())
    event_sender_panel();
  else {
    if(gGui->use_root_window)
      event_sender_toggle_visibility(NULL, NULL);
    else
      event_sender_end();
  }
}

void gui_stream_infos_show(xitk_widget_t *w, void *data) {
  
  if (stream_infos_is_running() && !stream_infos_is_visible())
    stream_infos_toggle_visibility(NULL, NULL);
  else if(!stream_infos_is_running())
    stream_infos_panel();
  else {
    if(gGui->use_root_window)
      stream_infos_toggle_visibility(NULL, NULL);
    else
      stream_infos_end();
  }
}

void gui_tvset_show(xitk_widget_t *w, void *data) {
  
  if (tvset_is_running() && !tvset_is_visible())
    tvset_toggle_visibility(NULL, NULL);
  else if(!tvset_is_running())
    tvset_panel();
  else {
    if(gGui->use_root_window)
      tvset_toggle_visibility(NULL, NULL);
    else
      tvset_end();
  }
}

void gui_vpp_show(xitk_widget_t *w, void *data) {
  
  if (vpplugin_is_running() && !vpplugin_is_visible())
    vpplugin_toggle_visibility(NULL, NULL);
  else if(!vpplugin_is_running())
    vpplugin_panel();
  else {
    if(gGui->use_root_window)
      vpplugin_toggle_visibility(NULL, NULL);
    else
      vpplugin_end();
  }
}

void gui_vpp_enable(void) {

  if(vpplugin_is_post_selected()) {
    gGui->post_video_enable = !gGui->post_video_enable;
    osd_display_info(_("Video post plugins: %s."), (gGui->post_video_enable) ? _("enabled") : _("disabled"));
    vpplugin_update_enable_button();
    if(vpplugin_is_visible())
      vpplugin_rewire_from_posts_window();
    else
      vpplugin_rewire_posts();
  }
}

void gui_viewlog_show(xitk_widget_t *w, void *data) {

  if (viewlog_is_running() && !viewlog_is_visible())
    viewlog_toggle_visibility(NULL, NULL);
  else if(!viewlog_is_running())
    viewlog_panel();
  else {
    if(gGui->use_root_window)
      viewlog_toggle_visibility(NULL, NULL);
    else
      viewlog_end();
  }
}

void gui_kbedit_show(xitk_widget_t *w, void *data) {

  if (kbedit_is_running() && !kbedit_is_visible())
    kbedit_toggle_visibility(NULL, NULL);
  else if(!kbedit_is_running())
    kbedit_window();
  else {
    if(gGui->use_root_window)
      kbedit_toggle_visibility(NULL, NULL);
    else
      kbedit_end();
  }
}

void gui_help_show(xitk_widget_t *w, void *data) {

  if (help_is_running() && !help_is_visible())
    help_toggle_visibility(NULL, NULL);
  else if(!help_is_running())
    help_panel();
  else {
    if(gGui->use_root_window)
      help_toggle_visibility(NULL, NULL);
    else
      help_end();
  }
}

/*
 * Return 1 if layer should be set
 */
int is_layer_above(void) {
  
  return (gGui->always_layer_above || gGui->layer_above) ? 1 : 0;
}
/*
 * set window layer property to something above GNOME (and KDE?) panel
 * (reset to normal if do_raise == 0)
 */
void layer_above_video(Window w) {
  int layer = 10;
  
  if(!(is_layer_above()))
    return;
  
  if ((!(video_window_get_fullscreen_mode() & WINDOWED_MODE)) && video_window_is_visible()) {
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
  if(value < 0)
    value = 0;
  if(value > 200)
    value = 200;
  gGui->mixer.amp_level = value;
  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_AMP_LEVEL, gGui->mixer.amp_level);
  if(gGui->mixer.method == SOFTWARE_MIXER)
    xitk_slider_set_pos(panel->mixer.slider, gGui->mixer.amp_level);
  osd_draw_bar(_("Amplification Level"), 0, 200, gGui->mixer.amp_level, OSD_BAR_STEPPER);
}
void gui_increase_amp_level(void) {
  change_amp_vol((gGui->mixer.amp_level + 1));
}
void gui_decrease_amp_level(void) {
  change_amp_vol((gGui->mixer.amp_level - 1));
}
void gui_reset_amp_level(void) {
  change_amp_vol(100);
}

void change_audio_vol(int value) {
  if(value < 0)
    value = 0;
  if(value > 100)
    value = 100;
  gGui->mixer.volume_level = value;
  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
  xitk_slider_set_pos(panel->mixer.slider, gGui->mixer.volume_level);
  osd_draw_bar(_("Audio Volume"), 0, 100, gGui->mixer.volume_level, OSD_BAR_STEPPER);
}
void gui_increase_audio_volume(void) {
  if((gGui->mixer.method == SOUND_CARD_MIXER) && (gGui->mixer.caps & MIXER_CAP_VOL))
    change_audio_vol((gGui->mixer.volume_level + 1));
  else if(gGui->mixer.method == SOFTWARE_MIXER)
    gui_increase_amp_level();
}
void gui_decrease_audio_volume(void) {
  if((gGui->mixer.method == SOUND_CARD_MIXER) && (gGui->mixer.caps & MIXER_CAP_VOL))
    change_audio_vol((gGui->mixer.volume_level - 1));
  else if(gGui->mixer.method == SOFTWARE_MIXER)
    gui_decrease_amp_level();
}

void gui_app_show(xitk_widget_t *w, void *data) {
  
  if (applugin_is_running() && !applugin_is_visible())
    applugin_toggle_visibility(NULL, NULL);
  else if(!applugin_is_running())
    applugin_panel();
  else {
    if(gGui->use_root_window)
      applugin_toggle_visibility(NULL, NULL);
    else
      applugin_end();
  }
}

void gui_app_enable(void) {

  if(applugin_is_post_selected()) {
    gGui->post_audio_enable = !gGui->post_audio_enable;
    osd_display_info(_("Audio post plugins: %s."), (gGui->post_audio_enable) ? _("enabled") : _("disabled"));
    applugin_update_enable_button();
    if(applugin_is_visible())
      applugin_rewire_from_posts_window();
    else
      applugin_rewire_posts();
  }
}

void gui_change_zoom(int zoom_dx, int zoom_dy) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_X,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_ZOOM_X) + zoom_dx);
  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y) + zoom_dy);
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, gGui->panel_window);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
}

/*
 * Reset zooming by recall aspect ratio.
 */
void gui_reset_zoom(void) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_X, 100);
  xine_set_param(gGui->stream, XINE_PARAM_VO_ZOOM_Y, 100);
  
  if (panel_is_visible())  {
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, gGui->panel_window);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, gGui->panel_window, gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
}

/* 
 * Toggle TV Modes on the dxr3
 */
void gui_toggle_tvmode(void) {

  xine_set_param(gGui->stream, XINE_PARAM_VO_TVMODE,
		 xine_get_param(gGui->stream, XINE_PARAM_VO_TVMODE) + 1);

  osd_display_info(_("TV Mode: %d"), xine_get_param(gGui->stream, XINE_PARAM_VO_TVMODE));
}

/*
 * Send an Expose event to given window.
 */
void gui_send_expose_to_window(Window window) {
  XEvent xev;

  if(window == None)
    return;

  xev.xany.type          = Expose;
  xev.xexpose.type       = Expose;
  xev.xexpose.send_event = True;
  xev.xexpose.display    = gGui->display;
  xev.xexpose.window     = window;
  xev.xexpose.count      = 0;
  
  XLockDisplay(gGui->display);
  if(!XSendEvent(gGui->display, window, False, ExposureMask, &xev)) {
    fprintf(stderr, _("XSendEvent(display, 0x%x ...) failed.\n"), (unsigned int) window);
  }
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);
  
}

void gui_add_mediamark(void) {
  
  if((gGui->logo_mode == 0) && (xine_get_status(gGui->stream) == XINE_STATUS_PLAY)) {
    int secs;

    if(gui_xine_get_pos_length(gGui->stream, NULL, &secs, NULL)) {
      secs /= 1000;
      
      mediamark_append_entry(gGui->mmk.mrl, gGui->mmk.ident, 
			     gGui->mmk.sub, secs, -1, gGui->mmk.av_offset, gGui->mmk.spu_offset);
      playlist_update_playlist();
    }
  }
}

static void fileselector_cancel_callback(filebrowser_t *fb) {
  char *cur_dir = filebrowser_get_current_dir(fb);

  if(fb == load_stream) {
    if(cur_dir && strlen(cur_dir)) {
      snprintf(gGui->curdir, sizeof(gGui->curdir), "%s", cur_dir);
      config_update_string("media.files.origin_path", gGui->curdir);
    }
    load_stream = NULL;
  }
  else if(fb == load_sub)
    load_sub = NULL;
}


/* Callback function for file select button or double-click in file browser.
   Append selected file to the current playlist */
static void fileselector_callback(filebrowser_t *fb) {
  char *file;
  char *cur_dir = filebrowser_get_current_dir(fb);
  
  /* Upate configuration with the selected directory path */
  if(cur_dir && strlen(cur_dir)) {
    snprintf(gGui->curdir, sizeof(gGui->curdir), "%s", cur_dir);
    config_update_string("media.files.origin_path", gGui->curdir);
  }
  
  /* Get the file path/name */
  if(((file = filebrowser_get_full_filename(fb)) != NULL) && strlen(file)) {
    int first  = gGui->playlist.num;
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
    free(file);
    free(ident);

    /* Enable controls on display */
    if((!is_playback_widgets_enabled()) && gGui->playlist.num)
      enable_playback_controls(1);

    /* If an MRL is not being played, select the first file appended. If in "smart mode" start
       playing the entry.  If a an MRL is currently being played, let it continue normally */
    if((first != gGui->playlist.num) && (xine_get_status(gGui->stream) == XINE_STATUS_STOP)) {
      gGui->playlist.cur = first;
      if(gGui->smart_mode) {
        gui_set_current_mmk(mediamark_get_current_mmk());
        gui_play(NULL, NULL);
      }
    }
  } /* If valid file name */

  load_stream = NULL;
}


/* Callback function for "Select All" button in file browser. Append all files in the
   currently selected directory to the current playlist. */
static void fileselector_all_callback(filebrowser_t *fb) {
  char **files;
  char  *path = filebrowser_get_current_dir(fb);
  
  /* Update the configuration with the current path */
  if(path && strlen(path)) {
    snprintf(gGui->curdir, sizeof(gGui->curdir), "%s", path);
    config_update_string("media.files.origin_path", gGui->curdir);
  }
  
  /* Get all of the file names in the current directory as an array of pointers to strings */
  if((files = filebrowser_get_all_files(fb)) != NULL) {
    int i = 0;

    if(path && strlen(path)) {
      char pathname[XITK_PATH_MAX + 1 + 1]; /* +1 for trailing '/' */
      char fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 2];
      int  first = gGui->playlist.num; /* current count of entries in playlist */

      /* If the path is anything other than "/", append a slash to it so that it can
         be concatenated with the file name */
      if(strcasecmp(path, "/"))
        snprintf(pathname, sizeof(pathname), "%s/", path);
      else
        snprintf(pathname, sizeof(pathname), "%s", path);
      
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
      free(path);

      /* Enable playback controls on display */
      if((!is_playback_widgets_enabled()) && gGui->playlist.num)
        enable_playback_controls(1);

      /* If an MRL is not being played, select the first file appended. If in "smart mode" start
         playing the entry.  If an MRL is currently being played, let it continue normally */
      if((first != gGui->playlist.num) && (xine_get_status(gGui->stream) == XINE_STATUS_STOP)) {
        gGui->playlist.cur = first;
        if(gGui->smart_mode) {
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

  load_stream = NULL;
}


void gui_file_selector(void) {
  filebrowser_callback_button_t  cbb[3];

  gGui->nongui_error_msg = NULL;

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
    load_stream = create_filebrowser(_("Stream(s) Loading"), gGui->curdir, hidden_file_cb, &cbb[0], &cbb[1], &cbb[2]);
  }
}

static void subselector_callback(filebrowser_t *fb) {
  char *file;

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    if(file) {
      mediamark_t *mmk = mediamark_clone_mmk(mediamark_get_current_mmk());
      
      if(mmk) {
	mediamark_replace_entry(&gGui->playlist.mmk[gGui->playlist.cur], mmk->mrl, mmk->ident, 
				file, mmk->start, mmk->end, mmk->av_offset, mmk->spu_offset);
	mediamark_free_mmk(&mmk);

	mmk = mediamark_get_current_mmk();
	gui_set_current_mmk(mmk);
	
	playlist_mrlident_toggle();
	
	if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY) {
	  int curpos;
	  xine_close (gGui->spu_stream);

	  if(xine_open(gGui->spu_stream, mmk->sub))
	    xine_stream_master_slave(gGui->stream, 
				     gGui->spu_stream, XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
	  
	  if(gui_xine_get_pos_length(gGui->stream, &curpos, NULL, NULL)) {
	    xine_stop(gGui->stream);
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
  
  if(gGui->playlist.num) {
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
	  open_path = strdup(gGui->curdir);
	
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
  char *buffer;

  asprintf(&buffer, "%s/%s", XINE_VISDIR, "default.avi");
  
  gGui->visual_anim.mrls = (char **) xine_xmalloc(sizeof(char *) * 3);
  gGui->visual_anim.num_mrls = 0;
  
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++]   = buffer;
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]     = NULL;
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls + 1] = NULL;
}
void visual_anim_add_animation(char *mrl) {
  gGui->visual_anim.mrls = (char **) realloc(gGui->visual_anim.mrls, sizeof(char *) * 
					     ((gGui->visual_anim.num_mrls + 1) + 2));
  
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++]   = strdup(mrl);
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]     = NULL;
  gGui->visual_anim.mrls[gGui->visual_anim.num_mrls + 1] = NULL;
}
static int visual_anim_open_and_play(xine_stream_t *stream, const char *mrl) {
  if((!xine_open(stream, mrl)) || (!xine_play(stream, 0, 0)))
    return 0;

  return 1;
}
void visual_anim_play(void) {
  if(gGui->visual_anim.enabled == 2) {
    if(!visual_anim_open_and_play(gGui->visual_anim.stream, 
				  gGui->visual_anim.mrls[gGui->visual_anim.current]))
      gui_handle_xine_error(gGui->visual_anim.stream, 
			    gGui->visual_anim.mrls[gGui->visual_anim.current]);
    gGui->visual_anim.running = 1;
  }
}
void visual_anim_play_next(void) {
  if(gGui->visual_anim.enabled == 2) {
    gGui->visual_anim.current++;
    if(gGui->visual_anim.mrls[gGui->visual_anim.current] == NULL)
      gGui->visual_anim.current = 0;
    visual_anim_play();
  }
}
void visual_anim_stop(void) {
  if(gGui->visual_anim.enabled == 2) {
    xine_stop(gGui->visual_anim.stream);
    gGui->visual_anim.running = 0;
  }
}
