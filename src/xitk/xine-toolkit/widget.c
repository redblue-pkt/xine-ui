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

#define XITK_WIDGET_C

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include <xine/sorted_array.h>

#include "_xitk.h"
#include "tips.h"
#include "menu.h"
#include "combo.h"

#define _ZERO_TO_MAX_MINUS_1(val,max) ((unsigned int)val < (unsigned int)max)


#if 0
void dump_widget_type(xitk_widget_t *w) {
  if(w->type & WIDGET_GROUP) {
    printf("WIDGET_TYPE_GROUP: ");
    if(w->type & WIDGET_GROUP_MEMBER)
      printf("[THE_WIDGET] ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)
      printf("WIDGET_TYPE_BROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_FILEBROWSER)
      printf("WIDGET_TYPE_FILEBROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER)
      printf("WIDGET_TYPE_MRLBROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO)
      printf("WIDGET_TYPE_COMBO | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_TABS)
      printf("WIDGET_TYPE_TABS | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX)
      printf("WIDGET_TYPE_INTBOX | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU)
      printf("WIDGET_TYPE_MENU | ");
  }

  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)      printf("WIDGET_TYPE_BUTTON ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) printf("WIDGET_TYPE_LABELBUTTON ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)      printf("WIDGET_TYPE_SLIDER ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)       printf("WIDGET_TYPE_LABEL ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)    printf("WIDGET_TYPE_CHECKBOX ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)       printf("WIDGET_TYPE_IMAGE ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)   printf("WIDGET_TYPE_INPUTTEXT ");
  printf("\n");
}
#endif

/* NOTE: at least for WIDGET_EVENT_{KEY|DESTROY|CLICK|CHANGE_SKIN}, xitk_widget_t.event ()
 * may modify the widget list, including deleting widgets -- with or without the help of
 * application callbacks.
 * Practical example: unclick a skin browser button -> change to skin with less browser
 * buttons -> delete that very button.
 * Remember to re-examine widget pointers after such call. */

static void xitk_widget_rel_init (xitk_widget_rel_t *r, xitk_widget_t *w) {
  xitk_dnode_init (&r->node);
  xitk_dlist_init (&r->list);
  r->group = NULL;
  r->w = w;
}

static void xitk_widget_rel_join (xitk_widget_rel_t *r, xitk_widget_rel_t *group) {
  if (r->group == group)
    return;
  if (r->group) {
    xitk_dnode_remove (&r->node);
    r->group = NULL;
  }
  if (group) {
    xitk_dlist_add_tail (&group->list, &r->node);
    r->group = group;
  }
}

static void xitk_widget_rel_deinit (xitk_widget_rel_t *r) {
  if (r->group) {
    xitk_dnode_remove (&r->node);
    r->group = NULL;
  }
  while (1) {
    xitk_widget_rel_t *s = (xitk_widget_rel_t *)r->list.tail.prev;
    if (!s->node.prev)
      break;
    xitk_dnode_remove (&s->node);
    s->group = NULL;
  }
}

/*
 * Alloc a memory area of size_t size.
 */
void *xitk_xmalloc(size_t size) {
  void *ptrmalloc;

  /* prevent xitk_xmalloc(0) of possibly returning NULL */
  if(!size)
    size++;

  if((ptrmalloc = calloc(1, size)) == NULL) {
    XITK_DIE("%s(): calloc() failed: %s\n", __FUNCTION__, strerror(errno));
  }

  return ptrmalloc;
}

/*
 * Call the notify_change_skin function, if exist, of each widget in
 * the specifier widget list.
 */
void xitk_change_skins_widget_list(xitk_widget_list_t *wl, xitk_skin_config_t *skonfig) {
  xitk_widget_t   *mywidget;
  widget_event_t   event;

  if (!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  event.type    = WIDGET_EVENT_CHANGE_SKIN;
  event.skonfig = skonfig;

  mywidget = (xitk_widget_t *)wl->list.head.next;
  while (mywidget->node.next && wl->xwin && skonfig) {

    (void) mywidget->event(mywidget, &event, NULL);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}

/*
 * (re)paint the specified widget list.
 */

#define XITK_WIDGET_STATE_PAINT (XITK_WIDGET_STATE_UNSET | XITK_WIDGET_STATE_ENABLE |\
  XITK_WIDGET_STATE_VISIBLE | XITK_WIDGET_STATE_CLICK | XITK_WIDGET_STATE_ON |\
  XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS)

static int _xitk_widget_paint (xitk_widget_t *w, widget_event_t *e) {
  int r;

  e->type = WIDGET_EVENT_PAINT;
  e->x = w->x;
  e->y = w->y;
  e->width = w->width;
  e->height = w->height;
  r = w->event (w, e, NULL);
  w->shown_state = w->state;
  return r;
}

int xitk_partial_paint_widget_list (xitk_widget_list_t *wl, xitk_hull_t *hull) {
  xitk_widget_t *w;
  xitk_hull_t h;
  int n = 0;

  if (!wl || !hull)
    return 0;
  if (!wl->xwin)
    return 0;
  h = *hull;

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    widget_event_t event;

    {
      xitk_hull_t wh;

      wh.x1 = xitk_max (w->x, h.x1);
      wh.x2 = xitk_min (w->x + w->width, h.x2);
      if (wh.x1 >= wh.x2)
        continue;
      wh.y1 = xitk_max (w->y, h.y1);
      wh.y2 = xitk_min (w->y + w->height, h.y2);
      if (wh.y1 >= wh.y2)
        continue;
      if (w->type & WIDGET_PARTIAL_PAINTABLE) {
        event.x = wh.x1;
        event.width = wh.x2 - wh.x1;
        event.y = wh.y1;
        event.height = wh.y2 - wh.y1;
      } else {
        event.x = w->x;
        event.width = w->width;
        event.y = w->y;
        event.height = w->height;
      }
      event.type = WIDGET_EVENT_PAINT;
      w->event (w, &event, NULL);
      w->shown_state = w->state;
      n += 1;
    }
  }
  return n;
}

int xitk_paint_widget_list (xitk_widget_list_t *wl) {
  xitk_widget_t   *w;
  widget_event_t   event;

  if (!wl)
    return 1;
  if (!wl->xwin)
    return 1;

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    _xitk_widget_paint (w, &event);
  }
  return 1;
}

/*
 * Return 1 if mouse pointer is in widget area.
 */
#ifdef YET_UNUSED
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y) {
  if(!widget) {
    XITK_WARNING("widget was NULL.\n");
    return 0;
  }

  if(((x >= widget->x) && (x < (widget->x + widget->width))) &&
     ((y >= widget->y) && (y < (widget->y + widget->height)))) {
    widget_event_t        event;

    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    return widget->event (widget, &event, NULL) != 2;
  }

  return 0;
}
#endif

/*
 * Return widget present at specified coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y) {
  xitk_widget_t *w;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  /* Overlapping widgets paint in forward order. Hit test in reverse then. */
  for (w = (xitk_widget_t *)wl->list.tail.prev; w->node.prev; w = (xitk_widget_t *)w->node.prev) {
    widget_event_t        event;
    int d;

    if ((w->state & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE)) != (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE))
      continue;
    if (w->type & WIDGET_GROUP)
      continue;
    d = x - w->x;
    if (!_ZERO_TO_MAX_MINUS_1 (d, w->width))
      continue;
    d = y - w->y;
    if (!_ZERO_TO_MAX_MINUS_1 (d, w->height))
      continue;

    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    if (w->event (w, &event, NULL) != 2)
      return w;
  }
  return NULL;
}

static void xitk_widget_apply_focus_redirect (xitk_widget_t **w) {
  xitk_widget_t *fr = *w;
  if (!fr)
    return;
  while (fr->focus_redirect.group) {
    fr = fr->focus_redirect.group->w;
    if (fr == *w)
      return;
  }
  *w = fr;
}

static xitk_widget_t *xitk_widget_test_focus_redirect (xitk_widget_t *w, int x, int y) {
  xitk_widget_t *iw = w;
  int level = 0;

  if (!w)
    return NULL;

  while (1) {
    do {
      int d;
      if ((iw->state & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE))
        != (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE))
        break;
      if (iw->type & WIDGET_GROUP)
        break;
      d = x - iw->x;
      if (!_ZERO_TO_MAX_MINUS_1 (d, iw->width))
        break;
      d = y - iw->y;
      if (!_ZERO_TO_MAX_MINUS_1 (d, iw->height))
        break;
      return iw;
    } while (0);

    {
      xitk_widget_rel_t *nr = (xitk_widget_rel_t *)iw->focus_redirect.list.head.next;
      if (nr->node.next && (nr->w != w)) {
        iw = nr->w;
        level++;
        continue;
      }
    }

    while (1) {
      xitk_widget_rel_t *nr = (xitk_widget_rel_t *)iw->focus_redirect.node.next;
      if (!nr)
        return NULL;
      if (nr->node.next) {
        if (level <= 0)
          return NULL;
        iw = nr->w;
        break;
      }
      if (--level <= 0)
        return NULL;
      iw = iw->focus_redirect.group->w;
    }
  }
}

/*
 * Call notify_focus (with FOCUS_MOUSE_[IN|OUT] as focus state),
 * function in right widget (the one who get, and the one who lose focus).
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y, unsigned int state) {
  xitk_widget_t *w;
  widget_event_t event;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  wl->mouse.x = x;
  wl->mouse.y = y;
  wl->qual    = state;

  w = wl->widget_pressed;
  if (w && (state & MODIFIER_BUTTON1)) {
    /* While holding a widget, it is the only one relevant. */
    if ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) {
      /* Convenience: while holding the slider, user need not stay within its
       * graphical bounds. Just move to closest possible. */
      wl->widget_focused   = w;
      event.type           = WIDGET_EVENT_CLICK;
      event.x              = x;
      event.y              = y;
      event.button_pressed = LBUTTON_DOWN;
      event.button         = 1;
      event.modifier       = state;
      w->event (w, &event, NULL);
    } else {
      /* For others, just (un)focus as needed. */
      uint32_t new_state = w->state & ~(XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS);
      if (xitk_widget_test_focus_redirect (w, x, y)) {
        new_state |= XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS;
        wl->widget_under_mouse = w;
      } else {
        wl->widget_under_mouse = NULL;
      }
      if (new_state != w->state) {
        w->state = new_state;
        _xitk_widget_paint (w, &event);
      }
    }
    return;
  }

  w = xitk_widget_test_focus_redirect (wl->widget_under_mouse, x, y);
  if (w) {
    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    if (w->event (w, &event, NULL) == 2)
      w = NULL;
  }
  if (!w)
    w = xitk_get_widget_at (wl, x, y);
  xitk_widget_apply_focus_redirect (&w);

  do {
    int f;
    if (!wl->widget_under_mouse) {
      /* maybe take over old focus? */
      wl->widget_under_mouse = wl->widget_focused;
      if (!wl->widget_under_mouse)
        break;
      /* HACK: menu sub branch opens with first entry focused. over it anyway to show sub sub. */
      if ((wl->widget_under_mouse->type & WIDGET_GROUP_MENU) && (w == wl->widget_under_mouse))
        break;
    }
    if (w == wl->widget_under_mouse) {
      /* inputtext uses this to set mouse pointer. */
      wl->widget_under_mouse->state |= XITK_WIDGET_STATE_MOUSE;
      if (wl->widget_under_mouse->state != wl->widget_under_mouse->shown_state)
        _xitk_widget_paint (wl->widget_under_mouse, &event);
      return;
    }
    f = wl->widget_under_mouse == wl->widget_focused;
    if (!(wl->widget_under_mouse->type & WIDGET_FOCUSABLE))
      break;
    if (!(wl->widget_under_mouse->state & XITK_WIDGET_STATE_ENABLE))
      break;
    if (f && (wl->widget_under_mouse->type & WIDGET_KEEP_FOCUS)) {
      if ((wl->widget_under_mouse->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) {
        wl->widget_under_mouse->state &= ~XITK_WIDGET_STATE_MOUSE;
        if (wl->widget_under_mouse->state != wl->widget_under_mouse->shown_state)
          _xitk_widget_paint (wl->widget_under_mouse, &event);
        break;
      }
      if (wl->widget_under_mouse->type & WIDGET_GROUP_COMBO)
        break;
      if (!w)
        break;
    }
    wl->widget_under_mouse->state &= f ? ~(XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS) : ~XITK_WIDGET_STATE_MOUSE;
    if (wl->widget_under_mouse->state != wl->widget_under_mouse->shown_state)
      _xitk_widget_paint (wl->widget_under_mouse, &event);
    if (f)
      wl->widget_focused = NULL;
  } while (0);

  wl->widget_under_mouse = w;
  /* Only give focus and paint when tips are accepted, otherwise associated window is invisible.
   * This call may occur from MotionNotify directly after iconifying window.
   * Also, xitk_tips_show_widget_tips (tips, NULL) now saves us xitk_tips_hide_tips (tips). */
  if (!xitk_tips_show_widget_tips (wl->xitk->tips, w))
    return;

  if (w && (w->type & WIDGET_FOCUSABLE)) {
#if 0
    dump_widget_type(mywidget);
#endif
    /* If widget still marked pressed or focus received, it shall receive the focus again. */
    w->state |= (w == wl->widget_pressed) || (w == wl->widget_focused)
      ? (XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS) : XITK_WIDGET_STATE_MOUSE;
    _xitk_widget_paint (w, &event);
    if (w->type & WIDGET_GROUP_MENU)
      menu_auto_pop (w);
  }
}

/*
 * Call notify_focus (with FOCUS_[RECEIVED|LOST] as focus state),
 * then call notify_click function, if exist, of widget at coords x/y.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp, int modifier) {
  int                    bRepaint = 0;
  xitk_widget_t         *w, *menu = NULL;
  widget_event_t         event;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  wl->mouse.x = x;
  wl->mouse.y = y;
  wl->qual    = modifier;

  if ((modifier & MODIFIER_BUTTON1) && (button != 1)
    && wl->widget_pressed && ((wl->widget_pressed->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    /* keep held slider */
    w = wl->widget_focused = wl->widget_pressed;
  } else {
    w = xitk_get_widget_at (wl, x, y);
    xitk_widget_apply_focus_redirect (&w);
  }
  if (w) {
    if (x > w->x + w->width - 1)
      x = w->x + w->width - 1;
    else if (x < w->x)
      x = w->x;
    if (y > w->y + w->height - 1)
      y = w->y + w->height - 1;
    else if (y < w->y)
      y = w->y;
  }

  if (!bUp) {

    if (w != wl->widget_focused) {

      if (wl->widget_focused) {
        xitk_tips_hide_tips (wl->xitk->tips);
        if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
          (wl->widget_focused->state & XITK_WIDGET_STATE_ENABLE)) {
          if (wl->widget_focused->type & WIDGET_GROUP_MENU)
            menu = xitk_menu_get_menu (wl->widget_focused);
          wl->widget_focused->state &= ~(XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS);
        }
        _xitk_widget_paint (wl->widget_focused, &event);
      }

      wl->widget_focused = w;

      if (w) {
        if ((w->type & WIDGET_FOCUSABLE) && (w->state & XITK_WIDGET_STATE_ENABLE)) {
          w->state |= XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS;
        } else {
          wl->widget_focused = NULL;
        }
        _xitk_widget_paint (w, &event);
      }

    } else {

      if (wl->widget_under_mouse && w && (wl->widget_under_mouse == w)) {
        if ((w->type & WIDGET_FOCUSABLE) && (w->state & XITK_WIDGET_STATE_ENABLE)) {
          w->state |= XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS;
        }
      }

    }

    if (button == 1)
      wl->widget_pressed = w;

    if (w) {
      xitk_tips_hide_tips(wl->xitk->tips);

      if ((w->type & WIDGET_CLICKABLE) && (w->state & XITK_WIDGET_STATE_ENABLE)) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = button;
        event.modifier       = modifier;

        bRepaint |= w->event (w, &event, NULL);
      }
    }

  } else { /* bUp */

    if(wl->widget_pressed) {
      if ((wl->widget_pressed->type & WIDGET_CLICKABLE) &&
        (wl->widget_pressed->state & XITK_WIDGET_STATE_ENABLE)) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_UP;
	event.button         = button;
        event.modifier       = modifier;

        bRepaint |= wl->widget_pressed->event (wl->widget_pressed, &event, NULL);

        /* user may still hold a slider (see xitk_motion_notify_widget_list ()).
         * ungrab it here. */
        if (wl->widget_pressed && (w != wl->widget_pressed) && (button == 1)) {
          if (wl->widget_pressed->type & WIDGET_FOCUSABLE) {
            wl->widget_pressed->state &= ~(XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS);
            wl->widget_focused = NULL;
            _xitk_widget_paint (wl->widget_pressed, &event);
          }
        }
      }

      if (button == 1)
        wl->widget_pressed = NULL;
    }
  }

  if(!(wl->widget_focused) && menu)
    wl->widget_focused = menu;

  return((bRepaint == 1));
}

/*
 * Find the first focusable widget in wl, according to direction
 */

static int xitk_widget_pos_cmp (void *a, void *b) {
  xitk_widget_t *d = (xitk_widget_t *)a;
  xitk_widget_t *e = (xitk_widget_t *)b;
  int gd, ge;
  /* sort widgets by center top down then left right. keep groups together. */
  if (d->parent.group != e->parent.group) {
    if (d->parent.group)
      d = d->parent.group->w;
    if (e->parent.group)
      e = e->parent.group->w;
  }
  gd = ((uint32_t)(d->y + (d->height >> 1)) << 16) + d->x + (d->width >> 1);
  ge = ((uint32_t)(e->y + (e->height >> 1)) << 16) + e->x + (e->width >> 1);
  return gd - ge;
}

static xitk_widget_t *xitk_find_nextprev_focus (xitk_widget_list_t *wl, int backward) {
  int i, n;
  xitk_widget_t *w;
  xine_sarray_t *a = xine_sarray_new (128, xitk_widget_pos_cmp);

  if (!a)
    return NULL;
  n = 0;
  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if (!(w->type & WIDGET_TABABLE) ||
      ((w->state & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE)) != (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE))
      || !w->width || !w->height)
      continue;
    xine_sarray_add (a, w);
    n += 1;
  }
  if (!n) {
    xine_sarray_delete (a);
    return NULL;
  }

  i = backward ? 0 : n - 1;
  w = wl->widget_focused;
  if (w) {
    int j;
    if (!(w->type & WIDGET_TABABLE) && w->parent.group && (w->parent.group->w->type & WIDGET_TABABLE))
      wl->widget_focused = w = w->parent.group->w;
    j = xine_sarray_binary_search (a, w);
    if (j >= 0)
      i = j;
  }

  if (backward) {
    i = i > 0 ? i - 1 : n - 1;
  } else {
    i = i < n - 1 ? i + 1 : 0;
  }
  w = xine_sarray_get (a, i);
  xine_sarray_delete (a);
  return w;
}

void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward, int modifier) {
  widget_event_t event;
  xitk_widget_t *w;

  (void)modifier;
  if (!wl)
    return;
  if (!wl->xwin)
    return;

  w = xitk_find_nextprev_focus (wl, backward);
  if (!w || (w == wl->widget_focused))
    return;

  if (wl->widget_focused) {
    if ((wl->widget_focused->type & (WIDGET_FOCUSABLE | WIDGET_TABABLE)) &&
      (wl->widget_focused->state & XITK_WIDGET_STATE_ENABLE)) {
      wl->widget_focused->state &= ~XITK_WIDGET_STATE_FOCUS;
      _xitk_widget_paint (wl->widget_focused, &event);
    }
  }
  wl->widget_focused = w;

  xitk_tips_hide_tips (wl->xitk->tips);

  if ((wl->widget_focused->type & (WIDGET_FOCUSABLE | WIDGET_TABABLE)) &&
      (wl->widget_focused->state & XITK_WIDGET_STATE_ENABLE)) {
    /* NOTE: this may redirect focus to a group member. rereaad wl->widget_focused. */
    wl->widget_focused->state |= XITK_WIDGET_STATE_FOCUS;
    _xitk_widget_paint (wl->widget_focused, &event);
  }
}

/*
 *
 */
void xitk_set_focus_to_widget(xitk_widget_t *w) {
  xitk_widget_t       *widget;
  widget_event_t       event;
  xitk_widget_list_t  *wl;

  if (!w)
    return;
  wl = w->wl;
  if (!wl) {
    XITK_WARNING("widget list is NULL.\n");
    return;
  }
  if (!wl->xwin)
    return;

  /* paranois: w (still) in list? */
  widget = (xitk_widget_t *)wl->list.head.next;
  while (widget->node.next && (widget != w))
    widget = (xitk_widget_t *)widget->node.next;

  if (widget->node.next) {

    if(wl->widget_focused) {

      /* Kill (hide) tips */
      xitk_tips_hide_tips(wl->xitk->tips);

      if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	  (wl->widget_focused->state & XITK_WIDGET_STATE_ENABLE)) {
        wl->widget_focused->state &= ~XITK_WIDGET_STATE_FOCUS;
        _xitk_widget_paint (wl->widget_focused, &event);
      }
    }

    wl->widget_focused = widget;

    if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	(wl->widget_focused->state & XITK_WIDGET_STATE_ENABLE)) {
      wl->widget_focused->state |= XITK_WIDGET_STATE_FOCUS;
      _xitk_widget_paint (wl->widget_focused, &event);
    }
  }
  else
    XITK_WARNING("widget not found in list.\n");
}

/*
 * Return the focused widget.
 */
xitk_widget_t *xitk_get_focused_widget(xitk_widget_list_t *wl) {

  if(wl) {
    return wl->widget_focused;
  }

  XITK_WARNING("widget list is NULL\n");
  return NULL;
}

/*
 * Return the pressed widget.
 */
xitk_widget_t *xitk_get_pressed_widget(xitk_widget_list_t *wl) {

  if(wl)
    return wl->widget_pressed;

  XITK_WARNING("widget list is NULL\n");
  return NULL;
}

/*
 * Return widget width.
 */
int xitk_get_widget_width(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return -1;
  }
  return w->width;
}

/*
 * Return widget height.
 */
int xitk_get_widget_height(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return -1;
  }
  return w->height;
}

/*
 * Set position of a widget.
 */
int xitk_set_widget_pos(xitk_widget_t *w, int x, int y) {
  /* do not warn here as this may be NULL intentionally,
   * eg with optional sub widgets in an intbox. */
  if (!w)
    return 0;
  if ((w->x == x) && (w->y == y))
    return 1;
  w->x = x;
  w->y = y;
  if (w->wl && (w->wl->widget_under_mouse == w))
    w->wl->widget_under_mouse = NULL;
  return 1;
}

/*
 * Get position of a widget.
 */
int xitk_get_widget_pos(xitk_widget_t *w, int *x, int *y) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    *x = *y = 0;
    return 0;
  }
  *x = w->x;
  *y = w->y;
  return 1;
}

uint32_t xitk_get_widget_type(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }

  return w->type;
}

/*
 * Return 1 if widget is enabled.
 */
int xitk_is_widget_enabled(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  return !!(w->state & XITK_WIDGET_STATE_ENABLE);
}

/*
 * Return 1 if widget have focus.
 */
int xitk_is_widget_focused(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  return !!(w->state & XITK_WIDGET_STATE_FOCUS);
}

static void xitk_widget_paint (xitk_widget_t *w) {
  widget_event_t  event;

  if (!(w->state & XITK_WIDGET_STATE_VISIBLE) && w->wl && (w->wl->widget_under_mouse == w))
    xitk_tips_hide_tips (w->wl->xitk->tips);
  _xitk_widget_paint (w, &event);
}

static void _xitk_widget_able (xitk_widget_t *w) {
  if ((w->state ^ w->shown_state) & (XITK_WIDGET_STATE_UNSET | XITK_WIDGET_STATE_ENABLE)) {
    if (w->wl) {
      if (w->state & XITK_WIDGET_STATE_ENABLE) {
        int dx = w->wl->mouse.x - w->x, dy = w->wl->mouse.y - w->y;
        if ((dx >= 0) && (dx < w->width) && (dy >= 0) && (dy < w->height)) {
          /* enabling the widget under mouse. how often does this happen ?? */
          xitk_motion_notify_widget_list (w->wl, w->wl->mouse.x, w->wl->mouse.y, w->wl->qual);
        }
      } else {
        if (w == w->wl->widget_under_mouse)
          xitk_tips_hide_tips (w->wl->xitk->tips);
        if (w->type & WIDGET_FOCUSABLE)
          w->state &= ~(XITK_WIDGET_STATE_FOCUS | XITK_WIDGET_STATE_MOUSE);
        if (w == w->wl->widget_focused)
          w->wl->widget_focused = NULL;
      }
    }
    if (w->type & WIDGET_GROUP) {
      widget_event_t  event;

      event.type = WIDGET_EVENT_ENABLE;
      w->event (w, &event, NULL);
    }
  }
}

/*
 * Destroy widgets.
 */
void xitk_widgets_delete (xitk_widget_t **w, unsigned int n) {
  if (w) {
    w += n;
    for (; n; n--) {
      xitk_widget_t *_w = *--w;

      if (_w) {
        widget_event_t event;

        xitk_clipboard_unregister_widget (_w);
        _w->state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
        _xitk_widget_able (_w);
        if ((_w->state ^ _w->shown_state) & XITK_WIDGET_STATE_PAINT)
          xitk_widget_paint (_w);

        xitk_widget_rel_deinit (&_w->parent);
        xitk_widget_rel_deinit (&_w->focus_redirect);

        if (_w->wl) {
          if (_w == _w->wl->widget_focused)
            _w->wl->widget_focused = NULL;
          if (_w == _w->wl->widget_under_mouse) {
            xitk_tips_hide_tips (_w->wl->xitk->tips);
            _w->wl->widget_under_mouse = NULL;
          }
          if (_w == _w->wl->widget_pressed)
            _w->wl->widget_pressed = NULL;
        }

        xitk_dnode_remove (&_w->node);

        event.type = WIDGET_EVENT_DESTROY;
        _w->event (_w, &event, NULL);

        XITK_FREE (_w->tips_string);
        free (_w);
        *w = NULL;
      }
    }
  }
}

/*
 * Destroy widgets from widget list.
 */
void xitk_destroy_widgets(xitk_widget_list_t *wl) {
  int again;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  /* do group leaders first, just in case they want to delete their members as well. */
  do {
    xitk_widget_t *w = (xitk_widget_t *)wl->list.tail.prev;
    again = 0;
    while (w->node.prev) {
      if (w->type & WIDGET_GROUP) {
        xitk_destroy_widget (w);
        again = 1;
        break;
      }
      w = (xitk_widget_t *)w->node.prev;
    }
  } while (again);

  while (1) {
    xitk_widget_t *mywidget = (xitk_widget_t *)wl->list.tail.prev;
    if (!mywidget->node.prev)
      break;
    xitk_destroy_widget(mywidget);
  }
}

#if 0
/*
 * Return the struct of color names/values.
 */
xitk_color_names_t *gui_get_color_names(void) {

  return xitk_color_names;
}
#endif

/*
 * Stop widgets from widget list.
 */
#ifdef YET_UNUSED
void xitk_stop_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  mywidget = (xitk_widget_t *)wl->list.head.next;

  while (mywidget->node.next) {

    xitk_stop_widget(mywidget);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}
#endif

/*
 * Show widgets from widget list.
 */
void xitk_show_widgets (xitk_widget_list_t *wl) {
  xitk_widget_t *w;

  if (!wl) {
    XITK_WARNING ("widget list was NULL.\n");
    return;
  }
  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if (w->saved_state & XITK_WIDGET_STATE_VISIBLE) {
      w->state |= XITK_WIDGET_STATE_VISIBLE;
      if ((w->state ^ w->shown_state) & XITK_WIDGET_STATE_PAINT)
        xitk_widget_paint (w);
    }
  }
}

/*
 * Hide widgets from widget list..
 */
void xitk_hide_widgets (xitk_widget_list_t *wl) {
  xitk_widget_t *w;

  if (!wl) {
    XITK_WARNING ("widget list was NULL.\n");
    return;
  }

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    w->saved_state = w->state;
    w->state &= ~XITK_WIDGET_STATE_VISIBLE;
    if ((w->state ^ w->shown_state) & XITK_WIDGET_STATE_PAINT)
      xitk_widget_paint (w);
  }
}

unsigned int xitk_widgets_state (xitk_widget_t * const *w, unsigned int n, unsigned int mask, unsigned int state) {
  uint32_t _and, _or, _new = 0;

  mask &= XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE | XITK_WIDGET_STATE_ON |
    XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS;
  /* disable implies lose focus. */
  if (mask & ~state & XITK_WIDGET_STATE_ENABLE) {
    mask |= XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS;
    state &= ~(XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS);
  }
  _or = state & mask;
  _and = ~mask;

  if (!w)
    return 0;
  for (; n; n--) {
    xitk_widget_t *_w = *w++;

    if (_w) {
      uint32_t d;

      _w->state &= _and;
      _w->state |= _or;
      _xitk_widget_able (_w);
      _new = _w->state;
      d = _w->state ^ _w->shown_state;
      if ((d & (XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS)) && _w->wl) {
        if (d & XITK_WIDGET_STATE_MOUSE) {
          if (_w->state & XITK_WIDGET_STATE_MOUSE) {
            _w->wl->widget_under_mouse = _w;
          } else {
            if (_w == _w->wl->widget_under_mouse)
              _w->wl->widget_under_mouse = NULL;
          }
        }
        if (d & XITK_WIDGET_STATE_FOCUS) {
          if (_w->state & XITK_WIDGET_STATE_FOCUS) {
            _w->wl->widget_focused = _w;
          } else {
            if (_w == _w->wl->widget_focused)
              _w->wl->widget_focused = NULL;
          }
        }
      }
      if (d & XITK_WIDGET_STATE_PAINT)
        xitk_widget_paint (_w);
    }
  }
  return _new;
}

/*
 *
 */
xitk_image_t *xitk_get_widget_foreground_skin(xitk_widget_t *w) {
  widget_event_t         event;
  widget_event_result_t  result;
  xitk_image_t          *image = NULL;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return NULL;
  }

  event.type = WIDGET_EVENT_GET_SKIN;
  event.skin_layer = FOREGROUND_SKIN;

  if(w->event(w, &event, &result))
    image = result.image;

  return image;
}

/*
 *
 */
#ifdef YET_UNUSED
xitk_image_t *xitk_get_widget_background_skin(xitk_widget_t *w) {
  widget_event_t         event;
  widget_event_result_t  result;
  xitk_image_t          *image = NULL;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return NULL;
  }

  event.type = WIDGET_EVENT_GET_SKIN;
  event.skin_layer = BACKGROUND_SKIN;

  if(w->event(w, &event, &result))
    image = result.image;

  return image;
}
#endif

void xitk_set_widgets_tips_timeout (xitk_widget_list_t *wl, unsigned int timeout) {
  if (wl) {
    xitk_widget_t *w;
    for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
      w->tips_timeout = timeout;
/*
      if (w->type & (WIDGET_GROUP | WIDGET_GROUP_MEMBER)) {
        widget_event_t  event;

        event.type         = WIDGET_EVENT_TIPS_TIMEOUT;
        event.tips_timeout = timeout;
        w->event (w, &event, NULL);
      }
*/
    }
  }
}

void xitk_set_widget_tips_and_timeout (xitk_widget_t *w, const char *str, unsigned int timeout) {
  if (w) {
    if (w->wl && (w == w->wl->widget_under_mouse))
      xitk_tips_hide_tips (w->wl->xitk->tips);
    if (str != XITK_TIPS_STRING_KEEP) {
      XITK_FREE (w->tips_string);
      if (str)
        w->tips_string = strdup (str);
    }
    w->tips_timeout = timeout;
    if (w->type & (WIDGET_GROUP | WIDGET_GROUP_MEMBER)) {
      widget_event_t  event;

      event.type         = WIDGET_EVENT_TIPS_TIMEOUT;
      event.tips_timeout = timeout;
      w->event (w, &event, NULL);
    }
  }
}

unsigned int xitk_get_widget_tips_timeout (xitk_widget_t *w) {
  return w ? w->tips_timeout : 0;
}

int xitk_widget_mode (xitk_widget_t *w, int mask, int mode) {
  if (!w)
    return 0;

  mask &= WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE;
  w->type = (w->type & ~mask) | (mode & mask);
  return w->type & (WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE);
}

void xitk_add_widget (xitk_widget_list_t *wl, xitk_widget_t *wi, unsigned int flags) {
  if (wl && wi) {
    xitk_dlist_add_tail (&wl->list, &wi->node);
    if (flags != XITK_WIDGET_STATE_KEEP) {
      wi->state &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
      wi->state |= flags & (XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
      _xitk_widget_able (wi);
      if ((wi->state ^ wi->shown_state) & XITK_WIDGET_STATE_PAINT)
        xitk_widget_paint (wi);
    }
  }
}

int xitk_widget_key_event (xitk_widget_t *w, const char *string, int modifier) {
  widget_event_t event;
  int handled;

  if (!w || !string)
    return 0;
  if (!string[0])
    return 0;

  event.type = WIDGET_EVENT_KEY;
  event.string = string;
  event.modifier = modifier;
  handled = 0;

  if (w->type & WIDGET_KEYABLE)
    handled = w->event (w, &event, NULL);

  if (!handled && w->parent.group && (w->parent.group->w->type & WIDGET_KEYABLE))
    handled = w->parent.group->w->event (w->parent.group->w, &event, NULL);

  return handled;
}


xitk_widget_t *xitk_widget_new (xitk_widget_list_t *wl, size_t size) {
  xitk_widget_t *w;

  if (size < sizeof (*w))
    size = sizeof (*w);
  w = xitk_xmalloc (size);
  if (!w)
    return NULL;

  w->node.next = w->node.prev = NULL;

  w->wl = wl;
  xitk_widget_rel_init (&w->parent, w);
  xitk_widget_rel_init (&w->focus_redirect, w);

  w->x = w->y = w->width = w->height = 0;

  w->type = 0;
  w->state =
  w->saved_state = XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE;
  w->shown_state = XITK_WIDGET_STATE_UNSET;

  w->event = NULL;

  w->tips_timeout = 0;
  w->tips_string = NULL;

  w->private_data = size > sizeof (*w) ? w : NULL;

  return w;
};

void xitk_widget_set_parent (xitk_widget_t *w, xitk_widget_t *parent) {
  if (!w)
    return;
  xitk_widget_rel_join (&w->parent, parent ? &parent->parent : NULL);
}

void xitk_widget_set_focus_redirect (xitk_widget_t *w, xitk_widget_t *focus_redirect) {
  if (!w)
    return;
  xitk_widget_rel_join (&w->focus_redirect,
    focus_redirect && (focus_redirect->wl == w->wl) ? &focus_redirect->focus_redirect : NULL);
}
