/* 
 * Copyright (C) 2000-2001 the xine project
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

#ifdef NEED_FILEBROWSER

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "xitk.h"

#include "event.h"
#include "utils.h"
#include "parseskin.h"
#include "file_browser.h"

#define MAX_LIST 9

extern gGui_t       *gGui;
static widget_t     *fb = NULL;

/*
 *
 */
int file_browser_is_visible(void) {

  if(fb)
    return(filebrowser_is_visible(fb));
  
  return 0;
}

/*
 *
 */
int file_browser_is_running(void) {

  if(fb)
    return(filebrowser_is_running(fb));
  
  return 0;
}

/*
 *
 */
void set_file_browser_transient(void) {

  if(fb) {
    filebrowser_set_transient(fb, gGui->video_window);
  }
}

/*
 *
 */
void show_file_browser(void) {

  if(fb) {
    filebrowser_show(fb);
    set_file_browser_transient();
  }
}

/*
 *
 */
void hide_file_browser(void) {

  if(fb) {
    filebrowser_hide(fb);
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
    if((filebrowser_get_window_info(fb, &wi))) {
      config_set_int("x_file_browser", wi.x);
      config_set_int("y_file_browser", wi.y);
    }
    filebrowser_destroy(fb);
    fb = NULL;
  }
}

/*
 *
 */
static void file_browser_kill(widget_t *w, void *data) {
  char *curdir = filebrowser_get_current_dir(fb);
  window_info_t wi;
  
  if(curdir) {
    config_set_str("filebrowser_dir", curdir);
    if(fb) {
      if((filebrowser_get_window_info(fb, &wi))) {
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
  xitk_filebrowser_t fbr;
  
  if(fb != NULL) {
    show_file_browser();
    set_file_browser_transient();
    return;
  }

  fbr.display                        = gGui->display;
  fbr.imlibdata                      = gGui->imlib_data;
  fbr.window_trans                   = gGui->video_window;

  fbr.x                              = config_lookup_int("x_file_browser", 200);
  fbr.y                              = config_lookup_int("y_file_browser", 100);
  fbr.window_title                   = "Xine File Browser";
  fbr.background_skin                = gui_get_skinfile("FBBG");
  fbr.resource_name                  = fbr.window_title;
  fbr.resource_class                 = "Xine";

  fbr.sort_default.x                 = gui_get_skinX("FBSortDef");
  fbr.sort_default.y                 = gui_get_skinY("FBSortDef");
  fbr.sort_default.skin_filename     = gui_get_skinfile("FBSortDef");

  fbr.sort_reverse.x                 = gui_get_skinX("FBSortRev");
  fbr.sort_reverse.y                 = gui_get_skinY("FBSortRev");
  fbr.sort_reverse.skin_filename     = gui_get_skinfile("FBSortRev");

  fbr.current_dir.x                  = gui_get_skinX("FBCurDir");
  fbr.current_dir.y                  = gui_get_skinY("FBCurDir");
  fbr.current_dir.skin_filename      = gui_get_skinfile("FBCurDir");
  fbr.current_dir.max_length         = 50;
  fbr.current_dir.cur_directory      = config_lookup_str("filebrowser_dir",
							 (char *)get_homedir());
  fbr.dndcallback                    = dnd_cb;

  fbr.homedir.x                      = gui_get_skinX("FBHome");
  fbr.homedir.y                      = gui_get_skinY("FBHome");
  fbr.homedir.caption                = "~/";
  fbr.homedir.skin_filename          = gui_get_skinfile("FBHome");
  fbr.homedir.normal_color           = gui_get_ncolor("FBHome");
  fbr.homedir.focused_color          = gui_get_fcolor("FBHome");
  fbr.homedir.clicked_color          = gui_get_ccolor("FBHome");

  fbr.select.x                       = gui_get_skinX("FBSelect");
  fbr.select.y                       = gui_get_skinY("FBSelect");
  fbr.select.caption                 = "Select";
  fbr.select.skin_filename           = gui_get_skinfile("FBSelect");
  fbr.select.normal_color            = gui_get_ncolor("FBSelect");
  fbr.select.focused_color           = gui_get_fcolor("FBSelect");
  fbr.select.clicked_color           = gui_get_ccolor("FBSelect");
  fbr.select.callback                = add_cb;

  fbr.dismiss.x                      = gui_get_skinX("FBDismiss");
  fbr.dismiss.y                      = gui_get_skinY("FBDismiss");
  fbr.dismiss.caption                = "Dismiss";
  fbr.dismiss.skin_filename          = gui_get_skinfile("FBDismiss");
  fbr.dismiss.normal_color           = gui_get_ncolor("FBDismiss");
  fbr.dismiss.focused_color          = gui_get_fcolor("FBDismiss");
  fbr.dismiss.clicked_color          = gui_get_ccolor("FBDismiss");

  fbr.kill.callback                  = file_browser_kill;

  /* The browser */  
  fbr.browser.display                = gGui->display;
  fbr.browser.imlibdata              = gGui->imlib_data;

  fbr.browser.arrow_up.x             = gui_get_skinX("FBUp");
  fbr.browser.arrow_up.y             = gui_get_skinY("FBUp");
  fbr.browser.arrow_up.skin_filename = gui_get_skinfile("FBUp");

  fbr.browser.slider.x               = gui_get_skinX("FBSlidBG");
  fbr.browser.slider.y               = gui_get_skinY("FBSlidBG");
  fbr.browser.slider.skin_filename   = gui_get_skinfile("FBSlidBG");

  fbr.browser.paddle.skin_filename   = gui_get_skinfile("FBSlidFG");

  fbr.browser.arrow_dn.x             = gui_get_skinX("FBDn");
  fbr.browser.arrow_dn.y             = gui_get_skinY("FBDn");
  fbr.browser.arrow_dn.skin_filename = gui_get_skinfile("FBDn");

  fbr.browser.browser.x              = gui_get_skinX("FBItemBtn");
  fbr.browser.browser.y              = gui_get_skinY("FBItemBtn");
  fbr.browser.browser.normal_color   = gui_get_ncolor("FBItemBtn");
  fbr.browser.browser.focused_color  = gui_get_fcolor("FBItemBtn");
  fbr.browser.browser.clicked_color  = gui_get_ccolor("FBItemBtn");
  fbr.browser.browser.skin_filename  = gui_get_skinfile("FBItemBtn");
  fbr.browser.browser.max_displayed_entries = MAX_LIST;
  fbr.browser.browser.num_entries    = 0;
  fbr.browser.browser.entries        = NULL;

  fbr.browser.callback               = sel_cb;
  fbr.browser.userdata               = NULL;

  fb = filebrowser_create(&fbr);

}

#endif
