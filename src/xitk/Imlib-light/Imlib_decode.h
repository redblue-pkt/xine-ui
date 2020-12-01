
#ifndef __IMLIB_DECODE_H__
#define __IMLIB_DECODE_H__

#include <stddef.h>

void *Imlib_decode_image_rgb(const void *p, size_t size, int *pw, int *ph, int *ptrans);
void *Imlib_load_image_rgb(const char *file, int *pw, int *ph, int *ptrans);

#endif
