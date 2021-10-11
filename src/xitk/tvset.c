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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "tvset.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/combo.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/intbox.h"

#include "frequencies.h"

#define WINDOW_WIDTH    500
#define WINDOW_HEIGHT   346


struct xui_tvset_s {
  gGui_t               *gui;

  xitk_window_t        *xwin;

  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *input;
  xitk_widget_t        *system;
  xitk_widget_t        *chann;
  xitk_widget_t        *framerate;
  xitk_widget_t        *vidstd;

  xitk_widget_t        *close;
  xitk_widget_t        *update;

  const char          **system_entries;
  const char          **chann_entries;
  const char          **vidstd_entries;

  int                   visible;
  xitk_register_key_t   widget_key;

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

static const struct {
  char     name[24];
  uint64_t std;
} std_list[] = {
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


static void tvset_update (xitk_widget_t *w, void *data, int state) {
  xui_tvset_t *tvset = data;
  xine_event_t          xine_event;
  xine_set_v4l2_data_t *ev_data;
  int current_system, current_chan, current_std;

  (void)w;
  (void)state;
  current_system = xitk_combo_get_current_selected (tvset->system);
  current_chan   = xitk_combo_get_current_selected (tvset->chann);
  current_std    = xitk_combo_get_current_selected (tvset->vidstd);

  if (current_system < 0 || (size_t)current_system >= sizeof(chanlists) / sizeof(chanlists[0])) {
    current_system = 0;
  }
  if (current_chan < 0 || current_chan >= chanlists[current_system].count) {
    current_chan = 0;
  }
  if (current_std < 0 || (size_t)current_std >= sizeof(std_list) / sizeof(std_list[0])) {
    current_std = 0;
  }

  ev_data = (xine_set_v4l2_data_t *)malloc(sizeof(xine_set_v4l2_data_t));
  if (!ev_data)
    return;

  ev_data->input     = xitk_intbox_get_value (tvset->input);
  ev_data->frequency = (chanlists[current_system].list[current_chan].freq * 16) / 1000;

  ev_data->standard_id = std_list[current_std].std;

  xine_event.type        = XINE_EVENT_SET_V4L2;
  xine_event.data_length = sizeof(xine_set_v4l2_data_t);
  xine_event.data        = ev_data;
  xine_event.stream      = tvset->gui->stream;
  gettimeofday(&xine_event.tv, NULL);

  xine_event_send (tvset->gui->stream, &xine_event);
}

static void tvset_exit (xitk_widget_t *w, void *data, int state) {
  xui_tvset_t *tvset = data;
  window_info_t wi;

  (void)w;
  (void)state;
  if (!tvset)
    return;

  tvset->visible = 0;

  if ((xitk_get_window_info (tvset->gui->xitk, tvset->widget_key, &wi))) {
    config_update_num (tvset->gui->xine, "gui.tvset_x", wi.x);
    config_update_num (tvset->gui->xine, "gui.tvset_y", wi.y);
  }

  tvset->gui->tvset = NULL;

  xitk_unregister_event_handler (tvset->gui->xitk, &tvset->widget_key);
  xitk_window_destroy_window (tvset->xwin);
  tvset->xwin = NULL;
  /* xitk_dlist_init (&tvset->widget_list->list); */

  free (tvset->system_entries);
  tvset->system_entries = NULL;
  free (tvset->chann_entries);
  tvset->chann_entries = NULL;
  free (tvset->vidstd_entries);
  tvset->vidstd_entries = NULL;

  video_window_set_input_focus (tvset->gui->vwin);

  free (tvset);
}

static int tvset_event (void *data, const xitk_be_event_t *e) {
  xui_tvset_t *tvset = data;

  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    tvset_exit (NULL, tvset, 0);
    return 1;
  }
  return gui_handle_be_event (tvset->gui, e);
}

int tvset_is_visible (xui_tvset_t *tvset) {
  if (!tvset)
    return 0;

  if (tvset->gui->use_root_window)
    return (xitk_window_flags (tvset->xwin, 0, 0) & XITK_WINF_VISIBLE);
  else
    return tvset->visible && (xitk_window_flags (tvset->xwin, 0, 0) & XITK_WINF_VISIBLE);
}

void tvset_raise_window (xui_tvset_t *tvset) {
  if (tvset)
    raise_window (tvset->gui, tvset->xwin, tvset->visible, 1);
}

void tvset_toggle_visibility (xitk_widget_t *w, void *data) {
  gGui_t *gui = data;

  (void)w;
  if (gui) {
    xui_tvset_t *tvset = gui->tvset;

    if (tvset)
      toggle_window (gui, tvset->xwin, tvset->widget_list, &tvset->visible, 1);
  }
}

void tvset_end (xui_tvset_t *tvset) {
  tvset_exit (NULL, tvset, 0);
}

static int update_chann_entries (xui_tvset_t *tvset, int system_entry) {
  int               i;
  const struct CHANLIST *list = chanlists[system_entry].list;
  int               len  = chanlists[system_entry].count;

  free (tvset->chann_entries);

  tvset->chann_entries = (const char **) calloc((len+1), sizeof(const char *));

  for(i = 0; i < len; i++)
    tvset->chann_entries[i] = list[i].name;

  tvset->chann_entries[i] = NULL;
  return len;
}

static void system_combo_select (xitk_widget_t *w, void *data, int select) {
  xui_tvset_t *tvset = data;
  int len;

  (void)w;
  len = update_chann_entries (tvset, select);

  if (tvset->chann) {
    xitk_combo_update_list (tvset->chann, tvset->chann_entries, len);
    xitk_combo_set_select (tvset->chann, 0);
  }
}

void tvset_panel (gGui_t *gui) {
  xui_tvset_t                *tvset;
  xitk_labelbutton_widget_t   lb;
  xitk_intbox_widget_t        ib;
  xitk_combo_widget_t         cmb;
  xitk_inputtext_widget_t     inp;
  xitk_image_t               *bg;
  size_t                      i;
  int                         x, y, w;
  xitk_widget_t              *widget;

  if (gui->tvset)
    return;

  tvset = calloc (1, sizeof (*tvset));
  if (!tvset)
    return;

  tvset->gui = gui;

  x = xine_config_register_num (gui->xine, "gui.tvset_x",
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gui->xine, "gui.tvset_y",
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);

  /* Create window */
  tvset->xwin = xitk_window_create_dialog_window (gui->xitk,
    _("TV Analog Video Parameters"), x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!tvset->xwin) {
    free (tvset);
    return;
  }

  set_window_states_start (gui, tvset->xwin);
  tvset->widget_list = xitk_window_widget_list (tvset->xwin);

  bg = xitk_window_get_background_image (tvset->xwin);

  x = 15;
  y = 34 - 6;
  xitk_image_draw_outter_frame (bg, _("General"), btnfontname,
    x, y, WINDOW_WIDTH - 30, ((20 + 22) + 5 + 2) + 15);

  /* First Line */
  x = 20;
  y += 15;
  w = 139;
  xitk_image_draw_inner_frame (bg, _("Input: "), lfontname, x, y, w, (20 + 22));

  XITK_WIDGET_INIT(&ib);
  ib.skin_element_name = NULL;
  ib.userdata          = tvset;

  ib.fmt               = INTBOX_FMT_DECIMAL;
  ib.min               = 0;
  ib.max               = 0;
  ib.value             = 4;
  ib.step              = 1;
  ib.callback          = NULL;
  tvset->input = xitk_noskin_intbox_create (tvset->widget_list, &ib,
    x + 10, y + 15, w - 20 + 1, 20);
  xitk_add_widget (tvset->widget_list, tvset->input, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  {
    static const size_t chanlists_count = sizeof(chanlists)/sizeof(chanlists[0]);
    tvset->system_entries = (const char **)calloc ((chanlists_count + 1), sizeof (const char *));

    for(i = 0; i < chanlists_count; i++)
      tvset->system_entries[i] = chanlists[i].name;
    tvset->system_entries[i] = NULL;
  }

  x += w + 5;
  w = 155;
  xitk_image_draw_inner_frame (bg, _("Broadcast System: "), lfontname, x, y, w, (20 + 22));

  XITK_WIDGET_INIT (&cmb);
  cmb.skin_element_name = NULL;
  cmb.layer_above       = is_layer_above (gui);
  cmb.parent_wkey       = &tvset->widget_key;
  cmb.userdata          = tvset;

  cmb.entries           = tvset->system_entries;
  cmb.callback          = system_combo_select;
  tvset->system = xitk_noskin_combo_create (tvset->widget_list, &cmb,
    x + 10, y + 15, w - 20 + 1);
  xitk_add_widget (tvset->widget_list, tvset->system, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_set_widget_pos (tvset->system, x + 10, y + 15 + (20 - xitk_get_widget_height (tvset->system)) / 2);

  xitk_combo_set_select (tvset->system, 0);
  update_chann_entries (tvset, 0);

  x += w + 5;
  w = 155;
  xitk_image_draw_inner_frame (bg, _("Channel: "), lfontname, x, y, w, (20 + 22));

  cmb.entries           = tvset->chann_entries;
  cmb.callback          = NULL;
  tvset->chann = xitk_noskin_combo_create (tvset->widget_list, &cmb,
    x + 10, y + 15, w - 20 + 1);
  xitk_add_widget (tvset->widget_list, tvset->chann, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_set_widget_pos (tvset->chann, x + 10, y + 15 + (20 - xitk_get_widget_height (tvset->chann)) / 2);

  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  xitk_image_draw_outter_frame (bg, _("Standard"), btnfontname,
    x, y, WINDOW_WIDTH - 30, ((20 + 22) + 5 + 2) + 15);

  x = 20;
  y += 15;
  w = 139;
  xitk_image_draw_inner_frame (bg, _("Frame Rate: "), lfontname, x, y, w, (20 + 22));

  XITK_WIDGET_INIT(&inp);
  inp.skin_element_name = NULL;
  inp.userdata          = tvset;

  inp.text              = NULL;
  inp.max_length        = 20;
  inp.callback          = NULL;
  tvset->framerate = xitk_noskin_inputtext_create (tvset->widget_list, &inp,
    x + 10, y + 15, w - 20 + 1, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
  xitk_add_widget (tvset->widget_list, tvset->framerate, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  tvset->vidstd_entries = (const char **) malloc(sizeof(const char *) *
                          (sizeof(std_list)/sizeof(std_list[0])+1));

  for(i = 0; i < (sizeof(std_list)/sizeof(std_list[0])); i++)
    tvset->vidstd_entries[i] = std_list[i].name;
  tvset->vidstd_entries[i] = NULL;

  x += w + 5;
  w = 155;
  xitk_image_draw_inner_frame(bg, _("Analog Standard: "), lfontname, x, y, w, (20 + 22));

  cmb.entries           = tvset->vidstd_entries;
  cmb.callback          = NULL;
  tvset->vidstd = xitk_noskin_combo_create (tvset->widget_list, &cmb,
    x + 10, y + 15, w - 20 + 1);
  xitk_add_widget (tvset->widget_list, tvset->vidstd, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  xitk_image_draw_outter_frame (bg, _("Frame Size"), btnfontname,
    x, y, WINDOW_WIDTH - 30, ((20 + 22) + 5 + 2) + 15);

  x = 20;
  y += 15;
  w = 139;

  x = 15;
  y += ((20 + 22) + 5 + 2) + 3;
  xitk_image_draw_outter_frame (bg, _("MPEG2"), btnfontname,
    x, y, WINDOW_WIDTH - 30, ((20 + 22) + 5 + 2) + 15);

  x = 20;
  y += 15;
  w = 139;

  /*  */
  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  XITK_WIDGET_INIT(&lb);
  lb.skin_element_name = NULL;
  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_CENTER;
  lb.state_callback    = NULL;
  lb.userdata          = tvset;

  lb.label             = _("Update");
  lb.callback          = tvset_update;
  tvset->update = xitk_noskin_labelbutton_create (tvset->widget_list,
    &lb, x, y, 100, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (tvset->widget_list, tvset->update, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = WINDOW_WIDTH - (100 + 15);

  lb.label             = _("Close");
  lb.callback          = tvset_exit;
  widget =  xitk_noskin_labelbutton_create (tvset->widget_list,
    &lb, x, y, 100, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (tvset->widget_list, widget, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  xitk_window_set_background_image (tvset->xwin, bg);

  tvset->widget_key = xitk_be_register_event_handler ("tvset", tvset->xwin, tvset_event, tvset, NULL, NULL);
  gui->tvset = tvset;

  tvset->visible = 1;
  tvset_raise_window (tvset);

  xitk_window_set_input_focus (tvset->xwin);
}
