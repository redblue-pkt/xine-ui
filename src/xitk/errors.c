/* 
 * Copyright (C) 2000-2002 the xine project
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

#include "xitk.h"
#include "event.h"
#include "i18n.h"
#include "viewlog.h"

extern gGui_t          *gGui;

/*
 * Callback used to display the viewlog window.
 */
static void _errors_display_log(xitk_widget_t *w, void *data, int state) {
  
  if(viewlog_is_visible())
    viewlog_raise_window();
  else
    viewlog_window();
}

/*
 * Create the real window.
 */
void errors_create_window(char *title, char *message) {
  
  if((title == NULL) || (message == NULL))
    return;
  
  xitk_window_dialog_two_buttons_with_width(gGui->imlib_data, title, 
					    _("Done"), _("More..."),
					    NULL, _errors_display_log, 
					    NULL, 400, ALIGN_CENTER,
					    message);
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
  
  xitk_window_dialog_error(gGui->imlib_data, buf);
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
  
  errors_create_window(_("Error"), buf);
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
  
  xitk_window_dialog_info(gGui->imlib_data, buf);
  free(buf);
}

/*
 * Display an error window error from a xine engine error.
 */
void gui_handle_xine_error(void) {
  int err;

  err = xine_get_error(gGui->xine);

  switch(err) {

  case XINE_ERROR_NONE:
    /* noop */
    break;
    
  case XINE_ERROR_NO_INPUT_PLUGIN:
    xine_error_with_more(_("- xine engine error -\n\n"
			   "There is no available input plugin available to handle '%s'.\n"),
			 gGui->filename);
    break;
    
  case XINE_ERROR_NO_DEMUXER_PLUGIN:
    xine_error_with_more(_("- xine engine error -\n\nThere is no available demuxer plugin "
			   "to handle '%s'.\n"), gGui->filename);
    break;
    
  default:
    xine_error_with_more(_("- xine engine error -\n\n!! Unhandled error !!\n"));
    break;
  }


}

