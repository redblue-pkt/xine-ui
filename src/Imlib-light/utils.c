#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib.h"
#include "Imlib_private.h"

#ifndef HAVE_SNPRINTF
#define snprintf my_snprintf
#ifdef HAVE_STDARGS
int                 my_snprintf(char *str, size_t count, const char *fmt,...);

#else
int                 my_snprintf(va_alist);

#endif
#endif

void
calc_map_tables(ImlibData * id, ImlibImage * im)
{
  int                 i;
  double              g, b, c, ii, v;

  if (!im)
    return;

  g = ((double)im->mod.gamma) / 256;
  b = ((double)im->mod.brightness) / 256;
  c = ((double)im->mod.contrast) / 256;
  if (g < 0.01)
    g = 0.01;

  for (i = 0; i < 256; i++)
    {
      ii = ((double)i) / 256;
      v = ((ii - 0.5) * c) + 0.5 + (b - 1);
      if (v > 0)
	v = pow(((ii - 0.5) * c) + 0.5 + (b - 1), 1 / g) * 256;
      else
	v = 0;
      if (v > 255)
	v = 255;
      else if (v < 0)
	v = 0;
      im->rmap[i] = (unsigned char)v;
      im->gmap[i] = (unsigned char)v;
      im->bmap[i] = (unsigned char)v;
    }
  g = ((double)im->rmod.gamma) / 256;
  b = ((double)im->rmod.brightness) / 256;
  c = ((double)im->rmod.contrast) / 256;
  if (g < 0.01)
    g = 0.01;

  for (i = 0; i < 256; i++)
    {
      ii = ((double)im->rmap[i]) / 256;
      v = ((ii - 0.5) * c) + 0.5 + (b - 1);
      if (v > 0)
	v = pow(((ii - 0.5) * c) + 0.5 + (b - 1), 1 / g) * 256;
      else
	v = 0;
      if (v > 255)
	v = 255;
      else if (v < 0)
	v = 0;
      im->rmap[i] = (unsigned char)v;
    }
  g = ((double)im->gmod.gamma) / 256;
  b = ((double)im->gmod.brightness) / 256;
  c = ((double)im->gmod.contrast) / 256;
  if (g < 0.01)
    g = 0.01;

  for (i = 0; i < 256; i++)
    {
      ii = ((double)im->gmap[i]) / 256;
      v = ((ii - 0.5) * c) + 0.5 + (b - 1);
      if (v > 0)
	v = pow(((ii - 0.5) * c) + 0.5 + (b - 1), 1 / g) * 256;
      else
	v = 0;
      if (v > 255)
	v = 255;
      else if (v < 0)
	v = 0;
      im->gmap[i] = (unsigned char)v;
    }
  g = ((double)im->bmod.gamma) / 256;
  b = ((double)im->bmod.brightness) / 256;
  c = ((double)im->bmod.contrast) / 256;
  if (g < 0.01)
    g = 0.01;
  for (i = 0; i < 256; i++)
    {
      ii = ((double)im->bmap[i]) / 256;
      v = ((ii - 0.5) * c) + 0.5 + (b - 1);
      if (v > 0)
	v = pow(((ii - 0.5) * c) + 0.5 + (b - 1), 1 / g) * 256;
      else
	v = 0;
      if (v > 255)
	v = 255;
      else if (v < 0)
	v = 0;
      im->bmap[i] = (unsigned char)v;
    }
  dirty_pixmaps(id, im);
}

ImlibImage         *
Imlib_clone_image(ImlibData * id, ImlibImage * im)
{
  ImlibImage         *im2;
  char               *s;

  if (!im)
    return NULL;
  im2 = malloc(sizeof(ImlibImage));
  if (!im2)
    return NULL;
  im2->rgb_width = im->rgb_width;
  im2->rgb_height = im->rgb_height;
  im2->rgb_data = malloc(im2->rgb_width * im2->rgb_height * 3);
  if (!im2->rgb_data)
    {
      free(im2);
      return NULL;
    }
  memcpy(im2->rgb_data, im->rgb_data, im2->rgb_width * im2->rgb_height * 3);
  if (im->alpha_data)
    {
      im2->alpha_data = malloc(im2->rgb_width * im2->rgb_height);
      if (!im2->alpha_data)
	{
	  free(im2->rgb_data);
	  free(im2);
	  return NULL;
	}
      memcpy(im2->alpha_data, im->alpha_data, im2->rgb_width * im2->rgb_height);
    }
  else
    im2->alpha_data = NULL;
  s = malloc(strlen(im->filename) + 320);
  if (s)
    {
      snprintf(s, sizeof(s), "%s_%x_%x", im->filename, (int)time(NULL), (int)rand());
      im2->filename = malloc(strlen(s) + 1);
      if (im2->filename)
	strcpy(im2->filename, s);
      free(s);
    }
  else
    im2->filename = NULL;
  im2->width = 0;
  im2->height = 0;
  im2->shape_color.r = im->shape_color.r;
  im2->shape_color.g = im->shape_color.g;
  im2->shape_color.b = im->shape_color.b;
  im2->border.left = im->border.left;
  im2->border.right = im->border.right;
  im2->border.top = im->border.top;
  im2->border.bottom = im->border.bottom;
  im2->pixmap = 0;
  im2->shape_mask = 0;
  im2->cache = 1;
  im2->mod.gamma = im->mod.gamma;
  im2->mod.brightness = im->mod.brightness;
  im2->mod.contrast = im->mod.contrast;
  im2->rmod.gamma = im->rmod.gamma;
  im2->rmod.brightness = im->rmod.brightness;
  im2->rmod.contrast = im->rmod.contrast;
  im2->gmod.gamma = im->gmod.gamma;
  im2->gmod.brightness = im->gmod.brightness;
  im2->gmod.contrast = im->gmod.contrast;
  im2->bmod.gamma = im->bmod.gamma;
  im2->bmod.brightness = im->bmod.brightness;
  im2->bmod.contrast = im->bmod.contrast;
  calc_map_tables(id, im2);
  if (id->cache.on_image)
    add_image(id, im2, im2->filename);
  return im2;
}

void
Imlib_apply_image(ImlibData * id, ImlibImage * im, Window p)
{
  Pixmap              pp, mm;
  int                 x, y;
  unsigned int        w, h, bd, d;
  Window              ww;

  if ((!im) || (!p))
    return;
  XGetGeometry(id->x.disp, p, &ww, &x, &y, &w, &h, &bd, &d);
  if ((w <= 0) || (h <= 0))
    return;
  Imlib_render(id, im, w, h);
  pp = Imlib_move_image(id, im);
  mm = Imlib_move_mask(id, im);
  XSetWindowBackgroundPixmap(id->x.disp, p, pp);
  if (mm)
    XShapeCombineMask(id->x.disp, p, ShapeBounding, 0, 0, mm, ShapeSet);
  else
    XShapeCombineMask(id->x.disp, p, ShapeBounding, 0, 0, 0, ShapeSet);
  XClearWindow(id->x.disp, p);
  Imlib_free_pixmap(id, pp);
}
