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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "utils.h"
#include "dump.h"
#include "_xitk.h"
#include "xitk_x11.h"
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
  }                           border_size, old_pos, new_pos;

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


typedef enum {
  XITK_A_WIN_LAYER = 0,
  XITK_A_END
} xitk_atom_t;

struct __xitk_s {
  xitk_t                      x;

  xitk_x11_t                 *xitk_x11;
  Display                    *display;

  int                         display_width;
  int                         display_height;
  int                         verbosity;
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

  uint32_t                    wm_type;

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

  xitk_widget_t              *menu;

  struct timeval              keypress;

  KeyCode                     ignore_keys[2];

  unsigned long               tips_timeout;

  uint32_t                    qual;

  pid_t                       xitk_pid;

  Atom                        atoms[XITK_A_END];
  Atom                        XA_XITK;

  struct {
    xitk_widget_t          *widget_in;
  }                         clipboard;
};

/*
 *
 */

static void _xitk_lock_display (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  XLockDisplay (xitk->display);
}
static void _xitk_unlock_display (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  XUnlockDisplay (xitk->display);
}

static void _xitk_dummy_lock_display (xitk_t *_xitk) {
  (void)_xitk;
}

/*
 *
 */

void xitk_set_focus_to_wl (xitk_widget_list_t *wl) {
  __xitk_t *xitk;
  __gfx_t *fx;

  if (!wl)
    return;
  if (!wl->xitk)
    return;
  xitk_container (xitk, wl->xitk, x);
  MUTLOCK ();
  for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next) {
    if (&fx->wl == wl) {
      xitk_window_set_input_focus (fx->wl.xwin);
      break;
    }
  }
  MUTUNLOCK ();
}


static void __fx_ref (__gfx_t *fx) {
  fx->refs++;
}

static void __fx_delete (__gfx_t *fx) {
  __xitk_t *xitk = fx->xitk;

  if (xitk->verbosity >= 2)
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

  if (xitk->verbosity >= 2)
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
      if (event.x < fx->expose.x1)
        fx->expose.x1 = event.x;
      if (event.x + event.w > fx->expose.x2)
        fx->expose.x2 = event.x + event.w;
      if (event.y < fx->expose.y1)
        fx->expose.y1 = event.y;
      if (event.y + event.h > fx->expose.y2)
        fx->expose.y2 = event.y + event.h;
      if (event.more > 0)
        continue;
      if (xitk->verbosity >= 2)
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

  if (xitk->verbosity >= 2)
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
  if (xitk->menu)
    xitk_destroy_widget (xitk->menu);
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
      xitk_x11_delete (xitk->xitk_x11);
      xitk->xitk_x11 = NULL;
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

static uint32_t xitk_check_wm (__xitk_t *xitk, Display *display)
{
  uint32_t type;

  xitk->x.lock_display (&xitk->x);

  type = xitk_x11_check_wm(display, xitk->verbosity >= 2);

  xitk->XA_XITK = XInternAtom (display, "_XITK_EVENT", False);

  switch(type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_KWIN:
    break;

  case WM_TYPE_MOTIF:
  case WM_TYPE_LARSWM:
    break;

  case WM_TYPE_UNKNOWN:
  case WM_TYPE_E:
  case WM_TYPE_ICE:
  case WM_TYPE_WINDOWMAKER:
  case WM_TYPE_XFCE:
  case WM_TYPE_SAWFISH:
  case WM_TYPE_METACITY: /* Untested */
  case WM_TYPE_AFTERSTEP:
  case WM_TYPE_BLACKBOX:
  case WM_TYPE_DTWM:
    xitk->atoms[XITK_A_WIN_LAYER] = XInternAtom(display, "_WIN_LAYER", False);
    break;
  }

  xitk->x.unlock_display (&xitk->x);

  return type;
}

uint32_t xitk_get_wm_type (xitk_t *xitk) {
  __xitk_t *_xitk;

  xitk_container (_xitk, xitk, x);
  return _xitk->wm_type;
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

  if (xitk->verbosity >= 2) {
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

  fx->xitk                  = xitk;
  fx->refs                  = 1;
  fx->name[0]               = 0;
  fx->move.enabled          = 0;
  fx->move.offset_x         = 0;
  fx->move.offset_y         = 0;
  fx->border_size.y         = 0;
  fx->border_size.x         = 0;
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

  if (xitk->verbosity >= 2)
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

const char *xitk_be_event_name (const xitk_be_event_t *event) {
  if (!event)
    return NULL;
  if (!event->window)
    return NULL;
  if (!event->window->display)
    return NULL;
  return event->window->display->event_name (event->window->display, event);
}

/*
 * Register a window, with his own event handler, callback
 * for DND events, and widget list.
 */

xitk_register_key_t xitk_be_register_event_handler (const char *name, xitk_window_t *xwin,
  xitk_widget_list_t *wl,
  xitk_be_event_handler_t *event_handler, void *user_data,
  void (*destructor) (void *data), void *destr_data) {
  __xitk_t *xitk;
  __gfx_t   *fx;

  //  printf("%s()\n", __FUNCTION__);

  ABORT_IF_NULL (xwin);

  if (!wl)
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

  if (fx->wl.xwin && fx->wl.xwin->bewin) {
    xitk_tagitem_t tags1[] = {
      {XITK_TAG_X, 0},
      {XITK_TAG_Y, 0},
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
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
    fx->wl.xwin->bewin->set_props (fx->wl.xwin->bewin, tags2);
  }

  fx->user_data = user_data;

  fx->event_handler = event_handler;
  fx->destructor = destructor;
  fx->destr_data = destr_data;

  if(fx->wl.xwin) {
    xitk_lock_display (&xitk->x);
    XChangeProperty (xitk->display, fx->wl.xwin->bewin->id, xitk->XA_XITK, XA_ATOM,
		     32, PropModeAppend, (unsigned char *)&XITK_VERSION, 1);
    xitk_unlock_display (&xitk->x);
  }

  MUTLOCK ();
  fx->key = ++xitk->key;
  xitk_dlist_add_tail (&xitk->gfxs, &fx->wl.node);
  MUTUNLOCK ();

  if (xitk->verbosity >= 2)
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

void xitk_window_set_border_size (xitk_t *_xitk, xitk_register_key_t key, int left, int top) {
  __xitk_t *xitk;
  __gfx_t  *fx;

  xitk_container (xitk, _xitk, x);
  if (!xitk || !key)
    return;
  MUTLOCK ();
  fx = __fx_from_key (xitk, key);
  if (fx) {
    fx->border_size.y = top;
    fx->border_size.x = left;
  }
  MUTUNLOCK ();
}

void xitk_register_eh_destructor (xitk_t *_xitk, xitk_register_key_t key,
  void (*destructor)(void *userdata), void *destr_data) {
  __xitk_t *xitk;
  __gfx_t *fx;

  if (!key || !_xitk)
    return;
  xitk_container (xitk, _xitk, x);
  MUTLOCK ();
  fx = __fx_from_key (xitk, key);
  if (fx) {
    fx->destructor = destructor;
    fx->destr_data = destr_data;
  }
  MUTUNLOCK ();
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
      {
        size_t i;
        /* Filter keys that dont't need to be handled by xine
         * and could be used by our screen saver reset "ping".
         * So they will not kill tips and menus. */
        for (i = 0; i < sizeof (xitk->ignore_keys) / sizeof (xitk->ignore_keys[0]); ++i)
          if (event->code == xitk->ignore_keys[i])
            break;
        if (i < sizeof (xitk->ignore_keys) / sizeof (xitk->ignore_keys[0])) {
          handled = 1;
          break;
        }
      }
      gettimeofday (&xitk->keypress, 0);
      break;

    case XITK_EV_KEY_DOWN:
      {
        size_t i;
        /* Filter keys that dont't need to be handled by xine
         * and could be used by our screen saver reset "ping".
         * So they will not kill tips and menus. */
        for (i = 0; i < sizeof (xitk->ignore_keys) / sizeof (xitk->ignore_keys[0]); ++i)
          if (event->code == xitk->ignore_keys[i])
            break;
        if (i < sizeof (xitk->ignore_keys) / sizeof (xitk->ignore_keys[0])) {
          handled = 1;
          break;
        }
      }
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

        w = fx->wl.widget_focused ? fx->wl.widget_focused : NULL;

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

        handled = xitk_widget_key_event (w, event->utf8, event->qual);

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
                xitk_destroy_widget (xitk_menu_get_menu (w));
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
    case XITK_EV_UNFOCUS:
      if (xwin && xwin->bewin) {
        xitk_tagitem_t tags[] = {{XITK_TAG_WIN_FLAGS, 0}, {XITK_TAG_END, 0}};
        xwin->bewin->get_props (xwin->bewin, tags);
        xwin->flags = tags[0].value;
      }
      break;

    case XITK_EV_MOVE:
      event->qual |= xitk->qual;
      if (!fx)
        break;

      while (xitk->x.d->next_event (xitk->x.d, event, NULL, XITK_EV_MOVE, 0)) ;

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

        tags[0].value = fx->new_pos.x - fx->border_size.x;
        tags[1].value = fx->new_pos.y - fx->border_size.y;
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
              handled = xitk_widget_key_event (w, event->code == 4 ? sup : sdown, event->qual);
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

Display *xitk_x11_get_display(xitk_t *xitk) {
  __xitk_t *_xitk;
  xitk_container (_xitk, xitk, x);
  return _xitk->display;
}

static void xitk_dummy_un_lock_display (Display *display) {
  (void)display;
}

void (*xitk_x_lock_display) (Display *display) = xitk_dummy_un_lock_display;
void (*xitk_x_unlock_display) (Display *display) = xitk_dummy_un_lock_display;

int xitk_image_quality (xitk_t *xitk, int qual) {
  if (xitk->d->image_quality)
    return xitk->d->image_quality(xitk->d, qual);
  return qual;
}

void xitk_sync(xitk_t *xitk) {
  __xitk_t *_xitk;
  xitk_container (_xitk, xitk, x);
  xitk_lock_display (xitk);
  XSync (_xitk->display, False);
  xitk_unlock_display (xitk);
}

/*
 * Initiatization of widget internals.
 */

xitk_t *xitk_init (const char *prefered_visual, int install_colormap,
                   int use_x_lock_display, int use_synchronized_x, int verbosity) {

  __xitk_t *xitk;
  char buffer[256];
  pthread_mutexattr_t attr;

  if (use_x_lock_display) {
    /* Nasty (temporary) kludge. */
    xitk_x_lock_display = XLockDisplay;
    xitk_x_unlock_display = XUnlockDisplay;
  }

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
  xitk->display = (Display *)xitk->x.d->id;

  xitk->xitk_pid = getppid ();

  if (use_x_lock_display) {
    xitk->x.lock_display   = _xitk_lock_display;
    xitk->x.unlock_display = _xitk_unlock_display;
  } else {
    xitk->x.lock_display   = _xitk_dummy_lock_display;
    xitk->x.unlock_display = _xitk_dummy_lock_display;
  }
#ifdef DEBUG_LOCKDISPLAY
  pthread_mutex_init (&xitk->x.debug_mutex, NULL);
  xitk->x.debug_level = 0;
#endif

  xitk->xitk_x11 = xitk_x11_new (&xitk->x);

  xitk->event_bridge.running = 0;

  xitk_color_db_init (xitk);

  {
    int s = DefaultScreen (xitk->display);
    xitk->display_width   = DisplayWidth (xitk->display, s);
    xitk->display_height  = DisplayHeight (xitk->display, s);
  }
  xitk->verbosity       = verbosity;
  xitk_dlist_init (&xitk->gfxs);
  xitk->key             = 0;
  xitk->sig_callback    = NULL;
  xitk->sig_data        = NULL;
  xitk->config          = xitk_config_init (&xitk->x);
  xitk->ignore_keys[0]  = XKeysymToKeycode (xitk->display, XK_Shift_L);
  xitk->ignore_keys[1]  = XKeysymToKeycode (xitk->display, XK_Control_L);
  xitk->qual            = 0;
  xitk->tips_timeout    = TIPS_TIMEOUT;
  {
    unsigned int u;
    for (u = 0; u < XITK_A_END; u++)
      xitk->atoms[u] = None;
  }

  memset(&xitk->keypress, 0, sizeof(xitk->keypress));

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&xitk->mutex, &attr);

  snprintf(buffer, sizeof(buffer), "-[ xiTK version %d.%d.%d ", XITK_MAJOR_VERSION, XITK_MINOR_VERSION, XITK_SUB_VERSION);

#ifdef WITH_XFT
  strlcat(buffer, "[XFT]", sizeof(buffer));
#elif defined(WITH_XMB)
  strlcat(buffer, "[XMB]", sizeof(buffer));
#endif

  strlcat(buffer, " ]-", sizeof(buffer));

  if (verbosity >= 1)
    printf("%s", buffer);

  xitk->wm_type = xitk_check_wm (xitk, xitk->display);

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
  xitk_lock_display (&xitk->x);
  xitk_unlock_display (&xitk->x);

  xitk->x.d->close (&xitk->x.d);
  xitk->display = NULL;
  xitk->x.be->_delete (&xitk->x.be);

#ifdef DEBUG_LOCKDISPLAY
  pthread_mutex_destroy (&xitk->x.debug_mutex);
  printf ("%s:%d %s (): final display lock level: #%d.\n", __FILE__, __LINE__, __FUNCTION__, xitk->x.debug_level);
#endif
  pthread_mutex_destroy (&xitk->mutex);

  xitk_x11_delete (xitk->xitk_x11);
  xitk->xitk_x11 = NULL;

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
  xitk_tips_delete (&xitk->x.tips);
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
    case XITK_TIPS_TIMEOUT: return xitk->tips_timeout;
    default: return xitk_config_get_num (xitk->config, item);
  }
}

void xitk_get_display_size (xitk_t *xitk, int *w, int *h) {
  __xitk_t *_xitk;

  xitk_container (_xitk, xitk, x);
  if (w)
    *w = _xitk->display_width;
  if (h)
    *h = _xitk->display_height;
}

void xitk_set_tips_timeout(xitk_t *_xitk, unsigned long timeout) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  xitk->tips_timeout = timeout;
}

char *xitk_filter_filename(const char *name) {
  if (!name)
    return NULL;
  if (!strncasecmp (name, "file:", 5)) {
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

/*
 * Return 0/1 from char value (valids are 1/0, true/false,
 * yes/no, on/off. Case isn't checked.
 */
int xitk_get_bool_value(const char *val) {
  if (!val)
    return 0;
  if ((*val >= '1') && (*val <= '9'))
    return 1;
  if (!strcasecmp (val, "true"))
    return 1;
  if (!strcasecmp (val, "yes"))
    return 1;
  if (!strcasecmp (val, "on"))
    return 1;
  return 0;
}

static const uint8_t tab_tolower[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64,'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w','x','y','z', 91, 92, 93, 94, 95,
   96,'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w','x','y','z',123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

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
          item->key = p - start;
          item = NULL;
          if (flags & XITK_CFG_PARSE_CASE) {
            while (!((z = tab_cfg_parse[*p]) & 0xfd))
              p++;
          } else {
            while (!((z = tab_cfg_parse[*p]) & 0xfd)) {
              *p = tab_tolower[*p];
              p++;
            }
          }
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

