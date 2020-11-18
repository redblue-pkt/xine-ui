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

#include "dnd_x11.h"

#include <stdio.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <string.h>
#include <signal.h>

#include "_xitk.h"

#define XDND_VERSION 3
#define MAX_SUPPORTED_TYPE 2

#undef DEBUG_DND

/* header was ripped from xdnd's example on its page */

#define XDND_THREE 3
#define XDND_ENTER_SOURCE_WIN(e)	((Atom)(e)->xclient.data.l[0])
#define XDND_ENTER_THREE_TYPES(e)	(((e)->xclient.data.l[1] & 0x1UL) == 0)
#define XDND_ENTER_THREE_TYPES_SET(e,b)	(e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_ENTER_VERSION(e)		((e)->xclient.data.l[1] >> 24)
#define XDND_ENTER_VERSION_SET(e,v)	(e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~(0xFF << 24)) | ((v) << 24)
#define XDND_ENTER_TYPE(e,i)		((e)->xclient.data.l[2 + i])	/* i => (0, 1, 2) */

/* XdndPosition */
#define XDND_POSITION_SOURCE_WIN(e)	((e)->xclient.data.l[0])
#define XDND_POSITION_ROOT_X(e)		((e)->xclient.data.l[2] >> 16)
#define XDND_POSITION_ROOT_Y(e)		((e)->xclient.data.l[2] & 0xFFFFUL)
#define XDND_POSITION_ROOT_SET(e,x,y)	(e)->xclient.data.l[2]  = ((x) << 16) | ((y) & 0xFFFFUL)
#define XDND_POSITION_TIME(e)		((e)->xclient.data.l[3])
#define XDND_POSITION_ACTION(e)		((e)->xclient.data.l[4])

/* XdndStatus */
#define XDND_STATUS_TARGET_WIN(e)	((e)->xclient.data.l[0])
#define XDND_STATUS_WILL_ACCEPT(e)	((e)->xclient.data.l[1] & 0x1L)
#define XDND_STATUS_WILL_ACCEPT_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_STATUS_WANT_POSITION(e)	((e)->xclient.data.l[1] & 0x2UL)
#define XDND_STATUS_WANT_POSITION_SET(e,b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x2UL) | (((b) == 0) ? 0 : 0x2UL)
#define XDND_STATUS_RECT_X(e)		((e)->xclient.data.l[2] >> 16)
#define XDND_STATUS_RECT_Y(e)		((e)->xclient.data.l[2] & 0xFFFFL)
#define XDND_STATUS_RECT_WIDTH(e)	((e)->xclient.data.l[3] >> 16)
#define XDND_STATUS_RECT_HEIGHT(e)	((e)->xclient.data.l[3] & 0xFFFFL)
#define XDND_STATUS_RECT_SET(e,x,y,w,h)	do {(e)->xclient.data.l[2] = ((x) << 16) | ((y) & 0xFFFFUL); (e)->xclient.data.l[3] = ((w) << 16) | ((h) & 0xFFFFUL); } while (0)
#define XDND_STATUS_ACTION(e)		((e)->xclient.data.l[4])

/* XdndLeave */
#define XDND_LEAVE_SOURCE_WIN(e)	((Window)(e)->xclient.data.l[0])

/* XdndDrop */
#define XDND_DROP_SOURCE_WIN(e)		((Window)(e)->xclient.data.l[0])
#define XDND_DROP_TIME(e)		((e)->xclient.data.l[2])

/* XdndFinished */
#define XDND_FINISHED_TARGET_WIN(e)	((e)->xclient.data.l[0])

static void _dnd_no_lock (Display *display) {
  (void)display;
}

typedef enum {
  XDND_INCR = 0,
  XDND_XdndAware,
  XDND_XdndEnter,
  XDND_XdndLeave,
  XDND_XdndDrop,
  XDND_XdndPosition,
  XDND_XdndStatus,
  XDND_XdndSelection,
  XDND_XdndFinished,
  XDND_XdndTypeList,
  XDND_WM_DELETE_WINDOW,
  XDND_XITK_PROTOCOL_ATOM,
  XDND_LAST
} xdnd_atom_t;

struct xitk_dnd_s {
  Display             *display;
  void                 (*lock) (Display *display);
  void                 (*unlock) (Display *display);

  int                  verbosity;

  Window               win;

  char                *ibuf, *rbuf;
  size_t               ipos, rsize;

  int                  x;
  int                  y;
  Window               dropper_toplevel;
  Window               dropper_window;
  Window               dragger_window;
  Atom                *dragger_typelist;
  Atom                 desired;
  Time                 time;

  Atom                 a[XDND_LAST];
  Atom                 supported[MAX_SUPPORTED_TYPE];
  Atom                 version;
};

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
  xevent.xany.display               = xdnd->display;
  xevent.xclient.window             = window;
  xevent.xclient.message_type       = xdnd->a[XDND_XdndFinished];
  xevent.xclient.format             = 32;
  XDND_FINISHED_TARGET_WIN(&xevent) = from;

  xdnd->lock (xdnd->display);
  XSendEvent (xdnd->display, window, 0, 0, &xevent);
  xdnd->unlock (xdnd->display);
}

static int _dnd_next_string (xitk_dnd_t *xdnd) {
  if (!xdnd->ibuf)
    return 0;
  if (!xdnd->rbuf || !xdnd->rsize) {
    XFree (xdnd->ibuf);
    xdnd->ibuf = NULL;
    xdnd->ipos = 0;
    return 1;
  }
  {
    char *b = xdnd->ibuf + xdnd->ipos;
    char *e = strchr (b, '\n'), *s;
    size_t l;
    if (!e)
      e = b + strlen (b);
    s = e;
    while ((s > b) && (!(s[-1] & 0xe0)))
      s--;
    l = s - b;
    if (l > xdnd->rsize - 1)
      l = xdnd->rsize - 1;
    memcpy (xdnd->rbuf, b, l);
    xdnd->rbuf[l] = 0;
    if (!*e) {
      XFree (xdnd->ibuf);
      xdnd->ibuf = NULL;
      xdnd->ipos = 0;
    } else {
      xdnd->ipos = e - xdnd->ibuf + 1;
    }
    return 2;
  }
}

/*
 * WARNING: X unlocked function
 */
static int _dnd_paste_prop_internal(xitk_dnd_t *xdnd, Window from,
                    Window insert, Atom prop, int delete_prop) {
  unsigned long  nitems;
  unsigned long  bytes_after;
  Atom      actual_type;
  int       actual_fmt;
  unsigned char *buf = NULL;

  if (XGetWindowProperty (xdnd->display, insert, prop, 0, 65536, delete_prop, AnyPropertyType,
    &actual_type, &actual_fmt, &nitems, &bytes_after, &buf) != Success) {
    /* Note that per XGetWindowProperty man page, buf will always be NULL-terminated */
    if (buf)
        XFree (buf);
      return 1;
  }
  xdnd->ibuf = (char *)buf;
  xdnd->ipos = 0;
  return _dnd_next_string (xdnd);
}

/*
 * Getting selections, using INCR if possible.
 */
static int _dnd_get_selection (xitk_dnd_t *xdnd, Window from, Atom prop, Window insert) {
  struct timeval  tv, tv_start;
  unsigned long   bytes_after;
  Atom            actual_type;
  int             actual_fmt;
  unsigned long   nitems;
  unsigned char  *s = NULL;
  int             ret = 1;

  if((xdnd == NULL) || (prop == None))
    return 0;

  xdnd->lock (xdnd->display);
  if(XGetWindowProperty (xdnd->display, insert, prop, 0, 8, False, AnyPropertyType,
			&actual_type, &actual_fmt, &nitems, &bytes_after, &s) != Success) {
    XFree(s);
    xdnd->unlock (xdnd->display);
    return 0;
  }

  XFree(s);

  if (actual_type != xdnd->a[XDND_INCR]) {
    ret = _dnd_paste_prop_internal(xdnd, from, insert, prop, True);
    xdnd->unlock (xdnd->display);
    return ret;
  }

  XDeleteProperty (xdnd->display, insert, prop);
  gettimeofday(&tv_start, 0);

  for(;;) {
    long    t;
    fd_set  r;
    XEvent  xe;

    if(XCheckMaskEvent (xdnd->display, PropertyChangeMask, &xe)) {
      if((xe.type == PropertyNotify) && (xe.xproperty.state == PropertyNewValue)) {

	/* time between arrivals of data */
	gettimeofday (&tv_start, 0);

        ret = _dnd_paste_prop_internal (xdnd, from, insert, prop, True);
        if (ret < 2)
          break;
      }
    }
    else {
      tv.tv_sec  = 0;
      tv.tv_usec = 10000;
      FD_ZERO(&r);
      FD_SET (ConnectionNumber (xdnd->display), &r);
      select (ConnectionNumber (xdnd->display) + 1, &r, 0, 0, &tv);

      if (FD_ISSET (ConnectionNumber (xdnd->display), &r))
	continue;
    }
    gettimeofday(&tv, 0);
    t = (tv.tv_sec - tv_start.tv_sec) * 1000000L + (tv.tv_usec - tv_start.tv_usec);

    /* No data for five seconds, so quit */
    if(t > 5000000L) {
      xdnd->unlock (xdnd->display);
      return ret;
    }
  }

  xdnd->unlock (xdnd->display);
  return ret;
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

  xdnd->lock (xdnd->display);
  XGetWindowProperty (xdnd->display, window, xdnd->a[XDND_XdndTypeList], 0, 0x8000000L,
		     False, XA_ATOM, &type, &format, &count, &remaining, &data);
  xdnd->unlock (xdnd->display);

  if((type != XA_ATOM) || (format != 32) || (count == 0) || (!data)) {

    if(data) {
      xdnd->lock (xdnd->display);
      XFree(data);
      xdnd->unlock (xdnd->display);
    }

    XITK_WARNING("%s@%d: XGetWindowProperty failed in xdnd_get_type_list - "
		 "dnd->_XA_XdndTypeList = %ld\n", __FILE__, __LINE__, xdnd->a[XDND_XdndTypeList]);
    return;
  }

  *typelist = (Atom *) calloc((count + 1), sizeof(Atom));
  a = (Atom *) data;

  for(i = 0; i < count; i++)
    (*typelist)[i] = a[i];

  (*typelist)[count] = 0;

  xdnd->lock (xdnd->display);
  XFree(data);
  xdnd->unlock (xdnd->display);
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
 * Add/Replace the XdndAware property of given window.
 */
int xitk_dnd_make_window_aware (xitk_dnd_t *xdnd, Window w) {
  Status status;

  if (!xdnd)
    return 0;

  xdnd->lock (xdnd->display);
  status = XChangeProperty (xdnd->display, w, xdnd->a[XDND_XdndAware], XA_ATOM,
    32, PropModeReplace, (unsigned char *)&xdnd->version, 1);
  xdnd->unlock (xdnd->display);

  if((status == BadAlloc) || (status == BadAtom) ||
     (status == BadMatch) || (status == BadValue) || (status == BadWindow)) {
    XITK_WARNING("XChangeProperty() failed.\n");
    return 0;
  }

  xdnd->lock (xdnd->display);
  XChangeProperty (xdnd->display, w, xdnd->a[XDND_XdndTypeList], XA_ATOM, 32,
    PropModeAppend, (unsigned char *)&xdnd->supported, 1);
  xdnd->unlock (xdnd->display);

  if((status == BadAlloc) || (status == BadAtom) ||
     (status == BadMatch) || (status == BadValue) || (status == BadWindow)) {
    XITK_WARNING("XChangeProperty() failed.\n");
    return 0;
  }

  xdnd->win = w;

  return 1;
}

xitk_dnd_t *xitk_dnd_new (Display *display, int lock, int verbosity) {
  xitk_dnd_t *xdnd;
  static const char * const prop_names[XDND_LAST] = {
    [XDND_INCR] = "INCR",
    [XDND_XdndAware] = "XdndAware",
    [XDND_XdndEnter] = "XdndEnter",
    [XDND_XdndLeave] = "XdndLeave",
    [XDND_XdndDrop] = "XdndDrop",
    [XDND_XdndPosition] = "XdndPosition",
    [XDND_XdndStatus] = "XdndStatus",
    [XDND_XdndSelection] = "XdndSelection",
    [XDND_XdndFinished] = "XdndFinished",
    [XDND_XdndTypeList] = "XdndTypeList",
    [XDND_WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
    [XDND_XITK_PROTOCOL_ATOM] = "XiTKXSelWindowProperty"
  };

  static const char * const mime_names[MAX_SUPPORTED_TYPE] = {
    "text/uri-list", /* supported[0] */
    "text/plain"  /* supported[1] */
  };

  if (!display)
    return NULL;
  xdnd = xitk_xmalloc (sizeof (*xdnd));
  if (!xdnd)
    return NULL;

  xdnd->display = display;
  xdnd->verbosity = verbosity;
  if (lock) {
    xdnd->lock = XLockDisplay;
    xdnd->unlock = XUnlockDisplay;
  } else {
    xdnd->lock =
    xdnd->unlock = _dnd_no_lock;
  }

  xdnd->lock (xdnd->display);
  XInternAtoms (xdnd->display, (char**)prop_names, XDND_LAST, False, xdnd->a);
  XInternAtoms (xdnd->display, (char**)mime_names, MAX_SUPPORTED_TYPE, False, xdnd->supported);
  xdnd->unlock (xdnd->display);

  xdnd->version = XDND_VERSION;
  xdnd->ibuf = NULL;
  xdnd->rbuf = NULL;
  xdnd->ipos = 0;
  xdnd->rsize = 0;
  xdnd->dragger_typelist = NULL;
  xdnd->desired = None;

  return xdnd;
}

void xitk_dnd_delete (xitk_dnd_t *xdnd) {
  if (!xdnd)
    return;
  if (xdnd->ibuf)
    XFree (xdnd->ibuf);
  free (xdnd);
}

/*
 * Handle ClientMessage/SelectionNotify events.
 */
int xitk_dnd_client_message (xitk_dnd_t *xdnd, XEvent *event, char *buf, size_t bsize) {
  int retval = 0;

  if (!xdnd)
    return 0;

  xdnd->rbuf = buf;
  xdnd->rsize = bsize;
  retval = _dnd_next_string (xdnd);
  if (retval)
    return retval;

  if (!event)
    return 0;

  if (event->type == ClientMessage) {

    if ((event->xclient.format == 32) && (XDND_ENTER_SOURCE_WIN(event) == xdnd->a[XDND_WM_DELETE_WINDOW])) {
      XEvent xevent;

      if (xdnd->verbosity >= 2)
        printf ("xitk.x11.dnd.kill.\n");

      memset(&xevent, 0, sizeof(xevent));
      xevent.xany.type                 = DestroyNotify;
      xevent.xany.display              = xdnd->display;
      xevent.xdestroywindow.type       = DestroyNotify;
      xevent.xdestroywindow.send_event = True;
      xevent.xdestroywindow.display    = xdnd->display;
      xevent.xdestroywindow.event      = xdnd->win;
      xevent.xdestroywindow.window     = xdnd->win;

      xdnd->lock (xdnd->display);
      XSendEvent (xdnd->display, xdnd->win, True, 0L, &xevent);
      xdnd->unlock (xdnd->display);

      retval = 1;
    }
    else if (event->xclient.message_type == xdnd->a[XDND_XdndEnter]) {

      if (xdnd->verbosity >= 2)
        printf ("xitk.x11.dnd.enter.\n");

      if (XDND_ENTER_VERSION (event) < 3)
        return 0;

      xdnd->dragger_window   = XDND_ENTER_SOURCE_WIN(event);
      xdnd->dropper_toplevel = event->xany.window;
      xdnd->dropper_window   = None;

      XITK_FREE(xdnd->dragger_typelist);

      if(XDND_ENTER_THREE_TYPES(event)) {
        if (xdnd->verbosity >= 2)
          printf ("xitk.x11.dnd.three_types.\n");
	_dnd_get_three_types(event, &xdnd->dragger_typelist);
      }
      else {
        if (xdnd->verbosity >= 2)
          printf ("xitk.x11.dnd.type_list.get.\n");
	_dnd_get_type_list(xdnd, xdnd->dragger_window, &xdnd->dragger_typelist);
      }

      if(xdnd->dragger_typelist) {
	int atom_match;
        if (xdnd->verbosity >= 2) {
          int i;
          xdnd->lock (xdnd->display);
          for (i = 0; xdnd->dragger_typelist[i] != 0; i++)
            printf("  %d: '%s'\n", i, XGetAtomName (xdnd->display, xdnd->dragger_typelist[i]));
          xdnd->unlock (xdnd->display);
	}

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
    else if(event->xclient.message_type == xdnd->a[XDND_XdndLeave]) {
      if (xdnd->verbosity >= 2)
        printf ("xitk.x11.dnd.leave.\n");

      if((event->xany.window == xdnd->dropper_toplevel) && (xdnd->dropper_window != None))
	event->xany.window = xdnd->dropper_window;

      if(xdnd->dragger_window == XDND_LEAVE_SOURCE_WIN(event)) {
	XITK_FREE(xdnd->dragger_typelist);
	xdnd->dropper_toplevel = xdnd->dropper_window = None;
	xdnd->desired = 0;
      }

      retval = 1;
    }
    else if(event->xclient.message_type == xdnd->a[XDND_XdndDrop]) {
      Window  win;

      if (xdnd->verbosity >= 2)
          printf ("xitk.x11.dnd.drop.\n");

      if(xdnd->desired != 0) {

	if((event->xany.window == xdnd->dropper_toplevel) && (xdnd->dropper_window != None))
	  event->xany.window = xdnd->dropper_window;

	if(xdnd->dragger_window == XDND_DROP_SOURCE_WIN(event)) {

	  xdnd->time = XDND_DROP_TIME (event);

          xdnd->lock (xdnd->display);
          if (!(win = XGetSelectionOwner (xdnd->display, xdnd->a[XDND_XdndSelection]))) {
	    XITK_WARNING("%s@%d: XGetSelectionOwner() failed.\n", __FILE__, __LINE__);
            xdnd->unlock (xdnd->display);
	    return 0;
	  }

          XConvertSelection (xdnd->display, xdnd->a[XDND_XdndSelection], xdnd->desired,
			    xdnd->a[XDND_XITK_PROTOCOL_ATOM], xdnd->dropper_window, xdnd->time);
          xdnd->unlock (xdnd->display);
	}
      }

      _dnd_send_finished(xdnd, xdnd->dragger_window, xdnd->dropper_toplevel);

      retval = 1;
    }
    else if(event->xclient.message_type == xdnd->a[XDND_XdndPosition]) {
      XEvent  xevent;
      Window  parent, child, new_child;

      if (xdnd->verbosity >= 3)
        printf ("xitk.x11.dnd.position.\n");

      xdnd->lock (xdnd->display);

      parent   = DefaultRootWindow (xdnd->display);
      child    = xdnd->dropper_toplevel;

      for(;;) {
	int xd, yd;

	new_child = None;
        if (!XTranslateCoordinates (xdnd->display, parent, child,
				   XDND_POSITION_ROOT_X(event), XDND_POSITION_ROOT_Y(event),
				   &xd, &yd, &new_child))
	  break;

	if(new_child == None)
	  break;

	child = new_child;
      }

      xdnd->unlock (xdnd->display);

      xdnd->dropper_window = event->xany.window = child;

      xdnd->x    = XDND_POSITION_ROOT_X(event);
      xdnd->y    = XDND_POSITION_ROOT_Y(event);
      xdnd->time = XDND_POSITION_TIME(event);

      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type            = ClientMessage;
      xevent.xany.display         = xdnd->display;
      xevent.xclient.window       = xdnd->dragger_window;
      xevent.xclient.message_type = xdnd->a[XDND_XdndStatus];
      xevent.xclient.format       = 32;

      XDND_STATUS_TARGET_WIN(&xevent) = event->xclient.window;
      XDND_STATUS_WILL_ACCEPT_SET(&xevent, True);
      XDND_STATUS_WANT_POSITION_SET(&xevent, True);
      XDND_STATUS_RECT_SET(&xevent, xdnd->x, xdnd->y, 1, 1);
      XDND_STATUS_ACTION(&xevent) = XDND_POSITION_ACTION(event);

      xdnd->lock (xdnd->display);
      XSendEvent (xdnd->display, xdnd->dragger_window, 0, 0, &xevent);
      xdnd->unlock (xdnd->display);
    }

    retval = 1;
  }
  else if(event->type == SelectionNotify) {
    retval = 1;

    if (xdnd->verbosity >= 2)
      printf ("xitk.x11.dnd.SelectionNotify\n");

    if(event->xselection.property == xdnd->a[XDND_XITK_PROTOCOL_ATOM]) {
      retval = _dnd_get_selection (xdnd, xdnd->dragger_window,
        event->xselection.property, event->xany.window);
      _dnd_send_finished(xdnd, xdnd->dragger_window, xdnd->dropper_toplevel);
    }

    XITK_FREE(xdnd->dragger_typelist);

  }

  return retval;
}
