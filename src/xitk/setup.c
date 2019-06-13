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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"

#include <xine/list.h>


#define WINDOW_WIDTH             630
#define WINDOW_HEIGHT            530

#define FRAME_WIDTH              541
#define FRAME_HEIGHT             44

#define MAX_DISPLAY_WIDGETS      8

#define NORMAL_CURS              0
#define WAIT_CURS                1

#define DISABLE_ME(wtriplet) do {                                                               \
    if((wtriplet)->frame)                                                                       \
      xitk_disable_and_hide_widget((wtriplet)->frame);                                          \
                                                                                                \
    if((wtriplet)->label) {                                                                     \
      xitk_disable_and_hide_widget((wtriplet)->label);                                          \
      xitk_disable_widget_tips((wtriplet)->label);		 	                        \
    }                                                                                           \
    if((wtriplet)->widget) {                                                                    \
      xitk_disable_and_hide_widget((wtriplet)->widget);                                         \
      xitk_disable_widget_tips((wtriplet)->widget);			                        \
    }                                                                                           \
  } while(0)


typedef struct {
  xitk_widget_t    *frame;
  xitk_widget_t    *label;
  xitk_widget_t    *widget;
  int               changed;
  xine_cfg_entry_t  cfg;
} widget_triplet_t;

struct xui_setup_st {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;
  xitk_widget_t        *ok;

  char                 *sections[20];
  int                   num_sections;
  
  /* Store widgets, this is needed to free them on tabs switching */
  xine_list_t          *widgets;
  
  xitk_widget_t        *slider_wg;
  widget_triplet_t     *wg[1024]; /* I hope that will not be reached, never */
  int                   num_wg;
  int                   first_displayed;

  xitk_register_key_t   kreg;

  int                   th; /* Tabs height */
  int                   fh; /* Font height */
};

static void setup_change_section (xitk_widget_t *, void *, int);

static void add_widget_to_list (xui_setup_t *setup, xitk_widget_t *w) {
  xine_list_push_back (setup->widgets, w);
}

/*
 * Leaving setup panel, release memory.
 */
static void setup_exit (xitk_widget_t *w, void *data) {
  xui_setup_t *setup = data;
  window_info_t wi;

  (void)w;
  if (!setup)
    return;
  /*
  if (!setup->running)
    return;
  */
  setup->running = 0;
  setup->visible = 0;
    
  if ((xitk_get_window_info (setup->kreg, &wi))) {
    config_update_num ("gui.setup_x", wi.x);
    config_update_num ("gui.setup_y", wi.y);
    WINDOW_INFO_ZERO (&wi);
  }
    
  xitk_unregister_event_handler (&setup->kreg);
    
  xitk_destroy_widgets (setup->widget_list);
  xitk_window_destroy_window (setup->gui->imlib_data, setup->xwin);
    
  setup->xwin = NULL;
  /* xitk_dlist_init (&setup->widget_list->list); */
  xine_list_delete (setup->widgets);
    
  XLockDisplay (setup->gui->display);
  XFreeGC (setup->gui->display, (XITK_WIDGET_LIST_GC (setup->widget_list)));
  XUnlockDisplay (setup->gui->display);
    
  XITK_WIDGET_LIST_FREE (setup->widget_list);
    
  try_to_set_input_focus (setup->gui->video_window);

  setup->gui->setup = NULL;
  free (setup);
}

void setup_show_tips (xui_setup_t *setup, int enabled, unsigned long timeout) {
  if (!setup)
    return;
  if (setup->running) {
    if (enabled)
      xitk_set_widgets_tips_timeout (setup->widget_list, timeout);
    else
      xitk_disable_widgets_tips (setup->widget_list);
  }
}

void setup_update_tips_timeout (xui_setup_t *setup, unsigned long timeout) {
  if (!setup)
    return;
  if (setup->running)
    xitk_set_widgets_tips_timeout (setup->widget_list, timeout);
}

/*
 * return 1 if setup panel is ON
 */
int setup_is_running (xui_setup_t *setup) {
  return setup ? setup->running : 0;
}

/*
 * Return 1 if setup panel is visible
 */
int setup_is_visible (xui_setup_t *setup) {
  if (!setup)
    return 0;
  if (setup->running) {
    if (setup->gui->use_root_window)
      return xitk_is_window_visible (setup->gui->display, xitk_window_get_window (setup->xwin));
    else
      return setup->visible && xitk_is_window_visible (setup->gui->display, xitk_window_get_window (setup->xwin));
  }
  return 0;
}

/*
 * Raise setup->xwin
 */
void setup_raise_window (xui_setup_t *setup) {
  if (!setup)
    return;
  if (setup->running)
    raise_window (xitk_window_get_window (setup->xwin), setup->visible, setup->running);
}

/*
 * Hide/show the setup panel
 */
void setup_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_setup_t *setup = data;
  (void)w;
  if (!setup)
    return;
  if (setup->running)
    toggle_window (xitk_window_get_window (setup->xwin), setup->widget_list,
      &setup->visible, setup->running);
}

static void setup_apply (xitk_widget_t *w, void *data) {
  xui_setup_t *setup = data;
  int need_restart = 0;

  (void)w;
  if (setup->num_wg > 0) {
    int i;

    for (i = 0; i < setup->num_wg; i++) {

      if (setup->wg[i]->widget) {
        int type = xitk_get_widget_type (setup->wg[i]->widget);

        if (setup->wg[i]->changed ||
          ((type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) || (type & WIDGET_GROUP_INTBOX)) {
          xitk_widget_t *w = setup->wg[i]->widget;
          int            numval = 0;
          char          *strval = NULL;

	  if(!need_restart) {
            if (setup->wg[i]->changed && (!setup->wg[i]->cfg.callback_data && !setup->wg[i]->cfg.callback))
	      need_restart = 1;
	  }

          setup->wg[i]->changed = 0;

	  switch(type & WIDGET_TYPE_MASK) {
	  case WIDGET_TYPE_SLIDER:
	    numval = xitk_slider_get_pos(w);
	    break;

	  case WIDGET_TYPE_CHECKBOX:
	    numval = xitk_checkbox_get_state(w);
	    break;

	  case WIDGET_TYPE_INPUTTEXT:
	    strval = xitk_inputtext_get_text(w);
	    if(!strcmp(strval, setup->wg[i]->cfg.str_value))
	      continue;
	    break;

	  default:
	    if(type & WIDGET_GROUP_COMBO)
	      numval = xitk_combo_get_current_selected(w);
	    else if(type & WIDGET_GROUP_INTBOX) {
	      numval = xitk_intbox_get_value(w);
              if (numval == setup->wg[i]->cfg.num_value)
		continue;
	    }

	    break;
	  }

          switch (setup->wg[i]->cfg.type) {
	  case XINE_CONFIG_TYPE_STRING:
            config_update_string ((char *)setup->wg[i]->cfg.key, strval);
	    break;
	  case XINE_CONFIG_TYPE_ENUM:
	  case XINE_CONFIG_TYPE_NUM:
	  case XINE_CONFIG_TYPE_BOOL:
	  case XINE_CONFIG_TYPE_RANGE:
            config_update_num ((char *)setup->wg[i]->cfg.key, numval);
	    break;
	  case XINE_CONFIG_TYPE_UNKNOWN:
	    break;
	  }
	}
      }
    }
    xine_config_save (setup->gui->xine, __xineui_global_config_file);

    if(w != setup->ok) {
      int section = xitk_tabs_get_current_selected (setup->tabs);
      if (section >= 0) {
        setup_change_section (setup->tabs, setup, section);
      }
    }

    if(need_restart) {
      xitk_window_t *xw;
      
      xw = xitk_window_dialog_ok (setup->gui->imlib_data, _("Important Notice"),
				 NULL, NULL,
				 ALIGN_CENTER,
				 _("You changed some configuration value which require"
				   " to restart xine to take effect."));
      if (!setup->gui->use_root_window && setup->gui->video_display == setup->gui->display) {
        XLockDisplay (setup->gui->display);
        XSetTransientForHint (setup->gui->display, xitk_window_get_window(xw), setup->gui->video_window);
        XUnlockDisplay (setup->gui->display);
      }
      layer_above_video(xitk_window_get_window(xw));
    }

  }
}

static void setup_set_cursor (xui_setup_t *setup, int state) {
  if (setup->running) {
    if(state == WAIT_CURS)
      xitk_cursors_define_window_cursor (setup->gui->display, (xitk_window_get_window (setup->xwin)), xitk_cursor_watch);
    else
      xitk_cursors_restore_window_cursor (setup->gui->display, (xitk_window_get_window (setup->xwin)));
  }
}

static void setup_ok (xitk_widget_t *w, void *data) {
  setup_apply(w, data);
  setup_exit(w, data);
}

/*
 *
 */
static void setup_clear_tab (xui_setup_t *setup) {
  xitk_image_t *im;

  im = xitk_image_create_image (setup->gui->imlib_data, (WINDOW_WIDTH - 30),
    (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30));

  draw_outter (setup->gui->imlib_data, im->image, im->width, im->height);

  XLockDisplay (setup->gui->display);
  XCopyArea (setup->gui->display, im->image->pixmap, (xitk_window_get_window(setup->xwin)),
    (XITK_WIDGET_LIST_GC (setup->widget_list)), 0, 0, im->width, im->height, 15, (24 + setup->th));
  XUnlockDisplay (setup->gui->display);

  xitk_image_free_image (setup->gui->imlib_data, &im);
}

/*
 *
 */
static void setup_paint_widgets (xui_setup_t *setup) {
  int i;
  int last;
  int wx, wy, y = (24 + setup->th + 13);
  int tips_timeout;

  tips_timeout = !panel_get_tips_enable (setup->gui->panel) ? 0
               : panel_get_tips_timeout (setup->gui->panel);

  last = setup->num_wg <= (setup->first_displayed + MAX_DISPLAY_WIDGETS)
    ? setup->num_wg : (setup->first_displayed + MAX_DISPLAY_WIDGETS);

  /* First, disable all widgets */
  for (i = 0; i < setup->num_wg; i++)
    DISABLE_ME (setup->wg[i]);

  /* Move widgets to new position, then display them */
  for (i = setup->first_displayed; i < last; i++) {
    widget_triplet_t *t = setup->wg[i];

    if (t->frame) {
      xitk_get_widget_pos (t->frame, &wx, &wy);
      xitk_set_widget_pos (t->frame, wx, y);
    }

    y += FRAME_HEIGHT >> 1;

    if (t->label) {
      xitk_get_widget_pos (t->label, &wx, &wy);
      xitk_set_widget_pos (t->label,
        wx, y + 4 - xitk_get_widget_height (t->label) / 2);
    }

    if (t->widget) {
      xitk_get_widget_pos (t->widget, &wx, &wy);
      xitk_set_widget_pos (t->widget,
        wx, y + 4 - xitk_get_widget_height (t->widget) / 2);
    }

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

    y += (FRAME_HEIGHT >> 1) + 3;
  }

  /* Repaint them now */
  xitk_paint_widget_list (setup->widget_list); 
  setup_set_cursor (setup, NORMAL_CURS);
}

/*
 * Handle X events here.
 */
static void setup_handle_event(XEvent *event, void *data) {
  xui_setup_t *setup = data;
  
  switch(event->type) {
    
  case ButtonPress:
    if(event->xbutton.button == Button4) {
      xitk_slider_make_step (setup->slider_wg);
      xitk_slider_callback_exec (setup->slider_wg);
    }
    else if(event->xbutton.button == Button5) {
      xitk_slider_make_backstep (setup->slider_wg);
      xitk_slider_callback_exec (setup->slider_wg);
    }
    break;

  case KeyPress:
    {
      KeySym         mkey;
      int            modifier;
      
      xitk_get_key_modifier(event, &modifier);
      mkey = xitk_get_key_pressed(event);
      
      switch(mkey) {

      case XK_Up:
        if (xitk_is_widget_enabled (setup->slider_wg) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
          xitk_slider_make_step (setup->slider_wg);
          xitk_slider_callback_exec (setup->slider_wg);
	  return;
	}
	break;

      case XK_Down:
        if (xitk_is_widget_enabled (setup->slider_wg) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
          xitk_slider_make_backstep (setup->slider_wg);
          xitk_slider_callback_exec (setup->slider_wg);
	  return;
	}
	break;

      case XK_Next:
        if (xitk_is_widget_enabled(setup->slider_wg)) {
          int pos, max = xitk_slider_get_max (setup->slider_wg);

          pos = max - (setup->first_displayed + MAX_DISPLAY_WIDGETS);
          xitk_slider_set_pos (setup->slider_wg, (pos >= 0) ? pos : 0);
          xitk_slider_callback_exec (setup->slider_wg);
	  return;
	}
	break;

      case XK_Prior:
        if (xitk_is_widget_enabled(setup->slider_wg)) {
          int pos, max = xitk_slider_get_max (setup->slider_wg);

          pos = max - (setup->first_displayed - MAX_DISPLAY_WIDGETS);
          xitk_slider_set_pos (setup->slider_wg, (pos <= max) ? pos : max);
          xitk_slider_callback_exec (setup->slider_wg);
	  return;
	}
	break;

      case XK_Escape:
        setup_exit (NULL, setup);
	return;
      }
      gui_handle_event(event, data);
    }
    break;
  }
}

/*
 *
 */
static xitk_widget_t *setup_add_label (xui_setup_t *setup,
  int x, int y, int w, const char *str, xitk_simple_callback_t cb, void *data) {
  xitk_label_widget_t   lb;
  xitk_widget_t        *label;

  XITK_WIDGET_INIT (&lb, setup->gui->imlib_data);

  lb.window              = xitk_window_get_window (setup->xwin);
  lb.gc                  = (XITK_WIDGET_LIST_GC (setup->widget_list));
  lb.skin_element_name   = NULL;
  lb.label               = (char *)str;
  lb.callback            = cb;
  lb.userdata            = data;
  label =  xitk_noskin_label_create (setup->widget_list, &lb, x, y, w, setup->fh, fontname);
  xitk_add_widget (setup->widget_list, label);
  add_widget_to_list (setup, label);

  return label;
}

/*
 *
 */
static void numtype_update(xitk_widget_t *w, void *data, int value) {
  widget_triplet_t *triplet = (widget_triplet_t *) data;

  (void)w;
  (void)value;
  triplet->changed = 1;
}

/*
 *
 */
static void stringtype_update(xitk_widget_t *w, void *data, char *str) {
  widget_triplet_t *triplet = (widget_triplet_t *) data;

  (void)w;
  (void)str;
  triplet->changed = 1;
}

static widget_triplet_t *setup_add_nothing_available (xui_setup_t *setup, const char *title, int x, int y) {
  static widget_triplet_t *wt; 
  xitk_widget_t           *frame = NULL;
  xitk_image_t            *image;
  xitk_image_widget_t      im;
  
  wt = (widget_triplet_t *) calloc(1, sizeof(widget_triplet_t));
  
  image = xitk_image_create_image_from_string (setup->gui->imlib_data, tabsfontname,
    FRAME_WIDTH, ALIGN_CENTER, (char *)title);
  
  XITK_WIDGET_INIT (&im, setup->gui->imlib_data);
  im.skin_element_name = NULL;
  frame =  xitk_noskin_image_create (setup->widget_list, &im, image, x, y);
  xitk_add_widget (setup->widget_list, frame);
  add_widget_to_list (setup, frame);

  wt->frame = frame;
  wt->label = NULL;
  wt->widget = NULL;
  
  return wt;
}

static void label_cb(xitk_widget_t *w, void *data) {
  xitk_widget_t  *checkbox = (xitk_widget_t *) data;

  (void)w;
  xitk_checkbox_set_state(checkbox, !(xitk_checkbox_get_state(checkbox)));
  xitk_checkbox_callback_exec(checkbox);
}

#define ADD_LABEL(widget,cb,data) do { \
    int wx, wy; \
    xitk_widget_t *lbl; \
    char b[200], *p = b, *e = b + sizeof (b); \
    p += strlcpy (p, labelkey, e - p); \
    if (!entry.callback_data && !entry.callback && (p < e)) \
      strlcpy (p, " (*)", e - p); \
    xitk_get_widget_pos (widget, &wx, &wy); \
    wx += xitk_get_widget_width (widget); \
    lbl = setup_add_label (setup, wx + 20, 0, \
      (FRAME_WIDTH - (xitk_get_widget_width (widget) + 35)), b, cb, data); \
    wt->label = lbl; \
  } while(0)

#define _SET_HELP do { \
  char *help = entry.help ? (char *)entry.help : _("No help available"); \
  xitk_set_widget_tips_and_timeout (wt->widget, help, tips_timeout); \
  xitk_set_widget_tips_and_timeout (wt->label, help, tips_timeout); \
} while(0)

/*
 *
 */
static void setup_section_widgets (xui_setup_t *setup, int s) {
  int                  y = 0; /* Position will be defined when painting widgets */
  int                  cfg_err_result;
  int                  len;
  char                *section;
  int                  slidmax = 1;
  unsigned int         known_types;
  int                  tips_timeout = panel_get_tips_timeout (setup->gui->panel);
  xine_cfg_entry_t     entry;

  known_types = (1 << XINE_CONFIG_TYPE_RANGE)
              | (1 << XINE_CONFIG_TYPE_STRING)
              | (1 << XINE_CONFIG_TYPE_ENUM)
              | (1 << XINE_CONFIG_TYPE_NUM)
              | (1 << XINE_CONFIG_TYPE_BOOL);

  xitk_disable_widget (setup->slider_wg);
  xitk_hide_widget (setup->slider_wg);
  
  section = setup->sections[s];
  len     = strlen (section);
  memset (&entry, 0, sizeof (entry));
  cfg_err_result   = xine_config_get_first_entry (setup->gui->xine, &entry);
    
  while (cfg_err_result) {
      
    if ((entry.exp_level <= setup->gui->experience_level)
      && !strncmp (entry.key, section, len)
      && entry.description
      && ((1 << entry.type) & known_types)) {
      widget_triplet_t    *wt;
      xitk_widget_t       *frame = NULL;
      xitk_image_t        *image;
      xitk_image_widget_t  im;
      const char          *labelkey = entry.key + len + 1;
      int                  x = (WINDOW_WIDTH >> 1) - (FRAME_WIDTH >> 1) - 11;

      wt = calloc (1, sizeof (*wt));
      setup->wg[setup->num_wg++] = wt;
      wt->cfg = entry;

      image = xitk_image_create_image (setup->gui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);
      XLockDisplay (setup->gui->display);
      XSetForeground (setup->gui->display, (XITK_WIDGET_LIST_GC (setup->widget_list)),
        xitk_get_pixel_color_gray (setup->gui->imlib_data));
      XFillRectangle (setup->gui->display, image->image->pixmap,
        (XITK_WIDGET_LIST_GC (setup->widget_list)), 0, 0, image->width, image->height);
      XUnlockDisplay (setup->gui->display);
      draw_inner_frame (setup->gui->imlib_data, image->image, (char *)entry.description,
        boldfontname, 0, 0, FRAME_WIDTH, FRAME_HEIGHT);
      XITK_WIDGET_INIT (&im, setup->gui->imlib_data);
      im.skin_element_name = NULL;
      frame = xitk_noskin_image_create (setup->widget_list, &im, image, x, y);
      xitk_add_widget (setup->widget_list, frame);
      add_widget_to_list (setup, frame);
      wt->frame = frame;
      x += 10;
      y += FRAME_HEIGHT >> 1;

      switch (entry.type) {

        case XINE_CONFIG_TYPE_RANGE: /* slider */
          {
            xitk_slider_widget_t sl;
            xitk_widget_t *slider;

            XITK_WIDGET_INIT (&sl, setup->gui->imlib_data);
            sl.min                      = entry.range_min;
            sl.max                      = entry.range_max;
            sl.step                     = 1;
            sl.skin_element_name        = NULL;
            sl.callback                 = NULL;
            sl.userdata                 = NULL;
            sl.motion_callback          = numtype_update;
            sl.motion_userdata          = wt;
            slider = xitk_noskin_slider_create (setup->widget_list, &sl, x, y, 150, 16, XITK_HSLIDER);
            xitk_add_widget (setup->widget_list, slider);
            xitk_slider_set_pos (slider, entry.num_value);
            ADD_LABEL (slider, NULL, NULL);
            add_widget_to_list (setup, slider);
            wt->widget = slider;
          };
          _SET_HELP;
          break;

        case XINE_CONFIG_TYPE_STRING:
          {
            xitk_inputtext_widget_t inp;
            xitk_widget_t *input;

            XITK_WIDGET_INIT (&inp, setup->gui->imlib_data);
            inp.skin_element_name = NULL;
            inp.text              = entry.str_value;
            inp.max_length        = 256;
            inp.callback          = stringtype_update;
            inp.userdata          = wt;
            input = xitk_noskin_inputtext_create (setup->widget_list, &inp,
              x, y, 260, 20, "Black", "Black", fontname);
            xitk_add_widget (setup->widget_list, input);
            ADD_LABEL (input, NULL, NULL);
            add_widget_to_list (setup, input);
            wt->widget = input;
          }
          _SET_HELP;
          break;

        case XINE_CONFIG_TYPE_ENUM:
          {
            xitk_combo_widget_t cmb;
            xitk_widget_t *combo, *lw, *bw;

            XITK_WIDGET_INIT (&cmb, setup->gui->imlib_data);
            cmb.skin_element_name = NULL;
            cmb.layer_above       = (is_layer_above ());
            cmb.parent_wlist      = setup->widget_list;
            cmb.entries           = (const char **)entry.enum_values;
            cmb.parent_wkey       = &setup->kreg;
            cmb.callback          = numtype_update;
            cmb.userdata          = wt;
            combo = xitk_noskin_combo_create (setup->widget_list, &cmb, x, y, 260, &lw, &bw);
            xitk_add_widget (setup->widget_list, combo);
            xitk_combo_set_select (combo, entry.num_value);
            ADD_LABEL (combo, NULL, NULL);
            add_widget_to_list (setup, combo);
            add_widget_to_list (setup, lw);
            add_widget_to_list (setup, bw);
            wt->widget = combo;
          }
          _SET_HELP;
          break;

        case XINE_CONFIG_TYPE_NUM:
          {
            xitk_intbox_widget_t ib;
            xitk_widget_t *intbox, *wi, *wbu, *wbd;

            XITK_WIDGET_INIT (&ib, setup->gui->imlib_data);
            ib.skin_element_name = NULL;
            ib.value             = entry.num_value;
            ib.step              = 1;
            ib.parent_wlist      = setup->widget_list;
            ib.callback          = numtype_update;
            ib.userdata          = wt;
            intbox = xitk_noskin_intbox_create (setup->widget_list, &ib, x, y, 60, 20, &wi, &wbu, &wbd);
            xitk_add_widget (setup->widget_list, intbox);
            ADD_LABEL (intbox, NULL, NULL);
            add_widget_to_list (setup, intbox);
            add_widget_to_list (setup, wi);
            add_widget_to_list (setup, wbu);
            add_widget_to_list (setup, wbd);
            wt->widget = intbox;
          }
          _SET_HELP;
          break;

        case XINE_CONFIG_TYPE_BOOL:
          {
            xitk_checkbox_widget_t cb;
            xitk_widget_t *checkbox;

            XITK_WIDGET_INIT (&cb, setup->gui->imlib_data);
            cb.skin_element_name = NULL;
            cb.callback          = numtype_update;
            cb.userdata          = wt;
            checkbox = xitk_noskin_checkbox_create (setup->widget_list, &cb, x, y, 12, 12);
            xitk_add_widget (setup->widget_list, checkbox);
            xitk_checkbox_set_state (checkbox, entry.num_value);
            ADD_LABEL (checkbox, label_cb, (void *)checkbox);
            add_widget_to_list (setup, checkbox);
            wt->widget = checkbox;
          }
          _SET_HELP;
          break;
      }
      
      DISABLE_ME (wt);
    }
      
    memset (&entry, 0, sizeof (entry));
    cfg_err_result = xine_config_get_next_entry (setup->gui->xine, &entry);
  }

    
  if(setup->num_wg == 0) {
    int x = (WINDOW_WIDTH >> 1) - (FRAME_WIDTH >> 1) - 11;
    setup->wg[setup->num_wg++] = setup_add_nothing_available (setup,
      _("There is no configuration option available in this user experience level."), x, y);
  }


  if (setup->num_wg > MAX_DISPLAY_WIDGETS) {
    slidmax = setup->num_wg - MAX_DISPLAY_WIDGETS;
    xitk_show_widget (setup->slider_wg);
    xitk_enable_widget (setup->slider_wg);
  }
  else
    slidmax = 1;

  xitk_slider_set_max (setup->slider_wg, slidmax);
  xitk_slider_set_pos (setup->slider_wg, slidmax);

}

/*
 *
 */
static void setup_change_section(xitk_widget_t *wx, void *data, int section) {
  xui_setup_t *setup = data;
  int i;

  (void)wx;
  setup_set_cursor (setup, WAIT_CURS);

  for (i = 0; i < setup->num_wg; i++ )
    free (setup->wg[i]);
  setup->num_wg = 0;
  setup->first_displayed = 0;

  /* remove old widgets */
  {
    xitk_widget_t *sw;
    xine_list_iterator_t ite = NULL;
    while (1) {
      sw = xine_list_next_value (setup->widgets, &ite);
      if (!ite)
        break;
      xitk_destroy_widget (sw);
    }
  }
  xine_list_clear (setup->widgets);

  setup_section_widgets (setup, section);
  
  setup_clear_tab (setup);
  setup_paint_widgets (setup);
}

/* 
 * collect config categories, setup tab widget
 */
static void setup_sections (xui_setup_t *setup) {
  xitk_pixmap_t       *bg;
  xine_cfg_entry_t  entry;
  int                  cfg_err_result;
  xitk_tabs_widget_t   tab;

  setup->num_sections = 0;
  cfg_err_result = xine_config_get_first_entry (setup->gui->xine, &entry);
  while (cfg_err_result) {

    char *point;
    
    point = strchr(entry.key, '.');
    
    if (entry.type != XINE_CONFIG_TYPE_UNKNOWN && point) {
      int found ;
      int i;
      int len;

      len = point - entry.key;
      found = 0;

      for (i = 0; i < setup->num_sections; i++) {
        if (!strncmp (setup->sections[i], entry.key, len)) {
	  found = 1;
	  break;
	}
      }

      if (!found) {
        setup->sections[setup->num_sections] = strndup (entry.key, len);
        setup->num_sections++;
      }
    }      
    
    cfg_err_result = xine_config_get_next_entry (setup->gui->xine, &entry);
  }

  XITK_WIDGET_INIT (&tab, setup->gui->imlib_data);

  tab.skin_element_name = NULL;
  tab.num_entries       = setup->num_sections;
  tab.entries           = setup->sections;
  tab.parent_wlist      = setup->widget_list;
  tab.callback          = setup_change_section;
  tab.userdata          = setup;
  setup->tabs = xitk_noskin_tabs_create (setup->widget_list, &tab, 15, 24, WINDOW_WIDTH - 30, tabsfontname);
  xitk_add_widget (setup->widget_list, setup->tabs);

  setup->th = xitk_get_widget_height (setup->tabs) - 1;
  
  xitk_enable_and_show_widget (setup->tabs);

  bg = xitk_image_create_xitk_pixmap (setup->gui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (setup->gui->display);
  XCopyArea (setup->gui->display, (xitk_window_get_background (setup->xwin)), bg->pixmap,
    bg->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay (setup->gui->display);
  
  draw_rectangular_outter_box (setup->gui->imlib_data, bg, 15, (24 + setup->th),
    (WINDOW_WIDTH - 30 - 1), (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30 - 1));
  xitk_window_change_background (setup->gui->imlib_data, setup->xwin, bg->pixmap,
    WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_image_destroy_xitk_pixmap(bg);

  setup->widgets = xine_list_new ();

  setup->num_wg = 0;
  setup->first_displayed = 0;
}

/*
 *
 */
void setup_end (xui_setup_t *setup) {
  setup_exit (NULL, setup);
}

/*
 *
 */
static void setup_nextprev_wg(xitk_widget_t *w, void *data, int pos) {
  xui_setup_t *setup = data;
  int rpos = (xitk_slider_get_max (setup->slider_wg)) - pos;

  (void)w;
  if (rpos != setup->first_displayed) {
    setup->first_displayed = rpos;
    setup_clear_tab (setup);
    setup_paint_widgets (setup);
  }
}

void setup_reparent (xui_setup_t *setup) {
  if (!setup)
    return;
  if (setup->running)
    reparent_window ((xitk_window_get_window (setup->xwin)));
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
    Window win;
    GC gc;
    int x, y;

    x = xine_config_register_num (setup->gui->xine, "gui.setup_x", 80,
      CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
    y = xine_config_register_num (setup->gui->xine, "gui.setup_y", 80,
      CONFIG_NO_DESC, CONFIG_NO_HELP, CONFIG_LEVEL_DEB, CONFIG_NO_CB, CONFIG_NO_DATA);
    /* Create window */
    setup->xwin = xitk_window_create_dialog_window (setup->gui->imlib_data,
      _("xine Setup"), x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
    win = xitk_window_get_window (setup->xwin);
    set_window_states_start (win);

    XLockDisplay (setup->gui->display);
    gc = XCreateGC (setup->gui->display, win, None, None);
    XUnlockDisplay (setup->gui->display);

    setup->widget_list = xitk_widget_list_new ();
    xitk_widget_list_set (setup->widget_list, WIDGET_LIST_WINDOW, (void *)win);
    xitk_widget_list_set (setup->widget_list, WIDGET_LIST_GC, gc);

    fs = xitk_font_load_font (setup->gui->display, fontname);
    xitk_font_set_font (fs, gc);
    setup->fh = xitk_font_get_string_height (fs, " ");
  }

  setup_sections (setup);

  {
    xitk_slider_widget_t sl;

    XITK_WIDGET_INIT (&sl, setup->gui->imlib_data);
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
      16, (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3), XITK_VSLIDER);
  }
  xitk_add_widget (setup->widget_list, setup->slider_wg);

  setup_section_widgets (setup, 0);
  setup_paint_widgets (setup);

  {
    xitk_label_widget_t lbl;
    char *label = _("(*)  you need to restart xine for this setting to take effect");
    int           len;
    
    len = xitk_font_get_string_length (fs, label);
    
    XITK_WIDGET_INIT(&lbl, gui->imlib_data);
    lbl.window              = xitk_window_get_window (setup->xwin);
    lbl.gc                  = (XITK_WIDGET_LIST_GC (setup->widget_list));
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

    XITK_WIDGET_INIT (&lb, setup->gui->imlib_data);
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

  setup->kreg = xitk_register_event_handler ("setup",
    xitk_window_get_window (setup->xwin),
    setup_handle_event, NULL, NULL,
    setup->widget_list, setup);
  
  setup->visible = 1;
  setup->running = 1;
  setup_raise_window (setup);

  try_to_set_input_focus (xitk_window_get_window (setup->xwin));

  setup->gui->setup = setup;
  return setup;
}
