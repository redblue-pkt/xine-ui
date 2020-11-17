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

/* #define DEBUG_LOCKDISPLAY */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#include <X11/Xlib.h>

struct _ImlibData;

#include "libcommon.h"
#include "xitk.h"

typedef struct {
  uint32_t want, value;
  uint16_t r, g, b, a;
} xitk_color_info_t;

int xitk_color_db_query_value (xitk_t *xitk, xitk_color_info_t *info);

#include "_backend.h"

typedef struct xitk_font_cache_s xitk_font_cache_t;
struct xitk_tips_s;

struct xitk_s {
  xitk_backend_t *be;
  xitk_be_display_t *d, *d2;
  /* TODO: remove this kludge. */
  void    (*lock_display) (xitk_t *);
  void    (*unlock_display) (xitk_t *);
  struct _ImlibData *imlibdata;
  /* /TODO: remove this kludge. */
#ifdef DEBUG_LOCKDISPLAY
  pthread_mutex_t debug_mutex;
  int debug_level;
#endif
  xitk_font_cache_t *font_cache;
  struct xitk_tips_s *tips;
};

#ifdef DEBUG_LOCKDISPLAY
#  include <pthread.h>
#  define xitk_lock_display(_xitk_) do { \
     int _level_; \
     pthread_mutex_lock (&(_xitk_)->debug_mutex); \
     _level_ = ++(_xitk_)->debug_level; \
     pthread_mutex_unlock (&(_xitk_)->debug_mutex); \
     printf ("%s:%d %s (): getting display lock #%d...\n", __FILE__, __LINE__, __FUNCTION__, _level_); \
     (_xitk_)->lock_display (_xitk_); \
     printf ("%s:%d %s (): got display lock #%d.\n", __FILE__, __LINE__, __FUNCTION__, _level_); \
   } while (0)
#  define xitk_unlock_display(_xitk_) do { \
     int _level_; \
     pthread_mutex_lock (&(_xitk_)->debug_mutex); \
     _level_ = (_xitk_)->debug_level--; \
     pthread_mutex_unlock (&(_xitk_)->debug_mutex); \
     printf ("%s:%d %s (): freeing display lock #%d...\n", __FILE__, __LINE__, __FUNCTION__, _level_); \
     (_xitk_)->unlock_display (_xitk_); \
     printf ("%s:%d %s (): freed display lock #%d.\n", __FILE__, __LINE__, __FUNCTION__, _level_); \
   } while (0)
#else
#  define xitk_lock_display(_xitk_) (_xitk_)->lock_display (_xitk_)
#  define xitk_unlock_display(_xitk_) (_xitk_)->unlock_display (_xitk_)
#endif

#include "_config.h"
#include "image.h"
#include "skin.h"
#include "widget.h"
#include "window.h"
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

#define DIRECTION_UP     1
#define DIRECTION_DOWN   2
#define DIRECTION_LEFT   3
#define DIRECTION_RIGHT  4

#define CHECK_STYLE_OLD   1
#define CHECK_STYLE_CHECK 2
#define CHECK_STYLE_ROUND 3

int xitk_install_x_error_handler (xitk_t *xitk);
int xitk_uninstall_x_error_handler (xitk_t *xitk);

const char *xine_get_homedir(void);
void xitk_usec_sleep(unsigned long);
int xitk_system(int dont_run_as_root, const char *command);
int xitk_is_use_xshm (xitk_t *xitk);

void xitk_set_xmb_enability(xitk_t *, int value);

char *xitk_filter_filename(const char *name);
void xitk_set_current_menu(xitk_t *, xitk_widget_t *menu);
void xitk_unset_current_menu(xitk_t *);
void xitk_set_tips_timeout(xitk_t *, unsigned long timeout);

const char *xitk_skin_get_logo(xitk_skin_config_t *);
const char *xitk_skin_get_animation(xitk_skin_config_t *);
void xitk_skin_lock(xitk_skin_config_t *);
void xitk_skin_unlock(xitk_skin_config_t *);

void *labelbutton_get_user_data(xitk_widget_t *w);
void menu_auto_pop(xitk_widget_t *w);

int xitk_get_bool_value(const char *val);

struct xitk_image_s {
  xitk_be_image_t *beimg;
  xitk_pix_font_t *pix_font;
  int width, height;
  /* image private */
  xitk_t *xitk;
  xitk_widget_list_t *wl;
  int refs, max_refs;
  char key[32];
};

void xitk_image_ref (xitk_image_t *img);

struct xitk_window_s {
  xitk_t                   *xitk;
  xitk_be_window_t         *bewin;
  xitk_image_t             *bg_image;
  xitk_window_t            *win_parent;
  int                       width;
  int                       height;
  xitk_wm_window_type_t     type;
  xitk_window_role_t        role;
  uint32_t                  flags;
  xitk_widget_list_t       *widget_list;
  /* to be removed */
  Window                    window;
};

void xitk_window_update_tree (xitk_window_t *xwin, uint32_t mask_and_flags);

xitk_widget_list_t *xitk_widget_list_get (xitk_t *xitk, xitk_window_t *xwin);
void xitk_register_eh_destructor (xitk_t *xitk, xitk_register_key_t key,
  void (*destructor)(void *userdata), void *destr_data);


void xitk_clipboard_unregister_widget (xitk_widget_t *w);
/* text == NULL: just tell length.
 * *text == NULL: return live buf.
 * *text != NULL: copy there.
 * return -1: wait for WIDGET_EVENT_CLIP_READY, then try again. */
int xitk_clipboard_get_text (xitk_widget_t *w, char **text, int max_len);

void xitk_sync(xitk_t *); /* XSync() wrapper */

#endif
