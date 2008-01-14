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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * $Id$
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

extern gGui_t                   *gGui;

#define WINDOW_WIDTH             630
#define WINDOW_HEIGHT            530

#define FRAME_WIDTH              541
#define FRAME_HEIGHT             44

#define MAX_DISPLAY_WIDGETS      8

#define NORMAL_CURS              0
#define WAIT_CURS                1

#define ADD_FRAME(title) do {						                        \
    xitk_widget_t       *frame = NULL;                                                          \
    xitk_image_t        *image;                                                                 \
    xitk_image_widget_t  im;                                                                    \
                                                                                                \
    image = xitk_image_create_image(gGui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);       \
                                                                                                \
    XLockDisplay(gGui->display);                                                                \
    XSetForeground(gGui->display, (XITK_WIDGET_LIST_GC(setup->widget_list)),                    \
		   xitk_get_pixel_color_gray(gGui->imlib_data));                                \
    XFillRectangle(gGui->display, image->image->pixmap,                                         \
		   (XITK_WIDGET_LIST_GC(setup->widget_list)),                                   \
		   0, 0, image->width, image->height);                                          \
    XUnlockDisplay(gGui->display);                                                              \
                                                                                                \
    draw_inner_frame(gGui->imlib_data, image->image, (char *)title, boldfontname,               \
                     0, 0, FRAME_WIDTH, FRAME_HEIGHT);                                          \
	                                                                                        \
    XITK_WIDGET_INIT(&im, gGui->imlib_data);                                                    \
    im.skin_element_name = NULL;                                                                \
                                                                                                \
    xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),                      \
	      (frame = xitk_noskin_image_create(setup->widget_list, &im, image, x, y)));        \
                                                                                                \
    add_widget_to_list(frame);                                                                  \
    wt->frame = frame;                                                                          \
    x += 10;                                                                                    \
    y += FRAME_HEIGHT >> 1;                                                                     \
  } while(0)

#define ADD_LABEL(widget, cb, data) do {                                                        \
    int            wx, wy;                                                                      \
    xitk_widget_t *lbl;                                                                         \
    char           _labelkey[strlen(labelkey) + 5];                                             \
                                                                                                \
    snprintf(_labelkey, sizeof(_labelkey), "%s%s", labelkey,		                        \
      (!entry->callback_data && !entry->callback) ? " (*)" : "");                               \
                                                                                                \
    xitk_get_widget_pos(widget, &wx, &wy);                                                      \
    wx += xitk_get_widget_width(widget);                                                        \
                                                                                                \
    lbl = setup_add_label (wx + 20, 0,                                                          \
                           (FRAME_WIDTH - (xitk_get_widget_width(widget) + 35)),                \
                           _labelkey, cb, data);                                                \
    wt->label = lbl;                                                                            \
  } while(0)

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

#define ENABLE_ME(wtriplet) do {                                                                \
    if((wtriplet)->frame)                                                                       \
      xitk_enable_and_show_widget((wtriplet)->frame);                                           \
                                                                                                \
    if((wtriplet)->label) {                                                                     \
      xitk_enable_and_show_widget((wtriplet)->label);                                           \
      if(panel_get_tips_enable())					                        \
	xitk_set_widget_tips_timeout((wtriplet)->label, panel_get_tips_timeout());              \
      else								                        \
	xitk_disable_widget_tips((wtriplet)->label);			                        \
    }                                                                                           \
    if((wtriplet)->widget) {                                                                    \
      xitk_enable_and_show_widget((wtriplet)->widget);                                          \
      if(panel_get_tips_enable())					                        \
	xitk_set_widget_tips_timeout((wtriplet)->widget, panel_get_tips_timeout());             \
      else								                        \
	xitk_disable_widget_tips((wtriplet)->widget);			                        \
    }                                                                                           \
  } while(0)


typedef struct {
  xitk_widget_t        *frame;
  xitk_widget_t        *label;
  xitk_widget_t        *widget;
  int                   changed;
  xine_cfg_entry_t  *cfg;
} widget_triplet_t;

typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;
  xitk_widget_t        *ok;

  char                 *sections[20];
  int                   num_sections;
  
  /* Store widgets, this is needed to free them on tabs switching */
  xitk_list_t          *widgets;
  
  xitk_widget_t        *slider_wg;
  widget_triplet_t     *wg[1024]; /* I hope that will not be reached, never */
  int                   num_wg;
  int                   first_displayed;

  xitk_register_key_t   kreg;

} _setup_t;

static _setup_t    *setup = NULL;

static int          th; /* Tabs height */
static int          fh; /* Font height */

static void setup_change_section(xitk_widget_t *, void *, int);

static void add_widget_to_list(xitk_widget_t *w) {
  xitk_list_append_content(setup->widgets, (void *) w);  
}

/*
 * Leaving setup panel, release memory.
 */
static void setup_exit(xitk_widget_t *w, void *data) {

  if(setup) {
    window_info_t wi;
    
    setup->running = 0;
    setup->visible = 0;
    
    if((xitk_get_window_info(setup->kreg, &wi))) {
      config_update_num ("gui.setup_x", wi.x);
      config_update_num ("gui.setup_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&setup->kreg);
    
    xitk_destroy_widgets(setup->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, setup->xwin);
    
    setup->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(setup->widget_list)));
    xitk_list_free(setup->widgets);
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(setup->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(setup->widget_list);
    
    free(setup);
    setup = NULL;

    try_to_set_input_focus(gGui->video_window);
  }
}

void setup_show_tips(int enabled, unsigned long timeout) {
  
  if(setup) {
    if(enabled)
      xitk_set_widgets_tips_timeout(setup->widget_list, timeout);
    else
      xitk_disable_widgets_tips(setup->widget_list);
  }
}

void setup_update_tips_timeout(unsigned long timeout) {
  if(setup)
    xitk_set_widgets_tips_timeout(setup->widget_list, timeout);
}

/*
 * return 1 if setup panel is ON
 */
int setup_is_running(void) {

  if(setup != NULL)
    return setup->running;

  return 0;
}

/*
 * Return 1 if setup panel is visible
 */
int setup_is_visible(void) {

  if(setup != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(setup->xwin));
    else
      return setup->visible && xitk_is_window_visible(gGui->display, xitk_window_get_window(setup->xwin));
  }

  return 0;
}

/*
 * Raise setup->xwin
 */
void setup_raise_window(void) {
  if(setup != NULL)
    raise_window(xitk_window_get_window(setup->xwin), setup->visible, setup->running);
}

/*
 * Hide/show the setup panel
 */
void setup_toggle_visibility (xitk_widget_t *w, void *data) {
  if(setup != NULL)
    toggle_window(xitk_window_get_window(setup->xwin), setup->widget_list,
		  &setup->visible, setup->running);
}

static void setup_apply(xitk_widget_t *w, void *data) {
  int need_restart = 0;

  if(setup->num_wg > 0) {
    int i;

    for(i = 0; i < setup->num_wg; i++) {

      if(setup->wg[i]->widget) {
	int  type = xitk_get_widget_type(setup->wg[i]->widget);
	
	if(setup->wg[i]->changed || 
	   ((type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) || (type & WIDGET_GROUP_INTBOX)) {
	  xitk_widget_t *w      = setup->wg[i]->widget;
	  int            numval = 0;
	  char          *strval = NULL;
	  
	  if(!need_restart) {
	    if(setup->wg[i]->changed && (!setup->wg[i]->cfg->callback_data && !setup->wg[i]->cfg->callback))
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
	    if(!strcmp(strval, setup->wg[i]->cfg->str_value))
	      continue;
	    break;
	    
	  default:
	    if(type & WIDGET_GROUP_COMBO)
	      numval = xitk_combo_get_current_selected(w);
	    else if(type & WIDGET_GROUP_INTBOX) {
	      numval = xitk_intbox_get_value(w);
	      if(numval == setup->wg[i]->cfg->num_value)
		continue;
	    }
	    
	    break;
	  }

	  switch(setup->wg[i]->cfg->type) {
	  case XINE_CONFIG_TYPE_STRING:
	    config_update_string((char *)(setup->wg[i]->cfg)->key, strval);
	    break;
	  case XINE_CONFIG_TYPE_ENUM:
	  case XINE_CONFIG_TYPE_NUM:
	  case XINE_CONFIG_TYPE_BOOL:
	  case XINE_CONFIG_TYPE_RANGE:
	    config_update_num((char *)(setup->wg[i]->cfg)->key, numval);
	    break;
	  case XINE_CONFIG_TYPE_UNKNOWN:
	    break;
	  }
	}
      }
    }
    config_save();

    if(w != setup->ok)
      setup_change_section(setup->tabs, NULL, xitk_tabs_get_current_selected(setup->tabs));

    if(need_restart) {
      xitk_window_t *xw;
      
      xw = xitk_window_dialog_ok(gGui->imlib_data, _("Important Notice"),
				 NULL, NULL,
				 ALIGN_CENTER,
				 _("You changed some configuration value which require"
				   " to restart xine to take effect."));
      if(!gGui->use_root_window && gGui->video_display == gGui->display) {
	XLockDisplay(gGui->display);
	XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
	XUnlockDisplay(gGui->display);
      }
      layer_above_video(xitk_window_get_window(xw));
    }

  }
}

static void setup_set_cursor(int state) {
  if(setup) {
    if(state == WAIT_CURS)
      xitk_cursors_define_window_cursor(gGui->display, (xitk_window_get_window(setup->xwin)), xitk_cursor_watch);
    else
      xitk_cursors_restore_window_cursor(gGui->display, (xitk_window_get_window(setup->xwin)));
  }
}

static void setup_ok(xitk_widget_t *w, void *data) {
  setup_apply(w, data);
  setup_exit(w, data);
}

/*
 *
 */
static void setup_clear_tab(void) {
  xitk_image_t *im;

  im = xitk_image_create_image(gGui->imlib_data, (WINDOW_WIDTH - 30),
			       (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30));

  draw_outter(gGui->imlib_data, im->image, im->width, im->height);

  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, im->image->pixmap, (xitk_window_get_window(setup->xwin)),
	    (XITK_WIDGET_LIST_GC(setup->widget_list)), 0, 0, im->width, im->height, 15, (24 + th));
  XUnlockDisplay(gGui->display);

  xitk_image_free_image(gGui->imlib_data, &im);
}

/*
 *
 */
static void setup_paint_widgets(void) {
  int     i;
  int     last;
  int     wx, wy, y = (24 + th + 13);

  last = setup->num_wg <= (setup->first_displayed + MAX_DISPLAY_WIDGETS)
    ? setup->num_wg : (setup->first_displayed + MAX_DISPLAY_WIDGETS);

  /* First, disable all widgets */
  for(i = 0; i < setup->num_wg; i++)
    DISABLE_ME(setup->wg[i]);

  /* Move widgets to new position, then display them */
  for(i = setup->first_displayed; i < last; i++) {

    if(setup->wg[i]->frame) {
      xitk_get_widget_pos(setup->wg[i]->frame, &wx, &wy);
      xitk_set_widget_pos(setup->wg[i]->frame, wx, y);
    }

    y += FRAME_HEIGHT >> 1;

    if(setup->wg[i]->label) {
      xitk_get_widget_pos(setup->wg[i]->label, &wx, &wy);
      xitk_set_widget_pos(setup->wg[i]->label,
			  wx, y + 4 - xitk_get_widget_height(setup->wg[i]->label) / 2);
    }

    if(setup->wg[i]->widget) {
      xitk_get_widget_pos(setup->wg[i]->widget, &wx, &wy);
      xitk_set_widget_pos(setup->wg[i]->widget,
			  wx, y + 4 - xitk_get_widget_height(setup->wg[i]->widget) / 2);
    }
    ENABLE_ME(setup->wg[i]);

    y += (FRAME_HEIGHT>>1) + 3;
  }

  /* Repaint them now */
  xitk_paint_widget_list (setup->widget_list); 
  setup_set_cursor(NORMAL_CURS);
}

/*
 * Handle X events here.
 */
static void setup_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {
    
  case ButtonPress:
    if(event->xbutton.button == Button4) {
      xitk_slider_make_step(setup->slider_wg);
      xitk_slider_callback_exec(setup->slider_wg);
    }
    else if(event->xbutton.button == Button5) {
      xitk_slider_make_backstep(setup->slider_wg);
      xitk_slider_callback_exec(setup->slider_wg);
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
	if(xitk_is_widget_enabled(setup->slider_wg) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_step(setup->slider_wg);
	  xitk_slider_callback_exec(setup->slider_wg);
	  return;
	}
	break;
	
      case XK_Down:
	if(xitk_is_widget_enabled(setup->slider_wg) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_backstep(setup->slider_wg);
	  xitk_slider_callback_exec(setup->slider_wg);
	  return;
	}
	break;
	
      case XK_Next:
	if(xitk_is_widget_enabled(setup->slider_wg)) {
	  int pos, max = xitk_slider_get_max(setup->slider_wg);
	  
	  pos = max - (setup->first_displayed + MAX_DISPLAY_WIDGETS);
	  xitk_slider_set_pos(setup->slider_wg, (pos >= 0) ? pos : 0);
	  xitk_slider_callback_exec(setup->slider_wg);
	  return;
	}
	break;
	
      case XK_Prior:
	if(xitk_is_widget_enabled(setup->slider_wg)) {
	  int pos, max = xitk_slider_get_max(setup->slider_wg);
	  
	  pos = max - (setup->first_displayed - MAX_DISPLAY_WIDGETS);
	  xitk_slider_set_pos(setup->slider_wg, (pos <= max) ? pos : max);
	  xitk_slider_callback_exec(setup->slider_wg);
	  return;
	}
	break;
	
      case XK_Escape:
	setup_exit(NULL, NULL);
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
static xitk_widget_t *setup_add_label (int x, int y, int w, 
				       const char *str, xitk_simple_callback_t cb, void *data) {
  xitk_label_widget_t   lb;
  xitk_widget_t        *label;

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.window              = xitk_window_get_window(setup->xwin);
  lb.gc                  = (XITK_WIDGET_LIST_GC(setup->widget_list));
  lb.skin_element_name   = NULL;
  lb.label               = (char *)str;
  lb.callback            = cb;
  lb.userdata            = data;

  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)), 
			   (label = xitk_noskin_label_create(setup->widget_list, &lb,
							     x, y, w, fh, fontname)));

  add_widget_to_list(label);

  return label;
}

/*
 *
 */
static void numtype_update(xitk_widget_t *w, void *data, int value) {
  widget_triplet_t *triplet = (widget_triplet_t *) data;

  triplet->changed = 1;
}

/*
 *
 */
static void stringtype_update(xitk_widget_t *w, void *data, char *str) {
  widget_triplet_t *triplet = (widget_triplet_t *) data;

  triplet->changed = 1;
}

static widget_triplet_t *setup_add_nothing_available(const char *title, int x, int y) {
  static widget_triplet_t *wt; 
  xitk_widget_t           *frame = NULL;
  xitk_image_t            *image;
  xitk_image_widget_t      im;
  
  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));
  
  image = xitk_image_create_image_from_string(gGui->imlib_data, tabsfontname,
					       FRAME_WIDTH, ALIGN_CENTER, (char *)title);
  
  XITK_WIDGET_INIT(&im, gGui->imlib_data);
  im.skin_element_name = NULL;
  
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),
			    (frame = xitk_noskin_image_create(setup->widget_list, &im, image, x, y)));
  
  add_widget_to_list(frame);

  wt->frame = frame;
  wt->label = NULL;
  wt->widget = NULL;
  
  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_slider (const char *title, const char *labelkey, 
					   int x, int y, xine_cfg_entry_t *entry) {
  xitk_slider_widget_t     sl;
  xitk_widget_t           *slider;
  static widget_triplet_t *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  ADD_FRAME(title);

  sl.min                      = entry->range_min;
  sl.max                      = entry->range_max;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = numtype_update;
  sl.motion_userdata          = (void *)wt;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)),
			   (slider = xitk_noskin_slider_create(setup->widget_list, &sl,
							       x, y, 150, 16,
							       XITK_HSLIDER)));
  xitk_slider_set_pos(slider, entry->num_value);
  
  ADD_LABEL(slider, NULL, NULL);
  
  add_widget_to_list(slider);
  
  wt->widget = slider;
  wt->cfg    = entry;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_inputnum(const char *title, const char *labelkey, 
					    int x, int y, xine_cfg_entry_t *entry) {
  xitk_intbox_widget_t      ib;
  xitk_widget_t            *intbox, *wi, *wbu, *wbd;
  static widget_triplet_t  *wt;
  
  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&ib, gGui->imlib_data);

  ADD_FRAME(title);

  ib.skin_element_name = NULL;
  ib.value             = entry->num_value;
  ib.step              = 1;
  ib.parent_wlist      = setup->widget_list;
  ib.callback          = numtype_update;
  ib.userdata          = (void *)wt;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),
    (intbox = 
     xitk_noskin_intbox_create(setup->widget_list, &ib, x, y, 60, 20, &wi, &wbu, &wbd)));

  ADD_LABEL(intbox, NULL, NULL);
  
  add_widget_to_list(intbox);
  add_widget_to_list(wi);
  add_widget_to_list(wbu);
  add_widget_to_list(wbd);
  
  wt->widget = intbox;
  wt->cfg    = entry;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_inputtext(const char *title, const char *labelkey, 
					     int x, int y, xine_cfg_entry_t *entry) {
  xitk_inputtext_widget_t   inp;
  xitk_widget_t            *input;
  static widget_triplet_t  *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&inp, gGui->imlib_data);

  ADD_FRAME(title);

  inp.skin_element_name = NULL;
  inp.text              = entry->str_value;
  inp.max_length        = 256;
  inp.callback          = stringtype_update;
  inp.userdata          = (void *)wt;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),
			   (input = 
			    xitk_noskin_inputtext_create(setup->widget_list, &inp,
							 x, y, 260, 20,
							 "Black", "Black", fontname)));

  ADD_LABEL(input, NULL, NULL);

  add_widget_to_list(input);
    
  wt->widget = input;
  wt->cfg    = entry;
  
  return wt;
}

/*
 *
 */
static void label_cb(xitk_widget_t *w, void *data) {
  xitk_widget_t  *checkbox = (xitk_widget_t *) data;

  xitk_checkbox_set_state(checkbox, !(xitk_checkbox_get_state(checkbox)));
  xitk_checkbox_callback_exec(checkbox);
}
static widget_triplet_t *setup_add_checkbox (const char *title, const char *labelkey, 
					     int x, int y, xine_cfg_entry_t *entry) {
  xitk_checkbox_widget_t    cb;
  xitk_widget_t            *checkbox;
  static widget_triplet_t  *wt;
  
  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  ADD_FRAME(title);

  cb.skin_element_name = NULL;
  cb.callback          = numtype_update;
  cb.userdata          = (void *)wt;

  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),
			   (checkbox = 
			    xitk_noskin_checkbox_create(setup->widget_list, &cb,
							x, y, 12, 12)));
  xitk_checkbox_set_state (checkbox, entry->num_value);  
  ADD_LABEL(checkbox, label_cb, (void *) checkbox);

  add_widget_to_list(checkbox);

  wt->widget = checkbox;
  wt->cfg    = entry;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_combo (const char *title, const char *labelkey, 
					  int x, int y, xine_cfg_entry_t *entry ) {
  xitk_combo_widget_t       cmb;
  xitk_widget_t            *combo, *lw, *bw;
  static widget_triplet_t  *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);

  ADD_FRAME(title);

  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.parent_wlist      = setup->widget_list;
  cmb.entries           = (const char **)entry->enum_values;
  cmb.parent_wkey       = &setup->kreg;
  cmb.callback          = numtype_update;
  cmb.userdata          = (void *)wt;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)), 
			   (combo = 
			    xitk_noskin_combo_create(setup->widget_list, &cmb,
						     x, y, 260, &lw, &bw)));
  xitk_combo_set_select(combo, entry->num_value );

  ADD_LABEL(combo, NULL, NULL);

  add_widget_to_list(combo);
  add_widget_to_list(lw);
  add_widget_to_list(bw);

  wt->widget = combo;
  wt->cfg    = entry;

  return wt;
}

/*
 *
 */
static void setup_section_widgets(int s) {
  int                  x = ((WINDOW_WIDTH>>1) - (FRAME_WIDTH>>1) - 11);
  int                  y = 0; /* Position will be defined when painting widgets */
  xine_cfg_entry_t *entry;
  int                  cfg_err_result;
  int                  len;
  char                *section;
  const char          *labelkey;
  int                  slidmax = 1;

  xitk_disable_widget(setup->slider_wg);
  xitk_hide_widget(setup->slider_wg);
  
  section = setup->sections[s];
  len     = strlen (section);
  entry   = (xine_cfg_entry_t *)xine_xmalloc(sizeof(xine_cfg_entry_t));
  cfg_err_result   = xine_config_get_first_entry(gGui->xine, entry);
    
  while (cfg_err_result) {
      
    if((entry->exp_level <= gGui->experience_level) &&
       ((!strncmp(entry->key, section, len)) && entry->description)) {
	
      labelkey = &entry->key[len+1];
	
      switch (entry->type) {

#define _SET_HELP do {						                                           \
	  xitk_set_widget_tips_and_timeout(setup->wg[setup->num_wg]->widget,                               \
					   (entry->help) ?  (char *) entry->help : _("No help available"), \
					   panel_get_tips_timeout());	                                   \
	  xitk_set_widget_tips_and_timeout(setup->wg[setup->num_wg]->label,                                \
					   (entry->help) ?  (char *) entry->help : _("No help available"), \
					   panel_get_tips_timeout());	                                   \
	} while(0)
      case XINE_CONFIG_TYPE_RANGE: /* slider */
	setup->wg[setup->num_wg] = setup_add_slider (entry->description, labelkey, x, y, entry);
	_SET_HELP;
	DISABLE_ME(setup->wg[setup->num_wg]);
	setup->num_wg++;
	break;
	
      case XINE_CONFIG_TYPE_STRING:
	setup->wg[setup->num_wg] = setup_add_inputtext (entry->description, labelkey, x, y, entry);
	_SET_HELP;
	DISABLE_ME(setup->wg[setup->num_wg]);
	setup->num_wg++;
	break;
	
      case XINE_CONFIG_TYPE_ENUM:
	setup->wg[setup->num_wg] = setup_add_combo (entry->description, labelkey, x, y, entry);
	_SET_HELP;
	DISABLE_ME(setup->wg[setup->num_wg]);
	setup->num_wg++;
	break;
	
      case XINE_CONFIG_TYPE_NUM:
	setup->wg[setup->num_wg] = setup_add_inputnum (entry->description, labelkey, x, y, entry);
	_SET_HELP;
	DISABLE_ME(setup->wg[setup->num_wg]);
	setup->num_wg++;
	break;
	
      case XINE_CONFIG_TYPE_BOOL:
	setup->wg[setup->num_wg] = setup_add_checkbox (entry->description, labelkey, x, y, entry);
	_SET_HELP;
	DISABLE_ME(setup->wg[setup->num_wg]);
	setup->num_wg++;
	break;
	  
      }
      
    } else
      free(entry);
      
    entry = (xine_cfg_entry_t *)xine_xmalloc(sizeof(xine_cfg_entry_t));
    cfg_err_result = xine_config_get_next_entry(gGui->xine, entry);
  }
  free(entry);

    
  if(setup->num_wg == 0) {
    setup->wg[setup->num_wg++] = 
      setup_add_nothing_available(_("There is no configuration option available in "
				    "this user experience level."), x, y);
  }


  if(setup->num_wg > MAX_DISPLAY_WIDGETS) {
    slidmax = setup->num_wg - MAX_DISPLAY_WIDGETS;
    xitk_show_widget(setup->slider_wg);
    xitk_enable_widget(setup->slider_wg);
  }
  else
    slidmax = 1;

  xitk_slider_set_max(setup->slider_wg, slidmax);
  xitk_slider_set_pos(setup->slider_wg, slidmax);

}

/*
 *
 */
static void setup_change_section(xitk_widget_t *wx, void *data, int section) {
  int i;
  xitk_widget_t *sw;

  setup_set_cursor(WAIT_CURS);

  for (i = 0; i < setup->num_wg; i++ )
    free(setup->wg[i]);

  /* remove old widgets */
  sw = (xitk_widget_t *) xitk_list_first_content(setup->widgets);
  while(sw) {
    xitk_widget_t *w;
    
    w = (xitk_widget_t *) xitk_list_first_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)));
    
    while (w) {
      
      if (sw == w) {
	xitk_destroy_widget(sw);

	xitk_list_delete_current ((XITK_WIDGET_LIST_LIST(setup->widget_list)));
	
	w = (xitk_widget_t *) (XITK_WIDGET_LIST_LIST(setup->widget_list))->cur;
	
      } else
	w = (xitk_widget_t *) xitk_list_next_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)));
    } 

    sw = (xitk_widget_t *) xitk_list_next_content(setup->widgets);
  }

  
  xitk_list_clear(setup->widgets);
  setup->num_wg = 0;
  setup->first_displayed = 0;

  setup_section_widgets (section);
  
  setup_clear_tab();
  setup_paint_widgets();
}

/* 
 * collect config categories, setup tab widget
 */
static void setup_sections (void) {
  xitk_pixmap_t       *bg;
  xine_cfg_entry_t  entry;
  int                  cfg_err_result;
  xitk_tabs_widget_t   tab;

  setup->num_sections = 0;
  cfg_err_result = xine_config_get_first_entry(gGui->xine, &entry);
  while (cfg_err_result) {

    char *point;
    
    point = strchr(entry.key, '.');
    
    if (entry.type != XINE_CONFIG_TYPE_UNKNOWN && point) {
      int found ;
      int i;
      int len;

      len = point - entry.key;
      found = 0;

      for (i=0; i<setup->num_sections; i++) {
	if (!strncmp (setup->sections[i], entry.key, len)) {
	  found = 1;
	  break;
	}
      }

      if (!found) {
        setup->sections[setup->num_sections] = strndup(entry.key, len);
	setup->num_sections++;
      }
    }      
    
    cfg_err_result = xine_config_get_next_entry(gGui->xine, &entry);
  }

  XITK_WIDGET_INIT(&tab, gGui->imlib_data);

  tab.skin_element_name = NULL;
  tab.num_entries       = setup->num_sections;
  tab.entries           = setup->sections;
  tab.parent_wlist      = setup->widget_list;
  tab.callback          = setup_change_section;
  tab.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(setup->widget_list)),
    (setup->tabs = 
     xitk_noskin_tabs_create(setup->widget_list, &tab, 15, 24, WINDOW_WIDTH - 30, tabsfontname)));

  th = xitk_get_widget_height(setup->tabs) - 1;
  
  xitk_enable_and_show_widget(setup->tabs);

  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(setup->xwin)), bg->pixmap,
	    bg->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 15, (24 + th),
			      (WINDOW_WIDTH - 30 - 1), (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30 - 1));
  xitk_window_change_background(gGui->imlib_data, setup->xwin, bg->pixmap,
				WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_image_destroy_xitk_pixmap(bg);

  setup->widgets = xitk_list_new();

  setup->num_wg = 0;
  setup->first_displayed = 0;
}

/*
 *
 */
void setup_end(void) {
  setup_exit(NULL, NULL);
}

/*
 *
 */
static void setup_nextprev_wg(xitk_widget_t *w, void *data, int pos) {
  int rpos = (xitk_slider_get_max(setup->slider_wg)) - pos;

  if(rpos != setup->first_displayed) {
    setup->first_displayed = rpos;
    setup_clear_tab();
    setup_paint_widgets();
  }
}

void setup_reparent(void) {
  if(setup)
    reparent_window((xitk_window_get_window(setup->xwin)));
}

/*
 * Create setup panel window
 */
void setup_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  xitk_slider_widget_t       sl;
  xitk_label_widget_t        lbl;
  int                        x, y;
  xitk_widget_t             *w;
  xitk_font_t               *fs;

  setup = (_setup_t *) xine_xmalloc(sizeof(_setup_t));

  x = xine_config_register_num (gGui->xine, "gui.setup_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.setup_y", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  setup->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						 _("xine Setup"),
						 x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start((xitk_window_get_window(setup->xwin)));

  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(setup->xwin)), None, None);
  XUnlockDisplay (gGui->display);

  setup->widget_list                = xitk_widget_list_new();
  xitk_widget_list_set(setup->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(setup->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(setup->xwin)));
  xitk_widget_list_set(setup->widget_list, WIDGET_LIST_GC, gc);

  fs = xitk_font_load_font(gGui->display, fontname);
  xitk_font_set_font(fs, (XITK_WIDGET_LIST_GC(setup->widget_list)));
  fh = xitk_font_get_string_height(fs, " ");

  setup_sections();

  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = setup_nextprev_wg;
  sl.motion_userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)),
   (setup->slider_wg = xitk_noskin_slider_create(setup->widget_list, &sl,
						 (WINDOW_WIDTH - 15 - 16 - 4 - 1), (24 + th + 15),
						 16, (MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3),
						 XITK_VSLIDER)));

  setup_section_widgets (0);

  setup_paint_widgets();

  {
    char         *label = _("(*)  you need to restart xine for this setting to take effect");
    int           len;
    
    len = xitk_font_get_string_length(fs, (const char *) label);
    
    XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
    
    lbl.window              = xitk_window_get_window(setup->xwin);
    lbl.gc                  = (XITK_WIDGET_LIST_GC(setup->widget_list));
    lbl.skin_element_name   = NULL;
    lbl.label               = label;
    lbl.callback            = NULL;
    lbl.userdata            = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)),
			     (w = xitk_noskin_label_create(setup->widget_list, &lbl,
							   (WINDOW_WIDTH - len) >> 1,
							   (24 + th + MAX_DISPLAY_WIDGETS * (FRAME_HEIGHT + 3) - 3 + 3 + 30),
							   len + 3, 18, fontname)));
    xitk_enable_and_show_widget(w);
  }

  xitk_font_unload_font(fs);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("OK");
  lb.align             = ALIGN_CENTER;
  lb.callback          = setup_ok; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)), 
   (setup->ok = xitk_noskin_labelbutton_create(setup->widget_list, &lb, 
					       15, WINDOW_HEIGHT - (23 + 15), 100, 23,
					       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(setup->ok);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = setup_apply; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)), 
   (w = xitk_noskin_labelbutton_create(setup->widget_list, &lb, 
				       (WINDOW_WIDTH - 100) >> 1, WINDOW_HEIGHT - (23 + 15), 100, 23,
				       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(w);
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = setup_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(setup->widget_list)), 
   (w = xitk_noskin_labelbutton_create(setup->widget_list, &lb, 
				       WINDOW_WIDTH - (100 + 15), WINDOW_HEIGHT - (23 + 15), 100, 23,
				       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(w);
  setup_show_tips(panel_get_tips_enable(), panel_get_tips_timeout());
  
  setup->kreg = xitk_register_event_handler("setup", 
					    (xitk_window_get_window(setup->xwin)),
					    setup_handle_event,
					    NULL,
					    NULL,
					    setup->widget_list,
					   NULL);
  
  setup->visible = 1;
  setup->running = 1;
  setup_raise_window();

  try_to_set_input_focus(xitk_window_get_window(setup->xwin));
}
