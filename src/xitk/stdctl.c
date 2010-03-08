/* 
 * Copyright (C) 2000-2007 the xine project
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

#define DEBUG_STDCTL 0

extern _panel_t        *panel;

static struct {
  int                   fd;
  pthread_t             thread;
  FILE                 *fbk;
} stdctl;

static __attribute__((noreturn)) void *xine_stdctl_loop(void *dummy) {
  char              buf[256], *c, *c1;
  int               len, selrt;
  kbinding_entry_t *k;
  fd_set            set;
  struct timeval    tv;
  int               secs, last_secs;
  char             *params;

  last_secs = -1;
  params = NULL;

  while(gGui->running) {

    FD_ZERO(&set);
    FD_SET(stdctl.fd, &set);

    tv.tv_sec  = 0;
    tv.tv_usec = 500000;

    selrt = select(stdctl.fd + 1, &set, NULL, NULL, &tv);

    if(selrt > 0 && FD_ISSET(stdctl.fd, &set)) {

      len = read(stdctl.fd, &buf, sizeof(buf) - 1);

      if(len > 0) {

	buf[len] = '\0';

	c = buf;
	while((c1 = strchr(c, '\n'))) {
	  *c1 = '\0';

#if DEBUG_STDCTL
	  fprintf(stderr, "Command Received = '%s'\n", c);
#endif

	  /* Handle optional parameter */

	  /* alphanum: separated from the command by a '$'        */
	  /* syntax:   "command$parameter"                        */
	  /* example:  "OSDWriteText$Some Information to Display" */

	  gGui->alphanum.set = 0;
	  params = strchr(c, '$');

	  if(params != NULL) {

	    /* parameters available */

	    *params = '\0';
	    params++;

	    gGui->alphanum.set = 1;
	    gGui->alphanum.arg = params;

#if DEBUG_STDCTL
	    fprintf(stderr, "Command: '%s'\nParameters: '%s'\n", c, params);
#endif
	  }

	  /* numeric: separated from the command by a '#' */
	  /* syntax:  "command#parameter"                 */
	  /* example: "SetPosition%#99"                   */

	  gGui->numeric.set = 0;
	  params = strchr(c, '#');

	  if(params != NULL) {

	    /* parameters available */

	    *params = '\0';
	    params++;

	    gGui->numeric.set = 1;
	    gGui->numeric.arg = atoi(params);

	    if(gGui->numeric.arg < 0)
	    {
	      gGui->numeric.arg = 0;
	      fprintf(stderr, "WARNING: stdctl: Negative num argument not supported, set to 0\n");
	    }

#if DEBUG_STDCTL
	    fprintf(stderr, "Command: '%s'\nParameters: '%d'\n", c, gGui->numeric.arg);
#endif
	  }

	  k = kbindings_lookup_action(gGui->kbindings, c);

	  if(k)
	    gui_execute_action_id((kbindings_get_action_id(k)));

	  c = c1 + 1;
	}
      }

      if(panel_is_visible())
	xitk_paint_widget_list (panel->widget_list);

      if(len <= 0) {
	gui_execute_action_id(ACTID_QUIT);
	break;
      }
    }

    if(gui_xine_get_pos_length(gGui->stream, NULL, &secs, NULL)) {

      secs /= 1000;

      if (secs != last_secs) {
	fprintf(stdctl.fbk, "time: %d\n", secs);
	last_secs = secs;
      }
    }

  }
  
  pthread_exit(NULL);
}

void stdctl_start(void) {
  int err;

  stdctl.fd = STDIN_FILENO;
  stdctl.fbk = gGui->stdout;
  
  if((err = pthread_create(&(stdctl.thread), NULL, xine_stdctl_loop, NULL)) != 0) {
    fprintf(stderr, _("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    gGui->stdctl_enable = 0;
  }
}

void stdctl_stop(void) {
  /*
   * We print the exit feedback here, not on exit of the stdctl thread.
   * Otherwise, if ACTID_QUIT (bringing us to this point) was triggered
   * by stdctl itself, we run into a race with the main thread which
   * could close the feedback channel beforehand: pthread_join doesn't
   * wait but returns immediately with EDEADLK as stdctl tries to join
   * itself and termination continues asynchronously.
   */
  fprintf(stdctl.fbk, "Exiting\n");
  pthread_join(stdctl.thread, NULL);
}

void stdctl_event(const xine_event_t *event)
{
  switch(event->type) {
  case XINE_EVENT_UI_PLAYBACK_FINISHED:
    fprintf(stdctl.fbk, "PlaybackFinished\n");
    break;
  }
}

void stdctl_keypress(XKeyEvent event)
{
    KeySym  sym;
    char   *str;

    if((sym = XKeycodeToKeysym(event.display, event.keycode, 0)))
      if((str = XKeysymToString(sym)))
	fprintf(stdctl.fbk, "KeyPress$%s\n", str);
}

void stdctl_playing(const char *mrl)
{
    fprintf(stdctl.fbk, "PlaybackStart$%s\n", mrl);
    fprintf(stdctl.fbk, "PlaylistPos#%i\n", gGui->playlist.cur);
}
