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
#include "xine-toolkit/tabs.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/browser.h"

/*
#define DEBUG_VIEWLOG
*/

#define WINDOW_WIDTH        630
#define WINDOW_HEIGHT       473
#define MAX_DISP_ENTRIES    17
        
typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;
  int                   tabs_height;

  const char          **log;
  int                   log_entries;
  int                   real_num_entries;

  xitk_widget_t        *browser_widget;

  xitk_register_key_t   kreg;

} _viewlog_t;

static _viewlog_t    *viewlog = NULL;

/*
 * Leaving setup panel, release memory.
 */
static void viewlog_exit(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

  if(viewlog) {
    window_info_t wi;
    int           i;
    
    viewlog->running = 0;
    viewlog->visible = 0;
    
    if((xitk_get_window_info(viewlog->kreg, &wi))) {
      config_update_num ("gui.viewlog_x", wi.x);
      config_update_num ("gui.viewlog_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&viewlog->kreg);
    xitk_window_destroy_window (viewlog->xwin);
    viewlog->xwin = NULL;
    /* xitk_dlist_init (&viewlog->widget_list->list); */

    for(i = 0; i < viewlog->log_entries; i++)
      free((char *)viewlog->log[i]);
    free(viewlog->log);

    free(viewlog);
    viewlog = NULL;

    video_window_set_input_focus (gui->vwin);
  }
}

/*
 * return 1 if viewlog window is ON
 */
int viewlog_is_running(void) {

  if(viewlog != NULL)
    return viewlog->running;

  return 0;
}

/*
 * Return 1 if viewlog window is visible
 */
int viewlog_is_visible(void) {
  gGui_t *gui = gGui;

  if(viewlog != NULL) {
    if (gui->use_root_window)
      return xitk_window_is_window_visible (viewlog->xwin);
    else
      return viewlog->visible && xitk_window_is_window_visible (viewlog->xwin);
  }

  return 0;
}

/*
 * Raise viewlog->xwin
 */
void viewlog_raise_window(void) {
  if (viewlog != NULL)
    raise_window(viewlog->xwin, viewlog->visible, viewlog->running);
}
/*
 * Hide/show the viewlog window.
 */
void viewlog_toggle_visibility (xitk_widget_t *w, void *data) {
  if(viewlog != NULL)
    toggle_window(viewlog->xwin, viewlog->widget_list, &viewlog->visible, viewlog->running);
}

/*
 *
 */
static void viewlog_paint_widgets(void) {

  /* Repaint widgets now */
  xitk_paint_widget_list (viewlog->widget_list); 
}

/*
 *
 */
static void viewlog_clear_tab(void) {
  gGui_t *gui = gGui;
  xitk_image_t *im;
  int width = WINDOW_WIDTH - 30;
  int height = MAX_DISP_ENTRIES * 20 + 16 + 10;

  im = xitk_image_create_image (gui->xitk, width, height);

  xitk_image_draw_outter (im, width, height);
  xitk_image_draw_image (viewlog->widget_list, im, 0, 0, width, height, 15, (24 + viewlog->tabs_height));
  xitk_image_free_image (&im);
}

/*
 *
 */
static void viewlog_change_section(xitk_widget_t *wx, void *data, int section) {
#if DEBUG_VIEWLOG
  gGui_t *gui = gGui;
#endif
  int    i, j, k;
  const char *const *log = (const char * const *)xine_get_log(__xineui_global_xine_instance, section);
  char   buf[2048];
  const char *p;
  
  /* Freeing entries */
  for(i = 0; i < viewlog->log_entries; i++)
    free((char *)viewlog->log[i]);
  
  /* Compute log entries */
  viewlog->real_num_entries = j = k = 0;
  
  if(log) {
    
    /* Look for entries number */
    while(log[k] != NULL) k++;

    for(i = 0; i < k; i++) {
      int buflen;

      buf[0] = '\0'; buflen = 0;
      
      p = &log[i][0];
      
      if(strlen(log[i]) > 0) {
	while(*p != '\0') {
	  
	  switch(*p) {
	  
	    /* Ignore */
	  case '\t':
	  case '\a':
	  case '\b':
	  case '\f':
	  case '\r':
	  case '\v':
	    break;
	    
	  case '\n':
	    if(buflen > 0) {
	      viewlog->log = (const char **) realloc(viewlog->log, sizeof(char *) * ((j + 1) + 1));
	      viewlog->log[j++] = strdup(buf);
	      viewlog->real_num_entries++;
	    }
	    buf[0] = '\0'; buflen = 0;
	    break;
	    
	  default:
	    buf[buflen++] = *p; buf[buflen] = '\0';
	    break;
	  }
	  
	  p++;
	}

	/* Remaining chars */
	if(buflen > 0) {
	  viewlog->log = (const char **) realloc(viewlog->log, sizeof(char *) * ((j + 1) + 1));
	  viewlog->log[j++] = strdup(buf);
	}
	
      }
      else {
	/* Empty log entry line */
	viewlog->log = (const char **) realloc(viewlog->log, sizeof(char *) * ((j + 1) + 1));
	viewlog->log[j++] = strdup(" ");
      }
    }
    
  }

  /* I like null terminated arrays ;-) */
  viewlog->log[j]      = NULL;
  viewlog->log_entries = j;
  
#if DEBUG_VIEWLOG
  if ((viewlog->log_entries == 0) || (log == NULL))
    xitk_window_dialog_3 (gui->xitk,
      viewlog->xwin,
      get_layer_above_video (gui), 400, _("log info"), NULL, NULL,
      XITK_LABEL_OK, NULL, NULL, NULL, 0, ALIGN_CENTER,
      _("There is no log entry for logging section '%s'.\n"),
      xitk_tabs_get_current_tab_selected (viewlog->tabs));
#endif

  if(__xineui_global_verbosity) {
    const char   *const *log_sections = xine_get_log_names(__xineui_global_xine_instance);

    printf("\nLOG SECTION [%s]\n", log_sections[section]);
    i = 0;
    while(viewlog->log[i]) {
      if((strlen(viewlog->log[i]) > 1) || (viewlog->log[i][0] != ' '))
	printf(" '%s'\n", viewlog->log[i]);
      i++;
    }
    printf("-----------------------------------\n\n");
  }
  
  xitk_browser_update_list(viewlog->browser_widget, viewlog->log, NULL, viewlog->real_num_entries, 0);
  
  viewlog_clear_tab();
  viewlog_paint_widgets();
}

/*
 * Refresh current displayed log.
 */
static void viewlog_refresh(xitk_widget_t *w, void *data) {
  int section =  xitk_tabs_get_current_selected(viewlog->tabs);
  if (section >= 0) {
    viewlog_change_section(NULL, NULL, section);
  }
}

/* 
 * collect config categories, viewlog tab widget
 */
static void viewlog_create_tabs(void) {
  xitk_pixmap_t       *bg;
  xitk_tabs_widget_t   tab;
  const char   *const *log_sections = xine_get_log_names(__xineui_global_xine_instance);
  unsigned int         log_section_count = xine_get_log_section_count(__xineui_global_xine_instance);
  char                *tab_sections[log_section_count + 1];
  unsigned int         i;

  /* 
   * create log sections
   */
  for(i = 0; i < log_section_count; i++) {
    tab_sections[i] = (char *)log_sections[i];
  }
  tab_sections[i] = NULL;

  XITK_WIDGET_INIT (&tab);
  
  tab.skin_element_name = NULL;
  tab.num_entries       = log_section_count;
  tab.entries           = tab_sections;
  tab.callback          = viewlog_change_section;
  tab.userdata          = NULL;
  viewlog->tabs = xitk_noskin_tabs_create (viewlog->widget_list, &tab, 15, 24, WINDOW_WIDTH - 30, tabsfontname);
  xitk_add_widget (viewlog->widget_list, viewlog->tabs);

  viewlog->tabs_height = xitk_get_widget_height(viewlog->tabs) - 1;

  xitk_enable_and_show_widget(viewlog->tabs);

  bg = xitk_window_get_background_pixmap (viewlog->xwin);

  draw_rectangular_outter_box (bg, 15, (24 + viewlog->tabs_height),
    (WINDOW_WIDTH - 30 - 1), (MAX_DISP_ENTRIES * 20 + 16 + 10 - 1));
  xitk_window_set_background (viewlog->xwin, bg);

  viewlog_change_section(NULL, NULL, 0);
}

/*
 * Leave viewlog window.
 */
void viewlog_end(void) {
  viewlog_exit(NULL, NULL);
}

static void viewlog_handle_key_event(void *data, const xitk_key_event_t *ke) {

  if (ke->event == XITK_KEY_PRESS) {
    if (ke->key_pressed == XK_Escape)
      viewlog_exit(NULL, NULL);
    else
      gui_handle_key_event (gGui, ke);
  }
}

static const xitk_event_cbs_t viewlog_event_cbs = {
  .key_cb = viewlog_handle_key_event,
};

void viewlog_reparent(void) {
  gGui_t *gui = gGui;

  if(viewlog)
    reparent_window(gui, viewlog->xwin);
}

/*
 * Create viewlog window
 */
void viewlog_panel(void) {
  gGui_t *gui = gGui;
  xitk_labelbutton_widget_t  lb;
  xitk_browser_widget_t      br;
  int                        x, y;
  xitk_widget_t             *w;
  
  viewlog = (_viewlog_t *) calloc(1, sizeof(_viewlog_t));
  viewlog->log = (const char **) calloc(1, sizeof(char *));

  x = xine_config_register_num (__xineui_global_xine_instance, "gui.viewlog_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, "gui.viewlog_y", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  viewlog->xwin = xitk_window_create_dialog_window (gui->xitk,
    _("xine Log Viewer"), x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start(gui, viewlog->xwin);

  viewlog->widget_list = xitk_window_widget_list(viewlog->xwin);

  viewlog_create_tabs();

  XITK_WIDGET_INIT (&br);

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = viewlog->log_entries;
  br.browser.entries               = viewlog->log;
  br.callback                      = NULL;
  br.dbl_click_callback            = NULL;
  br.userdata                      = NULL;
  viewlog->browser_widget = xitk_noskin_browser_create (viewlog->widget_list, &br,
    15 + 5, (24 + viewlog->tabs_height) + 5, WINDOW_WIDTH - (30 + 10 + 16), 20, 16, br_fontname);
  xitk_add_widget (viewlog->widget_list, viewlog->browser_widget);

  xitk_enable_and_show_widget(viewlog->browser_widget);

  xitk_browser_set_alignment(viewlog->browser_widget, ALIGN_LEFT);
  xitk_browser_update_list(viewlog->browser_widget, viewlog->log, NULL, viewlog->real_num_entries, 0);
  
  viewlog_paint_widgets();
  
  XITK_WIDGET_INIT (&lb);

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Refresh");
  lb.align             = ALIGN_CENTER;
  lb.callback          = viewlog_refresh; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  w =  xitk_noskin_labelbutton_create (viewlog->widget_list, &lb,
    x, y, 100, 23, "Black", "Black", "White", tabsfontname);
  xitk_add_widget (viewlog->widget_list, w);
  xitk_enable_and_show_widget(w);

  x = WINDOW_WIDTH - (100 + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = viewlog_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  w =  xitk_noskin_labelbutton_create (viewlog->widget_list, &lb,
    x, y, 100, 23, "Black", "Black", "White", tabsfontname);
  xitk_add_widget (viewlog->widget_list, w);
  xitk_enable_and_show_widget(w);

  viewlog->kreg = xitk_window_register_event_handler("viewlog", viewlog->xwin, &viewlog_event_cbs, viewlog);

  viewlog->visible = 1;
  viewlog->running = 1;
  viewlog_raise_window();

  xitk_window_try_to_set_input_focus(viewlog->xwin);
}
