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
 */
/* Largely inspired of xmms control socket stuff */

/* required for S_ISSOCK */
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "common.h"
#include "session_internal.h"

#ifndef S_ISSOCK
#define S_ISSOCK(mode) 0
#endif

int connect_to_session(int session) {
  int fd;
  
  if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
    union {
      struct sockaddr_un un;
      struct sockaddr sa;
    } saddr;
    uid_t                stored_uid, euid;
    
    saddr.un.sun_family = AF_UNIX;
    stored_uid       = getuid();
    euid             = geteuid();
    setuid(euid);

    snprintf(saddr.un.sun_path, 108, "%s%s%d", (xine_get_homedir()), "/.xine/session.", session);
    setreuid(stored_uid, euid);

    if((connect(fd,&saddr.sa, sizeof(saddr.un))) != -1)
      return fd;

  }

  close(fd);
  return -1;
}

void *read_packet(int fd, ctrl_header_packet_t *hdr) {
  void *data = NULL;
  
  if((read(fd, hdr, sizeof(ctrl_header_packet_t))) == sizeof(ctrl_header_packet_t)) {
    if(hdr->data_length) {
      data = malloc(hdr->data_length);
      read(fd, data, hdr->data_length);
    }
  }

  return data;
}

void read_ack(int fd) {
  ctrl_header_packet_t  hdr;
  void                 *data;
  
  data = read_packet(fd, &hdr);
  SAFE_FREE(data);
}

void _send_packet(int fd, void *data, ctrl_header_packet_t *hdr) {

  write(fd, hdr, sizeof(ctrl_header_packet_t));

  if(hdr->data_length && data)
    write(fd, data, hdr->data_length);
}

void send_packet(int fd, ctrl_commands_t command, void *data, uint32_t data_length) {
  ctrl_header_packet_t  hdr;
  
  hdr.version     = CTRL_PROTO_VERSION;
  hdr.command     = command;
  hdr.data_length = data_length;

  _send_packet(fd, data, &hdr);
}

void send_string(int session, ctrl_commands_t command, char *string) {
  int fd;
  
  if((fd = connect_to_session(session)) == -1)
    return;
  send_packet(fd, command, string, string ? strlen(string) + 1 : 0);
  read_ack(fd);
  close(fd);
}

int remote_cmd(int session, ctrl_commands_t command) {
  int fd;
  
  if((fd = connect_to_session(session)) == -1)
    return 0;

  send_packet(fd, command, NULL, 0);
  read_ack(fd);
  close(fd);
  
  return 1;
}

int is_remote_running(int session) {
  return remote_cmd(session, CMD_PING);
}
