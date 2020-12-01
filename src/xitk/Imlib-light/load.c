#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib.h"
#include "Imlib_types.h"
#include "Imlib_private.h"
#include "Imlib_decode.h"

/*
 *      Split the ID - damages input
 */
static void _SplitID(char *file) {
#ifndef __EMX__
  char *p = strrchr(file, ':');
#else
  char *p = strrchr(file, ';');
#endif

  if (p) {
    *p++ = 0;
  }
}

static ImlibImage * new_image(ImlibData * id, unsigned char *data, int w, int h, int trans) {
  ImlibImage         *im;

  im = (ImlibImage *) malloc(sizeof(ImlibImage));
  if (!im)
    return NULL;

  im->alpha_data = NULL;
  im->shape = trans;
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

  return im;
}

ImlibImage * Imlib_decode_image(ImlibData * id, const void *p, size_t size) {
  int                 w, h;
  unsigned char      *data = NULL;
  ImlibImage         *im;
  int                 trans = 0;

  if (!id || !p)
    return NULL;

  data = Imlib_decode_image_rgb(p, size, &w, &h, &trans);

  if (!data || !w || !h) {
    free(data);
    return NULL;
  }

  im = new_image(id, data, w, h, trans);
  if (!im) {
      fprintf(stderr, "IMLIB ERROR: Cannot allocate RAM for image structure\n");
      free(data);
      return NULL;
  }

  im->filename = strdup("data");
  calc_map_tables(id, im);
  return im;
}

ImlibImage * Imlib_load_image(ImlibData * id, const char *file) {
  int                 w, h;
  unsigned char      *data = NULL;
  ImlibImage         *im;
  char               *fil = NULL;
  int                 trans = 0;

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

  data = Imlib_load_image_rgb(file, &w, &h, &trans);
  if (!data)
    return NULL;

  im = new_image(id, data, w, h, trans);
  if (!im)
    {
      fprintf(stderr, "IMLIB ERROR: Cannot allocate RAM for image structure\n");
      goto error;
    }
  fil = strdup(file);
  _SplitID(fil);
  im->filename = strdup(file);
  if ((id->cache.on_image) && (im))
    add_image(id, im, fil);
  calc_map_tables(id, im);
  free(fil);
  return im;
 error:
  free(data);
  return NULL;
}
