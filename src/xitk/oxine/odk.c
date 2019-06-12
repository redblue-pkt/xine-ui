/*
 * Copyright (C) 2002-2003 Stefan Holst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
 *
 * odk abstracts from xine and other software / hardware interfaces
 */

/*
#define LOG
*/
 
#include <stdio.h>
#include <string.h>
#include <xine.h>

#include "common.h"
#include "odk.h"
#include "utils.h"
#include "globals.h"


#define V_WIDTH 800
#define V_HEIGHT 600

/* determined by xine engine: */
#define NUM_COLORS 256

struct  odk_s {

  xine_t     *xine;
  xine_stream_t *stream;
  
  xine_osd_t *osd;
  double vscale, hscale;

  int (*event_handler)(void *data, oxine_event_t *ev);
  void *event_handler_data;

  uint32_t color[NUM_COLORS];
  uint8_t trans[NUM_COLORS];

  int unscaled_osd;
  int palette_fill;

  char *menu_bg;

  int is_freetype;
};


/* generates a gradient on palette out of given values */

static void palette_transition(odk_t *odk, int from_index, int to_index,
                               uint32_t from_color, uint8_t from_trans,
			       uint32_t to_color, uint8_t to_trans) {
  double cr_step, cb_step, y_step;
  uint8_t cr_from, cb_from, y_from;
  uint8_t cr_to, cb_to, y_to;
  
  double trans_step;
  int num;

  num = to_index - from_index;

  trans_step = (double)(to_trans - from_trans) / num;

  cb_from = from_color & 0xff;
  cr_from = (from_color >> 8) & 0xff;
  y_from  = (from_color >> 16) & 0xff;

  cb_to   = to_color & 0xff;
  cr_to   = (to_color >> 8) & 0xff;
  y_to    = (to_color >> 16) & 0xff;

  cb_step = (double)(cb_to - cb_from) / num;
  cr_step = (double)(cr_to - cr_from) / num;
  y_step  = (double)(y_to  - y_from) / num;

  for (num = from_index; num < to_index; num++) {
    uint8_t cb_cur = cb_from + (int8_t)(cb_step * (num-from_index));
    uint8_t cr_cur = cr_from + (int8_t)(cr_step * (num-from_index));
    uint8_t y_cur  =  y_from + (int8_t)( y_step * (num-from_index));
    
    odk->color[num] = cb_cur + (cr_cur<<8) + (y_cur<<16);
    odk->trans[num] = from_trans + trans_step * (num-from_index);
    /* printf("writing: color %x trans %u to %u\n", odk->color[num], odk->trans[num], num); */
  }
  odk->color[to_index] = to_color;
  odk->trans[to_index] = to_trans;
}

int odk_get_color(odk_t *odk, uint32_t color, uint8_t trans) {

  int i;

  for(i=0; i<odk->palette_fill; i++) {
    if ((odk->color[i] == color) && (odk->trans[i] == trans))
      return i;
  }
  if (odk->palette_fill == NUM_COLORS) {
    printf("odk: too many colors: palette is full\n");
    return 0;
  }
  odk->color[odk->palette_fill]=color;
  odk->trans[odk->palette_fill]=trans;

  xine_osd_set_palette(odk->osd, odk->color, odk->trans);

  odk->palette_fill++;

  return odk->palette_fill - 1;
}

int odk_alloc_text_palette(odk_t *odk, uint32_t fg_color, uint8_t fg_trans,
                           uint32_t bg_color, uint8_t bg_trans,
                           uint32_t bo_color, uint8_t bo_trans) {

  odk->color[odk->palette_fill]=bg_color; /* not really used by fonts, set to bg */
  odk->trans[odk->palette_fill]=bg_trans;

  /* background (1) to border (6) transition */
  palette_transition(odk, odk->palette_fill+1, odk->palette_fill+6, 
      bg_color, bg_trans, bo_color, bo_trans);

  /* border (6) to foreground (10) transition */
  palette_transition(odk, odk->palette_fill+6, odk->palette_fill+10, 
      bo_color, bo_trans, fg_color, fg_trans);

  odk->palette_fill += 11;
  
  xine_osd_set_palette(odk->osd, odk->color, odk->trans);

  return odk->palette_fill-11;
}

/*
 * adapt osd to new stream size.
 * This only affects primitives drawn after
 * this call.
 */

static void odk_adapt(odk_t *odk) {

  int width, height;

  if (odk->osd) xine_osd_free(odk->osd);

  if( odk->unscaled_osd )
    video_window_get_output_size (gGui->vwin, &width, &height);
  else
    video_window_get_frame_size (gGui->vwin, &width, &height);

  lprintf("odk_adapt to: %i, %i\n", width, height);
  odk->vscale = height / (double)V_HEIGHT;
  odk->hscale = width / (double)V_WIDTH;

  odk->osd = xine_osd_new(odk->stream, 0, 0, width, height);

  xine_osd_set_palette(odk->osd, odk->color, odk->trans);
}


uint8_t *odk_get_pixmap(int type) {
  int i;
  uint8_t arrowup[] = {0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
		       0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
		       0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		       0, 0, 0, 0, 1, 1, 0, 0, 0, 0};
  uint8_t arrowdown[] = {0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
			 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
			 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
			 0, 0, 1, 1, 1, 1, 1, 1, 0, 0};

  uint8_t *p = NULL;

  p = ho_newstring(10*10 + 10);

  switch(type) {
  case PIXMAP_SIMPLE_ARROW_UP:
    memcpy(p, arrowup, 100);
    break;
  case PIXMAP_SIMPLE_ARROW_DOWN:
    memcpy(p, arrowdown, 100);
    break;
  }

  /* FIXME: check this stuff "XINE_OSD_TEXT1 + 2" */
  for(i=0;i<100;i++) p[i] = p[i] * XINE_OSD_TEXT1 + 2;

  return p;
}


/*
 * initializes drawable, xine stream and osd
 */

odk_t *odk_init(void) {

  odk_t *odk = ho_new_tagged(odk_t, "odk object");

  lprintf("initializing\n");

  odk->palette_fill = 0;
  odk->hscale = 0;
  odk->vscale = 0;

  odk->xine = __xineui_global_xine_instance;
  odk->stream = gGui->stream;

  /* test unscaled osd support */
  odk->osd = xine_osd_new(odk->stream, 0, 0, 10, 10);
  odk->unscaled_osd =
    (xine_osd_get_capabilities(odk->osd) & XINE_OSD_CAP_UNSCALED );
  xine_osd_free(odk->osd);
  odk->osd = NULL;

  odk_adapt(odk);

  return odk;
}


/*
 * primitive drawing functions
 */

void odk_draw_bitmap(odk_t *odk, uint8_t *bitmap, int x1, int y1, int width, int height,
		     uint8_t *palette_map)
{
  /* uint8_t a[] = {0, 0, 0, 255, 255, 255}; */
  if (!(odk->hscale&&odk->vscale)) return;

  /* FIXME: scaling? */
  xine_osd_draw_bitmap(odk->osd, bitmap, x1, y1, width, height, NULL);
}


void odk_draw_line(odk_t *odk, int x1, int y1, int x2, int y2, int color) {

 int px1, py1, px2, py2;

  if (!(odk->hscale&&odk->vscale)) return;

 px1 = (int) ((double)x1*odk->hscale);
 py1 = (int) ((double)y1*odk->vscale);
 px2 = (int) ((double)x2*odk->hscale);
 py2 = (int) ((double)y2*odk->vscale);
 lprintf("drawing line %u %u %u %u col: %u\n", px1, py1, px2, py2, color);

  xine_osd_draw_line(odk->osd,
      px1,
      py1,
      px2,
      py2,
      color);
}


void odk_draw_rect(odk_t *odk, int x1, int y1, int x2, int y2, int filled, int color) {

 int px1, py1, px2, py2;

  if (!(odk->hscale&&odk->vscale)) return;

 px1 = (int) ((double)x1*odk->hscale);
 py1 = (int) ((double)y1*odk->vscale);
 px2 = (int) ((double)x2*odk->hscale);
 py2 = (int) ((double)y2*odk->vscale);
 lprintf("drawing rect %u %u %u %u col: %u\n", px1, py1, px2, py2, color);

  xine_osd_draw_rect(odk->osd,
      px1, py1, px2, py2, color, filled);
}


void odk_draw_text(odk_t *odk, int x, int y, const char *text, int alignment, int color) {

  int px, py, w, h;

  if (!(odk->hscale&&odk->vscale)) return;

  odk_get_text_size(odk, text, &w, &h);

  if (odk->is_freetype) {
    if (alignment & ODK_ALIGN_VCENTER) y += h/2;
    if (alignment & ODK_ALIGN_TOP)  y += h;
  } else {
    if (alignment & ODK_ALIGN_VCENTER) y -= h/2;
    if (alignment & ODK_ALIGN_BOTTOM)  y -= h;
  }

  if (alignment & ODK_ALIGN_CENTER)  x -= w/2;
  if (alignment & ODK_ALIGN_RIGHT)   x -= w;

  px = (int) ((double)x*odk->hscale);
  py = (int) ((double)y*odk->vscale);

  lprintf("drawing text at %u %u (vs: %f hs: %f)\n", px, py, odk->vscale, odk->hscale);

  xine_osd_draw_text(odk->osd, px, py, text, color);
}


/*
 * overall osd control
 */

void odk_show(odk_t *odk) {
  if( odk->unscaled_osd )
    xine_osd_show_unscaled(odk->osd, 0);
  else
    xine_osd_show(odk->osd, 0);
}

void odk_hide(odk_t *odk) {
  xine_osd_hide(odk->osd, 0);
}

void odk_clear(odk_t *odk) {
  xine_osd_clear(odk->osd);
}


/*
 * font stuff
 */

void odk_get_text_size(odk_t *odk, const char *text, int *width, int *height) {

  int w, h;

  if (!(odk->hscale&&odk->vscale)) {
    *width = 0;
    *height = 0;
    return;
  }

  xine_osd_get_text_size(odk->osd, text, &w, &h);
  *width = (int) ((double)w/odk->hscale);
  *height = (int) ((double)h/odk->vscale);
}


void odk_set_font(odk_t *odk, const char *font, int size) {

  int psize;

  psize = (int)((double)size*(odk->hscale+odk->vscale)/2);

  /* smallest text size possible */
  if (psize<16) psize=16;

  lprintf("setting font to %s %u %u\n", font, size, psize);

  if (strchr(font, '.')||strchr(font, '/'))
    odk->is_freetype = 1;
  else
    odk->is_freetype = 0;

  xine_osd_set_font(odk->osd, font, psize);
}


/*
 * event stuff
 */

void odk_set_event_handler(odk_t *odk, int (*cb)(void *data, oxine_event_t *ev), void *data) {

  odk->event_handler = cb;
  odk->event_handler_data = data;
}

int odk_send_event(odk_t *odk, oxine_event_t *event) {

  switch (event->type) {
    case OXINE_EVENT_FORMAT_CHANGED:
      odk_adapt(odk);
      break;
    
    case OXINE_EVENT_BUTTON:
    case OXINE_EVENT_MOTION:
      if( !odk->unscaled_osd ) {
        x11_rectangle_t rect;

        rect.x = event->x;
        rect.y = event->y;
        rect.w = 0;
        rect.h = 0;

        if (xine_port_send_gui_data(gGui->vo_port,
            XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect) == -1) {
          return 0;
        }

        event->x = rect.x;
        event->y = rect.y;
      }
      event->x = (int) ((double)event->x/odk->hscale);
      event->y = (int) ((double)event->y/odk->vscale);
      if ((event->x < 0) || (event->x > V_WIDTH)) return 0;
      if ((event->y < 0) || (event->y > V_HEIGHT)) return 0;
  }
  
  if (odk->event_handler)
    return odk->event_handler(odk->event_handler_data, event);

  return 0;
}

/*
 * destructor
 */

void odk_free(odk_t *odk) {

  lprintf("finalizing\n");

  xine_osd_free(odk->osd);

  ho_free(odk);
}


/*
 * xine control
 */

void odk_enqueue(odk_t *odk, const char *mrl)
{
  if(mrl_look_like_playlist((char *)mrl)) {
    if(!mediamark_concat_mediamarks(mrl))
      mediamark_append_entry(mrl, mrl, NULL, 0, -1, 0, 0);
  }
  else
    mediamark_append_entry(mrl, mrl, NULL, 0, -1, 0, 0);
}

int odk_open_and_play(odk_t *odk, const char *mrl) {
  int entry_num = gGui->playlist.num;

  odk_enqueue(odk, mrl);

  if((xine_get_status(gGui->stream) != XINE_STATUS_STOP)) {
    gGui->ignore_next = 1;
    xine_stop(gGui->stream);
    gGui->ignore_next = 0;
  }

  if( gGui->playlist.num > entry_num ) {
    gGui->playlist.cur = entry_num;
    gui_set_current_mmk(gGui->playlist.mmk[entry_num]);

    return gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0,
           gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 0);
  } else
    return 0;
}

void odk_play(odk_t *odk) {

  gui_xine_play(odk->stream, 0, 0, 1);
}

void odk_stop(odk_t *odk) {

  gui_stop(NULL, NULL);
}

static int get_pos_length(xine_stream_t *stream, int *pos, int *time, int *length) {
  int t = 0, ret = 0;

  if(stream && (xine_get_status(stream) == XINE_STATUS_PLAY)) {
    while(((ret = xine_get_pos_length(stream, pos, time, length)) == 0) && (++t < 10))
      /*printf("wait");*/
      usleep(100000); /* wait before trying again */
  }
  return ret;
}

int odk_get_pos_length_high(odk_t *odk, int *pos, int *time, int *length) {
  int ret = 0;
#if 0
  static int last_time = 0;
#endif
  xine_stream_t *stream = odk->stream;

  if(stream && (xine_get_status(stream) == XINE_STATUS_PLAY)) {
    ret = xine_get_pos_length(stream, pos, time, length);
  }

  /*
   * smoothen the times xine returns a bit. filtering out small backjumps here.
   */

#if 0
  if (ret) {
    if ((*time < last_time) && (*time+1000 > last_time)) {
      *time = last_time;
    } else {
      last_time = *time;
    }
  }
#endif

  return ret;
}

int odk_get_pos_length(odk_t *odk, int *pos, int *time, int *length) {
  return get_pos_length(odk->stream, pos, time, length);
}

/* Return stream position 1..100 */
int odk_get_seek(odk_t *odk) {
  int pos_time, length;
  int pos=1;
  if(!odk_get_pos_length_high(odk, NULL, &pos_time, &length)) {
    return -1;
  }
  if (length) pos = ((pos_time*100) / length);
  /* printf("seek pos : %d\n", pos); */
  return pos;
}


void odk_seek(odk_t *odk, int how) {

  gui_seek_relative(how);
}

void odk_set_speed(odk_t *odk, uint32_t speed) {

  uint32_t s = XINE_SPEED_NORMAL;

  switch(speed) {
    case ODK_SPEED_PAUSE:
      s = XINE_SPEED_PAUSE;
      break;
    case ODK_SPEED_NORMAL:
      s = XINE_SPEED_NORMAL;
      break;
    case ODK_SPEED_SLOW_4:
      s = XINE_SPEED_SLOW_4;
      break;
    case ODK_SPEED_SLOW_2:
      s = XINE_SPEED_SLOW_2;
      break;
    case ODK_SPEED_FAST_4:
      s = XINE_SPEED_FAST_4;
      break;
    case ODK_SPEED_FAST_2:
      s = XINE_SPEED_FAST_2;
      break;
    default:
      printf("odk: odk_set_speed: invalid speed\n");

  }
  xine_set_param(odk->stream, XINE_PARAM_SPEED, s);
}

uint32_t odk_get_speed(odk_t *odk) {

  int s=xine_get_param(odk->stream, XINE_PARAM_SPEED);

  switch(s) {
    case XINE_SPEED_PAUSE:
      return ODK_SPEED_PAUSE;
      break;
    case XINE_SPEED_NORMAL:
      return ODK_SPEED_NORMAL;
      break;
    case XINE_SPEED_SLOW_4:
      return ODK_SPEED_SLOW_4;
      break;
    case XINE_SPEED_SLOW_2:
      return ODK_SPEED_SLOW_2;
      break;
    case XINE_SPEED_FAST_4:
      return ODK_SPEED_FAST_4;
      break;
    case XINE_SPEED_FAST_2:
      return ODK_SPEED_FAST_2;
      break;
    default:
      printf("odk: odk_get_speed: invalid speed\n");
  }
  return -1;
}

#if 0
void odk_toggle_pause(odk_t *odk) {

  gui_pause (NULL, (void*)(1), 0);
}
#endif

void odk_eject(odk_t *odk) {

  xine_eject(odk->stream);
}

const char *odk_get_mrl(odk_t *odk) {
  return mediamark_get_current_mrl();
}

char *odk_get_meta_info(odk_t *odk, int info) {
  const char *str = xine_get_meta_info(odk->stream, info);

  if (!str) return NULL;
  return ho_strdup(str);
}

void odk_user_color(odk_t *odk, const char *name, uint32_t *color, uint8_t *trans) {

  char id[512];
  char value[64];
  const char *v;
  unsigned int c, t;

  snprintf(id, 511, "gui.osdmenu.color_%s", name);
  snprintf(value, 63, "%06x-%01x", *color, *trans);
  v = xine_config_register_string (odk->xine, id, value,
				"color specification yuv-opacity",
				"color spec. format: [YUV color]-[opacity 0-f]",
				10, NULL, NULL);
  if (sscanf(v, "%06x-%01x", &c, &t) != 2) {
    printf("odk: bad formated color spec in entry '%s'\n", id);
    printf("odk: using standard color\n");
    return;
  }
  *color = c;
  *trans = t;
}

/*
 * error handling
 */

int odk_get_error(odk_t *odk) {
  return xine_get_error(odk->stream);
}
