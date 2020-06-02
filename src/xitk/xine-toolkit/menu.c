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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "_xitk.h"

#include "utils.h"

#undef DEBUG_MENU
#undef DUMP_MENU

static void _menu_create_menu_from_branch(menu_node_t *, xitk_widget_t *, int, int);

static menu_window_t *_menu_new_menu_window(ImlibData *im, xitk_window_t *xwin) {
  menu_window_t *menu_window;
  XSetWindowAttributes menu_attr;
  
  menu_window          = (menu_window_t *) xitk_xmalloc(sizeof(menu_window_t));
  menu_window->display = im->x.disp;
  menu_window->im      = im;
  menu_window->xwin    = xwin;

  menu_window->bevel_plain     = NULL;
  menu_window->bevel_arrow     = NULL;
  menu_window->bevel_unchecked = NULL;
  menu_window->bevel_checked   = NULL;

  xitk_dlist_init (&menu_window->wl.list);

  /* HACK: embedded widget list - make sure it is not accidentally (un)listed. */
  xitk_dnode_init (&menu_window->wl.node);
  menu_window->wl.win  = xitk_window_get_window(xwin);
  menu_window->wl.xitk = gXitk;

  menu_attr.override_redirect = True;

  XLOCK (im->x.x_lock_display, im->x.disp);
  XChangeWindowAttributes(im->x.disp, menu_window->wl.win, CWOverrideRedirect, &menu_attr);
  menu_window->wl.gc   = XCreateGC(im->x.disp, (xitk_window_get_window(xwin)), None, None);
  XUNLOCK (im->x.x_unlock_display, im->x.disp);

  return menu_window;
}

static menu_node_t *_menu_new_node(xitk_widget_t *w) {
  menu_node_t  *node;

  node             = (menu_node_t *) xitk_xmalloc(sizeof(menu_node_t));
  node->prev       = NULL;
  node->menu_entry = NULL;
  node->widget     = w;
  node->branch     = NULL;
  node->next       = NULL;

  return node;
}
static xitk_menu_entry_t *_menu_build_menu_entry(xitk_menu_entry_t *me, char *name) {
  xitk_menu_entry_t  *mentry;
  char                buffer[name ? (strlen(name) + 3) : 1];
  
  if(name) {
    char *s = name;
    char *d = buffer;

    while(*s) {
      
      switch(*s) {
      case '\\':
	*d = *(++s);
	break;
	
      default:
	*d = *s;
	break;
      }

      d++;
      s++;
    }
    *d = '\0';
  }

  mentry            = (xitk_menu_entry_t *) xitk_xmalloc(sizeof(xitk_menu_entry_t));
  mentry->menu      = strdup((name) ? buffer : me->menu);
  mentry->shortcut  = (me->shortcut) ? strdup(me->shortcut) : NULL;
  mentry->type      = (me->type) ? strdup(me->type) : NULL;
  mentry->cb        = me->cb;
  mentry->user_data = me->user_data;
  mentry->user_id   = me->user_id;

  return mentry;
}
static menu_node_t *_menu_add_to_node_branch(xitk_widget_t *w, menu_node_t **mnode, 
					     xitk_menu_entry_t *me, char *name) {
  menu_node_t        *node = _menu_new_node(w);

  node->menu_entry  = _menu_build_menu_entry(me, name);
  node->prev        = (*mnode);
  (*mnode)->branch  = node;
  
  return node;
}
static menu_node_t *_menu_append_to_node(xitk_widget_t *w, 
					 menu_node_t **mnode, xitk_menu_entry_t *me, char *name) {
  menu_node_t *node = _menu_new_node(w);
  
  node->menu_entry = _menu_build_menu_entry(me, name);
  node->prev       = (*mnode);
  
  if((*mnode))
    (*mnode)->next = node;

  return node;
}

static int _menu_is_separator(xitk_menu_entry_t *me) {
  if(me && me->type && ((strncasecmp(me->type, "<separator>", 11) == 0)))
    return 1;
  return 0;
}
static int _menu_is_branch(xitk_menu_entry_t *me) {
  if(me && me->type && ((strncasecmp(me->type, "<branch>", 8) == 0)))
    return 1;
  return 0;
}
static int _menu_is_check(xitk_menu_entry_t *me) {
  if(me && me->type && ((strncasecmp(me->type, "<check", 6) == 0)))
    return 1;
  return 0;
}
static int _menu_is_checked(xitk_menu_entry_t *me) {
  if(me && me->type && ((strncasecmp(me->type, "<checked>", 9) == 0)))
    return 1;
  return 0;
}
static int _menu_is_title(xitk_menu_entry_t *me) {
  if(me && me->type && ((strncasecmp(me->type, "<title>", 7) == 0)))
    return 1;
  return 0;
}
static int _menu_branch_have_check(menu_node_t *branch) {
  while(branch) {
    if(_menu_is_check(branch->menu_entry) || _menu_is_checked(branch->menu_entry))
      return 1;
    
    branch = branch->next;
  }
  return 0;
}
static int _menu_branch_have_branch(menu_node_t *branch) {
  while(branch) {
    if(_menu_is_branch(branch->menu_entry))
      return 1;
    
    branch = branch->next;
  }
  return 0;
}
static int _menu_branch_is_in_curbranch(menu_node_t *branch) {
  menu_private_data_t  *private_data = (menu_private_data_t *) branch->widget->private_data;
  menu_node_t          *me = private_data->curbranch;

  if(!me)
    return 0;

  while(branch != me) {
    if(!me->prev)
      return 0;

    while(me->prev) {
      me = me->prev;
      if(me->prev && me->prev->branch == me)
	break;
    }
  }
  return 1;
}

#ifdef DUMP_MENU
#ifdef	__GNUC__
#define prints(fmt, args...) do { int j; for(j = 0; j < i; j++) printf(" "); printf(fmt, ##args); } while(0)
#else
#define prints(...) do { int j; for(j = 0; j < i; j++) printf(" "); printf(__VA_ARGS__); } while(0)
#endif
static int i = 0;
static void _menu_dump_branch(menu_node_t *branch) {
  
  while(branch) {
    prints("[%s]\n", branch->menu_entry->menu);

    if(branch->branch) {
      i += 3;
      _menu_dump_branch(branch->branch);
    }
    
    branch = branch->next;
  }
  i -= 3;
}
static void _menu_dump(menu_private_data_t *private_data) {
  menu_tree_t *mtree = private_data->mtree;
  menu_node_t *me;
  
  printf("\n");
  me = mtree->first;
  while(me) {
    xitk_menu_entry_t *entry = me->menu_entry;
    menu_node_t *branch = me->branch;
    
    if(_menu_is_separator(entry))
      prints("===================\n");
    else
      prints("[%s]\n", entry->menu);
    
    if(branch) {
      i += 3;
      _menu_dump_branch(branch);
      branch = branch->next;
    }
    
    me = me->next;
  }
}
#endif

static void notify_destroy(xitk_widget_t *w);

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  int retval = 0;

  switch(event->type) {
  case WIDGET_EVENT_PAINT:
    break;
  case WIDGET_EVENT_CLICK:
    retval = 1;
    break;
  case WIDGET_EVENT_FOCUS:
    break;
  case WIDGET_EVENT_INSIDE:
    retval = 1;
    break;
  case WIDGET_EVENT_CHANGE_SKIN:
    break;
  case WIDGET_EVENT_DESTROY:
    notify_destroy(w);
    break;
  case WIDGET_EVENT_GET_SKIN:
    break;
  }

  return retval;
}

static int _menu_count_entry_from_branch(menu_node_t *branch) {
  menu_node_t  *me = branch;
  int           ret = 0;
  
  while(me) {
    if(me->menu_entry)
      ret++;
    me = me->next;
  }

  return ret;
}
static int _menu_count_separator_from_branch(menu_node_t *branch) {
  menu_node_t  *me = branch;
  int           ret = 0;
  
  while(me) {
    if(_menu_is_separator(me->menu_entry))
      ret++;
    me = me->next;
  }

  return ret;
}
static int _menu_is_title_in_branch(menu_node_t *branch) {
  menu_node_t  *me = branch;
  int           ret = 0;
  
  while(me) {
    if(_menu_is_title(me->menu_entry)) {
      ret = 1;
      break;
    }
    me = me->next;
  }
  
  return ret;
}
static menu_node_t *_menu_get_wider_menuitem_node(menu_node_t *branch) {
  menu_node_t  *me = branch;
  menu_node_t  *max = NULL;
  size_t        len = 0;
  
  while(me) {
    if((!_menu_is_separator(me->menu_entry)) && 
       (me->menu_entry->menu && 
	((strlen(me->menu_entry->menu) > len)))) {
      len = strlen(me->menu_entry->menu);
      max = me;
    }
    
    me = me->next;
  }

  return max;
}
static int _menu_branch_have_shortcut(menu_node_t *branch) {
  menu_node_t  *me = branch;
  
  while(me) {
    if((!_menu_is_separator(me->menu_entry)) && me->menu_entry->shortcut)
      return 1;
    me = me->next;
  }
  
  return 0;
}
static menu_node_t *_menu_get_wider_shortcut_node(menu_node_t *branch) {
  menu_node_t  *me = branch;
  menu_node_t  *max = NULL;
  size_t        len = 0;
  
  while(me) {
    if((!_menu_is_separator(me->menu_entry)) && 
       (me->menu_entry->shortcut && 
	((strlen(me->menu_entry->shortcut) > len)))) {
      len = strlen(me->menu_entry->shortcut);
      max = me;
    }
    
    me = me->next;
  }

  return max;
}

static menu_node_t *_menu_get_branch_from_node(menu_node_t *node) {
  menu_node_t          *me = node;
  menu_private_data_t  *private_data = (menu_private_data_t *) me->widget->private_data;

  while(me->prev) {
    if(me->prev->branch == me)
      return me;
    me = me->prev;
  }

  return private_data->mtree->first;
}

static menu_node_t *_menu_get_prev_branch_from_node(menu_node_t *node) {
  menu_node_t          *me;

  me = _menu_get_branch_from_node(node);
  if(me->prev)
    return _menu_get_branch_from_node(me->prev);
  else
    return NULL;
}

static void _menu_destroy_menu_window(menu_window_t **mw) {
  menu_private_data_t *private_data = (menu_private_data_t *)(*mw)->widget->private_data;

  xitk_unregister_event_handler(&(*mw)->key);
  xitk_destroy_widgets(&(*mw)->wl);
  if ((*mw)->bevel_plain)
    xitk_image_free_image (private_data->imlibdata, &(*mw)->bevel_plain);
  if ((*mw)->bevel_arrow)
    xitk_image_free_image (private_data->imlibdata, &(*mw)->bevel_arrow);
  if ((*mw)->bevel_unchecked)
    xitk_image_free_image (private_data->imlibdata, &(*mw)->bevel_unchecked);
  if ((*mw)->bevel_checked)
    xitk_image_free_image (private_data->imlibdata, &(*mw)->bevel_checked);
  xitk_window_destroy_window((*mw)->xwin);
  (*mw)->xwin = NULL;
  /* xitk_dlist_init (&(*mw)->wl.list); */

  XLOCK ((*mw)->im->x.x_lock_display, (*mw)->display);
  XFreeGC((*mw)->display, (*mw)->wl.gc);
  XUNLOCK ((*mw)->im->x.x_unlock_display, (*mw)->display);

  /* deferred free as widget list */
  xitk_dnode_remove (&(*mw)->wl.node);
  XITK_WIDGET_LIST_FREE((xitk_widget_list_t *)(*mw));
  (*mw) = NULL;
}

static void _menu_destroy_subs(menu_private_data_t *private_data, menu_window_t *menu_window) {
  menu_window_t *mw;
  
  mw = (menu_window_t *)private_data->menu_windows.tail.prev;
  if(!mw->wl.node.prev || (mw == menu_window))
    /* Nothing to be done, save useless following efforts */
    return;
  do {
    xitk_dnode_remove (&mw->wl.node);
    _menu_destroy_menu_window(&mw);
    mw = (menu_window_t *)private_data->menu_windows.tail.prev;
  } while (mw->wl.node.prev && (mw != menu_window));
  /* Set focus to parent menu */
  if (xitk_window_is_window_visible(menu_window->xwin)) {
    int    t = 5000;
    Window focused_win;

    do {
      int revert;

      XLOCK (menu_window->im->x.x_lock_display, menu_window->display);
      XSetInputFocus(menu_window->display, (xitk_window_get_window(menu_window->xwin)),
		     RevertToParent, CurrentTime);
      XSync(menu_window->display, False);
      XUNLOCK (menu_window->im->x.x_unlock_display, menu_window->display);

      /* Retry 5/15/30/50/75/105/140ms until the WM was mercyful to give us the focus */
      xitk_usec_sleep (t);
      t += 5000;
      XLOCK (menu_window->im->x.x_lock_display, menu_window->display);
      XGetInputFocus(menu_window->display, &focused_win, &revert);
      XUNLOCK (menu_window->im->x.x_unlock_display, menu_window->display);

    } while ((focused_win != xitk_window_get_window (menu_window->xwin)) && (t <= 140000));
  }
}

static int _menu_show_subs(menu_private_data_t *private_data, menu_window_t *menu_window) {
  menu_window_t *mw;

  mw = (menu_window_t *)private_data->menu_windows.tail.prev;
  if (!mw->wl.node.prev)
    mw = NULL;

  return (mw != menu_window);
}

static void _menu_hide_menu(menu_private_data_t *private_data) {
  menu_window_t *mw;
  
  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  mw = (menu_window_t *)private_data->menu_windows.tail.prev;
  while (mw->wl.node.prev) {
    XUnmapWindow(private_data->imlibdata->x.disp, xitk_window_get_window(mw->xwin));
    XSync(private_data->imlibdata->x.disp, False);
    mw = (menu_window_t *)mw->wl.node.prev;
  }
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
}

static void _menu_destroy_ntree(menu_node_t **mn) {

  if((*mn)->branch)
    _menu_destroy_ntree(&((*mn)->branch));
  
  if((*mn)->next)
    _menu_destroy_ntree(&((*mn)->next));

  if((*mn)->menu_entry) {
    free((*mn)->menu_entry->menu);
    free((*mn)->menu_entry->shortcut);
    free((*mn)->menu_entry->type);
    free((*mn)->menu_entry);
  }

  free((*mn));
  (*mn) = NULL;
}

xitk_widget_t *xitk_menu_get_menu(xitk_widget_t *w) {
  xitk_widget_t   *widget = NULL;
  
  if(w && ((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU)) {
    
    if(w->type & WIDGET_GROUP_WIDGET)
      widget = w;
    else {
      menu_window_t *mw = (menu_window_t *) w->wl;
      widget = mw->widget;
    }
  }
  
  return widget;
}

static void notify_destroy(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
    menu_window_t       *mw;
    
    private_data->curbranch = NULL;
    xitk_unset_current_menu();

    mw = (menu_window_t *)private_data->menu_windows.head.next;
    while (mw->wl.node.next) {
      xitk_dnode_remove (&mw->wl.node);
      _menu_destroy_menu_window(&mw);
      mw = (menu_window_t *)private_data->menu_windows.head.next;
    }
    
    xitk_dlist_clear (&private_data->menu_windows);
    _menu_destroy_ntree(&private_data->mtree->first);

    XITK_FREE(private_data->mtree);

    XITK_FREE(private_data);
  }
}

static void _menu_click_cb(xitk_widget_t *w, void *data) {
  menu_node_t          *me = (menu_node_t *) data;
  xitk_widget_t        *widget = me->widget;
  menu_private_data_t  *private_data = (menu_private_data_t *) widget->private_data;
  
  if(_menu_is_branch(me->menu_entry)) {
    
    if(me->branch) {

      if(!_menu_branch_is_in_curbranch(me->branch)) {
	int   wx = 0, wy = 0, x = 0, y = 0;
	
        xitk_window_get_window_position(me->menu_window->xwin, &wx, &wy, NULL, NULL);
	xitk_get_widget_pos(me->button, &x, &y);
	
	x += (xitk_get_widget_width(me->button)) + wx;
	y += wy;
	
	_menu_destroy_subs(private_data, me->menu_window);
	  
	_menu_create_menu_from_branch(me->branch, me->widget, x, y);
      }
      else {
        XLOCK (me->branch->menu_window->im->x.x_lock_display, private_data->imlibdata->x.disp);
	XRaiseWindow(me->branch->menu_window->display, 
		     xitk_window_get_window(me->branch->menu_window->xwin));
	XSetInputFocus(me->branch->menu_window->display, 
		       (xitk_window_get_window(me->branch->menu_window->xwin)),
		       RevertToParent, CurrentTime);
        XUNLOCK (me->branch->menu_window->im->x.x_unlock_display, private_data->imlibdata->x.disp);
      }
    }
    else {
      if(_menu_show_subs(private_data, me->menu_window)) {
	_menu_destroy_subs(private_data, me->menu_window);
	private_data->curbranch = _menu_get_branch_from_node(me);
      }
    }
  }
  else if(!_menu_is_title(me->menu_entry) && !_menu_is_separator(me->menu_entry)) {
    _menu_hide_menu(private_data);

    if(me->menu_entry->cb)
      me->menu_entry->cb(widget, me->menu_entry, me->menu_entry->user_data);
    
    xitk_destroy_widget(widget);
  }
#ifdef DEBUG_MENU
  if(_menu_is_separator(me->menu_entry))
    printf("SEPARATOR\n");
#endif
}

static void _menu_handle_xevents(XEvent *event, void *data) {
  /*
  menu_window_t *me = (menu_window_t *) data;
  menu_private_data_t  *private_data = (menu_private_data_t *) me->widget->private_data;
   
  switch(event->type) {
  }
  */
}

static void __menu_find_branch_by_name(menu_node_t *mnode, menu_node_t **fnode, char *name) {
  menu_node_t *m = mnode;
  
  if((*fnode))
    return;
  
  if(m->branch) {
    if(!strcmp(m->menu_entry->menu, name)) {
      (*fnode) = m;
      return;
    }
    __menu_find_branch_by_name(m->branch, fnode, name);
  }
  
  if((*fnode))
    return;
  
  if(m->next) {
    if(!strcmp(m->menu_entry->menu, name)) {
      (*fnode) = m;
      return;
    }
    __menu_find_branch_by_name(m->next, fnode, name);
  }
  else {
    if(!strcmp(m->menu_entry->menu, name)) {
      (*fnode) = m;
    }
  }
  
}

static menu_node_t *_menu_find_branch_by_name(menu_node_t *mnode, char *name) {
  menu_node_t *m = NULL;
  
  if (mnode) __menu_find_branch_by_name(mnode, &m, name);

  return m;
}
void xitk_menu_add_entry(xitk_widget_t *w, xitk_menu_entry_t *me) {
  
  if(w && me && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
		 (w->type & WIDGET_GROUP_WIDGET))) {
#ifdef DEBUG_MENU
    printf("======== ENTER =========\n%s\n", me->menu);
#endif
    
    if(me->menu) {
      menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
      char                *o, *c, *new_entry;
      char                 buffer[strlen(me->menu) + 1];
      menu_node_t         *branch = NULL;
      int                  in_trunk = 1;

      strlcpy(buffer, me->menu, sizeof(buffer));
      
      o = c = new_entry = buffer;

      if(!(branch = private_data->mtree->first)) {
#ifdef DEBUG_MENU
	printf("FIRST ENTRY OF MENU\n");
#endif
	private_data->mtree->last = private_data->mtree->cur = private_data->mtree->first = 
	  _menu_append_to_node(w, &(private_data->mtree->first), me, new_entry);
	return;
      }

      while( (c = strchr(c, '/')) && (c && (*(c - 1) != '\\'))/*  && branch */) {
	*c = '\0';
	
	in_trunk = 0;
	
#ifdef DEBUG_MENU
	printf("LOOKING FOR '%s': ", o);
#endif	
	
	/* Try to find branch */
	branch = _menu_find_branch_by_name(branch, o);

#ifdef DEBUG_MENU
	if(branch) {
	  printf(" FOUND %s\n", branch->menu_entry->menu);
	}
	else
	  printf("NOT FOUND\n");
#endif

	c++;
	o = new_entry = c;
      }

      if(branch) {
#ifdef DEBUG_MENU
	printf("ADD NEW ENTRY '%s' ", new_entry);
#endif	
	if(_menu_is_branch(branch->menu_entry)) {
#ifdef DEBUG_MENU
	  printf("[IS BRANCH], ");
#endif
	  if(branch->branch == NULL) {
#ifdef DEBUG_MENU
	    printf(" NEW BRANCH [%s]\n", branch->menu_entry->menu);
#endif
	    _menu_add_to_node_branch(w, &branch, me, new_entry);
	    goto __done;
	  }
	  else {
	    branch = branch->branch;
#ifdef DEBUG_MENU
	    printf(" IN BRANCH [%s] ", branch->menu_entry->menu);
#endif
	  }
	}
	
	while(branch->next)
	  branch = branch->next;
	
#ifdef DEBUG_MENU
	printf("AFTER '%s'\n", branch->menu_entry->menu);
#endif
	branch = _menu_append_to_node(w, &branch, me, new_entry);
	if(in_trunk)
	  private_data->mtree->last = branch;
      }
    __done: ;
#ifdef DEBUG_MENU
      printf("******* BYE ************\n");
#endif
#ifdef DUMP_MENU
      _menu_dump(private_data);
#endif
    }
  }
}

static void _menu_scissor(xitk_widget_t *w,
			  menu_private_data_t *private_data, xitk_menu_entry_t *me) {
  menu_tree_t  *mtree;
  
  mtree        = (menu_tree_t *) xitk_xmalloc(sizeof(menu_tree_t));
  mtree->first = NULL;
  mtree->cur   = NULL;
  mtree->last  = NULL;
  
  private_data->mtree = mtree;  
  
  while(me->menu) {
    xitk_menu_add_entry(w, me);
    me++;
  }
  
}

static void _menu_create_menu_from_branch(menu_node_t *branch, xitk_widget_t *w, int x, int y) {
  xitk_skin_element_info_t    info;
  menu_private_data_t        *private_data;
  int                         bentries, bsep, btitle, rentries;
  menu_node_t                *maxnode, *me;
  int                         maxlen, wwidth, wheight, swidth, sheight, shortcutlen = 0, shortcutpos = 0;
  xitk_font_t                *fs;
  static xitk_window_t       *xwin;
  menu_window_t              *menu_window;
  xitk_labelbutton_widget_t   lb;
  xitk_widget_t              *btn;
  xitk_pixmap_t              *bg = NULL;
  int                         yy = 0, got_title = 0;
  
  private_data = (menu_private_data_t *) w->private_data;
  
  private_data->curbranch = branch;

  XITK_WIDGET_INIT(&lb, private_data->imlibdata);
  
  bentries = _menu_count_entry_from_branch(branch);
  bsep     = _menu_count_separator_from_branch(branch);
  btitle   = _menu_is_title_in_branch(branch);
  rentries = bentries - bsep;
  maxnode  = _menu_get_wider_menuitem_node(branch);
  
  if(_menu_is_title(maxnode->menu_entry))
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, DEFAULT_BOLD_FONT_14);
  else
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, DEFAULT_BOLD_FONT_12);

  xitk_font_set_font(fs, private_data->widget->wl->gc);
  maxlen = xitk_font_get_string_length(fs, maxnode->menu_entry->menu);

  if(xitk_get_menu_shortcuts_enability() && _menu_branch_have_shortcut(branch)) {
    xitk_font_t *short_font;
    short_font = xitk_font_load_font(private_data->imlibdata->x.disp, DEFAULT_FONT_12);
    shortcutlen = xitk_font_get_string_length(short_font, (_menu_get_wider_shortcut_node(branch))->menu_entry->shortcut);
    maxlen += shortcutlen + 15;
    xitk_font_unload_font(short_font);
  }

  xitk_font_unload_font(fs);

  wwidth = maxlen + 40;

  if(_menu_branch_have_check(branch) || _menu_branch_have_branch(branch))
    wwidth += 20;
  wheight = (rentries * 20) + (bsep * 2) + (btitle * 2);

  shortcutpos = (wwidth - shortcutlen) - 15;
  
  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  swidth = DisplayWidth(private_data->imlibdata->x.disp, 
			(DefaultScreen(private_data->imlibdata->x.disp)));
  sheight = DisplayHeight(private_data->imlibdata->x.disp, 
			  (DefaultScreen(private_data->imlibdata->x.disp)));
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
  
  if(branch != private_data->mtree->first) {
    x -= 4; /* Overlap parent menu but leave text and symbols visible */
    y -= 1; /* Top item of submenu in line with parent item */
  }
  else {
    x++; y++; /* Upper left corner 1 pix distance to mouse pointer */
  }

  /* Check if menu fits on screen and adjust position if necessary in a way */
  /* that it doesn't obscure the parent menu or get under the mouse pointer */

  if((x + (wwidth + 2)) > swidth) { /* Exceeds right edge of screen */
    if(branch != private_data->mtree->first) {
      /* Align right edge of submenu to left edge of parent item */
      x -= xitk_get_widget_width(branch->prev->button) + (wwidth + 2) - 4;
    }
    else {
      /* Align right edge of top level menu to right edge of screen */
      x  = swidth - (wwidth + 2);
    }
  }
  if((y + (wheight + 2)) > sheight) { /* Exceeds bottom edge of screen */
    if(branch != private_data->mtree->first) {
      /* Align bottom edge of submenu for bottom item in line with parent item */
      y -= wheight - xitk_get_widget_height(branch->prev->button);
    }
    else {
      /* Align bottom edge of top level menu to requested (i.e. pointer) pos */
      y -= (wheight + 2) + 1;
    }
  }

  xwin = xitk_window_create_simple_window(private_data->imlibdata, 
					  x, y, wwidth + 2, wheight + 2);

  if(bsep || btitle) {
    int width, height;
    
    xitk_window_get_window_size(xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(private_data->imlibdata, width, height);
    XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
    XCopyArea(private_data->imlibdata->x.disp, (xitk_window_get_background(xwin)), bg->pixmap,
    	      bg->gc, 0, 0, width, height, 0, 0);
    XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
  }

  menu_window         = _menu_new_menu_window(private_data->imlibdata, xwin);
  menu_window->widget = w;
  
  menu_window->bevel_plain = xitk_image_create_image (private_data->imlibdata, wwidth * 3, 20);
  if (menu_window->bevel_plain)
    draw_flat_three_state (private_data->imlibdata, menu_window->bevel_plain);

  menu_window->bevel_arrow = xitk_image_create_image (private_data->imlibdata, wwidth * 3, 20);
  if (menu_window->bevel_arrow) {
    draw_flat_three_state (private_data->imlibdata, menu_window->bevel_arrow);
    menu_draw_arrow_branch (private_data->imlibdata, menu_window->bevel_arrow);
  }

  menu_window->bevel_unchecked = xitk_image_create_image (private_data->imlibdata, wwidth * 3, 20);
  if (menu_window->bevel_unchecked) {
    draw_flat_three_state (private_data->imlibdata, menu_window->bevel_unchecked);
    menu_draw_check (private_data->imlibdata, menu_window->bevel_unchecked, 0);
  }

  menu_window->bevel_checked = xitk_image_create_image (private_data->imlibdata, wwidth * 3, 20);
  if (menu_window->bevel_checked) {
    draw_flat_three_state (private_data->imlibdata, menu_window->bevel_checked);
    menu_draw_check (private_data->imlibdata, menu_window->bevel_checked, 1);
  }

  memset (&info, 0, sizeof (info));
  info.x                 = 1;
  info.visibility        = 0;
  info.enability         = 0;
  info.pixmap_name       = NULL;
  info.pixmap_img        = menu_window->bevel_plain;
  info.label_alignment   = ALIGN_LEFT;
  info.label_printable   = 1;
  info.label_staticity   = 0;
  info.label_color       =
  info.label_color_focus = (char *)"Black";
  info.label_color_click = (char *)"White";
  info.label_fontname    = (char *)DEFAULT_BOLD_FONT_12;

  xitk_dlist_add_tail (&private_data->menu_windows, &menu_window->wl.node);

  me = branch;
  yy = 1;
  while(me) {

    me->menu_window      = menu_window;
    me->button           = NULL;
    
    if(_menu_is_title(me->menu_entry)) {

      if(bg && (!got_title)) {
	int           lbear, rbear, width, asc, des;
	unsigned int  cfg, cbg;
	XColor        xcolorf, xcolorb;
  
	fs = xitk_font_load_font(private_data->imlibdata->x.disp, DEFAULT_BOLD_FONT_14);
	xitk_font_set_font(fs, private_data->widget->wl->gc);
	
	xitk_font_string_extent(fs, me->menu_entry->menu, &lbear, &rbear, &width, &asc, &des);

	xcolorb.red = xcolorb.blue = xcolorb.green = 140<<8;
	xcolorf.red = xcolorf.blue = xcolorf.green = 255<<8;

        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	XAllocColor(private_data->imlibdata->x.disp,
		    Imlib_get_colormap(private_data->imlibdata), &xcolorb);
	XAllocColor(private_data->imlibdata->x.disp,
		    Imlib_get_colormap(private_data->imlibdata), &xcolorf);
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

	cfg = xcolorf.pixel;
	cbg = xcolorb.pixel;

        XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
	XSetForeground(private_data->imlibdata->x.disp, private_data->widget->wl->gc, cbg);
	XFillRectangle(private_data->imlibdata->x.disp, bg->pixmap, private_data->widget->wl->gc, 
		       1, 1, wwidth , 20);
	XSetForeground(private_data->imlibdata->x.disp, private_data->widget->wl->gc, cfg);
	xitk_font_draw_string(fs, bg->pixmap, 
			      private_data->widget->wl->gc, 
			      5, 1+ ((20+asc+des)>>1)-des, 
			      me->menu_entry->menu, strlen(me->menu_entry->menu));
        XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);
	
	xitk_font_unload_font(fs);

	yy += 20;
	got_title = 1;

	goto __sep;
      }
    }
    else if(_menu_is_separator(me->menu_entry)) {
    __sep:
      if(bg)
	draw_rectangular_inner_box_light(private_data->imlibdata, bg, 3, yy, wwidth - 6, 1);
      yy += 2;
    }
    else {
      lb.button_type       = CLICK_BUTTON;
      lb.label             = me->menu_entry->menu;
      lb.align             = ALIGN_LEFT;
      lb.callback          = _menu_click_cb;
      lb.state_callback    = NULL;
      lb.userdata          = (void *) me;
      lb.skin_element_name = NULL;

      info.y = yy;
      if (_menu_is_branch (me->menu_entry)) {
        info.pixmap_img = menu_window->bevel_arrow;
      } else if (_menu_is_check (me->menu_entry)) {
        info.pixmap_img = _menu_is_checked (me->menu_entry) ? menu_window->bevel_checked : menu_window->bevel_unchecked;
      } else {
        info.pixmap_img = menu_window->bevel_plain;
      }

      btn = xitk_info_labelbutton_create (&menu_window->wl, &lb, &info);
      xitk_dlist_add_tail (&menu_window->wl.list, &btn->node);
      btn->type |= WIDGET_GROUP | WIDGET_GROUP_MENU;
      me->button = btn;
      
      if(xitk_get_menu_shortcuts_enability()  && me->menu_entry->shortcut)
	xitk_labelbutton_change_shortcut_label(btn, me->menu_entry->shortcut, shortcutpos, DEFAULT_FONT_12);
      
      xitk_labelbutton_set_label_offset(btn, 20);
      xitk_enable_and_show_widget(btn);

      yy += 20;
    }
    
    me = me->next;
  }
  
  if(bg) {
    xitk_window_change_background(xwin, bg->pixmap, bg->width, bg->height);
    xitk_image_destroy_xitk_pixmap(bg);
  }

  xitk_set_layer_above((xitk_window_get_window(xwin)));
  
  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  /* Set transient-for-hint to the immediate predecessor,     */
  /* so window stacking of submenus is kept upon raise/lower. */
  if(branch == private_data->mtree->first)
    XSetTransientForHint(private_data->imlibdata->x.disp,
			 (xitk_window_get_window(xwin)), private_data->parent_wlist->win);
  else
    XSetTransientForHint(private_data->imlibdata->x.disp,
			 (xitk_window_get_window(xwin)), (xitk_window_get_window(branch->prev->menu_window->xwin)));
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

  menu_window->key = xitk_register_event_handler("xitk menu",
						 (xitk_window_get_window(menu_window->xwin)), 
						 _menu_handle_xevents,
						 NULL,
						 NULL,
						 &(menu_window->wl),
						 (void *) menu_window);

  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  XMapRaised(private_data->imlibdata->x.disp, (xitk_window_get_window(xwin)));
  XSync(private_data->imlibdata->x.disp, False);
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

  {
    int t = 5000;
    while (!xitk_window_is_window_visible (xwin) && (t <= 140000)) {
      xitk_usec_sleep (t);
      t += 5000;
    }
  }

  if(!(xitk_get_wm_type() & WM_TYPE_KWIN))
    /* WINDOW_TYPE_MENU seems to be the natural choice. */
    xitk_set_wm_window_type(xitk_window_get_window(xwin), WINDOW_TYPE_MENU);
  else
    /* However, KWin has unacceptable behaviour for WINDOW_TYPE_MENU in  */
    /* our transient-for scheme: The transient-for window must be mapped */
    /* and the transient-for window or another transient window (incl.   */
    /* the menu itself) must have focus, otherwise it unmaps the menu.   */
    /* This causes menus not to be shown under many several conditions.  */
    /* WINDOW_TYPE_DOCK is definitely the right choice for KWin.         */
    xitk_set_wm_window_type(xitk_window_get_window(xwin), WINDOW_TYPE_DOCK);
  
  XLOCK (private_data->imlibdata->x.x_lock_display, private_data->imlibdata->x.disp);
  XSetInputFocus(private_data->imlibdata->x.disp, 
		 (xitk_window_get_window(xwin)), RevertToParent, CurrentTime);
  XUNLOCK (private_data->imlibdata->x.x_unlock_display, private_data->imlibdata->x.disp);

  btn = (xitk_widget_t *)menu_window->wl.list.head.next;
  if (btn) {
    xitk_set_focus_to_widget(btn);
  }
}

void xitk_menu_destroy_sub_branchs(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
    menu_node_t         *me = private_data->mtree->first;

    if (me) {
      if (me->next)
        me = me->next;

      if (me)
        _menu_destroy_subs(private_data, me->menu_window);
    }

    private_data->curbranch = private_data->mtree->first;
  }
}

int xitk_menu_show_sub_branchs(xitk_widget_t *w) {
  int ret = 0;

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
    
    if(private_data->curbranch && (private_data->curbranch != private_data->mtree->first))
      ret = 1;
  }

  return ret;
}

void xitk_menu_destroy_branch(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) && 
	   (w->type & WIDGET_TYPE_LABELBUTTON))) {
    menu_node_t         *me = labelbutton_get_user_data(w);    

    me = _menu_get_prev_branch_from_node(me);
    if(me) {
      menu_private_data_t *private_data = (menu_private_data_t *) me->widget->private_data;

      _menu_destroy_subs(private_data, me->menu_window);
      private_data->curbranch = me;
    }
    else {
      xitk_destroy_widget(xitk_menu_get_menu(w));
    }
  }
}

void menu_auto_pop(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) && 
	   (w->type & WIDGET_TYPE_LABELBUTTON) && (!(w->type & WIDGET_GROUP_WIDGET)))) {
    menu_window_t       *mw = (menu_window_t *) w->wl;
    menu_node_t         *me = labelbutton_get_user_data(w);    
    xitk_widget_t       *widget;
    menu_private_data_t *private_data;
    
    widget = mw->widget;
    private_data = (menu_private_data_t *) widget->private_data;

    if(_menu_is_branch(me->menu_entry))
      xitk_labelbutton_callback_exec(w);
    else {
      if(_menu_show_subs(private_data, me->menu_window)) {
	_menu_destroy_subs(private_data, me->menu_window);
	private_data->curbranch = _menu_get_branch_from_node(me);
      }
    }
  }
}

void xitk_menu_show_menu(xitk_widget_t *w) {

  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;

    w->visible = 1;
    _menu_create_menu_from_branch(private_data->mtree->first, w, private_data->x, private_data->y);
    xitk_set_current_menu(w);
  }
}

xitk_widget_t *xitk_noskin_menu_create(xitk_widget_list_t *wl, 
				       xitk_menu_widget_t *m, int x, int y) {
  xitk_widget_t         *mywidget;
  menu_private_data_t   *private_data;
  
  XITK_CHECK_CONSTITENCY(m);
  
  mywidget                   = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  private_data               = (menu_private_data_t *) xitk_xmalloc(sizeof(menu_private_data_t));
  private_data->imlibdata    = m->imlibdata;
  private_data->parent_wlist = m->parent_wlist;
  private_data->widget       = mywidget;
  xitk_dlist_init (&private_data->menu_windows);
  private_data->x            = x;
  private_data->y            = y;
  private_data->curbranch    = NULL;


  if(!m->menu_tree) {
    printf("Empty menu entries. You will not .\n");
    abort();
  }

  mywidget->private_data                 = private_data;
  mywidget->wl                           = wl;
  mywidget->type                         = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_MENU | WIDGET_FOCUSABLE;
  
  _menu_scissor(mywidget, private_data, m->menu_tree);

#ifdef DEBUG_DUMP
  _menu_dump(private_data);
#endif

  mywidget->enable                       = 1;
  mywidget->running                      = 1;
  mywidget->visible                      = 0;
  mywidget->have_focus                   = FOCUS_RECEIVED;
  mywidget->imlibdata                    = private_data->imlibdata;
  mywidget->x = mywidget->y = mywidget->width = mywidget->height = 0;
  mywidget->event                        = notify_event;
  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;
  return mywidget;
}
