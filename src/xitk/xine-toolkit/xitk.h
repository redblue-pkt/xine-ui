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
 * Unique public header file for xitk µTK.
 *
 */

#ifndef _XITK_H_
#define _XITK_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <pthread.h>
#include <stdarg.h>
#include "xitk/Imlib-light/Imlib.h"

#ifdef NEED_MRLBROWSER
#include <xine.h>
#endif

#define XITK_MAJOR_VERSION          (0)
#define XITK_MINOR_VERSION          (10)
#define XITK_SUB_VERSION            (6)
#define XITK_VERSION                "0.10.6"

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
                                      if((w)->name)       \
	                                free((w)->name);  \
                                      (w)->window = None; \
                                      (w)->name   = NULL; \
                                      (w)->x      = 0;    \
                                      (w)->y      = 0;    \
                                      (w)->height = 0;    \
                                      (w)->width  = 0;    \
                                    } while(0)

typedef struct xitk_widget_s xitk_widget_t;
typedef struct xitk_menu_entry_s xitk_menu_entry_t;
typedef struct xitk_widget_list_s xitk_widget_list_t;
typedef struct xitk_skin_config_s xitk_skin_config_t;
typedef struct xitk_font_s xitk_font_t;
typedef struct xitk_pixmap_s xitk_pixmap_t;
typedef struct xitk_window_s xitk_window_t;

typedef void (*xitk_startup_callback_t)(void *);
typedef void (*xitk_simple_callback_t)(xitk_widget_t *, void *);
typedef void (*xitk_menu_callback_t)(xitk_widget_t *, xitk_menu_entry_t *, void *);
typedef void (*xitk_state_callback_t)(xitk_widget_t *, void *, int);
typedef void (*xitk_state_double_callback_t)(xitk_widget_t *, void *, double);
typedef void (*xitk_string_callback_t)(xitk_widget_t *, void *, char *);
typedef void (*xitk_dnd_callback_t) (char *filename);
typedef void (*xitk_pixmap_destroyer_t)(xitk_pixmap_t *);
#ifdef NEED_MRLBROWSER
typedef void (*xitk_mrl_callback_t)(xitk_widget_t *, void *, xine_mrl_t *);
#endif

/*
 * Event callback function type.
 * Thefunction will be called on every xevent.
 * If the window match with that one specified at
 * register time, only event for this window
 * will be send to this function.
 */
typedef void (*widget_event_callback_t)(XEvent *event, void *data);

/*
 * New positioning window callback function.
 * This callback will be called when the window
 * moving will be done (at button release time),
 * and, of course, only if there was a window
 * specified at register time.
 */
typedef void (*widget_newpos_callback_t)(int, int, int, int);

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

typedef struct {
  xitk_pixmap_t                    *image;
  xitk_pixmap_t                    *mask;
  int                               width;
  int                               height;
} xitk_image_t;

typedef struct {
  int                               red;
  int                               green;
  int                               blue;
  char                             *colorname;
} xitk_color_names_t;

typedef struct xitk_node_s {
  struct xitk_node_s               *next;
  struct xitk_node_s               *prev;
  void                             *content;
} xitk_node_t;


typedef struct {
  xitk_node_t                      *first;
  xitk_node_t                      *last;
  xitk_node_t                      *cur;
} xitk_list_t;

#define MWM_HINTS_DECORATIONS       (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS     5
typedef struct _mwmhints {
  uint32_t                          flags;
  uint32_t                          functions;
  uint32_t                          decorations;
  int32_t                           input_mode;
  uint32_t                          status;
} MWMHints;

typedef struct {
  int                               enabled;
  int                               offset_x;
  int                               offset_y;
} xitk_move_t;

typedef struct {
  Window                            window;
  char                             *name;
  int                               x;
  int                               y;
  int                               height;
  int                               width;
} window_info_t;

/*
 *  1 <widget group >
 *   1 <The groupped widget>
 *    1 <focusable>
 *     1 <clickable>
 *      1 <support key events>
 *       0 <reserved, not used yet>
 *        1111111111111 <group types>         <13 types>
 *                     1111111111111 <widget> <13 types>
*/
/* Group of widgets widget */
#define WIDGET_GROUP                0x80000000
/* Grouped widget, itself */
#define WIDGET_GROUP_WIDGET         0x40000000
/* Is widget focusable */
#define WIDGET_FOCUSABLE            0x20000000
/* Is widget clickable */
#define WIDGET_CLICKABLE            0x10000000
/* Widget support key events */
#define WIDGET_KEYABLE              0x08000000

/* Grouped widgets */
#define WIDGET_GROUP_MASK           0x03FFE000
#define WIDGET_GROUP_BROWSER        0x00002000
#define WIDGET_GROUP_FILEBROWSER    0x00004000
#define WIDGET_GROUP_MRLBROWSER     0x00008000
#define WIDGET_GROUP_COMBO          0x00010000
#define WIDGET_GROUP_TABS           0x00020000
#define WIDGET_GROUP_INTBOX         0x00040000
#define WIDGET_GROUP_DOUBLEBOX      0x00080000
#define WIDGET_GROUP_MENU           0x00100000

/* Real widgets. */
#define WIDGET_TYPE_MASK            0x00001FFF
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
 * Slider type
 */
#define XITK_VSLIDER                1
#define XITK_HSLIDER                2
#define XITK_RSLIDER                3

/*
 * Label button type
 */
#define CLICK_BUTTON                1
#define RADIO_BUTTON                2

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

/* 
 * See xitk_widget_list_[set/get]()
 */
#define WIDGET_LIST_GC              1
#define WIDGET_LIST_WINDOW          2
#define WIDGET_LIST_LIST            3
#define XITK_WIDGET_LIST_LIST(wl)   (xitk_list_t *) xitk_widget_list_get(wl, WIDGET_LIST_LIST)
#define XITK_WIDGET_LIST_WINDOW(wl) (Window) xitk_widget_list_get(wl, WIDGET_LIST_WINDOW)
#define XITK_WIDGET_LIST_GC(wl)     (GC) xitk_widget_list_get(wl, WIDGET_LIST_GC)

#define XITK_WIDGET_INIT(X, I)      do {                              \
                                      (X)->magic = XITK_WIDGET_MAGIC; \
                                      (X)->imlibdata = I;             \
                                    } while(0)

/**
 *  Widget struct
 */

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  int                               min;
  int                               max;
  int                               step;
  char                             *skin_element_name;
  xitk_state_callback_t             callback;
  void                             *userdata;
  xitk_state_callback_t             motion_callback;
  void                             *motion_userdata;
} xitk_slider_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  int                               button_type;
  int                               align;
  const char                       *label;
  xitk_simple_callback_t            callback;
  xitk_state_callback_t             state_callback;
  void                             *userdata;
  char                             *skin_element_name;
} xitk_labelbutton_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  Window                            window;
  GC                                gc;
  char                             *label;
  char                             *skin_element_name;
  xitk_simple_callback_t            callback;
  void                             *userdata;
} xitk_label_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  char                             *skin_element_name;
} xitk_image_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  xitk_state_callback_t             callback;
  void                             *userdata;
  char                             *skin_element_name;
} xitk_checkbox_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  char                             *skin_element_name;
  xitk_simple_callback_t            callback;
  void                             *userdata;
} xitk_button_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;

  struct {
    char                           *skin_element_name;
  } arrow_up;
  
  struct {
    char                           *skin_element_name;
  } slider;

  struct {
    char                           *skin_element_name;
  } arrow_dn;

  struct {
    char                           *skin_element_name;
  } arrow_left;
  
  struct {
    char                           *skin_element_name;
  } slider_h;

  struct {
    char                           *skin_element_name;
  } arrow_right;

  struct {
    char                           *skin_element_name;
    int                             max_displayed_entries;
    int                             num_entries;
    const char *const              *entries;
  } browser;
  
  xitk_state_callback_t             dbl_click_callback;

  xitk_state_callback_t             callback;
  void                             *userdata;

  xitk_widget_list_t               *parent_wlist;
} xitk_browser_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;

  char                             *skin_element_name;
  Window                            window_trans;
  int                               layer_above;

  int                               x;
  int                               y;
  char                             *window_title;
  char                             *resource_name;
  char                             *resource_class;

  struct {
    char                           *skin_element_name;
  } sort_default;

  struct {
    char                           *skin_element_name;
  } sort_reverse;

  struct {
    char                           *cur_directory;
    char                           *skin_element_name;
  } current_dir;
  
  xitk_dnd_callback_t               dndcallback;

  struct {
    char                           *caption;
    char                           *skin_element_name;
  } homedir;

  struct {
    char                           *caption;
    char                           *skin_element_name;
    xitk_string_callback_t          callback;
  } select;

  struct {
    char                           *caption;
    char                           *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t          callback;
  } kill;
 
  xitk_browser_widget_t             browser;
} xitk_filebrowser_widget_t;

#ifdef NEED_MRLBROWSER

typedef struct {
  char                             *name;
  char                             *ending;
} xitk_mrlbrowser_filter_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  char                             *skin_element_name;
  Window                            window_trans;
  int                               layer_above;

  int                               x;
  int                               y;
  char                             *window_title;
  char                             *resource_name;
  char                             *resource_class;

  struct {
    char                           *cur_origin;
    char                           *skin_element_name;
  } origin;
  
  xitk_dnd_callback_t               dndcallback;

  struct {
    char                           *caption;
    char                           *skin_element_name;
    xitk_mrl_callback_t             callback;
  } select;

  struct {
    char                           *skin_element_name;
    xitk_mrl_callback_t             callback;
  } play;

  struct {
    char                           *caption;
    char                           *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t          callback;
  } kill;

  const char *const                *ip_availables;
  
  struct {

    struct {
      char                         *skin_element_name;
    } button;

    struct {
      char                         *label_str;
      char                         *skin_element_name;
    } label;

  } ip_name;
  
  xine_t                           *xine;

  xitk_browser_widget_t             browser;
  
  xitk_mrlbrowser_filter_t        **mrl_filters;

  struct {
    char                           *skin_element_name;
  } combo;

} xitk_mrlbrowser_widget_t;

#endif

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  char                             *text;
  int                               max_length;
  xitk_string_callback_t            callback;
  void                             *userdata;
  char                             *skin_element_name;
} xitk_inputtext_widget_t;

typedef struct {
  int                               magic;
  ImlibData                        *imlibdata;
  char                             *skin_element_name;
  xitk_widget_list_t               *parent_wlist;
  char                           **entries;
  int                              layer_above;
  xitk_state_callback_t            callback;
  void                            *userdata;
  xitk_register_key_t             *parent_wkey;
} xitk_combo_widget_t;

typedef struct {
  int                              magic;
  ImlibData                       *imlibdata;
  char                            *skin_element_name;
  int                              num_entries;
  char                           **entries;
  xitk_widget_list_t              *parent_wlist;
  xitk_state_callback_t            callback;
  void                            *userdata;
} xitk_tabs_widget_t;

typedef struct {
  int                              magic;
  ImlibData                       *imlibdata;
  
  char                            *skin_element_name;
  
  int                              value;
  int                              step;

  xitk_widget_list_t              *parent_wlist;

  xitk_state_callback_t            callback;
  void                             *userdata;
} xitk_intbox_widget_t;

typedef struct {
  int                              magic;
  ImlibData                       *imlibdata;
  
  char                            *skin_element_name;
  
  double                           value;
  double                           step;

  xitk_widget_list_t              *parent_wlist;

  xitk_state_double_callback_t      callback;
  void                             *userdata;
} xitk_doublebox_widget_t;

struct xitk_menu_entry_s {
  char                             *menu;
  char                             *type; /* NULL, <separator>, <branch>, <check>, <checked> */
  xitk_menu_callback_t              cb;
  void                             *user_data;
};

typedef struct {
  int                              magic;
  ImlibData                       *imlibdata;
  
  char                            *skin_element_name;
  
  xitk_widget_list_t              *parent_wlist;

  xitk_menu_entry_t               *menu_tree; /* NULL terminated */

} xitk_menu_widget_t;


/* *******
 * INIT: widget lib initialization and friends
 */
/*
 * Create a lew widget list, store it internaly,
 * then return the pointer to app.
 */
xitk_widget_list_t *xitk_widget_list_new (void);

/*
 * Humm, this should be probably removed soon.
 */
void xitk_change_window_for_event_handler (xitk_register_key_t key, Window window);

/*
 * Register a callback function called when a signal heppen.
 */
void xitk_register_signal_handler(xitk_signal_callback_t sigcb, void *user_data);

/*
 * Register function:
 * name:   temporary name about debug stuff, can be NULL.
 * window: desired window for event callback calls, can be None.
 * cb:     callback for xevent, can be NULL.
 * pos_cb; callback for window moving.
 * dnd_cb: callback for dnd event.
 * wl:     widget_list handled internaly for xevent reactions.
 */
xitk_register_key_t xitk_register_event_handler(char *name, Window window,
						widget_event_callback_t cb,
						widget_newpos_callback_t pos_cb,
						xitk_dnd_callback_t dnd_cb,
						xitk_widget_list_t *wl, void *user_data);

/*
 * Remove widgetkey_t entry in internal table.
 */
void xitk_unregister_event_handler(xitk_register_key_t *key);

/*
 * Copy window information matching with key in passed window_info_t struct.
 */

int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf);

/*
 * Initialization function, should be the first call to widget lib.
 */
void xitk_init(Display *display);

/*
 *
 */
char *xitk_set_locale(void);

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

/*
 *
 */
void xitk_set_window_layer(Window window, int layer);

/*
 *
 */
void xitk_set_ewmh_fullscreen(Window window);

/*
 * This function start the widget live. It's a block function,
 * it will only return after a widget_stop() call.
 */
void xitk_run(xitk_startup_callback_t cb, void *data);

/*
 * This function terminate the widget lib loop event. 
 * Other functions of the lib shouldn't be called after this
 * one.
 */
void xitk_stop(void);

/*
 * Some user settings values.
 */
char *xitk_get_system_font(void);
char *xitk_get_default_font(void);
int xitk_get_black_color(void);
int xitk_get_white_color(void);
int xitk_get_background_color(void);
int xitk_get_focus_color(void);
int xitk_get_select_color(void);
void xitk_subst_special_chars(char *src, char *dest);
unsigned long xitk_get_timer_label_animation(void);
long int xitk_get_timer_dbl_click(void);
int xitk_get_barstyle_feature(void);
unsigned long xitk_get_warning_foreground(void);
unsigned long xitk_get_warning_background(void);


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
int xitk_is_cursor_out_mask(Display *display, xitk_widget_t *w, Pixmap mask, int x, int y);

/**
 *
 */
xitk_color_names_t *xitk_get_color_name(char *color);

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

/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y, unsigned int state);

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp);

/**
 *
 */
void xitk_send_key_event(xitk_widget_t *, XEvent *);

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
void xitk_set_widget_tips(xitk_widget_t *w, char *str);

/**
 *
 */
void xitk_set_widget_tips_default(xitk_widget_t *w, char *str);

/**
 *
 */
void xitk_set_widget_tips_and_timeout(xitk_widget_t *w, char *str, unsigned int timeout);

/**
 *
 */
void xitk_set_widgets_tips_timeout(xitk_widget_list_t *wl, unsigned long timeout);

/**
 *
 */
int xitk_is_mouse_over_widget(Display *display, Window window, xitk_widget_t *w);

/**
 *
 */
int xitk_get_mouse_coords(Display *display, Window window, int *x, int *y, int *rx, int *ry);

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
 * *** Sliders ***
 */

/**
 * Create a slider
 */
xitk_widget_t *xitk_slider_create(xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_slider_widget_t *sl);

/**
 *
 */
xitk_widget_t *xitk_noskin_slider_create (xitk_widget_list_t *wl,
					  xitk_slider_widget_t *s,
					  int x, int y, int width, int height, int type);

/**
 * Get current position of paddle.
 */
int xitk_slider_get_pos(xitk_widget_t *);

/**
 * Set position of paddle.
 */
void xitk_slider_set_pos(xitk_widget_t *, int);

/**
 * Set min value of slider.
 */
void xitk_slider_set_min(xitk_widget_t *, int);

/**
 * Set max value of slider.
 */
void xitk_slider_set_max(xitk_widget_t *, int);

/**
 * Get min value of slider.
 */
int xitk_slider_get_min(xitk_widget_t *);

/**
 * Get max value of slider.
 */
int xitk_slider_get_max(xitk_widget_t *);

/**
 * Set position to 0 and redraw the widget.
 */
void xitk_slider_reset(xitk_widget_t *);

/**
 * Set position to max and redraw the widget.
 */
void xitk_slider_set_to_max(xitk_widget_t *);

/**
 * Increment by step the paddle position
 */
void xitk_slider_make_step(xitk_widget_t *);

/**
 * Decrement by step the paddle position.
 */
void xitk_slider_make_backstep(xitk_widget_t *);

/**
 * Call callback for current position
 */
void xitk_slider_callback_exec(xitk_widget_t *);

/*
 * *** Nodes ***
 */
/**
 * Create a new list.
 */
xitk_list_t *xitk_list_new (void);

/**
 * Clear list.
 */
void xitk_list_clear(xitk_list_t *l);

/**
 * Freeing list.
 */
void xitk_list_free(xitk_list_t *l);

/**
 * Boolean, status of list.
 */
int xitk_list_is_empty (xitk_list_t *l);

/**
 * return content of first entry in list.
 */
void *xitk_list_first_content (xitk_list_t *l);

/**
 * return next content in list.
 */
void *xitk_list_next_content (xitk_list_t *l);

/**
 * Return last content of list.
 */
void *xitk_list_last_content (xitk_list_t *l);

/**
 * Return previous content of list.
 */
void *xitk_list_prev_content (xitk_list_t *l);

/**
 * Append content to list.
 */
void xitk_list_append_content (xitk_list_t *l, void *content);

/**
 * Insert content in list. NOT IMPLEMENTED
 */
void xitk_list_insert_content (xitk_list_t *l, void *content);

/**
 * Remove current content in list. NOT IMPLEMENTED
 */
void xitk_list_delete_current (xitk_list_t *l);


/*
 * *** Label Buttons
 */
/**
 * Create a labeled button.
 */
xitk_widget_t *xitk_labelbutton_create(xitk_widget_list_t *wl,
				       xitk_skin_config_t *skonfig, xitk_labelbutton_widget_t *b);

/**
 *
 */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
					       xitk_labelbutton_widget_t *b,
					       int x, int y, int width, int height,
					       char *ncolor, char *fcolor, char *ccolor, 
					       char *fname);

/**
 * Change label of button 'widget'.
 */
int xitk_labelbutton_change_label(xitk_widget_t *, char *);

/**
 * Return label of button 'widget'.
 */
char *xitk_labelbutton_get_label(xitk_widget_t *);

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
xitk_widget_t *xitk_label_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_label_widget_t *l);

/**
 *
 */
xitk_widget_t *xitk_noskin_label_create(xitk_widget_list_t *wl,
					xitk_label_widget_t *l,
					int x, int y, int width, int height, char *fontname);

/**
 * Change label of widget 'widget'.
 */
int xitk_label_change_label(xitk_widget_t *l, char *newlabel);

/**
 * Get label.
 */
char *xitk_label_get_label(xitk_widget_t *w);

/*
 * *** Image
 */

/**
 * Load image and return a xitk_image_t data type.
 */
xitk_image_t *xitk_image_load_image(ImlibData *idata, char *image);

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
void xitk_image_change_image(ImlibData *im, 
			     xitk_image_t *src, xitk_image_t *dest, int width, int height);

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
					  xitk_browser_widget_t *br, GC gc, 
					  int x, int y, 
					  int itemw, int itemh, int slidw, char *fontname);

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start);

/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, const char *const *list, int len, int start);

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
void xitk_browser_warp_jump(xitk_widget_t *w, char *key, int modifier);

xitk_widget_t *xitk_browser_get_browser(xitk_widget_t *w);

/*
 * Filebrowser
 */
/**
 *
 */
xitk_widget_t *xitk_filebrowser_create(xitk_widget_list_t *wl,
				       xitk_skin_config_t *skonfig, xitk_filebrowser_widget_t *fb);

/**
 *
 */
void xitk_filebrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig);

/**
 *
 */
int xitk_filebrowser_is_running(xitk_widget_t *w);

/**
 *
 */
int xitk_filebrowser_is_visible(xitk_widget_t *w);

/**
 *
 */
void xitk_filebrowser_hide(xitk_widget_t *w);

/**
 *
 */
void xitk_filebrowser_show(xitk_widget_t *w);

/**
 *
 */
void xitk_filebrowser_set_transient(xitk_widget_t *w, Window window);

/**
 *
 */
void xitk_filebrowser_destroy(xitk_widget_t *w);

/**
 *
 */
char *xitk_filebrowser_get_current_dir(xitk_widget_t *w);

/**
 *
 */
int xitk_filebrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);

/**
 *
 */
Window xitk_filebrowser_get_window_id(xitk_widget_t *w);

#ifdef NEED_MRLBROWSER
/**
 *
 */
xitk_widget_t *xitk_mrlbrowser_create(xitk_widget_list_t *wl,
				      xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb);

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
void xitk_mrlbrowser_set_transient(xitk_widget_t *w, Window window);

/**
 *
 */
void xitk_mrlbrowser_destroy(xitk_widget_t *w);

/**
 *
 */
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);

/**
 *
 */
Window xitk_mrlbrowser_get_window_id(xitk_widget_t *w);

/**
 *
 */
void xitk_mrlbrowser_set_tips_timeout(xitk_widget_t *w, int enabled, unsigned long timeout);

#endif

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
					     char *ncolor, char *fcolor, char *fontname);
/**
 * Return the text of widget.
 */
char *xitk_inputtext_get_text(xitk_widget_t *it);

/**
 * Change and redisplay the text of widget.
 */
void xitk_inputtext_change_text(xitk_widget_t *it, char *text);


/*
 *  *** skin
 */

/*
 * Alloc a xitk_skin_config_t* memory area, nullify pointers.
 */
xitk_skin_config_t *xitk_skin_init_config(void);

/*
 * Release all allocated memory of a xitk_skin_config_t* variable (element chained struct too).
 */
void xitk_skin_free_config(xitk_skin_config_t *);

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *, char *, char *);

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
char *xitk_skin_get_label_color(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_label_color_focus(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_label_color_click(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_length(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_animation(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_animation_step(xitk_skin_config_t *, const char *);

/*
 *
 */
unsigned long xitk_skin_get_label_animation_timer(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_alignment(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_label_fontname(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_printable(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_label_staticity(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_skin_filename(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_slider_type(xitk_skin_config_t *, const char *);

/*
 *
 */
int xitk_skin_get_slider_radius(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_logo(xitk_skin_config_t *);

/*
 *
 */
char *xitk_skin_get_animation(xitk_skin_config_t *);

/*
 *
 */
int xitk_skin_get_browser_entries(xitk_skin_config_t *, const char *);

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
xitk_font_t *xitk_font_load_font(Display *display, char *font);

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs);

/*
 *
 */
void xitk_font_draw_string(xitk_font_t *xtfs, Pixmap pix, GC gc, 
			   int x, int y, const char *text, 
			   size_t nbytes);

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
int xitk_font_get_char_width(xitk_font_t *xtfs, char *c, int maxnbytes, int *nbytes);

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
int xitk_font_get_char_height(xitk_font_t *xtfs, char *c, int maxnbytes, int *nbytes);

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
char *xitk_combo_get_current_entry_selected(xitk_widget_t *w);

/**
 *
 */
void xitk_combo_set_select(xitk_widget_t *w, int select);

/**
 *
 */
void xitk_combo_update_list(xitk_widget_t *w, char **list, int len);

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
int xitk_combo_is_same_parent(xitk_widget_t *w1, xitk_widget_t *w2);

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
unsigned int xitk_get_pixel_color_from_rgb(ImlibData *im, int r, int g, int b);

/**
 *
 */
unsigned int xitk_get_pixel_color_black(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_white(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_lightgray(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_gray(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_darkgray(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_warning_foreground(ImlibData *im);

/**
 *
 */
unsigned int xitk_get_pixel_color_warning_background(ImlibData *im);

/**
 *
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string(ImlibData *im, 
							      char *fontname, 
							      int width, int align, char *str,
							      unsigned int foreground,
							      unsigned int background);
xitk_image_t *xitk_image_create_image_from_string(ImlibData *im, 
						  char *fontname, 
						  int width, int align, char *str);
xitk_image_t *xitk_image_create_image(ImlibData *im, int width, int height);

xitk_pixmap_t *xitk_image_create_xitk_pixmap_with_depth(ImlibData *im, 
							int width, int height, int depth);

xitk_pixmap_t *xitk_image_create_xitk_pixmap(ImlibData *im, int width, int height);

xitk_pixmap_t *xitk_image_create_xitk_mask_pixmap(ImlibData *im, int width, int height);

void xitk_image_destroy_xitk_pixmap(xitk_pixmap_t *p);

/**
 * Free an image object.
 */
void xitk_image_free_image(ImlibData *im, xitk_image_t **src);

/**
 *
 */
void xitk_image_add_mask(ImlibData *im, xitk_image_t *dest);

/**
 *
 */
Pixmap xitk_image_create_pixmap(ImlibData *idata, int width, int height);

/**
 *
 */
void draw_flat_three_state(ImlibData *im, xitk_image_t *p);

/**
 *
 */
void draw_bevel_three_state(ImlibData *im, xitk_image_t *p);

/**
 *
 */
void draw_bevel_two_state(ImlibData *im, xitk_image_t *p);

void draw_paddle_three_state_vertical(ImlibData *im, xitk_image_t *p);
void draw_paddle_three_state_horizontal(ImlibData *im, xitk_image_t *p);

/**
 *
 */
void draw_inner(ImlibData *im, xitk_pixmap_t *p, int w, int h);
void draw_inner_light(ImlibData *im, xitk_pixmap_t *p, int w, int h);

/**
 *
 */
void draw_outter(ImlibData *im, xitk_pixmap_t *p, int w, int h);
void draw_outter_light(ImlibData *im, xitk_pixmap_t *p, int w, int h);

void draw_flat_with_color(ImlibData *im, xitk_pixmap_t *p, int w, int h, unsigned int color);
/**
 *
 */
void draw_flat(ImlibData *im, xitk_pixmap_t *p, int w, int h);

/**
 *
 */
void draw_arrow_up(ImlibData *im, xitk_image_t *p);

/**
 *
 */
void draw_arrow_down(ImlibData *im, xitk_image_t *p);

/*
 * Draw and arrow (direction is LEFT).
 */
void draw_arrow_left(ImlibData *im, xitk_image_t *p);

/*
 * Draw and arrow (direction is RIGHT).
 */
void draw_arrow_right(ImlibData *im, xitk_image_t *p);

/**
 *
 */
void draw_rectangular_inner_box(ImlibData *im, xitk_pixmap_t *p, 
				int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_outter_box(ImlibData *im, xitk_pixmap_t *p,
				 int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_inner_box_light(ImlibData *im, xitk_pixmap_t *p, 
				      int x, int y, int width, int height);

/**
 *
 */
void draw_rectangular_outter_box_light(ImlibData *im, xitk_pixmap_t *p,
				       int x, int y, int width, int height);

/**
 *
 */
void draw_inner_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		      int x, int y, int w, int h);
void draw_outter_frame(ImlibData *im, xitk_pixmap_t *p, char *title, char *fontname,
		       int x, int y, int w, int h);

void draw_tab(ImlibData *im, xitk_image_t *p);

void draw_paddle_rotate(ImlibData *im, xitk_image_t *p);
void draw_rotate_button(ImlibData *im, xitk_image_t *p);

void draw_button_plus(ImlibData *im, xitk_image_t *p);
void draw_button_minus(ImlibData *im, xitk_image_t *p);

void menu_draw_check(ImlibData *im, xitk_image_t *p, int checked);
void menu_draw_arrow_branch(ImlibData *im, xitk_image_t *p);

/*
 * Windows
 */

/**
 *
 */
xitk_window_t *xitk_window_create_window(ImlibData *im, int x, int y, int width, int height);

/**
 *
 */
xitk_window_t *xitk_window_create_simple_window(ImlibData *im, int x, int y, int width, int height);

/**
 *
 */
xitk_window_t *xitk_window_create_dialog_window(ImlibData *im, char *title, int x, int y, int width, int height);


/**
 *
 */
void xitk_window_dialog_destroy(xitk_window_t *w);

/**
 *
 */
void xitk_window_destroy_window(ImlibData *im, xitk_window_t *w);

/**
 *
 */
void xitk_window_move_window(ImlibData *im, xitk_window_t *w, int x, int y);

/**
 *
 */
void xitk_window_center_window(ImlibData *im, xitk_window_t *w);

/**
 *
 */
Window xitk_window_get_window(xitk_window_t *w);

/**
 *
 */
Pixmap xitk_window_get_background(xitk_window_t *w);

/**
 *
 */
void xitk_window_apply_background(ImlibData *im, xitk_window_t *w);

/**
 *
 */
int xitk_window_change_background(ImlibData *im, xitk_window_t *w, Pixmap bg, int width, int height);

/**
 *
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height);

/*
 *
 */
void xitk_get_window_position(Display *display, Window window, 
			      int *x, int *y, int *width, int *height);

/*
 *
 */
void xitk_window_get_window_position(ImlibData *im, xitk_window_t *w, 
				     int *x, int *y, int *width, int *height);

/*
 *
 */
void xitk_set_window_title(Display *display, Window window, char *title);

/*
 *
 */
void xitk_window_set_window_title(ImlibData *im, xitk_window_t *w, char *title);

/*
 *
 */
int xitk_is_window_visible(Display *display, Window window);

/*
 *
 */
Window xitk_get_desktop_root_window(Display *display, int screen, Window *clientparent);

/*
 *
 */
int xitk_is_window_size(Display *display, Window window, int width, int height);

xitk_window_t *xitk_window_dialog_button_free_with_width(ImlibData *im, char *title,
							 int window_width, int align, char *message, ...);

/*
 *
 */
xitk_window_t *xitk_window_dialog_one_button_with_width(ImlibData *im, char *title, char *button_label,
							xitk_state_callback_t cb, void *userdata, 
							int window_width, int align, char *message, ...);

/**
 *
 */
xitk_window_t *xitk_window_dialog_ok_with_width(ImlibData *im, char *title,
						xitk_state_callback_t cb, void *userdata, 
						int window_width, int align, char *message, ...);

/**
 *
 */
xitk_window_t *xitk_window_dialog_ok(ImlibData *im, char *title,
				     xitk_state_callback_t cb, void *userdata, int align, char *message, ...);


#ifdef	__GNUC__
#define xitk_window_dialog_ok(im, title, cb, userdata, align, message, args...) \
  xitk_window_dialog_ok_with_width(im, title, cb, userdata, 400, align, message, ##args)

#define xitk_window_dialog_error(im, message, args...) \
  xitk_window_dialog_ok_with_width(im, _("Error"), NULL, NULL, 400, ALIGN_CENTER, message, ##args)

#define xitk_window_dialog_info(im, message, args...) \
  xitk_window_dialog_ok_with_width(im, _("Information"), NULL, NULL, 400, ALIGN_CENTER, message, ##args)
#else
#define xitk_window_dialog_ok(im, title, cb, userdata, align, ...) \
  xitk_window_dialog_ok_with_width(im, title, cb, userdata, 400, align, __VA_ARGS__)

#define xitk_window_dialog_error(im, ...) \
  xitk_window_dialog_ok_with_width(im, _("Error"), NULL, NULL, 400, ALIGN_CENTER, __VA_ARGS__)

#define xitk_window_dialog_info(im, ...) \
  xitk_window_dialog_ok_with_width(im, _("Information"), NULL, NULL, 400, ALIGN_CENTER, __VA_ARGS__)
#endif
/* 
 *
 */
xitk_window_t *xitk_window_dialog_three_buttons_with_width(ImlibData *im, char *title, 
							   char *button1_label,
							   char *button2_label,
							   char *button3_label,
							   xitk_state_callback_t cb1, 
							   xitk_state_callback_t cb2, 
							   xitk_state_callback_t cb3, 
							   void *userdata, 
							   int window_width, int align, char *message, ...);
/**
 *
 */
xitk_window_t *xitk_window_dialog_yesnocancel_with_width(ImlibData *im, char *title,
							 xitk_state_callback_t ycb, 
							 xitk_state_callback_t ncb, 
							 xitk_state_callback_t ccb, 
							 void *userdata, 
							 int window_width, int align, char *message, ...);

/**
 *
 */
xitk_window_t *xitk_window_dialog_yesnocancel(ImlibData *im, char *title,
					      xitk_state_callback_t ycb, 
					      xitk_state_callback_t ncb, 
					      xitk_state_callback_t ccb, 
					      void *userdata, int align, char *message, ...);

#ifdef __GNUC__
#define xitk_window_dialog_yesnocancel(im, title, ycb, ncb, ccb, userdata, align, message, args...) \
  xitk_window_dialog_yesnocancel_with_width(im, title, ycb, ncb, ccb, userdata, 400, align, message, ##args)
#else
#define xitk_window_dialog_yesnocancel(im, title, ycb, ncb, ccb, userdata, align, ...) \
  xitk_window_dialog_yesnocancel_with_width(im, title, ycb, ncb, ccb, userdata, 400, align, __VA_ARGS__)
#endif

/*
 *
 */
xitk_window_t *xitk_window_dialog_two_buttons_with_width(ImlibData *im, char *title,
							 char *button1_label, char *button2_label,
							 xitk_state_callback_t cb1, 
							 xitk_state_callback_t cb2, 
							 void *userdata, 
							 int window_width, int align, char *message, ...);

/**
 *
 */
xitk_window_t *xitk_window_dialog_yesno_with_width(ImlibData *im, char *title,
						   xitk_state_callback_t ycb, 
						   xitk_state_callback_t ncb, 
						   void *userdata, 
						   int window_width, int align, char *message, ...);

/**
 *
 */
xitk_window_t *xitk_window_dialog_yesno(ImlibData *im, char *title,
					xitk_state_callback_t ycb, 
					xitk_state_callback_t ncb, 
					void *userdata, int align, char *message, ...);
#ifdef __GNUC__
#define xitk_window_dialog_yesno(im, title, ycb, ncb, userdata, align, message, args...) \
  xitk_window_dialog_yesno_with_width(im, title, ycb, ncb, userdata, 400, align, message, ##args)
#else
#define xitk_window_dialog_yesno(im, title, ycb, ncb, userdata, align, ...) \
  xitk_window_dialog_yesno_with_width(im, title, ycb, ncb, userdata, 400, align, __VA_ARGS__)
#endif

void xitk_window_set_modal(xitk_window_t *w);
void xitk_window_dialog_set_modal(xitk_window_t *w);

xitk_widget_t *xitk_noskin_tabs_create(xitk_widget_list_t *wl,
				       xitk_tabs_widget_t *t, 
				       int x, int y, int width, char *fontname);
int xitk_tabs_get_current_selected(xitk_widget_t *w);
char *xitk_tabs_get_current_tab_selected(xitk_widget_t *w);
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
void xitk_menu_destroy(xitk_widget_t *w);
int xitk_menu_show_sub_branchs(xitk_widget_t *w);

#endif
