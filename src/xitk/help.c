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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#include "xine-toolkit/recode.h"

#include "common.h"
#include "help.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/tabs.h"
#include "xine-toolkit/browser.h"

#include "lang.h"

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_CODESET)
#  include <iconv.h>
#  include <langinfo.h>
#  define USE_CONV
#endif


#define WINDOW_WIDTH             632
#define WINDOW_HEIGHT            473

#define MAX_SECTIONS             50

#define MAX_DISP_ENTRIES         18

typedef struct {
  char                 *name;
  char                **content;
  int                   line_num;
  int                   order_num;
} help_section_t;

struct xui_help_s {
  gGui_t               *gui;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   visible;

  xitk_widget_t        *tabs;
  xitk_widget_t        *ok;
  xitk_widget_t        *browser;

  char                 *tab_sections[MAX_SECTIONS + 1];

  help_section_t       *sections[MAX_SECTIONS + 1];
  int                   num_sections;
  int                   tabs_height;

  xitk_register_key_t   kreg;

};

static void _help_unload_string_list (char **list) {
  if (list) {
    free (list[0]);
    free (list);
  }
}

static char **_help_load_text_as_string_list (const char *filename, uint32_t *num_lines, xitk_recode_t *xr) {
  char **list, *p, **rlist, *q;
  uint32_t *bv, *pv, *ev;
  uint32_t have, used, fsize, l;
  FILE *f;

  if (!filename)
    return NULL;
  if (!filename[0])
    return NULL;

  f = fopen (filename, "rb");
  if (!f)
    return NULL;
  if (fseek (f, 0, SEEK_END)) {
    fclose (f);
    return NULL;
  }
  fsize = ftell (f);
  if (fseek (f, 0, SEEK_SET)) {
    fclose (f);
    return NULL;
  }

  have = 256;
  used = 1;
  list = malloc (have * sizeof (*list));
  if (!list) {
    fclose (f);
    return NULL;
  }
  if (fsize > 2 << 20)
    fsize = 2 << 20;
  bv = (uint32_t *)malloc ((fsize + 7) & ~3);
  if (!bv) {
    free (list);
    fclose (f);
    return NULL;
  }
  list[0] = p = (char *)bv;
  fsize = fread (p, 1, fsize, f);
  fclose (f);
  memset (p + fsize, 0x0a, (~fsize & 3) + 4);

  ev = bv + ((fsize + 3) >> 2);
  for (pv = bv; pv < ev; pv++) {
    union {uint32_t v; char b[4];} u;
    uint32_t v;
    while (1) {
      v = *pv ^ ~0x0a0a0a0a;
      v = (v & 0x80808080) & ((v & 0x7f7f7f7f) + 0x01010101);
      if (v)
        break;
      pv++;
    }
    if (used + 5 > have) {
      char **new = realloc (list, (have + 256) * sizeof (*list));
      if (!new)
        break;
      list = new;
      have += 256;
    }
    p = (char *)pv;
    u.v = v;
    if (u.b[0]) {
      p[0] = 0;
      list[used++] = p + 1;
      if ((p > (char *)bv) && (p[-1] == '\r'))
        p[-1] = 0;
    }
    if (u.b[1]) {
      p[1] = 0;
      list[used++] = p + 2;
      if (p[0] == '\r')
        p[0] = 0;
    }
    if (u.b[2]) {
      p[2] = 0;
      list[used++] = p + 3;
      if (p[1] == '\r')
        p[1] = 0;
    }
    if (u.b[3]) {
      p[3] = 0;
      list[used++] = p + 4;
      if (p[2] == '\r')
        p[2] = 0;
    }
  }
  while (used && (list[used - 1] >= list[0] + fsize))
    used--;
  *num_lines = used;

  if (!xr || !used) {
    list[used] = NULL;
    return list;
  }

  rlist = malloc ((used + 1) * sizeof (*rlist));
  if (!rlist) {
    list[used] = NULL;
    return list;
  }
  q = malloc (2 * ((fsize + 7) & ~3));
  if (!q) {
    free (rlist);
    list[used] = NULL;
    return list;
  }
  p = q + 2 * fsize;
  for (l = 0; l < used; l++) {
    xitk_recode_string_t rr = {
      .src = list[l],
      .ssize = list[l + 1] - list[l] - 1,
      .buf = rlist[l] = q,
      .bsize = p - q,
      .res = NULL,
      .rsize = 0
    };
    xitk_recode2_do (xr, &rr);
    if (rr.res != q) {
      /* no recoding done/needed. */
      free (rlist[0]);
      free (rlist);
      list[used] = NULL;
      return list;
    }
    q += rr.rsize;
    *q++ = 0;
  }
  rlist[used] = NULL;
  _help_unload_string_list (list);
  return rlist;
}

static void help_change_section (xitk_widget_t *w, void *data, int section) {
  xui_help_t *help = data;

  (void)w;
  if (section < help->num_sections)
    xitk_browser_update_list (help->browser, (const char * const *)help->sections[section]->content, NULL,
      help->sections[section]->line_num, 0);
}

static void help_add_section (xui_help_t *help, const char *filename, const char *doc_encoding,
  int order_num, char *section_name) {

  /* ensure that the file is not empty */
  if (help->num_sections < MAX_SECTIONS) {
    xitk_recode_t *xr = xitk_recode_init (doc_encoding, NULL, 0);
    uint32_t num_lines = 0;
    char **rlist = _help_load_text_as_string_list (filename, &num_lines, xr);

    xitk_recode_done (xr);
    if (rlist && num_lines) {
      help_section_t *section = (help_section_t *)calloc (1, sizeof (help_section_t));

      if (section) {
        section->name = strdup ((section_name && section_name[0]) ? section_name : _("Undefined"));
        section->content = rlist;
        section->line_num = num_lines;
        section->order_num = order_num;
        rlist = NULL;

        help->sections[help->num_sections++] = section;
        help->sections[help->num_sections]   = NULL;
      }
    }
    _help_unload_string_list (rlist);
  }
}

static void help_sections (xui_help_t *help) {
  DIR                 *dir;
  int                  i;

  if ((dir = opendir(XINE_DOCDIR)) == NULL) {
    gui_msg (help->gui, XUI_MSG_ERROR, "Cannot open help files: %s", strerror(errno));
  }
  else {
    struct dirent       *dir_entry;
    const langs_t       *lang = get_lang();
    char                 locale_file[XITK_NAME_MAX + 18];
    char                 locale_readme[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    char                 default_readme[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    char                 ending[XITK_NAME_MAX + 2];
    char                 section_name[1024];

    while ((dir_entry = readdir(dir)) != NULL) {
      int order_num;

      memset(&ending, 0, sizeof(ending));
      memset(&section_name, 0, sizeof(section_name));

      if ((sscanf(dir_entry->d_name, "README.en.%d.%255s", &order_num, &section_name[0])) == 2) {
        sscanf(dir_entry->d_name, "README.en.%255s", &ending[0]);

	snprintf(locale_file, sizeof(locale_file), "%s.%s.%s", "README", lang->ext, ending);
	snprintf(locale_readme, sizeof(locale_readme), "%s/%s", XINE_DOCDIR, locale_file);
	snprintf(default_readme, sizeof(default_readme), "%s/%s", XINE_DOCDIR, dir_entry->d_name);

	if ((strcmp(locale_file, dir_entry->d_name)) && is_a_file(locale_readme)) {
          help_add_section (help, locale_readme, lang->doc_encoding, order_num, section_name);
	}
	else {
          help_add_section (help, default_readme, "ISO-8859-1", order_num, section_name);
	}
      }
    }

    closedir(dir);

    if(help->num_sections) {

      /* Sort sections */
      for(i = 0; i < help->num_sections; i++) {
	int j;
	for(j = 0; j < help->num_sections; j++) {
	  if(help->sections[i]->order_num < help->sections[j]->order_num) {
	    help_section_t *hsec = help->sections[i];
	    help->sections[i]    = help->sections[j];
	    help->sections[j]    = hsec;
	  }
	}
      }

      /* create tabs section names */
      for(i = 0; i < help->num_sections; i++) {
	char *p;

	p = help->tab_sections[i] = strdup(help->sections[i]->name);

	/* substitute underscores by spaces (nicer) */
	while(*p) {
	  if(*p == '_')
	    *p = ' ';
	  p++;
	}

	help->tab_sections[i + 1] = NULL;
      }
    }
  }
}

static void help_exit (xitk_widget_t *w, void *data, int state) {
  xui_help_t *help = data;

  (void)w;
  (void)state;
  if(help) {
    help->visible = 0;

    gui_save_window_pos (help->gui, "help", help->kreg);

    xitk_unregister_event_handler (help->gui->xitk, &help->kreg);
    xitk_window_destroy_window(help->xwin);
    help->xwin = NULL;
    /* xitk_dlist_init (&help->widget_list->list); */

    if(help->num_sections) {
      int i;

      for(i = 0; i < help->num_sections; i++) {
	free(help->tab_sections[i]);
	free(help->sections[i]->name);
        _help_unload_string_list (help->sections[i]->content);
	free(help->sections[i]);
      }
    }

    video_window_set_input_focus(help->gui->vwin);

    help->gui->help = NULL;
    SAFE_FREE (help);
  }
}

static int help_event (void *data, const xitk_be_event_t *e) {
  xui_help_t *help = data;

  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    help_exit (NULL, help, 0);
    return 1;
  }
  return gui_handle_be_event (help->gui, e);
}

void help_raise_window (xui_help_t *help) {
  if (help)
    raise_window (help->gui, help->xwin, help->visible, 1);
}

void help_end (xui_help_t *help) {
  help_exit (NULL, help, 0);
}

int help_is_visible (xui_help_t *help) {
  return help ? (help->gui->use_root_window ?
                 (xitk_window_flags (help->xwin, 0, 0) & XITK_WINF_VISIBLE) :
                 (help->visible && (xitk_window_flags (help->xwin, 0, 0) & XITK_WINF_VISIBLE)))
              : 0;
}

void help_toggle_visibility (xui_help_t *help) {
  if (help)
    toggle_window (help->gui, help->xwin, help->widget_list, &help->visible, 1);
}


void help_panel (gGui_t *gui) {
  xui_help_t *help;
  int                        x, y;

  if (!gui)
    return;
  if (gui->help)
    return;
  help = (xui_help_t *)calloc (1, sizeof (*help));
  if (!help)
    return;
  help->gui = gui;

  x = y = 80;
  gui_load_window_pos (gui, "help", &x, &y);

  /* Create window */
  help->xwin = xitk_window_create_dialog_window (help->gui->xitk, _("Help"),
    x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  set_window_states_start (help->gui, help->xwin);
  help->widget_list = xitk_window_widget_list (help->xwin);

  help_sections (help);

  {
    xitk_tabs_widget_t tab;
    XITK_WIDGET_INIT (&tab);

    if (help->num_sections) {
      tab.num_entries = help->num_sections;
      tab.entries     = (const char * const *)help->tab_sections;
    } else {
      static const char *no_section[2];

      no_section[0] = _("No Help Section Available");
      no_section[1] = NULL;
      tab.num_entries = 1;
      tab.entries     = no_section;
    }
    tab.skin_element_name = NULL;
    tab.callback          = help_change_section;
    tab.userdata          = help;
    help->tabs = xitk_noskin_tabs_create (help->widget_list, &tab,
      15, 24, WINDOW_WIDTH - 30, tabsfontname);
    if (help->tabs) {
      xitk_add_widget (help->widget_list, help->tabs, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
      help->tabs_height = xitk_get_widget_height (help->tabs) - 1;
    }
  }

  {
    xitk_image_t *bg = xitk_window_get_background_image (help->xwin);
    if (bg) {
      xitk_image_draw_rectangular_box (bg, 15, 24 + help->tabs_height,
        WINDOW_WIDTH - 30, MAX_DISP_ENTRIES * 20 + 10, DRAW_OUTTER);
      xitk_window_set_background_image (help->xwin, bg);
    }
  }

  {
    xitk_browser_widget_t br;
    XITK_WIDGET_INIT (&br);

    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
    br.browser.num_entries           = 0;
    br.browser.entries               = NULL;
    br.callback                      = NULL;
    br.dbl_click_callback            = NULL;
    br.userdata                      = NULL;
    help->browser = xitk_noskin_browser_create (help->widget_list, &br,
      15 + 5, 24 + help->tabs_height + 5, WINDOW_WIDTH - (30 + 10), 20, -16, br_fontname);
    xitk_add_widget (help->widget_list, help->browser, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  }
  xitk_browser_set_alignment (help->browser, ALIGN_LEFT);
  help_change_section (NULL, help, 0);

  {
    xitk_widget_t *w;
    xitk_labelbutton_widget_t lb;
    XITK_WIDGET_INIT (&lb);

    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Close");
    lb.align             = ALIGN_CENTER;
    lb.callback          = help_exit;
    lb.state_callback    = NULL;
    lb.userdata          = help;
    lb.skin_element_name = NULL;
    w = xitk_noskin_labelbutton_create (help->widget_list, &lb,
      WINDOW_WIDTH - (100 + 15), WINDOW_HEIGHT - (23 + 15), 100, 23,
      XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, tabsfontname);
      xitk_add_widget (help->widget_list, w, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  }

  help->kreg = xitk_be_register_event_handler ("help", help->xwin, help_event, help, NULL, NULL);

  help->visible = 1;
  help_raise_window (help);

  xitk_window_set_input_focus (help->xwin);
  help->gui->help = help;
}

