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
 */

#ifndef VIEWLOG_H
#define VIEWLOG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

void viewlog_window(void);
void viewlog_exit(xitk_widget_t *, void *);
int viewlog_is_visible(void);
int viewlog_is_running(void);
void viewlog_toggle_visibility(xitk_widget_t *, void *);
void viewlog_raise_window(void);

#endif
