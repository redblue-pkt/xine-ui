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

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "parseskin.h"
#include "actions.h"
#include "utils.h"
#include "xine.h"

#define MAX_LIST 9

extern gGui_t        *gGui;

typedef struct {
  Window              window;
  widget_t           *playlist;
  ImlibImage         *bg_image;
  widget_list_t      *widget_list;
  
  int                 running;
  int                 visible;
  widgetkey_t         widget_key;
} _playlist_t;

static _playlist_t   *playlist;

#define MOVEUP 1
#define MOVEDN 2

#define PL_FILENAME  ".xinepl"


void playlist_handle_event(XEvent *event);

/*
 * Toolkit event handler will call this function with new
 * coords of playlist window.
 */
static void playlist_store_new_position(int x, int y, int w, int h) {

  config_set_int("playlist_x", x);
  config_set_int("playlist_y", y);
}

/*
 *
 */
void pl_update_playlist(void) {

  browser_update_list(playlist->playlist, 
		      gGui->playlist, gGui->playlist_num, 0);
}

/*
 *
 */
static void handle_selection(widget_t *w, void *data) {

  //  perr(" +++ Selection called = %d = '%s'\n", 
  //       ((int)data), gGui->gui_playlist[((int)data)]);
}

/*
 * Leaving playlist editor
 */
void pl_exit(widget_t *w, void *data) {

  playlist->running = 0;
  playlist->visible = 0;

  widget_unregister_event_handler(&playlist->widget_key);
  XUnmapWindow(gGui->display, playlist->window);

  XDestroyWindow(gGui->display, playlist->window);
  XFlush(gGui->display);

  Imlib_destroy_image(gGui->imlib_data, playlist->bg_image);
  playlist->window = None;
  gui_list_free(playlist->widget_list->l);
  free(playlist->widget_list->gc);
  free(playlist->widget_list);

  free(playlist);
  playlist = NULL;
}

/*
 * Start playing an MRL
 */
static void pl_play(widget_t *w, void *data) {
  int j;
  
  j = browser_get_current_selected(playlist->playlist);

  if(j>=0 && gGui->playlist[j] != NULL) {
    
    gui_set_current_mrl(gGui->playlist[j]);
    if(xine_get_status(gGui->xine) != XINE_STOP)
      gui_stop(NULL, NULL);
    
    gGui->playlist_cur = j;
    
    gui_play(NULL, NULL);
    browser_release_all_buttons(playlist->playlist);
  }
}

/*
 * Delete selected MRL
 */
static void pl_delete(widget_t *w, void *data) {
  int i, j;

  j = browser_get_current_selected(playlist->playlist);
  
  if(j >= 0) {
    for(i = j; i < gGui->playlist_num; i++) {
      gGui->playlist[i] = gGui->playlist[i+1];
    }
    gGui->playlist_num--;
    if(gGui->playlist_cur) gGui->playlist_cur--;
  }

  browser_rebuild_browser(playlist->playlist, 0);
  
  if(gGui->playlist_num)
    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
  else
    gui_set_current_mrl(NULL);

}

/*
 * Delete all MRLs
 */
static void pl_delete_all(widget_t *w, void *data) {
  int i;

  for(i = 0; i < gGui->playlist_num; i++) {
    gGui->playlist[i] = NULL;
  }

  gGui->playlist_num = 0;
  gGui->playlist_cur = 0;

  browser_update_list(playlist->playlist, gGui->playlist, gGui->playlist_num, 0);
  gui_set_current_mrl(NULL);

}

/*
 * Move entry up/down in playlist
 */
static void pl_move_updown(widget_t *w, void *data) {
  int j;
  
  j = browser_get_current_selected(playlist->playlist);
  
  if((j >= 0) && gGui->playlist[gGui->playlist_cur] != NULL) {
    
    if(((int)data) == MOVEUP && (j > 0)) {
      char *tmplist[MAX_PLAYLIST_LENGTH];
      char *tmp;
      int i, k;

      tmp = gGui->playlist[j-1];
      for(i=0; i<j-1; i++) {
	tmplist[i] = gGui->playlist[i];
      }
      tmplist[i] = gGui->playlist[j];
      tmplist[i+1] = tmp;
      i += 2;
      for(k=i++; k<gGui->playlist_num; k++) {
	tmplist[k] = gGui->playlist[k];
      }
      for(i=0; i<gGui->playlist_num; i++)
	gGui->playlist[i] = tmplist[i];

      if(j > MAX_LIST || (browser_get_current_start(playlist->playlist) != 0)) {
	j -= ((MAX_LIST-1)/2);
	browser_rebuild_browser(playlist->playlist, j);
	browser_set_select(playlist->playlist, ((MAX_LIST-1)/2)-1);
      }
      else {
	j = browser_get_current_selected(playlist->playlist);
	browser_rebuild_browser(playlist->playlist, -1);
	browser_set_select(playlist->playlist, j-1);
      }
    }
    else if(((int)data) == MOVEDN && (j < (gGui->playlist_num-1))) {
      char *tmplist[MAX_PLAYLIST_LENGTH];
      char *tmp;
      int i, k;

      tmp = gGui->playlist[j];
      for(i=0; i<j; i++) {
	tmplist[i] = gGui->playlist[i];
      }
      tmplist[i] = gGui->playlist[j+1];
      tmplist[i+1] = tmp;
      i += 2;
      for(k=i++; k<gGui->playlist_num; k++) {
	tmplist[k] = gGui->playlist[k];
      }
      for(i=0; i<gGui->playlist_num; i++)
	gGui->playlist[i] = tmplist[i];

      if(j >= (MAX_LIST-1)) {
	browser_rebuild_browser(playlist->playlist, j);
	browser_set_select(playlist->playlist, 1);
      }
      else {
	j = browser_get_current_selected(playlist->playlist);
	browser_rebuild_browser(playlist->playlist, -1);
	browser_set_select(playlist->playlist, j+1);
      }      
    }
  }
}

/*
 * return 1 if playlist editor is ON
 */
int pl_is_running(void) {

  if(playlist != NULL)
    return playlist->running;
 
  return 0;
}

/*
 * Return 1 if playlist editor is visible
 */
int pl_is_visible(void) {

  if(playlist != NULL)
    return playlist->visible;

  return 0;
}

/*
 * Load $HOME/.xinepl playlist file
 */
static void pl_load_pl(widget_t *w, void *data) {
  FILE *plfile;
  char buf[256], *ln, tmpbuf[256];
  char *tmp_playlist[MAX_PLAYLIST_LENGTH];
  int tmp_playlist_num = -1, i;

  memset(&buf, 0, sizeof(buf));
  memset(&tmpbuf, 0, sizeof(tmpbuf));
  
  sprintf(tmpbuf, "%s/%s", get_homedir(), PL_FILENAME);
  if((plfile = fopen(tmpbuf, "r")) == NULL) {
    fprintf(stderr, "Can't read %s: %s\n", tmpbuf, strerror(errno));
    return;
  }
  else {
    
    ln = fgets(buf, 255, plfile) ;
    if(!strncasecmp(ln, "[PLAYLIST]", 9)) {
      tmp_playlist_num = 0;
      ln = fgets(buf, 255, plfile);
      while(ln != NULL && strncasecmp("[END]", ln, 5)) {
	
	while(*ln == ' ' || *ln == '\t') ++ln ;
	
	tmp_playlist[tmp_playlist_num] = (char *) xmalloc(strlen(ln));
	strncpy(tmp_playlist[tmp_playlist_num], ln, strlen(ln)-1);
	tmp_playlist[tmp_playlist_num][strlen(ln)-1] = '\0';
	
	tmp_playlist_num++;
	
	ln = fgets(buf, 255, plfile);
	
      }
    }
    
    ln = fgets(buf, 255, plfile);
  }
  
  fclose(plfile);
  
  if(tmp_playlist_num >= 0) {
    gGui->playlist_num = tmp_playlist_num;
    
    for(i = 0; i < tmp_playlist_num; i++)
      gGui->playlist[i] = tmp_playlist[i];
    
    browser_update_list(playlist->playlist, 
			gGui->playlist, gGui->playlist_num, 0);
    //    browser_rebuild_browser(playlist->playlist, 0);
  }
  
}

/*
 * Save playlist to $HOME/.xinepl file
 */
static void pl_save_pl(widget_t *w, void *data) {
  FILE *plfile;
  char buf[1024], tmpbuf[256];
  int i;

  if(!gGui->playlist_num) return;

  memset(&buf, 0, sizeof(buf));
  memset(&tmpbuf, 0, sizeof(tmpbuf));

  sprintf(tmpbuf, "%s/%s", get_homedir(), PL_FILENAME);
  if((plfile = fopen(tmpbuf, "w")) == NULL) {
    fprintf(stderr, "Can't read %s: %s\n", tmpbuf, strerror(errno));
    return;
  }
  else {
    sprintf(buf, "%s", "[PLAYLIST]\n");
    fputs(buf, plfile);
    
    for(i = 0; i < gGui->playlist_num; i++) {
      memset(&buf, 0, sizeof(buf));
      sprintf(buf, "%s\n", gGui->playlist[i]);
      fputs(buf, plfile);
    }
    
    sprintf(buf, "%s", "[END]\n");
    fputs(buf, plfile);
    
  }
  fclose(plfile);
  
}

/*
 * Handle autoplay buttons hitting (from panel and playlist windows)
 */
void pl_scan_input(widget_t *w, void *ip) {
  
  if(xine_get_status(gGui->xine) == XINE_STOP) {
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    int i = 0;
    
    if(autoplay_plugins) {
      while(autoplay_plugins[i] != NULL) {
	
	if(!strcasecmp(autoplay_plugins[i], labelbutton_get_label(w))) {
	  char **autoplay_mrls = 
	    xine_get_autoplay_mrls (gGui->xine, autoplay_plugins[i]);
	  int j = 0;
	  
	  if(autoplay_mrls) {
	    while(autoplay_mrls[j]) {
	      gGui->playlist[gGui->playlist_num + j] = autoplay_mrls[j];
	      j++;
	    }
	    
	    gGui->playlist_num += j;
	    gGui->playlist_cur = 0;
	    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	  }
	}
	
	i++;
      }
      
      if(playlist)
	browser_update_list(playlist->playlist, 
			    gGui->playlist, gGui->playlist_num, 0);
    }
  }
}

/*
 * Raise playlist->window
 */
void pl_raise_window(void) {
  
  if(playlist != NULL) {
    if(playlist->window) {
      if(playlist->visible && playlist->running) {
	if(playlist->running) {
	  XMapRaised(gGui->display, playlist->window); 
	  XSetTransientForHint (gGui->display, 
				playlist->window, gGui->video_window);
	}
      } else {
	XUnmapWindow (gGui->display, playlist->window);
      }
    }
  }
}

/*
 * Hide/show the pl panel
 */
void pl_toggle_visibility (widget_t *w, void *data) {
  
  if(playlist != NULL) {
    if (playlist->visible && playlist->running) {
      playlist->visible = 0;
      XUnmapWindow (gGui->display, playlist->window);
    } else {
      if(playlist->running) {
	playlist->visible = 1;
	XMapRaised(gGui->display, playlist->window); 
	XSetTransientForHint (gGui->display, 
			      playlist->window, gGui->video_window);
      }
    }
  }
}

/*
 * Handle X events here.
 */
void playlist_handle_event(XEvent *event) {

  switch(event->type) {

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;
    
  }
}

/*
 * Create playlist editor window
 */
void playlist_editor(void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  char                    title[] = {"Xine Playlist Editor"};
  Atom                    prop;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;

  /* This shouldn't be happend */
  if(playlist != NULL) {
    if(playlist->window)
      return;
  }

  playlist = (_playlist_t *) xmalloc(sizeof(_playlist_t));

  XLockDisplay (gGui->display);

  if (!(playlist->bg_image = Imlib_load_image(gGui->imlib_data,
				       gui_get_skinfile("PlBG")))) {
    fprintf(stderr, "xine-playlist: couldn't find image for background\n");
    exit(-1);
  }

  hint.x = config_lookup_int ("playlist_x", 200);
  hint.y = config_lookup_int ("playlist_y", 100);
  hint.width = playlist->bg_image->rgb_width;
  hint.height = playlist->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;
  attr.background_pixel  = gGui->black.pixel;
  attr.border_pixel      = 1;
  attr.colormap          = XCreateColormap(gGui->display,
                                           RootWindow(gGui->display, gGui->screen),
                                           gGui->imlib_data->x.visual, AllocNone);

  playlist->window = XCreateWindow (gGui->display, 
				    DefaultRootWindow(gGui->display), 
				    hint.x, hint.y, hint.width, 
				    hint.height, 0, 
				    gGui->imlib_data->x.depth, CopyFromParent, 
				    gGui->imlib_data->x.visual,
				    CWBackPixel | CWBorderPixel | CWColormap, &attr);
  
  XSetStandardProperties(gGui->display, playlist->window, title, title,
			 None, NULL, 0, &hint);
  
  XSelectInput(gGui->display, playlist->window,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);
  
  /*
   * wm, no border please
   */

  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, playlist->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XSetTransientForHint (gGui->display, playlist->window, gGui->video_window);

  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Playlist Editor";
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
  XSync(gGui->display, False); 

  /*
   * Widget-list
   */
  playlist->widget_list                = widget_list_new();
  playlist->widget_list->l             = gui_list_new ();
  playlist->widget_list->focusedWidget = NULL;
  playlist->widget_list->pressedWidget = NULL;
  playlist->widget_list->win           = playlist->window;
  playlist->widget_list->gc            = gc;

  gui_list_append_content (playlist->widget_list->l, 
			    create_label_button (gGui->display, 
						gGui->imlib_data, 
						gui_get_skinX("PlMoveUp"),
						gui_get_skinY("PlMoveUp"),
						CLICK_BUTTON, "Move Up",
						pl_move_updown, (void*)MOVEUP, 
						gui_get_skinfile("PlMoveUp"),
						gui_get_ncolor("PlMoveUp"),
						gui_get_fcolor("PlMoveUp"),
						gui_get_ccolor("PlMoveUp")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlMoveDn"),
						gui_get_skinY("PlMoveDn"),
						CLICK_BUTTON, "Move Dn",
						pl_move_updown, (void*)MOVEDN, 
						gui_get_skinfile("PlMoveDn"),
						gui_get_ncolor("PlMoveDn"),
						gui_get_fcolor("PlMoveDn"),
						gui_get_ccolor("PlMoveDn")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlPlay"),
						gui_get_skinY("PlPlay"),
						CLICK_BUTTON, "Play",
						pl_play, NULL, 
						gui_get_skinfile("PlPlay"),
						gui_get_ncolor("PlPlay"),
						gui_get_fcolor("PlPlay"),
						gui_get_ccolor("PlPlay")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlDelete"),
						gui_get_skinY("PlDelete"),
						CLICK_BUTTON, "Delete",
						pl_delete, NULL, 
						gui_get_skinfile("PlDelete"),
						gui_get_ncolor("PlDelete"),
						gui_get_fcolor("PlDelete"),
						gui_get_ccolor("PlDelete")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlDeleteAll"),
						gui_get_skinY("PlDeleteAll"),
						CLICK_BUTTON, "Delete All",
						pl_delete_all, NULL, 
						gui_get_skinfile("PlDeleteAll"),
						gui_get_ncolor("PlDeleteAll"),
						gui_get_fcolor("PlDeleteAll"),
						gui_get_ccolor("PlDeleteAll")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlLoad"),
						gui_get_skinY("PlLoad"),
						CLICK_BUTTON, "Load",
						pl_load_pl, NULL, 
						gui_get_skinfile("PlLoad"),
						gui_get_ncolor("PlLoad"),
						gui_get_fcolor("PlLoad"),
						gui_get_ccolor("PlLoad")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlSave"),
						gui_get_skinY("PlSave"),
						CLICK_BUTTON, "Save",
						pl_save_pl, NULL, 
						gui_get_skinfile("PlSave"),
						gui_get_ncolor("PlSave"),
						gui_get_fcolor("PlSave"),
						gui_get_ccolor("PlSave")));

  gui_list_append_content (playlist->widget_list->l, 
			   create_label_button (gGui->display,
						gGui->imlib_data, 
						gui_get_skinX("PlDismiss"),
						gui_get_skinY("PlDismiss"),
						CLICK_BUTTON, "Dismiss",
						pl_exit, NULL, 
						gui_get_skinfile("PlDismiss"),
						gui_get_ncolor("PlDismiss"),
						gui_get_fcolor("PlDismiss"),
						gui_get_ccolor("PlDismiss")));


  gui_list_append_content (playlist->widget_list->l, 
		   (playlist->playlist = 
		    create_browser(gGui->display,
				   gGui->imlib_data,
				   playlist->widget_list,
				   gui_get_skinX("PlUp"),
				   gui_get_skinY("PlUp"),
				   gui_get_skinfile("PlUp"),
				   gui_get_skinX("PlSlidBG"),
				   gui_get_skinY("PlSlidBG"),
				   gui_get_skinfile("PlSlidBG"),
				   gui_get_skinfile("PlSlidFG"),
				   gui_get_skinX("PlDn"),
				   gui_get_skinY("PlDn"),
				   gui_get_skinfile("PlDn"),
				   gui_get_skinX("PlItemBtn"),
				   gui_get_skinY("PlItemBtn"),
				   gui_get_ncolor("PlItemBtn"),
				   gui_get_fcolor("PlItemBtn"),
				   gui_get_ccolor("PlItemBtn"),
				   gui_get_skinfile("PlItemBtn"),
				   9, gGui->playlist_num, gGui->playlist,
				   handle_selection, NULL)));
  
  gui_list_append_content (playlist->widget_list->l,
			   create_label (gGui->display, gGui->imlib_data, 
					 gui_get_skinX("AutoPlayLbl"),
					 gui_get_skinY("AutoPlayLbl"),
					 9, "Scan for:",
					 gui_get_skinfile("AutoPlayLbl")));
  
  {
    int x, y;
    int i = 0;
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    widget_t *tmp;
    
    x = gui_get_skinX("AutoPlayBG");
    y = gui_get_skinY("AutoPlayBG");
    
    while(autoplay_plugins[i] != NULL) {
      gui_list_append_content (playlist->widget_list->l,
	       (tmp =
		create_label_button (gGui->display, gGui->imlib_data, 
				     x, y,
				     CLICK_BUTTON,
				     autoplay_plugins[i],
				     pl_scan_input, NULL, 
				     gui_get_skinfile("AutoPlayBG"),
				     gui_get_ncolor("AutoPlayBG"),
				     gui_get_fcolor("AutoPlayBG"),
				     gui_get_ccolor("AutoPlayBG"))));
      y += widget_get_height(tmp) + 1;
      i++;
    }
  }
  browser_update_list(playlist->playlist, 
		      gGui->playlist, gGui->playlist_num, 0);

  XMapRaised(gGui->display, playlist->window); 

  playlist->widget_key = 
    widget_register_event_handler("playlist", 
				  playlist->window, 
				  playlist_handle_event,
				  playlist_store_new_position,
				  gui_dndcallback,
				  playlist->widget_list);

  playlist->visible = 1;
  playlist->running = 1;
   
  XUnlockDisplay (gGui->display);
  XSync(gGui->display, False);
}
