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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>

#include "lang.h"

#include "common.h"
#include "osd.h"
#include "videowin.h"
#include "actions.h"

#define OVL_PALETTE_SIZE 256

#ifdef	__GNUC__
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{{ .y = (_y), .cr = (_cr), .cb = (_cb)}}
#else
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{{ (_cb), (_cr), (_y) }}
#endif

static const union {         /* CLUT == Color LookUp Table */
  struct {
    uint8_t cb;
    uint8_t cr;
    uint8_t y;
    uint8_t foo;
  } u8;
  uint32_t u32;
} textpalettes_color[OVL_PALETTE_SIZE] = {
  /* white, no border, translucid */
    CLUT_Y_CR_CB_INIT(0x00, 0x00, 0x00), //0
    CLUT_Y_CR_CB_INIT(0x60, 0x80, 0x80), //1
    CLUT_Y_CR_CB_INIT(0x70, 0x80, 0x80), //2
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0x80), //3
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0x80), //4
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0x80), //5
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0x80), //6
    CLUT_Y_CR_CB_INIT(0xa0, 0x80, 0x80), //7
    CLUT_Y_CR_CB_INIT(0xc0, 0x80, 0x80), //8
    CLUT_Y_CR_CB_INIT(0xe0, 0x80, 0x80), //9
    CLUT_Y_CR_CB_INIT(0xff, 0x80, 0x80), //10
  /* yellow, black border, transparent */
    CLUT_Y_CR_CB_INIT(0x00, 0x00, 0x00), //0
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0xe0), //1
    CLUT_Y_CR_CB_INIT(0x80, 0x80, 0xc0), //2
    CLUT_Y_CR_CB_INIT(0x60, 0x80, 0xa0), //3
    CLUT_Y_CR_CB_INIT(0x40, 0x80, 0x80), //4
    CLUT_Y_CR_CB_INIT(0x20, 0x80, 0x80), //5
    CLUT_Y_CR_CB_INIT(0x00, 0x80, 0x80), //6
    CLUT_Y_CR_CB_INIT(0x40, 0x84, 0x60), //7
    CLUT_Y_CR_CB_INIT(0xd0, 0x88, 0x40), //8
    CLUT_Y_CR_CB_INIT(0xe0, 0x8a, 0x00), //9
    CLUT_Y_CR_CB_INIT(0xff, 0x90, 0x00), //10
};

#ifdef DEBUG
/* check textpalettes_color is properly packed */
typedef char _xitk_static_assertion_testpalettes_size[2*(!!(sizeof(textpalettes_color) == sizeof(uint32_t)*OVL_PALETTE_SIZE))-1];
#endif

static const uint8_t textpalettes_trans[OVL_PALETTE_SIZE] = {
  /* white, no border, translucid */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
  /* yellow, black border, transparent */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
};

static const struct {
  char     symbol[4];
  int      status;
} xine_status[] = {
  { "\xD8",  XINE_STATUS_IDLE  }, /* Ø */
  { "}",  XINE_STATUS_STOP  },
  { ">" , XINE_STATUS_PLAY  },
  { "{" , XINE_STATUS_QUIT  }
};

static const struct {
  char     symbol[4];
  int      speed;
} xine_speeds[] = {
  { "<"  , XINE_SPEED_PAUSE  },
  { "@@>", XINE_SPEED_SLOW_4 },
  { "@>" , XINE_SPEED_SLOW_2 },
  { ">"  , XINE_SPEED_NORMAL },
  { ">$" , XINE_SPEED_FAST_2 },
  { ">$$", XINE_SPEED_FAST_4 }
};

#define BAR_WIDTH 336
#define BAR_HEIGHT 25

#define MINIMUM_WIN_WIDTH  300
#define FONT_SIZE          20
#define UNSCALED_FONT_SIZE 24

static pthread_mutex_t osd_mutex;

static void  _xine_osd_show(xine_osd_t *osd, int64_t vpts) {
  gGui_t *gui = gGui;
  if (gui->osd.use_unscaled && gui->osd.unscaled_available)
    xine_osd_show_unscaled(osd, vpts);
  else
    xine_osd_show(osd, vpts);
}

static void _osd_get_output_size(int *w, int *h) {
  gGui_t *gui = gGui;
  if (gui->osd.use_unscaled && gui->osd.unscaled_available)
    video_window_get_output_size (gui->vwin, w, h);
  else
    video_window_get_frame_size (gui->vwin, w, h);
}

static const char *_osd_get_speed_sym(int speed) {
  size_t i;

  for(i = 0; i < sizeof(xine_speeds)/sizeof(xine_speeds[0]); i++) {
    if(speed == xine_speeds[i].speed)
      return xine_speeds[i].symbol;
  }

  return NULL;
}
static const char *_osd_get_status_sym(int status) {
  size_t i;

  for(i = 0; i < sizeof(xine_status)/sizeof(xine_status[0]); i++) {
    if(status == xine_status[i].status)
      return xine_status[i].symbol;
  }

  return NULL;
}

void osd_init(void) {
  gGui_t *gui = gGui;
  int fonth = FONT_SIZE;

  gui->osd.sinfo.osd[0] = xine_osd_new (gui->stream, 0, 0, 900, (fonth * 6) + (5 * 3));
  xine_osd_set_font (gui->osd.sinfo.osd[0], "sans", fonth);
  xine_osd_set_text_palette (gui->osd.sinfo.osd[0],
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gui->osd.bar.osd[0] = xine_osd_new (gui->stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);

  xine_osd_set_palette (gui->osd.bar.osd[0], &textpalettes_color[0].u32, textpalettes_trans);

  gui->osd.bar.osd[1] = xine_osd_new (gui->stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);
  xine_osd_set_font (gui->osd.bar.osd[1], "sans", fonth);
  xine_osd_set_text_palette (gui->osd.bar.osd[1],
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gui->osd.status.osd[0] = xine_osd_new (gui->stream, 0, 0, 300, 2 * fonth);
  xine_osd_set_text_palette (gui->osd.status.osd[0],
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gui->osd.info.osd[0] = xine_osd_new (gui->stream, 0, 0, 2048, fonth + (fonth >> 1));
  xine_osd_set_font (gui->osd.info.osd[0], "sans", fonth);
  xine_osd_set_text_palette (gui->osd.info.osd[0],
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gui->osd.unscaled_available =
    (xine_osd_get_capabilities (gui->osd.status.osd[0]) & XINE_OSD_CAP_UNSCALED );

  pthread_mutex_init(&osd_mutex, NULL);
}

void osd_hide_sinfo(void) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&osd_mutex);
  if (gui->osd.sinfo.visible) {
    gui->osd.sinfo.visible = 0;
    xine_osd_hide (gui->osd.sinfo.osd[0], 0);
  }
  pthread_mutex_unlock(&osd_mutex);
}

void osd_hide_bar(void) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&osd_mutex);
  if (gui->osd.bar.visible) {
    gui->osd.bar.visible = 0;
    xine_osd_hide (gui->osd.bar.osd[0], 0);
    xine_osd_hide (gui->osd.bar.osd[1], 0);
  }
  pthread_mutex_unlock(&osd_mutex);
}

void osd_hide_status(void) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&osd_mutex);
  if (gui->osd.status.visible) {
    gui->osd.status.visible = 0;
    xine_osd_hide (gui->osd.status.osd[0], 0);
  }
  pthread_mutex_unlock(&osd_mutex);
}

void osd_hide_info(void) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&osd_mutex);
  if (gui->osd.info.visible) {
    gui->osd.info.visible = 0;
    xine_osd_hide (gui->osd.info.osd[0], 0);
  }
  pthread_mutex_unlock(&osd_mutex);
}

void osd_hide(void) {

  osd_hide_sinfo();
  osd_hide_bar();
  osd_hide_status();
  osd_hide_info();
}

void osd_deinit(void) {
  gGui_t *gui = gGui;

  osd_hide();

  xine_osd_free (gui->osd.sinfo.osd[0]);
  xine_osd_free (gui->osd.bar.osd[0]);
  xine_osd_free (gui->osd.bar.osd[1]);
  xine_osd_free (gui->osd.status.osd[0]);
  xine_osd_free (gui->osd.info.osd[0]);

  pthread_mutex_destroy(&osd_mutex);
}

void osd_update(void) {
  gGui_t *gui = gGui;

  pthread_mutex_lock(&osd_mutex);

  if (gui->osd.sinfo.visible) {
    gui->osd.sinfo.visible--;
    if (!gui->osd.sinfo.visible) {
      xine_osd_hide (gui->osd.sinfo.osd[0], 0);
    }
  }

  if (gui->osd.bar.visible) {
    gui->osd.bar.visible--;
    if (!gui->osd.bar.visible) {
      xine_osd_hide (gui->osd.bar.osd[0], 0);
      xine_osd_hide (gui->osd.bar.osd[1], 0);
    }
  }

  if (gui->osd.status.visible) {
    gui->osd.status.visible--;
    if (!gui->osd.status.visible) {
      xine_osd_hide (gui->osd.status.osd[0], 0);
    }
  }

  if (gui->osd.info.visible) {
    gui->osd.info.visible--;
    if (!gui->osd.info.visible) {
      xine_osd_hide (gui->osd.info.osd[0], 0);
    }
  }

  pthread_mutex_unlock(&osd_mutex);
}

void osd_stream_infos(void) {
  gGui_t *gui = gGui;

  if (gui->osd.enabled) {
    int         vwidth, vheight, asrate;
    int         wwidth, wheight;
    const char *vcodec, *acodec;
    char        buffer[256], *p;
    int         x, y;
    int         w, h, osdw;
    int         playedtime, playeddays, totaltime, pos;
    int         audiochannel, spuchannel, len;

    vcodec       = xine_get_meta_info (gui->stream, XINE_META_INFO_VIDEOCODEC);
    acodec       = xine_get_meta_info (gui->stream, XINE_META_INFO_AUDIOCODEC);
    vwidth       = xine_get_stream_info (gui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    vheight      = xine_get_stream_info (gui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    asrate       = xine_get_stream_info (gui->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
    audiochannel = xine_get_param (gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    spuchannel   = xine_get_param (gui->stream, XINE_PARAM_SPU_CHANNEL);

    if (!gui_xine_get_pos_length (gui, gui->stream, &pos, &playedtime, &totaltime))
      return;

    playedtime /= 1000;
    totaltime  /= 1000;

    xine_osd_clear (gui->osd.sinfo.osd[0]);

    /* We're in visual animation mode */
    if((vwidth == 0) && (vheight == 0)) {
      if (gui->visual_anim.running) {
        if (gui->visual_anim.enabled == 1) {
          video_window_get_frame_size (gui->vwin, &vwidth, &vheight);
	  vcodec = _("post animation");
	}
        else if (gui->visual_anim.enabled == 2) {
          vcodec  = xine_get_meta_info (gui->visual_anim.stream, XINE_META_INFO_VIDEOCODEC);
          vwidth  = xine_get_stream_info (gui->visual_anim.stream, XINE_STREAM_INFO_VIDEO_WIDTH);
          vheight = xine_get_stream_info (gui->visual_anim.stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
	}
      }
      else {
        video_window_get_frame_size (gui->vwin, &vwidth, &vheight);
	vcodec = _("unknown");
      }
    }

    _osd_get_output_size(&wwidth, &wheight);

    y = x = 0;

    pthread_mutex_lock (&gui->mmk_mutex);
    strlcpy (buffer, (gui->is_display_mrl) ? gui->mmk.mrl : gui->mmk.ident, sizeof (buffer));
    pthread_mutex_unlock (&gui->mmk_mutex);
    xine_osd_get_text_size (gui->osd.sinfo.osd[0], buffer, &osdw, &h);
    p = buffer;
    while(osdw > (wwidth - 40)) {
      *(p++) = '\0';
      *(p)   = '.';
      *(p+1) = '.';
      *(p+2) = '.';
      xine_osd_get_text_size (gui->osd.sinfo.osd[0], p, &osdw, &h);
    }
    xine_osd_draw_text (gui->osd.sinfo.osd[0], x, y, p, XINE_OSD_TEXT1);

    y += h;

    if(vcodec && vwidth && vheight) {
      snprintf(buffer, sizeof(buffer), "%s: %dX%d", vcodec, vwidth, vheight);
      xine_osd_draw_text (gui->osd.sinfo.osd[0], x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size (gui->osd.sinfo.osd[0], buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }

    if(acodec && asrate) {
      snprintf(buffer, sizeof(buffer), "%s: %d%s", acodec, asrate, "Hz");
      xine_osd_draw_text (gui->osd.sinfo.osd[0], x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size (gui->osd.sinfo.osd[0], buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }

    strlcpy(buffer, _("Audio: "), sizeof(buffer));
    len = strlen(buffer);
    switch(audiochannel) {
    case -2:
      strlcat(buffer, "off", sizeof(buffer));
      break;
    case -1:
      if(!xine_get_audio_lang (gui->stream, audiochannel, &buffer[len]))
	strlcat(buffer, "auto", sizeof(buffer));
      break;
    default:
      if(!xine_get_audio_lang (gui->stream, audiochannel, &buffer[len]))
	snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), "%3d", audiochannel);
      break;
    }

    strlcat(buffer, ", Spu: ", sizeof(buffer));
    len = strlen(buffer);
    switch (spuchannel) {
    case -2:
      strlcat(buffer, "off", sizeof(buffer));
      break;
    case -1:
      if (!xine_get_spu_lang (gui->stream, spuchannel, &buffer[len]))
	strlcat(buffer, "auto", sizeof(buffer));
      break;
    default:
      if(!xine_get_spu_lang (gui->stream, spuchannel, &buffer[len]))
        snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), "%3d", spuchannel);
      break;
    }
    strlcat(buffer, ".", sizeof(buffer));
    xine_osd_draw_text (gui->osd.sinfo.osd[0], x, y, buffer, XINE_OSD_TEXT1);
    xine_osd_get_text_size (gui->osd.sinfo.osd[0], buffer, &w, &h);
    if(w > osdw)
      osdw = w;

    y += (h);

    playeddays = playedtime / (3600 * 24);

    if(playeddays > 0)
      sprintf(buffer, "%d::%02d ", playeddays, playedtime / 3600);
    else
      sprintf(buffer, "%d:%02d:%02d ", playedtime / 3600, (playedtime % 3600) / 60, playedtime % 60);

    if(totaltime > 0) {
      int totaldays;

      totaldays  = totaltime / (3600 * 24);
      sprintf(buffer+strlen(buffer), "(%.0f%%) %s ", ((float)playedtime / (float)totaltime) * 100, _("of"));

      if(totaldays > 0)
	sprintf(buffer+strlen(buffer), "%d::%02d", totaldays, totaltime / 3600);
      else
	sprintf(buffer+strlen(buffer), "%d:%02d:%02d", totaltime / 3600, (totaltime % 3600) / 60, totaltime % 60);
    }

    xine_osd_draw_text (gui->osd.sinfo.osd[0], x, y, buffer, XINE_OSD_TEXT1);
    xine_osd_get_text_size (gui->osd.sinfo.osd[0], buffer, &w, &h);
    if(w > osdw)
      osdw = w;

    osd_stream_position(pos);

    x = (wwidth - osdw) - 40;
    xine_osd_set_position (gui->osd.sinfo.osd[0], (x >= 0) ? x : 0, 15);
    gui->osd.sinfo.x = (x >= 0) ? x : 0;
    gui->osd.sinfo.y = 15;
    gui->osd.sinfo.w = osdw;

    pthread_mutex_lock(&osd_mutex);
    _xine_osd_show (gui->osd.sinfo.osd[0], 0);
    gui->osd.sinfo.visible = gui->osd.timeout;
    pthread_mutex_unlock(&osd_mutex);
  }
}

void osd_draw_bar(const char *title, int min, int max, int val, int type) {
  gGui_t *gui = gGui;

  if (gui->osd.enabled) {
    int      wwidth, wheight;
    int      bar_color[40];
    int      i, x;
    float    _val = (int) val;
    float    _min = (int) min;
    float    _max = (int) max;
    int      pos;

    if(max <= min)
      _max = (int) (min + 1);
    if(min >= max)
      _min = (int) (max - 1);
    if(val > max)
      _val = (int) max;
    if(val < min)
      _val = (int) min;

    pos = (int) (_val + -_min) / ((_max + -_min) / 40);

    _osd_get_output_size(&wwidth, &wheight);

    xine_osd_clear (gui->osd.bar.osd[0]);
    xine_osd_clear (gui->osd.bar.osd[1]);

    memset(&bar_color, (XINE_OSD_TEXT1 + 7), sizeof(int) * 40);

    switch(type) {
    case OSD_BAR_PROGRESS:
    case OSD_BAR_STEPPER:
      if(pos)
	memset(bar_color, (XINE_OSD_TEXT1 + 21), sizeof(int) * pos);
      break;
    case OSD_BAR_POS:
    case OSD_BAR_POS2:
      if(pos)
	bar_color[pos - 1] = (XINE_OSD_TEXT1 + 21);
      break;
    }

    if((type == OSD_BAR_PROGRESS) || (type == OSD_BAR_POS)) {
      x = 3;
      xine_osd_draw_rect (gui->osd.bar.osd[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;

      for(i = 0; i < 40; i++, x += 8) {
        xine_osd_draw_rect (gui->osd.bar.osd[0],
			   x, 6, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
      }

      xine_osd_draw_rect (gui->osd.bar.osd[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_POS2) {
      x = 3;
      xine_osd_draw_rect (gui->osd.bar.osd[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;

      for(i = 0; i < 40; i++, x += 8) {
	if(i == (pos - 1))
          xine_osd_draw_rect (gui->osd.bar.osd[0],
			     x, 2, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
	else
          xine_osd_draw_rect (gui->osd.bar.osd[0],
			     x, 6, x + 3, BAR_HEIGHT - 6, bar_color[i], 1);
      }

      xine_osd_draw_rect (gui->osd.bar.osd[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_STEPPER) {
      int y = BAR_HEIGHT - 4;
      int step = y / 20;

      x = 11;

      for(i = 0; i < 40; i++, x += 8) {
        xine_osd_draw_rect (gui->osd.bar.osd[0],
			   x, y, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);

	if(!(i % 2))
	  y -= step;

      }
    }

    if(title) {
      int  tw, th;

      gui->osd.bar.have_text = 1;

      xine_osd_get_text_size (gui->osd.bar.osd[1], title, &tw, &th);
      xine_osd_draw_text (gui->osd.bar.osd[1], (BAR_WIDTH - tw) >> 1, 0, title, XINE_OSD_TEXT1);
    }
    else
      gui->osd.bar.have_text = 0;

    x = (wwidth - BAR_WIDTH) >> 1;
    xine_osd_set_position (gui->osd.bar.osd[0], (x >= 0) ? x : 0, (wheight - BAR_HEIGHT) - 40);
    xine_osd_set_position (gui->osd.bar.osd[1], (x >= 0) ? x : 0, (wheight - (BAR_HEIGHT * 2)) - 40);

    /* don't even bother drawing osd over those small streams.
     * it would look pretty bad.
     */
    if( wwidth > MINIMUM_WIN_WIDTH ) {
      pthread_mutex_lock(&osd_mutex);
      _xine_osd_show (gui->osd.bar.osd[0], 0);
      if(title)
        _xine_osd_show (gui->osd.bar.osd[1], 0);
      gui->osd.bar.visible = gui->osd.timeout;
      pthread_mutex_unlock(&osd_mutex);
    }
  }
}

void osd_stream_position(int pos) {
  osd_draw_bar(_("Position in Stream"), 0, 65535, pos, OSD_BAR_POS2);
}

void osd_display_info(const char *info, ...) {
  gGui_t *gui = gGui;

  if (gui->osd.enabled && !gui->on_quit) {
    va_list   args;
    char     *buf;

    va_start(args, info);
    buf = xitk_vasprintf(info, args);
    va_end(args);

    if (!buf)
      return;

    xine_osd_clear (gui->osd.info.osd[0]);

    xine_osd_draw_text (gui->osd.info.osd[0], 0, 0, buf, XINE_OSD_TEXT1);
    xine_osd_set_position (gui->osd.info.osd[0], 20, 10 + 30);

    pthread_mutex_lock(&osd_mutex);
    _xine_osd_show (gui->osd.info.osd[0], 0);
    gui->osd.info.visible = gui->osd.timeout;
    pthread_mutex_unlock(&osd_mutex);

    free(buf);
  }
}

void osd_update_status(void) {
  gGui_t *gui = gGui;

  if (gui->osd.enabled) {
    int  status;
    char buffer[256];
    int wwidth, wheight;

    status = xine_get_status (gui->stream);

    xine_osd_clear (gui->osd.status.osd[0]);

    /*
      { : eject
      [ : previous
      | : | (thin)
      @ : | (large)
      ] : next
      } : stop
      $ : > (large)
      > : play
      < : pause
    */

    memset(&buffer, 0, sizeof(buffer));

    switch(status) {
    case XINE_STATUS_IDLE:
    case XINE_STATUS_STOP:
      strlcpy(buffer, _osd_get_status_sym(status), sizeof(buffer));
      break;

    case XINE_STATUS_PLAY:
      {
        int speed = xine_get_param (gui->stream, XINE_PARAM_SPEED);
	int secs;

        if (gui_xine_get_pos_length (gui, gui->stream, NULL, &secs, NULL)) {
	  secs /= 1000;

	  snprintf(buffer, sizeof(buffer), "%s %02d:%02d:%02d", (_osd_get_speed_sym(speed)),
		   secs / (60*60), (secs / 60) % 60, secs % 60);
	}
	else
	  strlcpy(buffer, _osd_get_speed_sym(speed), sizeof(buffer));

      }
      break;

    case XINE_STATUS_QUIT:
      /* noop */
      break;
    }

    if (gui->osd.use_unscaled && gui->osd.unscaled_available)
      xine_osd_set_font (gui->osd.status.osd[0], "cetus", UNSCALED_FONT_SIZE);
    else
      xine_osd_set_font (gui->osd.status.osd[0], "cetus", FONT_SIZE);

    /* set latin1 encoding (NULL) for status text with special characters,
     * then switch back to locale encoding ("")
     */
    xine_osd_set_encoding (gui->osd.status.osd[0], NULL);
    xine_osd_draw_text (gui->osd.status.osd[0], 0, 0, buffer, XINE_OSD_TEXT1);
    xine_osd_set_encoding (gui->osd.status.osd[0], "");
    xine_osd_set_position (gui->osd.status.osd[0], 20, 10);

    _osd_get_output_size(&wwidth, &wheight);

    /* don't even bother drawing osd over those small streams.
     * it would look pretty bad.
     */
    if( wwidth > MINIMUM_WIN_WIDTH ) {
      pthread_mutex_lock(&osd_mutex);
      _xine_osd_show (gui->osd.status.osd[0], 0);
      gui->osd.status.visible = gui->osd.timeout;
      pthread_mutex_unlock(&osd_mutex);
    }
  }
}

void osd_display_spu_lang(void) {
  gGui_t *gui = gGui;
  char   buffer[XINE_LANG_MAX+128];
  char   lang_buffer[XINE_LANG_MAX];
  int    channel;
  const char *lang = NULL;

  channel = xine_get_param (gui->stream, XINE_PARAM_SPU_CHANNEL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_spu_lang (gui->stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_spu_lang (gui->stream, channel, &lang_buffer[0]))
      snprintf(lang_buffer, sizeof(lang_buffer), "%3d", channel);
    lang = lang_buffer;
    break;
  }

  snprintf(buffer, sizeof(buffer), "%s%s", _("Subtitles: "), get_language_from_iso639_1(lang));
  osd_display_info("%s", buffer);
}

void osd_display_audio_lang(void) {
  gGui_t *gui = gGui;
  char   buffer[XINE_LANG_MAX+128];
  char   lang_buffer[XINE_LANG_MAX];
  int    channel;
  const char *lang = NULL;

  channel = xine_get_param (gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_audio_lang (gui->stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_audio_lang (gui->stream, channel, &lang_buffer[0]))
      snprintf(lang_buffer, sizeof(lang_buffer), "%3d", channel);
    lang = lang_buffer;
    break;
  }

  snprintf(buffer, sizeof(buffer), "%s%s", _("Audio Channel: "), get_language_from_iso639_1(lang));
  osd_display_info("%s", buffer);
}

void osd_update_osd(void) {
  gGui_t *gui = gGui;
  int vwidth, vheight, wwidth, wheight;
  int x;

  if (!gui->osd.sinfo.visible && !gui->osd.bar.visible)
    return;

  vwidth  = xine_get_stream_info (gui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
  vheight = xine_get_stream_info (gui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);

  if((vwidth == 0) && (vheight == 0)) {
    if (gui->visual_anim.running) {
      if (gui->visual_anim.enabled == 1)
        video_window_get_frame_size (gui->vwin, &vwidth, &vheight);
      else if (gui->visual_anim.enabled == 2)
	vwidth  = xine_get_stream_info (gui->visual_anim.stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    }
    else
      video_window_get_frame_size (gui->vwin, &vwidth, &vheight);

  }

  _osd_get_output_size(&wwidth, &wheight);

  pthread_mutex_lock(&osd_mutex);

  if (gui->osd.sinfo.visible) {
    xine_osd_hide (gui->osd.sinfo.osd[0], 0);

    x = (wwidth - gui->osd.sinfo.w) - 40;
    xine_osd_set_position (gui->osd.sinfo.osd[0], (x >= 0) ? x : 0,  gui->osd.sinfo.y);
    _xine_osd_show (gui->osd.sinfo.osd[0], 0);
  }

  if (gui->osd.bar.visible) {
    xine_osd_hide (gui->osd.bar.osd[0], 0);
    xine_osd_hide (gui->osd.bar.osd[1], 0);

    x = (wwidth - BAR_WIDTH) >> 1;
    xine_osd_set_position (gui->osd.bar.osd[0], (x >= 0) ? x : 0, (wheight - BAR_HEIGHT) - 40);
    xine_osd_set_position (gui->osd.bar.osd[1], (x >= 0) ? x : 0, (wheight - (BAR_HEIGHT * 2)) - 40);

    /* don't even bother drawing osd over those small streams.
     * it would look pretty bad.
     */
    if(wwidth > MINIMUM_WIN_WIDTH) {
      _xine_osd_show (gui->osd.bar.osd[0], 0);

      if (gui->osd.bar.have_text)
	_xine_osd_show (gui->osd.bar.osd[1], 0);
    }

  }

  pthread_mutex_unlock(&osd_mutex);
}
