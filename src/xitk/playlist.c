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

extern gGui_t          *gGui;

typedef struct {
  Window                window;
  xitk_widget_t        *playlist;
  ImlibImage           *bg_image;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *winput;
  xitk_widget_t        *autoplay_plugins[64];
  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

  char                **playlist_mrls;
  char                **playlist_idents;
  int                   playlist_len;
} _playlist_t;

static _playlist_t   *playlist;

#define MOVEUP 1
#define MOVEDN 2

void playlist_handle_event(XEvent *event, void *data);

/*
 *
 */

static void _playlist_update_browser_list(int start) {
  int sel = xitk_browser_get_current_selected(playlist->playlist);

  if(gGui->is_display_mrl)
    xitk_browser_update_list(playlist->playlist, 
			     (const char **)playlist->playlist_mrls, 
			     playlist->playlist_len, start);
  else
    xitk_browser_update_list(playlist->playlist, 
			     (const char **)playlist->playlist_idents, 
			     playlist->playlist_len, start);

  if((sel >= 0) && (start == -1))
    xitk_browser_set_select(playlist->playlist, sel);

}


static void _playlist_free_playlists(void) {

  if(playlist->playlist_len) {

    while(playlist->playlist_len >= 0) {
      free(playlist->playlist_mrls[playlist->playlist_len]);
      free(playlist->playlist_idents[playlist->playlist_len]);
      playlist->playlist_len--;
    }
    
    free(playlist->playlist_mrls);
    free(playlist->playlist_idents);
    playlist->playlist_len = 0;
  }
}

static void _playlist_create_playlists(void) {
  int i;

  _playlist_free_playlists();

  if(gGui->playlist.mmk && gGui->playlist.num) {

    playlist->playlist_mrls = (char **) xine_xmalloc(sizeof(char *) * (gGui->playlist.num + 1));
    playlist->playlist_idents = (char **) xine_xmalloc(sizeof(char *) * (gGui->playlist.num + 1));

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
static void _playlist_handle_selection(xitk_widget_t *w, void *data) {
  int selected = (int)data;

  if(playlist->playlist_mrls[selected] != NULL) {
    xitk_inputtext_change_text(playlist->widget_list, 
			       playlist->winput, playlist->playlist_mrls[selected]);
    mmkeditor_set_mmk(&gGui->playlist.mmk[selected]);
  }

}

/*
 * Start playing an MRL
 */
static void _playlist_play(xitk_widget_t *w, void *data) {
  int j;
  
  j = xitk_browser_get_current_selected(playlist->playlist);

  if((j >= 0) && (gGui->playlist.mmk[j]->mrl != NULL)) {
    
    gui_set_current_mrl(gGui->playlist.mmk[j]);
    
    if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
      gui_stop(NULL, NULL);
    
    gGui->playlist.cur = j;
    
    if(!is_playback_widgets_enabled())
      enable_playback_controls(1);

    gui_play(NULL, NULL);
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}
/*
 * Start to play the selected stream on double click event in playlist.
 */
static void _playlist_play_on_dbl_click(xitk_widget_t *w, void *data, int selected) {

  if(gGui->playlist.mmk[selected]->mrl != NULL) {
    
    gui_set_current_mrl(gGui->playlist.mmk[selected]);

    if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
      gui_stop(NULL, NULL);
    
    gGui->playlist.cur = selected;
    
    if(!is_playback_widgets_enabled())
      enable_playback_controls(1);

    gui_play(NULL, NULL);
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}

/*
 * Delete selected MRL
 */
static void _playlist_delete(xitk_widget_t *w, void *data) {
  int i, j;
  
  if((j = xitk_browser_get_current_selected(playlist->playlist)) >= 0) {
    
    if((gGui->playlist.cur == j) && ((xine_get_status(gGui->stream) != XINE_STATUS_STOP)))
      gui_stop(NULL, NULL);
    
    mediamark_free_entry(j);
    
    for(i = j; i < gGui->playlist.num; i++)
      gGui->playlist.mmk[i] = gGui->playlist.mmk[i + 1];
    
    gGui->playlist.mmk = (mediamark_t **) 
      realloc(gGui->playlist.mmk, sizeof(mediamark_t *) * (gGui->playlist.num + 2));
    
    gGui->playlist.mmk[gGui->playlist.num] = NULL;
    
    gGui->playlist.cur = 0;
    
    playlist_update_playlist();
    
    if(gGui->playlist.num)
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
    else {
      
      if(is_playback_widgets_enabled())
	enable_playback_controls(0);
      
      if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
	gui_stop(NULL, NULL);
      
      gui_set_current_mrl(NULL);
      xitk_inputtext_change_text(playlist->widget_list, 
				 playlist->winput, NULL);
    }
  }

}

/*
 * Delete all MRLs
 */
static void _playlist_delete_all(xitk_widget_t *w, void *data) {

  mediamark_free_mediamarks();
  playlist_update_playlist();
  
  if(xine_get_status(gGui->stream) != XINE_STATUS_STOP)
    gui_stop(NULL, NULL);

  xitk_inputtext_change_text(playlist->widget_list, 
			     playlist->winput, NULL);
  gui_set_current_mrl(NULL);
  enable_playback_controls(0);
}

/*
 * Move entry up/down in playlist
 */
static void _playlist_move_updown(xitk_widget_t *w, void *data) {
  int j;

  if((j = xitk_browser_get_current_selected(playlist->playlist)) >= 0) {
    mediamark_t *mmk;
    int          start = xitk_browser_get_current_start(playlist->playlist);
    int          max_vis_len = xitk_browser_get_num_entries(playlist->playlist);

    if(((int)data) == MOVEUP && (j > 0)) {
      mmk = gGui->playlist.mmk[j - 1];

      gGui->playlist.mmk[j - 1] = gGui->playlist.mmk[j];
      gGui->playlist.mmk[j] = mmk;
      j--;
    }
    else if(((int)data) == MOVEDN && (j < (gGui->playlist.num - 1))) {
      mmk = gGui->playlist.mmk[j + 1];
      
      gGui->playlist.mmk[j + 1] = gGui->playlist.mmk[j];
      gGui->playlist.mmk[j] = mmk;
      j++;
    }

    _playlist_create_playlists();

    if(j < start) {
      _playlist_update_browser_list(j);
      xitk_browser_set_select(playlist->playlist, 0);
    }
    else if(j >= (start + max_vis_len)) {
      _playlist_update_browser_list(start + 1);
      xitk_browser_set_select(playlist->playlist, max_vis_len - 1);
    }
    else {
      _playlist_update_browser_list(-1);
      xitk_browser_set_select(playlist->playlist, j - start);
    }
  }
}

/*
 * Load playlist from $HOME/.xine/playlist
 */
static void _playlist_load_playlist(xitk_widget_t *w, void *data) {
  char buffer[_PATH_MAX + _NAME_MAX + 1];

  memset(&buffer, 0, sizeof(buffer));
  sprintf(buffer, "%s/.xine/playlist", xine_get_homedir());
  mediamark_load_mediamarks(buffer);
  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
  playlist_update_playlist();
}

/*
 * Save playlist to $HOME/.xine/playlist
 */
static void _playlist_save_playlist(xitk_widget_t *w, void *data) {
  char buffer[_PATH_MAX + _NAME_MAX + 1];

  memset(&buffer, 0, sizeof(buffer));
  sprintf(buffer, "%s/.xine/playlist", xine_get_homedir());
  mediamark_save_mediamarks(buffer);
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
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;

  switch(event->type) {

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    if (bevent->button == Button4) {
      xitk_browser_step_down(playlist->playlist, NULL);
      mmk_editor_end();
    }
    else if(bevent->button == Button5) {
      xitk_browser_step_up(playlist->playlist, NULL);
      mmk_editor_end();
    }
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
    
    if(!playlist)
      return;
    
    mykeyevent = event->xkey;
    
    XLockDisplay(gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay(gGui->display);
    
    switch (mykey) {
      
    case XK_Down:
    case XK_Next: {
      xitk_widget_t *w;
      
      mmk_editor_end();
      
      w = xitk_get_focused_widget(playlist->widget_list);
      if((!w) || (w && (!(w->widget_type & WIDGET_GROUP_BROWSER)))) {
	if(mykey == XK_Down)
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
      if((!w) || (w && (!(w->widget_type & WIDGET_GROUP_BROWSER)))) {
	if(mykey == XK_Up)
	  xitk_browser_step_down(playlist->playlist, NULL);
	else
	  xitk_browser_page_down(playlist->playlist, NULL);
      }
    }
      break;
      
    default:
      gui_handle_event(event, data);
      break;
    }
    break;

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;
    
  }
}

/* End of privates */
void mediamark_editor_change_cb(const char *mrl, const char *ident, int start, int end) {
  if(playlist) {
    int sel = xitk_browser_get_current_selected(playlist->playlist);

    if(sel >= 0) {
      mediamark_replace_entry(&gGui->playlist.mmk[sel], mrl, ident, start, end);
    }

  }
}


static void _playlist_apply_cb(void *data) {
  playlist_mrlident_toggle();
  gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
}

void playlist_mmk_editor(void) {
  if(playlist) {
    int sel = xitk_browser_get_current_selected(playlist->playlist);

    if(sel >= 0) {
      mmk_edit_mediamark(&gGui->playlist.mmk[sel], _playlist_apply_cb, NULL);
    }
  
  }
}

void playlist_scan_for_infos(void) {
  
  if(gGui->playlist.num) {
    int            i;
    char          *ident;
    int            old_pos = 0, rerun = 0;
    xine_stream_t *stream = gGui->playlist.scan_stream;
    int            mute = gGui->mixer.mute;
    
    /* 
     * Because we share audio driver (and since we can't stat infos of
     * audio only streams without audio driver), pausing the gGui->stream
     * avoid some sync/crash problems.
     */
    if(((xine_get_status(gGui->stream)) != XINE_STATUS_STOP) && (gGui->logo_mode == 0)) {
      xine_get_pos_length(gGui->stream, &old_pos, NULL, NULL);
      gui_stop(NULL, NULL);
      rerun = 1;
    }
    if((!mute) && gGui->mixer.caps & MIXER_CAP_MUTE) {
      mute = 1;
      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_MUTE, 1);
    }
    else
      mute = 0;
    
    for(i = 0; i < gGui->playlist.num; i++) {
      
      if((xine_open(stream, gGui->playlist.mmk[i]->mrl)) && (xine_play(stream, 0, 0))) {
	
	xine_usec_sleep(5000);

	if((ident = stream_infos_get_ident_from_stream(stream)) != NULL) {
	  
	  if(gGui->playlist.mmk[i]->ident)
	    free(gGui->playlist.mmk[i]->ident);
	  
	  gGui->playlist.mmk[i]->ident = strdup(ident);
	  
	  if(i == gGui->playlist.cur) {
	    
	    if(gGui->mmk.ident)
	      free(gGui->mmk.ident);
	    
	    gGui->mmk.ident = strdup(ident);
	    
	    panel_update_mrl_display();
	  }
	  
	  free(ident);
	}
      }
    }
    
    /* Restoring previous play status */
    if(mute)
      xine_set_param(gGui->stream, XINE_PARAM_AUDIO_MUTE, !mute);
    
    if(rerun)
      gui_xine_open_and_play(gGui->mmk.mrl, old_pos, 0);
    else
      xine_stop(stream);
    
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

void playlist_mrlident_toggle(void) {

  if(playlist && playlist->visible) {
    _playlist_create_playlists();
    _playlist_update_browser_list(-1);
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
  window_info_t wi;

  if(playlist) {
    
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
    xitk_list_free(playlist->widget_list->l);

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, playlist->widget_list->gc);
    XUnlockDisplay(gGui->display);

    _playlist_free_playlists();
    
    free(playlist->widget_list);
    
    free(playlist);
    playlist = NULL;  
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

  if(playlist != NULL)
    return playlist->visible;

  return 0;
}

/*
 * Handle autoplay buttons hitting (from panel and playlist windows)
 */
void playlist_scan_input(xitk_widget_t *w, void *ip) {
  
  if(xine_get_status(gGui->stream) == XINE_STATUS_STOP) {
    const char *const *autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    int                i = 0;
    
    if(autoplay_plugins) {
      while(autoplay_plugins[i] != NULL) {
	
	if(!strcasecmp(autoplay_plugins[i], xitk_labelbutton_get_label(w))) {
	  int                num_mrls;
	  char             **autoplay_mrls = 
	    xine_get_autoplay_mrls (gGui->xine, autoplay_plugins[i], &num_mrls);
	  
	  if(autoplay_mrls) {
	    int j;
	    
	    for (j = 0; j < num_mrls; j++)
	      mediamark_add_entry(autoplay_mrls[j], autoplay_mrls[j], 0, -1);
	    
	    gGui->playlist.cur = 0;
	    
	    gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	  }
	}
	
	i++;
      }
      
      if(playlist) {
	_playlist_create_playlists();
	_playlist_update_browser_list(0);
      }

    }

    enable_playback_controls((gGui->playlist.num > 0));
  }
}

/*
 * Raise playlist->window
 */
void playlist_raise_window(void) {
  
  if(playlist != NULL) {
    if(playlist->window) {
      if(playlist->visible && playlist->running) {
	XLockDisplay(gGui->display);
	XUnmapWindow(gGui->display, playlist->window); 
	XRaiseWindow(gGui->display, playlist->window); 
	XMapWindow(gGui->display, playlist->window); 
	XSetTransientForHint (gGui->display, 
			      playlist->window, gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(playlist->window);
      }
    }
    mmk_editor_raise_window();
  }
}

/*
 * Hide/show the pl panel
 */
void playlist_toggle_visibility (xitk_widget_t *w, void *data) {
  
  if(playlist != NULL) {
    if (playlist->visible && playlist->running) {
      playlist->visible = 0;
      xitk_hide_widgets(playlist->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, playlist->window);
      XUnlockDisplay(gGui->display);
    } else {
      if(playlist->running) {
	playlist->visible = 1;
	xitk_show_widgets(playlist->widget_list);
	XLockDisplay(gGui->display);
	XRaiseWindow(gGui->display, playlist->window); 
	XMapWindow(gGui->display, playlist->window); 
	XSetTransientForHint (gGui->display, 
			      playlist->window, gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(playlist->window);
      }
    }
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
	char *pa_mrl, *pl_mrl;
	mediamark_t *mmk = (mediamark_t *)mediamark_get_current_mmk();
	
	if(mmk) {

	  pa_mrl = (gGui->is_display_mrl) ? gGui->mmk.mrl : gGui->mmk.ident;
	  pl_mrl = (gGui->is_display_mrl) ? mmk->mrl : mmk->ident;
	  
	  if(!strcmp(pa_mrl, pl_mrl)) {
	    int max_displayed = xitk_browser_get_num_entries(playlist->playlist);
	    
	    if((gGui->playlist.num - gGui->playlist.cur) >= max_displayed)
	      _playlist_update_browser_list(gGui->playlist.cur);
	    else
	      _playlist_update_browser_list(gGui->playlist.num - max_displayed);
	    
	    xitk_inputtext_change_text(playlist->widget_list, 
				       playlist->winput, 
				       playlist->playlist_mrls[gGui->playlist.cur]);
	    
	  }
	  
	}
	else
	  xitk_inputtext_change_text(playlist->widget_list, playlist->winput, NULL);

      }
    }
  }
}

/*
 * Change the current skin.
 */
void playlist_change_skins(void) {
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
    
    XSetTransientForHint(gGui->display, playlist->window, gGui->video_window);
    
    Imlib_destroy_image(gGui->imlib_data, old_img);
    Imlib_apply_image(gGui->imlib_data, new_img, playlist->window);
    
    XUnlockDisplay(gGui->display);
    
    xitk_skin_unlock(gGui->skin_config);
    
    xitk_change_skins_widget_list(playlist->widget_list, gGui->skin_config);
    
    {
      int x, y, dir;
      int i = 0;
      
      x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
      y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
      dir = xitk_skin_get_direction(gGui->skin_config, "AutoPlayBG");
      
      switch(dir) {
      case DIRECTION_UP:
      case DIRECTION_DOWN:
	while(playlist->autoplay_plugins[i] != NULL) {
	  
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
	while(playlist->autoplay_plugins[i] != NULL) {
	  
	  (void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);
	  
	  if(dir == DIRECTION_RIGHT)
	    x += xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1;
	  else
	    x -= (xitk_get_widget_width(playlist->autoplay_plugins[i]) + 1);
	  
	  i++;
	}
	break;
      }
      ////
      /*       while(playlist->autoplay_plugins[i] != NULL) { */
      
      /* 	(void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y); */
      
      /* 	y += xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1; */
      /* 	i++; */
      /*       } */
    }
    
    xitk_paint_widget_list(playlist->widget_list);
  }
}

/*
 * Create playlist editor window
 */
void playlist_editor(void) {
  GC                         gc;
  XSizeHints                 hint;
  XWMHints                  *wm_hint;
  XSetWindowAttributes       attr;
  char                      *title;
  Atom                       prop;
  MWMHints                   mwmhints;
  XClassHint                *xclasshint;
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_inputtext_widget_t    inp;
  xitk_button_widget_t       b;
  xitk_widget_t             *w;

  xine_strdupa(title, _("Xine Playlist Editor"));

  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  XITK_WIDGET_INIT(&b, gGui->imlib_data);

  playlist = (_playlist_t *) xine_xmalloc(sizeof(_playlist_t));

  playlist->playlist_len = 0;

  _playlist_create_playlists();

  XLockDisplay (gGui->display);

  if (!(playlist->bg_image = Imlib_load_image(gGui->imlib_data,
					      xitk_skin_get_skin_filename(gGui->skin_config, "PlBG")))) {
    xine_error(_("playlist: couldn't find image for background\n"));
    exit(-1);
  }

  hint.x = xine_config_register_num (gGui->xine, "gui.playlist_x",
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_BEG,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);
  hint.y = xine_config_register_num (gGui->xine, "gui.playlist_y", 
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_BEG,
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
  
  XSetStandardProperties(gGui->display, playlist->window, title, title,
			 None, NULL, 0, &hint);
  
  XSelectInput(gGui->display, playlist->window, INPUT_MOTION | KeymapStateMask);
  
  if(is_layer_above())
    xitk_set_layer_above(playlist->window);

  /*
   * wm, no border please
   */

  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, playlist->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = title;
    xclasshint->res_class = "Xine";
    XSetClassHint(gGui->display, playlist->window, xclasshint);
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(gGui->display, playlist->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(gGui->display, playlist->window, 0, 0);
  
  Imlib_apply_image(gGui->imlib_data, playlist->bg_image, playlist->window);

  XUnlockDisplay (gGui->display);

  /*
   * Widget-list
   */
  playlist->widget_list                = xitk_widget_list_new();
  playlist->widget_list->l             = xitk_list_new();
  playlist->widget_list->win           = playlist->window;
  playlist->widget_list->gc            = gc;

  lbl.window          = playlist->widget_list->win;
  lbl.gc              = playlist->widget_list->gc;

  b.skin_element_name = "PlMoveUp";
  b.callback          = _playlist_move_updown;
  b.userdata          = (void *)MOVEUP;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Move up selected MRL"));


  b.skin_element_name = "PlMoveDn";
  b.callback          = _playlist_move_updown;
  b.userdata          = (void *)MOVEDN;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Move down selected MRL"));

  b.skin_element_name = "PlPlay";
  b.callback          = _playlist_play;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Start playback of selected MRL"));
  
  b.skin_element_name = "PlDelete";
  b.callback          = _playlist_delete;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Delete selected MRL from playlist"));

  b.skin_element_name = "PlDeleteAll";
  b.callback          = _playlist_delete_all;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_button_create (playlist->widget_list, gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Delete all entries in playlist"));

  lb.skin_element_name = "PlAdd";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Add");
  lb.callback          = open_mrlbrowser;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Add one or more entries in playlist"));
    

  lb.skin_element_name = "PlLoad";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Load");
  lb.callback          = _playlist_load_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Load saved playlist"));

  lb.skin_element_name = "PlSave";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.callback          = _playlist_save_playlist;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Save playlist"));

  lb.skin_element_name = "PlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.callback          = playlist_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
		    (w = xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Close playlist window"));

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
  xitk_list_append_content (playlist->widget_list->l, 
   (playlist->playlist = xitk_browser_create(playlist->widget_list, gGui->skin_config, &br)));

  lbl.skin_element_name = "AutoPlayLbl";
  lbl.label             = _("Scan for:");
  lbl.callback          = NULL;
  xitk_list_append_content (playlist->widget_list->l,
			    xitk_label_create (playlist->widget_list, gGui->skin_config, &lbl));
  
  inp.skin_element_name = "PlInputText";
  inp.text              = NULL;
  inp.max_length        = 256;
  inp.callback          = _playlist_add_input;
  inp.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l,
   (playlist->winput = xitk_inputtext_create (playlist->widget_list, gGui->skin_config, &inp)));
  xitk_set_widget_tips(playlist->winput, _("Direct MRL entry"));

  {
    int                x, y, dir;
    int                i = 0;
    const char *const *autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    
    x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
    y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
    dir = xitk_skin_get_direction(gGui->skin_config, "AutoPlayBG");
      
    while(autoplay_plugins[i] != NULL) {

      lb.skin_element_name = "AutoPlayBG";
      lb.button_type       = CLICK_BUTTON;
      lb.label             = autoplay_plugins[i];
      lb.callback          = playlist_scan_input;
      lb.state_callback    = NULL;
      lb.userdata          = NULL;
      xitk_list_append_content (playlist->widget_list->l,
	       (playlist->autoplay_plugins[i] = 
		xitk_labelbutton_create (playlist->widget_list, gGui->skin_config, &lb)));
      xitk_set_widget_tips(playlist->autoplay_plugins[i], 
		   xine_get_input_plugin_description(gGui->xine, (char *)autoplay_plugins[i]));
      
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

  while(!xitk_is_window_visible(gGui->display, playlist->window))
    xine_usec_sleep(5000);

  XLockDisplay(gGui->display);
  XSetInputFocus(gGui->display, playlist->window, RevertToParent, CurrentTime);
  XUnlockDisplay(gGui->display);

  xitk_set_focus_to_widget(playlist->widget_list, playlist->winput);
}
