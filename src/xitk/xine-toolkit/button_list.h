/*
 * Copyright (C) 2019 the xine project
 * 
 * This file is part of xine, a free video player.
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
 * A list of label buttons, optionally with a ... to make all choices available.
 */

#ifndef XITK_BUTTON_LIST_H
#define XITK_BUTTON_LIST_H

#include "xitk.h"

typedef struct xitk_button_list_st xitk_button_list_t;

xitk_button_list_t *xitk_button_list_new (
  ImlibData *imlib_data, xitk_widget_list_t *widget_list,
  xitk_skin_config_t *skin_config, const char *skin_element_name,
  xitk_simple_callback_t callback, void *callback_data,
  char **names,
  char **tips, int tips_timeout, uint32_t widget_type_flags);
void xitk_button_list_new_skin (xitk_button_list_t *bl, xitk_skin_config_t *skin_config);
xitk_widget_t *xitk_button_list_find (xitk_button_list_t *xitk_bl, const char *name);
void xitk_button_list_able (xitk_button_list_t *xitk_bl, int enable);
void xitk_button_list_delete (xitk_button_list_t *xitk_bl);

#endif