/*
 * Copyright (C) 2019-2020 the xine project
 * 
 * This file is part of xine, a free video player.
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
 * A list of label buttons, optionally with a ... to make all choices available.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "button_list.h"
#include "widget.h"
#include "labelbutton.h"

static size_t _strlcpy (char *t, const char *s, size_t l) {
  size_t n = strlen (s);
  if (l) {
    size_t p = n + 1 > l ? l : n + 1;
    memcpy (t, s, p);
    t[p - 1] = 0;
  }
  return n;
}

struct xitk_button_list_st {
  xitk_skin_config_t *skin_config;
  char                skin_element_name[64];
  int                 flags, num, visible, first;
  int                 fillx, filly, dx, dy;
  xitk_widget_t      *widgets[64];
};

static void xitk_button_list_swap (xitk_widget_t *w, void *ip) {
  xitk_button_list_t *bl = (xitk_button_list_t *)ip;
  int i, e;

  (void)w;

  e = bl->first + bl->visible;
  if (e > bl->num)
    e = bl->num;
  for (i = bl->first; i < e; i++)
    xitk_disable_and_hide_widget (bl->widgets[i]);

  bl->first += bl->visible;
  if (bl->first >= bl->num)
    bl->first = 0;

  e = bl->first + bl->visible;
  if (e > bl->num)
    e = bl->num;
  for (i = bl->first; i < e; i++) {
    xitk_enable_widget (bl->widgets[i]);
    xitk_show_widget (bl->widgets[i]);
  }
  e = bl->first + bl->visible;
  if (i < e) {
    int x = bl->fillx;
    int y = bl->filly;
    xitk_widget_t *dummy = bl->widgets[bl->num];
    for (; i < e; i++) {
      (void)xitk_set_widget_pos (dummy, x, y);
      xitk_show_widget (dummy);
      xitk_hide_widget (dummy);
      x += bl->dx;
      y += bl->dy;
    }
  }
}

xitk_button_list_t *xitk_button_list_new (
  xitk_widget_list_t *widget_list,
  xitk_skin_config_t *skin_config, const char *skin_element_name,
  xitk_simple_callback_t callback, void *callback_data,
  char **names,
  char **tips, int tips_timeout, uint32_t widget_type_flags) {

  xitk_labelbutton_widget_t lb;
  xitk_button_list_t *bl;
  const xitk_skin_element_info_t *info;
  int x = 0, y = 0, lastx = 0, lasty = 0, dir, i, max, vis;

  for (i = 0; names[i]; i++) ;
  if (i == 0)
    return NULL;
  if (i > (int)sizeof (bl->widgets) / (int)sizeof (bl->widgets[0]) - 3)
    i = sizeof (bl->widgets) / sizeof (bl->widgets[0]) - 3;

  bl = malloc (sizeof (*bl));
  if (!bl)
    return NULL;
  bl->skin_config = skin_config;
  _strlcpy (bl->skin_element_name, skin_element_name, sizeof (bl->skin_element_name));
  bl->flags = 1;

  info = xitk_skin_get_info (skin_config, skin_element_name);
  max = info ? info->max_buttons : 0;
  if (max <= 0)
    max = 10000;
  if (max == 1) {
    i = 1;
  } else if (i > max) {
    max -= 1;
  } else {
    max = i;
  }
  bl->num = i;
  bl->visible = max;
  bl->first = 0;
  bl->dx = 99999999;
  vis = 0;
  
  XITK_WIDGET_INIT (&lb);
  lb.skin_element_name = bl->skin_element_name;
  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_DEFAULT;
  lb.callback          = callback;
  lb.userdata          = callback_data;
  lb.state_callback    = NULL;

  dir = info ? info->direction : DIRECTION_LEFT;

  for (i = 0; i < bl->num; i++) {

    lb.label             = names[i];

    if (vis == 0) {
      lastx = x;
      lasty = y;
      x = info ? info->x : 0;
      y = info ? info->y : 0;
      vis = bl->visible - 1;
    } else {
      vis--;
    }

    bl->widgets[i] = xitk_labelbutton_create (widget_list, skin_config, &lb);
    if (!bl->widgets[i])
      break;

    bl->widgets[i]->type |= widget_type_flags;
    xitk_dlist_add_tail (&widget_list->list, &bl->widgets[i]->node);
    if (tips[i])
      xitk_set_widget_tips_and_timeout (bl->widgets[i], tips[i], tips_timeout);
      
/*  if (!tips_enable)
      xitk_disable_widget_tips (bl->widgets[i]); */

    (void)xitk_set_widget_pos (bl->widgets[i], x, y);

    if (bl->dx == 99999999) {
      switch (dir) {
        case DIRECTION_UP:
          bl->dx = 0;
          bl->dy = - xitk_get_widget_height (bl->widgets[i]) - 1;
          break;
        case DIRECTION_DOWN:
          bl->dx = 0;
          bl->dy = xitk_get_widget_height (bl->widgets[i]) + 1;
          break;
        case DIRECTION_LEFT:
          bl->dx = -xitk_get_widget_width (bl->widgets[i]) - 1;
          bl->dy = 0;
          break;
        case DIRECTION_RIGHT:
          bl->dx = xitk_get_widget_width (bl->widgets[i]) + 1;
          bl->dy = 0;
          break;
      }
    }
    x += bl->dx;
    y += bl->dy;

    if (i >= bl->visible)
      xitk_disable_and_hide_widget (bl->widgets[i]);
  }

  bl->fillx = x;
  bl->filly = y;
  lb.callback          = NULL;
  lb.label             = (char *)"";
  lb.userdata          = NULL;
  bl->widgets[i] = xitk_labelbutton_create (widget_list, skin_config, &lb);
  if (bl->widgets[i]) {
    bl->widgets[i]->type |= widget_type_flags;
    xitk_dlist_add_tail (&widget_list->list, &bl->widgets[i]->node);
    (void)xitk_set_widget_pos (bl->widgets[i], x, y);
    xitk_disable_and_hide_widget (bl->widgets[i]);
    i++;
  }

  lb.callback          = xitk_button_list_swap;
  lb.label             = (char *)_("...");
  lb.userdata          = (void *)bl;
  bl->widgets[i] = xitk_labelbutton_create (widget_list, skin_config, &lb);
  if (bl->widgets[i]) {
    bl->widgets[i]->type |= widget_type_flags;
    xitk_dlist_add_tail (&widget_list->list, &bl->widgets[i]->node);
    xitk_set_widget_tips_and_timeout (bl->widgets[i], _("More sources..."), tips_timeout);
/*  if (!tips.enable)
      xitk_disable_widget_tips (bl->widgets[i]); */
    (void)xitk_set_widget_pos (bl->widgets[i], lastx, lasty);
    if (bl->num <= bl->visible)
      xitk_disable_and_hide_widget (bl->widgets[i]);
    i++;
  }

  bl->widgets[i] = NULL;

  return bl;
}

void xitk_button_list_new_skin (xitk_button_list_t *bl, xitk_skin_config_t *skin_config) {
  const xitk_skin_element_info_t *info;
  int x = 0, y = 0, lastx = 0, lasty = 0, dir, i, max, vis;

  if (!bl)
    return;

  bl->skin_config = skin_config;
  info = xitk_skin_get_info (bl->skin_config, bl->skin_element_name);
  max = info ? info->max_buttons : 0;
  if (max <= 0)
    max = 10000;
  if (bl->num > max) {
    max -= 1;
  } else {
    max = bl->num;
  }
  bl->visible = max;
  bl->first = 0;
  bl->dx = 99999999;
  vis = 0;
  
  dir = info ? info->direction : DIRECTION_LEFT;

  for (i = 0; i < bl->num; i++) {

    if (vis == 0) {
      lastx = x;
      lasty = y;
      x = info ? info->x : 0;
      y = info ? info->y : 0;
      vis = bl->visible - 1;
    } else {
      vis--;
    }

    (void)xitk_set_widget_pos (bl->widgets[i], x, y);
  
    if (bl->dx == 99999999) {
      switch (dir) {
        case DIRECTION_UP:
          bl->dx = 0;
          bl->dy = - xitk_get_widget_height (bl->widgets[i]) - 1;
          break;
        case DIRECTION_DOWN:
          bl->dx = 0;
          bl->dy = xitk_get_widget_height (bl->widgets[i]) + 1;
          break;
        case DIRECTION_LEFT:
          bl->dx = -xitk_get_widget_width (bl->widgets[i]) - 1;
          bl->dy = 0;
          break;
        case DIRECTION_RIGHT:
          bl->dx = xitk_get_widget_width (bl->widgets[i]) + 1;
          bl->dy = 0;
          break;
      }
    }
    x += bl->dx;
    y += bl->dy;

    if (i < bl->visible) {
      xitk_enable_widget (bl->widgets[i]);
      xitk_show_widget (bl->widgets[i]);
    } else {
      xitk_disable_and_hide_widget (bl->widgets[i]);
    }
  }

  bl->fillx = x;
  bl->filly = y;
  xitk_set_widget_pos (bl->widgets[i], x, y);
  xitk_disable_and_hide_widget (bl->widgets[i]);
  i++;

  if (bl->num > bl->visible) {
    xitk_set_widget_pos (bl->widgets[i], lastx, lasty);
    xitk_enable_widget (bl->widgets[i]);
    xitk_show_widget (bl->widgets[i]);
  } else {
    xitk_set_widget_pos (bl->widgets[i], 0, 0);
    xitk_disable_and_hide_widget (bl->widgets[i]);
  }

  bl->flags |= 1;
}

xitk_widget_t *xitk_button_list_find (xitk_button_list_t *bl, const char *name) {
  xitk_widget_t **w;
  if (!bl)
    return NULL;
  w = bl->widgets;
  while (*w) {
    const char *p = xitk_labelbutton_get_label (w[0]);
    if (p && !strcasecmp (p, name))
      return *w;
    w++;
  }
  return NULL;
}

void xitk_button_list_able (xitk_button_list_t *bl, int enable) {
  if (!bl)
    return;
  if (enable) {
    int a, b;
    if (bl->flags & 1)
      return;
    a = bl->first;
    b = a + bl->visible;
    if (b > bl->num)
      b = bl->num;
    for (; a < b; a++)
      xitk_enable_widget (bl->widgets[a]);
    if (bl->num > bl->visible)
      xitk_enable_widget (bl->widgets[bl->num + 1]);
    bl->flags |= 1;
  } else {
    int a, b;
    if (!(bl->flags & 1))
      return;
    a = bl->first;
    b = a + bl->visible;
    if (b > bl->num)
      b = bl->num;
    for (; a < b; a++)
      xitk_disable_widget (bl->widgets[a]);
    if (bl->num > bl->visible)
      xitk_disable_widget (bl->widgets[bl->num + 1]);
    bl->flags &= ~1;
  }
}

void xitk_button_list_delete (xitk_button_list_t *bl) {
  free (bl);
}

