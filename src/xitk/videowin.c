/*
 * Copyright (C) 2000-2020 the xine project
 *
 * This file is part of xine, a free video player.
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
 * video window handling functions
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include "common.h"
#include "videowin.h"
#include "menus.h"
#include "panel.h"
#include "tvout.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/xitk_x11.h"

typedef struct xitk_color_info_s xitk_color_info_t;
#include "xine-toolkit/_backend.h"

#include "oxine/oxine.h"

#define EST_KEEP_VALID  10	  /* #frames to allow for changing fps */
#define EST_MAX_JITTER  0.01	  /* maximum jitter to detect valid fps */
#define EST_MAX_DIFF    0.01      /* maximum diff to detect valid fps */
#define ABS(x) ((x)>0?(x):-(x))


/* Video window private structure */
struct xui_vwin_st {
  gGui_t                *gui;

  void                   (*x_lock_display) (Display *display);
  void                   (*x_unlock_display) (Display *display);

  xitk_window_t         *wrapped_window;
  int                    border_left;
  int                    border_top;

  char                   window_title[1024];
  int                    current_cursor;  /* arrow or hand */
  int                    cursor_visible;
  int                    cursor_timer;

  int                    separate_display; /* gui and video window use different displays */
  int                    wid; /* use this window */
  xitk_backend_t        *video_backend;
  xitk_be_display_t     *video_be_display;
  Display               *video_display;
  Window                 video_window;
  int                    gui_depth;
  Visual                *gui_visual;
  char                  *prefered_visual;
  Visual	        *visual;          /* Visual for video window               */
  struct {
    const char *normal, *fullscreen, *borderless;
  } res_name;
  int                    video_screen;

  int                    video_width;     /* size of currently displayed video     */
  int                    video_height;

  int                    frame_width;     /* frame size, from xine-lib */
  int                    frame_height;

  double                 video_duration;  /* frame duratrion in seconds */
  double                 video_average;   /* average frame duration in seconds */
  double                 use_duration;    /* duration used for tv mode selection */
  double                 pixel_aspect;
  int                    video_duration_valid; /* is use_duration trustable? */
  int                    win_width;       /* size of non-fullscreen window         */
  int                    win_height;
  int                    old_win_width;
  int                    old_win_height;
  int                    output_width;    /* output video window width/height      */
  int                    output_height;

  int                    stream_resize_window; /* Boolean, 1 if new stream resize output window */
  int                    zoom_small_stream; /* Boolean, 1 to double size small streams */

  int                    fullscreen_mode; /* bitfield:                                      */
  int                    fullscreen_req;  /* WINDOWED_MODE, FULLSCR_MODE or FULLSCR_XI_MODE */
  int                    fullscreen_width;
  int                    fullscreen_height;

  int                    xinerama_fullscreen_x; /* will contain paramaters for very
						   fullscreen in xinerama mode */
  int                    xinerama_fullscreen_y;
  int                    xinerama_fullscreen_width;
  int                    xinerama_fullscreen_height;

  int                    visible_width;   /* Size of currently visible portion of screen */
  int                    visible_height;  /* May differ from fullscreen_* e.g. for TV mode */
  double                 visible_aspect;  /* Pixel ratio of currently visible screen */

#ifdef HAVE_XINERAMA
  XineramaScreenInfo    *xinerama;   /* pointer to xinerama struct, or NULL */
  int                    xinerama_cnt;    /* number of screens in Xinerama */
#endif

  int                    xwin;            /* current X location */
  int                    ywin;            /* current Y location */
  int                    old_xwin;
  int                    old_ywin;

  int                    desktopWidth;    /* desktop width */
  int                    desktopHeight;   /* desktop height */
  int                    depth;
  int                    show;
  int                    borderless;      /* borderless window (for windowed mode)? */

  xitk_register_key_t    widget_key;

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension stuff */
  XF86VidModeModeInfo**  XF86_modelines;
  int                    XF86_modelines_count;
#endif

  int                    hide_on_start; /* user use '-H' arg, don't map
					   video window the first time */

  struct timeval         click_time;

  pthread_t              second_display_thread;
  int                    second_display_running;

  pthread_mutex_t        mutex;

  x11_visual_t            xine_visual;
};

static int _vwin_is_ewmh (xui_vwin_t *vwin) {
  if (vwin->video_be_display)
    return (vwin->video_be_display->wm_type & WM_TYPE_EWMH_COMP) ? 1 : 0;
  return (xitk_get_wm_type (vwin->gui->xitk) & WM_TYPE_EWMH_COMP) ? 1 : 0;
}

static void *second_display_loop (void *data);

/* safe external actions */
int video_window_is_separate_display(xui_vwin_t *vwin) {
  if (!vwin)
    return 0;
  return vwin->separate_display;
}
void video_window_set_transient_for (xui_vwin_t *vwin, xitk_window_t *xwin) {
  if (!vwin || !xwin)
    return;
  if (vwin->gui->use_root_window || vwin->separate_display)
    return;
  pthread_mutex_lock (&vwin->mutex);
  xitk_window_set_transient_for_win(xwin, vwin->wrapped_window);
  pthread_mutex_unlock (&vwin->mutex);
}

void video_window_set_input_focus(xui_vwin_t *vwin) {
  if (!vwin)
    return;

  if (xitk_window_flags (vwin->wrapped_window, 0, 0) & XITK_WINF_VISIBLE)
    xitk_window_try_to_set_input_focus(vwin->wrapped_window);
}

/*
 *
 */
static int _video_window_is_visible (xui_vwin_t *vwin) {
  static const uint8_t map[6] = {0, 1, 0, 2, 2, 2};
  int visible = xitk_window_flags (vwin->wrapped_window, 0, 0) & XITK_WINF_VISIBLE;
  /* user may have changed this via task bar. */
  vwin->show = map[vwin->show + (visible ? 3 : 0)];
  return vwin->show;
}

int video_window_is_visible (xui_vwin_t *vwin) {
  if (!vwin)
    return 0;
  /* user may have changed this via task bar. */
  pthread_mutex_lock (&vwin->mutex);
  _video_window_is_visible (vwin);
  pthread_mutex_unlock (&vwin->mutex);
  return vwin->show;
}

void video_window_grab_input_focus(xui_vwin_t *vwin) {
  if (!vwin)
    return;

  pthread_mutex_lock (&vwin->mutex);
  if (vwin->gui->cursor_grabbed) {
    video_window_grab_pointer(vwin);
  }
  if (_video_window_is_visible (vwin) > 1) {
    /* Give focus to video output window */
    xitk_window_try_to_set_input_focus (vwin->wrapped_window);
  }
  pthread_mutex_unlock (&vwin->mutex);
}

void video_window_grab_pointer(xui_vwin_t *vwin) {
  if (!vwin)
    return;

  xitk_window_flags (vwin->wrapped_window, XITK_WINF_GRAB_POINTER, XITK_WINF_GRAB_POINTER);
}

void video_window_ungrab_pointer(xui_vwin_t *vwin) {
  if (!vwin)
    return;

 xitk_window_flags (vwin->wrapped_window, XITK_WINF_GRAB_POINTER, 0);
}

static void video_window_adapt_size (xui_vwin_t *vwin);
static int  video_window_check_mag (xui_vwin_t *vwin);
static void video_window_calc_mag_win_size (xui_vwin_t *vwin, float xmag, float ymag);

static void register_event_handler(xui_vwin_t *vwin);

static void _video_window_resize_cb(void *data, xine_cfg_entry_t *cfg) {
  xui_vwin_t *vwin = data;
  vwin->stream_resize_window = cfg->num_value;
}

static void _video_window_zoom_small_cb(void *data, xine_cfg_entry_t *cfg) {
  xui_vwin_t *vwin = data;
  vwin->zoom_small_stream = cfg->num_value;
}

static void video_window_find_visual (xui_vwin_t *vwin) {
  xitk_x11_find_visual(vwin->video_display, vwin->video_screen, vwin->prefered_visual,
                       &vwin->visual, &vwin->depth);
}

/*
 * Let the video driver override the selected visual
 */
void video_window_select_visual (xui_vwin_t *vwin) {
  XVisualInfo *vinfo = (XVisualInfo *) -1;

  if (!vwin)
    return;
  if (vwin->gui->vo_port && !vwin->separate_display) {

    xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_SELECT_VISUAL, &vinfo);

    if (vinfo != (XVisualInfo *) -1) {
      if (! vinfo) {
        fprintf (stderr, _("videowin: output driver cannot select a working visual\n"));
        exit (1);
      }
      vwin->gui_visual  = vinfo->visual;
      vwin->gui_depth   = vinfo->depth;
    }
    if (vwin->gui_visual != vwin->visual) {
      printf (_("videowin: output driver overrides selected visual to visual id 0x%lx\n"), vwin->gui_visual->visualid);
      xitk_x11_select_visual (vwin->gui->xitk, vwin->gui_visual);
      vwin->gui_depth = xitk_x11_get_depth(vwin->gui->xitk);
      pthread_mutex_lock (&vwin->mutex);
      video_window_adapt_size (vwin);
      pthread_mutex_unlock (&vwin->mutex);
    }
  }
}

/*
 * Lock the video window against WM-initiated transparency changes.
 * At the time of writing (2006-06-29), only xfwm4 SVN understands this.
 * Ref. http://bugzilla.xfce.org/show_bug.cgi?id=1958
 */
static void video_window_lock_opacity (xui_vwin_t *vwin) {
  Atom opacity_lock = XInternAtom (vwin->video_display, "_NET_WM_WINDOW_OPACITY_LOCKED", False);

  /* This shouldn't happen, but was reported in bug #1573056 */
  if (opacity_lock == None)
    return;

  XChangeProperty (vwin->video_display, vwin->video_window,
		  opacity_lock,
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)vwin->gui, 1);
}

#ifdef HAVE_XF86VIDMODE
static void _adjust_modeline(xui_vwin_t *vwin) {
  /* XF86VidMode Extension
   * In case a fullscreen request is received or if already in fullscreen, the
   * appropriate modeline will be looked up and used.
   */
  if (((!(vwin->fullscreen_req & WINDOWED_MODE)) || (!(vwin->fullscreen_mode & WINDOWED_MODE)))
     && (vwin->XF86_modelines_count > 1)) {
    int search = 0;

    /* skipping first entry because it is the current modeline */
    for (search = 1; search < vwin->XF86_modelines_count; search++) {
       if (vwin->XF86_modelines[search]->hdisplay >= vwin->video_width)
         break;
    }

    /*
     * in case we have a request for a resolution higher than any available
     * ones we take the highest currently available.
     */
    if ((!(vwin->fullscreen_mode & WINDOWED_MODE)) && (search >= vwin->XF86_modelines_count))
       search = 0;

    /* just switching to a different modeline if necessary */
    if (!(search >= vwin->XF86_modelines_count)) {
       if (XF86VidModeSwitchToMode (vwin->video_display, XDefaultScreen (vwin->video_display),
                                    vwin->XF86_modelines[search])) {
#ifdef HAVE_XINERAMA
         int dummy_event, dummy_error;
#endif

          vwin->gui->XF86VidMode_fullscreen = 1;
          vwin->fullscreen_width  = vwin->XF86_modelines[search]->hdisplay;
          vwin->fullscreen_height = vwin->XF86_modelines[search]->vdisplay;

          /* update pixel aspect */
          vwin->pixel_aspect = vwin->video_be_display ? vwin->video_be_display->ratio : xitk_get_display_ratio(vwin->gui->xitk);

#ifdef HAVE_XINERAMA
          if (XineramaQueryExtension (vwin->video_display, &dummy_event, &dummy_error)) {
            int count = 1;
            XineramaScreenInfo *xsi;
            xsi = XineramaQueryScreens (vwin->video_display, &count);
            if (count > 1)
              /* multihead -> assuming square pixels */
              vwin->pixel_aspect = 1.0;
            if (xsi)
              XFree (xsi);
          }
#endif
#ifdef DEBUG
          printf ("pixel_aspect: %f\n", vwin->pixel_aspect);
#endif

          // TODO
          /*
           * just in case the mouse pointer is off the visible area, move it
           * to the middle of the video window
           */
          XWarpPointer (vwin->video_display, None, vwin->video_window,
            0, 0, 0, 0, vwin->fullscreen_width / 2, vwin->fullscreen_height / 2);

          XF86VidModeSetViewPort (vwin->video_display, XDefaultScreen (vwin->video_display), 0, 0);

          /*
           * if this is true, we are back at the original resolution, so there
           * is no need to further worry about anything.
           */
          if ((!(vwin->fullscreen_mode & WINDOWED_MODE)) && (search == 0))
            vwin->gui->XF86VidMode_fullscreen = 0;
       } else {
          xine_error (vwin->gui, _("XF86VidMode Extension: modeline switching failed.\n"));
       }
    }
  }
}
#endif/* HAVE_XF86VIDMODE */

#ifdef HAVE_XF86VIDMODE
static void _reset_modeline(xui_vwin_t *vwin) {
  /*
   * toggling from fullscreen to window mode - time to switch back to
   * the original modeline
   */
  if (vwin->XF86_modelines_count > 1) {
#ifdef HAVE_XINERAMA
    int dummy_event, dummy_error;
#endif
    vwin->x_lock_display (vwin->video_display);
    XF86VidModeSwitchToMode (vwin->video_display, XDefaultScreen (vwin->video_display), vwin->XF86_modelines[0]);
    XF86VidModeSetViewPort (vwin->video_display, XDefaultScreen (vwin->video_display), 0, 0);
    vwin->x_unlock_display (vwin->video_display);

    vwin->gui->XF86VidMode_fullscreen = 0;

    vwin->fullscreen_width  = vwin->XF86_modelines[0]->hdisplay;
    vwin->fullscreen_height = vwin->XF86_modelines[0]->vdisplay;

    /* update pixel aspect */
    vwin->pixel_aspect = vwin->video_be_display ? vwin->video_be_display->ratio : xitk_get_display_ratio(vwin->gui->xitk);
#ifdef HAVE_XINERAMA
    if (XineramaQueryExtension(vwin->video_display, &dummy_event, &dummy_error)) {
      void *info;
      int count = 1;

      info = XineramaQueryScreens(vwin->video_display, &count);
      if (count > 1)
        /* multihead -> assuming square pixels */
        vwin->pixel_aspect = 1.0;
      if (info)
        XFree(info);
    }
#endif
#ifdef DEBUG
    printf ("pixel_aspect: %f\n", vwin->pixel_aspect);
#endif
  }
}
#endif

#ifdef HAVE_XINERAMA
static void _detect_xinerama_pos_size(xui_vwin_t *vwin, XSizeHints *hint) {
  if (vwin->xinerama) {
    int           i;
    int           knowLocation = 0;
    Window        root_win, dummy_win;
    int           x_mouse,y_mouse;
    int           dummy_x,dummy_y;
    unsigned int  dummy_opts;

    /* someday this could also use the centre of the window as the
     * test point I guess.  Right now it's the upper-left.
     */
    if (vwin->video_window != None) {
      if (vwin->xwin >= 0 && vwin->ywin >= 0 &&
          vwin->xwin < vwin->desktopWidth && vwin->ywin < vwin->desktopHeight) {
        knowLocation = 1;
      }
    }

    if (vwin->fullscreen_req & FULLSCR_XI_MODE) {
      hint->x = vwin->xinerama_fullscreen_x;
      hint->y = vwin->xinerama_fullscreen_y;
      hint->width  = vwin->xinerama_fullscreen_width;
      hint->height = vwin->xinerama_fullscreen_height;
      vwin->fullscreen_width = hint->width;
      vwin->fullscreen_height = hint->height;
    }
    else {
      /* Get mouse cursor position */
      XQueryPointer (vwin->video_display, RootWindow (vwin->video_display, vwin->video_screen),
                     &root_win, &dummy_win, &x_mouse, &y_mouse, &dummy_x, &dummy_y, &dummy_opts);

      for (i = 0; i < vwin->xinerama_cnt; i++) {
        if (
            (knowLocation == 1 &&
             vwin->xwin >= vwin->xinerama[i].x_org &&
             vwin->ywin >= vwin->xinerama[i].y_org &&
             vwin->xwin < vwin->xinerama[i].x_org + vwin->xinerama[i].width &&
             vwin->ywin < vwin->xinerama[i].y_org + vwin->xinerama[i].height) ||
            (knowLocation == 0 &&
             x_mouse >= vwin->xinerama[i].x_org &&
             y_mouse >= vwin->xinerama[i].y_org &&
             x_mouse < vwin->xinerama[i].x_org + vwin->xinerama[i].width &&
             y_mouse < vwin->xinerama[i].y_org + vwin->xinerama[i].height)) {
          /*  vwin->xinerama[i].screen_number ==
              XScreenNumberOfScreen (XDefaultScreenOfDisplay (vwin->video_display)))) {*/
          hint->x = vwin->xinerama[i].x_org;
          hint->y = vwin->xinerama[i].y_org;

          if (knowLocation == 0) {
            vwin->old_xwin = hint->x;
            vwin->old_ywin = hint->y;
          }

          if (!(vwin->fullscreen_req & WINDOWED_MODE)) {
            hint->width  = vwin->xinerama[i].width;
            hint->height = vwin->xinerama[i].height;
            vwin->fullscreen_width = hint->width;
            vwin->fullscreen_height = hint->height;
          }
          else {
            hint->width  = vwin->video_width;
            hint->height = vwin->video_height;
          }
          break;
        }
      }
    }
  }
  else {
    if (!(vwin->fullscreen_req & WINDOWED_MODE)) {
      hint->width  = vwin->fullscreen_width;
      hint->height = vwin->fullscreen_height;
    } else {
      hint->width  = vwin->win_width;
      hint->height = vwin->win_height;
    }
  }
}
#endif /* HAVE_XINERAMA */

/*
 * will modify/create video output window based on
 *
 * - fullscreen flags
 * - win_width/win_height
 *
 * will set
 * output_width/output_height
 * visible_width/visible_height/visible_aspect
 */
static void video_window_adapt_size (xui_vwin_t *vwin) {
  XSizeHints            hint;
  XWMHints             *wm_hint;
  XSetWindowAttributes  attr;
  Window                old_video_window = None;
  int                   border_width;

  vwin->x_lock_display (vwin->video_display);

  {
    XColor      dummy, black;
    Colormap    colormap;
    if (vwin->separate_display) {
      video_window_find_visual (vwin);
      colormap = DefaultColormap (vwin->video_display, vwin->video_screen);
    } else {
      colormap = xitk_x11_get_colormap (vwin->gui->xitk);
    }
    XAllocNamedColor(vwin->video_display, colormap, "black", &black, &dummy);
    attr.background_pixel  = black.pixel;
    attr.border_pixel      = black.pixel;
    attr.colormap          = colormap;
  }
  hint.x = 0;
  hint.y = 0;   /* for now -- could change later */

  if (vwin->gui->use_root_window) { /* Using root window, but not really */

    vwin->xwin = vwin->ywin = 0;
    vwin->output_width    = vwin->fullscreen_width;
    vwin->output_height   = vwin->fullscreen_height;
    vwin->visible_width   = vwin->fullscreen_width;
    vwin->visible_height  = vwin->fullscreen_height;
    vwin->visible_aspect  = vwin->pixel_aspect = 1.0;

    if (vwin->video_window == None) {
      Window      wparent;
      Window      rootwindow = None;

      vwin->fullscreen_mode = FULLSCR_MODE;
      vwin->visual          = vwin->gui_visual;
      vwin->depth           = vwin->gui_depth;

      /* This couldn't happen, but we're paranoid ;-) */
      if ((rootwindow = xitk_get_desktop_root_window (vwin->video_display,
        vwin->video_screen, &wparent)) == None)
        rootwindow = DefaultRootWindow (vwin->video_display);

      attr.override_redirect = True;

      border_width = 0;

      if (vwin->wid)
        vwin->video_window = vwin->wid;
      else
        vwin->video_window = XCreateWindow (vwin->video_display, rootwindow,
          0, 0, vwin->fullscreen_width, vwin->fullscreen_height, border_width,
          CopyFromParent, CopyFromParent, CopyFromParent, CWBackPixel | CWOverrideRedirect, &attr);

      if (vwin->gui->vo_port) {
        vwin->x_unlock_display (vwin->video_display);
        xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)vwin->video_window);
        vwin->x_lock_display (vwin->video_display);
      }

      if (!(vwin->gui->no_mouse))
        XSelectInput (vwin->video_display, vwin->video_window, ExposureMask);
      else
        XSelectInput (vwin->video_display, vwin->video_window,
          ExposureMask & (~(ButtonPressMask | ButtonReleaseMask)));

      hint.flags  = USSize | USPosition | PPosition | PSize;
      hint.width  = vwin->fullscreen_width;
      hint.height = vwin->fullscreen_height;
      XSetNormalHints (vwin->video_display, vwin->video_window, &hint);

      video_window_lock_opacity (vwin);

      XClearWindow (vwin->video_display, vwin->video_window);

      XMapWindow (vwin->video_display, vwin->video_window);

      XLowerWindow (vwin->video_display, vwin->video_window);

      vwin->x_unlock_display (vwin->video_display);

      register_event_handler (vwin);
      return;
    }

    vwin->x_unlock_display (vwin->video_display);

    return;
  }

#ifdef HAVE_XF86VIDMODE
  _adjust_modeline(vwin);
#endif
#ifdef HAVE_XINERAMA
  _detect_xinerama_pos_size(vwin, &hint);
#endif

  vwin->visible_width  = vwin->fullscreen_width;
  vwin->visible_height = vwin->fullscreen_height;
  vwin->visible_aspect = vwin->pixel_aspect;

  /* Retrieve size/aspect from tvout backend, if it should be set */
  if (vwin->gui->tvout) {
    tvout_get_size_and_aspect (vwin->gui->tvout,
      &vwin->visible_width, &vwin->visible_height, &vwin->visible_aspect);
    tvout_get_size_and_aspect (vwin->gui->tvout,
      &hint.width, &hint.height, NULL);
    tvout_set_fullscreen_mode (vwin->gui->tvout,
      !(vwin->fullscreen_req & WINDOWED_MODE) ? 1 : 0, vwin->visible_width, vwin->visible_height);
  }

#ifdef HAVE_XINERAMA
  /* ask for xinerama fullscreen mode */
  if (vwin->xinerama && (vwin->fullscreen_req & FULLSCR_XI_MODE)) {

    if (vwin->video_window) {
      if ((vwin->fullscreen_mode & FULLSCR_XI_MODE) && vwin->gui_visual == vwin->visual) {
        if (vwin->visible_width != vwin->output_width || vwin->visible_height != vwin->output_height) {
          /*
           * resizing the video window may be necessary if the modeline or tv mode has
           * just been switched
           */
          XMoveResizeWindow (vwin->video_display, vwin->video_window, hint.x, hint.y,
            vwin->visible_width, vwin->visible_height);
          vwin->output_width  = vwin->visible_width;
          vwin->output_height = vwin->visible_height;
        }
        vwin->fullscreen_mode = vwin->fullscreen_req;
        vwin->x_unlock_display (vwin->video_display);

        return;
      }

      xitk_x11_get_window_position (vwin->video_display, vwin->video_window,
        &vwin->old_xwin, &vwin->old_ywin, NULL, NULL);

      old_video_window = vwin->video_window;
    }

    vwin->fullscreen_mode = vwin->fullscreen_req;
    vwin->visual   = vwin->gui_visual;
    vwin->depth    = vwin->gui_depth;

    /*
     * open fullscreen window
     */

    border_width           = 0;
    if (vwin->wid)
      vwin->video_window = vwin->wid;
    else
      vwin->video_window = XCreateWindow (vwin->video_display,
        DefaultRootWindow (vwin->video_display),
        hint.x, hint.y, vwin->visible_width, vwin->visible_height,
        border_width, vwin->depth, InputOutput, vwin->visual, CWBackPixel | CWBorderPixel | CWColormap, &attr);

    hint.win_gravity = StaticGravity;
    hint.flags  = PPosition | PSize | PWinGravity;

    vwin->output_width    = hint.width;
    vwin->output_height   = hint.height;

  } else
#endif /* HAVE_XINERAMA */
  if (!(vwin->fullscreen_req & WINDOWED_MODE)) {

    if (vwin->video_window) {

      if ((!(vwin->fullscreen_mode & WINDOWED_MODE)) && (vwin->gui_visual == vwin->visual)) {
//#ifdef HAVE_XF86VIDMODE
//	if(vwin->XF86_modelines_count > 1) {
        if ((vwin->visible_width != vwin->output_width) || (vwin->visible_height != vwin->output_height)) {
	   /*
	    * resizing the video window may be necessary if the modeline or tv mode has
	    * just been switched
	    */
          XResizeWindow (vwin->video_display, vwin->video_window,
            vwin->visible_width, vwin->visible_height);
          vwin->output_width    = vwin->visible_width;
          vwin->output_height   = vwin->visible_height;
	}
//#endif
        vwin->fullscreen_mode = vwin->fullscreen_req;
	vwin->x_unlock_display (vwin->video_display);

	return;
      }

      xitk_x11_get_window_position (vwin->video_display, vwin->video_window,
        &vwin->old_xwin, &vwin->old_ywin, NULL, NULL);

      old_video_window = vwin->video_window;
    }

    vwin->fullscreen_mode = vwin->fullscreen_req;
    vwin->visual   = vwin->gui_visual;
    vwin->depth    = vwin->gui_depth;

    /*
     * open fullscreen window
     */

    border_width           = 0;

    if (vwin->wid)
      vwin->video_window = vwin->wid;
    else
      vwin->video_window = XCreateWindow (vwin->video_display,
        DefaultRootWindow(vwin->video_display),
        hint.x, hint.y, vwin->visible_width, vwin->visible_height,
        border_width, vwin->depth, InputOutput, vwin->visual,
        CWBackPixel | CWBorderPixel | CWColormap, &attr);

#ifndef HAVE_XINERAMA
    hint.width  = vwin->visible_width;
    hint.height = vwin->visible_height;
#endif
    hint.win_gravity = StaticGravity;
    hint.flags  = PPosition | PSize | PWinGravity;

    vwin->output_width    = hint.width;
    vwin->output_height   = hint.height;

  }
  else {
#ifndef HAVE_XINERAMA
    hint.width       = vwin->win_width;
    hint.height      = vwin->win_height;
#endif
    hint.flags       = PPosition | PSize;

    /*
     * user sets window geom, move back to original location.
     */
    if (vwin->stream_resize_window == 0) {
      hint.x = vwin->old_xwin;
      hint.y = vwin->old_ywin;
    }

    vwin->old_win_width  = hint.width;
    vwin->old_win_height = hint.height;

    vwin->output_width  = hint.width;
    vwin->output_height = hint.height;

    if (vwin->video_window) {

      if ((!(vwin->fullscreen_mode & WINDOWED_MODE)) || (vwin->visual != vwin->gui_visual)) {
#ifdef HAVE_XF86VIDMODE
        _reset_modeline(vwin);
#endif
        old_video_window = vwin->video_window;
      }
      else {

	/* Update window size hints with the new size */
        XSetNormalHints (vwin->video_display, vwin->video_window, &hint);

        XResizeWindow (vwin->video_display, vwin->video_window,
          vwin->win_width, vwin->win_height);

        vwin->x_unlock_display (vwin->video_display);

	return;
      }
    }

    vwin->fullscreen_mode   = WINDOWED_MODE;
    vwin->visual            = vwin->gui_visual;
    vwin->depth             = vwin->gui_depth;

    if (vwin->borderless)
      border_width = 0;
    else
      border_width = 4;

    if (vwin->wid)
      vwin->video_window = vwin->wid;
    else
      vwin->video_window = XCreateWindow (vwin->video_display,
        DefaultRootWindow (vwin->video_display),
        hint.x, hint.y, hint.width, hint.height, border_width,
        vwin->depth, InputOutput, vwin->visual, CWBackPixel | CWBorderPixel | CWColormap, &attr);
  }

  vwin->x_unlock_display (vwin->video_display);

  if (vwin->gui->vo_port) {
    xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_DRAWABLE_CHANGED, (void*)vwin->video_window);
  }

  if (!(vwin->fullscreen_req & WINDOWED_MODE)) {
    xitk_window_set_window_class (vwin->wrapped_window, vwin->res_name.fullscreen, "xine");
  } else if (vwin->borderless) {
    xitk_window_set_window_class (vwin->wrapped_window, vwin->res_name.borderless, "xine");
  } else {
    xitk_window_set_window_class (vwin->wrapped_window, vwin->res_name.normal, "xine");
  }

  vwin->x_lock_display (vwin->video_display);

  XSetWMNormalHints (vwin->video_display, vwin->video_window, &hint);

  video_window_lock_opacity (vwin);

  if (!(vwin->fullscreen_req & WINDOWED_MODE) || vwin->borderless) {
    xitk_window_flags (vwin->wrapped_window, XITK_WINF_DECORATED, 0);
  }

  if (!(vwin->gui->no_mouse))
    XSelectInput (vwin->video_display, vwin->video_window, INPUT_MOTION | KeymapStateMask);
  else
    XSelectInput (vwin->video_display, vwin->video_window,
      (INPUT_MOTION | KeymapStateMask) & (~(ButtonPressMask | ButtonReleaseMask)));

  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input = True;
    wm_hint->initial_state = NormalState;
    wm_hint->icon_pixmap = xitk_image_get_pixmap (vwin->gui->icon);
    wm_hint->icon_mask = xitk_image_get_mask (vwin->gui->icon);
    wm_hint->flags = InputHint | StateHint | IconPixmapHint | IconMaskHint;
    XSetWMHints (vwin->video_display, vwin->video_window, wm_hint);
    XFree(wm_hint);
  }

  register_event_handler (vwin); /* avoid destroy notify from old window (triggers exit) */

  if (vwin->hide_on_start == 1) {
    vwin->hide_on_start = -1;
    vwin->show = 0;
  }
  else {
    /* Map window. */

    if ((vwin->gui->always_layer_above ||
      ((!(vwin->fullscreen_mode & WINDOWED_MODE)) && is_layer_above (vwin->gui))) && !_vwin_is_ewmh (vwin)) {
      xitk_window_set_layer_above(vwin->wrapped_window);
    }

    xitk_window_raise_window(vwin->wrapped_window);
    xitk_window_flags (vwin->wrapped_window, XITK_WINF_VISIBLE, XITK_WINF_VISIBLE);

    while (!xitk_x11_is_window_visible (vwin->video_display, vwin->video_window))
      xine_usec_sleep(5000);

    if ((vwin->gui->always_layer_above ||
      ((!(vwin->fullscreen_mode & WINDOWED_MODE)) && is_layer_above (vwin->gui))) && _vwin_is_ewmh (vwin)) {
      xitk_window_set_layer_above(vwin->wrapped_window);
    }

    /* inform the window manager that we are fullscreen. This info musn't be set for xinerama-fullscreen,
       otherwise there are 2 different window size for one fullscreen mode ! (kwin doesn't accept this) */
    if (!(vwin->fullscreen_mode & (WINDOWED_MODE | FULLSCR_XI_MODE))) {
      xitk_window_flags (vwin->wrapped_window, XITK_WINF_FULLSCREEN, XITK_WINF_FULLSCREEN);
    }
  }

  XSync (vwin->video_display, False);

  if ((!(vwin->fullscreen_mode & WINDOWED_MODE))) {
    /* Waiting for visibility, avoid X error on some cases */

    video_window_set_input_focus(vwin);

#ifdef HAVE_XINERAMA
    if (vwin->xinerama)
      XMoveWindow (vwin->video_display, vwin->video_window, hint.x, hint.y);
    else
      XMoveWindow (vwin->video_display, vwin->video_window, 0, 0);
#else
    XMoveWindow (vwin->video_display, vwin->video_window, 0, 0);
#endif
  }

  vwin->x_unlock_display (vwin->video_display);

  /* The old window should be destroyed now */
  if(old_video_window != None) {
    vwin->x_lock_display (vwin->video_display);
    XDestroyWindow (vwin->video_display, old_video_window);
    vwin->x_unlock_display (vwin->video_display);

    if (vwin->gui->cursor_grabbed)
      video_window_grab_pointer(vwin);
  }

  /* take care about window decoration/pos */
  {
    Window tmp_win;
    int x = vwin->xwin < 0 ? 0 : vwin->xwin;
    int y = vwin->ywin < 0 ? 0 : vwin->ywin;

    vwin->x_lock_display (vwin->video_display);
    XTranslateCoordinates (vwin->video_display, vwin->video_window,
      DefaultRootWindow (vwin->video_display), 0, 0, &vwin->xwin, &vwin->ywin, &tmp_win);
    vwin->x_unlock_display (vwin->video_display);
    x = vwin->xwin - x;
    y = vwin->ywin - y;
    if ((x < 0) || (x > 32))
      x = 0;
    if ((y < 0) || (y > 80))
      y = 0;
    if (x > 0)
      vwin->border_left = x;
    if (y > 0)
      vwin->border_top  = y;
    xitk_window_set_border_size (vwin->gui->xitk, vwin->widget_key,
      vwin->borderless ? 0 : vwin->border_left,
      vwin->borderless ? 0 : vwin->border_top);
  }

  oxine_adapt();
}

static void get_default_mag (xui_vwin_t *vwin, int video_width, int video_height, float *xmag, float *ymag) {
  if (vwin->zoom_small_stream && video_width < 300 && video_height < 300)
    *xmag = *ymag = 2.0f;
  else
    *xmag = *ymag = 1.0f;
}

/*
 *
 */
static void _video_window_dest_size_cb (void *data,
                                        int video_width, int video_height,
                                        double video_pixel_aspect,
                                        int *dest_width, int *dest_height,
                                        double *dest_pixel_aspect)  {
  xui_vwin_t *vwin = data;

  if (!vwin)
    return;
  pthread_mutex_lock (&vwin->mutex);

  vwin->frame_width = video_width;
  vwin->frame_height = video_height;

  /* correct size with video_pixel_aspect */
  if (video_pixel_aspect >= vwin->pixel_aspect)
    video_width  = video_width * video_pixel_aspect / vwin->pixel_aspect + .5;
  else
    video_height = video_height * vwin->pixel_aspect / video_pixel_aspect + .5;

  if (vwin->stream_resize_window && (vwin->fullscreen_mode & WINDOWED_MODE)) {

    if (vwin->video_width != video_width || vwin->video_height != video_height) {

      if ((video_width > 0) && (video_height > 0)) {
	float xmag, ymag;

        get_default_mag (vwin, video_width, video_height, &xmag, &ymag);

        /* FIXME: this is supposed to give the same results as if a
         * video_window_set_mag(xmag, ymag) was called. Since video_window_adapt_size()
         * check several other details (like border, xinerama, etc) this
         * may produce wrong values in some cases. (?)
         */
        *dest_width  = (int) ((float) video_width * xmag + 0.5f);
        *dest_height = (int) ((float) video_height * ymag + 0.5f);
        *dest_pixel_aspect = vwin->pixel_aspect;
        pthread_mutex_unlock (&vwin->mutex);
        return;
      }
    }
  }

  if (!(vwin->fullscreen_mode & WINDOWED_MODE)) {
    *dest_width  = vwin->visible_width;
    *dest_height = vwin->visible_height;
    *dest_pixel_aspect = vwin->visible_aspect;
  } else {
    *dest_width  = vwin->output_width;
    *dest_height = vwin->output_height;
    *dest_pixel_aspect = vwin->pixel_aspect;
  }

  pthread_mutex_unlock (&vwin->mutex);
}

/*
 *
 */
static void _video_window_frame_output_cb (void *data,
                                           int video_width, int video_height,
                                           double video_pixel_aspect,
                                           int *dest_x, int *dest_y,
                                           int *dest_width, int *dest_height,
                                           double *dest_pixel_aspect,
                                           int *win_x, int *win_y) {
  xui_vwin_t *vwin = data;

  if (!vwin)
    return;
  pthread_mutex_lock (&vwin->mutex);

  vwin->frame_width = video_width;
  vwin->frame_height = video_height;

  /* correct size with video_pixel_aspect */
  if (video_pixel_aspect >= vwin->pixel_aspect)
    video_width  = video_width * video_pixel_aspect / vwin->pixel_aspect + .5;
  else
    video_height = video_height * vwin->pixel_aspect / video_pixel_aspect + .5;

  /* Please do NOT remove, support will be added soon! */
#if 0
  double jitter;
  vwin->video_duration = video_duration;
  vwin->video_average  = 0.5 * vwin->video_average + 0.5 video_duration;
  jitter = ABS (video_duration - vwin->video_average) / vwin->video_average;
  if (jitter > EST_MAX_JITTER) {
    if (vwin->duration_valid > -EST_KEEP_VALID)
      vwin->duration_valid--;
  } else {
    if (vwin->duration_valid < EST_KEEP_VALID)
      vwin->duration_valid++;
    if (ABS (video_duration - vwin->use_duration) / video_duration > EST_MAX_DIFF)
      vwin->use_duration = video_duration;
  }
#endif

  if (vwin->video_width != video_width || vwin->video_height != video_height) {

    vwin->video_width  = video_width;
    vwin->video_height = video_height;

    if (vwin->stream_resize_window && video_width > 0 && video_height > 0) {
      float xmag, ymag;

      /* Prepare window size */
      get_default_mag (vwin, video_width, video_height, &xmag, &ymag);
      video_window_calc_mag_win_size (vwin, xmag, ymag);
      /* If actually ready to adapt window size, do it now */
      if (video_window_check_mag (vwin))
        video_window_adapt_size (vwin);
    }

    oxine_adapt();
  }

  *dest_x = 0;
  *dest_y = 0;

  if (!(vwin->fullscreen_mode & WINDOWED_MODE)) {
    *dest_width  = vwin->visible_width;
    *dest_height = vwin->visible_height;
    *dest_pixel_aspect = vwin->visible_aspect;
    /* TODO: check video size/fps/ar if tv mode and call video_window_adapt_size if necessary */
  } else {
    *dest_width  = vwin->output_width;
    *dest_height = vwin->output_height;
    *dest_pixel_aspect = vwin->pixel_aspect;
  }

  *win_x = (vwin->xwin < 0) ? 0 : vwin->xwin;
  *win_y = (vwin->ywin < 0) ? 0 : vwin->ywin;

  pthread_mutex_unlock (&vwin->mutex);
}

void *video_window_get_xine_visual(xui_vwin_t *vwin, int *visual_type) {

  if (vwin) {
    x11_visual_t *v = &vwin->xine_visual;

    *visual_type = XINE_VISUAL_TYPE_X11;

    v->display           = vwin->video_display;
    v->screen            = vwin->video_screen;
    v->d                 = vwin->video_window;
    v->dest_size_cb      = _video_window_dest_size_cb;
    v->frame_output_cb   = _video_window_frame_output_cb;
    v->user_data         = vwin;
    return v;
  }
  return NULL;
}


/*
 *
 */
void video_window_set_fullscreen_mode (xui_vwin_t *vwin, int req_fullscreen) {
  if (!vwin)
    return;
  pthread_mutex_lock (&vwin->mutex);

  if (!(vwin->fullscreen_mode & req_fullscreen)) {

#ifdef HAVE_XINERAMA
    if ((req_fullscreen & FULLSCR_XI_MODE) && (!vwin->xinerama)) {
      if (vwin->fullscreen_mode & FULLSCR_MODE)
        vwin->fullscreen_req = WINDOWED_MODE;
      else
        vwin->fullscreen_req = FULLSCR_MODE;
    }
    else
#endif
      vwin->fullscreen_req = req_fullscreen;

  }
  else {

    if ((req_fullscreen & FULLSCR_MODE) && (vwin->fullscreen_mode & FULLSCR_MODE))
      vwin->fullscreen_req = WINDOWED_MODE;
#ifdef HAVE_XINERAMA
    else if ((req_fullscreen & FULLSCR_XI_MODE) && (vwin->fullscreen_mode & FULLSCR_XI_MODE))
      vwin->fullscreen_req = WINDOWED_MODE;
#endif

  }

  video_window_adapt_size (vwin);

  video_window_set_input_focus (vwin);
  osd_update_osd();

  pthread_mutex_unlock (&vwin->mutex);
}

/*
 *
 */
int video_window_get_fullscreen_mode (xui_vwin_t *vwin) {
  return vwin ? vwin->fullscreen_mode : 0;
}

#if 0
/*
 * set/reset xine in xinerama fullscreen
 * ie: try to expend display on further screens
 */
void video_window_set_xinerama_fullscreen_mode (xui_vwin_t *vwin, int req_fullscreen) {
  if (!vwin)
    return;

  pthread_mutex_lock (&vwin->mutex);
  vwin->fullscreen_req = req_fullscreen;
  video_window_adapt_size (vwin);
  pthread_mutex_unlock (&vwin->mutex);
}

/*
 *
 */
int video_window_get_xinerama_fullscreen_mode (xui_vwin_t *vwin) {
  return vwin ? vwin->fullscreen_mode : 0;
}
#endif

/*
 * Set cursor
 */
void video_window_set_cursor (xui_vwin_t *vwin, int cursor) {

  if (vwin && cursor) {
    vwin->current_cursor = cursor;
    if (vwin->cursor_visible) {
      vwin->cursor_timer = 0;
      switch (vwin->current_cursor) {
      case 0:
        xitk_window_define_window_cursor (vwin->wrapped_window, xitk_cursor_invisible);
        break;
      case CURSOR_ARROW:
        xitk_window_restore_window_cursor (vwin->wrapped_window);
        break;
      case CURSOR_HAND:
        xitk_window_define_window_cursor (vwin->wrapped_window, xitk_cursor_hand2);
        break;
      }
    }
  }

}

/*
 * hide/show cursor in video window
 */
void video_window_set_cursor_visibility (xui_vwin_t *vwin, int show_cursor) {

  if (!vwin)
    return;
  if (vwin->gui->use_root_window)
    return;

  vwin->cursor_visible = show_cursor;

    if (show_cursor) {
      vwin->cursor_timer = 0;
      if (vwin->current_cursor == CURSOR_ARROW)
        xitk_window_restore_window_cursor (vwin->wrapped_window);
      else
        xitk_window_define_window_cursor (vwin->wrapped_window, xitk_cursor_hand1);
    }
    else
      xitk_window_define_window_cursor (vwin->wrapped_window, xitk_cursor_invisible);
}

/*
 * Get cursor visiblity (boolean)
 */

int video_window_get_cursor_timer (xui_vwin_t *vwin) {
  return vwin ? vwin->cursor_timer : 0;
}

void video_window_set_cursor_timer (xui_vwin_t *vwin, int timer) {
  if (vwin)
    vwin->cursor_timer = timer;
}

/*
 * hide/show video window
 */
void video_window_set_visibility (xui_vwin_t *vwin, int show_window) {
#if defined (WEIRD_DEBUG)
  sleep (2);
  XIconifyWindow (vwin->video_display, vwin->video_window, XDefaultScreen (vwin->video_display));
  XSync (vwin->video_display, False);
  printf ("video_window: unmap.\n");
  sleep (3);
  XMapWindow (vwin->video_display, vwin->video_window);
  XSync (vwin->video_display, False);
  printf ("video_window: map.\n");
#endif
  if (!vwin)
    return;
  if (vwin->gui->use_root_window)
    return;

  if (vwin->gui->vo_port)
    xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *)(intptr_t)show_window);

  pthread_mutex_lock (&vwin->mutex);

  /* When shutting down (panel == NULL), unmap. Old kwin dislikes destroying iconified windows.
   * Otherwise, dont unmap both vwin and panel - user needs some handle to get back in. */
  vwin->show = show_window ? 2
             : !vwin->gui->panel ? 0
             : panel_is_visible (vwin->gui->panel) > 0 ? 0 : 1;

  /* Switching to visible: If new window size requested meanwhile, adapt window */
  if ((vwin->show > 1) && (vwin->fullscreen_mode & WINDOWED_MODE) &&
     (vwin->win_width != vwin->old_win_width || vwin->win_height != vwin->old_win_height))
    video_window_adapt_size (vwin);

  if (vwin->show > 1) {

    if ((vwin->gui->always_layer_above ||
      (((!(vwin->fullscreen_mode & WINDOWED_MODE)) && is_layer_above (vwin->gui)) &&
      (vwin->hide_on_start == 0))) && (!_vwin_is_ewmh (vwin))) {
      xitk_window_set_layer_above(vwin->wrapped_window);
    }

    xitk_window_flags (vwin->wrapped_window, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);

    if ((vwin->gui->always_layer_above ||
      (((!(vwin->fullscreen_mode & WINDOWED_MODE)) && is_layer_above (vwin->gui)) &&
      (vwin->hide_on_start == 0))) && (_vwin_is_ewmh (vwin))) {
      xitk_window_set_layer_above(vwin->wrapped_window);
    }

    /* inform the window manager that we are fullscreen. This info musn't be set for xinerama-fullscreen,
       otherwise there are 2 different window size for one fullscreen mode ! (kwin doesn't accept this) */
    if (!(vwin->fullscreen_mode & (WINDOWED_MODE | FULLSCR_XI_MODE))) {
      xitk_window_flags (vwin->wrapped_window, XITK_WINF_FULLSCREEN, XITK_WINF_FULLSCREEN);
    }
  } else if (vwin->show == 1) {
    xitk_window_flags (vwin->wrapped_window, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_ICONIFIED);
  } else {
    xitk_window_flags (vwin->wrapped_window, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
  }

  /* User used '-H', now he want to show video window */
  if (vwin->hide_on_start == -1)
    vwin->hide_on_start = 0;

  pthread_mutex_unlock (&vwin->mutex);
}

/*
 * check if screen_number is in the list
 */
static int screen_is_in_xinerama_fullscreen_list (const char *list, int screen_number) {
  const char *buffer;
  int         dummy;

  buffer = list;

  do {
    if((sscanf(buffer,"%d", &dummy) == 1) && (screen_number == dummy))
      return 1;
  } while((buffer = strchr(buffer,' ')) && (++buffer < (list + strlen(list))));

  return 0;
}

static void vwin_dummy_un_lock_display (Display *display) {
  (void)display;
}

/*
 *
 */
xui_vwin_t *video_window_init (gGui_t *gui, int window_id,
                               int borderless, const char *geometry, int hide_on_start,
                               const char *prefered_visual, int use_x_lock_display,
                               int install_colormap) {
  xui_vwin_t           *vwin;
  int                   i;
  const char           *video_display_name;
#ifdef HAVE_XINERAMA
  int                   screens;
  int                   dummy_a, dummy_b;
  XineramaScreenInfo   *screeninfo = NULL;
  const char           *screens_list;
#endif
#ifdef HAVE_XF86VIDMODE
  int                   dummy_query_event, dummy_query_error;
#endif
  int geometry_x = 0, geometry_y = 0, geometry_w = -8192, geometry_h = -8192;

  if (!gui)
    return NULL;

  video_display_name =
    xine_config_register_string (gui->xine, "gui.video_display",
                                 "",
                                 _("Name of video display"),
                                 _("Use this setting to configure to which "
                                   "display xine will show videos. When left blank, "
                                   "the main display will be used. Setting this "
                                   "option to something like ':0.1' or ':1' makes "
                                   "possible to display video and control on different "
                                   "screens (very useful for TV presentations)."),
                                 CONFIG_LEVEL_ADV, NULL, CONFIG_NO_DATA);

  vwin = calloc (1, sizeof (*vwin));
  if (!vwin)
    return NULL;

  if (use_x_lock_display) {
    vwin->x_lock_display = XLockDisplay;
    vwin->x_unlock_display = XUnlockDisplay;
  } else {
    vwin->x_lock_display =
    vwin->x_unlock_display = vwin_dummy_un_lock_display;
  }

  vwin->gui = gui;
  pthread_mutex_init (&vwin->mutex, NULL);

  vwin->video_display = NULL;
  if (video_display_name && video_display_name[0]) {
    do {
      vwin->video_backend = xitk_backend_new (vwin->gui->xitk, vwin->gui->verbosity);
      if (vwin->video_backend) {
        vwin->video_be_display = vwin->video_backend->open_display (vwin->video_backend,
                                                                    video_display_name, use_x_lock_display, 0,
                                                                    prefered_visual, install_colormap);
        if (vwin->video_be_display) {
          vwin->video_display = (Display *)vwin->video_be_display->id;
          vwin->separate_display = 1;
          break;
        }
        vwin->video_backend->_delete (&vwin->video_backend);
      }
      fprintf (stderr, _("Cannot open display '%s' for video. Falling back to primary display.\n"), video_display_name);
    } while (0);
  }
  if (vwin->video_display == NULL)
    vwin->video_display = xitk_x11_get_display (gui->xitk);
  if (vwin->video_display == NULL) {
    fprintf (stderr, _("Cannot open any X11 display for video.\n"));
    free(vwin);
    return NULL;
  }
  {
    char *xrm_geometry = NULL;
    if (prefered_visual)
      vwin->prefered_visual = strdup(prefered_visual);
    xitk_x11_xrm_parse("xine",
                       geometry ? NULL : &xrm_geometry,
                       borderless ? NULL : &borderless,
                       vwin->prefered_visual ? NULL : &vwin->prefered_visual,
                       NULL);
    if (!gui->use_root_window) {
      if (!geometry)
        geometry = xrm_geometry;
      if (geometry &&
          !xitk_x11_parse_geometry (geometry, &geometry_x, &geometry_y, &geometry_w, &geometry_h))
        printf(_("Bad geometry '%s'\n"), geometry);
    }
    free(xrm_geometry);
  }
  if (gui->use_root_window) {
    /*
     * Using root window mode don't allow
     * window geometry, so, reset those params.
     */
    geometry = NULL;
    borderless = 0;
  }

  vwin->x_lock_display (vwin->video_display);
  vwin->video_screen = DefaultScreen(vwin->video_display);
  vwin->x_unlock_display (vwin->video_display);

  vwin->video_window       = None;
  vwin->wid                = window_id;
  vwin->gui_depth          = xitk_x11_get_depth(gui->xitk);
  vwin->fullscreen_req     = WINDOWED_MODE;
  vwin->fullscreen_mode    = WINDOWED_MODE;
  vwin->video_window  = None;
  vwin->show               = 2;
  vwin->widget_key         = 0;
  vwin->borderless         = (borderless > 0);
  vwin->border_left        = 0;
  vwin->border_top         = 0;
  vwin->hide_on_start      = hide_on_start;

  vwin->depth              = vwin->gui_depth;
  vwin->gui_visual         =
  vwin->visual             = xitk_x11_get_visual(gui->xitk);

  /* Currently, there no plugin loaded so far, but that might change */
  video_window_select_visual (vwin);

  if (vwin->separate_display) {
    video_window_find_visual (vwin);
  }

  vwin->xwin               = geometry_x;
  vwin->ywin               = geometry_y;

  vwin->x_lock_display (vwin->video_display);
  vwin->desktopWidth       = DisplayWidth(vwin->video_display, vwin->video_screen);
  vwin->desktopHeight      = DisplayHeight(vwin->video_display, vwin->video_screen);
  vwin->fullscreen_width   = vwin->desktopWidth;
  vwin->fullscreen_height  = vwin->desktopHeight;

  memcpy (vwin->window_title, "xine", 5);

  gettimeofday (&vwin->click_time, NULL);

#ifdef HAVE_XINERAMA
  vwin->xinerama       = NULL;
  vwin->xinerama_cnt   = 0;
  /* Spark
   * some Xinerama stuff
   * I want to figure out what fullscreen means for this setup
   */

  if ((XineramaQueryExtension (vwin->video_display, &dummy_a, &dummy_b))
      && (screeninfo = XineramaQueryScreens (vwin->video_display, &screens))) {
    /* Xinerama Detected */
#ifdef DEBUG
    printf ("videowin: display is using xinerama with %d screens\n", screens);
    printf ("videowin: going to assume we are using the first screen.\n");
    printf ("videowin: size of the first screen is %dx%d.\n",
	     screeninfo[0].width, screeninfo[0].height);
#endif
    if (XineramaIsActive(vwin->video_display)) {
      vwin->fullscreen_width  = screeninfo[0].width;
      vwin->fullscreen_height = screeninfo[0].height;
      vwin->xinerama = screeninfo;
      vwin->xinerama_cnt = screens;

      screens_list = xine_config_register_string (vwin->gui->xine, "gui.xinerama_use_screens",
        "0 1",
        _("Screens to use in order to do a very fullscreen in xinerama mode. (example 0 2 3)"),
        _("Example, if you want the display to expand on screen 0, 2 and 3, enter 0 2 3"),
        CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA);

      if((sscanf(screens_list,"%d",&dummy_a) == 1) && (dummy_a >= 0) && (dummy_a < screens)) {

        /* try to calculate the best maximum size for xinerama fullscreen */
        vwin->xinerama_fullscreen_x = screeninfo[dummy_a].x_org;
        vwin->xinerama_fullscreen_y = screeninfo[dummy_a].y_org;
        vwin->xinerama_fullscreen_width = screeninfo[dummy_a].width;
        vwin->xinerama_fullscreen_height = screeninfo[dummy_a].height;

        i = dummy_a;
        while(i < screens) {

          if(screen_is_in_xinerama_fullscreen_list(screens_list, i)) {
            if(screeninfo[i].x_org < vwin->xinerama_fullscreen_x)
	      vwin->xinerama_fullscreen_x = screeninfo[i].x_org;
            if(screeninfo[i].y_org < vwin->xinerama_fullscreen_y)
	      vwin->xinerama_fullscreen_y = screeninfo[i].y_org;
          }

          i++;
        }

        i = dummy_a;
        while(i < screens) {

          if(screen_is_in_xinerama_fullscreen_list(screens_list, i)) {
            if((screeninfo[i].width + screeninfo[i].x_org) >
	       (vwin->xinerama_fullscreen_x + vwin->xinerama_fullscreen_width)) {
	      vwin->xinerama_fullscreen_width =
		screeninfo[i].width + screeninfo[i].x_org - vwin->xinerama_fullscreen_x;
	    }

            if((screeninfo[i].height + screeninfo[i].y_org) >
	       (vwin->xinerama_fullscreen_y + vwin->xinerama_fullscreen_height)) {
	      vwin->xinerama_fullscreen_height =
		screeninfo[i].height + screeninfo[i].y_org - vwin->xinerama_fullscreen_y;
	    }
          }

          i++;
        }
      } else {
        /* we can't find screens to use, so we use screen 0 */
        vwin->xinerama_fullscreen_x      = screeninfo[0].x_org;
        vwin->xinerama_fullscreen_y      = screeninfo[0].y_org;
        vwin->xinerama_fullscreen_width  = screeninfo[0].width;
        vwin->xinerama_fullscreen_height = screeninfo[0].height;
      }

      dummy_a = xine_config_register_num (vwin->gui->xine, "gui.xinerama_fullscreen_x",
        -8192,
        _("x coordinate for xinerama fullscreen (-8192 = autodetect)"),
        CONFIG_NO_HELP, CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA);
      if(dummy_a > -8192)
        vwin->xinerama_fullscreen_x = dummy_a;

      dummy_a = xine_config_register_num (vwin->gui->xine, "gui.xinerama_fullscreen_y",
        -8192,
        _("y coordinate for xinerama fullscreen (-8192 = autodetect)"),
        CONFIG_NO_HELP, CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA);
      if(dummy_a > -8192)
        vwin->xinerama_fullscreen_y = dummy_a;

      dummy_a = xine_config_register_num (vwin->gui->xine, "gui.xinerama_fullscreen_width",
        -8192,
        _("width for xinerama fullscreen (-8192 = autodetect)"),
        CONFIG_NO_HELP, CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA);
      if(dummy_a > -8192)
        vwin->xinerama_fullscreen_width = dummy_a;

      dummy_a = xine_config_register_num (vwin->gui->xine, "gui.xinerama_fullscreen_height",
        -8192,
        _("height for xinerama fullscreen (-8192 = autodetect)"),
        CONFIG_NO_HELP, CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA);
      if(dummy_a > -8192)
        vwin->xinerama_fullscreen_height = dummy_a;

#ifdef DEBUG
      printf ("videowin: Xinerama fullscreen parameters: X_origin=%d Y_origin=%d Width=%d Height=%d\n",
        vwin->xinerama_fullscreen_x, vwin->xinerama_fullscreen_y,
        vwin->xinerama_fullscreen_width, vwin->xinerama_fullscreen_height);
#endif
    }
    else {
      vwin->xinerama_fullscreen_x      = 0;
      vwin->xinerama_fullscreen_y      = 0;
      vwin->xinerama_fullscreen_width  = vwin->fullscreen_width;
      vwin->xinerama_fullscreen_height = vwin->fullscreen_height;

      XFree (screeninfo);
    }
  } else {
    /* no Xinerama */
    if (vwin->gui->verbosity)
      printf ("Display is not using Xinerama.\n");
  }
#endif
  vwin->x_unlock_display (vwin->video_display);

  vwin->visible_width  = vwin->fullscreen_width;
  vwin->visible_height = vwin->fullscreen_height;

  /* xclass hint for video window */
  vwin->res_name.normal     = _("xine Video Window");
  vwin->res_name.fullscreen = _("xine Video Fullscreen Window");
  vwin->res_name.borderless = _("xine Video Borderless Window");

  vwin->current_cursor = CURSOR_ARROW;
  vwin->cursor_timer   = 0;

  vwin->stream_resize_window = xine_config_register_bool (vwin->gui->xine, "gui.stream_resize_window",
    1,
    _("New stream sizes resize output window"),
    CONFIG_NO_HELP, CONFIG_LEVEL_ADV, _video_window_resize_cb, vwin);

  vwin->zoom_small_stream = xine_config_register_bool (vwin->gui->xine, "gui.zoom_small_stream",
    0,
    _("Double size for small streams (require stream_resize_window)"),
    CONFIG_NO_HELP, CONFIG_LEVEL_ADV, _video_window_zoom_small_cb, vwin);

  if ((geometry_w > 0) && (geometry_h > 0)) {
    vwin->video_width  = geometry_w;
    vwin->video_height = geometry_h;
    /*
     * Force to keep window size.
     * I don't update the config file, i think this window geometry
     * user defined can be temporary.
     */
    vwin->stream_resize_window = 0;
  }
  else {
    vwin->video_width  = 768;
    vwin->video_height = 480;
  }
  vwin->old_win_width  = vwin->win_width  = vwin->video_width;
  vwin->old_win_height = vwin->win_height = vwin->video_height;

#ifdef HAVE_XF86VIDMODE
  if (xine_config_register_bool (vwin->gui->xine, "gui.use_xvidext",
    0,
    _("use XVidModeExtension when switching to fullscreen"),
    CONFIG_NO_HELP, CONFIG_LEVEL_EXP, CONFIG_NO_CB, CONFIG_NO_DATA)) {
    /*
     * without the "stream resizes window" behavior, the XVidMode support
     * won't work correctly, so we force it for each session the user wants
     * to have XVideMode on...
     *
     * FIXME: maybe display a warning message or so?!
     */
    vwin->stream_resize_window = 1;

    vwin->x_lock_display (vwin->video_display);

    if (XF86VidModeQueryExtension (vwin->video_display, &dummy_query_event, &dummy_query_error)) {
      XF86VidModeModeInfo* XF86_modelines_swap;
      int                  mode, major, minor, sort_x, sort_y;

      XF86VidModeQueryVersion (vwin->video_display, &major, &minor);
      printf (_("XF86VidMode Extension (%d.%d) detected, trying to use it.\n"), major, minor);

      if (XF86VidModeGetAllModeLines (vwin->video_display, XDefaultScreen(vwin->video_display),
        &(vwin->XF86_modelines_count), &(vwin->XF86_modelines))) {
        printf (_("XF86VidMode Extension: %d modelines found.\n"), vwin->XF86_modelines_count);

	/* first, kick off unsupported modes */
        for (mode = 1; mode < vwin->XF86_modelines_count; mode++) {

          if (!XF86VidModeValidateModeLine (vwin->video_display, vwin->video_screen,
            vwin->XF86_modelines[mode])) {
	    int wrong_mode;

	    printf(_("XF86VidModeModeLine %dx%d isn't valid: discarded.\n"),
              vwin->XF86_modelines[mode]->hdisplay,
              vwin->XF86_modelines[mode]->vdisplay);

            for (wrong_mode = mode; wrong_mode < vwin->XF86_modelines_count; wrong_mode++)
	      vwin->XF86_modelines[wrong_mode] = vwin->XF86_modelines[wrong_mode + 1];

            vwin->XF86_modelines[wrong_mode] = NULL;
            vwin->XF86_modelines_count--;
	    mode--;
	  }
	}

	/*
	 * sorting modelines, skipping first entry because it is the current
	 * modeline in use - this is important so we know to which modeline
	 * we have to switch to when toggling fullscreen mode.
	 */
        for (sort_x = 1; sort_x < vwin->XF86_modelines_count; sort_x++) {

          for (sort_y = sort_x+1; sort_y < vwin->XF86_modelines_count; sort_y++) {

            if (vwin->XF86_modelines[sort_x]->hdisplay > vwin->XF86_modelines[sort_y]->hdisplay) {
              XF86_modelines_swap = vwin->XF86_modelines[sort_y];
              vwin->XF86_modelines[sort_y] = vwin->XF86_modelines[sort_x];
              vwin->XF86_modelines[sort_x] = XF86_modelines_swap;
	    }
	  }
	}
      } else {
        vwin->XF86_modelines_count = 0;
	printf(_("XF86VidMode Extension: could not get list of available modelines. Failed.\n"));
      }
    } else {
      printf(_("XF86VidMode Extension: initialization failed, not using it.\n"));
    }
    vwin->x_unlock_display (vwin->video_display);
  }
  else
    vwin->XF86_modelines_count = 0;

#endif

  /*
   * Create initial video window with the geometry constructed above.
   */
  video_window_adapt_size (vwin);

  /*
   * for plugins that aren't really bind to the window, it's necessary that the
   * vwin->xwin and vwin->ywin variables are set to *real* values, otherwise the
   * overlay will be displayed somewhere outside the window
   */
  if (vwin->video_window) {
    Window tmp_win;

    vwin->x_lock_display (vwin->video_display);
    if (geometry && (geometry_x > -8192) && (geometry_y > -8192)) {
      vwin->xwin = vwin->old_xwin = geometry_x;;
      vwin->ywin = vwin->old_ywin = geometry_y;

      XMoveResizeWindow (vwin->video_display, vwin->video_window,
        vwin->xwin, vwin->ywin, vwin->video_width, vwin->video_height);

    }
    else {

      XTranslateCoordinates (vwin->video_display, vwin->video_window,
        DefaultRootWindow (vwin->video_display), 0, 0, &vwin->xwin, &vwin->ywin, &tmp_win);
    }
    vwin->x_unlock_display (vwin->video_display);

  }

  if (vwin->separate_display) {
    vwin->second_display_running = 1;
    pthread_create (&vwin->second_display_thread, NULL, second_display_loop, vwin);
  }

  vwin->pixel_aspect = vwin->video_be_display ? vwin->video_be_display->ratio : xitk_get_display_ratio(vwin->gui->xitk);

#ifdef HAVE_XINERAMA
  {
    int    dummy_event, dummy_error;
    if (XineramaQueryExtension(vwin->video_display, &dummy_event, &dummy_error)) {
      void *info;
      int count = 1;

      info = XineramaQueryScreens(vwin->video_display, &count);
      if (count > 1)
        /* multihead -> assuming square pixels */
        vwin->pixel_aspect = 1.0;
      if (info)
        XFree(info);
    }
  }
#endif

#ifdef DEBUG
    printf("pixel_aspect: %f\n", vwin->pixel_aspect);
#endif

  return vwin;
}

/*
 * Necessary cleanup
 */
void video_window_exit (xui_vwin_t *vwin) {

  if (!vwin)
    return;

#ifdef HAVE_XINE_CONFIG_UNREGISTER_CALLBACKS
  xine_config_unregister_callbacks (vwin->gui->xine, NULL, NULL, vwin, sizeof (*vwin));
#endif

  pthread_mutex_lock (&vwin->mutex);
  vwin->second_display_running = 0;
#ifdef HAVE_XF86VIDMODE
  /* Restore original VidMode */
  if (vwin->gui->XF86VidMode_fullscreen) {
    _reset_modeline(vwin);
  }
#endif
  pthread_mutex_unlock (&vwin->mutex);

  {
    XEvent event;

    vwin->x_lock_display (vwin->video_display);
    XClearWindow (vwin->video_display, vwin->video_window);
    event.xexpose.type       = Expose;
    event.xexpose.send_event = True;
    event.xexpose.display    = vwin->video_display;
    event.xexpose.window     = vwin->video_window;
    event.xexpose.x          = 0;
    event.xexpose.y          = 0;
    event.xexpose.width      = vwin->video_width;
    event.xexpose.height     = vwin->video_height;
    XSendEvent (vwin->video_display, vwin->video_window, False, Expose, &event);
    vwin->x_unlock_display (vwin->video_display);
  }

  if (vwin->separate_display) {
    pthread_join (vwin->second_display_thread, NULL);
  } else {
    xitk_unregister_event_handler (vwin->gui->xitk, &vwin->widget_key);
  }
  xitk_window_destroy_window (vwin->wrapped_window);
  vwin->wrapped_window = NULL;

  if (!vwin->wid) {
    vwin->x_lock_display (vwin->video_display);
    XUnmapWindow (vwin->video_display, vwin->video_window);
    XDestroyWindow (vwin->video_display, vwin->video_window);
    XSync (vwin->video_display, False);
    vwin->x_unlock_display (vwin->video_display);
  }
  vwin->video_window = None;

  pthread_mutex_destroy (&vwin->mutex);

#ifdef HAVE_XINERAMA
  if (vwin->xinerama)
    XFree (vwin->xinerama);
#endif

  if (vwin->video_be_display)
    vwin->video_be_display->close (&vwin->video_be_display);
  if (vwin->video_backend)
    vwin->video_backend->_delete (&vwin->video_backend);

  free(vwin->prefered_visual);

  free (vwin);
}


/*
 * Translate screen coordinates to video coordinates
 */
static int video_window_translate_point (xui_vwin_t *vwin,
  int gui_x, int gui_y, int *video_x, int *video_y) {
  x11_rectangle_t rect;
  int             wwin, hwin;
  float           xf,yf;
  float           scale, width_scale, height_scale,aspect;

  if (!vwin || !vwin->gui || !vwin->gui->vo_port)
    return 0;

  rect.x = gui_x;
  rect.y = gui_y;
  rect.w = 0;
  rect.h = 0;

  if (xine_port_send_gui_data (vwin->gui->vo_port,
    XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect) != -1) {
    /* driver implements vwin->gui->video coordinate space translation, use it */
    *video_x = rect.x;
    *video_y = rect.y;
    return 1;
  }

  /* Driver cannot convert vwin->gui->video space, fall back to old code... */

  pthread_mutex_lock (&vwin->mutex);
  xitk_window_get_window_position (vwin->wrapped_window, NULL, NULL, &wwin, &hwin);
  if (wwin < 1 || hwin < 1) {
    pthread_mutex_unlock (&vwin->mutex);
    return 0;
  }

  /* Scale co-ordinate to image dimensions. */
  height_scale = (float)vwin->video_height / (float)hwin;
  width_scale  = (float)vwin->video_width / (float)wwin;
  aspect       = (float)vwin->video_width / (float)vwin->video_height;
  if (((float)wwin / (float)hwin) < aspect) {
    scale    = width_scale;
    xf       = (float)gui_x * scale;
    yf       = (float)gui_y * scale;
    /* wwin=wwin * scale; */
    hwin     = hwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x = xf * 1.25 / aspect;
    *video_y = yf - ((hwin - vwin->video_height) / 2);
    /* printf("wscale:a=%f, s=%f, x=%d, y=%d\n",aspect, scale,*video_x,*video_y);  */
  } else {
    scale    = height_scale;
    xf       = (float)gui_x * scale;
    yf       = (float)gui_y * scale;
    wwin     = wwin * scale;
    /* FIXME: The 1.25 should really come from the NAV packets. */
    *video_x = (xf - ((wwin - vwin->video_width) /2)) * 1.25 / aspect;
    *video_y = yf;
    /* printf("hscale:a=%f s=%f x=%d, y=%d\n",aspect,scale,*video_x,*video_y);  */
  }

  pthread_mutex_unlock (&vwin->mutex);

  return 1;
}

/*
 * Set/Get magnification.
 */
static int video_window_check_mag (xui_vwin_t *vwin) {
  if ((!(vwin->fullscreen_mode & WINDOWED_MODE))
/*
 * Currently, no support for magnification in fullscreen mode.
 * Commented out in order to not mess up current mag for windowed mode.
 *
#ifdef HAVE_XF86VIDMODE
     && !(vwin->XF86_modelines_count > 1)
#endif
 */
    )
    return 0;

  /* Allow mag only if video win is visible, so don't do something we can't see. */
  return !!(xitk_window_flags (vwin->wrapped_window, 0, 0) & XITK_WINF_VISIBLE);
}

static void video_window_calc_mag_win_size (xui_vwin_t *vwin, float xmag, float ymag) {
  vwin->win_width  = (int) ((float) vwin->video_width * xmag + 0.5f);
  vwin->win_height = (int) ((float) vwin->video_height * ymag + 0.5f);
}

int video_window_set_mag (xui_vwin_t *vwin, float xmag, float ymag) {
  if (!vwin)
    return 0;

  pthread_mutex_lock (&vwin->mutex);

  if (!video_window_check_mag (vwin)) {
    pthread_mutex_unlock (&vwin->mutex);
    return 0;
  }
  video_window_calc_mag_win_size (vwin, xmag, ymag);
  video_window_adapt_size (vwin);

  pthread_mutex_unlock (&vwin->mutex);

  return 1;
}

void video_window_get_mag (xui_vwin_t *vwin, float *xmag, float *ymag) {
  if (!vwin) {
    *xmag = *ymag = 1.0;
    return;
  }

  /* compute current mag */
  pthread_mutex_lock (&vwin->mutex);
  *xmag = (float) vwin->output_width / (float) vwin->video_width;
  *ymag = (float) vwin->output_height / (float) vwin->video_height;
  pthread_mutex_unlock (&vwin->mutex);
}

/*
 *
 */

static int _vwin_handle_be_event (void *data, const xitk_be_event_t *e) {
  xui_vwin_t *vwin = data;
  switch (e->type) {
    case XITK_EV_BUTTON_DOWN:
      {
        xine_input_data_t input;
        xine_event_t      event;
        int               x, y;

        if (!vwin->gui->cursor_visible) {
          vwin->gui->cursor_visible = 1;
          video_window_set_cursor_visibility (vwin, vwin->gui->cursor_visible);
        }

        if ((e->code == 3) && !vwin->separate_display)
          video_window_menu (vwin->gui, xitk_window_widget_list (vwin->wrapped_window), e->w, e->h);
        else if (e->code == 2)
          panel_toggle_visibility (NULL, vwin->gui->panel);
        else if (e->code == 1) {
          struct timeval old_click_time = vwin->click_time;

          gettimeofday (&vwin->click_time, NULL);
          if (xitk_is_dbl_click (vwin->gui->xitk, &old_click_time, &vwin->click_time)) {
            vwin->click_time = event.tv = old_click_time;
            gui_execute_action_id (vwin->gui, ACTID_TOGGLE_FULLSCREEN);
          } else {
            event.tv = vwin->click_time;
          }
        }

        event.type = XINE_EVENT_INPUT_MOUSE_BUTTON;
        if (e->code != 1 || !oxine_mouse_event (event.type, e->x, e->y)) {
          if (video_window_translate_point (vwin, e->x, e->y, &x, &y)) {
            event.stream      = vwin->gui->stream;
            event.data        = &input;
            event.data_length = sizeof (input);
            input.button      = e->code;
            input.x           = x;
            input.y           = y;
            xine_event_send (vwin->gui->stream, &event);
          }
        }
      }
      break;
    case XITK_EV_MOVE:
      {
        xine_event_t event;
        xine_input_data_t input;
        int x, y;

        if (!vwin->gui->cursor_visible) {
          vwin->gui->cursor_visible = 1;
          video_window_set_cursor_visibility (vwin, vwin->gui->cursor_visible);
        }

        event.type = XINE_EVENT_INPUT_MOUSE_MOVE;
        if (!oxine_mouse_event (event.type, e->x, e->y)) {
          if (video_window_translate_point (vwin, e->x, e->y, &x, &y)) {
            event.stream      = vwin->gui->stream;
            event.data        = &input;
            event.data_length = sizeof (input);
            gettimeofday (&event.tv, NULL);
            input.button      = 0; /*  No buttons, just motion. */
            input.x           = x;
            input.y           = y;
            xine_event_send (vwin->gui->stream, &event);
          }
        }
      }
      break;
    case XITK_EV_DEL_WIN:
      gui_exit (NULL, vwin->gui);
      break;
    case XITK_EV_EXPOSE:
      if ((e->more == 0) && vwin->gui->vo_port) {
        xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_EXPOSE_EVENT, (void *)e->id);
      }
      break;
    case XITK_EV_POS_SIZE:
      {
        Window tmp_win;
        int    h, w;

        pthread_mutex_lock (&vwin->mutex);

        h = vwin->output_height;
        w = vwin->output_width;
        vwin->output_width  = e->w;
        vwin->output_height = e->h;

        if ((e->x == 0) && (e->y == 0)) {
          vwin->x_lock_display (vwin->video_display);
          XTranslateCoordinates (vwin->video_display, vwin->video_window, DefaultRootWindow(vwin->video_display),
            0, 0, &vwin->xwin, &vwin->ywin, &tmp_win);
          vwin->x_unlock_display (vwin->video_display);
        } else {
          vwin->xwin = e->x;
          vwin->ywin = e->y;
        }

        /* Keep geometry memory of windowed mode in sync. */
        if (vwin->fullscreen_mode & WINDOWED_MODE) {
          vwin->old_win_width  = vwin->win_width  = vwin->output_width;
          vwin->old_win_height = vwin->win_height = vwin->output_height;
        }

        if ((h != vwin->output_height) || (w != vwin->output_width))
          osd_update_osd ();

        oxine_adapt ();

        pthread_mutex_unlock (&vwin->mutex);
      }
      break;
    case XITK_EV_KEY_DOWN:
      if ((e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_MENU) && !vwin->separate_display) {
        video_window_menu (vwin->gui, xitk_window_widget_list (vwin->wrapped_window), e->w, e->h);
        return 1;
      }
      /* fall through */
    default:
      return gui_handle_be_event (vwin->gui, e);
  }
  return 1;
}

static void register_event_handler(xui_vwin_t *vwin)
{
  if (vwin->video_be_display) {
    xitk_window_destroy_window (vwin->wrapped_window);
    vwin->wrapped_window = xitk_x11_wrap_window (NULL, vwin->video_be_display, vwin->video_window);
    xitk_window_flags (vwin->wrapped_window, XITK_WINF_DND, XITK_WINF_DND);
  } else {
    xitk_unregister_event_handler (vwin->gui->xitk, &vwin->widget_key);
    xitk_window_destroy_window (vwin->wrapped_window);
    vwin->wrapped_window = xitk_x11_wrap_window (vwin->gui->xitk, NULL, vwin->video_window);
    xitk_window_flags (vwin->wrapped_window,
      XITK_WINF_TASKBAR | XITK_WINF_PAGER | XITK_WINF_DND, XITK_WINF_TASKBAR | XITK_WINF_PAGER | XITK_WINF_DND);
    vwin->widget_key = xitk_be_register_event_handler ("video_window", vwin->wrapped_window, NULL,
      _vwin_handle_be_event, vwin, NULL, NULL);
    xitk_window_set_role (vwin->wrapped_window, vwin->gui->use_root_window ? XITK_WR_ROOT : XITK_WR_MAIN);
    /* NOTE: this makes kwin use a desktop file named <res_class>.desktop to set the icon.
     * any subsequent attempt to set an icon has no effect then.
     * setting an icon _before_ leads to random fallback to default x icon. */
    xitk_window_set_window_class (vwin->wrapped_window, vwin->res_name.normal, "xine");
    xitk_window_set_window_icon (vwin->wrapped_window, vwin->gui->icon);
  }

  xitk_window_set_window_title (vwin->wrapped_window, vwin->window_title);
}

/*
 * very small X event loop for the second display
 */
static void *second_display_loop (void *data) {
  xui_vwin_t *vwin = data;

  pthread_mutex_lock (&vwin->mutex);
  while (vwin->second_display_running) {
    xitk_be_event_t event;

    pthread_mutex_unlock (&vwin->mutex);
    if (vwin->video_be_display->next_event (vwin->video_be_display, &event, NULL, XITK_EV_ANY, 33)) {
      if (event.window && vwin->wrapped_window == event.window->data)
        _vwin_handle_be_event (vwin, &event);
    }
    pthread_mutex_lock (&vwin->mutex);
  }
  pthread_mutex_unlock (&vwin->mutex);

  return NULL;
}

/*
 *
 */

long int video_window_reset_ssaver (xui_vwin_t *vwin) {

  long int idle = 0;

  if (!vwin)
    return 0;
  if (!vwin->gui->ssaver_enabled)
    return 0;

  if (vwin->video_be_display) {
    if (vwin->video_be_display->reset_screen_saver)
      idle = vwin->video_be_display->reset_screen_saver(vwin->video_be_display, vwin->gui->ssaver_timeout);
  } else {
    idle = xitk_reset_screen_saver(vwin->gui->xitk, vwin->gui->ssaver_timeout);
  }

  return idle;
}

void video_window_get_frame_size (xui_vwin_t *vwin, int *w, int *h) {
  if (!vwin)
    return;

  if(w)
    *w = vwin->frame_width;
  if(h)
    *h = vwin->frame_height;
  if (!vwin->frame_width && !vwin->frame_height) {
    /* fall back to meta info */
    if (w) {
      *w = xine_get_stream_info (vwin->gui->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
    }
    if (h) {
      *h = xine_get_stream_info (vwin->gui->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    }
  }
}

void video_window_get_visible_size (xui_vwin_t *vwin, int *w, int *h) {
  if (!vwin)
    return;
  if(w)
    *w = vwin->visible_width;
  if(h)
    *h = vwin->visible_height;
}

void video_window_get_output_size (xui_vwin_t *vwin, int *w, int *h) {
  if (w)
    *w = vwin ? vwin->output_width : 0;
  if (h)
    *h = vwin ? vwin->output_height : 0;
}

void video_window_get_window_size (xui_vwin_t *vwin, int *window_width, int *window_height) {
  if (!vwin) {
    if (window_width)
      *window_width = 0;
    if (window_height)
      *window_height = 0;
    return;
  }
  xitk_window_get_window_position (vwin->wrapped_window, NULL, NULL, window_width, window_height);
}

void video_window_set_mrl (xui_vwin_t *vwin, char *mrl) {
  if (!vwin || !mrl)
    return;
  if (!mrl[0])
    return;
  if (!memcmp (vwin->window_title, "xine: ", 6) && !strcmp (vwin->window_title + 6, mrl))
    return;
  memcpy (vwin->window_title, "xine: ", 6);
  strlcpy (vwin->window_title + 6, mrl, sizeof (vwin->window_title) - 6);
  xitk_window_set_window_title (vwin->wrapped_window, vwin->window_title);
}

void video_window_toggle_border (xui_vwin_t *vwin) {
  if (!vwin)
    return;

  if (!vwin->gui->use_root_window && (vwin->fullscreen_mode & WINDOWED_MODE)) {
    vwin->borderless = !vwin->borderless;

    xitk_window_flags (vwin->wrapped_window, XITK_WINF_DECORATED, vwin->borderless ? 0 : XITK_WINF_DECORATED);

    xitk_window_set_window_class (vwin->wrapped_window,
                                  vwin->borderless ? vwin->res_name.borderless : vwin->res_name.normal,
                                  "xine");

    xitk_window_set_border_size (vwin->gui->xitk, vwin->widget_key,
      vwin->borderless ? 0 : vwin->border_left,
      vwin->borderless ? 0 : vwin->border_top);

    xine_port_send_gui_data (vwin->gui->vo_port, XINE_GUI_SEND_DRAWABLE_CHANGED, (void *)vwin->video_window);
  }
}
