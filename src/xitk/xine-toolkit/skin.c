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

#undef DEBUG_SKIN

/************************************************************************************
 *                                     PRIVATES
 ************************************************************************************/

typedef struct {
  const char *name;
  xitk_image_t *image;
} xitk_skin_img_t;

static int xitk_simg_cmp (void *a, void *b) {
  xitk_skin_img_t *d = a, *e = b;
  return strcmp (d->name, e->name);
}

/*
 *
 */
static xitk_image_t *skin_load_img (xitk_skin_config_t *skonfig, xitk_rect_t *rect, const char *pixmap, const char *format) {
  char b[1024];
  const char *name, *part;
  xitk_image_t *image;

  if (!skonfig || !pixmap)
    return NULL;

  if (rect)
    rect->x = rect->y = rect->width = rect->height = 0;

  part = strchr (pixmap, '|');
  if (part && rect) {
    uint32_t v;
    uint8_t z;
    const uint8_t *p = (const uint8_t *)part + 1;

    if (part > pixmap + sizeof (b) - 1)
      part = pixmap + sizeof (b) - 1;
    memcpy (b, pixmap, part - pixmap);
    b[part - pixmap] = 0;
    name = b;

    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    rect->x = v;
    if (*p == ',')
      p++;
    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    rect->y = v;
    if (*p == ',')
      p++;
    v = 0;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    rect->width = v;
    v = 0;
    if (*p == ',')
      p++;
    while ((z = *p ^ '0') < 10)
      v = v * 10u + z, p++;
    rect->height = v;
  } else {
    name = pixmap;
  }

  image = NULL;
  {
    xitk_skin_img_t here;
    int pos;
    here.name = name;
    pos = xine_sarray_binary_search (skonfig->imgs, &here);
    if (pos >= 0) {
      xitk_skin_img_t *si = xine_sarray_get (skonfig->imgs, pos);
      if (format && !si->image->pix_font)
        xitk_image_set_pix_font (si->image, format);
      image = si->image;
    }
  }

  if (!image) {
    xitk_skin_img_t *nimg;
    size_t nlen = strlen (name) + 1;
    char *nmem = malloc (sizeof (*nimg) + nlen);
    if (!nmem)
      return NULL;
    nimg = (xitk_skin_img_t *)nmem;
    nmem += sizeof (*nimg);
    memcpy (nmem, name, nlen);
    nimg->name = nmem;
    nimg->image = xitk_image_load_image (skonfig->xitk, nmem);
    if (!nimg->image) {
      free (nimg);
      return NULL;
    }
    if (format)
      xitk_image_set_pix_font (nimg->image, format);
    xine_sarray_add (skonfig->imgs, nimg);
    image = nimg->image;
  }

  if (rect) {
    if (rect->x < 0)
      rect->x = 0;
    if (rect->y < 0)
      rect->x = 0;
    if (rect->width <= 0)
      rect->width = image->width;
    if (rect->height <= 0)
      rect->height = image->height;
  }
  return image;
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

#if 0
static int _key_from_name (const char *name) {
  static const char * const names[] = {
    "\x13""align",
    "\x0f""animation",
    "\x00""browser",
    "\x14""color",
    "\x0e""color_click",
    "\x0d""color_focus",
    "\x02""coords",
    "\x01""entries",
    "\x17""font",
    "\x09""horizontal",
    "\x0c""label",
    "\x10""length",
    "\x06""pixmap",
    "\x11""pixmap_format",
    "\x15""print",
    "\x07""radius",
    "\x0b""rotate",
    "\x05""slider",
    "\x12""static",
    "\x18""step",
    "\x16""timer",
    "\x08""type",
    "\x0a""vertical",
    "\x03""x",
    "\x04""y",
  };
  int b = 0, e = sizeof (names) / sizeof (names[0]), m = e >> 1;
  do {
    int d = strcasecmp (name, names[m] + 1);
    if (d == 0)
      return names[m][0];
    if (d < 0)
      e = m;
    else
      b = m + 1;
    m = (b + e) >> 1;
  } while (b != e);
  return -1;
}
#endif

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
  s->info.pixmap_img             = NULL;
  s->info.slider_pixmap_pad_name = NULL;
  s->info.slider_pixmap_pad_img  = NULL;
  s->info.label_pixmap_font_name = NULL;
  s->info.label_pixmap_font_img  = NULL;
  s->info.label_color            = NULL;
  s->info.label_color_focus      = NULL;
  s->info.label_color_click      = NULL;
  s->info.label_fontname         = NULL;
  s->info.browser_entries        = -2;
  s->info.direction              = DIRECTION_LEFT; /* Compatibility */
  s->info.max_buttons            = 0;
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
  unsigned int dummy1, dummy2, dummy3;

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
	  skonfig->celement->info.browser_entries = strtol(p, &p, 10);
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
	  skonfig->celement->info.x = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "y", 1)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.y = strtol(p, &p, 10);
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
	  skonfig->celement->info.slider_pixmap_pad_name = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	  sprintf (skonfig->celement->info.slider_pixmap_pad_name, "%s/%s", skonfig->path, p);
          skonfig->celement->info.slider_pixmap_pad_img = skin_load_img (skonfig,
            &skonfig->celement->info.slider_pixmap_pad_rect, skonfig->celement->info.slider_pixmap_pad_name, NULL);
	}
	else if(!strncasecmp(skonfig->ln, "radius", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.slider_radius = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "type", 4)) {
	  skin_set_pos_to_value(&p);
	  if(!strncasecmp("horizontal", p, strlen(p))) {
	    skonfig->celement->info.slider_type = XITK_HSLIDER;
	  }
	  else if(!strncasecmp("vertical", p, strlen(p))) {
	    skonfig->celement->info.slider_type = XITK_VSLIDER;
	  }
	  else if(!strncasecmp("rotate", p, strlen(p))) {
	    skonfig->celement->info.slider_type = XITK_RSLIDER;
	  }
	  else
	    skonfig->celement->info.slider_type = XITK_HSLIDER;
	}
	
      }
      skin_get_next_line(skonfig);
    }
    else if(!strncasecmp(skonfig->ln, "label", 5)) {
      skonfig->celement->info.label_printable = 1;
      skonfig->celement->info.label_animation_step = 1;
      skonfig->celement->info.label_animation_timer = xitk_get_timer_label_animation();
      skonfig->celement->info.label_alignment = ALIGN_CENTER;

      while(skin_end_section(skonfig) < 0) {
	skin_get_next_line(skonfig);
	p = skonfig->ln;

	if(!strncasecmp(skonfig->ln, "color_focus", 11)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_color_focus = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "color_click", 11)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_color_click = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "animation", 9)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_animation = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "length", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_length = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "pixmap_format", 13)) {
	  skin_set_pos_to_value(&p);
          if (!skonfig->celement->info.label_pixmap_font_format) {
            skonfig->celement->info.label_pixmap_font_format = strdup (p);
            if (skonfig->celement->info.label_pixmap_font_name)
              skonfig->celement->info.label_pixmap_font_img = skin_load_img (skonfig, NULL,
                skonfig->celement->info.label_pixmap_font_name,
                skonfig->celement->info.label_pixmap_font_format);
          }
        }
	else if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_pixmap_font_name = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
          if (skonfig->celement->info.label_pixmap_font_name) {
            sprintf (skonfig->celement->info.label_pixmap_font_name, "%s/%s", skonfig->path, p);
            skonfig->celement->info.label_pixmap_font_img = skin_load_img (skonfig, NULL,
              skonfig->celement->info.label_pixmap_font_name,
              skonfig->celement->info.label_pixmap_font_format);
          }
        }
	else if(!strncasecmp(skonfig->ln, "static", 6)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_staticity = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "align", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_alignment = skin_get_align_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "color", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_color = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "print", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_printable = xitk_get_bool_value(p);
	}
	else if(!strncasecmp(skonfig->ln, "timer", 5)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_animation_timer = strtol(p, &p, 10);
	}
	else if(!strncasecmp(skonfig->ln, "font", 4)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_fontname = strdup(p);
	}
	else if(!strncasecmp(skonfig->ln, "step", 4)) {
	  skin_set_pos_to_value(&p);
	  skonfig->celement->info.label_animation_step = strtol(p, &p, 10);
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
	  
          skonfig->celement = s;

          strlcpy (s->section, section, sizeof (s->section));
	  s->info.visibility = s->info.enability = 1;
          xine_sarray_add (skonfig->elements, s);

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
	      s->info.max_buttons = strtol(p, &p, 10);
	    }
	    else if(!strncasecmp(skonfig->ln, "direction", 9)) {
	      skin_set_pos_to_value(&p);
	      s->info.direction = skin_get_direction(p);
	    }
	    else if(!strncasecmp(skonfig->ln, "visible", 7)) {
	      skin_set_pos_to_value(&p);
	      s->info.visibility = xitk_get_bool_value(p);
	    }
	    else if(!strncasecmp(skonfig->ln, "pixmap", 6)) {
	      skin_set_pos_to_value(&p);
	      s->info.pixmap_name = (char *) xitk_xmalloc(strlen(skonfig->path) + strlen(p) + 2);
	      sprintf (s->info.pixmap_name, "%s/%s", skonfig->path, p);
              s->info.pixmap_img = skin_load_img (skonfig,
                &s->info.pixmap_rect, s->info.pixmap_name, NULL);
	    }
	    else if(!strncasecmp(skonfig->ln, "enable", 6)) {
	      skin_set_pos_to_value(&p);
	      s->info.enability = xitk_get_bool_value(p);
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
int xitk_skin_get_direction(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.direction;

  return DIRECTION_LEFT; /* Compatibility */
}

/*
 *
 */
int xitk_skin_get_visibility(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.visibility;

  return 1;
}

/*
 *
 */
int xitk_skin_get_printability(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_printable;

  return 1;
}


/*
 *
 */
int xitk_skin_get_enability(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.enability;

  return 1;
}

/*
 *
 */
int xitk_skin_get_coord_x(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.x;

  return 0;
}

/*
 *
 */
int xitk_skin_get_coord_y(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.y;

  return 0;
}

/*
 *
 */
const char *xitk_skin_get_label_color(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_color;

  return NULL;
}

/*
 *
 */
const char *xitk_skin_get_label_color_focus(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_color_focus;

  return NULL;
}

/*
 *
 */
const char *xitk_skin_get_label_color_click(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_color_click;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_length(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_length;

  return 0;
}

/*
 *
 */
int xitk_skin_get_label_animation(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_animation;

  return 0;
}

/*
 *
 */
int xitk_skin_get_label_animation_step(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_animation_step;

  return 1;
}

unsigned long xitk_skin_get_label_animation_timer(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_animation_timer;
  
  return 0;
}

/*
 *
 */
const char *xitk_skin_get_label_fontname(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_fontname;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_label_printable(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_printable;
  
  return 1;
}

/*
 *
 */
int xitk_skin_get_label_staticity(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_staticity;
  
  return 0;
}

/*
 *
 */
int xitk_skin_get_label_alignment(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_alignment;
  
  return ALIGN_CENTER;
}

/*
 *
 */
const char *xitk_skin_get_label_skinfont_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.label_pixmap_font_name;
  
  return NULL;
}

/*
 *
 */
const char *xitk_skin_get_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.pixmap_name;

  return NULL;
}

/*
 *
 */
const char *xitk_skin_get_slider_skin_filename(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    if (s->info.slider_type)
      return s->info.slider_pixmap_pad_name;
  
  return NULL;
}

/*
 *
 */
int xitk_skin_get_slider_type(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;

  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return ((s->info.slider_type) ? s->info.slider_type : XITK_HSLIDER);

  return 0;
}

/*
 *
 */
int xitk_skin_get_slider_radius(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.slider_radius;

  return 0;
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

/*
 *
 */
int xitk_skin_get_browser_entries(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.browser_entries;
  
  return -1;
}

/*
 *
 */
xitk_image_t *xitk_skin_get_image(xitk_skin_config_t *skonfig, const char *str) {
  ABORT_IF_NULL(skonfig);
  return skin_load_img (skonfig, NULL, str, NULL);
}

xitk_image_t *xitk_skin_get_part_image (xitk_skin_config_t *skonfig, xitk_rect_t *rect, const char *str) {
  ABORT_IF_NULL(skonfig);
  return skin_load_img (skonfig, rect, str, NULL);
}

int xitk_skin_get_max_buttons(xitk_skin_config_t *skonfig, const char *str) {
  xitk_skin_element_t *s;
  
  if((s = skin_lookup_section(skonfig, str)) != NULL)
    return s->info.max_buttons;
  
  return 0;
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
