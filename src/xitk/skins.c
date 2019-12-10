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
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "skins_internal.h"


skins_locations_t **skins_avail = NULL;
int                 skins_avail_num = 0;
char               **skin_names;
static int                 change_config_entry;

/*
 * Fill **skins_avail with available skins from path.
 */ 
static void get_available_skins_from(const char *path) {
  DIR             *pdir;
  struct dirent   *pdirent;

  if(skins_avail == NULL)
    return;

  if((pdir = opendir(path)) != NULL) {
    while((pdirent = readdir(pdir)) != NULL) {
      char          *fullfilename = NULL;
      char          *skcfgname = NULL;

      fullfilename = xitk_asprintf("%s/%s", path, pdirent->d_name);

      if (fullfilename && (is_a_dir(fullfilename))
	 && (!(strlen(pdirent->d_name) == 1 && pdirent->d_name[0] == '.' )
	     && !(strlen(pdirent->d_name) == 2 
		  && (pdirent->d_name[0] == '.' && pdirent->d_name[1] == '.')))) {

	/*
	 * Check if a skinconfig file exist
	 */
        skcfgname = xitk_asprintf("%s/%s", fullfilename, "skinconfig");
        if(skcfgname && is_a_file(skcfgname)) {
	  
	  skins_avail = (skins_locations_t **) realloc(skins_avail, (skins_avail_num + 2) * sizeof(skins_locations_t*));
	  skins_avail[skins_avail_num] = (skins_locations_t *) calloc(1, sizeof(skins_locations_t));
	  
	  skins_avail[skins_avail_num]->pathname = strdup(path);
	  skins_avail[skins_avail_num]->skin = strdup(pdirent->d_name);
	  skins_avail[skins_avail_num]->number = skins_avail_num;
#ifdef SKIN_DEBUG
	  printf("add %s/%s at %d\n", 
		 skins_avail[skins_avail_num]->pathname, 
		 skins_avail[skins_avail_num]->skin,
		 skins_avail[skins_avail_num]->number);
#endif
	  skins_avail_num++;
	  
	}
	else {
	  fprintf(stderr, "skinconfig file in '%s' is missing: skin '%s' skiped.\n",
		  fullfilename, pdirent->d_name);
	}
      }

      free(skcfgname);
      free(fullfilename);
    }

    closedir(pdir);
    skins_avail[skins_avail_num] = NULL;
  }
}

/*
 * Grab all available skins from $HOME/.xine/skins and XINE_SKINDIR locations.
 */
static void looking_for_available_skins(void) {
  char    *buf;

  skins_avail = (skins_locations_t **) calloc(1, sizeof(skins_locations_t*));
  
  buf = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/skins");
  if (buf) {
    get_available_skins_from(buf);
    free(buf);
  }

  get_available_skins_from(XINE_SKINDIR);
  
}

#if 0
/*
 * Return default skindir.
 */
static char *skin_get_skindir(void) {
  static char          tmp[2048];
  xine_cfg_entry_t     entry;
  char                *skin;
  
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  if(xine_config_lookup_entry(__xineui_global_xine_instance, "gui.skin", &entry))
    skin = (char *) skins_avail[entry.num_value]->skin;
  else
    skin = DEFAULT_SKIN;
  
  snprintf(tmp, sizeof(tmp), "%s/%s", XINE_SKINDIR, skin);
  
  return tmp;
}

/*
 * Return the full pathname the skin (default) configfile.
 */
static char *skin_get_configfile(void) {
  static char          tmp[2048];
  xine_cfg_entry_t     entry;
  char                *skin;
  
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  if (xine_config_lookup_entry(__xineui_global_xine_instance, "gui.skin",&entry)) 
    skin = (char *) skins_avail[entry.num_value]->skin;
  else
    skin = DEFAULT_SKIN;
  
  snprintf(tmp, sizeof(tmp), "%s/%s/skinconfig", XINE_SKINDIR, skin);

  return tmp;
}
#endif

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
int get_skin_offset(const char *skin) {
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
static skins_locations_t *get_skin_location(const char *skin) {
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
static int change_skin(skins_locations_t *sk) {
  gGui_t              *gui = gGui;
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  const char          *old_skin;
  skins_locations_t   *sks = sk;
  int                  twice = 0, twice_load = 0;
  int                  ret = 0;
  const char          *skin_anim;
  xitk_skin_config_t  *nskin_config, *oskin_config;

  if(!sk)
    return -1;
  
  ret = sk->number;

  old_skin = DEFAULT_SKIN;
  
  nskin_config = xitk_skin_init_config(gui->imlib_data);

 __reload_skin:
  snprintf(buf, sizeof(buf), "%s/%s", sks->pathname, sks->skin);

  if(!xitk_skin_load_config(nskin_config, buf, "skinconfig")) {
    skins_locations_t   *osks;
    
    ret = get_skin_offset(old_skin);

    osks = get_skin_location(old_skin);
    if((!strcmp(osks->pathname, sks->pathname)) && (!strcmp(osks->skin, sks->skin))) {
      xine_error (gui, _("Failed to load %s/%s. Load fallback skin %s\n"), buf, "skinconfig", DEFAULT_SKIN);
      ret = get_skin_offset(DEFAULT_SKIN);
      sks = get_skin_location(DEFAULT_SKIN);
    }
    else {
      xine_error (gui, _("Failed to load %s/%s. Reload old skin '%s'.\n"), buf, "skinconfig", old_skin);
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
  if(xitk_skin_check_version(nskin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config(nskin_config);
    nskin_config = xitk_skin_init_config(gui->imlib_data);
    xine_error (gui, _("Failed to load %s, wrong version. Load fallback skin '%s'.\n"), buf, DEFAULT_SKIN);
    ret = get_skin_offset(DEFAULT_SKIN);
    sks = get_skin_location(DEFAULT_SKIN);
    twice++;
    goto __reload_skin;
  }
  
  if(gui->visual_anim.mrls[gui->visual_anim.num_mrls]) {
    free(gui->visual_anim.mrls[gui->visual_anim.num_mrls]);
    gui->visual_anim.mrls[gui->visual_anim.num_mrls--] = NULL;
  }
  if((skin_anim = xitk_skin_get_animation(nskin_config)) != NULL) {
    gui->visual_anim.mrls[gui->visual_anim.num_mrls++] = strdup(skin_anim);
  }
  
  oskin_config = gui->skin_config;
  gui->skin_config = nskin_config;

  /* Now, change skins for each window */
  video_window_change_skins (gui->vwin, 1);
  panel_change_skins (gui->panel, 1);
  control_change_skins (gui->vctrl, 1);
  mrl_browser_change_skins (gui->mrlb, 1);

  playlist_change_skins (1);

  xitk_skin_unload_config(oskin_config);

  return ret;
}

/*
 *
 */
void skin_change_cb(void *data, xine_cfg_entry_t *cfg) {
  skins_locations_t   *sk;
  int                  retval;
  
  if(skins_avail == NULL || change_config_entry == 0)
    return;
  
  /* First, try to see if the skin exist somewhere */
  sk = skins_avail[cfg->num_value];
  if(!sk) {
    xine_error (gGui, _("Ooch, skin not found, use fallback '%s'.\n"), DEFAULT_SKIN);
    sk = get_skin_location(DEFAULT_SKIN);
  }
  
  retval = change_skin(sk);
  
  if(retval != sk->number) {
    if(retval >= 0)
      cfg->num_value = retval;
    else
      cfg->num_value = get_skin_offset(DEFAULT_SKIN);
  }
}

/*
 * Select a new skin (used in control skin list).
 */
void select_new_skin(int selected) {
  config_update_num("gui.skin", selected);
}

/*
 * Return path of current skin
 */
char *skin_get_current_skin_dir(void) {
  xine_cfg_entry_t     entry;
  int                  skin_num = 0;
  char                *skin_dir;

  if((skins_avail == NULL) || (skins_avail_num == 0)) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }
  
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  (void) xine_config_lookup_entry(__xineui_global_xine_instance, "gui.skin", &entry);
  skin_num = entry.num_value;
  
  skin_dir = xitk_asprintf("%s/%s", skins_avail[skin_num]->pathname, skins_avail[skin_num]->skin);

  return skin_dir;
}

/*
 * Initialize skin support.
 */
void preinit_skins_support(void) {
  int  i;

  change_config_entry = 0;
  
  gGui->skin_config = xitk_skin_init_config(gGui->imlib_data);
  
  looking_for_available_skins();
  
  if((skins_avail == NULL) || (skins_avail_num == 0)) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }
  
  skin_names = (char **) calloc((skins_avail_num + 1), sizeof(char *));

  for(i = 0; i < skins_avail_num; i++)
    skin_names[i] = strdup(skins_avail[i]->skin);
  
  skin_names[skins_avail_num] = NULL;
  
  xine_config_register_enum (__xineui_global_xine_instance, "gui.skin", 
					(get_skin_offset(DEFAULT_SKIN)),
					skin_names,
					_("gui skin theme"), 
					CONFIG_NO_HELP, 
					CONFIG_LEVEL_BEG,
					skin_change_cb, 
					CONFIG_NO_DATA);
}

void init_skins_support(void) {
  skins_locations_t   *sk;
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  int                  twice = 0, twice_load = 0;
  int                  skin_num;
  const char          *skin_anim;
  xine_cfg_entry_t     entry;

  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  if(xine_config_lookup_entry(__xineui_global_xine_instance, "gui.skin", &entry))
    skin_num = entry.num_value;
  else {
    fprintf(stderr, _("Ooch, gui.skin config entry isn't registered. Say goodbye.\n"));
    exit(-1);
  }
  
  sk = (skin_num < skins_avail_num) ? skins_avail[skin_num] : NULL;
  
  if(!sk) {
    xine_error (gGui, _("Ooch, skin '%s' not found, use fallback '%s'.\n"), skins_avail[skin_num]->skin,
      (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    config_update_num("gui.skin", (get_skin_offset(DEFAULT_SKIN)));
    sk = get_skin_location(DEFAULT_SKIN);
    if(!sk) {
      fprintf(stderr, _("Failed to load fallback skin. Check your installed skins. Exiting.\n"));
      exit(-1);
    }
  } 
  
 __reload_skin:
  snprintf(buf, sizeof(buf), "%s/%s", sk->pathname, sk->skin);
  
  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    if(!twice_load) {
      twice_load++;
      fprintf(stderr, _("Failed to load %s/%s. Load fallback skin %s\n"), 
	      buf, "skinconfig", DEFAULT_SKIN);
      config_update_num("gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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
    xitk_skin_unload_config(gGui->skin_config);
    gGui->skin_config = xitk_skin_init_config(gGui->imlib_data);
    fprintf(stderr, _("Failed to load %s skin, wrong version. Load fallback skin '%s'.\n"), 
	    buf, (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    config_update_num("gui.skin", (get_skin_offset(DEFAULT_SKIN)));
    sk = get_skin_location(DEFAULT_SKIN);
    snprintf(buf, sizeof(buf), "%s/%s", sk->pathname, sk->skin);
    if(!twice) {
      twice++;
      goto __reload_skin;
    }
    else {
      exit(-1);
    }
  }
  
  change_config_entry = 1;

  if((skin_anim = xitk_skin_get_animation(gGui->skin_config)) != NULL) {
    gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++] = strdup(skin_anim);
  }
  
}

