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

#include "xitkintl.h"
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

/* mrlbrowser calls "show" on the entire widget list.
 * we should not add our hidden ones there, and keep "..." separate.
 * big example: num = 7, max_buttons = 4, visible = 3, (hidden), [shown]
 * 0                 first                   num         last
 * (101) (102) (103) [104] [105] [106] (107) (   ) (   )        [...]
 * medium example: num = 4, max_buttons = 4, visible = 4
 * 0 = first               num = last
 * [101] [102] [103] [104]            (...)
 * small exammple: num = 2, max_buttons = 4, visible = 2
 * 0 = first   num = last
 * [101] [102]            (...) */

typedef struct {
  xitk_widget_t       w;
  xitk_skin_config_t *skin_config;
  xitk_widget_list_t *widget_list;
  char                skin_element_name[64];
  int                 flags, num, visible, first, last;
  int                 x, y, dx, dy;
  uint32_t            widget_type_flags;
  xitk_widget_t      *add_here, *swap, *widgets[64];
} xitk_button_list_t;

static void xitk_button_list_remove (xitk_button_list_t *bl) {
  int i = bl->first, e;
  if (!bl->widgets[i])
    return;
  bl->add_here = (xitk_widget_t *)bl->widgets[i]->node.prev;
  e = i + bl->visible;
  if (e > bl->last)
    e = bl->last;
  /* [101] and [   ] */
  while (i < e) {
    xitk_disable_and_hide_widget (bl->widgets[i]);
    xitk_dnode_remove (&bl->widgets[i]->node);
    i++;
  }
}

static void xitk_button_list_add (xitk_button_list_t *bl) {
  int i = bl->first, e, x, y;
  if (!bl->add_here)
    return;
  e = i + bl->visible;
  if (e > bl->num)
    e = bl->num;
  x = bl->x;
  y = bl->y;
  /* [101] */
  while (i < e) {
    xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
    bl->add_here = bl->widgets[i];
    xitk_set_widget_pos (bl->widgets[i], x, y);
    xitk_enable_and_show_widget (bl->widgets[i]);
    x += bl->dx;
    y += bl->dy;
    i++;
  }
  e = bl->first + bl->visible;
  if (e > bl->last)
    e = bl->last;
  /* [   ] */
  while (i < e) {
    xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
    bl->add_here = bl->widgets[i];
    xitk_set_widget_pos (bl->widgets[i], x, y);
    xitk_show_widget (bl->widgets[i]);
    xitk_disable_widget (bl->widgets[i]);
    x += bl->dx;
    y += bl->dy;
    i++;
  }
  /* [...] */
  if ((bl->num > bl->visible) && bl->swap && !bl->swap->node.next) {
    xitk_dnode_insert_after (&bl->add_here->node, &bl->swap->node);
    xitk_set_widget_pos (bl->swap, x, y);
    xitk_enable_and_show_widget (bl->swap);
  }
}

static void xitk_button_list_swap (xitk_widget_t *w, void *ip, int state) {
  xitk_button_list_t *bl = (xitk_button_list_t *)ip;

  (void)w;
  (void)state;

  xitk_button_list_remove (bl);

  bl->first += bl->visible;
  if (bl->first >= bl->num)
    bl->first = 0;

  xitk_button_list_add (bl);
}

static void xitk_button_list_delete (xitk_button_list_t *bl) {
  int i;

  for (i = 0; i < bl->first; i++) {
    xitk_destroy_widget (bl->widgets[i]);
    bl->widgets[i] = NULL;
  }
  for (i = bl->first + bl->visible; i < bl->last; i++) {
    xitk_destroy_widget (bl->widgets[i]);
    bl->widgets[i] = NULL;
  }
  if (bl->swap && (bl->num <= bl->visible))
    xitk_destroy_widget (bl->swap);
  bl->swap = NULL;
}

static void xitk_button_list_new_skin (xitk_button_list_t *bl, xitk_skin_config_t *skin_config) {
  const xitk_skin_element_info_t *info;
  int dir, i, max;

  xitk_button_list_remove (bl);
  /* relay new skin to hidden ones */
  {
    widget_event_t event;
    event.type    = WIDGET_EVENT_CHANGE_SKIN;
    event.skonfig = skin_config;
    for (i = 0; i < bl->first; i++) {
      xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
      bl->widgets[i]->event (bl->widgets[i], &event, NULL);
      xitk_disable_and_hide_widget (bl->widgets[i]);
      xitk_dnode_remove (&bl->widgets[i]->node);
    }
    for (i = bl->first + bl->visible; i < bl->last; i++) {
      xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
      bl->widgets[i]->event (bl->widgets[i], &event, NULL);
      xitk_disable_and_hide_widget (bl->widgets[i]);
      xitk_dnode_remove (&bl->widgets[i]->node);
    }
    if (bl->swap) {
      if (!bl->swap->node.next) {
        xitk_dnode_insert_after (&bl->add_here->node, &bl->swap->node);
        bl->swap->event (bl->swap, &event, NULL);
      }
      xitk_disable_and_hide_widget (bl->swap);
      xitk_dnode_remove (&bl->swap->node);
    }
  }
  
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

  max = (bl->num + bl->visible - 1) / bl->visible * bl->visible;
  if (max > (int)(sizeof (bl->widgets) / sizeof (bl->widgets[0])))
    max = sizeof (bl->widgets) / sizeof (bl->widgets[0]);
  if (max > bl->last) {
    /* more "   " */
    xitk_labelbutton_widget_t lb;
    XITK_WIDGET_INIT (&lb);
    lb.skin_element_name = bl->skin_element_name;
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_DEFAULT;
    lb.callback          = NULL;
    lb.userdata          = NULL;
    lb.state_callback    = NULL;
    lb.label             = "";
    for (i = bl->last; i < max; i++) {
      bl->widgets[i] = xitk_labelbutton_create (bl->widget_list, skin_config, &lb);
      if (!bl->widgets[i])
        break;
      bl->widgets[i]->type |= bl->widget_type_flags;
      bl->widgets[i]->parent = &bl->w;
      xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
      xitk_disable_and_hide_widget (bl->widgets[i]);
      xitk_dnode_remove (&bl->widgets[i]->node);
    }
    bl->last = i;
  } else if (max < bl->last) {
    /* less "   " */
    for (i = max; i < bl->last; i++) {
      xitk_destroy_widget (bl->widgets[i]);
      bl->widgets[i] = NULL;
    }
    bl->last = max;
  }

  bl->w.x = bl->x = info ? info->x : 0;
  bl->w.y = bl->y = info ? info->y : 0;
  dir = info ? info->direction : DIRECTION_LEFT;
  switch (dir) {
    case DIRECTION_UP:
      bl->dx = 0;
      bl->dy = - xitk_get_widget_height (bl->widgets[0]) - 1;
      break;
    case DIRECTION_DOWN:
      bl->dx = 0;
      bl->dy = xitk_get_widget_height (bl->widgets[0]) + 1;
      break;
    case DIRECTION_LEFT:
    default:
      bl->dx = -xitk_get_widget_width (bl->widgets[0]) - 1;
      bl->dy = 0;
      break;
    case DIRECTION_RIGHT:
      bl->dx = xitk_get_widget_width (bl->widgets[0]) + 1;
      bl->dy = 0;
      break;
  }

  bl->first = 0;
  xitk_button_list_add (bl);

  bl->flags |= 1;
}

static int xitk_button_list_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  xitk_button_list_t *bl = (xitk_button_list_t *)w;

  (void)result;
  if (event && bl && ((bl->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON_LIST)) {
    switch (event->type) {
      case WIDGET_EVENT_CHANGE_SKIN:
        xitk_button_list_new_skin (bl, event->skonfig);
        return 0;
      case WIDGET_EVENT_DESTROY:
        xitk_button_list_delete (bl);
        return 0;
      default: ;
    }
  }
  return 0;
}

xitk_widget_t *xitk_button_list_new (
  xitk_widget_list_t *widget_list,
  xitk_skin_config_t *skin_config, const char *skin_element_name,
  xitk_state_callback_t callback, void *callback_data,
  const char * const *names,
  const char * const *tips, int tips_timeout, uint32_t widget_type_flags) {

  xitk_labelbutton_widget_t lb;
  xitk_button_list_t *bl;
  const xitk_skin_element_info_t *info;
  int dir, i, max;

  for (i = 0; names[i]; i++) ;
  if (i == 0)
    return NULL;
  if (i > (int)sizeof (bl->widgets) / (int)sizeof (bl->widgets[0]))
    i = sizeof (bl->widgets) / sizeof (bl->widgets[0]);

  bl = calloc (1, sizeof (*bl));
  if (!bl)
    return NULL;
  bl->skin_config = skin_config;
  _strlcpy (bl->skin_element_name, skin_element_name, sizeof (bl->skin_element_name));
  bl->flags = 1;

  bl->widget_type_flags = widget_type_flags | WIDGET_GROUP_BUTTON_LIST;
  bl->w.wl = bl->widget_list = widget_list;
  bl->w.private_data = bl;

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

  bl->last = (bl->num + bl->visible - 1) / bl->visible * bl->visible;
  if (bl->last > (int)(sizeof (bl->widgets) / sizeof (bl->widgets[0])))
    bl->last = sizeof (bl->widgets) / sizeof (bl->widgets[0]);

  bl->first = 0;
  bl->w.parent = NULL;
  bl->w.focus_redirect = NULL;
  bl->w.x = bl->x = info ? info->x : 0;
  bl->w.y = bl->y = info ? info->y : 0;
  bl->w.width = 0;
  bl->w.height = 0;
  bl->w.type = WIDGET_GROUP | WIDGET_TYPE_BUTTON_LIST;
  bl->w.enable = info ? info->enability : 1;
  bl->w.running = 1;
  bl->w.visible = info ? info->visibility : 1;
  bl->w.have_focus = FOCUS_LOST;
  bl->w.event = xitk_button_list_event;
  bl->w.tips_timeout = 0;
  bl->w.tips_string = NULL;

  bl->add_here = (xitk_widget_t *)widget_list->list.tail.prev;
  
  XITK_WIDGET_INIT (&lb);
  lb.skin_element_name = bl->skin_element_name;
  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_DEFAULT;
  lb.callback          = callback;
  lb.userdata          = callback_data;
  lb.state_callback    = NULL;

  /* "101" */
  lb.label = names[0];
  bl->widgets[0] = xitk_labelbutton_create (widget_list, skin_config, &lb);
  if (!bl->widgets[0]) {
    free (bl);
    return NULL;
  }
  bl->widgets[0]->type |= bl->widget_type_flags;
  bl->widgets[0]->parent = &bl->w;
  xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[0]->node);
  if (tips[0])
    xitk_set_widget_tips_and_timeout (bl->widgets[0], tips[0], tips_timeout);
  xitk_disable_and_hide_widget (bl->widgets[0]);
  xitk_dnode_remove (&bl->widgets[0]->node);

  dir = info ? info->direction : DIRECTION_LEFT;
  switch (dir) {
    case DIRECTION_UP:
      bl->dx = 0;
      bl->dy = - xitk_get_widget_height (bl->widgets[0]) - 1;
      break;
    case DIRECTION_DOWN:
      bl->dx = 0;
      bl->dy = xitk_get_widget_height (bl->widgets[0]) + 1;
      break;
    case DIRECTION_LEFT:
    default:
      bl->dx = -xitk_get_widget_width (bl->widgets[0]) - 1;
      bl->dy = 0;
      break;
    case DIRECTION_RIGHT:
      bl->dx = xitk_get_widget_width (bl->widgets[0]) + 1;
      bl->dy = 0;
      break;
  }

  /* "102" */
  for (i = 1; i < bl->num; i++) {
    lb.label       = names[i];
    bl->widgets[i] = xitk_labelbutton_create (widget_list, skin_config, &lb);
    if (!bl->widgets[i])
      break;
    bl->widgets[i]->type |= bl->widget_type_flags;
    bl->widgets[i]->parent = &bl->w;
    xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
    if (tips[i])
      xitk_set_widget_tips_and_timeout (bl->widgets[i], tips[i], tips_timeout);
    xitk_disable_and_hide_widget (bl->widgets[i]);
    xitk_dnode_remove (&bl->widgets[i]->node);
  }
  bl->num = i;

  /* "   " */
  lb.label = "";
  lb.callback = NULL;
  lb.userdata = NULL;
  for (; i < bl->last; i++) {
    bl->widgets[i] = xitk_labelbutton_create (widget_list, skin_config, &lb);
    if (!bl->widgets[i])
      break;
    bl->widgets[i]->type |= bl->widget_type_flags;
    bl->widgets[i]->parent = &bl->w;
    xitk_dnode_insert_after (&bl->add_here->node, &bl->widgets[i]->node);
    xitk_disable_and_hide_widget (bl->widgets[i]);
    xitk_dnode_remove (&bl->widgets[i]->node);
  }
  bl->last = i;

  /* "..." */
  lb.callback = xitk_button_list_swap;
  lb.label    = _("...");
  lb.userdata = bl;
  bl->swap    = xitk_labelbutton_create (widget_list, skin_config, &lb);
  if (bl->swap) {
    bl->swap->type |= bl->widget_type_flags;
    bl->swap->parent = &bl->w;
    xitk_dnode_insert_after (&bl->add_here->node, &bl->swap->node);
    xitk_set_widget_tips_and_timeout (bl->swap, _("More sources..."), tips_timeout);
    xitk_disable_and_hide_widget (bl->swap);
    xitk_dnode_remove (&bl->swap->node);
  }

  bl->first = 0;
  xitk_button_list_add (bl);

  return &bl->w;
}

xitk_widget_t *xitk_button_list_find (xitk_widget_t *w, const char *name) {
  xitk_button_list_t *bl = (xitk_button_list_t *)w;
  int i;
  if (!bl || !name)
    return NULL;
  if ((bl->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BUTTON_LIST)
    return NULL;
  for (i = 0; i < bl->num; i++) {
    const char *p = xitk_labelbutton_get_label (bl->widgets[i]);
    if (p && !strcasecmp (p, name))
      return bl->widgets[i];
  }
  return NULL;
}

void xitk_button_list_able (xitk_widget_t *w, int enable) {
  xitk_button_list_t *bl = (xitk_button_list_t *)w;
  if (!bl)
    return;
  if ((bl->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_BUTTON_LIST)
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
      xitk_enable_widget (bl->swap);
    bl->flags |= 1;
  } else {
    int a, b;
    if (!(bl->flags & 1))
      return;
    a = bl->first;
    b = a + bl->visible;
    if (b > bl->last)
      b = bl->last;
    for (; a < b; a++)
      xitk_disable_widget (bl->widgets[a]);
    if (bl->num > bl->visible)
      xitk_disable_widget (bl->swap);
    bl->flags &= ~1;
  }
}
