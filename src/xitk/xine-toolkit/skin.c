/* 
 * Copyright (C) 2000-2001 the xine project
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
#include <assert.h>
#include <errno.h>

#include "skin.h"
#include "_xitk.h"

extern int errno;

#undef DEBUG_SKIN

/************************************************************************************
 *                                     PRIVATES
 ************************************************************************************/

/*
 *
 */
static char *_get_expanded_command(xitk_skin_config_t *skonfig, char *cmd) {
  char *p;
  char *ret = NULL;
  char  buf[BUFSIZ], buf2[BUFSIZ], var[BUFSIZ];
  
  assert(skonfig != NULL);

  if(cmd) {

    if(strchr(cmd, '$')) {

      memset(&buf, 0, sizeof(buf));
      snprintf(buf, (BUFSIZ - 1), "%s", cmd);

      memset(&buf2, 0, sizeof(buf2));
      
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
		char ppath[XITK_PATH_MAX + XITK_NAME_MAX + 1];
		char *z;
		
		sprintf(ppath, "%s", skonfig->path);
		if((z = strrchr(ppath, '/')) != NULL) {
		  *z = '\0';
		  sprintf(buf2, "%s%s", buf2, ppath);
		}
	      }
	    }
	    else if(!strncmp("SKIN_VERSION", var, strlen(var))) {
	      if(skonfig->version >= 0)
		sprintf(buf2, "%s%d", buf2, skonfig->version);
	    }
	    else if(!strncmp("SKIN_AUTHOR", var, strlen(var))) {
	      if(skonfig->author)
		sprintf(buf2, "%s%s", buf2, skonfig->author);
	    }
	    else if(!strncmp("SKIN_PATH", var, strlen(var))) {
	      if(skonfig->path)
		sprintf(buf2, "%s%s", buf2, skonfig->path);
	    }
	    else if(!strncmp("SKIN_NAME", var, strlen(var))) {
	      if(skonfig->name)
		sprintf(buf2, "%s%s", buf2, skonfig->name);
	    }
	    else if(!strncmp("SKIN_DATE", var, strlen(var))) {
	      if(skonfig->date)
		sprintf(buf2, "%s%s", buf2, skonfig->date);
	    }
	    else if(!strncmp("SKIN_URL", var, strlen(var))) {
	      if(skonfig->url)
		sprintf(buf2, "%s%s", buf2, skonfig->url);
	    }
	    else if(!strncmp("HOME", var, strlen(var))) {
	      if(skonfig->url)
		sprintf(buf2, "%s%s", buf2, xitk_get_homedir());
	    }
	    /* else ignore */
	  }
	  break;
	  
	default:
	  sprintf(buf2, "%s%c", buf2, *p);
	  break;
	}
	p++;
      }

      ret = strdup(buf2);
    }
  }

  return ret;
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

  assert(skonfig != NULL);

  return _is_there_char(skonfig->ln, '{');
}

/*
 * Return >= 0 if it's end of section, otherwise -1
 */
static int skin_end_section(xitk_skin_config_t *skonfig) {

  assert(skonfig != NULL);

  return _is_there_char(skonfig->ln, '}');
}

/*
 * Cleanup the EOL ('\n','\r',' ')
 */
static void skin_clean_eol(xitk_skin_config_t *skonfig) {
  char *p;

  assert(skonfig != NULL);

  p = skonfig->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
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

  assert(skonfig != NULL && skonfig->fd != NULL);
  
 __get_next_line:

  skonfig->ln = fgets(skonfig->buf, 255, skonfig->fd);

  while(skonfig->ln && (*skonfig->ln == ' ' || *skonfig->ln == '\t')) ++skonfig->ln;

  if(skonfig->ln) {
    if((strncmp(skonfig->ln, "//", 2) == 0) ||
       (strncmp(skonfig->ln, "/*", 2) == 0) || /**/
       (strncmp(skonfig->ln, ";", 1) == 0) ||
       (strncmp(skonfig->ln, "#", 1) == 0)) {
      goto __get_next_line;
    }
  }

  skin_clean_eol(skonfig);
}

/*
 * Return 0/1 from char value (valids are 1/0, true/false, 
 * yes/no, on/off. Case isn't checked.
 */
static int skin_get_bool_value(const char *val) {
  static struct {
    const char *str;
    int value;
  } bools[] = {
    { "1",     1 }, { "true",  1 }, { "yes",   1 }, { "on",    1 },
    { "0",     0 }, { "false", 0 }, { "no",    0 }, { "off",   0 },
    { NULL,    0 }
  };
  int i;
  
  assert(val != NULL);

  for(i = 0; bools[i].str != NULL; i++) {
    if(!(strcasecmp(bools[i].str, val)))
      return bools[i].value;
  }

  return 0;
}

/*
 * Return alignement value.
 */
static int skin_get_align_value(const char *val) {
  static struct {
    const char *str;
    int value;
  } aligns[] = {
    { "left",   LABEL_ALIGN_LEFT   }, 
    { "center", LABEL_ALIGN_CENTER }, 
    { "right",  LABEL_ALIGN_RIGHT  },
    { NULL,     0 }
  };
  int i;
  
  assert(val != NULL);
  
  for(i = 0; aligns[i].str != NULL; i++) {
    if(!(strcasecmp(aligns[i].str, val)))
      return aligns[i].value;
  }

  return LABEL_ALIGN_CENTER;
}

/*
 * Set char pointer to first char of value. Delimiter of 
 * value is '=' or ':', e.g: "mykey = myvalue".
 */
static void skin_set_pos_to_value(char **p) {
  
  assert(*p != NULL);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Parse subsection of skin element (coords/label yet).
 */
static void skin_parse_subsection(xitk_skin_config_t *skonfig) {
  char *p;
  int  brace_offset;

  assert(skonfig != NULL);

  if((brace_offset = skin_begin_section(skonfig)) >= 0) {
    *(skonfig->ln + brace_offset) = '\0';
    skin_clean_eol(skonfig);

    if(!strncasecmp(skonfig->ln, "coords", 6)) {

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
	}
	else if(!strncasecmp(skonfig->ln, "type", 1)) {
	  skin_set_pos_to_value(&p);
	  if(!strncasecmp("horizontal", p, strlen(p))) {
	    skonfig->celement->slider_type = XITK_HSLIDER;
	  }
	  else if(!strncasecmp("vertical", p, strlen(p))) {
	    skonfig->celement->slider_type = XITK_VSLIDER;
	  }
	  else
	    skonfig->celement->slider_type = XITK_HSLIDER;
	}
	
      }
      skin_get_next_line(skonfig);
    }
    else if(!strncasecmp(skonfig->ln, "label", 5)) {
      skonfig->celement->print = 1;
      
      skonfig->celement->align = LABEL_ALIGN_CENTER;

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
	  skonfig->celement->animation = skin_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "length", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->length = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->pixmap_font = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	  sprintf(skonfig->celement->pixmap_font, "%s/%s", skonfig->path, p);
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
	  skonfig->celement->print = skin_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "font", 4)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->font = strdup(p);
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
  
  assert(skonfig != NULL);

  while(skonfig->ln != NULL) {

    p = skonfig->ln;
    
    if(sscanf(skonfig->ln, "skin.%s", &section[0]) == 1) {

      while(skonfig->ln != NULL) {
	
	if(skin_begin_section(skonfig) >= 0) {
	  xitk_skin_element_t *s;
	  
	  s = (xitk_skin_element_t *) xitk_xmalloc(sizeof(xitk_skin_element_t));

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
	  else
	    if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	      skin_set_pos_to_value(&p);
	      s->pixmap = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	      sprintf(s->pixmap, "%s/%s", skonfig->path, p);
	    }
	  
	  skin_get_next_line(skonfig);
	  
	  if(skin_end_section(skonfig) >= 0) {
	    return;
	  }
	  
	  goto __next_subsection;

	}
	else {
	  skin_set_pos_to_value(&p);

	  if(!strncasecmp(section, "unload_command", 14)) {
	    skonfig->unload_command = _get_expanded_command(skonfig, p);
	    return;
	  }
	  else if(!strncasecmp(section, "load_command", 12)) {
	    skonfig->load_command = _get_expanded_command(skonfig, p);
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
	  else if(!strncasecmp(section, "url", 3)) {
	    skonfig->url = strdup(p);
	    return;
	  }
	  else {
	    XITK_WARNING("%s(): wrong section entry found: '%s'\n", __FUNCTION__, section);
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

  assert(skonfig != NULL);
  
  s = skonfig->first;

  if(s) {

    printf("Skin name '%s'\n", skonfig->name);
    printf("     version   '%d'\n", skonfig->version);
    printf("     author    '%s'\n", skonfig->author);
    printf("     date      '%s'\n", skonfig->date);
    printf("     load cmd  '%s'\n", skonfig->load_command);
    printf("     uload cmd '%s'\n", skonfig->unload_command);
    printf("     URL       '%s'\n", skonfig->url);

    while(s) {
      printf("Section '%s'\n", s->section);
      printf("  X           = %d\n", s->x);
      printf("  Y           = %d\n", s->y);
      printf("  pixmap      = '%s'\n", s->pixmap);

      if(s->slider_type) {
	printf("  slider type = %d\n", s->slider_type);
	printf("  pad pixmap  = '%s'\n", s->pixmap_pad);
      }

      printf("  animation   = %d\n", s->animation);
      printf("  print       = %d\n", s->print);
      printf("  length      = %d\n", s->length);
      printf("  color       = '%s'\n", s->color);
      printf("  color focus = '%s'\n", s->color_focus);
      printf("  color click = '%s'\n", s->color_click);
      printf("  pixmap font = '%s'\n", s->pixmap_font);
      printf("  font        = '%s'\n", s->font);
      s = s->next;
    }

    printf("revert order\n");
    s = skonfig->last;
    while(s) {
      printf("Section '%s'\n", s->section);
      printf("  X           = %d\n", s->x);
      printf("  Y           = %d\n", s->y);
      printf("  pixmap      = '%s'\n", s->pixmap);
      printf("  animation   = %d\n", s->animation);
      printf("  print       = %d\n", s->print);
      printf("  length      = %d\n", s->length);
      printf("  color       = '%s'\n", s->color);
      printf("  color focus = '%s'\n", s->color_focus);
      printf("  color click = '%s'\n", s->color_click);
      printf("  pixmap font = '%s'\n", s->pixmap_font);
      printf("  font        = '%s'\n", s->font);
      s = s->prev;
    }
  }
}
#endif

static xitk_skin_element_t *skin_lookup_section(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig != NULL && str != NULL);

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
xitk_skin_config_t *xitk_skin_init_config(void) {
  xitk_skin_config_t *skonfig;
  
  if((skonfig = (xitk_skin_config_t *) xitk_xmalloc(sizeof(xitk_skin_config_t))) == NULL) {
    XITK_DIE("%s(): xitk_xmalloc() failed: %s\n", __FUNCTION__, strerror(errno));
  }
  skonfig->version = -1;
  skonfig->first = skonfig->last = skonfig->celement = NULL;
  skonfig->name = skonfig->author = skonfig->date = skonfig->url = 
    skonfig->load_command = skonfig->unload_command = NULL;
  skonfig->skinfile = skonfig->path = NULL;

  skonfig->ln = skonfig->buf;
  
  return skonfig;
}

/*
 * Release all allocated memory of a xitk_skin_config_t* variable (element chained struct too).
 */
void xitk_skin_free_config(xitk_skin_config_t *skonfig) {
  xitk_skin_element_t *s, *c;

  assert(skonfig != NULL);

  if(skonfig->celement) {
    s = skonfig->last;
    
    if(s) {
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
	s = s->prev;
	
	XITK_FREE(c);
      }
    }
  }
  
  XITK_FREE(skonfig->name);
  XITK_FREE(skonfig->author);
  XITK_FREE(skonfig->date);
  XITK_FREE(skonfig->url);
  XITK_FREE(skonfig->path);
  XITK_FREE(skonfig->load_command);
  XITK_FREE(skonfig->unload_command);
  XITK_FREE(skonfig->skinfile);
  XITK_FREE(skonfig);
}

/*
 * Load the skin configfile.
 */
int xitk_skin_load_config(xitk_skin_config_t *skonfig, char *path, char *filename) {
  char buf[2048];

  assert(skonfig != NULL && path != NULL && filename != NULL);

  skonfig->path     = strdup(path);
  skonfig->skinfile = strdup(filename);

  snprintf(buf, 2048, "%s/%s", skonfig->path, skonfig->skinfile);

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
    return 0;
  }

  if(!skonfig->first) {
    XITK_WARNING("%s(): no valid skin element found in '%s/%s'.\n",
		 __FUNCTION__, skonfig->path, skonfig->skinfile);
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

  assert(skonfig);

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
int xitk_skin_get_coord_x(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->x;

  return 0;
}

/*
 *
 */
int xitk_skin_get_coord_y(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->y;

  return 0;
}

/*
 *
 */
char *xitk_skin_get_label_color(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_label_color_focus(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color_focus;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_label_color_click(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->color_click;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_length(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->length;

  return 0;
}

/*
 *
 */
int xitk_skin_get_label_animation(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->animation;

  return 0;
}

/*
 *
 */
char *xitk_skin_get_label_fontname(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->font;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_printable(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  assert(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->print;
  
  return 1;
}

/*
 *
 */
int xitk_skin_get_label_alignment(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  assert(skonfig);
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->align;
  
  return LABEL_ALIGN_CENTER;
}

/*
 *
 */
char *xitk_skin_get_label_skinfont_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->pixmap_font;
  
  return NULL;
}

/*
 *
 */
char *xitk_skin_get_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->pixmap;

  return NULL;
}

/*
 *
 */
char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  assert(skonfig);

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

  assert(skonfig);

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return((s->slider_type) ? s->slider_type : XITK_HSLIDER);

  return 0;
}
