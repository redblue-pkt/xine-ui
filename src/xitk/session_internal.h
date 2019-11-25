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

int connect_to_session(int session);
int send_packet(int fd, ctrl_commands_t command, const void *data, uint32_t data_length);
int _send_packet(int fd, const void *data, ctrl_header_packet_t *hdr);
void read_ack(int fd);
void *read_packet(int fd, ctrl_header_packet_t *hdr);
int remote_cmd(int session, ctrl_commands_t command);

#endif
