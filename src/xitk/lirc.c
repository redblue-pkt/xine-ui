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
 * lirc-specific stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>

#include "xitk.h"

#include "event.h"
#include "panel.h"
#include "actions.h"

#ifdef HAVE_LIRC
#include "lirc/lirc_client.h"

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

void *xine_lirc_loop(void *dummy) {
  char             *code, *c;
  int               ret;
  kbinding_entry_t *k;

  /* I'm a poor lonesome pthread... */
  pthread_detach(pthread_self());

  while(gGui->running) {
    
    pthread_testcancel();
    
    while(lirc_nextcode(&code) == 0) {

      pthread_testcancel();
      
      if(code == NULL) 
	break;
      
      pthread_testcancel();
      
      while((ret = lirc_code2char(lirc.config, code, &c)) == 0 
	    && c != NULL) {
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif

	k = kbindings_lookup_action(gGui->kbindings, c);
	if(k)
	  gui_execute_action_id((kbindings_get_action_id(k)));
	
      }
      xitk_paint_widget_list (panel->widget_list);
      free(code);

      if(ret == -1) 
	break;

    }
  }
  
  pthread_exit(NULL);
}

void init_lirc(void) {
  /*  int flags; */

  if((lirc.fd = lirc_init("xine", LIRC_VERBOSE)) == -1) {
    gGui->lirc_enable = 0;
    return;
  }
  /*
  else {
    flags = fcntl(gGui->lirc.fd, F_GETFL, 0);
    if(flags != -1)
      fcntl(gGui->lirc.fd, F_SETFL, flags|FASYNC|O_NONBLOCK);
  }
  */

  if(lirc_readconfig(NULL, &lirc.config, NULL) != 0) {
    gGui->lirc_enable = 0;
    return;
  }

  gGui->lirc_enable = 1;

  if(gGui->lirc_enable)
    pthread_create (&lirc.thread, NULL, xine_lirc_loop, NULL) ;
}

void deinit_lirc(void) {

  pthread_cancel(lirc.thread);

  if(gGui->lirc_enable) {
    lirc_freeconfig(lirc.config);
    lirc_deinit();
  }
}

#endif
