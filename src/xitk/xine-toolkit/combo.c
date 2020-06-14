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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>

#include "_xitk.h"

typedef struct {
  xitk_widget_t           w;

  ImlibData              *imlibdata;
  char                   *skin_element_name;

  xitk_window_t          *xwin;
  GC                      gc;

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
  int                     visible;
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
  while (*p) {
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
    
static void _combo_close (_combo_private_t *wp) {
  wp->visible = 0;
  if (wp->xwin) {
    xitk_unregister_event_handler (&wp->widget_key);
    xitk_destroy_widgets (wp->widget_list);
    xitk_window_destroy_window (wp->xwin);
    wp->xwin = NULL;
  }

  if (wp->widget_list->gc) {
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XFreeGC (wp->imlibdata->x.disp, wp->widget_list->gc);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
    wp->widget_list->gc = NULL;
  }

  if (wp->widget_list) {
    xitk_widget_list_defferred_destroy (wp->widget_list);
    wp->widget_list = NULL;
  }
}

/*
 * Handle Xevents here.
 */
static void _combo_handle_event (XEvent *event, void *data) {
  _combo_private_t *wp = (_combo_private_t*)data;

  if (wp && wp->visible) {
    switch (event->type) {
      case KeyPress:
        {
          KeySym mykey;
          char kbuf[32];

          XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
          XLookupString (&event->xkey, kbuf, sizeof (kbuf), &mykey, NULL);
          XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
          if (mykey == XK_Escape) {
            _combo_close (wp);
            xitk_checkbox_set_state (wp->button_widget, 0);
            xitk_set_focus_to_widget (wp->button_widget);
          }
#if 0
          else if (mykey == XK_Up) {
            if (wp->sel2 > 0)
              xitk_browser_set_select (wp->browser_widget, --wp->sel2);
          } else if (mykey == XK_Down) {
            if (wp->sel2 < wp->num_entries)
              xitk_browser_set_select (wp->browser_widget, ++wp->sel2);
          }
#endif
        }
        break;
      case ButtonRelease:
        /* If we try to move the combo window,
         * move it back to right position (under label). */
        {
          int  x, y, modifier;
          /* FIXME: what is this good for here? */
          xitk_get_key_modifier (event, &modifier);
          xitk_window_get_window_position (wp->xwin, &x, &y, NULL, NULL);
          if ((x != wp->win_x) || (y != wp->win_y))
          xitk_combo_update_pos (wp->combo_widget);
        }
        break;
      default: ;
    }
  }
}

static void combo_select (xitk_widget_t *w, void *data, int selected, int modifier) {
  _combo_private_t *wp = (_combo_private_t *)data;

  if (wp && w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {

    if (wp->xwin) {
      wp->selected = selected;
      _combo_close (wp);
      xitk_label_change_label (wp->label_widget, wp->entries[selected]);
      xitk_checkbox_set_state (wp->button_widget, 0);
      xitk_set_focus_to_widget (wp->button_widget);
      if (wp->callback)
        wp->callback (wp->combo_widget, wp->userdata, selected);
    }
  }
}

static void _combo_open (_combo_private_t *wp) {
  Window win;
  unsigned int itemw, itemh, slidw;

  if (wp->xwin)
    return;

  itemh = 20;
  slidw = 12;
  itemw = xitk_get_widget_width (wp->label_widget);
  itemw += xitk_get_widget_width (wp->button_widget);
  itemw -= 2; /* space for border */

  wp->xwin = xitk_window_create_simple_window (wp->w.wl->imlibdata,
    0, 0, itemw + 2, itemh * 5 + slidw + 2);
  if (!wp->xwin)
    return;
  win = xitk_window_get_window (wp->xwin);

  XLOCK (wp->w.wl->imlibdata->x.x_lock_display, wp->w.wl->imlibdata->x.disp);
  {
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    XChangeWindowAttributes (wp->w.wl->imlibdata->x.disp, win, CWOverrideRedirect, &attr);
  }
  XSetTransientForHint (wp->w.wl->imlibdata->x.disp, win, wp->parent_wlist->win);
  wp->gc = XCreateGC (wp->w.wl->imlibdata->x.disp, win, None, None);
  XUNLOCK (wp->w.wl->imlibdata->x.x_unlock_display, wp->w.wl->imlibdata->x.disp);

  /* Change default classhint to new one. */
  xitk_window_set_window_class (wp->xwin, "Xitk Combo", "Xitk");

  wp->widget_list        = xitk_widget_list_new (wp->w.wl->imlibdata);
  xitk_dlist_init (&wp->widget_list->list);
  wp->widget_list->win   = win;
  wp->widget_list->gc    = wp->gc;

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
    browser.callback                      = combo_select;
    browser.dbl_click_callback            = NULL;
    browser.userdata                      = (void *)wp;
    wp->browser_widget = xitk_noskin_browser_create (wp->widget_list, &browser,
      wp->widget_list->gc, 1, 1, itemw - slidw, itemh, slidw, DEFAULT_FONT_10);
    xitk_dlist_add_tail (&wp->widget_list->list, &wp->browser_widget->node);
    xitk_enable_and_show_widget (wp->browser_widget);
    wp->browser_widget->type |= WIDGET_GROUP | WIDGET_GROUP_COMBO;
  }
  wp->browser_widget->tips_timeout = wp->browser_widget->tips_timeout;
  
  xitk_browser_update_list (wp->browser_widget, 
    (const char * const *)wp->entries, NULL, wp->num_entries, 0);
  wp->sel2 = wp->selected;
  xitk_browser_set_select (wp->browser_widget, wp->selected);

  wp->widget_key = xitk_register_event_handler ("xitk combo", wp->xwin,
    _combo_handle_event, NULL, NULL, wp->widget_list, (void *)wp);

  wp->visible = 1;
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
    if (state && wp->visible == 0) {
      _combo_open (wp);
      xitk_combo_update_pos (&wp->w);
    } else {
      _combo_close (wp);
    }
  }      
}

static void _combo_rollunroll_from_lbl(xitk_widget_t *w, void *data) {
  _combo_private_t *wp = (_combo_private_t *)data;

  if (wp && w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
    ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))) {
    int state;
    
    state = !xitk_checkbox_get_state (wp->button_widget);
    xitk_checkbox_set_state (wp->button_widget, state);

    if (state && wp->visible == 0) {
      wp->w.wl->widget_focused = wp->button_widget;
      _combo_open (wp);
      xitk_combo_update_pos (&wp->w);
    } else {
      _combo_close (wp);
    }
  }      
}

/*
 *
 */
static void enability (xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;
  
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_COMBO)
    return;

  if (wp->w.enable == WIDGET_ENABLE) {
    xitk_enable_widget (wp->label_widget);
    xitk_enable_widget (wp->button_widget);
  } else {
    if (wp->visible) {
      xitk_checkbox_set_state (wp->button_widget, 0);
      _combo_rollunroll (wp->button_widget, (void *)wp, 0);
    }
    xitk_disable_widget (wp->label_widget);
    xitk_disable_widget (wp->button_widget);
  }
}

static void notify_destroy (xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_COMBO)
    return;

  if (wp->visible)
    _combo_rollunroll (wp->button_widget, (void *)wp, 0);

  if (wp->w.wl) {
    xitk_widget_t *_w = (xitk_widget_t *)wp->w.wl->list.head.next;

    while (_w->node.next) {
      if ((_w == wp->label_widget) || (_w == wp->button_widget))
        _w->parent = NULL;
      _w = (xitk_widget_t *)_w->node.next;
    }
  }
    
  free (wp->entries);
  wp->entries = NULL;
  XITK_FREE (wp->skin_element_name);
}

/*
 *
 */
static void paint (xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_COMBO)
    return;

  if (wp->visible == 1 && (wp->w.visible < 1)) {
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
void xitk_combo_callback_exec (xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;
  
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_COMBO)
    return;

  if (wp->callback)
    wp->callback (wp->combo_widget, wp->userdata, wp->selected);
}

/*
 *
 */
static void notify_change_skin (xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  _combo_private_t *wp = (_combo_private_t *)w;
  
  if (wp->skin_element_name) {
    int x, y;

    xitk_skin_lock (skonfig);
      
    wp->w.visible = xitk_skin_get_visibility (skonfig, wp->skin_element_name) ? 1 : -1;
    wp->w.enable  = xitk_skin_get_enability (skonfig, wp->skin_element_name);
      
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    xitk_get_widget_pos (wp->label_widget, &x, &y);
      
    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width (wp->label_widget);
      
    (void)xitk_set_widget_pos (wp->button_widget, x, y);

    xitk_skin_unlock (skonfig);
  }
}

static void tips_timeout(xitk_widget_t *w, unsigned long timeout) {
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return;
  
  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    wp->tips_timeout = timeout;
    wp->combo_widget->tips_timeout = timeout;
    wp->label_widget->tips_timeout = timeout;
    wp->button_widget->tips_timeout = timeout;
    if (wp->xwin)
      wp->browser_widget->tips_timeout = timeout;
  }
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    paint(w);
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_ENABLE:
    enability(w);
    break;
  case WIDGET_EVENT_TIPS_TIMEOUT:
    tips_timeout(w, event->tips_timeout);
    break;
  }
  
  return retval;
}

/*
 *
 */

/*
 * ************************* END OF PRIVATES *******************************
 */

/*
 *
 */
void xitk_combo_rollunroll(xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return;
  
  if ((wp->w.type & (WIDGET_GROUP_COMBO | WIDGET_TYPE_MASK)) == (WIDGET_GROUP_COMBO | WIDGET_TYPE_CHECKBOX)) {
    int state = !xitk_checkbox_get_state (&wp->w);

    xitk_checkbox_callback_exec (&wp->w);
    xitk_checkbox_set_state (&wp->w, state);
  } else if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    int state = !xitk_checkbox_get_state (wp->button_widget);

    xitk_checkbox_callback_exec (wp->button_widget);
    xitk_checkbox_set_state (wp->button_widget, state);
  }
}

  /*
   *
   */
void xitk_combo_set_select(xitk_widget_t *w, int select) {
  _combo_private_t *wp = (_combo_private_t *)w;

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
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    int                    xx = 0, yy = 0;
    window_info_t          wi;
    XSizeHints             hint;
    
    if(wp->visible) {
      if((xitk_get_window_info(*(wp->parent_wkey), &wi))) {
	wp->win_x = wi.x;
	wp->win_y = wi.y;
	WINDOW_INFO_ZERO(&wi);
      }
      
      xitk_get_widget_pos(wp->label_widget, &xx, &yy);
      
      yy += xitk_get_widget_height(wp->label_widget);
      wp->win_x += xx;
      wp->win_y += yy;
      
      hint.x = wp->win_x;
      hint.y = wp->win_y;
      hint.flags = PPosition;

      XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
      XSetWMNormalHints (wp->imlibdata->x.disp,
			 xitk_window_get_window(wp->xwin),
			 &hint);
      XMoveWindow(wp->imlibdata->x.disp, 
		  (xitk_window_get_window(wp->xwin)), 
		  wp->win_x, wp->win_y);
      XMapRaised(wp->imlibdata->x.disp, (xitk_window_get_window(wp->xwin)));
      XSync(wp->imlibdata->x.disp, False);
      XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);

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
  _combo_private_t *wp = (_combo_private_t *)w;
  
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
  _combo_private_t *wp = (_combo_private_t *)w;
  
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
  _combo_private_t *wp = (_combo_private_t *)w;
  
  if (!wp)
    return;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    free (wp->entries);
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
xitk_widget_t *xitk_combo_get_label_widget(xitk_widget_t *w) {
  _combo_private_t *wp = (_combo_private_t *)w;

  if (!wp)
    return NULL;

  if ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO) {
    return wp->label_widget;
  }
  return NULL;
}

/*
 *
 */
static xitk_widget_t *_xitk_combo_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig,
  xitk_combo_widget_t *c, const char *skin_element_name, _combo_private_t *wp, int visible, int enable) {
  const char * const *entries = c->entries;
  xitk_browser_widget_t       browser;
  
  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_WIDGET_INIT(&browser);

  wp->xwin = NULL;

  wp->imlibdata                = wl->imlibdata;
  wp->skin_element_name        = (skin_element_name == NULL) ? NULL : strdup(skin_element_name);
  wp->combo_widget             = &wp->w;
  wp->parent_wlist             = wl;
  wp->parent_wkey              = c->parent_wkey;
  wp->callback                 = c->callback;
  wp->userdata                 = c->userdata;
  wp->entries                  = _combo_copy_string_list (c->entries, &wp->num_entries);
  wp->selected                 = -1;
  
  if (wp->num_entries) {
    xitk_label_change_label (wp->label_widget, entries[0]);
    wp->selected = 0;
  }

  wp->w.parent       = NULL;
  wp->w.private_data = wp;
  wp->w.wl           = wl;
  wp->w.enable       = enable;
  wp->w.running      = 1;
  wp->w.visible      = visible;
  wp->w.have_focus   = FOCUS_LOST;

  //  wp->w.x = wp->w.y = wp->w.width = wp->w.height = 0;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_COMBO;
  wp->w.event        = notify_event;
  wp->w.tips_timeout = 0;
  wp->w.tips_string  = NULL;

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_combo_create(xitk_widget_list_t *wl,
				 xitk_skin_config_t *skonfig, xitk_combo_widget_t *c,
				 xitk_widget_t **lw, xitk_widget_t **bw) {
  _combo_private_t        *wp;
  xitk_checkbox_widget_t   cb;
  xitk_label_widget_t      lbl;
  
  XITK_CHECK_CONSTITENCY(c);

  XITK_WIDGET_INIT(&cb);
  XITK_WIDGET_INIT(&lbl);

  wp = (_combo_private_t *) xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  /* Create label and button (skinable) */
  lbl.label             = "";
  lbl.skin_element_name = c->skin_element_name;
  lbl.callback          = _combo_rollunroll_from_lbl;
  lbl.userdata          = (void *)wp;
  if ((wp->label_widget = xitk_label_create (wl, skonfig, &lbl))) {
    wp->label_widget->parent = &wp->w;
    wp->label_widget->type |= WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->label_widget->node);
  }

  cb.skin_element_name = c->skin_element_name;
  cb.callback          = _combo_rollunroll;
  cb.userdata          = (void *)wp;
  if ((wp->button_widget = xitk_checkbox_create (wl, skonfig, &cb))) {
    wp->button_widget->parent = &wp->w;
    wp->button_widget->type |= WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
    xitk_dlist_add_tail (&wl->list, &wp->button_widget->node);
  }

  if(lw)
    *lw = wp->label_widget;
  if(bw)
    *bw = wp->button_widget;
  
  {
    int x, y;
    
    xitk_get_widget_pos(wp->label_widget, &x, &y);

    wp->w.x = x;
    wp->w.y = y;

    x += xitk_get_widget_width(wp->label_widget);
    
    (void) xitk_set_widget_pos(wp->button_widget, x, y);
  }
  return _xitk_combo_create (wl, skonfig, c, c->skin_element_name, wp,
    xitk_skin_get_visibility (skonfig, c->skin_element_name) ? 1 : -1,
    xitk_skin_get_enability (skonfig, c->skin_element_name));
}

/*
 *  ******************************************************************************
 */
xitk_widget_t *xitk_noskin_combo_create(xitk_widget_list_t *wl,
					xitk_combo_widget_t *c,
					int x, int y, int width, 
					xitk_widget_t **lw, xitk_widget_t **bw) {
  _combo_private_t        *wp;
  xitk_checkbox_widget_t   cb;
  xitk_label_widget_t      lbl;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_CHECK_CONSTITENCY(c);

  wp = (_combo_private_t *) xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  XITK_WIDGET_INIT(&cb);
  XITK_WIDGET_INIT(&lbl);


  /* Create label and button (skinable) */
  {
    xitk_font_t    *fs;
    int             height;
    
    fs = xitk_font_load_font(wl->imlibdata->x.disp, DEFAULT_FONT_10);
    xitk_font_set_font(fs, wl->gc);
    height = xitk_font_get_string_height(fs, " ") + 4;
    xitk_font_unload_font(fs);

    lbl.skin_element_name = NULL;
    lbl.label             = "";
    lbl.callback          = _combo_rollunroll_from_lbl;
    lbl.userdata          = (void *)wp;
    if ((wp->label_widget = xitk_noskin_label_create (wl, &lbl,
      x, y, (width - height), height, DEFAULT_FONT_10))) {
      wp->label_widget->parent = &wp->w;
      wp->label_widget->type |= WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->label_widget->node);
    }

    cb.skin_element_name = "XITK_NOSKIN_DOWN";
    cb.callback          = _combo_rollunroll;
    cb.userdata          = (void *)wp;

    if ((wp->button_widget = xitk_noskin_checkbox_create (wl, &cb,
      x + (width - height), y, height, height))) {
      wp->button_widget->parent = &wp->w;
      wp->button_widget->type |= WIDGET_GROUP_WIDGET | WIDGET_GROUP_COMBO;
      xitk_dlist_add_tail (&wl->list, &wp->button_widget->node);
    }

    if(lw)
      *lw = wp->label_widget;
    if(bw)
      *bw = wp->button_widget;
    
    wp->w.x = x;
    wp->w.y = y;
    wp->w.width = width;
    wp->w.height = height;
    
    {
      xitk_image_t *wimage = xitk_get_widget_foreground_skin(wp->label_widget);
      
      if(wimage)
        draw_rectangular_inner_box(wimage->image, 0, 0, wimage->width - 1, wimage->height - 1);
    }

  }

  return _xitk_combo_create (wl, NULL, c, NULL, wp, 0, 0);
}
