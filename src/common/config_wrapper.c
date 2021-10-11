/*
 * Copyright (C) 2000-2021 the xine project
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

int config_update_string (xine_t *xine, const char *key, const char *string) {
  xine_cfg_entry_t entry;

  if ((xine_config_lookup_entry (xine, key, &entry)) && string) {
    entry.str_value = (char *)string; /** << will not be written to */
    xine_config_update_entry (xine, &entry);
    return 1;
  }
  if (!string)
    fprintf (stderr, "string is NULL\n");
  else
    fprintf (stderr, "WOW, string key %s isn't registered\n", key);
  return 0;
}

int config_update_num (xine_t *xine, const char *key, int value) {
  xine_cfg_entry_t entry;

  if (xine_config_lookup_entry (xine, key, &entry)) {
    entry.num_value = value;
    xine_config_update_entry (xine, &entry);
    return 1;
  }
  fprintf (stderr, "WOW, num key %s isn't registered\n", key);
  return 0;
}

/*
 * Handle 'cfg:/' mrl style
 */
int config_mrl (xine_t *xine, const char *mrl) {
  xine_cfg_entry_t entry;
  char *key, *config, *_mrl;
  int n = 0;

  if (!xine_config_lookup_entry (xine, "misc.implicit_config", &entry) ||
      entry.type != XINE_CONFIG_TYPE_BOOL || !entry.num_value) {
    fprintf (stderr, _("You tried to change the configuration with a cfg: MRL.\n"
                       "This is not allowed unless you enable the 'misc.implicit_config' setting "
                       "after reading and understanding its help text."));
    return 0;
  }

  _mrl = strdup (mrl);
  if (!_mrl)
    return 0;
  config = strchr (_mrl, '/');

  if (config && config[0])
    config++;

  while ((key = strsep(&config, ",")) != NULL) {
    char *str_value = strchr(key, ':');

    if (str_value && str_value[0])
      *str_value++ = '\0';

    if (str_value && str_value[0]) {

      if (xine_config_lookup_entry (xine, key, &entry)) {
        int set = 0;

	if(entry.exp_level >= XINE_CONFIG_SECURITY) {
          fprintf (stderr, _("For security reason, you're not allowed to change "
                             "the configuration entry named '%s'."), entry.key);
	  break;
	}

        switch (entry.type) {
          case XINE_CONFIG_TYPE_STRING:
            entry.str_value = str_value;
            set = 1;
            break;
          case XINE_CONFIG_TYPE_RANGE:
          case XINE_CONFIG_TYPE_ENUM:
          case XINE_CONFIG_TYPE_NUM:
            entry.num_value = strtol (str_value, &str_value, 10);
            set = 1;
            break;
          case XINE_CONFIG_TYPE_BOOL:
            entry.num_value = get_bool_value (str_value);
            set = 1;
            break;
          default:
            fprintf (stderr, "WOW, key %s is of unknown type %d.\n", key, (int)entry.type);
        }
        if (set) {
          xine_config_update_entry (xine, &entry);
          n++;
        }
      }
    }
  }

  free(_mrl);
  return n;
}
