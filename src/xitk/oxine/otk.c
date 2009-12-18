/*
 * Copyright (C) 2002-2003 Stefan Holst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
 *
 * otk - the osd toolkit
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "oxine.h"
#include "odk.h"
#include "otk.h"
#include "list.h"
#include "utils.h"

/*
#define LOG
*/

#define OTK_WIDGET_WINDOW    (1)
#define OTK_WIDGET_BUTTON    (2)
#define OTK_WIDGET_LABEL     (3)
#define OTK_WIDGET_LIST      (4)
#define OTK_WIDGET_LISTENTRY (5)
#define OTK_WIDGET_SLIDER    (6)
#define OTK_WIDGET_SELECTOR  (7)
#define OTK_WIDGET_LAYOUT    (8)
#define OTK_WIDGET_SCROLLBAR (9)

#define OTK_BUTTON_TEXT   (1)
#define OTK_BUTTON_PIXMAP (2)

typedef struct otk_button_s otk_button_t;
typedef struct otk_label_s otk_label_t;
typedef struct otk_window_s otk_window_t;
typedef struct otk_list_s otk_list_t;
typedef struct otk_listentry_s otk_listentry_t;
typedef struct otk_slider_s otk_slider_t;
typedef struct otk_scrollbar_s otk_scrollbar_t;
typedef struct otk_selector_s otk_selector_t;
typedef struct otk_layout_s otk_layout_t;

struct otk_widget_s {

  int            widget_type;
  
  int             x, y, w, h;
  int             focus;
  int             selectable;
  int             needupdate;
  int             needcalc; /* used by layout container */
  int             major; /* if parent otk_window_t should draw/destroy this widget set this to 0 */

  otk_t          *otk;
  odk_t          *odk;

  otk_widget_t   *win;

  void (*draw) (otk_widget_t *this);
  void (*destroy) (otk_widget_t *this);
  void (*select_cb) (otk_widget_t *this);

};

struct otk_layout_s {
  otk_widget_t widget;
  g_list_t    *subs;
  int rows, columns;
};

struct otk_slider_s {
  otk_widget_t widget;

  int  old_pos;
  otk_slider_cb_t get_value;
};

struct otk_scrollbar_s {
  otk_widget_t widget;

  int  pos_start;
  int  pos_end;

  otk_button_cb_t  cb_up;
  otk_button_cb_t  cb_down;
  void            *cb_data;
};

struct otk_button_s {

  otk_widget_t     widget;

  char            *text;
  uint8_t         *pixmap;
  otk_button_cb_t  cb;
  otk_button_uc_t  uc;
  void            *cb_data;
  void            *uc_data;
  int              type;
};

struct otk_selector_s {

  otk_widget_t       widget;

  g_list_t          *items;
  otk_selector_cb_t  cb;
  void              *cb_data;
  int                pos;
};

struct otk_listentry_s {

  otk_widget_t     widget;

  char            *text;
  otk_list_t      *list;

  int              pos;
  int              number;
  int              visible;
  int              isfirst, islast;
  
  void            *data;
};

struct otk_list_s {

  otk_widget_t     widget;

  g_list_t        *entries;   /* of otk_listentry_t */
  int              position;

  int              entry_height;
  int              entries_visible;
  int              num_entries;
  int              last_selected;

  int              font_size;
  char            *font;

  otk_list_cb_t    cb;
  void            *cb_data;

  otk_widget_t    *scrollbar;
};

struct otk_label_s {

  otk_widget_t     widget;

  char            *text;
  int              alignment;
  int              font_size;
};

struct otk_window_s {

  otk_widget_t     widget;

  int              fill_color;
  g_list_t        *subs;
};

struct otk_s {

  odk_t        *odk;

  g_list_t     *windows;

  otk_widget_t *focus_ptr;

  int           showing;

  int           textcolor_win;
  int           textcolor_but;
  int           col_sl1, col_sl2, col_sl1f, col_sl2f;

  int           button_font_size;
  char         *button_font;

  char         *label_font;
  int           label_color;
  int           label_font_size;
  char         *title_font;
  int           title_font_size;

  int           update_job;

  int (*event_handler)(void *data, oxine_event_t *ev);
  void *event_handler_data;


  pthread_mutex_t draw_mutex;
};


/*
 * private utilities
 */

/* truncate the text to a fixed width */
static void check_text_width(odk_t *odk, char *text, int width) {
  int textwidth, num;
  odk_get_text_size(odk, text, &textwidth, &num);

  if (textwidth > width) {
    int i=strlen(text)-4;

    if (i<0) return;
 
    text[i]=text[i+1]=text[i+2]='.';text[i+3]=0;

    odk_get_text_size(odk, text, &textwidth, &num);
    while ((textwidth > width) && (i>0)) {
      i--;
      text[i]='.';
      text[i+3]=0;
      odk_get_text_size(odk, text, &textwidth, &num);
    }
  }
}

static int is_correct_widget(otk_widget_t *widget, int expected) {
 
  if (!widget) {
#ifdef LOG
    printf("otk: got widget NULL\n");
#endif
    return 0;
  }
  if (widget->widget_type != expected) {
#ifdef LOG
    printf("otk: bad widget type %u; expecting %u\n", widget->widget_type, expected);
#endif
    return 0;
  }
  return 1;
}

static void remove_widget_from_win(otk_widget_t *widget) {

  otk_window_t *win = (otk_window_t *) widget->win;
  otk_layout_t *lay = (otk_layout_t *) widget->win;

  if(is_correct_widget(widget->win, OTK_WIDGET_WINDOW)) {

    win->subs = g_list_remove(win->subs, widget);

  }else if (is_correct_widget(widget->win, OTK_WIDGET_LAYOUT)) {
    otk_window_t *parent = (otk_window_t *) lay->widget.win;
    
    lay->subs = g_list_remove(lay->subs, widget);
    parent->subs = g_list_remove(parent->subs, widget);
  }
}

/*
 * callback utilities
 */

static otk_widget_t *find_widget_xy (otk_t *otk, int x, int y) {

  g_list_t *cur, *cur2;

  cur = g_list_first (otk->windows);

  while (cur && cur->data) {

    otk_window_t *win = cur->data;

    cur2 = g_list_first (win->subs);
    while (cur2 && cur2->data) {
    
      otk_widget_t *widget = cur2->data;

      if ( (widget->x<=x) && (widget->y <= y)
	   && ((widget->x + widget->w) >= x)
	   && ((widget->y + widget->h) >= y)
	   && (widget->selectable) ) 
	return widget;

      cur2 = g_list_next (cur2);
    }

    cur = g_list_next (cur);
  }
  return NULL;
}

#define ABS(x) ((x)<0?-(x):(x))

static int get_distance(otk_widget_t *base, otk_widget_t *target) {

  int x1 = (base->x+base->w/2)/10;
  int y1 = (base->y+base->h/2)/10;
  int x2 = (target->x+target->w/2)/10;
  int y2 = (target->y+target->h/2)/10;
  int dist = (int) sqrt (pow((x1-x2),2) + pow((y1-y2),2));
  
#ifdef LOG
  printf("distance: returning: %i\n ", dist);
#endif

  return dist;
}

static double get_vertical_angle(otk_widget_t *base, otk_widget_t *target) {

  int x1 = base->x+base->w/2;
  int y1 = base->y+base->h/2;
  int x2 = target->x+target->w/2;
  int y2 = target->y+target->h/2;
  int x  = ABS(x1-x2);
  int y  = ABS(y1-y2);
  double a;

  if (x == 0)
    a = 0;
  else
    a = atan((double)y/(double)x)+1;

#ifdef LOG
  printf("vangle: returning: %f\n ", a);
#endif

  return a;
}

static double get_horizontal_angle(otk_widget_t *base, otk_widget_t *target) {

  int x1 = base->x+base->w/2;
  int y1 = base->y+base->h/2;
  int x2 = target->x+target->w/2;
  int y2 = target->y+target->h/2;
  int x  = ABS(x1-x2);
  int y  = ABS(y1-y2);
  double a;

  if (y == 0)
    a = 0;
  else
    a = atan((double)x/(double)y)+1;

#ifdef LOG
  printf("hangle: returning: %f\n ", a);
#endif

  return a;
}

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

static otk_widget_t *find_neighbour (otk_t *otk, int direction) {
  
  otk_widget_t *neighbour = NULL;
  int neighbour_ratio = 0;
  int x = otk->focus_ptr->x+otk->focus_ptr->w/2;
  int y = otk->focus_ptr->y+otk->focus_ptr->h/2;
  g_list_t *cur, *cur2;

  cur = g_list_first (otk->windows);

  while (cur && cur->data) {

    otk_window_t *win = cur->data;

    cur2 = g_list_first (win->subs);
    
    while (cur2 && cur2->data) {
      otk_widget_t *widget = cur2->data;

      if ( (widget != otk->focus_ptr) && (widget->selectable) ) {
	
        int ratio = 0;
	int nx = widget->x+widget->w/2;
	int ny = widget->y+widget->h/2;

	switch(direction) {
	  case UP:
	    if ((y-ny)<0) break;
	    ratio = get_distance(otk->focus_ptr, widget) 
	      * get_horizontal_angle(otk->focus_ptr, widget);
	    break;
	  case DOWN:
	    if ((ny-y)<0) break;
	    ratio = get_distance(otk->focus_ptr, widget) 
	      * get_horizontal_angle(otk->focus_ptr, widget);
	    break;
	  case LEFT:
	    if ((x-nx)<0) break;
	    ratio = get_distance(otk->focus_ptr, widget) 
	      * get_vertical_angle(otk->focus_ptr, widget);
	    break;
          case RIGHT:
	    if ((nx-x)<0) break;
	    ratio = get_distance(otk->focus_ptr, widget) 
	      * get_vertical_angle(otk->focus_ptr, widget);
	    break;
	}
#ifdef LOG
	{
	  otk_button_t *but = (otk_button_t*)widget;
	  printf("%s %i\n", but->text, ratio);
	}
#endif
	if (ratio>0)
	  if((ratio<neighbour_ratio) || !neighbour_ratio ) {
	    neighbour_ratio = ratio;
	    neighbour = widget;
	} 
      }
      cur2 = g_list_next (cur2);
    }

    cur = g_list_next (cur);
  }
#ifdef LOG
  printf("\n");
#endif
  return neighbour;
}

/*
 * global callbacks
 */

static void motion_handler (otk_t *otk, oxine_event_t *ev) {

  otk_widget_t *b;

  b = find_widget_xy (otk, ev->x, ev->y);
  
  if (!b)
    return;

  if (b->focus)
    return;

  otk_set_focus(b);
  
  otk_draw_all (otk);
}

static void list_pgdown (otk_list_t *list);
static void list_pgup (otk_list_t *list);

static void button_handler (otk_t *otk, oxine_event_t *ev) {

  otk_widget_t *b;

  b = find_widget_xy (otk, ev->x, ev->y);

  if (!b)
    return;

  if(ev->key == OXINE_KEY_NULL) {
    /*printf("event already processed\n");*/
    return;
  }

  switch( ev->key){
  case OXINE_BUTTON1:
    if (b->select_cb)
      b->select_cb (b);
    break;
  case OXINE_BUTTON4:
    if( otk->focus_ptr) {    
      if( otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY){
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;
	list_pgup(entry->list);
	otk_draw_all(otk);
      }
    }
    break;
  case OXINE_BUTTON5:
    if( otk->focus_ptr) {    
      if( otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY){
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;
	list_pgdown(entry->list);
	otk_draw_all(otk);
      }
    }
    break;
  }
}

static void key_handler (otk_t *otk, oxine_event_t *ev) {

  otk_widget_t *new = NULL;
  
  if (otk->event_handler) {
    otk->event_handler(otk->event_handler_data, ev);
  }

  if(ev->key == OXINE_KEY_NULL) {
    /*printf("event already processed\n");*/
    return;
  }

  /* we only handle key events if we have a focus */
  if(!otk->focus_ptr) {    
    return;
  }
  
  switch(ev->key) {
    case OXINE_KEY_UP:
      if (otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY) {
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;

	if(entry->isfirst) {
	  list_pgup(entry->list);
          otk_draw_all(otk);
	}
      }
      if (otk->focus_ptr->widget_type == OTK_WIDGET_SCROLLBAR) {
	otk_scrollbar_t *scrollbar = (otk_scrollbar_t*) otk->focus_ptr;

	if(scrollbar->cb_up) {
	  scrollbar->cb_up(scrollbar->cb_data);
          otk_draw_all(otk);
	}
      } else
        new = find_neighbour(otk, UP);
      break;
    case OXINE_KEY_DOWN:
      if (otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY) {
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;

	if(entry->islast) {
	  list_pgdown(entry->list);
          otk_draw_all(otk);
	} else
          new = find_neighbour(otk, DOWN);
      }
      if (otk->focus_ptr->widget_type == OTK_WIDGET_SCROLLBAR) {
	otk_scrollbar_t *scrollbar = (otk_scrollbar_t*) otk->focus_ptr;

	if(scrollbar->cb_down) {
	  scrollbar->cb_down(scrollbar->cb_data);
          otk_draw_all(otk);
	}
      } else
        new = find_neighbour(otk, DOWN);
      break;
    case OXINE_KEY_LEFT:
      new = find_neighbour(otk, LEFT);
      break;
    case OXINE_KEY_RIGHT:
      new = find_neighbour(otk, RIGHT);
      break;
    case OXINE_KEY_SELECT:
      if (otk->focus_ptr->select_cb)
	otk->focus_ptr->select_cb (otk->focus_ptr);
      return;
      break;
    case OXINE_KEY_PRIOR:
    if( otk->focus_ptr) {    
      if( otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY){
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;
	list_pgup(entry->list);
	otk_draw_all(otk);
      }
    }
    break;
    case OXINE_KEY_NEXT:
    if( otk->focus_ptr) {    
      if( otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY){
	otk_listentry_t *entry = (otk_listentry_t*) otk->focus_ptr;
	list_pgdown(entry->list);
	otk_draw_all(otk);
      }
    }
    break;
 
    case OXINE_KEY_1:
      printf("got key 1\n");
      break;
    case OXINE_KEY_2:
      printf("got key 2\n");
      break;
    case OXINE_KEY_3:
      printf("got key 3\n");
      break;      
  }
  if (new && (new != otk->focus_ptr)) {
    otk_set_focus(new);
    otk_draw_all(otk);
  }
}

static int otk_event_handler(void *this, oxine_event_t *ev) {

  otk_t *otk = (otk_t*) this;

  switch (ev->type) {
    case OXINE_EVENT_KEY:
      key_handler(otk, ev);
      return 1;
    case OXINE_EVENT_MOTION:
      motion_handler(otk, ev);
      return 1;
    case OXINE_EVENT_BUTTON:
      button_handler(otk, ev);
      return 1;
    case OXINE_EVENT_FORMAT_CHANGED:
      otk_draw_all(otk);
      return 1;
    default:
      if (otk->event_handler) {
        return otk->event_handler(otk->event_handler_data, ev);
      }
  }

  return 0;
}

int otk_send_event(otk_t *otk, oxine_event_t *ev) {

  return odk_send_event(otk->odk, ev);
}

/*
 * button widget
 */

/* FIXME : free displayed_text somewhere */

static void button_draw(otk_widget_t *this) {

  otk_button_t *button = (otk_button_t*) this;
  char *displayed_text;

  if (!is_correct_widget(this, OTK_WIDGET_BUTTON)) return;

  switch(button->type) {
  case OTK_BUTTON_TEXT:
    odk_set_font(this->odk, this->otk->button_font, this->otk->button_font_size);
    displayed_text = strdup(button->text);
    check_text_width(this->odk, displayed_text, button->widget.w); 

    if (this->focus) {
      odk_draw_rect (this->odk, this->x, this->y, 
		     this->x+this->w, this->y+this->h, 1, this->otk->textcolor_but+1); 

      odk_draw_text (this->odk, this->x+this->w/2, 
		     this->y+this->h/2, 
		     displayed_text, ODK_ALIGN_CENTER | ODK_ALIGN_VCENTER,
		     this->otk->textcolor_but);
    } else

      odk_draw_text (this->odk, this->x+this->w/2, 
		     this->y+this->h/2, 
		     displayed_text, ODK_ALIGN_CENTER | ODK_ALIGN_VCENTER,
		     this->otk->textcolor_win);
    free(displayed_text);
    break;
  case OTK_BUTTON_PIXMAP:
    printf("OTK_BUTTON_PIXMAP..\n");
    
    if (this->focus) {
      odk_draw_rect (this->odk, this->x, this->y, 
		     this->x+this->w, this->y+this->h, 1, this->otk->textcolor_but+1); 


      odk_draw_bitmap(this->odk, button->pixmap, this->x - this->w*2, this->y - this->h*2, 10, 10, NULL);

    } else

      odk_draw_bitmap(this->odk, button->pixmap, this->x - this->w*2, this->y - this->h*2, 10, 10, NULL);

    break;
  default: 
    printf("OTK_BUTTON_TYPE : %d not supported\n", button->type);
    break;
  }
}

static void slider_draw_graphic(otk_widget_t *this) {

  otk_slider_t *slider = (otk_slider_t*) this;
  int value;

  if (!is_correct_widget(this, OTK_WIDGET_SLIDER)) return;

  value = (slider->get_value(this->odk));
  if(value == -1) value = slider->old_pos;
  slider->old_pos = value;

  value = (value*this->w-20)/100;

  if (this->focus) {
    odk_draw_rect (this->odk, this->x, this->y, 
		   this->x+this->w, this->y+this->h, 1, this->otk->textcolor_but+1); 

    odk_draw_rect (this->odk, this->x, this->y+(this->h)/2-5, 
		   this->x+this->w , this->y+(this->h)/2+5 , 1, this->otk->col_sl1f);

    odk_draw_rect (this->odk, this->x+value+5, this->y+(this->h)/2-15, 
		   this->x+value+15 , this->y+(this->h)/2+15 , 1, this->otk->col_sl2f);

  } else {
    odk_draw_rect (this->odk, this->x, this->y, 
		   this->x+this->w, this->y+this->h, 1, this->otk->textcolor_win+1); 

    odk_draw_rect (this->odk, this->x, this->y+(this->h)/2-5, 
		   this->x+this->w , this->y+(this->h)/2+5, 1, this->otk->col_sl1);

    odk_draw_rect (this->odk, this->x+value+5, this->y+(this->h)/2-15, 
		   this->x+value+15 , this->y+(this->h)/2+15 , 1, this->otk->col_sl2);
  }
}

static void slider_destroy(otk_widget_t *this) {
  otk_slider_t *slider = (otk_slider_t*) this;
  if (!is_correct_widget(this, OTK_WIDGET_SLIDER)) return;

  remove_widget_from_win(this);
  if (this->otk->focus_ptr == this) this->otk->focus_ptr = NULL;
  ho_free(slider);
}

static void scrollbar_draw(otk_widget_t *this) {

  otk_scrollbar_t *scrollbar = (otk_scrollbar_t*) this;
  int value1, value2;
  int color[3];

  if (!is_correct_widget(this, OTK_WIDGET_SCROLLBAR)) return;

  value1 = ( scrollbar->pos_start * (this->h-10) ) / 100;
  value2 = ( scrollbar->pos_end * (this->h-10) ) / 100;

  if (this->focus) {
    color[0] = this->otk->textcolor_but+1;
    color[1] = this->otk->col_sl1f;
    color[2] = this->otk->col_sl2f;
  } else {
    color[0] = this->otk->textcolor_win+1;
    color[1] = this->otk->col_sl1;
    color[2] = this->otk->col_sl2;
  }
    
  odk_draw_rect (this->odk, this->x, this->y,
                 this->x+this->w, this->y+this->h, 1, color[0]);

/*
  odk_draw_rect (this->odk, this->x, this->y,
                 this->x+this->w, this->y+this->h, 0, color[1]);
*/

  odk_draw_rect (this->odk, this->x+5, this->y+5+value1,
                 this->x+this->w-5, this->y+5+value2, 1, color[2]);
}

static void scrollbar_destroy(otk_widget_t *this) {
  otk_scrollbar_t *scrollbar = (otk_scrollbar_t*) this;
  if (!is_correct_widget(this, OTK_WIDGET_SCROLLBAR)) return;

  remove_widget_from_win(this);
  if (this->otk->focus_ptr == this) this->otk->focus_ptr = NULL;
  ho_free(scrollbar);
}

static void button_destroy(otk_widget_t *this) {

  otk_button_t *button = (otk_button_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_BUTTON)) return;

  if(button->type == OTK_BUTTON_PIXMAP) 
    free(button->pixmap);

  remove_widget_from_win(this);
  if (this->otk->focus_ptr == this)
    this->otk->focus_ptr = NULL;
  ho_free(button);
}

static void button_selected(otk_widget_t *this) {

  otk_button_t *button = (otk_button_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_BUTTON)) return;

  if (button->cb)
    button->cb(button->cb_data);
}

/*void button_list_scroll_up (otk_list_t *list)*/

#if 0
/* I've added this function because i don't want to break existing code.             *
 * When we'll remage otk and odk it's better to select button type in otk_button_new *
 * and then pass text or pixmap                                                      */
static void otk_button_add_pixmap (otk_button_t *button, uint8_t *pixmap) {

  if (!is_correct_widget((otk_widget_t *)button, OTK_WIDGET_BUTTON)) return ;

  button->type               = OTK_BUTTON_PIXMAP;
  button->pixmap             = pixmap;
  return ;
}
#endif
/* slider widget */

otk_widget_t *otk_slider_grid_new (otk_slider_cb_t cb) {
  otk_slider_t *slider;

  slider = ho_new(otk_slider_t);

  slider->widget.widget_type = OTK_WIDGET_SLIDER;
  slider->widget.selectable  = 1;
  slider->widget.select_cb   = NULL; /*slider_selected;*/
  slider->widget.draw        = slider_draw_graphic;
  slider->widget.destroy     = slider_destroy;
  slider->widget.needupdate  = 0;
  slider->widget.needcalc    = 1;
  slider->widget.major       = 1;
  slider->old_pos            = 0;
  slider->get_value          = cb;

  return (otk_widget_t*) slider;
}

otk_widget_t *otk_button_grid_new (char *text,
			      otk_button_cb_t cb,
			      void *cb_data) {
  otk_button_t *button;

  button = ho_new(otk_button_t);

  button->widget.widget_type = OTK_WIDGET_BUTTON;
  button->text               = text;
  button->type               = OTK_BUTTON_TEXT;
  button->widget.selectable  = 1;
  button->widget.select_cb   = button_selected;
  button->widget.draw        = button_draw;
  button->widget.destroy     = button_destroy;
  button->widget.needupdate  = 0;
  button->widget.needcalc    = 1;
  button->widget.major       = 1;
  button->cb                 = cb;
  button->cb_data            = cb_data;
  button->uc                 = NULL;
 
  return (otk_widget_t*) button;
}

otk_widget_t *otk_button_new (otk_widget_t *win, int x, int y,
			      int w, int h, char *text,
			      otk_button_cb_t cb,
			      void *cb_data) {
  otk_button_t *button;
  otk_window_t *window = (otk_window_t*) win;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  button = ho_new(otk_button_t);

  button->widget.widget_type = OTK_WIDGET_BUTTON;
  button->widget.x           = win->x+x;
  button->widget.y           = win->y+y;
  button->widget.w           = w;
  button->widget.h           = h;
  button->text               = text;
  button->type               = OTK_BUTTON_TEXT;
  button->widget.win         = win;
  button->widget.otk         = win->otk;
  button->widget.odk         = win->otk->odk;
  button->widget.selectable  = 1;
  button->widget.select_cb   = button_selected;
  button->widget.draw        = button_draw;
  button->widget.destroy     = button_destroy;
  button->widget.needupdate  = 0;
  button->widget.major       = 0;
  button->cb                 = cb;
  button->cb_data            = cb_data;
  button->uc                 = NULL;
 
  window->subs = g_list_append (window->subs, button);

  return (otk_widget_t*) button;
}

otk_widget_t *otk_scrollbar_new (otk_widget_t *win, int x, int y,
			        int w, int h,
			        otk_button_cb_t cb_up, otk_button_cb_t cb_down,
			        void *user_data) {
                                
  otk_scrollbar_t *scrollbar;
  otk_window_t *window = (otk_window_t*) win;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  scrollbar = ho_new(otk_scrollbar_t);

  scrollbar->widget.widget_type = OTK_WIDGET_SCROLLBAR;
  scrollbar->widget.x           = win->x+x;
  scrollbar->widget.y           = win->y+y;
  scrollbar->widget.w           = w;
  scrollbar->widget.h           = h;
  scrollbar->widget.win         = win;
  scrollbar->widget.otk         = win->otk;
  scrollbar->widget.odk         = win->otk->odk;
  scrollbar->widget.selectable  = 1;
  scrollbar->widget.select_cb   = NULL;
  scrollbar->widget.draw        = scrollbar_draw;
  scrollbar->widget.destroy     = scrollbar_destroy;
  scrollbar->widget.needupdate  = 0;
  scrollbar->widget.major       = 0;
  scrollbar->cb_up              = cb_up;
  scrollbar->cb_down            = cb_down;
  scrollbar->cb_data            = user_data;
  scrollbar->pos_start          = 0;
  scrollbar->pos_end            = 100;
  
  window->subs = g_list_append (window->subs, scrollbar);

  return (otk_widget_t*) scrollbar;
}

void otk_scrollbar_set(otk_widget_t *this, int pos_start, int pos_end)
{
  otk_scrollbar_t *scrollbar = (otk_scrollbar_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_SCROLLBAR)) return;

  if(pos_start < 0 ) pos_start = 0;
  if(pos_start > 100 ) pos_start = 100;
  scrollbar->pos_start = pos_start;

  if(pos_end < pos_start ) pos_end = pos_start;
  if(pos_end > 100 ) pos_end = 100;
  scrollbar->pos_end = pos_end;
}

void otk_button_uc_set(otk_widget_t *this, otk_button_cb_t uc, void *uc_data) {
  otk_button_t *button = (otk_button_t *)this;
  
  if (!is_correct_widget(this, OTK_WIDGET_BUTTON)) return;

  button->uc = uc;
  button->widget.needupdate = 1;
  button->uc_data = uc_data;
 
}

/*
 * list widget
 */

/* FIXME : free displayed_text somewhere.. */
static void listentry_draw (otk_widget_t *this) {
  otk_listentry_t *listentry = (otk_listentry_t*) this;
  char *displayed_text;

  if (!is_correct_widget(this, OTK_WIDGET_LISTENTRY)) return;
  
#ifdef LOG
  printf("otk: draw listentry\n");
#endif

  if (!listentry->visible) return;
  
  odk_set_font(this->odk, listentry->list->font, listentry->list->font_size);
  displayed_text = strdup(listentry->text);
  check_text_width(listentry->widget.odk, displayed_text, listentry->widget.w); 

  /* printf("%d %d\n", this->x, this->y+this->h/2); */

  if (this->focus) {
    odk_draw_rect (this->odk, this->x, this->y, 
		   this->x+this->w, this->y+this->h, 1, this->otk->textcolor_but+1); 

    odk_draw_text (this->odk, this->x, 
	           this->y+this->h/2, 
		   displayed_text, ODK_ALIGN_LEFT | ODK_ALIGN_VCENTER,
		   this->otk->textcolor_but);
  } else

    odk_draw_text (this->odk, this->x, 
		   this->y+this->h/2, 
		   displayed_text, ODK_ALIGN_LEFT | ODK_ALIGN_VCENTER,
		   this->otk->textcolor_win);
  free(displayed_text);
}

static void listentry_select(otk_widget_t *this) {

  otk_listentry_t *listentry = (otk_listentry_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_LISTENTRY)) return;

  listentry->list->last_selected = listentry->number;

  if (listentry->list->cb)
    listentry->list->cb(listentry->list->cb_data,listentry->data);
}

static void listentry_destroy(otk_widget_t *this) {

  otk_listentry_t *listentry = (otk_listentry_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_LISTENTRY)) return;

  if (this->otk->focus_ptr == this)
    this->otk->focus_ptr = NULL;

  remove_widget_from_win(this);
  ho_free(listentry);
}


static void otk_listentry_set_pos (otk_listentry_t *entry, int pos) {

  if ((pos <= 0) || (pos > entry->list->entries_visible)) {
    entry->visible = 0;
    entry->widget.selectable = 0;
    entry->isfirst = 0;
    entry->islast = 0;
    return;
  }
  entry->pos = pos;
  entry->isfirst = 0;
  entry->islast = 0;
  
  entry->widget.y = entry->list->widget.y + entry->list->entry_height*(pos-1);
  entry->visible = 1;
  entry->widget.selectable = 1;
 
  /* we grab focus if nothing is selected */
  if (pos == 1) {
    entry->isfirst = 1;
    if (!entry->widget.otk->focus_ptr) {
      entry->widget.otk->focus_ptr = (otk_widget_t*)entry;
      entry->widget.focus = 1;
    }
  }
  if (pos == entry->list->entries_visible) entry->islast = 1;
}

static void list_adapt_entries(otk_list_t *list) {
  
  int i=1;
  g_list_t *cur;
  
  cur = g_list_first (list->entries);
  while (cur && cur->data) {
    otk_listentry_t *entry = cur->data;
    
    entry->number = i;
    otk_listentry_set_pos (entry, i-list->position);
    cur = g_list_next (cur);
    i++;
  }

  if(list->num_entries)
    otk_scrollbar_set(list->scrollbar,
                      (list->position) * 100 / list->num_entries,
                      (list->position+list->entries_visible) * 100 / list->num_entries);
}

void otk_add_listentry(otk_widget_t *this, char *text, void *data, int pos) {

  otk_list_t *list = (otk_list_t*) this;
  otk_listentry_t *entry;
  int num;
  otk_window_t *win;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) abort();

#ifdef LOG
  printf("otk: add_listentry %s\n", text);
#endif

  entry = ho_new(otk_listentry_t);

  entry->widget.widget_type = OTK_WIDGET_LISTENTRY;
  entry->widget.w           = list->widget.w - 25;
  entry->widget.h           = list->entry_height;
  entry->widget.x           = list->widget.x;
  entry->widget.win         = list->widget.win;
  entry->widget.otk         = list->widget.otk;
  entry->widget.odk         = list->widget.odk;
  entry->widget.selectable  = 0;
  entry->widget.select_cb   = listentry_select;
  entry->widget.draw        = listentry_draw;
  entry->widget.destroy     = listentry_destroy;
  entry->widget.needupdate  = 0;
  entry->widget.major       = 0;
  entry->list               = list;
  entry->text               = strdup(text);
  entry->data               = data;

    
  odk_set_font(entry->widget.odk, entry->list->font, entry->list->font_size);
  win = (otk_window_t *)list->widget.win;
  win->subs = g_list_append (win->subs, entry);

  /* determine the real position the entry is inserted */

  if (pos >= 0) {
    num = pos;
    if (num > list->num_entries)
      num = list->num_entries;
  } else {
    num = list->num_entries + pos + 1;
    if (num < 0)
      num = 0;
  }

  /* now add new entry to the list */

  list->entries = g_list_insert(list->entries, entry, num);

  /*
  cur = list_first_content(list->entries);
  if (cur) {
    for (i=0; i<num; i++)
      cur = list_next_content(list->entries);
  }
  if (cur)
    list_insert_content(list->entries, entry);
  else
    list_append_content(list->entries, entry);

  */
  list->num_entries++;

  list_adapt_entries(list);
}

void otk_remove_listentries(otk_widget_t *this) {
  
  otk_list_t *list = (otk_list_t*) this;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return;

  cur = g_list_first (list->entries);
  while (cur && cur->data) {
    otk_listentry_t *entry = cur->data;
    
    entry->widget.destroy((otk_widget_t*)entry);
    list->entries = g_list_remove_link(list->entries, cur);
    cur = g_list_first (list->entries);
  }

  list->num_entries = 0;
}

static void list_scroll_down (void *this) {
  otk_list_t *list = (otk_list_t *)this;

  list_pgdown(list);
  otk_draw_all(list->widget.otk);
}

static void list_scroll_up (void *this) {
  otk_list_t *list = (otk_list_t *)this;

  list_pgup(list);
  otk_draw_all(list->widget.otk);
}

static void list_pgdown (otk_list_t *list) {
  
  int newpos;
  g_list_t *cur;

  newpos = list->position + list->entries_visible / 2;

  if ((list->num_entries-newpos) < list->entries_visible)
    newpos = list->num_entries - list->entries_visible;

  if (newpos < 0) newpos = 0;

  list->position = newpos;

  list_adapt_entries(list);
  
  /* set focus to first entry if we lost it */
  if (list->widget.otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY) {
    otk_listentry_t *e = (otk_listentry_t*) list->widget.otk->focus_ptr;
    if ((e->list == list) && (e->visible == 0)) {
      e->widget.focus = 0;
      cur = g_list_first(list->entries);
      e = NULL;
      while (cur && cur->data) {
	e = cur->data;
	if (e->isfirst) break;
        cur = g_list_next(cur);
      }
      if (e) {
	list->widget.otk->focus_ptr = (otk_widget_t*) e;
	e->widget.focus = 1;
      }
    }
  }
}

static void list_pgup (otk_list_t *list) {
  
  int newpos;
  g_list_t *cur;

  newpos = list->position - list->entries_visible / 2;
  
  if (newpos < 0) newpos = 0;

  list->position = newpos;

  list_adapt_entries(list);

  /* set focus to last entry if we lost it */
  if (list->widget.otk->focus_ptr->widget_type == OTK_WIDGET_LISTENTRY) {
    otk_listentry_t *e = (otk_listentry_t*) list->widget.otk->focus_ptr;
    if ((e->list == list) && (e->visible == 0)) {
      e->widget.focus = 0;
      e = NULL;
      cur = g_list_first(list->entries);
      while (cur && cur->data) {
	e = cur->data;
	if (e->islast) break;
        cur = g_list_next(cur);
      }
      if (e) {
	list->widget.otk->focus_ptr = (otk_widget_t*) e;
	e->widget.focus = 1;
      }
    }
  }
}

/*static void list_add_scroolbar(otk_list_t *list) {
}
*/

int otk_list_get_entry_number(otk_widget_t *this) {
  otk_list_t *list = (otk_list_t *) this;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return -1;

  return list->last_selected;
}

void otk_list_set_pos(otk_widget_t *this, int newpos) {
  otk_list_t *list = (otk_list_t *) this;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return;

  if ((list->num_entries-newpos) < list->entries_visible)
    newpos = list->num_entries - list->entries_visible;
  if (newpos < 0) newpos = 0;
  list->position = newpos;

  list_adapt_entries(list);
}

int otk_list_get_pos(otk_widget_t *this) {

  otk_list_t *list = (otk_list_t *) this;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return -1;
  
  return list->position;
}

static void list_draw (otk_widget_t *this) {

  otk_list_t *list = (otk_list_t*) this;
  g_list_t   *cur;
  
  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return;

#ifdef LOG
  printf("otk: draw list\n");
#endif
  
  cur = g_list_first (list->entries);
  while (cur && cur->data) {
    otk_listentry_t *entry = cur->data;
    
    entry->widget.draw((otk_widget_t*)entry);
    cur = g_list_next (cur);
  }
}

static void list_destroy(otk_widget_t *this) {

  otk_list_t *list = (otk_list_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_LIST)) return;

  /* do not destroy listentries here; this is done by window destructor */
  /*otk_remove_listentries(this);*/

  g_list_free(list->entries);
    
  remove_widget_from_win(this);
  ho_free(list);
}

otk_widget_t *otk_list_new (otk_widget_t *win, int x, int y, int w, int h,
			  otk_list_cb_t cb,
			  void *cb_data) {

  otk_list_t *list;
  otk_window_t *window = (otk_window_t*) win;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  list = ho_new(otk_list_t);

  list->widget.widget_type = OTK_WIDGET_LIST;
  list->widget.x           = win->x+x;
  list->widget.y           = win->y+y;
  list->widget.w           = w;
  list->widget.h           = h;
  list->widget.win         = win;
  list->widget.otk         = win->otk;
  list->widget.odk         = win->otk->odk;
  list->widget.selectable  = 0;
  list->widget.draw        = list_draw;
  list->widget.destroy     = list_destroy;
  list->widget.needupdate  = 0;
  list->widget.major       = 0;
  list->position           = 0;
  list->cb                 = cb;
  list->cb_data            = cb_data;
  list->entries            = NULL; //g_list_new ();
  list->font               = strdup("sans");
  list->font_size          = 30;
  list->num_entries        = 0;
  list->entry_height       = 32;
 
  list->entries_visible = list->widget.h / list->entry_height;
  list->widget.h = list->entries_visible * list->entry_height;


  list->scrollbar = otk_scrollbar_new (win, x + list->widget.w - 25, y,
			        27, list->widget.h,
			        list_scroll_up, list_scroll_down,
			        list);

  window->subs = g_list_append(window->subs, list);

  return (otk_widget_t*) list;
}


/*
 * label widget
 */

static void label_draw (otk_widget_t *this) {

  otk_label_t *label = (otk_label_t*) this;
  char *displayed_text;

  if (!is_correct_widget(this, OTK_WIDGET_LABEL)) return;

  displayed_text = strdup(label->text);

  odk_set_font (this->odk, this->otk->label_font,
                (label->font_size) ? label->font_size : this->otk->label_font_size);
  check_text_width(this->odk, displayed_text, this->win->w );
  odk_draw_text (this->odk, this->x, this->y, displayed_text, label->alignment, 
      this->otk->label_color);

  free(displayed_text);
}

static void label_destroy (otk_widget_t *this) {

  otk_label_t *label = (otk_label_t*) this;

  if (!is_correct_widget(this, OTK_WIDGET_LABEL)) return;

  remove_widget_from_win(this);
  ho_free(label->text);
  ho_free(label);
}

otk_widget_t *otk_label_new (otk_widget_t *win, int x, int y, int alignment, char *text) {
 
  otk_label_t *label;
  otk_window_t *window = (otk_window_t*) win;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  label = ho_new(otk_label_t);

  label->widget.widget_type = OTK_WIDGET_LABEL;
  label->widget.x           = win->x+x;
  label->widget.y           = win->y+y;
  label->widget.win         = win;
  label->widget.otk         = win->otk;
  label->widget.odk         = win->otk->odk;
  label->widget.draw        = label_draw;
  label->widget.destroy     = label_destroy;
  label->widget.needupdate  = 0;
  label->widget.major       = 0;
  label->alignment          = alignment;
  label->text               = ho_strdup(text);
  label->font_size          = 0; /* use default */
 
  window->subs = g_list_append (window->subs, label);

  return (otk_widget_t*) label;
}

void otk_label_set_font_size(otk_widget_t *this, int font_size)
{
  otk_label_t *label = (otk_label_t *) this;
  label->font_size = font_size;
}


/*
 * layout widget
 */ 

static void layout_draw (otk_widget_t *this) {

  otk_widget_t *widget;
  otk_layout_t *lay = (otk_layout_t*) this;
  unsigned int sizey, sizex;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_LAYOUT)) return;

  sizey = lay->widget.h / lay->rows;
  sizex = lay->widget.w / lay->columns;
 
  cur = g_list_first (lay->subs);
  while (cur && cur->data) {   
    widget = cur->data;
    if(widget->needcalc) {
      widget->x = lay->widget.x + widget->x * sizex;
      widget->y = lay->widget.y + widget->y * sizey;
      widget->w = sizex * widget->w;
      widget->h = sizey * widget->h;
      widget->needcalc = 0;
    }
    widget->draw(widget);
    cur = g_list_next (cur);
  }
}

static void layout_destroy(otk_widget_t *this) {

  otk_widget_t *widget;
  otk_layout_t *lay = (otk_layout_t*) this;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_LAYOUT)) return;

  cur = g_list_first (lay->subs);
  while (cur && cur->data) {
    widget = cur->data;
    widget->destroy(widget);
    cur = g_list_first (lay->subs);
  }
  g_list_free(lay->subs);

  remove_widget_from_win(this);
  ho_free(lay);
}

void otk_layout_add_widget(otk_widget_t *layout, otk_widget_t *widget, int x, int y, int w, int h) {
  otk_layout_t *lay = (otk_layout_t*) layout;
  otk_window_t *win = (otk_window_t*) layout;

  if(is_correct_widget(layout, OTK_WIDGET_LAYOUT)) {
    otk_window_t *parent = (otk_window_t *) lay->widget.win;
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->win = layout;  
    widget->otk = lay->widget.otk;
    widget->odk = lay->widget.odk;

    lay->subs = g_list_append(lay->subs, widget);
    parent->subs = g_list_append(parent->subs, widget);
  }
  else if(is_correct_widget(layout, OTK_WIDGET_WINDOW)) {
    widget->x = win->widget.x + x;
    widget->y = win->widget.y + y;
    widget->w = w;
    widget->h = h;  
    widget->win = (otk_widget_t*) win;
    widget->otk = win->widget.otk;
    widget->odk = win->widget.odk;

    win->subs = g_list_append(win->subs, widget);
  }
  else return;
}

otk_widget_t *otk_layout_new(otk_widget_t *win, int x, int y, int w, int h, int rows, int columns) {
  otk_layout_t *layout;
  otk_window_t *window = (otk_window_t *) win;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  layout = ho_new(otk_layout_t);

  layout->widget.widget_type = OTK_WIDGET_LAYOUT;
  layout->widget.x           = window->widget.x+x;
  layout->widget.y           = window->widget.y+y;
  layout->widget.w           = w;
  layout->widget.h           = h;
  layout->widget.win         = win;
  layout->widget.otk         = win->otk;
  layout->widget.odk         = win->odk;
  layout->widget.draw        = layout_draw;
  layout->widget.destroy     = layout_destroy;
  layout->widget.selectable  = 0;
  layout->widget.needupdate  = 0;
  layout->widget.major       = 0;
  layout->rows               = rows;
  layout->columns            = columns;
  layout->subs               = NULL; //g_list_new();

  window->subs = g_list_append (window->subs, layout);

  return (otk_widget_t*) layout;
}

/*
 * selector widget
 */

static void selector_draw (otk_widget_t *this) {
  
  otk_selector_t *selector = (otk_selector_t*) this;
  char *text;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_SELECTOR)) return;

  cur = g_list_nth(selector->items, selector->pos-1);
  text = cur->data;

  odk_set_font(this->odk, this->otk->button_font, this->otk->button_font_size);

  if (this->focus) {
    odk_draw_rect (this->odk, this->x, this->y, 
		   this->x+this->w, this->y+this->h, 1, this->otk->textcolor_but+1); 

    odk_draw_text (this->odk, this->x+this->w/2, 
		   this->y+this->h/2, 
		   text, ODK_ALIGN_CENTER | ODK_ALIGN_VCENTER,
		   this->otk->textcolor_but);
  } else

    odk_draw_text (this->odk, this->x+this->w/2, 
		   this->y+this->h/2, 
		   text, ODK_ALIGN_CENTER | ODK_ALIGN_VCENTER,
		   this->otk->textcolor_win);

}

static void selector_destroy (otk_widget_t *this) {

  otk_selector_t *selector = (otk_selector_t*) this;
  char *i;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_SELECTOR)) return;

  remove_widget_from_win(this);
  if (this->otk->focus_ptr == this)
    this->otk->focus_ptr = NULL;

  cur = g_list_first(selector->items);

  while (cur && cur->data) {
    i = cur->data;
    free(i);
    cur = g_list_next(cur);
  }
  g_list_free(selector->items);
  
  ho_free(selector);
}

static void selector_selected (otk_widget_t *this) {
  
  otk_selector_t *selector = (otk_selector_t*) this;
 /* char *i; */

  if (!is_correct_widget(this, OTK_WIDGET_SELECTOR)) return;

  selector->pos++;
  if (selector->pos > g_list_length(selector->items)) selector->pos = 1;

  selector_draw(this);
  odk_show (this->odk);

  if (selector->cb)
    selector->cb(selector->cb_data, selector->pos);

}

otk_widget_t *otk_selector_grid_new (const char *const *items, int num,
			      otk_selector_cb_t cb,
			      void *cb_data) {
  otk_selector_t *selector;
  int i;

  selector = ho_new(otk_selector_t);

  selector->widget.widget_type = OTK_WIDGET_SELECTOR;
  selector->widget.selectable  = 1;
  selector->widget.select_cb   = selector_selected;
  selector->widget.draw        = selector_draw;
  selector->widget.destroy     = selector_destroy;
  selector->widget.needupdate  = 0;
  selector->widget.needcalc    = 1;
  selector->widget.major       = 1;
  selector->cb                 = cb;
  selector->cb_data            = cb_data;
  selector->items              = NULL; //g_list_new();

  for (i=0; i<num; i++)
    selector->items = g_list_append(selector->items, strdup(items[i]));

  selector->pos = 1;
 
  return (otk_widget_t*) selector;
}

otk_widget_t *otk_selector_new(otk_widget_t *win,int x, int y, 
                           int w, int h, const char *const *items, int num,
			   otk_selector_cb_t cb,
			   void *cb_data) {
  otk_selector_t *selector;
  otk_window_t *window = (otk_window_t*) win;
  int i;

  if (!is_correct_widget(win, OTK_WIDGET_WINDOW)) return NULL;

  selector = ho_new(otk_selector_t);

  selector->widget.widget_type = OTK_WIDGET_SELECTOR;
  selector->widget.x           = win->x+x;
  selector->widget.y           = win->y+y;
  selector->widget.w           = w;
  selector->widget.h           = h;
  selector->items              = NULL; //g_list_new();
  selector->widget.win         = win;
  selector->widget.otk         = win->otk;
  selector->widget.odk         = win->otk->odk;
  selector->widget.selectable  = 1;
  selector->widget.select_cb   = selector_selected;
  selector->widget.draw        = selector_draw;
  selector->widget.destroy     = selector_destroy;
  selector->widget.needupdate  = 0;
  selector->widget.major       = 0;
  selector->cb                 = cb;
  selector->cb_data            = cb_data;

  for (i=0; i<num; i++)
    selector->items = g_list_append(selector->items, strdup(items[i]));

  selector->pos = 1;
 
  window->subs = g_list_append (window->subs, selector);

  return (otk_widget_t*) selector;

}

void otk_selector_set(otk_widget_t *this, int pos) {
  
  otk_selector_t *selector = (otk_selector_t*) this;
  
  if (!is_correct_widget(this, OTK_WIDGET_SELECTOR)) return;

  if (pos < 1 ) pos = 1;
  if (pos > g_list_length(selector->items)) pos = g_list_length(selector->items);
  selector->pos = pos;

}
/*
 * window widget
 */

static void window_draw (otk_widget_t *this) {

  otk_window_t *win = (otk_window_t*) this;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_WINDOW)) return;

  odk_draw_rect (this->odk, this->x, this->y, 
		 this->x+this->w, this->y+this->h, 1, 
		 win->fill_color);

  cur = g_list_first (win->subs);
  while (cur && cur->data) {
    otk_widget_t *widget = cur->data;
    
    if(!widget->major) widget->draw(widget);
    cur = g_list_next (cur);
  }
}

static void window_destroy(otk_widget_t *this) {

  otk_window_t *win = (otk_window_t*) this;
  g_list_t *cur;

  if (!is_correct_widget(this, OTK_WIDGET_WINDOW)) return;

  cur = g_list_first (win->subs);
  while (cur && cur->data) {
    otk_widget_t *widget = cur->data;
    if(!widget->major) widget->destroy(widget);
    cur = g_list_first (win->subs);
  }
  g_list_free(win->subs);
    
  win->widget.otk->windows = g_list_remove(win->widget.otk->windows, win);
  ho_free(win);
}

otk_widget_t *otk_window_new (otk_t *otk, const char *title, int x, int y,
			      int w, int h) {

  otk_window_t *window;

  window = ho_new(otk_window_t);

  window->widget.widget_type = OTK_WIDGET_WINDOW;
  window->widget.x           = x;
  window->widget.y           = y;
  window->widget.w           = w;
  window->widget.h           = h;
  window->widget.otk         = otk;
  window->widget.odk         = otk->odk;
  window->widget.draw        = NULL;
  window->widget.draw        = window_draw;
  window->widget.destroy     = window_destroy;
  window->widget.needupdate  = 0;
  window->widget.major       = 0;
  window->fill_color         = otk->textcolor_win+1;

  window->subs = NULL; //g_list_new ();

  otk->windows = g_list_append (otk->windows, window);

  return (otk_widget_t*) window;
}

/*
 * other global functions
 */

void otk_draw_all (otk_t *otk) {

  g_list_t *cur;

  pthread_mutex_lock(&otk->draw_mutex);
  
  odk_clear (otk->odk);
  cur = g_list_first (otk->windows);

  if (!cur) {
    odk_hide (otk->odk);
    pthread_mutex_unlock(&otk->draw_mutex);
    return;
  }

  while (cur && cur->data) {
    otk_window_t *win = cur->data;
    
    win->widget.draw((otk_widget_t*)win);
    cur = g_list_next (cur);
  }

  odk_show (otk->odk);
  pthread_mutex_unlock(&otk->draw_mutex);
}

void otk_destroy_widget (otk_widget_t *widget) {

  widget->destroy(widget);
}

void otk_clear(otk_t *otk) {

  g_list_t *cur;

  if (!otk) return;

  cur = g_list_first (otk->windows);
  while (cur && cur->data) {
    otk_window_t *win = cur->data;
    
    win->widget.destroy((otk_widget_t*)win);
    cur = g_list_first (otk->windows);
  }

  g_list_free(otk->windows);
  otk->windows = NULL; //g_list_new();
  otk_draw_all (otk);
}

void otk_set_focus(otk_widget_t *widget) {

  if (widget->selectable) {
    if(widget->otk) {
      if (widget->otk->focus_ptr)
        widget->otk->focus_ptr->focus = 0;
      widget->otk->focus_ptr = widget;
      widget->otk->focus_ptr->focus = 1;
    }
  }
}

void otk_set_event_handler(otk_t *otk, int (*cb)(void *data, oxine_event_t *ev), void *data) {

  otk->event_handler = cb;
  otk->event_handler_data = data;
}

void otk_set_update(otk_widget_t *this, int update) {

  if (update)
    this->needupdate=1;
  else
    this->needupdate=0;
}


/* FIXME: is this really thread safe?
 * check if all otk updates need lock_job_mutex()
 */
static void otk_update_job(void *data) {
  otk_t *otk = (otk_t *) data;
  int changed = 0;
  g_list_t *cur, *cur2;

  cur = g_list_first (otk->windows);
  while (cur && cur->data) {
    otk_window_t *win = cur->data;
    
    cur2 = g_list_first (win->subs);
    while (cur2 && cur2->data) {
      otk_widget_t *widget = cur2->data;
      
      if(widget->needupdate) {
	changed = 1;
	switch (widget->widget_type) {
	case OTK_WIDGET_SLIDER: 
	  {
	    otk_slider_t *slider = (otk_slider_t *) widget;
	    if(slider->get_value) {
	      slider->old_pos = slider->get_value(widget->odk);
	    }
	    widget->draw(widget);
	    break;
	  }
	case OTK_WIDGET_BUTTON:
	  {
	    otk_button_t *button = (otk_button_t *) widget;
	    if(button->uc) {
	      button->uc(button->uc_data);
	      widget->draw(widget);
	    }
	    break;
	  }
	default:
	  widget->draw(widget);
	}
      }
      cur2 = g_list_next (cur2);
    }
    cur = g_list_next (cur);
  }
  if (changed) { odk_show(otk->odk); }
  otk->update_job = schedule_job(100, otk_update_job, otk);
}

otk_t *otk_init (odk_t *odk) {

  otk_t *otk;
  /* int err; */
  uint32_t c[3];
  uint8_t t[3];

  otk = ho_new_tagged(otk_t,"otk object");

  otk->odk = odk;
  
  otk->windows      = NULL; //g_list_new ();

  odk_set_event_handler(odk, otk_event_handler, otk);

  c[0] = 0xffff00; t[0] = 15;
  c[1] = 0x0080c0; t[1] = 15;
  c[2] = 0x008000; t[2] = 15;
  odk_user_color(odk, "text_foreground", &c[0], &t[0]);
  odk_user_color(odk, "text_window", &c[1], &t[1]);
  odk_user_color(odk, "text_border", &c[2], &t[2]);
  otk->textcolor_win = odk_alloc_text_palette(odk, c[0], t[0], c[1], t[1], c[2], t[2]);

  c[0] = 0xff8080; t[0] = 15;
  c[1] = 0x8080c0; t[1] = 15;
  c[2] = 0x808080; t[2] = 15;
  odk_user_color(odk, "focused_text_foreground", &c[0], &t[0]);
  odk_user_color(odk, "focused_button", &c[1], &t[1]);
  odk_user_color(odk, "focused_text_border", &c[2], &t[2]);
  otk->textcolor_but = odk_alloc_text_palette(odk, c[0], t[0], c[1], t[1], c[2], t[2]);

  c[0] = 0xc08080; t[0] = 15;
  c[1] = 0x0080c0; t[1] = 15;
  c[2] = 0x0080c0; t[2] = 15;
  odk_user_color(odk, "label_foreground", &c[0], &t[0]);
  odk_user_color(odk, "label_window", &c[1], &t[1]);
  odk_user_color(odk, "label_border", &c[2], &t[2]);
  otk->label_color = odk_alloc_text_palette(odk, c[0], t[0], c[1], t[1], c[2], t[2]);

  c[0] = 0x008000; t[0] = 15;
  odk_user_color(odk, "slider", &c[0], &t[0]);
  otk->col_sl1 = odk_get_color(odk, c[0], t[0]);
  c[0] = 0xffff00; t[0] = 15;
  odk_user_color(odk, "slider_knob", &c[0], &t[0]);
  otk->col_sl2 = odk_get_color(odk, c[0], t[0]);
  c[0] = 0x808080; t[0] = 15;
  odk_user_color(odk, "focused_slider", &c[0], &t[0]);
  otk->col_sl1f = odk_get_color(odk, c[0], t[0]);
  c[0] = 0xff8080; t[0] = 15;
  odk_user_color(odk, "focused_slider_knob", &c[0], &t[0]);
  otk->col_sl2f = odk_get_color(odk, c[0], t[0]);
  otk->title_font = strdup("sans");
  otk->title_font_size = 40;
  otk->label_font = otk->title_font;
  otk->label_font_size = 40;
  otk->button_font = strdup("cetus");
  otk->button_font_size = 40;

  pthread_mutex_init(&otk->draw_mutex, NULL);
#ifdef ENABLE_UPDATE_JOB
  otk->update_job = schedule_job(1000, otk_update_job, otk);
#else
  otk->update_job = 0;
#endif
  
  return otk;
}

void otk_free (otk_t *otk) {

  lock_job_mutex();
  if(otk->update_job)
    cancel_job(otk->update_job);
  unlock_job_mutex();
  
  otk_clear(otk);
  
  g_list_free(otk->windows);

  if (otk->title_font) free(otk->title_font);
  if (otk->button_font) free(otk->button_font);

  odk_set_event_handler(otk->odk, NULL, NULL);

  pthread_mutex_destroy(&otk->draw_mutex);
  ho_free (otk);
}
