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
#include <stdarg.h>

#include <X11/Xlib.h>

#ifdef NEED_MRLBROWSER
#include <xine.h>
#endif

#define XITK_MAJOR_VERSION          (0)
#define XITK_MINOR_VERSION          (10)
#define XITK_SUB_VERSION            (7)
#define XITK_VERSION                "0.10.7"

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

#define INPUT_MOTION                (ExposureMask | ButtonPressMask | ButtonReleaseMask | \
                                     KeyPressMask | KeyReleaseMask | ButtonMotionMask |   \
                                     StructureNotifyMask | PropertyChangeMask |           \
                                     LeaveWindowMask | EnterWindowMask | PointerMotionMask)

#define WINDOW_INFO_ZERO(w)         do {                  \
	                              free((w)->name);    \
                                      (w)->xwin   = NULL; \
                                      (w)->name   = NULL; \
                                      (w)->x      = 0;    \
                                      (w)->y      = 0;    \
                                      (w)->height = 0;    \
                                      (w)->width  = 0;    \
                                    } while(0)

typedef struct xitk_s xitk_t;

typedef struct xitk_widget_s xitk_widget_t;
typedef struct xitk_menu_entry_s xitk_menu_entry_t;
typedef struct xitk_widget_list_s xitk_widget_list_t;
typedef struct xitk_skin_config_s xitk_skin_config_t;
typedef struct xitk_font_s xitk_font_t;
typedef struct xitk_pixmap_s xitk_pixmap_t;
typedef struct xitk_image_s xitk_image_t;
typedef struct xitk_window_s xitk_window_t;

void xitk_add_widget (xitk_widget_list_t *wl, xitk_widget_t *wi);

typedef void (*xitk_startup_callback_t)(void *);
typedef void (*xitk_simple_callback_t)(xitk_widget_t *, void *);
typedef void (*xitk_menu_callback_t)(xitk_widget_t *, xitk_menu_entry_t *, void *);
typedef void (*xitk_state_callback_t)(xitk_widget_t *, void *, int);
typedef void (*xitk_ext_state_callback_t)(xitk_widget_t *, void *, int, int modifiers);
typedef void (*xitk_state_double_callback_t)(xitk_widget_t *, void *, double);
typedef void (*xitk_string_callback_t)(xitk_widget_t *, void *, const char *);
typedef void (*xitk_dnd_callback_t) (const char *filename);
typedef void (*xitk_pixmap_destroyer_t)(xitk_pixmap_t *);
#ifdef NEED_MRLBROWSER
typedef void (*xitk_mrl_callback_t)(xitk_widget_t *, void *, xine_mrl_t *);
#endif

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

/*
 * event callbacks
 */

typedef struct {
#define XITK_KEY_PRESS      1
#define XITK_KEY_RELEASE    2
  int           event;
  unsigned long key_pressed;     /* KeySym */
  int           modifiers;       /* modifier flags XITK_MODIFIER_* */
  int           keycode;         /* hardware keycode if known */
  const char   *keysym_str;      /* ASCII key name */
  const char   *keycode_str;     /* ASCII key name */
} xitk_key_event_t;

typedef struct {
#define XITK_BUTTON_PRESS   3
#define XITK_BUTTON_RELEASE 4
  int event;
  int button;
  int x, y;
  int modifiers;
} xitk_button_event_t;

typedef struct {
  int   expose_count;
  void *orig_event;
} xitk_expose_event_t;

typedef struct {
  int x, y;
} xitk_motion_event_t;

typedef struct {
  int x, y;
  int width, height;
} xitk_configure_event_t;

typedef void (*xitk_key_event_callback_t)(void *data, const xitk_key_event_t *);
typedef void (*xitk_button_event_callback_t)(void *data, const xitk_button_event_t *);
typedef void (*xitk_motion_event_callback_t)(void *data, const xitk_motion_event_t *);

typedef void (*xitk_configure_notify_callback_t)(void *data, const xitk_configure_event_t *);
typedef void (*xitk_map_notify_callback_t)(void *data);
typedef void (*xitk_expose_notify_callback_t)(void *data, const xitk_expose_event_t *);
typedef void (*xitk_destroy_notify_callback_t)(void *);

typedef struct {
  xitk_key_event_callback_t    key_cb;
  xitk_button_event_callback_t btn_cb;
  xitk_motion_event_callback_t motion_cb;

  widget_newpos_callback_t     pos_cb;
  xitk_dnd_callback_t          dnd_cb;

  xitk_configure_notify_callback_t configure_notify_cb;
  xitk_map_notify_callback_t       map_notify_cb;
  xitk_expose_notify_callback_t    expose_notify_cb;
  xitk_destroy_notify_callback_t   destroy_notify_cb;

} xitk_event_cbs_t;

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

typedef struct {
  int                               red;
  int                               green;
  int                               blue;
  char                              colorname[20];
} xitk_color_names_t;

typedef struct {
  xitk_window_t                    *xwin;
  char                             *name;
  int                               x;
  int                               y;
  int                               height;
  int                               width;
} window_info_t;

typedef struct {
  xitk_image_t *image;
  int x, y, width, height;
} xitk_part_image_t;

typedef struct {
  /* all */
  int x, y;
  int visibility, enability;
  char *pixmap_name;
  xitk_part_image_t pixmap_img;
  /* button list */
  int max_buttons, direction;
  /* browser */
  int browser_entries;
  /* label */
  int label_length, label_alignment, label_y, label_printable, label_staticity;
  int label_animation, label_animation_step;
  unsigned long int label_animation_timer;
  char *label_color, *label_color_focus, *label_color_click, *label_fontname;
  char *label_pixmap_font_name;
  char *label_pixmap_font_format;
  xitk_image_t *label_pixmap_font_img;
  /* slider */
  int slider_type, slider_radius;
  char *slider_pixmap_pad_name;
  xitk_part_image_t slider_pixmap_pad_img;
} xitk_skin_element_info_t;

/*
 *  1 <widget group >
 *   1 <The groupped widget>
 *    1 <focusable>
 *     1 <clickable>
 *      1 <support key events>
 *       1 <support for partial repaint>
 *        1111111111111 <group types>         <13 types>
 *                     1111111111111 <widget> <13 types>
*/
/* Group of widgets widget */
#define WIDGET_GROUP                0x80000000
/* Grouped widget, itself */
#define WIDGET_GROUP_MEMBER         0x40000000
/* Is widget focusable */
#define WIDGET_FOCUSABLE            0x20000000
/* Is widget clickable */
#define WIDGET_CLICKABLE            0x10000000
/* Widget support key events */
#define WIDGET_KEYABLE              0x08000000
/* Widget support partial repaint */
#define WIDGET_PARTIAL_PAINTABLE    0x04000000

/* Grouped widgets */
#define WIDGET_GROUP_MASK           0x03FFE000
#define WIDGET_GROUP_BROWSER        0x00002000
#define WIDGET_GROUP_MRLBROWSER     0x00004000
#define WIDGET_GROUP_COMBO          0x00008000
#define WIDGET_GROUP_TABS           0x00010000
#define WIDGET_GROUP_INTBOX         0x00020000
#define WIDGET_GROUP_DOUBLEBOX      0x00040000
#define WIDGET_GROUP_MENU           0x00080000

#define WIDGET_TYPE_MASK            0x00001FFF
/* Group leaders.. */
#define WIDGET_TYPE_COMBO           0x00001001
#define WIDGET_TYPE_DOUBLEBOX       0x00001002
#define WIDGET_TYPE_INTBOX          0x00001003
#define WIDGET_TYPE_BROWSER         0x00001004
#define WIDGET_TYPE_MRLBROWSER      0x00001005
#define WIDGET_TYPE_TABS            0x00001006
#define WIDGET_TYPE_MENU            0x00001007
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
 * Label button type
 */
#define CLICK_BUTTON                1
#define RADIO_BUTTON                2
#define TAB_BUTTON                  3


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

/*
 * See
 */
#define DIRECTION_UP                1
#define DIRECTION_DOWN              2
#define DIRECTION_LEFT              3
#define DIRECTION_RIGHT             4

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
  WINDOW_TYPE_DESKTOP = 1,
  WINDOW_TYPE_DOCK,
  WINDOW_TYPE_TOOLBAR,
  WINDOW_TYPE_MENU,
  WINDOW_TYPE_UTILITY,
  WINDOW_TYPE_SPLASH,
  WINDOW_TYPE_DIALOG,
  WINDOW_TYPE_DROPDOWN_MENU,
  WINDOW_TYPE_POPUP_MENU,
  WINDOW_TYPE_TOOLTIP,
  WINDOW_TYPE_NOTIFICATION,
  WINDOW_TYPE_COMBO,
  WINDOW_TYPE_DND,
  WINDOW_TYPE_NORMAL
} xitk_wm_window_type_t;

void xitk_set_wm_window_type(Window window, xitk_wm_window_type_t type);
void xitk_unset_wm_window_type(Window window, xitk_wm_window_type_t type);

void xitk_window_set_wm_window_type(xitk_window_t *w, xitk_wm_window_type_t type);
void xitk_window_unset_wm_window_type(xitk_window_t *w, xitk_wm_window_type_t type);


typedef enum {
  xitk_cursor_invisible,
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

/* 
 * See xitk_widget_list_[set/get]()
 */
#define WIDGET_LIST_GC              1
#define XITK_WIDGET_LIST_GC(wl)     (GC) xitk_widget_list_get(wl, WIDGET_LIST_GC)
#define XITK_WIDGET_LIST_FREE(wl)   xitk_widget_list_defferred_destroy(wl)

#define XITK_WIDGET_INIT(X)         do {                              \
                                      (X)->magic = XITK_WIDGET_MAGIC; \
                                    } while(0)

/**
 *  Widget struct
 */

/* Slider type */
#define XITK_VSLIDER                1
#define XITK_HSLIDER                2
#define XITK_RSLIDER                3
#define XITK_HVSLIDER               4
typedef struct {
  int                               magic;
  int                               min;
  int                               max;
  int                               step;
  const char                       *skin_element_name;
  xitk_state_callback_t             callback;
  void                             *userdata;
  xitk_state_callback_t             motion_callback;
  void                             *motion_userdata;
} xitk_slider_widget_t;
typedef struct {
  struct {
    int pos, step, visible, max;
  } h, v;
} xitk_slider_hv_t;
typedef enum {
  XITK_SLIDER_SYNC_GET = 0,
  XITK_SLIDER_SYNC_SET,
  XITK_SLIDER_SYNC_SET_AND_PAINT
} xitk_slider_sync_t;
void xitk_slider_hv_sync (xitk_widget_t *w, xitk_slider_hv_t *info, xitk_slider_sync_t mode);
/** Create a slider */
xitk_widget_t *xitk_slider_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig, xitk_slider_widget_t *sl);
xitk_widget_t *xitk_noskin_slider_create (xitk_widget_list_t *wl, xitk_slider_widget_t *s,
  int x, int y, int width, int height, int type);
/** * Get current position of paddle. */
int xitk_slider_get_pos (xitk_widget_t *);
/** Set position of paddle. */
void xitk_slider_set_pos (xitk_widget_t *, int);
/** * Set min value of slider. */
void xitk_slider_set_min (xitk_widget_t *, int);
/** Set max value of slider. */
void xitk_slider_set_max (xitk_widget_t *, int);
/** * Get min value of slider. */
int xitk_slider_get_min (xitk_widget_t *);
/** Get max value of slider. */
int xitk_slider_get_max (xitk_widget_t *);
/** Set position to 0 and redraw the widget. */
void xitk_slider_reset (xitk_widget_t *);
/** Set position to max and redraw the widget. */
void xitk_slider_set_to_max (xitk_widget_t *);
/** Increment by step the paddle position */
void xitk_slider_make_step (xitk_widget_t *);
/** Decrement by step the paddle position. */
void xitk_slider_make_backstep (xitk_widget_t *);
/** Call callback for current position */
void xitk_slider_callback_exec (xitk_widget_t *);

typedef struct {
  int                               magic;
  int                               button_type;
  int                               align;
  const char                       *label;
  xitk_simple_callback_t            callback;
  xitk_ext_state_callback_t         state_callback;
  void                             *userdata;
  const char                       *skin_element_name;
} xitk_labelbutton_widget_t;

typedef struct {
  int                               magic;
  const char                       *label;
  const char                       *skin_element_name;
  xitk_simple_callback_t            callback;
  void                             *userdata;
} xitk_label_widget_t;

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
} xitk_image_widget_t;

typedef struct {
  int                               magic;
  xitk_state_callback_t             callback;
  void                             *userdata;
  const char                       *skin_element_name;
} xitk_checkbox_widget_t;

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
  xitk_simple_callback_t            callback;
  void                             *userdata;
} xitk_button_widget_t;

typedef struct {
  int                               magic;

  struct {
    const char                     *skin_element_name;
  } arrow_up;
  
  struct {
    const char                     *skin_element_name;
  } slider;

  struct {
    const char                     *skin_element_name;
  } arrow_dn;

  struct {
    const char                     *skin_element_name;
  } arrow_left;
  
  struct {
    const char                     *skin_element_name;
  } slider_h;

  struct {
    const char                     *skin_element_name;
  } arrow_right;

  struct {
    const char                     *skin_element_name;
    int                             max_displayed_entries;
    int                             num_entries;
    const char *const              *entries;
  } browser;
  
  xitk_ext_state_callback_t         dbl_click_callback;

  xitk_ext_state_callback_t         callback;
  void                             *userdata;

} xitk_browser_widget_t;

#ifdef NEED_MRLBROWSER

typedef struct {
  char                             *name;
  char                             *ending;
} xitk_mrlbrowser_filter_t;

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
  int                               layer_above;
  xitk_pixmap_t                    *icon;
  int                               set_wm_window_normal;

  int                               x;
  int                               y;
  char                             *window_title;
  char                             *resource_name;
  char                             *resource_class;

  struct {
    char                           *cur_origin;
    const char                     *skin_element_name;
  } origin;

  xitk_key_event_callback_t         key_cb;
  void                             *key_cb_data;
  xitk_dnd_callback_t               dndcallback;

  struct {
    char                           *caption;
    const char                     *skin_element_name;
    xitk_mrl_callback_t             callback;
    void                           *data;
  } select;

  struct {
    const char                     *skin_element_name;
    xitk_mrl_callback_t             callback;
    void                           *data;
  } play;

  struct {
    char                           *caption;
    const char                     *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t          callback;
    void                           *data;
  } kill;

  const char *const                *ip_availables;
  
  struct {

    struct {
      const char                   *skin_element_name;
    } button;

    struct {
      const char                   *label_str;
      const char                   *skin_element_name;
    } label;

  } ip_name;
  
  xine_t                           *xine;

  xitk_browser_widget_t             browser;
  
  xitk_mrlbrowser_filter_t        **mrl_filters;

  struct {
    const char                     *skin_element_name;
  } combo;

} xitk_mrlbrowser_widget_t;

#endif

typedef struct {
  int                               magic;
  char                             *text;
  int                               max_length;
  xitk_string_callback_t            callback;
  void                             *userdata;
  const char                       *skin_element_name;
} xitk_inputtext_widget_t;

typedef struct {
  int                               magic;
  const char                       *skin_element_name;
  const char                      **entries;
  int                              layer_above;
  xitk_state_callback_t            callback;
  void                            *userdata;
  xitk_register_key_t             *parent_wkey;
} xitk_combo_widget_t;

typedef struct {
  int                              magic;
  const char                      *skin_element_name;
  int                              num_entries;
  char                           **entries;
  xitk_state_callback_t            callback;
  void                            *userdata;
} xitk_tabs_widget_t;

typedef struct {
  int                              magic;

  const char                      *skin_element_name;
  
  int                              value;
  int                              step;

  xitk_state_callback_t            callback;
  void                             *userdata;
} xitk_intbox_widget_t;

typedef struct {
  int                              magic;

  const char                      *skin_element_name;
  
  double                           value;
  double                           step;

  xitk_state_double_callback_t      callback;
  void                             *userdata;
} xitk_doublebox_widget_t;

struct xitk_menu_entry_s {
  char                             *menu;
  char                             *shortcut; /* displayed (can be NULL) */
  char                             *type;     /* NULL, <separator>, <branch>, <check>, <checked> */
  xitk_menu_callback_t              cb;
  void                             *user_data;
  int                               user_id;
};

typedef struct {
  int                              magic;
  const char                      *skin_element_name;
  xitk_menu_entry_t               *menu_tree; /* NULL terminated */

} xitk_menu_widget_t;


/* *******
 * INIT: widget lib initialization and friends
 */
/*
 * Create a new widget list, store it internaly,
 * then return the pointer to app.
 */
xitk_widget_list_t *xitk_widget_list_new (xitk_t *xitk);

/*
 * Register a callback function called when a signal heppen.
 */
void xitk_register_signal_handler(xitk_signal_callback_t sigcb, void *user_data);

/*
 * Register function:
 * name:   temporary name about debug stuff, can be NULL.
 * w:      desired window for event callback calls.
 *         window widget list is handled internally for event reactions.
 * cbs:    callback functions for events, can be NULL.
 * user_data: passed to each callback.
 */

xitk_register_key_t xitk_window_register_event_handler(const char *name, xitk_window_t *w,
                                                       const xitk_event_cbs_t *cbs, void *user_data);

/*
 * Remove widgetkey_t entry in internal table.
 */
void xitk_unregister_event_handler(xitk_register_key_t *key);

/*
 * Helper function to free widget list inside callbacks.
 */
void xitk_widget_list_defferred_destroy(xitk_widget_list_t *wl);

/*
 * Copy window information matching with key in passed window_info_t struct.
 */

int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf);

xitk_window_t *xitk_get_window(xitk_register_key_t key);

/*
 * Initialization function, should be the first call to widget lib.
 */
xitk_t *xitk_init(const char *prefered_visual, int install_colormap,
                  int use_x_lock_display, int use_synchronized_x, int verbosity);

void xitk_free(xitk_t **);

void xitk_sync(xitk_t *); /* XSync() wrapper */

/*
 *
 */
const char *xitk_set_locale(void);

/*
 *
 */
long int xitk_get_last_keypressed_time(void);

void xitk_ungrab_pointer(void);

int xitk_get_display_width(void);
int xitk_get_display_height(void);

/*
 *
 */
int xitk_get_layer_level(void);

/*
 * Return WM_TYPE_*
 */
uint32_t xitk_get_wm_type(void);

/*
 *
 */
void xitk_set_layer_above(Window window);
void xitk_window_set_layer_above(xitk_window_t *window);

/*
 *
 */
void xitk_set_window_layer(Window window, int layer);
void xitk_window_set_window_layer(xitk_window_t *w, int layer);

/*
 *
 */
void xitk_set_ewmh_fullscreen(Window window);
void xitk_unset_ewmh_fullscreen(Window window);

/*
 * This function start the widget live. It's a block function,
 * it will only return after a widget_stop() call.
 */
void xitk_run (void (* start_cb)(void *data), void *start_data,
  void (* stop_cb)(void *data), void *stop_data);

/*
 * This function terminate the widget lib loop event. 
 * Other functions of the lib shouldn't be called after this
 * one.
 */
void xitk_stop(void);

/*
 * Some user settings values.
 */
const char *xitk_get_system_font(void);
const char *xitk_get_default_font(void);
int xitk_get_xmb_enability(void);
void xitk_set_xmb_enability(int value);
int xitk_get_black_color(void);
int xitk_get_white_color(void);
int xitk_get_background_color(void);
int xitk_get_focus_color(void);
int xitk_get_select_color(void);
char *xitk_filter_filename(const char *name);
unsigned long xitk_get_timer_label_animation(void);
long int xitk_get_timer_dbl_click(void);
int xitk_get_barstyle_feature(void);
unsigned long xitk_get_warning_foreground(void);
unsigned long xitk_get_warning_background(void);
int xitk_get_menu_shortcuts_enability(void);


/*
 *
 ****** */

/**
 * Allocate an clean memory of "size" bytes.
 */
void *xitk_xmalloc(size_t);

/**
 *
 */
int xitk_is_cursor_out_mask(xitk_widget_t *w, xitk_pixmap_t *mask, int x, int y);

/**
 *
 */
xitk_color_names_t *xitk_get_color_name(const char *color);

/**
 * return pointer to the xitk_color_names struct.
 */
xitk_color_names_t *xitk_get_color_names(void);

/**
 * Free color object.
 */
void xitk_free_color_name(xitk_color_names_t *color);

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
 * Boolean function, if x and y coords is in widget.
 */
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y);

/**
 * Return widget from widget list 'wl' localted at x,y coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y);

// XXX move next to widget.h ??? and rename _widget_ ... ? 
/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y, unsigned int state);

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp, int modifier);

/**
 *
 */
void xitk_send_key_event(xitk_widget_t *, XEvent *, int modifier);

/**
 * Return the focused widget.
 */
xitk_widget_t *xitk_get_focused_widget(xitk_widget_list_t *);

/**
 * Force the focus to given widget.
 */
void xitk_set_focus_to_widget(xitk_widget_t *);

/**
 * Return the pressed widget.
 */
xitk_widget_t *xitk_get_pressed_widget(xitk_widget_list_t *);

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

/**
 * Boolean, return 1 if widget 'w' have focus.
 */
int xitk_is_widget_focused(xitk_widget_t *);

/**
 * Boolean, enable state of widget.
 */
int xitk_is_widget_enabled(xitk_widget_t *);

/**
 * Enable widget.
 */
void xitk_enable_widget(xitk_widget_t *);

/**
 * Disable widget.
 */
void xitk_disable_widget(xitk_widget_t *);

/**
 *
 */
void xitk_free_widget(xitk_widget_t *w);

/**
 * Destroy and free widget.
 */
void xitk_destroy_widget(xitk_widget_t *w);

/**
 * Destroy widgets from widget list.
 */
void xitk_destroy_widgets(xitk_widget_list_t *wl);

/**
 *
 */
void xitk_stop_widget(xitk_widget_t *w);

/**
 * Stop each (if widget handle it) widgets of widget list.
 */
void xitk_stop_widgets(xitk_widget_list_t *wl);

/**
 * Start widget.
 */
void xitk_start_widget(xitk_widget_t *w);

/**
 * Set widgets of widget list visible.
 */
void xitk_show_widgets(xitk_widget_list_t *);

/**
 * Set widget visible
 */
void xitk_show_widget(xitk_widget_t *);

/**
 *
 */
void xitk_enable_and_show_widget(xitk_widget_t *w);

/**
 * Set widgets of widget list not visible.
 */
void xitk_hide_widgets(xitk_widget_list_t *);

/**
 * Hide a widget.
 */
void xitk_hide_widget(xitk_widget_t *);

/**
 *
 */
void xitk_disable_and_hide_widget(xitk_widget_t *w);

/**
 *
 */
xitk_image_t *xitk_get_widget_foreground_skin(xitk_widget_t *w);

/**
 *
 */
xitk_image_t *xitk_get_widget_background_skin(xitk_widget_t *w);

/**
 *
 */
void xitk_set_widget_tips(xitk_widget_t *w, const char *str);

/**
 *
 */
void xitk_set_widget_tips_default(xitk_widget_t *w, const char *str);

/**
 *
 */
void xitk_set_widget_tips_and_timeout(xitk_widget_t *w, const char *str, unsigned long timeout);

/**
 *
 */
unsigned long xitk_get_widget_tips_timeout(xitk_widget_t *w);

/**
 *
 */
void xitk_set_widgets_tips_timeout(xitk_widget_list_t *wl, unsigned long timeout);

/**
 *
 */
int xitk_is_mouse_over_widget(xitk_widget_t *w);

/**
 *
 */
void xitk_enable_widget_tips(xitk_widget_t *w);

/**
 *
 */
void xitk_disable_widget_tips(xitk_widget_t *w);

/**
 *
 */
void xitk_disable_widgets_tips(xitk_widget_list_t *wl);

/**
 *
 */
void xitk_enable_widgets_tips(xitk_widget_list_t *wl);

/**
 *
 */
void xitk_set_widget_tips_timeout(xitk_widget_t *w, unsigned long timeout);

/**
 * Pass events to UI
 */
void xitk_xevent_notify(XEvent *event);

/*
 * *** Label Buttons
 */
/**
 * Create a labeled button.
 */
xitk_widget_t *xitk_labelbutton_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_labelbutton_widget_t *b);

/**
 *
 */
xitk_widget_t *xitk_info_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, const xitk_skin_element_info_t *info);

/**
 *
 */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b,
  int x, int y, int width, int height,
  const char *ncolor, const char *fcolor, const char *ccolor,
  const char *fname);

/**
 * Change label of button 'widget'.
 */
int xitk_labelbutton_change_label (xitk_widget_t *, const char *new_label);
int xitk_labelbutton_change_shortcut_label(xitk_widget_t *, const char *, int, const char *);

/**
 * Return label of button 'widget'.
 */
const char *xitk_labelbutton_get_label(xitk_widget_t *);
const char *xitk_labelbutton_get_shortcut_label(xitk_widget_t *);

/**
 * Get state of button 'widget'.
 */
int xitk_labelbutton_get_state(xitk_widget_t *);

/**
 * Set state of button 'widget'.
 */
void xitk_labelbutton_set_state(xitk_widget_t *, int);

/*
 * Return used font name
 */
char *xitk_labelbutton_get_fontname(xitk_widget_t *);

/**
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment(xitk_widget_t *, int);

/**
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment(xitk_widget_t *);

/**
 *
 */
void xitk_labelbutton_set_label_offset(xitk_widget_t *, int);
int xitk_labelbutton_get_label_offset(xitk_widget_t *);

void xitk_labelbutton_callback_exec(xitk_widget_t *w);

/*
 * *** Labels
 */
/**
 * Create a label widget.
 */
xitk_widget_t *xitk_label_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_label_widget_t *l);

/**
 *
 */
xitk_widget_t *xitk_noskin_label_create (xitk_widget_list_t *wl,
  const xitk_label_widget_t *l, int x, int y, int width, int height,
  const char *fontname);

/**
 * Change label of widget 'widget'.
 */
int xitk_label_change_label(xitk_widget_t *l, const char *newlabel);

/**
 * Get label.
 */
const char *xitk_label_get_label(xitk_widget_t *w);

/*
 * *** Image
 */

/**
 * Load image and return a xitk_image_t data type.
 */
xitk_image_t *xitk_image_load_image(xitk_t *xitk, const char *image);
void xitk_image_set_pix_font (xitk_image_t *image, const char *format);

xitk_image_t *xitk_image_decode_raw(xitk_t *xitk, const void *data, size_t size);
int xitk_image_render(xitk_image_t *, int width, int height);

int xitk_image_width(xitk_image_t *);
int xitk_image_height(xitk_image_t *);

/**
 * Create an image widget type.
 */
xitk_widget_t *xitk_image_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_image_widget_t *im);

/**
 * Same as above, without skin.
 */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
					 xitk_image_widget_t *im, 
					 xitk_image_t *image, int x, int y);

/**
 *
 */
void xitk_image_change_image(xitk_image_t *src, xitk_image_t *dest, int width, int height);

/*
 * *** Checkbox
 */
/**
 * Create a checkbox.
 */
xitk_widget_t *xitk_checkbox_create (xitk_widget_list_t *wl,
				     xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cp);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_checkbox_create(xitk_widget_list_t *wl,
					   xitk_checkbox_widget_t *cb,
					   int x, int y, int width, int height);


/**
 * get state of checkbox "widget".
 */
int xitk_checkbox_get_state(xitk_widget_t *);

/**
 * Set state of checkbox .
 */
void xitk_checkbox_set_state(xitk_widget_t *, int);

/**
 * Call callback
 */
void xitk_checkbox_callback_exec(xitk_widget_t *w);

/*
 * ** Buttons
 */
/**
 * Create a button
 */
/**
 *
 */
xitk_widget_t *xitk_button_create (xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_button_widget_t *b);

/**
 *
 */
xitk_widget_t *xitk_noskin_button_create (xitk_widget_list_t *wl,
					  xitk_button_widget_t *b,
					  int x, int y, int width, int height);


/*
 * *** Browser
 */
/**
 * Create the list browser
 */
/**
 *
 */
xitk_widget_t *xitk_browser_create(xitk_widget_list_t *wl,
				   xitk_skin_config_t *skonfig, xitk_browser_widget_t *b);

/**
 *
 */
xitk_widget_t *xitk_noskin_browser_create(xitk_widget_list_t *wl,
                                          xitk_browser_widget_t *br,
					  int x, int y, 
					  int itemw, int itemh, int slidw, const char *fontname);

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start);

/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, const char *const *list, const char *const *shortcut, int len, int start);

/**
 * slide up.
 */
void xitk_browser_step_up(xitk_widget_t *w, void *data);

/**
 * slide Down.
 */
void xitk_browser_step_down(xitk_widget_t *w, void *data);

/**
 * slide left.
 */
void xitk_browser_step_left(xitk_widget_t *w, void *data);

/**
 * slide right.
 */
void xitk_browser_step_right(xitk_widget_t *w, void *data);

/**
 * Page Up.
 */
void xitk_browser_page_up(xitk_widget_t *w, void *data);

/**
 * Page Down.
 */
void xitk_browser_page_down(xitk_widget_t *w, void *data);

/**
 * Return the current selected button (if not, return -1)
 */
int xitk_browser_get_current_selected(xitk_widget_t *w);

/**
 * Select the item 'select' in list
 */
void xitk_browser_set_select(xitk_widget_t *w, int select);

/**
 * Release all enabled buttons
 */
void xitk_browser_release_all_buttons(xitk_widget_t *w);

/**
 * Return the number of displayed entries
 */
int xitk_browser_get_num_entries(xitk_widget_t *w);

/**
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w);

/**
 * Change browser labels alignment
 */
void xitk_browser_set_alignment(xitk_widget_t *w, int align);

/*
 * Jump to entry in list which match with the alphanum char key.
 */
void xitk_browser_warp_jump(xitk_widget_t *w, const char *key, int modifier);

xitk_widget_t *xitk_browser_get_browser(xitk_widget_t *w);

#ifdef NEED_MRLBROWSER
/**
 *
 */
xitk_widget_t *xitk_mrlbrowser_create(xitk_t *xitk, xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb);

/**
 *
 */
void xitk_mrlbrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig);

/**
 *
 */
int xitk_mrlbrowser_is_running(xitk_widget_t *w);

/**
 *
 */
int xitk_mrlbrowser_is_visible(xitk_widget_t *w);

/**
 *
 */
void xitk_mrlbrowser_hide(xitk_widget_t *w);

/**
 *
 */
void xitk_mrlbrowser_show(xitk_widget_t *w);

/**
 *
 */
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);

/**
 *
 */
xitk_window_t *xitk_mrlbrowser_get_window(xitk_widget_t *w);

/**
 *
 */
void xitk_mrlbrowser_set_tips_timeout(xitk_widget_t *w, int enabled, unsigned long timeout);

#endif

/*
 * return len of keystring (stored in kbuf)
 * define ksym
 * kbuf should be large enought (256 usually)
 */
int xitk_get_keysym_and_buf(XEvent *event, KeySym *ksym, char kbuf[], int kblen);

/**
 * return keypressed
 */
KeySym xitk_get_key_pressed(XEvent *event);

KeySym xitk_keycode_to_keysym(XEvent *event);

/* Return size of output string (>=buf_size if truncated). -1 on error. */
int xitk_keysym_to_string(KeySym keysym, char *buf, size_t buf_size);

/* grab one input event (mouse button or keyboard key).
 * return -1 on error.
 */
int xitk_window_grab_input(xitk_window_t *w,
                           KeySym *key, unsigned int *keycode, int *modifier,
                           int *button);


/**
 * All states of modifiers (see xitk_get_key_modifier() bellow).
 */
/**
 * return 1 if a modifier key has been pressed (extracted from XEvent *)
 * modifier pointer will contain the modifier(s) bits (MODIFIER_*)
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier);

/**
 * Create an input text box.
 */
xitk_widget_t *xitk_inputtext_create (xitk_widget_list_t *wl,
				      xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it);

/**
 *
 */
xitk_widget_t *xitk_noskin_inputtext_create (xitk_widget_list_t *wl,
					     xitk_inputtext_widget_t *it,
					     int x, int y, int width, int height,
					     const char *ncolor, const char *fcolor, const char *fontname);
/**
 * Return the text of widget.
 */
char *xitk_inputtext_get_text(xitk_widget_t *it);

/**
 * Change and redisplay the text of widget.
 */
void xitk_inputtext_change_text(xitk_widget_t *it, const char *text);


/*
 *  *** skin
 */

/*
 * Alloc a xitk_skin_config_t* memory area, nullify pointers.
 */
xitk_skin_config_t *xitk_skin_init_config(xitk_t *);

/*
 * Release all allocated memory of a xitk_skin_config_t* variable (element chained struct too).
 */
void xitk_skin_free_config(xitk_skin_config_t *);

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *, const char *, const char *);

/*
 * Check skin version.
 * return: 0 if version found < min_version
 *         1 if version found == min_version
 *         2 if version found > min_version
 *        -1 if no version found
 */
int xitk_skin_check_version(xitk_skin_config_t *, int);

/*
 * Unload (free) xitk_skin_config_t object.
 */
void xitk_skin_unload_config(xitk_skin_config_t *);

/*
 *
 */
int xitk_skin_get_direction(xitk_skin_config_t *skonfig, const char *str);

/*
 *
 */
int xitk_skin_get_visibility(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_printability(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_enability(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_coord_x(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_coord_y(xitk_skin_config_t *, const char *);

/*
 *
 */
const char *xitk_skin_get_skin_filename(xitk_skin_config_t *, const char *);

/*
 *
 */
const char *xitk_skin_get_logo(xitk_skin_config_t *);

/*
 *
 */
const char *xitk_skin_get_animation(xitk_skin_config_t *);

/*
 *
 */
int xitk_skin_get_browser_entries(xitk_skin_config_t *, const char *);

/*
 *
 */
xitk_image_t *xitk_skin_get_image(xitk_skin_config_t *, const char *);
void xitk_skin_get_part_image (xitk_skin_config_t *skonfig, xitk_part_image_t *image, const char *str);

/*
 *
 */
int xitk_skin_get_max_buttons(xitk_skin_config_t *, const char *);

/*
 *
 */
void xitk_skin_lock(xitk_skin_config_t *);

/*
 *
 */
void xitk_skin_unlock(xitk_skin_config_t *);


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
void xitk_pixmap_draw_string(xitk_pixmap_t *pixmap, xitk_font_t *xtfs,
                             int x, int y, const char *text,
                             size_t nbytes, int color);

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
int xitk_font_get_char_width(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes);

/*
 *
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c);

/*
 *
 */
int xitk_font_get_char_height(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes);

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int nbytes,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			    int *lbearing, int *rbearing, int *width, int *ascent, int *descent);

/*
 *
 */
int xitk_font_get_ascent(xitk_font_t *xtfs, const char *c);

/*
 *
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c);

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc);
void xitk_widget_list_set_font(xitk_widget_list_t *wl, xitk_font_t *xtfs);

/**
 *
 */
xitk_widget_t *xitk_combo_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_combo_widget_t *c,
				 xitk_widget_t **lw, xitk_widget_t **bw);

/**
 *
 */
xitk_widget_t *xitk_noskin_combo_create(xitk_widget_list_t *wl,
					xitk_combo_widget_t *c, int x, int y, int width,
					xitk_widget_t **lw, xitk_widget_t **bw);

/**
 *
 */
int xitk_combo_get_current_selected(xitk_widget_t *w);

/**
 *
 */
const char *xitk_combo_get_current_entry_selected(xitk_widget_t *w);

/**
 *
 */
void xitk_combo_set_select(xitk_widget_t *w, int select);

/**
 *
 */
void xitk_combo_update_list(xitk_widget_t *w, const char *const *const list, int len);

/**
 *
 */
void xitk_combo_update_pos(xitk_widget_t *w);

/**
 *
 */
void xitk_combo_rollunroll(xitk_widget_t *w);

/**
 *
 */
void xitk_combo_callback_exec(xitk_widget_t *w);

/**
 *
 */
xitk_widget_t *xitk_combo_get_label_widget(xitk_widget_t *w);

/**
 *
 */
unsigned int xitk_get_pixel_color_from_rgb(xitk_t *xitk, int r, int g, int b);

/**
 *
 */
unsigned int xitk_get_pixel_color_black(xitk_t *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_white(xitk_t *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_lightgray(xitk_t *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_gray(xitk_t *xitk);

/**
 *
 */
unsigned int xitk_get_pixel_color_darkgray(xitk_t *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_warning_foreground(xitk_t *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_warning_background(xitk_t *im);

/**
 * xitk pixmaps
 */

xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(xitk_t *xitk, int width, int height, int depth);

xitk_pixmap_t *xitk_image_create_xitk_pixmap(xitk_t *xitk, int width, int height);

xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(xitk_t *xitk, int width, int height);

void xitk_image_destroy_xitk_pixmap(xitk_pixmap_t *p);

xitk_pixmap_t *xitk_pixmap_create_from_data(xitk_t *xitk, int width, int height, const char *data);
Pixmap xitk_pixmap_get_pixmap(xitk_pixmap_t *p);

int xitk_pixmap_width(xitk_pixmap_t *);
int xitk_pixmap_height(xitk_pixmap_t *);
int xitk_pixmap_get_pixel(xitk_pixmap_t *, int x, int y); /* note: expensive */

/*
 *
 */

void pixmap_draw_line(xitk_pixmap_t *p, int x0, int y0, int x1, int y1, unsigned color);
void pixmap_draw_rectangle(xitk_pixmap_t *p, int x, int y, int w, int h, unsigned color);
void pixmap_fill_rectangle(xitk_pixmap_t *p, int x, int y, int w, int h, unsigned color);
void pixmap_fill_polygon(xitk_pixmap_t *p, XPoint *points, int npoints, unsigned color);

void xitk_image_draw_line(xitk_image_t *i, int x0, int y0, int x1, int y1, unsigned color);
void xitk_image_fill_rectangle(xitk_image_t *i, int x, int y, int w, int h, unsigned color);
void xitk_image_fill_polygon(xitk_image_t *i, XPoint *points, int npoints, unsigned color);

void xitk_pixmap_copy_area(xitk_pixmap_t *src, xitk_pixmap_t *dst,
                           int src_x, int src_y, int width, int height, int dst_x, int dst_y);

/*
 *
 */

void draw_inner(xitk_pixmap_t *p, int w, int h);
void draw_inner_light(xitk_pixmap_t *p, int w, int h);

/**
 *
 */
void draw_outter(xitk_pixmap_t *p, int w, int h);
void draw_outter_light(xitk_pixmap_t *p, int w, int h);

void xitk_image_draw_outter(xitk_image_t *i, int w, int h);

void draw_flat_with_color(xitk_pixmap_t *p, int w, int h, unsigned int color);
/**
 *
 */
void draw_flat(xitk_pixmap_t *p, int w, int h);

/**
 *
 */
void draw_rectangular_inner_box(xitk_pixmap_t *p, int x, int y, int width, int height);
void xitk_image_draw_rectangular_inner_box(xitk_image_t *i, int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_outter_box(xitk_pixmap_t *p, int x, int y, int width, int height);
void xitk_image_draw_rectangular_outter_box(xitk_image_t *i, int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_inner_box_light(xitk_pixmap_t *p, int x, int y, int width, int height);
void xitk_image_draw_rectangular_inner_box_light(xitk_image_t *i, int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_outter_box_light(xitk_pixmap_t *p, int x, int y, int width, int height);
void xitk_image_draw_rectangular_outter_box_light(xitk_image_t *i, int x, int y, int width, int height);

/**
 *
 */
void draw_inner_frame(xitk_pixmap_t *p, const char *title, const char *fontname,
                      int x, int y, int w, int h);
void draw_outter_frame(xitk_pixmap_t *p, const char *title, const char *fontname,
                       int x, int y, int w, int h);

void xitk_image_draw_inner_frame(xitk_image_t *i, const char *title, const char *fontname,
                                 int x, int y, int w, int h);
void xitk_image_draw_outter_frame(xitk_image_t *i, const char *title, const char *fontname,
                                  int x, int y, int w, int h);

/**
 * xitk image
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(xitk_t *xitk,
                                                              const char *fontname,
                                                              int width, int align, const char *str,
							      unsigned int foreground,
							      unsigned int background);
xitk_image_t *xitk_image_create_image_from_string(xitk_t *xitk,
                                                  const char *fontname,
                                                  int width, int align, const char *str);
xitk_image_t *xitk_image_create_image(xitk_t *xitk, int width, int height);

void xitk_image_draw_image(xitk_widget_list_t *wl, xitk_image_t *im, int src_x, int src_y, int width, int height, int dst_x, int dst_y);
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
void xitk_image_add_mask(xitk_image_t *dest);

/**
 *
 */
//Pixmap xitk_image_create_pixmap(ImlibData *idata, int width, int height);

/**
 *
 */
void draw_flat_three_state(xitk_image_t *p);

/**
 *
 */
void draw_bevel_three_state(xitk_image_t *p);

/**
 *
 */
void draw_bevel_two_state(xitk_image_t *p);

void draw_three_state_round_style(xitk_image_t *p, int x, int y, int d, int w, int checked);
void draw_three_state_check_style(xitk_image_t *p, int x, int y, int d, int w, int checked);


void draw_paddle_three_state_vertical(xitk_image_t *p);
void draw_paddle_three_state_horizontal(xitk_image_t *p);

/**
 *
 */
void draw_arrow_up(xitk_image_t *p);

/**
 *
 */
void draw_arrow_down(xitk_image_t *p);

/*
 * Draw and arrow (direction is LEFT).
 */
void draw_arrow_left(xitk_image_t *p);

/*
 * Draw and arrow (direction is RIGHT).
 */
void draw_arrow_right(xitk_image_t *p);

/*
 *
 */

void draw_tab(xitk_image_t *p);

void draw_paddle_rotate(xitk_image_t *p);
void draw_rotate_button(xitk_image_t *p);

void draw_button_plus(xitk_image_t *p);
void draw_button_minus(xitk_image_t *p);

void menu_draw_check(xitk_image_t *p, int checked);
void menu_draw_arrow_branch(xitk_image_t *p);

/*
 * Windows
 */

/**
 *
 */
xitk_window_t *xitk_window_create_window(xitk_t *xitk, int x, int y, int width, int height);
xitk_window_t *xitk_window_create_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                             const char *title, const char *res_name, const char *res_class,
                                             int override_redirect, int layer_above,
                                             xitk_pixmap_t *icon);

/**
 *
 */
xitk_window_t *xitk_window_create_simple_window(xitk_t *xitk, int x, int y, int width, int height);

xitk_window_t *xitk_window_create_simple_window_ext(xitk_t *xitk, int x, int y, int width, int height,
                                                    const char *title, const char *res_name, const char *res_class,
                                                    int override_redirect, int layer_above, xitk_pixmap_t *icon);

/**
 *
 */
xitk_window_t *xitk_window_create_dialog_window(xitk_t *xitk, const char *title, int x, int y, int width, int height);

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

/**
 *
 */
void xitk_window_move_window(xitk_window_t *w, int x, int y);
void xitk_window_resize_window(xitk_window_t *w, int width, int height);

/**
 *
 */
void xitk_window_center_window(xitk_window_t *w);

/**
 *
 */
Window xitk_window_get_window(xitk_window_t *w);

/*
 *
 */
void xitk_window_set_input_focus(xitk_window_t *w);
void xitk_window_try_to_set_input_focus(xitk_window_t *w);

void xitk_window_set_parent_window(xitk_window_t *xwin, xitk_window_t *parent);

/**
 * Return copy of current background pixmap
 *  - caller must free pixmap (or use xitk_window_set_background)
 */
xitk_pixmap_t *xitk_window_get_background_pixmap(xitk_window_t *w);

/**
 *
 */
void xitk_window_apply_background(xitk_window_t *w);

/**
 *
 */
int xitk_window_change_background(xitk_window_t *w, Pixmap bg, int width, int height);

/*
 * Change background.
 *  - ownership of bg is transfered to window.
 */
int xitk_window_set_background(xitk_window_t *w, xitk_pixmap_t *bg);

/**
 *
 */
int xitk_window_change_background_with_image(xitk_window_t *w, xitk_image_t *img, int width, int height);

/**
 *
 */
void xitk_window_clear_window(xitk_window_t *);

void xitk_window_reparent_window(xitk_window_t *w, xitk_window_t *parent, int x, int y);

/**
 *
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height);

/*
 *
 */
void xitk_window_get_window_position(xitk_window_t *w, int *x, int *y, int *width, int *height);

/*
 *
 */
void xitk_window_set_window_title(xitk_window_t *w, const char *title);

/*
 *
 */
void xitk_window_set_window_icon(xitk_window_t *w, xitk_pixmap_t *);

/*
 *
 */
void xitk_window_set_window_class(xitk_window_t *w, const char *res_name, const char *res_class);

/*
 *
 */
void xitk_window_show_window(xitk_window_t *w, int raise);
void xitk_window_raise_window(xitk_window_t *w);
void xitk_window_hide_window(xitk_window_t *w);

void xitk_window_set_transient_for(xitk_window_t *xwin, Window win);
void xitk_window_set_transient_for_win(xitk_window_t *w, xitk_window_t *xwin);

/*
 *
 */
void xitk_window_iconify_window(xitk_window_t *w);

/*
 *
 */
int xitk_window_is_window_visible(xitk_window_t *w);

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

//void xitk_window_set_modal(xitk_window_t *w);
//void xitk_window_dialog_set_modal(xitk_window_t *w);

xitk_widget_t *xitk_noskin_tabs_create(xitk_widget_list_t *wl,
				       xitk_tabs_widget_t *t, 
                                       int x, int y, int width, const char *fontname);
int xitk_tabs_get_current_selected(xitk_widget_t *w);
const char *xitk_tabs_get_current_tab_selected(xitk_widget_t *w);
void xitk_tabs_set_current_selected(xitk_widget_t *w, int select);

xitk_widget_t *xitk_noskin_intbox_create(xitk_widget_list_t *wl,
					 xitk_intbox_widget_t *ib,
					 int x, int y, int width, int height, 
					 xitk_widget_t **iw, xitk_widget_t **mw, xitk_widget_t **lw);
void xitk_intbox_set_value(xitk_widget_t *, int);
int xitk_intbox_get_value(xitk_widget_t *);
xitk_widget_t *xitk_intbox_get_input_widget(xitk_widget_t *w);

xitk_widget_t *xitk_noskin_doublebox_create(xitk_widget_list_t *wl,
					    xitk_doublebox_widget_t *ib,
					    int x, int y, int width, int height, 
					    xitk_widget_t **iw, xitk_widget_t **mw, xitk_widget_t **lw);
void xitk_doublebox_set_value(xitk_widget_t *, double);
double xitk_doublebox_get_value(xitk_widget_t *);
xitk_widget_t *xitk_doublebox_get_input_widget(xitk_widget_t *w);

int xitk_widget_list_set(xitk_widget_list_t *wl, int param, void *data);
void *xitk_widget_list_get(xitk_widget_list_t *wl, int param);
void xitk_widget_keyable(xitk_widget_t *w, int keyable);

xitk_widget_t *xitk_noskin_menu_create(xitk_widget_list_t *wl, 
				       xitk_menu_widget_t *m, int x, int y);
void xitk_menu_show_menu(xitk_widget_t *w);
void xitk_menu_add_entry(xitk_widget_t *w, xitk_menu_entry_t *me);
xitk_widget_t *xitk_menu_get_menu(xitk_widget_t *w);
void xitk_menu_destroy_sub_branchs(xitk_widget_t *w);
void xitk_menu_destroy_branch(xitk_widget_t *w);
int xitk_menu_show_sub_branchs(xitk_widget_t *w);

void xitk_window_define_window_cursor(xitk_window_t *w, xitk_cursors_t cursor);
void xitk_window_restore_window_cursor(xitk_window_t *w);

int xitk_clipboard_set_text (xitk_widget_t *w, const char *text, int text_len);

#endif
