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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "common.h"

extern gGui_t                   *gGui;

static char                     *br_fontname = "-misc-fixed-medium-r-normal-*-10-*-*-*-*-*-*-*";
static char                     *tabsfontname = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

#define WINDOW_WIDTH             580
#define WINDOW_HEIGHT            480

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

  char                 *tab_sections[MAX_SECTIONS];
  
  help_section_t       *sections[MAX_SECTIONS];
  int                   num_sections;

  xitk_register_key_t   kreg;

} _help_t;

static _help_t    *help = NULL;

static void help_change_section(xitk_widget_t *wx, void *data, int section) {
  if(section < help->num_sections)
    xitk_browser_update_list(help->browser, help->sections[section]->content, 
			     help->sections[section]->line_num, 0);
}

static void help_add_section(char *filename, int order_num, char *section_name) {
  struct stat  st;
  
  /* ensure that the file is not empty */
  if(help->num_sections < MAX_SECTIONS) {
    if((stat(filename, &st) == 0) && (st.st_size)) {
      int   fd;

      if((fd = open(filename, O_RDONLY)) >= 0) {
	char  *buf = NULL;
	int    bytes_read;

	if((buf = (char *) xine_xmalloc(st.st_size + 1))) {
	  if((bytes_read = read(fd, buf, st.st_size)) == st.st_size) {
	    char  *p, **hbuf = NULL;
	    int    lines = 0;
	    
	    buf[st.st_size] = '\0';
	    
	    while((p = xine_strsep(&buf, "\n")) != NULL) {
	      if(!lines)
		hbuf = (char **) xine_xmalloc(sizeof(char *) * 2);
	      else
		hbuf  = (char **) realloc(hbuf, sizeof(char *) * (lines + 2));

	      while((*(p + strlen(p) - 1) == '\n') || (*(p + strlen(p) - 1) == '\r'))
		*(p + strlen(p) - 1) = '\0';
	      
	      hbuf[lines++] = strdup(p);
	      hbuf[lines]   = NULL;
	    }
	    
	    if(lines) {
	      help_section_t  *section;

	      section = (help_section_t *) xine_xmalloc(sizeof(help_section_t));

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
    }
  }
}

static int readme_select(const struct dirent *dir_entry) {
  int           num;
  char          section[XITK_NAME_MAX];
  
  if((sscanf(dir_entry->d_name, "README.en.%d.%s", &num, &section[0])) == 2)
    return 1;
  
  return 0;
}


static void help_sections(void) {
  static struct dirent **namelist;
  int                    i;
  
  if((i = scandir(XINE_DOCDIR, &namelist, readme_select, 0)) > 0) {
    const langs_t       *lang = get_lang();
    char                 locale_file[XITK_NAME_MAX];
    char                 locale_readme[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    char                 default_readme[XITK_PATH_MAX + XITK_NAME_MAX + 1];
    char                 ending[XITK_NAME_MAX];
    char                 section_name[1024];
    struct stat          pstat;
    
    while(--i >= 0) {
      int order_num;

      memset(&ending, 0, sizeof(ending));
      memset(&section_name, 0, sizeof(section_name));
      
      sscanf(namelist[i]->d_name, "README.en.%s", &ending[0]);
      sscanf(namelist[i]->d_name, "README.en.%d.%s", &order_num, &section_name[0]);
      
      sprintf(locale_file, "%s.%s.%s", "README", lang->ext, ending);
      sprintf(locale_readme, "%s/%s", XINE_DOCDIR, locale_file);
      sprintf(default_readme, "%s/%s", XINE_DOCDIR, namelist[i]->d_name);
      
      if((strcmp(locale_file, namelist[i]->d_name))
	 && ((stat(locale_readme, &pstat)) == 0) && (S_ISREG(pstat.st_mode)))
	help_add_section(locale_readme, order_num, section_name);
      else
	help_add_section(default_readme, order_num, section_name);

      free(namelist[i]);
    }
    
    free(namelist);
    
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
  else {
    if(i == -1) {
      /* an error occured, whish one ? */
      xine_error("Cannot open help files: %s", strerror(errno));
    }
  }
}

void help_exit(xitk_widget_t *w, void *data) {

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
    
    xitk_destroy_widgets(help->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, help->xwin);
    
    help->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(help->widget_list)));
    
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

    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(help->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(help->widget_list);
    
    SAFE_FREE(help);
  }
}

void help_raise_window(void) {
  if(help != NULL)
    raise_window(xitk_window_get_window(help->xwin), help->visible, help->running);
}

static void help_end(xitk_widget_t *w, void *data) {
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
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(help->xwin));
    else
      return help->visible;
  }

  return 0;
}

void help_toggle_visibility (xitk_widget_t *w, void *data) {
  if(help != NULL)
    toggle_window(xitk_window_get_window(help->xwin), help->widget_list, 
		  &help->visible, help->running);
}

void help_panel(void) {
  GC                         gc;
  xitk_labelbutton_widget_t  lb;
  xitk_browser_widget_t      br;
  xitk_tabs_widget_t         tab;
  xitk_pixmap_t             *bg;
  int                        x, y;
  xitk_widget_t             *w;

  help = (_help_t *) xine_xmalloc(sizeof(_help_t));

  x = xine_config_register_num (gGui->xine, "gui.help_x", 
				100, 
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.help_y", 
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  help->xwin = xitk_window_create_dialog_window(gGui->imlib_data,
						_("Help"), 
						x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(help->xwin)), None, None);
  XUnlockDisplay (gGui->display);

  help->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(help->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(help->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(help->xwin)));
  xitk_widget_list_set(help->widget_list, WIDGET_LIST_GC, gc);

  help_sections();

  XITK_WIDGET_INIT(&tab, gGui->imlib_data);
  
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
  tab.parent_wlist      = help->widget_list;
  tab.callback          = help_change_section;
  tab.userdata          = NULL;
  xitk_list_append_content ((XITK_WIDGET_LIST_LIST(help->widget_list)),
    (help->tabs = 
     xitk_noskin_tabs_create(help->widget_list, 
			     &tab, 20, 24, WINDOW_WIDTH - 40, tabsfontname)));
  
  xitk_enable_and_show_widget(help->tabs);

  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay(gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(help->xwin)), bg->pixmap,
	    bg->gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0);
  XUnlockDisplay(gGui->display);
  
  draw_rectangular_outter_box(gGui->imlib_data, bg, 20, 51, 
			      (WINDOW_WIDTH - 40) - 1, (WINDOW_HEIGHT - (51 + 57)) - 5);
  xitk_window_change_background(gGui->imlib_data, help->xwin, bg->pixmap, 
				WINDOW_WIDTH, WINDOW_HEIGHT);
  
  xitk_image_destroy_xitk_pixmap(bg);
  
  XITK_WIDGET_INIT(&br, gGui->imlib_data);

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
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(help->widget_list)), 
   (help->browser = 
    xitk_noskin_browser_create(help->widget_list, &br,
			       (XITK_WIDGET_LIST_GC(help->widget_list)), 25, 58, 
			       WINDOW_WIDTH - (50 + 16), 20,
			       16, br_fontname)));

  xitk_enable_and_show_widget(help->browser);

  xitk_browser_set_alignment(help->browser, ALIGN_LEFT);
  help_change_section(NULL, NULL, 0);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);

  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = help_end; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(help->widget_list)), 
   (w = xitk_noskin_labelbutton_create(help->widget_list, &lb, 
				       (WINDOW_WIDTH - (100 + 20)), WINDOW_HEIGHT - 40, 100, 23,
				       "Black", "Black", "White", tabsfontname)));
  xitk_enable_and_show_widget(w);
  
  help->kreg = xitk_register_event_handler("help", 
					   (xitk_window_get_window(help->xwin)),
					   NULL,
					   NULL,
					   NULL,
					   help->widget_list,
					   NULL);
  
  help->visible = 1;
  help->running = 1;
  help_raise_window();

  try_to_set_input_focus(xitk_window_get_window(help->xwin));
}
