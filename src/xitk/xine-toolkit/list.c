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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "_xitk.h"

/*
 *
 */
xitk_list_t *xitk_list_new (void) {
  xitk_list_t *list;

  list = (xitk_list_t *) xitk_xmalloc(sizeof(xitk_list_t));

  list->first=NULL;
  list->last =NULL;
  list->cur  =NULL;

  return list;
}

/*
 *
 */
void xitk_list_clear(xitk_list_t *l) {
  xitk_node_t *node;

  if (!l) {
    XITK_WARNING("No list.\n");
    return;
  }
 
  if(!l->first)
    return;

  node = l->first;
  
  while(node) {
    xitk_node_t *n = node;
    
    /* XITK_FREE(n->content);  Content should (IS) be fried elswhere */
    node = n->next;
    free(n);
  }
  
  l->first = l->cur = l->last = NULL;
}

/*
 *
 */
void xitk_list_free(xitk_list_t *l) {
  if (!l) {
    XITK_WARNING("No list.\n");
    return;
  }

  xitk_list_clear(l);
  free(l);
}

/*
 *
 */
void *xitk_list_first_content (xitk_list_t *l) {

  l->cur = l->first;

  if (l->first) 
    return l->first->content;
  else
    return NULL;
}

/*
 *
 */
void *xitk_list_next_content (xitk_list_t *l) {
  if (l->cur) {

    if (l->cur->next) {
      l->cur = l->cur->next;
      return l->cur->content;
    } 
    else
      return NULL;
    
  } 
  else
    return NULL;

}

/*
 *
 */
int xitk_list_is_empty (xitk_list_t *l) {

  return (l->first != NULL);
}

/*
 *
 */
void *xitk_list_last_content (xitk_list_t *l) {

  if (l->last) {
    l->cur = l->last;
    return l->last->content;
  } 

  return NULL;
}

/*
 *
 */
void *xitk_list_prev_content (xitk_list_t *l) {

  if (l->cur) {
    if (l->cur->prev) {
      l->cur = l->cur->prev;
      return l->cur->content;
    } 
  } 

  return NULL;
}

/*
 *
 */
void xitk_list_append_content (xitk_list_t *l, void *content) {
  xitk_node_t *node;

  node = (xitk_node_t *) xitk_xmalloc(sizeof(xitk_node_t));
  node->content = content;

  if (l->last) {
    node->next = NULL;
    node->prev = l->last;
    l->last->next = node;
    l->last = node;
    l->cur = node;
  } 
  else {
    l->first = l->last = l->cur = node;
    node->prev = node->next = NULL;
  }
}

/*
 *
 */
void xitk_list_insert_content (xitk_list_t *l, void *content) {
  xitk_node_t *nodecur, *nodenext, *nodenew;
  
  if(l->cur->next) {
    nodenew = (xitk_node_t *) xitk_xmalloc(sizeof(xitk_node_t));

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
    xitk_list_append_content(l, content);
  }

}

/*
 *
 */
void xitk_list_delete_current (xitk_list_t *l) {
  xitk_node_t *node_cur;

  node_cur = l->cur;

  if(node_cur->prev) {
    node_cur->prev->next = node_cur->next;
  } 
  else { /* First entry */
    l->first = node_cur->next;
  }
  
  if(node_cur->next) {
    node_cur->next->prev = node_cur->prev;
    l->cur = node_cur->next;
  }
  else { /* last entry in the list */
    l->last = l->cur = node_cur->prev;
  }

  //  XITK_FREE(node_cur->content);
  free(node_cur);
}
