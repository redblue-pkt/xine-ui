/* 
 * Copyright (C) 2000-2003 the xine project
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

#ifdef HAVE_ALLOCA_H   
#include <alloca.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <xine.h>

#include "_xitk.h"

#undef DEBUG_MRLB

static xitk_mrlbrowser_filter_t __mrl_filters[] = {
  { "All"                 , "*"            },
  { "*.vob"               , "vob"          }, /* mpeg block */
  { "*.mpv"               , "mpv"          }, /* elementary */
  { "*.mpg, *.mpeg, *.mpe", "mpg,mpeg,mpe" }, /* mpeg */
  { "*.avi"               , "avi"          }, /* avi */
  { "*.mp3"               , "mp3"          }, /* mp3 */
  { "*.asf, *.wmv"        , "asf,wmv"      }, /* asf */
  { "*.cpk, *.cak, *.film", "cpk,cak,film" }, /* film */
  { "*.ogg"               , "ogg"          }, /* ogg */
  { "*.vdr"               , "vdr"          }, /* pes */
  { "*.mov, *.mp4, *.qt"  , "mov,mp4,qt"   }, /* QT */
  { "*.roq"               , "roq"          }, /* roq */
  { "*.m2t, *.ts, *.trp"  , "m2t,ts,trp"   }, /* ts */
  { NULL                  , NULL           }
};

/*
 *
 */
static void _duplicate_mrl_filters(mrlbrowser_private_data_t *private_data, 
				   xitk_mrlbrowser_filter_t **mrl_f) {
  xitk_mrlbrowser_filter_t **mrl_filters = mrl_f;
  int                        i = 0;
  
  private_data->filters_num = 0;

  if(mrl_f == NULL) {
    int len = sizeof(__mrl_filters) / sizeof(xitk_mrlbrowser_filter_t);

    private_data->mrl_filters = (xitk_mrlbrowser_filter_t **) 
      xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t *) * len);

    for(i = 0; i < (len - 1); i++) {
      
      private_data->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *) 
	xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
      private_data->mrl_filters[i]->name   = strdup(__mrl_filters[i].name);
      private_data->mrl_filters[i]->ending = strdup(__mrl_filters[i].ending);

    }
  }
  else {
    
    private_data->mrl_filters = (xitk_mrlbrowser_filter_t **) 
      xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t *));
    
    for(i = 0; 
	mrl_filters[i] && (mrl_filters[i]->name && mrl_filters[i]->ending); i++) {

      private_data->mrl_filters            = (xitk_mrlbrowser_filter_t **)
	realloc(private_data->mrl_filters, sizeof(xitk_mrlbrowser_filter_t *) * (i + 2));
      private_data->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *) 
	xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
      private_data->mrl_filters[i]->name   = strdup(mrl_filters[i]->name);
      private_data->mrl_filters[i]->ending = strdup(mrl_filters[i]->ending);
    }
  }
  
  private_data->filters_num            = i;
  private_data->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *) 
    xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
  private_data->mrl_filters[i]->name   = NULL;
  private_data->mrl_filters[i]->ending = NULL;
  
  private_data->filters = (char **) xitk_xmalloc(sizeof(char *) * (private_data->filters_num + 1));
  
  for(i = 0; private_data->mrl_filters[i]->name != NULL; i++)
    private_data->filters[i] = private_data->mrl_filters[i]->name;
  
  private_data->filters[i] = NULL;
  private_data->filter_selected = 0;
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    
    xitk_mrlbrowser_destroy(w);
  }
}


static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  }
  
  return retval;
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
  
  xitk_label_change_label (private_data->widget_origin, 
			   private_data->current_origin);
  
}

/*
 * Return the *marker* of a given file *fully named*
 */
static int get_mrl_marker(xine_mrl_t *mrl) {
  
  if(mrl->type & XINE_MRL_TYPE_file_symlink)
    return '@';
  else if(mrl->type & XINE_MRL_TYPE_file_directory)
    return '/';
  else if(mrl->type & XINE_MRL_TYPE_file_fifo)
    return '|';
  else if(mrl->type & XINE_MRL_TYPE_file_sock)
    return '=';
  else if(mrl->type & XINE_MRL_TYPE_file_normal) {
    if(mrl->type & XINE_MRL_TYPE_file_exec)
      return '*';
  }

  return '\0';
}

/*
 *
 */
static void mrlbrowser_filter_mrls(mrlbrowser_private_data_t *private_data) {
  int i;
  int old_filtered_mrls_num = private_data->mc->mrls_to_disp;

  /* filtering is enabled */
  if(private_data->filter_selected) {
    char *filter_ends;
    char *p, *m;
    char *ending;

    private_data->mc->mrls_to_disp = 0;

    filter_ends = strdup(private_data->mrl_filters[private_data->filter_selected]->ending);
    xitk_strdupa(p, filter_ends);

    for(i = 0; i < private_data->mrls_num; i++) {

      if(private_data->mc->mrls[i]->type & XINE_MRL_TYPE_file_directory) {

	if(private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp] == NULL)
	  private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp] = (xine_mrl_t *) xitk_xmalloc(sizeof(xine_mrl_t));

	MRL_DUPLICATE(private_data->mc->mrls[i], private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp]);
	private_data->mc->mrls_to_disp++;
      }
      else {
	ending = strrchr(private_data->mc->mrls[i]->mrl, '.');
	
	if(ending) {

	  while((m = xitk_strsep(&filter_ends, ",")) != NULL) {
	    
	    if(!strcasecmp((ending + 1), m)) {
	      if(private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp] == NULL)
		private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp] = (xine_mrl_t *) xitk_xmalloc(sizeof(xine_mrl_t));
	      
	      MRL_DUPLICATE(private_data->mc->mrls[i], private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp]);
	      private_data->mc->mrls_to_disp++;
	    }
	  }
	  free(filter_ends);
	  filter_ends = strdup(p);
	}
      }    
    }
    
    if(filter_ends)
      free(filter_ends);
    
  }
  else { /* no filtering */

    for(i = 0; i < private_data->mrls_num; i++) {
      
      if(private_data->mc->filtered_mrls[i] == NULL)
	private_data->mc->filtered_mrls[i] = (xine_mrl_t *) xitk_xmalloc(sizeof(xine_mrl_t));
      
      MRL_DUPLICATE(private_data->mc->mrls[i], private_data->mc->filtered_mrls[i]);
    }
    private_data->mc->mrls_to_disp = i;
  }
  
  /* free unused mrls */
  while(old_filtered_mrls_num > private_data->mc->mrls_to_disp) {
    MRL_ZERO(private_data->mc->filtered_mrls[old_filtered_mrls_num - 1]);
    XITK_FREE(private_data->mc->filtered_mrls[old_filtered_mrls_num - 1]);
    private_data->mc->filtered_mrls[old_filtered_mrls_num - 1] = NULL;
    old_filtered_mrls_num--;
  }

  private_data->mc->filtered_mrls[private_data->mc->mrls_to_disp] = NULL;
}

/*
 * Create *eye candy* entries in browser.
 */
static void mrlbrowser_create_enlighted_entries(mrlbrowser_private_data_t *private_data) {
  int i = 0;
  
  mrlbrowser_filter_mrls(private_data);

  while(private_data->mc->filtered_mrls[i]) {
    char *p = private_data->mc->filtered_mrls[i]->mrl;

    if(private_data->mc->filtered_mrls[i]->origin) {

      p += strlen(private_data->mc->filtered_mrls[i]->origin);
      
      if(*p == '/') 
	p++;
    }
    
    if(private_data->mc->filtered_mrls[i]->type & XINE_MRL_TYPE_file_symlink) {

      if(private_data->mc->mrls_disp[i])
	private_data->mc->mrls_disp[i] = (char *) 
	  realloc(private_data->mc->mrls_disp[i], strlen(p) + 4 + 
		  strlen(private_data->mc->filtered_mrls[i]->link) + 2);
      else
	private_data->mc->mrls_disp[i] = (char *) 
	  xitk_xmalloc(strlen(p) + 4 + 
		      strlen(private_data->mc->filtered_mrls[i]->link) + 2);
      
      sprintf(private_data->mc->mrls_disp[i], "%s%c -> %s", 
	      p, get_mrl_marker(private_data->mc->filtered_mrls[i]), 
	      private_data->mc->filtered_mrls[i]->link);

    }
    else {
      if(private_data->mc->mrls_disp[i])
	private_data->mc->mrls_disp[i] = (char *) 
	  realloc(private_data->mc->mrls_disp[i], strlen(p) + 2);
      else
	private_data->mc->mrls_disp[i] = (char *) xitk_xmalloc(strlen(p) + 2);
      
      sprintf(private_data->mc->mrls_disp[i], "%s%c", 
	      p, get_mrl_marker(private_data->mc->filtered_mrls[i]));
    }

    i++;
  }
}

/*
 * Duplicate mrls from xine-engine's returned. We shouldn't play
 * directly with these ones.
 */
static void mrlbrowser_duplicate_mrls(mrlbrowser_private_data_t *private_data,
				      xine_mrl_t **mtmp, int num_mrls) {
  int i;
  int old_mrls_num = private_data->mrls_num;

  for (i = 0; i < num_mrls; i++) {
    if(private_data->mc->mrls[i] == NULL)
      private_data->mc->mrls[i] = (xine_mrl_t *) xitk_xmalloc(sizeof(xine_mrl_t));

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
      xine_mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					       private_data->last_mrl_source, 
					       NULL, &num_mrls);
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
			     (const char* const*)private_data->mc->mrls_disp, NULL,
			     private_data->mc->mrls_to_disp, 0);
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
    xine_mrl_t *ms = private_data->mc->mrls[j];
    
    printf("mrl '%s'\n\t+", ms->mrl);

    if(ms->type & XINE_MRL_TYPE_file_symlink)
      printf(" symlink to '%s' \n\t+ ", ms->link);

    printf("[");

    if(ms->type & XINE_MRL_TYPE_unknown)
      printf(" unknown ");
    
      if(ms->type & XINE_MRL_TYPE_dvd)
	printf(" dvd ");
      
      if(ms->type & XINE_MRL_TYPE_vcd)
	    printf(" vcd ");
      
      if(ms->type & XINE_MRL_TYPE_net)
	printf(" net ");
      
      if(ms->type & XINE_MRL_TYPE_rtp)
	printf(" rtp ");
      
      if(ms->type & XINE_MRL_TYPE_stdin)
	printf(" stdin ");
      
      if(ms->type & XINE_MRL_TYPE_file)
	printf(" file ");
      
      if(ms->type & XINE_MRL_TYPE_file_fifo)
	printf(" fifo ");
      
      if(ms->type & XINE_MRL_TYPE_file_chardev)
	printf(" chardev ");
      
      if(ms->type & XINE_MRL_TYPE_file_directory)
	printf(" directory ");
      
      if(ms->type & XINE_MRL_TYPE_file_blockdev)
	printf(" blockdev ");
      
      if(ms->type & XINE_MRL_TYPE_file_normal)
	printf(" normal ");
      
      if(ms->type & XINE_MRL_TYPE_file_sock)
	printf(" sock ");
      
      if(ms->type & XINE_MRL_TYPE_file_exec)
	printf(" exec ");
      
      if(ms->type & XINE_MRL_TYPE_file_backup)
	printf(" backup ");
      
      if(ms->type & XINE_MRL_TYPE_file_hidden)
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
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

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
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

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

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

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
 
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

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

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

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

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {

    private_data = (mrlbrowser_private_data_t *)w->private_data;

    if(private_data->visible) {
      private_data->visible = 0;
      xitk_hide_widgets(private_data->widget_list);
      XLOCK(private_data->imlibdata->x.disp);
      XUnmapWindow(private_data->imlibdata->x.disp, private_data->window);
      XUNLOCK(private_data->imlibdata->x.disp);
    }
  }
}

/*
 * Show mrlbrowser.
 */
void xitk_mrlbrowser_show(xitk_widget_t *w) {
  mrlbrowser_private_data_t *private_data;

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    
    private_data->visible = 1;
    xitk_show_widgets(private_data->widget_list);
    XLOCK(private_data->imlibdata->x.disp);
    XRaiseWindow(private_data->imlibdata->x.disp, private_data->window); 
    XMapWindow(private_data->imlibdata->x.disp, private_data->window); 
    XUNLOCK(private_data->imlibdata->x.disp);
  }
}

/*
 * Set mrlbrowser transient for hints for given window.
 */
void xitk_mrlbrowser_set_transient(xitk_widget_t *w, Window window) {
  mrlbrowser_private_data_t *private_data;

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET)) && (window != None)) {
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

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) && 
	   (w->type & WIDGET_GROUP_WIDGET))) {
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
      
      for(i = 0; i <= private_data->filters_num; i++) {
	XITK_FREE(private_data->mrl_filters[i]->name);
	XITK_FREE(private_data->mrl_filters[i]->ending);
	XITK_FREE(private_data->mrl_filters[i]);
      }
      XITK_FREE(private_data->mrl_filters);
      
      for(i = 0; i < private_data->mrls_num; i++) {
	MRL_ZERO(private_data->mc->mrls[i]);
	XITK_FREE(private_data->mc->mrls[i]);
	MRL_ZERO(private_data->mc->filtered_mrls[i]);
	XITK_FREE(private_data->mc->filtered_mrls[i]);
	XITK_FREE(private_data->mc->mrls_disp[i]);
      }
      
      free(private_data->filters);
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

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    private_data = (mrlbrowser_private_data_t *)w->private_data;
    
    xitk_skin_lock(skonfig);
    xitk_hide_widgets(private_data->widget_list);

    XLOCK(private_data->imlibdata->x.disp);
    
    if(!(new_img = Imlib_load_image(private_data->imlibdata,
				    xitk_skin_get_skin_filename(skonfig, 
							private_data->skin_element_name)))) {
      XITK_DIE("%s(): couldn't find image for background\n", __FUNCTION__);
    }
    XUNLOCK(private_data->imlibdata->x.disp);

    hint.width  = new_img->rgb_width;
    hint.height = new_img->rgb_height;
    hint.flags  = PPosition | PSize;

    XLOCK(private_data->imlibdata->x.disp);
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
      int dir, max;
      
      x   = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name_ip);
      y   = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name_ip);
      dir = xitk_skin_get_direction(skonfig, private_data->skin_element_name_ip);
      max = xitk_skin_get_max_buttons(skonfig, private_data->skin_element_name_ip);

      switch(dir) {
      case DIRECTION_UP:
      case DIRECTION_DOWN:
	while((max && ((i < max) && (private_data->autodir_plugins[i] != NULL))) || 
	      (!max && private_data->autodir_plugins[i] != NULL)) {

	  (void) xitk_set_widget_pos(private_data->autodir_plugins[i], x, y);
	  
	  if(dir == DIRECTION_DOWN)
	    y += xitk_get_widget_height(private_data->autodir_plugins[i]) + 1;
	  else
	    y -= (xitk_get_widget_height(private_data->autodir_plugins[i]) + 1);
	  
	  i++;
	}
	break;
      case DIRECTION_LEFT:
      case DIRECTION_RIGHT:
	while((max && ((i < max) && (private_data->autodir_plugins[i] != NULL))) || 
	      (!max && private_data->autodir_plugins[i] != NULL)) {
	  
	  (void) xitk_set_widget_pos(private_data->autodir_plugins[i], x, y);
	  
	  if(dir == DIRECTION_RIGHT)
	    x += xitk_get_widget_width(private_data->autodir_plugins[i]) + 1;
	  else
	    x -= (xitk_get_widget_width(private_data->autodir_plugins[i]) + 1);
	  
	  i++;
	}
	break;
      }

      if(max && (i < max)) {
	while(private_data->autodir_plugins[i] != NULL) {
	  xitk_disable_and_hide_widget(private_data->autodir_plugins[i]);
	  i++;
	}
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
  xine_mrl_t *ms = private_data->mc->filtered_mrls[j];
  char   buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  
  memset(&buf, '\0', sizeof(buf));
  sprintf(buf, "%s", ms->mrl);
  
  if((ms->type & XINE_MRL_TYPE_file) && (ms->type & XINE_MRL_TYPE_file_directory)) {
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
      xine_mrl_t **mtmp = xine_get_browse_mrls(private_data->xine, 
					       private_data->last_mrl_source, 
					       buf, &num_mrls);

      mrlbrowser_duplicate_mrls(private_data, mtmp, num_mrls);

    }
    
    update_current_origin(private_data);
    mrlbrowser_create_enlighted_entries(private_data);
    xitk_browser_update_list(private_data->mrlb_list, 
			     (const char* const*)private_data->mc->mrls_disp, NULL,
			     private_data->mc->mrls_to_disp, 0);
    
  }
  else {
    
    xitk_browser_release_all_buttons(private_data->mrlb_list);

    if(add_callback && private_data->add_callback)
      private_data->add_callback(NULL, (void *) j, 
				 private_data->mc->filtered_mrls[j]);
    
    if(play_callback && private_data->play_callback)
      private_data->play_callback(NULL, (void *) j,
				  private_data->mc->filtered_mrls[j]);
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
  
  if((modifier & MODIFIER_CTRL) && (modifier & MODIFIER_SHIFT))
    mrlbrowser_select_mrl(private_data, selected, 1, 1);
  else if(modifier & MODIFIER_CTRL)
    mrlbrowser_select_mrl(private_data, selected, 1, 0);
  else
    mrlbrowser_select_mrl(private_data, selected, 0, 1); 

}

/*
 * Refresh filtered list
 */
static void combo_filter_select(xitk_widget_t *w, void *data, int select) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;

  private_data->filter_selected = select;
  mrlbrowser_create_enlighted_entries(private_data);
  xitk_browser_update_list(private_data->mrlb_list, 
			   (const char* const*) private_data->mc->mrls_disp, NULL,
  			   private_data->mc->mrls_to_disp, 0);
}

/*
 * Handle here mrlbrowser events.
 */
static void mrlbrowser_handle_event(XEvent *event, void *data) {
  mrlbrowser_private_data_t *private_data = (mrlbrowser_private_data_t *)data;
  
  switch(event->type) {

  case MappingNotify:
    XLOCK(private_data->imlibdata->x.disp);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUNLOCK(private_data->imlibdata->x.disp);
    break;
    
  case KeyPress: {
    KeySym         mkey;
    int            modifier;

    xitk_get_key_modifier(event, &modifier);
    mkey = xitk_get_key_pressed(event);
    
    switch (mkey) {

    case XK_d: 
    case XK_D:
#ifdef DEBUG_MRLB
      /* This is for debugging purpose */
      if(modifier & MODIFIER_CTRL)
	mrlbrowser_dumpmrl(NULL, (void *)private_data);
#endif
      break;

    case XK_s:
    case XK_S:
      if(modifier & MODIFIER_CTRL)
	mrlbrowser_select(NULL, (void *)private_data);
      break;

    case XK_Return: {
      int selected;
      
      if((selected = xitk_browser_get_current_selected(private_data->mrlb_list)) >= 0)
	mrlbrowser_select_mrl(private_data, selected, 0, 1);
    }
    break;

    case XK_Escape:
      xitk_mrlbrowser_exit(NULL, (void *)private_data->fbWidget);
      break;
      
    }
  }
  break;

  }
}

/*
 * Create mrlbrowser window.
 */
xitk_widget_t *xitk_mrlbrowser_create(xitk_widget_list_t *wl,
				      xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb) {
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
  xitk_widget_t               *default_source = NULL;
  xitk_button_widget_t        pb;
  xitk_label_widget_t         lbl;
  xitk_combo_widget_t         cmb;
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
  XITK_WIDGET_INIT(&cmb, mb->imlibdata);
  
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
  XUNLOCK(mb->imlibdata->x.disp);

  private_data->mc              = (mrl_contents_t *) xitk_xmalloc(sizeof(mrl_contents_t));
  private_data->mrls_num        = 0;
  private_data->last_mrl_source = NULL;

  hint.x      = mb->x;
  hint.y      = mb->y;
  hint.width  = private_data->bg_image->rgb_width;
  hint.height = private_data->bg_image->rgb_height;
  hint.flags  = PPosition | PSize;

  XLOCK(mb->imlibdata->x.disp);
  screen = DefaultScreen(mb->imlibdata->x.disp);

  XAllocNamedColor(mb->imlibdata->x.disp, 
		   Imlib_get_colormap(mb->imlibdata),
		   "black", &black, &dummy);
  XUNLOCK(mb->imlibdata->x.disp);

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

  XLOCK(mb->imlibdata->x.disp);
  private_data->window   = XCreateWindow (mb->imlibdata->x.disp,
					  mb->imlibdata->x.root, 
					  hint.x, hint.y, hint.width, hint.height, 0, 
					  mb->imlibdata->x.depth, InputOutput, 
					  mb->imlibdata->x.visual,
					  CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect,
					  &attr);

  XmbSetWMProperties(mb->imlibdata->x.disp, private_data->window, title, title,
                     NULL, 0, &hint, NULL, NULL);

  XSelectInput(mb->imlibdata->x.disp, private_data->window, INPUT_MOTION | KeymapStateMask);
  XUNLOCK(mb->imlibdata->x.disp);

  if(mb->set_wm_window_normal)
    xitk_set_wm_window_type(private_data->window, WINDOW_TYPE_NORMAL);
  else
    xitk_unset_wm_window_type(private_data->window, WINDOW_TYPE_NORMAL);

  /*
   * layer above most other things, like gnome panel
   * WIN_LAYER_ABOVE_DOCK  = 10
   *
   */
  XLOCK(mb->imlibdata->x.disp);
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
  XUNLOCK(mb->imlibdata->x.disp);

  mwmhints.flags       = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XLOCK(mb->imlibdata->x.disp);
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
    XFree(xclasshint);
  }
  XUNLOCK(mb->imlibdata->x.disp);
  
  XLOCK(mb->imlibdata->x.disp);
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    if(mb->icon) {
      wm_hint->icon_pixmap = *mb->icon;
      wm_hint->flags       = IconPixmapHint;
    }
    wm_hint->flags         |= InputHint | StateHint;
    XSetWMHints(mb->imlibdata->x.disp, private_data->window, wm_hint);
    XFree(wm_hint);
  }
  XUNLOCK(mb->imlibdata->x.disp);
  
  XLOCK(mb->imlibdata->x.disp);
  gc = XCreateGC(mb->imlibdata->x.disp, private_data->window, 0, 0);
  
  Imlib_apply_image(mb->imlibdata, 
		    private_data->bg_image, private_data->window);

  XUNLOCK (mb->imlibdata->x.disp);

  private_data->widget_list                = xitk_widget_list_new() ;
  private_data->widget_list->l             = xitk_list_new ();
  private_data->widget_list->win           = private_data->window;
  private_data->widget_list->gc            = gc;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = mb->select.caption;
  lb.callback          = mrlbrowser_select;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)private_data;
  lb.skin_element_name = mb->select.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
		   (w = xitk_labelbutton_create (private_data->widget_list, skonfig, &lb)));
  w->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;
  xitk_set_widget_tips(w, _("Select current entry"));
  
  pb.skin_element_name = mb->play.skin_element_name;
  pb.callback          = mrlbrowser_play;
  pb.userdata          = (void *)private_data;
  xitk_list_append_content(private_data->widget_list->l,
		   (w = xitk_button_create (private_data->widget_list, skonfig, &pb)));
  w->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;
  xitk_set_widget_tips(w, _("Play selected entry"));

  lb.button_type       = CLICK_BUTTON;
  lb.label             = mb->dismiss.caption;
  lb.callback          = xitk_mrlbrowser_exit;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)mywidget;
  lb.skin_element_name = mb->dismiss.skin_element_name;
  xitk_list_append_content(private_data->widget_list->l,
		   (w = xitk_labelbutton_create (private_data->widget_list, skonfig, &lb)));
  w->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;
  xitk_set_widget_tips(w, _("Close MRL browser window"));
  
  private_data->add_callback      = mb->select.callback;
  private_data->play_callback     = mb->play.callback;
  private_data->kill_callback     = mb->kill.callback;
  mb->browser.dbl_click_callback  = handle_dbl_click;
  mb->browser.userdata            = (void *)private_data;
  mb->browser.parent_wlist        = private_data->widget_list;
  xitk_list_append_content (private_data->widget_list->l,
		    (private_data->mrlb_list = 
		     xitk_browser_create(private_data->widget_list, skonfig, &mb->browser)));
  private_data->mrlb_list->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;

  lbl.label             = "";
  lbl.skin_element_name = mb->origin.skin_element_name;
  lbl.window            = private_data->widget_list->win;
  lbl.gc                = private_data->widget_list->gc;
  lbl.callback          = NULL;
  xitk_list_append_content(private_data->widget_list->l,
			  (private_data->widget_origin = 
			   xitk_label_create(private_data->widget_list, skonfig, &lbl)));
  private_data->widget_origin->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;
  
  memset(&private_data->current_origin, 0, strlen(private_data->current_origin));
  if(mb->origin.cur_origin)
    sprintf(private_data->current_origin, "%s", mb->origin.cur_origin);

  if(mb->ip_name.label.label_str) {

    lbl.label             = mb->ip_name.label.label_str;
    lbl.skin_element_name = mb->ip_name.label.skin_element_name;
    lbl.window            = private_data->widget_list->win;
    lbl.gc                = private_data->widget_list->gc;
    lbl.callback          = NULL;
    xitk_list_append_content(private_data->widget_list->l, 
			     (w = xitk_label_create (private_data->widget_list, skonfig, &lbl)));
    w->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;

  }

  /*
   * Create buttons with input plugins names.
   */
  {
    int x, y;
    int i = 0;
    int dir, max;
    
    private_data->skin_element_name_ip = strdup(mb->ip_name.button.skin_element_name);
    x   = xitk_skin_get_coord_x(skonfig, mb->ip_name.button.skin_element_name);
    y   = xitk_skin_get_coord_y(skonfig, mb->ip_name.button.skin_element_name);
    dir = xitk_skin_get_direction(skonfig, mb->ip_name.button.skin_element_name);
    max = xitk_skin_get_max_buttons(skonfig, mb->ip_name.button.skin_element_name);

    while(mb->ip_availables[i] != NULL) {

      lb.button_type    = CLICK_BUTTON;
      lb.label          = mb->ip_availables[i];
      lb.callback       = mrlbrowser_grab_mrls;
      lb.state_callback = NULL;
      lb.userdata       = (void *)private_data;
      lb.skin_element_name = mb->ip_name.button.skin_element_name;

      xitk_list_append_content(private_data->widget_list->l,
		       (private_data->autodir_plugins[i] = 
			xitk_labelbutton_create (private_data->widget_list, skonfig, &lb)));
      xitk_set_widget_tips(private_data->autodir_plugins[i], 
			   (char *) xine_get_input_plugin_description(mb->xine, mb->ip_availables[i]));
      private_data->autodir_plugins[i]->type |= WIDGET_GROUP | WIDGET_GROUP_MRLBROWSER;
      
      (void) xitk_set_widget_pos(private_data->autodir_plugins[i], x, y);

      /* make "file" the default mrl source */
      if( !strcasecmp(lb.label, "file") )
        default_source = private_data->autodir_plugins[i];
      
      switch(dir) {
      case DIRECTION_UP:
      case DIRECTION_DOWN:
	if(dir == DIRECTION_DOWN)
	  y += xitk_get_widget_height(private_data->autodir_plugins[i]) + 1;
	else
	  y -= (xitk_get_widget_height(private_data->autodir_plugins[i]) + 1);
	break;
      case DIRECTION_LEFT:
      case DIRECTION_RIGHT:
	if(dir == DIRECTION_RIGHT)
	  x += xitk_get_widget_width(private_data->autodir_plugins[i]) + 1;
	else
	  x -= (xitk_get_widget_width(private_data->autodir_plugins[i]) + 1);
	break;
      }
      
      if(max && (i >= max))
	xitk_disable_and_hide_widget(private_data->autodir_plugins[i]);
      
      i++;
    }
    if(i)
      private_data->autodir_plugins[i+1] = NULL;
  }

  _duplicate_mrl_filters(private_data, mb->mrl_filters);

  cmb.skin_element_name = mb->combo.skin_element_name;
  cmb.layer_above       = mb->layer_above;
  cmb.parent_wlist      = private_data->widget_list;
  cmb.entries           = private_data->filters;
  cmb.parent_wkey       = &private_data->widget_key;
  cmb.callback          = combo_filter_select;
  cmb.userdata          = (void *)private_data;
  xitk_list_append_content(private_data->widget_list->l, 
		   (private_data->combo_filter = 
		    xitk_combo_create(private_data->widget_list, skonfig, &cmb, NULL, NULL)));

  private_data->visible        = 1;
  
  mywidget->wl                 = NULL;

  mywidget->enable             = 1;
  mywidget->running            = 1;
  mywidget->visible            = 1;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->imlibdata          = private_data->imlibdata;
  mywidget->x                  = mb->x;
  mywidget->y                  = mb->y;
  mywidget->width              = private_data->bg_image->width;
  mywidget->height             = private_data->bg_image->height;
  mywidget->type               = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_MRLBROWSER;
  mywidget->event              = notify_event;
  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  xitk_browser_update_list(private_data->mrlb_list, 
			   (const char* const*)private_data->mc->mrls_disp, NULL,
			   private_data->mrls_num, 0);

  XLOCK (mb->imlibdata->x.disp);
  XRaiseWindow(mb->imlibdata->x.disp, private_data->window); 
  XMapWindow(mb->imlibdata->x.disp, private_data->window); 
  XUNLOCK (mb->imlibdata->x.disp);

  private_data->widget_key = 
    xitk_register_event_handler("mrl browser",
				private_data->window, 
				mrlbrowser_handle_event,
				NULL,
				mb->dndcallback,
				private_data->widget_list,(void *) private_data);

  while(!xitk_is_window_visible(mb->imlibdata->x.disp, private_data->window))
    xitk_usec_sleep(5000);
  
  xitk_mrlbrowser_change_skins(mywidget, skonfig);
  
  XLOCK(mb->imlibdata->x.disp);
  XSetInputFocus(mb->imlibdata->x.disp, private_data->window, RevertToParent, CurrentTime);
  XUNLOCK(mb->imlibdata->x.disp);

  if( default_source && !private_data->last_mrl_source ) {
    mrlbrowser_grab_mrls(default_source, private_data);
  }

  return mywidget;
}

#endif
