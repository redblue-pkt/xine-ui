/* 
 * Copyright (C) 2000-2002 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
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

#include <locale.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "widget.h"
#include "list.h"
#include "dnd.h"
#include "inputtext.h"
#include "checkbox.h"
#include "browser.h"
#include "slider.h"
#include "tips.h"
#include "_config.h"

#include "_xitk.h"

extern char **environ;
extern int errno;
#undef TRACE_LOCKS

#ifdef TRACE_LOCKS
static int ml = 0;
#define MUTLOCK()                                                             \
  {                                                                           \
    int i;                                                                    \
    ml++;                                                                     \
    for(i=0; i<ml; i++) printf(".");                                          \
    printf("LOCK\n");                                                         \
    pthread_mutex_lock(&gXitk->mutex);                                        \
  }

#define MUTUNLOCK()                                                           \
  {                                                                           \
    int i;                                                                    \
    for(i=0; i<ml; i++) printf(".");                                          \
    printf("UNLOCK\n");                                                       \
    ml--;                                                                     \
    pthread_mutex_unlock(&gXitk->mutex);                                      \
  }

#else
#define MUTLOCK()   { pthread_mutex_lock(&gXitk->mutex); }
#define MUTUNLOCK() { pthread_mutex_unlock(&gXitk->mutex); }
#endif

typedef void (*widget_event_callback_t)(XEvent *event, void *user_data);
typedef void (*widget_newpos_callback_t)(int, int, int, int);
typedef void (*xitk_signal_callback_t)(int, void *);

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
} __gfx_t;

typedef struct {
  Display                    *display;
  xitk_list_t                *list;
  xitk_list_t                *gfx;

  pthread_mutex_t             mutex;
  int                         running;
  xitk_register_key_t         key;
  xitk_config_t              *config;
  xitk_signal_callback_t      sig_callback;
  void                       *sig_data;
} __xitk_t;

static __xitk_t    *gXitk;
static pid_t        xitk_pid;


void widget_stop(void);

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
#if HAVE_NANOSLEEP
  /* nanosleep is prefered on solaris, because it's mt-safe */
  struct timespec ts;
  
  ts.tv_sec =   usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  nanosleep(&ts, NULL);
#else
  usleep(usec);
#endif
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
  }

}

/*
 * Create a new widget_list, store the pointer in private
 * list of xitk_widget_list_t, then return the widget_list pointer.
 */
xitk_widget_list_t *xitk_widget_list_new (void) {
  xitk_widget_list_t *l;

  MUTLOCK();

  l = (xitk_widget_list_t *) xitk_xmalloc(sizeof(xitk_widget_list_t));

  l->widget_focused     = NULL;
  l->widget_under_mouse = NULL;
  l->widget_pressed     = NULL;

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

    if(fx->key == key) {

      fx->window = window;

      if(fx->xdnd && (window != None)) {
	if(!xitk_make_window_dnd_aware(fx->xdnd, window))
	  xitk_unset_dnd_callback(fx->xdnd);
      }
      
      MUTUNLOCK();
      return;
    }
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

  fx->name = (name != NULL) ? name : "NO_SET";

  fx->window    = window;
  fx->new_pos.x = 0;
  fx->new_pos.y = 0;
  fx->width     = 0;
  fx->height    = 0;
  fx->user_data = user_data;
  
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

  if(pos_cb && window)
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
		     32, PropModeAppend, (unsigned char *)&VERSION, 1);
    XUNLOCK(gXitk->display);

  }
  else
    fx->XA_XITK = None;

  MUTLOCK();

  xitk_list_append_content(gXitk->gfx, fx);

  MUTUNLOCK();

  return fx->key;
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

    if(fx->key == *key) {
      xitk_list_delete_current(gXitk->gfx); 

      *key = 0; 
      MUTUNLOCK();
      return;
    }
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);

  }

  MUTUNLOCK();
}

/*
 * Copy window information matching with key in passed window_info_t struct.
 */
int xitk_get_window_info(xitk_register_key_t key, window_info_t *winf) {
  __gfx_t  *fx;
  
  MUTLOCK();

  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
    
  while(fx) {

    if((fx->key == key) && (fx->window != None)) {
      
      winf->window = fx->window;

      if(fx->name)
	winf->name = strdup(fx->name);
      
      winf->x      = fx->new_pos.x;
      winf->y      = fx->new_pos.y;
      winf->height = fx->height;
      winf->width  = fx->width;
      
      MUTUNLOCK();
      return 1;

    }
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
  __gfx_t  *fx;
    
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);

  while(fx) {

    if(fx->window != None) {
      
      //      printf("event %d\n", event->type);

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

	  if(fx->widget_list && fx->widget_list->widget_under_mouse)
	    xitk_tips_tips_kill(fx->widget_list->widget_under_mouse);
	  
	  if(fx->widget_list && fx->widget_list->widget_focused)
	    xitk_tips_tips_kill(fx->widget_list->widget_focused);


	  if(fx->widget_list && fx->widget_list->widget_focused) {
	    w = fx->widget_list->widget_focused;
	  }

	  if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) &&
		   (mykey != XK_Tab) && (mykey != XK_KP_Tab) && (mykey != XK_ISO_Left_Tab))) {

	    if((mykey == XK_Return) || 
	       (mykey == XK_KP_Enter) || (mykey == XK_ISO_Enter) || (mykey == XK_ISO_Enter)) {
	      
	      if(w->paint)
		(w->paint) (w, fx->widget_list->win, fx->widget_list->gc);
	      
	      xitk_set_focus_to_next_widget(fx->widget_list, 0);
	    }
	    else
	      xitk_send_key_event(fx->widget_list, w, event);

	    return;
	  }
	  
	  /* set focus to next widget */
	  if((mykey == XK_Tab) || (mykey == XK_KP_Tab) || (mykey == XK_ISO_Left_Tab)) {
	    if(fx->widget_list) {
	      handled = 1;
	      xitk_set_focus_to_next_widget(fx->widget_list, (modifier & MODIFIER_SHIFT));
	    }
	  }
	  /* simulate click event on space/return/enter key event */
	  else if((mykey == XK_space) || (mykey == XK_Return) || 
		  (mykey == XK_KP_Enter) || (mykey == XK_ISO_Enter) || (mykey == XK_ISO_Enter)) {
	    if(w && (w->notify_click && w->visible && w->enable)) {
	      
	      if(w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON) ||
		       ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) ||
		       ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX))) {
		handled = 1;

		w->notify_click(fx->widget_list, w, 0, w->x, w->y);
		w->notify_click(fx->widget_list, w, 1, w->x, w->y);

		//		if(fx->xevent_callback)
		//		  fx->xevent_callback(event, fx->user_data);
	      }
	    }
	  }
	  /* move sliders */
	  else if(((mykey == XK_Left) || (mykey == XK_Right) 
		   || (mykey == XK_Up) || (mykey == XK_Down)) && (modifier == MODIFIER_NOMOD)) {
	    
	    if(w && ((w->widget_type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)) {
	      xitk_widget_t *b = xitk_browser_get_browser(fx->widget_list, w);

	      if(b) {
		if(mykey == XK_Up)
		  xitk_browser_step_down(b, NULL);
		else if(mykey == XK_Down)
		  xitk_browser_step_up(b, NULL);
	      }
	    }
	    else if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
	      
	      if((mykey == XK_Left) || (mykey == XK_Down)) {
		handled = 1;
		xitk_slider_make_backstep(fx->widget_list, w);
		xitk_slider_callback_exec(w);
	      }
	      else {
		handled = 1;
		xitk_slider_make_step(fx->widget_list, w);
		xitk_slider_callback_exec(w);
	      }
	    }

	  }
	  
	  if(!handled) {
#warning CHECKME
	    if((w == NULL) || (w && (((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) == 0))) {
	      if(fx->xevent_callback) {
		fx->xevent_callback(event, fx->user_data);
	      }
	    }
	    
	    if(w && ((w->widget_type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) && 
	       ((mykey == XK_Return) || (mykey == XK_KP_Enter) 
		|| (mykey == XK_ISO_Enter) || (mykey == XK_ISO_Enter))) {
	      
	      if(w->paint)
		(w->paint) (w, fx->widget_list->win, fx->widget_list->gc);
	      
	      xitk_set_focus_to_next_widget(fx->widget_list, 0);
	      
	    }
	  }
	  return;
	}
	break;

	case Expose:
	  if (fx->widget_list && (event->xexpose.count == 0)) {
	    xitk_paint_widget_list (fx->widget_list);
	  }
	  /*
	  else {
	    
	    if(event->xexpose.count > 0) {
	      while(XCheckWindowEvent(gXitk->display, fx->window, 
				      ExposureMask, event) == True);
	    }

	  }
	  */
	  break;
	  
	case MotionNotify: {
	  XWindowAttributes wattr;
	  Status            err;
	  
	  fx->old_event = event;
	  if (fx->move.enabled) {

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

	    if(err != BadDrawable && err != BadWindow) {
	      if(wattr.your_event_mask & PointerMotionMask) {
		
		XLOCK(gXitk->display);
		while(XCheckWindowEvent(gXitk->display, fx->window, 
					PointerMotionMask, event) == True);
		XUNLOCK(gXitk->display);
	      }
	    }

	    XLOCK(gXitk->display);
	    XSync(gXitk->display, False);
	    XUNLOCK(gXitk->display);

	  }
	  else {
	    if(fx->widget_list)
	      xitk_motion_notify_widget_list (fx->widget_list,
					      event->xbutton.x, event->xbutton.y);
	  }
	}
	break;
	
	case ButtonPress: {
	  XWindowAttributes   wattr;
	  Status              status;
	  Window              focused_window;
	  int                 revert;
	  
	  XLOCK(gXitk->display);
	  status = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	  XGetInputFocus(gXitk->display, &focused_window, &revert);
	  
	  /* 
	   * Give focus(and raise) to a window after a click, and 
	   * only if window isn't the current focused one.
	   */
	  if((status != BadDrawable) && (status != BadWindow) 
	     && (wattr.map_state == IsViewable)/* && (focused_window != fx->window)*/) {
	    XRaiseWindow(gXitk->display, fx->window);
	    XSetInputFocus(gXitk->display, fx->window, RevertToParent, CurrentTime);
	  }
	  XUNLOCK(gXitk->display);
	  
	  if(fx->widget_list) {

	    fx->move.enabled = !xitk_click_notify_widget_list (fx->widget_list, 
							       event->xbutton.x, 
							       event->xbutton.y, 0);
	    if (fx->move.enabled) {
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
	  
	  if(fx->move.enabled) {
	    fx->move.enabled = 0;
	    /* Inform application about window movement. */
	    if(fx->newpos_callback)
	      fx->newpos_callback(fx->new_pos.x, fx->new_pos.y, 
				  fx->width, fx->height);
	  }
	  else {
	    if(fx->widget_list) {
	      xitk_click_notify_widget_list (fx->widget_list, event->xbutton.x, 
					     event->xbutton.y, 1);
	    }
	  }
	  break;
	  
	case ConfigureNotify: {
	  XWindowAttributes wattr;
	  Status            err;
	  
	  XLOCK(gXitk->display);
	  err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	  XUNLOCK(gXitk->display);

	  if(err != BadDrawable && err != BadWindow) {
	    fx->width = wattr.width;
	    fx->height = wattr.height;
	  }
	  /* Inform application about window movement. */
	  if(fx->newpos_callback)
	    fx->newpos_callback(event->xconfigure.x,
				event->xconfigure.y,
				fx->width, fx->height);
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
    else {
      
      /* Don't forward event to all of windows */
      if(fx->xevent_callback 
	 && (fx->window != None && event->type != KeyRelease)) {
	fx->xevent_callback(event, fx->user_data);
      }
      
    }
    
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }
}

/*
 * Initiatization of widget internals.
 */
void xitk_init(Display *display) {


  xitk_pid = getppid();

#ifdef ENABLE_NLS
  bindtextdomain("xitk", XITK_LOCALE);
#endif  

  gXitk = (__xitk_t *) xitk_xmalloc(sizeof(__xitk_t));

  gXitk->list         = xitk_list_new();
  gXitk->gfx          = xitk_list_new();
  gXitk->display      = display;
  gXitk->key          = 0;
  gXitk->sig_callback = NULL;
  gXitk->sig_data     = NULL;
  gXitk->config       = xitk_config_init();
  
  pthread_mutex_init (&gXitk->mutex, NULL);

}

/*
 * Start widget event handling.
 * It will block till widget_stop() call
 */
void xitk_run(void) {
  XEvent            myevent;
  struct sigaction  action;
  __gfx_t          *fx;

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
  
  gXitk->running = 1;
  
  XLOCK(gXitk->display);
  XSync(gXitk->display, True);
  XUNLOCK(gXitk->display);

  /*
   * Force to repain the widget list if it exist
   */
  MUTLOCK();
  
  fx = (__gfx_t *) xitk_list_first_content(gXitk->gfx);
  
  while(fx) {
    
    if(fx->window != None && fx->widget_list) {
      XEvent xexp;
      
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
    fx = (__gfx_t *) xitk_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();

  /*
   * Now, wait for a new xevent
   */
  while(gXitk->running) {
    /* XLOCK(gXitk->display); 
       
       if(XPending (gXitk->display)) { 
    */
    XNextEvent (gXitk->display, &myevent) ;
      /* XUNLOCK(gXitk->display);  */
    xitk_xevent_notify(&myevent);
    /*
      } else {  
      XUNLOCK(gXitk->display); 
      xitk_usec_sleep(16666); // 1/60 sec
      } 
      
    */
  }
  /*XUNLOCK(gXitk->display); */

  xitk_list_free(gXitk->list);
  xitk_list_free(gXitk->gfx);
  xitk_config_deinit(gXitk->config);
  
  XITK_FREE(gXitk);
}

/*
 * Stop the wait xevent loop
 */
void xitk_stop(void) {
  gXitk->running = 0;
}
 
char *xitk_get_system_font(void) {
  return xitk_config_get_system_font(gXitk->config);
}
char *xitk_get_default_font(void) {
  return xitk_config_get_default_font(gXitk->config);
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
/*
 *
 */
char *xitk_set_locale(void) {
  char *cur_locale = NULL;
  
  if(setlocale (LC_ALL,"") == NULL) {
    XITK_WARNING("locale not supported by C library\n");
    return NULL;
  }
  
  cur_locale = setlocale(LC_ALL, NULL);
  
  return cur_locale;
}

/*
 * Return home directory.
 */
const char *xitk_get_homedir(void) {
  struct passwd *pw = NULL;
  char *homedir = NULL;
#ifdef HAVE_GETPWUID_R
  int ret;
  struct passwd pwd;
  char *buffer = NULL;
  int bufsize = 256;

  buffer = (char *) xitk_xmalloc(bufsize);
  
  if((ret = getpwuid_r(getuid(), &pwd, buffer, bufsize, &pw)) != 0) {
#else
  if((pw = getpwuid(getuid())) == NULL) {
#endif
    if((homedir = getenv("HOME")) == NULL) {
      XITK_WARNING("Unable to get home directory, set it to /tmp.\n");
      homedir = strdup("/tmp");
    }
  }
  else {
    if(pw) 
      homedir = strdup(pw->pw_dir);
  }
  
  
#ifdef HAVE_GETPWUID_R
  if(buffer) 
    XITK_FREE(buffer);
#endif
  
  return homedir;
}
