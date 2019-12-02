/* 
 * Copyright (C) 2000-2019 the xine project
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
#include <pthread.h>

#ifdef WITH_XMB
#include <wchar.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"
#include "recode.h"

#define XITK_CACHE_SIZE 10
#define XITK_FONT_LENGTH_NAME 256

typedef struct {
  unsigned int             lru;        /* old of item for LRU */
  xitk_font_t             *font;
} xitk_font_cache_item_t;

typedef struct {
  xitk_dnode_t             node;
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

  xitk_recode_t           *xr;         /* text recoding */
};

#if defined(WITH_XMB) && !defined(WITH_XFT)
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
      if(strcasecmp("ISO10646-1", missing[i]) == 0) {
        XITK_WARNING("font \"%s\" doesn't contain charset %s\n", name, missing[i]);
        XFreeStringList(missing);
        return 1;
      }
    }

    XFreeStringList(missing);
  }

  return 0;
}
#endif

#ifdef WITH_XFT
/* convert a -*-* .. style font description into something Xft can digest */
static const char * xitk_font_core_string_to_xft(char *new_name, size_t new_name_size, const char * old_name) {

  if(old_name[0] == '-' || old_name[0] == '*') {
    char  family[50], weight[15], slant[8], pxsize[5], ptsize[5];
    
    if( old_name[0] == '-' )
      old_name++;
    
    /* Extract description elements */
    family[0] = weight[0] = slant[0] = pxsize[0] = ptsize[0] = '\0';
    sscanf(old_name, "%*[^-]-%49[^-]", family);
    sscanf(old_name, "%*[^-]-%*[^-]-%14[^-]", weight);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%7[^-]", slant);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%4[^-]", pxsize);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%4[^-]", ptsize);

    /* Transform the elements */
    if(!strcmp(weight, "*"))
      weight[0] = '\0';
    if(!strcmp(pxsize, "*"))
      pxsize[0] = '\0';
    if(!strcmp(ptsize, "*"))
      ptsize[0] = '\0';
    strcpy(slant,
	   (!strcmp(slant, "i")) ? "italic" :
	   (!strcmp(slant, "o")) ? "oblique" :
	   (!strcmp(slant, "r")) ? "roman" :
				   "");

    /* Xft doesn't have lucida, which is a small font;
     * thus we make whatever is chosen 2 sizes smaller */
    if(!strcasecmp(family, "lucida")) {
      if(pxsize[0]) {
	int sz = strtol(pxsize, NULL, 10) - 2;
	snprintf(pxsize, sizeof(pxsize), "%i", sz);
      }
      if(ptsize[0]) {
	int sz = strtol(ptsize, NULL, 10) - 2;
	snprintf(ptsize, sizeof(ptsize), "%i", sz);
      }
    }
    
    /* Build Xft font description string from the elements */
    snprintf(new_name, new_name_size, "%s%s%s%s%s%s%s%s%s",
	     family,
	     ((weight[0]) ? ":weight="    : ""), weight,
	     ((slant[0])  ? ":slant="     : ""), slant,
	     ((pxsize[0]) ? ":pixelsize=" : ""), pxsize,
	     ((ptsize[0]) ? ":size="      : ""), ptsize);

    return new_name;
  }

  return old_name;
}
#endif

#if defined(WITH_XMB) && !defined(WITH_XFT)
/*
 * XCreateFontSet requires font name starting with '-'
 */
static char *xitk_font_right_name(const char *name) {
  char *right_name;

  ABORT_IF_NULL(name);

  right_name = strdup(name);

  switch(right_name[0]) {
  case '-':
    break;
  case '*':
    right_name[0] = '-';
    break;
  default:
    right_name[0] = '*';
    break;
  }

  return right_name;
}
#endif

/*
 * load the font and returns status
 */
static int xitk_font_load_one(Display *display, const char *font, xitk_font_t *xtfs) {
  int    ok;
#ifdef WITH_XFT
#else
# ifdef WITH_XMB
  char **missing;
  char  *def;
  char  *right_name;
  int    count;

  if(xitk_get_xmb_enability()) {
    right_name = xitk_font_right_name(font);
    XLOCK (xitk_x_lock_display, display);
    xtfs->fontset = XCreateFontSet(display, right_name, &missing, &count, &def);
    ok = !xitk_font_guess_error(xtfs->fontset, right_name, missing, count);
    XUNLOCK (xitk_x_unlock_display, display);
    free(right_name);
  }
  else
# endif
#endif
  {
    char new_name[255];
    XLOCK (xitk_x_lock_display, display);
#ifdef WITH_XFT
    xtfs->font = XftFontOpenName( display, DefaultScreen(display),
                                  xitk_font_core_string_to_xft(new_name, sizeof(new_name), font));
#else
    xtfs->font = XLoadQueryFont(display, font);
#endif
    XUNLOCK (xitk_x_unlock_display, display);
    ok = (xtfs->font != NULL);
  }

  if(ok)
    xtfs->display = display;

  return ok;
}

/*
 * unload the font
 */
static void xitk_font_unload_one(xitk_font_t *xtfs) {
  XLOCK (xitk_x_lock_display, xtfs->display);
#ifndef WITH_XFT 
#ifdef WITH_XMB
  if(xitk_get_xmb_enability())
    XFreeFontSet(xtfs->display, xtfs->fontset);
  else
#endif
    XFreeFont(xtfs->display, xtfs->font);
#else
    XftFontClose( xtfs->display, xtfs->font );
#endif
  XUNLOCK (xitk_x_unlock_display, xtfs->display);
  free(xtfs->name);
}

/*
 * init font cache subsystem
 */
void xitk_font_cache_init (void) {
  xitk_t *xitk = gXitk;

  xitk->font_cache = malloc (sizeof (*xitk->font_cache));

  if (xitk->font_cache) {
    xitk->font_cache->n    = 0;
    xitk->font_cache->life = 0;
    xitk_dlist_init (&xitk->font_cache->loaded);
    pthread_mutex_init (&xitk->font_cache->mutex, NULL);
#ifdef WITH_XFT
    xitk->font_cache->xr = xitk_recode_init (NULL, "UTF-8", 1);
#else
    xitk->font_cache->xr = NULL;
#endif
  }
}

/*
 * destroy font cache subsystem
 */
void xitk_font_cache_done(void) {
  xitk_t *xitk = gXitk;
  size_t       i;
  xitk_font_t *xtfs;

  for (i = 0; i < xitk->font_cache->n; i++) {
    xtfs = xitk->font_cache->items[i].font;

    xitk_font_unload_one(xtfs);
 
    free(xtfs);
  }
  xitk->font_cache->n = 0;

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

  xitk_dlist_clear (&xitk->font_cache->loaded);
  pthread_mutex_destroy(&xitk->font_cache->mutex);

  xitk_recode_done(xitk->font_cache->xr);

  free (xitk->font_cache);
  xitk->font_cache = NULL;
}

/* 
 * get index of oldest item in the cache 
 */
static size_t xitk_cache_get_oldest(void) {
  xitk_t *xitk = gXitk;
  size_t          i, oldest;
  unsigned long   lru;

  if(!xitk->font_cache->n) {
    fprintf(stderr, "%s(%d): cache.n == 0. Aborting.\n", __FUNCTION__, __LINE__);
    abort();
  }
  
  i      = 0;
  oldest = 0;
  lru    = xitk->font_cache->items[0].lru;

  for(i = 1; i < xitk->font_cache->n; i++) {
    if(xitk->font_cache->items[i].lru < lru) {
      oldest = i;
      lru = xitk->font_cache->items[i].lru;
    }
  }

  return oldest;
}

/* 
 * place item into cache and adjust all counters for LRU 
 */
static void xitk_cache_insert_final(size_t pos, xitk_font_t *font) {
  xitk_t *xitk = gXitk;
  size_t i;

  xitk->font_cache->items[pos].lru  = xitk->font_cache->life;
  xitk->font_cache->items[pos].font = font;

  if(xitk->font_cache->life + 1 == 0) {
    xitk->font_cache->life >>= 1;
    for(i = 0; i < xitk->font_cache->n; i++) 
      xitk->font_cache->items[i].lru >>= 1;
  } 
  else 
    xitk->font_cache->life++;
}

/* 
 * insert item into sorted cache, it shifts part of cache to move gap
 */
static void xitk_cache_insert_into(size_t gap, int step, xitk_font_t *font) {
  xitk_t *xitk = gXitk;
  size_t i;

  i = gap;
  while (((step == -1 && i > 0) || (step == 1 && i < xitk->font_cache->n - 1)) 
	 && strcmp(font->name, xitk->font_cache->items[i + step].font->name) * step > 0) {
    xitk->font_cache->items[i] = xitk->font_cache->items[i + step];
    i += step;
  }
  xitk_cache_insert_final(i, font);
}

/* 
 * add new item into cache, if it's full the oldest item will be released
 */
static void xitk_cache_add_item(xitk_font_t *font) {
  xitk_t *xitk = gXitk;
  size_t       oldest;
  int          cmp;
  xitk_font_t *xtfs;
	
  if(xitk->font_cache->n < XITK_CACHE_SIZE) {
    /* cache is not full */
    xitk->font_cache->n++;
    xitk_cache_insert_into(xitk->font_cache->n - 1, -1, font);
  } 
  else {
    /* cache is full */
    /* free the oldest item */
    oldest = xitk_cache_get_oldest();
    cmp    = strcmp(font->name, xitk->font_cache->items[oldest].font->name);
    xtfs   = xitk->font_cache->items[oldest].font;

#ifdef LOG
    printf("xiTK: dropped \"%s\", list: %zu, cache: %zu\n", xtfs->name, xitk->font_cache->nlist, xitk->font_cache->n);
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
  xitk_t *xitk = gXitk;
  xitk_font_t *xtfs;
  size_t       i;

  xtfs = xitk->font_cache->items[pos].font;
  for (i = pos; i < xitk->font_cache->n - 1; i++)
    xitk->font_cache->items[i] = xitk->font_cache->items[i + 1];
  xitk->font_cache->n--;

  return xtfs;
}

/* 
 * search the font of given display in the cache, remove it from the cache
 */
static xitk_font_t *xitk_cache_take_item(Display *display, const char *name) {
  xitk_t *xitk = gXitk;
  int     left, right;
  size_t  i, j;
  int     cmp;

  if(!xitk->font_cache->n)
    return NULL;

  /* binary search in the cache */
  left  = 0;
  right = xitk->font_cache->n - 1;

  while(right >= left) {
    i = (left + right) >> 1;
    cmp = strcmp(name, xitk->font_cache->items[i].font->name);

    if(cmp < 0)
      right = i - 1;
    else if (cmp > 0) 
      left = i + 1;
    else {
      /* found right name, search right display */
      /* forward */
      j = i;
      while(1) {
	if(xitk->font_cache->items[j].font->display == display)
	  return xitk_cache_remove_item(j);

	if(!j) 
	  break;

	cmp = strcmp(name, xitk->font_cache->items[--j].font->name);
      }
      /* backward */
      j = i + 1;

      while(j < xitk->font_cache->n) {
	cmp = strcmp(name, xitk->font_cache->items[j].font->name);

	if(cmp != 0)
	  return NULL;

	if(xitk->font_cache->items[j].font->display == display)
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
static xitk_font_list_item_t *cache_get_from_list(Display *display, const char *name) {
  xitk_t *xitk = gXitk;
  xitk_font_list_item_t *item = (xitk_font_list_item_t *)xitk->font_cache->loaded.head.next;
  while (item->node.next) {
    if ((strcmp(name, item->font->name) == 0) && (display == item->font->display))
      return item;
    item = (xitk_font_list_item_t *)item->node.next;
  }
  return NULL;
}

/*
 *
 */
xitk_font_t *xitk_font_load_font(Display *display, const char *font) {
  xitk_t *xitk = gXitk;
  xitk_font_t           *xtfs;
  xitk_font_list_item_t *list_item;
  
  ABORT_IF_NULL(display);
  ABORT_IF_NULL(font);
  pthread_mutex_lock(&xitk->font_cache->mutex);
  
  /* quick search in the cache of unloaded fonts */
  if((xtfs = xitk_cache_take_item(display, font)) == NULL) {
    /* search in the list of loaded fonts */
    if((list_item = cache_get_from_list(display, font)) != NULL) {
      list_item->number++;
      pthread_mutex_unlock(&xitk->font_cache->mutex);
      return list_item->font;
    }
  }
  
  if(!xtfs) {
    /* font not found, load it */ 
    xtfs = (xitk_font_t *) xitk_xmalloc(sizeof(xitk_font_t));

    if(!xitk_font_load_one(display, font, xtfs)) {
      const char *fdname = xitk_get_default_font();

      if(!fdname || !xitk_font_load_one(display, fdname, xtfs)) {
        const char *fsname = xitk_get_system_font();
 	if(!xitk_font_load_one(display, fsname, xtfs)) {
	  XITK_WARNING("loading font \"%s\" failed, default and system fonts \"%s\" and \"%s\" failed too\n", font, fdname, fsname);
	  free(xtfs);
	  pthread_mutex_unlock(&xitk->font_cache->mutex);

	  /* Maybe broken XMB support */
	  if(xitk_get_xmb_enability()) {
	    xitk_font_t *xtfs_fallback;

	    xitk_set_xmb_enability(0);
	    if((xtfs_fallback = xitk_font_load_font(display, font)))
	      XITK_WARNING("XMB support seems broken on your system, add \"font.xmb = 0\" in your ~/.xitkrc\n");
	    
	    return xtfs_fallback;
	  }
	  else
	    return NULL;
        }
      }
    }

    xtfs->name = strdup(font);
    xtfs->display = display;

#ifdef LOG
    printf("xiTK: loaded new \"%s\", list: %zu, cache: %zu\n", xtfs->name, xitk->font_cache->nlist, xitk->font_cache->n);
#endif
  }

  /* add the font into list */
  list_item         = xitk_xmalloc(sizeof(xitk_font_list_item_t));
  list_item->font   = xtfs;
  list_item->number = 1;
  xitk_dlist_add_tail (&xitk->font_cache->loaded, &list_item->node);
  xitk->font_cache->nlist++;

  pthread_mutex_unlock(&xitk->font_cache->mutex);

  return xtfs;
}

/*
 *
 */
void xitk_font_unload_font(xitk_font_t *xtfs) {
  xitk_t *xitk = gXitk;
  xitk_font_list_item_t *item;
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
#ifndef WITH_XFT
#ifdef WITH_XMB
  if(xitk_get_xmb_enability())
    ABORT_IF_NULL(xtfs->fontset);
  else
#endif
#endif
    ABORT_IF_NULL(xtfs->font);

  pthread_mutex_lock(&xitk->font_cache->mutex);
  /* search the font in the list */
  item = cache_get_from_list(xtfs->display, xtfs->name);
  /* it must be there */
  ABORT_IF_NULL(item);

  if(--item->number == 0) {
    xitk_dnode_remove (&item->node);
    xitk->font_cache->nlist--;
    xitk_cache_add_item(item->font);
    free(item);
  }

  pthread_mutex_unlock(&xitk->font_cache->mutex);
}

/*
 *
 */
void xitk_font_draw_string(xitk_font_t *xtfs, Pixmap pix, GC gc, 
			   int x, int y, const char *text, 
			   size_t nbytes) {

  xitk_t *xitk = gXitk;
#ifdef DEBUG
  if (nbytes > strlen(text) + 1) {
    XITK_WARNING("draw: %zu > %zu\n", nbytes, strlen(text));
  }
#endif
  
  XLOCK (xitk_x_lock_display, xtfs->display);
#ifndef WITH_XFT
# ifdef WITH_XMB
  if(xitk_get_xmb_enability())
    XmbDrawString(xtfs->display, pix, xtfs->fontset, gc, x, y, text, nbytes);
  else
# endif
    XDrawString(xtfs->display, pix, gc, x, y, text, nbytes);
#else
  {
    int           screen   = DefaultScreen( xtfs->display );
    Visual       *visual   = DefaultVisual( xtfs->display, screen );
    Colormap      colormap = DefaultColormap( xtfs->display, screen );
    XGCValues     gc_color;
    XColor        paint_color;
    XRenderColor  xr_color;
    XftColor      xcolor;
    XftDraw      *xft_draw;
    char          buf[2048];
    xitk_recode_string_t rs;

    rs.src = text;
    rs.ssize = strlen (text);
    if ((size_t)nbytes < rs.ssize)
      rs.ssize = nbytes;
    rs.buf = buf;
    rs.bsize = sizeof (buf);
    xitk_recode2_do (xitk->font_cache->xr, &rs);

    XGetGCValues(xtfs->display, gc, GCForeground, &gc_color);
    paint_color.pixel = gc_color.foreground;
    XQueryColor(xtfs->display, colormap, &paint_color);
    xr_color.red   = paint_color.red;
    xr_color.green = paint_color.green;
    xr_color.blue  = paint_color.blue;
    xr_color.alpha = (short)-1;
    xft_draw       = XftDrawCreate(xtfs->display, pix, visual, colormap);
    XftColorAllocValue(xtfs->display, visual, colormap, &xr_color, &xcolor);
    XftDrawStringUtf8(xft_draw, &xcolor, xtfs->font, x, y, (FcChar8 *)rs.res, rs.rsize);
    XftColorFree(xtfs->display, visual, colormap, &xcolor);
    XftDrawDestroy(xft_draw);

    xitk_recode2_done (xitk->font_cache->xr, &rs);
  }
#endif
  XUNLOCK (xitk_x_unlock_display, xtfs->display);
}

#ifndef WITH_XFT
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
#endif
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

/*
 *
 */
int xitk_font_get_text_width(xitk_font_t *xtfs, const char *c, int nbytes) {
  int width;
 
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);
#ifdef WITH_XFT
  ABORT_IF_NULL(xtfs->font);

  xitk_font_text_extent(xtfs, c, nbytes, NULL, NULL, &width, NULL, NULL);
# else
# ifdef WITH_XMB
  if(xitk_get_xmb_enability()) {
    ABORT_IF_NULL(xtfs->fontset);
    
    xitk_font_text_extent(xtfs, c, nbytes, NULL, NULL, &width, NULL, NULL);
  }
  else
# endif
    {
      ABORT_IF_NULL(xtfs->font);
      
      XLOCK (xitk_x_lock_display, xtfs->display);
      
      if(xitk_font_is_font_8(xtfs))
	width = XTextWidth (xtfs->font, c, nbytes);
      else
	width = XTextWidth16 (xtfs->font, (XChar2b *)c, nbytes);
      
      XUNLOCK (xitk_x_unlock_display, xtfs->display);
    }
#endif
 
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
int xitk_font_get_char_width(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes) {
#ifndef WITH_XFT
  unsigned int  ch = (*c & 0xff);
  int           width;
  XCharStruct  *chars;
#ifdef WITH_XMB
  mbstate_t state;
  size_t    n;
  
  if(xitk_get_xmb_enability()) {
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
  }
  else
# endif
    {
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
    }

  return 0;
#endif
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);
  ABORT_IF_NULL(c);
  return xitk_font_get_text_width(xtfs, c, 1);
}

/*
 *
 */
int xitk_font_get_text_height(xitk_font_t *xtfs, const char *c, int nbytes) {
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
int xitk_font_get_char_height(xitk_font_t *xtfs, const char *c, int maxnbytes, int *nbytes) {
# ifdef WITH_XMB
  mbstate_t state;
  size_t    n;

  if(xitk_get_xmb_enability()) {
    memset(&state, '\0', sizeof(mbstate_t));
    n = mbrtowc(NULL, c, maxnbytes, &state);
    
    if(n == (size_t)-1 || n == (size_t)-2) 
      return 0;
    
    if(nbytes) 
      *nbytes = n;
    
    return xitk_font_get_text_height(xtfs, c, n);
  }
  else
#endif
    return (xitk_font_get_text_height(xtfs, c, 1));
}

/*
 *
 */
void xitk_font_text_extent(xitk_font_t *xtfs, const char *c, int nbytes,
			   int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  xitk_t *xitk = gXitk;
#ifndef WITH_XFT  
  XCharStruct ov;
  int         dir;
  int         fascent, fdescent;
#else
  XGlyphInfo  xft_extents;
#endif
#if defined(WITH_XMB) && !defined(WITH_XFT)
  XRectangle  ink, logic;
#endif
  
  if (nbytes <= 0) {
#ifdef DEBUG
    if (nbytes < 0)
      XITK_WARNING("nbytes < 0\n");
#endif
    if (width) *width     = 0;
    if (ascent) *ascent   = 0;
    if (descent) *descent = 0;
    if (lbearing) *lbearing = 0;
    if (rbearing) *rbearing = 0;
    return;
  }
    
#ifdef DEBUG
  if ((size_t)nbytes > strlen(c) + 1) {
    XITK_WARNING("extent: %d > %zu\n", nbytes, strlen(c));
  }
#endif

#ifdef WITH_XFT
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->font);
  ABORT_IF_NULL(c);

  /* recode right part of string */
  {
    char buf[2048];
    xitk_recode_string_t rs;

    rs.src = c;
    rs.ssize = strlen (c);
    if ((size_t)nbytes < rs.ssize)
      rs.ssize = nbytes;
    rs.buf = buf;
    rs.bsize = sizeof (buf);
    xitk_recode2_do (xitk->font_cache->xr, &rs);

    /* FIXME: XftTextExtentsUtf8 () seems to be non reentrant - at least when called
     * with same font. We probably need to mutex it when XLockDisplay () is turned off. */
    XLOCK (xitk_x_lock_display, xtfs->display);
    XftTextExtentsUtf8 (xtfs->display, xtfs->font, (FcChar8 *)rs.res, rs.rsize, &xft_extents);
    XUNLOCK (xitk_x_unlock_display, xtfs->display);

    xitk_recode2_done (xitk->font_cache->xr, &rs);
  }

  if (width) *width       = xft_extents.xOff;
  /* Since Xft fonts have more top and bottom space rows than Xmb/Xcore fonts, */
  /* we tweak ascent and descent to appear the same like Xmb/Xcore, so we get  */
  /* unified vertical text positions. We must however be aware to eventually   */
  /* reserve space for these rows when drawing the text.                       */
  /* Following trick works for our font sizes 10...14.                         */
  if (ascent) *ascent     = xtfs->font->ascent - (xtfs->font->ascent - 9) / 2;
  if (descent) *descent   = xtfs->font->descent - (xtfs->font->descent - 0) / 2;
  if (lbearing) *lbearing = -xft_extents.x;
  if (rbearing) *rbearing = -xft_extents.x + xft_extents.width;
#else
#ifdef WITH_XMB
  if(xitk_get_xmb_enability()) {
    ABORT_IF_NULL(xtfs);
    ABORT_IF_NULL(xtfs->fontset);
    ABORT_IF_NULL(c);
    
    XLOCK (xitk_x_lock_display, xtfs->display);
    XmbTextExtents(xtfs->fontset, c, nbytes, &ink, &logic);
    XUNLOCK (xitk_x_unlock_display, xtfs->display);
    if (!logic.width || !logic.height) {
      /* XmbTextExtents() has problems, try char-per-char counting */
      mbstate_t state;
      size_t    i = 0, n;
      int       height = 0, width = 0, y = 0, lb = 0, rb;
      
      memset(&state, '\0', sizeof(mbstate_t));
      
      XLOCK (xitk_x_lock_display, xtfs->display);
      while (i < nbytes) {
	n = mbrtowc(NULL, c + i, nbytes - i, &state);
	if (n == (size_t)-1 || n == (size_t)-2 || i + n > nbytes) n = 1;
	XmbTextExtents(xtfs->fontset, c + i, n, &ink, &logic);
	if (logic.height > height) height = logic.height;
	if (logic.y < y) y = logic.y; /* y is negative! */
	if (i == 0) lb = ink.x; /* left bearing of string */
	width += logic.width;
	i += n;
      }
      rb = width - logic.width + ink.x + ink.width; /* right bearing of string */
      XUNLOCK (xitk_x_unlock_display, xtfs->display);
      
      if (!height || !width) {
	/* char-per-char counting fails too */
	XITK_WARNING("extents of the font \"%s\" are %dx%d!\n", xtfs->name, width, height);
	if (!height) height = 12, y = -10;
	if (!width) lb = rb = 0;
      } 
      
      logic.height = height;
      logic.width = width;
      logic.y = y;
      ink.x = lb;
      ink.width = rb - lb;
    }
    
    if (width) *width     = logic.width;
    if (ascent) *ascent   = -logic.y;
    if (descent) *descent = logic.height + logic.y;
    if (lbearing) *lbearing = ink.x;
    if (rbearing) *rbearing = ink.x + ink.width;
  }
  else
#endif
    {
      ABORT_IF_NULL(xtfs);
      ABORT_IF_NULL(xtfs->font);
      ABORT_IF_NULL(xtfs->display);
      ABORT_IF_NULL(c);
      
      XLOCK (xitk_x_lock_display, xtfs->display);
      if(xitk_font_is_font_8(xtfs))
	XTextExtents(xtfs->font, c, nbytes, &dir, &fascent, &fdescent, &ov);
      else
	XTextExtents16(xtfs->font, (XChar2b *)c, (nbytes / 2), &dir, &fascent, &fdescent, &ov);
      XUNLOCK (xitk_x_unlock_display, xtfs->display);
      
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
  int ascent;
  
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);
#ifndef WITH_XFT
#ifdef WITH_XMB 
  if(xitk_get_xmb_enability())
    ABORT_IF_NULL(xtfs->fontset);
  else
#endif
#endif
    ABORT_IF_NULL(xtfs->font);
  
  xitk_font_text_extent(xtfs, c, strlen(c), NULL, NULL, NULL, &ascent, NULL);

  return ascent;
}

/*
 *
 */
int xitk_font_get_descent(xitk_font_t *xtfs, const char *c) {
  int descent;
 
  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
  ABORT_IF_NULL(c);
#ifndef WITH_XFT
#ifdef WITH_XMB 
  if(xitk_get_xmb_enability())
    ABORT_IF_NULL(xtfs->fontset);
  else
#endif
#endif
    ABORT_IF_NULL(xtfs->font);
  
  xitk_font_text_extent(xtfs, c, strlen(c), NULL, NULL, NULL, NULL, &descent);

  return descent;
}

/*
 *
 */
void xitk_font_set_font(xitk_font_t *xtfs, GC gc) {

  ABORT_IF_NULL(xtfs);
  ABORT_IF_NULL(xtfs->display);
#ifndef WITH_XFT
# ifdef WITH_XMB
  if(xitk_get_xmb_enability())
    ;
  else
# endif
    {
      XLOCK (xitk_x_lock_display, xtfs->display);
      XSetFont(xtfs->display, gc, xitk_font_get_font_id(xtfs));
      XUNLOCK (xitk_x_unlock_display, xtfs->display);
    }
#else
  (void)gc;
#endif
}
