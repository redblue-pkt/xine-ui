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

#define DEFAULT_SKIN        "CelomaChrome"
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
    struct stat   pstat;
    char          fullfilename[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    
    while((pdirent = readdir(pdir)) != NULL) {
      memset(&fullfilename, 0, sizeof(fullfilename));
      sprintf(fullfilename, "%s/%s", path, pdirent->d_name);
      
      stat(fullfilename, &pstat);
      
      if((S_ISDIR(pstat.st_mode))
	 && (!(strlen(pdirent->d_name) == 1 && pdirent->d_name[0] == '.' )
	     && !(strlen(pdirent->d_name) == 2 
		  && (pdirent->d_name[0] == '.' && pdirent->d_name[1] == '.')))) {
	skins_avail = (skins_locations_t **) 
	  realloc(skins_avail, (skins_avail_num + 2) * sizeof(skins_locations_t*));
	skins_avail[skins_avail_num] = (skins_locations_t *) xine_xmalloc(sizeof(skins_locations_t));
	
	skins_avail[skins_avail_num]->pathname = strdup(path);
	skins_avail[skins_avail_num]->skin = strdup(pdirent->d_name);
	skins_avail_num++;
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
  cfg_entry_t   *entry;
  char          *skin;
  
  memset(&tmp, 0, 2048);
  
  entry = gGui->config->lookup_entry(gGui->config, "gui.skin");

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
  cfg_entry_t  *entry;
  char         *skin;
  
  memset(&tmp, 0, 2048);
  
  entry = gGui->config->lookup_entry(gGui->config, "gui.skin");

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
  
  gGui->config->update_num(gGui->config, "gui.skin", selected);
}

/*
 * unload and reload the new config for a given
 * skin file. There is fallback if that fail.
 */
void change_skin(skins_locations_t *sk) {
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char                *old_skin;
  skins_locations_t   *sks = sk;
  cfg_entry_t         *entry;
  int                  twice = 0;

  if(!sk)
    return;
  
  entry = gGui->config->lookup_entry(gGui->config, "gui.skin");
  if(entry)
    old_skin = (char *) skins_avail[entry->num_value]->skin;
  else
    old_skin = DEFAULT_SKIN;
  
  xitk_skin_unload_config(gGui->skin_config);

  gGui->skin_config = xitk_skin_init_config();

  if(change_config_entry)
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset((char * )sk->skin)));

 __reload_skin:
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/%s", sks->pathname, sks->skin);
  
  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    xine_error(_("Failed to load %s/%s. Reload old skin '%s'.\n"), buf, "skinconfig", old_skin);
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset(old_skin)));
    sks = get_skin_location(old_skin);
    if(!twice) {
      twice++;
      goto __reload_skin;
    }
    else {
      exit(-1);
    }
  }
  
  /* Check skin version */
  if(xitk_skin_check_version(gGui->skin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config(gGui->skin_config);
    gGui->skin_config = xitk_skin_init_config();
    xine_error(_("Failed to load %s, wrong version. Load fallback skin '%s'.\n"), 
	       buf, DEFAULT_SKIN);
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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
static void skin_change_cb(void *data, cfg_entry_t *cfg) {
  skins_locations_t   *sk;
  
  if(skins_avail == NULL || change_config_entry == 0)
    return;
  
  /* First, try to see if the skin exist somewhere */
  sk = skins_avail[cfg->num_value];
  if(!sk) {
    xine_error(_("Ooch, skin not found, use fallback '%s'.\n"), DEFAULT_SKIN);
    sk = get_skin_location(DEFAULT_SKIN);
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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
  int                  twice = 0;
  int                  skin_num, i;
    
  change_config_entry = 0;

  gGui->skin_config = xitk_skin_init_config();
  
  looking_for_available_skins();
  
  if(skins_avail == NULL) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }
  
  skin_names = (char **) xine_xmalloc(sizeof(char *) * (skins_avail_num + 1));
  for(i = 0; i < skins_avail_num; i++)
    skin_names[i] = strdup(skins_avail[i]->skin);
  skin_names[skins_avail_num] = NULL;

  skin_num = gGui->config->register_enum (gGui->config, "gui.skin", 
					  (get_skin_offset(DEFAULT_SKIN)), skin_names,
					  _("gui skin theme"), NULL, skin_change_cb, NULL);
  
  sk = (skin_num < skins_avail_num) ? skins_avail[skin_num] : NULL;
  
  if(!sk) {
    xine_error(_("Ooch, skin '%s' not found, use fallback '%s'.\n"), skins_avail[skin_num], 
	       (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset(DEFAULT_SKIN)));
    sk = get_skin_location(DEFAULT_SKIN);
    if(!sk) {
      fprintf(stderr, _("Failed to load fallback skin. Check your installed skins. Exiting.\n"));
      exit(-1);
    }
  } 
  
  memset(&buf, 0, XITK_PATH_MAX + XITK_NAME_MAX);
  sprintf(buf, "%s/%s", sk->pathname, sk->skin);

 __reload_skin:
  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    fprintf(stderr, _("Failed to load %s/%s. Exiting.\n"), buf, "skinconfig");
    xitk_skin_free_config(gGui->skin_config);
    exit(-1);
  }

  /* Check skin version */
  if(xitk_skin_check_version(gGui->skin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config(gGui->skin_config);
    gGui->skin_config = xitk_skin_init_config();
    fprintf(stderr, _("Failed to load %s skin, wrong version. Load fallback skin '%s'.\n"), 
	    buf, (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    gGui->config->update_num(gGui->config, "gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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
