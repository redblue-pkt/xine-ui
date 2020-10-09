/*
 * Copyright (C) 2000-2020 the xine project
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

#include <X11/keysym.h>

#include "common.h"
#include "setup.h"
#include "panel.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/tabs.h"
#include "xine-toolkit/slider.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/combo.h"
#include "xine-toolkit/intbox.h"
#include "xine-toolkit/checkbox.h"

#include <xine/sorted_array.h>

#define WINDOW_WIDTH             630
#define WINDOW_HEIGHT            530

#define FRAME_WIDTH              541
#define FRAME_HEIGHT             44

#define MAX_DISPLAY_WIDGETS      8

#define NORMAL_CURS              0
#define WAIT_CURS                1

typedef struct {
  char *name;
  int   nlen;
  int   num_entries;
} _setup_section_t;

typedef struct {
  xitk_widget_t    *frame;
  xitk_widget_t    *label;
  xitk_widget_t    *widget;
  int               changed;
  xine_cfg_entry_t  cfg;
} _widget_triplet_t;

struct xui_setup_st {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   visible;

  xitk_widget_t        *tabs;
  xitk_widget_t        *ok;

#define SETUP_MAX_SECTIONS 20
  _setup_section_t      sections[SETUP_MAX_SECTIONS + 1];
  char                 *section_names[SETUP_MAX_SECTIONS];
  int                   num_sections;

  xitk_widget_t        *slider_wg;

  _widget_triplet_t    *wg;
  int                   num_wg;
  int                   max_wg;
  int                   first_displayed;

  xitk_register_key_t   kreg;
  xitk_register_key_t   dialog;

  int                   th; /* Tabs height */
  int                   fh; /* Font height */

  char                  namebuf[1024];
};

static void setup_change_section (xitk_widget_t *, void *, int);

/*
 * Leaving setup panel, release memory.
 */
static void setup_exit (xitk_widget_t *w, void *data, int state) {
  xui_setup_t *setup = data;

  (void)w;
  (void)state;
  if (!setup)
    return;

  setup->visible = 0;

  if (setup->dialog)
    xitk_unregister_event_handler (&setup->dialog);

  gui_save_window_pos (setup->gui, "setup", setup->kreg);

  xitk_unregister_event_handler (&setup->kreg);

  xitk_window_destroy_window (setup->xwin);
  setup->xwin = NULL;
  /* xitk_dlist_init (&setup->widget_list->list); */

  video_window_set_input_focus (setup->gui->vwin);

  setup->gui->setup = NULL;

  free (setup->wg);

  free (setup);
}

void setup_show_tips (xui_setup_t *setup, int enabled, unsigned long timeout) {
  if (!setup)
    return;
  if (enabled)
    xitk_set_widgets_tips_timeout (setup->widget_list, timeout);
  else
    xitk_disable_widgets_tips (setup->widget_list);
}

/*
void setup_update_tips_timeout (xui_setup_t *setup, unsigned long timeout) {
  if (!setup)
    return;
  xitk_set_widgets_tips_timeout (setup->widget_list, timeout);
}
*/

/*
 * Return 1 if setup panel is visible
 */
int setup_is_visible (xui_setup_t *setup) {
  if (!setup)
    return 0;
  if (setup->gui->use_root_window)
    return xitk_window_is_window_visible (setup->xwin);
  else
    return setup->visible && xitk_window_is_window_visible (setup->xwin);
}

/*
 * Raise setup->xwin
 */
void setup_raise_window (xui_setup_t *setup) {
  if (setup)
    raise_window (setup->gui, setup->xwin, setup->visible, 1);
}

/*
 * Hide/show the setup panel
 */
void setup_toggle_visibility (xui_setup_t *setup) {
  if (setup)
    toggle_window (setup->gui, setup->xwin, setup->widget_list, &setup->visible, 1);
}

static void setup_apply (xitk_widget_t *w, void *data, int state) {
  xui_setup_t *setup = data;
  int need_restart = 0;

  (void)w;
  (void)state;
  if (setup->num_wg > 0) {
    int i;

    for (i = 0; i < setup->num_wg; i++) {

      if (setup->wg[i].widget) {
        int type = xitk_get_widget_type (setup->wg[i].widget);

        if (setup->wg[i].changed ||
          ((type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) || (type & WIDGET_GROUP_INTBOX)) {
          xitk_widget_t *w = setup->wg[i].widget;
          int            numval = 0;
          const char    *strval = NULL;

	  if(!need_restart) {
            if (setup->wg[i].changed && (!setup->wg[i].cfg.callback_data && !setup->wg[i].cfg.callback))
	      need_restart = 1;
	  }

          setup->wg[i].changed = 0;

	  switch(type & WIDGET_TYPE_MASK) {
	  case WIDGET_TYPE_SLIDER:
	    numval = xitk_slider_get_pos(w);
	    break;

	  case WIDGET_TYPE_CHECKBOX:
	    numval = xitk_checkbox_get_state(w);
	    break;

	  case WIDGET_TYPE_INPUTTEXT:
	    strval = xitk_inputtext_get_text(w);
	    if(!strcmp(strval, setup->wg[i].cfg.str_value))
	      continue;
	    break;

          case WIDGET_TYPE_COMBO:
            numval = xitk_combo_get_current_selected (w);
            break;

          case WIDGET_TYPE_INTBOX:
            numval = xitk_intbox_get_value (w);
            break;
          default: ;
	  }

          switch (setup->wg[i].cfg.type) {
	  case XINE_CONFIG_TYPE_STRING:
            config_update_string ((char *)setup->wg[i].cfg.key, strval);
	    break;
	  case XINE_CONFIG_TYPE_ENUM:
	  case XINE_CONFIG_TYPE_NUM:
	  case XINE_CONFIG_TYPE_BOOL:
	  case XINE_CONFIG_TYPE_RANGE:
            config_update_num ((char *)setup->wg[i].cfg.key, numval);
	    break;
	  case XINE_CONFIG_TYPE_UNKNOWN:
	    break;
	  }
	}
      }
    }
    xine_config_save (setup->gui->xine, setup->gui->cfg_file);

    if(w != setup->ok) {
      int section = xitk_tabs_get_current_selected (setup->tabs);
      if (section >= 0) {
        setup_change_section (setup->tabs, setup, section);
      }
    }


    if(need_restart) {
      setup->dialog = xitk_window_dialog_3 (setup->gui->xitk,
        setup->xwin,
        get_layer_above_video (setup->gui), 400, _("Important Notice"), NULL, NULL,
        XITK_LABEL_OK, NULL, NULL, NULL, 0, ALIGN_CENTER,
        "%s", _("You changed some configuration value which require to restart xine to take effect."));
    }

  }
}

static void setup_set_cursor (xui_setup_t *setup, int state) {
  if (state == WAIT_CURS)
    xitk_window_define_window_cursor (setup->xwin, xitk_cursor_watch);
  else
    xitk_window_restore_window_cursor (setup->xwin);
}

static void setup_ok (xitk_widget_t *w, void *data, int state) {
  setup_apply (w, data, state);
  setup_exit (w, data, state);
}

/*
 *
 */
static void setup_clear_tab (xui_setup_t *setup) {
  xitk_image_t *im;
  int width = (WINDOW_WIDTH - 30);
  int height = (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30);

  im = xitk_image_create_image (setup->gui->xitk, width, height);

  xitk_image_draw_outter (im, width, height);
  xitk_image_draw_image (setup->widget_list, im, 0, 0, width, height, 15, (24 + setup->th), 0);
  xitk_image_free_image (&im);
}

static void setup_triplets_enable_and_show (xui_setup_t *setup, int start, int stop, int tips_timeout) {
  int i;

  for (i = start; i < stop; i++) {
    _widget_triplet_t *t = setup->wg + i;

    if (t->frame)
      xitk_enable_and_show_widget (t->frame);
    if (t->label) {
      xitk_enable_and_show_widget (t->label);
      if (tips_timeout)
        xitk_set_widget_tips_timeout (t->label, tips_timeout);
      else
        xitk_disable_widget_tips (t->label);
    }
    if (t->widget) {
      xitk_enable_and_show_widget (t->widget);
      if (tips_timeout)
        xitk_set_widget_tips_timeout (t->widget, tips_timeout);
      else
        xitk_disable_widget_tips (t->widget);
    }
  }
}

static void setup_triplets_disable_and_hide (xui_setup_t *setup, int start, int stop) {
  int i;

  for (i = start; i < stop; i++) {
    _widget_triplet_t *t = setup->wg + i;

    if (t->frame)
      xitk_disable_and_hide_widget (t->frame);
    if (t->label) {
      xitk_disable_and_hide_widget (t->label);
      xitk_disable_widget_tips (t->label);
    }
    if (t->widget) {
      xitk_disable_and_hide_widget (t->widget);
      xitk_disable_widget_tips (t->widget);
    }
  }
}

/*
 *
 */
static void setup_paint_widgets (xui_setup_t *setup, int first) {
  int i, last;
  int wx, wy, y = (24 + setup->th + 13);
  int tips_timeout = !panel_get_tips_enable (setup->gui->panel) ? 0
                   : panel_get_tips_timeout (setup->gui->panel);

  last = setup->num_wg - setup->first_displayed;
  if (last > MAX_DISPLAY_WIDGETS)
    last = MAX_DISPLAY_WIDGETS;
  last += setup->first_displayed;

  /* First, disable old widgets. */
  setup_triplets_disable_and_hide (setup, setup->first_displayed, last);

  last = setup->num_wg - MAX_DISPLAY_WIDGETS;
  if (first > last)
    first = last;
  if (first < 0)
    first = 0;
  setup->first_displayed = first;
  last = setup->num_wg - setup->first_displayed;
  if (last > MAX_DISPLAY_WIDGETS)
    last = MAX_DISPLAY_WIDGETS;
  last += setup->first_displayed;

  /* Move widgets to new position. */
  for (i = setup->first_displayed; i < last; i++) {
    _widget_triplet_t *t = setup->wg + i;

    if (t->frame) {
      xitk_get_widget_pos (t->frame, &wx, &wy);
      xitk_set_widget_pos (t->frame, wx, y);
    }
    y += FRAME_HEIGHT >> 1;
    if (t->label) {
      xitk_get_widget_pos (t->label, &wx, &wy);
      xitk_set_widget_pos (t->label, wx, y + 4 - xitk_get_widget_height (t->label) / 2);
    }
    if (t->widget) {
      xitk_get_widget_pos (t->widget, &wx, &wy);
      xitk_set_widget_pos (t->widget, wx, y + 4 - xitk_get_widget_height (t->widget) / 2);
    }
    y += (FRAME_HEIGHT >> 1) + 3;
  }

/* Repaint our new ones just 1 time. */

  if (setup->num_wg > MAX_DISPLAY_WIDGETS)
    xitk_enable_and_show_widget (setup->slider_wg);
  setup_triplets_enable_and_show (setup, setup->first_displayed, last, tips_timeout);
  setup_set_cursor (setup, NORMAL_CURS);
}

static void setup_list_step (xui_setup_t *setup, int step) {
  int max, newpos;

  max = setup->num_wg - MAX_DISPLAY_WIDGETS;
  if (max < 0)
    max = 0;
  newpos = setup->first_displayed + step;
  if (newpos > max)
    newpos = max;
  if (newpos < 0)
    newpos = 0;
  if (setup->first_displayed == newpos)
    return;
  xitk_slider_set_pos (setup->slider_wg, max - newpos);
  setup_paint_widgets (setup, newpos);
}

/*
 * Handle X events here.
 */

static void _setup_handle_button_event(void *data, const xitk_button_event_t *be) {
  xui_setup_t *setup = data;

  if (be->event == XITK_BUTTON_PRESS) {
    if (be->button == Button4)
      setup_list_step (setup, -1);
    else if (be->button == Button5)
      setup_list_step (setup, 1);
  }
}

static void _setup_handle_key_event(void *data, const xitk_key_event_t *ke) {
  xui_setup_t *setup = data;

  if (ke->event == XITK_KEY_PRESS) {
    switch (ke->key_pressed) {
      case XK_Up:
        if (xitk_is_widget_enabled (setup->slider_wg) &&
            (ke->modifiers & 0xFFFFFFEF) == MODIFIER_NOMOD) {
          setup_list_step (setup, -1);
          return;
        }
        break;

      case XK_Down:
        if (xitk_is_widget_enabled (setup->slider_wg) &&
            (ke->modifiers & 0xFFFFFFEF) == MODIFIER_NOMOD) {
          setup_list_step (setup, 1);
          return;
        }
        break;

      case XK_Next:
        if (xitk_is_widget_enabled(setup->slider_wg)) {
          setup_list_step (setup, MAX_DISPLAY_WIDGETS);
          return;
        }
        break;

      case XK_Prior:
        if (xitk_is_widget_enabled(setup->slider_wg)) {
          setup_list_step (setup, -MAX_DISPLAY_WIDGETS);
          return;
        }
        break;

      case XK_Escape:
        setup_exit (NULL, setup, 0);
        return;
    }

    gui_handle_key_event (setup->gui, ke);
  }
}

static const xitk_event_cbs_t setup_event_cbs = {
  .key_cb            = _setup_handle_key_event,
  .btn_cb            = _setup_handle_button_event,
};

/*
 *
 */
static void numtype_update(xitk_widget_t *w, void *data, int value) {
  _widget_triplet_t *triplet = (_widget_triplet_t *) data;

  (void)w;
  (void)value;
  triplet->changed = 1;
}

/*
 *
 */
static void stringtype_update(xitk_widget_t *w, void *data, const char *str) {
  _widget_triplet_t *triplet = (_widget_triplet_t *) data;

  (void)w;
  (void)str;
  triplet->changed = 1;
}

static void setup_add_nothing_available (xui_setup_t *setup, const char *title, int x, int y) {
  _widget_triplet_t *wt = setup->wg;
  xitk_widget_t           *frame = NULL;
  xitk_image_t            *image;
  xitk_image_widget_t      im;

  image = xitk_image_create_image_from_string (setup->gui->xitk, tabsfontname,
    FRAME_WIDTH, ALIGN_CENTER, (char *)title);

  XITK_WIDGET_INIT (&im);
  im.skin_element_name = NULL;
  wt->frame =  xitk_noskin_image_create (setup->widget_list, &im, image, x, y);
  if (wt->frame)
    xitk_add_widget (setup->widget_list, frame);

  wt->label = NULL;
  wt->widget = NULL;
  wt->changed = 0;

  setup->num_wg = 1;
}

/*
 *
 */
static void setup_section_widgets (xui_setup_t *setup, int s) {
  int                  y = 0; /* Position will be defined when painting widgets */
  int                  cfg_err_result;
  _setup_section_t    *section;
  /*int                  slidmax = 1;*/
  unsigned int         known_types;
  xine_cfg_entry_t     entry;

  known_types = (1 << XINE_CONFIG_TYPE_RANGE)
              | (1 << XINE_CONFIG_TYPE_STRING)
              | (1 << XINE_CONFIG_TYPE_ENUM)
              | (1 << XINE_CONFIG_TYPE_NUM)
              | (1 << XINE_CONFIG_TYPE_BOOL);

  xitk_disable_widget (setup->slider_wg);
  xitk_hide_widget (setup->slider_wg);

  section = setup->sections + s;
  memset (&entry, 0, sizeof (entry));

  cfg_err_result = setup->wg ? xine_config_get_first_entry (setup->gui->xine, &entry) : 0;

  while (cfg_err_result) {

    if ((entry.exp_level <= setup->gui->experience_level)
      && !strncmp (entry.key, section->name, section->nlen)
      && entry.description
      && ((1 << entry.type) & known_types)) {
      _widget_triplet_t   *wt = setup->wg + setup->num_wg;
      const char          *labelkey = entry.key + section->nlen;
      int                  x = (WINDOW_WIDTH >> 1) - (FRAME_WIDTH >> 1) - 11;
      xitk_label_widget_t  lb;

      if (setup->num_wg >= setup->max_wg) {
        /* should be rare. */
        wt = realloc (setup->wg, (setup->max_wg + 8) * sizeof (*setup->wg));
        if (!wt)
          break;
        setup->max_wg += 8;
        setup->wg = wt;
        wt += setup->num_wg;
      }
      setup->num_wg += 1;

      wt->cfg = entry;
      wt->changed = 0;

      {
        xitk_image_widget_t  im;
        xitk_image_t        *image = xitk_image_create_image (setup->gui->xitk, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);

        if (image) {
          xitk_image_fill_rectangle (image, 0, 0, FRAME_WIDTH + 1, FRAME_HEIGHT + 1,
            xitk_get_cfg_num (setup->gui->xitk, XITK_BG_COLOR));
          xitk_image_draw_inner_frame (image, (char *)entry.description,
            boldfontname, 0, 0, FRAME_WIDTH, FRAME_HEIGHT);
        }
        XITK_WIDGET_INIT (&im);
        im.skin_element_name = NULL;
        wt->frame = xitk_noskin_image_create (setup->widget_list, &im, image, x, y);
        if (wt->frame)
          xitk_add_widget (setup->widget_list, wt->frame);
      }
      x += 10;
      y += FRAME_HEIGHT >> 1;

      XITK_WIDGET_INIT (&lb);
      lb.skin_element_name = NULL;
      lb.label             = NULL;
      lb.callback          = NULL;
      lb.userdata          = NULL;

      switch (entry.type) {

        case XINE_CONFIG_TYPE_RANGE: /* slider */
          {
            xitk_intbox_widget_t ib;
            int ib_width;

            XITK_WIDGET_INIT (&ib);
            /* HACK for stuff like the xv color key. */
            if ((entry.range_min == 0) && (entry.range_max > 0x7fffffff / 260)) {
              ib.fmt = INTBOX_FMT_HASH;
              ib_width = 100;
            } else {
              ib.fmt = INTBOX_FMT_DECIMAL;
              ib_width = 260;
            }
            ib.skin_element_name = NULL;
            ib.min               = entry.range_min;
            ib.max               = entry.range_max;
            ib.value             = entry.num_value;
            ib.step              = 1;
            ib.callback          = numtype_update;
            ib.userdata          = wt;
            wt->widget = xitk_noskin_intbox_create (setup->widget_list, &ib, x, y, ib_width, 20);
            if (wt->widget)
              xitk_add_widget (setup->widget_list, wt->widget);
          };
          break;

        case XINE_CONFIG_TYPE_STRING:
          {
            xitk_inputtext_widget_t inp;

            XITK_WIDGET_INIT (&inp);
            inp.skin_element_name = NULL;
            inp.text              = entry.str_value;
            inp.max_length        = 256;
            inp.callback          = stringtype_update;
            inp.userdata          = wt;
            wt->widget = xitk_noskin_inputtext_create (setup->widget_list, &inp,
              x, y, 260, 20, "Black", "Black", fontname);
            if (wt->widget)
              xitk_add_widget (setup->widget_list, wt->widget);
          }
          break;

        case XINE_CONFIG_TYPE_ENUM:
          {
            xitk_combo_widget_t cmb;

            XITK_WIDGET_INIT (&cmb);
            cmb.skin_element_name = NULL;
            cmb.layer_above       = is_layer_above (setup->gui);
            cmb.entries           = (const char **)entry.enum_values;
            cmb.parent_wkey       = &setup->kreg;
            cmb.callback          = numtype_update;
            cmb.userdata          = wt;
            wt->widget = xitk_noskin_combo_create (setup->widget_list, &cmb, x, y, 260);
            if (wt->widget) {
              xitk_add_widget (setup->widget_list, wt->widget);
              xitk_combo_set_select (wt->widget, entry.num_value);
            }
          }
          break;

        case XINE_CONFIG_TYPE_NUM:
          {
            xitk_intbox_widget_t ib;

            XITK_WIDGET_INIT (&ib);
            ib.skin_element_name = NULL;
            ib.fmt               = INTBOX_FMT_DECIMAL;
            ib.min               = 0;
            ib.max               = 0;
            ib.value             = entry.num_value;
            ib.step              = 1;
            ib.callback          = numtype_update;
            ib.userdata          = wt;
            wt->widget = xitk_noskin_intbox_create (setup->widget_list, &ib, x, y, 60, 20);
            if (wt->widget)
              xitk_add_widget (setup->widget_list, wt->widget);
          }
          break;

        case XINE_CONFIG_TYPE_BOOL:
          {
            xitk_checkbox_widget_t cb;

            XITK_WIDGET_INIT (&cb);
            cb.skin_element_name = "XITK_NOSKIN_CHECK";
            cb.callback          = numtype_update;
            cb.userdata          = wt;
            wt->widget = xitk_noskin_checkbox_create (setup->widget_list, &cb, x, y, 13, 13);
            if (wt->widget) {
              xitk_add_widget (setup->widget_list, wt->widget);
              xitk_checkbox_set_state (wt->widget, entry.num_value);
            }
          }
          break;
      }

      {
        int lx;
        char b[200], *p = b, *e = b + sizeof (b);
        if (!entry.callback_data && !entry.callback) {
          p += strlcpy (p, labelkey, e - p);
          if (p < e)
            strlcpy (p, " (*)", e - p);
          lb.label = b;
        } else {
          lb.label = labelkey;
        }
        lx = x + 20 + xitk_get_widget_width (wt->widget);
        wt->label = xitk_noskin_label_create (setup->widget_list, &lb, lx, y, FRAME_WIDTH - lx - 15, setup->fh, fontname);
        xitk_widget_set_focus_redirect (wt->label, wt->widget);
        if (wt->label)
          xitk_add_widget (setup->widget_list, wt->label);
      }

      if (wt->frame)
        xitk_disable_and_hide_widget (wt->frame);
      {
        const char *help = entry.help ? entry.help : (const char *)_("No help available");
        if (wt->label) {
          xitk_set_widget_tips (wt->label, help);
          xitk_disable_widget_tips (wt->label);
          xitk_disable_and_hide_widget (wt->label);
        }
        if (wt->widget) {
          xitk_set_widget_tips (wt->widget, help);
          xitk_disable_widget_tips (wt->widget);
          xitk_disable_and_hide_widget (wt->widget);
        }
      }
    }

    memset (&entry, 0, sizeof (entry));
    cfg_err_result = xine_config_get_next_entry (setup->gui->xine, &entry);
  }

  section->num_entries = setup->num_wg;

  if (!setup->num_wg && setup->wg) {
    int x = (WINDOW_WIDTH >> 1) - (FRAME_WIDTH >> 1) - 11;
    setup_add_nothing_available (setup,
      _("There is no configuration option available in this user experience level."), x, y);
  }


  if (setup->num_wg > MAX_DISPLAY_WIDGETS) {
    xitk_slider_hv_t si;

    si.h.pos = 0;
    si.h.visible = 0;
    si.h.step = 0;
    si.h.max = 0;
    si.v.pos = 0;
    si.v.visible = MAX_DISPLAY_WIDGETS;
    si.v.step = 1;
    si.v.max = setup->num_wg;
    xitk_slider_hv_sync (setup->slider_wg, &si, XITK_SLIDER_SYNC_SET);
    /*slidmax = setup->num_wg - MAX_DISPLAY_WIDGETS;*/
  }
  /*else
    slidmax = 1;*/
#if 0
  xitk_slider_set_max (setup->slider_wg, slidmax);
  xitk_slider_set_pos (setup->slider_wg, slidmax);
#endif
}

/*
 *
 */
static void setup_change_section(xitk_widget_t *wx, void *data, int section) {
  xui_setup_t *setup = data;

  (void)wx;
  setup_set_cursor (setup, WAIT_CURS);

  /* remove old widgets */
  {
    _widget_triplet_t *wt;
    for (wt = setup->wg + setup->num_wg - 1; wt >= setup->wg; wt--) {
      if (wt->label) {
        xitk_destroy_widget (wt->label);
        wt->label = NULL;
      }
      if (wt->widget) {
        xitk_destroy_widget (wt->widget);
        wt->widget = NULL;
      }
      if (wt->frame) {
        xitk_destroy_widget (wt->frame);
        wt->frame = NULL;
      }
    }
  }
  setup->num_wg = 0;
  setup->first_displayed = 0;

  setup_section_widgets (setup, section);

  setup_clear_tab (setup);
  setup_paint_widgets (setup, 0);
}

/*
 * collect config categories, setup tab widget
 */
#ifdef XINE_SARRAY_MODE_UNIQUE
static int _section_cmp (void *a, void *b) {
  _setup_section_t *d = (_setup_section_t *)a;
  _setup_section_t *e = (_setup_section_t *)b;
  const uint8_t *n1 = (const uint8_t *)d->name;
  const uint8_t *n2 = (const uint8_t *)e->name;
  int i = d->nlen;
  /* these are quite short, inline strncmp (). */
  while (--i >= 0) {
    if (*n1 < *n2)
      return -1;
    if (*n1 > *n2)
      return 1;
    n1++;
    n2++;
  }
  return 0;
}
#endif

static void setup_sections (xui_setup_t *setup) {
  xitk_pixmap_t      *bg;
  xine_cfg_entry_t    entry;
  int                 cfg_err_result;
  xitk_tabs_widget_t  tab;
  char               *q, *e;
#ifdef XINE_SARRAY_MODE_UNIQUE
  xine_sarray_t      *a = xine_sarray_new (20, _section_cmp);

  xine_sarray_set_mode (a, XINE_SARRAY_MODE_UNIQUE);
#endif

  q = setup->namebuf;
  e = q + sizeof (setup->namebuf);
  setup->num_sections = 0;

  cfg_err_result = xine_config_get_first_entry (setup->gui->xine, &entry);
  while (cfg_err_result) {
    char *point = strchr (entry.key, '.');

    if ((entry.type != XINE_CONFIG_TYPE_UNKNOWN) && point) {
      _setup_section_t *s = setup->sections;
      int nlen = point - entry.key + 1, i;
#ifdef XINE_SARRAY_MODE_UNIQUE
      s += setup->num_sections;
      s->name = (char *)entry.key; /* will not be written to. */
      s->nlen = nlen;
      i = xine_sarray_add (a, s);
      if (i < 0) {
        s = xine_sarray_get (a, ~i);
        s->num_entries += 1;
      }
#else
      for (i = 0; i < setup->num_sections; i++) {
        if (!strncmp (s->name, entry.key, nlen))
          break;
        s++;
      }
      if (i < setup->num_sections) {
        s->num_entries += 1;
      }
#endif
      else if ((setup->num_sections < SETUP_MAX_SECTIONS) && (2 * nlen + 1 <= e - q)) {
        s->nlen = nlen;
        s->name = q;
        memcpy (q, entry.key, nlen);
        q += nlen;
        *q++ = 0;
        /* Aargh. xitk_noskin_tabs_create () does not copy the strings.
         * Give it a "permanent" copy without the trailing . */
        setup->section_names[setup->num_sections] = q;
        memcpy (q, entry.key, nlen);
        q += nlen;
        q[-1] = 0;
        s->num_entries = 1;
        setup->num_sections++;
      }
    }

    cfg_err_result = xine_config_get_next_entry (setup->gui->xine, &entry);
  }

#ifdef XINE_SARRAY_MODE_UNIQUE
  xine_sarray_delete (a);
#endif

  {
    int max = 1, i;

    for (i = 0; i < setup->num_sections; i++) {
      if (max < setup->sections[i].num_entries)
        max = setup->sections[i].num_entries;
    }
    setup->max_wg = (max + 7) & ~7;
    free (setup->wg);
    setup->wg = malloc (setup->max_wg * sizeof (*setup->wg));
  }

  XITK_WIDGET_INIT (&tab);

  tab.skin_element_name = NULL;
  tab.num_entries       = setup->num_sections;
  tab.entries           = setup->section_names;
  tab.callback          = setup_change_section;
  tab.userdata          = setup;
  setup->tabs = xitk_noskin_tabs_create (setup->widget_list, &tab, 15, 24, WINDOW_WIDTH - 30, tabsfontname);
  xitk_add_widget (setup->widget_list, setup->tabs);

  setup->th = xitk_get_widget_height (setup->tabs) - 1;

  xitk_enable_and_show_widget (setup->tabs);

  bg = xitk_window_get_background_pixmap (setup->xwin);
  draw_rectangular_box (bg, 15, (24 + setup->th),
    WINDOW_WIDTH - 30, MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30, DRAW_OUTTER);
  xitk_window_set_background (setup->xwin, bg);

  setup->num_wg = 0;
  setup->first_displayed = 0;
}

/*
 *
 */
void setup_end (xui_setup_t *setup) {
  setup_exit (NULL, setup, 0);
}

/*
 *
 */
static void setup_nextprev_wg(xitk_widget_t *w, void *data, int pos) {
  xui_setup_t *setup = data;
  int rpos = (xitk_slider_get_max (setup->slider_wg)) - pos;

  (void)w;
  setup_paint_widgets (setup, rpos);
}

void setup_reparent (xui_setup_t *setup) {
  if (!setup)
    return;
  reparent_window (setup->gui, setup->xwin);
}

/*
 * Create setup panel window
 */
xui_setup_t *setup_panel (gGui_t *gui) {
  xui_setup_t   *setup;
  xitk_widget_t *w;
  xitk_font_t   *fs;

  if (!gui)
    return NULL;
  setup = calloc (1, sizeof (*setup));
  if (!setup)
    return NULL;

  setup->gui = gui;

  {
    int x = 80, y = 80;

    gui_load_window_pos (setup->gui, "setup", &x, &y);
    /* Create window */
    setup->xwin = xitk_window_create_dialog_window (setup->gui->xitk,
      _("xine Setup"), x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
    set_window_states_start (setup->gui, setup->xwin);

    setup->widget_list = xitk_window_widget_list(setup->xwin);

    fs = xitk_font_load_font (setup->gui->xitk, fontname);
    xitk_widget_list_set_font (setup->widget_list, fs);
    setup->fh = xitk_font_get_string_height (fs, " ");
  }

  setup_sections (setup);

  {
    xitk_slider_widget_t sl;

    XITK_WIDGET_INIT (&sl);
    sl.min               = 0;
    sl.max               = 1;
    sl.step              = 1;
    sl.skin_element_name = NULL;
    sl.callback          = NULL;
    sl.userdata          = NULL;
    sl.motion_callback   = setup_nextprev_wg;
    sl.motion_userdata   = setup;
    setup->slider_wg =  xitk_noskin_slider_create (setup->widget_list, &sl,
      (WINDOW_WIDTH - 15 - 16 - 4 - 1), (24 + setup->th + 15),
      16, (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3), XITK_HVSLIDER);
  }
  xitk_add_widget (setup->widget_list, setup->slider_wg);

  setup_section_widgets (setup, 0);
  setup_paint_widgets (setup, 0);

  {
    xitk_label_widget_t lbl;
    char *label = _("(*)  you need to restart xine for this setting to take effect");
    int           len;

    len = xitk_font_get_string_length (fs, label);

    XITK_WIDGET_INIT(&lbl);
    lbl.skin_element_name   = NULL;
    lbl.label               = label;
    lbl.callback            = NULL;
    lbl.userdata            = setup;
    w =  xitk_noskin_label_create (setup->widget_list, &lbl,
      (WINDOW_WIDTH - len) >> 1, (24 + setup->th + MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30), len + 3, 18, fontname);
  }
  xitk_add_widget (setup->widget_list, w);
  xitk_enable_and_show_widget(w);

  xitk_font_unload_font(fs);

  {
    xitk_labelbutton_widget_t lb;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.state_callback    = NULL;
    lb.skin_element_name = NULL;
    lb.userdata          = setup;

    lb.label             = _("OK");
    lb.callback          = setup_ok;
    setup->ok = xitk_noskin_labelbutton_create (setup->widget_list, &lb,
      15, WINDOW_HEIGHT - (23 + 15), 100, 23, "Black", "Black", "White", tabsfontname);
    xitk_add_widget (setup->widget_list, setup->ok);
    xitk_enable_and_show_widget(setup->ok);

    lb.label             = _("Apply");
    lb.callback          = setup_apply;
    w = xitk_noskin_labelbutton_create (setup->widget_list, &lb,
      (WINDOW_WIDTH - 100) >> 1, WINDOW_HEIGHT - (23 + 15), 100, 23, "Black", "Black", "White", tabsfontname);
    xitk_add_widget (setup->widget_list, w);
    xitk_enable_and_show_widget(w);

    lb.label             = _("Close");
    lb.callback          = setup_exit;
    w =  xitk_noskin_labelbutton_create (setup->widget_list, &lb,
      WINDOW_WIDTH - (100 + 15), WINDOW_HEIGHT - (23 + 15), 100, 23, "Black", "Black", "White", tabsfontname);
    xitk_add_widget (setup->widget_list, w);
    xitk_enable_and_show_widget(w);
  }
  setup_show_tips (setup, panel_get_tips_enable (setup->gui->panel), panel_get_tips_timeout (setup->gui->panel));

  setup->kreg = xitk_window_register_event_handler("setup", setup->xwin, &setup_event_cbs, setup);

  setup->visible = 1;
  setup_raise_window (setup);

  xitk_window_try_to_set_input_focus(setup->xwin);

  setup->gui->setup = setup;
  return setup;
}
