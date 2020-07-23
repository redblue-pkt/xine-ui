/* 
 * Copyright (C) 2000-2020 the xine project
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

struct skins_locations_s {
  const char *name, *fullname;
};

static int _skin_name_cmp (void *a, void *b) {
  skins_locations_t *d = (skins_locations_t *)a;
  skins_locations_t *e = (skins_locations_t *)b;
  return strcasecmp (d->name, e->name);
}

static skins_locations_t *_skin_add_1 (gGui_t *gui, int *index, const char *fullname, const char *name, const char *end) {
  skins_locations_t *s;
  int n, i;
  uint8_t *m = malloc (sizeof (*s) + (end - fullname) + 1);
  if (!m)
    return NULL;
  s = (skins_locations_t *)m;
  m += sizeof (*s);
  memcpy (m, fullname, end - fullname + 1);
  s->fullname = (const char *)m;
  m += name - fullname;
  s->name = (const char *)m;
  n = xine_sarray_size (gui->skins.avail);
  i = xine_sarray_add (gui->skins.avail, s);
  if (i >= 0) {
    *index = i;
    gui->skins.num = n + 1;
    if (!strcasecmp (name, DEFAULT_SKIN))
      gui->skins.default_skin = s;
    return s;
  }
  *index = ~i;
  free (s);
  return NULL;
}

static int _skin_name_index (gGui_t *gui, const char *name) {
  skins_locations_t s;
  int i;
  if (!gui)
    return -1;
  if (!name) {
    if (!gui->skins.current_skin)
      return -1;
    name = gui->skins.current_skin->name;
  }
  s.name = name;
  s.fullname = NULL;
  i = xine_sarray_binary_search (gui->skins.avail, &s);
  return i < 0 ? -1 : i;
}

int skin_add_1 (gGui_t *gui, const char *fullname, const char *name, const char *end) {
  int index;
  if (!gui || !fullname || !name || !end)
    return -1;
  if (_skin_add_1 (gui, &index, fullname, name, end)) {
    /* Attempt to update the existing config entry. Works with libxine 2020-07-23 (1.2.10hg).
     * Kills callback with old libxine. */
    int v1, v2, v3;
    xine_get_version (&v1, &v2, &v3);
    if (((v1 << 16) | (v2 << 8) | v3) >= 0x01020a) {
      const char *names[64];
      skin_get_names (gui, names, sizeof (names) / sizeof (names[0]));
      xine_config_register_enum (__xineui_global_xine_instance, "gui.skin",
        _skin_name_index (gui, DEFAULT_SKIN), (char **)names,
        _("gui skin theme"), CONFIG_NO_HELP, CONFIG_LEVEL_BEG, NULL, NULL);
    }
  }
  return index;
}

int skin_get_names (gGui_t *gui, const char **names, int max) {
  const char **q;
  int i, n;
  if (!gui || !names)
    return 0;
  if (!gui->skins.avail)
    return 0;
  q = names;
  n = xine_sarray_size (gui->skins.avail);
  gui->skins.num = n;
  if (n > max - 1)
    n = max - 1;
  for (i = 0; i < n; i++) {
    skins_locations_t *s = xine_sarray_get (gui->skins.avail, i);
    *q++ = s->name;
  }
  *q = NULL;
  return q - names;
}
    
const char *skin_get_name (gGui_t *gui, int index) {
  skins_locations_t *s;
  if (!gui)
    return NULL;
  if (!gui->skins.avail)
    return NULL;
  s = xine_sarray_get (gui->skins.avail, index);
  if (!s)
    return NULL;
  return s->name;
}
    
static const char *_skin_get_fullname (gGui_t *gui, int index) {
  skins_locations_t *s;
  if (!gui)
    return NULL;
  if (!gui->skins.avail)
    return NULL;
  s = xine_sarray_get (gui->skins.avail, index);
  if (!s)
    return NULL;
  return s->fullname;
}
    
static void _skin_start (gGui_t *gui) {
  gui->skins.get_names = skin_get_names;
  gui->skins.avail = xine_sarray_new (64, _skin_name_cmp);
#ifdef XINE_SARRAY_MODE_UNIQUE
  xine_sarray_set_mode (gui->skins.avail, XINE_SARRAY_MODE_UNIQUE);
#endif
  gui->skins.default_skin = NULL;
  gui->skins.current_skin = NULL;
  gui->skins.num = 0;
}

void skin_deinit (gGui_t *gui) {
  gui->skins.default_skin = NULL;
  gui->skins.current_skin = NULL;
  gui->skins.num = 0;
  if (gui->skins.avail) {
    int i, n = xine_sarray_size (gui->skins.avail);
    for (i = 0; i < n; i++)
      free (xine_sarray_get (gui->skins.avail, i));
    xine_sarray_delete (gui->skins.avail);
    gui->skins.avail = NULL;
  }
}

/*
 * Fill **skins_avail with available skins from path.
 */ 
static void _skin_add_dir (gGui_t *gui, const char *path) {
  char b[2048], *e = b + sizeof (b) - 1, *pend;
  DIR             *pdir;
  struct dirent   *pdirent;

  if (gui->skins.avail == NULL)
    return;
  pend = b + strlcpy (b, path, e - b);
  if (pend >= e)
    return;
  pdir = opendir (b);
  if (!pdir)
    return;
  *pend++ = '/';
  while ((pdirent = readdir (pdir))) {
    skins_locations_t *s;
    char *dend = pend + strlcpy (pend, pdirent->d_name, e - pend);
    int index;
    if (dend >= e)
      continue;
    if (pdirent->d_name[0] == '.') {
      if ((pdirent->d_name[1] == 0) || ((pdirent->d_name[1] == '.') && (pdirent->d_name[2] == 0)))
        continue;
    }
    if (!is_a_dir (b))
      continue;
    /* Check if a skinconfig file exist */
    *dend++ = '/';
    strlcpy (dend, "skinconfig", e - dend);
    if (!is_a_file (b)) {
      fprintf (stderr, "skinconfig file '%s' is missing: skin skipped.\n", b);
      continue;
    }
    *--dend = 0;
    s = _skin_add_1 (gui, &index, b, pend, dend);
#ifdef SKIN_DEBUG
    if (s)
      printf ("add %s at %d\n", s->fullname, index);
#else
    (void)s;
#endif
  }
  closedir (pdir);
}

/*
 * Grab all available skins from $HOME/.xine/skins and XINE_SKINDIR locations.
 */
static void _looking_for_available_skins (gGui_t *gui) {
  char    *buf;

  _skin_start (gui);
  buf = xitk_asprintf("%s%s", xine_get_homedir(), "/.xine/skins");
  if (buf) {
    _skin_add_dir (gui, buf);
    free(buf);
  }
  _skin_add_dir (gui, XINE_SKINDIR);
}

/*
 * Try to find desired skin in skins_avail, then return
 * a pointer to the right entry.
 */
#if 0
static skins_locations_t *_skin_name_location (gGui_t *gui, const char *name) {
  skins_locations_t s;
  int i;
  if (!name)
    return NULL;
  s.name = name;
  s.fullname = NULL;
  i = xine_sarray_binary_search (gui->skins.avail, &s);
  if (i < 0)
    return NULL;
  return (skins_locations_t *)xine_sarray_get (gui->skins.avail, i);
}
#endif

static skins_locations_t *_skin_index_location (gGui_t *gui, int index) {
  return (skins_locations_t *)xine_sarray_get (gui->skins.avail, index);
}

/*
 * unload and reload the new config for a given
 * skin file. There is fallback if that fail.
 */
static int _skin_alter (gGui_t *gui, int index) {
  int                  old_index;
  const char          *old_skin;
  skins_locations_t   *sks, *osks;
  const char          *skin_anim;
  xitk_skin_config_t  *nskin_config, *oskin_config;

  old_index = _skin_name_index (gui, NULL);
  sks = _skin_index_location (gui, index);
  if (!sks)
    return old_index;
  osks = gui->skins.current_skin;
  old_skin = osks ? osks->name : DEFAULT_SKIN;
  
  nskin_config = xitk_skin_init_config (gui->xitk);
  if (!nskin_config)
    return old_index;
  if (!xitk_skin_load_config (nskin_config, sks->fullname, "skinconfig")) {
    xitk_skin_unload_config (nskin_config);
    if (!osks || !strcmp (osks->fullname, sks->fullname)) {
      xine_error (gui, _("Failed to load %s/%s. Load fallback skin %s\n"), sks->fullname, "skinconfig", DEFAULT_SKIN);
      sks = gui->skins.default_skin;
      nskin_config = xitk_skin_init_config (gui->xitk);
      if (!nskin_config)
        return old_index;
      if (!xitk_skin_load_config (nskin_config, sks->fullname, "skinconfig")) {
        xitk_skin_unload_config (nskin_config);
        return old_index;
      }
    } else {
      xine_error (gui, _("Failed to load %s/%s. Reload old skin '%s'.\n"), sks->fullname, "skinconfig", old_skin);
      return old_index;
    }
  }
  /* Check skin version */
  if (xitk_skin_check_version (nskin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config (nskin_config);
    xine_error (gui, _("Failed to load %s, wrong version. Load fallback skin '%s'.\n"), sks->fullname, osks->name);
    return old_index;
  }
  gui->skins.current_skin = sks;
  
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

  return index;
}

static void gfx_quality_cb (void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  if (xitk_image_quality (gui->xitk, cfg->num_value)) {
    /* redraw skin. yes this does not need a full reload but this is rare,
     * and we dont want to add a whole new redraw API ;-) */
    _skin_alter (gui, _skin_name_index (gui, NULL));
  }
}

/*
 *
 */
void skin_change_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  int index, retval;
  
  if (!gui->skins.avail || gui->skins.change_config_entry)
    return;
  /* no recursion, please. */
  gui->skins.change_config_entry += 1;
  /* First, try to see if the skin exist somewhere */
  index = cfg->num_value;
/*
  sk = _skin_index_location (gui, index);
  if(!sk) {
    xine_error (gui, _("Ooch, skin not found, use fallback '%s'.\n"), DEFAULT_SKIN);
    sk = gui->skins.default_skin;
    index = _skin_name_index (gui, DEFAULT_SKIN);
  }
*/
  
  retval = _skin_alter (gui, index);
  
  if (retval != index) {
    if (retval >= 0)
      cfg->num_value = retval;
    else
      cfg->num_value = _skin_name_index (gui, DEFAULT_SKIN);
  }
  gui->skins.change_config_entry -= 1;
}

/*
 * Select a new skin (used in control skin list).
 */
void skin_select (gGui_t *gui, int selected) {
  (void)gui;
  config_update_num("gui.skin", selected);
}

/*
 * Return path of current skin
 */
char *skin_get_current_skin_dir (gGui_t *gui) {
  xine_cfg_entry_t     entry;
  const char          *fullname;

  if ((gui->skins.avail == NULL) || (gui->skins.num == 0)) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }
  
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  (void) xine_config_lookup_entry(__xineui_global_xine_instance, "gui.skin", &entry);
  fullname = _skin_get_fullname (gui, entry.num_value);
  return strdup (fullname);
}

/*
 * Initialize skin support.
 */
void skin_preinit (gGui_t *gui) {
  gui->skins.change_config_entry = 1;
  
  gui->skin_config = xitk_skin_init_config (gui->xitk);
  
  _looking_for_available_skins (gui);
  
  if ((gui->skins.avail == NULL) || (gui->skins.num == 0)) {
    fprintf(stderr, _("No available skin found. Say goodbye.\n"));
    exit(-1);
  }

  {
    const char *names[64];
    skin_get_names (gui, names, sizeof (names) / sizeof (names[0]));
    xine_config_register_enum (__xineui_global_xine_instance, "gui.skin",
      _skin_name_index (gui, DEFAULT_SKIN), (char **)names,
      _("gui skin theme"),
      CONFIG_NO_HELP, CONFIG_LEVEL_BEG, skin_change_cb, gui);
  }
  {
    int qmax = xitk_image_quality (gui->xitk, -2), v;
    v = xine_config_register_range (__xineui_global_xine_instance, "gui.gfx_quality", qmax, 0, qmax,
      _("Grapics quality"),
      _("Trade speed for quality with some screen modes."),
      20, gfx_quality_cb, gui);
    xitk_image_quality (gui->xitk, v);
  }
}

void skin_init (gGui_t *gui) {
  skins_locations_t   *sk;
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
  
  sk = _skin_index_location (gui, skin_num);
  if (!sk) {
    xine_error (gui, _("Ooch, skin '%s' not found, use fallback '%s'.\n"), "", DEFAULT_SKIN);
    config_update_num ("gui.skin", _skin_name_index (gui, DEFAULT_SKIN));
    sk = gui->skins.default_skin;
    if(!sk) {
      fprintf(stderr, _("Failed to load fallback skin. Check your installed skins. Exiting.\n"));
      exit(-1);
    }
  } 
  
 __reload_skin:
  
  if (!xitk_skin_load_config (gGui->skin_config, sk->fullname, "skinconfig")) {
    if(!twice_load) {
      twice_load++;
      fprintf (stderr, _("Failed to load %s/%s. Load fallback skin %s\n"),
        sk->fullname, "skinconfig", DEFAULT_SKIN);
      config_update_num ("gui.skin", _skin_name_index (gui, DEFAULT_SKIN));
      sk = gui->skins.default_skin;
      goto __reload_skin;
    }
    else {
      fprintf (stderr, _("Failed to load %s/%s. Exiting.\n"), sk->fullname, "skinconfig");
      exit(-1);
    }
  }
  twice_load = 0;

  /* Check skin version */
  if (xitk_skin_check_version (gui->skin_config, SKIN_IFACE_VERSION) < 1) {
    xitk_skin_unload_config (gui->skin_config);
    gui->skin_config = xitk_skin_init_config (gui->xitk);
    fprintf (stderr, _("Failed to load %s skin, wrong version. Load fallback skin '%s'.\n"),
      sk->fullname, DEFAULT_SKIN);
    config_update_num ("gui.skin", _skin_name_index (gui, DEFAULT_SKIN));
    sk = gui->skins.default_skin;
    if(!twice) {
      twice++;
      goto __reload_skin;
    }
    else {
      exit(-1);
    }
  }

  gui->skins.current_skin = sk;
  gui->skins.change_config_entry = 0;

  if((skin_anim = xitk_skin_get_animation (gui->skin_config)) != NULL) {
    gui->visual_anim.mrls[gui->visual_anim.num_mrls++] = strdup(skin_anim);
  }
}
