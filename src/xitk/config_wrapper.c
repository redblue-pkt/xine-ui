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

#include "event.h"

extern gGui_t                 *gGui;

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
  
  xine_config_update_entry(gGui->xine, entry);
}

void config_update_range(char *key, int min, int max) {
  xine_cfg_entry_t entry;

  memset(&entry, 0, sizeof(xine_cfg_entry_t));
  if(xine_config_lookup_entry(gGui->xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, range key %s isn't registered\n", key);
}

void config_update_string(char *key, char *string) {
  xine_cfg_entry_t entry;

  memset(&entry, 0, sizeof(xine_cfg_entry_t));
  
  if((xine_config_lookup_entry(gGui->xine, key, &entry)) && string)
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

  memset(&entry, 0, sizeof(xine_cfg_entry_t));
  if(xine_config_lookup_entry(gGui->xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, enum key %s isn't registered\n", key);
}

void config_update_bool(char *key, int value) {
  xine_cfg_entry_t entry;

  memset(&entry, 0, sizeof(xine_cfg_entry_t));
  if(xine_config_lookup_entry(gGui->xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, bool key %s isn't registered\n", key);
}

void config_update_num(char *key, int value) {
  xine_cfg_entry_t entry;

  memset(&entry, 0, sizeof(xine_cfg_entry_t));
  if(xine_config_lookup_entry(gGui->xine, key, &entry)) 
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, num key %s isn't registered\n", key);
}
