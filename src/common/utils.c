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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <alloca.h>

#include <xine/xineutils.h>

#include "utils.h"

extern char **environ;

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
      errno = EEXIST;
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

static int is_valid_inthex(char *strval) {
  char *p = strval;
  
  if(p && strlen(p)) {
    while(*p) {
      if(!isxdigit(*p))
	return 0;

      p++;
    }

    return 1;
  }

  return 0;
}

static int check_valid_value(char *value) {
  char *p   = value;
  char  buffer[2048];
  char *pp  = buffer;
  int   end = 0;
  
  while(*p && !end) {
    switch(*p) {
    case ':':
    case '\0':
      end++;
      break;
      
    default:
      *pp++ = *p++;
      break;
    }
  }
  
  *pp = '\0';

  return(is_valid_inthex(buffer));
}

static char *get_ipv6_addr(char *str) {
  int len;

  if(str && (len = strlen(str))) {
    char *p = str + (len - 1);
    char *bracketl, *bracketr = NULL;

    while(p > str) {

      if(*p == ']')
	bracketr = p;
      else if((*p == ':') && (*(p - 1) == ':')) {
	if(bracketr) {
	  bracketl = p;

	  while(bracketl && (bracketl > str)) {
	    
	    if(*bracketl == '[')
	      return bracketl;
	    
	    bracketl--;
	  }
	}
	
	return (p - 1);
      }
      
      p--;
    }
  }

  return NULL;
}

char *get_last_double_semicolon(char *str) {
  int len;

  if(str && (len = strlen(str))) {
    char *p = str + (len - 1);

    while(p > str) {

      if((*p == ':') && (*(p - 1) == ':'))
	return (p - 1);
      
      p--;
    }
  }

  return NULL;
}

int is_ipv6_double_semicolon(char *ostr) {

  if(ostr) {
    char *bracketl, *bracketr, *d_semic;
    char *str = get_ipv6_addr(ostr);
    
    if(str && (bracketl = strchr(str, '[')) && (bracketr = strchr(str, ']')) && 
       (d_semic = strstr(str, "::")) && 
       ((bracketl < d_semic) && (d_semic < bracketr))) {
      char   *addr;
      size_t  len;

      if(strstr(d_semic + 2, "::"))
	return 0;
      
      /* Okay there is something like [...::...] */
      len  = (bracketr - bracketl) + 1;
      
      /* [::] = unspecified address */
      if(len == 4) {
	return 1;
      }
      else {
	char *semic;
	char  buffer[2048];
	
	memset(&buffer, 0, sizeof(buffer));
	
	len--;
	
	addr = (char *) alloca(len);
	memcpy(addr, bracketl + 1, len);
	
	addr[len-1] = '\0';
	d_semic      = addr + (d_semic - bracketl) - 1;
	
	if((semic = strrchr(d_semic, ':')))
	  semic++;
	
	if(semic && (semic > (d_semic + 2))) {
	  if((semic != (d_semic + 1)) && (semic != (d_semic + 2))) {
	    
	    /* IPv6 style: '::%x:' */
	    if((*d_semic == ':') && (*(d_semic + 1) == ':'))
	      return(check_valid_value(d_semic + 2));
	    
	  }
	}
	else {
	  if(semic) {
	    int   ipv4[4];
	    
	    /* IPv6 style: '::%x' */
	    if(check_valid_value(semic))
	      return 1;
	    /* IPv4 style: ::%d.%d.%d.%d  ::%x|%d:....:%d.%d.%d.%d */
	    else if((sscanf(semic, "%d.%d.%d.%d", &ipv4[0], &ipv4[1], &ipv4[2], &ipv4[3])) == 4)
	      return 1;
	    
	  }
	  
	  /* IPv6 style: '%x::' */
	  if((*addr == ':') && (*(addr + 1) == ':'))
	    return (check_valid_value(addr + 2));
	  
	  /* IPv6 style: ':%x::' '%x::' */
	  else if((*(addr + (len - 2)) == ':') && (*(addr + (len - 3)) == ':')) {
	    *(addr + (len - 3)) = '\0';
	    
	    if((semic = strrchr(addr, ':')))
	      return (check_valid_value(semic + 1));
	    else
	      return (check_valid_value(addr));
	    
	  }
	}
      }
    }
  }

  return 0;
}
