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
#include <limits.h>	/*PATH_MAX*/
#include <dirent.h>
#include <sys/stat.h>       
#include "panel.h"
#include "control.h"
#include "mrl_browser.h"
#include "playlist.h"
#include "event.h"
#include "skins.h"
#include "xitk.h"
#include <xine/xineutils.h>

#ifndef NAME_MAX
#define NAME_MAX 256
#endif
#ifndef PATH_MAX
#define PATH_MAX 768
#endif


#define DEFAULT_SKIN "xinetic"

extern gGui_t             *gGui;

static skins_locations_t **skins_avail = NULL;
static int                 skins_avail_num = 0;

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
    char          fullfilename[PATH_MAX + NAME_MAX + 1];
    
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
  char    buf[NAME_MAX];

  skins_avail = (skins_locations_t **) xine_xmalloc(sizeof(skins_locations_t*));
  
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/.xineskins", xine_get_homedir());

  get_available_skins_from(buf);
  get_available_skins_from(XINE_SKINDIR);
  
}

/*
 * Return default skindir.
 */
char *skin_get_skindir(void) {
  static char tmp[2048];
  char *skin;
  
  memset(&tmp, 0, 2048);
  
  skin = gGui->config->register_string (gGui->config, "gui.skin", DEFAULT_SKIN,
					"gui skin theme", 
					NULL, NULL, NULL);
  
  snprintf(tmp, 2048, "%s/%s", XINE_SKINDIR, skin);

  return tmp;
}

/*
 * Return the full pathname the skin (default) configfile.
 */
char *skin_get_configfile(void) {
  static char tmp[2048];
  char *skin;
  
  memset(&tmp, 0, 2048);
  
  skin = gGui->config->register_string (gGui->config, "gui.skin", DEFAULT_SKIN,
					"gui skin theme", 
					NULL, NULL, NULL);
  
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
 * unload and reload the new config for a given
 * skin file. There is fallback if that fail.
 */
void change_skin(skins_locations_t *sk) {
  char                 buf[PATH_MAX + NAME_MAX + 1];
  char                *old_skin;
  skins_locations_t   *sks = sk;
  int                  twice = 0;

  if(!sk)
    return;

  old_skin = strdup(gGui->config->register_string (gGui->config, "gui.skin", DEFAULT_SKIN,
						   "gui skin theme", 
						   NULL, NULL, NULL));

  gGui->config->update_string (gGui->config, "gui.skin", (char *)sk->skin);

  xitk_skin_unload_config(gGui->skin_config);
  
  gGui->skin_config = xitk_skin_init_config();
  
 __reload_skin:
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/%s", sks->pathname, sks->skin);
  
  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    xine_error("Failed to load %s/%s. Reload old skin '%s'.\n", buf, "skinconfig", old_skin);
    gGui->config->update_string (gGui->config, "gui.skin", old_skin);
    sks = get_skin_location(old_skin);
    if(!twice) {
      twice++;
      goto __reload_skin;
    }
    else
      exit(-1);
  }
  
  panel_change_skins();
  control_change_skins();
  playlist_change_skins();
  mrl_browser_change_skins();
  free(old_skin);
}

/*
 * Initialize skin support.
 */
void init_skins_support(void) {
  skins_locations_t   *sk;
  char                *skin;
  char                buf[PATH_MAX + NAME_MAX + 1];
    
  gGui->skin_config = xitk_skin_init_config();
  
  looking_for_available_skins();
  
  if(skins_avail == NULL) {
    xine_error("No available skin found. Say goodbye.\n");
    exit(-1);
  }
  
  skin = gGui->config->register_string (gGui->config, "gui.skin", DEFAULT_SKIN,
					"gui skin theme", 
					NULL, NULL, NULL);

  sk = get_skin_location(skin);

  memset(&buf, 0, sizeof(buf));

  if(!sk) {
    xine_error("Ooch, skin '%s' not found, use fallback '%s'.\n", skin, DEFAULT_SKIN);
    gGui->config->update_string (gGui->config, "gui.skin", (char *)sk->skin);
    sprintf(buf, "%s/%s", XINE_SKINDIR, DEFAULT_SKIN);
  } else {
    sprintf(buf, "%s/%s", sk->pathname, sk->skin);
  }
  
  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    xine_error("Failed to load %s/%s. Exiting.\n", buf, "skinconfig");
    xitk_skin_free_config(gGui->skin_config);
    exit(-1);
  }
}
