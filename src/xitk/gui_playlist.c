/* 
 * Copyright (C) 2000 the xine project
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
#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <Imlib.h>
#include <pthread.h>
#include "gui_main.h"
#include "gui_list.h"
#include "gui_button.h"
#include "gui_labelbutton.h"
#include "gui_checkbox.h"
#include "gui_label.h"
#include "gui_dnd.h"
#include "gui_slider.h"
#include "gui_parseskin.h"
#include "gui_image.h"
#include "gui_browser.h"
#include "utils.h"
#include "xine.h"

#define MAX_LIST 9

extern gGlob_t        *gGlob;

static widget_t       *pl_list = NULL;

static Window          pl_win;
static DND_struct_t   *xdnd_pl_win;
static ImlibImage     *pl_bg_image;
static gui_move_t      pl_move; 
static widget_list_t  *pl_widget_list;

static int             pl_running;
static int             pl_panel_visible;

extern Window          gVideoWin;

#define MOVEUP 1
#define MOVEDN 2

#define PL_FILENAME  ".xinepl"

/*
 *
 */
void pl_update_playlist(void) {

  browser_update_list(pl_list, gGlob->gui_playlist, gGlob->gui_playlist_num, 0);
}
/*
 *
 */
static void handle_selection(widget_t *w, void *data) {

  //  perr(" +++ Selection called = %d = '%s'\n", 
  //       ((int)data), gGlob->gui_playlist[((int)data)]);
}
/*
 * Leaving playlist editor
 */
void pl_exit(widget_t *w, void *data) {

  pl_running = 0;
  pl_panel_visible = 0;

  XUnmapWindow(gGlob->gDisplay, pl_win);

  gui_list_free(pl_widget_list->l);
  free(pl_widget_list);
  pl_widget_list = NULL;

  XDestroyWindow(gGlob->gDisplay, pl_win);

  if(xdnd_pl_win)
    free(xdnd_pl_win);

  pl_win = 0;
}
/*
 * Start playing an MRL
 */
static void pl_play(widget_t *w, void *data) {
  int j;
  
  j = browser_get_current_selected(pl_list);

  if(j>=0 && gGlob->gui_playlist[j] != NULL) {
    
    gui_set_current_mrl(gGlob->gui_playlist[j]);
    if(xine_get_status(gGlob->gXine) != XINE_STOP)
      gui_stop(NULL, NULL);
    
    gGlob->gui_playlist_cur = j;
    
    gui_play(NULL, NULL);
    browser_release_all_buttons(pl_list);
  }
}
/*
 * Delete selected MRL
 */
static void pl_delete(widget_t *w, void *data) {
  int i, j;

  j = browser_get_current_selected(pl_list);
  
  if(j >= 0) {
    for(i = j; i < gGlob->gui_playlist_num; i++) {
      gGlob->gui_playlist[i] = gGlob->gui_playlist[i+1];
    }
    gGlob->gui_playlist_num--;
    if(gGlob->gui_playlist_cur) gGlob->gui_playlist_cur--;
  }

  browser_rebuild_browser(pl_list, 0);
  
  if(gGlob->gui_playlist_num)
    gui_set_current_mrl(gGlob->gui_playlist[gGlob->gui_playlist_cur]);
  else
    gui_set_current_mrl(NULL);

}
/*
 * Delete all MRLs
 */
static void pl_delete_all(widget_t *w, void *data) {
  int i;

  for(i = 0; i < gGlob->gui_playlist_num; i++) {
    gGlob->gui_playlist[i] = NULL;
  }

  gGlob->gui_playlist_num = 0;
  gGlob->gui_playlist_cur = 0;

  browser_update_list(pl_list, gGlob->gui_playlist, gGlob->gui_playlist_num, 0);
  gui_set_current_mrl(NULL);

}
/*
 * Move entry up/down in playlist
 */
static void pl_move_updown(widget_t *w, void *data) {
  int j;
  
  j = browser_get_current_selected(pl_list);
  
  if((j >= 0) && gGlob->gui_playlist[gGlob->gui_playlist_cur] != NULL) {
    
    if(((int)data) == MOVEUP && (j > 0)) {
      char *tmplist[MAX_PLAYLIST_LENGTH];
      char *tmp;
      int i, k;

      tmp = gGlob->gui_playlist[j-1];
      for(i=0; i<j-1; i++) {
	tmplist[i] = gGlob->gui_playlist[i];
      }
      tmplist[i] = gGlob->gui_playlist[j];
      tmplist[i+1] = tmp;
      i += 2;
      for(k=i++; k<gGlob->gui_playlist_num; k++) {
	tmplist[k] = gGlob->gui_playlist[k];
      }
      for(i=0; i<gGlob->gui_playlist_num; i++)
	gGlob->gui_playlist[i] = tmplist[i];

      if(j > MAX_LIST || (browser_get_current_start(pl_list) != 0)) {
	j -= ((MAX_LIST-1)/2);
	browser_rebuild_browser(pl_list, j);
	browser_set_select(pl_list, ((MAX_LIST-1)/2)-1);
      }
      else {
	j = browser_get_current_selected(pl_list);
	browser_rebuild_browser(pl_list, -1);
	browser_set_select(pl_list, j-1);
      }
    }
    else if(((int)data) == MOVEDN && (j < (gGlob->gui_playlist_num-1))) {
      char *tmplist[MAX_PLAYLIST_LENGTH];
      char *tmp;
      int i, k;

      tmp = gGlob->gui_playlist[j];
      for(i=0; i<j; i++) {
	tmplist[i] = gGlob->gui_playlist[i];
      }
      tmplist[i] = gGlob->gui_playlist[j+1];
      tmplist[i+1] = tmp;
      i += 2;
      for(k=i++; k<gGlob->gui_playlist_num; k++) {
	tmplist[k] = gGlob->gui_playlist[k];
      }
      for(i=0; i<gGlob->gui_playlist_num; i++)
	gGlob->gui_playlist[i] = tmplist[i];

      if(j >= (MAX_LIST-1)) {
	browser_rebuild_browser(pl_list, j);
	browser_set_select(pl_list, 1);
      }
      else {
	j = browser_get_current_selected(pl_list);
	browser_rebuild_browser(pl_list, -1);
	browser_set_select(pl_list, j+1);
      }      
    }
  }
}
/*
 * return 1 if playlist editor is ON
 */
int pl_is_running(void) {

  return pl_running;
}
/*
 * Return 1 if playlist editor is visible
 */
int pl_is_visible(void) {

  return pl_panel_visible;
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
    gGlob->gui_playlist_num = tmp_playlist_num;
    
    for(i = 0; i < tmp_playlist_num; i++)
      gGlob->gui_playlist[i] = tmp_playlist[i];
    
    browser_update_list(pl_list, gGlob->gui_playlist, gGlob->gui_playlist_num, 0);
    //    browser_rebuild_browser(pl_list, 0);
  }
  
}
/*
 * Save playlist to $HOME/.xinepl file
 */
static void pl_save_pl(widget_t *w, void *data) {
  FILE *plfile;
  char buf[1024], tmpbuf[256];
  int i;

  if(!gGlob->gui_playlist_num) return;

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
    
    for(i = 0; i < gGlob->gui_playlist_num; i++) {
      memset(&buf, 0, sizeof(buf));
      sprintf(buf, "%s\n", gGlob->gui_playlist[i]);
      fputs(buf, plfile);
    }
    
    sprintf(buf, "%s", "[END]\n");
    fputs(buf, plfile);
    
  }
  fclose(plfile);
  
}
/*
 * Handle autoplay buttons hitting
 */
void pl_scan_input(widget_t *w, void *ip) {
  /* FIXME
  if(xine_get_status(gGlob->gXine) == XINE_STOP) {
    
    // FIXME: unifying both
    if(!strcasecmp(((input_plugin_t*)ip)->get_identifier(), "DVD")) {
      char **list;
      int nfiles, i;
      
      if ((list = ((input_plugin_t*)ip)->get_autoplay_list (&nfiles))) {
	
	for (i=0; i<nfiles; i++) {
	  gGlob->gui_playlist[gGlob->gui_playlist_num + i] = list[i]; 
	}
	
	gGlob->gui_playlist_num += nfiles; 
	for (i=0; i<gGlob->gui_playlist_num; i++) {
	}
	gGlob->gui_playlist_cur = 0;
	gui_set_current_mrl(gGlob->gui_playlist[gGlob->gui_playlist_cur]);	
      }
    }
    else if(!strcasecmp(((input_plugin_t*)ip)->get_identifier(), "VCD")) {
      char **list;
      int nfiles, i;
      
      if ((list = ((input_plugin_t*)ip)->get_autoplay_list (&nfiles))) {
	
	for (i=0; i<nfiles; i++) {
	  gGlob->gui_playlist[gGlob->gui_playlist_num + i]     = list[i]; 
	}
	
	gGlob->gui_playlist_num += nfiles; 
	gGlob->gui_playlist_cur = 0;
	gui_set_current_mrl(gGlob->gui_playlist[gGlob->gui_playlist_cur]);
      }
    }
    //    pl_rebuild_buttons(0);
    browser_update_list(pl_list, gGlob->gui_playlist, gGlob->gui_playlist_num, 0);
  }
  */
}
/*
 * Raise pl_win
 */
void pl_raise_window(void) {
  
  if(pl_win) {
    if(pl_panel_visible && pl_running) {
      if(pl_running) {
	XMapRaised(gGlob->gDisplay, pl_win); 
/*  	XSetTransientForHint (gGlob->gDisplay, pl_win, gVideoWin); FIXME  */
      }
    } else {
      XUnmapWindow (gGlob->gDisplay, pl_win);
    }
  }
}
/*
 * Hide/show the pl panel
 */
void pl_toggle_panel_visibility (widget_t *w, void *data) {
  
  if (pl_panel_visible && pl_running) {
    pl_panel_visible = 0;
    XUnmapWindow (gGlob->gDisplay, pl_win);
  } else {
    if(pl_running) {
      pl_panel_visible = 1;
      XMapRaised(gGlob->gDisplay, pl_win); 
/*        XSetTransientForHint (gGlob->gDisplay, pl_win, gVideoWin); FIXME  */
    }
  }
}
/*
 * Handle X events here.
 */
void playlist_handle_event(XEvent *event) {
  XExposeEvent  *myexposeevent;
  static XEvent *old_event;
  /* FIXME
  if(event->xany.window == pl_win || event->xany.window == gVideoWin) {
    
    switch(event->type) {
    case Expose: {
      myexposeevent = (XExposeEvent *) event;
      
      if(event->xexpose.count == 0) {
	if (event->xany.window == pl_win)
	  paint_widget_list (pl_widget_list);
      }
    }
    break;
    
    case MotionNotify:
  */      
      /* printf ("MotionNotify\n"); */
/*        motion_notify_widget_list (pl_widget_list,  */
/*  				 event->xbutton.x, event->xbutton.y); */
      /* if window-moving is enabled move the window */
  /*      old_event = event;
      if (pl_move.enabled) {
	int x,y;
	x = (event->xmotion.x_root) 
	  + (event->xmotion.x_root - old_event->xmotion.x_root) 
	  - pl_move.offset_x;
	y = (event->xmotion.y_root) 
	  + (event->xmotion.y_root - old_event->xmotion.y_root) 
	  - pl_move.offset_y;
	
	if(event->xany.window == pl_win) {
	  XLOCK ();
	  XMoveWindow(gGlob->gDisplay, pl_win, x, y);
	  XUNLOCK ();
	  config_file_set_int ("x_playlist",x);
	  config_file_set_int ("y_playlist",y);
	}
      }
      break;
      
    case MappingNotify:
      // printf ("MappingNotify\n");
      XLOCK ();
      XRefreshKeyboardMapping((XMappingEvent *) event);
      XUNLOCK ();
      break;
      
      
    case ButtonPress: {
      XButtonEvent *bevent = (XButtonEvent *) event;
  */
      /* if no widget is hit enable moving the window */
  /*  if(bevent->window == pl_win)
	pl_move.enabled = !click_notify_widget_list (pl_widget_list, 
						     event->xbutton.x, 
						     event->xbutton.y, 0);
      if (pl_move.enabled) {
	pl_move.offset_x = event->xbutton.x;
	pl_move.offset_y = event->xbutton.y;
      }
    }
    break;
    
    case ButtonRelease:
      click_notify_widget_list (pl_widget_list, event->xbutton.x, 
				event->xbutton.y, 1);
				pl_move.enabled = 0; *//* disable moving the window       */  
  /*      break;
      
    case ClientMessage:
      if(event->xany.window == pl_win)
	gui_dnd_process_client_message (xdnd_pl_win, event);
      break;
      
    }
  }
*/
}
/*
 * Create playlist editor window
 */
void playlist_editor(void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  int                     screen;
  char                    title[] = {"Xine Playlist Editor"};
  Atom                    prop;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;

  /* This shouldn't be happend */
  if(pl_win) {
    /*  XLOCK (); FIXME  */
    XMapRaised(gGlob->gDisplay, pl_win); 
    /*  XUNLOCK(); FIXME  */
    pl_panel_visible = 1;
    pl_running = 1;
    return;
  }

  pl_running = 1;
  
  /*  XLOCK (); FIXME  */

  if (!(pl_bg_image = Imlib_load_image(gGlob->gImlib_data,
				       gui_get_skinfile("PlBG")))) {
    fprintf(stderr, "xine-playlist: couldn't find image for background\n");
    exit(-1);
  }

  screen = DefaultScreen(gGlob->gDisplay);
  /*  hint.x = config_file_lookup_int ("x_playlist", 200); FIXME  */
  /*  hint.y = config_file_lookup_int ("y_playlist", 100); FIXME  */
  hint.width = pl_bg_image->rgb_width;
  hint.height = pl_bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;
  pl_win = XCreateWindow (gGlob->gDisplay, DefaultRootWindow(gGlob->gDisplay), 
			  hint.x, hint.y, hint.width, hint.height, 0, 
			  CopyFromParent, CopyFromParent, 
			  CopyFromParent,
			  0, &attr);
  
  XSetStandardProperties(gGlob->gDisplay, pl_win, title, title,
			 None, NULL, 0, &hint);
  /*
   * wm, no border please
   */

  prop = XInternAtom(gGlob->gDisplay, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGlob->gDisplay, pl_win, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
/*    XSetTransientForHint (gGlob->gDisplay, pl_win, gVideoWin); FIXME  */

  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Playlist Editor";
    xclasshint->res_class = "Xine";
    XSetClassHint(gGlob->gDisplay, pl_win, xclasshint);
  }

  gc = XCreateGC(gGlob->gDisplay, pl_win, 0, 0);

  XSelectInput(gGlob->gDisplay, pl_win,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(gGlob->gDisplay, pl_win, wm_hint);
    XFree(wm_hint);
  }
  
  Imlib_apply_image(gGlob->gImlib_data, pl_bg_image, pl_win);
  XSync(gGlob->gDisplay, False); 

  /*  XUNLOCK (); FIXME  */
  
  if((xdnd_pl_win = (DND_struct_t *) xmalloc(sizeof(DND_struct_t))) != NULL) {
    gui_init_dnd(xdnd_pl_win);
    gui_dnd_set_callback (xdnd_pl_win, gui_dndcallback);
    gui_make_window_dnd_aware (xdnd_pl_win, pl_win);
  }

  /*
   * Widget-list
   */
  pl_widget_list = (widget_list_t *) xmalloc (sizeof (widget_list_t));
  pl_widget_list->l = gui_list_new ();
  pl_widget_list->focusedWidget = NULL;
  pl_widget_list->pressedWidget = NULL;
  pl_widget_list->win           = pl_win;
  pl_widget_list->gc            = gc;

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlMoveUp"),
						gui_get_skinY("PlMoveUp"),
						CLICK_BUTTON, "Move Up",
						pl_move_updown, (void*)MOVEUP, 
						gui_get_skinfile("PlMoveUp"),
						gui_get_ncolor("PlMoveUp"),
						gui_get_fcolor("PlMoveUp"),
						gui_get_ccolor("PlMoveUp")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlMoveDn"),
						gui_get_skinY("PlMoveDn"),
						CLICK_BUTTON, "Move Dn",
						pl_move_updown, (void*)MOVEDN, 
						gui_get_skinfile("PlMoveDn"),
						gui_get_ncolor("PlMoveDn"),
						gui_get_fcolor("PlMoveDn"),
						gui_get_ccolor("PlMoveDn")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlPlay"),
						gui_get_skinY("PlPlay"),
						CLICK_BUTTON, "Play",
						pl_play, NULL, 
						gui_get_skinfile("PlPlay"),
						gui_get_ncolor("PlPlay"),
						gui_get_fcolor("PlPlay"),
						gui_get_ccolor("PlPlay")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlDelete"),
						gui_get_skinY("PlDelete"),
						CLICK_BUTTON, "Delete",
						pl_delete, NULL, 
						gui_get_skinfile("PlDelete"),
						gui_get_ncolor("PlDelete"),
						gui_get_fcolor("PlDelete"),
						gui_get_ccolor("PlDelete")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlDeleteAll"),
						gui_get_skinY("PlDeleteAll"),
						CLICK_BUTTON, "Delete All",
						pl_delete_all, NULL, 
						gui_get_skinfile("PlDeleteAll"),
						gui_get_ncolor("PlDeleteAll"),
						gui_get_fcolor("PlDeleteAll"),
						gui_get_ccolor("PlDeleteAll")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlLoad"),
						gui_get_skinY("PlLoad"),
						CLICK_BUTTON, "Load",
						pl_load_pl, NULL, 
						gui_get_skinfile("PlLoad"),
						gui_get_ncolor("PlLoad"),
						gui_get_fcolor("PlLoad"),
						gui_get_ccolor("PlLoad")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlSave"),
						gui_get_skinY("PlSave"),
						CLICK_BUTTON, "Save",
						pl_save_pl, NULL, 
						gui_get_skinfile("PlSave"),
						gui_get_ncolor("PlSave"),
						gui_get_fcolor("PlSave"),
						gui_get_ccolor("PlSave")));

  gui_list_append_content (pl_widget_list->l, 
			   create_label_button (gui_get_skinX("PlDismiss"),
						gui_get_skinY("PlDismiss"),
						CLICK_BUTTON, "Dismiss",
						pl_exit, NULL, 
						gui_get_skinfile("PlDismiss"),
						gui_get_ncolor("PlDismiss"),
						gui_get_fcolor("PlDismiss"),
						gui_get_ccolor("PlDismiss")));


  gui_list_append_content (pl_widget_list->l, 
			   (pl_list = create_browser(pl_widget_list,
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
						     9, gGlob->gui_playlist_num, gGlob->gui_playlist,
						     handle_selection, NULL)));
  
  gui_list_append_content (pl_widget_list->l,
			   create_label (gui_get_skinX("AutoPlayLbl"),
					 gui_get_skinY("AutoPlayLbl"),
					 9, "Scan for:",
					 gui_get_skinfile("AutoPlayLbl")));
  /* FIXME   
  {
    int x, y, i, num_plugins;
    input_plugin_t *ip;
    widget_t *tmp;
    
    x = gui_get_skinX("AutoPlayBG");
    y = gui_get_skinY("AutoPlayBG");
    
    ip = xine_get_input_plugin_list (&num_plugins);
    fprintf (stderr, "%d input plugins ...\n",num_plugins);
    for (i = 0; i < num_plugins; i++) {
      fprintf (stderr, "plugin #%d : id=%s\n", i, ip->get_identifier());
      if(ip->get_capabilities() & INPUT_CAP_AUTOPLAY) {
	gui_list_append_content (pl_widget_list->l, 
		       (tmp =
			create_label_button (x, y,
					     CLICK_BUTTON,
					     ip->get_identifier(),
					     pl_scan_input, (void*)ip, 
					     gui_get_skinfile("AutoPlayBG"),
					     gui_get_ncolor("AutoPlayBG"),
					     gui_get_fcolor("AutoPlayBG"),
					     gui_get_ccolor("AutoPlayBG"))));
	y += widget_get_height(tmp) + 1;
      }
      ip++;
    }
  }
  */
  browser_update_list(pl_list, gGlob->gui_playlist, gGlob->gui_playlist_num, 0);

  XMapRaised(gGlob->gDisplay, pl_win); 

  pl_panel_visible = 1;

}
