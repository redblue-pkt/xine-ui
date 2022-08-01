/*
 * Copyright (C) 2000-2022 the xine project
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
 * xine network related stuff
 *
 */
/* required for getsubopt(); the __sun test avoids compilation problems on
    solaris. On FreeBSD defining this disable BSD functions to be visible
    and remove INADDR_NONE */
#if ! defined (__sun__) && ! defined (__OpenBSD__)  && ! defined(__FreeBSD__) && ! defined(__NetBSD__) && ! defined(__APPLE__) && ! defined (__DragonFly__)
#define _XOPEN_SOURCE 500
#endif
/* required for strncasecmp() */
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1
/* required to enable POSIX variant of getpwuid_r on solaris */
#define _POSIX_PTHREAD_SEMANTICS 1

//#warning IMPLEMENT POST SUPPORT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_READLINE

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#if !defined(__hpux)
#include <string.h>
#endif
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <readline.h>
#include <history.h>

#include "common.h"
#include "network.h"
#include "panel.h"
#include "playlist.h"
#include "snapshot.h"
#include "actions.h"
#include "event.h"
#include "errors.h"

#ifndef	MSG_NOSIGNAL
#define	MSG_NOSIGNAL	     0
#endif

#define DEFAULT_XINECTL_PORT "6789"
#define DEFAULT_SERVER       "localhost"

#define PROGNAME             "xine-remote"
#define PROGVERSION          "0.1.2"

#define _BUFSIZ              20480

#define COMMANDS_PREFIX      "/\377\200COMMANDS"

#ifdef NETWORK_CLIENT

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

/* options args */
static const char short_options[] = "?hH:P:ncv";
static const struct option long_options[] = {
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
static void client_close(session_t *, session_commands_t *, const char *);
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
  int             enable;
  client_func_t   function;
};

typedef struct client_commands_s {
  const char     *command;
  int             origin;
  int             enable;
  client_func_t   function;
} client_commands_t;

static session_t             session;
static const client_commands_t client_commands[] = {
  { "?",           ORIGIN_CLIENT,   1, client_help    },
  { "version",     ORIGIN_CLIENT,   1, client_version },
  { "open",        ORIGIN_CLIENT,   1, client_open    },
  { "close",       ORIGIN_CLIENT,   1, client_close   },
  { "quit",        ORIGIN_CLIENT,   1, client_quit    },
  { NULL,          ORIGIN_CLIENT,   1, NULL           }
};

static session_commands_t  **session_commands = NULL;

#else  /* NETWORK_CLIENT */

# include <pwd.h>
# include <xine.h>
# include <xine/xineutils.h>

typedef struct commands_s commands_t;
typedef struct client_info_s client_info_t;
typedef struct passwds_s passwds_t;
typedef void (*cmd_func_t)(const commands_t *, client_info_t *);

static passwds_t   **passwds = NULL;

static void do_commands(const commands_t *, client_info_t *);
static void do_help(const commands_t *, client_info_t *);
static void do_syntax(const commands_t *, client_info_t *);
static void do_auth(const commands_t *, client_info_t *);
static void do_mrl(const commands_t *, client_info_t *);
static void do_playlist(const commands_t *, client_info_t *);
static void do_play(const commands_t *, client_info_t *);
static void do_stop(const commands_t *, client_info_t *);
static void do_pause(const commands_t *, client_info_t *);
static void do_exit(const commands_t *, client_info_t *);
static void do_fullscreen(const commands_t *, client_info_t *);
#ifdef HAVE_XINERAMA
static void do_xinerama_fullscreen(const commands_t *, client_info_t *);
#endif
static void do_get(const commands_t *, client_info_t *);
static void do_set(const commands_t *, client_info_t *);
static void do_gui(const commands_t *, client_info_t *);
static void do_event(const commands_t *, client_info_t *);
static void do_seek(const commands_t *, client_info_t *);
static void do_halt(const commands_t *, client_info_t *);
static void do_snap(const commands_t *, client_info_t *);

static void handle_client_command(client_info_t *);

#define MAX_NAME_LEN        16
#define MAX_PASSWD_LEN      16

#define NO_ARGS             1
#define REQUIRE_ARGS        2
#define OPTIONAL_ARGS       3

#define NOT_PUBLIC          0
#define PUBLIC              1

#define AUTH_UNNEEDED       0
#define NEED_AUTH           1

#define PASSWD_ALL_ALLOWED  1
#define PASSWD_ALL_DENIED   2
#define PASSWD_USER_ALLOWED 3
#define PASSWD_USER_DENIED  4

struct commands_s {
  const char   *command;
  int           argtype;
  int           public;
  int           need_auth;
  cmd_func_t    function;
  const char   *help;
  const char   *syntax;
};

struct client_info_s {
  gGui_t               *gui;
  int                   authentified;
  char                  name[MAX_NAME_LEN + 1];
  char                  passwd[MAX_PASSWD_LEN + 1];

  int                   finished;

  int                   socket;
  union {
    struct sockaddr_in  in;
    struct sockaddr     sa;
  } fsin;
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

struct passwds_s {
  char     *ident;
  char     *passwd;
};

typedef struct {
  FILE             *fd;
  char             *ln;
  char              buf[256];
} fileobj_t;

static const commands_t commands[] = {
  { "commands",    NO_ARGS,         NOT_PUBLIC,      AUTH_UNNEEDED, do_commands,
    "Display all available commands, tab separated, dot terminated.",
    "commands"
  },
  { "help",        OPTIONAL_ARGS,   PUBLIC,          AUTH_UNNEEDED, do_help,
    "Display the help text if a command name is specified, otherwise display "
    "all available commands.",
    "  help [command]"
  },
  { "syntax",      REQUIRE_ARGS,    PUBLIC,          AUTH_UNNEEDED, do_syntax,
    "Display a command syntax.",
    "  syntax [command]"
  },
  { "identify",    REQUIRE_ARGS,    PUBLIC,          AUTH_UNNEEDED, do_auth,
    "Identify client.",
    "identify <ident> <password>."
  },
  { "mrl",         REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_mrl,
    "manage mrls",
    "  mrl add <mrl> <mrl> ...\n"
    "  mrl play <mrl>\n"
    "  mrl next\n"
    "  mrl prev"
  },
  { "play",        NO_ARGS,         PUBLIC,          NEED_AUTH,     do_play,
    "start playback",
    "  play"
  },
  { "playlist",    REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_playlist,
    "manage playlist",
    "  playlist show\n"
    "  playlist select <num>\n"
    "  playlist delete all|*\n"
    "  playlist delete <num>\n"
    "  playlist first\n"
    "  playlist prev\n"
    "  playlist next\n"
    "  playlist last\n"
    "  playlist stop\n"
    "  playlist continue"
  },
  { "stop",        NO_ARGS,         PUBLIC,          NEED_AUTH,     do_stop,
    "stop playback",
    "  stop"
  },
  { "pause",       NO_ARGS,         PUBLIC,          NEED_AUTH,     do_pause,
    "pause/resume playback",
    "  pause"
  },
  { "exit",        NO_ARGS,         PUBLIC,          AUTH_UNNEEDED, do_exit,
    "close connection",
    "  exit"
  },
  { "fullscreen",  NO_ARGS,         PUBLIC,          NEED_AUTH,     do_fullscreen,
    "fullscreen toggle",
    "  fullscreen"
  },
#ifdef HAVE_XINERAMA
  { "xineramafull" ,  NO_ARGS,      PUBLIC,          NEED_AUTH,     do_xinerama_fullscreen,
    "xinerama fullscreen toggle",
    "  expand display on further screens"
  },
#endif
  { "get",         REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_get,
    "get values",
    "  get status\n"
    "  get audio channel\n"
    "  get audio lang\n"
    "  get audio volume\n"
    "  get audio mute\n"
    "  get spu channel\n"
    "  get spu lang\n"
    "  get spu offset\n"
    "  get av offset\n"
    "  get speed\n"
    "  get position\n"
    "  get length\n"
    "  get loop"
  },
  { "set",         REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_set,
    "set values",
    "  set audio channel <num>\n"
    "  set audio volume <%>\n"
    "  set audio mute <state>\n"
    "  set spu channel <num>\n"
    "  set spu offset <offset>\n"
    "  set av offset <offset>\n"
    "  set speed <XINE_SPEED_PAUSE|XINE_SPEED_SLOW_4|XINE_SPEED_SLOW_2|XINE_SPEED_NORMAL|XINE_SPEED_FAST_2|XINE_SPEED_FAST_4>\n"
    "            <        |       |        /4       |        /2       |        =        |        *2       |        *4       >\n"
    "  set loop <no | loop | repeat | shuffle | shuffle+>"
  },
  { "gui",         REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_gui,
    "manage gui windows",
    "  gui hide\n"
    "  gui output\n"
    "  gui panel\n"
    "  gui playlist\n"
    "  gui control\n"
    "  gui mrl\n"
    "  gui setup\n"
    "  gui vpost\n"
    "  gui help\n"
    "  gui log"
  },
  { "event",       REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_event,
    "Send an event to xine",
    "  event menu1\n"
    "  event menu2\n"
    "  event menu3\n"
    "  event menu4\n"
    "  event menu5\n"
    "  event menu6\n"
    "  event menu7\n"
    "  event 0"
    "  event 1"
    "  event 2"
    "  event 3"
    "  event 4"
    "  event 5"
    "  event 6"
    "  event 7"
    "  event 8"
    "  event 9"
    "  event +10"
    "  event up\n"
    "  event down\n"
    "  event left\n"
    "  event right\n"
    "  event next\n"
    "  event previous\n"
    "  event angle next\n"
    "  event angle previous\n"
    "  event select"
  },
  { "seek",        REQUIRE_ARGS,    PUBLIC,          NEED_AUTH,     do_seek,
    "Seek in current stream",
    "seek %[0...100]\n"
    "seek [+|-][secs]"
  },
  { "halt",        NO_ARGS,         PUBLIC,          NEED_AUTH,     do_halt,
    "Stop xine program",
    "halt"
  },
  { "snapshot",    NO_ARGS,         PUBLIC,          NEED_AUTH,     do_snap,
    "Take a snapshot",
    "snapshot"
  },
  { NULL,          -1,              NOT_PUBLIC,      AUTH_UNNEEDED, NULL,
    NULL,
    NULL
  }
};

static const struct {
  const char  *name;
  int          status;
} status_struct[] = {
  { "XINE_STATUS_IDLE"   , XINE_STATUS_IDLE },
  { "XINE_STATUS_PLAY"   , XINE_STATUS_PLAY },
  { "XINE_STATUS_STOP"   , XINE_STATUS_STOP },
  { "XINE_STATUS_QUIT"   , XINE_STATUS_QUIT },
  { NULL                 , -1               }
};

static const struct {
  const char  *name;
  int          speed;
} speeds_struct[] = {
  { "XINE_SPEED_PAUSE"   , XINE_SPEED_PAUSE  },
  { "XINE_SPEED_SLOW_4"  , XINE_SPEED_SLOW_4 },
  { "XINE_SPEED_SLOW_2"  , XINE_SPEED_SLOW_2 },
  { "XINE_SPEED_NORMAL"  , XINE_SPEED_NORMAL },
  { "XINE_SPEED_FAST_2"  , XINE_SPEED_FAST_2 },
  { "XINE_SPEED_FAST_4"  , XINE_SPEED_FAST_4 },
  { NULL                 , 0                 }
};


#endif

static int sock_create(const char *service, const char *transport, struct sockaddr_in *sin) {
  struct servent    *iservice;
  struct protoent *itransport;
  int                sock;
  int                type;
  int                proto = 0;

  memset(sin, 0, sizeof(*sin));

  sin->sin_family = AF_INET;
  sin->sin_port   = htons(atoi(service));

  if(!sin->sin_port) {
    iservice = getservbyname(service, "tcp");

    if(!iservice)
      fprintf (stderr, "Service not registered: %s\n", service);
    else
      sin->sin_port = iservice->s_port;
  }

  itransport = getprotobyname(transport);

  if(!itransport)
    fprintf (stderr, "Protocol not registered: %s\n", transport);
  else
    proto = itransport->p_proto;

  if(!strcmp(transport, "udp"))
    type = SOCK_DGRAM;
  else
    type = SOCK_STREAM;

  sock = socket(AF_INET, type, proto);

  if(sock < 0) {
    fprintf (stderr, "Cannot create socket: %s\n", strerror(errno));
    return -1;
  }

  if (fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
    fprintf (stderr, "** socket cannot be made uninheritable (%s)\n", strerror(errno));
  }

  return sock;
}

/*
 * Check for socket validity.
 */
static int sock_check_opened(int socket) {
  fd_set   readfds, writefds, exceptfds;
  int      retval;
  struct   timeval timeout;

  for(;;) {
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    FD_SET(socket, &exceptfds);

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    retval = select(socket + 1, &readfds, &writefds, &exceptfds, &timeout);

    if(retval == -1 && (errno != EAGAIN && errno != EINTR))
      return 0;

    if (retval != -1)
      return 1;
  }

  return 0;
}

/*
 * Write to socket.
 */
static int _sock_write(int socket, const char *buf, int len) {
  ssize_t  size;
  int      wlen = 0;

  if (socket < 0)
    return -1;

  if(!sock_check_opened(socket))
    return -1;

  while(len) {
    size = write(socket, buf, len);

    if(size <= 0)
      return -1;

    len -= size;
    wlen += size;
    buf += size;
  }

  return wlen;
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

#ifdef NETWORK_CLIENT
static int __attribute__ ((format (printf, 3, 4))) __sock_write(int socket, int cr, const char *msg, ...) {
  char     buf[_BUFSIZ];
  size_t   s;
  va_list  args;

  va_start(args, msg);
  s = vsnprintf (buf, _BUFSIZ - 1, msg, args);
  va_end(args);

  /* Each line sent is '\n' terminated */
  if(cr) {
    if (s && (buf [s - 1] != '\n')) {
      buf[s++] = '\n';
      buf[s] = 0;
    }
  }

  return _sock_write (socket, buf, s);
}

static int sock_client(const char *host, const char *service, const char *transport) {
  union {
    struct sockaddr_in in;
    struct sockaddr sa;
  } fsin;
  struct hostent      *ihost;
  int                  sock;

  sock = sock_create(service, transport, &fsin.in);
  if (sock < 0) {
    return -1;
  }

  if ((fsin.in.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
    ihost = gethostbyname(host);

    if(!ihost) {
      fprintf (stderr, "Unknown host: %s\n", host);
      return -1;
    }
    memcpy(&fsin.in.sin_addr, ihost->h_addr_list[0], ihost->h_length);
  }

  if(connect(sock, &fsin.sa, sizeof(fsin.in)) < 0) {
    int err = errno;

    close(sock);
    errno = err;
    fprintf (stderr, "Unable to connect %s[%s]: %s\n", host, service, strerror(errno));
    return -1;
  }

  return sock;
}

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 91)
#define write_to_console_unlocked(session, msg, args...)  __sock_write(session->console, 1, msg, ##args)
#define write_to_console_unlocked_nocr(session, msg, args...) __sock_write(session->console, 0, msg, ##args)
#else
#define write_to_console_unlocked(session, ...)  __sock_write(session->console, 1, __VA_ARGS__)
#define write_to_console_unlocked_nocr(session, ...) __sock_write(session->console, 0, __VA_ARGS__)
#endif

static int __attribute__ ((format (printf, 2, 3))) write_to_console(session_t *session, const char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  int      err;

  va_start(args, msg);
  vsnprintf(buf, _BUFSIZ, msg, args);
  va_end(args);

  pthread_mutex_lock(&session->console_mutex);
  err = write_to_console_unlocked(session, "%s", buf);
  pthread_mutex_unlock(&session->console_mutex);

  return err;
}

#if 0
static int __attribute__ ((format (printf, 2, 3)) write_to_console_nocr(session_t *session, const char *msg, ...) {
  char     buf[_BUFSIZ];
  va_list  args;
  int      err;

  va_start(args, msg);
  vsnprintf(buf, _BUFSIZ, msg, args);
  va_end(args);

  pthread_mutex_lock(&session->console_mutex);
  err = write_to_console_unlocked_nocr(session, buf);
  pthread_mutex_unlock(&session->console_mutex);

  return err;
}
#endif

static void session_update_prompt(session_t *session) {
  if(session == NULL)
    return;

  if(session->socket >= 0)
    snprintf(session->prompt, sizeof(session->prompt), "[%s:%s]"PROGNAME" >", session->host, session->port);
  else
    strlcpy(session->prompt, "[******:****]"PROGNAME" >", sizeof(session->prompt));
}

static void session_create_commands(session_t *session) {
  static const size_t num_commands = (sizeof(client_commands) / sizeof(client_commands[0]));
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

  session_commands = (session_commands_t **) calloc(num_commands, sizeof(session_commands_t *));
  for(i = 0; client_commands[i].command != NULL; i++) {
    session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
    session_commands[i]->command   = strdup(client_commands[i].command);
    session_commands[i]->origin    = client_commands[i].origin;
    session_commands[i]->enable    = client_commands[i].enable;
    session_commands[i]->function  = client_commands[i].function;
  }
  session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
  session_commands[i]->command   = NULL;
  session_commands[i]->origin    = ORIGIN_CLIENT;
  session_commands[i]->enable    = 0;
  session_commands[i]->function  = NULL;

}

/*
 * Client commands
 */
static void client_noop(session_t *session, session_commands_t *command, const char *cmd) {
  (void)session;
  (void)command;
  (void)cmd;
}

static size_t _list_sess_cmds (char *buf, size_t bsize, int line_width) {
  char *p = buf, *s = buf, *e = buf + bsize - 1;
  int cwidth = 0, cnum, col = 0, i;

  for (i = 0; session_commands[i]->command; i++) {
    if (session_commands[i]->enable) {
      size_t l = strlen (session_commands[i]->command);

      if ((int)l > cwidth)
        cwidth = l;
    }
  }
  cwidth += 1;
  cnum = (line_width - 8) / cwidth;
  if (cnum <= 0)
    cnum = 1;

  for (i = 0; session_commands[i]->command; i++) {
    if (session_commands[i]->enable) {
      size_t l = strlen (session_commands[i]->command);
  
      if (col == 0) {
        if (p + 8 > e)
          break;
        memset (p, ' ', 8); p += 8;
      }
      if (p + cwidth > e)
        break;
      memcpy (p, session_commands[i]->command, l); p += l;
      s = p;
      if (++col < cnum) {
        memset (p, ' ', cwidth - l); p += cwidth - l;
      } else {
        if (p >= e)
          break;
        *p++ = '\n';
        col = 0;
      }
    }
  }
  p = s;
  if (p < e)
    *p++ = '\n';
  *p = 0;
  return p - buf;
}

static void client_help(session_t *session, session_commands_t *command, const char *cmd) {
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf);

  (void)cmd;
  if((session == NULL) || (command == NULL))
    return;

  p += strlcpy (p, "Available commands are:\n", e - p);
  /* if (p > e) p = e; */
  _list_sess_cmds (p, e - p, 80);
  write_to_console(session, "%s\n", buf);
}

static void client_version(session_t *session, session_commands_t *command, const char *cmd) {

  (void)cmd;
  if((session == NULL) || (command == NULL))
    return;

  write_to_console(session, "%s version %s\n\n", PROGNAME, PROGVERSION);
}

static void client_close(session_t *session, session_commands_t *command, const char *cmd) {

  (void)cmd;
  if((session == NULL) || (command == NULL))
    return;

  if(session->socket >= 0) {
    int i = 0;

    _sock_write (session->socket, "exit\n", 5);
    close(session->socket);
    session->socket = -1;
    session_update_prompt(session);

    while(session_commands[i]->command != NULL) {
      if(session_commands[i]->origin == ORIGIN_SERVER)
	session_commands[i]->enable = 0;
      i++;
    }
  }
}
static void client_open(session_t *session, session_commands_t *command, const char *cmd) {
  char  buf[_BUFSIZ];
  char  *pbuf, *p;
  char  *host = NULL, *port = NULL;

  if((session == NULL) || (command == NULL))
    return;

  if(session->socket >= 0) {
    write_to_console(session, "Already connected to '%s:%s'.", session->host, session->port);
    return;
  }

  if(cmd) {
    strlcpy(buf, cmd, sizeof(buf));
    pbuf = buf;
    if((p = strchr(pbuf, ' ')) != NULL) {
      host = _atoa(p);
      if((port = strrchr(p, ':')) != NULL) {
	if(strlen(port) > 1) {
	  *port = '\0';
	  port++;
	}
	else {
	  *port = '\0';
	  port = NULL;
	}
      }
    }

    if(host != NULL) {

      strlcpy(session->host, host, sizeof(session->host));

      if(port != NULL)
	strlcpy(session->port, port, sizeof(session->port));

      session_create_commands(session);
      session->socket = sock_client(session->host, session->port, "tcp");
      if(session->socket < 0) {
	write_to_console(session, "opening server '%s' failed: %s.\nExiting.\n",
			 session->host, strerror(errno));

	session->running = 0;
      }
       _sock_write (session->socket, "commands\n", 9);
      session_update_prompt(session);
    }
    else {
      write_to_console(session, "open require arguments (host or host:port)\n");
    }
  }

}
static void client_quit(session_t *session, session_commands_t *command, const char *cmd) {

  client_close(session, command, cmd);
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
      if(session_commands[index - 1]->enable) {
	if(strncasecmp(cmd, text, len) == 0) {
	  retcmd = strdup(cmd);
	  return retcmd;
	}
      }
    }
  }

  return NULL;
}
static char **completion_function(const char *text, int start, int end) {
  char  **cmd = NULL;

  (void)end;
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
        _sock_write (session.socket, "exit\n", 5);
	close(session.socket);
      }
      exit(1);
    }
    break;

  }
}

static __attribute__((noreturn)) void *select_thread(void *data) {
  session_t       *session = (session_t *) data;
  fd_set           readfds;
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
	FD_ZERO(&readfds);
	FD_SET(session->socket, &readfds);
	timeout.tv_sec  = 0;
	timeout.tv_usec = 200000;

	select(session->socket + 1, &readfds, (fd_set *) 0, (fd_set *) 0, &timeout);
	was_down = 0;
      }

      if(FD_ISSET(session->socket, &readfds)) {

	size = recvfrom(session->socket, buffer, sizeof(buffer), MSG_NOSIGNAL, NULL, NULL);

	if(size > 0) {

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
		  size_t i = (sizeof(client_commands) / sizeof(client_commands[0])) - 1;

		  pp = special + 11;
		  while((p = xine_strsep(&pp, "\t")) != NULL) {
		    if(strlen(p)) {
		      while(*p == ' ' || *p == '\t') p++;
		      session_commands = (session_commands_t **) realloc(session_commands, (i + 2) * (sizeof(session_commands_t *)));
		      session_commands[i] = (session_commands_t *) malloc(sizeof(session_commands_t));
		      session_commands[i]->command  = strdup(p);
		      session_commands[i]->origin   = ORIGIN_SERVER;
		      session_commands[i]->enable   = 1;
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
		  session_commands[i]->enable   = 1;
		  session_commands[i]->function = NULL;

		  ocount -= special_length;
		  obuffer[ocount] = '\0';

		}
		pthread_mutex_lock(&session->console_mutex);
		/* Erase chars til's col0 */
		while(pos) {

		  write_to_console_unlocked_nocr(session, "\b \b");
		  pos--;
		}
		write_to_console_unlocked(session, "%s", obuffer);

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
  strlcpy(cmd, command, sizeof(cmd));
  if((p = strchr(cmd, ' ')) != NULL)
    *p = '\0';

  i = found = 0;

  /* looking for command matching */
  while((session_commands[i]->command != NULL) && (found == 0)) {
    if(session_commands[i]->enable) {
      if(!strncasecmp(cmd, session_commands[i]->command, strlen(cmd))) {
	found++;

	if(session_commands[i]->origin == ORIGIN_CLIENT) {
	  if(session_commands[i]->function)
	    session_commands[i]->function(session, session_commands[i], command);
	}
	else {
	  char *p, *pp;
	  char buf[_BUFSIZ];

	  /*
	   * Duplicate single '%' char, vsnprintf() seems
	   * confused with single one in *format string
	   */
	  p = (char *)command;
	  pp = buf;
	  while(*p != '\0') {

	    switch(*p) {
	    case '%':
	      if(*(p + 1) != '%') {
		*pp++ = '%';
		*pp++ = '%';
	      }
	      break;

	    default:
	      *pp = *p;
	      pp++;
	      break;
	    }

	    p++;
	  }

	  *pp = '\0';

            if ((_sock_write (session->socket, buf, pp - buf)) == -1) {
	    session->running = 0;
	  }
	}

      }
    }
    i++;
  }

  /* Perhaps a ';' separated commands, so send anyway to server */
  if(found == 0) {
    char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;
    size_t l = strlen (command);
    if (p + l > e)
      l = e - p;
    memcpy (p, command, l); p += l;
    memcpy (p, "\n", 2); p += 1;
    _sock_write (session->socket, buf, p - buf);
  }

  if((!strncasecmp(cmd, "exit", strlen(cmd))) || (!strncasecmp(cmd, "halt", strlen(cmd)))) {
    session_create_commands(session);
    session->socket = -1;
  }

}

static void session_single_shot(session_t *session, int num_commands, char *commands[]) {
  int i;
  char buf[_BUFSIZ];

  buf[0] = 0;

  for(i = 0; i < num_commands; i++) {
    if(buf[0])
      sprintf(buf+strlen(buf), " %s", commands[i]);
    else
      strcpy(buf, commands[i]);
  }

  client_handle_command(session, buf);
  usleep(10000);
  _sock_write (session->socket, "exit\n", 5);
}

static void show_version(void) {
  printf("This is %s - xine's remote control v%s.\n"
	 "(c) 2000-2019 The xine Team.\n", PROGNAME, PROGVERSION);
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
  strcpy(session.host, DEFAULT_SERVER);
  strcpy(session.port, DEFAULT_XINECTL_PORT);

  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != EOF) {
    switch(c) {

    case 'H': /* Set host */
      if(optarg != NULL)
	strlcpy(session.host, optarg, sizeof(session.host));
      break;

    case 'P': /* Set port */
      if(optarg != NULL) {
	port_set = 1;
	strlcpy(session.port, optarg, sizeof(session.port));
      }
      break;

    case 'c': /* Execute argv[] command, then leave */
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
  rl_attempted_completion_function = completion_function;

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
    _sock_write (session.socket, "commands\n", 9);
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
      if (errno == 0 || errno == ENOTTY)
        exit(0);

      fprintf(stderr, "%s(%d): readline() failed: %s\n",
	      __XINE_FUNCTION__, __LINE__, strerror(errno));
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
    SAFE_FREE(grabbed_line);
  }

 __end:

  if(session.socket >= 0)
    close(session.socket);

  pthread_join(session.thread, &p);
  pthread_mutex_destroy(&session.console_mutex);

  return 0;
}

#else

static int sock_serv(const char *service, const char *transport, int queue_length) {
  union {
    struct sockaddr_in in;
    struct sockaddr sa;
  } fsin;
  int                 sock;
  int                 on = 1;

  sock = sock_create(service, transport, &fsin.in);
  if (sock < 0) {
    return -1;
  }

  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

  if(bind(sock, &fsin.sa, sizeof(fsin.in)) < 0)
    fprintf (stderr, "Unable to link socket %s: %s\n", service, strerror(errno));

  if(strcmp(transport, "udp") && listen(sock, queue_length) < 0)
    fprintf (stderr, "Passive mode impossible on %s: %s\n", service, strerror(errno));

  return sock;
}

/*
 * Read from socket.
 */
static ssize_t sock_read(int socket, char *buf, size_t len) {
  char    *pbuf;
  ssize_t  r, rr;
  void    *nl;

  if((socket < 0) || (buf == NULL) || (len < 1))
    return -1;

  if(!sock_check_opened(socket))
    return -1;

  if (--len < 1)
    return(-1);

  pbuf = buf;

  do {

    if((r = recv(socket, pbuf, len, MSG_PEEK)) <= 0)
      return -1;

    if((nl = memchr(pbuf, '\n', r)) != NULL)
      r = ((char *) nl) - pbuf + 1;

    if((rr = read(socket, pbuf, r)) < 0)
      return -1;

    pbuf += rr;
    len -= rr;

  } while((nl == NULL) && len);

  *pbuf = '\0';

  return (pbuf - buf);
}

/*
 * Password related
 */
static void _passwdfile_get_next_line(fileobj_t *fobj) {
  char *p;

 __get_next_line:

  fobj->ln = fgets(fobj->buf, sizeof(fobj->buf), fobj->fd);

  while(fobj->ln && (*fobj->ln == ' ' || *fobj->ln == '\t')) ++fobj->ln;

  if(fobj->ln) {
    if((strncmp(fobj->ln, ";", 1) == 0) ||
       (strncmp(fobj->ln, "#", 1) == 0) ||
       (*fobj->ln == '\0')) {
      goto __get_next_line;
    }

  }

  p = fobj->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      p++;
    }

    while(p > fobj->ln) {
      --p;

      if(*p == ' ' || *p == '\t')
	*p = '\0';
      else
	break;
    }
  }

}
static int _passwdfile_is_entry_valid(char *entry, char *name, char *passwd) {
  char buf[_BUFSIZ];
  char *n, *p;

  strlcpy(buf, entry, sizeof(buf));
  n = buf;
  if((p = strrchr(buf, ':')) != NULL) {
    if(strlen(p) > 1) {
      *p = '\0';
      p++;
    }
    else {
      *p = '\0';
      p = NULL;
    }
  }

  if((n != NULL) && (p != NULL)) {
    strlcpy(name, n, MAX_NAME_LEN);
    strlcpy(passwd, p, MAX_PASSWD_LEN);
    return 1;
  }

  return 0;
}
static int server_load_passwd(const char *passwdfilename) {
  fileobj_t   fobj;
  int         entries = 0;
  char        name[MAX_NAME_LEN];
  char        passwd[MAX_PASSWD_LEN];


  if((fobj.fd = fopen(passwdfilename, "r")) != NULL) {
    passwds = (passwds_t **) malloc(sizeof(passwds_t *));

    _passwdfile_get_next_line(&fobj);

    while(fobj.ln != NULL) {

      if(_passwdfile_is_entry_valid(fobj.buf, name, passwd)) {
	passwds = (passwds_t **) realloc(passwds, sizeof(passwds_t *) * (entries + 2));
	passwds[entries]         = (passwds_t *) malloc(sizeof(passwds_t));
	passwds[entries]->ident  = strdup(name);
	passwds[entries]->passwd = strdup(passwd);
	entries++;
      }

      _passwdfile_get_next_line(&fobj);
    }

    passwds[entries]         = (passwds_t *) malloc(sizeof(passwds_t));
    passwds[entries]->ident  = NULL;
    passwds[entries]->passwd = NULL;

    fclose(fobj.fd);
  }
  else {
    fprintf(stderr, "fopen() failed: %s\n", strerror(errno));
    return 0;
  }

  if(!entries)
    return 0;

  return 1;
}
static int is_user_in_passwds(client_info_t *client_info) {
  int i;

  if(client_info == NULL)
    return 0;

  for(i = 0; passwds[i]->ident != NULL; i++) {
    if(!strcasecmp(passwds[i]->ident, client_info->name))
      return 1;
  }
  return 0;
}
static int is_user_allowed(client_info_t *client_info) {
  int i;

  if (client_info == NULL)
    return PASSWD_USER_DENIED;

  for(i = 0; passwds[i]->ident != NULL; i++) {
    if(!strcmp(passwds[i]->ident, client_info->name)) {
      if(!strncmp(passwds[i]->passwd, "*", 1))
	return PASSWD_USER_DENIED;
      else if(!strcmp(passwds[i]->passwd, client_info->passwd))
	return PASSWD_USER_ALLOWED;
    }
  }
  return PASSWD_USER_DENIED;
}
static int is_client_authorized(client_info_t *client_info) {
  int i;
  int all, allowed;

  all = allowed = 0;

  /* First, find global permissions (ALL/ALLOW|DENY) */
  for(i = 0; passwds[i]->ident != NULL; i++) {
    if(!strcmp(passwds[i]->ident, "ALL")) {

      all = 1;

      if(!strcmp(passwds[i]->passwd, "ALLOW"))
	allowed = PASSWD_ALL_ALLOWED;
      else if(!strcmp(passwds[i]->passwd, "DENY"))
	allowed = PASSWD_ALL_DENIED;
      else
	all = allowed = 0;

    }
  }

  if(all) {
    if(allowed == PASSWD_ALL_ALLOWED)
      return 1;
    /*
     * If user in password list, ask him for passwd,
     * otherwise don't let him enter.
     */
    else if(allowed == PASSWD_ALL_DENIED) {

      if(!is_user_in_passwds(client_info))
	return 0;
      else
	return 1;

    }
  }

  /*
   * No valid ALL rule found, assume everybody are denied and
   * check if used is in password file.
   */
  return (is_user_in_passwds(client_info));
}

/*
 * Check access rights.
 */
static void check_client_auth(client_info_t *client_info) {
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;

  client_info->authentified = 0;

  if(is_client_authorized(client_info)) {
    if((is_user_allowed(client_info)) == PASSWD_USER_ALLOWED) {
      client_info->authentified = 1;
      p += snprintf (p, e - p, "user '%s' has been authentified.\n", client_info->name);
      return;
    }
  }
  if (p == buf)
    p += snprintf (p, e - p, "user '%s' isn't known/authorized.\n", client_info->name);
  _sock_write (client_info->socket, buf, p - buf);
}

static void handle_xine_error(gGui_t *gui, client_info_t *client_info) {
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;
  int err;

  err = xine_get_error(gui->stream);

  switch(err) {

  case XINE_ERROR_NONE:
    /* noop */
    break;

  case XINE_ERROR_NO_INPUT_PLUGIN:
    gui_playlist_lock (gui);
    p += snprintf (p, e - p,
      "xine engine error:\n"
      "There is no available input plugin available to handle '%s'.\n\n", gui->mmk.mrl);
    gui_playlist_unlock (gui);
    break;

  case XINE_ERROR_NO_DEMUX_PLUGIN:
    gui_playlist_lock (gui);
    p += snprintf (p, e - p,
      "xine engine error:\n"
      "There is no available demuxer plugin to handle '%s'.\n\n", gui->mmk.mrl);
    gui_playlist_unlock (gui);
    break;

  default:
    memcpy (p, "xine engine error:\n!! Unhandled error !!\n\n", 43); p += 42;
    break;
  }
  _sock_write (client_info->socket, buf, p - buf);
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
 * return argument <num>, NULL if there is not argument.
 */
static const char *get_arg(client_info_t *client_info, int num) {

  if((client_info == NULL) || (num < 1) || (client_info->command.num_args < num))
    return NULL;

  return(client_info->command.args[num - 1]);
}

/*
 * Set current command line from line.
 */
static void set_command_line(client_info_t *client_info, const char *line) {

  if(client_info && line && strlen(line)) {

    client_info->command.line = (char *) realloc(client_info->command.line, strlen(line) + 1);

    strcpy(client_info->command.line, line);

  }
}

/*
 * Display help of given command.
 */
static void command_help(const commands_t *command, client_info_t *client_info) {
  if(command) {
    char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;

    if (command->help)
      p += snprintf (p, e - p, "Help of '%s' command:\n       %s\n\n", command->command, command->help);
    else
      p += snprintf (p, e - p, "There is no help text for command '%s'\n\n", command->command);
    _sock_write (client_info->socket, buf, p - buf);
  }
}

/*
 * Display syntax of given command.
 */
static void command_syntax(const commands_t *command, client_info_t *client_info) {
  if(command) {
    char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;

    if (command->syntax)
      p += snprintf (p, e - p, "Syntax of '%s' command:\n%s\n", command->command, command->syntax);
    else
      p += snprintf (p, e - p, "There is no syntax definition for command '%s'\n\n", command->command);
    _sock_write (client_info->socket, buf, p - buf);
  }
}

static void do_commands(const commands_t *cmd, client_info_t *client_info) {
  int i = 0;
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;

  (void)cmd;
  p += strlcpy (p, COMMANDS_PREFIX, e - p);
  /* if (p > e) p = e; */

  while (commands[i].command) {
    if (commands[i].public) {
      if (p + 2 > e)
        break;
      *p++ = '\t';
      p += strlcpy (p, commands[i].command, e - p);
      if (p > e) {
        p = e;
        break;
      }
    }
    i++;
  }
  memcpy (p, ".\n\n", 4); p += 3;
  _sock_write (client_info->socket, buf, p - buf);
}

static size_t _list_cmds (char *buf, size_t bsize, int line_width) {
  char *p = buf, *s = buf, *e = buf + bsize - 1;
  int cwidth = 0, cnum, col = 0, i;

  for (i = 0; commands[i].command; i++) {
    if (commands[i].public) {
      size_t l = strlen (commands[i].command);

      if ((int)l > cwidth)
        cwidth = l;
    }
  }
  cwidth += 1;
  cnum = (line_width - 8) / cwidth;
  if (cnum <= 0)
    cnum = 1;

  for (i = 0; commands[i].command; i++) {
    if (commands[i].public) {
      size_t l = strlen (commands[i].command);
  
      if (col == 0) {
        if (p + 8 > e)
          break;
        memset (p, ' ', 8); p += 8;
      }
      if (p + cwidth > e)
        break;
      memcpy (p, commands[i].command, l); p += l;
      s = p;
      if (++col < cnum) {
        memset (p, ' ', cwidth - l); p += cwidth - l;
      } else {
        if (p >= e)
          break;
        *p++ = '\n';
        col = 0;
      }
    }
  }
  p = s;
  if (p < e)
    *p++ = '\n';
  *p = 0;
  return p - buf;
}

static void do_help(const commands_t *cmd, client_info_t *client_info) {
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf) - 4;

  (void)cmd;
  p += strlcpy (p, "Available commands are:\n", e - p);
  /* if (p > e) p = e; */

  if(!client_info->command.num_args) {
    p += _list_cmds (p, e - p, 80);
  }
  else {
    int i;

    for(i = 0; commands[i].command != NULL; i++) {
      if(!strcasecmp((get_arg(client_info, 1)), commands[i].command)) {
	command_help(&commands[i], client_info);
	return;
      }
    }

    p += snprintf (p, e - p, "Unknown command '%s'.\n", (get_arg (client_info, 1)));
  }
  _sock_write (client_info->socket, buf, p - buf);
}

static void do_syntax(const commands_t *command, client_info_t *client_info) {
  char buf[_BUFSIZ], *p = buf, *e = buf + sizeof (buf);
  int i;

  (void)command;
  for(i = 0; commands[i].command != NULL; i++) {
    if(!strcasecmp((get_arg(client_info, 1)), commands[i].command)) {
      command_syntax(&commands[i], client_info);
      return;
    }
  }

  p += snprintf (p, e - p, "Unknown command '%s'.\n\n", (get_arg (client_info, 1)));
  _sock_write (client_info->socket, buf, p - buf);
}

static void do_auth(const commands_t *cmd, client_info_t *client_info) {
  int nargs;

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs >= 1) {
      char buf[_BUFSIZ];
      char *name;
      char *passwd;

      strlcpy(buf, get_arg(client_info, 1), sizeof(buf));
      name = buf;
      if((passwd = strrchr(buf, ':')) != NULL) {
	if(strlen(passwd) > 1) {
	  *passwd = '\0';
	  passwd++;
	}
	else {
	  *passwd = '\0';
	  passwd = NULL;
	}
      }
      if((name != NULL) && (passwd != NULL)) {
	memset(&client_info->name, 0, sizeof(client_info->name));
	memset(&client_info->passwd, 0, sizeof(client_info->passwd));

	strlcpy(client_info->name, name, sizeof(client_info->name));
	strlcpy(client_info->passwd, passwd, sizeof(client_info->passwd));

	check_client_auth(client_info);
      }
      else
        _sock_write (client_info->socket, "use identity:password syntax.\n", 30);
    }
  }
}

static void do_mrl(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  size_t l1 = strlen (s1);

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      if (!strncmp (s1, "next", l1)) {
	gui->ignore_next = 1;
	xine_stop (gui->stream);
	gui->ignore_next = 0;
        gui_playlist_start_next (gui);
      }
      else if (!strncmp (s1, "prev", l1)) {
	gui->ignore_next = 1;
	xine_stop (gui->stream);
	gui->playlist.cur--;
	if ((gui->playlist.cur >= 0) && (gui->playlist.cur < gui->playlist.num)) {
          gui_current_set_index (gui, GUI_MMK_CURRENT);
          gui_playlist_lock (gui);
          (void)gui_xine_open_and_play (gui, gui->mmk.mrl, gui->mmk.sub, 0,
					gui->mmk.start, gui->mmk.av_offset, gui->mmk.spu_offset, 1);
          gui_playlist_unlock (gui);

	}
	else {
	  gui->playlist.cur = 0;
          gui_display_logo (gui);
	}
	gui->ignore_next = 0;
      }
    }
    else if(nargs >= 2) {

      if (!strncmp (s1, "add", l1)) {
	int argc = 2;

	while((get_arg(client_info, argc)) != NULL) {
          gui_dndcallback (gui, (char *)(get_arg(client_info, argc)));
	  argc++;
	}
      }
      else if (!strncmp (s1, "play", l1)) {
        gui_dndcallback (gui, (char *)(get_arg(client_info, 2)));

	if((xine_get_status(gui->stream) != XINE_STATUS_STOP)) {
	  gui->ignore_next = 1;
	  xine_stop(gui->stream);
	  gui->ignore_next = 0;
	}
        gui_playlist_lock (gui);
        gui_current_set_index (gui, gui->playlist.num - 1);
	if(!(xine_open(gui->stream, gui->mmk.mrl)
	     && xine_play (gui->stream, 0, gui->mmk.start))) {
          handle_xine_error(gui, client_info);
          gui_display_logo (gui);
	}
	else
	  gui->logo_mode = 0;
        gui_playlist_unlock (gui);
      }
    }
  }
}

static void do_playlist(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  const char *s2 = client_info->command.num_args >= 2 ? client_info->command.args[1] : "\x01";
  size_t l1 = strlen (s1), l2 = strlen (s2);

  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      int first = 0;

      if (!strncmp (s1, "show", l1)) {
	int i;

	if(gui->playlist.num) {
          char buf[_BUFSIZ], *p, *e = buf + sizeof (buf) - 4;
          gui_playlist_lock (gui);
	  for(i = 0; i < gui->playlist.num; i++) {
            p = buf;
            p += snprintf (p, e - p, "%2s %5d %s\n\n", (i == gui->playlist.cur) ? "*>" : "", i, gui->playlist.mmk[i]->mrl);
            _sock_write (client_info->socket, buf, p - buf);
	  }
          gui_playlist_unlock (gui);
	}
	else
          _sock_write (client_info->socket, "empty playlist.\n", 16);

        _sock_write (client_info->socket, "\n", 1);
      }
      else if (!strncmp (s1, "next", l1)) { /* Alias of mrl next */
	set_command_line(client_info, "mrl next");
	handle_client_command(client_info);
      }
      else if (!strncmp (s1, "prev", l1)) { /* Alias of mrl prev */
	set_command_line(client_info, "mrl prev");
	handle_client_command(client_info);
      }
      else if ((first = !strncmp (s1, "first", l1)) || !strncmp (s1, "last", l1)) {

	if(gui->playlist.num) {
	  int entry = (first) ? 0 : gui->playlist.num - 1;

	  if(entry != gui->playlist.cur) {

	    if(xine_get_status(gui->stream) != XINE_STATUS_STOP) {
	      gui->ignore_next = 1;
	      xine_stop (gui->stream);
	      gui->ignore_next = 0;
	    }

	    gui->playlist.cur = entry;
            gui_current_set_index (gui, GUI_MMK_CURRENT);
            gui_play (NULL, gui);
	  }
	}
      }
      else if (!strncmp (s1, "stop", l1)) {
	if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
	  gui->playlist.control |= PLAYLIST_CONTROL_STOP;
      }
      else if (!strncmp (s1, "continue", l1)) {
	if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
	  gui->playlist.control &= ~PLAYLIST_CONTROL_STOP;
      }

    }
    else if(nargs >= 2) {

      if (!strncmp (s1, "select", l1)) {
	int j;

	j = atoi(get_arg(client_info, 2));

	if((j >= 0) && (j <= gui->playlist.num)) {
	  gui->playlist.cur = j;
          gui_current_set_index (gui, GUI_MMK_CURRENT);

	  if(xine_get_status(gui->stream) != XINE_STATUS_STOP) {
	    gui->ignore_next = 1;
	    xine_stop(gui->stream);
	    gui->ignore_next = 0;

	    do_play(cmd, client_info);
	  }
	}
      }
      else if (!strncmp (s1, "delete", l1)) {

	if((!strncmp (s2, "all", l2)) ||
	   (!strncmp (s2, "*", l2))) {

          gui_playlist_free (gui);

	  if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
            gui_stop (NULL, gui);

          enable_playback_controls (gui->panel, 0);
	}
	else {
	  int j;

	  j = atoi(get_arg(client_info, 2));

	  if(j >= 0) {

	    if((gui->playlist.cur == j) && ((xine_get_status(gui->stream) != XINE_STATUS_STOP)))
              gui_stop (NULL, gui);

            gui_playlist_lock (gui);
            gui_playlist_remove (gui, j);
	    gui->playlist.cur = 0;
            gui_playlist_unlock (gui);
	  }
	}

        playlist_update_playlist (gui);

	if(gui->playlist.num)
          gui_current_set_index (gui, GUI_MMK_CURRENT);
	else {

          if (is_playback_widgets_enabled (gui->panel))
            enable_playback_controls (gui->panel, 0);

	  if(xine_get_status(gui->stream) != XINE_STATUS_STOP)
            gui_stop (NULL, gui);

          gui_current_set_index (gui, GUI_MMK_NONE);
	}
      }
    }
  }
}

static void do_play(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;

  (void)cmd;
  if (xine_get_status (gui->stream) != XINE_STATUS_PLAY) {
    gui_playlist_lock (gui);
    if(!(xine_open(gui->stream, gui->mmk.mrl) && xine_play (gui->stream, 0, gui->mmk.start))) {
      handle_xine_error(gui, client_info);
      gui_display_logo (gui);
    }
    else
      gui->logo_mode = 0;
    gui_playlist_unlock (gui);
  }
  else {
    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
  }

}

static void do_stop(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;

  (void)cmd;
  gui->ignore_next = 1;
  xine_stop(gui->stream);
  gui->ignore_next = 0;
  gui_display_logo (gui);
}

static void do_pause(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;

  (void)cmd;
  if (xine_get_param (gui->stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
  else
    xine_set_param(gui->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
}

static void do_exit(const commands_t *cmd, client_info_t *client_info) {
  (void)cmd;
  if(client_info) {
    client_info->finished = 1;
  }
}

static void do_fullscreen(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  action_id_t action = ACTID_TOGGLE_FULLSCREEN;

  (void)cmd;
  gui_execute_action_id (gui, action);
}

#ifdef HAVE_XINERAMA
static void do_xinerama_fullscreen(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  action_id_t action = ACTID_TOGGLE_XINERAMA_FULLSCREEN;

  (void)cmd;
  gui_execute_action_id (gui, action);
}
#endif

static void do_get(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  const char *s2 = client_info->command.num_args >= 2 ? client_info->command.args[1] : "\x01";
  size_t l1 = strlen (s1), l2 = strlen (s2);
  char buf[64], *p = buf, *e = buf + sizeof (buf) - 4;

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      if (!strncmp (s1, "status", l1)) {
	int  status;

        memcpy (buf, "Current status: ", 16); p += 16;
	status = xine_get_status(gui->stream);

        p += strlcpy (p, (status <= XINE_STATUS_QUIT) ? status_struct[status].name : "*UNKNOWN*", e - p);
        if (p > e)
          p = e;

        memcpy (p, "\n\n", 3); p += 2;
      }
      else if (!strncmp (s1, "speed", l1)) {
	int  speed = -1;
	size_t i;

        memcpy (p, "Current speed: ", 15); p += 15;
	speed = xine_get_param(gui->stream, XINE_PARAM_SPEED);

	for(i = 0; speeds_struct[i].name != NULL; i++) {
	  if(speed == speeds_struct[i].speed)
	    break;
	}

        p += strlcpy (p, (i < ((sizeof (speeds_struct) / sizeof (speeds_struct[0])) - 1))
          ? speeds_struct[i].name : "*UNKNOWN*", p - e);
        if (p > e)
          p = e;

        memcpy (p, "\n\n", 3); p += 2;
      }
      else if (!strncmp (s1, "position", l1)) {
	int pos_stream;
	int pos_time;
	int length_time;
        xine_get_pos_length (gui->stream, &pos_stream, &pos_time, &length_time);
        p += snprintf (p, e - p, "Current position: %d\n\n", pos_time);
      }
      else if (!strncmp (s1, "length", l1)) {
	int pos_stream;
	int pos_time;
	int length_time;
        xine_get_pos_length (gui->stream, &pos_stream, &pos_time, &length_time);
        p += snprintf (p, e - p, "Current length: %d\n\n", length_time);
      }
      else if (!strncmp (s1, "loop", l1)) {
        memcpy (p, "Current loop mode is: ", 22); p += 22;

	switch(gui->playlist.loop) {
	case PLAYLIST_LOOP_NO_LOOP:
          memcpy (p, "'No Loop'", 9); p += 9;
	  break;
	case PLAYLIST_LOOP_LOOP:
          memcpy (p, "'Loop'", 6); p += 6;
	  break;
	case PLAYLIST_LOOP_REPEAT:
          memcpy (p, "'Repeat'", 8); p += 8;
	  break;
	case PLAYLIST_LOOP_SHUFFLE:
          memcpy (p, "'Shuffle'", 9); p += 9;
	  break;
	case PLAYLIST_LOOP_SHUF_PLUS:
          memcpy (p, "'Shuffle forever'", 17); p += 17;
	  break;
	default:
          memcpy (p, "'!!Unknown!!'", 13); p += 13;
	  break;
	}

        memcpy (p, ".\n\n", 4); p += 3;
      }
    }
    else if(nargs >= 2) {
      if (!strncmp (s1, "audio", l1)) {
	if (!strncmp (s2, "channel", l2)) {
          p += snprintf (p, e - p, "Current audio channel: %d\n\n",
            (xine_get_param (gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL)));
	}
	else if (!strncmp (s2, "lang", l2)) {
	  char lbuf[XINE_LANG_MAX];
          int channel = xine_get_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
          lbuf[0] = 0;
          if (!xine_get_audio_lang(gui->stream, channel, &buf[0])) {
            snprintf (lbuf, sizeof (lbuf), "%3d", channel);
          }
          p += snprintf (p, e - p, "Current audio language: %s\n\n", lbuf);
	}
	else if (!strncmp (s2, "volume", l2)) {
	  if(gui->mixer.caps & MIXER_CAP_VOL) {
            p += snprintf (p, e - p, "Current audio volume: %d\n\n", gui->mixer.volume_level);
          } else {
            memcpy (p, "Audio is disabled.\n\n", 20); p += 20;
          }
	}
	else if (!strncmp (s2, "mute", l2)) {
	  if(gui->mixer.caps & MIXER_CAP_MUTE) {
            p += snprintf (p, e - p, "Current audio mute: %d\n\n", gui->mixer.mute);
          } else {
            memcpy (p, "Audio is disabled.\n\n", 20); p += 20;
          }
	}
      }
      else if (!strncmp (s1, "spu", l1)) {
	if (!strncmp (s2, "channel", l2)) {
          p += snprintf (p, e - p, "Current spu channel: %d\n\n",
            (xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL)));
	}
	else if (!strncmp (s2, "lang", l2)) {
          char lbuf[XINE_LANG_MAX];
          int channel = xine_get_param(gui->stream, XINE_PARAM_SPU_CHANNEL);
          lbuf[0] = 0;
          if (!xine_get_spu_lang (gui->stream, channel, &buf[0])) {
            snprintf(buf, sizeof(buf), "%3d", channel);
          }
          p += snprintf (p, e - p, "Current spu language: %s\n\n", lbuf);
	}
	else if (!strncmp (s2, "offset", l2)) {
	  int offset;

	  offset = xine_get_param(gui->stream, XINE_PARAM_SPU_OFFSET);
          p += snprintf (p, e - p, "Current spu offset: %d\n\n", offset);
	}
      }
      else if (!strncmp (s1, "av", l1)) {
	if (!strncmp (s2, "offset", l2)) {
	  int offset;

	  offset = xine_get_param(gui->stream, XINE_PARAM_AV_OFFSET);
          p += snprintf (p, e - p, "Current A/V offset: %d\n\n", offset);
	}
      }
    }
  }
  if (p > buf)
    _sock_write (client_info->socket, buf, p - buf);
}

static void do_set(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  const char *s2 = client_info->command.num_args >= 2 ? client_info->command.args[1] : "\x01";
  size_t l1 = strlen (s1), l2 = strlen (s2);

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 2) {
      if (!strncmp (s1, "speed", l1)) {
	int speed;

	if((!strncmp (s2, "XINE_SPEED_PAUSE", l2)) ||
	   (!strncmp (s2, "|", l2)))
	  speed = XINE_SPEED_PAUSE;
	else if((!strncmp (s2, "XINE_SPEED_SLOW_4", l2)) ||
		(!strncmp (s2, "/4", l2)))
	  speed = XINE_SPEED_SLOW_4;
	else if((!strncmp (s2, "XINE_SPEED_SLOW_2", l2)) ||
		(!strncmp (s2, "/2", l2)))
	  speed = XINE_SPEED_SLOW_2;
	else if((!strncmp (s2, "XINE_SPEED_NORMAL", l2)) ||
		(!strncmp (s2, "=", l2)))
	  speed = XINE_SPEED_NORMAL;
	else if((!strncmp (s2, "XINE_SPEED_FAST_2", l2)) ||
		(!strncmp (s2, "*2", l2)))
	  speed = XINE_SPEED_FAST_2;
	else if((!strncmp (s2, "XINE_SPEED_FAST_4", l2)) ||
		(!strncmp (s2, "*4", l2)))
	  speed = XINE_SPEED_FAST_4;
	else
	  speed = atoi((get_arg(client_info, 2)));

	xine_set_param(gui->stream, XINE_PARAM_SPEED, speed);
      }
      else if (!strncmp (s1, "loop", l1)) {
	int i;
	static const struct {
	  const char *mode;
	  int         loop_mode;
	} loop_modes[] = {
	  { "no",       PLAYLIST_LOOP_NO_LOOP   },
	  { "loop",     PLAYLIST_LOOP_LOOP      },
	  { "repeat",   PLAYLIST_LOOP_REPEAT    },
	  { "shuffle",  PLAYLIST_LOOP_SHUFFLE   },
	  { "shuffle+", PLAYLIST_LOOP_SHUF_PLUS },
	  { NULL,       -1                      }
	};
	char *arg = (char *) get_arg(client_info, 2);

	for(i = 0; loop_modes[i].mode; i++) {
	  if(!strncmp(arg, loop_modes[i].mode, strlen(arg))) {
	    gui->playlist.loop = loop_modes[i].loop_mode;
	    return;
	  }
	}
      }
    }
    else if(nargs >= 3) {
      if (!strncmp (s1, "audio", l1)) {
	if (!strncmp (s2, "channel", l2)) {
	  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, (atoi(get_arg(client_info, 3))));
	}
	else if (!strncmp (s2, "volume", l2)) {
	  if(gui->mixer.caps & MIXER_CAP_VOL) {
	    int vol = atoi(get_arg(client_info, 3));

	    if(vol < 0) vol = 0;
	    if(vol > 100) vol = 100;

	    gui->mixer.volume_level = vol;
	    xine_set_param(gui->stream, XINE_PARAM_AUDIO_VOLUME, gui->mixer.volume_level);
	  }
	  else
            _sock_write (client_info->socket, "Audio is disabled.\n", 19);
	}
	else if (!strncmp (s2, "mute", l2)) {
	  if(gui->mixer.caps & MIXER_CAP_MUTE) {
	    gui->mixer.mute = get_bool_value((get_arg(client_info, 3)));
	    xine_set_param(gui->stream, XINE_PARAM_AUDIO_MUTE, gui->mixer.mute);
	  }
	  else
            _sock_write (client_info->socket, "Audio is disabled.\n", 19);
	}
      }
      else if (!strncmp (s1, "spu", l1)) {
	if (!strncmp (s2, "channel", l2)) {
	  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, (atoi(get_arg(client_info, 3))));
	}
	else if (!strncmp (s2, "offset", l2)) {
	  xine_set_param(gui->stream, XINE_PARAM_SPU_OFFSET, (atoi(get_arg(client_info, 3))));
	}
      }
      else if (!strncmp (s1, "av", l1)) {
	if (!strncmp (s2, "offset", l2)) {
	  xine_set_param(gui->stream, XINE_PARAM_AV_OFFSET, (atoi(get_arg(client_info, 3))));
	}
      }
    }
  }
}

static void do_gui(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  size_t l1 = strlen (s1);

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {
    if(nargs == 1) {
      int flushing = 0;

      if (!strncmp (s1, "hide", l1)) {
        if (panel_is_visible (gui->panel) > 1) {
          panel_toggle_visibility (NULL, gui->panel);
	  flushing++;
	}
      }
      else if (!strncmp (s1, "output", l1)) {
        gui_toggle_visibility (gui);
	flushing++;
      }
      else if (!strncmp (s1, "panel", l1)) {
	panel_toggle_visibility (NULL, gui->panel);
	flushing++;
      }
      else if (!strncmp (s1, "playlist", l1)) {
        gui_playlist_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "control", l1)) {
        gui_control_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "mrl", l1)) {
        gui_mrlbrowser_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "setup", l1)) {
        gui_setup_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "vpost", l1)) {
        gui_vpp_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "apost", l1)) {
        gui_app_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "help", l1)) {
        gui_help_show (NULL, gui);
	flushing++;
      }
      else if (!strncmp (s1, "log", l1)) {
        gui_viewlog_show (NULL, gui);
	flushing++;
      }

      /* Flush event when xine !play */
      /* Why ?
      if(flushing && ((xine_get_status(gui->stream) != XINE_STATUS_PLAY))) {
	gui->x_lock_display (gui->display);
	XSync(gui->display, False);
	gui->x_unlock_display (gui->display);
      }
      */
    }
  }
}

static void do_event(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  xine_event_t   xine_event;
  int            nargs;
  const char *s1 = client_info->command.num_args >= 1 ? client_info->command.args[0] : "\x01";
  const char *s2 = client_info->command.num_args >= 2 ? client_info->command.args[1] : "\x01";
  size_t l1 = strlen (s1), l2 = strlen (s2);

  (void)cmd;
  nargs = is_args(client_info);
  if(nargs) {

    xine_event.type = 0;

    if(nargs == 1) {
      const char *arg = get_arg(client_info, 1);
      static const struct {
	const char *name;
	int         type;
      } events_struct[] = {
	{ "menu1"   , XINE_EVENT_INPUT_MENU1    },
	{ "menu2"   , XINE_EVENT_INPUT_MENU2    },
	{ "menu3"   , XINE_EVENT_INPUT_MENU3    },
	{ "menu4"   , XINE_EVENT_INPUT_MENU4    },
	{ "menu5"   , XINE_EVENT_INPUT_MENU5    },
	{ "menu6"   , XINE_EVENT_INPUT_MENU6    },
	{ "menu7"   , XINE_EVENT_INPUT_MENU7    },
	{ "up"      , XINE_EVENT_INPUT_UP       },
	{ "down"    , XINE_EVENT_INPUT_DOWN     },
	{ "left"    , XINE_EVENT_INPUT_LEFT     },
	{ "right"   , XINE_EVENT_INPUT_RIGHT    },
	{ "select"  , XINE_EVENT_INPUT_SELECT   },
	{ "next"    , XINE_EVENT_INPUT_NEXT     },
	{ "previous", XINE_EVENT_INPUT_PREVIOUS },
	{ NULL      , -1                        }
      };
      int i = -1;

      if(strlen(arg)) {
	i = 0;
	while(events_struct[i].name && strncmp(arg, events_struct[i].name, strlen(arg))) i++;
      }

      if((i >= 0) && ((size_t)i < ((sizeof(events_struct) / sizeof(events_struct[0])) - 1))) {
	xine_event.type = events_struct[i].type;
      }
      else {
	size_t value;
	int   input_numbers[] = {
	  XINE_EVENT_INPUT_NUMBER_0, XINE_EVENT_INPUT_NUMBER_1, XINE_EVENT_INPUT_NUMBER_2,
	  XINE_EVENT_INPUT_NUMBER_3, XINE_EVENT_INPUT_NUMBER_4, XINE_EVENT_INPUT_NUMBER_5,
	  XINE_EVENT_INPUT_NUMBER_6, XINE_EVENT_INPUT_NUMBER_7, XINE_EVENT_INPUT_NUMBER_8,
	  XINE_EVENT_INPUT_NUMBER_9, XINE_EVENT_INPUT_NUMBER_10_ADD
	};


	if(((sscanf(arg, "%zu", &value)) == 1) ||
	   ((sscanf(arg, "+%zu", &value)) == 1)) {

	  if(value < (sizeof(input_numbers) / sizeof(input_numbers[0])))
	    xine_event.type = input_numbers[value];
	}
      }
    }
    else if(nargs >= 2) {
      if (!strncmp (s1, "angle", l1)) {
	if (!strncmp (s2, "next", l2)) {
	  xine_event.type = XINE_EVENT_INPUT_ANGLE_NEXT;
	}
	else if (!strncmp (s2, "previous", l2)) {
	  xine_event.type = XINE_EVENT_INPUT_ANGLE_PREVIOUS;
	}
      }
    }

    if(xine_event.type != 0) {
      xine_event.data_length = 0;
      xine_event.data        = NULL;
      xine_event.stream      = gui->stream;
      gettimeofday(&xine_event.tv, NULL);

      xine_event_send(gui->stream, &xine_event);
    }

  }
}

static void do_seek(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;
  int            nargs;

  (void)cmd;
  if((xine_get_stream_info(gui->stream, XINE_STREAM_INFO_SEEKABLE)) == 0)
    return;

  nargs = is_args(client_info);
  if(nargs) {

    if(nargs == 1) {
      int      pos;
      char    *arg = (char *)get_arg(client_info, 1);

      if(sscanf(arg, "%%%d", &pos) == 1) {

	if(pos > 100) pos = 100;
	if(pos < 0) pos = 0;

	gui->ignore_next = 1;
	if(!xine_play(gui->stream, ((int) (655.35 * pos)), 0)) {
          gui_handle_xine_error (gui, gui->stream, NULL);
          gui_display_logo (gui);
	}
	else
	  gui->logo_mode = 0;

	gui->ignore_next = 0;

      }

      else if((((arg[0] == '+') || (arg[0] == '-')) && (isdigit(arg[1])))
	      || (isdigit(arg[0]))) {
	int msec;

	pos = atoi(arg);

	if(((arg[0] == '+') || (arg[0] == '-')) && (isdigit(arg[1]))) {

          if (gui_xine_get_pos_length (gui, gui->stream, NULL, &msec, NULL)) {
	    msec /= 1000;

	    if((msec + pos) < 0)
	      msec = 0;
	    else
	      msec += pos;
	  }
	  else
	    msec = pos;

	  msec *= 1000;

	  gui->ignore_next = 1;
	  if(!xine_play(gui->stream, 0, msec)) {
            gui_handle_xine_error (gui, gui->stream, NULL);
            gui_display_logo (gui);
	  }
	  else
	    gui->logo_mode = 0;

	  gui->ignore_next = 0;
	}
      }
    }
  }
}

static void do_halt(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;

  (void)cmd;
  gui_exit (NULL, gui);
}

static void network_messenger(void *data, char *message) {
  if (message) {
    char buf[_BUFSIZ];
    size_t l = strlen (message);
    int socket = (int)(intptr_t) data;

    if (l > sizeof (buf) - 2)
      l = sizeof (buf) - 2;
    memcpy (buf, message, l);
    memcpy (buf + l, "\n", 2);
    _sock_write (socket, buf, l + 1);
  }
}

static void do_snap(const commands_t *cmd, client_info_t *client_info) {
  gGui_t *gui = client_info->gui;

  (void)cmd;
  gui_playlist_lock (gui);
  create_snapshot (gui, gui->mmk.mrl,
		  network_messenger, network_messenger, (void *)(intptr_t)client_info->socket);
  gui_playlist_unlock (gui);
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
  size_t l;
  struct hostent *hp = NULL;

  if(!gethostname(myfqdn, 255) && (hp = gethostbyname(myfqdn)) != NULL) {
    l = snprintf (buf, sizeof (buf), "%s %s %s %s\n", hp->h_name, PACKAGE, VERSION, "remote server. Nice to meet you.");
  }
  else {
    l = snprintf(buf, sizeof(buf), "%s %s %s\n", PACKAGE, VERSION, "remote server. Nice to meet you.");
  }
  _sock_write (client_info->socket, buf, l);

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
    SAFE_FREE(client_info->command.line);
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
        strlcpy(remaining, (_atoa(p)), sizeof(remaining));

      client_info->command.line = (char *) realloc(client_info->command.line, sizeof(char) * (strlen(c) + 1));

      strcpy(client_info->command.line, c);

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
							 sizeof(char) * (strlen(remaining) + 1));
	  strcpy(client_info->command.remain, remaining);
	}
	else {
	  SAFE_FREE(client_info->command.remain);
	}

      }
      else {
	SAFE_FREE(client_info->command.remain);
      }

    }
    else { /* no ';' in remain, copy AS IS remain to line */
      client_info->command.line = (char *) realloc(client_info->command.line,
						   sizeof(char) * (strlen(client_info->command.remain) + 1));

      strcpy(client_info->command.line, client_info->command.remain);

      SAFE_FREE(client_info->command.remain);
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
      *client_info->command.args[i] = 0;
  }

  client_info->command.num_args = 0;

  while(*cmd != '\0' && (*cmd != ' ' && *(cmd - 1) != '\\') && *cmd != '\t')
    cmd++;

  if(cmd >= (client_info->command.line + strlen(client_info->command.line)))
    cmd = NULL;

  if(cmd) {
    client_info->command.command = (char *) realloc(client_info->command.command,
						    strlen(client_info->command.line) - strlen(cmd) + 1);

    *client_info->command.command = 0;
    strlcpy(client_info->command.command, client_info->command.line,
	    (strlen(client_info->command.line) - strlen(cmd)));

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
	    strcpy(client_info->command.args[nargs], _atoa(buf));
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
    client_info->command.command = (char *) realloc(client_info->command.command, strlen(client_info->command.line) + 1);

    strcpy(client_info->command.command, client_info->command.line);
  }

#if 0
  {
    int k;
    printf("client_info->command = '%s'\n", client_info->command.command);
    if(client_info->command.num_args)
      for(k = 0; k < client_info->command.num_args; k++)
	printf("client_info->command_arg[%d] = '%s'\n", k, client_info->command.args[k]);
  }
#endif
}

/*
 * Handle user entered commands.
 */
static void handle_client_command(client_info_t *client_info) {
  char buf[_BUFSIZ], *p, *e = buf + sizeof (buf) - 4;
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
            p = buf;
            p += snprintf (p, e - p, "Command '%s' require argument(s).\n\n", commands[i].command);
            _sock_write (client_info->socket, buf, p - buf);
	    found++;
	  }
	  else if((commands[i].argtype == NO_ARGS) && (client_info->command.num_args > 0)) {
            p = buf;
            p += snprintf (p, e - p, "Command '%s' doesn't require argument.\n\n", commands[i].command);
            _sock_write (client_info->socket, buf, p - buf);
	    found++;
	  }
	  else {
	    if((commands[i].need_auth == NEED_AUTH) && (client_info->authentified == 0)) {
              p = buf;
              p += snprintf (p, e - p, "You need to be authentified to use '%s' command, use 'identify'.\n\n", commands[i].command);
              _sock_write (client_info->socket, buf, p - buf);
	      found++;
	    }
	    else {
	      commands[i].function(&commands[i], client_info);
	      found++;
	    }
	  }
	}
	i++;
      }

      if (!found) {
        p = buf;
        p += snprintf (p, e - p, "unhandled command '%s'.\n", client_info->command.command);
        _sock_write (client_info->socket, buf, p - buf);
      }
    }

    SAFE_FREE(client_info->command.line);

  } while((client_info->command.remain != NULL));
}

/*
 *
 */
static __attribute__((noreturn)) void *client_thread(void *data) {
  client_info_t     *client_info = (client_info_t *) data;
  char               buffer[_BUFSIZ];
  int                i;
  ssize_t            len;
  sigset_t           sigpipe_mask;

  pthread_detach(pthread_self());

  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, NULL) == -1)
    perror("pthread_sigmask");

  client_info->finished = 0;
  memset(&client_info->name, 0, sizeof(client_info->name));
  memset(&client_info->passwd, 0, sizeof(client_info->passwd));

  for(i = 0; i < 256; i++)
    client_info->command.args[i] = (char *) malloc(sizeof(char) * 2048);

  say_hello(client_info);

  do {
    len = sock_read(client_info->socket, buffer, sizeof(buffer));

    if(len < 0)
      client_info->finished = 1;
    else {
      if(len > 0) {

	set_command_line(client_info, _atoa(buffer));
	handle_client_command(client_info);
      }
    }

  } while(client_info->finished == 0);

  close(client_info->socket);

  for(i = 0; i < 256; i++)
    SAFE_FREE(client_info->command.args[i]);

  free(client_info);
  client_info = NULL;

  pthread_exit(NULL);
}

/*
 *
 */
static __attribute__((noreturn)) void *server_thread(void *data) {
  gGui_t *gui = (gGui_t *)data;
  char            *service;
  struct servent  *serv_ent;
  int              msock;

  if(gui->network_port) {
    service = xitk_asprintf("%u", gui->network_port);
  }
  else {
    /*  Search in /etc/services if a xinectl entry exist */
    if((serv_ent = getservbyname("xinectl", "tcp")) != NULL) {
      service = xitk_asprintf("%u", ntohs(serv_ent->s_port));
    }
    else
      service = strdup(DEFAULT_XINECTL_PORT);
  }

  /* Load passwd file */
  /* password file syntax is:
   *  - one entry per line
   *  - syntax: 'identifier:password' length limit is 16 chars each
   *  - a password beginning by an asterisk '*' lock the user.
   *  - if a line contain 'ALL:ALLOW' (case sensitive),
   *    all client are allowed to execute all commands.
   *  - if a line contain 'ALL:DENY' (case sensitive),
   *    all client aren't allowed to execute a command, except those
   *    who are specified. ex:
   *  - rule 'ALL' is optional.
   *
   *    ALL:DENY
   *    daniel:f1rmb
   *    .......
   *    All users are denied, except 'daniel' (if he give good passwd)
   *
   *
   *    daniel:f1rmb
   *    foo:*mypasswd
   *    .......
   *    All users are denied, 'daniel' is authorized (with passwd), 'foo'
   *    is locked.
   *
   */
  {
    const char  *passwdfile = ".xine/passwd";
    char         passwdfilename[(strlen((xine_get_homedir())) + strlen(passwdfile)) + 2];
    struct stat  st;

    snprintf(passwdfilename, sizeof(passwdfilename), "%s/%s", (xine_get_homedir()), passwdfile);

    if(stat(passwdfilename, &st) < 0) {
      fprintf(stderr, "ERROR: there is no password file for network access.!\n");
      goto __failed;
    }

    if(!server_load_passwd(passwdfilename)) {
      fprintf(stderr, "ERROR: unable to load network server password file.!\n");
      goto __failed;
    }

  }

  while(gui->running) {
    client_info_t       *client_info;
    socklen_t            lsin;
    pthread_t            thread_client;

    msock = sock_serv(service, "tcp", 5);
    if (msock < 0) {
      break;
    }

    client_info = (client_info_t *) calloc(1, sizeof(client_info_t));
    if (!client_info)
      break;
    client_info->gui = gui;
    lsin = sizeof(client_info->fsin.in);

    client_info->socket = accept(msock, &(client_info->fsin.sa), &lsin);
    client_info->authentified = is_client_authorized(client_info);

    close(msock);

    if(client_info->socket < 0) {

      free(client_info);

      if(errno == EINTR)
	continue;

      fprintf (stderr, "accept: %s\n", strerror(errno));
      continue;
    }

    pthread_create(&thread_client, NULL, client_thread, (void *)client_info);
  }

  { /* Freeing passwords */
    int i;

    for(i = 0; passwds[i]->ident != NULL; i++) {
      free(passwds[i]->ident);
      free(passwds[i]->passwd);
      free(passwds[i]);
    }
    free(passwds);

  }

 __failed:
  free(service);

  pthread_exit(NULL);
}

/*
 *
 */
void start_remote_server (gGui_t *gui) {
  pthread_t thread;

  if (gui && gui->network)
    pthread_create (&thread, NULL, server_thread, gui);
}

#endif

#endif  /* HAVE_READLINE */
