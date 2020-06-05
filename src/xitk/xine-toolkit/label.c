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
  label_private_data_t  *private_data = (label_private_data_t *) w->private_data;
  xitk_image_t          *font         = (xitk_image_t *) private_data->font;
  xitk_pix_font_t       *pix_font;
  int                    pixwidth;
  int                    x_dest;
  int                    len, anim_add = 0;
  uint16_t               buf[2048];
  
  private_data->anim_offset = 0;

  if (!font->pix_font) {
    /* old style */
    xitk_image_set_pix_font (font, "");
    if (!font->pix_font)
      return;
  }
  pix_font = font->pix_font;

  {
    const uint8_t *p = (const uint8_t *)private_data->label;
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
    private_data->label_len = len = q - buf;
  }

  if (private_data->animation && (len > private_data->length))
    anim_add = private_data->length + 5;

  /* reuse or reallocate pixmap */
  pixwidth = len + anim_add;
  if (pixwidth < private_data->length)
    pixwidth = private_data->length;
  if (pixwidth < 1)
    pixwidth = 1;
  pixwidth *= pix_font->char_width;
  if (private_data->labelpix) {
    if ((private_data->labelpix->width != pixwidth)
      || (private_data->labelpix->height != pix_font->char_height)) {
      xitk_image_destroy_xitk_pixmap (private_data->labelpix);
      private_data->labelpix = NULL;
    }
  }
  if (!private_data->labelpix) {
    private_data->labelpix = xitk_image_create_xitk_pixmap (private_data->imlibdata, pixwidth,
      pix_font->char_height);
#if 0
    printf ("xine.label: new pixmap %d:%d -> %d:%d for \"%s\"\n", pixwidth, pix_font->char_height,
      private_data->labelpix->width, private_data->labelpix->height, private_data->label);
#endif
  }
  x_dest = 0;

  /* [foo] */
  {
    const uint16_t *p = buf;
    XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
    while (*p) {
      xitk_point_t pt;
      _find_pix_font_char (font->pix_font, &pt, *p);
      p++;
      XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
        private_data->labelpix->pixmap, private_data->labelpix->gc, pt.x, pt.y,
        pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    }
    XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
  }
  /* foo[ *** fo] */
  if (anim_add) {
    XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
    XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc,
      pix_font->asterisk.x, pix_font->asterisk.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc,
      pix_font->space.x, pix_font->space.y,
      pix_font->char_width, pix_font->char_height, x_dest, 0);
    x_dest += pix_font->char_width;
    XCopyArea (private_data->imlibdata->x.disp, private_data->labelpix->pixmap,
      private_data->labelpix->pixmap, private_data->labelpix->gc, 0, 0,
      pixwidth - x_dest, pix_font->char_height, x_dest, 0);
    XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
    x_dest = pixwidth;
  }
  
  /* fill gap with spaces */
  if (x_dest < pixwidth) {
    XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
    do {
      XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap,
        private_data->labelpix->pixmap, private_data->labelpix->gc,
        pix_font->space.x, pix_font->space.y,
        pix_font->char_width, pix_font->char_height, x_dest, 0);
      x_dest += pix_font->char_width;
    } while (x_dest < pixwidth);
    XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
  }
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *private_data = (label_private_data_t *) w->private_data;

    pthread_mutex_lock (&private_data->change_mutex);
    
    if(private_data->anim_running) {
      void *dummy;
      
      private_data->anim_running = 0;
      pthread_mutex_unlock (&private_data->change_mutex);
      pthread_join (private_data->anim_thread, &dummy);
      pthread_mutex_lock (&private_data->change_mutex);
    }
    
    if (private_data->labelpix) {
      xitk_image_destroy_xitk_pixmap(private_data->labelpix);
      private_data->labelpix = NULL;
    }
    
    if (!(private_data->skin_element_name[0] & ~1))
      xitk_image_free_image(&(private_data->font));

    XITK_FREE(private_data->label);
    XITK_FREE(private_data->fontname);

    pthread_mutex_unlock (&private_data->change_mutex);
    pthread_mutex_destroy (&private_data->change_mutex);

    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *private_data = (label_private_data_t *) w->private_data;

    if(sk == FOREGROUND_SKIN && private_data->font) {
      return private_data->font;
    }
  }

  return NULL;
}

/*
 *
 */
const char *xitk_label_get_label(xitk_widget_t *w) {

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
      label_private_data_t *private_data = (label_private_data_t *) w->private_data;
    return private_data->label;
  }

  return NULL;
}

/*
 *
 */
static void paint_label(xitk_widget_t *w) {

  if (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL) && w->visible == 1)) {
    label_private_data_t  *private_data = (label_private_data_t *) w->private_data;
    xitk_image_t          *font = (xitk_image_t *) private_data->font;
    
    if(!private_data->label_visible)
      return;
    
    /* non skinable widget */
    if (!(private_data->skin_element_name[0] & ~1)) {
      xitk_font_t   *fs = NULL;
      int            lbear, rbear, wid, asc, des;
      xitk_image_t  *bg;

      /* Clean old */
      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap, w->wl->win, font->image->gc, 
		 0, 0, private_data->font->width, font->height, w->x, w->y);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

      fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
      xitk_font_set_font(fs, private_data->font->image->gc);
      xitk_font_string_extent (fs, private_data->label, &lbear, &rbear, &wid, &asc, &des);

      bg = xitk_image_create_image(private_data->imlibdata, w->width, w->height);

      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp, font->image->pixmap, bg->image->pixmap, 
		 font->image->gc,
		 0, 0, private_data->font->width, private_data->font->height, 0, 0);
      XSetForeground(private_data->imlibdata->x.disp, font->image->gc, 
		     xitk_get_pixel_color_black(private_data->imlibdata));
      xitk_font_draw_string(fs, bg->image->pixmap, font->image->gc,
		  2, ((private_data->font->height + asc + des)>>1) - des,
		  private_data->label, private_data->label_len);
      XCopyArea (private_data->imlibdata->x.disp, bg->image->pixmap, w->wl->win, 
		 font->image->gc, 0, 0, font->width, font->height, w->x, w->y);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

      xitk_image_free_image(&bg);
      
      xitk_font_unload_font(fs);
      return;
    }
    else if (private_data->pix_font) {
      int width = private_data->pix_font->char_width * private_data->length;
      
      XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
      XCopyArea (private_data->imlibdata->x.disp,
        private_data->labelpix->pixmap, w->wl->win, font->image->gc,
        private_data->anim_offset, 0, width, private_data->pix_font->char_height, w->x, w->y);

      if (private_data->anim_running)
        XSync (private_data->imlibdata->x.disp, False);
      XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
      
    }
  }
}

/*
 *
 */
static void *xitk_label_animation_loop (void *data) {
  label_private_data_t *private_data = (label_private_data_t *)data;

  pthread_mutex_lock (&private_data->change_mutex);
  
  while (1) {
    xitk_widget_t *w = private_data->lWidget;
    unsigned long t_anim;

    if (!w->running || (private_data->anim_running != 1))
      break;
    if ((w->visible == 1) && private_data->pix_font) {
      private_data->anim_offset += private_data->anim_step;
      if (private_data->anim_offset >= (private_data->pix_font->char_width * ((int)private_data->label_len + 5)))
        private_data->anim_offset = 0;
      paint_label (private_data->lWidget);
    }
    t_anim = private_data->anim_timer;
    pthread_mutex_unlock (&private_data->change_mutex);
    xitk_usec_sleep (t_anim);
    pthread_mutex_lock (&private_data->change_mutex);
  }
  private_data->anim_running = 0;
  pthread_mutex_unlock (&private_data->change_mutex);
  
  return NULL;
}

/*
 *
 */
static void label_setup_label(xitk_widget_t *w, const char *new_label, int paint) {
  label_private_data_t *private_data = (label_private_data_t *) w->private_data;
  
  /* Inform animation thread to not paint the label */
  pthread_mutex_lock (&private_data->change_mutex);
  
  if (new_label != private_data->label) {
    size_t new_len;

    if (!new_label) {
      new_label = "";
      new_len = 0;
    } else {
      new_len = strlen (new_label);
    }
    if (private_data->label) {
      if (private_data->label_len != new_len) {
        free (private_data->label);
        private_data->label = NULL;
      }
    }
    if (!private_data->label)
      private_data->label = xitk_xmalloc (new_len + 1);
    if (private_data->label)
      memcpy (private_data->label, new_label, new_len + 1);
    private_data->label_len = new_len;
  }

  if (private_data->skin_element_name[0] & ~1) {
    _create_label_pixmap(w);
  } else {
    if (private_data->labelpix) {
      xitk_image_destroy_xitk_pixmap (private_data->labelpix);
      private_data->labelpix = NULL;
    }
    private_data->anim_offset = 0;
  }

  if (private_data->animation && ((int)private_data->label_len > private_data->length)) {
    if (!private_data->anim_running) {
      pthread_attr_t       pth_attrs;
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
      struct sched_param   pth_params;
#endif
      int r;
      
      private_data->anim_running = 1;
      pthread_attr_init (&pth_attrs);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && (_POSIX_THREAD_PRIORITY_SCHEDULING > 0)
      pthread_attr_getschedparam(&pth_attrs, &pth_params);
      pth_params.sched_priority = sched_get_priority_min(SCHED_OTHER);
      pthread_attr_setschedparam(&pth_attrs, &pth_params);
#endif
      r = pthread_create (&private_data->anim_thread, &pth_attrs, xitk_label_animation_loop, (void *)private_data);
      pthread_attr_destroy (&pth_attrs);
      if (r)
        private_data->anim_running = 0;
    } else {
      private_data->anim_running = 1;
    }
  } else {
    if (private_data->anim_running) {
      /* panel mrl animation has a long interval of 0.5s.
       * better not wait for that, as it may interfere with
       * gapleess stream switch. */
      private_data->anim_running = 2;
    }
  }
  if (paint)
    paint_label (w);
  pthread_mutex_unlock (&private_data->change_mutex);
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {
    label_private_data_t *private_data = (label_private_data_t *) w->private_data;
    
    if (private_data->skin_element_name[0] & ~1) {
      const xitk_skin_element_info_t *info;

      xitk_skin_lock(skonfig);
      info = xitk_skin_get_info (skonfig, private_data->skin_element_name);
      if (info) {
        private_data->font          = info->label_pixmap_font_img;
        private_data->pix_font      = private_data->font->pix_font;
        if (!private_data->pix_font) {
          xitk_image_set_pix_font (private_data->font, "");
          private_data->pix_font    = private_data->font->pix_font;
        }
        private_data->length        = info->label_length;
        private_data->animation     = info->label_animation;
        private_data->anim_step     = info->label_animation_step;
        private_data->anim_timer    = info->label_animation_timer;
        if (private_data->anim_timer <= 0)
          private_data->anim_timer = xitk_get_timer_label_animation ();
        private_data->label_visible = info->label_printable;
        w->x                        = info->x;
        w->y                        = info->y;
        if (private_data->pix_font) {
          w->width                  = private_data->pix_font->char_width * private_data->length;
          w->height                 = private_data->pix_font->char_height;
        }
        w->visible                  = info->visibility ? 1 : -1;
        w->enable                   = info->enability;
        label_setup_label (w, private_data->label, 1);
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
    label_private_data_t *private_data = (label_private_data_t *) w->private_data;
    
    if(!newlabel || !private_data->label ||
       (newlabel && (strcmp(private_data->label, newlabel))))
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
      label_private_data_t *private_data = (label_private_data_t *) w->private_data;
      
      if(private_data->callback) {
	if(bUp)
	  private_data->callback(private_data->lWidget, private_data->userdata);
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
    {
      if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL))) {
	label_private_data_t  *private_data = (label_private_data_t *) w->private_data;

	if(!pthread_mutex_trylock(&private_data->change_mutex)) {
	  paint_label(w);
	  pthread_mutex_unlock(&private_data->change_mutex);
	}
	
      }
    }
    break;
  case WIDGET_EVENT_CLICK:
    result->value = notify_click_label(w, event->button, 
				       event->button_pressed, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    notify_change_skin(w, event->skonfig);
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_GET_SKIN:
    if(result) {
      result->image = get_skin(w, event->skin_layer);
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
  label_private_data_t   *private_data;
  
  mywidget = (xitk_widget_t *) xitk_xmalloc(sizeof(xitk_widget_t));
  if (!mywidget)
    return NULL;
  
  private_data = (label_private_data_t *) xitk_xmalloc(sizeof(label_private_data_t));
  if (!private_data) {
    free (mywidget);
    return NULL;
  }

  private_data->imlibdata = l->imlibdata;
  private_data->callback  = l->callback;
  private_data->userdata  = l->userdata;
  
  private_data->lWidget = mywidget;

  private_data->font = info->label_pixmap_font_img;

  if (info->label_pixmap_font_name && (info->label_pixmap_font_name[0] == '\x01') && (info->label_pixmap_font_name[0] == 0)) {
    private_data->skin_element_name[0] = '\x01';
    private_data->skin_element_name[1] = 0;
  } else {
    _strlcpy (private_data->skin_element_name, l->skin_element_name, sizeof (private_data->skin_element_name));
  }
  if (!(private_data->skin_element_name[0] & ~1)) {
    mywidget->height            = info->label_pixmap_font_img->height;
    mywidget->width             =
    private_data->length        = info->label_pixmap_font_img->width;
    private_data->fontname      = strdup (info->label_fontname);
    private_data->pix_font      = NULL;
    private_data->animation     = 0;
    private_data->anim_step     = 1;
    private_data->anim_timer    = xitk_get_timer_label_animation ();
    private_data->label_visible = 1;
  } else {
    if (!private_data->font) {
      /* wrong pixmmap name in skin? */
      free (private_data);
      free (mywidget);
      return NULL;
    }
    private_data->length        = info->label_length;
    private_data->fontname      = NULL;
    private_data->pix_font      = private_data->font->pix_font;
    if (!private_data->pix_font) {
      xitk_image_set_pix_font (private_data->font, "");
      private_data->pix_font    = private_data->font->pix_font;
    }
    private_data->animation     = info->label_animation;
    private_data->anim_step     = info->label_animation_step;
    private_data->anim_timer    = info->label_animation_timer;
    if (private_data->anim_timer <= 0)
      private_data->anim_timer = xitk_get_timer_label_animation ();
    private_data->label_visible = info->label_printable;
    if (private_data->pix_font) {
      mywidget->width           = private_data->pix_font->char_width * private_data->length;
      mywidget->height          = private_data->pix_font->char_height;
    }
  }
  private_data->anim_running  = 0;
  private_data->label         = NULL;
  private_data->labelpix      = NULL;

  pthread_mutex_init (&private_data->change_mutex, NULL);

  mywidget->private_data       = private_data;

  mywidget->wl                 = wl;

  mywidget->enable             = info->enability;
  mywidget->visible            = info->visibility;
  mywidget->imlibdata          = private_data->imlibdata;
  mywidget->x                  = info->x;
  mywidget->y                  = info->y;
  mywidget->have_focus         = FOCUS_LOST;
  mywidget->running            = 1;

  mywidget->type               = WIDGET_TYPE_LABEL | WIDGET_CLICKABLE | WIDGET_KEYABLE;
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
  info.label_pixmap_font_img   = xitk_image_create_image (l->imlibdata, width, height);
  if (info.label_pixmap_font_img)
    draw_flat (l->imlibdata, info.label_pixmap_font_img->image, width, height);

  return _xitk_label_create (wl, l, &info);
}

