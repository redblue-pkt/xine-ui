#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Imlib.h"
#include "Imlib_types.h"
#include "Imlib_private.h"

static const union {
  uint32_t word;
  uint8_t  little;
} endian_is = {
  .word = 1
};

typedef union {
  uint32_t w;
  struct {
    uint8_t r, g, b, a;
  } b;
} _rgba_t;

typedef struct _rend_s {
  _rgba_t  *(*get_row) (struct _rend_s *r, int y), *row;
  XImage   *xim;
  int       w, h, qual, fast;
  ImlibColor *pal;
  uint8_t  *tab_15_8;
  int       num_pal_entries;
  _rgba_t **yarray;
  int      *xarray;
  void     *err;
  uint8_t  *mod_r, *mod_g, *mod_b;
} _rend_t;

static _rgba_t *_get_row_plain (_rend_t *r, int y) {
  return r->yarray[y];
}

static _rgba_t *_get_row_scale (_rend_t *r, int y) {
  _rgba_t *b = r->yarray[y], *q = r->row;
  int x;
  for (x = 0; x < r->w; x++) {
    _rgba_t *p = b + r->xarray[x];
    *q++ = *p;
  }
  return r->row;
}

static _rgba_t *_get_row_mod (_rend_t *r, int y) {
  _rgba_t *b = r->yarray[y], *q = r->row;
  int x;
  for (x = 0; x < r->w; x++) {
    _rgba_t *p = b + x;
    q->b.r = r->mod_r[p->b.r];
    q->b.g = r->mod_g[p->b.g];
    q->b.b = r->mod_b[p->b.b];
    q->b.a = 0;
    q += 1;
  }
  return r->row;
}

static _rgba_t *_get_row_scale_mod (_rend_t *r, int y) {
  _rgba_t *b = r->yarray[y], *q = r->row;
  int x;
  for (x = 0; x < r->w; x++) {
    _rgba_t *p = b + r->xarray[x];
    q->b.r = r->mod_r[p->b.r];
    q->b.g = r->mod_g[p->b.g];
    q->b.b = r->mod_b[p->b.b];
    q->b.a = 0;
    q += 1;
  }
  return r->row;
}

typedef struct {
  int16_t r, g, b, dummy;
} _rgb_t;

static _rend_t *_rend_new (int w, int h) {
  size_t yarrsize = h * sizeof (_rgba_t *);
  size_t xarrsize = w * sizeof (int);
  size_t rowsize = w * sizeof (_rgba_t);
  size_t errsize = (w + 4) * sizeof (_rgb_t);
  _rend_t *r;
  uint8_t *m = malloc (sizeof (*r) + yarrsize + xarrsize + rowsize + errsize);
  if (!m)
    return NULL;
  r = (_rend_t *)m;
  m += sizeof (*r);
  r->yarray = (_rgba_t **)m;
  m += yarrsize;
  r->xarray = (int *)m;
  m += xarrsize;
  r->row = (_rgba_t *)m;
  m += rowsize;
  r->err = (void *)m;
  r->get_row = NULL;
  r->xim = NULL;
  r->w = w;
  r->h = h;
  r->qual = 2;
  r->fast = 0;
  r->pal = NULL;
  r->num_pal_entries = 0;
  r->tab_15_8 = NULL;
  r->mod_r = NULL;
  r->mod_g = NULL;
  r->mod_b = NULL;
  return r;
}
  
/* static const _rgba_t _rgb_mask = { .b = { .r = 255, .g = 255, .b = 255, .a = 0 }}; */
static const _rgba_t _rgb15_err_mask = { .b = { .r = 7, .g = 7, .b = 7, .a = 0 }};
static const _rgba_t _rgb16_err_mask = { .b = { .r = 7, .g = 3, .b = 7, .a = 0 }};

static void _rgba_to_rgb (_rgba_t *rgba, _rgb_t *rgb) {
  rgb->r = rgba->b.r;
  rgb->g = rgba->b.g;
  rgb->b = rgba->b.b;
}

static void _rgb_mul (_rgb_t *to, int m) {
  to->r *= m;
  to->g *= m;
  to->b *= m;
}

static void _rgb_rsh (_rgb_t *to, int shift) {
  to->r >>= shift;
  to->g >>= shift;
  to->b >>= shift;
}

static void _rgb_add (_rgb_t *to, _rgb_t *from) {
  to->r += from->r;
  to->g += from->g;
  to->b += from->b;
}

static void _rgb_sat_uint8 (_rgb_t *rgb) {
  if (rgb->r & ~0xff)
    rgb->r = ~(rgb->r >> (sizeof (rgb->r) * 8 - 1)) & 0xff;
  if (rgb->g & ~0xff)
    rgb->g = ~(rgb->g >> (sizeof (rgb->r) * 8 - 1)) & 0xff;
  if (rgb->b & ~0xff)
    rgb->b = ~(rgb->b >> (sizeof (rgb->r) * 8 - 1)) & 0xff;
}

static int _rgb_color_match_8 (_rend_t *r, _rgb_t *rgb) {
  int i, col = 0, db = 1 << (sizeof (db) * 8 - 2);
  for (i = 0; i < r->num_pal_entries; i++) {
    int d1, d2;
    d1 = rgb->r - r->pal[i].r;
    d2 = d1 * d1;
    d1 = rgb->g - r->pal[i].g;
    d2 += d1 * d1;
    d1 = rgb->b - r->pal[i].b;
    d2 += d1 * d1;
    if (d2 < db) {
      db = d2;
      col = i;
    }
  }
  rgb->r -= r->pal[col].r;
  rgb->g -= r->pal[col].g;
  rgb->b -= r->pal[col].b;
  return r->pal[col].pixel;
}


static void _rend_8 (_rend_t *r) {
  static const _rgb_t rgb_0 = {.r = 0, .g = 0, .b = 0, .dummy = 0};
  if (r->xim->bits_per_pixel != 8)
    r->fast = 0;
  switch ((r->qual << 1) | !!r->fast) {
    case 5: {
      int y, jmp = r->xim->bytes_per_line;
      uint8_t *img = (uint8_t *)r->xim->data;
      _rgb_t *err = r->err;
      /* Floyd-Steinberg, pendulum style for less top row artifacts.3
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y] = rgb_0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1] = rgb_0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          int indx;
          _rgb_t v, t;

          v = err[x + 3];
          _rgb_rsh (&v, 4);
          _rgba_to_rgb (p, &t);
          p += 1;
          _rgb_add (&v, &t);
          _rgb_sat_uint8 (&v);
          indx = _rgb_color_match_8 (r, &v);
          t = v;
          _rgb_mul (&t, 3);
          _rgb_add (err + x + 0, &t);
          err[x + 2] = t;
          t = v;
          _rgb_mul (&t, 5);
          _rgb_add (err + x + 4, &t);
          _rgb_add (err + x + 1, &t);
          *img++ = indx;
        }
        img += jmp;
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2] = rgb_0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          int indx;
          _rgb_t v, t;

          v = err[x + 1];
          _rgb_rsh (&v, 4);
          p -= 1;
          _rgba_to_rgb (p, &t);
          _rgb_add (&v, &t);
          _rgb_sat_uint8 (&v);
          indx = _rgb_color_match_8 (r, &v);
          t = v;
          _rgb_mul (&t, 3);
          err[x + 2] = t;
          _rgb_add (err + x + 4, &t);
          t = v;
          _rgb_mul (&t, 5);
          _rgb_add (err + x + 0, &t);
          _rgb_add (err + x + 3, &t);
          *--img = indx;
        }
        img += jmp;
        y += 1;
      }
    }
    break;
    case 3: {
      int y, jmp = r->xim->bytes_per_line - r->w;
      uint8_t *img = (uint8_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++) {
          int indx;
          _rgb_t v;
          _rgba_to_rgb (p, &v);
          indx = _rgb_color_match_8 (r, &v);
          *img++ = indx;
        }
        img += jmp;
      }
    }
    break;
    case 1: {
      int y, jmp = r->xim->bytes_per_line - r->w;
      uint8_t *img = (uint8_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          *img++ = r->tab_15_8[((uint32_t)p[0].b.r >> 3 << 10) | ((uint32_t)p[0].b.g >> 3 << 5) | (p[0].b.b >> 3)];
        img += jmp;
      }
    }
    break;
    case 4: {
      int y;
      _rgb_t *err = r->err;

      /* Floyd-Steinberg, pendulum style for less top row artifacts.
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y] = rgb_0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1] = rgb_0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          int indx;
          _rgb_t v, t;

          v = err[x + 3];
          _rgb_rsh (&v, 4);
          _rgba_to_rgb (p, &t);
          p += 1;
          _rgb_add (&v, &t);
          _rgb_sat_uint8 (&v);
          indx = _rgb_color_match_8 (r, &v);
          t = v;
          _rgb_mul (&t, 3);
          _rgb_add (err + x + 0, &t);
          err[x + 2] = t;
          t = v;
          _rgb_mul (&t, 5);
          _rgb_add (err + x + 4, &t);
          _rgb_add (err + x + 1, &t);
          XPutPixel (r->xim, x, y, indx);
        }
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2] = rgb_0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          int indx;
          _rgb_t v, t;

          v = err[x + 1];
          _rgb_rsh (&v, 4);
          p -= 1;
          _rgba_to_rgb (p, &t);
          _rgb_add (&v, &t);
          _rgb_sat_uint8 (&v);
          indx = _rgb_color_match_8 (r, &v);
          t = v;
          _rgb_mul (&t, 3);
          err[x + 2] = t;
          _rgb_add (err + x + 4, &t);
          t = v;
          _rgb_mul (&t, 5);
          _rgb_add (err + x + 0, &t);
          _rgb_add (err + x + 3, &t);
          XPutPixel (r->xim, x, y, indx);
        }
        y += 1;
      }
    }
    break;
    case 2: {
      int y;

      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++) {
          int indx;
          _rgb_t v;
          _rgba_to_rgb (p, &v);
          indx = _rgb_color_match_8 (r, &v);
          XPutPixel (r->xim, x, y, indx);
        }
      }
    }
    break;
    default: {
      int y;
      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          XPutPixel (r->xim, x, y,
            r->tab_15_8[((uint32_t)p[0].b.r >> 3 << 10) | ((uint32_t)p[0].b.g >> 3 << 5) | (p[0].b.b >> 3)]);
      }
    }
  }
}

static const uint8_t _tab_8_to_5[268] = {
  /* ((index - 4) * 31 + 127) / 255 */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
   1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3,
   3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
   5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7,
   7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
   9, 9, 9,10,10,10,10,10,10,10,10,11,11,11,11,11,
  11,11,11,12,12,12,12,12,12,12,12,13,13,13,13,13,
  13,13,13,13,14,14,14,14,14,14,14,14,15,15,15,15,
  15,15,15,15,16,16,16,16,16,16,16,16,17,17,17,17,
  17,17,17,17,18,18,18,18,18,18,18,18,18,19,19,19,
  19,19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,
  21,21,21,21,21,22,22,22,22,22,22,22,22,22,23,23,
  23,23,23,23,23,23,24,24,24,24,24,24,24,24,25,25,
  25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,
  27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,29,
  29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,31,
  31,31,31,31,31,31,31,31
};

static const uint8_t _tab_8_to_6[260] = {
  /* ((index - 2) * 63 + 127) / 255 */
   0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
   3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7,
   7, 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,
  11,12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,
  15,16,16,16,16,17,17,17,17,18,18,18,18,19,19,19,
  19,20,20,20,20,21,21,21,21,21,22,22,22,22,23,23,
  23,23,24,24,24,24,25,25,25,25,26,26,26,26,27,27,
  27,27,28,28,28,28,29,29,29,29,30,30,30,30,31,31,
  31,31,32,32,32,32,33,33,33,33,34,34,34,34,35,35,
  35,35,36,36,36,36,37,37,37,37,38,38,38,38,39,39,
  39,39,40,40,40,40,41,41,41,41,42,42,42,42,42,43,
  43,43,43,44,44,44,44,45,45,45,45,46,46,46,46,47,
  47,47,47,48,48,48,48,49,49,49,49,50,50,50,50,51,
  51,51,51,52,52,52,52,53,53,53,53,54,54,54,54,55,
  55,55,55,56,56,56,56,57,57,57,57,58,58,58,58,59,
  59,59,59,60,60,60,60,61,61,61,61,62,62,62,62,63,
  63,63,63,63
};

static uint32_t _add_4_uint8_sat (uint32_t large, uint32_t small) {
  uint32_t res, hi, over;
  hi = large & 0x80808080;
  res = (large & 0x7f7f7f7f) + small;
  over = res & hi;
  res |= hi;
  over |= over >> 1;
  over |= over >> 2;
  over |= over >> 4;
  return res | over;
}
  
#define CLIP_UINT8(_v) do { if (_v & ~0xff) _v = ~(_v >> (sizeof (_v) * 8 - 1)) & 0xff; } while (0)

int
Imlib_best_color_match(ImlibData * id, int *r, int *g, int *b)
{
  int                 dr, dg, db;
  int                 col;

  col = 0;
  if (!id)
    {
      fprintf(stderr, "ImLib ERROR: No ImlibData initialised\n");
      return -1;
    }
  if ((id->render_type == RT_PLAIN_TRUECOL) ||
      (id->render_type == RT_DITHER_TRUECOL))
    {
      dr = *r;
      dg = *g;
      db = *b;
      switch (id->x.depth)
	{
	case 15:
	  *r = dr - (dr & 0xf8);
	  *g = dg - (dg & 0xf8);
	  *b = db - (db & 0xf8);
	  return ((dr & 0xf8) << 7) | ((dg & 0xf8) << 2) | ((db & 0xf8) >> 3);
	  break;
	case 16:
	  *r = dr - (dr & 0xf8);
	  *g = dg - (dg & 0xfc);
	  *b = db - (db & 0xf8);
	  return ((dr & 0xf8) << 8) | ((dg & 0xfc) << 3) | ((db & 0xf8) >> 3);
	  break;
	case 24:
	case 32:
	  *r = 0;
	  *g = 0;
	  *b = 0;
	  switch (id->byte_order)
	    {
	    case BYTE_ORD_24_RGB:
	      return ((dr & 0xff) << 16) | ((dg & 0xff) << 8) | (db & 0xff);
	      break;
	    case BYTE_ORD_24_RBG:
	      return ((dr & 0xff) << 16) | ((db & 0xff) << 8) | (dg & 0xff);
	      break;
	    case BYTE_ORD_24_BRG:
	      return ((db & 0xff) << 16) | ((dr & 0xff) << 8) | (dg & 0xff);
	      break;
	    case BYTE_ORD_24_BGR:
	      return ((db & 0xff) << 16) | ((dg & 0xff) << 8) | (dr & 0xff);
	      break;
	    case BYTE_ORD_24_GRB:
	      return ((dg & 0xff) << 16) | ((dr & 0xff) << 8) | (db & 0xff);
	      break;
	    case BYTE_ORD_24_GBR:
	      return ((dg & 0xff) << 16) | ((db & 0xff) << 8) | (dr & 0xff);
	      break;
	    default:
	      return 0;
	      break;
	    }
	  break;
	default:
	  return 0;
	  break;
	}
      return 0;
    }
  {
    int i, col = 0, db = 1 << (sizeof (db) * 8 - 2);
    for (i = 0; i < id->num_colors; i++) {
      int d1, d2;
      d1 = *r - id->palette[i].r;
      d2 = d1 * d1;
      d1 = *g - id->palette[i].g;
      d2 += d1 * d1;
      d1 = *b - id->palette[i].b;
      d2 += d1 * d1;
      if (d2 < db) {
        db = d2;
        col = i;
      }
    }
    *r -= id->palette[col].r;
    *g -= id->palette[col].g;
    *b -= id->palette[col].b;
    col = id->palette[col].pixel;
    return col;
  }
}

void _fill_rgb_fast_tab (ImlibData *id) {
  uint8_t *q;
  uint32_t r;
  if ((id->x.depth > 8) || id->hiq || !id->fast_rgb)
    return;
  q = id->fast_rgb;
  for (r = 0; r < 256; r += 8) {
    uint32_t g;
    r = (r & 248) | (r >> 5);
    for (g = 0; g < 256; g += 8) {
      uint32_t b;
      g = (g & 248) | (g >> 5);
      for (b = 0; b < 256; b += 8) {
        int i, col = 0, db = 1 << (sizeof (db) * 8 - 2);
        b = (b & 248) | (b >> 5);
        for (i = 0; i < id->num_colors; i++) {
          int d1, d2;
          d1 = r - id->palette[i].r;
          d2 = d1 * d1;
          d1 = g - id->palette[i].g;
          d2 += d1 * d1;
          d1 = b - id->palette[i].b;
          d2 += d1 * d1;
          if (d2 < db) {
            db = d2;
            col = i;
          }
        }
        *q++ = id->palette[col].pixel;
      }
    }
  }
}

static void _rend_15 (_rend_t *r) {
  if (r->xim->bits_per_pixel != 16)
    r->fast = 0;
  switch ((r->qual << 1) | !!r->fast) {
    case 5: {
      int y, jmp;
      unsigned short *img;
      _rgba_t *err = r->err;

      jmp = r->xim->bytes_per_line >> 1;
      img = (unsigned short *)r->xim->data;
      /* Floyd-Steinberg, pendulum style for less top row artifacts.
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y].w = 0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1].w = 0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          _rgba_t v, t;

          v.w = (err[x + 3].w >> 4) & 0x0f0f0f0f;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          p += 1;
          t.w = (v.w & _rgb15_err_mask.w) * 3;
          err[x + 0].w += t.w;
          err[x + 2].w  = t.w;
          t.w = (v.w & _rgb15_err_mask.w) * 5;
          err[x + 4].w += t.w;
          err[x + 1].w  += t.w;
          *img++ = ((uint32_t)v.b.r >> 3 << 10) | ((uint32_t)v.b.g >> 3 << 5) | (v.b.b >> 3);
        }
        img += jmp;
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2].w = 0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          _rgba_t v, t;

          v.w = (err[x + 1].w >> 4) & 0x0f0f0f0f;
          p -= 1;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          t.w = (v.w & _rgb15_err_mask.w) * 3;
          err[x + 2].w  = t.w;
          err[x + 4].w += t.w;
          t.w = (v.w & _rgb15_err_mask.w) * 5;
          err[x + 0].w += t.w;
          err[x + 3].w  += t.w;
          *--img = ((uint32_t)v.b.r >> 3 << 10) | ((uint32_t)v.b.g >> 3 << 5) | (v.b.b >> 3);
        }
        img += jmp;
        y += 1;
      }
    }
    break;
    case 3: {
      int y, val, jmp;
      unsigned short *img;

      jmp = (r->xim->bytes_per_line >> 1) - r->w;
      img = (unsigned short *)r->xim->data;
      /* Beyer matrix 2x2. */
      y = 0;
      while (y < r->h) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          *img++ = val;
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 4] << 10;
          val += (uint32_t)_tab_8_to_5[p[1].b.g + 4] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 4] <<  0;
          *img++ = val;
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          *img++ = val;
        }
        img += jmp;
        y += 1;
        if (y >= r->h)
          break;
        p = r->get_row (r, y);
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 6] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          *img++ = val;
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 2] << 10;
          val += (uint32_t)_tab_8_to_5[p[1].b.g + 2] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 2] <<  0;
          *img++ = val;
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 6] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          *img++ = val;
        }
        img += jmp;
        y += 1;
      }
    }
    break;
    case 1: {
      int y, jmp;
      unsigned short *img;

      jmp = (r->xim->bytes_per_line >> 1) - r->w;
      img = (unsigned short *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          *img++ = ((uint32_t)p[0].b.r >> 3 << 10) | ((uint32_t)p[0].b.g >> 3 << 5) | (p[0].b.b >> 3);
        img += jmp;
      }
    }
    break;
    case 4: {
      int y;
      _rgba_t *err = r->err;

      /* Floyd-Steinberg, pendulum style for less top row artifacts.
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y].w = 0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1].w = 0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          _rgba_t v, t;

          v.w = (err[x + 3].w >> 4) & 0x0f0f0f0f;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          p += 1;
          t.w = (v.w & _rgb15_err_mask.w) * 3;
          err[x + 0].w += t.w;
          err[x + 2].w  = t.w;
          t.w = (v.w & _rgb15_err_mask.w) * 5;
          err[x + 4].w += t.w;
          err[x + 1].w  += t.w;
          XPutPixel (r->xim, x, y, ((uint32_t)v.b.r >> 3 << 10) | ((uint32_t)v.b.g >> 3 << 5) | (v.b.b >> 3));
        }
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2].w = 0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          _rgba_t v, t;

          v.w = (err[x + 1].w >> 4) & 0x0f0f0f0f;
          p -= 1;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          t.w = (v.w & _rgb15_err_mask.w) * 3;
          err[x + 2].w  = t.w;
          err[x + 4].w += t.w;
          t.w = (v.w & _rgb15_err_mask.w) * 5;
          err[x + 0].w += t.w;
          err[x + 3].w  += t.w;
          XPutPixel (r->xim, x, y, ((uint32_t)v.b.r >> 3 << 10) | ((uint32_t)v.b.g >> 3 << 5) | (v.b.b >> 3));
        }
        y += 1;
      }
    }
    break;
    case 2: {
      int y, val;
      /* Beyer matrix 2x2. */
      y = 0;
      while (y < r->h) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          XPutPixel (r->xim, x, y, val);
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 4] << 10;
          val += (uint32_t)_tab_8_to_5[p[1].b.g + 4] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 4] <<  0;
          XPutPixel (r->xim, x + 1, y, val);
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          XPutPixel (r->xim, x, y, val);
        }
        y += 1;
        if (y >= r->h)
          break;
        p = r->get_row (r, y);
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 6] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          XPutPixel (r->xim, x, y, val);
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 2] << 10;
          val += (uint32_t)_tab_8_to_5[p[1].b.g + 2] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 2] <<  0;
          XPutPixel (r->xim, x + 1, y, val);
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 10;
          val += (uint32_t)_tab_8_to_5[p[0].b.g + 6] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          XPutPixel (r->xim, x, y, val);
        }
        y += 1;
      }
    }
    break;
    default: {
      int y;

      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.r >> 3 << 10) | ((uint32_t)p[0].b.g >> 3 <<  5) | (p[0].b.b >> 3));
      }
    }
  }
}

static void _rend_16 (_rend_t *r) {
  if (r->xim->bits_per_pixel != 16)
    r->fast = 0;
  switch ((r->qual << 1) | !!r->fast) {
    case 5: {
      int y, jmp;
      unsigned short *img;
      _rgba_t *err = r->err;

      jmp = r->xim->bytes_per_line >> 1;
      img = (unsigned short *)r->xim->data;
      /* Floyd-Steinberg, pendulum style for less top row artifacts.
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y].w = 0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1].w = 0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          _rgba_t v, t;

          v.w = (err[x + 3].w >> 4) & 0x0f0f0f0f;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          p += 1;
          t.w = (v.w & _rgb16_err_mask.w) * 3;
          err[x + 0].w += t.w;
          err[x + 2].w  = t.w;
          t.w = (v.w & _rgb16_err_mask.w) * 5;
          err[x + 4].w += t.w;
          err[x + 1].w  += t.w;
          *img++ = ((uint32_t)v.b.r >> 3 << 11) | ((uint32_t)v.b.g >> 2 << 5) | (v.b.b >> 3);
        }
        img += jmp;
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2].w = 0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          _rgba_t v, t;

          v.w = (err[x + 1].w >> 4) & 0x0f0f0f0f;
          p -= 1;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          t.w = (v.w & _rgb16_err_mask.w) * 3;
          err[x + 2].w  = t.w;
          err[x + 4].w += t.w;
          t.w = (v.w & _rgb16_err_mask.w) * 5;
          err[x + 0].w += t.w;
          err[x + 3].w  += t.w;
          *--img = ((uint32_t)v.b.r >> 3 << 11) | ((uint32_t)v.b.g >> 2 << 5) | (v.b.b >> 3);
        }
        img += jmp;
        y += 1;
      }
    }
    break;
    case 3: {
      int y, val, jmp;
      unsigned short *img;

      jmp = (r->xim->bytes_per_line >> 1) - r->w;
      img = (unsigned short *)r->xim->data;
      /* Beyer matrix 2x2. */
      y = 0;
      while (y < r->h) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          *img++ = val;
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 4] << 11;
          val += (uint32_t)_tab_8_to_6[p[1].b.g + 2] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 4] <<  0;
          *img++ = val;
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          *img++ = val;
        }
        img += jmp;
        y += 1;
        if (y >= r->h)
          break;
        p = r->get_row (r, y);
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 3] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          *img++ = val;
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 2] << 11;
          val += (uint32_t)_tab_8_to_6[p[1].b.g + 1] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 2] <<  0;
          *img++ = val;
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 3] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          *img++ = val;
        }
        img += jmp;
        y += 1;
      }
    }
    break;
    case 1: {
      int y, jmp;
      unsigned short *img;

      jmp = (r->xim->bytes_per_line >> 1) - r->w;
      img = (unsigned short *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          *img++ = ((uint32_t)p[0].b.r >> 3 << 11) | ((uint32_t)p[0].b.g >> 2 << 5) | (p[0].b.b >> 3);
        img += jmp;
      }
    }
    break;
    case 4: {
      int y;
      _rgba_t *err = r->err;

      /* Floyd-Steinberg, pendulum style for less top row artifacts.
       * error components are 0 <= e < 16 * 7 and can thus be vectorized. */
      for (y = 0; y < r->w + 4; y++)
        err[y].w = 0;

      y = 0;
      while (y < r->h) {
        _rgba_t *p;
        int x;

        err[1].w = 0;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          _rgba_t v, t;

          v.w = (err[x + 3].w >> 4) & 0x0f0f0f0f;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          p += 1;
          t.w = (v.w & _rgb16_err_mask.w) * 3;
          err[x + 0].w += t.w;
          err[x + 2].w  = t.w;
          t.w = (v.w & _rgb16_err_mask.w) * 5;
          err[x + 4].w += t.w;
          err[x + 1].w  += t.w;
          XPutPixel (r->xim, x, y, ((uint32_t)v.b.r >> 3 << 11) | ((uint32_t)v.b.g >> 2 << 5) | (v.b.b >> 3));
        }
        y += 1;
        if (y >= r->h)
          break;

        err[r->w + 2].w = 0;
        p = r->get_row (r, y) + r->w;
        for (x = r->w - 1; x >= 0; x--) {
          _rgba_t v, t;

          v.w = (err[x + 1].w >> 4) & 0x0f0f0f0f;
          p -= 1;
          v.w = _add_4_uint8_sat (p[0].w, v.w);
          t.w = (v.w & _rgb16_err_mask.w) * 3;
          err[x + 2].w  = t.w;
          err[x + 4].w += t.w;
          t.w = (v.w & _rgb16_err_mask.w) * 5;
          err[x + 0].w += t.w;
          err[x + 3].w  += t.w;
          XPutPixel (r->xim, x, y, ((uint32_t)v.b.r >> 3 << 11) | ((uint32_t)v.b.g >> 2 << 5) | (v.b.b >> 3));
        }
        y += 1;
      }
    }
    break;
    case 2: {
      int y, val;
      /* Beyer matrix 2x2. */
      y = 0;
      while (y < r->h) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          XPutPixel (r->xim, x, y, val);
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 4] << 11;
          val += (uint32_t)_tab_8_to_6[p[1].b.g + 2] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 4] <<  0;
          XPutPixel (r->xim, x + 1, y, val);
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 0] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 0] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 0] <<  0;
          XPutPixel (r->xim, x, y, val);
        }
        y += 1;
        if (y >= r->h)
          break;
        p = r->get_row (r, y);
        for (x = 0; x < r->w - 1; x += 2) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 3] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          XPutPixel (r->xim, x, y, val);
          val  = (uint32_t)_tab_8_to_5[p[1].b.r + 2] << 11;
          val += (uint32_t)_tab_8_to_6[p[1].b.g + 1] <<  5;
          val += (uint32_t)_tab_8_to_5[p[1].b.b + 2] <<  0;
          XPutPixel (r->xim, x + 1, y, val);
          p += 2;
        }
        if (x < r->w) {
          val  = (uint32_t)_tab_8_to_5[p[0].b.r + 6] << 11;
          val += (uint32_t)_tab_8_to_6[p[0].b.g + 3] <<  5;
          val += (uint32_t)_tab_8_to_5[p[0].b.b + 6] <<  0;
          XPutPixel (r->xim, x, y, val);
        }
        y += 1;
      }
    }
    break;
    default: {
      int y;

      for (y = 0; y < r->h; y++) {
        _rgba_t *p = r->get_row (r, y);
        int x;
        for (x = 0; x < r->w; p++, x++)
          XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.r >> 3 << 11) | ((uint32_t)p[0].b.g >> 2 <<  5) | (p[0].b.b >> 3));
      }
    }
  }
}

static void _rend_24 (_rend_t *r) {
  /* OK folks. Probe the behaviour of XPutPixel (), then auto fast the few coommon modes. */
  uint32_t mode1 = (0x11111111 & r->xim->red_mask)
                 | (0x22222222 & r->xim->green_mask)
                 | (0x33333333 & r->xim->blue_mask);
  uint32_t mode2 = (0x44444444 & r->xim->red_mask)
                 | (0x55555555 & r->xim->green_mask)
                 | (0x66666666 & r->xim->blue_mask);
  memset (r->xim->data, 0, 8);
  XPutPixel (r->xim, 0, 0, mode1);
  XPutPixel (r->xim, 1, 0, mode2);
  if (r->xim->bits_per_pixel == 32) {
    static const _rgba_t testf = {.b = {.r = 0, .g = 0x11, .b = 0x22, .a = 0x33}};
    static const _rgba_t testr = {.b = {.r = 0x33, .g = 0x22, .b = 0x11, .a = 0}};
    _rgba_t *p = (_rgba_t *)r->xim->data;
    mode2 = p[0].w;
    if (mode2 == testf.w) {
      int y, jmp = (r->xim->bytes_per_line >> 2) - r->w;
      uint32_t *q = (uint32_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        int x;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          q[0] = p[0].w >> 8;
          p += 1;
          q += 1;
        }
        q += jmp;
      }
      return;
    } else if (mode2 == testr.w) {
      int y, jmp = (r->xim->bytes_per_line >> 2) - r->w;
      uint32_t *q = (uint32_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        int x;
        p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          uint32_t v = p[0].w;
          v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24);
          q[0] = v >> 8;
          p += 1;
          q += 1;
        }
        q += jmp;
      }
      return;
    }
  } else if (r->xim->bits_per_pixel == 24) {
    if (!memcmp (r->xim->data, "\x11\x22\x33\x44", 4)) {
      int y, jmp = r->xim->bytes_per_line - 3 * r->w;
      uint8_t *q = (uint8_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        int x;
        _rgba_t *p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          q[0] = p[0].b.r;
          q[1] = p[0].b.g;
          q[2] = p[0].b.b;
          p += 1;
          q += 3;
        }
        q += jmp;
      }
      return;
    } else if (!memcmp (r->xim->data, "\x33\x22\x11\x66", 4)) {
      int y, jmp = r->xim->bytes_per_line - 3 * r->w;
      uint8_t *q = (uint8_t *)r->xim->data;
      for (y = 0; y < r->h; y++) {
        int x;
        _rgba_t *p = r->get_row (r, y);
        for (x = 0; x < r->w; x++) {
          q[0] = p[0].b.b;
          q[1] = p[0].b.g;
          q[2] = p[0].b.r;
          p += 1;
          q += 3;
        }
        q += jmp;
      }
      return;
    }
  }
  if (mode1 == 0x112233) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.r << 16) | ((uint32_t)p[0].b.g << 8) | p[0].b.b);
        p += 1;
      }
    }
  } else if (mode1 == 0x332211) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.b << 16) | ((uint32_t)p[0].b.g << 8) | p[0].b.r);
        p += 1;
      }
    }
  } else if (mode1 == 0x331122) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.b << 16) | ((uint32_t)p[0].b.r << 8) | p[0].b.g);
        p += 1;
      }
    }
  } else if (mode1 == 0x113322) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.r << 16) | ((uint32_t)p[0].b.b << 8) | p[0].b.g);
        p += 1;
      }
    }
  } else if (mode1 == 0x221133) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.g << 16) | ((uint32_t)p[0].b.r << 8) | p[0].b.b);
        p += 1;
      }
    }
  } else if (mode1 == 0x223311) {
    int y;
    for (y = 0; y < r->h; y++) {
      int x;
      _rgba_t *p = r->get_row (r, y);
      for (x = 0; x < r->w; x++) {
        XPutPixel (r->xim, x, y, ((uint32_t)p[0].b.g << 16) | ((uint32_t)p[0].b.b << 8) | p[0].b.r);
        p += 1;
      }
    }
  }
}

static void _imlib_fill_mask (_rgba_t **yarray, int *xarray, XImage *mask, int w, int h) {
  int y;
  if (!mask)
    return;
  if ((mask->format == 2) && (mask->bits_per_pixel == 1)) {
    uint8_t *q = (uint8_t *)mask->data;
    int skip = mask->bytes_per_line - ((w + 7) >> 3);
    int stop = w & ~7;
    if (mask->bitmap_bit_order == 0) {
      for (y = 0; y < h; y++) {
        const _rgba_t *p = yarray[y];
        int x;
        for (x = 0; x < stop; x += 8) {
          *q++ = ((p[xarray[x]    ].b.a & 0x80) >> 7)
               | ((p[xarray[x + 1]].b.a & 0x80) >> 6)
               | ((p[xarray[x + 2]].b.a & 0x80) >> 5)
               | ((p[xarray[x + 3]].b.a & 0x80) >> 4)
               | ((p[xarray[x + 4]].b.a & 0x80) >> 3)
               | ((p[xarray[x + 5]].b.a & 0x80) >> 2)
               | ((p[xarray[x + 6]].b.a & 0x80) >> 1)
               | ((p[xarray[x + 7]].b.a & 0x80) >> 0);
        }
        if (x < w) {
          do {
            *q  = ((p[xarray[x]].b.a & 0x80) >> 7);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 6);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 5);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 4);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 3);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 2);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 1);
          } while (0);
          q++;
        }
        q += skip;
      }
    } else {
      for (y = 0; y < h; y++) {
        const _rgba_t *p = yarray[y];
        int x;
        for (x = 0; x < stop; x += 8) {
          *q++ = ((p[xarray[x]    ].b.a & 0x80) >> 0)
               | ((p[xarray[x + 1]].b.a & 0x80) >> 1)
               | ((p[xarray[x + 2]].b.a & 0x80) >> 2)
               | ((p[xarray[x + 3]].b.a & 0x80) >> 3)
               | ((p[xarray[x + 4]].b.a & 0x80) >> 4)
               | ((p[xarray[x + 5]].b.a & 0x80) >> 5)
               | ((p[xarray[x + 6]].b.a & 0x80) >> 6)
               | ((p[xarray[x + 7]].b.a & 0x80) >> 7);
        }
        if (x < w) {
          do {
            *q  = ((p[xarray[x]].b.a & 0x80) >> 0);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 1);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 2);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 3);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 4);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 5);
            if (++x >= w) break;
            *q |= ((p[xarray[x]].b.a & 0x80) >> 6);
          } while (0);
          q++;
        }
        q += skip;
      }
    }
    return;
  }
  for (y = 0; y < h; y++) {
    const _rgba_t *p = yarray[y];
    int x;
    for (x = 0; x < w; x++) {
      XPutPixel (mask, x, y, p[xarray[x]].b.a >> 7);
    }
  }
}

int
Imlib_render(ImlibData * id, ImlibImage * im, int w, int h)
{
  _rend_t            *r;
  XImage             *xim, *sxim;
  static              Display *pd = NULL;
  static GC           tgc = 0, stgc = 0;
  XGCValues           gcv;
  unsigned char      *tmp, *stmp, **yarray, *ptr22;
  int                 w4, x, inc, pos, *xarray, bpp;
  Pixmap              pmap, mask;
  int                 shared_pixmap, shared_image;
#ifdef HAVE_SHM
  int                 huge;
#endif

  if (!pd)
    pd = id->x.disp;
  if (tgc)
    {
      if (id->x.disp != pd)
	{
	  XFreeGC(pd, tgc);
	  tgc = 0;
	}
    }
  if (stgc)
    {
      if (id->x.disp != pd)
	{
	  XFreeGC(pd, stgc);
	  stgc = 0;
	}
    }
  pd = id->x.disp;
  
  sxim = NULL;
  xim = NULL;
  tmp = NULL;
  stmp = NULL;
  pmap = 0;
  mask = 0;
  inc = 0;
  if (!im)
    return 0;
  if (w <= 0)
    return 0;
  if (h <= 0)
    return 0;
  gcv.graphics_exposures = False;

/* look for the pixmap in cache first */
  if (id->cache.on_pixmap)
    {
      pmap = 0;
      find_pixmap(id, im, w, h, &pmap, &mask);
      if (pmap)
	{
	  im->width = w;
	  im->height = h;
	  im->pixmap = pmap;
	  if (mask)
	    im->shape_mask = mask;
	  else
	    im->shape_mask = 0;
	  return 1;
	}
    }
  if (im->pixmap)
    free_pixmappmap(id, im->pixmap);
  im->pixmap = 0;
  im->shape_mask = 0;

  r = _rend_new (w, h);
  if (!r)
    return 0;
  xarray = r->xarray;
  yarray = (uint8_t **)r->yarray;
  if ((im->mod.gamma == 256) && (im->mod.brightness == 256) && (im->mod.contrast == 256) &&
    (im->rmod.gamma == 256) && (im->rmod.brightness == 256) && (im->rmod.contrast == 256) &&
    (im->gmod.gamma == 256) && (im->gmod.brightness == 256) && (im->gmod.contrast == 256) &&
    (im->bmod.gamma == 256) && (im->bmod.brightness == 256) && (im->bmod.contrast == 256)) {
    r->get_row = im->rgb_width == w ? _get_row_plain : _get_row_scale;
  } else {
    r->get_row = im->rgb_width == w ? _get_row_mod : _get_row_scale_mod;
  }
  r->mod_r = im->rmap;
  r->mod_g = im->gmap;
  r->mod_b = im->bmap;
  r->pal = id->palette;
  r->num_pal_entries = id->num_colors;
  r->tab_15_8 = id->fast_rgb;
  r->qual = id->hiq ? (id->ordered_dither ? 1 : 2) : 0;
  r->fast = id->fastrend;

/* setup stuff */
  if (id->x.depth <= 8)
    bpp = 1;
  else if (id->x.depth <= 16)
    bpp = 2;
  else if (id->x.depth <= 24)
    bpp = 3;
  else
    bpp = 4;

#ifdef HAVE_SHM
  huge = 0;
  if ((id->max_shm) && ((bpp * w * h) > id->max_shm))
    huge = 1;
#endif
  im->width = w;
  im->height = h;

/* setup pointers to point right */
  w4 = im->rgb_width * 4;
  ptr22 = im->rgb_data;

/* setup coord-mapping array (specially for border scaling) */
  {
    int                 l, r, m;

    if (w < im->border.left + im->border.right)
      {
	l = w >> 1;
	r = w - l;
	m = 0;
      }
    else
      {
	l = im->border.left;
	r = im->border.right;
	m = w - l - r;
      }
    if (m > 0)
      inc = ((im->rgb_width - im->border.left - im->border.right) << 16) / m;
    pos = 0;
    if (l)
      for (x = 0; x < l; x++)
	{
	  xarray[x] = pos >> 16;
	  pos += 0x10000;
	}
    if (m)
      {
	for (x = l; x < l + m; x++)
	  {
	    xarray[x] = pos >> 16;
	    pos += inc;
	  }
      }
    pos = (im->rgb_width - r) << 16;
    for (x = w - r; x < w; x++)
      {
	xarray[x] = pos >> 16;
	pos += 0x10000;
      }

    if (h < im->border.top + im->border.bottom)
      {
	l = h >> 1;
	r = h - l;
	m = 0;
      }
    else
      {
	l = im->border.top;
	r = im->border.bottom;
	m = h - l - r;
      }
    if (m > 0)
      inc = ((im->rgb_height - im->border.top - im->border.bottom) << 16) / m;
    pos = 0;
    for (x = 0; x < l; x++)
      {
	yarray[x] = ptr22 + ((pos >> 16) * w4);
	pos += 0x10000;
      }
    if (m)
      {
	for (x = l; x < l + m; x++)
	  {
	    yarray[x] = ptr22 + ((pos >> 16) * w4);
	    pos += inc;
	  }
      }
    pos = (im->rgb_height - r) << 16;
    for (x = h - r; x < h; x++)
      {
	yarray[x] = ptr22 + ((pos >> 16) * w4);
	pos += 0x10000;
      }
  }

/* work out if we should use shared pixmap. images etc */
  shared_pixmap = 0;
  shared_image = 0;
#ifdef HAVE_SHM
  if ((id->x.shmp) && (id->x.shm) && (!huge))
    {
#if defined(__alpha__)
      shared_pixmap = 0;
      shared_image = 1;
#else
      shared_pixmap = 1;
      shared_image = 0;
#endif
    }
  else if ((id->x.shm) && (!huge))
    {
      shared_pixmap = 0;
      shared_image = 1;
    }
  else
    {
      shared_pixmap = 0;
      shared_image = 0;
    }
#endif

/* init images and pixmaps */
#ifdef HAVE_SHM
  do {
    if (!shared_pixmap)
      break;
    xim = XShmCreateImage (id->x.disp, id->x.visual, id->x.depth, ZPixmap, NULL, &id->x.last_shminfo, w, h);
    if (!xim) {
      fprintf (stderr, "IMLIB ERROR: Mit-SHM can't create XImage for Shared Pixmap Wrapper\n");
      fprintf (stderr, "             Falling back on Shared XImages\n");
      shared_pixmap = 0;
      shared_image = 1;
      break;
    }
    id->x.last_shminfo.shmid = shmget (IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0777);
    if (id->x.last_shminfo.shmid == -1) {
      fprintf (stderr, "IMLIB ERROR: SHM can't get SHM Identifier for Shared Pixmap Wrapper\n");
      fprintf (stderr, "             Falling back on Shared XImages\n");
      XDestroyImage (xim);
      shared_pixmap = 0;
      shared_image = 1;
      break;
    }
    id->x.last_shminfo.shmaddr = xim->data = shmat (id->x.last_shminfo.shmid, 0, 0);
    if (xim->data == (char *)-1) {
      fprintf (stderr, "IMLIB ERROR: SHM can't attach SHM Segment for Shared Pixmap Wrapper\n");
      fprintf (stderr, "             Falling back on Shared XImages\n");
      shmctl (id->x.last_shminfo.shmid, IPC_RMID, 0);
      XDestroyImage (xim);
      shared_pixmap = 0;
      shared_image = 1;
      break;
    }
    id->x.last_shminfo.readOnly = False;
    XShmAttach (id->x.disp, &id->x.last_shminfo);
    tmp = (unsigned char *)xim->data;
    id->x.last_xim = xim;
    pmap = XShmCreatePixmap (id->x.disp, id->x.base_window,
      id->x.last_shminfo.shmaddr, &id->x.last_shminfo, w, h, id->x.depth);
    if (!tgc)
      tgc = XCreateGC (id->x.disp, pmap, GCGraphicsExposures, &gcv);
    if (im->shape) {
      sxim = XShmCreateImage (id->x.disp, id->x.visual, 1, ZPixmap, NULL, &id->x.last_sshminfo, w, h);
      if (!sxim) {
        fprintf (stderr, "IMLIB ERROR: Mit-SHM can't create XImage for Shared Pixmap mask Wrapper\n");
        fprintf (stderr, "             Falling back on Shared XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (id->x.last_shminfo.shmaddr);
        shared_pixmap = 0;
        shared_image = 1;
        break;
      }
      id->x.last_sshminfo.shmid = shmget (IPC_PRIVATE, sxim->bytes_per_line * sxim->height, IPC_CREAT | 0777);
      if (id->x.last_sshminfo.shmid == -1) {
        fprintf (stderr, "IMLIB ERROR: SHM can't get SHM Identifier for Shared Pixmap mask Wrapper\n");
        fprintf (stderr, "             Falling back on Shared XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (xim->data);
        /* missing shmctl(RMID) */
        XDestroyImage (sxim);
        shared_pixmap = 0;
        shared_image = 1;
        break;
      }
      id->x.last_sshminfo.shmaddr = sxim->data = shmat (id->x.last_sshminfo.shmid, 0, 0);
      if (sxim->data == (char *)-1) {
        fprintf (stderr, "IMLIB ERROR: SHM can't attach SHM Segment for Shared Pixmap mask Wrapper\n");
        fprintf (stderr, "             Falling back on Shared XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (xim->data);
        /* missing shmctl(RMID) */
        XDestroyImage (sxim);
        shmctl (id->x.last_shminfo.shmid, IPC_RMID, 0);
        break;
      }
      id->x.last_sshminfo.readOnly = False;
      XShmAttach (id->x.disp, &id->x.last_sshminfo);
      stmp = (unsigned char *)sxim->data;
      id->x.last_sxim = sxim;
      mask = XShmCreatePixmap (id->x.disp, id->x.base_window,
        id->x.last_sshminfo.shmaddr, &id->x.last_sshminfo, w, h, 1);
      if (!stgc)
        stgc = XCreateGC (id->x.disp, mask, GCGraphicsExposures, &gcv);
    }
  } while (0);

  do {
    if (!shared_image)
      break;
    xim = XShmCreateImage (id->x.disp, id->x.visual, id->x.depth, ZPixmap, NULL, &id->x.last_shminfo, w, h);
    if (!xim) {
      fprintf (stderr, "IMLIB ERROR: Mit-SHM can't create Shared XImage\n");
      fprintf (stderr, "             Falling back on XImages\n");
      shared_pixmap = 0;
      shared_image = 0;
      break;
    }
    id->x.last_shminfo.shmid = shmget (IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0777);
    if (id->x.last_shminfo.shmid == -1) {
      fprintf (stderr, "IMLIB ERROR: SHM can't get SHM Identifier for Shared XImage\n");
      fprintf (stderr, "             Falling back on XImages\n");
      XDestroyImage (xim);
      shared_pixmap = 0;
      shared_image = 0;
      break;
    }
    id->x.last_shminfo.shmaddr = xim->data = shmat (id->x.last_shminfo.shmid, 0, 0);
    if (xim->data == (char *)-1) {
      fprintf (stderr, "IMLIB ERROR: SHM can't attach SHM Segment for Shared XImage\n");
      fprintf (stderr, "             Falling back on XImages\n");
      XDestroyImage (xim);
      shmctl (id->x.last_shminfo.shmid, IPC_RMID, 0);
      shared_pixmap = 0;
      shared_image = 0;
      break;
    }
    id->x.last_shminfo.readOnly = False;
    XShmAttach (id->x.disp, &id->x.last_shminfo);
    tmp = (unsigned char *)xim->data;
    id->x.last_xim = xim;
    pmap = XCreatePixmap (id->x.disp, id->x.base_window, w, h, id->x.depth);
    if (!tgc)
      tgc = XCreateGC (id->x.disp, pmap, GCGraphicsExposures, &gcv);
    im->pixmap = pmap;
    if (im->shape) {
      sxim = XShmCreateImage (id->x.disp, id->x.visual, 1, ZPixmap, NULL, &id->x.last_sshminfo, w, h);
      if (!sxim) {
        fprintf (stderr, "IMLIB ERROR: Mit-SHM can't create Shared XImage mask\n");
        fprintf (stderr, "             Falling back on XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (id->x.last_shminfo.shmaddr);
        shmctl (id->x.last_shminfo.shmid, IPC_RMID, 0);
        shared_pixmap = 0;
        shared_image = 0;
        break;
      }
      id->x.last_sshminfo.shmid = shmget (IPC_PRIVATE, sxim->bytes_per_line * sxim->height, IPC_CREAT | 0777);
      if (id->x.last_sshminfo.shmid == -1) {
        fprintf (stderr, "Imlib ERROR: SHM can't get SHM Identifier for Shared XImage mask\n");
        fprintf (stderr, "             Falling back on XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (xim->data);
        /* missing shmctl(RMID) */
        XDestroyImage (sxim);
        shared_pixmap = 0;
        shared_image = 0;
        break;
      }
      id->x.last_sshminfo.shmaddr = sxim->data = shmat (id->x.last_sshminfo.shmid, 0, 0);
      if (sxim->data == (char *)-1) {
        fprintf (stderr, "Imlib ERROR: SHM can't attach SHM Segment for Shared XImage mask\n");
        fprintf (stderr, "             Falling back on XImages\n");
        XShmDetach (id->x.disp, &id->x.last_shminfo);
        XDestroyImage (xim);
        shmdt (xim->data);
        /* missing shmctl(RMID) */
        XDestroyImage (sxim);
        shmctl (id->x.last_shminfo.shmid, IPC_RMID, 0);
        shared_pixmap = 0;
        shared_image = 0;
        break;
      }
      id->x.last_sshminfo.readOnly = False;
      XShmAttach (id->x.disp, &id->x.last_sshminfo);
      stmp = (unsigned char *)sxim->data;
      id->x.last_sxim = sxim;
      mask = XCreatePixmap (id->x.disp, id->x.base_window, w, h, 1);
      if (!stgc)
        stgc = XCreateGC (id->x.disp, mask, GCGraphicsExposures, &gcv);
      im->shape_mask = mask;
    }
  } while (0);
  ok = 1;
#else
  shared_pixmap = 0;
  shared_image = 0;
#endif /* HAVE_SHM */

  do {
    if (shared_pixmap || shared_image)
      break;
    /* client side: make without data for now, lets see xim->bits_per_pixel first. */
    xim = XCreateImage (id->x.disp, id->x.visual, id->x.depth, ZPixmap, 0, NULL, w, h, 8, 0);
    if (xim) {
      xim->data = malloc (xim->bytes_per_line * xim->height);
      if (xim->data) {
        /* server side */
        pmap = XCreatePixmap (id->x.disp, id->x.base_window, w, h, id->x.depth);
        if (pmap) {
          im->pixmap = pmap;
          if (!im->shape)
            break;
          /* same game again for transparency mask */
          sxim = XCreateImage (id->x.disp, id->x.visual, 1, ZPixmap, 0, NULL, w, h, 8, 0);
          if (sxim) {
            sxim->data = malloc (sxim->bytes_per_line * sxim->height);
            if (sxim->data) {
              mask = XCreatePixmap (id->x.disp, id->x.base_window, w, h, 1);
              if (mask) {
                im->shape_mask = mask;
                break;
              } else {
                fprintf (stderr, "IMLIB ERROR: Cannot create shape pixmap\n");
              }
            } else {
              fprintf (stderr, "IMLIB ERROR: Cannot allocate RAM for shape XImage data\n");
            }
            XDestroyImage (sxim);
          } else {
            fprintf (stderr, "IMLIB ERROR: Cannot allocate XImage shape buffer\n");
          }
        } else {
          fprintf (stderr, "IMLIB ERROR: Cannot create pixmap\n");
        }
      } else {
        fprintf (stderr, "IMLIB ERROR: Cannot allocate RAM for XImage data\n");
      }
      XDestroyImage (xim);
    } else {
      fprintf (stderr, "IMLIB ERROR: Cannot allocate XImage buffer\n");
    }
    free (r);
    return 0;
  } while (0);

  if (!tgc)
    tgc = XCreateGC (id->x.disp, pmap, GCGraphicsExposures, &gcv);
  if (!stgc && im->shape)
    stgc = XCreateGC (id->x.disp, mask, GCGraphicsExposures, &gcv);

  bpp = id->x.depth <= 8 ? 8 : id->x.render_depth;

  r->xim = xim;
  if (bpp == 15)
    _rend_15 (r);
  else if (bpp == 16)
    _rend_16 (r);
  else if ((bpp == 24) || (bpp == 32))
    _rend_24 (r);
  else if (bpp == 8)
    _rend_8 (r);

  /* copy XImage to the pixmap, if not a shared pixmap */
  if (im->shape)
    {
      _imlib_fill_mask (r->yarray, r->xarray, sxim, w, h);
#ifdef HAVE_SHM
      if (shared_image)
	{
	  XShmPutImage(id->x.disp, pmap, tgc, xim, 0, 0, 0, 0, w, h, False);
	  XShmPutImage(id->x.disp, mask, stgc, sxim, 0, 0, 0, 0, w, h, False);
	  XSync(id->x.disp, False);
	  XShmDetach(id->x.disp, &id->x.last_shminfo);
	  XDestroyImage(xim);
	  shmdt(id->x.last_shminfo.shmaddr);
	  shmctl(id->x.last_shminfo.shmid, IPC_RMID, 0);
	  XShmDetach(id->x.disp, &id->x.last_sshminfo);
	  XDestroyImage(sxim);
	  shmdt(id->x.last_sshminfo.shmaddr);
	  shmctl(id->x.last_sshminfo.shmid, IPC_RMID, 0);
	  id->x.last_xim = NULL;
	  id->x.last_sxim = NULL;
	  xim = NULL;
	  sxim = NULL;
/*	  XFreeGC(id->x.disp, tgc);*/
/*	  XFreeGC(id->x.disp, stgc);*/
	}
      else if (shared_pixmap)
	{
	  Pixmap              p2, m2;

	  p2 = XCreatePixmap(id->x.disp, id->x.base_window, w, h, id->x.depth);
	  m2 = XCreatePixmap(id->x.disp, id->x.base_window, w, h, 1);
	  XCopyArea(id->x.disp, pmap, p2, tgc, 0, 0, w, h, 0, 0);
	  XCopyArea(id->x.disp, mask, m2, stgc, 0, 0, w, h, 0, 0);
	  im->pixmap = p2;
	  im->shape_mask = m2;
/*	  XFreeGC(id->x.disp, tgc);*/
/*	  XFreeGC(id->x.disp, stgc);*/
	  XFreePixmap(id->x.disp, pmap);
	  XFreePixmap(id->x.disp, mask);
	  XSync(id->x.disp, False);
	  XShmDetach(id->x.disp, &id->x.last_shminfo);
	  XDestroyImage(xim);
	  shmdt(id->x.last_shminfo.shmaddr);
	  shmctl(id->x.last_shminfo.shmid, IPC_RMID, 0);
	  XShmDetach(id->x.disp, &id->x.last_sshminfo);
	  XDestroyImage(sxim);
	  shmdt(id->x.last_sshminfo.shmaddr);
	  shmctl(id->x.last_sshminfo.shmid, IPC_RMID, 0);
	  id->x.last_xim = NULL;
	  id->x.last_sxim = NULL;
	  xim = NULL;
	  sxim = NULL;
	}
      else
#endif /* HAVE_SHM */
	{
	  XPutImage(id->x.disp, pmap, tgc, xim, 0, 0, 0, 0, w, h);
	  XPutImage(id->x.disp, mask, stgc, sxim, 0, 0, 0, 0, w, h);
	  XDestroyImage(xim);
	  XDestroyImage(sxim);
	  xim = NULL;
	  sxim = NULL;
/*	  XFreeGC(id->x.disp, tgc);*/
/*	  XFreeGC(id->x.disp, stgc);*/
	}
    }
  else
    {
#ifdef HAVE_SHM
      if (shared_image)
	{
	  XShmPutImage(id->x.disp, pmap, tgc, xim, 0, 0, 0, 0, w, h, False);
	  XSync(id->x.disp, False);
	  XShmDetach(id->x.disp, &id->x.last_shminfo);
	  XDestroyImage(xim);
	  shmdt(id->x.last_shminfo.shmaddr);
	  shmctl(id->x.last_shminfo.shmid, IPC_RMID, 0);
	  id->x.last_xim = NULL;
	  xim = NULL;
	  sxim = NULL;
/*	  XFreeGC(id->x.disp, tgc);*/
	}
      else if (shared_pixmap)
	{
	  Pixmap              p2;

	  p2 = XCreatePixmap(id->x.disp, id->x.base_window, w, h, id->x.depth);
	  XCopyArea(id->x.disp, pmap, p2, tgc, 0, 0, w, h, 0, 0);
	  im->pixmap = p2;
/*	  XFreeGC(id->x.disp, tgc);*/
	  XFreePixmap(id->x.disp, pmap);
	  XSync(id->x.disp, False);
	  XShmDetach(id->x.disp, &id->x.last_shminfo);
	  XDestroyImage(xim);
	  shmdt(id->x.last_shminfo.shmaddr);
	  shmctl(id->x.last_shminfo.shmid, IPC_RMID, 0);
	  id->x.last_xim = NULL;
	  xim = NULL;
	  sxim = NULL;
	}
      else
#endif /* HAVE_SHM */
	{
	  XPutImage(id->x.disp, pmap, tgc, xim, 0, 0, 0, 0, w, h);
	  XDestroyImage(xim);
	  xim = NULL;
	  sxim = NULL;
/*	  XFreeGC(id->x.disp, tgc);*/
	}
    }

/* cleanup */
/*  XSync(id->x.disp, False);*/
  free (r);

/* add this pixmap to the cache */
  add_pixmap(id, im, w, h, xim, sxim);
  return 1;
}

int Imlib_gfx_quality (ImlibData *im, int qual) {
  int old;
  if (!im)
    return 0;
  if (qual == -2)
    return 2;
  old = im->hiq ? (im->ordered_dither ? 1 : 2) : 0;
  if (qual == -1)
    return old;
  if (qual < 0)
    qual = 0;
  else if (qual > 2)
    qual = 2;
  if (old == qual)
    return 0;
  im->hiq = (qual > 0);
  im->ordered_dither = (qual == 1);
  return im->x.depth < 24;
}
