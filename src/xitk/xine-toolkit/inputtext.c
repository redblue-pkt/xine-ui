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
#include "widget_types.h"

#include "_xitk.h"

#define NORMAL                  1
#define FOCUS                   2

#define MODIFIER_NOMOD 0x00000000
#define MODIFIER_SHIFT 0x00000001
#define MODIFIER_LOCK  0x00000002
#define MODIFIER_CTRL  0x00000004
#define MODIFIER_META  0x00000008
#define MODIFIER_NUML  0x00000010
#define MODIFIER_MOD3  0x00000020
#define MODIFIER_MOD4  0x00000040
#define MODIFIER_MOD5  0x00000080

/*
 * Return a pixmap with text drawed.
 */
static Pixmap create_labelofinputtext(widget_t *it, 
				      Window win, GC gc, Pixmap pix, 
				      int xsize, int ysize, 
				      char *label, int state) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char               *plabel = label;
  XFontStruct        *fs = NULL;
  XCharStruct         cs;
  int                 dir, as, des, len, yoff = 0, DefaultColor = -1;
  unsigned int        fg;
  XColor              color;
  gui_color_names_t  *gColors;

  gColors = gui_get_color_names();

  color.flags = DoRed|DoBlue|DoGreen;

  /* Try to load font */
  /*
  if(private_data->fontname)
    fs = XLoadQueryFont(private_data->display, private_data->fontname);
  */
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "fixed");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "times-roman");
  if(fs == NULL) fs = XLoadQueryFont(private_data->display, "*times-medium-r*");

  if(fs == NULL) {
    fprintf(stderr, "%s()@%d: XLoadQueryFont() returned NULL!. Exiting\n",
	    __FUNCTION__, __LINE__);
    exit(1);
  }

  XSetFont(private_data->display, gc, fs->fid);

  XTextExtents(fs, label, strlen(label), &dir, &as, &des, &cs);
  len = cs.width;

  /*  Some colors configurations */
  switch(state) {
  case NORMAL:
    if(!strcasecmp(private_data->normal_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->normal_color)) {
	  break;
	}
      } 
    }
    break;

  case FOCUS:
    if(!strcasecmp(private_data->focused_color, "Default")) {
      DefaultColor = 0;
    }
    else {
      for(; gColors->colorname != NULL; gColors++) {
	if(!strcasecmp(gColors->colorname, private_data->focused_color)) {
	  break;
	}
      } 
    }
    break;
    
  }
  
  if(gColors->colorname == NULL || DefaultColor != -1) {
    color.red = color.blue = color.green = DefaultColor<<8;
  }
  else {
    color.red = gColors->red<<8; 
    color.blue = gColors->blue<<8;
    color.green = gColors->green<<8;
  }

  XAllocColor(private_data->display,
	      DefaultColormap(private_data->display, 0), &color);
  fg = color.pixel;
  
  XSetForeground(private_data->display, gc, fg);

  if((private_data->cursor_pos - private_data->disp_offset)
     > private_data->max_visible) {
    private_data->disp_offset += private_data->max_visible - 1;
  }
  else if(private_data->disp_offset > private_data->cursor_pos) {
    private_data->disp_offset -= private_data->max_visible;
    if(private_data->disp_offset < 0)
      private_data->disp_offset = 0;
  }

  if(private_data->disp_offset)
    plabel += private_data->disp_offset;

  /*  Put text in the right place */
  XDrawString(private_data->display, pix, gc, 
	      2, ((ysize+as+des+yoff)>>1)-des, 
	      plabel, strlen(plabel));

  /* Draw cursor pointer */
  if(private_data->cursor_pos >= 0) {
    
    XDrawLine(private_data->display, pix, gc,
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 1,
	      2, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 3,
	      2);

    XDrawLine(private_data->display, pix, gc, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 2
	      , 2, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 2,
	      ysize - 3);

    XDrawLine(private_data->display, pix, gc, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 1,
	      ysize - 3, 
	      (6 * (private_data->cursor_pos - private_data->disp_offset)) + 3,
	      ysize - 3);

  }
  
  XFreeFont(private_data->display, fs);

  return pix;
}

/*
 * Paint the input text box.
 */
static void paint_inputtext(widget_t *it, Window win, GC gc) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int          button_width, state = 0;
  gui_image_t *skin;
  Pixmap       btn, bgtmp;
  XWindowAttributes attr;

  if ((it->widget_type & WIDGET_TYPE_INPUTTEXT) && it->visible) {
        
    XGetWindowAttributes(private_data->display, win, &attr);
    
    skin = private_data->skin;
    
    button_width = skin->width/2;
    
    bgtmp = XCreatePixmap(private_data->display, skin->image,
			  button_width, skin->height, attr.depth);
    
    XLOCK(private_data->display);
    
    if(private_data->have_focus) {
      state = FOCUS;
      XCopyArea (private_data->display, skin->image,
		 bgtmp, gc, button_width, 0,
		 button_width, skin->height, 0, 0);
    }
    else {
      state = NORMAL;
      XCopyArea (private_data->display, skin->image,
		 bgtmp, gc, 0, 0,
		 button_width, skin->height, 0, 0);
    }
    

    btn = create_labelofinputtext(it, win, gc, bgtmp,
				  button_width, skin->height, 
				  private_data->text, state);
    
    XCopyArea (private_data->display, btn, win, gc, 0, 0,
	       button_width, skin->height, it->x, it->y);


    XFreePixmap(private_data->display, bgtmp);

    XUNLOCK(private_data->display);
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
static int notify_click_inputtext(widget_list_t *wl, widget_t *it, 
				   int bUp, int x, int y) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  int pos;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {

    if(it->have_focus == FOCUS_LOST) {
      it->have_focus = private_data->have_focus = FOCUS_RECEIVED;
    }

    pos = x - it->x;
    pos /= 6;

    if(private_data->text) {
      if(pos > strlen(private_data->text))
	pos = strlen(private_data->text);
    }
    else
      pos = 0;

    private_data->cursor_pos = pos;

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
static int notify_focus_inputtext(widget_list_t *wl, 
				   widget_t *it, int bEntered) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    private_data->have_focus = bEntered;
    if(bEntered == FOCUS_LOST) {
      private_data->cursor_pos = -1;
    }
    else {
      private_data->cursor_pos = strlen(private_data->text);
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
 * Erase one char from right of cursor.
 */
static void inputtext_erase_with_delete(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  char *p, *pp;
  int offset;
  
  if(strlen(private_data->text) > 0) {
    if((private_data->cursor_pos >= 0)
       && (private_data->cursor_pos < strlen(private_data->text))) {
      
      oldtext = strdup(private_data->text);
      newtext = (char *) gui_xmalloc(strlen(oldtext));
      
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
      
      free(private_data->text);
      private_data->text = strdup((newtext != NULL) ? newtext : "");
      
      paint_inputtext(it, wl->win, wl->gc);
      
      free(oldtext);
      free(newtext);
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
static void inputtext_erase_with_backspace(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  char *p, *pp;
  int offset;
  
  if(strlen(private_data->text) > 1) {
    if(private_data->cursor_pos > 0) {
      
      oldtext = strdup(private_data->text);
      newtext = (char *) gui_xmalloc(strlen(oldtext));
      
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
      
      free(private_data->text);
      private_data->text = strdup(newtext);
      
      paint_inputtext(it, wl->win, wl->gc);
      
      free(oldtext);
      free(newtext);
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
static void inputtext_kill_line(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  char *oldtext, *newtext;
  
  if(strlen(private_data->text) > 1) {
    if(private_data->cursor_pos >= 0) {

      oldtext = strdup(private_data->text);
      newtext = (char *) gui_xmalloc(strlen(oldtext));

      snprintf(newtext, private_data->cursor_pos + 1, "%s", oldtext);

      free(private_data->text);
      private_data->text = strdup(newtext);
      
      paint_inputtext(it, wl->win, wl->gc);
      
      free(oldtext);
      free(newtext);
    }
  }
}

/*
 * Move cursor pos to left.
 */
static void inputtext_move_left(widget_list_t *wl, widget_t *it) {
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
static void inputtext_move_right(widget_list_t *wl, widget_t *it) {
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
static void inputtext_exec_return(widget_list_t *wl, widget_t *it) {
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
static void inputtext_exec_escape(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  private_data->cursor_pos = -1;
  it->have_focus = private_data->have_focus = FOCUS_LOST;
  paint_inputtext(it, wl->win, wl->gc);
}

/*
 *
 */
static void inputtext_move_bol(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  if(private_data->text) {
    private_data->cursor_pos = 0;
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 *
 */
static void inputtext_move_eol(widget_list_t *wl, widget_t *it) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;

  if(private_data->text) {
    private_data->cursor_pos = strlen(private_data->text);
    paint_inputtext(it, wl->win, wl->gc);
  }
}

/*
 * Extract modifier keys.
 */
static int get_key_modifier(XEvent *xev, int *mod) {
  
  *mod = MODIFIER_NOMOD;

  /*
  if(xev->xbutton.state & ShiftMask)
    printf("Shift\n");
  if(xev->xbutton.state & LockMask)
    printf("Lock\n");
  if(xev->xbutton.state & ControlMask)
    printf("Control\n");
  if(xev->xbutton.state & Mod1Mask)
    printf("Mod1\n");
  if(xev->xbutton.state & Mod2Mask)
    printf("Mod2\n");
  if(xev->xbutton.state & Mod3Mask)
    printf("Mod3\n");
  if(xev->xbutton.state & Mod4Mask)
    printf("Mod4\n");
  if(xev->xbutton.state & Mod5Mask)
    printf("Mod5\n");
  */
  
  if(xev->xbutton.state & ShiftMask)
    *mod |= MODIFIER_SHIFT;

  if(xev->xbutton.state & LockMask)
    *mod |= MODIFIER_LOCK;

  if(xev->xbutton.state & ControlMask)
    *mod |= MODIFIER_CTRL;

  if(xev->xbutton.state & Mod1Mask)
    *mod |= MODIFIER_META;

  if(xev->xbutton.state & Mod2Mask)
    *mod |= MODIFIER_NUML;

  if(xev->xbutton.state & Mod3Mask)
    *mod |= MODIFIER_MOD3;

  if(xev->xbutton.state & Mod4Mask)
    *mod |= MODIFIER_MOD4;

  if(xev->xbutton.state & Mod5Mask)
    *mod |= MODIFIER_MOD5;

  return (*mod != MODIFIER_NOMOD);
}

/*
 * Handle keyboard event in input text box.
 */
static void notify_keyevent_inputtext(widget_list_t *wl,
				      widget_t *it, XEvent *xev) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    XKeyEvent   keyevent = xev->xkey;
    KeySym      key;
    char        buf[256];
    int         len;
    int         modifier;
    
    XLOCK(private_data->display);
    len = XLookupString(&keyevent, buf, sizeof(buf), &key, NULL);
    XUNLOCK(private_data->display);

    buf[len] = '\0';

    get_key_modifier(xev, &modifier);

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

	free(private_data->text);
	private_data->text = (char *) gui_xmalloc(strlen(oldtext) + 3);

	sprintf(private_data->text, "%s", oldtext);
	sprintf(&private_data->text[pos], "%c%s%c",
		buf[0], &oldtext[pos], 0);

	private_data->cursor_pos++;
	paint_inputtext(it, wl->win, wl->gc);
	free(oldtext);
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
char *inputttext_get_text(widget_t *it) {
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
void inputtext_change_text(widget_list_t *wl, widget_t *it, const char *text) {
  inputtext_private_data_t *private_data = 
    (inputtext_private_data_t *) it->private_data;
  
  if (it->widget_type & WIDGET_TYPE_INPUTTEXT) {
    free(private_data->text);
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
widget_t *inputtext_create (xitk_inputtext_t *it) {
  widget_t                  *mywidget;
  inputtext_private_data_t  *private_data;
  
  mywidget = (widget_t *) gui_xmalloc (sizeof(widget_t));
  
  private_data                = (inputtext_private_data_t *) 
    gui_xmalloc (sizeof(inputtext_private_data_t));
  
  private_data->display       = it->display;
  
  private_data->iWidget       = mywidget;
  private_data->text          = strdup((it->text != NULL)?it->text:"");

  private_data->fontname       = (it->fontname) ? strdup(it->fontname) : NULL;
 
  private_data->max_length    = it->max_length;
  private_data->cursor_pos    = -1;

  private_data->skin          = gui_load_image(it->imlibdata, it->skin_filename);

  private_data->max_visible   = (private_data->skin->width/2) / 6;
  private_data->disp_offset   = 0;

  private_data->callback      = it->callback;
  private_data->userdata      = it->userdata;

  private_data->normal_color  = strdup(it->normal_color);
  private_data->focused_color = strdup(it->focused_color);

  mywidget->private_data      = private_data;

  mywidget->enable            = 1;
  mywidget->running           = 1;
  mywidget->visible         = 1;
  mywidget->have_focus        = FOCUS_LOST;
  mywidget->x                 = it->x;
  mywidget->y                 = it->y;

  mywidget->width             = private_data->skin->width/2;
  mywidget->height            = private_data->skin->height;
  mywidget->widget_type       = WIDGET_TYPE_INPUTTEXT;

  mywidget->paint             = paint_inputtext;
  mywidget->notify_click      = notify_click_inputtext;
  mywidget->notify_focus      = notify_focus_inputtext;
  mywidget->notify_keyevent   = notify_keyevent_inputtext;

  return mywidget;
}
