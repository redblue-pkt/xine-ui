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
#include <pthread.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "parseskin.h"
#include "utils.h"
#include "actions.h"
#include "xine.h"

extern gGui_t        *gGui;

typedef struct {
  Window              window;
  widget_t           *hue;
  widget_t           *sat;
  widget_t           *bright;
  widget_t           *contr;
  ImlibImage         *bg_image;
  widget_list_t      *widget_list;
  
  int                 running;
  int                 visible;
  widgetkey_t         widget_key;
} _control_t;

static _control_t    *control = NULL;

/*
 * Get current property 'prop' value from vo_driver.
 */
static int get_current_prop(int prop) {
  return (gGui->vo_driver->get_property(gGui->vo_driver, prop));
}

/*
 * set property 'prop' to  value 'value'.
 * vo_driver return value on success, ~value on failure.
 */
static int set_current_prop(int prop, int value) {
  return (gGui->vo_driver->set_property(gGui->vo_driver, prop, value));
}

/*
 * Update silders positions
 */
static void update_sliders_video_settings(void) {

  if(widget_enabled(control->hue)) {
    slider_set_pos(control->widget_list, control->hue, 
		   get_current_prop(VO_PROP_HUE));
  }
  if(widget_enabled(control->sat)) {
    slider_set_pos(control->widget_list, control->sat, 
		   get_current_prop(VO_PROP_SATURATION));
  }
  if(widget_enabled(control->bright)) {
    slider_set_pos(control->widget_list, control->bright, 
		   get_current_prop(VO_PROP_BRIGHTNESS));
  }
  if(widget_enabled(control->contr)) {
    slider_set_pos(control->widget_list, control->contr, 
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
 * Leaving control panel, release memory.
 */
void control_exit(widget_t *w, void *data) {
  window_info_t wi;

  control->running = 0;
  control->visible = 0;

  if((widget_get_window_info(control->widget_key, &wi))) {
    config_set_int("control_x", wi.x);
    config_set_int("control_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  widget_unregister_event_handler(&control->widget_key);
  XUnmapWindow(gGui->display, control->window);


  XDestroyWindow(gGui->display, control->window);
  XFlush(gGui->display);

  Imlib_destroy_image(gGui->imlib_data, control->bg_image);
  control->window = None;

  widget_stop_widgets(control->widget_list);
  gui_list_free(control->widget_list->l);
  free(control->widget_list->gc);
  free(control->widget_list);
  
  free(control);
  control = NULL;

}

/*
 * return 1 if control panel is ON
 */
int control_is_running(void) {

  if(control != NULL)
    return control->running;

  return 0;
}

/*
 * Return 1 if control panel is visible
 */
int control_is_visible(void) {

  if(control != NULL)
    return control->visible;

  return 0;
}

/*
 * Raise control->window
 */
void control_raise_window(void) {
  
  if(control != NULL) {
    if(control->window) {
      if(control->visible && control->running) {
	if(control->running) {
	  XMapRaised(gGui->display, control->window);
	  control->visible = 1;
	  XSetTransientForHint (gGui->display, 
				control->window, gGui->video_window);
	  layer_above_video(control->window);
	}
      } else {
	XUnmapWindow (gGui->display, control->window);
	control->visible = 0;
      }
    }
  }
}
/*
 * Hide/show the control panel
 */
void control_toggle_panel_visibility (widget_t *w, void *data) {
  
  if(control != NULL) {
    if (control->visible && control->running) {
      control->visible = 0;
      widget_hide_widgets(control->widget_list);
      XUnmapWindow (gGui->display, control->window);
    } else {
      if(control->running) {
	control->visible = 1;
	widget_show_widgets(control->widget_list);
	XMapRaised(gGui->display, control->window); 
	XSetTransientForHint (gGui->display, 
			      control->window, gGui->video_window);
	layer_above_video(control->window);
      }
    }
  }
}

/*
 * Handle X events here.
 */
void control_handle_event(XEvent *event, void *data) {

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
 * Create control panel window
 */
void control_panel(void) {
  GC                      gc;
  XSizeHints              hint;
  XWMHints               *wm_hint;
  XSetWindowAttributes    attr;
  char                    title[] = {"Xine Control Panel"};
  Atom                    prop, XA_WIN_LAYER;
  MWMHints                mwmhints;
  XClassHint             *xclasshint;
  xitk_labelbutton_t      lb;
  xitk_label_t            lbl;
  xitk_slider_t           sl;
  long data[1];

  /* This shouldn't be happend */
  if(control != NULL) {
    if(control->window)
      return;
  }
  
  control = (_control_t *) xmalloc(sizeof(_control_t));

  XLockDisplay(gGui->display);
  
  if (!(control->bg_image = Imlib_load_image(gGui->imlib_data,
					gui_get_skinfile("CtlBG")))) {
    fprintf(stderr, "xine-playlist: couldn't find image for background\n");
    exit(-1);
  }

  hint.x = config_lookup_int ("control_x", 200);
  hint.y = config_lookup_int ("control_y", 100);
  hint.width = control->bg_image->rgb_width;
  hint.height = control->bg_image->rgb_height;
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

  control->window = XCreateWindow (gGui->display,
				   DefaultRootWindow(gGui->display),
				   hint.x, hint.y, hint.width, hint.height, 0, 
				   gGui->imlib_data->x.depth, CopyFromParent, 
				   gGui->imlib_data->x.visual,
				   CWBackPixel | CWBorderPixel | CWColormap, &attr);
  
  XSetStandardProperties(gGui->display, control->window, title, title,
 			 None, NULL, 0, &hint);
  
  XSelectInput(gGui->display, control->window,
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
    XChangeProperty(gGui->display, control->window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }
  
  /*
   * wm, no border please
   */

  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, control->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XSetTransientForHint (gGui->display, control->window, gGui->video_window);

  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = "Xine Control Panel";
    xclasshint->res_class = "Xine";
    XSetClassHint(gGui->display, control->window, xclasshint);
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(gGui->display, control->window, wm_hint);
    XFree(wm_hint); /* CHECKME */
  }
  
  gc = XCreateGC(gGui->display, control->window, 0, 0);
  
  Imlib_apply_image(gGui->imlib_data, control->bg_image, control->window);

  /*
   * Widget-list
   */
  control->widget_list                = widget_list_new();
  control->widget_list->l             = gui_list_new ();
  control->widget_list->focusedWidget = NULL;
  control->widget_list->pressedWidget = NULL;
  control->widget_list->win           = control->window;
  control->widget_list->gc            = gc;
  

  { /* All of sliders are disabled by default*/
    int vidcap = 0;
    int min, max, cur;

    lbl.display   = gGui->display;
    lbl.imlibdata = gGui->imlib_data;
    lbl.window    = control->widget_list->win;
    lbl.gc        = control->widget_list->gc;

    /* HUE */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_HUE, &min, &max);
    cur = get_current_prop(VO_PROP_HUE);
    
    sl.display         = gGui->display;
    sl.imlibdata       = gGui->imlib_data;
    sl.x               = gui_get_skinX("CtlHueBG");
    sl.y               = gui_get_skinY("CtlHueBG");
    sl.slider_type     = VSLIDER;
    sl.min             = min;
    sl.max             = max;
    sl.step            = 1;
    sl.background_skin = gui_get_skinfile("CtlHueBG");
    sl.paddle_skin     = gui_get_skinfile("CtlHueFG");
    sl.callback        = set_hue;
    sl.userdata        = NULL;
    sl.motion_callback = set_hue;
    sl.motion_userdata = NULL;
    
    gui_list_append_content(control->widget_list->l,
			    (control->hue = slider_create(&sl)));
    slider_set_pos(control->widget_list, control->hue, cur);

    lbl.x         = gui_get_skinX("CtlHueLbl");
    lbl.y         = gui_get_skinY("CtlHueLbl");
    lbl.length    = gui_get_label_length("CtlHueLbl");
    lbl.label     = "Hue";
    lbl.font      = gui_get_skinfile("CtlHueLbl");
    lbl.animation = gui_get_animation("CtlHueLbl");

    gui_list_append_content(control->widget_list->l,
			    label_create(&lbl));
    widget_disable(control->hue);


    /* SATURATION */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_SATURATION, &min, &max);
    cur = get_current_prop(VO_PROP_SATURATION);

    sl.display         = gGui->display;
    sl.imlibdata       = gGui->imlib_data;
    sl.x               = gui_get_skinX("CtlSatBG");
    sl.y               = gui_get_skinY("CtlSatBG");
    sl.slider_type     = VSLIDER;
    sl.min             = min;
    sl.max             = max;
    sl.step            = 1;
    sl.background_skin = gui_get_skinfile("CtlSatBG");
    sl.paddle_skin     = gui_get_skinfile("CtlSatFG");
    sl.callback        = set_saturation;
    sl.userdata        = NULL;
    sl.motion_callback = set_saturation;
    sl.motion_userdata = NULL;
    
    gui_list_append_content(control->widget_list->l,
	      (control->sat = slider_create(&sl)));
    slider_set_pos(control->widget_list, control->sat, cur);

    lbl.x         = gui_get_skinX("CtlSatLbl");
    lbl.y         = gui_get_skinY("CtlSatLbl");
    lbl.length    = gui_get_label_length("CtlSatLbl");
    lbl.label     = "Sat";
    lbl.font      = gui_get_skinfile("CtlSatLbl");
    lbl.animation = gui_get_animation("CtlSatLbl");

    gui_list_append_content(control->widget_list->l,
			    label_create(&lbl));
    widget_disable(control->sat);
      
    /* BRIGHTNESS */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_BRIGHTNESS, &min, &max);
    cur = get_current_prop(VO_PROP_BRIGHTNESS);

    sl.display         = gGui->display;
    sl.imlibdata       = gGui->imlib_data;
    sl.x               = gui_get_skinX("CtlBrightBG");
    sl.y               = gui_get_skinY("CtlBrightBG");
    sl.slider_type     = VSLIDER;
    sl.min             = min;
    sl.max             = max;
    sl.step            = 1;
    sl.background_skin = gui_get_skinfile("CtlBrightBG");
    sl.paddle_skin     = gui_get_skinfile("CtlBrightFG");
    sl.callback        = set_brightness;
    sl.userdata        = NULL;
    sl.motion_callback = set_brightness;
    sl.motion_userdata = NULL;

    gui_list_append_content(control->widget_list->l,
	    (control->bright = slider_create(&sl)));
    slider_set_pos(control->widget_list, control->bright, cur);

    lbl.x         = gui_get_skinX("CtlBrightLbl");
    lbl.y         = gui_get_skinY("CtlBrightLbl");
    lbl.length    = gui_get_label_length("CtlBrightLbl");
    lbl.label     = "Brt";
    lbl.font      = gui_get_skinfile("CtlBrightLbl");
    lbl.animation = gui_get_animation("CtlBrightLbl");

    gui_list_append_content(control->widget_list->l,
			    label_create(&lbl));
    widget_disable(control->bright);
      
    /* CONTRAST */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_CONTRAST, &min, &max);
    cur = get_current_prop(VO_PROP_CONTRAST);

    sl.display         = gGui->display;
    sl.imlibdata       = gGui->imlib_data;
    sl.x               = gui_get_skinX("CtlContBG");
    sl.y               = gui_get_skinY("CtlContBG");
    sl.slider_type     = VSLIDER;
    sl.min             = min;
    sl.max             = max;
    sl.step            = 1;
    sl.background_skin = gui_get_skinfile("CtlContBG");
    sl.paddle_skin     = gui_get_skinfile("CtlContFG");
    sl.callback        = set_contrast;
    sl.userdata        = NULL;
    sl.motion_callback = set_contrast;
    sl.motion_userdata = NULL;

    gui_list_append_content(control->widget_list->l,
	      (control->contr = slider_create(&sl)));
    slider_set_pos(control->widget_list, control->contr, cur);

    lbl.x         = gui_get_skinX("CtlContLbl");
    lbl.y         = gui_get_skinY("CtlContLbl");
    lbl.length    = gui_get_label_length("CtlContLbl");
    lbl.label     = "Ctr";
    lbl.font      = gui_get_skinfile("CtlContLbl");
    lbl.animation = gui_get_animation("CtlContLbl");

    gui_list_append_content(control->widget_list->l,
			    label_create(&lbl));
    widget_disable(control->contr);


    /*
     * Enable only supported settings.
     */
    if((vidcap = gGui->vo_driver->get_capabilities(gGui->vo_driver)) > 0) {

      if(vidcap & VO_CAP_BRIGHTNESS)
	widget_enable(control->bright);
      
      if(vidcap & VO_CAP_SATURATION)
      	widget_enable(control->sat);
      
      if(vidcap & VO_CAP_HUE)
	widget_enable(control->hue);
      
      if(vidcap & VO_CAP_CONTRAST)
	widget_enable(control->contr);
    }
  }
 
  lb.display        = gGui->display;
  lb.imlibdata      = gGui->imlib_data;

  /*  stopgap button ;-), will gone */
  /*
  {
    widget_t *w;
    
    lb.x              = gui_get_skinX("CtlSave");
    lb.y              = gui_get_skinY("CtlSave");
    lb.button_type    = CLICK_BUTTON;
    lb.label          = NULL;
    lb.callback       = NULL;
    lb.state_callback = NULL;
    lb.userdata       = NULL;
    lb.skin           = gui_get_skinfile("CtlDummy");
    lb.normcolor      = gui_get_ncolor("CtlDummy");
    lb.focuscolor     = gui_get_fcolor("CtlDummy");
    lb.clickcolor     = gui_get_ccolor("CtlDummy");
    lb.fonct
    gui_list_append_content (control->widget_list->l, 
	     (w = label_button_create (&lb)));

    widget_disable(w);
	
    lb.x              = gui_get_skinX("CtlReset");
    lb.y              = gui_get_skinY("CtlReset");
    lb.button_type    = CLICK_BUTTON;
    lb.label          = NULL;
    lb.callback       = NULL;
    lb.state_callback = NULL;
    lb.userdata       = NULL;
    lb.skin           = gui_get_skinfile("CtlDummy");
    lb.normcolor      = gui_get_ncolor("CtlDummy");
    lb.focuscolor     = gui_get_fcolor("CtlDummy");
    lb.clickcolor     = gui_get_ccolor("CtlDummy");

    gui_list_append_content (control->widget_list->l, 
	     (w = label_button_create (&lb)));

    widget_disable(w);

    lb.x              = gui_get_skinX("CtlDummy");
    lb.y              = gui_get_skinY("CtlDummy");
    lb.button_type    = CLICK_BUTTON;
    lb.label          = NULL;
    lb.callback       = NULL;
    lb.state_callback = NULL;
    lb.userdata       = NULL;
    lb.skin           = gui_get_skinfile("CtlDummy");
    lb.normcolor      = gui_get_ncolor("CtlDummy");
    lb.focuscolor     = gui_get_fcolor("CtlDummy");
    lb.clickcolor     = gui_get_ccolor("CtlDummy");

    gui_list_append_content (control->widget_list->l, 
	     (w = label_button_create (&lb)));

    widget_disable(w);
  }
  */
  
  lb.x              = gui_get_skinX("CtlDismiss");
  lb.y              = gui_get_skinY("CtlDismiss");
  lb.button_type    = CLICK_BUTTON;
  lb.label          = "Dismiss";
  lb.callback       = control_exit;
  lb.state_callback = NULL;
  lb.userdata       = NULL;
  lb.skin           = gui_get_skinfile("CtlDismiss");
  lb.normcolor      = gui_get_ncolor("CtlDismiss");
  lb.focuscolor     = gui_get_fcolor("CtlDismiss");
  lb.clickcolor     = gui_get_ccolor("CtlDismiss");
  lb.fontname       = gui_get_fontname("CtlDismiss");

  gui_list_append_content (control->widget_list->l, 
			  label_button_create (&lb));

  XMapRaised(gGui->display, control->window); 

  control->widget_key = 
    widget_register_event_handler("control", 
				  control->window, 
				  control_handle_event, 
				  NULL,
				  gui_dndcallback,
				  control->widget_list, NULL);
  
  control->visible = 1;
  control->running = 1;

  XUnlockDisplay (gGui->display);
}
