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
#include "oxine_event.h"
#include "xine/xmlparser.h"
#include "utils.h"
#include "globals.h"

#include "mediamarks.h"
#include "playlist.h"

static oxine_t         *oxine_instance = NULL;


typedef struct menuitem_s menuitem_t;

struct menuitem_s {
  char *title;
  void *data;
  int  x, y, w, h;
  void (*func)(void *data);
};


static void main_menu_cb(void *data);
static void media_stop_cb(void *data);
static void media_info_cb(void *data);
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

static void shutdown_cb (void *data) {
  gui_execute_action_id(ACTID_QUIT);
}

static void mrl_cb (void *data) {
  char *parameter = (char *) data;
  oxine_t *oxine = oxine_instance;

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;
  otk_clear(oxine->otk);
  oxine->mode = OXINE_MODE_NORMAL;
  odk_open_and_play(oxine->odk, parameter);
}

static void autoplay_cb (void *data) {
  char *parameter = (char *) data;
  oxine_t *oxine = oxine_instance;
  int    num_mrls, j;
  char **autoplay_mrls = xine_get_autoplay_mrls (__xineui_global_xine_instance,
                         parameter,
                         &num_mrls);

  if(autoplay_mrls) {
    playlist_delete_all(NULL, NULL);

    for (j = 0; j < num_mrls; j++)
      mediamark_append_entry((const char *)autoplay_mrls[j],
                             (const char *)autoplay_mrls[j], NULL, 0, -1, 0, 0);

    oxine->pauseplay = NULL;
    oxine->main_window = NULL;
    otk_clear(oxine->otk);
    oxine->mode = OXINE_MODE_NORMAL;

    gGui->playlist.cur = 0;
    gui_set_current_mmk(mediamark_get_current_mmk());

    gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0,
                           gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 0);
  }
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

#if 0
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
#endif

static char *read_entire_file (const char *mrl, int *file_size) {

  char        *buf;
  struct stat  statb;
  int          fd;

  if (stat (mrl, &statb) < 0) {
    lprintf ("cannot stat '%s'\n", mrl);
    return NULL;
  }

  *file_size = statb.st_size;

  fd = open (mrl, O_RDONLY);
  if (fd<0)
    return NULL;

  buf = ho_newstring((*file_size)+1);

  if (!buf)
    return NULL;

  buf[*file_size]=0;

  *file_size = read (fd, buf, *file_size);

  close (fd);

  return buf;
}

static menuitem_t *menuitem_load(xml_node_t *node) {
  
  menuitem_t *item = ho_new(menuitem_t);

  item->x = atoi(xml_parser_get_property(node, "x"));
  item->y = atoi(xml_parser_get_property(node, "y"));
  item->w = atoi(xml_parser_get_property(node, "width"));
  item->h = atoi(xml_parser_get_property(node, "height"));
  item->func = NULL;
  item->title = NULL;
  item->data = NULL;

  if ( (node = node->child) == NULL )
    return item;

  do {
    if (!strcasecmp (node->name, "title")) {
      item->title = ho_strdup (node->data);
    } else if (!strcasecmp (node->name, "action")) {
      const char *type = xml_parser_get_property(node, "type");

      if(type) {
        if(!strcasecmp(type, "autoplay")) {
          item->data = ho_strdup(xml_parser_get_property (node, "parameter"));
          item->func = autoplay_cb;
        } else if(!strcasecmp(type, "mrl")) {
          item->data = ho_strdup(xml_parser_get_property (node, "parameter"));
          item->func = mrl_cb;
        } else if(!strcasecmp(type, "mediamarks")) {
          item->func = mediamarks_cb;
        } else if(!strcasecmp(type, "playlist")) {
          item->func = playlist_cb;
        } else if(!strcasecmp(type, "shutdown")) {
          item->func = shutdown_cb;
        } else if(!strcasecmp(type, "shell")) {
        }
      }
    }
  } while ( (node = node->next) != NULL );

  return item;
}

static int read_main_menu(oxine_t *oxine, list_t *list, const char *mrl) {

  int size;
  char *file = read_entire_file(mrl, &size);
  xml_node_t *node;

  if (!file) return 0;

  xml_parser_init (file, strlen (file), XML_PARSER_CASE_INSENSITIVE);

  if (xml_parser_build_tree (&node)<0) {
    lprintf("xml parsing of %s failed\n", mrl);
    return 0;
  }

  if (strcasecmp (node->name, "oxinemm")) {
    lprintf ("error, root node must be OXINEMM\n");
    return 0;
  }

  node = node->child;

  if (!node || strcasecmp (node->name, "window")) {
    lprintf ("error, node WINDOW expected (%s found)\n", (!node) ? "(null)" : node->name );
    return 0;
  }

  oxine->win_x = atoi(xml_parser_get_property(node, "x"));
  oxine->win_y = atoi(xml_parser_get_property(node, "y"));
  oxine->win_w = atoi(xml_parser_get_property(node, "width"));
  oxine->win_h = atoi(xml_parser_get_property(node, "height"));

  node = node->child;

  while (node) {

    if (!strcasecmp (node->name, "entry")) {
      menuitem_t *item = menuitem_load(node);
      if( item )
        list_append_content(list, item);
    }

    node=node->next;
  }
  
  xml_parser_free_tree(node);
  ho_free(file);

  return 1;
}

static void main_menu_init(oxine_t *oxine)
{
  char         mmpath[XITK_NAME_MAX];

  oxine->main_menu_items = list_new();
  
  memset(mmpath,0,sizeof(mmpath));
  snprintf(mmpath,sizeof(mmpath),"%s/.xine/oxine/mainmenu", xine_get_homedir());
  if (!read_main_menu(oxine, oxine->main_menu_items, mmpath)) {
    lprintf("trying to load system wide mainmenu\n");
    snprintf(mmpath, sizeof(mmpath),"%s/mainmenu", XINE_OXINEDIR);
    if (read_main_menu(oxine, oxine->main_menu_items, mmpath)) {
      /**/
    } else {
      list_free(oxine->main_menu_items);
      oxine->main_menu_items = NULL;
    }
  } else {
    /**/
  }
}

static void main_menu_free(list_t *list) {

  menuitem_t *item;

  if (!list) return;

  item = list_first_content(list);
  while (item) {
    if (item->title) ho_free(item->title);
    if (item->data) ho_free(item->data);
    list_delete_current(list);
    ho_free(item);
    item = list_first_content(list);
  }

  list_free(list);
}

static void main_menu_cb(void *data) {
  
  oxine_t *oxine = (oxine_t*) data;
  menuitem_t *item;
  otk_widget_t *b;

  lock_job_mutex();
  if (oxine->info_window) {
    media_info_close_cb(oxine);
  }
  unlock_job_mutex();

  oxine->pauseplay = NULL;
  oxine->main_window = NULL;

  otk_clear(oxine->otk);

#if 0
  oxine->main_window = otk_window_new (oxine->otk, NULL, 50, 130, 700, 420);

  /*
  b = otk_button_new (oxine->main_window, 50, 45, 290, 60, "Play Disc", disc_cb, oxine);
  otk_set_focus(b);
  */
  
  b = otk_button_new (oxine->main_window, 360, 45, 290, 60, "Mediamarks", mediamarks_cb, oxine);
  otk_set_focus(b);
    
  /*
  b = otk_button_new (oxine->main_window, 50, 150, 290, 60, "Analogue TV", tv_cb, oxine);
  otk_set_focus(b);
  */
  
  otk_button_new (oxine->main_window, 360, 150, 290, 60, "Playlist", playlist_cb, oxine);

  /*
  otk_button_new (oxine->main_window, 50, 255, 290, 60, "Digital TV", dvb_cb, oxine);

  otk_button_new (oxine->main_window, 360, 255, 290, 60, "Control", control_cb, oxine);
  */

  otk_button_new (oxine->main_window, 205, 320, 290, 60, "Shutdown", shutdown_cb, oxine);

/*  otk_button_new (oxine->main_window, 50, 180, 290, 60, "File", file_cb, oxine);
  otk_button_new (oxine->main_window, 50, 260, 290, 60, "Streaming", streaming_cb, oxine);*/
#endif

  oxine->main_window = otk_window_new (oxine->otk, NULL, oxine->win_x, oxine->win_y,
                                                         oxine->win_w, oxine->win_h);

  item = list_first_content(oxine->main_menu_items);

  while (item) {

    b = otk_button_new (oxine->main_window, item->x, item->y, item->w, item->h,
                        item->title, item->func, (item->data) ? item->data : oxine);
    otk_set_focus(b);

    item = list_next_content(oxine->main_menu_items);
  }


  otk_draw_all(oxine->otk);
}

static void return_cb(void *this) {
  oxine_t *oxine = (oxine_t*) this;

  if (oxine->reentry)
    oxine->reentry(oxine->reentry_data);
  else
    oxine->main_menu_cb(oxine);
  return;
}

static void oxine_error_msg(char *text)
{
  oxine_t *oxine = oxine_instance;
  otk_widget_t *b;
  int l;
  char *text2, *s;

  s = text2 = strdup(text);
  
  otk_clear(oxine->otk);
  oxine->main_window = otk_window_new (oxine->otk, NULL, 100, 150, 600, 300);

  for( l = 0; l < 4 && s && strlen(s); l++ ) {
    char *line = s;
    otk_widget_t *label;

    if( (s = strchr(s,'\n')) ) {
      *s++ = '\0';
    }
    
    label = otk_label_new(oxine->main_window, 300, 30 + l*25,
                          OTK_ALIGN_CENTER|OTK_ALIGN_VCENTER, line);
    otk_label_set_font_size(label, 20);
  }
  
  b = otk_button_new(oxine->main_window, 260, 240, 80, 50, "OK", return_cb, oxine);
  otk_set_focus(b);
  oxine->mode = OXINE_MODE_MAINMENU;
  otk_draw_all(oxine->otk);

  free(text2);
}

/*
 * initialisation
 */
 
static oxine_t *create_oxine(void) {
  
  oxine_t *oxine;
  xine_cfg_entry_t centry;
  
  oxine = ho_new(oxine_t);
    
  oxine->main_menu_cb = main_menu_cb;

  oxine->xine = __xineui_global_xine_instance;
  
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

static void destroy_oxine(oxine_t *oxine) {
  
  if (oxine->otk) otk_free(oxine->otk);
  if (oxine->odk) odk_free(oxine->odk);

  main_menu_free(oxine->main_menu_items);
  
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

  if( !oxine->main_menu_items )
    main_menu_init(oxine);

  if( !oxine->main_menu_items ) {
    printf("oxine: main menu items missing, check ~/.xine/oxine/mainmenu\n");
    return;
  }

  oxine_adapt();
      
  if( oxine->mode != OXINE_MODE_MAINMENU ) {
    video_window_reset_ssaver();
    gGui->nongui_error_msg = oxine_error_msg;

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
    video_window_reset_ssaver();
    ev.key = OXINE_KEY_UP;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_DOWN:
    video_window_reset_ssaver();
    ev.key = OXINE_KEY_DOWN;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_LEFT:
    video_window_reset_ssaver();
    ev.key = OXINE_KEY_LEFT;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_RIGHT:
    video_window_reset_ssaver();
    ev.key = OXINE_KEY_RIGHT;
    otk_send_event(oxine->otk, &ev);
    return 1;
  case XINE_EVENT_INPUT_SELECT:
    video_window_reset_ssaver();
    gGui->nongui_error_msg = oxine_error_msg;
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
    video_window_reset_ssaver();
    ev.type = OXINE_EVENT_MOTION;
    return otk_send_event(oxine->otk, &ev);
  case XINE_EVENT_INPUT_MOUSE_BUTTON:
    video_window_reset_ssaver();
    gGui->nongui_error_msg = oxine_error_msg;
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

