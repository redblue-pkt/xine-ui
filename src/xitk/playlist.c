/* 
 * Copyright (C) 2000-2019 the xine project
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"
#include "xine-toolkit/button_list.h"


typedef struct {
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

} _playlist_t;

static filebrowser_t   *load_fb = NULL, *save_fb = NULL;
static _playlist_t     *playlist;

void playlist_handle_event(XEvent *event, void *data);

static void playlist_deactivate (void) {
  if (playlist) {
    xitk_disable_widget (playlist->playlist);
    xitk_disable_widget (playlist->winput);
    xitk_button_list_able (playlist->autoplay_buttons, 0);
    xitk_disable_widget (playlist->move_up);
    xitk_disable_widget (playlist->move_down);
    xitk_disable_widget (playlist->play);
    xitk_disable_widget (playlist->delete);
    xitk_disable_widget (playlist->delete_all);
    xitk_disable_widget (playlist->add);
    xitk_disable_widget (playlist->load);
    xitk_disable_widget (playlist->save);
    xitk_disable_widget (playlist->close);
  }
}

static void playlist_reactivate (void) {
  if (playlist) {
    xitk_enable_widget (playlist->playlist);
    xitk_enable_widget (playlist->winput);
    xitk_button_list_able (playlist->autoplay_buttons, 1);
    xitk_enable_widget (playlist->move_up);
    xitk_enable_widget (playlist->move_down);
    xitk_enable_widget (playlist->play);
    xitk_enable_widget (playlist->delete);
    xitk_enable_widget (playlist->delete_all);
    xitk_enable_widget (playlist->add);
    xitk_enable_widget (playlist->load);
    xitk_enable_widget (playlist->save);
    xitk_enable_widget (playlist->close);
  }
}
/*
 *
 */

static void _playlist_update_browser_list(int start) {
  gGui_t *gui = gGui;
  int sel = xitk_browser_get_current_selected(playlist->playlist);
  int old_start = xitk_browser_get_current_start(playlist->playlist);

  if(gui->is_display_mrl)
    xitk_browser_update_list(playlist->playlist, 
			     (const char **)playlist->playlist_mrls, NULL, 
			     playlist->playlist_len, start);
  else
    xitk_browser_update_list(playlist->playlist, 
			     (const char **)playlist->playlist_idents, NULL,
			     playlist->playlist_len, start);

  if((sel >= 0) && ((start == -1) || (old_start == start)))
    xitk_browser_set_select(playlist->playlist, sel);
}


static void _playlist_free_playlists(void) {

  if(playlist->playlist_len) {

    while(playlist->playlist_len >= 0) {
      free(playlist->playlist_mrls[playlist->playlist_len]);
      free(playlist->playlist_idents[playlist->playlist_len]);
      playlist->playlist_len--;
    }
    
    SAFE_FREE(playlist->playlist_mrls);
    SAFE_FREE(playlist->playlist_idents);
    playlist->playlist_len = 0;
  }
}

static void _playlist_create_playlists(void) {
  gGui_t *gui = gGui;
  int i;

  _playlist_free_playlists();

  pthread_mutex_lock (&gui->mmk_mutex);
  if(gui->playlist.mmk && gui->playlist.num) {

    playlist->playlist_mrls = (char **) calloc((gui->playlist.num + 1), sizeof(char *));
    playlist->playlist_idents = (char **) calloc((gui->playlist.num + 1), sizeof(char *));

    for(i = 0; i < gui->playlist.num; i++) {
      playlist->playlist_mrls[i] = strdup(gui->playlist.mmk[i]->mrl);
      playlist->playlist_idents[i] = strdup(gui->playlist.mmk[i]->ident);
    }

    playlist->playlist_mrls[i] = NULL;
    playlist->playlist_idents[i] = NULL;
    playlist->playlist_len = gui->playlist.num;
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}

/*
 *
 */
static void _playlist_handle_selection(xitk_widget_t *w, void *data, int selected) {
  gGui_t *gui = gGui;

  if(playlist->playlist_mrls[selected] != NULL) {
    xitk_inputtext_change_text(playlist->winput, playlist->playlist_mrls[selected]);
    pthread_mutex_lock (&gui->mmk_mutex);
    mmkeditor_set_mmk(&gui->playlist.mmk[selected]);
    pthread_mutex_unlock (&gui->mmk_mutex);
  }

}

static void _playlist_xine_play(void) {
  gGui_t *gui = gGui;

  if (!gui_playlist_play (gui, gui->playlist.cur)) {

    if(mediamark_all_played() && (gui->actions_on_start[0] == ACTID_QUIT))
      gui_exit (NULL, gui);
    else
      gui_display_logo();
  }
}
/*
 * Start playing an MRL
 */
void playlist_play_current(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int j;
  
  j = xitk_browser_get_current_selected(playlist->playlist);

  pthread_mutex_lock (&gui->mmk_mutex);
  if((j >= 0) && (gui->playlist.mmk[j]->mrl != NULL)) {
    gui->playlist.cur = j;
    _playlist_xine_play();
    xitk_browser_release_all_buttons(playlist->playlist);
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}
/*
 * Start to play the selected stream on double click event in playlist.
 */
static void _playlist_play_on_dbl_click(xitk_widget_t *w, void *data, int selected) {
  gGui_t *gui = gGui;
  
  pthread_mutex_lock (&gui->mmk_mutex);
  if(gui->playlist.mmk[selected]->mrl != NULL) {
    gui->playlist.cur = selected;
    _playlist_xine_play();
    xitk_browser_release_all_buttons(playlist->playlist);
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}

/*
 * Delete a given entry from playlist
 */
void playlist_delete_entry(int j) {
  gGui_t *gui = gGui;

  if(j  >= 0) {

    if((gui->playlist.cur == j) && ((xine_get_status(gui->stream) != XINE_STATUS_STOP)))
      gui_stop (NULL, gui);

    pthread_mutex_lock (&gui->mmk_mutex);
    mediamark_delete_entry(j);
    pthread_mutex_unlock (&gui->mmk_mutex);

    playlist_update_playlist();

    if(gui->playlist.num) {
      gui_set_current_mmk(mediamark_get_current_mmk());
      gui->playlist.cur = 0;
    }
    else {

      gui->playlist.cur = -1;

      if (is_playback_widgets_enabled (gui->panel))
        enable_playback_controls (gui->panel, 0);

      if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
        gui_stop (NULL, gui);

      gui_set_current_mmk(NULL);
      xitk_inputtext_change_text(playlist->winput, NULL);
    }
  }

}

/*
 * Delete selected MRL
 */
void playlist_delete_current(xitk_widget_t *w, void *data) {
  int j;
  
  mmk_editor_end();

  j = xitk_browser_get_current_selected(playlist->playlist);

  playlist_delete_entry(j);
}

/*
 * Delete all MRLs
 */
void playlist_delete_all(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  mmk_editor_end();

  mediamark_free_mediamarks();
  playlist_update_playlist();
  
  if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
    gui_stop (NULL, gui);
  
  if(playlist && playlist->winput)
    xitk_inputtext_change_text(playlist->winput, NULL);

  if(playlist)
    xitk_browser_release_all_buttons(playlist->playlist);

  gui_set_current_mmk(NULL);
  enable_playback_controls (gui->panel, 0);
}

/*
 * Move entry up/down in playlist
 */
void playlist_move_current_updown(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int j;

  mmk_editor_end();

  if((j = xitk_browser_get_current_selected(playlist->playlist)) >= 0) {
    mediamark_t *mmk;
    int          start = xitk_browser_get_current_start(playlist->playlist);
    int          max_vis_len = xitk_browser_get_num_entries(playlist->playlist);

    if(((intptr_t)data) == DIRECTION_UP && (j > 0)) {
      pthread_mutex_lock (&gui->mmk_mutex);
      mmk = gui->playlist.mmk[j - 1];
      
      if(j == gui->playlist.cur)
	gui->playlist.cur--;

      gui->playlist.mmk[j - 1] = gui->playlist.mmk[j];
      gui->playlist.mmk[j] = mmk;
      j--;
      pthread_mutex_unlock (&gui->mmk_mutex);
    }
    else if(((intptr_t)data) == DIRECTION_DOWN && (j < (gui->playlist.num - 1))) {
      pthread_mutex_lock (&gui->mmk_mutex);
      mmk = gui->playlist.mmk[j + 1];
      
      if(j == gui->playlist.cur)
	gui->playlist.cur++;

      gui->playlist.mmk[j + 1] = gui->playlist.mmk[j];
      gui->playlist.mmk[j] = mmk;
      j++;
      pthread_mutex_unlock (&gui->mmk_mutex);
    }

    _playlist_create_playlists();

    if(j <= start)
      _playlist_update_browser_list(j);
    else if(j >= (start + max_vis_len))
      _playlist_update_browser_list(start + 1);
    else
      _playlist_update_browser_list(-1);

    xitk_browser_set_select(playlist->playlist, j);

    
  }
}

/*
 * Load playlist from $HOME/.xine/playlist
 */
static void _playlist_load_callback(filebrowser_t *fb) {
  gGui_t *gui = gGui;
  char *file;

  mmk_editor_end();

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    mediamark_load_mediamarks(file);
    gui_set_current_mmk(mediamark_get_current_mmk());
    playlist_update_playlist();

    if((xine_get_status(gui->stream) == XINE_STATUS_PLAY))
      gui_stop (NULL, gui);

    if ((!is_playback_widgets_enabled (gui->panel)) && gui->playlist.num)
      enable_playback_controls (gui->panel, 1);

    free(file);
  }
  load_fb = NULL;
  playlist_reactivate();
}
static void _playlist_cancel_callback(filebrowser_t *fb) {
  if(fb == load_fb)
    load_fb = NULL;
  else if(fb == save_fb)
    save_fb = NULL;

  playlist_reactivate();
}
void playlist_load_playlist(xitk_widget_t *w, void *data) {
  filebrowser_callback_button_t  cbb[2];
  char                          *buffer;

  if(load_fb)
    filebrowser_raise_window(load_fb);
  else {
    buffer = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/playlist.tox");
    if (!buffer)
      return;

    cbb[0].label = _("Load");
    cbb[0].callback = _playlist_load_callback;
    cbb[0].need_a_file = 1;
    cbb[1].callback = _playlist_cancel_callback;
    
    playlist_deactivate();
    load_fb = create_filebrowser(_("Load a playlist"), buffer, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
    free(buffer);
  }
}

/*
 * Save playlist to $HOME/.xine/playlist
 */
static void _playlist_save_callback(filebrowser_t *fb) {
  char *file;

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    mediamark_save_mediamarks(file);
    free(file);
  }
  save_fb = NULL;
  playlist_reactivate();
}
void playlist_save_playlist(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  filebrowser_callback_button_t  cbb[2];
  char                          *buffer;

  if(gui->playlist.num) {
    if(save_fb)
      filebrowser_raise_window(save_fb);
    else {
      buffer = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/playlist.tox");
      if (!buffer)
        return;

      cbb[0].label = _("Save");
      cbb[0].callback = _playlist_save_callback;
      cbb[0].need_a_file = 1;
      cbb[1].callback = _playlist_cancel_callback;
      
      playlist_deactivate();
      save_fb = create_filebrowser(_("Save a playlist"), buffer, hidden_file_cb, &cbb[0], NULL, &cbb[1]);
      free(buffer);
    }
  }
}

/*
 *
 */
static void _playlist_add_input(xitk_widget_t *w, void *data, const char *filename) {

  if(filename)
    gui_dndcallback((char *)filename);

}

/*
 * Handle X events here.
 */
static void _playlist_handle_event(XEvent *event, void *data) {
  gGui_t *gui = gGui;

  switch(event->type) {

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;

    if(bevent->button == Button3) {
      xitk_widget_t *w = xitk_get_focused_widget(playlist->widget_list);
      
      if(w && ((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)) {
	int wx, wy;

        xitk_window_get_window_position(playlist->xwin, &wx, &wy, NULL, NULL);

        playlist_menu (gui, playlist->widget_list,
		      bevent->x + wx, bevent->y + wy, 
		      (xitk_browser_get_current_selected(playlist->playlist) >= 0));
      }

    }
    else if((bevent->button == Button4) || (bevent->button == Button5))
      mmk_editor_end();

  }
    break;

  case ButtonRelease:
    if(playlist && playlist->playlist) {
      if(xitk_browser_get_current_selected(playlist->playlist) < 0)
	mmk_editor_end();
    }

    gui_handle_event (event, gui);
    break;

  case KeyPress:
    {
      KeySym mkey;
      
      if(!playlist)
	return;

      mkey = xitk_get_key_pressed(event);

      switch (mkey) {
	
      case XK_Down:
      case XK_Next: {
	xitk_widget_t *w;
	
	mmk_editor_end();
	w = xitk_get_focused_widget(playlist->widget_list);
	if((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
	  if(mkey == XK_Down)
	    xitk_browser_step_up(playlist->playlist, NULL);
	  else
	    xitk_browser_page_up(playlist->playlist, NULL);
	}
      }
	break;
	
      case XK_Up:
      case XK_Prior: {
	xitk_widget_t *w;
	
	mmk_editor_end();
	w = xitk_get_focused_widget(playlist->widget_list);
	if((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
	  if(mkey == XK_Up)
	    xitk_browser_step_down(playlist->playlist, NULL);
	  else
	    xitk_browser_page_down(playlist->playlist, NULL);
	}
      }
	break;

      case XK_Escape:
	playlist_exit(NULL, NULL);
	break;

      default:
        gui_handle_event (event, gui);
	break;
      }
    }
    break;

  case MappingNotify:
    gui->x_lock_display (gui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    gui->x_unlock_display (gui->display);
    break;
    
  }
}

static void _playlist_apply_cb(void *data) {
  playlist_mrlident_toggle();
  gui_set_current_mmk(mediamark_get_current_mmk());
}

void playlist_get_input_focus(void) {
  if(playlist)
    xitk_window_try_to_set_input_focus(playlist->xwin);
}

void playlist_mmk_editor(void) {
  gGui_t *gui = gGui;
  if(playlist) {
    int sel = xitk_browser_get_current_selected(playlist->playlist);

    if(sel >= 0) {
      pthread_mutex_lock (&gui->mmk_mutex);
      mmk_edit_mediamark(&gui->playlist.mmk[sel], _playlist_apply_cb, NULL);
      pthread_mutex_unlock (&gui->mmk_mutex);
    }
  }
}

static void _scan_for_playlist_infos(xine_stream_t *stream, int n) {
  gGui_t *gui = gGui;
  
  pthread_mutex_lock (&gui->mmk_mutex);
  if(xine_open(stream, gui->playlist.mmk[n]->mrl)) {
    char  *ident;
    
    if((ident = stream_infos_get_ident_from_stream(stream)) != NULL) {
      
      free(gui->playlist.mmk[n]->ident);
      
      gui->playlist.mmk[n]->ident = strdup(ident);
      
      if(n == gui->playlist.cur) {
	
	free(gui->mmk.ident);
	
	gui->mmk.ident = strdup(ident);
	
        panel_update_mrl_display (gui->panel);
      }
      
      free(ident);
    }
    xine_close(stream);
  }
  pthread_mutex_unlock (&gui->mmk_mutex);
}

void playlist_scan_for_infos_selected(void) {
  gGui_t *gui = gGui;

  if(gui->playlist.num) {
    int                 selected = xitk_browser_get_current_selected(playlist->playlist);
    xine_stream_t      *stream;

    if (selected < 0) {
      return;
    }

    stream = xine_stream_new(__xineui_global_xine_instance, gui->ao_none, gui->vo_none);
    _scan_for_playlist_infos(stream, selected);
    
    xine_dispose(stream);
    
    playlist_mrlident_toggle();
  }
}

void playlist_scan_for_infos(void) {
  gGui_t *gui = gGui;
  
  if(gui->playlist.num) {
    int                 i;
    xine_stream_t      *stream;
    
    stream = xine_stream_new(__xineui_global_xine_instance, gui->ao_none, gui->vo_none);
    
    for(i = 0; i < gui->playlist.num; i++)
      _scan_for_playlist_infos(stream, i);
    
    xine_dispose(stream);
    
    playlist_mrlident_toggle();
  }
}

void playlist_show_tips(int enabled, unsigned long timeout) {
  
  if(playlist) {
    if(enabled)
      xitk_set_widgets_tips_timeout(playlist->widget_list, timeout);
    else
      xitk_disable_widgets_tips(playlist->widget_list);
  }
}

/*
void playlist_update_tips_timeout(unsigned long timeout) {
  if(playlist)
    xitk_set_widgets_tips_timeout(playlist->widget_list, timeout);
}
*/

void playlist_mrlident_toggle(void) {
  gGui_t *gui = gGui;

  if(playlist && playlist->visible) {
    int start = xitk_browser_get_current_start(playlist->playlist);
    
    _playlist_create_playlists();
    _playlist_update_browser_list(start);

    pthread_mutex_lock (&gui->mmk_mutex);
    mmkeditor_set_mmk(&gui->playlist.mmk[(xitk_browser_get_current_selected(playlist->playlist))]);
    pthread_mutex_unlock (&gui->mmk_mutex);
  }
}

/*
 *
 */
void playlist_update_playlist(void) {
  
  if(playlist) {
    _playlist_create_playlists();
    if(playlist_is_visible()) {
      _playlist_update_browser_list(0);
      mmk_editor_end();
    }
  }
}

/*
 * Leaving playlist editor
 */
void playlist_exit(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if(load_fb)
    filebrowser_end(load_fb);
  if(save_fb)
    filebrowser_end(save_fb);

  if(playlist) {
    window_info_t wi;
    
    mmk_editor_end();

    playlist->running = 0;
    playlist->visible = 0;

    if((xitk_get_window_info(playlist->widget_key, &wi))) {
      config_update_num ("gui.playlist_x", wi.x);
      config_update_num ("gui.playlist_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler(&playlist->widget_key);

    xitk_button_list_delete (playlist->autoplay_buttons);

    xitk_window_destroy_window(playlist->xwin);
    playlist->xwin = NULL;

    /* xitk_dlist_init (&playlist->widget_list->list); */

    _playlist_free_playlists();

    free(playlist);
    playlist = NULL;

    video_window_set_input_focus(gui->vwin);
  }
}


/*
 * return 1 if playlist editor is ON
 */
int playlist_is_running(void) {

  if(playlist != NULL)
    return playlist->running;
 
  return 0;
}

/*
 * Return 1 if playlist editor is visible
 */
int playlist_is_visible(void) {
  gGui_t *gui = gGui;

  if(playlist != NULL) {
    if(gui->use_root_window)
      return xitk_window_is_window_visible(playlist->xwin);
    else
      return playlist->visible && xitk_window_is_window_visible(playlist->xwin);
  }

  return 0;
}

/*
 * Handle autoplay buttons hitting (from panel and playlist windows)
 */
void playlist_scan_input(xitk_widget_t *w, void *ip) {
  gGui_t *gui = gGui;
  const char *name = xitk_labelbutton_get_label (w);
  int num_mrls = 0;
  const char * const * autoplay_mrls = xine_get_autoplay_mrls (__xineui_global_xine_instance, name, &num_mrls);

  if (autoplay_mrls) {
    xine_stream_t *stream;
    int j, cdda_mode = 0;

    if (!strcasecmp (name, "cd"))
      cdda_mode = 1;

    /* Flush playlist in newbie mode */
    if (gui->smart_mode) {
      mediamark_free_mediamarks ();
      playlist_update_playlist ();
      if (playlist != NULL)
        xitk_inputtext_change_text (playlist->winput, NULL);
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
    
    if (playlist) {
      _playlist_create_playlists ();
      _playlist_update_browser_list (0);
    }
    
    enable_playback_controls (gui->panel, (gui->playlist.num > 0));
  }
}

/*
 * Raise playlist->window
 */
void playlist_raise_window(void) {
  
  if(playlist != NULL) {
    raise_window(playlist->xwin, playlist->visible, playlist->running);
    mmk_editor_raise_window();
  }
}

/*
 * Hide/show the pl panel
 */
void playlist_toggle_visibility (xitk_widget_t *w, void *data) {

  if(playlist != NULL) {
    toggle_window(playlist->xwin, playlist->widget_list, &playlist->visible, playlist->running);
    mmk_editor_toggle_visibility();
  }
}

/*
 * Update first displayed entry and input text widget content with
 * current selected playlist entry.
 */
void playlist_update_focused_entry(void) {
  gGui_t *gui = gGui;

  if(playlist != NULL) {
    if (playlist->xwin) {
      if(playlist->visible && playlist->running && playlist->playlist_len) {
	char        *pa_mrl, *pl_mrl;
	mediamark_t *mmk;

        pthread_mutex_lock (&gui->mmk_mutex);
        mmk = mediamark_get_current_mmk ();
	if(mmk) {

	  pa_mrl = (gui->is_display_mrl) ? gui->mmk.mrl : gui->mmk.ident;
	  pl_mrl = (gui->is_display_mrl) ? mmk->mrl : mmk->ident;
	  
	  if(!strcmp(pa_mrl, pl_mrl)) {
	    int max_displayed = xitk_browser_get_num_entries(playlist->playlist);
	    int selected = xitk_browser_get_current_selected(playlist->playlist);
	    
	    if(gui->playlist.num <= max_displayed)
	      _playlist_update_browser_list(-1);
	    else if(gui->playlist.cur <= (gui->playlist.num - max_displayed))
	      _playlist_update_browser_list(gui->playlist.cur);
	    else
	      _playlist_update_browser_list(gui->playlist.num - max_displayed);
	    
	    if(selected >= 0) {
	      xitk_browser_set_select(playlist->playlist, selected);
 	      xitk_inputtext_change_text(playlist->winput,
 					 playlist->playlist_mrls[selected]);
	    }
	    else
	      xitk_inputtext_change_text(playlist->winput, 
					 playlist->playlist_mrls[gui->playlist.cur]);
	    
	  }
	}
	else
	  xitk_inputtext_change_text(playlist->winput, NULL);
        pthread_mutex_unlock (&gui->mmk_mutex);
      }
    }
  }
}

/*
 * Change the current skin.
 */
void playlist_change_skins(int synthetic) {
  gGui_t *gui = gGui;
  ImlibImage   *new_img;
  XSizeHints    hint;

  if(playlist_is_running()) {
    
    xitk_skin_lock(gui->skin_config);
    xitk_hide_widgets(playlist->widget_list);

    gui->x_lock_display (gui->display);
    
    if(!(new_img = Imlib_load_image(gui->imlib_data,
				    xitk_skin_get_skin_filename(gui->skin_config, "PlBG")))) {
      xine_error (gui, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
      exit(-1);
    }
    
    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PPosition | PSize;
    XSetWMNormalHints(gui->display, xitk_window_get_window(playlist->xwin), &hint);

    XResizeWindow (gui->display, xitk_window_get_window(playlist->xwin),
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);
    
    gui->x_unlock_display (gui->display);
    
    while(!xitk_is_window_size(gui->display, xitk_window_get_window(playlist->xwin),
			       new_img->rgb_width, new_img->rgb_height)) {
      xine_usec_sleep(10000);
    }

    video_window_set_transient_for (gui->vwin, playlist->xwin);

    gui->x_lock_display (gui->display);
    Imlib_apply_image(gui->imlib_data, new_img, xitk_window_get_window(playlist->xwin));
    Imlib_destroy_image(gui->imlib_data, new_img);
    gui->x_unlock_display (gui->display);

    if(playlist_is_visible())
      playlist_raise_window();

    xitk_skin_unlock(gui->skin_config);
    
    xitk_change_skins_widget_list(playlist->widget_list, gui->skin_config);
    
    xitk_button_list_new_skin (playlist->autoplay_buttons, gui->skin_config);
    
    xitk_paint_widget_list(playlist->widget_list);
  }
}

void playlist_deinit(void) {
  if(playlist) {
    if(playlist_is_visible())
      playlist_toggle_visibility(NULL, NULL);
    playlist_exit(NULL, NULL);
  }
  else {
    if(load_fb)
      filebrowser_end(load_fb);
    if(save_fb)
      filebrowser_end(save_fb);
  }
}

void playlist_reparent(void) {
  if (playlist && playlist->xwin)
    reparent_window(playlist->xwin);
}

/*
 * Create playlist editor window
 */
void playlist_editor(void) {
  gGui_t *gui = gGui;
  char                      *title = _("xine Playlist Editor");
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_inputtext_widget_t    inp;
  xitk_button_widget_t       b;
  int                        x, y;
  ImlibImage                *bg_image;

  XITK_WIDGET_INIT(&br, gui->imlib_data);
  XITK_WIDGET_INIT(&lb, gui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gui->imlib_data);
  XITK_WIDGET_INIT(&inp, gui->imlib_data);
  XITK_WIDGET_INIT(&b, gui->imlib_data);

  playlist = (_playlist_t *) calloc(1, sizeof(_playlist_t));

  playlist->playlist_len = 0;

  _playlist_create_playlists();

  gui->x_lock_display (gui->display);

  if (!(bg_image = Imlib_load_image(gui->imlib_data,
					      xitk_skin_get_skin_filename(gui->skin_config, "PlBG")))) {
    xine_error (gui, _("playlist: couldn't find image for background\n"));
    exit(-1);
  }
  gui->x_unlock_display (gui->display);

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

  playlist->xwin = xitk_window_create_simple_window(gui->imlib_data, x, y,
                                                    bg_image->rgb_width,
                                                    bg_image->rgb_height);
  xitk_window_set_window_title(playlist->xwin, title);

  set_window_states_start(playlist->xwin);

  if(is_layer_above())
    xitk_set_layer_above(xitk_window_get_window(playlist->xwin));

  gui->x_lock_display (gui->display);
  Imlib_apply_image(gui->imlib_data, bg_image, xitk_window_get_window(playlist->xwin));
  Imlib_destroy_image(gui->imlib_data, bg_image);
  gui->x_unlock_display (gui->display);

  /*
   * Widget-list
   */
  playlist->widget_list = xitk_window_widget_list(playlist->xwin);

  b.skin_element_name = "PlMoveUp";
  b.callback          = playlist_move_current_updown;
  b.userdata          = (void *)DIRECTION_UP;
  playlist->move_up =  xitk_button_create (playlist->widget_list, gui->skin_config, &b);
  xitk_add_widget (playlist->widget_list, playlist->move_up);
  xitk_set_widget_tips(playlist->move_up, _("Move up selected MRL"));

  b.skin_element_name = "PlMoveDn";
  b.callback          = playlist_move_current_updown;
  b.userdata          = (void *)DIRECTION_DOWN;
  playlist->move_down =  xitk_button_create (playlist->widget_list, gui->skin_config, &b);
  xitk_add_widget (playlist->widget_list, playlist->move_down);
  xitk_set_widget_tips(playlist->move_down, _("Move down selected MRL"));

  b.skin_element_name = "PlPlay";
  b.callback          = playlist_play_current;
  b.userdata          = NULL;
  playlist->play =  xitk_button_create (playlist->widget_list, gui->skin_config, &b);
  xitk_add_widget (playlist->widget_list, playlist->play);
  xitk_set_widget_tips(playlist->play, _("Start playback of selected MRL"));
  
  b.skin_element_name = "PlDelete";
  b.callback          = playlist_delete_current;
  b.userdata          = NULL;
  playlist->delete =  xitk_button_create (playlist->widget_list, gui->skin_config, &b);
  xitk_add_widget (playlist->widget_list, playlist->delete);
  xitk_set_widget_tips(playlist->delete, _("Delete selected MRL from playlist"));

  b.skin_element_name = "PlDeleteAll";
  b.callback          = playlist_delete_all;
  b.userdata          = NULL;
  playlist->delete_all =  xitk_button_create (playlist->widget_list, gui->skin_config, &b);
  xitk_add_widget (playlist->widget_list, playlist->delete_all);
  xitk_set_widget_tips(playlist->delete_all, _("Delete all entries in playlist"));

  lb.skin_element_name = "PlAdd";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Add");
  lb.align             = ALIGN_DEFAULT;
  lb.callback          = open_mrlbrowser_from_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = gui;
  playlist->add =  xitk_labelbutton_create (playlist->widget_list, gui->skin_config, &lb);
  xitk_add_widget (playlist->widget_list, playlist->add);
  xitk_set_widget_tips(playlist->add, _("Add one or more entries in playlist"));
    
  lb.skin_element_name = "PlLoad";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Load");
  lb.callback          = playlist_load_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  playlist->load =  xitk_labelbutton_create (playlist->widget_list, gui->skin_config, &lb);
  xitk_add_widget (playlist->widget_list, playlist->load);
  xitk_set_widget_tips(playlist->load, _("Load saved playlist"));

  lb.skin_element_name = "PlSave";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.callback          = playlist_save_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  playlist->save =  xitk_labelbutton_create (playlist->widget_list, gui->skin_config, &lb);
  xitk_add_widget (playlist->widget_list, playlist->save);
  xitk_set_widget_tips(playlist->save, _("Save playlist"));

  lb.skin_element_name = "PlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.callback          = playlist_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  playlist->close =  xitk_labelbutton_create (playlist->widget_list, gui->skin_config, &lb);
  xitk_add_widget (playlist->widget_list, playlist->close);
  xitk_set_widget_tips(playlist->close, _("Close playlist window"));

  br.arrow_up.skin_element_name    = "PlUp";
  br.slider.skin_element_name      = "SliderPl";
  br.arrow_dn.skin_element_name    = "PlDn";
  br.arrow_left.skin_element_name  = "PlLeft";
  br.slider_h.skin_element_name    = "SliderHPl";
  br.arrow_right.skin_element_name = "PlRight";
  br.browser.skin_element_name     = "PlItemBtn";
  br.browser.num_entries           = playlist->playlist_len;
  br.browser.entries               = (const char **)playlist->playlist_mrls;
  br.callback                      = _playlist_handle_selection;
  br.dbl_click_callback            = _playlist_play_on_dbl_click;
  br.parent_wlist                  = playlist->widget_list;
  playlist->playlist =  xitk_browser_create(playlist->widget_list, gui->skin_config, &br);
  xitk_add_widget (playlist->widget_list, playlist->playlist);

  lbl.skin_element_name = "AutoPlayLbl";
  /* TRANSLATORS: only ASCII characters (skin) */
  lbl.label             = pgettext("skin", "Scan for:");
  lbl.callback          = NULL;
  xitk_add_widget (playlist->widget_list, xitk_label_create (playlist->widget_list, gui->skin_config, &lbl));
  
  inp.skin_element_name = "PlInputText";
  inp.text              = NULL;
  inp.max_length        = 256;
  inp.callback          = _playlist_add_input;
  inp.userdata          = NULL;
  playlist->winput =  xitk_inputtext_create (playlist->widget_list, gui->skin_config, &inp);
  xitk_add_widget (playlist->widget_list, playlist->winput);
  xitk_set_widget_tips(playlist->winput, _("Direct MRL entry"));

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

    playlist->autoplay_buttons = xitk_button_list_new (
      gui->imlib_data, playlist->widget_list,
      gui->skin_config, "AutoPlayBG",
      playlist_scan_input, NULL,
      (char **)autoplay_plugins,
      tips, 5000, 0);
  } while (0);

  _playlist_update_browser_list(0);

  playlist_show_tips (panel_get_tips_enable (gui->panel), panel_get_tips_timeout (gui->panel));

  playlist->widget_key = 
    xitk_register_event_handler("playlist", 
                                xitk_window_get_window(playlist->xwin),
				_playlist_handle_event,
				NULL,
				gui_dndcallback,
				playlist->widget_list, (void*) playlist);

  playlist->visible = 1;
  playlist->running = 1;

  playlist_update_focused_entry();

  playlist_raise_window();

  xitk_window_try_to_set_input_focus(playlist->xwin);

  xitk_set_focus_to_widget(playlist->winput);
}
