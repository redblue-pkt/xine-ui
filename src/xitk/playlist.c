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
#include <X11/keysym.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "actions.h"
#include "mrl_browser.h"

#include <xine.h>
#include <xine/xineutils.h>

#define MAX_LIST 9

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
} _playlist_t;

static _playlist_t   *playlist;

#define MOVEUP 1
#define MOVEDN 2

#define PL_FILENAME  "playlist"


void playlist_handle_event(XEvent *event, void *data);

/*
 *
 */
void pl_update_playlist(void) {

  if(playlist)
    xitk_browser_update_list(playlist->playlist, 
			     gGui->playlist, gGui->playlist_num, 0);
}

/*
 *
 */
static void handle_selection(xitk_widget_t *w, void *data) {
  int selected = (int)data;

  if(gGui->playlist[selected] != NULL) {
    xitk_inputtext_change_text(playlist->widget_list, 
			       playlist->winput, gGui->playlist[selected]);
  }
}

/*
 * Leaving playlist editor
 */
void pl_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;

  playlist->running = 0;
  playlist->visible = 0;

  if((xitk_get_window_info(playlist->widget_key, &wi))) {
    gGui->config->update_num (gGui->config, "gui.playlist_x", wi.x);
    gGui->config->update_num (gGui->config, "gui.playlist_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  xitk_unregister_event_handler(&playlist->widget_key);

  XLockDisplay(gGui->display);
  XUnlockDisplay(gGui->display);
  XUnmapWindow(gGui->display, playlist->window);

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

  free(playlist->widget_list);
 
  free(playlist);
  playlist = NULL;
}

/*
 * Start playing an MRL
 */
static void pl_play(xitk_widget_t *w, void *data) {
  int j;
  
  j = xitk_browser_get_current_selected(playlist->playlist);

  if(j>=0 && gGui->playlist[j] != NULL) {
    
    gui_set_current_mrl(gGui->playlist[j]);
    if(xine_get_status(gGui->xine) != XINE_STOP)
      gui_stop(NULL, NULL);
    
    gGui->playlist_cur = j;
    
    gui_play(NULL, NULL);
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}

/*
 * Start to play the selected stream on double click event in playlist.
 */
static void pl_on_dbl_click(xitk_widget_t *w, void *data, int selected) {

  if(gGui->playlist[selected] != NULL) {
    
    gui_set_current_mrl(gGui->playlist[selected]);
    if(xine_get_status(gGui->xine) != XINE_STOP)
      gui_stop(NULL, NULL);
    
    gGui->playlist_cur = selected;
    
    gui_play(NULL, NULL);
    xitk_browser_release_all_buttons(playlist->playlist);
  }
}

/*
 * Delete selected MRL
 */
static void pl_delete(xitk_widget_t *w, void *data) {
  int i, j;

  j = xitk_browser_get_current_selected(playlist->playlist);
  
  if(j >= 0) {
    for(i = j; i < gGui->playlist_num; i++) {
      gGui->playlist[i] = gGui->playlist[i+1];
    }
    gGui->playlist_num--;
    if(gGui->playlist_cur) gGui->playlist_cur--;
  }

  xitk_browser_rebuild_browser(playlist->playlist, 0);
  
  if(gGui->playlist_num)
    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
  else
    gui_set_current_mrl(NULL);

}

/*
 * Delete all MRLs
 */
static void pl_delete_all(xitk_widget_t *w, void *data) {
  int i;

  for(i = 0; i < gGui->playlist_num; i++) {
    gGui->playlist[i] = NULL;
  }

  gGui->playlist_num = 0;
  gGui->playlist_cur = 0;

  xitk_browser_update_list(playlist->playlist, 
			   gGui->playlist, gGui->playlist_num, 0);
  gui_set_current_mrl(NULL);

}

/*
 * Move entry up/down in playlist
 */
static void pl_move_updown(xitk_widget_t *w, void *data) {
  int j;
  
  j = xitk_browser_get_current_selected(playlist->playlist);
  
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

      if(j > MAX_LIST || (xitk_browser_get_current_start(playlist->playlist) != 0)) {
	j -= ((MAX_LIST-1)/2);
	xitk_browser_rebuild_browser(playlist->playlist, j);
	xitk_browser_set_select(playlist->playlist, ((MAX_LIST-1)/2)-1);
      }
      else {
	j = xitk_browser_get_current_selected(playlist->playlist);
	xitk_browser_rebuild_browser(playlist->playlist, -1);
	xitk_browser_set_select(playlist->playlist, j-1);
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
	xitk_browser_rebuild_browser(playlist->playlist, j);
	xitk_browser_set_select(playlist->playlist, 1);
      }
      else {
	j = xitk_browser_get_current_selected(playlist->playlist);
	xitk_browser_rebuild_browser(playlist->playlist, -1);
	xitk_browser_set_select(playlist->playlist, j+1);
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
 * Callback called by mrl_browser on add event.
 */
static void playlist_add(xitk_widget_t *w, void *data, const char *filename) {

  if(filename)
    gui_dndcallback((char *)filename);
}

/*
 * Load $HOME/.xinepl playlist file
 */
static void pl_load_pl(xitk_widget_t *w, void *data) {
  FILE *plfile;
  char buf[256], *ln, tmpbuf[256];
  char *tmp_playlist[MAX_PLAYLIST_LENGTH];
  int tmp_playlist_num = -1, i;

  memset(&buf, 0, sizeof(buf));
  memset(&tmpbuf, 0, sizeof(tmpbuf));
  
  sprintf(tmpbuf, "%s/.xine/%s", xine_get_homedir(), PL_FILENAME);
  if((plfile = fopen(tmpbuf, "r")) == NULL) {
    xine_error(_("Can't read %s: %s\n"), tmpbuf, strerror(errno));
    return;
  }
  else {
    
    ln = fgets(buf, 255, plfile) ;
    if(!strncasecmp(ln, "[PLAYLIST]", 9)) {
      tmp_playlist_num = 0;
      ln = fgets(buf, 255, plfile);
      while(ln != NULL && strncasecmp("[END]", ln, 5)) {
	
	while(*ln == ' ' || *ln == '\t') ++ln ;
	
	tmp_playlist[tmp_playlist_num] = (char *) xine_xmalloc(strlen(ln));
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
    
    xitk_browser_update_list(playlist->playlist, 
			     gGui->playlist, gGui->playlist_num, 0);
    /* xitk_browser_rebuild_browser(playlist->playlist, 0); */
  }
  
}

/*
 * Save playlist to $HOME/.xinepl file
 */
static void pl_save_pl(xitk_widget_t *w, void *data) {
  FILE *plfile;
  char buf[1024], tmpbuf[256];
  int i;

  if(!gGui->playlist_num) return;

  memset(&buf, 0, sizeof(buf));
  memset(&tmpbuf, 0, sizeof(tmpbuf));

  sprintf(tmpbuf, "%s/.xine/%s", xine_get_homedir(), PL_FILENAME);
  if((plfile = fopen(tmpbuf, "w")) == NULL) {
    xine_error(_("Can't read %s: %s\n"), tmpbuf, strerror(errno));
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
void pl_scan_input(xitk_widget_t *w, void *ip) {
  
  if(xine_get_status(gGui->xine) == XINE_STOP) {
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    int i = 0;
    
    if(autoplay_plugins) {
      while(autoplay_plugins[i] != NULL) {
	
	if(!strcasecmp(autoplay_plugins[i], xitk_labelbutton_get_label(w))) {
	  int num_mrls;
	  char **autoplay_mrls = 
	    xine_get_autoplay_mrls (gGui->xine, autoplay_plugins[i], &num_mrls);

	  if(autoplay_mrls) {
	    int j;

	    for (j=0; j<num_mrls; j++) 
	      gGui->playlist[gGui->playlist_num + j] = autoplay_mrls[j];
	    
	    gGui->playlist_num += j;
	    gGui->playlist_cur = 0;
	    gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	  }
	}
	
	i++;
      }
      
      if(playlist)
	xitk_browser_update_list(playlist->playlist, 
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
	  layer_above_video(playlist->window);
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
void pl_toggle_visibility (xitk_widget_t *w, void *data) {
  
  if(playlist != NULL) {
    if (playlist->visible && playlist->running) {
      playlist->visible = 0;
      xitk_hide_widgets(playlist->widget_list);
      XUnmapWindow (gGui->display, playlist->window);
    } else {
      if(playlist->running) {
	playlist->visible = 1;
	xitk_show_widgets(playlist->widget_list);
	XMapRaised(gGui->display, playlist->window); 
	XSetTransientForHint (gGui->display, 
			      playlist->window, gGui->video_window);
	layer_above_video(playlist->window);
      }
    }
  }
}

/*
 * Handle X events here.
 */
void playlist_handle_event(XEvent *event, void *data) {
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;

  switch(event->type) {

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    if (bevent->button == Button4)
      xitk_browser_step_down(playlist->playlist, NULL);
    else if(bevent->button == Button5)
      xitk_browser_step_up(playlist->playlist, NULL);
  }
  break;

  case KeyPress:
    if(xitk_is_widget_focused(playlist->winput)) {
      xitk_send_key_event(playlist->widget_list, playlist->winput, event);
    }
    else {
      
      mykeyevent = event->xkey;
      
      XLockDisplay(gGui->display);
      len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
      XUnlockDisplay(gGui->display);
      
      switch (mykey) {
	
      case XK_Down:
      case XK_Next:
	xitk_browser_step_up(playlist->playlist, NULL);
	break;
	
      case XK_Up:
      case XK_Prior:
	xitk_browser_step_down(playlist->playlist, NULL);
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

/*
 *
 */
static void pl_add_input(xitk_widget_t *w, void *data, char *filename) {

  if(filename)
    gui_dndcallback((char *)filename);
}

/*
 * Change the current skin.
 */
void playlist_change_skins(void) {

  if(pl_is_running()) {
    
    XLockDisplay(gGui->display);
    
    Imlib_destroy_image(gGui->imlib_data, playlist->bg_image);
    
    if(!(playlist->bg_image = 
	 Imlib_load_image(gGui->imlib_data,
			  xitk_skin_get_skin_filename(gGui->skin_config, "PlBG")))) {
      xine_error("%s(): couldn't find image for background\n", __FUNCTION__);
      exit(-1);
    }
    
    XResizeWindow (gGui->display, playlist->window,
		   (unsigned int)playlist->bg_image->rgb_width,
		   (unsigned int)playlist->bg_image->rgb_height);
    
    /*
     * We should here, otherwise new skined window will have wrong size.
     */
    XFlush(gGui->display);
    
    Imlib_apply_image(gGui->imlib_data, playlist->bg_image, playlist->window);
    
    XUnlockDisplay(gGui->display);
    
    xitk_change_skins_widget_list(playlist->widget_list, gGui->skin_config);
    
    {
      int x, y;
      int i = 0;
      
      x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
      y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
      
      while(playlist->autoplay_plugins[i] != NULL) {
	
	(void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);
	
	y += xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1;
	i++;
      }
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
  char                       title[] = {"Xine Playlist Editor"};
  Atom                       prop, XA_WIN_LAYER;
  MWMHints                   mwmhints;
  XClassHint                *xclasshint;
  xitk_browser_widget_t       br;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_inputtext_widget_t     inp;
  xitk_button_widget_t        b;
  long data[1];

  /* This shouldn't be happend */
  if(playlist != NULL) {
    if(playlist->window)
      return;
  }

  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  XITK_WIDGET_INIT(&b, gGui->imlib_data);

  playlist = (_playlist_t *) xine_xmalloc(sizeof(_playlist_t));

  XLockDisplay (gGui->display);

  if (!(playlist->bg_image = Imlib_load_image(gGui->imlib_data,
					      xitk_skin_get_skin_filename(gGui->skin_config, "PlBG")))) {
    xine_error("xine-playlist: couldn't find image for background\n");
    exit(-1);
  }

  hint.x = gGui->config->register_num (gGui->config, "gui.playlist_x", 200,
				       NULL, NULL, NULL, NULL);
  hint.y = gGui->config->register_num (gGui->config, "gui.playlist_y", 200,
				       NULL, NULL, NULL, NULL);

  hint.width = playlist->bg_image->rgb_width;
  hint.height = playlist->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;
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
				    gGui->imlib_data->x.depth, CopyFromParent, 
				    gGui->imlib_data->x.visual,
				    CWBackPixel | CWBorderPixel | CWColormap, &attr);
  
  XSetStandardProperties(gGui->display, playlist->window, title, title,
			 None, NULL, 0, &hint);
  
  XSelectInput(gGui->display, playlist->window,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);
  
  /*
   * layer above most other things, like gnome panel
   * WIN_LAYER_ABOVE_DOCK  = 10
   *
   */
  if(gGui->layer_above) {
    XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);
    
    data[0] = 10;
    XChangeProperty(gGui->display, playlist->window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }

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

  /*
   * Widget-list
   */
  playlist->widget_list                = xitk_widget_list_new();
  playlist->widget_list->l             = xitk_list_new();
  playlist->widget_list->focusedWidget = NULL;
  playlist->widget_list->pressedWidget = NULL;
  playlist->widget_list->win           = playlist->window;
  playlist->widget_list->gc            = gc;

  lbl.window          = playlist->widget_list->win;
  lbl.gc              = playlist->widget_list->gc;

  b.skin_element_name = "PlMoveUp";
  b.callback          = pl_move_updown;
  b.userdata          = (void *)MOVEUP;
  xitk_list_append_content (playlist->widget_list->l, 
			    xitk_button_create (gGui->skin_config, &b));

  b.skin_element_name = "PlMoveDn";
  b.callback          = pl_move_updown;
  b.userdata          = (void *)MOVEDN;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_button_create (gGui->skin_config, &b));

  b.skin_element_name = "PlPlay";
  b.callback          = pl_play;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_button_create (gGui->skin_config, &b));

  b.skin_element_name = "PlDelete";
  b.callback          = pl_delete;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_button_create (gGui->skin_config, &b));

  b.skin_element_name = "PlDeleteAll";
  b.callback          = pl_delete_all;
  b.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_button_create (gGui->skin_config, &b));

  lb.skin_element_name = "PlAdd";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Add");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = open_mrlbrowser;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
                           xitk_labelbutton_create (gGui->skin_config, &lb));

  lb.skin_element_name = "PlLoad";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Load");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = pl_load_pl;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_labelbutton_create (gGui->skin_config, &lb));

  lb.skin_element_name = "PlSave";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = pl_save_pl;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_labelbutton_create (gGui->skin_config, &lb));

  lb.skin_element_name = "PlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = pl_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l, 
			   xitk_labelbutton_create (gGui->skin_config, &lb));

  br.arrow_up.skin_element_name    = "PlUp";
  br.slider.skin_element_name      = "SliderPl";
  br.arrow_dn.skin_element_name    = "PlDn";
  br.browser.skin_element_name     = "PlItemBtn";
  br.browser.max_displayed_entries = 9;
  br.browser.num_entries           = gGui->playlist_num;
  br.browser.entries               = gGui->playlist;
  br.callback                      = handle_selection;
  br.dbl_click_callback            = pl_on_dbl_click;
  br.dbl_click_time                = DEFAULT_DBL_CLICK_TIME;
  br.parent_wlist                  = playlist->widget_list;
  xitk_list_append_content (playlist->widget_list->l, 
			   (playlist->playlist = 
			    xitk_browser_create(gGui->skin_config, &br)));

  lbl.skin_element_name = "AutoPlayLbl";
  lbl.label             = _("Scan for:");
  xitk_list_append_content (playlist->widget_list->l,
			    xitk_label_create (gGui->skin_config, &lbl));
  
  inp.skin_element_name = "PlInputText";
  inp.text              = NULL;
  inp.max_length        = 256;
  inp.callback          = pl_add_input;
  inp.userdata          = NULL;
  xitk_list_append_content (playlist->widget_list->l,
			   (playlist->winput = xitk_inputtext_create (gGui->skin_config, &inp)));

  {
    int x, y;
    int i = 0;
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    
    x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayBG");
    y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayBG");
    
    while(autoplay_plugins[i] != NULL) {

      lb.skin_element_name = "AutoPlayBG";
      lb.button_type       = CLICK_BUTTON;
      lb.label             = autoplay_plugins[i];
      lb.align             = LABEL_ALIGN_CENTER;
      lb.callback          = pl_scan_input;
      lb.state_callback    = NULL;
      lb.userdata          = NULL;
      xitk_list_append_content (playlist->widget_list->l,
	       (playlist->autoplay_plugins[i] = 
		xitk_labelbutton_create (gGui->skin_config, &lb)));

      (void) xitk_set_widget_pos(playlist->autoplay_plugins[i], x, y);

      y += xitk_get_widget_height(playlist->autoplay_plugins[i]) + 1;
      i++;
    }
    if(i)
      playlist->autoplay_plugins[i+1] = NULL;

  }
  xitk_browser_update_list(playlist->playlist, 
			   gGui->playlist, gGui->playlist_num, 0);

  XMapRaised(gGui->display, playlist->window); 

  playlist->widget_key = 
    xitk_register_event_handler("playlist", 
				playlist->window, 
				playlist_handle_event,
				NULL,
				gui_dndcallback,
				playlist->widget_list, (void*) playlist);

  playlist->visible = 1;
  playlist->running = 1;
   
  XUnlockDisplay (gGui->display);
}
