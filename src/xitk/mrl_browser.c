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
 * MRL Browser
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "xitk.h"

#include "event.h"
#include "utils.h"
#include "parseskin.h"
#include "mrl_browser.h"

#define MAX_LIST 9

extern gGui_t       *gGui;
static widget_t     *mrlb = NULL;

/*
 *
 */
int mrl_browser_is_visible(void) {

  if(mrlb)
    return(mrlbrowser_is_visible(mrlb));
  
  return 0;
}

/*
 *
 */
int mrl_browser_is_running(void) {

  if(mrlb)
    return(mrlbrowser_is_running(mrlb));
  
  return 0;
}

/*
 *
 */
void set_mrl_browser_transient(void) {

  if(mrlb) {
    mrlbrowser_set_transient(mrlb, gGui->video_window);
  }
}

/*
 *
 */
void show_mrl_browser(void) {

  if(mrlb) {
    mrlbrowser_show(mrlb);
    set_mrl_browser_transient();
  }
}

/*
 *
 */
void hide_mrl_browser(void) {

  if(mrlb) {
    mrlbrowser_hide(mrlb);
  }
}

/*
 *
 */
void mrl_browser_toggle_visibility(void) {

  if(mrlb) {

    if(mrl_browser_is_visible())
      hide_mrl_browser();
    else
      show_mrl_browser();

  }
}
/*
 *
 */
void destroy_mrl_browser(void) {
  window_info_t wi;

  if(mrlb) {
    if((mrlbrowser_get_window_info(mrlb, &wi))) {
      config_set_int("x_mrl_browser", wi.x);
      config_set_int("y_mrl_browser", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    mrlbrowser_destroy(mrlb);
    mrlb = NULL;
  }
}

/*
 *
 */
static void mrl_browser_kill(widget_t *w, void *data) {
  window_info_t wi;

  if(mrlb) {
    if((mrlbrowser_get_window_info(mrlb, &wi))) {
      config_set_int("x_mrl_browser", wi.x);
      config_set_int("y_mrl_browser", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
  }

  /* FIXME */
  mrlb = NULL;
}

/*
 *
 */
void mrl_browser(xitk_mrl_callback_t add_cb, 
		 select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb) {
  xitk_mrlbrowser_t   mb;
  char              **ip_availables = 
    xine_get_browsable_input_plugin_ids(gGui->xine);

  if(mrlb != NULL) {
    show_mrl_browser();
    set_mrl_browser_transient();
    return;
  }

  mb.display                        = gGui->display;
  mb.imlibdata                      = gGui->imlib_data;
  mb.window_trans                   = gGui->video_window;

  mb.x                              = config_lookup_int("x_mrl_browser", 200);
  mb.y                              = config_lookup_int("y_mrl_browser", 100);
  mb.window_title                   = "Xine MRL Browser";
  mb.background_skin                = gui_get_skinfile("MrlBG");
  mb.resource_name                  = mb.window_title;
  mb.resource_class                 = "Xine";
  
  mb.origin.x                       = gui_get_skinX("MrlCurOrigin");
  mb.origin.y                       = gui_get_skinY("MrlCurOrigin");
  mb.origin.skin_filename           = gui_get_skinfile("MrlCurOrigin");
  mb.origin.max_length              = 50;
  mb.origin.cur_origin              = NULL;

  mb.dndcallback                    = dnd_cb;

  mb.select.x                       = gui_get_skinX("MrlSelect");
  mb.select.y                       = gui_get_skinY("MrlSelect");
  mb.select.caption                 = "Select";
  mb.select.skin_filename           = gui_get_skinfile("MrlSelect");
  mb.select.normal_color            = gui_get_ncolor("MrlSelect");
  mb.select.focused_color           = gui_get_fcolor("MrlSelect");
  mb.select.clicked_color           = gui_get_ccolor("MrlSelect");
  mb.select.callback                = add_cb;

  mb.dismiss.x                      = gui_get_skinX("MrlDismiss");
  mb.dismiss.y                      = gui_get_skinY("MrlDismiss");
  mb.dismiss.caption                = "Dismiss";
  mb.dismiss.skin_filename          = gui_get_skinfile("MrlDismiss");
  mb.dismiss.normal_color           = gui_get_ncolor("MrlDismiss");
  mb.dismiss.focused_color          = gui_get_fcolor("MrlDismiss");
  mb.dismiss.clicked_color          = gui_get_ccolor("MrlDismiss");

  mb.kill.callback                  = mrl_browser_kill;

  mb.ip_availables                  = ip_availables;

  mb.ip_name.button.x               = gui_get_skinX("MrlPlugNameBG");
  mb.ip_name.button.y               = gui_get_skinY("MrlPlugNameBG");
  mb.ip_name.button.skin_filename   = gui_get_skinfile("MrlPlugNameBG");
  mb.ip_name.button.normal_color    = gui_get_ncolor("MrlPlugNameBG");
  mb.ip_name.button.focused_color   = gui_get_fcolor("MrlPlugNameBG");
  mb.ip_name.button.clicked_color   = gui_get_ccolor("MrlPlugNameBG");

  mb.ip_name.label.x                = gui_get_skinX("MrlPlugLabel");
  mb.ip_name.label.y                = gui_get_skinY("MrlPlugLabel");
  mb.ip_name.label.skin_filename    = gui_get_skinfile("MrlPlugLabel");
  mb.ip_name.label.label_str        = "Source:";

  mb.xine                           = gGui->xine;

  /* The browser */
  mb.browser.display                = gGui->display;
  mb.browser.imlibdata              = gGui->imlib_data;

  mb.browser.arrow_up.x             = gui_get_skinX("MrlUp");
  mb.browser.arrow_up.y             = gui_get_skinY("MrlUp");
  mb.browser.arrow_up.skin_filename = gui_get_skinfile("MrlUp");

  mb.browser.slider.x               = gui_get_skinX("MrlSlidBG");
  mb.browser.slider.y               = gui_get_skinY("MrlSlidBG");
  mb.browser.slider.skin_filename   = gui_get_skinfile("MrlSlidBG");

  mb.browser.paddle.skin_filename   = gui_get_skinfile("MrlSlidFG");

  mb.browser.arrow_dn.x             = gui_get_skinX("MrlDn");
  mb.browser.arrow_dn.y             = gui_get_skinY("MrlDn");
  mb.browser.arrow_dn.skin_filename = gui_get_skinfile("MrlDn");

  mb.browser.browser.x              = gui_get_skinX("MrlItemBtn");
  mb.browser.browser.y              = gui_get_skinY("MrlItemBtn");
  mb.browser.browser.normal_color   = gui_get_ncolor("MrlItemBtn");
  mb.browser.browser.focused_color  = gui_get_fcolor("MrlItemBtn");
  mb.browser.browser.clicked_color  = gui_get_ccolor("MrlItemBtn");
  mb.browser.browser.skin_filename  = gui_get_skinfile("MrlItemBtn");
  mb.browser.browser.max_displayed_entries = MAX_LIST;
  mb.browser.browser.num_entries    = 0;
  mb.browser.browser.entries        = NULL;

  mb.browser.callback               = sel_cb;
  mb.browser.userdata               = NULL;

  mrlb = mrlbrowser_create(&mb);

  if(ip_availables)
    free(ip_availables);

}

/*
 *
 */
static void mrl_handle_selection(widget_t *w, void *data) {
  //  perr(" +++ Selection called = %d = '%s'\n", 
}

/*
 * Callback called by mrlbrowser on add event.
 */
static void mrl_add(widget_t *w, void *data, mrl_t *mrl) {

  if(mrl)
    gui_dndcallback((char *)mrl->mrl);
}

/*
 *
 */
void open_mrlbrowser(widget_t *w, void *data) {
  
  mrl_browser(mrl_add, mrl_handle_selection, gui_dndcallback);
}
