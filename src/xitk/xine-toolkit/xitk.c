/* 
 * Copyright (C) 2000-2001 the xine project
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

#define _XITK_C_ 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "widget.h"
#include "list.h"
#include "dnd.h"

#include "_xitk.h"

extern int              errno;
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

typedef void (*widget_cb_event_t)(XEvent *event, void *user_data);
typedef void (*widget_cb_newpos_t)(int, int, int, int);

typedef struct {
  Window              window;
  Atom                XA_XITK;

  gui_move_t          move;

  struct {
    int               x;
    int               y;
  } old_pos;

  struct {
    int               x;
    int               y;
  } new_pos;

  int                width;
  int                height;

  XEvent             *old_event;
  widget_list_t      *widget_list;
  char               *name;
  widget_cb_event_t   xevent_callback;
  widget_cb_newpos_t  newpos_callback;
  widgetkey_t         key;
  xitk_dnd_t         *xdnd;
  void               *user_data;
} __gfx_t;

typedef struct {
  Display            *display;
  gui_list_t         *list;
  gui_list_t         *gfx;
  pthread_mutex_t     mutex;
  int                 running;
  widgetkey_t         key;
} __xitk_t;

static __xitk_t  *gXitk;
static pid_t      xitk_pid;
static sigjmp_buf kill_jmp;


void widget_stop(void);


static void xitk_signal_handler(int sig) {
  pid_t cur_pid = getppid();

  switch (sig) {

  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
    if(cur_pid == xitk_pid) {
      __gfx_t   *fx;
      
      fprintf(stderr, "XITK killed with signal %d\n", sig);

      MUTLOCK();
      gXitk->running = 0;

      /* 
       * Tada.... and now Ladies and Gentlemen, the dirty hack
       */
      fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);

      while(fx) {
	if(fx->window) {
	  XEvent event;
	  
	  event.xany.type = ClientMessage;
	  event.xclient.type = ClientMessage;
	  event.xclient.send_event = True;
	  event.xclient.display = gXitk->display;
	  event.xclient.window = fx->window;
	  event.xclient.message_type = fx->XA_XITK;
	  event.xclient.format = 32;
	  memset(&event.xclient.data, 0, sizeof(event.xclient.data));
	  snprintf(event.xclient.data.b, 20, "%s", "KILL");
	  
	  XLOCK(gXitk->display);
	  if (!XSendEvent (gXitk->display, fx->window, True, 0L, &event)) {
	    fprintf(stderr, "XSendEvent(display, 0x%x ...) failed.\n",
		    (unsigned int) fx->window);
	  }
	  XUNLOCK(gXitk->display);

	  MUTUNLOCK();
	  siglongjmp(kill_jmp, 1);
	  return;
	}
	fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);
      }
      MUTUNLOCK();
    }
    break;
  }
}
/*
 * Create a new widget_list, store the pointer in private
 * list of widget_list_t, then return the widget_list pointer.
 */
widget_list_t *widget_list_new (void) {
  widget_list_t *l;

  MUTLOCK();

  l = (widget_list_t *) gui_xmalloc(sizeof(widget_list_t));

  gui_list_append_content(gXitk->list, l);

  MUTUNLOCK();

  return l;
}

/*
 * Change the window for the xevent_handler previously initialized
 * at widget_register_event_handler() time. It also remade it
 * DND aware, only if DND stuff was initialized at register time too.
 */
void widget_change_window_for_event_handler (widgetkey_t key, Window window) {
  __gfx_t  *fx;
  
  MUTLOCK();
      
  fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);
  
  while(fx) {

    if(fx->key == key) {

      XLOCK(gXitk->display);

      fx->window = window;

      if(fx->xdnd && (window != None))
	dnd_make_window_aware(fx->xdnd, window);
      
      XUNLOCK(gXitk->display);
      MUTUNLOCK();
      return;
    }
    fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();
}
/*
 * Register a window, with his own event handler, callback
 * for DND events, and widget list.
 */
widgetkey_t widget_register_event_handler(char *name, Window window,
					  widget_cb_event_t cb,
					  widget_cb_newpos_t pos_cb,
					  xitk_dnd_callback_t dnd_cb,
					  widget_list_t *wl, void *user_data) {
  __gfx_t   *fx;

  //  printf("%s()\n", __FUNCTION__);

  fx = (__gfx_t *) gui_xmalloc(sizeof(__gfx_t));

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
      fx->new_pos.x = wattr.x;
      fx->new_pos.y = wattr.y;
      fx->width     = wattr.width;
      fx->height    = wattr.height;
    }
  }

  if(cb)
    fx->xevent_callback = cb;
  else {
    fprintf(stderr, "%s()@%d: Callback should be non NULL\n",
	    __FUNCTION__, __LINE__);
    exit(1);
  }

  if(pos_cb && window)
    fx->newpos_callback = pos_cb;
  else
    fx->newpos_callback = NULL;
  
  if(wl)
    fx->widget_list = wl;
  else
    fx->widget_list = NULL;

  if(dnd_cb && (window != None)) {
    fx->xdnd = (xitk_dnd_t *) gui_xmalloc(sizeof(xitk_dnd_t));
    
    dnd_init_dnd(gXitk->display, fx->xdnd);
    dnd_set_callback(fx->xdnd, dnd_cb);
    dnd_make_window_aware(fx->xdnd, fx->window);
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

  gui_list_append_content(gXitk->gfx, fx);

  MUTUNLOCK();

  return fx->key;
}

/*
 * Remove from the list the window/event_handler
 * specified by the key.
 */
void widget_unregister_event_handler(widgetkey_t *key) {
  __gfx_t  *fx;

  //  printf("%s()\n", __FUNCTION__);

  MUTLOCK();

  fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);
  
  
  while(fx) {

    if(fx->key == *key) {
      gui_list_delete_current(gXitk->gfx); 

      *key = 0; 
      MUTUNLOCK();
      return;
    }
    fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);

  }
  

  MUTUNLOCK();
}

/*
 * Copy window information matching with key in passed window_info_t struct.
 */
int widget_get_window_info(widgetkey_t key, window_info_t *winf) {
  __gfx_t  *fx;
  
  MUTLOCK();

  fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);
    
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
    fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);

  }

  MUTUNLOCK();
  return 0;
}
/*
 * Here events are handled. All widget are locally
 * handled, then if a event handler callback was passed
 * at register time, it will be called.
 */
void widget_xevent_notify(XEvent *event) {
  __gfx_t  *fx;

    
  fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);

  while(fx) {

    if(fx->window != None) {

      //      printf("event %d\n", event->type);

      if(fx->window == event->xany.window) {
	
	switch(event->type) {
	case Expose:
	  if (fx->widget_list && (event->xexpose.count == 0)) {
	    paint_widget_list (fx->widget_list);
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
	    if(err != BadDrawable && err != BadWindow) {
	      if(wattr.your_event_mask & PointerMotionMask) {

		while(XCheckWindowEvent(gXitk->display, fx->window, 
					PointerMotionMask, event) == True);
	      }
	    }

	    XFlush(gXitk->display);

	    XUNLOCK(gXitk->display);

	  }
	  else {
	    if(fx->widget_list)
	      motion_notify_widget_list (fx->widget_list,
					 event->xbutton.x, event->xbutton.y);
	  }
	}
	break;
	
	case ButtonPress:
	  
	  if(fx->widget_list) {

	    fx->move.enabled = !click_notify_widget_list (fx->widget_list, 
							  event->xbutton.x, 
							  event->xbutton.y, 0);
	    if (fx->move.enabled) {
	      XWindowAttributes wattr;
	      Status            err;

	      XLOCK(gXitk->display);
	      err = XGetWindowAttributes(gXitk->display, fx->window, &wattr);
	      if(err != BadDrawable && err != BadWindow) {
		
		fx->old_pos.x = event->xmotion.x_root - event->xbutton.x;
		fx->old_pos.y = event->xmotion.y_root - event->xbutton.y;

	      }
	      
	      XUNLOCK(gXitk->display);

	      fx->move.offset_x = event->xbutton.x;
	      fx->move.offset_y = event->xbutton.y;

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
	  
	  if(fx->widget_list) {
	    click_notify_widget_list (fx->widget_list, event->xbutton.x, 
				      event->xbutton.y, 1);
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

	case ClientMessage:
	  if(fx->xdnd)
	    dnd_process_client_message(fx->xdnd, event);
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
	 && (fx->window != None && event->type != KeyPress)) {
	fx->xevent_callback(event, fx->user_data);
      }
      
    }
    
    fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);
  }
}

/*
 * Initiatization of widget internals.
 */
void widget_init(Display *display) {


  xitk_pid = getppid();

  gXitk = (__xitk_t *) gui_xmalloc(sizeof(__xitk_t));

  gXitk->list    = gui_list_new();
  gXitk->gfx     = gui_list_new();
  gXitk->display = display;
  gXitk->key     = 0;

  pthread_mutex_init (&gXitk->mutex, NULL);

}

/*
 * Start widget event handling.
 * It will block till widget_stop() call
 */
void widget_run(void) {
  XEvent            myevent;
  struct sigaction  action;
  __gfx_t          *fx;

  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGINT, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGINT) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGTERM, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGTERM) failed: %s\n", strerror(errno));
  }
  action.sa_handler = xitk_signal_handler;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  if(sigaction(SIGQUIT, &action, NULL) != 0) {
    fprintf(stderr, "sigaction(SIGQUIT) failed: %s\n", strerror(errno));
  }

  gXitk->running = 1;

  /*
   * Force to repain the widget list if it exist
   */
  MUTLOCK();

  fx = (__gfx_t *) gui_list_first_content(gXitk->gfx);
  
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
	fprintf(stderr, "XSendEvent(display, 0x%x ...) failed.\n",
		(unsigned int) fx->window);
      }
      XUNLOCK(gXitk->display);
    }
    fx = (__gfx_t *) gui_list_next_content(gXitk->gfx);
  }

  MUTUNLOCK();

  /*
   * Now, wait for a new xevent
   */
  while(gXitk->running) {
    /* XLOCK(gXitk->display); 

       if(XPending (gXitk->display)) { 
    */

    if(sigsetjmp(kill_jmp, 1) != 0)
      goto killing_end;
    
      XNextEvent (gXitk->display, &myevent) ;
      /* XUNLOCK(gXitk->display);  */
      widget_xevent_notify(&myevent);
      /*
    } else {  
      XUNLOCK(gXitk->display); 
      xine_usec_sleep(16666); // 1/60 sec
    } 

    */
  }
  /*XUNLOCK(gXitk->display); */

 killing_end:

  gui_list_free(gXitk->list);
  gui_list_free(gXitk->gfx);
  free(gXitk);
}

/*
 * Stop the wait xevent loop
 */
void widget_stop(void) {
  gXitk->running = 0;
}
