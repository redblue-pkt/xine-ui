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

extern gGlob_t        *gGlob;

static widget_t       *w_hue = NULL, *w_sat = NULL, *w_bright = NULL;
static widget_t       *w_cont = NULL;
static Window          ctl_win;
static DND_struct_t   *xdnd_ctl_win;
static ImlibImage     *ctl_bg_image;
static gui_move_t      ctl_move; 
static widget_list_t  *ctl_widget_list;

static int             ctl_running;
static int             ctl_panel_visible;


static int get_current_prop(int prop) {
  return (xine_get_window_property(gGlob->gXine, prop));
}
static int set_current_prop(int prop, int value) {
  return (xine_set_window_property(gGlob->gXine, prop, value));
}

/*
 * Update silders positions
 */
static void update_sliders_video_settings(void) {

  if(widget_enabled(w_hue)) {
    slider_set_pos(ctl_widget_list, w_hue, 
		   get_current_prop(VO_PROP_HUE));
  }
  if(widget_enabled(w_sat)) {
    slider_set_pos(ctl_widget_list, w_sat, 
		   get_current_prop(VO_PROP_SATURATION));
  }
  if(widget_enabled(w_bright)) {
    slider_set_pos(ctl_widget_list, w_bright, 
		   get_current_prop(VO_PROP_BRIGHTNESS));
  }
  if(widget_enabled(w_cont)) {
    slider_set_pos(ctl_widget_list, w_cont, 
		   get_current_prop(VO_PROP_CONTRAST));
  }
}

/*
 * Set hue
 */
static void set_hue(widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_HUE, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set saturation
 */
static void set_saturation(widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_SATURATION, value);
   
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set brightness
 */
static void set_brightness(widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_BRIGHTNESS, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set contrast
 */
static void set_contrast(widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_CONTRAST, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Leaving control panel
 */
void control_exit(widget_t *w, void *data) {

  ctl_running = 0;
  ctl_panel_visible = 0;

  XUnmapWindow(gGlob->gDisplay, ctl_win);

  gui_list_free(ctl_widget_list->l);
  free(ctl_widget_list);
  ctl_widget_list = NULL;

  XDestroyWindow(gGlob->gDisplay, ctl_win);

  if(xdnd_ctl_win)
    free(xdnd_ctl_win);

  ctl_win = 0;
}

/*
 * return 1 if control panel is ON
 */
int control_is_running(void) {

  return ctl_running;
}

/*
 * Return 1 if control panel is visible
 */
int control_is_visible(void) {

  return ctl_panel_visible;
}

/*
 * Raise ctl_win
 */
void control_raise_window(void) {
  
  if(ctl_win) {
    if(ctl_panel_visible && ctl_running) {
      if(ctl_running) {
	XMapRaised(gGlob->gDisplay, ctl_win);
	ctl_panel_visible = 1;
  	XSetTransientForHint (gGlob->gDisplay, ctl_win, gGlob->gVideoWin);
      }
    } else {
      XUnmapWindow (gGlob->gDisplay, ctl_win);
      ctl_panel_visible = 0;
    }
  }
}
/*
 * Hide/show the control panel
 */
void control_toggle_panel_visibility (widget_t *w, void *data) {
  
  if (ctl_panel_visible && ctl_running) {
    ctl_panel_visible = 0;
    XUnmapWindow (gGlob->gDisplay, ctl_win);
  } else {
    if(ctl_running) {
      ctl_panel_visible = 1;
      XMapRaised(gGlob->gDisplay, ctl_win); 
      XSetTransientForHint (gGlob->gDisplay, ctl_win, gGlob->gVideoWin);
    }
  }
}

/*
 * Handle X events here.
 */
void control_handle_event(XEvent *event) {
  XExposeEvent  *myexposeevent;
  static XEvent *old_event;

  if(event->xany.window == ctl_win || event->xany.window == gGlob->gVideoWin) {
    
    switch(event->type) {
    case Expose: {
      myexposeevent = (XExposeEvent *) event;
      
      if(event->xexpose.count == 0) {
	if (event->xany.window == ctl_win)
	  paint_widget_list (ctl_widget_list);
      }
    }
    break;
    
    case MotionNotify:
      /* printf ("MotionNotify\n"); */
      motion_notify_widget_list (ctl_widget_list, 
				 event->xbutton.x, event->xbutton.y);
      /* if window-moving is enabled move the window */
      old_event = event;
      if (ctl_move.enabled) {
	int x,y;
	x = (event->xmotion.x_root) 
	  + (event->xmotion.x_root - old_event->xmotion.x_root) 
	  - ctl_move.offset_x;
	y = (event->xmotion.y_root) 
	  + (event->xmotion.y_root - old_event->xmotion.y_root) 
	  - ctl_move.offset_y;
	
	if(event->xany.window == ctl_win) {
	  /* FIXME XLOCK (); */
	  XMoveWindow(gGlob->gDisplay, ctl_win, x, y);
	  /* FIXME XUNLOCK (); */
	  config_set_int ("x_control",x);
	  config_set_int ("y_control",y);
	}
      }
      break;
      
    case MappingNotify:
      /* printf ("MappingNotify\n");*/
      /* FIXME  XLOCK (); */
      XRefreshKeyboardMapping((XMappingEvent *) event);
      /* FIXME XUNLOCK (); */
      break;
      
      
    case ButtonPress: {
      XButtonEvent *bevent = (XButtonEvent *) event;
      
      /* if no widget is hit enable moving the window */
      if(bevent->window == ctl_win)
	ctl_move.enabled = !click_notify_widget_list (ctl_widget_list, 
						     event->xbutton.x, 
						     event->xbutton.y, 0);
      if (ctl_move.enabled) {
	ctl_move.offset_x = event->xbutton.x;
	ctl_move.offset_y = event->xbutton.y;
      }
    }
    break;
    
    case ButtonRelease:
      click_notify_widget_list (ctl_widget_list, event->xbutton.x, 
				event->xbutton.y, 1);
	ctl_move.enabled = 0; /* disable moving the window       */  
        break;
      
    case ClientMessage:
      if(event->xany.window == ctl_win)
	gui_dnd_process_client_message (xdnd_ctl_win, event);
      break;
      
    }
    }
}

/*
 * Create control panel window
 */
void control_panel(void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  int                     screen;
  char                    title[] = {"Xine Control Panel"};
  Atom                    prop;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;

  /* This shouldn't be happend */
  if(ctl_win) {
    /*  XLOCK (); FIXME  */
    XMapRaised(gGlob->gDisplay, ctl_win); 
    /*  XUNLOCK(); FIXME  */
    ctl_panel_visible = 1;
    ctl_running = 1;
    return;
  }

  ctl_running = 1;

  /*  XLOCK (); FIXME  */

  if (!(ctl_bg_image = Imlib_load_image(gGlob->gImlib_data,
					gui_get_skinfile("CtlBG")))) {
    fprintf(stderr, "xine-playlist: couldn't find image for background\n");
    exit(-1);
  }

  screen = DefaultScreen(gGlob->gDisplay);
  hint.x = config_lookup_int ("x_control", 200);
  hint.y = config_lookup_int ("y_control", 100);
  hint.width = ctl_bg_image->rgb_width;
  hint.height = ctl_bg_image->rgb_height;
  hint.flags = PPosition | PSize;
  
  attr.override_redirect = True;
  ctl_win = XCreateWindow (gGlob->gDisplay, DefaultRootWindow(gGlob->gDisplay),
			   hint.x, hint.y, hint.width, hint.height, 0, 
			   CopyFromParent, CopyFromParent, 
			   CopyFromParent,
			   0, &attr);
  
  XSetStandardProperties(gGlob->gDisplay, ctl_win, title, title,
			 None, NULL, 0, &hint);
  /*
   * wm, no border please
   */

  prop = XInternAtom(gGlob->gDisplay, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGlob->gDisplay, ctl_win, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XSetTransientForHint (gGlob->gDisplay, ctl_win, gGlob->gVideoWin);

  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Control Panel";
    xclasshint->res_class = "Xine";
    XSetClassHint(gGlob->gDisplay, ctl_win, xclasshint);
  }

  gc = XCreateGC(gGlob->gDisplay, ctl_win, 0, 0);

  XSelectInput(gGlob->gDisplay, ctl_win,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(gGlob->gDisplay, ctl_win, wm_hint);
    XFree(wm_hint);
  }
  
  Imlib_apply_image(gGlob->gImlib_data, ctl_bg_image, ctl_win);
  XSync(gGlob->gDisplay, False); 

  /*  XUNLOCK (); FIXME  */
  
  if((xdnd_ctl_win = (DND_struct_t *) xmalloc(sizeof(DND_struct_t))) != NULL) {
    gui_init_dnd(xdnd_ctl_win);
    gui_dnd_set_callback (xdnd_ctl_win, gui_dndcallback);
    gui_make_window_dnd_aware (xdnd_ctl_win, ctl_win);
  }

  /*
   * Widget-list
   */
  ctl_widget_list = (widget_list_t *) xmalloc (sizeof (widget_list_t));
  ctl_widget_list->l = gui_list_new ();
  ctl_widget_list->focusedWidget = NULL;
  ctl_widget_list->pressedWidget = NULL;
  ctl_widget_list->win           = ctl_win;
  ctl_widget_list->gc            = gc;

  { /* All of sliders are disabled by default*/
    widget_t *w;
    int vidcap = 0;
    int min, max, cur;

    /* HUE */
    xine_get_window_property_min_max(gGlob->gXine, VO_PROP_HUE, &min, &max);
    cur = get_current_prop(VO_PROP_HUE);
    gui_list_append_content(ctl_widget_list->l,
	      (w_hue = create_slider(VSLIDER,
				     gui_get_skinX("CtlHueBG"), 
				     gui_get_skinY("CtlHueBG"), 
				     min,max, 
				     1, 
				     gui_get_skinfile("CtlHueBG"),
				     gui_get_skinfile("CtlHueFG"),
				     set_hue, NULL,
				     set_hue, NULL)));
    slider_set_pos(ctl_widget_list, w_hue, cur);
    gui_list_append_content(ctl_widget_list->l,
	      (w = create_label(gui_get_skinX("CtlHueLbl"), 
				gui_get_skinY("CtlHueLbl"), 
				3, "Hue", 
				gui_get_skinfile("CtlHueLbl"))));
    widget_disable(w_hue);

    /* SATURATION */
    xine_get_window_property_min_max(gGlob->gXine, 
				     VO_PROP_SATURATION, &min, &max);
    cur = get_current_prop(VO_PROP_SATURATION);
    gui_list_append_content(ctl_widget_list->l,
	      (w_sat = create_slider(VSLIDER,
				     gui_get_skinX("CtlSatBG"), 
				     gui_get_skinY("CtlSatBG"), 
				     min, max,
				     1, 
				     gui_get_skinfile("CtlSatBG"),
				     gui_get_skinfile("CtlSatFG"),
				     set_saturation, NULL,
				     set_saturation, NULL)));
    slider_set_pos(ctl_widget_list, w_sat, cur);
    gui_list_append_content(ctl_widget_list->l,
	      (w = create_label(gui_get_skinX("CtlSatLbl"), 
				gui_get_skinY("CtlSatLbl"), 
				3, "Sat", 
				gui_get_skinfile("CtlSatLbl"))));
    widget_disable(w_sat);
      
    /* BRIGHTNESS */
    xine_get_window_property_min_max(gGlob->gXine, 
				     VO_PROP_BRIGHTNESS, &min, &max);
    cur = get_current_prop(VO_PROP_BRIGHTNESS);
    gui_list_append_content(ctl_widget_list->l,
	      (w_bright = create_slider(VSLIDER,
					gui_get_skinX("CtlBrightBG"), 
					gui_get_skinY("CtlBrightBG"), 
					min, max,
					1, 
					gui_get_skinfile("CtlBrightBG"),
					gui_get_skinfile("CtlBrightFG"),
					set_brightness, NULL,
					set_brightness, NULL)));
    slider_set_pos(ctl_widget_list, w_bright, cur);
    gui_list_append_content(ctl_widget_list->l,
	      (w = create_label(gui_get_skinX("CtlBrightLbl"), 
				gui_get_skinY("CtlBrightLbl"), 
				3, "Brt", 
				gui_get_skinfile("CtlBrightLbl"))));
    widget_disable(w_bright);
      
    /* CONTRAST */
    xine_get_window_property_min_max(gGlob->gXine, 
				     VO_PROP_CONTRAST, &min, &max);
    cur = get_current_prop(VO_PROP_CONTRAST);
    gui_list_append_content(ctl_widget_list->l,
	      (w_cont = create_slider(VSLIDER,
				      gui_get_skinX("CtlContBG"), 
				      gui_get_skinY("CtlContBG"),
				      min, max,
				      1, 
				      gui_get_skinfile("CtlContBG"),
				      gui_get_skinfile("CtlContFG"),
				      set_contrast, NULL,
				      set_contrast, NULL)));
    slider_set_pos(ctl_widget_list, w_cont, cur);
    gui_list_append_content(ctl_widget_list->l,
	      (w = create_label(gui_get_skinX("CtlContLbl"), 
				gui_get_skinY("CtlContLbl"), 
				3, "Ctr", 
				gui_get_skinfile("CtlContLbl"))));
    widget_disable(w_cont);

    /*
     * Enable only supported settings.
     */
    if((vidcap = xine_get_window_capabilities(gGlob->gXine)) > 0) {

      if(vidcap & VO_CAP_BRIGHTNESS)
	widget_enable(w_bright);
      
      if(vidcap & VO_CAP_SATURATION)
      	widget_enable(w_sat);
      
      if(vidcap & VO_CAP_HUE)
	widget_enable(w_hue);
      
      if(vidcap & VO_CAP_CONTRAST)
	widget_enable(w_cont);
    }
  }
 
  { /*  stopgap button ;-), will gone */
    widget_t *w;

    gui_list_append_content (ctl_widget_list->l, 
	     (w = create_label_button (gui_get_skinX("CtlSave"),
				       gui_get_skinY("CtlSave"),
				       CLICK_BUTTON, NULL,
				       NULL, NULL, 
				       gui_get_skinfile("CtlDummy"),
				       gui_get_ncolor("CtlDummy"),
				       gui_get_fcolor("CtlDummy"),
				       gui_get_ccolor("CtlDummy"))));
    widget_disable(w);
	
    gui_list_append_content (ctl_widget_list->l, 
	     (w = create_label_button (gui_get_skinX("CtlReset"),
				       gui_get_skinY("CtlReset"),
				       CLICK_BUTTON, NULL,
				       NULL, NULL, 
				       gui_get_skinfile("CtlDummy"),
				       gui_get_ncolor("CtlDummy"),
				       gui_get_fcolor("CtlDummy"),
				       gui_get_ccolor("CtlDummy"))));
    widget_disable(w);

    gui_list_append_content (ctl_widget_list->l, 
	     (w = create_label_button (gui_get_skinX("CtlDummy"),
				      gui_get_skinY("CtlDummy"),
				      CLICK_BUTTON, NULL,
				      NULL, NULL, 
				      gui_get_skinfile("CtlDummy"),
				      gui_get_ncolor("CtlDummy"),
				      gui_get_fcolor("CtlDummy"),
				      gui_get_ccolor("CtlDummy"))));
    widget_disable(w);
  }

  gui_list_append_content(ctl_widget_list->l, 
			  create_label_button (gui_get_skinX("CtlDismiss"),
					       gui_get_skinY("CtlDismiss"),
					       CLICK_BUTTON, "Dismiss",
					       control_exit, NULL, 
					       gui_get_skinfile("CtlDismiss"),
					       gui_get_ncolor("CtlDismiss"),
					       gui_get_fcolor("CtlDismiss"),
					       gui_get_ccolor("CtlDismiss")));

  XMapRaised(gGlob->gDisplay, ctl_win); 

  ctl_panel_visible = 1;
}
