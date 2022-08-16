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
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <xine/sorted_array.h>

#include "common.h"
#include "mediamark.h"
#include "download.h"
#include "file_browser.h"
#include "panel.h"
#include "playlist.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/intbox.h"

#define WINDOW_WIDTH            525
#define WINDOW_HEIGHT           270

struct xui_mmkedit_s {
  gGui_t                       *gui;

  apply_callback_t              callback;
  void                         *user_data;

  xitk_window_t                *xwin;

  xitk_widget_list_t           *widget_list;

  xitk_widget_t                *mrl;
  xitk_widget_t                *ident;
  xitk_widget_t                *sub;
  xitk_widget_t                *start;
  xitk_widget_t                *end;
  xitk_widget_t                *av_offset;
  xitk_widget_t                *spu_offset;

  mediamark_t                 **mmk;

  int                           visible;
  xitk_register_key_t           widget_key;
};

/** tools ******************************************************************************/

static inline uint32_t _find_byte (const char *s, uint32_t byte) {
  const uint32_t eor = ~((byte << 24) | (byte << 16) | (byte << 8) | byte);
  const uint32_t left = (uintptr_t)s & 3;
  const uint32_t *p = (const uint32_t *)(s - left);
  static const union {
    uint8_t b[4];
    uint32_t v;
  } mask[4] = {
    {{0xff, 0xff, 0xff, 0xff}},
    {{0x00, 0xff, 0xff, 0xff}},
    {{0x00, 0x00, 0xff, 0xff}},
    {{0x00, 0x00, 0x00, 0xff}},
  };
  static const uint8_t rest[32] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, /* big wndian */
    0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4  /* little endian */
  };
  const union {
    uint32_t v;
    uint8_t b[4];
  } endian = {16};
  uint32_t w = (*p++ ^ eor) & mask[left].v;
  while (1) {
    w = w & 0x80808080 & ((w & 0x7f7f7f7f) + 0x01010101);
    if (w)
      break;
    w = *p++ ^ eor;
  }
  /* bits 31, 23, 15, 7 -> 3, 2, 1, 0 */
  w = (w * 0x00204081) & 0xffffffff;
  w >>= 28;
  return ((const char *)p - s) - rest[endian.b[0] + w];
}

/** mrl_buf handling *******************************************************************/

static const uint8_t tab_unhex[256] = {
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,16,16,16,16,16,16,
  16,10,11,12,13,14,15,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,10,11,12,13,14,15,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16
};

static size_t _gui_string_unescape (char *_s, size_t len) {
  uint8_t *s = (uint8_t *)_s, *e = s + len, *d, save[2];

  memcpy (save, e, 2);
  memset (e, '%', 2);
  s += _find_byte ((char *)s, '%');
  d = s;
  while (s < e) {
    uint8_t a = tab_unhex[s[1]], b = tab_unhex[s[2]];

    if (!((a | b) & 16)) {
      s += 3;
      *d++ = (a << 4) + b;
    } else {
      s += (s[1] == '%') ? 2 : 1;
      *d++ = '%';
    }
    while (*s != '%')
      *d++ = *s++;
  }
  memcpy (e, save, 2);
  return d - (uint8_t *)_s;
}

void mrl_buf_init (mrl_buf_t *mrlb) {
  mrlb->start = mrlb->protend = mrlb->host = mrlb->root = mrlb->lastpart =
  mrlb->ext = mrlb->args = mrlb->info = mrlb->end = mrlb->buf + 8;
  mrlb->max = mrlb->buf + sizeof (mrlb->buf) - 8;
  memset (mrlb->buf, 0, 16);
}

static void _mrl_buf_working_dir (mrl_buf_t *mrlb) {
  if (getcwd (mrlb->start, mrlb->max - mrlb->start)) {
    mrlb->args = mrlb->start + _find_byte (mrlb->start, 0);
    mrlb->start[-1] = '/';
    if (mrlb->args[-1] != '/') {
      memcpy (mrlb->args, "/", 2);
      mrlb->args++;
    }
    mrlb->protend = mrlb->host = mrlb->root = mrlb->start;
    mrlb->lastpart = mrlb->ext = mrlb->info = mrlb->end = mrlb->args;
  } else {
    mrl_buf_init (mrlb);
  }
}

static int _mrl_buf_mkdir_p (mrl_buf_t *path) {
  struct stat sbuf;
  char *p, save;

  save = path->lastpart[0];
  path->lastpart[0] = '/';
  p = path->root;
  if (p[0] == '/')
    p++;
  while (p < path->lastpart) {
    p += _find_byte (p, '/');
    p[0] = 0;
    if (!stat (path->root, &sbuf)) {
      if (!S_ISDIR (sbuf.st_mode)) {
        p[0] = '/';
        break;
      }
    } else if (mkdir (path->root, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
      p[0] = '/';
      break;
    }
    p[0] = '/';
    p++;
  }
  path->lastpart[0] = save;
  return p >= path->lastpart;
}

static size_t _mrl_buf_resolve_dots (char *s, size_t len) {
  uint8_t *start = (uint8_t *)s, *p, *t, *e = start + len, save1[2], save2[2];
  /* set safe plugs */
  memcpy (save1, start - 2, 2);
  memcpy (save2, e, 2);
  memcpy (start - 2, "//", 2);
  memcpy (e, "//", 2);
  /* check only part 1 */
  for (t = p = start; p < e;) {
    uint8_t *here = p;

    for (p++; p[0] != '/'; p++) ;
    if (here[1] == '.') {
      if (here[2] == '/')
        break;
      if (!memcmp (here + 2, "./", 2)) {
        for (t = here - 1; t[0] != '/'; t--) ;
        if (t < start)
          t = start;
        break;
      }
    }
    t = p;
  }
  /* move part 2 */
  if (t < p) {
    uint8_t *f;
    for (f = p; p < e;) {
      uint8_t *here = p;

      for (p++; p[0] != '/'; p++) ;
      if (here[1] == '.') {
        if (here[2] == '/') {
          if (here > f) {
            memmove (t, f, here - f);
            t += here - f;
            f = p;
          }
        } else if (!memcmp (here + 2, "./", 2)) {
          if (here > f) {
            memmove (t, f, here - f);
            t += here - f;
            f = p;
          }
          for (t--; t[0] != '/'; t--) ;
          if (t < start)
            t = start;
        }
      }
    }
    if (p > f) {
      memmove (t, f, p - f);
      t += p - f;
    }
  }
  /* remove plugs */
  memcpy (start - 2, save1, 2);
  memcpy (e, save2, 2);
  return t - start;
}

int mrl_buf_set (mrl_buf_t *mrlb, mrl_buf_t *base, const char *name) {
  size_t size;
  char prot[16], save;
  char *scan_args;
  uint8_t *p;
  uint32_t plen;

  if (!name)
    return 0;
  if (!name[0])
    return 0;

  mrlb->start = mrlb->buf + 8;
  size = _find_byte (name, 0);
  if ((int)size > mrlb->max - mrlb->start)
    size = mrlb->max - mrlb->start;
  memcpy (mrlb->start, name, size + 1);
  mrlb->end = mrlb->start + size;

  plen = mrl_get_lowercase_prot (prot, sizeof (prot), mrlb->start);
  do {
    /* protocol */
    mrlb->protend = mrlb->start + plen;
    if ((plen == 4) && !memcmp (prot, "file", 4)) {
      p = (uint8_t *)mrlb->protend + 2;
      if (!memcmp (p, "//", 2))
        p++;
      mrlb->start = mrlb->protend = mrlb->host = mrlb->root = (char *)p;
      break;
    }
    if (!plen && memcmp (mrlb->protend, ":/", 2)) {
      /* plain file or relative path */
      mrlb->host = mrlb->root = mrlb->protend;
      break;
    }
    /* host */
    p = (uint8_t *)mrlb->protend + 2;
    for (; p[0] == '/'; p++) ;
    mrlb->host = (char *)p;
    /* root */
    mrlb->end[0] = '/';
    for (; p[0] != '/'; p++) ;
    mrlb->end[0] = 0;
    mrlb->root = (char *)p;
  } while (0);
  /* most filesystems accept #, ext even does ?.
   * test them after last / only then. */
  if (!plen && !(base && !mrl_buf_is_file (base))) {
    mrlb->start[-1] = '/';
    for (p = (uint8_t *)mrlb->end; p[-1] != '/'; p--) ;
    scan_args = (char *)p;
  } else {
    scan_args = mrlb->root;
  }
  /* extra info */
  mrlb->end[0] = '#';
  mrlb->info = scan_args + _find_byte (scan_args, '#');
  mrlb->end[0] = 0;
  /* args */
  save = mrlb->info[0];
  mrlb->info[0] = '?';
  mrlb->args = scan_args + _find_byte (scan_args, '?');
  mrlb->info[0] = save;
  /* file:// */
  if ((plen == 4) && !memcmp (prot, "file", 4)) {
    mrlb->args = mrlb->start + _gui_string_unescape (mrlb->start, mrlb->args - mrlb->start);
    if (mrlb->info > mrlb->args) {
      memmove (mrlb->args, mrlb->info, mrlb->end - mrlb->info + 1);
      mrlb->end -= mrlb->info - mrlb->args;
      mrlb->info = mrlb->args;
      mrlb->end[0] = 0;
    }
  }
  /* last part */
  mrlb->start[-1] = '/';
  for (p = (uint8_t *)mrlb->args; p[-1] != '/'; p--) ;
  mrlb->lastpart = (char *)p;
  mrlb->start[-1] = 0;
  /* ext */
  mrlb->lastpart[-1] = '.';
  for (p = (uint8_t *)mrlb->args; p[-1] != '.'; p--) ;
  mrlb->lastpart[-1] = '/';
  mrlb->ext = p[-1] == '.' ? (char *)p : mrlb->args;
  mrlb->start[-1] = 0;

  return 1;
}

void mrl_buf_merge (mrl_buf_t *to, mrl_buf_t *base, mrl_buf_t *name) {
  to->max = to->buf + sizeof (to->buf) - 8;
  if (name->protend > name->start) {
    /* full new mrl */
    memcpy (to->start, name->start, name->end - name->start + 1);
    to->protend  = to->start + (name->protend  - name->start);
    to->host     = to->start + (name->host     - name->start);
    to->root     = to->start + (name->root     - name->start);
    to->lastpart = to->start + (name->lastpart - name->start);
    to->ext      = to->start + (name->ext      - name->start);
    to->args     = to->start + (name->args     - name->start);
    to->info     = to->start + (name->info     - name->start);
    to->end      = to->start + (name->end      - name->start);
  } else if (name->host > name->protend) {
    /* yes i havr seen "://host/foo/bar" :-) */
    if (to != base) {
      int l = base->protend - base->start;
      if (l > 0)
        memcpy (to->start, base->start, l);
      to->protend  = to->start + (base->protend - base->start);
    }
    memcpy (to->protend, name->start, name->end - name->start + 1);
    to->host     = to->protend + (name->host     - name->start);
    to->root     = to->protend + (name->root     - name->start);
    to->lastpart = to->protend + (name->lastpart - name->start);
    to->ext      = to->protend + (name->ext      - name->start);
    to->args     = to->protend + (name->args     - name->start);
    to->info     = to->protend + (name->info     - name->start);
    to->end      = to->protend + (name->end      - name->start);
  } else if (name->root[0] == '/') {
    /* absolute path on same host */
    if (to != base) {
      int l = base->root - base->start;
      if (l > 0)
        memcpy (to->start, base->start, l);
      to->protend  = to->start + (base->protend - base->start);
      to->host     = to->start + (base->host    - base->start);
      to->root     = to->start + (base->root    - base->start);
    }
    memcpy (to->root, name->start, name->end - name->start + 1);
    to->lastpart = to->root + (name->lastpart - name->start);
    to->ext      = to->root + (name->ext      - name->start);
    to->args     = to->root + (name->args     - name->start);
    to->info     = to->root + (name->info     - name->start);
    to->end      = to->root + (name->end      - name->start);
  } else {
    /* relative path */
    int l;
    if (to != base) {
      l = base->lastpart - base->start;
      if (l > 0)
        memcpy (to->start, base->start, l);
      to->protend  = to->start + (base->protend - base->start);
      to->host     = to->start + (base->host    - base->start);
      to->root     = to->start + (base->root    - base->start);
      to->lastpart = to->start + (base->lastpart - base->start);
    }
    memcpy (to->lastpart , name->start, name->end - name->start + 1);
    to->ext      = to->lastpart + (name->ext      - name->start);
    to->args     = to->lastpart + (name->args     - name->start);
    to->info     = to->lastpart + (name->info     - name->start);
    to->end      = to->lastpart + (name->end      - name->start);
    to->lastpart += name->lastpart - name->start;
    l = to->lastpart - to->root;
    l -= _mrl_buf_resolve_dots (to->root, l);
    if (l > 0) {
      memmove (to->lastpart - l, to->lastpart, to->end - to->lastpart + 1);
      to->lastpart -= l;
      to->ext -= l;
      to->args -= l;
      to->info -= l;
      to->end -= l;
    }
  }
  /* security */
  if (!mrl_buf_is_file (base)) {
    to->info[0] = 0;
  }
}

int mrl_buf_is_file (mrl_buf_t *mrlb) {
  return mrlb->root == mrlb->start;
}

/** basic mediamark_t ******************************************************************/

static int _mediamark_new_from_mrl_buf (mediamark_t **m, const char *ident, mrl_buf_t *mrl, mrl_buf_t *sub) {
  mediamark_t *n;
  size_t l;

  if (*m)
    return 0;
  *m = n = calloc (1, sizeof (*n));
  if (!n)
    return 0;

  l = mrl->end - mrl->start;
  n->mrl = malloc (l + 1);
  if (n->mrl) {
    memcpy (n->mrl, mrl->start, l + 1);
    n->mrl[l] = 0;
  }

  n->ident = n->mrl;
  l = ident ? _find_byte (ident, 0) : 0;
  if (!l) {
    ident = mrl->lastpart;
    l = mrl->args - mrl->lastpart;
  }
  if (l) {
    n->ident = malloc (l + 1);
    if (n->ident) {
      memcpy (n->ident, ident, l);
      n->ident[l] = 0;
    }
  }

  if (sub) {
    l = sub->end - sub->start;
    if (l) {
      n->sub = malloc (l + 1);
      if (n->sub) {
        memcpy (n->sub, sub->start, l);
        n->sub[l] = 0;
      }
    }
  }

  n->end = -1;
  return 1;
}

int mediamark_copy (mediamark_t **to, const mediamark_t *from) {
  const char *_ident, *_mrl, *_sub, *_none = "";
  mediamark_t *m;
  int n = 1;

  if (!to || !from)
    return 0;
  if (!*to) {
    *to = calloc (1, sizeof (**to));
    if (!*to)
      return 0;
  }
  m = *to;

  _mrl   = from->mrl   ? from->mrl   : _none;
  _ident = from->ident ? from->ident : _mrl;
  _sub   = from->sub;

  if ((_ident != _mrl) && !strcmp (_ident, _mrl))
    _ident = _mrl;
  if (_ident == _mrl) {
    if (!m->ident || strcmp (m->ident, _ident) || !m->mrl || strcmp (m->mrl, _mrl)) {
      char *s = strdup (_ident);
      if (s) {
        if (m->ident != m->mrl)
          free (m->ident);
        free (m->mrl);
        m->ident = m->mrl = s;
        n += 2;
      }
    }
  } else {
    if (!m->ident || strcmp (m->ident, _ident)) {
      char *s = strdup (_ident);
      if (s) {
        if (m->ident != m->mrl)
          free (m->ident);
        m->ident = s;
        n++;
      }
    }
    if (!m->mrl || strcmp (m->mrl, _mrl)) {
      char *s = strdup (_mrl);
      if (s) {
        if (m->ident != m->mrl)
          free (m->mrl);
        m->mrl = s;
        n++;
      }
    }
  }
  if ((_sub && !m->sub)
    || (!_sub && m->sub)
    || (_sub && m->sub && strcmp (_sub, m->sub))) {
    free (m->sub);
    m->sub = _sub ? strdup (_sub) : NULL;
    n++;
  }
  if (m->start != from->start)
    m->start = from->start, n++;
  if (m->end != from->end)
    m->end = from->end, n++;
  if (m->av_offset != from->av_offset)
    m->av_offset = from->av_offset, n++;
  if (m->spu_offset != from->spu_offset)
    m->spu_offset = from->spu_offset, n++;
  m->type = from->type;
  m->from = from->from;
  if (m->got_alternate == from->got_alternate)
    m->got_alternate = from->got_alternate, n++;
  mediamark_duplicate_alternates (from, m);
  return n;
}

int mediamark_set_str_val (mediamark_t **mmk, const char *value, mmk_val_t what) {
  if (!mmk)
    return 0;
  if (!*mmk)
    return 0;

  switch (what) {
    case MMK_VAL_MRL:
      if (!value)
        value = "";
      if ((*mmk)->mrl && !strcmp ((*mmk)->mrl, value))
        return 0;
      if ((*mmk)->ident != (*mmk)->mrl) {
        if ((*mmk)->ident && !strcmp ((*mmk)->ident, value)) {
          free ((*mmk)->mrl);
          (*mmk)->mrl = (*mmk)->ident;
          return 1;
        }
        free ((*mmk)->mrl);
      }
      (*mmk)->mrl = strdup (value);
      break;

    case MMK_VAL_IDENT:
      if (!value || !value[0]) {
        if ((*mmk)->ident == (*mmk)->mrl)
          return 0;
        free ((*mmk)->ident);
        (*mmk)->ident = (*mmk)->mrl;
        return 1;
      }
      if ((*mmk)->ident && !strcmp ((*mmk)->ident, value))
        return 0;
      if ((*mmk)->ident != (*mmk)->mrl) {
        if ((*mmk)->mrl && !strcmp ((*mmk)->mrl, value)) {
          free ((*mmk)->ident);
          (*mmk)->ident = (*mmk)->mrl;
          return 1;
        }
        free ((*mmk)->ident);
      }
      (*mmk)->ident = strdup (value);
      break;

    case MMK_VAL_SUB:
      free ((*mmk)->sub);
      (*mmk)->sub = value ? strdup (value) : NULL;
      break;

    case MMK_VAL_ADD_ALTER:
      mediamark_append_alternate_mrl (*mmk, value);
      (*mmk)->got_alternate = 1;
      break;

    default:
      return 0;
  }

  return 1;
}

int mediamark_free (mediamark_t **mmk) {
  if (mmk && *mmk) {
    mediamark_free_alternates (*mmk);
    if ((*mmk)->ident != (*mmk)->mrl)
      free ((*mmk)->ident);
    (*mmk)->ident = NULL;
    SAFE_FREE ((*mmk)->mrl);
    SAFE_FREE ((*mmk)->sub);
    SAFE_FREE (*mmk);
    return 1;
  }
  return 0;
}

void mediamark_free_alternates(mediamark_t *mmk) {
  if(mmk && mediamark_have_alternates(mmk)) {
    alternate_t *alt = mmk->alternates;

    while(alt) {
      alternate_t *c_alt = alt;

      free(alt->mrl);

      c_alt = alt;
      alt   = alt->next;
      free(c_alt);
    }
    mmk->alternates = NULL;
    mmk->cur_alt    = NULL;
  }
}

char *mediamark_get_first_alternate_mrl (mediamark_t *mmk) {
  if (mmk && mmk->alternates) {
    mmk->cur_alt = mmk->alternates;
    return mmk->cur_alt->mrl;
  }
  return NULL;
}

char *mediamark_get_next_alternate_mrl (mediamark_t *mmk) {
  if (mmk && mmk->cur_alt) {
    if (mmk->cur_alt->next) {
      mmk->cur_alt = mmk->cur_alt->next;
      return mmk->cur_alt->mrl;
    }
  }
  return NULL;
}

char *mediamark_get_current_alternate_mrl (mediamark_t *mmk) {
  if(mmk && mmk->cur_alt)
    return mmk->cur_alt->mrl;

  return NULL;
}

void mediamark_append_alternate_mrl (mediamark_t *mmk, const char *mrl) {
  if(mmk && mrl) {
    alternate_t *alt;

    alt       = (alternate_t *) calloc(1, sizeof(alternate_t));
    alt->mrl  = strdup(mrl);
    alt->next = NULL;

    if (mmk->alternates) {
      alternate_t *p_alt = mmk->alternates;

      while (p_alt->next)
	p_alt = p_alt->next;
      p_alt->next = alt;
    }
    else
      mmk->alternates = alt;

  }
}

void mediamark_duplicate_alternates (const mediamark_t *s_mmk, mediamark_t *d_mmk) {
  if (s_mmk && s_mmk->alternates && d_mmk) {
    alternate_t *alt;

    if((alt = s_mmk->alternates)) {
      alternate_t *c_alt, *p_alt = NULL, *t_alt = NULL;

      while(alt) {

	c_alt       = (alternate_t *) calloc(1, sizeof(alternate_t));
	c_alt->mrl  = strdup(alt->mrl);
	c_alt->next = NULL;

	if(!p_alt)
	  t_alt = p_alt = c_alt;
	else {
	  p_alt->next = c_alt;
	  p_alt       = c_alt;
	}

	alt = alt->next;
      }

      d_mmk->alternates = t_alt;
      d_mmk->cur_alt    = NULL;

    }
  }
}

int mediamark_got_alternate (mediamark_t *mmk) {
  if (mmk && mmk->got_alternate)
    return 1;
  return 0;
}

void mediamark_unset_got_alternate (mediamark_t *mmk) {
  if(mmk)
    mmk->got_alternate = 0;
}

/** gui currently played item **********************************************************/

int gui_current_set_index (gGui_t *gui, int idx) {
  mediamark_t *mmk, *d, none;

  if (!gui)
    return GUI_MMK_NONE;

  gui_playlist_lock (gui);
  if (idx == GUI_MMK_CURRENT)
    idx = gui->playlist.cur;
  if ((idx >= 0) && (idx < gui->playlist.num) && gui->playlist.mmk && gui->playlist.mmk[idx]) {
    mmk = gui->playlist.mmk[idx];
  } else {
    memset (&none, 0, sizeof (none));
    none.end = -1;
    /* TRANSLATORS: only ASCII characters (skin) */
    none.mrl = (char *)pgettext ("skin", "There is no MRL."); /** will not be written to. */
    none.ident = (char *)("xine-ui version " VERSION);
    mmk = &none;
    idx = GUI_MMK_NONE;
  }
  gui->playlist.cur = idx;
  d = &gui->mmk;
  mediamark_copy (&d, mmk);
  gui_playlist_unlock (gui);

  gui_pl_updated (gui);
  return idx;
}

void gui_current_free (gGui_t *gui) {
  gui_playlist_lock (gui);
  if (gui->mmk.ident != gui->mmk.mrl)
    free (gui->mmk.ident);
  gui->mmk.ident = NULL;
  SAFE_FREE (gui->mmk.mrl);
  SAFE_FREE (gui->mmk.sub);
  if (mediamark_have_alternates (&(gui->mmk)))
    mediamark_free_alternates (&(gui->mmk));
  gui_playlist_unlock (gui);
}

/** gui playlist ***********************************************************************/

typedef union {
  char z[4];
  uint32_t v;
} known_ext_t;

static const known_ext_t _media_exts[] = {
  {"4xm "}, {"ac3 "}, {"aif "}, {"aiff"}, {"asf "}, {"au  "}, {"aud "}, {"avi "},
  {"cak "}, {"cin "}, {"cpk "}, {"dat "}, {"dif "}, {"dps "}, {"dv  "}, {"film"},
  {"flc "}, {"fli "}, {"flv "}, {"ik2 "}, {"iki "}, {"jpeg"}, {"jpg "}, {"m2p "},
  {"m2t "}, {"mjpg"}, {"mkv "}, {"mng "}, {"mov "}, {"mp2 "}, {"mp3 "}, {"mp4 "},
  {"mp4a"}, {"mp4v"}, {"mpa "}, {"mpeg"}, {"mpg "}, {"mpv "}, {"mv8 "}, {"mve "},
  {"nsf "}, {"nsv "}, {"ogg "}, {"ogm "}, {"opus"}, {"pes "}, {"png "}, {"pva "},
  {"qt  "}, {"ra  "}, {"ram "}, {"rm  "}, {"rmvb"}, {"roq "}, {"snd "}, {"spx "},
  {"str "}, {"trp "}, {"ts  "}, {"wav "}, {"voc "}, {"vob "}, {"vox "}, {"wax "},
  {"wma "}, {"wmv "}, {"vqa "}, {"wve "}, {"wvx "}, {"xa  "}, {"xa1 "}, {"xa2 "},
  {"xap "}, {"xas "}, {"y4m "}
};

static const known_ext_t _spu_exts[] = {
  {"asc "}, {"ass "}, {"smi "}, {"srt "}, {"ssa "}, {"sub "}, {"txt "}
};

static const known_ext_t _playlist_exts[] = {
  {"asx "}, {"fxd "}, {"m3u "}, {"pls "}, {"sfv "}, {"smi "}, {"smil"}, {"tox "},
  {"xml "}
};

static int _known_ext_cmp (void *a, void *b) {
  uint32_t d = (uintptr_t)a, e = (uintptr_t)b;

  return (int)d - (int)e;
}

static xine_sarray_t *_set_known_exts (const known_ext_t *exts, uint32_t n) {
  xine_sarray_t *a = xine_sarray_new (n, _known_ext_cmp);

  if (a) {
    uint32_t u;

    for (u = 0; u < n; u++)
      xine_sarray_add (a, (void *)(uintptr_t)exts[u].v);
  }
  return a;
}

static int _is_known_ext (xine_sarray_t *a, uint32_t ext) {
  return (ext == 0x20202020) ? 0 : xine_sarray_binary_search (a, (void *)(uintptr_t)ext) >= 0;
}

static uint32_t _get_ext_val (const char *s, uint32_t l) {
  known_ext_t ext;
  uint32_t u;
  /* allow 5 so "mpeg" gets "mpega" and "mpegv" as well. */
  if (!l || (l > 5))
    return 0x20202020;
  if (l == 5)
    l = 4;
  ext.v = 0x20202020;
  for (u = 0; u < l; u++)
    ext.z[u] = s[u];
  return ext.v | 0x20202020;
}

void gui_playlist_init (gGui_t *gui) {
  if (!gui)
    return;
  gui->playlist.mmk = NULL;
  gui->playlist.max = 0;
  gui->playlist.num = 0;
  gui->playlist.cur = -1;
  gui->playlist.ref_append = 0;
  gui->playlist.loop = PLAYLIST_LOOP_NO_LOOP;
  gui->playlist.control = 0;
  gui->playlist.known_playlist_exts = _set_known_exts (_playlist_exts, sizeof (_playlist_exts) / sizeof (_playlist_exts[0]));
  gui->playlist.known_media_exts = _set_known_exts (_media_exts, sizeof (_media_exts) / sizeof (_media_exts[0]));
  gui->playlist.known_spu_exts = _set_known_exts (_spu_exts, sizeof (_spu_exts) / sizeof (_spu_exts[0]));
}

void gui_playlist_deinit (gGui_t *gui) {
  if (!gui)
    return;
  if (gui->playlist.mmk) {
    int i;

    for (i = gui->playlist.num - 1; i >= 0; i--)
      mediamark_free (gui->playlist.mmk + i);
    SAFE_FREE (gui->playlist.mmk);
  }
  gui->playlist.max = 0;
  gui->playlist.num = 0;
  gui->playlist.cur = -1;
  gui->playlist.ref_append = 0;
  gui->playlist.loop = 0;
  gui->playlist.control = 0;
  xine_sarray_delete (gui->playlist.known_playlist_exts);
  gui->playlist.known_playlist_exts = NULL;
  xine_sarray_delete (gui->playlist.known_media_exts);
  gui->playlist.known_media_exts = NULL;
  xine_sarray_delete (gui->playlist.known_spu_exts);
  gui->playlist.known_spu_exts = NULL;
}

int mrl_look_like_playlist (gGui_t *gui, const char *mrl) {
#if 1
  mrl_buf_t mrlb;
  if (!gui)
    return 0;
  mrl_buf_init (&mrlb);
  if (!mrl_buf_set (&mrlb, NULL, mrl))
    return 0;
  if ((mrlb.ext >= mrlb.args) || (mrlb.ext + 5 < mrlb.args))
    return 0;
  return _is_known_ext (gui->playlist.known_playlist_exts, _get_ext_val (mrlb.ext, mrlb.args - mrlb.ext));
#else
  /* TJ. I dont know whether someone really needs to treat
   * "foo/bar.m3under/the/table" as an m3u playlist.
   * Lets keep this behaviour for now, but make sure that
   * hls (.m3u8) goes to xine-lib verbatim. */
  const char *extension = strrchr (mrl, '.');
  (void)gui;
  if (extension) {
    /* All known playlist ending */
    if ((!strncasecmp (extension, ".asx", 4))  ||
        (!strncasecmp (extension, ".smi", 4))  ||
      /*(!strncasecmp (extension, ".smil", 5)) || caught by ".smi" */
        (!strncasecmp (extension, ".pls", 4))  ||
        (!strncasecmp (extension, ".sfv", 4))  ||
        (!strncasecmp (extension, ".xml", 4))  ||
        (!strncasecmp (extension, ".tox", 4))  ||
        (!strncasecmp (extension, ".fxd", 4)))
      return 1;
    if ((!strncasecmp (extension, ".m3u", 4)) && (extension[4] != '8'))
      return 1;
  }
  return 0;
#endif
}

int gui_playlist_set_str_val (gGui_t *gui, const char *value, mmk_val_t what, int idx) {
  gui_playlist_lock (gui);
  if (idx == GUI_MMK_CURRENT)
    idx = gui->playlist.cur;
  if ((idx >= 0) && (idx < gui->playlist.num) && gui->playlist.mmk)
    idx = mediamark_set_str_val (&gui->playlist.mmk[idx], value, what);
  else
    idx = GUI_MMK_NONE;
  gui_playlist_unlock (gui);
  return idx;
}

mediamark_t *mediamark_get_current_mmk (gGui_t *gui) {
  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur])
    return gui->playlist.mmk[gui->playlist.cur];

  return (mediamark_t *) NULL;
}

mediamark_t *mediamark_get_mmk_by_index (gGui_t *gui, int index) {
  if(index < gui->playlist.num && index >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[index])
    return gui->playlist.mmk[index];

  return (mediamark_t *) NULL;
}

const char *mediamark_get_current_mrl (gGui_t *gui) {
  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur] &&
     gui->playlist.cur < gui->playlist.num)
    return gui->playlist.mmk[gui->playlist.cur]->mrl;

  return NULL;
}

const char *mediamark_get_current_ident (gGui_t *gui) {
  if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
     gui->playlist.mmk[gui->playlist.cur])
    return gui->playlist.mmk[gui->playlist.cur]->ident;

  return NULL;
}

typedef struct {
  char *buf1, *buf2, ext[8], **lines;
  mrl_buf_t *base, mrl, sub, item;
  const char *type;
  size_t size, num_lines, num_entries;
  mediamark_t **mmk;
  uint32_t have, used;
} _lf_t;

static int _lf_add (_lf_t *lf, int index, const mediamark_t *mmk) {
  if (index < 0)
    return 0;

  if ((uint32_t)index >= lf->have) {
    uint32_t nhave = (index + 32) & ~31;
    mediamark_t **n = realloc (lf->mmk, nhave * sizeof (*n));
    if (!n)
      return 0;
    memset (n + lf->have, 0, (nhave - lf->have) * sizeof (*n));
    lf->mmk = n;
    lf->have = nhave;
  }
  if (!mmk)
    return 0;
  if ((uint32_t)index >= lf->used)
    lf->used = index + 1;

  if (mrl_buf_set (&lf->item, lf->base, mmk->mrl))
    mrl_buf_merge (&lf->mrl, lf->base, &lf->item);
  else
    mrl_buf_init (&lf->mrl);
  if (mrl_buf_set (&lf->item, lf->base, mmk->sub))
    mrl_buf_merge (&lf->sub, lf->base, &lf->item);
  else
    mrl_buf_init (&lf->sub);
  if (!_mediamark_new_from_mrl_buf (lf->mmk + index, mmk->ident, &lf->mrl, &lf->sub))
    return 0;

  lf->mmk[index]->start = mmk->start;
  lf->mmk[index]->end = mmk->end;
  lf->mmk[index]->av_offset = mmk->av_offset;
  lf->mmk[index]->spu_offset = mmk->spu_offset;

  lf->mmk[index]->type = mmk->type;
  lf->mmk[index]->from = mmk->from;
  return 1;
}
  
static void _lf_add2 (_lf_t *lf, mediamark_t *mmk) {
  if (lf->used >= lf->have) {
    mediamark_t **n = realloc (lf->mmk, (lf->have + 32) * sizeof (*n));
    if (!n) {
      mediamark_t *_mmk = mmk;
      mediamark_free (&_mmk);
      return;
    }
    lf->mmk = n;
    lf->have += 32;
  }
  lf->mmk[lf->used++] = mmk;
}
  
static _lf_t *_lf_new (mrl_buf_t *name, size_t size) {
  _lf_t *lf;
  size_t need = (size + 4 + 1 + 4 + 3) & ~3;
  char *m = malloc (sizeof (*lf) + 2 * need);

  if (!m) {
    fprintf(stderr, "%s(): malloc() failed.\n", __XINE_FUNCTION__);
    return NULL;
  }

  lf = (_lf_t *)m;
  lf->ext[0] = 0;
  lf->base = name;
  mrl_buf_init (&lf->mrl);
  mrl_buf_init (&lf->sub);
  mrl_buf_init (&lf->item);
  lf->type = "";
  lf->num_entries = 0;
  lf->lines = NULL;
  lf->size = size;
  m += sizeof (*lf);
  memset (m, 0, 4);
  lf->buf1 = m + 4;
  m += need;
  memset (m - 4, 0, 8);
  lf->buf2 = m + 4;
  memset (m + need - 4, 0, 4);
  lf->mmk = NULL;
  lf->have = 0;
  lf->used = 0;
  return lf;
}

static char *_lf_dup (_lf_t *lf) {
  free (lf->lines);
  lf->lines = NULL;
  memcpy (lf->buf2, lf->buf1, lf->size + 1);
  return lf->buf2;
}

static int _lf_split_lines (_lf_t *lf) {
  char *line, *nextline;
  uint32_t have, used;

  if (lf->lines)
    return lf->num_lines;

  have = used = 0;
  memcpy (lf->buf2, lf->buf1, lf->size);
  lf->buf2[-1] = 0;
  lf->buf2[lf->size] = '\n';
  for (line = lf->buf2; line < lf->buf2 + lf->size; line = nextline) {
    char *lend;

    if (used + 3 > have) {
      char **n = realloc (lf->lines, (have + 128) * sizeof (*n));

      if (!n)
        break;
      lf->lines = n;
      have += 128;
    }
    lend = line + _find_byte (line, '\n');
    nextline = lend + 1;
    lend[0] = 0;
    if (lend[-1] == '\r')
      *--lend = 0;
    if (lend > line)
      lf->lines[used++] = line;
  }
  line[0] = 0;
  if (lf->lines) {
    lf->lines[used] = line;
    lf->lines[used + 1] = NULL;
  }
  lf->num_lines = used;
  return used;
}

static void _lf_delete (_lf_t *lf) {
  lf->buf1 = NULL;
  lf->buf2 = NULL;
  lf->type = NULL;
  free (lf->lines);
  lf->lines = NULL;
  free (lf->mmk);
  lf->mmk = NULL;
  free (lf);
}

typedef void (*playlist_guess_func_t) (_lf_t *lf);
typedef void (*playlist_xml_guess_func_t) (_lf_t *lf, xml_node_t *);

static _lf_t *_download_file (gGui_t *gui, mrl_buf_t *name) {
  _lf_t *lf;
  download_t  download;

  download.gui    = gui;
  download.buf    = NULL;
  download.error  = NULL;
  download.size   = 0;
  download.status = 0;

  if (!network_download (name->start, &download)) {
    gui_msg (gui, XUI_MSG_ERROR, "Unable to download '%s': %s", name->start, download.error);
    lf = NULL;
  } else {
    lf = _lf_new (name, download.size);
    if (lf)
      memcpy (lf->buf1, download.buf, lf->size);
  }

  SAFE_FREE (download.buf);
  SAFE_FREE (download.error);
  return lf;
}

#if 0
static int _file_exist(char *filename) {
  struct stat  st;

  if(filename && (stat(filename, &st) == 0))
    return 1;

  return 0;
}
#endif

static void _lf_ext (_lf_t *lf)  {
  size_t el = lf->base->args - lf->base->ext;

  if (el > sizeof (lf->ext) - 1)
    el = sizeof (lf->ext) - 1;
  memcpy (lf->ext, lf->base->ext, el);
  lf->ext[el] = 0;
}

static _lf_t *_lf_get (gGui_t *gui, mrl_buf_t *name) {
  struct stat st;
  _lf_t *lf;
  int fd, bytes_read;
  size_t size;

  if (!mrl_buf_is_file (name))
    return _download_file (gui, name);

  if (stat (name->start, &st) < 0) {
    fprintf (stderr, "%s(): Unable to stat() '%s' file: %s.\n", __XINE_FUNCTION__, name->start, strerror (errno));
    return NULL;
  }

  if ((size = st.st_size) == 0) {
    fprintf (stderr, "%s(): File '%s' is empty.\n", __XINE_FUNCTION__, name->start);
    return NULL;
  }

  if ((fd = xine_open_cloexec (name->start, O_RDONLY)) == -1) {
    fprintf (stderr, "%s(): open(%s) failed: %s.\n", __XINE_FUNCTION__, name->start, strerror (errno));
    return NULL;
  }

  lf = _lf_new (name, size);
  if (!lf) {
    close (fd);
    return NULL;
  }

  if ((bytes_read = read (fd, lf->buf1, size)) != (int)size) {
    fprintf (stderr, "%s(): read() return wrong size (%d / %d): %s.\n",
      __XINE_FUNCTION__, bytes_read, (int)size, strerror (errno));
    lf->size = bytes_read;
  }

  close (fd);

  _lf_ext (lf);
  return lf;
}

/*
 * Playlists guessing
 */
static void guess_pls_playlist (_lf_t *lf) {
  if (!strcasecmp (lf->ext, "pls")) {
    if (_lf_split_lines (lf)) {
      int   valid_pls     = 0;
      int   entries_pls   = 0;
      int   found_nument  = 0;
      int   stored_nument = 0;
      int   pl_line       = 0;
      uint32_t linen      = 0;
      int   count         = 0;
      mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

      do {
        while (linen < lf->num_lines) {
          const char *ln = lf->lines[linen++];

          if (valid_pls) {
            if (entries_pls) {
              int entry;

              if ((!strncasecmp (ln, "file", 4)) && (( sscanf(ln + 4, "%d=", &entry)) == 1)) {
                char *mrl = strchr (ln, '=');

                if (mrl && (entry > 0) && (entry <= entries_pls)) {
                  m.mrl = mrl + 1;
                  m.ident = NULL;
                  stored_nument += _lf_add (lf, entry - 1, &m);
                }
              }
            } else {
              if ((!strncasecmp (ln, "numberofentries", 15)) && ((sscanf (ln + 15, "=%d", &entries_pls)) == 1)) {
                if (!found_nument) {
                  if (entries_pls) {
                    lf->num_entries = entries_pls;
                    found_nument = 1;
                  }
                }
              }
            }
          } else if ((!strcasecmp (ln, "[playlist]"))) {
            if (!valid_pls) {
              valid_pls = 1;
              pl_line = linen;
            }
          }
        }
        count++;
        linen = pl_line;
      } while ((lf->num_entries && !stored_nument) && (count < 2));

      if (lf->mmk && valid_pls && entries_pls) {
        int i;
        /* Fill missing entries */
        m.mrl = NULL;
        m.ident = _("!!Invalid entry!!");
        for (i = 0; i < entries_pls; i++) {
          if (!lf->mmk[i])
            _lf_add (lf, i, &m);
        }
        lf->type = "PLS";
      }

      if (valid_pls && entries_pls)
        return;
    }
  }
}

static void guess_m3u_playlist (_lf_t *lf) {
  if (_lf_split_lines (lf)) {
    int   valid_m3u   = 0;
    int   entries_m3u = 0;
    char *title       = NULL;
    uint32_t linen    = 0;
    mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

    while (linen < lf->num_lines) {
      char *ln = lf->lines[linen++];

      if (valid_m3u) {
        if (!strncmp (ln, "#EXTINF", 7)) {
          char *ptitle = strchr (ln, ',');

          title = ptitle ? ptitle + 1 : NULL;
        } else if (ln[0] != '#') {
          m.mrl = ln;
          m.ident = title;
          _lf_add (lf, entries_m3u, &m);
          lf->num_entries = ++entries_m3u;
          title = NULL;
        }
      } else if ((!strcasecmp (ln, "#EXTM3U")))
        valid_m3u = 1;
    }
    if (valid_m3u && entries_m3u) {
      lf->type = "M3U";
    }
  }
}

static void guess_sfv_playlist (_lf_t *lf) {
  if (!strcasecmp (lf->ext, "sfv")) {
    if (_lf_split_lines (lf)) {
      int    valid_sfv = 0;
      int    entries_sfv = 0;
      uint32_t linen = 0;
      mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

      while (linen < lf->num_lines) {
        char  *ln = lf->lines[linen++];

        if (valid_sfv) {
          if (ln[0] != ';') {
            if (ln[0]) {
              long long int crc = 0;
              char *p = lf->lines[linen] - 2, *q = NULL;

              while ((p > ln) && (*p != ' '))
                p--;
              if (p > ln) {
                q = p;
                *p = '\0';
                p++;
              }
              if (p[0]) {
                errno = 0;
                crc = strtoll (p, &p, 16);
                if ((errno == ERANGE) || (errno == EINVAL))
                  crc = 0;
              }
              if (crc > 0) {
                m.mrl = ln;
                m.ident = NULL;
                _lf_add (lf, entries_sfv, &m);
                lf->num_entries = ++entries_sfv;
              }
              if (q)
                *q = ' ';
            }
          }
        } else if (ln[0] && ln[1]) {
          long int   size;
          int        h, m, s;
          int        Y, M, D;
          char       fn[2];
          char       mon[4];

          if (((sscanf (ln, ";%ld %d:%d.%d %d-%d-%d %1s", &size, &h, &m, &s, &Y, &M, &D, &fn[0])) == 8) ||
              ((sscanf (ln, ";%ld %3s %d %d:%d:%d %d %1s", &size, &mon[0], &D, &h, &m, &s, &Y, &fn[0])) == 8))
            valid_sfv = 1;
        }
      }

      if (valid_sfv && entries_sfv) {
        lf->type = "SFV";
      }
    }
  }
}

static void guess_raw_playlist (_lf_t *lf) {
  if (_lf_split_lines (lf)) {
    int   entries_raw = 0;
    uint32_t linen = 0;
    mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

    while (linen < lf->num_lines) {
      char *ln = lf->lines[linen++];

      if ((strncmp (ln, ";", 1)) && (strncmp (ln, "#", 1))) {
        m.mrl = ln;
        m.ident = NULL;
        _lf_add (lf, entries_raw, &m);
        lf->num_entries = ++entries_raw;
      }
    }

    if (entries_raw) {
      lf->type = "RAW";
    }
  }
}

static void guess_toxine_playlist (_lf_t *lf) {
  char *text = _lf_dup (lf);
  xitk_cfg_parse_t *tree = xitk_cfg_parse (text, XITK_CFG_PARSE_DEBUG);
  xitk_cfg_parse_t *entry;
  int num_entries;

  if (!tree)
    return;

  /* just avoid reallocations. */
  num_entries = 0;
  for (entry = tree + tree->first_child; entry != tree; entry = tree + entry->next)
    if ((entry->klen == 5) && !memcmp (text + entry->key, "entry", 5))
      num_entries += 1;
  _lf_add (lf, num_entries, NULL);

  num_entries = 0;
  for (entry = tree + tree->first_child; entry != tree; entry = tree + entry->next) {
    if ((entry->klen == 5) && !memcmp (text + entry->key, "entry", 5)) {
      mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };
      xitk_cfg_parse_t *elem;
      uint32_t mmkf_members = 0;

      for (elem = tree + entry->first_child; elem != tree; elem = tree + elem->next) {
        const char *key = text + elem->key;
        const char *val = text + elem->value;

        switch (elem->klen) {
          case 3:
            if (!memcmp (key, "end", 3)) {
              if (mmkf_members & 1)
                break;
              mmkf_members |= 1;
              m.end = xitk_str2int32 (&val);
            } else if (!memcmp (key, "mrl", 3)) {
              if (m.mrl)
                break;
              m.mrl = (char *)val;
            }
            break;
          case 5:
            if (!memcmp (key, "start", 5)) {
              if (mmkf_members & 2)
                break;
              mmkf_members |= 2;
              m.start = xitk_str2int32 (&val);
            }
            break;
          case 8:
            if (!memcmp (key, "subtitle", 8)) {
              /* Workaround old toxine playlist version bug */
              if (m.sub || !strcmp (val, "(null)"))
                break;
              m.sub = (char *)val;
            }
            break;
          case 9:
            if (!memcmp (key, "av_offset", 9)) {
              if (mmkf_members & 4)
                break;
              mmkf_members |= 4;
              m.av_offset = xitk_str2int32 (&val);
            }
            break;
          case 10:
            if (!memcmp (key, "identifier", 10)) {
              if (m.ident)
                break;
              m.ident = (char *)val; /** << will not be written to. */
            } else if (!memcmp (key, "spu_offset", 10)) {
              if (mmkf_members & 8)
                break;
              mmkf_members |= 8;
              m.spu_offset = xitk_str2int32 (&val);
            }
            break;
          default: ;
        }
      }
      if (!m.mrl)
        continue;
      _lf_add (lf, num_entries, &m);
      lf->num_entries = ++num_entries;
    }
  }
  xitk_cfg_unparse (tree);
  if (num_entries)
    lf->type = "TOX";
}

/*
 * XML based playlists
 */
static void xml_asx_playlist (_lf_t *lf, xml_node_t *xml_tree) {
  int entries_asx = 0;

  if (!strcasecmp (xml_tree->name, "ASX")) {
    mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };
    xml_property_t  *asx_prop;
    int version_ok = 0;

    for (asx_prop = xml_tree->props; asx_prop && strcasecmp (asx_prop->name, "VERSION"); asx_prop = asx_prop->next) ;
    if (asx_prop) {
      int  version_major, version_minor = 0;

      if ((((sscanf (asx_prop->value, "%d.%d", &version_major, &version_minor)) == 2) ||
        ((sscanf (asx_prop->value, "%d", &version_major)) == 1)) &&
        ((version_major == 3) && (version_minor == 0))) {
        version_ok = 1;
      } else {
        fprintf (stderr, "%s(): Wrong ASX version: %s\n", __XINE_FUNCTION__, asx_prop->value);
      }
    } else {
      fprintf (stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
      fprintf (stderr, "%s(): last chance: try to parse it anyway\n", __XINE_FUNCTION__);
      version_ok = 2;
    }

    if (version_ok) {
      xml_node_t *asx_entry;

      for (asx_entry = xml_tree->child; asx_entry; asx_entry = asx_entry->next) {
        if (!strcasecmp (asx_entry->name, "ENTRY") ||
          !strcasecmp (asx_entry->name, "ENTRYREF")) {
          char *title = NULL, *href = NULL, *author = NULL, *sub = NULL;
          char *atitle = NULL, *aauthor = NULL, *real_title = NULL;
          xml_node_t *asx_ref;
          int len = 0;

          for (asx_ref = asx_entry->child; asx_ref; asx_ref = asx_ref->next) {
            if (!strcasecmp (asx_ref->name, "TITLE")) {
              if (!title)
                title = asx_ref->data;
            } else if (!strcasecmp (asx_ref->name, "AUTHOR")) {
              if (!author)
                author = asx_ref->data;
            } else if (!strcasecmp (asx_ref->name, "REF")) {
              for (asx_prop = asx_ref->props; asx_prop; asx_prop = asx_prop->next) {
                if (!strcasecmp (asx_prop->name, "HREF")) {
                  if (!href)
                    href = asx_prop->value;
                } else if (!strcasecmp (asx_prop->name, "SUBTITLE")) {
                  /* This is not part of the ASX specs */
                  if (!sub)
                    sub = asx_prop->value;
                }
                if (href && sub)
                  break;
              }
            }
          }
          if (href && href[0]) {
            if (title && title[0]) {
              atitle = strdup (title);
              len = str_unquote (atitle);
            }
            if (author && author[0]) {
              aauthor = strdup (author);
              len += str_unquote (aauthor) + 3;
            }
            len++;
          }
          if (atitle && atitle[0]) {
            real_title = malloc (len);
            strcpy (real_title, atitle);
            if (aauthor && aauthor[0]) {
              int rtl = _find_byte (real_title, 0);

              snprintf (real_title + rtl, len - rtl, " (%s)", aauthor);
            }
          }
          m.mrl = href;
          m.ident = real_title;
          m.sub = sub;
          _lf_add (lf, entries_asx, &m);
          lf->num_entries = ++entries_asx;
          SAFE_FREE (real_title);
          SAFE_FREE (atitle);
          SAFE_FREE (aauthor);
        }
      }
    }
  }
#ifdef DEBUG
  else
    fprintf (stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif

  /* Maybe it's 'ASF <url> */
  if (entries_asx == 0) {
    if (_lf_split_lines (lf)) {
      uint32_t linen = 0;
      mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

      while (linen < lf->num_lines) {
        char *ln = lf->lines[linen++];

        if (!strncasecmp ("ASF", ln, 3)) {
          char *p = ln + 3;

          while ((*p == ' ') || (*p == '\t'))
            p++;
          if (p[0]) {
            m.mrl = p;
            m.ident = NULL;
            _lf_add (lf, entries_asx, &m);
            lf->num_entries = ++entries_asx;
          }
        }
      }
    }
  }
  if (entries_asx)
    lf->type = "ASX3";
}

static void __gx_get_entries (_lf_t *lf, int *entries, xml_node_t *entry) {
  xml_node_t      *node;
  mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };

  for (node = entry; node; node = node->next) {
    if (!strcasecmp (node->name, "SUB")) {
      __gx_get_entries (lf, entries, node->child);
    } else if (!strcasecmp (node->name, "ENTRY")) {
      xml_node_t *ref;

      m.ident = NULL;
      m.mrl = NULL;
      m.start = 0;

      for (ref = node->child; ref; ref = ref->next) {
        if (!strcasecmp (ref->name, "TITLE")) {
          if (!m.ident)
            m.ident = ref->data;
        } else if (!strcasecmp (ref->name, "REF")) {
          xml_property_t *prop;

          for (prop = ref->props; prop; prop = prop->next) {
            if (!strcasecmp (prop->name, "HREF")) {
              if (!m.mrl) {
                m.mrl = prop->value;
                if (m.mrl)
                  break;
              }
            }
          }
        } else if (!strcasecmp (ref->name, "TIME")) {
          xml_property_t *prop;

          for (prop = ref->props; prop; prop = prop->next) {
            if (!strcasecmp (prop->name, "START")) {
              if (prop->value && prop->value[0])
                m.start = atoi (prop->value);
            }
          }
        }
      }

      if (m.mrl && m.mrl[0]) {
        if (m.ident && m.ident[0]) {
          m.ident = strdup (m.ident);
          str_unquote (m.ident);
        } else {
          m.ident = NULL;
        }
        _lf_add (lf, *entries, &m);
	lf->num_entries = ++(*entries);
        SAFE_FREE (m.ident);
      }
    }
  }
}
static void xml_gx_playlist (_lf_t *lf, xml_node_t *xml_tree) {
  int              entries_gx = 0;

  if (!strcasecmp (xml_tree->name, "GXINEMM")) {
    xml_property_t *gx_prop;

    for (gx_prop = xml_tree->props; gx_prop && strcasecmp (gx_prop->name, "VERSION"); gx_prop = gx_prop->next) ;
    if (gx_prop) {
      int  version_major;

      if (((sscanf (gx_prop->value, "%d", &version_major)) == 1) && (version_major == 1)) {
        xml_node_t *gx_entry;

        for (gx_entry = xml_tree->child; gx_entry; gx_entry = gx_entry->next) {
          if (!strcasecmp (gx_entry->name, "SUB")) {
            __gx_get_entries (lf, &entries_gx, gx_entry->child);
          } else if (!strcasecmp (gx_entry->name, "ENTRY")) {
            __gx_get_entries (lf, &entries_gx, gx_entry);
          }
        }
      } else {
        fprintf (stderr, "%s(): Wrong GXINEMM version: %s\n", __XINE_FUNCTION__, gx_prop->value);
      }
    } else {
      fprintf (stderr, "%s(): Unable to find VERSION tag.\n", __XINE_FUNCTION__);
    }
  }
#ifdef DEBUG
  else
    fprintf (stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif
  if (entries_gx)
    lf->type = "GXMM";
}

static void xml_noatun_playlist (_lf_t *lf, xml_node_t *xml_tree) {
  int              entries_noa = 0;

  if (!strcasecmp (xml_tree->name, "PLAYLIST")) {
    mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };
    xml_property_t *noa_prop;
    int found = 0;

    for (noa_prop = xml_tree->props; noa_prop; noa_prop = noa_prop->next) {
      if (!strcasecmp (noa_prop->name, "CLIENT") && strcasecmp (noa_prop->value, "NOATUN")) {
        found++;
      } else if (!strcasecmp (noa_prop->name, "VERSION")) {
        int  version_major;

        if (((sscanf (noa_prop->value, "%d", &version_major)) == 1) && (version_major == 1))
          found++;
      }
    }

    if (found >= 2) {
      xml_node_t *noa_entry;

      for (noa_entry = xml_tree->child; noa_entry; noa_entry = noa_entry->next) {
        if (!strcasecmp (noa_entry->name, "ITEM")) {
          char *title = NULL, *album = NULL, *artist = NULL, *url = NULL;

          for (noa_prop = noa_entry->props; noa_prop; noa_prop = noa_prop->next) {
            if (!strcasecmp (noa_prop->name, "TITLE"))
              title = noa_prop->value;
            else if (!strcasecmp (noa_prop->name, "ALBUM"))
              album = noa_prop->value;
            else if (!strcasecmp (noa_prop->name, "ARTIST"))
              artist = noa_prop->value;
            else if (!strcasecmp (noa_prop->name, "URL"))
              url = noa_prop->value;
          }
          if (url) {
            char *real_title = NULL;
            /* title (artist - album) */
            if (title && title[0]) {
              if (artist && artist[0] && album && album[0]) {
                real_title = xitk_asprintf ("%s (%s - %s)", title, artist, album);
              } else if (artist && artist[0]) {
                real_title = xitk_asprintf ("%s (%s)", title, artist);
              } else if (album && album[0]) {
                real_title = xitk_asprintf ("%s (%s)", title, album);
              } else {
                real_title = strdup (title);
              }
            }
            m.mrl = url;
            m.ident = real_title;
            _lf_add (lf, entries_noa, &m);
            lf->num_entries = ++entries_noa;
            free (real_title);
          }
        }
      }
    }
  }
#ifdef DEBUG
  else
    fprintf (stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif
  if (entries_noa)
    lf->type = "NOATUN";
}

/*
 * ********************************** SMIL BEGIN ***********************************
 */
#undef DEBUG_SMIL

#ifdef DEBUG_SMIL
static int offset = 0;
#define palign do { int i; for(i = 0; i < offset; i++) { printf(" "); } } while(0)
#endif

typedef struct smil_node_s smil_node_t;

typedef struct {
  char   *anchor;

  int     repeat;
  int     begin;
  int     end;

  int     clip_begin;
  int     clip_end;

  int     duration;

} smil_property_t;

struct smil_node_s {
  mediamark_t      *mmk;
  smil_property_t   prop;
  smil_node_t      *next;
};

typedef struct {
  /* Header */
  char   *title;
  char   *author;
  char   *copyright;
  char   *year;
  char   *base;
  /* End Header */

  smil_node_t *first;
  smil_node_t *node;

} smil_t;

/* protos */
static void smil_par (smil_t *, smil_node_t **, xml_node_t *);
static void smil_switch (smil_t *, smil_node_t **, xml_node_t *);

static void smil_init_smil_property(smil_property_t *sprop) {
  sprop->anchor     = NULL;
  sprop->repeat     = 1;
  sprop->begin      = 0;
  sprop->clip_begin = 0;
  sprop->end        = -1;
  sprop->clip_end   = -1;
  sprop->duration   = 0;
}
static smil_node_t *smil_new_node(void) {
  smil_node_t *node;

  node       = (smil_node_t *) calloc(1, sizeof(smil_node_t));
  node->mmk  = NULL;
  node->next = NULL;

  smil_init_smil_property(&(node->prop));

  return node;
}
static mediamark_t *smil_new_mediamark (void) {
  mediamark_t *mmk = calloc (1, sizeof (*mmk));

  if (mmk) {
    mmk->end = -1;
    mmk->from = MMK_FROM_PLAYLIST;
  }
  return mmk;
}
static mediamark_t *smil_duplicate_mmk(mediamark_t *ommk) {
  mediamark_t *mmk = NULL;

  if(ommk) {
    mmk             = smil_new_mediamark();
    mmk->mrl        = strdup(ommk->mrl);
    mmk->ident      = strdup(ommk->ident ? ommk->ident : ommk->mrl);
    mmk->sub        = ommk->sub ? strdup(ommk->sub) : NULL;
    mmk->start      = ommk->start;
    mmk->end        = ommk->end;
    mmk->played     = ommk->played;
    mmk->type       = ommk->type;
    mmk->from       = ommk->from;
    mmk->alternates = NULL;
    mmk->cur_alt    = NULL;
  }

  return mmk;
}
static void smil_free_properties(smil_property_t *smil_props) {
  if(smil_props) {
    SAFE_FREE(smil_props->anchor);
  }
}

#ifdef DEBUG_SMIL
static void smil_dump_header(smil_t *smil) {
  printf("title: %s\n", smil->title);
  printf("author: %s\n", smil->author);
  printf("copyright: %s\n", smil->copyright);
  printf("year: %s\n", smil->year);
  printf("base: %s\n", smil->base);
}

static void smil_dump_tree(smil_node_t *node) {
  if(node->mmk) {
    printf("mrl:   %s\n", node->mmk->mrl);
    printf(" ident: %s\n", node->mmk->ident);
    printf("  sub:   %s\n", node->mmk->sub);
    printf("   start: %d\n", node->mmk->start);
    printf("    end:   %d\n", node->mmk->end);
  }

  if(node->next)
    smil_dump_tree(node->next);
}
#endif

static char *smil_get_prop_value(xml_property_t *props, const char *propname) {
  xml_property_t  *prop;

  for(prop = props; prop; prop = prop->next) {
    if(!strcasecmp(prop->name, propname)) {
      return prop->value;
    }
  }
  return NULL;
}

static int smil_get_time_in_seconds(const char *time_str) {
  int    retval = 0;
  //int    hours, mins, secs, msecs;
  int    unit = 0; /* 1 = ms, 2 = s, 3 = min, 4 = h */

  if(strstr(time_str, "ms"))
    unit = 1;
  else if(strstr(time_str, "s"))
    unit = 2;
  else if(strstr(time_str, "min"))
    unit = 3;
  else if(strstr(time_str, "h"))
    unit = 4;
  else
    unit = 2;

  /* not reached
  if(unit == 0) {
    if((sscanf(time_str, "%d:%d:%d.%d", &hours, &mins, &secs, &msecs)) == 4)
      retval = (hours * 60 * 60) + (mins * 60) + secs + ((int) msecs / 1000);
    else if((sscanf(time_str, "%d:%d:%d", &hours, &mins, &secs)) == 3)
      retval = (hours * 60 * 60) + (mins * 60) + secs;
    else if((sscanf(time_str, "%d:%d.%d", &mins, &secs, &msecs)) == 3)
      retval = (mins * 60) + secs + ((int) msecs / 1000);
    else if((sscanf(time_str, "%d:%d", &mins, &secs)) == 2) {
      retval = (mins * 60) + secs;
    }
  }
  else*/ {
    int val, dec, args;

    if(((args = sscanf(time_str, "%d.%d", &val, &dec)) == 2) ||
       ((args = sscanf(time_str, "%d", &val)) == 1)) {
      switch(unit) {
      case 1: /* ms */
	retval = (int) val / 1000;
	break;
      case 2: /* s */
	retval = val + ((args == 2) ? ((int)((((float) dec) * 10) / 1000)) : 0);
	break;
      case 3: /* min */
	retval = (val * 60) + ((args == 2) ? (dec * 10 * 60 / 100) : 0);
	break;
      case 4: /* h */
	retval = (val * 60 * 60) + ((args == 2) ? (dec * 10 * 60 / 100) : 0);
	break;
      }
    }
  }

  return retval;
}
static void smil_header(smil_t *smil, xml_property_t *props) {
  xml_property_t  *prop;

#ifdef DEBUG_SMIL
  offset += 2;
#endif

  for(prop = props; prop; prop = prop->next) {
#ifdef DEBUG_SMIL
    palign;
    printf("%s(): prop_name '%s', value: '%s'\n", __XINE_FUNCTION__,
	   prop->name, prop->value ? prop->value : "<NULL>");
#endif

    if(!strcasecmp(prop->name, "NAME")) {

      if(!strcasecmp(prop->value, "TITLE"))
	smil->title = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "AUTHOR"))
	smil->author = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "COPYRIGHT"))
	smil->copyright = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "YEAR"))
	smil->year = smil_get_prop_value(prop, "CONTENT");
      else if(!strcasecmp(prop->value, "BASE"))
	smil->base = smil_get_prop_value(prop, "CONTENT");

    }
  }
#ifdef DEBUG_SMIL
  palign;
  printf("---\n");
  offset -= 2;
#endif
}

static void smil_get_properties(smil_property_t *dprop, xml_property_t *props) {
  xml_property_t  *prop;

#ifdef DEBUG_SMIL
  offset += 2;
#endif
  for(prop = props; prop; prop = prop->next) {

    if(!strcasecmp(prop->name, "REPEAT")) {
      dprop->repeat = strtol(prop->value, &prop->value, 10);
#ifdef DEBUG_SMIL
      palign;
      printf("REPEAT: %d\n", dprop->repeat);
#endif
    }
    else if(!strcasecmp(prop->name, "BEGIN")) {
      dprop->begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("BEGIN: %d\n", dprop->begin);
#endif
    }
    else if((!strcasecmp(prop->name, "CLIP-BEGIN")) || (!strcasecmp(prop->name, "CLIPBEGIN"))) {
      dprop->clip_begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("CLIP-BEGIN: %d\n", dprop->clip_begin);
#endif
    }
    else if(!strcasecmp(prop->name, "END")) {
      dprop->end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("END: %d\n", dprop->end);
#endif
    }
    else if((!strcasecmp(prop->name, "CLIP-END")) || (!strcasecmp(prop->name, "CLIPEND"))) {
      dprop->clip_end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("CLIP-END: %d\n", dprop->clip_end);
#endif
    }
    else if(!strcasecmp(prop->name, "DUR")) {
      dprop->duration = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
      palign;
      printf("DURATION: %d\n", dprop->duration);
#endif
    }
    else if(!strcasecmp(prop->name, "HREF")) {

      SAFE_FREE(dprop->anchor);

      dprop->anchor = strdup(prop->value);

#ifdef DEBUG_SMIL
      palign;
      printf("HREF: %s\n", dprop->anchor);
#endif

    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}

static void smil_repeat (int times, smil_node_t *from, smil_node_t **snode) {
  int  i;

  if(times > 1) {
    smil_node_t *node, *to;

    to = (*snode);

    for(i = 0; i < (times - 1); i++) {
      int found = 0;

      node = from;

      while(!found) {
	smil_node_t *nnode = smil_new_node();

	nnode->mmk  = smil_duplicate_mmk(node->mmk);

	(*snode)->next = nnode;
	(*snode) = nnode;

	if(node == to)
	  found = 1;

	node = node->next;
      }
    }
  }
  else {
    smil_node_t *nnode = smil_new_node();

    (*snode)->next = nnode;
    (*snode) = (*snode)->next;
  }
}

static void smil_properties(smil_t *smil, smil_node_t **snode,
			    xml_property_t *props, smil_property_t *sprop) {
  xml_property_t  *prop;
  int              gotmrl = 0;

#ifdef DEBUG_SMIL
  offset += 2;
#endif
  for(prop = props; prop; prop = prop->next) {

#ifdef DEBUG_SMIL
    palign;
    printf("%s(): prop_name '%s', value: '%s'\n", __XINE_FUNCTION__,
	   prop->name, prop->value ? prop->value : "<NULL>");
#endif

    if(prop->name) {
      if((!strcasecmp(prop->name, "SRC")) || (!strcasecmp(prop->name, "HREF"))) {

	gotmrl++;

	if((*snode)->mmk == NULL)
	  (*snode)->mmk = smil_new_mediamark();

	/*
	  handle BASE, ANCHOR
	*/
	if(sprop && sprop->anchor)
	  (*snode)->mmk->mrl = strdup(sprop->anchor);
	else {
            size_t l1 = smil->base ? _find_byte (smil->base, 0) : 0, l2 = _find_byte (prop->value, 0) + 1;
          char *p = malloc (l1 + l2 + 1);
          (*snode)->mmk->mrl = p;
          if (p) {
            if (l1 && !strstr (prop->value, "://")) {
              memcpy (p, smil->base, l1); p += l1;
              if (p[-1] != '/')
                *p++ = '/';
            }
            memcpy (p, prop->value, l2);
          }
	}
      }
      else if(!strcasecmp(prop->name, "TITLE")) {

	gotmrl++;

	if((*snode)->mmk == NULL) {
	  (*snode)->mmk = smil_new_mediamark();
	}

	(*snode)->mmk->ident = strdup(prop->value);
      }
      else if(!strcasecmp(prop->name, "REPEAT")) {
	(*snode)->prop.repeat = strtol(prop->value, &prop->value, 10);
      }
      else if(!strcasecmp(prop->name, "BEGIN")) {
	(*snode)->prop.begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("begin: %d\n",(*snode)->prop.begin);
#endif
      }
      else if((!strcasecmp(prop->name, "CLIP-BEGIN")) || (!strcasecmp(prop->name, "CLIPBEGIN"))) {
	(*snode)->prop.clip_begin = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("clip-begin: %d\n",(*snode)->prop.clip_begin);
#endif
      }
      else if(!strcasecmp(prop->name, "END")) {
	(*snode)->prop.end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("end: %d\n",(*snode)->prop.end);
#endif
      }
      else if((!strcasecmp(prop->name, "CLIP-END")) || (!strcasecmp(prop->name, "CLIPEND"))) {
	(*snode)->prop.clip_end = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	printf("clip-end: %d\n",(*snode)->prop.clip_end);
#endif
      }
      else if(!strcasecmp(prop->name, "DUR")) {
	(*snode)->prop.duration = smil_get_time_in_seconds(prop->value);
#ifdef DEBUG_SMIL
	palign;
	printf("DURATION: %d\n", (*snode)->prop.duration);
#endif
      }
    }
  }

  if(gotmrl) {
    smil_node_t *node = (*snode);
    int          repeat = node->prop.repeat;

    if(sprop) {
      if(sprop->clip_begin && (node->prop.clip_begin == 0))
	node->mmk->start = sprop->clip_begin;
      else
	node->mmk->start = node->prop.clip_begin;

      if(sprop->clip_end && (node->prop.clip_end == -1))
	node->mmk->end = sprop->clip_end;
      else
	node->mmk->end = node->prop.clip_end;

      if(sprop->duration && (node->prop.duration == 0))
	node->mmk->end = node->mmk->start + sprop->duration;
      else
	node->mmk->end = node->mmk->start + node->prop.duration;

    }
    else {
      node->mmk->start = node->prop.clip_begin;
      node->mmk->end = node->prop.clip_end;
      if(node->prop.duration)
	node->mmk->end = node->mmk->start + node->prop.duration;
    }

    smil_repeat ((repeat <= 1) ? 1 : repeat, node, snode);
  }

#ifdef DEBUG_SMIL
  palign;
  printf("---\n");
  offset -= 2;
#endif
}

/*
 * Sequence playback
 */
static void smil_seq (smil_t *smil, smil_node_t **snode, xml_node_t *node) {
  xml_node_t      *seq;

#ifdef DEBUG_SMIL
  offset += 2;
#endif

  for(seq = node; seq; seq = seq->next) {

    if(!strcasecmp(seq->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("seq NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_seq(smil, snode, seq->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(seq->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("seq NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_par (smil, snode, seq->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(seq->name, "SWITCH")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("seq NEW SWITCH\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, seq->props);
      smil_switch (smil, snode, seq->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(seq->name, "VIDEO")) ||
	    (!strcasecmp(seq->name, "AUDIO")) ||
	    (!strcasecmp(seq->name, "A"))) {
      smil_property_t  smil_props;

      smil_init_smil_property(&smil_props);

      if(seq->child) {
	xml_node_t *child = seq->child;

	while(child) {

	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }

	  child = child->next;
	}
      }

      smil_properties(smil, snode, seq->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}
/*
 * Parallel playback
 */
static void smil_par (smil_t *smil, smil_node_t **snode, xml_node_t *node) {
  xml_node_t      *par;

#ifdef DEBUG_SMIL
  offset += 2;
#endif

  for(par = node; par; par = par->next) {

    if(!strcasecmp(par->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("par NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_seq(smil, snode, par->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(par->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("par NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_par (smil, snode, par->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(par->name, "SWITCH")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("par NEW SWITCH\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, par->props);
      smil_switch (smil, snode, par->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(par->name, "VIDEO")) ||
	    (!strcasecmp(par->name, "AUDIO")) ||
	    (!strcasecmp(par->name, "A"))) {
      smil_property_t  smil_props;

      smil_init_smil_property(&smil_props);

      if(par->child) {
	xml_node_t *child = par->child;

	while(child) {
	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }
	  child = child->next;
	}
      }

      smil_properties(smil, snode, par->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }

#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}
static void smil_switch (smil_t *smil, smil_node_t **snode, xml_node_t *node) {
  xml_node_t      *nswitch;

#ifdef DEBUG_SMIL
  offset += 2;
#endif

  for(nswitch = node; nswitch; nswitch = nswitch->next) {

    if(!strcasecmp(nswitch->name, "SEQ")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("switch NEW SEQ\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, nswitch->props);
      smil_seq(smil, snode, nswitch->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if(!strcasecmp(nswitch->name, "PAR")) {
      smil_property_t  smil_props;
      smil_node_t     *node = (*snode);

      smil_init_smil_property(&smil_props);

#ifdef DEBUG_SMIL
      offset += 2;
      palign;
      printf("switch NEW PAR\n");
      offset -= 2;
#endif

      smil_get_properties(&smil_props, nswitch->props);
      smil_par (smil, snode, nswitch->child);
      smil_repeat (smil_props.repeat, node, snode);
      smil_free_properties(&smil_props);
    }
    else if((!strcasecmp(nswitch->name, "VIDEO")) ||
	    (!strcasecmp(nswitch->name, "AUDIO")) ||
	    (!strcasecmp(nswitch->name, "A"))) {
      smil_property_t  smil_props;

      smil_init_smil_property(&smil_props);

      if(nswitch->child) {
	xml_node_t *child = nswitch->child;

	while(child) {

	  if(!strcasecmp(child->name, "ANCHOR")) {
#ifdef DEBUG_SMIL
	    palign;
	    printf("ANCHOR\n");
#endif
	    smil_get_properties(&smil_props, child->props);
	  }

	  child = child->next;
	}
      }

      smil_properties(smil, snode, nswitch->props, &smil_props);
      smil_free_properties(&smil_props);
    }
  }
#ifdef DEBUG_SMIL
  offset -= 2;
#endif
}

static int smil_fill_mmk (_lf_t *lf, smil_t *smil) {
  smil_node_t *node;
  int num = 0;

  for (node = smil->first; node; node = node->next) {
    if (node->mmk) {
      _lf_add2 (lf, node->mmk);
      node->mmk = NULL;
      num += 1;
    }
  }

  return num;
}

static void smil_free_smil(smil_t *smil) {
  smil_node_t *node, *next;

  for (node = smil->first; node; node = next) {
    next = node->next;
    smil_free_properties (&node->prop);
    free (node);
  }

  SAFE_FREE(smil->title);
  SAFE_FREE(smil->author);
  SAFE_FREE(smil->copyright);
  SAFE_FREE(smil->year);
  SAFE_FREE(smil->base);
}

static void xml_smil_playlist (_lf_t *lf, xml_node_t *xml_tree) {
  if(xml_tree) {
    xml_node_t      *smil_entry, *smil_ref;
    smil_t           smil;
    int              entries_smil = 0;

    memset(&smil, 0, sizeof(smil_t));

    if(!strcasecmp(xml_tree->name, "SMIL")) {

      smil.first = smil.node = smil_new_node();
      smil_entry = xml_tree->child;

      while(smil_entry) {

#ifdef DEBUG_SMIL
	printf("smil_entry '%s'\n", smil_entry->name);
#endif
	if(!strcasecmp(smil_entry->name, "HEAD")) {
#ifdef DEBUG_SMIL
	  printf("+---------+\n");
	  printf("| IN HEAD |\n");
	  printf("+---------+\n");
#endif
	  smil_ref = smil_entry->child;
	  while(smil_ref) {
#ifdef DEBUG_SMIL
	    printf("  smil_ref '%s'\n", smil_ref->name);
#endif
	    smil_header(&smil, smil_ref->props);

	    smil_ref = smil_ref->next;
	  }
	}
	else if(!strcasecmp(smil_entry->name, "BODY")) {
#ifdef DEBUG_SMIL
	  printf("+---------+\n");
	  printf("| IN BODY |\n");
	  printf("+---------+\n");
#endif
	__kino:
	  smil_ref = smil_entry->child;
	  while(smil_ref) {
#ifdef DEBUG_SMIL
	    printf("  smil_ref '%s'\n", smil_ref->name);
#endif
	    if(!strcasecmp(smil_ref->name, "SEQ")) {
	      smil_property_t  smil_props;
	      smil_node_t     *node = smil.node;

#ifdef DEBUG_SMIL
	      palign;
	      printf("SEQ Found\n");
#endif
	      smil_init_smil_property(&smil_props);

	      smil_get_properties(&smil_props, smil_ref->props);
	      smil_seq(&smil, &(smil.node), smil_ref->child);
              smil_repeat (smil_props.repeat, node, &(smil.node));
	      smil_free_properties(&smil_props);
	    }
	    else if(!strcasecmp(smil_ref->name, "PAR")) {
	      smil_property_t  smil_props;
	      smil_node_t     *node = smil.node;

#ifdef DEBUG_SMIL
	      palign;
	      printf("PAR Found\n");
#endif

	      smil_init_smil_property(&smil_props);

	      smil_get_properties(&smil_props, smil_ref->props);
              smil_par (&smil, &(smil.node), smil_ref->child);
              smil_repeat (smil_props.repeat, node, &(smil.node));
	      smil_free_properties(&smil_props);
	    }
	    else if(!strcasecmp(smil_ref->name, "A")) {
#ifdef DEBUG_SMIL
	      palign;
	      printf("  IN A\n");
#endif
	      smil_properties(&smil, &(smil.node), smil_ref->props, NULL);
	    }
	    else if(!strcasecmp(smil_ref->name, "SWITCH")) {
	      smil_property_t  smil_props;
	      smil_node_t     *node = smil.node;

#ifdef DEBUG_SMIL
	      palign;
	      printf("SWITCH Found\n");
#endif

	      smil_init_smil_property(&smil_props);

	      smil_get_properties(&smil_props, smil_ref->props);
	      smil_switch (&smil, &(smil.node), smil_ref->child);
              smil_repeat (smil_props.repeat, node, &(smil.node));
	      smil_free_properties(&smil_props);
	    }
	    else {
	      smil_properties(&smil, &smil.node, smil_ref->props, NULL);
	    }

	    smil_ref = smil_ref->next;
	  }
	}
	else if(!strcasecmp(smil_entry->name, "SEQ")) { /* smil2, kino format(no body) */
	  goto __kino;
	}

	smil_entry = smil_entry->next;
      }

#ifdef DEBUG_SMIL
      printf("DUMPING TREE:\n");
      printf("-------------\n");
      smil_dump_tree(smil.first);

      printf("DUMPING HEADER:\n");
      printf("---------------\n");
      smil_dump_header(&smil);
#endif

      entries_smil = smil_fill_mmk (lf, &smil);
      smil_free_smil(&smil);
    }
#ifdef DEBUG
    else
      fprintf(stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif

    if (entries_smil) {
      lf->num_entries = entries_smil;
      lf->type    = "SMIL";
    }
  }
}
/*
 * ********************************** SMIL END ***********************************
 */

static void xml_freevo_playlist (_lf_t *lf, xml_node_t *xml_tree) {
  int entries_fvo = 0;

  if (!strcasecmp (xml_tree->name, "FREEVO")) {
    mediamark_t m = { .end = -1, .from = MMK_FROM_PLAYLIST };
    xml_node_t *fvo_entry;

    for (fvo_entry = xml_tree->child; fvo_entry; fvo_entry = fvo_entry->next) {
      if (!strcasecmp (fvo_entry->name, "MOVIE")) {
        char *url = NULL, *sub = NULL, *title = NULL;
        xml_node_t *sentry;
        xml_property_t *fvo_prop;

        for (fvo_prop = fvo_entry->props; fvo_prop; fvo_prop = fvo_prop->next) {
          if (!strcasecmp (fvo_prop->name, "TITLE"))
            title = fvo_prop->value;
        }
        for (sentry = fvo_entry->child; sentry; sentry = sentry->next) {
          if ((!strcasecmp (sentry->name, "VIDEO")) || (!strcasecmp (sentry->name, "AUDIO"))) {
            xml_node_t *ssentry;

            for (ssentry = sentry->child; ssentry; ssentry = ssentry->next) {
              if (!strcasecmp (ssentry->name, "SUBTITLE")) {
                sub = ssentry->data;
              } else if (!strcasecmp (ssentry->name, "URL") || !strcasecmp (ssentry->name, "DVD") ||
                !strcasecmp (ssentry->name, "VCD") || !strcasecmp (ssentry->name, "FILE")) {
                url = ssentry->data;
              }
            }
            if (url) {
              m.mrl = url;
              m.ident = title;
              m.sub = sub;
              _lf_add (lf, entries_fvo, &m);
              lf->num_entries = ++entries_fvo;
              url = NULL;
            }
            sub = NULL;
          }
        }
        title = NULL;
      }
    }
  }
#ifdef DEBUG
  else
    fprintf (stderr, "%s(): Unsupported XML type: '%s'.\n", __XINE_FUNCTION__, xml_tree->name);
#endif
  if (entries_fvo)
    lf->type = "FREEVO";
}

static void guess_xml_based_playlist (_lf_t *lf) {
  char            *xml_content;

  if ((xml_content = _lf_dup (lf)) != NULL) {
    static const playlist_xml_guess_func_t guess_functions[] = {
      xml_asx_playlist,
      xml_gx_playlist,
      xml_noatun_playlist,
      xml_smil_playlist,
      xml_freevo_playlist,
      NULL
    };
    xml_node_t *xml_tree = NULL;

    xml_parser_init_R (xml_parser_t *xml, xml_content, lf->size, XML_PARSER_CASE_INSENSITIVE);
    if ((xml_parser_build_tree_R (xml, &xml_tree) == XML_PARSER_OK) && xml_tree) {
      int i;
      /* Check all playlists */
      for (i = 0; guess_functions[i]; i++) {
        guess_functions[i] (lf, xml_tree);
        if (lf->type[0])
          break;
      }
      xml_parser_free_tree (xml_tree);
    }
    xml_parser_finalize_R (xml);
  }
}
/*
 * XML based playlists end.
 */

/*
 * Public
 */
int mediamark_get_entry_from_id (gGui_t *gui, const char *ident) {
  if(ident && gui->playlist.num) {
    int i;

    for(i = 0; i < gui->playlist.num; i++) {
      if(!strcasecmp(ident, gui->playlist.mmk[i]->ident))
	return i;
    }
  }
  return -1;
}

int gui_playlist_insert (gGui_t *gui, int index, const char *mrl, const char *ident,
			    const char *sub, int start, int end, int av_offset, int spu_offset) {
  char  autosub[2*XITK_PATH_MAX + XITK_NAME_MAX + 2];
  char  subpath[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  DIR           *dir;
  struct dirent *dentry;

  if (!mrl)
    return -1;
  if (!mrl[0])
    return -1;

  gui_playlist_lock (gui);

  if ((index < 0) || (index >= gui->playlist.num))
    index = gui->playlist.num;

  if (gui->playlist.num + 1 > gui->playlist.max) {
    int i;
    mediamark_t **mmk = realloc (gui->playlist.mmk, sizeof (*mmk) * (gui->playlist.num + 32));
    if (!mmk)
      return -1;
    gui->playlist.mmk = mmk;
    gui->playlist.max += 32;
    for (i = gui->playlist.num; i < gui->playlist.max; i++)
      mmk[i] = NULL;
  }

  {
    int i;
    for (i = gui->playlist.num; i > index; i--)
      gui->playlist.mmk[i] = gui->playlist.mmk[i - 1];
  }
  gui->playlist.mmk[index] = NULL;
  gui->playlist.num++;

  /*
   * If subtitle_autoload is enabled and subtitle is NULL
   * then try to see if a matching subtitle exist
   */
  if(mrl && (!sub) && gui->subtitle_autoload) {

    if (mrl_look_like_file (mrl)) {
      char        *know_subs = "sub,srt,asc,smi,ssa,ass,txt";
      char        *vsubs, *pvsubs;
      char        *_mrl, *ending, *ext, *path, *d_name;
      struct stat  pstat;

      _mrl = (char *) mrl;
      if(!strncasecmp(_mrl, "file:", 5))
	_mrl += 5;

      strlcpy(autosub, _mrl, sizeof(autosub));

      if((ending = strrchr(autosub, '.')))
	ending++;
      else {
	ending = autosub + _find_byte (autosub, 0);
	*ending++ = '.';
      }

      vsubs = strdup(know_subs);

      pvsubs = vsubs;
      while((ext = xine_strsep(&pvsubs, ",")) && !sub) {
	strcpy(ending, ext);
	*(ending + _find_byte (ext, 0) + 1) = '\0';

	if(((stat(autosub, &pstat)) > -1) && (S_ISREG(pstat.st_mode)) && strcmp(autosub, _mrl))
	  sub = autosub;
      }
      free(vsubs);

      /* try matching "<name>.*.<know_subs>" like used by opensubtitles */
      if( !sub ) {
        *ending = '\0';

        strlcpy(subpath, autosub, sizeof(subpath));

        if((d_name = strrchr(subpath, '/'))) {
          *d_name++ = '\0';
          path = subpath;
        } else {
          d_name = subpath;
          path = ".";
        }

        if((dir = opendir(path))) {
          while((dentry = readdir(dir))) {
            if( (strncmp(dentry->d_name, d_name, _find_byte (d_name, 0)) == 0) &&
                (ending = strrchr(dentry->d_name, '.')) ) {

              if( strstr(know_subs, ending+1) ) {
                snprintf(autosub,sizeof(autosub),"%s/%s",path,dentry->d_name);
                autosub[sizeof(autosub)-1]='\0';
                if(((stat(autosub, &pstat)) > -1) && (S_ISREG(pstat.st_mode)) && strcmp(autosub, _mrl)) {
                  sub = autosub;
                  break;
                }
              }
            }
          }
          closedir(dir);
        }
      }
    }
  }

  {
    mediamark_t m = {
      .mrl = (char *)mrl, /** << will not be written to. */
      .ident = (char *)ident,
      .sub = (char *)sub,
      .start = start,
      .end = end,
      .type = MMK_TYPE_FILE,
      .from = MMK_FROM_USER,
      .av_offset = av_offset,
      .spu_offset = spu_offset
    };
    mediamark_copy (&gui->playlist.mmk[index], &m);
  }

  gui_playlist_unlock (gui);
  return index;
}

int gui_playlist_move (gGui_t *gui, int index, int n, int diff) {
  int res;

  gui_playlist_lock (gui);
  do {
    mediamark_t *stemp[32], **temp = stemp;

    res = GUI_MMK_NONE;
    if (gui->playlist.num <= 0)
      break;
    if (index == GUI_MMK_CURRENT)
      index = gui->playlist.cur;
    if ((index < 0) || (index >= gui->playlist.num))
      break;
    res = index;
    if (n > gui->playlist.num - index)
      n = gui->playlist.num - index;
    if (diff == 0)
      break;

    if (diff < 0) {
      diff = -diff;
      if (diff > index) {
        diff = index;
        if (diff <= 0)
          break;
      }
      if (diff > 32) {
        temp = malloc (diff * sizeof (*temp));
        if (!temp)
          break;
      }
      if (gui->playlist.cur < index) {
        if (gui->playlist.cur >= index - diff)
          gui->playlist.cur += n;
      } else {
        if (gui->playlist.cur < index + n)
          gui->playlist.cur -= diff;
      }
      memcpy (temp, gui->playlist.mmk + index - diff, diff * sizeof (*temp));
      memmove (gui->playlist.mmk + index - diff, gui->playlist.mmk + index, n * sizeof (*temp));
      memcpy (gui->playlist.mmk + index - diff + n, temp, diff * sizeof (*temp));
      res = index - diff;
    } else /* diff > 0 */ {
      if (diff > gui->playlist.num - index - n) {
        diff = gui->playlist.num - index - n;
        if (diff <= 0)
          break;
      }
      if (diff > 32) {
        temp = malloc (diff * sizeof (*temp));
        if (!temp)
          break;
      }
      if (gui->playlist.cur < index + n) {
        if (gui->playlist.cur >= index)
          gui->playlist.cur += diff;
      } else {
        if (gui->playlist.cur < index + n + diff)
          gui->playlist.cur -= n;
      }
      memcpy (temp, gui->playlist.mmk + index + n, diff * sizeof (*temp));
      memmove (gui->playlist.mmk + index + diff, gui->playlist.mmk + index, n * sizeof (*temp));
      memcpy (gui->playlist.mmk + index, temp, diff * sizeof (*temp));
      res = index + diff;
    }

    if (temp != stemp)
      free (temp);
  } while (0);
  gui_playlist_unlock (gui);
  return res;
}

void gui_playlist_free (gGui_t *gui) {
  gui_playlist_lock (gui);
  if(gui->playlist.num > 0) {
    int i;

    for (i = 0; i < gui->playlist.num; i++)
      mediamark_free(&gui->playlist.mmk[i]);

    SAFE_FREE(gui->playlist.mmk);
    gui->playlist.max = 0;
    gui->playlist.num = 0;
    gui->playlist.cur = -1;
  }
  gui_playlist_unlock (gui);
}

void mediamark_reset_played_state (gGui_t *gui) {
  if(gui->playlist.num) {
    int i;

    for(i = 0; i < gui->playlist.num; i++)
      gui->playlist.mmk[i]->played = 0;
  }
}

int mediamark_all_played (gGui_t *gui) {
  if(gui->playlist.num) {
    int i;

    for(i = 0; i < gui->playlist.num; i++) {
      if(gui->playlist.mmk[i]->played == 0)
	return 0;
    }
  }

  return 1;
}

int mediamark_get_shuffle_next (gGui_t *gui) {
  int  next = 0;

  if(gui->playlist.num >= 3 && gui->playlist.cur >= 0) {
    int    remain = gui->playlist.num;
    int    entries[remain];
    float  num = (float) gui->playlist.num;
    int    found = 0;

    memset(&entries, 0, sizeof(int) * remain);

    srandom((unsigned int)time(NULL));
    entries[gui->playlist.cur] = 1;

    do {
      next = (int) (num * random() / RAND_MAX);

      if(next != gui->playlist.cur) {

	if(gui->playlist.mmk[next]->played == 0)
	  found = 1;
	else if(entries[next] == 0) {
	  entries[next] = 1;
	  remain--;
	}

	if((!found) && (!remain))
	  found = 2; /* No more choice */

      }

    } while(!found);

    if(found == 2)
      next = gui->playlist.cur;

  }
  else if(gui->playlist.num == 2)
    next = !gui->playlist.cur;

  return next;
}

int gui_playlist_remove (gGui_t *gui, int index) {
  int i;
  gui_playlist_lock (gui);

  if (index == GUI_MMK_CURRENT)
    index = gui->playlist.cur;

  if ((index < gui->playlist.num) && (index >= 0) && gui->playlist.mmk) {
    mediamark_free (&gui->playlist.mmk[index]);
    for (i = index; i < gui->playlist.num - 1; i++)
      gui->playlist.mmk[i] = gui->playlist.mmk[i + 1];
    gui->playlist.num--;
    gui->playlist.mmk[gui->playlist.num] = NULL;
    if (gui->playlist.cur >= gui->playlist.num)
      gui->playlist.cur = -1;
  }

  i = gui->playlist.num;
  gui_playlist_unlock (gui);
  return i;
}

static void _gui_playlist_add_lf (gGui_t *gui, _lf_t *lf) {
  if (gui->playlist.num + (int)lf->used > gui->playlist.max) {
    int nmax = (gui->playlist.num + lf->used + 32) & ~31;
    mediamark_t **nmmk = realloc (gui->playlist.mmk, nmax * sizeof (*nmmk));
    if (nmmk) {
      gui->playlist.mmk = nmmk;
      gui->playlist.max = nmax;
    }
  }
  {
    uint32_t n = gui->playlist.max - gui->playlist.num, i, t;
    if (n > lf->used)
      n = lf->used;
    for (t = gui->playlist.num, i = 0; i < n; i++) {
      if (!lf->mmk[i])
        continue;
      lf->mmk[i]->from = MMK_FROM_PLAYLIST;
      if (!lf->mmk[i]->ident) {
        char *start = lf->mmk[i]->mrl;
        if (start[0] == '/') {
          char *lastpart;
          for (lastpart = start + _find_byte (start, 0); lastpart[-1] != '/'; lastpart--) ;
          lf->mmk[i]->ident = strdup (lastpart);
        }
      }
      if (!lf->mmk[i]->ident)
        lf->mmk[i]->ident = lf->mmk[i]->mrl;
      gui->playlist.mmk[t++] = lf->mmk[i];
    }
    gui->playlist.num = t;
    for (; i < lf->used; i++)
      mediamark_free (&lf->mmk[i]);
  }
}

static int _gui_playlist_add_playlist (gGui_t *gui, mrl_buf_t *name, int replace) {
  _lf_t *lf;
  size_t i;
  static const playlist_guess_func_t guess_functions[] = {
    guess_xml_based_playlist,
    guess_toxine_playlist,
    guess_pls_playlist,
    guess_m3u_playlist,
    guess_sfv_playlist,
    guess_raw_playlist,
    NULL
  };

  lf = _lf_get (gui, name);
  if (!lf)
    return 0;

  for (i = 0; guess_functions[i]; i++) {
    guess_functions[i] (lf);
    if (lf->type[0])
      break;
  }

  if (lf->mmk) {
#ifdef DEBUG
    printf("Playlist file (%s) is valid (%s).\n", name->start, lf->type);
#endif
  }
  else {
    fprintf (stderr, _("Playlist file (%s) is invalid.\n"), name->start);
    _lf_delete (lf);
    return 0;
  }

  {
    int n;
    gui_playlist_lock (gui);
    if (replace) {
      for (n = 0; n < gui->playlist.num; n++)
        mediamark_free (gui->playlist.mmk + i);
      gui->playlist.num = 0;
    }
    n = gui->playlist.num;
    _gui_playlist_add_lf (gui, lf);
    if (replace)
      gui->playlist.cur = (gui->playlist.loop == PLAYLIST_LOOP_SHUFFLE) ? mediamark_get_shuffle_next (gui) : 0;
    else if ((gui->playlist.cur < 0) && (gui->playlist.num > n))
      gui->playlist.cur = n;
    n = gui->playlist.num - n;
    gui_playlist_unlock (gui);
    _lf_delete (lf);
    return n;
  }
}

static char *_int2str (char *buf, int value) {
  unsigned int minus, u;

  if (value >= 0)
    minus = 0, u = value;
  else
    minus = 1, u = -value;
  do {
    *--buf = (u % 10u) + '0';
    u /= 10u;
  } while (u);
  if (minus)
    *--buf = '-';
  return buf;
}

void gui_playlist_save (gGui_t *gui, const char *filename) {
  mrl_buf_t base, item, full;
  FILE *fd;
  int i;

  gui_playlist_lock (gui);
  i = gui->playlist.num;
  gui_playlist_unlock (gui);
  if (i <= 0)
    return;

  mrl_buf_init (&base);
  mrl_buf_init (&item);
  mrl_buf_init (&full);
  _mrl_buf_working_dir (&base);

  mrl_buf_set (&item, &base, filename);
  mrl_buf_merge (&full, &base, &item);
  if (!_mrl_buf_mkdir_p (&full))
    return;

  fd = fopen (full.root, "wb");
  if (!fd) {
    fprintf (stderr, _("Unable to save playlist (%s): %s.\n"), filename, strerror (errno));
    return;
  }

  fwrite ("# toxine playlist\n", 1, 18, fd);

  gui_playlist_lock (gui);
  for (i = 0; i < gui->playlist.num; i++) {
    char buf[4 * 32], *p;

    fwrite ("\nentry {\n\tidentifier = ", 1, 23, fd);
    fwrite (gui->playlist.mmk[i]->ident, 1, _find_byte (gui->playlist.mmk[i]->ident, 0), fd);
    mrl_buf_set (&item, &base, gui->playlist.mmk[i]->mrl);
    mrl_buf_merge (&full, &base, &item);
    fwrite (";\n\tmrl = ", 1, 9, fd);
    fwrite (full.start, 1, full.end - full.start, fd);
    if (gui->playlist.mmk[i]->sub) {
      mrl_buf_set (&item, &base, gui->playlist.mmk[i]->sub);
      mrl_buf_merge (&full, &base, &item);
      fwrite (";\n\tsubtitle = ", 1, 14, fd);
      fwrite (full.start, 1, full.end - full.start, fd);
    }
    p = buf + sizeof (buf);
    p -= 3; memcpy (p, "};\n", 3);
    if (gui->playlist.mmk[i]->spu_offset != 0) {
      p -= 2; memcpy (p, ";\n", 2);
      p = _int2str (p, gui->playlist.mmk[i]->spu_offset);
      p -= 14; memcpy (p, "\tspu_offset = ", 14);
    }
    if (gui->playlist.mmk[i]->av_offset != 0) {
      p -= 2; memcpy (p, ";\n", 2);
      p = _int2str (p, gui->playlist.mmk[i]->av_offset);
      p -= 13; memcpy (p, "\tav_offset = ", 13);
    }
    if (gui->playlist.mmk[i]->end != -1) {
      p -= 2; memcpy (p, ";\n", 2);
      p = _int2str (p, gui->playlist.mmk[i]->end);
      p -= 7; memcpy (p, "\tend = ", 7);
    }
    if (gui->playlist.mmk[i]->start > 0) {
      p -= 2; memcpy (p, ";\n", 2);
      p = _int2str (p, gui->playlist.mmk[i]->start);
      p -= 9; memcpy (p, "\tstart = ", 9);
    }
    p -= 2; memcpy (p, ";\n", 2);
    fwrite (p, 1, buf + sizeof (buf) - p, fd);
  }
  gui_playlist_unlock (gui);

  fwrite ("# END\n", 1, 6, fd);

  fclose (fd);
#ifdef DEBUG
  printf ("Playlist '%s' saved.\n", filename);
#endif
}

size_t mrl_get_lowercase_prot (char *buf, size_t bsize, const char *mrl) {
  union {uint8_t s[16]; uint32_t v[4];} _buf;
  uint8_t *p = (uint8_t *)mrl, *s = _buf.s, *e = s + 14;
  uint32_t u;

  if (!p)
    return 0;

  while ((*p >= 'A') && (s < e))
    *s++ = *p++;
  if (p[0] != ':')
    return 0;
  if (p[1] != '/')
    return 0;
  for (u = 0; u < 4; u++)
    _buf.v[u] |= 0x20202020;
  s[0] = 0;
  u = s - _buf.s;

  if (buf && (bsize > u))
    memcpy (buf, _buf.s, u + 1);
  return u;
}

int mrl_look_like_file (const char *mrl) {
  char buf[16];
  size_t l = mrl_get_lowercase_prot (buf, sizeof (buf), mrl);

  if (l == 0)
    return 1;
  return (l == 4) && !memcmp (buf, "file", 4) ? 1 : 0;
}

static int _gui_mmk_cmp (void *a, void *b) {
  mediamark_t *d = (mediamark_t *)a;
  mediamark_t *e = (mediamark_t *)b;

  return strcmp (d->ident, e->ident);
}

static void _gui_mmk_sort (xine_sarray_t *sarray, mediamark_t **list, uint32_t n) {
  uint32_t u;

  if (!sarray)
    return;

  xine_sarray_clear (sarray);
  for (u = 0; u < n; u++)
    xine_sarray_add (sarray, list[u]);
  for (u = 0; u < n; u++)
    list[u] = xine_sarray_get (sarray, u);
}

static int _gui_playlist_dupl_cmp (void *a, void *b) {
  mediamark_t *d = (mediamark_t *)a;
  mediamark_t *e = (mediamark_t *)b;

  return strcmp (d->mrl + d->start, e->mrl + e->start);
}

int gui_playlist_add_item (gGui_t *gui, const char *filepathname, int max_levels, gui_item_type_t itemtype, int replace) {
  mrl_buf_t base, name, *mrlb;
  DIR *dirs[GUI_MAX_DIR_LEVELS];
  char *add[GUI_MAX_DIR_LEVELS];
  int num_subdirs[GUI_MAX_DIR_LEVELS];
  xine_sarray_t *sarray, *dupl;
  struct stat sbuf;
  char *start, *end;
  int n, level, sort_start = 0, all_files = 0, old_num;
  int type;

  if (max_levels < 0) {
    max_levels = -max_levels;
    all_files = 1;
  }
  if (max_levels > GUI_MAX_DIR_LEVELS)
    max_levels = GUI_MAX_DIR_LEVELS;

  mrl_buf_init (&name);
  mrl_buf_set (&name, NULL, filepathname);
  if (mrl_buf_is_file (&name)) {
    if (name.root[0] != '/') {
      mrl_buf_init (&base);
      _mrl_buf_working_dir (&base);
      mrl_buf_merge (&base, &base, &name);
      start = base.root;
      add[0] = base.end;
      end = base.max;
      mrlb = &base;
    } else {
      start = name.start;
      add[0] = name.end;
      end = name.max;
      mrlb = &name;
    }
    type = MMK_TYPE_FILE;
    start[-1] = 0;
    if (add[0][-1] == '/')
      (--add[0])[0] = 0;
  } else {
    start = name.start;
    add[0] = name.end;
    end = name.max;
    mrlb = &name;
    type = MMK_TYPE_NET;
  }

  /* add not found item to get the user error msg. */
  n = stat (start, &sbuf);
  if (n || S_ISREG (sbuf.st_mode)) {
    char *lastpart, *ext, *sub;
    uint32_t vext;

    /* test for "/some/dir/video.flv;;/some/dir/subtitiles.txt" */
    start[-2] = start[-1] = ';';
    for (sub = add[0]; sub >= start + 3; sub--) {
      while (sub[-1] != ';')
        sub--;
      if (sub[-2] == ';')
        break;
    }
    if (sub >= start + 3) {
      sub[-2] = 0;
      mrlb->args = mrlb->info = mrlb->end = add[0] = sub - 2;
      start[-1] = '/';
      for (lastpart = add[0]; lastpart[-1] != '/'; lastpart--) ;
      mrlb->lastpart = lastpart;
      lastpart[-1] = '.';
      for (ext = add[0]; ext[-1] != '.'; ext--) ;
      lastpart[-1] = '/';
      if (ext <= lastpart)
        ext = add[0];
      mrlb->ext = ext;
    } else {
      sub = NULL;
      lastpart = mrlb->lastpart;
      ext = mrlb->ext;
    }

    vext = _get_ext_val (ext, add[0] - ext);

    if (max_levels) {
      if (((vext == 0x20202020) && (itemtype == GUI_ITEM_TYPE_PLAYLIST))
        || _is_known_ext (gui->playlist.known_playlist_exts, vext)) {
        int n = _gui_playlist_add_playlist (gui, mrlb, replace);

        if (n > 0)
          return n;
      }
    }

    if (gui_playlist_append (gui, start, lastpart, sub, 0, -1, 0, 0) < 0)
      return 0;
    gui->playlist.mmk[gui->playlist.num - 1]->type = type;
    return 1;
  }

  if (!max_levels)
    return 0;

  sarray = xine_sarray_new (512, _gui_mmk_cmp);
  dupl = xine_sarray_new (512, _gui_playlist_dupl_cmp);
  n = 0;
  level = 0;
  *(add[level])++ = '/';
  dirs[level] = NULL;
  num_subdirs[level] = 0;

  gui_playlist_lock (gui);
  old_num = gui->playlist.num;

  while (1) {
    struct dirent *entry;
    char *stop;
    /* enter pass 1 */
    if (!dirs[level]) {
      sort_start = gui->playlist.num;
      add[level][0] = 0;
      dirs[level] = opendir (start);
      if (!dirs[level]) {
        if (--level < 0)
          break;
        continue;
      }
      num_subdirs[level] = 0;
    }
    /* get entry */
    entry = readdir (dirs[level]);
    /* sort, then enter pass 2 or level up */
    if (!entry) {
      closedir (dirs[level]);
      dirs[level] = NULL;
      if (num_subdirs[level] >= 0) {
        _gui_mmk_sort (sarray, gui->playlist.mmk + sort_start, gui->playlist.num - sort_start);
        if (num_subdirs[level] > 0) {
          add[level][0] = 0;
          dirs[level] = opendir (start);
          if (dirs[level]) {
            num_subdirs[level] = -1;
            continue;
          }
        }
      }
      if (--level < 0)
        break;
      continue;
    }
    /* add entry name */
    stop = add[level] + strlcpy (add[level], entry->d_name, end - add[level]);
    if (stop >= end)
      stop = end;
    /* try to get info */
    stop[0] = 0;
    if (stat (start, &sbuf))
      continue;
    /* dir */
    if (S_ISDIR (sbuf.st_mode)) {
      if (add[level][0] == '.') {
        if (!add[level][1] || ((add[level][1] == '.') && !add[level][2]))
          continue;
      }
      if (num_subdirs[level] >= 0) { /* pass 1: files */
        num_subdirs[level]++;
      } else { /* pass 2: dirs */
        if (level >= max_levels - 1)
          continue;
        add[level + 1] = stop;
        level++;
        *(add[level])++ = '/';
        dirs[level] = NULL;
        num_subdirs[level] = 0;
      }
    } else if (S_ISREG (sbuf.st_mode)) {
      uint32_t vext, type;

      if (num_subdirs[level] < 0)
        continue;

      mrlb->lastpart = add[level];
      mrlb->args = mrlb->info = mrlb->end = stop;
      {
        char *ext;
        add[level][-1] = '.';
        for (ext = stop; ext[-1] != '.'; ext--) ;
        add[level][-1] = '/';
        if (ext <= add[level])
          ext = stop;
        mrlb->ext = ext;
        vext = _get_ext_val (ext, stop - ext);
      }
      type = _is_known_ext (gui->playlist.known_playlist_exts, vext) ? 1 : 0;

      /* playlist file ? */
      if ((level < max_levels - 1) && type) {
        int r = _gui_playlist_add_playlist (gui, mrlb, 0);

        if (r > 0) {
          n += r;
          continue;
        }
      }
      /* want all files ? */
      if (!all_files) {
        /* media file ? */
        if (!type && _is_known_ext (gui->playlist.known_media_exts, vext))
          type = 2;
        if (!type)
          continue;
      }
      if (gui_playlist_append (gui, start, add[level], NULL, add[0] - start, -1, 0, 0) < 0)
        continue;
      gui->playlist.mmk[gui->playlist.num - 1]->from = MMK_FROM_DIR;
      xine_sarray_add (dupl, gui->playlist.mmk[gui->playlist.num - 1]);
      n++;
    }
  }
  {
    int i, t;
    /* we may have got some media files _and_ some playlists referencing them.
     * playlist shall take effect, mark the duplicates with MMK_FROM_USER,
     * which cannot happen here. */
    for (i = old_num; i < gui->playlist.num; i++) {
      if ((gui->playlist.mmk[i]->from == MMK_FROM_PLAYLIST) &&
        !strncmp (gui->playlist.mmk[i]->mrl, start, add[0] - start)) {
        int d_indx, real_start;

        real_start = gui->playlist.mmk[i]->start;
        gui->playlist.mmk[i]->start = add[0] - start;
        d_indx = xine_sarray_binary_search (dupl, gui->playlist.mmk[i]);
        gui->playlist.mmk[i]->start = real_start;

        if (d_indx >= 0) {
          mediamark_t *m = xine_sarray_get (dupl, d_indx);
          m->from = MMK_FROM_USER;
        }
      }
    }
    /* comb out the duplicates, and reset the dupl sort hints */
    for (t = i = old_num; i < gui->playlist.num; i++) {
      if (gui->playlist.mmk[i]->from == MMK_FROM_DIR) {
        gui->playlist.mmk[i]->start = 0;
        gui->playlist.mmk[t++] = gui->playlist.mmk[i];
      } else if (gui->playlist.mmk[i]->from == MMK_FROM_PLAYLIST) {
        gui->playlist.mmk[t++] = gui->playlist.mmk[i];
      } else /* MMK_FROM_USER */ {
        mediamark_free (gui->playlist.mmk + i);
      }
    }
    gui->playlist.num = t;
    n = gui->playlist.num - old_num;
  }

  gui_playlist_unlock (gui);

  xine_sarray_delete (dupl);
  xine_sarray_delete (sarray);
  return n;
}

/** gui mediamark editor window ********************************************************/

static void _mmkedit_exit (xitk_widget_t *w, void *data, int state) {
  xui_mmkedit_t *mmkedit = data;

  (void)w;
  (void)state;
  mmkedit->visible = 0;
  gui_save_window_pos (mmkedit->gui, "mmk_editor", mmkedit->widget_key);
  mmkedit->mmk = NULL;
  xitk_unregister_event_handler (mmkedit->gui->xitk, &mmkedit->widget_key);
  xitk_window_destroy_window (mmkedit->xwin);
  mmkedit->xwin = NULL;
  /* xitk_dlist_init (&mmkedit->widget_list.list); */
  playlist_get_input_focus (mmkedit->gui);
  mmkedit->gui->mmkedit = NULL;
  free (mmkedit);
}

static int mmkedit_event (void *data, const xitk_be_event_t *e) {
  xui_mmkedit_t *mmkedit = data;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      _mmkedit_exit (NULL, mmkedit, 0);
      return 1;
    case XITK_EV_KEY_DOWN:
      if ((e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE)) {
        _mmkedit_exit (NULL, mmkedit, 0);
        return 1;
      }
    default: ;
  }
  return gui_handle_be_event (mmkedit->gui, e);
}

void mmk_editor_raise_window (gGui_t *gui) {
  if (gui && gui->mmkedit)
    raise_window (gui, gui->mmkedit->xwin, gui->mmkedit->visible, 1);
}

void mmk_editor_toggle_visibility (gGui_t *gui) {
  if (gui && gui->mmkedit)
    toggle_window (gui, gui->mmkedit->xwin, gui->mmkedit->widget_list, &gui->mmkedit->visible, 1);
}

void mmk_editor_end (gGui_t *gui) {
  if (gui && gui->mmkedit)
    _mmkedit_exit (NULL, gui->mmkedit, 0);
}

static void _mmkedit_set_mmk (xui_mmkedit_t *mmkedit, mediamark_t **mmk) {
  mmkedit->mmk = mmk;
  xitk_inputtext_change_text (mmkedit->mrl, (*mmk)->mrl);
  xitk_inputtext_change_text (mmkedit->ident, (*mmk)->ident);
  xitk_inputtext_change_text (mmkedit->sub, (*mmk)->sub);
  xitk_intbox_set_value (mmkedit->start, (*mmk)->start);
  xitk_intbox_set_value (mmkedit->end, (*mmk)->end);
  xitk_intbox_set_value (mmkedit->av_offset, (*mmk)->av_offset);
  xitk_intbox_set_value (mmkedit->spu_offset, (*mmk)->spu_offset);
}

void mmk_editor_set_mmk (gGui_t *gui, mediamark_t **mmk) {
  if (gui && gui->mmkedit)
    _mmkedit_set_mmk (gui->mmkedit, mmk);
}

static void _mmkedit_apply (xitk_widget_t *w, void *data, int state) {
  xui_mmkedit_t *mmkedit = data;

  (void)w;
  (void)state;
  if (mmkedit->mmk) {
    const char *r, *sub;
    char *ident, *mrl;
    int start, end, av_offset, spu_offset;

    r = xitk_inputtext_get_text (mmkedit->mrl);
    if (r && r[0]) {
      mrl = strdup (r);
      str_unquote (mrl);
    } else {
      mrl = strdup ((*mmkedit->mmk)->mrl);
    }

    r = xitk_inputtext_get_text (mmkedit->ident);
    if (r && r[0]) {
      ident = strdup (r);
      str_unquote (ident);
    } else {
      ident = strdup (mrl);
    }

    sub = xitk_inputtext_get_text (mmkedit->sub);
    if (sub && (!sub[0]))
      sub = NULL;

    if ((start = xitk_intbox_get_value (mmkedit->start)) < 0)
      start = 0;

    if ((end = xitk_intbox_get_value (mmkedit->end)) <= -1)
      end = -1;
    else if (end < start)
      end = start + 1;

    av_offset  = xitk_intbox_get_value (mmkedit->av_offset);
    spu_offset = xitk_intbox_get_value (mmkedit->spu_offset);

    mediamark_set_str_val (mmkedit->mmk, mrl, MMK_VAL_MRL);
    mediamark_set_str_val (mmkedit->mmk, ident, MMK_VAL_IDENT);
    mediamark_set_str_val (mmkedit->mmk, sub, MMK_VAL_SUB);
    (*mmkedit->mmk)->start = start;
    (*mmkedit->mmk)->end   = end;
    (*mmkedit->mmk)->av_offset = av_offset;
    (*mmkedit->mmk)->spu_offset = spu_offset;

    if (mmkedit->callback)
      mmkedit->callback (mmkedit->user_data);

    free (mrl);
    free (ident);
  }
}

static void _mmkedit_ok (xitk_widget_t *w, void *data, int state) {
  xui_mmkedit_t *mmkedit = (xui_mmkedit_t *)data;

  (void)w;
  _mmkedit_apply (NULL, mmkedit, state);
  _mmkedit_exit (NULL, mmkedit, state);
}

static void mmk_fileselector_callback (filebrowser_t *fb, void *data) {
  xui_mmkedit_t *mmkedit = data;
  char *file, *dir;

  if ((dir = filebrowser_get_current_dir (fb)) != NULL) {
    strlcpy (mmkedit->gui->curdir, dir, sizeof (mmkedit->gui->curdir));
    free (dir);
  }

  if ((file = filebrowser_get_full_filename (fb)) != NULL) {
    if (file)
      xitk_inputtext_change_text (mmkedit->sub, file);
    free (file);
  }

}

static void _mmkedit_select_sub (xitk_widget_t *w, void *data, int state) {
  xui_mmkedit_t *mmkedit = data;
  filebrowser_callback_button_t  cbb;
  char                           *path, *open_path;

  (void)w;
  (void)state;
  cbb.label = _("Select");
  cbb.callback = mmk_fileselector_callback;
  cbb.userdata = mmkedit;
  cbb.need_a_file = 1;

  path = (*mmkedit->mmk)->sub ? (*mmkedit->mmk)->sub : (*mmkedit->mmk)->mrl;

  if(mrl_look_like_file(path)) {
    char *p;

    open_path = strdup(path);

    if(!strncasecmp(path, "file:", 5))
      path += 5;

    p = strrchr(open_path, '/');
    if (p && p[0])
      *p = '\0';
  }
  else
    open_path = strdup (mmkedit->gui->curdir);

  create_filebrowser (mmkedit->gui, _("Pick a subtitle file"), open_path, hidden_file_cb, mmkedit->gui, &cbb, NULL, NULL);

  free(open_path);
}

void mmk_edit_mediamark (gGui_t *gui, mediamark_t **mmk, apply_callback_t callback, void *data) {
  xui_mmkedit_t *mmkedit;
  xitk_widget_t              *b;
  int                         x, y, w;

  if (!gui)
    return;

  mmkedit = gui->mmkedit;
  if (mmkedit) {
    mmkedit->visible = 1;
    raise_window (mmkedit->gui, mmkedit->xwin, mmkedit->visible, 1);
    _mmkedit_set_mmk (mmkedit, mmk);
    return;
  }

  mmkedit = (xui_mmkedit_t *)xitk_xmalloc (sizeof (*mmkedit));
  if (!mmkedit)
    return;

  mmkedit->gui = gui;
  gui->mmkedit = mmkedit;

  mmkedit->callback = callback;
  mmkedit->user_data = data;

  x = y = 80;
  gui_load_window_pos (gui, "mmk_editor", &x, &y);

  /* Create window */
  mmkedit->xwin = xitk_window_create_dialog_window (mmkedit->gui->xitk, _("Mediamark Editor"), x, y,
    WINDOW_WIDTH, WINDOW_HEIGHT);

  set_window_states_start (mmkedit->gui, mmkedit->xwin);

  mmkedit->widget_list = xitk_window_widget_list (mmkedit->xwin);


  {
    xitk_image_t *bg = xitk_window_get_background_image (mmkedit->xwin);

    x = 15;
    y = 34 - 6;
    w = WINDOW_WIDTH - 30;
    xitk_image_draw_outter_frame (bg, _("Identifier"), btnfontname, x, y, w, 45);
    y += 45 + 3;
    xitk_image_draw_outter_frame (bg, _("Mrl"), btnfontname, x, y, w, 45);
    y += 45 + 3;
    xitk_image_draw_outter_frame (bg, _("Subtitle"), btnfontname, x, y, w, 45);
    y += 45 + 3;
    w = 120;
    xitk_image_draw_outter_frame (bg, _("Start at"), btnfontname, x, y, w, 45);
    x += w + 5;
    xitk_image_draw_outter_frame (bg, _("End at"), btnfontname, x, y, w, 45);
    x += w + 5;
    xitk_image_draw_outter_frame (bg, _("A/V offset"), btnfontname, x, y, w, 45);
    x += w + 5;
    xitk_image_draw_outter_frame (bg, _("SPU offset"), btnfontname, x, y, w, 45);
    xitk_window_set_background_image (mmkedit->xwin, bg);
  }

  {
    xitk_inputtext_widget_t inp;

    XITK_WIDGET_INIT (&inp);
    inp.skin_element_name = NULL;
    inp.text              = NULL;
    inp.max_length        = 2048;
    inp.callback          = NULL;
    inp.userdata          = NULL;

    x = 15;
    y = 34 - 6;
    w = WINDOW_WIDTH - 30;
    mmkedit->ident = xitk_noskin_inputtext_create (mmkedit->widget_list, &inp,
      x + 10, y + 16, w - 20, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
    xitk_add_widget (mmkedit->widget_list, mmkedit->ident, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->ident, _("Mediamark Identifier"));

    y += 45 + 3;
    mmkedit->mrl = xitk_noskin_inputtext_create (mmkedit->widget_list, &inp,
      x + 10, y + 16, w - 20, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
    xitk_add_widget (mmkedit->widget_list, mmkedit->mrl, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->mrl, _("Mediamark Mrl"));

    y += 45 + 3;
    mmkedit->sub = xitk_noskin_inputtext_create (mmkedit->widget_list, &inp,
      x + 10, y + 16, w - 20 - 100 - 10, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
    xitk_add_widget (mmkedit->widget_list, mmkedit->sub, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->sub, _("Subtitle File"));
  }

  {
    xitk_labelbutton_widget_t lb;
    xitk_intbox_widget_t ib;

    XITK_WIDGET_INIT (&lb);
    lb.skin_element_name = NULL;
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.state_callback    = NULL;
    lb.userdata          = mmkedit;
    XITK_WIDGET_INIT (&ib);
    ib.skin_element_name = NULL;
    ib.fmt               = INTBOX_FMT_DECIMAL;
    ib.callback          = NULL;
    ib.userdata          = NULL;
    ib.step              = 1;

    lb.label             = _("Sub File");
    lb.callback          = _mmkedit_select_sub;
    b =  xitk_noskin_labelbutton_create (mmkedit->widget_list, &lb,
      x + 10 + w - 20 - 100, y + 16, 100, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
    xitk_add_widget (mmkedit->widget_list, b, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (b, _("Select a subtitle file to use together with the mrl."));

    y += 45 + 3;
    w = 120;
    ib.min               = 0;
    ib.max               = 0;
    ib.value             = 0;
    mmkedit->start = xitk_noskin_intbox_create (mmkedit->widget_list, &ib,
      x + 30, y + 16, w - 60, 20);
    xitk_add_widget (mmkedit->widget_list, mmkedit->start, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->start, _("Mediamark start time (secs)."));

    x += w + 5;
    ib.value             = -1;
    mmkedit->end = xitk_noskin_intbox_create (mmkedit->widget_list, &ib,
      x + 30, y + 16, w - 60, 20);
    xitk_add_widget (mmkedit->widget_list, mmkedit->end, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->end, _("Mediamark end time (secs)."));

    x += w + 5;
    ib.value             = 0;
    mmkedit->av_offset = xitk_noskin_intbox_create (mmkedit->widget_list, &ib,
      x + 30, y + 16, w - 60, 20);
    xitk_add_widget (mmkedit->widget_list, mmkedit->av_offset, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->av_offset, _("Offset of Audio and Video."));

    x += w + 5;
    ib.value             = 0;
    mmkedit->spu_offset = xitk_noskin_intbox_create (mmkedit->widget_list, &ib,
      x + 30, y + 16, w - 60, 20);
    xitk_add_widget (mmkedit->widget_list, mmkedit->spu_offset, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (mmkedit->spu_offset, _("Subpicture offset."));

    y = WINDOW_HEIGHT - (23 + 15);
    x = 15;
    lb.label             = _("OK");
    lb.callback          = _mmkedit_ok;
    b =  xitk_noskin_labelbutton_create (mmkedit->widget_list,
      &lb, x, y, 100, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
    xitk_add_widget (mmkedit->widget_list, b, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (b, _("Apply the changes and close the window."));

    x = (WINDOW_WIDTH - 100) / 2;
    lb.label             = _("Apply");
    lb.callback          = _mmkedit_apply;
    b =  xitk_noskin_labelbutton_create (mmkedit->widget_list,
      &lb, x, y, 100, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
    xitk_add_widget (mmkedit->widget_list, b, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (b, _("Apply the changes to the playlist."));

    x = WINDOW_WIDTH - (100 + 15);
    lb.label             = _("Close");
    lb.callback          = _mmkedit_exit;
    b =  xitk_noskin_labelbutton_create (mmkedit->widget_list,
      &lb, x, y, 100, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
    xitk_add_widget (mmkedit->widget_list, b, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    xitk_set_widget_tips (b, _("Discard changes and dismiss the window."));
  }

  mmkedit->widget_key = xitk_be_register_event_handler ("gui->mmkedit", mmkedit->xwin, mmkedit_event, mmkedit, NULL, NULL);
  mmkedit->visible = 1;

  _mmkedit_set_mmk (mmkedit, mmk);
  raise_window (mmkedit->gui, mmkedit->xwin, 1, 1);
  xitk_window_set_input_focus (mmkedit->xwin);
}

