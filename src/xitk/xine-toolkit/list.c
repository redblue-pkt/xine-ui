/* 
 * Copyright (C) 2000-2001 the xine project
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "widget.h"
#include "list.h"

/*
 *
 */
gui_list_t *gui_list_new (void) {
  gui_list_t *list;

  list = (gui_list_t *) gui_xmalloc(sizeof(gui_list_t));

  list->first=NULL;
  list->last =NULL;
  list->cur  =NULL;

  return list;
}

/*
 *
 */
void gui_list_free(gui_list_t *l) {

  l->cur = l->first;

  while(l->cur->next) {
    free(l->cur->content);
    l->cur = l->cur->next;
  }

  l->first = l->cur = l->last = NULL;
}

/*
 *
 */
void *gui_list_first_content (gui_list_t *l) {

  l->cur = l->first;

  if (l->first) 
    return l->first->content;
  else
    return NULL;
}

/*
 *
 */
void *gui_list_next_content (gui_list_t *l) {
  if (l->cur) {

    if (l->cur->next) {
      l->cur = l->cur->next;
      return l->cur->content;
    } else
      return NULL;

  } else {

    fprintf (stderr, "gui_list : passed end of list");

    return NULL;
  }    
}

/*
 *
 */
int gui_list_is_empty (gui_list_t *l) {

  return (l->first != NULL);
}

/*
 *
 */
void *gui_list_last_content (gui_list_t *l) {
  if (l->last) {

    l->cur = l->last;
    return l->last->content;

  } else {


    fprintf (stderr, "gui_list : wanted last of empty list");

    return NULL;
  }    
}

/*
 *
 */
void *gui_list_prev_content (gui_list_t *l){

  if (l->cur) {

    if (l->cur->prev) {
      l->cur = l->cur->prev;
      return l->cur->content;
    } else
      return NULL;

  } else {

    fprintf (stderr, "gui_list : passed begin of list");

    return NULL;
  }    
}

/*
 *
 */
void gui_list_append_content (gui_list_t *l, void *content) {
  gui_node_t *node;

  node = (gui_node_t *) gui_xmalloc(sizeof(gui_node_t));
  node->content = content;

  if (l->last) {

    node->next = NULL;

    node->prev = l->last;
    l->last->next = node;
    l->last = node;

    l->cur = node;
    
  } else {
    l->first = l->last = l->cur = node;
    node->prev = node->next = NULL;
  }
}

/*
 *
 */
void gui_list_insert_content (gui_list_t *l, void *content) {
  gui_node_t *nodecur, *nodenext, *nodenew;
  
  if(l->cur->next) {
    nodenew = (gui_node_t *) gui_xmalloc(sizeof(gui_node_t));

    nodenew->content = content;
    
    nodecur = l->cur;
    nodenext = l->cur->next;

    nodecur->next = nodenew;
    nodenext->prev = nodenew;

    nodenew->prev = nodecur;
    nodenew->next = nodenext;

    l->cur = nodenew;
  }
  else { /* current is last, append to the list */
    gui_list_append_content(l, content);
  }

}

/*
 *
 */
void gui_list_delete_current (gui_list_t *l) {

  gui_node_t *node_cur;

  node_cur = l->cur;

  if(node_cur->prev) {

    node_cur->prev->next = node_cur->next;

  } else { /* First entry */

    l->first = node_cur->next;
    
  }
  
  if(node_cur->next) {

    node_cur->next->prev = node_cur->prev;
    l->cur = node_cur->next;

  }  else { /* last entry in the list */

    l->last = node_cur->prev;

    l->cur = node_cur->prev;
  }

  free(node_cur->content);
  free(node_cur);

}


