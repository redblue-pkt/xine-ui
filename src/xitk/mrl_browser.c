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
#include "actions.h"
#include "utils.h"
#include "mrl_browser.h"

#define MAX_LIST 9

extern gGui_t       *gGui;
static xitk_widget_t     *mrlb = NULL;

/*
 *
 */
void mrl_browser_change_skins(void) {

  if(mrlb)
    xitk_mrlbrowser_change_skins(mrlb, gGui->skin_config);
}

/*
 *
 */
int mrl_browser_is_visible(void) {

  if(mrlb)
    return(xitk_mrlbrowser_is_visible(mrlb));
  
  return 0;
}

/*
 *
 */
int mrl_browser_is_running(void) {

  if(mrlb)
    return(xitk_mrlbrowser_is_running(mrlb));
  
  return 0;
}

/*
 *
 */
void set_mrl_browser_transient(void) {

  if(mrlb) {
    xitk_mrlbrowser_set_transient(mrlb, gGui->video_window);
  }
}

/*
 *
 */
void show_mrl_browser(void) {

  if(mrlb) {
    xitk_mrlbrowser_show(mrlb);
    set_mrl_browser_transient();
    layer_above_video((xitk_mrlbrowser_get_window_id(mrlb)));
  }
}

/*
 *
 */
void hide_mrl_browser(void) {

  if(mrlb) {
    xitk_mrlbrowser_hide(mrlb);
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
    if((xitk_mrlbrowser_get_window_info(mrlb, &wi))) {
      config_set_int("x_mrl_browser", wi.x);
      config_set_int("y_mrl_browser", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    xitk_mrlbrowser_destroy(mrlb);
    mrlb = NULL;
  }
}

/*
 *
 */
static void mrl_browser_kill(xitk_widget_t *w, void *data) {
  window_info_t wi;

  if(mrlb) {
    if((xitk_mrlbrowser_get_window_info(mrlb, &wi))) {
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
void mrl_browser(xitk_mrl_callback_t add_cb, xitk_mrl_callback_t add_and_play_cb,
		 select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb) {
  xitk_mrlbrowser_widget_t   mb;
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
  mb.layer_above                    = gGui->layer_above;

  mb.x                              = config_lookup_int("x_mrl_browser", 200);
  mb.y                              = config_lookup_int("y_mrl_browser", 100);
  mb.window_title                   = "Xine MRL Browser";
  mb.skin_element_name              = "MrlBG";
  mb.resource_name                  = mb.window_title;
  mb.resource_class                 = "Xine";
  
  mb.origin.skin_element_name       = "MrlCurOrigin";
  mb.origin.cur_origin              = NULL;

  mb.dndcallback                    = dnd_cb;

  mb.select.skin_element_name       = "MrlSelect";
  mb.select.caption                 = "Select";
  mb.select.callback                = add_cb;

  mb.play.skin_element_name         = "MrlPlay";
  mb.play.callback                  = add_and_play_cb;

  mb.dismiss.skin_element_name      = "MrlDismiss";
  mb.dismiss.caption                = "Dismiss";

  mb.kill.callback                  = mrl_browser_kill;

  mb.ip_availables                  = ip_availables;

  mb.ip_name.button.skin_element_name = "MrlPlugNameBG";

  mb.ip_name.label.skin_element_name = "MrlPlugLabel";
  mb.ip_name.label.label_str        = "Source:";

  mb.xine                           = gGui->xine;

  /* The browser */
  mb.browser.display                = gGui->display;
  mb.browser.imlibdata              = gGui->imlib_data;

  mb.browser.arrow_up.skin_element_name = "MrlUp";

  mb.browser.slider.skin_element_name_bg = "MrlSlidBG";
  mb.browser.slider.skin_element_name_paddle = "MrlSlidFG";

  mb.browser.arrow_dn.skin_element_name = "MrlDn";

  mb.browser.browser.skin_element_name = "MrlItemBtn";
  mb.browser.browser.max_displayed_entries = MAX_LIST;
  mb.browser.browser.num_entries    = 0;
  mb.browser.browser.entries        = NULL;

  mb.browser.callback               = sel_cb;
  mb.browser.userdata               = NULL;

  mrlb = xitk_mrlbrowser_create(gGui->skin_config, &mb);

  if(ip_availables)
    free(ip_availables);

}

/*
 *
 */
static void mrl_handle_selection(xitk_widget_t *w, void *data) {
  //  perr(" +++ Selection called = %d = '%s'\n", 
}

/*
 * Callback called by mrlbrowser on add event.
 */
static void mrl_add(xitk_widget_t *w, void *data, mrl_t *mrl) {

  if(mrl)
    gui_dndcallback((char *)mrl->mrl);
}

/*
 * Callback called by mrlbrowser on play event.
 */
static void mrl_add_and_play(xitk_widget_t *w, void *data, mrl_t *mrl) {

  if(mrl) {
    gui_dndcallback((char *)mrl->mrl);

    if((xine_get_status(gGui->xine) != XINE_STOP)) {
      gGui->ignore_status = 1;
      xine_stop (gGui->xine);
      gGui->ignore_status = 0;
    }

    gui_set_current_mrl(gGui->playlist[gGui->playlist_num - 1]);
    xine_play (gGui->xine, gGui->filename, 0, 0 );
  }
}

/*
 * Create a new mrl browser.
 */
void open_mrlbrowser(xitk_widget_t *w, void *data) {
  
  mrl_browser(mrl_add, mrl_add_and_play, mrl_handle_selection, gui_dndcallback);
}
