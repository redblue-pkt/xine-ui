/*
 * Copyright (C) 2000-2021 the xine project
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

#include "libcommon.h"
#include "xitk.h"

static inline int xitk_min (int a, int b) {
  int d = b - a;
  return a + (d & (d >> (8 * sizeof (d) - 1)));
}

static inline int xitk_max (int a, int b) {
  int d = a - b;
  return a - (d & (d >> (8 * sizeof (d) - 1)));
}

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
  xitk_font_cache_t *font_cache;
  struct xitk_tips_s *tips;
  unsigned int tips_timeout;
  int verbosity; /** << 0 (quiet), 1 (base, errors), 2 (debug) */
};

#include "_config.h"
#include "skin.h"
#include "widget.h"
#include "xitkintl.h"

#if !defined(__GNUC__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define	__FUNCTION__	__func__
#endif

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

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

const char *xine_get_homedir(void);
void xitk_usec_sleep(unsigned long);
int xitk_system(int dont_run_as_root, const char *command);

void xitk_set_xmb_enability(xitk_t *, int value);

void xitk_set_current_menu(xitk_t *, xitk_widget_t *menu);
void xitk_unset_current_menu(xitk_t *);

void *labelbutton_get_user_data(xitk_widget_t *w);
void menu_auto_pop(xitk_widget_t *w);

int xitk_get_bool_value(const char *val);

typedef struct {
  int                               red;
  int                               green;
  int                               blue;
  char                              colorname[20];
} xitk_color_names_t;

xitk_color_names_t *xitk_get_color_name (xitk_color_names_t *cn, const char *color);

typedef struct {
  int                               width;
  int                               height;
  int                               chars_per_row;
  int                               chars_total;
  int                               char_width;
  int                               char_height;
  xitk_point_t                      space;
  xitk_point_t                      asterisk;
  xitk_point_t                      unknown;
#define XITK_MAX_UNICODE_RANGES 16
  xitk_range_t                      unicode_ranges[XITK_MAX_UNICODE_RANGES + 1];
} xitk_pix_font_t;

typedef enum {
  /* states from left to right */
  XITK_IMG_STATE_NORMAL = 0,
  XITK_IMG_STATE_FOCUS,
  XITK_IMG_STATE_SELECTED,
  XITK_IMG_STATE_SEL_FOCUS,
  XITK_IMG_STATE_DISABLED_NORMAL,
  XITK_IMG_STATE_DISABLED_SELECTED,
  XITK_IMG_STATE_LAST
} xitk_img_state_t;

xitk_img_state_t xitk_image_find_state (xitk_img_state_t max, int enable, int focus, int click, int selected);

struct xitk_image_s {
  xitk_be_image_t *beimg;
  xitk_pix_font_t *pix_font;
  int width, height;
  xitk_img_state_t last_state;
  /* image private */
  xitk_t *xitk;
  xitk_widget_list_t *wl;
  int refs, max_refs;
  char key[32];
};

void xitk_image_ref (xitk_image_t *img);

#define STYLE_FLAT     1
#define STYLE_BEVEL    2

#define ALIGN_LEFT    1
#define ALIGN_CENTER  2
#define ALIGN_RIGHT   3
#define ALIGN_DEFAULT (ALIGN_LEFT)

#define TABULATION_SIZE 6 /* number of chars inserted in place of a tabulation */

int xitk_shared_image (xitk_widget_list_t *wl, const char *key, int width, int height, xitk_image_t **image);
void xitk_shared_image_list_delete (xitk_widget_list_t *wl);

struct xitk_window_s {
  xitk_t                   *xitk;
  xitk_be_window_t         *bewin;
  xitk_image_t             *bg_image;
  int                       width;
  int                       height;
  xitk_wm_window_type_t     type;
  xitk_window_role_t        role;
  xitk_register_key_t       key;
  uint32_t                  flags;
  xitk_widget_list_t       *widget_list;
};

void xitk_window_update_tree (xitk_window_t *xwin, uint32_t mask_and_flags);
xitk_window_t *xitk_window_wrap_native_window (xitk_t *xitk /* may be null if be_display given */,
                                               xitk_be_display_t *be_display /* may be NULL */,
                                               uintptr_t window);

xitk_widget_list_t *xitk_widget_list_get (xitk_t *xitk, xitk_window_t *xwin);

xitk_register_key_t xitk_get_focus_key (xitk_t *xitk);
xitk_register_key_t xitk_set_focus_key (xitk_t *xitk, xitk_register_key_t key, int focus);

/*
 * Helper function to free widget list inside callbacks.
 */
void xitk_widget_list_defferred_destroy(xitk_widget_list_t *wl);

/* A few shortcuts for internal use. */
#ifndef XITK_WIDGET_C
#  define xitk_get_widget_width(w) ((w) ? (w)->width : 0)
#  define xitk_get_widget_height(w) ((w) ? (w)->height : 0)
#  define xitk_get_widget_pos(w,px,py) if (w) { *px = (w)->x; *py = (w)->y; } else { *px = 0; *py = 0; }
#endif
#ifndef XITK_WINDOW_C
#  define xitk_window_get_background_image(_w) ((_w) ? (_w)->bg_image : NULL)
#  define xitk_window_set_input_focus(_w) xitk_window_flags (_w, XITK_WINF_FOCUS, XITK_WINF_FOCUS)
#endif
#ifndef _XITK_C_
#  define xitk_get_display_size(xitk,pw,ph) *(pw) = (xitk)->d->width, *(ph) = (xitk)->d->height
#  define xitk_get_wm_type(xitk) ((xitk)->d->wm_type)
#endif

#define xitk_widget_state_from_info(_w,info) do { \
  (_w)->state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE); \
  (_w)->state |= (info) && (info)->visibility ? XITK_WIDGET_STATE_VISIBLE : 0; \
  (_w)->state |= (info) && (info)->enability ? XITK_WIDGET_STATE_ENABLE : 0; \
  (_w)->saved_state = (_w)->state; \
} while (0)

void xitk_clipboard_unregister_widget (xitk_widget_t *w);
/* text == NULL: just tell length.
 * *text == NULL: return live buf.
 * *text != NULL: copy there.
 * return -1: wait for WIDGET_EVENT_CLIP_READY, then try again. */
int xitk_clipboard_get_text (xitk_widget_t *w, char **text, int max_len);

#endif
