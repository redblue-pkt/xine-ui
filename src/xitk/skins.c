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
#include <dirent.h>
#include <sys/stat.h>       
#include "panel.h"
#include "control.h"
#include "mrl_browser.h"
#include "playlist.h"
#include "videowin.h"
#include "event.h"
#include "skins.h"
#include "errors.h"
#include "i18n.h"

#include <xine.h>
#include <xine/xineutils.h>

#include "xitk.h"

/*
#define SKIN_DEBUG 1
*/

#define DEFAULT_SKIN        "xinetic"
#define SKIN_IFACE_VERSION  2

extern gGui_t             *gGui;

static skins_locations_t **skins_avail = NULL;
static int                 skins_avail_num = 0;
static int                 change_config_entry;
static char               **skin_names;

/*
 * Fill **skins_avail with available skins from path.
 */ 
static void get_available_skins_from(char *path) {
  DIR             *pdir;
  struct dirent   *pdirent;

  if(skins_avail == NULL)
    return;

  if((pdir = opendir(path)) != NULL) {
    struct stat   pstat, sstat;
    char          fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    char          skcfgname[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    while((pdirent = readdir(pdir)) != NULL) {
      memset(&fullfilename, 0, sizeof(fullfilename));
      sprintf(fullfilename, "%s/%s", path, pdirent->d_name);
      
      stat(fullfilename, &pstat);
      
      if((S_ISDIR(pstat.st_mode))
	 && (!(strlen(pdirent->d_name) == 1 && pdirent->d_name[0] == '.' )
	     && !(strlen(pdirent->d_name) == 2 
		  && (pdirent->d_name[0] == '.' && pdirent->d_name[1] == '.')))) {

	/*
	 * Check if a skinconfig file exist
	 */
	memset(&skcfgname, 0, sizeof(skcfgname));
	sprintf(skcfgname, "%s/%s", fullfilename, "skinconfig");
	if(((stat(skcfgname, &sstat)) > -1) && (S_ISREG(sstat.st_mode))) {
	  
	  skins_avail = (skins_locations_t **) realloc(skins_avail, 
						       (skins_avail_num + 2) * sizeof(skins_locations_t*));
	  skins_avail[skins_avail_num] = (skins_locations_t *) xine_xmalloc(sizeof(skins_locations_t));
	  
	  skins_avail[skins_avail_num]->pathname = strdup(path);
	  skins_avail[skins_avail_num]->skin = strdup(pdirent->d_name);
	  skins_avail_num++;
	  
	}
	else {
	  fprintf(stderr, "skinconfig file in '%s' is missing: skin '%s' skiped.\n",
		  fullfilename, pdirent->d_name);
	}
      }
    }
    closedir(pdir);
    skins_avail[skins_avail_num] = NULL;
  }
}

/*
 * Grab all available skins from $HOME/.xineskins/ and XINE_SKINDIR locations.
 */
static void looking_for_available_skins(void) {
  char    buf[XITK_NAME_MAX];

  skins_avail = (skins_locations_t **) xine_xmalloc(sizeof(skins_locations_t*));
  
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/.xine/skins", xine_get_homedir());
  
  get_available_skins_from(buf);
  get_available_skins_from(XINE_SKINDIR);
  
}

/*
 * Return default skindir.
 */
char *skin_get_skindir(void) {
  static char    tmp[2048];
  xine_cfg_entry_t *entry;
  char          *skin;
  
  memset(&tmp, 0, 2048);
  
  entry = xine_config_lookup_entry(gGui->xine, "gui.skin");

  if(entry)
    skin = (char *) skins_avail[entry->num_value]->skin;
  else
    skin = DEFAULT_SKIN;
  
  snprintf(tmp, 2048, "%s/%s", XINE_SKINDIR, skin);
  
  return tmp;
}

/*
 * Return the full pathname the skin (default) configfile.
 */
char *skin_get_configfile(void) {
  static char   tmp[2048];
  xine_cfg_entry_t *entry;
  char         *skin;
  
  memset(&tmp, 0, 2048);
  
  entry = xine_config_lookup_entry(gGui->xine, "gui.skin");

  if(entry)
    skin = (char *) skins_avail[entry->num_value]->skin;
  else
    skin = DEFAULT_SKIN;
  
  snprintf(tmp, 2048, "%s/%s/skinconfig", XINE_SKINDIR, skin);

  return tmp;
}

/*
 * Return pointer to skins_avail.
 */
skins_locations_t **get_available_skins(void) {
  return skins_avail;
}


/*
 * Return the number of available skins.
 */
int get_available_skins_num(void) {
  return skins_avail_num;
}

/*
 *
 */
int get_skin_offset(char *skin) {
  int i = 0;
  
  while(skins_avail[i] != NULL) {
    if(!strcasecmp(skin, skins_avail[i]->skin))
      return i;
    i++;
  }
  return -1;
}

/*
 * Try to find desired skin in skins_avail, then return
 * a pointer to the right entry.
 */
skins_locations_t *get_skin_location(char *skin) {
  int i = 0;
  
  while(skins_avail[i] != NULL) {
    if(!strcasecmp(skin, skins_avail[i]->skin))
      return skins_avail[i];
    i++;
  }
  return NULL;
}

/*
 * Select a new skin (used in control skin list).
 */
void select_new_skin(int selected) {
  xine_cfg_entry_t *entry;
  
  entry = xine_config_lookup_entry(gGui->xine, "gui.skin");
  entry->num_value = selected;
  xine_config_update_entry(gGui->xine, entry);
}

/*
 * unload and reload the new config for a given
 * skin file. There is fallback if that fail.
 */
void change_skin(skins_locations_t *sk) {
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char                *old_skin;
  skins_locations_t   *sks = sk;
  xine_cfg_entry_t    *entry;
  int                  twice = 0, twice_load = 0;

  if(!sk)
    return;
  
  entry = xine_config_lookup_entry(gGui->xine, "gui.skin");
  if(entry)
    old_skin = (char *) skins_avail[entry->num_value]->skin;
  else
    old_skin = DEFAULT_SKIN;
  
  xitk_skin_unload_config(gGui->skin_config);

  gGui->skin_config = xitk_skin_init_config();

  if(change_config_entry) {
    entry->num_value = get_skin_offset((char * )sk->skin);
    xine_config_update_entry(gGui->xine, entry);
  }

 __reload_skin:
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/%s", sks->pathname, sks->skin);

  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    skins_locations_t   *osks;
    
    entry->num_value = get_skin_offset(old_skin);
    xine_config_update_entry(gGui->xine, entry);

    osks = get_skin_location(old_skin);
    if((!strcmp(osks->pathname, sks->pathname)) && (!strcmp(osks->skin, sks->skin))) {
      xine_error(_("Failed to load %s/%s. Load fallback skin %s\n"), 
		 buf, "skinconfig", DEFAULT_SKIN);
      entry->num_value = get_skin_offset(DEFAULT_SKIN);
      xine_config_update_entry(gGui->xine, entry);
      sks = get_skin_location(DEFAULT_SKIN);
    }
    else {
      xine_error(_("Failed to load %s/%s. Reload old skin '%s'.\n"), buf, "skinconfig", old_skin);
      sks = osks;
    }
    
    if(!twice_load) {
      twice_load++;
      goto __reload_skin;
    }
    else {
      exit(-1);
    }
  }
  twice_load = 0;

  /* Check skin version */
  if(xitk_skin_check_version(gGui->skin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config(gGui->skin_config);
    gGui->skin_config = xitk_skin_init_config();
    xine_error(_("Failed to load %s, wrong version. Load fallback skin '%s'.\n"), 
	       buf, DEFAULT_SKIN);
    entry->num_value = get_skin_offset(old_skin);
    xine_config_update_entry(gGui->xine, entry);
    sks = get_skin_location(DEFAULT_SKIN);
    twice++;
    goto __reload_skin;
  }
  
  { /* Now, change skins for each window */
    typedef struct {
      void (*change_skins)(void);
    } visible_state_t;
    int i;
    visible_state_t visible_state[] = {
      { video_window_change_skins },
      { panel_change_skins        },
      { control_change_skins      },
      { playlist_change_skins     },
      { mrl_browser_change_skins  },
      { NULL                      }
    };
    
    for(i = 0; visible_state[i].change_skins != NULL; i++) {
      if(visible_state[i].change_skins)
	visible_state[i].change_skins();
    }

  }
}

/*
 *
 */
static void skin_change_cb(void *data, xine_cfg_entry_t *cfg) {
  skins_locations_t   *sk;
  
  if(skins_avail == NULL || change_config_entry == 0)
    return;
  
  /* First, try to see if the skin exist somewhere */
  sk = skins_avail[cfg->num_value];
  if(!sk) {
    xine_error(_("Ooch, skin not found, use fallback '%s'.\n"), DEFAULT_SKIN);
    sk = get_skin_location(DEFAULT_SKIN);
    cfg->num_value = get_skin_offset(DEFAULT_SKIN);
    xine_config_update_entry(gGui->xine, cfg);
  }

  change_config_entry = 0;
  change_skin(sk);
  change_config_entry = 1;

}

/*
 * Initialize skin support.
 */
void init_skins_support(void) {
  skins_locations_t   *sk;
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  int                  twice = 0, twice_load = 0;
  int                  skin_num, i;
    
  change_config_entry = 0;

  gGui->skin_config = xitk_skin_init_config();
  
  looking_for_available_skins();
  
  if((skins_avail == NULL) || (skins_avail_num == 0)) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }
  
  skin_names = (char **) xine_xmalloc(sizeof(char *) * (skins_avail_num + 1));
  for(i = 0; i < skins_avail_num; i++)
    skin_names[i] = strdup(skins_avail[i]->skin);
  skin_names[skins_avail_num] = NULL;

  skin_num = xine_config_register_enum (gGui->xine, "gui.skin", 
					(get_skin_offset(DEFAULT_SKIN)), skin_names,
					_("gui skin theme"), NULL, 0, skin_change_cb, NULL);
  
  sk = (skin_num < skins_avail_num) ? skins_avail[skin_num] : NULL;
  
  if(!sk) {
    xine_cfg_entry_t *entry = xine_config_lookup_entry(gGui->xine, "gui.skin");
    xine_error(_("Ooch, skin '%s' not found, use fallback '%s'.\n"), skins_avail[skin_num], 
	       (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    entry->num_value = get_skin_offset(DEFAULT_SKIN);
    xine_config_update_entry(gGui->xine, entry);
    sk = get_skin_location(DEFAULT_SKIN);
    if(!sk) {
      fprintf(stderr, _("Failed to load fallback skin. Check your installed skins. Exiting.\n"));
      exit(-1);
    }
  } 
  
 __reload_skin:
  memset(&buf, 0, XITK_PATH_MAX + XITK_NAME_MAX);
  sprintf(buf, "%s/%s", sk->pathname, sk->skin);

  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    if(!twice_load) {
      xine_cfg_entry_t *entry = xine_config_lookup_entry(gGui->xine, "gui.skin");
      twice_load++;
      fprintf(stderr, _("Failed to load %s/%s. Load fallback skin %s\n"), 
	      buf, "skinconfig", DEFAULT_SKIN);
      entry->num_value = get_skin_offset(DEFAULT_SKIN);
      xine_config_update_entry(gGui->xine, entry);
      sk = get_skin_location(DEFAULT_SKIN);
      goto __reload_skin;
    }
    else {
      fprintf(stderr, _("Failed to load %s/%s. Exiting.\n"), buf, "skinconfig");
      exit(-1);
    }
  }
  twice_load = 0;

  /* Check skin version */
  if(xitk_skin_check_version(gGui->skin_config, SKIN_IFACE_VERSION) < 1) {
    xine_cfg_entry_t *entry = xine_config_lookup_entry(gGui->xine, "gui.skin");
    xitk_skin_unload_config(gGui->skin_config);
    gGui->skin_config = xitk_skin_init_config();
    fprintf(stderr, _("Failed to load %s skin, wrong version. Load fallback skin '%s'.\n"), 
	    buf, (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    entry->num_value = get_skin_offset(DEFAULT_SKIN);
    xine_config_update_entry(gGui->xine, entry);
    sk = get_skin_location(DEFAULT_SKIN);
    sprintf(buf, "%s/%s", sk->pathname, sk->skin);
    if(!twice) {
      twice++;
      goto __reload_skin;
    }
    else {
      exit(-1);
    }
  }
  
  change_config_entry = 1;

}
