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
#include <string.h>
#include <pthread.h>

#ifdef WITH_XMB
#include <wchar.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"

#define XITK_CACHE_SIZE 10
#define XITK_FONT_LENGTH_NAME 256

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

#ifdef WITH_XMB
/*
 * Guess if error occured, release missing charsets list.
 * There is a problem with rendering when "ISO10646-1" charset is missing.
 */
static int xitk_font_guess_error(XFontSet fs, char *name, char **missing, int count) {
  int i;

  if(fs == NULL) 
    return 1;

  if(count) {
    /* some uncovered codepages */
    for(i = 0; i < count; i++) {
      if(strcmp("ISO10646-1", missing[i]) == 0) {
        XITK_WARNING("font \"%s\" doesn't contain charset %s\n", name, missing[i]);
        XFreeStringList(missing);
        return 1;
      }
    }

    XFreeStringList(missing);
  }

  return 0;
}

/*
 * XCreateFontSet requires font name starting with '-'
 */
static char *xitk_font_right_name(char *name) {
  char right_name[XITK_FONT_LENGTH_NAME];

  ABORT_IF_NULL(name);

  if(name[0] == '-') 
    return strdup(name);

  right_name[0] = (name[0] == '*') ? '-' : '*';

  right_name[XITK_FONT_LENGTH_NAME - 1] = 0;
  strncpy(right_name + 1, name, XITK_FONT_LENGTH_NAME - 2);

  return strdup(right_name);
}
#endif

/*
 * load the font and returns status
 */
static int xitk_font_load_one(Display *display, char *font, xitk_font_t *xtfs) {
  int    ok;
#ifdef WITH_XMB
  char **missing;
  char  *def;
  char  *right_name;
  int    count;

  right_name = xitk_font_right_name(font);
  XLOCK(display);
  xtfs->fontset = XCreateFontSet(display, right_name, &missing, &count, &def);
  ok = !xitk_font_guess_error(xtfs->fontset, right_name, missing, count);
  XUNLOCK(display);
  free(right_name);
#else
  XLOCK(display);
  xtfs->font = XLoadQueryFont(display, font);
  XUNLOCK(display);
  ok = (xtfs->font != NULL);
#endif

  if(ok)
    xtfs->display = display;

  return ok;
}

/*
 * unload the font
 */
static void xitk_font_unload_one(xitk_font_t *xtfs) {
  XLOCK(xtfs->display);
#ifdef WITH_XMB
  XFreeFontSet(xtfs->display, xtfs->fontset);
#else
  XFreeFont(xtfs->display, xtfs->font);
#endif
  XUNLOCK(xtfs->display);
  free(xtfs->name);
}

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

    xitk_font_unload_one(xtfs);
 
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

  if(!cache.n) {
    fprintf(stderr, "%s(%d): cache.n != 0. Aborting.\n", __FUNCTION__, __LINE__);
    abort();
  }
  
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

#ifdef LOG
    printf("xiTK: dropped \"%s\", list: %d, cache: %d\n", xtfs->name, cache.nlist, cache.n);
#endif

    xitk_font_unload_one(xtfs);

    free(xtfs);

    /* insert new item into sorted cache */
    if(cmp == 0)
      xitk_cache_insert_final(oldest, font);
    else {
      if(cmp < 0) 
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

  while(right >= left) {
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

      while(j < cache.n) {
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
  
  ABORT_IF_NULL(display);
  ABORT_IF_NULL(font);
  pthread_mutex_lock(&cache.mutex);
  
  /* quick search in the cache of unloaded fonts */
  if((xtfs = xitk_cache_take_item(display, font)) == NULL) {
    /* search in the list of loaded fonts */
    if((list_item = cache_get_from_list(display, font)) != NULL) {
      list_item->number++;
      pthread_mutex_unlock(&cache.mutex);
      return list_item->font;
    }
  }
  
  if(!xtfs) {
    /* font not found, load it */ 
    xtfs = (xitk_font_t *) xitk_xmalloc(sizeof(xitk_font_t));

    if(!xitk_font_load_one(display, font, xtfs)) {
      char *fdname = xitk_get_default_font();

      if(!fdname || !xitk_font_load_one(display, fdname, xtfs)) {
        char *fsname = xitk_get_system_font();
 	if(!xitk_font_load_one(display, fsname, xtfs)) {
           XITK_WARNING("loading font \"%s\" failed, default and system fonts \"%s\" and \"%s\" failed too\n", font, fdname, fsname);
           free(xtfs);
           pthread_mutex_unlock(&cache.mutex);
           return NULL;
        }
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
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
#ifdef WITH_XMB
  ABORT_IF_NULL(xtfs->fontset);
#else
  ABORT_IF_NULL(xtfs->font);
#endif

  pthread_mutex_lock(&cache.mutex);
  /* search the font in the list */
  item = cache_get_from_list(xtfs->display, xtfs->name);
  /* it must be there */
  ABORT_IF_NULL(item);

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
void xitk_font_draw_string(xitk_font_t *xtfs, Pixmap pix, GC gc, 
			   int x, int y, const char *text, 
			   size_t nbytes) {

#ifdef WITH_XMB
  XmbDrawString(xtfs->display, pix, xtfs->fontset, gc, x, y, 
		text, nbytes);
#else 
  XDrawString(xtfs->display, pix, gc, x, y, text, nbytes);
#endif
}

#ifndef WITH_XMB
/*
 *
 */
static Font xitk_font_get_font_id(xitk_font_t *xtfs) {
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);

  return xtfs->font->fid;
}

/*
 *
 */
static int xitk_font_is_font_8(xitk_font_t *xtfs) {

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);

  return ((xtfs->font->min_byte1 == 0) && (xtfs->font->max_byte1 == 0));
}

/*
 *
 */
#if 0
static int xitk_font_is_font_16(xitk_font_t *xtfs) {

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);

  return ((xtfs->font->min_byte1 != 0) || (xtfs->font->max_byte1 != 0));
}
#endif
#endif

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int nbytes) {
  int width;
 
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);

#ifdef WITH_XMB
  ABORT_IF_NULL(xtfs->fontset);

  XLOCK(xtfs->display);
  width = XmbTextEscapement(xtfs->fontset, c, nbytes);
  XUNLOCK(xtfs->display);
#else 
  ABORT_IF_NULL(xtfs->font);

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    width = XTextWidth (xtfs->font, c, nbytes);
  else
    width = XTextWidth16 (xtfs->font, (XChar2b *)c, nbytes);

  XUNLOCK(xtfs->display);
#endif
 
  return width;
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
int xitk_font_get_char_width(xitk_font_t *xtfs, char *c, int maxnbytes, int *nbytes) {
#ifdef WITH_XMB
  mbstate_t state;
  size_t    n;
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->fontset);
  ABORT_IF_NULL(c);

  if(maxnbytes <= 0) 
    return 0;

  memset(&state, '\0', sizeof(mbstate_t));
  n = mbrtowc(NULL, c, maxnbytes, &state);

  if(n == (size_t)-1 || n == (size_t)-2) 
    return 0;

  if(nbytes)
    *nbytes = n;

  return xitk_font_get_text_width(xtfs, c, n);
#else
  unsigned int  ch = (*c & 0xff);
  int           width;
  XCharStruct  *chars;
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);
  ABORT_IF_NULL(c);

  if(maxnbytes <= 0)
    return 0;

  if(xitk_font_is_font_8(xtfs) && 
     ((ch >= xtfs->font->min_char_or_byte2) && (ch <= xtfs->font->max_char_or_byte2))) {

    chars = xtfs->font->per_char;

    if(chars)
      width = chars[ch - xtfs->font->min_char_or_byte2].width;
    else
      width = xtfs->font->min_bounds.width;

  }
  else
    width = xitk_font_get_text_width(xtfs, c, 1);
  
  return width;
#endif
}

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes) {
#ifdef WITH_XMB
  XRectangle logic;
	
  XLOCK(xtfs->display);  
  XmbTextExtents(xtfs->fontset, c, nbytes, NULL, &logic);
  XUNLOCK(xtfs->display);  
  
  return logic.height;
#else
  int lbearing, rbearing, width, ascent, descent, height;
  
  xitk_font_text_extent(xtfs, c, nbytes, &lbearing, &rbearing, &width, &ascent, &descent);
 
  height = ascent + descent;
 
  return height;
#endif
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
int xitk_font_get_char_height(xitk_font_t *xtfs, char *c, int maxnbytes, int *nbytes) {
#ifdef WITH_XMB
  mbstate_t state;
  size_t    n;

  memset(&state, '\0', sizeof(mbstate_t));
  n = mbrtowc(NULL, c, maxnbytes, &state);

  if(n == (size_t)-1 || n == (size_t)-2) 
    return 0;

  if(nbytes) 
    *nbytes = n;

  return xitk_font_get_text_width(xtfs, c, n);
#else
  return (xitk_font_get_text_height(xtfs, c, 1));
#endif
}

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int nbytes,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
#ifdef WITH_XMB
  XRectangle logic;

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->fontset);
  ABORT_IF_NULL(c);

  XLOCK(xtfs->display);
  XmbTextExtents(xtfs->fontset, c, nbytes, NULL, &logic);
  XUNLOCK(xtfs->display);

  /* lbearing and rbearing can't find out or compute */
  if(lbearing)
    *lbearing = 0;
  if(rbearing)
    *rbearing = 0;
  if(width)
    *width    = logic.width;
  if(ascent)
    *ascent  = -logic.y;
  if(descent)
    *descent = logic.height + logic.y;
#else
  XCharStruct ov;
  int         dir;
  int         fascent, fdescent;

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);

  XLOCK(xtfs->display);

  if(xitk_font_is_font_8(xtfs))
    XTextExtents(xtfs->font, c, nbytes, &dir, &fascent, &fdescent, &ov);
  else
    XTextExtents16(xtfs->font, (XChar2b *)c, (nbytes / 2), &dir, &fascent, &fdescent, &ov);

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
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);
#ifdef WITH_XMB
  ABORT_IF_NULL(xtfs->fontset);
#else 
  ABORT_IF_NULL(xtfs->font);
#endif

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return ascent;
}

/*
 *
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c) {
  int lbearing, rbearing, width, ascent, descent;
 
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);
#ifdef WITH_XMB
  ABORT_IF_NULL(xtfs->fontset);
#else 
  ABORT_IF_NULL(xtfs->font);
#endif

  xitk_font_text_extent(xtfs, c, strlen(c), &lbearing, &rbearing, &width, &ascent, &descent);

  return descent;
}

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc) {

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);

#ifndef WITH_XMB
  XLOCK(xtfs->display);
  XSetFont(xtfs->display, gc, xitk_font_get_font_id(xtfs));
  XUNLOCK(xtfs->display);
#endif
}
