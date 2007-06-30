/* 
 * Copyright (C) 2000-2007 the xine project
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

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

#define CONTROL_MIN     0
#define CONTROL_MAX     65535
#define CONTROL_STEP    565	/* approx. 1 pixel slider step */

#define TEST_VO_VALUE(val)  (val < CONTROL_MAX/3 || val > CONTROL_MAX*2/3) ? (CONTROL_MAX - val) : 0

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
  const char           *skins[64];
  int                   skins_num;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

  xitk_widget_t         *combo;

} _control_t;

static _control_t    *control = NULL;
static int hue_ena, sat_ena, bright_ena, contr_ena;


static void hue_changes_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->video_settings.hue =
    (cfg->num_value < 0) ? gGui->video_settings.default_hue : cfg->num_value;
}
static void brightness_changes_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->video_settings.brightness =
    (cfg->num_value < 0) ? gGui->video_settings.default_brightness : cfg->num_value;
}
static void saturation_changes_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->video_settings.saturation =
    (cfg->num_value < 0) ? gGui->video_settings.default_saturation : cfg->num_value;
}
static void contrast_changes_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->video_settings.contrast =
    (cfg->num_value < 0) ? gGui->video_settings.default_contrast : cfg->num_value;
}

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

void control_update_tips_timeout(unsigned long timeout) {
  if(control)
    xitk_set_widgets_tips_timeout(control->widget_list, timeout);
}

/*
 * Get current parameter 'param' value.
 */
static int get_current_param(int param) {
  return (xine_get_param(gGui->stream, param));
}

/*
 * set parameter 'param' to value 'value'.
 */
static void set_current_param(int param, int value) {
  xine_set_param(gGui->stream, param, value);
}

/*
 * Update silders positions
 */
static void update_sliders_video_settings(void) {

  if(control) {
    if(xitk_is_widget_enabled(control->hue)) {
      xitk_slider_set_pos(control->hue, get_current_param(XINE_PARAM_VO_HUE));
    }
    if(xitk_is_widget_enabled(control->sat)) {
      xitk_slider_set_pos(control->sat, get_current_param(XINE_PARAM_VO_SATURATION));
    }
    if(xitk_is_widget_enabled(control->bright)) {
      xitk_slider_set_pos(control->bright, get_current_param(XINE_PARAM_VO_BRIGHTNESS));
    }
    if(xitk_is_widget_enabled(control->contr)) {
      xitk_slider_set_pos(control->contr, get_current_param(XINE_PARAM_VO_CONTRAST));
    }
  }
}

/*
 * Set hue
 */
static void set_hue(xitk_widget_t *w, void *data, int value) {
  int hue;

  set_current_param(XINE_PARAM_VO_HUE, value);
  
  if((hue = get_current_param(XINE_PARAM_VO_HUE)) != value)
    update_sliders_video_settings();
  
  if(hue_ena)
    config_update_num("gui.vo_hue", hue);
}

/*
 * Set saturation
 */
static void set_saturation(xitk_widget_t *w, void *data, int value) {
  int saturation;

  set_current_param(XINE_PARAM_VO_SATURATION, value);
   
  if((saturation = get_current_param(XINE_PARAM_VO_SATURATION)) != value)
    update_sliders_video_settings();

  if(sat_ena)
    config_update_num("gui.vo_saturation", saturation);
}

/*
 * Set brightness
 */
static void set_brightness(xitk_widget_t *w, void *data, int value) {
  int brightness;

  set_current_param(XINE_PARAM_VO_BRIGHTNESS, value);
    
  if((brightness = get_current_param(XINE_PARAM_VO_BRIGHTNESS)) != value)
    update_sliders_video_settings();
  
  if(bright_ena)
    config_update_num("gui.vo_brightness", brightness);
}

/*
 * Set contrast
 */
static void set_contrast(xitk_widget_t *w, void *data, int value) {
  int contrast;

  set_current_param(XINE_PARAM_VO_CONTRAST, value);
  
  if((contrast = get_current_param(XINE_PARAM_VO_CONTRAST)) != value)
    update_sliders_video_settings();
  
  if(contr_ena)
    config_update_num("gui.vo_contrast", contrast);
}

void control_set_image_prop(int prop, int value) {
  switch(prop) {
  case XINE_PARAM_VO_HUE:
    set_current_param(XINE_PARAM_VO_HUE, value);
    break;
  case XINE_PARAM_VO_SATURATION:
    set_current_param(XINE_PARAM_VO_SATURATION, value);
    break;
  case XINE_PARAM_VO_BRIGHTNESS:
    set_current_param(XINE_PARAM_VO_BRIGHTNESS, value);
    break;
  case XINE_PARAM_VO_CONTRAST:
    set_current_param(XINE_PARAM_VO_CONTRAST, value);
    break;
  default:
    return;
    break;
  }
  update_sliders_video_settings();
}

static int test_vo_property(int property) {
  int cur;

  cur = get_current_param(property);
  set_current_param(property, TEST_VO_VALUE(cur));
  if((get_current_param(property)) == cur)
    return 0;
  else {
    set_current_param(property, cur);
    return 1;
  }
}


/*
 * Enable or disable video settings sliders.
 */
static void active_sliders_video_settings(void) {
  
  if(!bright_ena)
    xitk_disable_widget(control->bright);
  else
    xitk_enable_widget(control->bright);
  
  if(!sat_ena)
    xitk_disable_widget(control->sat);
  else
    xitk_enable_widget(control->sat);

  if(!hue_ena)
    xitk_disable_widget(control->hue);
  else
    xitk_enable_widget(control->hue);

  if(!contr_ena)
    xitk_disable_widget(control->contr);
  else
    xitk_enable_widget(control->contr);
}

static void probe_active_controls(void) {
  hue_ena    = test_vo_property(XINE_PARAM_VO_HUE);
  bright_ena = test_vo_property(XINE_PARAM_VO_BRIGHTNESS);
  sat_ena    = test_vo_property(XINE_PARAM_VO_SATURATION);
  contr_ena  = test_vo_property(XINE_PARAM_VO_CONTRAST);
}

/*
 * Reset video settings.
 */
void control_reset(void) {
  
  if(hue_ena) {
    set_current_param(XINE_PARAM_VO_HUE, gGui->video_settings.default_hue);
    config_update_num("gui.vo_hue", -1);
  }

  if(sat_ena) {
    set_current_param(XINE_PARAM_VO_SATURATION, gGui->video_settings.default_saturation);
    config_update_num("gui.vo_saturation", -1);
  }

  if(bright_ena) {
    set_current_param(XINE_PARAM_VO_BRIGHTNESS, gGui->video_settings.default_brightness);
    config_update_num("gui.vo_brightness", -1);
  }

  if(contr_ena) {
    set_current_param(XINE_PARAM_VO_CONTRAST, gGui->video_settings.default_contrast);
    config_update_num("gui.vo_contrast", -1);
  }

  update_sliders_video_settings();
}

void control_config_register(void) {
  
  probe_active_controls();
  
  if(hue_ena) {
    gGui->video_settings.hue = 
     xine_config_register_range(gGui->xine, "gui.vo_hue",
				-1,
				CONTROL_MIN, CONTROL_MAX,
				CONFIG_NO_DESC, /* _("hue value"), */
				CONFIG_NO_HELP, /* _("Hue value."), */
				CONFIG_LEVEL_DEB,
				hue_changes_cb, 
				CONFIG_NO_DATA);
    gGui->video_settings.default_hue = get_current_param(XINE_PARAM_VO_HUE);
    if(gGui->video_settings.hue < 0)
      gGui->video_settings.hue = gGui->video_settings.default_hue;
    else {
      set_current_param(XINE_PARAM_VO_HUE, gGui->video_settings.hue);
      gGui->video_settings.hue = get_current_param(XINE_PARAM_VO_HUE);
    }
  }
  
  if(bright_ena) {
    gGui->video_settings.brightness = 
     xine_config_register_range(gGui->xine, "gui.vo_brightness",
				-1,
				CONTROL_MIN, CONTROL_MAX,
				CONFIG_NO_DESC, /* _("brightness value"), */
				CONFIG_NO_HELP, /* _("Brightness value."), */
				CONFIG_LEVEL_DEB,
				brightness_changes_cb, 
				CONFIG_NO_DATA);
    gGui->video_settings.default_brightness = get_current_param(XINE_PARAM_VO_BRIGHTNESS);
    if(gGui->video_settings.brightness < 0)
      gGui->video_settings.brightness = gGui->video_settings.default_brightness;
    else {
      set_current_param(XINE_PARAM_VO_BRIGHTNESS, gGui->video_settings.brightness);
      gGui->video_settings.brightness = get_current_param(XINE_PARAM_VO_BRIGHTNESS);
    }
  }

  if(sat_ena) {
   gGui->video_settings.saturation = 
     xine_config_register_range(gGui->xine, "gui.vo_saturation",
				-1,
				CONTROL_MIN, CONTROL_MAX,
				CONFIG_NO_DESC, /* _("saturation value"), */
				CONFIG_NO_HELP, /* _("Saturation value."), */
				CONFIG_LEVEL_DEB,
				saturation_changes_cb, 
				CONFIG_NO_DATA);
    gGui->video_settings.default_saturation = get_current_param(XINE_PARAM_VO_SATURATION);
    if(gGui->video_settings.saturation < 0)
      gGui->video_settings.saturation = gGui->video_settings.default_saturation;
    else {
      set_current_param(XINE_PARAM_VO_SATURATION, gGui->video_settings.saturation);
      gGui->video_settings.saturation = get_current_param(XINE_PARAM_VO_SATURATION);
    }
  }

  if(contr_ena) {
    gGui->video_settings.contrast = 
     xine_config_register_range(gGui->xine, "gui.vo_contrast",
				-1,
				CONTROL_MIN, CONTROL_MAX,
				CONFIG_NO_DESC, /* _("contrast value"), */
				CONFIG_NO_HELP, /* _("Contrast value."), */
				CONFIG_LEVEL_DEB,
				contrast_changes_cb, 
				CONFIG_NO_DATA);
    gGui->video_settings.default_contrast = get_current_param(XINE_PARAM_VO_CONTRAST);
    if(gGui->video_settings.contrast < 0)
      gGui->video_settings.contrast = gGui->video_settings.default_contrast;
    else {
      set_current_param(XINE_PARAM_VO_CONTRAST, gGui->video_settings.contrast);
      gGui->video_settings.contrast = get_current_param(XINE_PARAM_VO_CONTRAST);
    }
  }
}

/*
 * Leaving control panel, release memory.
 */
void control_exit(xitk_widget_t *w, void *data) {

  if(control) {
    window_info_t wi;
    int           i;
    
    control->running = 0;
    control->visible = 0;

    if((xitk_get_window_info(control->widget_key, &wi))) {
      config_update_num("gui.control_x", wi.x);
      config_update_num("gui.control_y", wi.y);
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
    xitk_list_free((XITK_WIDGET_LIST_LIST(control->widget_list)));

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(control->widget_list)));
    XUnlockDisplay(gGui->display);

    free(control->widget_list);

    for(i = 0; i < control->skins_num; i++)
      free((char *)control->skins[i]);

    free(control);
    control = NULL;

    try_to_set_input_focus(gGui->video_window);
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

  if(control != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, control->window);
    else
      return control->visible && xitk_is_window_visible(gGui->display, control->window);
  }

  return 0;
}

/*
 * Raise control->window
 */
void control_raise_window(void) {
  if(control != NULL)
    raise_window(control->window, control->visible, control->running);
}

/*
 * Hide/show the control panel
 */
void control_toggle_visibility (xitk_widget_t *w, void *data) {
  if(control != NULL)
    toggle_window(control->window, control->widget_list,
		  &control->visible, control->running);
}

/*
 * Handle X events here.
 */
static void control_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {
  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    
    if(bevent->button == Button3) {
      int wx, wy;
      
      xitk_get_window_position(gGui->display, control->window, &wx, &wy, NULL, NULL);
      control_menu(control->widget_list, bevent->x + wx, bevent->y + wy);
    }
  }

  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      control_exit(NULL, NULL);
    else
      gui_handle_event(event, data);
    break;
    
  }
  
}

/*
 * Change the current skin.
 */
void control_change_skins(int synthetic) {
  ImlibImage   *new_img, *old_img;
  XSizeHints    hint;

  if(control_is_running()) {
    
    xitk_skin_lock(gGui->skin_config);
    xitk_hide_widgets(control->widget_list);

    XLockDisplay(gGui->display);
    
    if(!(new_img = Imlib_load_image(gGui->imlib_data,
				    xitk_skin_get_skin_filename(gGui->skin_config, "CtlBG")))) {
      xine_error(_("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
      exit(-1);
    }
    XUnlockDisplay(gGui->display);

    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PPosition | PSize;

    XLockDisplay(gGui->display);
    XSetWMNormalHints(gGui->display, control->window, &hint);
    
    XResizeWindow (gGui->display, control->window,
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);
    
    XUnlockDisplay(gGui->display);
    
    while(!xitk_is_window_size(gGui->display, control->window, 
			       new_img->rgb_width, new_img->rgb_height)) {
      xine_usec_sleep(10000);
    }
    
    old_img = control->bg_image;
    control->bg_image = new_img;
    
    XLockDisplay(gGui->display);
    if(!gGui->use_root_window && gGui->video_display == gGui->display)
      XSetTransientForHint(gGui->display, control->window, gGui->video_window);
    
    Imlib_destroy_image(gGui->imlib_data, old_img);
    Imlib_apply_image(gGui->imlib_data, new_img, control->window);
    
    XUnlockDisplay(gGui->display);

    if(control_is_visible())
      control_raise_window();

    xitk_skin_unlock(gGui->skin_config);
    
    xitk_change_skins_widget_list(control->widget_list, gGui->skin_config);
    xitk_paint_widget_list(control->widget_list);

    active_sliders_video_settings();
  }
}

/*
 *
 */
static void control_select_new_skin(xitk_widget_t *w, void *data, int selected) {
  xitk_browser_release_all_buttons(control->skinlist);
  select_new_skin(selected);
}

void control_deinit(void) {
  if(control) {
    if(control_is_visible())
      control_toggle_visibility(NULL, NULL);
    control_exit(NULL, NULL);
  }
}

void control_reparent(void) {
  if(control)
    reparent_window(control->window);
}

/*
 * Create control panel window
 */
void control_panel(void) {
  GC                         gc;
  XSizeHints                 hint;
  XWMHints                  *wm_hint;
  XSetWindowAttributes       attr;
  char                      *title;
  Atom                       prop;
  MWMHints                   mwmhints;
  XClassHint                *xclasshint;
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_slider_widget_t       sl;
  xitk_combo_widget_t        cmb;
  xitk_widget_t             *w;

  xine_strdupa(title, _("xine Control Window"));

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
    xine_error(_("control: couldn't find image for background\n"));
    exit(-1);
  }

  XUnlockDisplay(gGui->display);

  hint.x = xine_config_register_num (gGui->xine, "gui.control_x", 
				     200,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);
  hint.y = xine_config_register_num (gGui->xine, "gui.control_y", 
				     100,
				     CONFIG_NO_DESC,
				     CONFIG_NO_HELP,
				     CONFIG_LEVEL_DEB,
				     CONFIG_NO_CB,
				     CONFIG_NO_DATA);

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

  XLockDisplay(gGui->display);
  attr.colormap		 = Imlib_get_colormap(gGui->imlib_data);

  control->window = XCreateWindow (gGui->display,
				   gGui->imlib_data->x.root,
				   hint.x, hint.y, hint.width, hint.height, 0, 
				   gGui->imlib_data->x.depth, InputOutput, 
				   gGui->imlib_data->x.visual,
				   CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
				   &attr);
  
  XmbSetWMProperties(gGui->display, control->window, title, title, NULL, 0, 
                     &hint, NULL, NULL);
  
  XSelectInput(gGui->display, control->window, INPUT_MOTION | KeymapStateMask);
  XUnlockDisplay(gGui->display);

  if(!video_window_is_visible())
    xitk_set_wm_window_type(control->window, WINDOW_TYPE_NORMAL);
  else
    xitk_unset_wm_window_type(control->window, WINDOW_TYPE_NORMAL);
  
  if(is_layer_above())
    xitk_set_layer_above(control->window);

  /*
   * wm, no border please
   */

  XLockDisplay(gGui->display);
  prop = XInternAtom(gGui->display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;

  XChangeProperty(gGui->display, control->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  /* set xclass */

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = title;
    xclasshint->res_class = "xine";
    XSetClassHint(gGui->display, control->window, xclasshint);
    XFree(xclasshint);
    
  }

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap   = gGui->icon;
    wm_hint->flags         = InputHint | StateHint | IconPixmapHint;
    XSetWMHints(gGui->display, control->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(gGui->display, control->window, 0, 0);
  
  Imlib_apply_image(gGui->imlib_data, control->bg_image, control->window);

  XUnlockDisplay (gGui->display);

  /*
   * Widget-list
   */
  control->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(control->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(control->widget_list, WIDGET_LIST_WINDOW, (void *) control->window);
  xitk_widget_list_set(control->widget_list, WIDGET_LIST_GC, gc);
  
  { /* All of sliders are disabled by default*/
    int cur;

    lbl.window = XITK_WIDGET_LIST_WINDOW(control->widget_list);
    lbl.gc     = XITK_WIDGET_LIST_GC(control->widget_list);

    /* HUE */
    cur = get_current_param(XINE_PARAM_VO_HUE);
    
    sl.skin_element_name = "SliderCtlHue";
    sl.min               = CONTROL_MIN;
    sl.max               = CONTROL_MAX;
    sl.step              = CONTROL_STEP;
    sl.callback          = set_hue;
    sl.userdata          = NULL;
    sl.motion_callback   = set_hue;
    sl.motion_userdata   = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
	     (control->hue = xitk_slider_create(control->widget_list, gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->hue, cur);
    xitk_set_widget_tips(control->hue, _("Control HUE value"));
    
    lbl.skin_element_name = "CtlHueLbl";
    lbl.label             = _("Hue");
    lbl.callback          = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
			     xitk_label_create(control->widget_list, gGui->skin_config, &lbl));
    xitk_disable_widget(control->hue);
    
    /* SATURATION */
    cur = get_current_param(XINE_PARAM_VO_SATURATION);

    sl.skin_element_name = "SliderCtlSat";
    sl.min               = CONTROL_MIN;
    sl.max               = CONTROL_MAX;
    sl.step              = CONTROL_STEP;
    sl.callback          = set_saturation;
    sl.userdata          = NULL;
    sl.motion_callback   = set_saturation;
    sl.motion_userdata   = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
	      (control->sat = xitk_slider_create(control->widget_list, gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->sat, cur);
    xitk_set_widget_tips(control->sat, _("Control SATURATION value"));

    lbl.skin_element_name = "CtlSatLbl";
    lbl.label             = _("Sat");
    lbl.callback          = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
			     xitk_label_create(control->widget_list, gGui->skin_config, &lbl));
    xitk_disable_widget(control->sat);

    /* BRIGHTNESS */
    cur = get_current_param(XINE_PARAM_VO_BRIGHTNESS);

    sl.skin_element_name = "SliderCtlBright";
    sl.min               = CONTROL_MIN;
    sl.max               = CONTROL_MAX;
    sl.step              = CONTROL_STEP;
    sl.callback          = set_brightness;
    sl.userdata          = NULL;
    sl.motion_callback   = set_brightness;
    sl.motion_userdata   = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
	    (control->bright = xitk_slider_create(control->widget_list, gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->bright, cur);
    xitk_set_widget_tips(control->bright, _("Control BRIGHTNESS value"));

    lbl.skin_element_name = "CtlBrightLbl";
    lbl.label             = _("Brt");
    lbl.callback          = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
			     xitk_label_create(control->widget_list, gGui->skin_config, &lbl));
    xitk_disable_widget(control->bright);
      
    /* CONTRAST */
    cur = get_current_param(XINE_PARAM_VO_CONTRAST);

    sl.skin_element_name = "SliderCtlCont";
    sl.min               = CONTROL_MIN;
    sl.max               = CONTROL_MAX;
    sl.step              = CONTROL_STEP;
    sl.callback          = set_contrast;
    sl.userdata          = NULL;
    sl.motion_callback   = set_contrast;
    sl.motion_userdata   = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
	      (control->contr = xitk_slider_create(control->widget_list, gGui->skin_config, &sl)));
    xitk_slider_set_pos(control->contr, cur);
    xitk_set_widget_tips(control->contr, _("Control CONTRAST value"));

    lbl.skin_element_name = "CtlContLbl";
    lbl.label             = _("Ctr");
    lbl.callback          = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
			    xitk_label_create(control->widget_list, gGui->skin_config, &lbl));
    xitk_disable_widget(control->contr);

    probe_active_controls();
    active_sliders_video_settings();
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
  lbl.callback          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(control->widget_list)),
			   xitk_label_create(control->widget_list, gGui->skin_config, &lbl));

  br.arrow_up.skin_element_name    = "CtlSkUp";
  br.slider.skin_element_name      = "SliderCtlSk";
  br.arrow_dn.skin_element_name    = "CtlSkDn";
  br.arrow_left.skin_element_name  = "CtlSkLeft";
  br.slider_h.skin_element_name    = "SliderHCtlSk";
  br.arrow_right.skin_element_name = "CtlSkRight";
  br.browser.skin_element_name     = "CtlSkItemBtn";
  br.browser.num_entries           = control->skins_num;
  br.browser.entries               = control->skins;
  br.callback                      = control_select_new_skin;
  br.dbl_click_callback            = NULL;
  br.parent_wlist                  = control->widget_list;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(control->widget_list)), 
			   (control->skinlist = 
			    xitk_browser_create(control->widget_list, gGui->skin_config, &br)));

  xitk_browser_update_list(control->skinlist, 
			   control->skins, NULL, control->skins_num, 0);


  lb.skin_element_name = "CtlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Dismiss");
  lb.callback          = control_exit;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(control->widget_list)), 
		    (w = xitk_labelbutton_create (control->widget_list, gGui->skin_config, &lb)));
  xitk_set_widget_tips(w, _("Close control window"));

  control_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());

  control->widget_key = 
    xitk_register_event_handler("control", 
				control->window, 
				control_handle_event, 
				NULL,
				gui_dndcallback,
				control->widget_list, NULL);
  
  control->visible = 1;
  control->running = 1;
  control_raise_window();
  
  try_to_set_input_focus(control->window);
}
