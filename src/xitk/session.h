/* 
 * Copyright (C) 2000-2004 the xine project
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
#ifndef CTRLSOCK_H
#define CTRLSOCKS_H

#include <xine/os_types.h>

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

void send_uint32(int session, ctrl_commands_t command, uint32_t value);
uint32_t get_uint32(int session, ctrl_commands_t command);
void send_boolean(int session, ctrl_commands_t command, uint8_t value);
uint8_t get_boolean(int session, ctrl_commands_t command);
void send_string(int session, ctrl_commands_t command, char *string);
char *get_string(int session, ctrl_commands_t command);
int is_remote_running(int session);

int init_session(void);
void deinit_session(void);

int session_handle_subopt(char *optarg, int *session);

#endif
