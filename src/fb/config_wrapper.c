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

#include "main.h"

extern struct fbxine fbxine;
int get_bool_value(const char *val);

/* 
 * experience level above this level 
 * can't be changed by cfg:/ mrl type.
 * This reflect xine-lib policy about security issue
 */
#define XINE_CONFIG_SECURITY   30

static void config_update(xine_cfg_entry_t *entry, int type, int min, int max, int value, char *string) {

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
  
  xine_config_update_entry(fbxine.xine, entry);
}

void config_update_range(char *key, int min, int max) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(fbxine.xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, range key %s isn't registered\n", key);
}

void config_update_string(char *key, char *string) {
  xine_cfg_entry_t entry;

  if((xine_config_lookup_entry(fbxine.xine, key, &entry)) && string)
    config_update(&entry, XINE_CONFIG_TYPE_STRING, 0, 0, 0, string);
  else {
    if(string == NULL)
      fprintf(stderr, "string is NULL\n");
    else
      fprintf(stderr, "WOW, string key %s isn't registered\n", key);
  }
}

void config_update_enum(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(fbxine.xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, enum key %s isn't registered\n", key);
}

void config_update_bool(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(fbxine.xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, bool key %s isn't registered\n", key);
}

void config_update_num(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(fbxine.xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, num key %s isn't registered\n", key);
}

void config_load(void) {
  xine_config_load(fbxine.xine, fbxine.configfile);
}

void config_save(void) {
  xine_config_save(fbxine.xine, fbxine.configfile);
}

void config_reset(void) {
  xine_config_reset(fbxine.xine);
  xine_config_load(fbxine.xine, fbxine.configfile);
}

/*
 * Handle 'cfg:/' mrl style
 */
void config_mrl(const char *mrl) {
  char *key;
  char *config;
  char *_mrl;

  xine_strdupa(_mrl, mrl);
  config = strchr(_mrl, '/');
  
  if(config && strlen(config))
    config++;

  while((key = xine_strsep(&config, ",")) != NULL) {
    char *str_value = strchr(key, ':');

    if(str_value && strlen(str_value))
      *str_value++ = '\0';

    if(str_value && strlen(str_value)) {
      xine_cfg_entry_t entry;
      
      if(xine_config_lookup_entry(fbxine.xine, key, &entry)) {

        if(entry.exp_level >= XINE_CONFIG_SECURITY) {
          fprintf(stderr, "For security reason, you're not allowed to change "
		  "the configuration entry named '%s'.", entry.key);
          return;
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

}
