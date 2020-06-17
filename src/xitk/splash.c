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

#include "common.h"

static xitk_window_t     *xwin = NULL;

void splash_create(void) {
  xitk_image_t *xim;
  const char   *splash_image = XINE_SPLASH;
  char         *skin_splash_image = NULL;
  char         *skin_path = skin_get_current_skin_dir();
  
  if(skin_path && is_a_dir((char *) skin_path)) {
    static const char types[][4] = { "png", "jpg" };
    size_t i;

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

  if((xim = xitk_image_load_image(gGui->xitk, splash_image))) {
    int  x, y, width, height;

    width = xitk_image_width(xim);
    height = xitk_image_height(xim);

    x = (xitk_get_display_width() >> 1) - (width >> 1);
    y = (xitk_get_display_height() >> 1) - (height >> 1);

    xwin = xitk_window_create_simple_window_ext(gGui->xitk, x, y, width, height,
                                                _("xine Splash"), NULL, "xine", 0, 1, gGui->icon);
    xitk_window_set_wm_window_type(xwin, WINDOW_TYPE_SPLASH);

    xitk_window_change_background_with_image(xwin, xim, width, height);
    xitk_window_show_window(xwin, 1);

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
