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
#include "stream_infos.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/labelbutton.h"
#include "recode.h"

#define WINDOW_WIDTH        570
#define WINDOW_HEIGHT       616

#define METAINFO_CHARSET    "UTF-8"

typedef enum {
  SINF_STRING = 0,
  SINF_title = SINF_STRING,
  SINF_comment,
  SINF_artist,
  SINF_genre,
  SINF_album,
  SINF_year,
  SINF_vcodec,
  SINF_acodec,
  SINF_systemlayer,
  SINF_input_plugin,
  SINF_BOOL,
  SINF_seekable = SINF_BOOL,
  SINF_has_chapters,
  SINF_has_still,
  SINF_has_audio,
  SINF_ignore_audio,
  SINF_audio_handled,
  SINF_has_video,
  SINF_ignore_video,
  SINF_video_handled,
  SINF_ignore_spu,
  SINF_INT,
  SINF_bitrate = SINF_INT,
  SINF_video_ratio,
  SINF_video_channels,
  SINF_video_streams,
  SINF_video_bitrate,
  SINF_frame_duration,
  SINF_audio_channels,
  SINF_audio_bits,
  SINF_audio_samplerate,
  SINF_audio_bitrate,
  SINF_FOURCC,
  SINF_video_fourcc = SINF_FOURCC,
  SINF_audio_fourcc,
  SINF_END
} sinf_index_t;

static const int sinf_xine_type[SINF_END] = {
  [SINF_title]            = XINE_META_INFO_TITLE,
  [SINF_comment]          = XINE_META_INFO_COMMENT,
  [SINF_artist]           = XINE_META_INFO_ARTIST,
  [SINF_genre]            = XINE_META_INFO_GENRE,
  [SINF_album]            = XINE_META_INFO_ALBUM,
  [SINF_year]             = XINE_META_INFO_YEAR,
  [SINF_vcodec]           = XINE_META_INFO_VIDEOCODEC,
  [SINF_acodec]           = XINE_META_INFO_AUDIOCODEC,
  [SINF_systemlayer]      = XINE_META_INFO_SYSTEMLAYER,
  [SINF_input_plugin]     = XINE_META_INFO_INPUT_PLUGIN,
  [SINF_bitrate]          = XINE_STREAM_INFO_BITRATE,
  [SINF_seekable]         = XINE_STREAM_INFO_SEEKABLE,
  [SINF_video_ratio]      = XINE_STREAM_INFO_VIDEO_RATIO,
  [SINF_video_channels]   = XINE_STREAM_INFO_VIDEO_CHANNELS,
  [SINF_video_streams]    = XINE_STREAM_INFO_VIDEO_STREAMS,
  [SINF_video_bitrate]    = XINE_STREAM_INFO_VIDEO_BITRATE,
  [SINF_video_fourcc]     = XINE_STREAM_INFO_VIDEO_FOURCC,
  [SINF_video_handled]    = XINE_STREAM_INFO_VIDEO_HANDLED,
  [SINF_frame_duration]   = XINE_STREAM_INFO_FRAME_DURATION,
  [SINF_audio_channels]   = XINE_STREAM_INFO_AUDIO_CHANNELS,
  [SINF_audio_bits]       = XINE_STREAM_INFO_AUDIO_BITS,
  [SINF_audio_samplerate] = XINE_STREAM_INFO_AUDIO_SAMPLERATE,
  [SINF_audio_bitrate]    = XINE_STREAM_INFO_AUDIO_BITRATE,
  [SINF_audio_fourcc]     = XINE_STREAM_INFO_AUDIO_FOURCC,
  [SINF_audio_handled]    = XINE_STREAM_INFO_AUDIO_HANDLED,
  [SINF_has_chapters]     = XINE_STREAM_INFO_HAS_CHAPTERS,
  [SINF_has_video]        = XINE_STREAM_INFO_HAS_VIDEO,
  [SINF_has_audio]        = XINE_STREAM_INFO_HAS_AUDIO,
  [SINF_ignore_video]     = XINE_STREAM_INFO_IGNORE_VIDEO,
  [SINF_ignore_audio]     = XINE_STREAM_INFO_IGNORE_AUDIO,
  [SINF_ignore_spu]       = XINE_STREAM_INFO_IGNORE_SPU,
  [SINF_has_still]        = XINE_STREAM_INFO_VIDEO_HAS_STILL
};

static const struct {
  uint16_t x, y, w, h;
  const char *title;
} sinf_f_defs[] = {
  {  15,  28, 540, 199, N_("General")},
  {  15, 230, 540, 109, N_("Misc")},
  {  15, 342, 540, 109, N_("Video")},
  {  15, 454, 540, 109, N_("Audio")},
  {   0,   0,   0,   0, NULL},
  {  20,  43, 529,  42, N_("Title: ")},
  {  20,  88, 529,  42, N_("Comment: ")},
  {  20, 133, 262,  42, N_("Artist: ")},
  { 287, 133, 129,  42, N_("Genre: ")},
  { 421, 133, 128,  42, N_("Year: ")},
  {  20, 178, 262,  42, N_("Album: ")},
  {  20, 245, 128,  42, N_("Input Plugin: ")},
  { 153, 245, 129,  42, N_("System Layer: ")},
  { 287, 245, 129,  42, N_("Bitrate: ")},
  { 421, 245, 128,  42, N_("Frame Duration: ")},
  {  20, 290, 128,  42, N_("Is Seekable: ")},
  { 153, 290, 129,  42, N_("Has Chapters: ")},
  { 287, 290, 129,  42, N_("Ignore Spu: ")},
  { 421, 290, 128,  42, N_("Has Still: ")},
  {  20, 357, 102,  42, N_("Has: ")},
  { 127, 357, 102,  42, N_("Handled: ")},
  { 234, 357, 101,  42, N_("Ignore: ")},
  { 340, 357, 209,  42, N_("Codec: ")},
  {  20, 402,  84,  42, N_("FourCC: ")},
  { 109, 402,  84,  42, N_("Channel(s): ")},
  { 198, 402,  84,  42, N_("Bitrate: ")},
  { 287, 402,  84,  42, N_("Resolution: ")},
  { 376, 402,  84,  42, N_("Ratio: ")},
  { 465, 402,  84,  42, N_("Stream(s): ")},
  {  20, 469, 102,  42, N_("Has: ")},
  { 127, 469, 102,  42, N_("Handled: ")},
  { 234, 469, 101,  42, N_("Ignore: ")},
  { 340, 469, 209,  42, N_("Codec: ")},
  {  20, 514, 102,  42, N_("FourCC: ")},
  { 127, 514, 102,  42, N_("Channel(s): ")},
  { 234, 514, 101,  42, N_("Bitrate: ")},
  { 340, 514, 102,  42, N_("Bits: ")},
  { 447, 514, 102,  42, N_("Samplerate: ")},
  {   0,   0,   0,   0, NULL}
};

static const struct {
  uint16_t x, y, w, h;
} sinf_w_defs[SINF_END + 1] = {
  [SINF_title]            = {  25,  58, 519, 20},
  [SINF_comment]          = {  25, 103, 519, 20},
  [SINF_artist]           = {  25, 148, 252, 20},
  [SINF_genre]            = { 292, 148, 119, 20},
  [SINF_year]             = { 426, 148, 118, 20},
  [SINF_album]            = {  25, 193, 252, 20},
  [SINF_input_plugin]     = {  25, 260, 118, 20},
  [SINF_systemlayer]      = { 158, 260, 119, 20},
  [SINF_vcodec]           = { 345, 372, 199, 20},
  [SINF_acodec]           = { 345, 484, 199, 20},
  [SINF_seekable]         = {  25, 305, 118, 20},
  [SINF_has_chapters]     = { 158, 305, 119, 20},
  [SINF_ignore_spu]       = { 292, 305, 119, 20},
  [SINF_has_still]        = { 426, 305, 118, 20},
  [SINF_has_video]        = {  25, 372,  92, 20},
  [SINF_video_handled]    = { 132, 372,  92, 20},
  [SINF_ignore_video]     = { 239, 372,  92, 20},
  [SINF_has_audio]        = {  25, 484,  92, 20},
  [SINF_audio_handled]    = { 132, 484,  92, 20},
  [SINF_ignore_audio]     = { 239, 484,  92, 20},
  [SINF_bitrate]          = { 292, 260, 119, 20},
  [SINF_frame_duration]   = { 426, 260, 118, 20},
  [SINF_video_channels]   = { 114, 417,  74, 20},
  [SINF_video_bitrate]    = { 203, 417,  74, 20},
  [SINF_END]              = { 292, 417,  74, 20},
  [SINF_video_ratio]      = { 381, 417,  74, 20},
  [SINF_video_streams]    = { 470, 417,  74, 20},
  [SINF_audio_channels]   = { 132, 529,  92, 20},
  [SINF_audio_bitrate]    = { 239, 529,  92, 20},
  [SINF_audio_bits]       = { 345, 529,  92, 20},
  [SINF_audio_samplerate] = { 452, 529,  92, 20},
  [SINF_video_fourcc]     = {  25, 417,  74, 20},
  [SINF_audio_fourcc]     = {  25, 529,  92, 20}
};

struct xui_sinfo_s {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;
  xitk_widget_t        *close;
  xitk_widget_t        *update;

  xitk_widget_t        *w[SINF_END];
  xitk_widget_t        *video_resolution;

  xitk_recode_t        *xr;

  int                   visible;
  xitk_register_key_t   widget_key;

  const char            *yes, *no, *unavail;
  char                  *temp, buf[32];
};

static const char *sinf_get_string (xui_sinfo_t *sinfo, sinf_index_t type) {
  const char *s = xine_get_meta_info (sinfo->gui->stream, sinf_xine_type[type]);
  if (!s)
    return sinfo->unavail;
  free (sinfo->temp);
  sinfo->temp = xitk_recode (sinfo->xr, s);
  return sinfo->temp;
}

static const char *sinf_get_int (xui_sinfo_t *sinfo, sinf_index_t type) {
  int v = xine_get_stream_info (sinfo->gui->stream, sinf_xine_type[type]);
  unsigned int u;
  char *q = sinfo->buf + sizeof (sinfo->buf);

  u = v < 0 ? -v : v;
  *--q = 0;
  do {
    *--q = u % 10u + '0';
    u /= 10u;
  } while (u);
  if (v < 0)
    *--q = '-';

  return q;
}

static const char *sinf_get_res (xui_sinfo_t *sinfo) {
  int v;
  unsigned int u;
  char *q = sinfo->buf + sizeof (sinfo->buf);

  v = xine_get_stream_info (sinfo->gui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
  u = v < 0 ? -v : v;
  *--q = 0;
  do {
    *--q = u % 10u + '0';
    u /= 10u;
  } while (u);
  if (v < 0)
    *--q = '-';

  *--q = ' ';
  *--q = 'X';
  *--q = ' ';

  v = xine_get_stream_info (sinfo->gui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
  u = v < 0 ? -v : v;
  do {
    *--q = u % 10u + '0';
    u /= 10u;
  } while (u);
  if (v < 0)
    *--q = '-';

  return q;
}

static const char *sinf_get_bool (xui_sinfo_t *sinfo, sinf_index_t type) {
  int v = xine_get_stream_info (sinfo->gui->stream, sinf_xine_type[type]);

  return v ? sinfo->yes : sinfo->no;
}

const char *get_fourcc_string (char *dst, size_t dst_size, uint32_t f) {
  static const char tab_hex[16] = "0123456789abcdef";
  union {
    uint32_t u;
    char z[4];
  } v;
  char *q;
  int i;

  if (!dst || (dst_size < 17))
    return NULL;

  v.u = f;
  q = dst + dst_size;
  *--q = 0;
  if (f < 0x10000) {
    do {
      *--q = tab_hex[f & 15];
      f >>= 4;
    } while (f);
    *--q = 'x';
    *--q = '0';
  } else {
    for (i = 3; i >= 0; i--) {
      if ((v.z[i] >= ' ') && (v.z[i] < 127)) {
        *--q = v.z[i];
      } else {
        *--q = tab_hex[v.z[i] & 15];
        *--q = tab_hex[v.z[i] >> 4];
        *--q = 'x';
        *--q = '\\';
      }
    }
  }
  return q;
}

static const char *sinf_get_4cc (xui_sinfo_t *sinfo, sinf_index_t type) {
  return get_fourcc_string (sinfo->buf, sizeof (sinfo->buf), xine_get_stream_info (sinfo->gui->stream, sinf_xine_type[type]));
}

static void stream_infos_update (xitk_widget_t *w, void *data, int state) {
  xui_sinfo_t *sinfo = (xui_sinfo_t *)data;

  (void)w;
  (void)state;
  stream_infos_update_infos (sinfo);
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

static void stream_infos_exit (xitk_widget_t *w, void *data, int state) {
  xui_sinfo_t *sinfo = data;
  window_info_t wi;

  if (!sinfo)
    return;
  (void)w;
  (void)state;
  sinfo->visible = 0;
    
  if ((xitk_get_window_info (sinfo->widget_key, &wi))) {
    config_update_num ("gui.sinfos_x", wi.x);
    config_update_num ("gui.sinfos_y", wi.y);
    WINDOW_INFO_ZERO (&wi);
  }
    
  xitk_unregister_event_handler (&sinfo->widget_key);

  xitk_window_destroy_window (sinfo->xwin);
  sinfo->xwin = NULL;
  /* xitk_dlist_init (&sinfo->widget_list.list); */

  xitk_recode_done (sinfo->xr);
  sinfo->xr = NULL;
  free (sinfo->temp);
  sinfo->temp = NULL;

  video_window_set_input_focus (sinfo->gui->vwin);
  sinfo->gui->streaminfo = NULL;
  free (sinfo);
}

static void stream_infos_handle_key_event(void *data, const xitk_key_event_t *ke) {
  xui_sinfo_t *sinfo = (xui_sinfo_t *)data;

  if (ke->event == XITK_KEY_PRESS) {
    if (ke->key_pressed == XK_Escape)
      stream_infos_exit (NULL, sinfo, 0);
    else
      gui_handle_key_event (sinfo->gui, ke);
  }
}

static const xitk_event_cbs_t stream_infos_event_cbs = {
  .key_cb            = stream_infos_handle_key_event,
};

int stream_infos_is_visible (xui_sinfo_t *sinfo) {
  if (sinfo) {
    if (sinfo->gui->use_root_window)
      return xitk_window_is_window_visible (sinfo->xwin);
    else
      return sinfo->visible && xitk_window_is_window_visible (sinfo->xwin);
  }
  return 0;
}

void stream_infos_toggle_auto_update (xui_sinfo_t *sinfo) {
  if (sinfo) {
    if (sinfo->gui->stream_info_auto_update) {
      xitk_hide_widget (sinfo->update);
      xitk_window_clear_window (sinfo->xwin);
      xitk_paint_widget_list (sinfo->widget_list);
    } else {
      xitk_show_widget (sinfo->update);
    }
  }
}

void stream_infos_raise_window (xui_sinfo_t *sinfo) {
  if (sinfo)
    raise_window (sinfo->gui, sinfo->xwin, sinfo->visible, 1);
}

void stream_infos_toggle_visibility(xitk_widget_t *w, void *data) {
  xui_sinfo_t *sinfo = (xui_sinfo_t *)data;
  (void)w;
  if (sinfo)
    toggle_window (sinfo->gui, sinfo->xwin, sinfo->widget_list, &sinfo->visible, 1);
}

void stream_infos_end (xui_sinfo_t *sinfo) {
  stream_infos_exit (NULL, sinfo, 0);
}

void stream_infos_update_infos (xui_sinfo_t *sinfo) {
  if (!sinfo)
    return;
  if (!sinfo->gui->logo_mode) {
    sinf_index_t i;

    for (i = SINF_STRING; i < SINF_BOOL; i++)
      xitk_label_change_label (sinfo->w[i], sinf_get_string (sinfo, i));
    for (; i < SINF_INT; i++)
      xitk_label_change_label (sinfo->w[i], sinf_get_bool (sinfo, i));
    for (; i < SINF_FOURCC; i++)
      xitk_label_change_label (sinfo->w[i], sinf_get_int (sinfo, i));
    for (; i < SINF_END; i++)
      xitk_label_change_label (sinfo->w[i], sinf_get_4cc (sinfo, i));
    xitk_label_change_label (sinfo->video_resolution, sinf_get_res (sinfo));
  } else {
    sinf_index_t i;

    for (i = SINF_STRING; i < SINF_BOOL; i++)
      xitk_label_change_label (sinfo->w[i], sinfo->unavail);
    for (; i < SINF_END; i++)
      xitk_label_change_label (sinfo->w[i], "---");
    xitk_label_change_label (sinfo->video_resolution, "--- X ---");
  }
}

void stream_infos_reparent (xui_sinfo_t *sinfo) {
  if (sinfo)
    reparent_window (sinfo->gui, sinfo->xwin);
}

void stream_infos_panel (gGui_t *gui) {
  int         x, y;
  xui_sinfo_t *sinfo;

  if (!gui)
    return;
  if (gui->streaminfo)
    return;

  sinfo = (xui_sinfo_t *)xitk_xmalloc (sizeof (*sinfo));
  if (!sinfo)
    return;
  sinfo->gui = gui;

  x = xine_config_register_num (sinfo->gui->xine, "gui.sinfos_x", 80,
    CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
  y = xine_config_register_num (sinfo->gui->xine, "gui.sinfos_y", 80,
    CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
  
  /* Create window */
  sinfo->xwin = xitk_window_create_dialog_window (sinfo->gui->xitk, _("Stream Information"),
    x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!sinfo->xwin) {
    free (sinfo);
    return;
  }
  sinfo->gui->streaminfo = sinfo;
  set_window_states_start (sinfo->gui, sinfo->xwin);
  sinfo->widget_list = xitk_window_widget_list (sinfo->xwin);

  sinfo->xr = xitk_recode_init (METAINFO_CHARSET, NULL, 0);
  sinfo->temp = NULL;
  sinfo->yes = _("Yes");
  sinfo->no = _("No");
  sinfo->unavail = _("Unavailable");
  
  {
    xitk_pixmap_t *bg = xitk_window_get_background_pixmap (sinfo->xwin);
    int i;
    for (i = 0; sinf_f_defs[i].y; i++)
      draw_outter_frame (bg, gettext (sinf_f_defs[i].title), btnfontname,
        sinf_f_defs[i].x, sinf_f_defs[i].y, sinf_f_defs[i].w, sinf_f_defs[i].h);
    for (i += 1; sinf_f_defs[i].y; i++)
      draw_inner_frame (bg, gettext (sinf_f_defs[i].title), lfontname,
        sinf_f_defs[i].x, sinf_f_defs[i].y, sinf_f_defs[i].w, sinf_f_defs[i].h);
    xitk_window_set_background (sinfo->xwin, bg);
  }

  {
    sinf_index_t i;
    xitk_label_widget_t lbl;

    XITK_WIDGET_INIT (&lbl);
    lbl.skin_element_name = NULL;
    lbl.callback          = NULL;
    lbl.userdata          = NULL;

    for (i = SINF_STRING; i < SINF_BOOL; i++) {
      lbl.label = sinf_get_string (sinfo, i);
      sinfo->w[i] = xitk_noskin_label_create (sinfo->widget_list, &lbl,
        sinf_w_defs[i].x, sinf_w_defs[i].y, sinf_w_defs[i].w, sinf_w_defs[i].h, fontname);
      xitk_add_widget (sinfo->widget_list, sinfo->w[i]);
      xitk_enable_and_show_widget (sinfo->w[i]);
    }

    for (; i < SINF_INT; i++) {
      lbl.label = sinf_get_bool (sinfo, i);
      sinfo->w[i] = xitk_noskin_label_create (sinfo->widget_list, &lbl,
        sinf_w_defs[i].x, sinf_w_defs[i].y, sinf_w_defs[i].w, sinf_w_defs[i].h, fontname);
      xitk_add_widget (sinfo->widget_list, sinfo->w[i]);
      xitk_enable_and_show_widget (sinfo->w[i]);
    }

    for (; i < SINF_FOURCC; i++) {
      lbl.label = sinf_get_int (sinfo, i);
      sinfo->w[i] = xitk_noskin_label_create (sinfo->widget_list, &lbl,
        sinf_w_defs[i].x, sinf_w_defs[i].y, sinf_w_defs[i].w, sinf_w_defs[i].h, fontname);
      xitk_add_widget (sinfo->widget_list, sinfo->w[i]);
      xitk_enable_and_show_widget (sinfo->w[i]);
    }

    for (; i < SINF_END; i++) {
      lbl.label = sinf_get_4cc (sinfo, i);
      sinfo->w[i] = xitk_noskin_label_create (sinfo->widget_list, &lbl,
        sinf_w_defs[i].x, sinf_w_defs[i].y, sinf_w_defs[i].w, sinf_w_defs[i].h, fontname);
      xitk_add_widget (sinfo->widget_list, sinfo->w[i]);
      xitk_enable_and_show_widget (sinfo->w[i]);
    }

    lbl.label = sinf_get_res (sinfo);
    sinfo->video_resolution = xitk_noskin_label_create (sinfo->widget_list, &lbl,
      sinf_w_defs[i].x, sinf_w_defs[i].y, sinf_w_defs[i].w, sinf_w_defs[i].h, fontname);
    xitk_add_widget (sinfo->widget_list, sinfo->video_resolution);
    xitk_enable_and_show_widget (sinfo->video_resolution);
  }

  free (sinfo->temp);
  sinfo->temp = NULL;

  {
    xitk_labelbutton_widget_t lb;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.skin_element_name = NULL;
    lb.state_callback    = NULL;
    lb.userdata          = sinfo;
    y = WINDOW_HEIGHT - (23 + 15);

    x = 15;
    lb.label    = _("Update");
    lb.callback = stream_infos_update;
    sinfo->update = xitk_noskin_labelbutton_create (sinfo->widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (sinfo->widget_list, sinfo->update);
    xitk_enable_and_show_widget (sinfo->update);

    if (gui->stream_info_auto_update)
      xitk_hide_widget (sinfo->update);

    x = WINDOW_WIDTH - (100 + 15);
    lb.label    = _("Close");
    lb.callback = stream_infos_exit;
    sinfo->close = xitk_noskin_labelbutton_create (sinfo->widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (sinfo->widget_list, sinfo->close);
    xitk_enable_and_show_widget (sinfo->close);
  }

  sinfo->widget_key = xitk_window_register_event_handler ("sinfos", sinfo->xwin, &stream_infos_event_cbs, sinfo);
  sinfo->visible = 1;
  stream_infos_raise_window (sinfo);
  xitk_window_try_to_set_input_focus (sinfo->xwin);
}
