/* 
 * Copyright (C) 2000-2006 the xine project
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
#include <sys/utsname.h>

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
  
  if((stat(path, &pstat)) < 0) {
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
  
  strcpy(buf, path);
  pp = buf;
  while((p = xine_strsep(&pp, "/")) != NULL) {
    if(p && strlen(p)) {
      sprintf(buf2+strlen(buf2), "/%s", p);
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

const char *get_last_double_semicolon(const char *str) {
  int len;

  if(str && (len = strlen(str))) {
    const char *p = str + (len - 1);

    while(p > str) {

      if((*p == ':') && (*(p - 1) == ':'))
	return (p - 1);
      
      p--;
    }
  }

  return NULL;
}

int is_ipv6_last_double_semicolon(const char *str) {
  
  if(str && strlen(str)) {
    const char *d_semic = get_last_double_semicolon(str);

    if(d_semic) {
      const char *bracketl = NULL;
      const char *bracketr = NULL;
      const char *p        = d_semic + 2;
      
      while(*p && !bracketr) {
	if(*p == ']')
	  bracketr = p;

	p++;
      }
      
      if(bracketr) {
	p = d_semic;

	while((p >= str) && !bracketl) {
	  if(*p == '[')
	    bracketl = p;

	  p--;
	}
	
	if(bracketl) {

	  /* Look like an IPv6 address, check it */
	  p = d_semic + 2;

	  while(*p) {
	    switch(*p) {
	    case ':':
	    case '.':
	      break;

	    case ']': /* IPv6 delimiter end */

	      /* now go back to '[' */
	      p = d_semic;
	      
	      while(p >= str) {
		switch(*p) {
		case ':':
		  break;
		  
		case '.': /* This couldn't happen */
		  return 0;
		  break;

		case '[': /* We reach the beginning, it's a valid IPv6 */
		  return 1;
		  break;
		  
		default:
		  if(!isxdigit(*p))
		    return 0;
		  break;

		}

		p--;
	      }
	      break;

	    default:
	      if(!isxdigit(*p))
		return 0;
	      break;

	    }

	    p++;
	  }
	}
      }
    }
  }

  return 0;
}

int is_downloadable(char *filename) {
  if(filename && 
     ((!strncasecmp(filename, "http://", 7)) || (!strncasecmp(filename, "ftp://", 6))))
    return 1;
  return 0;
}

int is_a_dir(char *filename) {
  struct stat  pstat;
  
  if((stat(filename, &pstat)) < 0)
    return 0;
  
  return (S_ISDIR(pstat.st_mode));
}

int is_a_file(char *filename) {
  struct stat  pstat;
  
  if((stat(filename, &pstat)) < 0)
    return 0;
  
  return (S_ISREG(pstat.st_mode));
}

void dump_host_info(void) {
  struct utsname uts;

  if((uname(&uts)) == -1) {
    printf("uname() failed: %s\n", strerror(errno));
  }
  else {
    printf("   Plateform informations:\n");
    printf("   ----------------------\n");
    printf("        system name     : %s\n", uts.sysname);
    printf("        node name       : %s\n", uts.nodename);
    printf("        release         : %s\n", uts.release);
    printf("        version         : %s\n", uts.version);
    printf("        machine         : %s\n", uts.machine);
    /* ignoring the GNU extension domainname, it's useless here */
#if 0
#ifdef _GNU_SOURCE
    printf("        domain name     : %s\n", uts.domainname);
#endif
#endif
  }
}

void dump_cpu_infos(void) {
#if defined (__linux__)
  FILE *stream;
  char  buffer[2048];
  
  if((stream = fopen("/proc/cpuinfo", "r"))) {
    
    printf("   CPU Informations:\n");
    printf("   ----------------\n");
    
    memset(&buffer, 0, sizeof(buffer));
    while(fread(&buffer, 1, 2047, stream)) {
      char *p, *pp;
      
      pp = buffer;
      while((p = xine_strsep(&pp, "\n"))) {
	if(p && strlen(p))
	  printf("\t%s\n", p);
      } 
      
      memset(&buffer, 0, sizeof(buffer));
    }
    
    printf("   -------\n");
    fclose(stream);
  }
  else
    printf("   Unable to open '/proc/cpuinfo': '%s'.\n", strerror(errno));
  
#else
  printf("   CPU informations unavailable.\n"); 
#endif
}

void dump_error(int verbosity, char *msg) {
  if(verbosity) {
    fprintf(stderr, "%s", "\n---------------------- (ERROR) ----------------------\n");
    fputs(msg, stderr); 
    fprintf(stderr, "%s","\n------------------ (END OF ERROR) -------------------\n\n");
  }
}

void dump_info(int verbosity, char *msg) {
  if(verbosity) {
    fprintf(stderr, "%s", "\n---------------------- (INFO) ----------------------\n");
    fputs(msg, stderr);
    fprintf(stderr, "%s", "\n------------------- (END OF INFO) ------------------\n\n");
  }
}
