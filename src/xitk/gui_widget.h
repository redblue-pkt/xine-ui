/* 
 * Copyright (C) 2000 the xine project
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
 */

#ifndef HAVE_WIDGET_H
#define HAVE_WIDGET_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gui_widget_types.h"

#define FOCUS_RECEIVED 1
#define FOCUS_LOST     0
#define LBUTTON_DOWN   0
#define LBUTTON_UP     1

#define WIDGET_ENABLE  1

#define BROWSER_MAX_ENTRIES 4096

typedef struct {
  int red;
  int green;
  int blue;
  char *colorname;
} gui_color_names_t;

typedef struct gui_image_s {
  Pixmap image;
  int width;
  int height;
} gui_image_t;

typedef struct gui_color_s {
    XColor red;
    XColor blue;
    XColor green;
    XColor white;
    XColor black;
    XColor tmp;
} gui_color_t;

struct widget_list_s;
typedef struct widget_s {
  int      x,y;
  int      width,height;

  int      enable;

  void (*paint) (struct widget_s *,  Window, GC) ; 
  
  /* notify callback return value : 1 => repaint necessary 0=> do nothing */
                                       /*   parameter: up (1) or down (0) */
  int (*notify_click) (struct widget_list_s *, struct widget_s *,int,int,int);
                                       /*            entered (1) left (0) */
  int (*notify_focus) (struct widget_list_s *, struct widget_s *,int);
 
  void    *private_data;
  int      widget_type;
} widget_t;

typedef struct widget_list_s {

  struct gui_list_s *l;

  widget_t          *focusedWidget;
  widget_t          *pressedWidget;

  Window             win;
  GC                 gc;
} widget_list_t;


void *gui_xmalloc(size_t size);
gui_color_names_t *gui_get_color_names(void);

int paint_widget_list (widget_list_t *wl) ;

int is_inside_widget (widget_t *widget, int x, int y) ;

widget_t *get_widget_at (widget_list_t *wl, int x, int y) ;

void motion_notify_widget_list (widget_list_t *wl, int x, int y) ;

int click_notify_widget_list (widget_list_t *wl, int x, int y, int bUp) ;

int widget_get_width(widget_t *);

int widget_get_height(widget_t *);

int widget_enabled(widget_t *);
void widget_enable(widget_t *);
void widget_disable(widget_t *);

#endif
