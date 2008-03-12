/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "session.h"
#include "session_internal.h"

char *get_string(int session, ctrl_commands_t command) {
  ctrl_header_packet_t  hdr;
  char                 *data = NULL;
  int                   fd;
  
  if((fd = connect_to_session(session)) == -1)
    return data;
  
  send_packet(fd, command, NULL, 0);
  data = (char *)read_packet(fd, &hdr);
  read_ack(fd);
  close(fd);

  return data;
}

int main(int argc, char **argv) {
  int session = 0;
  
  if(argc > 1)
    session = atoi(argv[1]);
  
  if(is_remote_running(session)) {
    printf("session %d is running\n", session);
    send_string(session, CMD_PLAYLIST_ADD, "azertyuiopqsdfghjklmwxcvbn");
    printf("GET_VERSION: '%s'\n", get_string(session, CMD_GET_VERSION));
  }
  else
    printf("Session %d isn't running\n", session);

  return 1;
}
