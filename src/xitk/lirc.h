/*
 * Copyright (C) 2000-2021 the xine project
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
 * lirc specific stuff
 *
 */

#ifndef LIRC_H
#define LIRC_H

#ifdef HAVE_LIRC
void lirc_start (gGui_t *gui);
void lirc_stop (gGui_t *gui);
#else
#define lirc_start(_gui) (void)(_gui)
#define lirc_stop(_gui) (void)(_gui)
#endif

#endif
