/*
** Copyright (C) 2003 Daniel Caujolle-Bert <segfault@club-internet.fr>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/
/* Largely inspired of xmms control socket stuff */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "common.h"

#define CTRL_PROTO_VERSION    0x1

typedef struct {
  uint16_t         version;     /* client/server version */
  ctrl_commands_t  command;     /* ctrl_commands_t */
  uint32_t         data_length; /* data */
} ctrl_header_packet_t;

typedef struct {
  ctrl_header_packet_t  hdr;
  int                   fd;
  void                 *data;
} serv_header_packet_t;

static int           ctrl_fd, session_id;
static int           going;
static pthread_t     thread_server;
static char         *socket_name;

static void send_packet(int fd, ctrl_commands_t command, void *data, uint32_t data_length);

/*
  single session (store session id in gGui struct)

  commands: add enqueue
  ------------------------
  ------------------------

  server thread;

  read/write: pass struct pointer;


  packet_send
  packet_read
  ack_send
  ack_read

*/

static int connect_to_session(int session) {
  int fd;
  
  if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
    struct sockaddr_un   saddr;
    uid_t                stored_uid, euid;
    
    saddr.sun_family = AF_UNIX;
    stored_uid       = getuid();
    euid             = geteuid();
    setuid(euid);

    snprintf(saddr.sun_path, 108, "%s/.xine/session.%d", (xine_get_homedir()), session);
    setreuid(stored_uid, euid);

    if((connect(fd,(struct sockaddr *) &saddr, sizeof(saddr))) != -1)
      return fd;

  }

  close(fd);
  return -1;
}

/* READ */
static void *read_packet(int fd, ctrl_header_packet_t *hdr) {
  void *data = NULL;
  
  if((read(fd, hdr, sizeof(ctrl_header_packet_t))) == sizeof(ctrl_header_packet_t)) {
    if(hdr->data_length) {
      data = xine_xmalloc(hdr->data_length);
      read(fd, data, hdr->data_length);
    }
  }

  return data;
}

static void read_ack(int fd) {
  ctrl_header_packet_t  hdr;
  void                 *data;
  
  data = read_packet(fd, &hdr);
  SAFE_FREE(data);
}


/* SEND */
static void _send_packet(int fd, void *data, ctrl_header_packet_t *hdr) {

  write(fd, hdr, sizeof(ctrl_header_packet_t));

  if(hdr->data_length && data)
    write(fd, data, hdr->data_length);
}
static void send_packet(int fd, ctrl_commands_t command, void *data, uint32_t data_length) {
  ctrl_header_packet_t  hdr;
  
  hdr.version     = CTRL_PROTO_VERSION;
  hdr.command     = command;
  hdr.data_length = data_length;

  _send_packet(fd, data, &hdr);
}

static void ssend_packet(int fd, void *data, uint32_t data_length) {
  ctrl_header_packet_t  hdr;
  
  hdr.version     = CTRL_PROTO_VERSION;
  hdr.command     = 0;
  hdr.data_length = data_length;
  
  _send_packet(fd, data, &hdr);
}

static void send_ack(serv_header_packet_t *shdr) {
  ssend_packet(shdr->fd, NULL, 0);
  close(shdr->fd);
}

void send_uint32(int session, ctrl_commands_t command, uint32_t value) {
  int fd;
  
  if((fd = connect_to_session(session)) == -1)
    return;
  send_packet(fd, command, &value, sizeof(uint32_t));
  read_ack(fd);
  close(fd);
}
uint32_t get_uint32(int session, ctrl_commands_t command) {
  ctrl_header_packet_t  hdr;
  void                 *data;
  int                   fd, ret = 0;
  
  if((fd = connect_to_session(session)) == -1)
    return ret;
  
  send_packet(fd, command, NULL, 0);
  data = read_packet(fd, &hdr);
  if(data) {
    ret = *((uint32_t *) data);
    free(data);
  }
  read_ack(fd);
  close(fd);

  return ret;
}

void send_boolean(int session, ctrl_commands_t command, uint8_t value) {
  int     fd;
  uint8_t bvalue = (value > 0) ? 1 : 0;
  
  if((fd = connect_to_session(session)) == -1)
    return;
  send_packet(fd, command, &bvalue, sizeof(uint8_t));
  read_ack(fd);
  close(fd);
}
uint8_t get_boolean(int session, ctrl_commands_t command) {
  ctrl_header_packet_t  hdr;
  uint8_t               ret = 0;
  void                 *data;
  int                   fd;
  
  if((fd = connect_to_session(session)) == -1)
    return ret;

  send_packet(fd, command, NULL, 0);
  data = read_packet(fd, &hdr);
  if(data) {
    ret = *((uint8_t *) data);
    free(data);
  }
  read_ack(fd);
  close(fd);
  
  return ret;
}

void send_string(int session, ctrl_commands_t command, char *string) {
  int fd;
  
  if((fd = connect_to_session(session)) == -1)
    return;
  send_packet(fd, command, string, string ? strlen(string) + 1 : 0);
  read_ack(fd);
  close(fd);
}
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

static int remote_cmd(int session, ctrl_commands_t command) {
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


#ifndef CTRL_TEST
/* SERVER */
void *ctrlsocket_func(void *data) {
  fd_set                set;
  struct timeval        tv;
  struct sockaddr_un    saddr;
  serv_header_packet_t *shdr;
  int                   len, fd;

  while(!going)
    xine_usec_sleep(10000);
  
  while(going) {

    FD_ZERO(&set);
    FD_SET(ctrl_fd, &set);

    tv.tv_sec  = 0;
    tv.tv_usec = 100000;

    len = sizeof(saddr);
    
    if(((select(ctrl_fd + 1, &set, NULL, NULL, &tv)) <= 0) || 
       ((fd = accept(ctrl_fd, (struct sockaddr *) &saddr, &len)) == -1))
      continue;
    
    shdr = (serv_header_packet_t *) xine_xmalloc((sizeof(serv_header_packet_t)));

    (void) read(fd, &(shdr->hdr), sizeof(ctrl_header_packet_t));
    
    if(shdr->hdr.data_length) {
      shdr->data = xine_xmalloc(shdr->hdr.data_length);
      read(fd, shdr->data, shdr->hdr.data_length);
    }

    shdr->fd = fd;

    switch(shdr->hdr.command) {
      
    case CMD_PLAYLIST_FLUSH:
      send_ack(shdr);
      break;
      
    case CMD_PLAYLIST_ADD:
      gui_dndcallback((char *)shdr->data);
      send_ack(shdr);
      break;
      
    case CMD_PLAYLIST_NEXT:
    case CMD_PLAYLIST_PREV:
      send_ack(shdr);
      break;

    case CMD_GET_VERSION:
      printf("GET VERSION\n");
      send_packet(shdr->fd, CMD_GET_VERSION, VERSION,  strlen(VERSION));
      send_ack(shdr);
      break;

    case CMD_PING:
      send_ack(shdr);
      break;

    }

    SAFE_FREE(shdr->data);
    SAFE_FREE(shdr);
  }

  pthread_exit(NULL);
}

int setup_ctrlsocket(void) {
  struct sockaddr_un   saddr;
  int                  retval = 0;
  int                  i;
  
  if((ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
    for(i = 0;; i++)	{
      saddr.sun_family = AF_UNIX;
      
      snprintf(saddr.sun_path, 108, "%s/.xine/session.%d", (xine_get_homedir()), i);
      if(!is_remote_running(i)) {
	if((unlink(saddr.sun_path) == -1) && errno != ENOENT) {
	  printf("setup_ctrlsocket(): Failed to unlink %s (Error: %s)", 
		 saddr.sun_path, strerror(errno));
	}
	else
	  printf("Unlink OK\n");
      }
      else
	printf("remote_is_running()\n");
      
      if((bind(ctrl_fd, (struct sockaddr *) &saddr, sizeof (saddr))) != -1) {

	session_id = i;
	listen(ctrl_fd, 100);
	going = 1;
	
	pthread_create(&thread_server, NULL, ctrlsocket_func, NULL);
#warning CHECK RETURN CODE
	socket_name = strdup(saddr.sun_path);
	retval = 1;
	break;
      }
      else {
	printf("setup_ctrlsocket(): Failed to assign %s to a socket (Error: %s)",
	       saddr.sun_path, strerror(errno));
	break;
      }
    }
  }
  else
    printf("setup_ctrlsocket(): Failed to open socket: %s", strerror(errno));
  
  if(!retval) {
    if(ctrl_fd != -1)
      close(ctrl_fd);
    ctrl_fd = 0;
  }

  return retval;
}

#else /* CTRL_TEST */

int main(int argc, char **argv) {
  int session = 0;
  int fd;
  
  if(argc > 1)
    session = atoi(argv[1]);
  
  if(is_remote_running(session)) {
    printf("session %d is running\n", session);
    send_string(session, CMD_PLAYLIST_ADD, "azertyuiopqsdfghjklmwxcvbn");
    printf("GET_VERSION: '%s'\n", get_string(session, CMD_GET_VERSION));
  }
  else
    printf("Session %d isn't running\n");

  return 1;
}
#endif
