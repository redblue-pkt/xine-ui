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
 * File Browser
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define NEED_FILEBROWSER
#ifdef NEED_FILEBROWSER

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <xine/xineutils.h>

#include "xitk.h"

#include "event.h"
#include "actions.h"
#include "file_browser.h"

#define MAX_LIST 9

extern gGui_t       *gGui;
static xitk_widget_t     *fb = NULL;

/*
 *
 */
void file_browser_change_skins(void) {

  if(fb)
    xitk_filebrowser_change_skins(fb, gGui->skin_config);
}

/*
 *
 */
int file_browser_is_visible(void) {

  if(fb)
    return(xitk_filebrowser_is_visible(fb));
  
  return 0;
}

/*
 *
 */
int file_browser_is_running(void) {

  if(fb)
    return(xitk_filebrowser_is_running(fb));
  
  return 0;
}

/*
 *
 */
void set_file_browser_transient(void) {

  if(fb) {
    xitk_filebrowser_set_transient(fb, gGui->video_window);
  }
}

/*
 *
 */
void show_file_browser(void) {

  if(fb) {
    xitk_filebrowser_show(fb);
    set_file_browser_transient();
    layer_above_video((xitk_filebrowser_get_window_id(fb)));
  }
}

/*
 *
 */
void hide_file_browser(void) {

  if(fb) {
    xitk_filebrowser_hide(fb);
  }
}

/*
 *
 */
void file_browser_toggle_visibility(void) {

  if(fb) {

    if(file_browser_is_visible())
      hide_file_browser();
    else
      show_file_browser();

  }
}
/*
 *
 */
void destroy_file_browser(void) {
  window_info_t wi;

  if(fb) {
    if((xitk_filebrowser_get_window_info(fb, &wi))) {
      config_set_int("x_file_browser", wi.x);
      config_set_int("y_file_browser", wi.y);
    }
    xitk_filebrowser_destroy(fb);
    fb = NULL;
  }
}

/*
 *
 */
static void file_browser_kill(xitk_widget_t *w, void *data) {
  char *curdir = xitk_filebrowser_get_current_dir(fb);
  window_info_t wi;
  
  if(curdir) {
    config_set_str("filebrowser_dir", curdir);
    if(fb) {
      if((xitk_filebrowser_get_window_info(fb, &wi))) {
	config_set_int("x_file_browser", wi.x);
	config_set_int("y_file_browser", wi.y);
      }
    }
  }
  
  fb = NULL;
}

/*
 *
 */
void file_browser(xitk_string_callback_t add_cb, 
		  select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb) {
  xitk_filebrowser_widget_t fbr;
  
  if(fb != NULL) {
    show_file_browser();
    set_file_browser_transient();
    return;
  }

  fbr.imlibdata                      = gGui->imlib_data;
  fbr.window_trans                   = gGui->video_window;
  fbr.layer_above                    = gGui->layer_above;

  fbr.x                              = config_lookup_int("x_file_browser", 200);
  fbr.y                              = config_lookup_int("y_file_browser", 100);
  fbr.window_title                   = _("Xine File Browser");
  fbr.skin_element_name              = "FBBG";
  fbr.resource_name                  = fbr.window_title;
  fbr.resource_class                 = "Xine";

  fbr.sort_default.skin_element_name = "FBSortDef";

  fbr.sort_reverse.skin_element_name = "FBSortRev";
  
  fbr.current_dir.skin_element_name  = "FBCurDir";
  fbr.current_dir.cur_directory      = config_lookup_str("filebrowser_dir",
							 (char *)xine_get_homedir());

  fbr.dndcallback                    = dnd_cb;

  fbr.homedir.skin_element_name      = "FBHome";
  fbr.homedir.caption                = "~/";

  fbr.select.skin_element_name       = "FBSelect";
  fbr.select.caption                 = _("Select");
  fbr.select.callback                = add_cb;

  fbr.dismiss.skin_element_name      = "FBDismiss";

  fbr.kill.callback                  = file_browser_kill;

  /* The browser */  
  fbr.browser.imlibdata              = gGui->imlib_data;

  fbr.browser.arrow_up.skin_element_name = "FBUp";

  fbr.browser.slider.skin_element_name = "SliderFB";

  fbr.browser.arrow_dn.skin_element_name = "FBDn";

  fbr.browser.browser.skin_element_name = "FBItemBtn";
  fbr.browser.browser.max_displayed_entries = MAX_LIST;
  fbr.browser.browser.num_entries    = 0;
  fbr.browser.browser.entries        = NULL;

  fbr.browser.callback               = sel_cb;
  fbr.browser.userdata               = NULL;

  fb = xitk_filebrowser_create(gGui->skin_config, &fbr);

}

#endif
