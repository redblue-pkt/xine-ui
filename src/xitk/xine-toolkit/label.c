/*
 * Copyright (C) 2000-2022 the xine project
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
#include <pthread.h>
#include <unistd.h>

#include "_xitk.h"
#include "label.h"


typedef struct {
  xitk_widget_t           w;

  char                    skin_element_name[64];
  xitk_short_string_t     fontname, label;

  xitk_image_t           *labelpix;

  xitk_pix_font_t        *pix_font;

  int                     length;      /* length in char */
  xitk_image_t           *font, *highlight_font;
  size_t                  label_len;

  xitk_simple_callback_t  callback;
  void                   *userdata;

  int                     animation;
  int                     anim_step;
  int                     anim_timer;
  int                     anim_offset;

  int                     label_visible;

  int                     anim_running;
  pthread_t               anim_thread;
  pthread_mutex_t         change_mutex;
} _label_private_t;

static int _find_pix_font_char (xitk_pix_font_t *pf, xitk_point_t *found, int this_char) {
  int range, n = 0;

  for (range = 0; pf->unicode_ranges[range].first > 0; range++) {
    if ((this_char >= pf->unicode_ranges[range].first) && (this_char <= pf->unicode_ranges[range].last))
      break;
    n += pf->unicode_ranges[range].last - pf->unicode_ranges[range].first + 1;
  }

  if (pf->unicode_ranges[range].first <= 0) {
    *found = pf->unknown;
    return 0;
  }

  n += this_char - pf->unicode_ranges[range].first;
  found->x = (n % pf->chars_per_row) * pf->char_width;
  found->y = (n / pf->chars_per_row) * pf->char_height;
  return 1;
}

static void _create_label_pixmap (_label_private_t *wp) {
  xitk_image_t          *font;
  xitk_pix_font_t       *pix_font;
  int                    pixwidth;
  int                    x_dest;
  int                    len, anim_add = 0;
  uint16_t               buf[2048];

  font = (wp->w.state & (XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS)) && wp->highlight_font ? wp->highlight_font : wp->font;
  wp->anim_offset = 0;

  if (!font->pix_font) {
    /* old style */
    xitk_image_set_pix_font (font, "");
    if (!font->pix_font)
      return;
  }
  pix_font = font->pix_font;

  {
    const uint8_t *p = (const uint8_t *)wp->label.s;
    uint16_t *q = buf, *e = buf + sizeof (buf) / sizeof (buf[0]) - 1;
    while (*p && (q < e)) {
      if (!(p[0] & 0x80)) {
        *q++ = p[0];
        p += 1;
      } else if (((p[0] & 0xe0) == 0xc0) && ((p[1] & 0xc0) == 0x80)) {
        *q++ = ((uint32_t)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
        p += 2;
      } else if (((p[0] & 0xf0) == 0xe0) && ((p[1] & 0xc0) == 0x80) && ((p[2] & 0xc0) == 0x80)) {
        *q++ = ((uint32_t)(p[0] & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
        p += 3;
      } else {
        *q++ = p[0];
        p += 1;
      }
    }
    *q = 0;
    wp->label_len = len = q - buf;
  }

  if (wp->animation && (len > wp->length))
    anim_add = wp->length + 5;

  /* reuse or reallocate pixmap */
  pixwidth = len + anim_add;
  if (pixwidth < wp->length)
    pixwidth = wp->length;
  if (pixwidth < 1)
    pixwidth = 1;
  pixwidth *= pix_font->char_width;
  if (wp->labelpix) {
    if ((wp->labelpix->width != pixwidth) || (wp->labelpix->height != pix_font->char_height))
      xitk_image_free_image (&wp->labelpix);
  }
  if (!wp->labelpix) {
    wp->labelpix = xitk_image_new (wp->w.wl->xitk, NULL, 0, pixwidth, pix_font->char_height);
#if 0
    printf ("xine.label: new pixmap %d:%d -> %d:%d for \"%s\"\n", pixwidth, pix_font->char_height,
      wp->labelpix->width, wp->labelpix->height, wp->label);
#endif
    if (!wp->labelpix)
      return;
  }
  x_dest = 0;

  /* [foo] */
  {
    const uint16_t *p = buf;
    while (*p) {
      xitk_point_t pt;
      _find_pix_font_char (font->pix_font, &pt, *p);
      p++;

      xitk_image_copy_rect (font, wp->labelpix,
        pt.x, pt.y, pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    }
  }
  /* foo[ *** fo] */
  if (anim_add) {
    xitk_image_copy_rect (font, wp->labelpix,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    xitk_image_copy_rect (font, wp->labelpix,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    xitk_image_copy_rect (font, wp->labelpix,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    xitk_image_copy_rect (font, wp->labelpix,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    xitk_image_copy_rect (font, wp->labelpix,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    xitk_image_copy_rect (wp->labelpix, wp->labelpix,
      0, 0, pixwidth - x_dest, pix_font->char_height, x_dest, 0);
    x_dest = pixwidth;
  }

  /* fill gap with spaces */
  if (x_dest < pixwidth) {
    do {
      xitk_image_copy_rect (font, wp->labelpix,
        pix_font->space.x, pix_font->space.y,
        pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    } while (x_dest < pixwidth);
  }
}

/*
 *
 */
static void _label_destroy (_label_private_t *wp) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    pthread_mutex_lock (&wp->change_mutex);

    if (wp->anim_running) {
      void *dummy;

      wp->anim_running = 0;
      pthread_mutex_unlock (&wp->change_mutex);
      pthread_join (wp->anim_thread, &dummy);
      pthread_mutex_lock (&wp->change_mutex);
    }

    xitk_image_free_image (&(wp->labelpix));

    if (!wp->skin_element_name[0]) {
      xitk_image_free_image (&(wp->font));
      xitk_image_free_image (&(wp->highlight_font));
    }

    xitk_short_string_deinit (&wp->label);
    xitk_short_string_deinit (&wp->fontname);

    pthread_mutex_unlock (&wp->change_mutex);
    pthread_mutex_destroy (&wp->change_mutex);
  }
}

/*
 *
 */
static xitk_image_t *_label_get_skin (_label_private_t *wp, int sk) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    if (sk == FOREGROUND_SKIN && wp->font) {
      return wp->font;
    }
  }
  return NULL;
}

/*
 *
 */
const char *xitk_label_get_label (xitk_widget_t *w) {
  _label_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))
    return wp->label.s;
  return NULL;
}

/*
 *
 */
static void _label_paint (_label_private_t *wp, const widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
    printf ("xitk.label.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if ((wp->w.state & XITK_WIDGET_STATE_VISIBLE) && wp->w.width && wp->label_visible) {
    xitk_image_t *font = (xitk_image_t *) wp->font;

    /* non skinable widget */
    if (!wp->skin_element_name[0]) {
      xitk_font_t *fs = xitk_font_load_font (wp->w.wl->xitk, wp->fontname.s);
      xitk_image_t *bg = xitk_image_new (wp->w.wl->xitk, NULL, 0, wp->w.width, wp->w.height);

      if (fs && bg) {
        int lbear, rbear, wid, asc, des;

        xitk_font_string_extent (fs, wp->label.s, &lbear, &rbear, &wid, &asc, &des);

        xitk_image_copy_rect (font, bg, 0, 0, wp->font->width, wp->font->height, 0, 0);
        xitk_image_draw_string (bg, fs, 2, ((wp->font->height + asc + des) >> 1) - des,
          wp->label.s, wp->label_len, xitk_get_cfg_num (wp->w.wl->xitk, XITK_BLACK_COLOR));
        xitk_image_draw_image (wp->w.wl, bg,
          event->x - wp->w.x, event->y - wp->w.y, event->width, event->height, event->x, event->y, 0);
      }
      xitk_image_free_image (&bg);
      xitk_font_unload_font (fs);
      return;
    }
    else if (wp->pix_font) {
      if (((wp->w.state ^ wp->w.shown_state) & (XITK_WIDGET_STATE_MOUSE | XITK_WIDGET_STATE_FOCUS))) {
        if (wp->highlight_font)
          _create_label_pixmap (wp);
      }
      xitk_image_draw_image (wp->w.wl, wp->labelpix,
        wp->anim_offset + event->x - wp->w.x, event->y - wp->w.y,
        event->width, event->height,
        event->x, event->y, wp->anim_running);
      wp->w.shown_state = wp->w.state;
    }
  }
}

/*
 *
 */
static void *xitk_label_animation_loop (void *data) {
  _label_private_t *wp = (_label_private_t *)data;

  pthread_mutex_lock (&wp->change_mutex);

  while (1) {
    unsigned long t_anim;

    if (wp->anim_running != 1)
      break;
    if ((wp->w.state & XITK_WIDGET_STATE_VISIBLE) && wp->pix_font) {
      widget_event_t event;

      event.x = wp->w.x;
      event.y = wp->w.y;
      event.width = wp->w.width;
      event.height = wp->w.height;
      wp->anim_offset += wp->anim_step;
      if (wp->anim_offset >= (wp->pix_font->char_width * ((int)wp->label_len + 5)))
        wp->anim_offset = 0;
      _label_paint (wp, &event);
      wp->w.shown_state = wp->w.state;
    }
    t_anim = wp->anim_timer;
    pthread_mutex_unlock (&wp->change_mutex);
    xitk_usec_sleep (t_anim);
    pthread_mutex_lock (&wp->change_mutex);
  }
  wp->anim_running = 0;
  pthread_mutex_unlock (&wp->change_mutex);

  return NULL;
}

/*
 *
 */
static void _label_setup_label (_label_private_t *wp, int paint) {
  if (!wp->w.width)
    return;

  /* Inform animation thread to not paint the label */
  pthread_mutex_lock (&wp->change_mutex);

  if (wp->skin_element_name[0]) {
    _create_label_pixmap (wp);
  } else {
    xitk_image_free_image (&(wp->labelpix));
    wp->anim_offset = 0;
  }

  if (wp->animation && ((int)wp->label_len > wp->length)) {
    if (!wp->anim_running) {
      pthread_attr_t       pth_attrs;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
      struct sched_param   pth_params;
#endif
      int r;

      wp->anim_running = 1;
      pthread_attr_init (&pth_attrs);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
      pthread_attr_getschedparam(&pth_attrs, &pth_params);
      pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
      pthread_attr_setschedparam(&pth_attrs, &pth_params);
#endif
      r = pthread_create (&wp->anim_thread, &pth_attrs, xitk_label_animation_loop, (void *)wp);
      pthread_attr_destroy (&pth_attrs);
      if (r)
        wp->anim_running = 0;
    } else {
      wp->anim_running = 1;
    }
  } else {
    if (wp->anim_running) {
      /* panel mrl animation has a long interval of 0.5s.
       * better not wait for that, as it may interfere with
       * gapleess stream switch. */
      wp->anim_running = 2;
    }
  }
  if (paint) {
    widget_event_t event;

    event.x = wp->w.x;
    event.y = wp->w.y;
    event.width = wp->w.width;
    event.height = wp->w.height;
    _label_paint (wp, &event);
    wp->w.shown_state = wp->w.state;
  }
  pthread_mutex_unlock (&wp->change_mutex);
}

/*
 *
 */
static void _label_new_skin (_label_private_t *wp, xitk_skin_config_t *skonfig) {
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    if (wp->skin_element_name[0]) {
      const xitk_skin_element_info_t *info;

      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, wp->skin_element_name);
      if (info && info->label_pixmap_font_img) {
        wp->font          = info->label_pixmap_font_img;
        wp->highlight_font = info->label_pixmap_highlight_font_img;
        wp->pix_font      = wp->font->pix_font;
        if (!wp->pix_font) {
          xitk_image_set_pix_font (wp->font, "");
          wp->pix_font    = wp->font->pix_font;
        }
        xitk_short_string_set (&wp->fontname, info->label_fontname);
        wp->length        = info->label_length;
        wp->animation     = info->label_animation;
        wp->anim_step     = info->label_animation_step;
        wp->anim_timer    = info->label_animation_timer;
        if (wp->anim_timer <= 0)
          wp->anim_timer = xitk_get_cfg_num (wp->w.wl->xitk, XITK_TIMER_LABEL_ANIM);
        wp->label_visible = info->label_printable;
        wp->w.x           = info->x;
        wp->w.y           = info->y;
        if (wp->pix_font) {
          wp->w.width     = wp->pix_font->char_width * wp->length;
          wp->w.height    = wp->pix_font->char_height;
        }
        xitk_widget_state_from_info (&wp->w, info);
        _label_setup_label (wp, 1);
      } else {
        wp->w.x           = 0;
        wp->w.y           = 0;
        wp->w.width       = 0;
        wp->w.height      = 0;
        wp->w.state      &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
        wp->label_visible = 0;
        wp->length        = 0;
        wp->animation     = 0;
        wp->anim_step     = 0;
        wp->anim_timer    = 0;
        wp->font          = NULL;
        wp->highlight_font = NULL;
        wp->pix_font      = NULL;
        xitk_image_free_image (&wp->labelpix);
      }

      xitk_skin_unlock(skonfig);
      if (wp->highlight_font && wp->callback)
        wp->w.type |= WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_KEYABLE;
      else
        wp->w.type &= ~(WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_KEYABLE);
      xitk_set_widget_pos (&wp->w, wp->w.x, wp->w.y);
    }
  }
}

/*
 *
 */
int xitk_label_change_label(xitk_widget_t *w, const char *newlabel) {
  _label_private_t *wp;

  xitk_container (wp, w, w);
  if (wp && ((wp->w.type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    size_t s = xitk_short_string_set (&wp->label, newlabel);
    if (s != (size_t)-1) {
      wp->label_len = s;
      _label_setup_label (wp, 1);
    }
    return 1;
  }
  return 0;
}

/*
 *
 */
static int _label_input (_label_private_t *wp, const widget_event_t *event) {
  int  ret = 0;

  if (event->type == WIDGET_EVENT_KEY) {
    static const char k[] = {
      XITK_CTRL_KEY_PREFIX, XITK_KEY_RETURN,
      ' ', 0
    };
    int i, n = sizeof (k) / sizeof (k[0]);

    if (event->modifier & ~(MODIFIER_SHIFT | MODIFIER_NUML))
      return 0;
    if (!event->string)
      return 0;
    for (i = 0; i < n; i += 2) {
      if (!memcmp (event->string, k + i, 2))
        break;
    }
    if (i >= n)
      return 0;
  } else { /* WIDGET_EVENT_CLICK */
    if (event->button != 1)
      return 0;
  }
  if (wp->callback) {
    if (!event->pressed)
      wp->callback (&wp->w, wp->userdata);
    ret = 1;
  }
  return ret;
}

static int notify_event (xitk_widget_t *w, const widget_event_t *event) {
  _label_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_LABEL)
    return 0;
  if (!event)
    return 0;

  switch (event->type) {
    case WIDGET_EVENT_PAINT:
      if (!pthread_mutex_trylock (&wp->change_mutex)) {
        _label_paint (wp, event);
        pthread_mutex_unlock (&wp->change_mutex);
      }
      break;
    case WIDGET_EVENT_CLICK:
    case WIDGET_EVENT_KEY:
      return _label_input (wp, event);
    case WIDGET_EVENT_CHANGE_SKIN:
      _label_new_skin (wp, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      _label_destroy (wp);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (event->image) {
        *event->image = _label_get_skin (wp, event->skin_layer);
        return 1;
      }
      break;
    default: ;
  }
  return 0;
}

/*
 *
 */
static xitk_widget_t *_label_create (_label_private_t *wp, const xitk_label_widget_t *l) {
  wp->callback = l->callback;
  wp->userdata = l->userdata;

  wp->anim_running = 0;

  pthread_mutex_init (&wp->change_mutex, NULL);

  wp->w.type = (wp->highlight_font && wp->callback)
             ? WIDGET_TYPE_LABEL | WIDGET_CLICKABLE | WIDGET_PARTIAL_PAINTABLE
                | WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_KEYABLE
             : WIDGET_TYPE_LABEL | WIDGET_CLICKABLE | WIDGET_PARTIAL_PAINTABLE;
  wp->w.event = notify_event;

  xitk_short_string_init (&wp->label);
  wp->label_len = xitk_short_string_set (&wp->label, l->label);
  if (wp->label_len == (size_t)-1)
    wp->label_len = 0;
  _label_setup_label (wp, 0);

  return &wp->w;
}

/*
 *
 */
xitk_widget_t *xitk_label_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig,
  const xitk_label_widget_t *l) {
  const xitk_skin_element_info_t *info;
  _label_private_t *wp;

  ABORT_IF_NULL (wl);
  XITK_CHECK_CONSTITENCY (l);

  wp = (_label_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;
  strlcpy (wp->skin_element_name,
    l->skin_element_name && l->skin_element_name[0] ? l->skin_element_name : "-",
    sizeof (wp->skin_element_name));
  xitk_short_string_init (&wp->fontname);

  info = xitk_skin_get_info (skonfig, l->skin_element_name);
  if (!info || !info->label_pixmap_font_img) {
    /* not used by this skin || wrong pixmmap name in skin? */
    wp->w.x           = 0;
    wp->w.y           = 0;
    wp->w.width       = 0;
    wp->w.height      = 0;
    wp->w.state      &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    wp->label_visible = 0;
    wp->length        = 0;
    wp->animation     = 0;
    wp->anim_step     = 0;
    wp->anim_timer    = 0;
    wp->font          = NULL;
    wp->highlight_font = NULL;
    wp->pix_font      = NULL;
    wp->labelpix      = NULL;
    return _label_create (wp, l);
  }

  wp->font = info->label_pixmap_font_img;
  wp->highlight_font = info->label_pixmap_highlight_font_img;
  wp->length        = info->label_length;
  wp->pix_font      = wp->font->pix_font;
  if (!wp->pix_font) {
    xitk_image_set_pix_font (wp->font, "");
    wp->pix_font    = wp->font->pix_font;
  }
  wp->animation     = info->label_animation;
  wp->anim_step     = info->label_animation_step;
  wp->anim_timer    = info->label_animation_timer;
  if (wp->anim_timer <= 0)
    wp->anim_timer = xitk_get_cfg_num (wp->w.wl->xitk, XITK_TIMER_LABEL_ANIM);
  wp->label_visible = info->label_printable;
  if (wp->pix_font) {
    wp->w.width  = wp->pix_font->char_width * wp->length;
    wp->w.height = wp->pix_font->char_height;
  }

  xitk_widget_state_from_info (&wp->w, info);
  wp->w.x       = info->x;
  wp->w.y       = info->y;

  return _label_create (wp, l);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_label_create (xitk_widget_list_t *wl,
  const xitk_label_widget_t *l, int x, int y, int width, int height, const char *fontname) {
  _label_private_t *wp;

  ABORT_IF_NULL (wl);
  XITK_CHECK_CONSTITENCY (l);

  wp = (_label_private_t *)xitk_widget_new (wl, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->skin_element_name[0] = 0;
  wp->w.state    &= ~(XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  wp->w.x       = x;
  wp->w.y       = y;
  wp->w.height  = height;
  wp->w.width   =
  wp->length    = width;
  wp->pix_font  = NULL;
  wp->animation = 0;
  wp->anim_step = 1;
  wp->anim_timer = xitk_get_cfg_num (wp->w.wl->xitk, XITK_TIMER_LABEL_ANIM);
  wp->label_visible = 1;

  xitk_short_string_init (&wp->fontname);
  xitk_short_string_set (&wp->fontname, fontname);

  if (l->skin_element_name && !strcmp (l->skin_element_name, "XITK_NOSKIN_INNER")) {
    if (xitk_shared_image (wl, "xitk_label_i", width, height, &wp->font) == 1) {
      xitk_image_draw_flat (wp->font, width, height);
      xitk_image_draw_rectangular_box (wp->font, 0, 0, width, height, DRAW_INNER);
    }
  } else {
    if (xitk_shared_image (wl, "xitk_label_f", width, height, &wp->font) == 1)
      xitk_image_draw_flat (wp->font, width, height);
  }

  wp->highlight_font = NULL;

  return _label_create (wp, l);
}
