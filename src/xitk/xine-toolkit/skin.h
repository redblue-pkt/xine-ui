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
 */
#ifndef HAVE_XITK_SKIN_H
#define HAVE_XITK_SKIN_H

struct xitk_skin_element_info_s {
  /* all */
  int x, y;
  int visibility, enability;
  char *pixmap_name;
  xitk_part_image_t pixmap_img;
  /* button list */
  int max_buttons, direction;
  /* browser */
  int browser_entries;
  /* label */
  int label_length, label_alignment, label_y, label_printable, label_staticity;
  int label_animation, label_animation_step;
  unsigned long int label_animation_timer;
  char *label_color, *label_color_focus, *label_color_click, *label_fontname;
  char *label_pixmap_font_name, *label_pixmap_highlight_font_name;
  char *label_pixmap_font_format;
  xitk_image_t *label_pixmap_font_img, *label_pixmap_highlight_font_img;
  /* slider */
  int slider_type, slider_radius;
  char *slider_pixmap_pad_name;
  xitk_part_image_t slider_pixmap_pad_img;
};

/* Alloc a xitk_skin_config_t* memory area, nullify pointers. */
xitk_skin_config_t *xitk_skin_init_config (xitk_t *xitk);
/* Load the skin configfile. */
int xitk_skin_load_config (xitk_skin_config_t *skonfig, const char *path, const char *filename);

void xitk_skin_lock (xitk_skin_config_t *skonfig);
/* Check skin version.
 * return: 0 if version found < min_version
 *         1 if version found == min_version
 *         2 if version found > min_version
 *        -1 if no version found */
int xitk_skin_check_version (xitk_skin_config_t *skonfig, int min_version);
/* Misc info */
const char *xitk_skin_get_logo (xitk_skin_config_t *skonfig);
const char *xitk_skin_get_animation (xitk_skin_config_t *skonfig);
/* Query element by name */
const xitk_skin_element_info_t *xitk_skin_get_info (xitk_skin_config_t *skin, const char *element_name);

void xitk_skin_unlock (xitk_skin_config_t *skonfig);
/* Unload (free) xitk_skin_config_t object. */
void xitk_skin_unload_config (xitk_skin_config_t *skonfig);

#endif
