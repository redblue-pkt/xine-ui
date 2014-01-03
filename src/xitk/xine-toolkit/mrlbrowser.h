/* 
 * Copyright (C) 2000-2009 the xine project
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

#ifndef HAVE_XITK_MRLBROWSER_H
#define HAVE_XITK_MRLBROWSER_H

#ifdef NEED_MRLBROWSER

#include "_xitk.h"

#define MAXFILES      65535

/*
 * Freeing/zeroing all of entries of given mrl.
 */
#define MRL_ZERO(m) {                                                         \
  if((m)) {                                                                   \
    free((m)->origin);                                                        \
    free((m)->mrl);                                                           \
    free((m)->link);                                                          \
    (m)->origin = NULL;                                                       \
    (m)->mrl    = NULL;                                                       \
    (m)->link   = NULL;                                                       \
    (m)->type   = 0;                                                          \
    (m)->size   = (off_t) 0;                                                  \
  }                                                                           \
}

/*
 * Duplicate two mrls entries (s = source, d = destination).
 */
#define MRL_DUPLICATE(s, d) {                                                 \
  ABORT_IF_NULL(s);                                                           \
  ABORT_IF_NULL(d);                                                           \
                                                                              \
  if((s)->origin) {                                                           \
    if((d)->origin) {                                                         \
      (d)->origin = (char *) realloc((d)->origin, strlen((s)->origin) + 1);   \
      strcpy((d)->origin, (s)->origin);                                	      \
    }                                                                         \
    else                                                                      \
      (d)->origin = strdup((s)->origin);                                      \
  }                                                                           \
  else                                                                        \
    (d)->origin = NULL;                                                       \
                                                                              \
  if((s)->mrl) {                                                              \
    if((d)->mrl) {                                                            \
      (d)->mrl = (char *) realloc((d)->mrl, strlen((s)->mrl) + 1);            \
      strcpy((d)->mrl, (s)->mrl);                                      	      \
    }                                                                         \
    else                                                                      \
      (d)->mrl = strdup((s)->mrl);                                            \
  }                                                                           \
  else                                                                        \
    (d)->mrl = NULL;                                                          \
                                                                              \
  if((s)->link) {                                                             \
    if((d)->link) {                                                           \
      (d)->link = (char *) realloc((d)->link, strlen((s)->link) + 1);         \
      strcpy((d)->link, (s)->link);                                    	      \
    }                                                                         \
    else                                                                      \
      (d)->link = strdup((s)->link);                                          \
  }                                                                           \
  else                                                                        \
    (d)->link = NULL;                                                         \
                                                                              \
  (d)->type = (s)->type;                                                      \
  (d)->size = (s)->size;                                                      \
}

/**
 * Duplicate two arrays of mrls (s = source, d = destination).
 */
#define MRLS_DUPLICATE(s, d) {                                                \
  int i = 0;                                                                  \
                                                                              \
  ABORT_IF_NULL(s);                                                           \
  ABORT_IF_NULL(d);                                                           \
                                                                              \
  while((s) != NULL) {                                                        \
    d[i] = (xine_mrl_t *) malloc(sizeof(xine_mrl_t));                         \
    MRL_DUPLICATE(s[i], d[i]);                                                \
    i++;                                                                      \
  }                                                                           \
}

typedef struct {
  xine_mrl_t                *mrls[MAXFILES];
  xine_mrl_t                *filtered_mrls[MAXFILES];
  char                      *mrls_disp[MAXFILES];
  int                        mrls_to_disp;
} mrl_contents_t;

typedef struct {

  xitk_widget_t             *fbWidget; /*  My widget */

  ImlibData                 *imlibdata;
  char                      *skin_element_name;
  char                      *skin_element_name_ip;

  xitk_register_key_t        widget_key;

  Window                     window; /* file browser window */
  
  ImlibImage                *bg_image;
  xitk_widget_list_t        *widget_list; /* File browser widget list */
  
  xine_t                    *xine;

  mrl_contents_t            *mc;
  int                        mrls_num;

  char                      *last_mrl_source;

  xitk_widget_t             *widget_origin; /* Current directory widget */
  char                       current_origin[XITK_PATH_MAX + 1]; /* Current directory */

  int                        running; /* Boolean status */
  int                        visible; /* Boolean status */

  xitk_widget_t             *mrlb_list; /*  Browser list widget */
  xitk_widget_t             *autodir_plugins[64];

  xitk_widget_t             *combo_filter;
  const char               **filters;
  int                        filters_num;
  xitk_mrlbrowser_filter_t **mrl_filters;
  int                        filter_selected;

  xitk_mrl_callback_t        add_callback;
  xitk_mrl_callback_t        play_callback;
  xitk_simple_callback_t     kill_callback;
  xitk_simple_callback_t     ip_callback;

} mrlbrowser_private_data_t;

#endif

#endif
