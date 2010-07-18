/* 
 * Copyright (C) 2000-2008 the xine project
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

extern char **environ;
#undef TRACE_LOCKS

#ifdef TRACE_LOCKS
static int ml = 0;
#define MUTLOCK()                                                             \
  do {                                                                        \
    int i;                                                                    \
    ml++;                                                                     \
    for(i=0; i<ml; i++) printf(".");                                          \
    printf("LOCK\n");                                                         \
    pthread_mutex_lock(&gXitk->mutex);                                        \
  } while (0)

#define MUTUNLOCK()                                                           \
  do {                                                                           \
    int i;                                                                    \
    for(i=0; i<ml; i++) printf(".");                                          \
    printf("UNLOCK\n");                                                       \
    ml--;                                                                     \
    pthread_mutex_unlock(&gXitk->mutex);                                      \
  } while (0)

#else
#define MUTLOCK()   pthread_mutex_lock(&gXitk->mutex)
#define MUTUNLOCK() pthread_mutex_unlock(&gXitk->mutex)
#endif

#define FXLOCK(_fx) pthread_mutex_lock(&_fx->mutex)
#define FXUNLOCK(_fx) pthread_mutex_unlock(&_fx->mutex)


typedef struct {
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

  XEvent                     *old_event;

  xitk_widget_list_t         *widget_list;
  char                       *name;
  widget_event_callback_t     xevent_callback;
  widget_newpos_callback_t    newpos_callback;
  xitk_register_key_t         key;
  xitk_dnd_t                 *xdnd;
  void                       *user_data;

  pthread_mutex_t             mutex;
  int                         destroy;
} __gfx_t;

typedef struct {
  Display                    *display;
  XColor                      black;
  int                         display_width;
  int                         display_height;
  int                         verbosity;
  xitk_list_t                *list;
  xitk_list_t                *gfx;
  int                         use_xshm;

  uint32_t                    wm_type;

  int                        (*x_error_handler)(Display *, XErrorEvent *);

  pthread_mutex_t             mutex;
  int                         running;
  xitk_register_key_t         key;
  xitk_config_t              *config;
  xitk_signal_callback_t      sig_callback;
  void                       *sig_data;

  Window                      modalw;
  xitk_widget_t              *menu;
  
  struct timeval              keypress;

  KeyCode                     ignore_keys[2];

  pthread_t                  *tips_thread;
  unsigned long               tips_timeout;

  struct {
    Window                    window;
    int                       focus;
  } parent;
  
} __xitk_t;

static __xitk_t    *gXitk;
static pid_t        xitk_pid;
static Atom XA_WIN_LAYER = None, XA_STAYS_ON_TOP = None;
static Atom XA_NET_WM_STATE = None, XA_NET_WM_STATE_ABOVE = None;
static Atom XA_NET_WM_STATE_FULLSCREEN = None;

static Atom XA_WM_WINDOW_TYPE = None;
static Atom XA_WM_WINDOW_TYPE_DESKTOP = None;
static Atom XA_WM_WINDOW_TYPE_DOCK = None;
static Atom XA_WM_WINDOW_TYPE_TOOLBAR = None;
static Atom XA_WM_WINDOW_TYPE_MENU = None;
static Atom XA_WM_WINDOW_TYPE_UTILITY = None;
static Atom XA_WM_WINDOW_TYPE_SPLASH = None;
static Atom XA_WM_WINDOW_TYPE_DIALOG = None;
static Atom XA_WM_WINDOW_TYPE_NORMAL = None;

void widget_stop(void);

void xitk_modal_window(Window w) {
  gXitk->modalw = w;
}
void xitk_unmodal_window(Window w) {
  if(w == gXitk->modalw)
    gXitk->modalw = None;
}

/*
 * Execute a shell command.
 */
int xitk_system(int dont_run_as_root, char *command) {
  int pid, status;
  
  /* 
   * Don't permit run as root
   */
  if(dont_run_as_root) {
    if(getuid() == 0)
      return -1;
  }
  
  if(command == 0)
    return 1;
  
  pid = fork();
  
  if(pid == -1)
    return -1;
  
  if(pid == 0) {
    char *argv[4];
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = command;
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

static int _x_error_handler(Display *display, XErrorEvent *xevent) {
  char buffer[2048];
  
  XGetErrorText(display, xevent->error_code, &buffer[0], 1023);
  
  XITK_WARNING("X error received: '%s'\n", buffer);
  
  xitk_x_error = 1;
  return 0;
  
}

void xitk_set_current_menu(xitk_widget_t *menu) {
  if(gXitk->menu)
    xitk_menu_destroy(gXitk->menu);

  gXitk->menu = menu;
}
void xitk_unset_current_menu(void) {
  gXitk->menu = NULL;
}

int xitk_install_x_error_handler(void) {
  if(gXitk->x_error_handler == NULL) {
    gXitk->x_error_handler = XSetErrorHandler(_x_error_handler);
    XSync(gXitk->display, False);
    return 1;
  }
  return 0;
}

int xitk_uninstall_x_error_handler(void) {
  if(gXitk->x_error_handler != NULL) {
    XSetErrorHandler(gXitk->x_error_handler);
    gXitk->x_error_handler = NULL;
    XSync(gXitk->display, False);
    return 1;
  }
  return 0;
}

/*
 *
 */
static void xitk_signal_handler(int sig) {
  pid_t cur_pid = getppid();

  /* First, call registered handler */
  if(cur_pid == xitk_pid) {
    if(gXitk->sig_callback)
      gXitk->sig_callback(sig, gXitk->sig_data);
  }
  
  gXitk->running = 0;
  
  switch (sig) {
    
  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
    if(cur_pid == xitk_pid) {
      
      xitk_list_free(gXitk->list);
      xitk_list_free(gXitk->gfx);
      xitk_config_deinit(gXitk->config);
      
      XITK_FREE(gXitk);
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
#endif
  return 0;
}
int xitk_is_use_xshm(void) {
  return gXitk->use_xshm;
}

static char *get_wm_name(Display *display, Window win, char *atom_name) {
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
  Atom      atom;
  uint32_t  type = WM_TYPE_UNKNOWN;
  char     *wm_name = NULL;
  
  XLockDisplay(display);

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

  if(type & WM_TYPE_EWMH_COMP) {
    XA_WIN_LAYER               = XInternAtom(display, "_NET_WM_STATE", False);
    XA_STAYS_ON_TOP            = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
    XA_NET_WM_STATE            = XInternAtom(display, "_NET_WM_STATE", False);
    XA_NET_WM_STATE_ABOVE      = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    XA_NET_WM_STATE_FULLSCREEN = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

    XA_WM_WINDOW_TYPE          = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    XA_WM_WINDOW_TYPE_DESKTOP  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XA_WM_WINDOW_TYPE_DOCK     = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XA_WM_WINDOW_TYPE_TOOLBAR  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    XA_WM_WINDOW_TYPE_MENU     = XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False);
    XA_WM_WINDOW_TYPE_UTILITY  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    XA_WM_WINDOW_TYPE_SPLASH   = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    XA_WM_WINDOW_TYPE_DIALOG   = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XA_WM_WINDOW_TYPE_NORMAL   = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  }
  
  switch(type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_KWIN:
    if(XA_NET_WM_STATE == None)
      XA_NET_WM_STATE    = XInternAtom(display, "_NET_WM_STATE", False);
    if(XA_STAYS_ON_TOP == None)
      XA_STAYS_ON_TOP = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
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
    XA_WIN_LAYER = XInternAtom(display, "_WIN_LAYER", False);
    break;
  }

  XUnlockDisplay(display);
  
  if(gXitk->verbosity) {
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
  return gXitk->wm_type;
}

int xitk_get_layer_level(void) {
  int level = 10;
  
  if((gXitk->wm_type & WM_TYPE_GNOME_COMP) || (gXitk->wm_type & WM_TYPE_EWMH_COMP))
    level = 10;
  
  switch(gXitk->wm_type & WM_TYPE_COMP_MASK) {
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

  if((gXitk->wm_type & WM_TYPE_GNOME_COMP) && !(gXitk->wm_type & WM_TYPE_EWMH_COMP)) {
    long propvalue[1];
    
    propvalue[0] = xitk_get_layer_level();

    XLockDisplay(gXitk->display);
    XChangeProperty(gXitk->display, window, XA_WIN_LAYER,
		    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		    1);
    XUnlockDisplay(gXitk->display);
    return;
  }
  

  if(gXitk->wm_type & WM_TYPE_EWMH_COMP) {
    XEvent xev;

    memset(&xev, 0, sizeof xev);
    XLockDisplay(gXitk->display);
    if(gXitk->wm_type & WM_TYPE_KWIN) {
      xev.xclient.type         = ClientMessage;
      xev.xclient.display      = gXitk->display;
      xev.xclient.window       = window;
      xev.xclient.message_type = XA_NET_WM_STATE;
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = 1;
      xev.xclient.data.l[1]    = XA_STAYS_ON_TOP;
      xev.xclient.data.l[2]    = 0l;
      xev.xclient.data.l[3]    = 0l;
      xev.xclient.data.l[4]    = 0l;
      
      XSendEvent(gXitk->display, DefaultRootWindow(gXitk->display), True, SubstructureRedirectMask, &xev);
    }
    else {
      xev.xclient.type         = ClientMessage;
      xev.xclient.serial       = 0;
      xev.xclient.send_event   = True;
      xev.xclient.display      = gXitk->display;
      xev.xclient.window       = window;
      xev.xclient.message_type = XA_NET_WM_STATE;
      xev.xclient.format       = 32;
      xev.xclient.data.l[0]    = (long) 1;
      xev.xclient.data.l[1]    = (long) XA_NET_WM_STATE_ABOVE;
      xev.xclient.data.l[2]    = (long) None;
      
      XSendEvent(gXitk->display, DefaultRootWindow(gXitk->display), 
		 False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*) &xev);
      
    }
    XUnlockDisplay(gXitk->display);
    
    return;
  }
  
  switch(gXitk->wm_type & WM_TYPE_COMP_MASK) {
  case WM_TYPE_MOTIF:
  case WM_TYPE_LARSWM:
    break;
    
  case WM_TYPE_KWIN:
    XLockDisplay(gXitk->display);
    XChangeProperty(gXitk->display, window, XA_WIN_LAYER,
		    XA_ATOM, 32, PropModeReplace, (unsigned char *)&XA_STAYS_ON_TOP, 1);
    XUnlockDisplay(gXitk->display);
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
      
      XLockDisplay(gXitk->display);
      XChangeProperty(gXitk->display, window, XA_WIN_LAYER,
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		      1);
      XUnlockDisplay(gXitk->display);
    }
    break;
  }
}

void xitk_set_window_layer(Window window, int layer) {
  XEvent xev;

  if(((gXitk->wm_type & WM_TYPE_COMP_MASK) == WM_TYPE_KWIN) ||
     ((gXitk->wm_type & WM_TYPE_EWMH_COMP) && !(gXitk->wm_type & WM_TYPE_GNOME_COMP))) {
    return;
  }

  memset(&xev, 0, sizeof xev);
  xev.type                 = ClientMessage;
  xev.xclient.type         = ClientMessage;
  xev.xclient.window       = window;
  xev.xclient.message_type = XA_WIN_LAYER;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (long) layer;
  xev.xclient.data.l[1]    = (long) 0;
  xev.xclient.data.l[2]    = (long) 0;
  xev.xclient.data.l[3]    = (long) 0;

  XLockDisplay(gXitk->display);
  XSendEvent(gXitk->display, RootWindow(gXitk->display, (XDefaultScreen(gXitk->display))), 
	     False, SubstructureNotifyMask, (XEvent*) &xev);
  XUnlockDisplay(gXitk->display);
}

static void _set_ewmh_state(Window window, Atom atom, int enable) {
  XEvent xev;
  
  if((window == None) || (atom == None))
    return;

  memset(&xev, 0, sizeof(xev));
  xev.xclient.type         = ClientMessage;
  xev.xclient.message_type = XA_NET_WM_STATE;
  xev.xclient.display      = gXitk->display;
  xev.xclient.window       = window;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (enable == 1) ? 1 : 0;
  xev.xclient.data.l[1]    = atom;
  xev.xclient.data.l[2]    = 0l;
  xev.xclient.data.l[3]    = 0l;
  xev.xclient.data.l[4]    = 0l;

  XLockDisplay(gXitk->display);
  XSendEvent(gXitk->display, DefaultRootWindow(gXitk->display), True, SubstructureRedirectMask, &xev);
  XUnlockDisplay(gXitk->display);
}

void xitk_set_ewmh_fullscreen(Window window) {
  
  if(!(gXitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;
  
  _set_ewmh_state(window, XA_NET_WM_STATE_FULLSCREEN, 1);
  _set_ewmh_state(window, XA_STAYS_ON_TOP, 1);
}

void xitk_unset_ewmh_fullscreen(Window window) {
  
  if(!(gXitk->wm_type & WM_TYPE_EWMH_COMP) || (window == None))
    return;
  
  _set_ewmh_state(window, XA_NET_WM_STATE_FULLSCREEN, 0);
  _set_ewmh_state(window, XA_STAYS_ON_TOP, 0);
}

static void _set_wm_window_type(Window window, xitk_wm_window_type_t type, int value) {
  if(window && (gXitk->wm_type & WM_TYPE_EWMH_COMP)) {
    Atom *atom = NULL;
    
    switch(type) {
    case WINDOW_TYPE_DESKTOP:
      atom = &XA_WM_WINDOW_TYPE_DESKTOP;
      break;
    case WINDOW_TYPE_DOCK:
      atom = &XA_WM_WINDOW_TYPE_DOCK;
      break;
    case WINDOW_TYPE_TOOLBAR:
      atom = &XA_WM_WINDOW_TYPE_TOOLBAR;
      break;
    case WINDOW_TYPE_MENU:
      atom = &XA_WM_WINDOW_TYPE_MENU;
      break;
    case WINDOW_TYPE_UTILITY:
      atom = &XA_WM_WINDOW_TYPE_UTILITY;
      break;
    case WINDOW_TYPE_SPLASH:
      atom = &XA_WM_WINDOW_TYPE_SPLASH;
      break;
    case WINDOW_TYPE_DIALOG:
      atom = &XA_WM_WINDOW_TYPE_DIALOG;
      break;
    case WINDOW_TYPE_NORMAL:
      atom = &XA_WM_WINDOW_TYPE_NORMAL;
      break;
    }
    
    if(atom) {
      XLOCK(gXitk->display);
      XChangeProperty(gXitk->display, window, XA_WM_WINDOW_TYPE, XA_ATOM, 32, PropModeReplace, (unsigned char *)atom, 1);
      XRaiseWindow(gXitk->display, window);
      XUNLOCK(gXitk->display);
    }
  }
}

void xitk_unset_wm_window_type(Window window, xitk_wm_window_type_t type) {
  _set_wm_window_type(window, type, 0);
}

void xitk_set_wm_window_type(Window window, xitk_wm_window_type_t type) {
  _set_wm_window_type(window, type, 1);
}

/*
 * Create a new widget_list, store the pointer in private
 * list of xitk_widget_list_t, then return the widget_list pointer.
 */
xitk_widget_list_t *xitk_widget_list_new (void) {
  xitk_widget_list_t *l;

  l = (xitk_widget_list_t *) xitk_xmalloc(sizeof(xitk_widget_list_t));

  l->widget_focused     = NULL;
  l->widget_under_mouse = NULL;
  l->widget_pressed     = NULL;

  MUTLOCK();

  xitk_list_append_content(gXitk->list, l);

  MUTUNLOCK();

  return l;
}

/*
 * Change the window for the xevent_handler previously initialized
 * at widget_register_event_handler() time. It also remade it
 * DND aware, only if DND stuff was initialized at register time too.
 */
void xitk_change_window_for_event_handler (xitk_register_key_t key, Window window) {
  __gfx_t  *fx;
  
  MUTLOCK();
      
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  
  while(fx) {

    FXLOCK(fx);
    if(fx->key == key) {

      fx->window = window;

      if(fx->xdnd && (window != None)) {
	if(!xitk_make_window_dnd_aware(fx->xdnd, window))
	  xitk_unset_dnd_callback(fx->xdnd);
      }
      
      FXUNLOCK(fx);
      MUTUNLOCK();
      return;
    }
    
    FXUNLOCK(fx);
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();
}

/*
 * Register a callback function called when a signal happen.
 */
void xitk_register_signal_handler(xitk_signal_callback_t sigcb, void *user_data) {
  if(sigcb) {
    gXitk->sig_callback = sigcb;
    gXitk->sig_data     = user_data;
  }
}

/*
 * Register a window, with his own event handler, callback
 * for DND events, and widget list.
 */
xitk_register_key_t xitk_register_event_handler(char *name, Window window,
						widget_event_callback_t cb,
						widget_newpos_callback_t pos_cb,
						xitk_dnd_callback_t dnd_cb,
						xitk_widget_list_t *wl, void *user_data) {
  __gfx_t   *fx;
  
  //  printf("%s()\n", __FUNCTION__);

  fx = (__gfx_t *) xitk_xmalloc(sizeof(__gfx_t));

  fx->name = name ? strdup(name) : strdup("NO_SET");

  fx->window    = window;
  fx->new_pos.x = 0;
  fx->new_pos.y = 0;
  fx->width     = 0;
  fx->height    = 0;
  fx->user_data = user_data;
  pthread_mutex_init(&fx->mutex, NULL);
  
  if(window != None) {
    XWindowAttributes wattr;
    Status            err;
    
    XLOCK(gXitk->display);
    err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
    XUNLOCK(gXitk->display);
    
    if(err != BadDrawable && err != BadWindow) {
      Window c;
      
      XLOCK(gXitk->display);
      XTranslateCoordinates(gXitk->display, fx->window, wattr.root, 
			    0, 0, &(fx->new_pos.x), &(fx->new_pos.y), &c);
      XUNLOCK(gXitk->display);
      
      /*
	fx->new_pos.x = wattr.x;
	fx->new_pos.y = wattr.y;
      */
      fx->width     = wattr.width;
      fx->height    = wattr.height;
    }
  }

  if(cb)
    fx->xevent_callback = cb;
#if 0
  else {
    XITK_DIE("%s()@%d: Callback should be non NULL\n", __FUNCTION__, __LINE__);
  }
#endif

  if(pos_cb && (window != None))
    fx->newpos_callback = pos_cb;
  else
    fx->newpos_callback = NULL;
  
  if(wl)
    fx->widget_list = wl;
  else
    fx->widget_list = NULL;

  if(dnd_cb && (window != None)) {
    fx->xdnd = (xitk_dnd_t *) xitk_xmalloc(sizeof(xitk_dnd_t));
    
    xitk_init_dnd(gXitk->display, fx->xdnd);
    if(xitk_make_window_dnd_aware(fx->xdnd, fx->window))
      xitk_set_dnd_callback(fx->xdnd, dnd_cb);
  }
  else
    fx->xdnd = NULL;

  fx->key = ++gXitk->key;

  if(fx->window) {

    XLOCK(gXitk->display);
    fx->XA_XITK = XInternAtom(gXitk->display, "_XITK_EVENT", False);
    XChangeProperty (gXitk->display, fx->window, fx->XA_XITK, XA_ATOM,
		     32, PropModeAppend, (unsigned char *)&XITK_VERSION, 1);
    XUNLOCK(gXitk->display);

  }
  else
    fx->XA_XITK = None;

  MUTLOCK();

  xitk_list_append_content(gXitk->gfx, fx);

  MUTUNLOCK();

  return fx->key;
}

static void __fx_destroy(__gfx_t *fx, int locked) {
  __gfx_t *cur;
  
  if(!fx)
    return;
  
  if(!locked)
    MUTLOCK();
  
  cur = (__gfx_t *) xitk_list_get_current(gXitk->gfx);
  
  if(fx->xdnd) {
    xitk_unset_dnd_callback(fx->xdnd);
    free(fx->xdnd);
  }

  if (fx->widget_list && fx->widget_list->destroy)
    free(fx->widget_list);
  
  fx->xevent_callback = NULL;
  fx->newpos_callback = NULL;
  fx->user_data       = NULL;

  free(fx->name);
  
  if(cur) {
    if(cur == fx)
      xitk_list_delete_current(gXitk->gfx); 
    else {
      __gfx_t *cfx;
      
      cfx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
      while(cfx) {
	if(cfx == fx) {
	  xitk_list_delete_current(gXitk->gfx); 
	  break;
	}
	
	cfx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
      }
      
      /* set current back */
      cfx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
      while(cfx) {
	
	if(cfx == cur)
	  break;
	
	cfx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
      }
    }
  }
  
  FXUNLOCK(fx);
  pthread_mutex_destroy(&fx->mutex);

  free(fx);
  
  if(!locked)
    MUTUNLOCK();
}

/*
 * Remove from the list the window/event_handler
 * specified by the key.
 */
void xitk_unregister_event_handler(xitk_register_key_t *key) {
  __gfx_t  *fx;

  //  printf("%s()\n", __FUNCTION__);

  MUTLOCK();
  
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  
  while(fx) {

    if(pthread_mutex_trylock(&fx->mutex)) {
      if(fx->key == *key) {
	*key        = 0; 
	fx->destroy = 1;
	MUTUNLOCK();
	return;
      }
    }
    else {
      if(fx->key == *key) {
	*key = 0; 
	
	__fx_destroy(fx, 1);
	MUTUNLOCK();
	return;
      }
      FXUNLOCK(fx);
    }

    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();
}

void xitk_widget_list_defferred_destroy(xitk_widget_list_t *wl) {
  __gfx_t  *fx;

  MUTLOCK();
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  while (fx) {
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

    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();

  free(wl);
}

/*
 * Copy window information matching with key in passed window_info_t struct.
 */
int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf) {
  __gfx_t  *fx;

  MUTLOCK();

  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
    
  while(fx) {
    int  already_locked = 0;
    
    if(pthread_mutex_trylock(&fx->mutex))
      already_locked = 1;

    if((fx->key == key) && (fx->window != None)) {
      Window c;
      
      winf->window = fx->window;
      
      if(fx->name)
	winf->name = strdup(fx->name);
      
      XLOCK(gXitk->display);
      XTranslateCoordinates(gXitk->display, fx->window, DefaultRootWindow(gXitk->display), 
			    0, 0, &(fx->new_pos.x), &(fx->new_pos.y), &c);
      XUNLOCK(gXitk->display);
      
      
      winf->x      = fx->new_pos.x;
      winf->y      = fx->new_pos.y;
      winf->height = fx->height;
      winf->width  = fx->width;
      
      if(!already_locked)
	FXUNLOCK(fx);

      MUTUNLOCK();
      return 1;

    }
    
    if(!already_locked)
      FXUNLOCK(fx);
    
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();
  return 0;
}

/*
 * Here events are handled. All widget are locally
 * handled, then if a event handler callback was passed
 * at register time, it will be called.
 */
void xitk_xevent_notify(XEvent *event) {
  __gfx_t  *fx, *fxd;
     
  if(!(fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx)))
    return;

  if(event->type == KeyPress || event->type == KeyRelease) {

    /* Filter keys that dont't need to be handled by xine  */
    /* and could be used by our screen saver reset "ping". */
    /* So they will not kill tips and menus.               */

    size_t i;

    for(i = 0; i < sizeof(gXitk->ignore_keys)/sizeof(gXitk->ignore_keys[0]); ++i)
      if(event->xkey.keycode == gXitk->ignore_keys[i])
	return;
  }
  
  FXLOCK(fx);
  
  if(gXitk->modalw != None) {
    while(fx && (fx->window != gXitk->modalw)) {
      
      if(fx->xevent_callback && (fx->window != None && event->type != KeyRelease))
	fx->xevent_callback(event, fx->user_data);
      
      fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
    }
  }
  
  while(fx) {

    if(event->type == KeyRelease)
      gettimeofday(&gXitk->keypress, 0);
    
    if(fx->window != None) {
      
      //printf("event %d\n", event->type);

      if(fx->window == event->xany.window) {
	
	switch(event->type) {

	case MappingNotify:
	  XLOCK(gXitk->display);
	  XRefreshKeyboardMapping((XMappingEvent *) event);
	  XUNLOCK(gXitk->display);
	  break;

	case KeyPress: {
	  XKeyEvent      mykeyevent;
	  KeySym         mykey;
	  char           kbuf[256];
	  int            len;
	  int            modifier;
	  int            handled = 0;
	  xitk_widget_t *w = NULL;
	  
	  mykeyevent = event->xkey;

	  xitk_get_key_modifier(event, &modifier);

	  XLOCK(gXitk->display);
	  len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
	  XUNLOCK(gXitk->display);

	  xitk_tips_hide_tips();
	  
	  if(fx->widget_list && fx->widget_list->widget_focused) {
	    w = fx->widget_list->widget_focused;
	  }

	  if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) &&
		   (mykey != XK_Tab) && (mykey != XK_KP_Tab) && (mykey != XK_ISO_Left_Tab))) {
	    
	    xitk_send_key_event(w, event);
	    
	    if((mykey == XK_Return) || (mykey == XK_KP_Enter) || (mykey == XK_ISO_Enter)) {
	      widget_event_t  event;
	      
	      event.type = WIDGET_EVENT_PAINT;
	      (void) w->event(w, &event, NULL);
	      
	      xitk_set_focus_to_next_widget(fx->widget_list, 0);
	    }

	    FXUNLOCK(fx);
	    return;
	  }
	  
	  /* close menu */
	  if(mykey == XK_Escape) {
	    if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU)) {
	      xitk_widget_t *m = xitk_menu_get_menu(w);
	      
	      xitk_menu_destroy(m);
	      FXUNLOCK(fx);
	      return;
	    }
	  }
	  /* set focus to next widget */
	  else if((mykey == XK_Tab) || (mykey == XK_KP_Tab) || (mykey == XK_ISO_Left_Tab)) {
	    if(fx->widget_list) {
	      handled = 1;
	      xitk_set_focus_to_next_widget(fx->widget_list, (modifier & MODIFIER_SHIFT));
	    }
	  }
	  /* simulate click event on space/return/enter key event */
	  else if((mykey == XK_space) || (mykey == XK_Return) || 
		  (mykey == XK_KP_Enter) || (mykey == XK_ISO_Enter)) {
	    if(w && (((w->type & WIDGET_CLICKABLE) && (w->type & WIDGET_KEYABLE))
		     && w->visible && w->enable)) {

	    __menu_sim_click:

	      if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON) ||
		       ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) ||
		       ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX))) {
		widget_event_t         event;
		widget_event_result_t  result;

		handled = 1;

		event.type           = WIDGET_EVENT_CLICK;
		event.x              = w->x;
		event.y              = w->y;
		event.button_pressed = LBUTTON_DOWN;
		event.button         = Button1;

		(void) w->event(w, &event, &result);

		event.button_pressed = LBUTTON_UP;

		(void) w->event(w, &event, &result);

		//		if(fx->xevent_callback)
		//		  fx->xevent_callback(event, fx->user_data);
	      }
	    }
	  }
	  /* move sliders, handle menu items */
	  else if(((mykey == XK_Left) || (mykey == XK_Right) 
		   || (mykey == XK_Up) || (mykey == XK_Down)
		   || (mykey == XK_Prior) || (mykey == XK_Next)) 
		  && ((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD)) {
	    
	    if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
	      xitk_widget_t *b = xitk_browser_get_browser(w);
	      
	      if(b) {
		handled = 1;
		if(mykey == XK_Up)
		  xitk_browser_step_down(b, NULL);
		else if(mykey == XK_Down)
		  xitk_browser_step_up(b, NULL);
		else if(mykey == XK_Left)
		  xitk_browser_step_left(b, NULL);
		else if(mykey == XK_Right)
		  xitk_browser_step_right(b, NULL);
		else if(mykey == XK_Prior)
		  xitk_browser_page_down(b, NULL);
		else if(mykey == XK_Next)
		  xitk_browser_page_up(b, NULL);
	      }
	    }
	    else if(w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER) 
			  && (w->type & WIDGET_KEYABLE))) {
	      handled = 1;
	      if((mykey == XK_Left) || (mykey == XK_Down) || (mykey == XK_Next)) {
		xitk_slider_make_backstep(w);
	      }
	      else {
		xitk_slider_make_step(w);
	      }
	      xitk_slider_callback_exec(w);
	    }
	    else if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU)) {
	      handled = 1;
	      if(mykey == XK_Left) {
		/* close menu branch */
		xitk_menu_destroy_branch(w);
	      }
	      else if(mykey == XK_Right) {
		/* simulate click event: trigger action of menu item (e.g. open submenu) */
		goto __menu_sim_click; // (goto is bad but simple ...)
	      }
	      else {
		/* next/previous menu item */
		if(fx->widget_list)
		  xitk_set_focus_to_next_widget(fx->widget_list,
						((mykey == XK_Up) || (mykey == XK_Prior)));
	      }
	    }
	  }
	  else if((mykey == XK_0) || (mykey == XK_1) || (mykey == XK_2) || (mykey == XK_3) || 
		  (mykey == XK_4) || (mykey == XK_5) || (mykey == XK_6) || (mykey == XK_7) || 
		  (mykey == XK_8) || (mykey == XK_9) || 
		  (mykey == XK_underscore) ||
		  (mykey == XK_a) || (mykey == XK_A) || (mykey == XK_b) || (mykey == XK_B) ||
		  (mykey == XK_c) || (mykey == XK_C) || (mykey == XK_d) || (mykey == XK_D) ||
		  (mykey == XK_e) || (mykey == XK_E) || (mykey == XK_f) || (mykey == XK_F) ||
		  (mykey == XK_g) || (mykey == XK_G) || (mykey == XK_h) || (mykey == XK_H) ||
		  (mykey == XK_i) || (mykey == XK_I) || (mykey == XK_j) || (mykey == XK_J) ||
		  (mykey == XK_k) || (mykey == XK_K) || (mykey == XK_l) || (mykey == XK_L) ||
		  (mykey == XK_m) || (mykey == XK_M) || (mykey == XK_n) || (mykey == XK_N) ||
		  (mykey == XK_o) || (mykey == XK_O) || (mykey == XK_p) || (mykey == XK_P) ||
		  (mykey == XK_q) || (mykey == XK_Q) || (mykey == XK_r) || (mykey == XK_R) ||
		  (mykey == XK_s) || (mykey == XK_S) || (mykey == XK_t) || (mykey == XK_T) ||
		  (mykey == XK_u) || (mykey == XK_U) || (mykey == XK_v) || (mykey == XK_V) ||
		  (mykey == XK_w) || (mykey == XK_W) || (mykey == XK_x) || (mykey == XK_X) ||
		  (mykey == XK_y) || (mykey == XK_Y) || (mykey == XK_z) || (mykey == XK_Z)) {

	    
	    if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
	      xitk_widget_t *b = xitk_browser_get_browser(w);
	      
	      if(b) {
		handled = 1;
		xitk_browser_warp_jump(b, kbuf, modifier);
	      }

	    }
	  }

	  if(!handled) {

	    if(gXitk->menu && 
	       ((fx->widget_list && 
		 ((!fx->widget_list->widget_focused) || 
		  (!(fx->widget_list->widget_focused->type & WIDGET_GROUP_MENU)))) ||
		(!fx->widget_list)))  {
	      
	      xitk_set_current_menu(NULL);
	    }
	    
	    if((w == NULL) || (w && (((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) == 0))) {
	      if(fx->xevent_callback) {
		fx->xevent_callback(event, fx->user_data);
	      }
	    }
	    
	    if(w && ((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) && 
	       ((mykey == XK_Return) || (mykey == XK_KP_Enter) || (mykey == XK_ISO_Enter))) {
	      widget_event_t  event;
	      
	      event.type = WIDGET_EVENT_PAINT;
	      (void) w->event(w, &event, NULL);
	      
	      xitk_set_focus_to_next_widget(fx->widget_list, 0);
	      
	    }
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

	    XLOCK(gXitk->display);
	    while(XCheckTypedWindowEvent(gXitk->display, fx->window, 
					 Expose, event) == True);
	    XUNLOCK(gXitk->display);

	    if(event->xexpose.count == 0)
	      xitk_paint_widget_list(fx->widget_list);
	  }
	  break;
	  
	case MotionNotify: {
	  XWindowAttributes wattr;
	  Status            err;
	  
	  XLOCK(gXitk->display);
	  while(XCheckMaskEvent(gXitk->display, ButtonMotionMask, event) == True);
	  XUNLOCK(gXitk->display);

	  fx->old_event = event;
	  if(fx->move.enabled) {

	    if(fx->widget_list->widget_focused && 
	       (fx->widget_list->widget_focused->type & WIDGET_GROUP_MENU)) {
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
	    
	    XLOCK(gXitk->display);

	    XMoveWindow(gXitk->display, fx->window,
			fx->new_pos.x, fx->new_pos.y);
	    err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);

	    XUNLOCK(gXitk->display);

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

	  xitk_tips_hide_tips();
	  
	  XLOCK(gXitk->display);
	  status = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	  /* 
	   * Give focus (and raise) to window after click
	   * if it's viewable (e.g. not iconified).
	   */
	  if((status != BadDrawable) && (status != BadWindow) 
	     && (wattr.map_state == IsViewable)) {
	    XRaiseWindow(gXitk->display, fx->window);
	    XSetInputFocus(gXitk->display, fx->window, RevertToParent, CurrentTime);
	  }
	  XUNLOCK(gXitk->display);
	  
	  if(gXitk->menu && 
	     ((fx->widget_list && 
	       ((!fx->widget_list->widget_focused) || 
		(!(fx->widget_list->widget_focused->type & WIDGET_GROUP_MENU)))) ||
	      (!fx->widget_list)))  {

	    xitk_set_current_menu(NULL);

	    FXUNLOCK(fx);
	    return;
	  }
	  
	  if(fx->widget_list) {

	    fx->move.enabled = !xitk_click_notify_widget_list (fx->widget_list, 
							       event->xbutton.x, 
							       event->xbutton.y, 
							       event->xbutton.button, 0);
	    if(event->xbutton.button != Button1) {
	      xitk_widget_t *w = fx->widget_list->widget_focused;

	      fx->move.enabled = 0;
	      
	      if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
		xitk_widget_t *b = xitk_browser_get_browser(w);
		
		if(b) {
		  
		  if(event->xbutton.button == Button4) {
		    xitk_browser_step_down(b, NULL);
		  }
		  else if(event->xbutton.button == Button5) {
		    xitk_browser_step_up(b, NULL);
		  }
		  
		}
	      }
	    }

	    if(fx->move.enabled) {
	      XWindowAttributes wattr;
	      Status            err;

	      XLOCK(gXitk->display);
	      err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	      XUNLOCK(gXitk->display);

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

	  xitk_tips_hide_tips();
	  
	  if(fx->move.enabled) {

	    fx->move.enabled = 0;
	    /* Inform application about window movement. */

	    if(fx->newpos_callback)
	      fx->newpos_callback(fx->new_pos.x, fx->new_pos.y, 
				  fx->width, fx->height);
	  }
	  else {
	    if(fx->widget_list) {
	      xitk_click_notify_widget_list (fx->widget_list, 
					     event->xbutton.x, event->xbutton.y,
					     event->xbutton.button, 1);
	    }
	  }
	  break;
	  
	case ConfigureNotify: {
	  XWindowAttributes wattr;
	  Status            err;

	  if(fx->widget_list && fx->widget_list->l) {
	    xitk_widget_t *w = (xitk_widget_t *) xitk_list_first_content(fx->widget_list->l);
	    while (w) {
	      if(((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO) &&
		 (w->type & WIDGET_GROUP_WIDGET)) {
		xitk_combo_update_pos(w);
	      }
	      w = (xitk_widget_t *) xitk_list_next_content (fx->widget_list->l);
	    }
	  }

	  /* Inform application about window movement. */
	  if(fx->newpos_callback) {

	    XLOCK(gXitk->display);
	    err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	    XUNLOCK(gXitk->display);

	    if(err != BadDrawable && err != BadWindow) {
	      fx->width = wattr.width;
	      fx->height = wattr.height;
	    }
	    fx->newpos_callback(event->xconfigure.x,
				event->xconfigure.y,
				fx->width, fx->height);
	  }
	}
	break;

	case SelectionNotify:
	case ClientMessage:
	  if(fx->xdnd)
	    xitk_process_client_dnd_message(fx->xdnd, event);
	  break;
	}
	
	if(fx->xevent_callback) {
	  fx->xevent_callback(event, fx->user_data);
	}
      }
    }
    
    fxd = fx;
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);

    if(fxd->destroy)
      __fx_destroy(fxd, 0);
    else
      FXUNLOCK(fxd);
    
#warning FIXME
    if(gXitk->modalw != None) {

      /* Flush remain fxs */
      while(fx && (fx->window != gXitk->modalw)) {
	FXLOCK(fx);
	
	if(fx->xevent_callback && (fx->window != None && event->type != KeyRelease))
	  fx->xevent_callback(event, fx->user_data);
	
	fxd = fx;
	fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);

	if(fxd->destroy)
	  __fx_destroy(fxd, 0);
	else
	  FXUNLOCK(fxd);
      }
      return;
    }

    if(fx)
      FXLOCK(fx);
  }
}

/*
 * Initiatization of widget internals.
 */
void xitk_init(Display *display, XColor black, int verbosity) {
  char buffer[256];
  
  xitk_pid = getppid();

#ifdef ENABLE_NLS
  bindtextdomain("xitk", XITK_LOCALE);
#endif  

  gXitk = (__xitk_t *) xitk_xmalloc(sizeof(__xitk_t));

  gXitk->black           = black;
  gXitk->display_width   = DisplayWidth(display, DefaultScreen(display));
  gXitk->display_height  = DisplayHeight(display, DefaultScreen(display));
  gXitk->verbosity       = verbosity;
  gXitk->list            = xitk_list_new();
  gXitk->gfx             = xitk_list_new();
  gXitk->display         = display;
  gXitk->key             = 0;
  gXitk->sig_callback    = NULL;
  gXitk->sig_data        = NULL;
  gXitk->config          = xitk_config_init();
  gXitk->use_xshm        = (xitk_config_get_shm_feature(gXitk->config)) ? (xitk_check_xshm(display)) : 0;
  xitk_x_error           = 0;
  gXitk->x_error_handler = NULL;
  gXitk->modalw          = None;
  gXitk->ignore_keys[0]  = XKeysymToKeycode(display, XK_Shift_L);
  gXitk->ignore_keys[1]  = XKeysymToKeycode(display, XK_Control_L);
  gXitk->tips_timeout    = TIPS_TIMEOUT;
  XGetInputFocus(display, &(gXitk->parent.window), &(gXitk->parent.focus));

  memset(&gXitk->keypress, 0, sizeof(gXitk->keypress));

  pthread_mutex_init (&gXitk->mutex, NULL);
  
  snprintf(buffer, sizeof(buffer), "-[ xiTK version %d.%d.%d ", XITK_MAJOR_VERSION, XITK_MINOR_VERSION, XITK_SUB_VERSION);
  
  /* Check if SHM is working */
#ifdef HAVE_SHM
  if(gXitk->use_xshm) {
    XImage             *xim;
    XShmSegmentInfo     shminfo;
    
    xim = XShmCreateImage(display, 
			  (DefaultVisual(display, (DefaultScreen(display)))), 
			  (DefaultDepth(display, (DefaultScreen(display)))),
			  ZPixmap, NULL, &shminfo, 10, 10);
    if(!xim)
      gXitk->use_xshm = 0;
    else {
      shminfo.shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0777);
      if(shminfo.shmid < 0) {
	XDestroyImage(xim);
	gXitk->use_xshm = 0;
      }
      else {
	shminfo.shmaddr = xim->data =  shmat(shminfo.shmid, 0, 0);
	if(shminfo.shmaddr == (char *) -1) {
	  XDestroyImage(xim);
	  gXitk->use_xshm = 0;
	}
	else {
	  shminfo.readOnly = False;
	  
	  xitk_x_error = 0;
	  xitk_install_x_error_handler();
	  
	  XShmAttach(display, &shminfo);
	  XSync(display, False);
	  if(xitk_x_error)
	    gXitk->use_xshm = 0;
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

  gXitk->wm_type = xitk_check_wm(display);
  
  /* init font caching */
  xitk_font_cache_init();
  
  xitk_cursors_init(display);
  xitk_tips_init(display);
}

/*
 * Start widget event handling.
 * It will block till widget_stop() call
 */
void xitk_run(xitk_startup_callback_t cb, void *data) {
  XEvent            myevent;
  struct sigaction  action;
  fd_set            r;
  Bool              got_event;
  __gfx_t          *fx;
  struct timeval    tv;
  int               xconnection;

  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGHUP, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGHUP) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR1, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGUSR1) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGUSR2, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGUSR2) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGINT, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGINT) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGTERM, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGTERM) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGQUIT, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGQUIT) failed: %s\n", strerror(errno));
  }
#ifndef DEBUG
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGSEGV, &action, NULL) != 0) {
    XITK_WARNING("sigaction(SIGSEGV) failed: %s\n", strerror(errno));
  }
#endif  
  gXitk->running = 1;
  
  XLOCK(gXitk->display);
  XSync(gXitk->display, True); /* Flushing the toilets */
  XUNLOCK(gXitk->display);

  /*
   * Force to repain the widget list if it exist
   */
  MUTLOCK();
  
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  
  while(fx) {
    FXLOCK(fx);
   
    if(fx->window != None && fx->widget_list) {
      XEvent xexp;

      memset(&xexp, 0, sizeof xexp);
      xexp.xany.type          = Expose;
      xexp.xexpose.type       = Expose;
      xexp.xexpose.send_event = True;
      xexp.xexpose.display    = gXitk->display;
      xexp.xexpose.window     = fx->window;
      xexp.xexpose.count      = 0;
      
      XLOCK(gXitk->display);
      if(!XSendEvent(gXitk->display, fx->window, False, ExposureMask, &xexp)) {
	XITK_WARNING("XSendEvent(display, 0x%x ...) failed.\n", (unsigned int) fx->window);
      }
      XUNLOCK(gXitk->display);
    }

    FXUNLOCK(fx);
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();

  /* We're ready to handle anything */
  if(cb)
    cb(data);

  XLOCK(gXitk->display);
  xconnection = ConnectionNumber(gXitk->display);
  XUNLOCK(gXitk->display);
  
  /*
   * Now, wait for a new xevent
   */
  while(gXitk->running) {
    
    FD_ZERO(&r);
    FD_SET(xconnection, &r);

    tv.tv_sec  = 0;
    tv.tv_usec = 33000;

    select(xconnection + 1, &r, 0, 0, &tv);
    
    XLOCK(gXitk->display);
    got_event = (XPending(gXitk->display) != 0);
    if( got_event )
      XNextEvent(gXitk->display, &myevent);
    XUNLOCK(gXitk->display);
      
    while(got_event == True) {

      xitk_xevent_notify(&myevent);

      XLOCK(gXitk->display);
      got_event = (XPending(gXitk->display) != 0);
      if( got_event )
        XNextEvent(gXitk->display, &myevent);
      XUNLOCK(gXitk->display);
    }

  }

  /* pending destroys of the event handlers */
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  while(fx) {
    __fx_destroy(fx, 1);
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }
  xitk_list_free(gXitk->gfx);

  /* destroy font caching */
  xitk_font_cache_done();
  
  xitk_list_free(gXitk->list);

  xitk_config_deinit(gXitk->config);
  pthread_mutex_destroy(&gXitk->mutex);
  
  XITK_FREE(gXitk);

}

/*
 * Stop the wait xevent loop
 */
void xitk_stop(void) {
  xitk_tips_deinit();
  xitk_cursors_deinit(gXitk->display);
  gXitk->running = 0;

  if(gXitk->parent.window != None)
    XSetInputFocus(gXitk->display, gXitk->parent.window, gXitk->parent.focus, CurrentTime);
}
 
char *xitk_get_system_font(void) {
  return xitk_config_get_system_font(gXitk->config);
}
char *xitk_get_default_font(void) {
  return xitk_config_get_default_font(gXitk->config);
}
int xitk_get_xmb_enability(void) {
  return xitk_config_get_xmb_enability(gXitk->config);
}
void xitk_set_xmb_enability(int value) {
  xitk_config_set_xmb_enability(gXitk->config, value);
}
int xitk_get_black_color(void) {
  return xitk_config_get_black_color(gXitk->config);
}
int xitk_get_white_color(void) {
  return xitk_config_get_white_color(gXitk->config);
}
int xitk_get_background_color(void) {
  return xitk_config_get_background_color(gXitk->config);
}
int xitk_get_focus_color(void) {
  return xitk_config_get_focus_color(gXitk->config);
}
int xitk_get_select_color(void) {
  return xitk_config_get_select_color(gXitk->config);
}
unsigned long xitk_get_timer_label_animation(void) {
  return xitk_config_get_timer_label_animation(gXitk->config);
}
unsigned long xitk_get_warning_foreground(void) {
  return xitk_config_get_warning_foreground(gXitk->config);
}
unsigned long xitk_get_warning_background(void) {
  return xitk_config_get_warning_background(gXitk->config);
}
long int xitk_get_timer_dbl_click(void) {
  return xitk_config_get_timer_dbl_click(gXitk->config);
}
int xitk_get_barstyle_feature(void) {
  return xitk_config_get_barstyle_feature(gXitk->config);
}
int xitk_get_checkstyle_feature(void) {
  return xitk_config_get_checkstyle_feature(gXitk->config);
}
int xitk_get_cursors_feature(void) {
  return xitk_config_get_cursors_feature(gXitk->config);
}

int xitk_get_menu_shortcuts_enability(void) {
  return xitk_config_get_menu_shortcuts_enability(gXitk->config);
}

int xitk_get_display_width(void) {
  return gXitk->display_width;
}
int xitk_get_display_height(void) {
  return gXitk->display_height;
}
XColor xitk_get_black_pixel_color(void) {
  return gXitk->black;
}

unsigned long xitk_get_tips_timeout(void) {
  return gXitk->tips_timeout;
}
void xitk_set_tips_timeout(unsigned long timeout) {
  gXitk->tips_timeout = timeout;
}

/*
 * copy src to dest and substitute special chars. dest should have 
 * enought space to store chars.
 */
/*
void xitk_subst_special_chars(char *src, char *dest) {
  char *s, *d;
  
  if((src == NULL) || (dest == NULL)) {
    XITK_WARNING("pass NULL argument(s)\n");
    return;
  }
  
  if(!strlen(src))
    return;

  memset(dest, 0, sizeof(dest));
  s = src;
  d = dest;
  while(*s != '\0') {
    
    switch(*s) {
    case '%':
      if((*(s) == '%') && (*(s + 1) != '%')) {
	char    buffer[5] = { '0', 'x', *(s + 1) , *(s + 2), 0 };
	char   *p         = buffer;
	int     character = strtol(p, &p, 16);
	
	*d = character;
	s += 2;
      }
      else {
	*d++ = '%';
	*d = '%';
      }
      break;
      
    case '~':
      if(*(s + 1) == '/') {
	strcat(d, xine_get_homedir());
	d += (strlen(xine_get_homedir()) - 1);
      } else
        *d = *s;
      break;
      
    default:
      *d = *s;
      break;
    }
    s++;
    d++;
  }
  *d = '\0';
}
*/
/*
 *
 */
char *xitk_set_locale(void) {
  char *cur_locale = NULL;
  
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
long int xitk_get_last_keypressed_time(void) {
  struct timeval tm, tm_diff;
  
  gettimeofday(&tm, NULL);
  timersub(&tm, &gXitk->keypress, &tm_diff);
  return tm_diff.tv_sec;
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
  int i;
  
  ABORT_IF_NULL(val);

  for(i = 0; i < sizeof(bools)/sizeof(bools[0]); i++) {
    if(!(strcasecmp(bools[i].str, val)))
      return bools[i].value;
  }

  return 0;
}
