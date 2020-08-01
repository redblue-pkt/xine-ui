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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <X11/keysym.h>

#include "common.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/labelbutton.h"
#include "recode.h"
	
#define WINDOW_WIDTH        570
#define WINDOW_HEIGHT       616

#define METAINFO_CHARSET    "UTF-8"


struct {
  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *close;
  xitk_widget_t        *update;

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
    xitk_widget_t      *has_still;
  } infos;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

} sinfos;

static void stream_infos_update(xitk_widget_t *w, void *data) {
  stream_infos_update_infos();
}

static void set_label(xitk_widget_t *w, const char *label) {
  xitk_label_change_label(w, label);
}

static const char *get_yesno_string(uint32_t val) {
  static const char yesno[][4] =  { "No", "Yes" };

  return ((val > 0) ? yesno[1] : yesno[0]);
}

char *get_fourcc_string(char *fcc, size_t fcc_size, uint32_t f) {

  if (fcc_size < 5) {
    *fcc = 0;
    return fcc;
  }

  /* Should we take care about endianess ? */
  fcc[0] = f     | 0xFFFFFF00;
  fcc[1] = f>>8  | 0xFFFFFF00;
  fcc[2] = f>>16 | 0xFFFFFF00;
  fcc[3] = f>>24 | 0xFFFFFF00;
  fcc[4] = 0;
  
  if(f <= 0xFFFF)
    snprintf(fcc, fcc_size, "0x%x", f);
  
  if(memcmp(fcc, "ms\0\x55", 4) == 0) {
    strcpy(fcc, "3pm."); /* Force to '.mp3' */
  }
  
  return fcc;
}

static void get_meta_info(xitk_widget_t *w, int meta) {
  gGui_t *gui = gGui;
  char *minfo;
  xitk_recode_t *xr;
  
  xr = xitk_recode_init (METAINFO_CHARSET, NULL, 0);
  
  minfo = (char *)xine_get_meta_info(gui->stream, meta);
  if(minfo)
    minfo = xitk_recode(xr, minfo);
  set_label(w, (minfo) ? (char *) minfo : _("Unavailable"));
  
  free(minfo);
  xitk_recode_done(xr);
}

static void get_stream_info(xitk_widget_t *w, int info) {
  gGui_t *gui = gGui;
  char tmp[32];

  snprintf(tmp, sizeof(tmp), "%d", xine_get_stream_info(gui->stream, info));

  set_label(w, tmp);
}
      
static void get_stream_fourcc_info(xitk_widget_t *w, int info) {
  gGui_t *gui = gGui;
  uint32_t   iinfo;
  char       tmp[8];

  iinfo = xine_get_stream_info(gui->stream, info);
  set_label(w, (get_fourcc_string(tmp, sizeof(tmp), iinfo)));
}
      
static void get_stream_yesno_info(xitk_widget_t *w, int info) {
  gGui_t *gui = gGui;
  uint32_t   iinfo;
  
  iinfo = xine_get_stream_info(gui->stream, info);
  set_label(w, (get_yesno_string(iinfo)));
}

static void get_stream_video_resolution_info(void) {
  gGui_t *gui = gGui;
  uint32_t    video_w, video_h;
  char        buffer[1024];

  video_w = xine_get_stream_info(gui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
  video_h = xine_get_stream_info(gui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);

  snprintf(buffer, sizeof(buffer), "%d X %d", video_w, video_h);
  set_label(sinfos.infos.video_resolution, buffer);
}

char *stream_infos_get_ident_from_stream(xine_stream_t *stream) {
  char        *title   = NULL;
  char        *atitle  = NULL;
  char        *album   = NULL;
  char        *aartist = NULL;
  char        *artist  = NULL;
  char        *aalbum  = NULL; 
  char        *ident   = NULL;
  xitk_recode_t *xr;
  
  if(!stream)
    return NULL;
  
  title = (char *)xine_get_meta_info(stream, XINE_META_INFO_TITLE);
  artist = (char *)xine_get_meta_info(stream, XINE_META_INFO_ARTIST);
  album = (char *)xine_get_meta_info(stream, XINE_META_INFO_ALBUM);
  
  xr = xitk_recode_init (METAINFO_CHARSET, NULL, 0);
  if(title)
    title = xitk_recode(xr, title);
  if(artist)
    artist = xitk_recode(xr, artist);
  if(album)
    album = xitk_recode(xr, album);
  xitk_recode_done(xr);
  
  /*
   * Since meta info can be corrupted/wrong/ugly
   * we need to clean and check them before using.
   * Note: atoa() modify the string, so we work on a copy.
   */
  if(title && strlen(title)) {
    atitle = atoa(title);
    if ( ! *atitle )
      atitle = strdup(title);
    else
      atitle = strdup(atitle);
  }
  if(artist && strlen(artist)) {
    aartist = atoa(artist);
    if ( ! *aartist )
      aartist = strdup(artist);
    else
      aartist = strdup(aartist);
  }
  if(album && strlen(album)) {
    aalbum = atoa(album);
    if ( ! *aalbum )
      aalbum = strdup(album);
    else
      aalbum = strdup(aalbum);
  }
  free(title);
  free(artist);
  free(album);

  if(atitle) {
    int len = strlen(atitle) + 1;
    
    if(aartist && strlen(aartist))
      len += strlen(aartist) + 3;
    if(aalbum && strlen(aalbum))
      len += strlen(aalbum) + 3;
    
    ident = (char *) malloc(len + 1);
    strcpy(ident, atitle);
    
    if((aartist && strlen(aartist)) || (aalbum && strlen(aalbum))) {
      strlcat(ident, " (", len);
      if(aartist && strlen(aartist))
	strlcat(ident, aartist, len);
      if((aartist && strlen(aartist)) && (aalbum && strlen(aalbum)))
	strlcat(ident, " - ", len);
      if(aalbum && strlen(aalbum))
	strlcat(ident, aalbum, len);
      strlcat(ident, ")", len);
    }
  }
  free(atitle);
  free(aartist);
  free(aalbum);

  return ident;
}

static void stream_infos_exit(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  window_info_t wi;
    
  if ( ! sinfos.running ) return;

    sinfos.running = 0;
    sinfos.visible = 0;
    
    if((xitk_get_window_info(sinfos.widget_key, &wi))) {
      config_update_num ("gui.sinfos_x", wi.x);
      config_update_num ("gui.sinfos_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&sinfos.widget_key);

    xitk_window_destroy_window(sinfos.xwin);
    sinfos.xwin = NULL;
    /* xitk_dlist_init (&sinfos.widget_list.list); */

    video_window_set_input_focus(gui->vwin);
}

static void stream_infos_handle_key_event(void *data, const xitk_key_event_t *ke) {

  if (ke->event == XITK_KEY_PRESS) {
    if (ke->key_pressed == XK_Escape)
      stream_infos_exit(NULL, NULL);
    else
      gui_handle_key_event (gGui, ke);
  }
}

static const xitk_event_cbs_t stream_infos_event_cbs = {
  .key_cb            = stream_infos_handle_key_event,
};

int stream_infos_is_visible(void) {
  gGui_t *gui = gGui;
  
    if(gui->use_root_window)
      return xitk_window_is_window_visible(sinfos.xwin);
    else
      return sinfos.visible && xitk_window_is_window_visible(sinfos.xwin);
  
  return 0;
}

int stream_infos_is_running(void) {
  return sinfos.running;
}

void stream_infos_toggle_auto_update(void) {
  gGui_t *gui = gGui;
    if(gui->stream_info_auto_update) {
      xitk_hide_widget(sinfos.update);
      xitk_window_clear_window(sinfos.xwin);
      xitk_paint_widget_list(sinfos.widget_list);
    }
    else
      xitk_show_widget(sinfos.update);
}

void stream_infos_raise_window(void) {
    raise_window(sinfos.xwin, sinfos.visible, sinfos.running);
}

void stream_infos_toggle_visibility(xitk_widget_t *w, void *data) {
    toggle_window(sinfos.xwin, sinfos.widget_list, &sinfos.visible, sinfos.running);
}

void stream_infos_end(void) {
  stream_infos_exit(NULL, NULL);
}

static void stream_info_update_undefined(void) {
  const char *const unavail = _("Unavailable");

  set_label(sinfos.meta_infos.title, unavail);
  set_label(sinfos.meta_infos.comment, unavail);
  set_label(sinfos.meta_infos.artist, unavail);
  set_label(sinfos.meta_infos.genre, unavail);
  set_label(sinfos.meta_infos.album, unavail);
  set_label(sinfos.meta_infos.year, "19__");
  set_label(sinfos.meta_infos.videocodec, unavail);
  set_label(sinfos.meta_infos.audiocodec, unavail);
  set_label(sinfos.meta_infos.systemlayer, unavail);
  set_label(sinfos.meta_infos.input_plugin, unavail);
  /* */
  set_label(sinfos.infos.bitrate, "---");
  set_label(sinfos.infos.seekable, "---");
  set_label(sinfos.infos.video_resolution, "--- X ---");
  set_label(sinfos.infos.video_ratio, "---");
  set_label(sinfos.infos.video_channels, "---");
  set_label(sinfos.infos.video_streams, "---");
  set_label(sinfos.infos.video_bitrate, "---");
  set_label(sinfos.infos.video_fourcc, "---");
  set_label(sinfos.infos.video_handled, "---");
  set_label(sinfos.infos.frame_duration, "---");
  set_label(sinfos.infos.audio_channels, "---");
  set_label(sinfos.infos.audio_bits, "---");
  set_label(sinfos.infos.audio_samplerate, "---");
  set_label(sinfos.infos.audio_bitrate, "---");
  set_label(sinfos.infos.audio_fourcc, "---");
  set_label(sinfos.infos.audio_handled, "---");
  set_label(sinfos.infos.has_chapters, "---");
  set_label(sinfos.infos.has_video, "---");
  set_label(sinfos.infos.has_audio, "---");
  set_label(sinfos.infos.ignore_video, "---");
  set_label(sinfos.infos.ignore_audio, "---");
  set_label(sinfos.infos.ignore_spu, "---");
  set_label(sinfos.infos.has_still, "---");
}

void stream_infos_update_infos(void) {
  gGui_t *gui = gGui;
    if(!gui->logo_mode) {
      
      get_meta_info(sinfos.meta_infos.title, XINE_META_INFO_TITLE);
      get_meta_info(sinfos.meta_infos.comment, XINE_META_INFO_COMMENT);
      get_meta_info(sinfos.meta_infos.artist, XINE_META_INFO_ARTIST);
      get_meta_info(sinfos.meta_infos.genre, XINE_META_INFO_GENRE);
      get_meta_info(sinfos.meta_infos.album, XINE_META_INFO_ALBUM);
      get_meta_info(sinfos.meta_infos.year, XINE_META_INFO_YEAR);
      get_meta_info(sinfos.meta_infos.videocodec, XINE_META_INFO_VIDEOCODEC);
      get_meta_info(sinfos.meta_infos.audiocodec, XINE_META_INFO_AUDIOCODEC);
      get_meta_info(sinfos.meta_infos.systemlayer, XINE_META_INFO_SYSTEMLAYER);
      get_meta_info(sinfos.meta_infos.input_plugin, XINE_META_INFO_INPUT_PLUGIN);
      
      get_stream_info(sinfos.infos.bitrate, XINE_STREAM_INFO_BITRATE);
      get_stream_yesno_info(sinfos.infos.seekable, XINE_STREAM_INFO_SEEKABLE);
      get_stream_video_resolution_info();
      get_stream_info(sinfos.infos.video_ratio, XINE_STREAM_INFO_VIDEO_RATIO);
      get_stream_info(sinfos.infos.video_channels, XINE_STREAM_INFO_VIDEO_CHANNELS);
      get_stream_info(sinfos.infos.video_streams, XINE_STREAM_INFO_VIDEO_STREAMS);
      get_stream_info(sinfos.infos.video_bitrate, XINE_STREAM_INFO_VIDEO_BITRATE);
      get_stream_fourcc_info(sinfos.infos.video_fourcc, XINE_STREAM_INFO_VIDEO_FOURCC);
      get_stream_yesno_info(sinfos.infos.video_handled, XINE_STREAM_INFO_VIDEO_HANDLED);
      get_stream_info(sinfos.infos.frame_duration, XINE_STREAM_INFO_FRAME_DURATION);
      get_stream_info(sinfos.infos.audio_channels, XINE_STREAM_INFO_AUDIO_CHANNELS);
      get_stream_info(sinfos.infos.audio_bits, XINE_STREAM_INFO_AUDIO_BITS);
      get_stream_info(sinfos.infos.audio_samplerate, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
      get_stream_info(sinfos.infos.audio_bitrate, XINE_STREAM_INFO_AUDIO_BITRATE);
      get_stream_fourcc_info(sinfos.infos.audio_fourcc, XINE_STREAM_INFO_AUDIO_FOURCC);
      get_stream_yesno_info(sinfos.infos.audio_handled, XINE_STREAM_INFO_AUDIO_HANDLED);
      get_stream_yesno_info(sinfos.infos.has_chapters, XINE_STREAM_INFO_HAS_CHAPTERS);
      get_stream_yesno_info(sinfos.infos.has_video, XINE_STREAM_INFO_HAS_VIDEO);
      get_stream_yesno_info(sinfos.infos.has_audio, XINE_STREAM_INFO_HAS_AUDIO);
      get_stream_yesno_info(sinfos.infos.ignore_video, XINE_STREAM_INFO_IGNORE_VIDEO);
      get_stream_yesno_info(sinfos.infos.ignore_audio, XINE_STREAM_INFO_IGNORE_AUDIO);
      get_stream_yesno_info(sinfos.infos.ignore_spu, XINE_STREAM_INFO_IGNORE_SPU);
      get_stream_yesno_info(sinfos.infos.has_still, XINE_STREAM_INFO_VIDEO_HAS_STILL);
    }
    else
      stream_info_update_undefined();
}

void stream_infos_reparent(void) {
  gGui_t *gui = gGui;
  reparent_window(gui, sinfos.xwin);
}

void stream_infos_panel(void) {
  gGui_t *gui = gGui;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  int                         x, y, w;
  xitk_widget_t              *widget;

  x = xine_config_register_num (__xineui_global_xine_instance, "gui.sinfos_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, "gui.sinfos_y",
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  /* Create window */
  sinfos.xwin = xitk_window_create_dialog_window(gui->xitk, _("Stream Information"), x, y,
						  WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start(gui, sinfos.xwin);

  sinfos.widget_list = xitk_window_widget_list(sinfos.xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&lbl);
  lbl.skin_element_name = NULL;
  lbl.label             = "";
  lbl.callback          = NULL;
  XITK_WIDGET_INIT(&cb);

  bg = xitk_window_get_background_pixmap(sinfos.xwin);

  x = 15;
  y = 34 - 6;
  
  draw_outter_frame(bg, _("General"), btnfontname,
		    x, y, WINDOW_WIDTH - 30, (4 * ((20 + 22) + 3) - 3 + 5 + 2) + 15);

  /* First Line */
  x = 20;
  y += 15;
  w = WINDOW_WIDTH - 40 - 1;
  draw_inner_frame(bg, _("Title: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.title = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.title);
  xitk_enable_and_show_widget(sinfos.meta_infos.title);

  /* New Line */
  y += (20 + 22) + 3;
  draw_inner_frame(bg, _("Comment: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.comment = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.comment);
  xitk_enable_and_show_widget(sinfos.meta_infos.comment);

  /* New Line */
  y += (20 + 22) + 3;
  w = (((WINDOW_WIDTH - 40 - 3 * 5) / 4) * 2) + 5 + 1;
  draw_inner_frame(bg, _("Artist: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.artist = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.artist);
  xitk_enable_and_show_widget(sinfos.meta_infos.artist);

  x += w + 5;
  w = ((WINDOW_WIDTH - 40 - 3 * 5) / 4) + 1;
  draw_inner_frame(bg, _("Genre: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.genre = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.genre);
  xitk_enable_and_show_widget(sinfos.meta_infos.genre);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Year: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.year = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.year);
  xitk_enable_and_show_widget(sinfos.meta_infos.year);

  /* New Line */
  x = 20;
  y += (20 + 22) + 3;
  w = (((WINDOW_WIDTH - 40 - 3 * 5) / 4) * 2) + 5 + 1;
  draw_inner_frame(bg, _("Album: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.album = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.album);
  xitk_enable_and_show_widget(sinfos.meta_infos.album);

  /* frame space */
  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  draw_outter_frame(bg, _("Misc"), btnfontname,
		    x, y, WINDOW_WIDTH - 30, (2 * ((20 + 22) + 3) - 3 + 5 + 2) + 15);
  /* New Line */
  x = 20;
  y += 15;
  w = (WINDOW_WIDTH - 40 - 3 * 5) / 4;
  draw_inner_frame(bg, _("Input Plugin: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.input_plugin = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.input_plugin);
  xitk_enable_and_show_widget(sinfos.meta_infos.input_plugin);

  x += w + 5;
  w += 1;
  draw_inner_frame(bg, _("System Layer: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.systemlayer = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.systemlayer);
  xitk_enable_and_show_widget(sinfos.meta_infos.systemlayer);

  x += w + 5;
  draw_inner_frame(bg, _("Bitrate: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.bitrate = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.bitrate);
  xitk_enable_and_show_widget(sinfos.infos.bitrate);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Frame Duration: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.frame_duration = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.frame_duration);
  xitk_enable_and_show_widget(sinfos.infos.frame_duration);

  /* New Line */
  x = 20;
  y += (20 + 22) + 3;
  draw_inner_frame(bg, _("Is Seekable: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.seekable = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.seekable);
  xitk_enable_and_show_widget(sinfos.infos.seekable);

  x += w + 5;
  w += 1;
  draw_inner_frame(bg, _("Has Chapters: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.has_chapters = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.has_chapters);
  xitk_enable_and_show_widget(sinfos.infos.has_chapters);

  x += w + 5;
  draw_inner_frame(bg, _("Ignore Spu: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.ignore_spu = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.ignore_spu);
  xitk_enable_and_show_widget(sinfos.infos.ignore_spu);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Has Still: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.has_still = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.has_still);
  xitk_enable_and_show_widget(sinfos.infos.has_still);

  /* video frame */
  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  draw_outter_frame(bg, _("Video"), btnfontname,
		    x, y, WINDOW_WIDTH - 30, (2 * ((20 + 22) + 3) - 3 + 5 + 2) + 15);

  /* New Line */
  x = 20;
  y += 15;
  w = (WINDOW_WIDTH - 40 - 4 * 5) / 5;
  draw_inner_frame(bg, _("Has: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.has_video = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.has_video);
  xitk_enable_and_show_widget(sinfos.infos.has_video);

  x += w + 5;
  draw_inner_frame(bg, _("Handled: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_handled = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_handled);
  xitk_enable_and_show_widget(sinfos.infos.video_handled);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Ignore: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.ignore_video = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.ignore_video);
  xitk_enable_and_show_widget(sinfos.infos.ignore_video);

  x += w + 5;
  w += 1;
  w = (w * 2) + 5;
  draw_inner_frame(bg, _("Codec: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.videocodec = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.videocodec);
  xitk_enable_and_show_widget(sinfos.meta_infos.videocodec);

  /* New Line */
  x = 20;
  y += (20 + 22) + 3;
  w = (WINDOW_WIDTH - 40 - 5 * 5) / 6;
  draw_inner_frame(bg, _("FourCC: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_fourcc = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_fourcc);
  xitk_enable_and_show_widget(sinfos.infos.video_fourcc);
  
  x += w + 5;
  draw_inner_frame(bg, _("Channel(s): "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_channels = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_channels);
  xitk_enable_and_show_widget(sinfos.infos.video_channels);
  
  x += w + 5;
  draw_inner_frame(bg, _("Bitrate: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_bitrate = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_bitrate);
  xitk_enable_and_show_widget(sinfos.infos.video_bitrate);

  x += w + 5;
  draw_inner_frame(bg, _("Resolution: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_resolution = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_resolution);
  xitk_enable_and_show_widget(sinfos.infos.video_resolution);

  x += w + 5;
  draw_inner_frame(bg, _("Ratio: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_ratio = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_ratio);
  xitk_enable_and_show_widget(sinfos.infos.video_ratio);
  
  x += w + 5;
  draw_inner_frame(bg, _("Stream(s): "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.video_streams = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.video_streams);
  xitk_enable_and_show_widget(sinfos.infos.video_streams);

  /* Audio Frame */
  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  draw_outter_frame(bg, _("Audio"), btnfontname,
		    x, y, WINDOW_WIDTH - 30, (2 * ((20 + 22) + 3) - 3 + 5 + 2) + 15);

  /* New Line */
  x = 20;
  y += 15;
  w = (WINDOW_WIDTH - 40 - 4 * 5) / 5;
  draw_inner_frame(bg, _("Has: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.has_audio = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.has_audio);
  xitk_enable_and_show_widget(sinfos.infos.has_audio);

  x += w + 5;
  draw_inner_frame(bg, _("Handled: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_handled = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_handled);
  xitk_enable_and_show_widget(sinfos.infos.audio_handled);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Ignore: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.ignore_audio = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.ignore_audio);
  xitk_enable_and_show_widget(sinfos.infos.ignore_audio);

  x += w + 5;
  w += 1;
  w = (w * 2) + 5;
  draw_inner_frame(bg, _("Codec: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.meta_infos.audiocodec = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.meta_infos.audiocodec);
  xitk_enable_and_show_widget(sinfos.meta_infos.audiocodec);
  
  /* New Line */
  x = 20;
  y += (20 + 22) + 3;
  w = (WINDOW_WIDTH - 40 - 4 * 5) / 5;
  draw_inner_frame(bg, _("FourCC: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_fourcc = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_fourcc);
  xitk_enable_and_show_widget(sinfos.infos.audio_fourcc);

  x += w + 5;
  draw_inner_frame(bg, _("Channel(s): "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_channels = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_channels);
  xitk_enable_and_show_widget(sinfos.infos.audio_channels);

  x += w + 5;
  w -= 1;
  draw_inner_frame(bg, _("Bitrate: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_bitrate = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_bitrate);
  xitk_enable_and_show_widget(sinfos.infos.audio_bitrate);

  x += w + 5;
  w += 1;
  draw_inner_frame(bg, _("Bits: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_bits = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_bits);
  xitk_enable_and_show_widget(sinfos.infos.audio_bits);

  x += w + 5;
  draw_inner_frame(bg, _("Samplerate: "), lfontname,
		   x, y, w, (20 + 22));
  sinfos.infos.audio_samplerate = xitk_noskin_label_create (sinfos.widget_list, &lbl,
    x + 5, y + 15, w - 10, 20, fontname);
  xitk_add_widget (sinfos.widget_list, sinfos.infos.audio_samplerate);
  xitk_enable_and_show_widget(sinfos.infos.audio_samplerate);

  /*  */
  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Update");
  lb.align             = ALIGN_CENTER;
  lb.callback          = stream_infos_update; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  sinfos.update = xitk_noskin_labelbutton_create (sinfos.widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (sinfos.widget_list, sinfos.update);
  xitk_enable_and_show_widget(sinfos.update);

  if(gui->stream_info_auto_update)
    xitk_hide_widget(sinfos.update);
  
  x = WINDOW_WIDTH - (100 + 15);
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = stream_infos_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  widget =  xitk_noskin_labelbutton_create (sinfos.widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (sinfos.widget_list, widget);
  xitk_enable_and_show_widget(widget);

  xitk_window_set_background(sinfos.xwin, bg);

  sinfos.widget_key = xitk_window_register_event_handler("sinfos", sinfos.xwin, &stream_infos_event_cbs, &sinfos);
  
  stream_infos_update_infos();

  sinfos.visible = 1;
  sinfos.running = 1;
  stream_infos_raise_window();

  xitk_window_try_to_set_input_focus(sinfos.xwin);
}
