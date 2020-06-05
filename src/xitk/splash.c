/* 
 * Copyright (C) 2000-2019 the xine project
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
 * gui initialization and top-level event handling stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#include "common.h"

static xitk_window_t     *xwin = NULL;

void splash_create(void) {
  xitk_image_t *xim;
  const char   *splash_image = XINE_SPLASH;
  char         *skin_splash_image = NULL;
  char         *skin_path = skin_get_current_skin_dir();
  
  if(skin_path && is_a_dir((char *) skin_path)) {
    static const char types[][4] = { "png", "jpg" };
    int i;

    for (i = 0; i < sizeof (types) / sizeof (types[0]); ++i)
    {
      skin_splash_image = xitk_asprintf("%s/xine_splash.%s", skin_path, types[i]);

      if(skin_splash_image && is_a_file(skin_splash_image))
      {
        splash_image = skin_splash_image;
        break;
      }
      free (skin_splash_image);
      skin_splash_image = NULL;
    }
  }

  free(skin_path);

  if((xim = xitk_image_load_image(gGui->imlib_data, splash_image))) {
    int  x, y;
    
    gGui->x_lock_display (gGui->display);
    x = (((DisplayWidth(gGui->display, gGui->screen))) >> 1) - (xim->width >> 1);
    y = (((DisplayHeight(gGui->display, gGui->screen))) >> 1) - (xim->height >> 1);
    gGui->x_unlock_display (gGui->display);
    
    xwin = xitk_window_create_simple_window(gGui->imlib_data, x, y, xim->width, xim->height);
    xitk_window_set_wm_window_type(xwin, WINDOW_TYPE_SPLASH);

    gGui->x_lock_display (gGui->display);
    XStoreName(gGui->display, (xitk_window_get_window(xwin)), _("xine Splash"));
    gGui->x_unlock_display (gGui->display);

    xitk_window_set_window_class(xwin, NULL, "xine");
    xitk_window_set_window_icon(xwin, gGui->icon);

    xitk_window_change_background_with_image(xwin, xim, xim->width, xim->height);
    xitk_window_show_window(xwin, 1);
    xitk_window_set_layer_above(xwin);

    xitk_image_free_image(&xim);
  }

  free(skin_splash_image);
}

void splash_destroy(void) {

  if(xwin) {
    xitk_window_hide_window(xwin);
    xitk_window_destroy_window(xwin);
    xwin = NULL;
  }
}
