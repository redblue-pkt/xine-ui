/*
 * Copyright (C) 2000-2022 the xine project
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
 * Unique public header file for xitk µTK.
 *
 */

#ifndef _XITK_H_
#define _XITK_H_

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define XITK_MAJOR_VERSION          (0)
#define XITK_MINOR_VERSION          (11)
#define XITK_SUB_VERSION            (0)
#define XITK_VERSION                "0.11.0"

#define XITK_CHECK_VERSION(major, minor, sub)                          \
                                    (XITK_MAJOR_VERSION > (major) ||   \
                                     (XITK_MAJOR_VERSION == (major) && \
                                      XITK_MINOR_VERSION > (minor)) || \
                                     (XITK_MAJOR_VERSION == (major) && \
                                      XITK_MINOR_VERSION == (minor) && \
                                      XITK_SUB_VERSION >= (sub)))

#define XITK_WIDGET_MAGIC           0x7869746b

#ifndef NAME_MAX
#define XITK_NAME_MAX               256
#else
#define XITK_NAME_MAX               NAME_MAX
#endif

#ifndef PATH_MAX
#define XITK_PATH_MAX               768
#else
#define XITK_PATH_MAX               PATH_MAX
#endif

/* paranoia #1: error if *cont_ptr does not have an element elem_name.
 * paranoia #2: warn if it has wrong type. */
#define xitk_container(cont_ptr,elem_ptr,elem_name) do { \
  const typeof (elem_ptr) xc_dummy = &((const typeof (cont_ptr))0)->elem_name; \
  cont_ptr = (elem_ptr) ? (typeof (cont_ptr))(void *)((uint8_t *)(elem_ptr) - (uintptr_t)xc_dummy) : NULL; \
} while (0)

size_t xitk_lower_strlcpy (char *dest, const char *src, size_t dlen);

static inline uint32_t xitk_find_byte (const char *s, uint32_t byte) {
  const uint32_t eor = ~((byte << 24) | (byte << 16) | (byte << 8) | byte);
  const uint32_t left = (uintptr_t)s & 3;
  const uint32_t *p = (const uint32_t *)(s - left);
  static const union {
    uint8_t b[4];
    uint32_t v;
  } mask[4] = {
    {{0xff, 0xff, 0xff, 0xff}},
    {{0x00, 0xff, 0xff, 0xff}},
    {{0x00, 0x00, 0xff, 0xff}},
    {{0x00, 0x00, 0x00, 0xff}},
  };
  static const uint8_t rest[32] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, /* big wndian */
    0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4  /* little endian */
  };
  const union {
    uint32_t v;
    uint8_t b[4];
  } endian = {16};
  uint32_t w = (*p++ ^ eor) & mask[left].v;
  while (1) {
    w = w & 0x80808080 & ((w & 0x7f7f7f7f) + 0x01010101);
    if (w)
      break;
    w = *p++ ^ eor;
  }
  /* bits 31, 23, 15, 7 -> 3, 2, 1, 0 */
  w = (w * 0x00204081) & 0xffffffff;
  w >>= 28;
  return ((const char *)p - s) - rest[endian.b[0] + w];
}

typedef struct {
  char *s, buf[64];
} xitk_short_string_t;
static inline void xitk_short_string_init (xitk_short_string_t *s) {
  s->s = s->buf;
  s->buf[0] = 0;
}
/* return strlen (v), or (size_t)-1 if string is unchanged. */
size_t xitk_short_string_set (xitk_short_string_t *s, const char *v);
static inline void xitk_short_string_deinit (xitk_short_string_t *s) {
  if (s->s != s->buf) {
    free (s->s);
    s->s = NULL;
  }
}

/* the versioned string feature. */
typedef struct {
  char *s;
  unsigned int bsize;   /** << if not 0, s points to a user allocated buf of this size. */
  int          version; /** << how many changes since init. */
} xitk_vers_string_t;
void xitk_vers_string_init (xitk_vers_string_t *vs, char *user_buf, size_t user_bsize);
/* return whether or not there was a change. */
int xitk_vers_string_set (xitk_vers_string_t *vs, const char *s);
int xitk_vers_string_get (xitk_vers_string_t *to, const xitk_vers_string_t *from);
void xitk_vers_string_deinit (xitk_vers_string_t *vs);

/* this will pad the return with 8 zero bytes both before and after.
 * filesize, if set, is the size limit before and the actual size after the call. */
char *xitk_cfg_load (const char *filename, size_t *filesize);
void xitk_cfg_unload (char *buf);

typedef struct {
  /* 0 for root node tree[0] */
  short int level;
  /* byte length of key */
  unsigned short int klen;
  /* array index. */
  int next, prev, parent, first_child, last_child;
  /* root node: total count of nodes.
   * otherwise: offset into contents, converted to lowercase, or -1. */
  int key;
  /* offset into contents, or -1. */
  int value;
} xitk_cfg_parse_t;

#define XITK_CFG_PARSE_DEBUG 1 /* show extra info */
#define XITK_CFG_PARSE_CASE 2 /* dont lowercase keys */
xitk_cfg_parse_t *xitk_cfg_parse (char *contents, int flags);
void xitk_cfg_unparse (xitk_cfg_parse_t *tree);

/* supports "#<hex>", "0x<hex>". "0<octal", "<decimal>". */
int32_t xitk_str2int32 (const char **str);

/* supports some real names, "#rrggbb", "0xrrggbb".
 * returns (red << 16) | (green << 8) | blue, or ~0u. */
uint32_t xitk_get_color_name (const char *color);
/* special color values for xitk_*_noskin_create () */
#define XITK_NOSKIN_DEFAULT   0x80000000
#define XITK_NOSKIN_TEXT_NORM 0x80000001
#define XITK_NOSKIN_TEXT_INV  0x80000002

typedef struct xitk_s xitk_t;

typedef struct xitk_widget_s xitk_widget_t;
typedef struct xitk_widget_list_s xitk_widget_list_t;
typedef struct xitk_skin_config_s xitk_skin_config_t;
typedef struct xitk_skin_element_info_s xitk_skin_element_info_t;
typedef struct xitk_be_font_s xitk_font_t;
typedef struct xitk_image_s xitk_image_t;
typedef struct xitk_window_s xitk_window_t;

typedef void (*xitk_startup_callback_t)(void *);
typedef void (*xitk_simple_callback_t)(xitk_widget_t *, void *);
typedef void (*xitk_state_callback_t)(xitk_widget_t *, void *, int);
typedef void (*xitk_ext_state_callback_t)(xitk_widget_t *, void *, int, int modifiers);
typedef void (*xitk_state_double_callback_t)(xitk_widget_t *, void *, double);
typedef void (*xitk_string_callback_t)(xitk_widget_t *, void *, const char *);
typedef void (*xitk_dnd_callback_t) (void *data, const char *filename);

typedef struct {
  int x, y, width, height;
} xitk_rect_t;

/*
 * New positioning window callback function.
 * This callback will be called when the window
 * moving will be done (at button release time),
 * and, of course, only if there was a window
 * specified at register time.
 */
typedef void (*widget_newpos_callback_t)(void *data, int x, int y, int width, int height);

/*
 * Signal handler callback function.
 * Xitk will call this function when a signal happen.
 */
typedef void (*xitk_signal_callback_t)(int, void *);

/*
 * A unique key returned by register function.
 * It is necessary to store it at program side,
 * because it will be necessary to pass it for
 * identification of caller.
 */
typedef int xitk_register_key_t;

int xitk_widget_key_event (xitk_widget_t *w, const char *string, int modifier, int key_up);

/*
 *
 */

typedef struct {
  int x, y;
} xitk_point_t;

typedef struct {
  int first, last;
} xitk_range_t;

typedef struct {
  xitk_window_t                    *xwin;
  int                               x;
  int                               y;
  int                               height;
  int                               width;
  char                              name[64];
} window_info_t;

typedef struct {
  xitk_image_t *image;
  int x, y, width, height;
} xitk_part_image_t;

/* Group of widgets widget */
#define WIDGET_GROUP                0x00000800
/* Grouped widget, itself */
#define WIDGET_GROUP_MEMBER         0x80000000
/* Does widget take part in tab cycle */
#define WIDGET_TABABLE              0x40000000
/* Is widget focusable */
#define WIDGET_FOCUSABLE            0x20000000
/* Is widget clickable */
#define WIDGET_CLICKABLE            0x10000000
/* Widget keeps focus after tab/click */
#define WIDGET_KEEP_FOCUS           0x08000000
/* Widget support key events */
#define WIDGET_KEYABLE              0x04000000
/* Widget support partial repaint */
#define WIDGET_PARTIAL_PAINTABLE    0x02000000

/* Grouped widgets */
#define WIDGET_GROUP_MASK           0x01fff000
#define WIDGET_GROUP_BROWSER        0x00001000
#define WIDGET_GROUP_MRLBROWSER     0x00002000
#define WIDGET_GROUP_COMBO          0x00004000
#define WIDGET_GROUP_TABS           0x00008000
#define WIDGET_GROUP_INTBOX         0x00010000
#define WIDGET_GROUP_DOUBLEBOX      0x00020000
#define WIDGET_GROUP_MENU           0x00040000
#define WIDGET_GROUP_BUTTON_LIST    0x00080000

#define WIDGET_TYPE_MASK            0x00000fff
/* Group leaders.. */
#define WIDGET_TYPE_COMBO           0x00000801
#define WIDGET_TYPE_DOUBLEBOX       0x00000802
#define WIDGET_TYPE_INTBOX          0x00000803
#define WIDGET_TYPE_BROWSER         0x00000804
#define WIDGET_TYPE_MRLBROWSER      0x00000805
#define WIDGET_TYPE_TABS            0x00000806
#define WIDGET_TYPE_MENU            0x00000807
#define WIDGET_TYPE_BUTTON_LIST     0x00000808
/* Real widgets. */
#define WIDGET_TYPE_BUTTON          0x00000001
#define WIDGET_TYPE_LABELBUTTON     0x00000002
#define WIDGET_TYPE_LABEL           0x00000003
#define WIDGET_TYPE_SLIDER          0x00000004
#define WIDGET_TYPE_CHECKBOX        0x00000005
#define WIDGET_TYPE_IMAGE           0x00000006
#define WIDGET_TYPE_INPUTTEXT       0x00000007

/* See */
#define ALIGN_LEFT                  1
#define ALIGN_CENTER                2
#define ALIGN_RIGHT                 3
#define ALIGN_DEFAULT               (ALIGN_LEFT)

/*
 * See xitk_get_modifier_key()
 */
#define MODIFIER_NOMOD              0x00000000
#define MODIFIER_SHIFT              0x00000001
#define MODIFIER_LOCK               0x00000002
#define MODIFIER_CTRL               0x00000004
#define MODIFIER_META               0x00000008
#define MODIFIER_NUML               0x00000010
#define MODIFIER_MOD3               0x00000020
#define MODIFIER_MOD4               0x00000040
#define MODIFIER_MOD5               0x00000080
#define MODIFIER_BUTTON1            0x00000100
#define MODIFIER_BUTTON2            0x00000200
#define MODIFIER_BUTTON3            0x00000400
#define MODIFIER_BUTTON4            0x00000800
#define MODIFIER_BUTTON5            0x00001000

/*
 * Result of dialog window
 */
#define XITK_WINDOW_ANSWER_UNKNOWN  0
#define XITK_WINDOW_ANSWER_OK       1
#define XITK_WINDOW_ANSWER_YES      2
#define XITK_WINDOW_ANSWER_NO       3
#define XITK_WINDOW_ANSWER_CANCEL   4

/*
 * See xitk_get_wm_type()
 */
#define WM_TYPE_COMP_MASK           0x00007FFF
#define WM_TYPE_UNKNOWN             0x00000000
#define WM_TYPE_GNOME_COMP          0x80000000
#define WM_TYPE_EWMH_COMP           0x40000000
#define WM_TYPE_KWIN                0x00000001
#define WM_TYPE_E                   0x00000002
#define WM_TYPE_ICE                 0x00000003
#define WM_TYPE_WINDOWMAKER         0x00000004
#define WM_TYPE_MOTIF               0x00000005
#define WM_TYPE_XFCE                0x00000006
#define WM_TYPE_SAWFISH             0x00000007
#define WM_TYPE_METACITY            0x00000008
#define WM_TYPE_AFTERSTEP           0x00000009
#define WM_TYPE_BLACKBOX            0x0000000A
#define WM_TYPE_LARSWM              0x0000000B
#define WM_TYPE_DTWM                0x0000000C

typedef enum {
  WINDOW_TYPE_NONE = 0,
  WINDOW_TYPE_DESKTOP,
  WINDOW_TYPE_DOCK,
  WINDOW_TYPE_TOOLBAR,
  WINDOW_TYPE_MENU,
  WINDOW_TYPE_UTILITY,
  WINDOW_TYPE_SPLASH,
  WINDOW_TYPE_DIALOG,
  //WINDOW_TYPE_DROPDOWN_MENU,
  //WINDOW_TYPE_POPUP_MENU,
  //WINDOW_TYPE_TOOLTIP,
  //WINDOW_TYPE_NOTIFICATION,
  //WINDOW_TYPE_COMBO,
  WINDOW_TYPE_DND,
  WINDOW_TYPE_NORMAL,
  WINDOW_TYPE_END
} xitk_wm_window_type_t;

void xitk_window_set_wm_window_type (xitk_window_t *w, xitk_wm_window_type_t type);

typedef enum {
  xitk_cursor_invisible = 0,
  xitk_cursor_X_cursor,
  xitk_cursor_arrow,
  xitk_cursor_based_arrow_down,
  xitk_cursor_based_arrow_up,
  xitk_cursor_boat,
  xitk_cursor_bogosity,
  xitk_cursor_bottom_left_corner,
  xitk_cursor_bottom_right_corner,
  xitk_cursor_bottom_side,
  xitk_cursor_bottom_tee,
  xitk_cursor_box_spiral,
  xitk_cursor_center_ptr,
  xitk_cursor_circle,
  xitk_cursor_clock,
  xitk_cursor_coffee_mug,
  xitk_cursor_cross,
  xitk_cursor_cross_reverse,
  xitk_cursor_crosshair,
  xitk_cursor_diamond_cross,
  xitk_cursor_dot,
  xitk_cursor_dotbox,
  xitk_cursor_double_arrow,
  xitk_cursor_draft_large,
  xitk_cursor_draft_small,
  xitk_cursor_draped_box,
  xitk_cursor_exchange,
  xitk_cursor_fleur,
  xitk_cursor_gobbler,
  xitk_cursor_gumby,
  xitk_cursor_hand1,
  xitk_cursor_hand2,
  xitk_cursor_heart,
  xitk_cursor_icon,
  xitk_cursor_iron_cross,
  xitk_cursor_left_ptr,
  xitk_cursor_left_side,
  xitk_cursor_left_tee,
  xitk_cursor_leftbutton,
  xitk_cursor_ll_angle,
  xitk_cursor_lr_angle,
  xitk_cursor_man,
  xitk_cursor_middlebutton,
  xitk_cursor_mouse,
  xitk_cursor_pencil,
  xitk_cursor_pirate,
  xitk_cursor_plus,
  xitk_cursor_question_arrow,
  xitk_cursor_right_ptr,
  xitk_cursor_right_side,
  xitk_cursor_right_tee,
  xitk_cursor_rightbutton,
  xitk_cursor_rtl_logo,
  xitk_cursor_sailboat,
  xitk_cursor_sb_down_arrow,
  xitk_cursor_sb_h_double_arrow,
  xitk_cursor_sb_left_arrow,
  xitk_cursor_sb_right_arrow,
  xitk_cursor_sb_up_arrow,
  xitk_cursor_sb_v_double_arrow,
  xitk_cursor_shuttle,
  xitk_cursor_sizing,
  xitk_cursor_spider,
  xitk_cursor_spraycan,
  xitk_cursor_star,
  xitk_cursor_target,
  xitk_cursor_tcross,
  xitk_cursor_top_left_arrow,
  xitk_cursor_top_left_corner,
  xitk_cursor_top_right_corner,
  xitk_cursor_top_side,
  xitk_cursor_top_tee,
  xitk_cursor_trek,
  xitk_cursor_ul_angle,
  xitk_cursor_umbrella,
  xitk_cursor_ur_angle,
  xitk_cursor_watch,
  xitk_cursor_xterm,
  xitk_cursor_num_glyphs
} xitk_cursors_t;

#define XITK_WIDGET_INIT(X)         do {                              \
                                      (X)->magic = XITK_WIDGET_MAGIC; \
                                    } while(0)

/* *******
 * INIT: widget lib initialization and friends
 */

/*
 * Register a callback function called when a signal heppen.
 */
void xitk_register_signal_handler(xitk_t *, xitk_signal_callback_t sigcb, void *user_data);

/*
 * Remove widgetkey_t entry in internal table.
 */
void xitk_unregister_event_handler (xitk_t *xitk, xitk_register_key_t *key);

/*
 * Copy window information matching with key in passed window_info_t struct.
 */

int xitk_get_window_info (xitk_t *xitk, xitk_register_key_t key, window_info_t *winf);

xitk_window_t *xitk_get_window (xitk_t *xitk, xitk_register_key_t key);

int       xitk_window_get_backend_type(xitk_window_t *xwin);
uintptr_t xitk_window_get_native_id(xitk_window_t *xwin);
uintptr_t xitk_window_get_native_display_id(xitk_window_t *xwin);

/*
 * Initialization function, should be the first call to widget lib.
 */
xitk_t *xitk_init(const char *prefered_visual, int install_colormap,
                  int use_x_lock_display, int use_synchronized_x, int verbosity);

void xitk_free(xitk_t **);

/*
 *
 */
const char *xitk_set_locale(void);

/*
 *
 */

long int xitk_reset_screen_saver(xitk_t *xitk, long int timeout);
int xitk_change_video_mode(xitk_t *xitk, xitk_window_t *w, int min_width, int min_height);

void xitk_get_display_size (xitk_t *xitk, int *w, int *h);
double xitk_get_display_ratio (xitk_t *xitk);

/*
 *
 */
int xitk_get_layer_level(xitk_t *xitk);

/*
 * Return WM_TYPE_*
 */
uint32_t xitk_get_wm_type (xitk_t *xitk);

/*
 *
 */
void xitk_window_set_window_layer(xitk_window_t *w, int layer);
void xitk_window_set_layer_above(xitk_window_t *w);

/*
 * This function start the widget live. It's a block function,
 * it will only return after a widget_stop() call.
 */
void xitk_run (xitk_t *xitk, void (* start_cb)(void *data), void *start_data,
  void (* stop_cb)(void *data), void *stop_data);

/*
 * This function terminate the widget lib loop event.
 * Other functions of the lib shouldn't be called after this
 * one.
 */
void xitk_stop (xitk_t *xitk);

/*
 * Some user settings values.
 */
typedef enum {
  XITK_SYSTEM_FONT = 1,
  XITK_DEFAULT_FONT,
  XITK_XMB_ENABLE,
  XITK_SHM_ENABLE,
  XITK_MENU_SHORTCUTS_ENABLE,
  XITK_BLACK_COLOR,
  XITK_DISABLED_BLACK_COLOR,
  XITK_WHITE_COLOR,
  XITK_DISABLED_WHITE_COLOR,
  XITK_BG_COLOR,
  XITK_FOCUS_COLOR,
  XITK_SELECT_COLOR,
  XITK_SEL_FOCUS_COLOR,
  XITK_WARN_BG_COLOR,
  XITK_WARN_FG_COLOR,
  XITK_BAR_STYLE,
  XITK_CHECK_STYLE,
  XITK_CURSORS_FEATURE,
  XITK_TIPS_TIMEOUT,
  XITK_TIMER_LABEL_ANIM,
  XITK_DBL_CLICK_TIME,
  XITK_CFG_END
} xitk_cfg_item_t;

const char *xitk_get_cfg_string (xitk_t *xitk, xitk_cfg_item_t item);
int xitk_get_cfg_num (xitk_t *xitk, xitk_cfg_item_t item);

char *xitk_filter_filename(const char *name);
struct timeval;
int xitk_is_dbl_click (xitk_t *xitk, const struct timeval *t1, const struct timeval *t2);


/*
 *
 ****** */

int xitk_get_bool_value (const char *val);

/**
 * Allocate an clean memory of "size" bytes.
 */
void *xitk_xmalloc(size_t);


/**
 * (re)Paint a widget list.
 */
int xitk_paint_widget_list (xitk_widget_list_t *wl) ;
typedef struct {
  int x1, x2, y1, y2;
} xitk_hull_t;
/* Behold the master of partial arts ;-) */
int xitk_partial_paint_widget_list (xitk_widget_list_t *wl, xitk_hull_t *hull);

/**
 *
 */
void xitk_change_skins_widget_list(xitk_widget_list_t *wl, xitk_skin_config_t *skonfig);

/**
 * Return the focused widget.
 */
xitk_widget_t *xitk_get_focused_widget(xitk_widget_list_t *);

/**
 * Target mouse and tab focus to another widget.
 */
void xitk_widget_set_focus_redirect (xitk_widget_t *w, xitk_widget_t *focus_redirect);

/**
 * Force the focus to given widget.
 */
void xitk_set_focus_to_widget(xitk_widget_t *);

/**
 * Return width (in pixel) of widget.
 */
int xitk_get_widget_width(xitk_widget_t *);

/**
 * Return height (in pixel) of widget.
 */
int xitk_get_widget_height(xitk_widget_t *);

/**
 * Set position of a widget.
 */
int xitk_set_widget_pos(xitk_widget_t *w, int x, int y);

/**
 * Get position of a widget.
 */
int xitk_get_widget_pos(xitk_widget_t *w, int *x, int *y);

/**
 *
 */
uint32_t xitk_get_widget_type(xitk_widget_t *w);

/** use XITK_INT_KEEP to just query. */
int xitk_widget_select (xitk_widget_t *w, int index);

/**
 * Set widgets of widget list visible.
 */
void xitk_show_widgets(xitk_widget_list_t *);

/**
 * Set widgets of widget list not visible.
 */
void xitk_hide_widgets(xitk_widget_list_t *);

/**
 *
 */
xitk_image_t *xitk_get_widget_foreground_skin(xitk_widget_t *w);

/* xitk_add_widget (): keep state as is */
#define XITK_WIDGET_STATE_KEEP (~0u)
/* widget responds to user input */
#define XITK_WIDGET_STATE_ENABLE 1
/* widget is shown */
#define XITK_WIDGET_STATE_VISIBLE 2
/* left mouse button held on this */
#define XITK_WIDGET_STATE_CLICK 4
/* widget is "on" (radio button etc.) */
#define XITK_WIDGET_STATE_ON 8
/* fire on mouse/enter/space press instead of release. */
#define XITK_WIDGET_STATE_IMMEDIATE 16
/* widget can be "on". */
#define XITK_WIDGET_STATE_TOGGLE 32
void xitk_add_widget (xitk_widget_list_t *wl, xitk_widget_t *wi, unsigned int flags);
/* returns the new state of last done widget. */
unsigned int xitk_widgets_state (xitk_widget_t * const *w, unsigned int n, unsigned int mask, unsigned int state);
#define xitk_is_widget_enabled(_w) (!!(xitk_widgets_state (&(_w), 1, 0, 0) & XITK_WIDGET_STATE_ENABLE))
void xitk_widgets_delete (xitk_widget_t **w, unsigned int n);
#define xitk_destroy_widget(w) do {xitk_widget_t *__w = w; xitk_widgets_delete (&(__w), 1); } while (0)

#define XITK_TIPS_STRING_KEEP ((const char *)1)
#define XITK_TIPS_TIMEOUT_OFF 0
#define XITK_TIPS_TIMEOUT_AUTO 1
/* other values are milliseconds. */
void xitk_set_tips_timeout (xitk_t *xitk, unsigned int timeout);
void xitk_set_widgets_tips_timeout (xitk_widget_list_t *wl, unsigned int timeout);
void xitk_set_widget_tips_and_timeout (xitk_widget_t *w, const char *str, unsigned int timeout);
unsigned int xitk_get_widget_tips_timeout (xitk_widget_t *w);
#define xitk_disable_widgets_tips(wl) xitk_set_widgets_tips_timeout (wl, XITK_TIPS_TIMEOUT_OFF)
#define xitk_disable_widget_tips(w) xitk_set_widget_tips_and_timeout (w, XITK_TIPS_STRING_KEEP, XITK_TIPS_TIMEOUT_OFF)
#define xitk_enable_widget_tips(w) xitk_set_widget_tips_and_timeout (w, XITK_TIPS_STRING_KEEP, XITK_TIPS_TIMEOUT_AUTO)
#define xitk_set_widget_tips(w,s) xitk_set_widget_tips_and_timeout (w, s, XITK_TIPS_TIMEOUT_AUTO)

/*
 * *** Image
 */

/**
 * -2: return max; -1: return current; 0..max: return redraw_needed.
 */
int xitk_image_quality (xitk_t *xitk, int qual);

/**
 * Load image and return a xitk_image_t data type.
 * if (data != NULL):
 * dsize > 0: decode (down)loaded image file contents,
 *            and aspect preserving scale to fit w and/or h if set.
 * dsize == 0: read from named file, then same as above.
 * dsize == -1: use raw data as (w, h). */
xitk_image_t *xitk_image_new (xitk_t *xitk, const char *data, int dsize, int w, int h);
void xitk_image_set_pix_font (xitk_image_t *image, const char *format);
int xitk_image_inside (xitk_image_t *img, int x, int y);
void xitk_image_copy_rect (xitk_image_t *from, xitk_image_t *to, int x1, int y1, int w, int h, int x2, int y2);

int xitk_image_width(xitk_image_t *);
int xitk_image_height(xitk_image_t *);

/**
 *
 */
void xitk_image_copy (xitk_image_t *from, xitk_image_t *to);

/**
 * Font manipulations.
 */
/*
 *
 */
xitk_font_t *xitk_font_load_font(xitk_t *xitk, const char *font);

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs);

/*
 *
 */
void xitk_image_draw_string (xitk_image_t *img, xitk_font_t *xtfs, int x, int y, const char *text, size_t nbytes, int color);

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int nbytes);

/*
 *
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c);

/*
 *
 */
#ifdef YET_UNUSED
int xitk_font_get_char_width(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes);
int xitk_font_get_ascent(xitk_font_t *xtfs, const char *c);
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c);
int xitk_font_get_char_height(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);
#endif

/*
 *
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c);

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			    int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/**
 *
 */
uint32_t xitk_color_db_get (xitk_t *_xitk, uint32_t rgb);
void xitk_color_db_flush (xitk_t *_xitk);

/**
 * xitk image
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string (xitk_t *xitk,
  const char *fontname, int width, int pad_x, int pad_y, int align, const char *str,
  unsigned int foreground, unsigned int background);
xitk_image_t *xitk_image_create_image_from_string(xitk_t *xitk,
                                                  const char *fontname,
                                                  int width, int align, const char *str);
void xitk_image_draw_image (xitk_widget_list_t *wl, xitk_image_t *im,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y, int sync);
void xitk_part_image_draw (xitk_widget_list_t *wl, xitk_part_image_t *origin, xitk_part_image_t *copy,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y);
void xitk_part_image_copy (xitk_widget_list_t *wl, xitk_part_image_t *from, xitk_part_image_t *to,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y);

/**
 * Free an image object.
 */
void xitk_image_free_image(xitk_image_t **src);

/**
 *
 */
void xitk_image_draw_line (xitk_image_t *img, int x0, int y0, int x1, int y1, unsigned color);
void xitk_image_fill_polygon (xitk_image_t *img, const xitk_point_t *points, int npoints, unsigned color);
void xitk_image_draw_rectangle (xitk_image_t *img, int x, int y, int w, int h, unsigned int color);
void xitk_image_fill_rectangle (xitk_image_t *img, int x, int y, int w, int h, unsigned int color);

void xitk_image_draw_flat (xitk_image_t *img, int w, int h);
void xitk_image_draw_flat_three_state (xitk_image_t *img);

void xitk_image_draw_bevel_three_state (xitk_image_t *img);
void xitk_image_draw_bevel_two_state (xitk_image_t *img);

void xitk_image_draw_tab (xitk_image_t *img);

void xitk_image_draw_paddle_three_state (xitk_image_t *img, int width, int height);
void xitk_image_draw_paddle_three_state_vertical (xitk_image_t *img);
void xitk_image_draw_paddle_three_state_horizontal (xitk_image_t *img);
void xitk_image_draw_paddle_rotate (xitk_image_t *img);

void xitk_image_draw_rotate_button (xitk_image_t *img);

void xitk_image_draw_arrow_up (xitk_image_t *img);
void xitk_image_draw_arrow_down (xitk_image_t *img);
void xitk_image_draw_arrow_left (xitk_image_t *img);
void xitk_image_draw_arrow_right (xitk_image_t *img);

void xitk_image_draw_button_plus (xitk_image_t *img);
void xitk_image_draw_button_minus (xitk_image_t *img);

void xitk_image_draw_menu_check (xitk_image_t *img, int checked);
void xitk_image_draw_menu_arrow_branch (xitk_image_t *img);

void xitk_image_draw_outter (xitk_image_t *img, int w, int h);
void xitk_image_draw_inner (xitk_image_t *img, int w, int h);

#define DRAW_INNER     1
#define DRAW_OUTTER    2
#define DRAW_FLATTER   3
/* with DRAW_INNER/DRAW_OUTTER */
#define DRAW_DOUBLE    8
#define DRAW_LIGHT    16
void xitk_image_draw_rectangular_box (xitk_image_t *img, int x, int y, int width, int height, int type);

void xitk_image_draw_inner_frame (xitk_image_t *img, const char *title, const char *fontname,
    int x, int y, int w, int h);
void xitk_image_draw_outter_frame (xitk_image_t *img, const char *title, const char *fontname,
    int x, int y, int w, int h);

void xitk_image_draw_checkbox_check (xitk_image_t *img);

/*
 * Windows
 */

/**
 *
 */
xitk_window_t *xitk_window_create_window(xitk_t *xitk, int x, int y, int width, int height);
xitk_window_t *xitk_window_create_window_ext (xitk_t *xitk, int x, int y, int width, int height,
    const char *title, const char *res_name, const char *res_class,
    int override_redirect, int layer_above, xitk_image_t *icon, xitk_image_t *bg_image);

/**
 *
 */
xitk_window_t *xitk_window_create_simple_window(xitk_t *xitk, int x, int y, int width, int height);

xitk_window_t *xitk_window_create_simple_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                                    const char *title, const char *res_name, const char *res_class,
                                                    int override_redirect, int layer_above, xitk_image_t *icon);

/**
 *
 */
xitk_window_t *xitk_window_create_dialog_window(xitk_t *xitk, const char *title, int x, int y, int width, int height);

#define xitk_window_create_dialog_window_center(xitk, title, width, height) \
  xitk_window_create_dialog_window((xitk), (title), -1, -1, (width), (height))


/*
 * Get widget list of window.
 *  - list is created during first call.
 *  - list is automatically released when window is destroyed.
 */
xitk_widget_list_t *xitk_window_widget_list(xitk_window_t *);

/**
 *
 */
void xitk_window_destroy_window(xitk_window_t *w);

#define XITK_WINF_VISIBLE    0x0001 /** << contents is shown on screen */
#define XITK_WINF_ICONIFIED  0x0002 /** << window shown as icon or taskbar entry */
#define XITK_WINF_DECORATED  0x0004 /** << window manager frame around */
#define XITK_WINF_TASKBAR    0x0008 /** << do appear in taskbar */
#define XITK_WINF_PAGER      0x0010 /** << do appear in alt-tab */
#define XITK_WINF_MAX_X      0x0020 /** << maximized width */
#define XITK_WINF_MAX_Y      0x0040 /** << maximized height */
#define XITK_WINF_FULLSCREEN 0x0080 /** << full screen size */
#define XITK_WINF_FOCUS      0x0100 /** << input goes here */
#define XITK_WINF_OVERRIDE_REDIRECT 0x0200 /** << window manager please dont interfere */
#define XITK_WINF_FIXED_POS  0x0400 /** << user may _not_ click move */
#define XITK_WINF_FENCED_IN  0x0800 /** << always stay on screen cmpletely */
#define XITK_WINF_DND        0x1000 /** << do receive drag and drop events */
#define XITK_WINF_GRAB_POINTER 0x2000 /** << Grab mouse pointer */
#define XITK_WINF_LOCK_OPACITY 0x4000

/** try to set the flags in mask to value, return new flags. */
uint32_t xitk_window_flags (xitk_window_t *xwin, uint32_t mask, uint32_t value);

typedef enum {
  XITK_WR_HELPER = 0, /** << stay in front of main, show/hide with vice, default. */
  XITK_WR_ROOT,       /** << main using root window */
  XITK_WR_MAIN,       /** << represent the application. */
  XITK_WR_VICE,       /** << stay in front of main, be main while main is invisible. */
  XITK_WR_SUBMENU     /** << stay in front, keep transient_for parent. */
} xitk_window_role_t;
void xitk_window_set_role (xitk_window_t *xwin, xitk_window_role_t role);

/**
 *
 */
#define XITK_INT_KEEP 0x7fffffff
void xitk_window_move_resize (xitk_window_t *xwin, const xitk_rect_t *r);

/*
 *
 */
void xitk_window_set_input_focus(xitk_window_t *w);
void xitk_window_try_to_set_input_focus(xitk_window_t *w);

/**
 * Return current background image
 */
xitk_image_t *xitk_window_get_background_image (xitk_window_t *w);

/**
 *
 */
void xitk_window_apply_background(xitk_window_t *w);

/**
 * Change background.
 */
int xitk_window_set_background_image (xitk_window_t *w, xitk_image_t *bg);

void xitk_window_reparent_window(xitk_window_t *w, xitk_window_t *parent, int x, int y);

/*
 *
 */
void xitk_window_get_window_position (xitk_window_t *w, xitk_rect_t *r);

/*
 *
 */
void xitk_window_set_window_title(xitk_window_t *w, const char *title);

/*
 *
 */
void xitk_window_set_window_icon (xitk_window_t *w, xitk_image_t *icon);

/*
 *
 */
void xitk_window_set_window_class(xitk_window_t *w, const char *res_name, const char *res_class);

/*
 *
 */
void xitk_window_raise_window(xitk_window_t *w);

void xitk_window_set_transient_for_win(xitk_window_t *w, xitk_window_t *xwin);

/* done_cb status */
#define XITK_WINDOW_DIALOG_BUTTONS_MASK 0x7fff
#define XITK_WINDOW_DIALOG_CHECKED 0x8000
/* preset titles */
#define XITK_TITLE_INFO (_("Information"))
#define XITK_TITLE_WARN (_("Warning"))
#define XITK_TITLE_ERROR (_("Error"))
/* button label presets */
#define XITK_LABEL_OK     ((const char *)1)
#define XITK_LABEL_NO     ((const char *)2)
#define XITK_LABEL_YES    ((const char *)3)
#define XITK_LABEL_CANCEL ((const char *)4)
/* non NULL labels show 1-2-3, default preference is 3-1-2. */
xitk_register_key_t xitk_window_dialog_3 (xitk_t *xitk, xitk_window_t *transient_for, int layer_above,
  int width, const char *title,
  void (*done_cb)(void *userdata, int state), void *userdata,
  const char *button1_label, const char *button2_label, const char *button3_label,
  const char *check_label, int checked,
  int text_align, const char *text_fmt, ...) __attribute__ ((format (printf, 14, 15)));

#ifdef YET_UNUSED
void xitk_window_set_modal(xitk_window_t *w);
void xitk_window_dialog_set_modal(xitk_window_t *w);
#endif

/* change WIDGET_TABABLE, WIDGET_FOCUSABLE, WIDGET_CLICKABLE, WIDGET_KEEP_FOCUS, WIDGET_KEYABLE */
int xitk_widget_mode (xitk_widget_t *w, int mask, int mode);

void xitk_window_define_window_cursor(xitk_window_t *w, xitk_cursors_t cursor);
void xitk_window_restore_window_cursor(xitk_window_t *w);

int xitk_clipboard_set_text (xitk_widget_t *w, const char *text, int text_len);

#endif

