/* 
 * Copyright (C) 2000-2001 the xine project
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

#ifndef HAVE_FILEBROWSER_H
#define HAVE_FILEBROWSER_H

#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "dnd.h"
#include "browser.h"

#define MAXFILES      4096

#define DEFAULT_SORT 0
#define REVERSE_SORT 1

#define FNAME_MAX 256
#define FPATH_MAX 768

typedef struct {
  /* Files handle by the file browser */
  char                  *dir_contents[MAXFILES];
  /* Files displayed by the browser widget */
  char                  *dir_disp_contents[MAXFILES];

  int                    sort_order;
} file_contents_t;

typedef struct {
  
  int                     x;
  int                     y;
  char                   *window_title;
  char                   *bg_skinfile;
  char                   *resource_name;
  char                   *resource_class;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
  } sort_default;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
  } sort_reverse;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
    int                   max_length;
    char                 *cur_directory;
  } current_dir;
  
  dnd_callback_t          dndcallback;

  struct {
    int                   x;
    int                   y;
    char                 *caption;
    char                 *skin_filename;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
  } homedir;

  struct {
    int                   x;
    int                   y;
    char                 *caption;
    char                 *skin_filename;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
    void                (*callback) (widget_t *widget, void *data, const char *);
  } select;

  struct {
    int                   x;
    int                   y;
    char                 *caption;
    char                 *skin_filename;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
  } dismiss;

  struct {
    void                (*callback) (widget_t *widget, void *data);
  } kill;
 
  browser_placements_t   *br_placement;

} filebrowser_placements_t;

typedef struct {
  int                    sort;
  widget_t               *w;
} sort_param_t;

typedef struct {

  widget_t               *fbWidget; /*  My widget */

  Display                *display; /* Current display */

  widgetkey_t             widget_key;

  Window                  window; /* file browser window */
  
  ImlibImage             *bg_image;
  widget_list_t          *widget_list; /* File browser widget list */
  
  file_contents_t        *fc; /* file browser content */
  int                     dir_contents_num; /* number of entries in file browser */

  widget_t               *widget_current_dir; /* Current directory widget */
  char                    current_dir[FPATH_MAX + 1]; /* Current directory */

  int                     running; /* Boolean status */
  int                     visible; /* Boolean status */

  widget_t               *fb_list; /*  Browser list widget */

  sort_param_t           sort_default;
  sort_param_t           sort_reverse;

  void                  (*add_callback) (widget_t *widget, void *data, const char *filename);
  void                  (*kill_callback) (widget_t *widget, void *data);

} filebrowser_private_data_t;

widget_t *filebrowser_create(Display *display, ImlibData *idata,
			     Window window_trans,
			     filebrowser_placements_t *fbp);

int filebrowser_is_running(widget_t *w);
int filebrowser_is_visible(widget_t *w);
void filebrowser_hide(widget_t *w);
void filebrowser_show(widget_t *w);
void filebrowser_set_transient(widget_t *w, Window window);
void filebrowser_destroy(widget_t *w);
char *filebrowser_get_current_dir(widget_t *w);
#endif
