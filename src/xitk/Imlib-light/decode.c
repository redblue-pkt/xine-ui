#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib_decode.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <png.h>
#include <jpeglib.h>

#ifndef INT_MAX
#define INT_MAX ((int)((unsigned int)(1 << (8 * sizeof(int) - 1)) - 1))
#endif

struct _png_data {
  unsigned long pos;
  unsigned long size;
  const unsigned char *p;
};
static void _png_user_read(png_structp png, png_bytep data, png_size_t length)
{
  struct _png_data *p = (struct _png_data *)png_get_io_ptr(png);

  if (p->pos + length > p->size) {
    length = p->size - p->pos;
    fprintf(stderr, "png: not enough data\n");
  }

  memcpy(data, p->p + p->pos, length);
  p->pos += length;
}

static unsigned char *_LoadPNG(FILE * f,
                               struct _png_data *indata,
                               int *w, int *h, int *t) {
  png_structp         png_ptr;
  png_infop           info_ptr;
  unsigned char      *data, **lines;
  int                 bit_depth, color_type, interlace_type, trans;
  png_uint_32         ww, hh;

  if (!f && (!indata || !indata->p))
    return NULL;

  /* Init PNG Reader */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return NULL;
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return NULL;
    }
  if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB_ALPHA)
    {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return NULL;
    }
  if (f) {
    png_init_io(png_ptr, f);
  } else {
    png_set_read_fn (png_ptr, indata, _png_user_read);
  }
  /* Read Header */
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &ww, &hh, &bit_depth, &color_type, &interlace_type,
	       NULL, NULL);
  *w = ww;
  *h = hh;

  if ((*h <= 0) || (*w <= 0) || (*w > (INT_MAX / *h) / 3) || (*w > INT_MAX / 4)) {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }
  data = malloc (*w * *h * 4 + 4);
  if (!data) {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }
  lines = malloc (*h * sizeof (*lines));
  if (!lines) {
    free (data);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }
  if ((color_type == PNG_COLOR_TYPE_GRAY_ALPHA) || (color_type == PNG_COLOR_TYPE_GRAY)) {
    size_t ls = *w * 2;
    int i;
    lines[0] = data + *w * *h * 2;
    for (i = 1; i < *h; i++)
      lines[i] = lines[i - 1] + ls;
  } else {
    size_t ls = *w * 4;
    int i;
    lines[0] = data;
    for (i = 1; i < *h; i++)
      lines[i] = lines[i - 1] + ls;
  }

  /* Setup Translators */
  /* Long term libpng BUG: palette images are always reported without alpha :-/ */
  if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
    color_type |= PNG_COLOR_MASK_ALPHA;
#ifndef PNG_COLOR_TYPE_PALETTE_ALPHA
#  define PNG_COLOR_TYPE_PALETTE_ALPHA (PNG_COLOR_MASK_PALETTE | PNG_COLOR_MASK_ALPHA)
#endif
  if (color_type & PNG_COLOR_MASK_PALETTE)
    png_set_expand(png_ptr);
  png_set_strip_16(png_ptr);
  png_set_packing(png_ptr);
  png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

  png_read_image(png_ptr, lines);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  trans = 1;
  if (!(color_type & PNG_COLOR_MASK_COLOR)) {
    const uint8_t *p = (const uint8_t *)lines[0];
    uint32_t *q = (uint32_t *)data;
    int n;
    trans = 0x80;
    for (n = *w * *h; n > 0; n--) {
      union {
        uint8_t b[4];
        uint32_t w;
      } v;
      v.b[0] = v.b[1] = v.b[2] = p[0];
      v.b[3] = p[1];
      trans &= p[1];
      p += 2;
      *q++ = v.w;
    }
  } else if (color_type & PNG_COLOR_MASK_ALPHA) {
    int n;
    const uint32_t *p = (const uint32_t *)data;
    union {
      uint8_t b[4];
      uint32_t w;
    } v;
    v.b[0] = v.b[1] = v.b[2] = 0;
    v.b[3] = 0x80;
    for (n = *w * *h; n > 0; n--)
      v.w &= *p++;
    trans = v.w;
  }

  free(lines);
  *t = trans == 0;
  return data;
}

static unsigned char *_LoadJPEG(FILE * f,
                                const unsigned char *inbuf, unsigned long insize,
                                int *w, int *h) {
  struct jpeg_error_mgr jpeg_error;
  struct jpeg_decompress_struct jpeg_ptr;
  JSAMPROW rowptr;
  JSAMPARRAY buffer = &rowptr;
  int rowstride;
  unsigned char *out, *outptr;

  if (!f && !inbuf)
    return NULL;

  jpeg_ptr.err = jpeg_std_error (&jpeg_error);
  jpeg_create_decompress (&jpeg_ptr);
  if (f) {
    rewind (f);
    jpeg_stdio_src (&jpeg_ptr, f);
  } else {
    jpeg_mem_src(&jpeg_ptr, inbuf, insize);
  }

  jpeg_read_header (&jpeg_ptr, TRUE);
  jpeg_start_decompress (&jpeg_ptr);

  outptr = out = malloc (4 * jpeg_ptr.output_width * jpeg_ptr.output_height);

  if (!buffer || !out)
  {
    free (out);
    jpeg_destroy_decompress (&jpeg_ptr);
    return NULL;
  }

  outptr = jpeg_ptr.output_components == 1
         ? out + (4 - 1) * jpeg_ptr.output_width * jpeg_ptr.output_height
         : out + (4 - 3) * jpeg_ptr.output_width * jpeg_ptr.output_height;
  rowstride = jpeg_ptr.output_width * jpeg_ptr.output_components;

  while (jpeg_ptr.output_scanline < jpeg_ptr.output_height)
  {
    buffer[0] = outptr;
    jpeg_read_scanlines (&jpeg_ptr, buffer, 1);
    outptr += rowstride;
  }

  *w = jpeg_ptr.output_width;
  *h = jpeg_ptr.output_height;

  jpeg_finish_decompress (&jpeg_ptr);
  jpeg_destroy_decompress (&jpeg_ptr);

  if (jpeg_ptr.output_components == 1) {
    const uint8_t *p = (const uint8_t *)out + (4 - 1) * *w * *h;
    uint32_t *q = (uint32_t *)out;
    int n;
    for (n = *w * *h; n > 0; n--) {
      union {
        uint8_t b[4];
        uint32_t w;
      } v;
      v.b[0] = v.b[1] = v.b[2] = p[0];
      v.b[3] = 0xff;
      p += 1;
      *q++ = v.w;
    }
  } else {
    /* FIXME: yuv -> rgb ?? */
  }

  return out;
}

static int ispng(const unsigned char *p, size_t size) {
  return (int) !png_sig_cmp(p, 0, size);
}

static int isjpeg(const unsigned char *p, size_t size) {
  return (size > 3) && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF && p[3] == 0xE0;
}

void *Imlib_decode_image_rgb(const void *p, size_t size, int *pw, int *ph, int *ptrans) {
  unsigned char      *data = NULL;

  if (!p)
    return NULL;

  if (ispng(p, size)) {
    struct _png_data d = {0, size, p };
    data = _LoadPNG(NULL, &d, pw, ph, ptrans);
  } else if (isjpeg(p, size))
    data = _LoadJPEG(NULL, p, size, pw, ph);

  if (!data) {
    fprintf(stderr, "IMLIB ERROR: Cannot load image from memory\nAll fallbacks failed.\n");
    return NULL;
    }

  if (!*pw || !*ph) {
    fprintf(stderr, "IMLIB ERROR: zero image\n" );
    free(data);
    return NULL;
  }

  return data;
}

static int ispng_file(FILE *f) {
  unsigned char       buf[8];

  if (!f) {
    return 0;
  }

  if (fread (buf, 1, sizeof (buf), f) < sizeof (buf))
  {
    rewind (f);
    return 0;
  }
  rewind (f);
  return ispng(buf, sizeof(buf));
}

static int isjpeg_file(FILE *f) {
  unsigned char       buf[4];

  if (!f) {
    return 0;
  }

  if (fread (buf, 1, sizeof (buf), f) < sizeof (buf))
  {
    rewind (f);
    return 0;
  }
  rewind (f);
  return isjpeg(buf, sizeof(buf));
}

void *Imlib_load_image_rgb(const char *file, int *pw, int *ph, int *ptrans) {
  FILE               *p;
  unsigned char      *data = NULL;
  enum {
    FORMAT_UNKNOWN,
    FORMAT_PNG,
    FORMAT_JPEG,
  }                   fmt = FORMAT_UNKNOWN;

  if (!strcmp(file,"-")) {
    p = stdin;
  }
  else {
    p = fopen(file, "rb");
  }
  if (!p)
    return NULL;

  if (ispng_file(p))
    fmt = FORMAT_PNG;
  else if (isjpeg_file(p))
    fmt = FORMAT_JPEG;

  *ptrans = 0;

  switch (fmt)
  {
    case FORMAT_PNG:
      data = _LoadPNG(p, NULL, pw, ph, ptrans);
        break;
    case FORMAT_JPEG:
      data = _LoadJPEG(p, NULL, 0, pw, ph);
        break;
    default:
        break;
  }

  if (p) {
    if (p != stdin)
      fclose(p);
  }

  if (!data)
  {
    fprintf(stderr, "IMLIB ERROR: Cannot load image: %s\nAll fallbacks failed.\n", file);
    free(data);
    return NULL;
  }

  if (!*pw || !*ph)
  {
    fprintf(stderr, "IMLIB ERROR: zero image\n" );
    free(data);
    return NULL;
  }

  return data;
}
