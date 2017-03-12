/*
 * Copyright (C) 2000-2017 the xine project
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

#include "main.h"
#include "osd.h"
#include "actions.h"
#include "globals.h"
#include "utils.h"

#define OVL_PALETTE_SIZE 256

#ifdef	__GNUC__
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{{ .y = (_y), .cr = (_cr), .cb = (_cb)}}
#else
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{{ (_cb), (_cr), (_y) }}
#endif

typedef union {         /* CLUT == Color LookUp Table */
  struct {
    uint8_t cb    : 8;
    uint8_t cr    : 8;
    uint8_t y     : 8;
    uint8_t foo   : 8;
  } u8;
  uint32_t u32;
} __attribute__ ((packed)) clut_t;

static const clut_t textpalettes_color[] = {
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

static const uint8_t textpalettes_trans[] = {
  /* white, no border, translucid */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
  /* yellow, black border, transparent */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
};

static const struct xine_status_s {
  char     symbol[4];
  int      status;
} xine_status[] = {
  { "\xD8", XINE_STATUS_IDLE  }, /* Ø */
  { "}"   , XINE_STATUS_STOP  },
  { ">"   , XINE_STATUS_PLAY  },
  { "{"   , XINE_STATUS_QUIT  }
};

static const struct xine_speeds_s {
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


static __attribute__((noreturn)) void *osd_loop(void *dummy)
{
	pthread_detach(pthread_self());
	while(1) {
	    sleep(1);
	    if(fbxine.osd.sinfo_visible) {
	      fbxine.osd.sinfo_visible--;
	      if(!fbxine.osd.sinfo_visible) {
		xine_osd_hide(fbxine.osd.sinfo, 0);
	      }
	    }

	    if(fbxine.osd.status_visible) {
	        fbxine.osd.status_visible--;
		if(!fbxine.osd.status_visible)
		  xine_osd_hide(fbxine.osd.status, 0);
	    }

	    if(fbxine.osd.info_visible) {
	        fbxine.osd.info_visible--;
		if(!fbxine.osd.info_visible)
		    xine_osd_hide(fbxine.osd.info, 0);
	    }

	    if(fbxine.osd.bar_visible) {
	        fbxine.osd.bar_visible--;
		if(!fbxine.osd.bar_visible) {
		    xine_osd_hide(fbxine.osd.bar[0], 0);
		    xine_osd_hide(fbxine.osd.bar[1], 0);
		}
	    }
	}

	pthread_exit (NULL);
}

static const char *_osd_get_speed_sym(int speed) {
  int i;

  for(i = 0; i < sizeof(xine_speeds)/sizeof(xine_speeds[0]); i++) {
    if(speed == xine_speeds[i].speed)
      return xine_speeds[i].symbol;
  }

  return NULL;
}

static const char *_osd_get_status_sym(int status) {
  int i;

  for(i = 0; i < sizeof(xine_status)/sizeof(xine_status[0]); i++) {
    if(status == xine_status[i].status)
      return xine_status[i].symbol;
  }

  return NULL;
}

void osd_init(void) {
  int fonth = 20;

  fbxine.osd.sinfo = xine_osd_new(fbxine.stream, 0, 0, 900, (fonth * 6) + (5 * 3));
  xine_osd_set_font(fbxine.osd.sinfo, "sans", fonth);
  xine_osd_set_text_palette(fbxine.osd.sinfo, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  fbxine.osd.bar[0] = xine_osd_new(fbxine.stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);
  xine_osd_set_palette(fbxine.osd.bar[0], &textpalettes_color[0].u32, textpalettes_trans);

  fbxine.osd.bar[1] = xine_osd_new(fbxine.stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);
  xine_osd_set_font(fbxine.osd.bar[1], "sans", fonth);
  xine_osd_set_text_palette(fbxine.osd.bar[1], 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);
  
  fbxine.osd.status = xine_osd_new(fbxine.stream, 0, 0, 300, fonth + (fonth >> 1));
  xine_osd_set_font(fbxine.osd.status, "cetus", fonth);
  xine_osd_set_text_palette(fbxine.osd.status, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  fbxine.osd.info = xine_osd_new(fbxine.stream, 0, 0, 2048, fonth + (fonth >> 1));
  xine_osd_set_font(fbxine.osd.info, "sans", fonth);
  xine_osd_set_text_palette(fbxine.osd.info, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);
  
  
  fbxine.osd.timeout = 3;
  pthread_create(&fbxine.osd_thread, 0, osd_loop, 0);
}


void osd_deinit(void) {

  pthread_cancel(fbxine.osd_thread);

  if(fbxine.osd.sinfo_visible) {
    fbxine.osd.sinfo_visible = 0;
    xine_osd_hide(fbxine.osd.sinfo, 0);
  }

  if(fbxine.osd.bar_visible) {
    fbxine.osd.bar_visible = 0;
    xine_osd_hide(fbxine.osd.bar[0], 0);
    xine_osd_hide(fbxine.osd.bar[1], 0);
  } 

  if(fbxine.osd.status_visible) {
    fbxine.osd.status_visible = 0;
    xine_osd_hide(fbxine.osd.status, 0);
  } 

  if(fbxine.osd.info_visible) {
    fbxine.osd.info_visible = 0;
    xine_osd_hide(fbxine.osd.info, 0);
  } 

  xine_osd_free(fbxine.osd.sinfo);
  xine_osd_free(fbxine.osd.bar[0]);
  xine_osd_free(fbxine.osd.bar[1]);
  xine_osd_free(fbxine.osd.status);
  xine_osd_free(fbxine.osd.info);
}


void osd_display_info(const char *info, ...) {

  if(fbxine.osd.enabled) {
    va_list   args;
    char     *buf;

    va_start(args, info);
    buf = xitk_vasprintf(info, args);
    va_end(args);

    if(!buf)
      return;

    xine_osd_clear(fbxine.osd.info);

    xine_osd_draw_text(fbxine.osd.info, 0, 0, buf, XINE_OSD_TEXT1);
    xine_osd_set_position(fbxine.osd.info, 20, 10 + 30);
    if (xine_osd_get_capabilities(fbxine.osd.info) & XINE_OSD_CAP_UNSCALED)
      xine_osd_show_unscaled(fbxine.osd.info, 0);
    else
      xine_osd_show(fbxine.osd.info, 0); 
    fbxine.osd.info_visible = fbxine.osd.timeout;
    SAFE_FREE(buf);
  }
}

void osd_update_status(void) {
  if(fbxine.osd.enabled) {
    int  status;
    char buffer[256];
    
    status = xine_get_status(fbxine.stream);
    
    xine_osd_clear(fbxine.osd.status);
    
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
      strcpy(buffer, (_osd_get_status_sym(status)));
      break;
      
    case XINE_STATUS_PLAY:
      {
	int speed = xine_get_param(fbxine.stream, XINE_PARAM_SPEED);
	int secs;

	if(get_pos_length(fbxine.stream, NULL, &secs, NULL)) {
	  secs /= 1000;
	  
	  sprintf(buffer, "%s %02d:%02d:%02d", (_osd_get_speed_sym(speed)), 
		  secs / (60*60), (secs / 60) % 60, secs % 60);
	}
	else
	  strcpy(buffer, (_osd_get_speed_sym(speed)));
      }
      break;
      
    case XINE_STATUS_QUIT:
      /* noop */
      break;
    }
    
    xine_osd_draw_text(fbxine.osd.status, 0, 0, buffer, XINE_OSD_TEXT1);
    xine_osd_set_position(fbxine.osd.status, 20, 10);
    if (xine_osd_get_capabilities(fbxine.osd.status) & XINE_OSD_CAP_UNSCALED)
      xine_osd_show_unscaled(fbxine.osd.status, 0);
    else
      xine_osd_show(fbxine.osd.status, 0);
    fbxine.osd.status_visible = fbxine.osd.timeout;
  }
}

void osd_stream_infos(void) {

  if(fbxine.osd.enabled) {
    uint32_t    width;
    uint32_t    vwidth, vheight, asrate;
    const char *vcodec, *acodec;
    char        buffer[256];
    int         x, y;
    int         w, h, osdw;
    int         playedtime, totaltime, pos;
    int         audiochannel, spuchannel, len;

    vcodec       = xine_get_meta_info(fbxine.stream, XINE_META_INFO_VIDEOCODEC);
    acodec       = xine_get_meta_info(fbxine.stream, XINE_META_INFO_AUDIOCODEC);
    vwidth       = xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    vheight      = xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    asrate       = xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
    audiochannel = xine_get_param(fbxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    spuchannel   = xine_get_param(fbxine.stream, XINE_PARAM_SPU_CHANNEL);

    if(!get_pos_length(fbxine.stream, &pos, &playedtime, &totaltime))
      return;
    
    playedtime /= 1000;
    totaltime  /= 1000;

    xine_osd_clear(fbxine.osd.sinfo);

    y = x = 0;

    xine_osd_get_text_size(fbxine.osd.sinfo, buffer, &osdw, &h);
    
    if(vcodec && vwidth && vheight) {
      sprintf(buffer, "%s: %dX%d", vcodec, vwidth, vheight);
      xine_osd_draw_text(fbxine.osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(fbxine.osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }

    if(acodec && asrate) {
      sprintf(buffer, "%s: %dHz", acodec, asrate);
      xine_osd_draw_text(fbxine.osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(fbxine.osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }
    
    strlcpy(buffer, "Audio: ", sizeof(buffer));
    len = strlen(buffer);
    switch(audiochannel) {
    case -2:
      strlcat(buffer, "off", sizeof(buffer));
      break;
    case -1:
      if(!xine_get_audio_lang (fbxine.stream, audiochannel, &buffer[len]))
	strlcat(buffer, "auto", sizeof(buffer));
      break;
    default:
      if(!xine_get_audio_lang (fbxine.stream, audiochannel, &buffer[len]))
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
      if(!xine_get_spu_lang (fbxine.stream, spuchannel, &buffer[len]))
	strlcat(buffer, "auto", sizeof(buffer));
      break;
    default:
      if(!xine_get_spu_lang (fbxine.stream, spuchannel, &buffer[len]))
        sprintf(buffer+strlen(buffer), "%3d", spuchannel);
      break;
    }
    strlcat(buffer, ".", sizeof(buffer));
    xine_osd_draw_text(fbxine.osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
    xine_osd_get_text_size(fbxine.osd.sinfo, buffer, &w, &h);
    if(w > osdw)
      osdw = w;
    
    y += (h);

    if(totaltime) {
      sprintf(buffer, "%d:%.2d:%.2d (%.0f%%) of %d:%.2d:%.2d",
	      playedtime / 3600, (playedtime % 3600) / 60, playedtime % 60,
	      ((float)playedtime / (float)totaltime) * 100,
	      totaltime / 3600, (totaltime % 3600) / 60, totaltime % 60);
      xine_osd_draw_text(fbxine.osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(fbxine.osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;

      osd_stream_position();
    }

#ifdef XINE_PARAM_VO_WINDOW_WIDTH
    if (xine_osd_get_capabilities(fbxine.osd.sinfo) & XINE_OSD_CAP_UNSCALED)
      width = xine_get_param(fbxine.stream, XINE_PARAM_VO_WINDOW_WIDTH);
    else
#endif
      width = vwidth;
    
    x = (width - osdw) - 40;
    xine_osd_set_position(fbxine.osd.sinfo, (x >= 0) ? x : 0, 15);
    if (xine_osd_get_capabilities(fbxine.osd.sinfo) & XINE_OSD_CAP_UNSCALED)
      xine_osd_show_unscaled(fbxine.osd.sinfo, 0);
    else
      xine_osd_show(fbxine.osd.sinfo, 0);
    fbxine.osd.sinfo_visible = fbxine.osd.timeout;
  }
}

void osd_draw_bar(const char *title, int min, int max, int val, int type) {

  if(fbxine.osd.enabled) {
    uint32_t width, height;
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
    
    xine_osd_clear(fbxine.osd.bar[0]);
    xine_osd_clear(fbxine.osd.bar[1]);
    
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
      xine_osd_draw_rect(fbxine.osd.bar[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;
      
      for(i = 0; i < 40; i++, x += 8) {
	xine_osd_draw_rect(fbxine.osd.bar[0],
			   x, 6, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
      }
      
      xine_osd_draw_rect(fbxine.osd.bar[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_POS2) {
      x = 3;
      xine_osd_draw_rect(fbxine.osd.bar[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;
      
      for(i = 0; i < 40; i++, x += 8) {
	if(i == (pos - 1))
	  xine_osd_draw_rect(fbxine.osd.bar[0],
			     x, 2, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
	else
	  xine_osd_draw_rect(fbxine.osd.bar[0],
			     x, 6, x + 3, BAR_HEIGHT - 6, bar_color[i], 1);
      }
      
      xine_osd_draw_rect(fbxine.osd.bar[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_STEPPER) {
      int y = BAR_HEIGHT - 4;
      int step = y / 20;
      
      x = 11;
      
      for(i = 0; i < 40; i++, x += 8) {
	xine_osd_draw_rect(fbxine.osd.bar[0],
			   x, y, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
	
	if(!(i % 2))
	  y -= step;
	
      }
    }
    
    if(title) {
      int  tw, th;
      
      xine_osd_get_text_size(fbxine.osd.bar[1], title, &tw, &th);
      xine_osd_draw_text(fbxine.osd.bar[1], (BAR_WIDTH - tw) >> 1, 0, title, XINE_OSD_TEXT1);
    }

#ifdef XINE_PARAM_VO_WINDOW_WIDTH
    if (xine_osd_get_capabilities(fbxine.osd.bar[0]) & XINE_OSD_CAP_UNSCALED) {
      width  = xine_get_param(fbxine.stream, XINE_PARAM_VO_WINDOW_WIDTH);
      height = xine_get_param(fbxine.stream, XINE_PARAM_VO_WINDOW_HEIGHT);
    } else
#endif
    {
      width  = xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_VIDEO_WIDTH);
      height = xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    }
    
    x = (width - BAR_WIDTH) >> 1;
    xine_osd_set_position(fbxine.osd.bar[0], (x >= 0) ? x : 0, (height - BAR_HEIGHT) - 40);
    xine_osd_set_position(fbxine.osd.bar[1], (x >= 0) ? x : 0, (height - (BAR_HEIGHT * 2)) - 40);
    
    if (xine_osd_get_capabilities(fbxine.osd.bar[0]) & XINE_OSD_CAP_UNSCALED) {
      xine_osd_show_unscaled(fbxine.osd.bar[0], 0);
      if (title)
        xine_osd_show_unscaled(fbxine.osd.bar[1], 0);
    }
    else {
      xine_osd_show(fbxine.osd.bar[0], 0);
      if (title)
        xine_osd_show(fbxine.osd.bar[1], 0);
    }
    
    fbxine.osd.bar_visible = fbxine.osd.timeout;
  }
}

void osd_stream_position(void) {
  int pos;
  if(get_pos_length(fbxine.stream, &pos, NULL, NULL)) {
      osd_draw_bar("Position in Stream", 0, 65535, pos, OSD_BAR_POS2);
  }
}

void osd_display_spu_lang(void) {
  char   buffer[32];
  char   lang_buffer[XINE_LANG_MAX];
  int    channel;
  const char *lang = NULL;
  
  channel = xine_get_param(fbxine.stream, XINE_PARAM_SPU_CHANNEL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_spu_lang(fbxine.stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_spu_lang(fbxine.stream, channel, &lang_buffer[0]))
      sprintf(lang_buffer, "%3d", channel);
    lang = lang_buffer;
    break;
  }
  
  sprintf(buffer, "Subtitles: %s", lang);
  osd_display_info("%s", buffer);
}

void osd_display_audio_lang(void) {
  char   buffer[32];
  char   lang_buffer[XINE_LANG_MAX];
  int    channel;
  const char *lang = NULL;

  channel = xine_get_param(fbxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_audio_lang(fbxine.stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_audio_lang(fbxine.stream, channel, &lang_buffer[0]))
      sprintf(lang_buffer, "%3d", channel);
    lang = lang_buffer;
    break;
  }

  sprintf(buffer, "Audio Channel: %s", lang);
  osd_display_info("%s", buffer);
}

void osd_display_zoom(void) {

  /* show some average of x/y zoom (in case aspect ratio was changed) */
  osd_draw_bar("Zoom Setting", 0, 200,
	       (xine_get_param(fbxine.stream, XINE_PARAM_VO_ZOOM_X) +
		xine_get_param(fbxine.stream, XINE_PARAM_VO_ZOOM_Y)) / 2,
	       OSD_BAR_POS2);
}
