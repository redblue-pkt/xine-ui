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
 *
 * Thanks to Paul Sheer for his nice xdnd implementation in cooledit.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <string.h>
#include <signal.h>

#include "_xitk.h"

#define XDND_VERSION 3

#undef DEBUG_DND

/*
 * PRIVATES
 */

static int _is_atom_match(xitk_dnd_t *xdnd, Atom **atom) {
  int i, j;
  
  for(i = 0; (*atom)[i] != 0; i++) {
    for(j = 0; j < MAX_SUPPORTED_TYPE; j++) {
      if((*atom)[i] == xdnd->supported[j])
	return i;
    }
  }
  
  return -1;
}

/*
 * Send XdndFinished to 'window' from target 'from'
 */
static void _dnd_send_finished (xitk_dnd_t *xdnd, Window window, Window from) {
  XEvent xevent;
  
  if((xdnd == NULL) || (window == None) || (from == None))
    return;

  memset(&xevent, 0, sizeof(xevent));
  xevent.xany.type                  = ClientMessage;
  xevent.xany.display               = xdnd->xitk->display;
  xevent.xclient.window             = window;
  xevent.xclient.message_type       = xdnd->_XA_XdndFinished;
  xevent.xclient.format             = 32;
  XDND_FINISHED_TARGET_WIN(&xevent) = from;
  
  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  XSendEvent (xdnd->xitk->display, window, 0, 0, &xevent);
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
}

/*
 * WARNING: X unlocked function 
 */
static int _dnd_paste_prop_internal(xitk_dnd_t *xdnd, Window from, 
                    Window insert, Atom prop, int delete_prop) {
  long           nread;
  unsigned long  nitems;
  unsigned long  bytes_after;
  
  nread = 0;
  
  do {
    Atom      actual_type;
    int       actual_fmt;
    unsigned char *buf = NULL;
    char *pbuf;

    if(XGetWindowProperty (xdnd->xitk->display, insert, prop, 
			  nread / (sizeof(unsigned char *)), 65536, delete_prop, AnyPropertyType, 
              &actual_type, &actual_fmt, &nitems, &bytes_after, &buf) != Success) {
      /* Note that per XGetWindowProperty man page, buf will always be NULL-terminated */
      if(buf)
	XFree(buf);
      return 1;
    }
    
    nread += nitems;
    
    /* Okay, got something, handle */
    pbuf = (char *)buf;
      if(strlen(pbuf)) {
	char *p;
	int   plen;
	
	/* Extract all data, '\n' separated */
	while((p = strsep(&pbuf, "\n")) != NULL) {
	  
	  plen = strlen(p) - 1;
	  
	  /* Cleanup end of string */
	  while((plen >= 0) && ((p[plen] == 10) || (p[plen] == 12) || (p[plen] == 13)))
	    p[plen--] = '\0';
	  
	  if(strlen(p)) {
#ifdef DEBUG_DND
	    printf("GOT '%s'\n", p);
#endif
	    if(xdnd->callback) {
	      xdnd->callback(p);
	    }
	  }
	}
      }
    
    XFree(buf);
  } while(bytes_after);
  
  if(!nread)
    return 1;
  
  return 0;
}

/*
 * Getting selections, using INCR if possible.
 */
static void _dnd_get_selection (xitk_dnd_t *xdnd, Window from, Atom prop, Window insert) {
  struct timeval  tv, tv_start;
  unsigned long   bytes_after;
  Atom            actual_type;
  int             actual_fmt;
  unsigned long   nitems;
  unsigned char  *s = NULL;

  if((xdnd == NULL) || (prop == None))
    return;

  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  if(XGetWindowProperty (xdnd->xitk->display, insert, prop, 0, 8, False, AnyPropertyType, 
			&actual_type, &actual_fmt, &nitems, &bytes_after, &s) != Success) {
    XFree(s);
    XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
    return;
  }
  
  XFree(s);
  
  if(actual_type != XInternAtom (xdnd->xitk->display, "INCR", False)) {
    
    (void) _dnd_paste_prop_internal(xdnd, from, insert, prop, True);
    XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
    return;
  }
  
  XDeleteProperty (xdnd->xitk->display, insert, prop);
  gettimeofday(&tv_start, 0);

  for(;;) {
    long    t;
    fd_set  r;
    XEvent  xe;

    if(XCheckMaskEvent (xdnd->xitk->display, PropertyChangeMask, &xe)) {
      if((xe.type == PropertyNotify) && (xe.xproperty.state == PropertyNewValue)) {

	/* time between arrivals of data */
	gettimeofday (&tv_start, 0);

	if(_dnd_paste_prop_internal(xdnd, from, insert, prop, True))
	  break;
      }
    } 
    else {
      tv.tv_sec  = 0;
      tv.tv_usec = 10000;
      FD_ZERO(&r);
      FD_SET (ConnectionNumber (xdnd->xitk->display), &r);
      select (ConnectionNumber (xdnd->xitk->display) + 1, &r, 0, 0, &tv);

      if (FD_ISSET (ConnectionNumber (xdnd->xitk->display), &r))
	continue;
    }
    gettimeofday(&tv, 0);
    t = (tv.tv_sec - tv_start.tv_sec) * 1000000L + (tv.tv_usec - tv_start.tv_usec);

    /* No data for five seconds, so quit */
    if(t > 5000000L) {
      XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
      return;
    }
  }

  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
}

/*
 * Get list of type from window (more than 3).
 */
static void _dnd_get_type_list (xitk_dnd_t *xdnd, Window window, Atom **typelist) {
  Atom            type, *a;
  int             format;
  unsigned long   count, remaining, i;
  unsigned char  *data = NULL;
  
  *typelist = 0;
  
  if((xdnd == NULL) || (window == None))
    return;

  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  XGetWindowProperty (xdnd->xitk->display, window, xdnd->_XA_XdndTypeList, 0, 0x8000000L, 
		     False, XA_ATOM, &type, &format, &count, &remaining, &data);
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);

  if((type != XA_ATOM) || (format != 32) || (count == 0) || (!data)) {

    if(data) {
      XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
      XFree(data);
      XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
    }

    XITK_WARNING("%s@%d: XGetWindowProperty failed in xdnd_get_type_list - "
		 "dnd->_XA_XdndTypeList = %ld\n", __FILE__, __LINE__, xdnd->_XA_XdndTypeList);
    return;
  }

  *typelist = (Atom *) calloc((count + 1), sizeof(Atom));
  a = (Atom *) data;

  for(i = 0; i < count; i++)
    (*typelist)[i] = a[i];

  (*typelist)[count] = 0;
  
  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  XFree(data);
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
}

/*
 * Get list of type from window (3).
 */
static void _dnd_get_three_types (XEvent * xevent, Atom **typelist) {
  int i;
  
  *typelist = (Atom *) xitk_xmalloc((XDND_THREE + 1) * sizeof(Atom));

  for(i = 0; i < XDND_THREE; i++)
    (*typelist)[i] = XDND_ENTER_TYPE(xevent, i);
  /* although (*typelist)[1] or (*typelist)[2] may also be set to nill */
  (*typelist)[XDND_THREE] = 0;	
}

/*
 * END OF PRIVATES
 */

/*
 * Initialize Atoms, ...
 */
void xitk_init_dnd (xitk_t *xitk, xitk_dnd_t *xdnd) {

  enum {
    XdndAware,
    XdndEnter,
    XdndLeave,
    XdndDrop,
    XdndPosition,
    XdndStatus,
    XdndSelection,
    XdndFinished,
    XdndTypeList,
    WM_DELETE_WINDOW,
    XiTkXselWindowProperty,
    _XA_ATOMS_COUNT
  };

  static const char *const prop_names[_XA_ATOMS_COUNT] = {
    "XdndAware", /* _XA_XdndAware */
    "XdndEnter", /* _XA_XdndEnter */
    "XdndLeave", /* _XA_XdndLeave */
    "XdndDrop", /* _XA_XdndDrop */
    "XdndPosition", /* _XA_XdndPosition */
    "XdndStatus", /* _XA_XdndStatus */
    "XdndSelection", /* _XA_XdndSelection */
    "XdndFinished", /* _XA_XdndFinished */
    "XdndTypeList", /* _XA_XdndTypeList */
    "WM_DELETE_WINDOW", /* _XA_WM_DELETE_WINDOW */
    "XiTKXSelWindowProperty" /* _XA_XITK_PROTOCOL_ATOM */
  };

  static const char *const mime_names[MAX_SUPPORTED_TYPE] = {
    "text/uri-list", /* supported[0] */
    "text/plain"  /* supported[1] */
  };

  Atom props[_XA_ATOMS_COUNT];

  xdnd->xitk = xitk;

  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);

  XInternAtoms (xdnd->xitk->display, (char**)prop_names, _XA_ATOMS_COUNT, False, props);
  XInternAtoms (xdnd->xitk->display, (char**)mime_names, MAX_SUPPORTED_TYPE, False, xdnd->supported);

  xdnd->_XA_XdndAware             = props[XdndAware];
  xdnd->_XA_XdndEnter             = props[XdndEnter];
  xdnd->_XA_XdndLeave             = props[XdndLeave];
  xdnd->_XA_XdndDrop              = props[XdndDrop]; 
  xdnd->_XA_XdndPosition          = props[XdndPosition];
  xdnd->_XA_XdndStatus            = props[XdndStatus];
  xdnd->_XA_XdndSelection         = props[XdndSelection];
  xdnd->_XA_XdndFinished          = props[XdndFinished];
  xdnd->_XA_XdndTypeList          = props[XdndTypeList];
  xdnd->_XA_WM_DELETE_WINDOW      = props[WM_DELETE_WINDOW];
  xdnd->_XA_XITK_PROTOCOL_ATOM    = props[XiTkXselWindowProperty];
  
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);

  xdnd->version                   = XDND_VERSION;
  xdnd->callback                  = NULL;
  xdnd->dragger_typelist          = NULL;
  xdnd->desired                   = 0;

}

/*
 * Add/Replace the XdndAware property of given window. 
 */
int xitk_make_window_dnd_aware(xitk_dnd_t *xdnd, Window window) {
  Status        status;
  
  if (!xdnd->xitk->display)
    return 0;
  
  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  status = XChangeProperty (xdnd->xitk->display, window, xdnd->_XA_XdndAware, XA_ATOM,
			   32, PropModeReplace, (unsigned char *)&xdnd->version, 1);
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
  
  if((status == BadAlloc) || (status == BadAtom) || 
     (status == BadMatch) || (status == BadValue) || (status == BadWindow)) {
    XITK_WARNING("XChangeProperty() failed.\n");
    return 0;
  }
  
  XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
  XChangeProperty (xdnd->xitk->display, window, xdnd->_XA_XdndTypeList, XA_ATOM, 32,
		  PropModeAppend, (unsigned char *)&xdnd->supported, 1);
  XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
  
  if((status == BadAlloc) || (status == BadAtom) || 
     (status == BadMatch) || (status == BadValue) || (status == BadWindow)) {
    XITK_WARNING("XChangeProperty() failed.\n");
    return 0;
  }

  xdnd->win = window;

  return 1;
}

/*
 * Set/Unset callback
 */
void xitk_set_dnd_callback(xitk_dnd_t *xdnd, xitk_dnd_callback_t cb) {
  
  if((xdnd == NULL) || (cb == NULL))
    return;

  xdnd->callback = cb;
}
void xitk_unset_dnd_callback(xitk_dnd_t *xdnd) {
  
  if(xdnd == NULL)
    return;

  xdnd->callback = NULL;
}

/*
 * Handle ClientMessage/SelectionNotify events.
 */
int xitk_process_client_dnd_message(xitk_dnd_t *xdnd, XEvent *event) {
  int retval = 0;
  
  if((xdnd == NULL) || (event == NULL))
    return 0;
  
  if(event->type == ClientMessage) {

    if((event->xclient.format == 32) && 
       (XDND_ENTER_SOURCE_WIN(event) == xdnd->_XA_WM_DELETE_WINDOW)) {
      XEvent xevent;
      
#ifdef DEBUG_DND
      printf("ClientMessage KILL\n");
#endif
    
      memset(&xevent, 0, sizeof(xevent));
      xevent.xany.type                 = DestroyNotify;
      xevent.xany.display              = xdnd->xitk->display;
      xevent.xdestroywindow.type       = DestroyNotify;
      xevent.xdestroywindow.send_event = True;
      xevent.xdestroywindow.display    = xdnd->xitk->display;
      xevent.xdestroywindow.event      = xdnd->win;
      xevent.xdestroywindow.window     = xdnd->win;
      
      XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
      XSendEvent (xdnd->xitk->display, xdnd->win, True, 0L, &xevent);
      XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
      
      retval = 1;
    } 
    else if(event->xclient.message_type == xdnd->_XA_XdndEnter) {

#ifdef DEBUG_DND
      printf("XdndEnter\n");
#endif
      
      if(XDND_ENTER_VERSION(event) < 3) {
	return 0;
      }
      
      xdnd->dragger_window   = XDND_ENTER_SOURCE_WIN(event);
      xdnd->dropper_toplevel = event->xany.window;
      xdnd->dropper_window   = None;
      
      XITK_FREE(xdnd->dragger_typelist);

      if(XDND_ENTER_THREE_TYPES(event)) {
#ifdef DEBUG_DND
	printf("Three types only\n");
#endif
	_dnd_get_three_types(event, &xdnd->dragger_typelist);
      } 
      else {
#ifdef DEBUG_DND
	printf("More than three types - getting list\n");
#endif
	_dnd_get_type_list(xdnd, xdnd->dragger_window, &xdnd->dragger_typelist);
      }

      if(xdnd->dragger_typelist) {
	int atom_match;
#ifdef DEBUG_DND
	{
	  int   i;
	  for(i = 0; xdnd->dragger_typelist[i] != 0; i++) {
            XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
	    printf("%d: '%s' ", i, XGetAtomName (xdnd->xitk->display, xdnd->dragger_typelist[i]));
            XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
	    printf("\n");
	  }
	}
#endif
	
	if((atom_match = _is_atom_match(xdnd, &xdnd->dragger_typelist)) >= 0) {
	  xdnd->desired = xdnd->dragger_typelist[atom_match];
	}

      }
      else {
	XITK_WARNING("%s@%d: xdnd->dragger_typelist is zero length!\n", __FILE__, __LINE__);
	/* Probably doesn't work */
	if ((event->xclient.data.l[1] & 1) == 0) {
	  xdnd->desired = (Atom) event->xclient.data.l[1];
	}
      }
      retval = 1;
    } 
    else if(event->xclient.message_type == xdnd->_XA_XdndLeave) {
#ifdef DEBUG_DND
      printf("XdndLeave\n");
#endif
      
      if((event->xany.window == xdnd->dropper_toplevel) && (xdnd->dropper_window != None))
	event->xany.window = xdnd->dropper_window;
      
      if(xdnd->dragger_window == XDND_LEAVE_SOURCE_WIN(event)) {
	XITK_FREE(xdnd->dragger_typelist);
	xdnd->dropper_toplevel = xdnd->dropper_window = None;
	xdnd->desired = 0;
      } 

      retval = 1;
    } 
    else if(event->xclient.message_type == xdnd->_XA_XdndDrop) {
      Window  win;
      
#ifdef DEBUG_DND
      printf("XdndDrop\n");
#endif

      if(xdnd->desired != 0) {
	
	if((event->xany.window == xdnd->dropper_toplevel) && (xdnd->dropper_window != None))
	  event->xany.window = xdnd->dropper_window;
	
	if(xdnd->dragger_window == XDND_DROP_SOURCE_WIN(event)) {

	  xdnd->time = XDND_DROP_TIME (event);
	  
          XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
	  if(!(win = XGetSelectionOwner (xdnd->xitk->display, xdnd->_XA_XdndSelection))) {
	    XITK_WARNING("%s@%d: XGetSelectionOwner() failed.\n", __FILE__, __LINE__);
            XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
	    return 0;
	  }
	  
	  XConvertSelection (xdnd->xitk->display, xdnd->_XA_XdndSelection, xdnd->desired,
			    xdnd->_XA_XITK_PROTOCOL_ATOM, xdnd->dropper_window, xdnd->time);
          XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
	}
      }
      
      _dnd_send_finished(xdnd, xdnd->dragger_window, xdnd->dropper_toplevel);
      
      retval = 1;
    } 
    else if(event->xclient.message_type == xdnd->_XA_XdndPosition) {
      XEvent  xevent;
      Window  parent, child, new_child;

#ifdef DEBUG_DND
      printf("XdndPosition\n");
#endif
      
      XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
      
      parent   = DefaultRootWindow (xdnd->xitk->display);
      child    = xdnd->dropper_toplevel;
      
      for(;;) {
	int xd, yd;
	
	new_child = None;
	if(!XTranslateCoordinates  (xdnd->xitk->display, parent, child, 
				   XDND_POSITION_ROOT_X(event), XDND_POSITION_ROOT_Y(event),
				   &xd, &yd, &new_child))
	  break;
	
	if(new_child == None)
	  break;
	
	child = new_child;
      }

      XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
      
      xdnd->dropper_window = event->xany.window = child;
      
      xdnd->x    = XDND_POSITION_ROOT_X(event);
      xdnd->y    = XDND_POSITION_ROOT_Y(event);
      xdnd->time = XDND_POSITION_TIME(event);
      
      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type            = ClientMessage;
      xevent.xany.display         = xdnd->xitk->display;
      xevent.xclient.window       = xdnd->dragger_window;
      xevent.xclient.message_type = xdnd->_XA_XdndStatus;
      xevent.xclient.format       = 32;
      
      XDND_STATUS_TARGET_WIN(&xevent) = event->xclient.window;
      XDND_STATUS_WILL_ACCEPT_SET(&xevent, True);
      XDND_STATUS_WANT_POSITION_SET(&xevent, True);
      XDND_STATUS_RECT_SET(&xevent, xdnd->x, xdnd->y, 1, 1);
      XDND_STATUS_ACTION(&xevent) = XDND_POSITION_ACTION(event);
      
      XLOCK (xdnd->xitk->x_lock_display, xdnd->xitk->display);
      XSendEvent (xdnd->xitk->display, xdnd->dragger_window, 0, 0, &xevent);
      XUNLOCK (xdnd->xitk->x_unlock_display, xdnd->xitk->display);
    }

    retval = 1;
  }
  else if(event->type == SelectionNotify) {
    
#ifdef DEBUG_DND
      printf("SelectionNotify\n");
#endif

    if(event->xselection.property == xdnd->_XA_XITK_PROTOCOL_ATOM) {
      _dnd_get_selection(xdnd, xdnd->dragger_window, 
			 event->xselection.property, event->xany.window);
      _dnd_send_finished(xdnd, xdnd->dragger_window, xdnd->dropper_toplevel);
    } 
    
    XITK_FREE(xdnd->dragger_typelist);

    retval = 1;
  }
  
  return retval;
}
