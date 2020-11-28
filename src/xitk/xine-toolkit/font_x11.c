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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "font_x11.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_XMB
#include <wchar.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef WITH_XFT
#include <X11/Xft/Xft.h>
#endif

#include "_xitk.h"  // replace with local xmb ?

#ifdef WITH_XFT
#include "recode.h"
#endif

struct xitk_x11_font_s {
#ifdef WITH_XFT
  xitk_t        *xitk;
  XftFont       *font;
  xitk_recode_t *xr;
#else /* WITH_XFT */
  const char    *name;
  XFontStruct   *font;
# ifdef WITH_XMB
  XFontSet       fontset;
# endif /* WITH_XMB */

  void (*text_extent)(xitk_x11_font_t *, const char *c, size_t nbytes,
                      int *lbearing, int *rbearing, int *width, int *ascent, int *descent);
  void (*draw_string)(Display *, xitk_x11_font_t *, Drawable img,
                      int x, int y, const char *text, size_t nbytes, uint32_t color);
#endif /* WITH_XFT */
};


#ifdef WITH_XFT
/* convert a -*-* .. style font description into something Xft can digest */
static const char *_font_core_string_to_xft(char *new_name, size_t new_name_size, const char * old_name) {

  if(old_name[0] == '-' || old_name[0] == '*') {
    char  family[50], weight[15], slant[8], pxsize[5], ptsize[5];

    if( old_name[0] == '-' )
      old_name++;

    /* Extract description elements */
    family[0] = weight[0] = slant[0] = pxsize[0] = ptsize[0] = '\0';
    sscanf(old_name, "%*[^-]-%49[^-]", family);
    sscanf(old_name, "%*[^-]-%*[^-]-%14[^-]", weight);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%7[^-]", slant);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%4[^-]", pxsize);
    sscanf(old_name, "%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%4[^-]", ptsize);

    /* Transform the elements */
    if(!strcmp(weight, "*"))
      weight[0] = '\0';
    if(!strcmp(pxsize, "*"))
      pxsize[0] = '\0';
    if(!strcmp(ptsize, "*"))
      ptsize[0] = '\0';
    strcpy(slant,
           (!strcmp(slant, "i")) ? "italic" :
           (!strcmp(slant, "o")) ? "oblique" :
           (!strcmp(slant, "r")) ? "roman" :
                                   "");

    /* Xft doesn't have lucida, which is a small font;
     * thus we make whatever is chosen 2 sizes smaller */
    if(!strcasecmp(family, "lucida")) {
      if(pxsize[0]) {
        int sz = strtol(pxsize, NULL, 10) - 2;
        snprintf(pxsize, sizeof(pxsize), "%i", sz);
      }
      if(ptsize[0]) {
        int sz = strtol(ptsize, NULL, 10) - 2;
        snprintf(ptsize, sizeof(ptsize), "%i", sz);
      }
    }

    /* Build Xft font description string from the elements */
    snprintf(new_name, new_name_size, "%s%s%s%s%s%s%s%s%s",
             family,
             ((weight[0]) ? ":weight="    : ""), weight,
             ((slant[0])  ? ":slant="     : ""), slant,
             ((pxsize[0]) ? ":pixelsize=" : ""), pxsize,
             ((ptsize[0]) ? ":size="      : ""), ptsize);

    return new_name;
  }

  return old_name;
}
#endif

#ifndef WITH_XFT
static int _font_is_font_8(xitk_x11_font_t *font) {
  return ((font->font->min_byte1 == 0) && (font->font->max_byte1 == 0));
}
#endif

#if defined(WITH_XMB) && !defined(WITH_XFT)
/*
 * XCreateFontSet requires font name starting with '-'
 */
static char *_font_right_name(const char *name) {
  char *right_name;

  ABORT_IF_NULL(name);

  right_name = strdup(name);

  switch(right_name[0]) {
  case '-':
    break;
  case '*':
    right_name[0] = '-';
    break;
  default:
    right_name[0] = '*';
    break;
  }

  return right_name;
}
#endif /* !WITH_XFT && WITH_XMB */

/*
 *
 */

#if !defined(WITH_XFT) && defined(WITH_XMB)
/*
 * Guess if error occured, release missing charsets list.
 * There is a problem with rendering when "ISO10646-1" charset is missing.
 */
static int _font_guess_error(XFontSet fs, char *name, char **missing, int count) {
  int i;

  if (fs == NULL) {
    if (missing)
      XFreeStringList(missing);
    return 1;
  }

  /* some uncovered codepages */
  for (i = 0; i < count; i++) {
    if (strcasecmp("ISO10646-1", missing[i]) == 0) {
      XITK_WARNING("font \"%s\" doesn't contain charset %s\n", name, missing[i]);
      XFreeStringList(missing);
      return 1;
    }
  }

  if (missing)
    XFreeStringList(missing);

  return 0;
}
#endif /* !WITH_XFT && WITH_XMB */

#ifdef WITH_XFT
static void _xft_font_text_extent(Display *display, xitk_x11_font_t *font, const char *c, size_t nbytes,
                                  int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  XGlyphInfo xft_extents;
  char buf[2048];
  xitk_recode_string_t rs;

  ABORT_IF_NULL(font->font);

  /* recode right part of string */
  rs.src = c;
  rs.ssize = strlen (c);
  if (nbytes < rs.ssize)
    rs.ssize = nbytes;
  rs.buf = buf;
  rs.bsize = sizeof (buf);
  xitk_recode2_do (font->xr, &rs);

  XftTextExtentsUtf8 (display, font->font, (FcChar8 *)rs.res, rs.rsize, &xft_extents);

  xitk_recode2_done (font->xr, &rs);

  if (width) *width       = xft_extents.xOff;
  /* Since Xft fonts have more top and bottom space rows than Xmb/Xcore fonts, */
  /* we tweak ascent and descent to appear the same like Xmb/Xcore, so we get  */
  /* unified vertical text positions. We must however be aware to eventually   */
  /* reserve space for these rows when drawing the text.                       */
  /* Following trick works for our font sizes 10...14.                         */
  if (ascent) *ascent     = font->font->ascent - (font->font->ascent - 9) / 2;
  if (descent) *descent   = font->font->descent - (font->font->descent - 0) / 2;
  if (lbearing) *lbearing = -xft_extents.x;
  if (rbearing) *rbearing = -xft_extents.x + xft_extents.width;
}
#endif /* WITH_XFT */

#if !defined(WITH_XFT) && defined(WITH_XMB)
static void _xmb_font_text_extent(xitk_x11_font_t *font, const char *c, size_t nbytes,
                                  int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  XRectangle ink, logic;

  ABORT_IF_NULL(font->fontset);

  XmbTextExtents(font->fontset, c, nbytes, &ink, &logic);

  if (!logic.width || !logic.height) {
    /* XmbTextExtents() has problems, try char-per-char counting */
    mbstate_t state;
    size_t    i = 0, n;
    int       height = 0, width = 0, y = 0, lb = 0, rb;

    memset(&state, '\0', sizeof(mbstate_t));

    while (i < nbytes) {
      n = mbrtowc(NULL, c + i, nbytes - i, &state);
      if (n == (size_t)-1 || n == (size_t)-2 || i + n > nbytes) n = 1;
      XmbTextExtents(font->fontset, c + i, n, &ink, &logic);
      if (logic.height > height) height = logic.height;
      if (logic.y < y) y = logic.y; /* y is negative! */
      if (i == 0) lb = ink.x; /* left bearing of string */
      width += logic.width;
      i += n;
    }
    rb = width - logic.width + ink.x + ink.width; /* right bearing of string */

    if (!height || !width) {
      /* char-per-char counting fails too */
      XITK_WARNING("extents of the font \"%s\" are %dx%d!\n", font->name, width, height);
      if (!height) height = 12, y = -10;
      if (!width) lb = rb = 0;
    }

    logic.height = height;
    logic.width = width;
    logic.y = y;
    ink.x = lb;
    ink.width = rb - lb;
  }

  if (width)    *width    = logic.width;
  if (ascent)   *ascent   = -logic.y;
  if (descent ) *descent  = logic.height + logic.y;
  if (lbearing) *lbearing = ink.x;
  if (rbearing) *rbearing = ink.x + ink.width;
}
#endif /* !WITH_XFT && WITH_XMB */

#ifndef WITH_XFT
static void _x11_font_text_extent(xitk_x11_font_t *font, const char *c, size_t nbytes,
                                  int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
  XCharStruct ov;
  int         dir;
  int         fascent, fdescent;

  ABORT_IF_NULL(font->font);

  if (_font_is_font_8(font))
    XTextExtents(font->font, c, nbytes, &dir, &fascent, &fdescent, &ov);
  else
    XTextExtents16(font->font, (XChar2b *)c, (nbytes / 2), &dir, &fascent, &fdescent, &ov);

  if (lbearing) *lbearing = ov.lbearing;
  if (rbearing) *rbearing = ov.rbearing;
  if (width)    *width    = ov.width;
  if (ascent)   *ascent   = font->font->ascent;
  if (descent)  *descent  = font->font->descent;
}
#endif /* !WITH_XFT */

#ifdef WITH_XFT
static void _xft_font_draw_string (Display *display, xitk_x11_font_t *font, Drawable img,
    int x, int y, const char *text, size_t nbytes, uint32_t color) {

  int           screen   = DefaultScreen( display );
  Visual       *visual   = DefaultVisual( display, screen );
  Colormap      colormap = DefaultColormap( display, screen );
  xitk_color_info_t info;
  XftColor      xcolor;
  XftDraw      *xft_draw;
  char          buf[2048];
  xitk_recode_string_t rs;

  rs.src = text;
  rs.ssize = strlen (text);
  if ((size_t)nbytes < rs.ssize)
    rs.ssize = nbytes;
  rs.buf = buf;
  rs.bsize = sizeof (buf);
  xitk_recode2_do (font->xr, &rs);

  info.value = color;
  xitk_color_db_query_value (font->xitk, &info);
  xft_draw = XftDrawCreate (display, img, visual, colormap);
#define XITK_FONT_TEMPTED
#ifdef XITK_FONT_TEMPTED
  /* we are tempted to fill in a XftColor directly here.
   * however, xft will need a few more colors for smoothing internally ?? */
  xcolor.pixel       = info.value;
  xcolor.color.red   = info.r;
  xcolor.color.green = info.g;
  xcolor.color.blue  = info.b;
  xcolor.color.alpha = info.a;
  XftDrawStringUtf8 (xft_draw, &xcolor, font->font, x, y, (FcChar8 *)rs.res, rs.rsize);
#else
  {
    XRenderColor  xr_color;

    xr_color.red   = info.r;
    xr_color.green = info.g;
    xr_color.blue  = info.b;
    xr_color.alpha = info.a;
    XftColorAllocValue (display, visual, colormap, &xr_color, &xcolor);
    XftDrawStringUtf8 (xft_draw, &xcolor, font->font, x, y, (FcChar8 *)rs.res, rs.rsize);
    XftColorFree (display, visual, colormap, &xcolor);
  }
#endif
  XftDrawDestroy(xft_draw);

  xitk_recode2_done (font->xr, &rs);
}
#endif /* WITH_XFT */

#if !defined(WITH_XFT) && defined(WITH_XMB)
static void _xmb_font_draw_string (Display *display, xitk_x11_font_t *font, Drawable img,
    int x, int y, const char *text, size_t nbytes, uint32_t color) {

  XGCValues gcv;
  GC gc;

  gcv.graphics_exposures = False;

  gc = XCreateGC (display, img, GCGraphicsExposures, &gcv);
  XSetForeground (display, gc, color);
  XmbDrawString (display, img, font->fontset, gc, x, y, text, nbytes);
  XFreeGC (display, gc);
}
#endif /* !WITH_XFT && WITH_XMB */

#ifndef WITH_XFT
static void _x11_font_draw_string (Display *display, xitk_x11_font_t *font, Drawable img,
    int x, int y, const char *text, size_t nbytes, uint32_t color) {

  XGCValues gcv;
  GC gc;

  gcv.graphics_exposures = False;

  gc = XCreateGC (display, img, GCGraphicsExposures, &gcv);
  XSetForeground (display, gc, color);
  XSetFont(display, gc, font->font->fid);
  XDrawString (display, img, gc, x, y, text, nbytes);
  XFreeGC (display, gc);
}
#endif /* !WITH_XFT */

/*
 * Interface
 */

void xitk_x11_font_text_extent(Display *d, xitk_x11_font_t *f, const char *c, size_t nbytes,
                               int *lbearing, int *rbearing, int *width, int *ascent, int *descent) {
#ifdef WITH_XFT
  _xft_font_text_extent(d, f, c, nbytes, lbearing, rbearing, width, ascent, descent);
#else
  (void)d;
  f->text_extent(f, c, nbytes, lbearing, rbearing, width, ascent, descent);
#endif
}

void xitk_x11_font_draw_string (Display *d, xitk_x11_font_t *f, Drawable img,
                                int x, int y, const char *text, size_t nbytes, uint32_t color) {
#ifdef WITH_XFT
  _xft_font_draw_string(d, f, img, x, y, text, nbytes, color);
#else
  f->draw_string(d, f, img, x, y, text, nbytes, color);
#endif
}

void xitk_x11_font_destroy(Display *display, xitk_x11_font_t **_font) {
  xitk_x11_font_t *font;

  if (!_font || !*_font)
    return;

  font = *_font;
  *_font = NULL;

#ifdef WITH_XFT
  if (font->font)
    XftFontClose (display, font->font);
#else
#ifdef WITH_XMB
  if (font->fontset)
    XFreeFontSet (display, font->fontset);
#endif
  if (font->font)
    XFreeFont (display, font->font);
#endif

#ifdef WITH_XFT
  if (font->xr)
    xitk_recode_done(font->xr);
#endif

  free(font);
}

xitk_x11_font_t *xitk_x11_font_create (xitk_t *xitk, Display *display, const char *name) {
  xitk_x11_font_t *font;
#ifdef WITH_XFT
  char   new_name[255];
#endif
  int    ok;

  font = calloc (1, sizeof (*font) + strlen(name) + 1);
  if (!font)
    return NULL;

#ifdef WITH_XFT
  font->xitk = xitk;
  font->xr = xitk_recode_init (NULL, "UTF-8", 1);
  font->font = XftFontOpenName (display, DefaultScreen (display),
                                _font_core_string_to_xft(new_name, sizeof(new_name), name));
  ok = (font->font != NULL);
#else /* WITH_XFT */
  (void)xitk;
# ifdef WITH_XMB
  if (xitk_get_cfg_num (xitk, XITK_XMB_ENABLE)) {
    char *right_name = _font_right_name(name);
    char **missing = NULL;
    char  *def = NULL;
    int    count;
    font->fontset = XCreateFontSet (display, right_name, &missing, &count, &def);
    ok = !_font_guess_error(font->fontset, right_name, missing, count);
    free(right_name);
  }
  else
# endif /* WITH_XMB */
  {
    font->font = XLoadQueryFont (display, name);
    ok = (font->font != NULL);
  }
#endif /* WITH_XFT */

  if (!ok) {
    free(font);
    return NULL;
  }

#ifndef WITH_XFT
  font->name = strcpy((char *)(font + 1), name);
# ifdef WITH_XMB
  if (font->fontset) {
    font->text_extent = _xmb_font_text_extent;
    font->draw_string = _xmb_font_draw_string;
  }
  else
# endif /* WITH_XMB */
  {
    font->text_extent = _x11_font_text_extent;
    font->draw_string = _x11_font_draw_string;
  }
#endif /* WITH_XFT */

  return font;
}
