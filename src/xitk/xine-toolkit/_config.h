/*
 * Copyright (C) 2000-2017 the xine project
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
 *
 */

#ifndef HAVE_XITK_CONFIG_H
#define HAVE_XITK_CONFIG_H

#include <stdio.h>

typedef struct {
  xitk_t           *xitk;

  FILE             *fd;
  char             *cfgfilename;
  char             *ln;
  char              buf[256];

  struct {
    char           *fallback;
    char           *system;
    int             xmb;
  } fonts;

  struct {
    int             black;
    int             white;
    int             background;
    int             focus;
    int             select;
    int             warn_foreground;
    int             warn_background;
  } colors;

  struct {
    int             black;
    int             white;
    int             background;
    int             focus;
    int             select;
    int             warn_foreground;
    int             warn_background;
  } color_vals;

  struct {
    unsigned long   label_anim;
    long int        dbl_click;
  } timers;

  struct {
    int             shm;
    int             oldbarstyle;
    int             checkstyle;
    int             cursors;
  } features;

  struct {
    int             shortcuts;
  } menus;

} xitk_config_t;


xitk_config_t *xitk_config_init (xitk_t *xitk);
void xitk_config_deinit(xitk_config_t *xtcf);
const char *xitk_config_get_string (xitk_config_t *xtcf, xitk_cfg_item_t item);
int xitk_config_get_num (xitk_config_t *xtcf, xitk_cfg_item_t item);
void xitk_config_set_xmb_enability (xitk_config_t *xtcf, int value);

#endif
