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

#include <xine.h>
#include <xine/xineutils.h>

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "actions.h"
#include "skins.h"

extern gGui_t          *gGui;

static char            *fontname = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 600

#define FRAME_WIDTH   350
#define FRAME_HEIGHT  40

#define PLACE_LABEL(widget) {                                                            \
    int           wx, wy, wh, fh;                                                        \
    xitk_font_t  *fs;                                                                    \
                                                                                         \
    fs = xitk_font_load_font(gGui->display, fontname);                                   \
    xitk_font_set_font(fs, setup->widget_list->gc);                                      \
    fh = xitk_font_get_string_height(fs, labelkey);                                      \
    xitk_font_unload_font(fs);                                                           \
                                                                                         \
    xitk_get_widget_pos(widget, &wx, &wy);                                               \
    wx += xitk_get_widget_width(widget);                                                 \
    wh = xitk_get_widget_height(widget);                                                 \
                                                                                         \
    setup_add_label (wx + 20, (wy + (wh >> 1)) - (fh>>1), (FRAME_WIDTH - wx), labelkey); \
  } 

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

  xitk_register_key_t   kreg;

} _setup_t;

static _setup_t    *setup = NULL;

/*
 *
 */
static void setup_clear_tab(void) {
  xitk_image_t *im;

  im = xitk_image_create_image(gGui->imlib_data, (WINDOW_WIDTH - 40), (WINDOW_HEIGHT - 95)+1);
  draw_outter(gGui->imlib_data, im->image, im->width, im->height);
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, im->image, (xitk_window_get_window(setup->xwin)),
	    setup->widget_list->gc, 0, 0, im->width, im->height, 20, 51);
  XUnlockDisplay(gGui->display);
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
 * Leaving setup panel, release memory.
 */
void setup_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;

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
	  XMapRaised(gGui->display, xitk_window_get_window(setup->xwin));
	  setup->visible = 1;
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(setup->xwin), gGui->video_window);
	  layer_above_video(xitk_window_get_window(setup->xwin));
	}
      } else {
	XUnmapWindow (gGui->display, xitk_window_get_window(setup->xwin));
	setup->visible = 0;
      }
    }
  }
}
/*
 * Hide/show the setup panel
 */
void setup_toggle_panel_visibility (xitk_widget_t *w, void *data) {
  
  if(setup != NULL) {
    if (setup->visible && setup->running) {
      setup->visible = 0;
      xitk_hide_widgets(setup->widget_list);
      XUnmapWindow (gGui->display, xitk_window_get_window(setup->xwin));
    } else {
      if(setup->running) {
	setup->visible = 1;
	xitk_show_widgets(setup->widget_list);
	XMapRaised(gGui->display, xitk_window_get_window(setup->xwin)); 
	XSetTransientForHint (gGui->display, 
			      xitk_window_get_window(setup->xwin), gGui->video_window);
	layer_above_video(xitk_window_get_window(setup->xwin));
      }
    }
  }
}

/*
 * Handle X events here.
 */
void setup_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case KeyPress: {
    xitk_widget_t *w = xitk_get_focused_widget(setup->widget_list);

    if(w && (w->widget_type & WIDGET_TYPE_INPUTTEXT)) {
      xitk_send_key_event(setup->widget_list, w, event);
    }
    else
      gui_handle_event(event, data);
  }
  break;
    
  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case ConfigureNotify: {
    xitk_widget_t *w = (xitk_widget_t *) xitk_list_first_content (setup->widget_list->l);
    
    while (w) {
      xitk_combo_update_pos(w);
      w = (xitk_widget_t *) xitk_list_next_content (setup->widget_list->l);
    }
  }
  break;

  }
}

/*
 *
 */
static void setup_add_label (int x, int y, int w, char *str) {

  xitk_label_widget_t lb;
  xitk_widget_t      *label;
  xitk_font_t        *fs;
  int                 height;

  fs = xitk_font_load_font(gGui->display, fontname);
  xitk_font_set_font(fs, setup->widget_list->gc);
  height = xitk_font_get_string_height(fs, str);
  xitk_font_unload_font(fs);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.window              = xitk_window_get_window(setup->xwin);
  lb.gc                  = setup->widget_list->gc;
  lb.skin_element_name   = NULL;
  lb.label               = str;

  xitk_list_append_content(setup->widget_list->l, 
			   (label = xitk_noskin_label_create(&lb,
							     x, y, w, height, fontname)));

  setup->tmp_widgets[setup->num_tmp_widgets] = label;
  setup->num_tmp_widgets++;
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
  cfg_entry_t *entry;
  
  entry = (cfg_entry_t *)data;
 
  entry->config->update_string(entry->config, entry->key, str );
}


/*
 *
 */
static void setup_add_slider (char *labelkey, int x, int y, cfg_entry_t *entry ) {

  xitk_slider_widget_t  sl;
  xitk_widget_t        *slider;

  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  sl.min                      = entry->range_min;
  sl.max                      = entry->range_max;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = numtype_update;
  sl.motion_userdata          = entry;
  xitk_list_append_content(setup->widget_list->l,
			   (slider = xitk_noskin_slider_create(&sl,
							       x, y, 150, 16,
							       XITK_HSLIDER)));
  xitk_slider_set_pos(setup->widget_list, slider, entry->num_value);

  PLACE_LABEL(slider);

  setup->tmp_widgets[setup->num_tmp_widgets] = slider;
  setup->num_tmp_widgets++;
}

/*
 *
 */
static void setup_add_inputtext(char *labelkey, int x, int y, cfg_entry_t *entry) {

  xitk_inputtext_widget_t  inp;
  xitk_widget_t           *input;

  XITK_WIDGET_INIT(&inp, gGui->imlib_data);

  inp.skin_element_name = NULL;
  inp.text              = entry->str_value;
  inp.max_length        = 256;
  inp.callback          = stringtype_update;
  inp.userdata          = entry;
  xitk_list_append_content (setup->widget_list->l,
			   (input = 
			    xitk_noskin_inputtext_create(&inp,
							 x, y - 5, 150, 20,
							 "Black", "Black", fontname)));

  PLACE_LABEL(input);

  setup->tmp_widgets[setup->num_tmp_widgets] = input;
  setup->num_tmp_widgets++;
}

/*
 *
 */
static void setup_add_checkbox (char *labelkey, int x, int y, cfg_entry_t *entry) {

  xitk_checkbox_widget_t   cb;
  xitk_widget_t           *checkbox;

  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  cb.skin_element_name = NULL;
  cb.callback          = numtype_update;
  cb.userdata          = entry;

  xitk_list_append_content (setup->widget_list->l,
			   (checkbox = 
			    xitk_noskin_checkbox_create(&cb,
							x, y, 10, 10)));
  xitk_checkbox_set_state (checkbox, entry->num_value, xitk_window_get_window(setup->xwin),
			   setup->widget_list->gc);
  
  PLACE_LABEL(checkbox);

  setup->tmp_widgets[setup->num_tmp_widgets] = checkbox;
  setup->num_tmp_widgets++;
}

/*
 *
 */
static void setup_add_combo (char *labelkey, int x, int y, cfg_entry_t *entry ) {

  xitk_combo_widget_t      cmb;
  xitk_widget_t           *combo, *lw, *bw;

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);

  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = setup->widget_list;
  cmb.entries           = entry->enum_values;
  cmb.parent_wkey       = &setup->kreg;
  cmb.callback          = numtype_update;
  cmb.userdata          = entry;
  xitk_list_append_content(setup->widget_list->l, 
			   (combo = 
			    xitk_noskin_combo_create(&cmb,
						     x, y, 150, &lw, &bw)));
  xitk_combo_set_select(setup->widget_list, combo, entry->num_value );

  PLACE_LABEL(combo);

  setup->tmp_widgets[setup->num_tmp_widgets] = lw;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = bw;
  setup->num_tmp_widgets++;
  setup->tmp_widgets[setup->num_tmp_widgets] = combo;
  setup->num_tmp_widgets++;
}

/*
 *
 */
static void setup_section_widgets (int s) {

  int                  x = 40;
  int                  y = 70;
  cfg_entry_t         *entry;
  int                  len;
  char                *section;
  char                *labelkey;

  section = setup->sections[s];
  len     = strlen (section);
  entry   = gGui->config->first;
  
  while (entry) {

    if (!strncmp (entry->key, section, len) && entry->description) {
      
      /* Made frames */
      {
	xitk_image_t        *image;
	xitk_image_widget_t  im;
	int                  lbearing, rbearing, width, ascent, descent;
	char                *fontname = "*-lucida-*-r-*-*-10-*-*-*-*-*-*-*";
	xitk_font_t         *fs;

	image = xitk_image_create_image(gGui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);
	xitk_image_add_mask(gGui->imlib_data, image);

	fs = xitk_font_load_font(gGui->display, fontname);
	xitk_font_set_font(fs, setup->widget_list->gc);
	xitk_font_text_extent(fs, entry->description, strlen(entry->description), 
			      &lbearing, &rbearing, &width, &ascent, &descent);
	xitk_font_unload_font(fs);

	XLockDisplay(gGui->display);

	XSetForeground(gGui->display, setup->widget_list->gc, 
		       xitk_get_pixel_color_gray(gGui->imlib_data));
	XFillRectangle(gGui->display, image->image, setup->widget_list->gc,
		       0, 0, image->width, image->height);
	XUnlockDisplay(gGui->display);

	draw_inner_frame(gGui->imlib_data, image->image, 
			 entry->description, fontname, 
			 0, (ascent+descent), 
			 FRAME_WIDTH, FRAME_HEIGHT);
	
	XITK_WIDGET_INIT(&im, gGui->imlib_data);
	im.skin_element_name = NULL;

	{
	  xitk_widget_t *frame;

	  xitk_list_append_content (setup->widget_list->l,
				    (frame = xitk_noskin_image_create(&im, image, x, y)));
	  
	  setup->tmp_widgets[setup->num_tmp_widgets] = frame;
	  setup->num_tmp_widgets++;
	  
	}
      }	
      
      y += FRAME_HEIGHT >> 1;

      labelkey = &entry->key[len+1];

      switch (entry->type) {

      case CONFIG_TYPE_RANGE: /* slider */
	setup_add_slider (labelkey, x + 10, y, entry);
	break;
	
      case CONFIG_TYPE_STRING:
	setup_add_inputtext (labelkey, x + 10, y, entry);
	break;
	
      case CONFIG_TYPE_ENUM:
	setup_add_combo (labelkey, x + 10, y, entry);
	break;
	
      case CONFIG_TYPE_NUM:
	printf("%s is CONFIG_TYPE_NUM\n", labelkey);
	break;

      case CONFIG_TYPE_BOOL:
	setup_add_checkbox (labelkey, x + 10, y, entry);
	break;

      }
      
      y += (FRAME_HEIGHT >> 1) + 2;
      
    }
    
    entry = entry->next;
  } 

}

/*
 *
 */
static void setup_change_section(xitk_widget_t *wx, void *data, int section) {
  int i;

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
  
  setup_section_widgets (section);
  
  setup_clear_tab();
  xitk_paint_widget_list (setup->widget_list); 
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
      
      if (!found) {

	setup->sections[setup->num_sections] = xine_xmalloc (len+1);
	strncpy (setup->sections[setup->num_sections], entry->key, len);
	setup->sections[setup->num_sections][len] = 0;
	setup->num_sections++;
      }
    }      

    entry = entry->next;
  }


  XITK_WIDGET_INIT(&tab, gGui->imlib_data);

  tab.skin_element_name = NULL;
  tab.num_entries       = setup->num_sections;
  tab.entries           = setup->sections;
  tab.parent_wlist      = setup->widget_list;
  tab.callback          = setup_change_section;
  tab.userdata          = NULL;
  xitk_list_append_content (setup->widget_list->l,
			    (setup->tabs = 
			     xitk_noskin_tabs_create(&tab, 20, 25, WINDOW_WIDTH - 40)));

  bg = xitk_image_create_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(setup->xwin)), bg,
	    setup->widget_list->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 20, 51, 
			      (WINDOW_WIDTH - 40) - 1, (WINDOW_HEIGHT - 95));
  xitk_window_change_background(gGui->imlib_data, setup->xwin, bg, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XFreePixmap(gGui->display, bg);
  XUnlockDisplay(gGui->display);
  
  setup->num_tmp_widgets = 0;
  setup_section_widgets (0);
}

/*
 *
 */
static void setup_end(xitk_widget_t *w, void *data) {
  setup_exit(NULL, NULL);
}

/*
 * Create setup panel window
 */
void setup_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;

  /* this shouldn't happen */
  if(setup != NULL) {
    if(setup->xwin)
      return;
  }
  
  setup = (_setup_t *) xine_xmalloc(sizeof(_setup_t));

  /* Create window */
  setup->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						 _("xine setup"), 
						 100, 100, 
						 WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);

  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(setup->xwin)), None, None);

  setup->widget_list                = xitk_widget_list_new();
  setup->widget_list->l             = xitk_list_new ();
  setup->widget_list->focusedWidget = NULL;
  setup->widget_list->pressedWidget = NULL;
  setup->widget_list->win           = (xitk_window_get_window(setup->xwin));
  setup->widget_list->gc            = gc;

  setup_sections ();


  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.callback          = setup_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(setup->widget_list->l, 
	   xitk_noskin_labelbutton_create(&lb,
					  (WINDOW_WIDTH>>1) - 50, WINDOW_HEIGHT - 40,
					  100, 23,
					  "Black", "Black", "White", fontname));

  
  setup->kreg = xitk_register_event_handler("setup", 
					   (xitk_window_get_window(setup->xwin)),
					   setup_handle_event,
					   NULL,
					   NULL,
					   setup->widget_list,
					   NULL);

  XUnlockDisplay (gGui->display);

  XMapRaised(gGui->display, xitk_window_get_window(setup->xwin));

  setup->visible = 1;
  setup->running = 1;

  XUnlockDisplay (gGui->display);
}

