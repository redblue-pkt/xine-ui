/*
 * Copyright (C) 2000-2004 the xine project
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "common.h"

extern gGui_t     *gGui;

#define OVL_PALETTE_SIZE 256

#ifdef	__GNUC__
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{y: (_y), cr: (_cr), cb: (_cb)}
#else
#define CLUT_Y_CR_CB_INIT(_y,_cr,_cb)	{ (_cb), (_cr), (_y) }
#endif

typedef struct {         /* CLUT == Color LookUp Table */
  uint8_t cb    : 8;
  uint8_t cr    : 8;
  uint8_t y     : 8;
  uint8_t foo   : 8;
} __attribute__ ((packed)) clut_t;

static clut_t textpalettes_color[] = {
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

static uint8_t textpalettes_trans[] = {
  /* white, no border, translucid */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
  /* yellow, black border, transparent */
  0, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15,
};

static struct xine_status_s {
  char    *symbol;
  int      status;
} xine_status[] = {
  { "�",  XINE_STATUS_IDLE  },
  { "}",  XINE_STATUS_STOP  },
  { ">" , XINE_STATUS_PLAY  },
  { "{" , XINE_STATUS_QUIT  },
  { NULL, 0                 }
};

static struct xine_speeds_s {
  char    *symbol;
  int      speed;
} xine_speeds[] = {
  { "<"  , XINE_SPEED_PAUSE  },
  { "@@>", XINE_SPEED_SLOW_4 },
  { "@>" , XINE_SPEED_SLOW_2 },
  { ">"  , XINE_SPEED_NORMAL },
  { ">$" , XINE_SPEED_FAST_2 },
  { ">$$", XINE_SPEED_FAST_4 },
  { NULL , 0                 }
};

static uint32_t color[OVL_PALETTE_SIZE];
static uint8_t trans[OVL_PALETTE_SIZE];

#define BAR_WIDTH 336
#define BAR_HEIGHT 25

#define MINIMUM_WIN_WIDTH  300
#define FONT_SIZE          20
#define UNSCALED_FONT_SIZE 24

static void  _xine_osd_show(xine_osd_t *osd, int64_t vpts) {
  if( gGui->osd.use_unscaled && gGui->osd.unscaled_available )
    xine_osd_show_unscaled(osd, vpts);
  else
    xine_osd_show(osd, vpts);
}

static void _osd_get_output_size(int *w, int *h) {
  if( gGui->osd.use_unscaled && gGui->osd.unscaled_available )
    video_window_get_output_size(w, h);
  else
    video_window_get_frame_size(w, h);
}

static char *_osd_get_speed_sym(int speed) {
  int i;

  for(i = 0; xine_speeds[i].symbol != NULL; i++) {
    if(speed == xine_speeds[i].speed)
      return xine_speeds[i].symbol;
  }

  return NULL;
}
static char *_osd_get_status_sym(int status) {
  int i;

  for(i = 0; xine_status[i].symbol != NULL; i++) {
    if(status == xine_status[i].status)
      return xine_status[i].symbol;
  }

  return NULL;
}

void osd_init(void) {
  int fonth = FONT_SIZE;

  gGui->osd.sinfo = xine_osd_new(gGui->stream, 0, 0, 900, (fonth * 6) + (5 * 3));
  xine_osd_set_font(gGui->osd.sinfo, "sans", fonth);
  xine_osd_set_text_palette(gGui->osd.sinfo, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  memcpy(color, textpalettes_color, sizeof(textpalettes_color));
  memcpy(trans, textpalettes_trans, sizeof(textpalettes_trans));

  gGui->osd.bar[0] = xine_osd_new(gGui->stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);
  xine_osd_set_palette(gGui->osd.bar[0], color, trans);

  gGui->osd.bar[1] = xine_osd_new(gGui->stream, 0, 0, BAR_WIDTH + 1, BAR_HEIGHT + 1);
  xine_osd_set_font(gGui->osd.bar[1], "sans", fonth);
  xine_osd_set_text_palette(gGui->osd.bar[1], 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);
  
  gGui->osd.status = xine_osd_new(gGui->stream, 0, 0, 300, 2 * fonth);
  xine_osd_set_text_palette(gGui->osd.status, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gGui->osd.info = xine_osd_new(gGui->stream, 0, 0, 2048, fonth + (fonth >> 1));
  xine_osd_set_font(gGui->osd.info, "sans", fonth);
  xine_osd_set_text_palette(gGui->osd.info, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);

  gGui->osd.unscaled_available =
    (xine_osd_get_capabilities(gGui->osd.status) & XINE_OSD_CAP_UNSCALED );
}

void osd_deinit(void) {

  if(gGui->osd.sinfo_visible) {
    gGui->osd.sinfo_visible = 0;
    xine_osd_hide(gGui->osd.sinfo, 0);
  }

  if(gGui->osd.bar_visible) {
    gGui->osd.bar_visible = 0;
    xine_osd_hide(gGui->osd.bar[0], 0);
    xine_osd_hide(gGui->osd.bar[1], 0);
  } 

  if(gGui->osd.status_visible) {
    gGui->osd.status_visible = 0;
    xine_osd_hide(gGui->osd.status, 0);
  } 

  if(gGui->osd.info_visible) {
    gGui->osd.info_visible = 0;
    xine_osd_hide(gGui->osd.info, 0);
  } 

  xine_osd_free(gGui->osd.sinfo);
  xine_osd_free(gGui->osd.bar[0]);
  xine_osd_free(gGui->osd.bar[1]);
  xine_osd_free(gGui->osd.status);
  xine_osd_free(gGui->osd.info);
}

void osd_update(void) {

  if(gGui->osd.sinfo_visible) {
    gGui->osd.sinfo_visible--;
    if(!gGui->osd.sinfo_visible) {
      xine_osd_hide(gGui->osd.sinfo, 0);
    }
  }

  if(gGui->osd.bar_visible) {
    gGui->osd.bar_visible--;
    if(!gGui->osd.bar_visible) {
      xine_osd_hide(gGui->osd.bar[0], 0);
      xine_osd_hide(gGui->osd.bar[1], 0);
    }
  }

  if(gGui->osd.status_visible) {
    gGui->osd.status_visible--;
    if(!gGui->osd.status_visible) {
      xine_osd_hide(gGui->osd.status, 0);
    }
  }

  if(gGui->osd.info_visible) {
    gGui->osd.info_visible--;
    if(!gGui->osd.info_visible) {
      xine_osd_hide(gGui->osd.info, 0);
    }
  }

}

void osd_stream_infos(void) {

  if(gGui->osd.enabled) {
    int         vwidth, vheight, asrate;
    int         wwidth, wheight;
    const char *vcodec, *acodec;
    char        buffer[256], *p;
    int         x, y;
    int         w, h, osdw;
    int         playedtime, totaltime, pos;
    int         audiochannel, spuchannel, len;

    vcodec       = xine_get_meta_info(gGui->stream, XINE_META_INFO_VIDEOCODEC);
    acodec       = xine_get_meta_info(gGui->stream, XINE_META_INFO_AUDIOCODEC);
    vwidth       = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    vheight      = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    asrate       = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
    audiochannel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
    spuchannel   = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);

    if(!gui_xine_get_pos_length(gGui->stream, &pos, &playedtime, &totaltime))
      return;
    
    playedtime /= 1000;
    totaltime  /= 1000;

    xine_osd_clear(gGui->osd.sinfo);

    /* We're in visual animation mode */
    if((vwidth == 0) && (vheight == 0)) {
      if(gGui->visual_anim.running) {
	if(gGui->visual_anim.enabled == 1) {
	  video_window_get_frame_size(&vwidth, &vheight);
	  vcodec = _("post animation");
	}
	else if(gGui->visual_anim.enabled == 2) {
	  vcodec  = xine_get_meta_info(gGui->visual_anim.stream, XINE_META_INFO_VIDEOCODEC);
	  vwidth  = xine_get_stream_info(gGui->visual_anim.stream, XINE_STREAM_INFO_VIDEO_WIDTH);
	  vheight = xine_get_stream_info(gGui->visual_anim.stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
	}
      }
      else {
	video_window_get_frame_size(&vwidth, &vheight);
	vcodec = _("unknown");
      }
    }

    _osd_get_output_size(&wwidth, &wheight);

    y = x = 0;

    sprintf(buffer, "%s", (gGui->is_display_mrl) ? gGui->mmk.mrl : gGui->mmk.ident);
    xine_osd_get_text_size(gGui->osd.sinfo, buffer, &osdw, &h);
    p = buffer;
    while(osdw > (wwidth - 40)) {
      *(p++) = '\0';
      *(p)   = '.';
      *(p+1) = '.';
      *(p+2) = '.';
      xine_osd_get_text_size(gGui->osd.sinfo, p, &osdw, &h);
    }
    xine_osd_draw_text(gGui->osd.sinfo, x, y, p, XINE_OSD_TEXT1);
    
    y += h;
    
    if(vcodec && vwidth && vheight) {
      sprintf(buffer, "%s: %dX%d", vcodec, vwidth, vheight);
      xine_osd_draw_text(gGui->osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(gGui->osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }

    if(acodec && asrate) {
      sprintf(buffer, "%s: %dHz", acodec, asrate);
      xine_osd_draw_text(gGui->osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(gGui->osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;
      y += h;
    }
    
    memset(&buffer, 0, sizeof(buffer));

    sprintf(buffer, _("Audio: "));
    len = strlen(buffer);
    switch(audiochannel) {
    case -2:
      sprintf(buffer, "%soff", buffer);
      break;
    case -1:
      if(!xine_get_audio_lang (gGui->stream, audiochannel, &buffer[len]))
	sprintf(buffer, "%sauto", buffer);
      break;
    default:
      if(!xine_get_audio_lang (gGui->stream, audiochannel, &buffer[len]))
	sprintf(buffer, "%s%3d", buffer, audiochannel);
      break;
    }

    sprintf(buffer, "%s, Spu: ", buffer);
    len = strlen(buffer);
    switch (spuchannel) {
    case -2:
      sprintf(buffer, "%soff", buffer);
      break;
    case -1:
      if(!xine_get_spu_lang (gGui->stream, spuchannel, &buffer[len]))
	sprintf(buffer, "%sauto", buffer);
      break;
    default:
      if(!xine_get_spu_lang (gGui->stream, spuchannel, &buffer[len]))
        sprintf(buffer, "%s%3d", buffer, spuchannel);
      break;
    }
    sprintf(buffer, "%s.", buffer);
    xine_osd_draw_text(gGui->osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
    xine_osd_get_text_size(gGui->osd.sinfo, buffer, &w, &h);
    if(w > osdw)
      osdw = w;
    
    y += (h);

    if(totaltime) {
      sprintf(buffer, _("%d:%.2d:%.2d (%.0f%%) of %d:%.2d:%.2d"),
	      playedtime / 3600, (playedtime % 3600) / 60, playedtime % 60,
	      ((float)playedtime / (float)totaltime) * 100,
	      totaltime / 3600, (totaltime % 3600) / 60, totaltime % 60);
      xine_osd_draw_text(gGui->osd.sinfo, x, y, buffer, XINE_OSD_TEXT1);
      xine_osd_get_text_size(gGui->osd.sinfo, buffer, &w, &h);
      if(w > osdw)
	osdw = w;

      osd_stream_position(pos);
    }
    
    x = (wwidth - osdw) - 40;
    xine_osd_set_position(gGui->osd.sinfo, (x >= 0) ? x : 0, 15);

    _xine_osd_show(gGui->osd.sinfo, 0);
    gGui->osd.sinfo_visible = gGui->osd.timeout;
  }
}

void osd_draw_bar(char *title, int min, int max, int val, int type) {

  if(gGui->osd.enabled) {
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

    xine_osd_clear(gGui->osd.bar[0]);
    xine_osd_clear(gGui->osd.bar[1]);
    
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
      xine_osd_draw_rect(gGui->osd.bar[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;
      
      for(i = 0; i < 40; i++, x += 8) {
	xine_osd_draw_rect(gGui->osd.bar[0],
			   x, 6, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
      }
      
      xine_osd_draw_rect(gGui->osd.bar[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_POS2) {
      x = 3;
      xine_osd_draw_rect(gGui->osd.bar[0], x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
      x += 8;
      
      for(i = 0; i < 40; i++, x += 8) {
	if(i == (pos - 1))
	  xine_osd_draw_rect(gGui->osd.bar[0],
			     x, 2, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
	else
	  xine_osd_draw_rect(gGui->osd.bar[0],
			     x, 6, x + 3, BAR_HEIGHT - 6, bar_color[i], 1);
      }
      
      xine_osd_draw_rect(gGui->osd.bar[0],
			 x, 2, x + 3, BAR_HEIGHT - 2, XINE_OSD_TEXT1 + 9, 1);
    }
    else if(type == OSD_BAR_STEPPER) {
      int y = BAR_HEIGHT - 4;
      int step = y / 20;
      
      x = 11;
      
      for(i = 0; i < 40; i++, x += 8) {
	xine_osd_draw_rect(gGui->osd.bar[0],
			   x, y, x + 3, BAR_HEIGHT - 2, bar_color[i], 1);
	
	if(!(i % 2))
	  y -= step;
	
      }
    }
    
    if(title) {
      int  tw, th;
      
      xine_osd_get_text_size(gGui->osd.bar[1], title, &tw, &th);
      xine_osd_draw_text(gGui->osd.bar[1], (BAR_WIDTH - tw) >> 1, 0, title, XINE_OSD_TEXT1);
    }
    
    x = (wwidth - BAR_WIDTH) >> 1;
    xine_osd_set_position(gGui->osd.bar[0], (x >= 0) ? x : 0, (wheight - BAR_HEIGHT) - 40);
    xine_osd_set_position(gGui->osd.bar[1], (x >= 0) ? x : 0, (wheight - (BAR_HEIGHT * 2)) - 40);
    
    /* don't even bother drawing osd over those small streams.
     * it would look pretty bad.
     */
    if( wwidth > MINIMUM_WIN_WIDTH ) {
      _xine_osd_show(gGui->osd.bar[0], 0);
      if(title)
        _xine_osd_show(gGui->osd.bar[1], 0);
   
      gGui->osd.bar_visible = gGui->osd.timeout;
    }
  }
}

void osd_stream_position(int pos) {
  osd_draw_bar(_("Position in Stream"), 0, 65535, pos, OSD_BAR_POS2);
}

void osd_display_info(char *info, ...) {

  if(gGui->osd.enabled) {
    va_list   args;
    char     *buf;
    int       n, size = 100;
    
    if((buf = xine_xmalloc(size)) == NULL) 
      return;
    
    while(1) {
      
      va_start(args, info);
      n = vsnprintf(buf, size, info, args);
      va_end(args);
      
      if(n > -1 && n < size)
	break;
      
      if(n > -1)
	size = n + 1;
      else
	size *= 2;
      
      if((buf = realloc(buf, size)) == NULL)
	return;
    }

    xine_osd_clear(gGui->osd.info);

    xine_osd_draw_text(gGui->osd.info, 0, 0, buf, XINE_OSD_TEXT1);
    xine_osd_set_position(gGui->osd.info, 20, 10 + 30);
    _xine_osd_show(gGui->osd.info, 0);
    gGui->osd.info_visible = gGui->osd.timeout;
    SAFE_FREE(buf);
  }
}

void osd_update_status(void) {

  if(gGui->osd.enabled) {
    int  status;
    char buffer[256];
    int wwidth, wheight;

    status = xine_get_status(gGui->stream);
    
    xine_osd_clear(gGui->osd.status);
    
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
      sprintf(buffer, "%s", (_osd_get_status_sym(status)));
      break;
      
    case XINE_STATUS_PLAY:
      {
	int speed = xine_get_param(gGui->stream, XINE_PARAM_SPEED);
	int secs;

	if(gui_xine_get_pos_length(gGui->stream, NULL, &secs, NULL)) {
	  secs /= 1000;
	  
	  sprintf(buffer, "%s %02d:%02d:%02d", (_osd_get_speed_sym(speed)), 
		  secs / (60*60), (secs / 60) % 60, secs % 60);
	}
	else
	  sprintf(buffer, "%s", (_osd_get_speed_sym(speed)));
	
      }
      break;
      
    case XINE_STATUS_QUIT:
      /* noop */
      break;
    }

    if( gGui->osd.use_unscaled && gGui->osd.unscaled_available )
      xine_osd_set_font(gGui->osd.status, "cetus", UNSCALED_FONT_SIZE);
    else
      xine_osd_set_font(gGui->osd.status, "cetus", FONT_SIZE);

    /* set latin1 encoding (NULL) for status text with special characters,
     * then switch back to locale encoding ("") 
     */
    xine_osd_set_encoding(gGui->osd.status, NULL);
    xine_osd_draw_text(gGui->osd.status, 0, 0, buffer, XINE_OSD_TEXT1);
    xine_osd_set_encoding(gGui->osd.status, "");
    xine_osd_set_position(gGui->osd.status, 20, 10);

    _osd_get_output_size(&wwidth, &wheight);

    /* don't even bother drawing osd over those small streams.
     * it would look pretty bad.
     */
    if( wwidth > MINIMUM_WIN_WIDTH ) {
      _xine_osd_show(gGui->osd.status, 0);
      gGui->osd.status_visible = gGui->osd.timeout;
    }
  }
}

void osd_display_spu_lang(void) {
  char   buffer[XINE_LANG_MAX+128];
  char   lang_buffer[XINE_LANG_MAX];
  char  *lang = NULL;
  int    channel;
  
  channel = xine_get_param(gGui->stream, XINE_PARAM_SPU_CHANNEL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_spu_lang(gGui->stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_spu_lang(gGui->stream, channel, &lang_buffer[0]))
      sprintf(lang_buffer, "%3d", channel);
    lang = lang_buffer;
    break;
  }
  
  sprintf(buffer, _("Subtitles: %s"), get_language_from_iso639_1(lang));
  osd_display_info(buffer);
}

void osd_display_audio_lang(void) {
  char   buffer[XINE_LANG_MAX+128];
  char   lang_buffer[XINE_LANG_MAX];
  char  *lang = NULL;
  int    channel;

  channel = xine_get_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);

  switch(channel) {
  case -2:
    lang = "off";
    break;
  case -1:
    if(!xine_get_audio_lang(gGui->stream, channel, &lang_buffer[0]))
      lang = "auto";
    else
      lang = lang_buffer;
    break;
  default:
    if(!xine_get_audio_lang(gGui->stream, channel, &lang_buffer[0]))
      sprintf(lang_buffer, "%3d", channel);
    lang = lang_buffer;
    break;
  }

  sprintf(buffer, _("Audio Channel: %s"), get_language_from_iso639_1(lang));
  osd_display_info(buffer);
}
