/*
 * Copyright (C) 2000-2021 the xine project
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

#ifndef __FILE_BROWSER_H__
#define __FILE_BROWSER_H__

typedef void (*filebrowser_callback_t) (filebrowser_t *fb, void *userdata);
typedef int (*hidden_file_toggle_t)(void *data, int action, int value);

typedef struct {
  char                     *label;    /** << user visible text */
  filebrowser_callback_t    callback; /** << fired when user clicks... */
  void                     *userdata; /** << passed to callback */
  int                       need_a_file; /** << ...and there is a file selected if this is set */
} filebrowser_callback_button_t;

/** NOTE: after return from cbb{1|2|_close}->callback, this filebrowser instance will delete itself.
 *        make sure to NULL your pointers to it inside these callbacks. */
filebrowser_t *create_filebrowser (gGui_t *gui, const char *window_title, const char *filepathname,
  hidden_file_toggle_t hidden_callback, void *hidden_cb_data,
  const filebrowser_callback_button_t *cbb1, const filebrowser_callback_button_t *cbb2,
  const filebrowser_callback_button_t *cbb_close);

void filebrowser_raise_window(filebrowser_t *);
void filebrowser_end(filebrowser_t *);
char *filebrowser_get_current_dir(filebrowser_t *);
char *filebrowser_get_current_filename(filebrowser_t *);
char *filebrowser_get_full_filename(filebrowser_t *);
char **filebrowser_get_all_files(filebrowser_t *);


#endif
