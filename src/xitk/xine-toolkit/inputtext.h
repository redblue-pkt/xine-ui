/* 
 * Copyright (C) 2000-2002 the xine project
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

#ifndef HAVE_XITK_INPUTTEXT_H
#define HAVE_XITK_INPUTTEXT_H

#include <X11/Xlib.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"

#include "_xitk.h"

#define MODIFIER_NOMOD 0x00000000
#define MODIFIER_SHIFT 0x00000001
#define MODIFIER_LOCK  0x00000002
#define MODIFIER_CTRL  0x00000004
#define MODIFIER_META  0x00000008
#define MODIFIER_NUML  0x00000010
#define MODIFIER_MOD3  0x00000020
#define MODIFIER_MOD4  0x00000040
#define MODIFIER_MOD5  0x00000080

typedef struct {

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *iWidget;

  xitk_image_t           *skin;

  xitk_string_callback_t  callback;
  void                   *userdata;

  char                   *text;
  char                   *fontname;
  char                   *normal_color;
  char                   *focused_color;

  int                     have_focus;
  int                     max_length;
  int                     max_visible;
  int                     disp_offset;
  int                     cursor_pos;
  
  unsigned int            pos[128];
  int                     pos_in_pos;

} inputtext_private_data_t;

/* ***************************************************************** */

/*
 * Create an input text widget.
 */
xitk_widget_t *xitk_inputtext_create(xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it);

/*
 * Same as above, without skinable feature.
 */
xitk_widget_t *xitk_noskin_inputtext_create (xitk_inputtext_widget_t *it,
					     int x, int y, int width, int height,
					     char *ncolor, char *fcolor, char *fontname);

/*
 * Return current text in inputtext widget.
 */
char *xitk_inputtext_get_text(xitk_widget_t *it);

/*
 * Set text in inputtext widget.
 */
void xitk_inputtext_change_text(xitk_widget_list_t *wl, xitk_widget_t *it, char *text);

/*
 * Return key modifier from an xevent struct.
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier);

#endif
