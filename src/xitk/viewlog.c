/* 
 * Copyright (C) 2000-2004 the xine project
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

#include "common.h"

/*
#define DEBUG_VIEWLOG
*/

extern gGui_t              *gGui;

static char                *br_fontname = "-misc-fixed-medium-r-normal-*-10-*-*-*-*-*-*-*";
static char                *tabsfontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

#define WINDOW_WIDTH        580
#define WINDOW_HEIGHT       480
#define MAX_DISP_ENTRIES    17
        
typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;

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

  if(viewlog) {
    window_info_t wi;
    
    viewlog->running = 0;
    viewlog->visible = 0;
    
    if((xitk_get_window_info(viewlog->kreg, &wi))) {
      config_update_num ("gui.viewlog_x", wi.x);
      config_update_num ("gui.viewlog_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&viewlog->kreg);
    
    xitk_destroy_widgets(viewlog->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, viewlog->xwin);
    
    viewlog->xwin = NULL;
    xitk_list_free(XITK_WIDGET_LIST_LIST(viewlog->widget_list));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, XITK_WIDGET_LIST_GC(viewlog->widget_list));
    XUnlockDisplay(gGui->display);
    
    free(viewlog->widget_list);
    
    free(viewlog);
    viewlog = NULL;

    try_to_set_input_focus(gGui->video_window);
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

  if(viewlog != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(viewlog->xwin));
    else
      return viewlog->visible && xitk_is_window_visible(gGui->display, xitk_window_get_window(viewlog->xwin));
  }

  return 0;
}

/*
 * Raise viewlog->xwin
 */
void viewlog_raise_window(void) {
  if(viewlog != NULL)
    raise_window(xitk_window_get_window(viewlog->xwin), viewlog->visible, viewlog->running);
}
/*
 * Hide/show the viewlog window.
 */
void viewlog_toggle_visibility (xitk_widget_t *w, void *data) {
  if(viewlog != NULL)
    toggle_window(xitk_window_get_window(viewlog->xwin), viewlog->widget_list, 
		  &viewlog->visible, viewlog->running);
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
  xitk_image_t *im;
  
  im = xitk_image_create_image(gGui->imlib_data, (WINDOW_WIDTH - 40), 
			       (WINDOW_HEIGHT - (51 + 57) + 1) - 5);
  
  draw_outter(gGui->imlib_data, im->image, im->width, im->height);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, im->image->pixmap, (xitk_window_get_window(viewlog->xwin)),
	    (XITK_WIDGET_LIST_GC(viewlog->widget_list)), 0, 0, im->width, im->height, 20, 51);
  XUnlockDisplay(gGui->display);
  
  xitk_image_free_image(gGui->imlib_data, &im);
}

/*
 *
 */
static void viewlog_change_section(xitk_widget_t *wx, void *data, int section) {
  int    i, j, k;
  const char *const *log = xine_get_log(gGui->xine, section);
  char   buf[2048];
  const char *p;
  
  /* Freeing entries */
  for(i = 0; i <= viewlog->log_entries; i++) {
    free((char *)viewlog->log[i]);
  }
  
  /* Compute log entries */
  viewlog->log_entries = viewlog->real_num_entries = k = 0;
  
  if(log) {
    
    /* Look for entries number */
    while(log[k] != NULL) k++;

    for(i = 0, j = 0; i < k; i++) {

      memset(&buf, 0, sizeof(buf));
      
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
	    if(strlen(buf)) {
	      viewlog->log = (const char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	      viewlog->log[j++] = strdup(buf);
	      viewlog->real_num_entries++;
	    }
	    memset(&buf, 0, sizeof(buf));
	    break;
	    
	  default:
	    sprintf(buf, "%s%c", buf, *p);
	    break;
	  }
	  
	  p++;
	}

	/* Remaining chars */
	if(strlen(buf)) {
	  viewlog->log = (const char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	  viewlog->log[j++] = strdup(buf);
	}
	
      }
      else {
	/* Empty log entry line */
	viewlog->log = (const char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	viewlog->log[j++] = strdup(" ");
      }
    }
    
    /* I like null terminated arrays ;-) */
    viewlog->log[j]      = NULL;
    viewlog->log_entries = j;
    
  }
  
#if DEBUG_VIEWLOG
  if((viewlog->log_entries == 0) || (log == NULL))
    xitk_window_dialog_ok(gGui->imlib_data, _("log info"), 
			  NULL, NULL, ALIGN_CENTER, 
			  _("There is no log entry for logging section '%s'.\n"), 
			  xitk_tabs_get_current_tab_selected(viewlog->tabs));
#endif

  if(gGui->verbosity) {
    const char   *const *log_sections = xine_get_log_names(gGui->xine);

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
  viewlog_change_section(NULL, NULL, xitk_tabs_get_current_selected(viewlog->tabs));
}

/* 
 * collect config categories, viewlog tab widget
 */
static void viewlog_create_tabs(void) {
  xitk_pixmap_t       *bg;
  xitk_tabs_widget_t   tab;
  const char   *const *log_sections = xine_get_log_names(gGui->xine);
  unsigned int         log_section_count = xine_get_log_section_count(gGui->xine);
  char                *tab_sections[log_section_count + 1];
  int                  i;

  /* 
   * create log sections
   */
  for(i = 0; i < log_section_count; i++) {
    tab_sections[i] = (char *)log_sections[i];
  }
  tab_sections[i] = NULL;

  XITK_WIDGET_INIT(&tab, gGui->imlib_data);
  
  tab.skin_element_name = NULL;
  tab.num_entries       = log_section_count;
  tab.entries           = tab_sections;
  tab.parent_wlist      = viewlog->widget_list;
  tab.callback          = viewlog_change_section;
  tab.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(viewlog->widget_list)),
    (viewlog->tabs = 
     xitk_noskin_tabs_create(viewlog->widget_list, &tab, 20, 24, WINDOW_WIDTH - 40, tabsfontname)));
  xitk_enable_and_show_widget(viewlog->tabs);

  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(viewlog->xwin)), bg->pixmap,
	    bg->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 20, 51, 
			      (WINDOW_WIDTH - 40) - 1, (WINDOW_HEIGHT - (51 + 57)) - 5);
  xitk_window_change_background(gGui->imlib_data, viewlog->xwin, bg->pixmap, 
				WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_image_destroy_xitk_pixmap(bg);
  
  viewlog_change_section(NULL, NULL, 0);
}

/*
 * Leave viewlog window.
 */
void viewlog_end(void) {
  viewlog_exit(NULL, NULL);
}

static void viewlog_handle_event(XEvent *event, void *data) {
  switch(event->type) {
    
  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      viewlog_exit(NULL, NULL);
    break;
    
  }
}

void viewlog_reparent(void) {
  if(viewlog)
    reparent_window((xitk_window_get_window(viewlog->xwin)));
}

/*
 * Create viewlog window
 */
void viewlog_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  xitk_browser_widget_t      br;
  int                        x, y;
  xitk_widget_t             *w;
  
  viewlog = (_viewlog_t *) xine_xmalloc(sizeof(_viewlog_t));
  viewlog->log = (const char **) xine_xmalloc(sizeof(char **));

  x = xine_config_register_num (gGui->xine, "gui.viewlog_x", 
				100, 
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.viewlog_y", 
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  viewlog->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						   _("xine log viewer"), 
						   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start((xitk_window_get_window(viewlog->xwin)));

  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(viewlog->xwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  viewlog->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(viewlog->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(viewlog->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(viewlog->xwin)));
  xitk_widget_list_set(viewlog->widget_list, WIDGET_LIST_GC, gc);

  viewlog_create_tabs();

  XITK_WIDGET_INIT(&br, gGui->imlib_data);

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = viewlog->log_entries;
  br.browser.entries               = viewlog->log;
  br.callback                      = NULL;
  br.dbl_click_callback            = NULL;
  br.parent_wlist                  = viewlog->widget_list;
  br.userdata                      = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(viewlog->widget_list)), 
   (viewlog->browser_widget = 
    xitk_noskin_browser_create(viewlog->widget_list, &br,
			       (XITK_WIDGET_LIST_GC(viewlog->widget_list)), 25, 58, 
			       WINDOW_WIDTH - (50 + 16), 20,
			       16, br_fontname)));
  xitk_enable_and_show_widget(viewlog->browser_widget);

  xitk_browser_set_alignment(viewlog->browser_widget, ALIGN_LEFT);
  xitk_browser_update_list(viewlog->browser_widget, viewlog->log, NULL, viewlog->real_num_entries, 0);
  
  viewlog_paint_widgets();
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  x = ((WINDOW_WIDTH / 2) - 100) / 2;
  y = WINDOW_HEIGHT - 40;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Refresh");
  lb.align             = ALIGN_CENTER;
  lb.callback          = viewlog_refresh; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(viewlog->widget_list)), 
   (w = xitk_noskin_labelbutton_create(viewlog->widget_list, &lb,
				       x, y, 100, 23,
				       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(w);

  x += (WINDOW_WIDTH / 2);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = viewlog_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(viewlog->widget_list)), 
   (w = xitk_noskin_labelbutton_create(viewlog->widget_list, &lb,
				       x, y, 100, 23,
				       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(w);

  viewlog->kreg = xitk_register_event_handler("viewlog", 
					      (xitk_window_get_window(viewlog->xwin)),
					      viewlog_handle_event,
					      NULL,
					      NULL,
					      viewlog->widget_list,
					      NULL);

  viewlog->visible = 1;
  viewlog->running = 1;
  viewlog_raise_window();
  
  try_to_set_input_focus(xitk_window_get_window(viewlog->xwin));
}
