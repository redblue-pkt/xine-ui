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

ImlibData *Imlib_init(Display * disp) {
  ImlibData          *id;
  XWindowAttributes   xwa;
  XVisualInfo         xvi, *xvir;
  int                 override = 0;
  int                 dither = 0;
  int                 remap = 1;
  int                 num;
  int                 i, max, maxn;
  int                 clas;
  int                 loadpal;
  int                 vis;
  int                 newcm;

  if (!disp) {
    fprintf(stderr, "IMLIB ERROR: no display\n");
    return NULL;
  }

  vis = -1;
  loadpal = 0;
  id = (ImlibData *) malloc(sizeof(ImlibData));
  if (!id)
    {
      fprintf(stderr, "IMLIB ERROR: Cannot alloc RAM for Initial data struct\n");
      return NULL;
    }
  id->palette = NULL;
  id->palette_orig = NULL;
  id->fast_rgb = NULL;
  id->fast_err = NULL;
  id->fast_erg = NULL;
  id->fast_erb = NULL;
  id->x.disp = disp;
  id->x.screen = DefaultScreen(disp);	/* the screen number */
  id->x.root = DefaultRootWindow(disp);		/* the root window id */
  id->x.visual = DefaultVisual(disp, id->x.screen);	/* the visual type */
  id->x.depth = DefaultDepth(disp, id->x.screen);	/* the depth of the screen in bpp */

  id->cache.on_image = 0;
  id->cache.size_image = 0;
  id->cache.num_image = 0;
  id->cache.used_image = 0;
  id->cache.image = NULL;
  id->cache.on_pixmap = 0;
  id->cache.size_pixmap = 0;
  id->cache.num_pixmap = 0;
  id->cache.used_pixmap = 0;
  id->cache.pixmap = NULL;
  id->byte_order = 0;
  id->fastrend = 0;
  id->hiq = 0;
  id->fallback = 1;
  id->mod.gamma = 256;
  id->mod.brightness = 256;
  id->mod.contrast = 256;
  id->rmod.gamma = 256;
  id->rmod.brightness = 256;
  id->rmod.contrast = 256;
  id->gmod.gamma = 256;
  id->gmod.brightness = 256;
  id->gmod.contrast = 256;
  id->bmod.gamma = 256;
  id->bmod.brightness = 256;
  id->bmod.contrast = 256;
  id->ordered_dither = 1;

  if (XGetWindowAttributes(disp, id->x.root, &xwa)) {
    if (xwa.colormap)
      id->x.root_cmap = xwa.colormap;
    else
      id->x.root_cmap = 0;
    }
  else
    id->x.root_cmap = 0;
  id->num_colors = 0;

  /* list all visuals for the default screen */
  xvi.screen = id->x.screen;
  xvir = XGetVisualInfo(disp, VisualScreenMask, &xvi, &num);
  if (vis >= 0)
    {
      /* use the forced visual id */
      maxn = 0;
      for (i = 0; i < num; i++)
	{
	  if (xvir[i].visualid == (VisualID) vis)
	    maxn = i;
	}
      if (maxn >= 0)
	{
	  unsigned long       rmsk, gmsk, bmsk;

	  id->x.depth = xvir[maxn].depth;
	  id->x.visual = xvir[maxn].visual;
	  rmsk = xvir[maxn].red_mask;
	  gmsk = xvir[maxn].green_mask;
	  bmsk = xvir[maxn].blue_mask;

	  if ((rmsk > gmsk) && (gmsk > bmsk))
	    id->byte_order = BYTE_ORD_24_RGB;
	  else if ((rmsk > bmsk) && (bmsk > gmsk))
	    id->byte_order = BYTE_ORD_24_RBG;
	  else if ((bmsk > rmsk) && (rmsk > gmsk))
	    id->byte_order = BYTE_ORD_24_BRG;
	  else if ((bmsk > gmsk) && (gmsk > rmsk))
	    id->byte_order = BYTE_ORD_24_BGR;
	  else if ((gmsk > rmsk) && (rmsk > bmsk))
	    id->byte_order = BYTE_ORD_24_GRB;
	  else if ((gmsk > bmsk) && (bmsk > rmsk))
	    id->byte_order = BYTE_ORD_24_GBR;
	  else
	    id->byte_order = 0;
	}
      else
	fprintf(stderr, "IMLIB ERROR: Visual Id no 0x%x specified in the imrc file is invalid on this display.\nUsing Default Visual.\n", vis);
    }
  else
    {
      if (xvir)
	{
	  /* find the highest bit-depth supported by visuals */
	  max = 0;
	  for (i = 0; i < num; i++)
	    {
	      if (xvir[i].depth > max)
		max = xvir[i].depth;
	    }
	  if (max > 8)
	    {
	      id->x.depth = max;
	      clas = -1;
	      maxn = -1;
	      for (i = 0; i < num; i++)
		{
		  if (xvir[i].depth == id->x.depth)
		    {
		      if ((xvir[i].class > clas) && (xvir[i].class != DirectColor))
			{
			  maxn = i;
			  clas = xvir[i].class;
			}
		    }
		}
	      if (maxn >= 0)
		{
		  unsigned long       rmsk, gmsk, bmsk;

		  id->x.visual = xvir[maxn].visual;
		  rmsk = xvir[maxn].red_mask;
		  gmsk = xvir[maxn].green_mask;
		  bmsk = xvir[maxn].blue_mask;

		  if ((rmsk > gmsk) && (gmsk > bmsk))
		    id->byte_order = BYTE_ORD_24_RGB;
		  else if ((rmsk > bmsk) && (bmsk > gmsk))
		    id->byte_order = BYTE_ORD_24_RBG;
		  else if ((bmsk > rmsk) && (rmsk > gmsk))
		    id->byte_order = BYTE_ORD_24_BRG;
		  else if ((bmsk > gmsk) && (gmsk > rmsk))
		    id->byte_order = BYTE_ORD_24_BGR;
		  else if ((gmsk > rmsk) && (rmsk > bmsk))
		    id->byte_order = BYTE_ORD_24_GRB;
		  else if ((gmsk > bmsk) && (bmsk > rmsk))
		    id->byte_order = BYTE_ORD_24_GBR;
		  else
		    id->byte_order = 0;
		}
	    }
	}
    }
  id->x.render_depth = id->x.depth;
  XFree(xvir);
  if (id->x.depth == 16)
    {
      xvi.visual = id->x.visual;
      xvi.visualid = XVisualIDFromVisual(id->x.visual);
      xvir = XGetVisualInfo(disp, VisualIDMask, &xvi, &num);
      if (xvir)
	{
	  if (xvir->red_mask != 0xf800)
	    id->x.render_depth = 15;
	  XFree(xvir);
	}
    }
  if ((id->x.depth <= 8) || (override == 1))
    loadpal = 1;
  if (loadpal)
    {
      if (dither == 1)
	{
	  if (remap == 1)
	    id->render_type = RT_DITHER_PALETTE_FAST;
	  else
	    id->render_type = RT_DITHER_PALETTE;
	}
      else
	{
	  if (remap == 1)
	    id->render_type = RT_PLAIN_PALETTE_FAST;
	  else
	    id->render_type = RT_PLAIN_PALETTE;
	}
    }
  else
    {
      if (id->hiq == 1)
	id->render_type = RT_DITHER_TRUECOL;
      else
	id->render_type = RT_PLAIN_TRUECOL;
    }
  {
    XSetWindowAttributes at;
    unsigned long       mask;

    at.border_pixel = 0;
    at.backing_store = NotUseful;
    at.background_pixel = 0;
    at.save_under = False;
    at.override_redirect = True;
    mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel |
      CWBackingStore | CWSaveUnder;
    newcm = 0;
    if (id->x.visual != DefaultVisual(disp, id->x.screen))
      {
	Colormap            cm;

	cm = XCreateColormap(id->x.disp, id->x.root,
			     id->x.visual, AllocNone);
	if (cm)
	  {
	    mask |= CWColormap;
	    id->x.root_cmap = cm;
	    at.colormap = cm;
	    newcm = 1;
	  }
      }
    id->x.base_window = XCreateWindow(id->x.disp, id->x.root,
				      -100, -100, 10, 10, 0,
				      id->x.depth, InputOutput,
				      id->x.visual, mask, &at);
  }
  {
    /* Turn off fastrender if there is an endianess diff between */
    /* client and Xserver */
    int                 byt, bit;

    byt = ImageByteOrder(id->x.disp);	/* LSBFirst | MSBFirst */
    bit = BitmapBitOrder(id->x.disp);	/* LSBFirst | MSBFirst */
    id->x.byte_order = byt;
    id->x.bit_order = bit;
    /* if little endian && server big */
    if ((htonl(1) != 1) && (byt == MSBFirst))
      id->fastrend = 0;
    /* if big endian && server little */
    if ((htonl(1) == 1) && (byt == LSBFirst))
      id->fastrend = 0;
  }

  /* printf ("Imlib init, d : %d visual : %d\n", id, id->x.visual); */

  return id;
}

Pixmap
Imlib_copy_image(ImlibData * id, ImlibImage * im)
{
  Pixmap              p;
  GC                  tgc;
  XGCValues           gcv;

  if (!im || !im->pixmap)
    return 0;
  p = XCreatePixmap(id->x.disp, id->x.base_window, im->width, im->height, id->x.depth);
  gcv.graphics_exposures = False;
  tgc = XCreateGC(id->x.disp, p, GCGraphicsExposures, &gcv);
  XCopyArea(id->x.disp, im->pixmap, p, tgc, 0, 0, im->width, im->height, 0, 0);
  XFreeGC(id->x.disp, tgc);
  return p;
}

Pixmap
Imlib_move_image(ImlibData * id, ImlibImage * im)
{
  Pixmap              p;

  if (!im)
    return 0;
  p = im->pixmap;
  im->pixmap = 0;
  return p;
}

Pixmap
Imlib_copy_mask(ImlibData * id, ImlibImage * im)
{
  Pixmap              p;
  GC                  tgc;
  XGCValues           gcv;

  if (!im || !im->shape_mask)
    return 0;
  p = XCreatePixmap(id->x.disp, id->x.base_window, im->width, im->height, 1);
  gcv.graphics_exposures = False;
  tgc = XCreateGC(id->x.disp, p, GCGraphicsExposures, &gcv);
  XCopyArea(id->x.disp, im->shape_mask, p, tgc, 0, 0, im->width, im->height, 0, 0);
  XFreeGC(id->x.disp, tgc);
  return p;
}

Pixmap
Imlib_move_mask(ImlibData * id, ImlibImage * im)
{
  Pixmap              p;

  if (!im)
    return 0;
  p = im->shape_mask;
  im->shape_mask = 0;
  return p;
}

void
Imlib_destroy_image(ImlibData * id, ImlibImage * im)
{
  if (im)
    {
      if (id->cache.on_image)
	{
	  free_image(id, im);
	  clean_caches(id);
	}
      else
	nullify_image(id, im);
    }
}

void
Imlib_kill_image(ImlibData * id, ImlibImage * im)
{
  if (im)
    {
      if (id->cache.on_image)
	{
	  free_image(id, im);
	  flush_image(id, im);
	  clean_caches(id);
	}
      else
	nullify_image(id, im);
    }
}

void
Imlib_free_pixmap(ImlibData * id, Pixmap pmap)
{
  if (pmap)
    {
      free_pixmappmap(id, pmap);
      clean_caches(id);
    }
}
