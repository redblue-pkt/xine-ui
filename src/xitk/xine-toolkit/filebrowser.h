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

#ifndef HAVE_XITK_FILEBROWSER_H
#define HAVE_XITK_FILEBROWSER_H

#include "_xitk.h"

#define MAXFILES      65535

#define DEFAULT_SORT 0
#define REVERSE_SORT 1

typedef struct {
  /* Files handle by the file browser */
  char                  *dir_contents[MAXFILES];
  /* Files displayed by the browser widget */
  char                  *dir_disp_contents[MAXFILES];

  int                    sort_order;
} file_contents_t;

typedef struct {
  int                    sort;
  xitk_widget_t               *w;
} sort_param_t;

typedef struct {

  xitk_widget_t          *fbWidget; /*  My widget */

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_register_key_t     widget_key;

  Window                  window; /* file browser window */
  
  ImlibImage             *bg_image;
  xitk_widget_list_t     *widget_list; /* File browser widget list */
  
  file_contents_t        *fc; /* file browser content */
  int                     dir_contents_num; /* number of entries in file browser */

  xitk_widget_t          *widget_current_dir; /* Current directory widget */
  char                    current_dir[XITK_PATH_MAX + 1]; /* Current directory */

  int                     running; /* Boolean status */
  int                     visible; /* Boolean status */

  xitk_widget_t          *fb_list; /*  Browser list widget */

  sort_param_t            sort_default;
  sort_param_t            sort_reverse;

  xitk_string_callback_t  add_callback;
  xitk_simple_callback_t  kill_callback;

} filebrowser_private_data_t;

#endif
