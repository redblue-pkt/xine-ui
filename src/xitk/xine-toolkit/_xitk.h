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
 *
 */

#ifndef __XITK_H_
#define __XITK_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"

#include "dnd.h"
#include "widget.h"

typedef void (*xitk_simple_callback_t)(widget_t *, void *);
typedef void (*xitk_state_callback_t)(widget_t *, void *, int);
typedef void (*xitk_string_callback_t)(widget_t *, void *, const char *);

#ifdef NEED_MRLBROWSER
#include "xine.h"
typedef void (*xitk_mrl_callback_t)(widget_t *, void *, mrl_t *);
#endif

/*
 * timeradd/timersub is missing on solaris' sys/time.h, provide
 * some fallback macros
 */
#ifndef	timeradd
#define timeradd(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;                          \
    if ((result)->tv_usec >= 1000000)                                         \
      {                                                                       \
        ++(result)->tv_sec;                                                   \
        (result)->tv_usec -= 1000000;                                         \
      }                                                                       \
  } while (0)
#endif

#ifndef timersub
#define timersub(a, b, result)                                                \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
#endif

/* Duplicate s to d timeval values */
#define timercpy(s, d) {                                                      \
      (d)->tv_sec = (s)->tv_sec;                                              \
      (d)->tv_usec = (s)->tv_usec;                                            \
    }

#define WINDOW_INFO_ZERO(w) {                                                 \
      if((w)->name)                                                           \
	free((w)->name);                                                      \
      (w)->window = None;                                                     \
      (w)->name   = NULL;                                                     \
      (w)->x      = 0;                                                        \
      (w)->y      = 0;                                                        \
      (w)->height = 0;                                                        \
      (w)->width  = 0;                                                        \
    }

typedef struct {
  Window    window;
  char     *name;
  int       x;
  int       y;
  int       height;
  int       width;
} window_info_t;

typedef struct {
  Display     *display;
  ImlibData   *imlibdata;
  int          x;
  int          y;
  xitk_simple_callback_t callback;
  void         *userdata;
  const char   *skin;
} xitk_button_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  int                     button_type;
  const char             *label;
  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
  void                   *userdata;
  const char             *skin;
  const char             *normcolor;
  const char             *focuscolor;
  const char             *clickcolor;
} xitk_labelbutton_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  const char             *skin;
} xitk_image_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  xitk_state_callback_t  callback;
  void                   *userdata;
  const char             *skin;
} xitk_checkbox_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  Window                  window;
  GC                      gc;
  int                     x;
  int                     y;
  int                     length;
  const char             *label;
  const char             *font;
  int                     animation;
} xitk_label_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  int                     slider_type;
  int                     min;
  int                     max;
  int                     step;
  const char             *background_skin;
  const char             *paddle_skin;
  xitk_state_callback_t   callback;
  void                   *userdata;
  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
} xitk_slider_t;

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
  } arrow_up;
  
  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
  } slider;

  struct {
    char                 *skin_filename;
  } paddle;

  struct {
    int                   x;
    int                   y;
    char                 *skin_filename;
  } arrow_dn;

  struct {
    int                   x;
    int                   y;
    char                 *normal_color;
    char                 *focused_color;
    char                 *clicked_color;
    char                 *skin_filename;
    
    int                   max_displayed_entries;
    
    int                   num_entries;
    char                **entries;
  } browser;
  
  int                     dbl_click_time;
  xitk_state_callback_t   dbl_click_callback;

  /* Callback on selection function */
  xitk_simple_callback_t  callback;
  void                   *userdata;

  widget_list_t          *parent_wlist;
} xitk_browser_t;

typedef struct {
  Display                   *display;
  ImlibData                 *imlibdata;
  Window                     window_trans;

  int                        x;
  int                        y;
  char                      *window_title;
  char                      *background_skin;
  char                      *resource_name;
  char                      *resource_class;

  struct {
    int                      x;
    int                      y;
    char                    *skin_filename;
  } sort_default;

  struct {
    int                      x;
    int                      y;
    char                    *skin_filename;
  } sort_reverse;

  struct {
    int                      x;
    int                      y;
    char                    *skin_filename;
    int                      max_length;
    char                    *cur_directory;
    int                      animation;
  } current_dir;
  
  xitk_dnd_callback_t        dndcallback;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
  } homedir;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
    xitk_string_callback_t   callback;
  } select;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
  } dismiss;

  struct {
    xitk_simple_callback_t   callback;
  } kill;
 
  xitk_browser_t             browser;
} xitk_filebrowser_t;

#ifdef NEED_MRLBROWSER
typedef struct {
  Display                   *display;
  ImlibData                 *imlibdata;
  Window                     window_trans;

  int                        x;
  int                        y;
  char                      *window_title;
  char                      *background_skin;
  char                      *resource_name;
  char                      *resource_class;

  struct {
    int                      x;
    int                      y;
    char                    *skin_filename;
    int                      max_length;
    char                    *cur_origin;
    int                      animation;
  } origin;
  
  xitk_dnd_callback_t        dndcallback;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
    xitk_mrl_callback_t      callback;
  } select;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
  } dismiss;

  struct {
    xitk_simple_callback_t   callback;
  } kill;

  char                     **ip_availables;
  
  struct {

    struct {
      int                    x;
      int                    y;
      char                  *skin_filename;
      char                  *normal_color;
      char                  *focused_color;
      char                  *clicked_color;
    } button;

    struct {
      int                    x;
      int                    y;
      char                  *skin_filename;
      char                  *label_str;
      int                    length;
      int                    animation;
    } label;

  } ip_name;
  
  xine_t                    *xine;

  xitk_browser_t             browser;

} xitk_mrlbrowser_t;
#endif

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  const char             *text;
  int                     max_length;

  xitk_string_callback_t  callback;
  void                   *userdata;

  const char             *skin_filename;
  const char             *normal_color;
  const char             *focused_color;

} xitk_inputtext_t;

#ifndef _XITK_C_

typedef void (*widget_cb_event_t)(XEvent *event, void *user_data);
typedef void (*widget_cb_newpos_t)(int, int, int, int);

widget_list_t *widget_list_new (void);
widgetkey_t widget_register_event_handler(char *name, Window window,
					  widget_cb_event_t cb,
					  widget_cb_newpos_t pos_cb,
					  xitk_dnd_callback_t dnd_cb,
					  widget_list_t *wl,
					  void *user_data);
void widget_unregister_event_handler(widgetkey_t *key);
int widget_get_window_info(widgetkey_t key, window_info_t *winf);

#endif
#endif
