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

#ifndef SETUP_H
#define SETUP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

void setup_panel(void);
int setup_is_visible(void);
int setup_is_running(void);
void setup_toggle_visibility(xitk_widget_t *, void *);
void setup_raise_window(void);
void setup_reparent(void);
void setup_end(void);
void setup_show_tips(int enabled, unsigned long timeout);
void setup_update_tips_timeout(unsigned long timeout);

#endif
