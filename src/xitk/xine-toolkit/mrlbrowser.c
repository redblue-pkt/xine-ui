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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef NEED_MRLBROWSER

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>                                                      
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "_xitk.h"
#include "mrlbrowser.h"
#include "browser.h"
#include "label.h"
#include "button.h"
#include "labelbutton.h"
#include "dnd.h"
#include "widget.h"
#include "widget_types.h"

#include "xine.h"

/*
 * Redisplay the displayed current origin (useful for file plugin).
 */
static void update_current_origin(mrlbrowser_private_data_t *private_data) {

  if(private_data->mc->mrls[0] && private_data->mc->mrls[0]->origin)
    sprintf(private_data->current_origin, "%s", 
	    private_data->mc->mrls[0]->origin);
  else
    sprintf(private_data->current_origin, "%s", "");
  
  label_change_label (private_data->widget_list, 
		      private_data->widget_origin, 
		      private_data->current_origin);
  
}

/*
 * Return the *marker* of the giver file *fully named*
 */
static int get_mrl_marker(mrl_t *mrl) {
  
  if(mrl->type & mrl_file_symlink)
    return '@';
  else if(mrl->type & mrl_file_directory)
    return '/';
  else if(mrl->type & mrl_file_fifo)
    return '|';
  else if(mrl->type & mrl_file_sock)
    return '=';
  else if(mrl->type & mrl_file_normal) {
    if(mrl->type & mrl_file_exec)
      return '*';
  }

  return '\0';
}

/*
 * Create *eye candy* entries in browser.
 */
static void mrlbrowser_create_enlighted_entries(mrlbrowser_private_data_t *private_data) {
  int i = 0;
  
  while(private_data->mc->mrls[i]) {
    char *p = private_data->mc->mrls[i]->mrl;

    if(private_data->mc->mrls[i]->origin) {

      p += strlen(private_data->mc->mrls[i]->origin);
      
      if(*p == '/') 
	p++;
    }
    
    if(private_data->mc->mrls[i]->type & mrl_file_symlink) {

      if(private_data->mc->mrls_disp[i])
	private_data->mc->mrls_disp[i] = (char *) 
	  realloc(private_data->mc->mrls_disp[i], strlen(p) + 4 + 
		  strlen(private_data->mc->mrls[i]->link) + 2);
      else
	private_data->mc->mrls_disp[i] = (char *) 
	  gui_xmalloc(strlen(p) + 4 + 
		      strlen(private_data->mc->mrls[i]->link) + 2);
      
      sprintf(private_data->mc->mrls_disp[i], "%s%c -> %s", 
	      p, get_mrl_marker(private_data->mc->mrls[i]), 
	      private_data->mc->mrls[i]->link);

    }
    else {
      if(private_data->mc->mrls_disp[i])
	private_data->mc->mrls_disp[i] = (char *) 
	  realloc(private_data->mc->mrls_disp[i], strlen(p) + 2);
      else
      private_data->mc->mrls_disp[i] = (char *) gui_xmalloc(strlen(p) + 2);
      
      sprintf(private_data->mc->mrls_disp[i], "%s%c", 
	      p, get_mrl_marker(private_data->mc->mrls[i]));
    }

    i++;
  }
}

/*
 * Duplicate mrls from xine-engine's returned. We shouldn't play
 * directly with these ones.
 */
static void mrlbrowser_duplicate_mrls(mrlbrowser_private_data_t *private_data,
				      mrl_t **mtmp) {
  int i = 0;
  int old_mrls_num = private_data->mrls_num;

  while(mtmp[i]) {

    if(private_data->mc->mrls[i] == NULL)
      private_data->mc->mrls[i] = (mrl_t *) gui_xmalloc(sizeof(mrl_t));

    MRL_DUPLICATE(mtmp[i], private_data->mc->mrls[i]);
    i++;
  }

  private_data->mrls_num = i;

  while(old_mrls_num > private_data->mrls_num) {
    MRL_ZERO(private_data->mc->mrls[old_mrls_num-1]);
    free(private_data->mc->mrls[old_mrls_num-1]);
    private_data->mc->mrls[old_mrls_num-1] = NULL;
    old_mrls_num--;
  }

  private_data->mc->mrls[private_data->mrls_num] = NULL;
}

/*
 * Grab mrls from xine-engine.
 */
static void mrlbrowser_grab_mrls(widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  char *lbl = (char *) labelbutton_get_label(w);
  char *old_old_src;

  if(lbl) {
    
    if(private_data->last_mrl_source)
      private_data->last_mrl_source = (char *)
	realloc(private_data->last_mrl_source, strlen(lbl) + 1);
    else
      private_data->last_mrl_source = (char *) gui_xmalloc(strlen(lbl) + 1);
    
    old_old_src = strdup(private_data->last_mrl_source);
    sprintf(private_data->last_mrl_source, "%s", lbl);

    {
      mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					  private_data->last_mrl_source, NULL);
      
      if(!mtmp) {
	private_data->last_mrl_source = (char *)
	  realloc(private_data->last_mrl_source, strlen(old_old_src) + 1);
	sprintf(private_data->last_mrl_source, "%s", old_old_src);
	return;
      }

      mrlbrowser_duplicate_mrls(private_data, mtmp);
    }
    
    update_current_origin(private_data);
    mrlbrowser_create_enlighted_entries(private_data);
    browser_update_list(private_data->mrlb_list, 
			private_data->mc->mrls_disp, 
			private_data->mrls_num, 0);
  }
}

/*
 * Dump informations, on terminal, about selected mrl.
 */
static void mrlbrowser_dumpmrl(widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  int j = -1;
  
  if((j = browser_get_current_selected(private_data->mrlb_list)) >= 0) {
    mrl_t *ms = private_data->mc->mrls[j];
    
    printf("mrl '%s'\n\t+", ms->mrl);

    if(ms->type & mrl_file_symlink)
      printf(" symlink to '%s' \n\t+ ", ms->link);

    printf("[");

    if(ms->type & mrl_unknown)
      printf(" unknown ");
    
      if(ms->type & mrl_dvd)
	printf(" dvd ");
      
      if(ms->type & mrl_vcd)
	    printf(" vcd ");
      
      if(ms->type & mrl_net)
	printf(" net ");
      
      if(ms->type & mrl_rtp)
	printf(" rtp ");
      
      if(ms->type & mrl_stdin)
	printf(" stdin ");
      
      if(ms->type & mrl_file)
	printf(" file ");
      
      if(ms->type & mrl_file_fifo)
	printf(" fifo ");
      
      if(ms->type & mrl_file_chardev)
	printf(" chardev ");
      
      if(ms->type & mrl_file_directory)
	printf(" directory ");
      
      if(ms->type & mrl_file_blockdev)
	printf(" blockdev ");
      
      if(ms->type & mrl_file_normal)
	printf(" normal ");
      
      if(ms->type & mrl_file_sock)
	printf(" sock ");
      
      if(ms->type & mrl_file_exec)
	printf(" exec ");
      
      if(ms->type & mrl_file_backup)
	printf(" backup ");
      
      if(ms->type & mrl_file_hidden)
	printf(" hidden ");
      
      printf("] (%Ld byte%c)\n", ms->size, (ms->size > 0) ?'s':'\0');
  }
}
/*
 *                                 END OF PRIVATES
 *****************************************************************************/

/*
 * Fill window information struct of given widget.
 */
int mrlbrowser_get_window_info(widget_t *w, window_info_t *inf) {
  mrlbrowser_private_data_t *private_data = 
    (mrlbrowser_private_data_t *)w->private_data;
  
  return((widget_get_window_info(private_data->widget_key, inf))); 
}

/*
 * Boolean about running state.
 */
int mrlbrowser_is_running(widget_t *w) {
  mrlbrowser_private_data_t *private_data;
 
  if(w) {
    private_data = w->private_data;
    return (private_data->running);
  }

  return 0;
}

/*
 * Boolean about visible state.
 */
int mrlbrowser_is_visible(widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w) {
    private_data = w->private_data;
    return (private_data->visible);
  }

  return 0;
}

/*
 * Hide mrlbrowser.
 */
void mrlbrowser_hide(widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w) {
    private_data = w->private_data;

    if(private_data->visible) {
      XLOCK(private_data->display);
      XUnmapWindow(private_data->display, private_data->window);
      XUNLOCK(private_data->display);
      private_data->visible = 0;
    }
  }
}

/*
 * Show mrlbrowser.
 */
void mrlbrowser_show(widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w) {
    private_data = w->private_data;

    XLOCK(private_data->display);
    XMapRaised(private_data->display, private_data->window); 
    XUNLOCK(private_data->display);
    private_data->visible = 1;
  }
}

/*
 * Set mrlbrowser transient for hints for given window.
 */
void mrlbrowser_set_transient(widget_t *w, Window window) {
  mrlbrowser_private_data_t *private_data;

  if(w && (window != None)) {
    private_data = w->private_data;

    if(private_data->visible) {
      XLOCK(private_data->display);
      XSetTransientForHint (private_data->display,
			    private_data->window, 
			    window);
      XUNLOCK(private_data->display);
    }
  }
}

/*
 * Destroy the mrlbrowser.
 */
void mrlbrowser_destroy(widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w) {
    private_data = w->private_data;

    private_data->running = 0;
    private_data->visible = 0;
    
    XLOCK(private_data->display);
    XUnmapWindow(private_data->display, private_data->window);
    XUNLOCK(private_data->display);
   
    gui_list_free(private_data->widget_list->l);
    free(private_data->widget_list);

    {
      int i;
      for(i = private_data->mrls_num; i > 0; i--) {
	MRL_ZERO(private_data->mc->mrls[i]);
	free(private_data->mc->mrls[i]);
	free(private_data->mc->mrls_disp[i]);
      }
    }
    
    free(private_data->mc);

    XLOCK(private_data->display);
    XDestroyWindow(private_data->display, private_data->window);
    XUNLOCK(private_data->display);

    widget_unregister_event_handler(&private_data->widget_key);
    free(private_data->fbWidget);
    free(private_data);
  }
}

/*
 * Leaving mrlbrowser.
 */
void mrlbrowser_exit(widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = ((widget_t *)data)->private_data;
  
  if(private_data->kill_callback)
    private_data->kill_callback(private_data->fbWidget, NULL);

  private_data->running = 0;
  private_data->visible = 0;
  
  XLOCK(private_data->display);
  XUnmapWindow(private_data->display, private_data->window);
  XUNLOCK(private_data->display);

  gui_list_free(private_data->widget_list->l);
  free(private_data->widget_list);

  {
    int i;
    for(i = private_data->mrls_num; i > 0; i--) {
      MRL_ZERO(private_data->mc->mrls[i]);
      free(private_data->mc->mrls[i]);
      free(private_data->mc->mrls_disp[i]);
    }
  }

  free(private_data->mc);
  
  XLOCK(private_data->display);
  XDestroyWindow(private_data->display, private_data->window);
  XUNLOCK(private_data->display);

  widget_unregister_event_handler(&private_data->widget_key);
  free(private_data->fbWidget);
  free(private_data);
}

/*
 * 
 */
static void mrlbrowser_select_mrl(mrlbrowser_private_data_t *private_data,
				  int j) {
  mrl_t *ms = private_data->mc->mrls[j];
  char   buf[PATH_MAX + NAME_MAX + 1];
  
  memset(&buf, '\0', sizeof(buf));
  sprintf(buf, "%s", ms->mrl);
  
  if((ms->type & mrl_file) && (ms->type & mrl_file_directory)) {
    char *filename = ms->mrl;
    
    if(ms->origin) {
      filename += strlen(ms->origin);
      
      if(*filename == '/') 
	filename++;
    }
    
    /* Check if we want to re-read current dir or go in parent dir */
    if((filename[strlen(filename) - 1] == '.') &&
       (filename[strlen(filename) - 2] != '.')) {
      
      memset(&buf, '\0', sizeof(buf));
      sprintf(buf, "%s", ms->origin);
    }
    else if((filename[strlen(filename) - 1] == '.') &&
	    (filename[strlen(filename) - 2] == '.')) {
      char *p;
      
      memset(&buf, '\0', sizeof(buf));
      sprintf(buf, "%s", ms->origin);
      if(strlen(buf) > 1) { /* not '/' directory */
	  
	p = &buf[strlen(buf)-1];
	while(*p && *p != '/') {
	  *p = '\0';
	    p--;
	}
	
	/* Remove last '/' if current_dir isn't root */
	if((strlen(buf) > 1) && *p == '/') 
	  *p = '\0';
	  
      }
    }
    
    {
      mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					  private_data->last_mrl_source, 
					  buf);
      
      mrlbrowser_duplicate_mrls(private_data, mtmp);
    }
    
    update_current_origin(private_data);
    mrlbrowser_create_enlighted_entries(private_data);
    browser_update_list(private_data->mrlb_list, 
			private_data->mc->mrls_disp, 
			private_data->mrls_num, 0);
    
  }
  else {
    
    browser_release_all_buttons(private_data->mrlb_list);
    if(private_data->add_callback)
      private_data->add_callback(NULL, (void *) j, 
				 private_data->mc->mrls[j]);

  }
}

/*
 * Handle selection in mrlbrowser.
 */
static void mrlbrowser_select(widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  int j = -1;

  if((j = browser_get_current_selected(private_data->mrlb_list)) >= 0) {
    mrlbrowser_select_mrl(private_data, j);
  }
}

/*
 * Handle double click in labelbutton list.
 */
static void handle_dbl_click(widget_t *w, int selected, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;

  mrlbrowser_select_mrl(private_data, selected);
}

/*
 * Handle here mrlbrowser events.
 */
static void mrlbrowser_handle_event(XEvent *event, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;
  
  switch(event->type) {

  case MappingNotify:
    XLOCK(private_data->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUNLOCK(private_data->display);
    break;

  case KeyPress:
    mykeyevent = event->xkey;

    XLOCK (private_data->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUNLOCK (private_data->display);

    switch (mykey) {
    
    case XK_s:
    case XK_S:
      mrlbrowser_select(NULL, (void *)private_data);
    break;

    /* This is for debugging purpose */
    case XK_d:
    case XK_D:
      mrlbrowser_dumpmrl(NULL, (void *)private_data);
    break;

    case XK_Down:
      browser_step_up((widget_t *)private_data->mrlb_list, NULL);
    break;

    case XK_Up:
      browser_step_down((widget_t *)private_data->mrlb_list, NULL);
    break;

    }
  }
}

/*
 * Create mrlbrowser window.
 */
widget_t *mrlbrowser_create(Display *display, ImlibData *idata,
			    Window window_trans,
			    mrlbrowser_placements_t *mbp) {
  GC                         gc;
  XSizeHints                 hint;
  XSetWindowAttributes       attr;
  char                      *title = mbp->window_title;
  Atom                       prop;
  MWMHints                   mwmhints;
  XWMHints                  *wm_hint;
  XClassHint                *xclasshint;
  XColor                     black, dummy;
  int                        screen;
  widget_t                  *mywidget;
  mrlbrowser_private_data_t *private_data;
  
  if(mbp->ip_availables == NULL) {
    fprintf(stderr, "Something's going wrong, there is no input plugin "
	    "available having INPUT_CAP_GET_DIR capability !!\nExiting.\n");
    exit(1);
  }

  if(mbp->xine == NULL) {
    fprintf(stderr, "Xine engine should be initialized first !!\nExiting.\n");
    exit(1);
  }

  mywidget = (widget_t *) gui_xmalloc(sizeof(widget_t));
  
  private_data = (mrlbrowser_private_data_t *) 
    gui_xmalloc(sizeof(mrlbrowser_private_data_t));

  mywidget->private_data = private_data;

  private_data->fbWidget = mywidget;
  private_data->display  = display;
  private_data->xine     = mbp->xine;
  private_data->running  = 1;

  XLOCK(display);

  if(!(private_data->bg_image = Imlib_load_image(idata, mbp->bg_skinfile))) {
    fprintf(stderr, "%s(%d): couldn't find image for background\n",
	    __FILE__, __LINE__);

    XUNLOCK(display);
    return NULL;
  }

  private_data->mc              = (mrl_contents_t *) gui_xmalloc(sizeof(mrl_contents_t));
  private_data->mrls_num        = 0;
  private_data->last_mrl_source = NULL;

  hint.x      = mbp->x;
  hint.y      = mbp->y;
  hint.width  = private_data->bg_image->rgb_width;
  hint.height = private_data->bg_image->rgb_height;
  hint.flags  = PPosition | PSize;

  screen = DefaultScreen(display);

  XAllocNamedColor(display, 
                   DefaultColormap(display, screen), 
                   "black", &black, &dummy);

  attr.override_redirect = True;
  attr.background_pixel  = black.pixel;
  attr.border_pixel      = 1;
  attr.colormap  = XCreateColormap(display,
				   RootWindow(display, screen),
				   idata->x.visual, AllocNone);
  
  private_data->window = 
    XCreateWindow (display, DefaultRootWindow(display), 
		   hint.x, hint.y, hint.width, 
		   hint.height, 0, 
		   idata->x.depth, 
		   CopyFromParent, 
		   idata->x.visual,
		   CWBackPixel | CWBorderPixel | CWColormap, &attr);
  
  XSetStandardProperties(display, private_data->window, title, title,
			 None, NULL, 0, &hint);

  XSelectInput(display, private_data->window,
	       ButtonPressMask | ButtonReleaseMask | PointerMotionMask 
	       | KeyPressMask | ExposureMask | StructureNotifyMask);
  
  /*
   * wm, no border please
   */
  prop = XInternAtom(display, "_MOTIF_WM_HINTS", True);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(display, private_data->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);						
  if(window_trans != None)
    XSetTransientForHint (display, private_data->window, window_trans);

  /* set xclass */
  
  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = mbp->resource_name ? mbp->resource_name : "xitk mrl browser";
    xclasshint->res_class = mbp->resource_class ? mbp->resource_class : "xitk";
    XSetClassHint(display, private_data->window, xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags = InputHint | StateHint;
    XSetWMHints(display, private_data->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(display, private_data->window, 0, 0);
  
  Imlib_apply_image(idata, private_data->bg_image, private_data->window);

  private_data->widget_list                = widget_list_new() ;
  private_data->widget_list->l             = gui_list_new ();
  private_data->widget_list->focusedWidget = NULL;
  private_data->widget_list->pressedWidget = NULL;
  private_data->widget_list->win           = private_data->window;
  private_data->widget_list->gc            = gc;
  
  gui_list_append_content (private_data->widget_list->l,
	   label_button_create (display, 
				idata,
				mbp->select.x,
				mbp->select.y,
				CLICK_BUTTON,
				mbp->select.caption,
				mrlbrowser_select, (void*)private_data,
				mbp->select.skin_filename,
				mbp->select.normal_color,
				mbp->select.focused_color,
				mbp->select.clicked_color));

  gui_list_append_content (private_data->widget_list->l,
	   label_button_create (display,
				idata,
				mbp->dismiss.x,
				mbp->dismiss.y,
				CLICK_BUTTON,
				mbp->dismiss.caption,
				mrlbrowser_exit, (void*)mywidget,
				mbp->dismiss.skin_filename,
				mbp->dismiss.normal_color,
				mbp->dismiss.focused_color,
				mbp->dismiss.clicked_color));
  
  private_data->add_callback = mbp->select.callback;
  private_data->kill_callback = mbp->kill.callback;

  mbp->br_placement->dbl_click_cb = handle_dbl_click;
  mbp->br_placement->user_data = (void *)private_data;
  gui_list_append_content (private_data->widget_list->l,
			   (private_data->mrlb_list = 
			    browser_create(display, idata, 
					   private_data->widget_list,
					   mbp->br_placement)));

  gui_list_append_content (private_data->widget_list->l,
			   (private_data->widget_origin = 
			    label_create (display, idata,
					  mbp->origin.x,
					  mbp->origin.y,
					  mbp->origin.max_length,
					  "",
					  mbp->origin.skin_filename)));
  
  memset(&private_data->current_origin, 0, strlen(private_data->current_origin));
  if(mbp->origin.cur_origin)
    sprintf(private_data->current_origin, "%s", mbp->origin.cur_origin);

  if(mbp->ip_name.label.label_str) {
    gui_list_append_content (private_data->widget_list->l,
		     label_create (display, idata, 
				   mbp->ip_name.label.x,
				   mbp->ip_name.label.y,
				   strlen(mbp->ip_name.label.label_str),
				   mbp->ip_name.label.label_str,
				   mbp->ip_name.label.skin_filename));
  }

  /*
   * Create buttons with input plugins names.
   */
  {
    int x, y;
    int i = 0;
    widget_t *tmp;
    
    x = mbp->ip_name.button.x;
    y = mbp->ip_name.button.y;
    
    while(mbp->ip_availables[i] != NULL) {
      gui_list_append_content (private_data->widget_list->l,
	       (tmp =
		label_button_create (display, idata, 
				     x, y,
				     CLICK_BUTTON,
				     mbp->ip_availables[i],
				     mrlbrowser_grab_mrls, 
				     (void*)private_data, 
				     mbp->ip_name.button.skin_filename,
				     mbp->ip_name.button.normal_color,
				     mbp->ip_name.button.focused_color,
				     mbp->ip_name.button.clicked_color)));
      y += widget_get_height(tmp) + 1;
      i++;
    }
  }
    
  private_data->visible = 1;

  mywidget->enable         = 1;
  mywidget->x              = mbp->x;
  mywidget->y              = mbp->y;
  mywidget->width          = private_data->bg_image->width;
  mywidget->height         = private_data->bg_image->height;
  mywidget->widget_type    = WIDGET_TYPE_MRLBROWSER | WIDGET_TYPE_GROUP;
  mywidget->paint          = NULL;
  mywidget->notify_click   = NULL;
  mywidget->notify_focus   = NULL;

  browser_update_list(private_data->mrlb_list, 
		      private_data->mc->mrls_disp, 
		      private_data->mrls_num, 0);
  
  XMapRaised(display, private_data->window); 

  private_data->widget_key = 
    widget_register_event_handler("mrl browser",
				  private_data->window, 
				  mrlbrowser_handle_event,
				  NULL,
				  mbp->dndcallback,
				  private_data->widget_list,
				  (void *) private_data);

  XUNLOCK (display);

  return mywidget;
}

#endif
