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

#ifndef EVENT_H
#define EVENT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>

#include "common.h"

/*
 * Configuration file convenience functions
 */

int actions_on_start(action_id_t actions[], action_id_t a);

void gui_init(int nfiles, char *filenames[], window_attributes_t *window_attribute);

void gui_init_imlib(Visual *vis);

void gui_run (void);

void gui_playlist_start_next(void);

void gui_dndcallback(char *filename);

void gui_execute_action_id(action_id_t);

void gui_handle_event(XEvent *event, void *data);

#endif
