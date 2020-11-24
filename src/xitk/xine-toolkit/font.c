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

#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "_xitk.h"

#define XITK_CACHE_SIZE 10

typedef struct {
  unsigned int             lru;        /* old of item for LRU */
  char                    *name;       /* this is the name of requested font (loaded font may be different) */
  xitk_font_t             *font;
} xitk_font_cache_item_t;

typedef struct {
  xitk_dnode_t             node;
  const char              *name;       /* this is the name of requested font (loaded font may be different) */
  xitk_font_t             *font;       /* remembered loaded font */
  unsigned int             number;     /* number of instances of this font */
} xitk_font_list_item_t;

struct xitk_font_cache_s {
  size_t                   n;
  size_t                   nlist;      /* number of fonts in the cache and in the list */

  unsigned int             life;       /* life counter for LRU */
  xitk_dlist_t             loaded;     /* list of loaded fonts */
  pthread_mutex_t          mutex;      /* protecting mutex */
  xitk_font_cache_item_t   items[XITK_CACHE_SIZE]; /* cache */
};

/*
 * init font cache subsystem
 */
xitk_font_cache_t *xitk_font_cache_init (void) {
  xitk_font_cache_t *font_cache;

  font_cache = malloc (sizeof (*font_cache));

  if (font_cache) {
    font_cache->n    = 0;
    font_cache->life = 0;
    xitk_dlist_init (&font_cache->loaded);
    pthread_mutex_init (&font_cache->mutex, NULL);
  }

  return font_cache;
}

/*
 * destroy font cache subsystem
 */
void xitk_font_cache_destroy(xitk_font_cache_t **p) {
  xitk_font_cache_t *font_cache = *p;
  size_t       i;

  if (!font_cache)
    return;
  *p = NULL;

  for (i = 0; i < font_cache->n; i++) {
    font_cache->items[i].font->_delete(&font_cache->items[i].font);
    free(font_cache->items[i].name);
  }
  font_cache->n = 0;

#ifdef XITK_DEBUG
  {
    xitk_font_list_item_t *item = (xitk_font_list_item_t *)cached.loaded.head.next;
    if (item->node.next) {
      XITK_WARNING ("%s(): list of loaded font isn't empty:\n", __FUNCTION__);
      do {
        printf ("  %s (%u)\n", item->font->name, item->number);
        item = (xitk_font_list_item_t *)item->node.next;
      } while (item->node.next);
    }
  }
#endif

  xitk_dlist_clear (&font_cache->loaded);
  pthread_mutex_destroy(&font_cache->mutex);

  free (font_cache);
}

/*
 * get index of oldest item in the cache
 */
static size_t xitk_cache_get_oldest(xitk_font_cache_t *font_cache) {
  size_t          i, oldest;
  unsigned long   lru;

  if (!font_cache->n) {
    fprintf(stderr, "%s(%d): cache.n == 0. Aborting.\n", __FUNCTION__, __LINE__);
    abort();
  }

  i      = 0;
  oldest = 0;
  lru    = font_cache->items[0].lru;

  for (i = 1; i < font_cache->n; i++) {
    if (font_cache->items[i].lru < lru) {
      oldest = i;
      lru = font_cache->items[i].lru;
    }
  }

  return oldest;
}

/*
 * place item into cache and adjust all counters for LRU
 */
static void xitk_cache_insert_final(xitk_font_cache_t *font_cache, size_t pos, xitk_font_t *font, const char *name) {
  size_t i;

  font_cache->items[pos].lru  = font_cache->life;
  font_cache->items[pos].font = font;
  font_cache->items[pos].name = strdup(name);

  if (font_cache->life + 1 == 0) {
    font_cache->life >>= 1;
    for (i = 0; i < font_cache->n; i++)
      font_cache->items[i].lru >>= 1;
  }
  else
    font_cache->life++;
}

/*
 * insert item into sorted cache, it shifts part of cache to move gap
 */
static void xitk_cache_insert_into(xitk_font_cache_t *font_cache, size_t gap, int step, xitk_font_t *font, const char *name) {
  size_t i;

  i = gap;
  while (((step == -1 && i > 0) || (step == 1 && i < font_cache->n - 1))
         && strcmp(name, font_cache->items[i + step].name) * step > 0) {
    font_cache->items[i] = font_cache->items[i + step];
    i += step;
  }
  xitk_cache_insert_final(font_cache, i, font, name);
}

/*
 * add new item into cache, if it's full the oldest item will be released
 */
static void xitk_cache_add_item(xitk_font_cache_t *font_cache, xitk_font_t *font, const char *name) {
  size_t       oldest;
  int          cmp;
  xitk_font_t *xtfs;

  if (font_cache->n < XITK_CACHE_SIZE) {
    /* cache is not full */
    font_cache->n++;
    xitk_cache_insert_into(font_cache, font_cache->n - 1, -1, font, name);
  }
  else {
    /* cache is full */
    /* free the oldest item */
    oldest = xitk_cache_get_oldest(font_cache);
    cmp    = strcmp(name, font_cache->items[oldest].name);
    xtfs   = font_cache->items[oldest].font;

#ifdef LOG
    printf("xiTK: dropped \"%s\", list: %zu, cache: %zu\n", xtfs->name, font_cache->nlist, font_cache->n);
#endif

    xtfs->_delete(&font_cache->items[oldest].font);
    free(font_cache->items[oldest].name);

    /* insert new item into sorted cache */
    if(cmp == 0)
      xitk_cache_insert_final(font_cache, oldest, font, name);
    else {
      if(cmp < 0)
        cmp = -1;
      else
        cmp = 1;

      xitk_cache_insert_into(font_cache, oldest, cmp, font, name);
    }
  }
}

/*
 * remove the item in 'pos' and returns its data
 */
static xitk_font_t *xitk_cache_remove_item(xitk_font_cache_t *font_cache, size_t pos) {
  xitk_font_t *xtfs;
  size_t       i;

  xtfs = font_cache->items[pos].font;
  free(font_cache->items[pos].name);

  for (i = pos; i < font_cache->n - 1; i++)
    font_cache->items[i] = font_cache->items[i + 1];
  font_cache->n--;

  return xtfs;
}

/*
 * search the font of given display in the cache, remove it from the cache
 */
static xitk_font_t *xitk_cache_take_item(xitk_font_cache_t *font_cache, const char *name) {
  int     left, right;
  size_t  i;
  int     cmp;

  if (!font_cache->n)
    return NULL;

  /* binary search in the cache */
  left  = 0;
  right = font_cache->n - 1;

  while(right >= left) {
    i = (left + right) >> 1;
    cmp = strcmp(name, font_cache->items[i].name);

    if(cmp < 0)
      right = i - 1;
    else if (cmp > 0)
      left = i + 1;
    else {
      /* found right name */
      return xitk_cache_remove_item(font_cache, i);
    }
  }

  /* no such name */
  return NULL;
}

/*
 * search the font in the list of loaded fonts
 */
static xitk_font_list_item_t *cache_find_from_list(xitk_font_cache_t *font_cache, const char *name) {
  xitk_font_list_item_t *item = (xitk_font_list_item_t *)font_cache->loaded.head.next;
  while (item->node.next) {
    if (strcmp(name, item->name) == 0)
      return item;
    item = (xitk_font_list_item_t *)item->node.next;
  }
  return NULL;
}
static xitk_font_list_item_t *cache_get_from_list(xitk_font_cache_t *font_cache, xitk_font_t *xtfs) {
  xitk_font_list_item_t *item = (xitk_font_list_item_t *)font_cache->loaded.head.next;
  while (item->node.next) {
    if (xtfs == item->font)
      return item;
    item = (xitk_font_list_item_t *)item->node.next;
  }
  return NULL;
}

/*
 *
 */
xitk_font_t *xitk_font_load_font(xitk_t *xitk, const char *font) {
  xitk_font_t           *xtfs;
  xitk_font_list_item_t *list_item;
  xitk_font_cache_t     *font_cache;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(font);

  font_cache = xitk->font_cache;

  pthread_mutex_lock(&font_cache->mutex);

  /* quick search in the cache of unloaded fonts */
  if ((xtfs = xitk_cache_take_item(font_cache, font)) == NULL) {
    /* search in the list of loaded fonts */
    if((list_item = cache_find_from_list(font_cache, font)) != NULL) {
      list_item->number++;
      pthread_mutex_unlock(&font_cache->mutex);
      return list_item->font;
    }
  }

  if(!xtfs) {
    /* font not found, load it */
    xtfs = xitk->d->font_new(xitk->d, font);
    if (!xtfs) {
      const char *fdname = xitk_get_cfg_string (xitk, XITK_DEFAULT_FONT);

      xtfs = fdname ? xitk->d->font_new(xitk->d, fdname) : NULL;
      if (!xtfs) {
        const char *fsname = xitk_get_cfg_string (xitk, XITK_SYSTEM_FONT);

        xtfs = xitk->d->font_new(xitk->d, fsname);
        if (!xtfs) {
          XITK_WARNING("loading font \"%s\" failed, default and system fonts \"%s\" and \"%s\" failed too\n", font, fdname, fsname);
          pthread_mutex_unlock(&font_cache->mutex);

          /* Maybe broken XMB support */
          if (xitk_get_cfg_num (xitk, XITK_XMB_ENABLE)) {
            xitk_font_t *xtfs_fallback;

            xitk_set_xmb_enability(xitk, 0);
            if((xtfs_fallback = xitk_font_load_font(xitk, font)))
              XITK_WARNING("XMB support seems broken on your system, add \"font.xmb = 0\" in your ~/.xitkrc\n");

            return xtfs_fallback;
          }
          return NULL;
        }
      }
    }

    xtfs->data = font_cache;
#ifdef LOG
    printf("xiTK: loaded new \"%s\", list: %zu, cache: %zu\n", font, font_cache->nlist, font_cache->n);
#endif
  }

  /* add the font into list */
  list_item         = xitk_xmalloc(sizeof(xitk_font_list_item_t) + strlen(font) + 1);
  list_item->font   = xtfs;
  list_item->name   = strcpy((char *)&list_item[1], font);
  list_item->number = 1;
  xitk_dlist_add_tail (&font_cache->loaded, &list_item->node);
  font_cache->nlist++;

  pthread_mutex_unlock(&font_cache->mutex);

  return xtfs;
}

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs) {
  xitk_font_list_item_t *item;
  xitk_font_cache_t *font_cache = xtfs->data;

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->data);

  pthread_mutex_lock(&font_cache->mutex);
  /* search the font in the list */
  item = cache_get_from_list(font_cache, xtfs);
  /* it must be there */
  ABORT_IF_NULL(item);

  if(--item->number == 0) {
    xitk_dnode_remove (&item->node);
    font_cache->nlist--;
    xitk_cache_add_item(font_cache, item->font, item->name);
    free(item);
  }

  pthread_mutex_unlock(&font_cache->mutex);
}

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int nbytes) {
  int width;

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(c);

  xitk_font_text_extent(xtfs, c, nbytes, NULL, NULL, &width, NULL, NULL);
  return width;
}

/*
 *
 */
int xitk_font_get_string_length(xitk_font_t *xtfs, const char *c) {
  return ((c && strlen(c)) ? (xitk_font_get_text_width(xtfs, c, strlen(c))) : 0);
}

/*
 *
 */
static int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes) {
  int ascent, descent;

  xitk_font_text_extent(xtfs, c, nbytes, NULL, NULL, NULL, &ascent, &descent);

  return (ascent + descent);
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
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, size_t nbytes,
                           int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(c);

  if (nbytes <= 0) {
    if (width) *width     = 0;
    if (ascent) *ascent   = 0;
    if (descent) *descent = 0;
    if (lbearing) *lbearing = 0;
    if (rbearing) *rbearing = 0;
    return;
  }

#ifdef DEBUG
  if (nbytes > strlen(c) + 1) {
    XITK_WARNING("extent: %zu > %zu\n", nbytes, strlen(c));
  }
#endif

  xtfs->text_extent(xtfs, c, nbytes, lbearing, rbearing, width, ascent, descent);
}

/*
 *
 */
void xitk_font_string_extent(xitk_font_t *xtfs, const char *c,
                             int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {

  xitk_font_text_extent(xtfs, c, strlen(c), lbearing, rbearing, width, ascent, descent);
}
