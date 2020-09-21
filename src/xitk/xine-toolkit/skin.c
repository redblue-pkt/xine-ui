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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>

#include "utils.h"
#include "_xitk.h"
#include "slider.h"

#undef DEBUG_SKIN

/************************************************************************************
 *                                     PRIVATES
 ************************************************************************************/

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
  char b[1024];
  const char *name;
  int try;

  if (!skonfig || !pixmap)
    return;

  if (image) {
    image->x = image->y = image->width = image->height = 0;
    image->image = NULL;
  }
  name = pixmap;

  do {
    uint32_t v;
    uint8_t z;
    const uint8_t *p;
    const char *part = strchr (pixmap, '|');

    if (!part)
      break;
    if (part > pixmap + sizeof (b) - 2 - 1)
      part = pixmap + sizeof (b) - 2 - 1;
    memcpy (b + 2, pixmap, part - pixmap);
    b[2 + part - pixmap] = 0;
    name = b + 2;
    if (!image)
      break;

    p = (const uint8_t *)part + 1;
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
    /* already loaded? */
    {
      xitk_skin_img_t here;
      int pos;
      here.name = name;
      pos = xine_sarray_binary_search (skonfig->imgs, &here);
      if (pos >= 0) {
        xitk_skin_img_t *si = xine_sarray_get (skonfig->imgs, pos);
        if (format)
          si->format = format;
        if (!image)
          return;
        if (!si->image) {
          si->image = xitk_image_load_image (skonfig->xitk, name);
          if (si->image && si->format)
            xitk_image_set_pix_font (si->image, si->format);
        }
        if (si->image) {
          image->image = si->image;
          break;
        }
      }
    }
    /* load now */
    {
      xitk_skin_img_t *nimg;
      size_t nlen = strlen (name) + 1;
      char *nmem = malloc (sizeof (*nimg) + nlen);
      if (!nmem)
        return;
      nimg = (xitk_skin_img_t *)nmem;
      nmem += sizeof (*nimg);
      memcpy (nmem, name, nlen);
      nimg->name = nmem;
      nimg->format = format;
      nimg->image = NULL;
      xine_sarray_add (skonfig->imgs, nimg);
      if (!image)
        return;
      nimg->image = xitk_image_load_image (skonfig->xitk, nmem);
      if (nimg->image) {
        if (nimg->format)
          xitk_image_set_pix_font (nimg->image, nimg->format);
        image->image = nimg->image;
        break;
      }
      if (try < 2)
        return;
      /* try fallback texture */
      memcpy (b, "//", 2);
      if (nlen > sizeof (b) - 2 - 12)
        nlen = sizeof (b) - 2 - 12;
      if (name != b + 2) {
        memcpy (b + 2, name, nlen);
        name = b + 2;
      }
      while (b[1 + (--nlen)] != '/') ;
      while (b[1 + (--nlen)] != '/') ;
      memcpy (b + 2 + nlen, "missing.png", 12);
      image->x = 0;
      image->y = 0;
      image->width = 0;
      image->height = 0;
      format = NULL;
    }
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

/*
 *
 */
//#warning FIXME
static char *_expanded(xitk_skin_config_t *skonfig, char *cmd) {
  char *p;
  char *buf2 = NULL;
  char  buf[BUFSIZ], var[BUFSIZ];
  
  ABORT_IF_NULL(skonfig);

  if( ! ( cmd && strchr(cmd, '$') ) ) return NULL;

  buf2 = calloc(BUFSIZ, sizeof(char));

  strlcpy(buf, cmd, sizeof(buf));

  buf2[0] = 0;
      
  p = buf;

  while(*p != '\0') {
    switch(*p) {

      /*
       * Prefix of variable names.
       */
    case '$':
      memset(&var, 0, sizeof(var));
      if(sscanf(p, "$\(%[A-Z-_])", &var[0])) {

	p += (strlen(var) + 2);

	/*
	 * Now check variable validity
	 */
	if(!strncmp("SKIN_PARENT_PATH", var, strlen(var))) {
	  if(skonfig->path) {
	    char *ppath;
	    char *z;
		
	    ppath = strdup(skonfig->path);
	    if((z = strrchr(ppath, '/')) != NULL) {
	      *z = '\0';
	      strlcat(buf2, ppath, BUFSIZ);
	    }
	    free(ppath);
	  }
	}
	else if(!strncmp("SKIN_VERSION", var, strlen(var))) {
	  if(skonfig->version >= 0)
	    snprintf(buf2+strlen(buf2), sizeof(buf2)-strlen(buf2), "%d", skonfig->version);
	}
	else if(!strncmp("SKIN_AUTHOR", var, strlen(var))) {
	  if(skonfig->author)
	    strlcat(buf2, skonfig->author, BUFSIZ);
	}
	else if(!strncmp("SKIN_PATH", var, strlen(var))) {
	  if(skonfig->path)
	    strlcat(buf2, skonfig->path, BUFSIZ);
	}
	else if(!strncmp("SKIN_NAME", var, strlen(var))) {
	  if(skonfig->name)
	    strlcat(buf2, skonfig->name, BUFSIZ);
	}
	else if(!strncmp("SKIN_DATE", var, strlen(var))) {
	  if(skonfig->date)
	    strlcat(buf2, skonfig->date, BUFSIZ);
	}
	else if(!strncmp("SKIN_URL", var, strlen(var))) {
	  if(skonfig->url)
	    strlcat(buf2, skonfig->url, BUFSIZ);
	}
	else if(!strncmp("HOME", var, strlen(var))) {
	  if(skonfig->url)
	    strlcat(buf2, xine_get_homedir(), BUFSIZ);
	}
	/* else ignore */
      }
      break;
	  
    default:
      {
	const size_t buf2_len = strlen(buf2);
	buf2[buf2_len + 1] = 0;
	buf2[buf2_len] = *p;
      }
      break;
    }
    p++;
  }

  return buf2;
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
  s->info.label_color            = NULL;
  s->info.label_color_focus      = NULL;
  s->info.label_color_click      = NULL;
  s->info.label_fontname         = NULL;
  s->info.browser_entries        = -2;
  s->info.direction              = DIRECTION_LEFT; /* Compatibility */
  s->info.max_buttons            = 0;
}

/*
 * Return alignement value.
 */
static int skin_get_align_value(const char *val) {
  static struct {
    const char *str;
    int value;
  } aligns[] = {
    { "left",   ALIGN_LEFT   }, 
    { "center", ALIGN_CENTER }, 
    { "right",  ALIGN_RIGHT  },
    { NULL,     0 }
  };
  int i;
  
  ABORT_IF_NULL(val);
  
  for(i = 0; aligns[i].str != NULL; i++) {
    if(!(strcasecmp(aligns[i].str, val)))
      return aligns[i].value;
  }

  return ALIGN_CENTER;
}

/*
 * Return direction
 */
static int skin_get_direction(const char *val) {
  static struct {
    const char *str;
    int         value;
  } directions[] = {
    { "left",   DIRECTION_LEFT   }, 
    { "right",  DIRECTION_RIGHT  }, 
    { "up",     DIRECTION_UP     },
    { "down",   DIRECTION_DOWN   },
    { NULL,     0 }
  };
  int   i;

  ABORT_IF_NULL(val);
  
  for(i = 0; directions[i].str != NULL; i++) {
    if(!(strcasecmp(directions[i].str, val)))
      return directions[i].value;
  }

  return DIRECTION_LEFT;
}

static int _skin_make_filename (xitk_skin_config_t *skonfig, const char *name, char **dest) {
  size_t plen, nlen;
  if (*dest)
    return 0;
  plen = strlen (skonfig->path);
  nlen = strlen (name);
  *dest = (char *)xitk_xmalloc (plen + nlen + 2);
  if (!*dest)
    return 0;
  memcpy (*dest, skonfig->path, plen);
  (*dest)[plen] = '/';
  memcpy (*dest + plen + 1, name, nlen + 1);
  return 1;
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

    printf("Skin name '%s'\n", skonfig->name);
    printf("     version   '%d'\n", skonfig->version);
    printf("     author    '%s'\n", skonfig->author);
    printf("     date      '%s'\n", skonfig->date);
    printf("     load cmd  '%s'\n", skonfig->load_command);
    printf("     uload cmd '%s'\n", skonfig->unload_command);
    printf("     URL       '%s'\n", skonfig->url);
    printf("     logo      '%s'\n", skonfig->logo);
    printf("     animation '%s'\n", skonfig->animation);

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
      printf("  color       = '%s'\n", s->info.label_color);
      printf("  color focus = '%s'\n", s->info.label_color_focus);
      printf("  color click = '%s'\n", s->info.label_color_click);
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
  xitk_skin_element_t *s;
  int r;

  if (!skonfig || !str)
    return NULL;

  r = xine_sarray_binary_search (skonfig->elements, (char *)str);
  if (r < 0)
    return NULL;
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

  skonfig->elements = xine_sarray_new (128, (xine_sarray_comparator_t)strcasecmp);
  skonfig->imgs     = xine_sarray_new (128, xitk_simg_cmp);

  skonfig->xitk     = xitk;
  skonfig->version  = -1;
  skonfig->celement = NULL;
  skonfig->name     = skonfig->author 
                    = skonfig->date 
                    = skonfig->url
                    = skonfig->load_command 
                    = skonfig->unload_command 
                    = skonfig->logo 
                    = skonfig->animation 
                    = NULL;
  skonfig->skinfile = skonfig->path = NULL;

  skonfig->ln = skonfig->buf;
  
  return skonfig;
}

/*
 * Release all allocated memory of a xitk_skin_config_t* variable (element chained struct too).
 */
void xitk_skin_free_config(xitk_skin_config_t *skonfig) {

  ABORT_IF_NULL(skonfig);
  pthread_mutex_lock (&skonfig->skin_mutex);
  {
    int n = xine_sarray_size (skonfig->elements);
    for (n--; n >= 0; n--) {
      xitk_skin_element_t *s = xine_sarray_get (skonfig->elements, n);
      XITK_FREE(s->info.pixmap_name);
      XITK_FREE(s->info.slider_pixmap_pad_name);
      XITK_FREE(s->info.label_pixmap_font_name);
      XITK_FREE(s->info.label_pixmap_font_format);
      XITK_FREE(s->info.label_color);
      XITK_FREE(s->info.label_color_focus);
      XITK_FREE(s->info.label_color_click);
      XITK_FREE(s->info.label_fontname);
      XITK_FREE(s);
    }
  }
  xine_sarray_delete (skonfig->elements);

  skin_free_imgs (skonfig);
  xine_sarray_delete (skonfig->imgs);
  
  XITK_FREE(skonfig->name);
  XITK_FREE(skonfig->author);
  XITK_FREE(skonfig->date);
  XITK_FREE(skonfig->url);
  XITK_FREE(skonfig->logo);
  XITK_FREE(skonfig->animation);
  XITK_FREE(skonfig->path);
  XITK_FREE(skonfig->load_command);
  XITK_FREE(skonfig->unload_command);
  XITK_FREE(skonfig->skinfile);

  pthread_mutex_unlock (&skonfig->skin_mutex);
  pthread_mutex_destroy (&skonfig->skin_mutex);

  XITK_FREE(skonfig);
}

static int _skin_string_index (const char * const *list, size_t size, const char *s) {
  uint32_t b = 0, e = size;

  do {
    uint32_t m = (b + e) >> 1;
    int d = strcmp (s, list[m]);

    if (d < 0)
      e = m;
    else if (d > 0)
      b = m + 1;
    else
      return m;
  } while (b != e);
  return -1;
}

static void _skin_parse_2 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree, xitk_cfg_parse_t *sub) {
  char dummy[8] = {[0] = 0}, *key = sub->key >= 0 ? text + sub->key : dummy;
  char *val = sub->value >= 0 ? text + sub->value : dummy;
  xitk_skin_element_t *s = skonfig->celement;
  static const char * const list1[] = {
    "browser",
    "coords",
    "direction",
    "enable",
    "label",
    "max_buttons",
    "pixmap",
    "slider",
    "visible"
  };
  int n = _skin_string_index (list1, sizeof (list1) / sizeof (list1[0]), key);

  switch (n) {
    case 0: { /* browser */
        xitk_cfg_parse_t *sub2;

        for (sub2 = sub->first_child ? tree + sub->first_child : NULL; sub2; sub2 = sub2->next ? tree + sub2->next : NULL) {
          char *key2 = sub2->key >= 0 ? text + sub2->key : dummy, *val2 = sub2->value >= 0 ? text + sub2->value : dummy;

          if (!strcmp (key2, "entries"))
            s->info.browser_entries = strtol (val2, &val2, 10);
        }
      }
      break;
    case 1: { /* coords */
        xitk_cfg_parse_t *sub2;

        for (sub2 = sub->first_child ? tree + sub->first_child : NULL; sub2; sub2 = sub2->next ? tree + sub2->next : NULL) {
          char *key2 = sub2->key >= 0 ? text + sub2->key : dummy, *val2 = sub2->value >= 0 ? text + sub2->value : dummy;

          if (!strcmp (key2, "x"))
            s->info.x = strtol (val2, &val2, 10);
          else if (!strcmp (key2, "y"))
            s->info.y = strtol (val2, &val2, 10);
        }
      }
      break;
    case 2: /* direction */
      s->info.direction = skin_get_direction (val);
      break;
    case 3: /* enable */
      s->info.enability = xitk_get_bool_value (val);
      break;
    case 4: { /* label */
        xitk_cfg_parse_t *sub2;
        static const char * const list2[] = {
          "align",
          "animation",
          "color",
          "color_click",
          "color_focus",
          "font",
          "length",
          "pixmap",
          "pixmap_focus",
          "pixmap_format",
          "print",
          "static",
          "step",
          "timer",
          "y"
        };

        s->info.label_y = 0;
        s->info.label_printable = 1;
        s->info.label_animation_step = 1;
        s->info.label_animation_timer = xitk_get_timer_label_animation ();
        s->info.label_alignment = ALIGN_CENTER;

        for (sub2 = sub->first_child ? tree + sub->first_child : NULL; sub2; sub2 = sub2->next ? tree + sub2->next : NULL) {
          char *key2 = sub2->key >= 0 ? text + sub2->key : dummy, *val2 = sub2->value >= 0 ? text + sub2->value : dummy;

          n = _skin_string_index (list2, sizeof (list2) / sizeof (list2[0]), key2);
          switch (n) {
            case 0: /* align */
              s->info.label_alignment = skin_get_align_value (val2);
              break;
            case 1: /* animation */
              s->info.label_animation = xitk_get_bool_value (val2);
              break;
            case 2: /* color */
              s->info.label_color = strdup (val2);
              break;
            case 3: /* color_click */
              s->info.label_color_click = strdup (val2);
              break;
            case 4: /* color_focus */
              s->info.label_color_focus = strdup (val2);
              break;
            case 5: /* font */
              s->info.label_fontname = strdup (val2);
              break;
            case 6: /* length */
              s->info.label_length = strtol (val2, &val2, 10);
              break;
            case 7: /* pixmap */
              if (_skin_make_filename (skonfig, val2, &s->info.label_pixmap_font_name))
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_font_name, s->info.label_pixmap_font_format);
              break;
            case 8: /* pixmap_focus */
              if (_skin_make_filename (skonfig, val2, &s->info.label_pixmap_highlight_font_name))
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_highlight_font_name, s->info.label_pixmap_font_format);
              break;
            case 9: /* pixmap_format */
              if (!s->info.label_pixmap_font_format) {
                s->info.label_pixmap_font_format = strdup (val2);
                _skin_load_img (skonfig, NULL, s->info.label_pixmap_font_name, s->info.label_pixmap_font_format);
              }
              break;
            case 10: /* print */
              s->info.label_printable = xitk_get_bool_value (val2);
              break;
            case 11: /* static */
              s->info.label_staticity = xitk_get_bool_value (val2);
              break;
            case 12: /* step */
              s->info.label_animation_step = strtol (val2, &val2, 10);
              break;
            case 13: /* timer */
              s->info.label_animation_timer = strtol (val2, &val2, 10);
              break;
            case 14: /* y */
              s->info.label_y = strtol (val2, &val2, 10);
              break;
          }
        }
      }
      break;
    case 5: /* max_buttons */
      s->info.max_buttons = strtol (val, &val, 10);
      break;
    case 6: /* pixmap */
      _skin_make_filename (skonfig, val, &s->info.pixmap_name);
      break;
    case 7: { /* slider */
        xitk_cfg_parse_t *sub2;

        for (sub2 = sub->first_child ? tree + sub->first_child : NULL; sub2; sub2 = sub2->next ? tree + sub2->next : NULL) {
          char *key2 = sub2->key >= 0 ? text + sub2->key : dummy, *val2 = sub2->value >= 0 ? text + sub2->value : dummy;
      
          if (!strcmp (key2, "pixmap")) {
            _skin_make_filename (skonfig, val2, &skonfig->celement->info.slider_pixmap_pad_name);
          } else if (!strcmp (key2, "radius")) {
            skonfig->celement->info.slider_radius = strtol (val2, &val2, 10);
          } else if (!strcmp (key2, "type")) {
            if (!strcmp ("horizontal", val2)) {
              s->info.slider_type = XITK_HSLIDER;
            } else if (!strcmp ("vertical", val2)) {
              s->info.slider_type = XITK_VSLIDER;
            } else if (!strcmp ("rotate", val2)) {
              s->info.slider_type = XITK_RSLIDER;
            } else {
              s->info.slider_type = XITK_HSLIDER;
            }
          }
        }
      }
      break;
    case 8: /* visible */
      s->info.visibility = xitk_get_bool_value (val);
      break;
  }
}

static void _skin_parse_1 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree, xitk_cfg_parse_t *entry) {
  char dummy[8] = {[0] = 0}, *key = text + entry->key, *val = entry->value >= 0 ? text + entry->value : dummy;

  if (entry->first_child) {
    xitk_skin_element_t *s = xitk_xmalloc (sizeof (*s));

    if (s) {
      xitk_cfg_parse_t *sub;

      _nullify_me (s);
      skonfig->celement = s;
      strlcpy (s->section, key, sizeof (s->section));
      s->info.visibility = s->info.enability = 1;
      xine_sarray_add (skonfig->elements, s);

      for (sub = tree + entry->first_child; sub; sub = sub->next ? tree + sub->next : NULL) {
        _skin_parse_2 (skonfig, text, tree, sub);
      }
    }
  } else if (!strcmp (key, "unload_command")) {
    skonfig->unload_command = _expanded (skonfig, val);
  } else if (!strcmp (key, "load_command")) {
    skonfig->load_command = _expanded (skonfig, val);
  } else if (!strcmp (key, "animation")) {
    skonfig->animation = strdup (val);
  } else if (!strcmp (key, "version")) {
    skonfig->version = strtol (val, &val, 10);
  } else if (!strcmp (key, "author")) {
    skonfig->author = strdup (val);
  } else if (!strcmp (key, "name")) {
    skonfig->name = strdup (val);
  } else if (!strcmp (key, "date")) {
    skonfig->date = strdup (val);
  } else if (!strcmp (key, "logo")) {
    skonfig->logo = _expanded (skonfig, val);
  } else if (!strcmp (key, "url")) {
    skonfig->url = strdup (val);
  } else {
    XITK_WARNING ("wrong section entry found: '%s'\n", key);
  }
}

static void _skin_parse_0 (xitk_skin_config_t *skonfig, char *text, xitk_cfg_parse_t *tree) {
  xitk_cfg_parse_t *entry;

  for (entry = tree->first_child ? tree + tree->first_child : NULL; entry;
    entry = entry->next ? tree + entry->next : NULL) {
    char buf[1] = {0}, *key = entry->key >= 0 ? text + entry->key : buf;

    if (!strncmp (key, "skin.", 5)) {
      entry->key += 5;
      _skin_parse_1 (skonfig, text, tree, entry);
    }
  }
}

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *skonfig, const char *path, const char *filename) {
  char buf[2048];

  ABORT_IF_NULL(skonfig);
  ABORT_IF_NULL(path);
  ABORT_IF_NULL(filename);

  pthread_mutex_lock (&skonfig->skin_mutex);

  skonfig->path     = strdup(path);
  skonfig->skinfile = strdup(filename);

  snprintf(buf, sizeof(buf), "%s/%s", skonfig->path, skonfig->skinfile);

  if((skonfig->fd = fopen(buf, "r")) != NULL) {
    {
      size_t size;
      char *text;
      xitk_cfg_parse_t *tree;

      fseek (skonfig->fd, 0, SEEK_END);
      size = ftell (skonfig->fd);
      fseek (skonfig->fd, 0, SEEK_SET);
      if (size > (1 << 20) - 1)
        size = (1 << 20) - 1;
      text = malloc (size + 1);
      if (text) {
        int r = fread (text, 1, size, skonfig->fd);

        if (r > 0) {
          size = r;
          text[size] = 0;
          tree = xitk_cfg_parse (text, /* XITK_CFG_PARSE_DEBUG */ 0);
          _skin_parse_0 (skonfig, text, tree);
          xitk_cfg_unparse (tree);
        }
        free (text);
      }
      fseek (skonfig->fd, 0, SEEK_SET);
    }
    fclose(skonfig->fd);
  }
  else {
    XITK_WARNING("%s(): Unable to open '%s' file.\n", __FUNCTION__, skonfig->skinfile);
    XITK_FREE(skonfig->skinfile);
    pthread_mutex_unlock (&skonfig->skin_mutex);
    return 0;
  }

  if (!skonfig->celement) {
    XITK_WARNING("%s(): no valid skin element found in '%s/%s'.\n",
		 __FUNCTION__, skonfig->path, skonfig->skinfile);
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
    xitk_system(0, skonfig->load_command);

  pthread_mutex_unlock (&skonfig->skin_mutex);
  return 1;
}

/*
 * Unload (free) xitk_skin_config_t object.
 */
void xitk_skin_unload_config(xitk_skin_config_t *skonfig) {
  if(skonfig) {

    if(skonfig->unload_command)
      xitk_system(0, skonfig->unload_command);

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
  
  return skonfig->animation;
}

/*
 *
 */
const char *xitk_skin_get_logo(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);
  
  return skonfig->logo;
}

const xitk_skin_element_info_t *xitk_skin_get_info (xitk_skin_config_t *skin, const char *element_name) {
  xitk_skin_element_t *s;

  if (!skin || !element_name)
    return NULL;

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
