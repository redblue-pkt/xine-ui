/* 
 * Copyright (C) 2000-2008 the xine project
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


typedef struct {
  Window                window;
  ImlibImage           *bg_image;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *playlist;
  xitk_widget_t        *winput;
  xitk_widget_t        *autoplay_plugins[64];

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

static void _playlist_enability(int enable) {
  void (*enability)(xitk_widget_t *) = (enable == 1) ? xitk_enable_widget : xitk_disable_widget;
  int    i = 0;
  
  if(playlist) {
    enability(playlist->playlist);
    enability(playlist->winput);
    while(playlist->autoplay_plugins[i])
      enability(playlist->autoplay_plugins[i++]);
    enability(playlist->move_up);
    enability(playlist->move_down);
    enability(playlist->play);
    enability(playlist->delete);
    enability(playlist->delete_all);
    enability(playlist->add);
    enability(playlist->load);
    enability(playlist->save);
    enability(playlist->close);
  }
}
static void playlist_deactivate(void) {
  _playlist_enability(0);
}
static void playlist_reactivate(void) {
  _playlist_enability(1);
}
/*
 *
 */

static void _playlist_update_browser_list(int start) {
  int sel = xitk_browser_get_current_selected(playlist->playlist);
  int old_start = xitk_browser_get_current_start(playlist->playlist);

  if(gGui->is_display_mrl)
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
  int i;

  _playlist_free_playlists();

  if(gGui->playlist.mmk && gGui->playlist.num) {

    playlist->playlist_mrls = (char **) calloc((gGui->playlist.num + 1), sizeof(char *));
    playlist->playlist_idents = (char **) calloc((gGui->playlist.num + 1), sizeof(char *));

    for(i = 0; i < gGui->playlist.num; i++) {
      playlist->playlist_mrls[i] = strdup(gGui->playlist.mmk[i]->mrl);
      playlist->playlist_idents[i] = strdup(gGui->playlist.mmk[i]->ident);
    }

    playlist->playlist_mrls[i] = NULL;
    playlist->playlist_idents[i] = NULL;
    playlist->playlist_len = gGui->playlist.num;
  }
}

/*
 *
 */
static void _playlist_handle_selection(xitk_widget_t *w, void *data, int selected) {

  if(playlist->playlist_mrls[selected] != NULL) {
    xitk_inputtext_change_text(playlist->winput, playlist->playlist_mrls[selected]);
    mmkeditor_set_mmk(&gGui->playlist.mmk[selected]);
  }

}

static void _playlist_xine_play(void) {

  if(!gui_playlist_play(gGui->playlist.cur)) {

    if(mediamark_all_played() && (gGui->actions_on_start[0] == ACTID_QUIT))
      gui_exit(NULL, NULL);
    else
      gui_display_logo();
  }
}
/*
 * Start playing an MRL
 */
void playlist_play_current(xitk_widget_t *w, void *data) {
  int j;
  
  j = xitk_browser_get_current_selected(playlist->playlist);

  if((j >= 0) && (gGui->playlist.mmk[j]->mrl != NULL)) {
    gGui->playlist.cur = j;
    _playlist_xine_play();
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}
/*
 * Start to play the selected stream on double click event in playlist.
 */
static void _playlist_play_on_dbl_click(xitk_widget_t *w, void *data, int selected) {
  
  if(gGui->playlist.mmk[selected]->mrl != NULL) {
    gGui->playlist.cur = selected;
    _playlist_xine_play();
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}

/*
 * Delete a given entry from playlist
 */
void playlist_delete_entry(int j) {
  int i;

  if(j  >= 0) {

    if((gGui->playlist.cur == j) && ((xine_get_status(gGui->stream) != XINE_STATUS_STOP)))
      gui_stop(NULL, NULL);

    mediamark_free_entry(j);

    for(i = j; i < gGui->playlist.num; i++)
      gGui->playlist.mmk[i] = gGui->playlist.mmk[i + 1];

    gGui->playlist.mmk = (mediamark_t **) realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 2));

    gGui->playlist.mmk[gGui->playlist.num] = NULL;


    playlist_update_playlist();

    if(gGui->playlist.num) {
      gui_set_current_mmk(mediamark_get_current_mmk());
      gGui->playlist.cur = 0;
    }
    else {

      gGui->playlist.cur = -1;

      if(is_playback_widgets_enabled())
	enable_playback_controls(0);

      if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
	gui_stop(NULL, NULL);

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

  mmk_editor_end();

  mediamark_free_mediamarks();
  playlist_update_playlist();
  
  if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
    gui_stop(NULL, NULL);
  
  if(playlist && playlist->winput)
    xitk_inputtext_change_text(playlist->winput, NULL);

  if(playlist)
    xitk_browser_release_all_buttons(playlist->playlist);

  gui_set_current_mmk(NULL);
  enable_playback_controls(0);
}

/*
 * Move entry up/down in playlist
 */
void playlist_move_current_updown(xitk_widget_t *w, void *data) {
  int j;

  mmk_editor_end();

  if((j = xitk_browser_get_current_selected(playlist->playlist)) >= 0) {
    mediamark_t *mmk;
    int          start = xitk_browser_get_current_start(playlist->playlist);
    int          max_vis_len = xitk_browser_get_num_entries(playlist->playlist);

    if(((intptr_t)data) == DIRECTION_UP && (j > 0)) {
      mmk = gGui->playlist.mmk[j - 1];
      
      if(j == gGui->playlist.cur)
	gGui->playlist.cur--;

      gGui->playlist.mmk[j - 1] = gGui->playlist.mmk[j];
      gGui->playlist.mmk[j] = mmk;
      j--;
    }
    else if(((intptr_t)data) == DIRECTION_DOWN && (j < (gGui->playlist.num - 1))) {
      mmk = gGui->playlist.mmk[j + 1];
      
      if(j == gGui->playlist.cur)
	gGui->playlist.cur++;

      gGui->playlist.mmk[j + 1] = gGui->playlist.mmk[j];
      gGui->playlist.mmk[j] = mmk;
      j++;
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
  char *file;

  mmk_editor_end();

  if((file = filebrowser_get_full_filename(fb)) != NULL) {
    mediamark_load_mediamarks(file);
    gui_set_current_mmk(mediamark_get_current_mmk());
    playlist_update_playlist();

    if((xine_get_status(gGui->stream) == XINE_STATUS_PLAY))
      gui_stop(NULL, NULL);

    if((!is_playback_widgets_enabled()) && gGui->playlist.num)
      enable_playback_controls(1);

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
    asprintf(&buffer, "%s%s", xine_get_homedir(), "/.xine/playlist.tox");
    
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
  filebrowser_callback_button_t  cbb[2];
  char                          *buffer;

  if(gGui->playlist.num) {
    if(save_fb)
      filebrowser_raise_window(save_fb);
    else {
      asprintf(&buffer, "%s%s", xine_get_homedir(), "/.xine/playlist.tox");
      
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
static void _playlist_add_input(xitk_widget_t *w, void *data, char *filename) {

  if(filename)
    gui_dndcallback((char *)filename);

}

/*
 * Handle X events here.
 */
static void _playlist_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;

    if(bevent->button == Button3) {
      xitk_widget_t *w = xitk_get_focused_widget(playlist->widget_list);
      
      if(w && ((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)) {
	int wx, wy;

	xitk_get_window_position(gGui->display, playlist->window, &wx, &wy, NULL, NULL);

	playlist_menu(playlist->widget_list, 
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

    gui_handle_event(event, data);
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
	gui_handle_event(event, data);
	break;
      }
    }
    break;

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;
    
  }
}

static void _playlist_apply_cb(void *data) {
  playlist_mrlident_toggle();
  gui_set_current_mmk(mediamark_get_current_mmk());
}

void playlist_get_input_focus(void) {
  if(playlist)
    try_to_set_input_focus(playlist->window);
}

void playlist_mmk_editor(void) {
  if(playlist) {
    int sel = xitk_browser_get_current_selected(playlist->playlist);

    if(sel >= 0)
      mmk_edit_mediamark(&gGui->playlist.mmk[sel], _playlist_apply_cb, NULL);
  
  }
}

static void _scan_for_playlist_infos(xine_stream_t *stream, int n) {
  
  if(xine_open(stream, gGui->playlist.mmk[n]->mrl)) {
    char  *ident;
    
    if((ident = stream_infos_get_ident_from_stream(stream)) != NULL) {
      
      free(gGui->playlist.mmk[n]->ident);
      
      gGui->playlist.mmk[n]->ident = strdup(ident);
      
      if(n == gGui->playlist.cur) {
	
	free(gGui->mmk.ident);
	
	gGui->mmk.ident = strdup(ident);
	
	panel_update_mrl_display();
      }
      
      free(ident);
    }
    xine_close(stream);
  }
}

void playlist_scan_for_infos_selected(void) {

  if(gGui->playlist.num) {
    int                 selected = xitk_browser_get_current_selected(playlist->playlist);
    xine_stream_t      *stream;
    
    stream = xine_stream_new(__xineui_global_xine_instance, gGui->ao_none, gGui->vo_none);
    _scan_for_playlist_infos(stream, selected);
    
    xine_dispose(stream);
    
    playlist_mrlident_toggle();
  }
}

void playlist_scan_for_infos(void) {
  
  if(gGui->playlist.num) {
    int                 i;
    xine_stream_t      *stream;
    
    stream = xine_stream_new(__xineui_global_xine_instance, gGui->ao_none, gGui->vo_none);
    
    for(i = 0; i < gGui->playlist.num; i++)
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

void playlist_update_tips_timeout(unsigned long timeout) {
  if(playlist)
    xitk_set_widgets_tips_timeout(playlist->widget_list, timeout);
}

void playlist_mrlident_toggle(void) {

  if(playlist && playlist->visible) {
    int start = xitk_browser_get_current_start(playlist->playlist);
    
    _playlist_create_playlists();
    _playlist_update_browser_list(start);

    mmkeditor_set_mmk(&gGui->playlist.mmk[(xitk_browser_get_current_selected(playlist->playlist))]);
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

    XLockDisplay(gGui->display);
    XUnmapWindow(gGui->display, playlist->window);
    XUnlockDisplay(gGui->display);

    xitk_destroy_widgets(playlist->widget_list);

    XLockDisplay(gGui->display);
    XDestroyWindow(gGui->display, playlist->window);
    Imlib_destroy_image(gGui->imlib_data, playlist->bg_image);
    XUnlockDisplay(gGui->display);

    playlist->window = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(playlist->widget_list)));

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(playlist->widget_list)));
    XUnlockDisplay(gGui->display);

    _playlist_free_playlists();
    
    XITK_WIDGET_LIST_FREE(playlist->widget_list);
    
    free(playlist);
    playlist = NULL;

    try_to_set_input_focus(gGui->video_window);
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

  if(playlist != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, playlist->window);
    else
      return playlist->visible && xitk_is_window_visible(gGui->display, playlist->window);
  }

  return 0;
}

/*
 * Handle autoplay buttons hitting (from panel and playlist windows)
 */
void playlist_scan_input(xitk_widget_t *w, void *ip) {
  const char *const *autoplay_plugins = xine_get_autoplay_input_plugin_ids(__xineui_global_xine_instance);
  int                i = 0;
  
  if(autoplay_plugins) {
    while(autoplay_plugins[i] != NULL) {
      
      if(!strcasecmp(autoplay_plugins[i], xitk_labelbutton_get_label(w))) {
	int                num_mrls = 0;
	char             **autoplay_mrls = xine_get_autoplay_mrls(__xineui_global_xine_instance, autoplay_plugins[i], &num_mrls);
	xine_stream_t      *stream;
	
	if(autoplay_mrls) {
	  int                j;
	  int                cdda_mode = 0;

	  if(!strcasecmp(autoplay_plugins[i], "cd"))
	    cdda_mode = 1;
	  
	  /* Flush playlist in newbie mode */
	  if(gGui->smart_mode) {
	    mediamark_free_mediamarks();
	    playlist_update_playlist();
	    if(playlist != NULL)
	      xitk_inputtext_change_text(playlist->winput, NULL);
	  }
	  
	  stream = xine_stream_new(__xineui_global_xine_instance, gGui->ao_none, gGui->vo_none);
	  
	  for (j = 0; j < num_mrls; j++) {
	    char *ident = NULL;
	    
	    if(cdda_mode && xine_open(stream, autoplay_mrls[j])) {
	      ident = stream_infos_get_ident_from_stream(stream);
	      xine_close(stream);
	    }
	    
	    mediamark_append_entry(autoplay_mrls[j], ident ? ident : autoplay_mrls[j], NULL, 0, -1, 0, 0);
	    
	    free(ident);
	  }

	  xine_dispose(stream);
	  
	  gGui->playlist.cur = gGui->playlist.num ? 0 : -1;
	  
	  if(gGui->playlist.cur == 0)
	    gui_set_current_mmk(mediamark_get_current_mmk());
	  
	  /* 
	   * If we're in newbie mode, start playback immediately
	   * (even ignoring if we're currently playing something
	   */
	  if(gGui->smart_mode) {
	    if(xine_get_status(gGui->stream) == XINE_STATUS_PLAY)
	      gui_stop(NULL, NULL);
	    gui_play(NULL, NULL);
	  }
	}
      }
      
      i++;
    }
    
    if(playlist) {
      _playlist_create_playlists();
      _playlist_update_browser_list(0);
    }
    
    enable_playback_controls((gGui->playlist.num > 0));
  }
  
}

/*
 * Raise playlist->window
 */
void playlist_raise_window(void) {
  
  if(playlist != NULL) {
    raise_window(playlist->window, playlist->visible, playlist->running);
    mmk_editor_raise_window();
  }
}

/*
 * Hide/show the pl panel
 */
void playlist_toggle_visibility (xitk_widget_t *w, void *data) {

  if(playlist != NULL) {
    toggle_window(playlist->window, playlist->widget_list, 
		  &playlist->visible, playlist->running);
    mmk_editor_toggle_visibility();
  }
}

/*
 * Update first displayed entry and input text widget content with
 * current selected playlist entry.
 */
void playlist_update_focused_entry(void) {

  if(playlist != NULL) {
    if(playlist->window) {
      if(playlist->visible && playlist->running && playlist->playlist_len) {
	char        *pa_mrl, *pl_mrl;
	mediamark_t *mmk = mediamark_get_current_mmk();
	
	if(mmk) {

	  pa_mrl = (gGui->is_display_mrl) ? gGui->mmk.mrl : gGui->mmk.ident;
	  pl_mrl = (gGui->is_display_mrl) ? mmk->mrl : mmk->ident;
	  
	  if(!strcmp(pa_mrl, pl_mrl)) {
	    int max_displayed = xitk_browser_get_num_entries(playlist->playlist);
	    int selected = xitk_browser_get_current_selected(playlist->playlist);
	    
	    if(gGui->playlist.num <= max_displayed)
	      _playlist_update_browser_list(-1);
	    else if(gGui->playlist.cur <= (gGui->playlist.num - max_displayed))
	      _playlist_update_browser_list(gGui->playlist.cur);
	    else
	      _playlist_update_browser_list(gGui->playlist.num - max_displayed);
	    
	    if(selected >= 0) {
	      xitk_browser_set_select(playlist->playlist, selected);
 	      xitk_inputtext_change_text(playlist->winput,
 					 playlist->playlist_mrls[selected]);
	    }
	    else
	      xitk_inputtext_change_text(playlist->winput, 
					 playlist->playlist_mrls[gGui->playlist.cur]);
	    
	  }
	  
	}
	else
	  xitk_inputtext_change_text(playlist->winput, NULL);

      }
    }
  }
}

/*
 * Change the current skin.
 */
void playlist_change_skins(int synthetic) {
  ImlibImage   *new_img, *old_img;
  XSizeHints    hint;

  if(playlist_is_running()) {
    
    xitk_skin_lock(gGui->skin_config);
    xitk_hide_widgets(playlist->widget_list);

    XLockDisplay(gGui->display);
    
    if(!(new_img = Imlib_load_image(gGui->imlib_data,
				    xitk_skin_get_skin_filename(gGui->skin_config, "PlBG")))) {
      xine_error(_("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
      exit(-1);
    }
    
    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PPosition | PSize;
    XSetWMNormalHints(gGui->display, playlist->window, &hint);

    XResizeWindow (gGui->display, playlist->window,
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);
    
    XUnlockDisplay(gGui->display);
    
    while(!xitk_is_window_size(gGui->display, playlist->window, 
			       new_img->rgb_width, new_img->rgb_height)) {
      xine_usec_sleep(10000);
    }
    
    old_img = playlist->bg_image;
    playlist->bg_image = new_img;

    XLockDisplay(gGui->display);
    
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, playlist->window, gGui->video_window);
    
    Imlib_destroy_image(gGui->imlib_data, old_img);
    Imlib_apply_image(gGui->imlib_data, new_img, playlist->window);
    XUnlockDisplay(gGui->display);

    if(playlist_is_visible())
      playlist_raise_window();

    xitk_skin_unlock(gGui->skin_config);
    
    xitk_change_skins_widget_list(playlist->widget_list, gGui->skin_config);
    
    {
      int x, y, dir;
      int i = 0, max;
      
      x   = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
      y   = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
      dir = xitk_skin_get_direction(gGui->skin_config, "AutoPlayBG");
      max = xitk_skin_get_max_buttons(gGui->skin_config, "AutoPlayBG");

      switch(dir) {
      case DIRECTION_UP:
      case DIRECTION_DOWN:
	while((max && ((i < max) && (playlist->autoplay_plugins[i] != NULL))) || (!max && playlist->autoplay_plugins[i] != NULL)) {
	  
	  (void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);
	  
	  if(dir == DIRECTION_DOWN)
	    y += xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1;
	  else
	    y -= (xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1);
	  
	  i++;
	}
	break;
      case DIRECTION_LEFT:
      case DIRECTION_RIGHT:
	while((max && ((i < max) && (playlist->autoplay_plugins[i] != NULL))) || (!max && playlist->autoplay_plugins[i] != NULL)) {
	  
	  (void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);
	  
	  if(dir == DIRECTION_RIGHT)
	    x += xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1;
	  else
	    x -= (xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1);
	  
	  i++;
	}
	break;
      }

      if(max) {
	while(playlist->autoplay_plugins[i] != NULL) {
	  xitk_disable_and_hide_widget(playlist->autoplay_plugins[i]);
	  i++;
	}
      }
    }
    
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
  if(playlist)
    reparent_window(playlist->window);
}

/*
 * Create playlist editor window
 */
void playlist_editor(void) {
  GC                         gc;
  XSizeHints                 hint;
  XWMHints                  *wm_hint;
  XSetWindowAttributes       attr;
  char                      *title = _("xine Playlist Editor");
  Atom                       prop;
  MWMHints                   mwmhints;
  XClassHint                *xclasshint;
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_inputtext_widget_t    inp;
  xitk_button_widget_t       b;

  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  XITK_WIDGET_INIT(&b, gGui->imlib_data);

  playlist = (_playlist_t *) calloc(1, sizeof(_playlist_t));

  playlist->playlist_len = 0;

  _playlist_create_playlists();

  XLockDisplay (gGui->display);

  if (!(playlist->bg_image = Imlib_load_image(gGui->imlib_data,
					      xitk_skin_get_skin_filename(gGui->skin_config, "PlBG")))) {
    xine_error(_("playlist: couldn't find image for background\n"));
    exit(-1);
  }

  hint.x = xine_config_register_num (__xineui_global_xine_instance, "gui.playlist_x",
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);
  hint.y = xine_config_register_num (__xineui_global_xine_instance, "gui.playlist_y", 
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);

  hint.width = playlist->bg_image->rgb_width;
  hint.height = playlist->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = False;
  attr.background_pixel  = gGui->black.pixel;
  /*
   * XXX:multivis
   * To avoid BadMatch errors on XCreateWindow:
   * If the parent and the new window have different depths, we must supply either
   * a BorderPixmap or a BorderPixel.
   * If the parent and the new window use different visuals, we must supply a
   * Colormap
   */
  attr.border_pixel      = 1;
  attr.colormap		 = Imlib_get_colormap(gGui->imlib_data);

  playlist->window = XCreateWindow (gGui->display, 
				    gGui->imlib_data->x.root, 
				    hint.x, hint.y, hint.width, 
				    hint.height, 0, 
				    gGui->imlib_data->x.depth, InputOutput, 
				    gGui->imlib_data->x.visual,
				    CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
				    &attr);
  
  XmbSetWMProperties(gGui->display, playlist->window, title, title, NULL, 0,
                     &hint, NULL, NULL);
  
  XSelectInput(gGui->display, playlist->window, INPUT_MOTION | KeymapStateMask);
  XUnlockDisplay (gGui->display);
  
  if(!video_window_is_visible())
    xitk_set_wm_window_type(playlist->window, WINDOW_TYPE_NORMAL);
  else
    xitk_unset_wm_window_type(playlist->window, WINDOW_TYPE_NORMAL);

  if(is_layer_above())
    xitk_set_layer_above(playlist->window);

  /*
   * wm, no border please
   */

  XLockDisplay (gGui->display);
  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, playlist->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = title;
    xclasshint->res_class = "xine";
    XSetClassHint(gGui->display, playlist->window, xclasshint);
    XFree(xclasshint);
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap   = gGui->icon;
    wm_hint->flags = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->display, playlist->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(gGui->display, playlist->window, 0, 0);
  
  Imlib_apply_image(gGui->imlib_data, playlist->bg_image, playlist->window);

  XUnlockDisplay (gGui->display);

  /*
   * Widget-list
   */
  playlist->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(playlist->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(playlist->widget_list, WIDGET_LIST_WINDOW, (void *) playlist->window);
  xitk_widget_list_set(playlist->widget_list, WIDGET_LIST_GC, gc);

  lbl.window          = (XITK_WIDGET_LIST_WINDOW(playlist->widget_list));
  lbl.gc              = (XITK_WIDGET_LIST_GC(playlist->widget_list));

  b.skin_element_name = "PlMoveUp";
  b.callback          = playlist_move_current_updown;
  b.userdata          = (void *)DIRECTION_UP;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->move_up = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(playlist->move_up, _("Move up selected MRL"));

  b.skin_element_name = "PlMoveDn";
  b.callback          = playlist_move_current_updown;
  b.userdata          = (void *)DIRECTION_DOWN;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->move_down = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(playlist->move_down, _("Move down selected MRL"));

  b.skin_element_name = "PlPlay";
  b.callback          = playlist_play_current;
  b.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->play = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(playlist->play, _("Start playback of selected MRL"));
  
  b.skin_element_name = "PlDelete";
  b.callback          = playlist_delete_current;
  b.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->delete = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(playlist->delete, _("Delete selected MRL from playlist"));

  b.skin_element_name = "PlDeleteAll";
  b.callback          = playlist_delete_all;
  b.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->delete_all = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(playlist->delete_all, _("Delete all entries in playlist"));

  lb.skin_element_name = "PlAdd";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Add");
  lb.callback          = open_mrlbrowser_from_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->add = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(playlist->add, _("Add one or more entries in playlist"));
    
  lb.skin_element_name = "PlLoad";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Load");
  lb.callback          = playlist_load_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->load = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(playlist->load, _("Load saved playlist"));

  lb.skin_element_name = "PlSave";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.callback          = playlist_save_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->save = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(playlist->save, _("Save playlist"));

  lb.skin_element_name = "PlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.callback          = playlist_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->close = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
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
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)), 
    (playlist->playlist = xitk_browser_create(playlist->widget_list, gGui->skin_config, &br)));

  lbl.skin_element_name = "AutoPlayLbl";
  lbl.label             = _("Scan for:");
  lbl.callback          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)),
			    xitk_label_create (playlist->widget_list, gGui->skin_config, &lbl));
  
  inp.skin_element_name = "PlInputText";
  inp.text              = NULL;
  inp.max_length        = 256;
  inp.callback          = _playlist_add_input;
  inp.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)),
   (playlist->winput = xitk_inputtext_create (playlist->widget_list, gGui->skin_config, &inp)));
  xitk_set_widget_tips(playlist->winput, _("Direct MRL entry"));

  {
    int                x, y, dir;
    int                i = 0, max;
    const char *const *autoplay_plugins = xine_get_autoplay_input_plugin_ids(__xineui_global_xine_instance);
    
    x   = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
    y   = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
    dir = xitk_skin_get_direction(gGui->skin_config, "AutoPlayBG");
    max = xitk_skin_get_max_buttons(gGui->skin_config, "AutoPlayBG");

    while(autoplay_plugins[i] != NULL) {

      lb.skin_element_name = "AutoPlayBG";
      lb.button_type       = CLICK_BUTTON;
      lb.label             = autoplay_plugins[i];
      lb.callback          = playlist_scan_input;
      lb.state_callback    = NULL;
      lb.userdata          = NULL;
      xitk_list_append_content ((XITK_WIDGET_LIST_LIST(playlist->widget_list)),
	       (playlist->autoplay_plugins[i] = 
		xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
      xitk_set_widget_tips(playlist->autoplay_plugins[i], 
		   (char *) xine_get_input_plugin_description(__xineui_global_xine_instance, (char *)autoplay_plugins[i]));
      
      (void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);
      
      switch(dir) {
      case DIRECTION_UP:
      case DIRECTION_DOWN:
	if(dir == DIRECTION_DOWN)
	  y += xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1;
	else
	  y -= (xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1);
	break;
      case DIRECTION_LEFT:
      case DIRECTION_RIGHT:
	if(dir == DIRECTION_RIGHT)
	  x += xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1;
	else
	  x -= (xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1);
	break;
      }

      if(max && (i >= max))
	xitk_disable_and_hide_widget(playlist->autoplay_plugins[i]);
      
      i++;

    }
    if(i)
      playlist->autoplay_plugins[i+1] = NULL;

  }
  _playlist_update_browser_list(0);

  playlist_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());

  playlist->widget_key = 
    xitk_register_event_handler("playlist", 
				playlist->window, 
				_playlist_handle_event,
				NULL,
				gui_dndcallback,
				playlist->widget_list, (void*) playlist);

  playlist->visible = 1;
  playlist->running = 1;

  playlist_update_focused_entry();

  playlist_raise_window();

  try_to_set_input_focus(playlist->window);

  xitk_set_focus_to_widget(playlist->winput);
}
