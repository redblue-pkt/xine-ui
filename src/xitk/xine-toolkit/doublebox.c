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

#include <stdio.h>
#include <string.h>

#include "_xitk.h"
#include "doublebox.h"
#include "inputtext.h"
#include "button.h"

typedef struct {
  xitk_widget_t                   w;

  char                           *skin_element_name;

  xitk_widget_t                  *input_widget;
  xitk_widget_t                  *more_widget;
  xitk_widget_t                  *less_widget;

  double                          step;
  double                          value;

  xitk_state_double_callback_t    callback;
  void                           *userdata;
} _doublebox_private_t;

static void _db_enability (_doublebox_private_t *wp) {
  if (wp->w.enable == WIDGET_ENABLE) {
    xitk_enable_widget (wp->input_widget);
    xitk_enable_widget (wp->more_widget);
    xitk_enable_widget (wp->less_widget);
  } else {
    xitk_disable_widget (wp->input_widget);
    xitk_disable_widget (wp->more_widget);
    xitk_disable_widget (wp->less_widget);
  }
}

/*
 *
 */
static void _db_destroy (_doublebox_private_t *wp) {
  if (wp->input_widget) {
    xitk_destroy_widget (wp->input_widget);
    wp->input_widget = NULL;
  }
  if (wp->more_widget) {
    xitk_destroy_widget (wp->more_widget);
    wp->more_widget = NULL;
  }
  if (wp->less_widget) {
    xitk_destroy_widget (wp->less_widget);
    wp->less_widget = NULL;
  }
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static void _db_paint (_doublebox_private_t *wp) {
  if (wp->w.visible == 1) {
    int bx, ih, iw;
    iw = xitk_get_widget_width (wp->input_widget);
    ih = xitk_get_widget_height (wp->input_widget);
    xitk_set_widget_pos (wp->input_widget, wp->w.x, wp->w.y);
    bx = wp->w.x + iw;
    xitk_set_widget_pos (wp->more_widget, bx, wp->w.y);
    xitk_set_widget_pos (wp->less_widget, bx, wp->w.y + (ih >> 1));
    xitk_show_widget (wp->input_widget);
    xitk_show_widget (wp->more_widget);
    xitk_show_widget (wp->less_widget);
  } else {
    xitk_hide_widget (wp->input_widget);
    xitk_hide_widget (wp->more_widget);
    xitk_hide_widget (wp->less_widget);
  }
}

/*
 *
 */
static void _db_new_skin (_doublebox_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name) {
#if 0
    int x, y;
    xitk_skin_lock (skonfig);
    /* visibility && enability */
    xitk_set_widget_pos (c, c->x, c->y);
    xitk_get_widget_pos (wp->label_widget, &x, &y);
    x += xitk_get_widget_width (wp->label_widget);
    xitk_set_widget_pos (wp->button_widget, x, y);
    xitk_skin_unlock (skonfig);
#else
    (void)wp;
    (void)skonfig;
#endif
  }
}

static void _db_tips_timeout (_doublebox_private_t *wp, unsigned long timeout) {
  if (wp->input_widget)
    xitk_set_widget_tips_and_timeout (wp->input_widget, wp->w.tips_string, timeout);
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _doublebox_private_t *wp = (_doublebox_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return 0;
  (void)result;
  
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _db_paint (wp);
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _db_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _db_destroy (wp);
      break;
    case WIDGET_EVENT_ENABLE:
      _db_enability (wp);
      break;
    case WIDGET_EVENT_TIPS_TIMEOUT:
      _db_tips_timeout (wp, event->tips_timeout);
      break;
    default: ;
  }
  return 0;
}

/*
 *
 */
static void doublebox_change_value(xitk_widget_t *x, void *data, const char *string) {
  _doublebox_private_t *wp = (_doublebox_private_t *)data;
  char buf[256];

  if (!wp || !string)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return;

  (void)x;
  wp->value = strtod (string, NULL);
  memset (&buf, 0, sizeof (buf));
  snprintf (buf, sizeof (buf), "%lf", wp->value);
  xitk_inputtext_change_text (wp->input_widget, buf);
  if (wp->callback)
    wp->callback (&wp->w, wp->userdata, wp->value);
}

/*
 *
 */
void xitk_doublebox_set_value(xitk_widget_t *w, double value) {
  _doublebox_private_t *wp = (_doublebox_private_t *)w;
  char buf[256];
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return;

  wp->value = value;
  memset (&buf, 0, sizeof (buf));
  snprintf (buf, sizeof (buf), "%lf", wp->value);
  xitk_inputtext_change_text (wp->input_widget, buf);
}

/*
 *
 */
double xitk_doublebox_get_value(xitk_widget_t *w) {
  _doublebox_private_t *wp = (_doublebox_private_t *)w;
  const char *strval;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return 0;

  strval = xitk_inputtext_get_text (wp->input_widget);
  wp->value = strtod (strval, NULL);
  return wp->value;
}

/*
 *
 */
static void doublebox_minus(xitk_widget_t *x, void *data) {
  _doublebox_private_t *wp = (_doublebox_private_t *)data;
  char buf[256];
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return;

  (void)x;
  wp->value -= wp->step;
  memset (&buf, 0, sizeof (buf));
  snprintf (buf, sizeof (buf), "%lf", wp->value);
  xitk_inputtext_change_text (wp->input_widget, buf);
  if (wp->callback)
    wp->callback (&wp->w, wp->userdata, wp->value);
}

/*
 *
 */
static void doublebox_plus(xitk_widget_t *x, void *data) {
  _doublebox_private_t *wp = (_doublebox_private_t *)data;
  char buf[256];
  
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_DOUBLEBOX)
    return;

  (void)x;
  wp->value += wp->step;
  memset (&buf, 0, sizeof (buf));
  snprintf (buf, sizeof (buf), "%lf", wp->value);
  xitk_inputtext_change_text (wp->input_widget, buf);
  if (wp->callback)
    wp->callback (&wp->w, wp->userdata, wp->value);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_doublebox_create (xitk_widget_list_t *wl,
  xitk_doublebox_widget_t *ib, int x, int y, int width, int height) {
  _doublebox_private_t *wp;
  xitk_button_widget_t  b;
  xitk_inputtext_widget_t inp;
  char buf[256];

  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(ib);

  wp = (_doublebox_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&b);
  XITK_WIDGET_INIT(&inp);

  wp->w.x          = x;
  wp->w.y          = y;
  wp->w.width      = width;
  wp->w.height     = height;
  wp->w.enable     = 0;
  wp->w.visible    = 0;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_DOUBLEBOX;
  wp->w.event        = notify_event;

  wp->skin_element_name = NULL;
  wp->callback          = ib->callback;
  wp->userdata          = ib->userdata;
  wp->step              = ib->step;
  wp->value             = ib->value;
  /* Create inputtext and buttons (not skinable) */

  memset (&buf, 0, sizeof (buf));
  snprintf (buf, sizeof (buf), "%lf", ib->value);

  inp.skin_element_name = NULL;
  inp.text              = buf;
  inp.max_length        = 16;
  inp.callback          = doublebox_change_value;
  inp.userdata          = (void *)wp;
  wp->input_widget = xitk_noskin_inputtext_create (wl, &inp,
    x, y, (width - 10), height, "Black", "Black", DEFAULT_FONT_10);
  if (wp->input_widget) {
    xitk_dlist_add_tail (&wl->list, &wp->input_widget->node);
    wp->input_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_DOUBLEBOX;
  }

  b.skin_element_name = "XITK_NOSKIN_PLUS";
  b.callback          = doublebox_plus;
  b.userdata          = (void *)wp;
  wp->more_widget = xitk_noskin_button_create (wl, &b,
    x + width - (height >> 1), y, height >> 1, height >> 1);
  if (wp->more_widget) {
    xitk_dlist_add_tail (&wl->list, &wp->more_widget->node);
    wp->more_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_DOUBLEBOX;
  }

  b.skin_element_name = "XITK_NOSKIN_MINUS";
  b.callback          = doublebox_minus;
  b.userdata          = (void *)wp;
  wp->less_widget = xitk_noskin_button_create (wl, &b,
    x + width - (height >> 1), y + (height >> 1), height >> 1, height >> 1);
  if (wp->less_widget) {
    xitk_dlist_add_tail (&wl->list, &wp->less_widget->node);
    wp->less_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_DOUBLEBOX;
  }

  return &wp->w;
}
