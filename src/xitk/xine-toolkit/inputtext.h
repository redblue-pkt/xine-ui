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
 */

#ifndef HAVE_XITK_INPUTTEXT_H
#define HAVE_XITK_INPUTTEXT_H

typedef struct {
  int                     magic;
  char                   *text;
  int                     max_length;
  xitk_string_callback_t  callback;
  void                   *userdata;
  const char             *skin_element_name;
} xitk_inputtext_widget_t;

/** Create an input text box. */
xitk_widget_t *xitk_inputtext_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it);
/** */
xitk_widget_t *xitk_noskin_inputtext_create (xitk_widget_list_t *wl,
  xitk_inputtext_widget_t *it, int x, int y, int width, int height,
  const char *ncolor, const char *fcolor, const char *font_name);
/** Return the text of widget. */
char *xitk_inputtext_get_text (xitk_widget_t *w);
/** Change and redisplay the text of widget. */
void xitk_inputtext_change_text (xitk_widget_t *w, const char *text);


#endif
