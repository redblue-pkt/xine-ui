/*
 * Copyright (C) 2000-2021 the xine project
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <xine/xineutils.h>

#include "utils.h"
#include "libcommon.h" /* strsep replacement */

extern char **environ;

/*
 * Execute a shell command.
 */
int xine_system(int dont_run_as_root, const char *command) {
  int pid, status;

  /*
   * Don't permit run as root
   */
  if(dont_run_as_root) {
    if(getuid() == 0)
      return -1;
  }

  if(command == NULL)
    return 1;

  pid = fork();

  if(pid == -1)
    return -1;

  if(pid == 0) {
    char *argv[4];
    argv[0] = (char *)"sh";
    argv[1] = (char *)"-c";
    argv[2] = (char *)command;
    argv[3] = NULL;
    execve("/bin/sh", argv, environ);
    exit(127);
  }

  do {
    if(waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
	return -1;
    }
    else {
      return WEXITSTATUS(status);
    }
  } while(1);

  return -1;
}

/*
 * cleanup the str string, take care about '
 */
size_t str_unquote (char *str) {
  uint8_t *b1, *b2, *e, *q;

  if (!str)
    return 0;
  b1 = (uint8_t *)str;
  /* skip leading whitespace. */
  while ((b1[0] > 0) && (b1[0] <= ' '))
    b1++;
  if (!b1[0]) {
    str[0] = 0;
    return 0;
  }
  /* cut trailing whitespace. */
  e = b1 + strlen ((char *)b1);
  while ((e[-1] > 0) && (e[-1] <= ' '))
    e--;
  e[0] = 0;
  /* are we quoted? */
  if (((b1[0] == '"') || (b1[0] == '\'')) && (b1[0] == e[-1])) {
    /* yes we are. */
    if (b1 + 1 == e) {
      /* just the quote sign, maybe intended, keep it. */
      str[0] = (char)b1[0];
      str[1] = 0;
      return 1;
    }
    /* skip leading whitespace again. */
    b2 = b1 + 1;
    while ((b2[0] > 0) && (b2[0] <= ' '))
      b2++;
    if (b2 + 1 == e) {
      /* nothing serious in it, settle with a plain "". */
      str[0] = str[1] = (char)b1[0];
      str[2] = 0;
      return 2;
    }
    /* removr thn quote. */
    b1 = b2;
    e--;
    /* cut trailing whitespace again. */
    while ((e[-1] > 0) && (e[-1] <= ' '))
      e--;
    e[0] = 0;
  }
  /* test all embedded whitespace is single plain, or make it that way. */
  q = b1;
  while (1) {
    if (q == b1) {
      /* skip real text */
      while (b1[0] > ' ')
        b1++;
      q = b1;
    } else {
      /* move veal text */
      while (b1[0] > ' ')
        *q++ = *b1++;
    }
    if (!b1[0])
      break;
    /* skip whitespace */
    while ((b1[0] > 0) && (b1[0] <= ' '))
      b1++;
    /* reduce */
    *q++ = ' ';
  }
  /* e = q; */
  q[0] = 0;
  return q - (uint8_t *)str;
}

/*
 *
 */
static int _mkdir_safe(const char *path) {
  struct stat  pstat;

  if(path == NULL)
    return 0;

  if((stat(path, &pstat)) < 0) {
    /* file or directory no exist, create it */
    if(mkdir(path, 0755) < 0) {
      fprintf(stderr, "mkdir(%s) failed: %s\n", path, strerror(errno));
      return 0;
    }
  }
  else {
    /* Check of found file is a directory file */
    if(!S_ISDIR(pstat.st_mode)) {
      fprintf(stderr, "%s is not a directory.\n", path);
      errno = EEXIST;
      return 0;
    }
  }

  return 1;
}

/*
 *
 */
int mkdir_safe(const char *path) {
  char *p, *pp;
  char  buf[_PATH_MAX + _NAME_MAX + 1];
  char  buf2[_PATH_MAX + _NAME_MAX + 1];

  if(path == NULL)
    return 0;

  memset(&buf, 0, sizeof(buf));
  memset(&buf2, 0, sizeof(buf2));

  strlcpy(buf, path, sizeof(buf));
  pp = buf;
  while((p = xine_strsep(&pp, "/")) != NULL) {
    if(p && strlen(p)) {
      sprintf(buf2+strlen(buf2), "/%s", p);
      if(!_mkdir_safe(buf2))
	return 0;
    }
  }

  return 1;
}

int get_bool_value(const char *val) {
  static const struct {
    char str[7];
    char value;
  } bools[] = {
    { "1",     1 }, { "true",  1 }, { "yes",   1 }, { "on",    1 },
    { "0",     0 }, { "false", 0 }, { "no",    0 }, { "off",   0 },
  };
  unsigned int i;

  if(val) {
    for(i = 0; i < sizeof(bools)/sizeof(bools[0]); i++) {
      if(!(strcasecmp(bools[i].str, val)))
	return bools[i].value;
    }
  }

  return 0;
}

const char *get_last_double_semicolon(const char *str) {
  int len;

  if(str && (len = strlen(str))) {
    const char *p = str + (len - 1);

    while(p > str) {

      if((*p == ':') && (*(p - 1) == ':'))
	return (p - 1);

      p--;
    }
  }

  return NULL;
}

int is_ipv6_last_double_semicolon(const char *str) {

  if(str && strlen(str)) {
    const char *d_semic = get_last_double_semicolon(str);

    if(d_semic) {
      const char *bracketl = NULL;
      const char *bracketr = NULL;
      const char *p        = d_semic + 2;

      while(*p && !bracketr) {
	if(*p == ']')
	  bracketr = p;

	p++;
      }

      if(bracketr) {
	p = d_semic;

	while((p >= str) && !bracketl) {
	  if(*p == '[')
	    bracketl = p;

	  p--;
	}

	if(bracketl) {

	  /* Look like an IPv6 address, check it */
	  p = d_semic + 2;

	  while(*p) {
	    switch(*p) {
	    case ':':
	    case '.':
	      break;

	    case ']': /* IPv6 delimiter end */

	      /* now go back to '[' */
	      p = d_semic;

	      while(p >= str) {
		switch(*p) {
		case ':':
		  break;

		case '.': /* This couldn't happen */
		  return 0;
		  break;

		case '[': /* We reach the beginning, it's a valid IPv6 */
		  return 1;
		  break;

		default:
		  if(!isxdigit(*p))
		    return 0;
		  break;

		}

		p--;
	      }
	      break;

	    default:
	      if(!isxdigit(*p))
		return 0;
	      break;

	    }

	    p++;
	  }
	}
      }
    }
  }

  return 0;
}

int is_downloadable(const char *filename) {
  if(filename &&
     ((!strncasecmp(filename, "http://", 7)) || (!strncasecmp(filename, "ftp://", 6))))
    return 1;
  return 0;
}

int is_a_dir(const char *filename) {
  struct stat  pstat;

  if((stat(filename, &pstat)) < 0)
    return 0;

  return (S_ISDIR(pstat.st_mode));
}

int is_a_file(const char *filename) {
  struct stat  pstat;

  if((stat(filename, &pstat)) < 0)
    return 0;

  return (S_ISREG(pstat.st_mode));
}

char *xitk_vasprintf(const char *format, va_list args)
{
  char *result;
  int   size = 100;

  result = malloc(size);
  if (!result) {
    fprintf(stderr, "xitk_vasprintf: out of memory\n");
    return NULL;
  }

  while (1) {
    char *tmp;
    va_list ap;
    int n;

    va_copy(ap, args);
    n = vsnprintf(result, size, format, ap);
    va_end(ap);

    if (n > -1 && n < size)
      break;

    if(n > -1)
      size = n + 1;
    else
      size *= 2;

    if((tmp = realloc(result, size)) == NULL) {
      fprintf(stderr, "xitk_vasprintf: out of memory\n");
      free(result);
      return NULL;
    }
    result = tmp;
  }

  return result;
}

char *xitk_asprintf(const char *format, ...)
{
  va_list args;
  char *result;

  va_start(args, format);
  result = xitk_vasprintf(format, args);
  va_end(args);

  return result;
}
