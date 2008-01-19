/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */

#ifndef TVOUT_H
#define TVOUT_H

typedef struct tvout_s tvout_t;

tvout_t *tvout_init(Display *display, char *backend);
int tvout_setup(tvout_t *tvout);
void tvout_get_size_and_aspect(tvout_t *tvout, int *width, int *height, double *pix_aspect);
int tvout_set_fullscreen_mode(tvout_t *tvout, int fullscreen, int width, int height);
int tvout_get_fullscreen_mode(tvout_t *tvout);
void tvout_deinit(tvout_t *tvout);
const char **tvout_get_backend_names(void);

#endif
