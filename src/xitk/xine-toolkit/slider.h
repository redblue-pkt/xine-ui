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

#ifndef HAVE_XITK_SLIDER_H
#define HAVE_XITK_SLIDER_H

#include "_xitk.h"

/*  vertical slider type */
#define XITK_VSLIDER 1
/*  horizontal slider type */
#define XITK_HSLIDER 2
/*  rotate button *slider* type */
#define XITK_RSLIDER 3

typedef struct {
  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_widget_t          *sWidget;
  int                     sType;
  int                     bClicked;
  int                     focus;

  float                   angle;
  float                   percentage;

  float                   upper;
  float                   value;
  float                   lower;
  int                     step;

  int                     radius;

  xitk_image_t           *paddle_skin;
  xitk_image_t           *bg_skin;

  xitk_state_callback_t   motion_callback;
  void                   *motion_userdata;
			  
  xitk_state_callback_t   callback;
  void                   *userdata;

  int                     button_width;
  float                   ratio;

  int                     paddle_cover_bg;

} slider_private_data_t;

#endif
