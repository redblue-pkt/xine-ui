/* 
 * Copyright (C) 2000-2003 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"

#define XITK_CACHE_SIZE 10

typedef struct {
  unsigned int             lru;        /* old of item for LRU */
  xitk_font_t             *font;
} xitk_font_cache_item_t;

typedef struct {
  unsigned int             number;     /* number of instances of this font */
  xitk_font_t             *font;       /* remembered loaded font */
} xitk_font_list_item_t;

typedef struct {
  size_t                   n;
  size_t                   nlist;          /* number of fonts in the cache and in the list */

  unsigned int             life;        /* life counter for LRU */
  xitk_list_t             *loaded;      /* list of loaded fonts */
  pthread_mutex_t          mutex;    /* protecting mutex */
  xitk_font_cache_item_t   items[XITK_CACHE_SIZE]; /* cache */
} xitk_font_cache_t;

/* global font cache */
static xitk_font_cache_t cache;

/*
 * init font cache subsystem
 */
void xitk_font_cache_init(void) {

  cache.n      = 0;
  cache.life   = 0;
  cache.loaded = xitk_list_new();	

  pthread_mutex_init(&cache.mutex, NULL);
}

/*
 * destroy font cache subsystem
 */
void xitk_font_cache_done(void) {
  size_t       i;
  xitk_font_t *xtfs;

  for (i = 0; i < cache.n; i++) {
    xtfs = cache.items[i].font;

    XLOCK(xtfs->display);
    XFreeFont(xtfs->display, xtfs->font);
    XUNLOCK(xtfs->display);

    free(xtfs->name);
    free(xtfs);
  }
  cache.n = 0;

#ifdef DEBUG
  {
    xitk_font_list_item_t *item;
    
    if(xitk_list_is_empty(cache.loaded) == 1) {
      XITK_WARNING("%s(): list of loaded font isn't empty:\n", __FUNCTION__);
      item = xitk_list_first_content(cache.loaded);
      while(item) {
	printf("  %s (%d)\n", item->font->name, item->number);
	item = xitk_list_next_content(cache.loaded);
      }
    }
  }
#endif
  xitk_list_free(cache.loaded);
  pthread_mutex_destroy(&cache.mutex);
}

/* 
 * get index of oldest item in the cache 
 */
static size_t xitk_cache_get_oldest(void) {
  size_t          i, oldest;
  unsigned long   lru;

  assert(cache.n != 0);

  i      = 0;
  oldest = 0;
  lru    = cache.items[0].lru;

  for(i = 1; i < cache.n; i++) {
    if(cache.items[i].lru < lru) {
      oldest = i;
      lru = cache.items[i].lru;
    }
  }

  return oldest;
}

/* 
 * place item into cache and adjust all counters for LRU 
 */
static void xitk_cache_insert_final(size_t pos, xitk_font_t *font) {
  size_t i;

  cache.items[pos].lru  = cache.life;
  cache.items[pos].font = font;

  if(cache.life + 1 == 0) {
    cache.life >>= 1;
    for(i = 0; i < cache.n; i++) 
      cache.items[i].lru >>= 1;
  } 
  else 
    cache.life++;
}

/* 
 * insert item into sorted cache, it shifts part of cache to move gap
 */
static void xitk_cache_insert_into(size_t gap, int step, xitk_font_t *font) {
  size_t i;

  i = gap;
  while (((step == -1 && i > 0) || (step == 1 && i < cache.n - 1)) 
	 && strcmp(font->name, cache.items[i + step].font->name) * step > 0) {
    cache.items[i] = cache.items[i + step];
    i += step;
  }
  xitk_cache_insert_final(i, font);
}

/* 
 * add new item into cache, if it's full the oldest item will be released
 */
static void xitk_cache_add_item(xitk_font_t *font) {
  size_t       oldest;
  int          cmp;
  xitk_font_t *xtfs;
	
  if(cache.n < XITK_CACHE_SIZE) {
    /* cache is not full */
    cache.n++;
    xitk_cache_insert_into(cache.n - 1, -1, font);
  } 
  else {
    /* cache is full */
    /* free the oldest item */
    oldest = xitk_cache_get_oldest();
    cmp    = strcmp(font->name, cache.items[oldest].font->name);
    xtfs   = cache.items[oldest].font;

    XLOCK(xtfs->display);
    XFreeFont(xtfs->display, xtfs->font);
    XUNLOCK(xtfs->display);

#ifdef LOG
    printf("xiTK: dropped \"%s\", list: %d, cache: %d\n", xtfs->name, cache.nlist, cache.n);
#endif

    free(xtfs->name);
    free(xtfs);

    /* insert new item into sorted cache */
    if(cmp == 0)
      xitk_cache_insert_final(oldest, font);
    else {
      if (cmp < 0) 
	cmp = -1;
      else 
	cmp = 1;
      
      xitk_cache_insert_into(oldest, cmp, font);
    }
  }
}

/* 
 * remove the item in 'pos' and returns its data
 */
static xitk_font_t *xitk_cache_remove_item(size_t pos) {
  xitk_font_t *xtfs;
  size_t       i;

  xtfs = cache.items[pos].font;
  for (i = pos; i < cache.n - 1; i++)
    cache.items[i] = cache.items[i + 1];
  cache.n--;

  return xtfs;
}

/* 
 * search the font of given display in the cache, remove it from the cache
 */
static xitk_font_t *xitk_cache_take_item(Display *display, char *name) {
  int     left, right;
  size_t  i, j;
  int     cmp;

  if(!cache.n)
    return NULL;

  /* binary search in the cache */
  left  = 0;
  right = cache.n - 1;

  while (right >= left) {
    i = (left + right) >> 1;
    cmp = strcmp(name, cache.items[i].font->name);

    if(cmp < 0)
      right = i - 1;
    else if (cmp > 0) 
      left = i + 1;
    else {
      /* found right name, search right display */
      /* forward */
      j = i;
      while(1) {
	if(cache.items[j].font->display == display)
	  return xitk_cache_remove_item(j);

	if(!j) 
	  break;

	cmp = strcmp(name, cache.items[--j].font->name);
      }
      /* backward */
      j = i + 1;

      while (j < cache.n) {
	cmp = strcmp(name, cache.items[j].font->name);

	if(cmp != 0)
	  return NULL;

	if(cache.items[j].font->display == display)
	  return xitk_cache_remove_item(j);

	j++;
      }

      /* not in given display */
      return NULL;
    }
  }

  /* no such name */
  return NULL;
}

/*
 * search the font in the list of loaded fonts
 */
static xitk_font_list_item_t *cache_get_from_list(Display *display, char *name) {
  xitk_font_list_item_t *item;

  item = xitk_list_first_content(cache.loaded);
  if(!item)
    return NULL;

  do {
    if((strcmp(name, item->font->name) == 0) && (display == item->font->display)) 
      return item;

    item = xitk_list_next_content(cache.loaded);
  } while(item);

  return NULL;
}

/*
 *
 */
xitk_font_t *xitk_font_load_font(Display *display, char *font) {
  xitk_font_t           *xtfs;
  xitk_font_list_item_t *list_item;

  assert((display != NULL) && (font != NULL));
  pthread_mutex_lock(&cache.mutex);
  
  /* quick search in the cache of unloaded fonts */
  if((xtfs = xitk_cache_take_item(display, font)) == NULL) {
    /* search in the list of loaded fonts */
    if ((list_item = cache_get_from_list(display, font)) != NULL) {
      list_item->number++;
      pthread_mutex_unlock(&cache.mutex);
      return list_item->font;
    }
  }
  
  if (!xtfs) {
    /* font not found, load it */ 
    xtfs = (xitk_font_t *) xitk_xmalloc(sizeof(xitk_font_t));

    XLOCK(display);
    xtfs->font = XLoadQueryFont(display, font);
    XUNLOCK(display);
  
    if(xtfs->font == NULL) {
      char *fname = xitk_get_default_font();
      if(fname) {
        XLOCK(display);
        xtfs->font = XLoadQueryFont(display, fname);
        XUNLOCK(display);
      }

      if(xtfs->font == NULL) {
        fname = xitk_get_system_font();
        XLOCK(display);
        xtfs->font = XLoadQueryFont(display, fname);
        XUNLOCK(display);
      }
    
      if(xtfs->font == NULL) {
        XITK_WARNING("%s(): XLoadQueryFont() failed\n", __FUNCTION__);
        free(xtfs);
        pthread_mutex_unlock(&cache.mutex);
        return NULL;
      }
    }

    xtfs->name = strdup(font);
    xtfs->display = display;

#ifdef LOG
    printf("xiTK: loaded new \"%s\", list: %d, cache: %d\n", xtfs->name, cache.nlist, cache.n);
#endif
  }

  /* add the font into list */
  list_item         = xitk_xmalloc(sizeof(xitk_font_list_item_t));
  list_item->font   = xtfs;
  list_item->number = 1;
  xitk_list_append_content(cache.loaded, list_item);
  cache.nlist++;

  pthread_mutex_unlock(&cache.mutex);

  return xtfs;
}

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs) {
  xitk_font_list_item_t *item;

  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL));

  pthread_mutex_lock(&cache.mutex);
  /* search the font in the list */
  item = cache_get_from_list(xtfs->display, xtfs->name);
  /* it must be there */
  assert(item != NULL);

  if(--item->number == 0) {
    xitk_list_delete_current(cache.loaded);
    cache.nlist--;
    xitk_cache_add_item(item->font);
    free(item);
  }

  pthread_mutex_unlock(&cache.mutex);
}
  
/*
 *
 */
Font xitk_font_get_font_id(xitk_font_t *xtfs) {
  
  assert((xtfs != NULL) && (xtfs->font != NULL));

  return xtfs->font->fid;
}

/*
 *
 */
int xitk_font_is_font_8(xitk_font_t *xtfs) {

  assert((xtfs != NULL) && (xtfs->font != NULL));

  return ((xtfs->font->min_byte1 == 0) && (xtfs->font->max_byte1 == 0));
}

/*
 *
 */
int xitk_font_is_font_16(xitk_font_t *xtfs) {

  assert((xtfs != NULL) && (xtfs->font != NULL));

  return ((xtfs->font->min_byte1 != 0) || (xtfs->font->max_byte1 != 0));
}

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int length) {
  int len;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    len = XTextWidth (xtfs->font, c, length);
  else
    len = XTextWidth16 (xtfs->font, (XChar2b *)c, length);

  XUNLOCK(xtfs->display);
  
  return len;
}

/*
 *
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c) {
  
  return (xitk_font_get_text_width(xtfs, c, strlen(c)));
}

/*
 *
 */
int xitk_font_get_char_width(xitk_font_t *xtfs, unsigned char *c) {
  unsigned int  ch = (*c & 0xff);
  int           width;
  XCharStruct  *chars;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (c != NULL));

  if(xitk_font_is_font_8(xtfs) && 
     ((ch >= xtfs->font->min_char_or_byte2) && (ch <= xtfs->font->max_char_or_byte2))) {

    chars = xtfs->font->per_char;

    if(chars)
      width = chars[ch - xtfs->font->min_char_or_byte2].width;
    else
      width = xtfs->font->min_bounds.width;

  }
  else {
    width = xitk_font_get_text_width(xtfs, c, 1);
  }
  
  return width;
}

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int length) {
  int lbearing, rbearing, width, ascent, descent;
  int height;
  
  xitk_font_text_extent(xtfs, c, length, &lbearing, &rbearing, &width, &ascent, &descent);
 
  height = ascent + descent;
 
  return height;
}

/*
 *
 */
int xitk_font_get_string_height(xitk_font_t *xtfs, const char *c) {

  return (xitk_font_get_text_height(xtfs, c, strlen(c)));
}

/*
 *
 */
int xitk_font_get_char_height(xitk_font_t *xtfs, unsigned char *c) {

  return (xitk_font_get_text_height(xtfs, c, 1));
}

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int length,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  XCharStruct ov;
  int         dir;
  int         fascent, fdescent;

  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    XTextExtents(xtfs->font, c, length, &dir, &fascent, &fdescent, &ov);
  else
    XTextExtents16(xtfs->font, (XChar2b *)c, (length / 2), &dir, &fascent, &fdescent, &ov);

  XUNLOCK(xtfs->display);

  if(lbearing)
    *lbearing = ov.lbearing;
  if(rbearing)
    *rbearing = ov.rbearing;
  if(width)
    *width    = ov.width;
  if(ascent)
    *ascent  = xtfs->font->ascent;
  if(descent)
    *descent = xtfs->font->descent;
#if 0
  if(ascent)
    *ascent   = ov.ascent;
  if(descent)
    *descent  = ov.descent;
#endif
}

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
			     int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {

  xitk_font_text_extent(xtfs, c, strlen(c), lbearing, rbearing, width, ascent, descent);
}

/*
 *
 */
int xitk_font_get_ascent(xitk_font_t *xtfs, const char *c) {
  int lbearing, rbearing, width, ascent, descent;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return ascent;
}

/*
 *
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c) {
  int lbearing, rbearing, width, ascent, descent;
  
  assert((xtfs != NULL) && (xtfs->font != NULL) && (xtfs->display != NULL) && (c != NULL));

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return descent;
}

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc) {

  assert((xtfs != NULL) && (xtfs->display != NULL));

  XLOCK(xtfs->display);
  XSetFont(xtfs->display, gc, xitk_font_get_font_id(xtfs));
  XUNLOCK(xtfs->display);
}
