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
 *
 */

#ifndef HAVE_XITK_CONFIG_H
#define HAVE_XITK_CONFIG_H

#include <stdio.h>

typedef struct {
  FILE             *fd;
  char             *cfgfilename;
  char             *ln;
  char              buf[256];
  
  struct {
    char           *fallback;
    char           *system;
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
    unsigned long   label_anim;
    long int        dbl_click;
  } timers;

  struct {
    int             shm;
  } features;

} xitk_config_t;


xitk_config_t *xitk_config_init(void);
void xitk_config_deinit(xitk_config_t *xtcf);
int xitk_config_get_shm_feature(xitk_config_t *xtcf);
char *xitk_config_get_system_font(xitk_config_t *xtcf);
char *xitk_config_get_default_font(xitk_config_t *xtcf);
int xitk_config_get_black_color(xitk_config_t *xtcf);
int xitk_config_get_white_color(xitk_config_t *xtcf);
int xitk_config_get_background_color(xitk_config_t *xtcf);
int xitk_config_get_focus_color(xitk_config_t *xtcf);
int xitk_config_get_select_color(xitk_config_t *xtcf);
unsigned long xitk_config_get_timer_label_animation(xitk_config_t *xtcf);
long xitk_config_get_timer_dbl_click(xitk_config_t *xtcf);
unsigned long xitk_config_get_warning_foreground(xitk_config_t *xtcf);
unsigned long xitk_config_get_warning_background(xitk_config_t *xtcf);

#endif
