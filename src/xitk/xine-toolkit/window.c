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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include "_xitk.h"
#include "xitk.h"

#define TITLE_BAR_HEIGHT 20

#define _XITK_VASPRINTF(_target_ptr,_fmt) \
  if (_fmt) { \
    va_list args; \
    va_start (args, _fmt); \
    _target_ptr = xitk_vasprintf (_fmt, args); \
    va_end (args); \
  } else { \
    _target_ptr = NULL; \
  }

typedef struct xitk_dialog_s xitk_dialog_t;

struct xitk_dialog_s {
  ImlibData              *imlibdata;
  xitk_window_t          *xwin;
  xitk_widget_list_t     *widget_list;
  xitk_register_key_t     key;
  int                     type;

  xitk_widget_t          *w1, *w2, *w3;
  void                  (*done3cb)(void *userdata, int state);
  void                   *done3data;

  xitk_widget_t          *checkbox;
  xitk_widget_t          *checkbox_label;

  xitk_widget_t          *default_button;
};

/* NOTE: this will free (text). */
static xitk_dialog_t *_xitk_dialog_new (ImlibData *im,
  const char *title, char *text, int *width, int *height, int align) {
  xitk_dialog_t *wd;
  xitk_image_t *image;

  if (!text)
    return NULL;

  if (!im || !*width) {
    free (text);
    return NULL;
  }

  wd = xitk_xmalloc (sizeof (*wd));
  if (!wd) {
    free (text);
    return NULL;
  }
  wd->imlibdata = im;

  image = xitk_image_create_image_from_string (im, DEFAULT_FONT_12, *width - 40, align, text);
  free (text);
  if (!image) {
    free (wd);
    return NULL;
  }

  *height += image->height;
  wd->xwin = xitk_window_create_dialog_window (im, title, 0, 0, *width, *height);
  if (!wd->xwin) {
    xitk_image_free_image (&image);
    free (wd);
    return NULL;
  }
  wd->w1 = wd->w2 = wd->w3 = NULL;
  wd->default_button = NULL;
  wd->widget_list = NULL;

  /* Draw text area */
  {
    xitk_pixmap_t  *bg;

    bg = xitk_window_get_background_pixmap (wd->xwin);
    if (bg) {
      XLOCK (im->x.x_lock_display, im->x.disp);
      XCopyArea (im->x.disp, image->image->pixmap, bg->pixmap,
        image->image->gc, 0, 0, image->width, image->height, 20, TITLE_BAR_HEIGHT + 20);
      XUNLOCK (im->x.x_unlock_display, im->x.disp);
      xitk_window_set_background (wd->xwin, bg);
    }
  }
  xitk_image_free_image (&image);

  xitk_window_center_window (wd->xwin);
  
  return wd;
}

static void _xitk_window_dialog_3_destr (void *data) {
  xitk_dialog_t *wd = data;

  xitk_window_destroy_window (wd->xwin);
  if (wd->widget_list) {
    xitk_destroy_widgets (wd->widget_list);
    XLOCK (wd->imlibdata->x.x_lock_display, wd->imlibdata->x.disp);
    XFreeGC (wd->imlibdata->x.disp, wd->widget_list->gc);
    XUNLOCK (wd->imlibdata->x.x_unlock_display, wd->imlibdata->x.disp);
    XITK_WIDGET_LIST_FREE (wd->widget_list);
  }
  XITK_FREE (wd);
}

static void _xitk_window_dialog_3_done (xitk_widget_t *w, void *data) {
  xitk_dialog_t *wd = data;
  xitk_register_key_t key;

  if (wd->done3cb) {
    int state = !w ? 0
              : w == wd->w1 ? 1
              : w == wd->w2 ? 2
              : /* w == wd->w3 */ 3;
    if (wd->checkbox && xitk_checkbox_get_state (wd->checkbox))
      state += XITK_WINDOW_DIALOG_CHECKED;
    wd->done3cb (wd->done3data, state);
  }

  /* this will _xitk_window_dialog_3_destr (wd). */
  key = wd->key;
  xitk_unregister_event_handler (&key);
}

static void _checkbox_label_click (xitk_widget_t *w, void *data) {
  xitk_dialog_t *wd = (xitk_dialog_t *) data;

  (void)w;
  xitk_checkbox_set_state (wd->checkbox, !xitk_checkbox_get_state (wd->checkbox));
  xitk_checkbox_callback_exec (wd->checkbox);
}

static Window _xitk_window_get_window (xitk_window_t *w) {
  return w->window;
}

static int _xitk_window_set_focus (Display *display, Window window) {
  int t = 0;

  while ((!xitk_is_window_visible (display, window)) && (++t < 3))
    xitk_usec_sleep (5000);
  
  if (xitk_is_window_visible (display, window)) {
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

void xitk_try_to_set_input_focus(Display *display, Window window) {
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

void xitk_window_try_to_set_input_focus(xitk_window_t *w) {

  if (w == NULL)
    return;

  return xitk_try_to_set_input_focus(w->imlibdata->x.disp, w->window);
}

/*
 * Local XEvent handling.
 */
static void _window_handle_event (XEvent *event, void *data) {
  xitk_dialog_t *wd = data;
  
  switch (event->type) {

    case Expose:
      if (wd->widget_list) {
        wd->widget_list->widget_focused = wd->default_button;
        if ((wd->widget_list->widget_focused->type & WIDGET_FOCUSABLE) &&
            (wd->widget_list->widget_focused->enable == WIDGET_ENABLE)) {
          xitk_widget_t *w = wd->widget_list->widget_focused;
          widget_event_t event;
          event.type = WIDGET_EVENT_FOCUS;
          event.focus = FOCUS_RECEIVED;
          w->event (w, &event, NULL);
          w->have_focus = FOCUS_RECEIVED;
          event.type = WIDGET_EVENT_PAINT;
          w->event (w, &event, NULL);
        }
      }
      break;

    case KeyPress: {
      XKeyEvent  mykeyevent;
      KeySym     mykey;
      char       kbuf[256];
    
      mykeyevent = event->xkey;
    
      XLOCK (wd->imlibdata->x.x_lock_display, wd->imlibdata->x.disp);
      XLookupString (&mykeyevent, kbuf, sizeof (kbuf), &mykey, NULL);
      XUNLOCK (wd->imlibdata->x.x_unlock_display, wd->imlibdata->x.disp);
    
      switch (mykey) {

        case XK_Return:
          if (wd->default_button)
            _xitk_window_dialog_3_done (wd->default_button, wd);
          break;
      
        case XK_Escape:
          if (wd->default_button)
            _xitk_window_dialog_3_done (wd->default_button, wd);
          break;

      }

    }
    break;
  }
}

static const char *_xitk_window_dialog_label (const char *label) {
  switch ((uintptr_t)label) {
    case (uintptr_t)XITK_LABEL_OK: return _("OK");
    case (uintptr_t)XITK_LABEL_NO: return _("No");
    case (uintptr_t)XITK_LABEL_YES: return _("Yes");
    case (uintptr_t)XITK_LABEL_CANCEL: return _("Cancel");
    default: return label;
  }
}

xitk_register_key_t xitk_window_dialog_3 (ImlibData *im, xitk_window_t *transient_for, int layer_above,
  int width, const char *title,
  void (*done_cb)(void *userdata, int state), void *userdata,
  const char *button1_label, const char *button2_label, const char *button3_label,
  const char *check_label, int checked,
  int text_align, const char *text_fmt, ...) {
  xitk_dialog_t *wd;
  int num_buttons = !!button1_label + !!button2_label +!!button3_label;
  int winw = width, winh = TITLE_BAR_HEIGHT + 40;

  if (num_buttons)
    winh += 50;
  if (check_label)
    winh += 50;
  {
    const char *_title = title ? title : (num_buttons < 2) ? "Notice" : _("Question?");
    char *text;
    _XITK_VASPRINTF (text, text_fmt);
    wd = _xitk_dialog_new (im, _title, text, &winw, &winh, text_align);
    if (!wd)
      return 0;
  }
  wd->type = 33;

  if (num_buttons || check_label) {
    wd->widget_list = xitk_widget_list_new (im);
    wd->widget_list->win = _xitk_window_get_window (wd->xwin);
    XLOCK (wd->imlibdata->x.x_lock_display, wd->imlibdata->x.disp);
    wd->widget_list->gc = XCreateGC (im->x.disp, _xitk_window_get_window (wd->xwin), None, None);
    XUNLOCK (wd->imlibdata->x.x_unlock_display, wd->imlibdata->x.disp);
  }

  if (check_label) {
    int x = 25, y = winh - (num_buttons ? 50 : 0) - 50;
    xitk_checkbox_widget_t cb;
    xitk_label_widget_t lbl;
  
    XITK_WIDGET_INIT(&cb);
    XITK_WIDGET_INIT(&lbl);
  
    cb.skin_element_name = NULL;
    cb.callback          = NULL;
    cb.userdata          = NULL;
    wd->checkbox = xitk_noskin_checkbox_create (wd->widget_list, &cb, x, y + 5, 10, 10);
    if (wd->checkbox) {
      xitk_dlist_add_tail (&wd->widget_list->list, &wd->checkbox->node);
      xitk_checkbox_set_state (wd->checkbox, checked);
    }

    lbl.skin_element_name = NULL;
    lbl.label             = check_label;
    lbl.callback          = _checkbox_label_click;
    lbl.userdata          = wd;
    wd->checkbox_label = xitk_noskin_label_create (wd->widget_list, &lbl, x + 15, y, winw - x - 40, 20, DEFAULT_FONT_12);
    if (wd->checkbox_label)
      xitk_dlist_add_tail (&wd->widget_list->list, &wd->checkbox_label->node);
  }

  wd->done3cb = done_cb;
  wd->done3data = userdata;

  if (num_buttons) {
    xitk_labelbutton_widget_t lb;
    int bx, bdx, by, bwidth = 150;

    if (bwidth > (winw - 8) / num_buttons)
      bwidth = (winw - 8) / num_buttons;
    bx = (winw - bwidth * num_buttons) / (num_buttons + 1);
    bdx = bx + bwidth;
    by = winh - 50;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_CENTER;
    lb.callback          = _xitk_window_dialog_3_done;
    lb.state_callback    = NULL;
    lb.userdata          = wd;
    lb.skin_element_name = NULL;

    if (button1_label) {
      lb.label = _xitk_window_dialog_label (button1_label);
      wd->w1 = xitk_noskin_labelbutton_create (wd->widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w1)
        xitk_dlist_add_tail (&wd->widget_list->list, &wd->w1->node);
      bx += bdx;
    }

    if (button2_label) {
      lb.label = _xitk_window_dialog_label (button2_label);
      wd->w2 = xitk_noskin_labelbutton_create (wd->widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w2)
        xitk_dlist_add_tail (&wd->widget_list->list, &wd->w2->node);
      bx += bdx;
    }

    if (button3_label) {
      lb.label = _xitk_window_dialog_label (button3_label);
      wd->w3 = xitk_noskin_labelbutton_create (wd->widget_list, &lb,
        bx, by, bwidth, 30, "Black", "Black", "White", DEFAULT_BOLD_FONT_12);
      if (wd->w3)
        xitk_dlist_add_tail (&wd->widget_list->list, &wd->w3->node);
    }
  }
  wd->default_button = wd->w3;
  if (!wd->default_button) {
    wd->default_button = wd->w1;
    if (!wd->default_button)
      wd->default_button = wd->w2;
  }

  XLOCK (im->x.x_lock_display, im->x.disp);
  XMapRaised (im->x.disp, _xitk_window_get_window (wd->xwin));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  if (wd->w1)
    xitk_enable_and_show_widget (wd->w1);
  if (wd->w2)
    xitk_enable_and_show_widget (wd->w2);
  if (wd->w3)
    xitk_enable_and_show_widget (wd->w3);
  if (check_label) {
    xitk_enable_and_show_widget (wd->checkbox);
    xitk_enable_and_show_widget (wd->checkbox_label);
  }

  if (transient_for) {
    xitk_window_set_parent_window (wd->xwin, transient_for->window);
    XLOCK (im->x.x_lock_display, im->x.disp);
    XSetTransientForHint (im->x.disp, _xitk_window_get_window (wd->xwin), transient_for->window);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
  if (layer_above)
    xitk_window_set_window_layer (wd->xwin, layer_above);
  _xitk_window_set_focus (im->x.disp, _xitk_window_get_window (wd->xwin));

  {
    xitk_register_key_t key;
    wd->key = key = xitk_register_event_handler ("xitk_dialog_3", wd->xwin,
      _window_handle_event, NULL, NULL, wd->widget_list, wd);
    xitk_register_eh_destructor (key, _xitk_window_dialog_3_destr, wd);
    return key;
  }
}

/*
 *
 */
int xitk_is_window_iconified(Display *display, Window window) {
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

/*
 * Is window is size match with given args
 */
int xitk_is_window_visible(Display *display, Window window) {
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

int xitk_window_is_window_visible(xitk_window_t *w) {

  if (w == NULL)
    return 0;

  return xitk_is_window_visible(w->imlibdata->x.disp, w->window);
}

/*
 * Is window is size match with given args
 */
int xitk_is_window_size(Display *display, Window window, int width, int height) {
  XWindowAttributes  wattr;
  
  if((display == NULL) || (window == None))
    return -1;
  
  xitk_x_lock_display (display);
  if(!XGetWindowAttributes(display, window, &wattr)) {
    XITK_WARNING("XGetWindowAttributes() failed.n");
    xitk_x_unlock_display (display);
    return -1;
  }
  xitk_x_unlock_display (display);
  
  if((wattr.width == width) && (wattr.height == height))
    return 1;
  
  return 0;
}

/*
 * Set/Change window title.
 */
void xitk_set_window_title(Display *display, Window window, const char *title) {

  if((display == NULL) || (window == None) || (title == NULL))
    return;

  xitk_x_lock_display (display);
  XmbSetWMProperties(display, window, title, title, NULL, 0, NULL, NULL, NULL);
  xitk_x_unlock_display (display);
}

/*
 * Set/Change window title.
 */
void xitk_window_set_window_title(xitk_window_t *w, const char *title) {

  if ((w == NULL) || (title == NULL))
    return;

  xitk_set_window_title(w->imlibdata->x.disp, w->window, title);
}

/*
 *
 */

static void xitk_set_window_icon(Display *display, Window window, Pixmap icon) {

  XWMHints *wmhints;

  if ((display == NULL) || (window == None)) /* icon == None is valid */
    return;

  xitk_x_lock_display (display);

  wmhints = XAllocWMHints();
  if (wmhints) {
    wmhints->icon_pixmap   = icon;
    wmhints->flags         = IconPixmapHint;
    XSetWMHints(display, (window), wmhints);
    XFree(wmhints);
  }
  xitk_x_unlock_display (display);
}

void xitk_window_set_window_icon(xitk_window_t *w, xitk_pixmap_t *icon) {

  if (w == NULL)
    return;

  xitk_set_window_icon(w->imlibdata->x.disp, w->window, xitk_pixmap_get_pixmap(icon));
}

void xitk_window_set_layer_above(xitk_window_t *w) {

  if (w == NULL)
    return;

  xitk_set_layer_above(w->window);
}

void xitk_window_set_window_layer(xitk_window_t *w, int layer) {

  if (w == NULL)
    return;

  xitk_set_window_layer(w->window, layer);
}

/*
 *
 */

void xitk_set_window_class(Display *display, Window window, const char *res_name, const char *res_class) {

  XClassHint xclasshint, new_xclasshint;

  if ((display == NULL) || (window == None))
    return;

  xitk_x_lock_display (display);

  if ((XGetClassHint(display, (window), &xclasshint)) != 0) {
    new_xclasshint.res_name  = res_name  ? (char*)res_name  : xclasshint.res_name;
    new_xclasshint.res_class = res_class ? (char*)res_class : xclasshint.res_class;
    XSetClassHint(display, window, &new_xclasshint);
    XFree(xclasshint.res_name);
    XFree(xclasshint.res_class);
  }

  xitk_x_unlock_display (display);
}

void xitk_window_set_window_class(xitk_window_t *w, const char *res_name, const char *res_class) {

  if (w == NULL)
    return;

  xitk_set_window_class(w->imlibdata->x.disp, w->window, res_name, res_class);
}

void xitk_window_raise_window(xitk_window_t *w)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  XRaiseWindow(w->imlibdata->x.disp, w->window);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

void xitk_window_show_window(xitk_window_t *w, int raise)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  if (raise)
    XRaiseWindow(w->imlibdata->x.disp, w->window);
  XMapWindow(w->imlibdata->x.disp, w->window);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

void xitk_window_iconify_window(xitk_window_t *w)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  XIconifyWindow(w->imlibdata->x.disp, w->window, w->imlibdata->x.screen);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

void xitk_window_hide_window(xitk_window_t *w)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  XUnmapWindow(w->imlibdata->x.disp, w->window);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

void xitk_window_clear_window(xitk_window_t *w)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  XClearWindow(w->imlibdata->x.disp, w->window);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

void xitk_window_reparent_window(xitk_window_t *w, xitk_window_t *parent, int x, int y)
{
  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  XReparentWindow(w->imlibdata->x.disp, w->window,
                  parent ? parent->window : w->imlibdata->x.root, x, y);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);
}

/*
 * Get (safely) window pos.
 */
void xitk_get_window_position(Display *display, Window window, 
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
 * Get (safely) window pos.
 */
void xitk_window_get_window_position(xitk_window_t *w, int *x, int *y, int *width, int *height) {

  if (w == NULL)
    return;

  xitk_get_window_position(w->imlibdata->x.disp, w->window, x, y, width, height);
}

/*
 * Center a window in root window.
 */
void xitk_window_move_window(xitk_window_t *w, int x, int y) {
  ImlibData *im;

  if (w == NULL)
    return;

  im = w->imlibdata;
  XLOCK (im->x.x_lock_display, im->x.disp);
  XMoveResizeWindow (im->x.disp, w->window, x, y, w->width, w->height);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

}

/*
 * Center a window in root window.
 */
void xitk_window_center_window(xitk_window_t *w) {
  ImlibData    *im;
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;
  int           xx = 0, yy = 0;

  if (w == NULL)
    return;

  im = w->imlibdata;
  XLOCK (im->x.x_lock_display, im->x.disp);
  if(XGetGeometry(im->x.disp, im->x.root, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    xx = (wwin / 2) - (w->width / 2);
    yy = (hwin / 2) - (w->height / 2);
  }

  XMoveResizeWindow (im->x.disp, w->window, xx, yy, w->width, w->height);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 * Create a simple (empty) window.
 */
xitk_window_t *xitk_window_create_window(ImlibData *im, int x, int y, int width, int height) {
  xitk_window_t         *xwin;
  const char             title[] = {"xiTK Window"};
  XSizeHints             hint;
  XWMHints              *wm_hint;
  XSetWindowAttributes   attr;
  Atom                   prop, XA_WIN_LAYER, XA_DELETE_WINDOW;
  XColor                 black, dummy;
  MWMHints               mwmhints;
  XClassHint            *xclasshint;
  long                   data[1];

  if((im == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin                  = (xitk_window_t *) xitk_xmalloc(sizeof(xitk_window_t));
  xwin->imlibdata       = im;
  xwin->win_parent      = None;
  xwin->background      = NULL;
  xwin->background_mask = NULL;
  xwin->width           = width;
  xwin->height          = height;

  memset(&hint, 0, sizeof(hint));
  hint.x               = x;
  hint.y               = y;
  hint.width           = width;
  hint.base_width      = width;
  hint.min_width       = width;
  hint.max_width       = width;
  hint.height          = height;
  hint.base_height     = height;
  hint.min_height      = height;
  hint.max_height      = height;
  hint.win_gravity     = NorthWestGravity;
  hint.flags           = PWinGravity | PBaseSize | PMinSize | PMaxSize | USSize | USPosition;
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XAllocNamedColor(im->x.disp, Imlib_get_colormap(im), "black", &black, &dummy);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  attr.override_redirect = False;
  attr.background_pixel  = black.pixel;
  attr.border_pixel      = black.pixel;
  attr.colormap          = Imlib_get_colormap(im);
  attr.win_gravity       = NorthWestGravity;

  XLOCK (im->x.x_lock_display, im->x.disp);
  xwin->window = XCreateWindow(im->x.disp, im->x.root, hint.x, hint.y, hint.width, hint.height,
			       0, im->x.depth,  InputOutput, im->x.visual,
			       CWBackPixel | CWBorderPixel | CWColormap
			       | CWOverrideRedirect | CWWinGravity ,
			       &attr);
  
  XmbSetWMProperties(im->x.disp, xwin->window, title, title, NULL, 0, &hint, NULL, NULL);

  XSelectInput(im->x.disp, xwin->window, INPUT_MOTION | KeymapStateMask);
  
  XA_WIN_LAYER = XInternAtom(im->x.disp, "_WIN_LAYER", False);
  
  data[0] = 10;
  XChangeProperty(im->x.disp, xwin->window, XA_WIN_LAYER,
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data,
		  1);

  memset(&mwmhints, 0, sizeof(mwmhints));
  prop = XInternAtom(im->x.disp, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  
  XChangeProperty(im->x.disp, xwin->window, prop, prop, 32,
                  PropModeReplace, (unsigned char *) &mwmhints,
                  PROP_MWM_HINTS_ELEMENTS);
  
  XA_DELETE_WINDOW = XInternAtom(im->x.disp, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(im->x.disp, xwin->window, &XA_DELETE_WINDOW, 1);

  if((xclasshint = XAllocClassHint()) != NULL) {
    xclasshint->res_name = (char *)"Xine Window";
    xclasshint->res_class = (char *)"Xitk";
    XSetClassHint(im->x.disp, xwin->window, xclasshint);
    XFree(xclasshint);
  }
  
  wm_hint = XAllocWMHints();
  if (wm_hint != NULL) {
    wm_hint->input         = True;
    wm_hint->initial_state = NormalState;
    wm_hint->flags         = InputHint | StateHint;
    XSetWMHints(im->x.disp, xwin->window, wm_hint);
    XFree(wm_hint);
  }

  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return xwin;
}

/*
 * Create a simple painted window.
 */
xitk_window_t *xitk_window_create_simple_window(ImlibData *im, int x, int y, int width, int height) {
  xitk_window_t *xwin;
  
  if((im == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin = xitk_window_create_window(im, x, y, width, height);
  xwin->width = width;
  xwin->height = height;
  
  xwin->background = xitk_image_create_xitk_pixmap(im, width, height);
  draw_outter(xwin->background, width, height);
  xitk_window_apply_background(xwin);

  xitk_window_move_window(xwin, x, y);

  return xwin;
}

xitk_widget_list_t *xitk_window_widget_list(xitk_window_t *w)
{
  GC gc;

  if (w->widget_list)
    return w->widget_list;

  w->widget_list = xitk_widget_list_new(w->imlibdata);

  XLOCK (w->imlibdata->x.x_lock_display, w->imlibdata->x.disp);
  gc = XCreateGC (w->imlibdata->x.disp, w->window, None, None);
  XUNLOCK (w->imlibdata->x.x_unlock_display, w->imlibdata->x.disp);

  xitk_widget_list_set(w->widget_list, WIDGET_LIST_WINDOW, (void *) w->window);
  xitk_widget_list_set(w->widget_list, WIDGET_LIST_GC, gc);

  return w->widget_list;
}

/*
 * Create a simple, with title bar, window.
 */
xitk_window_t *xitk_window_create_dialog_window(ImlibData *im, const char *title,
						int x, int y, int width, int height) {
  xitk_t *xitk = gXitk;
  xitk_window_t *xwin;
  xitk_pixmap_t  *bar, *pix_bg;
  unsigned int   colorblack, colorwhite, colorgray, colordgray;
  xitk_font_t   *fs = NULL;
  int            lbear, rbear, wid, asc, des;
  int            bar_style = xitk_get_barstyle_feature();
  
  if((im == NULL) || (title == NULL) || (width == 0 || height == 0))
    return NULL;

  xwin = xitk_window_create_simple_window(im, x, y, width, height);

  xitk_window_set_window_title(xwin, title);

  bar = xitk_image_create_xitk_pixmap(im, width, TITLE_BAR_HEIGHT);
  pix_bg = xitk_image_create_xitk_pixmap(im, width, height);

  fs = xitk_font_load_font(xitk, DEFAULT_BOLD_FONT_12);
  xitk_font_set_font(fs, bar->gc);
  xitk_font_string_extent(fs, (title && strlen(title)) ? title : "Window", &lbear, &rbear, &wid, &asc, &des);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XCopyArea(im->x.disp, xwin->background->pixmap, pix_bg->pixmap, xwin->background->gc,
	    0, 0, width, height, 0, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  colorblack = xitk_get_pixel_color_black(im);
  colorwhite = xitk_get_pixel_color_white(im);
  colorgray = xitk_get_pixel_color_gray(im);
  colordgray = xitk_get_pixel_color_darkgray(im);

 /* Draw window title bar background */
  if(bar_style) {
    int s, bl = 255;
    unsigned int colorblue;

    colorblue = xitk_get_pixel_color_from_rgb(im, 0, 0, bl);
    XLOCK (im->x.x_lock_display, im->x.disp);
    for(s = 0; s <= TITLE_BAR_HEIGHT; s++, bl -= 8) {
      XSetForeground(im->x.disp, bar->gc, colorblue);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, s, width, s);
      colorblue = xitk_get_pixel_color_from_rgb(im, 0, 0, bl);
    }
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
  else {
    int s;
    unsigned int c, cd;

    cd = xitk_get_pixel_color_from_rgb(im, 115, 12, 206);
    c = xitk_get_pixel_color_from_rgb(im, 135, 97, 168);

    draw_flat_with_color(bar, width, TITLE_BAR_HEIGHT, colorgray);
    draw_rectangular_inner_box(bar, 2, 2, width - 6, (TITLE_BAR_HEIGHT - 1 - 4));
    
    XLOCK (im->x.x_lock_display, im->x.disp);
    for(s = 6; s <= (TITLE_BAR_HEIGHT - 6); s += 3) {
      XSetForeground(im->x.disp, bar->gc, c);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 5, s, (width - 8), s);
      XSetForeground(im->x.disp, bar->gc, cd);
      XDrawLine(im->x.disp, bar->pixmap, bar->gc, 5, s+1, (width - 8), s+1);
    }
    
    XSetForeground(im->x.disp, bar->gc, colorgray);
    XFillRectangle(im->x.disp, bar->pixmap, bar->gc, 
		   ((width - wid) - TITLE_BAR_HEIGHT) - 10, 6, 
		   wid + 20, TITLE_BAR_HEIGHT - 1 - 8);
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
  }
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorwhite);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, 0, width, 0);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 0, 0, 0, TITLE_BAR_HEIGHT - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorblack);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, width - 1, 0, width - 1, TITLE_BAR_HEIGHT - 1);

  XDrawLine(im->x.disp, bar->pixmap, bar->gc, 2, TITLE_BAR_HEIGHT - 1, width - 2, TITLE_BAR_HEIGHT - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colordgray);
  XDrawLine(im->x.disp, bar->pixmap, bar->gc, width - 2, 2, width - 2, TITLE_BAR_HEIGHT - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colorblack);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, width - 1, 0, width - 1, height - 1);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, 0, height - 1, width - 1, height - 1);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XSetForeground(im->x.disp, bar->gc, colordgray);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, width - 2, 0, width - 2, height - 2);
  XDrawLine(im->x.disp, pix_bg->pixmap, bar->gc, 2, height - 2, width - 2, height - 2);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(bar_style)
    XSetForeground(im->x.disp, bar->gc, colorwhite);
  else
    XSetForeground(im->x.disp, bar->gc, (xitk_get_pixel_color_from_rgb(im, 85, 12, 135)));
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  xitk_font_draw_string(fs, bar, bar->gc,
			(width - wid) - TITLE_BAR_HEIGHT, ((TITLE_BAR_HEIGHT+asc+des) >> 1) - des, title, strlen(title));

  xitk_font_unload_font(fs);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XCopyArea(im->x.disp, bar->pixmap, pix_bg->pixmap, bar->gc, 0, 0, width, TITLE_BAR_HEIGHT, 0, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  
  xitk_window_change_background(xwin, pix_bg->pixmap, width, height);
  
  xitk_image_destroy_xitk_pixmap(bar);
  xitk_image_destroy_xitk_pixmap(pix_bg);

  return xwin;
}

/*
 * Get window sizes.
 */
void xitk_window_get_window_size(xitk_window_t *w, int *width, int *height) {

  if (w == NULL) {
    *width = 0;
    *height = 0;
    return;
  }

  *width = w->width;
  *height = w->height;
}

void xitk_window_resize_window(xitk_window_t *w, int width, int height) {
  ImlibData   *im;
  XSizeHints   hint;
  int          t = 0;

  if (w == NULL)
    return;

  im = w->imlibdata;

  w->width = width;
  w->height = height;

  XLOCK (im->x.x_lock_display, im->x.disp);

  hint.width  = width;
  hint.height = height;
  hint.flags  = PPosition | PSize;
  XSetWMNormalHints(im->x.disp, w->window, &hint);
  XResizeWindow (im->x.disp, w->window, width, height);
  XSync(im->x.disp, False);

  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  while (t < 10 && !xitk_is_window_size(im->x.disp, w->window, w->width, w->height)) {
    xitk_usec_sleep(10000);
  }
}

/*
 * Get window (X) id.
 */
Window xitk_window_get_window(xitk_window_t *w) {
  return w ? _xitk_window_get_window (w) : None;
}

/*
 * Return window background pixmap.
 */
Pixmap xitk_window_get_background(xitk_window_t *w) {

  if(w == NULL)
    return None;

  return w->background->pixmap;
}

xitk_pixmap_t *xitk_window_get_background_pixmap(xitk_window_t *w) {

  ImlibData *im;
  xitk_pixmap_t *pixmap;
  int width, height;

  if(w == NULL)
    return None;

  im = w->imlibdata;

  xitk_window_get_window_size(w, &width, &height);
  pixmap = xitk_image_create_xitk_pixmap(im, width, height);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XCopyArea(im->x.disp, xitk_window_get_background(w), pixmap->pixmap,
            pixmap->gc, 0, 0, width, height, 0, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return pixmap;
}

/*
 * Return window background pixmap.
 */
Pixmap xitk_window_get_background_mask(xitk_window_t *w) {

  if(w == NULL)
    return None;

  if(w->background_mask == NULL)
    return None;
  
  return w->background->pixmap;
}

/*
 * Apply (draw) window background.
 */
void xitk_window_apply_background(xitk_window_t *w) {
  ImlibData *im;

  if (w == NULL)
    return;

  im = w->imlibdata;
  XLOCK (im->x.x_lock_display, im->x.disp);
  
  XSetWindowBackgroundPixmap(im->x.disp, w->window, w->background->pixmap);
  
  if(w->background_mask)
    XShapeCombineMask(im->x.disp, w->window, ShapeBounding, 0, 0, w->background_mask->pixmap, ShapeSet);
  else
    XShapeCombineMask(im->x.disp, w->window, ShapeBounding, 0, 0, 0, ShapeSet);

  XClearWindow(im->x.disp, w->window);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
}

/*
 * Change window background with 'bg', then draw it.
 */
int xitk_window_change_background(xitk_window_t *w, Pixmap bg, int width, int height) {

  ImlibData     *im;
  xitk_pixmap_t *pixmap;

  if ((w == NULL) || (bg == None) || (width == 0 || height == 0))
    return 0;

  im = w->imlibdata;

  pixmap = xitk_image_create_xitk_pixmap(im, width, height);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XCopyArea(im->x.disp, bg, pixmap->pixmap, pixmap->gc, 0, 0, width, height, 0, 0);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return xitk_window_set_background(w, pixmap);
}

int xitk_window_set_background(xitk_window_t *w, xitk_pixmap_t *bg) {

  ImlibData    *im;
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;

  if ((w == NULL) || (bg == NULL))
    return 0;

  im = w->imlibdata;

  xitk_image_destroy_xitk_pixmap(w->background);
  if(w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->background      = bg;
  w->background_mask = NULL;

  XLOCK (im->x.x_lock_display, im->x.disp);
  if(XGetGeometry(im->x.disp, w->window, &rootwin,
                  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {

    XResizeWindow (im->x.disp, w->window, wwin, hwin);
  }
  else {
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    return 0;
  }

  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  xitk_window_apply_background(w);

  return 1;
}

/*
 * Change window background with img, then draw it.
 */
int xitk_window_change_background_with_image(xitk_window_t *w, xitk_image_t *img, int width, int height) {
  ImlibData    *im;
  Window        rootwin;
  int           xwin, ywin;
  unsigned int  wwin, hwin, bwin, dwin;

  if ((w == NULL) || (img == NULL) || (width == 0 || height == 0))
    return 0;

  im = w->imlibdata;

  if (w->background)
    xitk_image_destroy_xitk_pixmap(w->background);
  if(w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->background      = xitk_image_create_xitk_pixmap(im, width, height);
  w->background_mask = NULL;
  
  if(img->mask)
    w->background_mask = xitk_image_create_xitk_mask_pixmap(im, width, height);
  
  XLOCK (im->x.x_lock_display, im->x.disp);
  if(XGetGeometry(im->x.disp, w->window, &rootwin, 
		  &xwin, &ywin, &wwin, &hwin, &bwin, &dwin) != BadDrawable) {
    
    XResizeWindow (im->x.disp, w->window, wwin, hwin);
  }
  else {
    XUNLOCK (im->x.x_unlock_display, im->x.disp);
    return 0;
  }
  
  XCopyArea(im->x.disp, img->image->pixmap, w->background->pixmap, w->background->gc, 0, 0, width, height, 0, 0);
  if(w->background_mask)
    XCopyArea(im->x.disp, img->mask->pixmap, w->background_mask->pixmap, w->background_mask->gc, 0, 0, width, height, 0, 0);
  
  XUNLOCK (im->x.x_unlock_display, im->x.disp);
  xitk_window_apply_background(w);
  
  return 1;
}

/*
void xitk_window_set_modal(xitk_window_t *w) {
  xitk_modal_window(w->window);  
}
void xitk_window_dialog_set_modal(xitk_window_t *w) {
  xitk_dialog_t *wd = w->dialog;
  xitk_window_set_modal(wd->xwin);
}
*/

void xitk_window_destroy_window(xitk_window_t *w) {

  ImlibData *im = w->imlibdata;

  if (w->widget_list)
    xitk_destroy_widgets(w->widget_list);

  xitk_clipboard_unregister_window (w->window);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XUnmapWindow(im->x.disp, w->window);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  if(w->background)
    xitk_image_destroy_xitk_pixmap(w->background);
  if(w->background_mask)
    xitk_image_destroy_xitk_pixmap(w->background_mask);

  w->width = -1;
  w->height = -1;

  //xitk_unmodal_window(w->window);

  XLOCK (im->x.x_lock_display, im->x.disp);
  XDestroyWindow(im->x.disp, w->window);

  if((w->win_parent != None) && xitk_is_window_visible(im->x.disp, w->win_parent))
    XSetInputFocus(im->x.disp, w->win_parent, RevertToParent, CurrentTime);

  if (w->widget_list)
    XFreeGC (im->x.disp, XITK_WIDGET_LIST_GC (w->widget_list));

  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  if (w->widget_list)
    XITK_WIDGET_LIST_FREE(w->widget_list);

  XITK_FREE(w);
}

/**
 *
 * @TODO Should be split on a different unit, as it's only used with TAR support
 *       enabled.
 */
#if 0
void xitk_window_dialog_destroy(xitk_window_t *w) {
  xitk_dialog_t *wd = w->dialog;

  if(wd) {
    xitk_window_destroy_window(wd->xwin);
    xitk_unregister_event_handler(&wd->key);
    
    if(wd->widget_list) {
      xitk_destroy_widgets(wd->widget_list);
      XLOCK (wd->imlibdata->x.x_lock_display, wd->imlibdata->x.disp);
      XFreeGC(wd->imlibdata->x.disp, wd->widget_list->gc);
      XUNLOCK (wd->imlibdata->x.x_unlock_display, wd->imlibdata->x.disp);
      
      /* xitk_dlist_init (&wd->widget_list->list); */
     
     XITK_WIDGET_LIST_FREE(wd->widget_list); 
    }
    XITK_FREE(wd);
  }  
}
#endif
void xitk_window_set_parent_window(xitk_window_t *xwin, Window parent) {
  if(xwin)
    xwin->win_parent = parent;
}
