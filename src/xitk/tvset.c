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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "common.h"

#include "frequencies.h"
	
#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       310

extern gGui_t          *gGui;

static char            *tvsetfontname     = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char            *lfontname         = "-*-helvetica-bold-r-*-*-11-*-*-*-*-*-*-*";
static char            *btnfontname       = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

typedef struct {
  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *input;
  xitk_widget_t        *system;
  xitk_widget_t        *chann;
  xitk_widget_t        *framerate;
  xitk_widget_t        *vidstd;

  xitk_widget_t        *close;
  xitk_widget_t        *update;

  char                **system_entries;
  char                **chann_entries;
  char                **vidstd_entries;

  int                   running;
  int                   visible;
  xitk_register_key_t   widget_key;

} _tvset_t;

static _tvset_t    *tvset = NULL;


struct _std_list {
    const char     *name;
    uint64_t        std;
};

typedef uint64_t v4l2_std_id;

/* one bit for each */
#define V4L2_STD_PAL_B          ((v4l2_std_id)0x00000001)
#define V4L2_STD_PAL_B1         ((v4l2_std_id)0x00000002)
#define V4L2_STD_PAL_G          ((v4l2_std_id)0x00000004)
#define V4L2_STD_PAL_H          ((v4l2_std_id)0x00000008)
#define V4L2_STD_PAL_I          ((v4l2_std_id)0x00000010)
#define V4L2_STD_PAL_D          ((v4l2_std_id)0x00000020)
#define V4L2_STD_PAL_D1         ((v4l2_std_id)0x00000040)
#define V4L2_STD_PAL_K          ((v4l2_std_id)0x00000080)

#define V4L2_STD_PAL_M          ((v4l2_std_id)0x00000100)
#define V4L2_STD_PAL_N          ((v4l2_std_id)0x00000200)
#define V4L2_STD_PAL_Nc         ((v4l2_std_id)0x00000400)
#define V4L2_STD_PAL_60         ((v4l2_std_id)0x00000800)

#define V4L2_STD_NTSC_M         ((v4l2_std_id)0x00001000)
#define V4L2_STD_NTSC_M_JP      ((v4l2_std_id)0x00002000)

#define V4L2_STD_SECAM_B        ((v4l2_std_id)0x00010000)
#define V4L2_STD_SECAM_D        ((v4l2_std_id)0x00020000)
#define V4L2_STD_SECAM_G        ((v4l2_std_id)0x00040000)
#define V4L2_STD_SECAM_H        ((v4l2_std_id)0x00080000)
#define V4L2_STD_SECAM_K        ((v4l2_std_id)0x00100000)
#define V4L2_STD_SECAM_K1       ((v4l2_std_id)0x00200000)
#define V4L2_STD_SECAM_L        ((v4l2_std_id)0x00400000)

/* ATSC/HDTV */
#define V4L2_STD_ATSC_8_VSB     ((v4l2_std_id)0x01000000)
#define V4L2_STD_ATSC_16_VSB    ((v4l2_std_id)0x02000000)


static struct _std_list std_list[] = {
  { "PAL-B",       V4L2_STD_PAL_B       },
  { "PAL-B1",      V4L2_STD_PAL_B1      },
  { "PAL-G",       V4L2_STD_PAL_G       },
  { "PAL-H",       V4L2_STD_PAL_H       },
  { "PAL-I",       V4L2_STD_PAL_I       },
  { "PAL-D",       V4L2_STD_PAL_D       },
  { "PAL-D1",      V4L2_STD_PAL_D1      },
  { "PAL-K",       V4L2_STD_PAL_K       },
  { "PAL-M",       V4L2_STD_PAL_M       },
  { "PAL-N",       V4L2_STD_PAL_N       },
  { "PAL-Nc",      V4L2_STD_PAL_Nc      },
  { "PAL-60",      V4L2_STD_PAL_60      },
  { "NTSC-M",      V4L2_STD_NTSC_M      },
  { "NTSC-M-JP",   V4L2_STD_NTSC_M_JP   },
  { "SECAM-B",     V4L2_STD_SECAM_B     },
  { "SECAM-D",     V4L2_STD_SECAM_D     },
  { "SECAM-G",     V4L2_STD_SECAM_G     },
  { "SECAM-H",     V4L2_STD_SECAM_H     },
  { "SECAM-K",     V4L2_STD_SECAM_K     },
  { "SECAM-K1",    V4L2_STD_SECAM_K1    },
  { "SECAM-L",     V4L2_STD_SECAM_L     },
  { "ATSC-8-VSB",  V4L2_STD_ATSC_8_VSB  },
  { "ATSC-16-VSB", V4L2_STD_ATSC_16_VSB }
};



static void tvset_update(xitk_widget_t *w, void *data) {
  xine_event_t          xine_event;
  xine_set_v4l2_data_t *ev_data;
  
  ev_data = (xine_set_v4l2_data_t *)xine_xmalloc(sizeof(xine_set_v4l2_data_t));
  
  ev_data->input     = xitk_intbox_get_value(tvset->input);
  ev_data->frequency = (chanlists[xitk_combo_get_current_selected(tvset->system)].
			list[xitk_combo_get_current_selected(tvset->chann)].freq * 16) / 1000;
  
  ev_data->standard_id = std_list[xitk_combo_get_current_selected(tvset->vidstd)].std;
  
  xine_event.type        = XINE_EVENT_SET_V4L2;
  xine_event.data_length = sizeof(xine_set_v4l2_data_t);
  xine_event.data        = ev_data;
  xine_event.stream      = gGui->stream;
  gettimeofday(&xine_event.tv, NULL);
  
  xine_event_send(gGui->stream, &xine_event);
}


static void tvset_exit(xitk_widget_t *w, void *data) {

#warning CHECK FOR MEMLEAK  
  if(tvset) {
    window_info_t wi;
    
    tvset->running = 0;
    tvset->visible = 0;
    
    if((xitk_get_window_info(tvset->widget_key, &wi))) {
      config_update_num ("gui.tvset_x", wi.x);
      config_update_num ("gui.tvset_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    xitk_unregister_event_handler(&tvset->widget_key);

    xitk_destroy_widgets(tvset->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, tvset->xwin);

    tvset->xwin = None;
    xitk_list_free((XITK_WIDGET_LIST_LIST(tvset->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(tvset->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(tvset->widget_list);
    
    free(tvset);
    tvset = NULL;
  }
}

int tvset_is_visible(void) {
  
  if(tvset != NULL) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(tvset->xwin));
    else
      return tvset->visible;
  }
  
  return 0;
}

int tvset_is_running(void) {
  
  if(tvset != NULL)
    return tvset->running;
  
  return 0;
}

void tvset_toggle_visibility(xitk_widget_t *w, void *data) {
  if(tvset != NULL) {
    if (tvset->visible && tvset->running) {
      XLockDisplay(gGui->display);
      if(gGui->use_root_window) {
	if(xitk_is_window_visible(gGui->display, xitk_window_get_window(tvset->xwin)))
	  XIconifyWindow(gGui->display, xitk_window_get_window(tvset->xwin), gGui->screen);
	else
	  XMapWindow(gGui->display, xitk_window_get_window(tvset->xwin));
      }
      else {
	tvset->visible = 0;
	xitk_hide_widgets(tvset->widget_list);
	XUnmapWindow (gGui->display, xitk_window_get_window(tvset->xwin));
      }
      XUnlockDisplay(gGui->display);
    } 
    else {
      if(tvset->running) {
	tvset->visible = 1;
	xitk_show_widgets(tvset->widget_list);
	XLockDisplay(gGui->display);
	XRaiseWindow(gGui->display, xitk_window_get_window(tvset->xwin)); 
	XMapWindow(gGui->display, xitk_window_get_window(tvset->xwin)); 
	if(!gGui->use_root_window)
	  XSetTransientForHint (gGui->display, 
				xitk_window_get_window(tvset->xwin), gGui->video_window);
	XUnlockDisplay(gGui->display);
	layer_above_video(xitk_window_get_window(tvset->xwin));
      }
    }
  }
}


void tvset_raise_window(void) {
  if(tvset != NULL) {
    if(tvset->xwin) {
      if(tvset->visible && tvset->running) {
	  XLockDisplay(gGui->display);
	  XUnmapWindow(gGui->display, xitk_window_get_window(tvset->xwin));
	  XRaiseWindow(gGui->display, xitk_window_get_window(tvset->xwin));
	  XMapWindow(gGui->display, xitk_window_get_window(tvset->xwin));
	  if(!gGui->use_root_window)
	    XSetTransientForHint (gGui->display, 
				  xitk_window_get_window(tvset->xwin), gGui->video_window);
	  XUnlockDisplay(gGui->display);
	  layer_above_video(xitk_window_get_window(tvset->xwin));
      }
    }
  }
}

void tvset_end(void) {
  tvset_exit(NULL, NULL);
}


static int update_chann_entries(int system_entry) {
  int               i;
  struct CHANLIST  *list = chanlists[system_entry].list;
  int               len  = chanlists[system_entry].count;
  
  if( tvset->chann_entries ) {
    for(i = 0; tvset->chann_entries[i]; i++)
      free(tvset->chann_entries[i]);
    free(tvset->chann_entries);
  }

  tvset->chann_entries = (char **) xine_xmalloc(sizeof(char *) * (len+1) );
  
  for(i = 0; i < len; i++)
    tvset->chann_entries[i] = strdup(list[i].name);

  tvset->chann_entries[i] = NULL;
  return len;
}

static void system_combo_select(xitk_widget_t *w, void *data, int select) {
  int len;
  
  len = update_chann_entries(select);

  if( tvset->chann ) {
    xitk_combo_update_list(tvset->chann, tvset->chann_entries, len);
    xitk_combo_set_select(tvset->chann, 0);
  }
}


void tvset_panel(void) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_intbox_widget_t        ib;
  xitk_checkbox_widget_t      cb;
  xitk_combo_widget_t         cmb;
  xitk_inputtext_widget_t     inp;
  xitk_pixmap_t              *bg;
  int                         i;
  int                         x, y, w, width, height;
  xitk_widget_t              *widget;

  tvset = (_tvset_t *) xine_xmalloc(sizeof(_tvset_t));
  
  x = xine_config_register_num (gGui->xine, "gui.tvset_x", 
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.tvset_y",
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  /* Create window */
  tvset->xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
						 _("TV/Analog Video parameters"), x, y,
						 WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(tvset->xwin)), None, None);
  XUnlockDisplay (gGui->display);

  tvset->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(tvset->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(tvset->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(tvset->xwin)));
  xitk_widget_list_set(tvset->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  xitk_window_get_window_size(tvset->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(tvset->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay (gGui->display);
  
  x = 5;
  y = 35;
  
  draw_outter_frame(gGui->imlib_data, bg, _("General"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + 15 + 5);


  /* First Line */
  x = 15;
  y += 15;
  w = 130;
  draw_inner_frame(gGui->imlib_data, bg, _("Input: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);
  XITK_WIDGET_INIT(&ib, gGui->imlib_data);
  ib.skin_element_name = NULL;
  ib.value             = 4;
  ib.step              = 1;
  ib.parent_wlist      = tvset->widget_list;
  ib.callback          = NULL;
  ib.userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(tvset->widget_list)), 
			   (tvset->input = 
			    xitk_noskin_intbox_create(tvset->widget_list, &ib,
						     x, y, w, 20, NULL, NULL, NULL)));
  xitk_enable_and_show_widget(tvset->input);

  for(i = 0; chanlists[i].name; i++) ;
  tvset->system_entries = (char **) xine_xmalloc(sizeof(char *) * (i+1));
  
  for(i = 0; chanlists[i].name; i++)
    tvset->system_entries[i] = strdup(chanlists[i].name);
  tvset->system_entries[i] = NULL;

  x += w + 15;
  w = 150;
  draw_inner_frame(gGui->imlib_data, bg, _("Broadcast system: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = tvset->widget_list;
  cmb.entries           = tvset->system_entries;
  cmb.parent_wkey       = &tvset->widget_key;
  cmb.callback          = system_combo_select;
  cmb.userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(tvset->widget_list), 
		   (tvset->system = 
		    xitk_noskin_combo_create(tvset->widget_list, &cmb,
					     x, y, w, NULL, NULL)));
  xitk_enable_and_show_widget(tvset->system);

  xitk_combo_set_select(tvset->system, 0);
  update_chann_entries(0);

  x += w + 15;
  w = 150;
  draw_inner_frame(gGui->imlib_data, bg, _("Channel: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = tvset->widget_list;
  cmb.entries           = tvset->chann_entries;
  cmb.parent_wkey       = &tvset->widget_key;
  cmb.callback          = NULL;
  cmb.userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(tvset->widget_list), 
		   (tvset->chann = 
		    xitk_noskin_combo_create(tvset->widget_list, &cmb,
					     x, y, w, NULL, NULL)));
  xitk_enable_and_show_widget(tvset->chann);

  x = 5;
  y += 45;
  draw_outter_frame(gGui->imlib_data, bg, _("Standard"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + 15 + 5);


  x = 15;
  y += 15;
  w = 130;
  draw_inner_frame(gGui->imlib_data, bg, _("Frame Rate: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);

  XITK_WIDGET_INIT(&inp, gGui->imlib_data);
  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = 20;
  inp.callback          = NULL;
  inp.userdata          = NULL;
  xitk_list_append_content (XITK_WIDGET_LIST_LIST(tvset->widget_list),
	   (tvset->framerate = 
	    xitk_noskin_inputtext_create(tvset->widget_list, &inp,
					 x, y, w, 20,
					 "Black", "Black", tvsetfontname)));
  xitk_enable_and_show_widget(tvset->framerate);

  tvset->vidstd_entries = (char **) xine_xmalloc(sizeof(char *) * 
                          (sizeof(std_list)/sizeof(std_list[0])+1));
  
  for(i = 0; i < (sizeof(std_list)/sizeof(std_list[0])); i++)
    tvset->vidstd_entries[i] = strdup(std_list[i].name);
  tvset->vidstd_entries[i] = NULL;

  x += w + 15;
  w = 150;
  draw_inner_frame(gGui->imlib_data, bg, _("Analog Standard: "), lfontname, 
		    x - 5, y - 2, w + 10, 20 + 15);

  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  cmb.skin_element_name = NULL;
  cmb.parent_wlist      = tvset->widget_list;
  cmb.entries           = tvset->vidstd_entries;
  cmb.parent_wkey       = &tvset->widget_key;
  cmb.callback          = NULL;
  cmb.userdata          = NULL;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(tvset->widget_list), 
		   (tvset->vidstd = 
		    xitk_noskin_combo_create(tvset->widget_list, &cmb,
					     x, y, w, NULL, NULL)));
  xitk_enable_and_show_widget(tvset->vidstd);

  x = 5;
  y += 45;
  draw_outter_frame(gGui->imlib_data, bg, _("Frame Size"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + 15 + 5);

  x = 15;
  y += 15;
  w = 130;



  x = 5;
  y += 45;
  draw_outter_frame(gGui->imlib_data, bg, _("MPEG2"), btnfontname, 
		    x, y, WINDOW_WIDTH - 10, (2 * 20) + 15 + 5);


  x = 15;
  y += 15;
  w = 130;

  /*  */
  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Update");
  lb.align             = ALIGN_CENTER;
  lb.callback          = tvset_update; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(tvset->widget_list)), 
	   (tvset->update = 
	    xitk_noskin_labelbutton_create(tvset->widget_list, 
					   &lb, x, y, 100, 23,
					   "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(tvset->update);
 
  x = WINDOW_WIDTH - 115;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Close");
  lb.align             = ALIGN_CENTER;
  lb.callback          = tvset_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(tvset->widget_list)), 
   (widget = xitk_noskin_labelbutton_create(tvset->widget_list, 
					    &lb, x, y, 100, 23,
					    "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(widget);
  
  xitk_window_change_background(gGui->imlib_data, tvset->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);

  tvset->widget_key = xitk_register_event_handler("tvset", 
						   (xitk_window_get_window(tvset->xwin)),
						   NULL,
						   NULL,
						   NULL,
						   tvset->widget_list,
						   NULL);
  
  tvset->visible = 1;
  tvset->running = 1;
  tvset_raise_window();


  while(!xitk_is_window_visible(gGui->display, xitk_window_get_window(tvset->xwin)))
    xine_usec_sleep(5000);

  XLockDisplay (gGui->display);
  XSetInputFocus(gGui->display, xitk_window_get_window(tvset->xwin), RevertToParent, CurrentTime);
  XUnlockDisplay (gGui->display);
}
