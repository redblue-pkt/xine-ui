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

#include "xitk.h"

#include "Imlib-light/Imlib.h"
#include "event.h"
#include "actions.h"
#include "skins.h"
#include "lang.h"

/*
#define DEBUG_VIEWLOG
*/

extern gGui_t              *gGui;

//static char                *br_fontname = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char                *br_fontname = "-misc-fixed-medium-r-normal-*-10-*-*-*-*-*-*-*";

#define WINDOW_WIDTH        580
#define WINDOW_HEIGHT       480
#define MAX_DISP_ENTRIES    18
        
typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;

  char                **log;
  int                   log_entries;
  
  xitk_widget_t        *browser_widget;

  xitk_register_key_t   kreg;

} _viewlog_t;

static _viewlog_t    *viewlog = NULL;

static void viewlog_end(xitk_widget_t *, void *);

/*
 * Leaving setup panel, release memory.
 */
void viewlog_exit(xitk_widget_t *w, void *data) {
  window_info_t wi;
  
  viewlog->running = 0;
  viewlog->visible = 0;

  if((xitk_get_window_info(viewlog->kreg, &wi))) {
    gGui->config->update_num (gGui->config, "gui.viewlog_x", wi.x);
    gGui->config->update_num (gGui->config, "gui.viewlog_y", wi.y);
    WINDOW_INFO_ZERO(&wi);
  }

  xitk_unregister_event_handler(&viewlog->kreg);

  XLockDisplay(gGui->display);
  XUnmapWindow(gGui->display, xitk_window_get_window(viewlog->xwin));
  XUnlockDisplay(gGui->display);

  xitk_destroy_widgets(viewlog->widget_list);

  XLockDisplay(gGui->display);
  XDestroyWindow(gGui->display, xitk_window_get_window(viewlog->xwin));
  XUnlockDisplay(gGui->display);

  viewlog->xwin = None;
  xitk_list_free(viewlog->widget_list->l);
  
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, viewlog->widget_list->gc);
  XUnlockDisplay(gGui->display);

  free(viewlog->widget_list);
  
  free(viewlog);
  viewlog = NULL;

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

  if(viewlog != NULL)
    return viewlog->visible;

  return 0;
}

/*
 * Raise viewlog->xwin
 */
void viewlog_raise_window(void) {
  
  if(viewlog != NULL) {
    if(viewlog->xwin) {
      if(viewlog->visible && viewlog->running) {
	if(viewlog->running) {
	  XLockDisplay(gGui->display);
	  XMapRaised(gGui->display, xitk_window_get_window(viewlog->xwin));
	  viewlog->visible = 1;
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(viewlog->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(viewlog->xwin));
	}
      } else {
	XLockDisplay(gGui->display);
	XUnmapWindow (gGui->display, xitk_window_get_window(viewlog->xwin));
	XUnlockDisplay(gGui->display);
	viewlog->visible = 0;
      }
    }
  }
}
/*
 * Hide/show the viewlog window.
 */
void viewlog_toggle_visibility (xitk_widget_t *w, void *data) {
  
  if(viewlog != NULL) {
    if (viewlog->visible && viewlog->running) {
      viewlog->visible = 0;
      xitk_hide_widgets(viewlog->widget_list);
      XLockDisplay(gGui->display);
      XUnmapWindow (gGui->display, xitk_window_get_window(viewlog->xwin));
      XUnlockDisplay(gGui->display);
    } else {
      if(viewlog->running) {
	viewlog->visible = 1;
	xitk_show_widgets(viewlog->widget_list);
	XLockDisplay(gGui->display);
	XMapRaised(gGui->display, xitk_window_get_window(viewlog->xwin)); 
	XSetTransientForHint (gGui->display, 
			      xitk_window_get_window(viewlog->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(viewlog->xwin));
      }
    }
  }
}

/*
 * Handle X events here.
 */
void viewlog_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case KeyPress: {
    XKeyEvent      mykeyevent;
    KeySym         mykey;
    char           kbuf[256];
    int            len;
    
    mykeyevent = event->xkey;
    
    XLockDisplay(gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay(gGui->display);
    
    switch (mykey) {
      
    case XK_Return:
      viewlog_end(NULL, NULL);
      break;
      
    default:
      gui_handle_event(event, data);
      break;
    }
  }
  break;
  
  case MappingNotify:
    XLockDisplay(gGui->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(gGui->display);
    break;
    
  }
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
			       (WINDOW_HEIGHT - (51 + 57) + 1));
  
  draw_outter(gGui->imlib_data, im->image, im->width, im->height);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, im->image, (xitk_window_get_window(viewlog->xwin)),
	    viewlog->widget_list->gc, 0, 0, im->width, im->height, 20, 51);
  XUnlockDisplay(gGui->display);
  
  xitk_image_free_image(gGui->imlib_data, &im);
}

/*
 *
 */
static void viewlog_change_section(xitk_widget_t *wx, void *data, int section) {
  int    i, j, k;
  char **log = xine_get_log(gGui->xine, section);
  char   buf[2048], *p;
  
  /* Freeing entries */
  for(i = 0; i <= viewlog->log_entries; i++) {
    free(viewlog->log[i]);
  }
  
  /* Compute log entries */
  viewlog->log_entries = k = 0;
  
  if(log) {
    
    /* Look for entries number */
    while(log[k] != NULL) k++;

    for(i = 0, j = 0; i < k; i++) {

      memset(&buf, 0, sizeof(buf));
      
      //      printf("handling line: %d '%s'\n", i, log[i]);
      
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
	      viewlog->log = (char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	      viewlog->log[j++] = strdup(buf);
	      //	      printf("added line '%s'\n", viewlog->log[j-1]);
	    }
	    memset(&buf, 0, sizeof(buf));
	    break;
	    
	  default:
	    //	    printf("- %c", *p);
	    sprintf(buf, "%s%c", buf, *p);
	    break;
	  }
	  
	  p++;
	}

	/* Remaining chars */
	if(strlen(buf)) {
	  viewlog->log = (char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	  viewlog->log[j++] = strdup(buf);
	}
	
      }
      else {
	/* Empty log entry line */
	viewlog->log = (char **) realloc(viewlog->log, sizeof(char **) * ((j + 1) + 1));
	viewlog->log[j++] = strdup(" ");
      }
    }
    
    /* I like null terminated arrays ;-) */
    //    printf("viewlog->log[%d] = NULL\n", j);
    viewlog->log[j]      = NULL;
    viewlog->log_entries = j;
    
    /*
    for(i = 0; i < j; i++)
      printf("line %d '%s'\n", i, viewlog->log[i]);
    */
  }
  
#if DEBUG_VIEWLOG
  if((viewlog->log_entries == 0) || (log == NULL))
    xitk_window_dialog_ok(gGui->imlib_data, _("log info"), 
			  NULL, NULL, ALIGN_CENTER, 
			  _("There is no log entry for logging section '%s'.\n"), 
			  xitk_tabs_get_current_tab_selected(viewlog->tabs));
#endif
  
  xitk_browser_update_list(viewlog->browser_widget, viewlog->log, viewlog->log_entries, 0);
  
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
  Pixmap               bg;
  xitk_tabs_widget_t   tab;
  char               **log_sections = xine_get_log_names(gGui->xine);
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
  xitk_list_append_content (viewlog->widget_list->l,
			    (viewlog->tabs = 
			     xitk_noskin_tabs_create(&tab, 20, 25, WINDOW_WIDTH - 40)));

  bg = xitk_image_create_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(viewlog->xwin)), bg,
	    viewlog->widget_list->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 20, 51, 
			      (WINDOW_WIDTH - 40) - 1, (WINDOW_HEIGHT - (51 + 57)));
  xitk_window_change_background(gGui->imlib_data, viewlog->xwin, bg, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XFreePixmap(gGui->display, bg);
  XUnlockDisplay(gGui->display);

  viewlog_change_section(NULL, NULL, 0);
}

/*
 * Leave viewlog window.
 */
static void viewlog_end(xitk_widget_t *w, void *data) {
  viewlog_exit(NULL, NULL);
}


/*
 * Create viewlog window
 */
void viewlog_window(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  xitk_browser_widget_t      br;
  char                      *fontname = "*-lucida-*-r-*-*-10-*-*-*-*-*-*-*";
  int                        x, y;
  
  /* this shouldn't happen */
  if(viewlog != NULL) {
    if(viewlog->xwin)
      return;
  }
  
  viewlog = (_viewlog_t *) xine_xmalloc(sizeof(_viewlog_t));
  viewlog->log = (char **) xine_xmalloc(sizeof(char **));

  x = gGui->config->register_num (gGui->config, "gui.viewlog_x", 100, NULL, NULL, NULL, NULL);
  y = gGui->config->register_num (gGui->config, "gui.viewlog_y", 100, NULL, NULL, NULL, NULL);

  /* Create window */
  viewlog->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						   _("xine log viewer"), 
						   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);

  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(viewlog->xwin)), None, None);

  viewlog->widget_list                = xitk_widget_list_new();
  viewlog->widget_list->l             = xitk_list_new ();
  viewlog->widget_list->focusedWidget = NULL;
  viewlog->widget_list->pressedWidget = NULL;
  viewlog->widget_list->win           = (xitk_window_get_window(viewlog->xwin));
  viewlog->widget_list->gc            = gc;


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
  br.dbl_click_time                = DEFAULT_DBL_CLICK_TIME;
  br.parent_wlist                  = viewlog->widget_list;
  br.userdata                      = NULL;
  xitk_list_append_content(viewlog->widget_list->l, 
			   (viewlog->browser_widget = 
			    xitk_noskin_browser_create(&br,
						       viewlog->widget_list->gc, 25, 58, 
						       WINDOW_WIDTH - (50 + 16), 20,
						       16, br_fontname)));

  xitk_browser_set_alignment(viewlog->browser_widget, LABEL_ALIGN_LEFT);
  xitk_browser_update_list(viewlog->browser_widget, viewlog->log, viewlog->log_entries, 0);
  
  viewlog_paint_widgets();
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  x = (WINDOW_WIDTH - (100 * 2)) / 3;
  y = WINDOW_HEIGHT - 40;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Refresh");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = viewlog_refresh; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(viewlog->widget_list->l, 
	   xitk_noskin_labelbutton_create(&lb,
					  x, y, 100, 23,
					  "Black", "Black", "White", fontname));

  x += x * 2;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = LABEL_ALIGN_CENTER;
  lb.callback          = viewlog_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(viewlog->widget_list->l, 
	   xitk_noskin_labelbutton_create(&lb,
					  x, y, 100, 23,
					  "Black", "Black", "White", fontname));

  
  XUnlockDisplay (gGui->display);

  XMapRaised(gGui->display, xitk_window_get_window(viewlog->xwin));

  viewlog->kreg = xitk_register_event_handler("viewlog", 
					      (xitk_window_get_window(viewlog->xwin)),
					      viewlog_handle_event,
					      NULL,
					      NULL,
					      viewlog->widget_list,
					      NULL);
  

  viewlog->visible = 1;
  viewlog->running = 1;
  
}
