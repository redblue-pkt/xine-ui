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
#include <stdlib.h>
#include <string.h>

#include "_xitk.h"
#include "labelbutton.h"

#define CLICK     1
#define FOCUS     2
#define NORMAL    3

typedef struct {
  xitk_widget_t           w;

  xitk_widget_t          *bWidget;
  int                     bType;
  int                     bClicked;

  int                     focus;

  int                     bState;
  int                     bOldState;
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

  char                   *label;
  char                   *fontname;
  
  /* Only used if (w->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER || WIDGET_GROUP_MENU */
  char                   *shortcut_label;
  char                   *shortcut_font;
  int                     shortcut_pos;

  char                    skin_element_name[64];
  char                    lbuf[32];
  char                    normcolor[32];
  char                    focuscolor[32];
  char                    clickcolor[32];
} _lbutton_private_t;

static size_t _strlcpy (char *d, const char *s, size_t l) {
  size_t n;
  if (!s)
    s = "";
  n = strlen (s);
  if (l > n + 1)
    l = n + 1;
  memcpy (d, s, l);
  d[l - 1] = 0;
  return n;
}

static void _labelbutton_set_label (_lbutton_private_t *wp, const char *label) {
  size_t n;

  if (wp->label != wp->lbuf)
    free (wp->label);
  if (!label)
    label = "";
  n = strlen (label) + 1;
  if (n <= sizeof (wp->lbuf)) {
    memcpy (wp->lbuf, label, n);
    wp->label = wp->lbuf;
  } else {
    wp->label = strdup (label);
  }
}

/*
 *
 */
static void _labelbutton_destroy (_lbutton_private_t *wp) {
  xitk_image_free_image (&wp->temp_image.image);
  if (wp->skin_element_name[0] == '\x01')
    xitk_image_free_image (&wp->skin.image);

  if (wp->label != wp->lbuf)
    XITK_FREE (wp->label);
  XITK_FREE (wp->shortcut_label);
  XITK_FREE (wp->shortcut_font);
  XITK_FREE (wp->fontname);
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
  if (wp->w.visible == 1) {
    xitk_image_t *skin = wp->skin.image;

    if (skin->mask)
        return xitk_is_cursor_out_mask (&wp->w, skin->mask, x + wp->skin.x, y + wp->skin.y);
    else 
      return 1;
  }
  return 0;
}

/*
 * Draw the string in pixmap pix, then return it
 */
static void _create_labelofbutton (_lbutton_private_t *wp,
    xitk_pixmap_t *pix, int xsize, int ysize,
    char *label, char *shortcut_label, int shortcut_pos, int state) {
  xitk_font_t             *fs = NULL;
  int                      lbear, rbear, width, asc, des;
  int                      xoff = 0, yoff = 0, DefaultColor = -1;
  unsigned int             fg = 0;
  unsigned int             origin = 0;
  xitk_color_names_t      *color = NULL;
  GC gc = wp->w.wl->gc;

  /* Try to load font */
  if (wp->fontname)
    fs = xitk_font_load_font (wp->w.wl->xitk, wp->fontname);
  if (fs == NULL) 
    fs = xitk_font_load_font (wp->w.wl->xitk, xitk_get_system_font ());

  if (fs == NULL)
    XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);
  
  xitk_font_set_font(fs, gc);
  xitk_font_string_extent(fs, (label && strlen(label)) ? label : "Button", &lbear, &rbear, &width, &asc, &des);

  if ((state == CLICK) && (wp->label_static == 0)) {
    xoff = -4;
    yoff = 1;
  }

  if ((wp->skin_element_name[0] & ~1)
    || (!(wp->skin_element_name[0] & ~1) && ((fg = xitk_get_black_color ()) == (unsigned int)-1))) {
    
    /*  Some colors configurations */
    switch(state) {
    case CLICK:
      if(!strcasecmp(wp->clickcolor, "Default")) {
	DefaultColor = 255;
      }
      else {
	color = xitk_get_color_name(wp->clickcolor);
      }
      break;
      
    case FOCUS:
      if(!strcasecmp(wp->focuscolor, "Default")) {
	DefaultColor = 0;
      }
      else {
	color = xitk_get_color_name(wp->focuscolor);
      }
      break;
      
    case NORMAL:
      if(!strcasecmp(wp->normcolor, "Default")) {
	DefaultColor = 0;
      }
      else {
	color = xitk_get_color_name(wp->normcolor);
      }
      break;
    }
    
    if(color == NULL || DefaultColor != -1) {
      fg = xitk_get_pixel_color_from_rgb(wp->w.wl->xitk, DefaultColor, DefaultColor, DefaultColor);
    }
    else {
      fg = xitk_get_pixel_color_from_rgb(wp->w.wl->xitk, color->red, color->green, color->blue);
    }
  }

  if (wp->bType == TAB_BUTTON) {
    /* The tab lift effect. */
    origin = ((ysize - 3 + asc + des + yoff) >> 1) - des + 3;
    if ((state == FOCUS) || (state == CLICK))
      origin -= 3;
  } else {
    origin = ((ysize + asc + des + yoff) >> 1) - des + wp->label_dy;
  }

  /*  Put text in the right place */
  if (wp->align == ALIGN_CENTER) {
    xitk_pixmap_draw_string (pix, fs,
      ((xsize - (width + xoff)) >> 1) + wp->label_offset, origin, label, strlen(label), fg);
  } else if (wp->align == ALIGN_LEFT) {
    xitk_pixmap_draw_string (pix, fs,
      (((state != CLICK) ? 1 : 5)) + wp->label_offset, origin, label, strlen(label), fg);
    /* shortcut is only permited if alignement is set to left */
    if (shortcut_label[0] && (shortcut_pos >= 0)) {
      xitk_font_t *short_font;
  
      if (wp->shortcut_font)
        short_font = xitk_font_load_font (wp->w.wl->xitk, wp->shortcut_font);
      else
        short_font = fs;
      if (shortcut_pos == 0)
        shortcut_pos = xsize - 5 - xitk_font_get_string_length (short_font, shortcut_label);
      xitk_pixmap_draw_string (pix, short_font,
        (((state != CLICK) ? 1 : 5)) + shortcut_pos, origin, shortcut_label, strlen (shortcut_label), fg);
      if (short_font != fs)
        xitk_font_unload_font (short_font);
    }
  } else if (wp->align == ALIGN_RIGHT) {
    xitk_pixmap_draw_string (pix, fs,
      (xsize - (width + ((state != CLICK) ? 5 : 1))) + wp->label_offset, origin, label, strlen(label), fg);
  }

  xitk_font_unload_font (fs);
  if (color)
    xitk_free_color_name (color);
}

/*
 * Paint the button with correct background pixmap
 */
static void _labelbutton_partial_paint (_lbutton_private_t *wp, widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
  printf ("xitk.labelbutton.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (wp->w.visible == 1) {
    int state = 0, mode;

    mode = -1;
    if ((wp->focus == FOCUS_RECEIVED) || (wp->focus == FOCUS_MOUSE_IN)) {
      if (wp->bClicked) {
        mode = 2;
        state = CLICK;
      } else {
        if (!wp->bState || (wp->bType == CLICK_BUTTON)) {
          mode = 1;
          state = FOCUS;
        } else {
          if ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON)) {
            mode = 2;
            state = CLICK;
          } else {
            /* FIXME: original code did nothing here. however, initial contents of pixmap
               * are proven undefined... */
            mode = 0;
            state = NORMAL;
          }
        }
      }
    } else {
      if (wp->bState && ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON))) {
        if ((wp->bOldState == 1) && (wp->bClicked == 1)) {
          mode = 0;
          state = NORMAL;
        } else {
          mode = 2;
          state = CLICK;
        }
      } else {
        mode = 0;
        state = NORMAL;
      }
    }
    if (mode >= 0)
      xitk_part_image_copy (wp->w.wl, &wp->skin, &wp->temp_image,
        mode * wp->skin.width / 3, 0, wp->skin.width / 3, wp->skin.height, 0, 0);
    if (wp->label_visible) {
      _create_labelofbutton (wp, wp->temp_image.image->image,
        wp->skin.width / 3, wp->skin.height,
        wp->label, wp->shortcut_label, wp->shortcut_pos, state);
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
}

/*
 * Handle click events
 */
static int _labelbutton_click (_lbutton_private_t *wp, int button, int bUp, int x, int y, int modifier) {
  int ret = 0;

  (void)x;
  (void)y;
  if (button == Button1) {
    wp->bClicked = !bUp;
    wp->bOldState = wp->bState;

    if (bUp && (wp->focus == FOCUS_RECEIVED)) {
      wp->bState = !wp->bState;
      _labelbutton_paint (wp);
      if ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON)) {
        if (wp->state_callback)
          wp->state_callback (wp->bWidget, wp->userdata, wp->bState, modifier);
      } else if (wp->bType == CLICK_BUTTON) {
        if (wp->callback)
          wp->callback (wp->bWidget, wp->userdata, wp->bState);
      }
    } else {
      _labelbutton_paint (wp);
    }
    ret = 1;
  }
  return ret;
}

static int _labelbutton_key (_lbutton_private_t *wp, const char *s, int modifier) {
  static const char k[] = {
    XITK_CTRL_KEY_PREFIX, XITK_KEY_RIGHT,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_RETURN,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_NUMPAD_ENTER,
    XITK_CTRL_KEY_PREFIX, XITK_KEY_ISO_ENTER,
    ' ', 0
  };
  int i, n = sizeof (k) / sizeof (k[0]);

  if (wp->focus != FOCUS_RECEIVED)
    return 0;
  if (!s)
    return 0;

  if (modifier & ~(MODIFIER_SHIFT | MODIFIER_NUML))
    return 0;

  for (i = (wp->w.type & WIDGET_GROUP_MENU) ? 0 : 2; i < n; i += 2) {
    if (!memcmp (s, k + i, 2))
      break;
  }
  if (i >= n)
    return 0;

  wp->bClicked = 0;
  wp->bOldState = wp->bState;
  if (wp->focus == FOCUS_RECEIVED) {
    wp->bState = !wp->bState;
    _labelbutton_paint (wp);
    if ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON)) {
      if (wp->state_callback)
        wp->state_callback (wp->bWidget, wp->userdata, wp->bState, modifier);
    } else if (wp->bType == CLICK_BUTTON) {
      if (wp->callback)
        wp->callback (wp->bWidget, wp->userdata, wp->bState);
    }
  } else {
    _labelbutton_paint (wp);
  }
  return 1;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_label (xitk_widget_t *w, const char *newlabel) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    _labelbutton_set_label (wp, newlabel);
    _labelbutton_paint (wp);
    return 1;
  }
  return 0;
}

/*
 * Return the current button label
 */
const char *xitk_labelbutton_get_label (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->label;
  return NULL;
}

/*
 * Changing button caption
 */
int xitk_labelbutton_change_shortcut_label (xitk_widget_t *w, const char *newlabel, int pos, const char *newfont) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp
    && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)
    && (((wp->w.type & WIDGET_GROUP_MASK) == WIDGET_GROUP_MENU) || ((wp->w.type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER)))) {
    if ((wp->shortcut_label = (char *)realloc (wp->shortcut_label, strlen (newlabel) + 1)) != NULL)
      strcpy (wp->shortcut_label, newlabel);
    if (newfont && (wp->shortcut_font = (char *)realloc (wp->shortcut_font, strlen (newfont) + 1)) != NULL)
      strcpy (wp->shortcut_font, newfont);
    if (wp->shortcut_label[0]) {
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
const char *xitk_labelbutton_get_shortcut_label (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp
    && (((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)
    && (((wp->w.type & WIDGET_GROUP_MASK) == WIDGET_GROUP_MENU) || ((wp->w.type & WIDGET_GROUP_MASK) == WIDGET_GROUP_BROWSER)))) {
    return wp->shortcut_label;
  }
  return NULL;
}

/*
 * Handle focus on button
 */
static int _labelbutton_focus (_lbutton_private_t *wp, int focus) {
  wp->focus = focus;
  return 1;
}

/*
 *
 */
static void _labelbutton_new_skin (_lbutton_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp->skin_element_name[0] & ~1) {
    const xitk_skin_element_info_t *info;

    xitk_skin_lock (skonfig);
    info = xitk_skin_get_info (skonfig, wp->skin_element_name);
    XITK_FREE (wp->fontname);
    if (info) {
      wp->skin = info->pixmap_img;
      _strlcpy (wp->normcolor,  info->label_color,       sizeof (wp->normcolor));
      _strlcpy (wp->focuscolor, info->label_color_focus, sizeof (wp->focuscolor));
      _strlcpy (wp->clickcolor, info->label_color_click, sizeof (wp->clickcolor));
      wp->fontname      = strdup (info->label_fontname);
      wp->label_visible = info->label_printable;
      wp->label_static  = info->label_staticity;
      wp->align         = info->label_alignment;
      wp->w.x      = info->x;
      wp->w.y      = info->y;
      wp->w.width  = wp->skin.width / 3;
      wp->w.height = wp->skin.height;
      wp->label_dy = (info->label_y > 0) && (info->label_y < wp->skin.height)
                   ? info->label_y - (wp->skin.height >> 1) : 0;
      wp->w.visible = info->visibility ? 1 : -1;
      wp->w.enable = info->enability;
      xitk_image_free_image (&wp->temp_image.image);
      xitk_shared_image (wp->w.wl, "xitk_lbutton_temp", wp->skin.width / 3, wp->skin.height, &wp->temp_image.image);
      wp->temp_image.x = 0;
      wp->temp_image.y = 0;
      wp->temp_image.width = wp->skin.width / 3;
      wp->temp_image.height = wp->skin.height;
    }
    xitk_skin_unlock (skonfig);
    xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
  }
}


static int labelbutton_event (xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (!event || !wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_LABELBUTTON)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = wp->w.x;
      event->y = wp->w.y;
      event->width = wp->w.width;
      event->height = wp->w.height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      _labelbutton_partial_paint (wp, event);
      break;
    case WIDGET_EVENT_CLICK: {
      int r = _labelbutton_click (wp, event->button,
        event->button_pressed, event->x, event->y, event->modifier);
      if (result) {
        result->value = r;
        return 1;
      }
      break;
    }
    case WIDGET_EVENT_KEY:
      return _labelbutton_key (wp, event->string, event->modifier);
    case WIDGET_EVENT_FOCUS:
      _labelbutton_focus (wp, event->focus);
      break;
    case WIDGET_EVENT_INSIDE:
      if (result) {
        result->value = _labelbutton_inside (wp, event->x, event->y);
        return 1;
      }
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      _labelbutton_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _labelbutton_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = _labelbutton_get_skin (wp, event->skin_layer);
        return 1;
      }
      break;
    default: ;
  }
  return 0;
}

/*
 * Return state (ON/OFF) if button is radio button, otherwise -1
 */
int xitk_labelbutton_get_state (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    if ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON))
      return wp->bState;
  }
  return -1;
}

/*
 * Set label button alignment
 */
void xitk_labelbutton_set_alignment (xitk_widget_t *w, int align) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    wp->align = align;
}

/*
 * Get label button alignment
 */
int xitk_labelbutton_get_alignment (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->align;
  return -1;
}

/*
 * Return used font name
 */
char *xitk_labelbutton_get_fontname (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    if (wp->fontname)
      return strdup (wp->fontname);
  }
  return NULL;
}

/*
 *
 */
void xitk_labelbutton_set_label_offset (xitk_widget_t *w, int offset) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    wp->label_offset = offset;
}

int xitk_labelbutton_get_label_offset (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->label_offset;
  return 0;
}

/*
 * Set radio button to state 'state'
 */
void xitk_labelbutton_set_state (xitk_widget_t *w, int state) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;
  
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON)) {
    int clk, focus;
    
    if ((wp->bType == RADIO_BUTTON) || (wp->bType == TAB_BUTTON)) {
      if (xitk_labelbutton_get_state (&wp->w) != state) {
        focus = wp->focus;
        clk = wp->bClicked;

        wp->focus = FOCUS_RECEIVED;
        wp->bClicked = 1;
        wp->bOldState = wp->bState;
        wp->bState = state;
        _labelbutton_paint (wp);

        wp->focus = focus;
        wp->bClicked = clk;
        _labelbutton_paint (wp);
      }
    }
  }
}

void *labelbutton_get_user_data (xitk_widget_t *w) {
  _lbutton_private_t *wp = (_lbutton_private_t *)w;

  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON))
    return wp->userdata;
  return NULL;
}

/*
 * Create the labeled button
 */
xitk_widget_t *xitk_info_labelbutton_create (xitk_widget_list_t *wl,
  const xitk_labelbutton_widget_t *b, const xitk_skin_element_info_t *info) {
  _lbutton_private_t *wp;
  
  ABORT_IF_NULL(wl);

  wp = (_lbutton_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->bWidget           = &wp->w;
  wp->bType             = b->button_type;
  wp->bClicked          = 0;
  wp->focus             = FOCUS_LOST;
  wp->bState            = 0;
  wp->bOldState         = 0;

  wp->callback          = b->callback;
  wp->state_callback    = b->state_callback;
  wp->userdata          = b->userdata;

  wp->label = wp->lbuf;
  _labelbutton_set_label (wp, b->label);

  wp->shortcut_label    = strdup ("");
  wp->shortcut_font     = strdup (info->label_fontname);
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

  if (info->pixmap_name && (info->pixmap_name[0] == '\x01') && (info->pixmap_name[1] == 0)) {
    wp->skin_element_name[0] = '\x01';
    wp->skin_element_name[1] = 0;
  } else {
    _strlcpy (wp->skin_element_name, b->skin_element_name, sizeof (wp->skin_element_name));
  }
  _strlcpy (wp->normcolor, info->label_color, sizeof (wp->normcolor));
  _strlcpy (wp->focuscolor, info->label_color_focus, sizeof (wp->focuscolor));
  _strlcpy (wp->clickcolor, info->label_color_click, sizeof (wp->clickcolor));
  wp->fontname          = strdup (info->label_fontname);

  wp->label_offset      = 0;
  wp->align             = info->label_alignment;
  wp->label_dy          = (info->label_y > 0) && (info->label_y < wp->skin.height)
                        ? info->label_y - (wp->skin.height >> 1) : 0;

  wp->w.enable          = info->enability;
  wp->w.visible         = info->visibility;
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
  const char *ncolor, const char *fcolor, const char *ccolor, const char *fname) {
  xitk_skin_element_info_t info;

  ABORT_IF_NULL(wl);

  XITK_CHECK_CONSTITENCY(b);
  info.x                 = x;
  info.y                 = y;
  info.label_alignment   = b->align;
  info.label_y           = 0;
  info.label_printable   = 1;
  info.label_staticity   = 0;
  info.visibility        = 0;
  info.enability         = 0;
  info.label_color       = (char *)ncolor;
  info.label_color_focus = (char *)fcolor;
  info.label_color_click = (char *)ccolor;
  info.label_fontname    = (char *)fname;
  info.pixmap_name       = (char *)"\x01";
  if (b->button_type == TAB_BUTTON) {
    if (xitk_shared_image (wl, "xitk_tabbutton", width * 3, height, &info.pixmap_img.image) == 1)
      draw_tab (info.pixmap_img.image);
  } else if (b->skin_element_name && !strcmp (b->skin_element_name, "XITK_NOSKIN_FLAT")) {
    if (xitk_shared_image (wl, "xitk_labelbutton_f", width * 3, height, &info.pixmap_img.image) == 1)
      draw_flat_three_state (info.pixmap_img.image);
  } else {
    if (xitk_shared_image (wl, "xitk_labelbutton_b", width * 3, height, &info.pixmap_img.image) == 1)
      draw_bevel_three_state (info.pixmap_img.image);
  }
  info.pixmap_img.x     = 0;
  info.pixmap_img.y     = 0;
  info.pixmap_img.width = 0;
  info.pixmap_img.height = 0;
  return xitk_info_labelbutton_create (wl, b, &info);
}
