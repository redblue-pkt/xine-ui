#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib.h"
#include "Imlib_types.h"
#include "Imlib_private.h"

#ifdef __EMX__
extern const char *__XOS2RedirRoot(const char*);
#endif

static int PaletteLUTGet(ImlibData *id);
static void PaletteLUTSet(ImlibData *id);

static int
PaletteLUTGet(ImlibData *id)
{
  unsigned char      *retval;
  Atom                type_ret;
  unsigned long       bytes_after, num_ret;
  int                 format_ret;
  long                length;
  Atom                to_get;

  retval = NULL;
  length = 0x7fffffff;
  to_get = XInternAtom(id->x.disp, "_IMLIB_COLORMAP", False);
  XGetWindowProperty(id->x.disp, id->x.root, to_get, 0, length, False,
		     XA_CARDINAL, &type_ret, &format_ret, &num_ret,
		     &bytes_after, &retval);
  if ((retval) && (num_ret > 0) && (format_ret > 0))
    {
      if (format_ret == 8)
	{
	  int i, pnum;
	  unsigned long j;

	  pnum = (int)(retval[0]);
	  j = 1;
	  if (pnum != id->num_colors)
	    {
	      XFree(retval);
	      return 0;
	    }
	  for (i = 0; i < id->num_colors; i++)
	    {
	      if (retval[j++] != ((unsigned char)id->palette[i].r))
		{
		  XFree(retval);
		  return 0;
		}
	      if (retval[j++] != ((unsigned char)id->palette[i].g))
		{
		  XFree(retval);
		  return 0;
		}
	      if (retval[j++] != ((unsigned char)id->palette[i].b))
		{
		  XFree(retval);
		  return 0;
		}
	      if (retval[j++] != ((unsigned char)id->palette[i].pixel))
		{
		  XFree(retval);
		  return 0;
		}
	    }
	  free(id->fast_rgb);
	  id->fast_rgb = malloc(sizeof(unsigned char) * 32 * 32 * 32);
	  for (i = 0; (i < (32 * 32 * 32)) && (j < num_ret); i++)
	    id->fast_rgb[i] = retval[j++];
	  XFree(retval);
	  return 1;
	}
      else
	XFree(retval);
    }
  return 0;
}

static void
PaletteLUTSet(ImlibData *id)
{
  Atom                to_set;
  unsigned char       *prop;
  int                 i, j;

  to_set = XInternAtom(id->x.disp, "_IMLIB_COLORMAP", False);
  prop = malloc((id->num_colors * 4) + 1 + (32 * 32 * 32));
  prop[0] = id->num_colors;
  j = 1;
  for (i = 0; i < id->num_colors; i++)
    {
      prop[j++] = (unsigned char)id->palette[i].r;
      prop[j++] = (unsigned char)id->palette[i].g;
      prop[j++] = (unsigned char)id->palette[i].b;
      prop[j++] = (unsigned char)id->palette[i].pixel;
    }
  memcpy (&prop[j], id->fast_rgb, (32*32*32));
  j += (32*32*32);
  XChangeProperty(id->x.disp, id->x.root, to_set, XA_CARDINAL, 8,
		  PropModeReplace, (unsigned char *)prop, j);
  free(prop);
}

void
_PaletteAlloc(ImlibData * id, int num, const int *cols)
{
  XColor              xcl;
  int                 colnum, i;
  int                 r, g, b;
  unsigned int        used[256], num_used, is_used, j;

  free(id->palette);
  id->palette = calloc(num, sizeof(ImlibColor));
  free(id->palette_orig);
  id->palette_orig = calloc(num, sizeof(ImlibColor));
  num_used = 0;
  colnum = 0;
  for (i = 0; i < num; i++)
    {
      r = cols[(i * 3) + 0];
      g = cols[(i * 3) + 1];
      b = cols[(i * 3) + 2];
      xcl.red = (unsigned short)((r << 8) | (r));
      xcl.green = (unsigned short)((g << 8) | (g));
      xcl.blue = (unsigned short)((b << 8) | (b));
      xcl.flags = DoRed | DoGreen | DoBlue;
      XAllocColor(id->x.disp, id->x.root_cmap, &xcl);
      is_used = 0;
      for (j = 0; j < num_used; j++)
	{
	  if (xcl.pixel == used[j])
	    {
	      is_used = 1;
	      j = num_used;
	    }
	}
      if (!is_used)
	{
	  id->palette[colnum].r = xcl.red >> 8;
	  id->palette[colnum].g = xcl.green >> 8;
	  id->palette[colnum].b = xcl.blue >> 8;
	  id->palette[colnum].pixel = xcl.pixel;
	  used[num_used++] = xcl.pixel;
	  colnum++;
	}
      else
	xcl.pixel = 0;
      id->palette_orig[i].r = r;
      id->palette_orig[i].g = g;
      id->palette_orig[i].b = b;
      id->palette_orig[i].pixel = xcl.pixel;
    }
  id->num_colors = colnum;
}

int
Imlib_load_colors(ImlibData * id, const char *file)
{
  FILE               *f;
  char                s[1024];
  int                 i;
  int                 pal[768];
  unsigned int        r, g, b;

#ifndef __EMX__
  f = fopen(file, "r");
#else
  if (*file =='/')
    f = fopen(__XOS2RedirRoot(file), "rt");
  else
    f = fopen(file, "rt");
#endif

  if (!f)
    {
      fprintf(stderr, "ImLib ERROR: Cannot find palette file %s\n", file);
      return 0;
    }
  i = 0;
  while (fgets(s, sizeof(s), f))
    {
      if (s[0] == '0')
	{
	  sscanf(s, "%x %x %x", &r, &g, &b);
	  if (r > 255)
	    r = 255;
	  if (g > 255)
	    g = 255;
	  if (b > 255)
	    b = 255;
	  pal[i++] = r;
	  pal[i++] = g;
	  pal[i++] = b;
	}
      if (i >= 768 - 2)
	break;
    }
  fclose(f);
  XGrabServer(id->x.disp);
  _PaletteAlloc(id, (i / 3), pal);
  if (!PaletteLUTGet(id))
    {
      free(id->fast_rgb);
      id->fast_rgb = malloc(sizeof(unsigned char) * 32 * 32 * 32);
      _fill_rgb_fast_tab (id);
      PaletteLUTSet(id);
    }
  XUngrabServer(id->x.disp);
  return 1;
}

static const int default_pal[] = {
  0x00, 0x00, 0x00,
  0xff, 0xff, 0xff,
  0xc0, 0xc0, 0xc0,
  0x00, 0x00, 0xff,
  0x99, 0x00, 0x66,
  0xff, 0x00, 0x00,
  0xff, 0xff, 0xcc,
  0x00, 0xff, 0x00,
  0x95, 0x95, 0x95,
  0x80, 0x00, 0x00,
  0x33, 0x33, 0x66,
  0x66, 0x66, 0xcc,
  0x80, 0x80, 0x80,
  0x99, 0x99, 0xff,
  0x00, 0x00, 0x80,
  0x22, 0x22, 0x22,
  0xff, 0xff, 0x00,
  0x80, 0x80, 0x00,
  0x00, 0x80, 0x80,
  0x42, 0x9a, 0xa7,
  0x00, 0xff, 0xff,
  0x00, 0x37, 0x3c,
  0x00, 0x80, 0x00,
  0xff, 0x66, 0x33,
  0xff, 0x66, 0xcc,
  0x1a, 0x5f, 0x67,
  0x00, 0x00, 0xee,
  0x55, 0x1a, 0x8b,
  0xe4, 0xe4, 0xe4,
  0x6a, 0x6a, 0x6a,
  0xa3, 0xa3, 0xa3,
  0x99, 0x99, 0x99,
  0x00, 0x00, 0x33,
  0x00, 0x00, 0x66,
  0x00, 0x00, 0x99,
  0x00, 0x00, 0xcc,
  0x00, 0x33, 0x00,
  0x00, 0x33, 0x33,
  0x00, 0x33, 0x66,
  0x00, 0x33, 0x99,
  0x00, 0x33, 0xcc,
  0x00, 0x33, 0xff,
  0x00, 0x66, 0x00,
  0x00, 0x66, 0x33,
  0x00, 0x66, 0x66,
  0x00, 0x66, 0x99,
  0x00, 0x66, 0xcc,
  0x00, 0x66, 0xff,
  0x00, 0x99, 0x00,
  0x00, 0x99, 0x33,
  0x00, 0x99, 0x66,
  0x00, 0x99, 0x99,
  0x00, 0x99, 0xcc,
  0x00, 0x99, 0xff,
  0x00, 0xcc, 0x00,
  0x00, 0xcc, 0x33,
  0x00, 0xcc, 0x66,
  0x00, 0xcc, 0x99,
  0x00, 0xcc, 0xcc,
  0x00, 0xcc, 0xff,
  0x00, 0xff, 0x33,
  0x00, 0xff, 0x66,
  0x00, 0xff, 0x99,
  0x00, 0xff, 0xcc,
  0x33, 0x00, 0x00,
  0x33, 0x00, 0x33,
  0x33, 0x00, 0x66,
  0x33, 0x00, 0x99,
  0x33, 0x00, 0xcc,
  0x33, 0x00, 0xff,
  0x33, 0x33, 0x00,
  0x33, 0x33, 0x33,
  0x33, 0x33, 0x99,
  0x33, 0x33, 0xcc,
  0x33, 0x33, 0xff,
  0x33, 0x66, 0x00,
  0x33, 0x66, 0x33,
  0x33, 0x66, 0x66,
  0x33, 0x66, 0x99,
  0x33, 0x66, 0xcc,
  0x33, 0x66, 0xff,
  0x33, 0x99, 0x00,
  0x33, 0x99, 0x33,
  0x33, 0x99, 0x66,
  0x33, 0x99, 0x99,
  0x33, 0x99, 0xcc,
  0x33, 0x99, 0xff,
  0x33, 0xcc, 0x00,
  0x33, 0xcc, 0x33,
  0x33, 0xcc, 0x66,
  0x33, 0xcc, 0x99,
  0x33, 0xcc, 0xcc,
  0x33, 0xcc, 0xff,
  0x33, 0xff, 0x00,
  0x33, 0xff, 0x33,
  0x33, 0xff, 0x66,
  0x33, 0xff, 0x99,
  0x33, 0xff, 0xcc,
  0x33, 0xff, 0xff,
  0x66, 0x00, 0x00,
  0x66, 0x00, 0x33,
  0x66, 0x00, 0x66,
  0x66, 0x00, 0x99,
  0x66, 0x00, 0xcc,
  0x66, 0x00, 0xff,
  0x66, 0x33, 0x00,
  0x66, 0x33, 0x33,
  0x66, 0x33, 0x66,
  0x66, 0x33, 0x99,
  0x66, 0x33, 0xcc,
  0x66, 0x33, 0xff,
  0x66, 0x66, 0x00,
  0x66, 0x66, 0x33,
  0x66, 0x66, 0x66,
  0x66, 0x66, 0x99,
  0x66, 0x66, 0xff,
  0x66, 0x99, 0x00,
  0x66, 0x99, 0x33,
  0x66, 0x99, 0x66,
  0x66, 0x99, 0x99,
  0x66, 0x99, 0xcc,
  0x66, 0x99, 0xff,
  0x66, 0xcc, 0x00,
  0x66, 0xcc, 0x33,
  0x66, 0xcc, 0x66,
  0x66, 0xcc, 0x99,
  0x66, 0xcc, 0xcc,
  0x66, 0xcc, 0xff,
  0x66, 0xff, 0x00,
  0x66, 0xff, 0x33,
  0x66, 0xff, 0x66,
  0x66, 0xff, 0x99,
  0x66, 0xff, 0xcc,
  0x66, 0xff, 0xff,
  0x99, 0x00, 0x00,
  0x99, 0x00, 0x33,
  0x99, 0x00, 0x99,
  0x99, 0x00, 0xcc,
  0x99, 0x00, 0xff,
  0x99, 0x33, 0x00,
  0x99, 0x33, 0x33,
  0x99, 0x33, 0x66,
  0x99, 0x33, 0x99,
  0x99, 0x33, 0xcc,
  0x99, 0x33, 0xff,
  0x99, 0x66, 0x00,
  0x99, 0x66, 0x33,
  0x99, 0x66, 0x66,
  0x99, 0x66, 0x99,
  0x99, 0x66, 0xcc,
  0x99, 0x66, 0xff,
  0x99, 0x99, 0x00,
  0x99, 0x99, 0x33,
  0x99, 0x99, 0x66,
  0x99, 0x99, 0xcc,
  0x99, 0xcc, 0x00,
  0x99, 0xcc, 0x33,
  0x99, 0xcc, 0x66,
  0x99, 0xcc, 0x99,
  0x99, 0xcc, 0xcc,
  0x99, 0xcc, 0xff,
  0x99, 0xff, 0x00,
  0x99, 0xff, 0x33,
  0x99, 0xff, 0x66,
  0x99, 0xff, 0x99,
  0x99, 0xff, 0xcc,
  0x99, 0xff, 0xff,
  0xcc, 0x00, 0x00,
  0xcc, 0x00, 0x33,
  0xcc, 0x00, 0x66,
  0xcc, 0x00, 0x99,
  0xcc, 0x00, 0xcc,
  0xcc, 0x00, 0xff,
  0xcc, 0x33, 0x00,
  0xcc, 0x33, 0x33,
  0xcc, 0x33, 0x66,
  0xcc, 0x33, 0x99,
  0xcc, 0x33, 0xcc,
  0xcc, 0x33, 0xff,
  0xcc, 0x66, 0x00,
  0xcc, 0x66, 0x33,
  0xcc, 0x66, 0x66,
  0xcc, 0x66, 0x99,
  0xcc, 0x66, 0xcc,
  0xcc, 0x66, 0xff,
  0xcc, 0x99, 0x00,
  0xcc, 0x99, 0x33,
  0xcc, 0x99, 0x66,
  0xcc, 0x99, 0x99,
  0xcc, 0x99, 0xcc,
  0xcc, 0x99, 0xff,
  0xcc, 0xcc, 0x00,
  0xcc, 0xcc, 0x33,
  0xcc, 0xcc, 0x66,
  0xcc, 0xcc, 0x99,
  0xcc, 0xcc, 0xcc,
  0xcc, 0xcc, 0xff,
  0xcc, 0xff, 0x00,
  0xcc, 0xff, 0x33,
  0xcc, 0xff, 0x66,
  0xcc, 0xff, 0x99,
  0xcc, 0xff, 0xcc,
  0xcc, 0xff, 0xff,
  0xff, 0x00, 0x33,
  0xff, 0x00, 0x66,
  0xff, 0x00, 0x99,
  0xff, 0x00, 0xcc,
  0xff, 0x00, 0xff,
  0xff, 0x33, 0x00,
  0xff, 0x33, 0x33,
  0xff, 0x33, 0x66,
  0xff, 0x33, 0x99,
  0xff, 0x33, 0xcc,
  0xff, 0x33, 0xff,
  0xff, 0x66, 0x00,
  0xff, 0x66, 0x66,
  0xff, 0x66, 0x99,
  0xff, 0x66, 0xff,
  0xff, 0x99, 0x00,
  0xff, 0x99, 0x33,
  0xff, 0x99, 0x66,
  0xff, 0x99, 0x99,
  0xff, 0x99, 0xcc,
  0xff, 0x99, 0xff,
  0xff, 0xcc, 0x00,
  0xff, 0xcc, 0x33,
  0xff, 0xcc, 0x66,
  0xff, 0xcc, 0x99,
  0xff, 0xcc, 0xcc,
  0xff, 0xcc, 0xff,
  0xff, 0xff, 0x33,
  0xff, 0xff, 0x66,
  0xff, 0xff, 0x99};

int
Imlib_load_default_colors(ImlibData * id)
{
  int                 i;
  const int          *pal = default_pal;
#ifdef DEBUG
  /*
   * WARNING: you cannot single step into the
   * XGrabServer()...XUngrabServer() part of the code in a debugger,
   * if your debugger/xterm/ide uses the same X11 display as the
   * debugged client.
   */
#endif

  i = 699;

  XGrabServer(id->x.disp);
  _PaletteAlloc(id, (i / 3), pal);
  if (!PaletteLUTGet(id))
    {
      free(id->fast_rgb);
      id->fast_rgb = malloc(sizeof(unsigned char) * 32 * 32 * 32);
      _fill_rgb_fast_tab (id);
      PaletteLUTSet(id);
    }
  XUngrabServer(id->x.disp);
#ifdef DEBUG
  /*
   * Add an XSync() here to flush the XUngrabServer() op to the X11 server,
   * so that we can continue debugging after the "return", or use a "next"
   * to step over this subroutine.
   */
  XSync(id->x.disp, 0);
#endif
  return 1;
}

void
Imlib_free_colors(ImlibData * id)
{
  int                 i;
  unsigned long       pixels[256];

  for (i = 0; i < id->num_colors; i++)
    pixels[i] = id->palette[i].pixel;
  XFreeColors(id->x.disp, id->x.root_cmap, pixels, id->num_colors, 0);
  id->num_colors = 0;
}
