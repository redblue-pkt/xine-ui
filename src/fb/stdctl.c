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

#include "main.h"
#include "keys.h"
#include "actions.h"

#ifdef DEBUG
#define STDCTL_VERBOSE 1
#else
#define STDCTL_VERBOSE 0
#endif

typedef struct {
  int                   fd;
  pthread_t             thread;
} _stdctl_t;

static _stdctl_t stdctl;

void *xine_stdctl_loop(void *dummy) {
  char              c[255];
  int               len;
  int               k;
  fd_set            set;
  struct timeval    tv;

  while(1) {

    FD_ZERO(&set);
    FD_SET(stdctl.fd, &set);
    
    tv.tv_sec  = 0;
    tv.tv_usec = 500000;

    select(stdctl.fd + 1, &set, NULL, NULL, &tv);
    
    if (FD_ISSET(stdctl.fd, &set)) {
      len = read(stdctl.fd, &c, 255);
      if (len > 0) {
	c[len-1] = '\0';
	
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif
	
	k = default_command_action(c);
	if(k)
	     do_action(k);
      }
    }
  }
  
  pthread_exit(NULL);
}

void exit_stdctl(void) {
  pthread_cancel(stdctl.thread);
  pthread_join(stdctl.thread, NULL);
}


void fbxine_init_stdctl(void) {
  static struct fbxine_callback exit_callback;

  stdctl.fd = STDIN_FILENO;
  fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_stdctl);
  pthread_create(&(stdctl.thread), NULL, xine_stdctl_loop, NULL) ;
}

