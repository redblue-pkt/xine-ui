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
} gui_move_t;

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
  int       width;
  int       height;
} gui_image_t;

typedef struct {
  int       red;
  int       green;
  int       blue;
  char     *colorname;
} gui_color_names_t;

typedef struct gui_node_s {
  struct gui_node_s    *next, *prev;
  void                 *content;
} gui_node_t;


typedef struct {
  gui_node_t           *first, *last, *cur;
} gui_list_t;

struct widget_list_s;
typedef struct widget_s {
  int                  x, y;
  int                  width, height;

  int                  enable;

  void               (*paint) (struct widget_s *,  Window, GC) ; 
  
  int                (*notify_click) (struct widget_list_s *, 
				      struct widget_s *,int,int,int);

  int                (*notify_focus) (struct widget_list_s *, 
				      struct widget_s *,int);
 
  void                *private_data;
  int                  widget_type;
} widget_t;

typedef struct widget_list_s {
  gui_list_t          *l;

  widget_t            *focusedWidget;
  widget_t            *pressedWidget;

  Window               win;
  GC                   gc;
} widget_list_t;


typedef void (*xitk_simple_callback_t)(widget_t *, void *);
typedef void (*xitk_state_callback_t)(widget_t *, void *, int);
typedef void (*xitk_string_callback_t)(widget_t *, void *, char *);
typedef void (*xitk_dnd_callback_t) (char *filename);
#ifdef NEED_MRLBROWSER
typedef void (*xitk_mrl_callback_t)(widget_t *, void *, mrl_t *);
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
typedef void (*widget_cb_event_t)(XEvent *event, void *data);

/*
 * New positioning window callback function.
 * This callback will be called when the window
 * moving will be done (at button release time),
 * and, of course, only if there was a window
 * specified at register time.
 */
typedef void (*widget_cb_newpos_t)(int, int, int, int);

/*
 * A unique key returned by register function.
 * It is necessary to store it at program side,
 * because it will be necessary to pass it for
 * identification of caller.
 */
typedef int widgetkey_t;

/*
 * Create a lew widget list, store it internaly,
 * then return the pointer to app.
 */
widget_list_t *widget_list_new (void);

/*
 * Humm, this should be probably removed soon.
 */
void widget_change_window_for_event_handler (widgetkey_t key, Window window);

/*
 * Register function:
 * name:   temporary name about debug stuff, can be NULL.
 * window: desired window for event callback calls, can be None.
 * cb:     callback for xevent, can be NULL.
 * pos_cb; callback for window moving.
 * dnd_cb: callback for dnd event.
 * wl:     widget_list handled internaly for xevent reactions.
 */
widgetkey_t widget_register_event_handler(char *name, Window window,
					  widget_cb_event_t cb,
					  widget_cb_newpos_t pos_cb,
					  xitk_dnd_callback_t dnd_cb,
					  widget_list_t *wl, void *user_data);

/*
 * Remove widgetkey_t entry in internal table.
 */
void widget_unregister_event_handler(widgetkey_t *key);

/*
 * Copy window information matching with key in passed window_info_t struct.
 */

int widget_get_window_info(widgetkey_t key, window_info_t *winf);
/*
 * Initialization function, should be the first call to widget lib.
 */
void widget_init(Display *display);

/*
 * This function start the widget live. It's a block function,
 * it will only return after a widget_stop() call.
 */
void widget_run(void);

/*
 * This function terminate the widget lib loop event. 
 * Other functions of the lib shouldn't be called after this
 * one.
 */
void widget_stop(void);

/*
 *
 ****** */

/**
 * Allocate an clean memory of "size" bytes.
 */
void *gui_xmalloc(size_t);

/**
 * return pointer to the gui_color_names struct.
 */
gui_color_names_t *gui_get_color_names(void);

/**
 * (re)Paint a widget list.
 */
int paint_widget_list (widget_list_t *wl) ;

/**
 * Boolean function, if x and y coords is in widget.
 */
int is_inside_widget (widget_t *widget, int x, int y) ;

/**
 * Return widget from widget list 'wl' localted at x,y coords.
 */
widget_t *get_widget_at (widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if motion happend at x, y coords.
 */
void motion_notify_widget_list (widget_list_t *wl, int x, int y) ;

/**
 * Notify widget (if enabled) if click event happend at x, y coords.
 */
int click_notify_widget_list (widget_list_t *wl, int x, int y, int bUp) ;

/**
 *
 */
void widget_send_key_event(widget_list_t *, widget_t *, XEvent *);

/**
 * Return width (in pixel) of widget.
 */
int widget_get_width(widget_t *);

/**
 * Return height (in pixel) of widget.
 */
int widget_get_height(widget_t *);

/**
 * Boolean, return 1 if widget 'w' have focus.
 */
int widget_have_focus(widget_t *);

/**
 * Boolean, enable state of widget.
 */
int widget_enabled(widget_t *);

/**
 * Enable widget.
 */
void widget_enable(widget_t *);

/**
 * Disable widget.
 */
void widget_disable(widget_t *);

/**
 * Stop each (if widget handle it) widgets of widget list.
 */
void widget_stop_widgets(widget_list_t *wl);

/**
 * Set widgets of widget list visible.
 */
void widget_show_widgets(widget_list_t *);

/**
 * Set widgets of widget list not visible.
 */
void widget_hide_widgets(widget_list_t *);

/**
 * Pass events to UI
 */
void widget_xevent_notify(XEvent *event);

/* 
 * *** Sliders ***
 */

/*  To handle a vertical slider */
#define VSLIDER 1
/*  To handle an horizontal slider */
#define HSLIDER 2

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  int                     slider_type;
  int                     min;
  int                     max;
  int                     step;
  char                   *background_skin;
  char                   *paddle_skin;
  xitk_state_callback_t   callback;
  void                   *userdata;
  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
} xitk_slider_t;

/**
 * Create a slider
 */
widget_t *slider_create(xitk_slider_t *sl);

/**
 * Get current position of paddle.
 */
int slider_get_pos(widget_t *);

/**
 * Set position of paddle.
 */
void slider_set_pos(widget_list_t *, widget_t *, int);

/**
 * Set min value of slider.
 */
void slider_set_min(widget_t *, int);

/**
 * Set max value of slider.
 */
void slider_set_max(widget_t *, int);

/**
 * Get min value of slider.
 */
int slider_get_min(widget_t *);

/**
 * Get max value of slider.
 */
int slider_get_max(widget_t *);

/**
 * Set position to 0 and redraw the widget.
 */
void slider_reset(widget_list_t *, widget_t *);

/**
 * Set position to max and redraw the widget.
 */
void slider_set_to_max(widget_list_t *, widget_t *);

/**
 * Increment by step the paddle position
 */
void slider_make_step(widget_list_t *, widget_t *);

/**
 * Decrement by step the paddle position.
 */
void slider_make_backstep(widget_list_t *, widget_t *);


/*
 * *** Nodes ***
 */
/**
 * Create a new list.
 */
gui_list_t *gui_list_new (void);

/**
 * Freeing list.
 */
void gui_list_free(gui_list_t *l);

/**
 * Boolean, status of list.
 */
int gui_list_is_empty (gui_list_t *l);

/**
 * return content of first entry in list.
 */
void *gui_list_first_content (gui_list_t *l);

/**
 * return next content in list.
 */
void *gui_list_next_content (gui_list_t *l);

/**
 * Return last content of list.
 */
void *gui_list_last_content (gui_list_t *l);

/**
 * Return previous content of list.
 */
void *gui_list_prev_content (gui_list_t *l);

/**
 * Append content to list.
 */
void gui_list_append_content (gui_list_t *l, void *content);

/**
 * Insert content in list. NOT IMPLEMENTED
 */
void gui_list_insert_content (gui_list_t *l, void *content);

/**
 * Remove current content in list. NOT IMPLEMENTED
 */
void gui_list_delete_current (gui_list_t *l);



/*
 * *** Label Buttons
 */
#define CLICK_BUTTON 1
#define RADIO_BUTTON 2
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  int                     button_type;
  char                   *label;
  char                   *fontname;

  xitk_simple_callback_t  callback;
  xitk_state_callback_t   state_callback;
  void                   *userdata;
  char                   *skin;
  char                   *normcolor;
  char                   *focuscolor;
  char                   *clickcolor;
} xitk_labelbutton_t;

/**
 * Create a labeled button.
 */
widget_t *label_button_create(xitk_labelbutton_t *b);

/**
 * Change label of button 'widget'.
 */
int labelbutton_change_label(widget_list_t *wl, widget_t *, char *);

/**
 * Return label of button 'widget'.
 */
char *labelbutton_get_label(widget_t *);

/**
 * Get state of button 'widget'.
 */
int labelbutton_get_state(widget_t *);

/**
 * Set state of button 'widget'.
 */
void labelbutton_set_state(widget_t *, int, Window, GC);


/*
 * *** Labels
 */
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  Window                  window;
  GC                      gc;
  int                     x;
  int                     y;
  int                     length;
  char                   *label;
  char                   *font;
  int                     animation;
} xitk_label_t;

/**
 * Create a label widget.
 */
widget_t *label_create(xitk_label_t *l);

/**
 * Change label of widget 'widget'.
 */
int label_change_label(widget_list_t *wl, widget_t *l, char *newlabel);


/*
 * *** Image
 */

/**
 * Load image and return a gui_image_t data type.
 */
gui_image_t *gui_load_image(ImlibData *idata, char *image);

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  char                   *skin;
} xitk_image_t;

/**
 * Create an image widget type.
 */
widget_t *image_create(xitk_image_t *im);

/*
 * *** DND
 */
void dnd_init_dnd(Display *display, xitk_dnd_t *);

void dnd_make_window_aware (xitk_dnd_t *, Window);

Bool dnd_process_client_message(xitk_dnd_t *, XEvent *);

void dnd_set_callback (xitk_dnd_t *, void *);


/*
 * *** Checkbox
 */
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  xitk_state_callback_t  callback;
  void                   *userdata;
  char                   *skin;
} xitk_checkbox_t;
/**
 * Create a checkbox.
 */
widget_t *checkbox_create (xitk_checkbox_t *cp);

/**
 * get state of checkbox "widget".
 */
int checkbox_get_state(widget_t *);

/**
 * Set state of checkbox .
 */
void checkbox_set_state(widget_t *, int, Window, GC);

/*
 * ** Buttons
 */
/**
 * Create a button
 */
typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  xitk_simple_callback_t  callback;
  void                   *userdata;
  char                   *skin;
} xitk_button_t;

widget_t *button_create (xitk_button_t *b);


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
    char                 *fontname;
    
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

widget_t *browser_create(xitk_browser_t *b);

/**
 * Redraw buttons/slider
 */
void browser_rebuild_browser(widget_t *w, int start);
/**
 * Update the list, and rebuild button list
 */
void browser_update_list(widget_t *w, char **list, int len, int start);
/**
 * slide up.
 */
void browser_step_up(widget_t *w, void *data);
/**
 * slide Down.
 */
void browser_step_down(widget_t *w, void *data);
/**
 * Return the current selected button (if not, return -1)
 */
int browser_get_current_selected(widget_t *w);
/**
 * Select the item 'select' in list
 */
void browser_set_select(widget_t *w, int select);
/**
 * Release all enabled buttons
 */
void browser_release_all_buttons(widget_t *w);
/**
 * Return the real number of first displayed in list
 */
int browser_get_current_start(widget_t *w);

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
    char                    *fontname;
  } homedir;

  struct {
    int                      x;
    int                      y;
    char                    *caption;
    char                    *skin_filename;
    char                    *normal_color;
    char                    *focused_color;
    char                    *clicked_color;
    char                    *fontname;
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
    char                    *fontname;
  } dismiss;

  struct {
    xitk_simple_callback_t   callback;
  } kill;
 
  xitk_browser_t             browser;
} xitk_filebrowser_t;

widget_t *filebrowser_create(xitk_filebrowser_t *fb);
int filebrowser_is_running(widget_t *w);
int filebrowser_is_visible(widget_t *w);
void filebrowser_hide(widget_t *w);
void filebrowser_show(widget_t *w);
void filebrowser_set_transient(widget_t *w, Window window);
void filebrowser_destroy(widget_t *w);
char *filebrowser_get_current_dir(widget_t *w);
int filebrowser_get_window_info(widget_t *w, window_info_t *inf);

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
    char                    *fontname;
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
    char                    *fontname;
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
      char                  *fontname;
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

widget_t *mrlbrowser_create(xitk_mrlbrowser_t *mb);
int mrlbrowser_is_running(widget_t *w);
int mrlbrowser_is_visible(widget_t *w);
void mrlbrowser_hide(widget_t *w);
void mrlbrowser_show(widget_t *w);
void mrlbrowser_set_transient(widget_t *w, Window window);
void mrlbrowser_destroy(widget_t *w);
int mrlbrowser_get_window_info(widget_t *w, window_info_t *inf);

#endif

typedef struct {
  Display                *display;
  ImlibData              *imlibdata;
  int                     x;
  int                     y;
  char                   *text;
  int                     max_length;

  char                   *fontname;

  xitk_string_callback_t  callback;
  void                   *userdata;

  char                   *skin_filename;
  char                   *normal_color;
  char                   *focused_color;

} xitk_inputtext_t;

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
widget_t *inputtext_create (xitk_inputtext_t *it);
/**
 * Return the text of widget.
 */
char *inputttext_get_text(widget_t *it);
/**
 * Change and redisplay the text of widget.
 */
void inputtext_change_text(widget_list_t *wl, widget_t *it, char *text);


#endif
