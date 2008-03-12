/* 
 * Copyright (C) 2000-2004 the xine project
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

#include "common.h"

extern gGui_t            *gGui;
static xitk_widget_t     *mrlb = NULL;

/*
 *
 */
void mrl_browser_show_tips(int enabled, unsigned long timeout) {
  
  if(mrlb)
    xitk_mrlbrowser_set_tips_timeout(mrlb, enabled, timeout);

}

void mrl_browser_update_tips_timeout(unsigned long timeout) {
  if(mrlb) {
    if(xitk_get_widget_tips_timeout(mrlb) > 0)
      xitk_mrlbrowser_set_tips_timeout(mrlb, 1, timeout);

  }
}

/*
 *
 */
void set_mrl_browser_transient(void) {

  if(mrlb) {
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      xitk_mrlbrowser_set_transient(mrlb, gGui->video_window);
  }
}

/*
 *
 */
void mrl_browser_change_skins(int synthetic) {

  if(mrlb) {
    xitk_mrlbrowser_change_skins(mrlb, gGui->skin_config);
    set_mrl_browser_transient();
    if(mrl_browser_is_visible())
      raise_window((xitk_mrlbrowser_get_window_id(mrlb)), 1, 1);
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
    gGui->nongui_error_msg = NULL;
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
      config_update_num (gGui->xine, "gui.mrl_browser_x", wi.x);
      config_update_num (gGui->xine, "gui.mrl_browser_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    xitk_mrlbrowser_destroy(mrlb);
    mrlb = NULL;

    try_to_set_input_focus(gGui->video_window);
  }
}

/*
 *
 */
static void mrl_browser_kill(xitk_widget_t *w, void *data) {
  window_info_t wi;

  if(mrlb) {
    if((xitk_mrlbrowser_get_window_info(mrlb, &wi))) {
      config_update_num (gGui->xine, "gui.mrl_browser_x", wi.x);
      config_update_num (gGui->xine, "gui.mrl_browser_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    try_to_set_input_focus(gGui->video_window);
  }

  /* FIXME */
  mrlb = NULL;
}

static xitk_mrlbrowser_filter_t **mrl_browser_get_valid_mrl_ending(void) {
  xitk_mrlbrowser_filter_t **filters = NULL;
  int                        num_endings = 0;
  char                      *mrl_exts, *p, *pp;
  
  filters                      = (xitk_mrlbrowser_filter_t **) 
    calloc((num_endings + 2), sizeof(xitk_mrlbrowser_filter_t *));
  filters[num_endings]         = (xitk_mrlbrowser_filter_t *)
    xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
  filters[num_endings]->name   = strdup("All");
  filters[num_endings]->ending = strdup("*");

  mrl_exts = xine_get_file_extensions(gGui->xine);
  if(mrl_exts) {
    char  patterns[2048];
    char *e;
    
    p = strdup(mrl_exts);

    num_endings++;
    
    pp = p;
    while(*p != '\0') {
      if(*p == ' ')
	*p = ',';
      p++;
    }
    
    filters[num_endings]         = (xitk_mrlbrowser_filter_t *) xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
    filters[num_endings]->name   = strdup(_("All extensions"));
    filters[num_endings]->ending = pp;

    while((e = xine_strsep(&mrl_exts, " ")) != NULL) {
      
      snprintf(patterns, sizeof(patterns), "*.%s", e);

      num_endings++;
      
      filters                      = (xitk_mrlbrowser_filter_t **) 
	realloc(filters, sizeof(xitk_mrlbrowser_filter_t *) * (num_endings + 2));
      
      filters[num_endings]         = (xitk_mrlbrowser_filter_t *)
	xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));

      filters[num_endings]->name   = strdup(patterns);
      filters[num_endings]->ending = strdup(e);
    }
    
    free(mrl_exts);
  }

  filters[num_endings + 1]         = (xitk_mrlbrowser_filter_t *)
    xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
  filters[num_endings + 1]->name   = NULL;
  filters[num_endings + 1]->ending = NULL;
  
  return filters;
}


/*
 *
 */
void mrl_browser(xitk_mrl_callback_t add_cb, xitk_mrl_callback_t play_cb,
		 select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb) {
  xitk_mrlbrowser_widget_t     mb;
  const char *const           *ip_availables = xine_get_browsable_input_plugin_ids(gGui->xine);
  xitk_mrlbrowser_filter_t   **mrl_filters = mrl_browser_get_valid_mrl_ending();

  if(mrlb != NULL) {
    show_mrl_browser();
    set_mrl_browser_transient();
    return;
  }

  XITK_WIDGET_INIT(&mb, gGui->imlib_data);

  mb.window_trans                   = (gGui->use_root_window || gGui->video_display != gGui->display) ? None : gGui->video_window;
  mb.layer_above                    = (is_layer_above());
  mb.icon                           = &gGui->icon;
  mb.set_wm_window_normal           = !video_window_is_visible();

  mb.x                              = xine_config_register_num (gGui->xine, "gui.mrl_browser_x", 
								200,
								"gui mrl browser x coordinate",
								CONFIG_NO_HELP,
								CONFIG_LEVEL_DEB,
								CONFIG_NO_CB,
								CONFIG_NO_DATA);
  mb.y                              = xine_config_register_num (gGui->xine, "gui.mrl_browser_y",
								100,
								"gui mrl browser y coordinate",
								CONFIG_NO_HELP,
								CONFIG_LEVEL_DEB,
								CONFIG_NO_CB,
								CONFIG_NO_DATA);

  mb.window_title                          = _("xine MRL Browser");
  mb.skin_element_name                     = "MrlBG";
  mb.resource_name                         = mb.window_title;
  mb.resource_class                        = "xine";
  
  mb.origin.skin_element_name              = "MrlCurOrigin";
  mb.origin.cur_origin                     = NULL;

  mb.dndcallback                           = dnd_cb;

  mb.select.skin_element_name              = "MrlSelect";
  mb.select.caption                        = _("Add");
  mb.select.callback                       = add_cb;

  mb.play.skin_element_name                = "MrlPlay";
  mb.play.callback                         = play_cb;

  mb.dismiss.skin_element_name             = "MrlDismiss";
  mb.dismiss.caption                       = _("Dismiss");

  mb.kill.callback                         = mrl_browser_kill;

  mb.ip_availables                         = ip_availables;

  mb.ip_name.button.skin_element_name      = "MrlPlugNameBG";

  mb.ip_name.label.skin_element_name       = "MrlPlugLabel";
  mb.ip_name.label.label_str               = _("Source:");

  mb.xine                                  = (xine_t *)gGui->xine;

  /* The browser */

  mb.browser.arrow_up.skin_element_name    = "MrlUp";
  mb.browser.slider.skin_element_name      = "SliderMrl";
  mb.browser.arrow_dn.skin_element_name    = "MrlDn";

  mb.browser.arrow_left.skin_element_name  = "MrlLeft";
  mb.browser.slider_h.skin_element_name    = "SliderHMrl";
  mb.browser.arrow_right.skin_element_name = "MrlRight";

  mb.browser.browser.skin_element_name     = "MrlItemBtn";
  mb.browser.browser.num_entries           = 0;
  mb.browser.browser.entries               = NULL;
  mb.browser.callback                      = sel_cb;
  mb.browser.userdata                      = NULL;

  mb.combo.skin_element_name               = "MrlFilt";

  mb.mrl_filters                           = mrl_filters;

  mrlb = xitk_mrlbrowser_create(NULL, gGui->skin_config, &mb);

  if(mrl_filters) {
    int i;

    for(i = 0; mrl_filters[i] && (mrl_filters[i]->name && mrl_filters[i]->ending); i++) {
      free(mrl_filters[i]->name);
      free(mrl_filters[i]->ending);
      free(mrl_filters[i]);
    }
    free(mrl_filters[i]);
    free(mrl_filters);
  }
}

/*
 *
 */
static void mrl_handle_selection(xitk_widget_t *w, void *data, int selected) {
}

/*
 * Callback called by mrlbrowser on add event.
 */
static void mrl_add_noautoplay(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {
  if(mrl) {
    int num = gGui->playlist.num;
    
    if(!playlist_is_running()) {
      playlist_editor();
    }
    else {
      if(!playlist_is_visible())
	playlist_toggle_visibility(NULL, NULL);
    }
    
    mediamark_append_entry((char *)mrl->mrl, (char *)mrl->mrl, NULL, 0, -1, 0, 0);
    
    if((!num) && ((xine_get_status(gGui->stream) == XINE_STATUS_STOP) || gGui->logo_mode)) {
      gGui->playlist.cur = (gGui->playlist.num - 1);
      gui_set_current_mmk(mediamark_get_current_mmk());
    }   
    
    playlist_update_playlist();
    
    if((!is_playback_widgets_enabled()) && gGui->playlist.num)
      enable_playback_controls(1);
  }
}
static void mrl_add(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {

  if(mrl) {
    
    if(!playlist_is_running()) {
      playlist_editor();
    }
    else {
      if(!playlist_is_visible())
	playlist_toggle_visibility(NULL, NULL);
    }
    
    gui_dndcallback((char *)mrl->mrl);
  }
}

/*
 * Callback called by mrlbrowser on play event.
 */
static void mrl_play(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {

  if(mrl) {
    mediamark_t mmk;
    char        *_mrl = mrl->mrl;
    
    if((xine_get_status(gGui->stream) != XINE_STATUS_STOP)) {
      gGui->ignore_next = 1;
      xine_stop(gGui->stream);
      gGui->ignore_next = 0;
    }
    
    if(!is_playback_widgets_enabled())
      enable_playback_controls(1);

    if(mrl_look_like_playlist(_mrl)) {
      if(mediamark_concat_mediamarks(_mrl)) {
	gui_set_current_mmk(mediamark_get_current_mmk());
	_mrl = (char *) mediamark_get_current_mrl();
	playlist_update_playlist();
      }
    }
    
    osd_hide();

    if(!xine_open(gGui->stream, (const char *) _mrl)) {
      gui_handle_xine_error(gGui->stream, _mrl);
      enable_playback_controls(0);
      gui_display_logo();
      return;
    }

    if(!gui_xine_play(gGui->stream, 0, 0, 0)) {
      enable_playback_controls(0);
      gui_display_logo();
      return;
    }
    
    mmk.mrl           = _mrl;
    mmk.ident         = NULL;
    mmk.sub           = NULL;
    mmk.start         = 0;
    mmk.end           = -1;
    mmk.av_offset     = 0;
    mmk.spu_offset    = 0;
    mmk.got_alternate = 0;
    mmk.alternates    = NULL;
    mmk.cur_alt       = NULL;
    gui_set_current_mmk(&mmk);
  }
}

void mrl_browser_deinit(void) {
  if(mrlb) {
    if(mrl_browser_is_running())
      destroy_mrl_browser();
  }
}

void mrl_browser_reparent(void) {
  if(mrlb)
    reparent_window((xitk_mrlbrowser_get_window_id(mrlb)));
}

/*
 * Create a new mrl browser.
 */
void open_mrlbrowser(xitk_widget_t *w, void *data) {
  
  mrl_browser(mrl_add, mrl_play, mrl_handle_selection, gui_dndcallback);
  set_mrl_browser_transient();
  mrl_browser_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());
}

void open_mrlbrowser_from_playlist(xitk_widget_t *w, void *data) {
  
  mrl_browser(mrl_add_noautoplay, mrl_play, mrl_handle_selection, gui_dndcallback);
  set_mrl_browser_transient();
  mrl_browser_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());
}
