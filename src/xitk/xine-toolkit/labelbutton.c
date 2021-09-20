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
#include <stdlib.h>
#include <string.h>

#include "_xitk.h"
#include "labelbutton.h"
#include "font.h"

typedef struct {
  xitk_widget_t           w;

  int                     num_gfx;
  int                     bType;

  xitk_part_image_t       skin;
  xitk_part_image_t       temp_image;

  xitk_state_callback_t   callback;
  xitk_ext_state_callback_t state_callback;

  void                   *userdata;

  int                     align;
  int                     label_dy;
  int                     label_offset;
  int                     label_visible;
  int                     label_static;
  int                     shortcut_pos;

  char                    skin_element_name[64];
  xitk_short_string_t     label, shortcut_label, font, shortcut_font;
  uint32_t                color[XITK_IMG_STATE_SELECTED + 1];
} _lbutton_private_t;

size_t xitk_short_string_set (xitk_short_string_t *s, const char *v) {
  size_t n;

  if (!v)
    v = "";
  if (!strcmp (s->s, v))
    return (size_t)-1;

  if (s->s != s->buf) {
    free (s->s);
    s->s = s->buf;
  }
  /* TJ. trying to avoid a Coverity Scan false positive with v == "PlInputText":
   * >>> Overrunning buffer pointed to by "inp.skin_element_name" of 12 bytes
   * by passing it to a function which accesses it at byte offset 62. */
  if ((n = strlen (v) + 1) > sizeof (s->buf)) {
    char *d = malloc (n);
    if (d) {
      s->s = d;
    } else {
      n = sizeof (s->buf) - 1;
      s->buf[sizeof (s->buf) - 1] = 0;
    }
  }
  memcpy (s->s, v, n);
  return n - 1;
}

/*
 *
 */
static void _labelbutton_destroy (_lbutton_private_t *wp) {
  xitk_image_free_image (&wp->temp_image.image);
  if (!wp->skin_element_name[0])
    xitk_image_free_image (&wp->skin.image);

  xitk_short_string_deinit (&wp->label);
  xitk_short_string_deinit (&wp->shortcut_label);
  xitk_short_string_deinit (&wp->font);
  xitk_short_string_deinit (&wp->shortcut_font);
}

/*
 *
 */
static xitk_image_t *_labelbutton_get_skin (_lbutton_private_t *wp, int sk) {
  if ((sk == FOREGROUND_SKIN) && wp->skin.image)
    return wp->skin.image;
  return NULL;
}

/*
 *
 */
static int _labelbutton_inside (_lbutton_private_t *wp, int x, int y) {
  if (wp->w.state & XITK_WIDGET_STATE_VISIBLE) {
    xitk_image_t *skin = wp->skin.image;

    return xitk_image_inside (skin, x + wp->skin.x - wp->w.x, y + wp->skin.y - wp->w.y);
  }
  return 0;
}

/*
 * Draw the string in pixmap pix, then return it
 */
static void _create_labelofbutton (_lbutton_private_t *wp,
    xitk_image_t *pix, int xsize, int ysize,
    int shortcut_pos, xitk_img_state_t state) {
  xitk_font_t             *fs, *short_font;
  size_t                   slen;
  int                      lbear, rbear, width, asc, des, xoff, yoff, roff, mode;
  unsigned int             fg = 0;
  unsigned int             origin = 0;

  /* shortcut is only permited if alignement is set to left */
  mode = (wp->label.s[0] ? 2 : 0)
       + ((wp->align == ALIGN_LEFT) && wp->shortcut_label.s[0] && (shortcut_pos >= 0) ? 1 : 0);
  if (!mode)
    return;

  /* Try to load font */
  fs = xitk_font_load_font (wp->w.wl->xitk, wp->font.s[0] ? wp->font.s : xitk_get_cfg_string (wp->w.wl->xitk, XITK_SYSTEM_FONT));
  if (!fs)
    XITK_DIE ("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);

  slen = strlen (wp->label.s);
  xitk_font_text_extent (fs, wp->label.s[0] ? wp->label.s : "Button", wp->label.s[0] ? slen : 6,
    &lbear, &rbear, &width, &asc, &des);

  /*  Some colors configurations */
  fg = wp->color[state > XITK_IMG_STATE_SELECTED ? XITK_IMG_STATE_SELECTED : state];
  if (fg & 0x80000000) {
    fg = xitk_get_cfg_num (wp->w.wl->xitk, (fg == XITK_NOSKIN_TEXT_INV)
        ? ((wp->w.state & XITK_WIDGET_STATE_ENABLE) ? XITK_WHITE_COLOR : XITK_DISABLED_WHITE_COLOR)
        : ((wp->w.state & XITK_WIDGET_STATE_ENABLE) ? XITK_BLACK_COLOR : XITK_DISABLED_BLACK_COLOR));
  } else {
    if (!(wp->w.state & XITK_WIDGET_STATE_ENABLE))
      fg = xitk_disabled_color (fg);
    fg = xitk_color_db_get (wp->w.wl->xitk, fg);
  }

  roff = (state == XITK_IMG_STATE_SELECTED) || (state == XITK_IMG_STATE_SEL_FOCUS) ? 5 : 1;
  if ((roff == 5) && (wp->label_static == 0)) {
    xoff = -4;
    yoff = 1;
  } else {
    xoff = 0;
    yoff = 0;
  }

  if (wp->bType == TAB_BUTTON) {
    /* The tab lift effect. */
    origin = ((ysize - 3 + asc + des + yoff) >> 1) - des + 3;
    if ((state == XITK_IMG_STATE_FOCUS) || (state == XITK_IMG_STATE_SELECTED) || (state == XITK_IMG_STATE_SEL_FOCUS))
      origin -= 3;
  } else {
    origin = ((ysize + asc + des + yoff) >> 1) - des + wp->label_dy;
  }

  /*  Put text in the right place */
  if (mode & 2) {
    int xx = wp->align == ALIGN_CENTER ? ((xsize - width - xoff) >> 1)
           : wp->align == ALIGN_RIGHT  ? (xsize - width - roff)
           : /* wp->align == ALIGN_LEFT */ roff;
    xitk_image_draw_string (pix, fs, xx + wp->label_offset, origin, wp->label.s, slen, fg);
  }

  short_font = fs;
  if (mode & 1) {
    slen = strlen (wp->shortcut_label.s);
    if (wp->shortcut_font.s[0])
      short_font = xitk_font_load_font (wp->w.wl->xitk, wp->shortcut_font.s);
    if (!short_font)
      short_font = fs;
    if (shortcut_pos == 0) {
      xitk_font_text_extent (short_font, wp->shortcut_label.s, slen, &lbear, &rbear, &width, &asc, &des);
      shortcut_pos = xsize - 5 - width;
    }
    xitk_image_draw_string (pix, short_font, roff + shortcut_pos, origin, wp->shortcut_label.s, slen, fg);
  }

  if (short_font != fs)
    xitk_font_unload_font (short_font);
  xitk_font_unload_font (fs);
}

/*
 * Paint the button with correct background pixmap
 */
static void _labelbutton_partial_paint (_lbutton_private_t *wp, const widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.labelbutton.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if ((wp->bType != RADIO_BUTTON) && (wp->bType != TAB_BUTTON))
    wp->w.state &= ~XITK_WIDGET_STATE_ON;
  if ((wp->w.state & XITK_WIDGET_STATE_VISIBLE) && wp->skin.width) {
    xitk_img_state_t state = xitk_image_find_state (XITK_IMG_STATE_SEL_FOCUS, wp->w.state);

    xitk_part_image_copy (wp->w.wl, &wp->skin, &wp->temp_image,
      ((int)state >= wp->num_gfx ? wp->num_gfx - 1 : (int)state) * wp->skin.width / wp->num_gfx,
      0, wp->skin.width / wp->num_gfx, wp->skin.height, 0, 0);
    if (wp->label_visible) {
      _create_labelofbutton (wp, wp->temp_image.image,
        wp->skin.width / wp->num_gfx, wp->skin.height, wp->shortcut_pos, state);
    }

    xitk_part_image_draw (wp->w.wl, &wp->skin, &wp->temp_image,
      event->x - wp->w.x, event->y - wp->w.y,
      event->width, event->height,
      event->x, event->y);
  }
}

static void _labelbutton_paint (_lbutton_private_t *wp) {
  widget_event_t event;

  event.x = wp->w.x;
  event.y = wp->w.y;
  event.width = wp->w.width;
  event.height = wp->w.height;
  _labelbutton_partial_paint (wp, &event);
  wp->w.shown_state = wp->w.state;
}

/*
 * Handle click events
 */
static int _labelbutton_input (_lbutton_private_t *wp, const widget_event_t *event) {
  int fire;

  if (!(wp->w.state & XITK_WIDGET_STATE_FOCUS))
    return 0;

  if (event->type == WIDGET_EVENT_KEY) {
    static const char k[] = {
      XITK_CTRL_KEY_PREFIX, XITK_KEY_RIGHT,
      XITK_CTRL_KEY_PREFIX, XITK_KEY_RETURN,
      XITK_CTRL_KEY_PREFIX, XITK_KEY_NUMPAD_ENTER,
      XITK_CTRL_KEY_PREFIX, XITK_KEY_ISO_ENTER,
      ' ', 0
    };
    int i, n = sizeof (k) / sizeof (k[0]);

    if (event->modifier & ~(MODIFIER_SHIFT | MODIFIER_NUML))
      return 0;
    if (!event->string)
      return 0;

    for (i = (wp->w.type & WIDGET_GROUP_MENU) ? 0 : 2; i < n; i += 2) {
      if (!memcmp (event->string, k + i, 2))
      break;
    }
    if (i >= n)
      return 0;
  } else { /* WIDGET_EVENT_CLICK */
    if (event->button != 1)
      return 0;
  }

  fire = (wp->w.state ^ (event->pressed ? 0 : ~0u)) & XITK_WIDGET_STATE_IMMEDIATE;
  wp->w.state &= ~XITK_WIDGET_STATE_CLICK;
  if (event->pressed)
    wp->w.state |= XITK_WIDGET_STATE_CLICK;
  if (fire && (wp->w.state & XITK_WIDGET_STATE_TOGGLE))
    wp->w.state ^= XITK_WIDGET_STATE_ON;

  _labelbutton_paint (wp);

  if (fire) {
    if (wp->state_callback)
      wp->state_callback (&wp->w, wp->userdata, !!(wp->w.state & XITK_WIDGET_STATE_ON), event->modifier);
    else if (wp->callback)
      wp->callback (&wp->w, wp->userdata, !!(wp->w.state & XITK_WIDGET_STATE_ON));
  }
  return 1;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_label (xitk_widget_t *w, const char *newlabel) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    xitk_short_string_set (&wp->label, newlabel);
    _labelbutton_paint (wp);
    return 1;
  }
  return 0;
}

/*
 * Return the current button label
 */
const char *xitk_labelbutton_get_label (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->label.s;
  return NULL;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_shortcut_label (xitk_widget_t *w, const char *newlabel, int pos, const char *newfont) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp
    && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)
    && (wp->w.type & (WIDGET_GROUP_MENU | WIDGET_GROUP_BROWSER))) {
    xitk_short_string_set (&wp->shortcut_label, newlabel);
    if (newfont)
      xitk_short_string_set (&wp->shortcut_font, !strcmp (newfont, wp->font.s) ? "" : newfont);
    if (wp->shortcut_label.s[0]) {
      if (pos >= 0)
        wp->shortcut_pos = pos;
      else
        wp->shortcut_pos = -1;
    }
    _labelbutton_paint (wp);
    return 1;
  }
  return 0;
}

/*
 * Return the current button label
 */
#if 0
const char *xitk_labelbutton_get_shortcut_label (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp
    && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)
    && (wp->w.type & (WIDGET_GROUP_MENU | WIDGET_GROUP_BROWSER)))
    return wp->shortcut_label.s;
  return NULL;
}
#endif

/*
 *
 */
static void _labelbutton_new_skin (_lbutton_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name[0]) {
    const xitk_skin_element_info_t *info;

    xitk_skin_lock (skonfig);
    info = xitk_skin_get_info (skonfig, wp->skin_element_name);
    if (info) {
      wp->skin = info->pixmap_img;
      wp->color[XITK_IMG_STATE_NORMAL] = info->label_color;
      wp->color[XITK_IMG_STATE_FOCUS] = info->label_color_focus;
      wp->color[XITK_IMG_STATE_SELECTED] = info->label_color_click;
      xitk_short_string_set (&wp->font, info->label_fontname);
      wp->label_visible = info->label_printable;
      wp->label_static  = info->label_staticity;
      wp->align         = info->label_alignment;
      wp->w.x      = info->x;
      wp->w.y      = info->y;
      wp->w.width  = wp->skin.width / 3;
      wp->w.height = wp->skin.height;
      wp->label_dy = (info->label_y > 0) && (info->label_y < wp->skin.height)
                   ? info->label_y - (wp->skin.height >> 1) : 0;
      xitk_widget_state_from_info (&wp->w, info);
      xitk_image_free_image (&wp->temp_image.image);
      xitk_shared_image (wp->w.wl, "xitk_lbutton_temp", wp->skin.width / 3, wp->skin.height, &wp->temp_image.image);
      wp->temp_image.x = 0;
      wp->temp_image.y = 0;
      wp->temp_image.width = wp->skin.width / 3;
      wp->temp_image.height = wp->skin.height;
    } else {
      wp->skin.x        = 0;
      wp->skin.y        = 0;
      wp->skin.width    = 0;
      wp->skin.height   = 0;
      wp->skin.image    = NULL;
      wp->color[XITK_IMG_STATE_NORMAL]   = 0;
      wp->color[XITK_IMG_STATE_FOCUS]    = 0;
      wp->color[XITK_IMG_STATE_SELECTED] = 0;
      xitk_short_string_set (&wp->font, "");
      wp->label_visible = 0;
      wp->label_static  = 0;
      wp->align         = ALIGN_LEFT;
      wp->w.x       = 0;
      wp->w.y       = 0;
      wp->w.width   = 0;
      wp->w.height  = 0;
      wp->label_dy  = 0;
      wp->w.state  &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
      xitk_image_free_image (&wp->temp_image.image);
      wp->temp_image.x      = 0;
      wp->temp_image.y      = 0;
      wp->temp_image.width  = 0;
      wp->temp_image.height = 0;
    }
    xitk_skin_unlock (skonfig);
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}


static int labelbutton_event (xitk_widget_t *w, const widget_event_t *event) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (!event || !wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_LABELBUTTON)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      _labelbutton_partial_paint (wp, event);
      break;
    case WIDGET_EVENT_CLICK:
    case WIDGET_EVENT_KEY:
      return _labelbutton_input (wp, event);
    case WIDGET_EVENT_INSIDE:
      return _labelbutton_inside (wp, event->x, event->y) ? 1 : 2;
    case WIDGET_EVENT_CHANGE_SKIN:
      _labelbutton_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _labelbutton_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (event->image) {
        *event->image = _labelbutton_get_skin (wp, event->skin_layer);
        return 1;
      }
      break;
    default: ;
  }
  return 0;
}

/*
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment (xitk_widget_t *w, int align) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    wp->align = align;
}

/*
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->align;
  return -1;
}

/*
 * Return used font name
 */
#if 0
char *xitk_labelbutton_get_fontname (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return strdup (wp->font.s);
  return NULL;
}
#endif

/*
 *
 */
void xitk_labelbutton_set_label_offset (xitk_widget_t *w, int offset) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    wp->label_offset = offset;
}

int xitk_labelbutton_get_label_offset (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->label_offset;
  return 0;
}

void *labelbutton_get_user_data (xitk_widget_t *w) {
  _lbutton_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->userdata;
  return NULL;
}

/*
 * Create the labeled button.
 * NOTE: menu calls this with b->skin_element_name == NULL, info->pixmap_name == NULL,
 * and info->pixmap_img to be used but not to be freed like a regular skin.
 */
xitk_widget_t *xitk_info_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, const xitk_skin_element_info_t *info) {
  _lbutton_private_t *wp;

  ABORT_IF_NULL(wl);

  wp = (_lbutton_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->num_gfx           = 3;
  wp->bType             = b->button_type;

  wp->callback          = b->callback;
  wp->state_callback    = b->state_callback;
  wp->userdata          = b->userdata;

  xitk_short_string_init (&wp->label);
  xitk_short_string_set (&wp->label, b->label);

  xitk_short_string_init (&wp->shortcut_label);

  xitk_short_string_init (&wp->shortcut_font);

  xitk_short_string_init (&wp->font);
  xitk_short_string_set (&wp->font, info->label_fontname);

  wp->shortcut_pos      = -1;
  wp->label_visible     = info->label_printable;
  wp->label_static      = info->label_staticity;

  wp->skin              = info->pixmap_img;
  if (((wp->skin.width <= 0) || (wp->skin.height <= 0)) && wp->skin.image) {
    wp->skin.width = wp->skin.image->width;
    wp->skin.height = wp->skin.image->height;
  }
  xitk_shared_image (wl, "xitk_lbutton_temp", wp->skin.width / 3, wp->skin.height, &wp->temp_image.image);
  wp->temp_image.x = 0;
  wp->temp_image.y = 0;
  wp->temp_image.width = wp->skin.width / 3;
  wp->temp_image.height = wp->skin.height;
  strlcpy (wp->skin_element_name,
    b->skin_element_name && b->skin_element_name[0] ? b->skin_element_name : "-",
    sizeof (wp->skin_element_name));

  wp->color[XITK_IMG_STATE_NORMAL] = info->label_color;
  wp->color[XITK_IMG_STATE_FOCUS] = info->label_color_focus;
  wp->color[XITK_IMG_STATE_SELECTED] = info->label_color_click;

  wp->label_offset      = 0;
  wp->align             = info->label_alignment;
  wp->label_dy          = (info->label_y > 0) && (info->label_y < wp->skin.height)
                        ? info->label_y - (wp->skin.height >> 1) : 0;

  xitk_widget_state_from_info (&wp->w, info);
  wp->w.state          |= (wp->bType == RADIO_BUTTON) || (wp->bType== TAB_BUTTON)
                        ? XITK_WIDGET_STATE_TOGGLE : 0;
  wp->w.x               = info->x;
  wp->w.y               = info->y;
  wp->w.width           = wp->skin.width / 3;
  wp->w.height          = wp->skin.height;
  wp->w.type            = WIDGET_TYPE_LABELBUTTON | WIDGET_FOCUSABLE | WIDGET_TABABLE
                        | WIDGET_CLICKABLE | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = labelbutton_event;

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_labelbutton_create (xitk_widget_list_t *wl,
  xitk_skin_config_t *skonfig, const xitk_labelbutton_widget_t *b) {
  const xitk_skin_element_info_t *pinfo;
  xitk_skin_element_info_t info;

  XITK_CHECK_CONSTITENCY (b);
  pinfo = xitk_skin_get_info (skonfig, b->skin_element_name);
  if (pinfo) {
    if (!pinfo->visibility) {
      info = *pinfo;
      info.visibility = 1;
      pinfo = &info;
    }
  } else {
    memset (&info, 0, sizeof (info));
    info.label_alignment = b->align;
    info.visibility = -1;
    pinfo = &info;
  }

  return xitk_info_labelbutton_create (wl, b, pinfo);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, int x, int y, int width, int height,
  uint32_t ncolor, uint32_t fcolor, uint32_t ccolor, const char *fname) {
  _lbutton_private_t *wp;

  ABORT_IF_NULL (wl);
  XITK_CHECK_CONSTITENCY (b);

  wp = (_lbutton_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->bType             = b->button_type;

  wp->callback          = b->callback;
  wp->state_callback    = b->state_callback;
  wp->userdata          = b->userdata;

  xitk_short_string_init (&wp->label);
  xitk_short_string_set (&wp->label, b->label);

  xitk_short_string_init (&wp->shortcut_label);

  xitk_short_string_init (&wp->shortcut_font);

  xitk_short_string_init (&wp->font);
  xitk_short_string_set (&wp->font, fname);

  wp->shortcut_pos      = -1;
  wp->label_visible     = 1;
  wp->label_static      = 0;
  wp->num_gfx           = 4;

  if (b->button_type == TAB_BUTTON) {
    if (xitk_shared_image (wl, "xitk_tabbutton", width * 4, height, &wp->skin.image) == 1) {
      wp->skin.image->last_state = XITK_IMG_STATE_SEL_FOCUS;
      xitk_image_draw_tab (wp->skin.image);
    }
  } else if (b->skin_element_name && !strcmp (b->skin_element_name, "XITK_NOSKIN_FLAT")) {
    if (xitk_shared_image (wl, "xitk_labelbutton_f", width * 4, height, &wp->skin.image) == 1) {
      wp->skin.image->last_state = XITK_IMG_STATE_SEL_FOCUS;
      xitk_image_draw_flat_three_state (wp->skin.image);
    }
  } else {
    if (xitk_shared_image (wl, "xitk_labelbutton_b", width * 4, height, &wp->skin.image) == 1) {
      wp->skin.image->last_state = XITK_IMG_STATE_SEL_FOCUS;
      xitk_image_draw_bevel_three_state (wp->skin.image);
    }
  }
  wp->skin.x     = 0;
  wp->skin.y     = 0;
  wp->skin.width = 0;
  wp->skin.height = 0;
  if (wp->skin.image) {
    wp->skin.width = wp->skin.image->width;
    wp->skin.height = wp->skin.image->height;
  }

  xitk_shared_image (wl, "xitk_lbutton_temp", wp->skin.width / 4, wp->skin.height, &wp->temp_image.image);
  wp->temp_image.x = 0;
  wp->temp_image.y = 0;
  wp->temp_image.width = wp->skin.width / 4;
  wp->temp_image.height = wp->skin.height;

  wp->skin_element_name[0] = 0;
  wp->color[XITK_IMG_STATE_NORMAL] = ncolor;
  wp->color[XITK_IMG_STATE_FOCUS] = fcolor;
  wp->color[XITK_IMG_STATE_SELECTED] = ccolor;

  wp->label_offset      = 0;
  wp->align             = b->align;
  wp->label_dy          = 0;

  wp->w.state          &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->w.state          |= (wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON)
                        ? XITK_WIDGET_STATE_TOGGLE : 0;
  wp->w.x               = x;
  wp->w.y               = y;
  wp->w.width           = wp->skin.width / 4;
  wp->w.height          = wp->skin.height;
  wp->w.type            = WIDGET_TYPE_LABELBUTTON | WIDGET_FOCUSABLE | WIDGET_TABABLE
                        | WIDGET_CLICKABLE | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event           = labelbutton_event;

  return &wp->w;
}

