/*
 * Copyright (C) 2000-2002 the xine project
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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>

#include "common.h"
	
#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       500

extern gGui_t          *gGui;

static char            *sinfosfontname     = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char            *lfontname          = "-*-helvetica-bold-r-*-*-11-*-*-*-*-*-*-*";
static char            *btnfontname        = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

typedef struct {
  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *close;

  struct {
    xitk_widget_t      *title;
    xitk_widget_t      *comment;
    xitk_widget_t      *artist;
    xitk_widget_t      *genre;
    xitk_widget_t      *album;
    xitk_widget_t      *year;
    xitk_widget_t      *videocodec;
    xitk_widget_t      *audiocodec;
    xitk_widget_t      *systemlayer;
    xitk_widget_t      *input_plugin;
  } meta_infos;

  struct {
    xitk_widget_t      *bitrate;
    xitk_widget_t      *seekable;
    xitk_widget_t      *video_resolution;
    xitk_widget_t      *video_ratio;
    xitk_widget_t      *video_channels;
    xitk_widget_t      *video_streams;
    xitk_widget_t      *video_bitrate;
    xitk_widget_t      *video_fourcc;
    xitk_widget_t      *video_handled;
    xitk_widget_t      *frame_duration;
    xitk_widget_t      *audio_channels;
    xitk_widget_t      *audio_bits;
    xitk_widget_t      *audio_samplerate;
    xitk_widget_t      *audio_bitrate;
    xitk_widget_t      *audio_fourcc;
    xitk_widget_t      *audio_handled;
    xitk_widget_t      *has_chapters;
    xitk_widget_t      *has_video;
    xitk_widget_t      *has_audio;
    xitk_widget_t      *ignore_video;
    xitk_widget_t      *ignore_audio;
    xitk_widget_t      *ignore_spu;
  } infos;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

} _stream_infos_t;

static _stream_infos_t    *sinfos = NULL;

static char *get_yesno_string(uint32_t val) {
  static char *yesno[] =  { "No", "Yes" };

  return ((val > 0) ? yesno[1] : yesno[0]);
}

static char *get_fourcc_string(uint32_t fourcc) {
  static char fourcc_txt[10];
  
  memset(&fourcc_txt, 0, sizeof(fourcc_txt));
  if(fourcc <= 0xFFFF)
    sprintf(fourcc_txt, "0x%x", fourcc);
  else {
    *(uint32_t *)fourcc_txt = fourcc;
    fourcc_txt[4] = '\0';
  }
  
  return &fourcc_txt[0];
}

static char *get_num_string(uint32_t num) {
  static char buffer[1024];

  memset(&buffer, 0, sizeof(buffer));
  snprintf(buffer, 1023, "%d", num);

  return &buffer[0];
}

char *stream_infos_get_ident_from_stream(xine_stream_t *stream) {
  const char  *title   = NULL;
  char        *atitle  = NULL;
  const char  *album   = NULL;
  char        *aartist = NULL;
  const char  *artist  = NULL;
  char        *aalbum  = NULL; 
  char        *ident   = NULL;
  
  if(!stream)
    return NULL;
  
  title = xine_get_meta_info(stream, XINE_META_INFO_TITLE);
  artist = xine_get_meta_info(stream, XINE_META_INFO_ARTIST);
  album = xine_get_meta_info(stream, XINE_META_INFO_ALBUM);
  
  /*
   * Since meta info can be corrupted/wrong/ugly
   * we need to clean and check them before using.
   * Note: atoa() modify the string, so we work on a copy.
   */
  if(title && strlen(title)) {
    xine_strdupa(atitle, title);
    atitle = atoa(atitle);
  }
  if(artist && strlen(artist)) {
    xine_strdupa(aartist, artist);
    aartist = atoa(aartist);
  }
  if(album && strlen(album)) {
    xine_strdupa(aalbum, album);
    aalbum = atoa(aalbum);
  }

  if(atitle) {
    int len = strlen(atitle);
    
    if(aartist && strlen(aartist))
      len += strlen(aartist) + 3;
    if(aalbum && strlen(aalbum))
      len += strlen(aalbum) + 3;
    
    ident = (char *) xine_xmalloc(len + 1);
    sprintf(ident, "%s", atitle);
    
    if((aartist && strlen(aartist)) || (aalbum && strlen(aalbum))) {
      strcat(ident, " (");
      if(aartist && strlen(aartist))
	sprintf(ident, "%s%s", ident, aartist);
      if((aartist && strlen(aartist)) && (aalbum && strlen(aalbum)))
	strcat(ident, " - ");
      if(aalbum && strlen(aalbum))
	sprintf(ident, "%s%s", ident, aalbum);
      strcat(ident, ")");
    }
  }

  return ident;
}

void stream_infos_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;

  sinfos->running = 0;
  sinfos->visible = 0;

  if((xitk_get_window_info(sinfos->widget_key, &wi))) {
    config_update_num ("gui.sinfos_x", wi.x);
    config_update_num ("gui.sinfos_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  xitk_unregister_event_handler(&sinfos->widget_key);

  XLockDisplay(gGui->display);
  XUnmapWindow(gGui->display, xitk_window_get_window(sinfos->xwin));
  XUnlockDisplay(gGui->display);

  xitk_destroy_widgets(sinfos->widget_list);

  XLockDisplay(gGui->display);
  XDestroyWindow(gGui->display, xitk_window_get_window(sinfos->xwin));
  XUnlockDisplay(gGui->display);

  sinfos->xwin = None;
  xitk_list_free(sinfos->widget_list->l);
  
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, sinfos->widget_list->gc);
  XUnlockDisplay(gGui->display);

  free(sinfos->widget_list);

  free(sinfos);
  sinfos = NULL;
}

int stream_infos_is_visible(void) {
  
  if(sinfos != NULL)
    return sinfos->visible;
  
  return 0;
}

int stream_infos_is_running(void) {
  
  if(sinfos != NULL)
    return sinfos->running;
  
  return 0;
}

void stream_infos_toggle_visibility(xitk_widget_t *w, void *data) {
  if(sinfos != NULL) {
    if (sinfos->visible && sinfos->running) {
      sinfos->visible = 0;
      xitk_hide_widgets(sinfos->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, xitk_window_get_window(sinfos->xwin));
      XUnlockDisplay(gGui->display);
    } else {
      if(sinfos->running) {
	sinfos->visible = 1;
	xitk_show_widgets(sinfos->widget_list);
	XLockDisplay(gGui->display);
	XMapRaised(gGui->display, xitk_window_get_window(sinfos->xwin)); 
	XSetTransientForHint (gGui->display, 
			      xitk_window_get_window(sinfos->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(sinfos->xwin));
      }
    }
  }
}

void stream_infos_raise_window(void) {
  if(sinfos != NULL) {
    if(sinfos->xwin) {
      if(sinfos->visible && sinfos->running) {
	if(sinfos->running) {
	  XLockDisplay(gGui->display);
	  XMapRaised(gGui->display, xitk_window_get_window(sinfos->xwin));
	  sinfos->visible = 1;
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(sinfos->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(sinfos->xwin));
	}
      } else {
	XLockDisplay(gGui->display);
	XUnmapWindow (gGui->display, xitk_window_get_window(sinfos->xwin));
	XUnlockDisplay(gGui->display);
	sinfos->visible = 0;
      }
    }
  }
}

static void stream_infos_end(xitk_widget_t *w, void *data) {
  stream_infos_exit(NULL, NULL);
}

static void stream_info_update_undefined(void) {

  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.title, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.comment, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.artist, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.genre, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.album, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.year, "19--");
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.videocodec, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.audiocodec, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.systemlayer, _("Unavailable"));
  xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.input_plugin, _("Unavailable"));
  /* */
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.bitrate, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.seekable, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_resolution, "--- X ---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_ratio, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_channels, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_streams, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_bitrate, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_fourcc, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_handled, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.frame_duration, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_channels, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_bits, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_samplerate, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_bitrate, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_fourcc, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_handled, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_chapters, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_video, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_audio, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_video, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_audio, "---");
  xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_spu, "---");
}

void stream_infos_update_infos(void) {

  if(sinfos != NULL) {
    stream_info_update_undefined();
    if(!gGui->logo_mode) {
      const char *minfo;
      uint32_t    iinfo, iinfo2;
      char        buffer[1024];

      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_TITLE)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.title, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_COMMENT)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.comment, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_ARTIST)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.artist, minfo);

      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_GENRE)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.genre, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_ALBUM)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.album, minfo);

      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_YEAR)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.year, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_VIDEOCODEC)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.videocodec, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_AUDIOCODEC)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.audiocodec, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_SYSTEMLAYER)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.systemlayer, minfo);
      
      if((minfo = xine_get_meta_info(gGui->stream, XINE_META_INFO_INPUT_PLUGIN)) != NULL)
	xitk_label_change_label(sinfos->widget_list, sinfos->meta_infos.input_plugin, minfo);

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_BITRATE);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.bitrate, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_SEEKABLE);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.seekable, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
      iinfo2 = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
      memset(&buffer, 0, sizeof(buffer));
      sprintf(buffer, "%d X %d", iinfo, iinfo2);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_resolution, buffer);

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_RATIO);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_ratio, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_CHANNELS);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_channels, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_STREAMS);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_streams, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_BITRATE);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_bitrate, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_FOURCC);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_fourcc, (get_fourcc_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_HANDLED);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.video_handled, (get_yesno_string(iinfo)));;

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_FRAME_DURATION);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.frame_duration, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_CHANNELS);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_channels, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_BITS);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_bits, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_samplerate, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_BITRATE);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_bitrate, (get_num_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_FOURCC);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_fourcc, (get_fourcc_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_HANDLED);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.audio_handled, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_HAS_CHAPTERS);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_chapters, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_HAS_VIDEO);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_video, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_HAS_AUDIO);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.has_audio, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_IGNORE_VIDEO);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_video, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_IGNORE_AUDIO);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_audio, (get_yesno_string(iinfo)));

      iinfo = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_IGNORE_SPU);
      xitk_label_change_label(sinfos->widget_list, sinfos->infos.ignore_spu, (get_yesno_string(iinfo)));
    }
  }
}

static void stream_infos_update(xitk_widget_t *w, void *data) {
  stream_infos_update_infos();
}

void stream_infos_panel(void) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  int                         x, y, w, width, height;

  sinfos = (_stream_infos_t *) xine_xmalloc(sizeof(_stream_infos_t));
  
  x = xine_config_register_num (gGui->xine, "gui.sinfos_x", 
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_BEG,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.sinfos_y",
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_BEG,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  /* Create window */
  sinfos->xwin = xitk_window_create_dialog_window(gGui->imlib_data, _("stream informations"), x, y,
						  WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(sinfos->xwin)), None, None);
  
  sinfos->widget_list                = xitk_widget_list_new();
  sinfos->widget_list->l             = xitk_list_new ();
  sinfos->widget_list->win           = (xitk_window_get_window(sinfos->xwin));
  sinfos->widget_list->gc            = gc;
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  xitk_window_get_window_size(sinfos->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
  XCopyArea(gGui->display, (xitk_window_get_background(sinfos->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);

  x = 5;
  y = 35;
  
  draw_outter_frame(gGui->imlib_data, bg, _("General"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (4 * 20) + (4 * 15) + 15 + 5);

  /* First Line */
  x = 15;
  y += 15;
  w = WINDOW_WIDTH - 30;
  draw_inner_frame(gGui->imlib_data, bg, _("Title: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.title = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* New Line */
  y += 35;
  w = WINDOW_WIDTH - 30;
  draw_inner_frame(gGui->imlib_data, bg, _("Comment: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.comment = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* New Line */
  y += 35;
  w = (WINDOW_WIDTH - 60) >> 1;
  draw_inner_frame(gGui->imlib_data, bg, _("Artist: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.artist = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  w = (WINDOW_WIDTH - 60) >> 2;
  draw_inner_frame(gGui->imlib_data, bg, _("Genre: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.genre = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Year: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.year = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* New Line */
  x = 15;
  y += 35;
  w = (WINDOW_WIDTH - 60) >> 1;
  draw_inner_frame(gGui->imlib_data, bg, _("Album: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.album = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* frame space */
  x = 5;
  y += 40;
  draw_outter_frame(gGui->imlib_data, bg, _("Misc"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + (2 * 15) + 15 + 5);
  /* New Line */
  y += 15;
  x = 15;
  w = (WINDOW_WIDTH - 75) >> 2;
  draw_inner_frame(gGui->imlib_data, bg, _("Input Plugin: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.input_plugin = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("System Layer: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.systemlayer = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Bitrate: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.bitrate = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Frame Duration: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.frame_duration = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* New Line */
  y += 35;
  x = 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Is Seekable: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.seekable = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Has Chapters: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.has_chapters = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Ignore Spu: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.ignore_spu = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* video frame */
  x = 5;
  y += 40;
  draw_outter_frame(gGui->imlib_data, bg, "Video", btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + (2 * 15) + 15 + 5);

  /* New Line */
  y += 15;
  x = 15;

  w = (WINDOW_WIDTH - 90) / 5;
  draw_inner_frame(gGui->imlib_data, bg, _("Has: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.has_video = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Handled: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_handled = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Ignore: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.ignore_video = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  x += w + 15;
  w = (w * 2) + 15 - 1;
  draw_inner_frame(gGui->imlib_data, bg, _("Codec: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.videocodec = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* New Line */
  y += 35;
  x = 15;
  w = (WINDOW_WIDTH - 105) / 6;
  draw_inner_frame(gGui->imlib_data, bg, _("FourCC: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_fourcc = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Channel(s): "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_channels = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Bitrate: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_bitrate = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  w += 4;
  draw_inner_frame(gGui->imlib_data, bg, _("Resolution: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_resolution = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  x += w + 15;
  w -= 4;
  draw_inner_frame(gGui->imlib_data, bg, _("Ratio: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_ratio = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Stream(s): "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.video_streams = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  /* Audio Frame */
  x = 5;
  y += 40;
  draw_outter_frame(gGui->imlib_data, bg, _("Audio"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + (2 * 15) + 15 + 5);

  /* New Line */
  y += 15;
  x = 15;

  w = (WINDOW_WIDTH - 90) / 5;
  draw_inner_frame(gGui->imlib_data, bg, _("Has: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.has_audio = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Handled: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_handled = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Ignore: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.ignore_audio = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  x += w + 15;
  w = (w * 2) + 15 - 1;
  draw_inner_frame(gGui->imlib_data, bg, _("Codec: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->meta_infos.audiocodec = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  
  /* New Line */
  y += 35;
  x = 15;
  w = (WINDOW_WIDTH - 90) / 5;
  draw_inner_frame(gGui->imlib_data, bg, _("FourCC: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_fourcc = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Channel(s): "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_channels = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Bitrate: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_bitrate = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  draw_inner_frame(gGui->imlib_data, bg, _("Bits: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_bits = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));

  x += w + 15;
  w -= 1;
  draw_inner_frame(gGui->imlib_data, bg, _("Samplerate: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  lbl.window            = xitk_window_get_window(sinfos->xwin);
  lbl.gc                = sinfos->widget_list->gc;
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
			   (sinfos->infos.audio_samplerate = 
			    xitk_noskin_label_create(sinfos->widget_list, &lbl,
						     x, y, w, 20, sinfosfontname)));
  

  /*  */
  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Update");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = stream_infos_update; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
	   xitk_noskin_labelbutton_create(sinfos->widget_list, 
					  &lb, x, y, 100, 23,
					  "Black", "Black", "White", btnfontname));
  x = WINDOW_WIDTH - 115;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = stream_infos_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(sinfos->widget_list->l, 
	   xitk_noskin_labelbutton_create(sinfos->widget_list, 
					  &lb, x, y, 100, 23,
					  "Black", "Black", "White", btnfontname));
  
  xitk_window_change_background(gGui->imlib_data, sinfos->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);

  XMapRaised(gGui->display, xitk_window_get_window(sinfos->xwin));
  XUnlockDisplay (gGui->display);

  sinfos->widget_key = xitk_register_event_handler("sinfos", 
						   (xitk_window_get_window(sinfos->xwin)),
						   NULL,
						   NULL,
						   NULL,
						   sinfos->widget_list,
						   NULL);
  
  stream_infos_update_infos();

  sinfos->visible = 1;
  sinfos->running = 1;
  stream_infos_raise_window();

  XLockDisplay (gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(sinfos->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay (gGui->display);
}
/*
  titre           1
  commentaire     1
  artiste         1/2
  genre           1/4
  annee           1/4      
  album           1/2      3,5L (4)

 +- general frame ----------------------+
 |
 |  input plugin    1/4
 |  system layer    1/4
 |  bitrate         1/4
  frame duration    1/4
  is seekable     1/4
  has chapter     1/4     1,5++L (2)
  ignore spu      1/4     


 +- video frame ----------------------+
 |
 |  has video    1/5
 |  video handled  1/5
 |  ignore video  1/5
 |  codec video  1/3    --1L
  video fourcc     1/6
  video channels   1/6
  video bitrate     ""
      video width   "" merge
      video height ""   merge
  video ratio      ""
  video streams    ""       2L
  
 +- audio frame ----------------------+
 |
 |  has audio 1/5
 |  audio handled 1/5
 |  ignore audio 1/5
 |  codec audio   1/3   --1L
 |  audio fourcc  1/5
  audio channels 1/5
  audio bitrate  "" 
  audio bits     "" 
  audio samplerate  ""   2L
  


 */
