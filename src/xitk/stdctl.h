/* 
 * Copyright (C) 2000-2007 the xine project
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
 * stdctl specific stuff
 *
 */

#ifndef STDCTL_H
#define STDCTL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void stdctl_start(void);
void stdctl_stop(void);
void stdctl_event(const xine_event_t *event);
void stdctl_keypress(XKeyEvent event);
void stdctl_playing(const char *mrl);

#endif
