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
 * stdctl-specific stuff
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

extern gGui_t          *gGui;
extern _panel_t        *panel;

typedef struct {
  int                   fd;
  pthread_t             thread;
} _stdctl_t;

static _stdctl_t stdctl;

static void *xine_stdctl_loop(void *dummy) {
  char              c[255];
  int               len;
  kbinding_entry_t *k;
  fd_set            set;
  struct timeval    tv;
  int               secs, last_secs;

  last_secs = -1;

  while(gGui->running) {

    FD_ZERO(&set);
    FD_SET(stdctl.fd, &set);
    
    tv.tv_sec  = 0;
    tv.tv_usec = 500000;

    select(stdctl.fd + 1, &set, NULL, NULL, &tv);
    
    if(FD_ISSET(stdctl.fd, &set)) {
      len = read(stdctl.fd, &c, 255);
      if(len > 0) {
	c[len - 1] = '\0';
	
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif
	
	k = kbindings_lookup_action(gGui->kbindings, c);
	
	if(k)
	  gui_execute_action_id((kbindings_get_action_id(k)));
      }
      
      if(panel_is_visible())
	xitk_paint_widget_list (panel->widget_list);
      
      if(len == -1) 
	break;
      
    }

    if(gui_xine_get_pos_length(gGui->stream, NULL, &secs, NULL)) {
      secs /= 1000;
      
      if (secs != last_secs) {
	fprintf(stdout, "time: %d\n", secs);
	fflush(stdout);
	last_secs = secs;
      }
    }

  }
  
  pthread_exit(NULL);
}

void stdctl_start(void) {
  int err;

  stdctl.fd = STDIN_FILENO;
  
  if((err = pthread_create(&(stdctl.thread), NULL, xine_stdctl_loop, NULL)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    gGui->stdctl_enable = 0;
  }
}

void stdctl_stop(void) {
  pthread_join(stdctl.thread, NULL);
}
