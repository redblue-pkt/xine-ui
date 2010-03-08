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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * lirc-specific stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

#ifdef DEBUG
#define LIRC_VERBOSE 1
#else
#define LIRC_VERBOSE 0
#endif


extern _panel_t        *panel;

static struct {
  struct lirc_config   *config;
  int                   fd;
  pthread_t             thread;
} lirc;

static void lirc_get_playlist(char *from) {
  int    i;
  char **autoscan_plugins = (char **)xine_get_autoplay_input_plugin_ids(__xineui_global_xine_instance);

  for(i = 0; autoscan_plugins[i] != NULL; ++i) {
    if(!strcasecmp(autoscan_plugins[i], from)) {
      int    num_mrls;
      int    j;
      char **autoplay_mrls = (char **)xine_get_autoplay_mrls (__xineui_global_xine_instance, from, &num_mrls);
      if(autoplay_mrls) {

	/* First, free existing playlist */
	mediamark_free_mediamarks();
	
	for (j = 0; j < num_mrls; j++)
	  mediamark_append_entry((const char *)autoplay_mrls[j], 
				 (const char *)autoplay_mrls[j], NULL, 0, -1, 0, 0);
	
	gGui->playlist.cur = 0;
	gui_set_current_mmk(mediamark_get_current_mmk());
	playlist_update_playlist();
      }    
    }
  }
}

static __attribute__((noreturn)) void *xine_lirc_loop(void *dummy) {
  char             *code, *c;
  int               ret;
  kbinding_entry_t *k;
  fd_set            set;
  struct timeval    tv;

  while(gGui->running) {

    FD_ZERO(&set);
    FD_SET(lirc.fd, &set);
    
    tv.tv_sec  = 0;
    tv.tv_usec = 500000;

    select(lirc.fd + 1, &set, NULL, NULL, &tv);
    
    while((ret = lirc_nextcode(&code)) == 0) {

      if(code == NULL) 
	break;
      
      while((ret = lirc_code2char(lirc.config, code, &c)) == 0 
	    && c != NULL) {
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif
	
	k = kbindings_lookup_action(gGui->kbindings, c);
	
	if(k) {
	  XLockDisplay(gGui->display);
	  gui_execute_action_id((kbindings_get_action_id(k)));
	  XUnlockDisplay(gGui->display);
	}
	else {
	  char from[256];
	  
	  memset(&from, 0, sizeof(from));
	  if(sscanf(c, "PlaylistFrom:%s", &from[0]) == 1) {
	    if(strlen(from))
	      lirc_get_playlist(from);   
	  }
	}
      }
      
      if(panel_is_visible()) {
	XLockDisplay(gGui->display);
	xitk_paint_widget_list (panel->widget_list);
	XUnlockDisplay(gGui->display);
      }
      
      free(code);
      
      if(ret == -1) 
	break;

    }
    if(ret == -1) 
      break;
  }
  
  pthread_exit(NULL);
}

void lirc_start(void) {
  int flags;
  int err;
  
  if((lirc.fd = lirc_init("xine", LIRC_VERBOSE)) == -1) {
    __xineui_global_lirc_enable = 0;
    return;
  }
  
  fcntl(lirc.fd, F_SETOWN, getpid());
  flags = fcntl(lirc.fd, F_GETFL, 0);
  if(flags != -1)
    fcntl(lirc.fd, F_SETFL, flags|O_NONBLOCK);
  
  if(lirc_readconfig(NULL, &lirc.config, NULL) != 0) {
    __xineui_global_lirc_enable = 0;
    lirc_deinit();
    return;
  }

  if((err = pthread_create(&(lirc.thread), NULL, xine_lirc_loop, NULL)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    lirc_freeconfig(lirc.config);
    lirc_deinit();
    __xineui_global_lirc_enable = 0;
  }
}

void lirc_stop(void) {
  pthread_join(lirc.thread, NULL);
  lirc_freeconfig(lirc.config);
  lirc_deinit();
}
