/*
 * taken from xine-lib
 */

#include <config.h>

#ifndef HAVE_STRNDUP

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n) {
  char *ret;
  
  ret = malloc (n + 1);
  strncpy(ret, s, n);
  ret[n] = '\0';
  return ret;
}

#endif


