/* 
 * Copyright (C) 2000 the xine project
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

#ifndef HAVE_GUI_H
#define HAVE_GUI_H

typedef struct gui_node_s {

  struct gui_node_s    *next, *prev;

  void                 *content;

} gui_node_t;


typedef struct gui_list_s {

  gui_node_t    *first, *last, *cur;

} gui_list_t;


gui_list_t *gui_list_new (void);

void gui_list_free(gui_list_t *l);

int gui_list_is_empty (gui_list_t *l);

void *gui_list_first_content (gui_list_t *l);

void *gui_list_next_content (gui_list_t *l);

void *gui_list_last_content (gui_list_t *l);

void *gui_list_prev_content (gui_list_t *l);

void gui_list_append_content (gui_list_t *l, void *content);

void gui_list_insert_content (gui_list_t *l, void *content);

void gui_list_delete_current (gui_list_t *l);

#endif
