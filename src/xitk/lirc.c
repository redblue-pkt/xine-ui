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

extern gGui_t          *gGui;

extern widget_t        *panel_checkbox_pause;
extern widget_list_t   *panel_widget_list;

struct lirc_config     *xlirc_config;
int                     lirc_fd;
pthread_t               lirc_thread;

/*
 * Check if rxcmd is present in tockens[] array.
 * On success, return the position in tokens[] array, and set retunk to NULL.
 * On failure, return -1 and set retunk to the unknown rxcmd.
 */
static int handle_lirc_command(char **rxcmd, 
			       const char **tokens, char **retunk) {
  int ret;

  if(*rxcmd == NULL)
    return -1;
  
  for(ret = 0; tokens[ret] != NULL; ret++) {
    if(!(strcasecmp(*rxcmd, tokens[ret]))) {
      *retunk = NULL;
      return ret;
    }
  }
  
  *retunk = *rxcmd;
  return -1;
}

void *xine_lirc_loop(void *dummy) {
  char *code, *c, *uc = NULL;
  int ret, lsub;
  const char *lirc_commands[] = {
    "0%",
    "10%",
    "20%",
    "30%",
    "40%",
    "50%",
    "60%",
    "70%",
    "80%",
    "90%",
    "quit",
    "play", 
    "pause",
    "next",
    "prev",
    "spu+",
    "spu-",
    "audio+",
    "audio-",
    "eject",
    "fullscr",
    "hidegui",
    "hideoutput",
    NULL
  };
  enum action {
    ZEROP,
    TENP,
    TWENTYP,
    THIRTYP,
    FOURTYP,
    FIVETYP,
    SIXTYP,
    SEVENTYP,
    HEIGHTYP,
    NINETYP,
    lQUIT,
    lPLAY,
    lPAUSE,
    lNEXT,
    lPREV,
    lSPUnext,
    lSPUprev,
    lAUDIOnext,
    lAUDIOprev,
    lEJECT,
    lFULLSCR,
    lHIDEGUI,
    lHIDEOUTPUT
  };
  

  /* I'm a poor lonesome pthread... */
  pthread_detach(pthread_self());


  while(gGui->running) {
    
    pthread_testcancel();
    
    while(lirc_nextcode(&code) == 0) {
      
      pthread_testcancel();
      
      if(code == NULL) 
	break;
      
      pthread_testcancel();
      
      while((ret = lirc_code2char(xlirc_config, code, &c)) == 0 
	    && c != NULL) {
	//fprintf(stdout, "Command Received = '%s'\n", c);
	
	if((lsub = handle_lirc_command(&c, lirc_commands, &uc)) != -1) {
	  switch(lsub) {

	  case ZEROP:
	  case TENP:
	  case TWENTYP:
	  case THIRTYP:
	  case FOURTYP:
	  case FIVETYP:
	  case SIXTYP:
	  case SEVENTYP:
	  case HEIGHTYP:
	  case NINETYP:
	    gui_set_current_position ((6553 * lsub));
	    break;


	  case lQUIT:
	    // Dont work
	    //gui_exit(NULL, NULL);
	    kill(getpid(), SIGINT);
	    break;

	  case lPLAY:
	    if(xine_get_status(gGui->xine) != XINE_PLAY)
	      gui_play(NULL, NULL);
	    else
	      gui_stop(NULL, NULL);
	    break;

	  case lPAUSE:
	    gui_pause (panel_checkbox_pause, (void*)(1), 0); 
	    break;

	  case lNEXT:
	    gui_nextprev(NULL, (void*)GUI_NEXT);
 	    break;

	  case lPREV:
	    gui_nextprev(NULL, (void*)GUI_PREV);
	    break;

	  case lSPUnext:
	    gui_change_spu_channel(NULL, (void*)GUI_NEXT);
	    break;

	  case lSPUprev:
	    gui_change_spu_channel(NULL, (void*)GUI_PREV);
	    break;
	    
	  case lAUDIOnext:
	    gui_change_audio_channel(NULL, (void*)GUI_NEXT);
	    break;

	  case lAUDIOprev:
	    gui_change_audio_channel(NULL, (void*)GUI_PREV);
	    break;

	  case lEJECT:
	    gui_eject(NULL, NULL);
	    break;

	  case lFULLSCR:
	    gui_toggle_fullscreen(NULL, NULL);
	    break;

	  case lHIDEGUI:
	    panel_toggle_visibility (NULL, NULL);
	    break;

	  case lHIDEOUTPUT:
	    /* FIXME
	    xine_set_window_visible(gGui->xine, 
				    !(xine_get_window_visible(gGui->xine)));
	    */
	    break;
	  }
	}
	
	/* FIXME
	   if(uc) { */
	  /* 
	   * Check here if the unknown IR order match with
	   * ID of one of input plugins.
	   */
	/* FIXME
	  int i, num_plugins;
	  input_plugin_t *ip;
	  
	  ip = xine_get_input_plugin_list (&num_plugins);
	  for (i = 0; i < num_plugins; i++) {
	    
	    if(ip->get_capabilities() & INPUT_CAP_AUTOPLAY) {
	      
	      if(!strcasecmp(ip->get_identifier(), uc)) {
		pl_scan_input(NULL, ip);
	      }
	    }
	    ip++;
	  }
	  }*/
	
      }
      paint_widget_list (panel_widget_list);
      free(code);

      if(ret == -1) 
	break;

    }
  }
  
  pthread_exit(NULL);
}

void init_lirc(void) {
  /*  int flags; */

  if((lirc_fd = lirc_init("xine", 1)) == -1) {
    gGui->lirc_enable = 0;
    return;
  }
  /*
  else {
    flags = fcntl(gGui->lirc_fd, F_GETFL, 0);
    if(flags != -1)
      fcntl(gGui->lirc_fd, F_SETFL, flags|FASYNC|O_NONBLOCK);
  }
  */

  if(lirc_readconfig(NULL, &xlirc_config, NULL) != 0) {
    gGui->lirc_enable = 0;
    return;
  }

  gGui->lirc_enable = 1;

  if(gGui->lirc_enable) {
    pthread_create (&lirc_thread, NULL, xine_lirc_loop, NULL) ;
    printf ("gui_main: LIRC thread created\n");
  }
}

void deinit_lirc(void) {

  pthread_cancel(lirc_thread);
  
  if(gGui->lirc_enable) {
    lirc_freeconfig(xlirc_config);
    lirc_deinit();
  }
}

#endif
