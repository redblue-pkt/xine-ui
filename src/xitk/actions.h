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
 * all the various actions for the gui (play, stop, open, pause...)
 *
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#include "xitk.h"

#define GUI_NEXT     1
#define GUI_PREV     2

void gui_exit (widget_t *w, void *data);

void gui_play (widget_t *w, void *data);

void gui_stop (widget_t *w, void *data);

void gui_pause (widget_t *w, void *data, int state) ;

void gui_eject(widget_t *w, void *data);

void gui_toggle_fullscreen(widget_t *w, void *data);

void gui_toggle_aspect(void);

void gui_toggle_interlaced(void);

void gui_change_audio_channel(widget_t *w, void *data);

void gui_change_spu_channel(widget_t *w, void *data);

void gui_set_current_position (int pos);

void gui_dndcallback (char *filename);

void gui_nextprev(widget_t *w, void *data);

void gui_playlist_show(widget_t *w, void *data);

void gui_mrlbrowser_show(widget_t *w, void *data);

void gui_set_current_mrl(char *mrl);

char *gui_get_next_mrl (void);

void gui_control_show(widget_t *w, void *data);

void gui_mrl_browser_show(widget_t *w, void *data);

#endif
