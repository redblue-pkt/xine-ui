/*
** Copyright (C) 2002 Daniel Caujolle-Bert <segfault@club-internet.fr>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

extern gGui_t     *gGui;

void osd_init(void) {
  int fonth = 20;

#ifdef OSD
  gGui->osd.info = xine_osd_new(gGui->stream, 0, 0, 300, (fonth * 3) + (5 * 3));
  xine_osd_set_font(gGui->osd.info, "sans", fonth);
  xine_osd_set_text_palette(gGui->osd.info, 
			    XINE_TEXTPALETTE_WHITE_BLACK_TRANSPARENT, XINE_OSD_TEXT1);
#endif
}

void osd_update(int tick) {

#ifdef OSD
  if(gGui->osd.info_visible) {
    gGui->osd.info_visible--;
    if(!gGui->osd.info_visible) {
      xine_osd_hide(gGui->osd.info, 0);
    }
  }
  
#endif

}

void osd_stream_infos(void) {
  uint32_t    vwidth, vheight, asrate;
  const char *vcodec, *acodec;
  char        buffer[256];
  int         x, y;
  int         w1, w2, h;

#ifdef OSD
  vcodec  = xine_get_meta_info(gGui->stream, XINE_META_INFO_VIDEOCODEC);
  acodec  = xine_get_meta_info(gGui->stream, XINE_META_INFO_AUDIOCODEC);
  vwidth  = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
  vheight = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
  asrate  = xine_get_stream_info(gGui->stream, XINE_STREAM_INFO_AUDIO_SAMPLERATE);

  xine_osd_clear(gGui->osd.info);

  y = x = 0;

  sprintf(buffer, "%s: %dX%d", vcodec, vwidth, vheight);
  xine_osd_draw_text(gGui->osd.info, x, y, buffer, XINE_OSD_TEXT1);
  xine_osd_get_text_size(gGui->osd.info, buffer, &w1, &h);
  
  y += (h);

  sprintf(buffer, "%s @ %dHz", acodec, asrate);
  xine_osd_draw_text(gGui->osd.info, x, y, buffer, XINE_OSD_TEXT1);
  xine_osd_get_text_size(gGui->osd.info, buffer, &w2, &h);

  x = (vwidth - ((w1 + w2) >> 1)) - 40;
  xine_osd_set_position(gGui->osd.info, x, y);

  xine_osd_show(gGui->osd.info, 0);
  gGui->osd.info_visible = gGui->osd.timeout * 2;
#endif
}
