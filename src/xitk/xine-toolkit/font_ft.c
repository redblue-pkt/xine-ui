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

#include "font_ft.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "recode.h"

/*
 * Font factory
 */

struct xitk_ft_font_factory_s {
  FT_Library  ft_lib;
  FcConfig   *fc;
};

xitk_ft_font_factory_t *xitk_ft_font_factory_create(void) {
  xitk_ft_font_factory_t *factory;

  factory = calloc(1, sizeof(*factory));
  if (!factory)
    return NULL;

  factory->fc = FcInitLoadConfigAndFonts();
  if (!factory->fc) {
    goto fail_fc;
  }

  if (FT_Init_FreeType(&factory->ft_lib)) {
    goto fail_ft;
  }

  return factory;

 fail_ft:
  FcConfigDestroy(factory->fc);
 fail_fc:
  free(factory);
  return NULL;
}

void xitk_ft_font_factory_destroy(xitk_ft_font_factory_t **p) {
  xitk_ft_font_factory_t *factory;

  if (!p || !*p)
    return;
  factory = *p;
  *p = NULL;

  FcConfigDestroy(factory->fc);
  FT_Done_FreeType(factory->ft_lib);
  free(factory);

  FcFini();
}

/* convert a -*-* .. style font description into something FontConfig can digest */
static int _font_core_string_to_fc(FcPattern *pat, const char *old_name, int *px_size) {

  char  family[50], weight[15], slant[8], pxsize[5], ptsize[5];

  if (old_name[0] != '-' && old_name[0] != '*')
    return 0;

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
  if (!strcmp(pxsize, "*"))
    pxsize[0] = '\0';
  if (!strcmp(ptsize, "*"))
    ptsize[0] = '\0';

  FcPatternAddString (pat, FC_FAMILY, (const FcChar8*)family);
  FcPatternAddBool   (pat, FC_OUTLINE, FcTrue);

  if (!strcmp(slant, "i")) FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_ITALIC);
  if (!strcmp(slant, "o")) FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_OBLIQUE);
  if (!strcmp(slant, "r")) FcPatternAddInteger(pat, FC_SLANT, FC_SLANT_ROMAN);

  if (!strcmp(weight, "medium"))  FcPatternAddInteger(pat, FC_WEIGHT,  FC_WEIGHT_MEDIUM);
  if (!strcmp(weight, "bold"))    FcPatternAddInteger(pat, FC_WEIGHT,  FC_WEIGHT_BOLD);
  if (!strcmp(weight, "regular")) FcPatternAddInteger(pat, FC_WEIGHT,  FC_WEIGHT_REGULAR);

  if (pxsize[0]) {
    *px_size = strtol(pxsize, NULL, 10);
  } else  if (ptsize[0]) {
    *px_size = strtol(pxsize, NULL, 10) * 16 / 12; /* XXX not used ? */
  }

  return 1;
}

static int _load_font(xitk_ft_font_factory_t *ft, FT_Face *face, const char *name)
{
  FcResult   fc_result = FcResultMatch;
  FcPattern *pat, *font;
  FcChar8   *fc_filename = NULL;
  int        px_size = 12;
  int        result = 0;

  pat = FcPatternCreate();
  if (!pat) {
    return 0;
  }

  if (!_font_core_string_to_fc(pat, name, &px_size)) {
    /* continue anyway, random font is better than no font ... */
    //FcPatternDestroy(pat);
    //return 0;
  }

  FcDefaultSubstitute(pat);
  if (!FcConfigSubstitute(ft->fc, pat, FcMatchPattern)) {
    FcPatternDestroy(pat);
    return 0;
  }

  font = FcFontMatch(ft->fc, pat, &fc_result);
  FcPatternDestroy(pat);
  if (!font || fc_result == FcResultNoMatch) {
    return 0;
  }

  if (FcResultMatch == FcPatternGetString(font, FC_FILE, 0, &fc_filename)) {
    result = !FT_New_Face(ft->ft_lib, (const char *)fc_filename, 0, face);
    if (result)
      FT_Set_Char_Size(*face, 0, px_size << 6, 0, 0);
  }
  FcPatternDestroy(font);

  return result;
}

/*
 * Font
 */

struct xitk_ft_font_s {
  FT_Face        font;
  xitk_recode_t *xr;
};

void xitk_ft_font_destroy(xitk_ft_font_t **_font) {
  xitk_ft_font_t *font;

  if (!_font || !*_font)
    return;

  font = *_font;
  *_font = NULL;

  FT_Done_Face(font->font);
  font->font = NULL;

  if (font->xr)
    xitk_recode_done(font->xr);

  free(font);
}

xitk_ft_font_t *xitk_ft_font_create (xitk_ft_font_factory_t *ft, const char *name) {
  xitk_ft_font_t *font;

  font = calloc (1, sizeof (*font));
  if (!font)
    return NULL;

  if (!_load_font(ft, &font->font, name)) {
    free(font);
    return NULL;
  }

  font->xr = xitk_recode_init (NULL, "UTF-16", 1);

  return font;
}

void xitk_ft_font_text_extent(xitk_ft_font_t *font, const char *c, size_t nbytes,
                              int *_lbearing, int *_rbearing, int *_width, int *_ascent, int *_descent) {

  if (_ascent)  *_ascent  = font->font->size->metrics.ascender >> 6;
  if (_descent) *_descent = -(font->font->size->metrics.descender >> 6);

  if (_width || _lbearing || _rbearing) {
    uint16_t buf[2048];
    xitk_recode_string_t rs;
    int lbearing = 0, width = 0;
    size_t i;

    /* recode right part of string to UTF-16 code points */
    rs.src = c;
    rs.ssize = strlen (c);
    if (nbytes < rs.ssize)
      rs.ssize = nbytes;
    rs.buf = (char*)buf;
    rs.bsize = sizeof (buf) / 2;
    xitk_recode2_do (font->xr, &rs);

    for (i = 0; i < rs.rsize / 2; i++) {

      if (i == 0 && buf[i] == 0xfeff /* skip BOM */)
        continue;

      if (FT_Load_Char(font->font, buf[i], FT_LOAD_DEFAULT) == 0) {
        if (i < 2 || !lbearing)
          lbearing = font->font->glyph->metrics.horiBearingX >> 6;
        width += font->font->glyph->metrics.horiAdvance >> 6;
      }
    }

    xitk_recode2_done (font->xr, &rs);

    if (_width)    *_width = width;
    if (_lbearing) *_lbearing = -lbearing;
    if (_rbearing) *_rbearing = -lbearing + width;
  }
}

static uint32_t _render(unsigned a, uint32_t bg, uint32_t c) {
  //if (a == 0) return bg;
  if (a == 0xff) return (bg & 0xff000000) | (c & 0x00ffffff);
  return
       (bg & 0xff000000) |
    ((((bg & 0x00ff0000) * (0xff - a) + (c & 0x00ff0000) * a) / 0xff) & 0x00ff0000 /* cant overflow, just wipe out remainder */) |
    ((((bg & 0x0000ff00) * (0xff - a) + (c & 0x0000ff00) * a) / 0xff) & 0x0000ff00) |
    ((((bg & 0x000000ff) * (0xff - a) + (c & 0x000000ff) * a) / 0xff) /*& 0x000000ff*/) ;
}

void xitk_ft_font_draw_string(xitk_ft_font_t *font, uint32_t *dst,
                              int x, int y, int stride, int max_h,
                              const char *text, size_t nbytes, uint32_t color) {
  uint16_t      buf[2*2048];
  FT_Face       face = font->font;
  xitk_recode_string_t rs;

  /* recode right part of string to UTF-16 code points */
  rs.src = text;
  rs.ssize = strlen (text);
  if ((size_t)nbytes < rs.ssize)
    rs.ssize = nbytes;
  rs.buf = (char *)buf;
  rs.bsize = sizeof (buf);
  xitk_recode2_do (font->xr, &rs);

  for (size_t i = 0; i < rs.rsize / 2; i++) {

    /* skip BOM */
    if (i == 0 && buf[i] == 0xfeff)
      continue;

    if (FT_Load_Char(font->font, buf[i], FT_LOAD_RENDER) == 0) {
      unsigned jj, kk;
      for (jj = 0; jj < face->glyph->bitmap.rows; jj++) {
        int ypos = y - face->glyph->bitmap_top + jj;
        if (ypos >= 0 && ypos < max_h) {
          for (kk = 0; kk < face->glyph->bitmap.width; kk++) {
            uint8_t alpha = face->glyph->bitmap.buffer[jj * face->glyph->bitmap.pitch + kk];
            if (alpha) {
              int xpos = x + face->glyph->bitmap_left + kk;
              if (xpos >= 0 && xpos < stride) {
                dst[xpos + ypos * stride] = _render(alpha, dst[xpos + ypos * stride], color);
              }
            }
          }
        }
      }
    }
    x += face->glyph->metrics.horiAdvance >> 6;
  }

  xitk_recode2_done (font->xr, &rs);
}
