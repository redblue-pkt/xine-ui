/*
 * Copyright (C) 2000-2003 the xine project
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       391

#define MAXFILES            65535

#ifndef S_ISLNK
#define S_ISLNK(mode)  0
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) 0
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(mode) 0
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode)  0
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode)  0
#endif
#ifndef S_ISREG
#define S_ISREG(mode)  0
#endif
#if !S_IXUGO
#define S_IXUGO        (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

extern gGui_t          *gGui;

static char            *fontname           = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char            *btnfontname        = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

typedef struct {
  char                      *name;
  char                      *ending;
} filebrowser_filter_t;

static filebrowser_filter_t __fb_filters[] = {
  { NULL                  , "*"                        },
  { NULL                  , "sub,srt,asc,smi,ssa"      }, /* subtitles */
  { NULL                  , "pls,m3u,sfv,tox,asx,smil" }, /* playlists */
  { "*.vob"               , "vob"                      }, /* mpeg block */
  { "*.mpv"               , "mpv"                      }, /* elementary */
  { "*.mpg, *.mpeg, *.mpe", "mpg,mpeg,mpe"             }, /* mpeg */
  { "*.avi"               , "avi"                      }, /* avi */
  { "*.mp3"               , "mp3"                      }, /* mp3 */
  { "*.asf, *.wma, *.wmv" , "asf,wma,wmv"              }, /* asf */
  { "*.cpk, *.cak, *.film", "cpk,cak,film"             }, /* film */
  { "*.ogg"               , "ogg"                      }, /* ogg */
  { "*.vdr"               , "vdr"                      }, /* pes */
  { "*.mov, *.mp4, *.qt"  , "mov,mp4,qt"               }, /* QT */
  { "*.rm"                , "rm"                       }, /* rm */
  { "*.roq"               , "roq"                      }, /* roq */
  { "*.m2t, *.ts, *.trp"  , "m2t,ts,trp"               }, /* ts */
  { "*.dv"                , "dv"                       }, /* dv */
  { NULL                  , NULL                       }
};

typedef struct {
  char                  *name;
} fileinfo_t;

#define DEFAULT_SORT 0
#define REVERSE_SORT 1

#define NORMAL_CURS  0
#define WAIT_CURS    1

struct filebrowser_s {
  xitk_window_t                  *xwin;
  
  xitk_widget_list_t             *widget_list;

  xitk_widget_t                  *origin;

  Cursor                          cursor[2];

  xitk_widget_t                  *directories_browser;
  xitk_widget_t                  *directories_sort;
  int                             directories_sort_direction;
  xitk_widget_t                  *files_browser;
  xitk_widget_t                  *files_sort;
  int                             files_sort_direction;

  xitk_image_t                   *sort_skin_up;
  xitk_image_t                   *sort_skin_down;

  xitk_widget_t                  *rename;
  xitk_widget_t                  *delete;
  xitk_widget_t                  *create;

  xitk_widget_t                  *filters;
  int                             filter_selected;
  char                          **file_filters;

  xitk_widget_t                  *show_hidden;
  int                             show_hidden_files;

  char                            current_dir[XINE_PATH_MAX + 1];
  char                            filename[XINE_NAME_MAX + 1];

  fileinfo_t                     *dir_files;
  char                          **directories;
  int                             directories_num;
  int                             directories_sel;

  fileinfo_t                     *norm_files;
  char                          **files;
  int                             files_num;

  int                             running;
  int                             visible;
  
  xitk_widget_t                  *close;

  filebrowser_callback_button_t   cbb[3];
  xitk_widget_t                  *cb_buttons[2];
  
  xitk_register_key_t             widget_key;
};


typedef struct {
  xitk_window_t            *xwin;
  xitk_widget_list_t       *widget_list;
  xitk_widget_t            *input;

  xitk_string_callback_t    callback;
  filebrowser_t            *fb;
  
  xitk_widget_t            *button_apply;
  xitk_widget_t            *button_cancel;

  xitk_register_key_t       widget_key;
} filename_editor_t;

/*
 * Enable/disable widgets in file browser window
 */
static void _fb_enability(filebrowser_t *fb, int enable) {
  void (*enability)(xitk_widget_t *) = (enable == 1) ? xitk_enable_widget : xitk_disable_widget;
  
  enability(fb->origin);
  enability(fb->directories_browser);
  enability(fb->directories_sort);
  enability(fb->files_browser);
  enability(fb->files_sort);
  enability(fb->rename);
  enability(fb->delete);
  enability(fb->create);
  enability(fb->filters);
  if(fb->cb_buttons[0]) {
    enability(fb->cb_buttons[0]);
    if(fb->cb_buttons[1])
      enability(fb->cb_buttons[1]);
  }
  enability(fb->show_hidden);
  enability(fb->close);
}
static void fb_deactivate(filebrowser_t *fb) {
  _fb_enability(fb, 0);
}
static void fb_reactivate(filebrowser_t *fb) {
  _fb_enability(fb, 1);
}

static void _fb_set_cursor(filebrowser_t *fb, int state) {
  if(fb) {
    XLockDisplay(gGui->display);
    XDefineCursor(gGui->display, (xitk_window_get_window(fb->xwin)), fb->cursor[state]);
    XSync(gGui->display, False);
    XUnlockDisplay(gGui->display);
  }
}
/*
 * **************************************************
 */
static void fb_exit(xitk_widget_t *, void *);

/*
 * ************************* filename editor **************************
 */
static void fne_destroy(filename_editor_t *fne) {
  if(fne) {

    xitk_unregister_event_handler(&fne->widget_key);

    xitk_destroy_widgets(fne->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, fne->xwin);

    fne->xwin = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(fne->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(fne->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(fne->widget_list);
    
    free(fne);
  }
}
static void fne_apply_cb(xitk_widget_t *w, void *data) {
  filename_editor_t *fne = (filename_editor_t *) data;
  
  if(fne->callback)
    fne->callback(NULL, (void *)fne->fb, (xitk_inputtext_get_text(fne->input)));

  fb_reactivate(fne->fb);
  fne_destroy(fne);
}
static void fne_cancel_cb(xitk_widget_t *w, void *data) {
  filename_editor_t *fne = (filename_editor_t *) data;

  fb_reactivate(fne->fb);
  fne_destroy(fne);
}
static void fb_create_input_window(char *title, char *text,
				   xitk_string_callback_t cb, filebrowser_t *fb) {
  filename_editor_t          *fne;
  GC                          gc;
  int                         x, y, w;
  int                         width = WINDOW_WIDTH;
  int                         height = 100;
  xitk_labelbutton_widget_t   lb;
  xitk_inputtext_widget_t     inp;
  
  XLockDisplay(gGui->display);
  x = (((DisplayWidth(gGui->display, gGui->screen))) >> 1) - (width >> 1);
  y = (((DisplayHeight(gGui->display, gGui->screen))) >> 1) - (height >> 1);
  XUnlockDisplay(gGui->display);

  fne = (filename_editor_t *) xine_xmalloc(sizeof(filename_editor_t));
  
  fne->callback = cb;
  fne->fb = fb;

  fne->xwin = xitk_window_create_dialog_window(gGui->imlib_data, title, x, y, width, height);

  XLockDisplay(gGui->display);
  gc = XCreateGC(gGui->display, (xitk_window_get_window(fne->xwin)), None, None);
  XUnlockDisplay(gGui->display);
  
  fne->widget_list                = xitk_widget_list_new();

  xitk_widget_list_set(fne->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(fne->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(fne->xwin)));
  xitk_widget_list_set(fne->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);

  x = 15;
  y = 23;
  w = width - 30;
  
  inp.skin_element_name = NULL;
  inp.text              = text;
  inp.max_length        = XITK_PATH_MAX + XITK_NAME_MAX + 1;
  inp.callback          = NULL;
  inp.userdata          = (void *)fne;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fne->widget_list)),
	    (fne->input = 
	     xitk_noskin_inputtext_create(fne->widget_list, &inp,
					  x, y, w, 20, "Black", "Black", fontname)));
  xitk_enable_and_show_widget(fne->input);

  y = height - 23 - 15;
  x = width - (100 * 2) - (15 * 2);
  w = 100;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_apply_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fne->widget_list)),
	   (fne->button_apply = 
	    xitk_noskin_labelbutton_create(fne->widget_list, 
					   &lb, x, y, w, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fne->button_apply);

  x += 100 + 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Cancel");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_cancel_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fne->widget_list)),
	   (fne->button_cancel = 
	    xitk_noskin_labelbutton_create(fne->widget_list, 
					   &lb, x, y, w, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fne->button_cancel);

  {
    char buffer[256];
    sprintf(buffer, "filenameed%d", (unsigned int) time(NULL));
    
    fne->widget_key = xitk_register_event_handler(buffer, 
						  (xitk_window_get_window(fne->xwin)),
						  NULL,
						  NULL,
						  NULL,
						  fne->widget_list,
						  NULL);
  }
  
  XLockDisplay(gGui->display);
  XRaiseWindow(gGui->display, xitk_window_get_window(fne->xwin));
  XMapWindow(gGui->display, xitk_window_get_window(fne->xwin));
  if(!gGui->use_root_window)
    XSetTransientForHint(gGui->display, 
			 xitk_window_get_window(fne->xwin), gGui->video_window);
  XUnlockDisplay(gGui->display);
  layer_above_video(xitk_window_get_window(fne->xwin));
  
  while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(fne->xwin)))
    xine_usec_sleep(5000);
  
  XLockDisplay(gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(fne->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay(gGui->display);
}
/*
 * ************************** END OF filename editor **************************
 */

/*
 * **************************** File related funcs **************************
 */

/*
 * Return 1 if file match with current filter, otherwise 0.
 */
static int is_file_match_to_filter(filebrowser_t *fb, char *file) {
  char  *filter_ends, *m, *ending;
  
  if(!fb->filter_selected)
    return 1;
  
  xine_strdupa(filter_ends, __fb_filters[fb->filter_selected].ending);
  if((ending = strrchr(file, '.')) != NULL) {
    
    while((m = xine_strsep(&filter_ends, ",")) != NULL) {
      
      if(!strcasecmp((ending + 1), m))
	return 1;
      
    }
  }
  
  return 0;
}

static void fb_update_origin(filebrowser_t *fb) {
  char   buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  
  if(strcasecmp(fb->current_dir, "/")) {
    if(strlen(fb->filename))
      sprintf(buf, "%s/%s", fb->current_dir, fb->filename);
    else
      sprintf(buf, "%s", fb->current_dir);
  }
  else {
    if(strlen(fb->filename))
      sprintf(buf, "%s%s", fb->current_dir, fb->filename);
    else
      sprintf(buf, "%s", fb->current_dir);
  }
  
  xitk_inputtext_change_text(fb->origin, buf); 
}

static void fb_extract_path_and_file(filebrowser_t *fb, char *filepathname) {
  if(filepathname) {
    char *filename = NULL;
    char *_filepathname;
    
    xine_strdupa(_filepathname, filepathname);
    
    if(!is_a_dir((char *)_filepathname))
      filename = strrchr(_filepathname, '/');
    
    if(filename && (strlen(filename) > 1)) {
      
      *filename++ = '\0';
      sprintf(fb->filename, "%s", filename);
      
      if(is_a_dir(_filepathname))
	sprintf(fb->current_dir, "%s", _filepathname);

    }
    else {
      if(is_a_dir((char *)_filepathname)) {
	sprintf(fb->current_dir, "%s", _filepathname);
	memset(&fb->filename, 0, sizeof(fb->filename));
      }
    }
  }
}

/*
 * Sorting function, it comes from GNU fileutils package.
 */
#define S_N        0x0
#define S_I        0x4
#define S_F        0x8
#define S_Z        0xC
#define CMP          2
#define LEN          3
#define ISDIGIT(c)   ((unsigned) (c) - '0' <= 9)
static int strverscmp(const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;
  static const unsigned int next_state[] = {
    S_N, S_I, S_Z, S_N,
    S_N, S_I, S_I, S_I,
    S_N, S_F, S_F, S_F,
    S_N, S_F, S_Z, S_Z
  };
  static const int result_type[] = {
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,  -1,  -1, CMP,   1, LEN, LEN, CMP,
      1, LEN, LEN, CMP, CMP, CMP, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,   1,   1, CMP,  -1, CMP, CMP, CMP,
     -1, CMP, CMP, CMP
  };

  if(p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;

  state = S_N | ((c1 == '0') + (ISDIGIT(c1) != 0));

  while((diff = c1 - c2) == 0 && c1 != '\0') {
    state = next_state[state];
    c1 = *p1++;
    c2 = *p2++;
    state |= (c1 == '0') + (ISDIGIT(c1) != 0);
  }
  
  state = result_type[state << 2 | ((c2 == '0') + (ISDIGIT(c2) != 0))];
  
  switch(state) {
  case CMP:
    return diff;
    
  case LEN:
    while(ISDIGIT(*p1++))
      if(!ISDIGIT(*p2++))
	return 1;
    
    return ISDIGIT(*p2) ? -1 : diff;
    
  default:
    return state;
  }
}

/*
 * Wrapper to strverscmp() for qsort() calls, which sort mrl_t type array.
 */
static int _sortfiles_default(const fileinfo_t *s1, const fileinfo_t *s2) {
  return(strverscmp(s1->name, s2->name));
}
static int _sortfiles_reverse(const fileinfo_t *s1, const fileinfo_t *s2) {
  return(strverscmp(s2->name, s1->name));
}

static void sort_files(filebrowser_t *fb) {
  int  (*func) () = NULL;

  if(fb->files) {
    int i = 0;
    
    while(fb->files[i]) {
      SAFE_FREE(fb->files[i]);
      i++;
    }
  }
  
  if(fb->files_num) {
    int i;
    
    if(fb->files_sort_direction == DEFAULT_SORT)
      func = _sortfiles_default;
    else if(fb->files_sort_direction == REVERSE_SORT)
      func = _sortfiles_reverse;
    
    qsort(fb->norm_files, fb->files_num, sizeof(fileinfo_t), func);
    
    fb->files = (char **) realloc(fb->files, sizeof(char *) * (fb->files_num + 2));

    for(i = 0; i < fb->files_num; i++)
      fb->files[i] = strdup(fb->norm_files[i].name);

    fb->files[i] = NULL;
  }
  
  xitk_browser_update_list(fb->files_browser, (const char *const *)fb->files, fb->files_num, 0);
}

static void sort_directories(filebrowser_t *fb) {
  int  (*func) () = NULL;

  if(fb->directories) {
    int i = 0;

    while(fb->directories[i]) {
      SAFE_FREE(fb->directories[i]);
      i++;
    }
  }

  if(fb->directories_num) {
    int i;
    
    if(fb->directories_sort_direction == DEFAULT_SORT)
      func = _sortfiles_default;
    else if(fb->directories_sort_direction == REVERSE_SORT)
      func = _sortfiles_reverse;
    
    qsort(fb->dir_files, fb->directories_num, sizeof(fileinfo_t), func);
    
    fb->directories = (char **) realloc(fb->directories, sizeof(char *) * (fb->directories_num + 2));
    for(i = 0; i < fb->directories_num; i++) {
      fb->directories[i] = (char *) xine_xmalloc(strlen(fb->dir_files[i].name) + 2);
      sprintf(fb->directories[i], "%s%c", fb->dir_files[i].name, '/');
    }

    fb->directories[i] = NULL;
  }

  xitk_browser_update_list(fb->directories_browser, 
			   (const char *const *)fb->directories, fb->directories_num, 0);
}

static void fb_getdir(filebrowser_t *fb) {
  char                  fullfilename[XINE_PATH_MAX + XINE_NAME_MAX + 1];
  struct dirent        *pdirent;
  int                   num_dir_files   = 0;
  int                   num_norm_files  = 0;
  DIR                  *pdir;
  int                   num_files       = -1;

  _fb_set_cursor(fb, WAIT_CURS);

  if(fb->norm_files) {
    while(fb->files_num) {
      free(fb->norm_files[fb->files_num - 1].name);
      fb->files_num--;
    }
  }

  if(fb->dir_files) {
    while(fb->directories_num) {
      free(fb->dir_files[fb->directories_num - 1].name);
      fb->directories_num--;
    }
  }
  
  if((pdir = opendir(fb->current_dir)) == NULL) {
    char *p = strrchr(fb->current_dir, '/');
    
    xine_error(_("Unable to open directory '%s': %s."), 
	       (p) ? p + 1 : fb->current_dir, strerror(errno));
    
    /* One step back */
    if(p)
      *p = '\0';

    fb_update_origin(fb);
    fb_getdir(fb);
    return;
  }

  while((pdirent = readdir(pdir)) != NULL) {
    
    memset(&fullfilename, 0, sizeof(fullfilename));
    sprintf(fullfilename, "%s/%s", fb->current_dir, pdirent->d_name);
    
    if(is_a_dir(fullfilename)) {
      
      /* if user don't want to see hidden files, ignore them */
      if(fb->show_hidden_files == 0 && 
	 ((strlen(pdirent->d_name) > 1)
	  && (pdirent->d_name[0] == '.' &&  pdirent->d_name[1] != '.'))) {
	;
      }
      else {
	fb->dir_files[num_dir_files].name = strdup(pdirent->d_name);
	num_dir_files++;
      }
      
    } /* Hmmmm, an hidden file ? */
    else if((strlen(pdirent->d_name) > 1)
	    && (pdirent->d_name[0] == '.' &&  pdirent->d_name[1] != '.')) {
      
      /* if user don't want to see hidden files, ignore them */
      if(fb->show_hidden_files) {
	if(is_file_match_to_filter(fb, pdirent->d_name)) {
	  fb->norm_files[num_norm_files].name = strdup(pdirent->d_name);
	  num_norm_files++;
	}
      }

    } /* So a *normal* one. */
    else {
      if(is_file_match_to_filter(fb, pdirent->d_name)) {
	fb->norm_files[num_norm_files].name = strdup(pdirent->d_name);
	num_norm_files++;
      }
    }
    
    num_files++;
  }
  
  closedir(pdir);
  
  /*
   * Sort arrays
   */
  fb->directories_num = num_dir_files;
  fb->files_num = num_norm_files;
  sort_directories(fb);
  sort_files(fb);
  _fb_set_cursor(fb, NORMAL_CURS);
}

/*
 * ****************************** END OF file related funcs ***************************
 */

/*
 * ***************************** widget callbacks *******************************
 */
static void fb_select(xitk_widget_t *w, void *data, int selected) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  if(w == fb->files_browser) {
    sprintf(fb->filename, "%s", fb->norm_files[selected].name);
    fb_update_origin(fb);
  }
}

static void fb_callback_button_cb(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  if(w == fb->cb_buttons[0]) {
    if(fb->cbb[0].need_a_file && (!strlen(fb->filename)))
      return;
    fb->cbb[0].callback(fb);
  }
  else if(w == fb->cb_buttons[1]) {
    if(fb->cbb[1].need_a_file && (!strlen(fb->filename)))
      return;
    fb->cbb[1].callback(fb);
  }
  
  fb_exit(NULL, (void *)fb);
}


static void fb_dbl_select(xitk_widget_t *w, void *data, int selected) {
  filebrowser_t *fb = (filebrowser_t *) data;

  if(w == fb->directories_browser) {
    char buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    /* Want to re-read current dir */
    if(!strcasecmp(fb->dir_files[selected].name, ".")) {
      /* NOOP */
    }
    else if(!strcasecmp(fb->dir_files[selected].name, "..")) {
      char *p;
      
      memset(&buf, '\0', sizeof(buf));
      sprintf(buf, "%s", fb->current_dir);
      if(strlen(buf) > 1) { /* not '/' directory */
	
	p = &buf[strlen(buf)-1];
	while(*p && *p != '/') {
	  *p = '\0';
	  p--;
	}
	
	/* Remove last '/' if current_dir isn't root */
	if((strlen(buf) > 1) && *p == '/') 
	  *p = '\0';
	
	sprintf(fb->current_dir, "%s", buf);
      }
    }
    else {
      
      /* not '/' directory */
      if(strcasecmp(fb->current_dir, "/")) {
	memset(&buf, 0, sizeof(buf));
	sprintf(buf, "%s/%s", fb->current_dir, fb->dir_files[selected].name);
      }
      else {
	memset(&buf, 0, sizeof(buf));
	sprintf(buf, "/%s", fb->dir_files[selected].name);
      }
      
      if(is_a_dir(buf))
	sprintf(fb->current_dir, "%s", buf);

    }
    
    fb_update_origin(fb);
    fb_getdir(fb);
  }
  else if(w == fb->files_browser) {
    sprintf(fb->filename, "%s", fb->norm_files[selected].name);
    fb_callback_button_cb(fb->cb_buttons[0], (void *)data);
  }

}

static void fb_change_origin(xitk_widget_t *w, void *data, char *currenttext) {
  filebrowser_t *fb = (filebrowser_t *)data;
  char           buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char          *p;

  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s", currenttext);

  p = &buf[strlen(buf) - 1];
  while((strlen(buf) > 1) && (*p == '/'))
    *p-- = '\0';

  fb_extract_path_and_file(fb, buf);
  fb_update_origin(fb);
  fb_getdir(fb);
}

static void fb_sort(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  if(w == fb->directories_sort) {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->directories_sort);
    
    fb->directories_sort_direction = (fb->directories_sort_direction == DEFAULT_SORT) ? 
      REVERSE_SORT : DEFAULT_SORT;
    
    xitk_hide_widget(fb->directories_sort);

    if(fb->directories_sort_direction == DEFAULT_SORT)
      xitk_image_change_image(gGui->imlib_data, fb->sort_skin_down,
			      dsimage, dsimage->width, dsimage->height);
    else
      xitk_image_change_image(gGui->imlib_data, fb->sort_skin_up, 
			    dsimage, dsimage->width, dsimage->height);

    xitk_show_widget(fb->directories_sort);
    
    sort_directories(fb);
  }
  else if(w == fb->files_sort) {
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->files_sort);
    
    fb->files_sort_direction = (fb->files_sort_direction == DEFAULT_SORT) ? 
      REVERSE_SORT : DEFAULT_SORT;
    
    xitk_hide_widget(fb->files_sort);

    if(fb->files_sort_direction == DEFAULT_SORT)
      xitk_image_change_image(gGui->imlib_data, fb->sort_skin_down,
			      fsimage, fsimage->width, fsimage->height);
    else
      xitk_image_change_image(gGui->imlib_data, fb->sort_skin_up, 
			    fsimage, fsimage->width, fsimage->height);

    xitk_show_widget(fb->files_sort);

    sort_files(fb);
  }    
}

static void fb_exit(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  if(fb) {
    int i;

    fb->running = 0;
    fb->visible = 0;
    
    xitk_unregister_event_handler(&fb->widget_key);

    xitk_destroy_widgets(fb->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, fb->xwin);

    fb->xwin = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(fb->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeCursor(gGui->display, fb->cursor[NORMAL_CURS]);
    XFreeCursor(gGui->display, fb->cursor[WAIT_CURS]);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(fb->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(fb->widget_list);

    if(fb->norm_files) {
      while(fb->files_num) {
	free(fb->norm_files[fb->files_num - 1].name);
	fb->files_num--;
      }
    }
    SAFE_FREE(fb->norm_files);
    
    if(fb->dir_files) {
      while(fb->directories_num) {
	free(fb->dir_files[fb->directories_num - 1].name);
	fb->directories_num--;
      }
    }
    SAFE_FREE(fb->dir_files);
    
    if(fb->files) {
      i = 0;
      
      while(fb->files[i]) {
	free(fb->files[i]);
	i++;
      }
      free(fb->files);
    }

    if(fb->directories) {
      i = 0;
      
      while(fb->directories[i]) {
	free(fb->directories[i]);
	i++;
      }
      free(fb->directories);
    }

    for(i = 0; fb->file_filters[i]; i++)
      free(fb->file_filters[i]);
    free(fb->file_filters);
    
    SAFE_FREE(fb->cbb[0].label);
    SAFE_FREE(fb->cbb[1].label);
    
    xitk_image_free_image(gGui->imlib_data, &fb->sort_skin_up);
    xitk_image_free_image(gGui->imlib_data, &fb->sort_skin_down);

    free(fb);
    fb = NULL;
  }
}
static void _fb_exit(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  if(fb->cbb[2].callback)
    fb->cbb[2].callback(fb);
  fb_exit(NULL, (void *)fb);
}

static void fb_delete_file_cb(xitk_widget_t *w, void *data, int button) {
  filebrowser_t *fb = (filebrowser_t *) data;

  switch(button) {
  case XITK_WINDOW_ANSWER_YES:
    {
      char buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
      int sel = xitk_browser_get_current_selected(fb->files_browser);

      memset(&buf, 0, sizeof(buf));
      sprintf(buf, "%s", fb->current_dir);
      if(strlen(fb->current_dir) > 1)
	sprintf(buf, "%s/", buf);
      sprintf(buf, "%s%s", buf, fb->norm_files[sel].name);
      
      if((unlink(buf)) == -1)
	xine_error(_("Unable to delete file '%s': %s."), buf, strerror(errno));
      else
	fb_getdir(fb);

    }
    break;
  }
  fb_reactivate(fb);
}

static void fb_delete_file(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;
  
  if((sel = xitk_browser_get_current_selected(fb->files_browser)) >= 0) {
    char buf[256 + XITK_PATH_MAX + XITK_NAME_MAX + 1];

    sprintf(buf, "Do you really want to delete the file: '%s", fb->current_dir);
    if(strlen(fb->current_dir) > 1)
      sprintf(buf, "%s/", buf);
    sprintf(buf, "%s%s' ?.", buf, fb->norm_files[sel].name);
    
    fb_deactivate(fb);
    xitk_window_dialog_yesno(gGui->imlib_data, _("Confirm deletion ?"),
			     fb_delete_file_cb, 
			     fb_delete_file_cb, 
			     (void *)fb, ALIGN_DEFAULT, buf);
  }
}

static void fb_rename_file_cb(xitk_widget_t *w, void *data, char *newname) {
  filebrowser_t *fb = (filebrowser_t *) data;
  char buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  int sel = xitk_browser_get_current_selected(fb->files_browser);
  
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s", fb->current_dir);
  if(strlen(fb->current_dir) > 1)
    sprintf(buf, "%s/", buf);
  sprintf(buf, "%s%s", buf, fb->norm_files[sel].name);
  
  if((rename(buf, newname)) == -1)
    xine_error(_("Unable to rename file '%s' to '%s': %s."), buf, newname, strerror(errno));
  else
    fb_getdir(fb);

}
static void fb_rename_file(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;
  
  if((sel = xitk_browser_get_current_selected(fb->files_browser)) >= 0) {
    char buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    memset(&buf, 0, sizeof(buf));
    sprintf(buf, "%s", fb->current_dir);
    if(strlen(fb->current_dir) > 1)
      sprintf(buf, "%s/", buf);
    sprintf(buf, "%s%s", buf, fb->norm_files[sel].name);
    
    fb_deactivate(fb);
    fb_create_input_window(_("Rename file"), buf, fb_rename_file_cb, fb);
  }
}

static void fb_create_directory_cb(xitk_widget_t *w, void *data, char *newdir) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  if(!mkdir_safe(newdir))
    xine_error(_("Unable to create the directory '%s': %s."), newdir, strerror(errno));
  else
    fb_getdir(fb);
}
static void fb_create_directory(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  char           buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s", fb->current_dir);
  if(strlen(fb->current_dir) > 1)
    sprintf(buf, "%s/", buf);
  
  fb_deactivate(fb);
  fb_create_input_window(_("Create a new directory"), buf, fb_create_directory_cb, fb);
}

static void fb_select_file_filter(xitk_widget_t *w, void *data, int selected) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  fb->filter_selected = selected;
  fb_getdir(fb);
}

static void fb_hidden_files(xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  fb->show_hidden_files = state;
  fb_getdir(fb);
}
static void fb_lbl_hidden_files(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  xitk_checkbox_set_state(fb->show_hidden, (!xitk_checkbox_get_state(fb->show_hidden)));
  xitk_checkbox_callback_exec(fb->show_hidden);
}

static void fb_handle_events(XEvent *event, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  switch(event->type) {

  case KeyPress:
    {
      int sel = xitk_browser_get_current_selected(fb->files_browser);
      
      if(sel >= 0)
	fb_select(fb->files_browser, (void *) fb, sel);
    }
    break;
  }
}

void filebrowser_raise_window(filebrowser_t *fb) {
  if(fb != NULL) {
    if(fb->xwin) {
      if(fb->visible && fb->running) {
	  XLockDisplay(gGui->display);
	  XUnmapWindow(gGui->display, xitk_window_get_window(fb->xwin));
	  XRaiseWindow(gGui->display, xitk_window_get_window(fb->xwin));
	  XMapWindow(gGui->display, xitk_window_get_window(fb->xwin));
	  if(!gGui->use_root_window)
	    XSetTransientForHint (gGui->display, 
				  xitk_window_get_window(fb->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(fb->xwin));
      }
    }
  }
}

void filebrowser_end(filebrowser_t *fb) {
  if(fb)
    fb_exit(NULL, (void *)fb);
}

char *filebrowser_get_current_dir(filebrowser_t *fb) {
  char *current_dir = NULL;

  if(fb)
    current_dir = strdup(fb->current_dir);
  
  return current_dir;
}

char *filebrowser_get_current_filename(filebrowser_t *fb) {
  char *filename = NULL;
  
  if(fb && strlen(fb->filename))
    filename = strdup(fb->filename);
  
  return filename;
}

char *filebrowser_get_full_filename(filebrowser_t *fb) {
  char *fullfilename = NULL;

  if(fb)
    fullfilename = strdup(xitk_inputtext_get_text(fb->origin));

  return fullfilename;
}

char **filebrowser_get_all_files(filebrowser_t *fb) {
  char **files = NULL;

  if(fb && fb->files_num) {
    int i;
    files = (char **) xine_xmalloc(sizeof(char *) * (fb->files_num + 2));
    
    for(i = 0; i < fb->files_num; i++)
      files[i] = strdup(fb->norm_files[i].name);
    files[i] = NULL;
  }
  
  return files;
}

filebrowser_t *create_filebrowser(char *window_title, char *filepathname,
				  filebrowser_callback_button_t *cbb1,
				  filebrowser_callback_button_t *cbb2,
				  filebrowser_callback_button_t *cbb_close) {
  filebrowser_t              *fb;
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  xitk_browser_widget_t       br;
  xitk_inputtext_widget_t     inp;
  xitk_button_widget_t        b;
  xitk_combo_widget_t         cmb;
  xitk_widget_t              *widget;
  int                         i, x, y, w, width, height;

  fb = (filebrowser_t *) xine_xmalloc(sizeof(filebrowser_t));
  
  if(cbb1 && (strlen(cbb1->label) && cbb1->callback)) {
    fb->cbb[0].label = strdup(cbb1->label);
    fb->cbb[0].callback = cbb1->callback;
    fb->cbb[0].need_a_file = cbb1->need_a_file;
    if(cbb2 && (strlen(cbb2->label) && cbb2->callback)) {
      fb->cbb[1].label = strdup(cbb2->label);
      fb->cbb[1].callback = cbb2->callback;
      fb->cbb[1].need_a_file = cbb2->need_a_file;
  }
    else {
      fb->cbb[1].label = NULL;
      fb->cbb[1].callback = NULL;
    }
  }
  else {
    fb->cbb[0].label = NULL;
    fb->cbb[0].callback = NULL;
    fb->cbb[1].label = NULL;
    fb->cbb[1].callback = NULL;
  }

  if(cbb_close)
    fb->cbb[2].callback = cbb_close->callback;
  
  XLockDisplay(gGui->display);
  x = (((DisplayWidth(gGui->display, gGui->screen))) >> 1) - (WINDOW_WIDTH >> 1);
  y = (((DisplayHeight(gGui->display, gGui->screen))) >> 1) - (WINDOW_HEIGHT >> 1);
  XUnlockDisplay(gGui->display);

  /* Create window */
  fb->xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
					      (window_title) ? window_title : _("File Browser"), 
					      x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  fb->directories                = NULL;
  fb->directories_num            = 0;
  fb->files                      = NULL;
  fb->files_num                  = 0;
  fb->directories_sort_direction = DEFAULT_SORT;
  fb->files_sort_direction       = DEFAULT_SORT;
  fb->show_hidden_files          = 1;

  sprintf(fb->current_dir, "%s", xine_get_homedir());
  memset(&fb->filename, 0, sizeof(fb->filename));
  fb_extract_path_and_file(fb, filepathname);

  fb->norm_files = (fileinfo_t *) xine_xmalloc(sizeof(fileinfo_t) * MAXFILES);
  fb->dir_files = (fileinfo_t *) xine_xmalloc(sizeof(fileinfo_t) * MAXFILES);

  fb->files = (char **) xine_xmalloc(sizeof(char *) * 2);
  fb->directories = (char **) xine_xmalloc(sizeof(char *) * 2);
  
  __fb_filters[0].name = _("All files");
  __fb_filters[1].name = _("All known subtitles");
  __fb_filters[2].name = _("All known playlists");

  fb->file_filters = (char **) xine_xmalloc(sizeof(filebrowser_filter_t) * ((sizeof(__fb_filters) / sizeof(__fb_filters[0])) + 1));
  
  for(i = 0; __fb_filters[i].ending; i++)
    fb->file_filters[i] = strdup(__fb_filters[i].name);

  fb->file_filters[i] = NULL;
  fb->filter_selected = 0;

  XLockDisplay(gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(fb->xwin)), None, None);
  fb->cursor[NORMAL_CURS] = XCreateFontCursor(gGui->display, XC_left_ptr);
  fb->cursor[WAIT_CURS] = XCreateFontCursor(gGui->display, XC_watch);
  XUnlockDisplay(gGui->display);

  fb->widget_list                = xitk_widget_list_new();

  xitk_widget_list_set(fb->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(fb->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(fb->xwin)));
  xitk_widget_list_set(fb->widget_list, WIDGET_LIST_GC, gc);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);
  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  XITK_WIDGET_INIT(&b, gGui->imlib_data);

  xitk_window_get_window_size(fb->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(fb->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay(gGui->display);


  x = 15;
  y = 30;
  w = WINDOW_WIDTH - 30;

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = XITK_PATH_MAX + XITK_NAME_MAX + 1;
  inp.callback          = fb_change_origin;
  inp.userdata          = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)),
	    (fb->origin = 
	     xitk_noskin_inputtext_create(fb->widget_list, &inp,
					  x, y, w, 20, "Black", "Black", fontname)));
  xitk_enable_and_show_widget(fb->origin);

  fb_update_origin(fb);

  y += 46;
  w = (WINDOW_WIDTH >> 1) - 15 - 12 - 10;

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = 10;
  br.browser.num_entries           = fb->directories_num;
  br.browser.entries               = (const char *const *)fb->directories;
  br.callback                      = fb_select;
  br.dbl_click_callback            = fb_dbl_select;
  br.parent_wlist                  = fb->widget_list;
  br.userdata                      = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->directories_browser = 
	    xitk_noskin_browser_create(fb->widget_list, &br,
				       (XITK_WIDGET_LIST_GC(fb->widget_list)),
				       x, y, w, 20, 12, fontname)));
  xitk_enable_and_show_widget(fb->directories_browser);
  
  draw_rectangular_inner_box(gGui->imlib_data, bg, x - 1, y - 1,
			     xitk_get_widget_width(fb->directories_browser) + 2,
			     xitk_get_widget_height(fb->directories_browser) + 2);
  
  y -= 16;
  
  b.skin_element_name = NULL;
  b.callback          = fb_sort;
  b.userdata          = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->directories_sort =
	    xitk_noskin_button_create(fb->widget_list, &b, x - 1, y, w + 12 + 2, 15)));
  xitk_enable_and_show_widget(fb->directories_sort);

  x = WINDOW_WIDTH - (w + 15 + 12);
  y += 16;

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = 10;
  br.browser.num_entries           = fb->files_num;
  br.browser.entries               = (const char *const *)fb->files;
  br.callback                      = fb_select;
  br.dbl_click_callback            = fb_dbl_select;
  br.parent_wlist                  = fb->widget_list;
  br.userdata                      = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->files_browser = 
	    xitk_noskin_browser_create(fb->widget_list, &br,
				       (XITK_WIDGET_LIST_GC(fb->widget_list)),
				       x, y, w, 20, 12, fontname)));
  xitk_enable_and_show_widget(fb->files_browser);

  draw_rectangular_inner_box(gGui->imlib_data, bg, x - 1, y - 1,
			     xitk_get_widget_width(fb->files_browser) + 2,
			     xitk_get_widget_height(fb->files_browser) + 2);

  y -= 16;

  b.skin_element_name = NULL;
  b.callback          = fb_sort;
  b.userdata          = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->files_sort =
	    xitk_noskin_button_create(fb->widget_list, &b, x - 1, y, w + 12 + 2, 15)));
  xitk_enable_and_show_widget(fb->files_sort);


  {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->directories_sort);
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->files_sort);
    xitk_image_t *image;
    XPoint        points[4];
    int           i, j, w, h, offset;
    short         x1, x2, x3;
    short         y1, y2, y3;

    fb->sort_skin_up = xitk_image_create_image(gGui->imlib_data, 
					       dsimage->width, dsimage->height);
    fb->sort_skin_down = xitk_image_create_image(gGui->imlib_data, 
						 dsimage->width, dsimage->height);
    
    draw_bevel_three_state(gGui->imlib_data, fb->sort_skin_up);
    draw_bevel_three_state(gGui->imlib_data, fb->sort_skin_down);

    w = dsimage->width / 3;
    h = dsimage->height;
    
    for(j = 0; j < 2; j++) {
      if(j == 0)
	image = fb->sort_skin_up;
      else
	image = fb->sort_skin_down;
      
      offset = 0;
      for(i = 0; i < 2; i++) {
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    5, 4 + offset, w - 45, 1);
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    w - 20, 4 + offset, 10, 1);
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    w + 5, 4 + offset, w - 45, 1);
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    (w * 2) - 20, 4 + offset, 10, 1);
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    (w * 2) + 5 + 1, 4 + 1 + offset, w - 45, 1);
	draw_rectangular_outter_box(gGui->imlib_data, image->image, 
				    ((w * 3) - 20) + 1, 4 + 1 + offset, 10 + 1, 1);
	offset += 4;
      }
      
      offset = 0;
      for(i = 0; i < 3; i++) {
	int k;

	if(j == 0) {
	  x1 = (w - 30) + offset;
	  y1 = (2);
	  
	  x2 = (w - 30) - 7 + offset;
	  y2 = (2 + 8);
	  
	  x3 = (w - 30) + 7 + offset;
	  y3 = (2 + 8);
	}
	else {
	  x1 = (w - 30) + offset;
	  y1 = (2 + 8);
	  
	  x2 = (w - 30) - 6 + offset;
	  y2 = (3);
	  
	  x3 = (w - 30) + 6 + offset;
	  y3 = (3);
	}
	
	if(i == 2) {
	  x1++; x2++; x3++;
	  y1++; y2++; y3++;
	}
	
	points[0].x = x1;
	points[0].y = y1;
	points[1].x = x2;
	points[1].y = y2;
	points[2].x = x3;
	points[2].y = y3;
	points[3].x = x1;
	points[3].y = y1;
	
	offset += w;
	
	XLockDisplay(gGui->display);
	XSetForeground(gGui->display, image->image->gc, 
		       xitk_get_pixel_color_lightgray(gGui->imlib_data));
	XFillPolygon(gGui->display, image->image->pixmap, image->image->gc, 
		     &points[0], 4, Complex, CoordModeOrigin);
	XUnlockDisplay(gGui->display);
	
	XLockDisplay(gGui->display);
	for(k = 0; k < 3; k++) {
	  if(k == 0)
	    XSetForeground(gGui->display, image->image->gc, 
			   xitk_get_pixel_color_black(gGui->imlib_data));
	  else if(k == 1)
	    XSetForeground(gGui->display, image->image->gc, 
			   xitk_get_pixel_color_darkgray(gGui->imlib_data));
	  else
	    XSetForeground(gGui->display, image->image->gc, 
			   xitk_get_pixel_color_white(gGui->imlib_data));
	  
	  XDrawLine(gGui->display, image->image->pixmap, image->image->gc,
		    points[k].x, points[k].y, points[k+1].x, points[k+1].y);
	}
	XUnlockDisplay(gGui->display);
	
      }
    }

    xitk_image_change_image(gGui->imlib_data, fb->sort_skin_down,
			    dsimage, dsimage->width, dsimage->height);
    xitk_image_change_image(gGui->imlib_data, fb->sort_skin_down,
			    fsimage, fsimage->width, fsimage->height);
  }

  x--;
  y += xitk_get_widget_height(fb->files_browser) + 16 + 5;
  w = xitk_get_widget_width(fb->files_browser);

  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.parent_wlist      = fb->widget_list;
  cmb.entries           = fb->file_filters;
  cmb.parent_wkey       = &fb->widget_key;
  cmb.callback          = fb_select_file_filter;
  cmb.userdata          = (void *)fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->filters = 
	    xitk_noskin_combo_create(fb->widget_list, &cmb, x, y, w, NULL, NULL)));
  xitk_combo_set_select(fb->filters, fb->filter_selected);
  xitk_enable_and_show_widget(fb->filters);

  x = 15;

  cb.skin_element_name = NULL;
  cb.callback          = fb_hidden_files;
  cb.userdata          = (void *) fb;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(fb->widget_list)),
	    (fb->show_hidden = 
	     xitk_noskin_checkbox_create(fb->widget_list, &cb, x, y+5, 10, 10)));
  xitk_checkbox_set_state(fb->show_hidden, fb->show_hidden_files);
  xitk_enable_and_show_widget(fb->show_hidden);

  lbl.window            = xitk_window_get_window(fb->xwin);
  lbl.gc                = (XITK_WIDGET_LIST_GC(fb->widget_list));
  lbl.skin_element_name = NULL;
  lbl.label             = _("Show hidden file");
  lbl.callback          = fb_lbl_hidden_files;
  lbl.userdata          = (void *) fb;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (widget = 
	    xitk_noskin_label_create(fb->widget_list, &lbl, x + 15, y, w - 15, 20, fontname)));
  xitk_enable_and_show_widget(widget);
  

  y += 30;
  w = (WINDOW_WIDTH - (4 * 15)) / 3;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Rename");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_rename_file;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)),
	   (fb->rename = 
	    xitk_noskin_labelbutton_create(fb->widget_list, 
					   &lb, x, y, w, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fb->rename);

  x += w + 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Delete");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_delete_file; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->delete = 
	    xitk_noskin_labelbutton_create(fb->widget_list, 
					   &lb, x, y, w, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fb->delete);
  
  x += w + 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Create a directory");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_create_directory;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->create = 
	    xitk_noskin_labelbutton_create(fb->widget_list, 
					   &lb, x, y, w, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fb->create);

  fb->cb_buttons[0] = fb->cb_buttons[1] = NULL;

  if(fb->cbb[0].label) {
    x = 15;
    y += 30;
    
    lb.button_type       = CLICK_BUTTON;
    lb.label             = fb->cbb[0].label;
    lb.align             = ALIGN_CENTER;
    lb.callback          = fb_callback_button_cb;
    lb.state_callback    = NULL;
    lb.userdata          = (void *)fb;
    lb.skin_element_name = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)),
	     (fb->cb_buttons[0] = 
	      xitk_noskin_labelbutton_create(fb->widget_list, 
					     &lb, x, y, w, 23,
					     "Black", "Black", "White", btnfontname)));
    xitk_enable_and_show_widget(fb->cb_buttons[0]);

    if(fb->cbb[1].label) {
      x += w + 15;
      
      lb.button_type       = CLICK_BUTTON;
      lb.label             = fb->cbb[1].label;
      lb.align             = ALIGN_CENTER;
      lb.callback          = fb_callback_button_cb;
      lb.state_callback    = NULL;
      lb.userdata          = (void *)fb;
      lb.skin_element_name = NULL;
      xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	       (fb->cb_buttons[1] = 
		xitk_noskin_labelbutton_create(fb->widget_list, 
					       &lb, x, y, w, 23,
					       "Black", "Black", "White", btnfontname)));
      xitk_enable_and_show_widget(fb->cb_buttons[1]);
    }
  }

  x = WINDOW_WIDTH - w - 15;
  y = WINDOW_HEIGHT - 23 - 15;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = _fb_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(fb->widget_list)), 
	   (fb->close = xitk_noskin_labelbutton_create(fb->widget_list, 
						       &lb, x, y, w, 23,
						       "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(fb->close);

  xitk_window_change_background(gGui->imlib_data, fb->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);


  if(fb->cb_buttons[0])
    xitk_set_focus_to_widget(fb->cb_buttons[0]);

  {
    char buffer[256];
    sprintf(buffer, "filebrowser%d", (unsigned int) time(NULL));
    fb->widget_key = xitk_register_event_handler(buffer, 
						 (xitk_window_get_window(fb->xwin)),
						 fb_handle_events,
						 NULL,
						 NULL,
						 fb->widget_list,
						 (void *)fb);
  }

  fb->visible = 1;
  fb->running = 1;
  filebrowser_raise_window(fb);
    
  fb_getdir(fb);

  while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(fb->xwin)))
    xine_usec_sleep(5000);

  XLockDisplay(gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(fb->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay(gGui->display);

  return fb;
}
