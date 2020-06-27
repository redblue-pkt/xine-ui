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

#include "xitk_x11.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>

#include <xine/xineutils.h> /* xine_get_homedir() */

#include "_xitk.h" /* xitk_get_bool_value */

#include "xitkintl.h"

#include "../../common/dump.h"
#include "../../common/dump_x11.h"

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

/*
 *
 */

void xitk_x11_translate_xevent(XEvent *event, const xitk_event_cbs_t *cbs, void *user_data) {

  switch (event->type) {

  case DestroyNotify:
    if (cbs->destroy_notify_cb) {
      cbs->destroy_notify_cb(user_data);
    }
    break;
  case MapNotify:
    if (cbs->map_notify_cb) {
      cbs->map_notify_cb(user_data);
    }
    break;
  case Expose:
    if (cbs->expose_notify_cb) {
      xitk_expose_event_t ee = {
        .expose_count = event->xexpose.count,
        .orig_event = event,
      };
      cbs->expose_notify_cb(user_data, &ee);
    }
    break;
  case KeyPress:
  case KeyRelease:
    if (cbs->key_cb) {
      char keycode_str[256] = "", keysym_str[256] = "";
      xitk_key_event_t ke = {
        .event       = event->type == KeyPress ? XITK_KEY_PRESS : XITK_KEY_RELEASE,
        .key_pressed = xitk_get_key_pressed(event),
        .modifiers   = 0,
        .keycode     = event->xkey.keycode,
        .keysym_str  = keysym_str,
        .keycode_str = keycode_str,
      };
      // ???????
      xitk_get_key_modifier(event, &ke.modifiers);
      // ????????
      if (xitk_keysym_to_string(ke.key_pressed, keysym_str, sizeof(keysym_str)) <= 0)
        ke.keysym_str = NULL;
      if (xitk_keysym_to_string(xitk_keycode_to_keysym(event), keycode_str, sizeof(keycode_str)) <= 0)
        ke.keycode_str = NULL;
      cbs->key_cb(user_data, &ke);
    }
    break;

  case ButtonPress:
  case ButtonRelease:
    if (cbs->btn_cb) {
      xitk_button_event_t be = {
        .event     = event->type == ButtonPress ? XITK_BUTTON_PRESS : XITK_BUTTON_RELEASE,
        .button    = event->xbutton.button,
        .x         = event->xbutton.x,
        .y         = event->xbutton.y,
        .modifiers = 0,
      };
      xitk_get_key_modifier(event, &be.modifiers);
      cbs->btn_cb(user_data, &be);
    }
    break;

  case MotionNotify:
    if (cbs->motion_cb) {
      xitk_motion_event_t me = {
        .x         = event->xmotion.x,
        .y         = event->xmotion.y,
      };
      cbs->motion_cb(user_data, &me);
    }
    break;

  case ConfigureNotify:
    if (cbs->configure_notify_cb) {
      xitk_configure_event_t ce = {
        .x         = event->xconfigure.x,
        .y         = event->xconfigure.y,
        .width     = event->xconfigure.width,
        .height    = event->xconfigure.height,
      };
      cbs->configure_notify_cb(user_data, &ce);
    }
    break;
  }
}

/*
 *
 */

/*
 * Extract modifier keys.
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier) {
  unsigned int state = 0;

  *modifier = MODIFIER_NOMOD;

  if(!xev)
    return 0;

  if((xev->type == ButtonPress) || (xev->type == ButtonRelease))
    state = xev->xbutton.state;
  else if ((xev->type == KeyPress) || (xev->type == KeyRelease))
    state = xev->xkey.state;
  else
    state = xev->xkey.state;

  if(state & XK_Multi_key)
    state = (state | XK_Multi_key) & 0xFF;
  
  if(state & ShiftMask)
    *modifier |= MODIFIER_SHIFT;
  if(state & LockMask)
    *modifier |= MODIFIER_LOCK;
  if(state & ControlMask)
    *modifier |= MODIFIER_CTRL;
  if(state & Mod1Mask)
    *modifier |= MODIFIER_META;
  if(state & Mod2Mask)
    *modifier |= MODIFIER_NUML;
  if(state & Mod3Mask)
    *modifier |= MODIFIER_MOD3;
  if(state & Mod4Mask)
    *modifier |= MODIFIER_MOD4;
  if(state & Mod5Mask)
    *modifier |= MODIFIER_MOD5;
  
  return (*modifier != MODIFIER_NOMOD);
}
