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
 * Playlist Editor
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "playlist.h"
#include "file_browser.h"
#include "menus.h"
#include "mrl_browser.h"
#include "panel.h"
#include "stream_infos.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/button_list.h"
#include "xine-toolkit/browser.h"
#include "xine-toolkit/skin.h"

typedef enum {
  _W_playlist = 0,
  _W_autoplay_buttons,
  _W_winput,
  _W_move_up,
  _W_move_down,
  _W_play,
  _W_delete,
  _W_delete_all,
  _W_add,
  _W_load,
  _W_save,
  _W_close,
  _W_LAST
} _W_t;

struct xui_playlist_st {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *w[_W_LAST];

  int                   visible;
  xitk_register_key_t   widget_key;

  char                **playlist_mrls;
  char                **playlist_idents;
  int                   playlist_len;
};

static void playlist_deactivate (xui_playlist_t *pl) {
  if (pl)
    xitk_widgets_state (pl->w, _W_LAST, XITK_WIDGET_STATE_ENABLE, 0);
}

static void playlist_reactivate (xui_playlist_t *pl) {
  if (pl)
    xitk_widgets_state (pl->w, _W_LAST, XITK_WIDGET_STATE_ENABLE, ~0u);
}
/*
 *
 */

static void _playlist_update_browser_list (xui_playlist_t *pl, int start) {
  int sel = xitk_browser_get_current_selected (pl->w[_W_playlist]);
  int old_start = xitk_browser_get_current_start (pl->w[_W_playlist]);

  if (pl->gui->is_display_mrl)
    xitk_browser_update_list (pl->w[_W_playlist], (const char **)pl->playlist_mrls, NULL, pl->playlist_len, start);
  else
    xitk_browser_update_list (pl->w[_W_playlist], (const char **)pl->playlist_idents, NULL, pl->playlist_len, start);

  if((sel >= 0) && ((start == -1) || (old_start == start)))
    xitk_browser_set_select (pl->w[_W_playlist], sel);
}


static void _playlist_free_playlists (xui_playlist_t *pl) {
  if (pl->playlist_len) {

    while (pl->playlist_len >= 0) {
      free (pl->playlist_mrls[pl->playlist_len]);
      free (pl->playlist_idents[pl->playlist_len]);
      pl->playlist_len--;
    }

    SAFE_FREE (pl->playlist_mrls);
    SAFE_FREE (pl->playlist_idents);
    pl->playlist_len = 0;
  }
}

static void _playlist_create_playlists (xui_playlist_t *pl) {
  int i;

  _playlist_free_playlists (pl);

  gui_playlist_lock (pl->gui);
  if (pl->gui->playlist.mmk && pl->gui->playlist.num) {

    pl->playlist_mrls = (char **)calloc ((pl->gui->playlist.num + 1), sizeof (char *));
    pl->playlist_idents = (char **)calloc ((pl->gui->playlist.num + 1), sizeof (char *));

    for (i = 0; i < pl->gui->playlist.num; i++) {
      pl->playlist_mrls[i] = strdup (pl->gui->playlist.mmk[i]->mrl);
      pl->playlist_idents[i] = strdup (pl->gui->playlist.mmk[i]->ident);
    }

    pl->playlist_mrls[i] = NULL;
    pl->playlist_idents[i] = NULL;
    pl->playlist_len = pl->gui->playlist.num;
  }
  gui_playlist_unlock (pl->gui);
}

/*
 *
 */
static void _playlist_handle_selection(xitk_widget_t *w, void *data, int selected, int modifier) {
  xui_playlist_t *pl = data;

  (void)w;
  (void)modifier;
  if (selected < 0)
    return;

  if (pl->playlist_mrls[selected] != NULL) {
    xitk_inputtext_change_text (pl->w[_W_winput], pl->playlist_mrls[selected]);
    gui_playlist_lock (pl->gui);
    mmk_editor_set_mmk (pl->gui, &pl->gui->playlist.mmk[selected]);
    gui_playlist_unlock (pl->gui);
  }
}

static void _playlist_xine_play (xui_playlist_t *pl) {
  if (!gui_playlist_play (pl->gui, pl->gui->playlist.cur)) {
    if (mediamark_all_played (pl->gui) && (pl->gui->actions_on_start[0] == ACTID_QUIT))
      gui_exit (NULL, pl->gui);
    else
      gui_display_logo (pl->gui);
  }
}
/*
 * Start playing an MRL
 */
void playlist_play_current (gGui_t *gui) {
  xui_playlist_t *pl;
  int j;

  if (!gui)
    return;
  pl = gui->plwin;
  if (!pl)
    return;

  j = xitk_browser_get_current_selected (pl->w[_W_playlist]);
  gui_playlist_lock (gui);
  if ((j >= 0) && (gui->playlist.mmk[j]->mrl != NULL)) {
    gui->playlist.cur = j;
    _playlist_xine_play (pl);
    xitk_browser_release_all_buttons (pl->w[_W_playlist]);
  }
  gui_playlist_unlock (gui);
}
static void _playlist_play_current (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  playlist_play_current (gui);
}

/*
 * Start to play the selected stream on double click event in playlist.
 */
static void _playlist_play_on_dbl_click(xitk_widget_t *w, void *data, int selected, int modifier) {
  xui_playlist_t *pl = data;

  (void)w;
  (void)modifier;
  gui_playlist_lock (pl->gui);
  if (pl->gui->playlist.mmk[selected]->mrl != NULL) {
    pl->gui->playlist.cur = selected;
    _playlist_xine_play (pl);
    xitk_browser_release_all_buttons (pl->w[_W_playlist]);
  }
  gui_playlist_unlock (pl->gui);
}

/*
 * Delete a given entry from playlist
 */
void playlist_delete_entry (gGui_t *gui, int j) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (j >= 0) {
    int num;

    if ((gui->playlist.cur == j) && (xine_get_status (gui->stream) != XINE_STATUS_STOP))
      gui_stop (NULL, gui);

    num = gui_playlist_remove (gui, j);
    playlist_update_playlist (gui);

    if (num) {
      gui_current_set_index (gui, GUI_MMK_CURRENT);
    } else {
      if (is_playback_widgets_enabled (gui->panel))
        enable_playback_controls (gui->panel, 0);

      if (xine_get_status (gui->stream) != XINE_STATUS_STOP)
        gui_stop (NULL, gui);

      gui_current_set_index (gui, GUI_MMK_NONE);
      if (pl)
        xitk_inputtext_change_text (pl->w[_W_winput], NULL);
    }
  }
}

/*
 * Delete selected MRL
 */
void playlist_delete_current (gGui_t *gui) {
  xui_playlist_t *pl;
  int j;

  if (!gui)
    return;

  mmk_editor_end (gui);

  pl = gui->plwin;
  if (!pl)
    return;
  j = xitk_browser_get_current_selected (pl->w[_W_playlist]);
  playlist_delete_entry (gui, j);
}
static void _playlist_delete_current (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  playlist_delete_current (gui);
}

/*
 * Delete all MRLs
 */
void playlist_delete_all (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return;

  mmk_editor_end (gui);

  pl = gui->plwin;
  if (!pl)
    return;

  gui_playlist_free (gui);
  playlist_update_playlist (gui);
  if (xine_get_status (gui->stream) != XINE_STATUS_STOP)
    gui_stop (NULL, gui);
  if (pl) {
    if (pl->w[_W_winput])
      xitk_inputtext_change_text (pl->w[_W_winput], NULL);
    xitk_browser_release_all_buttons (pl->w[_W_playlist]);
  }
  gui_current_set_index (gui, GUI_MMK_NONE);
  enable_playback_controls (gui->panel, 0);
}
static void _playlist_delete_all (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  playlist_delete_all (gui);
}

/*
 * Move entry up/down in playlist
 */
static void _playlist_move_current_updown (xui_playlist_t *pl, int diff) {
  int j;

  if (!pl)
    return;

  mmk_editor_end (pl->gui);

  if ((j = xitk_browser_get_current_selected (pl->w[_W_playlist])) >= 0) {
    int start = xitk_browser_get_current_start (pl->w[_W_playlist]);
    int max_vis_len = xitk_browser_get_num_entries (pl->w[_W_playlist]);
    int new_pos = gui_playlist_move (pl->gui, j, 1, diff);

    if ((new_pos < 0) || (new_pos == j))
      return;
    _playlist_create_playlists (pl);
    _playlist_update_browser_list (pl,
        (new_pos < start) ? new_pos :
        (new_pos > start + max_vis_len - 1) ? new_pos + 1 - max_vis_len :
        -1);
    xitk_browser_set_select (pl->w[_W_playlist], new_pos);
  }
}

void playlist_move_current_up (gGui_t *gui) {
  if (!gui)
    return;
  _playlist_move_current_updown (gui->plwin, -1);
}
static void _playlist_move_current_up (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  if (!gui)
    return;
  _playlist_move_current_updown (gui->plwin, -1);
}

void playlist_move_current_down (gGui_t *gui) {
  if (!gui)
    return;
  _playlist_move_current_updown (gui->plwin, 1);
}
static void _playlist_move_current_down (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;
  (void)w;
  if (!gui)
    return;
  _playlist_move_current_updown (gui->plwin, 1);
}

/*
 * Load playlist from $HOME/.xine/playlist
 */
static void _playlist_load_callback (filebrowser_t *fb, void *data) {
  gGui_t *gui = data;
  char *file;

  mmk_editor_end (gui);

  if ((file = filebrowser_get_full_filename(fb)) != NULL) {
    gui_playlist_add_item (gui, file, 1, GUI_ITEM_TYPE_PLAYLIST, 1);
    gui_current_set_index (gui, GUI_MMK_CURRENT);
    playlist_update_playlist (gui);

    if ((xine_get_status (gui->stream) == XINE_STATUS_PLAY))
      gui_stop (NULL, gui);

    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    free(file);
  }
  gui->pl_load = NULL;
  playlist_reactivate (gui->plwin);
}

static void _playlist_exit_callback (filebrowser_t *fb, void *data) {
  gGui_t *gui = data;

  if (fb == gui->pl_load)
    gui->pl_load = NULL;
  else if (fb == gui->pl_save)
    gui->pl_save = NULL;
  playlist_reactivate (gui->plwin);
}

void playlist_load_playlist (gGui_t *gui) {
  filebrowser_callback_button_t  cbb[2];
  char *buffer;

  if (!gui)
    return;

  if (gui->pl_load)
    filebrowser_raise_window (gui->pl_load);
  else {
    buffer = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/playlist.tox");
    if (!buffer)
      return;

    cbb[0].label = _("Load");
    cbb[0].callback = _playlist_load_callback;
    cbb[0].userdata = gui;
    cbb[0].need_a_file = 1;
    cbb[1].label = NULL;
    cbb[1].callback = _playlist_exit_callback;
    cbb[1].userdata = gui;
    cbb[1].need_a_file = 0;

    playlist_deactivate (gui->plwin);
    gui->pl_load = create_filebrowser (gui, _("Load a playlist"), buffer, hidden_file_cb, gui, &cbb[0], NULL, &cbb[1]);
    free (buffer);
  }
}
static void _playlist_load_playlist (xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = data;
  (void)w;
  (void)state;
  playlist_load_playlist (gui);
}

/*
 * Save playlist to $HOME/.xine/playlist
 */
static void _playlist_save_callback (filebrowser_t *fb, void *data) {
  gGui_t *gui = data;
  char *file;

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    gui_playlist_save (gui, file);
    free(file);
  }
  gui->pl_save = NULL;
  playlist_reactivate (gui->plwin);
}

void playlist_save_playlist (gGui_t *gui) {
  filebrowser_callback_button_t  cbb[2];
  char *buffer;

  if (!gui)
    return;

  if (gui->playlist.num) {
    if (gui->pl_save)
      filebrowser_raise_window (gui->pl_save);
    else {
      buffer = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/playlist.tox");
      if (!buffer)
        return;

      cbb[0].label = _("Save");
      cbb[0].callback = _playlist_save_callback;
      cbb[0].userdata = gui;
      cbb[0].need_a_file = 1;
      cbb[1].label = NULL;
      cbb[1].callback = _playlist_exit_callback;
      cbb[1].userdata = gui;
      cbb[1].need_a_file = 0;

      playlist_deactivate (gui->plwin);
      gui->pl_save = create_filebrowser (gui, _("Save a playlist"), buffer, hidden_file_cb, gui, &cbb[0], NULL, &cbb[1]);
      free (buffer);
    }
  }
}
static void _playlist_save_playlist (xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = data;
  (void)w;
  (void)state;
  playlist_save_playlist (gui);
}

/*
 *
 */
static void _playlist_add_input(xitk_widget_t *w, void *data, const char *filename) {
  xui_playlist_t *pl = data;

  (void)w;
  if (filename)
    gui_dndcallback (pl->gui, (char *)filename);
}

/*
 * Handle X events here.
 */

static int playlist_event (void *data, const xitk_be_event_t *e) {
  xui_playlist_t *pl = data;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      playlist_deinit (pl->gui);
      return 1;
    case XITK_EV_BUTTON_UP:
      if (pl->w[_W_playlist]) {
        if (xitk_browser_get_current_selected (pl->w[_W_playlist]) < 0) {
          mmk_editor_end (pl->gui);
          return 1;
        }
      }
      break;
    case XITK_EV_BUTTON_DOWN:
      if (e->code == 3) {
        xitk_widget_t *w = xitk_get_focused_widget (pl->widget_list);

        if (w && ((xitk_get_widget_type (w)) & WIDGET_GROUP_BROWSER)) {
          playlist_menu (pl->gui, pl->widget_list, e->w, e->h,
            (xitk_browser_get_current_selected (pl->w[_W_playlist]) >= 0));
          return 1;
        }
      } else if ((e->code == 4) || (e->code == 5)) {
        mmk_editor_end (pl->gui);
        return 1;
      }
      break;
    case XITK_EV_KEY_DOWN:
      if (e->utf8[0] == XITK_CTRL_KEY_PREFIX) {
        xitk_widget_t *w;

        switch (e->utf8[1]) {
          case XITK_MOUSE_WHEEL_UP:
          case XITK_KEY_UP:
          case XITK_MOUSE_WHEEL_DOWN:
          case XITK_KEY_DOWN:
          case XITK_KEY_NEXT:
          case XITK_KEY_PREV:
            mmk_editor_end (pl->gui);
            w = xitk_get_focused_widget (pl->widget_list);
            if ((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER))))
              xitk_widget_key_event (pl->w[_W_playlist], e->utf8, e->qual, 0);
            return 1;
          case XITK_KEY_ESCAPE:
            playlist_exit (pl->gui);
            return 1;
          case XITK_KEY_MENU:
            {
              xitk_widget_t *w = xitk_get_focused_widget (pl->widget_list);

              if (w && ((xitk_get_widget_type (w)) & WIDGET_GROUP_BROWSER)) {
                playlist_menu (pl->gui, pl->widget_list, e->w, e->h,
                  (xitk_browser_get_current_selected (pl->w[_W_playlist]) >= 0));
                return 1;
              }
            }
            break;
          default: ;
        }
      }
    default: ;
  }
  return gui_handle_be_event (pl->gui, e);
}

static void _playlist_apply_cb(void *data) {
  gGui_t *gui = data;
  playlist_mrlident_toggle (gui);
  gui_current_set_index (gui, GUI_MMK_CURRENT);
}

void playlist_get_input_focus (gGui_t *gui) {
  if (gui && gui->plwin)
    xitk_window_set_input_focus (gui->plwin->xwin);
}

void playlist_mmk_editor (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;
  if (pl) {
    int sel = xitk_browser_get_current_selected (pl->w[_W_playlist]);

    if(sel >= 0) {
      gui_playlist_lock (pl->gui);
      mmk_edit_mediamark (pl->gui, &pl->gui->playlist.mmk[sel], _playlist_apply_cb, gui);
      gui_playlist_unlock (pl->gui);
    }
  }
}

static void _scan_for_playlist_infos (gGui_t *gui, xine_stream_t *stream, int n) {
  gui_playlist_lock (gui);
  if (xine_open (stream, gui->playlist.mmk[n]->mrl)) {
    char  *ident;

    if ((ident = stream_infos_get_ident_from_stream (stream)) != NULL) {
      gui_playlist_set_str_val (gui, ident, MMK_VAL_IDENT, n);
      if (n == gui->playlist.cur) {
        mediamark_t *m = &gui->mmk;

        if (mediamark_set_str_val (&m, ident, MMK_VAL_IDENT))
          panel_update_mrl_display (gui->panel);
      }
      free(ident);
    }
    xine_close(stream);
  }
  gui_playlist_unlock (gui);
}

void playlist_scan_for_infos_selected (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (!pl)
    return;
  if (gui->playlist.num) {
    int                 selected = xitk_browser_get_current_selected (pl->w[_W_playlist]);
    xine_stream_t      *stream;

    if (selected < 0)
      return;
    stream = xine_stream_new (gui->xine, gui->ao_none, gui->vo_none);
    _scan_for_playlist_infos (gui, stream, selected);
    xine_dispose (stream);
    playlist_mrlident_toggle (gui);
  }
}

void playlist_scan_for_infos (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (!pl)
    return;
  if (gui->playlist.num) {
    int                 i;
    xine_stream_t      *stream;

    stream = xine_stream_new (gui->xine, gui->ao_none, gui->vo_none);
    for (i = 0; i < gui->playlist.num; i++)
      _scan_for_playlist_infos (gui, stream, i);
    xine_dispose (stream);
    playlist_mrlident_toggle (gui);
  }
}

void playlist_mrlident_toggle (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (pl && pl->visible) {
    int start = xitk_browser_get_current_start (pl->w[_W_playlist]);

    _playlist_create_playlists (pl);
    _playlist_update_browser_list (pl, start);

    gui_playlist_lock (pl->gui);
    mmk_editor_set_mmk (pl->gui, &pl->gui->playlist.mmk[xitk_browser_get_current_selected (pl->w[_W_playlist])]);
    gui_playlist_unlock (pl->gui);
  }
}

/*
 *
 */
void playlist_update_playlist (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return;
  pl = gui->plwin;

  if (pl) {
    _playlist_create_playlists (pl);
    if (playlist_is_visible (gui)) {
      _playlist_update_browser_list (pl, 0);
      mmk_editor_end (pl->gui);
    }
  }
}

/*
 * Leaving playlist editor
 */
void playlist_exit (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return;
  pl = gui->plwin;

  if (gui->pl_load) {
    filebrowser_end (gui->pl_load);
    gui->pl_load = NULL;
  }
  if (gui->pl_save) {
    filebrowser_end (gui->pl_save);
    gui->pl_save = NULL;
  }

  if (pl) {
    mmk_editor_end (gui);

    pl->visible = 0;

    gui_save_window_pos (pl->gui, "playlist", pl->widget_key);

    xitk_unregister_event_handler (gui->xitk, &pl->widget_key);

    xitk_window_destroy_window (pl->xwin);
    pl->xwin = NULL;

    /* xitk_dlist_init (&pl->widget_list->list); */

    _playlist_free_playlists (pl);

    video_window_set_input_focus (pl->gui->vwin);

    pl->gui->plwin = NULL;
    free (pl);
  }
}
static void _playlist_exit (xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = data;
  (void)w;
  (void)state;
  playlist_exit (gui);
}

/*
 * Return 1 if playlist editor is visible
 */
int playlist_is_visible (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return 0;
  pl = gui->plwin;

  if (pl) {
    if (gui->use_root_window)
      return (xitk_window_flags (pl->xwin, 0, 0) & XITK_WINF_VISIBLE);
    else
      return pl->visible && (xitk_window_flags (pl->xwin, 0, 0) & XITK_WINF_VISIBLE);
  }
  return 0;
}

/*
 * Handle autoplay buttons hitting (from panel and playlist windows)
 */
void playlist_scan_input (xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = data;
  xui_playlist_t *pl;
  const char *name;
  int num_mrls = 0;
  const char * const *autoplay_mrls;

  (void)state;
  if (!gui)
    return;
  pl = gui->plwin;
  name = xitk_labelbutton_get_label (w);
  autoplay_mrls = xine_get_autoplay_mrls (gui->xine, name, &num_mrls);

  if (autoplay_mrls) {
    xine_stream_t *stream;
    int j, cdda_mode = 0;

    if (!strcasecmp (name, "cd"))
      cdda_mode = 1;

    /* Flush playlist in newbie mode */
    if (gui->smart_mode) {
      gui_playlist_free (gui);
      playlist_update_playlist (gui);
      if (pl)
        xitk_inputtext_change_text (pl->w[_W_winput], NULL);
    }

    stream = xine_stream_new (gui->xine, gui->ao_none, gui->vo_none);
    for (j = 0; j < num_mrls; j++) {
      char *ident = NULL;

      if (cdda_mode && xine_open (stream, autoplay_mrls[j])) {
        ident = stream_infos_get_ident_from_stream (stream);
        xine_close (stream);
      }
      gui_playlist_append (gui, autoplay_mrls[j], ident ? ident : autoplay_mrls[j], NULL, 0, -1, 0, 0);
      free (ident);
    }
    xine_dispose (stream);

    gui->playlist.cur = gui->playlist.num ? 0 : -1;
    if (gui->playlist.cur == 0)
      gui_current_set_index (gui, GUI_MMK_CURRENT);
    /*
     * If we're in newbie mode, start playback immediately
     * (even ignoring if we're currently playing something
     */
    if (gui->smart_mode) {
      if (xine_get_status (gui->stream) == XINE_STATUS_PLAY)
        gui_stop (NULL, gui);
      gui_play (NULL, gui);
    }

    if (pl) {
      _playlist_create_playlists (pl);
      _playlist_update_browser_list (pl, 0);
    }

    enable_playback_controls (gui->panel, (gui->playlist.num > 0));
  }
}

/*
 * Raise playlist->window
 */
static void playlist_raise_window (xui_playlist_t *pl) {
  raise_window (pl->gui, pl->xwin, pl->visible, 1);
  mmk_editor_raise_window (pl->gui);
}

/*
 * Hide/show the pl panel
 */
void playlist_toggle_visibility (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return;
  pl = gui->plwin;
  if (pl) {
    toggle_window (pl->gui, pl->xwin, pl->widget_list, &pl->visible, 1);
    mmk_editor_toggle_visibility (pl->gui);
  }
}

/*
 * Update first displayed entry and input text widget content with
 * current selected playlist entry.
 */
void playlist_update_focused_entry (gGui_t *gui) {
  xui_playlist_t *pl;
  mediamark_t *mmk;
  if (!gui)
    return;
  pl = gui->plwin;

  if (!pl)
    return;
  if (!pl->xwin)
    return;
  if (!pl->visible)
    return;
  if (pl->playlist_len <= 0)
    return;

  gui_playlist_lock (pl->gui);
  mmk = mediamark_get_current_mmk (gui);
  if (mmk) {
    char *pa_mrl = pl->gui->is_display_mrl ? pl->gui->mmk.mrl : pl->gui->mmk.ident;
    char *pl_mrl = pl->gui->is_display_mrl ? mmk->mrl : mmk->ident;

    if (!strcmp (pa_mrl, pl_mrl)) {
      int max_displayed = xitk_browser_get_num_entries (pl->w[_W_playlist]);
      int selected = xitk_browser_get_current_selected (pl->w[_W_playlist]);

      if (pl->gui->playlist.num <= max_displayed)
        _playlist_update_browser_list (pl, -1);
      else if (pl->gui->playlist.cur <= pl->gui->playlist.num - max_displayed)
        _playlist_update_browser_list (pl, pl->gui->playlist.cur);
      else
        _playlist_update_browser_list (pl, pl->gui->playlist.num - max_displayed);
      if (selected >= 0) {
        xitk_browser_set_select (pl->w[_W_playlist], selected);
        xitk_inputtext_change_text (pl->w[_W_winput], pl->playlist_mrls[selected]);
      } else {
        xitk_inputtext_change_text (pl->w[_W_winput], pl->playlist_mrls[pl->gui->playlist.cur]);
      }
    }
  } else {
    xitk_inputtext_change_text (pl->w[_W_winput], NULL);
  }
  gui_playlist_unlock (pl->gui);
}

/*
 * Change the current skin.
 */
void playlist_change_skins (gGui_t *gui, int synthetic) {
  xui_playlist_t *pl;
  xitk_image_t *bg_image;

  if (!gui)
    return;
  pl = gui->plwin;
  if (!pl)
    return;

  (void)synthetic;
  xitk_skin_lock (pl->gui->skin_config);
  xitk_hide_widgets (pl->widget_list);
  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (pl->gui->skin_config, "PlBG");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    gui_msg (pl->gui, XUI_MSG_ERROR, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
    exit(-1);
  }
  xitk_window_set_background_image (pl->xwin, bg_image);
  video_window_set_transient_for (pl->gui->vwin, pl->xwin);
  if (playlist_is_visible (gui))
    playlist_raise_window (pl);
  xitk_skin_unlock (pl->gui->skin_config);

  xitk_change_skins_widget_list (pl->widget_list, pl->gui->skin_config);
  xitk_paint_widget_list (pl->widget_list);
}

void playlist_deinit (gGui_t *gui) {
  xui_playlist_t *pl;

  if (!gui)
    return;
  pl = gui->plwin;

  if (pl) {
    if (playlist_is_visible (gui))
      playlist_toggle_visibility (gui);
    playlist_exit (gui);
  }
  else {
    if (gui->pl_load) {
      filebrowser_end (gui->pl_load);
      gui->pl_load = NULL;
    }
    if (gui->pl_save) {
      filebrowser_end (gui->pl_save);
      gui->pl_save = NULL;
    }
  }
}


static void _open_mrlbrowser_from_playlist (xitk_widget_t *w, void *data, int state) {
  (void)state;
  open_mrlbrowser_from_playlist (w, data);
}

/*
 * Create playlist editor window
 */
void playlist_editor (gGui_t *gui) {
  xui_playlist_t *pl;
  char                      *title = _("xine Playlist Editor");
  int                        x, y, width, height;
  xitk_image_t              *bg_image;

  if (!gui)
    return;
  if (gui->plwin)
    return;
  pl = (xui_playlist_t *)calloc (1, sizeof (xui_playlist_t));
  if (!pl)
    return;

  pl->gui = gui;
  pl->gui->plwin = pl;
  pl->playlist_len = 0;

  _playlist_create_playlists (pl);

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (pl->gui->skin_config, "PlBG");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    gui_msg (pl->gui, XUI_MSG_ERROR, _("playlist: couldn't find image for background\n"));
    exit(-1);
  }
  width = xitk_image_width(bg_image);
  height = xitk_image_height(bg_image);

  x = y = 200;
  gui_load_window_pos (pl->gui, "playlist", &x, &y);

  pl->xwin = xitk_window_create_window_ext (pl->gui->xitk, x, y, width, height,
    title, NULL, "xine", 0, is_layer_above (pl->gui), pl->gui->icon, bg_image);

  set_window_type_start (pl->gui, pl->xwin);

  /*
   * Widget-list
   */
  pl->widget_list = xitk_window_widget_list (pl->xwin);

  {
    xitk_button_widget_t b;

    XITK_WIDGET_INIT (&b);
    b.state_callback    = NULL;
    b.userdata          = pl->gui;

    b.skin_element_name = "PlMoveUp";
    b.callback          = _playlist_move_current_up;
    pl->w[_W_move_up] =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    xitk_add_widget (pl->widget_list, pl->w[_W_move_up], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_move_up], _("Move up selected MRL"));

    b.skin_element_name = "PlMoveDn";
    b.callback          = _playlist_move_current_down;
    pl->w[_W_move_down] =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    xitk_add_widget (pl->widget_list, pl->w[_W_move_down], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_move_down], _("Move down selected MRL"));

    b.skin_element_name = "PlPlay";
    b.callback          = _playlist_play_current;
    pl->w[_W_play] =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    xitk_add_widget (pl->widget_list, pl->w[_W_play], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_play], _("Start playback of selected MRL"));

    b.skin_element_name = "PlDelete";
    b.callback          = _playlist_delete_current;
    pl->w[_W_delete] =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    xitk_add_widget (pl->widget_list, pl->w[_W_delete], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_delete], _("Delete selected MRL from playlist"));

    b.skin_element_name = "PlDeleteAll";
    b.callback          = _playlist_delete_all;
    pl->w[_W_delete_all] = xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    xitk_add_widget (pl->widget_list, pl->w[_W_delete_all], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_delete_all], _("Delete all entries in playlist"));
  }

  {
    xitk_labelbutton_widget_t lb;
    XITK_WIDGET_INIT (&lb);

    lb.skin_element_name = "PlAdd";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Add");
    lb.align             = ALIGN_DEFAULT;
    lb.callback          = _open_mrlbrowser_from_playlist;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->w[_W_add] =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    xitk_add_widget (pl->widget_list, pl->w[_W_add], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_add], _("Add one or more entries in playlist"));

    lb.skin_element_name = "PlLoad";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Load");
    lb.callback          = _playlist_load_playlist;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->w[_W_load] =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    xitk_add_widget (pl->widget_list, pl->w[_W_load], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_load], _("Load saved playlist"));

    lb.skin_element_name = "PlSave";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Save");
    lb.callback          = _playlist_save_playlist;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->w[_W_save] =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    xitk_add_widget (pl->widget_list, pl->w[_W_save], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_save], _("Save playlist"));

    lb.skin_element_name = "PlDismiss";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Dismiss");
    lb.callback          = _playlist_exit;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->w[_W_close] =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    xitk_add_widget (pl->widget_list, pl->w[_W_close], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_close], _("Close playlist window"));
  }

  {
    xitk_browser_widget_t br;
    XITK_WIDGET_INIT (&br);

    br.arrow_up.skin_element_name    = "PlUp";
    br.slider.skin_element_name      = "SliderPl";
    br.arrow_dn.skin_element_name    = "PlDn";
    br.arrow_left.skin_element_name  = "PlLeft";
    br.slider_h.skin_element_name    = "SliderHPl";
    br.arrow_right.skin_element_name = "PlRight";
    br.browser.skin_element_name     = "PlItemBtn";
    br.browser.num_entries           = pl->playlist_len;
    br.browser.entries               = (const char **)pl->playlist_mrls;
    br.callback                      = _playlist_handle_selection;
    br.dbl_click_callback            = _playlist_play_on_dbl_click;
    br.userdata                      = pl;
    pl->w[_W_playlist] = xitk_browser_create (pl->widget_list, pl->gui->skin_config, &br);
    xitk_add_widget (pl->widget_list, pl->w[_W_playlist], XITK_WIDGET_STATE_KEEP);
  }

  {
    xitk_widget_t *w;
    xitk_label_widget_t lbl;
    XITK_WIDGET_INIT (&lbl);

    lbl.skin_element_name = "AutoPlayLbl";
    /* TRANSLATORS: only ASCII characters (skin) */
    lbl.label             = pgettext("skin", "Scan for:");
    lbl.callback          = NULL;
    w = xitk_label_create (pl->widget_list, pl->gui->skin_config, &lbl);
    xitk_add_widget (pl->widget_list, w, XITK_WIDGET_STATE_KEEP);
  }

  {
    xitk_inputtext_widget_t inp;
    XITK_WIDGET_INIT (&inp);

    inp.skin_element_name = "PlInputText";
    inp.text              = NULL;
    inp.max_length        = 256;
    inp.callback          = _playlist_add_input;
    inp.userdata          = pl;
    pl->w[_W_winput] =  xitk_inputtext_create (pl->widget_list, pl->gui->skin_config, &inp);
    xitk_add_widget (pl->widget_list, pl->w[_W_winput], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_winput], _("Direct MRL entry"));
  }

  do {
    const char *tips[64];
    const char * const *autoplay_plugins = xine_get_autoplay_input_plugin_ids (pl->gui->xine);
    unsigned int i;

    if (!autoplay_plugins)
      return;

    for (i = 0; autoplay_plugins[i]; i++) {
      if (i >= sizeof (tips) / sizeof (tips[0]))
        break;
      tips[i] = xine_get_input_plugin_description (pl->gui->xine, autoplay_plugins[i]);
    }

    pl->w[_W_autoplay_buttons] = xitk_button_list_new (pl->widget_list, pl->gui->skin_config, "AutoPlayBG",
      playlist_scan_input, pl->gui, autoplay_plugins, tips, 5000, 0);
    xitk_add_widget (pl->widget_list, pl->w[_W_autoplay_buttons], XITK_WIDGET_STATE_KEEP);
    xitk_set_widget_tips (pl->w[_W_autoplay_buttons], _("More sources..."));
  } while (0);

  _playlist_update_browser_list (pl, 0);

  pl->widget_key = xitk_be_register_event_handler ("playlist", pl->xwin, playlist_event, pl, NULL, NULL);
  xitk_window_flags (pl->xwin, XITK_WINF_DND, XITK_WINF_DND);

  pl->visible = 1;

  playlist_update_focused_entry (pl->gui);
  playlist_raise_window (pl);

  xitk_window_set_input_focus (pl->xwin);
  xitk_set_focus_to_widget (pl->w[_W_winput]);
}

