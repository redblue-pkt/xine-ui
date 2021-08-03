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

#include <stdarg.h>
#include <string.h>

#include "_xitk.h"
#include "label.h"
#include "labelbutton.h"
#include "button.h"
#include "default_font.h"

#define TITLE_BAR_HEIGHT 20

/*
 * Dialog window
 */

typedef struct xitk_dialog_s xitk_dialog_t;

typedef enum {
  _W_w1 = 0,
  _W_w2,
  _W_w3,
  _W_checkbox,
  _W_checkbox_label,
  _W_LAST
} _W_t;
  
struct xitk_dialog_s {
  xitk_t                 *xitk;
  xitk_window_t          *xwin;
  xitk_register_key_t     key;
  int                     type;

  xitk_widget_t          *w[_W_LAST];

  void                  (*done3cb)(void *userdata, int state);
  void                   *done3data;

  xitk_widget_t          *default_button;
};

static xitk_dialog_t *_xitk_dialog_new (xitk_t *xitk,
  const char *title, const char *text, int *width, int *height, int align) {
  xitk_dialog_t *wd;
  xitk_image_t *image;

  if (!text)
    return NULL;

  if (!xitk || !*width)
    return NULL;

  wd = xitk_xmalloc (sizeof (*wd));
  if (!wd)
    return NULL;

  wd->xitk = xitk;

  image = xitk_image_create_image_from_string (xitk, DEFAULT_FONT_12, *width - 40, align, text);
  if (!image) {
    free (wd);
    return NULL;
  }

  *height += image->height;
  wd->xwin = xitk_window_create_dialog_window (xitk, title, XITK_XY_CENTER, XITK_XY_CENTER, *width, *height);
  if (!wd->xwin) {
    xitk_image_free_image (&image);
    free (wd);
    return NULL;
  }

  wd->w[_W_w1] = wd->w[_W_w2] = wd->w[_W_w3] = NULL;
  wd->default_button = NULL;

  /* Draw text area */
  {
    xitk_image_t *bg = xitk_window_get_background_image (wd->xwin);
    if (bg) {
      xitk_image_copy_rect (image, bg, 0, 0, image->width, image->height, 20, TITLE_BAR_HEIGHT + 20);
      xitk_window_set_background_image (wd->xwin, bg);
    }
  }
  xitk_image_free_image (&image);

  return wd;
}

static void _xitk_window_dialog_3_destr (void *data) {
  xitk_dialog_t *wd = data;

  xitk_window_destroy_window (wd->xwin);
  XITK_FREE (wd);
}

static void _xitk_window_dialog_3_done (xitk_widget_t *w, void *data, int state) {
  xitk_dialog_t *wd = data;
  xitk_register_key_t key;

  (void)state;

  if (wd->done3cb) {
    int state = !w ? 0
              : w == wd->w[_W_w1] ? 1
              : w == wd->w[_W_w2] ? 2
              : /* w == wd->w[_W_w3] */ 3;
    if (wd->w[_W_checkbox] && xitk_button_get_state (wd->w[_W_checkbox]))
      state += XITK_WINDOW_DIALOG_CHECKED;
    wd->done3cb (wd->done3data, state);
  }

  /* this will _xitk_window_dialog_3_destr (wd). */
  key = wd->key;
  xitk_unregister_event_handler (wd->xitk, &key);
}

/*
 * Local XEvent handling.
 */
static int dialog_event (void *data, const xitk_be_event_t *e) {
  xitk_dialog_t *wd = data;

  if (wd->default_button) {
    if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
      || (e->type == XITK_EV_DEL_WIN)) {
      _xitk_window_dialog_3_done (wd->default_button, wd, 0);
      return 1;
    } else if (e->type == XITK_EV_EXPOSE) {
      xitk_set_focus_to_widget (wd->default_button);
      return 1;
    }
  }
  return 0;
}

static const char *_xitk_window_dialog_label (const char *label) {
  switch ((uintptr_t)label) {
    case (uintptr_t)XITK_LABEL_OK: return _("OK");
    case (uintptr_t)XITK_LABEL_NO: return _("No");
    case (uintptr_t)XITK_LABEL_YES: return _("Yes");
    case (uintptr_t)XITK_LABEL_CANCEL: return _("Cancel");
    default: return label;
  }
}


/* needed to turn button into checkbox */
static void _dialog_dummy (xitk_widget_t *w, void *data, int state) {
  (void)w;
  (void)data;
  (void)state;
}

xitk_register_key_t xitk_window_dialog_3 (xitk_t *xitk, xitk_window_t *transient_for, int layer_above,
  int width, const char *title,
  void (*done_cb)(void *userdata, int state), void *userdata,
  const char *button1_label, const char *button2_label, const char *button3_label,
  const char *check_label, int checked,
  int text_align, const char *text_fmt, ...) {
  xitk_dialog_t *wd;
  xitk_widget_list_t *widget_list;
  int num_buttons = !!button1_label + !!button2_label +!!button3_label;
  int winw = width, winh = TITLE_BAR_HEIGHT + 40;

  if (num_buttons)
    winh += 50;
  if (check_label)
    winh += 50;
  {
    const char *_title = title ? title : (num_buttons < 2) ? "Notice" : _("Question?");
    char *buf;
    const char *text;
    if (text_fmt) {
      va_list args;
      va_start (args, text_fmt);
      if (!strcmp (text_fmt, "%s")) {
        buf = NULL;
        text = va_arg (args, const char *);
      } else {
        text = buf = xitk_vasprintf (text_fmt, args);
      }
      va_end (args);
    } else {
      text = buf = NULL;
    }
    wd = _xitk_dialog_new (xitk, _title, text, &winw, &winh, text_align);
    free (buf);
    if (!wd)
      return 0;
  }
  wd->type = 33;

  if (check_label) {
    xitk_widget_list_t *widget_list = xitk_window_widget_list (wd->xwin);
    int x = 25, y = winh - (num_buttons ? 50 : 0) - 50;
    xitk_button_widget_t b;
    xitk_label_widget_t lbl;

    XITK_WIDGET_INIT (&b);
    XITK_WIDGET_INIT(&lbl);

    b.skin_element_name = "XITK_NOSKIN_CHECK";
    b.callback          = NULL;
    b.state_callback    = _dialog_dummy;
    b.userdata          = NULL;
    wd->w[_W_checkbox] = xitk_noskin_button_create (widget_list, &b, x, y + 5, 10, 10);
    if (wd->w[_W_checkbox]) {
      xitk_dlist_add_tail (&widget_list->list, &wd->w[_W_checkbox]->node);
      xitk_button_set_state (wd->w[_W_checkbox], checked);
    }

    lbl.skin_element_name = NULL;
    lbl.label             = check_label;
    lbl.callback          = NULL;
    lbl.userdata          = NULL;
    wd->w[_W_checkbox_label] = xitk_noskin_label_create (widget_list, &lbl, x + 15, y, winw - x - 40, 20, DEFAULT_FONT_12);
    if (wd->w[_W_checkbox_label])
      xitk_dlist_add_tail (&widget_list->list, &wd->w[_W_checkbox_label]->node);
    xitk_widget_set_focus_redirect (wd->w[_W_checkbox_label], wd->w[_W_checkbox]);
  }

  wd->done3cb = done_cb;
  wd->done3data = userdata;

  widget_list = xitk_window_widget_list (wd->xwin);
  if (num_buttons) {
    xitk_labelbutton_widget_t lb;
    int bx, bdx, by, bwidth = 150;

    if (bwidth > (winw - 8) / num_buttons)
      bwidth = (winw - 8) / num_buttons;
    bx = (winw - bwidth * num_buttons) / (num_buttons + 1);
    bdx = bx + bwidth;
    by = winh - 50;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.callback          = _xitk_window_dialog_3_done;
    lb.state_callback    = NULL;
    lb.userdata          = wd;
    lb.skin_element_name = NULL;

    if (button1_label) {
      lb.label = _xitk_window_dialog_label (button1_label);
      wd->w[_W_w1] = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w[_W_w1])
        xitk_dlist_add_tail (&widget_list->list, &wd->w[_W_w1]->node);
      bx += bdx;
    }

    if (button2_label) {
      lb.label = _xitk_window_dialog_label (button2_label);
      wd->w[_W_w2] = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w[_W_w2])
        xitk_dlist_add_tail (&widget_list->list, &wd->w[_W_w2]->node);
      bx += bdx;
    }

    if (button3_label) {
      lb.label = _xitk_window_dialog_label (button3_label);
      wd->w[_W_w3] = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w[_W_w3])
        xitk_dlist_add_tail (&widget_list->list, &wd->w[_W_w3]->node);
    }
  }
  wd->default_button = wd->w[_W_w3];
  if (!wd->default_button) {
    wd->default_button = wd->w[_W_w1];
    if (!wd->default_button)
      wd->default_button = wd->w[_W_w2];
  }

  xitk_window_flags (wd->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
  xitk_window_raise_window (wd->xwin);

  xitk_widgets_state (wd->w, _W_LAST, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE, ~0u);

  if (transient_for) {
    xitk_window_set_transient_for_win(wd->xwin, transient_for);
  }
  if (layer_above)
    xitk_window_set_window_layer (wd->xwin, layer_above);
  xitk_window_set_input_focus (wd->xwin);

  wd->key = xitk_be_register_event_handler ("xitk_dialog_3", wd->xwin, dialog_event, wd, _xitk_window_dialog_3_destr, wd);
  xitk_widget_list_defferred_destroy (widget_list);
  return wd->key;
}
