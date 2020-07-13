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

#include "menu.h"

#include <stdio.h>
#include <string.h>

#include "_xitk.h"

#include "xitk_x11.h"

#include "utils.h"

#undef DEBUG_MENU
#undef DUMP_MENU

#define _MENU_MAX_OPEN 5 /* xine needs just 3. */

typedef struct _menu_node_s _menu_node_t;
typedef struct _menu_private_s _menu_private_t;

typedef struct {
  xitk_window_t        *xwin;
  xitk_register_key_t   key;
  _menu_private_t      *wp;
  _menu_node_t         *node;
  xitk_image_t         *bevel_plain;
  xitk_image_t         *bevel_arrow;
  xitk_image_t         *bevel_unchecked;
  xitk_image_t         *bevel_checked;
} _menu_window_t;

#define _MENU_NODE_PLAIN 1
#define _MENU_NODE_SEP 2
#define _MENU_NODE_BRANCH 4
#define _MENU_NODE_CHECK 8
#define _MENU_NODE_CHECKED 16
#define _MENU_NODE_TITLE 32
#define _MENU_NODE_SHORTCUT 64
#define _MENU_NODE_HAS 8

struct _menu_node_s {
  xitk_dnode_t         node;
  _menu_node_t        *parent;
  xitk_dlist_t         branches;
  _menu_private_t     *wp;
  _menu_window_t      *menu_window;
  xitk_widget_t       *button;
  int                  num;
  unsigned int         type;
  xitk_menu_entry_t    menu_entry;
};
  
struct _menu_private_s {
  xitk_widget_t        w;
  _menu_node_t         root;
  int                  x, y, num_open;
  _menu_window_t       open_windows[_MENU_MAX_OPEN];
};

static void _menu_tree_free (_menu_node_t *root) {
  _menu_node_t *this = (_menu_node_t *)root->branches.tail.prev;
  while (this->node.prev) {
    _menu_node_t *prev = (_menu_node_t *)this->node.prev;
    xitk_dnode_remove (&this->node);
    _menu_tree_free (this);
    free (this);
    this = prev;
  }
}

static _menu_node_t *_menu_node_new (_menu_private_t *wp, xitk_menu_entry_t *me, const char *name, size_t lname) {
  _menu_node_t *node;
  size_t lshort = (me->shortcut ? strlen (me->shortcut) : 0) + 1;
  size_t ltype = (me->type ? strlen (me->type) : 0) + 1;
  char *s;

  lname += 1;
  s = (char *)xitk_xmalloc (sizeof (_menu_node_t) + lname + lshort + ltype);
  if (!s)
    return NULL;
  node = (_menu_node_t *)s;
  s += sizeof (*node);

  node->menu_entry.menu = s;
  memcpy (s, name, lname);
  s += lname;

  node->type = _MENU_NODE_PLAIN;
  node->menu_entry.type = NULL;
  if (me->type) {
    node->menu_entry.type = s;
    memcpy (s, me->type, ltype);
    if (!strcasecmp (s, "<separator>")) {
      node->type = _MENU_NODE_SEP;
    } else if (!strcasecmp (s, "<branch>")) {
      node->type = _MENU_NODE_BRANCH;
    } else if (!strcasecmp (s, "<check>")) {
      node->type = _MENU_NODE_CHECK;
    } else if (!strcasecmp (s, "<checked>")) {
      node->type = _MENU_NODE_CHECKED;
    } else if (!strcasecmp (s, "<title>")) {
      node->type = _MENU_NODE_TITLE;
    }
    s += ltype;
  }

  if (me->shortcut) {
    node->menu_entry.shortcut = s;
    memcpy (s, me->shortcut, lshort);
    s += lshort;
    node->type |= _MENU_NODE_SHORTCUT;
  } else {
    node->menu_entry.shortcut = NULL;
  }

  node->menu_entry.cb        = me->cb;
  node->menu_entry.user_data = me->user_data;
  node->menu_entry.user_id   = me->user_id;

  node->node.next = NULL;
  node->node.prev = NULL;
  node->parent = NULL;
  xitk_dlist_init (&node->branches);
  node->menu_window = NULL;
  node->button = NULL;
  node->wp = wp;
  node->num = 0;
  return node;
}

void xitk_menu_add_entry (xitk_widget_t *w, xitk_menu_entry_t *me) {
  _menu_private_t *wp = (_menu_private_t *)w;
  _menu_node_t *here;
  char buf[400], *e = buf + sizeof (buf) - 1;
  const char *p;

  if (!w || !me)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MENU)
    return;

  here = &wp->root;
  p = me->menu ? me->menu : "";

  while (p[0]) {
    _menu_node_t *scan;
    char *q = buf;
    int found;

    while ((q < e) && p[0] && (p[0] != '/')) {
      if ((p[0] == '\\') && p[1])
        *q++ = p[1], p += 2;
      else
        *q++ = p[0], p += 1;
    }
    *q = 0;
    while (p[0] && (p[0] != '/'))
      p += 1;

    found = 0;
    if (p[0] == '/') {
      /* merge branchhes */
      for (scan = (_menu_node_t *)here->branches.tail.prev; scan->node.prev; scan = (_menu_node_t *)scan->node.prev) {
        if (!strcmp (scan->menu_entry.menu, buf))
          break;
      }
      if (scan->node.prev)
        found = 1;
    }
    if (found) {
      here = scan;
    } else { /* new */
      _menu_node_t *add = _menu_node_new (wp, me, buf, q - buf);
      if (!add)
        break;
      add->parent = here;
      xitk_dlist_add_tail (&here->branches, &add->node);
      here->num += 1;
      here->type |= (add->type << _MENU_NODE_HAS) | _MENU_NODE_BRANCH;
      here = add;
    }
    if (p[0] == '/')
      p += 1;
  }
}

static void _menu_close_1 (_menu_private_t *wp) {
  _menu_window_t *mw;

  if (wp->num_open <= 0)
    return;
  wp->num_open -= 1;
  mw = wp->open_windows + wp->num_open;
  xitk_unregister_event_handler (&mw->key);
  xitk_image_free_image (&mw->bevel_plain);
  xitk_image_free_image (&mw->bevel_arrow);
  xitk_image_free_image (&mw->bevel_unchecked);
  xitk_image_free_image (&mw->bevel_checked);
  xitk_window_destroy_window (mw->xwin);
  mw->xwin = NULL;
  mw->node->menu_window = NULL;
  if ((wp->num_open > 0) && xitk_window_is_window_visible (mw[-1].xwin))
    xitk_window_try_to_set_input_focus (mw[-1].xwin);
}

static void _menu_close_subs_in (_menu_private_t *wp, _menu_node_t *node) {
  if (!node)
    node = &wp->root;
  while (wp->num_open > 0) {
    _menu_close_1 (wp);
    if (wp->open_windows[wp->num_open].node == node)
      break;
  }
}

static void _menu_close_subs_ex (_menu_private_t *wp, _menu_node_t *node) {
  if (!node)
    node = &wp->root;
  while (wp->num_open > 0) {
    if (wp->open_windows[wp->num_open - 1].node == node)
      break;
    _menu_close_1 (wp);
  }
}

static int _menu_show_subs (_menu_private_t *wp, _menu_node_t *node) {
  int i;

  for (i = 1; i < wp->num_open; i++) {
    if (wp->open_windows[i].node == node)
      return 1;
  }
  return 0;
}

static void _menu_exit (_menu_private_t *wp) {
  /* 핵: this runs inside xitk_destroy_widget ().
   * dont destroy again through xitk_set_current_menu (NULL). */
  xitk_unset_current_menu ();
  _menu_close_subs_in (wp, NULL);
  _menu_tree_free (&wp->root);
}

static void _menu_open (_menu_node_t *branch, int x, int y);

static void _menu_click_cb(xitk_widget_t *w, void *data) {
  _menu_node_t *me = (_menu_node_t *)data;
  _menu_private_t *wp = me->wp;

  (void)w;
  if (me->type & _MENU_NODE_BRANCH) {
    if (me->num > 0) {
      if (!me->menu_window) {
        int wx = 0, wy = 0, x = 0, y = 0;

        xitk_window_get_window_position (me->parent->menu_window->xwin, &wx, &wy, NULL, NULL);
        xitk_get_widget_pos (me->button, &x, &y);

        x += xitk_get_widget_width (me->button) + wx;
        y += wy;

        _menu_close_subs_ex (wp, me->parent);
        _menu_open (me, x, y);
      } else {
        xitk_window_raise_window (me->menu_window->xwin);
        xitk_window_set_input_focus (me->menu_window->xwin);
      }
    } else {
      if (_menu_show_subs (wp, me)) {
        _menu_close_subs_ex (wp, me);
      }
    }
  } else if (!(me->type & (_MENU_NODE_TITLE | _MENU_NODE_SEP))) {
    _menu_close_subs_in (wp, NULL);
    /* 핵: detach from parent window. it may go away in user callback,
     * eg when switching fullscreen mode. */
    xitk_dnode_remove (&wp->w.node);
    wp->w.wl = NULL;
    if (me->menu_entry.cb)
      me->menu_entry.cb (&wp->w, &me->menu_entry, me->menu_entry.user_data);
    xitk_destroy_widget (&wp->w);
  }
}

static void _menu_open (_menu_node_t *node, int x, int y) {
  _menu_private_t *wp = node->wp;
  xitk_skin_element_info_t    info;
  xitk_labelbutton_widget_t   lb;
  int                         bentries, bsep, btitle, rentries;
  _menu_node_t               *maxnode, *me;
  int                         maxlen, wwidth, wheight, swidth, sheight, shortcutlen = 0, shortcutpos = 0;
  xitk_font_t                *fs;
  static xitk_window_t       *xwin;
  _menu_window_t             *mw;
  xitk_widget_t              *btn;
  xitk_pixmap_t              *bg = NULL;
  int                         yy = 0, got_title = 0;

  if (wp->num_open >= _MENU_MAX_OPEN)
    return;
  if (wp->num_open > 0) {
    if (node->parent != wp->open_windows[wp->num_open - 1].node)
      return;
  } else {
    if (node->parent != NULL)
      return;
  }

  XITK_WIDGET_INIT(&lb);
  
  bentries = node->num;
  bsep = 0;
  maxlen = 0;
  for (maxnode = me = (_menu_node_t *)node->branches.head.next; me->node.next; me = (_menu_node_t *)me->node.next) {
    if (me->type & _MENU_NODE_SEP) {
      bsep += 1;
    } else {
      /* FIXME: non fixed width font? */
      int len = strlen (me->menu_entry.menu);
      if (len > maxlen) {
        maxlen = len;
        maxnode = me;
      }
    }
  }
  btitle   = !!(node->type & (_MENU_NODE_TITLE << _MENU_NODE_HAS));
  rentries = bentries - bsep;
  
  if (maxnode->type & _MENU_NODE_TITLE)
    fs = xitk_font_load_font (wp->w.wl->xitk, DEFAULT_BOLD_FONT_14);
  else
    fs = xitk_font_load_font (wp->w.wl->xitk, DEFAULT_BOLD_FONT_12);

  xitk_font_set_font (fs, wp->w.wl->gc);
  maxlen = xitk_font_get_string_length (fs, maxnode->menu_entry.menu);
  xitk_font_unload_font(fs);

  shortcutlen = 0;
  if (xitk_get_menu_shortcuts_enability () && (node->type & (_MENU_NODE_SHORTCUT << _MENU_NODE_HAS))) {
    xitk_font_t *short_font;
    _menu_node_t *smaxnode;
    for (smaxnode = me = (_menu_node_t *)node->branches.head.next; me->node.next; me = (_menu_node_t *)me->node.next) {
      if (me->menu_entry.shortcut) {
        /* FIXME: non fixed width font? */
        int len = strlen (me->menu_entry.shortcut);
        if (len > shortcutlen) {
          shortcutlen = len;
          smaxnode = me;
        }
      }
    }
    short_font = xitk_font_load_font(wp->w.wl->xitk, DEFAULT_FONT_12);
    shortcutlen = xitk_font_get_string_length (short_font, smaxnode->menu_entry.shortcut);
    xitk_font_unload_font(short_font);
    maxlen += shortcutlen + 15;
  }

  wwidth = maxlen + 40;

  if (node->type & ((_MENU_NODE_CHECK | _MENU_NODE_CHECKED | _MENU_NODE_BRANCH) << _MENU_NODE_HAS))
    wwidth += 20;
  wheight = (rentries * 20) + (bsep * 2) + (btitle * 2);

  shortcutpos = (wwidth - shortcutlen) - 15;

  swidth = xitk_get_display_width();
  sheight = xitk_get_display_height();

  if (node->parent) {
    x -= 4; /* Overlap parent menu but leave text and symbols visible */
    y -= 1; /* Top item of submenu in line with parent item */
  }
  else {
    x++; y++; /* Upper left corner 1 pix distance to mouse pointer */
  }

  /* Check if menu fits on screen and adjust position if necessary in a way */
  /* that it doesn't obscure the parent menu or get under the mouse pointer */

  if((x + (wwidth + 2)) > swidth) { /* Exceeds right edge of screen */
    if (node->parent) {
      /* Align right edge of submenu to left edge of parent item */
      x -= xitk_get_widget_width (node->parent->button) + (wwidth + 2) - 4;
    }
    else {
      /* Align right edge of top level menu to right edge of screen */
      x  = swidth - (wwidth + 2);
    }
  }
  if((y + (wheight + 2)) > sheight) { /* Exceeds bottom edge of screen */
    if (node->parent) {
      /* Align bottom edge of submenu for bottom item in line with parent item */
      y -= wheight - xitk_get_widget_height (node->parent->button);
    }
    else {
      /* Align bottom edge of top level menu to requested (i.e. pointer) pos */
      y -= (wheight + 2) + 1;
    }
  }

  xwin = xitk_window_create_simple_window_ext(wp->w.wl->xitk,
                                              x, y, wwidth + 2, wheight + 2,
                                              NULL, NULL, NULL, 1, 1, NULL);

  if(bsep || btitle) {
    bg = xitk_window_get_background_pixmap(xwin);
  }

  mw = wp->open_windows + wp->num_open;
  wp->num_open += 1;

  mw->xwin = xwin;
  mw->wp = wp;
  mw->node = node;
  node->menu_window = mw;

  mw->bevel_plain = NULL;
  mw->bevel_arrow = NULL;
  mw->bevel_unchecked = NULL;
  mw->bevel_checked = NULL;

  if (node->type & (_MENU_NODE_PLAIN << _MENU_NODE_HAS)) {
    mw->bevel_plain = xitk_image_create_image (wp->w.wl->xitk, wwidth * 3, 20);
    if (mw->bevel_plain)
      draw_flat_three_state (mw->bevel_plain);
  }

  if (node->type & (_MENU_NODE_BRANCH << _MENU_NODE_HAS)) {
    mw->bevel_arrow = xitk_image_create_image (wp->w.wl->xitk, wwidth * 3, 20);
    if (mw->bevel_arrow) {
      draw_flat_three_state (mw->bevel_arrow);
      menu_draw_arrow_branch (mw->bevel_arrow);
    }
  }

  if (node->type & (_MENU_NODE_CHECK << _MENU_NODE_HAS)) {
    mw->bevel_unchecked = xitk_image_create_image (wp->w.wl->xitk, wwidth * 3, 20);
    if (mw->bevel_unchecked) {
      draw_flat_three_state (mw->bevel_unchecked);
      menu_draw_check (mw->bevel_unchecked, 0);
    }
  }

  if (node->type & (_MENU_NODE_CHECKED << _MENU_NODE_HAS)) {
    mw->bevel_checked = xitk_image_create_image (wp->w.wl->xitk, wwidth * 3, 20);
    if (mw->bevel_checked) {
      draw_flat_three_state (mw->bevel_checked);
      menu_draw_check (mw->bevel_checked, 1);
    }
  }

  memset (&info, 0, sizeof (info));
  info.x                 = 1;
  info.visibility        = 0;
  info.enability         = 0;
  info.pixmap_name       = NULL;
  info.pixmap_img.image  = mw->bevel_plain;
  info.label_alignment   = ALIGN_LEFT;
  info.label_printable   = 1;
  info.label_staticity   = 0;
  info.label_color       =
  info.label_color_focus = (char *)"Black";
  info.label_color_click = (char *)"White";
  info.label_fontname    = (char *)DEFAULT_BOLD_FONT_12;

  yy = 1;
  for (me = (_menu_node_t *)node->branches.head.next; me->node.next; me = (_menu_node_t *)me->node.next) {

    me->wp = wp;
    me->button = NULL;
    
    if (me->type & _MENU_NODE_TITLE) {

      if(bg && (!got_title)) {
	int           lbear, rbear, width, asc, des;
	unsigned int  cfg, cbg;

        fs = xitk_font_load_font(wp->w.wl->xitk, DEFAULT_BOLD_FONT_14);
	xitk_font_set_font(fs, wp->w.wl->gc);
	
	xitk_font_string_extent (fs, me->menu_entry.menu, &lbear, &rbear, &width, &asc, &des);

        cbg = xitk_get_pixel_color_from_rgb(wp->w.wl->xitk, 140, 140, 140);
        cfg = xitk_get_pixel_color_from_rgb(wp->w.wl->xitk, 255, 255, 255);

        pixmap_fill_rectangle(bg, 1, 1, wwidth, 20, cbg);

        xitk_pixmap_draw_string (bg, fs, 5, 1 + ((20 + asc + des) >> 1) - des,
          me->menu_entry.menu, strlen (me->menu_entry.menu), cfg);

	xitk_font_unload_font(fs);

	yy += 20;
	got_title = 1;

	goto __sep;
      }
    }
    else if (me->type & _MENU_NODE_SEP) {
    __sep:
      if(bg)
        draw_rectangular_inner_box_light(bg, 3, yy, wwidth - 6, 1);
      yy += 2;
    }
    else {
      xitk_widget_list_t *wl = xitk_window_widget_list(xwin);

      lb.button_type       = CLICK_BUTTON;
      lb.label             = me->menu_entry.menu;
      lb.align             = ALIGN_LEFT;
      lb.callback          = _menu_click_cb;
      lb.state_callback    = NULL;
      lb.userdata          = (void *) me;
      lb.skin_element_name = NULL;

      info.y = yy;
      info.pixmap_img.image = (me->type & _MENU_NODE_BRANCH)  ? mw->bevel_arrow
                            : (me->type & _MENU_NODE_CHECK)   ? mw->bevel_unchecked
                            : (me->type & _MENU_NODE_CHECKED) ? mw->bevel_checked
                            : mw->bevel_plain;

      btn = xitk_info_labelbutton_create (wl, &lb, &info);
      xitk_dlist_add_tail (&wl->list, &btn->node);

      btn->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MENU;
      me->button = btn;
      
      if(xitk_get_menu_shortcuts_enability()  && me->menu_entry.shortcut)
	xitk_labelbutton_change_shortcut_label (btn, me->menu_entry.shortcut, shortcutpos, DEFAULT_FONT_12);
      
      xitk_labelbutton_set_label_offset(btn, 20);
      xitk_enable_and_show_widget(btn);

      yy += 20;
    }
  }
  
  if(bg) {
    xitk_window_set_background(xwin, bg);
  }

  /* Set transient-for-hint to the immediate predecessor,     */
  /* so window stacking of submenus is kept upon raise/lower. */
  if (!node->parent)
    xitk_window_set_transient_for (xwin, wp->w.wl->win);
  else
    xitk_window_set_transient_for_win (xwin, node->parent->menu_window->xwin);
  mw->key = xitk_window_register_event_handler("xitk menu", mw->xwin, NULL, mw);

  xitk_window_show_window(xwin, 1);
  xitk_window_try_to_set_input_focus(xwin);

  if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
    /* WINDOW_TYPE_MENU seems to be the natural choice. */
    xitk_window_set_wm_window_type(xwin, WINDOW_TYPE_MENU);
  else
    /* However, KWin has unacceptable behaviour for WINDOW_TYPE_MENU in  */
    /* our transient-for scheme: The transient-for window must be mapped */
    /* and the transient-for window or another transient window (incl.   */
    /* the menu itself) must have focus, otherwise it unmaps the menu.   */
    /* This causes menus not to be shown under many several conditions.  */
    /* WINDOW_TYPE_DOCK is definitely the right choice for KWin.         */
    xitk_window_set_wm_window_type(xwin, WINDOW_TYPE_DOCK);
  btn = (xitk_widget_t *) xitk_window_widget_list(xwin)->list.head.next;
  if (btn) {
    xitk_set_focus_to_widget(btn);
  }
}

#ifdef DUMP_MENU
#ifdef	__GNUC__
#define prints(fmt, args...) do { int j; for(j = 0; j < i; j++) printf(" "); printf(fmt, ##args); } while(0)
#else
#define prints(...) do { int j; for(j = 0; j < i; j++) printf(" "); printf(__VA_ARGS__); } while(0)
#endif
#endif

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _menu_private_t *wp = (_menu_private_t *)w;

  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MENU)
    return 0;
  (void)result;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
    case WIDGET_EVENT_FOCUS:
    case WIDGET_EVENT_CHANGE_SKIN:
    case WIDGET_EVENT_GET_SKIN:
      return 0;
    case WIDGET_EVENT_CLICK:
    case WIDGET_EVENT_INSIDE:
      return 1;
    case WIDGET_EVENT_DESTROY:
      _menu_exit (wp);
      return 1;
  }
  return 0;
}

xitk_widget_t *xitk_menu_get_menu (xitk_widget_t *w) {
  if (!w)
    return NULL;
  if ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_MENU)
    return w;
  if ((w->type & (WIDGET_GROUP_MENU | WIDGET_TYPE_MASK)) == (WIDGET_GROUP_MENU | WIDGET_TYPE_LABELBUTTON)) {
    _menu_node_t *me = (_menu_node_t *)labelbutton_get_user_data (w);
    if (me)
      return &me->wp->w;
  }
  return NULL;
}

int xitk_menu_show_sub_branchs(xitk_widget_t *w) {
  _menu_private_t *wp = (_menu_private_t *)w;
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MENU)
    return 0;
  if (wp->num_open > 1)
    return 1;
  return 0;
}

void xitk_menu_destroy_sub_branchs (xitk_widget_t *w) {
  _menu_private_t *wp = (_menu_private_t *)w;
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MENU)
    return;
  if (wp->num_open < 2)
    return;
  _menu_close_subs_ex (wp, &wp->root);
}

void xitk_menu_destroy_branch (xitk_widget_t *w) {
  _menu_node_t *me;

  if (!w)
    return;
  if ((w->type & (WIDGET_GROUP_MENU | WIDGET_TYPE_MASK)) != (WIDGET_GROUP_MENU | WIDGET_TYPE_LABELBUTTON))
    return;

  me = labelbutton_get_user_data (w);
  if (!me->parent)
    return;
  if (_menu_show_subs (me->wp, me->parent)) {
    _menu_close_subs_in (me->wp, me->parent);
  }
}

void menu_auto_pop (xitk_widget_t *w) {
  _menu_node_t *me;

  if (!w)
    return;
  if ((w->type & (WIDGET_GROUP_MENU | WIDGET_TYPE_MASK)) != (WIDGET_GROUP_MENU | WIDGET_TYPE_LABELBUTTON))
    return;

  me = labelbutton_get_user_data (w);
  if (me->type & _MENU_NODE_BRANCH) {
    xitk_labelbutton_callback_exec (w);
  } else {
    _menu_close_subs_ex (me->wp, me->parent);
  }
}

void xitk_menu_show_menu (xitk_widget_t *w) {
  _menu_private_t *wp = (_menu_private_t *)w;

  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MENU)
    return;

  wp->w.visible = 1;
  _menu_open (&wp->root, wp->x, wp->y);
  xitk_set_current_menu (&wp->w);
}

xitk_widget_t *xitk_noskin_menu_create(xitk_widget_list_t *wl, 
				       xitk_menu_widget_t *m, int x, int y) {
  _menu_private_t *wp;
  
  ABORT_IF_NULL(wl);
  XITK_CHECK_CONSTITENCY(m);
  
  wp = (_menu_private_t *)xitk_xmalloc (sizeof (*wp));
  if (!wp)
    return NULL;

  wp->w.private_data = wp;
  wp->w.wl           = wl;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_MENU | WIDGET_FOCUSABLE;

  wp->x              = x;
  wp->y              = y;
  wp->num_open       = 0;

  wp->root.node.next   = NULL;
  wp->root.node.prev   = NULL;
  wp->root.parent      = NULL;
  wp->root.num         = 0;
  wp->root.type        = 0;
  xitk_dlist_init (&wp->root.branches);
  wp->root.menu_window = NULL;
  wp->root.wp          = wp;
  wp->root.button      = NULL;

  if (!m->menu_tree) {
    printf ("Empty menu entries. You will not .\n");
    abort ();
  }

  {
    xitk_menu_entry_t *me;
    for (me = m->menu_tree; me->menu; me++)
      xitk_menu_add_entry (&wp->w, me);
  }

  wp->w.enable        = 1;
  wp->w.running       = 1;
  wp->w.visible       = 0;
  wp->w.have_focus    = FOCUS_RECEIVED;
  wp->w.x             = 0;
  wp->w.y             = 0;
  wp->w.width         = 0;
  wp->w.height        = 0;
  wp->w.event         = notify_event;
  wp->w.tips_timeout  = 0;
  wp->w.tips_string   = NULL;

  return &wp->w;
}
