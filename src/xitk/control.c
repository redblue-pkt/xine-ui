/* 
 * Copyright (C) 2000-2019 the xine project
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
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

#include "common.h"
#include <xine/video_out.h>

#define CONTROL_MIN     0
#define CONTROL_MAX     65535
#define CONTROL_STEP    565	/* approx. 1 pixel slider step */
#define STEP_SIZE       256     /* action event step */

#define TEST_VO_VALUE(val)  (val < CONTROL_MAX/3 || val > CONTROL_MAX*2/3) ? (CONTROL_MAX - val) : 0

typedef struct {
  xui_vctrl_t   *vctrl;
  char          *name;
  char          *hint;
  const char    *label;
  const char    *cfg;
  const char    *desc;
  const char    *help;
  xitk_widget_t *w;
  int            prop;
  int            enable;
  int            value;
  int            default_value;
} vctrl_item_t;

struct xui_vctrl_st {
  gGui_t               *gui;

#define NUM_SLIDERS 7
  vctrl_item_t          items[NUM_SLIDERS];

  xitk_window_t        *xwin;

  ImlibImage           *bg_image;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *skinlist;
  const char           *skins[64];
  int                   skins_num;

  int                   status;
  xitk_register_key_t   widget_key;

  xitk_widget_t         *combo;
};

static void control_select_new_skin (xitk_widget_t *w, void *data, int selected) {
  xui_vctrl_t *vctrl = data;

  (void)w;
  if (vctrl) {
    xitk_browser_release_all_buttons (vctrl->skinlist);
    select_new_skin (selected);
  }
}

static void control_set_value (xitk_widget_t *w, void *data, int value) {
  vctrl_item_t *item = data;
  int new_value;

  (void)w;
  xine_set_param (item->vctrl->gui->stream, item->prop, value);
  new_value = xine_get_param (item->vctrl->gui->stream, item->prop);
  if ((new_value != value) && item->w && xitk_is_widget_enabled (item->w))
    xitk_slider_set_pos (item->w, new_value);
  if (item->enable)
    config_update_num (item->cfg, new_value);
}

static void control_changes_cb (void *data, xine_cfg_entry_t *cfg) {
  vctrl_item_t *item = data;

  item->value = (cfg->num_value < 0) ? item->default_value : cfg->num_value;
}

xui_vctrl_t *control_init (gGui_t *gui) {
  xui_vctrl_t *vctrl;

  /* the minimal part, for +/- keyboard events. */

  if (!gui)
    return NULL;
  vctrl = calloc (1, sizeof (*vctrl));
  if (!vctrl)
    return NULL;

  vctrl->gui = gui;

  {
    unsigned int u;
    for (u = 0; u < NUM_SLIDERS; u++)
      vctrl->items[u].vctrl = vctrl;
  }

  vctrl->items[0].prop = XINE_PARAM_VO_HUE;
  vctrl->items[1].prop = XINE_PARAM_VO_SATURATION;
  vctrl->items[2].prop = XINE_PARAM_VO_BRIGHTNESS;
  vctrl->items[3].prop = XINE_PARAM_VO_CONTRAST;
#ifdef XINE_PARAM_VO_GAMMA
  vctrl->items[4].prop = XINE_PARAM_VO_GAMMA;
#endif
#ifdef XINE_PARAM_VO_SHARPNESS
  vctrl->items[5].prop = XINE_PARAM_VO_SHARPNESS;
#endif
#ifdef XINE_PARAM_VO_NOISE_REDUCTION
  vctrl->items[6].prop = XINE_PARAM_VO_NOISE_REDUCTION;
#endif

  vctrl->items[0].name  = _("Hue");
  vctrl->items[1].name  = _("Saturation");
  vctrl->items[2].name  = _("Brightness");
  vctrl->items[3].name  = _("Contrast");
  vctrl->items[4].name  = _("Gamma");
  vctrl->items[5].name  = _("Sharpness");
  vctrl->items[6].name  = _("Noise");

  vctrl->items[0].hint = _("Control HUE value");
  vctrl->items[1].hint = _("Control SATURATION value");
  vctrl->items[2].hint = _("Control BRIGHTNESS value");
  vctrl->items[3].hint = _("Control CONTRAST value");
  vctrl->items[4].hint = _("Control GAMMA value");
  vctrl->items[5].hint = _("Control SHARPNESS value");
  vctrl->items[6].hint = _("Control NOISE REDUCTION value");

  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[0].label = pgettext ("skin", "Hue");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[1].label = pgettext ("skin", "Sat");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[2].label = pgettext ("skin", "Brt");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[3].label = pgettext ("skin", "Ctr");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[4].label = pgettext ("skin", "Gam");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[5].label = pgettext ("skin", "Sha");
  /* TRANSLATORS: only ASCII characters (skin). */
  vctrl->items[6].label = pgettext ("skin", "Noi");

  vctrl->items[0].cfg   = "gui.vo_hue";
  vctrl->items[1].cfg   = "gui.vo_saturation";
  vctrl->items[2].cfg   = "gui.vo_brightness";
  vctrl->items[3].cfg   = "gui.vo_contrast";
  vctrl->items[4].cfg   = "gui.vo_gamma";
  vctrl->items[5].cfg   = "gui.vo_sharpness";
  vctrl->items[6].cfg   = "gui.vo_noise_reduction";

  vctrl->items[0].desc  = _("hue value");
  vctrl->items[1].desc  = _("saturation value");
  vctrl->items[2].desc  = _("brightness value");
  vctrl->items[3].desc  = _("contrast value");
  vctrl->items[4].desc  = _("gamma value");
  vctrl->items[5].desc  = _("sharpness value");
  vctrl->items[6].desc  = _("noise reduction value");

  vctrl->items[0].help  = _("Hue value.");
  vctrl->items[1].help  = _("Saturation value.");
  vctrl->items[2].help  = _("Brightness value.");
  vctrl->items[3].help  = _("Contrast value.");
  vctrl->items[4].help  = _("Gamma value.");
  vctrl->items[5].help  = _("Sharpness value.");
  vctrl->items[6].help  = _("Noise reduction value.");

#ifdef VO_CAP_HUE
  /* xine-lib-1.2 */
  {
    uint32_t cap = vctrl->gui->vo_port->get_capabilities (vctrl->gui->vo_port);

    vctrl->items[0].enable = !!(cap & VO_CAP_HUE);
    vctrl->items[1].enable = !!(cap & VO_CAP_SATURATION);
    vctrl->items[2].enable = !!(cap & VO_CAP_BRIGHTNESS);
    vctrl->items[3].enable = !!(cap & VO_CAP_CONTRAST);
#ifdef XINE_PARAM_VO_GAMMA
    vctrl->items[4].enable = !!(cap & VO_CAP_GAMMA);
#endif
#ifdef XINE_PARAM_VO_SHARPNESS
    vctrl->items[5].enable = !!(cap & VO_CAP_SHARPNESS);
#endif
#ifdef XINE_PARAM_VO_NOISE_REDUCTION
    vctrl->items[6].enable = !!(cap & VO_CAP_NOISE_REDUCTION);
#endif
  }
#else
  /* xine-lib-1.1 */
  {
    unsigned int u;
    for (u = 0; u < NUM_SLIDERS; u++) {
      vctrl_item_t *item = &vctrl->items[u];
      int old_value = xine_get_param (vctrl->gui->stream, item->prop);

      xine_set_param (vctrl->gui->stream, item->prop, TEST_VO_VALUE (old_value));
      if (xine_get_param (vctrl->gui->stream, item->prop) != old_value) {
        xine_set_param (vctrl->gui->stream, item->prop, old_value);
        item->enable = 1;
      }
    }
  }
#endif

  {
    unsigned int u;
    for (u = 0; u < NUM_SLIDERS; u++) {
      vctrl_item_t *item = &vctrl->items[u];

      if (item->enable) {
        item->value = xine_config_register_range (vctrl->gui->xine,
          item->cfg, -1, CONTROL_MIN, CONTROL_MAX,
          item->desc, item->help, CONFIG_LEVEL_DEB,
          control_changes_cb, item);
        item->default_value = xine_get_param (vctrl->gui->stream, item->prop);
        if (item->value < 0) {
          item->value = item->default_value;
        } else {
          xine_set_param (vctrl->gui->stream, item->prop, item->value);
          item->value = xine_get_param (vctrl->gui->stream, item->prop);
        }
      }
    }
  }

  /* defer all window stuff to first use. */
  vctrl->status = 1;

  vctrl->gui->vctrl = vctrl;
  return vctrl;
}

/*
 * Handle X events here.
 */
static void control_handle_event(XEvent *event, void *data) {
  xui_vctrl_t *vctrl = data;
  
  switch(event->type) {
  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    
    if(bevent->button == Button3) {
      int wx, wy;
      
      xitk_window_get_window_position (vctrl->xwin, &wx, &wy, NULL, NULL);
      control_menu (vctrl->gui, vctrl->widget_list, bevent->x + wx, bevent->y + wy);
    }
  }

  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      control_toggle_window (NULL, vctrl);
    else
      gui_handle_event (event, vctrl->gui);
    break;
    
  }
  
}

/*
 * Create control panel window
 */
static int vctrl_open_window (xui_vctrl_t *vctrl) {
  GC                         gc;
  char                      *title = _("xine Control Window");
  xitk_browser_widget_t      br;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        lbl;
  xitk_combo_widget_t        cmb;
  xitk_widget_t             *w;
  int x, y;

  XITK_WIDGET_INIT (&br, vctrl->gui->imlib_data);
  XITK_WIDGET_INIT (&lb, vctrl->gui->imlib_data);
  XITK_WIDGET_INIT (&cmb, vctrl->gui->imlib_data);

  vctrl->gui->x_lock_display (vctrl->gui->display);
  vctrl->bg_image = Imlib_load_image (vctrl->gui->imlib_data,
    xitk_skin_get_skin_filename (vctrl->gui->skin_config, "CtlBG"));
  vctrl->gui->x_unlock_display (vctrl->gui->display);
  if (!vctrl->bg_image) {
    xine_error (vctrl->gui, _("control: couldn't find image for background\n"));
    exit(-1);
  }

  x = xine_config_register_num (vctrl->gui->xine, "gui.control_x",
    200, CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
  y = xine_config_register_num (vctrl->gui->xine, "gui.control_y",
    100, CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);

  vctrl->xwin = xitk_window_create_simple_window(vctrl->gui->imlib_data, x + 100, y + 100,
                                                 vctrl->bg_image->rgb_width, vctrl->bg_image->rgb_height);
  xitk_window_set_window_title(vctrl->xwin, _(title));

  set_window_states_start(vctrl->xwin);

  if (is_layer_above ())
    xitk_window_set_layer_above (vctrl->xwin);

  vctrl->gui->x_lock_display (vctrl->gui->display);
  gc = XCreateGC (vctrl->gui->display, xitk_window_get_window(vctrl->xwin), 0, 0);
  Imlib_apply_image (vctrl->gui->imlib_data, vctrl->bg_image, xitk_window_get_window(vctrl->xwin));
  vctrl->gui->x_unlock_display (vctrl->gui->display);

  /*
   * Widget-list
   */
  vctrl->widget_list = xitk_widget_list_new ();
  xitk_widget_list_set (vctrl->widget_list, WIDGET_LIST_WINDOW, (void *)xitk_window_get_window(vctrl->xwin));
  xitk_widget_list_set (vctrl->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT (&lbl, vctrl->gui->imlib_data);
  lbl.callback = NULL;

  {
    static const char * const sl_skins[] = {
      "SliderCtlHue", "SliderCtlSat", "SliderCtlBright", "SliderCtlCont",
      "SliderCtlGamma", "SliderCtlSharp", "SliderCtlNoise"
    };
    static const char * const lbl_skins[] = {
      "CtlHueLbl", "CtlSatLbl", "CtlBrightLbl", "CtlContLbl",
      "CtlGammaLbl", "CtlSharpLbl", "CtlNoiseLbl"
    };
    xitk_slider_widget_t sl;
    unsigned int u;

    XITK_WIDGET_INIT (&sl, vctrl->gui->imlib_data);
    sl.min    = CONTROL_MIN;
    sl.max    = CONTROL_MAX;
    sl.step   = CONTROL_STEP;
    sl.callback = control_set_value;
    sl.motion_callback = control_set_value;

    for (u = 0; u < NUM_SLIDERS; u++) {
      vctrl_item_t *item = &vctrl->items[u];
      int v;

      if (item->enable)
        v = xine_get_param (vctrl->gui->stream, item->prop);
      else
        v = CONTROL_MIN;
      sl.skin_element_name = sl_skins[u];
      sl.userdata        =
      sl.motion_userdata = item;
      item->w = xitk_slider_create (vctrl->widget_list, vctrl->gui->skin_config, &sl);
      if (item->w) {
        xitk_add_widget (vctrl->widget_list, item->w);
        xitk_slider_set_pos (item->w, item->value);
        xitk_set_widget_tips (item->w, item->hint);
        if (item->enable)
          xitk_enable_widget (item->w);
        else
          xitk_disable_widget (item->w);
      }
      lbl.skin_element_name = lbl_skins[u];
      lbl.label             = item->label;
      xitk_add_widget (vctrl->widget_list, xitk_label_create (vctrl->widget_list, vctrl->gui->skin_config, &lbl));
    }
  }

  {
    skins_locations_t **sks = get_available_skins ();
    int i;

    vctrl->skins_num = get_available_skins_num ();
    for (i = 0; i < vctrl->skins_num; i++)
      vctrl->skins[i] = strdup (sks[i]->skin);
  }
  
  lbl.skin_element_name = "CtlSkLbl";
  /* TRANSLATORS: only ASCII characters (skin) */
  lbl.label             = pgettext("skin", "Choose a Skin");
  xitk_add_widget (vctrl->widget_list, xitk_label_create (vctrl->widget_list, vctrl->gui->skin_config, &lbl));

  br.arrow_up.skin_element_name    = "CtlSkUp";
  br.slider.skin_element_name      = "SliderCtlSk";
  br.arrow_dn.skin_element_name    = "CtlSkDn";
  br.arrow_left.skin_element_name  = "CtlSkLeft";
  br.slider_h.skin_element_name    = "SliderHCtlSk";
  br.arrow_right.skin_element_name = "CtlSkRight";
  br.browser.skin_element_name     = "CtlSkItemBtn";
  br.browser.num_entries           = vctrl->skins_num;
  br.browser.entries               = vctrl->skins;
  br.callback                      =
  br.dbl_click_callback            = control_select_new_skin;
  br.userdata                      = vctrl;
  br.parent_wlist                  = vctrl->widget_list;
  vctrl->skinlist = xitk_browser_create (vctrl->widget_list, vctrl->gui->skin_config, &br);
  xitk_add_widget (vctrl->widget_list, vctrl->skinlist);

  xitk_browser_update_list (vctrl->skinlist, vctrl->skins, NULL, vctrl->skins_num, 0);

  lb.skin_element_name = "CtlDismiss";
  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_DEFAULT;
  lb.label             = _("Dismiss");
  lb.callback          = control_toggle_window;
  lb.state_callback    = NULL;
  lb.userdata          = vctrl;
  w = xitk_labelbutton_create (vctrl->widget_list, vctrl->gui->skin_config, &lb);
  xitk_add_widget (vctrl->widget_list, w);
  xitk_set_widget_tips(w, _("Close control window"));

  control_show_tips (vctrl, panel_get_tips_enable (vctrl->gui->panel), panel_get_tips_timeout (vctrl->gui->panel));

  vctrl->widget_key = xitk_register_event_handler ("control",
    xitk_window_get_window(vctrl->xwin), control_handle_event, NULL,
    gui_dndcallback, vctrl->widget_list, vctrl);
  
  vctrl->status = 3;
  control_raise_window (vctrl);

  xitk_window_try_to_set_input_focus(vctrl->xwin);
  return 1;
}

static void vctrl_close_window (xui_vctrl_t *vctrl) {
  vctrl->status = 1;

  if (vctrl->xwin) {
    window_info_t wi;
    int           i;
    
    if ((xitk_get_window_info (vctrl->widget_key, &wi))) {
      config_update_num ("gui.control_x", wi.x);
      config_update_num ("gui.control_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }

    xitk_unregister_event_handler (&vctrl->widget_key);

    xitk_destroy_widgets (vctrl->widget_list);

    xitk_window_destroy_window(vctrl->xwin);
    vctrl->xwin = NULL;

    vctrl->gui->x_lock_display (vctrl->gui->display);
    Imlib_destroy_image (vctrl->gui->imlib_data, vctrl->bg_image);
    vctrl->gui->x_unlock_display (vctrl->gui->display);

    /* xitk_dlist_init (&control->widget_list->list); */

    vctrl->gui->x_lock_display (vctrl->gui->display);
    XFreeGC (vctrl->gui->display, (XITK_WIDGET_LIST_GC (vctrl->widget_list)));
    vctrl->gui->x_unlock_display (vctrl->gui->display);

    XITK_WIDGET_LIST_FREE (vctrl->widget_list);

    for (i = 0; i < vctrl->skins_num; i++)
      free ((char *)vctrl->skins[i]);

    for (i = 0; i < NUM_SLIDERS; i++)
      vctrl->items[0].w = NULL;
  }

  video_window_set_input_focus (vctrl->gui->vwin);
}

void control_dec_image_prop (xui_vctrl_t *vctrl, int prop) {
  if (vctrl) {
    vctrl_item_t *item;
    unsigned int u;

    for (u = 0; u < NUM_SLIDERS; u++) {
      item = &vctrl->items[u];
      if (item->prop == prop)
        break;
    }
    if (u < NUM_SLIDERS) {
      int v = item->value - STEP_SIZE;
      if (v < 0)
        v = 0;
      if (item->enable) {
        xine_set_param (item->vctrl->gui->stream, item->prop, v);
        /* not doing this here because xine video out may support
         * just a few coarse steps, and snap us back. in other words:
         * we are stuck inside a black hole.
        v = xine_get_param (item->vctrl->gui->stream, item->prop);
        */
        item->value = v;
        osd_draw_bar (item->name, 0, 65535, v, OSD_BAR_STEPPER);
        if (item->w && xitk_is_widget_enabled (item->w))
          xitk_slider_set_pos (item->w, v);
      }
    }
  }
}

void control_inc_image_prop (xui_vctrl_t *vctrl, int prop) {
  if (vctrl) {
    vctrl_item_t *item;
    unsigned int u;

    for (u = 0; u < NUM_SLIDERS; u++) {
      item = &vctrl->items[u];
      if (item->prop == prop)
        break;
    }
    if (u < NUM_SLIDERS) {
      int v = item->value + STEP_SIZE;
      if (v > 65535)
        v = 65535;
      if (item->enable) {
        xine_set_param (item->vctrl->gui->stream, item->prop, v);
        item->value = v;
        osd_draw_bar (item->name, 0, 65535, v, OSD_BAR_STEPPER);
        if (item->w && xitk_is_widget_enabled (item->w))
          xitk_slider_set_pos (item->w, v);
      }
    }
  }
}

void control_reset (xui_vctrl_t *vctrl) {
  if (vctrl) {
    unsigned int u;

    for (u = 0; u < NUM_SLIDERS; u++) {
      vctrl_item_t *item = &vctrl->items[u];

      if (item->enable) {
        xine_set_param (vctrl->gui->stream, item->prop, item->default_value);
        config_update_num (item->cfg, -1);
        if (item->w && xitk_is_widget_enabled (item->w))
          xitk_slider_set_pos (item->w, item->default_value);
      }
    }
  }
}

/*
 *
 */
void control_show_tips (xui_vctrl_t *vctrl, int enabled, unsigned long timeout) {
  if (vctrl) {
    if (enabled)
      xitk_set_widgets_tips_timeout (vctrl->widget_list, timeout);
    else
      xitk_disable_widgets_tips (vctrl->widget_list);
  }
}

/*
void control_update_tips_timeout (xui_vctrl_t *vctrl, unsigned long timeout) {
  if (vctrl)
    xitk_set_widgets_tips_timeout (vctrl->widget_list, timeout);
}
*/

int control_status (xui_vctrl_t *vctrl) {
  if (!vctrl)
    return 0;
  if (vctrl->status == 1)
    return 1;
  if (!vctrl->gui->use_root_window && (vctrl->status == 2))
    return 2;
  return (!!xitk_window_is_window_visible (vctrl->xwin)) + 2;
}

/*
 * Raise control->window
 */
void control_raise_window (xui_vctrl_t *vctrl) {
  if (vctrl && (vctrl->status >= 2)) {
    int visible = vctrl->status - 2;

    raise_window (vctrl->xwin, visible, 1);
  }
}

/*
 * Hide/show the control panel
 */
void control_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_vctrl_t *vctrl = data;

  if (vctrl && (vctrl->status >= 2)) {
    int visible;

    if (w == XUI_W_ON) {
      if (vctrl->status == 3)
        return;
    } else if (w == XUI_W_OFF) {
      if (vctrl->status == 2)
        return;
    }
    visible = vctrl->status - 2;
    toggle_window (vctrl->xwin, vctrl->widget_list, &visible, 1);
    vctrl->status = visible + 2;
  }
}

void control_toggle_window (xitk_widget_t *w, void *data) {
  xui_vctrl_t *vctrl = data;

  if (vctrl) {
    int new_status;

    if (w == XUI_W_ON) {
      new_status = 3;
    } else if (w == XUI_W_OFF) {
      new_status = 1;
    } else {
      new_status = vctrl->status < 2 ? 3 : 1;
    }
    if (new_status == vctrl->status)
      return;
    if (new_status == 3) {
      if (vctrl->status < 2) {
        vctrl_open_window (vctrl);
      } else {
        int visible = 0;
        toggle_window (vctrl->xwin, vctrl->widget_list, &visible, 1);
        vctrl->status = visible + 2;
      }
    } else {
      vctrl_close_window (vctrl);
    }
  }
}

/*
 * Change the current skin.
 */
void control_change_skins (xui_vctrl_t *vctrl, int synthetic) {
  ImlibImage   *new_img, *old_img;
  XSizeHints    hint;

  if (vctrl->status >= 2) {

    xitk_skin_lock (vctrl->gui->skin_config);
    xitk_hide_widgets (vctrl->widget_list);

    vctrl->gui->x_lock_display (vctrl->gui->display);

    if (!(new_img = Imlib_load_image (vctrl->gui->imlib_data,
      xitk_skin_get_skin_filename (vctrl->gui->skin_config, "CtlBG")))) {
      xine_error (vctrl->gui, _("%s(): couldn't find image for background\n"), __XINE_FUNCTION__);
      exit(-1);
    }
    vctrl->gui->x_unlock_display (vctrl->gui->display);
    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PPosition | PSize;

    vctrl->gui->x_lock_display (vctrl->gui->display);
    XSetWMNormalHints (vctrl->gui->display, xitk_window_get_window(vctrl->xwin), &hint);
    XResizeWindow (vctrl->gui->display, xitk_window_get_window(vctrl->xwin),
      (unsigned int)new_img->rgb_width, (unsigned int)new_img->rgb_height);
    vctrl->gui->x_unlock_display (vctrl->gui->display);
    
    while (!xitk_is_window_size (vctrl->gui->display, xitk_window_get_window(vctrl->xwin),
      new_img->rgb_width, new_img->rgb_height)) {
      xine_usec_sleep(10000);
    }
    old_img = vctrl->bg_image;
    vctrl->bg_image = new_img;

    video_window_set_transient_for (vctrl->gui->vwin, vctrl->xwin);

    vctrl->gui->x_lock_display (vctrl->gui->display);
    Imlib_destroy_image (vctrl->gui->imlib_data, old_img);
    Imlib_apply_image (vctrl->gui->imlib_data, new_img, xitk_window_get_window(vctrl->xwin));
    vctrl->gui->x_unlock_display (vctrl->gui->display);

    control_raise_window (vctrl);

    xitk_skin_unlock (vctrl->gui->skin_config);
    
    xitk_change_skins_widget_list (vctrl->widget_list, vctrl->gui->skin_config);
    xitk_paint_widget_list (vctrl->widget_list);

    {
      unsigned int u;

      for (u = 0; u < NUM_SLIDERS; u++) {
        vctrl_item_t *item = &vctrl->items[u];

        if (item->enable)
          xitk_enable_widget (item->w);
        else
          xitk_disable_widget (item->w);
      }
    }
  }
}

/*
 * Leaving control panel, release memory.
 */
void control_deinit (xui_vctrl_t *vctrl) {
  if (vctrl) {
    vctrl_close_window (vctrl);
#ifdef HAVE_XINE_CONFIG_UNREGISTER_CALLBACKS
    xine_config_unregister_callbacks (vctrl->gui->xine, NULL, NULL, vctrl, sizeof (*vctrl));
#endif
    vctrl->gui->vctrl = NULL;
    free (vctrl);
  }
}

void control_reparent (xui_vctrl_t *vctrl) {
  if (vctrl && vctrl->xwin)
    reparent_window(vctrl->xwin);
}

