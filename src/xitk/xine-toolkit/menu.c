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

#include "_xitk.h"

#undef DEBUG_MENU

static void _menu_create_menu_from_branch(menu_node_t *, xitk_widget_t *, int, int);

static menu_window_t *_menu_new_menu_window(ImlibData *im, xitk_window_t *xwin) {
  menu_window_t *menu_window;
  
  menu_window          = (menu_window_t *) xitk_xmalloc(sizeof(menu_window_t));
  menu_window->display = im->x.disp;
  menu_window->im      = im;
  menu_window->xwin    = xwin;
  menu_window->wl.l    = xitk_list_new();
  menu_window->wl.win  = xitk_window_get_window(xwin);

  XLOCK(im->x.disp);
  menu_window->wl.gc   = XCreateGC(im->x.disp, (xitk_window_get_window(xwin)), None, None);
  XUNLOCK(im->x.disp);

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
  mentry->type      = (me->type) ? strdup(me->type) : NULL;
  mentry->cb        = me->cb;
  mentry->user_data = me->user_data;

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

#ifdef DEBUG_MENU
static void _menu_dump(menu_private_data_t *private_data) {
  menu_tree_t *mtree = private_data->mtree;
  menu_node_t *me;
  
  me = mtree->first;
  while(me) {
    xitk_menu_entry_t *entry = me->menu_entry;
    menu_node_t *branch = me->branch;

    if(_menu_is_separator(entry))
      printf("===================\n");
    else
      printf("[%s]\n", entry->menu);

    while(branch) {
      xitk_menu_entry_t *sentry = branch->menu_entry;
      menu_node_t *sbranch = branch->branch;

      if(_menu_is_separator(sentry))
	printf("  ===================\n");
      else
	printf("  [%s]\n", sentry->menu);

      while(sbranch) {
	xitk_menu_entry_t *ssentry = sbranch->menu_entry;

	if(_menu_is_separator(ssentry))
	  printf("    ===================\n");
	else
	  printf("    [%s]\n", ssentry->menu);

	sbranch = sbranch->next;
      }
      
      branch = branch->next;
    }
    
    me = me->next;
  }
}
#endif

static void _menu_scissor(xitk_widget_t *w,
			  menu_private_data_t *private_data, xitk_menu_entry_t *me) {
  char         *c;
  int           sub, offset;
  menu_tree_t  *mtree;
  menu_node_t  *main_trunk = NULL, *branch = NULL, *subbranch = NULL;
  
  mtree        = (menu_tree_t *) xitk_xmalloc(sizeof(menu_tree_t));
  mtree->first = NULL;
  mtree->cur   = NULL;
  mtree->last  = NULL;
  
  while(me->menu) {

    sub = offset = 0;
    c = me->menu;
    
    if(_menu_is_branch(me)) {
      int     off = 0, same_branch = 0;
      char   *cbranch = strchr(c, '/');

      if(cbranch && (*(cbranch - 1) == '\\'))
	cbranch = NULL;
      
      if(cbranch) {
	cbranch++;
	off = cbranch - c;

#ifdef DEBUG_MENU
	printf("  /[%s] << SUBBRANCH (add_in_branch, create_new_branch) \n", cbranch);
#endif

        if(branch == NULL)
	  branch = _menu_add_to_node_branch(w, &main_trunk, me, cbranch);
	else
	  branch = _menu_append_to_node(w, &branch, me, cbranch);
	
	me++;
	do {
	  same_branch = 0;
	  
	  if(!strncmp(me->menu + off, cbranch, strlen(cbranch))) {

#ifdef DEBUG_MENU
	    printf("    |\n");
	    if(_menu_is_separator(me)) {
	      printf("    +- ========================== << SUBSEP (add_in_branch)\n");
	    }
	    else {
	      printf("    +- [%s] << SUBENTRY (add_in_branch)\n", 
		     me->menu + off + strlen(cbranch) + 1);
	    }
#endif

	    if(subbranch == NULL)
	      subbranch = _menu_add_to_node_branch(w, &branch, me, (me->menu + off + strlen(cbranch) + 1));
	    else
	      subbranch = _menu_append_to_node(w, &subbranch, me, (me->menu + off + strlen(cbranch) + 1));
	    me++;
	    same_branch = 1;
	  }
	  
	} while(same_branch);

#ifdef DEBUG_MENU
	printf("(back_to_parent_branch)\n");
#endif

	subbranch = NULL;
	
	continue;
      }
      else {

#ifdef DEBUG_MENU
	printf("[%s]> << NEWBRANCH (add_in_trunk, create_new_branch)\n", me->menu);
#endif

	main_trunk = mtree->cur = _menu_append_to_node(w, &main_trunk, me, me->menu);
	branch = subbranch = NULL;
	me++;
	continue;
      }
    }
    
    while( (c = strchr(c, '/')) && (c && (*(c - 1) != '\\')) ) {
      offset = c - me->menu + 1;
      sub++;
      c++;
    }

    if(sub) {
      if(sub > 1) {

#ifdef DEBUG_MENU
	printf("WHAT ?\n");
#endif

      }
      else {

#ifdef DEBUG_MENU
	printf("  /[%s] SUB (add_in_branch)\n", me->menu + offset);
#endif
	if(branch == NULL)
	  branch = _menu_add_to_node_branch(w, &main_trunk, me, (me->menu + offset));
	else
	  branch = _menu_append_to_node(w, &branch, me, (me->menu + offset));
	
      }
    }
    else {

#ifdef DEBUG_MENU
      if(_menu_is_separator(me)) {
	printf("========================= << MAIN (add_in_trunk)\n");
      }
      else {
	printf("(go_to_trunk)\n");
      }
#endif

      if(main_trunk)
	mtree->cur = main_trunk;
      
      branch = subbranch = NULL;

#ifdef DEBUG_MENU
      printf("[%s] << MAIN (add_in_trunk)\n", me->menu);
#endif
      
      main_trunk = mtree->last = mtree->cur = _menu_append_to_node(w, &mtree->cur, me, me->menu);
      
      if(mtree->first == NULL)
      	mtree->first = main_trunk;
    }
    
    me++;
  }
  
  private_data->mtree = mtree;  
}

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
  int           len = 0;
  
  
  while(me) {
    if((!_menu_is_separator(me->menu_entry)) && 
       (me->menu_entry->menu && (strlen(me->menu_entry->menu) > len))) {
      len = strlen(me->menu_entry->menu);
      max = me;
    }
    
    me = me->next;
  }
  
  return max;
}

static void _menu_destroy_menu_window(menu_window_t **mw) {

  xitk_unregister_event_handler(&(*mw)->key);
  xitk_destroy_widgets(&(*mw)->wl);
  xitk_window_destroy_window((*mw)->im, (*mw)->xwin);
  (*mw)->xwin = NULL;
  xitk_list_free((*mw)->wl.l);

  XLOCK((*mw)->display);
  XFreeGC((*mw)->display, (*mw)->wl.gc);
  XUNLOCK((*mw)->display);

  free((*mw));
  (*mw) = NULL;
}

static void _menu_destroy_subs(menu_private_data_t *private_data, menu_window_t *menu_window) {
  menu_window_t *mw;
  
  mw = (menu_window_t *) xitk_list_last_content(private_data->menu_windows);
  while(mw && (mw != menu_window)) {
    xitk_list_delete_current(private_data->menu_windows);
    _menu_destroy_menu_window(&mw);
    mw = (menu_window_t *) xitk_list_last_content(private_data->menu_windows);
  }
}
static void _menu_hide_menu(menu_private_data_t *private_data) {
  menu_window_t *mw;
  
  mw = (menu_window_t *) xitk_list_last_content(private_data->menu_windows);
  XLOCK(private_data->imlibdata->x.disp);
  while(mw) {
    XUnmapWindow(private_data->imlibdata->x.disp, xitk_window_get_window(mw->xwin));
    XSync(private_data->imlibdata->x.disp, False);
    mw = (menu_window_t *) xitk_list_prev_content(private_data->menu_windows);
  }
  XUNLOCK(private_data->imlibdata->x.disp);
}

static void _menu_destroy_ntree(menu_node_t **mn) {

  if((*mn)->branch)
    _menu_destroy_ntree(&((*mn)->branch));
  
  if((*mn)->next)
    _menu_destroy_ntree(&((*mn)->next));

  if((*mn)->menu_entry) {
    if((*mn)->menu_entry->menu)
      free((*mn)->menu_entry->menu);
    if((*mn)->menu_entry->type)
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

void xitk_menu_destroy(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
    menu_window_t       *mw;
    
    private_data->curbranch = NULL;
    xitk_unset_current_menu();

    mw = (menu_window_t *) xitk_list_first_content(private_data->menu_windows);
    while(mw) {
      xitk_list_delete_current(private_data->menu_windows);
      _menu_destroy_menu_window(&mw);
      mw = (menu_window_t *) xitk_list_first_content(private_data->menu_windows);
    }
    
    xitk_list_free(private_data->menu_windows);
    _menu_destroy_ntree(&private_data->mtree->first);
  }
}

static void _menu_click_cb(xitk_widget_t *w, void *data) {
  menu_node_t          *me = (menu_node_t *) data;
  xitk_widget_t        *widget = me->widget;
  menu_private_data_t  *private_data = (menu_private_data_t *) widget->private_data;
  
  if(_menu_is_branch(me->menu_entry)) {

    if(me->branch) {
      if(private_data->curbranch && (me->branch != private_data->curbranch)) {
	int   wx, wy, x, y;
	
	xitk_window_get_window_position(private_data->imlibdata, 
					me->menu_window->xwin, &wx, &wy, NULL, NULL);
	xitk_get_widget_pos(w, &x, &y);
	
	x += (xitk_get_widget_width(w)) + wx;
	x -= 10;
	y += wy;
	
	_menu_destroy_subs(private_data, me->menu_window);
	_menu_create_menu_from_branch(me->branch, widget, x, y);
      }
      else {
	
	if(private_data->curbranch) {
	  XLOCK(private_data->curbranch->menu_window->display);
	  XRaiseWindow(private_data->curbranch->menu_window->display, 
		       xitk_window_get_window(private_data->curbranch->menu_window->xwin));
	  XSetInputFocus(private_data->curbranch->menu_window->display, 
			 (xitk_window_get_window(private_data->curbranch->menu_window->xwin)),
			 RevertToParent, CurrentTime);
	  XUNLOCK(private_data->curbranch->menu_window->display);
	}
      }
    }
    else {
      menu_node_t *mnode = (private_data->curbranch && private_data->curbranch->prev) ? 
	private_data->curbranch->prev : private_data->mtree->first;
      
      _menu_destroy_subs(private_data, me->menu_window);
      private_data->curbranch = mnode;
    }

  }
#ifdef DEBUG_MENU
  else if(_menu_is_separator(me->menu_entry))
    printf("SEPARATOR\n");
#endif
  else if(_menu_is_check(me->menu_entry)) {
    _menu_hide_menu(private_data);

    if(me->menu_entry->cb)
      me->menu_entry->cb(widget, me->menu_entry, me->menu_entry->user_data);
    
    xitk_menu_destroy(widget);
  }
  else {
    if(!_menu_is_title(me->menu_entry)) {
      _menu_hide_menu(private_data);

      if(me->menu_entry->cb)
	me->menu_entry->cb(widget, me->menu_entry, me->menu_entry->user_data);
      
      xitk_menu_destroy(widget);
    }
  }
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
  
  __menu_find_branch_by_name(mnode, &m, name);

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

      sprintf(buffer, "%s", me->menu);
      
      o = c = new_entry = buffer;

      branch = private_data->mtree->first;
      while( (c = strchr(c, '/')) && (c && (*(c - 1) != '\\')) && branch) {
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
    }      
  }
}

static void _menu_create_menu_from_branch(menu_node_t *branch, xitk_widget_t *w, int x, int y) {
  menu_private_data_t        *private_data;
  int                         bentries, bsep, btitle, rentries;
  menu_node_t                *maxnode, *me;
  int                         maxlen, wwidth, wheight, swidth, sheight;
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
  xitk_font_unload_font(fs);
  
  wwidth = maxlen + 40;
  wheight = (rentries * 20) + (bsep * 2) + (btitle * 2);
  
  XLOCK(private_data->imlibdata->x.disp);
  swidth = DisplayWidth(private_data->imlibdata->x.disp, 
			(DefaultScreen(private_data->imlibdata->x.disp)));
  sheight = DisplayHeight(private_data->imlibdata->x.disp, 
			  (DefaultScreen(private_data->imlibdata->x.disp)));
  XUNLOCK(private_data->imlibdata->x.disp);
  
  if((x + (wwidth + 2)) > swidth)
    x = swidth - (wwidth + 2);
  if((y + (wheight + 2)) > sheight)
    y = sheight - (wheight + 2);
  
  xwin = xitk_window_create_simple_window(private_data->imlibdata, 
					  x, y, wwidth + 2, wheight + 2);
  
  if(bsep || btitle) {
    int width, height;
    
    xitk_window_get_window_size(xwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(private_data->imlibdata, width, height);
    XLOCK(private_data->imlibdata->x.disp);
    XCopyArea(private_data->imlibdata->x.disp, (xitk_window_get_background(xwin)), bg->pixmap,
    	      bg->gc, 0, 0, width, height, 0, 0);
    XUNLOCK(private_data->imlibdata->x.disp);
  }
  
  menu_window         = _menu_new_menu_window(private_data->imlibdata, xwin);
  menu_window->widget = w;
  
  xitk_list_append_content(private_data->menu_windows, menu_window);

  me = branch;
  yy = 1;
  while(me) {

    me->menu_window      = menu_window;

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

	XLOCK(private_data->imlibdata->x.disp);
	XAllocColor(private_data->imlibdata->x.disp,
		    Imlib_get_colormap(private_data->imlibdata), &xcolorb);
	XAllocColor(private_data->imlibdata->x.disp,
		    Imlib_get_colormap(private_data->imlibdata), &xcolorf);
	XUNLOCK(private_data->imlibdata->x.disp);

	cfg = xcolorf.pixel;
	cbg = xcolorb.pixel;

	XLOCK(private_data->imlibdata->x.disp);
	XSetForeground(private_data->imlibdata->x.disp, private_data->widget->wl->gc, cbg);
	XFillRectangle(private_data->imlibdata->x.disp, bg->pixmap, private_data->widget->wl->gc, 
		       1, 1, wwidth , 20);
	XSetForeground(private_data->imlibdata->x.disp, private_data->widget->wl->gc, cfg);
	XDrawString(private_data->imlibdata->x.disp, 
		    bg->pixmap, private_data->widget->wl->gc, 5, 1+ ((20+asc+des)>>1)-des, 
		    me->menu_entry->menu, strlen(me->menu_entry->menu));
	XUNLOCK(private_data->imlibdata->x.disp);
	
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
      xitk_image_t  *wimage;

      lb.button_type       = CLICK_BUTTON;
      lb.label             = me->menu_entry->menu;
      lb.align             = ALIGN_LEFT;
      lb.callback          = _menu_click_cb;
      lb.state_callback    = NULL;
      lb.userdata          = (void *) me;
      lb.skin_element_name = NULL;
      xitk_list_append_content(menu_window->wl.l,
			       (btn = xitk_noskin_labelbutton_create(&menu_window->wl, &lb,
								     1, yy,
								     wwidth, 20,
								     "Black", "Black", "White", 
								     DEFAULT_BOLD_FONT_12)));
      btn->type |= WIDGET_GROUP | WIDGET_GROUP_MENU;

      wimage = xitk_get_widget_foreground_skin(btn);
      if(wimage) {
	draw_flat_three_state(private_data->imlibdata, wimage);				
	if(_menu_is_branch(me->menu_entry))
	  menu_draw_arrow_branch(private_data->imlibdata, wimage);
	else if(_menu_is_check(me->menu_entry)) {
	  menu_draw_check(private_data->imlibdata, wimage, (_menu_is_checked(me->menu_entry)));
	}
      }
      
      xitk_labelbutton_set_label_offset(btn, 20);

      yy += 20;
    }
    
    me = me->next;
  }
  
  if(bg) {
    xitk_window_change_background(private_data->imlibdata, xwin, 
				  bg->pixmap, bg->width, bg->height);
    xitk_image_destroy_xitk_pixmap(bg);
  }

  xitk_set_layer_above((xitk_window_get_window(xwin)));
  
  XLOCK(private_data->imlibdata->x.disp);
  XSetTransientForHint(private_data->imlibdata->x.disp, 
		       (xitk_window_get_window(xwin)), private_data->parent_wlist->win);
  XUNLOCK(private_data->imlibdata->x.disp);


  menu_window->key = xitk_register_event_handler("xitk menu",
						 (xitk_window_get_window(menu_window->xwin)), 
						 _menu_handle_xevents,
						 NULL,
						 NULL,
						 &(menu_window->wl),
						 (void *) menu_window);

  XLOCK(private_data->imlibdata->x.disp);
  XMapRaised(private_data->imlibdata->x.disp, (xitk_window_get_window(xwin)));
  XSync(private_data->imlibdata->x.disp, False);
  XUNLOCK(private_data->imlibdata->x.disp);
  
  while(!xitk_is_window_visible(private_data->imlibdata->x.disp, (xitk_window_get_window(xwin))))
    xitk_usec_sleep(5000);
  
  XLOCK(private_data->imlibdata->x.disp);
  XSetInputFocus(private_data->imlibdata->x.disp, 
		 (xitk_window_get_window(xwin)), RevertToParent, CurrentTime);
  XUNLOCK(private_data->imlibdata->x.disp);
  
  xitk_set_focus_to_widget((xitk_widget_t *) (xitk_list_first_content(menu_window->wl.l)));
}

void xitk_menu_destroy_sub_branchs(xitk_widget_t *w) {
  
  if(w && (((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU) &&
	   (w->type & WIDGET_GROUP_WIDGET))) {
    menu_private_data_t *private_data = (menu_private_data_t *) w->private_data;
    menu_node_t         *me = private_data->mtree->first;

    if(me && me->next)
      me = me->next;
    
    _menu_destroy_subs(private_data, me->menu_window);
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
      if(xitk_menu_show_sub_branchs(widget)) {
	menu_node_t *mnode;
	
	mnode = (private_data->curbranch && private_data->curbranch->prev) ? 
	  private_data->curbranch->prev : private_data->mtree->first;
	
	_menu_destroy_subs(private_data, me->menu_window);
	private_data->curbranch = mnode;
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
  private_data->menu_windows = xitk_list_new();
  private_data->x            = x;
  private_data->y            = y;


  if(!m->menu_tree) {
    printf("Empty menu entries. You will not .\n");
    abort();
  }

  _menu_scissor(mywidget, private_data, m->menu_tree);
  
#ifdef DEBUG_MENU
  _menu_dump(private_data);
#endif

  mywidget->private_data                 = private_data;
  
  mywidget->wl                           = wl;
  
  mywidget->enable                       = 1;
  mywidget->running                      = 1;
  mywidget->visible                      = 0;
  mywidget->have_focus                   = FOCUS_RECEIVED;
  mywidget->imlibdata                    = private_data->imlibdata;
  mywidget->x = mywidget->y = mywidget->width = mywidget->height = 0;
  mywidget->type                         = WIDGET_GROUP | WIDGET_GROUP_WIDGET | WIDGET_GROUP_MENU | WIDGET_FOCUSABLE;
  mywidget->event                        = notify_event;
  mywidget->tips_timeout                 = 0;
  mywidget->tips_string                  = NULL;

  return mywidget;
}
