/* 
 * Copyright (C) 2003-2009 the oxine project
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
 * doubly linked lists with builtin iterator,
 * based upon xine_list functions of the xine project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#ifdef DEBUG
#include <string.h>
#endif

#include "list.h"
#include "utils.h"

/*
 * private data structures
 */

typedef struct node_s {

  struct node_s    *next, *prev;
  void             *content;
  int              priority;
  
} node_t;


struct list_s {

  node_t    *first, *last, *cur;

#ifdef DEBUG
#define TAG_SIZE 256
  char tag[TAG_SIZE];
#endif
};


/*
 * create a new, empty list
 */

#ifdef DEBUG
list_t *_list_new_tagged(const char *file, int line) {

  char tag[TAG_SIZE];
  list_t *list;

  snprintf(tag, TAG_SIZE, "list @ %s:%i", file, line);
  list = ho_new_tagged(list_t, tag);
  strcpy(list->tag, tag);

  list->first=NULL;
  list->last =NULL;
  list->cur  =NULL;

  return list;
}
#else
list_t *list_new (void) {
  list_t *list;

  list = ho_new(list_t);

  list->first=NULL;
  list->last =NULL;
  list->cur  =NULL;

  return list;
}
#endif

/*
 * dispose a list (and only the list, contents have to be managed separately)
 */

void list_free(list_t *l) {
  node_t *node;

  if (!l) {
    printf ("list: tried to free empty list\n");
    abort();
  }
 
  if (!l->first) {
  
    ho_free(l);

    return;
  }

  node = l->first;
  
  while(node) {
    node_t *n = node;
    
    node = n->next;
    ho_free(n);
  }
  
  ho_free(l);
}

void *list_first_content (list_t *l) {

  l->cur = l->first;

  if (l->first) 
    return l->first->content;
  else
    return NULL;
}

void *list_next_content (list_t *l) {
  if (l->cur) {

    if (l->cur->next) {
      l->cur = l->cur->next;
      return l->cur->content;
    } 
    else
      return NULL;
    
  } else {
    printf ("list: next_content - passed end of list\n");
    abort ();
  }    
}

void *list_current_content (list_t *l) {
  if (l->cur) {

    return l->cur->content;
    
  } else {
    printf ("list: next_content - passed end of list\n");
    abort ();
  }    
}

int list_is_empty (list_t *l) {

  if (l == NULL){
    printf ("list: list_is_empty : list is NULL\n");
    abort();
  }
  return (l->first != NULL);
}

void *list_last_content (list_t *l) {

  if (l->last) {
    l->cur = l->last;
    return l->last->content;
  } else 
    return NULL;
}

void *list_prev_content (list_t *l) {

  if (l->cur) {
    if (l->cur->prev) {
      l->cur = l->cur->prev;
      return l->cur->content;
    } 
    else
      return NULL;
  } else {
    printf ("list: passed begin of list\n");
    abort ();
  }    
}

void list_append_priority_content (list_t *l, void *content, int priority) {
  node_t *node;
  
#ifdef DEBUG
  char tag[TAG_SIZE + 12];

  snprintf(tag, sizeof(tag), "pri node in %s", l->tag);
  node = ho_new_tagged(node_t, tag);
#else
  node = ho_new(node_t);
#endif
  
  node->content = content;
  node->priority = priority;

  if (l->first) {
    node_t *cur;

    cur = l->first;

    while(1) {
      if( priority >= cur->priority ) {
        node->next = cur;
        node->prev = cur->prev;

        if( node->prev )
          node->prev->next = node;
        else
          l->first = node;
        cur->prev = node;

        l->cur = node;
        break;
      }

      if( !cur->next ) {
        node->next = NULL;
        node->prev = cur;
        cur->next = node;

        l->cur = node;
        l->last = node;
        break;
      }
     
      cur = cur->next;
    }
  } 
  else {
    l->first = l->last = l->cur = node;
    node->prev = node->next = NULL;
  }
}

#ifdef DEBUG
void _list_append_content(list_t *l, void *content, const char *file, int line) {
  node_t *node;
  char tag[TAG_SIZE];

  snprintf(tag, TAG_SIZE, "app node @ %s:%i", file, line);
  node = ho_new_tagged(node_t, tag);

#else
void list_append_content (list_t *l, void *content) {
  node_t *node;
  
  node = ho_new(node_t);
#endif
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

void list_insert_content (list_t *l, void *content) {
  node_t *nodecur, *nodenew, *nodeprev;
  
#ifdef DEBUG
  char tag[TAG_SIZE + 12];

  snprintf(tag, sizeof(tag), "ins node in %s", l->tag);
  nodenew = ho_new_tagged(node_t, tag);
#else
  nodenew = ho_new(node_t);
#endif
  nodenew->content = content;
  nodecur = l->cur;

  if(!nodecur) {
    list_append_content(l, content);
    return;
  }
  
  nodeprev = nodecur->prev;
  nodecur->prev = nodenew;
  nodenew->next = nodecur;
  nodenew->prev = nodeprev;
  if(nodecur != l->first) {
    nodeprev->next = nodenew;
  } else {
    l->first = nodenew;
  }
  l->cur = nodenew;
}

void list_delete_current (list_t *l) {
  node_t *node_cur;

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
    l->last = node_cur->prev;
    l->cur = node_cur->prev;
  }
  ho_free(node_cur);
}

/*
 * glib's implementation of lists
 */

#define _g_list_new g_list_new
g_list_t* g_list_new (void) {
  g_list_t *list;
  
  list = ho_new(g_list_t);
  
  return list;
}

void g_list_free (g_list_t *list) {
  g_list_t *last;
  
  while (list)
  {
    last = list;
    list = list->next;
    ho_free (last);
  }
}

#define _g_list_free_1 g_list_free_1
void g_list_free_1 (g_list_t *list) {
  ho_free (list);
}

g_list_t* g_list_append (g_list_t *list, void* data) {
  g_list_t *new_list;
  g_list_t *last;
  
  new_list = _g_list_new ();
  new_list->data = data;
  
  if (list)
  {
    last = g_list_last (list);
    last->next = new_list;
    new_list->prev = last;

    return list;
  }
  else
    return new_list;
}

g_list_t* g_list_prepend (g_list_t *list, void* data) {
  g_list_t *new_list;
  
  new_list = _g_list_new ();
  new_list->data = data;
  
  if (list)
  {
    if (list->prev)
    {
      list->prev->next = new_list;
      new_list->prev = list->prev;
    }
    list->prev = new_list;
    new_list->next = list;
  }
  
  return new_list;
}

g_list_t* g_list_insert (g_list_t *list, void* data, int position) {
  g_list_t *new_list;
  g_list_t *tmp_list;
  
  if (position < 0)
    return g_list_append (list, data);
  else if (position == 0)
    return g_list_prepend (list, data);
  
  tmp_list = g_list_nth (list, position);
  if (!tmp_list)
    return g_list_append (list, data);
  
  new_list = _g_list_new ();
  new_list->data = data;
  
  if (tmp_list->prev)
  {
    tmp_list->prev->next = new_list;
    new_list->prev = tmp_list->prev;
  }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;
  
  if (tmp_list == list)
    return new_list;
  else
    return list;
}

g_list_t* g_list_insert_before (g_list_t *list, g_list_t *sibling, void* data) {
  if (!list)
  {
    list = g_list_new ();
    list->data = data;
    return list;
  }
  else if (sibling)
  {
    g_list_t *node;

    node = g_list_new ();
    node->data = data;
    if (sibling->prev)
    {
      node->prev = sibling->prev;
      node->prev->next = node;
      node->next = sibling;
      sibling->prev = node;
      return list;
    }
    else
    {
      node->next = sibling;
      sibling->prev = node;
      return node;
    }
  }
  else
  {
    g_list_t *last;

    last = list;
    while (last->next)
      last = last->next;

    last->next = g_list_new ();
    last->next->data = data;
    last->next->prev = last;

    return list;
  }
}

g_list_t * g_list_concat (g_list_t *list1, g_list_t *list2) {
  g_list_t *tmp_list;
  
  if (list2)
  {
    tmp_list = g_list_last (list1);
    if (tmp_list)
      tmp_list->next = list2;
    else
      list1 = list2;
    list2->prev = tmp_list;
  }
  
  return list1;
}

g_list_t* g_list_remove (g_list_t *list, const void *data) {
  g_list_t *tmp;
  
  tmp = list;
  while (tmp)
  {
    if (tmp->data != data)
      tmp = tmp->next;
    else
    {
      if (tmp->prev)
	tmp->prev->next = tmp->next;
      if (tmp->next)
	tmp->next->prev = tmp->prev;
      
      if (list == tmp)
	list = list->next;
      
      _g_list_free_1 (tmp);
      
      break;
    }
  }
  return list;
}

g_list_t* g_list_remove_all (g_list_t *list, const void *data) {
  g_list_t *tmp = list;

  while (tmp)
  {
    if (tmp->data != data)
      tmp = tmp->next;
    else
    {
      g_list_t *next = tmp->next;

      if (tmp->prev)
	tmp->prev->next = next;
      else
	list = next;
      if (next)
	next->prev = tmp->prev;

      _g_list_free_1 (tmp);
      tmp = next;
    }
  }
  return list;
}

static inline g_list_t* _g_list_remove_link (g_list_t *list, g_list_t *link) {
  if (link)
  {
    if (link->prev)
      link->prev->next = link->next;
    if (link->next)
      link->next->prev = link->prev;
    
    if (link == list)
      list = list->next;
    
    link->next = NULL;
    link->prev = NULL;
  }
  
  return list;
}

g_list_t* g_list_remove_link (g_list_t *list, g_list_t *link) {
  return _g_list_remove_link (list, link);
}

g_list_t* g_list_delete_link (g_list_t *list, g_list_t *link) {
  list = _g_list_remove_link (list, link);
  _g_list_free_1 (link);

  return list;
}

g_list_t* g_list_copy (g_list_t *list) {
  g_list_t *new_list = NULL;

  if (list)
  {
    g_list_t *last;

    new_list = _g_list_new ();
    new_list->data = list->data;
    last = new_list;
    list = list->next;
    while (list)
    {
      last->next = _g_list_new ();
      last->next->prev = last;
      last = last->next;
      last->data = list->data;
      list = list->next;
    }
  }

  return new_list;
}

g_list_t* g_list_reverse (g_list_t *list) {
  g_list_t *last;
  
  last = NULL;
  while (list)
    {
      last = list;
      list = last->next;
      last->next = last->prev;
      last->prev = list;
    }
  
  return last;
}

g_list_t* g_list_nth (g_list_t *list, unsigned int  n) {
  while ((n-- > 0) && list)
    list = list->next;
  
  return list;
}

g_list_t* g_list_nth_prev (g_list_t *list, unsigned int n) {
  while ((n-- > 0) && list)
    list = list->prev;
  
  return list;
}

void* g_list_nth_data (g_list_t *list, unsigned int n) {
  while ((n-- > 0) && list)
    list = list->next;
  
  return list ? list->data : NULL;
}

g_list_t* g_list_find (g_list_t *list, const void *data) {
  while (list)
  {
    if (list->data == data)
      break;
    list = list->next;
  }
  
  return list;
}

g_list_t* g_list_find_custom (g_list_t *list, const void *data, list_compare func) {

  while (list)
  {
    if (! func (list->data, data))
      return list;
    list = list->next;
  }

  return NULL;
}


int g_list_position (g_list_t *list, g_list_t *link) {
  int i;

  i = 0;
  while (list)
  {
    if (list == link)
      return i;
    i++;
    list = list->next;
  }

  return -1;
}

int g_list_index (g_list_t *list, const void *data) {
  int i;

  i = 0;
  while (list)
  {
    if (list->data == data)
      return i;
    i++;
    list = list->next;
  }

  return -1;
}

g_list_t* g_list_last (g_list_t *list) {
  if (list)
  {
    while (list->next)
      list = list->next;
  }
  
  return list;
}

g_list_t* g_list_first (g_list_t *list) {
  if (list)
  {
    while (list->prev)
      list = list->prev;
  }
  
  return list;
}

unsigned int g_list_length (g_list_t *list) {
  unsigned int length;
  
  length = 0;
  while (list)
  {
    length++;
    list = list->next;
  }
  
  return length;
}

void g_list_foreach (g_list_t *list, list_func func, void *user_data) {
  while (list)
  {
    g_list_t *next = list->next;
    (*func) (list->data, user_data);
    list = next;
  }
}


g_list_t* g_list_insert_sorted (g_list_t *list, void *data, list_compare func) {
  g_list_t *tmp_list = list;
  g_list_t *new_list;
  int cmp;

  if (!func) return list;
  
  if (!list) 
  {
    new_list = _g_list_new ();
    new_list->data = data;
    return new_list;
  }
  
  cmp = (*func) (data, tmp_list->data);
  
  while ((tmp_list->next) && (cmp > 0))
  {
    tmp_list = tmp_list->next;
    cmp = (*func) (data, tmp_list->data);
  }

  new_list = _g_list_new ();
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
  {
    tmp_list->next = new_list;
    new_list->prev = tmp_list;
    return list;
  }
   
  if (tmp_list->prev)
  {
    tmp_list->prev->next = new_list;
    new_list->prev = tmp_list->prev;
  }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;
 
  if (tmp_list == list)
    return new_list;
  else
    return list;
}

#if 0
static g_list_t *g_list_sort_merge (g_list_t *l1, g_list_t *l2, list_func compare_func, 
    int use_data, void *user_data) {
  
  g_list_t list, *l, *lprev;
  int cmp;

  l = &list; 
  lprev = NULL;

  while (l1 && l2)
  {
    if (use_data)
      cmp = ((list_compare_data) compare_func) (l1->data, l2->data, user_data);
    else
      cmp = ((list_compare) compare_func) (l1->data, l2->data);

    if (cmp <= 0)
    {
      l->next = l1;
      l = l->next;
      l->prev = lprev; 
      lprev = l;
      l1 = l1->next;
    } 
    else 
    {
      l->next = l2;
      l = l->next;
      l->prev = lprev; 
      lprev = l;
      l2 = l2->next;
    }
  }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}
 
static g_list_t* g_list_sort_real (g_list_t *list, list_func compare_func, 
    int use_data, void *user_data) {
  
  g_list_t *l1, *l2;
  
  if (!list) 
    return NULL;
  if (!list->next) 
    return list;
  
  l1 = list; 
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
  {
    if ((l2 = l2->next) == NULL) 
      break;
    l1 = l1->next;
  }
  l2 = l1->next; 
  l1->next = NULL; 

  return g_list_sort_merge (g_list_sort_real (list, compare_func, use_data, user_data),
			    g_list_sort_real (l2, compare_func, use_data, user_data),
			    compare_func,
			    use_data,
			    user_data);
}

g_list_t *g_list_sort (g_list_t *list, list_compare compare_func) {
  return g_list_sort_real (list, (list_func) compare_func, 0, NULL);
}

g_list_t * g_list_sort_with_data (g_list_t *list, list_compare compare_func, void *user_data) {
  return g_list_sort_real (list, (list_func) compare_func, 1, user_data);
}
#endif

#if 0
static g_list_t* g_list_sort2 (g_list_t *list, list_compare compare_func) {
  g_list_t *runs = NULL;
  g_list_t *tmp;

  /* Degenerate case.  */
  if (!list) return NULL;

  /* Assume: list = [12,2,4,11,2,4,6,1,1,12].  */
  for (tmp = list; tmp; )
  {
    g_list_t *tmp2;
    for (tmp2 = tmp;
	 tmp2->next && compare_func (tmp2->data, tmp2->next->data) <= 0;
	 tmp2 = tmp2->next)
      /* Nothing */;
    runs = g_list_append (runs, tmp);
    tmp = tmp2->next;
    tmp2->next = NULL;
  }
  /* Now: runs = [[12],[2,4,11],[2,4,6],[1,1,12]].  */
  
  while (runs->next)
  {
    /* We have more than one run.  Merge pairwise.  */
    g_list_t *dst, *src, *dstprev = NULL;
    dst = src = runs;
    while (src && src->next)
    {
      dst->data = g_list_sort_merge (src->data,
				     src->next->data,
				     (list_func) compare_func,
				     0, NULL);
      dstprev = dst;
      dst = dst->next;
      src = src->next->next;
    }

    /* If number of runs was odd, just keep the last.  */
    if (src)
    {
      dst->data = src->data;
      dstprev = dst;
      dst = dst->next;
    }

    dstprev->next = NULL;
    g_list_free (dst);
  }

  /* After 1st loop: runs = [[2,4,11,12],[1,1,2,4,6,12]].  */
  /* After 2nd loop: runs = [[1,1,2,2,4,4,6,11,12,12]].  */

  list = runs->data;
  g_list_free (runs);
  return list;
}
#endif
