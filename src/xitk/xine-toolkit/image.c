/*
 * Copyright (C) 2000-2020 the xine project
 *
 * This file is part of xine, a unix video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <xine/sorted_array.h>

#include "_xitk.h"

#include "utils.h"
#include "font.h"

typedef struct {
  xitk_widget_t  w;

  char          *skin_element_name;
  xitk_image_t  *skin;
} _image_private_t;

static int _xitk_pix_font_find_char (xitk_pix_font_t *pf, xitk_point_t *found, int this_char) {
  int range, n = 0;

  for (range = 0; pf->unicode_ranges[range].first > 0; range++) {
    if ((this_char >= pf->unicode_ranges[range].first) && (this_char <= pf->unicode_ranges[range].last))
      break;
    n += pf->unicode_ranges[range].last - pf->unicode_ranges[range].first + 1;
  }

  if (pf->unicode_ranges[range].first <= 0) {
    *found = pf->unknown;
    return 0;
  }

  n += this_char - pf->unicode_ranges[range].first;
  found->x = (n % pf->chars_per_row) * pf->char_width;
  found->y = (n / pf->chars_per_row) * pf->char_height;
  return 1;
}

void xitk_image_set_pix_font (xitk_image_t *image, const char *format) {
  xitk_pix_font_t *pf;
  int range, total;

  if (!image || !format)
    return;
  if (image->pix_font)
    return;
  image->pix_font = pf = malloc (sizeof (*pf));
  if (!pf)
    return;

  pf->width = image->width;
  pf->height = image->height;

  range = 0;
  total = 0;
  do {
    const uint8_t *p = (const uint8_t *)format;
    int v;
    uint8_t z;
    if (!p)
      break;
    while (1) {
      while (*p && (*p != '('))
        p++;
      if (!(*p))
        break;
      p++;
      if ((*p ^ '0') < 10)
        break;
    }
    if (!(*p))
      break;

    v = 0;
    while ((z = (*p ^ '0')) < 10)
      v = v * 10u + z, p++;
    if (v <= 0)
      break;
    pf->chars_per_row = v;
    if (*p != '!')
      break;
    p++;

    while (1) {
      pf->unicode_ranges[range].last = 0;
      v = 0;
      while ((z = (*p ^ '0')) < 10)
        v = v * 10u + z, p++;
      pf->unicode_ranges[range].first = v;
      if (*p != '-')
        break;
      p++;
      v = 0;
      while ((z = (*p ^ '0')) < 10)
        v = v * 10u + z, p++;
      pf->unicode_ranges[range].last = v;
      if (pf->unicode_ranges[range].last < pf->unicode_ranges[range].first)
        pf->unicode_ranges[range].last = pf->unicode_ranges[range].first;
      total += pf->unicode_ranges[range].last - pf->unicode_ranges[range].first + 1;
      range++;
      if (range >= XITK_MAX_UNICODE_RANGES)
        break;
      if (*p != '!')
        break;
      p++;
    }
  } while (0);

  if (range == 0) {
    pf->chars_per_row = 32;
    pf->unicode_ranges[0].first = 32;
    pf->unicode_ranges[0].last = 127;
    total = 127 - 32 + 1;
    range = 1;
  }

  pf->char_width = pf->width / pf->chars_per_row;
  pf->chars_total = total;
  total = (total + pf->chars_per_row - 1) / pf->chars_per_row;
  pf->char_height = pf->height / total;
  pf->unicode_ranges[range].first = 0;
  pf->unicode_ranges[range].last = 0;

  pf->unknown.x = 0;
  pf->unknown.y = 0;
  _xitk_pix_font_find_char (pf, &pf->unknown, 127);
  _xitk_pix_font_find_char (pf, &pf->space, ' ');
  _xitk_pix_font_find_char (pf, &pf->asterisk, '*');
}

static void _xitk_image_destroy_pix_font (xitk_pix_font_t **pix_font) {
  free (*pix_font);
  *pix_font = NULL;
}

/*
 *
 */
void xitk_image_ref (xitk_image_t *img) {
  if (!img)
    return;
  img->refs += 1;
  if (img->refs > img->max_refs)
    img->max_refs = img->refs;
}

void xitk_image_free_image (xitk_image_t **src) {
  xitk_image_t *image;

  if (!src)
    return;
  image = *src;
  if (!image)
    return;

  image->refs -= 1;
  if (image->refs > 0)
    return;

#ifdef XINE_SARRAY_MODE_UNIQUE
  if (image->wl && image->wl->shared_images) {
    xine_sarray_remove_ptr (image->wl->shared_images, image);
#ifdef DEBUG_SHARED_IMAGE
    printf ("xitk_shared_image: %d uses of %s (%d x %d).\n",
      image->max_refs, image->key, image->width, image->height);
#endif
  }
#endif
  if (image->beimg) {
    image->beimg->_delete (&image->beimg);
  }
  _xitk_image_destroy_pix_font (&image->pix_font);

  XITK_FREE (image);
  *src = NULL;
}

void xitk_image_copy (xitk_image_t *from, xitk_image_t *to) {
  if (!from || !to)
    return;
  if (!from->beimg || !to->beimg)
    return;
  to->beimg->copy_rect (to->beimg, from->beimg, 0, 0, from->width, from->height, 0, 0);
}

void xitk_image_copy_rect (xitk_image_t *from, xitk_image_t *to, int x1, int y1, int w, int h, int x2, int y2) {
  if (!from || !to)
    return;
  if (!from->beimg || !to->beimg)
    return;
  to->beimg->copy_rect (to->beimg, from->beimg, x1, y1, w, h, x2, y2);
}

uintptr_t xitk_image_get_pixmap (xitk_image_t *img) {
  return img && img->beimg ? img->beimg->id1 : 0;
}

uintptr_t xitk_image_get_mask (xitk_image_t *img) {
  return img && img->beimg ? img->beimg->id2 : 0;
}

static void _xitk_image_add_beimg (xitk_image_t *img, const char *data, int dsize) {
  xitk_tagitem_t tags[7] = {
    {XITK_TAG_FILENAME, (uintptr_t)NULL},
    {XITK_TAG_FILEBUF, (uintptr_t)NULL},
    {XITK_TAG_FILESIZE, 0},
    {XITK_TAG_RAW, (uintptr_t)NULL},
    {XITK_TAG_WIDTH, img->width},
    {XITK_TAG_HEIGHT, img->height},
    {XITK_TAG_END, 0}
  };
  if (data) {
    if (dsize > 0) {
      tags[1].value = (uintptr_t)data;
      tags[2].value = dsize;
    } else if (dsize == 0) {
      tags[0].value = (uintptr_t)data;
    } else if (dsize == -1) {
      tags[3].value = (uintptr_t)data;
    }
  }
  img->beimg = img->xitk->d->image_new (img->xitk->d, tags);
  if (!img->beimg)
    return;
  if (data) {
    img->beimg->get_props (img->beimg, tags + 4);
    img->width = tags[4].value;
    img->height = tags[5].value;
  }
}

/* if (data != NULL):
 * dsize > 0: decode (down)loaded image file contents,
 *            and aspect preserving scale to fit w and/or h if set.
 * dsize == 0: read from named file, then same as above.
 * dsize == -1: use raw data as (w, h). */
xitk_image_t *xitk_image_new (xitk_t *xitk, const char *data, int dsize, int w, int h) {
  xitk_image_t *img;

  if (!xitk)
    return NULL;
  img = (xitk_image_t *)xitk_xmalloc (sizeof (*img));
  if (!img)
    return NULL;
  img->xitk = xitk;
  img->width = w;
  img->height = h;
  _xitk_image_add_beimg (img, data, dsize);
  if (!img->beimg) {
    XITK_FREE (img);
    return NULL;
  }
  img->wl = NULL;
  img->max_refs =
  img->refs     = 1;
  img->key[0] = 0;
  return img;
}

int xitk_image_inside (xitk_image_t *img, int x, int y) {
  if (!img)
    return 0;
  if ((x < 0) || (x >= img->width) || (y < 0) || (y >= img->height))
    return 0;
  if (!img->beimg)
    return 1;
  return img->beimg->pixel_is_visible (img->beimg, x, y);
}

#ifdef XINE_SARRAY_MODE_UNIQUE
static int _xitk_shared_image_cmp (void *a, void *b) {
  xitk_image_t *d = (xitk_image_t *)a;
  xitk_image_t *e = (xitk_image_t *)b;

  if (d->width < e->width)
    return -1;
  if (d->width > e->width)
    return 1;
  if (d->height < e->height)
    return -1;
  if (d->height > e->height)
    return 1;
  return strcmp (d->key, e->key);
}
#endif

int xitk_shared_image (xitk_widget_list_t *wl, const char *key, int width, int height, xitk_image_t **image) {
  xitk_image_t *i;
  size_t keylen;

  ABORT_IF_NOT_COND (width > 0);
  ABORT_IF_NOT_COND (height > 0);

  if (!image)
    return 0;
  if (!wl || !key) {
    *image = NULL;
    return 0;
  }

  keylen = strlen (key);
  if (keylen > sizeof (i->key) - 1)
    keylen = sizeof (i->key) - 1;
  i = (xitk_image_t *)xitk_xmalloc (sizeof (*i) + keylen);
  if (!i) {
    *image = NULL;
    return 0;
  }
  i->xitk  = wl->xitk;
  i->width = width;
  i->height = height;
  memcpy (i->key, key, keylen);
  i->key[keylen] = 0;

#ifdef XINE_SARRAY_MODE_UNIQUE
  if (!wl->shared_images) {
    wl->shared_images = xine_sarray_new (16, _xitk_shared_image_cmp);
    xine_sarray_set_mode (wl->shared_images, XINE_SARRAY_MODE_UNIQUE);
  }
  if (wl->shared_images) {
    int ai;

    ai = xine_sarray_add (wl->shared_images, i);
    if (ai < 0) {
      XITK_FREE (i);
      i = xine_sarray_get (wl->shared_images, ~ai);
      i->refs += 1;
      if (i->refs > i->max_refs)
        i->max_refs = i->refs;
      *image = i;
      return i->refs;
    }
  }
#endif
  _xitk_image_add_beimg (i, NULL, 0);
  if (!i->beimg) {
    XITK_FREE (i);
    return 0;
  }
  i->max_refs = i->refs = 1;
  i->wl = wl;
  *image = i;
  return i->refs;
}

void xitk_shared_image_list_delete (xitk_widget_list_t *wl) {
  if (wl && wl->shared_images) {
    int i, max = xine_sarray_size (wl->shared_images);

    for (i = 0; i < max; i++) {
      xitk_image_t *image = xine_sarray_get (wl->shared_images, i);

      image->wl = NULL;
    }

    xine_sarray_delete(wl->shared_images);
    wl->shared_images = NULL;
  }
}

void xitk_image_fill_rectangle (xitk_image_t *img, int x, int y, int w, int h, unsigned int color) {
  if (img && img->beimg) {
    xitk_be_rect_t xr[1];

    xr[0].x = x; xr[0].y = y; xr[0].w = w; xr[0].h = h;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->fill_rects (img->beimg, xr, 1, color, 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

/*
 *
 */
xitk_image_t *xitk_image_create_image_with_colors_from_string (xitk_t *xitk,
  const char *fontname, int width, int pad_x, int pad_y, int align, const char *str,
  unsigned int foreground, unsigned int background) {
  xitk_image_t   *image;
  xitk_font_t    *fs;
  int             length, height, lbearing, rbearing, ascent, descent, linel, linew, wlinew, lastws;
  int             maxw = 0;
  const char     *p;
  char           *bp;
  char           *lines[256];
  int             numlines = 0;
  char            buf[BUFSIZ]; /* Could be allocated dynamically for bigger texts */

  int             add_line_spc = 2;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(fontname);
  ABORT_IF_NULL(str);

  width -= 2 * pad_x;
  ABORT_IF_NOT_COND(width > 0);

  /* Creating an image from an empty string would cause an abort with failed */
  /* condition "width > 0". So we substitute some spaces (one single space   */
  /* may not be enough!). Should only happen in case of error.               */
  if(!*str)
    str = "   ";

  fs = xitk_font_load_font(xitk, fontname);
  xitk_font_string_extent(fs, str, NULL, NULL, NULL, &ascent, &descent);
  height = ascent + descent;

  p      = str;
  bp     = buf;
  wlinew = linew = lastws = linel = 0;

  /*
   * Create string image using exactly the ink width
   * without left and right spacing measures
   */

  while((*p!='\0') && (bp + linel) < (buf + BUFSIZ - TABULATION_SIZE - 2)) {

    switch(*p) {

    case '\t':
      {
	int a;

	if((linel == 0) || (bp[linel - 1] != ' ')) {
	  lastws = linel;
	  wlinew = linew; /* width if wrapped */
	}

	a = TABULATION_SIZE - (linel % TABULATION_SIZE);

        while(a--)
          bp[linel++] = ' ';
      }
      break;

      case '\a':
      case '\b':
      case '\f':
      case '\r':
      case '\v':
        break; /* Ignore those */

    case '\n':
      lines[numlines++] = bp;
      bp                = bp + linel + 1;

      wlinew = lastws = linel = 0;

      if(linew > maxw)
	maxw = linew;
      break;

    case ' ':
      if((linel == 0) || (bp[linel-1] != ' ')) {
	lastws = linel;
	wlinew = linew; /* width if wrapped */
      }

      /* fall through */
    default:
      bp[linel++] = *p;

      if(!lastws)
	wlinew = linew;
      break;
    }

    bp[linel] = '\0'; /* terminate the string for reading with xitk_font_string_extent or strlen */

    xitk_font_string_extent(fs, bp, &lbearing, &rbearing, NULL, NULL, NULL);
    if((linew = rbearing - lbearing) > width) {

      if(lastws == 0) { /* if we haven't found a whitespace */
        bp[linel]         = bp[linel-1]; /* Move that last character down */
        bp[linel-1]       = 0;
        lines[numlines++] = bp;
        bp                += linel;
        linel             = 1;
        bp[linel]         = 0;
      }
      else {
        char *nextword = (bp + lastws);
        int   wordlen;

        while(*nextword == ' ')
          nextword++;

        wordlen           = (bp + linel) - nextword;
        bp[lastws]        = '\0';
        lines[numlines++] = bp;
        bp                = bp + lastws + 1;

        if(bp != nextword)
          memmove(bp, nextword, wordlen + 1);

        linel = wordlen;
        lastws = 0;
      }

      if(wlinew > maxw)
        maxw = wlinew;

      xitk_font_string_extent(fs, bp, &lbearing, &rbearing, NULL, NULL, NULL);
      linew = rbearing - lbearing;
    }

    p++;
  }

  if(linel) { /* In case last chars aren't stored */
    lines[numlines++] = bp;

    if(linew > maxw)
      maxw = linew;
  }

  /* If default resp. left aligned, we may shrink the image */
  if((align == ALIGN_DEFAULT) || (align == ALIGN_LEFT))
    width = MIN(maxw, width);

  image = xitk_image_new (xitk, NULL, 0, width + 2 * pad_x, (height + add_line_spc) * numlines - add_line_spc + 2 * pad_y);
  xitk_image_fill_rectangle (image, 0, 0, image->width, image->height, background);

  { /* Draw string in image */
    int i, y;

    for (y = ascent + pad_y, i = 0; i < numlines; i++, y += (height + add_line_spc)) {
      int x;

      xitk_font_string_extent(fs, lines[i], &lbearing, &rbearing, NULL, NULL, NULL);
      length = rbearing - lbearing;

      x = align == ALIGN_CENTER ? (width - length) >> 1
        : align == ALIGN_RIGHT ? width - length
        : 0;
      x += pad_x;

      xitk_font_draw_string (fs, image, (x - lbearing), y, lines[i], strlen(lines[i]), foreground);
                                        /*   ^^^^^^^^ Adjust to start of ink */
    }
  }

  xitk_font_unload_font(fs);
  return image;
}

xitk_image_t *xitk_image_create_image_from_string (xitk_t *xitk, const char *fontname,
    int width, int align, const char *str) {

  return xitk_image_create_image_with_colors_from_string (xitk, fontname, width, 0, 0, align, str,
    xitk_get_cfg_num (xitk, XITK_BLACK_COLOR), xitk_get_cfg_num (xitk, XITK_BG_COLOR));
}

void xitk_image_draw_menu_arrow_branch (xitk_image_t *img) {
  int w, h, i, x1, x2, x3, y1, y2, y3;
  xitk_point_t points[4];

  if (!img)
    return;
  if (!img->beimg || !img->xitk)
    return;

  w = img->width / 3;
  h = img->height;

  x1 = (w - 5);
  y1 = (h / 2);

  x2 = (w - 10);
  y2 = ((h / 2) + 5);

  x3 = (w - 10);
  y3 = ((h / 2) - 5);

  for(i = 0; i < 3; i++) {

    if(i == 2) {
      x1++; x2++; x3++;
      y1++; y2++; y3++;
    }

    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    points[2].x = x3;
    points[2].y = y3;
    points[3].x = x1;
    points[3].y = y1;

    xitk_image_fill_polygon (img, &points[0], 4, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR));

    x1 += w;
    x2 += w;
    x3 += w;
  }

}

/*
 *
 */
static void _xitk_image_draw_arrow (xitk_image_t *img, int direction) {
  int w, h, nsegments, i, s, x1, x2, dx, y1, y2, dy;
  xitk_be_line_t *segments;

  if (!img)
    return;
  if (!img->beimg)
    return;

  w = img->width / 3;
  h = img->height;

  /*
   * XFillPolygon doesn't yield equally shaped arbitrary sized small triangles
   * because of its filling algorithm (see also fill-rule in XCreateGC(3X11)
   * as for which pixels are drawn on the boundary).
   * So we handcraft them using XDrawSegments applying Bresenham's algorithm.
   */

  /* Coords of the enclosing rectangle for the triangle:   */
  /* Pay attention to integer precision loss and calculate */
  /* carefully to obtain symmetrical and centered shapes.  */
  x1 = ((w - 1) / 2) - (w / 4);
  x2 = ((w - 1) / 2) + (w / 4);
  y1 = ((h - 1) / 2) - (h / 4);
  y2 = ((h - 1) / 2) + (h / 4);

  dx = (x2 - x1 + 1);
  dy = (y2 - y1 + 1);

  if(direction == DIRECTION_UP || direction == DIRECTION_DOWN) {
    int y, iy, dd;

    nsegments = dy;
    segments = calloc (nsegments, sizeof (*segments));

    if(direction == DIRECTION_DOWN) {
      y = y1; iy = 1;
    }
    else {
      y = y2; iy = -1;
    }
    dx = (dx + 1) / 2;
    dd = 0;
    for(s = 0; s < nsegments; s++) {
      segments[s].y1 = y; segments[s].x1 = x1;
      segments[s].y2 = y; segments[s].x2 = x2;
      y += iy;
      if(dy >= dx) {
	if((dd += dx) >= dy) {
	  x1++; x2--;
	  dd -= dy;
	}
      }
      else {
	do {
	  x1++; x2--;
	} while((dd += dy) < dx);
	dd -= dx;
      }
    }
  }
  else if(direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT) {
    int x, ix, dd;

    nsegments = dx;
    segments = calloc (nsegments, sizeof (*segments));

    if(direction == DIRECTION_RIGHT) {
      x = x1; ix = 1;
    }
    else {
      x = x2; ix = -1;
    }
    dy = (dy + 1) / 2;
    dd = 0;
    for(s = 0; s < nsegments; s++) {
      segments[s].x1 = x; segments[s].y1 = y1;
      segments[s].x2 = x; segments[s].y2 = y2;
      x += ix;
      if(dx >= dy) {
	if((dd += dy) >= dx) {
	  y1++; y2--;
	  dd -= dx;
	}
      }
      else {
	do {
	  y1++; y2--;
	} while((dd += dx) < dy);
	dd -= dy;
      }
    }
  }
  else {
    XITK_WARNING("direction '%d' is unhandled.\n", direction);
    return;
  }

  img->beimg->display->lock (img->beimg->display);
  for (i = 0; i < 3; i++) {
    if (i == 2) {
      for (s = 0; s < nsegments; s++) {
        segments[s].x1++; segments[s].y1++;
        segments[s].x2++; segments[s].y2++;
      }
    }
    img->beimg->draw_lines (img->beimg, segments, nsegments,xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
    for (s = 0; s < nsegments; s++) {
      segments[s].x1 += w;
      segments[s].x2 += w;
    }
  }
  img->beimg->display->unlock (img->beimg->display);

  free(segments);
}

/*
 *
 */
void xitk_image_draw_arrow_up (xitk_image_t *img) {
  _xitk_image_draw_arrow (img, DIRECTION_UP);
}
void xitk_image_draw_arrow_down (xitk_image_t *img) {
  _xitk_image_draw_arrow (img, DIRECTION_DOWN);
}
void xitk_image_draw_arrow_left (xitk_image_t *img) {
  _xitk_image_draw_arrow (img, DIRECTION_LEFT);
}
void xitk_image_draw_arrow_right (xitk_image_t *img) {
  _xitk_image_draw_arrow (img, DIRECTION_RIGHT);
}

/*
 *
 */
void xitk_image_draw_line (xitk_image_t *img, int x0, int y0, int x1, int y1, unsigned color) {
  if (img && img->beimg) {
    xitk_be_line_t lines[1];

    lines[0].x1 = x0; lines[0].y1 = y0; lines[0].x2 = x1; lines[0].y2 = y1;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->draw_lines (img->beimg, lines, 1, color, 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

void xitk_image_draw_rectangle (xitk_image_t *img, int x, int y, int w, int h, unsigned int color) {
  if (img && img->beimg) {
    xitk_be_line_t lines[4];

    lines[0].x1 = x;         lines[0].y1 = y;         lines[0].x2 = x + w - 1; lines[0].y2 = y;
    lines[1].x1 = x;         lines[1].y1 = y + h - 1; lines[1].x2 = x + w - 1; lines[1].y2 = y + h - 1;
    lines[2].x1 = x;         lines[2].y1 = y;         lines[2].x2 = x;         lines[2].y2 = y + h - 1;
    lines[3].x1 = x + w - 1; lines[3].y1 = y;         lines[3].x2 = x + w - 1; lines[3].y2 = y + h - 1;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->draw_lines (img->beimg, lines, 4, color, 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

void xitk_image_fill_polygon (xitk_image_t *img, const xitk_point_t *points, int npoints, unsigned color) {
  if (!img)
    return;
  if (!img->beimg || !img->xitk)
    return;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_polygon (img->beimg, points, npoints, color, 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
static void _xitk_image_draw_rectangular_box (xitk_image_t *img,
  int x, int y, int excstart, int excstop, int width, int height, int type) {
  unsigned int color[2];
  xitk_be_line_t xs[5], *q;

  if (!img)
    return;
  if (!img->beimg)
    return;

  color[(type & DRAW_FLATTER) != DRAW_OUTTER] = xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR);
  color[(type & DRAW_FLATTER) == DRAW_OUTTER] = (type & DRAW_LIGHT)
                                              ? xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR)
                                              : xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR);

  /* +---     ----- *              | *
   * |              *              | *
   * |              * -------------+ */
  q = xs;
  if (excstart < excstop) {
    q->x1 = x + 1; q->x2 = x + excstart;        q->y1 = q->y2 = y; q++;
    q->x1 = x + excstop; q->x2 = x + width - 2; q->y1 = q->y2 = y; q++;
  } else {
    q->x1 = x + 1; q->x2 = x + width - 2;       q->y1 = q->y2 = y; q++;
  }
  q->x1 = q->x2 = x; q->y1 = y + 1; q->y2 = y + height - 2; q++;
  if (type & DRAW_DOUBLE) {
    q->x1 = q->x2 = x + width - 2;        q->y1 = y + 2; q->y2 = y + height - 3; q++;
    q->x1 = x + 2; q->x2 = x + width - 3; q->y1 = q->y2 = y + height - 2; q++;
  }
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, color[0], 0);
  img->beimg->display->unlock (img->beimg->display);

  /*              | * +---     ----- *
   *              | * |              *
   * -------------+ * |              */
  q = xs;
  q->x1 = q->x2 = x + width - 1;        q->y1 = y + 1; q->y2 = y + height - 2; q++;
  q->x1 = x + 1; q->x2 = x + width - 2; q->y1 = q->y2 = y + height - 1;        q++;
  if (type & DRAW_DOUBLE) {
    if (excstart < excstop) {
      q->x1 = x + 2; q->x2 = x + excstart;        q->y1 = q->y2 = y + 1; q++;
      q->x1 = x + excstop; q->x2 = x + width - 3; q->y1 = q->y2 = y + 1; q++;
    } else {
      q->x1 = x + 2; q->x2 = x + width - 3;       q->y1 = q->y2 = y + 1; q++;
    }
    q->x1 = q->x2 = x + 1; q->y1 = y + 2; q->y2 = y + height - 3; q++;
  }
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, color[1], 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
void xitk_image_draw_rectangular_box (xitk_image_t *img, int x, int y, int width, int height, int type) {
  _xitk_image_draw_rectangular_box (img, x, y, 0, 0, width, height, type);
}

static void _xitk_image_draw_check_round (xitk_image_t *img, int x, int y, int d, int checked) {
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_arc (img->beimg, x, y, d, d, (30 * 64), (180 * 64),
    xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->fill_arc (img->beimg, x, y, d, d, (210 * 64), (180 * 64),
    xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);
  img->beimg->fill_arc (img->beimg, x + 2, y + 2, d - 4, d - 4, (0 * 64), (360 * 64),
    xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  if (checked)
    img->beimg->fill_arc (img->beimg, x + 4, y + 4, d - 8, d - 8, (0 * 64), (360 * 64),
      xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
}

static void _xitk_image_draw_check_check (xitk_image_t *img, int x, int y, int d, int checked) {
  xitk_be_rect_t xr[1];
  xitk_be_line_t xs[4];

  if (!img)
    return;
  if (!img->beimg)
    return;

  /* background */
  xr[0].x = x, xr[0].y = y, xr[0].w = xr[0].h = d;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_rects (img->beimg, xr, 1, (checked & 2) ? xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR)
    : xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
  /* */
  xs[0].x1 = x, xs[0].y1 = y, xs[0].x2 = x + d, xs[0].y2 = y;
  xs[1].x1 = x, xs[1].y1 = y, xs[1].x2 = x,     xs[1].y2 = y + d;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 2, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  xs[0].x1 = x,     xs[0].y1 = y + d, xs[0].x2 = x + d, xs[0].y2 = y + d;
  xs[1].x1 = x + d, xs[1].y1 = y,     xs[1].x2 = x + d, xs[1].y2 = y + d;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 2, xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  if (checked & 1) {
    xs[0].x1 = x + (d / 5),     xs[0].y1 = (y + ((d / 3) * 2)) - 2, xs[0].x2 = x + (d / 2),     xs[0].y2 = y + d - 2;
    xs[1].x1 = x + (d / 5) + 1, xs[1].y1 = (y + ((d / 3) * 2)) - 2, xs[1].x2 = x + (d / 2) + 1, xs[1].y2 = y + d - 2;
    xs[2].x1 = x + (d / 2),     xs[2].y1 =  y +   d            - 2, xs[2].x2 = x +  d      - 2, xs[2].y2 = y     + 1;
    xs[3].x1 = x + (d / 2) + 1, xs[3].y1 =  y +   d            - 2, xs[3].x2 = x +  d      - 1, xs[3].y2 = y     + 1;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->draw_lines (img->beimg, xs, 4, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

static void _xitk_image_draw_check_three_state_round (xitk_image_t *img, int x, int y, int d, int w, int checked) {
  int i;

  for (i = 0; i < 3; i++) {
    if (i == 2) {
      x++;
      y++;
    }
    _xitk_image_draw_check_round (img, x, y, d, checked);
    x += w;
  }
}

static void _xitk_image_draw_check_three_state_check (xitk_image_t *img, int x, int y, int d, int w, int checked) {
  int i;

  for (i = 0; i < 3; i++) {
    if (i == 2) {
      x++;
      y++;
    }
    _xitk_image_draw_check_check (img, x, y, d, checked);
    x += w;
  }
}

void xitk_image_draw_menu_check (xitk_image_t *img, int checked) {
  int  style;

  if (!img)
    return;

  style = xitk_get_cfg_num (img->xitk, XITK_CHECK_STYLE);
  switch (style) {
    case CHECK_STYLE_CHECK:
      _xitk_image_draw_check_three_state_check (img, 4, 4, img->height - 8, img->width / 3, checked);
      break;

    case CHECK_STYLE_ROUND:
      _xitk_image_draw_check_three_state_round (img, 4, 4, img->height - 8, img->width / 3, checked);
      break;

    case CHECK_STYLE_OLD:
    default:
      {
        int relief = (checked) ? DRAW_INNER : DRAW_OUTTER;
        int nrelief = (checked) ? DRAW_OUTTER : DRAW_INNER;
        int w, h;

        w = img->width / 3;
        h = img->height - 12;
        _xitk_image_draw_rectangular_box (img, 4,               6,     0, 0, 12, h, relief);
        _xitk_image_draw_rectangular_box (img, w + 4,           6,     0, 0, 12, h, relief);
        _xitk_image_draw_rectangular_box (img, (w * 2) + 4 + 1, 6 + 1, 0, 0, 12, h, nrelief);
      }
      break;
  }
}

/*
 *
 */
static void _xitk_image_draw_three_state (xitk_image_t *img, int style) {
  int w, h;
  xitk_be_rect_t xr[1];
  xitk_be_line_t xs[8], *q;

  if (!img)
    return;
  if (!img->beimg)
    return;

  w = img->width / 3;
  h = img->height;

  img->beimg->display->lock (img->beimg->display);
  xr[0].x = 0, xr[0].y = 0, xr[0].w = w, xr[0].h = h;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
  xr[0].x = w, xr[0].y = 0, xr[0].w = w * 2, xr[0].h = h;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /* +----+----       *      +----       *
   * |    |           *      |           *
   * |    |           *      |           */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = 0 * w; q->x2 = 1 * w; q->y1 = q->y2 = 0; q++;
    q->x1 = q->x2 = 0 * w; q->y1 = 0; q->y2 = h - 1; q++;
  }
  q->x1 = 1 * w; q->x2 = 2 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 1 * w; q->y1 = 0; q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /*     |    |+----  *          |+----  *
   *     |    ||      *          ||      *
   * ----+----+|      *      ----+|      */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 2; q->y1 = 2; q->y2 = h - 3; q++;
    q->x1 = 2; q->x2 = 1 * w - 2; q->y1 = q->y2 = h - 2; q++;
  }
  q->x1 = q->x2 = 2 * w - 2; q->y1 = 2; q->y2 = h - 3; q++;
  q->x1 = 1 * w + 2; q->x2 = 2 * w - 2; q->y1 = q->y2 = h - 2; q++;
  q->x1 = 2 * w + 0; q->x2 = 3 * w + 0; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 2 * w + 0; q->y1 = 0; q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);
  xr[0].x = 2 * w, xr[0].y = 0, xr[0].w = w - 1, xr[0].h = h - 1;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 1; q->y1 = 0; q->y2 = h - 1; q++;
    q->x1 = 0; q->x2 = 1 * w - 1; q->y1 = q->y2 = h - 1; q++;
  }
  q->x1 = 2 * w + 1; q->x2 = 3 * w - 1; q->y1 = q->y2 = 1; q++;
  q->x1 = q->x2 = 2 * w + 1; q->y1 = 1; q->y2 = h - 2; q++;
  q->x1 = q->x2 = 2 * w - 1; q->y1 = 0; q->y2 = h; q++;
  q->x1 = 1 * w + 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /*                | *
   *                | *
   *            ----+ */
  q = xs;
  q->x1 = q->x2 = 3 * w - 1; q->y1 = 1; q->y2 = h - 1; q++;
  q->x1 = 2 * w + 1; q->x2 = 3 * w + 0; q->y1 = q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /* +   ++   ++   + *
   *                 *
   * +   ++   ++   + */
  q = xs;
  q->x1 = q->x2 = 0 * w;            q->y1 = q->y2 = 0; q++;
  q->x1 = 1 * w - 1; q->x2 = 1 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = 2 * w - 1; q->x2 = 2 * w; q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 3 * w - 1;        q->y1 = q->y2 = 0; q++;
  q->x1 = q->x2 = 0 * w;            q->y1 = q->y2 = h - 1; q++;
  q->x1 = 1 * w - 1; q->x2 = 1 * w; q->y1 = q->y2 = h - 1; q++;
  q->x1 = 2 * w - 1; q->x2 = 2 * w; q->y1 = q->y2 = h - 1; q++;
  q->x1 = q->x2 = 3 * w - 1;        q->y1 = q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
static void _xitk_image_draw_two_state (xitk_image_t *img, int style) {
  int w, h;
  xitk_be_rect_t xr[1];
  xitk_be_line_t xs[3], *q;

  if (!img)
    return;
  if (!img->beimg)
    return;

  w = img->width / 2;
  h = img->height;

  img->beimg->display->lock (img->beimg->display);
  xr[0].x = 0, xr[0].y = 0, xr[0].w = w - 1, xr[0].h = h - 1;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
  xr[0].x = w, xr[0].y = 0, xr[0].w = (w * 2) - 1, xr[0].h = h - 1;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /* +-----+----- *       +----- *
   * |     |      *       |      *
   * |     |      *       |      */
  q = xs;
  if (style == STYLE_BEVEL) {
    q->x1 = 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = 0;        q++;
    q->x1 = q->x2 = 0;            q->y1 = 0; q->y2 = h - 1; q++;
  } else {
    q->x1 = w; q->x2 = 2 * w - 1; q->y1 = q->y2 = 0; q++;
  }
  q->x1 = q->x2 = w; q->y1 = 0; q->y2 = h - 1; q++;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /*      |     | *            | *
   *      |     | *            | *
   * -----+-----+ *       -----+ */
  q = xs;
  q->x1 = q->x2 = 2 * w - 1;      q->y1 = 0; q->y2 = h - 1; q++;
  if (style == STYLE_BEVEL) {
    q->x1 = q->x2 = 1 * w - 1;    q->y1 = 0; q->y2 = h - 1; q++;
    q->x1 = 0; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1;    q++;
  } else {
    q->x1 = 1 * w; q->x2 = 2 * w - 1; q->y1 = q->y2 = h - 1; q++;
  }
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, q - xs, xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
static void _xitk_image_draw_relief (xitk_image_t *img, int w, int h, int type) {
  if (!img)
    return;

  xitk_image_fill_rectangle (img, 0, 0, w, h, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR));

  if (((type & DRAW_FLATTER) == DRAW_OUTTER) || ((type & DRAW_FLATTER) == DRAW_INNER))
    _xitk_image_draw_rectangular_box (img, 0, 0, 0, 0, w, h, type);
}

void xitk_image_draw_checkbox_check (xitk_image_t *img) {
  int style = xitk_get_cfg_num (img->xitk, XITK_CHECK_STYLE);

  xitk_image_fill_rectangle (img, 0, 0, img->width, img->height, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR));

  switch (style) {
    case CHECK_STYLE_CHECK:
      if (img->width == img->height * 4) {
        int w = img->width / 4;
        _xitk_image_draw_check_check (img, w * 0, 0, img->height, 0);
        _xitk_image_draw_check_check (img, w * 1, 0, img->height, 1);
        _xitk_image_draw_check_check (img, w * 2, 0, img->height, 2);
        _xitk_image_draw_check_check (img, w * 3, 0, img->height, 3);
      } else {
        int w = img->width / 3;
        _xitk_image_draw_check_check (img, 0, 0, img->height, 0);
        _xitk_image_draw_check_check (img, w, 0, img->height, 0);
        _xitk_image_draw_check_check (img, w * 2, 0, img->height, 1);
      }
      break;

    case CHECK_STYLE_ROUND:
      {
        int w = img->width / 3;
        _xitk_image_draw_check_round (img, 0, 0, img->height, 0);
        _xitk_image_draw_check_round (img, w, 0, img->height, 0);
        _xitk_image_draw_check_round (img, w * 2, 0, img->height, 1);
      }
      break;

    case CHECK_STYLE_OLD:
    default:
      _xitk_image_draw_three_state (img, STYLE_BEVEL);
      break;
  }
}

/*
 *
 */
void xitk_image_draw_flat_three_state (xitk_image_t *img) {
  _xitk_image_draw_three_state (img, STYLE_FLAT);
}
void xitk_image_draw_bevel_three_state (xitk_image_t *img) {
  _xitk_image_draw_three_state (img, STYLE_BEVEL);
}
void xitk_image_draw_bevel_two_state (xitk_image_t *img) {
  _xitk_image_draw_two_state (img, STYLE_BEVEL);
}

void xitk_image_draw_paddle_three_state (xitk_image_t *img, int width, int height) {
  int w, h, gap, m, dir;
  xitk_be_line_t xs[9];
  xitk_be_rect_t xr[3];

  if (!img)
    return;
  if (!img->beimg)
    return;

  dir = width > height;
  w = img->width / 3;
  if ((width > 0) && (width <= w))
    w = width;
  h = img->height;
  if ((height > 0) && (height <= h))
    h = height;

  gap = (w < 11) || (h < 11) ? 1 : 2;

  /* ------------
   * |  ||  ||  |
   * ------------ */
  /* Draw mask */
  xr[0].x = 0, xr[0].y = 0, xr[0].w = img->width, xr[0].h = img->height;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_rects (img->beimg, xr, 1, 0, 1);
  img->beimg->display->unlock (img->beimg->display);
  xr[0].x = 0 * w + 1; xr[0].y = 1; xr[0].w = w - 2; xr[0].h = h - 2;
  xr[1].x = 1 * w + 1; xr[1].y = 1; xr[1].w = w - 2; xr[1].h = h - 2;
  xr[2].x = 2 * w + 1; xr[2].y = 1; xr[2].w = w - 2; xr[2].h = h - 2;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_rects (img->beimg, xr, 3, 1, 1);
  img->beimg->display->unlock (img->beimg->display);

  img->beimg->display->lock (img->beimg->display);
  xr[0].x = 0 * w, xr[0].y = 0, xr[0].w = w, xr[0].h = h;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
  xr[0].x = 1 * w, xr[0].y = 0, xr[0].w = w, xr[0].h = h;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR), 0);
  xr[0].x = 2 * w, xr[0].y = 0, xr[0].w = w, xr[0].h = h;
  img->beimg->fill_rects (img->beimg, xr, 1, xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
  /* +---+---
   * |   |       |
   *          ---+ */
  xs[0].x1 = 0 * w + gap + 1; xs[0].x2 = 1 * w - gap - 2; xs[0].y1 = xs[0].y2 = gap;
  xs[1].x1 = 1 * w + gap + 1; xs[1].x2 = 2 * w - gap - 2; xs[1].y1 = xs[1].y2 = gap;
  xs[2].x1 = xs[2].x2 = 0 * w + gap;     xs[2].y1 = gap + 1; xs[2].y2 = h - gap - 2;
  xs[3].x1 = xs[3].x2 = 1 * w + gap;     xs[3].y1 = gap + 1; xs[3].y2 = h - gap - 2;
  xs[4].x1 = xs[4].x2 = 3 * w - gap - 1; xs[4].y1 = gap + 1; xs[4].y2 = h - gap - 2;
  xs[5].x1 = 2 * w + gap + 1; xs[5].x2 = 3 * w - gap - 2; xs[5].y1 = xs[5].y2 = h - gap - 1;
  if (!dir) {
    /*   -    -    -   */
    m = (h - 1) >> 1;
    xs[6].x1 = 0 * w + gap + 3; xs[6].x2 = 1 * w - gap - 4; xs[6].y1 = xs[6].y2 = m;
    xs[7].x1 = 1 * w + gap + 3; xs[7].x2 = 2 * w - gap - 4; xs[7].y1 = xs[7].y2 = m;
    xs[8].x1 = 2 * w + gap + 3; xs[8].x2 = 3 * w - gap - 4; xs[8].y1 = xs[8].y2 = m;
  } else {
    /*   |    |    |   */
    m = (w - 1) >> 1;
    xs[6].x1 = xs[6].x2 = 0 * w + m; xs[6].y1 = gap + 3; xs[6].y2 = h - gap - 4;
    xs[7].x1 = xs[7].x2 = 1 * w + m; xs[7].y1 = gap + 3; xs[7].y2 = h - gap - 4;
    xs[8].x1 = xs[8].x2 = 2 * w + m; xs[8].y1 = gap + 3; xs[8].y2 = h - gap - 4;
  }
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 9, xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
  /*         +---
   *    |   ||
   * ---+---+    */
  xs[0].x1 = 2 * w + gap + 1; xs[0].x2 = 3 * w - gap - 2; xs[0].y1 = xs[0].y2 = gap;
  xs[1].x1 = xs[1].x2 = 1 * w - gap - 1; xs[1].y1 = gap + 1; xs[1].y2 = h - gap - 2;
  xs[2].x1 = xs[2].x2 = 2 * w - gap - 1; xs[2].y1 = gap + 1; xs[2].y2 = h - gap - 2;
  xs[3].x1 = xs[3].x2 = 2 * w + gap;     xs[3].y1 = gap + 1; xs[3].y2 = h - gap - 2;
  xs[4].x1 = 0 * w + gap + 1; xs[4].x2 = 1 * w - gap - 2; xs[4].y1 = xs[4].y2 = h - gap - 1;
  xs[5].x1 = 1 * w + gap + 1; xs[5].x2 = 2 * w - gap - 2; xs[5].y1 = xs[5].y2 = h - gap - 1;
  if (!dir) {
    /*   -    -    -   */
    m = ((h - 1) >> 1) + 1;
    xs[6].x1 = 0 * w + gap + 3; xs[6].x2 = 1 * w - gap - 4; xs[6].y1 = xs[6].y2 = m;
    xs[7].x1 = 1 * w + gap + 3; xs[7].x2 = 2 * w - gap - 4; xs[7].y1 = xs[7].y2 = m;
    xs[8].x1 = 2 * w + gap + 3; xs[8].x2 = 3 * w - gap - 4; xs[8].y1 = xs[8].y2 = m;
  } else {
    /*   |    |    |   */
    m = ((w - 1) >> 1) + 1;
    xs[6].x1 = xs[6].x2 = 0 * w + m; xs[6].y1 = gap + 3; xs[6].y2 = h - gap - 4;
    xs[7].x1 = xs[7].x2 = 1 * w + m; xs[7].y1 = gap + 3; xs[7].y2 = h - gap - 4;
    xs[8].x1 = xs[8].x2 = 2 * w + m; xs[8].y1 = gap + 3; xs[8].y2 = h - gap - 4;
  }
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 9, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
void xitk_image_draw_paddle_three_state_vertical (xitk_image_t *img) {
  xitk_image_draw_paddle_three_state (img, 0, img->height);
}
void xitk_image_draw_paddle_three_state_horizontal (xitk_image_t *img) {
  xitk_image_draw_paddle_three_state (img, img->width / 3, 0);
}

/*
 *
 */
void xitk_image_draw_inner (xitk_image_t *img, int w, int h) {
  _xitk_image_draw_relief (img, w, h, DRAW_INNER);
}
void xitk_image_draw_outter (xitk_image_t *img, int w, int h) {
  _xitk_image_draw_relief (img, w, h, DRAW_OUTTER);
}

void xitk_image_draw_flat (xitk_image_t *img, int w, int h) {
  _xitk_image_draw_relief (img, w, h, DRAW_FLATTER | DRAW_LIGHT);
}

/*
 * Draw a frame outline with embedded title.
 */
static void _xitk_image_draw_frame (xitk_image_t *img,
                        const char *title, const char *fontname,
                        int style, int x, int y, int w, int h) {
  xitk_t        *xitk;
  xitk_font_t   *fs = NULL;
  int            yoff = 0, xstart = 0, xstop = 0;
  int            ascent = 0, descent = 0, lbearing = 0, rbearing = 0;
  size_t         titlelen = 0;
  const char    *titlebuf = NULL;
  char           buf[BUFSIZ];

  if (!img)
    return;
  if (!img->beimg)
    return;

  xitk = img->xitk;

  if(title) {
    int maxinkwidth = (w - 12);

    titlelen = strlen(title);
    titlebuf = title;

    fs = xitk_font_load_font (xitk, (fontname ? fontname : DEFAULT_FONT_12));
    xitk_font_text_extent (fs, title, titlelen, &lbearing, &rbearing, NULL, &ascent, &descent);

    /* Limit title to frame width */
    if((rbearing - lbearing) > maxinkwidth) {
      char  dots[]  = "...";
      size_t dotslen = strlen(dots);
      int   dotsrbearing;
      int   titlewidth;

      /* Cut title, append dots */
      xitk_font_text_extent(fs, dots, dotslen, NULL, &dotsrbearing, NULL, NULL, NULL);
      do {
	titlelen--;
	xitk_font_text_extent(fs, title, titlelen, NULL, NULL, &titlewidth, NULL, NULL);
	rbearing = titlewidth + dotsrbearing;
      } while((rbearing - lbearing) > maxinkwidth);
      { /* Cut possible incomplete multibyte character at the end */
	int titlewidth1;
	while((titlelen > 0) &&
	      (xitk_font_text_extent(fs, title, (titlelen - 1), NULL, NULL, &titlewidth1, NULL, NULL),
	       titlewidth1 == titlewidth))
	  titlelen--;
      }
      if (titlelen > ((int)sizeof (buf) - dotslen - 1)) /* Should never happen, */
	titlelen = (sizeof(buf) - dotslen - 1);  /* just to be sure ...  */
      strlcpy(buf, title, titlelen);
      strcat(buf, dots);
      titlelen += dotslen;
      titlebuf = buf;
    }
  }

  /* Dont draw frame box under frame title */
  if(title) {
    yoff = (ascent >> 1) + 1; /* Roughly v-center outline to ascent part of glyphs */
    xstart = 4 - 1;
    xstop = (rbearing - lbearing) + 8;
  }

  _xitk_image_draw_rectangular_box (img, x, y + yoff, xstart, xstop, w, h - yoff, style | DRAW_DOUBLE | DRAW_LIGHT);

  if (title) {
    xitk_image_draw_string (img, fs, (x - lbearing + 6), (y + ascent), titlebuf, titlelen,
        xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR));
    xitk_font_unload_font (fs);
  }

}

void xitk_image_draw_string (xitk_image_t *img, xitk_font_t *xtfs, int x, int y, const char *text, size_t nbytes, int color) {
  if (!img)
    return;
  if (!img->beimg || !img->xitk)
    return;

  xitk_font_draw_string (xtfs, img, x, y, text, nbytes, color);
}

/*
 *
 */
void xitk_image_draw_inner_frame (xitk_image_t *img, const char *title, const char *fontname,
    int x, int y, int w, int h) {
  _xitk_image_draw_frame (img, title, fontname, DRAW_INNER, x, y, w, h);
}
void xitk_image_draw_outter_frame (xitk_image_t *img, const char *title, const char *fontname,
    int x, int y, int w, int h) {
  _xitk_image_draw_frame (img, title, fontname, DRAW_OUTTER, x, y, w, h);
}

/*
 *
 */
void xitk_image_draw_tab (xitk_image_t *img) {
  int           w, h;
  xitk_be_line_t xs[10];
  xitk_be_rect_t xr[2];

  if (!img)
    return;
  if (!img->beimg)
    return;

  w = img->width / 3;
  h = img->height;

  xr[0].x = 0 * w; xr[0].w = 2 * w; xr[0].y = 0; xr[0].h = 5;
  xr[1].x = 2 * w; xr[1].w = 1 * w; xr[1].y = 0; xr[1].h = h;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_rects (img->beimg, xr, 2, xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  xr[0].x = 0 * w + 1; xr[0].w = w - 2; xr[0].y = 4; xr[0].h = h - 4;
  xr[1].x = 1 * w + 1; xr[1].w = w - 2; xr[1].y = 0; xr[1].h = h;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->fill_rects (img->beimg, xr, 2, xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /*          *  /-----  *  /-----  *
   *  /-----  * |        * |        *
   * |        * |        * |        *
   * +------- * +------- * |        */
  xs[0].x1 = 1 * w + 2; xs[0].x2 = 2 * w - 3; xs[0].y1 = xs[0].y2 = 0;
  xs[1].x1 = xs[1].x2 = 1 * w + 1;            xs[1].y1 = xs[1].y2 = 1;
  xs[2].x1 = 2 * w + 2; xs[2].x2 = 3 * w - 3; xs[2].y1 = xs[2].y2 = 0;
  xs[3].x1 = xs[3].x2 = 2 * w + 1;            xs[3].y1 = xs[3].y2 = 1;
  xs[4].x1 = 0 * w + 2; xs[4].x2 = 1 * w - 3; xs[4].y1 = xs[4].y2 = 3;
  xs[5].x1 = xs[5].x2 = 0 * w + 1;            xs[5].y1 = xs[5].y2 = 4;
  xs[6].x1 = xs[6].x2 = 0 * w;                xs[6].y1 = 5; xs[6].y2 = h - 1;
  xs[7].x1 = xs[7].x2 = 1 * w;                xs[7].y1 = 2; xs[7].y2 = h - 1;
  xs[8].x1 = xs[8].x2 = 2 * w;                xs[8].y1 = 2; xs[8].y2 = h - 1;
  xs[9].x1 = 0 * w;     xs[9].x2 = 2 * w - 1; xs[9].y1 = xs[9].y2 = h - 1;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 10, xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);

  /*          *          *          *
   *          *        | *        | *
   *        | *        | *        | *
   *        | *        | *        | */
  xs[0].x1 = xs[0].x2 = 1 * w - 1;            xs[0].y1 = 5; xs[0].y2 = h - 1;
  xs[1].x1 = xs[1].x2 = 2 * w - 1;            xs[1].y1 = 2; xs[1].y2 = h - 1;
  xs[2].x1 = xs[2].x2 = 3 * w - 1;            xs[2].y1 = 2; xs[2].y2 = h - 1;
  img->beimg->display->lock (img->beimg->display);
  img->beimg->draw_lines (img->beimg, xs, 3, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
void xitk_image_draw_paddle_rotate (xitk_image_t *img) {
  int w, h;
  unsigned int ccolor, fcolor, ncolor;

  if (!img)
    return;
  if (!img->beimg || !img->xitk)
    return;

  w = img->width / 3;
  h = img->height;
  ncolor = xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR);
  fcolor = xitk_get_cfg_num (img->xitk, XITK_WARN_BG_COLOR);
  ccolor = xitk_get_cfg_num (img->xitk, XITK_FOCUS_COLOR);

  {
    int x, i;
    unsigned int bg_colors[3] = { ncolor, fcolor, ccolor };
    xitk_be_rect_t rect[1] = {{0, 0, w * 3, h}};

    img->beimg->display->lock (img->beimg->display);
    img->beimg->fill_rects (img->beimg, rect, 1, 0, 1);

    for (x = 0, i = 0; i < 3; i++) {
      img->beimg->fill_arc (img->beimg, x, 0, w - 1, h - 1, (0 * 64), (360 * 64), 1, 1);
      img->beimg->draw_arc (img->beimg, x, 0, w - 1, h - 1, (0 * 64), (360 * 64), 1, 1);

      img->beimg->fill_arc (img->beimg, x, 0, w - 1, h - 1, (0 * 64), (360 * 64), bg_colors[i], 0);
      img->beimg->draw_arc (img->beimg, x, 0, w - 1, h - 1, (0 * 64), (360 * 64), xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);

      x += w;
    }
    img->beimg->display->unlock (img->beimg->display);
  }
}

/*
 *
 */
void xitk_image_draw_rotate_button (xitk_image_t *img) {
  int w, h;

  if (!img)
    return;
  if (!img->beimg || !img->xitk)
    return;

  w = img->width;
  h = img->height;

  img->beimg->display->lock (img->beimg->display);

  /* Draw mask */
  {
    xitk_be_rect_t rect[1] = {{0, 0, w, h}};
    img->beimg->fill_rects (img->beimg, rect, 1, 0, 1);
  }
  img->beimg->fill_arc (img->beimg, 0, 0, w - 1, h - 1, (0 * 64), (360 * 64), 1, 1);

  /* */
  img->beimg->fill_arc (img->beimg, 0, 0, w - 1, h - 1, (0 * 64), (360 * 64), xitk_get_cfg_num (img->xitk, XITK_BG_COLOR), 0);
/*img->beimg->draw_arc (img->beimg, 0, 0, w - 1, h - 1, (30 * 64), (180 * 64), xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);*/
  img->beimg->draw_arc (img->beimg, 1, 1, w - 2, h - 2, (30 * 64), (180 * 64), xitk_get_cfg_num (img->xitk, XITK_WHITE_COLOR), 0);

/*img->beimg->draw_arc (img->beimg, 0, 0, w - 1, h - 1, (210 * 64), (180 * 64), xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);*/
  img->beimg->draw_arc (img->beimg, 1, 1, w - 3, h - 3, (210 * 64), (180 * 64), xitk_get_cfg_num (img->xitk, XITK_SELECT_COLOR), 0);

  img->beimg->display->unlock (img->beimg->display);
}

/*
 *
 */
void xitk_image_draw_button_plus (xitk_image_t *img) {
  if (img && img->beimg) {
    xitk_be_line_t lines[6];
    int w, h;

    w = img->width / 3;
    h = img->height;

    lines[0].x1 =           2, lines[0].y1 = (h >> 1) - 1, lines[0].x2 =  w      - 4, lines[0].y2 = (h >> 1) - 1;
    lines[1].x1 =  w      + 2, lines[1].y1 = (h >> 1) - 1, lines[1].x2 = (w * 2) - 4, lines[1].y2 = (h >> 1) - 1;
    lines[2].x1 = (w * 2) + 3, lines[2].y1 =  h >> 1,      lines[2].x2 = (w * 3) - 3, lines[2].y2 =  h >> 1;
    lines[3].x1 = (w >> 1)           - 1, lines[3].y1 = 2, lines[3].x2 = (w >> 1)           - 1, lines[3].y2 = h - 4;
    lines[4].x1 =  w      + (w >> 1) - 1, lines[4].y1 = 2, lines[4].x2 =  w      + (w >> 1) - 1, lines[4].y2 = h - 4;
    lines[5].x1 = (w * 2) + (w >> 1),     lines[5].y1 = 3, lines[5].x2 = (w * 2) + (w >> 1),     lines[5].y2 = h - 3;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->draw_lines (img->beimg, lines, 6, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

/*
 *
 */
void xitk_image_draw_button_minus (xitk_image_t *img) {
  if (img && img->beimg) {
    xitk_be_line_t lines[3];
    int w, h;

    w = img->width / 3;
    h = img->height;

    lines[0].x1 =           2, lines[0].y1 = (h >> 1) - 1, lines[0].x2 =  w      - 4, lines[0].y2 = (h >> 1) - 1;
    lines[1].x1 =  w      + 2, lines[1].y1 = (h >> 1) - 1, lines[1].x2 = (w * 2) - 4, lines[1].y2 = (h >> 1) - 1;
    lines[2].x1 = (w * 2) + 3, lines[2].y1 =  h >> 1,      lines[2].x2 = (w * 3) - 3, lines[2].y2 = h >> 1;
    img->beimg->display->lock (img->beimg->display);
    img->beimg->draw_lines (img->beimg, lines, 3, xitk_get_cfg_num (img->xitk, XITK_BLACK_COLOR), 0);
    img->beimg->display->unlock (img->beimg->display);
  }
}

void xitk_part_image_copy (xitk_widget_list_t *wl, xitk_part_image_t *from, xitk_part_image_t *to,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y) {

  if (!wl || !from || !to)
    return;
  if (!from->image || !to->image)
    return;
  if (!from->image->xitk || !from->image->beimg || !to->image->beimg)
    return;

  to->image->beimg->copy_rect (to->image->beimg, from->image->beimg,
    from->x + src_x, from->y + src_y, width, height, to->x + dst_x, to->y + dst_y);
}

void xitk_part_image_draw (xitk_widget_list_t *wl, xitk_part_image_t *origin, xitk_part_image_t *copy,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
  if (!wl || !origin)
    return;
  if (!wl->xwin)
    return;
  if (!wl->xwin->bewin)
    return;
  if (!origin->image)
    return;

  if (copy) {
    if (!copy->image)
      return;
  } else {
    copy = origin;
  }
  if (!copy->image->beimg)
    return;

  wl->xwin->bewin->copy_rect (wl->xwin->bewin, copy->image->beimg,
    copy->x + src_x, copy->y + src_y, width, height, dst_x, dst_y, 0);
}

void xitk_image_draw_image (xitk_widget_list_t *wl, xitk_image_t *img,
  int src_x, int src_y, int width, int height, int dst_x, int dst_y, int sync) {
  if (!wl || !img)
    return;
  if (!wl->xwin)
    return;
  if (!wl->xwin->bewin)
    return;
  if (!img->beimg)
    return;

  wl->xwin->bewin->copy_rect (wl->xwin->bewin, img->beimg,
    src_x, src_y, width, height, dst_x, dst_y, sync);
}

int xitk_image_width(xitk_image_t *i) {
  return i->width;
}

int xitk_image_height(xitk_image_t *i) {
  return i->height;
}

/*
 * ********************************************************************************
 *                              Widget specific part
 * ********************************************************************************
 */

/*
 *
 */
static void _notify_destroy (_image_private_t *wp) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    if (!wp->skin_element_name)
      xitk_image_free_image (&(wp->skin));
    XITK_FREE (wp->skin_element_name);
  }
}

/*
 *
 */
static int _notify_inside (_image_private_t *wp, int x, int y) {
  (void)wp;
  (void)x;
  (void)y;
  return 0;
}

/*
 *
 */
static xitk_image_t *_get_skin (_image_private_t *wp, int sk) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)) {
    if ((sk == BACKGROUND_SKIN) && wp->skin)
      return wp->skin;
  }
  return NULL;
}

/*
 *
 */
static void _paint_image (_image_private_t *wp, widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.image.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (wp->w.visible == 1)) {
    xitk_image_draw_image (wp->w.wl, wp->skin,
        event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y, 0);
  }
}

/*
 *
 */
static void _notify_change_skin (_image_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE) && (wp->w.visible == 1))) {
    if (wp->skin_element_name) {
      const xitk_skin_element_info_t *info;

      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, wp->skin_element_name);
      if (info) {
        wp->skin = info->pixmap_img.image;
        wp->w.x       = info->x;
        wp->w.y       = info->y;
        wp->w.width   = wp->skin->width;
        wp->w.height  = wp->skin->height;
      }
      xitk_skin_unlock(skonfig);

      xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    }
  }
}

static int _notify_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _image_private_t *wp;
  int retval = 0;

  xitk_container (wp, w, w);
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = wp->w.x;
      event->y = wp->w.y;
      event->width = wp->w.width;
      event->height = wp->w.height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      _paint_image (wp, event);
      break;
    case WIDGET_EVENT_INSIDE:
      result->value = _notify_inside (wp, event->x, event->y);
      retval = 1;
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _notify_change_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _notify_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _get_skin (wp, event->skin_layer);
        retval = 1;
      }
      break;
    default: ;
  }
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_image_create (xitk_widget_list_t *wl,
					  xitk_image_widget_t *im,
					  int x, int y,
					  const char *skin_element_name,
					  xitk_image_t *skin) {
  _image_private_t *wp;

  ABORT_IF_NULL(wl);

  wp = (_image_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->skin_element_name = (skin_element_name == NULL) ? NULL : strdup (im->skin_element_name);

  wp->skin              = skin;

  wp->w.enable          = 0;
  wp->w.visible         = 0;
  wp->w.x               = x;
  wp->w.y               = y;
  wp->w.width           = wp->skin->width;
  wp->w.height          = wp->skin->height;
  wp->w.type            = WIDGET_TYPE_IMAGE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = _notify_event;

  return &wp->w;
}

xitk_widget_t *xitk_image_create (xitk_widget_list_t *wl,
				  xitk_skin_config_t *skonfig, xitk_image_widget_t *im) {
  const xitk_skin_element_info_t *info;

  XITK_CHECK_CONSTITENCY(im);

  info = xitk_skin_get_info (skonfig, im->skin_element_name);
  if (!info)
    return NULL;
  return _xitk_image_create (wl, im, info->x, info->y, im->skin_element_name, info->pixmap_img.image);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_image_create (xitk_widget_list_t *wl,
					 xitk_image_widget_t *im,
					 xitk_image_t *image, int x, int y) {
  XITK_CHECK_CONSTITENCY(im);

  return _xitk_image_create (wl, im, x, y, NULL, image);
}
