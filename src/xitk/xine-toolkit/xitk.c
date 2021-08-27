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
#define _XITK_C_

/* required to enable POSIX variant of getpwuid_r on solaris */
#define _POSIX_PTHREAD_SEMANTICS 1


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pwd.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "utils.h"
#include "dump.h"
#include "_xitk.h"
#include "tips.h"
#include "widget.h"
#include "menu.h"
#include "combo.h"
#include "font.h"

extern char **environ;
#undef TRACE_LOCKS

#ifdef TRACE_LOCKS
static int ml = 0;
#define MUTLOCK()                          \
  do {                                     \
    int i;                                 \
    ml++;                                  \
    for (i = 0; i < ml; i++) printf ("."); \
    printf ("LOCK\n");                     \
    pthread_mutex_lock (&xitk->mutex);     \
  } while (0)

#define MUTUNLOCK()                        \
  do {                                     \
    int i;                                 \
    for (i = 0; i < ml; i++) printf ("."); \
    printf ("UNLOCK\n");                   \
    ml--;                                  \
    pthread_mutex_unlock (&xitk->mutex);   \
  } while (0)

#else
#define MUTLOCK()   pthread_mutex_lock (&xitk->mutex)
#define MUTUNLOCK() pthread_mutex_unlock (&xitk->mutex)
#endif

int xitk_tags_get (const xitk_tagitem_t *from, xitk_tagitem_t *to) {
  const xitk_tagitem_t *stack1[32];
  xitk_tagitem_t *stack2[32];
  size_t bufsize = 0;
  uint32_t sp1, sp2;
  int res = 0;

  if (!from || !to)
    return 0;
  sp1 = 0;
  while (1) {
    if (from->type <= XITK_TAG_BUFSIZE) {
      if (from->type == XITK_TAG_END) {
        if (sp1 == 0)
          break;
        from = stack1[--sp1];
      } else if (from->type == XITK_TAG_SKIP) {
        from += from->value + 1;
      } else if (from->type == XITK_TAG_INSERT) {
        if (from->value && (sp1 < sizeof (stack1) / sizeof (stack1[0]) - 1)) {
          stack1[sp1++] = from + 1;
          from = (const xitk_tagitem_t *)from->value;
        } else {
          from += 1;
        }
      } else if (from->type == XITK_TAG_JUMP) {
        if (from->value)
          from = (const xitk_tagitem_t *)from->value;
        else
          break;
      } else /* from->type == XITK_TAG_BUFSIZE */ {
        from += 1;
      }
    } else {
      xitk_tagitem_t *q = to;

      sp2 = 0;
      while (1) {
        if (q->type <= XITK_TAG_BUFSIZE) {
          if (q->type == XITK_TAG_END) {
            if (sp2 == 0)
              break;
            q = stack2[--sp2];
          } else if (q->type == XITK_TAG_SKIP) {
            q += q->value + 1;
          } else if (q->type == XITK_TAG_INSERT) {
            if (q->value && (sp2 < sizeof (stack2) / sizeof (stack2[0]) - 1)) {
              stack2[sp2++] = q + 1;
              q = (xitk_tagitem_t *)q->value;
            } else {
              q += 1;
            }
          } else if (q->type == XITK_TAG_JUMP) {
            if (q->value)
              q = (xitk_tagitem_t *)q->value;
            else
              break;
          } else /* q->type == XITK_TAG_BUFSIZE */ {
            bufsize = q->value;
            q += 1;
          }
        } else if (from->type == q->type) {
          if ((from->type >= XITK_TAG_NAME) && bufsize && q->value) {
            if (from->value) {
              strlcpy ((char *)q->value, (const char *)from->value, bufsize);
            } else {
              char *s = (char *)q->value;
              s[0] = 0;
            }
          } else {
            q->value = from->value;
          }
          q += 1;
          res += 1;
          /* XXX: does anyone really want multiple copies?? break; */
        } else {
          q += 1;
        }
      }
      from += 1;
    }
  }
  return res;
}

typedef struct {
  int                               enabled;
  int                               offset_x;
  int                               offset_y;
} xitk_move_t;

typedef struct __xitk_s __xitk_t;

typedef struct {
  xitk_widget_list_t          wl;

  __xitk_t                   *xitk;
  int                         refs;

  xitk_move_t                 move;

  struct {
    int                       x, y;
  }                           old_pos, new_pos;

  int                         width;
  int                         height;

  xitk_hull_t                 expose;

  char                        name[64];
  xitk_register_key_t         key;
  void                       *user_data;

  xitk_be_event_handler_t    *event_handler;
  void                      (*destructor)(void *userdata);
  void                       *destr_data;
} __gfx_t;


struct __xitk_s {
  xitk_t                      x;

  xitk_dlist_t                gfxs;

  struct {
    struct {
      uint32_t                rgb, value;
    }                        *a;
    uint32_t                  used, size;
  }                           color_db;
  struct {
    xitk_color_info_t        *a;
    uint32_t                  used, size;
  }                           color_query;

  pthread_mutex_t             mutex;
  int                         running;
  xitk_register_key_t         key;

  struct {
    pthread_t                 thread;
    int                       running;
  }                           event_bridge;

  xitk_config_t              *config;
  xitk_signal_callback_t      sig_callback;
  void                       *sig_data;

  xitk_register_key_t         focused;

  xitk_widget_t              *menu;

  struct timeval              keypress;

  uint32_t                    qual; /** << track held mouse buttons */

  pid_t                       xitk_pid;

  struct {
    xitk_widget_t          *widget_in;
  }                         clipboard;
};

static void __fx_ref (__gfx_t *fx) {
  fx->refs++;
}

static void __fx_delete (__gfx_t *fx) {
  __xitk_t *xitk = fx->xitk;

  if (xitk->x.verbosity >= 2)
    printf ("xitk_gfx_delete (%d) = \"%s\".\n", fx->key, fx->name);

  xitk_dnode_remove (&fx->wl.node);

  fx->event_handler = NULL;
  fx->user_data = NULL;

  /* dialog.c uses this to clean out by either widget callback
   * or xitk_unregister_event_handler () from outside.
   * that includes another xitk_widget_list_defferred_destroy (),
   * which does not harm here because
   * a) widget list is still there, and
   * b) fx->refs == -1 prevents recursion. */
  if (fx->destructor)
    fx->destructor (fx->destr_data);
  fx->destructor = NULL;
  fx->destr_data = NULL;

  xitk_destroy_widgets (&fx->wl);
  xitk_shared_image_list_delete (&fx->wl);
  xitk_dlist_clear (&fx->wl.list);

  free (fx);
}

static void __fx_unref (__gfx_t *fx) {
  if (--fx->refs == 0)
    __fx_delete (fx);
}

static void _xitk_reset_hull (xitk_hull_t *hull) {
  hull->x1 = 0x7fffffff;
  hull->x2 = 0;
  hull->y1 = 0x7fffffff;
  hull->y2 = 0;
}

static void *xitk_event_bridge_thread (void *data) {
  __xitk_t *xitk = data;
    xitk_be_window_t *bewin = NULL;

  if (xitk->x.verbosity >= 2)
    printf ("xitk.event_bridge.start.\n");

  MUTLOCK ();
  while (xitk->event_bridge.running) {
    xitk_be_event_t event;
    __gfx_t  *fx;

    MUTUNLOCK ();
    if (xitk->x.d->next_event (xitk->x.d, &event, bewin, XITK_EV_EXPOSE, 33)) {
      xitk_window_t *xwin;

      bewin = event.window;
      xwin = bewin ? bewin->data : NULL;
      fx = xwin ? (__gfx_t *)xwin->widget_list : NULL;
      MUTLOCK ();
      if (!fx) {
        bewin = NULL;
        continue;
      }
      fx->expose.x1 = xitk_min (fx->expose.x1, event.x);
      fx->expose.x2 = xitk_max (fx->expose.x2, event.x + event.w);
      fx->expose.y1 = xitk_min (fx->expose.y1, event.y);
      fx->expose.y2 = xitk_max (fx->expose.y2, event.y + event.h);
      if (event.more > 0)
        continue;
      if (xitk->x.verbosity >= 2)
        printf ("xitk.event_bridge.expose (%s): x %d-%d, y %d-%d.\n",
          fx->name, fx->expose.x1, fx->expose.x2, fx->expose.y1, fx->expose.y2);
      xitk_partial_paint_widget_list (&fx->wl, &fx->expose);
      _xitk_reset_hull (&fx->expose);
    } else {
      MUTLOCK ();
    }
    bewin = NULL;
  }
  MUTUNLOCK ();

  if (xitk->x.verbosity >= 2)
    printf ("xitk.event_bridge.stop.\n");

  return NULL;
}

static void xitk_event_bridge_start (__xitk_t *xitk) {
  pthread_attr_t pth_attrs;

  pthread_attr_init (&pth_attrs);
  MUTLOCK ();
  xitk->event_bridge.running = 1;
  if (pthread_create (&xitk->event_bridge.thread, &pth_attrs, xitk_event_bridge_thread, xitk))
    xitk->event_bridge.running = 0;
  MUTUNLOCK ();
  pthread_attr_destroy (&pth_attrs);
}

static void xitk_event_bridge_stop (__xitk_t *xitk) {
  MUTLOCK ();
  if (xitk->event_bridge.running) {
    void *dummy;

    xitk->event_bridge.running = 0;
    MUTUNLOCK ();
    pthread_join (xitk->event_bridge.thread, &dummy);
    return;
  }
  MUTUNLOCK ();
}


/* The color cache database.
 * XAllocColor () is slow - not because it is much work for the server,
 * but because we need to wait for the result. */

static void xitk_color_db_init (__xitk_t *xitk) {
  xitk->color_db.a = NULL;
  xitk->color_db.used = 0;
  xitk->color_db.size = 0;

  xitk->color_query.a = NULL;
  xitk->color_query.used = 0;
  xitk->color_query.size = 0;
}

void xitk_color_db_flush (xitk_t *_xitk) {
  __xitk_t *xitk;
  uint32_t u;

  xitk_container (xitk, _xitk, x);
  for (u = xitk->color_db.used; u > 0; u--)
    xitk->x.d->color._delete (xitk->x.d, xitk->color_db.a[u - 1].value);
  xitk->color_db.used = 0;
  xitk->color_query.used = 0;
}

static void xitk_color_db_deinit (__xitk_t *xitk) {
  free (xitk->color_db.a);
  xitk->color_db.a = NULL;
  xitk->color_db.used = 0;
  xitk->color_db.size = 0;

  free (xitk->color_query.a);
  xitk->color_query.a = NULL;
  xitk->color_query.used = 0;
  xitk->color_query.size = 0;
}

uint32_t xitk_color_db_get (xitk_t *_xitk, uint32_t rgb) {
  __xitk_t *xitk;
  xitk_color_info_t info;
  uint32_t b, m, e;
  
  xitk_container (xitk, _xitk, x);

  b = 0;
  e = xitk->color_db.used;
  if (e) {
    do {
      m = (b + e) >> 1;
      if (rgb == xitk->color_db.a[m].rgb)
        return xitk->color_db.a[m].value;
      if (rgb < xitk->color_db.a[m].rgb)
        e = m;
      else
        b = m + 1;
    } while (b != e);
  }

  if (xitk->color_db.used >= xitk->color_db.size) {
    void *n = realloc (xitk->color_db.a, (xitk->color_db.size + 32) * sizeof (xitk->color_db.a[0]));

    if (!n)
      return 0;
    xitk->color_db.a = n;
    xitk->color_db.size += 32;
  }
  for (e = xitk->color_db.used; e > b; e--)
    xitk->color_db.a[e] = xitk->color_db.a[e - 1];

  xitk->color_db.used += 1;
  xitk->color_db.a[b].rgb = rgb;
  info.want = rgb;
  xitk->x.d->color._new (xitk->x.d, &info);
  xitk->color_db.a[b].value = info.value;

  b = 0;
  e = xitk->color_query.used;
  if (e) {
    do {
      m = (b + e) >> 1;
      if (info.value == xitk->color_query.a[m].value)
        return info.value;
      if (info.value < xitk->color_query.a[m].value)
        e = m;
      else
        b = m + 1;
    } while (b != e);
  }

  if (xitk->color_query.used >= xitk->color_query.size) {
    void *n = realloc (xitk->color_query.a, (xitk->color_query.size + 32) * sizeof (xitk->color_query.a[0]));

    if (!n)
      return info.value;
    xitk->color_query.a = n;
    xitk->color_query.size += 32;
  }
  for (e = xitk->color_query.used; e > b; e--)
    xitk->color_query.a[e] = xitk->color_query.a[e - 1];

  xitk->color_query.used += 1;
  xitk->color_query.a[b] = info;

  return info.value;
}

int xitk_color_db_query_value (xitk_t *_xitk, xitk_color_info_t *info) {
  __xitk_t *xitk;
  uint32_t b, m, e;
  
  xitk_container (xitk, _xitk, x);

  b = 0;
  e = xitk->color_query.used;
  if (e) {
    do {
      m = (b + e) >> 1;
      if (info->value == xitk->color_query.a[m].value) {
        *info = xitk->color_query.a[m];
        return 1;
      }
      if (info->value < xitk->color_query.a[m].value)
        e = m;
      else
        b = m + 1;
    } while (b != e);
  }

  memset(info, 0, sizeof(*info));
  return 0;
}


void xitk_clipboard_unregister_widget (xitk_widget_t *w) {
  if (w && w->wl && w->wl->xitk) {
    __xitk_t *xitk;

    xitk_container (xitk, w->wl->xitk, x);
    if (xitk->clipboard.widget_in == w)
      xitk->clipboard.widget_in = NULL;
  }
}

static void _xitk_clipboard_init (__xitk_t *xitk) {
  xitk->clipboard.widget_in = NULL;
}

static void _xitk_clipboard_deinit (__xitk_t *xitk) {
  xitk->clipboard.widget_in = NULL;
}

int xitk_clipboard_set_text (xitk_widget_t *w, const char *text, int text_len) {
  __xitk_t *xitk;

  if (!w || !text || (text_len <= 0))
    return 0;
  if (!w->wl)
    return 0;
  xitk_container (xitk, w->wl->xitk, x);
  if (!xitk || !w->wl->xwin)
    return 0;
  if (!w->wl->xwin->bewin)
    return 0;
  return w->wl->xwin->bewin->set_clip_utf8 (w->wl->xwin->bewin, text, text_len);
}

int xitk_clipboard_get_text (xitk_widget_t *w, char **text, int max_len) {
  __xitk_t *xitk;
  int l;

  if (!w || !text || (max_len <= 0))
    return 0;
  if (!w->wl)
    return 0;
  xitk_container (xitk, w->wl->xitk, x);
  if (!xitk || !w->wl->xwin)
    return 0;
  if (!w->wl->xwin->bewin)
    return 0;
  l = w->wl->xwin->bewin->get_clip_utf8 (w->wl->xwin->bewin, text, max_len);
  xitk->clipboard.widget_in = l < 0 ? w : NULL;
  return l;
}

/*
 * Execute a shell command.
 */
int xitk_system(int dont_run_as_root, const char *command) {
  int pid, status;

  /*
   * Don't permit run as root
   */
  if(dont_run_as_root) {
    if(getuid() == 0)
      return -1;
  }

  if(command == NULL)
    return 1;

  pid = fork();

  if(pid == -1)
    return -1;

  if(pid == 0) {
    char *argv[4];
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = (char*)command;
    argv[3] = 0;
    execve("/bin/sh", argv, environ);
    exit(127);
  }

  do {
    if(waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
	return -1;
    }
    else {
      return WEXITSTATUS(status);
    }
  } while(1);

  return -1;
}

/*
 * A thread-safe usecond sleep
 */
void xitk_usec_sleep(unsigned long usec) {
#ifdef HAVE_NANOSLEEP
  /* nanosleep is prefered on solaris, because it's mt-safe */
  struct timespec ts, remaining;

  ts.tv_sec =   usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  while (nanosleep (&ts, &remaining) == -1 && errno == EINTR)
    ts = remaining;
#else
  usleep(usec);
#endif
}

void xitk_set_current_menu(xitk_t *_xitk, xitk_widget_t *menu) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  xitk_widgets_delete (&xitk->menu, 1);
  xitk->menu = menu;
}

void xitk_unset_current_menu(xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  xitk->menu = NULL;
}

/*
 *
 */

xitk_t *gXitk;

static void xitk_signal_handler(int sig) {
  __xitk_t *xitk;
  pid_t cur_pid = getppid();


  xitk_container (xitk, gXitk, x);
  if (xitk) {
    /* First, call registered handler */
    if (cur_pid == xitk->xitk_pid) {
      if (xitk->sig_callback)
        xitk->sig_callback (sig, xitk->sig_data);
    }
    xitk->running = 0;
  }

  switch (sig) {

  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
    if (xitk && (cur_pid == xitk->xitk_pid)) {
      xitk_dlist_clear (&xitk->gfxs);
      xitk_config_deinit (xitk->config);
      XITK_FREE (xitk);
      gXitk = NULL;
      exit(1);
    }
    break;

  case SIGSEGV:
    {
      fprintf(stderr, "xiTK received SIGSEGV signal, RIP.\n");
#ifndef DEBUG
      abort();
#elif defined(HAVE_EXECINFO_H)
      void    *backtrace_array[255];
      char   **backtrace_strings;
      int      entries, i;

      if((entries = backtrace(backtrace_array, 255))) {
	if((backtrace_strings = backtrace_symbols(backtrace_array, entries))) {
	  printf("Backtrace:\n");

	  for(i = 0; i < entries; i++) {
	    printf("  [%d] %s\n", i, backtrace_strings[i]);
	  }

	  free(backtrace_strings);
	  printf("--\n");
	}
      }
#endif
    }
    break;


  }
}

uint32_t xitk_get_wm_type (xitk_t *xitk) {
  return xitk->d->wm_type;
}

int xitk_get_layer_level(xitk_t *xitk) {
  uint32_t wm_type = xitk_get_wm_type(xitk);
  int level = 10;

  if ((wm_type & WM_TYPE_GNOME_COMP) || (wm_type & WM_TYPE_EWMH_COMP))
    level = 10;

  switch (wm_type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_UNKNOWN:
  case WM_TYPE_KWIN:
  case WM_TYPE_SAWFISH:
  case WM_TYPE_METACITY: /* Untested */
  case WM_TYPE_ICE:
  case WM_TYPE_AFTERSTEP:
  case WM_TYPE_BLACKBOX:
  case WM_TYPE_WINDOWMAKER:
    level = 10; /* Wrong, but we need to provide a default value */
    break;
  case WM_TYPE_XFCE:
    level = 8;
    break;
  case WM_TYPE_E:
    level = 6;
    break;
  case WM_TYPE_MOTIF:
  case WM_TYPE_LARSWM:
    level = 0;
    break;
  }
  return level;
}

void xitk_window_update_tree (xitk_window_t *xwin, uint32_t mask_and_flags) {
  __xitk_t *xitk;
  __gfx_t *fx, *_main, *vice, *trans;
  xitk_tagitem_t tags[] = {
    {XITK_TAG_TRANSIENT_FOR, (uintptr_t)NULL},
    {XITK_TAG_WIN_FLAGS, 0},
    {XITK_TAG_END, 0}
  };

  if (!xwin)
    return;
  xitk_container (xitk, xwin->xitk, x);
  if (!xitk)
    return;

  MUTLOCK ();

  if (xitk->x.verbosity >= 2) {
    xitk_container (fx, xwin->widget_list, wl);
    if (fx)
        printf ("xitk.window.update_tree (%s, 0x%x).\n", fx->name, (unsigned int)mask_and_flags);
  }
  /* find latest main and vice. */
  _main = vice = NULL;
  for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
    if (fx->wl.xwin && fx->wl.xwin->bewin) {
      if ((fx->wl.xwin->role == XITK_WR_ROOT) || (fx->wl.xwin->role == XITK_WR_MAIN)) {
        _main = fx;
      } else if (fx->wl.xwin->role == XITK_WR_VICE) {
        vice = fx;
      }
    }
  }
  /* find new transient for win (NULL = root). */
  trans = (_main && (_main->wl.xwin->flags & XITK_WINF_VISIBLE)) ? _main : NULL;
  /* take action. */
  fx = (_main && (_main->wl.xwin == xwin)) ? _main
     : (vice && (vice->wl.xwin == xwin)) ? vice
     : NULL;
  if (fx) {
    __gfx_t *helper;
    /* main or vice changed, adjust all.
     * move main/vice to front of list, and find first helper. */
    if (vice) {
      xitk_dnode_remove (&vice->wl.node);
      xitk_dlist_add_head (&xitk->gfxs, &vice->wl.node);
    }
    if (_main) {
      xitk_dnode_remove (&_main->wl.node);
      xitk_dlist_add_head (&xitk->gfxs, &_main->wl.node);
    }
    helper = vice ? vice : _main;
    helper = (__gfx_t *)(helper->wl.node.next);
    /* do not touch root visibility. */
    if ((fx == _main) && (xwin->role == XITK_WR_ROOT))
      mask_and_flags &= ~((XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16);
    /* do not hide them both, it would make us nearly unreachable for the user. */
    if ((mask_and_flags ^ xwin->flags) & (mask_and_flags >> 16) & XITK_WINF_VISIBLE) {
      if (fx == _main) {
        trans = (mask_and_flags & XITK_WINF_VISIBLE) ? _main : NULL;
        if (vice) {
          if (vice->wl.xwin->flags & XITK_WINF_VISIBLE) {
            /* do main */
            tags[0].value = (uintptr_t)NULL;
            tags[1].value = mask_and_flags;
            xwin->bewin->set_props (xwin->bewin, tags);
            xwin->bewin->get_props (xwin->bewin, tags);
            xwin->flags = tags[1].value;
            /* toggle vice mode, remap vice to tell task bar about the new type. */
            tags[1].value = (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16;
            vice->wl.xwin->bewin->set_props (vice->wl.xwin->bewin, tags + 1);
            xitk_window_set_wm_window_type (vice->wl.xwin,
              !(mask_and_flags & XITK_WINF_VISIBLE) ? WINDOW_TYPE_NORMAL
              : !(xitk_get_wm_type (&xitk->x) & WM_TYPE_KWIN) ? WINDOW_TYPE_TOOLBAR : WINDOW_TYPE_NONE);
            tags[0].value = (uintptr_t)(trans ? trans->wl.xwin->bewin : NULL);
            tags[1].value = ((XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16) | XITK_WINF_VISIBLE;
            vice->wl.xwin->bewin->set_props (vice->wl.xwin->bewin, tags);
            vice->wl.xwin->bewin->get_props (vice->wl.xwin->bewin, tags);
            vice->wl.xwin->bewin->raise (vice->wl.xwin->bewin);
            vice->wl.xwin->flags = tags[1].value;
          } else {
            /* do main */
            mask_and_flags &= ~XITK_WINF_ICONIFIED;
            mask_and_flags |= (mask_and_flags & XITK_WINF_VISIBLE) ? 0 : XITK_WINF_ICONIFIED;
            mask_and_flags |= (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16;
            tags[0].value = (uintptr_t)NULL;
            tags[1].value = mask_and_flags;
            xwin->bewin->set_props (xwin->bewin, tags);
            xwin->bewin->get_props (xwin->bewin, tags);
            xwin->flags = tags[1].value;
            /* toggle vice mode */
            xitk_window_set_wm_window_type (vice->wl.xwin,
              !(mask_and_flags & XITK_WINF_VISIBLE) ? WINDOW_TYPE_NORMAL
              : !(xitk_get_wm_type (&xitk->x) & WM_TYPE_KWIN) ? WINDOW_TYPE_TOOLBAR : WINDOW_TYPE_NONE);
            /* retrans vice */
            tags[0].value = (uintptr_t)(((fx == _main) || !trans) ? NULL : trans->wl.xwin->bewin);
            tags[1].value = 0;
            vice->wl.xwin->bewin->set_props (vice->wl.xwin->bewin, tags);
          }
        } else {
          /* do main. */
#if 0
          /* HACK: on shutdown (hide main with no vice), dont iconify instead of unmap.
           * avoid kwin 4 losing focus completely when destroying an iconified window. */
          mask_and_flags &= ~XITK_WINF_ICONIFIED;
          mask_and_flags |= (mask_and_flags & XITK_WINF_VISIBLE) ? 0 : XITK_WINF_ICONIFIED;
          mask_and_flags |= (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16;
#endif
          tags[0].value = (uintptr_t)NULL;
          tags[1].value = mask_and_flags;
          xwin->bewin->set_props (xwin->bewin, tags);
          xwin->bewin->get_props (xwin->bewin, tags);
          xwin->flags = tags[1].value;
        }
      } else { /* fx == vice */
        mask_and_flags &= ~XITK_WINF_ICONIFIED;
        mask_and_flags |= (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16;
        if (!(_main && (_main->wl.xwin->flags & XITK_WINF_VISIBLE)))
          mask_and_flags |= (mask_and_flags & XITK_WINF_VISIBLE) ? 0 : XITK_WINF_ICONIFIED;
        tags[0].value = (uintptr_t)(trans ? trans->wl.xwin->bewin : NULL);
        tags[1].value = mask_and_flags;
        xwin->bewin->set_props (xwin->bewin, tags);
        xwin->bewin->get_props (xwin->bewin, tags);
        xwin->flags = tags[1].value;
        if (xwin->flags & XITK_WINF_VISIBLE)
          xwin->bewin->raise (xwin->bewin);
      }
    } else { /* no visibility change */
      helper = (__gfx_t *)(fx->wl.node.next);
      mask_and_flags &= ~((XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16);
      tags[0].value = (uintptr_t)(((fx == _main) || !trans) ? NULL : trans->wl.xwin->bewin);
      tags[1].value = mask_and_flags;
      xwin->bewin->set_props (xwin->bewin, tags);
      xwin->bewin->get_props (xwin->bewin, tags);
      xwin->flags = tags[1].value;
      if (xwin->flags & XITK_WINF_VISIBLE)
        xwin->bewin->raise (xwin->bewin);
    }
    /* finally, the helpers. */
    mask_and_flags = vice ? ((vice->wl.xwin->flags & 0xffff) | ((XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED) << 16)) : 0;
    for (fx = helper; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
      if (!fx->wl.xwin)
        continue;
      if (!fx->wl.xwin->bewin)
        continue;
      tags[0].value = (uintptr_t)(((fx == _main) || !trans) ? NULL : trans->wl.xwin->bewin);
      tags[1].value = mask_and_flags;
      fx->wl.xwin->bewin->set_props (fx->wl.xwin->bewin, tags + (fx->wl.xwin->role == XITK_WR_SUBMENU ? 1 : 0));
      fx->wl.xwin->bewin->get_props (fx->wl.xwin->bewin, tags + 1);
      fx->wl.xwin->flags = tags[1].value;
      if (fx->wl.xwin->flags & XITK_WINF_VISIBLE)
        fx->wl.xwin->bewin->raise (fx->wl.xwin->bewin);
    }
  } else {
    /* do just this 1. */
    if (xwin->bewin) {
      tags[0].value = (uintptr_t)(trans ? trans->wl.xwin->bewin : NULL);
      tags[1].value = mask_and_flags;
      xwin->bewin->set_props (xwin->bewin, tags + (xwin->role == XITK_WR_SUBMENU ? 1 : 0));
      xwin->bewin->get_props (xwin->bewin, tags + 1);
      xwin->flags = tags[1].value;
    }
  }

  MUTUNLOCK ();
}

/*
 * Create a new widget_list, store the pointer in private
 * list of xitk_widget_list_t, then return the widget_list pointer.
 */
static __gfx_t *_xitk_gfx_new (__xitk_t *xitk) {
  __gfx_t *fx;

  fx = (__gfx_t *)xitk_xmalloc (sizeof (*fx));
  if (!fx)
    return NULL;

  xitk_dnode_init (&fx->wl.node);
  xitk_dlist_init (&fx->wl.list);
  fx->wl.xitk               = &xitk->x;
  fx->wl.xwin               = NULL;
  fx->wl.shared_images      = NULL;
  fx->wl.widget_focused     = NULL;
  fx->wl.widget_under_mouse = NULL;
  fx->wl.widget_pressed     = NULL;
  fx->wl.mouse.x            = 0;
  fx->wl.mouse.y            = 0;
  fx->wl.qual               = 0;

  fx->xitk                  = xitk;
  fx->refs                  = 1;
  fx->name[0]               = 0;
  fx->move.enabled          = 0;
  fx->move.offset_x         = 0;
  fx->move.offset_y         = 0;
  fx->old_pos.x             = 0;
  fx->old_pos.y             = 0;
  fx->new_pos.x             = 0;
  fx->new_pos.y             = 0;
  fx->width                 = 0;
  fx->height                = 0;
  _xitk_reset_hull (&fx->expose);
  fx->user_data             = NULL;
  fx->event_handler         = NULL;
  fx->destructor            = NULL;
  fx->destr_data            = NULL;
  fx->key                   = 0;

  return fx;
}

/* nasty xitk_window_widget_list () helper. */
xitk_widget_list_t *xitk_widget_list_get (xitk_t *_xitk, xitk_window_t *xwin) {
  __xitk_t *xitk;
  __gfx_t *fx;

  xitk_container (xitk, _xitk, x);
  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->x.be);

  if (xwin) {
    MUTLOCK ();
    for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next)
      if (xwin == fx->wl.xwin)
        break;
    if (fx->wl.node.next) {
      fx->refs += 1;
      fx->wl.xwin = xwin;
      MUTUNLOCK ();
      return &fx->wl;
    }
    MUTUNLOCK ();
  }

  fx = _xitk_gfx_new (xitk);
  if (!fx)
    return NULL;
  fx->wl.xwin = xwin;
  strcpy (fx->name, "XITK_WIN_WL");
  MUTLOCK();
  xitk_dlist_add_tail (&xitk->gfxs, &fx->wl.node);
  MUTUNLOCK ();

  if (xitk->x.verbosity >= 2)
    printf ("xitk_gfx_new (\"%s\") = %d.\n", fx->name, fx->key);
  return &fx->wl;
}

/*
 * Register a callback function called when a signal happen.
 */
void xitk_register_signal_handler(xitk_t *xitk, xitk_signal_callback_t sigcb, void *user_data) {
  __xitk_t *_xitk;

  xitk_container (_xitk, xitk, x);
  if (sigcb) {
    _xitk->sig_callback = sigcb;
    _xitk->sig_data     = user_data;
  }
}

size_t xitk_be_event_name (const xitk_be_event_t *event, char *buf, size_t buf_size) {
  if (!event)
    return 0;
  if (!event->window)
    return 0;
  if (!event->window->display)
    return 0;
  return event->window->display->event_name (event->window->display, event, buf, buf_size);
}

/*
 * Register a window, with his own event handler, callback
 * for DND events, and widget list.
 */

xitk_register_key_t xitk_be_register_event_handler (const char *name, xitk_window_t *xwin,
  xitk_be_event_handler_t *event_handler, void *user_data,
  void (*destructor) (void *data), void *destr_data) {
  __xitk_t *xitk;
  __gfx_t   *fx;
  xitk_widget_list_t *wl;

  //  printf("%s()\n", __FUNCTION__);

  ABORT_IF_NULL (xwin);

  wl = xwin->widget_list;
  if (!wl) {
    xitk_t *_xitk = xwin->xitk;
    xitk_container (xitk, _xitk, x);
    fx = _xitk_gfx_new (xitk);
    if (!fx)
      return -1;
    xwin->widget_list = &fx->wl;
  } else {
    xitk_container (fx, wl, wl);
    xitk = fx->xitk;
    MUTLOCK ();
    xitk_dnode_remove (&fx->wl.node);
    fx->refs += 1;
    MUTUNLOCK ();
  }
  fx->wl.xwin = xwin;

  if (name)
    strlcpy (fx->name, name, sizeof (fx->name));
  else if (!fx->name[0])
    strcpy (fx->name, "NO_SET");

  if (fx->wl.xwin->bewin) {
    xitk_tagitem_t tags1[] = {
      {XITK_TAG_X, 0},
      {XITK_TAG_Y, 0},
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
      {XITK_TAG_WIN_FLAGS, 0},
      {XITK_TAG_END, 0}
    };
    xitk_tagitem_t tags2[] = {
      {XITK_TAG_NAME, (uintptr_t)fx->name},
      {XITK_TAG_END, 0}
    };
    fx->wl.xwin->bewin->get_props (fx->wl.xwin->bewin, tags1);
    fx->new_pos.x = tags1[0].value;
    fx->new_pos.y = tags1[1].value;
    fx->wl.xwin->width = fx->width = tags1[2].value;
    fx->wl.xwin->height = fx->height = tags1[3].value;
    fx->wl.xwin->flags = tags1[4].value;
    fx->wl.xwin->bewin->set_props (fx->wl.xwin->bewin, tags2);
  }

  fx->user_data = user_data;

  fx->event_handler = event_handler;
  fx->destructor = destructor;
  fx->destr_data = destr_data;

  MUTLOCK ();
  xwin->key = fx->key = ++xitk->key;
  xitk_dlist_add_tail (&xitk->gfxs, &fx->wl.node);
  /* set new focus early, to hint menu not to revert it. */
  if (fx->wl.xwin->flags & XITK_WINF_FOCUS)
    xitk->focused = fx->key;
  MUTUNLOCK ();

  if (xitk->x.verbosity >= 2)
    printf ("xitk_gfx_new (\"%s\") = %d.\n", fx->name, fx->key);
  return fx->key;
}

static __gfx_t *__fx_from_key (__xitk_t *xitk, xitk_register_key_t key) {
  /* fx->key is set uniquely in xitk_register_event_handler, then never changed.
   * MUTLOCK () is enough. */
  __gfx_t *fx;
  for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
    if (fx->key == key)
      return fx;
  }
  return NULL;
}

/*
 * Remove from the list the window/event_handler
 * specified by the key.
 */
void xitk_unregister_event_handler (xitk_t *_xitk, xitk_register_key_t *key) {
  __xitk_t *xitk;
  __gfx_t  *fx;

  //  printf("%s()\n", __FUNCTION__);

  if (!key || !_xitk)
    return;
  if (!*key)
    return;
  xitk_container (xitk, _xitk, x);
  MUTLOCK ();
  fx = __fx_from_key (xitk, *key);
  if (fx) {
    /* NOTE: After return from this, application may free () fx->user_data. */
    fx->event_handler   = NULL;
    fx->user_data       = NULL;
    __fx_unref (fx);
  }
  MUTUNLOCK ();
  *key = 0;
}

void xitk_widget_list_defferred_destroy(xitk_widget_list_t *wl) {
  __xitk_t *xitk;
  __gfx_t  *fx;

  if (!wl)
    return;
  xitk_container (fx, wl, wl);
  xitk = fx->xitk;
  if (!xitk)
    return;

  MUTLOCK ();
  __fx_unref (fx);
  MUTUNLOCK ();
}


/*
 * Copy window information matching with key in passed window_info_t struct.
 */
int xitk_get_window_info (xitk_t *_xitk, xitk_register_key_t key, window_info_t *winf) {
  __xitk_t *xitk;
  __gfx_t  *fx;

  if (!winf)
    return 0;
  if (key && _xitk) {
    xitk_container (xitk, _xitk, x);
    MUTLOCK ();
    fx = __fx_from_key (xitk, key);
    if (fx && fx->wl.xwin) {
      xitk_tagitem_t tags[] = {
        {XITK_TAG_X, 0},
        {XITK_TAG_Y, 0},
        {XITK_TAG_WIDTH, 0},
        {XITK_TAG_HEIGHT, 0},
        {XITK_TAG_END, 0}
      };

      fx->wl.xwin->bewin->get_props (fx->wl.xwin->bewin, tags);
      winf->x      = fx->new_pos.x = tags[0].value;
      winf->y      = fx->new_pos.y = tags[1].value;
      winf->width  = fx->wl.xwin->width = fx->width = tags[2].value;
      winf->height = fx->wl.xwin->height = fx->height = tags[3].value;
      winf->xwin   = fx->wl.xwin;
      strcpy (winf->name, fx->name);
      MUTUNLOCK ();
      return 1;
    }
    MUTUNLOCK ();
  }
  winf->x      = 0;
  winf->y      = 0;
  winf->height = 0;
  winf->width  = 0;
  winf->xwin   = NULL;
  winf->name[0] = 0;
  return 0;
}

xitk_window_t *xitk_get_window (xitk_t *_xitk, xitk_register_key_t key) {
  xitk_window_t *w = NULL;

  if (key && _xitk) {
    __xitk_t *xitk;
    __gfx_t  *fx;

    xitk_container (xitk, _xitk, x);
    MUTLOCK ();
    fx = __fx_from_key (xitk, key);
    if (fx)
      w = fx->wl.xwin;
    MUTUNLOCK ();
  }
  return w;
}

xitk_register_key_t xitk_get_focus_key (xitk_t *_xitk) {
  __xitk_t *xitk;
  xitk_register_key_t key;

  xitk_container (xitk, _xitk, x);
  if (!xitk)
    return 0;

  MUTLOCK ();
  key = xitk->focused;
  MUTUNLOCK ();
  return key;
}

xitk_register_key_t xitk_set_focus_key (xitk_t *_xitk, xitk_register_key_t key, int focus) {
  __xitk_t *xitk;
  __gfx_t  *fx;
  xitk_register_key_t prev_key;

  xitk_container (xitk, _xitk, x);
  if (!xitk)
    return 0;

  MUTLOCK ();
  prev_key = xitk->focused;
  if (focus) {
    for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
      if (fx->key == key)
        break;
    }
    if (fx->wl.node.next && fx->wl.xwin && fx->wl.xwin->bewin) {
      xitk_tagitem_t tags[] = {
        {XITK_TAG_WIN_FLAGS, (XITK_WINF_FOCUS << 16) + XITK_WINF_FOCUS},
        {XITK_TAG_END, 0}
      };
      fx->wl.xwin->bewin->set_props (fx->wl.xwin->bewin, tags);
      fx->wl.xwin->bewin->get_props (fx->wl.xwin->bewin, tags);
      fx->wl.xwin->flags = tags[0].value;
    }
  } else {
    xitk->focused = key;
  }
  MUTUNLOCK ();
  return prev_key;
}

/*
 * Here events are handled. All widget are locally
 * handled, then if a event handler callback was passed
 * at register time, it will be called.
 */
static void xitk_handle_event (__xitk_t *xitk, xitk_be_event_t *event) {
  __gfx_t  *fx;
  xitk_window_t *xwin;
  int handled;

  xwin = event->window ? event->window->data : NULL;
  do {
    if (xwin) {
      for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
        if (fx->wl.xwin == xwin) {
          __fx_ref (fx);
          break;
        }
      }
      if (fx->wl.node.next)
        break;
    }
    xwin = NULL;
    fx = NULL;
  } while (0);

  handled = 0;
  switch (event->type) {

    case XITK_EV_CLIP_READY:
      if (xwin && xitk->clipboard.widget_in) {
        widget_event_t we;

        we.type = WIDGET_EVENT_CLIP_READY;
        (void)xitk->clipboard.widget_in->event (xitk->clipboard.widget_in, &we, NULL);
        xitk->clipboard.widget_in = NULL;
      }
      break;

    case XITK_EV_SHOW:
      if (xwin && ((xwin->flags & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) != XITK_WINF_VISIBLE))
        xitk_window_update_tree (xwin, 0);
      break;

    case XITK_EV_HIDE:
      if (xwin && ((xwin->flags & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) == XITK_WINF_VISIBLE))
        xitk_window_update_tree (xwin, 0);
      break;

    case XITK_EV_KEY_UP:
      gettimeofday (&xitk->keypress, 0);
      if (fx && fx->wl.widget_focused)
        handled = xitk_widget_key_event (fx->wl.widget_focused, event->utf8, event->qual, 1);
      break;

    case XITK_EV_KEY_DOWN:
      if (fx) {
        static const uint8_t t[XITK_KEY_LASTCODE + 1] = {
          [XITK_KEY_ESCAPE] = 32,
          [XITK_KEY_TAB] = 1,
          [XITK_KEY_KP_TAB] = 1,
          [XITK_KEY_ISO_LEFT_TAB] = 1,
          [XITK_KEY_RETURN] = 2,
          [XITK_KEY_NUMPAD_ENTER] = 2,
          [XITK_KEY_ISO_ENTER] = 2,
          [XITK_KEY_UP] = 8,
          [XITK_KEY_DOWN] = 16,
          [XITK_KEY_PREV] = 8,
          [XITK_KEY_NEXT] = 16,
          [XITK_KEY_LASTCODE] = 4
        };
        uint8_t       *kbuf = (uint8_t *)event->utf8;
        int            modifier = event->qual;
        xitk_widget_t *w;

        xitk_tips_hide_tips (xitk->x.tips);

        w = fx->wl.widget_focused;

        /* hint possible menu location */
        if ((event->utf8[0] == XITK_CTRL_KEY_PREFIX) && (event->utf8[1] == XITK_KEY_MENU) && fx->wl.xwin->bewin) {
          xitk_tagitem_t tags[] = {{XITK_TAG_X, 0}, {XITK_TAG_Y, 0}, {XITK_TAG_END, 0}};
          fx->wl.xwin->bewin->get_props (fx->wl.xwin->bewin, tags);
          event->w = tags[0].value;
          event->h = tags[1].value;
          if (w) {
            event->w += w->x + 10;
            event->h += w->y + 10;
          } else {
            event->w += 20;
            event->h += 20;
          }
        }

        handled = xitk_widget_key_event (w, event->utf8, event->qual, 0);
        /* this may have changed focus or deleted w. */
        w = fx->wl.widget_focused;

        if (!handled) {
          if (kbuf[0] == ' ')
            kbuf[0] = XITK_CTRL_KEY_PREFIX, kbuf[1] = XITK_KEY_LASTCODE;
          if (kbuf[0] == XITK_CTRL_KEY_PREFIX) {
            if ((t[kbuf[1]] == 1) || (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) && (t[kbuf[1]] & 34))) {
              handled = 1;
              xitk_set_focus_to_next_widget (&fx->wl, (modifier & MODIFIER_SHIFT), modifier);
            } else if (kbuf[1] == XITK_KEY_ESCAPE) {
              if (w && (w->type & WIDGET_GROUP_MENU)) {
                /* close menu */
                handled = 1;
                w = xitk_menu_get_menu (w);
                xitk_widgets_delete (&w, 1);
              }
            } else if ((kbuf[1] == XITK_KEY_LEFT) && w && (w->type & WIDGET_GROUP_MENU)) {
              /* close menu branch */
              handled = 1;
              xitk_menu_destroy_branch (w);
            } else if ((t[kbuf[1]] & 24) && w && (w->type & WIDGET_GROUP_MENU)) {
              /* next/previous menu item */
              handled = 1;
              xitk_set_focus_to_next_widget (&fx->wl, (t[kbuf[1]] & 8), modifier);
            }
          }
        }

        if (!handled) {
          if (xitk->menu) {
            if (!(fx->wl.widget_focused && (fx->wl.widget_focused->type & WIDGET_GROUP_MENU)))
              xitk_set_current_menu (&xitk->x, NULL);
          }
        }
      }
      break;

    case XITK_EV_EXPOSE:
      if (fx) {
        do {
          if (event->x < fx->expose.x1)
            fx->expose.x1 = event->x;
          if (event->x + event->w > fx->expose.x2)
            fx->expose.x2 = event->x + event->w;
          if (event->y < fx->expose.y1)
            fx->expose.y1 = event->y;
          if (event->y + event->h > fx->expose.y2)
            fx->expose.y2 = event->y + event->h;
        } while (xitk->x.d->next_event (xitk->x.d, event, event->window, XITK_EV_EXPOSE, 0));
        if (event->more == 0) {
#ifdef XITK_PAINT_DEBUG
          printf ("xitk.expose: x %d-%d, y %d-%d.\n", fx->expose.x1, fx->expose.x2, fx->expose.y1, fx->expose.y2);
#endif
          xitk_partial_paint_widget_list (&fx->wl, &fx->expose);
          event->x = fx->expose.x1;
          event->y = fx->expose.y1;
          event->w = fx->expose.x2 - fx->expose.x1;
          event->h = fx->expose.y2 - fx->expose.y1;
          _xitk_reset_hull (&fx->expose);
        }
        /* not handled = 1; here, window event cb may want to see this too. */
      }
      break;

    case XITK_EV_FOCUS:
      if (fx) {
        xitk->focused = fx->key;
        if (xitk->x.verbosity >= 2)
          printf ("xitk.window.focus.received (%s).\n", fx->name);
      }
      goto _get_flags;
    case XITK_EV_UNFOCUS:
      if (fx && (fx->key == xitk->focused)) {
        xitk->focused = 0;
        if (xitk->x.verbosity >= 2)
          printf ("xitk.window.focus.lost (%s).\n", fx->name);
      }
    _get_flags:
      if (xwin && xwin->bewin) {
        xitk_tagitem_t tags[] = {{XITK_TAG_WIN_FLAGS, 0}, {XITK_TAG_END, 0}};
        xwin->bewin->get_props (xwin->bewin, tags);
        xwin->flags = tags[0].value;
      }
      break;

    case XITK_EV_MOVE:
      if (!fx)
        break;

      while (xitk->x.d->next_event (xitk->x.d, event, NULL, XITK_EV_MOVE, 0)) ;
      event->qual |= xitk->qual;

      if (fx->move.enabled) {
        xitk_tagitem_t tags[] = {
          {XITK_TAG_X, 0}, {XITK_TAG_Y, 0}, {XITK_TAG_END, 0}
        };

        if (fx->wl.widget_under_mouse && (fx->wl.widget_under_mouse->type & WIDGET_GROUP_MENU)) {
          xitk_widget_t *menu = xitk_menu_get_menu(fx->wl.widget_focused);
          if (xitk_menu_show_sub_branchs (menu))
            xitk_menu_destroy_sub_branchs (menu);
        }

        fx->new_pos.x = fx->old_pos.x + event->w - fx->move.offset_x;
        fx->new_pos.y = fx->old_pos.y + event->h - fx->move.offset_y;

        tags[0].value = fx->new_pos.x;
        tags[1].value = fx->new_pos.y;
        if (xwin && xwin->bewin)
          xwin->bewin->set_props (xwin->bewin, tags);
      } else {
        xitk_motion_notify_widget_list (&fx->wl, event->x, event->y, event->qual);
      }
      break;

    case XITK_EV_LEAVE:
      if (!(fx && fx->wl.widget_pressed &&
        (fx->wl.widget_pressed->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
        event->x = event->y = -1; /* Simulate moving out of any widget */
        /* but leave the actual coords for an active slider, otherwise the slider may jump */
      /* fall through */
    case XITK_EV_ENTER:
      event->qual |= xitk->qual;
      if (fx)
        if (event->code == 0) /* Ptr. moved rel. to win., not (un)grab */
          xitk_motion_notify_widget_list (&fx->wl, event->x, event->y, event->qual);
      break;

    case XITK_EV_BUTTON_DOWN:
      event->qual |= xitk->qual;
      if ((event->code >= 1) && (event->code <= 5))
        xitk->qual |= MODIFIER_BUTTON1 << (event->code - 1);
      if (!xwin) {
        /* A click anywhere outside an open menu shall close it with no further action. */
        if (xitk->menu)
          xitk_set_current_menu (&xitk->x, NULL);
      } else {
        xitk_tagitem_t tags[] = {{XITK_TAG_WIN_FLAGS, 0}, {XITK_TAG_END, 0}};

        xitk_tips_hide_tips(xitk->x.tips);

        xwin->bewin->get_props (xwin->bewin, tags);
        /* Give focus (and raise) to window after click * if it's viewable (e.g. not iconified). */
        if ((tags[0].value & (XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED)) == XITK_WINF_VISIBLE) {
          xwin->bewin->raise (xwin->bewin);
          tags[0].value = (XITK_WINF_FOCUS << 16) | XITK_WINF_FOCUS;
          xwin->bewin->set_props (xwin->bewin, tags);
          xwin->bewin->get_props (xwin->bewin, tags);
        }
        xwin->flags = tags[0].value;

        /* A click anywhere outside an open menu shall close it with no further action.
         * FIXME: this currently works only if the click goes into one of our windows. */
        if (xitk->menu) {
          if (!(fx->wl.widget_under_mouse && (fx->wl.widget_under_mouse->type & WIDGET_GROUP_MENU))) {
            xitk_set_current_menu (&xitk->x, NULL);
            break;
          }
        }

        {
          fx->move.enabled = !xitk_click_notify_widget_list (&fx->wl,
            event->x, event->y, event->code, 0, event->qual);
          if (event->code != 1)
            fx->move.enabled = 0;
          if (xwin && (xwin->flags & XITK_WINF_FIXED_POS))
            fx->move.enabled = 0;

          if ((event->code == 4) || (event->code == 5)) {
            if (fx->wl.widget_focused) {
              xitk_widget_t *w = fx->wl.widget_focused;
              const char sup[3] = {XITK_CTRL_KEY_PREFIX, XITK_MOUSE_WHEEL_UP, 0};
              const char sdown[3] = {XITK_CTRL_KEY_PREFIX, XITK_MOUSE_WHEEL_DOWN, 0};
              handled = xitk_widget_key_event (w, event->code == 4 ? sup : sdown, event->qual, 0);
            }
          }

          if (fx->move.enabled) {
            /* BTW. for some reason, XGetWindowAttributes () _always_ returns
             * wattr.x == wattr.y == wattr.border_width == 0. */
            fx->old_pos.x = event->w - event->x;
            fx->old_pos.y = event->h - event->y;
            fx->move.offset_x = event->w;
            fx->move.offset_y = event->h;
          }
        }
      }
      break;

    case XITK_EV_BUTTON_UP:
      event->qual |= xitk->qual;
      if ((event->code >= 1) && (event->code <= 5))
        xitk->qual &= ~(MODIFIER_BUTTON1 << (event->code - 1));
      if (!fx)
        break;
      xitk_tips_hide_tips (xitk->x.tips);
      if (fx->move.enabled) {
        fx->move.enabled = 0;
      } else {
        xitk_click_notify_widget_list (&fx->wl, event->x, event->y, event->code, 1, event->qual);
      }
      break;

    case XITK_EV_POS_SIZE:
      if (fx) {
        fx->new_pos.x = event->x;
        fx->new_pos.y = event->y;
        fx->width = event->w;
        fx->height = event->h;

        {
          xitk_widget_t *w = (xitk_widget_t *)fx->wl.list.head.next;
          while (w->node.next) {
            if ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO)
              xitk_combo_update_pos (w);
            w = (xitk_widget_t *)w->node.next;
          }
        }

      }
      break;

    default: ;
  }

  if (!handled && fx && fx->event_handler)
    fx->event_handler (fx->user_data, event);

  if (fx)
    __fx_unref (fx);
}

long int xitk_get_last_keypressed_time (xitk_t *xitk);
long int xitk_reset_screen_saver(xitk_t *xitk, long int timeout)
{
  long int idle;

  // XXX is this useful at all ... ???
  idle = xitk_get_last_keypressed_time (xitk);
  if (idle < timeout)
    return idle;

  if (xitk->d && xitk->d->reset_screen_saver)
    return xitk->d->reset_screen_saver(xitk->d, timeout);

  return 0;
}

int xitk_change_video_mode(xitk_t *xitk, xitk_window_t *w, int min_width, int min_height)
{
  if (w) {
    if (w->bewin->display->change_vmode)
      return w->bewin->display->change_vmode(w->bewin->display, w->bewin, min_width, min_height);
  } else if (xitk->d && xitk->d->change_vmode) {
    return xitk->d->change_vmode(xitk->d, NULL, min_width, min_height);
  }

  return -1;
}

int xitk_image_quality (xitk_t *xitk, int qual) {
  if (xitk->d->image_quality)
    return xitk->d->image_quality(xitk->d, qual);
  return qual;
}

/*
 * Initiatization of widget internals.
 */

xitk_t *xitk_init (const char *prefered_visual, int install_colormap,
                   int use_x_lock_display, int use_synchronized_x, int verbosity) {

  __xitk_t *xitk;
  pthread_mutexattr_t attr;

#ifdef ENABLE_NLS
  bindtextdomain("xitk", XITK_LOCALE);
#endif

  if (verbosity > 1) {
    dump_host_info ();
    dump_cpu_infos ();
  }

  xitk = xitk_xmalloc (sizeof (*xitk));
  gXitk = &xitk->x;
  if (!xitk)
    return NULL;

  xitk->x.be = xitk_backend_new (&xitk->x, verbosity);
  if (!xitk->x.be) {
    free (xitk);
    return NULL;
  }
  xitk->x.d = xitk->x.be->open_display (xitk->x.be, NULL, use_x_lock_display, use_synchronized_x,
                                        prefered_visual, install_colormap);
  if (!xitk->x.d) {
    xitk->x.be->_delete (&xitk->x.be);
    free (xitk);
    return NULL;
  }
  xitk->xitk_pid = getppid ();

  xitk->event_bridge.running = 0;

  xitk_color_db_init (xitk);

  xitk->x.verbosity     = verbosity;
  xitk_dlist_init (&xitk->gfxs);
  xitk->key             = 0;
  xitk->focused         = 0;
  xitk->sig_callback    = NULL;
  xitk->sig_data        = NULL;
  xitk->config          = xitk_config_init (&xitk->x);
  xitk->qual            = 0;
  xitk->x.tips_timeout  = TIPS_TIMEOUT;

  memset(&xitk->keypress, 0, sizeof(xitk->keypress));

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&xitk->mutex, &attr);

  if (verbosity >= 1) {
    printf (
      "-[ xiTK version %d.%d.%d"
#ifdef WITH_XFT
      " [XFT]"
#elif defined(WITH_XMB)
      " [XMB]"
#endif
      " ]-\n", XITK_MAJOR_VERSION, XITK_MINOR_VERSION, XITK_SUB_VERSION);
  }

  /* init font caching */
  xitk->x.font_cache = xitk_font_cache_init();

  xitk->x.tips = xitk_tips_new ();

  return &xitk->x;
}

/*
 * Start widget event handling.
 * It will block till xitk_stop () call
 */
void xitk_run (xitk_t *_xitk, void (* start_cb)(void *data), void *start_data,
  void (* stop_cb)(void *data), void *stop_data) {
  __xitk_t *xitk;

  if (!_xitk)
    return;
  xitk_container (xitk, _xitk, x);
  {
    struct sigaction  action;

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGHUP, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGHUP) failed: %s\n", strerror (errno));
    }

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGUSR1, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGUSR1) failed: %s\n", strerror (errno));
    }

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGUSR2, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGUSR2) failed: %s\n", strerror (errno));
    }

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGINT, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGINT) failed: %s\n", strerror (errno));
    }

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGTERM, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGTERM) failed: %s\n", strerror (errno));
    }

    action.sa_handler = xitk_signal_handler;
    sigemptyset (&(action.sa_mask));
    action.sa_flags = 0;
    if (sigaction (SIGQUIT, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGQUIT) failed: %s\n", strerror (errno));
    }
#ifndef DEBUG
    action.sa_handler = xitk_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGSEGV, &action, NULL) != 0) {
      XITK_WARNING ("sigaction(SIGSEGV) failed: %s\n", strerror (errno));
    }
#endif
  }

  _xitk_clipboard_init (xitk);

  xitk->running = 1;

  /* We're ready to handle anything */
  if (start_cb) {
    xitk_event_bridge_start (xitk);
    start_cb (start_data);
    xitk_event_bridge_stop (xitk);
  }

  /* Now, wait for a new xevent */
  while (xitk->running) {
    xitk_be_event_t event;

    if (xitk->x.d->next_event (xitk->x.d, &event, NULL, XITK_EV_ANY, 33)) {
      MUTLOCK ();
      xitk_handle_event (xitk, &event);
      MUTUNLOCK ();
    }
  }

  if (stop_cb)
    stop_cb (stop_data);

  xitk_set_current_menu(_xitk, NULL);

  xitk_tips_delete (&xitk->x.tips);
  _xitk_clipboard_deinit (xitk);

  /* pending destroys of the event handlers */
  MUTLOCK ();
  while (1) {
    __gfx_t *fx = (__gfx_t *)xitk->gfxs.head.next;
    if (!fx->wl.node.next)
      break;
    __fx_unref (fx);
  }
  xitk_dlist_clear (&xitk->gfxs);
  MUTUNLOCK ();

  /* destroy font caching */
  xitk_font_cache_destroy(&xitk->x.font_cache);

  xitk_config_deinit (xitk->config);
}

void xitk_free(xitk_t **p) {

  __xitk_t *xitk;

  xitk_container (xitk, *p, x);
  if (!xitk)
    return;
  *p = NULL;

  xitk_color_db_flush (&xitk->x);
  xitk_color_db_deinit (xitk);

  /*
   * Close X display before unloading modules linked against libGL.so
   * https://www.xfree86.org/4.3.0/DRI11.html
   *
   * Do not close the library with dlclose() until after XCloseDisplay() has
   * been called. When libGL.so initializes itself it registers several
   * callbacks functions with Xlib. When XCloseDisplay() is called those
   * callback functions are called. If libGL.so has already been unloaded
   * with dlclose() this will cause a segmentation fault.
   */

  xitk->x.d->close (&xitk->x.d);
  xitk->x.be->_delete (&xitk->x.be);

  pthread_mutex_destroy (&xitk->mutex);

  XITK_FREE (xitk);
  gXitk = NULL;
}

/*
 * Stop the wait xevent loop
 */
void xitk_stop (xitk_t *_xitk) {
  __xitk_t *xitk;

  if (!_xitk)
    return;
  xitk_container (xitk, _xitk, x);
  /* defer this to xitk_run (), where it cannot freeze the tips thread waiting for xitk->mutex
   * in _tips_loop_thread () -> xitk_window_set_role () -> xitk_window_update_tree ().
  xitk_tips_delete (&xitk->x.tips);
   */
  xitk->running = 0;
}

void xitk_set_xmb_enability(xitk_t *_xitk, int value) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  xitk_config_set_xmb_enability (xitk->config, value);
}

int xitk_is_dbl_click (xitk_t *xitk, const struct timeval *t1, const struct timeval *t2) {
  __xitk_t *_xitk;
  struct timeval td;

  xitk_container (_xitk, xitk, x);
  td.tv_usec = xitk_config_get_num (_xitk->config, XITK_DBL_CLICK_TIME);
  td.tv_sec  = t2->tv_sec - t1->tv_sec;
  if (td.tv_sec > td.tv_usec / 1000 + 1)
    return 0;
  return td.tv_sec * 1000 + (t2->tv_usec - t1->tv_usec) / 1000 < td.tv_usec;
}

const char *xitk_get_cfg_string (xitk_t *_xitk, xitk_cfg_item_t item) {
  __xitk_t *xitk;

  if (!_xitk)
    return NULL;
  xitk_container (xitk, _xitk, x);
  return xitk_config_get_string (xitk->config, item);
}

int xitk_get_cfg_num (xitk_t *_xitk, xitk_cfg_item_t item) {
  __xitk_t *xitk;

  if (!_xitk)
    return 0;
  xitk_container (xitk, _xitk, x);
  switch (item) {
    case XITK_TIPS_TIMEOUT: return xitk->x.tips_timeout;
    default: return xitk_config_get_num (xitk->config, item);
  }
}

void xitk_get_display_size (xitk_t *xitk, int *w, int *h) {
  if (w)
    *w = xitk->d->width;
  if (h)
    *h = xitk->d->height;
}

double xitk_get_display_ratio (xitk_t *xitk) {
  return xitk->d->ratio;
}

void xitk_set_tips_timeout (xitk_t *_xitk, unsigned int timeout) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  xitk->x.tips_timeout = timeout == XITK_TIPS_TIMEOUT_AUTO ? TIPS_TIMEOUT : timeout;
}

static const uint8_t tab_unhex[256] = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,255,255,255,255,255,255,
  255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

char *xitk_filter_filename(const char *name) {
  if (!name)
    return NULL;
  if (!strncasecmp (name, "file:", 5)) {
    const uint8_t *p = (const uint8_t *)name + 5;
    uint8_t *ret, *q;
    size_t l = strlen ((const char *)p);
    ret = malloc (l + 2);
    if (!ret)
      return NULL;
    while (*p == '/') p++;
    q = ret;
    *q++ = '/';
    while (*p) {
      uint8_t z = *p++;
      if (z == '%') {
        do {
          uint8_t y;
          y = tab_unhex[*p];
          if (y & 128) break;
          p++;
          z = y;
          y = tab_unhex[*p];
          if (y & 128) break;
          p++;
          z = (z << 4) | y;
        } while (0);
      }
      *q++ = z;
    }
    *q = 0;
    return (char *)ret;
  }
  return strdup (name);
}

/*
 *
 */
const char *xitk_set_locale(void) {
  const char *cur_locale = NULL;

#ifdef ENABLE_NLS
  if(setlocale (LC_ALL,"") == NULL) {
    XITK_WARNING("locale not supported by C library\n");
    return NULL;
  }

  cur_locale = setlocale(LC_ALL, NULL);
#endif

  return cur_locale;
}


/*
 *
 */
long int xitk_get_last_keypressed_time (xitk_t *xitk) {
  __xitk_t *_xitk;
  struct timeval tm;

  xitk_container (_xitk, xitk, x);
  gettimeofday (&tm, NULL);
  tm.tv_sec -= _xitk->keypress.tv_sec;
  tm.tv_usec -= _xitk->keypress.tv_usec;
  return tm.tv_usec < 0 ? tm.tv_sec - 1 : tm.tv_sec;
}

char *xitk_cfg_load (const char *filename, size_t *filesize) {
  char *buf;
  size_t fsize;
  FILE *f;

  if (!filename) {
    errno = EINVAL;
    return NULL;
  }
  if (!filename[0]) {
    errno = EINVAL;
    return NULL;
  }

  errno = 0;
  f = fopen (filename, "rb");
  if (!f)
    return NULL;

  if (fseek (f, 0, SEEK_END)) {
    int e = errno;
    fclose (f);
    errno = e;
    return NULL;
  }
  fsize = ftell (f);
  if (fseek (f, 0, SEEK_SET)) {
    int e = errno;
    fclose (f);
    errno = e;
    return NULL;
  }

  if (filesize) {
    if (fsize > *filesize)
      fsize = *filesize;
  }
  buf = malloc (fsize + 16);
  if (!buf) {
    fclose (f);
    errno = ENOMEM;
    return NULL;
  }

  memset (buf, 0, 8);
  fsize = fread (buf + 8, 1, fsize, f);
  memset (buf + 8 + fsize, 0, 8);
  fclose (f);
  if (filesize)
    *filesize = fsize;
  return buf + 8;
}

void xitk_cfg_unload (char *buf) {
  if (!buf)
    return;
  free (buf - 8);
}

/*
 * Return 0/1 from char value (valids are 1/0, true/false,
 * yes/no, on/off. Case isn't checked.
 */
int xitk_get_bool_value(const char *val) {
  union {
    char b[4];
    uint32_t v;
  } _true = {{'t','r','u','e'}},
    _yes  = {{'y','e','s',' '}},
    _on   = {{'o','n',' ',' '}},
    _have;

  if (!val)
    return 0;
  if ((*val >= '1') && (*val <= '9'))
    return 1;
  _have.v = 0x20202020;
  if (val[0]) {
    _have.b[0] = val[0];
    if (val[1]) {
      _have.b[1] = val[1];
      if (val[2]) {
        _have.b[2] = val[2];
        if (val[3])
          _have.b[3] = val[3];
      }
    }
  }
  _have.v |= 0x20202020;
  if (_have.v == _true.v)
    return 1;
  if (_have.v == _yes.v)
    return 1;
  if (_have.v == _on.v)
    return 1;
  return 0;
}

static void _xitk_lower (uint8_t *s, size_t l) {
  static const union {
    uint8_t b[4];
    uint32_t w;
  } bmask[8] = {
    {{  0,  0,  0,  0}},
    {{128,  0,  0,  0}},
    {{128,128,  0,  0}},
    {{128,128,128,  0}},
    {{128,128,128,128}},
    {{  0,128,128,128}},
    {{  0,  0,128,128}},
    {{  0,  0,  0,128}}
  };
  uint32_t *p = (uint32_t *)((uintptr_t)s & ~(uintptr_t)3);
  size_t n = s - (uint8_t *)p;

  l += n;
  {
    uint32_t v, mask;
    mask = bmask[4 + n].w;
    mask &= ~*p;
    v = *p | 0x80808080;
    mask &= v - 0x41414141; /* 4 * 'A' */
    mask &= ~(v - 0x5b5b5b5b); /* 4 * ('Z' + 1) */
    if (l <= 4) {
      mask &= bmask[l].w;
      *p |= mask >> 2;
      return;
    }
    *p++ |= mask >> 2;
    l -= 4;
  }
  while (l >= 4) {
    uint32_t v, mask;
    mask = ~*p & 0x80808080;
    v = *p | 0x80808080;
    mask &= v - 0x41414141; /* 4 * 'A' */
    mask &= ~(v - 0x5b5b5b5b); /* 4 * ('Z' + 1) */
    *p++ |= mask >> 2;
    l -= 4;
  }
  if (l) {
    uint32_t v, mask;
    mask = bmask[l].w;
    mask &= ~*p;
    v = *p | 0x80808080;
    mask &= v - 0x41414141; /* 4 * 'A' */
    mask &= ~(v - 0x5b5b5b5b); /* 4 * ('Z' + 1) */
    *p++ |= mask >> 2;
  }
}

size_t ATTR_INLINE_ALL_STRINGOPS xitk_lower_strlcpy (char *dest, const char *src, size_t dlen) {
  uint8_t *q = (uint8_t *)dest;
  const uint8_t *p = (const uint8_t *)src;
  size_t l;

  if (!src)
    return 0;
  l = strlen ((const char *)p);
  if (!dest || !dlen)
    return l;
  if (l + 1 < dlen)
    dlen = l + 1;
  memcpy (q, p, dlen);
  _xitk_lower (q, dlen);
  q[dlen] = 0;
  return l;
}
    
/* 0x01 (end), 0x02 (hash), 0x04 (start), 0x08 (stop), 0x10 (space), 0x20 (value_sep), 0x40 (lf), 0x80 (;) */
static const uint8_t tab_cfg_parse[256] = {
  [0] = 0x01, ['#'] = 0x02, ['{'] = 0x04, ['}'] = 0x08,
  [' '] = 0x10, ['\t'] = 0x10, ['\n'] = 0x40, ['\r'] = 0x40,
  [':'] = 0x20, ['='] = 0x20, [';'] = 0x80
};

xitk_cfg_parse_t *xitk_cfg_parse (char *contents, int flags) {
  xitk_cfg_parse_t *tree, *box, *item;
  int size, used;
  enum {
    XCP_FIND_KEY = 0,
    XCP_READ_KEY,
    XCP_FIND_VALUE,
    XCP_READ_VALUE,
    XCP_LINE,
    XCP_DOWN,
    XCP_UP,
    XCP_NEW_NODE,
    XCP_AUTO,
    XCP_DONE
  } state, oldstate;
  uint8_t *start = (uint8_t *)contents, *p = start, z, dummy, *e = &dummy;

  if (!start)
    return NULL;
  size = 32;
  tree = malloc (size * sizeof (*tree));
  if (!tree)
    return NULL;

  box = tree;
  box->level = 0;
  box->next = box->prev = box->parent = box->first_child = box->last_child = 0;
  box->key = box->value = -1;
  used = 1;

  item = NULL;
  z = 0;

  oldstate = XCP_FIND_KEY;
  state = XCP_LINE;
  while (state != XCP_DONE) {
    switch (state) {

      case XCP_AUTO:
        state = (z & 0x01) ? XCP_DONE
              : (z & 0x04) ? XCP_DOWN
              : (z & 0x08) ? XCP_UP
              : (z & 0x42) ? XCP_LINE
              : (z & 0x80) ? XCP_FIND_KEY
              : oldstate;
        break;

      case XCP_LINE:
        while (1) {
          while ((z = tab_cfg_parse[*p]) & 0x50)
            p++;
          if (z & 0x02) {
            while (!((z = tab_cfg_parse[*p]) & 0x41))
              p++;
          }
          if (!(z & 0x40))
            break;
          p++;
        }
        state = oldstate;
        break;

      case XCP_FIND_KEY:
        while ((z = tab_cfg_parse[*p]) & 0xd0)
          p++;
        oldstate = (z & 0x02) ? XCP_FIND_KEY : XCP_READ_KEY;
        state = XCP_AUTO;
        break;

      case XCP_READ_KEY:
        if (!item) {
          oldstate = state;
          state = XCP_NEW_NODE;
        } else {
          uint8_t *key = p;
          item->key = p - start;
          item = NULL;
          while (!((z = tab_cfg_parse[*p]) & 0xfd))
            p++;
          if (!(flags & XITK_CFG_PARSE_CASE))
            _xitk_lower (key, p - key);
          *e = 0;
          e = p;
          oldstate = XCP_FIND_VALUE;
          state = XCP_AUTO;
        }
        break;

      case XCP_FIND_VALUE:
        while ((z = tab_cfg_parse[*p]) & 0x30)
          p++;
        /* value may contain '#'. */
        z &= ~0x02;
        oldstate = XCP_READ_VALUE;
        state = XCP_AUTO;
        break;

      case XCP_READ_VALUE:
        item = tree + box->last_child;
        item->value = p - start;
        item = NULL;
        /* value may contain '# \t'. */
        while (!((z = tab_cfg_parse[*p]) & 0xcd))
          p++;
        /* defer string end write to when we dont read it anymore. */
        *e = 0;
        e = p;
        /* remove trailing space. */
        while (tab_cfg_parse[e[-1]] & 0x10)
          e--;
        oldstate = XCP_FIND_KEY;
        state = XCP_AUTO;
        break;

      case XCP_UP:
        z = tab_cfg_parse[*++p];
        box = tree + box->parent;
        oldstate = XCP_FIND_KEY;
        state = XCP_AUTO;
        break;

      case XCP_DOWN:
        if (!item) {
          z = tab_cfg_parse[*++p];
          if ((oldstate == XCP_FIND_VALUE) || (oldstate == XCP_READ_VALUE)) {
            /* want a value? guess this bramch _is_ the value :-) */
            box = tree + box->last_child;
            oldstate = XCP_FIND_KEY;
            state = XCP_AUTO;
          } else {
            oldstate = state;
            state = XCP_NEW_NODE;
          }
        } else {
          box = item;
          item = NULL;
          oldstate = XCP_FIND_KEY;
          state = XCP_AUTO;
        }
        break;

      case XCP_NEW_NODE:
        if (used + 1 > size) {
          size_t add = size < 1024 ? size : 1024;
          xitk_cfg_parse_t *ntree = realloc (tree, (size + add) * sizeof (*tree));

          if (!ntree) {
            state = XCP_DONE;
            break;
          }
          box = ntree + (box - tree);
          tree = ntree;
          size += add;
        }
        if (box->last_child) {
          item = tree + box->last_child;
          item->next = used;
        }
        item = tree + used;
        item->level = box->level + 1;
        item->parent = box - tree;
        item->next = 0;
        item->prev = box->last_child;
        box->last_child = used;
        if (!box->first_child)
          box->first_child = used;
        item->first_child = item->last_child = 0;
        item->key = item->value = -1;
        used++;
        state = oldstate;
        break;

      default: ;
    }
  }
  *e = 0;

  if (flags & XITK_CFG_PARSE_DEBUG) {
    int i;

    for (i = 0; i < used; i++) {
      item = tree + i;
      printf ("xitk_cfg_parse: level (%d), parent (%d), key (%s), value (%s).\n",
        item->level, item->parent,
        item->key >= 0 ? contents + item->key : "", item->value >= 0 ? contents + item->value : "");
    }
  }

  return tree;
}

void xitk_cfg_unparse (xitk_cfg_parse_t *tree) {
  free (tree);
}

typedef struct {
  char    colorname[21]; /* longest is "lightgoldenrodyellow", and sizeof is now 8 * 3 ;-) */
  uint8_t red, green, blue;
} xitk_color_names_t;

static const xitk_color_names_t xitk_sorted_color_names[] = {
  { "aliceblue",              240, 248, 255 },
  { "antiquewhite",           250, 235, 215 },
  { "antiquewhite1",          255, 239, 219 },
  { "antiquewhite2",          238, 223, 204 },
  { "antiquewhite3",          205, 192, 176 },
  { "antiquewhite4",          139, 131, 120 },
  { "aquamarine",             127, 255, 212 },
  { "aquamarine1",            127, 255, 212 },
  { "aquamarine2",            118, 238, 198 },
  { "aquamarine3",            102, 205, 170 },
  { "aquamarine4",             69, 139, 116 },
  { "azure",                  240, 255, 255 },
  { "azure1",                 240, 255, 255 },
  { "azure2",                 224, 238, 238 },
  { "azure3",                 193, 205, 205 },
  { "azure4",                 131, 139, 139 },
  { "beige",                  245, 245, 220 },
  { "bisque",                 255, 228, 196 },
  { "bisque1",                255, 228, 196 },
  { "bisque2",                238, 213, 183 },
  { "bisque3",                205, 183, 158 },
  { "bisque4",                139, 125, 107 },
  { "black",                    0,   0,   0 },
  { "blanchedalmond",         255, 235, 205 },
  { "blue",                     0,   0, 255 },
  { "blue1",                    0,   0, 255 },
  { "blue2",                    0,   0, 238 },
  { "blue3",                    0,   0, 205 },
  { "blue4",                    0,   0, 139 },
  { "blueviolet",             138,  43, 226 },
  { "brown",                  165,  42,  42 },
  { "brown1",                 255,  64,  64 },
  { "brown2",                 238,  59,  59 },
  { "brown3",                 205,  51,  51 },
  { "brown4",                 139,  35,  35 },
  { "burlywood",              222, 184, 135 },
  { "burlywood1",             255, 211, 155 },
  { "burlywood2",             238, 197, 145 },
  { "burlywood3",             205, 170, 125 },
  { "burlywood4",             139, 115,  85 },
  { "cadetblue",               95, 158, 160 },
  { "cadetblue1",             152, 245, 255 },
  { "cadetblue2",             142, 229, 238 },
  { "cadetblue3",             122, 197, 205 },
  { "cadetblue4",              83, 134, 139 },
  { "chartreuse",             127, 255,   0 },
  { "chartreuse1",            127, 255,   0 },
  { "chartreuse2",            118, 238,   0 },
  { "chartreuse3",            102, 205,   0 },
  { "chartreuse4",             69, 139,   0 },
  { "chocolate",              210, 105,  30 },
  { "chocolate1",             255, 127,  36 },
  { "chocolate2",             238, 118,  33 },
  { "chocolate3",             205, 102,  29 },
  { "chocolate4",             139,  69,  19 },
  { "coral",                  255, 127,  80 },
  { "coral1",                 255, 114,  86 },
  { "coral2",                 238, 106,  80 },
  { "coral3",                 205,  91,  69 },
  { "coral4",                 139,  62,  47 },
  { "cornflowerblue",         100, 149, 237 },
  { "cornsilk",               255, 248, 220 },
  { "cornsilk1",              255, 248, 220 },
  { "cornsilk2",              238, 232, 205 },
  { "cornsilk3",              205, 200, 177 },
  { "cornsilk4",              139, 136, 120 },
  { "cyan",                     0, 255, 255 },
  { "cyan1",                    0, 255, 255 },
  { "cyan2",                    0, 238, 238 },
  { "cyan3",                    0, 205, 205 },
  { "cyan4",                    0, 139, 139 },
  { "darkblue",                 0,   0, 139 },
  { "darkcyan",                 0, 139, 139 },
  { "darkgoldenrod",          184, 134,  11 },
  { "darkgoldenrod1",         255, 185,  15 },
  { "darkgoldenrod2",         238, 173,  14 },
  { "darkgoldenrod3",         205, 149,  12 },
  { "darkgoldenrod4",         139, 101,   8 },
  { "darkgray",               169, 169, 169 },
  { "darkgreen",                0, 100,   0 },
  { "darkgrey",               169, 169, 169 },
  { "darkkhaki",              189, 183, 107 },
  { "darkmagenta",            139,   0, 139 },
  { "darkolivegreen",          85, 107,  47 },
  { "darkolivegreen1",        202, 255, 112 },
  { "darkolivegreen2",        188, 238, 104 },
  { "darkolivegreen3",        162, 205,  90 },
  { "darkolivegreen4",        110, 139,  61 },
  { "darkorange",             255, 140,   0 },
  { "darkorange1",            255, 127,   0 },
  { "darkorange2",            238, 118,   0 },
  { "darkorange3",            205, 102,   0 },
  { "darkorange4",            139,  69,   0 },
  { "darkorchid",             153,  50, 204 },
  { "darkorchid1",            191,  62, 255 },
  { "darkorchid2",            178,  58, 238 },
  { "darkorchid3",            154,  50, 205 },
  { "darkorchid4",            104,  34, 139 },
  { "darkred",                139,   0,   0 },
  { "darksalmon",             233, 150, 122 },
  { "darkseagreen",           143, 188, 143 },
  { "darkseagreen1",          193, 255, 193 },
  { "darkseagreen2",          180, 238, 180 },
  { "darkseagreen3",          155, 205, 155 },
  { "darkseagreen4",          105, 139, 105 },
  { "darkslateblue",           72,  61, 139 },
  { "darkslategray1",         151, 255, 255 },
  { "darkslategray2",         141, 238, 238 },
  { "darkslategray3",         121, 205, 205 },
  { "darkslategray4",          82, 139, 139 },
  { "darkslategrey",           47,  79,  79 },
  { "darkturquoise",            0, 206, 209 },
  { "darkviolet",             148,   0, 211 },
  { "deeppink",               255,  20, 147 },
  { "deeppink1",              255,  20, 147 },
  { "deeppink2",              238,  18, 137 },
  { "deeppink3",              205,  16, 118 },
  { "deeppink4",              139,  10,  80 },
  { "deepskyblue",              0, 191, 255 },
  { "deepskyblue1",             0, 191, 255 },
  { "deepskyblue2",             0, 178, 238 },
  { "deepskyblue3",             0, 154, 205 },
  { "deepskyblue4",             0, 104, 139 },
  { "dimgray",                105, 105, 105 },
  { "dimgrey",                105, 105, 105 },
  { "dodgerblue",              30, 144, 255 },
  { "dodgerblue1",             30, 144, 255 },
  { "dodgerblue2",             28, 134, 238 },
  { "dodgerblue3",             24, 116, 205 },
  { "dodgerblue4",             16,  78, 139 },
  { "firebrick",              178,  34,  34 },
  { "firebrick1",             255,  48,  48 },
  { "firebrick2",             238,  44,  44 },
  { "firebrick3",             205,  38,  38 },
  { "firebrick4",             139,  26,  26 },
  { "floralwhite",            255, 250, 240 },
  { "forestgreen",             34, 139,  34 },
  { "gainsboro",              220, 220, 220 },
  { "ghostwhite",             248, 248, 255 },
  { "gold",                   255, 215,   0 },
  { "gold1",                  255, 215,   0 },
  { "gold2",                  238, 201,   0 },
  { "gold3",                  205, 173,   0 },
  { "gold4",                  139, 117,   0 },
  { "goldenrod",              218, 165,  32 },
  { "goldenrod1",             255, 193,  37 },
  { "goldenrod2",             238, 180,  34 },
  { "goldenrod3",             205, 155,  29 },
  { "goldenrod4",             139, 105,  20 },
  { "gray",                   190, 190, 190 },
  { "gray0",                    0,   0,   0 },
  { "gray1",                    3,   3,   3 },
  { "gray10",                  26,  26,  26 },
  { "gray100",                255, 255, 255 },
  { "gray11",                  28,  28,  28 },
  { "gray12",                  31,  31,  31 },
  { "gray13",                  33,  33,  33 },
  { "gray14",                  36,  36,  36 },
  { "gray15",                  38,  38,  38 },
  { "gray16",                  41,  41,  41 },
  { "gray17",                  43,  43,  43 },
  { "gray18",                  46,  46,  46 },
  { "gray19",                  48,  48,  48 },
  { "gray2",                    5,   5,   5 },
  { "gray20",                  51,  51,  51 },
  { "gray21",                  54,  54,  54 },
  { "gray22",                  56,  56,  56 },
  { "gray23",                  59,  59,  59 },
  { "gray24",                  61,  61,  61 },
  { "gray25",                  64,  64,  64 },
  { "gray26",                  66,  66,  66 },
  { "gray27",                  69,  69,  69 },
  { "gray28",                  71,  71,  71 },
  { "gray29",                  74,  74,  74 },
  { "gray3",                    8,   8,   8 },
  { "gray30",                  77,  77,  77 },
  { "gray31",                  79,  79,  79 },
  { "gray32",                  82,  82,  82 },
  { "gray33",                  84,  84,  84 },
  { "gray34",                  87,  87,  87 },
  { "gray35",                  89,  89,  89 },
  { "gray36",                  92,  92,  92 },
  { "gray37",                  94,  94,  94 },
  { "gray38",                  97,  97,  97 },
  { "gray39",                  99,  99,  99 },
  { "gray4",                   10,  10,  10 },
  { "gray40",                 102, 102, 102 },
  { "gray41",                 105, 105, 105 },
  { "gray42",                 107, 107, 107 },
  { "gray43",                 110, 110, 110 },
  { "gray44",                 112, 112, 112 },
  { "gray45",                 115, 115, 115 },
  { "gray46",                 117, 117, 117 },
  { "gray47",                 120, 120, 120 },
  { "gray48",                 122, 122, 122 },
  { "gray49",                 125, 125, 125 },
  { "gray5",                   13,  13,  13 },
  { "gray50",                 127, 127, 127 },
  { "gray51",                 130, 130, 130 },
  { "gray52",                 133, 133, 133 },
  { "gray53",                 135, 135, 135 },
  { "gray54",                 138, 138, 138 },
  { "gray55",                 140, 140, 140 },
  { "gray56",                 143, 143, 143 },
  { "gray57",                 145, 145, 145 },
  { "gray58",                 148, 148, 148 },
  { "gray59",                 150, 150, 150 },
  { "gray6",                   15,  15,  15 },
  { "gray60",                 153, 153, 153 },
  { "gray61",                 156, 156, 156 },
  { "gray62",                 158, 158, 158 },
  { "gray63",                 161, 161, 161 },
  { "gray64",                 163, 163, 163 },
  { "gray65",                 166, 166, 166 },
  { "gray66",                 168, 168, 168 },
  { "gray67",                 171, 171, 171 },
  { "gray68",                 173, 173, 173 },
  { "gray69",                 176, 176, 176 },
  { "gray7",                   18,  18,  18 },
  { "gray70",                 179, 179, 179 },
  { "gray71",                 181, 181, 181 },
  { "gray72",                 184, 184, 184 },
  { "gray73",                 186, 186, 186 },
  { "gray74",                 189, 189, 189 },
  { "gray75",                 191, 191, 191 },
  { "gray76",                 194, 194, 194 },
  { "gray77",                 196, 196, 196 },
  { "gray78",                 199, 199, 199 },
  { "gray79",                 201, 201, 201 },
  { "gray8",                   20,  20,  20 },
  { "gray80",                 204, 204, 204 },
  { "gray81",                 207, 207, 207 },
  { "gray82",                 209, 209, 209 },
  { "gray83",                 212, 212, 212 },
  { "gray84",                 214, 214, 214 },
  { "gray85",                 217, 217, 217 },
  { "gray86",                 219, 219, 219 },
  { "gray87",                 222, 222, 222 },
  { "gray88",                 224, 224, 224 },
  { "gray89",                 227, 227, 227 },
  { "gray9",                   23,  23,  23 },
  { "gray90",                 229, 229, 229 },
  { "gray91",                 232, 232, 232 },
  { "gray92",                 235, 235, 235 },
  { "gray93",                 237, 237, 237 },
  { "gray94",                 240, 240, 240 },
  { "gray95",                 242, 242, 242 },
  { "gray96",                 245, 245, 245 },
  { "gray97",                 247, 247, 247 },
  { "gray98",                 250, 250, 250 },
  { "gray99",                 252, 252, 252 },
  { "green1",                   0, 255,   0 },
  { "green2",                   0, 238,   0 },
  { "green3",                   0, 205,   0 },
  { "green4",                   0, 139,   0 },
  { "greenyellow",            173, 255,  47 },
  { "grey",                   190, 190, 190 },
  { "grey0",                    0,   0,   0 },
  { "grey1",                    3,   3,   3 },
  { "grey10",                  26,  26,  26 },
  { "grey100",                255, 255, 255 },
  { "grey11",                  28,  28,  28 },
  { "grey12",                  31,  31,  31 },
  { "grey13",                  33,  33,  33 },
  { "grey14",                  36,  36,  36 },
  { "grey15",                  38,  38,  38 },
  { "grey16",                  41,  41,  41 },
  { "grey17",                  43,  43,  43 },
  { "grey18",                  46,  46,  46 },
  { "grey19",                  48,  48,  48 },
  { "grey2",                    5,   5,   5 },
  { "grey20",                  51,  51,  51 },
  { "grey21",                  54,  54,  54 },
  { "grey22",                  56,  56,  56 },
  { "grey23",                  59,  59,  59 },
  { "grey24",                  61,  61,  61 },
  { "grey25",                  64,  64,  64 },
  { "grey26",                  66,  66,  66 },
  { "grey27",                  69,  69,  69 },
  { "grey28",                  71,  71,  71 },
  { "grey29",                  74,  74,  74 },
  { "grey3",                    8,   8,   8 },
  { "grey30",                  77,  77,  77 },
  { "grey31",                  79,  79,  79 },
  { "grey32",                  82,  82,  82 },
  { "grey33",                  84,  84,  84 },
  { "grey34",                  87,  87,  87 },
  { "grey35",                  89,  89,  89 },
  { "grey36",                  92,  92,  92 },
  { "grey37",                  94,  94,  94 },
  { "grey38",                  97,  97,  97 },
  { "grey39",                  99,  99,  99 },
  { "grey4",                   10,  10,  10 },
  { "grey40",                 102, 102, 102 },
  { "grey41",                 105, 105, 105 },
  { "grey42",                 107, 107, 107 },
  { "grey43",                 110, 110, 110 },
  { "grey44",                 112, 112, 112 },
  { "grey45",                 115, 115, 115 },
  { "grey46",                 117, 117, 117 },
  { "grey47",                 120, 120, 120 },
  { "grey48",                 122, 122, 122 },
  { "grey49",                 125, 125, 125 },
  { "grey5",                   13,  13,  13 },
  { "grey50",                 127, 127, 127 },
  { "grey51",                 130, 130, 130 },
  { "grey52",                 133, 133, 133 },
  { "grey53",                 135, 135, 135 },
  { "grey54",                 138, 138, 138 },
  { "grey55",                 140, 140, 140 },
  { "grey56",                 143, 143, 143 },
  { "grey57",                 145, 145, 145 },
  { "grey58",                 148, 148, 148 },
  { "grey59",                 150, 150, 150 },
  { "grey6",                   15,  15,  15 },
  { "grey60",                 153, 153, 153 },
  { "grey61",                 156, 156, 156 },
  { "grey62",                 158, 158, 158 },
  { "grey63",                 161, 161, 161 },
  { "grey64",                 163, 163, 163 },
  { "grey65",                 166, 166, 166 },
  { "grey66",                 168, 168, 168 },
  { "grey67",                 171, 171, 171 },
  { "grey68",                 173, 173, 173 },
  { "grey69",                 176, 176, 176 },
  { "grey7",                   18,  18,  18 },
  { "grey70",                 179, 179, 179 },
  { "grey71",                 181, 181, 181 },
  { "grey72",                 184, 184, 184 },
  { "grey73",                 186, 186, 186 },
  { "grey74",                 189, 189, 189 },
  { "grey75",                 191, 191, 191 },
  { "grey76",                 194, 194, 194 },
  { "grey77",                 196, 196, 196 },
  { "grey78",                 199, 199, 199 },
  { "grey79",                 201, 201, 201 },
  { "grey8",                   20,  20,  20 },
  { "grey80",                 204, 204, 204 },
  { "grey81",                 207, 207, 207 },
  { "grey82",                 209, 209, 209 },
  { "grey83",                 212, 212, 212 },
  { "grey84",                 214, 214, 214 },
  { "grey85",                 217, 217, 217 },
  { "grey86",                 219, 219, 219 },
  { "grey87",                 222, 222, 222 },
  { "grey88",                 224, 224, 224 },
  { "grey89",                 227, 227, 227 },
  { "grey9",                   23,  23,  23 },
  { "grey90",                 229, 229, 229 },
  { "grey91",                 232, 232, 232 },
  { "grey92",                 235, 235, 235 },
  { "grey93",                 237, 237, 237 },
  { "grey94",                 240, 240, 240 },
  { "grey95",                 242, 242, 242 },
  { "grey96",                 245, 245, 245 },
  { "grey97",                 247, 247, 247 },
  { "grey98",                 250, 250, 250 },
  { "grey99",                 252, 252, 252 },
  { "honeydew",               240, 255, 240 },
  { "honeydew1",              240, 255, 240 },
  { "honeydew2",              224, 238, 224 },
  { "honeydew3",              193, 205, 193 },
  { "honeydew4",              131, 139, 131 },
  { "hotpink",                255, 105, 180 },
  { "hotpink1",               255, 110, 180 },
  { "hotpink2",               238, 106, 167 },
  { "hotpink3",               205,  96, 144 },
  { "hotpink4",               139,  58,  98 },
  { "indianred",              205,  92,  92 },
  { "indianred1",             255, 106, 106 },
  { "indianred2",             238,  99,  99 },
  { "indianred3",             205,  85,  85 },
  { "indianred4",             139,  58,  58 },
  { "ivory",                  255, 255, 240 },
  { "ivory1",                 255, 255, 240 },
  { "ivory2",                 238, 238, 224 },
  { "ivory3",                 205, 205, 193 },
  { "ivory4",                 139, 139, 131 },
  { "khaki",                  240, 230, 140 },
  { "khaki1",                 255, 246, 143 },
  { "khaki2",                 238, 230, 133 },
  { "khaki3",                 205, 198, 115 },
  { "khaki4",                 139, 134,  78 },
  { "lavender",               230, 230, 250 },
  { "lavenderblush",          255, 240, 245 },
  { "lavenderblush1",         255, 240, 245 },
  { "lavenderblush2",         238, 224, 229 },
  { "lavenderblush3",         205, 193, 197 },
  { "lavenderblush4",         139, 131, 134 },
  { "lawngreen",              124, 252,   0 },
  { "lemonchiffon",           255, 250, 205 },
  { "lemonchiffon1",          255, 250, 205 },
  { "lemonchiffon2",          238, 233, 191 },
  { "lemonchiffon3",          205, 201, 165 },
  { "lemonchiffon4",          139, 137, 112 },
  { "lightblue",              173, 216, 230 },
  { "lightblue1",             191, 239, 255 },
  { "lightblue2",             178, 223, 238 },
  { "lightblue3",             154, 192, 205 },
  { "lightblue4",             104, 131, 139 },
  { "lightcoral",             240, 128, 128 },
  { "lightcyan",              224, 255, 255 },
  { "lightcyan1",             224, 255, 255 },
  { "lightcyan2",             209, 238, 238 },
  { "lightcyan3",             180, 205, 205 },
  { "lightcyan4",             122, 139, 139 },
  { "lightgoldenrod",         238, 221, 130 },
  { "lightgoldenrod1",        255, 236, 139 },
  { "lightgoldenrod2",        238, 220, 130 },
  { "lightgoldenrod3",        205, 190, 112 },
  { "lightgoldenrod4",        139, 129,  76 },
  { "lightgoldenrodyellow",   250, 250, 210 },
  { "lightgray",              211, 211, 211 },
  { "lightgreen",             144, 238, 144 },
  { "lightgrey",              211, 211, 211 },
  { "lightpink",              255, 182, 193 },
  { "lightpink1",             255, 174, 185 },
  { "lightpink2",             238, 162, 173 },
  { "lightpink3",             205, 140, 149 },
  { "lightpink4",             139,  95, 101 },
  { "lightsalmon",            255, 160, 122 },
  { "lightsalmon1",           255, 160, 122 },
  { "lightsalmon2",           238, 149, 114 },
  { "lightsalmon3",           205, 129,  98 },
  { "lightsalmon4",           139,  87,  66 },
  { "lightseagreen",           32, 178, 170 },
  { "lightskyblue",           135, 206, 250 },
  { "lightskyblue1",          176, 226, 255 },
  { "lightskyblue2",          164, 211, 238 },
  { "lightskyblue3",          141, 182, 205 },
  { "lightskyblue4",           96, 123, 139 },
  { "lightslateblue",         132, 112, 255 },
  { "lightslategray",         119, 136, 153 },
  { "lightslategrey",         119, 136, 153 },
  { "lightsteelblue",         176, 196, 222 },
  { "lightsteelblue1",        202, 225, 255 },
  { "lightsteelblue2",        188, 210, 238 },
  { "lightsteelblue3",        162, 181, 205 },
  { "lightsteelblue4",        110, 123, 139 },
  { "lightyellow",            255, 255, 224 },
  { "lightyellow1",           255, 255, 224 },
  { "lightyellow2",           238, 238, 209 },
  { "lightyellow3",           205, 205, 180 },
  { "lightyellow4",           139, 139, 122 },
  { "limegreen",               50, 205,  50 },
  { "linen",                  250, 240, 230 },
  { "magenta",                255,   0, 255 },
  { "magenta1",               255,   0, 255 },
  { "magenta2",               238,   0, 238 },
  { "magenta3",               205,   0, 205 },
  { "magenta4",               139,   0, 139 },
  { "maroon",                 176,  48,  96 },
  { "maroon1",                255,  52, 179 },
  { "maroon2",                238,  48, 167 },
  { "maroon3",                205,  41, 144 },
  { "maroon4",                139,  28,  98 },
  { "mediumaquamarine",       102, 205, 170 },
  { "mediumblue",               0,   0, 205 },
  { "mediumorchid",           186,  85, 211 },
  { "mediumorchid1",          224, 102, 255 },
  { "mediumorchid2",          209,  95, 238 },
  { "mediumorchid3",          180,  82, 205 },
  { "mediumorchid4",          122,  55, 139 },
  { "mediumpurple",           147, 112, 219 },
  { "mediumpurple1",          171, 130, 255 },
  { "mediumpurple2",          159, 121, 238 },
  { "mediumpurple3",          137, 104, 205 },
  { "mediumpurple4",           93,  71, 139 },
  { "mediumseagreen",          60, 179, 113 },
  { "mediumslateblue",        123, 104, 238 },
  { "mediumspringgreen",        0, 250, 154 },
  { "mediumturquoise",         72, 209, 204 },
  { "mediumvioletred",        199,  21, 133 },
  { "midnightblue",            25,  25, 112 },
  { "mintcream",              245, 255, 250 },
  { "mistyrose",              255, 228, 225 },
  { "mistyrose1",             255, 228, 225 },
  { "mistyrose2",             238, 213, 210 },
  { "mistyrose3",             205, 183, 181 },
  { "mistyrose4",             139, 125, 123 },
  { "moccasin",               255, 228, 181 },
  { "navajowhite",            255, 222, 173 },
  { "navajowhite1",           255, 222, 173 },
  { "navajowhite2",           238, 207, 161 },
  { "navajowhite3",           205, 179, 139 },
  { "navajowhite4",           139, 121,  94 },
  { "navy",                     0,   0, 128 },
  { "navyblue",                 0,   0, 128 },
  { "oldlace",                253, 245, 230 },
  { "olivedrab",              107, 142,  35 },
  { "olivedrab1",             192, 255,  62 },
  { "olivedrab2",             179, 238,  58 },
  { "olivedrab3",             154, 205,  50 },
  { "olivedrab4",             105, 139,  34 },
  { "orange",                 255, 165,   0 },
  { "orange1",                255, 165,   0 },
  { "orange2",                238, 154,   0 },
  { "orange3",                205, 133,   0 },
  { "orange4",                139,  90,   0 },
  { "orangered",              255,  69,   0 },
  { "orangered1",             255,  69,   0 },
  { "orangered2",             238,  64,   0 },
  { "orangered3",             205,  55,   0 },
  { "orangered4",             139,  37,   0 },
  { "orchid",                 218, 112, 214 },
  { "orchid1",                255, 131, 250 },
  { "orchid2",                238, 122, 233 },
  { "orchid3",                205, 105, 201 },
  { "orchid4",                139,  71, 137 },
  { "palegoldenrod",          238, 232, 170 },
  { "palegreen",              152, 251, 152 },
  { "palegreen1",             154, 255, 154 },
  { "palegreen2",             144, 238, 144 },
  { "palegreen3",             124, 205, 124 },
  { "palegreen4",              84, 139,  84 },
  { "paleturquoise",          175, 238, 238 },
  { "paleturquoise1",         187, 255, 255 },
  { "paleturquoise2",         174, 238, 238 },
  { "paleturquoise3",         150, 205, 205 },
  { "paleturquoise4",         102, 139, 139 },
  { "palevioletred",          219, 112, 147 },
  { "palevioletred1",         255, 130, 171 },
  { "palevioletred2",         238, 121, 159 },
  { "palevioletred3",         205, 104, 137 },
  { "palevioletred4",         139,  71,  93 },
  { "papayawhip",             255, 239, 213 },
  { "peachpuff",              255, 218, 185 },
  { "peachpuff1",             255, 218, 185 },
  { "peachpuff2",             238, 203, 173 },
  { "peachpuff3",             205, 175, 149 },
  { "peachpuff4",             139, 119, 101 },
  { "peru",                   205, 133,  63 },
  { "pink",                   255, 192, 203 },
  { "pink1",                  255, 181, 197 },
  { "pink2",                  238, 169, 184 },
  { "pink3",                  205, 145, 158 },
  { "pink4",                  139,  99, 108 },
  { "plum",                   221, 160, 221 },
  { "plum1",                  255, 187, 255 },
  { "plum2",                  238, 174, 238 },
  { "plum3",                  205, 150, 205 },
  { "plum4",                  139, 102, 139 },
  { "powderblue",             176, 224, 230 },
  { "purple",                 160,  32, 240 },
  { "purple1",                155,  48, 255 },
  { "purple2",                145,  44, 238 },
  { "purple3",                125,  38, 205 },
  { "purple4",                 85,  26, 139 },
  { "red",                    255,   0,   0 },
  { "red1",                   255,   0,   0 },
  { "red2",                   238,   0,   0 },
  { "red3",                   205,   0,   0 },
  { "red4",                   139,   0,   0 },
  { "rosybrown",              188, 143, 143 },
  { "rosybrown1",             255, 193, 193 },
  { "rosybrown2",             238, 180, 180 },
  { "rosybrown3",             205, 155, 155 },
  { "rosybrown4",             139, 105, 105 },
  { "royalblue",               65, 105, 225 },
  { "royalblue1",              72, 118, 255 },
  { "royalblue2",              67, 110, 238 },
  { "royalblue3",              58,  95, 205 },
  { "royalblue4",              39,  64, 139 },
  { "saddlebrown",            139,  69,  19 },
  { "salmon",                 250, 128, 114 },
  { "salmon1",                255, 140, 105 },
  { "salmon2",                238, 130,  98 },
  { "salmon3",                205, 112,  84 },
  { "salmon4",                139,  76,  57 },
  { "sandybrown",             244, 164,  96 },
  { "seagreen",                46, 139,  87 },
  { "seagreen1",               84, 255, 159 },
  { "seagreen2",               78, 238, 148 },
  { "seagreen3",               67, 205, 128 },
  { "seagreen4",               46, 139,  87 },
  { "seashell",               255, 245, 238 },
  { "seashell1",              255, 245, 238 },
  { "seashell2",              238, 229, 222 },
  { "seashell3",              205, 197, 191 },
  { "seashell4",              139, 134, 130 },
  { "sienna",                 160,  82,  45 },
  { "sienna1",                255, 130,  71 },
  { "sienna2",                238, 121,  66 },
  { "sienna3",                205, 104,  57 },
  { "sienna4",                139,  71,  38 },
  { "skyblue",                135, 206, 235 },
  { "skyblue1",               135, 206, 255 },
  { "skyblue2",               126, 192, 238 },
  { "skyblue3",               108, 166, 205 },
  { "skyblue4",                74, 112, 139 },
  { "slateblue",              106,  90, 205 },
  { "slateblue1",             131, 111, 255 },
  { "slateblue2",             122, 103, 238 },
  { "slateblue3",             105,  89, 205 },
  { "slateblue4",              71,  60, 139 },
  { "slategray",              112, 128, 144 },
  { "slategray1",             198, 226, 255 },
  { "slategray2",             185, 211, 238 },
  { "slategray3",             159, 182, 205 },
  { "slategray4",             108, 123, 139 },
  { "slategrey",              112, 128, 144 },
  { "snow",                   255, 250, 250 },
  { "snow1",                  255, 250, 250 },
  { "snow2",                  238, 233, 233 },
  { "snow3",                  205, 201, 201 },
  { "snow4",                  139, 137, 137 },
  { "springgreen",              0, 255, 127 },
  { "springgreen1",             0, 255, 127 },
  { "springgreen2",             0, 238, 118 },
  { "springgreen3",             0, 205, 102 },
  { "springgreen4",             0, 139,  69 },
  { "steelblue",               70, 130, 180 },
  { "steelblue1",              99, 184, 255 },
  { "steelblue2",              92, 172, 238 },
  { "steelblue3",              79, 148, 205 },
  { "steelblue4",              54, 100, 139 },
  { "tan",                    210, 180, 140 },
  { "tan1",                   255, 165,  79 },
  { "tan2",                   238, 154,  73 },
  { "tan3",                   205, 133,  63 },
  { "tan4",                   139,  90,  43 },
  { "thistle",                216, 191, 216 },
  { "thistle1",               255, 225, 255 },
  { "thistle2",               238, 210, 238 },
  { "thistle3",               205, 181, 205 },
  { "thistle4",               139, 123, 139 },
  { "tomato",                 255,  99,  71 },
  { "tomato1",                255,  99,  71 },
  { "tomato2",                238,  92,  66 },
  { "tomato3",                205,  79,  57 },
  { "tomato4",                139,  54,  38 },
  { "turquoise",               64, 224, 208 },
  { "turquoise1",               0, 245, 255 },
  { "turquoise2",               0, 229, 238 },
  { "turquoise3",               0, 197, 205 },
  { "turquoise4",               0, 134, 139 },
  { "violet",                 238, 130, 238 },
  { "violetred",              208,  32, 144 },
  { "violetred1",             255,  62, 150 },
  { "violetred2",             238,  58, 140 },
  { "violetred3",             205,  50, 120 },
  { "violetred4",             139,  34,  82 },
  { "wheat",                  245, 222, 179 },
  { "wheat1",                 255, 231, 186 },
  { "wheat2",                 238, 216, 174 },
  { "wheat3",                 205, 186, 150 },
  { "wheat4",                 139, 126, 102 },
  { "white",                  255, 255, 255 },
  { "whitesmoke",             245, 245, 245 },
  { "yellow",                 255, 255,   0 },
  { "yellow1",                255, 255,   0 },
  { "yellow2",                238, 238,   0 },
  { "yellow3",                205, 205,   0 },
  { "yellow4",                139, 139,   0 },
  { "yellowgreen",            154, 205,  50 }
};

int32_t xitk_str2int32 (const char **str) {
  const uint8_t *p;
  uint32_t v = 0, sign = 0;
  int32_t w;
  uint8_t z;

  if (!str)
    return 0;
  p = (const uint8_t *)*str;
  if (*p == '~')
    sign |= 2, p++;
  while (*p == '-')
    sign ^= 1, p++;

  do {
    if (*p == '#') {
      p++;
      /* hex */
      while ((z = tab_unhex[*p]) < 16u)
        v = (v << 4) + z, p++;
      break;
    }
    if (*p == '0') {
      p++;
      if ((*p | 0x20) == 'x') {
        p++;
        /* hex */
        while ((z = tab_unhex[*p]) < 16u)
          v = (v << 4) + z, p++;
        break;
      }
      /* octal */
      while ((z = *p ^ '0') < 8u)
        v = (v << 3) + z, p++;
      break;
    }
    /* decimal */
    while ((z = *p ^ '0') < 10u)
      v = v * 10u + z, p++;
  } while (0);
  *str = (const char *)p;
  w = (int32_t)v;
  if (sign & 1)
    w = -w;
  if (sign & 2)
    w = ~w;
  return w;
}

/* Return a rgb color from a string color. */
uint32_t xitk_get_color_name (const char *color) {
  char lname[32];
  const char *s;
  uint32_t v;

  if (!color) {
    XITK_WARNING ("color was NULL.\n");
    return ~0;
  }

  /* try plain int */
  s = color;
  v = xitk_str2int32 (&s);
  if (s > color)
    return v;

  /* convert copied color to lowercase */
  xitk_lower_strlcpy (lname, color, sizeof (lname));
  {
    unsigned int b = 0, m, e = sizeof (xitk_sorted_color_names) / sizeof (xitk_sorted_color_names[0]);
    int d;
    do {
      m = (b + e) >> 1;
      d = strcmp (lname, xitk_sorted_color_names[m].colorname);
      if (d == 0)
        break;
      if (d < 0)
        e = m;
      else
        b = m + 1;
    } while (b != e);
    if (d == 0) {
      return ((uint32_t)xitk_sorted_color_names[m].red << 16)
           | ((uint32_t)xitk_sorted_color_names[m].green << 8)
           |            xitk_sorted_color_names[m].blue;
    }
  }
  return ~0u;
}

