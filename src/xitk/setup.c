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

static char            *fontname = "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*";

typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *browser;
  char                 *sections[20];
  int                   num_sections;

  xitk_widget_t        *tmp_widgets[200];
  int                   num_tmp_widgets;

  xitk_register_key_t   kreg;

} _setup_t;

static _setup_t    *setup = NULL;

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

  case KeyPress:
    gui_handle_event(event, data);
    break;
    
  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;

  case ConfigureNotify:
    /*  xitk_combo_update_pos(setup->combo); */
    break;

  }
}

static void setup_add_label (int x, int y, int w, char *str) {

  xitk_label_widget_t lb;
  xitk_widget_t      *label;

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  lb.window              = xitk_window_get_window(setup->xwin);
  lb.gc                  = setup->widget_list->gc;
  lb.skin_element_name   = NULL;
  lb.label               = str;

  xitk_list_append_content(setup->widget_list->l, 
			   (label = xitk_noskin_label_create(&lb,
							     x, y, w, fontname)));

  setup->tmp_widgets[setup->num_tmp_widgets] = label;
  setup->num_tmp_widgets++;
}

static void setup_add_slider (int x, int y, int min, int max, int pos) {

  xitk_slider_widget_t  sl;
  xitk_widget_t        *slider;

  XITK_WIDGET_INIT(&sl, gGui->imlib_data);

  sl.min                      = min;
  sl.max                      = max;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  /* sl.motion_callback          = move_sliders; */
  sl.motion_callback          = NULL;
  sl.motion_userdata          = NULL;
  xitk_list_append_content(setup->widget_list->l,
			   (slider = xitk_noskin_slider_create(&sl,
							       x, y, 117, 16,
							       XITK_HSLIDER)));
  xitk_slider_set_pos(setup->widget_list, slider, pos);

  setup->tmp_widgets[setup->num_tmp_widgets] = slider;
  setup->num_tmp_widgets++;
}

static void setup_add_inputtext(int x, int y, char *str) {

  xitk_inputtext_widget_t  inp;
  xitk_widget_t           *input;

  XITK_WIDGET_INIT(&inp, gGui->imlib_data);

  inp.skin_element_name = NULL;
  inp.text              = str;
  inp.max_length        = 256;
  /* inp.callback          = change_browser_entry; */
  inp.callback          = NULL;
  inp.userdata          = NULL;
  xitk_list_append_content (setup->widget_list->l,
			   (input = 
			    xitk_noskin_inputtext_create(&inp,
							 x, y, 150, 20,
							 "Black", "Black", fontname)));
  setup->tmp_widgets[setup->num_tmp_widgets] = input;
  setup->num_tmp_widgets++;
}

static void setup_add_checkbox (int x, int y, int state) {

  xitk_checkbox_widget_t   cb;
  xitk_widget_t           *checkbox;

  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  cb.skin_element_name = NULL;
  /* cb.callback          = change_browser_entry; */
  cb.callback          = NULL;
  cb.userdata          = NULL;

  xitk_list_append_content (setup->widget_list->l,
			   (checkbox = 
			    xitk_noskin_checkbox_create(&cb,
							x, y, 10, 10)));
  xitk_checkbox_set_state (checkbox, state, xitk_window_get_window(setup->xwin),
			   setup->widget_list->gc);

  setup->tmp_widgets[setup->num_tmp_widgets] = checkbox;
  setup->num_tmp_widgets++;
}

static void setup_add_combo (int x, int y, int state, char **choices) {

  xitk_combo_widget_t      cmb;
  xitk_widget_t           *combo;

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);

  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = setup->widget_list;
  cmb.entries           = choices;
  cmb.parent_wkey       = &setup->kreg;
  /* cmb.callback          = combo_select; */
  cmb.callback          = NULL;
  cmb.userdata          = NULL;
  xitk_list_append_content(setup->widget_list->l, 
			   (combo = 
			    xitk_noskin_combo_create(&cmb,
						     x, y, 150)));

  setup->tmp_widgets[setup->num_tmp_widgets] = combo;
  setup->num_tmp_widgets++;
}


static void setup_section_widgets (int s) {

  int                  y = 40;
  cfg_entry_t         *entry;
  int                  len;
  char                *section;

  section = setup->sections[s];
  len     = strlen (section);
  entry   = gGui->config->first;

  while (entry) {

    if (!strncmp (entry->key, section, len) && entry->description) {
    
      printf ("setup: generating labels for %s\n", entry->key);
      printf ("setup: generating labels for %s\n", entry->description);

      setup_add_label (150, y+4, 130, &entry->key[len+1]);
      setup_add_label (150, y+24, 400, entry->description);

      switch (entry->type) {

      case CONFIG_TYPE_RANGE: /* slider */
	setup_add_slider (280, y, entry->range_min, entry->range_max, entry->num_value);
	break;

      case CONFIG_TYPE_STRING:
	setup_add_inputtext (280, y, entry->str_value);
	break;
	
      case CONFIG_TYPE_ENUM:
	setup_add_combo (280, y, entry->num_value, entry->enum_values);
	break;
	
      case CONFIG_TYPE_NUM:
	break;

      case CONFIG_TYPE_BOOL:
	setup_add_checkbox (280, y, entry->num_value);
	break;

      }

      y += 50;

    }

    entry = entry->next;
  } 

}

static void setup_change_section(xitk_widget_t *w, void *data) {
  int section = (int)data;
  int i;

  /* remove old widgets */
  /* FIXME: free memory / resources */

  for (i=0; i<setup->num_tmp_widgets; i++ ) {

    xitk_widget_t *w;

    w = (xitk_widget_t *) xitk_list_first_content (setup->widget_list->l);

    while (w) {
      
      if (setup->tmp_widgets[i] == w) {
	xitk_list_delete_current (setup->widget_list->l);
	
	w = (xitk_widget_t *) setup->widget_list->l->cur;

      } else
	w = (xitk_widget_t *) xitk_list_next_content (setup->widget_list->l);
    }
  }

  setup->num_tmp_widgets = 0;

  
  setup_section_widgets (section);

  XClearWindow (gGui->display,xitk_window_get_window(setup->xwin) );
  xitk_paint_widget_list (setup->widget_list); 
}


/* 
 * collect config categories, setup browser widget
 */

static void setup_sections () {

  cfg_entry_t *entry;
  xitk_browser_widget_t      browser;

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



  XITK_WIDGET_INIT(&browser, gGui->imlib_data);

  browser.arrow_up.skin_element_name    = NULL;
  browser.slider.skin_element_name      = NULL;
  browser.arrow_dn.skin_element_name    = NULL;
  browser.browser.skin_element_name     = NULL;
  browser.browser.max_displayed_entries = 8;
  browser.browser.num_entries           = setup->num_sections;
  browser.browser.entries               = setup->sections;
  /*
  browser.callback                      = change_inputtext;
  browser.dbl_click_callback            = change_inputtext_dbl_click;
  */
  browser.callback                      = setup_change_section;
  browser.dbl_click_callback            = NULL;
  browser.dbl_click_time                = DEFAULT_DBL_CLICK_TIME;
  browser.parent_wlist                  = setup->widget_list;
  browser.userdata                      = NULL;
  xitk_list_append_content (setup->widget_list->l, 
			    (setup->browser = 
			     xitk_noskin_browser_create(&browser,
							setup->widget_list->gc, 20, 30, 
							100, 20, 12, fontname)));
  
  xitk_browser_update_list(setup->browser, setup->sections, setup->num_sections, 0);

  xitk_browser_set_select(setup->browser, 0);

  setup->num_tmp_widgets = 0;
  setup_section_widgets (0);
}

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
						 "xine setup", 
						 100, 100, 
						 640, 600);
  
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
  lb.label             = "Close";
  lb.callback          = setup_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(setup->widget_list->l, 
	   xitk_noskin_labelbutton_create(&lb,
					  (640 / 2) - 50, 560,
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

