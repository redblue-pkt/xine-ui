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

  if(fb) {
    filebrowser_destroy(fb);
    fb = NULL;
  }
}

/*
 *
 */
static void file_browser_kill(widget_t *w, void *data) {
  char *curdir = filebrowser_get_current_dir(fb);

  if(curdir)
    config_set_str("filebrowser_dir", curdir);
  
  fb = NULL;
}

/*
 *
 */
void file_browser(add_cb_t add_cb, select_cb_t sel_cb, dnd_callback_t dnd_cb) {
  browser_placements_t     *bp;
  filebrowser_placements_t *fbp;

  if(fb != NULL) {
    show_file_browser();
    set_file_browser_transient();
    return;
  }

  bp  = (browser_placements_t *) xmalloc(sizeof(browser_placements_t));
  bp->arrow_up.x                    = gui_get_skinX("FBUp");
  bp->arrow_up.y                    = gui_get_skinY("FBUp");
  bp->arrow_up.skinfile             = gui_get_skinfile("FBUp");
  bp->slider.x                      = gui_get_skinX("FBSlidBG");
  bp->slider.y                      = gui_get_skinY("FBSlidBG");
  bp->slider.skinfile               = gui_get_skinfile("FBSlidBG");
  bp->paddle.skinfile               = gui_get_skinfile("FBSlidFG");
  bp->arrow_dn.x                    = gui_get_skinX("FBDn");
  bp->arrow_dn.y                    = gui_get_skinY("FBDn");
  bp->arrow_dn.skinfile             = gui_get_skinfile("FBDn");
  bp->browser.x                     = gui_get_skinX("FBItemBtn");
  bp->browser.y                     = gui_get_skinY("FBItemBtn");
  bp->browser.norm_color            = gui_get_ncolor("FBItemBtn");
  bp->browser.focused_color         = gui_get_fcolor("FBItemBtn");
  bp->browser.clicked_color         = gui_get_ccolor("FBItemBtn");
  bp->browser.skinfile              = gui_get_skinfile("FBItemBtn");
  bp->browser.max_displayed_entries = MAX_LIST;
  bp->browser.num_entries           = 0;
  bp->browser.entries               = NULL;
  bp->user_data                     = NULL;

  fbp                               = (filebrowser_placements_t *) 
    xmalloc(sizeof(filebrowser_placements_t));
  fbp->x                            = config_lookup_int("x_file_browser", 200);
  fbp->y                            = config_lookup_int("y_file_browser", 100);
  fbp->window_title                 = "Xine File Browser";
  fbp->bg_skinfile                  = gui_get_skinfile("FBBG");
  fbp->resource_name                = fbp->window_title;
  fbp->resource_class               = "Xine";
  fbp->sort_default.x               = gui_get_skinX("FBSortDef");
  fbp->sort_default.y               = gui_get_skinY("FBSortDef");
  fbp->sort_default.skin_filename   = gui_get_skinfile("FBSortDef");
  fbp->sort_reverse.x               = gui_get_skinX("FBSortRev");
  fbp->sort_reverse.y               = gui_get_skinY("FBSortRev");
  fbp->sort_reverse.skin_filename   = gui_get_skinfile("FBSortRev");
  fbp->current_dir.x                = gui_get_skinX("FBCurDir");
  fbp->current_dir.y                = gui_get_skinY("FBCurDir");
  fbp->current_dir.skin_filename    = gui_get_skinfile("FBCurDir");
  fbp->current_dir.max_length       = 50;
  fbp->current_dir.cur_directory    = config_lookup_str("filebrowser_dir",
							(char *)get_homedir());
  fbp->homedir.x                    = gui_get_skinX("FBHome");
  fbp->homedir.y                    = gui_get_skinY("FBHome");
  fbp->homedir.caption              = "~/";
  fbp->homedir.skin_filename        = gui_get_skinfile("FBHome");
  fbp->homedir.normal_color         = gui_get_ncolor("FBHome");
  fbp->homedir.focused_color        = gui_get_fcolor("FBHome");
  fbp->homedir.clicked_color        = gui_get_ccolor("FBHome");

  fbp->select.x                     = gui_get_skinX("FBSelect");
  fbp->select.y                     = gui_get_skinY("FBSelect");
  fbp->select.caption               = "Select";
  fbp->select.skin_filename         = gui_get_skinfile("FBSelect");
  fbp->select.normal_color          = gui_get_ncolor("FBSelect");
  fbp->select.focused_color         = gui_get_fcolor("FBSelect");
  fbp->select.clicked_color         = gui_get_ccolor("FBSelect");

  fbp->dismiss.x                    = gui_get_skinX("FBDismiss");
  fbp->dismiss.y                    = gui_get_skinY("FBDismiss");
  fbp->dismiss.caption              = "Dismiss";
  fbp->dismiss.skin_filename        = gui_get_skinfile("FBDismiss");
  fbp->dismiss.normal_color         = gui_get_ncolor("FBDismiss");
  fbp->dismiss.focused_color        = gui_get_fcolor("FBDismiss");
  fbp->dismiss.clicked_color        = gui_get_ccolor("FBDismiss");

  bp->callback                      = sel_cb;
  fbp->dndcallback                  = dnd_cb;
  fbp->select.callback              = add_cb;
  fbp->kill.callback                = file_browser_kill;

  fbp->br_placement                 = bp;


  fb = filebrowser_create(gGui->display, gGui->imlib_data, 
			  gGui->video_window, fbp);

  free(bp);
  free(fbp);
}

#endif
