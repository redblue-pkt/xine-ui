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
 * Doublly linked list helpers.
 *
 */

#ifndef DLIST_H_
#define DLIST_H_

typedef struct xitk_dnode_st {
  struct xitk_dnode_st *next, *prev;
} xitk_dnode_t;

typedef struct {
  xitk_dnode_t head, tail;
} xitk_dlist_t;

static inline void xitk_dlist_init (xitk_dlist_t *list) {
  list->head.next = &list->tail;
  list->head.prev = NULL;
  list->tail.next = NULL;
  list->tail.prev = &list->head;
}

static inline void xitk_dnode_init (xitk_dnode_t *node) {
  node->next = node->prev = NULL;
}

static inline void xitk_dnode_remove (xitk_dnode_t *node) {
  if (node->next) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = NULL;
  }
}

static inline void xitk_dlist_add_head (xitk_dlist_t *list, xitk_dnode_t *node) {
  node->next = list->head.next;
  list->head.next->prev = node;
  list->head.next = node;
  node->prev = &list->head;
}

static inline void xitk_dlist_add_tail (xitk_dlist_t *list, xitk_dnode_t *node) {
#ifdef XITK_DEBUG
  if (node->next)
    printf ("xitk_dlist_add_tail: node %p already added (next = %p, prev = %p).\n",
        (void *)node, (void *)node->next, (void *)node->prev);
#endif
  node->prev = list->tail.prev;
  list->tail.prev->next = node;
  list->tail.prev = node;
  node->next = &list->tail;
}

static inline void xitk_dnode_insert_after (xitk_dnode_t *here, xitk_dnode_t *node) {
  xitk_dnode_t *next;
#ifdef XITK_DEBUG
  if (node->next)
    printf ("xitk_dnode_insert_after: node %p already added (next = %p, prev = %p).\n",
        (void *)node, (void *)node->next, (void *)node->prev);
  if (!here->next) {
    printf ("xitk_dnode_insert: node %p not in a list.\n", (void *)here);
    return;
  }
#endif
  next = here->next;
  node->next = next;
  node->prev = here;
  here->next = node;
  next->prev = node;
}

static inline int xitk_dlist_clear (xitk_dlist_t *list) {
  int n = 0;
  xitk_dnode_t *node = list->head.next;
  list->head.next = &list->tail;
  list->head.prev = NULL;
  list->tail.next = NULL;
  list->tail.prev = &list->head;
  while (1) {
    xitk_dnode_t *next = node->next;
    if (!next)
      break;
    node->next = node->prev = NULL;
    node = next;
    n++;
  }
  return n;
}

#if 0 /* yet unused */

static inline int xitk_dlist_is_empty (xitk_dlist_t *list) {
  return list->tail.prev == &list->head;
}

static inline xitk_dnode_t *xitk_dlist_remove_head (xitk_dlist_t *list) {
  xitk_dnode_t *node = list->head.next;
  if (!node->next)
    return NULL;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->next = node->prev = NULL;
  return node;
}

static inline xitk_dnode_t *xitk_dlist_remove_tail (xitk_dlist_t *list) {
  xitk_dnode_t *node = list->tail.prev;
  if (!node->prev)
    return NULL;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->next = node->prev = NULL;
  return node;
}

#endif

#endif
