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

#include "common.h"

#ifdef HAVE_LIRC

#ifdef DEBUG
#define LIRC_VERBOSE 1
#else
#define LIRC_VERBOSE 0
#endif

extern gGui_t          *gGui;

extern _panel_t        *panel;

typedef struct {
  struct lirc_config   *config;
  int                   fd;
  pthread_t             thread;
} _lirc_t;

static _lirc_t lirc;

static void lirc_get_playlist(char *from) {
  int    i;
  char **autoscan_plugins = (char **)xine_get_autoplay_input_plugin_ids(gGui->xine);

  for(i = 0; autoscan_plugins[i] != NULL; ++i) {
    if(!strcasecmp(autoscan_plugins[i], from)) {
      int    num_mrls;
      int    j;
      char **autoplay_mrls = (char **)xine_get_autoplay_mrls (gGui->xine, from, &num_mrls);
      if(autoplay_mrls) {

	/* First, free existing playlist */
	mediamark_free_mediamarks();
	
	for (j = 0; j < num_mrls; j++)
	  mediamark_add_entry((const char *)autoplay_mrls[j], 
			      (const char *)autoplay_mrls[j], NULL, 0, -1, 0);
	
	gGui->playlist.cur = 0;
	gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
	playlist_update_playlist();
      }    
    }
  }
}

void *xine_lirc_loop(void *dummy) {
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
    
    while(lirc_nextcode(&code) == 0) {

      if(code == NULL) 
	break;
      
      while((ret = lirc_code2char(lirc.config, code, &c)) == 0 
	    && c != NULL) {
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif
	
	k = kbindings_lookup_action(gGui->kbindings, c);
	
	if(k)
	  gui_execute_action_id((kbindings_get_action_id(k)));
	else {
	  char from[256];
	  
	  memset(&from, 0, sizeof(from));
	  if(sscanf(c, "PlaylistFrom:%s", &from[0]) == 1) {
	    if(strlen(from))
	      lirc_get_playlist(from);   
	  }
	}
      }
      
      if(panel_is_visible())
	xitk_paint_widget_list (panel->widget_list);
      
      if(code)
	free(code);
      
      if(ret == -1) 
	break;

    }
  }
  
  pthread_exit(NULL);
}

void init_lirc(void) {
  int flags;
  
  if((lirc.fd = lirc_init("xine", LIRC_VERBOSE)) == -1) {
    gGui->lirc_enable = 0;
    return;
  }
  
  fcntl(lirc.fd, F_SETOWN, getpid());
  flags = fcntl(lirc.fd, F_GETFL, 0);
  if(flags != -1)
    fcntl(lirc.fd, F_SETFL, flags|O_NONBLOCK);
  
  if(lirc_readconfig(NULL, &lirc.config, NULL) != 0) {
    gGui->lirc_enable = 0;
    return;
  }

  gGui->lirc_enable = 1;

  if(gGui->lirc_enable)
    pthread_create(&(lirc.thread), NULL, xine_lirc_loop, NULL) ;
}

void deinit_lirc(void) {

  pthread_join(lirc.thread, NULL);

  if(gGui->lirc_enable) {
    lirc_freeconfig(lirc.config);
    lirc_deinit();
  }
}

#endif
