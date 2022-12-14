/*
 * Copyright (C) 2000-2022 the xine project
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
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include <xine/xineutils.h> /* xine_get_homedir() */
#include <xine/sorted_array.h>

#include "_xitk.h" /* xitk_get_bool_value */

#include "xitkintl.h"

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

Display *xitk_x11_get_display(xitk_t *xitk) {
  if (xitk->d->type != XITK_BE_TYPE_X11)
    return NULL;

  return (Display *)xitk->d->id;
}

/*
 *
 */

xitk_window_t *xitk_x11_wrap_window(xitk_t *xitk, xitk_be_display_t *be_display, Window window) {
  if (xitk && (!xitk->be || xitk->be->type != XITK_BE_TYPE_X11)) {
    XITK_WARNING("Trying to wrap X11 window to non-X11 backend %d\n", xitk->be->type);
    return NULL;
  }
  if (be_display && be_display->type != XITK_BE_TYPE_X11) {
    XITK_WARNING("Tried to wrap X11 window to non-X11 backend %d\n", be_display->type);
    return NULL;
  }
  if (window == None)
    return NULL;
  return xitk_window_wrap_native_window (xitk, be_display, (uintptr_t)window);
}

/*
 *
 */

#ifdef HAVE_XF86VIDMODE
void xitk_x11_modelines_init(Display *display, xitk_x11_modelines_t *m) {

  int dummy_query_event, dummy_query_error;
  if (XF86VidModeQueryExtension (display, &dummy_query_event, &dummy_query_error)) {
    XF86VidModeModeInfo* XF86_modelines_swap;
    int                  mode, major, minor, sort_x, sort_y;
    int                  screen = XDefaultScreen(display);

    XF86VidModeQueryVersion (display, &major, &minor);
    printf (_("XF86VidMode Extension (%d.%d) detected, trying to use it.\n"), major, minor);

    if (XF86VidModeGetAllModeLines (display, screen, &m->count, &m->info)) {
      printf (_("XF86VidMode Extension: %d modelines found.\n"), m->count);

      /* first, kick off unsupported modes */
      for (mode = 1; mode < m->count; mode++) {

        if (!XF86VidModeValidateModeLine (display, screen, m->info[mode])) {
          int wrong_mode;

          printf(_("XF86VidModeModeLine %dx%d isn't valid: discarded.\n"),
                 m->info[mode]->hdisplay,
                 m->info[mode]->vdisplay);

          for (wrong_mode = mode; wrong_mode < m->count; wrong_mode++)
            m->info[wrong_mode] = m->info[wrong_mode + 1];

          m->info[wrong_mode] = NULL;
          m->count--;
          mode--;
        }
      }

      /*
       * sorting modelines, skipping first entry because it is the current
       * modeline in use - this is important so we know to which modeline
       * we have to switch to when toggling fullscreen mode.
       */
      for (sort_x = 1; sort_x < m->count; sort_x++) {

        for (sort_y = sort_x+1; sort_y < m->count; sort_y++) {

          if (m->info[sort_x]->hdisplay > m->info[sort_y]->hdisplay) {
            XF86_modelines_swap = m->info[sort_y];
            m->info[sort_y] = m->info[sort_x];
            m->info[sort_x] = XF86_modelines_swap;
          }
        }
      }
    } else {
      m->count = 0;
      printf(_("XF86VidMode Extension: could not get list of available modelines. Failed.\n"));
    }
  } else {
    printf(_("XF86VidMode Extension: initialization failed, not using it.\n"));
  }
}
#endif /* HAVE_XF86VIDMODE */

#ifdef HAVE_XF86VIDMODE
void xitk_x11_modelines_adjust(Display *display, Window window, xitk_x11_modelines_t *m, int width, int height) {
  int search = 0;

  if (m->count < 1)
    return;

  if (width > 0 && height > 0) {
    /* skipping first entry because it is the current modeline */
    for (search = 1; search < m->count; search++) {
      if (m->info[search]->hdisplay >= width)
        break;
    }
  }

  /*
   * in case we have a request for a resolution higher than any available
   * ones we take the highest currently available.
   */
  if (search >= m->count)
    search = 0;

  /* just switching to a different modeline if necessary */
  if (m->current != search) {
    if (XF86VidModeSwitchToMode (display, XDefaultScreen (display), m->info[search])) {
      m->current = search;

      // TODO
      /*
       * just in case the mouse pointer is off the visible area, move it
       * to the middle of the video window
       */
      XWarpPointer (display, None, window,
                    0, 0, 0, 0, m->info[search]->hdisplay / 2, m->info[search]->vdisplay / 2);

      XF86VidModeSetViewPort (display, XDefaultScreen (display), 0, 0);

    } else {
      //gui_msg (vwin->gui, XUI_MSG_ERROR, _("XF86VidMode Extension: modeline switching failed.\n"));
    }
  }
}
#endif /* HAVE_XF86VIDMODE */

#ifdef HAVE_XF86VIDMODE
void xitk_x11_modelines_reset(Display *display, xitk_x11_modelines_t *m) {
  /*
   * toggling from fullscreen to window mode - time to switch back to
   * the original modeline
   */

  if (m->count > 1 && m->current) {
    XF86VidModeSwitchToMode (display, XDefaultScreen (display), m->info[0]);
    XF86VidModeSetViewPort (display, XDefaultScreen (display), 0, 0);
    m->current = 0;
  }
}
#endif /* HAVE_XF86VIDMODE */

#ifdef HAVE_XF86VIDMODE
void xitk_x11_modelines_shutdown(Display *display, xitk_x11_modelines_t *m) {
  if (m->info) {
    xitk_x11_modelines_reset(display, m);
    XFree(m->info);
    m->info = NULL;
  }
}
#endif /* HAVE_XF86VIDMODE */
