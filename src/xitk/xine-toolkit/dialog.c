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

#include <X11/keysym.h>

#include "font.h"

#include "_xitk.h"

#define TITLE_BAR_HEIGHT 20

#define _XITK_VASPRINTF(_target_ptr,_fmt) \
  if (_fmt) { \
    va_list args; \
    va_start (args, _fmt); \
    _target_ptr = xitk_vasprintf (_fmt, args); \
    va_end (args); \
  } else { \
    _target_ptr = NULL; \
  }


/*
 * Dialog window
 */

typedef struct xitk_dialog_s xitk_dialog_t;

struct xitk_dialog_s {
  xitk_window_t          *xwin;
  xitk_register_key_t     key;
  int                     type;

  xitk_widget_t          *w1, *w2, *w3;
  void                  (*done3cb)(void *userdata, int state);
  void                   *done3data;

  xitk_widget_t          *checkbox;
  xitk_widget_t          *checkbox_label;

  xitk_widget_t          *default_button;
};

/* NOTE: this will free (text). */
static xitk_dialog_t *_xitk_dialog_new (xitk_t *xitk,
  const char *title, char *text, int *width, int *height, int align) {
  xitk_dialog_t *wd;
  xitk_image_t *image;

  if (!text)
    return NULL;

  if (!xitk || !*width) {
    free (text);
    return NULL;
  }

  wd = xitk_xmalloc (sizeof (*wd));
  if (!wd) {
    free (text);
    return NULL;
  }

  image = xitk_image_create_image_from_string (xitk, DEFAULT_FONT_12, *width - 40, align, text);
  free (text);
  if (!image) {
    free (wd);
    return NULL;
  }

  *height += image->height;
  wd->xwin = xitk_window_create_dialog_window (xitk, title, 0, 0, *width, *height);
  if (!wd->xwin) {
    xitk_image_free_image (&image);
    free (wd);
    return NULL;
  }
  wd->w1 = wd->w2 = wd->w3 = NULL;
  wd->default_button = NULL;

  /* Draw text area */
  {
    xitk_pixmap_t  *bg;

    bg = xitk_window_get_background_pixmap (wd->xwin);
    if (bg) {
      xitk_pixmap_copy_area(image->image, bg, 0, 0, image->width, image->height, 20, TITLE_BAR_HEIGHT + 20);
      xitk_window_set_background (wd->xwin, bg);
    }
  }
  xitk_image_free_image (&image);

  xitk_window_center_window (wd->xwin);
  
  return wd;
}

static void _xitk_window_dialog_3_destr (void *data) {
  xitk_dialog_t *wd = data;

  xitk_window_destroy_window (wd->xwin);
  XITK_FREE (wd);
}

static void _xitk_window_dialog_3_done (xitk_widget_t *w, void *data) {
  xitk_dialog_t *wd = data;
  xitk_register_key_t key;

  if (wd->done3cb) {
    int state = !w ? 0
              : w == wd->w1 ? 1
              : w == wd->w2 ? 2
              : /* w == wd->w3 */ 3;
    if (wd->checkbox && xitk_checkbox_get_state (wd->checkbox))
      state += XITK_WINDOW_DIALOG_CHECKED;
    wd->done3cb (wd->done3data, state);
  }

  /* this will _xitk_window_dialog_3_destr (wd). */
  key = wd->key;
  xitk_unregister_event_handler (&key);
}

static void _checkbox_label_click (xitk_widget_t *w, void *data) {
  xitk_dialog_t *wd = (xitk_dialog_t *) data;

  (void)w;
  xitk_checkbox_set_state (wd->checkbox, !xitk_checkbox_get_state (wd->checkbox));
  xitk_checkbox_callback_exec (wd->checkbox);
}

/*
 * Local XEvent handling.
 */
static void _dialog_window_handle_expose_event (void *data, const xitk_expose_event_t *ee) {
  xitk_dialog_t *wd = data;

  if (wd->default_button) {
    xitk_widget_list_t *widget_list = xitk_window_widget_list(wd->xwin);
    widget_list->widget_focused = wd->default_button;
    if ((widget_list->widget_focused->type & WIDGET_FOCUSABLE) &&
        (widget_list->widget_focused->enable == WIDGET_ENABLE)) {
      xitk_widget_t *w = widget_list->widget_focused;
      widget_event_t event;
      event.type = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_RECEIVED;
      w->event (w, &event, NULL);
      w->have_focus = FOCUS_RECEIVED;
      event.type = WIDGET_EVENT_PAINT;
      w->event (w, &event, NULL);
    }
  }
}

static void _dialog_window_handle_key_event (void *data, const xitk_key_event_t *ke) {
  xitk_dialog_t *wd = data;

  if (ke->event == XITK_KEY_PRESS) {
    switch (ke->key_pressed) {
      case XK_Return:
        if (wd->default_button)
          _xitk_window_dialog_3_done (wd->default_button, wd);
        break;

      case XK_Escape:
        if (wd->default_button)
          _xitk_window_dialog_3_done (wd->default_button, wd);
        break;
    }
  }
}

static const xitk_event_cbs_t _dialog_event_cbs = {
  .key_cb           = _dialog_window_handle_key_event,
  .expose_notify_cb = _dialog_window_handle_expose_event,
};

static const char *_xitk_window_dialog_label (const char *label) {
  switch ((uintptr_t)label) {
    case (uintptr_t)XITK_LABEL_OK: return _("OK");
    case (uintptr_t)XITK_LABEL_NO: return _("No");
    case (uintptr_t)XITK_LABEL_YES: return _("Yes");
    case (uintptr_t)XITK_LABEL_CANCEL: return _("Cancel");
    default: return label;
  }
}


xitk_register_key_t xitk_window_dialog_3 (xitk_t *xitk, xitk_window_t *transient_for, int layer_above,
  int width, const char *title,
  void (*done_cb)(void *userdata, int state), void *userdata,
  const char *button1_label, const char *button2_label, const char *button3_label,
  const char *check_label, int checked,
  int text_align, const char *text_fmt, ...) {
  xitk_dialog_t *wd;
  int num_buttons = !!button1_label + !!button2_label +!!button3_label;
  int winw = width, winh = TITLE_BAR_HEIGHT + 40;

  if (num_buttons)
    winh += 50;
  if (check_label)
    winh += 50;
  {
    const char *_title = title ? title : (num_buttons < 2) ? "Notice" : _("Question?");
    char *text;
    _XITK_VASPRINTF (text, text_fmt);
    wd = _xitk_dialog_new (xitk, _title, text, &winw, &winh, text_align);
    if (!wd)
      return 0;
  }
  wd->type = 33;

  if (check_label) {
    xitk_widget_list_t *widget_list = xitk_window_widget_list (wd->xwin);
    int x = 25, y = winh - (num_buttons ? 50 : 0) - 50;
    xitk_checkbox_widget_t cb;
    xitk_label_widget_t lbl;
  
    XITK_WIDGET_INIT(&cb);
    XITK_WIDGET_INIT(&lbl);
  
    cb.skin_element_name = NULL;
    cb.callback          = NULL;
    cb.userdata          = NULL;
    wd->checkbox = xitk_noskin_checkbox_create (widget_list, &cb, x, y + 5, 10, 10);
    if (wd->checkbox) {
      xitk_dlist_add_tail (&widget_list->list, &wd->checkbox->node);
      xitk_checkbox_set_state (wd->checkbox, checked);
    }

    lbl.skin_element_name = NULL;
    lbl.label             = check_label;
    lbl.callback          = _checkbox_label_click;
    lbl.userdata          = wd;
    wd->checkbox_label = xitk_noskin_label_create (widget_list, &lbl, x + 15, y, winw - x - 40, 20, DEFAULT_FONT_12);
    if (wd->checkbox_label)
      xitk_dlist_add_tail (&widget_list->list, &wd->checkbox_label->node);
  }

  wd->done3cb = done_cb;
  wd->done3data = userdata;

  if (num_buttons) {
    xitk_widget_list_t *widget_list = xitk_window_widget_list (wd->xwin);
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
      wd->w1 = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w1)
        xitk_dlist_add_tail (&widget_list->list, &wd->w1->node);
      bx += bdx;
    }

    if (button2_label) {
      lb.label = _xitk_window_dialog_label (button2_label);
      wd->w2 = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w2)
        xitk_dlist_add_tail (&widget_list->list, &wd->w2->node);
      bx += bdx;
    }

    if (button3_label) {
      lb.label = _xitk_window_dialog_label (button3_label);
      wd->w3 = xitk_noskin_labelbutton_create (widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w3)
        xitk_dlist_add_tail (&widget_list->list, &wd->w3->node);
    }
  }
  wd->default_button = wd->w3;
  if (!wd->default_button) {
    wd->default_button = wd->w1;
    if (!wd->default_button)
      wd->default_button = wd->w2;
  }

  xitk_window_show_window(wd->xwin, 1);

  if (wd->w1)
    xitk_enable_and_show_widget (wd->w1);
  if (wd->w2)
    xitk_enable_and_show_widget (wd->w2);
  if (wd->w3)
    xitk_enable_and_show_widget (wd->w3);
  if (check_label) {
    xitk_enable_and_show_widget (wd->checkbox);
    xitk_enable_and_show_widget (wd->checkbox_label);
  }

  if (transient_for) {
    xitk_window_set_parent_window (wd->xwin, transient_for);
    xitk_window_set_transient_for_win(wd->xwin, transient_for);
  }
  if (layer_above)
    xitk_window_set_window_layer (wd->xwin, layer_above);
  xitk_window_try_to_set_input_focus (wd->xwin);

  wd->key = xitk_window_register_event_handler ("xitk_dialog_3", wd->xwin, &_dialog_event_cbs, wd);
  xitk_register_eh_destructor (wd->key, _xitk_window_dialog_3_destr, wd);
  return wd->key;
}