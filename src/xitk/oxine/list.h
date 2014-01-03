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


#ifndef HAVE_LIST_H
#define HAVE_LIST_H

#include <inttypes.h>

typedef struct list_s list_t;

/*
 * create a new, empty list
 */

#ifdef DEBUG
list_t *_list_new_tagged(char*, int) ;
#define list_new() (_list_new_tagged(__FILE__, __LINE__))
#define xine_list_new() (_list_new_tagged(__FILE__, __LINE__))
#else
list_t *list_new (void) ;
#define xine_list_new list_new
#endif

/*
 * dispose a list (and only the list, contents have to be managed separately)
 */

void list_free (list_t *l) ;

/*
 * set cursor to the first element in list, return contents of first entry
 */

void *list_first_content (list_t *l) ;

/*
 * set cursor to the next element, returns its contents
 */

void *list_next_content (list_t *l) ;

/*
 * returns contents of the current element
 */

void *list_current_content (list_t *l) ;

/*
 * returns 1, if list is empty
 */

int list_is_empty (list_t *l) ;

/*
 * set cursor to the last element, returns its contents
 */

void *list_last_content (list_t *l) ;

/*
 * set cursor to the previous element, return its contents
 */

void *list_prev_content (list_t *l) ;

/*
 * sort element into list by priority. list_first_content returns then
 * the element with the highest priority.
 */

void list_append_priority_content (list_t *l, void *content, int priority) ;

/*
 * append element at the end of the list
 */

#ifdef DEBUG
void _list_append_content (list_t *l, void *content, char*, int) ;
#define list_append_content(list,cont) (_list_append_content(list, cont, __FILE__, __LINE__))
#define xine_list_append_content(list,cont) (_list_append_content(list, cont, __FILE__, __LINE__))
#else
void list_append_content (list_t *l, void *content) ;
#define xine_list_append_content list_append_content
#endif

/*
 * insert content just before cursor position
 */

void list_insert_content (list_t *l, void *content) ;

/*
 * delete current element (its contents have to be freen seperately!)
 */

void list_delete_current (list_t *l) ;

/*
 * compatibility macros
 */

#define xine_list_t list_t
#define xine_list_free list_free
#define xine_list_first_content list_first_content
#define xine_list_next_content list_next_content
#define xine_list_current_content list_current_content
#define xine_list_is_empty list_is_empty
#define xine_list_last_content list_last_content
#define xine_list_prev_content list_prev_content
#define xine_list_append_priority_content list_append_priority_content
#define xine_list_insert_content list_insert_content
#define xine_list_delete_current list_delete_current

/*
 * yet another list implementation
 * thanks to glib project.
 */

typedef struct _g_list_t g_list_t;

struct _g_list_t
{
  void* data;
  g_list_t *next;
  g_list_t *prev;
};

typedef int(*list_compare)(const void*, const void*);
typedef int(*list_compare_data)(const void*, const void*, void*);
typedef int(*list_func)(void*,void*);

/* Doubly linked lists
 */
//void   g_list_push_allocator (GAllocator *allocator);
//void   g_list_pop_allocator (void);
g_list_t* g_list_new (void);
void   g_list_free (g_list_t *list);
void   g_list_free_1 (g_list_t *list);
g_list_t* g_list_append (g_list_t *list, void *data);
g_list_t* g_list_prepend (g_list_t *list, void *data);
g_list_t* g_list_insert (g_list_t *list, void *data, int position);
g_list_t* g_list_insert_sorted (g_list_t *list, void* data, list_compare func);
g_list_t* g_list_insert_before (g_list_t *list, g_list_t *sibling, void* data);
g_list_t* g_list_concat (g_list_t *list1, g_list_t *list2);
g_list_t* g_list_remove (g_list_t *list, const void *data);
g_list_t* g_list_remove_all (g_list_t *list, const void *data);
g_list_t* g_list_remove_link (g_list_t *list, g_list_t *llink);
g_list_t* g_list_delete_link (g_list_t *list, g_list_t *link_);
g_list_t* g_list_reverse (g_list_t *list);
g_list_t* g_list_copy (g_list_t *list);
g_list_t* g_list_nth (g_list_t *list, unsigned int n);
g_list_t* g_list_nth_prev (g_list_t *list, unsigned int n);
g_list_t* g_list_find (g_list_t *list, const void *data);
g_list_t* g_list_find_custom (g_list_t *list, const void *data, list_compare func);
int   g_list_position (g_list_t *list, g_list_t *llink);
int   g_list_index (g_list_t *list, const void * data);
g_list_t* g_list_last (g_list_t *list);
g_list_t* g_list_first (g_list_t *list);
unsigned int  g_list_length (g_list_t *list);
void   g_list_foreach (g_list_t *list, list_func func, void* user_data);
g_list_t* g_list_sort (g_list_t *list, list_compare compare_func);
g_list_t* g_list_sort_with_data (g_list_t *list, list_compare compare_func, void* user_data);
void* g_list_nth_data (g_list_t *list, unsigned int n);

#define g_list_previous(list)	((list) ? (((g_list_t *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((g_list_t *)(list))->next) : NULL)

#endif
