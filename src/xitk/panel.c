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
 * xine panel related stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "xitk.h"

#include "utils.h"

#include "event.h"
#include "actions.h"
#include "control.h"
#include "playlist.h"
#include "videowin.h"
#include "parseskin.h"
#include "panel.h"

extern gGui_t     *gGui;

_panel_t          *panel;

/*
 * Toolkit event handler will call this function with new
 * coords of panel window.
 */
static void panel_store_new_position(int x, int y, int w, int h) {
 
  config_set_int("panel_x", x);
  config_set_int("panel_y", y);
}

/*
 * Update slider thread.
 */
static void *slider_loop(void *dummy) {

  pthread_detach(pthread_self());

  while(gGui->running) {
    if(panel_is_visible()) {
      if(gGui->xine) {
	if(xine_get_status(gGui->xine) != XINE_STOP) {
	  slider_set_pos(panel->widget_list, panel->slider_play, 
			 xine_get_current_position(gGui->xine));
	}
      }
    }
    sleep(3);
  }
  pthread_exit(NULL);
}

/*
 * Boolean about panel visibility.
 */
int panel_is_visible(void) {

  if(panel)
    return panel->visible;

  return 0;
}

/*
 * Show/Hide panel window.
 */
void panel_toggle_visibility (widget_t *w, void *data) {

  pl_toggle_visibility(NULL, NULL);

  if(!panel->visible && control_is_visible()) {}
  else control_toggle_panel_visibility(NULL, NULL);

  if (panel->visible) {
    
    if (video_window_is_visible ()) {
      panel->visible = 0;
      XUnmapWindow (gGui->display, gGui->panel_window);
    }
    
  } else {

    panel->visible = 1;
    XMapRaised(gGui->display, gGui->panel_window); 
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);

  }

  video_window_set_cursor_visibility (panel->visible);
  
  config_set_int("open_panel", panel->visible);
  
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause(void) {
  
  checkbox_set_state(panel->checkbox_pause, 
		     ((xine_get_status(gGui->xine)==XINE_PAUSE)?1:0), 
		     gGui->panel_window, panel->widget_list->gc);

}

/*
 * Reset the slider of panel window (set to 0).
 */
void panel_reset_slider (void) {
  slider_reset(panel->widget_list, panel->slider_play);
}

/*
 * Update audio/spu channel displayed informations.
 */
void panel_update_channel_display (void) {

  sprintf (panel->audiochan, "%3d", xine_get_audio_channel(gGui->xine));
  label_change_label (panel->widget_list, panel->audiochan_label, 
		      panel->audiochan);

  if(xine_get_spu_channel(gGui->xine) >= 0) 
    sprintf (panel->spuid, "%3d", xine_get_spu_channel (gGui->xine));
  else 
    sprintf (panel->spuid, "%3s", "off");

  label_change_label (panel->widget_list, panel->spuid_label, panel->spuid);
}

/*
 * Update displayed MRL according to the current one.
 */
void panel_update_mrl_display (void) {
  label_change_label (panel->widget_list, panel->title_label, gGui->filename);
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_cb(widget_t *w, void *data, int pos) {

  if(w == panel->slider_play) {
    gui_set_current_position (pos);
    if(xine_get_status(gGui->xine) != XINE_PLAY)
      slider_reset(panel->widget_list, panel->slider_play);
  }
  else if(w == panel->slider_mixer) {
    // TODO
  }
  else
    fprintf(stderr, "unknown widget slider caller\n");

  panel_check_pause();
}

/*
 * Handle X events here.
 */
void panel_handle_event(XEvent *event) {

  switch(event->type) {

  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;
    
  }
}

/*
 * Create buttons from information if input plugins autoplay featured. 
 * We couldn't do this into panel_init(), this function is
 * called before xine engine initialization.
 */
void panel_add_autoplay_buttons(void) {
  int x, y;
  int i = 0;
  char **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
  widget_t *tmp;
  
  x = gui_get_skinX("AutoPlayGUI");
  y = gui_get_skinY("AutoPlayGUI");
    
  while(autoplay_plugins[i] != NULL) {
    gui_list_append_content (panel->widget_list->l,
	     (tmp =
	      create_label_button (gGui->display, gGui->imlib_data, 
				   x, y,
				   CLICK_BUTTON,
				   autoplay_plugins[i],
				   pl_scan_input, NULL, 
				   gui_get_skinfile("AutoPlayGUI"),
				   gui_get_ncolor("AutoPlayGUI"),
				   gui_get_fcolor("AutoPlayGUI"),
				   gui_get_ccolor("AutoPlayGUI"))));
    x -= widget_get_width(tmp) + 1;
    i++;
  }
}

/*
 * Create the panel window, and fit all widgets in.
 */
void panel_init (void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  char                    title[] = {"Xine Panel"}; /* window-title     */
  Atom                    prop;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;


  if (gGui->panel_window)
    return ; /* panel already open  - FIXME: bring to foreground */

  panel = (_panel_t *) xmalloc(sizeof(_panel_t));

  XLockDisplay (gGui->display);
  
  /*
   * load bg image before opening window, so we can determine it's size
   */

  if (!(panel->bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 gui_get_skinfile("BackGround")))) {
    fprintf(stderr, "xine-panel: couldn't find image for background\n");
    exit(-1);
  }

  /*
   * open the panel window
   */

  hint.x = config_lookup_int ("panel_x", 200);
  hint.y = config_lookup_int ("panel_y", 100);
  hint.width = panel->bg_image->rgb_width;
  hint.height = panel->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  /*  
      printf ("imlib_data: %d visual : %d\n",gGui->imlib_data,gGui->imlib_data->x.visual);
      printf ("w : %d h : %d\n",hint.width, hint.height);
  */
  
  attr.override_redirect = True;
  gGui->panel_window = XCreateWindow (gGui->display, 
				      DefaultRootWindow(gGui->display), 
				      hint.x, hint.y,
				      hint.width, hint.height, 0, 
				      gGui->imlib_data->x.depth,
				      CopyFromParent, 
				      gGui->imlib_data->x.visual,
				      0, &attr);
  
  XSetStandardProperties(gGui->display, gGui->panel_window, title, title,
			 None, NULL, 0, &hint);

  XSelectInput(gGui->display, gGui->panel_window,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);

  /*
   * wm, no border please
   */

  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, gGui->panel_window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XSetTransientForHint (gGui->display, 
			gGui->panel_window, gGui->video_window);

  /* 
   * set wm properties 
   */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Panel";
    xclasshint->res_class = "Xine";
    XSetClassHint(gGui->display, gGui->panel_window, xclasshint);
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap   = gGui->icon;
    wm_hint->flags         = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->display, gGui->panel_window, wm_hint);
    XFree(wm_hint); /* CHECKME */
  }

  /*
   * set background image
   */
  
  gc = XCreateGC(gGui->display, gGui->panel_window, 0, 0);

  Imlib_apply_image(gGui->imlib_data, panel->bg_image, gGui->panel_window);

  /*
   * Widget-list
   */

  panel->widget_list                = widget_list_new();
  panel->widget_list->l             = gui_list_new ();
  panel->widget_list->focusedWidget = NULL;
  panel->widget_list->pressedWidget = NULL;
  panel->widget_list->win           = gGui->panel_window;
  panel->widget_list->gc            = gc;
 
  /* Check and place some extra images on GUI */
  gui_place_extra_images(panel->widget_list);

  //PREV-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Prev"),
					  gui_get_skinY("Prev"),
					  gui_nextprev, 
					  (void*)GUI_PREV, 
					  gui_get_skinfile("Prev")));

  //STOP-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Stop"),
					  gui_get_skinY("Stop"), gui_stop, 
					  NULL, gui_get_skinfile("Stop")));
  
  //PLAY-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Play"),
					  gui_get_skinY("Play"), gui_play,
					  NULL, gui_get_skinfile("Play")));

  // PAUSE-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   (panel->checkbox_pause = 
			    create_checkbox (gGui->display, gGui->imlib_data, 
					     gui_get_skinX("Pause"),
					     gui_get_skinY("Pause"), 
					     gui_pause, NULL, 
					     gui_get_skinfile("Pause"))));
  
  // NEXT-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Next"),
					  gui_get_skinY("Next"),
					  gui_nextprev, (void*)GUI_NEXT, 
					  gui_get_skinfile("Next")));

  //Eject Button
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Eject"),
					  gui_get_skinY("Eject"),
					  gui_eject, NULL, 
					  gui_get_skinfile("Eject")));

  // EXIT-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Exit"),
					  gui_get_skinY("Exit"), gui_exit,
					  NULL, gui_get_skinfile("Exit")));

  // Close-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			    create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Close"),
					  gui_get_skinY("Close"), 
					  panel_toggle_visibility,
					  NULL, gui_get_skinfile("Close"))); 
  // Fullscreen-BUTTON
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("FullScreen"),
					  gui_get_skinY("FullScreen"),
					  gui_toggle_fullscreen, NULL, 
					  gui_get_skinfile("FullScreen")));
  // Prev audio channel
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioNext"),
					  gui_get_skinY("AudioNext"),
					  gui_change_audio_channel,
					  (void*)GUI_NEXT, 
					  gui_get_skinfile("AudioNext")));
  // Next audio channel
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioPrev"),
					  gui_get_skinY("AudioPrev"),
					  gui_change_audio_channel,
					  (void*)GUI_PREV, 
					  gui_get_skinfile("AudioPrev")));

  // Prev spuid
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuNext"),
					  gui_get_skinY("SpuNext"),
					  gui_change_spu_channel,
					  (void*)GUI_NEXT, 
					  gui_get_skinfile("SpuNext")));
  // Next spuid
  gui_list_append_content (panel->widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuPrev"),
					  gui_get_skinY("SpuPrev"),
					  gui_change_spu_channel,
					  (void*)GUI_PREV, 
					  gui_get_skinfile("SpuPrev")));

  /* LABEL TITLE */
  gui_list_append_content (panel->widget_list->l, 
			   (panel->title_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TitleLabel"),
					  gui_get_skinY("TitleLabel"),
					  60, gGui->filename, 
					  gui_get_skinfile("TitleLabel"))));

  /* runtime label */
  gui_list_append_content (panel->widget_list->l, 
			   (panel->runtime_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TimeLabel"),
					  gui_get_skinY("TimeLabel"),
					  8, panel->runtime,
					  gui_get_skinfile("TimeLabel"))));

  /* Audio channel label */
  gui_list_append_content (panel->widget_list->l, 
			   (panel->audiochan_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioLabel"),
					  gui_get_skinY("AudioLabel"),
					  3, panel->audiochan, 
					  gui_get_skinfile("AudioLabel"))));

  /* Spuid label */
  gui_list_append_content (panel->widget_list->l, 
			   (panel->spuid_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuLabel"),
					  gui_get_skinY("SpuLabel"),
					  3, panel->spuid, 
					  gui_get_skinfile("SpuLabel"))));

  /* SLIDERS */
  gui_list_append_content (panel->widget_list->l, 
			   (panel->slider_play = 
			    create_slider(gGui->display, gGui->imlib_data, 
					  HSLIDER,
					  gui_get_skinX("SliderBGPlay"),
					  gui_get_skinY("SliderBGPlay"),
					  0, 65535, 1, 
					  gui_get_skinfile("SliderBGPlay"),
					  gui_get_skinfile("SliderFGPlay"),
					  NULL, NULL,
					  panel_slider_cb, NULL)));

  /* FIXME: Need to implement mixer control
  gui_list_append_content (panel->widget_list->l, 
			   (panel->slider_mixer = 
			    create_slider(gGui->display, gGui->imlib_data, 
			    VSLIDER, 
			    gui_get_skinX("SliderBGVol"),
			    gui_get_skinY("SliderBGVol"),
			    0, 100, 1, 
			    gui_get_skinfile("SliderBGVol"),
			    gui_get_skinfile("SliderFGVol"),
			    panel_slider_cb, NULL)));
  */

  gui_list_append_content (panel->widget_list->l, 
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

  gui_list_append_content (panel->widget_list->l, 
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

  /* 
   * show panel 
   */

  if (config_lookup_int("panel_visible", 1)) {
    XMapRaised(gGui->display, gGui->panel_window); 
    panel->visible = 1;
  } 
  else {
    panel->visible = 0;
  }

  panel->widget_key = widget_register_event_handler("panel", 
						    gGui->panel_window, 
						    panel_handle_event,
						    panel_store_new_position,
						    gui_dndcallback,
						    panel->widget_list);

  video_window_set_cursor_visibility (panel->visible);

  XUnlockDisplay (gGui->display);
  XSync(gGui->display, False); 

  pthread_create(&panel->slider_thread, NULL, slider_loop, NULL);
}
