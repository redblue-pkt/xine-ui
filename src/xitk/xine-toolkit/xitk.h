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
 * Unique public header file for xitk µTK.
 *
 */

#ifndef _XITK_H_
#define _XITK_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"

#ifdef NEED_MRLBROWSER
#include "xine.h"
#endif

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct _mwmhints {
  uint32_t  flags;
  uint32_t  functions;
  uint32_t  decorations;
  int32_t   input_mode;
  uint32_t  status;
} MWMHints;

typedef struct {
  int       enabled;
  int       offset_x;
  int       offset_y;
} xitk_move_t;

typedef struct {
  Window    window;
  char     *name;
  int       x;
  int       y;
  int       height;
  int       width;
} window_info_t;
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
  Pixmap    image;
  Pixmap    mask;
  int       width;
  int       height;
} xitk_image_t;

typedef struct {
  int       red;
  int       green;
  int       blue;
  char     *colorname;
} xitk_color_names_t;

typedef struct xitk_node_s {
  struct xitk_node_s    *next, *prev;
  void                  *content;
} xitk_node_t;


typedef struct {
  xitk_node_t           *first, *last, *cur;
} xitk_list_t;

struct xitk_widget_list_s;

struct xitk_widget_s;

typedef void xitk_skin_config_t;

typedef void (*widget_paint_callback_t)(struct xitk_widget_s *, Window, GC);

typedef int (*widget_click_callback_t) (struct xitk_widget_list_s *, struct xitk_widget_s *, int, int, int);

typedef int (*widget_focus_callback_t)(struct xitk_widget_list_s *, struct xitk_widget_s *, int);

typedef void (*widget_keyevent_callback_t)(struct xitk_widget_list_s *, struct xitk_widget_s *, XEvent *);

typedef int (*widget_inside_callback_t)(struct xitk_widget_s *, int, int);

typedef struct xitk_widget_s {
  int                        x;
  int                        y;
  int                        width;
  int                        height;

  int                        enable;
  int                        have_focus;
  int                        running;
  int                        visible;

  widget_paint_callback_t    paint;

  /* notify callback return value : 1 => repaint necessary 0=> do nothing */
                                       /*   parameter: up (1) or down (0) */
  widget_click_callback_t    notify_click;
                                       /*            entered (1) left (0) */
  widget_focus_callback_t    notify_focus;

  widget_keyevent_callback_t notify_keyevent;

  widget_inside_callback_t   notify_inside;

  void                      *private_data;
  uint32_t                   widget_type;
} xitk_widget_t;

typedef struct witk_widget_list_s {
  xitk_list_t          *l;

  xitk_widget_t            *focusedWidget;
  xitk_widget_t            *pressedWidget;

  Window               win;
  GC                   gc;
} xitk_widget_list_t;


typedef void (*xitk_simple_callback_t)(xitk_widget_t *, void *);
typedef void (*xitk_state_callback_t)(xitk_widget_t *, void *, int);
typedef void (*xitk_string_callback_t)(xitk_widget_t *, void *, char *);
typedef void (*xitk_dnd_callback_t) (char *filename);
#ifdef NEED_MRLBROWSER
typedef void (*xitk_mrl_callback_t)(xitk_widget_t *, void *, mrl_t *);
#endif

typedef struct {
  Display             *display;
  Window               win;
  
  xitk_dnd_callback_t  callback;

  Atom                 _XA_XdndAware;
  Atom                 _XA_XdndEnter;
  Atom                 _XA_XdndLeave;
  Atom                 _XA_XdndDrop;
  Atom                 _XA_XdndPosition;
  Atom                 _XA_XdndStatus;
  Atom                 _XA_XdndActionCopy;
  Atom                 _XA_XdndSelection;
  Atom                 _XA_XdndFinished;
  Atom                 _XA_XINE_XDNDEXCHANGE;
  Atom                 _XA_WM_DELETE_WINDOW;
  Atom                 atom_support;
  Atom                 version;
} xitk_dnd_t;


/* *******
 * INIT: widget lib initialization and friends
 */
/*
 * Event callback function type.
 * This function will be called on every xevent.
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
 * A unique key returned by register function.
 * It is necessary to store it at program side,
 * because it will be necessary to pass it for
 * identification of caller.
 */
typedef int xitk_register_key_t;

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
 * This function start the widget live. It's a block function,
 * it will only return after a widget_stop() call.
 */
void xitk_run(void);

/*
 * This function terminate the widget lib loop event. 
 * Other functions of the lib shouldn't be called after this
 * one.
 */
void xitk_stop(void);

/*
 *
 ****** */

/**
 * Allocate an clean memory of "size" bytes.
 */
void *xitk_xmalloc(size_t);

/**
 * return pointer to the xitk_color_names struct.
 */
xitk_color_names_t *xitk_get_color_names(void);

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
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y) ;

/**
 * Return widget from widget list 'wl' localted at x,y coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int bUp) ;

/**
 *
 */
void xitk_send_key_event(xitk_widget_list_t *, xitk_widget_t *, XEvent *);

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
 * Stop each (if widget handle it) widgets of widget list.
 */
void xitk_stop_widgets(xitk_widget_list_t *wl);

/**
 * Set widgets of widget list visible.
 */
void xitk_show_widgets(xitk_widget_list_t *);

/**
 * Set widgets of widget list not visible.
 */
void xitk_hide_widgets(xitk_widget_list_t *);

/**
 * Pass events to UI
 */
void xitk_xevent_notify(XEvent *event);

/* 
 * *** Sliders ***
 */

/*  To handle a vertical slider */
#define XITK_VSLIDER 1
/*  To handle an horizontal slider */
#define XITK_HSLIDER 2

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     min;
  int                     max;
  int                     step;
  char                   *skin_element_name;
  xitk_state_callback_t   callback;
  void                   *userdata;
  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
} xitk_slider_widget_t;

/**
 * Create a slider
 */
xitk_widget_t *xitk_slider_create(xitk_skin_config_t *skonfig, xitk_slider_widget_t *sl);

/**
 * Get current position of paddle.
 */
int xitk_slider_get_pos(xitk_widget_t *);

/**
 * Set position of paddle.
 */
void xitk_slider_set_pos(xitk_widget_list_t *, xitk_widget_t *, int);

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
void xitk_slider_reset(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Set position to max and redraw the widget.
 */
void xitk_slider_set_to_max(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Increment by step the paddle position
 */
void xitk_slider_make_step(xitk_widget_list_t *, xitk_widget_t *);

/**
 * Decrement by step the paddle position.
 */
void xitk_slider_make_backstep(xitk_widget_list_t *, xitk_widget_t *);


/*
 * *** Nodes ***
 */
/**
 * Create a new list.
 */
xitk_list_t *xitk_list_new (void);

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
#define CLICK_BUTTON 1
#define RADIO_BUTTON 2
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     button_type;
  char                   *label;
  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
  void                   *userdata;
  char                   *skin_element_name;
} xitk_labelbutton_widget_t;

/**
 * Create a labeled button.
 */
xitk_widget_t *xitk_labelbutton_create(xitk_skin_config_t *skonfig, xitk_labelbutton_widget_t *b);

/**
 * Change label of button 'widget'.
 */
int xitk_labelbutton_change_label(xitk_widget_list_t *wl, xitk_widget_t *, char *);

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
void xitk_labelbutton_set_state(xitk_widget_t *, int, Window, GC);


/*
 * *** Labels
 */
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  Window                  window;
  GC                      gc;
  char                   *label;
  char                   *skin_element_name;
} xitk_label_widget_t;

/**
 * Create a label widget.
 */
xitk_widget_t *xitk_label_create(xitk_skin_config_t *skonfig, xitk_label_widget_t *l);

/**
 * Change label of widget 'widget'.
 */
int xitk_label_change_label(xitk_widget_list_t *wl, xitk_widget_t *l, char *newlabel);


/*
 * *** Image
 */

/**
 * Load image and return a xitk_image_t data type.
 */
xitk_image_t *xitk_load_image(ImlibData *idata, char *image);

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  char                   *skin_element_name;
} xitk_image_widget_t;

/**
 * Create an image widget type.
 */
xitk_widget_t *xitk_image_create(xitk_skin_config_t *skonfig, xitk_image_widget_t *im);

/*
 * *** DND
 */
void xitk_init_dnd(Display *display, xitk_dnd_t *);

void xitk_make_window_dnd_aware(xitk_dnd_t *, Window);

Bool xitk_process_client_dnd_message(xitk_dnd_t *, XEvent *);

void xitk_set_dnd_callback(xitk_dnd_t *, void *);


/*
 * *** Checkbox
 */
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  xitk_state_callback_t  callback;
  void                   *userdata;
  char                   *skin_element_name;
} xitk_checkbox_widget_t;
/**
 * Create a checkbox.
 */
xitk_widget_t *xitk_checkbox_create (xitk_skin_config_t *skonfig, xitk_checkbox_widget_t *cp);

/**
 * get state of checkbox "widget".
 */
int xitk_checkbox_get_state(xitk_widget_t *);

/**
 * Set state of checkbox .
 */
void xitk_checkbox_set_state(xitk_widget_t *, int, Window, GC);

/*
 * ** Buttons
 */
/**
 * Create a button
 */
typedef struct {
  Display                  *display;
  ImlibData                *imlibdata;
  char                     *skin_element_name;
  xitk_simple_callback_t    callback;
  void                     *userdata;
} xitk_button_widget_t;

xitk_widget_t *xitk_button_create (xitk_skin_config_t *skonfig, xitk_button_widget_t *b);


/*
 * *** Browser
 */
/**
 * Create the list browser
 */
/* Default time (in ms) between mouse click to act as double click */
#define DEFAULT_DBL_CLICK_TIME 200
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;

  struct {
    char                 *skin_element_name;
  } arrow_up;
  
  struct {
    char                 *skin_element_name;
  } slider;

  struct {
    char                 *skin_element_name;
  } arrow_dn;

  struct {
    char                 *skin_element_name;
    int                   max_displayed_entries;
    int                   num_entries;
    char                **entries;
  } browser;
  
  int                     dbl_click_time;
  xitk_state_callback_t   dbl_click_callback;

  /* Callback on selection function */
  xitk_simple_callback_t  callback;
  void                   *userdata;

  xitk_widget_list_t          *parent_wlist;
} xitk_browser_widget_t;

xitk_widget_t *xitk_browser_create(xitk_skin_config_t *skonfig, xitk_browser_widget_t *b);

/**
 * Redraw buttons/slider
 */
void xitk_browser_rebuild_browser(xitk_widget_t *w, int start);
/**
 * Update the list, and rebuild button list
 */
void xitk_browser_update_list(xitk_widget_t *w, char **list, int len, int start);
/**
 * slide up.
 */
void xitk_browser_step_up(xitk_widget_t *w, void *data);
/**
 * slide Down.
 */
void xitk_browser_step_down(xitk_widget_t *w, void *data);
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
 * Return the real number of first displayed in list
 */
int xitk_browser_get_current_start(xitk_widget_t *w);

/*
 * Filebrowser
 */
typedef struct {
  Display                   *display;
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
    char                    *skin_element_name;
  } sort_default;

  struct {
    char                    *skin_element_name;
  } sort_reverse;

  struct {
    char                    *cur_directory;
    char                    *skin_element_name;
  } current_dir;
  
  xitk_dnd_callback_t        dndcallback;

  struct {
    char                    *caption;
    char                    *skin_element_name;
  } homedir;

  struct {
    char                    *caption;
    char                    *skin_element_name;
    xitk_string_callback_t   callback;
  } select;

  struct {
    char                    *caption;
    char                    *skin_element_name;
  } dismiss;

  struct {
    xitk_simple_callback_t   callback;
  } kill;
 
  xitk_browser_widget_t             browser;
} xitk_filebrowser_widget_t;

xitk_widget_t *xitk_filebrowser_create(xitk_skin_config_t *skonfig, xitk_filebrowser_widget_t *fb);
void xitk_filebrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig);
int xitk_filebrowser_is_running(xitk_widget_t *w);
int xitk_filebrowser_is_visible(xitk_widget_t *w);
void xitk_filebrowser_hide(xitk_widget_t *w);
void xitk_filebrowser_show(xitk_widget_t *w);
void xitk_filebrowser_set_transient(xitk_widget_t *w, Window window);
void xitk_filebrowser_destroy(xitk_widget_t *w);
char *xitk_filebrowser_get_current_dir(xitk_widget_t *w);
int xitk_filebrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);
Window xitk_filebrowser_get_window_id(xitk_widget_t *w);

#ifdef NEED_MRLBROWSER

typedef struct {
  Display                   *display;
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

  xitk_browser_widget_t             browser;

} xitk_mrlbrowser_widget_t;

xitk_widget_t *xitk_mrlbrowser_create(xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb);
void xitk_mrlbrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig);
int xitk_mrlbrowser_is_running(xitk_widget_t *w);
int xitk_mrlbrowser_is_visible(xitk_widget_t *w);
void xitk_mrlbrowser_hide(xitk_widget_t *w);
void xitk_mrlbrowser_show(xitk_widget_t *w);
void xitk_mrlbrowser_set_transient(xitk_widget_t *w, Window window);
void xitk_mrlbrowser_destroy(xitk_widget_t *w);
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf);
Window xitk_mrlbrowser_get_window_id(xitk_widget_t *w);

#endif

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  char                   *text;
  int                     max_length;
  xitk_string_callback_t  callback;
  void                   *userdata;
  char                   *skin_element_name;
} xitk_inputtext_widget_t;

/**
 * All states of modifiers (see xitk_get_key_modifier() bellow).
 */
#define MODIFIER_NOMOD 0x00000000
#define MODIFIER_SHIFT 0x00000001
#define MODIFIER_LOCK  0x00000002
#define MODIFIER_CTRL  0x00000004
#define MODIFIER_META  0x00000008
#define MODIFIER_NUML  0x00000010
#define MODIFIER_MOD3  0x00000020
#define MODIFIER_MOD4  0x00000040
#define MODIFIER_MOD5  0x00000080

/**
 * return 1 if a modifier key has been pressed (extracted from XEvent *)
 * modifier pointer will contain the modifier(s) bits (MODIFIER_*)
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier);

/**
 * Create an input text box.
 */
xitk_widget_t *xitk_inputtext_create (xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it);
/**
 * Return the text of widget.
 */
char *xitk_inputttext_get_text(xitk_widget_t *it);
/**
 * Change and redisplay the text of widget.
 */
void xitk_inputtext_change_text(xitk_widget_list_t *wl, xitk_widget_t *it, char *text);


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
 * Unload (free) xitk_skin_config_t object.
 */
void xitk_skin_unload_config(xitk_skin_config_t *);

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
char *xitk_skin_get_label_fontname(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_skin_filename(xitk_skin_config_t *, const char *);

/*
 *
 */
char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *skonfig, const char *str);

/*
 *
 */
int xitk_skin_get_slider_type(xitk_skin_config_t *skonfig, const char *str);
#endif
