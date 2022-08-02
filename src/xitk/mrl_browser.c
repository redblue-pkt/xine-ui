/*
 * Copyright (C) 2000-2022 the xine project
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
 * MRL Browser
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "mrl_browser.h"
#include "panel.h"
#include "playlist.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/mrlbrowser.h"

typedef void (*select_cb_t) (xitk_widget_t *w, void *mrlb, int, int);

struct xui_mrlb_st {
  gGui_t *gui;
  xitk_widget_t *w;
};

/*
 *
 */
void mrl_browser_change_skins (xui_mrlb_t *mrlb, int synthetic) {
  (void)synthetic;
  if (mrlb && mrlb->w) {
    xitk_window_t *xwin = xitk_mrlbrowser_get_window(mrlb->w);
    xitk_mrlbrowser_change_skins (mrlb->w, mrlb->gui->skin_config);
    video_window_set_transient_for(mrlb->gui->vwin, xwin);
    if (mrl_browser_is_visible (mrlb))
      raise_window (mrlb->gui, xwin, 1, 1);
  }
}

/*
 *
 */
int mrl_browser_is_visible (xui_mrlb_t *mrlb) {
  if (mrlb && mrlb->w)
    return (xitk_mrlbrowser_is_visible (mrlb->w));
  return 0;
}

/*
 *
 */
int mrl_browser_is_running (xui_mrlb_t *mrlb) {
  if (mrlb && mrlb->w)
    return (xitk_mrlbrowser_is_running (mrlb->w));
  return 0;
}

/*
 *
 */
void show_mrl_browser (xui_mrlb_t *mrlb) {
  if (mrlb && mrlb->w) {
    mrlb->gui->nongui_error_msg = NULL;
    xitk_mrlbrowser_show (mrlb->w);
    video_window_set_transient_for(mrlb->gui->vwin, xitk_mrlbrowser_get_window(mrlb->w));
    layer_above_video (mrlb->gui, xitk_mrlbrowser_get_window (mrlb->w));
  }
}

/*
 *
 */
void hide_mrl_browser (xui_mrlb_t *mrlb) {
  if (mrlb && mrlb->w)
    xitk_mrlbrowser_hide (mrlb->w);
}

/*
 *
 */
void mrl_browser_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_mrlb_t *mrlb = data;

  (void)w;
  if (mrlb) {
    if (mrl_browser_is_visible (mrlb))
      hide_mrl_browser (mrlb);
    else
      show_mrl_browser (mrlb);
  }
}

/*
 *
 */
void destroy_mrl_browser (xui_mrlb_t *mrlb) {
  if (mrlb) {
    if (mrlb->w) {
      window_info_t wi;

      if ((xitk_mrlbrowser_get_window_info (mrlb->w, &wi))) {
        config_update_num (mrlb->gui->xine, "gui.mrl_browser_x", wi.x);
        config_update_num (mrlb->gui->xine, "gui.mrl_browser_y", wi.y);
      }
      xitk_widgets_delete (&mrlb->w, 1);
    }
    mrlb->gui->mrlb = NULL;
    video_window_set_input_focus (mrlb->gui->vwin);
    free (mrlb);
  }
}

/*
 *
 */
static void mrl_browser_kill(xitk_widget_t *w, void *data) {
  xui_mrlb_t *mrlb = data;

  (void)w;
  /* xitk_mrlbrowser_exit () calls this, then xitk_mrlbrowser_destroy ().
     so we just shut down our own stuff here. */
  if (mrlb) {
    if (mrlb->w) {
      window_info_t wi;

      if ((xitk_mrlbrowser_get_window_info (mrlb->w, &wi))) {
        config_update_num (mrlb->gui->xine, "gui.mrl_browser_x", wi.x);
        config_update_num (mrlb->gui->xine, "gui.mrl_browser_y", wi.y);
      }
      mrlb->w = NULL;
    }
    mrlb->gui->mrlb = NULL;
    video_window_set_input_focus (mrlb->gui->vwin);
    free (mrlb);
  }
}

static xitk_mrlbrowser_filter_t **mrl_browser_get_valid_mrl_ending (xui_mrlb_t *mrlb) {
  xitk_mrlbrowser_filter_t **filters = NULL;
  int                        num_endings = 0;
  char                      *mrl_exts, *pmrl_exts, *p, *pp;

  filters                      = (xitk_mrlbrowser_filter_t **)
    calloc((num_endings + 2), sizeof(xitk_mrlbrowser_filter_t *));
  filters[num_endings]         = (xitk_mrlbrowser_filter_t *)
    xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
  filters[num_endings]->name   = strdup("All");
  filters[num_endings]->ending = strdup("*");

  mrl_exts = xine_get_file_extensions (mrlb->gui->xine);
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

    pmrl_exts = mrl_exts;
    while((e = xine_strsep(&pmrl_exts, " ")) != NULL) {

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


static void mrl_browser_rpwin (void *data, xitk_window_t *xwin) {
  gGui_t *gui = data;
  reparent_window (gui, xwin);
}

/*
 *
 */
static xui_mrlb_t *mrl_browser (gGui_t *gui,
  xitk_mrl_callback_t add_cb, xitk_mrl_callback_t play_cb, select_cb_t sel_cb, xitk_dnd_callback_t dnd_cb) {
  xui_mrlb_t *mrlb;
  xitk_mrlbrowser_widget_t     mb;
  const char *const           *ip_availables;
  xitk_mrlbrowser_filter_t   **mrl_filters;

  if (!gui)
    return NULL;

  if (gui->mrlb) {
    show_mrl_browser (gui->mrlb);
    video_window_set_transient_for(gui->vwin, xitk_mrlbrowser_get_window(gui->mrlb->w));
    return gui->mrlb;
  }

  mrlb = calloc (1, sizeof (*mrlb));
  if (!mrlb)
    return NULL;
  mrlb->gui = gui;

  ip_availables = xine_get_browsable_input_plugin_ids (mrlb->gui->xine);
  mrl_filters = mrl_browser_get_valid_mrl_ending (mrlb);

  XITK_WIDGET_INIT (&mb);

  mb.reparent_window = mrl_browser_rpwin;
  mb.rw_data = mrlb->gui;
  mb.layer_above  = is_layer_above (mrlb->gui);
  mb.icon         = mrlb->gui->icon;

  mb.x = 200;
  mb.y = 100;
  gui_load_window_pos (mrlb->gui, "mrl_browser", &mb.x, &mb.y);
  mb.window_title = _("xine MRL Browser");

  mb.skin_element_name        = "MrlBG";
  mb.resource_name            = mb.window_title;
  mb.resource_class           = "xine";

  mb.origin.skin_element_name = "MrlCurOrigin";
  mb.origin.cur_origin        = NULL;

  mb.dndcallback              = dnd_cb;
  mb.dnd_cb_data              = gui;
  mb.input_cb                 = gui_handle_be_event;
  mb.input_cb_data            = gui;

  mb.select.skin_element_name = "MrlSelect";
  mb.select.caption           = _("Add");
  mb.select.callback          = add_cb;
  mb.select.data              = mrlb;

  mb.play.skin_element_name   = "MrlPlay";
  mb.play.callback            = play_cb;
  mb.play.data                = mrlb;

  mb.dismiss.skin_element_name = "MrlDismiss";
  mb.dismiss.caption           = _("Dismiss");

  mb.kill.callback            = mrl_browser_kill;
  mb.kill.data                = mrlb;

  mb.ip_availables            = ip_availables;

  mb.ip_name.button.skin_element_name = "MrlPlugNameBG";

  mb.ip_name.label.skin_element_name  = "MrlPlugLabel";
  /* TRANSLATORS: only ASCII characters (skin) */
  mb.ip_name.label.label_str  = pgettext ("skin", "Source:");

  mb.xine = mrlb->gui->xine;

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
  mb.browser.userdata                      = mrlb;

  mb.combo.skin_element_name               = "MrlFilt";

  mb.mrl_filters                           = mrl_filters;

  mrlb->w = xitk_mrlbrowser_create (mrlb->gui->xitk, mrlb->gui->skin_config, &mb);

  video_window_set_transient_for(mrlb->gui->vwin, xitk_mrlbrowser_get_window(mrlb->w));

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

  mrlb->gui->mrlb = mrlb;
  return mrlb;
}

/*
 * Callback called by mrlbrowser on add event.
 */
static void mrl_add_noautoplay(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {
  xui_mrlb_t *mrlb = data;

  (void)w;
  if (mrlb && mrl) {
    int idx;

    gui_playlist_add_dir (mrlb->gui, mrl->mrl);
    idx = mrlb->gui->playlist.num - 1;

    if (!mrlb->gui->plwin) {
      playlist_editor (mrlb->gui);
    }
    else {
      if (!playlist_is_visible (mrlb->gui))
        playlist_toggle_visibility (mrlb->gui);
    }

    if ((!idx) && ((xine_get_status (mrlb->gui->stream) == XINE_STATUS_STOP) || mrlb->gui->logo_mode)) {
      mrlb->gui->playlist.cur = 0;
      gui_current_set_index (mrlb->gui, GUI_MMK_CURRENT);
    }

    playlist_update_playlist (mrlb->gui);

    if ((!is_playback_widgets_enabled (mrlb->gui->panel)) && (idx >= 0))
      enable_playback_controls (mrlb->gui->panel, 1);
  }
}

static void mrl_add(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {
  xui_mrlb_t *mrlb = data;

  (void)w;
  if (mrlb && mrl) {

    if (!mrlb->gui->plwin) {
      playlist_editor (mrlb->gui);
    }
    else {
      if (!playlist_is_visible (mrlb->gui))
        playlist_toggle_visibility (mrlb->gui);
    }

    gui_dndcallback (mrlb->gui, (const char *)mrl->mrl);
  }
}

/*
 * Callback called by mrlbrowser on play event.
 */
static void mrl_play(xitk_widget_t *w, void *data, xine_mrl_t *mrl) {
  xui_mrlb_t *mrlb = data;

  (void)w;
  if (mrlb && mrl) {
    char        *_mrl = mrl->mrl;

    if ((xine_get_status (mrlb->gui->stream) != XINE_STATUS_STOP)) {
      mrlb->gui->ignore_next = 1;
      xine_stop (mrlb->gui->stream);
      mrlb->gui->ignore_next = 0;
    }

    if (!is_playback_widgets_enabled (mrlb->gui->panel))
      enable_playback_controls (mrlb->gui->panel, 1);

    if(mrl_look_like_playlist(_mrl)) {
      if (gui_playlist_add_file (mrlb->gui, _mrl)) {
        gui_current_set_index (mrlb->gui, GUI_MMK_CURRENT);
        _mrl = (char *) mediamark_get_current_mrl (mrlb->gui);
        playlist_update_playlist (mrlb->gui);
      }
    }

    osd_hide (mrlb->gui);

    if (!xine_open (mrlb->gui->stream, (const char *) _mrl)) {
      gui_handle_xine_error (mrlb->gui, mrlb->gui->stream, _mrl);
      enable_playback_controls (mrlb->gui->panel, 0);
      gui_display_logo (mrlb->gui);
      return;
    }

    if (!gui_xine_play (mrlb->gui, mrlb->gui->stream, 0, 0, 0)) {
      enable_playback_controls (mrlb->gui->panel, 0);
      gui_display_logo (mrlb->gui);
      return;
    }

    {
      mediamark_t mmk = { .mrl = _mrl, .end = -1 }, *d;
      gui_playlist_lock (mrlb->gui);
      d = &mrlb->gui->mmk;
      mediamark_copy (&d, &mmk);
      gui_playlist_unlock (mrlb->gui);
    }
  }
}


/*
 * Create a new mrl browser.
 */
void open_mrlbrowser(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (gui) {
    mrl_browser (gui, mrl_add, mrl_play, NULL, gui_dndcallback);
  }
}

void open_mrlbrowser_from_playlist(xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (gui) {
    mrl_browser (gui, mrl_add_noautoplay, mrl_play, NULL, gui_dndcallback);
  }
}
