/* 
 * Copyright (C) 2000-2010 the xine project
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

#include "main.h"
#include "keys.h"
#include "actions.h"
#include "stdctl.h"

#ifdef DEBUG
#define STDCTL_VERBOSE 1
#else
#define STDCTL_VERBOSE 0
#endif

extern parameter_t gParameter;

typedef struct {
  int                   fd;
  pthread_t             thread;
} _stdctl_t;

static _stdctl_t stdctl;

static __attribute__((noreturn)) void *xine_stdctl_loop(void *dummy) {
  char              c[255];
  int               len;
  int               k;
  fd_set            set;
  struct timeval    tv;
  int               secs, last_secs;
  char             *params;

  (void)dummy;

  last_secs = -1;
  params = NULL;

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

#if STDCTL_VERBOSE
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif

	/* Handle optional parameter */

	/* Note: Many commands seem not implemented in fbxine :-( */
	/* The parameter mechanism is implemented analog to xitk  */
	/* so that it can be used when more parms are implemented */

	/* alphanum: separated from the command by a '$'        */
	/* syntax:   "command$parameter"                        */
	/* example:  "OSDWriteText$Some Information to Display" */

	gParameter.alphanum.set = 0;
	params = strchr(c, '$');

	if (params != NULL) {

	  /* parameters available */

	  *params = '\0';
	  params++;

	  gParameter.alphanum.set = 1;
	  gParameter.alphanum.arg = params;

#if STDCTL_VERBOSE
	  fprintf(stdout, "Command: '%s'\nParameters: '%s'\n", c, params);
#endif
	}

	/* numeric: separated from the command by a '#' */
	/* syntax:  "command#parameter"                 */
	/* example: "SetPosition%#99"                   */

	gParameter.numeric.set = 0;
	params = strchr(c, '#');

	if (params != NULL) {

	  /* parameters available */

	  *params = '\0';
	  params++;

	  gParameter.numeric.set = 1;
	  gParameter.numeric.arg = atoi(params);

#if STDCTL_VERBOSE
	  fprintf(stdout, "Command: '%s'\nParameters: '%d'\n", c, gParameter.numeric.arg);
#endif
	}

	k = default_command_action(c);

	if(k)
	  do_action(k);
      }
    }

    if(get_pos_length(fbxine.stream, NULL, &secs, NULL)) {

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

static void exit_stdctl(void) {

  pthread_cancel(stdctl.thread);
  pthread_join(stdctl.thread, NULL);
}

void fbxine_init_stdctl(void) {
  static struct fbxine_callback exit_callback;

  stdctl.fd = STDIN_FILENO;
  fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_stdctl);
  pthread_create(&(stdctl.thread), NULL, xine_stdctl_loop, NULL) ;
}
