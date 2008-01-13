#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib.h"
#include "Imlib_private.h"
#include <setjmp.h>

/*
 *      Split the ID - damages input
 */
static char *_SplitID(char *file) {
#ifndef __EMX__
  char *p = strrchr(file, ':');
#else
  char *p = strrchr(file, ';');
#endif
  
  if (p == NULL)
    return "";
  else {
    *p++ = 0;
    return p;
  }
}

/*
 * *     Doesn't damage the input
 */
char *_GetExtension(char *file) {
  char *p = strrchr(file, '.');

  if (p == NULL)
    return "";
  else
    return p + 1;
}

/** 
 *  * This error handling is broken beyond belief, but oh well it works
 *  **/
unsigned char *_LoadPNG(ImlibData * id, FILE * f, int *w, int *h, int *t) {
  png_structp         png_ptr;
  png_infop           info_ptr;
  unsigned char      *data, *ptr, **lines, *ptr2, r, g, b, a;
  int                 i, x, y, transp, bit_depth, color_type, interlace_type;
  png_uint_32         ww, hh;

  /* Init PNG Reader */
  transp = 0;
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return NULL;
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return NULL;
    }
  if (info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return NULL;
    }
  png_init_io(png_ptr, f);
  /* Read Header */
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &ww, &hh, &bit_depth, &color_type, &interlace_type,
	       NULL, NULL);
  *w = ww;
  *h = hh;
  /* Setup Translators */
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand(png_ptr);
  png_set_strip_16(png_ptr);
  png_set_packing(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_expand(png_ptr);
  png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
  data = malloc(*w ** h * 3);
  if (!data)
    {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return NULL;
    }
  lines = (unsigned char **)malloc(*h * sizeof(unsigned char *));

  if (lines == NULL)
    {
      free(data);
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return NULL;
    }

  for (i = 0; i < *h; i++)
    {
      if ((lines[i] = malloc(*w * (sizeof(unsigned char) * 4))) == NULL)
	{
	  int                 n;

	  free(data);
	  for (n = 0; n < i; n++)
	    free(lines[n]);
	  free(lines);
	  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	  return NULL;
	}
    }

  png_read_image(png_ptr, lines);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  ptr = data;
  if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
      for (y = 0; y < *h; y++)
	{
	  ptr2 = lines[y];
	  for (x = 0; x < *w; x++)
	    {
	      r = *ptr2++;
	      a = *ptr2++;
	      if (a < 128)
		{
		  *ptr++ = 255;
		  *ptr++ = 0;
		  *ptr++ = 255;
		  transp = 1;
		}
	      else
		{
		  *ptr++ = r;
		  *ptr++ = r;
		  *ptr++ = r;
		}
	    }
	}
    }
  else if (color_type == PNG_COLOR_TYPE_GRAY)
    {
      for (y = 0; y < *h; y++)
	{
	  ptr2 = lines[y];
	  for (x = 0; x < *w; x++)
	    {
	      r = *ptr2++;
	      *ptr++ = r;
	      *ptr++ = r;
	      *ptr++ = r;
	    }
	}
    }
  else
    {
      for (y = 0; y < *h; y++)
	{
	  ptr2 = lines[y];
	  for (x = 0; x < *w; x++)
	    {
	      r = *ptr2++;
	      g = *ptr2++;
	      b = *ptr2++;
	      a = *ptr2++;
	      if (a < 128)
		{
		  *ptr++ = 255;
		  *ptr++ = 0;
		  *ptr++ = 255;
		  transp = 1;
		}
	      else
		{
		  if ((r == 255) && (g == 0) && (b == 255))
		    r = 254;
		  *ptr++ = r;
		  *ptr++ = g;
		  *ptr++ = b;
		}
	    }
	}
    }
  for (i = 0; i < *h; i++)
    free(lines[i]);
  free(lines);
  *t = transp;
  return data;
}

int ispng(FILE *f) {
  unsigned char       buf[8];
  
  if (!f)
    return 0;
  fread(buf, 1, 8, f);
  rewind(f);
  return (int)png_check_sig(buf, 8);
}

ImlibImage * Imlib_load_image(ImlibData * id, char *file) {
  int                 w, h;
  unsigned char      *data = NULL;
  ImlibImage         *im;
  char               *fil = NULL;
  char               *iden;
  char               *e;
  FILE               *p;
  int                 fmt;
  int                 trans;

  fmt = 0;
  data = NULL;

  if (!file)
    return NULL;
  if (id->cache.on_image)
    if ((im = find_image(id, file)))
      {
        if (im->rgb_width == 0 || im->rgb_height == 0)
          {
            Imlib_destroy_image(id, im);
            return NULL;
          }
        else
          return im;
      }
  if (!strcmp(file,"-")) {
  	p = stdin;
  }
  else {
  	p = fopen(file, "rb");
  }
  if (!p)
	return NULL;
  fil = strdup(file);
  iden = _SplitID(fil);
  e = _GetExtension(fil);

  if (ispng(p))
    fmt = 1;

  trans = 0;
  if (!data) {
      switch (fmt)
	{
	case 1:
	    data = _LoadPNG(id, p, &w, &h, &trans);
	  break;
	}
    }
  if (p) {
    if (p != stdin) 
      fclose(p);
  }
  
  if (!data)
    {
      fprintf(stderr, "IMLIB ERROR: Cannot load image: %s\nAll fallbacks failed.\n", fil);
      goto error;
    }
    
  if (!w || !h)
    {
      fprintf(stderr, "IMLIB ERROR: zero image\n" );
      goto error;
    }
    
  im = (ImlibImage *) malloc(sizeof(ImlibImage));
  if (!im)
    {
      fprintf(stderr, "IMLIB ERROR: Cannot allocate RAM for image structure\n");
      goto error;
    }
  im->alpha_data = NULL;
  if (trans)
    {
      im->shape_color.r = 255;
      im->shape_color.g = 0;
      im->shape_color.b = 255;
    }
  else
    {
      im->shape_color.r = -1;
      im->shape_color.g = -1;
      im->shape_color.b = -1;
    }
  im->border.left = 0;
  im->border.right = 0;
  im->border.top = 0;
  im->border.bottom = 0;
  im->cache = 1;
  im->rgb_data = data;
  im->rgb_width = w;
  im->rgb_height = h;
  im->pixmap = 0;
  im->shape_mask = 0;
  im->mod.gamma = id->mod.gamma;
  im->mod.brightness = id->mod.brightness;
  im->mod.contrast = id->mod.contrast;
  im->rmod.gamma = id->rmod.gamma;
  im->rmod.brightness = id->rmod.brightness;
  im->rmod.contrast = id->rmod.contrast;
  im->gmod.gamma = id->gmod.gamma;
  im->gmod.brightness = id->gmod.brightness;
  im->gmod.contrast = id->gmod.contrast;
  im->bmod.gamma = id->bmod.gamma;
  im->bmod.brightness = id->bmod.brightness;
  im->bmod.contrast = id->bmod.contrast;
  im->filename = malloc(strlen(file) + 1);
  if (im->filename)
    strcpy(im->filename, file);
  if ((id->cache.on_image) && (im))
    add_image(id, im, fil);
  calc_map_tables(id, im);
  free(fil);
  return im;
 error:
  free(fil);
  free(data);
  return NULL;
}
