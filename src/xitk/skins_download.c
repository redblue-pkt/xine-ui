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

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <X11/keysym.h>

#include "common.h"
#include "skins.h"
#include "download.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/browser.h"

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

struct xui_skdloader_s {
  gGui_t               *gui;
  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;
  xitk_widget_t        *browser;
  xitk_image_t         *preview_image;

  char                **entries;
  int                   num;
  slx_entry_t         **slxs;

  xitk_register_key_t   widget_key;
};

static slx_entry_t **skins_get_slx_entries(gGui_t *gui, char *url) {
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
    xine_error (gui, _("Unable to retrieve skin list from %s: %s"), url, download.error);

 __failure:

  free(download.buf);
  free(download.error);

  return slxs;
}

/*
 * Remote skin loader
 */
static void download_set_cursor_state(xui_skdloader_t *skd, int state) {
  if(state == WAIT_CURS)
    xitk_window_define_window_cursor(skd->xwin, xitk_cursor_watch);
  else
    xitk_window_restore_window_cursor(skd->xwin);
}

static void download_skin_exit (xitk_widget_t *w, void *data, int state) {
  xui_skdloader_t *skd = data;

  (void)w;
  (void)state;
  if (skd) {
    gGui_t *gui = skd->gui;
    int i;

    gui->skdloader = NULL;

    xitk_unregister_event_handler(&skd->widget_key);

    if(skd->preview_image)
      xitk_image_free_image(&skd->preview_image);

    xitk_window_destroy_window(skd->xwin);
    skd->xwin = NULL;
    /* xitk_dlist_init (&skdloader.widget_list->list); */

    for(i = 0; i < skd->num; i++) {
      SAFE_FREE(skd->slxs[i]->name);
      SAFE_FREE(skd->slxs[i]->author.name);
      SAFE_FREE(skd->slxs[i]->author.email);
      SAFE_FREE(skd->slxs[i]->skin.href);
      SAFE_FREE(skd->slxs[i]->skin.preview);
      free(skd->slxs[i]);
      free(skd->entries[i]);
    }

    free(skd->slxs);
    free(skd->entries);

    skd->num = 0;

    free(skd);

    video_window_set_input_focus(gui->vwin);
  }
}

static void download_update_blank_preview(xui_skdloader_t *skd) {

  xitk_image_t *p = xitk_image_create_image(skd->gui->xitk, PREVIEW_WIDTH, PREVIEW_HEIGHT);
  xitk_image_fill_rectangle (p, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT,
    xitk_color_db_get (skd->gui->xitk, (52 << 16) + (52 << 8) + 52));
  xitk_image_draw_image (skd->widget_list, p, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, 15, 34, 0);
  xitk_image_free_image (&p);
}

static void download_update_preview(xui_skdloader_t *skd) {
  download_update_blank_preview(skd);

  if (skd->preview_image) {
    int img_width = xitk_image_width(skd->preview_image);
    int img_height = xitk_image_height(skd->preview_image);
    xitk_image_draw_image (skd->widget_list, skd->preview_image,
        0, 0, img_width, img_height,
        15 + ((PREVIEW_WIDTH - img_width) >> 1), 34 + ((PREVIEW_HEIGHT - img_height) >> 1), 0);
  }
}

static xitk_image_t *decode(gGui_t *gui, const void *buf, int size) {
  int            preview_width, preview_height;
  float          ratio;
  xitk_image_t  *i;

  i = xitk_image_decode_raw(gui->xitk, buf, size);
  if (!i)
    return NULL;

  preview_width  = xitk_image_width(i);
  preview_height = xitk_image_height(i);
  ratio = ((float)preview_width / (float)preview_height);

  if (ratio > PREVIEW_RATIO) {
    if (preview_width > PREVIEW_WIDTH) {
      preview_width = PREVIEW_WIDTH;
      preview_height = (float)preview_width / ratio;
    }
  } else {
    if (preview_height > PREVIEW_HEIGHT) {
      preview_height = PREVIEW_HEIGHT;
      preview_width = (float)preview_height * ratio;
    }
  }

  /* Rescale preview */
  if (xitk_image_render(i, preview_width, preview_height) < 0)
    xitk_image_free_image(&i);

  return i;
}

static void download_skin_preview(xitk_widget_t *w, void *data, int selected, int modifier) {
  xui_skdloader_t *skd = data;
  gGui_t          *gui = skd->gui;
  download_t       download;

  (void)w;
  (void)modifier;
  if(skd->slxs[selected]->skin.preview == NULL)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0;

  download_set_cursor_state(skd, WAIT_CURS);

  if((network_download(skd->slxs[selected]->skin.preview, &download))) {
    xitk_image_t *ximg;

    ximg = decode(gui, download.buf, download.size);
    if (ximg) {
      xitk_image_t *oimg = skd->preview_image;
      skd->preview_image = ximg;
      if(oimg)
        xitk_image_free_image (&oimg);
    }

    download_update_preview(skd);
  }
  else {
    xine_error (gui, _("Unable to download '%s': %s"),
      skd->slxs[selected]->skin.preview, download.error);
    download_update_blank_preview(skd);
  }

  download_set_cursor_state(skd, NORMAL_CURS);

  free(download.buf);
  free(download.error);
}

static void download_skin_select (xitk_widget_t *w, void *data, int state) {
  xui_skdloader_t *skd = data;
  gGui_t      *gui = skd->gui;
  int          selected = xitk_browser_get_current_selected(skd->browser);
  download_t   download;

  (void)state;
  if(selected < 0)
    return;

  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0;

  download_set_cursor_state(skd, WAIT_CURS);

  do {
    char skindir[2048], *skinname, *skinend, *end = skindir + sizeof (skindir) - 8;
    char *filename;
    struct stat  st;

    if (!network_download (skd->slxs[selected]->skin.href, &download)) {
      xine_error (gui, _("Unable to download '%s': %s"), skd->slxs[selected]->skin.href, download.error);
      break;
    }

    filename = strrchr (skd->slxs[selected]->skin.href, '/');
    if (filename)
      filename++;
    else
      filename = skd->slxs[selected]->skin.href;
    if (!filename[0])
      break;
    if (strlen (filename) <= strlen (".tar.gz"))
      break;

    skinname = skindir + strlcpy (skindir, xine_get_homedir (), end - skindir);
    if (skinname >= end)
      skinname = end;
    skinname += strlcpy (skinname, "/.xine/skins", end - skinname);
    if (skinname >= end)
      skinname = end;
    /* Make sure to have the directory */
    if (stat (skindir, &st) < 0) {
      (void)mkdir_safe (skindir);
      if (stat (skindir, &st) < 0) {
        xine_error (gui, _("Unable to create '%s' directory: %s."), skindir, strerror(errno));
        break;
      }
    }

    {
      char  tmpskin[XITK_PATH_MAX + XITK_NAME_MAX + 2], *cmd;
      FILE *fd;
      snprintf (tmpskin, sizeof (tmpskin), "%s%u%s", "/tmp/", (unsigned int)time (NULL), filename);
      if (!(fd = fopen (tmpskin, "w+b"))) {
        xine_error (gui, _("Unable to create '%s'."), tmpskin);
        break;
      }
      fwrite (download.buf, download.size, 1, fd);
      fflush (fd);
      fclose (fd);
      cmd = xitk_asprintf ("%s %s %s %s %s",
        "which tar > /dev/null 2>&1 && cd ", skindir, " && gunzip -c ", tmpskin, " | tar xf -");
      if (cmd) {
        xine_system (0, cmd);
        free (cmd);
      }
      unlink (tmpskin);
    }

    *skinname++ = '/';
    {
      size_t len = strlen (filename) - strlen (".tar.gz");
      if (len > (size_t)(end - skinname))
        len = end - skinname;
      memcpy (skinname, filename, len);
      skinend = skinname + len;
      *skinend = 0;
    }

    strlcpy (skinend, "/doinst.sh", end - skinend);
    if (is_a_file (skindir)) {
      char *doinst;
      *skinend = 0;
      doinst = xitk_asprintf ("cd %s && ./doinst.sh", skindir);
      if (doinst) {
        xine_system (0, doinst);
        free (doinst);
      }
    } else {
      *skinend = 0;
    }

    {
      int r = skin_add_1 (gui, skindir, skinname, skinend);
      if (r >= 0) {
        xine_info (gui, _("Skin %s correctly installed"), skinname);
        /* Okay, load this skin */
        skin_select (gui, r);
      }
    }
  } while (0);

  download_set_cursor_state(skd, NORMAL_CURS);

  free(download.buf);
  free(download.error);

  download_skin_exit (w, skd, 0);
}

static void download_skin_handle_expose_event(void *data, const xitk_expose_event_t *ee) {
  xui_skdloader_t *skd = data;

  if (ee->expose_count == 0)
    download_update_preview(skd);
}

static void download_skin_handle_key_event(void *data, const xitk_key_event_t *ke) {
  xui_skdloader_t *skd = data;

  if (ke->event == XITK_KEY_PRESS) {
    if (ke->key_pressed == XK_Escape)
      download_skin_exit (NULL, skd, 0);
    else
      gui_handle_key_event (skd->gui, ke);
  }
}

static const xitk_event_cbs_t download_skin_event_cbs = {
  .key_cb            = download_skin_handle_key_event,
  .expose_notify_cb  = download_skin_handle_expose_event,
};


void skin_download_end (xui_skdloader_t *skd) {
  if (skd)
    download_skin_exit (NULL, skd, 0);
}

void skin_download (gGui_t *gui, char *url) {
  slx_entry_t         **slxs;
  xitk_pixmap_t        *bg;
  xitk_widget_t        *widget;
  xui_skdloader_t      *skd;
  xitk_register_key_t   downloading_key;

  if (gui->skdloader) {
    xitk_window_t *win = gui->skdloader->xwin;
    xitk_window_raise_window(win);
    xitk_window_try_to_set_input_focus(win);
    return;
  }

  downloading_key = xitk_window_dialog_3 (gui->xitk,
    NULL,
    get_layer_above_video (gui), 400, _("Be patient..."), NULL, NULL,
    NULL, NULL, NULL, NULL, 0, ALIGN_CENTER, _("Retrieving skin list from %s"), url);
  video_window_set_transient_for(gui->vwin, xitk_get_window(downloading_key));

  slxs = skins_get_slx_entries(gui, url);

  xitk_unregister_event_handler (&downloading_key);

  if (slxs) {
    int                        i;
    xitk_browser_widget_t      br;
    xitk_labelbutton_widget_t  lb;
    int                        x, y;

    skd = calloc(1, sizeof(*skd));
    skd->gui  = gui;
    skd->slxs = slxs;
    for (i = 0; slxs[i]; i++) {
      skd->entries = (char **) realloc(skd->entries, sizeof(char *) * (i + 2));
      skd->entries[i] = strdup(slxs[i]->name);
    }
    skd->entries[i] = NULL;
    skd->num = i;
    skd->preview_image = NULL;

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

    xitk_get_display_size (skd->gui->xitk, &x, &y);
    x = (x >> 1) - (WINDOW_WIDTH >> 1);
    y = (y >> 1) - (WINDOW_HEIGHT >> 1);

    skd->xwin = xitk_window_create_dialog_window (gui->xitk,
                                                  _("Choose a skin to download..."),
                                                  x, y, WINDOW_WIDTH, WINDOW_HEIGHT);

    set_window_states_start(gui, skd->xwin);
    bg = xitk_window_get_background_pixmap (skd->xwin);
    skd->widget_list = xitk_window_widget_list(skd->xwin);

    x = 15;
    y = 34 + PREVIEW_HEIGHT + 14;

    XITK_WIDGET_INIT (&br);
    XITK_WIDGET_INIT (&lb);

    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
    br.browser.num_entries           = skd->num;
    br.browser.entries               = (const char *const *)skd->entries;
    br.callback                      = download_skin_preview;
    br.dbl_click_callback            = NULL;
    br.userdata                      = skd;
    skd->browser = xitk_noskin_browser_create (skd->widget_list, &br,
      x + 5, y + 5, WINDOW_WIDTH - (30 + 10 + 16), 20, 16, br_fontname);
    xitk_add_widget (skd->widget_list, skd->browser);

    xitk_browser_update_list(skd->browser,
                             (const char *const *)skd->entries, NULL, skd->num, 0);

    xitk_enable_and_show_widget(skd->browser);

    draw_rectangular_box (bg, x, y, WINDOW_WIDTH - 30, MAX_DISP_ENTRIES * 20 + 16 + 10, DRAW_INNER);

    xitk_window_set_background (skd->xwin, bg);

    y = WINDOW_HEIGHT - (23 + 15);
    x = 15;

    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Load");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_select;
    lb.state_callback    = NULL;
    lb.userdata          = skd;
    lb.skin_element_name = NULL;
    widget =  xitk_noskin_labelbutton_create (skd->widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (skd->widget_list, widget);
    xitk_enable_and_show_widget(widget);

    x = WINDOW_WIDTH - (100 + 15);

    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Cancel");
    lb.align             = ALIGN_CENTER;
    lb.callback          = download_skin_exit;
    lb.state_callback    = NULL;
    lb.userdata          = skd;
    lb.skin_element_name = NULL;
    widget =  xitk_noskin_labelbutton_create (skd->widget_list,
      &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
    xitk_add_widget (skd->widget_list, widget);
    xitk_enable_and_show_widget(widget);

    skd->widget_key = xitk_window_register_event_handler("skdloader", skd->xwin, &download_skin_event_cbs, skd);

    xitk_window_show_window(skd->xwin, 1);
    video_window_set_transient_for (gui->vwin, skd->xwin);
    layer_above_video (skd->gui, skd->xwin);

    xitk_window_try_to_set_input_focus(skd->xwin);
    download_update_blank_preview(skd);

    gui->skdloader = skd;
  }
}

