/*
 * Copyright (C) 2000-2022 the xine project
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>

#include <xine/sorted_array.h>

#include "utils.h"
#include "_xitk.h"
#include "slider.h"

#undef DEBUG_SKIN

/************************************************************************************
 *                                     PRIVATES
 ************************************************************************************/

typedef struct {
  /* this needs to stay first. */
  char                          section[64];

  xitk_skin_element_info_t      info;
} xitk_skin_element_t;

struct xitk_skin_config_s {
  xitk_t              *xitk;
  int                  version;
  uint32_t             plen;
  /* offsets into sbuf */
  uint32_t             path;
  uint32_t             skinfile;
  uint32_t             name;
  uint32_t             author;
  uint32_t             date;
  uint32_t             url;
  uint32_t             logo;
  uint32_t             animation;
  uint32_t             load_command;
  uint32_t             unload_command;

  xine_sarray_t       *elements;
  xitk_skin_element_t *celement;

  xine_sarray_t       *imgs;

  pthread_mutex_t      skin_mutex;

  uint32_t             sbuf_pos;
  char                 sbuf[8192];
};

static ATTR_INLINE_ALL_STRINGOPS uint32_t _skin_strdup (xitk_skin_config_t *skonfig, const char *s) {
  uint32_t r = skonfig->sbuf_pos;
  uint32_t l = strlen (s) + 1;
  if (l > sizeof (skonfig->sbuf) - r)
    return 0;
  memcpy (skonfig->sbuf + r, s, l);
  skonfig->sbuf_pos = r + l;
  return r;
}

typedef struct {
  const char *name, *format;
  xitk_image_t *image;
} xitk_skin_img_t;

static int xitk_simg_cmp (void *a, void *b) {
  xitk_skin_img_t *d = a, *e = b;
  return strcmp (d->name, e->name);
}

/*
 *
 */
static void _skin_load_img (xitk_skin_config_t *skonfig, xitk_part_image_t *image, const char *pixmap, const char *format) {
  char nbuf[2048], *key;
  size_t nlen;
  int try;

  if (!skonfig || !pixmap)
    return;

  /* for part read and database keys, omit path. */
  nbuf[0] = 0;
  nlen = xitk_find_byte (pixmap, 0);
  if (2 + skonfig->plen + 1 + nlen + 1 > sizeof (nbuf))
    return;
  key = nbuf + 2 + skonfig->plen + 1;
  memcpy (key, pixmap, nlen + 1);

  if (image) {
    image->x = image->y = image->width = image->height = 0;
    image->image = NULL;
  }

  do {
    uint32_t v;
    uint8_t z;
    const uint8_t *p;
    char *part = strchr (key, '|');

    if (!part)
      break;

    nlen = part - key;
    *part++ = 0;
    if (!image)
      break;

    p = (const uint8_t *)part;
    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    image->x = v;
    if (*p == ',')
      p++;
    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    image->y = v;
    if (*p == ',')
      p++;
    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    image->width = v;
    v = 0;
    if (*p == ',')
      p++;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    image->height = v;
  } while (0);

  for (try = 2; try > 0; try--) {
    xitk_skin_img_t *si = NULL;
    /* already loaded? */
    {
      xitk_skin_img_t here;
      int pos;
      here.name = key;
      pos = xine_sarray_binary_search (skonfig->imgs, &here);
      if (pos >= 0)
        si = xine_sarray_get (skonfig->imgs, pos);
    }
    /* load now */
    if (!si) {
      char *nmem = malloc (sizeof (*si) + nlen + 1);
      if (!nmem)
        return;
      si = (xitk_skin_img_t *)nmem;
      nmem += sizeof (*si);
      memcpy (nmem, key, nlen + 1);
      si->name = nmem;
      si->format = format;
      si->image = NULL;
      xine_sarray_add (skonfig->imgs, si);
    } else {
      if (format)
        si->format = format;
    }
    if (!image)
      return;
    if (!si->image) {
      if (!nbuf[0]) {
        memcpy (nbuf, "//", 2);
        memcpy (nbuf + 2, skonfig->sbuf + skonfig->path, skonfig->plen + 1);
        key[-1] = '/';
      }
      si->image = xitk_image_new (skonfig->xitk, nbuf + 2, 0, 0, 0);
    }
    if (si->image) {
      image->image = si->image;
      if (si->format)
        xitk_image_set_pix_font (si->image, si->format);
      break;
    }
    /* try fallback texture */
    do
      key--;
    while (key[-1] != '/');
    memcpy (key, "missing.png", 12);
    nlen = 11;
    image->x = 0;
    image->y = 0;
    image->width = 0;
    image->height = 0;
    format = NULL;
  }

  if (image->x < 0)
    image->x = 0;
  if (image->y < 0)
    image->x = 0;
  if (image->width <= 0)
    image->width = image->image->width;
  if (image->height <= 0)
    image->height = image->image->height;
  if  ((image->x + image->width > image->image->width)
    || (image->y + image->height > image->image->height)) {
    image->x = 0;
    image->x = 0;
    image->width = image->image->width;
    image->height = image->image->height;
  }
}

static void skin_free_imgs (xitk_skin_config_t *skonfig) {
  int n;

  for (n = xine_sarray_size (skonfig->imgs) - 1; n >= 0; n--) {
    xitk_skin_img_t *simg = xine_sarray_get (skonfig->imgs, n);
    xitk_image_free_image (&simg->image);
    free (simg);
  }
  xine_sarray_clear (skonfig->imgs);
}

/* 1 = \0; 2 = 0-9; 4 = A-Z,a-z,_; 8 = $; 16 = (,{; 32 = ),}; */
static const uint8_t _tab_char[256] = {
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 8, 0, 0, 0,16,32, 0, 0, 0, 0, 0, 0,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
   0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 4,
   0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,16, 0,32, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 *
 */
//#warning FIXME
static uint32_t _expanded (xitk_skin_config_t *skonfig, const char *cmd) {
  const uint8_t *p = (const uint8_t *)cmd;
  uint8_t buf[2048], *q = buf, *e = buf + sizeof (buf) - 1;
  int num_vars = 0;

  while (q < e) {
    const uint8_t *start, *stop;
    const char *val = NULL;
    int l;
    start = p;
    while (!(_tab_char[*p] & (1 | 8))) /* \0, $ */
      p++;
    /* literal part */
    l = p - start;
    if (l > e - q)
      l = e - q;
    if (l > 0) {
      memcpy (q, start, l);
      q += l;
    }
    /* eot */
    if (!*p)
      break;
    /* $ */
    if ((_tab_char[p[1]] & 16) && (_tab_char[p[2]] & 4)) {
      /* ${VARNAME}, $(VARNAME) */
      p += 2;
      start = p;
      while (_tab_char[*p] & (2 | 4)) /* 0-9, A-Z, a-z, _ */
        p++;
      stop = p;
      while (!(_tab_char[*p] & (1 | 32))) /* \0, ), } */
        p++;
      if (*p)
        p++;
    } else if (_tab_char[p[1]] & 4) {
      /* $VARNAME */
      p += 1;
      start = p;
      while (_tab_char[*p] & (2 | 4)) /* 0-9, A-Z, a-z, _ */
        p++;
      stop = p;
    } else {
      if (q < e)
        *q++ = *p;
      p++;
      continue;
    }
    num_vars += 1;
    l = stop - start;
    if ((l == 16) && !strncmp ((const char *)start, "SKIN_PARENT_PATH", 16)) {
      if (skonfig->path) {
        const char *z = strrchr (skonfig->sbuf + skonfig->path, '/');
        l = z ? z - (skonfig->sbuf + skonfig->path) : (int)skonfig->plen;
        if (l > e - q)
          l = e - q;
        if (l > 0) {
          memcpy (q, skonfig->sbuf + skonfig->path, l); q += l;
        }
      }
    } else if ((l == 12) && !strncmp ((const char *)start, "SKIN_VERSION", 12)) {
      if (skonfig->version >= 0) {
        q += snprintf ((char *)q, e - q, "%d", skonfig->version);
      }
    } else if ((l == 11) && !strncmp ((const char *)start, "SKIN_AUTHOR", 11)) {
      val = skonfig->sbuf + skonfig->author;
    } else if ((l == 9) && !strncmp ((const char *)start, "SKIN_PATH", 9)) {
      val = skonfig->sbuf + skonfig->path;
    } else if ((l == 9) && !strncmp ((const char *)start, "SKIN_NAME", 9)) {
      val = skonfig->sbuf + skonfig->name;
    } else if ((l == 9) && !strncmp ((const char *)start, "SKIN_DATE", 9)) {
      val = skonfig->sbuf + skonfig->date;
    } else if ((l == 8) && !strncmp ((const char *)start, "SKIN_URL", 8)) {
      val = skonfig->sbuf + skonfig->url;
    } else if ((l == 4) && !strncmp ((const char *)start, "HOME", 4)) {
      val = xine_get_homedir ();
    }
    if (val) {
      l = xitk_find_byte (val, 0);
      if (l > e - q)
        l = e - q;
      if (l > 0) {
        memcpy (q, val, l); q += l;
      }
    }
  }
  *q = 0;
  if (num_vars > 0)
    return _skin_strdup (skonfig, (const char *)buf);
  return 0;
}

/*
 * Nullify all entried from s element.
 */
static void _nullify_me(xitk_skin_element_t *s) {
  s->section[0]                  = 0;
  s->info.pixmap_name            = NULL;
  s->info.pixmap_img.image       = NULL;
  s->info.slider_pixmap_pad_name = NULL;
  s->info.slider_pixmap_pad_img.image = NULL;
  s->info.label_pixmap_font_name = NULL;
  s->info.label_pixmap_font_img  = NULL;
  s->info.label_pixmap_highlight_font_name = NULL;
  s->info.label_pixmap_highlight_font_img = NULL;
  s->info.label_color            = 0;
  s->info.label_color_focus      = 0;
  s->info.label_color_click      = 0;
  s->info.label_fontname         = NULL;
  s->info.browser_entries        = -2;
  s->info.direction              = DIRECTION_LEFT; /* Compatibility */
  s->info.max_buttons            = 0;
}

/*
 * Return alignement value.
 */
static int skin_get_align_value (const char *val) {
  union {
    char b[4];
    uint32_t v;
  } _left = {{'l','e','f','t'}},
    _right = {{'r','i','g','h'}},
    _have;

  /* _not_ an overread here. */
  memcpy (_have.b, val, 4);
  _have.v |= 0x20202020;
  if (_have.v == _left.v)
    return ALIGN_LEFT;
  if (_have.v == _right.v)
    return ALIGN_RIGHT;
  return ALIGN_CENTER;
}

/*
 * Return direction
 */
static int skin_get_direction (const char *val) {
  union {
    char b[4];
    uint32_t v;
  } _down  = {{'d','o','w','n'}},
    _right = {{'r','i','g','h'}},
    _up    = {{'u','p',' ',' '}},
    _have;

  /* _not_ an overread here. */
  memcpy (_have.b, val, 4);
  if (!_have.b[2])
    _have.b[3] = 0;
  _have.v |= 0x20202020;
  if (_have.v == _right.v)
    return DIRECTION_RIGHT;
  if (_have.v == _down.v)
    return DIRECTION_DOWN;
  if (_have.v == _up.v)
    return DIRECTION_UP;
  return DIRECTION_LEFT;
}

/*
 * Return slider typw
 */
static int skin_get_slider_type (const char *val) {
  union {
    char b[4];
    uint32_t v;
  } _vert = {{'v','e','r','t'}}, /* "vertical" */
    _rota = {{'r','o','t','a'}}, /* "rotate" */
    _have;

  /* _not_ an overread here. */
  memcpy (_have.b, val, 4);
  _have.v |= 0x20202020;
  if (_have.v == _vert.v)
    return XITK_VSLIDER;
  if (_have.v == _rota.v)
    return XITK_RSLIDER;
  return XITK_HSLIDER;
}

#ifdef DEBUG_SKIN
/*
 * Just to check list chained constitency.
 */
static void check_skonfig(xitk_skin_config_t *skonfig) {
  int n;

  ABORT_IF_NULL(skonfig);

  n = xine_sarray_size (skonfig->elements);
  if (n) {
    int i;

    printf("Skin name '%s'\n",      skonfig->sbuf + skonfig->name);
    printf("     version   '%d'\n", skonfig->version);
    printf("     author    '%s'\n", skonfig->sbuf + skonfig->author);
    printf("     date      '%s'\n", skonfig->sbuf + skonfig->date);
    printf("     load cmd  '%s'\n", skonfig->sbuf + skonfig->load_command);
    printf("     uload cmd '%s'\n", skonfig->sbuf + skonfig->unload_command);
    printf("     URL       '%s'\n", skonfig->sbuf + skonfig->url);
    printf("     logo      '%s'\n", skonfig->sbuf + skonfig->logo);
    printf("     animation '%s'\n", skonfig->sbuf + skonfig->animation);

    for (i = 0; i < n; i++) {
      xitk_skin_element_t *s = xine_sarray_get (skonfig->elements, i);

      printf("Section '%s'\n", s->section);
      printf("  enable      = %d\n", s->info.enability);
      printf("  visible     = %d\n", s->info.visibility);
      printf("  X           = %d\n", s->info.x);
      printf("  Y           = %d\n", s->info.y);
      printf("  direction   = %d\n", s->info.direction);
      printf("  pixmap      = '%s'\n", s->info.pixmap_name);

      if (s->info.slider_type) {
	printf("  slider type = %d\n", s->info.slider_type);
	printf("  pad pixmap  = '%s'\n", s->info.slider_pixmap_pad_name);
      }

      if (s->info.browser_entries > -1)
	printf("  browser entries = %d\n", s->info.browser_entries);

      printf("  animation   = %d\n", s->info.label_animation);
      printf("  step        = %d\n", s->info.label_animation_step);
      printf("  print       = %d\n", s->info.label_printable);
      printf("  static      = %d\n", s->info.label_staticity);
      printf("  length      = %d\n", s->info.label_length);
      printf("  color       = '#%06x'\n", (unsigned int)s->info.label_color);
      printf("  color focus = '#%06x'\n", (unsigned int)s->info.label_color_focus);
      printf("  color click = '#%06x'\n", (unsigned int)s->info.label_color_click);
      printf("  pixmap font = '%s'\n", s->info.label_pixmap_font_name);
      printf("  pixmap highlight_font = '%s'\n", s->info.label_pixmap_highlight_font_name);
      printf("  pixmap fmt  = '%s'\n", s->info.label_pixmap_font_format);
      printf("  font        = '%s'\n", s->info.label_fontname);
      printf("  max_buttons = %d\n", s->info.max_buttons);
    }

  }
}
#endif

static xitk_skin_element_t *skin_lookup_section(xitk_skin_config_t *skonfig, const char *str) {
  char name[64];
  xitk_skin_element_t *s;
  int r;

  if (!skonfig || !str)
    return NULL;

  xitk_lower_strlcpy (name, str, sizeof (name));
  r = xine_sarray_binary_search (skonfig->elements, name);
  if (r < 0) {
    if (skonfig->xitk->verbosity >= 1)
      printf ("xitk.skin.section.missing (%s, %s).\n", skonfig->sbuf + skonfig->name, str);
    return NULL;
  }
  s = xine_sarray_get (skonfig->elements, r);

  if (!s->info.pixmap_img.image && s->info.pixmap_name) {
    _skin_load_img (skonfig, &s->info.pixmap_img, s->info.pixmap_name, NULL);
  }
  if (!s->info.label_pixmap_font_img && s->info.label_pixmap_font_name) {
    xitk_part_image_t image;
    _skin_load_img (skonfig, &image, s->info.label_pixmap_font_name, s->info.label_pixmap_font_format);
    s->info.label_pixmap_font_img = image.image;
  }
  if (!s->info.label_pixmap_highlight_font_img && s->info.label_pixmap_highlight_font_name) {
    xitk_part_image_t image;
    _skin_load_img (skonfig, &image, s->info.label_pixmap_highlight_font_name, s->info.label_pixmap_font_format);
    s->info.label_pixmap_highlight_font_img = image.image;
  }
  if (!s->info.slider_pixmap_pad_img.image && s->info.slider_pixmap_pad_name) {
    _skin_load_img (skonfig, &s->info.slider_pixmap_pad_img, s->info.slider_pixmap_pad_name, NULL);
  }

  return s;
}

/************************************************************************************
 *                                   END OF PRIVATES
 ************************************************************************************/

/*
 * Alloc a xitk_skin_config_t* memory area, nullify pointers.
 */
xitk_skin_config_t *xitk_skin_init_config(xitk_t *xitk) {
  xitk_skin_config_t *skonfig;

  if((skonfig = (xitk_skin_config_t *) xitk_xmalloc(sizeof(xitk_skin_config_t))) == NULL) {
    XITK_DIE("xitk_xmalloc() failed: %s\n", strerror(errno));
  }

  pthread_mutex_init (&skonfig->skin_mutex, NULL);
  skonfig->sbuf_pos = 1;
  skonfig->sbuf[0] = 0;

  skonfig->elements = xine_sarray_new (128, (xine_sarray_comparator_t)strcmp);
  skonfig->imgs     = xine_sarray_new (128, xitk_simg_cmp);

  skonfig->xitk     = xitk;
  skonfig->version  = -1;
  skonfig->celement = NULL;
  skonfig->name     =
  skonfig->author   =
  skonfig->date     =
  skonfig->url      =
  skonfig->load_command =
  skonfig->unload_command =
  skonfig->logo     =
  skonfig->animation =
  skonfig->skinfile =
  skonfig->path     = 0;
  skonfig->plen     = 0;

  return skonfig;
}

/*
 * Release all allocated memory of a xitk_skin_config_t* variable (element chained struct too).
 */
static void xitk_skin_free_config(xitk_skin_config_t *skonfig) {
  pthread_mutex_lock (&skonfig->skin_mutex);
  {
    int n = xine_sarray_size (skonfig->elements);
    for (n--; n >= 0; n--) {
      xitk_skin_element_t *s = xine_sarray_get (skonfig->elements, n);
      XITK_FREE(s);
    }
  }
  xine_sarray_delete (skonfig->elements);

  skin_free_imgs (skonfig);
  xine_sarray_delete (skonfig->imgs);

  pthread_mutex_unlock (&skonfig->skin_mutex);
  pthread_mutex_destroy (&skonfig->skin_mutex);

  XITK_FREE(skonfig);
}

typedef enum {
  _K_NONE = 0,
  _K_align,
  _K_animation,
  _K_author,
  _K_browser,
  _K_color,
  _K_color_click,
  _K_color_focus,
  _K_coords,
  _K_date,
  _K_direction,
  _K_enable,
  _K_entries,
  _K_font,
  _K_label,
  _K_length,
  _K_load_command,
  _K_logo,
  _K_max_buttons,
  _K_name,
  _K_pixmap,
  _K_pixmap_focus,
  _K_pixmap_format,
  _K_print,
  _K_radius,
  _K_slider,
  _K_static,
  _K_step,
  _K_timer,
  _K_type,
  _K_unload_command,
  _K_url,
  _K_version,
  _K_visible,
  _K_x,
  _K_y,
  _K_LAST
} _skin_key_t;

static _skin_key_t _skin_key_index (const char *key, unsigned int klen) {
  switch (klen) {
    int d;
    case 1:
      if (key[0] == 'x')
        return _K_x;
      if (key[0] == 'y')
        return _K_y;
      break;
    case 3:
      if (!memcmp (key, "url", 3))
        return _K_url;
      break;
    case 4:
      d = memcmp (key, "logo", 4);
      if (d == 0)
        return _K_logo;
      if (d < 0) {
        if (!memcmp (key, "date", 4))
          return _K_date;
        if (!memcmp (key, "font", 4))
          return _K_font;
      } else {
        if (!memcmp (key, "name", 4))
          return _K_name;
        if (!memcmp (key, "step", 4))
          return _K_step;
        if (!memcmp (key, "type", 4))
          return _K_type;
      }
      break;
    case 5:
      d = memcmp (key, "label", 5);
      if (d == 0)
        return _K_label;
      if (d < 0) {
        if (!memcmp (key, "align", 5))
          return _K_align;
        if (!memcmp (key, "color", 5))
          return _K_color;
      } else {
        if (!memcmp (key, "print", 5))
          return _K_print;
        if (!memcmp (key, "timer", 5))
          return _K_timer;
      }
      break;
    case 6:
      d = memcmp (key, "length", 6);
      if (d == 0)
        return _K_length;
      if (d < 0) {
        d = memcmp (key, "coords", 6);
        if (d == 0)
          return _K_coords;
        if (d < 0) {
          if (!memcmp (key, "author", 6))
            return _K_author;
        } else {
          if (!memcmp (key, "enable", 6))
            return _K_enable;
        }
      } else {
        d = memcmp (key, "slider", 6);
        if (d == 0)
          return _K_slider;
        if (d < 0) {
          if (!memcmp (key, "radius", 6))
            return _K_radius;
          if (!memcmp (key, "pixmap", 6))
            return _K_pixmap;
        } else {
          if (!memcmp (key, "static", 6))
            return _K_static;
        }
      }
      break;
    case 7:
      if (!memcmp (key, "browser", 7))
        return _K_browser;
      if (!memcmp (key, "entries", 7))
        return _K_entries;
      if (!memcmp (key, "version", 7))
        return _K_version;
      if (!memcmp (key, "visible", 7))
        return _K_visible;
      break;
    case 9:
      if (!memcmp (key, "animation", 9))
        return _K_animation;
      if (!memcmp (key, "direction", 9))
        return _K_direction;
      break;
    case 11:
      if (!memcmp (key, "color_click", 11))
        return _K_color_click;
      if (!memcmp (key, "color_focus", 11))
        return _K_color_focus;
      if (!memcmp (key, "max_buttons", 11))
        return _K_max_buttons;
      break;
    case 12:
      if (!memcmp (key, "load_command", 12))
        return _K_load_command;
      if (!memcmp (key, "pixmap_focus", 12))
        return _K_pixmap_focus;
      break;
    case 13:
      if (!memcmp (key, "pixmap_format", 13))
        return _K_pixmap_format;
      break;
    case 14:
      if (!memcmp (key, "unload_command", 14))
        return _K_unload_command;
      break;
    default: ;
  }
  return _K_NONE;
}

typedef enum {
  _KT_string = 0,
  _KT_int,
  _KT_bool,
  _KT_color,
  _KT_LAST
} _skin_key_type_t;

static const uint8_t _skin_key_types[_K_LAST] = {
  /* [_K_animation] can be string or bool. */
  [_K_color] = _KT_color,
  [_K_color_click] = _KT_color,
  [_K_color_focus] = _KT_color,
  [_K_enable] = _KT_bool,
  [_K_entries] = _KT_int,
  [_K_length] = _KT_int,
  [_K_max_buttons] = _KT_int,
  [_K_print] = _KT_bool,
  [_K_radius] = _KT_int,
  [_K_static] = _KT_bool,
  [_K_step] = _KT_int,
  [_K_timer] = _KT_int,
  [_K_version] = _KT_int,
  [_K_visible] = _KT_bool,
  [_K_x] = _KT_int,
  [_K_y] = _KT_int
};

static void _skin_parse_2 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree, xitk_cfg_parse_t *sub) {
  const char *val = text + sub->value;
  xitk_skin_element_t *s = skonfig->celement;
  _skin_key_t n = sub->key;

  switch (n) {
    case _K_browser:
      {
        xitk_cfg_parse_t *sub2;

        for (sub2 = tree + sub->first_child; sub2 != tree; sub2 = tree + sub2->next) {
          if ((_skin_key_t)sub2->key == _K_entries)
            s->info.browser_entries = sub2->value;
        }
      }
      break;
    case _K_coords:
      {
        xitk_cfg_parse_t *sub2;

        for (sub2 = tree + sub->first_child; sub2 != tree; sub2 = tree + sub2->next) {
          n = sub2->key;
          switch (n) {
            case _K_x:
              s->info.x = sub2->value;
              break;
            case _K_y:
              s->info.y = sub2->value;
              break;
            default: ;
          }
        }
      }
      break;
    case _K_direction:
      s->info.direction = skin_get_direction (val);
      break;
    case _K_enable:
      s->info.enability = sub->value;
      break;
    case _K_label:
      {
        xitk_cfg_parse_t *sub2;

        s->info.label_y = 0;
        s->info.label_printable = 1;
        s->info.label_animation_step = 1;
        s->info.label_animation_timer = xitk_get_cfg_num (skonfig->xitk, XITK_TIMER_LABEL_ANIM);
        s->info.label_alignment = ALIGN_CENTER;

        for (sub2 = tree + sub->first_child; sub2 != tree; sub2 = tree + sub2->next) {
          const char *val2 = text + sub2->value;

          n = sub2->key;
          switch (n) {
	    case _K_align:
              s->info.label_alignment = skin_get_align_value (val2);
              break;
	    case _K_animation:
              s->info.label_animation = xitk_get_bool_value (val2);
              break;
	    case _K_color:
              s->info.label_color = sub2->value;
              break;
	    case _K_color_click:
              s->info.label_color_click = sub2->value;
              break;
	    case _K_color_focus:
              s->info.label_color_focus = sub2->value;
              break;
	    case _K_font:
              s->info.label_fontname = skonfig->sbuf + _skin_strdup (skonfig, val2);
              break;
	    case _K_length:
              s->info.label_length = sub2->value;
              break;
	    case _K_pixmap:
              s->info.label_pixmap_font_name = skonfig->sbuf + _skin_strdup (skonfig, val2);
              if (s->info.label_pixmap_font_name[0])
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_font_name, s->info.label_pixmap_font_format);
              break;
	    case _K_pixmap_focus:
              s->info.label_pixmap_highlight_font_name = skonfig->sbuf + _skin_strdup (skonfig, val2);
              if (s->info.label_pixmap_highlight_font_name[0])
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_highlight_font_name, s->info.label_pixmap_font_format);
              break;
	    case _K_pixmap_format:
              if (!s->info.label_pixmap_font_format) {
                s->info.label_pixmap_font_format = skonfig->sbuf + _skin_strdup (skonfig, val2);
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_font_name, s->info.label_pixmap_font_format);
              }
              break;
	    case _K_print:
              s->info.label_printable = sub2->value;
              break;
	    case _K_static:
              s->info.label_staticity = sub2->value;
              break;
	    case _K_step:
              s->info.label_animation_step = sub2->value;
              break;
	    case _K_timer:
              s->info.label_animation_timer = sub2->value;
              break;
	    case _K_y:
              s->info.label_y = sub2->value;
              break;
            default: ;
          }
        }
      }
      break;
    case _K_max_buttons:
      s->info.max_buttons = sub->value;
      break;
    case _K_pixmap:
      s->info.pixmap_name = skonfig->sbuf + _skin_strdup (skonfig, val);
      break;
    case _K_slider:
      {
        xitk_cfg_parse_t *sub2;

        for (sub2 = tree + sub->first_child; sub2 != tree; sub2 = tree + sub2->next) {
          const char *val2 = text + sub2->value;

          n = sub2->key;
          switch (n) {
            case _K_pixmap:
              skonfig->celement->info.slider_pixmap_pad_name = skonfig->sbuf + _skin_strdup (skonfig, val2);
              break;
            case _K_radius:
              skonfig->celement->info.slider_radius = sub2->value;
              break;
            case _K_type:
              s->info.slider_type = skin_get_slider_type (val2);
              break;
            default: ;
          }
        }
      }
      break;
    case _K_visible:
      s->info.visibility = sub->value;
      break;
    default: ;
  }
}

static void _skin_parse_1 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree, xitk_cfg_parse_t *entry) {
  const char *val = text + entry->value;

  if (entry->first_child) {
    const char *key = text + entry->key;
    xitk_skin_element_t *s = xitk_xmalloc (sizeof (*s));

    if (s) {
      xitk_cfg_parse_t *sub;

      _nullify_me (s);
      skonfig->celement = s;
      /* xitk_cfg_parse () already lowercased this key. */
      strlcpy (s->section, key, sizeof (s->section));
      s->info.visibility = s->info.enability = 1;
      xine_sarray_add (skonfig->elements, s);

      for (sub = tree + entry->first_child; sub != tree; sub = tree + sub->next) {
        _skin_parse_2 (skonfig, text, tree, sub);
      }
    }
  } else {
    _skin_key_t n = entry->key;
    switch (n) {
      case _K_animation:
        skonfig->animation = _skin_strdup (skonfig, val);
        break;
      case _K_author:
        skonfig->author = _skin_strdup (skonfig, val);
        break;
      case _K_date:
        skonfig->date = _skin_strdup (skonfig, val);
        break;
      case _K_load_command:
        skonfig->load_command = _expanded (skonfig, val);
        break;
      case _K_logo:
        skonfig->logo = _expanded (skonfig, val);
        break;
      case _K_name:
        skonfig->name = _skin_strdup (skonfig, val);
        break;
      case _K_unload_command:
        skonfig->unload_command = _expanded (skonfig, val);
        break;
      case _K_url:
        skonfig->url = _skin_strdup (skonfig, val);
        break;
      case _K_version:
        skonfig->version = entry->value;
        break;
      default: ;
    }
  }
}

static void _skin_parse_0 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree) {
  xitk_cfg_parse_t *entry;

  for (entry = tree + tree->first_child; entry != tree; entry = tree + entry->next) {

    if (entry->key >= 0)
      _skin_parse_1 (skonfig, text, tree, entry);
  }
}

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *skonfig, const char *path, const char *filename) {
  char buf[2048], *text;
  size_t fsize = 2 << 20;
  xitk_cfg_parse_t *tree;

  ABORT_IF_NULL(skonfig);
  ABORT_IF_NULL(path);
  ABORT_IF_NULL(filename);

  pthread_mutex_lock (&skonfig->skin_mutex);

  skonfig->path     = _skin_strdup (skonfig, path);
  skonfig->plen     = skonfig->sbuf_pos - skonfig->path - 1;
  skonfig->skinfile = _skin_strdup (skonfig, filename);

  snprintf(buf, sizeof(buf), "%s/%s", path, filename);
  text = xitk_cfg_load (buf, &fsize);
  if (!text) {
    XITK_WARNING ("%s(): Unable to open '%s' file.\n", __FUNCTION__, skonfig->sbuf + skonfig->skinfile);
    pthread_mutex_unlock (&skonfig->skin_mutex);
    return 0;
  }

  tree = xitk_cfg_parse (text, /* XITK_CFG_PARSE_DEBUG */ 0);
  if (!tree) {
    xitk_cfg_unload (text);
    pthread_mutex_unlock (&skonfig->skin_mutex);
    return 0;
  }

  {
    int i;

    for (i = 1; i < tree[0].key; i++) {
      if (tree[i].level == 1) {
        if (memcmp (text + tree[i].key, "skin.", 5)) {
          tree[i].key = -1;
          continue;
        }
        tree[i].key += 5, tree[i].klen -= 5;
        if (tree[i].first_child)
          continue;
      }
      tree[i].key = _skin_key_index (text + tree[i].key, tree[i].klen);
      switch (_skin_key_types[tree[i].key]) {
        const char *v;

        case _KT_int:
          v = text + tree[i].value;
          tree[i].value = xitk_str2int32 (&v);
          break;
        case _KT_bool:
          v = text + tree[i].value;
          tree[i].value = xitk_get_bool_value (v);
          break;
        case _KT_color:
          v = text + tree[i].value;
          tree[i].value = xitk_get_color_name (v);
          break;
        default: ;
      }
    }
  }

  _skin_parse_0 (skonfig, text, tree);

  xitk_cfg_unparse (tree);
  xitk_cfg_unload (text);

  if (!skonfig->celement) {
    XITK_WARNING("%s(): no valid skin element found in '%s/%s'.\n",
		 __FUNCTION__, skonfig->sbuf + skonfig->path, skonfig->sbuf + skonfig->skinfile);
    pthread_mutex_unlock (&skonfig->skin_mutex);
    return 0;
  }

#ifdef DEBUG_SKIN
  check_skonfig(skonfig);
#endif

  /*
   * Execute load command
   */
  if(skonfig->load_command)
    xitk_system (0, skonfig->sbuf + skonfig->load_command);

  pthread_mutex_unlock (&skonfig->skin_mutex);

  if (skonfig->xitk->verbosity >= 2)
    printf ("xitk.skin.load (%s): %u string bytes.\n", skonfig->sbuf + skonfig->name, (unsigned int)skonfig->sbuf_pos);

  return 1;
}

/*
 * Unload (free) xitk_skin_config_t object.
 */
void xitk_skin_unload_config(xitk_skin_config_t *skonfig) {
  if(skonfig) {
    if (skonfig->xitk->verbosity >= 2)
      printf ("xitk.skin.unload (%s).\n", skonfig->sbuf + skonfig->name);

    if(skonfig->unload_command)
      xitk_system (0, skonfig->sbuf + skonfig->unload_command);

    xitk_skin_free_config(skonfig);
  }
}

/*
 * Check skin version.
 * return: 0 if version found < min_version
 *         1 if version found == min_version
 *         2 if version found > min_version
 *        -1 if no version found
 */
int xitk_skin_check_version(xitk_skin_config_t *skonfig, int min_version) {

  ABORT_IF_NULL(skonfig);

  if(skonfig->version == -1)
    return -1;
  else if(skonfig->version < min_version)
    return 0;
  else if(skonfig->version == min_version)
    return 1;
  else if(skonfig->version > min_version)
    return 2;

  return -1;
}

/*
 *
 */
const char *xitk_skin_get_animation(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);

  return skonfig->animation ? skonfig->sbuf + skonfig->animation : NULL;
}

/*
 *
 */
const char *xitk_skin_get_logo(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);

  return skonfig->logo ? skonfig->sbuf + skonfig->logo : NULL;
}

const xitk_skin_element_info_t *xitk_skin_get_info (xitk_skin_config_t *skin, const char *element_name) {
  xitk_skin_element_t *s;

  s = skin_lookup_section (skin, element_name);
  if (!s)
    return NULL;

  return &s->info;
}

/*
 *
 */
void xitk_skin_lock(xitk_skin_config_t *skonfig) {
  if (skonfig)
    pthread_mutex_lock (&skonfig->skin_mutex);
}

/*
 *
 */
void xitk_skin_unlock(xitk_skin_config_t *skonfig) {
  if (skonfig)
    pthread_mutex_unlock (&skonfig->skin_mutex);
}
