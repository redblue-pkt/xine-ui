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
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "Imlib-light/Imlib.h"
#include "widget.h"
#include "image.h"
#include "inputtext.h"
#include "font.h"
#include "widget_types.h"

#include "_xitk.h"

#define NORMAL                  1
#define FOCUS                   2

/*
 *
 */
static void notify_destroy(xitk_widget_t *w, void *data) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) w->private_data;

  if(w->widget_type & WIDGET_TYPE_INPUTTEXT) {
    XITK_FREE(private_data->skin_element_name);
    xitk_image_free_image(private_data->imlibdata, &private_data->skin);
    XITK_FREE(private_data->text);
    XITK_FREE(private_data->normal_color);
    XITK_FREE(private_data->focused_color);
    XITK_FREE(private_data->fontname);
    XITK_FREE(private_data);
  }
}

/*
 *
 */
static xitk_image_t *get_skin(xitk_widget_t *w, int sk) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) w->private_data;
  
  if(w->widget_type & WIDGET_TYPE_INPUTTEXT) {
    if(sk == BACKGROUND_SKIN && private_data->skin) {
      return private_data->skin;
    }
  }

  return NULL;
}

/*
 *
 */
static int notify_inside(xitk_widget_t *it, int x, int y) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if(it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    if(it->visible) {
      xitk_image_t *skin = private_data->skin;
      
      return xitk_is_cursor_out_mask(private_data->imlibdata->x.disp, it, skin->mask, x, y);
    }
    else
      return 0;
  }

  return 1;
}

/*
 * Extract modifier keys.
 */
int xitk_get_key_modifier(XEvent *xev, int *modifier) {
  
  *modifier = MODIFIER_NOMOD;

  if(!xev)
    return 0;

  /*
  printf("%s(): ", __FUNCTION__);
  if(xev->xbutton.state & ShiftMask)
    printf("Shift ");
  if(xev->xbutton.state & LockMask)
    printf("CapsLock ");
  if(xev->xbutton.state & ControlMask)
    printf("Control ");
  if(xev->xbutton.state & Mod1Mask)
    printf("Meta(mod1) ");
  if(xev->xbutton.state & Mod2Mask)
    printf("NumLock(mod2) ");
  if(xev->xbutton.state & Mod3Mask)
    printf("Mod3 ");
  if(xev->xbutton.state & Mod4Mask)
    printf("Mod4 ");
  if(xev->xbutton.state & Mod5Mask)
    printf("Mod5 ");
  printf(".\n");
  */

  if(xev->xbutton.state & ShiftMask)
    *modifier |= MODIFIER_SHIFT;

  if(xev->xbutton.state & LockMask)
    *modifier |= MODIFIER_LOCK;

  if(xev->xbutton.state & ControlMask)
    *modifier |= MODIFIER_CTRL;

  if(xev->xbutton.state & Mod1Mask)
    *modifier |= MODIFIER_META;

  if(xev->xbutton.state & Mod2Mask)
    *modifier |= MODIFIER_NUML;

  if(xev->xbutton.state & Mod3Mask)
    *modifier |= MODIFIER_MOD3;

  if(xev->xbutton.state & Mod4Mask)
    *modifier |= MODIFIER_MOD4;

  if(xev->xbutton.state & Mod5Mask)
    *modifier |= MODIFIER_MOD5;

  return (*modifier != MODIFIER_NOMOD);
}

/*
 * Recalculate display offsets.
 */
static void inputtext_recalc_offsets(xitk_widget_list_t *wl, 
				     inputtext_private_data_t *private_data) {
  char         *label = private_data->text;
  char         *p = private_data->text;
  xitk_font_t  *fs = NULL;
  int           width;
  int           i = 0;

  if(private_data->cursor_pos >= 0) {
    
    if(private_data->fontname)
      fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);

    if(fs == NULL) 
      fs = xitk_font_load_font(private_data->imlibdata->x.disp, xitk_get_system_font());

    if(fs == NULL)
      XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);

    xitk_font_set_font(fs, wl->gc);

    private_data->disp_offset = private_data->pos_in_pos = 0;
    
    while(i < private_data->cursor_pos) {
      
      p = label;
      p += private_data->disp_offset;
      
      width = xitk_font_get_string_length(fs, p);

      if(((i == (strlen(label))) && (width > private_data->max_visible)) 
	 || ((xitk_font_get_text_width(fs, p, 
				       (i - private_data->disp_offset))) 
	     >= private_data->max_visible)) {
	
	private_data->pos[++private_data->pos_in_pos] = i - 1;
	private_data->disp_offset = private_data->pos[private_data->pos_in_pos];
      }

      i++;
    }

    xitk_font_unload_font(fs);
  }
}

/*
 * Return a pixmap with text drawed.
 */
static Pixmap create_labelofinputtext(xitk_widget_t *it, 
				      Window win, GC gc, Pixmap pix, 
				      int xsize, int ysize, 
				      char *label, int state) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char               *plabel = label;
  char               *p = label;
  xitk_font_t        *fs = NULL;
  int                 lbear, rbear, width, asc, des;
  int                 yoff = 0, DefaultColor = -1;
  unsigned int        fg;
  XColor              color;
  xitk_color_names_t  *gColor = NULL;

  color.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */

  if(private_data->fontname)
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);

  if(fs == NULL) 
    fs = xitk_font_load_font(private_data->imlibdata->x.disp, xitk_get_system_font());

  if(fs == NULL)
    XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);

  xitk_font_set_font(fs, gc);
  xitk_font_string_extent(fs, label, &lbear, &rbear, &width, &asc, &des);

  /*  Some colors configurations */
  switch(state) {
  case NORMAL:
    if(!strcasecmp(private_data->normal_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = xitk_get_color_name(private_data->normal_color);
    }
    break;

  case FOCUS:
    if(!strcasecmp(private_data->focused_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      gColor = xitk_get_color_name(private_data->focused_color);
    }
    break;
    
  }
  
  if(gColor == NULL || DefaultColor != -1) {
    color.red = color.blue = color.green = DefaultColor<<8;
  }
  else {
    color.red = gColor->red<<8; 
    color.blue = gColor->blue<<8;
    color.green = gColor->green<<8;
  }

  XLOCK(private_data->imlibdata->x.disp);

  XAllocColor(private_data->imlibdata->x.disp,
	      Imlib_get_colormap(private_data->imlibdata), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->imlibdata->x.disp, gc, fg);

  XUNLOCK(private_data->imlibdata->x.disp);

  if(private_data->cursor_pos >= 0) {

    if(private_data->disp_offset)
      p += private_data->disp_offset;
    
    width = xitk_font_get_string_length(fs, p);
    
    if(private_data->disp_offset > private_data->cursor_pos && private_data->pos_in_pos) {
      
      private_data->disp_offset = private_data->pos[--private_data->pos_in_pos];
      
      if(private_data->disp_offset < 0) {
	private_data->pos[private_data->pos_in_pos] = private_data->disp_offset = 0;
      }
      
    }
    else if(((private_data->cursor_pos == (strlen(label)))
	     && (width > private_data->max_visible))
	    || ((xitk_font_get_text_width(fs, p, 
					  (private_data->cursor_pos - private_data->disp_offset)))
		>= private_data->max_visible)) {
      
      private_data->pos[++private_data->pos_in_pos] = private_data->cursor_pos - 1;
      private_data->disp_offset = private_data->pos[private_data->pos_in_pos];
    }
  }

  if(private_data->disp_offset)
    plabel += private_data->disp_offset;

  /*  Put text in the right place */
  XLOCK(private_data->imlibdata->x.disp);

  XDrawString(private_data->imlibdata->x.disp, pix, gc, 
	      2, ((ysize+asc+des+yoff)>>1)-des, 
	      plabel, strlen(plabel));

  width = xitk_font_get_text_width(fs, plabel,
				   (private_data->cursor_pos - private_data->disp_offset));
  
  /* Draw cursor pointer */
  if(private_data->cursor_pos >= 0) {
    
    XDrawLine(private_data->imlibdata->x.disp, pix, gc,
	      width + 1, 2, width + 3, 2);

    XDrawLine(private_data->imlibdata->x.disp, pix, gc, 
	      width + 2, 2, width + 2, ysize - 3);

    XDrawLine(private_data->imlibdata->x.disp, pix, gc, 
	      width + 1, ysize - 3, width + 3, ysize - 3);

  }
  
  XUNLOCK(private_data->imlibdata->x.disp);

  xitk_font_unload_font(fs);

  if(gColor) {
    XITK_FREE(gColor->colorname);
    XITK_FREE(gColor);
  }

  return pix;
}

/*
 * Paint the input text box.
 */
static void paint_inputtext(xitk_widget_t *it, Window win, GC gc) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int                 button_width, state = 0;
  xitk_image_t        *skin;
  Pixmap              btn, bgtmp;
  GC                  lgc;
  XWindowAttributes   attr;

  if ((it->widget_type & WIDGET_TYPE_INPUTTEXT) && it->visible) {
        
    XGetWindowAttributes(private_data->imlibdata->x.disp, win, &attr);
    
    skin = private_data->skin;
    
    lgc = XCreateGC(private_data->imlibdata->x.disp, win, None, None);
    XCopyGC(private_data->imlibdata->x.disp, gc, (1 << GCLastBit) - 1, lgc);

    if (skin->mask) {
      XSetClipOrigin(private_data->imlibdata->x.disp, lgc, it->x, it->y);
      XSetClipMask(private_data->imlibdata->x.disp, lgc, skin->mask);
    }

    button_width = skin->width/2;
    
    bgtmp = XCreatePixmap(private_data->imlibdata->x.disp, skin->image,
			  button_width, skin->height, attr.depth);
    
    XLOCK(private_data->imlibdata->x.disp);
    
    if(private_data->have_focus) {
      state = FOCUS;
      XCopyArea (private_data->imlibdata->x.disp, skin->image,
		 bgtmp, gc, button_width, 0,
		 button_width, skin->height, 0, 0);
    }
    else {
      state = NORMAL;
      XCopyArea (private_data->imlibdata->x.disp, skin->image,
		 bgtmp, gc, 0, 0,
		 button_width, skin->height, 0, 0);
    }
    

    btn = create_labelofinputtext(it, win, gc, bgtmp,
				  button_width, skin->height, 
				  private_data->text, state);
    
    XCopyArea (private_data->imlibdata->x.disp, btn, win, lgc, 0, 0,
	       button_width, skin->height, it->x, it->y);


    XFreePixmap(private_data->imlibdata->x.disp, bgtmp);

    XFreeGC(private_data->imlibdata->x.disp, lgc);

    XUNLOCK(private_data->imlibdata->x.disp);
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "paint input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif
}

/*
 * Handle click events.
 */
static int notify_click_inputtext(xitk_widget_list_t *wl, xitk_widget_t *it, 
				   int bUp, int x, int y) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int pos;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {

    if(it->have_focus == FOCUS_LOST) {
      it->have_focus = private_data->have_focus = FOCUS_RECEIVED;
    }

    pos = x - it->x;
    
    {
      char        *p = private_data->text;
      xitk_font_t *fs = NULL;
      int          width = 0, i = 0;
      
      p += private_data->disp_offset;

      if(private_data->fontname)
	fs = xitk_font_load_font(private_data->imlibdata->x.disp, private_data->fontname);
      
      if(fs == NULL) 
	fs = xitk_font_load_font(private_data->imlibdata->x.disp, xitk_get_system_font());
      
      if(fs == NULL)
	XITK_DIE("%s()@%d: xitk_font_load_font() failed. Exiting\n", __FUNCTION__, __LINE__);
      
      xitk_font_set_font(fs, wl->gc);
      
      while(width < pos) {
	
	width = xitk_font_get_text_width(fs, p, i);
	i++;
      }
      xitk_font_unload_font(fs);
      pos = (i-2) + private_data->disp_offset;
    }
    
    if(private_data->text) {
      if(pos > strlen(private_data->text))
	pos = strlen(private_data->text);
    }
    else
      pos = 0;

    private_data->cursor_pos = (pos < 0) ? 0 : pos;

    paint_inputtext(it, wl->win, wl->gc);

  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify focus input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif

  return 1;
}

/*
 * Handle motion on input text box.
 */
static int notify_focus_inputtext(xitk_widget_list_t *wl, 
				   xitk_widget_t *it, int bEntered) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    private_data->have_focus = bEntered;
    if(bEntered == FOCUS_LOST) {
      private_data->cursor_pos = 0;
    }
    paint_inputtext(it, wl->win, wl->gc);

  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "notify focus input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif

  return 1;
}

/*
 *
 */
static void notify_change_skin(xitk_widget_list_t *wl, 
			       xitk_widget_t *it, xitk_skin_config_t *skonfig) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    if(private_data->skin_element_name) {
      XITK_FREE(private_data->fontname);
      private_data->fontname      = xitk_skin_get_label_fontname(skonfig, private_data->skin_element_name);
      
      XITK_FREE_XITK_IMAGE(private_data->imlibdata->x.disp, private_data->skin);
      private_data->skin          = xitk_image_load_image(private_data->imlibdata, 
							  xitk_skin_get_skin_filename(skonfig, private_data->skin_element_name));
      
      private_data->max_visible   = (private_data->skin->width/2);
      
      XITK_FREE(private_data->normal_color);
      private_data->normal_color  = xitk_skin_get_label_color(skonfig, private_data->skin_element_name);
      XITK_FREE(private_data->focused_color);
      private_data->focused_color = xitk_skin_get_label_color_focus(skonfig, private_data->skin_element_name);
      
      it->x                       = xitk_skin_get_coord_x(skonfig, private_data->skin_element_name);
      it->y                       = xitk_skin_get_coord_y(skonfig, private_data->skin_element_name);
      it->width                   = private_data->skin->width/2;
      it->height                  = private_data->skin->height;
      
      xitk_set_widget_pos(it, it->x, it->y);
    }
  }
}

/*
 * Erase one char from right of cursor.
 */
static void inputtext_erase_with_delete(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  char *p, *pp;
  int offset;
  
  if(strlen(private_data->text) > 0) {
    if((private_data->cursor_pos >= 0)
       && (private_data->cursor_pos < strlen(private_data->text))) {
      
      oldtext = strdup(private_data->text);
      newtext = (char *) xitk_xmalloc(strlen(oldtext));
      
      offset = 0;
      p = oldtext;
      pp = newtext;
      
      while(offset < private_data->cursor_pos) {
	*pp = *p;
	p++;
	pp++;
	offset++;
      }
      p++;
      
      while(*p != '\0') {
	*pp = *p;
	offset++;
	p++;
	pp++;
      } 
      
      *pp = 0;
      
      XITK_FREE(private_data->text);
      private_data->text = strdup((newtext != NULL) ? newtext : "");
      
      paint_inputtext(it, wl->win, wl->gc);
      
      XITK_FREE(oldtext);
      XITK_FREE(newtext);
    }
  }
  else {
    sprintf(private_data->text, "%s", "");
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Erase one char from left of cursor.
 */
static void inputtext_erase_with_backspace(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  char *p, *pp;
  int offset;
  
  if(strlen(private_data->text) > 1) {
    if(private_data->cursor_pos > 0) {
      
      oldtext = strdup(private_data->text);
      newtext = (char *) xitk_xmalloc(strlen(oldtext));
      
      private_data->cursor_pos--;
      
      offset = 0;
      p = oldtext;
      pp = newtext;
      
      while(offset < private_data->cursor_pos) {
	*pp = *p;
	p++;
	pp++;
	offset++;
      }
      p++;
      
      while(*p != '\0') {
	*pp = *p;
	offset++;
	p++;
	pp++;
      } 
      *pp = 0;
      
      XITK_FREE(private_data->text);
      private_data->text = strdup(newtext);
      
      paint_inputtext(it, wl->win, wl->gc);
      
      XITK_FREE(oldtext);
      XITK_FREE(newtext);
    }
  }
  else {
    sprintf(private_data->text, "%s", "");
    private_data->cursor_pos = 0;
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Erase chars from cursor pos to EOL.
 */
static void inputtext_kill_line(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  
  if(strlen(private_data->text) > 1) {
    if(private_data->cursor_pos >= 0) {

      oldtext = strdup(private_data->text);
      newtext = (char *) xitk_xmalloc(strlen(oldtext));

      snprintf(newtext, private_data->cursor_pos + 1, "%s", oldtext);

      XITK_FREE(private_data->text);
      private_data->text = strdup(newtext);
      
      paint_inputtext(it, wl->win, wl->gc);
      
      XITK_FREE(oldtext);
      XITK_FREE(newtext);
    }
  }
}

/*
 * Move cursor pos to left.
 */
static void inputtext_move_left(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if(private_data->cursor_pos > 0) {
    private_data->cursor_pos--;
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Move cursor pos to right.
 */
static void inputtext_move_right(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  if(private_data->cursor_pos < strlen(private_data->text)) {
    private_data->cursor_pos++;
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Remove focus of widget, then call callback function.
 */
static void inputtext_exec_return(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  private_data->cursor_pos = -1;
  it->have_focus = private_data->have_focus = FOCUS_LOST;
  paint_inputtext(it, wl->win, wl->gc);
  
  if(strlen(private_data->text) > 0) {
    if(private_data->callback)
      private_data->callback(it, 
			     private_data->userdata, private_data->text);
  }
}

/*
 * Remove focus of widget.
 */
static void inputtext_exec_escape(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  private_data->cursor_pos = -1;
  it->have_focus = private_data->have_focus = FOCUS_LOST;
  paint_inputtext(it, wl->win, wl->gc);
}

/*
 *
 */
static void inputtext_move_bol(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  if(private_data->text) {
    private_data->cursor_pos = private_data->disp_offset = 0;
    inputtext_recalc_offsets(wl, private_data);
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 *
 */
static void inputtext_move_eol(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  if(private_data->text) {
    private_data->cursor_pos = strlen(private_data->text);
    inputtext_recalc_offsets(wl, private_data);
    
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Transpose two characters.
 */
static void inputtext_transpose_chars(xitk_widget_list_t *wl, xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if(private_data->text && (strlen(private_data->text) >= 2)) {
    if((private_data->cursor_pos >= 2)) {
      int c = private_data->text[private_data->cursor_pos - 2];

      private_data->text[private_data->cursor_pos - 2] = private_data->text[private_data->cursor_pos - 1];
      private_data->text[private_data->cursor_pos - 1] = c;
      
      paint_inputtext(it, wl->win, wl->gc);
    }
  }
}

/*
 * Handle keyboard event in input text box.
 */
static void notify_keyevent_inputtext(xitk_widget_list_t *wl,
				      xitk_widget_t *it, XEvent *xev) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    XKeyEvent   keyevent = xev->xkey;
    KeySym      key;
    char        buf[256];
    int         len;
    int         modifier;
    
    XLOCK(private_data->imlibdata->x.disp);
    len = XLookupString(&keyevent, buf, sizeof(buf), &key, NULL);
    XUNLOCK(private_data->imlibdata->x.disp);

    buf[len] = '\0';

    (void) xitk_get_key_modifier(xev, &modifier);

    /* One of modifier key is pressed, none of keylock or shift */
    if(((buf[0] != 0) && (buf[1] == 0)) 
       && ((modifier & MODIFIER_CTRL) 
	   || (modifier & MODIFIER_META)
	   || (modifier & MODIFIER_MOD3) 
	   || (modifier & MODIFIER_MOD4)
	   || (modifier & MODIFIER_MOD5))) {
      
      switch(key) {

	/* BOL */
      case XK_a:
      case XK_A:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_move_bol(wl, it);
	}
	break;
	
	/* Backward */
      case XK_b:
      case XK_B:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_move_left(wl, it);
	}
	break;
	
	/* Cancel */
      case XK_c:
      case XK_C:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_exec_escape(wl, it);
	}
	break;
	
	/* Delete current char */
      case XK_d:
      case XK_D:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_erase_with_delete(wl, it);
	}
	break;
	
	/* EOL */
      case XK_e:
      case XK_E:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_move_eol(wl, it);
	}
	break;

	/* Forward */
      case XK_f:
      case XK_F:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_move_right(wl, it);
	}
	break;

	/* Kill line (from cursor) */
      case XK_k:
      case XK_K:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_kill_line(wl, it);
	}
	break;

	/* Return */
      case XK_m:
      case XK_M:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_exec_return(wl, it);
	}
	break;

	/* Transpose chars */
      case XK_t:
      case XK_T:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_transpose_chars(wl, it);
	}
	break;

      case XK_question:
	if(modifier & MODIFIER_CTRL) {
	  inputtext_erase_with_backspace(wl, it);
	}
	break;

      }
    }
    else if(key == XK_Delete) {
      inputtext_erase_with_delete(wl, it);
    }
    else if(key == XK_BackSpace) {
      inputtext_erase_with_backspace(wl, it);
    }
    else if(key == XK_Left) {
      inputtext_move_left(wl, it);
    }
    else if(key == XK_Right) {
      inputtext_move_right(wl, it);
    }
    else if(key == XK_Home) {
      inputtext_move_bol(wl, it);
    }
    else if(key == XK_End) {
      inputtext_move_eol(wl, it);
    }
    else if((key == XK_Return) || (key == XK_KP_Enter)) {
      inputtext_exec_return(wl, it);
    }
    else if((key == XK_Escape) || (key == XK_Tab)) {
      inputtext_exec_escape(wl, it);
    }
    else if((buf[0] != 0) && (buf[1] == 0)) {
      char *oldtext;
      int pos;
      
      if((private_data->text != NULL) 
	 && (strlen(private_data->text) < private_data->max_length)) {

	oldtext = strdup(private_data->text);
	
	if(strlen(private_data->text) <= private_data->cursor_pos)
	  pos = strlen(private_data->text);
	else
	  pos = private_data->cursor_pos;

	XITK_FREE(private_data->text);
	private_data->text = (char *) xitk_xmalloc(strlen(oldtext) + 3);

	sprintf(private_data->text, "%s", oldtext);
	sprintf(&private_data->text[pos], "%c%s%c",
		buf[0], &oldtext[pos], 0);

	private_data->cursor_pos++;
	paint_inputtext(it, wl->win, wl->gc);
	XITK_FREE(oldtext);
      }
      
    }
    else {
      //      printf("got unhandled key = [%ld]\n", key);
    }
  }
#ifdef DEBUG_GUI
  else
    fprintf (stderr, "notify key event input text on something (%d) "
	     "that is not an input text\n", it->widget_type);
#endif

}

/*
 * Return the text of widget.
 */
char *xitk_inputtext_get_text(xitk_widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    return private_data->text;
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "change input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif
  
  return NULL;
}

/*
 * Change and redisplay the text of widget.
 */
void xitk_inputtext_change_text(xitk_widget_list_t *wl, xitk_widget_t *it, char *text) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    XITK_FREE(private_data->text);
    private_data->text = strdup((text != NULL)?text:"");
    private_data->cursor_pos = -1;
    paint_inputtext(it, wl->win, wl->gc);
  }
#ifdef DEBUG_GUI
  else
    fprintf(stderr, "change input text on something (%d) "
	    "that is not an input text\n", it->widget_type);
#endif
}

/*
 * Create input text box
 */
static xitk_widget_t *_xitk_inputtext_create (xitk_skin_config_t *skonfig, 
					      xitk_inputtext_widget_t *it,
					      int x, int y, char *skin_element_name,
					      xitk_image_t *skin,
					      char *fontname,
					      char *ncolor, char *fcolor) {
  xitk_widget_t             *mywidget;
  inputtext_private_data_t  *private_data;
  
  mywidget                        = (xitk_widget_t *) xitk_xmalloc (sizeof(xitk_widget_t));
  
  private_data                    = (inputtext_private_data_t *) xitk_xmalloc(sizeof(inputtext_private_data_t));
  
  private_data->imlibdata         = it->imlibdata;
  private_data->skin_element_name = (skin_element_name == NULL) ? NULL : strdup(it->skin_element_name);

  private_data->iWidget           = mywidget;

  private_data->text              = strdup((it->text != NULL)?it->text:"");

  private_data->fontname          = strdup(fontname);
 
  private_data->max_length        = it->max_length;
  private_data->cursor_pos        = -1;

  private_data->skin              = skin;

  private_data->max_visible       = (private_data->skin->width/2);
  private_data->disp_offset       = 0;
  
  private_data->callback          = it->callback;
  private_data->userdata          = it->userdata;
  
  private_data->normal_color      = strdup(ncolor);
  private_data->focused_color     = strdup(fcolor);

  private_data->pos_in_pos        = 0;
  private_data->pos[0]            = 0;

  mywidget->private_data          = private_data;

  mywidget->enable                = 1;
  mywidget->running               = 1;
  mywidget->visible               = 1;
  mywidget->have_focus            = FOCUS_LOST;
  mywidget->x                     = x;
  mywidget->y                     = y;
  mywidget->width                 = private_data->skin->width/2;
  mywidget->height                = private_data->skin->height;
  mywidget->widget_type           = WIDGET_TYPE_INPUTTEXT;

  mywidget->paint                 = paint_inputtext;
  mywidget->notify_click          = notify_click_inputtext;
  mywidget->notify_focus          = notify_focus_inputtext;
  mywidget->notify_keyevent       = notify_keyevent_inputtext;
  mywidget->notify_inside         = notify_inside;
  mywidget->notify_change_skin    = (skin_element_name == NULL) ? NULL : notify_change_skin;
  mywidget->notify_destroy        = notify_destroy;
  mywidget->get_skin              = get_skin;

  return mywidget;
}


xitk_widget_t *xitk_inputtext_create (xitk_skin_config_t *skonfig, xitk_inputtext_widget_t *it) {

  XITK_CHECK_CONSTITENCY(it);

  return _xitk_inputtext_create (skonfig, it,
				 (xitk_skin_get_coord_x(skonfig, it->skin_element_name)),
				 (xitk_skin_get_coord_y(skonfig, it->skin_element_name)),
				 it->skin_element_name,
				 (xitk_image_load_image(it->imlibdata, 
							xitk_skin_get_skin_filename(skonfig, it->skin_element_name))),
				 (xitk_skin_get_label_fontname(skonfig, it->skin_element_name)),
				 (xitk_skin_get_label_color(skonfig, it->skin_element_name)),
				 (xitk_skin_get_label_color_focus(skonfig, it->skin_element_name)));
}

/*
 *
 */
xitk_widget_t *xitk_noskin_inputtext_create (xitk_inputtext_widget_t *it,
					     int x, int y, int width, int height,
					     char *ncolor, char *fcolor, char *fontname) {
  xitk_image_t *i;

  XITK_CHECK_CONSTITENCY(it);
  i = xitk_image_create_image(it->imlibdata, width * 2, height);
  draw_bevel_two_state(it->imlibdata, i);
  

  return _xitk_inputtext_create(NULL, it, x, y, NULL, i, fontname, ncolor, fcolor);
}

