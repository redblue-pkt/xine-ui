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

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>       
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "common.h"

typedef struct {
  char      *name;
  struct {
    char    *name;
    char    *email;
  } author;
  struct {
    char    *href;
    char    *preview;
    int      version;
    int      maintained;
  } skin;
} slx_entry_t;

#define WINDOW_WIDTH  400
#define WINDOW_HEIGHT 380

#define NORMAL_CURS   0
#define WAIT_CURS     1

typedef struct {
  xitk_window_t        *xwin;
  
  xitk_widget_t        *browser;
  
  Cursor                cursor[2];

  xitk_image_t         *preview_image;
  int                   preview_width;
  int                   preview_height;

  xitk_widget_list_t   *widget_list;
  char                **entries;
  int                   num;
  
  slx_entry_t         **slxs;

  xitk_register_key_t   widget_key;
} skin_downloader_t;

static skin_downloader_t  *skdloader = NULL;

#undef SKIN_DEBUG

#define DEFAULT_SKIN        "xinetic"
#define SKIN_IFACE_VERSION  4

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
  static char          tmp[2048];
  xine_cfg_entry_t  entry;
  char                *skin;
  
  memset(&tmp, 0, 2048);
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  if(xine_config_lookup_entry(gGui->xine, "gui.skin", &entry))
    skin = (char *) skins_avail[entry.num_value]->skin;
  else
    skin = DEFAULT_SKIN;
  
  snprintf(tmp, 2048, "%s/%s", XINE_SKINDIR, skin);
  
  return tmp;
}

/*
 * Return the full pathname the skin (default) configfile.
 */
char *skin_get_configfile(void) {
  static char          tmp[2048];
  xine_cfg_entry_t  entry;
  char                *skin;
  
  memset(&tmp, 0, 2048);
  memset(&entry, 0, sizeof(xine_cfg_entry_t)); 
  if (xine_config_lookup_entry(gGui->xine, "gui.skin",&entry)) 
    skin = (char *) skins_avail[entry.num_value]->skin;
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
 * unload and reload the new config for a given
 * skin file. There is fallback if that fail.
 */
static int change_skin(skins_locations_t *sk) {
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char                *old_skin;
  skins_locations_t   *sks = sk;
  int                  twice = 0, twice_load = 0;
  int                  ret = 0;
  char                *skin_anim;

  if(!sk)
    return -1;
  
  ret = sk->number;

  old_skin = DEFAULT_SKIN;
  
  xitk_skin_unload_config(gGui->skin_config);

  gGui->skin_config = xitk_skin_init_config();

 __reload_skin:
  memset(&buf, 0, sizeof(buf));
  sprintf(buf, "%s/%s", sks->pathname, sks->skin);

  if(!xitk_skin_load_config(gGui->skin_config, buf, "skinconfig")) {
    skins_locations_t   *osks;
    
    ret = get_skin_offset(old_skin);

    osks = get_skin_location(old_skin);
    if((!strcmp(osks->pathname, sks->pathname)) && (!strcmp(osks->skin, sks->skin))) {
      xine_error(_("Failed to load %s/%s. Load fallback skin %s\n"), 
		 buf, "skinconfig", DEFAULT_SKIN);
      ret = get_skin_offset(DEFAULT_SKIN);
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
    ret = get_skin_offset(DEFAULT_SKIN);
    sks = get_skin_location(DEFAULT_SKIN);
    twice++;
    goto __reload_skin;
  }
  
  if(gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]) {
    free(gGui->visual_anim.mrls[gGui->visual_anim.num_mrls]);
    gGui->visual_anim.mrls[gGui->visual_anim.num_mrls--] = NULL;
  }
  if((skin_anim = xitk_skin_get_animation(gGui->skin_config)) != NULL) {
    gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++] = strdup(skin_anim);
  }
  
  { /* Now, change skins for each window */
    typedef struct {
      void (*change_skins)(void);
    } visible_state_t;
    int   i;
    visible_state_t visible_state[] = {
      { video_window_change_skins },
      { panel_change_skins        },
      { control_change_skins      },
      { playlist_change_skins     },
      { mrl_browser_change_skins  },
      { NULL                      }
    };
    
    for(i = 0; visible_state[i].change_skins != NULL; i++) {
      if(visible_state[i].change_skins) {
	visible_state[i].change_skins();
      }
    }
  }

  return ret;
}

/*
 *
 */
static void skin_change_cb(void *data, xine_cfg_entry_t *cfg) {
  skins_locations_t   *sk;
  int                  retval;
  
  if(skins_avail == NULL || change_config_entry == 0)
    return;
  
  /* First, try to see if the skin exist somewhere */
  sk = skins_avail[cfg->num_value];
  if(!sk) {
    xine_error(_("Ooch, skin not found, use fallback '%s'.\n"), DEFAULT_SKIN);
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
 * Initialize skin support.
 */
void init_skins_support(void) {
  skins_locations_t   *sk;
  char                 buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  int                  twice = 0, twice_load = 0;
  int                  skin_num, i;
  char                *skin_anim;
    
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
					(get_skin_offset(DEFAULT_SKIN)),
					skin_names,
					_("gui skin theme"), 
					CONFIG_NO_HELP, 
					CONFIG_LEVEL_BEG,
					skin_change_cb, 
					CONFIG_NO_DATA);

  sk = (skin_num < skins_avail_num) ? skins_avail[skin_num] : NULL;
  
  if(!sk) {
    xine_error(_("Ooch, skin '%s' not found, use fallback '%s'.\n"), skins_avail[skin_num], 
	       (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    config_update_num("gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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
    gGui->skin_config = xitk_skin_init_config();
    fprintf(stderr, _("Failed to load %s skin, wrong version. Load fallback skin '%s'.\n"), 
	    buf, (skins_avail[(get_skin_offset(DEFAULT_SKIN))]->skin));
    config_update_num("gui.skin", (get_skin_offset(DEFAULT_SKIN)));
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

  if((skin_anim = xitk_skin_get_animation(gGui->skin_config)) != NULL) {
    gGui->visual_anim.mrls[gGui->visual_anim.num_mrls++] = strdup(skin_anim);
  }
  
}

/*
 * Remote skin loader
 */
static void download_set_cursor_state(int state) {
  XLockDisplay(gGui->display);
  XDefineCursor(gGui->display, 
		(xitk_window_get_window(skdloader->xwin)), skdloader->cursor[state]);
  XSync(gGui->display, False);
  XUnlockDisplay(gGui->display);
}

static slx_entry_t **skins_get_slx_entries(char *url) {
  int              result;
  slx_entry_t    **slxs = NULL, slx;
  xml_node_t      *xml_tree, *skin_entry, *skin_ref;
  xml_property_t  *skin_prop;
  download_t       download;
  
  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0; 
  
  if((network_download(url, &download))) {
    int entries_slx = 0;
    
    xml_parser_init(download.buf, download.size, XML_PARSER_CASE_INSENSITIVE);
    if((result = xml_parser_build_tree(&xml_tree)) != XML_PARSER_OK)
      goto __failure;
    
    if(!strcasecmp(xml_tree->name, "SLX")) {
      
      skin_prop = xml_tree->props;
      
      while((skin_prop) && (strcasecmp(skin_prop->name, "VERSION")))
	skin_prop = skin_prop->next;
      
      if(skin_prop) {
	int  version_major, version_minor = 0;
	
	if((((sscanf(skin_prop->value, "%d.%d", &version_major, &version_minor)) == 2) ||
	    ((sscanf(skin_prop->value, "%d", &version_major)) == 1)) && 
	   ((version_major >= 2) && (version_minor >= 0))) {
	  
	  skin_entry = xml_tree->child;
	  
	  slx.name = slx.author.name = slx.author.email = slx.skin.href = slx.skin.preview = NULL;
	  slx.skin.version = slx.skin.maintained = 0;
	  
	  while(skin_entry) {

	    if(!strcasecmp(skin_entry->name, "SKIN")) {
	      skin_ref = skin_entry->child;

	      while(skin_ref) {

		if(!strcasecmp(skin_ref->name, "NAME")) {
		  slx.name = strdup(skin_ref->data);
		}
		else if(!strcasecmp(skin_ref->name, "AUTHOR")) {
		  for(skin_prop = skin_ref->props; skin_prop; skin_prop = skin_prop->next) {
		    if(!strcasecmp(skin_prop->name, "NAME")) {
		      slx.author.name = strdup(skin_prop->value);
		    }
		    else if(!strcasecmp(skin_prop->name, "EMAIL")) {
		      slx.author.email = strdup(skin_prop->value);
		    }
		  }
		}
		else if(!strcasecmp(skin_ref->name, "REF")) {
		  for(skin_prop = skin_ref->props; skin_prop; skin_prop = skin_prop->next) {
		    if(!strcasecmp(skin_prop->name, "HREF")) {
		      slx.skin.href = strdup(skin_prop->value);
		    }
		    else if(!strcasecmp(skin_prop->name, "VERSION")) {
		      slx.skin.version = atoi(skin_prop->value);
		    }
		    else if(!strcasecmp(skin_prop->name, "MAINTAINED")) {
		      slx.skin.maintained = get_bool_value(skin_prop->value);
		    }
		  }
		}
		else if(!strcasecmp(skin_ref->name, "PREVIEW")) {
		  for(skin_prop = skin_ref->props; skin_prop; skin_prop = skin_prop->next) {
		    if(!strcasecmp(skin_prop->name, "HREF")) {
		      slx.skin.preview = strdup(skin_prop->value);
		    }
		  }
		}

		skin_ref = skin_ref->next;
	      }

	      if(slx.name && slx.skin.href && 
		 ((slx.skin.version >= SKIN_IFACE_VERSION) && slx.skin.maintained)) {
		
#if 0
		printf("get slx: Skin number %d:\n", entries_slx);
		printf("  Name: %s\n", slx.name);
		printf("  Author Name: %s\n", slx.author.name);
		printf("  Author email: %s\n", slx.author.email);
		printf("  Href: %s\n", slx.skin.href);
		printf("  Preview: %s\n", slx.skin.preview);
		printf("  Version: %d\n", slx.skin.version);
		printf("  Maintained: %d\n", slx.skin.maintained);
		printf("--\n");
#endif
		
		if(entries_slx == 0)
		  slxs = (slx_entry_t **) xine_xmalloc(sizeof(slx_entry_t *) * 2);
		else
		  slxs = (slx_entry_t **) realloc(slxs, sizeof(slx_entry_t *) * (entries_slx + 2));
		
		slxs[entries_slx] = (slx_entry_t *) xine_xmalloc(sizeof(slx_entry_t));
		slxs[entries_slx]->name            = strdup(slx.name);
		slxs[entries_slx]->author.name     = slx.author.name ? strdup(slx.author.name) : NULL;
		slxs[entries_slx]->author.email    = slx.author.email ? strdup(slx.author.email) : NULL;
		slxs[entries_slx]->skin.href       = strdup(slx.skin.href);
		slxs[entries_slx]->skin.preview    = slx.skin.preview ? strdup(slx.skin.preview) : NULL;
		slxs[entries_slx]->skin.version    = slx.skin.version;
		slxs[entries_slx]->skin.maintained = slx.skin.maintained;

		entries_slx++;

		slxs[entries_slx] = NULL;
		
	      }
	      
	      SAFE_FREE(slx.name);
	      SAFE_FREE(slx.author.name);
	      SAFE_FREE(slx.author.email);
	      SAFE_FREE(slx.skin.href);
	      SAFE_FREE(slx.skin.preview);
	      slx.skin.version = 0;
	      slx.skin.maintained = 0;
	    }

	    skin_entry = skin_entry->next;
	  }
	}
      }
    }
    
    xml_parser_free_tree(xml_tree);

    if(entries_slx)
      slxs[entries_slx] = NULL;

  }
  else
    xine_error(_("Unable to retrieve skin list from %s: %s"), url, download.error);
 
 __failure:
  
  if(download.buf)
    free(download.buf);
  if(download.error)
    free(download.error);
  
  return slxs;
}

static void download_skin_cancel(xitk_widget_t *w, void *data) {
  int i;
  
  xitk_unregister_event_handler(&skdloader->widget_key);
  
  xitk_image_free_image(gGui->imlib_data, &skdloader->preview_image);

  xitk_destroy_widgets(skdloader->widget_list);
  xitk_window_destroy_window(gGui->imlib_data, skdloader->xwin);

  skdloader->xwin = None;

  xitk_list_free((XITK_WIDGET_LIST_LIST(skdloader->widget_list)));
  
  XLockDisplay(gGui->display);
  XFreeCursor(gGui->display, skdloader->cursor[NORMAL_CURS]);
  XFreeCursor(gGui->display, skdloader->cursor[WAIT_CURS]);
  XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(skdloader->widget_list)));
  XUnlockDisplay(gGui->display);
  
  free(skdloader->widget_list);
  
  for(i = 0; i < skdloader->num; i++) {
    SAFE_FREE(skdloader->slxs[i]->name);
    SAFE_FREE(skdloader->slxs[i]->author.name);
    SAFE_FREE(skdloader->slxs[i]->author.email);
    SAFE_FREE(skdloader->slxs[i]->skin.href);
    SAFE_FREE(skdloader->slxs[i]->skin.preview);
    free(skdloader->slxs[i]);
    free(skdloader->entries[i]);
  }
  
  free(skdloader->slxs);
  free(skdloader->entries);

  skdloader->num = 0;

  free(skdloader);
  skdloader = NULL;
}

static void download_update_blank_preview(void) {

  XLockDisplay(gGui->display);
  XSetForeground(gGui->display,(XITK_WIDGET_LIST_GC(skdloader->widget_list)),
		 (xitk_get_pixel_color_from_rgb(gGui->imlib_data, 52, 52, 52)));
  XFillRectangle(gGui->display, 
		 (XITK_WIDGET_LIST_WINDOW(skdloader->widget_list)),
		 (XITK_WIDGET_LIST_GC(skdloader->widget_list)), 10, 30, 
		  skdloader->preview_image->image->width, skdloader->preview_image->image->height);
  XUnlockDisplay(gGui->display);

}

static void download_update_preview(void) {

  download_update_blank_preview();

  if(skdloader->preview_image->image && skdloader->preview_image->image->pixmap) {
    GC   lgc;
    int  x, y;
    
    XLockDisplay (gGui->display);
    lgc = XCreateGC(gGui->display, (XITK_WIDGET_LIST_WINDOW(skdloader->widget_list)), None, None);
    XCopyGC(gGui->display, 
	    (XITK_WIDGET_LIST_GC(skdloader->widget_list)), (1 << GCLastBit) - 1, lgc);
    XUnlockDisplay (gGui->display);
    
    y = 30 + ((skdloader->preview_image->image->height - skdloader->preview_height) >> 1);
    x = 10 + ((skdloader->preview_image->image->width - skdloader->preview_width) >> 1);
    
    if(skdloader->preview_image->mask && skdloader->preview_image->mask->pixmap) {
      XLockDisplay (gGui->display);
      XSetClipOrigin(gGui->display, lgc, x, y);
      XSetClipMask(gGui->display, lgc, skdloader->preview_image->mask->pixmap);
      XUnlockDisplay (gGui->display);
    }
    
    XLockDisplay (gGui->display);
    XCopyArea (gGui->display, skdloader->preview_image->image->pixmap, 
	       (XITK_WIDGET_LIST_WINDOW(skdloader->widget_list)), lgc, 0, 0,
	       skdloader->preview_width, skdloader->preview_height, x, y);
    XSync(gGui->display, False);
    XFreeGC(gGui->display, lgc);
    XUnlockDisplay (gGui->display);
  }
}

static void download_skin_preview(xitk_widget_t *w, void *data, int selected) {
  download_t   download;

  if(skdloader->slxs[selected]->skin.preview == NULL)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0; 

  download_set_cursor_state(WAIT_CURS);

  if((network_download(skdloader->slxs[selected]->skin.preview, &download))) {
    char          *skpname;
    FILE          *fd;
    char           tmpfile[XITK_PATH_MAX + XITK_NAME_MAX + 1];

    skpname = strrchr(skdloader->slxs[selected]->skin.preview, '/');

    if(*skpname == '/')
      skpname++;
    
    memset(&tmpfile, 0, sizeof(tmpfile));
    sprintf(tmpfile, "/tmp/%d%s", (unsigned int)time(NULL), skpname);
    
    if((fd = fopen(tmpfile, "w+b")) != NULL) {
      ImlibImage    *img = NULL;
      
      fwrite(download.buf, download.size, 1, fd);
      fflush(fd);
      fclose(fd);

      XLockDisplay(gGui->display);
      img = Imlib_load_image(gGui->imlib_data, tmpfile);
      XUnlockDisplay(gGui->display);

      if(img) {
	xitk_image_t *ximg, *oimg = skdloader->preview_image;
	int           owidth, oheight;
	float         ratio;
	
	owidth = skdloader->preview_image->width;
	oheight = skdloader->preview_image->height;
	skdloader->preview_width = img->rgb_width;
	skdloader->preview_height = img->rgb_height;
	
	ratio = ((float)skdloader->preview_width / (float)skdloader->preview_height);

	if(skdloader->preview_width >= skdloader->preview_height) {
	  skdloader->preview_width = owidth;
	  skdloader->preview_height = abs((float)skdloader->preview_width / ratio);
	}
	else {
	  skdloader->preview_height = oheight;
	  skdloader->preview_width = abs((float)skdloader->preview_height / ratio);
	}
	
	/* Rescale preview */
	XLockDisplay(gGui->display);
	Imlib_render(gGui->imlib_data, img, skdloader->preview_width, skdloader->preview_height);
	XUnlockDisplay(gGui->display);
	
	ximg = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
	ximg->width  = owidth;
	ximg->height = oheight;
	
	ximg->image = xitk_image_create_xitk_pixmap(gGui->imlib_data, owidth, oheight);
	ximg->mask = xitk_image_create_xitk_mask_pixmap(gGui->imlib_data, owidth, oheight);
	
	XLockDisplay(gGui->display);
	ximg->image->pixmap = Imlib_copy_image(gGui->imlib_data, img);
	ximg->mask->pixmap = Imlib_copy_mask(gGui->imlib_data, img);
	Imlib_destroy_image(gGui->imlib_data, img);
	XUnlockDisplay(gGui->display);
	
	skdloader->preview_image = ximg;
	
	xitk_image_free_image(gGui->imlib_data, &oimg);

	download_update_preview();
      }
      
      unlink(tmpfile);
    }
  }
  else {
    xine_error(_("Unable to download '%s': %s\n"), 
	       skdloader->slxs[selected]->skin.preview, download.error);
    download_update_blank_preview();
  }

  download_set_cursor_state(NORMAL_CURS);

  if(download.buf)
    free(download.buf);
  if(download.error)
    free(download.error);
}

static void download_skin_select(xitk_widget_t *w, void *data) {
  int          selected = xitk_browser_get_current_selected(skdloader->browser);
  download_t   download;
  
  if(selected < 0)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0; 

  download_set_cursor_state(WAIT_CURS);

  if((network_download(skdloader->slxs[selected]->skin.href, &download))) {
    char *filename;

    filename = strrchr(skdloader->slxs[selected]->skin.href, '/');
    if(strlen(filename) >= 2) {
      struct stat  st;
      char         skindir[XITK_PATH_MAX];
      
      filename++;
      
      memset(&skindir, 0, sizeof(skindir));
      sprintf(skindir, "%s/.xine/skins", xine_get_homedir());
      
      /*
       * Make sure to have the directory
       */
      if(stat(skindir, &st) < 0)
	(void) mkdir_safe(skindir);
      
      if(stat(skindir, &st) < 0) {
	xine_error(_("Unable to create '%s' directory: %s."), skindir, strerror(errno));
      }
      else {
	char   tmpskin[XITK_PATH_MAX + XITK_NAME_MAX + 1];
	FILE  *fd;
	
	memset(&tmpskin, 0, sizeof(tmpskin));
	sprintf(tmpskin, "/tmp/%d%s", (unsigned int)time(NULL), filename);
	
	if((fd = fopen(tmpskin, "w+b")) != NULL) {
	  char   buffer[2048];
	  int    i, skin_found = -1;
	  
	  fwrite(download.buf, download.size, 1, fd);
	  fflush(fd);
	  fclose(fd);

	  memset(&buffer, 0, sizeof(buffer));
	  
	  sprintf(buffer, "`which tar`||tar -C %s -xzf %s", skindir, tmpskin);
	  xine_system(0, buffer);
	  unlink(tmpskin);
	  
	  memset(&buffer, 0, sizeof(buffer));
	  snprintf(buffer, ((strlen(filename) + 1) - 7), "%s", filename);

	  for(i = 0; i < skins_avail_num; i++) {
	    if((!strcmp(skins_avail[i]->pathname, skindir)) 
	       && (!strcmp(skins_avail[i]->skin, buffer))) {
	      skin_found = i;
	      break;
	    }
	  }
	  
	  if(skin_found == -1) {
	    skins_avail = (skins_locations_t **) realloc(skins_avail, 
							 (skins_avail_num + 2) * sizeof(skins_locations_t*));
	    skin_names = (char **) realloc(skin_names, (skins_avail_num + 2) * sizeof(char *));
	    
	    skins_avail[skins_avail_num] = (skins_locations_t *) xine_xmalloc(sizeof(skins_locations_t));
	    skins_avail[skins_avail_num]->pathname = strdup(skindir);
	    skins_avail[skins_avail_num]->skin = strdup(buffer);
	    skins_avail[skins_avail_num]->number = skins_avail_num;
	    
	    skin_names[skins_avail_num] = strdup(skins_avail[skins_avail_num]->skin);
	    
	    skins_avail_num++;
	    
	    skins_avail[skins_avail_num] = NULL;
	    skin_names[skins_avail_num] = NULL;
	    
	    /* Re-register skin enum config, a new one has been added */
	    (void) xine_config_register_enum (gGui->xine, "gui.skin", 
					      (get_skin_offset(DEFAULT_SKIN)),
					      skin_names,
					      _("gui skin theme"), 
					      CONFIG_NO_HELP, 
					      CONFIG_LEVEL_EXP,
					      skin_change_cb, 
					      CONFIG_NO_DATA);
	  }
	  else
	    xine_info(_("Skin %s correctly installed"), buffer);
	  
	  /* Okay, load this skin */
	  select_new_skin((skin_found >= 0) ? skin_found : skins_avail_num - 1);
	}
	else
	  xine_error(_("Unable to create '%s'."), tmpskin);
	
      }
    }
  }
  else
    xine_error(_("Unable to download '%s': %s\n"), 
	       skdloader->slxs[selected]->skin.href, download.error);

  download_set_cursor_state(NORMAL_CURS);
  
  if(download.buf)
    free(download.buf);
  if(download.error)
    free(download.error);
  
  download_skin_cancel(w, NULL);
}

static void download_skin_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {
    
  case KeyPress: {
    XKeyEvent      mykeyevent;
    KeySym         mykey;
    char           kbuf[256];
    int            len;
    int            modifier;
    
    mykeyevent = event->xkey;
    
    xitk_get_key_modifier(event, &modifier);
    
    XLockDisplay(gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay(gGui->display);
    
    switch(mykey) {
      
    case XK_Down:
    case XK_Next: {
      xitk_widget_t *w;
      
      w = xitk_get_focused_widget(skdloader->widget_list);
      if((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
	if(mykey == XK_Down)
	  xitk_browser_step_up(skdloader->browser, NULL);
	else
	  xitk_browser_page_up(skdloader->browser, NULL);
      }
    }
      break;
      
    case XK_Up:
    case XK_Prior: {
      xitk_widget_t *w;
      
      w = xitk_get_focused_widget(skdloader->widget_list);
      if((!w) || (w && (!((xitk_get_widget_type(w)) & WIDGET_GROUP_BROWSER)))) {
	if(mykey == XK_Up)
	  xitk_browser_step_down(skdloader->browser, NULL);
	else
	  xitk_browser_page_down(skdloader->browser, NULL);
      }
    }
      break;
     
    case XK_Escape:
      download_skin_cancel(NULL, NULL);
      break;
    }
  }
    break;

  }
}
    
void download_skin(char *url) {
  slx_entry_t         **slxs;
  xitk_window_t        *xwin;
  xitk_pixmap_t        *bg;
  int                   x, y, w, h, width, height;

  w = 300;
  h = 50;
  XLockDisplay(gGui->display);
  x = (((DisplayWidth(gGui->display, gGui->screen))) >> 1) - (w >> 1);
  y = (((DisplayHeight(gGui->display, gGui->screen))) >> 1) - (h >> 1);
  XUnlockDisplay(gGui->display);
  
  xwin = xitk_window_dialog_button_free_with_width(gGui->imlib_data, _("Be Patient..."), 
  						   w, ALIGN_CENTER, 
  						   _("Retrieve skin list from %s"), url);
  
  while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(xwin)))
    xine_usec_sleep(5000);
  
  if(!gGui->use_root_window) {
    XLockDisplay(gGui->display);
    XSetTransientForHint (gGui->display, 
			  xitk_window_get_window(xwin), gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
  layer_above_video(xitk_window_get_window(xwin));
  
  if((slxs = skins_get_slx_entries(url)) != NULL) {
    char                      *fontname    = "*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
    char                      *btnfontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
    int                        i;
    xitk_browser_widget_t      br;
    xitk_labelbutton_widget_t  lb;
    GC                         gc;
    int                        x, y;

    xitk_window_dialog_destroy(xwin);

    XITK_WIDGET_INIT(&br, gGui->imlib_data);
    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    
#if 0
    for(i = 0; slxs[i]; i++) {
      printf("download skins: Skin number %d:\n", i);
      printf("  Name: %s\n", slxs[i]->name);
      printf("  Author Name: %s\n", slxs[i]->author.name);
      printf("  Author email: %s\n", slxs[i]->author.email);
      printf("  Href: %s\n", slxs[i]->skin.href);
      printf("  Version: %d\n", slxs[i]->skin.version);
      printf("  Maintained: %d\n", slxs[i]->skin.maintained);
      printf("--\n");
    }
#endif

    skdloader = (skin_downloader_t *) xine_xmalloc(sizeof(skin_downloader_t));

    XLockDisplay(gGui->display);
    x = ((DisplayWidth(gGui->display, gGui->screen)) >> 1) - (WINDOW_WIDTH >> 1);
    y = ((DisplayHeight(gGui->display, gGui->screen)) >> 1) - (WINDOW_HEIGHT >> 1);
    XUnlockDisplay(gGui->display);
    
    skdloader->xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
						       _("Choose a skin to download..."),
						       x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    XLockDisplay(gGui->display);
    skdloader->cursor[NORMAL_CURS] = XCreateFontCursor(gGui->display, XC_left_ptr);
    skdloader->cursor[WAIT_CURS] = XCreateFontCursor(gGui->display, XC_watch);
    gc = XCreateGC(gGui->display, 
		   (xitk_window_get_window(skdloader->xwin)), None, None);
    XUnlockDisplay(gGui->display);

    xitk_window_get_window_size(skdloader->xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
    XLockDisplay (gGui->display);
    XCopyArea(gGui->display, (xitk_window_get_background(skdloader->xwin)), bg->pixmap,
	      bg->gc, 0, 0, width, height, 0, 0);
    XUnlockDisplay (gGui->display);

    skdloader->widget_list = xitk_widget_list_new();
    xitk_widget_list_set(skdloader->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
    xitk_widget_list_set(skdloader->widget_list, 
			 WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(skdloader->xwin)));
    xitk_widget_list_set(skdloader->widget_list, WIDGET_LIST_GC, gc);
    
    skdloader->slxs = slxs;
    
    i = 0;
    while(slxs[i]) {

      if(i == 0)
	skdloader->entries = (char **) xine_xmalloc(sizeof(char *) * 2);
      else
	skdloader->entries = (char **) realloc(skdloader->entries, sizeof(char *) * (i + 2));
      
      skdloader->entries[i] = strdup(slxs[i]->name);
      i++;
      skdloader->entries[i] = NULL;
    }
    
    skdloader->num = i;

    skdloader->preview_image = xitk_image_create_image(gGui->imlib_data, WINDOW_WIDTH - 20, 180);
    
    y = WINDOW_HEIGHT - (5 * 20) - (23 + 15 + 15 + 12);

    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = 5;
    br.browser.num_entries           = skdloader->num;
    br.browser.entries               = (const char *const *)skdloader->entries;
    br.callback                      = download_skin_preview;
    br.dbl_click_callback            = NULL;
    br.parent_wlist                  = skdloader->widget_list;
    br.userdata                      = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(skdloader->widget_list)), 
			     (skdloader->browser = 
			      xitk_noskin_browser_create(skdloader->widget_list, &br,
							 (XITK_WIDGET_LIST_GC(skdloader->widget_list)), 
							 10, y, 
							 WINDOW_WIDTH - (20 + 10) , 20, 12, fontname)));
    
    xitk_browser_update_list(skdloader->browser, 
    			     (const char *const *)skdloader->entries, skdloader->num, 0);
    
    
    draw_rectangular_inner_box(gGui->imlib_data, bg, 8, y - 2, (WINDOW_WIDTH - 20) + 5, (5 * 20) + 10 + 5);
    
    xitk_window_change_background(gGui->imlib_data, skdloader->xwin, bg->pixmap, width, height);
    xitk_image_destroy_xitk_pixmap(bg);

    y = WINDOW_HEIGHT - (23 + 15);
    x = 15;

    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Load");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_select; 
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(skdloader->widget_list)), 
			     xitk_noskin_labelbutton_create(skdloader->widget_list, 
							    &lb, x, y, 100, 23,
							    "Black", "Black", "White", btnfontname));
    x = WINDOW_WIDTH - 115;
    
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Cancel");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_cancel; 
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(skdloader->widget_list)), 
			     xitk_noskin_labelbutton_create(skdloader->widget_list, 
							    &lb, x, y, 100, 23,
							    "Black", "Black", "White", btnfontname));

    skdloader->widget_key = xitk_register_event_handler("skdloader", 
							(xitk_window_get_window(skdloader->xwin)),
							download_skin_handle_event,
							NULL,
							NULL,
							skdloader->widget_list,
							NULL);
    
    XLockDisplay(gGui->display);
    XRaiseWindow(gGui->display, xitk_window_get_window(skdloader->xwin));
    XMapWindow(gGui->display, xitk_window_get_window(skdloader->xwin));
    if(!gGui->use_root_window)
      XSetTransientForHint (gGui->display, 
			    xitk_window_get_window(skdloader->xwin), gGui->video_window);
    XUnlockDisplay(gGui->display);
    layer_above_video(xitk_window_get_window(skdloader->xwin));

    while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(skdloader->xwin)))
      xine_usec_sleep(5000);
    
    XLockDisplay (gGui->display);
    XSetInputFocus(gGui->display, xitk_window_get_window(skdloader->xwin), RevertToParent, CurrentTime);
    XUnlockDisplay (gGui->display);

    download_update_blank_preview();
  }
  else
    xitk_window_dialog_destroy(xwin);
  
}
