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
 *
 */

#ifndef __XITK_H_
#define __XITK_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <X11/Xlib.h>
#ifdef HAVE_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SHM_H
#include <sys/shm.h>
#endif
#ifdef HAVE_XSHM_H
#include <X11/extensions/XShm.h>
#endif

#include "Imlib-light/Imlib.h"

#define XITK_MAJOR_VERSION (0)
#define XITK_MINOR_VERSION (9)
#define XITK_SUB_VERSION   (0)
#define XITK_VERSION       "0.9.0"

#include "dnd.h"
#include "widget.h"
#include "skin.h"
#include "xitkintl.h"

#define XITK_WIDGET_MAGIC 0x7869746b

typedef void (*xitk_simple_callback_t)(xitk_widget_t *, void *);
typedef void (*xitk_state_callback_t)(xitk_widget_t *, void *, int);
typedef void (*xitk_string_callback_t)(xitk_widget_t *, void *, char *);

#ifdef NEED_MRLBROWSER
#include <xine.h>
typedef void (*xitk_mrl_callback_t)(xitk_widget_t *, void *, xine_mrl_t *);
#endif

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

#ifndef NAME_MAX
#define XITK_NAME_MAX   256
#else
#define XITK_NAME_MAX   NAME_MAX
#endif

#ifndef PATH_MAX
#define XITK_PATH_MAX   768
#else
#define XITK_PATH_MAX   PATH_MAX
#endif

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#ifdef	__GNUC__
#define XITK_DIE(FMT, ARGS...) do { fprintf(stderr, "xiTK DIE: "FMT, ##ARGS); exit(-1); } while(0)
#define XITK_WARNING(FMT, ARGS...) do { fprintf(stderr, "xiTK WARNING: "FMT, ##ARGS); } while(0)
#else	/* C99 version: */
#define XITK_DIE(...) do { fprintf(stderr, "xiTK DIE: "__VA_ARGS__); exit(-1); } while(0)
#define XITK_WARNING(...) do { fprintf(stderr, "xiTK WARNING: "__VA_ARGS__); } while(0)
#endif

#define XITK_FREE(X) do { if((X)) { free((X)); (X) = NULL; } } while(0)

#define XITK_WIDGET_INIT(X, IMLIBDATA) do {                                              \
                                         (X)->magic = XITK_WIDGET_MAGIC;                 \
                                         (X)->imlibdata = IMLIBDATA;                     \
                                       } while(0)

#define XITK_CHECK_CONSTITENCY(X) do {                                                    \
                                    if(((X) == NULL) || ((X)->magic != XITK_WIDGET_MAGIC) \
                                       || ((X)->imlibdata == NULL))                       \
                                      XITK_DIE("%s(): widget constitency failed.!\n",     \
                                               __FUNCTION__);                             \
                                  } while(0)

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

#ifdef HAVE_STRPBRK
#define xitk_strpbrk strpbrk
#else
static inline char *_x_strpbrk(const char *s, const char *accept) {

  while(*s != '\0') {
    const char *a = accept;
    while(*a != '\0')
      if(*a++ == *s)
	return(char *) s;
    ++s;
  }

  return NULL;
}
#define xitk_strpbrk _x_strpbrk
#endif

#ifdef HAVE_STRSEP
#define xitk_strsep strsep
#else
static inline char *_x_strsep(char **stringp, const char *delim) {
  char *begin, *end;
  
  begin = *stringp;
  if(begin == NULL)
    return NULL;
  
  if(delim[0] == '\0' || delim[1] == '\0') {
    char ch = delim[0];
    
    if(ch == '\0')
      end = NULL;
    else {
      if(*begin == ch)
	end = begin;
      else if(*begin == '\0')
	end = NULL;
      else
	end = strchr(begin + 1, ch);
    }
  }
  else
    end = xitk_strpbrk(begin, delim);
  
  if(end) {
    *end++ = '\0';
    *stringp = end;
  }
  else
    *stringp = NULL;
  
  return begin;
}
#define xitk_strsep _x_strsep
#endif

void xitk_strdupa(char *dest, char *src);
#define xitk_strdupa(d, s) do {                                     \
  (d) = NULL;                                                       \
  if((s) != NULL) {                                                 \
    (d) = (char *) alloca(strlen((s)) + 1);                         \
    strcpy((d), (s));                                               \
  }                                                                 \
} while(0)

/* Duplicate s to d timeval values */
#define timercpy(s, d) do {                                                   \
      (d)->tv_sec = (s)->tv_sec;                                              \
      (d)->tv_usec = (s)->tv_usec;                                            \
} while(0)

#define WINDOW_INFO_ZERO(w) do {                                              \
      if((w)->name)                                                           \
	free((w)->name);                                                      \
      (w)->window = None;                                                     \
      (w)->name   = NULL;                                                     \
      (w)->x      = 0;                                                        \
      (w)->y      = 0;                                                        \
      (w)->height = 0;                                                        \
      (w)->width  = 0;                                                        \
} while(0)

#define INPUT_MOTION (ExposureMask | ButtonPressMask | ButtonReleaseMask |    \
                      KeyPressMask | KeyReleaseMask | ButtonMotionMask |      \
                      StructureNotifyMask | PropertyChangeMask |              \
                      LeaveWindowMask | EnterWindowMask | PointerMotionMask)

int xitk_x_error;

typedef struct xitk_pixmap_s xitk_pixmap_t;

typedef struct {
  Window    window;
  char     *name;
  int       x;
  int       y;
  int       height;
  int       width;
} window_info_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  char                     *skin_element_name;
  xitk_simple_callback_t    callback;
  void                     *userdata;
} xitk_button_widget_t;

#define LABEL_ALIGN_CENTER 1
#define LABEL_ALIGN_LEFT   2
#define LABEL_ALIGN_RIGHT  3
typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  int                       button_type;
  int                       align;
  char                     *label;
  xitk_simple_callback_t    callback;
  xitk_state_callback_t     state_callback;
  void                     *userdata;
  char                     *skin_element_name;
} xitk_labelbutton_widget_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  char                     *skin_element_name;
} xitk_image_widget_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  xitk_state_callback_t     callback;
  void                     *userdata;
  char                     *skin_element_name;
} xitk_checkbox_widget_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  Window                    window;
  GC                        gc;
  char                     *label;
  char                     *skin_element_name;
  xitk_simple_callback_t    callback;
  void                     *userdata;
} xitk_label_widget_t;

#define XITK_VSLIDER 1
#define XITK_HSLIDER 2
#define XITK_RSLIDER 3

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  int                       min;
  int                       max;
  int                       step;
  char                     *skin_element_name;
  xitk_state_callback_t     callback;
  void                     *userdata;
  xitk_state_callback_t     motion_callback;
  void                     *motion_userdata;
} xitk_slider_widget_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;

  struct {
    char                   *skin_element_name;
  } arrow_up;
  
  struct {
    char                   *skin_element_name;
  } slider;

  struct {
    char                   *skin_element_name;
  } arrow_dn;

  struct {
    char                   *skin_element_name;
    int                     max_displayed_entries;
    int                     num_entries;
    char                  **entries;
  } browser;
  
  xitk_state_callback_t     dbl_click_callback;

  /* Callback on selection function */
  xitk_simple_callback_t    callback;
  void                     *userdata;

  xitk_widget_list_t       *parent_wlist;
} xitk_browser_widget_t;

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  char                     *skin_element_name;
  Window                    window_trans;
  int                       layer_above;

  int                       x;
  int                       y;
  char                     *window_title;
  char                     *resource_name;
  char                     *resource_class;

  struct {
    char                   *skin_element_name;
  } sort_default;

  struct {
    char                   *skin_element_name;
  } sort_reverse;

  struct {
    char                   *cur_directory;
    char                   *skin_element_name;
  } current_dir;
  
  xitk_dnd_callback_t       dndcallback;

  struct {
    char                   *caption;
    char                   *skin_element_name;
  } homedir;

  struct {
    char                   *caption;
    char                   *skin_element_name;
    xitk_string_callback_t  callback;
  } select;

  struct {
    char                   *caption;
    char                   *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t  callback;
  } kill;
 
  xitk_browser_widget_t     browser;
} xitk_filebrowser_widget_t;

#ifdef NEED_MRLBROWSER
typedef struct {
  char                      *name;
  char                      *ending;
} xitk_mrlbrowser_filter_t;

typedef struct {
  int                        magic;
  ImlibData                 *imlibdata;
  char                      *skin_element_name;
  Window                     window_trans;
  int                        layer_above;

  int                        x;
  int                        y;
  char                      *window_title;
  char                      *resource_name;
  char                      *resource_class;

  struct {
    char                    *cur_origin;
    char                    *skin_element_name;
  } origin;
  
  xitk_dnd_callback_t        dndcallback;

  struct {
    char                    *caption;
    char                    *skin_element_name;
    xitk_mrl_callback_t      callback;
  } select;

  struct {
    char                    *skin_element_name;
    xitk_mrl_callback_t      callback;
  } play;

  struct {
    char                    *caption;
    char                    *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t   callback;
  } kill;

  char                     **ip_availables;
  
  struct {

    struct {
      char                  *skin_element_name;
    } button;

    struct {
      char                  *label_str;
      char                  *skin_element_name;
    } label;

  } ip_name;
  
  xine_t                    *xine;

  xitk_browser_widget_t      browser;
  
  xitk_mrlbrowser_filter_t **mrl_filters;

  struct {
    char                     *skin_element_name;
  } combo;

} xitk_mrlbrowser_widget_t;
#endif

typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  char                     *text;
  int                       max_length;
  xitk_string_callback_t    callback;
  void                     *userdata;
  char                     *skin_element_name;
} xitk_inputtext_widget_t;

#ifndef _XITK_C_

typedef void (*widget_event_callback_t)(XEvent *event, void *user_data);
typedef void (*widget_newpos_callback_t)(int, int, int, int);

xitk_widget_list_t *xitk_widget_list_new (void);
xitk_register_key_t xitk_register_event_handler(char *name, Window window,
						widget_event_callback_t cb,
						widget_newpos_callback_t pos_cb,
						xitk_dnd_callback_t dnd_cb,
						xitk_widget_list_t *wl,
						void *user_data);
void xitk_unregister_event_handler(xitk_register_key_t *key);
int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf);
void xitk_xevent_notify(XEvent *event);

#endif

int xitk_install_x_error_handler(void);
int xitk_uninstall_x_error_handler(void);

const char *xitk_get_homedir(void);
void xitk_usec_sleep(unsigned long);
int xitk_system(int dont_run_as_root, char *command);
int xitk_is_use_xshm(void);

char *xitk_get_system_font(void);
char *xitk_get_default_font(void);

int xitk_get_black_color(void);
int xitk_get_white_color(void);
int xitk_get_background_color(void);
int xitk_get_focus_color(void);
int xitk_get_select_color(void);
/*
 * copy src to dest and substitute special chars. dest should have 
 * enought space to store chars.
 */
void xitk_subst_special_chars(char *, char *);
unsigned long xitk_get_timer_label_animation(void);
long int xitk_get_timer_dbl_click(void);
unsigned long xitk_get_warning_foreground(void);
unsigned long xitk_get_warning_background(void);

int xitk_skin_get_visibility(xitk_skin_config_t *, const char *);
int xitk_skin_get_enability(xitk_skin_config_t *, const char *);
int xitk_skin_get_coord_x(xitk_skin_config_t *, const char *);
int xitk_skin_get_coord_y(xitk_skin_config_t *, const char *);
char *xitk_skin_get_label_color(xitk_skin_config_t *, const char *);
char *xitk_skin_get_label_color_focus(xitk_skin_config_t *, const char *);
char *xitk_skin_get_label_color_click(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_length(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_animation(xitk_skin_config_t *, const char *);
char *xitk_skin_get_label_fontname(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_printable(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_staticity(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_alignment(xitk_skin_config_t *, const char *);
char *xitk_skin_get_label_skinfont_filename(xitk_skin_config_t *, const char *);
char *xitk_skin_get_skin_filename(xitk_skin_config_t *, const char *);
char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *, const char *);
int xitk_skin_get_slider_type(xitk_skin_config_t *, const char *);
int xitk_skin_get_slider_radius(xitk_skin_config_t *, const char *);
char *xitk_skin_get_logo(xitk_skin_config_t *);
int xitk_skin_get_browser_entries(xitk_skin_config_t *, const char *);
void xitk_skin_lock(xitk_skin_config_t *);
void xitk_skin_unlock(xitk_skin_config_t *);

typedef struct {
  Display       *display;
  XFontStruct   *font;
} xitk_font_t;


typedef struct {
  int                       magic;
  ImlibData                *imlibdata;
  char                     *skin_element_name;
  xitk_widget_list_t       *parent_wlist;
  char                    **entries;
  int                       layer_above;
  xitk_state_callback_t     callback;
  void                     *userdata;
  xitk_register_key_t      *parent_wkey;
} xitk_combo_widget_t;


typedef struct {
  Window                    window;
  xitk_pixmap_t            *background;
  int                       width;
  int                       height;
} xitk_window_t;

typedef void (*xitk_pixmap_destroyer_t)(xitk_pixmap_t *);
struct xitk_pixmap_s {
  ImlibData                *imlibdata;
  XImage                   *xim;
  Pixmap                    pixmap;
  GC                        gc;
  XGCValues                 gcv;
  int                       width;
  int                       height;
  int                       shm;
#ifdef HAVE_SHM
  XShmSegmentInfo          *shminfo;
#endif
  xitk_pixmap_destroyer_t   destroy;
};

struct xitk_image_s {
  xitk_pixmap_t               *image;
  xitk_pixmap_t               *mask;
  int                          width;
  int                          height;
};

typedef struct {
  int                     magic;
  ImlibData              *imlibdata;
  
  char                   *skin_element_name;
  int                     num_entries;
  char                  **entries;

  xitk_widget_list_t     *parent_wlist;

  xitk_state_callback_t  callback;
  void                   *userdata;

} xitk_tabs_widget_t;

typedef struct {
  int                     magic;
  ImlibData              *imlibdata;

  char                   *skin_element_name;
  
  int                     value;
  int                     step;

  xitk_widget_list_t     *parent_wlist;

  xitk_state_callback_t   callback;
  void                   *userdata;

} xitk_intbox_widget_t;

#endif
