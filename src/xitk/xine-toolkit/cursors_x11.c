/*
 * Copyright (C) 2004-2020 the xine project
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

#include "cursors_x11.h"

#include <string.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include "_xitk.h"

#define MAX_CURSORS  xitk_cursor_num_glyphs

#define X_CURSOR     0
#define XITK_CURSOR  1

static const struct cursors_init_s {
  xitk_cursors_t  xitk_shape;
  unsigned int    x_shape;
  unsigned int    embedded;
} cursors_init[] = {
  { xitk_cursor_invisible,              0,                      XITK_CURSOR,   },
  { xitk_cursor_X_cursor,               XC_X_cursor,            X_CURSOR,      },
  { xitk_cursor_arrow,                  XC_arrow,               X_CURSOR,      },
  { xitk_cursor_based_arrow_down,       XC_based_arrow_down,    X_CURSOR,      },
  { xitk_cursor_based_arrow_up,         XC_based_arrow_up,      X_CURSOR,      },
  { xitk_cursor_boat,                   XC_boat,                X_CURSOR,      },
  { xitk_cursor_bogosity,               XC_bogosity,            X_CURSOR,      },
  { xitk_cursor_bottom_left_corner,     XC_bottom_left_corner,  XITK_CURSOR,   },
  { xitk_cursor_bottom_right_corner,    XC_bottom_right_corner, XITK_CURSOR,   },
  { xitk_cursor_bottom_side,            XC_bottom_side,         XITK_CURSOR,   },
  { xitk_cursor_bottom_tee,             XC_bottom_tee,          X_CURSOR,      },
  { xitk_cursor_box_spiral,             XC_box_spiral,          X_CURSOR,      },
  { xitk_cursor_center_ptr,             XC_center_ptr,          XITK_CURSOR,   },
  { xitk_cursor_circle,                 XC_circle,              XITK_CURSOR,   },
  { xitk_cursor_clock,                  XC_clock,               X_CURSOR,      },
  { xitk_cursor_coffee_mug,             XC_coffee_mug,          X_CURSOR,      },
  { xitk_cursor_cross,                  XC_cross,               XITK_CURSOR,   },
  { xitk_cursor_cross_reverse,          XC_cross_reverse,       X_CURSOR,      },
  { xitk_cursor_crosshair,              XC_crosshair,           X_CURSOR,      },
  { xitk_cursor_diamond_cross,          XC_diamond_cross,       X_CURSOR,      },
  { xitk_cursor_dot,                    XC_dot,                 X_CURSOR,      },
  { xitk_cursor_dotbox,                 XC_dotbox,              XITK_CURSOR,   },
  { xitk_cursor_double_arrow,           XC_double_arrow,        XITK_CURSOR,   },
  { xitk_cursor_draft_large,            XC_draft_large,         X_CURSOR,      },
  { xitk_cursor_draft_small,            XC_draft_small,         X_CURSOR,      },
  { xitk_cursor_draped_box,             XC_draped_box,          XITK_CURSOR,   },
  { xitk_cursor_exchange,               XC_exchange,            X_CURSOR,      },
  { xitk_cursor_fleur,                  XC_fleur,               XITK_CURSOR,   },
  { xitk_cursor_gobbler,                XC_gobbler,             X_CURSOR,      },
  { xitk_cursor_gumby,                  XC_gumby,               X_CURSOR,      },
  { xitk_cursor_hand1,                  XC_hand1,               X_CURSOR,      },
  { xitk_cursor_hand2,                  XC_hand2,               XITK_CURSOR,   },
  { xitk_cursor_heart,                  XC_heart,               X_CURSOR,      },
  { xitk_cursor_icon,                   XC_icon,                X_CURSOR,      },
  { xitk_cursor_iron_cross,             XC_iron_cross,          X_CURSOR,      },
  { xitk_cursor_left_ptr,               XC_left_ptr,            XITK_CURSOR,   },
  { xitk_cursor_left_side,              XC_left_side,           XITK_CURSOR,   },
  { xitk_cursor_left_tee,               XC_left_tee,            X_CURSOR,      },
  { xitk_cursor_leftbutton,             XC_leftbutton,          X_CURSOR,      },
  { xitk_cursor_ll_angle,               XC_ll_angle,            X_CURSOR,      },
  { xitk_cursor_lr_angle,               XC_lr_angle,            X_CURSOR,      },
  { xitk_cursor_man,                    XC_man,                 X_CURSOR,      },
  { xitk_cursor_middlebutton,           XC_middlebutton,        X_CURSOR,      },
  { xitk_cursor_mouse,                  XC_mouse,               X_CURSOR,      },
  { xitk_cursor_pencil,                 XC_pencil,              XITK_CURSOR,   },
  { xitk_cursor_pirate,                 XC_pirate,              X_CURSOR,      },
  { xitk_cursor_plus,                   XC_plus,                X_CURSOR,      },
  { xitk_cursor_question_arrow,         XC_question_arrow,      XITK_CURSOR,   },
  { xitk_cursor_right_ptr,              XC_right_ptr,           XITK_CURSOR,   },
  { xitk_cursor_right_side,             XC_right_side,          XITK_CURSOR,   },
  { xitk_cursor_right_tee,              XC_right_tee,           X_CURSOR,      },
  { xitk_cursor_rightbutton,            XC_rightbutton,         X_CURSOR,      },
  { xitk_cursor_rtl_logo,               XC_rtl_logo,            X_CURSOR,      },
  { xitk_cursor_sailboat,               XC_sailboat,            X_CURSOR,      },
  { xitk_cursor_sb_down_arrow,          XC_sb_down_arrow,       XITK_CURSOR,   },
  { xitk_cursor_sb_h_double_arrow,      XC_sb_h_double_arrow,   XITK_CURSOR,   },
  { xitk_cursor_sb_left_arrow,          XC_sb_left_arrow,       XITK_CURSOR,   },
  { xitk_cursor_sb_right_arrow,         XC_sb_right_arrow,      XITK_CURSOR,   },
  { xitk_cursor_sb_up_arrow,            XC_sb_up_arrow,         XITK_CURSOR,   },
  { xitk_cursor_sb_v_double_arrow,      XC_sb_v_double_arrow,   XITK_CURSOR,   },
  { xitk_cursor_shuttle,                XC_shuttle,             X_CURSOR,      },
  { xitk_cursor_sizing,                 XC_sizing,              X_CURSOR,      },
  { xitk_cursor_spider,                 XC_spider,              X_CURSOR,      },
  { xitk_cursor_spraycan,               XC_spraycan,            X_CURSOR,      },
  { xitk_cursor_star,                   XC_star,                X_CURSOR,      },
  { xitk_cursor_target,                 XC_target,              X_CURSOR,      },
  { xitk_cursor_tcross,                 XC_tcross,              X_CURSOR,      },
  { xitk_cursor_top_left_arrow,         XC_top_left_arrow,      X_CURSOR,      },
  { xitk_cursor_top_left_corner,        XC_top_left_corner,     XITK_CURSOR,   },
  { xitk_cursor_top_right_corner,       XC_top_right_corner,    XITK_CURSOR,   },
  { xitk_cursor_top_side,               XC_top_side,            XITK_CURSOR,   },
  { xitk_cursor_top_tee,                XC_top_tee,             X_CURSOR,      },
  { xitk_cursor_trek,                   XC_trek,                X_CURSOR,      },
  { xitk_cursor_ul_angle,               XC_ul_angle,            X_CURSOR,      },
  { xitk_cursor_umbrella,               XC_umbrella,            X_CURSOR,      },
  { xitk_cursor_ur_angle,               XC_ur_angle,            X_CURSOR,      },
  { xitk_cursor_watch,                  XC_watch,               X_CURSOR,      },
  { xitk_cursor_xterm,                  XC_xterm,               XITK_CURSOR,   },
  { xitk_cursor_num_glyphs,             XC_num_glyphs,          X_CURSOR,      },
};

/* (fg / bg / trans) normal: (* / . /  ) hotspot: (# / + / -) */

static const char * const _maps[xitk_cursor_num_glyphs] = {
  [xitk_cursor_invisible] =
    "|--------"
    "|--------"
    "|--------"
    "|--------"
    "|--------"
    "|--------"
    "|--------"
    "|--------",
  [xitk_cursor_bottom_left_corner] =
    "|       ********"
    "|       *......*"
    "|        *.....*"
    "|         *....*"
    "|        *.....*"
    "|       *...*..*"
    "|      *...* *.*"
    "|**   *...*   **"
    "|*.* *...*"
    "|*..*...*"
    "|*.....*"
    "|*....*"
    "|*.....*"
    "|*......*"
    "|#*******",
  [xitk_cursor_bottom_right_corner] =
    "|********"
    "|*......*"
    "|*.....*"
    "|*....*"
    "|*.....*"
    "|*..*...*"
    "|*.* *...*"
    "|**   *...*   **"
    "|      *...* *.*"
    "|       *...*..*"
    "|        *.....*"
    "|         *....*"
    "|        *.....*"
    "|       *......*"
    "|       *******#",
  [xitk_cursor_bottom_side] =
    "|    **"
    "|   *..*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|****..****"
    "|*........*"
    "| *......*"
    "|  *....*"
    "|   *..*"
    "|    #*",
  [xitk_cursor_center_ptr] =
    "|    **"
    "|   *+.*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   ****",
  [xitk_cursor_circle] =
    "| *"
    "| #*"
    "| *.*"
    "| *..*"
    "| *...*"
    "| *....*"
    "| *.....*"
    "| *......*"
    "| *.......*"
    "| *........*"
    "| *.....*****"
    "| *..*..*"
    "| *.* *..*"
    "| **  *..*       ...."
    "| *    *..*    ........"
    "|      *..*   ...    ..."
    "|       *..*  ....    .."
    "|       *..* .. ...    .."
    "|        *** ..  ...   .."
    "|            ..   ...  .."
    "|            ..    ... .."
    "|             ..    ...."
    "|             ...    ..."
    "|              ........"
    "|                ....",
  [xitk_cursor_cross] =
    "|          *"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|..........*........"
    "|**********#**********"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|         .*"
    "|          *",
  [xitk_cursor_dotbox] =
    "|*** ** ** ** ** ***"
    "|*                 *"
    "|*                 *"
    "|"
    "|*                 *"
    "|*       ....      *"
    "|      .....**"
    "|*     .....***    *"
    "|*    ....*****    *"
    "|     *.**#****"
    "|*    *********    *"
    "|*     ******.*    *"
    "|      ****.**"
    "|*       ***       *"
    "|*                 *"
    "|"
    "|*                 *"
    "|*                 *"
    "|*** ** ** ** ** ***",
  [xitk_cursor_double_arrow] =
    "|    **"
    "|   *..*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *+.*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|****..****"
    "|*........*"
    "| *......*"
    "|  *....*"
    "|   *..*"
    "|    **",
  [xitk_cursor_draped_box] =
    "|         ...*.."
    "|       .********"
    "|      .**********"
    "|      ***********"
    "|      ***********"
    "|      .*********."
    "|      .*   *   *."
    "|      ..   *   *."
    "|      .*.......**"
    "|       ...*....-"
    "|        ...**.."
    "|        ......."
    "|  .*    ......*    *."
    "|  **     *...**  *.."
    "|  ***     *.**  .**.*"
    "| *.***.    _   **...*"
    "|.***.***..   .**..*"
    "|      .**..***.."
    "| **    ..***..***   .*"
    "| .*******.*  ..**.****"
    "| .**.**.        ....*"
    "| .*..             .**"
    "|  ..                .",
  [xitk_cursor_fleur] =
    "|          **"
    "|         *..*"
    "|        *....*"
    "|       *......*"
    "|      *........*"
    "|      ****..****"
    "|    **   *..*   **"
    "|   *.*   *..*   *.*"
    "|  *..*   *..*   *..*"
    "| *...*****..*****...*"
    "|*.........+..........*"
    "|*....................*"
    "| *...*****..*****...*"
    "|  *..*   *..*   *..*"
    "|   *.*   *..*   *.*"
    "|    **   *..*   **"
    "|      ****..****"
    "|      *........*"
    "|       *......*"
    "|        *....*"
    "|         *..*"
    "|          **",
  [xitk_cursor_hand2] =
    "|    **"
    "|   *+.*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..***"
    "|   *..*..***"
    "|   *..*..*..**"
    "| ***..*..*..*.*"
    "|**.*..*..*..*..*"
    "|*..*........*..*"
    "|*..*...........*"
    "|*..*...........*"
    "|*..............*"
    "|*..............*"
    "|*..............*"
    "| *.............*"
    "| *............**"
    "|  *...........*"
    "|  *..........**"
    "|  ************",
  [xitk_cursor_left_ptr] =
    "|*"
    "|#*"
    "|*.*"
    "|*..*"
    "|*...*"
    "|*....*"
    "|*.....*"
    "|*......*"
    "|*.......*"
    "|*........*"
    "|*.....*****"
    "|*..*..*"
    "|*.* *..*"
    "|**  *..*"
    "|*    *..*"
    "|     *..*"
    "|      *..*"
    "|      *..*"
    "|       ***",
  [xitk_cursor_left_side] =
    "|    **       **"
    "|   *.*       *.*"
    "|  *..*       *..*"
    "| *...*********...*"
    "|*+................*"
    "|*.................*"
    "| *...*********...*"
    "|  *..*       *..*"
    "|   *.*       *.*"
    "|    **       **",
  [xitk_cursor_pencil] =
    "|#"
    "| **"
    "| ****"
    "|  **.*"
    "|  ***.*"
    "|   **..*"
    "|    **..*"
    "|     **..*"
    "|      **..*"
    "|       **..*"
    "|        **..*"
    "|         ***.*  *"
    "|          ***.* **"
    "|           ***.****"
    "|            ***.*.**"
    "|             ***.*.**"
    "|              ***.*.**"
    "|               ***.*.*"
    "|                ***.**"
    "|                 ***.*"
    "|                  ****"
    "|                   **",
  [xitk_cursor_question_arrow] =
    "|*         *******"
    "|#*       ***  ****"
    "|*.*     ***    ****"
    "|*..*    ***    ****"
    "|*...*   ***    ****"
    "|*....*  ***    ***"
    "|*.....*       ***"
    "|*......*     ***"
    "|*.......*   ***"
    "|*........*  ***"
    "|*.....***** ***"
    "|*..*..*"
    "|*.* *..*    ***"
    "|**  *..*   *****"
    "|*    *..*   ***"
    "|     *..*"
    "|      *..*"
    "|      *..*"
    "|       ***",
  [xitk_cursor_right_ptr] =
    "|          *"
    "|         *#"
    "|        *.*"
    "|       *..*"
    "|      *...*"
    "|     *....*"
    "|    *.....*"
    "|   *......*"
    "|  *.......*"
    "| *........*"
    "|*****.....*"
    "|    *..*..*"
    "|   *..* *.*"
    "|   *..*  **"
    "|  *..*    *"
    "|  *..*"
    "| *..*"
    "| *..*"
    "| ***",
  [xitk_cursor_right_side] =
    "|    **       **"
    "|   *.*       *.*"
    "|  *..*       *..*"
    "| *...*********...*"
    "|*................+*"
    "|*.................*"
    "| *...*********...*"
    "|  *..*       *..*"
    "|   *.*       *.*"
    "|    **       **",
  [xitk_cursor_sb_down_arrow] =
    "|   ****"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|****..****"
    "|*........*"
    "| *......*"
    "|  *....*"
    "|   *+.*"
    "|    **",
  [xitk_cursor_sb_h_double_arrow] =
    "|    **       **"
    "|   *.*       *.*"
    "|  *..*       *..*"
    "| *...*********...*"
    "|*........+........*"
    "|*.................*"
    "| *...*********...*"
    "|  *..*       *..*"
    "|   *.*       *.*"
    "|    **       **",
  [xitk_cursor_sb_left_arrow] =
    "|    **"
    "|   *.*"
    "|  *..*"
    "| *...************"
    "|*+..............*"
    "|*...............*"
    "| *...************"
    "|  *..*"
    "|   *.*"
    "|    **",
  [xitk_cursor_sb_right_arrow] =
    "|           **"
    "|           *.*"
    "|           *..*"
    "|************...*"
    "|*..............+*"
    "|*...............*"
    "|************...*"
    "|           *..*"
    "|           *.*"
    "|           **",
  [xitk_cursor_sb_up_arrow] =
    "|    **"
    "|   *+.*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   ****",
  [xitk_cursor_sb_v_double_arrow] =
    "|    **"
    "|   *..*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *+.*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|****..****"
    "|*........*"
    "| *......*"
    "|  *....*"
    "|   *..*"
    "|    **",
  [xitk_cursor_top_left_corner] =
    "|#*******"
    "|*......*"
    "|*.....*"
    "|*....*"
    "|*.....*"
    "|*..*...*"
    "|*.* *...*"
    "|**   *...*   **"
    "|      *...* *.*"
    "|       *...*..*"
    "|        *.....*"
    "|         *....*"
    "|        *.....*"
    "|       *......*"
    "|       ********",
  [xitk_cursor_top_right_corner] =
    "|       *******#"
    "|       *......*"
    "|        *.....*"
    "|         *....*"
    "|        *.....*"
    "|       *...*..*"
    "|      *...* *.*"
    "|**   *...*   **"
    "|*.* *...*"
    "|*..*...*"
    "|*.....*"
    "|*....*"
    "|*.....*"
    "|*......*"
    "|********",
  [xitk_cursor_top_side] =
    "|    #*"
    "|   *..*"
    "|  *....*"
    "| *......*"
    "|*........*"
    "|****..****"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|   *..*"
    "|****..****"
    "|*........*"
    "| *......*"
    "|  *....*"
    "|   *..*"
    "|    **",
  [xitk_cursor_xterm] =
    "|*******"
    "|...*.."
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .#"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|  .*"
    "|...*.."
    "|*******"
};

struct cursors_s {
  Cursor          cursor;
  Pixmap          p;
  Pixmap          mask;
};

struct xitk_x11_cursors_s {
  Display  *display;
  void    (*lock) (Display *display);
  void    (*unlock) (Display *display);
  struct cursors_s cursors[sizeof(cursors_init)/sizeof(cursors_init[0])];
};

static void _cursors_create_cursor(struct xitk_x11_cursors_s *c, int idx) {
  const char *map;
  XColor  bg, fg;
  Display *display = c->display;
  struct cursors_s *cursor = &c->cursors[idx];
  const struct cursors_init_s *cursor_init = &cursors_init[idx];

  bg.red   = 255 << 8;
  bg.green = 255 << 8;
  bg.blue  = 255 << 8;
  fg.red   = 0;
  fg.green = 0;
  fg.blue  = 0;

  c->lock(c->display);

  cursor->cursor = None;
  map = NULL;
  if (cursor_init->xitk_shape < xitk_cursor_num_glyphs)
    map = _maps[cursor_init->xitk_shape];
  if (map) {
    char img[1536], mask[1536];
    const char *p;
    uint8_t m;
    uint32_t wx = 0, w = 0, h = 0, hx = 0, hy = 0, x, y;

    for (p = map; *p; p++) {
      if (*p == '|') {
        h += 1;
        if (hx > w)
          w = hx;
        hx = wx = 0;
      } else if (*p == ' ') {
        wx += 1;
      } else {
        wx += 1;
        hx = wx;
      }
    }

    if (w && h) {
      wx = (w + 7) >> 3;
      if (wx * h > sizeof (img))
        h = sizeof (img) / wx;

      memset (img, 0, wx * h);
      memset (mask, 0, wx * h);

      m = 0x01;
      x = y = 0;
      for (p = map; *p; p++) {
        uint32_t u;

        switch (*p) {
          case '-':
            hx = x;
            hy = y;
            /* fall through */
          default:
          case ' ':
            x += 1;
            m = (m << 1) | (m >> 7);
            break;
          case '+':
            hx = x;
            hy = y;
            /* fall through */
          case '.':
            u = wx * y + (x >> 3);
            mask[u] |= m;
            x += 1;
            m = (m << 1) | (m >> 7);
            break;
          case '|':
            x = 0;
            y += 1;
            m = 0x01;
            break;
          case '#':
            hx = x;
            hy = y;
            /* fall through */
          case '*':
            u = wx * y + (x >> 3);
            mask[u] |= m;
            img[u] |= m;
            x += 1;
            m = (m << 1) | (m >> 7);
            break;
        }
        cursor->p       = XCreateBitmapFromData (display, DefaultRootWindow (display), img, w, h);
        cursor->mask    = XCreateBitmapFromData (display, DefaultRootWindow (display), mask, w, h);
        cursor->cursor  = XCreatePixmapCursor (display, cursor->p, cursor->mask, &fg, &bg, hx, hy);
      }
    }

    if (cursor->cursor == None) {
      XITK_WARNING ("cursor unhandled: %d. Fallback to X one.\n", cursor_init->xitk_shape);
      cursor->cursor = XCreateFontCursor (display, cursor_init->x_shape);
    }
  }

  c->unlock(c->display);
}

static void _no_lock (Display *display) {
  (void)display;
}

xitk_x11_cursors_t *xitk_x11_cursors_init(Display *display, int xitk_cursors, int lock) {
  int   i;
  xitk_x11_cursors_t *c;

  c = calloc(1, sizeof(*c));
  if (!c)
    return NULL;

  c->display = display;
  if (lock) {
    c->lock = XLockDisplay;
    c->unlock = XUnlockDisplay;
  } else {
    c->lock =
    c->unlock = _no_lock;
  }

  /* Transparent cursor isn't a valid X cursor */
  _cursors_create_cursor(c, 0);

  /* XXX create cursor on demand ? */

  for(i = 1; i < MAX_CURSORS; i++) {

    if(xitk_cursors) {
      if (cursors_init[i].embedded == X_CURSOR) {
        c->lock(c->display);
        c->cursors[i].cursor = XCreateFontCursor(c->display, cursors_init[i].x_shape);
        c->unlock(c->display);
      }
      else
        _cursors_create_cursor(c, i);
    }
    else {
      c->lock(c->display);
      c->cursors[i].cursor = XCreateFontCursor(c->display, cursors_init[i].x_shape);
      c->unlock(c->display);
    }
  }

  return c;
}

void xitk_x11_cursors_deinit(xitk_x11_cursors_t **p) {
  xitk_x11_cursors_t *c = *p;
  int i;

  if (!c)
    return;
  *p = NULL;

  c->lock(c->display);

  for(i = 0; i < MAX_CURSORS; i++) {
    if (c->cursors[i].cursor != None)
      XFreeCursor(c->display, c->cursors[i].cursor);
    if (c->cursors[i].p != None)
      XFreePixmap(c->display, c->cursors[i].p);
    if (c->cursors[i].mask != None)
      XFreePixmap(c->display, c->cursors[i].mask);
  }

  c->unlock(c->display);

  free(c);
}

/* Public */
void xitk_x11_cursors_define_window_cursor(xitk_x11_cursors_t *c, Window window, xitk_cursors_t cursor) {
  if (/*c &&*/ window != None && cursor >= 0 && cursor < sizeof(c->cursors)/sizeof(c->cursors[0])) {
    c->lock(c->display);
    XDefineCursor(c->display, window, c->cursors[cursor].cursor);
    XSync(c->display, False);
    c->unlock(c->display);
  }
}

void xitk_x11_cursors_restore_window_cursor(xitk_x11_cursors_t *c, Window window) {
  if (/*c && */window != None) {
    c->lock(c->display);
    XUndefineCursor(c->display, window);
    XSync(c->display, False);
    c->unlock(c->display);
  }
}
