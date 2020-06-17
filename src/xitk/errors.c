/* 
 * Copyright (C) 2000-2019 the xine project
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
 * xine engine error handling/notification
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>

#include "dump.h"

#include "common.h"

/*
 * Callback used to display the viewlog window.
 */
static void _errors_display_log_3 (void *data, int state) {
  gGui_t *gui = data;

  (void)gui;
  if (state == 2) {
    if (viewlog_is_visible ())
      viewlog_raise_window ();
    else
      viewlog_panel ();
  }
}

/*
 * Create the real window.
 */
static void errors_create_window (gGui_t *gui, char *title, char *message) {

  if((title == NULL) || (message == NULL))
    return;
  
  dump_error(message);

  if (gui->nongui_error_msg) {
    gui->nongui_error_msg (message);
    return;
  }
  xitk_register_key_t key =
  xitk_window_dialog_3 (gui->xitk,
    NULL,
    get_layer_above_video (gui), 400, title, _errors_display_log_3, gui,
    _("Done"), _("More..."), NULL, NULL, 0, ALIGN_CENTER, "%s", message);
  video_window_set_transient_for(gui->vwin, xitk_get_window(key));
}

/*
 * Display an error window.
 */
void xine_error (gGui_t *gui, const char *message, ...) {
  va_list   args;
  char     *buf;

  va_start(args, message);
  buf = xitk_vasprintf(message, args);
  va_end(args);

  if (!buf)
    return;

  if (gui->stdctl_enable || !gui->xitk) {
    printf("%s\n", buf);
  }
  else {
    dump_error(buf);

    if (gui->nongui_error_msg) {
      gui->nongui_error_msg (buf);
    } else {
      xitk_register_key_t key =
      xitk_window_dialog_3 (gui->xitk,
        NULL,
        get_layer_above_video (gui), 400, XITK_TITLE_ERROR, NULL, NULL,
        XITK_LABEL_OK, NULL, NULL, NULL, 0, ALIGN_CENTER, "%s", buf);
      video_window_set_transient_for(gui->vwin, xitk_get_window(key));
    }
  }

  free(buf);
}

/*
 * Display an error window, with more button.
 */
void xine_error_with_more (gGui_t *gui, const char *message, ...) {
  va_list   args;
  char     *buf;

  va_start(args, message);
  buf = xitk_vasprintf(message, args);
  va_end(args);

  if (!buf)
    return;

  dump_error(buf);

  if (gui->stdctl_enable) {
    printf("%s\n", buf);
  }
  else {
    errors_create_window (gui, _("Error"), buf);
  }
  
  free(buf);
}

/*
 * Display an informative window.
 */
void xine_info (gGui_t *gui, const char *message, ...) {
  va_list   args;
  char     *buf;

  va_start(args, message);
  buf = xitk_vasprintf(message, args);
  va_end(args);

  if (!buf)
    return;

  dump_info(buf);

  if (gui->stdctl_enable) {
    printf("%s\n", buf);
  }
  else {
    if (gui->nongui_error_msg) {
      gui->nongui_error_msg (buf);
    } else {
      xitk_register_key_t key =
      xitk_window_dialog_3 (gui->xitk,
        NULL,
        get_layer_above_video (gui), 400, XITK_TITLE_INFO, NULL, NULL,
        XITK_LABEL_OK, NULL, NULL, NULL, 0, ALIGN_CENTER, "%s", buf);
      video_window_set_transient_for(gui->vwin, xitk_get_window(key));
    }
  }

  free(buf);
}

/*
 * Display an error window error from a xine engine error.
 */
void gui_handle_xine_error (gGui_t *gui, xine_stream_t *stream, const char *mrl) {
  int   err;
  const char *_mrl = mrl;

  if(_mrl == NULL)
    _mrl = (stream == gui->stream) ? gui->mmk.mrl : gui->visual_anim.mrls [gui->visual_anim.current];

  err = xine_get_error(stream);

  switch(err) {

  case XINE_ERROR_NONE:
    dump_error("got XINE_ERROR_NONE.");
    /* noop */
    break;
    
  case XINE_ERROR_NO_INPUT_PLUGIN:
    dump_error("got XINE_ERROR_NO_INPUT_PLUGIN.");
    xine_error_with_more (gui,
      _("- xine engine error -\n\n"
        "There is no input plugin available to handle '%s'.\n"
        "Maybe MRL syntax is wrong or file/stream source doesn't exist."), _mrl);
    break;
    
  case XINE_ERROR_NO_DEMUX_PLUGIN:
    dump_error("got XINE_ERROR_NO_DEMUX_PLUGIN.");
    xine_error_with_more (gui,
      _("- xine engine error -\n\n"
        "There is no demuxer plugin available to handle '%s'.\n"
        "Usually this means that the file format was not recognized."), _mrl);
    break;
    
  case XINE_ERROR_DEMUX_FAILED:
    dump_error("got XINE_ERROR_DEMUX_FAILED.");
    xine_error_with_more (gui,
      _("- xine engine error -\n\n"
        "Demuxer failed. Maybe '%s' is a broken file?\n"), _mrl);
    break;
    
  case XINE_ERROR_MALFORMED_MRL:
    dump_error("got XINE_ERROR_MALFORMED_MRL.");
    xine_error_with_more (gui,
      _("- xine engine error -\n\n"
        "Malformed mrl. Mrl '%s' seems malformed/invalid.\n"), _mrl);
    break;

  case XINE_ERROR_INPUT_FAILED:
    dump_error("got XINE_ERROR_INPUT_FAILED.");
    xine_error_with_more (gui,
      _("- xine engine error -\n\n"
        "Input plugin failed to open mrl '%s'\n"), _mrl);
    break;
    
  default:
    dump_error("got unhandle error.");
    xine_error_with_more (gui, _("- xine engine error -\n\n!! Unhandled error !!\n"));
    break;
  }
  
  /* gui->new_pos = -1; */
}

static void _too_slow_done (void *data, int state) {
  gGui_t *gui = data;

  if (state & XITK_WINDOW_DIALOG_CHECKED)
    config_update_num ("gui.dropped_frames_warning", 0);

  if ((state & XITK_WINDOW_DIALOG_BUTTONS_MASK) == 2) {
    /* FIXME: how to properly open the system browser?
     * should we just make it configurable? */
    xine_info (gui, _("Opening mozilla web browser, this might take a while..."));
    xine_system (1, "mozilla http://www.xine-project.org/faq#SPEEDUP");
  }
}

/*
 * Create the real window.
 */
void too_slow_window (gGui_t *gui) {
  char *message;
  int display_warning;
    
  message = _("The amount of dropped frame is too high, your system might be slow, not properly optimized or just too loaded.\n\nhttp://www.xine-project.org/faq#SPEEDUP");
  
  dump_error(message);

  display_warning = xine_config_register_bool (__xineui_global_xine_instance, "gui.dropped_frames_warning", 
		     1,
		     _("Warn user when too much frames are dropped."),
		     CONFIG_NO_HELP,
		     CONFIG_LEVEL_ADV,
		     CONFIG_NO_CB,
		     CONFIG_NO_DATA);

  if( !display_warning )
    return;

  if (gui->nongui_error_msg || gui->stdctl_enable)
    return;

  xitk_register_key_t key =
  xitk_window_dialog_3 (gui->xitk,
    NULL,
    get_layer_above_video (gui), 500, XITK_TITLE_WARN, _too_slow_done, gui,
    _("Done"), _("Learn More..."), NULL, _("Disable this warning."), 0, ALIGN_CENTER, "%s", message);
  video_window_set_transient_for(gui->vwin, xitk_get_window(key));
}
