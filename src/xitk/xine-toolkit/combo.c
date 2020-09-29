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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "_xitk.h"
#include "combo.h"
#include "button.h"
#include "label.h"
#include "checkbox.h"
#include "browser.h"
#include "xitk_x11.h"


typedef struct {
  xitk_widget_t           w;

  xitk_short_string_t     skin_element_name;

  xitk_window_t          *xwin;

  int                     win_x;
  int                     win_y;

  xitk_register_key_t    *parent_wkey;

  xitk_widget_t          *combo_widget;
  xitk_widget_t          *label_widget;
  xitk_widget_t          *button_widget;
  xitk_widget_t          *browser_widget;

  char                  **entries;
  int                     num_entries;
  int                     selected;
  int                     sel2;

  xitk_register_key_t     widget_key;
  xitk_widget_list_t     *widget_list;
  xitk_widget_list_t     *parent_wlist;

  xitk_state_callback_t   callback;
  void                   *userdata;
  int                     tips_timeout;

} _combo_private_t;

static char **_combo_copy_string_list (const char * const *s, int *n) {
  const char * const *p;
  char **q, **r, *d;
  size_t _n, _s;

  if (!s)
    return NULL;
  _n = 0;
  _s = 0;
  p = s;
  while ((--*n >= 0) && *p) {
    _s += sizeof (*p) + strlen (*p) + 1;
    _n += 1;
    p++;
  }
  *n = _n;
  r = malloc (_s + sizeof (*p));
  if (!r)
    return NULL;
  p = s;
  q = r;
  d = (char *)(r + _n + 1);
  while (_n--) {
    size_t _l = strlen (*p) + 1;
    *q = d;
    memcpy (d, *p, _l);
    d += _l;
    p++;
    q++;
  }
  *q = NULL;
  return r;
}
    
static void _combo_close (_combo_private_t *wp, int focus) {
  if (wp->xwin) {
    wp->browser_widget = NULL;
    xitk_unregister_event_handler (&wp->widget_key);
    xitk_window_destroy_window (wp->xwin);
    wp->xwin = NULL;
    if (focus) {
      xitk_set_focus_to_widget (wp->button_widget);
      xitk_checkbox_set_state (wp->button_widget, 0);
      xitk_set_focus_to_wl (wp->parent_wlist);
    }
    if (wp->button_widget)
      wp->button_widget->type &= ~WIDGET_KEEP_FOCUS;
  }
}

/*
 * Handle Xevents here.
 */

static void _combo_handle_key_event(void *data, const xitk_key_event_t *ke) {

  _combo_private_t *wp = (_combo_private_t*)data;

    if (ke->key_pressed == XK_Escape)


  if (wp && wp->xwin) {
    if (ke->event == XITK_KEY_PRESS) {
      if (ke->key_pressed == XK_Escape) {
        _combo_close (wp, 1);
      }
#if 0
      else if (ke->key_pressed == XK_Up) {
        if (wp->sel2 > 0)
          xitk_browser_set_select (wp->browser_widget, --wp->sel2);
      } else if (ke->key_pressed == XK_Down) {
        if (wp->sel2 < wp->num_entries)
          xitk_browser_set_select (wp->browser_widget, ++wp->sel2);
      }
#endif
    }
  }
}

static void _combo_handle_button_event(void *data, const xitk_button_event_t *be) {
  _combo_private_t *wp = (_combo_private_t*)data;

  if (wp && wp->xwin) {
    if (be->event == XITK_BUTTON_RELEASE) {
      /* If we try to move the combo window,
       * move it back to right position (under label). */
      int  x, y;
      xitk_window_get_window_position (wp->xwin, &x, &y, NULL, NULL);
      if ((x != wp->win_x) || (y != wp->win_y))
        xitk_combo_update_pos (wp->combo_widget);
    }
  }
}

static const xitk_event_cbs_t combo_event_cbs = {
  .key_cb            = _combo_handle_key_event,
  .btn_cb            = _combo_handle_button_event,
};

static void _combo_select (xitk_widget_t *w, void *data, int selected, int modifier) {
  _combo_private_t *wp = (_combo_private_t *)data;

  if (!wp || !w)
    return;
  if ((w->type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BROWSER)
    return;
  if (modifier & (MODIFIER_SHIFT | MODIFIER_CTRL | MODIFIER_META))
    return;
  if (!wp->xwin)
    return;

  _combo_close (wp, 1);
  wp->selected = selected;
  xitk_label_change_label (wp->label_widget, wp->entries[selected]);
  if (wp->callback)
    wp->callback (wp->combo_widget, wp->userdata, selected);
}

static void _combo_open (_combo_private_t *wp) {
  unsigned int itemw, itemh, slidw;

  if (wp->xwin)
    return;

  itemh = 20;
  slidw = 12;
  itemw = xitk_get_widget_width (wp->label_widget);
  itemw += xitk_get_widget_width (wp->button_widget);
  itemw -= 2; /* space for border */

  wp->xwin = xitk_window_create_simple_window_ext (wp->w.wl->xitk,
    0, 0, itemw + 2, itemh * 5 + slidw + 2,
    NULL, "Xitk Combo", "Xitk", 1, 0, NULL);
  if (!wp->xwin)
    return;

  xitk_window_set_transient_for(wp->xwin, wp->parent_wlist->win);

  wp->widget_list        = xitk_window_widget_list(wp->xwin);

  {
    xitk_browser_widget_t browser;
    /* Browser */
    XITK_WIDGET_INIT (&browser);
    browser.arrow_up.skin_element_name    = NULL;
    browser.slider.skin_element_name      = NULL;
    browser.arrow_dn.skin_element_name    = NULL;
    browser.browser.skin_element_name     = NULL;
    browser.browser.max_displayed_entries = 5;
    browser.browser.num_entries           = wp->num_entries;
    browser.browser.entries               = (const char * const *)wp->entries;
    browser.callback                      = _combo_select;
    browser.dbl_click_callback            = NULL;
    browser.userdata                      = (void *)wp;
    wp->browser_widget = xitk_noskin_browser_create (wp->widget_list, &browser,
      1, 1, itemw - slidw, itemh, slidw, DEFAULT_FONT_10);
    xitk_dlist_add_tail (&wp->widget_list->list, &wp->browser_widget->node);
    xitk_enable_and_show_widget (wp->browser_widget);
    wp->browser_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
  }

  xitk_browser_update_list (wp->browser_widget, 
    (const char * const *)wp->entries, NULL, wp->num_entries, 0);
  wp->sel2 = wp->selected;
  xitk_browser_set_select (wp->browser_widget, wp->selected);

  wp->widget_key = xitk_window_register_event_handler ("xitk combo", wp->xwin, &combo_event_cbs, wp);

  if (wp->button_widget)
    wp->button_widget->type |= WIDGET_KEEP_FOCUS;
}

/*
 *
 */
static void _combo_rollunroll (xitk_widget_t *w, void *data, int state) {
  _combo_private_t *wp = (_combo_private_t *)data;

  if (!wp)
    return;

  if (wp && w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
    ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX))) {
    if (state && !wp->xwin) {
      _combo_open (wp);
      xitk_combo_update_pos (&wp->w);
    } else {
      _combo_close (wp, 0);
    }
  }      
}

/*
 *
 */
static void _combo_enability (_combo_private_t *wp) {
  if (wp->w.enable == WIDGET_ENABLE) {
    xitk_enable_widget (wp->label_widget);
    xitk_enable_widget (wp->button_widget);
  } else {
    if (wp->xwin) {
      xitk_checkbox_set_state (wp->button_widget, 0);
      _combo_rollunroll (wp->button_widget, (void *)wp, 0);
    }
    xitk_disable_widget (wp->label_widget);
    xitk_disable_widget (wp->button_widget);
  }
}

static void _combo_destroy (_combo_private_t *wp) {
  if (wp->xwin)
    _combo_rollunroll (wp->button_widget, (void *)wp, 0);
  if (wp->label_widget) {
    xitk_destroy_widget (wp->label_widget);
    wp->label_widget = NULL;
  }
  if (wp->button_widget) {
    xitk_destroy_widget (wp->button_widget);
    wp->button_widget = NULL;
  }
  XITK_FREE (wp->entries);
  xitk_short_string_deinit (&wp->skin_element_name);
}

/*
 *
 */
static void _combo_paint (_combo_private_t *wp) {
  if (wp->xwin && (wp->w.visible < 1)) {
    xitk_checkbox_set_state (wp->button_widget, 0);
    _combo_rollunroll (wp->button_widget, (void *)wp, 0);
  }
  if (wp->w.visible == 1) {
    int bx, lw;

    lw = xitk_get_widget_width (wp->label_widget);
    xitk_set_widget_pos (wp->label_widget, wp->w.x, wp->w.y);
    bx = wp->w.x + lw;
    xitk_set_widget_pos (wp->button_widget, bx, wp->w.y);
    xitk_show_widget (wp->label_widget);
    xitk_show_widget (wp->button_widget);
  } else {
    xitk_hide_widget (wp->label_widget);
    xitk_hide_widget (wp->button_widget);
  }
}

/*
 *
 */
static void _combo_new_skin (_combo_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name.s) {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (skonfig, wp->skin_element_name.s);
    int x, y;

    xitk_skin_lock (skonfig);
      
    wp->w.visible = info ? (info->visibility ? 1 : -1) : 0;
    wp->w.enable  = info ? info->enability : 0;
      
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    xitk_get_widget_pos (wp->label_widget, &x, &y);
      
    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width (wp->label_widget);
      
    (void)xitk_set_widget_pos (wp->button_widget, x, y);

    xitk_skin_unlock (skonfig);
  }
}

static void _combo_tips_timeout (_combo_private_t *wp, unsigned long timeout) {
  if (wp->label_widget)
    xitk_set_widget_tips_and_timeout (wp->button_widget, wp->w.tips_string, timeout);
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp || !event)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_COMBO)
    return 0;

  (void)result;
  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _combo_paint (wp);
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _combo_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _combo_destroy (wp);
      break;
    case WIDGET_EVENT_ENABLE:
      _combo_enability (wp);
      break;
    case WIDGET_EVENT_TIPS_TIMEOUT:
      _combo_tips_timeout (wp, event->tips_timeout);
      break;
    default: ;
  }
  return 0;
}

/* ************************* END OF PRIVATES ******************************* */

/*
 *
 */
void xitk_combo_set_select(xitk_widget_t *w, int select) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    if (wp->entries && wp->entries[select]) {
      wp->selected = select;
      xitk_label_change_label (wp->label_widget, wp->entries[select]);
    }
  }
}

/*
 *
 */
void xitk_combo_update_pos(xitk_widget_t *w) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    int                    xx = 0, yy = 0;
    window_info_t          wi;
    
    if (wp->xwin) {
      if((xitk_get_window_info(*(wp->parent_wkey), &wi))) {
	wp->win_x = wi.x;
	wp->win_y = wi.y;
	WINDOW_INFO_ZERO(&wi);
      }
      
      xitk_get_widget_pos(wp->label_widget, &xx, &yy);
      
      yy += xitk_get_widget_height(wp->label_widget);
      wp->win_x += xx;
      wp->win_y += yy;

      xitk_window_move_window(wp->xwin, wp->win_x, wp->win_y);
      xitk_window_show_window(wp->xwin, 1);

      xitk_window_try_to_set_input_focus(wp->xwin);

      /* No widget focused, give focus to the first one */
      if (wp->widget_list->widget_focused == NULL)
        xitk_set_focus_to_next_widget(wp->widget_list, 0, 0);
    }
  }
}

/*
 *
 */
int xitk_combo_get_current_selected(xitk_widget_t *w) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return -1;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    return wp->selected;
  }
  return -1;    
}

/*
 * 
 */
const char *xitk_combo_get_current_entry_selected(xitk_widget_t *w) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return NULL;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    if (wp->entries && wp->selected >= 0)
      return (wp->entries[wp->selected]);
  }
  return NULL;    
}

/*
 *
 */
void xitk_combo_update_list(xitk_widget_t *w, const char *const *const list, int len) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    free (wp->entries);
    wp->num_entries = len;
    wp->entries     = _combo_copy_string_list (list, &wp->num_entries);
    wp->selected    = -1;

    if (wp->xwin)
      xitk_browser_update_list (wp->browser_widget,
        (const char* const*)wp->entries, NULL, wp->num_entries, 0);
  }
}

/*
 *
 */
static xitk_widget_t *_combo_create (xitk_widget_list_t *wl, xitk_combo_widget_t *c,
  const char *skin_element_name, _combo_private_t *wp, int visible, int enable) {
  const char * const *entries = c->entries;

  ABORT_IF_NULL(wl);

  wp->xwin = NULL;

  if (skin_element_name) {
    xitk_short_string_init (&wp->skin_element_name);
    xitk_short_string_set (&wp->skin_element_name, skin_element_name);
  } else {
    wp->skin_element_name.s = NULL;
  }
  wp->combo_widget      = &wp->w;
  wp->parent_wlist      = wl;
  wp->parent_wkey       = c->parent_wkey;
  wp->callback          = c->callback;
  wp->userdata          = c->userdata;
  wp->num_entries       = 0x7fffffff;
  wp->entries           = _combo_copy_string_list (c->entries, &wp->num_entries);
  wp->selected          = -1;

  if (wp->num_entries) {
    xitk_label_change_label (wp->label_widget, entries[0]);
    wp->selected = 0;
  }

  wp->w.enable         = enable;
  wp->w.visible        = visible;

  //  wp->w.x = wp->w.y = wp->w.width = wp->w.height = 0;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_COMBO;
  wp->w.event        = notify_event;

  xitk_widget_set_focus_redirect (wp->label_widget, wp->button_widget);
  xitk_widget_set_focus_redirect (&wp->w, wp->button_widget);

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_combo_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_combo_widget_t *c) {
  _combo_private_t        *wp;
  xitk_checkbox_widget_t   cb;
  xitk_label_widget_t      lbl;

  XITK_CHECK_CONSTITENCY(c);

  XITK_WIDGET_INIT(&cb);
  XITK_WIDGET_INIT(&lbl);

  wp = (_combo_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  /* Create label and button (skinable) */
  lbl.label             = "";
  lbl.skin_element_name = c->skin_element_name;
  lbl.callback          = NULL;
  lbl.userdata          = NULL;
  if ((wp->label_widget = xitk_label_create (wl, skonfig, &lbl))) {
    xitk_widget_set_parent (wp->label_widget, &wp->w);
    wp->label_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->label_widget->node);
  }

  cb.skin_element_name = c->skin_element_name;
  cb.callback          = _combo_rollunroll;
  cb.userdata          = (void *)wp;
  if ((wp->button_widget = xitk_checkbox_create (wl, skonfig, &cb))) {
    xitk_widget_set_parent (wp->button_widget, &wp->w);
    wp->button_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->button_widget->node);
  }

  {
    int x, y;
    
    xitk_get_widget_pos(wp->label_widget, &x, &y);

    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width(wp->label_widget);
    
    (void) xitk_set_widget_pos(wp->button_widget, x, y);
  }
  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (skonfig, c->skin_element_name);
    return _combo_create (wl, c, c->skin_element_name ? c->skin_element_name : "", wp,
      info ? (info->visibility ? 1 : -1) : 0, info ? info->enability : 0);
  }
}

/*
 *  ******************************************************************************
 */
xitk_widget_t *xitk_noskin_combo_create (xitk_widget_list_t *wl,
  xitk_combo_widget_t *c, int x, int y, int width) {
  _combo_private_t        *wp;
  xitk_checkbox_widget_t   cb;
  xitk_label_widget_t      lbl;

  ABORT_IF_NULL(wl);

  XITK_CHECK_CONSTITENCY(c);

  wp = (_combo_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&cb);
  XITK_WIDGET_INIT(&lbl);


  /* Create label and button (skinable) */
  {
    xitk_font_t    *fs;
    int             height;
    
    fs = xitk_font_load_font(wl->xitk, DEFAULT_FONT_10);
    xitk_font_set_font(fs, wl->gc);
    height = xitk_font_get_string_height(fs, " ") + 6;
    xitk_font_unload_font(fs);

    lbl.skin_element_name = "XITK_NOSKIN_INNER";
    lbl.label             = "";
    lbl.callback          = NULL;
    lbl.userdata          = NULL;
    if ((wp->label_widget = xitk_noskin_label_create (wl, &lbl,
      x, y, (width - height), height, DEFAULT_FONT_10))) {
      xitk_widget_set_parent (wp->label_widget, &wp->w);
      wp->label_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->label_widget->node);
    }

    cb.skin_element_name = "XITK_NOSKIN_DOWN";
    cb.callback          = _combo_rollunroll;
    cb.userdata          = (void *)wp;

    if ((wp->button_widget = xitk_noskin_checkbox_create (wl, &cb,
      x + (width - height), y, height, height))) {
      xitk_widget_set_parent (wp->button_widget, &wp->w);
      wp->button_widget->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->button_widget->node);
    }

    wp->w.x = x;
    wp->w.y = y;
    wp->w.width = width;
    wp->w.height = height;
  }

  return _combo_create (wl, c, NULL, wp, 0, 0);
}
