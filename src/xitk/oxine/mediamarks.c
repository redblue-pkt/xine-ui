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
 * parsing mediamarks
 */

#define _GNU_SOURCE   /* this is needed for getline(..) and strndup(..) */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>

#include "common.h"

#include "oxine.h"
#include "mediamarks.h"
#include "playlist.h"
#include "otk.h"
#include "xine/xmlparser.h"
#include "utils.h"

extern gGui_t          *gGui;

#define TYPE_NONE (0)
#define TYPE_DIR  (1)
#define TYPE_REG  (2)
#define TYPE_RDIR (3)
#define TYPE_RREG (4)
#define TYPE_UP   (5)
#define TYPE_MULTIPLE (6)
#define TYPE_M3U  (7)

/* sort methods 0 is default*/
#define PLAYITEM_SORT_FILES (1)
#define PLAYITEM_SORT_DIRFILES (0)

#define MM_ACTION_PLAY (1)
#define MM_ACTION_ADD (2)

const char *mm_action_strings[] = { "Play", "Add to PL" };

typedef struct playitem_s playitem_t;

struct playitem_s {

  int   type;
  char *title;
  char *mrl;
  list_t *sub;
};

typedef struct mm_session_s mm_session_t;

struct mm_session_s {

  oxine_t            *oxine;
  otk_widget_t       *list;
  int                listpos;
  list_t             *backpath;
  int                action;
  char               ilabel[64];
};

/* private functions */
static int file_is_m3u(const char *mrl);


/* body of mediamarks functions */

static void bpush (list_t *list, list_t *item) {

  list_first_content(list);
  list_insert_content(list, item);
}

static list_t *bpop (list_t *list) {

  list_t *item;

  item = list_first_content(list);
  list_delete_current(list);

  return item;
}

/*
 * implements case insensitive lengthlexicographic order
 * 
 * returns 1  if a <= b
 *         0  otherwise.
 */

static int in_cill_order(playitem_t *ita, playitem_t *itb, int type) {

  char *a = ita->title;
  char *b = itb->title;

  char *ap, *bp; /* pointers to a, b */
  char ac, bc;

  int rtypea=0, rtypeb=0;

  ap = a; bp = b;

  /* printf("%s %s\n", a, b); 
  stat(ita->mrl, &filestat1);
  stat(itb->mrl, &filestat2);*/

  if(type == PLAYITEM_SORT_DIRFILES) {
    if((ita->type == TYPE_DIR)||(ita->type == TYPE_RDIR)) rtypea = 1;
    if((itb->type == TYPE_DIR)||(itb->type == TYPE_RDIR)) rtypeb = 1;

    if((rtypea)&&(!rtypeb)) return 1;
    if((!rtypea)&&(rtypeb)) return 0;

    /*if((S_ISDIR(filestat1.st_mode))&&(!S_ISDIR(filestat2.st_mode))) return 1;
      if((!S_ISDIR(filestat1.st_mode))&&(S_ISDIR(filestat2.st_mode))) return 0;*/
  }

  while (1) {
    ac = *ap; bc = *bp;
    if ((ac >= 'A') && (ac <= 'Z')) ac |= 0x20;
    if ((bc >= 'A') && (bc <= 'Z')) bc |= 0x20;

    if (ac < bc) return 1;
    if (ac > bc) return 0;
    if (!ac) return 1;

    ap++; bp++;
  }
  return 1;
}


static void playitem_sort_into(playitem_t *item, list_t *list, int type) {

  playitem_t *i;

  i = list_first_content(list);

  while(i) {
    if (in_cill_order(item, i, type)) {      
      list_insert_content(list, item);
      return;
    }
    i = list_next_content(list);
  }
  list_append_content(list, item);

}

static void playitem_append(playitem_t *item, list_t *list) {

  list_append_content(list, item);
}

static playitem_t *playitem_new(int type, char *title, char *mrl, list_t *sub) {

  playitem_t *item = ho_new(playitem_t);
      
  item->type = type;
  item->sub = sub;
  if (title) item->title = ho_strdup(title);
  if (mrl) item->mrl = ho_strdup(mrl);

  return item;
}

static void free_subtree(list_t *list, int i) {

  playitem_t *item;
 
  if (!list) return;
  
  item = list_first_content(list);
  while (item) {
    if (item->title) ho_free(item->title);
    if (item->mrl) ho_free(item->mrl);
    if(item->type != TYPE_UP) free_subtree(item->sub, 1);    
    list_delete_current(list);
    ho_free(item);
    item = list_first_content(list);
  }
  
  if(i) list_free(list);
}

/*static void playitem_free(playitem_t *item) {
  playitem_t *child;

  if(item->title) ho_free(item->title);
  if(item->mrl) ho_free(item->mrl);

  if(item->sub) {
    child = list_first_content(item->sub);

    while(child) {
      playitem_free(child);
      list_delete_current(item->sub);
      child = list_first_content(item->sub);
    }
    list_free(item->sub);
  }
      
  ho_free(item);
  }*/

static playitem_t *playitem_load (xml_node_t *node) {

  playitem_t *play_item;
  char *type;
  struct stat filestat;

  play_item = ho_new(playitem_t);

  while (node) {

    if (!strcasecmp (node->name, "title")) {
      play_item->title = ho_strdup (node->data);
#ifdef LOG
      printf ("mediamarks: title = %s\n", play_item->title);
#endif
    } else if (!strcasecmp (node->name, "ref")) {
      play_item->mrl = ho_strdup(xml_parser_get_property (node, "href"));
      type = xml_parser_get_property(node, "type");
      if(type) {
	if(!strcasecmp(type, "multiple")) {
	  play_item->type = TYPE_MULTIPLE;
	  play_item->sub = list_new();
	}
      } else {
	filestat.st_mode = 0;
	stat(play_item->mrl, &filestat);
	
	if(S_ISDIR(filestat.st_mode)) {
	  play_item->sub = list_new();
	  play_item->type = TYPE_RDIR;
	} else {
	  play_item->type = TYPE_REG;
	  if(file_is_m3u(play_item->mrl)) play_item->type = TYPE_M3U;
	}
      }
    
#ifdef LOG
      printf ("mediamarks: mrl   = %s\n", play_item->mrl);
#endif
      } else if (!strcasecmp (node->name, "time")) {
    } else 
      printf ("mediamarks: error while loading mediamarks file, unknown node '%s'\n", node->name);

    node = node->next;
  }

  return play_item;
}

static int parse_multiple(oxine_t *oxine, const char *mrl, list_t *list) {
  int i=0, num=0;
  playitem_t *item;
  char **str;

  str = xine_get_autoplay_mrls (oxine->xine, mrl, &num);
  if (num<=0) return 0;
 
  free_subtree(list, 0);

  while(i < num) {
    /* printf("%d %s\n", i, str[i]); */
    item = playitem_new(TYPE_REG, str[i], str[i], list_new());
    playitem_append(item, list);
    i++;
  }
  return 1;
}

static int read_directory(oxine_t *oxine, const char *dir, list_t *list) {
  
  DIR *dirp;
  struct dirent *entp;
  playitem_t *item;
  int ret = 0;

  free_subtree(list, 0);

#if 0
  printf("Processing %s\n", dir);
#endif
  dirp = opendir (dir);
  if (dirp != NULL)
  {
    while ((entp = readdir(dirp))) {

      struct stat filestat;
      char mrl[1024];
      char title[256];
      int type = 0;
      
      if((!strcmp(entp->d_name, "."))||(!strcmp(entp->d_name, "..")))
        continue;

#ifndef SHOW_HIDDEN_FILES
      if(entp->d_name[0] == '.')
        continue;
#endif
      
      snprintf(mrl, 1023, "%s/%s", dir, entp->d_name);
      stat(mrl, &filestat);
      if(S_ISDIR(filestat.st_mode)) {
      type = TYPE_RDIR;
      snprintf(title, 255, "[%s]", entp->d_name);	  
      }else if (S_ISREG(filestat.st_mode)) {
      strncpy(title, entp->d_name, 255);
      type = TYPE_RREG;
      }
      if(file_is_m3u(mrl)) {
      type = TYPE_M3U;
      snprintf(title, 255, "[%s]", entp->d_name);
      }

      item = playitem_new(type, title, mrl, list_new());
#if 0
      printf("mrl   : %s\n", item->mrl);
      printf("title : %s\n", item->title);
      printf("type  : %d\n", item->type);
#endif
      /* printf("ei %d\n", oxine->mm_sort_type); */
      /* xine_config_lookup_entry (oxine->xine, "oxine.sort_type", &entry); */

      playitem_sort_into(item, list, oxine->mm_sort_type);
    }
    closedir (dirp);
    ret = 1;
  }
  else
    printf ("mediamarks: Couldn't open the directory.\n");
  
  return ret;
}

static char *read_entire_file (const char *mrl, int *file_size) {

  char        *buf;
  struct stat  statb;
  int          fd;

  if (stat (mrl, &statb) < 0) {
    printf ("mediamarks: cannot stat '%s'\n", mrl);
    return NULL;
  }

  *file_size = statb.st_size;

  fd = open (mrl, O_RDONLY);
  if (fd<0)
    return NULL;

  /*  buf = malloc (sizeof(char)*((*file_size)+1)); */
  buf = ho_newstring((*file_size)+1);

  if (!buf)
    return NULL;

  buf[*file_size]=0;

  *file_size = read (fd, buf, *file_size);

  close (fd);

  return buf;
}

static int file_is_m3u(const char *mrl) {

#ifdef M3U_AS_SUBDIR

  FILE *file;
  char **line;
  int *n;
  int a;

  file = fopen(mrl, "r");
  if(!file) return 0;

  n = ho_new(size_t);
  line = ho_new(char *);

  *line = NULL;
  *n = 0;
  a = getline(line, n, file);
  if(a<=0) {
    ho_free(n);
    ho_free(line);
    return 0;
  }
  if(!strncmp(*line, "#EXTM3U", 7)) { 
    ho_free(n);
    ho_free(line);
    return 1;
  }

  ho_free(n);
  ho_free(line);

  if( strstr(mrl,".m3u") != NULL || strstr(mrl,".M3U") )
    return 1;
  else
    return 0;

#else

  return 0; /* m3u is a normal file here, let xine-ui handle it as playlist */

#endif
}

static void parse_m3u(const char *mrl, list_t *items) {
  FILE *file;
  char **line;
  size_t *n;
  int a;

  file = fopen(mrl, "r");
  if(!file) return ;

  n = ho_new(size_t);
  line = ho_new(char *);

  *line = NULL;
  *n = 0;
  a = getline(line, n, file);
  if(a<=0) return;

  while((a = getline(line, n, file))>0) {
    char *str;
    playitem_t *item;

    if(*line[0] == '#') continue;
    str = strndup(*line, a-1);
    /* printf("%s\n", str); */
    item = playitem_new (TYPE_REG, basename(str), str, list_new());
    ho_free(str);
    playitem_append(item, items);
  }
  ho_free(line);
  ho_free(n);
  fclose(file);
}

static void read_subs(xml_node_t *node, list_t *items) {

  playitem_t *item = NULL;
  
  while (node) {

    if (!strcasecmp (node->name, "entry")) {
      
      item = playitem_load (node->child);
      
    } else if(!strcasecmp (node->name, "sub")) {
      char title[256];
      
      snprintf(title, 255, "[%s]", xml_parser_get_property (node, "name"));
      item = playitem_new (TYPE_DIR, title, NULL, list_new());
      read_subs(node->child, item->sub);
    } 
    playitem_append(item, items);
    node=node->next;
  }
}

static int read_mediamarks(list_t *list, const char *mrl) {

  int size;
  char *file = read_entire_file(mrl, &size);
  xml_node_t *node;

  if (!file) return 0;

  xml_parser_init (file, strlen (file), XML_PARSER_CASE_INSENSITIVE);

  if (xml_parser_build_tree (&node)<0) {
    printf("mediamarks: xml parsing of %s failed\n", mrl);
    return 0;
  }
  
  if (strcasecmp (node->name, "oxinemm")) {
    printf ("mediamarks: error, root node must be OXINEMM\n");
    return 0;
  }

  read_subs(node->child, list);
  xml_parser_free_tree(node);
  ho_free(file);

  return 1;
}

static void changelist (otk_widget_t *list, list_t *backpath) {

  playitem_t *item, *back;
  list_t *current = list_first_content(backpath);
  list_t *parent = list_next_content(backpath);

  otk_remove_listentries(list);

  item = list_first_content(current);

  if((item->type != TYPE_UP)&&(parent)) {
    back = playitem_new(TYPE_UP, "[..]", NULL, parent);
    list_insert_content(current, back);
    //otk_add_listentry(list, back->title, back, -1);
  } 
  item = list_first_content(current);
  while (item) {
    /*printf("item : %s\n" ,item->title);*/
    otk_add_listentry(list, item->title, item, -1);

    item = list_next_content(current);
  }
  otk_list_set_pos(list,0);
  otk_set_focus(list);
}

static void action_changed (void *data, int pos) {

  mm_session_t *session = (mm_session_t*) data;

  session->action = pos;
}

static void set_ilabel(mm_session_t *session) {
  int n;
  
  n = gGui->playlist.num;
  sprintf(session->ilabel, "Selected: %3d", n);
}

static void mediamarks_play_cb(void *data, void *entry) {

  mm_session_t *session = (mm_session_t*) data;
  oxine_t *oxine = session->oxine;
  playitem_t *item = (playitem_t*) entry;

  session->listpos = otk_list_get_pos(session->list);
#ifdef LOG
  printf("mediamarks: mediamarks_play_cb %s\n", item->mrl);
  fflush(NULL);
#endif

  if (item->type == TYPE_DIR) {
    bpush(session->backpath, item->sub);
    changelist(session->list, session->backpath);
    otk_draw_all(oxine->otk);
  } else if (item->type == TYPE_RDIR) {
    if( read_directory(oxine, item->mrl, item->sub) ) {
      bpush(session->backpath, item->sub);
      changelist(session->list, session->backpath);
      otk_draw_all(oxine->otk);
    }
  } else if (item->type == TYPE_UP) {
    bpop(session->backpath);
    changelist(session->list, session->backpath);
    otk_draw_all(oxine->otk);
  } else if (session->action == MM_ACTION_PLAY) {
    if ((item->type == TYPE_REG)||(item->type == TYPE_RREG)) {      
      printf("mediamarks: playing %s\n", item->mrl);
/*      if (is_movie(item->mrl)) {
        oxine->main_window = NULL;
        oxine->info_window = NULL;
        otk_clear(oxine->otk);
        if (oxine->filename)
          ho_free(oxine->filename);
        oxine->filename=ho_strdup(item->mrl);
        select_subtitle_cb(get_dirname(oxine->filename), oxine, 0);
      } else */ if(odk_open_and_play(oxine->odk, item->mrl)) {
        set_ilabel(session);
        oxine->main_window = NULL;
        oxine->info_window = NULL;
        oxine->mode = OXINE_MODE_NORMAL;
        otk_clear(oxine->otk);
      } /* else play_error(oxine); */
      set_ilabel(session);
    } else if (item->type == TYPE_M3U) {
      parse_m3u(item->mrl, item->sub);  
      bpush(session->backpath, item->sub);    
      changelist(session->list, session->backpath);
      otk_draw_all(oxine->otk);
    } else if (item->type == TYPE_MULTIPLE) {
      if (parse_multiple(oxine, item->mrl, item->sub)) {
        bpush(session->backpath, item->sub);
        changelist(session->list, session->backpath);
        otk_draw_all(oxine->otk);
      } else
        printf("mediamarks: error getting autoplay mrls\n");
    } else printf("mediamarks: %s is of unknown type\n", item->mrl);
  } else if (session->action == MM_ACTION_ADD) {
    if ((item->type == TYPE_REG)||(item->type == TYPE_RREG)) {
      odk_enqueue(session->oxine->odk, item->mrl); /* FIXME: item->title unused */
      set_ilabel(session);
      otk_draw_all(session->oxine->otk);
    }
  }
}

static void mediamarks_leavetoplaylist_cb (void *data) {

  mm_session_t *session = (mm_session_t*) data;

  session->oxine->reentry = NULL;
  session->oxine->reentry_data = NULL;
  
  playlist_cb(session->oxine);

}

static void mediamarks_freeall(void *data) {

  mm_session_t *session = (mm_session_t *) data;
  list_t *current = list_first_content(session->backpath);

  current = list_first_content(session->backpath);

  while(current) {
    free_subtree(current, 1);
    list_delete_current(session->backpath);
    current = list_first_content(session->backpath);
  }

  list_free(session->backpath);
}

static void mediamarks_leave_cb (void *data) {

  mm_session_t *session = (mm_session_t*) data;

  session->oxine->reentry = NULL;
  session->oxine->reentry_data = NULL;

  mediamarks_freeall(session);
  
  session->oxine->main_menu_cb(session->oxine);
  ho_free(session);
}

static void mediamarks_reentry_cb (void *data) {

  mm_session_t *session = (mm_session_t*) data;
  oxine_t      *oxine = session->oxine;
  otk_widget_t *b, *list_window;
  
  lock_job_mutex();
  if (oxine->info_window && oxine->media_info_close_cb) {
    oxine->media_info_close_cb(oxine);
  }
  unlock_job_mutex();

  otk_clear(oxine->otk);

  oxine->main_window = otk_window_new (oxine->otk, NULL, 20, 130, 225, 420);

  otk_label_new (oxine->main_window, 108, 60, OTK_ALIGN_CENTER|OTK_ALIGN_BOTTOM, "Action:");
  b = otk_selector_new (oxine->main_window, 10, 80, 195, 60, mm_action_strings, 2, action_changed, session);
  otk_set_focus(b);
  otk_selector_set(b, session->action);
  otk_button_new (oxine->main_window, 10, 170, 195, 60, "Playlist", mediamarks_leavetoplaylist_cb, session);
  set_ilabel(session);
  otk_label_new (oxine->main_window, 108, 280, OTK_ALIGN_CENTER|OTK_ALIGN_BOTTOM, session->ilabel);
  otk_button_new (oxine->main_window, 10, 340, 195, 60, "Back", mediamarks_leave_cb, session);

  list_window = otk_window_new (oxine->otk, NULL, 245, 130, 535, 420);

  session->list = otk_list_new(list_window, 10, 15, 523, 390, mediamarks_play_cb, session); 
  changelist(session->list, session->backpath);
  otk_list_set_pos(session->list, session->listpos);
  
  otk_draw_all(oxine->otk);
}


void mediamarks_cb (void *data) {

  oxine_t      *oxine = (oxine_t*) data;
  char         mmpath[XITK_NAME_MAX];
  mm_session_t *session;
  list_t  *items = list_new();

  session = ho_new(mm_session_t);
  session->oxine = oxine;
  session->backpath = list_new();
  session->action = 1;
  session->listpos = 0;

  memset(mmpath,0,sizeof(mmpath));
  snprintf(mmpath,sizeof(mmpath),"%s/.xine/oxine/mediamarks", xine_get_homedir());
  if (!read_mediamarks(items, mmpath)) {
    lprintf("trying to load system wide mediamarks\n");
    snprintf(mmpath,sizeof(mmpath),"%s/mediamarks", XINE_OXINEDIR);
    if (read_mediamarks(items, mmpath)) {
      bpush(session->backpath, items);
      oxine->reentry_data = session;
      oxine->reentry = mediamarks_reentry_cb;
      mediamarks_reentry_cb(session);
    } else {
      list_free(items);
      list_free(session->backpath);
      ho_free(session);
    }
  } else {
    bpush(session->backpath, items);
    oxine->reentry_data = session;
    oxine->reentry = mediamarks_reentry_cb;
    mediamarks_reentry_cb(session);
  }
}
