/*
 * Copyright (C) 2004-2021 the xine project
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

static const struct cursors_init_s {
  const char *name; /** << name[0] == '_' means xitk embedded, plain x otherwise. */
  unsigned int x_shape;
} cursors_init[xitk_cursor_num_glyphs] = {
  [xitk_cursor_invisible]           = { "_invisible",           0                      },
  [xitk_cursor_X_cursor]            = {  "X_cursor",            XC_X_cursor            },
  [xitk_cursor_arrow]               = {  "arrow",               XC_arrow               },
  [xitk_cursor_based_arrow_down]    = {  "based_arrow_down",    XC_based_arrow_down    },
  [xitk_cursor_based_arrow_up]      = {  "based_arrow_up",      XC_based_arrow_up      },
  [xitk_cursor_boat]                = {  "boat",                XC_boat                },
  [xitk_cursor_bogosity]            = {  "bogosity",            XC_bogosity            },
  [xitk_cursor_bottom_left_corner]  = { "_bottom_left_corner",  XC_bottom_left_corner  },
  [xitk_cursor_bottom_right_corner] = { "_bottom_right_corner", XC_bottom_right_corner },
  [xitk_cursor_bottom_side]         = { "_bottom_side",         XC_bottom_side         },
  [xitk_cursor_bottom_tee]          = {  "bottom_tee",          XC_bottom_tee          },
  [xitk_cursor_box_spiral]          = {  "box_spiral",          XC_box_spiral          },
  [xitk_cursor_center_ptr]          = { "_center_ptr",          XC_center_ptr          },
  [xitk_cursor_circle]              = { "_circle",              XC_circle              },
  [xitk_cursor_clock]               = {  "clock",               XC_clock               },
  [xitk_cursor_coffee_mug]          = {  "coffee_mug",          XC_coffee_mug          },
  [xitk_cursor_cross]               = { "_cross",               XC_cross               },
  [xitk_cursor_cross_reverse]       = {  "cross_reverse",       XC_cross_reverse       },
  [xitk_cursor_crosshair]           = {  "crosshair",           XC_crosshair           },
  [xitk_cursor_diamond_cross]       = {  "diamond_cross",       XC_diamond_cross       },
  [xitk_cursor_dot]                 = {  "dot",                 XC_dot                 },
  [xitk_cursor_dotbox]              = { "_dotbox",              XC_dotbox              },
  [xitk_cursor_double_arrow]        = { "_double_arrow",        XC_double_arrow        },
  [xitk_cursor_draft_large]         = {  "draft_large",         XC_draft_large         },
  [xitk_cursor_draft_small]         = {  "draft_small",         XC_draft_small         },
  [xitk_cursor_draped_box]          = { "_draped_box",          XC_draped_box          },
  [xitk_cursor_exchange]            = {  "exchange",            XC_exchange            },
  [xitk_cursor_fleur]               = { "_fleur",               XC_fleur               },
  [xitk_cursor_gobbler]             = {  "gobbler",             XC_gobbler             },
  [xitk_cursor_gumby]               = {  "gumby",               XC_gumby               },
  [xitk_cursor_hand1]               = {  "hand1",               XC_hand1               },
  [xitk_cursor_hand2]               = { "_hand2",               XC_hand2               },
  [xitk_cursor_heart]               = {  "heart",               XC_heart               },
  [xitk_cursor_icon]                = {  "icon",                XC_icon                },
  [xitk_cursor_iron_cross]          = {  "iron_cross",          XC_iron_cross          },
  [xitk_cursor_left_ptr]            = { "_left_ptr",            XC_left_ptr            },
  [xitk_cursor_left_side]           = { "_left_side",           XC_left_side           },
  [xitk_cursor_left_tee]            = {  "left_tee",            XC_left_tee            },
  [xitk_cursor_leftbutton]          = {  "leftbutton",          XC_leftbutton          },
  [xitk_cursor_ll_angle]            = {  "ll_angle",            XC_ll_angle            },
  [xitk_cursor_lr_angle]            = {  "lr_angle",            XC_lr_angle            },
  [xitk_cursor_man]                 = {  "man",                 XC_man                 },
  [xitk_cursor_middlebutton]        = {  "middlebutton",        XC_middlebutton        },
  [xitk_cursor_mouse]               = {  "mouse",               XC_mouse               },
  [xitk_cursor_pencil]              = { "_pencil",              XC_pencil              },
  [xitk_cursor_pirate]              = {  "pirate",              XC_pirate              },
  [xitk_cursor_plus]                = {  "plus",                XC_plus                },
  [xitk_cursor_question_arrow]      = { "_question_arrow",      XC_question_arrow      },
  [xitk_cursor_right_ptr]           = { "_right_ptr",           XC_right_ptr           },
  [xitk_cursor_right_side]          = { "_right_side",          XC_right_side          },
  [xitk_cursor_right_tee]           = {  "right_tee",           XC_right_tee           },
  [xitk_cursor_rightbutton]         = {  "rightbutton",         XC_rightbutton         },
  [xitk_cursor_rtl_logo]            = {  "rtl_logo",            XC_rtl_logo            },
  [xitk_cursor_sailboat]            = {  "sailboat",            XC_sailboat            },
  [xitk_cursor_sb_down_arrow]       = { "_sb_down_arrow",       XC_sb_down_arrow       },
  [xitk_cursor_sb_h_double_arrow]   = { "_sb_h_double_arrow",   XC_sb_h_double_arrow   },
  [xitk_cursor_sb_left_arrow]       = { "_sb_left_arrow",       XC_sb_left_arrow       },
  [xitk_cursor_sb_right_arrow]      = { "_sb_right_arrow",      XC_sb_right_arrow      },
  [xitk_cursor_sb_up_arrow]         = { "_sb_up_arrow",         XC_sb_up_arrow         },
  [xitk_cursor_sb_v_double_arrow]   = { "_sb_v_double_arrow",   XC_sb_v_double_arrow   },
  [xitk_cursor_shuttle]             = {  "shuttle",             XC_shuttle             },
  [xitk_cursor_sizing]              = {  "sizing",              XC_sizing              },
  [xitk_cursor_spider]              = {  "spider",              XC_spider              },
  [xitk_cursor_spraycan]            = {  "spraycan",            XC_spraycan            },
  [xitk_cursor_star]                = {  "star",                XC_star                },
  [xitk_cursor_target]              = {  "target",              XC_target              },
  [xitk_cursor_tcross]              = {  "tcross",              XC_tcross              },
  [xitk_cursor_top_left_arrow]      = {  "top_left_arrow",      XC_top_left_arrow      },
  [xitk_cursor_top_left_corner]     = { "_top_left_corner",     XC_top_left_corner     },
  [xitk_cursor_top_right_corner]    = { "_top_right_corner",    XC_top_right_corner    },
  [xitk_cursor_top_side]            = { "_top_side",            XC_top_side            },
  [xitk_cursor_top_tee]             = {  "top_tee",             XC_top_tee             },
  [xitk_cursor_trek]                = {  "trek",                XC_trek                },
  [xitk_cursor_ul_angle]            = {  "ul_angle",            XC_ul_angle            },
  [xitk_cursor_umbrella]            = {  "umbrella",            XC_umbrella            },
  [xitk_cursor_ur_angle]            = {  "ur_angle",            XC_ur_angle            },
  [xitk_cursor_watch]               = {  "watch",               XC_watch               },
  [xitk_cursor_xterm]               = { "_xterm",               XC_xterm               }
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
  Cursor cursor;
  Pixmap p;
  Pixmap mask;
};

struct xitk_x11_cursors_s {
  int verbosity;
  int xitk_cursors;
  Display  *display;
  void    (*lock) (Display *display);
  void    (*unlock) (Display *display);
  struct cursors_s cursors[xitk_cursor_num_glyphs];
};

static void _cursors_create_cursor(struct xitk_x11_cursors_s *c, xitk_cursors_t idx) {
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
  map = _maps[idx];
  {
    char img[1536], mask[1536];
    const char *p;
    uint8_t m;
    uint32_t wx = 0, w = 0, h = 0, hx = 0, hy = 0;

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
    if (hx > w)
      w = hx;

    if (w && h) {
      int32_t x, y;

      wx = (w + 7) >> 3;
      if (wx * h > sizeof (img))
        h = sizeof (img) / wx;

      memset (img, 0, wx * h);
      memset (mask, 0, wx * h);

      m = 0x01;
      x = 0;
      y = -1;
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
      }
      cursor->p       = XCreateBitmapFromData (display, DefaultRootWindow (display), img, w, h);
      cursor->mask    = XCreateBitmapFromData (display, DefaultRootWindow (display), mask, w, h);
      cursor->cursor  = XCreatePixmapCursor (display, cursor->p, cursor->mask, &fg, &bg, hx, hy);
    }

    if (cursor->cursor == None) {
      XITK_WARNING ("cursor unhandled: %d. Fallback to X one.\n", idx);
      cursor->cursor = XCreateFontCursor (display, cursor_init->x_shape);
    }
  }

  c->unlock(c->display);
}

static void _no_lock (Display *display) {
  (void)display;
}

xitk_x11_cursors_t *xitk_x11_cursors_init (Display *display, int xitk_cursors, int lock, int verbosity) {
  int   i;
  xitk_x11_cursors_t *c;

  c = calloc(1, sizeof(*c));
  if (!c)
    return NULL;

  c->verbosity = verbosity;

  c->xitk_cursors = xitk_cursors;

  c->display = display;
  if (lock) {
    c->lock = XLockDisplay;
    c->unlock = XUnlockDisplay;
  } else {
    c->lock =
    c->unlock = _no_lock;
  }

  for (i = 1; i < xitk_cursor_num_glyphs; i++) {
    c->cursors[i].p = None;
    c->cursors[i].mask = None;
    c->cursors[i].cursor = None;
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

  for(i = 0; i < xitk_cursor_num_glyphs; i++) {
    if (c->cursors[i].cursor != None) {
      if (c->verbosity >= 2)
        printf ("xitk.x11.cursor.delete (%s).\n", cursors_init[i].name);
      XFreeCursor(c->display, c->cursors[i].cursor);
    }
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
  if (c && (window != None) && (cursor < xitk_cursor_num_glyphs)) {
    c->lock (c->display);
    if (c->cursors[cursor].cursor == None) {
      if (c->verbosity >= 2)
        printf ("xitk.x11.cursor.new (%s).\n", cursors_init[cursor].name);
      /* if both the x and xitk versions exitst, follow user pref. */
      if ((c->xitk_cursors && (cursors_init[cursor].name[0] == '_')) || (cursors_init[cursor].x_shape == 0)) {
        _cursors_create_cursor (c, cursor);
      } else {
        c->cursors[cursor].cursor = XCreateFontCursor (c->display, cursors_init[cursor].x_shape);
      }
    }
    if (c->cursors[cursor].cursor != None) {
      XDefineCursor (c->display, window, c->cursors[cursor].cursor);
      XSync (c->display, False);
    }
    c->unlock (c->display);
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
