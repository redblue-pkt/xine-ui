/*
 * Copyright (C) 2000-2019 the xine project
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
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"

#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       390
#define MAX_DISP_ENTRIES    10

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


typedef struct {
  const char                *name;
  const char                *ending;
} filebrowser_filter_t;

static const filebrowser_filter_t __fb_filters[] = {
  { N_("All files"), "*"                                                           },
  { N_("All known subtitles"), ".sub .srt .asc .smi .ssa .ass .txt "               },
  { N_("All known playlists"), ".pls .m3u .sfv .tox .asx .smil .smi .xml .fxd "    },
  /* media files */
  { "*.4xm", ".4xm "                                                               },
  { "*.ac3", ".ac3 "                                                               },
  { "*.aif, *.aiff", ".aif .aiff "                                                 },
  {"*.asf, *.wmv, *.wma, *.wvx, *.wax", ".asf .wmv .wma .wvx .wax "                },
  { "*.aud", ".aud "                                                               },
  { "*.avi", ".avi "                                                               },
  { "*.cin", ".cin "                                                               },
  { "*.cpk, *.cak, *.film", ".cpk .cak .film "                                     },
  { "*.dv, *.dif", ".dv .dif "                                                     },
  { "*.fli, *.flc", ".fli .flc "                                                   },
  { "*.mp3, *.mp2, *.mpa, *.mpega", ".mp3 .mp2 .mpa .mpega "                       },
  { "*.mjpg", ".mjpg "                                                             },
  { "*.mov, *.qt, *.mp4", ".mov .qt .mp4 "                                         },
  { "*.m2p", ".m2p "                                                               },
  { "*.mpg, *.mpeg", ".mpg .mpeg "                                                 },
  { "*.mpv", ".mpv "                                                               },
  { "*.mve, *.mv8", ".mve .mv8 "                                                   },
  { "*.nsf", ".nsf "                                                               },
  { "*.nsv", ".nsv "                                                               },
  { "*.ogg, *.ogm, *.spx", ".ogg .ogm .spx "                                       },
  { "*.pes", ".pes "                                                               },
  { "*.png, *.mng", ".png .mng "                                                   },
  { "*.pva", ".pva "                                                               },
  { "*.ra", ".ra "                                                                 },
  { "*.rm, *.ram, *.rmvb ", ".rm .ram .rmvb "                                      },
  { "*.roq", ".roq "                                                               },
  { "*.str, *.iki, *.ik2, *.dps, *.dat, *.xa1, *.xa2, *.xas, *.xap, *.xa",
    ".str .iki .ik2 .dps .dat .xa1 .xa2 .xas .xap .xa "                            },
  { "*.snd, *.au", ".snd .au "                                                     },
  { "*.ts, *.m2t, *.trp", ".ts .m2t .trp "                                         },
  { "*.vqa", ".vqa "                                                               },
  { "*.vob", ".vob "                                                               },
  { "*.voc", ".voc "                                                               },
  { "*.vox", ".vox "                                                               },
  { "*.wav", ".wav "                                                               },
  { "*.wve", ".wve "                                                               },
  { "*.y4m", ".y4m "                                                               },
  { NULL, NULL                                                                     }
};

typedef struct {
  char                  *name;
} fileinfo_t;

#define DEFAULT_SORT 0
#define REVERSE_SORT 1

#define NORMAL_CURS  0
#define WAIT_CURS    1

struct filebrowser_s {
  gGui_t                         *gui;

  xitk_window_t                  *xwin;
  
  xitk_widget_list_t             *widget_list;

  xitk_widget_t                  *origin;

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
  const char                    **file_filters;

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

  hidden_file_toggle_t            hidden_cb;

  filebrowser_callback_button_t   cbb[3];
  xitk_widget_t                  *cb_buttons[2];
  
  xitk_register_key_t             widget_key;
  xitk_register_key_t             dialog;
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
    if(state == WAIT_CURS)
      xitk_window_define_window_cursor (fb->xwin, xitk_cursor_watch);
    else
      xitk_window_restore_window_cursor (fb->xwin);
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

    xitk_window_destroy_window(fne->xwin);
    fne->xwin = NULL;
    /* xitk_dlist_init (&fne->widget_list->list); */

    free(fne);
  }
}
static void fne_apply_cb(xitk_widget_t *w, void *data) {
  filename_editor_t *fne = (filename_editor_t *) data;
  
  (void)w;
  if(fne->callback)
    fne->callback(NULL, (void *)fne->fb, (xitk_inputtext_get_text(fne->input)));

  fb_reactivate(fne->fb);
  fne_destroy(fne);
}

static void fne_cancel_cb(xitk_widget_t *w, void *data) {
  filename_editor_t *fne = (filename_editor_t *) data;

  (void)w;
  fb_reactivate(fne->fb);
  fne_destroy(fne);
}
static void fne_handle_event(XEvent *event, void *data) {
  switch(event->type) {
    
  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      fne_cancel_cb(NULL, data);
    else
      gui_handle_event(event, gGui);
    break;
  }
}

static void fb_create_input_window(char *title, char *text,
				   xitk_string_callback_t cb, filebrowser_t *fb) {
  filename_editor_t          *fne;
  int                         x, y, w;
  int                         width = WINDOW_WIDTH;
  int                         height = 102;
  xitk_labelbutton_widget_t   lb;
  xitk_inputtext_widget_t     inp;

  x = (xitk_get_display_width()  >> 1) - (width >> 1);
  y = (xitk_get_display_height() >> 1) - (height >> 1);

  fne = (filename_editor_t *) calloc(1, sizeof(filename_editor_t));
  
  fne->callback = cb;
  fne->fb = fb;

  fne->xwin = xitk_window_create_dialog_window (fb->gui->imlib_data, title, x, y, width, height);

  xitk_window_set_wm_window_type(fne->xwin, WINDOW_TYPE_NORMAL);
  xitk_window_set_window_class(fne->xwin, NULL, "xine");
  xitk_window_set_window_icon(fne->xwin, gGui->icon);

  fne->widget_list                = xitk_window_widget_list(fne->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&inp);

  x = 15;
  y = 30;
  w = width - 30;
  
  inp.skin_element_name = NULL;
  inp.text              = text;
  inp.max_length        = XITK_PATH_MAX + XITK_NAME_MAX + 1;
  inp.callback          = NULL;
  inp.userdata          = (void *)fne;
  fne->input = xitk_noskin_inputtext_create (fne->widget_list, &inp,
    x, y, w, 20, "Black", "Black", fontname);
  xitk_add_widget (fne->widget_list, fne->input);

  xitk_enable_and_show_widget(fne->input);

  y = height - (23 + 15);
  x = 15;
  w = 100;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_apply_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  fne->button_apply = xitk_noskin_labelbutton_create (fne->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fne->widget_list, fne->button_apply);
  xitk_enable_and_show_widget(fne->button_apply);

  x = width - (w + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Cancel");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_cancel_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  fne->button_cancel = xitk_noskin_labelbutton_create (fne->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fne->widget_list, fne->button_cancel);
  xitk_enable_and_show_widget(fne->button_cancel);

  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "filenameed%u", (unsigned int) time(NULL));
    
    fne->widget_key = xitk_register_event_handler(buffer,
                                                  fne->xwin,
						  fne_handle_event,
						  NULL,
						  NULL,
						  fne->widget_list,
						  (void *)fne);
  }

  xitk_window_show_window(fne->xwin, 1);

  video_window_set_transient_for (fb->gui->vwin, fne->xwin);
  layer_above_video(fne->xwin);

  xitk_window_try_to_set_input_focus(fne->xwin);
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

  if(!fb->filter_selected)
    return 1;
  else {
    char *ending;

    if((ending = strrchr(file, '.'))) {
      char  ext[strlen(ending) + 2];
      char *p;
      
      sprintf(ext, "%s ", ending);

      p = ext;
      while(*p) {
	*p = tolower(*p);
	p++;
      }
	
      if(strstr(__fb_filters[fb->filter_selected].ending, ext))
	return 1;
      
    }
  }
  return 0;
}

static void fb_update_origin(filebrowser_t *fb) {
  char   buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  
  if(strcmp(fb->current_dir, "/"))
    snprintf(buf, sizeof(buf), "%s/%s", fb->current_dir, fb->filename);
  else
    snprintf(buf, sizeof(buf), "/%s", fb->filename);
  
  xitk_inputtext_change_text(fb->origin, buf); 
}

static void fb_extract_path_and_file(filebrowser_t *fb, const char *filepathname) {
  if(filepathname && *filepathname) {
    char *dirname = NULL;
    char *filename = NULL;
    char  _filepathname[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    char *p;
    
    if((*filepathname == '~') && (*(filepathname + 1) == '/' || *(filepathname + 1) == '\0')) {
      const char *homedir = xine_get_homedir();
      
      snprintf(_filepathname, sizeof(_filepathname), "%s%s", homedir, (filepathname + 1));
    }
    else {
      if((*filepathname == '\\') && (*(filepathname + 1) == '~'))
	filepathname++;
      if(*filepathname == '/')
	strlcpy(_filepathname, filepathname, sizeof(_filepathname));
      else
	snprintf(_filepathname, sizeof(_filepathname), "%s/%s", fb->current_dir, filepathname);
    }

    p = _filepathname + strlen(_filepathname) - 1;
    while((p > _filepathname) && (*p == '/'))	/* Remove trailing '/' from path name */
      *p-- = '\0';

    if(is_a_dir(_filepathname)) {		/* Whole name is a dir name */
      dirname = _filepathname;
    }
    else {
      filename = strrchr(_filepathname, '/');
    
      if(!filename) {				/* Whole name treated as file name */
	filename = _filepathname;
      }
      else {					/* Split into dir and file part */
	*filename++ = '\0';

	if(*_filepathname == '\0')
	  dirname = "/";			/* Dir part was "/", restore it */
	else
	  dirname = _filepathname;

	p = dirname + strlen(dirname) - 1;
	while((p > dirname) && (*p == '/'))	/* Remove trailing '/' from dir name */
	  *p-- = '\0';

	if(!is_a_dir(dirname))			/* Invalid path, don't change anything */
	  return;
      }
    }

    if(dirname)
      strlcpy(fb->current_dir, dirname, sizeof(fb->current_dir));

    if(filename)
      strlcpy(fb->filename, filename, sizeof(fb->filename));
    else
      *fb->filename = '\0';
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
static int my_strverscmp(const char *s1, const char *s2) {
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
 * Wrapper to my_strverscmp() for qsort() calls, which sort mrl_t type array.
 */
static int _sortfiles_default(const fileinfo_t *s1, const fileinfo_t *s2) {
  return(my_strverscmp(s1->name, s2->name));
}
static int _sortfiles_reverse(const fileinfo_t *s1, const fileinfo_t *s2) {
  return(my_strverscmp(s2->name, s1->name));
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
    
    if(fb->files_sort_direction == REVERSE_SORT)
      func = _sortfiles_reverse;
    else /*if(fb->files_sort_direction == DEFAULT_SORT)*/
      func = _sortfiles_default;
    
    qsort(fb->norm_files, fb->files_num, sizeof(fileinfo_t), func);
    
    fb->files = (char **) realloc(fb->files, sizeof(char *) * (fb->files_num + 2));

    for(i = 0; i < fb->files_num; i++)
      fb->files[i] = strdup(fb->norm_files[i].name);

    fb->files[i] = NULL;
  }
  
  xitk_browser_update_list(fb->files_browser, (const char *const *)fb->files, NULL, fb->files_num, 0);
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
    
    if(fb->directories_sort_direction == REVERSE_SORT)
      func = _sortfiles_reverse;
    else /*if(fb->directories_sort_direction == DEFAULT_SORT)*/
      func = _sortfiles_default;
    
    qsort(fb->dir_files, fb->directories_num, sizeof(fileinfo_t), func);
    
    fb->directories = (char **) realloc(fb->directories, sizeof(char *) * (fb->directories_num + 2));
    for(i = 0; i < fb->directories_num; i++) {
      fb->directories[i] = xitk_asprintf("%s%c", fb->dir_files[i].name, '/');
    }

    fb->directories[i] = NULL;
  }

  xitk_browser_update_list(fb->directories_browser, 
			   (const char *const *)fb->directories, NULL, fb->directories_num, 0);
}

static void fb_getdir(filebrowser_t *fb) {
  char                  fullfilename[XINE_PATH_MAX + XINE_NAME_MAX + 2];
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
  
  /* Following code relies on the fact that fb->current_dir has no trailing '/' */
  if((pdir = opendir(fb->current_dir)) == NULL) {
    char *p = strrchr(fb->current_dir, '/');
    
    xine_error (fb->gui, _("Unable to open directory '%s': %s."),
	       (p && *(p + 1)) ? p + 1 : fb->current_dir, strerror(errno));
    
    /* One step back if dir has a subdir component */
    if(p && *(p + 1)) {
      if(p == fb->current_dir) /* we are in the root dir */
	*(p + 1) = '\0';
      else
	*p = '\0';

      fb_update_origin(fb);
      fb_getdir(fb);
    }
    return;
  }

  while((pdirent = readdir(pdir)) != NULL) {
    
    snprintf(fullfilename, sizeof(fullfilename), "%s/%s", fb->current_dir, pdirent->d_name);
    
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
static void fb_select(xitk_widget_t *w, void *data, int selected, int modifier) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  if(w == fb->files_browser) {
    strlcpy(fb->filename, fb->norm_files[selected].name, sizeof(fb->filename));
    fb_update_origin(fb);
  }
}

static void fb_callback_button_cb(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  (void)w;
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


static void fb_dbl_select(xitk_widget_t *w, void *data, int selected, int modifier) {
  filebrowser_t *fb = (filebrowser_t *) data;

  if(w == fb->directories_browser) {
    char buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    
    /* Want to re-read current dir */
    if(!strcasecmp(fb->dir_files[selected].name, ".")) {
      /* NOOP */
    }
    else if(!strcasecmp(fb->dir_files[selected].name, "..")) {
      char *p;
      
      strlcpy(buf, fb->current_dir, sizeof(buf));
      if(strlen(buf) > 1) { /* not '/' directory */
	
	p = &buf[strlen(buf)-1];
	while(*p && *p != '/') {
	  *p = '\0';
	  p--;
	}
	
	/* Remove last '/' if current_dir isn't root */
	if((strlen(buf) > 1) && *p == '/') 
	  *p = '\0';
	
	strlcpy(fb->current_dir, buf, sizeof(fb->current_dir));
      }
    }
    else {
      
      /* not '/' directory */
      if(strcasecmp(fb->current_dir, "/")) {
	snprintf(buf, sizeof(buf), "%s/%s", fb->current_dir, fb->dir_files[selected].name);
      }
      else {
	snprintf(buf, sizeof(buf), "/%s", fb->dir_files[selected].name);
      }
      
      if(is_a_dir(buf))
	strlcpy(fb->current_dir, buf, sizeof(fb->current_dir));

    }
    
    fb_update_origin(fb);
    fb_getdir(fb);
  }
  else if(w == fb->files_browser) {
    strlcpy(fb->filename, fb->norm_files[selected].name, sizeof(fb->filename));
    fb_callback_button_cb(fb->cb_buttons[0], (void *)data);
  }

}

static void fb_change_origin(xitk_widget_t *w, void *data, const char *currenttext) {
  filebrowser_t *fb = (filebrowser_t *)data;

  (void)w;
  fb_extract_path_and_file(fb, currenttext);
  fb_update_origin(fb);
  fb_getdir(fb);
}

static void fb_sort(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  if(w == fb->directories_sort) {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->directories_sort);
    
    fb->directories_sort_direction = (fb->directories_sort_direction == DEFAULT_SORT) ? 
      REVERSE_SORT : DEFAULT_SORT;
    
    xitk_hide_widget(fb->directories_sort);

    if(fb->directories_sort_direction == DEFAULT_SORT)
      xitk_image_change_image (fb->sort_skin_down, dsimage, dsimage->width, dsimage->height);
    else
      xitk_image_change_image (fb->sort_skin_up, dsimage, dsimage->width, dsimage->height);

    xitk_show_widget(fb->directories_sort);
    
    sort_directories(fb);
  }
  else if(w == fb->files_sort) {
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->files_sort);
    
    fb->files_sort_direction = (fb->files_sort_direction == DEFAULT_SORT) ? 
      REVERSE_SORT : DEFAULT_SORT;
    
    xitk_hide_widget(fb->files_sort);

    if(fb->files_sort_direction == DEFAULT_SORT)
      xitk_image_change_image (fb->sort_skin_down, fsimage, fsimage->width, fsimage->height);
    else
      xitk_image_change_image (fb->sort_skin_up, fsimage, fsimage->width, fsimage->height);

    xitk_show_widget(fb->files_sort);

    sort_files(fb);
  }    
}

static void fb_exit(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  if(fb) {
    int i;

    fb->running = 0;
    fb->visible = 0;

    if (fb->dialog)
      xitk_unregister_event_handler (&fb->dialog);
    
    xitk_unregister_event_handler(&fb->widget_key);

    xitk_window_destroy_window (fb->xwin);
    fb->xwin = NULL;
    /* xitk_dlist_init (&fb->widget_list->list); */

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

    free(fb->file_filters);
    
    SAFE_FREE(fb->cbb[0].label);
    SAFE_FREE(fb->cbb[1].label);
    
    xitk_image_free_image (&fb->sort_skin_up);
    xitk_image_free_image (&fb->sort_skin_down);

    free(fb);
    fb = NULL;
  }
}
static void _fb_exit(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  if(fb->cbb[2].callback)
    fb->cbb[2].callback(fb);
  fb_exit(NULL, (void *)fb);
}

static void _fb_delete_file_done (void *data, int state) {
  filebrowser_t *fb = data;

  if (state == 2) {
    char buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    int sel = xitk_browser_get_current_selected (fb->files_browser);

    snprintf (buf, sizeof(buf), "%s%s%s",
      fb->current_dir, ((fb->current_dir[0] && strcmp(fb->current_dir, "/")) ? "/" : ""),
      fb->norm_files[sel].name);
      
    if ((unlink (buf)) == -1)
      xine_error (fb->gui, _("Unable to delete file '%s': %s."), buf, strerror (errno));
    else
      fb_getdir (fb);
  }
  fb_reactivate (fb);
}

static void fb_delete_file (xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;
  
  (void)w;
  if ((sel = xitk_browser_get_current_selected (fb->files_browser)) >= 0) {
    char buf[256 + XITK_PATH_MAX + XITK_NAME_MAX + 2];

    snprintf(buf, sizeof(buf), _("Do you really want to delete the file '%s%s%s' ?"),
	     fb->current_dir, ((fb->current_dir[0] && strcmp(fb->current_dir, "/")) ? "/" : ""),
	     fb->norm_files[sel].name);
    
    fb_deactivate(fb);
    fb->dialog = xitk_window_dialog_3 (fb->gui->imlib_data,
      fb->xwin,
      get_layer_above_video (fb->gui), 400, _("Confirm deletion ?"), _fb_delete_file_done, fb,
      NULL, XITK_LABEL_YES, XITK_LABEL_NO, NULL, 0, ALIGN_DEFAULT, "%s", buf);
  }
}

static void fb_rename_file_cb(xitk_widget_t *w, void *data, const char *newname) {
  filebrowser_t *fb = (filebrowser_t *) data;
  char buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  int sel = xitk_browser_get_current_selected(fb->files_browser);

  (void)w;
  snprintf(buf, sizeof(buf), "%s%s%s",
	   fb->current_dir, ((fb->current_dir[0] && strcmp(fb->current_dir, "/")) ? "/" : ""),
	   fb->norm_files[sel].name);
      
  if((rename(buf, newname)) == -1)
    xine_error (fb->gui, _("Unable to rename file '%s' to '%s': %s."), buf, newname, strerror(errno));
  else
    fb_getdir(fb);

}
static void fb_rename_file(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;
  
  (void)w;
  if((sel = xitk_browser_get_current_selected(fb->files_browser)) >= 0) {
    char buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    
    snprintf(buf, sizeof(buf), "%s%s%s",
	     fb->current_dir, ((fb->current_dir[0] && strcmp(fb->current_dir, "/")) ? "/" : ""),
	     fb->norm_files[sel].name);
      
    fb_deactivate(fb);
    fb_create_input_window(_("Rename file"), buf, fb_rename_file_cb, fb);
  }
}

static void fb_create_directory_cb(xitk_widget_t *w, void *data, const char *newdir) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  (void)w;
  if(!mkdir_safe(newdir))
    xine_error (fb->gui, _("Unable to create the directory '%s': %s."), newdir, strerror(errno));
  else
    fb_getdir(fb);
}
static void fb_create_directory(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  char           buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  
  (void)w;
  snprintf(buf, sizeof(buf), "%s%s",
	   fb->current_dir, ((fb->current_dir[0] && strcmp(fb->current_dir, "/")) ? "/" : ""));
      
  fb_deactivate(fb);
  fb_create_input_window(_("Create a new directory"), buf, fb_create_directory_cb, fb);
}

static void fb_select_file_filter(xitk_widget_t *w, void *data, int selected) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  (void)w;
  fb->filter_selected = selected;
  fb_getdir(fb);
}

static void fb_hidden_files(xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  (void)w;
  fb->show_hidden_files = state;
  fb->hidden_cb(1, state);
  fb_getdir(fb);
}
static void fb_lbl_hidden_files(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  (void)w;
  xitk_checkbox_set_state(fb->show_hidden, (!xitk_checkbox_get_state(fb->show_hidden)));
  xitk_checkbox_callback_exec(fb->show_hidden);
}

static void fb_handle_events(XEvent *event, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;
  
  switch(event->type) {

  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape) {
      if(xitk_is_widget_enabled(fb->close)) /* Exit only if close button would exit */
	_fb_exit(NULL, data);
    }
    else {
      int sel = xitk_browser_get_current_selected(fb->files_browser);
      if(sel >= 0) {
        int modifier = 0;
        xitk_get_key_modifier(event, &modifier);
        fb_select(fb->files_browser, (void *) fb, sel, modifier);
      } else {
	gui_handle_event(event, data);
      }
    }
    break;
  }
}

void filebrowser_raise_window(filebrowser_t *fb) {
  if(fb != NULL)
    raise_window(fb->xwin, fb->visible, fb->running);
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
    files = (char **) calloc((fb->files_num + 2), sizeof(char *));
    
    for(i = 0; i < fb->files_num; i++)
      files[i] = strdup(fb->norm_files[i].name);
    files[i] = NULL;
  }
  
  return files;
}

filebrowser_t *create_filebrowser(char *window_title, char *filepathname, hidden_file_toggle_t hidden_cb,
				  filebrowser_callback_button_t *cbb1,
				  filebrowser_callback_button_t *cbb2,
				  filebrowser_callback_button_t *cbb_close) {
  filebrowser_t              *fb;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  xitk_browser_widget_t       br;
  xitk_inputtext_widget_t     inp;
  xitk_button_widget_t        b;
  xitk_combo_widget_t         cmb;
  xitk_widget_t              *widget;
  int                         i, x, y, w;

  fb = (filebrowser_t *) calloc(1, sizeof(filebrowser_t));
  if (!fb)
    return NULL;

  fb->gui = gGui;
  
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

  x = (xitk_get_display_width()  >> 1) - (WINDOW_WIDTH >> 1);
  y = (xitk_get_display_height() >> 1) - (WINDOW_HEIGHT >> 1);

  /* Create window */
  fb->xwin = xitk_window_create_dialog_window (fb->gui->imlib_data, 
					      (window_title) ? window_title : _("File Browser"), 
					      x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_window_set_wm_window_type(fb->xwin, WINDOW_TYPE_NORMAL);
  xitk_window_set_window_class(fb->xwin, NULL, "xine");
  xitk_window_set_window_icon(fb->xwin, gGui->icon);

  fb->directories                = NULL;
  fb->directories_num            = 0;
  fb->files                      = NULL;
  fb->files_num                  = 0;
  fb->directories_sort_direction = DEFAULT_SORT;
  fb->files_sort_direction       = DEFAULT_SORT;
  fb->hidden_cb                  = hidden_cb;
  fb->show_hidden_files          = hidden_cb(0, 0);

  strlcpy(fb->current_dir, xine_get_homedir(), sizeof(fb->current_dir));
  memset(&fb->filename, 0, sizeof(fb->filename));
  fb_extract_path_and_file(fb, filepathname);

  fb->norm_files = (fileinfo_t *) calloc(MAXFILES, sizeof(fileinfo_t));
  fb->dir_files = (fileinfo_t *) calloc(MAXFILES, sizeof(fileinfo_t));

  fb->files = (char **) calloc(2, sizeof(char *));
  fb->directories = (char **) calloc(2, sizeof(char *));
  
  fb->file_filters = (const char **) malloc(sizeof(filebrowser_filter_t) * ((sizeof(__fb_filters) / sizeof(__fb_filters[0])) + 1));
  
  for(i = 0; __fb_filters[i].ending; i++)
    fb->file_filters[i] = _(__fb_filters[i].name);

  fb->file_filters[i] = NULL;
  fb->filter_selected = 0;

  fb->widget_list = xitk_window_widget_list(fb->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&lbl);
  XITK_WIDGET_INIT(&cb);
  XITK_WIDGET_INIT(&br);
  XITK_WIDGET_INIT(&inp);
  XITK_WIDGET_INIT(&cmb);
  XITK_WIDGET_INIT(&b);

  bg = xitk_window_get_background_pixmap(fb->xwin);

  x = 15;
  y = 30;
  w = WINDOW_WIDTH - 30;

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = XITK_PATH_MAX + XITK_NAME_MAX + 1;
  inp.callback          = fb_change_origin;
  inp.userdata          = (void *)fb;
  fb->origin = xitk_noskin_inputtext_create (fb->widget_list, &inp,
    x, y, w, 20, "Black", "Black", fontname);
  xitk_add_widget (fb->widget_list, fb->origin);
  xitk_enable_and_show_widget(fb->origin);

  fb_update_origin(fb);

  y += 45;
  w = (WINDOW_WIDTH - 30 - 10) / 2;

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = fb->directories_num;
  br.browser.entries               = (const char *const *)fb->directories;
  br.callback                      = fb_select;
  br.dbl_click_callback            = fb_dbl_select;
  br.userdata                      = (void *)fb;
  fb->directories_browser = xitk_noskin_browser_create (fb->widget_list, &br,
    (XITK_WIDGET_LIST_GC(fb->widget_list)), x + 2, y + 2, w - 4 - 12, 20, 12, fontname);
  xitk_add_widget (fb->widget_list, fb->directories_browser);
  xitk_enable_and_show_widget(fb->directories_browser);
  
  draw_rectangular_inner_box (bg, x, y,
			     w - 1, xitk_get_widget_height(fb->directories_browser) + 4 - 1);
  
  y -= 15;
  
  b.skin_element_name = NULL;
  b.callback          = fb_sort;
  b.userdata          = (void *)fb;
  fb->directories_sort = xitk_noskin_button_create (fb->widget_list, &b, x, y, w, 15);
  xitk_add_widget (fb->widget_list, fb->directories_sort);
  xitk_enable_and_show_widget(fb->directories_sort);

  x = WINDOW_WIDTH - (w + 15);
  y += 15;

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = fb->files_num;
  br.browser.entries               = (const char *const *)fb->files;
  br.callback                      = fb_select;
  br.dbl_click_callback            = fb_dbl_select;
  br.userdata                      = (void *)fb;
  fb->files_browser = xitk_noskin_browser_create (fb->widget_list, &br,
    (XITK_WIDGET_LIST_GC(fb->widget_list)), x + 2, y + 2, w - 4 - 12, 20, 12, fontname);
  xitk_add_widget (fb->widget_list, fb->files_browser);
  xitk_enable_and_show_widget(fb->files_browser);

  draw_rectangular_inner_box (bg, x, y,
			     w - 1, xitk_get_widget_height(fb->files_browser) + 4 - 1);

  y -= 15;

  b.skin_element_name = NULL;
  b.callback          = fb_sort;
  b.userdata          = (void *)fb;
  fb->files_sort = xitk_noskin_button_create (fb->widget_list, &b, x, y, w, 15);
  xitk_add_widget (fb->widget_list, fb->files_sort);
  xitk_enable_and_show_widget(fb->files_sort);


  {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->directories_sort);
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->files_sort);
    xitk_image_t *image;
    XPoint        points[4];
    int           i, j, w, offset;
    short         x1, x2, x3;
    short         y1, y2, y3;

    fb->sort_skin_up = xitk_image_create_image (fb->gui->imlib_data, 
					       dsimage->width, dsimage->height);
    fb->sort_skin_down = xitk_image_create_image (fb->gui->imlib_data, 
						 dsimage->width, dsimage->height);
    
    draw_bevel_three_state (fb->sort_skin_up);
    draw_bevel_three_state (fb->sort_skin_down);

    w = dsimage->width / 3;

    for(j = 0; j < 2; j++) {
      if(j == 0)
	image = fb->sort_skin_up;
      else
	image = fb->sort_skin_down;
      
      offset = 0;
      for(i = 0; i < 2; i++) {
        draw_rectangular_outter_box (image->image,
				    5, 4 + offset, w - 45, 1);
        draw_rectangular_outter_box (image->image,
				    w - 20, 4 + offset, 10, 1);
        draw_rectangular_outter_box (image->image,
				    w + 5, 4 + offset, w - 45, 1);
        draw_rectangular_outter_box (image->image,
				    (w * 2) - 20, 4 + offset, 10, 1);
        draw_rectangular_outter_box (image->image,
				    (w * 2) + 5 + 1, 4 + 1 + offset, w - 45, 1);
        draw_rectangular_outter_box (image->image,
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
	
        pixmap_fill_polygon (image->image, &points[0], 4,
          xitk_get_pixel_color_lightgray (fb->gui->imlib_data));

	for(k = 0; k < 3; k++) {
          int color;
          if (k == 0)
            color = xitk_get_pixel_color_black (fb->gui->imlib_data);
	  else if(k == 1)
            color = xitk_get_pixel_color_darkgray (fb->gui->imlib_data);
	  else
            color = xitk_get_pixel_color_white (fb->gui->imlib_data);
  
          pixmap_draw_line(image->image,
                           points[k].x, points[k].y, points[k+1].x, points[k+1].y,
                           color);
	}
      }
    }

    xitk_image_change_image (fb->sort_skin_down, dsimage, dsimage->width, dsimage->height);
    xitk_image_change_image (fb->sort_skin_down, fsimage, fsimage->width, fsimage->height);
  }

  y += xitk_get_widget_height(fb->files_browser) + 15 + 4 + 5;

  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.entries           = fb->file_filters;
  cmb.parent_wkey       = &fb->widget_key;
  cmb.callback          = fb_select_file_filter;
  cmb.userdata          = (void *)fb;
  fb->filters = xitk_noskin_combo_create (fb->widget_list, &cmb, x, y, w, NULL, NULL);
  xitk_add_widget (fb->widget_list, fb->filters);
  xitk_combo_set_select(fb->filters, fb->filter_selected);
  xitk_enable_and_show_widget(fb->filters);

  x = 15;

  cb.skin_element_name = NULL;
  cb.callback          = fb_hidden_files;
  cb.userdata          = (void *) fb;
  fb->show_hidden = xitk_noskin_checkbox_create (fb->widget_list, &cb, x, y+5, 10, 10);
  xitk_add_widget (fb->widget_list, fb->show_hidden);
  xitk_checkbox_set_state(fb->show_hidden, fb->show_hidden_files);
  xitk_enable_and_show_widget(fb->show_hidden);

  lbl.skin_element_name = NULL;
  lbl.label             = _("Show hidden file");
  lbl.callback          = fb_lbl_hidden_files;
  lbl.userdata          = (void *) fb;
  widget = xitk_noskin_label_create (fb->widget_list, &lbl, x + 15, y, w - 15, 20, fontname);
  xitk_add_widget (fb->widget_list, widget);
  xitk_enable_and_show_widget(widget);
  
  y = WINDOW_HEIGHT - (23 + 15) - (23 + 8);
  w = (WINDOW_WIDTH - (4 * 15)) / 3;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Rename");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_rename_file;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  fb->rename = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fb->widget_list, fb->rename);
  xitk_enable_and_show_widget(fb->rename);

  x = (WINDOW_WIDTH - w) / 2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Delete");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_delete_file; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  fb->delete = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fb->widget_list, fb->delete);
  xitk_enable_and_show_widget(fb->delete);
  
  x = WINDOW_WIDTH - (w + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Create a directory");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fb_create_directory;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  fb->create = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fb->widget_list, fb->create);
  xitk_enable_and_show_widget(fb->create);

  fb->cb_buttons[0] = fb->cb_buttons[1] = NULL;

  y = WINDOW_HEIGHT - (23 + 15);

  if(fb->cbb[0].label) {
    x = 15;
    
    lb.button_type       = CLICK_BUTTON;
    lb.label             = fb->cbb[0].label;
    lb.align             = ALIGN_CENTER;
    lb.callback          = fb_callback_button_cb;
    lb.state_callback    = NULL;
    lb.userdata          = (void *)fb;
    lb.skin_element_name = NULL;
    fb->cb_buttons[0] = xitk_noskin_labelbutton_create (fb->widget_list,
      &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (fb->widget_list, fb->cb_buttons[0]);

    xitk_enable_and_show_widget(fb->cb_buttons[0]);

    if(fb->cbb[1].label) {
      x = (WINDOW_WIDTH - w) / 2;
      
      lb.button_type       = CLICK_BUTTON;
      lb.label             = fb->cbb[1].label;
      lb.align             = ALIGN_CENTER;
      lb.callback          = fb_callback_button_cb;
      lb.state_callback    = NULL;
      lb.userdata          = (void *)fb;
      lb.skin_element_name = NULL;
      fb->cb_buttons[1] = xitk_noskin_labelbutton_create (fb->widget_list,
        &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
      xitk_add_widget (fb->widget_list, fb->cb_buttons[1]);
      xitk_enable_and_show_widget(fb->cb_buttons[1]);
    }
  }

  x = WINDOW_WIDTH - (w + 15);
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = _fb_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;
  fb->close =  xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (fb->widget_list, fb->close);
  xitk_enable_and_show_widget(fb->close);

  xitk_window_set_background (fb->xwin, bg);

  if(fb->cb_buttons[0])
    xitk_set_focus_to_widget(fb->cb_buttons[0]);

  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "filebrowser%u", (unsigned int) time(NULL));
    fb->widget_key = xitk_register_event_handler(buffer,
                                                 fb->xwin,
						 fb_handle_events,
						 NULL,
						 NULL,
						 fb->widget_list,
						 (void *)fb);
  }
  fb->dialog = 0;

  fb->visible = 1;
  fb->running = 1;
  filebrowser_raise_window(fb);
    
  fb_getdir(fb);

  layer_above_video(fb->xwin);
  xitk_window_try_to_set_input_focus(fb->xwin);

  return fb;
}
