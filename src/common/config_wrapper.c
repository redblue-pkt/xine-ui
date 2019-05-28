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
#include <string.h>
#include <stdlib.h>

#include <xine.h>

#include "config_wrapper.h"
#include "utils.h"
#include "globals.h"
#include "libcommon.h" /* strsep replacement */

#ifndef _
/* This is for other GNU distributions with internationalized messages.
   When compiling libc, the _ macro is predefined.  */
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
#  define _(msgid)	gettext (msgid)
# else
#  define _(msgid)	(msgid)
# endif
#endif

/* 
 * experience level above this level 
 * can't be changed by cfg:/ mrl type.
 * This reflect xine-lib policy about security issue
 */
#define XINE_CONFIG_SECURITY   30


static void config_update(xine_cfg_entry_t *entry, int type, int min, int max, int value, const char *string) {

  switch(type) {

  case XINE_CONFIG_TYPE_UNKNOWN:
    fprintf(stderr, "Config key '%s' isn't registered yet.\n", entry->key);
    return;
    break;

  case XINE_CONFIG_TYPE_RANGE:
    entry->range_min = min;
    entry->range_max = max;
    break;

  case XINE_CONFIG_TYPE_STRING: 
    entry->str_value = string;
    break;
    
  case XINE_CONFIG_TYPE_ENUM:
  case XINE_CONFIG_TYPE_NUM:
  case XINE_CONFIG_TYPE_BOOL:
    entry->num_value = value;
    break;

  default:
    fprintf(stderr, "Unknown config type %d\n", type);
    return;
    break;
  }
  
  xine_config_update_entry(__xineui_global_xine_instance, entry);
}

void config_update_range(const char *key, int min, int max) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, range key %s isn't registered\n", key);
}

void config_update_string(const char *key, const char *string) {
  xine_cfg_entry_t entry;

  if((xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) && string)
    config_update(&entry, XINE_CONFIG_TYPE_STRING, 0, 0, 0, string);
  else {
    if(string == NULL)
      fprintf(stderr, "string is NULL\n");
    else
      fprintf(stderr, "WOW, string key %s isn't registered\n", key);
  }
}

void config_update_enum(const char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, enum key %s isn't registered\n", key);
}

void config_update_bool(const char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, bool key %s isn't registered\n", key);
}

void config_update_num(const char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, num key %s isn't registered\n", key);
}

/*
 * Handle 'cfg:/' mrl style
 */
void config_mrl(const char *mrl) {
  xine_cfg_entry_t entry;
  char *key;
  char *config;
  char *_mrl;
  
  if (!xine_config_lookup_entry(__xineui_global_xine_instance, "misc.implicit_config", &entry) ||
      entry.type != XINE_CONFIG_TYPE_BOOL || !entry.num_value) {
    fprintf (stderr, _("You tried to change the configuration with a cfg: MRL.\n"
                       "This is not allowed unless you enable the 'misc.implicit_config' setting "
                       "after reading and understanding its help text."));
    return;
  }

  _mrl = strdup(mrl);
  config = strchr(_mrl, '/');
  
  if(config && strlen(config))
    config++;

  while((key = strsep(&config, ",")) != NULL) {
    char *str_value = strchr(key, ':');

    if(str_value && strlen(str_value))
      *str_value++ = '\0';

    if(str_value && strlen(str_value)) {
      
      if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry)) {

	if(entry.exp_level >= XINE_CONFIG_SECURITY) {
          fprintf (stderr, _("For security reason, you're not allowed to change "
                             "the configuration entry named '%s'."), entry.key);
	  break;
	}

	switch(entry.type) {

	case XINE_CONFIG_TYPE_STRING:
	  config_update(&entry, entry.type, 0, 0, 0, str_value);
	  break;
	  
	case XINE_CONFIG_TYPE_RANGE:
	case XINE_CONFIG_TYPE_ENUM:
	case XINE_CONFIG_TYPE_NUM:
	  config_update(&entry, ((entry.type == XINE_CONFIG_TYPE_RANGE) ? 
				 XINE_CONFIG_TYPE_NUM : entry.type), 
			0, 0, (strtol(str_value, &str_value, 10)), NULL);
	  break;

	case XINE_CONFIG_TYPE_BOOL:
	  config_update(&entry, entry.type, 0, 0, (get_bool_value(str_value)), NULL);
	  break;
	
	case XINE_CONFIG_TYPE_UNKNOWN:
	default:
	  fprintf(stderr, "WOW, key %s isn't registered\n", key);
	  break;
	}
      }

    }
  }

  free(_mrl);
}

