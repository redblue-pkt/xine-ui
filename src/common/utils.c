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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;
extern int errno;

#ifndef __GNUC__
#define	__FUNCTION__	__func__
#endif

/*
 * A thread-safe usecond sleep
 */
void xine_usec_sleep(unsigned usec) {
#if HAVE_NANOSLEEP
  /* nanosleep is prefered on solaris, because it's mt-safe */
  struct timespec ts;

  ts.tv_sec =   usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  nanosleep(&ts, NULL);
#else
  usleep(usec);
#endif
}

/*
 * Execute a shell command.
 */
int xine_system(int dont_run_as_root, char *command) {
  int pid, status;
  
  /* 
   * Don't permit run as root
   */
  if(dont_run_as_root) {
    if(getuid() == 0)
      return -1;
  }

  if(command == 0)
    return 1;

  pid = fork();

  if(pid == -1)
    return -1;
  
  if(pid == 0) {
    char *argv[4];
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = command;
    argv[3] = 0;
    execve("/bin/sh", argv, environ);
    exit(127);
  }

  do {
    if(waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
	return -1;
    } 
    else {
      return WEXITSTATUS(status);
    }
  } while(1);
  
  return -1;
}
