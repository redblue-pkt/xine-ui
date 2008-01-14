/* 
 * Copyright (C) 2000-2004 the xine project
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
 * $Id$
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

#undef DEBUG_SKIN

/************************************************************************************
 *                                     PRIVATES
 ************************************************************************************/

static pthread_mutex_t  skin_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 *
 */
static xitk_image_t *skin_load_to_cache(xitk_skin_config_t *skonfig, char *pixmap) {

  if(skonfig && pixmap) {
    xitk_image_t  *image  = xitk_image_load_image(skonfig->im, pixmap);
    cache_entry_t *cache  = skonfig->cache;
    cache_entry_t *ncache = NULL;

    if(image) {

      if(cache) {
	while(cache) {
	  if(!strcmp(cache->filename, pixmap)) {
	    xitk_image_free_image(skonfig->im, &image);
	    return cache->image;
	  }
	  cache = cache->next;
	}
	cache = skonfig->cache;
      }

      ncache           = (cache_entry_t *) xitk_xmalloc(sizeof(cache_entry_t));
      ncache->image    = image;
      ncache->filename = strdup(pixmap);
      ncache->next     = NULL;
    }
    else
      return image;
    
    if(!cache) {
      skonfig->cache = ncache;
    }
    else {
      while(cache->next)
	cache = cache->next;
      
      cache->next  = ncache;
    }

    return image;
  }

  return NULL;
}

static void skin_free_cache(xitk_skin_config_t *skonfig) {
  cache_entry_t *pcache, *cache  = skonfig->cache;
  
  while(cache) {
    pcache = cache;
    
    xitk_image_free_image(skonfig->im, &(cache->image));
    free(cache->filename);
    
    cache  = cache->next;
    free(pcache);
  }

  skonfig->cache = NULL;
}

/*
 *
 */
#warning FIXME
static char *_expanded(xitk_skin_config_t *skonfig, char *cmd) {
  char *p;
  char *buf2 = NULL;
  char  buf[BUFSIZ], var[BUFSIZ];
  
  ABORT_IF_NULL(skonfig);

  if(cmd) {

    if(strchr(cmd, '$')) {
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
		  strlcat(buf2, ppath, sizeof(buf2));
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
		strlcat(buf2, skonfig->author, sizeof(buf2));
	    }
	    else if(!strncmp("SKIN_PATH", var, strlen(var))) {
	      if(skonfig->path)
		strlcat(buf2, skonfig->path, sizeof(buf2));
	    }
	    else if(!strncmp("SKIN_NAME", var, strlen(var))) {
	      if(skonfig->name)
		strlcat(buf2, skonfig->name, sizeof(buf2));
	    }
	    else if(!strncmp("SKIN_DATE", var, strlen(var))) {
	      if(skonfig->date)
		strlcat(buf2, skonfig->date, sizeof(buf2));
	    }
	    else if(!strncmp("SKIN_URL", var, strlen(var))) {
	      if(skonfig->url)
		strlcat(buf2, skonfig->url, sizeof(buf2));
	    }
	    else if(!strncmp("HOME", var, strlen(var))) {
	      if(skonfig->url)
		strlcat(buf2, xitk_get_homedir(), sizeof(buf2));
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
    }
  }

  return buf2;
}

/*
 * Nullify all entried from s element.
 */
static void _nullify_me(xitk_skin_element_t *s) {
  s->prev            = NULL;
  s->next            = NULL;
  s->section         = NULL;
  s->pixmap          = NULL;
  s->pixmap_pad      = NULL;
  s->pixmap_font     = NULL;
  s->color           = NULL;
  s->color_focus     = NULL;
  s->color_click     = NULL;
  s->font            = NULL;
  s->browser_entries = -2;
  s->direction       = DIRECTION_LEFT; /* Compatibility */
  s->max_buttons     = 0;
}

/*
 * Return position in str of char 'c'. -1 if not found
 */
static int _is_there_char(const char *str, int c) {
  char *p;

  if(str)
    if((p = strrchr(str, c)) != NULL) {
      return (p - str);
    }
  
  return -1;
}

/*
 * Return >= 0 if it's begin of section, otherwise -1
 */
static int skin_begin_section(xitk_skin_config_t *skonfig) {

  ABORT_IF_NULL(skonfig);

  return _is_there_char(skonfig->ln, '{');
}

/*
 * Return >= 0 if it's end of section, otherwise -1
 */
static int skin_end_section(xitk_skin_config_t *skonfig) {

  ABORT_IF_NULL(skonfig);

  return _is_there_char(skonfig->ln, '}');
}

static int istriplet(char *c) {
  int dummy1, dummy2, dummy3;

  if((strncasecmp(c, "#", 1) <= 0) && (strlen(c) >= 7)) {

    if(((isalnum(*(c+1))) && (isalnum(*(c+2))) && (isalnum(*(c+3))) 
       && (isalnum(*(c+4))) && (isalnum(*(c+5))) && (isalnum(*(c+6)))
       && ((*(c+7) == '\0') || (*(c+7) == '\n') || (*(c+7) == '\r') || (*(c+7) == ' ')))
       && (sscanf(c, "#%2x%2x%2x", &dummy1, &dummy2, &dummy3) == 3)) {
      return 1;
    }
  }

  return 0;
}
/*
 * Cleanup the EOL ('\n','\r',' ')
 */
static void skin_clean_eol(xitk_skin_config_t *skonfig) {
  char *p;

  ABORT_IF_NULL(skonfig);

  p = skonfig->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r' || (*p == '#' && (istriplet(p) == 0)) || *p == ';' 
	 || (*p == '/' && *(p+1) == '*')) {
	*p = '\0';
	break;
      }

      p++;
    }

    while(p > skonfig->ln) {
      --p;
      
      if(*p == ' ') 
	*p = '\0';
      else
	break;
    }
  }
}

/*
 * Read from file, store in skonfig->buf, char pointer skonfig->ln point
 * to buf. Cleanup the BOL/EOL. It also ignore comments lines.
 */
static void skin_get_next_line(xitk_skin_config_t *skonfig) {

  ABORT_IF_NULL(skonfig);
  ABORT_IF_NULL(skonfig->fd);
  
  do {
    skonfig->ln = fgets(skonfig->buf, 255, skonfig->fd);
    
    while(skonfig->ln && (*skonfig->ln == ' ' || *skonfig->ln == '\t')) ++skonfig->ln;

  } while(skonfig->ln && 
	  (!strncmp(skonfig->ln, "//", 2) ||
	   !strncmp(skonfig->ln, "/*", 2) || /* */
	   !strncmp(skonfig->ln, ";", 1) ||
	   !strncmp(skonfig->ln, "#", 1)));

  skin_clean_eol(skonfig);
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

/*
 * Set char pointer to first char of value. Delimiter of 
 * value is '=' or ':', e.g: "mykey = myvalue".
 */
static void skin_set_pos_to_value(char **p) {
  
  ABORT_IF_NULL(*p);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Parse subsection of skin element (coords/label yet).
 */
static void skin_parse_subsection(xitk_skin_config_t *skonfig) {
  char *p;
  int  brace_offset;

  ABORT_IF_NULL(skonfig);

  if((brace_offset = skin_begin_section(skonfig)) >= 0) {
    *(skonfig->ln + brace_offset) = '\0';
    skin_clean_eol(skonfig);

    if(!strncasecmp(skonfig->ln, "browser", 7)) {

      while(skin_end_section(skonfig) < 0) {
	skin_get_next_line(skonfig);
	p = skonfig->ln;

	if(!strncasecmp(skonfig->ln, "entries", 7)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->browser_entries = strtol(p, &p, 10);
	}

      }
      skin_get_next_line(skonfig);
    }
    else if(!strncasecmp(skonfig->ln, "coords", 6)) {

      while(skin_end_section(skonfig) < 0) {
	skin_get_next_line(skonfig);
	p = skonfig->ln;
	if(!strncasecmp(skonfig->ln, "x", 1)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->x = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "y", 1)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->y = strtol(p, &p, 10);
	}

      }
      skin_get_next_line(skonfig);
    }
    else if(!strncasecmp(skonfig->ln, "slider", 6)) {
      
      while(skin_end_section(skonfig) < 0) {
	skin_get_next_line(skonfig);
	p = skonfig->ln;
	if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->pixmap_pad = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	  sprintf(skonfig->celement->pixmap_pad, "%s/%s", skonfig->path, p);
	  skin_load_to_cache(skonfig, skonfig->celement->pixmap_pad);
	}
	else if(!strncasecmp(skonfig->ln, "radius", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->radius = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "type", 4)) {
	  skin_set_pos_to_value(&p);
	  if(!strncasecmp("horizontal", p, strlen(p))) {
	    skonfig->celement->slider_type = XITK_HSLIDER;
	  }
	  else if(!strncasecmp("vertical", p, strlen(p))) {
	    skonfig->celement->slider_type = XITK_VSLIDER;
	  }
	  else if(!strncasecmp("rotate", p, strlen(p))) {
	    skonfig->celement->slider_type = XITK_RSLIDER;
	  }
	  else
	    skonfig->celement->slider_type = XITK_HSLIDER;
	}
	
      }
      skin_get_next_line(skonfig);
    }
    else if(!strncasecmp(skonfig->ln, "label", 5)) {
      skonfig->celement->print           = 1;
      skonfig->celement->animation_step  = 1;
      skonfig->celement->animation_timer = xitk_get_timer_label_animation();
      skonfig->celement->align           = ALIGN_CENTER;

      while(skin_end_section(skonfig) < 0) {
	skin_get_next_line(skonfig);
	p = skonfig->ln;

	if(!strncasecmp(skonfig->ln, "color_focus", 11)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->color_focus = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "color_click", 11)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->color_click = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "animation", 9)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->animation = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "length", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->length = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->pixmap_font = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	  sprintf(skonfig->celement->pixmap_font, "%s/%s", skonfig->path, p);
	  skin_load_to_cache(skonfig, skonfig->celement->pixmap_font);
	}
	else if(!strncasecmp(skonfig->ln, "static", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->staticity = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "align", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->align = skin_get_align_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "color", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->color = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "print", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->print = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "timer", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->animation_timer = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "font", 4)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->font = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "step", 4)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->animation_step = strtol(p, &p, 10);
	}

      }
      skin_get_next_line(skonfig);
    }

  }
    
}

/*
 * Parse skin element section.
 */
static void skin_parse_section(xitk_skin_config_t *skonfig) {
  char section[256];
  char *p;
  
  ABORT_IF_NULL(skonfig);

  while(skonfig->ln != NULL) {

    p = skonfig->ln;
    
    if(sscanf(skonfig->ln, "skin.%s", &section[0]) == 1) {

      while(skonfig->ln != NULL) {
	
	if(skin_begin_section(skonfig) >= 0) {
	  xitk_skin_element_t *s;
	  
	  s = (xitk_skin_element_t *) xitk_xmalloc(sizeof(xitk_skin_element_t));
	  _nullify_me(s);
	  
	  if(skonfig->celement) {
	    skonfig->celement->next = s;
	    s->prev = skonfig->celement;
	    skonfig->celement = skonfig->last = s;
	  }
	  else {
	    s->prev = s->next = NULL;
	    skonfig->celement = skonfig->first = skonfig->last = s;
	  }

	  s->section = strdup(section);
	  s->visible = s->enable = 1;

	  skin_get_next_line(skonfig);

	__next_subsection:
	  
	  if(skin_begin_section(skonfig) >= 0) {
	    skin_parse_subsection(skonfig);
	    
	    if(skin_end_section(skonfig) >= 0) {
	      return;
	    }
	    else
	     goto  __next_subsection;
	  }
	  else {
	    if(!strncasecmp(skonfig->ln, "max_buttons", 11)) {
	      skin_set_pos_to_value(&p);
	      s->max_buttons = strtol(p, &p, 10);
	    }
	    else if(!strncasecmp(skonfig->ln, "direction", 9)) {
	      skin_set_pos_to_value(&p);
	      s->direction = skin_get_direction(p);
	    }
	    else if(!strncasecmp(skonfig->ln, "visible", 7)) {
	      skin_set_pos_to_value(&p);
	      s->visible = xitk_get_bool_value(p);
	    }
	    else if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	      skin_set_pos_to_value(&p);
	      s->pixmap = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	      sprintf(s->pixmap, "%s/%s", skonfig->path, p);
	      skin_load_to_cache(skonfig, s->pixmap);
	    }
	    else if(!strncasecmp(skonfig->ln, "enable", 6)) {
	      skin_set_pos_to_value(&p);
	      s->enable = xitk_get_bool_value(p);
	    }
	    
	  }

	  skin_get_next_line(skonfig);
	  p = skonfig->ln;

	  if(skin_end_section(skonfig) >= 0) {
	    return;
	  }
	  
	  goto __next_subsection;

	}
	else {
	  skin_set_pos_to_value(&p);

	  if(!strncasecmp(section, "unload_command", 14)) {
	    skonfig->unload_command = _expanded(skonfig, p);
	    return;
	  }
	  else if(!strncasecmp(section, "load_command", 12)) {
	    skonfig->load_command = _expanded(skonfig, p);
	    return;
	  }
	  else if(!strncasecmp(section, "animation", 9)) {
	    skonfig->animation = strdup(p);
	    return;
	  }
	  else if(!strncasecmp(section, "version", 7)) {
	    skonfig->version = strtol(p, &p, 10);
	    return;
	  }
	  else if(!strncasecmp(section, "author", 6)) {
	    skonfig->author = strdup(p);
	    return;
	  }
	  else if(!strncasecmp(section, "name", 4)) {
	    skonfig->name = strdup(p);
	    return;
	  }
	  else if(!strncasecmp(section, "date", 4)) {
	    skonfig->date = strdup(p);
	    return;
	  }
	  else if(!strncasecmp(section, "logo", 4)) {
	    skonfig->logo = _expanded(skonfig, p);
	    return;
	  }
	  else if(!strncasecmp(section, "url", 3)) {
	    skonfig->url = strdup(p);
	    return;
	  }
	  else {
	    XITK_WARNING("wrong section entry found: '%s'\n", section);
	    return;
	  }
	}
	skin_get_next_line(skonfig);
      }
    }
  }
}

#ifdef DEBUG_SKIN
/*
 * Just to check list chained constitency.
 */
static void check_skonfig(xitk_skin_config_t *skonfig) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);
  
  s = skonfig->first;

  if(s) {

    printf("Skin name '%s'\n", skonfig->name);
    printf("     version   '%d'\n", skonfig->version);
    printf("     author    '%s'\n", skonfig->author);
    printf("     date      '%s'\n", skonfig->date);
    printf("     load cmd  '%s'\n", skonfig->load_command);
    printf("     uload cmd '%s'\n", skonfig->unload_command);
    printf("     URL       '%s'\n", skonfig->url);
    printf("     logo      '%s'\n", skonfig->logo);
    printf("     animation '%s'\n", skonfig->animation);

    while(s) {
      printf("Section '%s'\n", s->section);
      printf("  enable      = %d\n", s->enable);
      printf("  visible     = %d\n", s->visible);
      printf("  X           = %d\n", s->x);
      printf("  Y           = %d\n", s->y);
      printf("  direction   = %d\n", s->direction);
      printf("  pixmap      = '%s'\n", s->pixmap);

      if(s->slider_type) {
	printf("  slider type = %d\n", s->slider_type);
	printf("  pad pixmap  = '%s'\n", s->pixmap_pad);
      }

      if(s->browser_entries > -1)
	printf("  browser entries = %d\n", s->browser_entries);
      
      printf("  animation   = %d\n", s->animation);
      printf("  step        = %d\n", s->animation_step);
      printf("  print       = %d\n", s->print);
      printf("  static      = %d\n", s->staticity);
      printf("  length      = %d\n", s->length);
      printf("  color       = '%s'\n", s->color);
      printf("  color focus = '%s'\n", s->color_focus);
      printf("  color click = '%s'\n", s->color_click);
      printf("  pixmap font = '%s'\n", s->pixmap_font);
      printf("  font        = '%s'\n", s->font);
      printf("  max_buttons = %d\n", s->max_buttons);
      s = s->next;
    }

    printf("revert order\n");
    s = skonfig->last;
    while(s) {
      printf("Section '%s'\n", s->section); 
      printf("  enable      = %d\n", s->enable);
      printf("  visible     = %d\n", s->visible);
      printf("  X           = %d\n", s->x);
      printf("  Y           = %d\n", s->y);
      printf("  direction   = %d\n", s->direction);
      printf("  pixmap      = '%s'\n", s->pixmap);
      printf("  animation   = %d\n", s->animation);
      printf("  step        = %d\n", s->animation_step);
      printf("  print       = %d\n", s->print);
      printf("  static      = %d\n", s->staticity);
      printf("  length      = %d\n", s->length);
      printf("  color       = '%s'\n", s->color);
      printf("  color focus = '%s'\n", s->color_focus);
      printf("  color click = '%s'\n", s->color_click);
      printf("  pixmap font = '%s'\n", s->pixmap_font);
      printf("  font        = '%s'\n", s->font);
      printf("  max_buttons = %d\n", s->max_buttons);
      s = s->prev;
    }
  }
}
#endif

static xitk_skin_element_t *skin_lookup_section(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);
  ABORT_IF_NULL(str);

  s = skonfig->first;
  while(s) {

    if(!strcasecmp(str, s->section))
      return s;

    s = s->next;
  }

  return NULL;
}
  
/************************************************************************************
 *                                   END OF PRIVATES
 ************************************************************************************/

/*
 * Alloc a xitk_skin_config_t* memory area, nullify pointers.
 */
xitk_skin_config_t *xitk_skin_init_config(ImlibData *im) {
  xitk_skin_config_t *skonfig;
  
  if((skonfig = (xitk_skin_config_t *) xitk_xmalloc(sizeof(xitk_skin_config_t))) == NULL) {
    XITK_DIE("xitk_xmalloc() failed: %s\n", strerror(errno));
  }
  skonfig->im       = im;
  skonfig->cache    = NULL;
  skonfig->version  = -1;
  skonfig->first    = skonfig->last 
                    = skonfig->celement 
                    = NULL;
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
  xitk_skin_element_t *s, *c;

  ABORT_IF_NULL(skonfig);

  pthread_mutex_lock(&skin_mutex);
  if(skonfig->celement) {
    
    s = skonfig->last;
    
    while(s) {
      XITK_FREE(s->section);
      XITK_FREE(s->pixmap);
      XITK_FREE(s->pixmap_pad);
      XITK_FREE(s->pixmap_font);
      XITK_FREE(s->color);
      XITK_FREE(s->color_focus);
      XITK_FREE(s->color_click);
      XITK_FREE(s->font);
      
      c = s;
      s = c->prev;
      
      XITK_FREE(c);
    }
  }
  
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

  skin_free_cache(skonfig);

  XITK_FREE(skonfig);
  pthread_mutex_unlock(&skin_mutex);
}

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *skonfig, char *path, char *filename) {
  char buf[2048];

  ABORT_IF_NULL(skonfig);
  ABORT_IF_NULL(path);
  ABORT_IF_NULL(filename);

  pthread_mutex_lock(&skin_mutex);

  skonfig->path     = strdup(path);
  skonfig->skinfile = strdup(filename);

  snprintf(buf, sizeof(buf), "%s/%s", skonfig->path, skonfig->skinfile);

  if((skonfig->fd = fopen(buf, "r")) != NULL) {
    
    skin_get_next_line(skonfig);

    while(skonfig->ln != NULL) {

      if(!strncasecmp(skonfig->ln, "skin.", 5)) {
	skin_parse_section(skonfig);
      }
      
      skin_get_next_line(skonfig);
    }
    fclose(skonfig->fd);
  }
  else {
    XITK_WARNING("%s(): Unable to open '%s' file.\n", __FUNCTION__, skonfig->skinfile);
    XITK_FREE(skonfig->skinfile);
    pthread_mutex_unlock(&skin_mutex);
    return 0;
  }

  if(!skonfig->first) {
    XITK_WARNING("%s(): no valid skin element found in '%s/%s'.\n",
		 __FUNCTION__, skonfig->path, skonfig->skinfile);
    pthread_mutex_unlock(&skin_mutex);
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

  pthread_mutex_unlock(&skin_mutex);

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
int xitk_skin_get_direction(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->direction;

  return DIRECTION_LEFT; /* Compatibility */
}

/*
 *
 */
int xitk_skin_get_visibility(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->visible;

  return 1;
}

/*
 *
 */
int xitk_skin_get_printability(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->print;

  return 1;
}


/*
 *
 */
int xitk_skin_get_enability(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->enable;

  return 1;
}

/*
 *
 */
int xitk_skin_get_coord_x(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->x;

  return 0;
}

/*
 *
 */
int xitk_skin_get_coord_y(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->y;

  return 0;
}

/*
 *
 */
char *xitk_skin_get_label_color(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_label_color_focus(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color_focus;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_label_color_click(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color_click;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_length(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->length;

  return 0;
}

/*
 *
 */
int xitk_skin_get_label_animation(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->animation;

  return 0;
}

/*
 *
 */
int xitk_skin_get_label_animation_step(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->animation_step;

  return 1;
}

unsigned long xitk_skin_get_label_animation_timer(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->animation_timer;
  
  return 0;
}

/*
 *
 */
char *xitk_skin_get_label_fontname(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->font;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_printable(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->print;
  
  return 1;
}

/*
 *
 */
int xitk_skin_get_label_staticity(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->staticity;
  
  return 0;
}

/*
 *
 */
int xitk_skin_get_label_alignment(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->align;
  
  return ALIGN_CENTER;
}

/*
 *
 */
char *xitk_skin_get_label_skinfont_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->pixmap_font;
  
  return NULL;
}

/*
 *
 */
char *xitk_skin_get_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->pixmap;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    if(s->slider_type)
      return s->pixmap_pad;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_slider_type(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  ABORT_IF_NULL(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return((s->slider_type) ? s->slider_type : XITK_HSLIDER);

  return 0;
}

/*
 *
 */
int xitk_skin_get_slider_radius(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->radius;

  return 0;
}

/*
 *
 */
char *xitk_skin_get_animation(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);
  
  return skonfig->animation;
}

/*
 *
 */
char *xitk_skin_get_logo(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);
  
  return skonfig->logo;
}

/*
 *
 */
int xitk_skin_get_browser_entries(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->browser_entries;
  
  return -1;
}

/*
 *
 */
xitk_image_t *xitk_skin_get_image(xitk_skin_config_t *skonfig, const char *str) {
  ABORT_IF_NULL(skonfig);

  if(skonfig->cache) {
    cache_entry_t *cache = skonfig->cache;
    
    while(cache) {
      if(!strcmp(str, cache->filename))
	return cache->image;
      
      cache = cache->next;
    }
  }

  return (skin_load_to_cache(skonfig, (char *) str));
  //  return (xitk_image_load_image(skonfig->im, (char *) str));
}

int xitk_skin_get_max_buttons(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  ABORT_IF_NULL(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->max_buttons;
  
  return 0;
}

/*
 *
 */
void xitk_skin_lock(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);
  pthread_mutex_lock(&skin_mutex);
}

/*
 *
 */
void xitk_skin_unlock(xitk_skin_config_t *skonfig) {
  ABORT_IF_NULL(skonfig);
  pthread_mutex_unlock(&skin_mutex);
}
