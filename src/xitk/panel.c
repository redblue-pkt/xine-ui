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
 * xine panel related stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"

#include "event.h"
#include "actions.h"
#include "control.h"
#include "playlist.h"
#include "videowin.h"
#include "parseskin.h"

extern gGui_t          *gGui;

widget_list_t          *panel_widget_list;

static widget_t        *panel_title_label;
static widget_t        *panel_runtime_label;
static widget_t        *panel_slider_play;
static widget_t        *panel_slider_mixer;
widget_t               *panel_checkbox_pause;
static int              panel_visible;
static char             panel_runtime[20];
static gui_move_t       panel_move; 
static char             panel_audiochan[20];
static widget_t        *panel_audiochan_label;
static char             panel_spuid[20];
static widget_t        *panel_spuid_label;

#define MAX_UPDSLD 25
static int              panel_slider_timer; /* repaint slider if slider_timer<=0 */


int panel_is_visible(void) {
  return panel_visible;
}

void panel_toggle_visibility (widget_t *w, void *data) {

  pl_toggle_visibility(NULL, NULL);

  if(!panel_visible && control_is_visible()) {}
  else control_toggle_panel_visibility(NULL, NULL);

  if (panel_visible) {
    
    if (video_window_is_visible ()) {
      panel_visible = 0;
      XUnmapWindow (gGui->display, gGui->panel_window);
    }
    
  } else {

    panel_visible = 1;
    XMapRaised(gGui->display, gGui->panel_window); 
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);

  }

  video_window_set_cursor_visibility (panel_visible);

  config_set_int("open_panel", panel_visible);

}

void panel_check_pause(void) {
  
  checkbox_set_state(panel_checkbox_pause, ((xine_get_status(gGui->xine)==XINE_PAUSE)?1:0), 
		     gGui->panel_window, panel_widget_list->gc);

}

void panel_reset_slider () {
  slider_reset(panel_widget_list, panel_slider_play);
}

void panel_update_channel_display () {

  sprintf (panel_audiochan, "%3d", xine_get_audio_channel(gGui->xine));
  label_change_label (panel_widget_list, panel_audiochan_label, panel_audiochan);

  if(xine_get_spu_channel(gGui->xine) >= 0) 
    sprintf (panel_spuid, "%3d", xine_get_spu_channel (gGui->xine));
  else 
    sprintf (panel_spuid, "%3s", "off");

  label_change_label (panel_widget_list, panel_spuid_label, panel_spuid);
}

void panel_update_mrl_display () {
  label_change_label (panel_widget_list, panel_title_label, gGui->filename);
}

void panel_update_slider () {

  if (panel_is_visible()) {
    if(panel_slider_timer > MAX_UPDSLD) {
      slider_set_pos(panel_widget_list, panel_slider_play, 
		     xine_get_current_position(gGui->xine));
    
      panel_slider_timer = 0;
    }
    panel_slider_timer++;
  }
}

static void panel_slider_cb(widget_t *w, void *data, int pos) {

  if(w == panel_slider_play) {
    gui_set_current_position (pos);
    if(xine_get_status(gGui->xine) != XINE_PLAY)
      slider_reset(panel_widget_list, panel_slider_play);
  }
  else if(w == panel_slider_mixer) {
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

  XExposeEvent  *myexposeevent;
  static XEvent *old_event;

  if(event->xany.window == gGui->panel_window || event->xany.window == gGui->video_window) {
    
    switch(event->type) {
    case Expose: {
      myexposeevent = (XExposeEvent *) event;
      
      if(event->xexpose.count == 0) {
	if (event->xany.window == gGui->panel_window)
	  paint_widget_list (panel_widget_list);
      }
    }
    break;
    
    case MotionNotify:      

      if(event->xany.window == gGui->panel_window) {

	/* printf ("MotionNotify\n"); */
	motion_notify_widget_list (panel_widget_list,
				   event->xbutton.x, event->xbutton.y);
	/* if window-moving is enabled move the window */
	old_event = event;
	if (panel_move.enabled) {
	  int x,y;
	  x = (event->xmotion.x_root) 
	    + (event->xmotion.x_root - old_event->xmotion.x_root) 
	    - panel_move.offset_x;
	  y = (event->xmotion.y_root) 
	    + (event->xmotion.y_root - old_event->xmotion.y_root) 
	    - panel_move.offset_y;
	  
	  if(event->xany.window == gGui->panel_window) {
	    XLockDisplay (gGui->display);
	    XMoveWindow(gGui->display,gGui->panel_window, x, y);
	    XUnlockDisplay (gGui->display);
	    config_set_int ("x_panel", x);
	    config_set_int ("y_panel", y);
	  }
	}
      }
      break;
      
    case ButtonPress: {
      XButtonEvent *bevent = (XButtonEvent *) event;
      
      /* if no widget is hit enable moving the window */
      if(bevent->window == gGui->panel_window) {
	panel_move.enabled = !click_notify_widget_list (panel_widget_list, 
							event->xbutton.x, 
							event->xbutton.y, 0);
	if (panel_move.enabled) {
	  panel_move.offset_x = event->xbutton.x;
	  panel_move.offset_y = event->xbutton.y;
	}
      }
    }
    break;
    
    case ButtonRelease: {
      XButtonEvent *bevent = (XButtonEvent *) event;
      
      if(bevent->window == gGui->panel_window) {
	click_notify_widget_list (panel_widget_list, event->xbutton.x, 
				  event->xbutton.y, 1);
	panel_move.enabled = 0; /* disable moving the window       */
      }
      break;
    }      
    }
  }
}
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

  XLockDisplay (gGui->display);
  
  /*
   * load bg image before opening window, so we can determine it's size
   */

  if (!(gGui->bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 gui_get_skinfile("BackGround")))) {
    fprintf(stderr, "xine-panel: couldn't find image for background\n");
    exit(-1);
  }

  /*
   * open the panel window
   */

  hint.x = config_lookup_int ("x_panel", 200);
  hint.y = config_lookup_int ("y_panel", 100);

  hint.x = 200;
  hint.y = 100;
  hint.width = gGui->bg_image->rgb_width;
  hint.height = gGui->bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;

  /*
  printf ("imlib_data: %d visual : %d\n",gGui->imlib_data,gGui->imlib_data->x.visual);
  printf ("w : %d h : %d\n",hint.width, hint.height);
  */

  /* FIXME: somhow gGui->imlib_data->x.visual is wrong
  gGui->panel_window = XCreateWindow (gGui->display, DefaultRootWindow(gGui->display), 
				      hint.x, hint.y, hint.width, hint.height, 0, 
				      gGui->imlib_data->x.depth, CopyFromParent, 
				      gGui->imlib_data->x.visual,
				      0, &attr);
  */
  gGui->panel_window = XCreateWindow (gGui->display, DefaultRootWindow(gGui->display), 
				      hint.x, hint.y, hint.width, hint.height, 0, 
				      CopyFromParent, CopyFromParent, 
				      CopyFromParent,
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
  }

  /*
   * set background image
   */
  
  gc = XCreateGC(gGui->display, gGui->panel_window, 0, 0);

  Imlib_apply_image(gGui->imlib_data, gGui->bg_image, gGui->panel_window);
  XSync(gGui->display, False); 

  XUnlockDisplay (gGui->display);

  /*
   * drag and drop
   */
  
  dnd_make_window_aware (&gGui->xdnd, gGui->panel_window);


  /*
   * Widget-list
   */

  panel_widget_list = (widget_list_t *) malloc (sizeof (widget_list_t));
  panel_widget_list->l = gui_list_new ();
  panel_widget_list->focusedWidget = NULL;
  panel_widget_list->pressedWidget = NULL;
  panel_widget_list->win           = gGui->panel_window;
  panel_widget_list->gc            = gc;
 
  /* Check and place some extra images on GUI */
  gui_place_extra_images(panel_widget_list);

  //PREV-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Prev"),
					  gui_get_skinY("Prev"),
					  gui_nextprev, 
					  (void*)GUI_PREV, 
					  gui_get_skinfile("Prev")));

  //STOP-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Stop"),
					  gui_get_skinY("Stop"), gui_stop, 
					  NULL, gui_get_skinfile("Stop")));
  
  //PLAY-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Play"),
					  gui_get_skinY("Play"), gui_play,
					  NULL, gui_get_skinfile("Play")));

  // PAUSE-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   (panel_checkbox_pause = 
			    create_checkbox (gGui->display, gGui->imlib_data, 
					     gui_get_skinX("Pause"),
					     gui_get_skinY("Pause"), 
					     gui_pause, NULL, 
					     gui_get_skinfile("Pause"))));
  
  // NEXT-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Next"),
					  gui_get_skinY("Next"),
					  gui_nextprev, (void*)GUI_NEXT, 
					  gui_get_skinfile("Next")));

  //Eject Button
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Eject"),
					  gui_get_skinY("Eject"),
					  gui_eject, NULL, 
					  gui_get_skinfile("Eject")));

  // EXIT-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Exit"),
					  gui_get_skinY("Exit"), gui_exit,
					  NULL, gui_get_skinfile("Exit")));

  // Close-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("Close"),
					  gui_get_skinY("Close"), 
					  panel_toggle_visibility,
					  NULL, gui_get_skinfile("Close"))); 
  // Fullscreen-BUTTON
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("FullScreen"),
					  gui_get_skinY("FullScreen"),
					  gui_toggle_fullscreen, NULL, 
					  gui_get_skinfile("FullScreen")));
  // Prev audio channel
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioNext"),
					  gui_get_skinY("AudioNext"),
					  gui_change_audio_channel,
					  (void*)GUI_NEXT, 
					  gui_get_skinfile("AudioNext")));
  // Next audio channel
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioPrev"),
					  gui_get_skinY("AudioPrev"),
					  gui_change_audio_channel,
					  (void*)GUI_PREV, 
					  gui_get_skinfile("AudioPrev")));

  // Prev spuid
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuNext"),
					  gui_get_skinY("SpuNext"),
					  gui_change_spu_channel,
					  (void*)GUI_NEXT, 
					  gui_get_skinfile("SpuNext")));
  // Next spuid
  gui_list_append_content (panel_widget_list->l, 
			   create_button (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuPrev"),
					  gui_get_skinY("SpuPrev"),
					  gui_change_spu_channel,
					  (void*)GUI_PREV, 
					  gui_get_skinfile("SpuPrev")));

/*    gui_list_append_content (panel_widget_list->l,  */
/*  			   create_button (gGui->display, gGui->imlib_data, 
			                  gui_get_skinX("DVDScan"), */
/*  					  gui_get_skinY("DVDScan"), */
/*  					  gui_scan_input, */
/*  					  (void*)SCAN_DVD,  */
/*  					  gui_get_skinfile("DVDScan"))); */

/*    gui_list_append_content (panel_widget_list->l,  */
/*  			   create_button (gGui->display, gGui->imlib_data, 
			                  gui_get_skinX("VCDScan"), */
/*  					  gui_get_skinY("VCDScan"), */
/*  					  gui_scan_input, */
/*  					  (void*)SCAN_VCD,  */
/*  					  gui_get_skinfile("VCDScan"))); */

  /* LABEL TITLE */
  gui_list_append_content (panel_widget_list->l,
			   (panel_title_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TitleLabel"),
					  gui_get_skinY("TitleLabel"),
					  60, gGui->filename, 
					  gui_get_skinfile("TitleLabel"))));

  /* runtime label */
  gui_list_append_content (panel_widget_list->l,
			   (panel_runtime_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("TimeLabel"),
					  gui_get_skinY("TimeLabel"),
					  8, panel_runtime,
					  gui_get_skinfile("TimeLabel"))));

  /* Audio channel label */
  gui_list_append_content (panel_widget_list->l,
			   (panel_audiochan_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("AudioLabel"),
					  gui_get_skinY("AudioLabel"),
					  3, panel_audiochan, 
					  gui_get_skinfile("AudioLabel"))));

  /* Spuid label */
  gui_list_append_content (panel_widget_list->l,
			   (panel_spuid_label = 
			    create_label (gGui->display, gGui->imlib_data, 
					  gui_get_skinX("SpuLabel"),
					  gui_get_skinY("SpuLabel"),
					  3, panel_spuid, 
					  gui_get_skinfile("SpuLabel"))));

  /* SLIDERS */
  panel_slider_timer = MAX_UPDSLD;
  gui_list_append_content (panel_widget_list->l,
			   (panel_slider_play = 
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
  gui_list_append_content (panel_widget_list->l,
			   (panel_slider_mixer = 
			    create_slider(gGui->display, gGui->imlib_data, 
			    VSLIDER, 
			    gui_get_skinX("SliderBGVol"),
			    gui_get_skinY("SliderBGVol"),
			    0, 100, 1, 
			    gui_get_skinfile("SliderBGVol"),
			    gui_get_skinfile("SliderFGVol"),
			    panel_slider_cb, NULL)));
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
	gui_list_append_content (panel_widget_list->l,
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


  gui_list_append_content (panel_widget_list->l, 
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

  gui_list_append_content (panel_widget_list->l, 
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
   * moving the window
   */

  panel_move.enabled  = 0;
  panel_move.offset_x = 0;
  panel_move.offset_y = 0; 

  /* 
   * show panel 
   */

  XLockDisplay (gGui->display);
  
  if (config_lookup_int("open_panel", 1)) {
    XMapRaised(gGui->display, gGui->panel_window); 
    panel_visible = 1;
  } 
  else {
    panel_visible = 0;
  }

  video_window_set_cursor_visibility (panel_visible);

  XUnlockDisplay (gGui->display);
}

