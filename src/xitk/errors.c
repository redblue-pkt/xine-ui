/* 
 * Copyright (C) 2000-2004 the xine project
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
 * $Id$
 *
 * xine engine error handling/notification
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"

extern gGui_t          *gGui;

/*
 * Callback used to display the viewlog window.
 */
static void _errors_display_log(xitk_widget_t *w, void *data, int state) {
  
  if(viewlog_is_visible())
    viewlog_raise_window();
  else
    viewlog_panel();
}

/*
 * Create the real window.
 */
static void errors_create_window(char *title, char *message) {
  xitk_window_t *xw;

  if((title == NULL) || (message == NULL))
    return;
  
  dump_error(gGui->verbosity, message);

  if( gGui->nongui_error_msg ) {
    gGui->nongui_error_msg( message );
    return;
  }
  
  xw = xitk_window_dialog_two_buttons_with_width(gGui->imlib_data, title, 
						 _("Done"), _("More..."),
						 NULL, _errors_display_log, 
						 NULL, 400, ALIGN_CENTER,
						 "%s", message);

  xitk_window_set_parent_window(xw, gGui->video_window);

  if(!gGui->use_root_window && gGui->video_display == gGui->display) {
    XLockDisplay(gGui->display);
    XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
    XUnlockDisplay(gGui->display);
  }

  layer_above_video(xitk_window_get_window(xw));
}

/*
 * Display an error window.
 */
void xine_error(char *message, ...) {
  va_list   args;
  char     *buf;
  int       n, size = 100;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return;
  
  while(1) {
    
    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return;
  }
  
  if(gGui->stdctl_enable || !gGui->display) {
    printf("%s\n", buf);
  }
  else {
    char buf2[(strlen(buf) * 2) + 1];
    
    dump_error(gGui->verbosity, buf);

    xitk_subst_special_chars(buf, buf2);

    if( gGui->nongui_error_msg ) {
      gGui->nongui_error_msg( buf2 );
    } else {
      xitk_window_t *xw;

      xw = xitk_window_dialog_error(gGui->imlib_data, "%s", buf2);

      if(!gGui->use_root_window && gGui->video_display == gGui->display) {
        XLockDisplay(gGui->display);
        XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
        XUnlockDisplay(gGui->display);
      }
      layer_above_video(xitk_window_get_window(xw));
    }
  }

  free(buf);
}

/*
 * Display an error window, with more button.
 */
void xine_error_with_more(char *message, ...) {
  va_list   args;
  char     *buf;
  int       n, size = 100;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return;
  
  while(1) {
    
    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return;
  }
  
  dump_error(gGui->verbosity, buf);

  if(gGui->stdctl_enable) {
    printf("%s\n", buf);
  }
  else {
    char buf2[(strlen(buf) * 2) + 1];

    xitk_subst_special_chars(buf, buf2);

    errors_create_window(_("Error"), buf2);
  }
  
  free(buf);
}

/*
 * Display an informative window.
 */
void xine_info(char *message, ...) {
  va_list   args;
  char     *buf;
  int       n, size = 100;
  
  if((buf = xitk_xmalloc(size)) == NULL) 
    return;
  
  while(1) {
    
    va_start(args, message);
    n = vsnprintf(buf, size, message, args);
    va_end(args);
    
    if(n > -1 && n < size)
      break;
    
    if(n > -1)
      size = n + 1;
    else
      size *= 2;
    
    if((buf = realloc(buf, size)) == NULL)
      return;
  }
  
  dump_info(gGui->verbosity, buf);

  if(gGui->stdctl_enable) {
    printf("%s\n", buf);
  }
  else {
    char           buf2[(strlen(buf) * 2) + 1];

    xitk_subst_special_chars(buf, buf2);

    if( gGui->nongui_error_msg ) {
      gGui->nongui_error_msg( buf2 );
    } else {
      xitk_window_t *xw;

      xw = xitk_window_dialog_info(gGui->imlib_data, "%s", buf2);

      if(!gGui->use_root_window && gGui->video_display == gGui->display) {
        XLockDisplay(gGui->display);
        XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
        XUnlockDisplay(gGui->display);
      }
      layer_above_video(xitk_window_get_window(xw));
    }
  }

  free(buf);
}

/*
 * Display an error window error from a xine engine error.
 */
void gui_handle_xine_error(xine_stream_t *stream, char *mrl) {
  int   err;
  char *_mrl = mrl;

  if(_mrl == NULL)
    _mrl = (stream == gGui->stream) ? gGui->mmk.mrl : gGui->visual_anim.mrls[gGui->visual_anim.current];

  err = xine_get_error(stream);

  switch(err) {

  case XINE_ERROR_NONE:
    dump_error(gGui->verbosity, "got XINE_ERROR_NONE.");
    /* noop */
    break;
    
  case XINE_ERROR_NO_INPUT_PLUGIN:
    dump_error(gGui->verbosity, "got XINE_ERROR_NO_INPUT_PLUGIN.");
    xine_error_with_more(_("- xine engine error -\n\n"
			   "There is no input plugin available to handle '%s'.\n"
			   "Maybe MRL syntax is wrong or file/stream source doesn't exist."),
			 _mrl);
    break;
    
  case XINE_ERROR_NO_DEMUX_PLUGIN:
    dump_error(gGui->verbosity, "got XINE_ERROR_NO_DEMUX_PLUGIN.");
    xine_error_with_more(_("- xine engine error -\n\nThere is no demuxer plugin available "
			   "to handle '%s'.\n"
			   "Usually this means that the file format was not recognized."), 
			 _mrl);
    break;
    
  case XINE_ERROR_DEMUX_FAILED:
    dump_error(gGui->verbosity, "got XINE_ERROR_DEMUX_FAILED.");
    xine_error_with_more(_("- xine engine error -\n\nDemuxer failed. "
			   "Maybe '%s' is a broken file?\n"), _mrl);
    break;
    
  case XINE_ERROR_MALFORMED_MRL:
    dump_error(gGui->verbosity, "got XINE_ERROR_MALFORMED_MRL.");
    xine_error_with_more(_("- xine engine error -\n\nMalformed mrl. "
			   "Mrl '%s' seems malformed/invalid.\n"), _mrl);
    break;

  case XINE_ERROR_INPUT_FAILED:
    dump_error(gGui->verbosity, "got XINE_ERROR_INPUT_FAILED.");
    xine_error_with_more(_("- xine engine error -\n\nInput plugin failed to open "
			   "mrl '%s'\n"), _mrl);
    break;
    
  default:
    dump_error(gGui->verbosity, "got unhandle error.");
    xine_error_with_more(_("- xine engine error -\n\n!! Unhandled error !!\n"));
    break;
  }
  
  gGui->new_pos = -1;
}


static void _dont_show_too_slow_again(xitk_widget_t *w, void *data, int state) {
  config_update_num ("gui.dropped_frames_warning", !state);
}

static void _learn_more_about_too_slow(xitk_widget_t *w, void *data, int state) {
  /*
   * fixme: how to properly open the system browser? 
   * should we just make it configurable? 
   */
  xine_info(_("Opening mozilla web browser, this might take a while..."));
  system ("mozilla http://xinehq.de/index.php/faq#SPEEDUP");
}

/*
 * Create the real window.
 */
void too_slow_window(void) {
  xitk_window_t *xw;
  char *title, *message;
  int display_warning;
  int checked = 0;
    
  title = _("Warning");
  message = _("The amount of dropped frame is too high, your system might be slow, not properly optimized or just too loaded.\n\nhttp://xinehq.de/index.php/faq#SPEEDUP");
  
  dump_error(gGui->verbosity, message);

  display_warning = xine_config_register_bool (__xineui_global_xine_instance, "gui.dropped_frames_warning", 
		     1,
		     _("Warn user when too much frames are dropped."),
		     CONFIG_NO_HELP,
		     CONFIG_LEVEL_ADV,
		     CONFIG_NO_CB,
		     CONFIG_NO_DATA);

  if( !display_warning )
    return;

  if( gGui->nongui_error_msg || gGui->stdctl_enable )
    return;
      
  _dont_show_too_slow_again(NULL, NULL, checked);
    
  xw = xitk_window_dialog_checkbox_two_buttons_with_width(gGui->imlib_data, title, 
						 _("Done"), _("Learn More..."),
						 NULL, _learn_more_about_too_slow, 
                                                 _("Disable this warning."),
                                                 checked, _dont_show_too_slow_again,
						 NULL, 500, ALIGN_CENTER,
						 "%s", message);
  if(!gGui->use_root_window && gGui->video_display == gGui->display) {
    XLockDisplay(gGui->display);
    XSetTransientForHint(gGui->display, xitk_window_get_window(xw), gGui->video_window);
    XUnlockDisplay(gGui->display);
  }
  layer_above_video(xitk_window_get_window(xw));
}
