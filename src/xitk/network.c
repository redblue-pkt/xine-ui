/*
 * Copyright (C) 2000-2002 the xine project
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
 * xine network related stuff
 *
 */
/* required for getsubopt(); the __sun test gives us strncasecmp() on solaris */
#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
/* required for strncasecmp() */
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "actions.h"
#include "kbindings.h"
#include "errors.h"

#include "readline.h"
#include "history.h"

#define _FREE(x) {               \
                   if(x) {       \
                     free(x);    \
                     x = NULL;   \
                   }             \
                 }

#define DEFAULT_XINECTL_PORT "6789"
#define DEFAULT_SERVER       "localhost"

#define PROGNAME             "xine-remote"
#define PROGVERSION          "0.1.1"

#define _BUFSIZ              20480

#define COMMANDS_PREFIX      "/\377\200COMMANDS"
extern int errno;


#ifdef NETWORK_CLIENT

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

/* options args */
static const char *short_options = "?hH:P:ncv";
static struct option long_options[] = {
  { "help"           , no_argument      , 0, 'h' },
  { "host"           , required_argument, 0, 'H' },
  { "port"           , required_argument, 0, 'P' },
  { "command"        , no_argument      , 0, 'c' },
  { "noconnect"      , no_argument      , 0, 'n' },
  { "version"        , no_argument      , 0, 'v' },
  { 0                , no_argument      , 0,  0  }
};


typedef struct session_s session_t;
typedef struct session_commands_s session_commands_t;
typedef void (*client_func_t)(session_t *, session_commands_t *, const char *);

static void client_noop(session_t *, session_commands_t *, const char *);
static void client_help(session_t *, session_commands_t *, const char *);
static void client_version(session_t *, session_commands_t *, const char *);
static void client_open(session_t *, session_commands_t *, const char *);
static void client_quit(session_t *, session_commands_t *, const char *);

struct session_s {
  char             host[256];
  char             port[256];
  int              socket;
  int              console;
  int              running;
  char             prompt[544];
  pthread_t        thread;
  pthread_mutex_t  console_mutex;
};

#define ORIGIN_SERVER 1
#define ORIGIN_CLIENT 2

struct session_commands_s {
  char           *command;
  int             origin;
  client_func_t   function;
};

static session_t             session;
static session_commands_t    client_commands[] = {
  { "?",           ORIGIN_CLIENT,   client_help    },
  { "version",     ORIGIN_CLIENT,   client_version },
  { "open",        ORIGIN_CLIENT,   client_open    },
  { "quit",        ORIGIN_CLIENT,   client_quit    },
  { NULL,          ORIGIN_CLIENT,   NULL           }
};

static session_commands_t  **session_commands = NULL;

#else  /* NETWORK_CLIENT */

# include "event.h"
# include <xine.h>
# include <xine/xineutils.h>

static pthread_t   thread_server;

typedef struct commands_s commands_t;
typedef struct client_info_s client_info_t;
typedef void (*cmd_func_t)(commands_t *, client_info_t *, void *);

static void do_commands(commands_t *, client_info_t *, void *);
static void do_help(commands_t *, client_info_t *, void *);
static void do_syntax(commands_t *, client_info_t *, void *);
static void do_mrl(commands_t *, client_info_t *, void *);
static void do_playlist(commands_t *, client_info_t *, void *);
static void do_play(commands_t *, client_info_t *, void *);
static void do_stop(commands_t *, client_info_t *, void *);
static void do_pause(commands_t *, client_info_t *, void *);
static void do_exit(commands_t *, client_info_t *, void *);
static void do_fullscreen(commands_t *, client_info_t *, void *);
static void do_get(commands_t *, client_info_t *, void *);
static void do_set(commands_t *, client_info_t *, void *);

static void handle_client_command(client_info_t *);

#define NOT_PUBLIC  0
#define PUBLIC      1

struct commands_s {
  char         *command;
  int           argtype;
  int           public;
  cmd_func_t    function;
  char         *help;
  char         *syntax;
};

struct client_info_s {
  int                   finished;

  int                   socket;
  struct sockaddr_in    sin;
  char                **actions;

  struct {
    int                  execute;
    char                *remain;
    char                *line;
    char                *command;
    int                  num_args;
    char                *args[256];
  } command;

};

#define NO_ARGS        1
#define REQUIRE_ARGS   2
#define OPTIONAL_ARGS  3

static commands_t commands[] = {
  { "commands",    NO_ARGS,         NOT_PUBLIC,      do_commands,
    "Display all available commands, tab separated, dot terminated.", 
    "commands"
  },
  { "help",        OPTIONAL_ARGS,   PUBLIC,          do_help,
    "Display the help text if a command name is specified, otherwise display "
    "all available commands.",
    "  help [command]"
  },
  { "syntax",      REQUIRE_ARGS,    PUBLIC,          do_syntax,
    "Display a command syntax.",
    "  syntax [command]"
  },

  { "mrl",         REQUIRE_ARGS,    PUBLIC,          do_mrl,
    "manage mrls", 
    "  mrl add <mrl> <mrl> ...\n"
    "  mrl play <mrl>\n"
    "  mrl next\n"
    "  mrl prev"
  },
  { "play",        NO_ARGS,         PUBLIC,          do_play,
    "start playback", 
    "  play"
  },
  { "playlist",    REQUIRE_ARGS,    PUBLIC,          do_playlist,
    "manage playlist", 
    "  playlist show\n"
    "  playlist select <num>\n"
    "  playlist delete all|*\n"
    "  playlist delete <num>"
    "  playlist next\n"
    "  playlist prev"
  },
  { "stop",        NO_ARGS,         PUBLIC,          do_stop,
    "stop playback", 
    "  stop"
  },
  { "pause",       NO_ARGS,         PUBLIC,          do_pause,
    "pause/resume playback", 
    "  pause"
  },
  { "exit",        NO_ARGS,         PUBLIC,          do_exit,
    "close connection", 
    "  exit"
  },
  { "fullscreen",  NO_ARGS,         PUBLIC,          do_fullscreen,
    "fullscreen toggle", 
    "  fullscreen"
  },
  { "get",         REQUIRE_ARGS,    PUBLIC,          do_get,
    "get values", 
    "  get status\n"
    "  get audio channel\n"
    "  get audio lang\n"
    "  get audio volume\n"
    "  get audio mute\n"
    "  get spu channel\n"
    "  get spu lang\n"
    "  get speed"
  },
  { "set",         REQUIRE_ARGS,    PUBLIC,          do_set,
    "set values", 
    "  set audio channel <num>\n"
    "  set audio volume <%>\n"
    "  set audio mute <state>\n"
    "  set spu channel <num>\n"
    "  set speed <SPEED_PAUSE|SPEED_SLOW_4|SPEED_SLOW_2|SPEED_NORMAL|SPEED_FAST_2|SPEED_FAST_4>\n"
    "            <     |     |     /4     |     /2     |     =      |     *2     |     *2     >"
  },
  { NULL,          -1,              NOT_PUBLIC,      NULL, 
    NULL, 
    NULL
  } 
};

extern gGui_t *gGui;

#endif

void sock_err(const char *error_msg, ...) {
  va_list args;
  
  va_start(args, error_msg);
  vfprintf(stderr, error_msg, args);
  va_end(args);
}

int sock_create(const char *service, const char *transport, struct sockaddr_in *sin) {
  struct servent    *iservice;
  struct protoent *itransport;
  int                sock;
  int                type;
  int                on = 1;
  int                proto;

  memset(sin, 0, sizeof(*sin));

  sin->sin_family = AF_INET;
  sin->sin_port   = htons(atoi(service));
  
  if(!sin->sin_port) {
    iservice = getservbyname(service, "tcp");

    if(!service)
      sock_err("Service not registered: %s\n", service);
    
    sin->sin_port = iservice->s_port;
  }

  itransport = getprotobyname(transport);

  if(!transport)
    sock_err("Protocol not registered: %s\n", transport);

  proto = itransport->p_proto;

  if(!strcmp(transport, "udp"))
    type = SOCK_DGRAM;
  else
    type = SOCK_STREAM;

  sock = socket(PF_INET, type, proto);
  
  if(sock < 0) {
    sock_err("Cannot create socket: %s\n", strerror(errno));
    return -1;
  }
  
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
  
  return sock;
}

int sock_client(const char *host, const char *service, const char *transport) {
  struct sockaddr_in   sin;
  struct in_addr       interface;
  struct hostent      *ihost;
  int                  sock;
  
  sock = sock_create(service, transport, &sin);

  sin.sin_addr.s_addr = inet_addr(host);

  if(!inet_aton(host, &interface)) {
    ihost = gethostbyname(host);
    if(!ihost) {
      sock_err("Unknown host: %s\n", host);
      return -1;
    }
    memcpy(&sin.sin_addr, ihost->h_addr, ihost->h_length);
  }
  
  if(interface.s_addr == INADDR_NONE) {
    ihost = gethostbyname(host);
    
    if(!ihost) {
      sock_err("Unknown host: %s\n", host);
      return -1;
    }
    memcpy(&sin.sin_addr, ihost->h_addr, ihost->h_length);
  }
  
  if(connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    sock_err("Unable to connect %s[%s]: %s\n", host, service, strerror(errno));
    return -1;
  }

  return sock;
}

int sock_serv(const char *service, const char *transport, int queue_length) {
  struct sockaddr_in  sin;
  int                 sock;

  sock = sock_create(service, transport, &sin);
  
  if(bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    sock_err("Unable to link socket %s: %s\n", service, strerror(errno));
  
  if(strcmp(transport, "udp") && listen(sock, queue_length) < 0)
    sock_err("Passive mode impossible on %s: %s\n", service, strerror(errno));
  
  return sock;
}

static char *_atoa(char *str) {
  char *pbuf;
  
  pbuf = str;
  
  while(*pbuf != '\0') pbuf++;
  
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
  
  pbuf = str;
  while(*pbuf == '"' || *pbuf == ' ' || *pbuf == '\t') pbuf++;
  
  return pbuf;
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

#ifdef NETWORK_CLIENT

static void send_to_server(session_t *session, char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  int      err;

  if(session->socket < 0)
    return;
  
  va_start(args, msg);
  vsnprintf(buf, _BUFSIZ, msg, args);
  va_end(args);
  
  sprintf(buf, "%s\n", buf);
  
  err = send(session->socket, buf, strlen(buf)+1, MSG_NOSIGNAL);
  if(err < 0) {
    fprintf(stderr, "send() failed: %s\n", strerror(errno));
    session->socket = -1;
  }
}

static void write_to_console_unlocked(session_t *session, const char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  
  if(session->console != -1) {
    va_start(args, msg);
    vsnprintf(buf, _BUFSIZ, msg, args);
    va_end(args);
    
    write(session->console, buf, strlen(buf));
  }
}

static void write_to_console(session_t *session, const char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  
  va_start(args, msg);
  vsnprintf(buf, _BUFSIZ, msg, args);
  va_end(args);
  
  pthread_mutex_lock(&session->console_mutex);
  write_to_console_unlocked(session, buf);
  pthread_mutex_unlock(&session->console_mutex);
}

static void session_update_prompt(session_t *session) {
  char buf[514];

  if(session == NULL)
    return;

  if(session->socket >= 0) {
    snprintf(buf, 514, "%s:%s", session->host, session->port);
  }
  else
    snprintf(buf, 514, "%s", "******:****");

  snprintf(session->prompt, 256, "[%s]%s >", buf, PROGNAME);
}

static void session_create_commands(session_t *session) {
  int num_commands = (sizeof(client_commands) / sizeof(client_commands[0]));
  int i;
  
  if(session == NULL)
    return;
  
  if(session_commands != NULL) {
    i = 0;
    while(session_commands[i]->command != NULL) {
      free(session_commands[i]->command);
      free(session_commands[i]);
      i++;
    }
    free(session_commands[i]);
    free(session_commands);
  }
  
  session_commands = (session_commands_t **) malloc(sizeof(session_commands_t *) * num_commands);
  for(i = 0; client_commands[i].command != NULL; i++) {
    session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
    session_commands[i]->command   = strdup(client_commands[i].command);
    session_commands[i]->origin    = client_commands[i].origin;
    session_commands[i]->function  = client_commands[i].function;
  }
  session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
  session_commands[i]->command   = NULL;
  session_commands[i]->origin    = ORIGIN_CLIENT;
  session_commands[i]->function  = NULL;

}

/*
 * Client commands
 */
static void client_noop(session_t *session, session_commands_t *command, const char *cmd) {
}
static void client_help(session_t *session, session_commands_t *command, const char *cmd) {
  int i = 0, j;
  int maxlen = 0;
  int curpos = 0;
  char buf[_BUFSIZ];
  
  if((session == NULL) || (command == NULL))
    return;
  
  memset(&buf, 0, sizeof(buf));

  while(session_commands[i]->command != NULL) {
    
    if(strlen(session_commands[i]->command) > maxlen)
      maxlen = strlen(session_commands[i]->command);
    
    i++;
  }
  
  maxlen++; 
  i = 0;  
  
  sprintf(buf, "%s", "Available commands are:\n       ");
  curpos += 7;
  
  while(session_commands[i]->command != NULL) {
    if((curpos + maxlen) >= 80) {
      sprintf(buf, "%s\n       ", buf);
      curpos = 7;
    }
    
    sprintf(buf, "%s%s", buf, session_commands[i]->command);
    curpos += strlen(session_commands[i]->command);
    
    for(j = 0; j < (maxlen - strlen(session_commands[i]->command)); j++) {
      sprintf(buf, "%s ", buf);
      curpos++;
    }
    i++;
  }
  
  write_to_console(session, "%s\n", buf);
}
static void client_version(session_t *session, session_commands_t *command, const char *cmd) {

  if((session == NULL) || (command == NULL))
    return;

  write_to_console(session, "%s version %s\n\n", PROGNAME, PROGVERSION);
}
static void client_open(session_t *session, session_commands_t *command, const char *cmd) {
  char  buf[_BUFSIZ];
  char  *pbuf, *p;
  char  *host = NULL, *port = NULL;
  
  if((session == NULL) || (command == NULL))
    return;

  if(cmd) {
    sprintf(buf, "%s", cmd);
    pbuf = buf;
    if((p = strchr(pbuf, ' ')) != NULL) {
      host = _atoa(p);
      if((port = strrchr(p, ':')) != NULL) {
	*port = '\0';
	if(strlen(port) > 1)
	  port++;
	else
	  port = NULL;
      }
    }
    
    if(host != NULL) {
      
      snprintf(session->host, sizeof(session->host), "%s", host);
      
      if(port != NULL) 
	snprintf(session->port, sizeof(session->port), "%s", port);
      
      if(session->socket >= 0)
	send_to_server(session, "exit");
      
      session->socket = sock_client(session->host, session->port, "tcp");
      if(session->socket < 0) {
	write_to_console(session, "opening server '%s' failed: %s.\nExiting.\n", 
			 session->host, strerror(errno));
	
	session->running = 0;
      }
      session_create_commands(session);
      send_to_server(session, "commands");
      session_update_prompt(session);
    }
    else {
      write_to_console(session, "open require arguments (host or host:port)\n");
    }
  }
  
}
static void client_quit(session_t *session, session_commands_t *command, const char *cmd) {
  
  if((session == NULL) || (command == NULL))
    return;
  
  if(session->socket >= 0)
    send_to_server(session, "exit");
  
  session->running = 0;
}
/*
 * End of client commands
 */

/*
 * completion generator functions.
 */
static char *command_generator(const char *text, int state) {
  static int   index, len;
  char        *cmd, *retcmd = NULL;
  
  if(!state) {
    index = 0;
    len = strlen(text);
  }

  if(len) {
    while((cmd = session_commands[index]->command) != NULL) {
      index++;
      if(strncasecmp(cmd, text, len) == 0) {
	retcmd = strdup(cmd);
	return retcmd;
      }
    }
  }
  
  return NULL;
}
static char **completion_function(const char *text, int start, int end) {
  char  **cmd = NULL;
  
  if(start == 0)
    cmd = rl_completion_matches (text, command_generator);
  
  return cmd;
}

static void signals_handler (int sig) {
  switch(sig) {

    /* Kill the line on C-c */
  case SIGINT:
  case SIGTERM:
    if(rl_prompt) { /* readline is running, otherwise we are in script mode */
      rl_kill_full_line(1, 1);
      rl_redisplay();
    }
    else {
      if(session.socket >= 0) {
	send_to_server(&session, "exit");
	close(session.socket);
      }
      exit(1);
    }
    break;

  }
}

static void *select_thread(void *data) {
  session_t       *session = (session_t *) data;
  fd_set           session_sets;
  struct timeval   timeout;
  char             obuffer[_BUFSIZ];
  int              ocount;
  char             buffer[_BUFSIZ];
  int              size;
  int              i;
  char             c;
  int              was_down = 1;
  
  memset(&buffer, 0, sizeof(buffer));
  memset(&obuffer, 0, sizeof(obuffer));
  ocount = 0;

  while(session->running) {
    
    if(session->socket >= 0) {
      if(was_down) {
	FD_ZERO(&session_sets);
	FD_SET(session->socket, &session_sets);
	timeout.tv_sec  = 0;
	timeout.tv_usec = 200000;
	
	select(session->socket + 1, &session_sets, (fd_set *) 0, (fd_set *) 0, &timeout);
	was_down = 0;
      }
      
      if(FD_ISSET(session->socket, &session_sets)) {
	
	size = read(session->socket, buffer, sizeof(buffer));
	if(size) {
	  for(i = 0; i < size; i++) {
	    c = buffer[i];
	    
	    switch (c) {
	    case '\r':
	      break;
	      
	    case '\n':
	      obuffer[ocount++] = c;
	      if(i == (size - 1)) {
		int     pos = (strlen(session->prompt) + rl_end);
		char   *special;
		
		if((special = strstr(obuffer, COMMANDS_PREFIX)) != NULL) {
		  char  *p, *pp;
		  int    special_length = strlen(special);
		  int    i = (sizeof(client_commands) / sizeof(client_commands[0])) - 1;
		  
		  pp = special + 11;
		  while((p = strsep(&pp, "\t")) != NULL) {
		    if(strlen(p)) {
		      while(*p == ' ' || *p == '\t') p++;
		      session_commands = (session_commands_t **) realloc(session_commands, (i + 2) * (sizeof(session_commands_t *)));
		      session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
		      session_commands[i]->command = strdup(p);
		      session_commands[i]->origin = ORIGIN_SERVER;
		      session_commands[i]->function = client_noop;
		      i++;
		    }
		  }
		  
		  /* remove '.\n' in last grabbed command */
		  session_commands[i - 1]->command[strlen(session_commands[i - 1]->command) - 1] = '\0';
		  session_commands[i - 1]->command[strlen(session_commands[i - 1]->command) - 1] = '\0';
		  session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
		  session_commands[i]->command  = NULL;
		  session_commands[i]->origin   = ORIGIN_CLIENT;
		  session_commands[i]->function = NULL;
		  
		  ocount -= special_length;
		  obuffer[ocount] = '\0';
		  
		}
		pthread_mutex_lock(&session->console_mutex);
		/* Erase chars til's col0 */
		while(pos) { 
		  
		  write_to_console_unlocked(session, "\b \b");
		  pos--;
		}
		write_to_console_unlocked(session, obuffer);

		rl_crlf();
		rl_forced_update_display();

		pthread_mutex_unlock(&session->console_mutex);
		
		memset(&obuffer, 0, sizeof(obuffer));
		ocount = 0;
	      }
	      break;
	      
	    default:
	      obuffer[ocount++] = c;
	      break;
	    }
	  }
	}
      }
    }
    else {
      was_down = 1;
    }
  }
  
  pthread_exit(NULL);
}

static void client_handle_command(session_t *session, const char *command) {
  int i, found;
  char cmd[_BUFSIZ], *p;
  
  if((command == NULL) || (strlen(command) == 0))
    return;
  
  /* Get only first arg of command */
  memset(&cmd, 0, sizeof(cmd));
  sprintf(cmd, "%s", command);
  if((p = strchr(cmd, ' ')) != NULL)
    *p = '\0';
  
  i = found = 0;
  /* looking for command matching */
  while((session_commands[i]->command != NULL) && (found == 0)) {
    if(!strncasecmp(cmd, session_commands[i]->command, strlen(cmd))) {
      found ++;
      
      if(session_commands[i]->origin == ORIGIN_CLIENT) {
	if(session_commands[i]->function)
	  session_commands[i]->function(session, session_commands[i], command);
      }
      else {
	send_to_server(session, (char *)command);
      }
      
    }
    i++;
  }

  /* Perhaps a ';' separated commands, so send anyway to server */
  if(found == 0) {
    send_to_server(session, (char *)command);
  }
  
  if(!strncasecmp(cmd, "exit", strlen(cmd))) {
    session_create_commands(session);
    session->socket = -1;
  }
}

static void session_single_shot(session_t *session, int num_commands, char *commands[]) {
  int i;
  char buf[_BUFSIZ];
  
  memset(&buf, 0, sizeof(buf));

  for(i = 0; i < num_commands; i++) {
    if(strlen(buf))
      sprintf(buf, "%s %s", buf, commands[i]);
    else
      sprintf(buf, "%s", commands[i]);
  }

  client_handle_command(session, buf);
  send_to_server(session, "exit");

  /* Wait til' socket is closed. */
  while(session->socket <= 0)
    usleep(10000);
}

static void show_version(void) {
  printf("This is %s - xine's remote control v%s.\n"
	 "(c) 2000, 2001 by G. Bartsch and the xine project team.\n", PROGNAME, PROGVERSION);
}

static void show_usage(void) {
  printf("usage: %s [options]\n", PROGNAME);
  printf("  -H, --host <hostname>        Connect host <hostname>.\n");
  printf("  -P, --port <port>            Connect using <port>.\n");
  printf("  -c, --command <command>      Send <command> to server then quit.\n");
  printf("  -n, --noconnect              Do not connect default server.\n");
  printf("  -v, --version                Display version.\n");
  printf("  -h, --help                   Display this help text.\n");
  printf("\n");
}

int main(int argc, char **argv) {
  int                c = '?';
  int                option_index = 0;
  char              *grabbed_line;
  struct sigaction   action;
  struct servent    *serv_ent;
  int                port_set = 0;
  int                auto_connect = 1;
  int                single_shot = 0;
  void              *p;
  
  /*
   * install sighandler.
   */
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGHUP, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGHUP) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR1, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGUSR1) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR2, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGUSR2) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGINT, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGINT) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGTERM, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGTERM) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGQUIT, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGQUIT) failed: %s\n", strerror(errno));
  }
  action.sa_handler = signals_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGALRM, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGALRM) failed: %s\n", strerror(errno));
  }

  grabbed_line = NULL;

  session.socket = -1;
  pthread_mutex_init(&session.console_mutex, NULL);
  snprintf(session.host, sizeof(session.host), "%s", DEFAULT_SERVER);
  snprintf(session.port, sizeof(session.port), "%s", DEFAULT_XINECTL_PORT);

  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != EOF) {
    switch(c) {
      
    case 'H': /* Set host */
      if(optarg != NULL)
	snprintf(session.host, sizeof(session.host), "%s", optarg);
      break;
      
    case 'P': /* Set port */
      if(optarg != NULL) {
	port_set = 1;
	snprintf(session.port, sizeof(session.port), "%s", optarg);
      }
      break;

    case 'c':
      single_shot = 1;
      break;
      
    case 'n': /* Disable automatic connection */
      auto_connect = 0;
      break;

    case 'v': /* Display version */
      show_version();
      exit(1);
      break;

    case 'h': /* Display usage */
    case '?':
      show_usage();
      exit(1);
      break;
      
    default:
      show_usage();
      fprintf(stderr, "invalid argument %d => exit\n", c);
      exit(1);
    }
    
  }

  if(single_shot)
    session.console = -1;
  else
    session.console = STDOUT_FILENO;
  
  /* Few realdline inits */
  rl_readline_name = PROGNAME;
  rl_set_prompt(session.prompt);
  rl_initialize();
  rl_attempted_completion_function = (CPPFunction *)completion_function;
  
  signal(SIGPIPE, SIG_IGN);
  
  if(!port_set) {
    if((serv_ent = getservbyname("xinectl", "tcp")) != NULL) {
      snprintf(session.port, sizeof(session.port), "%u", ntohs(serv_ent->s_port));
    }
  }
  
  /* Prepare client commands */
  session_create_commands(&session);

  if(auto_connect) {
    session.socket = sock_client(session.host, session.port, "tcp");
    if(session.socket < 0) {
      fprintf(stderr, "opening server '%s' failed: %s\n", session.host, strerror(errno));
      exit(1);
    }
    /* Ask server for available commands */
    send_to_server(&session, "commands");
  }

  write_to_console(&session, "? for help.\n");

  session.running = 1;
  pthread_create(&session.thread, NULL, select_thread, (void *)&session);
  
  if(single_shot) {
    session_single_shot(&session, (argc - optind), &argv[optind]);
    session.running = 0;
    goto __end;
  }

  while(session.running) {
    
    session_update_prompt(&session);

    if((grabbed_line = readline(session.prompt)) == NULL) {
      fprintf(stderr, "%s(%d): readline() failed: %s\n",
	      __FUNCTION__, __LINE__, strerror(errno));
      exit(1);
    }
    
    if(grabbed_line) {
      char *line;
      
      line = _atoa(grabbed_line);
      
      if(strlen(line)) {
	
	add_history(line);
	client_handle_command(&session, line);
      }
    }
    _FREE(grabbed_line);
  }

 __end:
  
  if(session.socket >= 0)
    close(session.socket);
  
  pthread_join(session.thread, &p);
  pthread_mutex_destroy(&session.console_mutex);
  
  return 0;
}

#else

/*
 * Receive msg from socket.
 */
static int msg_from_client(client_info_t *client_info, char *buf, int buflen) {
  int i = 0, r;
  char c;

  if(client_info->socket < 0)
    return 0;
 
  while((r = recv(client_info->socket, &c, 1, 0)) != 0) {
    if(c == '\r' || c == '\n')
      break;

    if(i > buflen)
      break;

    buf[i] = c;
    i++;
  }
  buf[i] = '\n';
  buf[i+1] = 0;

  recv(client_info->socket, &c, 1, 0);
  
  buf[i] = 0;

  return r;
}

/*
 * Send msg to socket.
 */
static void send_to_client(client_info_t *client_info, char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  int      err;

  if(client_info->socket < 0)
    return;
  
  va_start(args, msg);
  vsnprintf(buf, _BUFSIZ, msg, args);
  va_end(args);

  err = send(client_info->socket, buf, strlen(buf), MSG_NOSIGNAL);
  if(err < 0) {
    fprintf(stderr, "send() failed: %s\n", strerror(errno));
    close(client_info->socket);
    client_info->socket = -1;
    client_info->finished = 1;
  }
}

static void handle_xine_error(client_info_t *client_info) {
  int err;
  
  err = xine_get_error(gGui->xine);
  
  switch(err) {
    
  case XINE_ERROR_NONE:
    /* noop */
    break;
    
  case XINE_ERROR_NO_INPUT_PLUGIN:
    send_to_client(client_info, "xine engine error:\n"
		   "There is no available input plugin available to handle '%s'.\n",
		   gGui->filename);
    break;
    
  case XINE_ERROR_NO_DEMUXER_PLUGIN:
    send_to_client(client_info, "xine engine error:\n"
		   "There is no available demuxer plugin to handle '%s'.\n", 
		   gGui->filename);
    break;
    
  default:
    send_to_client(client_info, "xine engine error:\n!! Unhandled error !!\n");
    break;
  }
}

/*
 * ************* COMMANDS ************
 */
/*
 * return argument number.
 */
static int is_args(client_info_t *client_info) {

  if(!client_info)
    return -1;

  return client_info->command.num_args;
}

/*
 * return argumemnt <num>, NULL if there is not argument.
 */
static const char *get_arg(client_info_t *client_info, int num) {

  if((client_info == NULL) || (num < 1) || (client_info->command.num_args < num))
    return NULL;
  
  return(client_info->command.args[num - 1]);
}

/*
 * return 1 if *arg match with argument <pos>
 */
int is_arg_contain(client_info_t *client_info, int pos, const char *arg) {
  
  if(client_info && pos && ((arg != NULL) && (strlen(arg))) && (client_info->command.num_args >= pos)) {
    if(!strncmp(client_info->command.args[pos - 1], arg, strlen(client_info->command.args[pos - 1])))
      return 1;
  }
  
  return 0;
}

/*
 * Set current command line from line.
 */
static void set_command_line(client_info_t *client_info, char *line) {

  if(client_info && line && strlen(line)) {

    if(client_info->command.line)
      client_info->command.line = (char *) realloc(client_info->command.line, strlen(line) + 1);
    else
      client_info->command.line = (char *) xine_xmalloc(strlen(line) + 1);
    
    memset(client_info->command.line, 0, sizeof(client_info->command.line));
    sprintf(client_info->command.line, "%s", line);

  }
}

/*
 * Display help of given command.
 */
static void command_help(commands_t *command, client_info_t *client_info) {

  if(command) {
    if(command->help) {
      send_to_client(client_info, "Help of '%s' command:\n       %s\n", command->command, command->help);
    }
    else
      send_to_client(client_info, "There is no help text for command '%s'\n", command->command);
  }
}

/*
 * Display syntax of given command.
 */
static void command_syntax(commands_t *command, client_info_t *client_info) {
  
  if(command) {
    if(command->syntax) {
      send_to_client(client_info, "Syntax of '%s' command:\n%s\n", command->command, command->syntax);
    }
    else
      send_to_client(client_info, "There is no syntax definition for command '%s'\n", command->command);
  }
}

static void do_commands(commands_t *cmd, client_info_t *client_info, void *data) {
  int i = 0;
  char buf[_BUFSIZ];

  memset(&buf, 0, sizeof(buf));
  sprintf(buf, COMMANDS_PREFIX);
  
  while(commands[i].command != NULL) {
    if(commands[i].public) {
      sprintf(buf, "%s\t%s", buf, commands[i].command);
    }
    i++;
  }
  sprintf(buf, "%s.\n", buf);
  send_to_client(client_info, buf);
}

static void do_help(commands_t *cmd, client_info_t *client_info, void *data) {

  if(!client_info->command.num_args) {
    int i = 0, j;
    int maxlen = 0;
    int curpos = 0;
    
    while(commands[i].command != NULL) {

      if(commands[i].public) {
	if(strlen(commands[i].command) > maxlen)
	  maxlen = strlen(commands[i].command);
      }

      i++;
    }
    
    maxlen++; 
    i = 0;  
    
    send_to_client(client_info, "Available commands are:\n       ");
    curpos += 7;
    
    while(commands[i].command != NULL) {
      if(commands[i].public) {
	if((curpos + maxlen) >= 80) {
	  send_to_client(client_info, "\n       ");
	  curpos = 7;
	}
	
	send_to_client(client_info, "%s", commands[i].command);
	curpos += strlen(commands[i].command);
	
	for(j = 0; j < (maxlen - strlen(commands[i].command)); j++) {
	  send_to_client(client_info, " ");
	  curpos++;
	}
      }
      i++;
    }
    
    send_to_client(client_info, "\n");
  }
  else {
    int i;
    
    for(i = 0; commands[i].command != NULL; i++) {
      if(!strcasecmp((get_arg(client_info, 1)), commands[i].command)) {
	command_help(&commands[i], client_info);
	return;
      }
    }

    send_to_client(client_info, "Unknown command '%s'.\n", (get_arg(client_info, 1)));
  }
}

static void do_syntax(commands_t *command, client_info_t *client_info, void *data) {
  int i;
  
  for(i = 0; commands[i].command != NULL; i++) {
    if(!strcasecmp((get_arg(client_info, 1)), commands[i].command)) {
      command_syntax(&commands[i], client_info);
      return;
    }
  }

  send_to_client(client_info, "Unknown command '%s'.\n", (get_arg(client_info, 1)));
}

static void do_mrl(commands_t *cmd, client_info_t *client_info, void *data) {
  int nargs;

  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      if(is_arg_contain(client_info, 1, "next")) {
	gGui->ignore_status = 1;
	xine_stop (gGui->xine);
	gGui->ignore_status = 0;
	gui_status_callback (XINE_STOP);
      }
      else if(is_arg_contain(client_info, 1, "prev")) {
	gGui->ignore_status = 1;
	xine_stop (gGui->xine);
	gGui->playlist_cur--;
	if ((gGui->playlist_cur>=0) && (gGui->playlist_cur < gGui->playlist_num)) {
	  gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	  if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
	    gui_handle_xine_error();
	  
	} else {
	  gGui->playlist_cur = 0;
	}
	gGui->ignore_status = 0;
      }
    }
    else if(nargs >= 2) {
      
      if(is_arg_contain(client_info, 1, "add")) {
	int argc = 2;
	
	while((get_arg(client_info, argc)) != NULL) {
	  gui_dndcallback((char *)(get_arg(client_info, argc)));
	  argc++;
	}
      }
      else if (is_arg_contain(client_info, 1, "play")) {
	gui_dndcallback((char *)(get_arg(client_info, 2)));
	
	if((xine_get_status(gGui->xine) != XINE_STOP)) {
	  gGui->ignore_status = 1;
	  xine_stop (gGui->xine);
	  gGui->ignore_status = 0;
	}
	gui_set_current_mrl(gGui->playlist[gGui->playlist_num - 1]);
	if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
	  handle_xine_error(client_info);
      }
    }
  }
}

static void do_playlist(commands_t *cmd, client_info_t *client_info, void *data) {
  int nargs;
  
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      if(is_arg_contain(client_info, 1, "show")) {
	int i;
	if(gGui->playlist_num) {
	  for(i = 0; gGui->playlist[i] != NULL; i++) {
	    send_to_client(client_info, "%5d %s\n", i, gGui->playlist[i]);
	  }
	}
	else
	  send_to_client(client_info, "empty playlist.");

	send_to_client(client_info, "\n");
      }
      else if(is_arg_contain(client_info, 1, "next")) { /* Alias of mrl next */
	set_command_line(client_info, "mrl next");
	handle_client_command(client_info);
      }
      else if(is_arg_contain(client_info, 1, "prev")) { /* Alias of mrl prev */
	set_command_line(client_info, "mrl prev");
	handle_client_command(client_info);
      }
    }
    else if(nargs >= 2) {
      
      if(is_arg_contain(client_info, 1, "select")) {
	int j;
	
	j = atoi(get_arg(client_info, 2));
	
	if((j >= 0) && (j <= gGui->playlist_num)) {
	  gui_set_current_mrl(gGui->playlist[j]);
	}
      }
      else if(is_arg_contain(client_info, 1, "delete")) {
	int i;
	
	if((is_arg_contain(client_info, 2, "all")) || 
	   (is_arg_contain(client_info, 2, "*"))) {

	  for(i = 0; i < gGui->playlist_num; i++) {
	    gGui->playlist[i] = NULL;
	  }
	  
	  gGui->playlist_num = 0;
	  gGui->playlist_cur = 0;
	}
	else {
	  int j;
	  
	  j = atoi(get_arg(client_info, 2));
	  
	  if(j >= 0) {
	    for(i = j; i < gGui->playlist_num; i++) {
	      gGui->playlist[i] = gGui->playlist[i+1];
	    }
	    gGui->playlist_num--;
	    if(gGui->playlist_cur) gGui->playlist_cur--;
	  }
	}
	
	if(gGui->playlist_num)
	  gui_set_current_mrl(gGui->playlist[gGui->playlist_cur]);
	else
	  gui_set_current_mrl(NULL);
      }
      
    }
  }
}

static void do_play(commands_t *cmd, client_info_t *client_info, void *data) {

  if (xine_get_status (gGui->xine) != XINE_PLAY) {
    if(!xine_play (gGui->xine, gGui->filename, 0, 0 ))
      handle_xine_error(client_info);
  } else {
    xine_set_speed(gGui->xine, SPEED_NORMAL);
  }

}

static void do_stop(commands_t *cmd, client_info_t *client_info, void *data) {
  gGui->ignore_status = 1;
  xine_stop (gGui->xine);
  gGui->ignore_status = 0; 
}

static void do_pause(commands_t *cmd, client_info_t *client_info, void *data) {
  if (xine_get_speed (gGui->xine) != SPEED_PAUSE)
    xine_set_speed(gGui->xine, SPEED_PAUSE);
  else
    xine_set_speed(gGui->xine, SPEED_NORMAL);
}

static void do_exit(commands_t *cmd, client_info_t *client_info, void *data) {
  if(client_info) {
    client_info->finished = 1;
  }
}

static void do_fullscreen(commands_t *cmd, client_info_t *client_info, void *data) {
  action_id_t action = ACTID_TOGGLE_FULLSCREEN;

  gui_execute_action_id(action);
}

static void do_get(commands_t *cmd, client_info_t *client_info, void *data) {
  int nargs;
  
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      if(is_arg_contain(client_info, 1, "status")) {
	char buf[64];
	int  status;

	sprintf(buf, "%s", "Current status: ");
	status = xine_get_status(gGui->xine);
	
	switch(status) {
	case XINE_STOP:
	  sprintf(buf, "%s%s", buf, "XINE_STOP");
	  break;
	case XINE_PLAY:
	  sprintf(buf, "%s%s", buf, "XINE_PLAY");
	  break;
	case XINE_QUIT:
	  sprintf(buf, "%s%s", buf, "XINE_QUIT");
	  break;
	default:
	  sprintf(buf, "%s%s", buf, "*UNKNOWN*");
	  break;
	}
	sprintf(buf, "%s%c", buf, '\n');
	send_to_client(client_info, buf);
      }
      else if(is_arg_contain(client_info, 1, "speed")) {
	char buf[64];
	int  speed;
	
	sprintf(buf, "%s", "Current speed: ");
	speed = xine_get_speed(gGui->xine);
	
	switch(speed) {
	case SPEED_PAUSE:
	  sprintf(buf, "%s%s", buf, "SPEED_PAUSE");
	  break;
	case SPEED_SLOW_4:
	  sprintf(buf, "%s%s", buf, "SPEED_SLOW_4");
	  break;
	case SPEED_SLOW_2:
	  sprintf(buf, "%s%s", buf, "SPEED_SLOW_2");
	  break;
	case SPEED_NORMAL:
	  sprintf(buf, "%s%s", buf, "SPEED_NORMAL");
	  break;
	case SPEED_FAST_2:
	  sprintf(buf, "%s%s", buf, "SPEED_FAST_2");
	  break;
	case SPEED_FAST_4:
	  sprintf(buf, "%s%s", buf, "SPEED_FAST_4");
	  break;
	default:
	  sprintf(buf, "%s%s", buf, "*UNKNOWN*");
	  break;
	}

	sprintf(buf, "%s%c", buf, '\n');
	send_to_client(client_info, buf);
      }
    }
    else if(nargs >= 2) {
      if(is_arg_contain(client_info, 1, "audio")) {
	if(is_arg_contain(client_info, 2, "channel")) {
	  send_to_client(client_info, "Current audio channel: %d\n", 
			 (xine_get_audio_selection(gGui->xine)));
	}
	else if(is_arg_contain(client_info, 2, "lang")) {
	  char buf[20];

	  xine_get_audio_lang(gGui->xine, buf);
	  send_to_client(client_info, "Current audio language: %s\n", buf);
	}
	else if(is_arg_contain(client_info, 2, "volume")) {
	  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
	    send_to_client(client_info, "Current audio volume: %d\n", gGui->mixer.volume_level);
	  }
	  else
	    send_to_client(client_info, "Audio is disabled.\n");
	}
	else if(is_arg_contain(client_info, 2, "mute")) {
	  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
	    send_to_client(client_info, "Current audio mute: %d\n", gGui->mixer.mute);
	  }
	  else
	    send_to_client(client_info, "Audio is disabled.\n");
	}
      }
      else if(is_arg_contain(client_info, 1, "spu")) {
	if(is_arg_contain(client_info, 2, "channel")) {
	  send_to_client(client_info, "Current spu channel: %d\n", 
			 (xine_get_spu_channel(gGui->xine)));
	}
	else if(is_arg_contain(client_info, 2, "lang")) {
	  char buf[20];

	  xine_get_audio_lang (gGui->xine, buf);
	  send_to_client(client_info, "Current spu language: %s\n", buf);
	}
      }
    }
  }
}
	
static void do_set(commands_t *cmd, client_info_t *client_info, void *data) {
  int nargs;

  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 2) {
      if(is_arg_contain(client_info, 1, "speed")) {
	int speed;
	
	if((is_arg_contain(client_info, 2, "SPEED_PAUSE")) || 
	   (is_arg_contain(client_info, 2, "|")))
	  speed = SPEED_PAUSE;
	else if((is_arg_contain(client_info, 2, "SPEED_SLOW_4")) ||
		(is_arg_contain(client_info, 2, "/4")))
	  speed = SPEED_SLOW_4;
	else if((is_arg_contain(client_info, 2, "SPEED_SLOW_2")) ||
		(is_arg_contain(client_info, 2, "/2")))
	  speed = SPEED_SLOW_2;
	else if((is_arg_contain(client_info, 2, "SPEED_NORMAL")) ||
		(is_arg_contain(client_info, 2, "=")))
	  speed = SPEED_NORMAL;
	else if((is_arg_contain(client_info, 2, "SPEED_FAST_2")) ||
		(is_arg_contain(client_info, 2, "*2")))
	  speed = SPEED_FAST_2;
	else if((is_arg_contain(client_info, 2, "SPEED_FAST_4")) ||
		(is_arg_contain(client_info, 2, "*4")))
	  speed = SPEED_FAST_4;
	else
	  speed = atoi((get_arg(client_info, 2)));
	
	xine_set_speed(gGui->xine, speed);
      }
    }
    else if(nargs >= 3) {
      if(is_arg_contain(client_info, 1, "audio")) {
	if(is_arg_contain(client_info, 2, "channel")) {
	  xine_select_audio_channel(gGui->xine, (atoi(get_arg(client_info, 3))));
	}
	else if(is_arg_contain(client_info, 2, "volume")) {
	  if(gGui->mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
	    int vol = atoi(get_arg(client_info, 3));

	    if(vol < 0) vol = 0;
	    if(vol > 100) vol = 100;

	    gGui->mixer.volume_level = vol;
	    xine_set_audio_property(gGui->xine, 
				    gGui->mixer.volume_mixer, gGui->mixer.volume_level);
	  }
	  else
	    send_to_client(client_info, "Audio is disabled.\n");
	}
	else if(is_arg_contain(client_info, 2, "mute")) {
	  if(gGui->mixer.caps & AO_CAP_MUTE_VOL) {
	    gGui->mixer.mute = get_bool_value((get_arg(client_info, 3)));
	    xine_set_audio_property(gGui->xine, AO_PROP_MUTE_VOL, gGui->mixer.mute);
	  }
	  else
	    send_to_client(client_info, "Audio is disabled.\n");
	}
      }
      else if(is_arg_contain(client_info, 1, "spu")) {
	if(is_arg_contain(client_info, 2, "channel")) {
	  xine_select_spu_channel(gGui->xine, (atoi(get_arg(client_info, 3))));
	}
      }
    }
  }
}
/*
 * *********** COMMANDS ENDS **********
 */
/*
 *
 */
static void say_hello(client_info_t *client_info) {
  char buf[256] = "";
  char myfqdn[256] = "";
  struct hostent *hp = NULL;
  
  if(!gethostname(myfqdn, 255) && (hp = gethostbyname(myfqdn)) != NULL) {
    sprintf(buf, "%s %s %s remote server. Nice to meet you.\n", hp->h_name, PACKAGE, VERSION);
  }
  else {
    sprintf(buf, "%s %s remote server. Nice to meet you.\n", PACKAGE, VERSION);
  }
  send_to_client(client_info, buf);
  
}

/*
 * Push first command in remain queue to current command.
 */
static void parse_destock_remain(client_info_t *client_info) {
  char   *p, *pp, *c;
  char   *pbuf;
  char    commandline[_BUFSIZ];
  char    remaining[_BUFSIZ];

  /* 
   * Remain and line are already filled, perhaps a forced command line
   * happen here: don't handle it here
   */

  if((client_info->command.remain) && (client_info->command.line))
    return;
  
  if((client_info->command.remain == NULL) 
     && (client_info->command.line && (strchr(client_info->command.line, ';')))) {
    client_info->command.remain = strdup(client_info->command.line);
    _FREE(client_info->command.line);
  }

  if(client_info->command.remain && 
     ((client_info->command.line == NULL) || 
      ((client_info->command.line != NULL) && (strlen(client_info->command.line) == 0)))) {
    
    memset(&commandline, 0, sizeof(commandline));
    memset(&remaining, 0, sizeof(remaining));
    
    if((p = strchr(client_info->command.remain, ';')) != NULL) {
      
      pp = client_info->command.remain;
      pbuf = commandline;
      
      while(pp < p) {
	*pbuf = *pp;
	pp++;
	pbuf++;
      }
      *pbuf = '\0';
      
      if(strlen(commandline)) {
	c = &commandline[strlen(commandline)];
	while((*c == ';') && (c >= commandline)) {
	  *c = '\0';
	  c--;
	}
      }
      
      c = _atoa(commandline);
      
      if(*p == ';')
	p++;
      
      if(p)
	sprintf(remaining, "%s", (_atoa(p)));
      
      if(client_info->command.line)
	client_info->command.line = (char *) realloc(client_info->command.line, 
						     sizeof(char *) * (strlen(c) + 1));
      else
	client_info->command.line = (char *) xine_xmalloc(sizeof(char *) * (strlen(c) + 1));
      
      sprintf(client_info->command.line, "%s", c);
      
      if(p) {
	/* remove last ';' */
	if(strchr(remaining, ';')) {
	  p = &remaining[strlen(remaining)];
	  while((*p == ';') && (p >= remaining)) {
	    *p = '\0';
	    p--;
	  }
	}
	
	if(strlen(remaining)) {
	  client_info->command.remain = (char *) realloc(client_info->command.remain, 
							 sizeof(char *) * (strlen(remaining) + 1));
	  sprintf(client_info->command.remain, "%s", remaining);
	}
	else {
	  _FREE(client_info->command.remain);
	}

      }
      else {
	_FREE(client_info->command.remain);
      }
      
    }
    else { /* no ';' in remain, copy AS IS remain to line */
      if(client_info->command.line)
	client_info->command.line = (char *) realloc(client_info->command.line, 
						     sizeof(char *) * (strlen(client_info->command.remain) + 1));
      else
	client_info->command.line = (char *) xine_xmalloc(sizeof(char *) * 
							  (strlen(client_info->command.remain) + 1));
      
      sprintf(client_info->command.line, "%s", client_info->command.remain);
      
      _FREE(client_info->command.remain);
    }
  }
}

/*
 * handle multicommand line.
 */
static void parse_handle_multicommands(client_info_t *client_info) {

  if(client_info->command.remain) {
    fprintf(stderr, "Ooch, Unexpected state, remain isn't empty\n");
  }
  else {
    client_info->command.remain = strdup(client_info->command.line);
    parse_destock_remain(client_info);
  }
}

/*
 * parse command, extract arguments.
 */
static void parse_command(client_info_t *client_info) {
  char  *cmd;
  int    i = 0;
  
  parse_destock_remain(client_info);

  if(strchr(client_info->command.line, ';'))
    parse_handle_multicommands(client_info);

  cmd = client_info->command.line;
  
  if(client_info->command.num_args) {
    for(i = 0; i < client_info->command.num_args; i++)
      memset(client_info->command.args[i], 0, sizeof(client_info->command.args[i]));
  }
  
  client_info->command.num_args = 0;
  
  while(*cmd != '\0' && (*cmd != ' ' && *(cmd - 1) != '\\') && *cmd != '\t') 
    cmd++;
  
  if(cmd >= (client_info->command.line + strlen(client_info->command.line)))
    cmd = NULL;

  if(cmd) {
    if(client_info->command.command)
      client_info->command.command = (char *) realloc(client_info->command.command, 
						      strlen(client_info->command.line) - strlen(cmd) + 1);
    else
      client_info->command.command = (char *) xine_xmalloc(strlen(client_info->command.line) - strlen(cmd) + 1);

    memset(client_info->command.command, 0, sizeof(client_info->command.command));
    snprintf(client_info->command.command, 
	     (strlen(client_info->command.line) - strlen(cmd))+1, "%s", client_info->command.line);

    cmd = _atoa(cmd);
    
    /*
     * Extract and store args
     */
    if(cmd < client_info->command.line + strlen(client_info->command.line)) {
      char   *pcmd, *pb, buf[256];
      int     nargs = 0, get_quote = 0;
      
      pcmd = cmd;
      memset(&buf, 0, sizeof(buf));
      pb = buf;
      
      while(*(pcmd - 1) != '\0') {
	
	switch(*pcmd) {

	case '\'':
	  if(*(pcmd - 1) != '\\') {
	    get_quote++;
	    if(get_quote == 2)
	      get_quote = 0;
	  }
	  else {
	    pb--;
	    goto __store_char;
	  }

	  break;
	  
	case '\t':
	case '\0':
	  goto __end_args;
	  break;
	  
	case ' ':
	  if((*(pcmd - 1) != '\\') && (get_quote == 0)) {

	  __end_args:
	    sprintf(client_info->command.args[nargs], "%s", _atoa(buf));
	    nargs++;
	    memset(&buf, 0, sizeof(buf));
	    pb = buf;
	  }
	  else {
	    if(*(pcmd - 1) == '\\')
	      pb--;
	    goto __store_char;
	  }
	  break;

	default:
	__store_char:
	  *pb = *pcmd;
	  pb++;
	  break; 
	}
	pcmd++;
      }
      client_info->command.num_args = nargs;
    }
  }
  else {
    if(client_info->command.command)
      client_info->command.command = (char *) realloc(client_info->command.command, strlen(client_info->command.line) + 1);
    else
      client_info->command.command = (char *) xine_xmalloc(strlen(client_info->command.line) + 1);
    
    memset(client_info->command.command, 0, sizeof(client_info->command.command));
    sprintf(client_info->command.command, "%s", client_info->command.line);
  }

#if 0
  {
    int k;
    pinfo("client_info->command = '%s'\n", client_info->command.command);
    if(client_info->command.num_args)
      for(k = 0; k < client_info->command.num_args; k++)
	pinfo("client_info->command_arg[%d] = '%s'\n", k, client_info->command.args[k]);
  }
#endif
}

/*
 * Handle user entered commands.
 */
static void handle_client_command(client_info_t *client_info) {
  int i, found;
  
  /* pass command line to the chainsaw */
  
  do {
    
    parse_command(client_info);
    
    if(strlen(client_info->command.line)) {
      
      /*
       * Is the command match with one available ?
       */
      i = found = 0;
      while((commands[i].command != NULL) && (found == 0)) {
	if(!strncasecmp(client_info->command.command, commands[i].command, strlen(client_info->command.command))) {
	  if((commands[i].argtype == REQUIRE_ARGS) 
	     && (client_info->command.num_args <= 0)) {
	    send_to_client(client_info, "Command '%s' require argument(s).\n", commands[i].command);
	    found++;
	  }
	  else if((commands[i].argtype == NO_ARGS) && (client_info->command.num_args > 0)) {
	    send_to_client(client_info, "Command '%s' doesn't require argument.\n", commands[i].command);
	    found++;
	  }
	  else {
	    commands[i].function(&commands[i], client_info, NULL);
	    found++;
	  }
	}
	i++;
      }
      
      if(!found)
	send_to_client(client_info, "unhandled command '%s'.\n", client_info->command.command);
      
    }
    
    _FREE(client_info->command.line);

  } while((client_info->command.remain != NULL));
}

/*
 *
 */
static void *client_thread(void *data) {
  client_info_t     *client_info = (client_info_t *) data;
  char               buf[_BUFSIZ];
  int                i;

  client_info->finished = 0;

  for(i = 0; i < 256; i++)
    client_info->command.args[i] = (char *) xine_xmalloc(sizeof(char *) * 2048);
  
  say_hello(client_info);
  
  while(!client_info->finished) {
    
    if(msg_from_client(client_info, buf, _BUFSIZ)) {
      
      if(strlen(buf)) {
	set_command_line(client_info, buf);
	handle_client_command(client_info);
      }
    }
    memset(&buf, 0, sizeof(buf));
  }
  
  close(client_info->socket);
  
#warning FIXME, free commands
  for(i = 0; i < 256; i++)
    _FREE(client_info->command.args[i]);
  
  free(client_info);

  pthread_exit(NULL);
}

/*
 *
 */
static void *server_thread(void *data) {
  char            *service;
  struct servent  *serv_ent;
  int              msock;
 
  /*  Search in /etc/services if a xinectl entry exist */
  if((serv_ent = getservbyname("xinectl", "tcp")) != NULL) {
    service = (char *) xine_xmalloc(ntohs(serv_ent->s_port));
    sprintf(service, "%u", ntohs(serv_ent->s_port));
  }
  else
    service = DEFAULT_XINECTL_PORT;
  
  while(gGui->running) {
    client_info_t       *client_info;
    int                  lsin;
    pthread_t            thread_client;
    
    msock = sock_serv(service, "tcp", 5);
    
    client_info = (client_info_t *) xine_xmalloc(sizeof(client_info_t));
    lsin = sizeof(client_info->sin);
    
    client_info->socket = accept(msock, (struct sockaddr *)&(client_info->sin), &lsin);
    
    if(client_info->socket < 0) {
      
      if(errno == EINTR)
	continue;
      
      sock_err("accept: %s\n", strerror(errno));
    }
    
    close(msock);

    pthread_create(&thread_client, NULL, client_thread, (void *)client_info);
  }
  free(service);
 
  pthread_exit(NULL);
}

/*
 *
 */
void start_remote_server(void) {

  if(gGui->network) {
    pthread_create(&thread_server, NULL, server_thread, NULL);
  }
  
}
#endif
