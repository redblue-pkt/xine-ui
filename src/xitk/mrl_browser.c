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

#include <xine/xineutils.h>

#include "xitk.h"

#include "event.h"
#include "actions.h"
#include "videowin.h"
#include "mrl_browser.h"
#include "errors.h"

#define MAX_LIST 9

extern gGui_t            *gGui;
static xitk_widget_t     *mrlb = NULL;

/*
 *
 */
void mrl_browser_show_tips(int enabled, unsigned long timeout) {
  
  if(mrlb)
    xitk_mrlbrowser_set_tips_timeout(mrlb, enabled, timeout);

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
void mrl_browser_change_skins(void) {

  if(mrlb) {
    xitk_mrlbrowser_change_skins(mrlb, gGui->skin_config);
    set_mrl_browser_transient();
  }
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
void mrl_browser_toggle_visibility(xitk_widget_t *w, void *data) {
  
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
      gGui->config->update_num (gGui->config, "gui.mrl_browser_x", wi.x);
      gGui->config->update_num (gGui->config, "gui.mrl_browser_y", wi.y);
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
      gGui->config->update_num (gGui->config, "gui.mrl_browser_x", wi.x);
      gGui->config->update_num (gGui->config, "gui.mrl_browser_y", wi.y);
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

  XITK_WIDGET_INIT(&mb, gGui->imlib_data);

  mb.window_trans                   = gGui->video_window;
  mb.layer_above                    = gGui->layer_above;

  mb.x                              = gGui->config->register_num (gGui->config, "gui.mrl_browser_x", 200,
								  "gui mrl browser x coordinate",
								  NULL, NULL, NULL);
  mb.y                              = gGui->config->register_num (gGui->config, "gui.mrl_browser_y", 100,
								  "gui mrl browser y coordinate",
								  NULL, NULL, NULL);
  mb.window_title                   = _("Xine MRL Browser");
  mb.skin_element_name              = "MrlBG";
  mb.resource_name                  = mb.window_title;
  mb.resource_class                 = "Xine";
  
  mb.origin.skin_element_name       = "MrlCurOrigin";
  mb.origin.cur_origin              = NULL;

  mb.dndcallback                    = dnd_cb;

  mb.select.skin_element_name       = "MrlSelect";
  mb.select.caption                 = _("Select");
  mb.select.callback                = add_cb;

  mb.play.skin_element_name         = "MrlPlay";
  mb.play.callback                  = add_and_play_cb;

  mb.dismiss.skin_element_name      = "MrlDismiss";
  mb.dismiss.caption                = _("Dismiss");

  mb.kill.callback                  = mrl_browser_kill;

  mb.ip_availables                  = ip_availables;

  mb.ip_name.button.skin_element_name = "MrlPlugNameBG";

  mb.ip_name.label.skin_element_name = "MrlPlugLabel";
  mb.ip_name.label.label_str        = _("Source:");

  mb.xine                           = gGui->xine;

  /* The browser */

  mb.browser.arrow_up.skin_element_name = "MrlUp";

  mb.browser.slider.skin_element_name = "SLiderMrl";

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
}

/*
 * Callback called by mrlbrowser on add event.
 */
static void mrl_add(xitk_widget_t *w, void *data, mrl_t *mrl) {

  if(mrl) {
    
    if(!pl_is_running()) {
      playlist_editor();
    }
    else {
      if(!pl_is_visible())
	pl_toggle_visibility(NULL, NULL);
    }
      
    gui_dndcallback((char *)mrl->mrl);
  }
}

/*
 * Callback called by mrlbrowser on play event.
 */
static void mrl_add_and_play(xitk_widget_t *w, void *data, mrl_t *mrl) {

  if(mrl) {
    if((xine_get_status(gGui->xine) != XINE_STOP)) {
      gGui->ignore_status = 1;
      xine_stop (gGui->xine);
      gGui->ignore_status = 0;
    }

    gui_set_current_mrl(mrl->mrl);
    if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
      gui_handle_xine_error();

  }
}

/*
 * Create a new mrl browser.
 */
void open_mrlbrowser(xitk_widget_t *w, void *data) {
  
  mrl_browser(mrl_add, mrl_add_and_play, mrl_handle_selection, gui_dndcallback);
}
