/* 
 * Copyright (C) 2000-2020 the xine project
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

#include <X11/keysym.h>

#include "common.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/button_list.h"
#include "xine-toolkit/browser.h"


struct xui_playlist_st {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *playlist;
  xitk_widget_t        *winput;
  xitk_button_list_t   *autoplay_buttons;

  xitk_widget_t        *move_up;
  xitk_widget_t        *move_down;
  xitk_widget_t        *play;
  xitk_widget_t        *delete;
  xitk_widget_t        *delete_all;
  xitk_widget_t        *add;
  xitk_widget_t        *load;
  xitk_widget_t        *save;
  xitk_widget_t        *close;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

  char                **playlist_mrls;
  char                **playlist_idents;
  int                   playlist_len;
};

static void playlist_deactivate (xui_playlist_t *pl) {
  if (pl) {
    xitk_disable_widget (pl->playlist);
    xitk_disable_widget (pl->winput);
    xitk_button_list_able (pl->autoplay_buttons, 0);
    xitk_disable_widget (pl->move_up);
    xitk_disable_widget (pl->move_down);
    xitk_disable_widget (pl->play);
    xitk_disable_widget (pl->delete);
    xitk_disable_widget (pl->delete_all);
    xitk_disable_widget (pl->add);
    xitk_disable_widget (pl->load);
    xitk_disable_widget (pl->save);
    xitk_disable_widget (pl->close);
  }
}

static void playlist_reactivate (xui_playlist_t *pl) {
  if (pl) {
    xitk_enable_widget (pl->playlist);
    xitk_enable_widget (pl->winput);
    xitk_button_list_able (pl->autoplay_buttons, 1);
    xitk_enable_widget (pl->move_up);
    xitk_enable_widget (pl->move_down);
    xitk_enable_widget (pl->play);
    xitk_enable_widget (pl->delete);
    xitk_enable_widget (pl->delete_all);
    xitk_enable_widget (pl->add);
    xitk_enable_widget (pl->load);
    xitk_enable_widget (pl->save);
    xitk_enable_widget (pl->close);
  }
}
/*
 *
 */

static void _playlist_update_browser_list (xui_playlist_t *pl, int start) {
  int sel = xitk_browser_get_current_selected (pl->playlist);
  int old_start = xitk_browser_get_current_start (pl->playlist);

  if (pl->gui->is_display_mrl)
    xitk_browser_update_list (pl->playlist, (const char **)pl->playlist_mrls, NULL, pl->playlist_len, start);
  else
    xitk_browser_update_list (pl->playlist, (const char **)pl->playlist_idents, NULL, pl->playlist_len, start);

  if((sel >= 0) && ((start == -1) || (old_start == start)))
    xitk_browser_set_select (pl->playlist, sel);
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

  pthread_mutex_lock (&pl->gui->mmk_mutex);
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
  pthread_mutex_unlock (&pl->gui->mmk_mutex);
}

/*
 *
 */
static void _playlist_handle_selection(xitk_widget_t *w, void *data, int selected, int modifier) {
  xui_playlist_t *pl = data;

  (void)w;
  if (pl->playlist_mrls[selected] != NULL) {
    xitk_inputtext_change_text (pl->winput, pl->playlist_mrls[selected]);
    pthread_mutex_lock (&pl->gui->mmk_mutex);
    mmkeditor_set_mmk (&pl->gui->playlist.mmk[selected]);
    pthread_mutex_unlock (&pl->gui->mmk_mutex);
  }
}

static void _playlist_xine_play (xui_playlist_t *pl) {
  if (!gui_playlist_play (pl->gui, pl->gui->playlist.cur)) {
    if (mediamark_all_played () && (pl->gui->actions_on_start[0] == ACTID_QUIT))
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

  j = xitk_browser_get_current_selected (pl->playlist);
  pthread_mutex_lock (&gui->mmk_mutex);
  if ((j >= 0) && (gui->playlist.mmk[j]->mrl != NULL)) {
    gui->playlist.cur = j;
    _playlist_xine_play (pl);
    xitk_browser_release_all_buttons (pl->playlist);
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
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
  pthread_mutex_lock (&pl->gui->mmk_mutex);
  if (pl->gui->playlist.mmk[selected]->mrl != NULL) {
    pl->gui->playlist.cur = selected;
    _playlist_xine_play (pl);
    xitk_browser_release_all_buttons (pl->playlist);
  }
  pthread_mutex_unlock (&pl->gui->mmk_mutex);
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
    if ((gui->playlist.cur == j) && (xine_get_status (gui->stream) != XINE_STATUS_STOP))
      gui_stop (NULL, gui);

    pthread_mutex_lock (&gui->mmk_mutex);
    mediamark_delete_entry (j);
    pthread_mutex_unlock (&gui->mmk_mutex);

    playlist_update_playlist (gui);

    if (gui->playlist.num) {
      gui_set_current_mmk (mediamark_get_current_mmk ());
      gui->playlist.cur = 0;
    }
    else {

      gui->playlist.cur = -1;

      if (is_playback_widgets_enabled (gui->panel))
        enable_playback_controls (gui->panel, 0);

      if (xine_get_status (gui->stream) != XINE_STATUS_STOP)
        gui_stop (NULL, gui);

      gui_set_current_mmk (NULL);
      if (pl)
        xitk_inputtext_change_text (pl->winput, NULL);
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
  pl = gui->plwin;

  mmk_editor_end ();
  if (!pl)
    return;
  j = xitk_browser_get_current_selected (pl->playlist);
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
  pl = gui->plwin;

  mmk_editor_end ();
  mediamark_free_mediamarks ();
  playlist_update_playlist (gui);
  if (xine_get_status (gui->stream) != XINE_STATUS_STOP)
    gui_stop (NULL, gui);
  if (pl) {
    if (pl->winput)
      xitk_inputtext_change_text (pl->winput, NULL);
    xitk_browser_release_all_buttons (pl->playlist);
  }
  gui_set_current_mmk (NULL);
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
static void _playlist_move_current_updown (xui_playlist_t *pl, int dir) {
  int j;

  mmk_editor_end();

  if (!pl)
    return;
  if ((j = xitk_browser_get_current_selected (pl->playlist)) >= 0) {
    mediamark_t *mmk;
    int          start = xitk_browser_get_current_start (pl->playlist);
    int          max_vis_len = xitk_browser_get_num_entries (pl->playlist);

    if ((dir == -1) && (j > 0)) {
      pthread_mutex_lock (&pl->gui->mmk_mutex);
      mmk = pl->gui->playlist.mmk[j - 1];
      
      if (j == pl->gui->playlist.cur)
        pl->gui->playlist.cur--;

      pl->gui->playlist.mmk[j - 1] = pl->gui->playlist.mmk[j];
      pl->gui->playlist.mmk[j] = mmk;
      j--;
      pthread_mutex_unlock (&pl->gui->mmk_mutex);
    }
    else if ((dir == 1) && (j < (pl->gui->playlist.num - 1))) {
      pthread_mutex_lock (&pl->gui->mmk_mutex);
      mmk = pl->gui->playlist.mmk[j + 1];
      
      if (j == pl->gui->playlist.cur)
        pl->gui->playlist.cur++;

      pl->gui->playlist.mmk[j + 1] = pl->gui->playlist.mmk[j];
      pl->gui->playlist.mmk[j] = mmk;
      j++;
      pthread_mutex_unlock (&pl->gui->mmk_mutex);
    }

    _playlist_create_playlists (pl);

    if(j <= start)
      _playlist_update_browser_list (pl, j);
    else if(j >= (start + max_vis_len))
      _playlist_update_browser_list (pl, start + 1);
    else
      _playlist_update_browser_list (pl, -1);

    xitk_browser_set_select (pl->playlist, j);
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
  xui_playlist_t *pl = gui->plwin;
  char *file;

  mmk_editor_end();

  if (!pl)
    return;
  if ((file = filebrowser_get_full_filename(fb)) != NULL) {
    mediamark_load_mediamarks(file);
    gui_set_current_mmk(mediamark_get_current_mmk());
    playlist_update_playlist (gui);

    if ((xine_get_status (pl->gui->stream) == XINE_STATUS_PLAY))
      gui_stop (NULL, pl->gui);

    if ((!is_playback_widgets_enabled (pl->gui->panel)) && pl->gui->playlist.num)
      enable_playback_controls (pl->gui->panel, 1);

    free(file);
  }
  gui->pl_load = NULL;
  playlist_reactivate (pl);
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
    cbb[1].callback = _playlist_exit_callback;
    cbb[1].userdata = gui;
    
    playlist_deactivate (gui->plwin);
    gui->pl_load = create_filebrowser (_("Load a playlist"), buffer, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
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
    mediamark_save_mediamarks(file);
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
      cbb[1].callback = _playlist_exit_callback;
      cbb[1].userdata = gui;

      playlist_deactivate (gui->plwin);
      gui->pl_save = create_filebrowser (_("Save a playlist"), buffer, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
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
  /* xui_playlist_t *pl = data; */

  (void)w;
  (void)data;
  if (filename)
    gui_dndcallback ((char *)filename);
}

/*
 * Handle X events here.
 */

static void _playlist_handle_button_event (void *data, const xitk_button_event_t *be) {
  xui_playlist_t *pl = data;

  if (!pl)
    return;
  if (be->event == XITK_BUTTON_PRESS) {
    if (be->button == Button3) {
      xitk_widget_t *w = xitk_get_focused_widget (pl->widget_list);

      if (w && ((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)) {
        int wx, wy;

        xitk_window_get_window_position (pl->xwin, &wx, &wy, NULL, NULL);

        playlist_menu (pl->gui, pl->widget_list,
                       be->x + wx, be->y + wy,
                       (xitk_browser_get_current_selected (pl->playlist) >= 0));
      }
    } else if (be->button == Button4 || be->button == Button5) {
      mmk_editor_end();
    }
  }

  if (be->event == XITK_BUTTON_RELEASE) {
    if (pl && pl->playlist) {
      if (xitk_browser_get_current_selected (pl->playlist) < 0)
        mmk_editor_end();
    }

    gui_handle_button_event (pl->gui, be);
  }
}

static void _playlist_handle_key_event(void *data, const xitk_key_event_t *ke) {
  xui_playlist_t *pl = data;

  if (!pl)
    return;

  if (ke->event == XITK_KEY_PRESS) {
    xitk_widget_t *w;

    switch (ke->key_pressed) {
      case XK_Down:
      case XK_Next:
        mmk_editor_end();
        w = xitk_get_focused_widget (pl->widget_list);
        if ((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
          if (ke->key_pressed == XK_Down)
            xitk_browser_step_up (pl->playlist, NULL);
          else
            xitk_browser_page_up (pl->playlist, NULL);
        }
        break;

      case XK_Up:
      case XK_Prior:
        mmk_editor_end();
        w = xitk_get_focused_widget (pl->widget_list);
        if ((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
          if (ke->key_pressed == XK_Up)
            xitk_browser_step_down (pl->playlist, NULL);
          else
            xitk_browser_page_down (pl->playlist, NULL);
        }
        break;

      case XK_Escape:
        playlist_exit (pl->gui);
        break;

      default:
        gui_handle_key_event (pl->gui, ke);
        break;
    }
  }
}

static const xitk_event_cbs_t playlist_event_cbs = {
  .key_cb            = _playlist_handle_key_event,
  .btn_cb            = _playlist_handle_button_event,
  .dnd_cb            = gui_dndcallback,
};

static void _playlist_apply_cb(void *data) {
  gGui_t *gui = data;
  playlist_mrlident_toggle (gui);
  gui_set_current_mmk(mediamark_get_current_mmk());
}

void playlist_get_input_focus (gGui_t *gui) {
  if (gui && gui->plwin)
    xitk_window_try_to_set_input_focus (gui->plwin->xwin);
}

void playlist_mmk_editor (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;
  if (pl) {
    int sel = xitk_browser_get_current_selected (pl->playlist);

    if(sel >= 0) {
      pthread_mutex_lock (&pl->gui->mmk_mutex);
      mmk_edit_mediamark (&pl->gui->playlist.mmk[sel], _playlist_apply_cb, gui);
      pthread_mutex_unlock (&pl->gui->mmk_mutex);
    }
  }
}

static void _scan_for_playlist_infos (gGui_t *gui, xine_stream_t *stream, int n) {
  pthread_mutex_lock (&gui->mmk_mutex);
  if (xine_open (stream, gui->playlist.mmk[n]->mrl)) {
    char  *ident;
    
    if((ident = stream_infos_get_ident_from_stream(stream)) != NULL) {
      
      free (gui->playlist.mmk[n]->ident);
      gui->playlist.mmk[n]->ident = strdup (ident);
      if (n == gui->playlist.cur) {
        free (gui->mmk.ident);
        gui->mmk.ident = strdup (ident);
        panel_update_mrl_display (gui->panel);
      }
      free(ident);
    }
    xine_close(stream);
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}

void playlist_scan_for_infos_selected (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (!pl)
    return;
  if (gui->playlist.num) {
    int                 selected = xitk_browser_get_current_selected (pl->playlist);
    xine_stream_t      *stream;

    if (selected < 0)
      return;
    stream = xine_stream_new (__xineui_global_xine_instance, gui->ao_none, gui->vo_none);
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
    
    stream = xine_stream_new (__xineui_global_xine_instance, gui->ao_none, gui->vo_none);
    for (i = 0; i < gui->playlist.num; i++)
      _scan_for_playlist_infos (gui, stream, i);
    xine_dispose (stream);
    playlist_mrlident_toggle (gui);
  }
}

void playlist_show_tips (gGui_t *gui, int enabled, unsigned long timeout) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;
  
  if (pl) {
    if(enabled)
      xitk_set_widgets_tips_timeout (pl->widget_list, timeout);
    else
      xitk_disable_widgets_tips (pl->widget_list);
  }
}

/*
void playlist_update_tips_timeout (gGui_t *gui, unsigned long timeout) {
  if (gui && gui->plwin)
    xitk_set_widgets_tips_timeout (gui->plwin->widget_list, timeout);
}
*/

void playlist_mrlident_toggle (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;

  if (pl && pl->visible) {
    int start = xitk_browser_get_current_start (pl->playlist);
    
    _playlist_create_playlists (pl);
    _playlist_update_browser_list (pl, start);

    pthread_mutex_lock (&pl->gui->mmk_mutex);
    mmkeditor_set_mmk (&pl->gui->playlist.mmk[xitk_browser_get_current_selected (pl->playlist)]);
    pthread_mutex_unlock (&pl->gui->mmk_mutex);
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
      mmk_editor_end();
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

  if (gui->pl_load)
    filebrowser_end (gui->pl_load);
  if (gui->pl_save)
    filebrowser_end (gui->pl_save);

  if (pl) {
    window_info_t wi;
    
    mmk_editor_end();

    pl->running = 0;
    pl->visible = 0;

    if ((xitk_get_window_info (pl->widget_key, &wi))) {
      config_update_num ("gui.playlist_x", wi.x);
      config_update_num ("gui.playlist_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler (&pl->widget_key);

    xitk_button_list_delete (pl->autoplay_buttons);

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
 * return 1 if playlist editor is ON
 */
int playlist_is_running (gGui_t *gui) {
  return gui ? (gui->plwin ? gui->plwin->running : 0) : 0;
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
      return xitk_window_is_window_visible (pl->xwin);
    else
      return pl->visible && xitk_window_is_window_visible (pl->xwin);
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
  autoplay_mrls = xine_get_autoplay_mrls (__xineui_global_xine_instance, name, &num_mrls);

  if (autoplay_mrls) {
    xine_stream_t *stream;
    int j, cdda_mode = 0;

    if (!strcasecmp (name, "cd"))
      cdda_mode = 1;

    /* Flush playlist in newbie mode */
    if (gui->smart_mode) {
      mediamark_free_mediamarks ();
      playlist_update_playlist (gui);
      if (pl)
        xitk_inputtext_change_text (pl->winput, NULL);
    }

    stream = xine_stream_new (__xineui_global_xine_instance, gui->ao_none, gui->vo_none);
    for (j = 0; j < num_mrls; j++) {
      char *ident = NULL;

      if (cdda_mode && xine_open (stream, autoplay_mrls[j])) {
        ident = stream_infos_get_ident_from_stream (stream);
        xine_close (stream);
      }
      mediamark_append_entry (autoplay_mrls[j], ident ? ident : autoplay_mrls[j], NULL, 0, -1, 0, 0);
      free (ident);
    }
    xine_dispose (stream);

    gui->playlist.cur = gui->playlist.num ? 0 : -1;
    if (gui->playlist.cur == 0)
      gui_set_current_mmk (mediamark_get_current_mmk ());
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
void playlist_raise_window (gGui_t *gui) {
  xui_playlist_t *pl;
  if (!gui)
    return;
  pl = gui->plwin;
  
  if (pl) {
    raise_window (pl->gui, pl->xwin, pl->visible, pl->running);
    mmk_editor_raise_window();
  }
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
    toggle_window (pl->gui, pl->xwin, pl->widget_list, &pl->visible, pl->running);
    mmk_editor_toggle_visibility();
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
  if (!pl->running)
    return;
  if (pl->playlist_len <= 0)
    return;

  pthread_mutex_lock (&pl->gui->mmk_mutex);
  mmk = mediamark_get_current_mmk ();
  if (mmk) {
    char *pa_mrl = pl->gui->is_display_mrl ? pl->gui->mmk.mrl : pl->gui->mmk.ident;
    char *pl_mrl = pl->gui->is_display_mrl ? mmk->mrl : mmk->ident;

    if (!strcmp (pa_mrl, pl_mrl)) {
      int max_displayed = xitk_browser_get_num_entries (pl->playlist);
      int selected = xitk_browser_get_current_selected (pl->playlist);

      if (pl->gui->playlist.num <= max_displayed)
        _playlist_update_browser_list (pl, -1);
      else if (pl->gui->playlist.cur <= pl->gui->playlist.num - max_displayed)
        _playlist_update_browser_list (pl, pl->gui->playlist.cur);
      else
        _playlist_update_browser_list (pl, pl->gui->playlist.num - max_displayed);
      if (selected >= 0) {
        xitk_browser_set_select (pl->playlist, selected);
        xitk_inputtext_change_text (pl->winput, pl->playlist_mrls[selected]);
      } else {
        xitk_inputtext_change_text (pl->winput, pl->playlist_mrls[pl->gui->playlist.cur]);
      }
    }
  } else {
    xitk_inputtext_change_text (pl->winput, NULL);
  }
  pthread_mutex_unlock (&pl->gui->mmk_mutex);
}

/*
 * Change the current skin.
 */
void playlist_change_skins (gGui_t *gui, int synthetic) {
  xui_playlist_t *pl;
  xitk_image_t *bg_image;
  int width, height;

  if (!gui)
    return;
  pl = gui->plwin;
  if (!pl)
    return;
  if (!pl->running)
    return;

  xitk_skin_lock (pl->gui->skin_config);
  xitk_hide_widgets (pl->widget_list);
  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (pl->gui->skin_config, "PlBG");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    xine_error (pl->gui, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
    exit(-1);
  }
  width = xitk_image_width (bg_image);
  height = xitk_image_height (bg_image);
  xitk_window_resize_window (pl->xwin, width, height);
  xitk_window_change_background_with_image (pl->xwin, bg_image, width, height);
  video_window_set_transient_for (pl->gui->vwin, pl->xwin);
  if (playlist_is_visible (gui))
    playlist_raise_window (gui);
  xitk_skin_unlock (pl->gui->skin_config);
    
  xitk_change_skins_widget_list (pl->widget_list, pl->gui->skin_config);
  xitk_button_list_new_skin (pl->autoplay_buttons, pl->gui->skin_config);
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

void playlist_reparent (gGui_t *gui) {
  if (gui && gui->plwin && gui->plwin->xwin)
    reparent_window (gui, gui->plwin->xwin);
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
    xine_error (pl->gui, _("playlist: couldn't find image for background\n"));
    exit(-1);
  }
  width = xitk_image_width(bg_image);
  height = xitk_image_height(bg_image);

  x = xine_config_register_num (__xineui_global_xine_instance, "gui.playlist_x",
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, "gui.playlist_y",
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);

  pl->xwin = xitk_window_create_simple_window_ext (pl->gui->xitk, x, y, width, height,
    title, NULL, "xine", 0, is_layer_above (pl->gui), pl->gui->icon);

  set_window_type_start (pl->gui, pl->xwin);
  xitk_window_change_background_with_image (pl->xwin, bg_image, width, height);

  /*
   * Widget-list
   */
  pl->widget_list = xitk_window_widget_list (pl->xwin);

  {
    xitk_button_widget_t b;
    XITK_WIDGET_INIT (&b);

    b.skin_element_name = "PlMoveUp";
    b.callback          = _playlist_move_current_up;
    b.userdata          = pl->gui;
    pl->move_up =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    if (pl->move_up) {
      xitk_add_widget (pl->widget_list, pl->move_up);
      xitk_set_widget_tips (pl->move_up, _("Move up selected MRL"));
    }

    b.skin_element_name = "PlMoveDn";
    b.callback          = _playlist_move_current_down;
    b.userdata          = pl->gui;
    pl->move_down =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    if (pl->move_down) {
      xitk_add_widget (pl->widget_list, pl->move_down);
      xitk_set_widget_tips (pl->move_down, _("Move down selected MRL"));
    }

    b.skin_element_name = "PlPlay";
    b.callback          = _playlist_play_current;
    b.userdata          = pl->gui;
    pl->play =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    if (pl->play) {
      xitk_add_widget (pl->widget_list, pl->play);
      xitk_set_widget_tips (pl->play, _("Start playback of selected MRL"));
    }

    b.skin_element_name = "PlDelete";
    b.callback          = _playlist_delete_current;
    b.userdata          = pl->gui;
    pl->delete =  xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    if (pl->delete) {
      xitk_add_widget (pl->widget_list, pl->delete);
      xitk_set_widget_tips (pl->delete, _("Delete selected MRL from playlist"));
    }

    b.skin_element_name = "PlDeleteAll";
    b.callback          = _playlist_delete_all;
    b.userdata          = pl->gui;
    pl->delete_all = xitk_button_create (pl->widget_list, pl->gui->skin_config, &b);
    if (pl->delete_all) {
      xitk_add_widget (pl->widget_list, pl->delete_all);
      xitk_set_widget_tips (pl->delete_all, _("Delete all entries in playlist"));
    }
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
    pl->add =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    if (pl->add) {
      xitk_add_widget (pl->widget_list, pl->add);
      xitk_set_widget_tips (pl->add, _("Add one or more entries in playlist"));
    }

    lb.skin_element_name = "PlLoad";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Load");
    lb.callback          = _playlist_load_playlist;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->load =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    if (pl->load) {
      xitk_add_widget (pl->widget_list, pl->load);
      xitk_set_widget_tips (pl->load, _("Load saved playlist"));
    }

    lb.skin_element_name = "PlSave";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Save");
    lb.callback          = _playlist_save_playlist;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->save =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    if (pl->save) {
      xitk_add_widget (pl->widget_list, pl->save);
      xitk_set_widget_tips (pl->save, _("Save playlist"));
    }

    lb.skin_element_name = "PlDismiss";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Dismiss");
    lb.callback          = _playlist_exit;
    lb.state_callback    = NULL;
    lb.userdata          = pl->gui;
    pl->close =  xitk_labelbutton_create (pl->widget_list, pl->gui->skin_config, &lb);
    if (pl->close) {
      xitk_add_widget (pl->widget_list, pl->close);
      xitk_set_widget_tips (pl->close, _("Close playlist window"));
    }
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
    pl->playlist = xitk_browser_create (pl->widget_list, pl->gui->skin_config, &br);
    if (pl->playlist)
      xitk_add_widget (pl->widget_list, pl->playlist);
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
    if (w)
      xitk_add_widget (pl->widget_list, w);
  }

  {
    xitk_inputtext_widget_t inp;
    XITK_WIDGET_INIT (&inp);

    inp.skin_element_name = "PlInputText";
    inp.text              = NULL;
    inp.max_length        = 256;
    inp.callback          = _playlist_add_input;
    inp.userdata          = pl;
    pl->winput =  xitk_inputtext_create (pl->widget_list, pl->gui->skin_config, &inp);
    if (pl->winput) {
      xitk_add_widget (pl->widget_list, pl->winput);
      xitk_set_widget_tips (pl->winput, _("Direct MRL entry"));
    }
  }

  do {
    char *tips[64];
    const char * const *autoplay_plugins = xine_get_autoplay_input_plugin_ids (__xineui_global_xine_instance);
    unsigned int i;

    if (!autoplay_plugins)
      return;

    for (i = 0; autoplay_plugins[i]; i++) {
      if (i >= sizeof (tips) / sizeof (tips[0]))
        break;
      tips[i] = (char *)xine_get_input_plugin_description (__xineui_global_xine_instance, autoplay_plugins[i]);
    }

    pl->autoplay_buttons = xitk_button_list_new (pl->widget_list, pl->gui->skin_config, "AutoPlayBG",
      playlist_scan_input, pl->gui, (char **)autoplay_plugins, tips, 5000, 0);
  } while (0);

  _playlist_update_browser_list (pl, 0);

  playlist_show_tips (pl->gui, panel_get_tips_enable (pl->gui->panel), panel_get_tips_timeout (pl->gui->panel));

  pl->widget_key = xitk_window_register_event_handler ("playlist", pl->xwin, &playlist_event_cbs, pl);

  pl->visible = 1;
  pl->running = 1;

  playlist_update_focused_entry (pl->gui);
  playlist_raise_window (pl->gui);

  xitk_window_try_to_set_input_focus (pl->xwin);
  xitk_set_focus_to_widget (pl->winput);
}
