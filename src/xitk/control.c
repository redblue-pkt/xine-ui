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

#include <xine.h>
#include <xine/xineutils.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "actions.h"
#include "skins.h"
#include "errors.h"

#ifndef __GNUC__
#define __FUNCTION__    __func__
#endif

extern gGui_t          *gGui;

typedef struct {
  Window                window;
  xitk_widget_t        *hue;
  xitk_widget_t        *sat;
  xitk_widget_t        *bright;
  xitk_widget_t        *contr;
  ImlibImage           *bg_image;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *skinlist;
  char                 *skins[64];
  int                   skins_num;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

  xitk_widget_t         *combo;

} _control_t;

static _control_t    *control = NULL;

/*
 *
 */
void control_show_tips(int enabled, unsigned long timeout) {
  
  if(control) {
    if(enabled)
      xitk_set_widgets_tips_timeout(control->widget_list, timeout);
    else
      xitk_disable_widgets_tips(control->widget_list);
  }
}

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

  if(xitk_is_widget_enabled(control->hue)) {
    xitk_slider_set_pos(control->widget_list, control->hue, 
			get_current_prop(VO_PROP_HUE));
  }
  if(xitk_is_widget_enabled(control->sat)) {
    xitk_slider_set_pos(control->widget_list, control->sat, 
			get_current_prop(VO_PROP_SATURATION));
  }
  if(xitk_is_widget_enabled(control->bright)) {
    xitk_slider_set_pos(control->widget_list, control->bright, 
			get_current_prop(VO_PROP_BRIGHTNESS));
  }
  if(xitk_is_widget_enabled(control->contr)) {
    xitk_slider_set_pos(control->widget_list, control->contr, 
			get_current_prop(VO_PROP_CONTRAST));
  }
}

/*
 * Set hue
 */
static void set_hue(xitk_widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_HUE, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set saturation
 */
static void set_saturation(xitk_widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_SATURATION, value);
   
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set brightness
 */
static void set_brightness(xitk_widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_BRIGHTNESS, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Set contrast
 */
static void set_contrast(xitk_widget_t *w, void *data, int value) {
  int ret = 0;

  ret = set_current_prop(VO_PROP_CONTRAST, value);
    
  if(ret != value)
    update_sliders_video_settings();
}

/*
 * Leaving control panel, release memory.
 */
void control_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;
  int           i;

  if(control) {
    control->running = 0;
    control->visible = 0;

    if((xitk_get_window_info(control->widget_key, &wi))) {
      gGui->config->update_num (gGui->config, "gui.control_x", wi.x);
      gGui->config->update_num (gGui->config, "gui.control_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler(&control->widget_key);

    XLockDisplay(gGui->display);
    XUnmapWindow(gGui->display, control->window);
    XUnlockDisplay(gGui->display);

    xitk_destroy_widgets(control->widget_list);

    XLockDisplay(gGui->display);
    XDestroyWindow(gGui->display, control->window);
    Imlib_destroy_image(gGui->imlib_data, control->bg_image);
    XUnlockDisplay(gGui->display);

    control->window = None;
    xitk_list_free(control->widget_list->l);

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, control->widget_list->gc);
    XUnlockDisplay(gGui->display);

    free(control->widget_list);

    for(i = 0; i < control->skins_num; i++)
      free(control->skins[i]);

    free(control);
    control = NULL;
  }
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
	  XLockDisplay(gGui->display);
	  XSetTransientForHint (gGui->display, 
				control->window, gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(control->window);
	}
      } else {
	XLockDisplay(gGui->display);
	XUnmapWindow (gGui->display, control->window);
	XUnlockDisplay(gGui->display);
	control->visible = 0;
      }
    }
  }
}

/*
 * Hide/show the control panel
 */
void control_toggle_visibility (xitk_widget_t *w, void *data) {
  
  if(control != NULL) {
    if (control->visible && control->running) {
      control->visible = 0;
      xitk_hide_widgets(control->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, control->window);
      XUnlockDisplay(gGui->display);
    } else {
      if(control->running) {
	control->visible = 1;
	xitk_show_widgets(control->widget_list);
	XLockDisplay(gGui->display);
	XMapRaised(gGui->display, control->window); 
	XSetTransientForHint (gGui->display, 
			      control->window, gGui->video_window);
	XUnlockDisplay(gGui->display);
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

  case ConfigureNotify:
    /*  xitk_combo_update_pos(control->combo); */
    break;

  }
}

/*
 * Change the current skin.
 */
void control_change_skins(void) {
  XEvent        xev;
  ImlibImage   *new_img, *old_img;
 
  if(control_is_running()) {
    
    xitk_skin_lock(gGui->skin_config);

    XLockDisplay(gGui->display);
    
    XUnmapWindow(gGui->display, control->window);
    
    if(!(new_img = Imlib_load_image(gGui->imlib_data,
				    xitk_skin_get_skin_filename(gGui->skin_config, "CtlBG")))) {
      xine_error(_("%s(): couldn't find image for background\n"), __FUNCTION__);
      exit(-1);
    }
    
    XResizeWindow (gGui->display, control->window,
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);
    
    old_img = control->bg_image;
    control->bg_image = new_img;
    
    Imlib_destroy_image(gGui->imlib_data, old_img);
    
    XMapRaised(gGui->display, control->window); 
    XSetTransientForHint(gGui->display, control->window, gGui->video_window);
    
    do  {
      XMaskEvent(gGui->display, StructureNotifyMask, &xev) ;
    } while (xev.type != MapNotify || xev.xmap.event != control->window);
    
    Imlib_apply_image(gGui->imlib_data, new_img, control->window);
        
    XUnlockDisplay(gGui->display);

    xitk_skin_unlock(gGui->skin_config);
    
    xitk_change_skins_widget_list(control->widget_list, gGui->skin_config);
    xitk_paint_widget_list(control->widget_list);
  }
}

/*
 *
 */
static void control_select_new_skin(xitk_widget_t *w, void *data) {
  skins_locations_t **sks = get_available_skins();
  int                 selected = (int)data;

  xitk_browser_release_all_buttons(control->skinlist);
  select_new_skin(sks[selected]);
}

/*
 * Create control panel window
 */
void control_panel(void) {
  GC                         gc;
  XSizeHints                 hint;
  XWMHints                  *wm_hint;
  XSetWindowAttributes       attr;
  char                       title[] = {"Xine Control Panel"};
  Atom                       prop, XA_WIN_LAYER;
  MWMHints                   mwmhints;
  XClassHint                *xclasshint;
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_slider_widget_t       sl;
  xitk_combo_widget_t        cmb;
  xitk_widget_t             *w;
  long                       data[1];

  /* This shouldn't be happend */
  if(control != NULL) {
    if(control->window)
      return;
  }
  
  XITK_WIDGET_INIT(&br, gGui->imlib_data);
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);

  control = (_control_t *) xine_xmalloc(sizeof(_control_t));

  XLockDisplay(gGui->display);
  
  if (!(control->bg_image = 
	Imlib_load_image(gGui->imlib_data,
			 xitk_skin_get_skin_filename(gGui->skin_config, "CtlBG")))) {
    xine_error(_("xine-playlist: couldn't find image for background\n"));
    exit(-1);
  }

  hint.x = gGui->config->register_num (gGui->config, "gui.control_x", 200,
				       NULL, NULL, NULL, NULL);
  hint.y = gGui->config->register_num (gGui->config, "gui.control_y", 100,
				       NULL, NULL, NULL, NULL);
  hint.width = control->bg_image->rgb_width;
  hint.height = control->bg_image->rgb_height;
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
  attr.border_pixel      = 1;
  attr.colormap		 = Imlib_get_colormap(gGui->imlib_data);

  control->window = XCreateWindow (gGui->display,
				   gGui->imlib_data->x.root,
				   hint.x, hint.y, hint.width, hint.height, 0, 
				   gGui->imlib_data->x.depth, CopyFromParent, 
				   gGui->imlib_data->x.visual,
				   CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
				   &attr);
  
  XSetStandardProperties(gGui->display, control->window, title, title,
 			 None, NULL, 0, &hint);
  
  XSelectInput(gGui->display, control->window,
	       ButtonPressMask | ButtonReleaseMask | KeymapStateMask | PointerMotionMask
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
  control->widget_list                = xitk_widget_list_new();
  control->widget_list->l             = xitk_list_new ();
  control->widget_list->focusedWidget = NULL;
  control->widget_list->pressedWidget = NULL;
  control->widget_list->win           = control->window;
  control->widget_list->gc            = gc;
  
  { /* All of sliders are disabled by default*/
    int vidcap = 0;
    int min, max, cur;

    lbl.window = control->widget_list->win;
    lbl.gc     = control->widget_list->gc;

    /* HUE */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_HUE, &min, &max);
    cur = get_current_prop(VO_PROP_HUE);
    
    sl.skin_element_name = "SliderCtlHue";
    sl.min               = min;
    sl.max               = max;
    sl.step              = 1;
    sl.callback          = set_hue;
    sl.userdata          = NULL;
    sl.motion_callback   = set_hue;
    sl.motion_userdata   = NULL;
    xitk_list_append_content(control->widget_list->l,
			    (control->hue = xitk_slider_create(gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->widget_list, control->hue, cur);
    xitk_set_widget_tips(control->hue, _("Control HUE value"));
  
    lbl.skin_element_name = "CtlHueLbl";
    lbl.label             = _("Hue");
    xitk_list_append_content(control->widget_list->l,
			    xitk_label_create(gGui->skin_config, &lbl));
    xitk_disable_widget(control->hue);

    /* SATURATION */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_SATURATION, &min, &max);
    cur = get_current_prop(VO_PROP_SATURATION);

    sl.skin_element_name = "SliderCtlSat";
    sl.min               = min;
    sl.max               = max;
    sl.step              = 1;
    sl.callback          = set_saturation;
    sl.userdata          = NULL;
    sl.motion_callback   = set_saturation;
    sl.motion_userdata   = NULL;
    xitk_list_append_content(control->widget_list->l,
	      (control->sat = xitk_slider_create(gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->widget_list, control->sat, cur);
    xitk_set_widget_tips(control->sat, _("Control SATURATION value"));

    lbl.skin_element_name = "CtlSatLbl";
    lbl.label             = _("Sat");
    xitk_list_append_content(control->widget_list->l,
			    xitk_label_create(gGui->skin_config, &lbl));
    xitk_disable_widget(control->sat);
      
    /* BRIGHTNESS */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_BRIGHTNESS, &min, &max);
    cur = get_current_prop(VO_PROP_BRIGHTNESS);

    sl.skin_element_name = "SliderCtlBright";
    sl.min               = min;
    sl.max               = max;
    sl.step              = 1;
    sl.callback          = set_brightness;
    sl.userdata          = NULL;
    sl.motion_callback   = set_brightness;
    sl.motion_userdata   = NULL;
    xitk_list_append_content(control->widget_list->l,
	    (control->bright = xitk_slider_create(gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->widget_list, control->bright, cur);
    xitk_set_widget_tips(control->bright, _("Control BRIGHTNESS value"));

    lbl.skin_element_name = "CtlBrightLbl";
    lbl.label             = _("Brt");
    xitk_list_append_content(control->widget_list->l,
			    xitk_label_create(gGui->skin_config, &lbl));
    xitk_disable_widget(control->bright);
      
    /* CONTRAST */
    gGui->vo_driver->get_property_min_max(gGui->vo_driver, 
					  VO_PROP_CONTRAST, &min, &max);
    cur = get_current_prop(VO_PROP_CONTRAST);

    sl.skin_element_name = "SliderCtlCont";
    sl.min               = min;
    sl.max               = max;
    sl.step              = 1;
    sl.callback          = set_contrast;
    sl.userdata          = NULL;
    sl.motion_callback   = set_contrast;
    sl.motion_userdata   = NULL;
    xitk_list_append_content(control->widget_list->l,
	      (control->contr = xitk_slider_create(gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->widget_list, control->contr, cur);
    xitk_set_widget_tips(control->contr, _("Control CONTRAST value"));

    lbl.skin_element_name = "CtlContLbl";
    lbl.label             = _("Ctr");
    xitk_list_append_content(control->widget_list->l,
			    xitk_label_create(gGui->skin_config, &lbl));
    xitk_disable_widget(control->contr);

    /*
     * Enable only supported settings.
     */
    if((vidcap = gGui->vo_driver->get_capabilities(gGui->vo_driver)) > 0) {

      if(vidcap & VO_CAP_BRIGHTNESS)
	xitk_enable_widget(control->bright);
      
      if(vidcap & VO_CAP_SATURATION)
      	xitk_enable_widget(control->sat);
      
      if(vidcap & VO_CAP_HUE)
	xitk_enable_widget(control->hue);
      
      if(vidcap & VO_CAP_CONTRAST)
	xitk_enable_widget(control->contr);
    }
  }

  {
    skins_locations_t **sks = get_available_skins();
    int i;

    control->skins_num = get_available_skins_num();

    for(i = 0; i < control->skins_num; i++)
      control->skins[i] = strdup(sks[i]->skin);

  }
  
  lbl.skin_element_name = "CtlSkLbl";
  lbl.label             = _("Choose a Skin");
  xitk_list_append_content(control->widget_list->l,
			   xitk_label_create(gGui->skin_config, &lbl));

  br.arrow_up.skin_element_name    = "CtlSkUp";
  br.slider.skin_element_name      = "SliderCtlSk";
  br.arrow_dn.skin_element_name    = "CtlSkDn";
  br.browser.skin_element_name     = "CtlSkItemBtn";
  br.browser.max_displayed_entries = 5;
  br.browser.num_entries           = control->skins_num;
  br.browser.entries               = control->skins;
  br.callback                      = control_select_new_skin;
  br.dbl_click_callback            = NULL;
  br.dbl_click_time                = DEFAULT_DBL_CLICK_TIME;
  br.parent_wlist                  = control->widget_list;
  xitk_list_append_content (control->widget_list->l, 
			   (control->skinlist = 
			    xitk_browser_create(gGui->skin_config, &br)));

  xitk_browser_update_list(control->skinlist, 
			   control->skins, control->skins_num, 0);

  /* Temporary test, don't remove me.
  cmb.skin_element_name = "combotest";
  cmb.layer_above       = gGui->layer_above;
  cmb.parent_wlist      = control->widget_list;
  cmb.entries           = control->skins;
  cmb.parent_wkey       = &control->widget_key;
  cmb.callback          = NULL;
  cmb.userdata          = NULL;
  xitk_list_append_content(control->widget_list->l, 
			   (control->combo = xitk_combo_create(gGui->skin_config, &cmb)));
  xitk_combo_set_select(control->widget_list, control->combo, 3);
  */

  lb.skin_element_name = "CtlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.callback          = control_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content (control->widget_list->l, 
			    (w = xitk_labelbutton_create (gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Close control window"));

  XMapRaised(gGui->display, control->window); 

  control->widget_key = 
    xitk_register_event_handler("control", 
				control->window, 
				control_handle_event, 
				NULL,
				gui_dndcallback,
				control->widget_list, NULL);
  
  control->visible = 1;
  control->running = 1;

  XUnlockDisplay (gGui->display);
}
