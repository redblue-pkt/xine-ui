/*
 * Copyright (C) 2002 Stefan Holst
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
 * a simple playlist implementataion
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "oxine.h"
#include "otk.h"
#include "playlist.h"
#include "utils.h"


/*
#define LOG
*/


/*
 * playlist gui
 */

#define PL_ACTION_PLAY 1
#define PL_ACTION_REMOVE 2

const char *const pl_action_strings[] = { "Play", "Remove" };

typedef struct {
  oxine_t *oxine;
  int action;
  int listpos;
  otk_widget_t *list;

} pl_session_t;

static void playlist_reentry_cb (void *data);

/**
 * @TODO see if it can be replaced by basename(), and at least use strrchr.
 */
static char * get_basename(char *path) {
  char *buf;
  int i=strlen(path);

  for (i--; i>=0; i--)
    if (path[i]=='/')
      break;

  if (i<=0)
    buf = strdup(path);
  else
    buf = strdup(&path[i+1]);

#ifdef LOG
  printf("basename(%s)=%s\n", path, buf);
  fflush(NULL);
#endif
  return buf;
}

static void changelist (otk_widget_t *list) {

  char tmp[512];
  int i;
  char *pretty_name;
  char *pretty_name_free = NULL;

  otk_remove_listentries(list);

  for( i = 0; gGui->playlist.mmk && i < gGui->playlist.num; i++) {

    /*
    if (item->played)
      if ((ptr=strstr(item->mrl, "#subtitle:")))
        snprintf(tmp, 511, "[ ] %s#%s", item->title,
			get_basename(ptr));
      else
        snprintf(tmp, 511, "[ ] %s", item->title);
    else
      if ((ptr=strstr(item->mrl, "#subtitle:")))
        snprintf(tmp, 511, "[*] %s#%s", item->title,
			get_basename(ptr));
      else
        snprintf(tmp, 511, "[*] %s", item->title);
    */
    if( gGui->playlist.mmk[i]->ident &&
        strlen(gGui->playlist.mmk[i]->ident) &&
        strcmp(gGui->playlist.mmk[i]->ident,gGui->playlist.mmk[i]->mrl) )
      pretty_name = gGui->playlist.mmk[i]->ident;
    else if( !strchr(gGui->playlist.mmk[i]->mrl, ':') )
      pretty_name_free = pretty_name = get_basename(gGui->playlist.mmk[i]->mrl);
    else
      pretty_name = gGui->playlist.mmk[i]->ident;
    
    snprintf(tmp, 511, "%s %s",  ( i == gGui->playlist.cur ) ? "->" : "  ", pretty_name);
    otk_add_listentry(list, tmp, NULL, -1);
    free(pretty_name_free);
    pretty_name_free = NULL;
  }
  otk_list_set_pos(list,0);
  otk_set_focus(list);
}

static void playlist_play_cb(void *data, void *entry) {

  pl_session_t *session = (pl_session_t*) data;
  oxine_t *oxine = session->oxine;
  int n = otk_list_get_entry_number(session->list) - 1;

  session->listpos = otk_list_get_pos(session->list);

  if (session->action == PL_ACTION_PLAY) {
    gGui->playlist.cur = n;
    gui_set_current_mmk(gGui->playlist.mmk[n]);

    if( gui_xine_open_and_play(gGui->mmk.mrl, gGui->mmk.sub, 0,
        gGui->mmk.start, gGui->mmk.av_offset, gGui->mmk.spu_offset, 0) ) {

      oxine->main_window = NULL;
      oxine->info_window = NULL;
      oxine->mode = OXINE_MODE_NORMAL;
      otk_clear(oxine->otk);

    } /* else play_error(oxine); */
  }
  if (session->action == PL_ACTION_REMOVE) {
    playlist_delete_entry(n);
    playlist_reentry_cb(session);
  }
}

static void playlist_clear_cb (void *data) {

  /* pl_session_t *session = (pl_session_t*) data; */

  playlist_delete_all(NULL, NULL);

  playlist_reentry_cb(data);
}

static void playlist_leave_cb (void *data) {

  pl_session_t *session = (pl_session_t*) data;

  session->oxine->reentry = NULL;
  session->oxine->reentry_data = NULL;
  
  session->oxine->main_menu_cb(session->oxine);
}

static void action_changed (void *data, int pos) {

  pl_session_t *session = (pl_session_t*) data;

  session->action = pos;
}

static void playlist_reentry_cb (void *data) {

  pl_session_t *session = (pl_session_t*) data;
  oxine_t      *oxine = session->oxine;
  otk_widget_t *b, *list_window;

  lock_job_mutex();
  if (oxine->info_window && oxine->media_info_close_cb) {
    oxine->media_info_close_cb(oxine);
  }
  unlock_job_mutex();

  oxine->main_window = NULL;
  otk_clear(oxine->otk);

  oxine->main_window = otk_window_new (oxine->otk, NULL, 20, 130, 215, 420);

  otk_label_new (oxine->main_window, 108, 60, OTK_ALIGN_CENTER|OTK_ALIGN_BOTTOM, "Action:");
  b = otk_selector_new (oxine->main_window, 10, 80, 195, 60, pl_action_strings, 2, action_changed, session);
  otk_set_focus(b);
  otk_selector_set(b, session->action);
  
  /*
  otk_button_new (oxine->main_window, 10, 150, 195, 60, "load", playlist_load_cb, session);
  otk_button_new (oxine->main_window, 10, 210, 195, 60, "Save", playlist_save_cb, session);
  */
  otk_button_new (oxine->main_window, 10, 270, 195, 60, "Clear", playlist_clear_cb, session);
  otk_button_new (oxine->main_window, 10, 340, 195, 60, "Main Menu", playlist_leave_cb, session);

  list_window = otk_window_new (oxine->otk, NULL, 245, 130, 535, 420);

  session->list = otk_list_new(list_window, 10, 15, 523, 390, playlist_play_cb, session);
  changelist(session->list);
  otk_list_set_pos(session->list, session->listpos);
  
  otk_draw_all(oxine->otk);
}


void playlist_cb (void *data) {

  oxine_t      *oxine = (oxine_t*) data;
  pl_session_t *session;

  session = ho_new(pl_session_t);
  session->oxine = oxine;
  session->action = 1;

  oxine->reentry_data = session;
  oxine->reentry = playlist_reentry_cb;

  playlist_reentry_cb(session);
}
