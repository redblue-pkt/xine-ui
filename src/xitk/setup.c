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

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "actions.h"
#include "viewlog.h"
#include "kbindings.h"
#include "skins.h"
#include "lang.h"
#include "i18n.h"

#include "xitk.h"

extern gGui_t              *gGui;

static char                *fontname     = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char                *boldfontname = "-*-helvetica-bold-r-*-*-10-*-*-*-*-*-*-*";
static char                *tabsfontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       480

#define FRAME_WIDTH         350
#define FRAME_HEIGHT        40

#define MAX_DISPLAY_WIDGETS 8
#define BROWSER_MAX_DISP_ENTRIES 17

#define ADD_FRAME(title) {                                                                      \
    xitk_widget_t       *frame = NULL;                                                          \
    xitk_image_t        *image;                                                                 \
    xitk_image_widget_t  im;                                                                    \
    int                  lbearing, rbearing, width, ascent, descent;                            \
    xitk_font_t         *fs;                                                                    \
                                                                                                \
    image = xitk_image_create_image(gGui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);       \
                                                                                                \
    fs = xitk_font_load_font(gGui->display, boldfontname);                                      \
    xitk_font_set_font(fs, setup->widget_list->gc);                                             \
    xitk_font_text_extent(fs, title, strlen(title),                                             \
                          &lbearing, &rbearing, &width, &ascent, &descent);                     \
    xitk_font_unload_font(fs);                                                                  \
                                                                                                \
    XLockDisplay(gGui->display);                                                                \
                                                                                                \
    XSetForeground(gGui->display, setup->widget_list->gc,                                       \
		   xitk_get_pixel_color_gray(gGui->imlib_data));                                \
    XFillRectangle(gGui->display, image->image->pixmap, setup->widget_list->gc,                 \
		   0, 0, image->width, image->height);                                          \
    XUnlockDisplay(gGui->display);                                                              \
                                                                                                \
    draw_inner_frame(gGui->imlib_data, image->image->pixmap, title, boldfontname,               \
                     0, (ascent+descent), FRAME_WIDTH, FRAME_HEIGHT);                           \
	                                                                                        \
    XITK_WIDGET_INIT(&im, gGui->imlib_data);                                                    \
    im.skin_element_name = NULL;                                                                \
                                                                                                \
    xitk_list_append_content (setup->widget_list->l,                                            \
	      (frame = xitk_noskin_image_create(setup->widget_list, &im, image, x, y)));        \
                                                                                                \
    setup->tmp_widgets[setup->num_tmp_widgets] = frame;                                         \
    setup->num_tmp_widgets++;                                                                   \
    wt->frame = frame;                                                                          \
    x += 10;                                                                                    \
    y += FRAME_HEIGHT >> 1;                                                                     \
  }

#define ADD_LABEL(widget) {                                                                     \
    int           wx, wy, wh, fh;                                                               \
    xitk_font_t  *fs;                                                                           \
    xitk_widget_t *lbl;                                                                         \
                                                                                                \
    fs = xitk_font_load_font(gGui->display, fontname);                                          \
    xitk_font_set_font(fs, setup->widget_list->gc);                                             \
    fh = xitk_font_get_string_height(fs, labelkey);                                             \
    xitk_font_unload_font(fs);                                                                  \
                                                                                                \
    xitk_get_widget_pos(widget, &wx, &wy);                                                      \
    wx += xitk_get_widget_width(widget);                                                        \
    wh = xitk_get_widget_height(widget);                                                        \
                                                                                                \
    lbl = setup_add_label (wx + 20, (wy + (wh >> 1)) - (fh>>1), (FRAME_WIDTH - wx), labelkey);  \
    wt->label = lbl;                                                                            \
  }

#define DISABLE_ME(wtriplet) {                                                                  \
    if((wtriplet)->frame) {                                                                     \
      xitk_disable_widget((wtriplet)->frame);                                                   \
      xitk_hide_widget(setup->widget_list, (wtriplet)->frame);                                  \
    }                                                                                           \
    if((wtriplet)->label) {                                                                     \
      xitk_disable_widget((wtriplet)->label);                                                   \
      xitk_hide_widget(setup->widget_list, (wtriplet)->label);                                  \
    }                                                                                           \
    if((wtriplet)->widget) {                                                                    \
      xitk_disable_widget((wtriplet)->widget);                                                  \
      xitk_hide_widget(setup->widget_list, (wtriplet)->widget);                                 \
    }                                                                                           \
}

#define ENABLE_ME(wtriplet) {                                                                   \
    if((wtriplet)->frame) {                                                                     \
      xitk_enable_widget((wtriplet)->frame);                                                    \
      xitk_show_widget(setup->widget_list, (wtriplet)->frame);                                  \
    }                                                                                           \
    if((wtriplet)->label) {                                                                     \
      xitk_enable_widget((wtriplet)->label);                                                    \
      xitk_show_widget(setup->widget_list, (wtriplet)->label);                                  \
    }                                                                                           \
    if((wtriplet)->widget) {                                                                    \
      xitk_enable_widget((wtriplet)->widget);                                                   \
      xitk_show_widget(setup->widget_list, (wtriplet)->widget);                                 \
    }                                                                                           \
}


typedef struct {
  xitk_widget_t        *frame;
  xitk_widget_t        *label;
  xitk_widget_t        *widget;
} widget_triplet_t;

typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;

  char                 *sections[20];
  int                   num_sections;
  
  xitk_widget_t        *tmp_widgets[200];
  int                   num_tmp_widgets;

  xitk_widget_t        *slider_wg;
  widget_triplet_t     *wg[100];
  int                   num_wg;
  int                   first_displayed;

  char                **config_content;
  int                   config_lines;
  char                **readme_content;
  int                   readme_lines;
  char                **faq_content;
  int                   faq_lines;

  xitk_register_key_t   kreg;

} _setup_t;

static _setup_t    *setup = NULL;

static void setup_end(xitk_widget_t *, void *);

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
 * Leaving setup panel, release memory.
 */
void setup_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;
  int           i;

  setup->running = 0;
  setup->visible = 0;

  if((xitk_get_window_info(setup->kreg, &wi))) {
    gGui->config->update_num (gGui->config, "gui.setup_x", wi.x);
    gGui->config->update_num (gGui->config, "gui.setup_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  xitk_unregister_event_handler(&setup->kreg);

  XLockDisplay(gGui->display);
  XUnmapWindow(gGui->display, xitk_window_get_window(setup->xwin));
  XUnlockDisplay(gGui->display);

  xitk_destroy_widgets(setup->widget_list);

  XLockDisplay(gGui->display);
  XDestroyWindow(gGui->display, xitk_window_get_window(setup->xwin));
  XUnlockDisplay(gGui->display);

  setup->xwin = None;
  xitk_list_free(setup->widget_list->l);
  
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, setup->widget_list->gc);
  XUnlockDisplay(gGui->display);

  free(setup->widget_list);

  if(setup->config_content) {
    for(i = 0; i < setup->config_lines; i++) {
      free(setup->config_content[i]);
    }
    free(setup->config_content);
  }

  if(setup->faq_content) {
    for(i = 0; i < setup->faq_lines; i++) {
      free(setup->faq_content[i]);
    }
    free(setup->faq_content);
  }

  if(setup->readme_content) {
    for(i = 0; i < setup->readme_lines; i++) {
      free(setup->readme_content[i]);
    }
    free(setup->readme_content);
  }

  free(setup);
  setup = NULL;

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

  if(setup != NULL)
    return setup->visible;

  return 0;
}

/*
 * Raise setup->xwin
 */
void setup_raise_window(void) {
  
  if(setup != NULL) {
    if(setup->xwin) {
      if(setup->visible && setup->running) {
	if(setup->running) {
	  XLockDisplay(gGui->display);
	  XMapRaised(gGui->display, xitk_window_get_window(setup->xwin));
	  setup->visible = 1;
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(setup->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(setup->xwin));
	}
      } else {
	XLockDisplay(gGui->display);
	XUnmapWindow (gGui->display, xitk_window_get_window(setup->xwin));
	XUnlockDisplay(gGui->display);
	setup->visible = 0;
      }
    }
  }
}

/*
 * Hide/show the setup panel
 */
void setup_toggle_visibility (xitk_widget_t *w, void *data) {
  
  if(setup != NULL) {
    if (setup->visible && setup->running) {
      setup->visible = 0;
      xitk_hide_widgets(setup->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, xitk_window_get_window(setup->xwin));
      XUnlockDisplay(gGui->display);
    } else {
      if(setup->running) {
	setup->visible = 1;
	xitk_show_widgets(setup->widget_list);
	XLockDisplay(gGui->display);
	XMapRaised(gGui->display, xitk_window_get_window(setup->xwin)); 
	XSetTransientForHint (gGui->display, 
			      xitk_window_get_window(setup->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(setup->xwin));
      }
    }
  }
}

/*
 *
 */
static void setup_clear_tab(void) {
  xitk_image_t *im;

  im = xitk_image_create_image(gGui->imlib_data, (WINDOW_WIDTH - 40), 
			       (WINDOW_HEIGHT - (51 + 57) + 1));

  draw_outter(gGui->imlib_data, im->image->pixmap, im->width, im->height);

  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, im->image->pixmap, (xitk_window_get_window(setup->xwin)),
	    setup->widget_list->gc, 0, 0, im->width, im->height, 20, 51);
  XUnlockDisplay(gGui->display);

  xitk_image_free_image(gGui->imlib_data, &im);
}

/*
 *
 */
static void setup_paint_widgets(void) {
  int     i;
  int     last;
  int     wx, wy, y = 70;

  last = setup->num_wg <= (setup->first_displayed + MAX_DISPLAY_WIDGETS)
    ? setup->num_wg : (setup->first_displayed + MAX_DISPLAY_WIDGETS);

  /* First, disable all widgets */
  for(i = 0; i < setup->num_wg; i++)
    DISABLE_ME(setup->wg[i]);

  /* Move widgets to new position, then display its */
  for(i = setup->first_displayed; i < last; i++) {

    if(setup->wg[i]->frame) {
      xitk_get_widget_pos(setup->wg[i]->frame, &wx, &wy);
      xitk_set_widget_pos(setup->wg[i]->frame, wx, y);

      y += FRAME_HEIGHT >> 1;
    }

    if(setup->wg[i]->label) {
      xitk_get_widget_pos(setup->wg[i]->label, &wx, &wy);
      xitk_set_widget_pos(setup->wg[i]->label, wx, y);
    }

    xitk_get_widget_pos(setup->wg[i]->widget, &wx, &wy);
    /* Inputtext/intbox/combo widgets have special treatments */
    if((setup->wg[i]->widget->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) {
      xitk_set_widget_pos(setup->wg[i]->widget, wx, y - 4);
    }
    else if(((setup->wg[i]->widget->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) ||
    	    ((setup->wg[i]->widget->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX))
      xitk_set_widget_pos(setup->wg[i]->widget, wx, y - 5);
    else
      xitk_set_widget_pos(setup->wg[i]->widget, wx, y);
    
    ENABLE_ME(setup->wg[i]);

    y += (FRAME_HEIGHT>>1) + 2;
  }

  /* Repaint them now */
  xitk_paint_widget_list (setup->widget_list); 
}

/*
 * Handle X events here.
 */
static void setup_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {
    
  case KeyPress: {
    XKeyEvent      mykeyevent;
    KeySym         mykey;
    char           kbuf[256];
    int            len;
    int            modifier;
    
    if(xitk_is_widget_enabled(setup->slider_wg)) {
      
      mykeyevent = event->xkey;
      
      xitk_get_key_modifier(event, &modifier);
      
      XLockDisplay(gGui->display);
      len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
      XUnlockDisplay(gGui->display);
      
      switch(mykey) {
	
      case XK_Up:
	if((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_step(setup->widget_list, setup->slider_wg);
	  xitk_slider_callback_exec(setup->slider_wg);
	}
	break;
	
      case XK_Down:
	if((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) { 
	  xitk_slider_make_backstep(setup->widget_list, setup->slider_wg);
	  xitk_slider_callback_exec(setup->slider_wg);
	}
	break;

      }
    }
  }
  break;
  
  }
}

/*
 *
 */
static xitk_widget_t *setup_add_label (int x, int y, int w, char *str) {
  xitk_label_widget_t   lb;
  xitk_widget_t        *label;
  xitk_font_t          *fs;
  int                   height;

  fs = xitk_font_load_font(gGui->display, fontname);
  xitk_font_set_font(fs, setup->widget_list->gc);
  height = xitk_font_get_string_height(fs, str);
  xitk_font_unload_font(fs);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.window              = xitk_window_get_window(setup->xwin);
  lb.gc                  = setup->widget_list->gc;
  lb.skin_element_name   = NULL;
  lb.label               = str;
  lb.callback            = NULL;

  xitk_list_append_content(setup->widget_list->l, 
			   (label = xitk_noskin_label_create(setup->widget_list, &lb,
							     x, y, w, height, fontname)));

  setup->tmp_widgets[setup->num_tmp_widgets] = label;
  setup->num_tmp_widgets++;

  return label;
}

/*
 *
 */
static void numtype_update(xitk_widget_t *w, void *data, int value) {
  cfg_entry_t *entry;
  
  entry = (cfg_entry_t *)data;

  entry->config->update_num(entry->config, entry->key, value );
}

/*
 *
 */
static void stringtype_update(xitk_widget_t *w, void *data, char *str) {
  cfg_entry_t *entry, *check_entry;
  
  entry = (cfg_entry_t *)data;
  
  entry->config->update_string(entry->config, entry->key, str );
  check_entry = gGui->config->lookup_entry(gGui->config, entry->key);

  if(check_entry) {
    if((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)
      xitk_inputtext_change_text(setup->widget_list, w, check_entry->str_value);
  }
}

/*
 *
 */
static widget_triplet_t *setup_add_slider (char *title, char *labelkey, 
					   int x, int y, cfg_entry_t *entry ) {
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
  sl.motion_userdata          = entry;
  xitk_list_append_content(setup->widget_list->l,
			   (slider = xitk_noskin_slider_create(setup->widget_list, &sl,
							       x, y, 150, 16,
							       XITK_HSLIDER)));
  xitk_slider_set_pos(setup->widget_list, slider, entry->num_value);

  ADD_LABEL(slider);

  setup->tmp_widgets[setup->num_tmp_widgets] = slider;
  setup->num_tmp_widgets++;

  wt->widget = slider;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_inputnum(char *title, char *labelkey, 
					    int x, int y, cfg_entry_t *entry) {
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
  ib.userdata          = (void *)entry;
  xitk_list_append_content (setup->widget_list->l,
    (intbox = 
     xitk_noskin_intbox_create(setup->widget_list, &ib, x, y - 5, 60, 20, &wi, &wbu, &wbd)));

  ADD_LABEL(intbox);
  
  setup->tmp_widgets[setup->num_tmp_widgets] = intbox;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = wi;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = wbu;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = wbd;
  setup->num_tmp_widgets++;
  
  wt->widget = intbox;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_inputtext(char *title, char *labelkey, 
					     int x, int y, cfg_entry_t *entry) {
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
  inp.userdata          = entry;
  xitk_list_append_content (setup->widget_list->l,
			   (input = 
			    xitk_noskin_inputtext_create(setup->widget_list, &inp,
							 x, y - 5, 150, 20,
							 "Black", "Black", fontname)));

  ADD_LABEL(input);

  setup->tmp_widgets[setup->num_tmp_widgets] = input;
  setup->num_tmp_widgets++;
  
  wt->widget = input;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_checkbox (char *title, char *labelkey, 
					     int x, int y, cfg_entry_t *entry) {
  xitk_checkbox_widget_t    cb;
  xitk_widget_t            *checkbox;
  static widget_triplet_t  *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  ADD_FRAME(title);

  cb.skin_element_name = NULL;
  cb.callback          = numtype_update;
  cb.userdata          = entry;

  xitk_list_append_content (setup->widget_list->l,
			   (checkbox = 
			    xitk_noskin_checkbox_create(setup->widget_list, &cb,
							x, y, 10, 10)));
  xitk_checkbox_set_state (checkbox, entry->num_value, xitk_window_get_window(setup->xwin),
			   setup->widget_list->gc);
  
  ADD_LABEL(checkbox);

  setup->tmp_widgets[setup->num_tmp_widgets] = checkbox;
  setup->num_tmp_widgets++;

  wt->widget = checkbox;

  return wt;
}

/*
 *
 */
static widget_triplet_t *setup_add_combo (char *title, char *labelkey, 
					  int x, int y, cfg_entry_t *entry ) {
  xitk_combo_widget_t       cmb;
  xitk_widget_t            *combo, *lw, *bw;
  static widget_triplet_t  *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);

  ADD_FRAME(title);

  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = setup->widget_list;
  cmb.entries           = entry->enum_values;
  cmb.parent_wkey       = &setup->kreg;
  cmb.callback          = numtype_update;
  cmb.userdata          = entry;
  xitk_list_append_content(setup->widget_list->l, 
			   (combo = 
			    xitk_noskin_combo_create(setup->widget_list, &cmb,
						     x, y - 4, 150, &lw, &bw)));
  xitk_combo_set_select(setup->widget_list, combo, entry->num_value );

  ADD_LABEL(combo);

  setup->tmp_widgets[setup->num_tmp_widgets] = combo;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = lw;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = bw;
  setup->num_tmp_widgets++;

  wt->widget = combo;

  return wt;
}

/*
 * Add a browser (needed for help files display).
 */
static widget_triplet_t *setup_list_browser(int x, int y, char **content, int len) {
  xitk_browser_widget_t     br;
  xitk_widget_t            *browser;
  static widget_triplet_t  *wt;

  wt = (widget_triplet_t *) xine_xmalloc(sizeof(widget_triplet_t));

  XITK_WIDGET_INIT(&br, gGui->imlib_data);

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = BROWSER_MAX_DISP_ENTRIES;
  br.browser.num_entries           = len;
  br.browser.entries               = content;
  br.callback                      = NULL;
  br.dbl_click_callback            = NULL;
  br.parent_wlist                  = setup->widget_list;
  br.userdata                      = NULL;
  xitk_list_append_content(setup->widget_list->l, 
			   (browser = 
			    xitk_noskin_browser_create(setup->widget_list, &br,
						       setup->widget_list->gc, 
						       x, y,
						       (WINDOW_WIDTH - 50) - 16,
						       20, 16, fontname)));
  
  xitk_browser_set_alignment(browser, LABEL_ALIGN_LEFT);
  xitk_browser_update_list(browser, content, len, 0);

  setup->tmp_widgets[setup->num_tmp_widgets] = browser;
  setup->num_tmp_widgets++;

  wt->widget = browser;
  wt->frame  = NULL;
  wt->label  = NULL;

  return wt;
}

/*
 *
 */
static void setup_section_widgets(int s) {
  int                  x = ((WINDOW_WIDTH>>1) - (FRAME_WIDTH>>1) - 10);
  int                  y = 70;
  cfg_entry_t         *entry;
  int                  len;
  char                *section;
  char                *labelkey;
  int                  slidmax = 1;

  xitk_disable_widget(setup->slider_wg);
  
  /* Selected tab is one of help sections */
  if(s >= (setup->num_sections - 3)) {
    if(s == (setup->num_sections - 3)) {
      setup->wg[setup->num_wg] = setup_list_browser (25, 70, 
						     setup->config_content, setup->config_lines);
    }
    else if(s == (setup->num_sections - 2)) {
      setup->wg[setup->num_wg] = setup_list_browser (25, 70, 
						     setup->readme_content, setup->readme_lines);
    }
    else if(s == (setup->num_sections - 1)) {
      setup->wg[setup->num_wg] = setup_list_browser (25, 70, 
						     setup->faq_content, setup->faq_lines);
    }
    
    DISABLE_ME(setup->wg[setup->num_wg]);
    setup->num_wg++;
  }
  else {
    
    section = setup->sections[s];
    len     = strlen (section);
    entry   = gGui->config->first;
    
    while (entry) {
      
      if (!strncmp (entry->key, section, len) && entry->description) {
	
	labelkey = &entry->key[len+1];
	
	switch (entry->type) {
	  
	case CONFIG_TYPE_RANGE: /* slider */
	  setup->wg[setup->num_wg] = setup_add_slider (entry->description, labelkey, x, y, entry);
	  DISABLE_ME(setup->wg[setup->num_wg]);
	  setup->num_wg++;
	  break;
	  
	case CONFIG_TYPE_STRING:
	  setup->wg[setup->num_wg] = setup_add_inputtext (entry->description, labelkey, x, y, entry);
	  DISABLE_ME(setup->wg[setup->num_wg]);
	  setup->num_wg++;
	  break;
	  
	case CONFIG_TYPE_ENUM:
	  setup->wg[setup->num_wg] = setup_add_combo (entry->description, labelkey, x, y, entry);
	  DISABLE_ME(setup->wg[setup->num_wg]);
	  setup->num_wg++;
	  break;
	  
	case CONFIG_TYPE_NUM:
	  setup->wg[setup->num_wg] = setup_add_inputnum (entry->description, labelkey, x, y, entry);
	  DISABLE_ME(setup->wg[setup->num_wg]);
	  setup->num_wg++;
	  break;
	  
	case CONFIG_TYPE_BOOL:
	  setup->wg[setup->num_wg] = setup_add_checkbox (entry->description, labelkey, x, y, entry);
	  DISABLE_ME(setup->wg[setup->num_wg]);
	  setup->num_wg++;
	  break;
	  
	}
	
      }
      
      entry = entry->next;
    }

    xitk_enable_widget(setup->slider_wg);

    if(setup->num_wg > MAX_DISPLAY_WIDGETS)
      slidmax = setup->num_wg - MAX_DISPLAY_WIDGETS;
    else
      slidmax = setup->num_wg;

    xitk_slider_set_max(setup->slider_wg, slidmax);
    xitk_slider_set_pos(setup->widget_list, setup->slider_wg, slidmax);
  }
}

/*
 *
 */
static void setup_change_section(xitk_widget_t *wx, void *data, int section) {
  int i;

  for (i = 0; i < setup->num_wg; i++ ) {
    free(setup->wg[i]);
  }

  /* remove old widgets */
  for (i=0; i<setup->num_tmp_widgets; i++ ) {
    xitk_widget_t *w;

    w = (xitk_widget_t *) xitk_list_first_content (setup->widget_list->l);

    while (w) {
      
      if (setup->tmp_widgets[i] == w) {
	xitk_destroy_widget(setup->widget_list, setup->tmp_widgets[i]);

	xitk_list_delete_current (setup->widget_list->l);
	
	w = (xitk_widget_t *) setup->widget_list->l->cur;

      } else
	w = (xitk_widget_t *) xitk_list_next_content (setup->widget_list->l);
    }
  }

  setup->num_tmp_widgets = 0;
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
  Pixmap               bg;
  cfg_entry_t         *entry;
  xitk_tabs_widget_t   tab;

  setup->num_sections = 0;
  entry = gGui->config->first;
  while (entry) {

    char *point;
    
    point = strchr(entry->key, '.');
    
    if (point) {
      int found ;
      int i;
      int len;

      len = point - entry->key;
      found = 0;

      for (i=0; i<setup->num_sections; i++) {
	if (!strncmp (setup->sections[i], entry->key, len)) {
	  found = 1;
	  break;
	}
      }

      /* 
       * Only add a tab for non-empty sections (we don't need to handle
       * sinek/gnome-xine/etc... specific entries)
       */
      if (!found) {
	cfg_entry_t *check_entry = gGui->config->first;
	int          num_entries = 0;
	
	while (check_entry) {
	  
	  if(!strncmp(check_entry->key, entry->key, len) && check_entry->description) {
	    switch(check_entry->type) {
	    default:
	      num_entries++;
	      break;
	    }
	  }
	  check_entry = check_entry->next;
	}
	
	if(num_entries) {
	  setup->sections[setup->num_sections] = xine_xmalloc (len + 1);
	  strncpy (setup->sections[setup->num_sections], entry->key, len);
	  setup->sections[setup->num_sections][len] = 0;
	  setup->num_sections++;
	}
      }
    }      
    
    entry = entry->next;
  }

  setup->sections[setup->num_sections] = strdup(_("Help"));
  setup->num_sections++;
  setup->sections[setup->num_sections] = strdup(_("README"));
  setup->num_sections++;
  setup->sections[setup->num_sections] = strdup(_("FAQ"));
  setup->num_sections++;

  XITK_WIDGET_INIT(&tab, gGui->imlib_data);

  tab.skin_element_name = NULL;
  tab.num_entries       = setup->num_sections;
  tab.entries           = setup->sections;
  tab.parent_wlist      = setup->widget_list;
  tab.callback          = setup_change_section;
  tab.userdata          = NULL;
  xitk_list_append_content (setup->widget_list->l,
    (setup->tabs = 
     xitk_noskin_tabs_create(setup->widget_list, &tab, 20, 24, WINDOW_WIDTH - 40, tabsfontname)));
  
  bg = xitk_image_create_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(setup->xwin)), bg,
	    setup->widget_list->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 20, 51, 
			      (WINDOW_WIDTH - 40) - 1, (WINDOW_HEIGHT - (51 + 57)));
  xitk_window_change_background(gGui->imlib_data, setup->xwin, bg, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XFreePixmap(gGui->display, bg);
  XUnlockDisplay(gGui->display);
  
  setup->num_tmp_widgets = 0;
  setup->num_wg = 0;
  setup->first_displayed = 0;

  setup_section_widgets (0);
}

/*
 *
 */
static void setup_end(xitk_widget_t *w, void *data) {
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

/*
 * Read adn store some files.
 */
static char **_setup_read_given(char *given, int *ndest) {
  char            buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char            buffer[256], *ln;
  const langs_t  *l;
  FILE           *fd;
  int             first_try = 1;
  char          **dest = NULL;

  if((given == NULL) || (ndest == NULL))
    return NULL;
  
  *ndest = 0;
  
  memset(&buf, 0, sizeof(buf));
  
  l = get_lang();

 __redo:

  sprintf(buf, "%s/%s%s", XINE_DOCDIR, given, l->ext);

  if((fd = fopen(buf, "r")) != NULL) {
    
    while((ln = fgets(buffer, 255, fd)) != NULL) {
      dest = (char **) realloc(dest, sizeof(char *) * (*ndest + 2));
      
      /* label widget hate empty labels */
      dest[*ndest] = strdup(strlen(ln) ? ln : " ");
      
      /* Remove newline */
      if(dest[*ndest][strlen(dest[*ndest]) - 1] == '\n')
	dest[*ndest][strlen(dest[*ndest]) - 1] = '\0';
      
      (*ndest)++;
    }
    
    dest[*ndest] = NULL;
    
    fclose(fd);
  }

  if(first_try && (dest == NULL)) {
    l = get_default_lang();
    first_try--;
    goto __redo;
  }

  return dest;  
}
static void setup_read_config(void) {
  setup->config_content = _setup_read_given("README.config", &setup->config_lines);
}
static void setup_read_readme(void) {
  setup->readme_content = _setup_read_given("README", &setup->readme_lines);
}
static void setup_read_faq(void) {
  setup->faq_content =  _setup_read_given("FAQ", &setup->faq_lines);
}

/*
 * Create setup panel window
 */
void setup_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  xitk_button_widget_t       b;
  xitk_slider_widget_t       sl;
  int                        x, y;

  /* this shouldn't happen */
  if(setup != NULL) {
    if(setup->xwin)
      return;
  }
  
  setup = (_setup_t *) xine_xmalloc(sizeof(_setup_t));

  x = gGui->config->register_num (gGui->config, "gui.setup_x", 100, NULL, NULL, NULL, NULL);
  y = gGui->config->register_num (gGui->config, "gui.setup_y", 100, NULL, NULL, NULL, NULL);

  /* Create window */
  setup->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						 _("xine setup"), 
						 x, y, WINDOW_WIDTH, WINDOW_HEIGHT);

  /* Read some help files */
  setup_read_config();
  setup_read_readme();
  setup_read_faq();
  
  XLockDisplay (gGui->display);

  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(setup->xwin)), None, None);

  setup->widget_list                = xitk_widget_list_new();
  setup->widget_list->l             = xitk_list_new ();
  setup->widget_list->win           = (xitk_window_get_window(setup->xwin));
  setup->widget_list->gc            = gc;


  XITK_WIDGET_INIT(&b, gGui->imlib_data);
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  {

    sl.min                      = 0;
    sl.max                      = 1;
    sl.step                     = 1;
    sl.skin_element_name        = NULL;
    sl.callback                 = NULL;
    sl.userdata                 = NULL;
    sl.motion_callback          = setup_nextprev_wg;
    sl.motion_userdata          = NULL;
    xitk_list_append_content(setup->widget_list->l,
     (setup->slider_wg = xitk_noskin_slider_create(setup->widget_list, &sl,
						   (WINDOW_WIDTH - 41), 70, 
						   16, (WINDOW_HEIGHT - 140), XITK_VSLIDER)));
    xitk_slider_set_pos(setup->widget_list, setup->slider_wg, 1);
  }

  setup_sections();
  setup_paint_widgets();

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  x = (WINDOW_WIDTH - (3 * 100))>>2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Logs");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = gui_viewlog_show; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(setup->widget_list->l, 
	   xitk_noskin_labelbutton_create(setup->widget_list, &lb, x, WINDOW_HEIGHT - 40, 100, 23,
					  "Black", "Black", "White", tabsfontname));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Key Bindings");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = gui_kbedit_show; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(setup->widget_list->l, 
   xitk_noskin_labelbutton_create(setup->widget_list, &lb, (x * 2) + 100, WINDOW_HEIGHT - 40, 100, 23,
					  "Black", "Black", "White", tabsfontname));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = setup_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(setup->widget_list->l, 
   xitk_noskin_labelbutton_create(setup->widget_list, &lb, (x * 3) + (100 * 2), WINDOW_HEIGHT - 40, 100, 23,
					  "Black", "Black", "White", tabsfontname));
  
  XMapRaised(gGui->display, xitk_window_get_window(setup->xwin));

  XUnlockDisplay (gGui->display);

  setup->kreg = xitk_register_event_handler("setup", 
					   (xitk_window_get_window(setup->xwin)),
					   setup_handle_event,
					   NULL,
					   NULL,
					   setup->widget_list,
					   NULL);


  setup->visible = 1;
  setup->running = 1;

}
