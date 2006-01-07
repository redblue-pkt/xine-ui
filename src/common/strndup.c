/*
 * written for xine project, 2006
 *
 * public domain replacement function of getline
 *
 */

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n) {
  char *ret;
  
  ret = malloc (n + 1);
  strncpy(ret, s, n);
  ret[n] = '\0';
  return ret;
}
