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
 * xine panel related stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <xine/xineutils.h>
#include <zlib.h>

#include "xitk.h"

#include "event.h"
#include "actions.h"
#include "control.h"
#include "playlist.h"
#include "videowin.h"
#include "panel.h"
#include "mrl_browser.h"
#include "snapshot.h"
#include "errors.h"
#include "utils.h"

#ifndef __GNUC__
#define __FUNCTION__    __func__
#endif

extern gGui_t     *gGui;

_panel_t          *panel;

/*
 *
 */
void panel_show_tips(void) {
  
  if(panel->tips.enable)
    xitk_set_widgets_tips_timeout(panel->widget_list, panel->tips.timeout);
  else
    xitk_disable_widgets_tips(panel->widget_list);

  playlist_show_tips(panel->tips.enable, panel->tips.timeout);
  control_show_tips(panel->tips.enable, panel->tips.timeout);
  mrl_browser_show_tips(panel->tips.enable, panel->tips.timeout);
}

/*
 * Config callbacks.
 */
static void panel_enable_tips_cb(void *data, cfg_entry_t *cfg) {
  panel->tips.enable = cfg->num_value;
  panel_show_tips();
}
static void panel_timeout_tips_cb(void *data, cfg_entry_t *cfg) {
  panel->tips.timeout = cfg->num_value;
  xitk_set_widgets_tips_timeout(panel->widget_list, panel->tips.timeout);
}

/*
 * Try to guess if current WM is E.
 */
static int is_wm_is_enlightenment(Display *d, Window w) {
  Atom  *atoms;
  int    i, natoms;
  
  XLockDisplay (d);
  atoms = XListProperties(d, w, &natoms);
  
  if(natoms) {
    for(i = 0; i < natoms; i++) {
      if(!strncasecmp("_E_FRAME_SIZE", (XGetAtomName(d, atoms[i])), 13)) {
	XUnlockDisplay (d);
	return 1;
      }
    }
  }
  
  XUnlockDisplay (d);
  return 0;
}

/*
 * Toolkit event handler will call this function with new
 * coords of panel window.
 */
static void panel_store_new_position(int x, int y, int w, int h) {
 
  gGui->config->update_num (gGui->config, "gui.panel_x", x);
  gGui->config->update_num (gGui->config, "gui.panel_y", y);
}

/*
 * Change the current skin.
 */
void panel_change_skins(void) {
  ImlibImage   *new_img, *old_img;
  XSizeHints    hint;
  
  xitk_skin_lock(gGui->skin_config);
  xitk_hide_widgets(panel->widget_list);

  XLockDisplay(gGui->display);
  
  if(!(new_img = Imlib_load_image(gGui->imlib_data,
				  xitk_skin_get_skin_filename(gGui->skin_config, "BackGround")))) {
    xine_error("%s(): couldn't find image for background\n", __FUNCTION__);
    exit(-1);
  }
  
  hint.width  = new_img->rgb_width;
  hint.height = new_img->rgb_height;
  hint.flags  = PSize;
  XSetWMNormalHints(gGui->display, gGui->panel_window, &hint);
  
  XResizeWindow (gGui->display, gGui->panel_window,
		 (unsigned int)new_img->rgb_width,
		 (unsigned int)new_img->rgb_height);
  
  XUnlockDisplay(gGui->display);

  while(!xitk_is_window_size(gGui->display, gGui->panel_window, 
			     new_img->rgb_width, new_img->rgb_height)) {
    xine_usec_sleep(10000);
  }
  
  old_img = panel->bg_image;
  panel->bg_image = new_img;

  XLockDisplay(gGui->display);

  XSetTransientForHint(gGui->display, gGui->panel_window, gGui->video_window);

  Imlib_destroy_image(gGui->imlib_data, old_img);
  Imlib_apply_image(gGui->imlib_data, new_img, gGui->panel_window);
  
  XUnlockDisplay(gGui->display);
  
  xitk_skin_unlock(gGui->skin_config);
  
  xitk_change_skins_widget_list(panel->widget_list, gGui->skin_config);

  /*
   * Update position of dynamic buttons.
   */
  {
    int i = 0, x, y;
    
    x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayGUI");
    y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayGUI");
    
    while(panel->autoplay_plugins[i] != NULL) {
      
      (void) xitk_set_widget_pos(panel->autoplay_plugins[i], x, y);
      
      x -= xitk_get_widget_width(panel->autoplay_plugins[i]) + 1;
      i++;
    }
  }

  xitk_paint_widget_list(panel->widget_list);
}

/*
 * Update runtime displayed informations.
 */
void panel_update_runtime_display(void) {
  int seconds;
  char timestr[10];

  if(!panel_is_visible())
    return;

  seconds = xine_get_current_time (gGui->xine);
  
  sprintf (timestr, "%02d:%02d:%02d",
	   seconds / (60*60),
	   (seconds / 60) % 60, seconds % 60);
  
  xitk_label_change_label (panel->widget_list, panel->runtime_label, timestr); 
}

/*
 * Update slider thread.
 */
static void *slider_loop(void *dummy) {
  
  pthread_detach(pthread_self());

  while(gGui->running) {

    if(panel_is_visible()) {
      if(gGui->xine) {
	int status = xine_get_status(gGui->xine);
	
	if(status == XINE_PLAY) {
	  xitk_slider_set_pos(panel->widget_list, panel->slider_play, 
			      xine_get_current_position(gGui->xine));
	}

        if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
          gGui->mixer.volume_level = xine_get_audio_property(gGui->xine, 
                                                             gGui->mixer.volume_mixer);
          xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
	  panel_check_mute();
        }
	
	if(status != XINE_STOP)
	  panel_update_runtime_display();
      }
    }
    
    if(gGui->cursor_visible) {
      gGui->cursor_visible = !gGui->cursor_visible;
      video_window_set_cursor_visibility(gGui->cursor_visible);
    }

    sleep(1);
  }

  pthread_exit(NULL);
  return NULL;
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
void panel_toggle_visibility (xitk_widget_t *w, void *data) {

  if(!panel->visible && pl_is_visible()) {}
  else {
    pl_toggle_visibility(NULL, NULL);
  }
    
  if(!panel->visible && control_is_visible()) {}
  else {
    control_toggle_visibility(NULL, NULL);
  }

  if(!panel->visible && mrl_browser_is_visible()) {}
  else {
    mrl_browser_toggle_visibility(NULL, NULL);
  }


  if (panel->visible) {
    
    XLockDisplay(gGui->display);
    
    if (video_window_is_visible ()) {
      if(gGui->use_root_window) { /* Using root window */
	XIconifyWindow(gGui->display, gGui->panel_window, gGui->screen);
      }
      else {
	panel->visible = 0;
	XUnmapWindow (gGui->display, gGui->panel_window);
	xitk_hide_widgets(panel->widget_list);
      }
    }

    if(gGui->cursor_grabbed)
       XGrabPointer(gGui->display, gGui->video_window, 1, None, GrabModeAsync, GrabModeAsync, gGui->video_window, None, CurrentTime);
    
    XUnlockDisplay(gGui->display);
     
  } else {

    panel->visible = 1;
    xitk_show_widgets(panel->widget_list);

    XLockDisplay(gGui->display);

    XMapRaised(gGui->display, gGui->panel_window); 
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);


    layer_above_video(gGui->panel_window);
     
    if(gGui->cursor_grabbed)
       XUngrabPointer(gGui->display, CurrentTime);
     
#ifdef HAVE_XF86VIDMODE
    /*
     * necessary to place the panel in a visible area (otherwise it might
     * appear off the video window while switched to a different modline)
     */
    if(gGui->XF86VidMode_fullscreen)
       XMoveWindow(gGui->display, gGui->panel_window, 0, 0);
#endif

    XUnlockDisplay(gGui->display);
  }

  gGui->config->update_num (gGui->config, "gui.panel_visible", panel->visible);
}

void panel_check_mute(void) {

  xitk_checkbox_set_state(panel->mixer.mute, gGui->mixer.mute, 
			  gGui->panel_window, panel->widget_list->gc);
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause(void) {
  
  xitk_checkbox_set_state(panel->checkbox_pause, 
			  (((xine_get_status(gGui->xine) == XINE_PLAY) && 
			    (xine_get_speed(gGui->xine) == SPEED_PAUSE)) ? 1 : 0 ), 
			  gGui->panel_window, panel->widget_list->gc);
  
}

/*
 * Reset the slider of panel window (set to 0).
 */
void panel_reset_slider (void) {
  xitk_slider_reset(panel->widget_list, panel->slider_play);
}

/*
 * Update audio/spu channel displayed informations.
 */
void panel_update_channel_display (void) {

  xine_get_audio_lang (gGui->xine, panel->audiochan);
  xitk_label_change_label (panel->widget_list, panel->audiochan_label, 
			   panel->audiochan);
  
  xine_get_spu_lang (gGui->xine, panel->spuid);

  xitk_label_change_label (panel->widget_list, panel->spuid_label, 
			   panel->spuid);
}

/*
 * Display an informative message when there is no mrl in playlist.
 */
void panel_set_no_mrl(void) {

  sprintf(gGui->filename, "xine-ui version %s", VERSION);
}

/*
 * Update displayed MRL according to the current one.
 */
void panel_update_mrl_display (void) {

  panel_set_title(gGui->filename);
}

/*
 *
 */
void panel_toggle_audio_mute(xitk_widget_t *w, void *data, int state) {

  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
    gGui->mixer.mute = state;
    xine_set_audio_property(gGui->xine, AO_PROP_MUTE_VOL, gGui->mixer.mute);
  }
  panel_check_mute();
}

/*
 *  Use internal xine-lib framegrabber function
 *  to snapshot current frame.
 */
void panel_snapshot(xitk_widget_t *w, void *data) {
  create_snapshot();
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_cb(xitk_widget_t *w, void *data, int pos) {

  if(w == panel->slider_play) {
    gui_set_current_position (pos);
    if(xine_get_status(gGui->xine) != XINE_PLAY) {
      panel_reset_slider();
    }
  }
  else if(w == panel->mixer.slider) {
    gGui->mixer.volume_level = pos;
    xine_set_audio_property(gGui->xine, gGui->mixer.volume_mixer, gGui->mixer.volume_level);
  }
  else
    xine_error("unknown widget slider caller\n");

  panel_check_pause();
}

/*
 * Handle X events here.
 */
void panel_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case KeyPress:
  case ButtonRelease:
    gui_handle_event(event, data);
    break;

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
  int                        x, y;
  int                        i = 0;
  xitk_labelbutton_widget_t  lb;
  char                     **autoplay_plugins = xine_get_autoplay_input_plugin_ids(gGui->xine);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  x = xitk_skin_get_coord_x(gGui->skin_config, "AutoPlayGUI");
  y = xitk_skin_get_coord_y(gGui->skin_config, "AutoPlayGUI");
  
  while(autoplay_plugins[i] != NULL) {

    lb.skin_element_name = "AutoPlayGUI";
    lb.button_type       = CLICK_BUTTON;
    lb.label             = autoplay_plugins[i];
    lb.callback          = pl_scan_input;
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    xitk_list_append_content (panel->widget_list->l,
	     (panel->autoplay_plugins[i] =
	      xitk_labelbutton_create (gGui->skin_config, &lb)));
    xitk_set_widget_tips(panel->autoplay_plugins[i], 
			 xine_get_input_plugin_description(gGui->xine, autoplay_plugins[i]));

    if(!panel->tips.enable)
      xitk_disable_widget_tips(panel->autoplay_plugins[i]);

    (void) xitk_set_widget_pos(panel->autoplay_plugins[i], x, y);

    x -= xitk_get_widget_width(panel->autoplay_plugins[i]) + 1;
    i++;
  }

  if(i)
    panel->autoplay_plugins[i+1] = NULL;

}

/*
 * Check if there a mixer control available,
 * We couldn't do this into panel_init(), this function is
 * called before xine engine initialization.
 */
void panel_add_mixer_control(void) {
  
  gGui->mixer.caps = xine_get_audio_capabilities(gGui->xine);

  if(gGui->mixer.caps & AO_CAP_PCM_VOL)
    gGui->mixer.volume_mixer = AO_PROP_PCM_VOL;
  else if(gGui->mixer.caps & AO_CAP_MIXER_VOL)
    gGui->mixer.volume_mixer = AO_PROP_MIXER_VOL;
  
  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
    xitk_enable_widget(panel->mixer.slider);
    gGui->mixer.volume_level = xine_get_audio_property(gGui->xine, gGui->mixer.volume_mixer);
    xitk_slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
  }

  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
    xitk_enable_widget(panel->mixer.mute);
    gGui->mixer.mute = xine_get_audio_property(gGui->xine, AO_PROP_MUTE_VOL);
    xitk_checkbox_set_state(panel->mixer.mute, gGui->mixer.mute,
			    gGui->panel_window, panel->widget_list->gc);
  }

  /* Tips should be available only if widgets are enabled */
  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL))
    xitk_set_widget_tips(panel->mixer.slider, _("Volume control"));
  if(gGui->mixer.caps & AO_CAP_MUTE_VOL)
    xitk_set_widget_tips(panel->mixer.mute, _("Mute toggle"));

  if(!panel->tips.enable) {
    xitk_disable_widget_tips(panel->mixer.slider);
    xitk_disable_widget_tips(panel->mixer.mute);
  }

}

/*
 * Create the panel window, and fit all widgets in.
 */
void panel_init (void) {
  GC                        gc;
  XSizeHints                hint;
  XWMHints                 *wm_hint;
  XSetWindowAttributes      attr;
  char                      title[] = {"Xine Panel"}; /* window-title     */
  Atom                      prop, XA_WIN_LAYER;
  MWMHints                  mwmhints;
  XClassHint               *xclasshint;
  xitk_button_widget_t      b;
  xitk_checkbox_widget_t    cb;
  xitk_label_widget_t       lbl;
  xitk_slider_widget_t      sl;
  xitk_widget_t            *w;
  long data[1];
  
  if (gGui->panel_window)
    return ; /* panel already open  - FIXME: bring to foreground */

  XITK_WIDGET_INIT(&b, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  panel = (_panel_t *) xine_xmalloc(sizeof(_panel_t));

  XLockDisplay (gGui->display);
  
  /*
   * load bg image before opening window, so we can determine it's size
   */
  if (!(panel->bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 xitk_skin_get_skin_filename(gGui->skin_config, "BackGround")))) {
    xine_error("xine-panel: couldn't find image for background\n");
    exit(-1);
  }

  /*
   * open the panel window
   */

  hint.x = gGui->config->register_num (gGui->config, "gui.panel_x", 200,
				       NULL, NULL, NULL, NULL);
  hint.y = gGui->config->register_num (gGui->config, "gui.panel_y", 100,
				       NULL, NULL, NULL, NULL);
  hint.width = panel->bg_image->rgb_width;
  hint.height = panel->bg_image->rgb_height;
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
  attr.border_pixel      = gGui->black.pixel;
  attr.colormap          = Imlib_get_colormap(gGui->imlib_data);
  /*  
      printf ("imlib_data: %d visual : %d\n",gGui->imlib_data,gGui->imlib_data->x.visual);
      printf ("w : %d h : %d\n",hint.width, hint.height);
  */
  
  gGui->panel_window = XCreateWindow (gGui->display, 
				      gGui->imlib_data->x.root, 
				      hint.x, hint.y,
				      hint.width, hint.height, 0, 
				      gGui->imlib_data->x.depth,
				      InputOutput,
				      gGui->imlib_data->x.visual,
				      CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
				      &attr);

  XSetStandardProperties(gGui->display, gGui->panel_window, title, title,
			 None, NULL, 0, &hint);

  XSelectInput(gGui->display, gGui->panel_window, INPUT_MOTION | 
	       /*SubstructureRedirectMask | */ KeymapStateMask);

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
   * layer above most other things, like gnome panel
   * WIN_LAYER_ABOVE_DOCK  = 10
   *
   */
  if(gGui->layer_above) {
    XA_WIN_LAYER = XInternAtom(gGui->display, "_WIN_LAYER", False);
    
    data[0] = 10;
    XChangeProperty(gGui->display, gGui->panel_window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }

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

  panel->widget_list                = xitk_widget_list_new();
  panel->widget_list->l             = xitk_list_new ();
  panel->widget_list->focusedWidget = NULL;
  panel->widget_list->pressedWidget = NULL;
  panel->widget_list->win           = gGui->panel_window;
  panel->widget_list->gc            = gc;
 
  lbl.window    = panel->widget_list->win;
  lbl.gc        = panel->widget_list->gc;

  /* Prev button */
  b.skin_element_name = "Prev";
  b.callback          = gui_nextprev;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Play previous entry from playlist"));

  /*  Stop button */
  b.skin_element_name = "Stop";
  b.callback          = gui_stop;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Stop playback"));
  
  /*  Play button */
  b.skin_element_name = "Play";
  b.callback          = gui_play;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Play selected entry"));

  /*  Pause button */
  cb.skin_element_name = "Pause";
  cb.callback          = gui_pause;
  cb.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, 
			   (panel->checkbox_pause = xitk_checkbox_create(gGui->skin_config, &cb)));
  xitk_set_widget_tips(panel->checkbox_pause, _("Pause/Resume playback"));
  
  /*  Next button */
  b.skin_element_name = "Next";
  b.callback          = gui_nextprev;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Play next entry from playlist")); 

  /*  Eject button */
  b.skin_element_name = "Eject";
  b.callback          = gui_eject;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Eject current medium"));

  /*  Exit button */
  b.skin_element_name = "Exit";
  b.callback          = gui_exit;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Quit"));
 
  /*  Setup button */
  b.skin_element_name = "Setup";
  b.callback          = gui_setup_show;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Setup window"));

  /*  Close button */
  b.skin_element_name = "Close";
  b.callback          = panel_toggle_visibility;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Hide GUI"));

  /*  Fullscreen button */
  b.skin_element_name = "FullScreen";
  b.callback          = gui_toggle_fullscreen;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Fullscreen/Window mode"));

  /*  Next audio channel */
  b.skin_element_name = "AudioNext";
  b.callback          = gui_change_audio_channel;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Next audio channel"));

  /*  Prev audio channel */
  b.skin_element_name = "AudioPrev";
  b.callback          = gui_change_audio_channel;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Previous audio channel"));

  /*  Prev spuid */
  b.skin_element_name = "SpuNext";
  b.callback          = gui_change_spu_channel;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Next SPU channel"));

  /*  Next spuid */
  b.skin_element_name = "SpuPrev";
  b.callback          = gui_change_spu_channel;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Previous SPU channel"));

  /*  Label title */
  lbl.skin_element_name = "TitleLabel";
  lbl.label             = gGui->filename;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->title_label = xitk_label_create (gGui->skin_config, &lbl)));

  /*  Runtime label */
  lbl.skin_element_name = "TimeLabel";
  lbl.label             = panel->runtime;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->runtime_label = xitk_label_create (gGui->skin_config, &lbl)));
  /* 
   * Init to default, otherwise if panel is hide
   * at startup, label is empty 'till it's updated
   */
  xitk_label_change_label (panel->widget_list, panel->runtime_label, "00:00:00"); 

  /*  Audio channel label */
  lbl.skin_element_name = "AudioLabel";
  lbl.label             = panel->audiochan;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->audiochan_label = xitk_label_create (gGui->skin_config, &lbl)));

  /*  Spuid label */
  lbl.skin_element_name = "SpuLabel";
  lbl.label             = panel->spuid;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->spuid_label = xitk_label_create (gGui->skin_config, &lbl)));

  /*  slider seek */
  sl.skin_element_name = "SliderPlay";
  sl.min               = 0;
  sl.max               = 65535;
  sl.step              = 1;
  sl.callback          = NULL;
  sl.userdata          = NULL;
  sl.motion_callback   = panel_slider_cb;
  sl.motion_userdata   = NULL;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->slider_play = xitk_slider_create(gGui->skin_config, &sl)));
  xitk_set_widget_tips(panel->slider_play, _("Stream playback position slider"));

  /* Mixer volume slider */
  sl.skin_element_name = "SliderVol";
  sl.min               = 0;
  sl.max               = 100;
  sl.step              = 1;
  sl.callback          = NULL;
  sl.userdata          = NULL;
  sl.motion_callback   = panel_slider_cb;
  sl.motion_userdata   = NULL;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->mixer.slider = xitk_slider_create(gGui->skin_config, &sl)));
  xitk_disable_widget(panel->mixer.slider);

  /*  Mute toggle */
  cb.skin_element_name = "Mute";
  cb.callback          = panel_toggle_audio_mute;
  cb.userdata          = NULL;
  xitk_list_append_content (panel->widget_list->l, 
			   (panel->mixer.mute = xitk_checkbox_create (gGui->skin_config, &cb)));
  xitk_disable_widget(panel->mixer.mute);

  /* Snapshot */
  b.skin_element_name = "Snapshot";
  b.callback          = panel_snapshot;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Take a snapshot"));

  /*  Playback speed slow */
  b.skin_element_name = "PlaySlow";
  b.callback          = gui_change_speed_playback;
  b.userdata          = (void *)GUI_NEXT;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Slow motion"));

  /*  Playback speed fast */
  b.skin_element_name = "PlayFast";
  b.callback          = gui_change_speed_playback;
  b.userdata          = (void *)GUI_PREV;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Fast motion"));
  
  /*  Playlist button */
  b.skin_element_name = "PlBtn";
  b.callback          = gui_playlist_show;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Playlist"));
  
  /*  Control button */
  b.skin_element_name = "CtlBtn";
  b.callback          = gui_control_show;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("Control"));

  /*  Mrl button */
  b.skin_element_name = "MrlBtn";
  b.callback          = gui_mrlbrowser_show;
  b.userdata          = NULL;
  xitk_list_append_content(panel->widget_list->l, (w = xitk_button_create(gGui->skin_config, &b)));
  xitk_set_widget_tips(w, _("MRL Browser"));
  

  panel->tips.enable = gGui->config->register_bool(gGui->config, "gui.tips_visible", 1,
						   "gui tips visibility", NULL,
						   panel_enable_tips_cb, NULL);
  panel->tips.timeout = gGui->config->register_num(gGui->config, "gui.tips_timeout", 500,
						   "tips timeout (ms)", NULL, 
						   panel_timeout_tips_cb, NULL);

  panel_show_tips();

  /* 
   * show panel 
   */
  panel->visible = gGui->config->register_bool (gGui->config, "gui.panel_visible", 1,
						"gui panel visibility",
						NULL, NULL, NULL);
  
  if(gGui->use_root_window && (!panel->visible))
    panel->visible = 1;

  if (panel->visible)
    XMapRaised(gGui->display, gGui->panel_window); 
  
  XUnlockDisplay (gGui->display);

  panel->widget_key = xitk_register_event_handler("panel", 
						  gGui->panel_window, 
						  panel_handle_event,
						  panel_store_new_position,
						  gui_dndcallback,
						  panel->widget_list,
						  NULL);

  gGui->cursor_visible = 1;
  video_window_set_cursor_visibility(gGui->cursor_visible);
  
  /*
   * Grag Atoms from a mapped window, panel window could be not already mapped.
   */
  gGui->reparent_hack = is_wm_is_enlightenment(gGui->display, gGui->video_window);
  
  {
    pthread_attr_t       pth_attrs;
    struct sched_param   pth_params;
    
    pthread_attr_init(&pth_attrs);

    /* this won't work on linux, freebsd 5.0 */
    pthread_attr_getschedparam(&pth_attrs, &pth_params);
    pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
    pthread_attr_setschedparam(&pth_attrs, &pth_params);
    
    pthread_create(&panel->slider_thread, &pth_attrs, slider_loop, NULL);
  }
}

void panel_set_title(char *title) {
  xitk_label_change_label(panel->widget_list, panel->title_label, title);
}
