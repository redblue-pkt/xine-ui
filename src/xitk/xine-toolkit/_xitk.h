/* 
 * Copyright (C) 2000-2020 the xine project
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
 *
 */

#ifndef __XITK_H_
#define __XITK_H_

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h> 

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XSHM_H
#include <X11/extensions/XShm.h>
#endif
#ifdef WITH_XFT
#include <ft2build.h>
#include <X11/Xft/Xft.h>
#endif

#include "xitk/Imlib-light/Imlib.h"

#include "libcommon.h"
#include "xitk.h"

typedef struct xitk_font_cache_s xitk_font_cache_t;
struct xitk_tips_s;

struct xitk_s {
  Display  *display;
  void    (*x_lock_display) (Display *display);
  void    (*x_unlock_display) (Display *display);

  xitk_font_cache_t *font_cache;

  struct xitk_tips_s *tips;
};

extern xitk_t *gXitk;

extern void (*xitk_x_lock_display) (Display *display);
extern void (*xitk_x_unlock_display) (Display *display);

typedef struct {
  xitk_widget_t    *itemlist;
  int               sel;
} btnlist_t;

const xitk_skin_element_info_t *xitk_skin_get_info (xitk_skin_config_t *skin, const char *element_name);

#include "_config.h"
#include "browser.h"
#include "button.h"
#include "checkbox.h"
#include "combo.h"
#include "cursors.h"
#include "dnd.h"
#include "doublebox.h"
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
#include "widget.h"
#include "window.h"
#include "menu.h"
#include "xitkintl.h"

#if !defined(__GNUC__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define	__FUNCTION__	__func__
#endif

#define XITK_WIDGET_MAGIC 0x7869746b

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#if defined(__GNUC__) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)
#define XITK_DIE(FMT, ARGS...)                                        \
  do {                                                                \
    fprintf(stderr, "xiTK DIE: "FMT, ##ARGS);                         \
    exit(-1);                                                         \
  } while(0)

#define XITK_WARNING(FMT, ARGS...)                                    \
  do {                                                                \
    fprintf(stderr, "xiTK WARNING(%s:%d): ", __FUNCTION__, __LINE__); \
    fprintf(stderr, FMT, ##ARGS);                                     \
  } while(0)
#else	/* C99 version: */
#define XITK_DIE(...)                                                 \
  do {                                                                \
    fprintf(stderr, "xiTK DIE: "__VA_ARGS__);                         \
    exit(-1);                                                         \
  } while(0)

#define XITK_WARNING(...)                                             \
  do {                                                                \
    fprintf(stderr, "xiTK WARNING(%s:%d): ", __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                     \
  } while(0)
#endif

#define ABORT_IF_NOT_COND(cond)                                                                      \
  do {                                                                                               \
    if(!(cond)) {                                                                                    \
      fprintf(stderr, "%s(%d): condition '%s' failed. Aborting.\n",  __FUNCTION__, __LINE__, #cond); \
      abort();                                                                                       \
    }                                                                                                \
  } while(0)

#define ABORT_IF_NULL(p) ABORT_IF_NOT_COND((p) != NULL)

#define XITK_FREE(X) \
  do {               \
      free((X));     \
      (X) = NULL;    \
  } while(0)

#define XITK_CHECK_CONSTITENCY(X)                                                \
  do {                                                                           \
    if(((X) == NULL) || ((X)->magic != XITK_WIDGET_MAGIC))                       \
      XITK_DIE("%s(%d): widget consistency failed.!\n", __FUNCTION__, __LINE__); \
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

/* Duplicate s to d timeval values */
#define timercpy(s, d) do {                                                   \
      (d)->tv_sec = (s)->tv_sec;                                              \
      (d)->tv_usec = (s)->tv_usec;                                            \
} while(0)

#define INPUT_MOTION (ExposureMask | ButtonPressMask | ButtonReleaseMask |    \
                      KeyPressMask | KeyReleaseMask | ButtonMotionMask |      \
                      StructureNotifyMask | PropertyChangeMask |              \
                      LeaveWindowMask | EnterWindowMask | PointerMotionMask)

#define DIRECTION_UP     1
#define DIRECTION_DOWN   2
#define DIRECTION_LEFT   3
#define DIRECTION_RIGHT  4

#define CHECK_STYLE_OLD   1
#define CHECK_STYLE_CHECK 2
#define CHECK_STYLE_ROUND 3

extern int xitk_x_error;

#ifndef _XITK_C_

xitk_widget_list_t *xitk_widget_list_new (ImlibData *imlibdata);
xitk_register_key_t xitk_register_event_handler(const char *name,
                                                xitk_window_t *w,
                                                //Window window,
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

const char *xine_get_homedir(void);
void xitk_usec_sleep(unsigned long);
int xitk_system(int dont_run_as_root, const char *command);
int xitk_is_use_xshm(void);

const char *xitk_get_system_font(void);
const char *xitk_get_default_font(void);

int xitk_get_black_color(void);
int xitk_get_white_color(void);
int xitk_get_background_color(void);
int xitk_get_focus_color(void);
int xitk_get_select_color(void);

uint32_t xitk_get_wm_type(void);

char *xitk_filter_filename(const char *name);
unsigned long xitk_get_timer_label_animation(void);
long int xitk_get_timer_dbl_click(void);
int xitk_get_barstyle_feature(void);
int xitk_get_checkstyle_feature(void);
int xitk_get_cursors_feature(void);
unsigned long xitk_get_warning_foreground(void);
unsigned long xitk_get_warning_background(void);
void xitk_modal_window(Window w);
void xitk_unmodal_window(Window w);
void xitk_set_current_menu(xitk_widget_t *menu);
void xitk_unset_current_menu(void);
//int xitk_get_display_width(void);
//int xitk_get_display_height(void);
unsigned long xitk_get_tips_timeout(void);
void xitk_set_tips_timeout(unsigned long timeout);

int xitk_is_running(void);

int xitk_skin_get_direction(xitk_skin_config_t *, const char *);
int xitk_skin_get_visibility(xitk_skin_config_t *, const char *);
int xitk_skin_get_printability(xitk_skin_config_t *, const char *);
int xitk_skin_get_enability(xitk_skin_config_t *, const char *);
int xitk_skin_get_coord_x(xitk_skin_config_t *, const char *);
int xitk_skin_get_coord_y(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_label_color(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_label_color_focus(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_label_color_click(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_length(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_animation(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_label_fontname(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_printable(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_staticity(xitk_skin_config_t *, const char *);
int xitk_skin_get_label_alignment(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_label_skinfont_filename(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_skin_filename(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *, const char *);
int xitk_skin_get_slider_type(xitk_skin_config_t *, const char *);
int xitk_skin_get_slider_radius(xitk_skin_config_t *, const char *);
const char *xitk_skin_get_logo(xitk_skin_config_t *);
const char *xitk_skin_get_animation(xitk_skin_config_t *);
int xitk_skin_get_browser_entries(xitk_skin_config_t *, const char *);
void xitk_skin_lock(xitk_skin_config_t *);
void xitk_skin_unlock(xitk_skin_config_t *);

void *labelbutton_get_user_data(xitk_widget_t *w);
void menu_auto_pop(xitk_widget_t *w);

int xitk_get_bool_value(const char *val);

struct xitk_font_s {
  Display       *display;
#ifdef WITH_XFT
  XftFont       *font;
#else
# ifdef WITH_XMB
  XFontSet       fontset;
# endif
  XFontStruct   *font;
#endif
  char          *name;
};

struct xitk_pixmap_s {
  ImlibData                        *imlibdata;
  XImage                           *xim;
  Pixmap                            pixmap;
  GC                                gc;
  XGCValues                         gcv;
  int                               width;
  int                               height;
  int                               shm;
#ifdef HAVE_SHM
  XShmSegmentInfo                  *shminfo;
#endif
  xitk_pixmap_destroyer_t           destroy;
};

typedef struct xitk_dialog_s xitk_dialog_t;

struct xitk_window_s {
  ImlibData                *imlibdata;
  Window                    window;
  Window                    win_parent;
  xitk_pixmap_t            *background;
  xitk_pixmap_t            *background_mask;
  int                       width;
  int                       height;

  xitk_widget_list_t       *widget_list;
};

struct xitk_dialog_s {
  ImlibData              *imlibdata;
  xitk_window_t          *xwin;
  xitk_widget_list_t     *widget_list;
  xitk_register_key_t     key;
  int                     type;

  xitk_widget_t          *w1, *w2, *w3;
  void                  (*done3cb)(void *userdata, int state);
  void                   *done3data;

  xitk_widget_t          *checkbox;
  xitk_widget_t          *checkbox_label;

  xitk_widget_t          *default_button;
};

Pixmap xitk_window_get_background(xitk_window_t *w);
Pixmap xitk_window_get_background_mask(xitk_window_t *w);


void xitk_register_eh_destructor (xitk_register_key_t key,
  void (*destructor)(void *userdata), void *destr_data);

void xitk_clipboard_unregister_widget (xitk_widget_t *w);
void xitk_clipboard_unregister_window (Window win);
/* text == NULL: just tell length.
 * *text == NULL: return live buf.
 * *text != NULL: copy there.
 * return -1: wait for WIDGET_EVENT_CLIP_READY, then try again. */
int xitk_clipboard_get_text (xitk_widget_t *w, char **text, int max_len);

#endif

