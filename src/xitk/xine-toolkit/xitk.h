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

#include <stdlib.h>
#include <X11/Xlib.h>
#include "Imlib-light/Imlib.h"

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

typedef struct DND_struct_s {
  Display             *display;
  Window               win;
  
  void               (*callback) (char *fname);

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
} DND_struct_t;

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
 * Return width (in pixel) of widget.
 */
int widget_get_width(widget_t *);

/**
 * Return height (in pixel) of widget.
 */
int widget_get_height(widget_t *);

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


/* 
 * *** Sliders ***
 */

/*  To handle a vertical slider */
#define VSLIDER 1
/*  To handle an horizontal slider */
#define HSLIDER 2

/**
 * Create a slider
 */
widget_t *create_slider(Display *display, ImlibData *idata,
			int type, int x, int y, int min, int max, 
			int step, const char *bg, const char *paddle, 
/* cb for motion      */void *fm, void *udm,
/* cb for btn release */void *f, void *ud) ;

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
/**
 * Create a labeled button.
 */
widget_t *create_label_button (Display *display, ImlibData *idata,
			       int x, int y, int btype, const char *label, 
			       void* f, void* ud, const char *skin,
 			       const char *normcolor, const char *focuscolor, 
			       const char *clickcolor);

/**
 * Change label of button 'widget'.
 */
int labelbutton_change_label(widget_list_t *wl, widget_t *, const char *);

/**
 * Return label of button 'widget'.
 */
const char *labelbutton_get_label(widget_t *);

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
/**
 * Create a label widget.
 */
widget_t *create_label (Display *display, ImlibData *idata,
			int x, int y, int length, const char *label, char *bg);

/**
 * Change label of wodget 'widget'.
 */
int label_change_label (widget_list_t *wl, widget_t *l, const char *newlabel);


/*
 * *** Image
 */

/**
 * Load image and return a gui_image_t data type.
 */
gui_image_t *gui_load_image(ImlibData *idata, const char *image);

/**
 * Create an image widget type.
 */
widget_t *create_image (Display *display, ImlibData *idata,
			int x, int y, const char *skin) ;


/*
 * *** DND
 */
void dnd_init_dnd(Display *display, DND_struct_t *);

void dnd_make_window_aware (DND_struct_t *, Window);

Bool dnd_process_client_message(DND_struct_t *, XEvent *);

void dnd_set_callback (DND_struct_t *, void *);


/*
 * *** Checkbox
 */
/**
 * Create a checkbox.
 */
widget_t *create_checkbox (Display *display, ImlibData *idata,
			   int x, int y, void* f, void* ud, const char *skin) ;
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
widget_t *create_button (Display *display, ImlibData *idata,
			 int x, int y, void* f, void* ud, const char *skin) ;


/*
 * *** Browser
 */
/**
 * Create the list browser
 */
widget_t *create_browser(Display *display, ImlibData *idata,
			 /* The receiver list */
			 widget_list_t *thelist, 
			 /* X, Y, skin for slider up button */
			 int upX, int upY, char *upSK,
			 /* X, Y, backgrnd, foregrnd skin for slider */
			 int slX, int slY, char *slSKB, char *slSKF,
			 /* X, Y, skin for slider up button */
			 int dnX, int dnY, char *dnSK,
			 /* X, Y for item buttons */
			 int btnX, int btnY, 
			 /* normal/focus/click colors, skin for item buttons */
			 char *btnN, char *btnF, char *btnC, char *btnSK,
			 /* max item displayed, length of content, content */
			 int maxlength, int listlength, char **content,
			 /* callback and data to pass to callback */
			 void *selCB, void *data);

/**
 * Redraw buttons/slider
 */
void browser_rebuild_browser(widget_t *w, int start);
/**
 * Update the list, and rebuild button list
 */
void browser_update_list(widget_t *w, char **list, int len, int start);
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

#endif
