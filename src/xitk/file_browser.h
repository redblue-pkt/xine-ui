/*
 * Copyright (C) 2000-2003 the xine project
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
 */

#ifndef __FILE_BROWSER_H__
#define __FILE_BROWSER_H__

typedef struct filebrowser_s filebrowser_t;

typedef void (*filebrowser_callback_t)(filebrowser_t *);

typedef struct {
  char                     *label;
  filebrowser_callback_t    callback;
  int                       need_a_file;
} filebrowser_callback_button_t;
		

filebrowser_t *create_filebrowser(char *, char *,
				  filebrowser_callback_button_t *, 
				  filebrowser_callback_button_t *,
				  filebrowser_callback_button_t *);

void filebrowser_raise_window(filebrowser_t *);
void filebrowser_end(filebrowser_t *);
char *filebrowser_get_current_dir(filebrowser_t *);
char *filebrowser_get_current_filename(filebrowser_t *);
char *filebrowser_get_full_filename(filebrowser_t *);
char **filebrowser_get_all_files(filebrowser_t *);


#endif
