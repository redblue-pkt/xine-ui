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
#include <inttypes.h> 
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SHM_H
#include <sys/shm.h>
#endif
#ifdef HAVE_XSHM_H
#include <X11/extensions/XShm.h>
#endif

#include "xitk/Imlib-light/Imlib.h"

#include "xitk.h"

#include "_config.h"

#include "browser.h"
#include "button.h"
#include "checkbox.h"
#include "combo.h"
#include "dnd.h"
#include "doublebox.h"
#include "filebrowser.h"
#include "font.h"
#include "image.h"
#include "inputtext.h"
#include "intbox.h"
#include "labelbutton.h"
#include "label.h"
#include "mrlbrowser.h"
#include "skin.h"
#include "slider.h"
#include "tabs.h"
#include "tips.h"
#include "widget.h"
#include "window.h"
#include "menu.h"
#include "xitkintl.h"

#define XITK_WIDGET_MAGIC 0x7869746b

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#ifdef	__GNUC__
#define XITK_DIE(FMT, ARGS...) do { fprintf(stderr, "xiTK DIE: "FMT, ##ARGS); exit(-1); } while(0)
#define XITK_WARNING(FMT, ARGS...) do { fprintf(stderr, "xiTK WARNING(%s:%d): ", __FUNCTION__, __LINE__); fprintf(stderr, FMT, ##ARGS); } while(0)
#else	/* C99 version: */
#define XITK_DIE(...) do { fprintf(stderr, "xiTK DIE: "__VA_ARGS__); exit(-1); } while(0)
#define XITK_WARNING(...) do { fprintf(stderr, "xiTK WARNING(%s:%d): ", __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); } while(0)
#endif

#define XITK_FREE(X) do { if((X)) { free((X)); (X) = NULL; } } while(0)

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

#define DIRECTION_UP     1
#define DIRECTION_DOWN   2
#define DIRECTION_LEFT   3
#define DIRECTION_RIGHT  4

int xitk_x_error;

#ifndef _XITK_C_

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

uint32_t xitk_get_wm_type(void);

/*
 * copy src to dest and substitute special chars. dest should have 
 * enought space to store chars.
 */
void xitk_subst_special_chars(char *, char *);
unsigned long xitk_get_timer_label_animation(void);
long int xitk_get_timer_dbl_click(void);
int xitk_get_barstyle_feature(void);
unsigned long xitk_get_warning_foreground(void);
unsigned long xitk_get_warning_background(void);
void xitk_modal_window(Window w);
void xitk_unmodal_window(Window w);
void xitk_set_current_menu(xitk_widget_t *menu);
void xitk_unset_current_menu(void);

int xitk_skin_get_direction(xitk_skin_config_t *, const char *);
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
char *xitk_skin_get_animation(xitk_skin_config_t *);
int xitk_skin_get_browser_entries(xitk_skin_config_t *, const char *);
void xitk_skin_lock(xitk_skin_config_t *);
void xitk_skin_unlock(xitk_skin_config_t *);

void *labelbutton_get_user_data(xitk_widget_t *w);
void menu_auto_pop(xitk_widget_t *w);

struct xitk_font_s {
  Display       *display;
#ifdef WITH_XMB
  XFontSet       fontset;
#else
  XFontStruct   *font;
#endif
  char          *name;
};

typedef struct xitk_dialog_s xitk_dialog_t;

struct xitk_window_s {
  Window                    window;
  xitk_pixmap_t            *background;
  int                       width;
  int                       height;
  xitk_dialog_t            *parent;
};

struct xitk_dialog_s {
  ImlibData              *imlibdata;
  xitk_window_t          *xwin;
  xitk_register_key_t     key;
  xitk_widget_list_t     *widget_list;

  int                     type;

  xitk_widget_t          *wyes;
  xitk_widget_t          *wno;
  xitk_widget_t          *wcancel;

  xitk_widget_t          *default_button;

  xitk_state_callback_t  yescallback;
  xitk_state_callback_t  nocallback;
  xitk_state_callback_t  cancelcallback;

  void                   *userdata;
};

#endif
