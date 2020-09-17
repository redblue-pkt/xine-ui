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
#include "_xitk.h"
#include "xitk_x11.h"
#include "tips.h"
#include "widget.h"
#include "menu.h"
#include "combo.h"

#define _XITK_CLIPBOARD_DEBUG

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

#define FXLOCK(_fx)       __gfx_lock(_fx)
#define FXUNLOCK(_fx)     __gfx_unlock(_fx)

typedef struct {
  int                               enabled;
  int                               offset_x;
  int                               offset_y;
} xitk_move_t;

typedef struct {
  xitk_dnode_t                node;

  xitk_window_t              *w;
  Window                      window;
  Atom                        XA_XITK;

  xitk_move_t                 move;

  struct {
    int                       x;
    int                       y;
  } old_pos;

  struct {
    int                       x;
    int                       y;
  } new_pos;

  int                         width;
  int                         height;

  xitk_hull_t                 expose;
    
  XEvent                     *old_event;

  xitk_widget_list_t         *widget_list;
  char                       *name;
  const xitk_event_cbs_t     *cbs;
  xitk_register_key_t         key;
  xitk_dnd_t                 *xdnd;
  void                       *user_data;

  pthread_mutex_t             mutex;
  int                         destroy;
  pthread_t                   owning_thread;

  void                      (*destructor)(void *userdata);
  void                       *destr_data;
} __gfx_t;


static int __gfx_lock(__gfx_t *fx)
{
  int retval = pthread_mutex_lock(&fx->mutex);
  if (0 == retval)
    fx->owning_thread = pthread_self();

  return retval;
}

static int __gfx_unlock(__gfx_t *fx)
{
  fx->owning_thread = (pthread_t)0;
  return pthread_mutex_unlock(&fx->mutex);
}

static int __gfx_safe_lock (__gfx_t *fx) {
  pthread_t self = pthread_self ();
  if (!pthread_mutex_trylock (&fx->mutex)) {
    fx->owning_thread = self;
    return 0;
  }
  if (pthread_equal (self, fx->owning_thread))
    return 1;
  if (!pthread_mutex_lock (&fx->mutex)) {
    fx->owning_thread = self;
    return 0;
  }
  return 2;
}

typedef struct {
  xitk_t                      x;

  xitk_x11_t                 *xitk_x11;

  int                         display_width;
  int                         display_height;
  int                         verbosity;
  xitk_dlist_t                wlists;
  xitk_dlist_t                gfxs;
  int                         use_xshm;
  int                         install_colormap;

  uint32_t                    wm_type;

  int                        (*x_error_handler)(Display *, XErrorEvent *);

  pthread_mutex_t             mutex;
  int                         running;
  xitk_register_key_t         key;
  xitk_config_t              *config;
  xitk_signal_callback_t      sig_callback;
  void                       *sig_data;

  //Window                      modalw;
  xitk_widget_t              *menu;
  
  struct timeval              keypress;

  KeyCode                     ignore_keys[2];

  unsigned long               tips_timeout;

#if 0
  /* not used  ? */
  struct {
    Window                    window;
    int                       focus;
  } parent;
#endif

  pid_t                       xitk_pid;
  
  struct {
    Atom                      XA_WIN_LAYER;
    Atom                      XA_STAYS_ON_TOP;

    Atom                      XA_NET_WM_STATE;
    Atom                      XA_NET_WM_STATE_ABOVE;
    Atom                      XA_NET_WM_STATE_FULLSCREEN;

    Atom                      XA_WM_WINDOW_TYPE;
    Atom                      XA_WM_WINDOW_TYPE_DESKTOP;
    Atom                      XA_WM_WINDOW_TYPE_DOCK;
    Atom                      XA_WM_WINDOW_TYPE_TOOLBAR;
    Atom                      XA_WM_WINDOW_TYPE_MENU;
    Atom                      XA_WM_WINDOW_TYPE_UTILITY;
    Atom                      XA_WM_WINDOW_TYPE_SPLASH;
    Atom                      XA_WM_WINDOW_TYPE_DIALOG;
    Atom                      XA_WM_WINDOW_TYPE_DROPDOWN_MENU;
    Atom                      XA_WM_WINDOW_TYPE_POPUP_MENU;
    Atom                      XA_WM_WINDOW_TYPE_TOOLTIP;
    Atom                      XA_WM_WINDOW_TYPE_NOTIFICATION;
    Atom                      XA_WM_WINDOW_TYPE_COMBO;
    Atom                      XA_WM_WINDOW_TYPE_DND;
    Atom                      XA_WM_WINDOW_TYPE_NORMAL;
  } atoms;

  struct {
    char                   *text;
    int                     text_len;
    xitk_widget_t          *widget_in;
    Window                  window_in;
    Window                  window_out;
    Atom                    req;
    Atom                    prop;
    Atom                    target;
    Atom                    dummy;
    Time                    own_time;
    union {
      Atom                  a[15];
      struct {
        Atom                null;
        Atom                atom;
        Atom                timestamp;
        Atom                integer;
        Atom                c_string;
        Atom                string;
        Atom                utf8_string;
        Atom                text;
        Atom                filename;
        Atom                net_address;
        Atom                window;
        Atom                clipboard;
        Atom                length;
        Atom                targets;
        Atom                multiple;
      }                     n;
    }                       atoms;
  }                         clipboard;
} __xitk_t;

/*
 *
 */

static void _xitk_lock_display (xitk_t *_xitk) {
  __xitk_t *xitk = (__xitk_t *)_xitk;
  XLockDisplay (xitk->x.display);
}
static void _xitk_unlock_display (xitk_t *_xitk) {
  __xitk_t *xitk = (__xitk_t *)_xitk;
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
  xitk = (__xitk_t *)wl->xitk;
  MUTLOCK ();
  for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->node.next; fx = (__gfx_t *)fx->node.next) {
    if (fx->widget_list == wl) {
      xitk_lock_display (&xitk->x);
      XSetInputFocus (xitk->x.display, fx->window, RevertToParent, CurrentTime);
      xitk_unlock_display (&xitk->x);
      break;
    }
  }
  MUTUNLOCK ();
}


void xitk_clipboard_unregister_widget (xitk_widget_t *w) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  if (xitk->clipboard.widget_in == w) {
    xitk->clipboard.widget_in = NULL;
    xitk->clipboard.window_in = None;
  }
}

void xitk_clipboard_unregister_window (Window win) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  if (xitk->clipboard.window_out == win)
    xitk->clipboard.window_out = None;
}

static void _xitk_clipboard_init (__xitk_t *xitk) {
  static const char *atom_names[] = {
    "NULL", "ATOM", "TIMESTAMP", "INTEGER", "C_STRING", "STRING", "UTF8_STRING", "TEXT",
    "FILENAME", "NET_ADDRESS", "WINDOW", "CLIPBOARD", "LENGTH", "TARGETS", "MULTIPLE"
  };

  xitk->clipboard.text = NULL;
  xitk->clipboard.text_len = 0;
  xitk->clipboard.widget_in = NULL;
  xitk->clipboard.window_in = None;
  xitk->clipboard.window_out = None;
  xitk->clipboard.own_time = CurrentTime;

  xitk_lock_display (&xitk->x);
  XInternAtoms (xitk->x.display,
    (char **)atom_names, sizeof (atom_names) / sizeof (atom_names[0]), True,
    xitk->clipboard.atoms.a);
  xitk->clipboard.dummy = XInternAtom (xitk->x.display, "_XITK_CLIP", False);
  xitk_unlock_display (&xitk->x);
#ifdef _XITK_CLIPBOARD_DEBUG
  printf ("xitk.window.clipboard: "
    "null=%d atom=%d timestamp=%d integer=%d c_string=%d string=%d utf8_string=%d text=%d "
    "filename=%d net_address=%d window=%d clipboard=%d length=%d targets=%d multiple=%d dummy=%d.\n",
    (int)xitk->clipboard.atoms.n.null,
    (int)xitk->clipboard.atoms.n.atom,
    (int)xitk->clipboard.atoms.n.timestamp,
    (int)xitk->clipboard.atoms.n.integer,
    (int)xitk->clipboard.atoms.n.c_string,
    (int)xitk->clipboard.atoms.n.string,
    (int)xitk->clipboard.atoms.n.utf8_string,
    (int)xitk->clipboard.atoms.n.text,
    (int)xitk->clipboard.atoms.n.filename,
    (int)xitk->clipboard.atoms.n.net_address,
    (int)xitk->clipboard.atoms.n.window,
    (int)xitk->clipboard.atoms.n.clipboard,
    (int)xitk->clipboard.atoms.n.length,
    (int)xitk->clipboard.atoms.n.targets,
    (int)xitk->clipboard.atoms.n.multiple,
    (int)xitk->clipboard.dummy);
#endif
}

static void _xitk_clipboard_deinit (__xitk_t *xitk) {
  free (xitk->clipboard.text);
  xitk->clipboard.text = NULL;
  xitk->clipboard.text_len = 0;
}

static int _xitk_clipboard_event (__xitk_t *xitk, XEvent *event) {
  if (event->type == PropertyNotify) {
    if (event->xproperty.atom == xitk->clipboard.dummy) {
      if (event->xproperty.window == xitk->clipboard.window_out) {
        /* set #2. */
#ifdef _XITK_CLIPBOARD_DEBUG
        printf ("xitk.clipboard: set #2: own clipboard @ time=%ld.\n", (long int)event->xproperty.time);
#endif
        xitk->clipboard.own_time = event->xproperty.time;
        xitk_lock_display (&xitk->x);
        XSetSelectionOwner (xitk->x.display, xitk->clipboard.atoms.n.clipboard, xitk->clipboard.window_out, xitk->clipboard.own_time);
        xitk_unlock_display (&xitk->x);
      } else if (event->xproperty.window == xitk->clipboard.window_in) {
        /* get #2. */
#ifdef _XITK_CLIPBOARD_DEBUG
        printf ("xitk.clipboard: get #2 time=%ld.\n", (long int)event->xproperty.time);
#endif
        if (event->xproperty.state == PropertyNewValue) {
          do {
            Atom actual_type;
            int actual_format = 0;
            unsigned long nitems, bytes_after;
            unsigned char *prop;

            xitk_lock_display (&xitk->x);
            XGetWindowProperty (xitk->x.display, xitk->clipboard.window_in,
              xitk->clipboard.dummy, 0, 0, False, xitk->clipboard.atoms.n.utf8_string,
              &actual_type, &actual_format, &nitems, &bytes_after, &prop);
            XFree (prop);
            if ((actual_type != xitk->clipboard.atoms.n.utf8_string) || (actual_format != 8))
              break;
            xitk->clipboard.text = malloc ((bytes_after + 1 + 3) & ~3);
            if (!xitk->clipboard.text)
              break;
            xitk->clipboard.text_len = bytes_after;
            XGetWindowProperty (xitk->x.display, xitk->clipboard.window_in,
              xitk->clipboard.dummy, 0, (xitk->clipboard.text_len + 3) >> 2,
              True, xitk->clipboard.atoms.n.utf8_string,
              &actual_type, &actual_format, &nitems, &bytes_after, &prop);
            if (!prop)
              break;
            memcpy (xitk->clipboard.text, prop, xitk->clipboard.text_len);
            XFree (prop);
            xitk->clipboard.text[xitk->clipboard.text_len] = 0;
          } while (0);
          xitk_unlock_display (&xitk->x);
          if (xitk->clipboard.widget_in && (xitk->clipboard.text_len > 0)) {
            widget_event_t  event;

            event.type = WIDGET_EVENT_CLIP_READY;
            (void)xitk->clipboard.widget_in->event (xitk->clipboard.widget_in, &event, NULL);
          }
        }
        xitk->clipboard.widget_in = NULL;
        xitk->clipboard.window_in = None;
      }
      return 1;
    }
  } else if (event->type == SelectionClear) {
    if ((event->xselectionclear.window == xitk->clipboard.window_out)
     && (event->xselectionclear.selection == xitk->clipboard.atoms.n.clipboard))
#ifdef _XITK_CLIPBOARD_DEBUG
      printf ("xitk.clipboard: lost.\n");
#endif
      xitk->clipboard.window_out = None;
      return 1;
  } else if (event->type == SelectionNotify) {
  } else if (event->type == SelectionRequest) {
    if ((event->xselectionrequest.owner == xitk->clipboard.window_out)
     && (event->xselectionrequest.selection == xitk->clipboard.atoms.n.clipboard)) {
#ifdef _XITK_CLIPBOARD_DEBUG
      char *tname, *pname;
      xitk_lock_display (&xitk->x);
      tname = XGetAtomName (xitk->x.display, event->xselectionrequest.target);
      pname = XGetAtomName (xitk->x.display, event->xselectionrequest.property);
      xitk_unlock_display (&xitk->x);
      printf ("xitk.clipboard: serve #1 requestor=%d target=%d (%s) property=%d (%s).\n",
        (int)event->xselectionrequest.requestor,
        (int)event->xselectionrequest.target, tname,
        (int)event->xselectionrequest.property, pname);
      XFree (pname);
      XFree (tname);
#endif
      xitk->clipboard.req    = event->xselectionrequest.requestor;
      xitk->clipboard.target = event->xselectionrequest.target;
      xitk->clipboard.prop   = event->xselectionrequest.property;
      if (xitk->clipboard.target == xitk->clipboard.atoms.n.targets) {
        int r;
        Atom atoms[] = {
          xitk->clipboard.atoms.n.string, xitk->clipboard.atoms.n.utf8_string,
          xitk->clipboard.atoms.n.length, xitk->clipboard.atoms.n.timestamp
        };
#ifdef _XITK_CLIPBOARD_DEBUG
        printf ("xitk.clipboard: serve #2: reporting %d (STRING) %d (UTF8_STRING) %d (LENGTH) %d (TIMESTAMP).\n",
          (int)xitk->clipboard.atoms.n.string, (int)xitk->clipboard.atoms.n.utf8_string,
          (int)xitk->clipboard.atoms.n.length, (int)xitk->clipboard.atoms.n.timestamp);
#endif
        xitk_lock_display (&xitk->x);
        r = XChangeProperty (xitk->x.display, xitk->clipboard.req, xitk->clipboard.prop,
          xitk->clipboard.atoms.n.atom, 32, PropModeReplace,
          (const unsigned char *)atoms, sizeof (atoms) / sizeof (atoms[0]));
        xitk_unlock_display (&xitk->x);
        if (r) {
#ifdef _XITK_CLIPBOARD_DEBUG
          printf ("xitk.clipboard: serve #2: reporting OK.\n");
#endif
        }
      } else if ((xitk->clipboard.target == xitk->clipboard.atoms.n.string)
        || (xitk->clipboard.target == xitk->clipboard.atoms.n.utf8_string)) {
        int r;
#ifdef _XITK_CLIPBOARD_DEBUG
        printf ("xitk.clipboard: serve #3: sending %d bytes.\n", xitk->clipboard.text_len + 1);
#endif
        xitk_lock_display (&xitk->x);
        r = XChangeProperty (xitk->x.display, xitk->clipboard.req, xitk->clipboard.prop,
          xitk->clipboard.atoms.n.utf8_string, 8, PropModeReplace,
          (const unsigned char *)xitk->clipboard.text, xitk->clipboard.text_len + 1);
        xitk_unlock_display (&xitk->x);
        if (r) {
#ifdef _XITK_CLIPBOARD_DEBUG
          printf ("xitk.clipboard: serve #3 len=%d OK.\n", xitk->clipboard.text_len);
#endif
        }
      }
      {
        XEvent event2;
        event2.xany.type = SelectionNotify;
        event2.xselection.requestor = event->xselectionrequest.requestor;
        event2.xselection.target = event->xselectionrequest.target;
        event2.xselection.property = event->xselectionrequest.property;
        event2.xselection.time = event->xselectionrequest.time;
        xitk_lock_display (&xitk->x);
        XSendEvent (xitk->x.display, event->xselectionrequest.requestor, False, 0, &event2);
        xitk_unlock_display (&xitk->x);
      }
      return 1;
    }
  }
  return 0;
}
  
int xitk_clipboard_set_text (xitk_widget_t *w, const char *text, int text_len) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  Window win = w->wl->win;

  free (xitk->clipboard.text);
  xitk->clipboard.text = NULL;
  xitk->clipboard.text_len = 0;
  if (!text || (text_len <= 0))
    return 0;
  xitk->clipboard.text = malloc (text_len + 1);
  if (!xitk->clipboard.text)
    return 0;
  memcpy (xitk->clipboard.text, text, text_len);
  xitk->clipboard.text[text_len] = 0;
  xitk->clipboard.text_len = text_len;

#ifdef _XITK_CLIPBOARD_DEBUG
  printf ("xitk.clipboard: set #1.\n");
#endif
  xitk->clipboard.window_out = win;
  xitk_lock_display (&xitk->x);
  /* set #1: HACK: get current server time. */
  XChangeProperty (xitk->x.display, win,
    xitk->clipboard.dummy, xitk->clipboard.atoms.n.utf8_string, 8, PropModeAppend, NULL, 0);
  xitk_unlock_display (&xitk->x);

  return text_len;
}

int xitk_clipboard_get_text (xitk_widget_t *w, char **text, int max_len) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  Window win = w->wl->win;
  int l;

  if (xitk->clipboard.widget_in != w) {

    if (xitk->clipboard.window_out == None) {
      free (xitk->clipboard.text);
      xitk->clipboard.text = NULL;
      xitk->clipboard.text_len = 0;
      xitk->clipboard.widget_in = w;
      xitk->clipboard.window_in = win;
      /* get #1. */
#ifdef _XITK_CLIPBOARD_DEBUG
      printf ("xitk.clipboard: get #1.\n");
#endif
      xitk_lock_display (&xitk->x);
      XConvertSelection (xitk->x.display, xitk->clipboard.atoms.n.clipboard,
        xitk->clipboard.atoms.n.utf8_string, xitk->clipboard.dummy, win, CurrentTime);
      xitk_unlock_display (&xitk->x);
      return -1;
    }
#ifdef _XITK_CLIPBOARD_DEBUG
    printf ("xitk.clipboard: get #1 from self: %d bytes.\n", xitk->clipboard.text_len);
#endif
  } else {
#ifdef _XITK_CLIPBOARD_DEBUG
    printf ("xitk.clipboard: get #3: %d bytes.\n", xitk->clipboard.text_len);
#endif
  }

  if (!xitk->clipboard.text || (xitk->clipboard.text_len <= 0))
    return 0;
  l = xitk->clipboard.text_len;
  if (l > max_len)
    l = max_len;
  if (l <= 0)
    return 0;
  if (!text)
    return l;
  if (*text)
    memcpy (*text, xitk->clipboard.text, l);
  else
    *text = xitk->clipboard.text;
  return l;
}

void widget_stop(void);

/*
void xitk_modal_window(Window w) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  xitk->modalw = w;
}
void xitk_unmodal_window(Window w) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
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
  __xitk_t *xitk = (__xitk_t *)gXitk;

  if (xitk->menu)
    xitk_destroy_widget (xitk->menu);
  xitk->menu = menu;
}

void xitk_unset_current_menu(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  xitk->menu = NULL;
}

int xitk_install_x_error_handler(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  if (xitk->x_error_handler == NULL) {
    xitk->x_error_handler = XSetErrorHandler (_x_error_handler);
    XSync (xitk->x.display, False);
    return 1;
  }
  return 0;
}

int xitk_uninstall_x_error_handler(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

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
  __xitk_t *xitk = (__xitk_t *)gXitk;
  pid_t cur_pid = getppid();

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
      xitk_dlist_clear (&xitk->wlists);
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

int xitk_is_use_xshm(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk->use_xshm;
}

static char *get_wm_name(Display *display, Window win, const char *atom_name) {
  char *wm_name = NULL;
  Atom  atom;

  /* Extract WM Name */	
  if((atom = XInternAtom(display, atom_name, True)) != None) {
    unsigned char   *prop_return = NULL;
    unsigned long    nitems_return;
    unsigned long    bytes_after_return;
    Atom             type_return, type_utf8;
    int              format_return;
    
    if((XGetWindowProperty(display, win, atom, 0, 4, False, XA_STRING,
			   &type_return, &format_return, &nitems_return,
			   &bytes_after_return, &prop_return)) == Success) {
      
      if(type_return != None) {
	if(type_return != XA_STRING) {
	  type_utf8 = XInternAtom(display, "UTF8_STRING", True);
	  if(type_utf8 != None) {
	    
	    if(type_return == type_utf8) {
	      
	      if(prop_return)
		XFree(prop_return);
	      prop_return = NULL;
	      
	      if((XGetWindowProperty(display, win, atom, 0, 4, False, type_utf8,
				     &type_return, &format_return, &nitems_return,
				     &bytes_after_return, &prop_return)) == Success) {
		
		if(format_return == 8)
		  wm_name = strdup((char *)prop_return);

	      }
	    }
	    else if(format_return == 8)
	      wm_name = strdup((char *)prop_return);

	  }
	}
	else
	  wm_name = strdup((char *)prop_return);

      }
      
      if(prop_return)
	XFree(prop_return);
    }
  }
  
  return wm_name;
}
static uint32_t xitk_check_wm(Display *display) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  Atom      atom;
  uint32_t  type = WM_TYPE_UNKNOWN;
  char     *wm_name = NULL;
  
  xitk->x.x_lock_display (display);

  if((atom = XInternAtom(display, "XFWM_FLAGS", True)) != None) {
    type |= WM_TYPE_XFCE;
  }
  else if((atom = XInternAtom(display, "_WINDOWMAKER_WM_PROTOCOLS", True)) != None) {
    type |= WM_TYPE_WINDOWMAKER;
  }
  else if((atom = XInternAtom(display, "_SAWMILL_TIMESTAMP", True)) != None) {
    type |= WM_TYPE_SAWFISH;
  }
  else if((atom = XInternAtom(display, "ENLIGHTENMENT_COMMS", True)) != None) {
    type |= WM_TYPE_E;
  }
  else if((atom = XInternAtom(display, "_METACITY_RESTART_MESSAGE", True)) != None) {
    type |= WM_TYPE_METACITY;
  }
  else if((atom = XInternAtom(display, "_AS_STYLE", True)) != None) {
    type |= WM_TYPE_AFTERSTEP;
  }
  else if((atom = XInternAtom(display, "_ICEWM_WINOPTHINT", True)) != None) {
    type |= WM_TYPE_ICE;
  }
  else if((atom = XInternAtom(display, "_BLACKBOX_HINTS", True)) != None) {
    type |= WM_TYPE_BLACKBOX;
  }
  else if((atom = XInternAtom(display, "LARSWM_EXIT", True)) != None) {
    type |= WM_TYPE_LARSWM;
  }
  else if(((atom = XInternAtom(display, "KWIN_RUNNING", True)) != None) &&
	  ((atom = XInternAtom(display, "_DT_SM_WINDOW_INFO", True)) != None)) {
    type |= WM_TYPE_KWIN;
  }
  else if((atom = XInternAtom(display, "DTWM_IS_RUNNING", True)) != None) {
    type |= WM_TYPE_DTWM;
  }

  xitk_install_x_error_handler();

  if((atom = XInternAtom(display, "_WIN_SUPPORTING_WM_CHECK", True)) != None) {
    unsigned char   *prop_return = NULL;
    unsigned long    nitems_return;
    unsigned long    bytes_after_return;
    Atom             type_return;
    int              format_return;
    Status           status;
    
    /* Check for Gnome Compliant WM */
    if((XGetWindowProperty(display, (XDefaultRootWindow(display)), atom, 0,
			   1, False, XA_CARDINAL,
			   &type_return, &format_return, &nitems_return,
			   &bytes_after_return, &prop_return)) == Success) {

      if((type_return != None) && (type_return == XA_CARDINAL) &&
	 ((format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0))) {
	unsigned char   *prop_return2 = NULL;
	Window           win_id;
	
	win_id = *(unsigned long *)prop_return;
	
	status = XGetWindowProperty(display, win_id, atom, 0,
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

	if((wm_name = get_wm_name(display, win_id, "_NET_WM_NAME")) == NULL) {
	  /*
	   * Enlightenment is Gnome compliant, but don't set 
	   * the _NET_WM_NAME atom property 
	   */
	  wm_name = get_wm_name(display, (XDefaultRootWindow(display)), "_WIN_WM_NAME");
	}
      }
      
      if(prop_return)
	XFree(prop_return);
    }
  }
  
  /* Check for Extended Window Manager Hints (EWMH) Compliant */
  if(((atom = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", True)) != None) && 
     (XInternAtom(display, "_NET_WORKAREA", True) != None)) {
    unsigned char   *prop_return = NULL;
    unsigned long    nitems_return;
    unsigned long    bytes_after_return;
    Atom             type_return;
    int              format_return;
    Status           status;
    
    if((XGetWindowProperty(display, (XDefaultRootWindow(display)), atom, 0, 1, False, XA_WINDOW,
			   &type_return, &format_return, &nitems_return,
			   &bytes_after_return, &prop_return)) == Success) {
      
      if((type_return != None) && (type_return == XA_WINDOW) && 
	 (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {
	unsigned char   *prop_return2 = NULL;
	Window           win_id;
	
	win_id = *(unsigned long *)prop_return;
	
	status = XGetWindowProperty(display, win_id, atom, 0,
				    1, False, XA_WINDOW,
				    &type_return, &format_return, &nitems_return,
				    &bytes_after_return, &prop_return2);
	
	if((status == Success) && (type_return != None) && (type_return == XA_WINDOW) &&
	   (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {
	  
	  if(win_id == *(unsigned long *)prop_return) {
	    free(wm_name);
	    wm_name = get_wm_name(display, win_id, "_NET_WM_NAME");
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

  xitk_uninstall_x_error_handler();

  if (type & WM_TYPE_EWMH_COMP) {
    xitk->atoms.XA_WIN_LAYER               = XInternAtom(display, "_NET_WM_STATE", False);
    xitk->atoms.XA_STAYS_ON_TOP            = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
    xitk->atoms.XA_NET_WM_STATE            = XInternAtom(display, "_NET_WM_STATE", False);
    xitk->atoms.XA_NET_WM_STATE_ABOVE      = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    xitk->atoms.XA_NET_WM_STATE_FULLSCREEN = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

    xitk->atoms.XA_WM_WINDOW_TYPE               = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_DESKTOP       = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_DOCK          = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_TOOLBAR       = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_MENU          = XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_UTILITY       = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_SPLASH        = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_DIALOG        = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_DROPDOWN_MENU = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_POPUP_MENU    = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_TOOLTIP       = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_NOTIFICATION  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_COMBO         = XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_DND           = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DND", False);
    xitk->atoms.XA_WM_WINDOW_TYPE_NORMAL        = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  }
  
  switch(type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_KWIN:
    if (xitk->atoms.XA_NET_WM_STATE == None)
      xitk->atoms.XA_NET_WM_STATE    = XInternAtom(display, "_NET_WM_STATE", False);
    if (xitk->atoms.XA_STAYS_ON_TOP == None)
      xitk->atoms.XA_STAYS_ON_TOP = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
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
    xitk->atoms.XA_WIN_LAYER = XInternAtom(display, "_WIN_LAYER", False);
    break;
  }

  xitk->x.x_unlock_display (display);
  
  if(xitk->verbosity) {
    printf("[ WM type: ");
    
    if(type & WM_TYPE_GNOME_COMP)
      printf("(GnomeCompliant) ");
    if(type & WM_TYPE_EWMH_COMP)
      printf("(EWMH) ");
    
    switch(type & WM_TYPE_COMP_MASK) {
    case WM_TYPE_UNKNOWN:
      printf("Unknown");
      break;
    case WM_TYPE_KWIN:
      printf("KWIN");
      break;
    case WM_TYPE_E:
      printf("Enlightenment");
      break;
    case WM_TYPE_ICE:
      printf("Ice");
      break;
    case WM_TYPE_WINDOWMAKER:
      printf("WindowMaker");
      break;
    case WM_TYPE_MOTIF:
      printf("Motif(like?)");
      break;
    case WM_TYPE_XFCE:
      printf("XFce");
      break;
    case WM_TYPE_SAWFISH:
      printf("Sawfish");
      break;
    case WM_TYPE_METACITY:
      printf("Metacity");
      break;
    case WM_TYPE_AFTERSTEP:
      printf("Afterstep");
      break;
    case WM_TYPE_BLACKBOX:
      printf("Blackbox");
      break;
    case WM_TYPE_LARSWM:
      printf("LarsWM");
      break;
    case WM_TYPE_DTWM:
      printf("dtwm");
      break;
    }
    
    if(wm_name)
      printf(" {%s}", wm_name);
    
    printf(" ]-\n");
  }
  
  if(wm_name)
    free(wm_name);
  
  return type;
}
uint32_t xitk_get_wm_type(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk->wm_type;
}

int xitk_get_layer_level(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  int level = 10;
  
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
  __xitk_t *xitk = (__xitk_t *)gXitk;

  if ((xitk->wm_type & WM_TYPE_GNOME_COMP) && !(xitk->wm_type & WM_TYPE_EWMH_COMP)) {
    long propvalue[1];
    
    propvalue[0] = xitk_get_layer_level();

    xitk->x.x_lock_display (xitk->x.display);
    XChangeProperty (xitk->x.display, window, xitk->atoms.XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		    1);
    xitk->x.x_unlock_display (xitk->x.display);
    return;
  }
  

  if (xitk->wm_type & WM_TYPE_EWMH_COMP) {
    XEvent xev;

    memset(&xev, 0, sizeof xev);
    xitk->x.x_lock_display (xitk->x.display);
    if(xitk->wm_type & WM_TYPE_KWIN) {
      xev.xclient.type         = ClientMessage;
      xev.xclient.display      = xitk->x.display;
      xev.xclient.window       = window;
      xev.xclient.message_type = xitk->atoms.XA_NET_WM_STATE;
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = 1;
      xev.xclient.data.l[1]    = xitk->atoms.XA_STAYS_ON_TOP;
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
      xev.xclient.message_type = xitk->atoms.XA_NET_WM_STATE;
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = (long) 1;
      xev.xclient.data.l[1]    = (long) xitk->atoms.XA_NET_WM_STATE_ABOVE;
      xev.xclient.data.l[2]    = (long) None;
      
      XSendEvent (xitk->x.display, DefaultRootWindow (xitk->x.display), 
		 False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*) &xev);
      
    }
    xitk->x.x_unlock_display (xitk->x.display);
    
    return;
  }
  
  switch (xitk->wm_type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_MOTIF:
  case WM_TYPE_LARSWM:
    break;
    
  case WM_TYPE_KWIN:
    xitk->x.x_lock_display (xitk->x.display);
    XChangeProperty (xitk->x.display, window, xitk->atoms.XA_WIN_LAYER,
		    XA_ATOM, 32, PropModeReplace, (unsigned char *)&xitk->atoms.XA_STAYS_ON_TOP, 1);
    xitk->x.x_unlock_display (xitk->x.display);
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
      
      xitk->x.x_lock_display (xitk->x.display);
      XChangeProperty (xitk->x.display, window, xitk->atoms.XA_WIN_LAYER,
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		      1);
      xitk->x.x_unlock_display (xitk->x.display);
    }
    break;
  }
}

void xitk_set_window_layer(Window window, int layer) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  XEvent xev;

  if (((xitk->wm_type & WM_TYPE_COMP_MASK) == WM_TYPE_KWIN) ||
      ((xitk->wm_type & WM_TYPE_EWMH_COMP) && !(xitk->wm_type & WM_TYPE_GNOME_COMP))) {
    return;
  }

  memset(&xev, 0, sizeof xev);
  xev.type                 = ClientMessage;
  xev.xclient.type         = ClientMessage;
  xev.xclient.window       = window;
  xev.xclient.message_type = xitk->atoms.XA_WIN_LAYER;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (long) layer;
  xev.xclient.data.l[1]    = (long) 0;
  xev.xclient.data.l[2]    = (long) 0;
  xev.xclient.data.l[3]    = (long) 0;

  xitk->x.x_lock_display (xitk->x.display);
  XSendEvent (xitk->x.display, RootWindow (xitk->x.display, (XDefaultScreen (xitk->x.display))), 
	     False, SubstructureNotifyMask, (XEvent*) &xev);
  xitk->x.x_unlock_display (xitk->x.display);
}

static void _set_ewmh_state(Window window, Atom atom, int enable) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  XEvent xev;
  
  if((window == None) || (atom == None))
    return;

  memset(&xev, 0, sizeof(xev));
  xev.xclient.type         = ClientMessage;
  xev.xclient.message_type = xitk->atoms.XA_NET_WM_STATE;
  xev.xclient.display      = xitk->x.display;
  xev.xclient.window       = window;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (enable == 1) ? 1 : 0;
  xev.xclient.data.l[1]    = atom;
  xev.xclient.data.l[2]    = 0l;
  xev.xclient.data.l[3]    = 0l;
  xev.xclient.data.l[4]    = 0l;

  xitk->x.x_lock_display (xitk->x.display);
  XSendEvent (xitk->x.display, DefaultRootWindow (xitk->x.display), True, SubstructureRedirectMask, &xev);
  xitk->x.x_unlock_display (xitk->x.display);
}

void xitk_set_ewmh_fullscreen(Window window) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  
  if (!(xitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;
  
  _set_ewmh_state (window, xitk->atoms.XA_NET_WM_STATE_FULLSCREEN, 1);
  _set_ewmh_state (window, xitk->atoms.XA_STAYS_ON_TOP, 1);
}

void xitk_unset_ewmh_fullscreen(Window window) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  
  if (!(xitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;
  
  _set_ewmh_state (window, xitk->atoms.XA_NET_WM_STATE_FULLSCREEN, 0);
  _set_ewmh_state (window, xitk->atoms.XA_STAYS_ON_TOP, 0);
}

static void _set_wm_window_type(Window window, xitk_wm_window_type_t type, int value) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  if (window && (xitk->wm_type & WM_TYPE_EWMH_COMP)) {
    Atom *atom = NULL;
    
    switch(type) {
    case WINDOW_TYPE_DESKTOP:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_DESKTOP;
      break;
    case WINDOW_TYPE_DOCK:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_DOCK;
      break;
    case WINDOW_TYPE_TOOLBAR:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_TOOLBAR;
      break;
    case WINDOW_TYPE_MENU:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_MENU;
      break;
    case WINDOW_TYPE_UTILITY:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_UTILITY;
      break;
    case WINDOW_TYPE_SPLASH:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_SPLASH;
      break;
    case WINDOW_TYPE_DIALOG:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_DIALOG;
      break;
    case WINDOW_TYPE_DROPDOWN_MENU:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_DROPDOWN_MENU;
      break;
    case WINDOW_TYPE_POPUP_MENU:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_POPUP_MENU;
      break;
    case WINDOW_TYPE_TOOLTIP:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_TOOLTIP;
      break;
    case WINDOW_TYPE_NOTIFICATION:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_NOTIFICATION;
      break;
    case WINDOW_TYPE_COMBO:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_COMBO;
      break;
    case WINDOW_TYPE_DND:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_DND;
      break;
    case WINDOW_TYPE_NORMAL:
      atom = &xitk->atoms.XA_WM_WINDOW_TYPE_NORMAL;
      break;
    }
    
    if(atom) {
      xitk_lock_display (&xitk->x);
      XChangeProperty (xitk->x.display, window, xitk->atoms.XA_WM_WINDOW_TYPE, XA_ATOM, 32, PropModeReplace, (unsigned char *)atom, 1);
      XRaiseWindow (xitk->x.display, window);
      xitk_unlock_display (&xitk->x);
    }
  }
}

void xitk_unset_wm_window_type(Window window, xitk_wm_window_type_t type) {
  _set_wm_window_type(window, type, 0);
}

void xitk_set_wm_window_type(Window window, xitk_wm_window_type_t type) {
  _set_wm_window_type(window, type, 1);
}

void xitk_ungrab_pointer(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  xitk_lock_display (&xitk->x);
  XUngrabPointer(xitk->x.display, CurrentTime);
  xitk_unlock_display (&xitk->x);
}

/*
 * Create a new widget_list, store the pointer in private
 * list of xitk_widget_list_t, then return the widget_list pointer.
 */
xitk_widget_list_t *xitk_widget_list_new (xitk_t *_xitk) {
  __xitk_t *xitk = (__xitk_t *)_xitk;
  xitk_widget_list_t *l;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->x.imlibdata);

  l = (xitk_widget_list_t *)xitk_xmalloc (sizeof (xitk_widget_list_t));
  if (!l)
    return NULL;

  l->xitk               = &xitk->x;
  xitk_dlist_init (&l->list);
  l->win                = None;
  l->gc                 = NULL;
  l->origin_gc          = NULL;
  l->temp_gc            = NULL;
  l->shared_images      = NULL;
  l->widget_focused     = NULL;
  l->widget_under_mouse = NULL;
  l->widget_pressed     = NULL;
  l->destroy            = 0;
  l->imlibdata          = xitk->x.imlibdata;

  MUTLOCK();
  xitk_dlist_add_tail (&xitk->wlists, &l->node);
  MUTUNLOCK();

#ifdef XITK_DEBUG
  printf  ("xitk: new wl @ %p.\n", (void *)l);
#endif
  return l;
}

/*
 * Register a callback function called when a signal happen.
 */
void xitk_register_signal_handler(xitk_signal_callback_t sigcb, void *user_data) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  if (sigcb) {
    xitk->sig_callback = sigcb;
    xitk->sig_data     = user_data;
  }
}

static void _xitk_reset_hull (xitk_hull_t *hull) {
  hull->x1 = 0x7fffffff;
  hull->x2 = 0;
  hull->y1 = 0x7fffffff;
  hull->y2 = 0;
}

/*
 * Register a window, with his own event handler, callback
 * for DND events, and widget list.
 */

xitk_register_key_t xitk_register_event_handler_ext(const char *name, xitk_window_t *w,
                                                    const xitk_event_cbs_t *cbs, void *user_data,
                                                    xitk_widget_list_t *wl) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t   *fx;

  ABORT_IF_NULL(w);

  //  printf("%s()\n", __FUNCTION__);

  fx = (__gfx_t *) xitk_xmalloc(sizeof(__gfx_t));
  if (!fx)
    return -1;

  fx->name = name ? strdup(name) : strdup("NO_SET");

  fx->w         = w;
  fx->window    = w ? xitk_window_get_window(w) : None;
  fx->new_pos.x = 0;
  fx->new_pos.y = 0;
  fx->width     = 0;
  fx->height    = 0;

  _xitk_reset_hull (&fx->expose);

  fx->user_data = user_data;
  fx->destructor = NULL;
  fx->destr_data = NULL;
  pthread_mutex_init(&fx->mutex, NULL);
  fx->owning_thread = (pthread_t)0;
  
  if (fx->window != None) {
    XWindowAttributes wattr;
    Status            err;
    
    xitk_lock_display (&xitk->x);
    err = XGetWindowAttributes (xitk->x.display, fx->window, &wattr);
    xitk_unlock_display (&xitk->x);
    
    if(err != BadDrawable && err != BadWindow) {
      Window c;
      
      xitk_lock_display (&xitk->x);
      XTranslateCoordinates (xitk->x.display, fx->window, wattr.root, 
			    0, 0, &(fx->new_pos.x), &(fx->new_pos.y), &c);
      xitk_unlock_display (&xitk->x);
      
      /*
	fx->new_pos.x = wattr.x;
	fx->new_pos.y = wattr.y;
      */
      fx->width     = wattr.width;
      fx->height    = wattr.height;
    }
  }

  fx->cbs = cbs;

  if(wl)
    fx->widget_list = wl;
  else
    fx->widget_list = NULL;

  if (cbs && cbs->dnd_cb && (fx->window != None)) {
    fx->xdnd = xitk_dnd_new (&xitk->x);
    if (fx->xdnd && xitk_make_window_dnd_aware(fx->xdnd, fx->window))
      xitk_set_dnd_callback(fx->xdnd, cbs->dnd_cb);
  }
  else
    fx->xdnd = NULL;

  if(fx->window) {

    xitk_lock_display (&xitk->x);
    fx->XA_XITK = XInternAtom (xitk->x.display, "_XITK_EVENT", False);
    XChangeProperty (xitk->x.display, fx->window, fx->XA_XITK, XA_ATOM,
		     32, PropModeAppend, (unsigned char *)&XITK_VERSION, 1);
    xitk_unlock_display (&xitk->x);

  }
  else
    fx->XA_XITK = None;

  MUTLOCK();
  fx->key = ++xitk->key;
  xitk_dlist_add_tail (&xitk->gfxs, &fx->node);
  MUTUNLOCK();

#ifdef XITK_DEBUG
  printf  ("xitk: new fx #%d \"%s\" @ %p.\n", fx->key, fx->name, (void *)fx);
#endif
  return fx->key;
}

static __gfx_t *__fx_from_key (__xitk_t *xitk, xitk_register_key_t key) {
  /* fx->key is set uniquely in xitk_register_event_handler, then never changed.
   * MUTLOCK () is enough. */
  __gfx_t *fx;
  for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->node.next; fx = (__gfx_t *)fx->node.next) {
    if (fx->key == key)
      return fx;
  }
  return NULL;
}

void xitk_register_eh_destructor (xitk_register_key_t key,
  void (*destructor)(void *userdata), void *destr_data) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t *fx;

  MUTLOCK ();
  fx = __fx_from_key (xitk, key);
  if (fx) {
    FXLOCK (fx);
    fx->destructor = destructor;
    fx->destr_data = destr_data;
    FXUNLOCK (fx);
  }
  MUTUNLOCK ();
}

static void _widget_list_destroy(xitk_widget_list_t **p) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  xitk_widget_list_t *wl = *p;

  if (!wl)
    return;

  *p = NULL;

  if (wl->temp_gc || wl->gc) {
    xitk_lock_display (&xitk->x);
    if (wl->temp_gc)
      XFreeGC (xitk->x.display, wl->temp_gc);
    if (wl->gc)
      XFreeGC (xitk->x.display, wl->gc);
    xitk_unlock_display (&xitk->x);
    wl->gc = NULL;
    wl->temp_gc = NULL;
  }
  xitk_shared_image_list_delete (wl);
  xitk_dnode_remove (&wl->node);
  xitk_dlist_clear (&wl->list);
  free(wl);
}

static void __fx_destroy(__gfx_t *fx, int locked) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  void (*destructor)(void *userdata);
  void *destr_data;

  if(!fx)
    return;
  
  if(!locked)
    MUTLOCK();
  
  if(fx->xdnd) {
    xitk_unset_dnd_callback(fx->xdnd);
    free(fx->xdnd);
  }

  if (fx->widget_list && fx->widget_list->destroy) {
    _widget_list_destroy(&fx->widget_list);
#ifdef XITK_DEBUG
    printf  ("xitk: deferredly killed wl @ %p.\n", (void *)fx->widget_list);
#endif
  }
  
  fx->cbs             = NULL;
  fx->user_data       = NULL;

  destructor = fx->destructor;
  destr_data = fx->destr_data;
  fx->destructor = NULL;
  fx->destr_data = NULL;

  free(fx->name);

  xitk_dnode_remove (&fx->node);
  
  FXUNLOCK(fx);
  pthread_mutex_destroy(&fx->mutex);

  free(fx);
  
  if(!locked)
    MUTUNLOCK();

  if (destructor)
    destructor (destr_data);

#ifdef XITK_DEBUG
  printf  ("xitk: killed fx @ %p.\n", (void *)fx);
#endif
}

/*
 * Remove from the list the window/event_handler
 * specified by the key.
 */
void xitk_unregister_event_handler(xitk_register_key_t *key) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t  *fx;

  //  printf("%s()\n", __FUNCTION__);

  MUTLOCK ();
  fx = __fx_from_key (xitk, *key);
  if (fx) {
    *key = 0;
    if (__gfx_safe_lock (fx)) {
      /* We already hold this, we are inside a callback. */
      /* NOTE: After return from this, application may free () fx->user_data. */
      fx->cbs             = NULL;
      fx->user_data       = NULL;
      fx->destroy = 1;
    } else {
      __fx_destroy (fx, 1);
    }
  }
  MUTUNLOCK ();
}

void xitk_widget_list_defferred_destroy(xitk_widget_list_t *wl) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t  *fx;

  MUTLOCK();
  fx = (__gfx_t *)xitk->gfxs.head.next;
  while (fx->node.next) {
    if (fx->widget_list && fx->widget_list == wl) {
      if (fx->destroy) {
      /* there was pending destroy, widget list needed */
        fx->widget_list->destroy = 1;
        MUTUNLOCK();
        return;
      } else {
	fx->widget_list = NULL;
	break;
      }
    }

    fx = (__gfx_t *)fx->node.next;
  }

  _widget_list_destroy(&wl);

  MUTUNLOCK();

#ifdef XITK_DEBUG
  printf  ("xitk: killed wl @ %p.\n", (void *)wl);
#endif
}

/*
 * Copy window information matching with key in passed window_info_t struct.
 */
int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t  *fx;

  MUTLOCK();

  fx = __fx_from_key (xitk, key);
  if (fx) {
      Window c;
      
    int already_locked = __gfx_safe_lock (fx);

    if (fx->window != None) {
      winf->xwin = fx->w;
      
      if(fx->name)
	winf->name = strdup(fx->name);
      
      xitk_lock_display (&xitk->x);
      XTranslateCoordinates (xitk->x.display, fx->window, DefaultRootWindow (xitk->x.display), 
			    0, 0, &(fx->new_pos.x), &(fx->new_pos.y), &c);
      xitk_unlock_display (&xitk->x);
      
      
      winf->x      = fx->new_pos.x;
      winf->y      = fx->new_pos.y;
      winf->height = fx->height;
      winf->width  = fx->width;
      
      if (!already_locked)
        __gfx_unlock (fx);

      MUTUNLOCK();
      return 1;

    }
    if (!already_locked)
      __gfx_unlock (fx);
  }

  MUTUNLOCK();
  return 0;
}

xitk_window_t *xitk_get_window(xitk_register_key_t key) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  __gfx_t  *fx;
  xitk_window_t *w = NULL;

  MUTLOCK();

  fx = __fx_from_key (xitk, key);
  if (fx) {
    int already_locked = __gfx_safe_lock (fx);

    w = fx->w;

    if (!already_locked)
      __gfx_unlock (fx);
  }

  MUTUNLOCK();
  return w;
}

/*
 * Here events are handled. All widget are locally
 * handled, then if a event handler callback was passed
 * at register time, it will be called.
 */
static void xitk_xevent_notify_impl (__xitk_t *xitk, XEvent *event) {
  __gfx_t  *fx, *fxd;

  fx = (__gfx_t *)xitk->gfxs.head.next;
  if (!fx->node.next)
    return;

  if(event->type == KeyPress || event->type == KeyRelease) {

    /* Filter keys that dont't need to be handled by xine  */
    /* and could be used by our screen saver reset "ping". */
    /* So they will not kill tips and menus.               */

    size_t i;

    for (i = 0; i < sizeof (xitk->ignore_keys) / sizeof (xitk->ignore_keys[0]); ++i)
      if(event->xkey.keycode == xitk->ignore_keys[i])
	return;
  }
  
  FXLOCK(fx);

  /*
  if (xitk->modalw != None) {
    while (fx->node.next && (fx->window != xitk->modalw)) {
      
      if(fx->xevent_callback && (fx->window != None && event->type != KeyRelease))
	fx->xevent_callback(event, fx->user_data);
      
      fx = (__gfx_t *)fx->node.next;
    }
  }
  */

  while (fx->node.next) {
    int handled = 0;

    if(event->type == KeyRelease)
      gettimeofday (&xitk->keypress, 0);
    
    if(fx->window != None) {
      
      //printf("event %d\n", event->type);

      if(fx->window == event->xany.window) {

	switch(event->type) {

        case PropertyNotify:
        case SelectionClear:
        case SelectionRequest:
          _xitk_clipboard_event (xitk, event);
          break;

	case MappingNotify:
          xitk_lock_display (&xitk->x);
	  XRefreshKeyboardMapping((XMappingEvent *) event);
          xitk_unlock_display (&xitk->x);
	  break;

	case KeyPress: {
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
	  KeySym         mykey;
          uint8_t        kbuf[256];
	  int            modifier;
          xitk_widget_t *w;

          xitk_tips_hide_tips(xitk->x.tips);

          w = fx->widget_list && fx->widget_list->widget_focused ? fx->widget_list->widget_focused : NULL;

          xitk_x11_keyevent_2_string (xitk->xitk_x11, event, &mykey, &modifier, (char *)kbuf, sizeof (kbuf));
          handled = xitk_widget_key_event (w, (char *)kbuf, modifier);

          if (!handled) {
            if (kbuf[0] == ' ')
              kbuf[0] = XITK_CTRL_KEY_PREFIX, kbuf[1] = XITK_KEY_LASTCODE;
            if (kbuf[0] == XITK_CTRL_KEY_PREFIX) {
              if ((t[kbuf[1]] == 1) || (w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) && (t[kbuf[1]] & 34))) {
                if (fx->widget_list) {
                  handled = 1;
                  xitk_set_focus_to_next_widget (fx->widget_list, (modifier & MODIFIER_SHIFT), modifier);
                }
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
                if (fx->widget_list)
                  xitk_set_focus_to_next_widget (fx->widget_list, (t[kbuf[1]] & 8), modifier);
              }
            }
          }

	  if(!handled) {

            if (xitk->menu) {
              if (!(fx->widget_list && fx->widget_list->widget_focused
                && (fx->widget_list->widget_focused->type & WIDGET_GROUP_MENU)))
                xitk_set_current_menu (NULL);
	    }

            if (fx->cbs)
              xitk_x11_translate_xevent(event, fx->cbs, fx->user_data);
	  }
	  
	  if(fx->destroy)
	    __fx_destroy(fx, 0);
	  else
	    FXUNLOCK(fx);

	  return;
	}
	break;

	case Expose:
	  if (fx->widget_list) {
            int r;
            do {
              if (event->xexpose.x < fx->expose.x1)
                fx->expose.x1 = event->xexpose.x;
              if (event->xexpose.x + event->xexpose.width > fx->expose.x2)
                fx->expose.x2 = event->xexpose.x + event->xexpose.width;
              if (event->xexpose.y < fx->expose.y1)
                fx->expose.y1 = event->xexpose.y;
              if (event->xexpose.y + event->xexpose.height > fx->expose.y2)
                fx->expose.y2 = event->xexpose.y + event->xexpose.height;
              xitk_lock_display (&xitk->x);
              r = XCheckTypedWindowEvent (xitk->x.display, fx->window, Expose, event);
              xitk_unlock_display (&xitk->x);
            } while (r == True);
            if (event->xexpose.count == 0) {
#ifdef XITK_PAINT_DEBUG
              printf ("xitk.expose: x %d-%d, y %d-%d.\n", fx->expose.x1, fx->expose.x2, fx->expose.y1, fx->expose.y2);
#endif
              /* initial panel open yields this?? */
              if ((fx->expose.x1 | fx->expose.x2 | fx->expose.y1 | fx->expose.y2) == 0)
                fx->expose.x2 = fx->expose.y2 = 0x7fffffff;
              xitk_partial_paint_widget_list (fx->widget_list, &fx->expose);
              _xitk_reset_hull (&fx->expose);
            }
	  }
	  break;
	  
	case MotionNotify: {
	  XWindowAttributes wattr;

          xitk_lock_display (&xitk->x);
          while (XCheckMaskEvent (xitk->x.display, ButtonMotionMask, event) == True);
          xitk_unlock_display (&xitk->x);

	  fx->old_event = event;
	  if(fx->move.enabled) {

            if (fx->widget_list && fx->widget_list->widget_under_mouse
              && (fx->widget_list->widget_under_mouse->type & WIDGET_GROUP_MENU)) {
	      xitk_widget_t *menu = xitk_menu_get_menu(fx->widget_list->widget_focused);

	      if(xitk_menu_show_sub_branchs(menu))
		xitk_menu_destroy_sub_branchs(menu);

	    }

	    fx->old_pos.x = fx->new_pos.x;
	    fx->old_pos.y = fx->new_pos.y;

	    fx->new_pos.x = (event->xmotion.x_root) 
	      + (event->xmotion.x_root - fx->old_event->xmotion.x_root) 
	      - fx->move.offset_x;
	    fx->new_pos.y = (event->xmotion.y_root) 
	      + (event->xmotion.y_root - fx->old_event->xmotion.y_root) 
	      - fx->move.offset_y;
	    
            xitk_lock_display (&xitk->x);

            XMoveWindow (xitk->x.display, fx->window,
			fx->new_pos.x, fx->new_pos.y);
            XGetWindowAttributes (xitk->x.display, fx->window, &wattr);

            xitk_unlock_display (&xitk->x);

	  }
	  else {
	    if(fx->widget_list) {
	      xitk_motion_notify_widget_list (fx->widget_list,
					      event->xmotion.x, 
					      event->xmotion.y, event->xmotion.state);
	    }
	  }
	}
	  break;
	  
	case LeaveNotify:
	  if(!(fx->widget_list && fx->widget_list->widget_pressed &&
	       (fx->widget_list->widget_pressed->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER))
	    event->xcrossing.x = event->xcrossing.y = -1; /* Simulate moving out of any widget */
	    /* but leave the actual coords for an active slider, otherwise the slider may jump */
	  /* fall through */
	case EnterNotify:
	  if(fx->widget_list)
	    if(event->xcrossing.mode == NotifyNormal) /* Ptr. moved rel. to win., not (un)grab */
	      xitk_motion_notify_widget_list (fx->widget_list,
					      event->xcrossing.x,
					      event->xcrossing.y, event->xcrossing.state);
	  break;
	  
	case ButtonPress: {
	  XWindowAttributes   wattr;
	  Status              status;

          xitk_tips_hide_tips(xitk->x.tips);
	  
          xitk_lock_display (&xitk->x);
          status = XGetWindowAttributes (xitk->x.display, fx->window, &wattr);
	  /* 
	   * Give focus (and raise) to window after click
	   * if it's viewable (e.g. not iconified).
	   */
	  if((status != BadDrawable) && (status != BadWindow) 
	     && (wattr.map_state == IsViewable)) {
            XRaiseWindow (xitk->x.display, fx->window);
            XSetInputFocus (xitk->x.display, fx->window, RevertToParent, CurrentTime);
	  }
          xitk_unlock_display (&xitk->x);

          /* A click anywhere outside an open menu shall close it with no further action.
           * FIXME: this currently works only if the click goes into one of our windows. */
          if (xitk->menu) {
            if (!(fx->widget_list && fx->widget_list->widget_under_mouse
              && (fx->widget_list->widget_under_mouse->type & WIDGET_GROUP_MENU))) {
              xitk_set_current_menu (NULL);
              FXUNLOCK (fx);
              return;
            }
          }

	  if(fx->widget_list) {
            int modifier = 0;
            xitk_get_key_modifier(event, &modifier);

	    fx->move.enabled = !xitk_click_notify_widget_list (fx->widget_list, 
							       event->xbutton.x, 
							       event->xbutton.y, 
							       event->xbutton.button, 0,
                                                               modifier);
            if (event->xbutton.button != Button1)
              fx->move.enabled = 0;

            if ((event->xbutton.button == Button4) || (event->xbutton.button == Button5)) {
              if (fx->widget_list && fx->widget_list->widget_focused) {
                xitk_widget_t *w = fx->widget_list->widget_focused;
                const char sup[3] = {XITK_CTRL_KEY_PREFIX, XITK_MOUSE_WHEEL_UP, 0};
                const char sdown[3] = {XITK_CTRL_KEY_PREFIX, XITK_MOUSE_WHEEL_DOWN, 0};
                handled = xitk_widget_key_event (w, event->xbutton.button == Button4 ? sup : sdown, modifier);
              }
	    }

	    if(fx->move.enabled) {
	      XWindowAttributes wattr;
	      Status            err;

              xitk_lock_display (&xitk->x);
              err = XGetWindowAttributes (xitk->x.display, fx->window, &wattr);
              xitk_unlock_display (&xitk->x);

	      if(err != BadDrawable && err != BadWindow) {
		
		fx->old_pos.x = event->xmotion.x_root - event->xbutton.x;
		fx->old_pos.y = event->xmotion.y_root - event->xbutton.y;

	      }
	      
	      fx->move.offset_x = event->xbutton.x;
	      fx->move.offset_y = event->xbutton.y;

	    }
	  }
	}
	break;
	  
	case ButtonRelease:

          xitk_tips_hide_tips(xitk->x.tips);
	  
	  if(fx->move.enabled) {

	    fx->move.enabled = 0;
	    /* Inform application about window movement. */

            if (fx->cbs && fx->cbs->pos_cb)
              fx->cbs->pos_cb (fx->user_data,
                fx->new_pos.x, fx->new_pos.y, fx->width, fx->height);
	  }
	  else {
            int modifier = 0;
            xitk_get_key_modifier(event, &modifier);
	    if(fx->widget_list) {
	      xitk_click_notify_widget_list (fx->widget_list, 
					     event->xbutton.x, event->xbutton.y,
                                             event->xbutton.button, 1,
                                             modifier);
	    }
	  }
	  break;
	  
	case ConfigureNotify: {
	  XWindowAttributes wattr;
	  Status            err;

          if (fx->widget_list) {
            xitk_widget_t *w = (xitk_widget_t *)fx->widget_list->list.head.next;
            while (w->node.next) {
              if ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO)
                xitk_combo_update_pos (w);
              w = (xitk_widget_t *)w->node.next;
            }
          }

	  /* Inform application about window movement. */
          if (fx->cbs && fx->cbs->pos_cb) {

            xitk_lock_display (&xitk->x);
            err = XGetWindowAttributes (xitk->x.display, fx->window, &wattr);
            xitk_unlock_display (&xitk->x);

	    if(err != BadDrawable && err != BadWindow) {
	      fx->width = wattr.width;
	      fx->height = wattr.height;
	    }
            fx->cbs->pos_cb (fx->user_data,
              event->xconfigure.x, event->xconfigure.y, fx->width, fx->height);
	  }
	}
	break;

	case SelectionNotify:
          if (_xitk_clipboard_event (xitk, event))
            break;
          /* fall through */
	case ClientMessage:
	  if(fx->xdnd)
	    xitk_process_client_dnd_message(fx->xdnd, event);
	  break;
	}

        if (!handled && fx->cbs) {
          xitk_x11_translate_xevent(event, fx->cbs, fx->user_data);
        }
      }
    }
    
    fxd = fx;
    fx = (__gfx_t *)fx->node.next;

    if(fxd->destroy)
      __fx_destroy(fxd, 0);
    else
      FXUNLOCK(fxd);
    
#if 0
#warning FIXME
    if (xitk->modalw != None) {

      /* Flush remain fxs */
      while (fx->node.next && (fx->window != xitk->modalw)) {
	FXLOCK(fx);
	
        if (fx->cbs && fx->window != None && event->type != KeyRelease)
          xitk_x11_translate_xevent(event, fx->cbs, fx->user_data);
	
	fxd = fx;
	fx = (__gfx_t *)fx->node.next;

	if(fxd->destroy)
	  __fx_destroy(fxd, 0);
	else
	  FXUNLOCK(fxd);
      }
      return;
    }
#endif

    if (fx->node.next)
      FXLOCK(fx);
  }
}

#if 0
void xitk_xevent_notify (XEvent *event) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  /* protect walking through gfx list */
  MUTLOCK ();
  xitk_xevent_notify_impl (xitk, event);
  MUTUNLOCK ();
}
#endif

Display *xitk_x11_get_display(xitk_t *xitk) {
  return xitk->display;
}

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

  int install_colormap = ((__xitk_t *)xitk)->install_colormap;
  ImlibInitParams  imlib_init;

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

  xitk->imlibdata->x.x_lock_display = xitk->x_lock_display;
  xitk->imlibdata->x.x_unlock_display = xitk->x_unlock_display;
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

#ifdef YET_UNUSED
void xitk_sync(xitk_t *_xitk) {
  xitk_lock_display (_xitk);
  XSync (_xitk->display, False);
  xitk_unlock_display (_xitk);
}
#endif

/*
 * Initiatization of widget internals.
 */

static void xitk_dummy_un_lock_display (Display *display) {
  (void)display;
}

void (*xitk_x_lock_display) (Display *display) = xitk_dummy_un_lock_display;
void (*xitk_x_unlock_display) (Display *display) = xitk_dummy_un_lock_display;

xitk_t *xitk_init (const char *prefered_visual, int install_colormap,
                   int use_x_lock_display, int use_synchronized_x, int verbosity) {

  __xitk_t *xitk;
  char buffer[256];
  pthread_mutexattr_t attr;
  Display *display;

  if (use_x_lock_display) {
    /* Nasty (temporary) kludge. */
    xitk_x_lock_display = XLockDisplay;
    xitk_x_unlock_display = XUnlockDisplay;
  }

  display = xitk_x11_open_display(use_x_lock_display, use_synchronized_x, verbosity);

#ifdef ENABLE_NLS
  bindtextdomain("xitk", XITK_LOCALE);
#endif  

  xitk = xitk_xmalloc (sizeof (*xitk));
  gXitk = &xitk->x;
  if (!xitk)
    return NULL;

  xitk->xitk_pid = getppid ();

  xitk->x.x_lock_display   = xitk_x_lock_display;
  xitk->x.x_unlock_display = xitk_x_unlock_display;
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

  xitk->display_width   = DisplayWidth(display, DefaultScreen(display));
  xitk->display_height  = DisplayHeight(display, DefaultScreen(display));
  xitk->verbosity       = verbosity;
  xitk_dlist_init (&xitk->wlists);
  xitk_dlist_init (&xitk->gfxs);
  xitk->x.display       = display;
  xitk->key             = 0;
  xitk->sig_callback    = NULL;
  xitk->sig_data        = NULL;
  xitk->config          = xitk_config_init();
  xitk->use_xshm        = (xitk_config_get_shm_feature(xitk->config)) ? (xitk_check_xshm(display)) : 0;
  xitk_x_error           = 0;
  xitk->x_error_handler = NULL;
  //xitk->modalw          = None;
  xitk->ignore_keys[0]  = XKeysymToKeycode(display, XK_Shift_L);
  xitk->ignore_keys[1]  = XKeysymToKeycode(display, XK_Control_L);
  xitk->tips_timeout    = TIPS_TIMEOUT;
#if 0
  XGetInputFocus(display, &(xitk->parent.window), &(xitk->parent.focus));
#endif
  xitk->atoms.XA_WIN_LAYER = None;
  xitk->atoms.XA_STAYS_ON_TOP = None;

  xitk->atoms.XA_NET_WM_STATE = None;
  xitk->atoms.XA_NET_WM_STATE_ABOVE = None;
  xitk->atoms.XA_NET_WM_STATE_FULLSCREEN = None;

  xitk->atoms.XA_WM_WINDOW_TYPE = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_DESKTOP = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_DOCK = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_TOOLBAR = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_MENU = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_UTILITY = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_SPLASH = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_DIALOG = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_DROPDOWN_MENU = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_POPUP_MENU = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_TOOLTIP = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_NOTIFICATION = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_COMBO = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_DND = None;
  xitk->atoms.XA_WM_WINDOW_TYPE_NORMAL = None;

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

  if(verbosity)
    printf("%s", buffer);

  xitk->wm_type = xitk_check_wm(display);
  
  /* imit imlib */
  _init_imlib(xitk, prefered_visual, install_colormap);

  /* init font caching */
  xitk->x.font_cache = xitk_font_cache_init();

  xitk_cursors_init(display);
  xitk->x.tips = xitk_tips_new (display);

  return &xitk->x;
}

/*
 * Start widget event handling.
 * It will block till widget_stop() call
 */
void xitk_run (void (* start_cb)(void *data), void *start_data,
  void (* stop_cb)(void *data), void *stop_data) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

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
  
  xitk_lock_display (&xitk->x);
  XSync (xitk->x.display, True); /* Flushing the toilets */
  xitk_unlock_display (&xitk->x);

  /*
   * Force to repain the widget list if it exist
   */
  {
    __gfx_t *fx;

    MUTLOCK ();
    for (fx = (__gfx_t *)xitk->gfxs.head.next; fx->node.next; fx = (__gfx_t *)fx->node.next) {
      FXLOCK (fx);
   
      if ((fx->window != None) && fx->widget_list) {
        XEvent xexp;

        memset(&xexp, 0, sizeof xexp);
        xexp.xany.type          = Expose;
        xexp.xexpose.type       = Expose;
        xexp.xexpose.send_event = True;
        xexp.xexpose.display    = xitk->x.display;
        xexp.xexpose.window     = fx->window;
        xexp.xexpose.count      = 0;
      
        xitk_lock_display (&xitk->x);
        if (!XSendEvent (xitk->x.display, fx->window, False, ExposureMask, &xexp)) {
          XITK_WARNING("XSendEvent(display, 0x%x ...) failed.\n", (unsigned int) fx->window);
        }
        xitk_unlock_display (&xitk->x);
      }

      FXUNLOCK (fx);
    }
    MUTUNLOCK ();
  }

  /* We're ready to handle anything */
  if (start_cb)
    start_cb (start_data);

  {
    int xconnection;

    xitk_lock_display (&xitk->x);
    xconnection = ConnectionNumber (xitk->x.display);
    xitk_unlock_display (&xitk->x);

    /* Now, wait for a new xevent */
    while (xitk->running) {
      XEvent myevent;
      int have_events;
    
      xitk_lock_display (&xitk->x);
      have_events = XPending (xitk->x.display);

      if (have_events <= 0) {
        fd_set fdset;
        struct timeval tv;

        xitk_unlock_display (&xitk->x);
        FD_ZERO (&fdset);
        FD_SET (xconnection, &fdset);
        tv.tv_sec  = 0;
        tv.tv_usec = 33000;
        select (xconnection + 1, &fdset, 0, 0, &tv);
        continue;
      }

      XNextEvent (xitk->x.display, &myevent);
      xitk_unlock_display (&xitk->x);
      
      MUTLOCK ();
      xitk_xevent_notify_impl (xitk, &myevent);
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
    if (!fx->node.next)
      break;
    FXLOCK (fx);
    __fx_destroy(fx, 1);
  }
  xitk_dlist_clear (&xitk->gfxs);
  xitk_dlist_clear (&xitk->wlists);
  MUTUNLOCK ();

  /* destroy font caching */
  xitk_font_cache_destroy(&xitk->x.font_cache);

  xitk_config_deinit (xitk->config);
}

void xitk_free(xitk_t **p) {

  __xitk_t *xitk = (__xitk_t*) *p;

  if (!xitk)
    return;
  *p = NULL;

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

  XCloseDisplay(xitk->x.display);
  xitk->x.display = NULL;

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
void xitk_stop(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;
  xitk_tips_delete (&xitk->x.tips);
  xitk_cursors_deinit (xitk->x.display);
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
 
const char *xitk_get_system_font(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_system_font (xitk->config);
}

const char *xitk_get_default_font(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_default_font (xitk->config);
}

int xitk_get_xmb_enability(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_xmb_enability (xitk->config);
}

void xitk_set_xmb_enability(int value) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  xitk_config_set_xmb_enability (xitk->config, value);
}

int xitk_get_black_color(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_black_color (xitk->config);
}

int xitk_get_white_color(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_white_color (xitk->config);
}

int xitk_get_background_color(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_background_color (xitk->config);
}

int xitk_get_focus_color(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_focus_color (xitk->config);
}

int xitk_get_select_color(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_select_color (xitk->config);
}

unsigned long xitk_get_timer_label_animation(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_timer_label_animation (xitk->config);
}

unsigned long xitk_get_warning_foreground(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_warning_foreground (xitk->config);
}

unsigned long xitk_get_warning_background(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_warning_background (xitk->config);
}

int xitk_is_dbl_click (xitk_t *xitk, const struct timeval *t1, const struct timeval *t2) {
  __xitk_t *_xitk = (__xitk_t *)xitk;
  struct timeval td;

  td.tv_usec = xitk_config_get_timer_dbl_click (_xitk->config);
  td.tv_sec  = t2->tv_sec - t1->tv_sec;
  if (td.tv_sec > td.tv_usec / 1000 + 1)
    return 0;
  return td.tv_sec * 1000 + (t2->tv_usec - t1->tv_usec) / 1000 < td.tv_usec;
}

int xitk_get_barstyle_feature(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_barstyle_feature (xitk->config);
}

int xitk_get_checkstyle_feature(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_checkstyle_feature (xitk->config);
}

int xitk_get_cursors_feature(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_cursors_feature (xitk->config);
}

int xitk_get_menu_shortcuts_enability(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk_config_get_menu_shortcuts_enability (xitk->config);
}

int xitk_get_display_width(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk->display_width;
}

int xitk_get_display_height(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk->display_height;
}

unsigned long xitk_get_tips_timeout(void) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

  return xitk->tips_timeout;
}

void xitk_set_tips_timeout(unsigned long timeout) {
  __xitk_t *xitk = (__xitk_t *)gXitk;

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
  __xitk_t *_xitk = (__xitk_t *)xitk;
  struct timeval tm;
  
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
  static const struct {
    char str[7];
    uint8_t value;
  } bools[] = {
    { "1",     1 }, { "true",  1 }, { "yes",   1 }, { "on",    1 },
    { "0",     0 }, { "false", 0 }, { "no",    0 }, { "off",   0 }
  };
  size_t i;
  
  ABORT_IF_NULL(val);

  for(i = 0; i < sizeof(bools)/sizeof(bools[0]); i++) {
    if(!(strcasecmp(bools[i].str, val)))
      return bools[i].value;
  }

  return 0;
}

