/* 
 * Copyright (C) 2000-2002 the xine project
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
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>       
#include <unistd.h>                                                      
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "filebrowser.h"
#include "browser.h"
#include "label.h"
#include "button.h"
#include "labelbutton.h"
#include "dnd.h"
#include "window.h"
#include "widget.h"
#include "widget_types.h"

#include "_xitk.h"

extern int errno;

#define MAX_LIST       9 /* Max entries in browser widget */
#define MOVEUP         1
#define MOVEDN         2

/* ******************************************************* 
 *               File Browser decl.
 */
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

#ifndef S_ISDIR
#warning "You will experienced some problems in file browser with directory selection."
#define S_ISDIR(mode) 0
#endif

typedef struct {
  char                  *name;
  char                  *linkname;
  int                    marker;
} finfo_t;


/*
 * Return window id of widget.
 */
Window xitk_filebrowser_get_window_id(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;
  
  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;
    return private_data->window;
  }
  
  return None;
}

/*
 * Fill window information struct of given widget.
 */
int xitk_filebrowser_get_window_info(xitk_widget_t *w, window_info_t *inf) {
  filebrowser_private_data_t *private_data;
  
  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;
    return((xitk_get_window_info(private_data->widget_key, inf))); 
  }

  return 0;
}

/*
 * Boolean about running state.
 */
int xitk_filebrowser_is_running(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;
 
  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;
    return (private_data->running);
  }

  return 0;
}

/*
 * Boolean about visible state.
 */
int xitk_filebrowser_is_visible(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;
    return (private_data->visible);
  }

  return 0;
}

/*
 * Hide filebrowser.
 */
void xitk_filebrowser_hide(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;

    if(private_data->visible) {
      XLOCK(private_data->imlibdata->x.disp);
      XUnmapWindow(private_data->imlibdata->x.disp, private_data->window);
      XUNLOCK(private_data->imlibdata->x.disp);
      xitk_hide_widgets(private_data->widget_list);
      private_data->visible = 0;
    }
  }
}

/*
 * Show filebrowser.
 */
void xitk_filebrowser_show(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;

    xitk_show_widgets(private_data->widget_list);
    XLOCK(private_data->imlibdata->x.disp);
    XMapRaised(private_data->imlibdata->x.disp, private_data->window); 
    XUNLOCK(private_data->imlibdata->x.disp);
    private_data->visible = 1;
  }
}

/*
 * Set filebrowser transient for hints for given window.
 */
void xitk_filebrowser_set_transient(xitk_widget_t *w, Window window) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER) && (window != None)) {
    private_data = (filebrowser_private_data_t *)w->private_data;

    if(private_data->visible) {
      XLOCK(private_data->imlibdata->x.disp);
      XSetTransientForHint (private_data->imlibdata->x.disp,
			    private_data->window, 
			    window);
      XUNLOCK(private_data->imlibdata->x.disp);
    }
  }
}

/*
 * Destroy a filebrowser.
 */
void xitk_filebrowser_destroy(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;

    private_data->running = 0;
    private_data->visible = 0;
    
    xitk_unregister_event_handler(&private_data->widget_key);
    
    XLOCK(private_data->imlibdata->x.disp);
    XUnmapWindow(private_data->imlibdata->x.disp, private_data->window);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    xitk_destroy_widgets(private_data->widget_list);
    
    XLOCK(private_data->imlibdata->x.disp);
    XDestroyWindow(private_data->imlibdata->x.disp, private_data->window);
    Imlib_destroy_image(private_data->imlibdata, private_data->bg_image);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    private_data->window = None;
    xitk_list_free(private_data->widget_list->l);
    
    XLOCK(private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, private_data->widget_list->gc);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    XITK_FREE(private_data->widget_list);
    
    {
      int i;
      for(i = 0; i < private_data->dir_contents_num; i++) {
	XITK_FREE(private_data->fc->dir_contents[i]);
	XITK_FREE(private_data->fc->dir_disp_contents[i]);
      }
    }
    XITK_FREE(private_data->fc);
    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data);
  }
}

/*
 * Return the current directory location.
 */
char *xitk_filebrowser_get_current_dir(xitk_widget_t *w) {
  filebrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;

    return private_data->current_dir;
  }

  return NULL;
}

/*
 * Redisplay the displayed current directory.
 */
static void update_current_dir(filebrowser_private_data_t *private_data) {

  xitk_label_change_label (private_data->widget_list, 
			   private_data->widget_current_dir, 
			   private_data->current_dir);
  
}

/*
 * Return 1 is filepathname is a directory, otherwise 0
 */
static int is_a_dir(char *filepathname) {
  struct stat  pstat;
  
  stat(filepathname, &pstat);

  return (S_ISDIR(pstat.st_mode));
}

/*
 * Return the *marker* of the giver file *fully named*
 */
static int get_file_marker(char *filepathname) {
  struct stat  pstat;
  int          mode;
  
  lstat(filepathname, &pstat);
  mode = pstat.st_mode;

  if(S_ISDIR(mode))
    return '/';
  else if(S_ISLNK(mode))
    return '@';
  else if(S_ISFIFO(mode))
    return '|';
  else if(S_ISSOCK(mode))
    return '=';
  else 
    if(mode & S_IXUGO)
      return '*';

  return '\0';
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
 * Wrappers to strverscmp() for qsort() calls
 */
static int _sortfiles_default(const finfo_t *s1, const finfo_t *s2) {
  return(strverscmp(s1->name, s2->name));
}
static int _sortfiles_reverse(const finfo_t *s1, const finfo_t *s2) {
  return(strverscmp(s2->name, s1->name));
}

/*
 * Function fill the dir_contents array entries with found files.
 */
static void load_files(xitk_widget_t *w, void *data) {
  filebrowser_private_data_t *private_data = (filebrowser_private_data_t *)data;
  DIR             *pdir;
  struct dirent   *pdirent;
  finfo_t         *hide_files, *dir_files, *norm_files;
  char             fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  int              num_hide_files  = 0;
  int              num_dir_files   = 0;
  int              num_norm_files  = 0;
  int              num_files       = -1;
  int            (*func) ()        = _sortfiles_default;

  if((pdir = opendir(private_data->current_dir)) == NULL)
    return;

  hide_files = (finfo_t *) xitk_xmalloc(sizeof(finfo_t) * MAXFILES);
  norm_files = (finfo_t *) xitk_xmalloc(sizeof(finfo_t) * MAXFILES);
  dir_files = (finfo_t *) xitk_xmalloc(sizeof(finfo_t) * MAXFILES);
  
  while((pdirent = readdir(pdir)) != NULL) {

    /* 
     * Is it a directory ?
     * ... yes, it's pretty ugly, but i don't want to lstat()
     * here ;>
     */
    memset(&fullfilename, 0, strlen(fullfilename));
    sprintf(fullfilename, "%s/%s", private_data->current_dir,
	    pdirent->d_name);
    
    if(is_a_dir(fullfilename)) {
      dir_files[num_dir_files].name = (char *) 
	xitk_xmalloc(strlen(pdirent->d_name) + 1);
      strcpy(dir_files[num_dir_files].name, pdirent->d_name);
      dir_files[num_dir_files].marker = get_file_marker(fullfilename);
      dir_files[num_dir_files].linkname = NULL;
      
      /* The file is a link, follow it */
      if(dir_files[num_dir_files].marker == '@') {
	char linkbuf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
	int linksize;
	
	memset(&linkbuf, 0, XITK_PATH_MAX + XITK_NAME_MAX);
	linksize = readlink(fullfilename, linkbuf, XITK_PATH_MAX + XITK_NAME_MAX);
	
	if(linksize < 0) {
	  XITK_WARNING("%s(%d): readlink() failed: %s\n", __FUNCTION__, __LINE__, strerror(errno));
	}
	else {
	  dir_files[num_dir_files].linkname = (char *) 
	    xitk_xmalloc(linksize + 1);
	  
	  strncpy(dir_files[num_dir_files].linkname, linkbuf, linksize);
	}
      }
      
      num_dir_files++;
    } /* Hmmmm, an hidden file ? */
    else if(pdirent->d_name[0] == '.') {
      hide_files[num_hide_files].name = (char *) 
	xitk_xmalloc(strlen(pdirent->d_name) + 1);
      strcpy(hide_files[num_hide_files].name, pdirent->d_name);
      hide_files[num_hide_files].marker = get_file_marker(fullfilename);
      hide_files[num_hide_files].linkname = NULL;
      
      /* The file is a link, follow it */
      if(hide_files[num_hide_files].marker == '@') {
	char linkbuf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
	int linksize;
	
	memset(&linkbuf, 0, XITK_PATH_MAX + XITK_NAME_MAX);
	linksize = readlink(fullfilename, linkbuf, XITK_PATH_MAX + XITK_NAME_MAX);
	
	if(linksize < 0) {
	  XITK_WARNING("%s(%d): readlink() failed: %s\n", __FUNCTION__, __LINE__, strerror(errno));
	}
	else {
	  hide_files[num_hide_files].linkname = (char *) 
	    xitk_xmalloc(linksize + 1);
	  strncpy(hide_files[num_hide_files].linkname, linkbuf, linksize);
	}
      }

      num_hide_files++;
    } /* So a *normal* one. */
    else {
      norm_files[num_norm_files].name = (char *) 
	xitk_xmalloc(strlen(pdirent->d_name) + 1);
      strcpy(norm_files[num_norm_files].name, pdirent->d_name);
      norm_files[num_norm_files].marker = get_file_marker(fullfilename);
      norm_files[num_norm_files].linkname = NULL;
      
      /* The file is a link, follow it */
      if(norm_files[num_norm_files].marker == '@') {
	char linkbuf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
	int linksize;
	
	memset(&linkbuf, 0, XITK_PATH_MAX + XITK_NAME_MAX);
	linksize = readlink(fullfilename, linkbuf, XITK_PATH_MAX + XITK_NAME_MAX);
	
	if(linksize < 0) {
	  XITK_WARNING("%s(%d): readlink() failed: %s\n", __FUNCTION__, __LINE__, strerror(errno));
	}
	else {
	  norm_files[num_norm_files].linkname = (char *) 
	    xitk_xmalloc(linksize + 1);
	  strncpy(norm_files[num_norm_files].linkname, linkbuf, linksize);
	}
      }
      
      num_norm_files++;
    }
    
    num_files++;
  }
  
  closedir(pdir);

  if(num_files > 0) {
    int sorder[2][3] = {
      {1, 2, 3},
      {3, 2, 1}
    };
    int i, j;
    
    private_data->dir_contents_num = num_files+1;
    num_files = 0;
    
    /*
     * Sort arrays
     */
    if(private_data->fc->sort_order == DEFAULT_SORT)
      func = _sortfiles_default;
    else if(private_data->fc->sort_order == REVERSE_SORT)
      func = _sortfiles_reverse;

    if(num_dir_files)
      qsort(dir_files, num_dir_files, sizeof(finfo_t), func);
    
    if(num_hide_files)
      qsort(hide_files, num_hide_files, sizeof(finfo_t), func);
    
    if(num_norm_files)
      qsort(norm_files, num_norm_files, sizeof(finfo_t), func);
    
    for(j = 0; j < 3; j++) {
      
      /*
       * Add directories entries
       */
      if(sorder[private_data->fc->sort_order][j] == 1) {

	for(i = 0; i < num_dir_files; i++) {
	  
	  if(private_data->fc->dir_contents[num_files] != NULL) {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      realloc(private_data->fc->dir_contents[num_files], 
		      strlen(dir_files[i].name) + 1);
	  }
	  else {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      xitk_xmalloc(strlen(dir_files[i].name) + 1);
	  }
	  
	  /* It's a link file, display the source */
	  if(dir_files[i].linkname != NULL) {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
			strlen(dir_files[i].name) + 4 +
			strlen(dir_files[i].linkname) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(dir_files[i].name) + 4 +
			strlen(dir_files[i].linkname) + 2);
	    }
	    
	    sprintf(private_data->fc->dir_disp_contents[num_files], 
		    "%s%c -> %s", 
		    dir_files[i].name, dir_files[i].marker,
		    dir_files[i].linkname);
	  }
	  else {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
			strlen(dir_files[i].name) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(dir_files[i].name) + 2);
	    }
	    
	    sprintf(private_data->fc->dir_disp_contents[num_files], "%s%c", 
		    dir_files[i].name, dir_files[i].marker);
	  }

	  strcpy(private_data->fc->dir_contents[num_files], dir_files[i].name);
	  num_files++;
	}
      }

      /*
       * Add hidden files entries
       */
      if(sorder[private_data->fc->sort_order][j] == 2) {

	for(i = 0; i < num_hide_files; i++) {
	  
	  if(private_data->fc->dir_contents[num_files] != NULL) {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      realloc(private_data->fc->dir_contents[num_files], 
		      strlen(hide_files[i].name) + 1);
	  }
	  else {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      xitk_xmalloc(strlen(hide_files[i].name) + 1);
	  }
	  
	  /* It's a link file, display the source */
	  if(hide_files[i].linkname != NULL) {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
			strlen(hide_files[i].name) + 4 +
			strlen(hide_files[i].linkname) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(hide_files[i].name) + 4 +
			strlen(hide_files[i].linkname) + 2);
	    }
	    
	    sprintf(private_data->fc->dir_disp_contents[num_files], 
		    "%s%c -> %s", 
		    hide_files[i].name, hide_files[i].marker,
		    hide_files[i].linkname);
	  }
	  else {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
			strlen(hide_files[i].name) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(hide_files[i].name) + 2);
	    }
	  
	    sprintf(private_data->fc->dir_disp_contents[num_files], "%s%c",
		    hide_files[i].name, hide_files[i].marker);
	  }
	  
	  strcpy(private_data->fc->dir_contents[num_files], 
		 hide_files[i].name);
	  num_files++;
	}
      }

      /* 
       * Add other files entries
       */
      if(sorder[private_data->fc->sort_order][j] == 3) {

	for(i = 0; i < num_norm_files; i++) {
	  
	  if(private_data->fc->dir_contents[num_files] != NULL) {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      realloc(private_data->fc->dir_contents[num_files], 
		      strlen(norm_files[i].name) + 1);
	  }
	  else {
	    private_data->fc->dir_contents[num_files] = (char *) 
	      xitk_xmalloc(strlen(norm_files[i].name) + 1);
	  }
	  
	  /* It's a link file, display the source */
	  if(norm_files[i].linkname != NULL) {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
			strlen(norm_files[i].name) + 4 +
			strlen(norm_files[i].linkname) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(norm_files[i].name) + 4 +
			strlen(norm_files[i].linkname) + 2);
	    }
	    
	    sprintf(private_data->fc->dir_disp_contents[num_files], 
		    "%s%c -> %s", 
		    norm_files[i].name, norm_files[i].marker,
		    norm_files[i].linkname);
	  }
	  else {
	    if(private_data->fc->dir_disp_contents[num_files] != NULL) {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		realloc(private_data->fc->dir_disp_contents[num_files], 
		      strlen(norm_files[i].name) + 2);
	    }
	    else {
	      private_data->fc->dir_disp_contents[num_files] = (char *) 
		xitk_xmalloc(strlen(norm_files[i].name) + 2);
	    }
	  
	    sprintf(private_data->fc->dir_disp_contents[num_files], "%s%c",
		    norm_files[i].name, norm_files[i].marker);
	  }
	  
	  strcpy(private_data->fc->dir_contents[num_files], 
		 norm_files[i].name);
	  num_files++;
	}
      }
    }

    /* Some cleanups before leaving */
    for(i = 0; i < num_dir_files; i++) {
      XITK_FREE(dir_files[i].name);
      XITK_FREE(dir_files[i].linkname);
    }
    XITK_FREE(dir_files);
    
    for(i = 0; i < num_hide_files; i++){
      XITK_FREE(hide_files[i].name);
      XITK_FREE(hide_files[i].linkname);
    }
    XITK_FREE(hide_files);
    
    for(i = 0; i < num_norm_files; i++) {
      XITK_FREE(norm_files[i].name);
      XITK_FREE(norm_files[i].linkname);
    }
    XITK_FREE(norm_files);

  }
  else 
    private_data->dir_contents_num = 0;
  
  xitk_browser_update_list(private_data->fb_list, 
			   private_data->fc->dir_disp_contents, 
			   private_data->dir_contents_num, 0);
  
  update_current_dir(private_data);
}

/*
 * Leaving add files dialog
 */
void xitk_filebrowser_exit(xitk_widget_t *w, void *data) {
  filebrowser_private_data_t *private_data = ((xitk_widget_t *)data)->private_data;
  
  if(private_data->kill_callback)
    private_data->kill_callback(private_data->fbWidget, NULL);

  xitk_filebrowser_destroy(private_data->fbWidget);
}

/*
 * Change sort order of file list
 */
static void filebrowser_sortfiles(xitk_widget_t *w, void *data) {
  filebrowser_private_data_t *private_data = 
    ((xitk_widget_t*)((sort_param_t *)data)->w)->private_data;
  int sort_order = ((sort_param_t*)data)->sort;

  switch(sort_order) {
    
  case DEFAULT_SORT:
    if(private_data->fc->sort_order != DEFAULT_SORT) {
      private_data->fc->sort_order = DEFAULT_SORT;
      load_files(NULL, (void *)private_data);
    }
   break;
   
  case REVERSE_SORT:
    if(private_data->fc->sort_order != REVERSE_SORT) {
      private_data->fc->sort_order = REVERSE_SORT;
      load_files(NULL, (void *)private_data);
    }
    break;
    
  }
}

/*
 * Set current directory to home directory then read the directory content.
 */
static void filebrowser_homedir(xitk_widget_t *w, void *data) {
  filebrowser_private_data_t *private_data = (filebrowser_private_data_t *) data;
  
  sprintf(private_data->current_dir, "%s", xitk_get_homedir());
  load_files(NULL, (void *)private_data);
  xitk_paint_widget_list (private_data->widget_list);
}

/*
 *
 */
static void filebrowser_select_entry(filebrowser_private_data_t *private_data, int j) {
  char buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];

  /* Want to re-read current dir */
  if(!strcasecmp(private_data->fc->dir_contents[j], ".")) {
    /* NOOP */
  }
  else if(!strcasecmp(private_data->fc->dir_contents[j], "..")) {
    char *p;
    
    memset(&buf, '\0', sizeof(buf));
    sprintf(buf, "%s", private_data->current_dir);
    if(strlen(buf) > 1) { /* not '/' directory */
      
      p = &buf[strlen(buf)-1];
      while(*p && *p != '/') {
	*p = '\0';
	p--;
      }
      
      /* Remove last '/' if current_dir isn't root */
      if((strlen(buf) > 1) && *p == '/') 
	*p = '\0';
      
      sprintf(private_data->current_dir, "%s", buf);
    }
  }
  else {
    
    /* not '/' directory */
    if(strcasecmp(private_data->current_dir, "/")) {
      memset(&buf, '\0', sizeof(buf));
      sprintf(buf, "%s/%s", private_data->current_dir, 
	      private_data->fc->dir_contents[j]);
    }
    else {
      memset(&buf, '\0', sizeof(buf));
      sprintf(buf, "/%s", private_data->fc->dir_contents[j]);
    }
    
    if(is_a_dir(buf)) {
      sprintf(private_data->current_dir, "%s", buf);
    }
    else {
      if(private_data->add_callback)
	private_data->add_callback(NULL, (void *) j, buf);
    }
  }
}

/*
 * Handle selection in browser.
 */
static void filebrowser_select(xitk_widget_t *w, void *data) {
  filebrowser_private_data_t *private_data = (filebrowser_private_data_t *)data;
  int j = -1;

  if((j = xitk_browser_get_current_selected(private_data->fb_list)) >= 0) {

    filebrowser_select_entry(private_data, j);

  }
  
  load_files(NULL, (void *)private_data);
}

/*
 * Handle double click in labelbutton list.
 */
static void handle_dbl_click(xitk_widget_t *w, void *data, int selected) {
  filebrowser_private_data_t *private_data = (filebrowser_private_data_t *)data;

  filebrowser_select_entry(private_data, selected);

  load_files(NULL, (void *)private_data);
}

static void filebrowser_handle_event(XEvent *event, void *data) {
  filebrowser_private_data_t *private_data = (filebrowser_private_data_t *)data;
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;
  
  switch(event->type) {

  case EnterNotify:
    XLOCK(private_data->imlibdata->x.disp);
    XRaiseWindow(private_data->imlibdata->x.disp, private_data->window);
    XUNLOCK(private_data->imlibdata->x.disp);
    break;

  case MappingNotify:
    XLOCK(private_data->imlibdata->x.disp);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUNLOCK(private_data->imlibdata->x.disp);
    break;

  case KeyPress:
    mykeyevent = event->xkey;

    XLOCK (private_data->imlibdata->x.disp);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUNLOCK (private_data->imlibdata->x.disp);

    switch (mykey) {
      
    case XK_s:
    case XK_S:
      filebrowser_select(NULL, (void *)private_data);
      break;
      
    case XK_asciitilde:
      filebrowser_homedir(NULL, (void *)private_data);
      break;
      
    case XK_Down:
    case XK_Next:
      xitk_browser_step_up((xitk_widget_t *)private_data->fb_list, NULL);
      break;
      
    case XK_Up:
    case XK_Prior:
      xitk_browser_step_down((xitk_widget_t *)private_data->fb_list, NULL);
      break;
    }
  }
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  
  if(w->widget_type & WIDGET_TYPE_FILEBROWSER) {
    xitk_filebrowser_destroy(w);
  }
}

/*
 *
 */
void xitk_filebrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  filebrowser_private_data_t *private_data;
  ImlibImage                 *new_img, *old_img;
  XSizeHints                  hint;

  if(w && (w->widget_type & WIDGET_TYPE_FILEBROWSER)) {
    private_data = (filebrowser_private_data_t *)w->private_data;
    
    xitk_skin_lock(skonfig);

    XLOCK(private_data->imlibdata->x.disp);
    
    if(!(new_img = Imlib_load_image(private_data->imlibdata,
				    xitk_skin_get_skin_filename(skonfig,
							private_data->skin_element_name)))) {
      XITK_DIE("%s(): couldn't find image for background\n", __FUNCTION__);
    }
    
    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PSize;
    XSetWMNormalHints(private_data->imlibdata->x.disp, private_data->window, &hint);
    
    XResizeWindow (private_data->imlibdata->x.disp, private_data->window,
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);
    
    
    XUNLOCK(private_data->imlibdata->x.disp);
    
    while(!xitk_is_window_size(private_data->imlibdata->x.disp, private_data->window, 
			       new_img->rgb_width, new_img->rgb_height)) {
      xitk_usec_sleep(10000);
    }
    
    old_img = private_data->bg_image;
    private_data->bg_image = new_img;
    
    XLOCK(private_data->imlibdata->x.disp);
    
    Imlib_destroy_image(private_data->imlibdata, old_img);
    Imlib_apply_image(private_data->imlibdata, new_img, private_data->window);

    XUNLOCK(private_data->imlibdata->x.disp);
    
    xitk_skin_unlock(skonfig);

    xitk_change_skins_widget_list(private_data->widget_list, skonfig);
    
    xitk_paint_widget_list(private_data->widget_list);
  }
}

/*
 * Create file browser window
 */
xitk_widget_t *xitk_filebrowser_create(xitk_skin_config_t *skonfig, xitk_filebrowser_widget_t *fb) {
  GC                            gc;
  XSizeHints                    hint;
  XSetWindowAttributes          attr;
  char                         *title = fb->window_title;
  Atom                          prop, XA_WIN_LAYER;
  MWMHints                      mwmhints;
  XWMHints                     *wm_hint;
  XClassHint                   *xclasshint;
  XColor                        black, dummy;
  int                           screen;
  xitk_widget_t                *mywidget;
  xitk_button_widget_t          b;
  xitk_labelbutton_widget_t     lb;
  xitk_label_widget_t           lbl;
  filebrowser_private_data_t   *private_data;
  long data[1];
  
  XITK_CHECK_CONSTITENCY(fb);

  XITK_WIDGET_INIT(&b, fb->imlibdata);
  XITK_WIDGET_INIT(&lb, fb->imlibdata);
  XITK_WIDGET_INIT(&lbl, fb->imlibdata);

  mywidget                        = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));

  private_data                    = (filebrowser_private_data_t *) xitk_xmalloc(sizeof(filebrowser_private_data_t));

  mywidget->private_data          = private_data;

  private_data->fbWidget          = mywidget;

  private_data->imlibdata         = fb->imlibdata;
  private_data->skin_element_name = strdup(fb->skin_element_name);
  private_data->running           = 1;

  XLOCK(fb->imlibdata->x.disp);
  
  if(!(private_data->bg_image  = Imlib_load_image(fb->imlibdata, 
						  xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name)))) {
    XITK_WARNING("%s(%d): couldn't find image for background\n", __FILE__, __LINE__);
    
    XUNLOCK(fb->imlibdata->x.disp);
    return NULL;
  }

  private_data->fc             = (file_contents_t *)
    xitk_xmalloc(sizeof(file_contents_t));
  private_data->fc->sort_order = DEFAULT_SORT;

  hint.x                       = fb->x;
  hint.y                       = fb->y;
  hint.width                   = private_data->bg_image->rgb_width;
  hint.height                  = private_data->bg_image->rgb_height;
  hint.flags                   = PPosition | PSize;

  screen                       = DefaultScreen(fb->imlibdata->x.disp);

  XAllocNamedColor(fb->imlibdata->x.disp, 
		   Imlib_get_colormap(fb->imlibdata),
                   "black", &black, &dummy);

  attr.override_redirect       = False;
  attr.background_pixel        = black.pixel;
  /*
   * XXX:multivis
   * To avoid BadMatch errors on XCreateWindow:
   * If the parent and the new window have different depths, we must supply either
   * a BorderPixmap or a BorderPixel.
   * If the parent and the new window use different visuals, we must supply a
   * Colormap
   */
  attr.border_pixel            = 1;
  attr.colormap		       = Imlib_get_colormap(fb->imlibdata);

  private_data->window = 
    XCreateWindow (fb->imlibdata->x.disp, DefaultRootWindow(fb->imlibdata->x.disp), 
		   hint.x, hint.y, hint.width, 
		   hint.height, 0, 
		   fb->imlibdata->x.depth, 
		   InputOutput, 
		   fb->imlibdata->x.visual,
		   CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect, &attr);
  
  XSetStandardProperties(fb->imlibdata->x.disp, private_data->window, title, title,
			 None, NULL, 0, &hint);

  XSelectInput(fb->imlibdata->x.disp, private_data->window, INPUT_MOTION | KeymapStateMask);
  
  /*
   * layer above most other things, like gnome panel
   * WIN_LAYER_ABOVE_DOCK  = 10
   *
   */
  if(fb->layer_above) {
    XA_WIN_LAYER = XInternAtom(fb->imlibdata->x.disp, "_WIN_LAYER", False);
    
    data[0] = 10;
    XChangeProperty(fb->imlibdata->x.disp, private_data->window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }

  /*
   * wm, no border please
   */
  prop                 = XInternAtom(fb->imlibdata->x.disp, "_MOTIF_WM_HINTS", True);
  mwmhints.flags       = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(fb->imlibdata->x.disp, private_data->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);						
  if(fb->window_trans != None)
    XSetTransientForHint (fb->imlibdata->x.disp, private_data->window, fb->window_trans);
  
  /* set xclass */
  
  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = fb->resource_name ? fb->resource_name : "xitk file browser";
    xclasshint->res_class = fb->resource_class ? fb->resource_class : "xitk";
    XSetClassHint(fb->imlibdata->x.disp, private_data->window, xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags         = InputHint | StateHint;
    XSetWMHints(fb->imlibdata->x.disp, private_data->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(fb->imlibdata->x.disp, private_data->window, 0, 0);
  
  Imlib_apply_image(fb->imlibdata, 
		    private_data->bg_image, private_data->window);

  XUNLOCK(fb->imlibdata->x.disp);

  private_data->widget_list                = xitk_widget_list_new() ;
  private_data->widget_list->l             = xitk_list_new ();
  private_data->widget_list->focusedWidget = NULL;
  private_data->widget_list->pressedWidget = NULL;
  private_data->widget_list->win           = private_data->window;
  private_data->widget_list->gc            = gc;
  
  b.imlibdata          = fb->imlibdata;

  lb.imlibdata         = fb->imlibdata;

  lbl.imlibdata        = fb->imlibdata;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = fb->homedir.caption;
  lb.callback          = filebrowser_homedir;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)private_data;
  lb.skin_element_name = fb->homedir.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
			  xitk_labelbutton_create (skonfig, &lb));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = fb->select.caption;
  lb.callback          = filebrowser_select;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)private_data;
  lb.skin_element_name = fb->select.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
			  xitk_labelbutton_create (skonfig, &lb));

  lb.button_type    = CLICK_BUTTON;
  lb.label          = fb->dismiss.caption;
  lb.callback       = xitk_filebrowser_exit;
  lb.state_callback = (void *)mywidget;
  lb.userdata       = (void *)private_data;
  lb.skin_element_name = fb->dismiss.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
			  xitk_labelbutton_create (skonfig, &lb));
  
  private_data->add_callback      = fb->select.callback;
  private_data->kill_callback     = fb->kill.callback;

  fb->browser.dbl_click_callback = handle_dbl_click;
  fb->browser.dbl_click_time     = DEFAULT_DBL_CLICK_TIME;
  fb->browser.userdata           = (void *)private_data;
  fb->browser.parent_wlist       = private_data->widget_list;
  xitk_list_append_content(private_data->widget_list->l,
			  (private_data->fb_list = 
			   xitk_browser_create(skonfig, &fb->browser)));

  b.skin_element_name = fb->sort_default.skin_element_name;
  b.callback          = filebrowser_sortfiles;
  b.userdata          = (void*)&private_data->sort_default;
  
  private_data->sort_default.sort = DEFAULT_SORT;
  private_data->sort_default.w    = mywidget;
  xitk_list_append_content (private_data->widget_list->l, 
			   xitk_button_create (skonfig, &b));
  
  b.skin_element_name = fb->sort_reverse.skin_element_name;
  b.callback          = filebrowser_sortfiles;
  b.userdata          = (void*)&private_data->sort_reverse;
  
  private_data->sort_reverse.sort = REVERSE_SORT;
  private_data->sort_reverse.w    = mywidget;
  xitk_list_append_content (private_data->widget_list->l, 
			   xitk_button_create (skonfig, &b));
  
  lbl.label             = fb->current_dir.cur_directory;
  lbl.skin_element_name = fb->current_dir.skin_element_name;
  lbl.window            = private_data->widget_list->win;
  lbl.gc                = private_data->widget_list->gc;
  xitk_list_append_content (private_data->widget_list->l,
			   (private_data->widget_current_dir = 
			    xitk_label_create (skonfig, &lbl)));
  

  if(fb->current_dir.cur_directory) {
    sprintf(private_data->current_dir, "%s", fb->current_dir.cur_directory);
    
    if((is_a_dir(private_data->current_dir) != 1)
       || (strlen(private_data->current_dir) == 0))
      sprintf(private_data->current_dir, "%s", xitk_get_homedir());
  }
  else
    sprintf(private_data->current_dir, "%s", xitk_get_homedir());
  
  private_data->visible        = 1;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->imlibdata          = private_data->imlibdata;
  mywidget->x                  = fb->x;
  mywidget->y                  = fb->y;
  mywidget->width              = private_data->bg_image->width;
  mywidget->height             = private_data->bg_image->height;
  mywidget->widget_type        = WIDGET_TYPE_FILEBROWSER | WIDGET_TYPE_GROUP;
  mywidget->paint              = NULL;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = NULL;
  mywidget->notify_change_skin = NULL;
  mywidget->notify_destroy     = notify_destroy;
  mywidget->get_skin           = NULL;

  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  load_files(NULL, (void *)private_data);
  
  xitk_browser_update_list(private_data->fb_list, 
			   private_data->fc->dir_disp_contents,
			   private_data->dir_contents_num, 0);

  XLOCK(fb->imlibdata->x.disp);
  XMapRaised(fb->imlibdata->x.disp, private_data->window); 
  XUNLOCK(fb->imlibdata->x.disp);

  private_data->widget_key = 
    xitk_register_event_handler("file browser",
				private_data->window, 
				filebrowser_handle_event,
				NULL,
				fb->dndcallback,
				private_data->widget_list,
				(void *) private_data);

  return mywidget;
}

