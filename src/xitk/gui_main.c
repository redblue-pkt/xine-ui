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
 * XINE-PANEL 
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <errno.h>
#include "Imlib.h"
#include <pthread.h>
#include <signal.h>
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
#include "gui_playlist.h"
#include "gui_control.h"

#include "xine.h"
#include "utils.h"

#ifdef HAVE_LIRC
#include "lirc/lirc_client.h"

extern int  no_lirc;

/*  Functions prototypes */
static void init_lirc(void);
static void deinit_lirc(void);
static void *xine_lirc_loop(void *dummy);
#endif

#define NEXT     1
#define PREV     2

/*
 * global variables
 */
extern gGui_t          *gGui;

static gui_move_t        gui_move; 
static widget_list_t    *gui_widget_list;

static widget_t         *gui_slider_play;
static widget_t         *gui_slider_mixer;
static widget_t         *gui_checkbox_pause;

static widget_t         *gui_title_label;
static char              gui_runtime[20];
static widget_t         *gui_runtime_label;
static int               gui_panel_visible;
static int               gui_running;

static char              gui_audiochan[20];
static widget_t         *gui_audiochan_label;
static char              gui_spuid[20];
static widget_t         *gui_spuid_label;
static int               gui_ignore_status=0;
static int               cursor_visible;

#define MAX_UPDSLD 25
static int update_slider;

static int gui_exiting = 0;

/**
 * Configuration file lookup/set functions
 */
char *config_lookup_str(char *key, char *def) {

  return(gGui->config->lookup_str(gGui->config, key, def));
}

int config_lookup_int(char *key, int def) {

  return(gGui->config->lookup_int(gGui->config, key, def));
}

void config_set_str(char *key, char *value) {

  if(key)
    gGui->config->set_str(gGui->config, key, value);
}

void config_set_int(char *key, int value) {
  
  if(key)
  gGui->config->set_int(gGui->config, key, value);
}

void config_save(void) {

  gGui->config->save(gGui->config);
}

void config_reset(void) {

  gGui->config->read(gGui->config, gGui->configfile);
}
/*
 *
 *******/

/*
 * Unlock pause when status != XINE_PAUSE
 */
void check_pause(void) {
  
  if((xine_get_status(gGui->xine) != XINE_PAUSE)
     && (checkbox_get_state(gui_checkbox_pause)))
    checkbox_set_state(gui_checkbox_pause, 0, 
		       gGui->gui_panel_win, gui_widget_list->gc);
}

void gui_set_current_mrl(const char *mrl) {

  if(mrl)
    strcpy(gGui->gui_filename, mrl);
  else 
    sprintf(gGui->gui_filename, "DROP A FILE ON XINE");  
  
  label_change_label (gui_widget_list, gui_title_label, gGui->gui_filename);
}

/*
 * Callback-functions for widget-button click
 */
void gui_exit (widget_t *w, void *data) {

  fprintf(stderr, "xine-panel: EXIT\n");

  config_save();

  xine_exit(gGui->xine); 

  if(gGui->xdnd_panel_win)
    free(gGui->xdnd_panel_win);

  gui_running = 0;

#ifdef HAVE_LIRC
  if(gGui->lirc_enable) {
    pthread_cancel(gGui->lirc_thread);
    deinit_lirc();
  }
#endif

}

static void gui_handleSIG (int sig) {
  static XEvent myevent;
  Status status;

  signal (SIGINT, NULL);

  if (!gui_exiting) {
    switch (sig) {
    case SIGINT:
      gui_exiting = 1;
      
      printf ("\nsigint caught => bye\n");

      myevent.xkey.type        = KeyPress;
      myevent.xkey.state       = 0;
      myevent.xkey.keycode     = XK_Q;
      myevent.xkey.subwindow   = gGui->gui_panel_win;
      myevent.xkey.time        = 0;
      myevent.xkey.same_screen = True;
      
      status = XSendEvent (gGui->display, gGui->gui_panel_win, True, KeyPressMask, &myevent);
      
      XFlush (gGui->display);
      break;
    }
  }
}

void gui_play (widget_t *w, void *data) {

  fprintf(stderr, "xine-panel: PLAY\n");
  xine_play (gGui->xine, gGui->gui_filename, 0 );

  check_pause();
}

void gui_stop (widget_t *w, void *data) {

  gui_ignore_status = 1;
  xine_stop (gGui->xine);
  /* FIXME vo_set_logo_mode (1); */
  gui_ignore_status = 0; 
  slider_reset(gui_widget_list, gui_slider_play);
  check_pause();
}

void gui_dndcallback (char *filename) {

  if(filename) {
    gGui->gui_playlist_cur = gGui->gui_playlist_num++;
    gGui->gui_playlist[gGui->gui_playlist_cur] = strdup(filename);

    if((xine_get_status(gGui->xine) == XINE_STOP)) {
      gui_set_current_mrl(gGui->gui_playlist[gGui->gui_playlist_cur]);
      gui_play (NULL, NULL); 
      update_slider = MAX_UPDSLD;
    }

    pl_update_playlist();
  }
}

void gui_pause (widget_t *w, void *data, int state) {
  
  xine_pause(gGui->xine);
  check_pause();
  
  if(((int)data) == 1)
    checkbox_set_state(w, ((xine_get_status(gGui->xine)==XINE_PAUSE)?1:0), 
		       gGui->gui_panel_win, gui_widget_list->gc);
}

char *gui_get_next_mrl () {
  if (gGui->gui_playlist_cur < gGui->gui_playlist_num-1) {
    return gGui->gui_playlist[gGui->gui_playlist_cur + 1];
  } else
    return NULL;
}

void gui_notify_demux_branched () {
  gGui->gui_playlist_cur ++;
  strcpy(gGui->gui_filename, gGui->gui_playlist [gGui->gui_playlist_cur]);
  label_change_label (gui_widget_list, gui_title_label, gGui->gui_filename);
}

void gui_nextprev(widget_t *w, void *data) {

  if(((int)data) == NEXT) {
    gui_ignore_status = 1;
    xine_stop (gGui->xine);
    gui_ignore_status = 0;
    gui_status_callback (XINE_STOP);
  }
  else if(((int)data) == PREV) {
    gui_ignore_status = 1;
    xine_stop (gGui->xine);
    gGui->gui_playlist_cur--;
    if ((gGui->gui_playlist_cur>=0) && (gGui->gui_playlist_cur < gGui->gui_playlist_num)) {
      gui_set_current_mrl(gGui->gui_playlist[gGui->gui_playlist_cur]);
      xine_play (gGui->xine, gGui->gui_filename, 0 );
    } else {
      /* vo_set_logo_mode (1); FIXME */
      gGui->gui_playlist_cur = 0;
    }
    gui_ignore_status = 0;
  }

  check_pause();
  update_slider = MAX_UPDSLD;
}

void gui_eject(widget_t *w, void *data) {
  char *tmp_playlist[MAX_PLAYLIST_LENGTH];
  int i, new_num = 0;
  char *tok = NULL;
  
  if(gGui->gui_playlist_num && xine_eject(gGui->gui_filename)) {
    /*
     * If MRL is dvd:// or vcd:// remove all of them in playlist
     */
    if(!strncasecmp(gGui->gui_playlist[gGui->gui_playlist_cur], "dvd://", 6))
      tok = "dvd://";
    else if(!strncasecmp(gGui->gui_playlist[gGui->gui_playlist_cur], "vcd://", 6))
      tok = "vcd://";
    
    if(tok != NULL) {
      /* 
       * Store all of not maching entries
       */
      for(i=0; i < gGui->gui_playlist_num; i++) {
	if(strncasecmp(gGui->gui_playlist[i], tok, strlen(tok))) {
	  tmp_playlist[new_num] = gGui->gui_playlist[i];
	  new_num++;
	}
      }
      /*
       * Create new _cleaned_ playlist
       */
      memset(&gGui->gui_playlist, 0, sizeof(gGui->gui_playlist));
      for(i=0; i<new_num; i++)
	gGui->gui_playlist[i] = tmp_playlist[i];

      gGui->gui_playlist_num = new_num;

    }
    else {
      /*
       * Remove only the current MRL
       */
      for(i = gGui->gui_playlist_cur; i < gGui->gui_playlist_num; i++)
	gGui->gui_playlist[i] = gGui->gui_playlist[i+1];
      
      gGui->gui_playlist_num--;
      if(gGui->gui_playlist_cur) gGui->gui_playlist_cur--;
    }

    gui_set_current_mrl(gGui->gui_playlist [gGui->gui_playlist_cur]);
    pl_update_playlist();
  }
}

void gui_toggle_fullscreen(widget_t *w, void *data) {

  xine_set_window_fullscreen(gGui->xine,
			     !(xine_get_window_fullscreen(gGui->xine)));
  xine_set_display_cursor(gGui->xine, gui_panel_visible);
  
  cursor_visible = gui_panel_visible;
  if (gui_panel_visible)  {
    pl_raise_window();
    control_raise_window();
    XRaiseWindow (gGui->display, gGui->gui_panel_win);
    XSetTransientForHint (gGui->display, 
			  gGui->gui_panel_win, gGui->video_window);
  }
}

void gui_toggle_aspect(void) {

  /*

    FIXME

  vo_set_aspect (vo_get_aspect () + 1);
  */
  if (gui_panel_visible)  {
    XRaiseWindow (gGui->display, gGui->gui_panel_win);
    XSetTransientForHint (gGui->display, 
			  gGui->gui_panel_win, gGui->video_window);
  }
}

int is_gui_panel_visible(void) {
  return gui_panel_visible;
}

void gui_toggle_panel_visibility (widget_t *w, void *data) {

  pl_toggle_panel_visibility(NULL, NULL);

  if(!gui_panel_visible && control_is_visible()) {}
  else control_toggle_panel_visibility(NULL, NULL);

  if (gui_panel_visible) {

    if (xine_get_window_visible(gGui->xine)) {
      gui_panel_visible = 0;
      XUnmapWindow (gGui->display, gGui->gui_panel_win);
  }
    
  } else {

    gui_panel_visible = 1;
    XMapRaised(gGui->display, gGui->gui_panel_win); 
    XSetTransientForHint (gGui->display, 
			  gGui->gui_panel_win, gGui->video_window);
    update_slider = MAX_UPDSLD;

  }

  xine_set_display_cursor(gGui->xine, gui_panel_visible);
  cursor_visible = gui_panel_visible;
  config_set_int("open_panel", gui_panel_visible);
}

void gui_change_audio_channel(widget_t *w, void *data) {
  
  if(((int)data) == NEXT) {
    xine_select_audio_channel(gGui->xine,
			      (xine_get_audio_channel(gGui->xine) + 1));
  }
  else if(((int)data) == PREV) {
    if(xine_get_audio_channel(gGui->xine))
      xine_select_audio_channel(gGui->xine, 
				(xine_get_audio_channel(gGui->xine) - 1));
  }
  
  sprintf (gui_audiochan, "%3d", xine_get_audio_channel(gGui->xine));
  label_change_label (gui_widget_list, gui_audiochan_label, gui_audiochan);
}

void gui_change_spu_channel(widget_t *w, void *data) {
  
  if(((int)data) == NEXT) {
    xine_select_spu_channel(gGui->xine, 
			    (xine_get_spu_channel(gGui->xine) + 1));
  }
  else if(((int)data) == PREV) {
    if(xine_get_spu_channel(gGui->xine) >= 0) {
      /* 
       * Subtitle will be disabled; so hide it otherwise 
       * the latest still be showed forever.
       */
      /* FIXME
      if((xine_get_spu_channel(gGui->xine) - 1) == -1)
      	vo_hide_spu();
      */

      xine_select_spu_channel(gGui->xine, 
			      (xine_get_spu_channel(gGui->xine) - 1));
    }
  }

  
  if(xine_get_spu_channel(gGui->xine) >= 0) 
    sprintf (gui_spuid, "%3d", xine_get_spu_channel (gGui->xine));
  else 
    sprintf (gui_spuid, "%3s", "off");

  label_change_label (gui_widget_list, gui_spuid_label, gui_spuid);
}

void gui_set_current_position (int pos) {

  gui_ignore_status = 1;
  xine_stop (gGui->xine);
  xine_play (gGui->xine, gGui->gui_filename, pos);
  gui_ignore_status = 0;
  update_slider = MAX_UPDSLD;
  check_pause();
}

void gui_slider(widget_t *w, void *data, int pos) {

  if(w == gui_slider_play) {
    gui_set_current_position (pos);
    if(xine_get_status(gGui->xine) != XINE_PLAY)
      slider_reset(gui_widget_list, gui_slider_play);
  }
  else if(w == gui_slider_mixer) {
    // TODO
  }
  else
    fprintf(stderr, "unknown widget slider caller\n");

  check_pause();
}

void gui_playlist_show(widget_t *w, void *datan, int st) {

  if(!pl_is_running()) {
    playlist_editor();
  }
  else {
    pl_exit(NULL, NULL);
  }
}

void gui_control_show(widget_t *w, void *data, int st) {

  if(control_is_running() && !control_is_visible())
    control_toggle_panel_visibility(NULL, NULL);
  else if(!control_is_running())
    control_panel();
  else
    control_exit(NULL, NULL);
}

void gui_nothing (void)
{
	// Just do nothing ...
}

/*
 * Function for Loading Images, using ImLib
 */
 
void gui_open_panel (void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  int                     screen;
  char                    title[] = {"Xine Panel"}; /* window-title     */
  Atom                    prop;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;


  if (gGui->gui_panel_win)
    return ; /* panel already open  - FIXME: bring to foreground */

  XLockDisplay (gGui->display);
  
  /*
   * load bg image before opening window, so we can determine it's size
   */

  if (!(gGui->gui_bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 gui_get_skinfile("BackGround")))) {
    fprintf(stderr, "xine-panel: couldn't find image for background\n");
    exit(-1);
  }

  screen = DefaultScreen(gGui->display);

  hint.x = config_lookup_int ("x_panel", 200);
  hint.y = config_lookup_int ("y_panel", 100);

  hint.x = 200;
  hint.y = 100;
  hint.width = gGui->gui_bg_image->rgb_width;
  hint.height = gGui->gui_bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;
  gGui->gui_panel_win = XCreateWindow (gGui->display, DefaultRootWindow(gGui->display), 
				 hint.x, hint.y, hint.width, hint.height, 0, 
				 CopyFromParent, CopyFromParent, 
				 CopyFromParent,
				 0, &attr);
  
  XSetStandardProperties(gGui->display, gGui->gui_panel_win, title, title,
			 None, NULL, 0, &hint);
  /*
   * wm, no border please
   */

  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, gGui->gui_panel_win, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XSetTransientForHint (gGui->display, 
			gGui->gui_panel_win, gGui->video_window);

  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Panel";
    xclasshint->res_class = "Xine";
    XSetClassHint(gGui->display, gGui->gui_panel_win, xclasshint);
  }

  gc = XCreateGC(gGui->display, gGui->gui_panel_win, 0, 0);

  XSelectInput(gGui->display, gGui->gui_panel_win,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(gGui->display, gGui->gui_panel_win, wm_hint);
    XFree(wm_hint);
  }
  
  Imlib_apply_image(gGui->imlib_data, gGui->gui_bg_image, gGui->gui_panel_win);
  XSync(gGui->display, False); 

  XUnlockDisplay (gGui->display);

  /*
   * drag and drop
   */
  
  if((gGui->xdnd_panel_win = (DND_struct_t *) 
      xmalloc(sizeof(DND_struct_t))) != NULL) {
    gui_init_dnd(gGui->display, gGui->xdnd_panel_win);
    gui_dnd_set_callback (gGui->xdnd_panel_win, gui_dndcallback);
    gui_make_window_dnd_aware (gGui->xdnd_panel_win, gGui->gui_panel_win);
  }
  else {
    fprintf(stderr, "Cannot allocate memory: %s\n", strerror(errno));
    exit(-1);
  }
  /*
   * Widget-list
   */
  gui_widget_list = (widget_list_t *) xmalloc (sizeof (widget_list_t));
  gui_widget_list->l = gui_list_new ();
  gui_widget_list->focusedWidget = NULL;
  gui_widget_list->pressedWidget = NULL;
  gui_widget_list->win           = gGui->gui_panel_win;
  gui_widget_list->gc            = gc;
 
  /* Check and place some extra images on GUI */
  gui_place_extra_images(gui_widget_list);

  //PREV-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Prev"),
					  gui_get_skinY("Prev"),
					  gui_nextprev, 
					  (void*)PREV, 
					  gui_get_skinfile("Prev")));

  //STOP-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Stop"),
					  gui_get_skinY("Stop"), gui_stop, 
					  NULL, gui_get_skinfile("Stop")));
  
  //PLAY-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Play"),
					  gui_get_skinY("Play"), gui_play,
					  NULL, gui_get_skinfile("Play")));

  // PAUSE-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   (gui_checkbox_pause = 
			    create_checkbox (gGui->display, gGui->imlib_data, 
					     gui_get_skinX("Pause"),
					     gui_get_skinY("Pause"), 
					     gui_pause, NULL, 
					     gui_get_skinfile("Pause"))));
  
  // NEXT-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Next"),
					  gui_get_skinY("Next"),
					  gui_nextprev, (void*)NEXT, 
					  gui_get_skinfile("Next")));

  //Eject Button
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Eject"),
					  gui_get_skinY("Eject"),
					  gui_eject, NULL, 
					  gui_get_skinfile("Eject")));

  // EXIT-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Exit"),
					  gui_get_skinY("Exit"), gui_exit,
					  NULL, gui_get_skinfile("Exit")));

  // Close-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Close"),
					  gui_get_skinY("Close"), 
					  gui_toggle_panel_visibility,
					  NULL, gui_get_skinfile("Close"))); 
  // Fullscreen-BUTTON
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("FullScreen"),
					  gui_get_skinY("FullScreen"),
					  gui_toggle_fullscreen, NULL, 
					  gui_get_skinfile("FullScreen")));
  // Prev audio channel
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioNext"),
					  gui_get_skinY("AudioNext"),
					  gui_change_audio_channel,
					  (void*)NEXT, 
					  gui_get_skinfile("AudioNext")));
  // Next audio channel
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioPrev"),
					  gui_get_skinY("AudioPrev"),
					  gui_change_audio_channel,
					  (void*)PREV, 
					  gui_get_skinfile("AudioPrev")));

  // Prev spuid
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuNext"),
					  gui_get_skinY("SpuNext"),
					  gui_change_spu_channel,
					  (void*)NEXT, 
					  gui_get_skinfile("SpuNext")));
  // Next spuid
  gui_list_append_content (gui_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuPrev"),
					  gui_get_skinY("SpuPrev"),
					  gui_change_spu_channel,
					  (void*)PREV, 
					  gui_get_skinfile("SpuPrev")));

/*    gui_list_append_content (gui_widget_list->l,  */
/*  			   create_button (gGui->display, gGui->imlib_data, 
			                  gui_get_skinX("DVDScan"), */
/*  					  gui_get_skinY("DVDScan"), */
/*  					  gui_scan_input, */
/*  					  (void*)SCAN_DVD,  */
/*  					  gui_get_skinfile("DVDScan"))); */

/*    gui_list_append_content (gui_widget_list->l,  */
/*  			   create_button (gGui->display, gGui->imlib_data, 
			                  gui_get_skinX("VCDScan"), */
/*  					  gui_get_skinY("VCDScan"), */
/*  					  gui_scan_input, */
/*  					  (void*)SCAN_VCD,  */
/*  					  gui_get_skinfile("VCDScan"))); */

  /* LABEL TITLE */
  gui_list_append_content (gui_widget_list->l,
			   (gui_title_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TitleLabel"),
					  gui_get_skinY("TitleLabel"),
					  60, gGui->gui_filename, 
					  gui_get_skinfile("TitleLabel"))));

  /* runtime label */
  gui_list_append_content (gui_widget_list->l,
			   (gui_runtime_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TimeLabel"),
					  gui_get_skinY("TimeLabel"),
					  8, gui_runtime,
					  gui_get_skinfile("TimeLabel"))));

  /* Audio channel label */
  gui_list_append_content (gui_widget_list->l,
			   (gui_audiochan_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioLabel"),
					  gui_get_skinY("AudioLabel"),
					  3, gui_audiochan, 
					  gui_get_skinfile("AudioLabel"))));

  /* Spuid label */
  gui_list_append_content (gui_widget_list->l,
			   (gui_spuid_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuLabel"),
					  gui_get_skinY("SpuLabel"),
					  3, gui_spuid, 
					  gui_get_skinfile("SpuLabel"))));

  /* SLIDERS */
  gui_list_append_content (gui_widget_list->l,
			   (gui_slider_play = 
			    create_slider(gGui->display, gGui->imlib_data, 
					  HSLIDER,
					  gui_get_skinX("SliderBGPlay"),
					  gui_get_skinY("SliderBGPlay"),
					  0, 65535, 1, 
					  gui_get_skinfile("SliderBGPlay"),
					  gui_get_skinfile("SliderFGPlay"),
					  NULL, NULL,
					  gui_slider, NULL)));

  /* FIXME: Need to implement mixer control
  gui_list_append_content (gui_widget_list->l,
			   (gui_slider_mixer = 
			    create_slider(gGui->display, gGui->imlib_data, 
			    VSLIDER, 
			    gui_get_skinX("SliderBGVol"),
			    gui_get_skinY("SliderBGVol"),
			    0, 100, 1, 
			    gui_get_skinfile("SliderBGVol"),
			    gui_get_skinfile("SliderFGVol"),
			    gui_slider, NULL)));
  */

  /* FIXME
  {  
    int x, y, i, num_plugins;
    input_plugin_t *ip;
    widget_t *tmp;
    
    x = gui_get_skinX("AutoPlayGUI");
    y = gui_get_skinY("AutoPlayGUI");
    
    ip = xine_get_input_plugin_list (&num_plugins);
    fprintf (stderr, "%d input plugins ...\n",num_plugins);
    for (i = 0; i < num_plugins; i++) {
      fprintf (stderr, "plugin #%d : id=%s\n", i, ip->get_identifier());
      if(ip->get_capabilities() & INPUT_CAP_AUTOPLAY) {
	gui_list_append_content (gui_widget_list->l,
		       (tmp =
			create_label_button (gGui->display, gGui->imlib_data, 
			                     x, y,
					     CLICK_BUTTON,
					     ip->get_identifier(),
					     pl_scan_input, (void*)ip, 
					     gui_get_skinfile("AutoPlayGUI"),
					     gui_get_ncolor("AutoPlayGUI"),
					     gui_get_fcolor("AutoPlayGUI"),
					     gui_get_ccolor("AutoPlayGUI"))));
	x -= widget_get_width(tmp) + 1;
      }
      ip++;
    }
  }
  */


  gui_list_append_content (gui_widget_list->l, 
			   create_label_button (gGui->display, 
						gGui->imlib_data, 
						gui_get_skinX("PlBtn"),
						gui_get_skinY("PlBtn"),
						CLICK_BUTTON,
						"Playlist",
						gui_playlist_show, 
						NULL, 
						gui_get_skinfile("PlBtn"),
						gui_get_ncolor("PlBtn"),
						gui_get_fcolor("PlBtn"),
						gui_get_ccolor("PlBtn")));


  gui_list_append_content (gui_widget_list->l, 
			   create_label_button (gGui->display, 
						gGui->imlib_data, 
						gui_get_skinX("CtlBtn"),
						gui_get_skinY("CtlBtn"),
						CLICK_BUTTON,
						"Setup",
						gui_control_show, 
						NULL, 
						gui_get_skinfile("CtlBtn"),
						gui_get_ncolor("CtlBtn"),
						gui_get_fcolor("CtlBtn"),
						gui_get_ccolor("CtlBtn")));

}

void gui_handle_event (XEvent *event) {
  XKeyEvent      mykeyevent;
  XExposeEvent  *myexposeevent;
  KeySym         mykey;
  static XEvent *old_event;
  char kbuf[256];
  int len;

  playlist_handle_event(event);
  control_handle_event(event);

  switch(event->type) {
  case Expose: {
    /* printf ("Expose\n"); */
    myexposeevent = (XExposeEvent *) event;

    if(event->xexpose.count == 0) {
      if (myexposeevent->window == gGui->video_window)
	xine_window_handle_event(gGui->xine, (void *)event);
      else if (event->xany.window == gGui->gui_panel_win)
	paint_widget_list (gui_widget_list);
    }
  }
  break;
    
  case MotionNotify:

    /* printf ("MotionNotify\n"); */
    if(event->xany.window == gGui->gui_panel_win) {
      motion_notify_widget_list (gui_widget_list, event->xbutton.x, event->xbutton.y);
      /* if window-moving is enabled move the window */
      old_event = event;
      if (gui_move.enabled) {
	int x,y;
	x = (event->xmotion.x_root) + ( event->xmotion.x_root - old_event->xmotion.x_root) - gui_move.offset_x;
	y = (event->xmotion.y_root) + ( event->xmotion.y_root - old_event->xmotion.y_root) - gui_move.offset_y;
	
	if(event->xany.window == gGui->gui_panel_win) {
	  XLockDisplay (gGui->display);
	  XMoveWindow(gGui->display, gGui->gui_panel_win, x, y);
	  XUnlockDisplay (gGui->display);
	  config_set_int ("x_panel", x);
	  config_set_int ("y_panel", y);
	}
      }
    }
    break;

  case MappingNotify:
    /* printf ("MappingNotify\n");*/
    XLockDisplay (gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay (gGui->display);
    break;

  case DestroyNotify:
    printf ("DestroyNotify\n"); 
    if(event->xany.window == gGui->gui_panel_win 
       || event->xany.window == gGui->video_window) {
      xine_exit (gGui->xine);
      gui_running = 0;
    }
    break;
    
  case VisibilityNotify:
    if(event->xany.window == gGui->video_window)
      xine_window_handle_event(gGui->xine, (void *)event);
    break;  

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    /* printf ("ButtonPress\n"); */

    /* printf ("button: %d\n",bevent->button); */

    if ((bevent->button==3) && (bevent->window == gGui->video_window)) {
      gui_toggle_panel_visibility (NULL, NULL);
      break;
    }
    
    /* if no widget is hit enable moving the window */
    if(bevent->window == gGui->gui_panel_win) {
      gui_move.enabled = !click_notify_widget_list (gui_widget_list, 
						    event->xbutton.x, 
						    event->xbutton.y, 0);
      if (gui_move.enabled) {
	gui_move.offset_x = event->xbutton.x;
	gui_move.offset_y = event->xbutton.y;
      }
    }
  }
  break;

  case ButtonRelease:
    /* printf ("ButtonRelease\n");*/
    if(event->xany.window == gGui->gui_panel_win) {
      click_notify_widget_list (gui_widget_list, event->xbutton.x, 
				event->xbutton.y, 1);
      gui_move.enabled = 0; /* disable moving the window       */  
    }
    break;

  case KeyPress:
    mykeyevent = event->xkey;

    /* hack: exit on ctrl-c */
    if (gui_exiting) {
      gui_exit(NULL, NULL);
      break;
    }

    /* printf ("KeyPress (state : %d, keycode: %d)\n", mykeyevent.state, mykeyevent.keycode);  */
      
    XLockDisplay (gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay (gGui->display);

    switch (mykey) {
    
    case XK_greater:
    case XK_period:
      gui_change_spu_channel(NULL, (void*)NEXT);
      break;
      
    case XK_less:
    case XK_comma:
      gui_change_spu_channel(NULL, (void*)PREV);
      break;

    case XK_c:
    case XK_C:
      gui_control_show(NULL, NULL, 0);
      break;

    case XK_h:
    case XK_H:
      xine_set_window_visible(gGui->xine, 
			      !(xine_get_window_visible(gGui->xine)));
      break;
      
    case XK_plus:
    case XK_KP_Add:
      gui_change_audio_channel(NULL, (void*)NEXT);
      break;
      
    case XK_minus:
    case XK_KP_Subtract:
      gui_change_audio_channel(NULL, (void*)PREV);
      break;
      
    case XK_space:
    case XK_P:
    case XK_p:
      gui_pause (gui_checkbox_pause, (void*)(1), 0); 
      break;
      
    case XK_g:
    case XK_G:
      gui_toggle_panel_visibility (NULL, NULL);
      break;

    case XK_f:
    case XK_F:
      gui_toggle_fullscreen(NULL, NULL);
      break;

    case XK_a:
    case XK_A:
      gui_toggle_aspect();
      break;

    case XK_q:
    case XK_Q:
      gui_exit(NULL, NULL);
      break;

    case XK_Return:
    case XK_KP_Enter:
      gui_play(NULL, NULL);
      break;

    case XK_Control_L:
    case XK_Control_R:
      if(!gui_panel_visible) {
	cursor_visible = !cursor_visible;
	xine_set_display_cursor(gGui->xine, cursor_visible);
      }
      break;

    case XK_Next:
      gui_nextprev(NULL, (void*)NEXT);
      break;

    case XK_Prior:
      gui_nextprev(NULL, (void*)PREV);
      break;
      
    case XK_e:
    case XK_E:
      gui_eject(NULL, NULL);
      break;

    case XK_1:
    case XK_KP_1:
      gui_set_current_position (6553);
      break;

    case XK_2:
    case XK_KP_2:
      gui_set_current_position (13107);
      break;

    case XK_3:
    case XK_KP_3:
      gui_set_current_position (19660);
      break;

    case XK_4:
    case XK_KP_4:
      gui_set_current_position (26214);
      break;

    case XK_5:
    case XK_KP_5:
      gui_set_current_position (32767);
      break;

    case XK_6:
    case XK_KP_6:
      gui_set_current_position (39321);
      break;

    case XK_7:
    case XK_KP_7:
      gui_set_current_position (45874);
      break;

    case XK_8:
    case XK_KP_8:
      gui_set_current_position (52428);
      break;

    case XK_9:
    case XK_KP_9:
      gui_set_current_position (58981);
      break;

    case XK_0:
    case XK_KP_0:
      gui_set_current_position (0);
      break;
      /* FIXME
    case XK_Left:
      metronom_set_av_offset (metronom_get_av_offset () - 3600);
      break;
    case XK_Right:
      metronom_set_av_offset (metronom_get_av_offset () + 3600);
      break;
    case XK_Home:
      metronom_set_av_offset (0);
      break;
      */
    }

    break;

  case ConfigureNotify:
    xine_window_handle_event(gGui->xine, (void *)event);

    /* printf ("ConfigureNotify\n"); */
    /*  background */
    /* CHECKME
    XLOCK ();
    Imlib_apply_image(gGui->imlib_data, gGui->gui_bg_image, gGui->gui_panel_win);
    XUNLOCK ();
    paint_widget_list (gui_widget_list);
    XLOCK ();
    XSync(gGui->display, False);
    XUNLOCK ();
    */
    break;

  case ClientMessage:
    /* printf ("ClientMessage\n"); */
    if(event->xany.window == gGui->gui_panel_win)
      gui_dnd_process_client_message (gGui->xdnd_panel_win, event);
    else if(event->xany.window == gGui->video_window)
      xine_window_handle_event(gGui->xine, (void *)event);

    break;
    /*
  default:
    printf("Got event: %i\n", event->type);
    */
  }

}

void gui_status_callback (int nStatus) {

  if (gui_ignore_status)
    return;

  /* printf ("gui status callback : %d\n", nStatus); */

  if (gui_panel_visible) {
    /* FIXME
    sprintf (gui_runtime, "%02d:%02d:%02d", nVideoHours, nVideoMins, nVideoSecs);

    label_change_label (gui_widget_list, gui_runtime_label, gui_runtime);
    */

    if(update_slider > MAX_UPDSLD) {
      slider_set_pos(gui_widget_list, gui_slider_play, 
		     xine_get_current_position(gGui->xine));
    
      update_slider = 0;
    }
    update_slider++;

  }

  if (nStatus == XINE_STOP) {
    gGui->gui_playlist_cur++;
    slider_reset(gui_widget_list, gui_slider_play);
    if (gGui->gui_playlist_cur < gGui->gui_playlist_num) {
      gui_set_current_mrl(gGui->gui_playlist[gGui->gui_playlist_cur]);
      xine_play (gGui->xine, gGui->gui_filename, 0 );
    } else {
      
      if(gGui->autoplay_options & QUIT_ON_STOP)
	gui_exit(NULL, NULL);
      
      /* FIXME
      vo_set_logo_mode (1); 
      */
      gGui->gui_playlist_cur--;
    }
  }
}

#define SEND_KEVENT(key) {                                                    \
     static XEvent startevent;                                                \
     startevent.type = KeyPress;                                              \
     startevent.xkey.type = KeyPress;                                         \
     startevent.xkey.send_event = True;                                       \
     startevent.xkey.display = gGui->display;                               \
     startevent.xkey.window = gGui->gui_panel_win;                           \
     startevent.xkey.keycode = XKeysymToKeycode(gGui->display, key);        \
     XSendEvent(gGui->display, gGui->gui_panel_win, True, KeyPressMask,    \
                &startevent);                                                 \
   }

void gui_start (int nfiles, char *filenames[]) {
  XEvent  myevent;
  int     i;
  struct sigaction action;

  /*
   * moving the window
   */

  gui_move.enabled  = 0;
  gui_move.offset_x = 0;
  gui_move.offset_y = 0; 

  for (i=0; i<nfiles; i++)
    gGui->gui_playlist[i]     = filenames[i];
  gGui->gui_playlist_num = nfiles; 
  gGui->gui_playlist_cur = 0;

  if (nfiles)
    strcpy(gGui->gui_filename, gGui->gui_playlist [gGui->gui_playlist_cur]);
  else 
    sprintf(gGui->gui_filename, "DROP A FILE ON XINE");

  /* FIXME
  sprintf (gui_runtime, "%02d:%02d:%02d", nVideoHours, nVideoMins, nVideoSecs);
  */
  sprintf (gui_audiochan, "%3d", xine_get_audio_channel(gGui->xine));
  if(xine_get_spu_channel(gGui->xine) >= 0) 
    sprintf (gui_spuid, "%3d", xine_get_spu_channel(gGui->xine));
  else
    sprintf (gui_spuid, "%3s", "off");

  gGui->imlib_data = Imlib_init (gGui->display);

  gGui->video_window = (Window) xine_get_window_output(gGui->xine);

  /*
   * setup panel
   */
  gui_open_panel ();

  XLockDisplay (gGui->display);
  
  if (config_lookup_int("open_panel", 1)) {
    XMapRaised(gGui->display, gGui->gui_panel_win); 
    gui_panel_visible = 1;
  } 
  else {
    gui_panel_visible = 0;
  }

  xine_set_display_cursor(gGui->xine, gui_panel_visible);
  cursor_visible = gui_panel_visible;
  XUnlockDisplay (gGui->display);
  
  /*  The user request "play on start" */
  if(gGui->autoplay_options & PLAY_ON_START) {
    /* probe DVD && VCD  */
    int i = 0;
    char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
    
    perr("autoplay\n");
    
    while(autoplay_plugins[i] != NULL) {
      
      perr("try '%s'\n", autoplay_plugins[i]);
      if(gGui->autoplay_options & PLAY_FROM_DVD)

	if(!strcasecmp(autoplay_plugins[i], "DVD")) {
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine, "DVD");
	  int j = 0;
	    
	  while(autoplay_mrls[j]) {
	    printf("DVD: autoplaylist[%d]='%s'\n", j, autoplay_mrls[j]);
	    j++;
	  }
	}

      if(gGui->autoplay_options & PLAY_FROM_VCD)
	if(!strcasecmp(autoplay_plugins[i], "VCD")) {
	  char **autoplay_mrls = xine_get_autoplay_mrls (gGui->xine, "VCD");
	  int j = 0;
	    
	  while(autoplay_mrls[j]) {
	    printf("VCD: autoplaylist[%d]='%s'\n", j, autoplay_mrls[j]);
	    j++;
	  }
	}

      i++;
    }

    /*  The user wants to hide control panel  */
    if(gui_panel_visible && (gGui->autoplay_options & HIDEGUI_ON_START))
      SEND_KEVENT(XK_G);
    
    /*  The user wants to see in fullscreen mode  */
    if(gGui->autoplay_options & FULL_ON_START)
      SEND_KEVENT(XK_F);
    
    if(gGui->gui_playlist[0] != NULL)
      SEND_KEVENT(XK_Return);

    gGui->autoplay_options |= PLAYED_ON_START;    
  }

  /* install sighandler */
  action.sa_handler = gui_handleSIG;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGINT, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGINT) failed: %s\n", strerror(errno));
  }

  /*
   * event loop
   */

  gui_running = 1;

#ifdef HAVE_LIRC
  if(!no_lirc) {
    init_lirc();
    if(gGui->lirc_enable) {
      pthread_create (&gGui->lirc_thread, NULL, xine_lirc_loop, NULL) ;
      printf ("gui_main: LIRC thread created\n");
    }
  }
#endif

  while (gui_running) {
    //    fprintf (stderr, "gui: XNextEvent\n");
    XNextEvent (gGui->display, &myevent) ;
    //    fprintf (stderr, "gui: =>handle event\n");

    gui_handle_event (&myevent) ;
  }
}

#ifdef HAVE_LIRC
/*
 * Check if rxcmd is present in tockens[] array.
 * On success, return the position in tokens[] array, and set retunk to NULL.
 * On failure, return -1 and set retunk to the unknown rxcmd.
 */
static int handle_lirc_command(char **rxcmd, 
			       const char **tokens, char **retunk) {
  int ret;

  if(*rxcmd == NULL)
    return -1;
  
  for(ret = 0; tokens[ret] != NULL; ret++) {
    if(!(strcasecmp(*rxcmd, tokens[ret]))) {
      *retunk = NULL;
      return ret;
    }
  }
  
  *retunk = *rxcmd;
  return -1;
}

/*
 * Lirc thread loop
 */
static void *xine_lirc_loop(void *dummy) {
  char *code, *c, *uc = NULL;
  int ret, lsub;
  const char *lirc_commands[] = {
    "0%",
    "10%",
    "20%",
    "30%",
    "40%",
    "50%",
    "60%",
    "70%",
    "80%",
    "90%",
    "quit",
    "play", 
    "pause",
    "next",
    "prev",
    "spu+",
    "spu-",
    "audio+",
    "audio-",
    "eject",
    "fullscr",
    "hidegui",
    "hideoutput",
    NULL
  };
  enum action {
    ZEROP,
    TENP,
    TWENTYP,
    THIRTYP,
    FOURTYP,
    FIVETYP,
    SIXTYP,
    SEVENTYP,
    HEIGHTYP,
    NINETYP,
    lQUIT,
    lPLAY,
    lPAUSE,
    lNEXT,
    lPREV,
    lSPUnext,
    lSPUprev,
    lAUDIOnext,
    lAUDIOprev,
    lEJECT,
    lFULLSCR,
    lHIDEGUI,
    lHIDEOUTPUT
  };
  

  while(gui_running) {
    
    pthread_testcancel();
    
    while(lirc_nextcode(&code) == 0) {
      
      pthread_testcancel();
      
      if(code == NULL) 
	break;
      
      pthread_testcancel();
      
      while((ret = lirc_code2char(gGui->xlirc_config, code, &c)) == 0 
	    && c != NULL) {
	//fprintf(stdout, "Command Received = '%s'\n", c);
	
	if((lsub = handle_lirc_command(&c, lirc_commands, &uc)) != -1) {
	  switch(lsub) {

	  case ZEROP:
	  case TENP:
	  case TWENTYP:
	  case THIRTYP:
	  case FOURTYP:
	  case FIVETYP:
	  case SIXTYP:
	  case SEVENTYP:
	  case HEIGHTYP:
	  case NINETYP:
	    gui_set_current_position ((6553 * lsub));
	    break;


	  case lQUIT:
	    // Dont work
	    //gui_exit(NULL, NULL);
	    kill(getpid(), SIGINT);
	    break;

	  case lPLAY:
	    if(xine_get_status(gGui->xine) != XINE_PLAY)
	      gui_play(NULL, NULL);
	    else
	      gui_stop(NULL, NULL);
	    break;

	  case lPAUSE:
	    gui_pause (gui_checkbox_pause, (void*)(1), 0); 
	    break;

	  case lNEXT:
	    gui_nextprev(NULL, (void*)NEXT);
 	    break;

	  case lPREV:
	    gui_nextprev(NULL, (void*)PREV);
	    break;

	  case lSPUnext:
	    gui_change_spu_channel(NULL, (void*)NEXT);
	    break;

	  case lSPUprev:
	    gui_change_spu_channel(NULL, (void*)PREV);
	    break;
	    
	  case lAUDIOnext:
	    gui_change_audio_channel(NULL, (void*)NEXT);
	    break;

	  case lAUDIOprev:
	    gui_change_audio_channel(NULL, (void*)PREV);
	    break;

	  case lEJECT:
	    gui_eject(NULL, NULL);
	    break;

	  case lFULLSCR:
	    gui_toggle_fullscreen(NULL, NULL);
	    break;

	  case lHIDEGUI:
	    gui_toggle_panel_visibility (NULL, NULL);
	    break;

	  case lHIDEOUTPUT:
	    xine_set_window_visible(gGui->xine, 
				    !(xine_get_window_visible(gGui->xine)));
	    break;
	  }
	}
	
	/* FIXME
	   if(uc) { */
	  /* 
	   * Check here if the unknown IR order match with
	   * ID of one of input plugins.
	   */
	/* FIXME
	  int i, num_plugins;
	  input_plugin_t *ip;
	  
	  ip = xine_get_input_plugin_list (&num_plugins);
	  for (i = 0; i < num_plugins; i++) {
	    
	    if(ip->get_capabilities() & INPUT_CAP_AUTOPLAY) {
	      
	      if(!strcasecmp(ip->get_identifier(), uc)) {
		pl_scan_input(NULL, ip);
	      }
	    }
	    ip++;
	  }
	  }*/
	
      }
      paint_widget_list (gui_widget_list);
      free(code);

      if(ret == -1) 
	break;

    }
  }
  
  return NULL;
}

/*
 * Initialize lirc support
 */
static void init_lirc(void) {
  /*  int flags; */

  if((gGui->lirc_fd = lirc_init("xine", 1)) == -1) {
    gGui->lirc_enable = 0;
    return;
  }
  /*
  else {
    flags = fcntl(gGui->lirc_fd, F_GETFL, 0);
    if(flags != -1)
      fcntl(gGui->lirc_fd, F_SETFL, flags|FASYNC|O_NONBLOCK);
  }
  */

  if(lirc_readconfig(NULL, &gGui->xlirc_config, NULL) != 0) {
    gGui->lirc_enable = 0;
    return;
  }

  gGui->lirc_enable = 1;
}

/*
 * De-init lirc support
 */
static void deinit_lirc(void) {

  if(gGui->lirc_enable) {
    lirc_freeconfig(gGui->xlirc_config);
    lirc_deinit();
  }
}
#endif
