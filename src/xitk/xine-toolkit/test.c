/*
** Copyright (C) 2001-2003 Daniel Caujolle-Bert <segfault@club-internet.fr>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <locale.h>

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

#include "xitk.h"

typedef struct {
  Display              *display;
  ImlibData            *imlibdata;

  xitk_window_t        *xwin;

  /* Browser */
  xitk_widget_t        *browser;
  char                 *entries[100];
  int                   num_entries;

  /* Sliders */
  xitk_widget_t        *hslider;
  xitk_widget_t        *vslider;
  xitk_widget_t        *rslider;

  /* Button */
  xitk_widget_t        *button;

  /* Label */
  xitk_widget_t        *combo;

  /* Label */
  xitk_widget_t        *label;

  /* Label */
  xitk_widget_t        *input;

  /* Tabs */
  xitk_widget_t        *tabs;

  /* Intbox */
  xitk_widget_t        *intbox;
  int                   oldintvalue;

  /* Doublebox */
  xitk_widget_t        *doublebox;
  double                olddoublevalue;

  /* checkbox */
  xitk_widget_t        *checkbox;

  /* menu */
  xitk_widget_t        *menu;

  xitk_widget_list_t   *widget_list;
  xitk_register_key_t   kreg;
} test_t;


static test_t *test;
static int nlab = 0;
static int align = ALIGN_LEFT;
#define FONT_HEIGHT_MODEL "azertyuiopqsdfghjklmwxcvbnAZERTYUIOPQSDFGHJKLMWXCVBN&é(-è_çà)=¹~#{[|`\\^@]}%"

static void create_menu(void);
/*
 *
 */
static int init_test(void) {
  ImlibInitParams    imlib_init;
  int                screen;
  int		     depth = 0;
  Visual	    *visual = NULL;
  
  if(!XInitThreads ()) {
    printf (_("XInitThreads() failed.\n"));
    return 0;
  } 

  test = (test_t *) xitk_xmalloc(sizeof(test_t));
  
  if((test->display = XOpenDisplay((getenv("DISPLAY")))) == NULL) {
    fprintf(stderr, _("Cannot open display\n"));
    return 0;
  }
  
  XLockDisplay (test->display);
  
  screen = DefaultScreen(test->display);
  depth = DefaultDepth(test->display, screen);
  visual = DefaultVisual(test->display, screen); 

  imlib_init.flags = PARAMS_VISUALID;
  imlib_init.visualid = visual->visualid;
  test->imlibdata = Imlib_init_with_params(test->display, &imlib_init);
  if (test->imlibdata == NULL) {
    fprintf(stderr, _("Imlib_init_with_params() failed.\n"));
    return 0;
  }

  xitk_init(test->display);

  XUnlockDisplay (test->display);

  return 1;
}

/*
 *
 */
static void deinit_test(void) {
  int i = 0;

  while(test->entries[i]) {
    free(test->entries[i]);
    i++;
  }
}

/*
 *
 */
static void test_end(xitk_widget_t *w, void *data) {
  XUnmapWindow(test->display, (xitk_window_get_window(test->xwin)));
  xitk_destroy_widgets(test->widget_list);
  free(test->widget_list);
  xitk_stop();
}

/*
 *
 */
void test_handle_event(XEvent *event, void *data) {
  XKeyEvent      mykeyevent;
  KeySym         mykey;
  char           kbuf[256];
  int            len;
  
  switch(event->type) {
    
  case EnterNotify:
    XLockDisplay(test->display);
    XRaiseWindow(test->display, xitk_window_get_window(test->xwin));
    XUnlockDisplay(test->display);
    break;

  case ButtonPress:
    {
      XButtonEvent *bevent = (XButtonEvent *) event;
      
      if(bevent->button == Button3)
	create_menu();
    }
    break;

  case KeyPress: {
    int modifier;
    
    (void) xitk_get_key_modifier(event, &modifier);
    
    mykeyevent = event->xkey;
    
    XLockDisplay (test->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay (test->display);
    
    switch (mykey) {
      
    case XK_q:
    case XK_Q:
      if(modifier & MODIFIER_CTRL)
	test_end(NULL, NULL);
      break;
      
    }   
  }
  break;
  
  case MappingNotify:
    XLockDisplay(test->display);
    XRefreshKeyboardMapping((XMappingEvent *) event);
    XUnlockDisplay(test->display);
    break;
    
    //  case ConfigureNotify:
    //    xitk_combo_update_pos(test->combo);
    //    break;
  }
}

/*
 * Set same pos in both sliders.
 */
static void move_sliders(xitk_widget_t *w, void *data, int pos) {
  if(w == test->vslider) {
    xitk_slider_set_pos(test->hslider, pos);
    xitk_slider_set_pos(test->rslider, pos);
  }
  else if(w == test->hslider) {
    xitk_slider_set_pos(test->vslider, pos);
    xitk_slider_set_pos(test->rslider, pos);
  }
  else if(w == test->rslider) {
    xitk_slider_set_pos(test->vslider, pos);
    xitk_slider_set_pos(test->hslider, pos);
  }

}

/*
 *
 */
static void window_message_cb(xitk_widget_t *w, void *data, int btn) {

  switch(btn) {
  case XITK_WINDOW_ANSWER_OK:
    printf("Button OK pressed.\n");
    break;
    
  case XITK_WINDOW_ANSWER_YES:
    printf("Button YES pressed.\n");
    break;
    
  case XITK_WINDOW_ANSWER_NO:
    printf("Button NO pressed.\n");
    break;

  case XITK_WINDOW_ANSWER_CANCEL:
    printf("Button CANCEL pressed.\n");
    break;

  default:
    printf("Button %d is unknown.\n", btn);
    break;
  }
}

/*
 *
 */
static void change_label(xitk_widget_t *w, void *data) {
  xitk_window_t *xw;
  static char *labels[] = {
    "A Label",
    "Boom"
  };

  if((++align) > ALIGN_RIGHT)
    align = ALIGN_CENTER;

  nlab = !nlab;

  xitk_label_change_label(test->label, labels[nlab]);

  xitk_browser_set_alignment(test->browser, align);
  
  //  xitk_window_dialog_ok_with_width(test->imlibdata, "Long error message", NULL, NULL, 500, ALIGN_LEFT, "premier \n\n\nnum %d\n", nlab);
  //  xitk_window_dialog_ok_with_width(test->imlibdata, "License information", NULL, NULL, 500, ALIGN_CENTER, "** This program is free software; you can redistribute it and/or modify** it under the terms of the GNU General Public License as published by** the Free Software Foundation; either version 2 of the License, or** (at your option) any later version.");
  //xitk_window_dialog_ok_with_width(test->imlibdata, "Long error message", window_message_cb, NULL, 500, ALIGN_DEFAULT, "** This program is free software; you can redistribute it and/or modify\n** it under the terms of the GNU General Public License as published by\n** the Free Software Foundation; either version 2 of the License, or\n** (at your option) any later version.");
  // xitk_window_dialog_yesno(test->imlibdata, NULL, NULL, NULL, NULL, ALIGN_LEFT, "Le programme <linux kernel> a provoqué une faute de protection dans le module <unknown> à l'adresse 0x00001234.\nCitroën dump:\nAX:0x00\t\tBX:0x00\nCX:0x00\t\tGS:0x00;-)");
  //  xitk_window_dialog_ok_with_width(test->imlibdata, "Long error message", window_message_cb, NULL, 500, ALIGN_DEFAULT, "**Thisprogramisfreesoftware;youcanredistributeitand/ormodify**itunderthetermsoftheGNUGeneralPublicLicenseaspublishedby**TheFreeSoftwareFoundation;eitherversion2oftheLicense,or**(atyouroption)anylaterversion.");
  xw = xitk_window_dialog_error(test->imlibdata, 
				"Stream number %d <%s.mpg> is not valid.\n", nlab, labels[nlab]);
}

/*
 *
 */
static void change_inputtext(xitk_widget_t *w, void *data, int selected) {

  if(test->entries[selected] != NULL)
    xitk_inputtext_change_text(test->input, test->entries[selected]);

}

/*
 *
 */
static void change_inputtext_dbl_click(xitk_widget_t *w, void *data, int selected) {

  if(test->entries[selected] != NULL)
    xitk_inputtext_change_text(test->input, test->entries[selected]);

}

/*
 *
 */
static void change_browser_entry(xitk_widget_t *w, void *data, char *currenttext) {
  int j;

  if((j = xitk_browser_get_current_selected(test->browser)) >= 0) {
    free(test->entries[j]);
    test->entries[j] = strdup(currenttext);
    xitk_browser_rebuild_browser(test->browser, j);
  }
}

/*
 *
 */
static void intchange_cb(xitk_widget_t *w, void *data, int btn) {

  switch(btn) {
  case XITK_WINDOW_ANSWER_YES:
    test->oldintvalue = xitk_intbox_get_value(test->intbox);
    break;
    
  case XITK_WINDOW_ANSWER_NO:
  case XITK_WINDOW_ANSWER_CANCEL:
    xitk_intbox_set_value(test->intbox, test->oldintvalue);
    break;
  }
}

/*
 *
 */
static void notify_intbox_change(xitk_widget_t *w, void *data, int value) {

  xitk_window_dialog_yesnocancel(test->imlibdata, NULL,
				 intchange_cb, 
				 intchange_cb, 
				 intchange_cb, 
				 NULL, ALIGN_DEFAULT, 
				 _("New integer value is: %d. Confirm ?"), value);
}

/*
 *
 */
static void create_intbox(void) {
  int x = 150, y = 300;
  xitk_intbox_widget_t ib;

  XITK_WIDGET_INIT(&ib, test->imlibdata);

  ib.skin_element_name = NULL;
  ib.value             = test->oldintvalue = 10;
  ib.step              = 1;
  ib.parent_wlist      = test->widget_list;
  ib.callback          = notify_intbox_change;
  ib.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list),
	    (test->intbox = 
	     xitk_noskin_intbox_create(test->widget_list, &ib, x, y, 60, 20, NULL, NULL, NULL)));
  xitk_enable_and_show_widget(test->intbox);
  xitk_set_widget_tips_default(test->intbox, "This is a intbox");
}

/*
 *
 */
static void doublechange_cb(xitk_widget_t *w, void *data, int btn) {

  switch(btn) {
  case XITK_WINDOW_ANSWER_YES:
    test->olddoublevalue = xitk_doublebox_get_value(test->doublebox);
    break;
    
  case XITK_WINDOW_ANSWER_NO:
  case XITK_WINDOW_ANSWER_CANCEL:
    xitk_doublebox_set_value(test->doublebox, test->olddoublevalue);
    break;
  }
}

/*
 *
 */
static void notify_doublebox_change(xitk_widget_t *w, void *data, double value) {

  xitk_window_dialog_yesnocancel(test->imlibdata, NULL,
				 doublechange_cb, 
				 doublechange_cb, 
				 doublechange_cb, 
				 NULL, ALIGN_DEFAULT, 
				 _("New double value is: %e. Confirm ?"), value);
}

/*
 *
 */
static void create_doublebox(void) {
  int x = 150, y = 330;
  xitk_doublebox_widget_t ib;

  XITK_WIDGET_INIT(&ib, test->imlibdata);

  ib.skin_element_name = NULL;
  ib.value             = test->olddoublevalue = 1.500;
  ib.step              = .5;
  ib.parent_wlist      = test->widget_list;
  ib.callback          = notify_doublebox_change;
  ib.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list),
	    (test->doublebox = 
	     xitk_noskin_doublebox_create(test->widget_list, &ib, x, y, 60, 20, NULL, NULL, NULL)));
  xitk_enable_and_show_widget(test->doublebox);
  xitk_set_widget_tips_default(test->doublebox, "This is a doublebox");
}

/*
 *
 */
static void create_checkbox(void) {
  int x = 250, y = 300;
  xitk_checkbox_widget_t cb;

  XITK_WIDGET_INIT(&cb, test->imlibdata);

  cb.skin_element_name = NULL;
  cb.callback          = NULL;
  cb.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list),
		    (test->checkbox = 
		     xitk_noskin_checkbox_create(test->widget_list, &cb, x, y, 20, 20)));
  xitk_enable_and_show_widget(test->checkbox);

  xitk_set_widget_tips_default(test->checkbox, "This is a checkbox");
}

/*
 *
 */
static void create_tabs(void) {
  xitk_pixmap_t      *bg;
  xitk_tabs_widget_t  t;
  char               *fontname = "*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*";
  int                 x = 150, y = 200, w = 300;
  int                 width, height;
  static char        *tabs_labels[] = {
    "Jumpin'", "Jack", "Flash", "It's", "Gas", "Gas", "Gas", ",", "Tabarnak"
  };

  XITK_WIDGET_INIT(&t, test->imlibdata);

  xitk_window_get_window_size(test->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(test->imlibdata, width, height);
  XCopyArea(test->display, (xitk_window_get_background(test->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  
  draw_rectangular_outter_box(test->imlibdata, bg, x, y+20, (w-1), 60);
  xitk_window_change_background(test->imlibdata, test->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);
  
  t.skin_element_name = NULL;
  t.num_entries       = 9;
  t.entries           = tabs_labels;
  t.parent_wlist      = test->widget_list;
  t.callback          = NULL;
  t.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list),
		    (test->tabs = 
		     xitk_noskin_tabs_create(test->widget_list, &t, x, y, w, fontname)));
  xitk_enable_and_show_widget(test->tabs);

}

/*
 *
 */
static void create_frame(void) {
  xitk_pixmap_t  *bg;
  int             width, height;
  char           *fontname = "*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
  int              x = 350, y = 50, w = 200, h = 150;
  
  xitk_window_get_window_size(test->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(test->imlibdata, width, height);
  XCopyArea(test->display, (xitk_window_get_background(test->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  
  draw_outter_frame(test->imlibdata, bg, _("My Frame"), fontname, x, y, w, h);
  draw_inner_frame(test->imlibdata, bg, NULL, NULL, x+(w>>2), y+(h>>2), w>>1, h>>1);
  
  xitk_window_change_background(test->imlibdata, test->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);
}

/*
 *
 */
static void create_inputtext(void) {
  xitk_inputtext_widget_t  inp;
  char                    *fontname = "*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";

  XITK_WIDGET_INIT(&inp, test->imlibdata);

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 256;
  inp.callback          = change_browser_entry;
  inp.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list),
	   (test->input = 
	    xitk_noskin_inputtext_create(test->widget_list, &inp,
					 150, 150, 150, 20,
					 "Black", "Black", fontname)));
  xitk_enable_and_show_widget(test->input);

  xitk_set_widget_tips_default(test->input, "This is an inputtext");
}

/*
 *
 */
static void create_label(void) {
  xitk_label_widget_t   lbl;
  char                 *fontname = "*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
  int                   x = 150, y = 120, len = 100;
  xitk_font_t          *fs;
  int                   lbear, rbear, wid, asc, des;
  char                 *label = _("A Label");

  XITK_WIDGET_INIT(&lbl, test->imlibdata);

  fs = xitk_font_load_font(test->display, fontname);
  xitk_font_set_font(fs, XITK_WIDGET_LIST_GC(test->widget_list));
  xitk_font_string_extent(fs, label, &lbear, &rbear, &wid, &asc, &des);
  xitk_font_unload_font(fs);

  lbl.window            = xitk_window_get_window(test->xwin);
  lbl.gc                = XITK_WIDGET_LIST_GC(test->widget_list);
  lbl.skin_element_name = NULL;
  lbl.label             = label;
  lbl.callback          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list), 
	   (test->label = 
	    xitk_noskin_label_create(test->widget_list, &lbl,
				     x, y, len, (asc+des)*2, fontname)));
  xitk_enable_and_show_widget(test->label);

  xitk_set_widget_tips_default(test->label, "This is a label");

  {
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(test->label);
    
    if(wimage) {
      draw_rectangular_inner_box(test->imlibdata, 
				 wimage->image, 0, 0, wimage->width-1, wimage->height-1);
    }
  }
}

/*
 *
 */
static void create_button(void) {
  xitk_button_widget_t b;
  int                  width = 130, height = 40, x = 150, y = 60;

  XITK_WIDGET_INIT(&b, test->imlibdata);

  b.skin_element_name = NULL;
  b.callback          = change_label;
  b.userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list), 
	   (test->button = 
	    xitk_noskin_button_create(test->widget_list, &b,
				      x, y, width, height)));
  xitk_enable_and_show_widget(test->button);

  xitk_set_widget_tips_default(test->button, "This is a button");

  { /* Draw red spot. */
    xitk_image_t *wimage = xitk_get_widget_foreground_skin(test->button);

    if(wimage) {
      unsigned int   col;
      xitk_font_t   *fs = NULL;
      char          *fontname = "*-helvetica-bold-r-*-*-14-*-*-*-*-*-*-*";
      int            lbear, rbear, wid, asc, des;
      char          *label = _("Fire !!");


      col = xitk_get_pixel_color_from_rgb(test->imlibdata, 255, 0, 0);

      XSetForeground(test->display, XITK_WIDGET_LIST_GC(test->widget_list), col);
      XSetBackground(test->display, XITK_WIDGET_LIST_GC(test->widget_list), col);
      XFillArc(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list),
	       10, (height >> 1) - 13, 26, 26, 
	       (0 * 64), (360 * 64));
      XFillArc(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list),
	       (width) + 10, (height >> 1) - 13, 26, 26, 
	       (0 * 64), (360 * 64));
      XFillArc(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list),
	       ((width*2) + 10) + 1, ((height >> 1) - 13) + 1, 26, 26, 
	       (0 * 64), (360 * 64));


      fs = xitk_font_load_font(test->display, fontname);
      xitk_font_set_font(fs, XITK_WIDGET_LIST_GC(test->widget_list));
      xitk_font_string_extent(fs, label, &lbear, &rbear, &wid, &asc, &des);
      
      col = xitk_get_pixel_color_black(test->imlibdata);

      XSetForeground(test->display, XITK_WIDGET_LIST_GC(test->widget_list), col);
      XDrawString(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list), 
		  50, ((height+asc+des) >> 1) - des, label, strlen(label));
      XDrawString(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list), 
		  (width) + 50, ((height+asc+des) >> 1) - des, label, strlen(label));
      
      {
	char *nlabel = _("!BOOM!");

      xitk_font_string_extent(fs, nlabel, &lbear, &rbear, &wid, &asc, &des);
      XDrawString(test->display, wimage->image->pixmap, XITK_WIDGET_LIST_GC(test->widget_list), 
		  ((width * 2) + 50) + 1, (((height+asc+des) >> 1) - des) + 1, 
		  nlabel, strlen(nlabel));
      }

      xitk_font_unload_font(fs);

    }

  }

}

/*
 *
 */
static void create_sliders(void) {
  xitk_slider_widget_t sl;

  XITK_WIDGET_INIT(&sl, test->imlibdata);

  sl.min                      = -1000;
  sl.max                      = 1000;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = move_sliders;
  sl.motion_userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list),
		   (test->hslider = xitk_noskin_slider_create(test->widget_list, &sl,
							      17, 208, 117, 20,
							      XITK_HSLIDER)));
  xitk_enable_and_show_widget(test->hslider);
  xitk_slider_set_pos(test->hslider, 0);

  xitk_set_widget_tips_default(test->hslider, "This is an horizontal slider");

  sl.min                      = -1000;
  sl.max                      = 1000;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = move_sliders;
  sl.motion_userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list),
		   (test->vslider = xitk_noskin_slider_create(test->widget_list, &sl,
							      17, 230, 20, 117,
							      XITK_VSLIDER)));
  xitk_enable_and_show_widget(test->vslider);
  xitk_slider_set_pos(test->vslider, 0);

  xitk_set_widget_tips_default(test->vslider, "This is a vertical slider");

  sl.min                      = -1000;
  sl.max                      = 1000;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = move_sliders;
  sl.motion_userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list),
		   (test->rslider = xitk_noskin_slider_create(test->widget_list, &sl,
							      50, 240, 80, 80,
							      XITK_RSLIDER)));
  xitk_enable_and_show_widget(test->rslider);
  xitk_slider_set_pos(test->rslider, 0);
  
  xitk_set_widget_tips_default(test->rslider, "This is a rotate button");

}

static void combo_select(xitk_widget_t *w, void *data, int select) {

  /*  
  xitk_window_dialog_error(test->imlibdata, NULL, NULL, NULL, buf);
  */
  /*
  xitk_window_dialog_yesnocancel(test->imlibdata, NULL,
				 window_message_cb, 
				 window_message_cb, 
				 window_message_cb, NULL, buf);
  */
  xitk_window_dialog_yesno(test->imlibdata, NULL,
			   window_message_cb, 
			   window_message_cb, 
			   NULL, ALIGN_DEFAULT, 
			   _("New entries selected in combo box is:\n%s [%d]."), 
			   test->entries[select], select);

}

/*
 *
 */
static void create_combo(void) {
  xitk_combo_widget_t    cmb;
  char                  *fontname = "*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
  int                    x = 150, y = 36, width = 100, height;
  xitk_font_t           *fs;

  XITK_WIDGET_INIT(&cmb, test->imlibdata);
  /*
  xitk_window_get_window_size(test->xwin, &wwidth, &wheight);
  bg = xitk_image_create_pixmap(test->imlibdata, wwidth, wheight);
  XCopyArea(test->display, (xitk_window_get_background(test->xwin)), bg, XITK_WIDGET_LIST_GC(test->widget_list),
	    0, 0, wwidth, wheight, 0, 0);
  */
  fs = xitk_font_load_font(test->display, fontname);
  xitk_font_set_font(fs, XITK_WIDGET_LIST_GC(test->widget_list));
  height = xitk_font_get_string_height(fs, FONT_HEIGHT_MODEL);
  xitk_font_unload_font(fs);
  /*
  draw_rectangular_inner_box(test->imlibdata, bg, (x - 4), (y - 4), (width + 8), (height + 7));
  xitk_window_change_background(test->imlibdata, test->xwin, bg, wwidth, wheight);
  
  XFreePixmap(test->display, bg);
  */

  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = test->widget_list;
  cmb.entries           = test->entries;
  cmb.layer_above       = 0;
  cmb.parent_wkey       = &test->kreg;
  cmb.callback          = combo_select;
  cmb.userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list), 
		   (test->combo = 
		    xitk_noskin_combo_create(test->widget_list, &cmb,
					     x, y, width, NULL, NULL)));
  xitk_enable_and_show_widget(test->combo);
}

/*
 *
 */
static void create_browser(void) {
  xitk_browser_widget_t  browser;
  char                  *fontname = "*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
  Pixmap                 bg;
  int                    width, height;

  XITK_WIDGET_INIT(&browser, test->imlibdata);

  xitk_window_get_window_size(test->xwin, &width, &height);
  bg = xitk_image_create_pixmap(test->imlibdata, width, height);
  XCopyArea(test->display, (xitk_window_get_background(test->xwin)), bg, XITK_WIDGET_LIST_GC(test->widget_list),
	    0, 0, width, height, 0, 0);

  XSetForeground(test->display, XITK_WIDGET_LIST_GC(test->widget_list), 
		 xitk_get_pixel_color_black(test->imlibdata));
  XDrawRectangle(test->display, bg, XITK_WIDGET_LIST_GC(test->widget_list), 17, 27, 117, 176);

  xitk_window_change_background(test->imlibdata, 
				test->xwin, bg, width, height);


  {
    int i;
    char buf[64];
    
    for(i = 0; i < 25; i++) {
      memset(&buf, 0, sizeof(buf));
      sprintf(buf, "Entry %d", i);
      test->entries[i] = strdup(buf);
    }
    
    test->entries[i] = NULL;
    test->num_entries = i;
  }

  browser.arrow_up.skin_element_name    = NULL;
  browser.slider.skin_element_name      = NULL;
  browser.arrow_dn.skin_element_name    = NULL;
  browser.browser.skin_element_name     = NULL;
  browser.browser.max_displayed_entries = 8;
  browser.browser.num_entries           = test->num_entries;
  browser.browser.entries               = (const char *const *)test->entries;
  browser.callback                      = change_inputtext;
  browser.dbl_click_callback            = change_inputtext_dbl_click;
  browser.parent_wlist                  = test->widget_list;
  browser.userdata                      = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(test->widget_list), 
		    (test->browser = 
		     xitk_noskin_browser_create(test->widget_list, &browser,
						XITK_WIDGET_LIST_GC(test->widget_list), 20, 30, 
						100, 20, 12, fontname)));
  xitk_enable_and_show_widget(test->browser);
  
  xitk_browser_update_list(test->browser, 
			   (const char *const *)test->entries, test->num_entries, 0);

}

static void menu_test_end(xitk_widget_t *w, xitk_menu_entry_t *me, void *data) {
  test_end(NULL, NULL);
}
static void create_menu(void) {
  xitk_menu_widget_t   menu;
  int                  x = 10, y = 10;
  xitk_menu_entry_t    menu_entries[] = {
    { "Popup menu:-------------------",  "<title>",       NULL,     NULL },
    { "Load a file",                     NULL,            NULL,     NULL },
    { "Reload",                          NULL,            NULL,     NULL },
    { "Auto save",                       "<check>",       NULL,     NULL },
    { "SEP",                             "<separator>",   NULL,     NULL },
    { "Empty",                           "<branch>",      NULL,     NULL },
    { "Playlist",                        "<branch>",      NULL,     NULL },
    { "Playlist/Playlist Management",    "<title>",       NULL,     NULL },
    { "Playlist/Load",                   NULL,            NULL,     NULL },
    { "Playlist/Loop",                   "<branch>",      NULL,     NULL },
    { "Playlist/Loop/no loop",           "<checked>",     NULL,     NULL },
    { "Playlist/Loop/simple loop",       "<check>",       NULL,     NULL },
    { "Playlist/Loop/SEP",               "<separator>",   NULL,     NULL },
    { "Playlist/Loop/repeat entry",      "<check>",       NULL,     NULL },
    { "Playlist/Loop/SEP",               "<separator>",   NULL,     NULL },
    { "Playlist/Loop/shuffle",           "<check>",       NULL,     NULL },
    { "Playlist/Loop/infinite shuffle",  "<check>",       NULL,     NULL },
    { "Playlist/Loop/Extra",             "<branch>",      NULL,     NULL },
    { "Playlist/Save",                   NULL,            NULL,     NULL },
    { "Another branch",                  "<branch>",      NULL,     NULL },
    { "Another branch/dummy",            NULL,            NULL,     NULL },
    { "Another branch/vice\\/versa",     NULL,            NULL,     NULL },
    { "SEP",                             "<separator>",   NULL,     NULL },
    { "Save a file",                     NULL,            NULL,     NULL },
    { "SEP",                             "<separator>",   NULL,     NULL },
    { "Video",                           "<branch>",      NULL,     NULL },
    { "Video/premier",                   "<branch>",      NULL,     NULL },
    { "Video/premier/Le Voili",          NULL,            NULL,     NULL },
    { "Video/second",                    NULL,            NULL,     NULL },
    { "Video/troisieme",                 NULL,            NULL,     NULL },
    { "Video/quatrieme",                 NULL,            NULL,     NULL },
    { "SEP",                             "<separator>",   NULL,     NULL },
    { "Quit",                            NULL,            menu_test_end, NULL },
    { NULL,                              NULL,            NULL,     NULL }
  };
  
  XITK_WIDGET_INIT(&menu, test->imlibdata);

  (void) xitk_get_mouse_coords(test->display, 
			       (xitk_window_get_window(test->xwin)), NULL, NULL, &x, &y);
  
  menu.menu_tree         = &menu_entries[0];
  menu.parent_wlist      = test->widget_list;
  menu.skin_element_name = NULL;
  
  test->menu = xitk_noskin_menu_create(test->widget_list, &menu, x, y);

  {
    xitk_menu_entry_t menu_entry;

    menu_entry.menu      = "Playlist/Loop/Extra/errrr?";
    menu_entry.type      = NULL;
    menu_entry.cb        = NULL;
    menu_entry.user_data = NULL;
    xitk_menu_add_entry(test->menu, &menu_entry);
  }
  

  xitk_menu_show_menu(test->menu);
}

/*
 *
 */
int main(int argc, char **argv) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  char                       *fontname = "*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
  int                         windoww = 600, windowh = 400;
  xitk_widget_t              *w;
  
  if(!init_test()) {
    printf(_("init_test() failed\n"));
    exit(1);
  }

#ifdef HAVE_SETLOCALE
  xitk_set_locale();
  setlocale (LC_ALL, "");
  printf("locale\n");
#endif
  
  bindtextdomain("xitk", XITK_LOCALE);
  textdomain("xitk");


  /* Create window */
  test->xwin = xitk_window_create_dialog_window(test->imlibdata,
						_("My Fucking Window"), 
						100, 100, windoww, windowh);
  
  XLockDisplay (test->display);

  gc = XCreateGC(test->display, 
		 (xitk_window_get_window(test->xwin)), None, None);

  test->widget_list  = xitk_widget_list_new();
  xitk_widget_list_set(test->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(test->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(test->xwin)));
  xitk_widget_list_set(test->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, test->imlibdata);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Quit");
  lb.align             = ALIGN_CENTER;
  lb.callback          = test_end;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(test->widget_list),
	   (w = xitk_noskin_labelbutton_create(test->widget_list, &lb,
					       (windoww / 2) - 50, windowh - 50,
					       100, 30,
					       "Black", "Black", "White", fontname)));
  xitk_enable_and_show_widget(w);
  xitk_set_widget_tips_default(w, "Do you really want to leave me ?");

  create_browser();
  create_sliders();
  create_button();
  create_combo();
  create_label();
  create_inputtext();
  create_frame();
  create_tabs();
  create_intbox();
  create_doublebox();
  create_checkbox();
  
  test->kreg = xitk_register_event_handler("test", 
					   (xitk_window_get_window(test->xwin)),
					   test_handle_event,
					   NULL,
					   NULL,
					   test->widget_list,
					   NULL);

  XUnlockDisplay (test->display);

  XMapRaised(test->display, xitk_window_get_window(test->xwin));

  xitk_run(NULL, NULL);

  deinit_test();

  XLockDisplay (test->display);
  XFreeGC(test->display, gc);
  XUnlockDisplay (test->display);

  XCloseDisplay(test->display);

  free(test);
  
  return 1;
}
