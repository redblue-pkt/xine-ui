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

#include "xitk.h"

#include "event.h"
#include "utils.h"

extern gGui_t          *gGui; 

#define DEFAULT_SKIN "xinetic"

/* Old "default" skin is unavailable now, change to "metal" */
#define CHECK_IF_NO_DEFAULT(sk) {                                             \
  if(!strncasecmp(sk, "default", strlen(sk))) {                               \
    config_set_str("skin", DEFAULT_SKIN);                                     \
    sk = config_lookup_str("skin", DEFAULT_SKIN);                             \
  }                                                                           \
}

/*
 * Return full pathname of a skin file.
 */
char *gui_get_skindir(const char *file) {
  static char tmp[256];
  char *skin;

  skin = config_lookup_str("skin", DEFAULT_SKIN);
  
  CHECK_IF_NO_DEFAULT(skin);
  
  sprintf(tmp, "%s/%s/%s", XINE_SKINDIR, skin, file);
  
  return tmp;
}

/*
 * Return 1 if *entry exist in skinfile description, 0 otherwise.
 */
int is_entry_exist(const char *entry) {
  FILE *fd_read;
  char skincfgfile[1024], tok[80], *skin, buf[256], *ln = buf;

  skin = config_lookup_str("skin", DEFAULT_SKIN);

  CHECK_IF_NO_DEFAULT(skin);

  snprintf(skincfgfile, 1024, "%s/%s/skinconfig", XINE_SKINDIR, skin);
  snprintf(tok, 80, "%s:", entry);
  
  if((fd_read = fopen(skincfgfile, "r")) != NULL) {
    ln = fgets(buf, 255, fd_read);
    while(ln != NULL) {
      while(*ln == ' ' || *ln == '\t') ++ln ;
      if(!strncasecmp(ln, tok, strlen(tok))) {
	fclose(fd_read);
	return 1;
      }
      ln = fgets(buf, 256, fd_read);
    }
    fclose(fd_read);
  } 
  else {
    fprintf(stderr, "SKIN: Unable to find entry '%s' in '%s' skin file."
	    " Exiting.\n", entry, skincfgfile);
    exit(-1);
  }

  return 0;
}

#define LN_FORWARD   {                                                      \
                      while(*ln == '\t' || *ln == ':') ++ln;  \
                      while(*ln != ':' && *ln != '\t'         \
                            && *ln != '\n' && *ln != 0) ++ln;               \
                      ++ln;                                                 \
                      while(*ln == '\t' || *ln == ':') ++ln;  \
                     }
/*
 * Return a *char value of column 'pos' for *entry in skinfile description.
 * Return NULL on failure.
 */
char *extract_value(const char *entry, int pos) {
  FILE *fd_read;
  char skincfgfile[1024], tok[80], *skin, buf[256], *ln = buf, *oln, val[256],
    *ret = NULL;
  int i;

  skin = config_lookup_str("skin", DEFAULT_SKIN);

  CHECK_IF_NO_DEFAULT(skin);

  snprintf(skincfgfile, 1024, "%s/%s/skinconfig", XINE_SKINDIR, skin);
  snprintf(tok, 80, "%s:", entry);
  memset(&val, 0, sizeof(val));

  if((fd_read = fopen(skincfgfile, "r")) != NULL) {
    ln = fgets(buf, 255, fd_read);
    while(ln != NULL) {
      while(*ln == '\t') ++ln ;
      if(!strncasecmp(ln, tok, strlen(tok))) {
	for(i=0;i<pos;i++) {
	  LN_FORWARD;
	}
	oln = ln;
	while(*ln == '\t' || *ln == ':') ++ln;
	while(*ln != ':' && *ln != '\t' && *ln != '\n' 
	      && *ln != 0) ++ln;
	memcpy(val, oln, ln-oln);
      }
      ln = fgets(buf, 256, fd_read);
    }
    fclose(fd_read);
  } 
  else
    return NULL;

  if(strlen(val) > 0) {
    ret = (char *) xmalloc(strlen(XINE_SKINDIR)+strlen(skin)+strlen(val)+3);
    sprintf(ret, "%s", val);
  }

  return ret;
}

/*
 * Return X coord of *str in skinfile
 */
int gui_get_skinX(const char *str) {
  char *v;
  int value = -1;

  if(is_entry_exist(str) && ((v = extract_value(str, 1)) != NULL)){
    value = strtol(v, &v, 10);
    }
  else {
    fprintf(stderr, "SKIN: Unable to get '%s' value. Exiting.\n", str);
    exit(-1);
  }
  
  return value;
}

/*
 * Return Y coord of *str in skinfile
 */
int gui_get_skinY(const char *str) {
  char *v;
  int value = -1;

  if(is_entry_exist(str) && ((v = extract_value(str, 2)) != NULL)) {
    value = strtol(v, &v, 10);
  }
  else {
    fprintf(stderr, "SKIN: Unable to get '%s' value. Exiting.\n", str);
    exit(-1);
  }
  
  return value;
}

/*
 * Return color for normal state
 */
char *gui_get_ncolor(const char *str) {
  char *v;
  
  if(is_entry_exist(str) && ((v = extract_value(str, 4)) != NULL))
    return v;
  
  return "Default";
}

/*
 * Return color for focused state
 */
char *gui_get_fcolor(const char *str) {
  char *v;
  
  if(is_entry_exist(str) && ((v = extract_value(str, 5)) != NULL)) 
    return v;
  
  return "Default";
}

/*
 * Return color for clicked state
 */
char *gui_get_ccolor(const char *str) {
  char *v;
  
  if(is_entry_exist(str) && ((v = extract_value(str, 6)) != NULL))
    return v;
  
  return "Default";
}

/*
 * Return length of given label.
 */
int gui_get_label_length(const char *str) {
  char *v;
  int len = 0;
  
  if(is_entry_exist(str) && ((v = extract_value(str, 4)) != NULL)) {
      len = atoi(v);
  }
  
  return len;
}

/*
 * Boolean, return animation value.
 */
int gui_get_animation(const char *str) {
  char *v;
  int anim = 0;
  
  if(is_entry_exist(str) && ((v = extract_value(str, 5)) != NULL)) {
    if(!strncasecmp(v, "TRUE", strlen(v)))
      anim = 1;
    else if(!strncasecmp(v, "FALSE", strlen(v)))
      anim = 0;
    else {
      anim = atoi(v);
    }
  }
  
  return anim;
}

/*
 * Return font name.
 */
char *gui_get_fontname(const char *str) {
  char *v = NULL;
  
  if(is_entry_exist(str))
    v = extract_value(str, 7);
  
  return v;
}

/*
 * Return image filename of *str in skinfile
 */
char *gui_get_skinfile(const char *str) {
  char *v=NULL;
  char *ret=NULL, *skin;
  
  skin = config_lookup_str("skin", DEFAULT_SKIN);
  
  CHECK_IF_NO_DEFAULT(skin);

  if(is_entry_exist(str) && ((v = extract_value(str, 3)) != NULL)) {
    ret = (char *) xmalloc(strlen(XINE_SKINDIR)+strlen(skin)+strlen(v)+3);
    sprintf(ret, "%s/%s/%s", XINE_SKINDIR, skin, v);
  }
  else {
    fprintf(stderr, "SKIN: Unable to find '%s:' entry(%s). Exiting.\n", str,v);
    exit(-1);
  }
  
  return ret;
}

/*
 * Automatic place some image on GUI, if 'AutoPlace:' is found.
 */
void gui_place_extra_images(widget_list_t *gui_widget_list) {
  FILE         *fd_read;
  char          skincfgfile[1024];
  char         *skin, tmp2[256], skinfile[1024];
  char          buf[256], *ln = buf, *oln;
  int           x, y;
  xitk_image_t  im;

  skin = config_lookup_str("skin", DEFAULT_SKIN);

  CHECK_IF_NO_DEFAULT(skin);

  snprintf(skincfgfile, 1024, "%s/%s/skinconfig", XINE_SKINDIR, skin);

  if((fd_read = fopen(skincfgfile, "r")) != NULL) {
    ln = fgets(buf, 255, fd_read);
    while(ln != NULL) {
      while(*ln == ' ' || *ln == '\t') ++ln ;
      
      if(!strncasecmp(ln, "AutoPlace:", 10)) {
	oln = ln; 
	LN_FORWARD;
	x = strtol(ln, &ln, 10);
	ln = oln;	
	oln = ln;
	LN_FORWARD; 
	LN_FORWARD;
	y = strtol(ln, &ln, 10);
	ln = oln;
	oln = ln;
	LN_FORWARD;
	LN_FORWARD;
	LN_FORWARD;
	if((strlen(ln)) > 1) {
	  strncpy(tmp2, ln, strlen(ln) - 1);
	  tmp2[strlen(ln) - 1] = '\0';
	  sprintf(skinfile, "%s/%s/%s", XINE_SKINDIR, skin, tmp2);

	  im.display = gGui->display;
	  im.imlibdata = gGui->imlib_data;
	  im.x = x;
	  im.y = y;
	  im.skin = skinfile;

	  gui_list_append_content (gui_widget_list->l, image_create (&im));	  
	}
      }
      ln = fgets(buf, 256, fd_read);
    }
    fclose(fd_read);
  }
  else {
    fprintf(stderr, "SKIN: Unable to open '%s'. Exiting.\n", skincfgfile);
    exit(-1);
  }
}
