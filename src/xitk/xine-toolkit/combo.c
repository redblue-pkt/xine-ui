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

#include <stdio.h>
#include <string.h>

#include "_xitk.h"
#include "combo.h"
#include "button.h"
#include "label.h"
#include "browser.h"
#include "default_font.h"

typedef enum {
  _W_combo = 0,
  /* keep order */
  _W_label,
  _W_button,
  /* /keep order */
  _W_browser,
  _W_LAST
} _W_t;

typedef struct {
  xitk_widget_t           w;

  char                    skin_element_name[64];

  xitk_window_t          *xwin;

  int                     win_x;
  int                     win_y;

  xitk_register_key_t    *parent_wkey;

  xitk_widget_t          *iw[_W_LAST];

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
    xitk_widget_register_win_pos (&wp->w, 0);
    if (focus)
      xitk_window_set_input_focus (wp->parent_wlist->xwin);
    wp->iw[_W_browser] = NULL;
    xitk_unregister_event_handler (wp->w.wl->xitk, &wp->widget_key);
    xitk_window_destroy_window (wp->xwin);
    wp->xwin = NULL;
    if (focus) {
      xitk_set_focus_to_widget (wp->iw[_W_button]);
      xitk_button_set_state (wp->iw[_W_button], 0);
    }
    if (wp->iw[_W_button])
      wp->iw[_W_button]->type &= ~WIDGET_KEEP_FOCUS;
  }
}

/*
 * Handle Xevents here.
 */

static int combo_event (void *data, const xitk_be_event_t *e) {
  _combo_private_t *wp = (_combo_private_t*)data;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      _combo_close (wp, 1);
      return 1;
    case XITK_EV_KEY_DOWN:
      if ((e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE)) {
        _combo_close (wp, 1);
        return 1;
      }
      break;
#if 0
    case XITK_EV_BUTTON_UP:
      /* If we try to move the combo window, move it back to right position (under label).
       * XXX: cannot happen anymore because combo win drag is blocked by XITK_WINF_FIXED_POS */
      {
	xitk_rect_t wr = {0, 0, 0, 0};
        xitk_window_get_window_position (wp->xwin, &wr);
        if ((wr.x != wp->win_x) || (wr.y != wp->win_y))
          xitk_combo_update_pos (wp->iw[_W_combo]);
      }
      return 1;
#endif
    default: ;
  }
  return 0;
}

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
  if (selected < 0)
    return;
  wp->selected = selected;
  xitk_label_change_label (wp->iw[_W_label], wp->entries[selected]);
  if (wp->callback)
    wp->callback (wp->iw[_W_combo], wp->userdata, selected);
}

static void _combo_open (_combo_private_t *wp) {
  unsigned int itemw, itemh, slidw;

  if (wp->xwin)
    return;

  itemh = 20;
  slidw = 12;
  itemw = xitk_get_widget_width (wp->iw[_W_label]);
  itemw += xitk_get_widget_width (wp->iw[_W_button]);
  itemw -= 2; /* space for border */

  {
    window_info_t wi;

    xitk_get_widget_pos (wp->iw[_W_label], &wp->win_x, &wp->win_y);
    wp->win_y += xitk_get_widget_height (wp->iw[_W_label]);
    if ((xitk_get_window_info (wp->w.wl->xitk, *(wp->parent_wkey), &wi)))
      wp->win_x += wi.x, wp->win_y += wi.y;
  }

  wp->xwin = xitk_window_create_simple_window_ext (wp->w.wl->xitk,
    wp->win_x, wp->win_y, itemw + 2, itemh * 5 + 2,
    NULL, "Xitk Combo", "Xitk", 1, 0, NULL);
  if (!wp->xwin)
    return;

  if (wp->iw[_W_button])
    wp->iw[_W_button]->type |= WIDGET_KEEP_FOCUS;

  xitk_window_flags (wp->xwin, XITK_WINF_FIXED_POS, XITK_WINF_FIXED_POS);
  xitk_window_set_transient_for_win (wp->xwin, wp->parent_wlist->xwin);

  wp->widget_list        = xitk_window_widget_list(wp->xwin);

  {
    xitk_browser_widget_t browser;
    /* Browser */
    XITK_WIDGET_INIT (&browser);
    browser.arrow_up.skin_element_name    = NULL;
    browser.slider.skin_element_name      = NULL;
    browser.arrow_dn.skin_element_name    = NULL;
    browser.arrow_left.skin_element_name  = NULL;
    browser.slider_h.skin_element_name    = NULL;
    browser.arrow_right.skin_element_name = NULL;
    browser.browser.skin_element_name     = NULL;
    browser.browser.num_entries           = wp->num_entries;
    browser.browser.entries               = (const char * const *)wp->entries;
    browser.dbl_click_callback            = NULL;
    browser.browser.max_displayed_entries = 5;
    browser.callback                      = _combo_select;
    browser.userdata                      = (void *)wp;
    wp->iw[_W_browser] = xitk_noskin_browser_create (wp->widget_list, &browser,
      1, 1, itemw, itemh, -slidw, DEFAULT_FONT_10);
    xitk_dlist_add_tail (&wp->widget_list->list, &wp->iw[_W_browser]->node);
    wp->iw[_W_browser]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
  }

  xitk_browser_update_list (wp->iw[_W_browser],
    (const char * const *)wp->entries, NULL, wp->num_entries, 0);
  wp->sel2 = wp->selected;
  xitk_browser_set_select (wp->iw[_W_browser], wp->selected);
  xitk_widgets_state (wp->iw + _W_browser, 1, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE, ~0u);

  wp->widget_key = xitk_be_register_event_handler ("xitk combo", wp->xwin, combo_event, wp, NULL, NULL);

  xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
  xitk_window_raise_window (wp->xwin);
  xitk_window_set_input_focus (wp->xwin);

  /* No widget focused, give focus to the first one */
  if (wp->widget_list->widget_focused == NULL)
    xitk_set_focus_to_next_widget (wp->widget_list, 0, 0);

  xitk_widget_register_win_pos (&wp->w, 1);
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
    /* no typo, thats what it reports with state ^ */
    if (state && !wp->xwin) {
      _combo_open (wp);
    } else {
      _combo_close (wp, 0);
    }
  }
}

/*
 *
 */
static void _combo_enability (_combo_private_t *wp) {
  if (wp->w.state & XITK_WIDGET_STATE_ENABLE) {
    /* label, button */
    xitk_widgets_state (wp->iw + _W_label, 2, XITK_WIDGET_STATE_ENABLE, ~0u);
  } else {
    if (wp->xwin) {
      xitk_button_set_state (wp->iw[_W_button], 0);
      _combo_rollunroll (wp->iw[_W_button], (void *)wp, 0);
    }
    /* label, button */
    xitk_widgets_state (wp->iw + _W_label, 2, XITK_WIDGET_STATE_ENABLE, 0);
  }
}

static void _combo_destroy (_combo_private_t *wp) {
  if (wp->xwin)
    _combo_rollunroll (wp->iw[_W_button], (void *)wp, 0);
  xitk_widgets_delete (wp->iw + _W_label, 2);
  XITK_FREE (wp->entries);
}

/*
 *
 */
static void _combo_paint (_combo_private_t *wp) {
  unsigned int show = 0;
  if (wp->xwin && !(wp->w.state & XITK_WIDGET_STATE_VISIBLE)) {
    xitk_button_set_state (wp->iw[_W_button], 0);
    _combo_rollunroll (wp->iw[_W_button], (void *)wp, 0);
  }
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    int bx, lw;

    lw = xitk_get_widget_width (wp->iw[_W_label]);
    xitk_set_widget_pos (wp->iw[_W_label], wp->w.x, wp->w.y);
    bx = wp->w.x + lw;
    xitk_set_widget_pos (wp->iw[_W_button], bx, wp->w.y);
    show = ~0u;
  }
  /* label, button */
  xitk_widgets_state (wp->iw + _W_label, 2, XITK_WIDGET_STATE_VISIBLE, show);
}

/*
 *
 */
static void _combo_new_skin (_combo_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name[0]) {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (skonfig, wp->skin_element_name);
    int x, y;

    xitk_skin_lock (skonfig);

    xitk_widget_state_from_info (&wp->w, info);

    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    xitk_get_widget_pos (wp->iw[_W_label], &x, &y);

    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width (wp->iw[_W_label]);

    (void)xitk_set_widget_pos (wp->iw[_W_button], x, y);

    xitk_skin_unlock (skonfig);
  }
}

static void _combo_tips_timeout (_combo_private_t *wp, unsigned long timeout) {
  if (wp->iw[_W_label])
    xitk_set_widget_tips_and_timeout (wp->iw[_W_button], wp->w.tips_string, timeout);
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
    case WIDGET_EVENT_WIN_POS:
      if (wp->xwin) {
        xitk_rect_t wr = {0, 0, XITK_INT_KEEP, XITK_INT_KEEP};

        xitk_get_widget_pos (wp->iw[_W_label], &wr.x, &wr.y);
        wr.y += xitk_get_widget_height (wp->iw[_W_label]);
        wr.x += event->x;
        wr.y += event->y;
        xitk_window_move_resize (wp->xwin, &wr);
        xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
        xitk_window_raise_window (wp->xwin);
        xitk_window_set_input_focus (wp->xwin);
        /* No widget focused, give focus to the first one */
        if (wp->widget_list->widget_focused == NULL)
          xitk_set_focus_to_next_widget (wp->widget_list, 0, 0);
        return 1;
      }
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
      xitk_label_change_label (wp->iw[_W_label], wp->entries[select]);
    }
  }
}

/*
 *
 */
#if 0
void xitk_combo_update_pos(xitk_widget_t *w) {
  _combo_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    if (wp->xwin) {
      xitk_rect_t wr = {0, 0, XITK_INT_KEEP, XITK_INT_KEEP};
      window_info_t wi;

      xitk_get_widget_pos (wp->iw[_W_label], &wr.x, &wr.y);
      wr.y += xitk_get_widget_height (wp->iw[_W_label]);
      if ((xitk_get_window_info (wp->w.wl->xitk, *(wp->parent_wkey), &wi)))
        wr.x += wi.x, wr.y += wi.y;
      wp->win_x = wr.x, wp->win_y = wr.y;

      xitk_window_move_resize (wp->xwin, &wr);
      xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
      xitk_window_raise_window (wp->xwin);

      xitk_window_set_input_focus (wp->xwin);

      /* No widget focused, give focus to the first one */
      if (wp->widget_list->widget_focused == NULL)
        xitk_set_focus_to_next_widget(wp->widget_list, 0, 0);
    }
  }
}
#endif

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
      xitk_browser_update_list (wp->iw[_W_browser],
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
    strlcpy (wp->skin_element_name,
      skin_element_name && skin_element_name[0] ? skin_element_name : "-",
      sizeof (wp->skin_element_name));
  } else {
    wp->skin_element_name[0] = 0;
  }
  wp->iw[_W_combo]      = &wp->w;
  wp->parent_wlist      = wl;
  wp->parent_wkey       = c->parent_wkey;
  wp->callback          = c->callback;
  wp->userdata          = c->userdata;
  wp->num_entries       = 0x7fffffff;
  wp->entries           = _combo_copy_string_list (c->entries, &wp->num_entries);
  wp->selected          = -1;

  if (wp->num_entries) {
    xitk_label_change_label (wp->iw[_W_label], entries[0]);
    wp->selected = 0;
  }

  wp->w.state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->w.state |= visible ? XITK_WIDGET_STATE_VISIBLE : 0;
  wp->w.state |= enable ? XITK_WIDGET_STATE_ENABLE : 0;

  //  wp->w.x = wp->w.y = wp->w.width = wp->w.height = 0;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_COMBO;
  wp->w.event        = notify_event;

  xitk_widget_set_focus_redirect (wp->iw[_W_label], wp->iw[_W_button]);
  xitk_widget_set_focus_redirect (&wp->w, wp->iw[_W_button]);

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_combo_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, xitk_combo_widget_t *c) {
  _combo_private_t        *wp;
  xitk_button_widget_t     b;
  xitk_label_widget_t      lbl;

  XITK_CHECK_CONSTITENCY(c);

  XITK_WIDGET_INIT (&b);
  XITK_WIDGET_INIT(&lbl);

  wp = (_combo_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  /* Create label and button (skinable) */
  lbl.label             = "";
  lbl.skin_element_name = c->skin_element_name;
  lbl.callback          = NULL;
  lbl.userdata          = NULL;
  if ((wp->iw[_W_label] = xitk_label_create (wl, skonfig, &lbl))) {
    xitk_widget_set_parent (wp->iw[_W_label], &wp->w);
    wp->iw[_W_label]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->iw[_W_label]->node);
  }

  b.skin_element_name = c->skin_element_name;
  b.callback          = NULL;
  b.state_callback    = _combo_rollunroll;
  b.userdata          = (void *)wp;
  if ((wp->iw[_W_button] = xitk_button_create (wl, skonfig, &b))) {
    xitk_widget_set_parent (wp->iw[_W_button], &wp->w);
    wp->iw[_W_button]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->iw[_W_button]->node);
  }

  {
    int x, y;

    xitk_get_widget_pos(wp->iw[_W_label], &x, &y);

    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width(wp->iw[_W_label]);

    (void) xitk_set_widget_pos(wp->iw[_W_button], x, y);
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
  _combo_private_t     *wp;
  xitk_button_widget_t  b;
  xitk_label_widget_t   lbl;

  ABORT_IF_NULL(wl);

  XITK_CHECK_CONSTITENCY(c);

  wp = (_combo_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT (&b);
  XITK_WIDGET_INIT(&lbl);


  /* Create label and button (skinable) */
  {
    xitk_font_t    *fs;
    int             height;

    fs = xitk_font_load_font (wl->xitk, DEFAULT_FONT_10);
    height = xitk_font_get_string_height(fs, " ") + 6;
    xitk_font_unload_font(fs);

    lbl.skin_element_name = "XITK_NOSKIN_INNER";
    lbl.label             = "";
    lbl.callback          = NULL;
    lbl.userdata          = NULL;
    if ((wp->iw[_W_label] = xitk_noskin_label_create (wl, &lbl,
      x, y, (width - height), height, DEFAULT_FONT_10))) {
      xitk_widget_set_parent (wp->iw[_W_label], &wp->w);
      wp->iw[_W_label]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->iw[_W_label]->node);
    }

    b.skin_element_name = "XITK_NOSKIN_DOWN";
    b.callback          = NULL;
    b.state_callback    = _combo_rollunroll;
    b.userdata          = (void *)wp;

    if ((wp->iw[_W_button] = xitk_noskin_button_create (wl, &b,
      x + (width - height), y, height, height))) {
      xitk_widget_set_parent (wp->iw[_W_button], &wp->w);
      wp->iw[_W_button]->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->iw[_W_button]->node);
    }

    wp->w.x = x;
    wp->w.y = y;
    wp->w.width = width;
    wp->w.height = height;
  }

  return _combo_create (wl, c, NULL, wp, 0, 0);
}
