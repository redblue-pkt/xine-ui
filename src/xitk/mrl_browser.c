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
void mrl_browser(mrl_add_cb_t add_cb, select_cb_t sel_cb, dnd_callback_t dnd_cb) {
  browser_placements_t     *bp;
  mrlbrowser_placements_t *mbp;
  char **ip_availables = xine_get_browsable_input_plugin_ids(gGui->xine);

  if(mrlb != NULL) {
    show_mrl_browser();
    set_mrl_browser_transient();
    return;
  }

  bp = (browser_placements_t *) xmalloc(sizeof(browser_placements_t));
  bp->arrow_up.x                    = gui_get_skinX("MrlUp");
  bp->arrow_up.y                    = gui_get_skinY("MrlUp");
  bp->arrow_up.skinfile             = gui_get_skinfile("MrlUp");
  bp->slider.x                      = gui_get_skinX("MrlSlidBG");
  bp->slider.y                      = gui_get_skinY("MrlSlidBG");
  bp->slider.skinfile               = gui_get_skinfile("MrlSlidBG");
  bp->paddle.skinfile               = gui_get_skinfile("MrlSlidFG");
  bp->arrow_dn.x                    = gui_get_skinX("MrlDn");
  bp->arrow_dn.y                    = gui_get_skinY("MrlDn");
  bp->arrow_dn.skinfile             = gui_get_skinfile("MrlDn");
  bp->browser.x                     = gui_get_skinX("MrlItemBtn");
  bp->browser.y                     = gui_get_skinY("MrlItemBtn");
  bp->browser.norm_color            = gui_get_ncolor("MrlItemBtn");
  bp->browser.focused_color         = gui_get_fcolor("MrlItemBtn");
  bp->browser.clicked_color         = gui_get_ccolor("MrlItemBtn");
  bp->browser.skinfile              = gui_get_skinfile("MrlItemBtn");
  bp->browser.max_displayed_entries = MAX_LIST;
  bp->browser.num_entries           = 0;
  bp->browser.entries               = NULL;
  bp->user_data                     = NULL;

  mbp                               = (mrlbrowser_placements_t *) 
    xmalloc(sizeof(filebrowser_placements_t));
  mbp->x                            = config_lookup_int("x_mrl_browser", 200);
  mbp->y                            = config_lookup_int("y_mrl_browser", 100);
  mbp->window_title                 = "Xine MRL Browser";
  mbp->bg_skinfile                  = gui_get_skinfile("MrlBG");
  mbp->resource_name                = mbp->window_title;
  mbp->resource_class               = "Xine";

  mbp->origin.x                     = gui_get_skinX("MrlCurOrigin");
  mbp->origin.y                     = gui_get_skinY("MrlCurOrigin");
  mbp->origin.skin_filename         = gui_get_skinfile("MrlCurOrigin");
  mbp->origin.max_length            = 50;
  mbp->origin.cur_origin            = NULL;

  mbp->select.x                     = gui_get_skinX("MrlSelect");
  mbp->select.y                     = gui_get_skinY("MrlSelect");
  mbp->select.caption               = "Select";
  mbp->select.skin_filename         = gui_get_skinfile("MrlSelect");
  mbp->select.normal_color          = gui_get_ncolor("MrlSelect");
  mbp->select.focused_color         = gui_get_fcolor("MrlSelect");
  mbp->select.clicked_color         = gui_get_ccolor("MrlSelect");

  mbp->dismiss.x                    = gui_get_skinX("MrlDismiss");
  mbp->dismiss.y                    = gui_get_skinY("MrlDismiss");
  mbp->dismiss.caption              = "Dismiss";
  mbp->dismiss.skin_filename        = gui_get_skinfile("MrlDismiss");
  mbp->dismiss.normal_color         = gui_get_ncolor("MrlDismiss");
  mbp->dismiss.focused_color        = gui_get_fcolor("MrlDismiss");
  mbp->dismiss.clicked_color        = gui_get_ccolor("MrlDismiss");

  mbp->ip_name.label.x              = gui_get_skinX("MrlPlugLabel");
  mbp->ip_name.label.y              = gui_get_skinY("MrlPlugLabel");
  mbp->ip_name.label.skin_filename  = gui_get_skinfile("MrlPlugLabel");
  mbp->ip_name.label.label_str      = "Source:";

  mbp->ip_name.button.x             = gui_get_skinX("MrlPlugNameBG");
  mbp->ip_name.button.y             = gui_get_skinY("MrlPlugNameBG");
  mbp->ip_name.button.skin_filename = gui_get_skinfile("MrlPlugNameBG");
  mbp->ip_name.button.normal_color  = gui_get_ncolor("MrlPlugNameBG");
  mbp->ip_name.button.focused_color = gui_get_fcolor("MrlPlugNameBG");
  mbp->ip_name.button.clicked_color = gui_get_ccolor("MrlPlugNameBG");
  
  bp->callback                      = sel_cb;
  mbp->dndcallback                  = dnd_cb;
  mbp->select.callback              = add_cb;
  mbp->kill.callback                = mrl_browser_kill;

  mbp->br_placement                 = bp;

  mbp->xine                         = gGui->xine;
  mbp->ip_availables                = ip_availables;
  
  mrlb = mrlbrowser_create(gGui->display, gGui->imlib_data, 
			   gGui->video_window, mbp);

  if(ip_availables)
    free(ip_availables);

  free(bp);
  free(mbp);
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
