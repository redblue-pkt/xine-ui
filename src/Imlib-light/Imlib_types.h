#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef _HAVE_STRING_H
#include <string.h>
#elif _HAVE_STRINGS_H
#include <strings.h>
#endif

typedef struct _ImlibBorder ImlibBorder;
typedef struct _ImlibColor ImlibColor;
typedef struct _ImlibColorModifier ImlibColorModifier;
typedef struct _ImlibImage ImlibImage;
typedef struct _xdata Xdata;
typedef struct _ImlibData ImlibData;

struct _ImlibBorder
  {
    int                 left, right;
    int                 top, bottom;
  };

struct _ImlibColor
  {
    int                 r, g, b;
    int                 pixel;
  };

struct _ImlibColorModifier
  {
    int                 gamma;
    int                 brightness;
    int                 contrast;
  };

struct _ImlibImage
  {
    int                 rgb_width, rgb_height;
    unsigned char      *rgb_data;
    unsigned char      *alpha_data;
    char               *filename;
/* the below information is private */
    int                 width, height;
    ImlibColor          shape_color;
    ImlibBorder         border;
    Pixmap              pixmap;
    Pixmap              shape_mask;
    char                cache;
    ImlibColorModifier  mod, rmod, gmod, bmod;
    unsigned char       rmap[256], gmap[256], bmap[256];
  };

struct _xdata
  {
    Display            *disp;
    int                 screen;
    Window              root;
    Visual             *visual; 
    int                 depth;
    int                 render_depth;
    Colormap            root_cmap;
    XImage             *last_xim;
    XImage             *last_sxim;
    Window              base_window;
    int                 byte_order, bit_order;
  };

struct _ImlibData
  {
    int                 num_colors;
    ImlibColor         *palette;
    ImlibColor         *palette_orig;
    unsigned char      *fast_rgb;
    int                *fast_err;
    int                *fast_erg;
    int                *fast_erb;
    int                 render_type;
    Xdata               x;
    int                 byte_order;
    struct _cache
      {
	char                on_image;
	int                 size_image;
	int                 num_image;
	int                 used_image;
	struct image_cache *image;
	char                on_pixmap;
	int                 size_pixmap;
	int                 num_pixmap;
	int                 used_pixmap;
	struct pixmap_cache *pixmap;
      }
    cache;
    char                fastrend;
    char                hiq;
    ImlibColorModifier  mod, rmod, gmod, bmod;
    unsigned char       rmap[256], gmap[256], bmap[256];
    char                fallback;
    char                ordered_dither;
  };

#define RT_PLAIN_PALETTE       0
#define RT_PLAIN_PALETTE_FAST  1
#define RT_DITHER_PALETTE      2
#define RT_DITHER_PALETTE_FAST 3
#define RT_PLAIN_TRUECOL       4
/* a special high-quality renderer for people with 15 and 16bpp that dithers */
#define RT_DITHER_TRUECOL      5
