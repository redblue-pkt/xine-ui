/*
 * Copyright (C) 2021 the xine project
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
#include <pthread.h>

#include "common.h"
#include "actions.h"
#include "event.h"
#include "acontrol.h"
#include "skins.h"
#include "menus.h"
#include "videowin.h"
#include "panel.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/slider.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/skin.h"
#include <xine/audio_out.h>

#define NUM_EQ 10

static const struct {
  const char hint[8], skin[16];
} _actrl_names[NUM_EQ] = {
  { "30Hz",    "SliderACtl30" },
  { "60Hz",    "SliderACtl60" },
  { "125Hz",   "SliderACtl125" },
  { "250Hz",   "SliderACtl250" },
  { "500Hz",   "SliderACtl500" },
  { "1kHz",    "SliderACtl1k" },
  { "2kHz",    "SliderACtl2k" },
  { "4kHz",    "SliderACtl4k" },
  { "8kHz",    "SliderACtl8k" },
  { "16kHz",   "SliderACtl16k" }
};

struct xui_actrl_st {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *slid[NUM_EQ], *amp, *flat, *dismiss;

  uint8_t               refs[NUM_EQ];
  int                   val[NUM_EQ], v_amp;

  int                   status;
  xitk_register_key_t   widget_key;
};

static void _actrl_flat (xitk_widget_t *w, void *data, int state) {
  xui_actrl_t *actrl = data;

  (void)w;
  (void)state;
  acontrol_reset (actrl);
}

static void _actrl_set_value (xitk_widget_t *w, void *data, int value) {
  uint8_t *ref = data;
  int i = *ref;
  xui_actrl_t *actrl;

  (void)w;
  xitk_container (actrl, ref, refs[i]);
  actrl->val[i] = value;
  xine_set_param (actrl->gui->stream, XINE_PARAM_EQ_30HZ + i, value);
}

static void _actrl_set_amp (xitk_widget_t *w, void *data, int value) {
  xui_actrl_t *actrl = data;

  (void)w;
  actrl->v_amp = value;
  gui_set_amp_level (actrl->gui, value);
}

void acontrol_update_mixer_display (xui_actrl_t *actrl) {
  if (actrl->v_amp != actrl->gui->mixer.amp_level) {
    actrl->v_amp = actrl->gui->mixer.amp_level;
    if (actrl->amp)
      xitk_slider_set_pos (actrl->amp, actrl->v_amp);
  }
}

xui_actrl_t *acontrol_init (gGui_t *gui) {
  xui_actrl_t *actrl;
  int i;

  /* the minimal part, for +/- keyboard events. */

  if (!gui)
    return NULL;
  actrl = calloc (1, sizeof (*actrl));
  if (!actrl)
    return NULL;

  actrl->gui = gui;
  for (i = 0; i < NUM_EQ; i++) {
    int val = xine_get_param (actrl->gui->stream, XINE_PARAM_EQ_30HZ + i);

    if (val < -50)
      val = -50;
    if (val > 50)
      val = 50;
    actrl->val[i] = val;
  }

  /* defer all window stuff to first use. */
  actrl->status = 1;

  actrl->gui->actrl = actrl;
  return actrl;
}

/*
 * Handle X events here.
 */

static int _actrl_event (void *data, const xitk_be_event_t *e) {
  xui_actrl_t *actrl = data;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      acontrol_toggle_window (NULL, actrl);
      return 1;
    case XITK_EV_KEY_DOWN:
      if (e->utf8[0] == XITK_CTRL_KEY_PREFIX) {
        if (e->utf8[1] == XITK_KEY_ESCAPE) {
          acontrol_toggle_window (NULL, actrl);
          return 1;
        }
      }
      break;
    default: ;
  }
  return gui_handle_be_event (actrl->gui, e);
}

static void _actrl_toggle_window (xitk_widget_t *w, void *data, int state) {
  (void)state;
  acontrol_toggle_window (w, data);
}

/*
 * Create control panel window
 */
static int _actrl_open_window (xui_actrl_t *actrl) {
  char                      *title = _("xine audio control Window");
  xitk_labelbutton_widget_t  lb;
  xitk_image_t              *bg_image;
  int x, y, width, height;

  XITK_WIDGET_INIT (&lb);

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (actrl->gui->skin_config, "ACtlBG");
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image)
    return 0; /* TODO: noskin fallback ?? */

  x = 200;
  y = 100;
  gui_load_window_pos (actrl->gui, "acontrol", &x, &y);
  width = xitk_image_width (bg_image);
  height = xitk_image_height (bg_image);

  actrl->xwin = xitk_window_create_window_ext (actrl->gui->xitk, x, y, width, height,
    title, NULL, "xine", 0, is_layer_above (actrl->gui), actrl->gui->icon, bg_image);
  set_window_type_start (actrl->gui, actrl->xwin);

  /*
   * Widget-list
   */
  actrl->widget_list = xitk_window_widget_list (actrl->xwin);

  {
    xitk_slider_widget_t sl;
    unsigned int u;
    int val;

    XITK_WIDGET_INIT (&sl);
    sl.min    = -50;
    sl.max    = 50;
    sl.step   = 1;
    sl.callback = _actrl_set_value;
    sl.motion_callback = _actrl_set_value;

    for (u = 0; u < NUM_EQ; u++) {
      val = xine_get_param (actrl->gui->stream, XINE_PARAM_EQ_30HZ + u);

      if (val < -50)
        val = -50;
      if (val > 50)
        val = 50;
      actrl->val[u] = val;

      actrl->refs[u] = u;
      sl.skin_element_name = _actrl_names[u].skin;
      sl.userdata        =
      sl.motion_userdata = actrl->refs + u;
      actrl->slid[u] = xitk_slider_create (actrl->widget_list, actrl->gui->skin_config, &sl);
      if (actrl->slid[u]) {
        xitk_add_widget (actrl->widget_list, actrl->slid[u], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
        xitk_slider_set_pos (actrl->slid[u], actrl->val[u]);
        xitk_set_widget_tips (actrl->slid[u], _actrl_names[u].hint);
      }
    }

    val = xine_get_param (actrl->gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL);
    if (val < 0)
      val = 0;
    if (val > 200)
      val = 200;
    actrl->v_amp = val;
    sl.min    = 0;
    sl.max    = 200;
    sl.callback = _actrl_set_amp;
    sl.motion_callback = _actrl_set_amp;
    sl.userdata        =
    sl.motion_userdata = actrl;
    sl.skin_element_name = "SliderACtlAmp";
    actrl->amp = xitk_slider_create (actrl->widget_list, actrl->gui->skin_config, &sl);
    if (actrl->amp) {
      xitk_add_widget (actrl->widget_list, actrl->amp, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
      xitk_slider_set_pos (actrl->amp, actrl->v_amp);
      xitk_set_widget_tips (actrl->amp, _("Audio amplification"));
    }
  }

  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_DEFAULT;
  lb.state_callback    = NULL;
  lb.userdata          = actrl;

  lb.skin_element_name = "ACtlFlat";
  lb.label             = "----";
  lb.callback          = _actrl_flat;
  actrl->flat = xitk_labelbutton_create (actrl->widget_list, actrl->gui->skin_config, &lb);
  xitk_add_widget (actrl->widget_list, actrl->flat, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_set_widget_tips (actrl->flat, _("Neutral sound"));

  lb.skin_element_name = "ACtlDismiss";
  lb.label             = _("Dismiss");
  lb.callback          = _actrl_toggle_window;
  actrl->dismiss = xitk_labelbutton_create (actrl->widget_list, actrl->gui->skin_config, &lb);
  xitk_add_widget (actrl->widget_list, actrl->dismiss, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_set_widget_tips (actrl->dismiss, _("Close control window"));

  actrl->widget_key = xitk_be_register_event_handler ("acontrol", actrl->xwin, _actrl_event, actrl, NULL, NULL);

  actrl->status = 3;
  acontrol_raise_window (actrl);

  xitk_window_set_input_focus (actrl->xwin);
  return 1;
}

static void _actrl_close_window (xui_actrl_t *actrl) {
  actrl->status = 1;

  if (actrl->xwin) {
    int i;

    gui_save_window_pos (actrl->gui, "acontrol", actrl->widget_key);

    xitk_unregister_event_handler (actrl->gui->xitk, &actrl->widget_key);

    xitk_window_destroy_window (actrl->xwin);
    actrl->xwin = NULL;

    /* xitk_dlist_init (&control->widget_list->list); */

    for (i = 0; i < NUM_EQ; i++)
      actrl->slid[i] = NULL;
    actrl->amp = NULL;
    actrl->flat = NULL;
    actrl->dismiss = NULL;
  }

  video_window_set_input_focus (actrl->gui->vwin);
}

void acontrol_reset (xui_actrl_t *actrl) {
  if (actrl) {
    unsigned int u;

    for (u = 0; u < NUM_EQ; u++) {
      actrl->val[u] = 0;
      xine_set_param (actrl->gui->stream, XINE_PARAM_EQ_30HZ + u, 0);
      xitk_slider_set_pos (actrl->slid[u], 0);
    }
    gui_set_amp_level (actrl->gui, 100);
  }
}

/*
 * Raise control->window
 */
void acontrol_raise_window (xui_actrl_t *actrl) {
  if (actrl && (actrl->status >= 2)) {
    int visible = actrl->status - 2;

    raise_window (actrl->gui, actrl->xwin, visible, 1);
  }
}

/*
 * Hide/show the control panel
 */
void acontrol_toggle_visibility (xui_actrl_t *actrl) {
  if (actrl && (actrl->status >= 2)) {
    int visible;

    visible = actrl->status - 2;
    toggle_window (actrl->gui, actrl->xwin, actrl->widget_list, &visible, 1);
    actrl->status = visible + 2;
  }
}

void acontrol_toggle_window (xitk_widget_t *w, void *data) {
  xui_actrl_t *actrl = data;

  if (actrl) {
    int new_status;

    if (w == XUI_W_ON) {
      new_status = 3;
    } else if (w == XUI_W_OFF) {
      new_status = 1;
    } else {
      new_status = actrl->status < 2 ? 3 : 1;
    }
    if (new_status == actrl->status)
      return;
    if (new_status == 3) {
      if (actrl->status < 2) {
        _actrl_open_window (actrl);
      } else {
        int visible = 0;
        toggle_window (actrl->gui, actrl->xwin, actrl->widget_list, &visible, 1);
        actrl->status = visible + 2;
      }
    } else {
      _actrl_close_window (actrl);
    }
  }
}

/*
 * Change the current skin.
 */
void acontrol_change_skins (xui_actrl_t *actrl, int synthetic) {
  (void)synthetic;
  if (actrl->status >= 2)
    _actrl_close_window (actrl);
}

/*
 * Leaving control panel, release memory.
 */
void acontrol_deinit (xui_actrl_t *actrl) {
  if (actrl) {
    _actrl_close_window (actrl);
    actrl->gui->actrl = NULL;
    free (actrl);
  }
}

