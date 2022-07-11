/*
 * Copyright (C) 2000-2022 the xine project
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

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "common.h"
#include "event_sender.h"
#include "panel.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/labelbutton.h"

#define WINDOW_WIDTH    250
#define WINDOW_HEIGHT   221

static const struct {
  uint8_t x, y, w, h;
  int type;
  const char *label;
} _es_event_types[] = {
  {  5,  5, 80, 23, XINE_EVENT_INPUT_MENU1, "" },
  {  5, 28, 80, 23, XINE_EVENT_INPUT_MENU2, "" },
  {  5, 52, 80, 23, XINE_EVENT_INPUT_MENU3, "" },
  { 85,  5, 80, 23, XINE_EVENT_INPUT_MENU4, "" },
  {165,  5, 80, 23, XINE_EVENT_INPUT_MENU5, "" },
  {165, 28, 80, 23, XINE_EVENT_INPUT_MENU6, "" },
  {165, 52, 80, 23, XINE_EVENT_INPUT_MENU7, "" },
  { 57,124, 23, 23, XINE_EVENT_INPUT_NUMBER_9, "9" },
  { 34,124, 23, 23, XINE_EVENT_INPUT_NUMBER_8, "8" },
  { 11,124, 23, 23, XINE_EVENT_INPUT_NUMBER_7, "7" },
  { 57,147, 23, 23, XINE_EVENT_INPUT_NUMBER_6, "6" },
  { 34,147, 23, 23, XINE_EVENT_INPUT_NUMBER_5, "5" },
  { 11,147, 23, 23, XINE_EVENT_INPUT_NUMBER_4, "4" },
  { 57,170, 23, 23, XINE_EVENT_INPUT_NUMBER_3, "3" },
  { 34,170, 23, 23, XINE_EVENT_INPUT_NUMBER_2, "2" },
  { 11,170, 23, 23, XINE_EVENT_INPUT_NUMBER_1, "1" },
  { 11,193, 23, 23, XINE_EVENT_INPUT_NUMBER_0, "0" },
  { 34,193, 46, 23, XINE_EVENT_INPUT_NUMBER_10_ADD, "+10" },
  { 90, 39, 70, 40, XINE_EVENT_INPUT_UP,     N_("Up") },
  { 20, 79, 70, 40, XINE_EVENT_INPUT_LEFT,   N_("Left") },
  { 95, 84, 60, 30, XINE_EVENT_INPUT_SELECT, N_("Select") },
  {160, 79, 70, 40, XINE_EVENT_INPUT_RIGHT,  N_("Right") },
  { 90,119, 70, 40, XINE_EVENT_INPUT_DOWN,   N_("Down") },
  {165,124, 80, 23, XINE_EVENT_INPUT_ANGLE_NEXT,     N_("Angle +") },
  {165,147, 80, 23, XINE_EVENT_INPUT_ANGLE_PREVIOUS, N_("Angle -") }
};

struct xui_event_sender_st {
  struct xui_event_sender_st *self[sizeof (_es_event_types) / sizeof (_es_event_types[0])];
  gGui_t               *gui;

  xitk_window_t        *xwin;

  int                   x;
  int                   y;

  xitk_widget_list_t   *widget_list;

  xitk_widget_t        *wdgts[sizeof (_es_event_types) / sizeof (_es_event_types[0])];

  int                   visible;
  xitk_register_key_t   widget_key;

};

void event_sender_sticky_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  int old_sticky_value = gui->eventer_sticky;

  gui->eventer_sticky = cfg->num_value;
  if (gui->eventer) {
    if ((!old_sticky_value) && gui->eventer_sticky) {
      xitk_rect_t pr = {0, 0, 0, 0};

      panel_get_window_position (gui->panel, &pr);
      pr.x += pr.width; pr.width = XITK_INT_KEEP; pr.height = XITK_INT_KEEP;
      gui->eventer->x = pr.x;
      gui->eventer->y = pr.y;
      xitk_window_move_resize (gui->eventer->xwin, &pr);
    }
  }
}

/* Send given event to xine engine */
void event_sender_send (gGui_t *gui, int event) {
  xine_event_t   xine_event;

  if (!gui || !event)
    return;

  xine_event.type        = event;
  xine_event.data_length = 0;
  xine_event.data        = NULL;
  xine_event.stream      = gui->stream;
  gettimeofday (&xine_event.tv, NULL);
  xine_event_send (gui->stream, &xine_event);
}

static void event_sender_send2 (xitk_widget_t *w, void *data, int state) {
  xui_event_sender_t **p = data, *es = *p;
  int event = p - es->self;
  xine_event_t   xine_event;

  (void)w;
  (void)state;
  xine_event.type        = _es_event_types[event].type;
  xine_event.data_length = 0;
  xine_event.data        = NULL;
  xine_event.stream      = es->gui->stream;
  gettimeofday (&xine_event.tv, NULL);
  xine_event_send (es->gui->stream, &xine_event);
}

static void event_sender_exit (xitk_widget_t *w, void *data, int state);

static int event_sender_event (void *data, const xitk_be_event_t *e) {
  xui_event_sender_t *es = data;
  xine_event_t xine_event;

  xine_event.type = -1;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      event_sender_exit (NULL, es, 0);
      return 1;
    case XITK_EV_BUTTON_UP:
      /* If we tried to move sticky window, move it back to stored position. */
      if (es->gui->eventer_sticky) {
        if (panel_is_visible (es->gui->panel) > 1) {
          xitk_rect_t wr = {0, 0, 0, 0};

          xitk_window_get_window_position (es->xwin, &wr);
          if ((wr.x != es->x) || (wr.y != es->y)) {
            wr.x = es->x, wr.y = es->y, wr.width = XITK_INT_KEEP, wr.height = XITK_INT_KEEP;
            xitk_window_move_resize (es->xwin, &wr);
          }
        }
      }
      return 1;
    case XITK_EV_KEY_DOWN:
      switch (e->utf8[0]) {
        case XITK_CTRL_KEY_PREFIX:
          switch (e->utf8[1]) {
            case XITK_KEY_UP:    xine_event.type = XINE_EVENT_INPUT_UP; break;
            case XITK_KEY_DOWN:  xine_event.type = XINE_EVENT_INPUT_DOWN; break;
            case XITK_KEY_LEFT:  xine_event.type = XINE_EVENT_INPUT_LEFT; break;
            case XITK_KEY_RIGHT: xine_event.type = XINE_EVENT_INPUT_RIGHT; break;
            case XITK_KEY_RETURN: xine_event.type = XINE_EVENT_INPUT_SELECT; break;
            case XITK_KEY_ESCAPE:
              event_sender_exit (NULL, es, 0);
              return 1;
            default: ;
          }
          break;
        case '+': xine_event.type = XINE_EVENT_INPUT_NUMBER_10_ADD; break;
        case '0': xine_event.type = XINE_EVENT_INPUT_NUMBER_0; break;
        case '1': xine_event.type = XINE_EVENT_INPUT_NUMBER_1; break;
        case '2': xine_event.type = XINE_EVENT_INPUT_NUMBER_2; break;
        case '3': xine_event.type = XINE_EVENT_INPUT_NUMBER_3; break;
        case '4': xine_event.type = XINE_EVENT_INPUT_NUMBER_4; break;
        case '5': xine_event.type = XINE_EVENT_INPUT_NUMBER_5; break;
        case '6': xine_event.type = XINE_EVENT_INPUT_NUMBER_6; break;
        case '7': xine_event.type = XINE_EVENT_INPUT_NUMBER_7; break;
        case '8': xine_event.type = XINE_EVENT_INPUT_NUMBER_8; break;
        case '9': xine_event.type = XINE_EVENT_INPUT_NUMBER_9; break;
      }
    default: ;
  }
  if (xine_event.type >= 0) {
    xine_event.data_length = 0;
    xine_event.data        = NULL;
    xine_event.stream      = es->gui->stream;
    gettimeofday (&xine_event.tv, NULL);
    xine_event_send (es->gui->stream, &xine_event);
    return 1;
  }
  return gui_handle_be_event (es->gui, e);
}

void event_sender_update_menu_buttons (gGui_t *gui) {
  if (gui && gui->eventer) {
    if ((!strncmp (gui->mmk.mrl, "dvd:/", 5)) || (!strncmp (gui->mmk.mrl, "dvdnav:/", 8))) {
      xitk_labelbutton_change_label (gui->eventer->wdgts[0], _("Menu toggle"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[1], _("Title"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[2], _("Root"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[3], _("Subpicture"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[4], _("Audio"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[5], _("Angle"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[6], _("Part"));
    }
    else if (!strncmp (gui->mmk.mrl, "bd:/", 4)) {
      xitk_labelbutton_change_label (gui->eventer->wdgts[0], _("Top Menu"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[1], _("Popup Menu"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[2], _("Menu 3"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[3], _("Menu 4"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[4], _("Menu 5"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[5], _("Menu 6"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[6], _("Menu 7"));
    }
    else if (!strncmp (gui->mmk.mrl, "xvdr", 4)) {
      xitk_labelbutton_change_label (gui->eventer->wdgts[0], _("Menu"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[1], _("Red"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[2], _("Green"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[3], _("Yellow"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[4], _("Blue"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[5], _("Menu 6"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[6], _("Menu 7"));
    } else {
      xitk_labelbutton_change_label (gui->eventer->wdgts[0], _("Menu 1"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[1], _("Menu 2"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[2], _("Menu 3"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[3], _("Menu 4"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[4], _("Menu 5"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[5], _("Menu 6"));
      xitk_labelbutton_change_label (gui->eventer->wdgts[6], _("Menu 7"));
    }
  }
}

static void event_sender_exit (xitk_widget_t *w, void *data, int state) {
  xui_event_sender_t *es = data;

  (void)w;
  (void)state;
  if (es) {
    es->visible = 0;

    gui_save_window_pos (es->gui, "eventer", es->widget_key);

    xitk_unregister_event_handler (es->gui->xitk, &es->widget_key);
    xitk_window_destroy_window (es->xwin);
    es->xwin = NULL;
    es->gui->eventer = NULL;
    video_window_set_input_focus (es->gui->vwin);
    free (es);
  }
}

int event_sender_is_visible (gGui_t *gui) {
  if (!gui)
    return 0;
  if (!gui->eventer)
    return 0;
  return gui->use_root_window ? (xitk_window_flags (gui->eventer->xwin, 0, 0) & XITK_WINF_VISIBLE)
                              : (gui->eventer->visible && (xitk_window_flags (gui->eventer->xwin, 0, 0) & XITK_WINF_VISIBLE));
}

static void _event_sender_raise_window (gGui_t *gui) {
  if (gui && gui->eventer)
    raise_window (gui, gui->eventer->xwin, gui->eventer->visible, 1);
}

void event_sender_toggle_visibility (gGui_t *gui) {
  if (gui && gui->eventer)
    toggle_window (gui, gui->eventer->xwin, gui->eventer->widget_list, &gui->eventer->visible, 1);
}

void event_sender_move (gGui_t *gui, int x, int y) {
  if (gui && gui->eventer) {
    if (gui->eventer_sticky) {
      xitk_rect_t wr = {x, y, XITK_INT_KEEP, XITK_INT_KEEP};

      gui->eventer->x = x;
      gui->eventer->y = y;
      config_update_num (gui->xine, "gui.eventer_x", x);
      config_update_num (gui->xine, "gui.eventer_y", y);
      xitk_window_move_resize (gui->eventer->xwin, &wr);
    }
  }
}

void event_sender_end (gGui_t *gui) {
  if (gui && gui->eventer)
    event_sender_exit (NULL, gui->eventer, 0);
}


void event_sender_panel (gGui_t *gui) {
  int                        x, y, i;
  xitk_widget_t             *w;
  xui_event_sender_t        *es;

  if (!gui)
    return;
  if (gui->eventer)
    return;

  es = (xui_event_sender_t *)calloc (1, sizeof (*es));
  if (!es)
    return;

  es->gui = gui;
  for (i = 0; i < (int)(sizeof (es->self) / sizeof (es->self[0])); i++)
    es->self[i] = es;

  es->x = es->y = 80;
  gui_load_window_pos (es->gui, "eventer", &es->x, &es->y);

  if (es->gui->eventer_sticky && panel_is_visible (es->gui->panel) > 1) {
    xitk_rect_t pr = {0, 0, 0, 0};
    panel_get_window_position (es->gui->panel, &pr);
    es->x = pr.x + pr.width;
    es->y = pr.y;
  }

  /* Create window */
  es->xwin = xitk_window_create_simple_window_ext (es->gui->xitk, es->x, es->y,
    WINDOW_WIDTH, WINDOW_HEIGHT, _("xine Event Sender"), NULL, "xine", 0, 0, es->gui->icon);
  if (!es->xwin) {
    free (es);
    return;
  }
  gui->eventer = es;
  set_window_type_start (es->gui, es->xwin);

  es->widget_list = xitk_window_widget_list (es->xwin);

  {
    xitk_labelbutton_widget_t lb;
    int i;
    XITK_WIDGET_INIT (&lb);

    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.callback          = event_sender_send2;
    lb.state_callback    = NULL;
    lb.skin_element_name = NULL;

    for (i = 0; i < 18; i++) {
      lb.label    = _es_event_types[i].label;
      lb.userdata = es->self + i;
      es->wdgts[i] = xitk_noskin_labelbutton_create (es->widget_list, &lb,
        _es_event_types[i].x, _es_event_types[i].y,
        _es_event_types[i].w, _es_event_types[i].h,
        XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, hboldfontname);
      xitk_add_widget (es->widget_list, es->wdgts[i], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    }

    for (; i < (int)(sizeof (_es_event_types) / sizeof (_es_event_types[0])); i++) {
      lb.label    = gettext (_es_event_types[i].label);
      lb.userdata = es->self + i;
      es->wdgts[i] = xitk_noskin_labelbutton_create (es->widget_list, &lb,
        _es_event_types[i].x, _es_event_types[i].y,
        _es_event_types[i].w, _es_event_types[i].h,
        XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, hboldfontname);
      xitk_add_widget (es->widget_list, es->wdgts[i], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    }

    event_sender_update_menu_buttons (es->gui);

    x = WINDOW_WIDTH - 70 - 5;
    y = WINDOW_HEIGHT - 23 - 5;
    lb.label    = _("Close");
    lb.callback = event_sender_exit;
    lb.userdata = es;
    w = xitk_noskin_labelbutton_create (es->widget_list,
      &lb, x, y, 70, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, hboldfontname);
    xitk_add_widget (es->widget_list, w, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  }

  es->widget_key = xitk_be_register_event_handler ("eventer", es->xwin, event_sender_event, es, NULL, NULL);

  es->visible = 1;
  _event_sender_raise_window (es->gui);
  xitk_window_set_input_focus (es->xwin);
}
