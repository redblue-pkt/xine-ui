/* 
 * Copyright (C) 2000 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include "xine.h"
#include "gui_main.h"
#include "gui_widget.h"
#include "gui_dnd.h"

extern uint32_t xine_debug;

/*
Atom _XA_XdndAware;
Atom _XA_XdndEnter;
Atom _XA_XdndLeave;
Atom _XA_XdndDrop;
Atom _XA_XdndPosition;
Atom _XA_XdndStatus;
Atom _XA_XdndActionCopy;
Atom _XA_XdndSelection;
Atom _XA_XdndFinished;
Atom _XA_XINE_XDNDEXCHANGE;
//Atom atom_support;
*/

#define XDND_VERSION 3

extern gGlob_t         *gGlob;

//static gui_dnd_callback_t gui_dnd_callback;

void gui_init_dnd(DND_struct_t *xdnd) {
  
  xdnd->_XA_XdndAware = XInternAtom(gGlob->gDisplay, "XdndAware", False);
  xdnd->_XA_XdndEnter = XInternAtom(gGlob->gDisplay, "XdndEnter", False);
  xdnd->_XA_XdndLeave = XInternAtom(gGlob->gDisplay, "XdndLeave", False);
  xdnd->_XA_XdndDrop = XInternAtom(gGlob->gDisplay, "XdndDrop", False);
  xdnd->_XA_XdndPosition = XInternAtom(gGlob->gDisplay, "XdndPosition", False);
  xdnd->_XA_XdndStatus = XInternAtom(gGlob->gDisplay, "XdndStatus", False);
  xdnd->_XA_XdndActionCopy = XInternAtom(gGlob->gDisplay, "XdndActionCopy", False);
  xdnd->_XA_XdndSelection = XInternAtom(gGlob->gDisplay, "XdndSelection", False);
  xdnd->_XA_XdndFinished = XInternAtom(gGlob->gDisplay, "XdndFinished", False);

  xdnd->_XA_WM_DELETE_WINDOW = XInternAtom(gGlob->gDisplay, "WM_DELETE_WINDOW", False);

  xdnd->_XA_XINE_XDNDEXCHANGE = XInternAtom(gGlob->gDisplay, "_XINE_XDNDEXCHANGE", 
					    False);
  xdnd->version = XDND_VERSION;

  xdnd->callback = NULL;
}

void gui_make_window_dnd_aware (DND_struct_t *xdnd, Window window) {
  
  XLockDisplay (gGlob->gDisplay);
  xdnd->win = window;
  XChangeProperty (gGlob->gDisplay, xdnd->win, xdnd->_XA_XdndAware, XA_ATOM,
		   32, PropModeAppend, (unsigned char *)&xdnd->version, 1);
  XUnlockDisplay (gGlob->gDisplay);
}

Bool gui_dnd_process_selection(DND_struct_t *xdnd, XEvent *event) {
  Atom ret_type;
  int ret_format;
  unsigned long ret_item;
  unsigned long remain_byte;
  char * delme;
  XEvent xevent;
  Window selowner;

  XLockDisplay (gGlob->gDisplay);

  selowner = XGetSelectionOwner(gGlob->gDisplay, xdnd->_XA_XdndSelection);

  XGetWindowProperty(gGlob->gDisplay, event->xselection.requestor,
		     xdnd->_XA_XINE_XDNDEXCHANGE,
		     0, 65536, True, xdnd->atom_support, 
		     &ret_type, &ret_format,
		     &ret_item, &remain_byte, (unsigned char **)&delme);

  
  /*send finished*/
  memset (&xevent, 0, sizeof(xevent));
  xevent.xany.type = ClientMessage;
  xevent.xany.display = gGlob->gDisplay;
  xevent.xclient.window = selowner;
  xevent.xclient.message_type = xdnd->_XA_XdndFinished;
  xevent.xclient.format = 32;
  XDND_FINISHED_TARGET_WIN(&xevent) = event->xselection.requestor;
  XSendEvent(gGlob->gDisplay, selowner, 0, 0, &xevent);

  XUnlockDisplay (gGlob->gDisplay);
  
  if (delme) {
    int x;

    fprintf (stderr, "drop received : %s\n", delme);
    x = strlen(delme)-1;

    while ((x>=0) && ((delme[x]==10) || (delme[x]==12) || (delme[x]==13))) {
      delme[x] = 0;
      x--;
    }
      

    if (xdnd->callback)
      xdnd->callback (delme);
  }
  
  return False;
}

void gui_dnd_set_callback (DND_struct_t *xdnd, void *cb) {
  xdnd->callback = cb;
}

Bool gui_dnd_process_client_message(DND_struct_t *xdnd, XEvent *event) {

  if (event->xclient.format == 32 && event->xclient.data.l[0] == xdnd->_XA_WM_DELETE_WINDOW) {
    raise(SIGINT); /* video window closed, quit program */

  } else if (event->xclient.message_type == xdnd->_XA_XdndEnter) {

    if ((event->xclient.data.l[1] & 1) == 0){
      xdnd->atom_support = event->xclient.data.l[2];
    }
    
    return True;
  } else if (event->xclient.message_type == xdnd->_XA_XdndLeave) {
    return True;
  } else if (event->xclient.message_type == xdnd->_XA_XdndDrop) {

    if (event->xclient.data.l[0] == 
	XGetSelectionOwner(gGlob->gDisplay, xdnd->_XA_XdndSelection)){
      XLockDisplay (gGlob->gDisplay);

      XConvertSelection(gGlob->gDisplay, xdnd->_XA_XdndSelection, xdnd->atom_support,
			xdnd->_XA_XINE_XDNDEXCHANGE, event->xclient.window, 
			CurrentTime);

      XUnlockDisplay (gGlob->gDisplay);


      gui_dnd_process_selection (xdnd, event);
    }

    return True;
  } else if (event->xclient.message_type == xdnd->_XA_XdndPosition) {
    XEvent xevent;
    Window srcwin = event->xclient.data.l[0];
    
    XLockDisplay (gGlob->gDisplay);
  
    if (xdnd->atom_support != XInternAtom(gGlob->gDisplay, "text/uri-list", False)) {
      XUnlockDisplay (gGlob->gDisplay);
      return True;
    }
    
    memset (&xevent, 0, sizeof(xevent));
    xevent.xany.type = ClientMessage;
    xevent.xany.display = gGlob->gDisplay;
    xevent.xclient.window = srcwin;
    xevent.xclient.message_type = xdnd->_XA_XdndStatus;
    xevent.xclient.format = 32; 
    
    XDND_STATUS_TARGET_WIN (&xevent) = event->xclient.window;
    XDND_STATUS_WILL_ACCEPT_SET (&xevent, True);
    XDND_STATUS_WANT_POSITION_SET(&xevent, True);
    XDND_STATUS_RECT_SET(&xevent, 0, 0, 1024,768);
    XDND_STATUS_ACTION(&xevent) = xdnd->_XA_XdndActionCopy;
    
    XSendEvent(gGlob->gDisplay, srcwin, 0, 0, &xevent);

    XUnlockDisplay (gGlob->gDisplay);

    return True;
  }
  return False;
}

