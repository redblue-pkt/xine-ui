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
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "common.h"
#include "file_browser.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/combo.h"
#include "xine-toolkit/browser.h"


#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       390
#define MAX_DISP_ENTRIES    10

#define MAXFILES            65535

#ifndef S_ISLNK
#define S_ISLNK(mode)  0
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) 0
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(mode) 0
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode)  0
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode)  0
#endif
#ifndef S_ISREG
#define S_ISREG(mode)  0
#endif
#if !S_IXUGO
#define S_IXUGO        (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

typedef union {
  uint8_t n[4];
  uint32_t v;
} _fb_ext_t;

static const char _fb_ext_list[][12 * 4] = {
  { "4xm " },
  { "ac3 " },
  { "aif aiff" },
  { "asf wmv wma wvx wax " },
  { "aud " },
  { "avi " },
  { "cin " },
  { "cpk cak film" },
  { "dv  dif " },
  { "fli flc " },
  { "flv " },
  { "mp3 mp2 mpa mpga" },
  { "mjpg" },
  { "mov qt  mp4 mp4amp4v" },
  { "m2p " },
  { "mpg mpeg " },
  { "mpv " },
  { "mve mv8 " },
  { "nsf " },
  { "nsv " },
  { "ogg ogm spx " },
  { "pes " },
  { "png mng " },
  { "pva " },
  { "ra  " },
  { "rm  ram rmvb" },
  { "roq " },
  { "str iki ik2 dps dat xa1 xa2 xas xap xa  " },
  { "snd au  " },
  { "ts  m2t trp " },
  { "vqa " },
  { "vob " },
  { "voc " },
  { "vox " },
  { "wav " },
  { "wve " },
  { "y4m " }
};

static const char *_fb_exts[XUI_EXTS_LAST + sizeof (_fb_ext_list) / sizeof (_fb_ext_list[0]) + 1] = {[0] = NULL};

const char **filebrowser_ext_list_get (gGui_t *gui) {
  (void)gui;
  if (!_fb_exts[0]) {
    uint32_t u;
    _fb_exts[XUI_EXTS_ANY] = _("All");
    _fb_exts[XUI_EXTS_MEDIA] = _("Media");
    _fb_exts[XUI_EXTS_AUDIO] = _("Audio");
    _fb_exts[XUI_EXTS_VIDEO] = _("Video");
    _fb_exts[XUI_EXTS_IMAGE] = _("Images");
    _fb_exts[XUI_EXTS_SUBTITLE] = _("Subtitles");
    _fb_exts[XUI_EXTS_PLAYLIST] = _("Playlists");
    for (u = 0; u < sizeof (_fb_ext_list) / sizeof (_fb_ext_list[0]) - 1; u++)
      _fb_exts[XUI_EXTS_LAST + u] = _fb_ext_list[u];
    _fb_exts[XUI_EXTS_LAST + u] = NULL;
  }
  return (const char **)_fb_exts;
}

void filebrowser_ext_list_unget (gGui_t *gui, const char ***list) {
  (void)gui;
  *list = NULL;
}

static uint32_t _fb_ext_from_name (const char *name) {
  _fb_ext_t ext = {"    "};
  const uint8_t *s = (const uint8_t *)name, *e = NULL;
  uint32_t u;

  if (!name)
    return ext.v;

  while (1) {
    while (s[0] > '/')
      s++;
    if (s[0] == '/')
      e = NULL;
    else if (s[0] == '.')
      e = s;
    else if (!s[0])
      break;
    s++;
  }
  if (!e)
    return ext.v;
  for (e++, u = 0; e[u] && (u < 4); u++)
    ext.n[u] = e[u];
  return ext.v | 0x20202020;
}

int filebrowser_ext_match (gGui_t *gui, const char *name, uint32_t n) {
  uint32_t ext, u;

  ext = (n > XUI_EXTS_ANY) ? _fb_ext_from_name (name) : 0;
  switch (n) {
    case XUI_EXTS_ANY:
      return 1;
    case XUI_EXTS_MEDIA:
    case XUI_EXTS_AUDIO:
    case XUI_EXTS_VIDEO:
    case XUI_EXTS_IMAGE:
    case XUI_EXTS_SUBTITLE:
    case XUI_EXTS_PLAYLIST:
      return xine_sarray_binary_search (gui->playlist.exts[n], (void *)(uintptr_t)ext) >= 0;
    default:
      n -= XUI_EXTS_LAST;
      if (n >= sizeof (_fb_ext_list) / sizeof (_fb_ext_list[0]))
        return 0;
      for (u = 0; _fb_ext_list[n][u]; u += 4) {
        _fb_ext_t _ext;
        memcpy (&_ext.n, &_fb_ext_list[n][u], 4);
        if (_ext.v == ext)
          return 1;
      }
  }
  return 0;
}
  
static char *_fb_strldup (const char *s, uint32_t l) {
  char *res = malloc (l + 1);
  if (!res)
    return NULL;
  if (l)
    memcpy (res, s, l);
  res[l] = 0;
  return res;
}

typedef struct {
  uint32_t have, used, bufsize, bufused;
  char **array, *buf;
} fb_list_t;

static void _fb_list_init (fb_list_t *list) {
  list->have = list->used = list->bufsize = list->bufused = 0;
  list->array = NULL;
  list->buf = NULL;
}

static void _fb_list_deinit (fb_list_t *list) {
  list->have = list->used = list->bufsize = list->bufused = 0;
  SAFE_FREE (list->array);
  SAFE_FREE (list->buf);
}

static void _fb_list_clear (fb_list_t *list) {
  list->used = list->bufused = 0;
}

static void _fb_list_add (fb_list_t *list, const char *str, uint32_t term) {
  size_t slen;
  char *p;
  if (!str)
    return;
  if (list->used + 2 > list->have) {
    uint32_t nhave = list->have + 128;
    char **na = realloc (list->array, nhave * sizeof (list->array[0]));
    if (!na)
      return;
    list->array = na;
    list->have = nhave;
  }
  slen = strlen (str);
  /* keep start and end pads */
  if (list->bufused + slen + 8 > list->bufsize) {
    uint32_t nsize = list->bufsize + 8192, u;
    char **a, *nb = realloc (list->buf, nsize);
    if (!nb)
      return;
    if (!list->buf) {
      memset (nb, 0, 4);
      list->bufused = 4;
    }
    for (a = list->array, u = list->used; u; a++, u--)
      *a = nb + (*a - list->buf);
    list->buf = nb;
    list->bufsize = nsize;
  }
  p = list->buf + list->bufused;
  list->array[list->used++] = p;
  list->array[list->used] = NULL;
  if (!term) {
    slen += 1;
    memcpy (p, str, slen);
  } else {
    memcpy (p, str, slen);
    p[slen++] = term;
    p[slen++] = 0;
  }
  list->bufused += slen;
}

static void fb_list_reverse (fb_list_t *list) {
  char **a, **b;
  if (list->used < 2)
    return;
  for (a = list->array, b = list->array + list->used - 1; a < b; a++, b--) {
    char *temp = *a;
    *a = *b;
    *b = temp;
  }
}

typedef struct {
  char                  *name;
} fileinfo_t;

#define DEFAULT_SORT 0
#define REVERSE_SORT 1

#define NORMAL_CURS  0
#define WAIT_CURS    1

typedef enum {
  _W_origin = 0,
  _W_directories_browser,
  _W_directories_sort,
  _W_files_browser,
  _W_files_sort,
  _W_rename,
  _W_delete,
  _W_create,
  _W_filters,
  _W_show_hidden,
  _W_close,
  _W_cb_button0,
  _W_cb_button1,
  _W_LAST
} _W_t;

struct filebrowser_s {
  gGui_t                         *gui;

  xitk_window_t                  *xwin;

  xitk_widget_list_t             *widget_list;

  xitk_widget_t                  *w[_W_LAST];

  int                             directories_sort_direction;
  int                             files_sort_direction;

  xitk_image_t                   *sort_skin_up;
  xitk_image_t                   *sort_skin_down;

  const char                    **file_filters;
  int                             filter_selected;

  int                             show_hidden_files;

  fb_list_t                       dir_list;
  int                             directories_sel;

  fb_list_t                       file_list;

  int                             running;
  int                             visible;

  hidden_file_toggle_t            hidden_cb;
  void                           *hidden_data;

  filebrowser_callback_button_t   cbb[3];

  xitk_register_key_t             widget_key;
  xitk_register_key_t             dialog;

  uint32_t                        saved_file_len;
  /* offsets into path */
  uint32_t                        dirstart, dirstop, filestart, filestop;
  char                            path[2048], trypath[2048];
  char                            saved_file[768];
};

static const char *_fb_file_get (filebrowser_t *fb, const char *name) {
  uint32_t have, l;

  if (!fb->saved_file_len) {
    have = sizeof (fb->saved_file);
    l = fb->filestop - fb->filestart + 1;
    if (l > have)
      l = have;
    fb->saved_file_len = l;
    memcpy (fb->saved_file, fb->path + fb->filestart, l);
  }

  have = sizeof (fb->path) - fb->filestart;
  l = strlen (name) + 1;
  if (l > have)
    l = have;
  memcpy (fb->path + fb->filestart, name, l);

  return fb->path + fb->dirstart;
}

static void _fb_file_unget (filebrowser_t *fb) {
  if (!fb->saved_file_len)
    return;
  memcpy (fb->path + fb->filestart, fb->saved_file, fb->saved_file_len);
  fb->saved_file_len = 0;
}

typedef struct {
  xitk_window_t            *xwin;
  xitk_widget_list_t       *widget_list;
  xitk_widget_t            *input;

  xitk_string_callback_t    callback;
  filebrowser_t            *fb;

  xitk_widget_t            *button_apply;
  xitk_widget_t            *button_cancel;

  xitk_register_key_t       widget_key;
} filename_editor_t;

/*
 * Enable/disable widgets in file browser window
 */
static void fb_deactivate(filebrowser_t *fb) {
  xitk_widgets_state (fb->w, _W_LAST, XITK_WIDGET_STATE_ENABLE, 0);
}
static void fb_reactivate(filebrowser_t *fb) {
  xitk_widgets_state (fb->w, _W_LAST, XITK_WIDGET_STATE_ENABLE, ~0u);
}

static void _fb_set_cursor(filebrowser_t *fb, int state) {
  if(fb) {
    if(state == WAIT_CURS)
      xitk_window_define_window_cursor (fb->xwin, xitk_cursor_watch);
    else
      xitk_window_restore_window_cursor (fb->xwin);
  }
}
/*
 * **************************************************
 */
static void fb_exit(xitk_widget_t *, void *);

/*
 * ************************* filename editor **************************
 */
static void fne_destroy(filename_editor_t *fne) {
  if(fne) {

    xitk_unregister_event_handler (fne->fb->gui->xitk, &fne->widget_key);

    xitk_window_destroy_window(fne->xwin);
    fne->xwin = NULL;
    /* xitk_dlist_init (&fne->widget_list->list); */

    free(fne);
  }
}
static void fne_apply_cb (xitk_widget_t *w, void *data, int state) {
  filename_editor_t *fne = (filename_editor_t *) data;

  (void)w;
  (void)state;
  if(fne->callback)
    fne->callback(NULL, (void *)fne->fb, (xitk_inputtext_get_text(fne->input)));

  fb_reactivate(fne->fb);
  fne_destroy(fne);
}

static void fne_exit_cb (xitk_widget_t *w, void *data, int state) {
  filename_editor_t *fne = (filename_editor_t *) data;

  (void)w;
  (void)state;
  fb_reactivate(fne->fb);
  fne_destroy(fne);
}

static int fne_event (void *data, const xitk_be_event_t *e) {
  filename_editor_t *fne = data;

  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    fne_exit_cb (NULL, fne, 0);
    return 1;
  }
  return gui_handle_be_event (fne->fb->gui, e);
}

static void fb_create_input_window (filebrowser_t *fb, xitk_string_callback_t cb,
  const char *title, char *text, size_t maxlen) {
  filename_editor_t          *fne;
  int                         x, y, w;
  int                         width = WINDOW_WIDTH;
  int                         height = 102;
  xitk_labelbutton_widget_t   lb;
  xitk_inputtext_widget_t     inp;

  fne = (filename_editor_t *) calloc(1, sizeof(filename_editor_t));
  if (!fne)
    return;

  fne->callback = cb;
  fne->fb = fb;

  fne->xwin = xitk_window_create_dialog_window_center (fb->gui->xitk, title, width, height);

  xitk_window_set_wm_window_type(fne->xwin, WINDOW_TYPE_NORMAL);
  xitk_window_set_window_class(fne->xwin, NULL, "xine");
  xitk_window_set_window_icon (fne->xwin, fne->fb->gui->icon);

  fne->widget_list                = xitk_window_widget_list(fne->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&inp);

  x = 15;
  y = 30;
  w = width - 30;

  inp.skin_element_name = NULL;
  inp.text              = text;
  inp.max_length        = maxlen;
  inp.callback          = NULL;
  inp.userdata          = (void *)fne;
  fne->input = xitk_noskin_inputtext_create (fne->widget_list, &inp,
    x, y, w, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
  xitk_add_widget (fne->widget_list, fne->input, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  y = height - (23 + 15);
  x = 15;
  w = 100;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Apply");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_apply_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  fne->button_apply = xitk_noskin_labelbutton_create (fne->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fne->widget_list, fne->button_apply, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = width - (w + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Cancel");
  lb.align             = ALIGN_CENTER;
  lb.callback          = fne_exit_cb;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fne;
  lb.skin_element_name = NULL;
  fne->button_cancel = xitk_noskin_labelbutton_create (fne->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fne->widget_list, fne->button_cancel, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "filenameed%u", (unsigned int) time(NULL));

    fne->widget_key = xitk_be_register_event_handler (buffer, fne->xwin, fne_event, fne, NULL, NULL);
  }

  xitk_window_flags (fne->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
  xitk_window_raise_window (fne->xwin);

  video_window_set_transient_for (fb->gui->vwin, fne->xwin);
  layer_above_video (fb->gui, fne->xwin);

  xitk_window_set_input_focus (fne->xwin);
}
/*
 * ************************** END OF filename editor **************************
 */

/*
 * **************************** File related funcs **************************
 */

/*
 * Return 1 if file match with current filter, otherwise 0.
 */
static int is_file_match_to_filter (filebrowser_t *fb, const char *file) {
  return filebrowser_ext_match (fb->gui, file, fb->filter_selected);
}

static void fb_update_origin(filebrowser_t *fb) {
  xitk_inputtext_change_text (fb->w[_W_origin], fb->path + fb->dirstart);
}

static void fb_extract_path_and_file(filebrowser_t *fb, const char *filepathname) {
  uint32_t l, have;
  char *d, temp;

  if (!filepathname)
    return;
  if (!filepathname[0])
    return;

  have = sizeof (fb->path) - fb->dirstart;
  d = fb->path + fb->dirstart;
  /* "\~..." -> "~..." */
  if ((filepathname[0] == '\\') && (filepathname[1] == '~'))
    filepathname++;
  /* "~/..." -> "/home/user" "/..." */
  if ((filepathname[0] == '~') && ((filepathname[1] == '/') || !filepathname[1])) {
    const char *homedir = xine_get_homedir ();

    filepathname++;
    l = strlcpy (d, homedir, have);
    if (l > have - 1)
      l = have - 1;
    d += l;
    have -= l;
  } else
  /* XXX: "/some/dir/filename.flv" "newfile.flv" -> "/some/dir/newfile.flv" */
  if (filepathname[0] != '/') {
    have = sizeof (fb->path) - fb->filestart;
    d = fb->path + fb->filestart;
    l = strlcpy (d, filepathname, have);
    if (l > have - 1)
      l = have - 1;
    fb->filestop = fb->filestart + l;
    return;
  }
  /* get everything */
  l = strlcpy (fb->path + fb->dirstart, filepathname, have);
  if (l > have - 1)
    l = have - 1;
  d += l;
  /* "...////" -> "..." */
  fb->path[fb->dirstart - 1] = 0;
  temp = fb->path[fb->dirstart];
  fb->path[fb->dirstart] = 0;
  while (d[-1] == '/')
    d--;
  fb->path[fb->dirstart] = temp;
  *d = 0;
  /* file part? */
  if (is_a_dir (fb->path + fb->dirstart)) {
    /* known a dir */
    fb->dirstop = d - fb->path;
    if (d[-1] != '/') {
      *d++ = '/';
      *d = 0;
    }
    fb->filestart = fb->filestop = d - fb->path;
  } else {
    fb->filestop = d - fb->path;
    /* find last / */
    fb->path[fb->dirstart -1] = '/';
    while (d[-1] != '/')
      d--;
    fb->dirstop = fb->filestart = d - fb->path;
    if (fb->dirstop < fb->dirstart + 2) {
      /* "justafilename.flv" or "/filename.flv" */
      ;
    } else {
      /* "...somedir/filename.flv" */
      fb->dirstop--;
    }
  }
}

/*
 * Sorting function, it comes from GNU fileutils package.
 */
#define S_N        0x0
#define S_I        0x4
#define S_F        0x8
#define S_Z        0xC
#define CMP          2
#define LEN          3
#define ISDIGIT(c)   ((unsigned) (c) - '0' <= 9)
static int my_strverscmp(const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;
  static const unsigned int next_state[] = {
    S_N, S_I, S_Z, S_N,
    S_N, S_I, S_I, S_I,
    S_N, S_F, S_F, S_F,
    S_N, S_F, S_Z, S_Z
  };
  static const int result_type[] = {
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,  -1,  -1, CMP,   1, LEN, LEN, CMP,
      1, LEN, LEN, CMP, CMP, CMP, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
    CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
    CMP,   1,   1, CMP,  -1, CMP, CMP, CMP,
     -1, CMP, CMP, CMP
  };

  if(p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;

  state = S_N | ((c1 == '0') + (ISDIGIT(c1) != 0));

  while((diff = c1 - c2) == 0 && c1 != '\0') {
    state = next_state[state];
    c1 = *p1++;
    c2 = *p2++;
    state |= (c1 == '0') + (ISDIGIT(c1) != 0);
  }

  state = result_type[state << 2 | ((c2 == '0') + (ISDIGIT(c2) != 0))];

  switch(state) {
  case CMP:
    return diff;

  case LEN:
    while(ISDIGIT(*p1++))
      if(!ISDIGIT(*p2++))
	return 1;

    return ISDIGIT(*p2) ? -1 : diff;

  default:
    return state;
  }
}

/*
 * Wrapper to my_strverscmp() for qsort() calls, which sort mrl_t type array.
 */
static int _sortfiles_default (const void *a, const void *b) {
  const char * const *d = (const char * const *)a;
  const char * const *e = (const char * const *)b;
  return my_strverscmp (*d, *e);
}

static void fb_list_sort (fb_list_t *list, uint32_t reverse) {
  if (list->used < 2)
    return;
  qsort (list->array, list->used, sizeof (list->array[0]), _sortfiles_default);
  if (reverse == REVERSE_SORT)
    fb_list_reverse (list);
}

static void sort_directories (filebrowser_t *fb) {
  fb_list_sort (&fb->dir_list, fb->directories_sort_direction);
  xitk_browser_update_list (fb->w[_W_directories_browser],
    (const char *const *)fb->dir_list.array, NULL, fb->dir_list.used, 0);
}

static void sort_files (filebrowser_t *fb) {
  fb_list_sort (&fb->file_list, fb->files_sort_direction);
  xitk_browser_update_list (fb->w[_W_files_browser],
    (const char *const *)fb->file_list.array, NULL, fb->file_list.used, 0);
}

static void fb_getdir(filebrowser_t *fb) {
  char           temp;
  struct dirent *pdirent;
  DIR           *pdir;

  _fb_set_cursor(fb, WAIT_CURS);

  _fb_list_clear (&fb->file_list);
  _fb_list_clear (&fb->dir_list);

  /* Following code relies on the fact that fb->current_dir has no trailing '/' */
  temp = fb->path[fb->dirstop];
  fb->path[fb->dirstop] = 0;
  if ((pdir = opendir (fb->path + fb->dirstart)) == NULL) {
    gui_msg (fb->gui, XUI_MSG_ERROR, _("Unable to open directory '%s': %s."),
      fb->path + fb->dirstart, strerror (errno));
    fb->path[fb->dirstop] = temp;
    /* give user an exit */
    _fb_list_add (&fb->dir_list, "..", '/');
    _fb_set_cursor (fb, NORMAL_CURS);
    return;
  }
  fb->path[fb->dirstop] = temp;

  while ((pdirent = readdir (pdir)) != NULL) {
    fb_list_t *list;
    const char *fullname;
    uint32_t term;

    /* if user don't want to see hidden files, ignore them */
    if ((pdirent->d_name[0] == '.') && pdirent->d_name[1] && (pdirent->d_name[1] != '.') && !fb->show_hidden_files)
      continue;

    fullname = _fb_file_get (fb, pdirent->d_name);
    if (is_a_dir (fullname)) {
      list = &fb->dir_list;
      term = '/';
    } else {
      if (!is_file_match_to_filter (fb, pdirent->d_name))
        continue;
      list = &fb->file_list;
      term = 0;
    }
    _fb_list_add (list, pdirent->d_name, term);
  }
  _fb_file_unget (fb);

  closedir(pdir);

  /*
   * Sort arrays
   */
  sort_directories(fb);
  sort_files(fb);
  _fb_set_cursor(fb, NORMAL_CURS);
}

/*
 * ****************************** END OF file related funcs ***************************
 */

/*
 * ***************************** widget callbacks *******************************
 */
static void fb_select(xitk_widget_t *w, void *data, int selected, int modifier) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)modifier;
  if (selected < 0)
    return;
  if(w == fb->w[_W_files_browser]) {
    uint32_t have = sizeof (fb->path) - fb->filestart;
    uint32_t l = strlcpy (fb->path + fb->filestart, fb->file_list.array[selected], have);
    if (l > have - 1)
      l = have - 1;
    fb->filestop = fb->filestart + l;
    fb_update_origin(fb);
  }
}

static void fb_callback_button_cb (xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *)data;
  filebrowser_callback_button_t *b = (w == fb->w[_W_cb_button0]) ? fb->cbb : fb->cbb + 1;

  (void)state;
  if (b->need_a_file) {
    if (!fb->path[fb->filestart])
      return;
  }
  if (b->callback)
    b->callback (fb, b->userdata);
  fb_exit (NULL, (void *)fb);
}


static void fb_dbl_select(xitk_widget_t *w, void *data, int selected, int modifier) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)modifier;
  if (w == fb->w[_W_directories_browser]) {
    uint32_t l = strlen (fb->dir_list.array[selected]);

    if (!memcmp (fb->dir_list.array[selected], "./", 3)) {
      /* Want to re-read current dir */
      ;
    } else if (!memcmp (fb->dir_list.array[selected], "../", 4)) {
      char *d, *f;
      /* level up */
      fb->path[fb->dirstart - 1] = '/';
      d = fb->path + fb->dirstop;
      while (d[-1] != '/')
        d--;
      fb->dirstop = d - fb->path;
      if (fb->dirstop < fb->dirstart + 2) {
        /* all dirs gone or leading / reached */
        ;
      } else {
        fb->dirstop--;
      }
      f = fb->path + fb->filestart;
      if (f > d) {
        memmove (d, f, fb->filestop - fb->filestart + 1);
        fb->filestart -= f - d;
        fb->filestop -= f - d;
      }
    } else if (fb->filestop + l < sizeof (fb->path)) {
      char *d = fb->path + fb->dirstop, *f = fb->path + fb->filestart;
      /* level down */
      fb->path[fb->dirstart - 1] = '/';
      if (d[-1] == '/') {
        /* "file" -> "add/file" or "/" -> "/add/file" */
        ;
      } else {
        /* "...have/file" -> "...have/add/file" */
        d++;
      }
      memmove (d + l, f, fb->filestop - fb->filestart + 1);
      memcpy (d, fb->dir_list.array[selected], l);
      d += l;
      fb->dirstop = d - fb->path - 1;
      l = d - f;
      fb->filestart += l;
      fb->filestop += l;
    }
    fb_update_origin(fb);
    fb_getdir(fb);
  } else if (w == fb->w[_W_files_browser]) {
    uint32_t l = strlen (fb->file_list.array[selected]);

    if (fb->filestop + l < sizeof (fb->path)) {
      memcpy (fb->path + fb->filestart, fb->file_list.array[selected], l + 1);
      fb->filestop = fb->filestart + l;
      fb_callback_button_cb (fb->w[_W_cb_button0], (void *)data, 0);
    }
  }
}

static void fb_change_origin(xitk_widget_t *w, void *data, const char *currenttext) {
  filebrowser_t *fb = (filebrowser_t *)data;

  (void)w;
  fb_extract_path_and_file(fb, currenttext);
  fb_update_origin(fb);
  fb_getdir(fb);
}

static void fb_sort(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  if(w == fb->w[_W_directories_sort]) {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->w[_W_directories_sort]);

    fb->directories_sort_direction = (fb->directories_sort_direction == DEFAULT_SORT) ?
      REVERSE_SORT : DEFAULT_SORT;

    xitk_widgets_state (fb->w + _W_directories_sort, 1, XITK_WIDGET_STATE_VISIBLE, 0);

    if(fb->directories_sort_direction == DEFAULT_SORT)
      xitk_image_copy (fb->sort_skin_down, dsimage);
    else
      xitk_image_copy (fb->sort_skin_up, dsimage);

    xitk_widgets_state (fb->w + _W_directories_sort, 1, XITK_WIDGET_STATE_VISIBLE, ~0u);

    fb_list_reverse (&fb->dir_list);
    xitk_browser_update_list (fb->w[_W_directories_browser],
      (const char * const *)fb->dir_list.array, NULL, fb->dir_list.used, 0);
  }
  else if(w == fb->w[_W_files_sort]) {
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->w[_W_files_sort]);

    fb->files_sort_direction = (fb->files_sort_direction == DEFAULT_SORT) ?
      REVERSE_SORT : DEFAULT_SORT;

    xitk_widgets_state (fb->w + _W_files_sort, 1, XITK_WIDGET_STATE_VISIBLE, 0);

    if(fb->files_sort_direction == DEFAULT_SORT)
      xitk_image_copy (fb->sort_skin_down, fsimage);
    else
      xitk_image_copy (fb->sort_skin_up, fsimage);

    xitk_widgets_state (fb->w + _W_files_sort, 1, XITK_WIDGET_STATE_VISIBLE, ~0u);

    fb_list_reverse (&fb->file_list);
    xitk_browser_update_list (fb->w[_W_files_browser],
      (const char * const *)fb->file_list.array, NULL, fb->file_list.used, 0);
  }
}

static void fb_exit(xitk_widget_t *w, void *data) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  if(fb) {
    fb->running = 0;
    fb->visible = 0;

    if (fb->dialog)
      xitk_unregister_event_handler (fb->gui->xitk, &fb->dialog);

    xitk_unregister_event_handler (fb->gui->xitk, &fb->widget_key);

    xitk_window_destroy_window (fb->xwin);
    fb->xwin = NULL;
    /* xitk_dlist_init (&fb->widget_list->list); */

    filebrowser_ext_list_unget (fb->gui, &fb->file_filters);

    SAFE_FREE(fb->cbb[0].label);
    SAFE_FREE(fb->cbb[1].label);

    xitk_image_free_image (&fb->sort_skin_up);
    xitk_image_free_image (&fb->sort_skin_down);

    _fb_list_deinit (&fb->file_list);
    _fb_list_deinit (&fb->dir_list);

    free(fb);
    fb = NULL;
  }
}
static void _fb_exit (xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  (void)state;
  if (fb->cbb[2].callback)
    fb->cbb[2].callback (fb, fb->cbb[2].userdata);
  fb_exit(NULL, (void *)fb);
}

static void _fb_delete_file_done (void *data, int state) {
  filebrowser_t *fb = data;

  if (state == 2) {
    int sel = xitk_browser_get_current_selected (fb->w[_W_files_browser]);
    const char *fullname = _fb_file_get (fb, fb->file_list.array[sel]);

    if ((unlink (fullname)) == -1) {
      gui_msg (fb->gui, XUI_MSG_ERROR, _("Unable to delete file '%s': %s."), fullname, strerror (errno));
      _fb_file_unget (fb);
    } else {
      _fb_file_unget (fb);
      fb_getdir (fb);
    }
  }
  fb_reactivate (fb);
}

static void fb_delete_file (xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;

  (void)w;
  (void)state;
  if ((sel = xitk_browser_get_current_selected (fb->w[_W_files_browser])) >= 0) {
    const char *fullname = _fb_file_get (fb, fb->file_list.array[sel]);

    fb_deactivate(fb);
    fb->dialog = xitk_window_dialog_3 (fb->gui->xitk,
      fb->xwin,
      get_layer_above_video (fb->gui), 400, _("Confirm deletion ?"), _fb_delete_file_done, fb,
      NULL, XITK_LABEL_YES, XITK_LABEL_NO, NULL, 0, ALIGN_DEFAULT, "%s", fullname);
    _fb_file_unget (fb);
  }
}

static void fb_rename_file_cb(xitk_widget_t *w, void *data, const char *newname) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int sel = xitk_browser_get_current_selected(fb->w[_W_files_browser]);
  const char *fullname = _fb_file_get (fb, fb->file_list.array[sel]);

  (void)w;
  if ((rename (fullname, newname)) == -1) {
    gui_msg (fb->gui, XUI_MSG_ERROR, _("Unable to rename file '%s' to '%s': %s."), fullname, newname, strerror (errno));
    _fb_file_unget (fb);
  } else {
    _fb_file_unget (fb);
    fb_getdir(fb);
  }
}

static void fb_rename_file (xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int            sel;

  (void)w;
  (void)state;
  if((sel = xitk_browser_get_current_selected(fb->w[_W_files_browser])) >= 0) {
    const char *fullname = _fb_file_get (fb, fb->file_list.array[sel]);

    strcpy (fb->trypath, fullname);
    _fb_file_unget (fb);
    fb_deactivate(fb);
    fb_create_input_window (fb, fb_rename_file_cb, _("Rename file"), fb->trypath, sizeof (fb->trypath));
  }
}

static void fb_create_directory_cb(xitk_widget_t *w, void *data, const char *newdir) {
  filebrowser_t *fb = (filebrowser_t *) data;
  int err;

  (void)w;
  err = mkdir_safe (newdir);
  if (err)
    gui_msg (fb->gui, XUI_MSG_ERROR, _("Unable to create the directory '%s': %s."), newdir, strerror (err));
  else
    fb_getdir (fb);
}

static void fb_create_directory(xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;
  uint32_t       l = fb->filestart - fb->dirstart;

  (void)w;
  (void)state;

  memcpy (fb->trypath, fb->path + fb->dirstart, l);
  fb->trypath[l] = 0;
  fb_deactivate(fb);
  fb_create_input_window (fb, fb_create_directory_cb, _("Create a new directory"), fb->trypath, sizeof (fb->trypath));
}

static void fb_select_file_filter(xitk_widget_t *w, void *data, int selected) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  fb->filter_selected = selected;
  fb_getdir(fb);
}

static void fb_hidden_files(xitk_widget_t *w, void *data, int state) {
  filebrowser_t *fb = (filebrowser_t *) data;

  (void)w;
  fb->show_hidden_files = state;
  if (fb->hidden_cb)
    fb->hidden_cb (fb->hidden_data, 1, state);
  fb_getdir(fb);
}

static int fb_event (void *data, const xitk_be_event_t *e) {
  filebrowser_t *fb = (filebrowser_t *)data;

  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    if (xitk_is_widget_enabled (fb->w[_W_close])) { /* Exit only if close button would exit */
      _fb_exit (NULL, data, 0);
      return 1;
    }
  } else if (e->type == XITK_EV_KEY_DOWN) {
    int sel = xitk_browser_get_current_selected (fb->w[_W_files_browser]);
    if (sel >= 0) {
      fb_select (fb->w[_W_files_browser], (void *)fb, sel, e->qual);
      return 1;
    }
  }
  return gui_handle_be_event (fb->gui, e);
}

void filebrowser_raise_window(filebrowser_t *fb) {
  if(fb != NULL)
    raise_window (fb->gui, fb->xwin, fb->visible, fb->running);
}

void filebrowser_end(filebrowser_t *fb) {
  if(fb)
    fb_exit(NULL, (void *)fb);
}

char *filebrowser_get_current_dir(filebrowser_t *fb) {
  return fb ? _fb_strldup (fb->path + fb->dirstart, fb->dirstop - fb->dirstart) : NULL;
}

char *filebrowser_get_current_filename(filebrowser_t *fb) {
  return fb ? _fb_strldup (fb->path + fb->filestart, fb->filestop - fb->filestart) : NULL;
}

char *filebrowser_get_full_filename(filebrowser_t *fb) {
  return fb ? _fb_strldup (fb->path + fb->dirstart, fb->filestop- fb->dirstart) : NULL;
}

char **filebrowser_get_all_files(filebrowser_t *fb) {
  char **files = NULL;

  if (fb && fb->file_list.used) {
    uint32_t i;
    files = malloc ((fb->file_list.used + 2) * sizeof (files[0]));

    for (i = 0; i < fb->file_list.used; i++)
      files[i] = strdup (fb->file_list.array[i]);
    files[i] = NULL;
  }

  return files;
}

filebrowser_t *create_filebrowser (gGui_t *gui, const char *window_title, const char *filepathname,
  hidden_file_toggle_t hidden_cb, void *hidden_data, const filebrowser_callback_button_t *cbb1,
  const filebrowser_callback_button_t *cbb2, const filebrowser_callback_button_t *cbb_close) {
  filebrowser_t              *fb;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_image_t               *bg;
  xitk_browser_widget_t       br;
  xitk_inputtext_widget_t     inp;
  xitk_button_widget_t        b;
  xitk_combo_widget_t         cmb;
  xitk_widget_t              *widget;
  int                         x, y, w;

  if (!gui)
    return NULL;

  fb = (filebrowser_t *)calloc (1, sizeof (*fb));
  if (!fb)
    return NULL;

  fb->gui = gui;

  _fb_list_init (&fb->dir_list);
  _fb_list_init (&fb->file_list);

  fb->dirstart = fb->dirstop = fb->filestart = fb->filestop = 4;
  fb->path[4] = 0;

  if (cbb1) {
    fb->cbb[0].label = (cbb1->label && cbb1->label[0]) ? strdup (cbb1->label) : NULL;
    fb->cbb[0].callback = cbb1->callback;
    fb->cbb[0].userdata = cbb1->userdata;
    fb->cbb[0].need_a_file = cbb1->need_a_file;
  } else {
    fb->cbb[0].label = NULL;
    fb->cbb[0].callback = NULL;
    fb->cbb[0].userdata = NULL;
    fb->cbb[0].need_a_file = 0;
  }
  if (cbb2) {
    fb->cbb[1].label = (cbb2->label && cbb2->label[0]) ? strdup (cbb2->label) : NULL;
    fb->cbb[1].callback = cbb2->callback;
    fb->cbb[1].userdata = cbb2->userdata;
    fb->cbb[1].need_a_file = cbb2->need_a_file;
  } else {
    fb->cbb[1].label = NULL;
    fb->cbb[1].callback = NULL;
    fb->cbb[1].userdata = NULL;
    fb->cbb[1].need_a_file = 0;
  }
  if (cbb_close) {
    fb->cbb[2].callback = cbb_close->callback;
    fb->cbb[2].userdata = cbb_close->userdata;
  } else {
    fb->cbb[2].callback = NULL;
    fb->cbb[2].userdata = NULL;
  }
  fb->cbb[2].label = NULL;
  fb->cbb[2].need_a_file = 0;

  /* Create window */
  fb->xwin = xitk_window_create_dialog_window_center (fb->gui->xitk,
                                                      (window_title) ? window_title : _("File Browser"),
                                                      WINDOW_WIDTH, WINDOW_HEIGHT);

  xitk_window_set_wm_window_type(fb->xwin, WINDOW_TYPE_NORMAL);
  xitk_window_set_window_class(fb->xwin, NULL, "xine");
  xitk_window_set_window_icon (fb->xwin, fb->gui->icon);

  fb->directories_sort_direction = DEFAULT_SORT;
  fb->files_sort_direction       = DEFAULT_SORT;
  fb->hidden_cb                  = hidden_cb;
  fb->hidden_data                = hidden_data;
  fb->show_hidden_files          = hidden_cb ? hidden_cb (hidden_data, 0, 0) : 1;

  {
    uint32_t have = sizeof (fb->path) - fb->dirstart;
    uint32_t l = strlcpy (fb->path + fb->dirstart, xine_get_homedir (), have);
    if (l > have - 1)
      l = have - 1;
    fb->dirstop = fb->filestart = fb->filestop = fb->dirstart + l;
  }
  fb_extract_path_and_file(fb, filepathname);

  fb->file_filters = filebrowser_ext_list_get (fb->gui);
  fb->filter_selected = 0;

  fb->widget_list = xitk_window_widget_list(fb->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&lbl);
  XITK_WIDGET_INIT(&br);
  XITK_WIDGET_INIT(&inp);
  XITK_WIDGET_INIT(&cmb);
  XITK_WIDGET_INIT(&b);

  bg = xitk_window_get_background_image (fb->xwin);

  x = 15;
  y = 30;
  w = WINDOW_WIDTH - 30;

  inp.skin_element_name = NULL;
  inp.text              = NULL;
  inp.max_length        = XITK_PATH_MAX + XITK_NAME_MAX + 1;
  inp.callback          = fb_change_origin;
  inp.userdata          = (void *)fb;
  fb->w[_W_origin] = xitk_noskin_inputtext_create (fb->widget_list, &inp,
    x, y, w, 20, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, fontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_origin], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  fb_update_origin(fb);

  y += 45;
  w = (WINDOW_WIDTH - 30 - 10) / 2;

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.userdata                      = (void *)fb;
  br.callback                      = fb_select;
  br.dbl_click_callback            = fb_dbl_select;

  br.browser.num_entries           = fb->dir_list.used;
  br.browser.entries               = (const char *const *)fb->dir_list.array;
  fb->w[_W_directories_browser] = xitk_noskin_browser_create (fb->widget_list, &br,
    x + 2, y + 2, w - 4 - 12, 20, 12, fontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_directories_browser], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  xitk_image_draw_rectangular_box (bg, x, y, w, xitk_get_widget_height (fb->w[_W_directories_browser]) + 4, DRAW_INNER);

  y -= 15;

  b.userdata          = (void *)fb;
  b.state_callback    = NULL;
  b.callback          = fb_sort;

  b.skin_element_name = NULL;
  fb->w[_W_directories_sort] = xitk_noskin_button_create (fb->widget_list, &b, x, y, w, 15);
  xitk_add_widget (fb->widget_list, fb->w[_W_directories_sort], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = WINDOW_WIDTH - (w + 15);
  y += 15;

  br.browser.num_entries           = fb->file_list.used;
  br.browser.entries               = (const char * const *)fb->file_list.array;
  fb->w[_W_files_browser] = xitk_noskin_browser_create (fb->widget_list, &br,
    x + 2, y + 2, w - 4 - 12, 20, 12, fontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_files_browser], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  xitk_image_draw_rectangular_box (bg, x, y, w, xitk_get_widget_height (fb->w[_W_files_browser]) + 4, DRAW_INNER);

  y -= 15;

  fb->w[_W_files_sort] = xitk_noskin_button_create (fb->widget_list, &b, x, y, w, 15);
  xitk_add_widget (fb->widget_list, fb->w[_W_files_sort], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);


  {
    xitk_image_t *dsimage = xitk_get_widget_foreground_skin(fb->w[_W_directories_sort]);
    xitk_image_t *fsimage = xitk_get_widget_foreground_skin(fb->w[_W_files_sort]);
    xitk_image_t *image;
    xitk_point_t points[4];
    int           i, j, w, offset;
    short         x1, x2, x3;
    short         y1, y2, y3;
    int ds_width = xitk_image_width(dsimage);
    int ds_height = xitk_image_height(dsimage);

    fb->sort_skin_up = xitk_image_new (fb->gui->xitk, NULL, 0, ds_width, ds_height);
    fb->sort_skin_down = xitk_image_new (fb->gui->xitk, NULL, 0, ds_width, ds_height);

    xitk_image_draw_bevel_three_state (fb->sort_skin_up);
    xitk_image_draw_bevel_three_state (fb->sort_skin_down);

    w = ds_width / 3;

    for(j = 0; j < 2; j++) {
      if(j == 0)
	image = fb->sort_skin_up;
      else
	image = fb->sort_skin_down;

      offset = 0;
      for(i = 0; i < 2; i++) {
        xitk_image_draw_rectangular_box (image, 5, 4 + offset, w - 44, 2, DRAW_OUTTER);
        xitk_image_draw_rectangular_box (image, w - 20, 4 + offset, 11, 2, DRAW_OUTTER);
        xitk_image_draw_rectangular_box (image, w + 5, 4 + offset, w - 44, 2, DRAW_OUTTER);
        xitk_image_draw_rectangular_box (image, (w * 2) - 20, 4 + offset, 11, 2, DRAW_OUTTER);
        xitk_image_draw_rectangular_box (image, (w * 2) + 5 + 1, 4 + 1 + offset, w - 44, 2, DRAW_OUTTER);
        xitk_image_draw_rectangular_box (image, ((w * 3) - 20) + 1, 4 + 1 + offset, 11 + 1, 2, DRAW_OUTTER);
	offset += 4;
      }

      offset = 0;
      for(i = 0; i < 3; i++) {
	int k;

	if(j == 0) {
	  x1 = (w - 30) + offset;
	  y1 = (2);

	  x2 = (w - 30) - 7 + offset;
	  y2 = (2 + 8);

	  x3 = (w - 30) + 7 + offset;
	  y3 = (2 + 8);
	}
	else {
	  x1 = (w - 30) + offset;
	  y1 = (2 + 8);

	  x2 = (w - 30) - 6 + offset;
	  y2 = (3);

	  x3 = (w - 30) + 6 + offset;
	  y3 = (3);
	}

	if(i == 2) {
	  x1++; x2++; x3++;
	  y1++; y2++; y3++;
	}

	points[0].x = x1;
	points[0].y = y1;
	points[1].x = x2;
	points[1].y = y2;
	points[2].x = x3;
	points[2].y = y3;
	points[3].x = x1;
	points[3].y = y1;

	offset += w;

        xitk_image_fill_polygon (image, &points[0], 4, xitk_get_cfg_num (fb->gui->xitk, XITK_FOCUS_COLOR));

	for(k = 0; k < 3; k++) {
          int color;
          if (k == 0)
            color = xitk_get_cfg_num (fb->gui->xitk, XITK_BLACK_COLOR);
	  else if(k == 1)
            color = xitk_get_cfg_num (fb->gui->xitk, XITK_SELECT_COLOR);
	  else
            color = xitk_get_cfg_num (fb->gui->xitk, XITK_WHITE_COLOR);

          xitk_image_draw_line(image,
                               points[k].x, points[k].y, points[k+1].x, points[k+1].y,
                               color);
	}
      }
    }

    xitk_image_copy (fb->sort_skin_down, dsimage);
    xitk_image_copy (fb->sort_skin_down, fsimage);
  }

  y += xitk_get_widget_height(fb->w[_W_files_browser]) + 15 + 4 + 5;

  cmb.skin_element_name = NULL;
  cmb.layer_above       = is_layer_above (fb->gui);
  cmb.entries           = fb->file_filters;
  cmb.parent_wkey       = &fb->widget_key;
  cmb.callback          = fb_select_file_filter;
  cmb.userdata          = (void *)fb;
  fb->w[_W_filters] = xitk_noskin_combo_create (fb->widget_list, &cmb, x, y, w);
  xitk_add_widget (fb->widget_list, fb->w[_W_filters], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_combo_set_select(fb->w[_W_filters], fb->filter_selected);

  x = 15;

  b.skin_element_name = "XITK_NOSKIN_CHECK";
  b.callback          = NULL;
  b.state_callback    = fb_hidden_files;
  fb->w[_W_show_hidden] = xitk_noskin_button_create (fb->widget_list, &b, x, y+5, 10, 10);
  xitk_add_widget (fb->widget_list, fb->w[_W_show_hidden], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_button_set_state (fb->w[_W_show_hidden], fb->show_hidden_files);

  lbl.skin_element_name = NULL;
  lbl.label             = _("Show hidden file");
  lbl.callback          = NULL;
  lbl.userdata          = NULL;
  widget = xitk_noskin_label_create (fb->widget_list, &lbl, x + 15, y, w - 15, 20, fontname);
  xitk_add_widget (fb->widget_list, widget, XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
  xitk_widget_set_focus_redirect (widget, fb->w[_W_show_hidden]);

  y = WINDOW_HEIGHT - (23 + 15) - (23 + 8);
  w = (WINDOW_WIDTH - (4 * 15)) / 3;

  lb.button_type       = CLICK_BUTTON;
  lb.align             = ALIGN_CENTER;
  lb.state_callback    = NULL;
  lb.userdata          = (void *)fb;
  lb.skin_element_name = NULL;

  lb.label             = _("Rename");
  lb.callback          = fb_rename_file;
  fb->w[_W_rename] = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_rename], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = (WINDOW_WIDTH - w) / 2;

  lb.label             = _("Delete");
  lb.callback          = fb_delete_file;
  fb->w[_W_delete] = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_delete], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  x = WINDOW_WIDTH - (w + 15);

  lb.label             = _("Create a directory");
  lb.callback          = fb_create_directory;
  fb->w[_W_create] = xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_create], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  fb->w[_W_cb_button0] = fb->w[_W_cb_button1] = NULL;

  y = WINDOW_HEIGHT - (23 + 15);

  if(fb->cbb[0].label) {
    x = 15;

    lb.label             = fb->cbb[0].label;
    lb.callback          = fb_callback_button_cb;
    fb->w[_W_cb_button0] = xitk_noskin_labelbutton_create (fb->widget_list,
      &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
    xitk_add_widget (fb->widget_list, fb->w[_W_cb_button0], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

    if(fb->cbb[1].label) {
      x = (WINDOW_WIDTH - w) / 2;

      lb.label             = fb->cbb[1].label;
      lb.callback          = fb_callback_button_cb;
      fb->w[_W_cb_button1] = xitk_noskin_labelbutton_create (fb->widget_list,
        &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
      xitk_add_widget (fb->widget_list, fb->w[_W_cb_button1], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);
    }
  }

  x = WINDOW_WIDTH - (w + 15);

  lb.label             = _("Close");
  lb.callback          = _fb_exit;
  fb->w[_W_close] =  xitk_noskin_labelbutton_create (fb->widget_list,
    &lb, x, y, w, 23, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_NORM, XITK_NOSKIN_TEXT_INV, btnfontname);
  xitk_add_widget (fb->widget_list, fb->w[_W_close], XITK_WIDGET_STATE_ENABLE | XITK_WIDGET_STATE_VISIBLE);

  xitk_window_set_background_image (fb->xwin, bg);

  if(fb->w[_W_cb_button0])
    xitk_set_focus_to_widget(fb->w[_W_cb_button0]);

  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "filebrowser%u", (unsigned int) time(NULL));
    fb->widget_key = xitk_be_register_event_handler (buffer, fb->xwin, fb_event, fb, NULL, NULL);
  }
  fb->dialog = 0;

  fb->visible = 1;
  fb->running = 1;
  filebrowser_raise_window(fb);

  fb_getdir(fb);

  layer_above_video (fb->gui, fb->xwin);
  xitk_window_set_input_focus (fb->xwin);

  return fb;
}

