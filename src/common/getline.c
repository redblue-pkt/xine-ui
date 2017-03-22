/*
 * written for xine project, 2006
 *
 * public domain replacement function of getline
 *
 */

#include <config.h>

#ifndef HAVE_GETLINE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "libcommon.h"

#define BLOCK_SIZE 128

static ssize_t getdelims(char **lineptr, size_t *n, const char *delims, FILE *stream) {
  void *tmp;
  int c;
  size_t i;

  if (!lineptr || !n || !delims || !stream) {
    errno = EINVAL;
    return -1;
  }
  if (!*lineptr) *n = 0;
  i = 0;

  while ((c = fgetc(stream)) != EOF) {
    if (i + 1 >= *n) {
      if ((tmp = realloc(*lineptr, *n + BLOCK_SIZE)) == NULL) {
        errno = ENOMEM;
        return -1;
      }
      *lineptr = tmp;
      *n += BLOCK_SIZE;
    }
    (*lineptr)[i++] = (unsigned char)c;
    if (strchr(delims, c)) break;
  }
  if (i != 0) (*lineptr)[i] = '\0';

  return (i == 0) ? -1 : (ssize_t)i;
}


ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
  return getdelims(lineptr, n, "\n\r", stream);
}

#endif
