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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#include "xine-toolkit/recode.h"

#include "common.h"

#include "lang.h"

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_CODESET)
#  include <iconv.h>
#  include <langinfo.h>
#  define USE_CONV
#endif


#define WINDOW_WIDTH             570
#define WINDOW_HEIGHT            473

#define MAX_SECTIONS             50

#define MAX_DISP_ENTRIES         17

typedef struct {
  char                 *name;
  const char          **content;
  int                   line_num;
  int                   order_num;
} help_section_t;

typedef struct {
  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;

  int                   running;
  int                   visible;

  xitk_widget_t        *tabs;
  xitk_widget_t        *ok;
  xitk_widget_t        *browser;

  char                 *tab_sections[MAX_SECTIONS + 1];
  
  help_section_t       *sections[MAX_SECTIONS + 1];
  int                   num_sections;

  xitk_register_key_t   kreg;

} _help_t;

static _help_t    *help = NULL;

static int         th; /* Tabs height */

static void help_change_section(xitk_widget_t *wx, void *data, int section) {
  if(section < help->num_sections)
    xitk_browser_update_list(help->browser, help->sections[section]->content, NULL,
			     help->sections[section]->line_num, 0);
}

static void help_add_section(const char *filename, const char *doc_encoding, 
                             int order_num, char *section_name) {
  struct stat  st;
  xitk_recode_t *xr;
  
  /* ensure that the file is not empty */
  if(help->num_sections < MAX_SECTIONS) {
    if((stat(filename, &st) == 0) && (st.st_size)) {
      int   fd;

      assert(doc_encoding != NULL);
      xr = xitk_recode_init (doc_encoding, NULL, 0);

      if((fd = xine_open_cloexec(filename, O_RDONLY)) >= 0) {
	char  *buf = NULL, *pbuf;
	int    bytes_read;

	if((buf = (char *) malloc(st.st_size + 1))) {
	  if((bytes_read = read(fd, buf, st.st_size)) == st.st_size) {
	    char  *p, **hbuf = NULL;
	    int    lines = 0;
	    
	    buf[st.st_size] = '\0';

	    pbuf = buf;
	    while((p = xine_strsep(&pbuf, "\n")) != NULL) {
	      hbuf  = (char **) realloc(hbuf, sizeof(char *) * (lines + 2));
	      
	      while((*(p + strlen(p) - 1) == '\n') || (*(p + strlen(p) - 1) == '\r'))
		*(p + strlen(p) - 1) = '\0';

              hbuf[lines] = xitk_recode(xr, p);
	      hbuf[++lines]   = NULL;
	    }
	    
	    if(lines) {
	      help_section_t  *section;

	      section = (help_section_t *) calloc(1, sizeof(help_section_t));

	      section->name      = strdup((section_name && strlen(section_name)) ? 
					  section_name : _("Undefined"));
	      section->content   = (const char **)hbuf;
	      section->line_num  = lines;
	      section->order_num = order_num;
	      
	      help->sections[help->num_sections++] = section;
	      help->sections[help->num_sections]   = NULL;
	    }
	  }

	  free(buf);
	}
	close(fd);
      }
      xitk_recode_done(xr);
    }
  }
}

static void help_sections(void) {
  DIR                 *dir;
  int                  i;

  if ((dir = opendir(XINE_DOCDIR)) == NULL) {
    xine_error (gGui, "Cannot open help files: %s", strerror(errno));
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
	  help_add_section(locale_readme, lang->doc_encoding, order_num, section_name);
	}
	else {
	  help_add_section(default_readme, "ISO-8859-1", order_num, section_name);
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

static void help_exit(xitk_widget_t *w, void *data) {

  if(help) {
    window_info_t wi;
    
    help->running = 0;
    help->visible = 0;
    
    if((xitk_get_window_info(help->kreg, &wi))) {
      config_update_num("gui.help_x", wi.x);
      config_update_num("gui.help_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&help->kreg);
    xitk_window_destroy_window(help->xwin);
    help->xwin = NULL;
    /* xitk_dlist_init (&help->widget_list->list); */
    
    if(help->num_sections) {
      int i;

      for(i = 0; i < help->num_sections; i++) {
	int j = 0;
	
	free(help->tab_sections[i]);
	
	free(help->sections[i]->name);
	
	while(help->sections[i]->content[j]) {
	  free((char *) help->sections[i]->content[j]);
	  j++;
	}
	
	free(help->sections[i]->content);
	free(help->sections[i]);
      }
    }

    SAFE_FREE(help);

    video_window_set_input_focus(gGui->vwin);
  }
}

static void help_handle_event(XEvent *event, void *data) {

  switch(event->type) {
    
  case KeyPress:
    if(xitk_get_key_pressed(event) == XK_Escape)
      help_exit(NULL, NULL);
    else
      gui_handle_event (event, gGui);
    break;
  }
}

void help_raise_window(void) {
  if(help != NULL)
    raise_window(help->xwin, help->visible, help->running);
}

void help_end(void) {
  help_exit(NULL, NULL);
}

int help_is_running(void) {

  if(help != NULL)
    return help->running;

  return 0;
}

int help_is_visible(void) {
  
  if(help != NULL) {
    if(gGui->use_root_window)
      return xitk_window_is_window_visible(help->xwin);
    else
      return help->visible && xitk_window_is_window_visible(help->xwin);
  }

  return 0;
}

void help_toggle_visibility (xitk_widget_t *w, void *data) {
  if(help != NULL)
    toggle_window(help->xwin, help->widget_list, &help->visible, help->running);
}

void help_reparent(void) {
  if(help)
    reparent_window(help->xwin);
}

void help_panel(void) {
  xitk_labelbutton_widget_t  lb;
  xitk_browser_widget_t      br;
  xitk_tabs_widget_t         tab;
  xitk_pixmap_t             *bg;
  int                        x, y;
  xitk_widget_t             *w;

  help = (_help_t *) calloc(1, sizeof(_help_t));

  x = xine_config_register_num (__xineui_global_xine_instance, "gui.help_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, "gui.help_y", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  help->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						_("Help"), 
						x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start(help->xwin);

  help->widget_list = xitk_window_widget_list(help->xwin);

  help_sections();

  XITK_WIDGET_INIT(&tab);
  
  if(help->num_sections) {
    tab.num_entries       = help->num_sections;
    tab.entries           = help->tab_sections;
  }
  else {
    static char *no_section[] = { NULL, NULL };
    
    no_section[0] = _("No Help Section Available");
    
    tab.num_entries       = 1;
    tab.entries           = no_section;
  }
  tab.skin_element_name = NULL;
  tab.callback          = help_change_section;
  tab.userdata          = NULL;
  help->tabs = xitk_noskin_tabs_create (help->widget_list,
    &tab, 15, 24, WINDOW_WIDTH - 30, tabsfontname);
  xitk_add_widget (help->widget_list, help->tabs);
  th = xitk_get_widget_height(help->tabs) - 1;
  xitk_enable_and_show_widget(help->tabs);

  bg = xitk_window_get_background_pixmap(help->xwin);

  draw_rectangular_outter_box(bg, 15, (24 + th),
			      (WINDOW_WIDTH - 30 - 1), (MAX_DISP_ENTRIES * 20 + 16 + 10 - 1));
  xitk_window_set_background(help->xwin, bg);

  XITK_WIDGET_INIT(&br);

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = 0;
  br.browser.entries               = NULL;
  br.callback                      = NULL;
  br.dbl_click_callback            = NULL;
  br.parent_wlist                  = help->widget_list;
  br.userdata                      = NULL;
  help->browser = xitk_noskin_browser_create (help->widget_list, &br,
    (XITK_WIDGET_LIST_GC(help->widget_list)), 15 + 5, (24 + th) + 5, WINDOW_WIDTH - (30 + 10 + 16), 20, 16, br_fontname);
  xitk_add_widget (help->widget_list, help->browser);
  xitk_enable_and_show_widget(help->browser);

  xitk_browser_set_alignment(help->browser, ALIGN_LEFT);
  help_change_section(NULL, NULL, 0);
  
  XITK_WIDGET_INIT(&lb);

  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = help_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  w = xitk_noskin_labelbutton_create (help->widget_list, &lb,
    WINDOW_WIDTH - (100 + 15), WINDOW_HEIGHT - (23 + 15), 100, 23, "Black", "Black", "White", tabsfontname);
  xitk_add_widget (help->widget_list, w);
  xitk_enable_and_show_widget(w);
  
  help->kreg = xitk_register_event_handler("help",
                                           help->xwin,
					   help_handle_event,
					   NULL,
					   NULL,
					   help->widget_list,
					   NULL);
  
  help->visible = 1;
  help->running = 1;
  help_raise_window();

  xitk_window_try_to_set_input_focus(help->xwin);
}

