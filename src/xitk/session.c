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
/* Largely inspired of xmms control socket stuff */

/* required for getsubopt(); the __sun test gives us strncasecmp() on solaris */
#if ! defined (__sun) && ! defined (__OpenBSD__)
#define _XOPEN_SOURCE 500
#endif
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "common.h"

extern gGui_t          *gGui;

#ifndef S_ISSOCK
#define S_ISSOCK(mode) 0
#endif

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

static int           ctrl_fd, session_id = -1;
static int           going;
static pthread_t     thread_server;
static char         *socket_name;

static void send_packet(int fd, ctrl_commands_t command, void *data, uint32_t data_length);

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

    case CMD_PLAY:
      gui_play(NULL, NULL);
      send_ack(shdr);
      break;
      
    case CMD_SLOW_2:
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_2);
      send_ack(shdr);
      break;

    case CMD_SLOW_4:
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
      send_ack(shdr);
      break;

    case CMD_PAUSE:
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
      send_ack(shdr);
      break;

    case CMD_FAST_2:
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_FAST_2);
      send_ack(shdr);
      break;

    case CMD_FAST_4:
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_FAST_4);
      send_ack(shdr);
      break;

    case CMD_STOP:
      gui_stop(NULL, NULL);
      send_ack(shdr);
      break;

    case CMD_QUIT:
      gui_exit(NULL, NULL);
      send_ack(shdr);
      break;

    case CMD_FULLSCREEN:
      gui_set_fullscreen_mode(NULL, NULL);
      send_ack(shdr);
      break;

    case CMD_EJECT:
      gui_eject(NULL, NULL);
      send_ack(shdr);
      break;

    case CMD_AUDIO_NEXT:
      gui_change_audio_channel(NULL, (void *) GUI_NEXT);
      send_ack(shdr);
      break;

    case CMD_AUDIO_PREV:
      gui_change_audio_channel(NULL, (void *) GUI_PREV);
      send_ack(shdr);
      break;

    case CMD_SPU_NEXT:
      gui_change_spu_channel(NULL, (void *) GUI_NEXT);
      send_ack(shdr);
      break;

    case CMD_SPU_PREV:
      gui_change_spu_channel(NULL, (void *) GUI_PREV);
      send_ack(shdr);
      break;
	
    case CMD_PLAYLIST_FLUSH:
      playlist_delete_all(NULL, NULL);
      send_ack(shdr);
      break;
      
    case CMD_PLAYLIST_ADD:
      gui_dndcallback((char *)shdr->data);
      send_ack(shdr);
      break;
      
    case CMD_PLAYLIST_NEXT:
      gui_direct_nextprev(NULL, (void *) GUI_NEXT, 1);
      send_ack(shdr);
      break;

    case CMD_PLAYLIST_PREV:
      gui_direct_nextprev(NULL, (void *) GUI_PREV, 1);
      send_ack(shdr);
      break;

    case CMD_PLAYLIST_LOAD:
      mediamark_load_mediamarks((const char *)shdr->data);
      gui_set_current_mrl((mediamark_t *)mediamark_get_current_mmk());
      playlist_update_playlist();
      if((!is_playback_widgets_enabled()) && gGui->playlist.num)
	enable_playback_controls(1);
      send_ack(shdr);
      break;

    case CMD_VOLUME:
      {
	uint32_t *vol = (uint32_t *)shdr->data;

	if((*vol >= 0) && (*vol <= 100) && (gGui->mixer.caps & MIXER_CAP_VOL)) { 
	  gGui->mixer.volume_level = *vol;
	  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_VOLUME, gGui->mixer.volume_level);
	  osd_draw_bar(_("Audio Volume"), 0, 100, gGui->mixer.volume_level, OSD_BAR_STEPPER);
	}

	send_ack(shdr);
      }
      break;

    case CMD_AMP:
      {
	uint32_t *amp = (uint32_t *)shdr->data;

	if((*amp >= 0) && (*amp <= 200))
	  config_update_num("gui.amp_level", (int) *amp);

	send_ack(shdr);
      }
      break;

    case CMD_GET_VERSION:
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

void deinit_session(void) {
  if(session_id >= 0) {
    struct stat   sstat;
    
    going = 0;
    close(ctrl_fd);
    if(((stat(socket_name, &sstat)) > -1) && (S_ISSOCK(sstat.st_mode)))
      unlink(socket_name);
    
    free(socket_name);
  }
}

int init_session(void) {
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
      }
      else
	continue;
      
      if((bind(ctrl_fd, (struct sockaddr *) &saddr, sizeof (saddr))) != -1) {

	session_id = i;
	listen(ctrl_fd, 100);
	going = 1;
	
	pthread_create(&thread_server, NULL, ctrlsocket_func, NULL);
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

void session_handle_subopt(char *suboptarg, int *session) {
  char        *sopts = suboptarg;
  int          optsess = -1;
  int          playlist_clear, playlist_next, playlist_prev = 0;
  int          audio_next, audio_prev, spu_next, spu_prev;
  int          volume, amp;
  char        *playlist_load = NULL;
  int          fullscreen, s, c;
  uint32_t     state;
  char        *optstr;
  char       **mrls = NULL;
  int          num_mrls = 0;
  const char  *tokens[] = {
    /* Don't change order */
    "play", "slow2", "slow4", "pause", "fast2", "fast4", "stop", "quit", "fullscreen", "eject",
    "audio", "spu", "session", "mrl", "playlist", "pl", "volume", "amp", NULL
  };
  
  playlist_clear = playlist_next = playlist_prev = fullscreen = 0;
  audio_next = audio_prev = spu_next = spu_prev = 0;
  volume = amp = -1;
  state = 0;

  while((c = getsubopt(&sopts, (char *const *)tokens, &optstr)) != -1) {
    switch(c) {

      /* play -> eject */
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      state |= 0x80000000 >> c;
      break;

      /* audio */
    case 10:
      if(!strcasecmp(optstr, "next"))
	audio_next++;
      else if(!strcasecmp(optstr, "prev"))
	audio_prev++;
      break;

      /* spu */
    case 11:
      if(!strcasecmp(optstr, "next"))
	spu_next++;
      else if(!strcasecmp(optstr, "prev"))
	spu_prev++;
      break;

      /* session */
    case 12:
      if((atoi(optstr)) >= 0)
	optsess = atoi(optstr);
      break;

      /* mrl */
    case 13:
      if(num_mrls == 0)
	mrls = (char **) xine_xmalloc(sizeof(char *) * 2);
      else
	mrls = (char **) realloc(mrls, sizeof(char *) * (num_mrls + 2));
	
	mrls[num_mrls++] = strdup(optstr);
	mrls[num_mrls]   = NULL;
      break;

      /* playlist */
    case 14:
    case 15:
      if(!strcasecmp(optstr, "clear"))
	playlist_clear = 1;
      else if(!strcasecmp(optstr, "next"))
	playlist_next++;
      else if(!strcasecmp(optstr, "prev"))
	playlist_prev++;
      else if(!strncasecmp(optstr, "load:", 5))
	playlist_load = strdup(optstr + 5);
      break;

      /* volume */
    case 16:
      volume = strtol(optstr, &optstr, 10);
      break;

      /* amplification */
    case 17:
      amp = strtol(optstr, &optstr, 10);
      break;
      
    }
  }
  
  *session = (optsess >= 0) ? optsess : 0;
  
  if(is_remote_running(*session)) {
    
    if((state << (CMD_QUIT - 1)) & 0x80000000)
      remote_cmd(*session, CMD_QUIT);

    if(playlist_clear)
      remote_cmd(*session, CMD_PLAYLIST_FLUSH);

    if(playlist_load) {
      send_string(*session, CMD_PLAYLIST_LOAD, playlist_load);
      free(playlist_load);
    }

    while(playlist_next) {
      remote_cmd(*session, CMD_PLAYLIST_NEXT);
      playlist_next--;
    }
    
    while(playlist_prev) {
      remote_cmd(*session, CMD_PLAYLIST_PREV);
      playlist_prev--;
    }
    
    if(num_mrls) {
      int i;

      for(i = 0; i < num_mrls; i++)
	send_string(*session, CMD_PLAYLIST_ADD, mrls[i]);

      for(i = 0; i < num_mrls; i++)
	free(mrls[i]);
      
      free(mrls);
    }
    
    /* 
     * CMD_PLAY, CMD_SLOW_4, CMD_SLOW_2, CMD_PAUSE, CMD_FAST_2, 
     * CMD_FAST_4, CMD_STOP, CMD_FULLSCREEN, CMD_EJECT
     */
    if(state) {
      for(s = 0; s < 9; s++) {
	if(((state << s) & 0x80000000) && ((s + 1) != CMD_QUIT))
	  remote_cmd(*session, (s + 1));
      }
    }

    while(audio_next) {
      remote_cmd(*session, CMD_AUDIO_NEXT);
      playlist_next--;
    }

    while(audio_prev) {
      remote_cmd(*session, CMD_AUDIO_PREV);
      playlist_next--;
    }

    while(spu_next) {
      remote_cmd(*session, CMD_SPU_NEXT);
      spu_next--;
    }

    while(spu_prev) {
      remote_cmd(*session, CMD_SPU_PREV);
      spu_prev--;
    }

    if(volume >= 0)
      send_uint32(*session, CMD_VOLUME, (uint32_t) volume);

    if(amp >= 0)
      send_uint32(*session, CMD_AMP, (uint32_t) amp);

  }
  else
    printf(_("Session %d isn't running.\n"), *session);
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
