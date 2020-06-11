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

#include <sys/stat.h>
#include <errno.h>

#include <X11/keysym.h>

#include "common.h"
#include "skins_internal.h"


#define NORMAL_CURS   0
#define WAIT_CURS     1

#define MAX_DISP_ENTRIES  5

#define WINDOW_WIDTH      630
#define WINDOW_HEIGHT     446

#define PREVIEW_WIDTH     (WINDOW_WIDTH - 30)
#define PREVIEW_HEIGHT    220
#define PREVIEW_RATIO     ((float)PREVIEW_WIDTH / (float)PREVIEW_HEIGHT)

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

static struct {
  xitk_window_t        *xwin;
  
  xitk_widget_t        *browser;
  
  xitk_image_t         *preview_image;

  xitk_widget_list_t   *widget_list;
  char                **entries;
  int                   num;
  
  slx_entry_t         **slxs;

  xitk_register_key_t   widget_key;

  int                   running;
} skdloader;

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
    
    xml_parser_init_R(xml_parser_t *xml, download.buf, download.size, XML_PARSER_CASE_INSENSITIVE);
    if((result = xml_parser_build_tree_R(xml, &xml_tree)) != XML_PARSER_OK)
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
		
		slxs = (slx_entry_t **) realloc(slxs, sizeof(slx_entry_t *) * (entries_slx + 2));
		
		slxs[entries_slx] = (slx_entry_t *) calloc(1, sizeof(slx_entry_t));
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
    xml_parser_finalize_R(xml);

    if(entries_slx)
      slxs[entries_slx] = NULL;

  }
  else
    xine_error (gGui, _("Unable to retrieve skin list from %s: %s"), url, download.error);
 
 __failure:
  
  free(download.buf);
  free(download.error);
  
  return slxs;
}

/*
 * Remote skin loader
 */
static void download_set_cursor_state(int state) {
  if(state == WAIT_CURS)
    xitk_window_define_window_cursor(skdloader.xwin, xitk_cursor_watch);
  else
    xitk_window_restore_window_cursor(skdloader.xwin);
}

static void download_skin_exit(xitk_widget_t *w, void *data) {
  int i;
  
  if(skdloader.running) {
    xitk_unregister_event_handler(&skdloader.widget_key);
    
    if(skdloader.preview_image)
      xitk_image_free_image(&skdloader.preview_image);

    xitk_window_destroy_window(skdloader.xwin);
    skdloader.xwin = NULL;
    /* xitk_dlist_init (&skdloader.widget_list->list); */

    for(i = 0; i < skdloader.num; i++) {
      SAFE_FREE(skdloader.slxs[i]->name);
      SAFE_FREE(skdloader.slxs[i]->author.name);
      SAFE_FREE(skdloader.slxs[i]->author.email);
      SAFE_FREE(skdloader.slxs[i]->skin.href);
      SAFE_FREE(skdloader.slxs[i]->skin.preview);
      free(skdloader.slxs[i]);
      free(skdloader.entries[i]);
    }
    
    free(skdloader.slxs);
    free(skdloader.entries);
    
    skdloader.num = 0;

    skdloader.running = 0;

    video_window_set_input_focus(gGui->vwin);
  }
}

static void download_update_blank_preview(void) {

  xitk_image_t *p = xitk_image_create_image(gGui->imlib_data, PREVIEW_WIDTH, PREVIEW_HEIGHT);
  pixmap_fill_rectangle(p->image, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT,
                        xitk_get_pixel_color_from_rgb(gGui->imlib_data, 52, 52, 52));
  xitk_image_draw_image (skdloader.widget_list, p,
                         0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, 15, 34);
  xitk_image_free_image (&p);
}

static void download_update_preview(xitk_image_t *p) {

  download_update_blank_preview();

  if (p && p->image) {
    xitk_image_draw_image (skdloader.widget_list, p,
                           0, 0, p->width, p->height,
                           15 + ((PREVIEW_WIDTH - p->width) >> 1),
                           34 + ((PREVIEW_HEIGHT - p->height) >> 1));
  }
}

static void download_skin_preview(xitk_widget_t *w, void *data, int selected) {
  gGui_t *gui = gGui;
  download_t   download;

  if(skdloader.slxs[selected]->skin.preview == NULL)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0; 

  download_set_cursor_state(WAIT_CURS);

  if((network_download(skdloader.slxs[selected]->skin.preview, &download))) {
      ImlibImage    *img = NULL;

      gui->x_lock_display (gui->display);
      img = Imlib_decode_image (gui->imlib_data, download.buf, download.size);
      gui->x_unlock_display (gui->display);

      if(img) {
	xitk_image_t *ximg, *oimg = skdloader.preview_image;
	int           preview_width, preview_height;
	float         ratio;
	
	preview_width = img->rgb_width;
	preview_height = img->rgb_height;
	
	ratio = ((float)preview_width / (float)preview_height);

	if(ratio > PREVIEW_RATIO) {
	  if(preview_width > PREVIEW_WIDTH) {
	    preview_width = PREVIEW_WIDTH;
	    preview_height = (float)preview_width / ratio;
	  }
	}
	else {
	  if(preview_height > PREVIEW_HEIGHT) {
	    preview_height = PREVIEW_HEIGHT;
	    preview_width = (float)preview_height * ratio;
	  }
	}
	
	/* Rescale preview */
        gui->x_lock_display (gui->display);
        Imlib_render (gui->imlib_data, img, preview_width, preview_height);
        gui->x_unlock_display (gui->display);
	
	ximg = (xitk_image_t *) xitk_xmalloc(sizeof(xitk_image_t));
	ximg->width  = preview_width;
	ximg->height = preview_height;
	
        ximg->image = xitk_image_create_xitk_pixmap (gui->imlib_data, preview_width, preview_height);
        ximg->mask = xitk_image_create_xitk_mask_pixmap (gui->imlib_data, preview_width, preview_height);

        gui->x_lock_display (gui->display);
        ximg->image->pixmap = Imlib_copy_image (gui->imlib_data, img);
        ximg->mask->pixmap = Imlib_copy_mask (gui->imlib_data, img);
        Imlib_destroy_image (gui->imlib_data, img);
        gui->x_unlock_display (gui->display);

	skdloader.preview_image = ximg;

	if(oimg)
          xitk_image_free_image (&oimg);

	download_update_preview(skdloader.preview_image);
      }
  }
  else {
    xine_error (gui, _("Unable to download '%s': %s"),
      skdloader.slxs[selected]->skin.preview, download.error);
    download_update_blank_preview();
  }

  download_set_cursor_state(NORMAL_CURS);

  free(download.buf);
  free(download.error);
}

static void download_skin_select(xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;
  int          selected = xitk_browser_get_current_selected(skdloader.browser);
  download_t   download;
  
  if(selected < 0)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0; 

  download_set_cursor_state(WAIT_CURS);

  if((network_download(skdloader.slxs[selected]->skin.href, &download))) {
    char *filename;

    filename = strrchr(skdloader.slxs[selected]->skin.href, '/');

    if(filename)
      filename++;
    else
      filename = skdloader.slxs[selected]->skin.href;

    if(filename[0]) {
      struct stat  st;
      char         skindir[XITK_PATH_MAX + 1];
      
      snprintf(skindir, sizeof(skindir), "%s%s", xine_get_homedir(), "/.xine/skins");
      
      /*
       * Make sure to have the directory
       */
      if(stat(skindir, &st) < 0)
	(void) mkdir_safe(skindir);
      
      if(stat(skindir, &st) < 0) {
        xine_error (gui, _("Unable to create '%s' directory: %s."), skindir, strerror(errno));
      }
      else {
	char   tmpskin[XITK_PATH_MAX + XITK_NAME_MAX + 2];
	FILE  *fd;
	
        snprintf(tmpskin, sizeof(tmpskin), "%s%u%s", "/tmp/", (unsigned int)time(NULL), filename);
	
	if((fd = fopen(tmpskin, "w+b")) != NULL) {
	  char      buffer[2048];
          char     *cmd;
	  char     *fskin_path;
	  int       i, skin_found = -1, len;

	  fwrite(download.buf, download.size, 1, fd);
	  fflush(fd);
	  fclose(fd);

          cmd = xitk_asprintf("%s %s %s %s %s", "which tar > /dev/null 2>&1 && cd ", skindir, " && gunzip -c ", tmpskin, " | tar xf -");
          if (cmd)
            xine_system(0, cmd);
          free(cmd);
	  unlink(tmpskin);

	  len = strlen(filename) - strlen(".tar.gz");
	  if (len > sizeof(buffer) - 1) len = sizeof(buffer) - 1;
	  memcpy(buffer, filename, len);
	  buffer[len] = '\0';

          fskin_path = xitk_asprintf("%s/%s/%s", skindir, buffer, "doinst.sh");
          if (fskin_path && is_a_file(fskin_path)) {
	    char *doinst;

	    doinst = xitk_asprintf("%s %s/%s %s", "cd", skindir, buffer, "&& ./doinst.sh");
            if (doinst)
              xine_system(0, doinst);
            free(doinst);
	  }
          free(fskin_path);
	  
	  for(i = 0; i < skins_avail_num; i++) {
	    if((!strcmp(skins_avail[i]->pathname, skindir)) 
	       && (!strcmp(skins_avail[i]->skin, buffer))) {
	      skin_found = i;
	      break;
	    }
	  }
	  
	  if(skin_found == -1) {
	    skins_avail = (skins_locations_t **) realloc(skins_avail, (skins_avail_num + 2) * sizeof(skins_locations_t*));
	    skin_names = (char **) realloc(skin_names, (skins_avail_num + 2) * sizeof(char *));
	    
	    skins_avail[skins_avail_num] = (skins_locations_t *) calloc(1, sizeof(skins_locations_t));
	    skins_avail[skins_avail_num]->pathname = strdup(skindir);
	    skins_avail[skins_avail_num]->skin = strdup(buffer);
	    skins_avail[skins_avail_num]->number = skins_avail_num;
	    
	    skin_names[skins_avail_num] = strdup(skins_avail[skins_avail_num]->skin);
	    
	    skins_avail_num++;
	    
	    skins_avail[skins_avail_num] = NULL;
	    skin_names[skins_avail_num] = NULL;
	    
	    /* Re-register skin enum config, a new one has been added */
	    (void) xine_config_register_enum (__xineui_global_xine_instance, "gui.skin", 
					      (get_skin_offset(DEFAULT_SKIN)),
					      skin_names,
					      _("gui skin theme"), 
					      CONFIG_NO_HELP, 
					      CONFIG_LEVEL_EXP,
					      skin_change_cb, 
					      CONFIG_NO_DATA);
	  }
	  else
            xine_info (gui, _("Skin %s correctly installed"), buffer);

	  /* Okay, load this skin */
	  select_new_skin((skin_found >= 0) ? skin_found : skins_avail_num - 1);
	}
	else
          xine_error (gui, _("Unable to create '%s'."), tmpskin);

      }
    }
  }
  else
    xine_error (gui, _("Unable to download '%s': %s"),
      skdloader.slxs[selected]->skin.href, download.error);

  download_set_cursor_state(NORMAL_CURS);
  
  free(download.buf);
  free(download.error);
  
  download_skin_exit(w, NULL);
}

static void download_skin_handle_event(XEvent *event, void *data) {
  
  switch(event->type) {

  case Expose:
    if(event->xexpose.count == 0)
      download_update_preview(skdloader.preview_image);
    break;

  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      download_skin_exit(NULL, NULL);
    else
      gui_handle_event (event, gGui);
    break;

  }
}

void download_skin_end(void) {
  download_skin_exit(NULL, NULL);
}
    
void download_skin(char *url) {
  gGui_t *gui = gGui;
  slx_entry_t         **slxs;
  xitk_pixmap_t        *bg;
  xitk_widget_t        *widget;
  xitk_register_key_t   dialog;

  if(skdloader.running)
  {
    xitk_window_raise_window(skdloader.xwin);
    xitk_window_try_to_set_input_focus(skdloader.xwin);
    return;
  }

  dialog = xitk_window_dialog_3 (gui->imlib_data,
    NULL,
    get_layer_above_video (gui), 400, _("Be patient..."), NULL, NULL,
    NULL, NULL, NULL, NULL, 0, ALIGN_CENTER, _("Retrieving skin list from %s"), url);
  video_window_set_transient_for(gui->vwin, xitk_get_window(dialog));

  if((slxs = skins_get_slx_entries(url)) != NULL) {
    int                        i;
    xitk_browser_widget_t      br;
    xitk_labelbutton_widget_t  lb;
    int                        x, y;

    xitk_unregister_event_handler (&dialog);

    XITK_WIDGET_INIT (&br);
    XITK_WIDGET_INIT (&lb);
    
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

    skdloader.running = 1;

    x = (xitk_get_display_width()  >> 1) - (WINDOW_WIDTH >> 1);
    y = (xitk_get_display_height() >> 1) - (WINDOW_HEIGHT >> 1);

    skdloader.xwin = xitk_window_create_dialog_window (gui->imlib_data, 
						       _("Choose a skin to download..."),
						       x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    set_window_states_start(skdloader.xwin);

    bg = xitk_window_get_background_pixmap (skdloader.xwin);

    skdloader.widget_list = xitk_window_widget_list(skdloader.xwin);

    skdloader.slxs = slxs;
    
    i = 0;
    while(slxs[i]) {

      skdloader.entries = (char **) realloc(skdloader.entries, sizeof(char *) * (i + 2));
      
      skdloader.entries[i] = strdup(slxs[i]->name);
      i++;
    }
    skdloader.entries[i] = NULL;
    
    skdloader.num = i;

    skdloader.preview_image = NULL;
    
    x = 15;
    y = 34 + PREVIEW_HEIGHT + 14;

    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
    br.browser.num_entries           = skdloader.num;
    br.browser.entries               = (const char *const *)skdloader.entries;
    br.callback                      = download_skin_preview;
    br.dbl_click_callback            = NULL;
    br.parent_wlist                  = skdloader.widget_list;
    br.userdata                      = NULL;
    skdloader.browser = xitk_noskin_browser_create (skdloader.widget_list, &br,
      (XITK_WIDGET_LIST_GC(skdloader.widget_list)), x + 5, y + 5, WINDOW_WIDTH - (30 + 10 + 16), 20, 16, br_fontname);
    xitk_add_widget (skdloader.widget_list, skdloader.browser);

    xitk_browser_update_list(skdloader.browser, 
    			     (const char *const *)skdloader.entries, NULL, skdloader.num, 0);
    
    xitk_enable_and_show_widget(skdloader.browser);
    
    draw_rectangular_inner_box (bg, x, y,
      (WINDOW_WIDTH - 30 - 1), (MAX_DISP_ENTRIES * 20 + 16 + 10 - 1));

    xitk_window_set_background (skdloader.xwin, bg);

    y = WINDOW_HEIGHT - (23 + 15);
    x = 15;

    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Load");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_select; 
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    widget =  xitk_noskin_labelbutton_create (skdloader.widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (skdloader.widget_list, widget);
    xitk_enable_and_show_widget(widget);

    x = WINDOW_WIDTH - (100 + 15);
    
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Cancel");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_exit; 
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    widget =  xitk_noskin_labelbutton_create (skdloader.widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (skdloader.widget_list, widget);
    xitk_enable_and_show_widget(widget);
    
    skdloader.widget_key = xitk_register_event_handler("skdloader",
                                                       skdloader.xwin,
							download_skin_handle_event,
							NULL,
							NULL,
							skdloader.widget_list,
							NULL);
    
    xitk_window_show_window(skdloader.xwin, 1);
    video_window_set_transient_for (gui->vwin, skdloader.xwin);
    layer_above_video(skdloader.xwin);

    xitk_window_try_to_set_input_focus(skdloader.xwin);
    download_update_blank_preview();
  }
  else
    xitk_unregister_event_handler (&dialog);
}
