/* 
 * Copyright (C) 2000-2002 the xine project
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
#include <assert.h>
#include <string.h>

#include "_config.h"
#include "_xitk.h"

/*
 * Extract color values
 */
static void xitk_config_colors(xitk_config_t *xtcf) {
  char  *p = NULL;
  char  *c = NULL;
  int    pixel;

  assert(xtcf != NULL && xtcf->ln != NULL);

  p = xtcf->ln + 6;
  if(p)
    c = strchr(p, '=');

  if(c) {

    *(c++) = '\0';

    while(*c == ' ' || *c == '\t') c++;

    if((strchr(c, '#')) || (isalpha(*c))) {
      xitk_color_names_t *color; 

      if((color = xitk_get_color_name(c)) != NULL) {
	/* 
	 * We can't use xitk_get_pixel_from_rgb() here, 
	 * 'cause we didn't have any ImlibData object.
	 */
	pixel = ((color->red & 0xf8) << 8) | 
	  ((color->green & 0xf8) << 3) | 
	  ((color->blue & 0xf8) >> 3);
	
	xitk_free_color_name(color);
	
      }
      else {
	XITK_WARNING("%s@%d: wrong color name: '%s'\n", __FUNCTION__, __LINE__, c);
	pixel = 0;
      }
    }
    else
      sscanf(c, "%d", &pixel);
    
    if(!strncasecmp(p, "warning_foreground", 15))
      xtcf->colors.warn_foreground = pixel;
    else if(!strncasecmp(p, "warning_background", 15))
      xtcf->colors.warn_background = pixel;
    else if(!strncasecmp(p, "background", 10))
      xtcf->colors.background = pixel;
    else if(!strncasecmp(p, "select", 6))
      xtcf->colors.select = pixel;
    else if(!strncasecmp(p, "focus", 5))
      xtcf->colors.focus = pixel;
    else if(!strncasecmp(p, "black", 5))
      xtcf->colors.black = pixel;
    else if(!strncasecmp(p, "white", 5))
      xtcf->colors.white = pixel;

  }
}

/*
 * Extract font names.
 */
static void xitk_config_fonts(xitk_config_t *xtcf) {
  char  *p = NULL;
  char  *c = NULL;

  assert(xtcf != NULL && xtcf->ln != NULL);
  
  p = xtcf->ln + 5;
  if(p)
    c = strchr(p, '=');
  
  if(c) {
    
    *(c++) = '\0';
    
    while(*c == ' ' || *c == '\t') c++;
    
    if(!strncasecmp(p, "default", 7))
      xtcf->fonts.fallback = strdup(c);
    else if(!strncasecmp(p, "system", 6)) {
      xtcf->fonts.system = (char *) realloc(xtcf->fonts.system, (strlen(c) + 1));
      xtcf->fonts.system = strdup(c);
    }

  }
}

/*
 * Extract timer.
 */
static void xitk_config_timers(xitk_config_t *xtcf) {
  char  *p = NULL;
  char  *c = NULL;

  assert(xtcf != NULL && xtcf->ln != NULL);

  p = xtcf->ln + 6;
  if(p)
    c = strchr(p, '=');
  
  if(c) {
    
    *(c++) = '\0';
    
    while(*c == ' ' || *c == '\t') c++;
    
    if(!strncasecmp(p, "label_animation", 15))
      xtcf->timers.label_anim = strtol(c, &c, 10);
  }
}

/*
 * Guess entries.
 */
static void xitk_config_store_entry(xitk_config_t *xtcf) {

  assert(xtcf != NULL);

  if(!strncasecmp(xtcf->ln, "color.", 6))
    xitk_config_colors(xtcf);
  else if(!strncasecmp(xtcf->ln, "timer.", 6))
    xitk_config_timers(xtcf);
  else if(!strncasecmp(xtcf->ln, "font.", 5))
    xitk_config_fonts(xtcf);
    
}

/*
 * Cleanup the EOL ('\n','\r',' ')
 */
static void xitk_config_clean_eol(xitk_config_t *xtcf) {
  char *p;

  assert(xtcf != NULL);

  p = xtcf->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      p++;
    }

    while(p > xtcf->ln) {
      --p;
      
      if(*p == ' ') 
	*p = '\0';
      else
	break;
    }
  }
}

/*
 * Read next line from file.
 */
static void xitk_config_get_next_line(xitk_config_t *xtcf) {

  assert(xtcf != NULL && xtcf->fd != NULL);
  
 __get_next_line:

  xtcf->ln = fgets(xtcf->buf, 255, xtcf->fd);
  
  while(xtcf->ln && (*xtcf->ln == ' ' || *xtcf->ln == '\t')) ++xtcf->ln;
  
  if(xtcf->ln) {
    if((strncmp(xtcf->ln, "//", 2) == 0) ||
       (strncmp(xtcf->ln, "/*", 2) == 0) || /**/
       (strncmp(xtcf->ln, ";", 1) == 0) ||
       (strncmp(xtcf->ln, "#", 1) == 0)) {
      goto __get_next_line;
    }
  }

  xitk_config_clean_eol(xtcf);
}

/*
 * load config file.
 */
static void xitk_config_load_configfile(xitk_config_t *xtcf) {
  assert(xtcf);
  
  xitk_config_get_next_line(xtcf);
  
  while(xtcf->ln != NULL) {

    xitk_config_store_entry(xtcf);
    
    xitk_config_get_next_line(xtcf);
  }

  fclose(xtcf->fd);

}

/*
 * Initialiaze values to default.
 */
static void xitk_config_init_default_values(xitk_config_t *xtcf) {
  assert(xtcf != NULL);

  xtcf->fonts.system           = strdup("fixed");
  xtcf->fonts.fallback         = NULL;
  xtcf->colors.black           = -1;
  xtcf->colors.white           = -1;
  xtcf->colors.background      = -1;
  xtcf->colors.focus           = -1;
  xtcf->colors.select          = -1;
  xtcf->colors.warn_foreground = -1;
  xtcf->colors.warn_background = -1;
  xtcf->timers.label_anim      = 50000;
}

/*
 * Get stored values.
 */
char *xitk_config_get_system_font(xitk_config_t *xtcf) {

  if(!xtcf)
    return NULL;
  
  return xtcf->fonts.system;
}
char *xitk_config_get_default_font(xitk_config_t *xtcf) {

  if(!xtcf)
    return NULL;
  
  return xtcf->fonts.fallback;
}
int xitk_config_get_black_color(xitk_config_t *xtcf) {

  if(!xtcf)
    return -1;
  
  return xtcf->colors.black;
}
int xitk_config_get_white_color(xitk_config_t *xtcf) {

  if(!xtcf)
    return -1;
  
  return xtcf->colors.white;
}
int xitk_config_get_background_color(xitk_config_t *xtcf) {

  if(!xtcf)
    return -1;
  
  return xtcf->colors.background;
}
int xitk_config_get_focus_color(xitk_config_t *xtcf) {

  if(!xtcf)
    return -1;
  
  return xtcf->colors.focus;
}
int xitk_config_get_select_color(xitk_config_t *xtcf) {
  
  if(!xtcf)
    return -1;
  
  return xtcf->colors.select;
}
unsigned long xitk_config_get_timer_label_animation(xitk_config_t *xtcf) {
  
  if(!xtcf)
    return 5000;
  
  return xtcf->timers.label_anim;
}
unsigned long xitk_config_get_warning_foreground(xitk_config_t *xtcf) {

  if(!xtcf)
    return -1;

  return xtcf->colors.warn_foreground;
}
unsigned long xitk_config_get_warning_background(xitk_config_t *xtcf) {
  
  if(!xtcf)
    return -1;

  return xtcf->colors.warn_background;
}

#define SYSTEM_RC  "/etc/xitkrc"
/*
 * Initialize config stuff.
 */
xitk_config_t *xitk_config_init(void) {
  xitk_config_t *xtcf;
  char          *rcfile;
  char          *user_rc = ".xitkrc";

  xtcf = (xitk_config_t *) xitk_xmalloc(sizeof(xitk_config_t));
  
  rcfile = (char *) xitk_xmalloc((strlen((xitk_get_homedir())) + strlen(user_rc)) + 2);
  sprintf(rcfile, "%s/%s", (xitk_get_homedir()), user_rc);
  
  if((xtcf->fd = fopen(rcfile, "r")) == NULL) {
    rcfile = (char *) realloc(rcfile, (strlen(SYSTEM_RC) + 1));
    sprintf(rcfile, "%s", SYSTEM_RC);
    if((xtcf->fd = fopen(rcfile, "r")) == NULL) {
      XITK_FREE(rcfile);
      rcfile = NULL;
    }
  }
  
  xtcf->cfgfilename = rcfile;
  
  xitk_config_init_default_values(xtcf);
  
  if(rcfile)
    xitk_config_load_configfile(xtcf);

  return xtcf;
}

/*
 * Release memory from config object.
 */
void xitk_config_deinit(xitk_config_t *xtcf) {

  if(!xtcf)
    return;

  XITK_FREE(xtcf->cfgfilename);
  XITK_FREE(xtcf->fonts.fallback);
  XITK_FREE(xtcf->fonts.system);
  XITK_FREE(xtcf);
}
