/* 
 * Copyright (C) 2000-2004 the xine project
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

#ifndef TVSET_H
#define TVSET_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

void tvset_panel(void);
void tvset_end(void);
int tvset_is_visible(void);
int tvset_is_running(void);
void tvset_toggle_visibility(xitk_widget_t *, void *);
void tvset_raise_window(void);
void tvset_reparent(void);

#endif
