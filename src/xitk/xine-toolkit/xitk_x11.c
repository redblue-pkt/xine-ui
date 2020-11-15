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

Display *xitk_x11_open_display(int use_x_lock_display, int use_synchronized_x, int verbosity)
{
  Display *display;

  if (!XInitThreads ()) {
    printf (_("\nXInitThreads failed - looks like you don't have a thread-safe xlib.\n"));
    exit(1);
  }

  if((display = XOpenDisplay((getenv("DISPLAY")))) == NULL) {
    fprintf(stderr, _("Cannot open display\n"));
    exit(1);
  }

  if (use_synchronized_x) {
    XSynchronize (display, True);
    fprintf (stderr, _("Warning! Synchronized X activated - this is very slow...\n"));
  }

  /* Some infos */
  if(verbosity) {
    dump_host_info();
    dump_cpu_infos();
    dump_xfree_info(display, DefaultScreen(display), verbosity);
  }

  return display;
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

