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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 *
 * oxine main program
 */

#define LOG

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#include <xine.h>
#include "common.h"
#include "oxine.h"
#include "odk.h"
#include "otk.h"
#include "event.h"
#include "utils.h"

#include "mediamarks.h"

extern gGui_t          *gGui;
static oxine_t         *oxine_instance = NULL;

static void main_menu_cb(void *data);
static void playing_menu_cb(void *data);
static void media_stop_cb(void *data);
static void media_info_cb(void *data);
//static void media_returnto_cb(void *data);
//static void media_freeandreturnto_cb(void *data);
static void media_pause_cb(void *data, int i);

//static void event_delay(void *data);

const char *playpause_strings[] = { "<", ">" };

static void media_pause_cb(void *data, int i) {
  oxine_t *oxine = (oxine_t*) data;
 
  switch(i) {
    case 1:
      odk_set_speed(oxine->odk, ODK_SPEED_NORMAL);
      break;
    case 2:
      odk_set_speed(oxine->odk, ODK_SPEED_PAUSE);
      break;
  }
}

static void format_time(char *buf, int sec) {
    sprintf(buf, "%d:%02d:%02d", sec/3600, (sec%3600)/60, ((sec%3600)%60));
}

static void media_stop_cb(void *data) {
  oxine_t *oxine = (oxine_t*) data;

  odk_stop(oxine->odk);
  /*
  odk_open_and_play(oxine->odk, oxine->menu_bg);
  */
  oxine->main_menu_cb(oxine);
}

static void media_tracks_list_cb(void *data, void *entry) {
  oxine_t *oxine = (oxine_t *) data;
  char *mrl = (char *) entry;

  odk_stop(oxine->odk);
  odk_open_and_play(oxine->odk, mrl);
}

/*
static void media_tracks_quit(void *data) {
  oxine_t *oxine = (oxine_t*) data;

  oxine->need_draw = 0;
  printf("%d\n", oxine->mode);
  oxine->mode = OXINE_MODE_NORMAL;
  printf("%d\n", oxine->mode);
  oxine->pauseplay = NULL;
  otk_clear(oxine->otk); 
}
*/

static void media_tracks_cb(void *data) {
  oxine_t *oxine = (oxine_t *) data;
  int num, i = 0;
  char **str;
  otk_widget_t *list;

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;
  oxine->info_window = NULL;
  otk_clear(oxine->otk);
  oxine->main_window = otk_window_new (oxine->otk, NULL, 50, 50, 550, 500);
  list = otk_list_new(oxine->main_window, 10, 10, 500, 490, media_tracks_list_cb, oxine);

  str = xine_get_autoplay_mrls (oxine->xine, "CD", &num);
  while(i < num) {
    /* printf("%d %s\n", i, str[i]); */
    otk_add_listentry(list, str[i], str[i], -1);
    i++;
  } 
  otk_list_set_pos(list, 0);
  otk_set_focus(list);
  otk_draw_all(oxine->otk);
}

#if 0
static void info_button_time(void *data) {
  oxine_t *oxine = (oxine_t *)data;
  int ret, pos_time, length;
  
  ret = odk_get_pos_length_high(oxine->odk, NULL, &pos_time, &length);  
  /* oxine->time = malloc(sizeof(char)*255); */
  if(ret) {
    pos_time /= 1000;
    length /= 1000;
    sprintf(oxine->time, "(%d:%02d:%02d / %d:%02d:%02d)", pos_time/3600, (pos_time%3600)/60, ((pos_time%3600)%60), 
	    length/3600, (length%3600)/60, ((length%3600)%60));
  }
  else sprintf(oxine->time, "N/A");
}
#endif

static void media_info_close_cb(void *data) {
  oxine_t *oxine = (oxine_t*) data;

  if (!oxine->info_window) return;
  
  otk_destroy_widget(oxine->info_window);
  oxine->info_window = NULL;
  otk_draw_all(oxine->otk);

  if (oxine->lines[0]) ho_free(oxine->lines[0]);
  if (oxine->lines[1]) ho_free(oxine->lines[1]);
  if (oxine->lines[2]) ho_free(oxine->lines[2]);
}

static void media_info_cb(void *data) {
  oxine_t *oxine = (oxine_t*) data;
  int pos_time, length, ret;
  int cline = 0;
  char *buf1, *buf2;
  /* otk_widget_t *b, *layout; */

  oxine->media_info_close_cb = media_info_close_cb;
  oxine->pauseplay = NULL;
  if (oxine->info_window) {
    otk_destroy_widget(oxine->info_window);
    oxine->info_window = NULL;
    otk_draw_all(oxine->otk);
    return;
  }
  /* otk_clear(oxine->otk); */

  buf2 = NULL;
  buf1 = odk_get_mrl(oxine->odk);
  if (buf1) {
    buf2 = strrchr(buf1,'/');
    if (buf2) buf2++; else buf2 = buf1;
  }

  if (oxine->lines[0]) ho_free(oxine->lines[0]);
  if (oxine->lines[1]) ho_free(oxine->lines[1]);
  if (oxine->lines[2]) ho_free(oxine->lines[2]);
  
  oxine->lines[0] = odk_get_meta_info(oxine->odk, XINE_META_INFO_TITLE);
  if(!oxine->lines[0] && buf2) oxine->lines[0] = ho_strdup(buf2);
  if(oxine->lines[0]) cline++;
  buf1 = odk_get_meta_info(oxine->odk, XINE_META_INFO_ARTIST);
  buf2 = odk_get_meta_info(oxine->odk, XINE_META_INFO_ALBUM);
  if (buf1 && buf2) {
    oxine->lines[cline] = ho_newstring(strlen(buf1)+strlen(buf2)+10);
    sprintf(oxine->lines[cline], "%s: %s", buf1, buf2);
    cline++;
  } else if (buf1 && (strlen(buf1)>0)) {
    oxine->lines[cline++] = ho_strdup(buf1);
  } else if (buf2 && (strlen(buf2)>0)) {
    oxine->lines[cline++] = ho_strdup(buf2);
  }
  /*
  oxine->genre = odk_get_meta_info(oxine->odk, XINE_META_INFO_GENRE);
  if(!oxine->genre) oxine->genre = ho_strdup("Genre N/A");
  oxine->year = odk_get_meta_info(oxine->odk, XINE_META_INFO_YEAR);
  if(!oxine->year) oxine->year = ho_strdup("Year N/A");
*/
  ret = odk_get_pos_length_high(oxine->odk, NULL, &pos_time, &length);  

  if(ret && (length > 0)) {
    oxine->lines[cline] = ho_newstring(255);
    pos_time /= 1000;
    length /= 1000;
    format_time(oxine->lines[cline], length);
  }

  oxine->info_window = otk_window_new (oxine->otk, NULL, 5, 5, 790, 240);

  if (oxine->lines[0])
  otk_label_new(oxine->info_window, 5, 40, OTK_ALIGN_LEFT|OTK_ALIGN_VCENTER, oxine->lines[0]);
  
  if (oxine->lines[1])
  otk_label_new(oxine->info_window, 5, 110, OTK_ALIGN_LEFT|OTK_ALIGN_VCENTER, oxine->lines[1]);
  
  if (oxine->lines[2])
  otk_label_new(oxine->info_window, 5, 180, OTK_ALIGN_LEFT|OTK_ALIGN_VCENTER, oxine->lines[2]);
  
  /*
  layout = otk_layout_new(oxine->info_window, 10, 10, 680, 480, 6, 1);

  b = otk_button_grid_new(oxine->title, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 0, 1, 1);
  otk_set_focus(b);
 
  b = otk_button_grid_new(oxine->artist, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 1, 1, 1);

  b = otk_button_grid_new(oxine->genre, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 2, 1, 1);
   
  b = otk_button_grid_new(oxine->album, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 3, 1, 1);
 
  b = otk_button_grid_new(oxine->year, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 4, 1, 1);
  
  b = otk_button_grid_new(oxine->time, media_freeandreturnto_cb, oxine);  
  otk_layout_add_widget(layout, b, 0, 5, 1, 1);
  otk_button_uc_set(b, info_button_time, oxine);
 */
  
  otk_draw_all(oxine->otk);

  schedule_job(5000, media_info_close_cb, oxine);
}

/*
static void media_returnto_cb(void *data) {
  oxine_t *oxine = (oxine_t*) data;

  oxine->need_draw = 0;
  printf("%d\n", oxine->mode);
  oxine->mode = OXINE_MODE_NORMAL;
  printf("%d\n", oxine->mode);
  oxine->pauseplay = NULL;
  otk_clear(oxine->otk); 
}
*/

static void shutdown_cb (void *data) {
  /*
  oxine_t *oxine = (oxine_t*) data;
  */
}

static void dvb_cb (void *data) {
  oxine_t *oxine = (oxine_t*) data;

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;
  otk_clear(oxine->otk);
  oxine->mode = OXINE_MODE_NORMAL;
  odk_open_and_play(oxine->odk, "dvb://");
}

static void tv_cb (void *data) {
  oxine_t *oxine = (oxine_t*) data;

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;
  otk_clear(oxine->otk);
  oxine->mode = OXINE_MODE_NORMAL;
  odk_open_and_play(oxine->odk, "v4l://");
}


static void playing_menu_update(void *data) {
  oxine_t *oxine = (oxine_t *) data;
  int pos_time, length;

  if (oxine->pos_str)
    if (odk_get_pos_length_high(oxine->odk, NULL, &pos_time, &length)) {
      pos_time /= 1000;
      format_time(oxine->pos_str, pos_time);
    }
}

static void playing_menu_cb(void *data) {
   oxine_t *oxine = (oxine_t *) data;
   otk_widget_t *b, *l;
   otk_widget_t *layout;
   int pos_time, length;
 
   oxine->pauseplay = NULL;
   if (oxine->main_window) {
     otk_destroy_widget(oxine->main_window);
     oxine->main_window = NULL;
     otk_draw_all(oxine->otk);
   }
   
   /* otk_clear(oxine->otk); */

   if(oxine->mode == OXINE_MODE_PLAY_MENU) {
     oxine->mode = OXINE_MODE_NORMAL;
     return ;
   }
   oxine->mode = OXINE_MODE_PLAY_MENU;

   oxine->main_window = otk_window_new (oxine->otk, NULL, 50, 400, 700, 150);
   layout = otk_layout_new(oxine->main_window, 10, 10, 680, 130, 2, 6); 
   oxine->pauseplay = otk_selector_grid_new (playpause_strings, 2, media_pause_cb, oxine);   
   otk_layout_add_widget(layout, oxine->pauseplay, 0, 0, 1, 1);
   otk_set_focus(oxine->pauseplay);
   if (odk_get_speed(oxine->odk) == ODK_SPEED_PAUSE) otk_selector_set(oxine->pauseplay, 2);
   
   b = otk_button_grid_new ("}", media_stop_cb, oxine);  
   otk_layout_add_widget(layout, b, 1, 0, 1, 1);
   
/*   b = otk_button_grid_new ("Volume", NULL, NULL);
   otk_layout_add_widget(layout, b, 0, 1, 1, 1);
 
   b = otk_slider_grid_new (odk_get_volume);
   otk_layout_add_widget(layout, b, 1, 1, 1, 1);

   if(!oxine->cd_in_use) {
     b = otk_button_grid_new("Seek", NULL, NULL);
     otk_layout_add_widget(layout, b, 0, 2, 1, 1);
   }else {
     b = otk_button_grid_new ("T", media_tracks_cb, oxine);
     otk_layout_add_widget(layout, b, 0, 2, 1, 1);
   }*/
   
   if(oxine->cd_in_use) {
     b = otk_button_grid_new ("T", media_tracks_cb, oxine);
     otk_layout_add_widget(layout, b, 3, 0, 1, 1);
   }
   
   b = otk_slider_grid_new (odk_get_seek);
   otk_layout_add_widget(layout, b, 2, 1, 4, 1);
   otk_set_update(b,1);

   b = otk_button_grid_new ("i", media_info_cb, oxine);
   otk_layout_add_widget(layout, b, 2, 0, 1, 1);
   
   if (!oxine->pos_str) oxine->pos_str = ho_newstring(64);
   if (odk_get_pos_length_high(oxine->odk, NULL, &pos_time, &length)) {
     pos_time /= 1000;
     format_time(oxine->pos_str, pos_time);
   }
   
   l = otk_label_new (oxine->main_window,  110, 100, OTK_ALIGN_CENTER|OTK_ALIGN_VCENTER, oxine->pos_str);
   otk_set_update(l,1);
   otk_button_uc_set(b, playing_menu_update, oxine);

   /* oxine->need_draw = 1; */

   otk_draw_all(oxine->otk);
}

static void main_menu_cb(void *data) {
  
  oxine_t *oxine = (oxine_t*) data;
  otk_widget_t *b;

  lock_job_mutex();
  if (oxine->info_window) {
    media_info_close_cb(oxine);
  }
  unlock_job_mutex();

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;

  otk_clear(oxine->otk);

  oxine->main_window = otk_window_new (oxine->otk, NULL, 50, 130, 700, 420);

  /*
  b = otk_button_new (oxine->main_window, 50, 45, 290, 60, "Play Disc", disc_cb, oxine);
  otk_set_focus(b);
  */
  
  b = otk_button_new (oxine->main_window, 360, 45, 290, 60, "Mediamarks", mediamarks_cb, oxine);
  otk_set_focus(b);
    
  b = otk_button_new (oxine->main_window, 50, 150, 290, 60, "Analogue TV", tv_cb, oxine);
  otk_set_focus(b);
  
  /*
  otk_button_new (oxine->main_window, 360, 150, 290, 60, "Playlist", playlist_cb, oxine);
  */
  
  otk_button_new (oxine->main_window, 50, 255, 290, 60, "Digital TV", dvb_cb, oxine);
  /*
  otk_button_new (oxine->main_window, 360, 255, 290, 60, "Control", control_cb, oxine);
  */

  otk_button_new (oxine->main_window, 205, 320, 290, 60, "Shutdown", shutdown_cb, oxine);

/*  otk_button_new (oxine->main_window, 50, 180, 290, 60, "File", file_cb, oxine);
  otk_button_new (oxine->main_window, 50, 260, 290, 60, "Streaming", streaming_cb, oxine);*/

  otk_draw_all(oxine->otk);
}

/*
 * initialisation
 */
 
oxine_t *create_oxine(void) {
  
  oxine_t *oxine;
  xine_cfg_entry_t centry;
  
  oxine = ho_new(oxine_t);
  oxine->main_menu_cb = main_menu_cb;

  oxine->xine = gGui->xine;
  
  oxine->cd_mountpoint = 
  xine_config_register_string (oxine->xine,
				"gui.osdmenu.dvd_mountpoint",
				"/dvd",
				"directory a media in dvd device will be mounted",
				"directory a media in dvd device will be mounted",
				10,
				NULL,
				NULL);
 
  xine_config_lookup_entry (oxine->xine, "input.dvd_device", &centry);
  oxine->cd_device = centry.str_value;
  
  start_scheduler();
  
  oxine->odk = odk_init();
    
  oxine->otk = otk_init(oxine->odk);
    
  oxine->mode = OXINE_MODE_NORMAL;
   
  return oxine;
}

void destroy_oxine(oxine_t *oxine) {
  
  if (oxine->otk) otk_free(oxine->otk);
  if (oxine->odk) odk_free(oxine->odk);

  ho_free(oxine);
  
  stop_scheduler();

#ifdef DEBUG
  heapstat();
#endif
}

void oxine_init(void)
{
  oxine_instance = create_oxine();
}

void oxine_exit(void)
{
  destroy_oxine(oxine_instance);
  oxine_instance = NULL;
}

void oxine_menu(void)
{
  oxine_t *oxine = oxine_instance;

  if( !oxine )
    return;

  oxine_adapt();
      
  if( oxine->mode != OXINE_MODE_MAINMENU ) {
    if( oxine->reentry )
      oxine->reentry(oxine->reentry_data);
    else
      main_menu_cb(oxine);
    oxine->mode = OXINE_MODE_MAINMENU;
  } else {
    oxine->mode = OXINE_MODE_NORMAL;
    otk_clear(oxine->otk);
  }
}

int oxine_action_event(int xine_event_type)
{
  oxine_t *oxine = oxine_instance;
  oxine_event_t ev;

  if( !oxine )
    return 0;

  if( oxine->mode == OXINE_MODE_NORMAL )
    return 0;

  ev.type = OXINE_EVENT_KEY;
  
  switch( xine_event_type ) {
  case XINE_EVENT_INPUT_UP:
    ev.key = OXINE_KEY_UP;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_DOWN:
    ev.key = OXINE_KEY_DOWN;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_LEFT:
    ev.key = OXINE_KEY_LEFT;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_RIGHT:
    ev.key = OXINE_KEY_RIGHT;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_SELECT:
    ev.key = OXINE_KEY_SELECT;
    otk_send_event(oxine->otk, &ev);
    return 1;
  }
  
  return 0;
}

int oxine_mouse_event(int xine_event_type, int x, int y) {
  oxine_t *oxine = oxine_instance;
  oxine_event_t ev;

  if( !oxine )
    return 0;

  if( oxine->mode == OXINE_MODE_NORMAL )
    return 0;

  ev.x = x;
  ev.y = y;
    
  switch( xine_event_type ) {
  case XINE_EVENT_INPUT_MOUSE_MOVE:
    ev.type = OXINE_EVENT_MOTION;
    return otk_send_event(oxine->otk, &ev);
  case XINE_EVENT_INPUT_MOUSE_BUTTON:
    ev.type = OXINE_EVENT_BUTTON;
    ev.key = OXINE_BUTTON1;
    return otk_send_event(oxine->otk, &ev);
  }

  return 0;
}

void oxine_adapt(void)
{
  oxine_t *oxine = oxine_instance;
  oxine_event_t ev;

  if( !oxine )
    return;
  
  ev.type = OXINE_EVENT_FORMAT_CHANGED;
  otk_send_event(oxine->otk, &ev);
}
