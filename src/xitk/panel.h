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
 * xine panel related stuff
 *
 */

#ifndef PANEL_H
#define PANEL_H

void panel_init (void) ;

int panel_is_visible(void) ;

void panel_toggle_visibility (widget_t *w, void *data) ;

void panel_handle_event(XEvent *event) ;

void panel_check_pause(void) ;

void panel_reset_slider ();

void panel_update_channel_display () ;

void panel_update_mrl_display ();

void panel_update_slider () ;

#endif
