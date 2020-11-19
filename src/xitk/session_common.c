/*
 * Copyright (C) 2000-2020 the xine project
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
#define _DEFAULT_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "common.h"
#include "session.h"
#include "session_internal.h"

#ifndef S_ISSOCK
#define S_ISSOCK(mode) 0
#endif

int connect_to_session(int session) {
  int fd;

  if((fd = xine_socket_cloexec(AF_UNIX, SOCK_STREAM, 0)) != -1) {
    union {
      struct sockaddr_un un;
      struct sockaddr sa;
    } saddr;
    uid_t                stored_uid, euid;

    saddr.un.sun_family = AF_UNIX;
    stored_uid       = getuid();
    euid             = geteuid();
    if (setuid(euid) == -1)
      perror("setuid() failed");

    snprintf(saddr.un.sun_path, sizeof(saddr.un.sun_path), "%s%s%d", (xine_get_homedir()), "/.xine/session.", session);
    if (setreuid(stored_uid, euid) == -1)
      perror("setreuid() failed");
    if((connect(fd,&saddr.sa, sizeof(saddr.un))) != -1) {
      return fd;
    }

    close(fd);
  }

  return -1;
}

void *read_packet(int fd, ctrl_header_packet_t *hdr) {
  void *data = NULL;

  if((read(fd, hdr, sizeof(ctrl_header_packet_t))) == sizeof(ctrl_header_packet_t)) {
    if(hdr->data_length) {
      data = malloc(hdr->data_length);
      if (read(fd, data, hdr->data_length) != hdr->data_length) {
        perror("session read() failed");
        SAFE_FREE(data);
      }
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

static int _poll_rd(int fd, int timeout)
{
  fd_set         set;
  struct timeval tv;
  int            result;

  FD_ZERO(&set);
  FD_SET(fd, &set);

  tv.tv_sec  = 0;
  tv.tv_usec = 1000 * timeout;

 retry:
  result = select(fd + 1, &set, NULL, NULL, &tv);
  if (result < 0) {
    if (errno == EINTR)
      goto retry;
    perror("polling remote socket failed\n");
    return result;
  }
  if (result == 0) {
    fprintf(stderr, "remote socket timeout\n");
    return -1;
  }

  return 0;
}

int _send_packet(int fd, const void *data, ctrl_header_packet_t *hdr) {

  ssize_t r;

  r = write(fd, hdr, sizeof(ctrl_header_packet_t));
  if (r != sizeof(ctrl_header_packet_t)) {
    perror("socket write failed");
    return -1;
  }

  if (hdr->data_length && data) {
    r = write(fd, data, hdr->data_length);
    if (r != hdr->data_length) {
      perror("socket write failed");
      return -1;
    }
  }

  return 0;
}

int send_packet(int fd, ctrl_commands_t command, const void *data, uint32_t data_length) {
  ctrl_header_packet_t  hdr;

  hdr.version     = CTRL_PROTO_VERSION;
  hdr.command     = command;
  hdr.data_length = data_length;

  return _send_packet(fd, data, &hdr);
}

int send_string(int session, ctrl_commands_t command, const char *string) {
  int fd;

  if((fd = connect_to_session(session)) == -1)
    return -1;
  if (send_packet(fd, command, string, string ? strlen(string) + 1 : 0) < 0) {
    close(fd);
    return -1;
  }

  read_ack(fd);
  close(fd);

  return 0;
}

static int _remote_cmd(int session, ctrl_commands_t command, int timeout) {
  int fd;

  if((fd = connect_to_session(session)) == -1)
    return 0;

  if (send_packet(fd, command, NULL, 0) < 0) {
    close(fd);
    return 0;
  }

  if (timeout >= 0 && _poll_rd(fd, timeout) < 0) {
    close(fd);
    return 0;
  }

  read_ack(fd);
  close(fd);

  return 1;
}

int remote_cmd(int session, ctrl_commands_t command) {
  return _remote_cmd(session, command, -1);
}

int is_remote_running(int session) {
  return _remote_cmd(session, CMD_PING, 1000);
}
