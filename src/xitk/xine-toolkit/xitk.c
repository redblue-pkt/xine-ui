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

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#ifdef HAVE_X11_EXTENSIONS_XSHM_H
#include <X11/extensions/XShm.h>
#endif

#include "xitk/Imlib-light/Imlib.h"

#include "utils.h"
#include "dump.h"
#include "_xitk.h"
#include "xitk_x11.h"
#include "tips.h"
#include "widget.h"
#include "menu.h"
#include "combo.h"
#include "cursors.h"
#include "dnd.h"
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

  xitk_window_t              *w;
  Window                      window;

  xitk_move_t                 move;

  struct {
    int                       x, y;
  }                           border_size, old_pos, new_pos;

  int                         width;
  int                         height;

  xitk_hull_t                 expose;

  char                        name[64];
  const xitk_event_cbs_t     *cbs;
  xitk_register_key_t         key;
  xitk_dnd_t                 *xdnd;
  void                       *user_data;

  xitk_be_event_handler_t    *event_handler;
  void                      (*destructor)(void *userdata);
  void                       *destr_data;
} __gfx_t;


typedef enum {
  XITK_A_WIN_LAYER = 0,
  XITK_A_STAYS_ON_TOP,
  XITK_A_NET_WM_STATE,
  XITK_A_NET_WM_STATE_ABOVE,
  XITK_A_NET_WM_STATE_FULLSCREEN,
  XITK_A_WM_WINDOW_TYPE,
  XITK_A_WM_WINDOW_TYPE_DESKTOP,
  XITK_A_WM_WINDOW_TYPE_DOCK,
  XITK_A_WM_WINDOW_TYPE_TOOLBAR,
  XITK_A_WM_WINDOW_TYPE_MENU,
  XITK_A_WM_WINDOW_TYPE_UTILITY,
  XITK_A_WM_WINDOW_TYPE_SPLASH,
  XITK_A_WM_WINDOW_TYPE_DIALOG,
  XITK_A_WM_WINDOW_TYPE_DROPDOWN_MENU,
  XITK_A_WM_WINDOW_TYPE_POPUP_MENU,
  XITK_A_WM_WINDOW_TYPE_TOOLTIP,
  XITK_A_WM_WINDOW_TYPE_NOTIFICATION,
  XITK_A_WM_WINDOW_TYPE_COMBO,
  XITK_A_WM_WINDOW_TYPE_DND,
  XITK_A_WM_WINDOW_TYPE_NORMAL,
  XITK_A_END
} xitk_atom_t;

struct __xitk_s {
  xitk_t                      x;

  xitk_x11_t                 *xitk_x11;

  int                         display_width;
  int                         display_height;
  int                         verbosity;
  xitk_dlist_t                gfxs;
  int                         use_xshm;
  int                         install_colormap;

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

  int                        (*x_error_handler)(Display *, XErrorEvent *);

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

  //Window                      modalw;
  xitk_widget_t              *menu;

  struct timeval              keypress;

  KeyCode                     ignore_keys[2];

  unsigned long               tips_timeout;

  uint32_t                    qual;

#if 0
  /* not used  ? */
  struct {
    Window                    window;
    int                       focus;
  } parent;
#endif

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
  XLockDisplay (xitk->x.display);
}
static void _xitk_unlock_display (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  XUnlockDisplay (xitk->x.display);
}

static void _xitk_dummy_lock_display (xitk_t *_xitk) {
  (void)_xitk;
}

/*
 *
 */

xitk_t *gXitk;

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
      xitk_lock_display (&xitk->x);
      XSetInputFocus (xitk->x.display, fx->window, RevertToParent, CurrentTime);
      xitk_unlock_display (&xitk->x);
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

  if (fx->xdnd) {
    xitk_dnd_delete (fx->xdnd);
    fx->xdnd = NULL;
  }

  fx->cbs       = NULL;
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

void xitk_clipboard_unregister_window (Window win) {
  (void)win;
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
void xitk_modal_window(Window w) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  xitk->modalw = w;
}
void xitk_unmodal_window(Window w) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if (w == xitk->modalw)
    xitk->modalw = None;
}
*/

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

#if 0
static int _x_ignoring_error_handler(Display *display, XErrorEvent *xevent) {
  (void)display;
  (void)xevent;
  return 0;
}
#endif

static int _x_error_handler(Display *display, XErrorEvent *xevent) {
  char buffer[2048];

  XGetErrorText(display, xevent->error_code, &buffer[0], 1023);

  XITK_WARNING("X error received: '%s'\n", buffer);

  xitk_x_error = 1;
  return 0;

}

void xitk_set_current_menu(xitk_widget_t *menu) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if (xitk->menu)
    xitk_destroy_widget (xitk->menu);
  xitk->menu = menu;
}

void xitk_unset_current_menu(void) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  xitk->menu = NULL;
}

int xitk_install_x_error_handler (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  if (xitk->x_error_handler == NULL) {
    xitk->x_error_handler = XSetErrorHandler (_x_error_handler);
    XSync (xitk->x.display, False);
    return 1;
  }
  return 0;
}

int xitk_uninstall_x_error_handler (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  if (xitk->x_error_handler != NULL) {
    XSetErrorHandler (xitk->x_error_handler);
    xitk->x_error_handler = NULL;
    XSync (xitk->x.display, False);
    return 1;
  }
  return 0;
}

/*
 *
 */
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

/*
 * Desc: query the server for support for the MIT_SHM extension
 * Return:  0 = not available
 *          1 = shared XImage support available
 *          2 = shared Pixmap support available also
 * Shamelessly stolen from gdk, and slightly tuned for xitk.
 */
static int xitk_check_xshm(Display *display) {
#ifdef HAVE_SHM
  if(XShmQueryExtension(display)) {
    int major, minor, ignore;
    Bool pixmaps;

    if(XQueryExtension(display, "MIT-SHM", &ignore, &ignore, &ignore)) {
      if(XShmQueryVersion(display, &major, &minor, &pixmaps ) == True) {
	if((pixmaps == True) && ((XShmPixmapFormat(display)) == ZPixmap))
	  return 2;
	else if(pixmaps == True)
	  return 1;
      }
    }
  }
#else
  (void)display;
#endif
  return 0;
}

int xitk_is_use_xshm (xitk_t *_xitk) {
  __xitk_t *xitk;

  xitk_container (xitk, _xitk, x);
  return xitk->use_xshm;
}

/* Extract WM Name */
static unsigned char *get_wm_name (Display *display, Window win, Atom atom, Atom type_utf8) {
  unsigned char   *prop_return = NULL;
  unsigned long    nitems_return, bytes_after_return;
  Atom             type_return;
  int              format_return;

  if (atom == None)
    return NULL;

  if ((XGetWindowProperty (display, win, atom, 0, 4, False, XA_STRING,
    &type_return, &format_return, &nitems_return, &bytes_after_return, &prop_return)) != Success)
    return NULL;

  do {
    if (type_return == None)
      break;
    if (type_return == XA_STRING)
      return prop_return;
    if (type_return == type_utf8) {
      if (prop_return) {
        XFree (prop_return);
        prop_return = NULL;
      }
      if ((XGetWindowProperty (display, win, atom, 0, 4, False, type_utf8,
        &type_return, &format_return, &nitems_return, &bytes_after_return, &prop_return)) != Success)
        break;
    }
    if (format_return == 8)
      return prop_return;
  } while (0);
  if (prop_return)
    XFree (prop_return);
  return NULL;
}
static uint32_t xitk_check_wm (__xitk_t *xitk, Display *display) {
  enum {
    _XFWM_F = 0,
    __WINDO, __SAWMI, _ENLIGH, __METAC, __AS_ST,
    __ICEWM, __BLACK, _LARSWM, _KWIN_R, __DT_SM,
    _DTWM_I, __WIN_S, __NET_S, __NET_W, __NET_N,
    __WIN_N, _UTF8_S, _WMEND_
  };
  static const char * const wm_det_names[_WMEND_] = {
    [_XFWM_F] = "XFWM_FLAGS",
    [__WINDO] = "_WINDOWMAKER_WM_PROTOCOLS",
    [__SAWMI] = "_SAWMILL_TIMESTAMP",
    [_ENLIGH] = "ENLIGHTENMENT_COMMS",
    [__METAC] = "_METACITY_RESTART_MESSAGE",
    [__AS_ST] = "_AS_STYLE",
    [__ICEWM] = "_ICEWM_WINOPTHINT",
    [__BLACK] = "_BLACKBOX_HINTS",
    [_LARSWM] = "LARSWM_EXIT",
    [_KWIN_R] = "KWIN_RUNNING",
    [__DT_SM] = "_DT_SM_WINDOW_INFO",
    [_DTWM_I] = "DTWM_IS_RUNNING",
    [__WIN_S] = "_WIN_SUPPORTING_WM_CHECK",
    [__NET_S] = "_NET_SUPPORTING_WM_CHECK",
    [__NET_W] = "_NET_WORKAREA",
    [__NET_N] = "_NET_WM_NAME",
    [__WIN_N] = "_WIN_WM_NAME",
    [_UTF8_S] = "UTF8_STRING"
  };
  Atom      wm_det_atoms[_WMEND_];
  uint32_t  type = WM_TYPE_UNKNOWN;
  unsigned char *wm_name = NULL;

  xitk->x.lock_display (&xitk->x);

  xitk->XA_XITK = XInternAtom (xitk->x.display, "_XITK_EVENT", False);
  
  XInternAtoms (display, (char **)wm_det_names, _WMEND_, True, wm_det_atoms);
  if (wm_det_atoms[_XFWM_F] != None)
    type |= WM_TYPE_XFCE;
  else if (wm_det_atoms[__WINDO] != None)
    type |= WM_TYPE_WINDOWMAKER;
  else if (wm_det_atoms[__SAWMI] != None)
    type |= WM_TYPE_SAWFISH;
  else if (wm_det_atoms[_ENLIGH] != None)
    type |= WM_TYPE_E;
  else if (wm_det_atoms[__METAC] != None)
    type |= WM_TYPE_METACITY;
  else if (wm_det_atoms[__AS_ST] != None)
    type |= WM_TYPE_AFTERSTEP;
  else if (wm_det_atoms[__ICEWM] != None)
    type |= WM_TYPE_ICE;
  else if (wm_det_atoms[__BLACK] != None)
    type |= WM_TYPE_BLACKBOX;
  else if (wm_det_atoms[_LARSWM] != None)
    type |= WM_TYPE_LARSWM;
  else if ((wm_det_atoms[_KWIN_R] != None) && (wm_det_atoms[__DT_SM] != None))
    type |= WM_TYPE_KWIN;
  else if (wm_det_atoms[_DTWM_I] != None)
    type |= WM_TYPE_DTWM;

  xitk_install_x_error_handler (&xitk->x);

  if (wm_det_atoms[__WIN_S] != None) {
    unsigned char   *prop_return = NULL;
    unsigned long    nitems_return;
    unsigned long    bytes_after_return;
    Atom             type_return;
    int              format_return;
    Status           status;

    /* Check for Gnome Compliant WM */
    if ((XGetWindowProperty (display, (XDefaultRootWindow (display)), wm_det_atoms[__WIN_S], 0,
			   1, False, XA_CARDINAL,
			   &type_return, &format_return, &nitems_return,
			   &bytes_after_return, &prop_return)) == Success) {

      if((type_return != None) && (type_return == XA_CARDINAL) &&
	 ((format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0))) {
	unsigned char   *prop_return2 = NULL;
	Window           win_id;

	win_id = *(unsigned long *)prop_return;

        status = XGetWindowProperty (display, win_id, wm_det_atoms[__WIN_S], 0,
				    1, False, XA_CARDINAL,
				    &type_return, &format_return, &nitems_return,
				    &bytes_after_return, &prop_return2);

	if((status == Success) && (type_return != None) && (type_return == XA_CARDINAL)) {

	  if((format_return == 32) && (nitems_return == 1)
	     && (bytes_after_return == 0) && (win_id == *(unsigned long *)prop_return2)) {
	    type |= WM_TYPE_GNOME_COMP;
	  }
	}

	if(prop_return2)
	  XFree(prop_return2);

        if (!(wm_name = get_wm_name (display, win_id, wm_det_atoms[__NET_N], wm_det_atoms[_UTF8_S]))) {
	  /*
	   * Enlightenment is Gnome compliant, but don't set
	   * the _NET_WM_NAME atom property
	   */
            wm_name = get_wm_name (display, (XDefaultRootWindow (display)), wm_det_atoms[__WIN_N], wm_det_atoms[_UTF8_S]);
	}
      }

      if(prop_return)
	XFree(prop_return);
    }
  }

  /* Check for Extended Window Manager Hints (EWMH) Compliant */
  if ((wm_det_atoms[__NET_S] != None) && (wm_det_atoms[__NET_W] != None)) {
    unsigned char   *prop_return = NULL;
    unsigned long    nitems_return;
    unsigned long    bytes_after_return;
    Atom             type_return;
    int              format_return;
    Status           status;

    if ((XGetWindowProperty (display, (XDefaultRootWindow (display)), wm_det_atoms[__NET_S], 0, 1, False, XA_WINDOW,
			   &type_return, &format_return, &nitems_return,
			   &bytes_after_return, &prop_return)) == Success) {

      if((type_return != None) && (type_return == XA_WINDOW) &&
	 (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {
	unsigned char   *prop_return2 = NULL;
	Window           win_id;

	win_id = *(unsigned long *)prop_return;

        status = XGetWindowProperty (display, win_id, wm_det_atoms[__NET_S], 0,
				    1, False, XA_WINDOW,
				    &type_return, &format_return, &nitems_return,
				    &bytes_after_return, &prop_return2);

	if((status == Success) && (type_return != None) && (type_return == XA_WINDOW) &&
	   (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {

	  if(win_id == *(unsigned long *)prop_return) {
            if (wm_name)
              XFree (wm_name);
            wm_name = get_wm_name (display, win_id, wm_det_atoms[__NET_N], wm_det_atoms[_UTF8_S]);
	    type |= WM_TYPE_EWMH_COMP;
	  }
	}
	if(prop_return2)
	  XFree(prop_return2);
      }
      if(prop_return)
	XFree(prop_return);
    }
  }

  xitk_uninstall_x_error_handler (&xitk->x);

  if (type & WM_TYPE_EWMH_COMP) {
    static const char * const atom_names[XITK_A_END] = {
        [XITK_A_WIN_LAYER]                    = "_NET_WM_STATE",
        [XITK_A_STAYS_ON_TOP]                 = "_NET_WM_STATE_STAYS_ON_TOP",
        [XITK_A_NET_WM_STATE]                 = "_NET_WM_STATE",
        [XITK_A_NET_WM_STATE_ABOVE]           = "_NET_WM_STATE_ABOVE",
        [XITK_A_NET_WM_STATE_FULLSCREEN]      = "_NET_WM_STATE_FULLSCREEN",
        [XITK_A_WM_WINDOW_TYPE]               = "_NET_WM_WINDOW_TYPE",
        [XITK_A_WM_WINDOW_TYPE_DESKTOP]       = "_NET_WM_WINDOW_TYPE_DESKTOP",
        [XITK_A_WM_WINDOW_TYPE_DOCK]          = "_NET_WM_WINDOW_TYPE_DOCK",
        [XITK_A_WM_WINDOW_TYPE_TOOLBAR]       = "_NET_WM_WINDOW_TYPE_TOOLBAR",
        [XITK_A_WM_WINDOW_TYPE_MENU]          = "_NET_WM_WINDOW_TYPE_MENU",
        [XITK_A_WM_WINDOW_TYPE_UTILITY]       = "_NET_WM_WINDOW_TYPE_UTILITY",
        [XITK_A_WM_WINDOW_TYPE_SPLASH]        = "_NET_WM_WINDOW_TYPE_SPLASH",
        [XITK_A_WM_WINDOW_TYPE_DIALOG]        = "_NET_WM_WINDOW_TYPE_DIALOG",
        [XITK_A_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
        [XITK_A_WM_WINDOW_TYPE_POPUP_MENU]    = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
        [XITK_A_WM_WINDOW_TYPE_TOOLTIP]       = "_NET_WM_WINDOW_TYPE_TOOLTIP",
        [XITK_A_WM_WINDOW_TYPE_NOTIFICATION]  = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
        [XITK_A_WM_WINDOW_TYPE_COMBO]         = "_NET_WM_WINDOW_TYPE_COMBO",
        [XITK_A_WM_WINDOW_TYPE_DND]           = "_NET_WM_WINDOW_TYPE_DND",
        [XITK_A_WM_WINDOW_TYPE_NORMAL]        = "_NET_WM_WINDOW_TYPE_NORMAL"
    };
    XInternAtoms (xitk->x.display, (char **)atom_names, XITK_A_END, False, xitk->atoms);
  }

  switch(type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_KWIN:
    if (xitk->atoms[XITK_A_NET_WM_STATE] == None)
      xitk->atoms[XITK_A_NET_WM_STATE] = XInternAtom(display, "_NET_WM_STATE", False);
    if (xitk->atoms[XITK_A_STAYS_ON_TOP] == None)
      xitk->atoms[XITK_A_STAYS_ON_TOP] = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
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

  if (xitk->verbosity >= 2) {
    static const char * const names[] = {
        [WM_TYPE_UNKNOWN]     = "Unknown",
        [WM_TYPE_KWIN]        = "KWIN",
        [WM_TYPE_E]           = "Enlightenment",
        [WM_TYPE_ICE]         = "Ice",
        [WM_TYPE_WINDOWMAKER] = "WindowMaker",
        [WM_TYPE_MOTIF]       = "Motif(like?)",
        [WM_TYPE_XFCE]        = "XFce",
        [WM_TYPE_SAWFISH]     = "Sawfish",
        [WM_TYPE_METACITY]    = "Metacity",
        [WM_TYPE_AFTERSTEP]   = "Afterstep",
        [WM_TYPE_BLACKBOX]    = "Blackbox",
        [WM_TYPE_LARSWM]      = "LarsWM",
        [WM_TYPE_DTWM]        = "dtwm"
    };
    uint32_t t = type & WM_TYPE_COMP_MASK;
    const char *name = (t < sizeof (names) / sizeof (names[0])) ? names[t] : NULL;

    if (!name)
      name = names[WM_TYPE_UNKNOWN];
    printf ("[ WM type: %s%s%s {%s} ]-\n",
      (type & WM_TYPE_GNOME_COMP) ? "(GnomeCompliant) " : "",
      (type & WM_TYPE_EWMH_COMP) ? "(EWMH) " : "",
      name, wm_name ? (char *)wm_name : "");
  }

  if (wm_name)
    XFree (wm_name);

  return type;
}
uint32_t xitk_get_wm_type (xitk_t *xitk) {
  __xitk_t *_xitk;

  xitk_container (_xitk, xitk, x);
  return _xitk->wm_type;
}

int xitk_get_layer_level(void) {
  __xitk_t *xitk;
  int level = 10;

  xitk_container (xitk, gXitk, x);
  if ((xitk->wm_type & WM_TYPE_GNOME_COMP) || (xitk->wm_type & WM_TYPE_EWMH_COMP))
    level = 10;

  switch (xitk->wm_type & WM_TYPE_COMP_MASK) {
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

void xitk_set_layer_above(Window window) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if ((xitk->wm_type & WM_TYPE_GNOME_COMP) && !(xitk->wm_type & WM_TYPE_EWMH_COMP)) {
    long propvalue[1];

    propvalue[0] = xitk_get_layer_level();

    xitk->x.lock_display (&xitk->x);
    XChangeProperty (xitk->x.display, window, xitk->atoms[XITK_A_WIN_LAYER],
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		    1);
    xitk->x.unlock_display (&xitk->x);
    return;
  }


  if (xitk->wm_type & WM_TYPE_EWMH_COMP) {
    XEvent xev;

    memset(&xev, 0, sizeof xev);
    xitk->x.lock_display (&xitk->x);
    if(xitk->wm_type & WM_TYPE_KWIN) {
      xev.xclient.type         = ClientMessage;
      xev.xclient.display      = xitk->x.display;
      xev.xclient.window       = window;
      xev.xclient.message_type = xitk->atoms[XITK_A_NET_WM_STATE];
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = 1;
      xev.xclient.data.l[1]    = xitk->atoms[XITK_A_STAYS_ON_TOP];
      xev.xclient.data.l[2]    = 0l;
      xev.xclient.data.l[3]    = 0l;
      xev.xclient.data.l[4]    = 0l;

      XSendEvent (xitk->x.display, DefaultRootWindow (xitk->x.display), True, SubstructureRedirectMask, &xev);
    }
    else {
      xev.xclient.type         = ClientMessage;
      xev.xclient.serial       = 0;
      xev.xclient.send_event   = True;
      xev.xclient.display      = xitk->x.display;
      xev.xclient.window       = window;
      xev.xclient.message_type = xitk->atoms[XITK_A_NET_WM_STATE];
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = (long) 1;
      xev.xclient.data.l[1]    = (long) xitk->atoms[XITK_A_NET_WM_STATE_ABOVE];
      xev.xclient.data.l[2]    = (long) None;

      XSendEvent (xitk->x.display, DefaultRootWindow (xitk->x.display),
		 False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*) &xev);

    }
    xitk->x.unlock_display (&xitk->x);

    return;
  }

  switch (xitk->wm_type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_MOTIF:
  case WM_TYPE_LARSWM:
    break;

  case WM_TYPE_KWIN:
    xitk->x.lock_display (&xitk->x);
    XChangeProperty (xitk->x.display, window, xitk->atoms[XITK_A_WIN_LAYER],
		    XA_ATOM, 32, PropModeReplace, (unsigned char *)&xitk->atoms[XITK_A_STAYS_ON_TOP], 1);
    xitk->x.unlock_display (&xitk->x);
    break;

  case WM_TYPE_UNKNOWN:
  case WM_TYPE_WINDOWMAKER:
  case WM_TYPE_ICE:
  case WM_TYPE_E:
  case WM_TYPE_XFCE:
  case WM_TYPE_SAWFISH:
  case WM_TYPE_METACITY: /* Untested */
  case WM_TYPE_AFTERSTEP:
  case WM_TYPE_BLACKBOX:
  case WM_TYPE_DTWM:
    {
      long propvalue[1];

      propvalue[0] = xitk_get_layer_level();

      xitk->x.lock_display (&xitk->x);
      XChangeProperty (xitk->x.display, window, xitk->atoms[XITK_A_WIN_LAYER],
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		      1);
      xitk->x.unlock_display (&xitk->x);
    }
    break;
  }
}

void xitk_set_window_layer(Window window, int layer) {
  __xitk_t *xitk;
  XEvent xev;

  xitk_container (xitk, gXitk, x);
  if (((xitk->wm_type & WM_TYPE_COMP_MASK) == WM_TYPE_KWIN) ||
      ((xitk->wm_type & WM_TYPE_EWMH_COMP) && !(xitk->wm_type & WM_TYPE_GNOME_COMP))) {
    return;
  }

  memset(&xev, 0, sizeof xev);
  xev.type                 = ClientMessage;
  xev.xclient.type         = ClientMessage;
  xev.xclient.window       = window;
  xev.xclient.message_type = xitk->atoms[XITK_A_WIN_LAYER];
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (long) layer;
  xev.xclient.data.l[1]    = (long) 0;
  xev.xclient.data.l[2]    = (long) 0;
  xev.xclient.data.l[3]    = (long) 0;

  xitk->x.lock_display (&xitk->x);
  XSendEvent (xitk->x.display, RootWindow (xitk->x.display, (XDefaultScreen (xitk->x.display))),
	     False, SubstructureNotifyMask, (XEvent*) &xev);
  xitk->x.unlock_display (&xitk->x);
}

static void _set_ewmh_state (__xitk_t *xitk, Window window, Atom atom, int enable) {
  XEvent xev;

  if((window == None) || (atom == None))
    return;

  memset(&xev, 0, sizeof(xev));
  xev.xclient.type         = ClientMessage;
  xev.xclient.message_type = xitk->atoms[XITK_A_NET_WM_STATE];
  xev.xclient.display      = xitk->x.display;
  xev.xclient.window       = window;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (enable == 1) ? 1 : 0;
  xev.xclient.data.l[1]    = atom;
  xev.xclient.data.l[2]    = 0l;
  xev.xclient.data.l[3]    = 0l;
  xev.xclient.data.l[4]    = 0l;

  xitk->x.lock_display (&xitk->x);
  XSendEvent (xitk->x.display, DefaultRootWindow (xitk->x.display), True, SubstructureRedirectMask, &xev);
  xitk->x.unlock_display (&xitk->x);
}

void xitk_set_ewmh_fullscreen(Window window) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if (!(xitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;

  _set_ewmh_state (xitk, window, xitk->atoms[XITK_A_NET_WM_STATE_FULLSCREEN], 1);
  _set_ewmh_state (xitk, window, xitk->atoms[XITK_A_STAYS_ON_TOP], 1);
}

void xitk_unset_ewmh_fullscreen(Window window) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if (!(xitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;

  _set_ewmh_state (xitk, window, xitk->atoms[XITK_A_NET_WM_STATE_FULLSCREEN], 0);
  _set_ewmh_state (xitk, window, xitk->atoms[XITK_A_STAYS_ON_TOP], 0);
}

void xitk_set_wm_window_type (xitk_t *xitk, Window window, xitk_wm_window_type_t type) {
  __xitk_t *_xitk;

  if (!xitk)
    return;
  xitk_container (_xitk, xitk, x);
  if (_xitk->wm_type & WM_TYPE_EWMH_COMP) {
    if (type == WINDOW_TYPE_NONE) {
      xitk_lock_display (&_xitk->x);
      XDeleteProperty (_xitk->x.display, window, _xitk->atoms[XITK_A_WM_WINDOW_TYPE]);
      xitk_unlock_display (&_xitk->x);
    } else if ((type >= 1) && (type < WINDOW_TYPE_END)) {
      static const xitk_atom_t ai[WINDOW_TYPE_END] = {
        [WINDOW_TYPE_DESKTOP]       = XITK_A_WM_WINDOW_TYPE_DESKTOP,
        [WINDOW_TYPE_DOCK]          = XITK_A_WM_WINDOW_TYPE_DOCK,
        [WINDOW_TYPE_TOOLBAR]       = XITK_A_WM_WINDOW_TYPE_TOOLBAR,
        [WINDOW_TYPE_MENU]          = XITK_A_WM_WINDOW_TYPE_MENU,
        [WINDOW_TYPE_UTILITY]       = XITK_A_WM_WINDOW_TYPE_UTILITY,
        [WINDOW_TYPE_SPLASH]        = XITK_A_WM_WINDOW_TYPE_SPLASH,
        [WINDOW_TYPE_DIALOG]        = XITK_A_WM_WINDOW_TYPE_DIALOG,
        [WINDOW_TYPE_DROPDOWN_MENU] = XITK_A_WM_WINDOW_TYPE_DROPDOWN_MENU,
        [WINDOW_TYPE_POPUP_MENU]    = XITK_A_WM_WINDOW_TYPE_POPUP_MENU,
        [WINDOW_TYPE_TOOLTIP]       = XITK_A_WM_WINDOW_TYPE_TOOLTIP,
        [WINDOW_TYPE_NOTIFICATION]  = XITK_A_WM_WINDOW_TYPE_NOTIFICATION,
        [WINDOW_TYPE_COMBO]         = XITK_A_WM_WINDOW_TYPE_COMBO,
        [WINDOW_TYPE_DND]           = XITK_A_WM_WINDOW_TYPE_DND,
        [WINDOW_TYPE_NORMAL]        = XITK_A_WM_WINDOW_TYPE_NORMAL
      };

      xitk_lock_display (&_xitk->x);
      XChangeProperty (_xitk->x.display, window, _xitk->atoms[XITK_A_WM_WINDOW_TYPE],
        XA_ATOM, 32, PropModeReplace, (unsigned char *)&_xitk->atoms[ai[type]], 1);
      XRaiseWindow (_xitk->x.display, window);
      xitk_unlock_display (&_xitk->x);
    }
  }
}

void xitk_ungrab_pointer(void) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  xitk_lock_display (&xitk->x);
  XUngrabPointer(xitk->x.display, CurrentTime);
  xitk_unlock_display (&xitk->x);
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
  fx->w                     = NULL;
  fx->window                = None;
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
  fx->cbs                   = NULL;
  fx->xdnd                  = NULL;
  fx->key                   = 0;

  return fx;
}

/* nasty xitk_window_widget_list () helper. */
xitk_widget_list_t *xitk_widget_list_get (xitk_t *_xitk, xitk_window_t *xwin) {
  __xitk_t *xitk;
  __gfx_t *fx;
  Window win = xitk_window_get_window(xwin);

  xitk_container (xitk, _xitk, x);
  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->x.imlibdata);

  if (win != None) {
    MUTLOCK ();
    for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->wl.node.next; fx = (__gfx_t *)fx->wl.node.next)
      if (win == fx->window)
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
  fx->window = win;
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
void xitk_register_signal_handler(xitk_signal_callback_t sigcb, void *user_data) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  if (sigcb) {
    xitk->sig_callback = sigcb;
    xitk->sig_data     = user_data;
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

static xitk_register_key_t _xitk_register_event_handler (const char *name, xitk_window_t *xwin,
  xitk_widget_list_t *wl,
  xitk_be_event_handler_t *event_handler, const xitk_event_cbs_t *cbs, void *user_data,
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
  fx->wl.xwin = fx->w = xwin;
  fx->window = xitk_window_get_window (xwin);

  if (name)
    strlcpy (fx->name, name, sizeof (fx->name));
  else if (!fx->name[0])
    strcpy (fx->name, "NO_SET");

  if (fx->w && fx->w->bewin) {
    xitk_tagitem_t tags1[] = {
      {XITK_TAG_X, 0},
      {XITK_TAG_Y, 0},
      {XITK_TAG_WIDTH, 0},
      {XITK_TAG_HEIGHT, 0},
      {XITK_TAG_END, 0}
    };
    xitk_tagitem_t tags2[] = {
      {XITK_TAG_TITLE, (uintptr_t)fx->name},
      {XITK_TAG_END, 0}
    };
    fx->w->bewin->get_props (fx->w->bewin, tags1);
    fx->new_pos.x = tags1[0].value;
    fx->new_pos.y = tags1[1].value;
    fx->w->width = fx->width = tags1[2].value;
    fx->w->height = fx->height = tags1[3].value;
    fx->w->bewin->set_props (fx->w->bewin, tags2);
  }

  fx->user_data = user_data;
  fx->cbs = cbs;

  if (cbs && cbs->dnd_cb && (fx->window != None)) {
    fx->xdnd = xitk_dnd_new (xitk->x.display, xitk->x.lock_display == _xitk_lock_display, xitk->verbosity);
    xitk_dnd_make_window_aware (fx->xdnd, fx->window);
  }

  fx->event_handler = event_handler;
  fx->destructor = destructor;
  fx->destr_data = destr_data;

  if(fx->window) {
    xitk_lock_display (&xitk->x);
    XChangeProperty (xitk->x.display, fx->window, xitk->XA_XITK, XA_ATOM,
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

xitk_register_key_t xitk_be_register_event_handler (const char *name, xitk_window_t *xwin,
  xitk_widget_list_t *wl,
  xitk_be_event_handler_t *event_handler, void *eh_data,
  void (*destructor) (void *data), void *destr_data) {
  return _xitk_register_event_handler (name, xwin, wl, event_handler, NULL, eh_data, destructor, destr_data);
}

xitk_register_key_t xitk_register_event_handler_ext (const char *name, xitk_window_t *xwin,
  const xitk_event_cbs_t *cbs, void *user_data, xitk_widget_list_t *wl) {
  return _xitk_register_event_handler (name, xwin, wl, NULL, cbs, user_data, NULL, NULL);
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
    fx->cbs             = NULL;
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
    if (fx && (fx->window != None)) {
      xitk_tagitem_t tags[] = {
        {XITK_TAG_X, 0},
        {XITK_TAG_Y, 0},
        {XITK_TAG_WIDTH, 0},
        {XITK_TAG_HEIGHT, 0},
        {XITK_TAG_END, 0}
      };

      fx->w->bewin->get_props (fx->w->bewin, tags);
      winf->x      = fx->new_pos.x = tags[0].value;
      winf->y      = fx->new_pos.y = tags[1].value;
      winf->width  = fx->w->width = fx->width = tags[2].value;
      winf->height = fx->w->height = fx->height = tags[3].value;
      winf->xwin   = fx->w;
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
      w = fx->w;
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
              xitk_set_current_menu (NULL);
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
          xitk_set_current_menu (NULL);
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
            xitk_set_current_menu (NULL);
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
        /* Inform application about window movement. */
        if (fx->cbs && fx->cbs->pos_cb)
          fx->cbs->pos_cb (fx->user_data, fx->new_pos.x, fx->new_pos.y, fx->width, fx->height);
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

#if 0
  /*
  if (xitk->modalw != None) {
    while (fx->wl.node.next && (fx->window != xitk->modalw)) {

      if(fx->xevent_callback && (fx->window != None && event->type != KeyRelease))
	fx->xevent_callback(event, fx->user_data);

      fx = (__gfx_t *)fx->wl.node.next;
    }
  }
  */

#warning FIXME
    if (xitk->modalw != None) {

      /* Flush remain fxs */
      while (fx->wl.node.next && (fx->window != xitk->modalw)) {
	FXLOCK(fx);

        if (fx->cbs && fx->window != None && event->type != KeyRelease)
          xitk_x11_translate_xevent(event, fx->cbs, fx->user_data);

	fxd = fx;
	fx = (__gfx_t *)fx->wl.node.next;

	if(fxd->destroy)
	  __fx_destroy(fxd, 0);
	else
	  FXUNLOCK(fxd);
      }
      return;
    }
#endif


#if 0
void xitk_xevent_notify (XEvent *event) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
  /* protect walking through gfx list */
  MUTLOCK ();
  xitk_xevent_notify_impl (xitk, event);
  MUTUNLOCK ();
}
#endif

Display *xitk_x11_get_display(xitk_t *xitk) {
  return xitk->display;
}

static void xitk_dummy_un_lock_display (Display *display) {
  (void)display;
}

void (*xitk_x_lock_display) (Display *display) = xitk_dummy_un_lock_display;
void (*xitk_x_unlock_display) (Display *display) = xitk_dummy_un_lock_display;

/*
 * Imlib
 */

Visual *xitk_x11_get_visual(xitk_t *xitk) {
  return Imlib_get_visual(xitk->imlibdata);
}

int xitk_x11_get_depth(xitk_t *xitk) {
  return xitk->imlibdata->x.depth;
}

Colormap xitk_x11_get_colormap(xitk_t *xitk) {
  return Imlib_get_colormap(xitk->imlibdata);
}

void xitk_x11_select_visual(xitk_t *xitk, Visual *gui_visual) {
  __xitk_t *_xitk;
  int install_colormap;
  ImlibInitParams  imlib_init;

  xitk_container (_xitk, xitk, x);
  install_colormap = _xitk->install_colormap;

  /*
   * This routine isn't re-entrant. I cannot find a Imlib_cleanup either.
   * However, we have to reinitialize Imlib if we have to change the visual.
   * This will be a (small) memory leak.
   */
  imlib_init.flags = PARAMS_VISUALID;
  imlib_init.visualid = gui_visual->visualid;

  xitk_lock_display (xitk);

  if (install_colormap && (gui_visual->class & 1)) {
      /*
       * We're using a visual with changable colors / colormaps
       * (According to the comment in X11/X.h, an odd display class
       * has changable colors), and the user requested to install a
       * private colormap for xine.  Allocate a fresh colormap for
       * Imlib and Xine.
       */
      Colormap cm;
      cm = XCreateColormap(xitk->display,
                           RootWindow(xitk->display, DefaultScreen(xitk->display)),
                           gui_visual, AllocNone);

      imlib_init.cmap = cm;
      imlib_init.flags |= PARAMS_COLORMAP;
  }

  xitk->imlibdata = Imlib_init_with_params (xitk->display, &imlib_init);
  xitk_unlock_display (xitk);
  if (xitk->imlibdata == NULL) {
    fprintf(stderr, _("Unable to initialize Imlib\n"));
    exit(1);
  }

  xitk->imlibdata->x.x_lock_display = xitk_x_lock_display;
  xitk->imlibdata->x.x_unlock_display = xitk_x_unlock_display;
}

static void _init_imlib(__xitk_t *xitk, const char *prefered_visual, int install_colormap)
{
  Visual *visual = NULL;
  char *xrm_prefered_visual = NULL;

  if (!prefered_visual || !install_colormap)
    xitk_x11_xrm_parse("xine", NULL, NULL,
                       prefered_visual ? NULL : &xrm_prefered_visual,
                       install_colormap  ? NULL : &install_colormap);
  xitk->install_colormap = install_colormap;

  xitk_x11_find_visual(xitk->x.display, DefaultScreen(xitk->x.display),
                       prefered_visual ? prefered_visual : xrm_prefered_visual,
                       &visual, NULL);
  xitk_x11_select_visual(&xitk->x, visual);
  free(xrm_prefered_visual);
}

void xitk_sync(xitk_t *_xitk) {
  xitk_lock_display (_xitk);
  XSync (_xitk->display, False);
  xitk_unlock_display (_xitk);
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
  xitk->x.d = xitk->x.be->open_display (xitk->x.be, NULL, use_x_lock_display, use_synchronized_x);
  if (!xitk->x.d) {
    xitk->x.be->_delete (&xitk->x.be);
    free (xitk);
    return NULL;
  }
  xitk->x.display = (Display *)xitk->x.d->id;

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
    int s = DefaultScreen (xitk->x.display);
    xitk->display_width   = DisplayWidth (xitk->x.display, s);
    xitk->display_height  = DisplayHeight (xitk->x.display, s);
  }
  xitk->verbosity       = verbosity;
  xitk_dlist_init (&xitk->gfxs);
  xitk->key             = 0;
  xitk->sig_callback    = NULL;
  xitk->sig_data        = NULL;
  xitk->config          = xitk_config_init (&xitk->x);
  xitk->use_xshm        = xitk_config_get_num (xitk->config, XITK_SHM_ENABLE) ? xitk_check_xshm (xitk->x.display) : 0;
  xitk_x_error           = 0;
  xitk->x_error_handler = NULL;
  //xitk->modalw          = None;
  xitk->ignore_keys[0]  = XKeysymToKeycode (xitk->x.display, XK_Shift_L);
  xitk->ignore_keys[1]  = XKeysymToKeycode (xitk->x.display, XK_Control_L);
  xitk->qual            = 0;
  xitk->tips_timeout    = TIPS_TIMEOUT;
#if 0
  XGetInputFocus(display, &(xitk->parent.window), &(xitk->parent.focus));
#endif
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

  /* Check if SHM is working */
#ifdef HAVE_SHM
  if(xitk->use_xshm) {
    XImage             *xim;
    XShmSegmentInfo     shminfo;

    xim = XShmCreateImage(display,
			  (DefaultVisual(display, (DefaultScreen(display)))),
			  (DefaultDepth(display, (DefaultScreen(display)))),
			  ZPixmap, NULL, &shminfo, 10, 10);
    if(!xim)
      xitk->use_xshm = 0;
    else {
      shminfo.shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0777);
      if(shminfo.shmid < 0) {
	XDestroyImage(xim);
	xitk->use_xshm = 0;
      }
      else {
	shminfo.shmaddr = xim->data =  shmat(shminfo.shmid, 0, 0);
	if(shminfo.shmaddr == (char *) -1) {
	  XDestroyImage(xim);
	  xitk->use_xshm = 0;
	}
	else {
	  shminfo.readOnly = False;

	  xitk_x_error = 0;
	  xitk_install_x_error_handler();

	  XShmAttach(display, &shminfo);
	  XSync(display, False);
	  if(xitk_x_error)
	    xitk->use_xshm = 0;
	  else {
	    XShmDetach(display, &shminfo);
	    strlcat(buffer, "[XShm]", sizeof(buffer));
	  }

	  XDestroyImage(xim);
	  shmdt(shminfo.shmaddr);

	  xitk_uninstall_x_error_handler();
	  xitk_x_error = 0;
	}
	shmctl(shminfo.shmid, IPC_RMID, 0);
      }
    }
  }
#endif

#ifdef WITH_XFT
  strlcat(buffer, "[XFT]", sizeof(buffer));
#elif defined(WITH_XMB)
  strlcat(buffer, "[XMB]", sizeof(buffer));
#endif

  strlcat(buffer, " ]-", sizeof(buffer));

  if (verbosity >= 1)
    printf("%s", buffer);

  xitk->wm_type = xitk_check_wm (xitk, xitk->x.display);

  /* imit imlib */
  _init_imlib(xitk, prefered_visual, install_colormap);

  /* init font caching */
  xitk->x.font_cache = xitk_font_cache_init();

  xitk_cursors_init (&xitk->x);
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

  xitk_set_current_menu(NULL);

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

  Imlib_destroy(&xitk->x.imlibdata);

  xitk->x.d->close (&xitk->x.d);
  xitk->x.display = NULL;
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
  xitk_cursors_deinit (_xitk);
  xitk->running = 0;
#if 0
  if (xitk->parent.window != None) {
    int (*previous_error_handler)(Display *, XErrorEvent *);
    XSync (xitk->x.display, False);
    /* don't care about BadWindow when the focussed window is gone already */
    previous_error_handler = XSetErrorHandler(_x_ignoring_error_handler);
    XSetInputFocus (xitk->x.display, xitk->parent.window, xitk->parent.focus, CurrentTime);
    XSync (xitk->x.display, False);
    XSetErrorHandler(previous_error_handler);
  }
#endif
}

void xitk_set_xmb_enability(int value) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
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
  if (_xitk) {
    if (w)
      *w = _xitk->display_width;
    if (h)
      *h = _xitk->display_height;
  }
}

void xitk_set_tips_timeout(unsigned long timeout) {
  __xitk_t *xitk;

  xitk_container (xitk, gXitk, x);
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
