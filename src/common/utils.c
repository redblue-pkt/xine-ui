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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <xine/xineutils.h>

#include "utils.h"

extern char **environ;
extern int errno;

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

/*
 * cleanup the str string, take care about '
 */
char *atoa(char *str) {
  char *pbuf;
  int   quote = 0, dblquote = 0;

  pbuf = str;
  
  while(*pbuf == ' ') 
    pbuf++;

  if(*pbuf == '\'')
    quote = 1;
  else if(*pbuf == '"')
    dblquote = 1;
  
  pbuf = str;

  while(*pbuf != '\0')
    pbuf++;
  
  if(pbuf > str)
    pbuf--;

  while((pbuf > str) && (*pbuf == '\r' || *pbuf == '\n')) {
    *pbuf = '\0';
    pbuf--;
  }

  while((pbuf > str) && (*pbuf == ' ')) {
    *pbuf = '\0';
    pbuf--;
  }
  
  if((quote && (*pbuf == '\'')) || (dblquote && (*pbuf == '"'))) {
    *pbuf = '\0';
    pbuf--;
  }
  
  pbuf = str;

  while(*pbuf == ' ' || *pbuf == '\t')
    pbuf++;
  
  if((quote && (*pbuf == '\'')) || (dblquote && (*pbuf == '"')))
    pbuf++;
  
  return pbuf;
}

/*
 *
 */
static int _mkdir_safe(char *path) {
  struct stat  pstat;
  
  if(path == NULL)
    return 0;
  
  if((lstat(path, &pstat)) < 0) {
    /* file or directory no exist, create it */
    if(mkdir(path, 0755) < 0) {
      fprintf(stderr, "mkdir(%s) failed: %s\n", path, strerror(errno));
      return 0;
    }
  }
  else {
    /* Check of found file is a directory file */
    if(!S_ISDIR(pstat.st_mode)) {
      fprintf(stderr, "%s is not a directory.\n", path);
      return 0;
    }
  }

  return 1;
}

/*
 *
 */
int mkdir_safe(char *path) {
  char *p, *pp;
  char  buf[_PATH_MAX + _NAME_MAX + 1];
  char  buf2[_PATH_MAX + _NAME_MAX + 1];
  
  if(path == NULL)
    return 0;
  
  memset(&buf, 0, sizeof(buf));
  memset(&buf2, 0, sizeof(buf2));
  
  sprintf(buf, "%s", path);
  pp = buf;
  while((p = xine_strsep(&pp, "/")) != NULL) {
    if(p && strlen(p)) {
      sprintf(buf2, "%s/%s", buf2, p);
      if(!_mkdir_safe(buf2))
	return 0;
    }
  }

  return 1;
}

int get_bool_value(const char *val) {
  static struct {
    const char *str;
    int value;
  } bools[] = {
    { "1",     1 }, { "true",  1 }, { "yes",   1 }, { "on",    1 },
    { "0",     0 }, { "false", 0 }, { "no",    0 }, { "off",   0 },
    { NULL,    0 }
  };
  int i;
  
  if(val) {
    for(i = 0; bools[i].str != NULL; i++) {
      if(!(strcasecmp(bools[i].str, val)))
	return bools[i].value;
    }
  }
  
  return 0;
}

