/* 
 * Copyright (C) 2000-2017 the xine project
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
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include "dump.h"
#include "globals.h"

void dump_host_info(void) {
#ifdef HAVE_SYS_UTSNAME_H
  struct utsname uts;

  if((uname(&uts)) == -1) {
    printf("uname() failed: %s\n", strerror(errno));
  }
  else {
    printf("   Platform information:\n");
    printf("   --------------------\n");
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
#endif
}

void dump_cpu_infos(void) {
#if defined (__linux__)
  FILE *stream;
  char  buffer[2048];
  size_t got;

  if((stream = fopen("/proc/cpuinfo", "r"))) {
    
    printf("   CPU information:\n");
    printf("   ---------------\n");
    
    memset(&buffer, 0, sizeof(buffer));
    while((got = fread(&buffer, 1, 2047, stream)) > 0) {
      char *p, *pp;
      buffer[got] = 0;
      pp = buffer;
      while((p = strsep(&pp, "\n"))) {
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
  printf("   CPU information unavailable.\n"); 
#endif
}

void dump_error(const char *msg) {
  if(!__xineui_global_verbosity) return;

  fprintf(stderr, "%s", "\n---------------------- (ERROR) ----------------------\n");
  fputs(msg, stderr); 
  fprintf(stderr, "%s","\n------------------ (END OF ERROR) -------------------\n\n");
}

void dump_info(const char *msg) {
  if(!__xineui_global_verbosity) return;

  fprintf(stderr, "%s", "\n---------------------- (INFO) ----------------------\n");
  fputs(msg, stderr);
  fprintf(stderr, "%s", "\n------------------- (END OF INFO) ------------------\n\n");
}
