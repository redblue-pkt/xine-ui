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

#include <xine.h>

#include "_xitk.h"
#include "mrlbrowser.h"
#include "browser.h"
#include "label.h"
#include "button.h"
#include "labelbutton.h"
#include "inputtext.h"
#include "dnd.h"
#include "window.h"
#include "widget.h"
#include "widget_types.h"

#undef DEBUG_MRLB

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {

  if(w->widget_type & WIDGET_TYPE_MRLBROWSER) {
    xitk_mrlbrowser_destroy(w);
  }
}

/*
 * Redisplay the displayed current origin (useful for file plugin).
 */
static void update_current_origin(mrlbrowser_private_data_t *private_data) {

  if(private_data->mc->mrls[0] && private_data->mc->mrls[0]->origin)
    sprintf(private_data->current_origin, "%s", 
	    private_data->mc->mrls[0]->origin);
  else
    sprintf(private_data->current_origin, "%s", "");
  
  xitk_label_change_label (private_data->widget_list, 
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
	  xitk_xmalloc(strlen(p) + 4 + 
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
      private_data->mc->mrls_disp[i] = (char *) xitk_xmalloc(strlen(p) + 2);
      
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
				      mrl_t **mtmp, int num_mrls) {
  int i;
  int old_mrls_num = private_data->mrls_num;

  for (i=0; i<num_mrls; i++) {

    if(private_data->mc->mrls[i] == NULL)
      private_data->mc->mrls[i] = (mrl_t *) xitk_xmalloc(sizeof(mrl_t));

    MRL_DUPLICATE(mtmp[i], private_data->mc->mrls[i]);
  }

  private_data->mrls_num = i;

  while(old_mrls_num > private_data->mrls_num) {
    MRL_ZERO(private_data->mc->mrls[old_mrls_num-1]);
    XITK_FREE(private_data->mc->mrls[old_mrls_num-1]);
    private_data->mc->mrls[old_mrls_num-1] = NULL;
    old_mrls_num--;
  }

  private_data->mc->mrls[private_data->mrls_num] = NULL;
}

/*
 * Grab mrls from xine-engine.
 */
static void mrlbrowser_grab_mrls(xitk_widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  char *lbl = (char *) xitk_labelbutton_get_label(w);
  char *old_old_src;

  if(lbl) {
    
    if(private_data->last_mrl_source)
      private_data->last_mrl_source = (char *)
	realloc(private_data->last_mrl_source, strlen(lbl) + 1);
    else
      private_data->last_mrl_source = (char *) xitk_xmalloc(strlen(lbl) + 1);
    
    old_old_src = strdup(private_data->last_mrl_source);
    sprintf(private_data->last_mrl_source, "%s", lbl);

    {
      int num_mrls;
      mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					  private_data->last_mrl_source, NULL,
					  &num_mrls);
      
      if(!mtmp) {
	private_data->last_mrl_source = (char *)
	  realloc(private_data->last_mrl_source, strlen(old_old_src) + 1);
	sprintf(private_data->last_mrl_source, "%s", old_old_src);
	return;
      }

      mrlbrowser_duplicate_mrls(private_data, mtmp, num_mrls);
    }
    
    update_current_origin(private_data);
    mrlbrowser_create_enlighted_entries(private_data);
    xitk_browser_update_list(private_data->mrlb_list, 
			     private_data->mc->mrls_disp, 
			     private_data->mrls_num, 0);
  }
}

#ifdef DEBUG_MRLB
/*
 * Dump informations, on terminal, about selected mrl.
 */
static void mrlbrowser_dumpmrl(xitk_widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  int j = -1;
  
  if((j = xitk_browser_get_current_selected(private_data->mrlb_list)) >= 0) {
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
#endif
/*
 *                                 END OF PRIVATES
 *****************************************************************************/

/*
 * Enable/Disable tips.
 */
void xitk_mrlbrowser_set_tips_timeout(xitk_widget_t *w, int enabled, unsigned long timeout) {
  mrlbrowser_private_data_t *private_data;
  
  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    
    if(enabled)
      xitk_set_widgets_tips_timeout(private_data->widget_list, timeout);
    else
      xitk_disable_widgets_tips(private_data->widget_list);
  }
}

/*
 * Return window id of widget.
 */
Window xitk_mrlbrowser_get_window_id(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;
  
  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    return private_data->window;
  }

  return None;
}


/*
 * Fill window information struct of given widget.
 */
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    
    return((xitk_get_window_info(private_data->widget_key, inf))); 
  }
  return 0;
  
}

/*
 * Boolean about running state.
 */
int xitk_mrlbrowser_is_running(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;
 
  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    return (private_data->running);
  }

  return 0;
}

/*
 * Boolean about visible state.
 */
int xitk_mrlbrowser_is_visible(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    return (private_data->visible);
  }

  return 0;
}

/*
 * Hide mrlbrowser.
 */
void xitk_mrlbrowser_hide(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;

    if(private_data->visible) {

      XLOCK(private_data->imlibdata->x.disp);
      XUnmapWindow(private_data->imlibdata->x.disp, private_data->window);
      XUNLOCK(private_data->imlibdata->x.disp);

      xitk_hide_widgets(private_data->widget_list);
      private_data->visible = 0;
    }
  }
}

/*
 * Show mrlbrowser.
 */
void xitk_mrlbrowser_show(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;

    xitk_show_widgets(private_data->widget_list);

    XLOCK(private_data->imlibdata->x.disp);
    XMapRaised(private_data->imlibdata->x.disp, private_data->window); 
    XUNLOCK(private_data->imlibdata->x.disp);

    private_data->visible = 1;
  }
}

/*
 * Set mrlbrowser transient for hints for given window.
 */
void xitk_mrlbrowser_set_transient(xitk_widget_t *w, Window window) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER) && (window != None)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;

    if(private_data->visible) {
      XLOCK(private_data->imlibdata->x.disp);
      XSetTransientForHint (private_data->imlibdata->x.disp,
			    private_data->window, 
			    window);
      XUNLOCK(private_data->imlibdata->x.disp);
    }
  }
}

/*
 * Destroy the mrlbrowser.
 */
void xitk_mrlbrowser_destroy(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;

    private_data->running = 0;
    private_data->visible = 0;
    
    xitk_unregister_event_handler(&private_data->widget_key);
    
    XLOCK(private_data->imlibdata->x.disp);
    XUnmapWindow(private_data->imlibdata->x.disp, private_data->window);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    xitk_destroy_widgets(private_data->widget_list);
    
    XLOCK(private_data->imlibdata->x.disp);
    XDestroyWindow(private_data->imlibdata->x.disp, private_data->window);
    Imlib_destroy_image(private_data->imlibdata, private_data->bg_image);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    private_data->window = None;
    xitk_list_free(private_data->widget_list->l);
    
    XLOCK(private_data->imlibdata->x.disp);
    XFreeGC(private_data->imlibdata->x.disp, private_data->widget_list->gc);
    XUNLOCK(private_data->imlibdata->x.disp);
    
    XITK_FREE(private_data->widget_list);
    
    {
      int i;
      for(i = private_data->mrls_num; i > 0; i--) {
	MRL_ZERO(private_data->mc->mrls[i]);
	XITK_FREE(private_data->mc->mrls[i]);
	XITK_FREE(private_data->mc->mrls_disp[i]);
      }
    }
    
    XITK_FREE(private_data->skin_element_name);
    XITK_FREE(private_data->skin_element_name_ip);
    XITK_FREE(private_data->mc);
    XITK_FREE(private_data);

  }
}

/*
 * Leaving mrlbrowser.
 */
void xitk_mrlbrowser_exit(xitk_widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = ((xitk_widget_t *)data)->private_data;
  
  if(private_data->kill_callback)
    private_data->kill_callback(private_data->fbWidget, NULL);

  xitk_mrlbrowser_destroy(private_data->fbWidget);
}


/*
 *
 */
void xitk_mrlbrowser_change_skins(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  ImlibImage                *new_img, *old_img;
  mrlbrowser_private_data_t *private_data;
  XSizeHints                 hint;

  if(w && (w->widget_type & WIDGET_TYPE_MRLBROWSER)) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    
    xitk_skin_lock(skonfig);
    xitk_hide_widgets(private_data->widget_list);

    XLOCK(private_data->imlibdata->x.disp);
    
    if(!(new_img = Imlib_load_image(private_data->imlibdata,
				    xitk_skin_get_skin_filename(skonfig, 
							private_data->skin_element_name)))) {
      XITK_DIE("%s(): couldn't find image for background\n", __FUNCTION__);
    }
    
    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PSize;
    XSetWMNormalHints(private_data->imlibdata->x.disp, private_data->window, &hint);

    XResizeWindow (private_data->imlibdata->x.disp, private_data->window,
		   (unsigned int)new_img->rgb_width,
		   (unsigned int)new_img->rgb_height);

    XUNLOCK(private_data->imlibdata->x.disp);

    while(!xitk_is_window_size(private_data->imlibdata->x.disp, private_data->window, 
			       new_img->rgb_width, new_img->rgb_height)) {
      xitk_usec_sleep(10000);
    }

    old_img = private_data->bg_image;
    private_data->bg_image = new_img;

    XLOCK(private_data->imlibdata->x.disp);

    Imlib_destroy_image(private_data->imlibdata, old_img);
    Imlib_apply_image(private_data->imlibdata, new_img, private_data->window);

    XUNLOCK(private_data->imlibdata->x.disp);

    xitk_skin_unlock(skonfig);
    
    xitk_change_skins_widget_list(private_data->widget_list, skonfig);

    {
      int x, y;
      int i = 0;
      
      x = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name_ip);
      y = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name_ip);
    
      while(private_data->autodir_plugins[i] != NULL) {
	
	(void) xitk_set_widget_pos(private_data->autodir_plugins[i], x, y);
	
	y += xitk_get_widget_height(private_data->autodir_plugins[i]) + 1;
	i++;
      }
    }

    xitk_paint_widget_list(private_data->widget_list);
    
  }
}

/*
 * 
 */
static void mrlbrowser_select_mrl(mrlbrowser_private_data_t *private_data,
				  int j, int add_callback, int play_callback) {
  mrl_t *ms = private_data->mc->mrls[j];
  char   buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  
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
      int num_mrls;
      mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					  private_data->last_mrl_source, 
					  buf, &num_mrls);
      
      mrlbrowser_duplicate_mrls(private_data, mtmp, num_mrls);
    }
    
    update_current_origin(private_data);
    mrlbrowser_create_enlighted_entries(private_data);
    xitk_browser_update_list(private_data->mrlb_list, 
			     private_data->mc->mrls_disp, 
			     private_data->mrls_num, 0);
    
  }
  else {
    
    xitk_browser_release_all_buttons(private_data->mrlb_list);

    if(add_callback && private_data->add_callback)
      private_data->add_callback(NULL, (void *) j, 
				 private_data->mc->mrls[j]);
    
    if(play_callback && private_data->play_callback)
      private_data->play_callback(NULL, (void *) j,
				  private_data->mc->mrls[j]);
  }
}

/*
 * Handle selection in mrlbrowser.
 */
static void mrlbrowser_select(xitk_widget_t *w, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  int j = -1;

  if((j = xitk_browser_get_current_selected(private_data->mrlb_list)) >= 0) {
    mrlbrowser_select_mrl(private_data, j, 1, 0);
  }
}

/*
 * Handle selection in mrlbrowser, then 
 */
static void mrlbrowser_play(xitk_widget_t *w, void *data) {
  
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  int j = -1;
  
  if((j = xitk_browser_get_current_selected(private_data->mrlb_list)) >= 0) {
    mrlbrowser_select_mrl(private_data, j, 0, 1);
  }
}

/*
 * Handle double click in labelbutton list.
 */
static void handle_dbl_click(xitk_widget_t *w, void *data, int selected) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  XEvent                     xev;
  int                        modifier;
  
  XLOCK(private_data->imlibdata->x.disp);
  XPeekEvent(private_data->imlibdata->x.disp, &xev);
  XUNLOCK(private_data->imlibdata->x.disp);

  xitk_get_key_modifier(&xev, &modifier);

  if(modifier & MODIFIER_META)
    mrlbrowser_select_mrl(private_data, selected, 1, 0);
  else
    mrlbrowser_select_mrl(private_data, selected, 0, 1);
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

  case EnterNotify:
    XLOCK(private_data->imlibdata->x.disp);
    XRaiseWindow(private_data->imlibdata->x.disp, private_data->window);
    XUNLOCK(private_data->imlibdata->x.disp);
    break;

  case ButtonPress: {
    XButtonEvent *bevent = (XButtonEvent *) event;
    if (bevent->button == Button4)
      xitk_browser_step_down((xitk_widget_t *)private_data->mrlb_list, NULL);
    else if(bevent->button == Button5)
      xitk_browser_step_up((xitk_widget_t *)private_data->mrlb_list, NULL);
  }
  break;

  case MappingNotify:
    XLOCK(private_data->imlibdata->x.disp);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUNLOCK(private_data->imlibdata->x.disp);
    break;
    
  case KeyPress:
    mykeyevent = event->xkey;
    
    XLOCK (private_data->imlibdata->x.disp);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUNLOCK (private_data->imlibdata->x.disp);
    
    switch (mykey) {
      
    case XK_space:
    case XK_s:
    case XK_S:
      mrlbrowser_select(NULL, (void *)private_data);
      break;

    case XK_Return: {
      int selected;
      
      if((selected = xitk_browser_get_current_selected(private_data->mrlb_list)) >= 0)
	mrlbrowser_select_mrl(private_data, selected, 0, 1);
    }
    break;
      
      /* This is for debugging purpose */
#ifdef DEBUG_MRLB
    case XK_d:
    case XK_D:
      mrlbrowser_dumpmrl(NULL, (void *)private_data);
      break;
#endif
      
    case XK_Down:
    case XK_Next:
      xitk_browser_step_up((xitk_widget_t *)private_data->mrlb_list, NULL);
      break;
      
    case XK_Up:
    case XK_Prior:
      xitk_browser_step_down((xitk_widget_t *)private_data->mrlb_list, NULL);
      break;
    }
    
    break;
  }
}

/*
 * Create mrlbrowser window.
 */
xitk_widget_t *xitk_mrlbrowser_create(xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb) {
  GC                          gc;
  XSizeHints                  hint;
  XSetWindowAttributes        attr;
  char                       *title = mb->window_title;
  Atom                        prop, XA_WIN_LAYER;
  MWMHints                    mwmhints;
  XWMHints                   *wm_hint;
  XClassHint                 *xclasshint;
  XColor                      black, dummy;
  int                         screen;
  xitk_widget_t              *mywidget;
  mrlbrowser_private_data_t  *private_data;
  xitk_labelbutton_widget_t   lb;
  xitk_button_widget_t        pb;
  xitk_label_widget_t         lbl;
  xitk_widget_t              *w;
  long                        data[1];
  
  XITK_CHECK_CONSTITENCY(mb);

  if(mb->ip_availables == NULL) {
    XITK_DIE("Something's going wrong, there is no input plugin "
	     "available having INPUT_CAP_GET_DIR capability !!\nExiting.\n");
  }

  if(mb->xine == NULL) {
    XITK_DIE("Xine engine should be initialized first !!\nExiting.\n");
  }

  XITK_WIDGET_INIT(&lb, mb->imlibdata);
  XITK_WIDGET_INIT(&pb, mb->imlibdata);
  XITK_WIDGET_INIT(&lbl, mb->imlibdata);
  XITK_WIDGET_INIT(&mb->browser, mb->imlibdata);
  
  mywidget                        = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  
  private_data                    = (mrlbrowser_private_data_t *) xitk_xmalloc(sizeof(mrlbrowser_private_data_t));

  mywidget->private_data          = private_data;

  private_data->fbWidget          = mywidget;
  private_data->imlibdata         = mb->imlibdata;
  private_data->skin_element_name = strdup(mb->skin_element_name);

  private_data->xine              = mb->xine;
  private_data->running           = 1;

  XLOCK(mb->imlibdata->x.disp);

  if(!(private_data->bg_image = Imlib_load_image(mb->imlibdata, 
						 xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name)))) {
    XITK_WARNING("%s(%d): couldn't find image for background\n", __FILE__, __LINE__);
    
    XUNLOCK(mb->imlibdata->x.disp);
    return NULL;
  }

  private_data->mc              = (mrl_contents_t *) xitk_xmalloc(sizeof(mrl_contents_t));
  private_data->mrls_num        = 0;
  private_data->last_mrl_source = NULL;

  hint.x      = mb->x;
  hint.y      = mb->y;
  hint.width  = private_data->bg_image->rgb_width;
  hint.height = private_data->bg_image->rgb_height;
  hint.flags  = PPosition | PSize;

  screen = DefaultScreen(mb->imlibdata->x.disp);

  XAllocNamedColor(mb->imlibdata->x.disp, 
		   Imlib_get_colormap(mb->imlibdata),
		   "black", &black, &dummy);

  attr.override_redirect = False;
  attr.background_pixel  = black.pixel;
  /*
   * XXX:multivis
   * To avoid BadMatch errors on XCreateWindow:
   * If the parent and the new window have different depths, we must supply either
   * a BorderPixmap or a BorderPixel.
   * If the parent and the new window use different visuals, we must supply a
   * Colormap
   */
  attr.border_pixel      = 1;
  attr.colormap		 = Imlib_get_colormap(mb->imlibdata);

  private_data->window   = XCreateWindow (mb->imlibdata->x.disp,
					  DefaultRootWindow(mb->imlibdata->x.disp), 
					  hint.x, hint.y, hint.width, 
					  hint.height, 0, 
					  mb->imlibdata->x.depth, 
					  InputOutput, 
					  mb->imlibdata->x.visual,
					  CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
					  &attr);
  
  XSetStandardProperties(mb->imlibdata->x.disp, private_data->window, title, title,
			 None, NULL, 0, &hint);

  XSelectInput(mb->imlibdata->x.disp, private_data->window, INPUT_MOTION | KeymapStateMask);
  
  /*
   * layer above most other things, like gnome panel
   * WIN_LAYER_ABOVE_DOCK  = 10
   *
   */
  if(mb->layer_above) {
    XA_WIN_LAYER = XInternAtom(mb->imlibdata->x.disp, "_WIN_LAYER", False);
    
    data[0] = 10;
    XChangeProperty(mb->imlibdata->x.disp, private_data->window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		    1);
  }
  
  /*
   * wm, no border please
   */
  prop                 = XInternAtom(mb->imlibdata->x.disp, "_MOTIF_WM_HINTS", True);
  mwmhints.flags       = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(mb->imlibdata->x.disp, private_data->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);						
  if(mb->window_trans != None)
    XSetTransientForHint (mb->imlibdata->x.disp, private_data->window, mb->window_trans);

  /* set xclass */
  
  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name  = mb->resource_name ? mb->resource_name : "xitk mrl browser";
    xclasshint->res_class = mb->resource_class ? mb->resource_class : "xitk";
    XSetClassHint(mb->imlibdata->x.disp, private_data->window, xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags         = InputHint | StateHint;
    XSetWMHints(mb->imlibdata->x.disp, private_data->window, wm_hint);
    XFree(wm_hint);
  }
  
  gc = XCreateGC(mb->imlibdata->x.disp, private_data->window, 0, 0);
  
  Imlib_apply_image(mb->imlibdata, 
		    private_data->bg_image, private_data->window);

  XUNLOCK (mb->imlibdata->x.disp);

  private_data->widget_list                = xitk_widget_list_new() ;
  private_data->widget_list->l             = xitk_list_new ();
  private_data->widget_list->focusedWidget = NULL;
  private_data->widget_list->pressedWidget = NULL;
  private_data->widget_list->win           = private_data->window;
  private_data->widget_list->gc            = gc;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = mb->select.caption;
  lb.callback          = mrlbrowser_select;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)private_data;
  lb.skin_element_name = mb->select.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
			   (w = xitk_labelbutton_create (skonfig, &lb)));
  xitk_set_widget_tips(w, _("Select current entry"));
  
  pb.skin_element_name = mb->play.skin_element_name;
  pb.callback          = mrlbrowser_play;
  pb.userdata          = (void *)private_data;
  xitk_list_append_content(private_data->widget_list->l,
			   (w = xitk_button_create (skonfig, &pb)));
  xitk_set_widget_tips(w, _("Play selected entry"));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = mb->dismiss.caption;
  lb.callback          = xitk_mrlbrowser_exit;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)mywidget;
  lb.skin_element_name = mb->dismiss.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
			   (w = xitk_labelbutton_create (skonfig, &lb)));
  xitk_set_widget_tips(w, _("Close MRL browser window"));
  
  private_data->add_callback      = mb->select.callback;
  private_data->play_callback     = mb->play.callback;
  private_data->kill_callback     = mb->kill.callback;
  mb->browser.dbl_click_callback  = handle_dbl_click;
  mb->browser.userdata            = (void *)private_data;
  mb->browser.parent_wlist        = private_data->widget_list;
  xitk_list_append_content (private_data->widget_list->l,
			   (private_data->mrlb_list = 
			    xitk_browser_create(skonfig, &mb->browser)));

  lbl.label             = "";
  lbl.skin_element_name = mb->origin.skin_element_name;
  lbl.window            = private_data->widget_list->win;
  lbl.gc                = private_data->widget_list->gc;
  xitk_list_append_content(private_data->widget_list->l,
			  (private_data->widget_origin = 
			   xitk_label_create(skonfig, &lbl)));
  
  memset(&private_data->current_origin, 0, strlen(private_data->current_origin));
  if(mb->origin.cur_origin)
    sprintf(private_data->current_origin, "%s", mb->origin.cur_origin);

  if(mb->ip_name.label.label_str) {

    lbl.label             = mb->ip_name.label.label_str;
    lbl.skin_element_name = mb->ip_name.label.skin_element_name;
    lbl.window            = private_data->widget_list->win;
    lbl.gc                = private_data->widget_list->gc;
    xitk_list_append_content(private_data->widget_list->l, 
			    xitk_label_create (skonfig, &lbl));
  }

  /*
   * Create buttons with input plugins names.
   */
  {
    int x, y;
    int i = 0;
    
    private_data->skin_element_name_ip = strdup(mb->ip_name.button.skin_element_name);
    x = xitk_skin_get_coord_x(skonfig, mb->ip_name.button.skin_element_name);
    y = xitk_skin_get_coord_y(skonfig, mb->ip_name.button.skin_element_name);
    
    while(mb->ip_availables[i] != NULL) {

      lb.button_type    = CLICK_BUTTON;
      lb.label          = mb->ip_availables[i];
      lb.callback       = mrlbrowser_grab_mrls;
      lb.state_callback = NULL;
      lb.userdata       = (void *)private_data;
      lb.skin_element_name = mb->ip_name.button.skin_element_name;
      xitk_list_append_content(private_data->widget_list->l,
			       (private_data->autodir_plugins[i] = xitk_labelbutton_create (skonfig, &lb)));
      xitk_set_widget_tips(private_data->autodir_plugins[i], 
			   xine_get_input_plugin_description(mb->xine, mb->ip_availables[i]));
      
      (void) xitk_set_widget_pos(private_data->autodir_plugins[i], x, y);

      y += xitk_get_widget_height(private_data->autodir_plugins[i]) + 1;
      i++;
    }
    if(i)
      private_data->autodir_plugins[i+1] = NULL;
  }
    
  private_data->visible        = 1;
  
  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->imlibdata          = private_data->imlibdata;
  mywidget->x                  = mb->x;
  mywidget->y                  = mb->y;
  mywidget->width              = private_data->bg_image->width;
  mywidget->height             = private_data->bg_image->height;
  mywidget->widget_type        = WIDGET_TYPE_MRLBROWSER | WIDGET_TYPE_GROUP;
  mywidget->paint              = NULL;
  mywidget->notify_click       = NULL;
  mywidget->notify_focus       = NULL;
  mywidget->notify_keyevent    = NULL;
  mywidget->notify_inside      = NULL;
  mywidget->notify_change_skin = NULL;
  mywidget->notify_destroy     = notify_destroy;
  mywidget->get_skin           = NULL;

  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  xitk_browser_update_list(private_data->mrlb_list, 
			   private_data->mc->mrls_disp, 
			   private_data->mrls_num, 0);
  
  XLOCK (mb->imlibdata->x.disp);
  XMapRaised(mb->imlibdata->x.disp, private_data->window); 
  XUNLOCK (mb->imlibdata->x.disp);

  private_data->widget_key = 
    xitk_register_event_handler("mrl browser",
				private_data->window, 
				mrlbrowser_handle_event,
				NULL,
				mb->dndcallback,
				private_data->widget_list,
				(void *) private_data);

  return mywidget;
}

#endif
