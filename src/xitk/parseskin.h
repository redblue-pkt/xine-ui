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

#ifndef GUI_PARSESKIN_H
#define GUI_PARSESKIN_H

#include "xitk.h"

char *gui_get_skindir(const char *);
int gui_get_skinX(const char *);
int gui_get_skinY(const char *);
char *gui_get_skinfile(const char *);
void gui_place_extra_images(widget_list_t *);
char *gui_get_ncolor(const char *str);
char *gui_get_fcolor(const char *str);
char *gui_get_ccolor(const char *str);

#endif
