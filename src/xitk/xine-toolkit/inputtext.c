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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "_xitk.h"

#define NEW_CLIPBOARD

#define NORMAL 1
#define FOCUS  2

#define DIRTY_BEFORE     1
#define DIRTY_AFTER      2
#define DIRTY_MOVE_LEFT  4
#define DIRTY_MOVE_RIGHT 8

#define SBUF_PAD 4
#define SBUF_MASK 127

static uint32_t _X_BE_32 (const uint8_t *p) {
  static const union {
    uint8_t byte;
    uint32_t word;
  } test = {
    .word = 1
  };
  uint32_t v;
  memcpy (&v, p, 4);
  if (test.byte == 1)
    v = (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8) | (v << 24);
  return v;
}

typedef struct {
  int bytes;
  int pixels;
} _inputtext_pos_t;

/* exact text width may be fractional, dont try to assemble it from text fragments.
 * xitk_font_get_text_width () may be slow, try to minimize its use by phonebook search.
 * negative values count from end of text.
 * input: pos.bytes = text length, pos.pixels = wanted pos.
 * output: pos = found pos. */
static int _inputtext_find_text_pos (inputtext_private_data_t *wp,
  const char *text, _inputtext_pos_t *pos, xitk_font_t *font, int round_up) {
  const uint8_t *btext = (const uint8_t *)text;
  xitk_font_t *fs;
  _inputtext_pos_t want, ref, try, best;
  int tries;
      
  if (!wp->text.buf)
    return 0;
  if ((pos->bytes == 0) || (pos->pixels == 0)) {
    pos->bytes = 0;
    pos->pixels = 0;
    return 1;
  }

  fs = font;
  if (!fs) {
    if (wp->fontname)
      fs = xitk_font_load_font (wp->imlibdata->x.disp, wp->fontname);
    if (!fs)
      fs = xitk_font_load_font (wp->imlibdata->x.disp, xitk_get_system_font ());
    if (!fs)
      return 0;
    xitk_font_set_font (fs, wp->iWidget->wl->gc);
  }

  tries = 12;
  ref.bytes = 0;
  ref.pixels = 0;
  best.bytes = 0;
  best.pixels = 0;
  want.pixels = pos->pixels;
  if (want.pixels < 0) {
    /* start with 200 bytes, or the entire text if less. */
    int best_diff = -pos->pixels /* + best.pixels */, last_bytes = 1;
    if (pos->bytes < 0) {
      want.bytes = pos->bytes;
    } else {
      btext += pos->bytes;
      want.bytes = -pos->bytes;
    }
    try.bytes = -200;
    try.pixels = -1000000;
    do {
      int diff;
      if (try.bytes >= 0) {
        try.bytes = 0;
      } else if (try.bytes <= want.bytes) {
        try.bytes = want.bytes;
      } else {
        while ((btext[try.bytes] & 0xc0) == 0x80)
          try.bytes += 1;
        if (try.bytes > 0)
          try.bytes = 0;
      }
      if (try.bytes == last_bytes)
        break;
      last_bytes = try.bytes;
      /* naive optimization: do it from the near side. */
      tries -= 1;
      if (try.bytes <= ref.bytes) {
        try.pixels = -xitk_font_get_text_width (fs, (char *)btext + try.bytes, -try.bytes);
        ref = try;
      } else if (try.bytes > (ref.bytes >> 1)) {
        try.pixels = -xitk_font_get_text_width (fs, (char *)btext + try.bytes, -try.bytes);
      } else {
        try.pixels = ref.pixels + xitk_font_get_text_width (fs, (char *)btext + ref.bytes, -ref.bytes + try.bytes);
      }
      diff = want.pixels - try.pixels;
      diff = diff < 0 ? -diff : diff;
      if (diff < best_diff) {
        best_diff = diff;
        best = try;
      } else if (try.bytes == best.bytes) {
        break;
      }
      diff = try.pixels;
      if (diff >= 0)
        diff = 1;
      try.bytes = (try.bytes * want.pixels + (diff >> 1)) / diff;
    } while (tries > 0);
    if (round_up && (want.pixels < best.pixels)) {
      int b = best.bytes;
      while ((btext[--b] & 0xc0) == 0x80) ;
      if (b >= want.bytes) {
        best.bytes = b;
        best.pixels = -xitk_font_get_text_width (fs, (char *)btext + best.bytes, -best.bytes);
        tries -= 1;
      }
    }
    if (pos->bytes >= 0)
      best.bytes += pos->bytes;
  } else {
    int best_diff = pos->pixels /* - best.pixels */, last_bytes = -1;
    if (pos->bytes < 0) {
      btext += pos->bytes;
      want.bytes = -pos->bytes;
    } else {
      want.bytes = pos->bytes;
    }
    try.bytes = 200;
    try.pixels = 1000000;
    do {
      int diff;
      if (try.bytes <= 0) {
        try.bytes = 0;
      } else if (try.bytes >= want.bytes) {
        try.bytes = want.bytes;
      } else {
        while ((btext[try.bytes] & 0xc0) == 0x80)
          try.bytes -= 1;
        if (try.bytes < 0)
          try.bytes = 0;
      }
      if (try.bytes == last_bytes)
        break;
      last_bytes = try.bytes;
      /* naive optimization: do it from the near side. */
      tries -= 1;
      if (try.bytes >= ref.bytes) {
        try.pixels = xitk_font_get_text_width (fs, (char *)btext, try.bytes);
        ref = try;
      } else if (try.bytes < (ref.bytes >> 1)) {
        try.pixels = xitk_font_get_text_width (fs, (char *)btext, try.bytes);
      } else {
        try.pixels = ref.pixels - xitk_font_get_text_width (fs, (char *)btext + try.bytes, ref.bytes - try.bytes);
      }
      diff = want.pixels - try.pixels;
      diff = diff < 0 ? -diff : diff;
      if (diff < best_diff) {
        best_diff = diff;
        best = try;
      } else if (try.bytes == best.bytes) {
        break;
      }
      diff = try.pixels;
      if (diff <= 0)
        diff = 1;
      try.bytes = (try.bytes * want.pixels + (diff >> 1)) / diff;
    } while (tries > 0);
    if (round_up && (want.pixels > best.pixels)) {
      int b = best.bytes;
      while (btext[b] && (btext[++b] & 0xc0) == 0x80) ;
      if (b <= want.bytes) {
        best.bytes = b;
        best.pixels = xitk_font_get_text_width (fs, (char *)btext, best.bytes);
        tries -= 1;
      }
    }
    if (pos->bytes < 0)
      best.bytes += pos->bytes;
  }

  if (!font)
    xitk_font_unload_font (fs);
#if 0
  printf ("_inputtext_find_text_pos (%d, %d, %d) = (%d, %d) after %d iterations.\n",
    pos->bytes, pos->pixels, round_up, best.bytes, best.pixels, 12 - tries);
#endif
  *pos = best;
  return 1;
}


static char *_inputtext_sbuf_set (inputtext_private_data_t *wp, int need_size) {
  do {
    int nsize = (need_size + 2 * SBUF_PAD + SBUF_MASK) & ~SBUF_MASK;
    char *nbuf;
    if (!wp->text.buf) {
      nbuf = malloc (nsize);
    } else if (wp->text.size < need_size) {
      nbuf = realloc (wp->text.buf - SBUF_PAD, nsize);
    } else {
      break;
    }
    if (!nbuf)
      return NULL;
    memset (nbuf, 0, SBUF_PAD);
    wp->text.buf = nbuf + SBUF_PAD;
    wp->text.size = nsize - 2 * SBUF_PAD;
  } while (0);
  return wp->text.buf;
}

static void _inputtext_sbuf_unset (inputtext_private_data_t *wp) {
  if (wp->text.buf) {
    free (wp->text.buf - SBUF_PAD);
    wp->text.buf = NULL;
    wp->text.size = 0;
    wp->text.used = 0;
  }
}


/*
 *
 */
static void _cursor_focus(inputtext_private_data_t *wp, Window win, int focus) {

  wp->cursor_focus = focus;
  
  if(focus)
    xitk_cursors_define_window_cursor(wp->imlibdata->x.disp, win, xitk_cursor_xterm);
  else
    xitk_cursors_restore_window_cursor(wp->imlibdata->x.disp, win);
}

/*
 *
 */
static void notify_destroy(xitk_widget_t *w) {
  inputtext_private_data_t *wp;

  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;

    if (wp->text.temp_pixmap) {
      xitk_image_destroy_xitk_pixmap (wp->text.temp_pixmap);
      wp->text.temp_pixmap = NULL;
      XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
      XFreeGC (wp->imlibdata->x.disp, wp->text.temp_gc);
      XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
      wp->text.temp_gc = None;
    }
    if(!wp->skin_element_name)
      xitk_image_free_image(&(wp->skin));
    
    XITK_FREE(wp->skin_element_name);
    _inputtext_sbuf_unset (wp);
    XITK_FREE(wp->normal_color);
    XITK_FREE(wp->focused_color);
    XITK_FREE(wp->fontname);
    XITK_FREE(wp);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  inputtext_private_data_t *wp;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;
    if(sk == BACKGROUND_SKIN && wp->skin) {
      return wp->skin;
    }
  }
  
  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *w, int x, int y) {
  inputtext_private_data_t *wp;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;
    if(w->visible == 1) {
      xitk_image_t *skin = wp->skin;
      
      if(skin->mask)
	return xitk_is_cursor_out_mask(wp->imlibdata->x.disp, w, skin->mask->pixmap, x, y);
    }
    else
      return 0;
  }

  return 1;
}

/*
 * Extract modifier keys.
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier) {
  unsigned int state = 0;

  *modifier = MODIFIER_NOMOD;

  if(!xev)
    return 0;

  if((xev->type == ButtonPress) || (xev->type == ButtonRelease))
    state = xev->xbutton.state;
  else if ((xev->type == KeyPress) || (xev->type == KeyRelease))
    state = xev->xkey.state;
  else
    state = xev->xkey.state;

  if(state & XK_Multi_key)
    state = (state | XK_Multi_key) & 0xFF;
  
  if(state & ShiftMask)
    *modifier |= MODIFIER_SHIFT;
  if(state & LockMask)
    *modifier |= MODIFIER_LOCK;
  if(state & ControlMask)
    *modifier |= MODIFIER_CTRL;
  if(state & Mod1Mask)
    *modifier |= MODIFIER_META;
  if(state & Mod2Mask)
    *modifier |= MODIFIER_NUML;
  if(state & Mod3Mask)
    *modifier |= MODIFIER_MOD3;
  if(state & Mod4Mask)
    *modifier |= MODIFIER_MOD4;
  if(state & Mod5Mask)
    *modifier |= MODIFIER_MOD5;
  
  return (*modifier != MODIFIER_NOMOD);
}

int xitk_get_keysym_and_buf(XEvent *event, KeySym *ksym, char kbuf[], int kblen) {
  int len = 0;
  if(event) {
    XKeyEvent  pkeyev = event->xkey;
    
    XLOCK (xitk_x_lock_display, pkeyev.display);
    len = XLookupString(&pkeyev, kbuf, kblen, ksym, NULL);
    XUNLOCK (xitk_x_unlock_display, pkeyev.display);
  }
  return len;
}

/*
 * Return key pressed (XK_VoidSymbol on failure)
 */
KeySym xitk_get_key_pressed(XEvent *event) {
  KeySym   pkey = XK_VoidSymbol;
  
  if(event) {
    char  buf[256];
    (void) xitk_get_keysym_and_buf(event, &pkey, buf, sizeof(buf));
  }

  return pkey;
}

/*
 * Paint the input text box.
 */
static void paint_partial_inputtext (xitk_widget_t *w, widget_event_t *event) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  XWindowAttributes         attr;
  XColor                    xcolor;
  xitk_font_t              *fs = NULL;
  int                       xsize, ysize, state = 0;
  int                       lbear, rbear, width, asc, des;
  int                       cursor_x;
  int                       yoff = 0, DefaultColor = -1;
  unsigned int              fg = 0;
  xitk_color_names_t       *color = NULL;

  if (!w)
    return;
  if ((w->type & WIDGET_TYPE_MASK) != WIDGET_TYPE_INPUTTEXT)
    return;

  if (w->visible != 1) {
    if (wp->cursor_focus)
      _cursor_focus (wp, w->wl->win, 0);
    return;
  }

#ifdef XITK_PAINT_DEBUG
  printf ("xitk.inputtext.paint (%d, %d, %d, %d).\n", event->x, event->y, event->width, event->height);
#endif
  if (w->enable && (!wp->cursor_focus)
    && (xitk_is_mouse_over_widget (wp->imlibdata->x.disp, w->wl->win, w)))
      _cursor_focus (wp, w->wl->win, 1);

  /* FIXME: what is this needed for? */
  XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
  XGetWindowAttributes (wp->imlibdata->x.disp, w->wl->win, &attr);
  XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);

  if (!wp->text.temp_pixmap) {
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    wp->text.temp_gc = XCreateGC (wp->imlibdata->x.disp, w->wl->win, None, None);
    XCopyGC (wp->imlibdata->x.disp, w->wl->gc, (1 << GCLastBit) - 1, wp->text.temp_gc);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }

  if (wp->skin->mask) {
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XSetClipOrigin (wp->imlibdata->x.disp, wp->text.temp_gc, w->x, w->y);
    XSetClipMask (wp->imlibdata->x.disp, wp->text.temp_gc, wp->skin->mask->pixmap);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }
      
  xsize = wp->skin->width / 2;
  ysize = wp->skin->height;
  if (!wp->text.temp_pixmap)
    wp->text.temp_pixmap = xitk_image_create_xitk_pixmap (wp->imlibdata, xsize + 2 * ysize, ysize);
      
  state = (w->have_focus == FOCUS_RECEIVED) || (wp->have_focus == FOCUS_MOUSE_IN) ? FOCUS : NORMAL;
      
  xcolor.flags = DoRed | DoBlue | DoGreen;

  /* Try to load font */

  if (wp->text.buf && wp->text.buf[0]) {
    if (wp->fontname)
      fs = xitk_font_load_font (wp->imlibdata->x.disp, wp->fontname);
    if (!fs) 
      fs = xitk_font_load_font (wp->imlibdata->x.disp, xitk_get_system_font ());
    if (!fs)
      XITK_DIE ("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);
    xitk_font_set_font (fs, wp->text.temp_gc);
    xitk_font_string_extent (fs, wp->text.buf, &lbear, &rbear, &width, &asc, &des);
  }
  
  if (wp->skin_element_name || (!wp->skin_element_name && (fg = xitk_get_black_color ()) == (unsigned int)-1)) {
    /*  Some colors configurations */
    switch (state) {
      case NORMAL:
        if (!strcasecmp (wp->normal_color, "Default"))
          DefaultColor = 0;
        else
          color = xitk_get_color_name (wp->normal_color);
        break;
      case FOCUS:
        if (!strcasecmp (wp->focused_color, "Default"))
          DefaultColor = 0;
        else
          color = xitk_get_color_name (wp->focused_color);
        break;
      default: ;
    }
    if (!color || (DefaultColor != -1)) {
      xcolor.red = xcolor.blue = xcolor.green = DefaultColor << 8;
    } else {
      xcolor.red = color->red << 8;
      xcolor.blue = color->blue << 8;
      xcolor.green = color->green << 8;
    }
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XAllocColor (wp->imlibdata->x.disp, Imlib_get_colormap (wp->imlibdata), &xcolor);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
    fg = xcolor.pixel;
  }

  cursor_x = 0;
  if (fs) {
    _inputtext_pos_t pos;
    cursor_x = -1;
    if (wp->text.cursor_pos < 0) {
      wp->text.draw_start = 0;
      wp->text.dirty |= DIRTY_MOVE_LEFT;
    } else if (wp->text.cursor_pos <= wp->text.draw_start) {
      if (wp->text.dirty & DIRTY_BEFORE) {
        wp->text.draw_stop = wp->text.cursor_pos;
        wp->text.dirty |= DIRTY_MOVE_RIGHT;
      } else {
        wp->text.draw_start = wp->text.cursor_pos;
        wp->text.dirty |= DIRTY_MOVE_LEFT;
      }
    } else if (wp->text.cursor_pos >= wp->text.draw_stop) {
      wp->text.draw_stop = wp->text.cursor_pos;
      wp->text.dirty |= DIRTY_MOVE_RIGHT;
    }
    if (wp->text.cursor_pos >= wp->text.used) {
      wp->text.cursor_pos =
      wp->text.draw_stop  = wp->text.used;
      wp->text.dirty |= DIRTY_MOVE_RIGHT;
    }
    do {
      if (!wp->text.dirty)
        break;
      if (!(wp->text.dirty & (DIRTY_MOVE_LEFT | DIRTY_MOVE_RIGHT))
        && (wp->text.dirty & (DIRTY_BEFORE | DIRTY_AFTER))) {
        /* keep render pos if cursor still visible.
         * pitfall: partial chars both left (inherited) and right (due to editing). */
        pos.bytes = wp->text.used - wp->text.draw_start;
        pos.pixels = wp->text.box_width - wp->text.shift;
        if (_inputtext_find_text_pos (wp, wp->text.buf + wp->text.draw_start, &pos, fs, 1)) {
          int stop;
          pos.bytes += wp->text.draw_start;
          stop = pos.bytes;
          if (pos.pixels > wp->text.box_width - wp->text.shift) {
            while ((wp->text.buf[--stop] & 0xc0) == 0x80) ;
            if (stop < 0)
              stop = 0;
          }
          if (stop >= wp->text.cursor_pos) {
            wp->text.draw_stop = pos.bytes;
            wp->text.width = pos.pixels;
            break;
          }
        }
      }
      if (wp->text.dirty & DIRTY_MOVE_RIGHT) {
        /* right align cursor, with left fallback when leading text is too short.
         * check for right part as well. */
        pos.bytes = wp->text.cursor_pos;
        pos.pixels = -wp->text.box_width;
        if (_inputtext_find_text_pos (wp, wp->text.buf, &pos, fs, 1)) {
          wp->text.draw_start = pos.bytes;
          wp->text.draw_stop = wp->text.cursor_pos;
          wp->text.width = cursor_x = -pos.pixels;
          wp->text.shift = pos.pixels + wp->text.box_width;
          if (wp->text.shift <= 0)
            break;
          wp->text.shift = 0;
          if (wp->text.cursor_pos >= wp->text.used)
            break;
          pos.bytes = wp->text.used - wp->text.draw_start;
          pos.pixels = wp->text.box_width;
          if (_inputtext_find_text_pos (wp, wp->text.buf + wp->text.draw_start, &pos, fs, 1)) {
            pos.bytes += wp->text.draw_start;
            wp->text.draw_stop = pos.bytes;
            wp->text.width = pos.pixels;
          }
          break;
        }
      }
      {
        /* left align. */
        wp->text.shift = 0;
        cursor_x = 0;
        if (wp->text.dirty & (DIRTY_MOVE_LEFT | DIRTY_AFTER)) {
          pos.bytes = wp->text.used - wp->text.draw_start;
          pos.pixels = wp->text.box_width;
          if (_inputtext_find_text_pos (wp, wp->text.buf + wp->text.draw_start, &pos, fs, 1)) {
            pos.bytes += wp->text.draw_start;
            wp->text.draw_stop = pos.bytes;
            wp->text.width = pos.pixels;
          }
        }
      }
    } while (0);
    wp->text.dirty = 0;
  }

  XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
  XSetForeground (wp->imlibdata->x.disp, wp->text.temp_gc, fg);
  XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  
  /*  Put text in the right place */
  {
    int src_x = state == FOCUS ? xsize : 0;

    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XCopyArea (wp->imlibdata->x.disp, wp->skin->image->pixmap, wp->text.temp_pixmap->pixmap, w->wl->gc,
      src_x, 0, xsize, ysize, ysize, 0);
    if (fs) {
      xitk_font_draw_string (fs, wp->text.temp_pixmap->pixmap, wp->text.temp_gc,
        ysize + wp->text.box_start + wp->text.shift,
        ((ysize + asc + des + yoff) >> 1) - des,
        wp->text.buf + wp->text.draw_start,
        wp->text.draw_stop - wp->text.draw_start);
      /* with 1 partial char left and/or right, fix borders. */
      if (wp->text.shift < 0)
        XCopyArea (wp->imlibdata->x.disp, wp->skin->image->pixmap, wp->text.temp_pixmap->pixmap, w->wl->gc,
          src_x, 0, wp->text.box_start, ysize, ysize, 0);
      if (wp->text.shift + wp->text.width > wp->text.box_width)
        XCopyArea (wp->imlibdata->x.disp, wp->skin->image->pixmap, wp->text.temp_pixmap->pixmap, w->wl->gc,
          src_x + wp->text.box_start + wp->text.box_width, 0,
          xsize - wp->text.box_start - wp->text.box_width, ysize,
          ysize + wp->text.box_start + wp->text.box_width, ysize);
    }
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }

  /* Draw cursor pointer */
  if (wp->text.cursor_pos >= 0) {
    width = cursor_x >= 0
          ? cursor_x
          : xitk_font_get_text_width (fs,
              wp->text.buf + wp->text.draw_start,
              wp->text.cursor_pos - wp->text.draw_start);
#if 0 /* ifdef WITH_XFT */
    if (width)
      width += 2;
#endif
    width += ysize + wp->text.box_start + wp->text.shift;
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XDrawLine (wp->imlibdata->x.disp, wp->text.temp_pixmap->pixmap, wp->text.temp_gc,
      width - 1,         2, width + 1, 2);
    XDrawLine (wp->imlibdata->x.disp, wp->text.temp_pixmap->pixmap, wp->text.temp_gc,
      width,             2, width,     ysize - 3);
    XDrawLine (wp->imlibdata->x.disp, wp->text.temp_pixmap->pixmap, wp->text.temp_gc,
      width - 1, ysize - 3, width,     ysize - 3);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
  }

  if (fs)
    xitk_font_unload_font (fs);
  
  if (color)
    xitk_free_color_name (color);
      
  XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
  XCopyArea (wp->imlibdata->x.disp, wp->text.temp_pixmap->pixmap, w->wl->win, wp->text.temp_gc,
    ysize + event->x - w->x, event->y - w->y, event->width, event->height, event->x, event->y);
  XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);

  if (state != FOCUS) {
    xitk_image_destroy_xitk_pixmap (wp->text.temp_pixmap);
    wp->text.temp_pixmap = NULL;
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XFreeGC (wp->imlibdata->x.disp, wp->text.temp_gc);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
    wp->text.temp_gc = None;
  }
}

static void paint_inputtext (xitk_widget_t *w) {
  widget_event_t event;

  event.x = w->x;
  event.y = w->y;
  event.width = w->width;
  event.height = w->height;
  paint_partial_inputtext (w, &event);
}

/*
 * Handle click events.
 */
static int notify_click_inputtext(xitk_widget_t *w, int button, int bUp, int x, int y) {
  (void)button;
  (void)bUp;
  (void)y;
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
    
    if(w->have_focus == FOCUS_LOST)
      w->have_focus = wp->have_focus = FOCUS_RECEIVED;
    
    if(w->enable && (!wp->cursor_focus)
       && (xitk_is_mouse_over_widget(wp->imlibdata->x.disp, w->wl->win, w)))
      _cursor_focus(wp, w->wl->win, 1);

    {
      _inputtext_pos_t pos;
      pos.bytes = wp->text.used - wp->text.draw_start;
      pos.pixels = x - w->x - wp->text.box_start - wp->text.shift;
      if (pos.pixels < 0)
        pos.pixels = 0;
      if (_inputtext_find_text_pos (wp, wp->text.buf + wp->text.draw_start, &pos, NULL, 0))
        wp->text.cursor_pos = pos.bytes + wp->text.draw_start;
    }
    
    paint_inputtext(w);
  }

  return 1;
}

/*
 * Handle motion on input text box.
 */
static int notify_focus_inputtext(xitk_widget_t *w, int focus) {
  inputtext_private_data_t *wp;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;
    
    if((wp->have_focus = focus) == FOCUS_LOST)
      wp->text.cursor_pos = -1;
  
    if((focus == FOCUS_MOUSE_OUT) || (focus == FOCUS_LOST))
      _cursor_focus(wp, w->wl->win, 0);
    else if(w->enable && (focus == FOCUS_MOUSE_IN))
      _cursor_focus(wp, w->wl->win, 1);

  }
  
  return 1;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  inputtext_private_data_t *wp;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;

    if (wp->text.temp_pixmap) {
      xitk_image_destroy_xitk_pixmap (wp->text.temp_pixmap);
      wp->text.temp_pixmap = NULL;
      XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
      XFreeGC (wp->imlibdata->x.disp, wp->text.temp_gc);
      XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
      wp->text.temp_gc = None;
    }

    if(wp->skin_element_name) {

      xitk_skin_lock(skonfig);

      XITK_FREE(wp->fontname);
      wp->fontname = strdup (xitk_skin_get_label_fontname(skonfig, wp->skin_element_name));
      
      wp->skin     = xitk_skin_get_image (skonfig, xitk_skin_get_skin_filename (skonfig, wp->skin_element_name));
      
      wp->text.box_width = wp->skin->width / 2 - 2 * 2;
      
      XITK_FREE(wp->normal_color);
      wp->normal_color  = strdup(xitk_skin_get_label_color(skonfig, wp->skin_element_name));
      XITK_FREE(wp->focused_color);
      wp->focused_color = strdup(xitk_skin_get_label_color_focus(skonfig, wp->skin_element_name));
      
      w->x        = xitk_skin_get_coord_x (skonfig, wp->skin_element_name);
      w->y        = xitk_skin_get_coord_y (skonfig, wp->skin_element_name);
      w->width    = wp->skin->width / 2;
      w->height   = wp->skin->height;
      w->visible  = (xitk_skin_get_visibility (skonfig, wp->skin_element_name)) ? 1 : -1;
      w->enable   = xitk_skin_get_enability (skonfig, wp->skin_element_name);
     
      xitk_skin_unlock(skonfig);

      xitk_set_widget_pos(w, w->x, w->y);
    }
  }
}

static int _inputtext_utf8_peek_left (const char *s, int have) {
  const uint8_t *p = (const uint8_t *)s;
  uint32_t v;
  if (have <= 0)
    return 0;
  v = _X_BE_32 (p - 4);
  if ((v & 0x0000e0c0) == 0x0000c080)
    return 2;
  if ((v & 0x00f0c0c0) == 0x00e08080)
    return 3;
  if ((v & 0xf8c0c0c0) == 0xf0808080)
    return 4;
  return 1;
}

static int _inputtext_utf8_peek_right (const char *s, int have) {
  const uint8_t *p = (const uint8_t *)s;
  uint32_t v;
  if (have <= 0)
    return 0;
  v = _X_BE_32 (p);
  if ((v & 0xe0c00000) == 0xc0800000)
    return 2;
  if ((v & 0xf0c0c000) == 0xe0808000)
    return 3;
  if ((v & 0xf8c0c0c0) == 0xf0808080)
    return 4;
  return 1;
}

/*
 * Erase one char from right of cursor.
 */
static void inputtext_erase_with_delete(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf) {
    char *p = wp->text.buf + wp->text.cursor_pos;
    int rest = wp->text.used - wp->text.cursor_pos;
    int n = _inputtext_utf8_peek_right (p, rest);
    if ((n > 0) && (n < rest))
      memmove (p, p + n, rest - n);
    wp->text.used -= n;
    wp->text.buf[wp->text.used] = 0;
    wp->text.dirty |= DIRTY_AFTER;
    paint_inputtext (w);
  }
}

/*
 * Erase one char from left of cursor.
 */
static void inputtext_erase_with_backspace(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf) {
    char *p = wp->text.buf + wp->text.cursor_pos;
    int rest = wp->text.used - wp->text.cursor_pos;
    int n = _inputtext_utf8_peek_left (p, wp->text.cursor_pos);
    if ((n > 0) && (rest > 0))
      memmove (p - n, p, rest);
    wp->text.cursor_pos -= n;
    wp->text.used -= n;
    wp->text.buf[wp->text.used] = 0;
    wp->text.dirty |= DIRTY_BEFORE;
    paint_inputtext (w);
  }
}

/*
 * Erase chars from cursor pos to EOL.
 */
static void inputtext_kill_line(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf && (wp->text.used > 1)) {
    if (wp->text.cursor_pos >= 0) {
      wp->text.used = wp->text.cursor_pos;
      wp->text.buf[wp->text.used] = 0;
      wp->text.dirty |= DIRTY_AFTER;
      paint_inputtext (w);
    }
  }
}

static void inputtext_copy (xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf && (wp->text.used > 0)) {
#ifndef NEW_CLIPBOARD
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XStoreBytes (wp->imlibdata->x.disp, wp->text.buf, wp->text.used);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
#else
    xitk_clipboard_set_text (w, wp->text.buf, wp->text.used);
#endif
  }
}

static void inputtext_cut (xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf && (wp->text.used > 0)) {
#ifndef NEW_CLIPBOARD
    XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
    XStoreBytes (wp->imlibdata->x.disp, wp->text.buf, wp->text.used);
    XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
#else
    xitk_clipboard_set_text (w, wp->text.buf, wp->text.used);
#endif
    wp->text.buf[0] = 0;
    wp->text.cursor_pos = 0;
    wp->text.used = 0;
    wp->text.dirty |= DIRTY_BEFORE | DIRTY_AFTER;
    paint_inputtext (w);
  }
}

static void inputtext_paste (xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  char *insert = NULL, *newtext;
  int olen = 0, ilen = 0, pos = 0, nlen;

  if (wp->text.buf && wp->text.buf[0])
    olen = wp->text.used;
  pos = wp->text.cursor_pos;
  if (pos < 0)
    pos = 0;
  else if (pos > olen)
    pos = olen;
  olen -= pos;
  
#ifndef NEW_CLIPBOARD
  XLOCK (wp->imlibdata->x.x_lock_display, wp->imlibdata->x.disp);
  insert = XFetchBytes (wp->imlibdata->x.disp, &ilen);
  XUNLOCK (wp->imlibdata->x.x_unlock_display, wp->imlibdata->x.disp);
#else
  insert = NULL;
  ilen = xitk_clipboard_get_text (w, &insert, wp->text.size - wp->text.used);
#endif
  if (insert) {
    int i;
    for (i = 0; (i < ilen) && (insert[i] & 0xe0); i++) ;
    ilen = i;
  } else {
    ilen = 0;
  }

  nlen = pos + ilen + olen;
  newtext = _inputtext_sbuf_set (wp, nlen);
  if (newtext) {
    if (olen > 0)
      memmove (newtext + pos + ilen, newtext + pos, olen);
    if (ilen > 0)
      memcpy (newtext + pos, insert, ilen);
    newtext[nlen] = 0;
    wp->text.cursor_pos = pos + ilen;
    wp->text.used = nlen;
    wp->text.dirty |= DIRTY_BEFORE;
    paint_inputtext (w);
  }

#ifndef NEW_CLIPBOARD
  if (insert)
    XFree (insert);
#endif
}

/*
 * Move cursor pos to left.
 */
static void inputtext_move_left(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;

  if (wp->text.buf) {
    wp->text.cursor_pos -= _inputtext_utf8_peek_left (
      wp->text.buf + wp->text.cursor_pos, wp->text.cursor_pos);
    paint_inputtext (w);
  }
}

/*
 * Move cursor pos to right.
 */
static void inputtext_move_right(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;

  if (wp->text.buf) {
    wp->text.cursor_pos += _inputtext_utf8_peek_right (
      wp->text.buf + wp->text.cursor_pos, wp->text.used - wp->text.cursor_pos);
    paint_inputtext (w);
  }
}

/*
 * Remove focus of widget, then call callback function.
 */
static void inputtext_exec_return(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;

  wp->text.cursor_pos   = -1;
  w->have_focus              = 
    wp->have_focus = FOCUS_LOST;
  //  wl->widget_focused = NULL;
  _cursor_focus(wp, w->wl->win, 0);
  paint_inputtext(w);
  
  if(wp->text.buf && (strlen(wp->text.buf) > 0)) {
    if(wp->callback)
      wp->callback(w, wp->userdata, wp->text.buf);
  }
}

/*
 * Remove focus of widget.
 */
static void inputtext_exec_escape(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  wp->text.cursor_pos = -1;
  w->have_focus = wp->have_focus = FOCUS_LOST;
  w->wl->widget_focused = NULL;
  _cursor_focus(wp, w->wl->win, 0);
  paint_inputtext(w);
}

/*
 *
 */
static void inputtext_move_bol(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;

  if(wp->text.buf) {
    wp->text.cursor_pos = 0;
    paint_inputtext(w);
  }
}

/*
 *
 */
static void inputtext_move_eol(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;

  if (wp->text.buf) {
    wp->text.cursor_pos = wp->text.used;
    paint_inputtext (w);
  }
}

/*
 * Transpose two characters.
 */
static void inputtext_transpose_chars(xitk_widget_t *w) {
  inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
  
  if (wp->text.buf) {
    char *p = wp->text.buf + wp->text.cursor_pos;
    int b = _inputtext_utf8_peek_left (p, wp->text.cursor_pos);
    int a = _inputtext_utf8_peek_left (p - b, wp->text.cursor_pos - b);
    if (a && b) {
      char temp[8];
      int i;
      p -= a + b;
      for (i = 0; i < a + b; i++)
        temp[i] = p[i];
      for (i = 0; i < a; i++)
        p[b + i] = temp[i];
      for (i = 0; i < b; i++)
        p[i] = temp[a + i];
    }
    wp->text.dirty |= DIRTY_BEFORE;
    paint_inputtext (w);
  }
}

/*
 * Handle keyboard event in input text box.
 */
static void notify_keyevent_inputtext(xitk_widget_t *w, XEvent *xev) {
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
    KeySym      key;
    char        buf[256];
    int         len;
    int         modifier;
    
    wp->have_focus = FOCUS_RECEIVED;

    len = xitk_get_keysym_and_buf(xev, &key, buf, sizeof(buf));
    buf[len] = '\0';

    (void) xitk_get_key_modifier(xev, &modifier);

    /* One of modifier key is pressed, none of keylock or shift */
    if (buf[0] && !buf[1] && (modifier & MODIFIER_CTRL)) {
      switch (key) {
        /* Beginning of line */
        case XK_a:
        case XK_A:
          inputtext_move_bol (w);
          break;
        /* Backward */
        case XK_b:
        case XK_B:
          inputtext_move_left (w);
          break;
        /* Copy */
        case XK_c:
        case XK_C:
          inputtext_copy (w);
          break;
        /* Delete current char */
        case XK_d:
        case XK_D:
          inputtext_erase_with_delete (w);
          break;
        /* End of line */
        case XK_e:
        case XK_E:
          inputtext_move_eol (w);
          break;
        /* Forward */
        case XK_f:
        case XK_F:
          inputtext_move_right (w);
          break;
        /* Kill line (from cursor) */
        case XK_k:
        case XK_K:
          inputtext_kill_line (w);
          break;
        /* Return */
        case XK_m:
        case XK_M:
          inputtext_exec_return (w);
          break;
        /* Transpose chars */
        case XK_t:
        case XK_T:
          inputtext_transpose_chars (w);
          break;
        /* Paste */
        case XK_v:
        case XK_V:
          inputtext_paste (w);
          break;
        /* Cut */
        case XK_x:
        case XK_X:
          inputtext_cut (w);
          break;
        case XK_question:
          inputtext_erase_with_backspace (w);
          break;
        default: ;
      }
    } else if (modifier & (MODIFIER_CTRL | MODIFIER_META)) {
      ;
    } else {
      switch (key) {
        case XK_Tab:
          return;
        case XK_Delete:
          inputtext_erase_with_delete (w);
          break;
        case XK_BackSpace:
          inputtext_erase_with_backspace (w);
          break;
        case XK_Left:
          inputtext_move_left (w);
          break;
        case XK_Right:
          inputtext_move_right (w);
          break;
        case XK_Home:
          inputtext_move_bol (w);
          break;
        case XK_End:
          inputtext_move_eol (w);
          break;
        case XK_Return:
        case XK_KP_Enter:
          inputtext_exec_return (w);
          break;
        case XK_Escape:
/*      case XK_Tab: */
          inputtext_exec_escape (w);
          break;
        default:
          if (buf[0] && (wp->text.used < wp->max_length)) {
            char *newtext = _inputtext_sbuf_set (wp, wp->text.used + len);

            if (newtext) {
              int pos = wp->text.cursor_pos, rest;

              if (pos > wp->text.used)
                pos = wp->text.used;
              rest = wp->text.used - pos;
              if (rest > 0)
                memmove (newtext + pos + len, newtext + pos, rest);
              for (rest = 0; rest < len; rest++)
                newtext[pos + rest] = buf[rest];
              wp->text.used += len;
              newtext[wp->text.used] = 0;
              wp->text.cursor_pos = pos + len;
              wp->text.dirty |= DIRTY_BEFORE;
              paint_inputtext (w);
            }
          }
      }
    }
  }
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;

  switch(event->type) {
  case WIDGET_EVENT_PARTIAL_PAINT:
    paint_partial_inputtext (w, event);
    break;
  case WIDGET_EVENT_PAINT:
    paint_inputtext (w);
    break;
  case WIDGET_EVENT_CLICK:
    result->value = notify_click_inputtext(w, event->button,
					   event->button_pressed, event->x, event->y);
    retval = 1;
    break;
  case WIDGET_EVENT_FOCUS:
    notify_focus_inputtext(w, event->focus);
    break;
  case WIDGET_EVENT_KEY_EVENT:
    notify_keyevent_inputtext(w, event->xevent);
    break;
  case WIDGET_EVENT_INSIDE:
    result->value = notify_inside(w, event->x, event->y);
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
  case WIDGET_EVENT_CLIP_READY:
    inputtext_paste (w);
    break;
  }
  
  return retval;
}

/*
 * Return the text of widget.
 */
char *xitk_inputtext_get_text(xitk_widget_t *w) {
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    inputtext_private_data_t *wp = (inputtext_private_data_t *) w->private_data;
    
    return wp->text.buf ? wp->text.buf : "";
  }
  return NULL;
}

static void _xitk_inputtext_set_text (inputtext_private_data_t *wp, const char *text) {
  wp->text.size = 0;
  wp->text.used = 0;
  wp->text.dirty |= DIRTY_AFTER;
  if (text) {
    int nlen = strlen (text);
    char *nbuf = _inputtext_sbuf_set (wp, nlen);
    if (nbuf) {
      memcpy (nbuf, text, nlen + 1);
      wp->text.used = nlen;
    }
  }
  wp->text.draw_start = 0;
  wp->text.draw_stop = 0;
  wp->text.cursor_pos = -1;
}

/*
 * Change and redisplay the text of widget.
 */
void xitk_inputtext_change_text(xitk_widget_t *w, const char *text) {
  inputtext_private_data_t *wp;
  
  if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)) {
    wp = (inputtext_private_data_t *) w->private_data;

    _inputtext_sbuf_unset (wp);
    _xitk_inputtext_set_text (wp, text);
    paint_inputtext (w);
  }
}

/*
 * Create input text box
 */
static xitk_widget_t *_xitk_inputtext_create (xitk_widget_list_t *wl,
					      xitk_skin_config_t *skonfig, 
					      xitk_inputtext_widget_t *it,
                                              int x, int y, const char *skin_element_name,
					      xitk_image_t *skin,
                                              const char *fontname,
                                              const char *ncolor, const char *fcolor,
					      int visible, int enable) {
  xitk_widget_t             *mywidget;
  inputtext_private_data_t  *wp;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  (void)skonfig;
  mywidget              = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  
  wp                    = (inputtext_private_data_t *) xitk_xmalloc(sizeof(inputtext_private_data_t));
  
  wp->imlibdata         = wl->imlibdata;
  wp->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(it->skin_element_name);

  wp->iWidget           = mywidget;

  _xitk_inputtext_set_text (wp, it->text);
    
  wp->fontname          = strdup(fontname);
 
  wp->max_length        = it->max_length;

  wp->skin              = skin;

  wp->text.box_width    = wp->skin->width / 2 - 2 * 2;
  wp->text.box_start    = 2;
  if (!wp->skin_element_name) {
    wp->text.box_width -= 2 * 1;
    wp->text.box_start += 1;
  }
  wp->text.temp_pixmap  = NULL;
  wp->text.temp_gc      = None;

  wp->cursor_focus      = 0;

  wp->callback          = it->callback;
  wp->userdata          = it->userdata;
  
  wp->normal_color      = strdup(ncolor);
  wp->focused_color     = strdup(fcolor);

  mywidget->private_data          = wp;

  mywidget->wl                    = wl;

  mywidget->enable                = enable;
  mywidget->running               = 1;
  mywidget->visible               = visible;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = wp->skin->width/2;
  mywidget->height                = wp->skin->height;
  mywidget->type                  = WIDGET_TYPE_INPUTTEXT | WIDGET_FOCUSABLE
                                  | WIDGET_CLICKABLE | WIDGET_KEYABLE | WIDGET_PARTIAL_PAINTABLE;
  mywidget->event                 = notify_event;
  mywidget->tips_timeout          = 0;
  mywidget->tips_string           = NULL;

  return mywidget;
}

xitk_widget_t *xitk_inputtext_create (xitk_widget_list_t *wl,
				      xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it) {

  XITK_CHECK_CONSTITENCY(it);

  return _xitk_inputtext_create (wl, skonfig, it,
				 (xitk_skin_get_coord_x(skonfig, it->skin_element_name)),
				 (xitk_skin_get_coord_y(skonfig, it->skin_element_name)),
				 it->skin_element_name,
				 (xitk_skin_get_image(skonfig,
						      xitk_skin_get_skin_filename(skonfig, it->skin_element_name))),
				 (xitk_skin_get_label_fontname(skonfig, it->skin_element_name)),
				 (xitk_skin_get_label_color(skonfig, it->skin_element_name)),
				 (xitk_skin_get_label_color_focus(skonfig, it->skin_element_name)),
				 ((xitk_skin_get_visibility(skonfig, it->skin_element_name)) ? 1 : -1),
				 (xitk_skin_get_enability(skonfig, it->skin_element_name)));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_inputtext_create (xitk_widget_list_t *wl,
					     xitk_inputtext_widget_t *it,
					     int x, int y, int width, int height,
                                             const char *ncolor, const char *fcolor, const char *fontname) {
  xitk_image_t *i;

  ABORT_IF_NULL(wl);
  ABORT_IF_NULL(wl->imlibdata);

  XITK_CHECK_CONSTITENCY(it);
  i = xitk_image_create_image(wl->imlibdata, width * 2, height);
  draw_bevel_two_state(wl->imlibdata, i);
  
  return _xitk_inputtext_create(wl, NULL, it, x, y, NULL, i, fontname, ncolor, fcolor, 0, 0);
}
