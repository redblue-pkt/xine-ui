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
#include "mrl_browser.h"

extern gGui_t     *gGui;

_panel_t          *panel;

/*
 * Try to guess if current WM is E.
 */
static int is_wm_is_enlightenment(Display *display, Window window) {
  Atom  *atoms;
  int    i, natoms;
  
  XLockDisplay (gGui->display);
  atoms = XListProperties(display, window, &natoms);
  
  if(natoms) {
    for(i = 0; i < natoms; i++) {
      if(!strncasecmp("_E_FRAME_SIZE", (XGetAtomName(display, atoms[i])), 13)) {
	XUnlockDisplay (gGui->display);
	return 1;
      }
    }
  }
  
  XUnlockDisplay (gGui->display);
  return 0;
}

/*
 * Toolkit event handler will call this function with new
 * coords of panel window.
 */
static void panel_store_new_position(int x, int y, int w, int h) {
 
  config_set_int("panel_x", x);
  config_set_int("panel_y", y);
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
  
  label_change_label (panel->widget_list, panel->runtime_label, timestr); 
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
	  slider_set_pos(panel->widget_list, panel->slider_play, 
			 xine_get_current_position(gGui->xine));
	}
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
void panel_toggle_visibility (widget_t *w, void *data) {

  if(!panel->visible && pl_is_visible()) {}
  else {
    pl_toggle_visibility(NULL, NULL);
  }
    
  if(!panel->visible && control_is_visible()) {}
  else {
    control_toggle_panel_visibility(NULL, NULL);
  }

  if(!panel->visible && mrl_browser_is_visible()) {}
  else {
    mrl_browser_toggle_visibility();
  }


  if (panel->visible) {

    if (video_window_is_visible ()) {
      panel->visible = 0;
      XUnmapWindow (gGui->display, gGui->panel_window);
      widget_hide_widgets(panel->widget_list);
    }
    
  } else {

    panel->visible = 1;
    widget_show_widgets(panel->widget_list);
    XMapRaised(gGui->display, gGui->panel_window); 
    XSetTransientForHint (gGui->display, 
			  gGui->panel_window, gGui->video_window);
    
    layer_above_video(gGui->panel_window);
  }

  config_set_int("open_panel", panel->visible);
  
}

void panel_check_mute(void) {

  checkbox_set_state(panel->mixer.mute, gGui->mixer.mute, 
		     gGui->panel_window, panel->widget_list->gc);
}

/*
 * Check and set the correct state of pause button.
 */
void panel_check_pause(void) {
  
  checkbox_set_state(panel->checkbox_pause, 
		     (((xine_get_status(gGui->xine) == XINE_PLAY) && 
		      (xine_get_speed(gGui->xine) == SPEED_PAUSE)) ? 1 : 0 ), 
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
  ui_event_t uevent;

  uevent.event.type = XINE_UI_EVENT;
  uevent.handled = 0;

  uevent.sub_type = XINE_UI_GET_AUDIO_LANG;
  uevent.data = panel->audiochan;
  uevent.data_len = 4;

  xine_send_event(gGui->xine, &uevent.event, NULL);

  if(!uevent.handled)
    sprintf (panel->audiochan, "%3d", xine_get_audio_channel(gGui->xine));
  label_change_label (panel->widget_list, panel->audiochan_label, 
		      panel->audiochan);

  if(xine_get_spu_channel(gGui->xine) >= 0) {
    uevent.event.type = XINE_UI_EVENT;
    uevent.handled = 0;
    uevent.sub_type = XINE_UI_GET_SPU_LANG;
    uevent.data = panel->spuid;
    uevent.data_len = 4;

    xine_send_event(gGui->xine, &uevent.event, NULL);

    if(!uevent.handled)
      sprintf (panel->spuid, "%3d", xine_get_spu_channel (gGui->xine));
  } else {
    sprintf (panel->spuid, "%3s", "off");
  }

  label_change_label (panel->widget_list, panel->spuid_label, panel->spuid);
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
  label_change_label (panel->widget_list, panel->title_label, gGui->filename);
}

/*
 *
 */
void panel_toggle_audio_mute(widget_t *w, void *data, int state) {

  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
    gGui->mixer.mute = state;
    xine_set_audio_property(gGui->xine, AO_PROP_MUTE_VOL, gGui->mixer.mute);
  }
  panel_check_mute();
}

/*
 *
 */
void panel_execute_snapshot(widget_t *w, void *data) {
  int err;
  char cmd[2048];
  char *vo_name;

  vo_name = config_lookup_str("video_driver_name", NULL);

  if(!strcmp(vo_name, "XShm")) {
    snprintf(cmd, 2048, "%s/%s", XINE_SCRIPTDIR, "xineshot");
    if((err = xine_system(0, cmd)) < 0) {
      printf("xine_system() returned %d\n", err);
    }
  }
  else
    printf("Grab snapshots only works with XShm video driver.\n");
  
}

/*
 * Handle paddle moving of slider.
 */
static void panel_slider_cb(widget_t *w, void *data, int pos) {

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
    fprintf(stderr, "unknown widget slider caller\n");

  panel_check_pause();
}

/*
 * Handle X events here.
 */
void panel_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case KeyPress:
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
  int                  x, y;
  int                  i = 0;
  xitk_labelbutton_t   lb;
  widget_t            *tmp;
  char               **autoplay_plugins = 
    xine_get_autoplay_input_plugin_ids(gGui->xine);
  
  x = gui_get_skinX("AutoPlayGUI");
  y = gui_get_skinY("AutoPlayGUI");
  
  lb.display   = gGui->display;
  lb.imlibdata = gGui->imlib_data;

  while(autoplay_plugins[i] != NULL) {

    lb.x              = x;
    lb.y              = y;
    lb.button_type    = CLICK_BUTTON;
    lb.label          = autoplay_plugins[i];
    lb.callback       = pl_scan_input;
    lb.state_callback = NULL;
    lb.userdata       = NULL;
    lb.skin           = gui_get_skinfile("AutoPlayGUI");
    lb.normcolor      = gui_get_ncolor("AutoPlayGUI");
    lb.focuscolor     = gui_get_fcolor("AutoPlayGUI");
    lb.clickcolor     = gui_get_ccolor("AutoPlayGUI");
    lb.fontname       = gui_get_fontname("AutoPlayGUI");

    gui_list_append_content (panel->widget_list->l,
	     (tmp =
	      label_button_create (&lb)));

    x -= widget_get_width(tmp) + 1;
    i++;
  }
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
    widget_enable(panel->mixer.slider);
    gGui->mixer.volume_level = xine_get_audio_property(gGui->xine, gGui->mixer.volume_mixer);
    slider_set_pos(panel->widget_list, panel->mixer.slider, gGui->mixer.volume_level);
  }

  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
    widget_enable(panel->mixer.mute);
    gGui->mixer.mute = xine_get_audio_property(gGui->xine, AO_PROP_MUTE_VOL);
    checkbox_set_state(panel->mixer.mute, gGui->mixer.mute,
		       gGui->panel_window, panel->widget_list->gc);
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
  Atom                    prop, XA_WIN_LAYER;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;
  xitk_button_t           b;
  xitk_checkbox_t         cb;
  xitk_label_t            lbl;
  xitk_slider_t           sl;
  long data[1];
  
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
  attr.border_pixel      = gGui->black.pixel;
  attr.colormap          = Imlib_get_colormap(gGui->imlib_data);
  /*  
      printf ("imlib_data: %d visual : %d\n",gGui->imlib_data,gGui->imlib_data->x.visual);
      printf ("w : %d h : %d\n",hint.width, hint.height);
  */
  
  gGui->panel_window = XCreateWindow (gGui->display, 
				      DefaultRootWindow(gGui->display), 
				      hint.x, hint.y,
				      hint.width, hint.height, 0, 
				      gGui->imlib_data->x.depth,
				      CopyFromParent, 
				      gGui->imlib_data->x.visual,
				      CWBackPixel | CWBorderPixel | CWColormap, &attr);
  
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

  panel->widget_list                = widget_list_new();
  panel->widget_list->l             = gui_list_new ();
  panel->widget_list->focusedWidget = NULL;
  panel->widget_list->pressedWidget = NULL;
  panel->widget_list->win           = gGui->panel_window;
  panel->widget_list->gc            = gc;
 
  b.display     = gGui->display;
  b.imlibdata   = gGui->imlib_data;

  cb.display    = gGui->display;
  cb.imlibdata  = gGui->imlib_data;

  lbl.display   = gGui->display;
  lbl.imlibdata = gGui->imlib_data;
  lbl.window    = panel->widget_list->win;
  lbl.gc        = panel->widget_list->gc;

  sl.display    = gGui->display;
  sl.imlibdata  = gGui->imlib_data;

  /* Check and place some extra images on GUI */
  gui_place_extra_images(panel->widget_list);

  /*  Prev button */
  b.x        = gui_get_skinX("Prev");
  b.y        = gui_get_skinY("Prev");
  b.callback = gui_nextprev;
  b.userdata = (void *)GUI_PREV;
  b.skin     = gui_get_skinfile("Prev");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Stop button */
  b.x        = gui_get_skinX("Stop");
  b.y        = gui_get_skinY("Stop");
  b.callback = gui_stop;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Stop");
  gui_list_append_content(panel->widget_list->l, button_create(&b));
  
  /*  Play button */
  b.x        = gui_get_skinX("Play");
  b.y        = gui_get_skinY("Play");
  b.callback = gui_play;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Play");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Pause button */
  cb.x        = gui_get_skinX("Pause");
  cb.y        = gui_get_skinY("Pause");
  cb.callback = gui_pause;
  cb.userdata = NULL;
  cb.skin     = gui_get_skinfile("Pause");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->checkbox_pause = 
			    checkbox_create (&cb)));
  
  /*  Next button */
  b.x        = gui_get_skinX("Next");
  b.y        = gui_get_skinY("Next");
  b.callback = gui_nextprev;
  b.userdata = (void *)GUI_NEXT;
  b.skin     = gui_get_skinfile("Next");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Eject button */
  b.x        = gui_get_skinX("Eject");
  b.y        = gui_get_skinY("Eject");
  b.callback = gui_eject;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Eject");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Exit button */
  b.x        = gui_get_skinX("Exit");
  b.y        = gui_get_skinY("Exit");
  b.callback = gui_exit;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Exit");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Close button */
  b.x        = gui_get_skinX("Close");
  b.y        = gui_get_skinY("Close");
  b.callback = panel_toggle_visibility;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Close");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Fullscreen button */
  b.x        = gui_get_skinX("FullScreen");
  b.y        = gui_get_skinY("FullScreen");
  b.callback = gui_toggle_fullscreen;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("FullScreen");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Next audio channel */
  b.x        = gui_get_skinX("AudioNext");
  b.y        = gui_get_skinY("AudioNext");
  b.callback = gui_change_audio_channel;
  b.userdata = (void *)GUI_NEXT;
  b.skin     = gui_get_skinfile("AudioNext");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Prev audio channel */
  b.x        = gui_get_skinX("AudioPrev");
  b.y        = gui_get_skinY("AudioPrev");
  b.callback = gui_change_audio_channel;
  b.userdata = (void *)GUI_PREV;
  b.skin     = gui_get_skinfile("AudioPrev");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Prev spuid */
  b.x        = gui_get_skinX("SpuNext");
  b.y        = gui_get_skinY("SpuNext");
  b.callback = gui_change_spu_channel;
  b.userdata = (void *)GUI_NEXT;
  b.skin     = gui_get_skinfile("SpuNext");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Next spuid */
  b.x        = gui_get_skinX("SpuPrev");
  b.y        = gui_get_skinY("SpuPrev");
  b.callback = gui_change_spu_channel;
  b.userdata = (void *)GUI_PREV;
  b.skin     = gui_get_skinfile("SpuPrev");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Label title */
  lbl.x         = gui_get_skinX("TitleLabel");
  lbl.y         = gui_get_skinY("TitleLabel");
  lbl.length    = gui_get_label_length("TitleLabel");
  lbl.label     = gGui->filename;
  lbl.font      = gui_get_skinfile("TitleLabel");
  lbl.animation = gui_get_animation("TitleLabel");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->title_label = label_create (&lbl)));

  /*  Runtime label */
  lbl.x         = gui_get_skinX("TimeLabel");
  lbl.y         = gui_get_skinY("TimeLabel");
  lbl.length    = gui_get_label_length("TimeLabel");
  lbl.label     = panel->runtime;
  lbl.font      = gui_get_skinfile("TimeLabel");
  lbl.animation = gui_get_animation("TimeLabel");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->runtime_label = label_create (&lbl)));

  /*  Audio channel label */
  lbl.x         = gui_get_skinX("AudioLabel");
  lbl.y         = gui_get_skinY("AudioLabel");
  lbl.length    = gui_get_label_length("AudioLabel");
  lbl.label     = panel->audiochan;
  lbl.font      = gui_get_skinfile("AudioLabel");
  lbl.animation = gui_get_animation("AudioLabel");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->audiochan_label = label_create (&lbl)));

  /*  Spuid label */
  lbl.x         = gui_get_skinX("SpuLabel");
  lbl.y         = gui_get_skinY("SpuLabel");
  lbl.length    = gui_get_label_length("SpuLabel");
  lbl.label     = panel->spuid;
  lbl.font      = gui_get_skinfile("SpuLabel");
  lbl.animation = gui_get_animation("SpuLabel");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->spuid_label = label_create (&lbl)));

  /*  slider seek */
  sl.x               = gui_get_skinX("SliderBGPlay");
  sl.y               = gui_get_skinY("SliderBGPlay");
  sl.slider_type     = HSLIDER;
  sl.min             = 0;
  sl.max             = 65535;
  sl.step            = 1;
  sl.background_skin = gui_get_skinfile("SliderBGPlay");
  sl.paddle_skin     = gui_get_skinfile("SliderFGPlay");
  sl.callback        = NULL; /* panel_slider_cb; */
  sl.userdata        = NULL;
  sl.motion_callback = panel_slider_cb;
  sl.motion_userdata = NULL;
  gui_list_append_content (panel->widget_list->l, 
			   (panel->slider_play = slider_create(&sl)));

  /* Mixer volume slider */
  sl.x               = gui_get_skinX("SliderBGVol");
  sl.y               = gui_get_skinY("SliderBGVol");
  sl.slider_type     = VSLIDER;
  sl.min             = 0;
  sl.max             = 100;
  sl.step            = 1;
  sl.background_skin = gui_get_skinfile("SliderBGVol");
  sl.paddle_skin     = gui_get_skinfile("SliderFGVol");
  sl.callback        = NULL;
  sl.userdata        = NULL;
  sl.motion_callback = panel_slider_cb;
  sl.motion_userdata = NULL;
  gui_list_append_content (panel->widget_list->l, 
			   (panel->mixer.slider = slider_create(&sl)));
  widget_disable(panel->mixer.slider);

  /*  Mute toggle */
  cb.x        = gui_get_skinX("Mute");
  cb.y        = gui_get_skinY("Mute");
  cb.callback = panel_toggle_audio_mute;
  cb.userdata = NULL;
  cb.skin     = gui_get_skinfile("Mute");
  gui_list_append_content (panel->widget_list->l, 
			   (panel->mixer.mute = checkbox_create (&cb)));
  widget_disable(panel->mixer.mute);

  /* Snapshot */
  b.x        = gui_get_skinX("Snapshot");
  b.y        = gui_get_skinY("Snapshot");
  b.callback = panel_execute_snapshot;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("Snapshot");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Playback speed slow */
  b.x        = gui_get_skinX("PlaySlow");
  b.y        = gui_get_skinY("PlaySlow");
  b.callback = gui_change_speed_playback;
  b.userdata = (void *)GUI_NEXT;
  b.skin     = gui_get_skinfile("PlaySlow");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Playback speed fast */
  b.x        = gui_get_skinX("PlayFast");
  b.y        = gui_get_skinY("PlayFast");
  b.callback = gui_change_speed_playback;
  b.userdata = (void *)GUI_PREV;
  b.skin     = gui_get_skinfile("PlayFast");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Playlist button */
  b.x        = gui_get_skinX("PlBtn");
  b.y        = gui_get_skinY("PlBtn");
  b.callback = gui_playlist_show;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("PlBtn");
  gui_list_append_content(panel->widget_list->l, button_create(&b));
  
  /*  Control button */
  b.x        = gui_get_skinX("CtlBtn");
  b.y        = gui_get_skinY("CtlBtn");
  b.callback = gui_control_show;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("CtlBtn");
  gui_list_append_content(panel->widget_list->l, button_create(&b));

  /*  Mrl button */
  b.x        = gui_get_skinX("MrlBtn");
  b.y        = gui_get_skinY("MrlBtn");
  b.callback = gui_mrlbrowser_show;
  b.userdata = NULL;
  b.skin     = gui_get_skinfile("MrlBtn");
  gui_list_append_content(panel->widget_list->l, button_create(&b));
  
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
						    panel->widget_list,
						    NULL);

  gGui->cursor_visible = 1;
  video_window_set_cursor_visibility(gGui->cursor_visible);

  XUnlockDisplay (gGui->display);

  gGui->reparent_hack = is_wm_is_enlightenment(gGui->display, gGui->panel_window);

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
