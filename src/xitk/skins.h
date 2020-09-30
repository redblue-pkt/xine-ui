/*
 * Copyright (C) 2000-2020 the xine project
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
 */

#ifndef SKINS_H
#define SKINS_H

int skin_get_names (gGui_t *gui, const char **names, int max);
int skin_add_1 (gGui_t *gui, const char *fullname, const char *name, const char *end);
const char *skin_get_name (gGui_t *gui, int index);
void skin_preinit (gGui_t *gui);
void skin_init (gGui_t *gui);
void skin_deinit (gGui_t *gui);
void skin_select (gGui_t *gui, int selected);
char *skin_get_current_skin_dir (gGui_t *gui);

#ifdef HAVE_TAR
void skin_download (gGui_t *gui, char *url);
void skin_download_end (xui_skdloader_t *skd);
#endif

#endif
