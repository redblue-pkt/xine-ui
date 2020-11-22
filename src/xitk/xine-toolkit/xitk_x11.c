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
 * xitk X11 functions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xitk.h"
#include "xitk_x11.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <xine/xineutils.h> /* xine_get_homedir() */
#include <xine/sorted_array.h>

#include "_xitk.h" /* xitk_get_bool_value */

#include "xitkintl.h"

#include "../../common/dump.h"
#include "../../common/dump_x11.h"

struct xitk_x11_s {
  xitk_t        *xitk;
};

/*
*
*/

int xitk_x11_parse_geometry(const char *geomstr, int *x, int *y, int *w, int *h) {
  int xoff, yoff, ret;
  unsigned int width, height;

  if ((ret = XParseGeometry(geomstr, &xoff, &yoff, &width, &height))) {

    if ((ret & XValue) && x)
      *x = xoff;
    if ((ret & YValue) && y)
      *y = yoff;
    if ((ret & WidthValue) && w)
      *w = width;
    if ((ret & HeightValue) && h)
      *h = height;

    return 1;
  }
  return 0;
}

void xitk_x11_xrm_parse(const char *xrm_class_name,
                        char **geometry, int *borderless,
                        char **prefered_visual, int *install_colormap) {
  Display      *display;
  char         *environment_buf = NULL;
  char         *wide_dbname;
  const char   *environment;
  char         *str_type;
  XrmDatabase   rmdb, home_rmdb, server_rmdb;
  XrmValue      value;

  XrmInitialize();

  if((display = XOpenDisplay((getenv("DISPLAY")))) == NULL)
    return;

  rmdb = home_rmdb = server_rmdb = NULL;

  wide_dbname = xitk_asprintf("%s%s", "/usr/lib/X11/app-defaults/", xrm_class_name);
  if (wide_dbname) {
    XrmDatabase application_rmdb;
    application_rmdb = XrmGetFileDatabase(wide_dbname);
    free(wide_dbname);
    if (application_rmdb)
      XrmMergeDatabases(application_rmdb, &rmdb);
  }


  if(XResourceManagerString(display) != NULL) {
    server_rmdb = XrmGetStringDatabase(XResourceManagerString(display));
  } else {
    char *user_dbname = xitk_asprintf("%s/.Xdefaults", xine_get_homedir());
    if (user_dbname)
      server_rmdb = XrmGetFileDatabase(user_dbname);
  }

  XrmMergeDatabases(server_rmdb, &rmdb);

  if ((environment = getenv("XENVIRONMENT")) == NULL) {
    char host[128] = "";

    if (gethostname(host, sizeof(host)) < 0)
      goto fail;
    environment_buf = xitk_asprintf("%s/.Xdefaults-%s", xine_get_homedir(), host);
    environment = environment_buf;
  }

  home_rmdb = XrmGetFileDatabase(environment);
  free(environment_buf);

  if (home_rmdb)
    XrmMergeDatabases(home_rmdb, &rmdb);

  if (geometry) {
    if (XrmGetResource(rmdb, "xine.geometry", "xine.Geometry", &str_type, &value) == True) {
      *geometry = strdup((const char *)value.addr);
    }
  }

  if (borderless) {
    if (XrmGetResource(rmdb, "xine.border", "xine.Border", &str_type, &value) == True) {
      *borderless = !xitk_get_bool_value((char *)value.addr);
    }
  }

  if (prefered_visual) {
    if (XrmGetResource(rmdb, "xine.visual", "xine.Visual", &str_type, &value) == True) {
      *prefered_visual = strdup((const char *)value.addr);
    }
  }

  if (install_colormap) {
    if(XrmGetResource(rmdb, "xine.colormap", "xine.Colormap", &str_type, &value) == True) {
      *install_colormap = !xitk_get_bool_value((char *)value.addr);
    }
  }

  XrmDestroyDatabase(rmdb);

 fail:
  XCloseDisplay(display);
}

static int parse_visual(VisualID *vid, int *vclass, const char *visual_str) {
  int ret = 0;
  unsigned int visual = 0;

  if (sscanf(visual_str, "%x", &visual) == 1) {
    *vid = visual;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "StaticGray") == 0) {
    *vclass = StaticGray;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "GrayScale") == 0) {
    *vclass = GrayScale;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "StaticColor") == 0) {
    *vclass = StaticColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "PseudoColor") == 0) {
    *vclass = PseudoColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "TrueColor") == 0) {
    *vclass = TrueColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "DirectColor") == 0) {
    *vclass = DirectColor;
    ret = 1;
  }

  return ret;
}

void xitk_x11_find_visual(Display *display, int screen, const char *prefered_visual,
                          Visual **visual_out, int *depth_out)
{
  XWindowAttributes  attribs;
  XVisualInfo        vinfo_tmpl;
  XVisualInfo       *vinfo;
  int                num_visuals;
  int                depth = 0;
  Visual            *visual = NULL;
  VisualID           prefered_visual_id = None;
  int                prefered_visual_class = -1;

  if (prefered_visual) {
    if (!parse_visual(&prefered_visual_id, &prefered_visual_class, prefered_visual)) {
      fprintf(stderr, "error parsing visual '%s'\n", prefered_visual);
    }
  }

  if (prefered_visual_id == None) {
    /*
     * List all available TrueColor visuals, pick the best one for xine.
     * We prefer visuals of depth 15/16 (fast).  Depth 24/32 may be OK,
     * but could be slow.
     */
    vinfo_tmpl.screen = screen;
    vinfo_tmpl.class  = (prefered_visual_class != -1 ? prefered_visual_class : TrueColor);
    vinfo = XGetVisualInfo(display, VisualScreenMask | VisualClassMask,
                           &vinfo_tmpl, &num_visuals);
    if (vinfo != NULL) {
      int i, pref;
      int best_visual_index = -1;
      int best_visual = -1;

      for (i = 0; i < num_visuals; i++) {
        if (vinfo[i].depth == 15 || vinfo[i].depth == 16)
          pref = 3;
        else if (vinfo[i].depth > 16)
          pref = 2;
        else
          pref = 1;

        if (pref > best_visual) {
          best_visual = pref;
          best_visual_index = i;
        }
      }

      if (best_visual_index != -1) {
        depth = vinfo[best_visual_index].depth;
        visual = vinfo[best_visual_index].visual;
      }

      XFree(vinfo);
    }
  } else {
    /*
     * Use the visual specified by the user.
     */
    vinfo_tmpl.visualid = prefered_visual_id;
    vinfo = XGetVisualInfo(display, VisualIDMask, &vinfo_tmpl, &num_visuals);
    if (vinfo == NULL) {
      fprintf(stderr, _("gui_main: selected visual %#lx does not exist, trying default visual\n"),
             (long) prefered_visual_id);
    } else {
      depth = vinfo[0].depth;
      visual = vinfo[0].visual;
      XFree(vinfo);
    }
  }

  if (depth == 0) {
    XVisualInfo vinfo;

    XGetWindowAttributes(display, (DefaultRootWindow(display)), &attribs);

    depth = attribs.depth;

    if (XMatchVisualInfo(display, screen, depth, TrueColor, &vinfo)) {
      visual = vinfo.visual;
    } else {
      fprintf (stderr, _("gui_main: couldn't find true color visual.\n"));

      depth = DefaultDepth (display, screen);
      visual = DefaultVisual (display, screen);
    }
  }

  if (depth_out)
    *depth_out = depth;
  if (visual_out)
    *visual_out = visual;
}

int xitk_x11_is_window_iconified(Display *display, Window window) {
  unsigned char *prop_return = NULL;
  unsigned long  nitems_return;
  unsigned long  bytes_after_return;
  int            format_return;
  Atom           type_return, atom;
  int            retval = 0;

  xitk_x_lock_display (display);
  atom = XInternAtom(display, "WM_STATE", False);
  XGetWindowProperty (display, window, atom, 0, 0x7fffffff, False,
                     atom, &type_return, &format_return, &nitems_return, &bytes_after_return, &prop_return);

  if(prop_return) {
    if (prop_return[0] == IconicState)
      retval = 1;
    XFree(prop_return);
  }
  xitk_x_unlock_display (display);

  return retval;
}

int xitk_x11_is_window_visible(Display *display, Window window) {
  XWindowAttributes  wattr;
  Status             status;

  if((display == NULL) || (window == None))
    return -1;

  xitk_x_lock_display (display);
  status = XGetWindowAttributes(display, window, &wattr);
  xitk_x_unlock_display (display);

  if((status != BadDrawable) && (status != BadWindow) && (wattr.map_state == IsViewable))
    return 1;

  return 0;
}

static int _xitk_window_set_focus (Display *display, Window window) {
  int t = 0;

  while ((!xitk_x11_is_window_visible (display, window)) && (++t < 3))
    xitk_usec_sleep (5000);

  if (xitk_x11_is_window_visible (display, window)) {
    xitk_x_lock_display (display);
    XSetInputFocus (display, window, RevertToParent, CurrentTime);
    xitk_x_unlock_display (display);
    return 0;
  }

  return -1;
}

static int _xitk_window_has_focus(Display *display, Window window) {
  Window focused_win;
  int revert;

  xitk_x_lock_display (display);
  XGetInputFocus(display, &focused_win, &revert);
  xitk_x_unlock_display (display);

  return (focused_win == window);
}

void xitk_x11_try_to_set_input_focus(Display *display, Window window) {
  int retry = 0;

  if (_xitk_window_set_focus(display, window) < 0)
    return;

  do {

    /* Retry until the WM was mercyful to give us the focus (but not indefinitely) */

    xitk_x_lock_display (display);
    XSync(display, False);
    xitk_x_unlock_display (display);

    if (_xitk_window_has_focus(display, window))
      break;

    xitk_usec_sleep(5000);

    if (_xitk_window_set_focus(display, window) < 0)
      break;

  } while (retry++ < 30);
}

/*
 * Get (safely) window pos.
 */
void xitk_x11_get_window_position(Display *display, Window window,
                                  int *x, int *y, int *width, int *height) {
  XWindowAttributes  wattr;
  Window             wdummy;
  int                xx = 0, yy = 0;

  if((display == NULL) || (window == None))
    return;

  xitk_x_lock_display (display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    wattr.width = wattr.height = 0;
    goto __failure;
  }

  (void) XTranslateCoordinates (display, window, wattr.root,
                                -wattr.border_width, -wattr.border_width,
                                &xx, &yy, &wdummy);

 __failure:

  xitk_x_unlock_display (display);

  if(x)
    *x = xx;
  if(y)
    *y = yy;
  if(width)
    *width = wattr.width;
  if(height)
    *height = wattr.height;
}

/*
 *
 */

/* Extract WM Name */
unsigned char *xitk_x11_get_wm_name (Display *display, Window win, Atom atom, Atom type_utf8) {
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

static int _x_error_handler(Display *display, XErrorEvent *xevent) {
  char buffer[2048];

  XGetErrorText(display, xevent->error_code, &buffer[0], 1023);

  XITK_WARNING("X error received: '%s'\n", buffer);

  return 0;
}

uint32_t xitk_x11_check_wm (Display *display, int verbose) {
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
  int (*old_error_handler)(Display *, XErrorEvent *);

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

  old_error_handler = XSetErrorHandler (_x_error_handler);
  XSync (display, False);

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

      if ((type_return != None) && (type_return == XA_CARDINAL) &&
          ((format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0))) {
        unsigned char   *prop_return2 = NULL;
        Window           win_id;

        win_id = *(unsigned long *)prop_return;

        status = XGetWindowProperty (display, win_id, wm_det_atoms[__WIN_S], 0,
                                     1, False, XA_CARDINAL,
                                     &type_return, &format_return, &nitems_return,
                                     &bytes_after_return, &prop_return2);

        if ((status == Success) && (type_return != None) && (type_return == XA_CARDINAL)) {
          if ((format_return == 32) && (nitems_return == 1)
              && (bytes_after_return == 0) && (win_id == *(unsigned long *)prop_return2)) {
            type |= WM_TYPE_GNOME_COMP;
          }
        }

        if (prop_return2)
          XFree(prop_return2);

        if (!(wm_name = xitk_x11_get_wm_name (display, win_id, wm_det_atoms[__NET_N], wm_det_atoms[_UTF8_S]))) {
          /*
           * Enlightenment is Gnome compliant, but don't set
           * the _NET_WM_NAME atom property
           */
            wm_name = xitk_x11_get_wm_name (display, (XDefaultRootWindow (display)), wm_det_atoms[__WIN_N], wm_det_atoms[_UTF8_S]);
        }
      }

      if (prop_return)
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

      if ((type_return != None) && (type_return == XA_WINDOW) &&
          (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {
        unsigned char   *prop_return2 = NULL;
        Window           win_id;

        win_id = *(unsigned long *)prop_return;

        status = XGetWindowProperty (display, win_id, wm_det_atoms[__NET_S], 0,
                                     1, False, XA_WINDOW,
                                     &type_return, &format_return, &nitems_return,
                                     &bytes_after_return, &prop_return2);

        if ((status == Success) && (type_return != None) && (type_return == XA_WINDOW) &&
            (format_return == 32) && (nitems_return == 1) && (bytes_after_return == 0)) {

          if (win_id == *(unsigned long *)prop_return) {
            if (wm_name)
              XFree (wm_name);
            wm_name = xitk_x11_get_wm_name (display, win_id, wm_det_atoms[__NET_N], wm_det_atoms[_UTF8_S]);
            type |= WM_TYPE_EWMH_COMP;
          }
        }
        if (prop_return2)
          XFree(prop_return2);
      }
      if (prop_return)
        XFree(prop_return);
    }
  }

  XSetErrorHandler (old_error_handler);
  XSync (display, False);

  if (verbose) {
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

static void _set_ewmh_state (Display *display, Window window, Atom atom, int enable) {
  XEvent xev;

  if ((window == None) || (atom == None))
    return;

  memset(&xev, 0, sizeof(xev));
  xev.xclient.type         = ClientMessage;
  xev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
  xev.xclient.display      = display;
  xev.xclient.window       = window;
  xev.xclient.format       = 32;
  xev.xclient.data.l[0]    = (enable == 1) ? 1 : 0;
  xev.xclient.data.l[1]    = atom;
  xev.xclient.data.l[2]    = 0l;
  xev.xclient.data.l[3]    = 0l;
  xev.xclient.data.l[4]    = 0l;

  XSendEvent (display, DefaultRootWindow (display), True, SubstructureRedirectMask, &xev);
}

void xitk_x11_set_ewmh_fullscreen(Display *display, Window window, int enable) {

  //if (!(xitk_get_wm_type(xitk) & WM_TYPE_EWMH_COMP) || (window == None))
  //  return;

  _set_ewmh_state (display, window, XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False), enable);
  _set_ewmh_state (display, window, XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False), enable);
}

/*
 *
 */

void xitk_x11_select_visual(xitk_t *xitk, Visual *gui_visual) {
  xitk_be_display_t *d = xitk->d;
  if (d->set_visual)
    d->set_visual(d, gui_visual);
}

Visual *xitk_x11_get_visual(xitk_t *xitk) {
  xitk_be_display_t *d = xitk->d;
  if (d->get_visual)
    return d->get_visual(d);
  return NULL;
}

int xitk_x11_get_depth(xitk_t *xitk) {
  xitk_be_display_t *d = xitk->d;
  if (d->get_depth)
    return d->get_depth(d);
  return 0;
}

Colormap xitk_x11_get_colormap(xitk_t *xitk) {
  xitk_be_display_t *d = xitk->d;
  if (d->get_colormap)
    return (Colormap)d->get_colormap(d);
  return None;
}

/*
 *
 */

xitk_window_t *xitk_x11_wrap_window(xitk_t *xitk, Window window) {
  if (!xitk->be || xitk->be->type != XITK_BE_TYPE_X11) {
    XITK_WARNING("Trying to wrap X11 window to non-X11 backend %d\n", xitk->be->type);
    return NULL;
  }
  if (window == None)
    return NULL;
 return xitk_window_wrap_native_window (xitk, (uintptr_t)window);
}

/*
 *
 */

int xitk_keysym_to_string(unsigned long keysym, char *buf, size_t buf_size) {
  const char *s = XKeysymToString(keysym);
  if (!s)
    return -1;
  return strlcpy(buf, s, buf_size);
}

xitk_x11_t *xitk_x11_new (xitk_t *xitk) {
  xitk_x11_t *xitk_x11;

  if (!xitk)
    return NULL;
  xitk_x11 = malloc (sizeof (*xitk_x11));
  if (!xitk_x11)
    return NULL;

  xitk_x11->xitk = xitk;
  return xitk_x11;
}

void xitk_x11_delete (xitk_x11_t *xitk_x11) {
  if (!xitk_x11)
    return;
  free (xitk_x11);
}

