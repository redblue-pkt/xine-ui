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

static const uint8_t _tab_8_to_5[260] = {
  /* ((index - 2) * 31 + 127) / (255 + 3) */
   0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2,
   2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4,
   4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
   6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
   8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
   9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,
  11,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,
  13,13,14,14,14,14,14,14,14,14,15,15,15,15,15,15,
  15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,
  17,17,18,18,18,18,18,18,18,18,18,19,19,19,19,19,
  19,19,19,20,20,20,20,20,20,20,20,21,21,21,21,21,
  21,21,21,22,22,22,22,22,22,22,22,22,23,23,23,23,
  23,23,23,23,24,24,24,24,24,24,24,24,25,25,25,25,
  25,25,25,25,26,26,26,26,26,26,26,26,27,27,27,27,
  27,27,27,27,27,28,28,28,28,28,28,28,28,29,29,29,
  29,29,29,29,29,30,30,30,30,30,30,30,30,31,31,31,
  31,31,31,31
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

static int _best_color_match_8 (ImlibData * id, int *r, int *g, int *b) {
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

int
Imlib_best_color_match(ImlibData * id, int *r, int *g, int *b)
{
  int                 i;
  int                 dif;
  int                 dr, dg, db;
  int                 col;
  int                 mindif = 0x7fffffff;

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
  for (i = 0; i < id->num_colors; i++)
    {
      dr = *r - id->palette[i].r;
      if (dr < 0)
	dr = -dr;
      dg = *g - id->palette[i].g;
      if (dg < 0)
	dg = -dg;
      db = *b - id->palette[i].b;
      if (db < 0)
	db = -db;
      dif = dr + dg + db;
      if (dif < mindif)
	{
	  mindif = dif;
	  col = i;
	}
    }
  *r -= id->palette[col].r;
  *g -= id->palette[col].g;
  *b -= id->palette[col].b;
  col = id->palette[col].pixel;
  return col;
}

int
index_best_color_match(ImlibData * id, int *r, int *g, int *b)
{
  int                 i;
  int                 dif;
  int                 dr, dg, db;
  int                 col;
  int                 mindif = 0x7fffffff;

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
  for (i = 0; i < id->num_colors; i++)
    {
      dr = *r - id->palette[i].r;
      if (dr < 0)
	dr = -dr;
      dg = *g - id->palette[i].g;
      if (dg < 0)
	dg = -dg;
      db = *b - id->palette[i].b;
      if (db < 0)
	db = -db;
      dif = dr + dg + db;
      if (dif < mindif)
	{
	  mindif = dif;
	  col = i;
	}
    }
  *r -= id->palette[col].r;
  *g -= id->palette[col].g;
  *b -= id->palette[col].b;
  return col;
}

static const unsigned char _dmat1[4][4] = {
  {0, 4, 1, 5},
  {6, 2, 7, 3},
  {1, 5, 0, 4},
  {7, 3, 6, 2}
};

static const unsigned char _dmat2[4][4] = {
  {0, 4, 6, 5},
  {6, 2, 7, 3},
  {2, 6, 1, 5},
  {7, 4, 7, 3}
};

static void
render_15_fast_dither (int w, int h, XImage * xim, int *er1, int *er2, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 7) | ((eg & 0xf8) << 2) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x07;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_15_fast_dither_ordered (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int y, val;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h - 1; y += 2) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
    }
    img += jmp;
    ptr2 = yarray[y + 1];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 1] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 1] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 1] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      *img++ = val;
    }
    img += jmp;
  }
  if (y < h) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
    }
  }
}

static void
render_16_fast_dither (int w, int h, XImage * xim, int *er1, int *er2, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 8) | ((eg & 0xfc) << 3) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x03;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_16_fast_dither_ordered (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int y, val;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h - 1; y += 2) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
    }
    img += jmp;
    ptr2 = yarray[y + 1];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 1] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 1] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 1] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      *img++ = val;
    }
    img += jmp;
  }
  if (y < h) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      *img++ = val;
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      *img++ = val;
    }
  }
}

static void
render_15_dither (int w, int h, XImage * xim, int *er1, int *er2, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 7) | ((eg & 0xf8) << 2) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x07;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_15_dither_ordered (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int y, val;

  for (y = 0; y < h - 1; y += 2) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      XPutPixel (xim, x + 1, y, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
    }
    ptr2 = yarray[y + 1];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      XPutPixel (xim, x, y + 1, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 1] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 1] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 1] <<  0;
      XPutPixel (xim, x + 1, y + 1, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      XPutPixel (xim, x, y + 1, val);
    }
  }
  if (y < h) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      XPutPixel (xim, x + 1, y, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 10;
      val += (uint32_t)_tab_8_to_5[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
    }
  }
}

static void
render_16_dither (int w, int h, XImage * xim, int *er1, int *er2, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 8) | ((eg & 0xfc) << 3) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x03;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_16_dither_ordered (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int y, val;

  for (y = 0; y < h - 1; y += 2) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      XPutPixel (xim, x + 1, y, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
    }
    ptr2 = yarray[y + 1];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      XPutPixel (xim, x, y + 1, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 1] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 1] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 1] <<  0;
      XPutPixel (xim, x + 1, y + 1, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 3] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 3] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 3] <<  0;
      XPutPixel (xim, x, y + 1, val);
    }
  }
  if (y < h) {
    int x;
    uint8_t *ptr2 = yarray[y];
    for (x = 0; x < w - 1; x += 2) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
      ptr = ptr2 + xarray[x + 1];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 2] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 2] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 2] <<  0;
      XPutPixel (xim, x + 1, y, val);
    }
    if (x < w) {
      uint8_t *ptr = ptr2 + xarray[x];
      val  = (uint32_t)_tab_8_to_5[ptr[0] + 0] << 11;
      val += (uint32_t)_tab_8_to_6[ptr[1] + 0] <<  5;
      val += (uint32_t)_tab_8_to_5[ptr[2] + 0] <<  0;
      XPutPixel (xim, x, y, val);
    }
  }
}

static void
render_15_fast (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_16_fast (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_24_fast (ImlibData * id, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, r, g, b;
  unsigned char      *ptr2;
  unsigned char      *img;
  int                 jmp;

  jmp = (xim->bytes_per_line) - w * 3;
  img = (unsigned char *)xim->data;
  if (id->x.byte_order == MSBFirst)
    {
      if (id->byte_order == BYTE_ORD_24_RGB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = r;
		  *img++ = g;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_RBG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = r;
		  *img++ = b;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BRG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = b;
		  *img++ = r;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BGR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = b;
		  *img++ = g;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GRB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = g;
		  *img++ = r;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GBR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = g;
		  *img++ = b;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
    }
  else
    {
      if (id->byte_order == BYTE_ORD_24_RGB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = b;
		  *img++ = g;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_RBG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = g;
		  *img++ = b;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BRG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = g;
		  *img++ = r;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BGR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = r;
		  *img++ = g;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GRB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = b;
		  *img++ = r;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GBR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  *img++ = r;
		  *img++ = b;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
    }
}

static void
render_32_fast (ImlibData * id, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int            x, y, val, r, g, b;
  unsigned char *ptr2;
  uint32_t      *img;
  int            jmp;

  jmp = (xim->bytes_per_line >> 2) - w;
  img = (uint32_t *)xim->data;
  if (id->byte_order == BYTE_ORD_24_RGB) {
    if (endian_is.little) {
      for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
          uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
          uint32_t v = *p; /* abgr */
          v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24); /* rgba */
          *img++ = v >> 8;
        }
        img += jmp;
      }
    } else {
      for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
          uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
          uint32_t v = *p; /* rgba */
          *img++ = v >> 8;
        }
        img += jmp;
      }
    }
  } else if (id->byte_order == BYTE_ORD_24_RBG) {
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        ptr2 = yarray[y] + xarray[x];
        r = (int)ptr2[0];
        g = (int)ptr2[1];
        b = (int)ptr2[2];
        val = (r << 16) | (b << 8) | g;
        *img++ = val;
      }
      img += jmp;
    }
  } else if (id->byte_order == BYTE_ORD_24_BRG) {
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        ptr2 = yarray[y] + xarray[x];
        r = (int)ptr2[0];
        g = (int)ptr2[1];
        b = (int)ptr2[2];
        val = (b << 16) | (r << 8) | g;
        *img++ = val;
      }
      img += jmp;
    }
  } else if (id->byte_order == BYTE_ORD_24_BGR) {
    if (endian_is.little) {
      for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
          uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
          uint32_t v = *p; /* abgr */
          *img++ = v & 0x00ffffff;
        }
        img += jmp;
      }
    } else {
      for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
          uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
          uint32_t v = *p; /* rgba */
          v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24); /* abgr */
          *img++ = v & 0x00ffffff;
        }
        img += jmp;
      }
    }
  } else if (id->byte_order == BYTE_ORD_24_GRB) {
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        ptr2 = yarray[y] + xarray[x];
        r = (int)ptr2[0];
        g = (int)ptr2[1];
        b = (int)ptr2[2];
        val = (g << 16) | (r << 8) | b;
        *img++ = val;
      }
      img += jmp;
    }
  } else if (id->byte_order == BYTE_ORD_24_GBR) {
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        ptr2 = yarray[y] + xarray[x];
        r = (int)ptr2[0];
        g = (int)ptr2[1];
        b = (int)ptr2[2];
        val = (g << 16) | (b << 8) | r;
        *img++ = val;
      }
      img += jmp;
    }
  }
}

static void
render_15 (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_16 (int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_24 (ImlibData * id, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  if (id->byte_order == BYTE_ORD_24_RGB)
    {
      if (endian_is.little) {
        for (y = 0; y < h; y++) {
          for (x = 0; x < w; x++) {
            uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
            uint32_t v = *p; /* abgr */
            v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24); /* rgba */
            XPutPixel (xim, x, y, v >> 8);
          }
        }
      } else {
        for (y = 0; y < h; y++) {
          for (x = 0; x < w; x++) {
            uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
            uint32_t v = *p; /* rgba */
            XPutPixel (xim, x, y, v >> 8);
          }
        }
      }
    }
  else if (id->byte_order == BYTE_ORD_24_RBG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      val = (r << 16) | (b << 8) | g;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BRG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      val = (b << 16) | (r << 8) | g;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BGR)
    {
      if (endian_is.little) {
        for (y = 0; y < h; y++) {
          for (x = 0; x < w; x++) {
            uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
            uint32_t v = *p; /* abgr */
            XPutPixel (xim, x, y, v & 0x00ffffff);
          }
        }
      } else {
        for (y = 0; y < h; y++) {
          for (x = 0; x < w; x++) {
            uint32_t *p = (uint32_t *)(yarray[y] + xarray[x]);
            uint32_t v = *p; /* rgba */
            v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24); /* abgr */
            XPutPixel (xim, x, y, v & 0x00ffffff);
          }
        }
      }
    }
  else if (id->byte_order == BYTE_ORD_24_GRB)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      val = (g << 16) | (r << 8) | b;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_GBR)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      val = (g << 16) | (b << 8) | r;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
}

static void
render (ImlibData * id, int w, int h, XImage *xim, int *er1, int *er2, int *xarray, unsigned char **yarray, int bpp)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;
  unsigned char      *img;
  int                 jmp;

  jmp = (xim->bytes_per_line) - w * (bpp >> 3);
  img = (unsigned char *)xim->data;
  switch (id->render_type)
    {
    case RT_PLAIN_PALETTE:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  val = _best_color_match_8 (id, &r, &g, &b);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  val = _best_color_match_8 (id, &r, &g, &b);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_PLAIN_PALETTE_FAST:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  val = COLOR_RGB(r >> 3, g >> 3, b >> 3);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  val = COLOR_RGB(r >> 3, g >> 3, b >> 3);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_DITHER_PALETTE:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = _best_color_match_8 (id, &er, &eg, &eb);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = _best_color_match_8 (id, &er, &eg, &eb);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_DITHER_PALETTE_FAST:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = INDEX_RGB(er >> 3, eg >> 3, eb >> 3);
		  er = ERROR_RED(er, val);
		  eg = ERROR_GRN(eg, val);
		  eb = ERROR_BLU(eb, val);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  *img++ = COLOR_INDEX(val);
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = INDEX_RGB(er >> 3, eg >> 3, eb >> 3);
		  er = ERROR_RED(er, val);
		  eg = ERROR_GRN(eg, val);
		  eb = ERROR_BLU(eb, val);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  XPutPixel(xim, x, y, COLOR_INDEX(val));
		}
	    }
	}
      break;
    default:
      if (id->fastrend)
	{
	  switch (bpp)
	    {
	    case 8:
	      break;
	    case 15:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_15_fast_dither_ordered (w, h, xim, xarray, yarray);
		  else
		    render_15_fast_dither (w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_15_fast (w, h, xim, xarray, yarray);
	      break;
	    case 16:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_16_fast_dither_ordered (w, h, xim, xarray, yarray);
		  else
		    render_16_fast_dither (w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_16_fast (w, h, xim, xarray, yarray);
	      break;
	    case 24:
	    case 32:
	      if (xim->bits_per_pixel == 24)
		render_24_fast (id, w, h, xim, xarray, yarray);
	      else
		render_32_fast (id, w, h, xim, xarray, yarray);
	      break;
	    default:
	      break;
	    }
	}
      else
	{
	  switch (bpp)
	    {
	    case 8:
	      break;
	    case 15:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_15_dither_ordered (w, h, xim, xarray, yarray);
		  else
		    render_15_dither (w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_15 (w, h, xim, xarray, yarray);
	      break;
	    case 16:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_16_dither_ordered (w, h, xim, xarray, yarray);
		  else
		    render_16_dither (w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_16 (w, h, xim, xarray, yarray);
	      break;
	    case 24:
	      render_24 (id, w, h, xim, xarray, yarray);
	      break;
	    case 32:
              /* we do know what we are doing? */
	      render_32_fast (id, w, h, xim, xarray, yarray);
	      break;
	    default:
	      break;
	    }
	  break;
	}
    }
}

static void
render_15_fast_dither_mod (ImlibImage * im, int w, int h, XImage * xim,
			  int *er1, int *er2, int *xarray,
			  unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 7) | ((eg & 0xf8) << 2) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x07;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_15_fast_dither_mod_ordered (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, er, eg, eb;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  int                 dithy, dithx;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      dithy = y & 0x3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r & 0x07;
	  eg = g & 0x07;
	  eb = b & 0x07;
	  dithx = x & 0x3;
	  if ((_dmat2[dithy][dithx] < er) && (r < (256 - 8)))
	    r += 8;
	  if ((_dmat2[dithy][dithx] < eg) && (g < (256 - 8)))
	    g += 8;
	  if ((_dmat2[dithy][dithx] < eb) && (b < (256 - 8)))
	    b += 8;
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_16_fast_dither_mod (ImlibImage * im, int w, int h, XImage * xim,
			  int *er1, int *er2, int *xarray,
			  unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 8) | ((eg & 0xfc) << 3) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x03;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_16_fast_dither_mod_ordered (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, er, eg, eb;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  int                 dithy, dithx;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      dithy = y & 0x3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r & 0x07;
	  eg = g & 0x03;
	  eb = b & 0x07;
	  dithx = x & 0x3;
	  if ((_dmat2[dithy][dithx] < er) && (r < (256 - 8)))
	    r += 8;
	  if ((_dmat2[dithy][dithx] < (eg << 1)) && (g < (256 - 4)))
	    g += 4;
	  if ((_dmat2[dithy][dithx] < eb) && (b < (256 - 8)))
	    b += 8;
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_15_dither_mod_ordered (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, er, eg, eb;
  unsigned char      *ptr2;

  int                 dithy, dithx;

  for (y = 0; y < h; y++)
    {
      dithy = y & 0x3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r & 0x07;
	  eg = g & 0x07;
	  eb = b & 0x07;
	  dithx = x & 0x3;
	  if ((_dmat2[dithy][dithx] < er) && (r < (256 - 8)))
	    r += 8;
	  if ((_dmat2[dithy][dithx] < eg) && (g < (256 - 8)))
	    g += 8;
	  if ((_dmat2[dithy][dithx] < eb) && (b < (256 - 8)))
	    b += 8;
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_16_dither_mod_ordered (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b, er, eg, eb;
  unsigned char      *ptr2;

  int                 dithy, dithx;

  for (y = 0; y < h; y++)
    {
      dithy = y & 0x3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r & 0x07;
	  eg = g & 0x03;
	  eb = b & 0x07;
	  dithx = x & 0x3;
	  if ((_dmat2[dithy][dithx] < er) && (r < (256 - 8)))
	    r += 8;
	  if ((_dmat2[dithy][dithx] < (eg << 1)) && (g < (256 - 4)))
	    g += 4;
	  if ((_dmat2[dithy][dithx] < eb) && (b < (256 - 8)))
	    b += 8;
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_15_dither_mod (ImlibImage * im, int w, int h, XImage * xim,
		     int *er1, int *er2, int *xarray,
		     unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 7) | ((eg & 0xf8) << 2) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x07;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_16_dither_mod (ImlibImage * im, int w, int h, XImage * xim,
		     int *er1, int *er2, int *xarray,
		     unsigned char **yarray)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      ter = er1;
      er1 = er2;
      er2 = ter;
      for (ex = 0; ex < (w + 2) * 3; ex++)
	er2[ex] = 0;
      ex = 3;
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  er = r + er1[ex++];
	  eg = g + er1[ex++];
	  eb = b + er1[ex++];
	  if (er > 255)
	    er = 255;
	  if (eg > 255)
	    eg = 255;
	  if (eb > 255)
	    eb = 255;
	  val = ((er & 0xf8) << 8) | ((eg & 0xfc) << 3) | ((eb & 0xf8) >> 3);
	  er = er & 0x07;
	  eg = eg & 0x03;
	  eb = eb & 0x07;
	  DITHER_ERROR(er1, er2, ex, er, eg, eb);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_15_fast_mod (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;
  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_16_fast_mod (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  unsigned short     *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 1) - w;
  img = (unsigned short *)xim->data;
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  *img++ = val;
	}
      img += jmp;
    }
}

static void
render_24_fast_mod (ImlibData * id, ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, r, g, b;
  unsigned char      *ptr2;
  unsigned char      *img;
  int                 jmp;

  jmp = (xim->bytes_per_line) - w * 3;
  img = (unsigned char *)xim->data;

  if (id->x.byte_order == MSBFirst)
    {
      if (id->byte_order == BYTE_ORD_24_RGB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = r;
		  *img++ = g;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_RBG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = r;
		  *img++ = b;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BRG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = b;
		  *img++ = r;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BGR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = b;
		  *img++ = g;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GRB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = g;
		  *img++ = r;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GBR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = g;
		  *img++ = b;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
    }
  else
    {
      if (id->byte_order == BYTE_ORD_24_RGB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = b;
		  *img++ = g;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_RBG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = g;
		  *img++ = b;
		  *img++ = r;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BRG)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = g;
		  *img++ = r;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_BGR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = r;
		  *img++ = g;
		  *img++ = b;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GRB)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = b;
		  *img++ = r;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
      else if (id->byte_order == BYTE_ORD_24_GBR)
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  *img++ = r;
		  *img++ = b;
		  *img++ = g;
		}
	      img += jmp;
	    }
	}
    }
}

static void
render_32_fast_mod (ImlibData * id, ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;
  unsigned int       *img;
  int                 jmp;

  jmp = (xim->bytes_per_line >> 2) - w;
  img = (unsigned int *)xim->data;
  if (id->byte_order == BYTE_ORD_24_RGB)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (r << 16) | (g << 8) | b;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
  else if (id->byte_order == BYTE_ORD_24_RBG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (r << 16) | (b << 8) | g;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BRG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (b << 16) | (r << 8) | g;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BGR)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (b << 16) | (g << 8) | r;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
  else if (id->byte_order == BYTE_ORD_24_GRB)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (g << 16) | (r << 8) | b;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
  else if (id->byte_order == BYTE_ORD_24_GBR)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (g << 16) | (b << 8) | r;
	      *img++ = val;
	    }
	  img += jmp;
	}
    }
}

static void
render_15_mod (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  val = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}

static void
render_16_mod (ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
	{
	  ptr2 = yarray[y] + xarray[x];
	  r = (int)*ptr2++;
	  g = (int)*ptr2++;
	  b = (int)*ptr2;
	  r = im->rmap[r];
	  g = im->gmap[g];
	  b = im->bmap[b];
	  val = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
	  XPutPixel(xim, x, y, val);
	}
    }
}


static void
render_24_mod (ImlibData * id, ImlibImage * im, int w, int h, XImage * xim, int *xarray, unsigned char **yarray)
{
  int                 x, y, val, r, g, b;
  unsigned char      *ptr2;

  if (id->byte_order == BYTE_ORD_24_RGB)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (r << 16) | (g << 8) | b;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_RBG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (r << 16) | (b << 8) | g;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BRG)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (b << 16) | (r << 8) | g;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_BGR)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (b << 16) | (g << 8) | r;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_GRB)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (g << 16) | (r << 8) | b;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
  else if (id->byte_order == BYTE_ORD_24_GBR)
    {
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      ptr2 = yarray[y] + xarray[x];
	      r = (int)*ptr2++;
	      g = (int)*ptr2++;
	      b = (int)*ptr2;
	      r = im->rmap[r];
	      g = im->gmap[g];
	      b = im->bmap[b];
	      val = (g << 16) | (b << 8) | r;
	      XPutPixel(xim, x, y, val);
	    }
	}
    }
}

static void
render_mod(ImlibData * id, ImlibImage * im, int w, int h, XImage * xim,
	   int *er1, int *er2, int *xarray,
	   unsigned char **yarray, int bpp)
{
  int                 x, y, val, r, g, b, *ter, ex, er, eg, eb;
  unsigned char      *ptr2;
  unsigned char      *img;
  int                 jmp;

  jmp = (xim->bytes_per_line) - w * (bpp >> 3);
  img = (unsigned char *)xim->data;
  switch (id->render_type)
    {
    case RT_PLAIN_PALETTE:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  val = _best_color_match_8 (id, &r, &g, &b);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  val = _best_color_match_8 (id, &r, &g, &b);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_PLAIN_PALETTE_FAST:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  val = COLOR_RGB(r >> 3, g >> 3, b >> 3);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  val = COLOR_RGB(r >> 3, g >> 3, b >> 3);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_DITHER_PALETTE:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = _best_color_match_8 (id, &er, &eg, &eb);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  *img++ = val;
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = _best_color_match_8 (id, &er, &eg, &eb);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  XPutPixel(xim, x, y, val);
		}
	    }
	}
      break;
    case RT_DITHER_PALETTE_FAST:
      if ((id->fastrend) && (xim->bits_per_pixel == 8))
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = INDEX_RGB(er >> 3, eg >> 3, eb >> 3);
		  er = ERROR_RED(er, val);
		  eg = ERROR_GRN(eg, val);
		  eb = ERROR_BLU(eb, val);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  *img++ = COLOR_INDEX(val);
		}
	      img += jmp;
	    }
	}
      else
	{
	  for (y = 0; y < h; y++)
	    {
	      ter = er1;
	      er1 = er2;
	      er2 = ter;
	      for (ex = 0; ex < (w + 2) * 3; ex++)
		er2[ex] = 0;
	      ex = 3;
	      for (x = 0; x < w; x++)
		{
		  ptr2 = yarray[y] + xarray[x];
		  r = (int)*ptr2++;
		  g = (int)*ptr2++;
		  b = (int)*ptr2;
		  r = im->rmap[r];
		  g = im->gmap[g];
		  b = im->bmap[b];
		  er = r + er1[ex++];
		  eg = g + er1[ex++];
		  eb = b + er1[ex++];
		  if (er > 255)
		    er = 255;
		  else if (er < 0)
		    er = 0;
		  if (eg > 255)
		    eg = 255;
		  else if (eg < 0)
		    eg = 0;
		  if (eb > 255)
		    eb = 255;
		  else if (eb < 0)
		    eb = 0;
		  val = INDEX_RGB(er >> 3, eg >> 3, eb >> 3);
		  er = ERROR_RED(er, val);
		  eg = ERROR_GRN(eg, val);
		  eb = ERROR_BLU(eb, val);
		  DITHER_ERROR(er1, er2, ex, er, eg, eb);
		  XPutPixel(xim, x, y, COLOR_INDEX(val));
		}
	    }
	}
      break;
    default:
      if (id->fastrend)
	{
	  switch (bpp)
	    {
	    case 8:
	      break;
	    case 15:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_15_fast_dither_mod_ordered (im, w, h, xim, xarray, yarray);
		  else
		    render_15_fast_dither_mod (im, w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_15_fast_mod (im, w, h, xim, xarray, yarray);
	      break;
	    case 16:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_16_fast_dither_mod_ordered (im, w, h, xim, xarray, yarray);
		  else
		    render_16_fast_dither_mod (im, w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_16_fast_mod (im, w, h, xim, xarray, yarray);
	      break;
	    case 24:
	    case 32:
	      if (xim->bits_per_pixel == 24)
		render_24_fast_mod (id, im, w, h, xim, xarray, yarray);
	      else
		render_32_fast_mod (id, im, w, h, xim, xarray, yarray);
	      break;
	    default:
	      break;
	    }
	}
      else
	{
	  switch (bpp)
	    {
	    case 8:
	      break;
	    case 15:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_15_dither_mod_ordered (im, w, h, xim, xarray, yarray);
		  else
		    render_15_dither_mod (im, w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_15_mod (im, w, h, xim, xarray, yarray);
	      break;
	    case 16:
	      if (id->render_type == RT_DITHER_TRUECOL)
		{
		  if (id->ordered_dither)
		    render_16_dither_mod_ordered (im, w, h, xim, xarray, yarray);
		  else
		    render_16_dither_mod (im, w, h, xim, er1, er2, xarray, yarray);
		}
	      else
		render_16_mod (im, w, h, xim, xarray, yarray);
	      break;
	    case 24:
	      render_24_mod (id, im, w, h, xim, xarray, yarray);
	      break;
	    case 32:
	      render_24_mod (id, im, w, h, xim, xarray, yarray);
	      break;
	    default:
	      break;
	    }
	  break;
	}
    }
}


static void _imlib_fill_mask (unsigned char **yarray, int *xarray, XImage *mask, int w, int h) {
  int y;
  if (!mask)
    return;
  if ((mask->format == 2) && (mask->bits_per_pixel == 1)) {
    uint8_t *q = (uint8_t *)mask->data;
    int skip = mask->bytes_per_line - ((w + 7) >> 3);
    int stop = w & ~7;
    if (mask->bitmap_bit_order == 0) {
      for (y = 0; y < h; y++) {
        const uint8_t *p = yarray[y] + 3;
        int x;
        for (x = 0; x < stop; x += 8) {
          *q++ = ((p[xarray[x]    ] & 0x80) >> 7)
               | ((p[xarray[x + 1]] & 0x80) >> 6)
               | ((p[xarray[x + 2]] & 0x80) >> 5)
               | ((p[xarray[x + 3]] & 0x80) >> 4)
               | ((p[xarray[x + 4]] & 0x80) >> 3)
               | ((p[xarray[x + 5]] & 0x80) >> 2)
               | ((p[xarray[x + 6]] & 0x80) >> 1)
               | ((p[xarray[x + 7]] & 0x80) >> 0);
        }
        if (x < w) {
          do {
            *q  = ((p[xarray[x]] & 0x80) >> 7);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 6);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 5);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 4);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 3);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 2);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 1);
          } while (0);
          q++;
        }
        q += skip;
      }
    } else {
      for (y = 0; y < h; y++) {
        const uint8_t *p = yarray[y] + 3;
        int x;
        for (x = 0; x < stop; x += 8) {
          *q++ = ((p[xarray[x]    ] & 0x80) >> 0)
               | ((p[xarray[x + 1]] & 0x80) >> 1)
               | ((p[xarray[x + 2]] & 0x80) >> 2)
               | ((p[xarray[x + 3]] & 0x80) >> 3)
               | ((p[xarray[x + 4]] & 0x80) >> 4)
               | ((p[xarray[x + 5]] & 0x80) >> 5)
               | ((p[xarray[x + 6]] & 0x80) >> 6)
               | ((p[xarray[x + 7]] & 0x80) >> 7);
        }
        if (x < w) {
          do {
            *q  = ((p[xarray[x]] & 0x80) >> 0);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 1);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 2);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 3);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 4);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 5);
            if (++x >= w) break;
            *q |= ((p[xarray[x]] & 0x80) >> 6);
          } while (0);
          q++;
        }
        q += skip;
      }
    }
    return;
  }
  for (y = 0; y < h; y++) {
    unsigned char *p = yarray[y];
    int x;
    for (x = 0; x < w; x++) {
      XPutPixel (mask, x, y, p[xarray[x] + 3] >> 7);
    }
  }
}

int
Imlib_render(ImlibData * id, ImlibImage * im, int w, int h)
{
  XImage             *xim, *sxim;
  static              Display *pd = NULL;
  static GC           tgc = 0, stgc = 0;
  XGCValues           gcv;
  unsigned char      *tmp, *stmp, **yarray, *ptr22;
  int                 w4, x, inc, pos, *error, *er1, *er2, *xarray, ex, bpp;
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

/* dithering array */
  error = (int *)calloc((w + 2), sizeof(int) * 2 * 3);

  if (!error)
    {
      fprintf(stderr, "ERROR: Cannot allocate RAM for image dither buffer\n");
      return 0;
    }

/* setup pointers to point right */
  er1 = error;
  er2 = error + ((w + 2) * 3);
  w4 = im->rgb_width * 4;
  ptr22 = im->rgb_data;

/* setup coord-mapping array (specially for border scaling) */
  xarray = calloc(w, sizeof(int));

  if (!xarray)
    {
      fprintf(stderr, "ERROR: Cannot allocate X co-ord buffer\n");
      free(error);
      return 0;
    }
  yarray = calloc(h, sizeof(unsigned char *));

  if (!yarray)
    {
      fprintf(stderr, "ERROR: Cannot allocate Y co-ord buffer\n");
      free(xarray);
      free(error);
      return 0;
    }
  for (ex = 0; ex < ((w + 2) * 3 * 2); ex++)
    error[ex] = 0;
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
	  xarray[x] = (pos >> 16) * 4;
	  pos += 0x10000;
	}
    if (m)
      {
	for (x = l; x < l + m; x++)
	  {
	    xarray[x] = (pos >> 16) * 4;
	    pos += inc;
	  }
      }
    pos = (im->rgb_width - r) << 16;
    for (x = w - r; x < w; x++)
      {
	xarray[x] = (pos >> 16) * 4;
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
    free (xarray);
    free (yarray);
    free (error);
    return 0;
  } while (0);

  if (!tgc)
    tgc = XCreateGC (id->x.disp, pmap, GCGraphicsExposures, &gcv);
  if (!stgc && im->shape)
    stgc = XCreateGC (id->x.disp, mask, GCGraphicsExposures, &gcv);

  bpp = id->x.depth <= 8 ? 8
    : id->x.render_depth == 24 ? xim->bits_per_pixel
    : id->x.render_depth;

  /* copy XImage to the pixmap, if not a shared pixmap */
  if (im->shape)
    {
      _imlib_fill_mask (yarray, xarray, sxim, w, h);
      if ((im->mod.gamma == 256) && (im->mod.brightness == 256) && (im->mod.contrast == 256) &&
	  (im->rmod.gamma == 256) && (im->rmod.brightness == 256) && (im->rmod.contrast == 256) &&
	  (im->gmod.gamma == 256) && (im->gmod.brightness == 256) && (im->gmod.contrast == 256) &&
	  (im->bmod.gamma == 256) && (im->bmod.brightness == 256) && (im->bmod.contrast == 256))
        render (id, w, h, xim, er1, er2, xarray, yarray, bpp);
      else
        render_mod (id, im, w, h, xim, er1, er2, xarray, yarray, bpp);
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
      if ((im->mod.gamma == 256) && (im->mod.brightness == 256) && (im->mod.contrast == 256) &&
	  (im->rmod.gamma == 256) && (im->rmod.brightness == 256) && (im->rmod.contrast == 256) &&
	  (im->gmod.gamma == 256) && (im->gmod.brightness == 256) && (im->gmod.contrast == 256) &&
	  (im->bmod.gamma == 256) && (im->bmod.brightness == 256) && (im->bmod.contrast == 256))
        render (id, w, h, xim, er1, er2, xarray, yarray, bpp);
      else
        render_mod (id, im, w, h, xim, er1, er2, xarray, yarray, bpp);
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
  free(error);
  free(xarray);
  free(yarray);

/* add this pixmap to the cache */
  add_pixmap(id, im, w, h, xim, sxim);
  return 1;
}

