/* 
 * Copyright (C) 2000-2019 the xine project
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

static void _create_label_pixmap(xitk_widget_t *w) {
  label_private_data_t  *wp = (label_private_data_t *) w->private_data;
  xitk_image_t          *font         = (xitk_image_t *) wp->font;
  xitk_pix_font_t       *pix_font;
  int                    pixwidth;
  int                    x_dest;
  int                    len, anim_add = 0;
  uint16_t               buf[2048];
  
  wp->anim_offset = 0;

  if (!font->pix_font) {
    /* old style */
    xitk_image_set_pix_font (font, "");
    if (!font->pix_font)
      return;
  }
  pix_font = font->pix_font;

  {
    const uint8_t *p = (const uint8_t *)wp->label;
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
    if ((wp->labelpix->width != pixwidth)
      || (wp->labelpix->height != pix_font->char_height)) {
      xitk_image_destroy_xitk_pixmap (wp->labelpix);
      wp->labelpix = NULL;
    }
  }
  if (!wp->labelpix) {
    wp->labelpix = xitk_image_create_xitk_pixmap (wp->imlibdata, pixwidth,
      pix_font->char_height);
#if 0
    printf ("xine.label: new pixmap %d:%d -> %d:%d for \"%s\"\n", pixwidth, pix_font->char_height,
      wp->labelpix->width, wp->labelpix->height, wp->label);
#endif
  }
  x_dest = 0;

  /* [foo] */
  {
    const uint16_t *p = buf;
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    while (*p) {
      xitk_point_t pt;
      _find_pix_font_char (font->pix_font, &pt, *p);
      p++;
      XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
        wp->labelpix->pixmap, wp->labelpix->gc, pt.x, pt.y,
        pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    }
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }
  /* foo[ *** fo] */
  if (anim_add) {
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (wp->imlibdata->x.disp, wp->labelpix->pixmap,
      wp->labelpix->pixmap, wp->labelpix->gc, 0, 0,
      pixwidth - x_dest, pix_font->char_height, x_dest, 0);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
    x_dest = pixwidth;
  }
  
  /* fill gap with spaces */
  if (x_dest < pixwidth) {
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    do {
      XCopyArea (wp->imlibdata->x.disp, font->image->pixmap,
        wp->labelpix->pixmap, wp->labelpix->gc,
        pix_font->space.x, pix_font->space.y,
        pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    } while (x_dest < pixwidth);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *wp = (label_private_data_t *) w->private_data;

    pthread_mutex_lock (&wp->change_mutex);
    
    if(wp->anim_running) {
      void *dummy;
      
      wp->anim_running = 0;
      pthread_mutex_unlock (&wp->change_mutex);
      pthread_join (wp->anim_thread, &dummy);
      pthread_mutex_lock (&wp->change_mutex);
    }
    
    if (wp->labelpix) {
      xitk_image_destroy_xitk_pixmap(wp->labelpix);
      wp->labelpix = NULL;
    }
    
    if (!(wp->skin_element_name[0] & ~1))
      xitk_image_free_image(&(wp->font));

    XITK_FREE(wp->label);
    XITK_FREE(wp->fontname);

    pthread_mutex_unlock (&wp->change_mutex);
    pthread_mutex_destroy (&wp->change_mutex);

    XITK_FREE(wp);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *wp = (label_private_data_t *) w->private_data;

    if(sk == FOREGROUND_SKIN && wp->font) {
      return wp->font;
    }
  }

  return NULL;
}

/*
 *
 */
const char *xitk_label_get_label(xitk_widget_t *w) {

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
      label_private_data_t *wp = (label_private_data_t *) w->private_data;
    return wp->label;
  }

  return NULL;
}

/*
 *
 */
static void paint_label (xitk_widget_t *w, widget_event_t *event) {
#ifdef XITK_PAINT_DEBUG
    printf ("xitk.label.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL) && w->visible == 1)) {
    label_private_data_t  *wp = (label_private_data_t *) w->private_data;
    xitk_image_t          *font = (xitk_image_t *) wp->font;
    
    if(!wp->label_visible)
      return;
    
    /* non skinable widget */
    if (!(wp->skin_element_name[0] & ~1)) {
      xitk_font_t   *fs = NULL;
      int            lbear, rbear, wid, asc, des;
      xitk_image_t  *bg;

      fs = xitk_font_load_font(wp->imlibdata->x.disp, wp->fontname);
      xitk_font_set_font(fs, wp->font->image->gc);
      xitk_font_string_extent (fs, wp->label, &lbear, &rbear, &wid, &asc, &des);

      bg = xitk_image_create_image(wp->imlibdata, w->width, w->height);

      XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
      XCopyArea (wp->imlibdata->x.disp, font->image->pixmap, bg->image->pixmap, 
		 font->image->gc,
		 0, 0, wp->font->width, wp->font->height, 0, 0);
      XSetForeground(wp->imlibdata->x.disp, font->image->gc, 
		     xitk_get_pixel_color_black(wp->imlibdata));
      xitk_font_draw_string(fs, bg->image->pixmap, font->image->gc,
		  2, ((wp->font->height + asc + des)>>1) - des,
		  wp->label, wp->label_len);
      XCopyArea (wp->imlibdata->x.disp, bg->image->pixmap, w->wl->win, font->image->gc,
        event->x - w->x, event->y - w->y, event->width, event->height, event->x, event->y);
      XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);

      xitk_image_free_image(&bg);
      
      xitk_font_unload_font(fs);
      return;
    }
    else if (wp->pix_font) {
      XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
      XCopyArea (wp->imlibdata->x.disp,
        wp->labelpix->pixmap, w->wl->win, font->image->gc,
        wp->anim_offset + event->x - w->x, event->y - w->y,
        event->width, event->height,
        event->x, event->y);
      if (wp->anim_running)
        XSync (wp->imlibdata->x.disp, False);
      XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
    }
  }
}

/*
 *
 */
static void *xitk_label_animation_loop (void *data) {
  label_private_data_t *wp = (label_private_data_t *)data;

  pthread_mutex_lock (&wp->change_mutex);
  
  while (1) {
    xitk_widget_t *w = wp->lWidget;
    unsigned long t_anim;

    if (!w->running || (wp->anim_running != 1))
      break;
    if ((w->visible == 1) && wp->pix_font) {
      widget_event_t event;

      event.x = w->x;
      event.y = w->y;
      event.width = w->width;
      event.height = w->height;
      wp->anim_offset += wp->anim_step;
      if (wp->anim_offset >= (wp->pix_font->char_width * ((int)wp->label_len + 5)))
        wp->anim_offset = 0;
      paint_label (w, &event);
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
static void label_setup_label(xitk_widget_t *w, const char *new_label, int paint) {
  label_private_data_t *wp = (label_private_data_t *) w->private_data;
  
  /* Inform animation thread to not paint the label */
  pthread_mutex_lock (&wp->change_mutex);
  
  if (new_label != wp->label) {
    size_t new_len;

    if (!new_label) {
      new_label = "";
      new_len = 0;
    } else {
      new_len = strlen (new_label);
    }
    if (wp->label) {
      if (wp->label_len != new_len) {
        free (wp->label);
        wp->label = NULL;
      }
    }
    if (!wp->label)
      wp->label = xitk_xmalloc (new_len + 1);
    if (wp->label)
      memcpy (wp->label, new_label, new_len + 1);
    wp->label_len = new_len;
  }

  if (wp->skin_element_name[0] & ~1) {
    _create_label_pixmap(w);
  } else {
    if (wp->labelpix) {
      xitk_image_destroy_xitk_pixmap (wp->labelpix);
      wp->labelpix = NULL;
    }
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

    event.x = w->x;
    event.y = w->y;
    event.width = w->width;
    event.height = w->height;
    paint_label (w, &event);
  }
  pthread_mutex_unlock (&wp->change_mutex);
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *wp = (label_private_data_t *) w->private_data;
    
    if (wp->skin_element_name[0] & ~1) {
      const xitk_skin_element_info_t *info;

      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, wp->skin_element_name);
      if (info) {
        wp->font          = info->label_pixmap_font_img;
        wp->pix_font      = wp->font->pix_font;
        if (!wp->pix_font) {
          xitk_image_set_pix_font (wp->font, "");
          wp->pix_font    = wp->font->pix_font;
        }
        wp->length        = info->label_length;
        wp->animation     = info->label_animation;
        wp->anim_step     = info->label_animation_step;
        wp->anim_timer    = info->label_animation_timer;
        if (wp->anim_timer <= 0)
          wp->anim_timer = xitk_get_timer_label_animation ();
        wp->label_visible = info->label_printable;
        w->x                        = info->x;
        w->y                        = info->y;
        if (wp->pix_font) {
          w->width                  = wp->pix_font->char_width * wp->length;
          w->height                 = wp->pix_font->char_height;
        }
        w->visible                  = info->visibility ? 1 : -1;
        w->enable                   = info->enability;
        label_setup_label (w, wp->label, 1);
      }
      
      xitk_skin_unlock(skonfig);
      xitk_set_widget_pos (w, w->x, w->y);
    }
  }
}

/*
 *
 */
int xitk_label_change_label(xitk_widget_t *w, const char *newlabel) {
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *wp = (label_private_data_t *) w->private_data;
    
    if(!newlabel || !wp->label ||
       (newlabel && (strcmp(wp->label, newlabel))))
      label_setup_label (w, newlabel, 1);
    return 1;
  }

  return 0;
}

/*
 *
 */
static int notify_click_label(xitk_widget_t *w, int button, int bUp, int x, int y) {
  int  ret = 0;

  (void)x;
  (void)y;
  if (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    if(button == Button1) {
      label_private_data_t *wp = (label_private_data_t *) w->private_data;
      
      if(wp->callback) {
	if(bUp)
	  wp->callback(wp->lWidget, wp->userdata);
	ret = 1;
      }
    }
  }
  
  return ret;
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;
  
  switch(event->type) {
    case WIDGET_EVENT_PAINT:
      event->x = w->x;
      event->y = w->y;
      event->width = w->width;
      event->height = w->height;
      /* fall through */
    case WIDGET_EVENT_PARTIAL_PAINT:
      if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))) {
        label_private_data_t *wp = (label_private_data_t *) w->private_data;

        if (!pthread_mutex_trylock (&wp->change_mutex)) {
          paint_label (w, event);
          pthread_mutex_unlock (&wp->change_mutex);
        }
      }
      break;
    case WIDGET_EVENT_CLICK:
      result->value = notify_click_label (w, event->button,
        event->button_pressed, event->x, event->y);
      retval = 1;
      break;
    case WIDGET_EVENT_CHANGE_SKIN:
      notify_change_skin (w, event->skonfig);
      break;
    case WIDGET_EVENT_DESTROY:
      notify_destroy (w);
      break;
    case WIDGET_EVENT_GET_SKIN:
      if (result) {
        result->image = get_skin (w, event->skin_layer);
        retval = 1;
      }
      break;
  }
  
  return retval;
}

/*
 *
 */
static xitk_widget_t *_xitk_label_create (xitk_widget_list_t *wl, const xitk_label_widget_t *l,
  const xitk_skin_element_info_t *info) {
  xitk_widget_t          *mywidget;
  label_private_data_t   *wp;
  
  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  if (!mywidget)
    return NULL;
  
  wp = (label_private_data_t *) xitk_xmalloc(sizeof(label_private_data_t));
  if (!wp) {
    free (mywidget);
    return NULL;
  }

  wp->imlibdata = wl->imlibdata;
  wp->callback  = l->callback;
  wp->userdata  = l->userdata;
  
  wp->lWidget = mywidget;

  wp->font = info->label_pixmap_font_img;

  if (info->label_pixmap_font_name && (info->label_pixmap_font_name[0] == '\x01') && (info->label_pixmap_font_name[0] == 0)) {
    wp->skin_element_name[0] = '\x01';
    wp->skin_element_name[1] = 0;
  } else {
    _strlcpy (wp->skin_element_name, l->skin_element_name, sizeof (wp->skin_element_name));
  }
  if (!(wp->skin_element_name[0] & ~1)) {
    mywidget->height            = info->label_pixmap_font_img->height;
    mywidget->width             =
    wp->length        = info->label_pixmap_font_img->width;
    wp->fontname      = strdup (info->label_fontname);
    wp->pix_font      = NULL;
    wp->animation     = 0;
    wp->anim_step     = 1;
    wp->anim_timer    = xitk_get_timer_label_animation ();
    wp->label_visible = 1;
  } else {
    if (!wp->font) {
      /* wrong pixmmap name in skin? */
      free (wp);
      free (mywidget);
      return NULL;
    }
    wp->length        = info->label_length;
    wp->fontname      = NULL;
    wp->pix_font      = wp->font->pix_font;
    if (!wp->pix_font) {
      xitk_image_set_pix_font (wp->font, "");
      wp->pix_font    = wp->font->pix_font;
    }
    wp->animation     = info->label_animation;
    wp->anim_step     = info->label_animation_step;
    wp->anim_timer    = info->label_animation_timer;
    if (wp->anim_timer <= 0)
      wp->anim_timer = xitk_get_timer_label_animation ();
    wp->label_visible = info->label_printable;
    if (wp->pix_font) {
      mywidget->width  = wp->pix_font->char_width * wp->length;
      mywidget->height = wp->pix_font->char_height;
    }
  }
  wp->anim_running  = 0;
  wp->label         = NULL;
  wp->labelpix      = NULL;

  pthread_mutex_init (&wp->change_mutex, NULL);

  mywidget->private_data       = wp;

  mywidget->wl                 = wl;

  mywidget->enable             = info->enability;
  mywidget->visible            = info->visibility;
  mywidget->x                  = info->x;
  mywidget->y                  = info->y;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->running            = 1;

  mywidget->type               = WIDGET_TYPE_LABEL | WIDGET_CLICKABLE | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  mywidget->event              = notify_event;
  mywidget->tips_timeout       = 0;
  mywidget->tips_string        = NULL;

  label_setup_label(mywidget, l->label, 0);

  return mywidget;
}

/*
 *
 */
xitk_widget_t *xitk_label_create (xitk_widget_list_t *wl, xitk_skin_config_t *skonfig,
  const xitk_label_widget_t *l) {
  const xitk_skin_element_info_t *pinfo;
  xitk_skin_element_info_t info;

  XITK_CHECK_CONSTITENCY (l);
  pinfo = xitk_skin_get_info (skonfig, l->skin_element_name);
  if (pinfo) {
    if (!pinfo->visibility) {
      info = *pinfo;
      info.visibility = 1;
      pinfo = &info;
    }
  } else {
    memset (&info, 0, sizeof (info));
    info.visibility = -1;
    pinfo = &info;
  }

  return _xitk_label_create (wl, l, pinfo);
}

/*
 *
 */
xitk_widget_t *xitk_noskin_label_create (xitk_widget_list_t *wl,
  const xitk_label_widget_t *l, int x, int y, int width, int height,
  const char *fontname) {
  xitk_skin_element_info_t info;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_CHECK_CONSTITENCY (l);
  memset (&info, 0, sizeof (info));
  info.x                 = x;
  info.y                 = y;
  info.label_printable   = 1;
  info.label_staticity   = 0;
  info.visibility        = 0;
  info.enability         = 0;
  info.label_fontname    = (char *)fontname;
  info.label_pixmap_font_name  = (char *)"\x01";
  info.label_pixmap_font_img   = xitk_image_create_image (wl->imlibdata, width, height);
  if (info.label_pixmap_font_img)
    draw_flat (info.label_pixmap_font_img->image, width, height);

  return _xitk_label_create (wl, l, &info);
}
