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

#ifndef SESSION_INTERNAL_H
#define SESSION_INTERNAL_H

#include <xine/os_types.h>

#define CTRL_PROTO_VERSION    0x1

typedef enum {
  /* Don't change order */
  CMD_PLAY = 1,
  CMD_SLOW_4,
  CMD_SLOW_2,
  CMD_PAUSE,
  CMD_FAST_2,
  CMD_FAST_4,
  CMD_STOP,
  CMD_QUIT,
  CMD_FULLSCREEN,
  CMD_EJECT,
  CMD_AUDIO_NEXT,
  CMD_AUDIO_PREV,
  CMD_SPU_NEXT,
  CMD_SPU_PREV,
  CMD_PLAYLIST_FLUSH,
  CMD_PLAYLIST_ADD,
  CMD_PLAYLIST_FIRST,
  CMD_PLAYLIST_PREV,
  CMD_PLAYLIST_NEXT,
  CMD_PLAYLIST_LAST,
  CMD_PLAYLIST_LOAD,
  CMD_PLAYLIST_STOP,
  CMD_PLAYLIST_CONTINUE,
  CMD_VOLUME,
  CMD_AMP,
  CMD_LOOP,
  CMD_GET_SPEED_STATUS,
  CMD_GET_TIME_STATUS_IN_SECS,
  CMD_GET_TIME_STATUS_IN_POS,
  CMD_GET_VERSION,
  CMD_PING
} ctrl_commands_t;

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

int connect_to_session(int session);
int send_packet(int fd, ctrl_commands_t command, const void *data, uint32_t data_length);
int _send_packet(int fd, const void *data, ctrl_header_packet_t *hdr);
void read_ack(int fd);
void *read_packet(int fd, ctrl_header_packet_t *hdr);
int remote_cmd(int session, ctrl_commands_t command);
int send_string(int session, ctrl_commands_t command, const char *string);

#endif
