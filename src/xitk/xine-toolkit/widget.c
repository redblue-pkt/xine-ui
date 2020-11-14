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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include <xine/sorted_array.h>

#include "_xitk.h"
#include "tips.h"
#include "menu.h"
#include "combo.h"
#include "checkbox.h"
#include "xitk_x11.h"

static const xitk_color_names_t xitk_sorted_color_names[] = {
  { 240,  248,  255,  "aliceblue" },
  { 250,  235,  215,  "antiquewhite" },
  { 255,  239,  219,  "antiquewhite1" },
  { 238,  223,  204,  "antiquewhite2" },
  { 205,  192,  176,  "antiquewhite3" },
  { 139,  131,  120,  "antiquewhite4" },
  { 127,  255,  212,  "aquamarine" },
  { 127,  255,  212,  "aquamarine1" },
  { 118,  238,  198,  "aquamarine2" },
  { 102,  205,  170,  "aquamarine3" },
  {  69,  139,  116,  "aquamarine4" },
  { 240,  255,  255,  "azure" },
  { 240,  255,  255,  "azure1" },
  { 224,  238,  238,  "azure2" },
  { 193,  205,  205,  "azure3" },
  { 131,  139,  139,  "azure4" },
  { 245,  245,  220,  "beige" },
  { 255,  228,  196,  "bisque" },
  { 255,  228,  196,  "bisque1" },
  { 238,  213,  183,  "bisque2" },
  { 205,  183,  158,  "bisque3" },
  { 139,  125,  107,  "bisque4" },
  {   0,    0,    0,  "black" },
  { 255,  235,  205,  "blanchedalmond" },
  {   0,    0,  255,  "blue" },
  {   0,    0,  255,  "blue1" },
  {   0,    0,  238,  "blue2" },
  {   0,    0,  205,  "blue3" },
  {   0,    0,  139,  "blue4" },
  { 138,   43,  226,  "blueviolet" },
  { 165,   42,   42,  "brown" },
  { 255,   64,   64,  "brown1" },
  { 238,   59,   59,  "brown2" },
  { 205,   51,   51,  "brown3" },
  { 139,   35,   35,  "brown4" },
  { 222,  184,  135,  "burlywood" },
  { 255,  211,  155,  "burlywood1" },
  { 238,  197,  145,  "burlywood2" },
  { 205,  170,  125,  "burlywood3" },
  { 139,  115,   85,  "burlywood4" },
  {  95,  158,  160,  "cadetblue" },
  { 152,  245,  255,  "cadetblue1" },
  { 142,  229,  238,  "cadetblue2" },
  { 122,  197,  205,  "cadetblue3" },
  {  83,  134,  139,  "cadetblue4" },
  { 127,  255,    0,  "chartreuse" },
  { 127,  255,    0,  "chartreuse1" },
  { 118,  238,    0,  "chartreuse2" },
  { 102,  205,    0,  "chartreuse3" },
  {  69,  139,    0,  "chartreuse4" },
  { 210,  105,   30,  "chocolate" },
  { 255,  127,   36,  "chocolate1" },
  { 238,  118,   33,  "chocolate2" },
  { 205,  102,   29,  "chocolate3" },
  { 139,   69,   19,  "chocolate4" },
  { 255,  127,   80,  "coral" },
  { 255,  114,   86,  "coral1" },
  { 238,  106,   80,  "coral2" },
  { 205,   91,   69,  "coral3" },
  { 139,   62,   47,  "coral4" },
  { 100,  149,  237,  "cornflowerblue" },
  { 255,  248,  220,  "cornsilk" },
  { 255,  248,  220,  "cornsilk1" },
  { 238,  232,  205,  "cornsilk2" },
  { 205,  200,  177,  "cornsilk3" },
  { 139,  136,  120,  "cornsilk4" },
  {   0,  255,  255,  "cyan" },
  {   0,  255,  255,  "cyan1" },
  {   0,  238,  238,  "cyan2" },
  {   0,  205,  205,  "cyan3" },
  {   0,  139,  139,  "cyan4" },
  {   0,    0,  139,  "darkblue" },
  {   0,  139,  139,  "darkcyan" },
  { 184,  134,   11,  "darkgoldenrod" },
  { 255,  185,   15,  "darkgoldenrod1" },
  { 238,  173,   14,  "darkgoldenrod2" },
  { 205,  149,   12,  "darkgoldenrod3" },
  { 139,  101,    8,  "darkgoldenrod4" },
  { 169,  169,  169,  "darkgray" },
  {   0,  100,    0,  "darkgreen" },
  { 169,  169,  169,  "darkgrey" },
  { 189,  183,  107,  "darkkhaki" },
  { 139,    0,  139,  "darkmagenta" },
  {  85,  107,   47,  "darkolivegreen" },
  { 202,  255,  112,  "darkolivegreen1" },
  { 188,  238,  104,  "darkolivegreen2" },
  { 162,  205,   90,  "darkolivegreen3" },
  { 110,  139,   61,  "darkolivegreen4" },
  { 255,  140,    0,  "darkorange" },
  { 255,  127,    0,  "darkorange1" },
  { 238,  118,    0,  "darkorange2" },
  { 205,  102,    0,  "darkorange3" },
  { 139,   69,    0,  "darkorange4" },
  { 153,   50,  204,  "darkorchid" },
  { 191,   62,  255,  "darkorchid1" },
  { 178,   58,  238,  "darkorchid2" },
  { 154,   50,  205,  "darkorchid3" },
  { 104,   34,  139,  "darkorchid4" },
  { 139,    0,    0,  "darkred" },
  { 233,  150,  122,  "darksalmon" },
  { 143,  188,  143,  "darkseagreen" },
  { 193,  255,  193,  "darkseagreen1" },
  { 180,  238,  180,  "darkseagreen2" },
  { 155,  205,  155,  "darkseagreen3" },
  { 105,  139,  105,  "darkseagreen4" },
  {  72,   61,  139,  "darkslateblue" },
  { 151,  255,  255,  "darkslategray1" },
  { 141,  238,  238,  "darkslategray2" },
  { 121,  205,  205,  "darkslategray3" },
  {  82,  139,  139,  "darkslategray4" },
  {  47,   79,   79,  "darkslategrey" },
  {   0,  206,  209,  "darkturquoise" },
  { 148,    0,  211,  "darkviolet" },
  { 255,   20,  147,  "deeppink" },
  { 255,   20,  147,  "deeppink1" },
  { 238,   18,  137,  "deeppink2" },
  { 205,   16,  118,  "deeppink3" },
  { 139,   10,   80,  "deeppink4" },
  {   0,  191,  255,  "deepskyblue" },
  {   0,  191,  255,  "deepskyblue1" },
  {   0,  178,  238,  "deepskyblue2" },
  {   0,  154,  205,  "deepskyblue3" },
  {   0,  104,  139,  "deepskyblue4" },
  { 105,  105,  105,  "dimgray" },
  { 105,  105,  105,  "dimgrey" },
  {  30,  144,  255,  "dodgerblue" },
  {  30,  144,  255,  "dodgerblue1" },
  {  28,  134,  238,  "dodgerblue2" },
  {  24,  116,  205,  "dodgerblue3" },
  {  16,   78,  139,  "dodgerblue4" },
  { 178,   34,   34,  "firebrick" },
  { 255,   48,   48,  "firebrick1" },
  { 238,   44,   44,  "firebrick2" },
  { 205,   38,   38,  "firebrick3" },
  { 139,   26,   26,  "firebrick4" },
  { 255,  250,  240,  "floralwhite" },
  {  34,  139,   34,  "forestgreen" },
  { 220,  220,  220,  "gainsboro" },
  { 248,  248,  255,  "ghostwhite" },
  { 255,  215,    0,  "gold" },
  { 255,  215,    0,  "gold1" },
  { 238,  201,    0,  "gold2" },
  { 205,  173,    0,  "gold3" },
  { 139,  117,    0,  "gold4" },
  { 218,  165,   32,  "goldenrod" },
  { 255,  193,   37,  "goldenrod1" },
  { 238,  180,   34,  "goldenrod2" },
  { 205,  155,   29,  "goldenrod3" },
  { 139,  105,   20,  "goldenrod4" },
  { 190,  190,  190,  "gray" },
  {   0,    0,    0,  "gray0" },
  {   3,    3,    3,  "gray1" },
  {  26,   26,   26,  "gray10" },
  { 255,  255,  255,  "gray100" },
  {  28,   28,   28,  "gray11" },
  {  31,   31,   31,  "gray12" },
  {  33,   33,   33,  "gray13" },
  {  36,   36,   36,  "gray14" },
  {  38,   38,   38,  "gray15" },
  {  41,   41,   41,  "gray16" },
  {  43,   43,   43,  "gray17" },
  {  46,   46,   46,  "gray18" },
  {  48,   48,   48,  "gray19" },
  {   5,    5,    5,  "gray2" },
  {  51,   51,   51,  "gray20" },
  {  54,   54,   54,  "gray21" },
  {  56,   56,   56,  "gray22" },
  {  59,   59,   59,  "gray23" },
  {  61,   61,   61,  "gray24" },
  {  64,   64,   64,  "gray25" },
  {  66,   66,   66,  "gray26" },
  {  69,   69,   69,  "gray27" },
  {  71,   71,   71,  "gray28" },
  {  74,   74,   74,  "gray29" },
  {   8,    8,    8,  "gray3" },
  {  77,   77,   77,  "gray30" },
  {  79,   79,   79,  "gray31" },
  {  82,   82,   82,  "gray32" },
  {  84,   84,   84,  "gray33" },
  {  87,   87,   87,  "gray34" },
  {  89,   89,   89,  "gray35" },
  {  92,   92,   92,  "gray36" },
  {  94,   94,   94,  "gray37" },
  {  97,   97,   97,  "gray38" },
  {  99,   99,   99,  "gray39" },
  {  10,   10,   10,  "gray4" },
  { 102,  102,  102,  "gray40" },
  { 105,  105,  105,  "gray41" },
  { 107,  107,  107,  "gray42" },
  { 110,  110,  110,  "gray43" },
  { 112,  112,  112,  "gray44" },
  { 115,  115,  115,  "gray45" },
  { 117,  117,  117,  "gray46" },
  { 120,  120,  120,  "gray47" },
  { 122,  122,  122,  "gray48" },
  { 125,  125,  125,  "gray49" },
  {  13,   13,   13,  "gray5" },
  { 127,  127,  127,  "gray50" },
  { 130,  130,  130,  "gray51" },
  { 133,  133,  133,  "gray52" },
  { 135,  135,  135,  "gray53" },
  { 138,  138,  138,  "gray54" },
  { 140,  140,  140,  "gray55" },
  { 143,  143,  143,  "gray56" },
  { 145,  145,  145,  "gray57" },
  { 148,  148,  148,  "gray58" },
  { 150,  150,  150,  "gray59" },
  {  15,   15,   15,  "gray6" },
  { 153,  153,  153,  "gray60" },
  { 156,  156,  156,  "gray61" },
  { 158,  158,  158,  "gray62" },
  { 161,  161,  161,  "gray63" },
  { 163,  163,  163,  "gray64" },
  { 166,  166,  166,  "gray65" },
  { 168,  168,  168,  "gray66" },
  { 171,  171,  171,  "gray67" },
  { 173,  173,  173,  "gray68" },
  { 176,  176,  176,  "gray69" },
  {  18,   18,   18,  "gray7" },
  { 179,  179,  179,  "gray70" },
  { 181,  181,  181,  "gray71" },
  { 184,  184,  184,  "gray72" },
  { 186,  186,  186,  "gray73" },
  { 189,  189,  189,  "gray74" },
  { 191,  191,  191,  "gray75" },
  { 194,  194,  194,  "gray76" },
  { 196,  196,  196,  "gray77" },
  { 199,  199,  199,  "gray78" },
  { 201,  201,  201,  "gray79" },
  {  20,   20,   20,  "gray8" },
  { 204,  204,  204,  "gray80" },
  { 207,  207,  207,  "gray81" },
  { 209,  209,  209,  "gray82" },
  { 212,  212,  212,  "gray83" },
  { 214,  214,  214,  "gray84" },
  { 217,  217,  217,  "gray85" },
  { 219,  219,  219,  "gray86" },
  { 222,  222,  222,  "gray87" },
  { 224,  224,  224,  "gray88" },
  { 227,  227,  227,  "gray89" },
  {  23,   23,   23,  "gray9" },
  { 229,  229,  229,  "gray90" },
  { 232,  232,  232,  "gray91" },
  { 235,  235,  235,  "gray92" },
  { 237,  237,  237,  "gray93" },
  { 240,  240,  240,  "gray94" },
  { 242,  242,  242,  "gray95" },
  { 245,  245,  245,  "gray96" },
  { 247,  247,  247,  "gray97" },
  { 250,  250,  250,  "gray98" },
  { 252,  252,  252,  "gray99" },
  {   0,  255,    0,  "green1" },
  {   0,  238,    0,  "green2" },
  {   0,  205,    0,  "green3" },
  {   0,  139,    0,  "green4" },
  { 173,  255,   47,  "greenyellow" },
  { 190,  190,  190,  "grey" },
  {   0,    0,    0,  "grey0" },
  {   3,    3,    3,  "grey1" },
  {  26,   26,   26,  "grey10" },
  { 255,  255,  255,  "grey100" },
  {  28,   28,   28,  "grey11" },
  {  31,   31,   31,  "grey12" },
  {  33,   33,   33,  "grey13" },
  {  36,   36,   36,  "grey14" },
  {  38,   38,   38,  "grey15" },
  {  41,   41,   41,  "grey16" },
  {  43,   43,   43,  "grey17" },
  {  46,   46,   46,  "grey18" },
  {  48,   48,   48,  "grey19" },
  {   5,    5,    5,  "grey2" },
  {  51,   51,   51,  "grey20" },
  {  54,   54,   54,  "grey21" },
  {  56,   56,   56,  "grey22" },
  {  59,   59,   59,  "grey23" },
  {  61,   61,   61,  "grey24" },
  {  64,   64,   64,  "grey25" },
  {  66,   66,   66,  "grey26" },
  {  69,   69,   69,  "grey27" },
  {  71,   71,   71,  "grey28" },
  {  74,   74,   74,  "grey29" },
  {   8,    8,    8,  "grey3" },
  {  77,   77,   77,  "grey30" },
  {  79,   79,   79,  "grey31" },
  {  82,   82,   82,  "grey32" },
  {  84,   84,   84,  "grey33" },
  {  87,   87,   87,  "grey34" },
  {  89,   89,   89,  "grey35" },
  {  92,   92,   92,  "grey36" },
  {  94,   94,   94,  "grey37" },
  {  97,   97,   97,  "grey38" },
  {  99,   99,   99,  "grey39" },
  {  10,   10,   10,  "grey4" },
  { 102,  102,  102,  "grey40" },
  { 105,  105,  105,  "grey41" },
  { 107,  107,  107,  "grey42" },
  { 110,  110,  110,  "grey43" },
  { 112,  112,  112,  "grey44" },
  { 115,  115,  115,  "grey45" },
  { 117,  117,  117,  "grey46" },
  { 120,  120,  120,  "grey47" },
  { 122,  122,  122,  "grey48" },
  { 125,  125,  125,  "grey49" },
  {  13,   13,   13,  "grey5" },
  { 127,  127,  127,  "grey50" },
  { 130,  130,  130,  "grey51" },
  { 133,  133,  133,  "grey52" },
  { 135,  135,  135,  "grey53" },
  { 138,  138,  138,  "grey54" },
  { 140,  140,  140,  "grey55" },
  { 143,  143,  143,  "grey56" },
  { 145,  145,  145,  "grey57" },
  { 148,  148,  148,  "grey58" },
  { 150,  150,  150,  "grey59" },
  {  15,   15,   15,  "grey6" },
  { 153,  153,  153,  "grey60" },
  { 156,  156,  156,  "grey61" },
  { 158,  158,  158,  "grey62" },
  { 161,  161,  161,  "grey63" },
  { 163,  163,  163,  "grey64" },
  { 166,  166,  166,  "grey65" },
  { 168,  168,  168,  "grey66" },
  { 171,  171,  171,  "grey67" },
  { 173,  173,  173,  "grey68" },
  { 176,  176,  176,  "grey69" },
  {  18,   18,   18,  "grey7" },
  { 179,  179,  179,  "grey70" },
  { 181,  181,  181,  "grey71" },
  { 184,  184,  184,  "grey72" },
  { 186,  186,  186,  "grey73" },
  { 189,  189,  189,  "grey74" },
  { 191,  191,  191,  "grey75" },
  { 194,  194,  194,  "grey76" },
  { 196,  196,  196,  "grey77" },
  { 199,  199,  199,  "grey78" },
  { 201,  201,  201,  "grey79" },
  {  20,   20,   20,  "grey8" },
  { 204,  204,  204,  "grey80" },
  { 207,  207,  207,  "grey81" },
  { 209,  209,  209,  "grey82" },
  { 212,  212,  212,  "grey83" },
  { 214,  214,  214,  "grey84" },
  { 217,  217,  217,  "grey85" },
  { 219,  219,  219,  "grey86" },
  { 222,  222,  222,  "grey87" },
  { 224,  224,  224,  "grey88" },
  { 227,  227,  227,  "grey89" },
  {  23,   23,   23,  "grey9" },
  { 229,  229,  229,  "grey90" },
  { 232,  232,  232,  "grey91" },
  { 235,  235,  235,  "grey92" },
  { 237,  237,  237,  "grey93" },
  { 240,  240,  240,  "grey94" },
  { 242,  242,  242,  "grey95" },
  { 245,  245,  245,  "grey96" },
  { 247,  247,  247,  "grey97" },
  { 250,  250,  250,  "grey98" },
  { 252,  252,  252,  "grey99" },
  { 240,  255,  240,  "honeydew" },
  { 240,  255,  240,  "honeydew1" },
  { 224,  238,  224,  "honeydew2" },
  { 193,  205,  193,  "honeydew3" },
  { 131,  139,  131,  "honeydew4" },
  { 255,  105,  180,  "hotpink" },
  { 255,  110,  180,  "hotpink1" },
  { 238,  106,  167,  "hotpink2" },
  { 205,   96,  144,  "hotpink3" },
  { 139,   58,   98,  "hotpink4" },
  { 205,   92,   92,  "indianred" },
  { 255,  106,  106,  "indianred1" },
  { 238,   99,   99,  "indianred2" },
  { 205,   85,   85,  "indianred3" },
  { 139,   58,   58,  "indianred4" },
  { 255,  255,  240,  "ivory" },
  { 255,  255,  240,  "ivory1" },
  { 238,  238,  224,  "ivory2" },
  { 205,  205,  193,  "ivory3" },
  { 139,  139,  131,  "ivory4" },
  { 240,  230,  140,  "khaki" },
  { 255,  246,  143,  "khaki1" },
  { 238,  230,  133,  "khaki2" },
  { 205,  198,  115,  "khaki3" },
  { 139,  134,   78,  "khaki4" },
  { 230,  230,  250,  "lavender" },
  { 255,  240,  245,  "lavenderblush" },
  { 255,  240,  245,  "lavenderblush1" },
  { 238,  224,  229,  "lavenderblush2" },
  { 205,  193,  197,  "lavenderblush3" },
  { 139,  131,  134,  "lavenderblush4" },
  { 124,  252,    0,  "lawngreen" },
  { 255,  250,  205,  "lemonchiffon" },
  { 255,  250,  205,  "lemonchiffon1" },
  { 238,  233,  191,  "lemonchiffon2" },
  { 205,  201,  165,  "lemonchiffon3" },
  { 139,  137,  112,  "lemonchiffon4" },
  { 173,  216,  230,  "lightblue" },
  { 191,  239,  255,  "lightblue1" },
  { 178,  223,  238,  "lightblue2" },
  { 154,  192,  205,  "lightblue3" },
  { 104,  131,  139,  "lightblue4" },
  { 240,  128,  128,  "lightcoral" },
  { 224,  255,  255,  "lightcyan" },
  { 224,  255,  255,  "lightcyan1" },
  { 209,  238,  238,  "lightcyan2" },
  { 180,  205,  205,  "lightcyan3" },
  { 122,  139,  139,  "lightcyan4" },
  { 238,  221,  130,  "lightgoldenrod" },
  { 255,  236,  139,  "lightgoldenrod1" },
  { 238,  220,  130,  "lightgoldenrod2" },
  { 205,  190,  112,  "lightgoldenrod3" },
  { 139,  129,   76,  "lightgoldenrod4" },
  { 250,  250,  210,  "lightgoldenrodyellow" },
  { 211,  211,  211,  "lightgray" },
  { 144,  238,  144,  "lightgreen" },
  { 211,  211,  211,  "lightgrey" },
  { 255,  182,  193,  "lightpink" },
  { 255,  174,  185,  "lightpink1" },
  { 238,  162,  173,  "lightpink2" },
  { 205,  140,  149,  "lightpink3" },
  { 139,   95,  101,  "lightpink4" },
  { 255,  160,  122,  "lightsalmon" },
  { 255,  160,  122,  "lightsalmon1" },
  { 238,  149,  114,  "lightsalmon2" },
  { 205,  129,   98,  "lightsalmon3" },
  { 139,   87,   66,  "lightsalmon4" },
  {  32,  178,  170,  "lightseagreen" },
  { 135,  206,  250,  "lightskyblue" },
  { 176,  226,  255,  "lightskyblue1" },
  { 164,  211,  238,  "lightskyblue2" },
  { 141,  182,  205,  "lightskyblue3" },
  {  96,  123,  139,  "lightskyblue4" },
  { 132,  112,  255,  "lightslateblue" },
  { 119,  136,  153,  "lightslategray" },
  { 119,  136,  153,  "lightslategrey" },
  { 176,  196,  222,  "lightsteelblue" },
  { 202,  225,  255,  "lightsteelblue1" },
  { 188,  210,  238,  "lightsteelblue2" },
  { 162,  181,  205,  "lightsteelblue3" },
  { 110,  123,  139,  "lightsteelblue4" },
  { 255,  255,  224,  "lightyellow" },
  { 255,  255,  224,  "lightyellow1" },
  { 238,  238,  209,  "lightyellow2" },
  { 205,  205,  180,  "lightyellow3" },
  { 139,  139,  122,  "lightyellow4" },
  {  50,  205,   50,  "limegreen" },
  { 250,  240,  230,  "linen" },
  { 255,    0,  255,  "magenta" },
  { 255,    0,  255,  "magenta1" },
  { 238,    0,  238,  "magenta2" },
  { 205,    0,  205,  "magenta3" },
  { 139,    0,  139,  "magenta4" },
  { 176,   48,   96,  "maroon" },
  { 255,   52,  179,  "maroon1" },
  { 238,   48,  167,  "maroon2" },
  { 205,   41,  144,  "maroon3" },
  { 139,   28,   98,  "maroon4" },
  { 102,  205,  170,  "mediumaquamarine" },
  {   0,    0,  205,  "mediumblue" },
  { 186,   85,  211,  "mediumorchid" },
  { 224,  102,  255,  "mediumorchid1" },
  { 209,   95,  238,  "mediumorchid2" },
  { 180,   82,  205,  "mediumorchid3" },
  { 122,   55,  139,  "mediumorchid4" },
  { 147,  112,  219,  "mediumpurple" },
  { 171,  130,  255,  "mediumpurple1" },
  { 159,  121,  238,  "mediumpurple2" },
  { 137,  104,  205,  "mediumpurple3" },
  {  93,   71,  139,  "mediumpurple4" },
  {  60,  179,  113,  "mediumseagreen" },
  { 123,  104,  238,  "mediumslateblue" },
  {   0,  250,  154,  "mediumspringgreen" },
  {  72,  209,  204,  "mediumturquoise" },
  { 199,   21,  133,  "mediumvioletred" },
  {  25,   25,  112,  "midnightblue" },
  { 245,  255,  250,  "mintcream" },
  { 255,  228,  225,  "mistyrose" },
  { 255,  228,  225,  "mistyrose1" },
  { 238,  213,  210,  "mistyrose2" },
  { 205,  183,  181,  "mistyrose3" },
  { 139,  125,  123,  "mistyrose4" },
  { 255,  228,  181,  "moccasin" },
  { 255,  222,  173,  "navajowhite" },
  { 255,  222,  173,  "navajowhite1" },
  { 238,  207,  161,  "navajowhite2" },
  { 205,  179,  139,  "navajowhite3" },
  { 139,  121,   94,  "navajowhite4" },
  {   0,    0,  128,  "navy" },
  {   0,    0,  128,  "navyblue" },
  { 253,  245,  230,  "oldlace" },
  { 107,  142,   35,  "olivedrab" },
  { 192,  255,   62,  "olivedrab1" },
  { 179,  238,   58,  "olivedrab2" },
  { 154,  205,   50,  "olivedrab3" },
  { 105,  139,   34,  "olivedrab4" },
  { 255,  165,    0,  "orange" },
  { 255,  165,    0,  "orange1" },
  { 238,  154,    0,  "orange2" },
  { 205,  133,    0,  "orange3" },
  { 139,   90,    0,  "orange4" },
  { 255,   69,    0,  "orangered" },
  { 255,   69,    0,  "orangered1" },
  { 238,   64,    0,  "orangered2" },
  { 205,   55,    0,  "orangered3" },
  { 139,   37,    0,  "orangered4" },
  { 218,  112,  214,  "orchid" },
  { 255,  131,  250,  "orchid1" },
  { 238,  122,  233,  "orchid2" },
  { 205,  105,  201,  "orchid3" },
  { 139,   71,  137,  "orchid4" },
  { 238,  232,  170,  "palegoldenrod" },
  { 152,  251,  152,  "palegreen" },
  { 154,  255,  154,  "palegreen1" },
  { 144,  238,  144,  "palegreen2" },
  { 124,  205,  124,  "palegreen3" },
  {  84,  139,   84,  "palegreen4" },
  { 175,  238,  238,  "paleturquoise" },
  { 187,  255,  255,  "paleturquoise1" },
  { 174,  238,  238,  "paleturquoise2" },
  { 150,  205,  205,  "paleturquoise3" },
  { 102,  139,  139,  "paleturquoise4" },
  { 219,  112,  147,  "palevioletred" },
  { 255,  130,  171,  "palevioletred1" },
  { 238,  121,  159,  "palevioletred2" },
  { 205,  104,  137,  "palevioletred3" },
  { 139,   71,   93,  "palevioletred4" },
  { 255,  239,  213,  "papayawhip" },
  { 255,  218,  185,  "peachpuff" },
  { 255,  218,  185,  "peachpuff1" },
  { 238,  203,  173,  "peachpuff2" },
  { 205,  175,  149,  "peachpuff3" },
  { 139,  119,  101,  "peachpuff4" },
  { 205,  133,   63,  "peru" },
  { 255,  192,  203,  "pink" },
  { 255,  181,  197,  "pink1" },
  { 238,  169,  184,  "pink2" },
  { 205,  145,  158,  "pink3" },
  { 139,   99,  108,  "pink4" },
  { 221,  160,  221,  "plum" },
  { 255,  187,  255,  "plum1" },
  { 238,  174,  238,  "plum2" },
  { 205,  150,  205,  "plum3" },
  { 139,  102,  139,  "plum4" },
  { 176,  224,  230,  "powderblue" },
  { 160,   32,  240,  "purple" },
  { 155,   48,  255,  "purple1" },
  { 145,   44,  238,  "purple2" },
  { 125,   38,  205,  "purple3" },
  {  85,   26,  139,  "purple4" },
  { 255,    0,    0,  "red" },
  { 255,    0,    0,  "red1" },
  { 238,    0,    0,  "red2" },
  { 205,    0,    0,  "red3" },
  { 139,    0,    0,  "red4" },
  { 188,  143,  143,  "rosybrown" },
  { 255,  193,  193,  "rosybrown1" },
  { 238,  180,  180,  "rosybrown2" },
  { 205,  155,  155,  "rosybrown3" },
  { 139,  105,  105,  "rosybrown4" },
  {  65,  105,  225,  "royalblue" },
  {  72,  118,  255,  "royalblue1" },
  {  67,  110,  238,  "royalblue2" },
  {  58,   95,  205,  "royalblue3" },
  {  39,   64,  139,  "royalblue4" },
  { 139,   69,   19,  "saddlebrown" },
  { 250,  128,  114,  "salmon" },
  { 255,  140,  105,  "salmon1" },
  { 238,  130,   98,  "salmon2" },
  { 205,  112,   84,  "salmon3" },
  { 139,   76,   57,  "salmon4" },
  { 244,  164,   96,  "sandybrown" },
  {  46,  139,   87,  "seagreen" },
  {  84,  255,  159,  "seagreen1" },
  {  78,  238,  148,  "seagreen2" },
  {  67,  205,  128,  "seagreen3" },
  {  46,  139,   87,  "seagreen4" },
  { 255,  245,  238,  "seashell" },
  { 255,  245,  238,  "seashell1" },
  { 238,  229,  222,  "seashell2" },
  { 205,  197,  191,  "seashell3" },
  { 139,  134,  130,  "seashell4" },
  { 160,   82,   45,  "sienna" },
  { 255,  130,   71,  "sienna1" },
  { 238,  121,   66,  "sienna2" },
  { 205,  104,   57,  "sienna3" },
  { 139,   71,   38,  "sienna4" },
  { 135,  206,  235,  "skyblue" },
  { 135,  206,  255,  "skyblue1" },
  { 126,  192,  238,  "skyblue2" },
  { 108,  166,  205,  "skyblue3" },
  {  74,  112,  139,  "skyblue4" },
  { 106,   90,  205,  "slateblue" },
  { 131,  111,  255,  "slateblue1" },
  { 122,  103,  238,  "slateblue2" },
  { 105,   89,  205,  "slateblue3" },
  {  71,   60,  139,  "slateblue4" },
  { 112,  128,  144,  "slategray" },
  { 198,  226,  255,  "slategray1" },
  { 185,  211,  238,  "slategray2" },
  { 159,  182,  205,  "slategray3" },
  { 108,  123,  139,  "slategray4" },
  { 112,  128,  144,  "slategrey" },
  { 255,  250,  250,  "snow" },
  { 255,  250,  250,  "snow1" },
  { 238,  233,  233,  "snow2" },
  { 205,  201,  201,  "snow3" },
  { 139,  137,  137,  "snow4" },
  {   0,  255,  127,  "springgreen" },
  {   0,  255,  127,  "springgreen1" },
  {   0,  238,  118,  "springgreen2" },
  {   0,  205,  102,  "springgreen3" },
  {   0,  139,   69,  "springgreen4" },
  {  70,  130,  180,  "steelblue" },
  {  99,  184,  255,  "steelblue1" },
  {  92,  172,  238,  "steelblue2" },
  {  79,  148,  205,  "steelblue3" },
  {  54,  100,  139,  "steelblue4" },
  { 210,  180,  140,  "tan" },
  { 255,  165,   79,  "tan1" },
  { 238,  154,   73,  "tan2" },
  { 205,  133,   63,  "tan3" },
  { 139,   90,   43,  "tan4" },
  { 216,  191,  216,  "thistle" },
  { 255,  225,  255,  "thistle1" },
  { 238,  210,  238,  "thistle2" },
  { 205,  181,  205,  "thistle3" },
  { 139,  123,  139,  "thistle4" },
  { 255,   99,   71,  "tomato" },
  { 255,   99,   71,  "tomato1" },
  { 238,   92,   66,  "tomato2" },
  { 205,   79,   57,  "tomato3" },
  { 139,   54,   38,  "tomato4" },
  {  64,  224,  208,  "turquoise" },
  {   0,  245,  255,  "turquoise1" },
  {   0,  229,  238,  "turquoise2" },
  {   0,  197,  205,  "turquoise3" },
  {   0,  134,  139,  "turquoise4" },
  { 238,  130,  238,  "violet" },
  { 208,   32,  144,  "violetred" },
  { 255,   62,  150,  "violetred1" },
  { 238,   58,  140,  "violetred2" },
  { 205,   50,  120,  "violetred3" },
  { 139,   34,   82,  "violetred4" },
  { 245,  222,  179,  "wheat" },
  { 255,  231,  186,  "wheat1" },
  { 238,  216,  174,  "wheat2" },
  { 205,  186,  150,  "wheat3" },
  { 139,  126,  102,  "wheat4" },
  { 255,  255,  255,  "white" },
  { 245,  245,  245,  "whitesmoke" },
  { 255,  255,    0,  "yellow" },
  { 255,  255,    0,  "yellow1" },
  { 238,  238,    0,  "yellow2" },
  { 205,  205,    0,  "yellow3" },
  { 139,  139,    0,  "yellow4" },
  { 154,  205,   50,  "yellowgreen" }
};


#if 0
void dump_widget_type(xitk_widget_t *w) {
  if(w->type & WIDGET_GROUP) {
    printf("WIDGET_TYPE_GROUP: ");
    if(w->type & WIDGET_GROUP_MEMBER)
      printf("[THE_WIDGET] ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_BROWSER)
      printf("WIDGET_TYPE_BROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_FILEBROWSER)
      printf("WIDGET_TYPE_FILEBROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MRLBROWSER)
      printf("WIDGET_TYPE_MRLBROWSER | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_COMBO)
      printf("WIDGET_TYPE_COMBO | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_TABS)
      printf("WIDGET_TYPE_TABS | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_INTBOX)
      printf("WIDGET_TYPE_INTBOX | ");
    if((w->type & WIDGET_GROUP_MASK) & WIDGET_GROUP_MENU)
      printf("WIDGET_TYPE_MENU | ");
  }

  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_BUTTON)      printf("WIDGET_TYPE_BUTTON ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABELBUTTON) printf("WIDGET_TYPE_LABELBUTTON ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)      printf("WIDGET_TYPE_SLIDER ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)       printf("WIDGET_TYPE_LABEL ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)    printf("WIDGET_TYPE_CHECKBOX ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_IMAGE)       printf("WIDGET_TYPE_IMAGE ");
  if((w->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)   printf("WIDGET_TYPE_INPUTTEXT ");
  printf("\n");
}
#endif

static void xitk_widget_rel_init (xitk_widget_rel_t *r, xitk_widget_t *w) {
  xitk_dnode_init (&r->node);
  xitk_dlist_init (&r->list);
  r->group = NULL;
  r->w = w;
}

static void xitk_widget_rel_join (xitk_widget_rel_t *r, xitk_widget_rel_t *group) {
  if (r->group == group)
    return;
  if (r->group) {
    xitk_dnode_remove (&r->node);
    r->group = NULL;
  }
  if (group) {
    xitk_dlist_add_tail (&group->list, &r->node);
    r->group = group;
  }
}

static void xitk_widget_rel_deinit (xitk_widget_rel_t *r) {
  if (r->group) {
    xitk_dnode_remove (&r->node);
    r->group = NULL;
  }
  while (1) {
    xitk_widget_rel_t *s = (xitk_widget_rel_t *)r->list.tail.prev;
    if (!s->node.prev)
      break;
    xitk_dnode_remove (&s->node);
    s->group = NULL;
  }
}

/*
 * Alloc a memory area of size_t size.
 */
void *xitk_xmalloc(size_t size) {
  void *ptrmalloc;

  /* prevent xitk_xmalloc(0) of possibly returning NULL */
  if(!size)
    size++;

  if((ptrmalloc = calloc(1, size)) == NULL) {
    XITK_DIE("%s(): calloc() failed: %s\n", __FUNCTION__, strerror(errno));
  }

  return ptrmalloc;
}

/*
 * Call the notify_change_skin function, if exist, of each widget in
 * the specifier widget list.
 */
void xitk_change_skins_widget_list(xitk_widget_list_t *wl, xitk_skin_config_t *skonfig) {
  xitk_widget_t   *mywidget;
  widget_event_t   event;

  if (!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  event.type    = WIDGET_EVENT_CHANGE_SKIN;
  event.skonfig = skonfig;

  mywidget = (xitk_widget_t *)wl->list.head.next;
  while (mywidget->node.next && wl->xwin && skonfig) {

    (void) mywidget->event(mywidget, &event, NULL);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}

/*
 * (re)paint the specified widget list.
 */
static void _xitk_widget_to_hull (xitk_widget_t *w, xitk_hull_t *hull) {
  hull->x1 = w->x;
  hull->x2 = w->x + w->width;
  hull->y1 = w->y;
  hull->y2 = w->y + w->height;
}
static int _xitk_is_hull_in_hull (xitk_hull_t *h1, xitk_hull_t *h2) {
  if (h1->x1 >= h2->x2)
    return 0;
  if (h1->x2 <= h2->x1)
    return 0;
  if (h1->y1 >= h2->y2)
    return 0;
  if (h1->y2 <= h2->y1)
    return 0;
  return 1;
}
#if 0
static void _xitk_or_hulls (xitk_hull_t *h1, xitk_hull_t *h2) {
  if (h1->x1 < h2->x1)
    h2->x1 = h1->x1;
  if (h1->x2 > h2->x2)
    h2->x2 = h1->x2;
  if (h1->y1 < h2->y1)
    h2->y1 = h1->y1;
  if (h1->y2 > h2->y2)
    h2->y2 = h1->y2;
}
#endif
static void _xitk_and_hulls (xitk_hull_t *h1, xitk_hull_t *h2) {
  if (h1->x1 > h2->x1)
    h2->x1 = h1->x1;
  if (h1->x2 < h2->x2)
    h2->x2 = h1->x2;
  if (h1->y1 > h2->y1)
    h2->y1 = h1->y1;
  if (h1->y2 < h2->y2)
    h2->y2 = h1->y2;
}

int xitk_partial_paint_widget_list (xitk_widget_list_t *wl, xitk_hull_t *hull) {
  xitk_widget_t   *w;
  widget_event_t   event;
  int n = 0;

  if (!wl || !hull)
    return 0;
  if (!wl->xwin)
    return 0;

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    xitk_hull_t wh;

    _xitk_widget_to_hull (w, &wh);
    if ((w->enable != WIDGET_ENABLE) && (w->have_focus != FOCUS_LOST)) {
      event.type = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_LOST;
      (void)w->event (w, &event, NULL);
      event.type = WIDGET_EVENT_PAINT;
      (void)w->event (w, &event, NULL);
      w->have_focus = FOCUS_LOST;
#if 0
      _xitk_or_hulls (&wh, hull);
#endif
      w->state.visible = w->visible;
      n += 1;
    } else if (_xitk_is_hull_in_hull (&wh, hull)) {
      if (w->type & WIDGET_PARTIAL_PAINTABLE) {
        _xitk_and_hulls (hull, &wh);
        event.type = WIDGET_EVENT_PARTIAL_PAINT;
        event.x = wh.x1;
        event.width = wh.x2 - wh.x1;
        event.y = wh.y1;
        event.height = wh.y2 - wh.y1;
        (void)w->event (w, &event, NULL);
      } else {
        event.type = WIDGET_EVENT_PAINT;
        (void)w->event (w, &event, NULL);
#if 0
        _xitk_or_hulls (&wh, hull);
#endif
      }
      w->state.visible = w->visible;
      n += 1;
    }
  }
  return n;
}

int xitk_paint_widget_list (xitk_widget_list_t *wl) {
  xitk_widget_t   *w;
  widget_event_t   event;

  if (!wl)
    return 1;
  if (!wl->xwin)
    return 1;

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if ((w->enable != WIDGET_ENABLE) && (w->have_focus != FOCUS_LOST)) {
      event.type = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_LOST;
      (void)w->event (w, &event, NULL);
      w->have_focus = FOCUS_LOST;
    }
    event.type = WIDGET_EVENT_PAINT;
    (void)w->event (w, &event, NULL);
    w->state.visible = w->visible;
  }
  return 1;
}

/*
 * Return 1 if mouse pointer is in widget area.
 */
#ifdef YET_UNUSED
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y) {
  int inside = 0;

  if(!widget) {
    XITK_WARNING("widget was NULL.\n");
    return 0;
  }

  if(((x >= widget->x) && (x < (widget->x + widget->width))) &&
     ((y >= widget->y) && (y < (widget->y + widget->height)))) {
    widget_event_t        event;
    widget_event_result_t result;

    inside = 1;

    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    if(widget->event(widget, &event, &result))
      inside = result.value;
  }

  return inside;
}
#endif

/*
 * Return widget present at specified coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y) {
  xitk_widget_t *w;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    widget_event_t        event;
    widget_event_result_t result;
    int d;

    if (w->enable != WIDGET_ENABLE)
      continue;
    if (!w->visible)
      continue;
    if (w->type & WIDGET_GROUP)
      continue;
    d = x - w->x;
    if ((d < 0) || (d >= w->width))
      continue;
    d = y - w->y;
    if ((d < 0) || (d >= w->height))
      continue;

    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    result.value = 1;
    if (!w->event (w, &event, &result))
      return w;
    if (result.value)
      return w;
  }
  return NULL;
}

static void xitk_widget_apply_focus_redirect (xitk_widget_t **w) {
  xitk_widget_t *fr = *w;
  if (!fr)
    return;
  while (fr->focus_redirect.group) {
    fr = fr->focus_redirect.group->w;
    if (fr == *w)
      return;
  }
  *w = fr;
}

/*
 * Call notify_focus (with FOCUS_MOUSE_[IN|OUT] as focus state),
 * function in right widget (the one who get, and the one who lose focus).
 */
void xitk_motion_notify_widget_list (xitk_widget_list_t *wl, int x, int y, unsigned int state) {
  xitk_widget_t *w;
  widget_event_t event;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  /* Convenience: while holding the slider, user need not stay within its
   * graphical bounds. Just move to closest possible. */
  if (wl->widget_pressed && (wl->widget_pressed == wl->widget_focused) &&
    ((wl->widget_pressed->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    widget_event_result_t   result;

    //    printf("slider already clicked -> send event\n");

    if (state & MODIFIER_BUTTON1) {
      event.type           = WIDGET_EVENT_CLICK;
      event.x              = x;
      event.y              = y;
      event.button_pressed = LBUTTON_DOWN;
      event.button         = 1;
      event.modifier       = state;
      (void) wl->widget_focused->event(wl->widget_focused, &event, &result);
    }
    return;
  }

  w = NULL;
  do {
    int d;
    widget_event_result_t result;
    xitk_widget_t *lw = wl->widget_under_mouse;
    /* Still over same widget? */
    if (!lw)
      break;
    if (lw->enable != WIDGET_ENABLE)
      break;
    if (!lw->visible)
      break;
    d = x - lw->x;
    if ((d < 0) || (d >= lw->width))
      break;
    d = y - lw->y;
    if ((d < 0) || (d >= lw->height))
      break;
    /* Yes, but for group leader recheck its members. */
    if (lw->type & WIDGET_GROUP)
      break;

    event.type = WIDGET_EVENT_INSIDE;
    event.x    = x;
    event.y    = y;
    result.value = 1;
    w = lw;
    if (!lw->event (lw, &event, &result))
      break;
    if (result.value)
      break;
    w = NULL;
  } while (0);
  if (!w)
    w = xitk_get_widget_at (wl, x, y);
  xitk_widget_apply_focus_redirect (&w);

  do {
    int f;
    if (!wl->widget_under_mouse) {
      /* maybe take over old focus? */
      wl->widget_under_mouse = wl->widget_focused;
      if (!wl->widget_under_mouse)
        break;
      /* HACK: menu sub branch opens with first entry focused. over it anyway to show sub sub. */
      if ((wl->widget_under_mouse->type & WIDGET_GROUP_MENU) && (w == wl->widget_under_mouse))
        break;
    }
    if (w == wl->widget_under_mouse)
      return;
    f = wl->widget_under_mouse == wl->widget_focused;
    if (!(wl->widget_under_mouse->type & WIDGET_FOCUSABLE))
      break;
    if (wl->widget_under_mouse->enable != WIDGET_ENABLE)
      break;
    if (f && (wl->widget_under_mouse->type & WIDGET_KEEP_FOCUS)) {
      if (((wl->widget_under_mouse->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT)
        || (wl->widget_under_mouse->type & WIDGET_GROUP_COMBO))
        break;
      if (!w)
        break;
    }
    event.type  = WIDGET_EVENT_FOCUS;
    event.focus = f ? FOCUS_LOST : FOCUS_MOUSE_OUT;
    wl->widget_under_mouse->event (wl->widget_under_mouse, &event, NULL);
    event.type = WIDGET_EVENT_PAINT;
    wl->widget_under_mouse->event (wl->widget_under_mouse, &event, NULL);
    if (f)
      wl->widget_focused = NULL;
  } while (0);

  wl->widget_under_mouse = w;
  /* Only give focus and paint when tips are accepted, otherwise associated window is invisible.
   * This call may occur from MotionNotify directly after iconifying window.
   * Also, xitk_tips_show_widget_tips (tips, NULL) now saves us xitk_tips_hide_tips (tips). */
  if (!xitk_tips_show_widget_tips (wl->xitk->tips, w))
    return;

  if (w && (w->type & WIDGET_FOCUSABLE)) {
#if 0
    dump_widget_type(mywidget);
#endif
    event.type  = WIDGET_EVENT_FOCUS;
    /* If widget still marked pressed or focus received, it shall receive the focus again. */
    event.focus = (w == wl->widget_pressed) || (w == wl->widget_focused) ? FOCUS_RECEIVED : FOCUS_MOUSE_IN;
    w->event (w, &event, NULL);
    event.type  = WIDGET_EVENT_PAINT;
    w->event (w, &event, NULL);
    if (w->type & WIDGET_GROUP_MENU)
      menu_auto_pop (w);
  }
}

/*
 * Call notify_focus (with FOCUS_[RECEIVED|LOST] as focus state),
 * then call notify_click function, if exist, of widget at coords x/y.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp, int modifier) {
  int                    bRepaint = 0;
  xitk_widget_t         *w, *menu = NULL;
  widget_event_t         event;
  widget_event_result_t  result;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  w = xitk_get_widget_at (wl, x, y);
  xitk_widget_apply_focus_redirect (&w);
  if (w) {
    if (x > w->x + w->width - 1)
      x = w->x + w->width - 1;
    else if (x < w->x)
      x = w->x;
    if (y > w->y + w->height - 1)
      y = w->y + w->height - 1;
    else if (y < w->y)
      y = w->y;
  }

  if(w != wl->widget_focused && !bUp) {

    if (wl->widget_focused) {

      xitk_tips_hide_tips(wl->xitk->tips);

      if((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	 wl->widget_focused->enable == WIDGET_ENABLE) {

        if (wl->widget_focused->type & WIDGET_GROUP_MENU)
	  menu = xitk_menu_get_menu(wl->widget_focused);

        event.type  = WIDGET_EVENT_FOCUS;
        event.focus = FOCUS_LOST;
        wl->widget_focused->event (wl->widget_focused, &event, NULL);
        wl->widget_focused->have_focus = FOCUS_LOST;
      }

      event.type = WIDGET_EVENT_PAINT;
      (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
    }

    wl->widget_focused = w;

    if (w) {

      if ((w->type & WIDGET_FOCUSABLE) && w->enable == WIDGET_ENABLE) {
	event.type = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_RECEIVED;
	(void) w->event(w, &event, NULL);
	w->have_focus = FOCUS_RECEIVED;
      }
      else
	wl->widget_focused = NULL;

      event.type = WIDGET_EVENT_PAINT;
      (void) w->event(w, &event, NULL);
    }

  }
  else if (!bUp) {

    if(wl->widget_under_mouse && w && (wl->widget_under_mouse == w)) {
      if ((w->type & WIDGET_FOCUSABLE) && w->enable == WIDGET_ENABLE) {
	event.type  = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_RECEIVED;
	(void) w->event(w, &event, NULL);
	w->have_focus = FOCUS_RECEIVED;
      }
    }

  }

  if (!bUp) {

    wl->widget_pressed = w;

    if (w) {
      widget_event_result_t result;

      xitk_tips_hide_tips(wl->xitk->tips);

      if((w->type & WIDGET_CLICKABLE) &&
	 w->enable == WIDGET_ENABLE && w->running) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = button;
        event.modifier       = modifier;

	if(w->event(w, &event, &result))
	  bRepaint |= result.value;
      }
    }
  }
  else {

    if(wl->widget_pressed) {
      if((wl->widget_pressed->type & WIDGET_CLICKABLE) &&
	 (wl->widget_pressed->enable == WIDGET_ENABLE) && wl->widget_pressed->running) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_UP;
	event.button         = button;
        event.modifier       = modifier;

	if(wl->widget_pressed->event(wl->widget_pressed, &event, &result))
	  bRepaint |= result.value;
      }

      wl->widget_pressed = NULL;
    }
  }

  if(!(wl->widget_focused) && menu)
    wl->widget_focused = menu;

  return((bRepaint == 1));
}

/*
 * Find the first focusable widget in wl, according to direction
 */

static int xitk_widget_pos_cmp (void *a, void *b) {
  xitk_widget_t *d = (xitk_widget_t *)a;
  xitk_widget_t *e = (xitk_widget_t *)b;
  int gd, ge;
  /* sort widgets by center top down then left right. keep groups together. */
  if (d->parent.group != e->parent.group) {
    if (d->parent.group)
      d = d->parent.group->w;
    if (e->parent.group)
      e = e->parent.group->w;
  }
  gd = ((uint32_t)(d->y + (d->height >> 1)) << 16) + d->x + (d->width >> 1);
  ge = ((uint32_t)(e->y + (e->height >> 1)) << 16) + e->x + (e->width >> 1);
  return gd - ge;
}

static xitk_widget_t *xitk_find_nextprev_focus (xitk_widget_list_t *wl, int backward) {
  int i, n;
  xitk_widget_t *w;
  xine_sarray_t *a = xine_sarray_new (128, xitk_widget_pos_cmp);

  if (!a)
    return NULL;
  n = 0;
  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if (!(w->type & WIDGET_TABABLE) || !w->enable || !w->visible || !w->width || !w->height)
      continue;
    xine_sarray_add (a, w);
    n += 1;
  }
  if (!n) {
    xine_sarray_delete (a);
    return NULL;
  }

  i = backward ? 0 : n - 1;
  w = wl->widget_focused;
  if (w) {
    int j;
    if (!(w->type & WIDGET_TABABLE) && w->parent.group && (w->parent.group->w->type & WIDGET_TABABLE))
      w = w->parent.group->w;
    j = xine_sarray_binary_search (a, w);
    if (j >= 0)
      i = j;
  }

  if (backward) {
    i = i > 0 ? i - 1 : n - 1;
  } else {
    i = i < n - 1 ? i + 1 : 0;
  }
  w = xine_sarray_get (a, i);
  xine_sarray_delete (a);
  return w;
}

void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward, int modifier) {
  widget_event_t event;
  xitk_widget_t *w;

  if (!wl)
    return;
  if (!wl->xwin)
    return;

  w = xitk_find_nextprev_focus (wl, backward);
  if (!w || (w == wl->widget_focused))
    return;

  if (wl->widget_focused) {
    if ((wl->widget_focused->type & (WIDGET_FOCUSABLE | WIDGET_TABABLE)) &&
        (wl->widget_focused->enable == WIDGET_ENABLE)) {
      event.type  = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_LOST;
      wl->widget_focused->have_focus = FOCUS_LOST;
      (void)wl->widget_focused->event (wl->widget_focused, &event, NULL);
      event.type = WIDGET_EVENT_PAINT;
      (void)wl->widget_focused->event (wl->widget_focused, &event, NULL);
    }
  }
  wl->widget_focused = w;

  xitk_tips_hide_tips (wl->xitk->tips);

  if ((wl->widget_focused->type & (WIDGET_FOCUSABLE | WIDGET_TABABLE)) &&
      (wl->widget_focused->enable == WIDGET_ENABLE)) {
    /* NOTE: this may redirect focus to a group member. rereaad wl->widget_focused. */
    event.type  = WIDGET_EVENT_FOCUS;
    event.focus = FOCUS_RECEIVED;
    (void)wl->widget_focused->event (wl->widget_focused, &event, NULL);
    if (!wl->widget_focused)
      return;
    wl->widget_focused->have_focus = FOCUS_RECEIVED;
    event.type = WIDGET_EVENT_PAINT;
    (void)wl->widget_focused->event (wl->widget_focused, &event, NULL);
  }

  if (wl->widget_focused->enable == WIDGET_ENABLE && wl->widget_focused->running) {
    if ((wl->widget_focused->type & WIDGET_CLICKABLE) &&
        (wl->widget_focused->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) {
      widget_event_result_t  result;
      event.type           = WIDGET_EVENT_CLICK;
      event.x              = wl->widget_focused->x + 1;
      event.y              = wl->widget_focused->y + 1;
      event.button_pressed = LBUTTON_DOWN;
      event.button         = AnyButton;
      event.modifier       = modifier;
      (void)wl->widget_focused->event (wl->widget_focused, &event, &result);
      event.type = WIDGET_EVENT_PAINT;
        (void)wl->widget_focused->event (wl->widget_focused, &event, NULL);
    }
  }

#if 0
    /* next widget will be focused */
    if((widget == wl->widget_focused) ||
       (widget && wl->widget_focused &&
	(((wl->widget_focused->type & WIDGET_GROUP_MASK) == WIDGET_GROUP_MENU)
	 && (wl->widget_focused->type & WIDGET_GROUP_MEMBER)))) {
      xitk_widget_t *next_widget;

      if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	  (wl->widget_focused->enable == WIDGET_ENABLE)) {

	event.type  = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_LOST;

	wl->widget_focused->have_focus = FOCUS_LOST;
	(void) wl->widget_focused->event(wl->widget_focused, &event, NULL);

	event.type = WIDGET_EVENT_PAINT;
	(void) wl->widget_focused->event(wl->widget_focused, &event, NULL);

      }

      next_widget = get_next_focusable_widget(wl, backward);

      if(next_widget == NULL)
	next_widget = first_widget;

      wl->widget_focused = next_widget;

      goto __focus_the_widget;
    }
#endif
}

/*
 *
 */
void xitk_set_focus_to_widget(xitk_widget_t *w) {
  xitk_widget_t       *widget;
  widget_event_t       event;
  xitk_widget_list_t  *wl;

  if (!w)
    return;
  wl = w->wl;
  if (!wl) {
    XITK_WARNING("widget list is NULL.\n");
    return;
  }
  if (!wl->xwin)
    return;

  /* paranois: w (still) in list? */
  widget = (xitk_widget_t *)wl->list.head.next;
  while (widget->node.next && (widget != w))
    widget = (xitk_widget_t *)widget->node.next;

  if (widget->node.next) {

    if(wl->widget_focused) {

      /* Kill (hide) tips */
      xitk_tips_hide_tips(wl->xitk->tips);

      if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	  (wl->widget_focused->enable == WIDGET_ENABLE)) {
	event.type = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_LOST;
	wl->widget_focused->have_focus = FOCUS_LOST;
	(void) wl->widget_focused->event(wl->widget_focused, &event, NULL);

	event.type = WIDGET_EVENT_PAINT;
	(void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
      }
    }

    wl->widget_focused = widget;

    if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	(wl->widget_focused->enable == WIDGET_ENABLE)) {
      event.type = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_RECEIVED;
      (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
      wl->widget_focused->have_focus = FOCUS_RECEIVED;

      event.type = WIDGET_EVENT_PAINT;
      (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
    }

    if (wl->widget_focused->enable == WIDGET_ENABLE && wl->widget_focused->running) {

      if((wl->widget_focused->type & WIDGET_CLICKABLE) &&
	 (wl->widget_focused->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) {
	widget_event_result_t  result;

	event.type           = WIDGET_EVENT_CLICK;
	event.x              = wl->widget_focused->x + 1;
	event.y              = wl->widget_focused->y + 1;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = AnyButton;
        event.modifier       = 0;
	(void) wl->widget_focused->event(wl->widget_focused, &event, &result);

	event.type = WIDGET_EVENT_PAINT;
	(void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
      }

    }
  }
  else
    XITK_WARNING("widget not found in list.\n");
}

/*
 * Return the focused widget.
 */
xitk_widget_t *xitk_get_focused_widget(xitk_widget_list_t *wl) {

  if(wl) {
    return wl->widget_focused;
  }

  XITK_WARNING("widget list is NULL\n");
  return NULL;
}

/*
 * Return the pressed widget.
 */
xitk_widget_t *xitk_get_pressed_widget(xitk_widget_list_t *wl) {

  if(wl)
    return wl->widget_pressed;

  XITK_WARNING("widget list is NULL\n");
  return NULL;
}

/*
 * Return widget width.
 */
int xitk_get_widget_width(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return -1;
  }
  return w->width;
}

/*
 * Return widget height.
 */
int xitk_get_widget_height(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return -1;
  }
  return w->height;
}

/*
 * Set position of a widget.
 */
int xitk_set_widget_pos(xitk_widget_t *w, int x, int y) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  if ((w->x == x) && (w->y == y))
    return 1;
  w->x = x;
  w->y = y;
  if (w->wl && (w->wl->widget_under_mouse == w))
    w->wl->widget_under_mouse = NULL;
  return 1;
}

/*
 * Get position of a widget.
 */
int xitk_get_widget_pos(xitk_widget_t *w, int *x, int *y) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    *x = *y = 0;
    return 0;
  }
  *x = w->x;
  *y = w->y;
  return 1;
}

uint32_t xitk_get_widget_type(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }

  return w->type;
}

/*
 * Return 1 if widget is enabled.
 */
int xitk_is_widget_enabled(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  return (w->enable == WIDGET_ENABLE);
}

/*
 * Return 1 if widget have focus.
 */
int xitk_is_widget_focused(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  return(w->have_focus == FOCUS_RECEIVED);
}

static void xitk_widget_show_hide (xitk_widget_t *w, int visible) {
  w->visible = visible;
  if (w->state.visible != visible) {
    widget_event_t  event;

    w->state.visible = visible;
    if (!visible && w->wl && (w->wl->widget_under_mouse == w))
      xitk_tips_hide_tips (w->wl->xitk->tips);
    event.type = WIDGET_EVENT_PAINT;
    w->event (w, &event, NULL);
  }
}

static void xitk_widget_able (xitk_widget_t *w, int enable) {
  w->enable = enable;
  if (w->state.enable != enable) {
    w->state.enable = enable;
    if (!enable) {
      if (w->wl) {
        if (w == w->wl->widget_under_mouse)
          xitk_tips_hide_tips (w->wl->xitk->tips);
        if ((w->type & WIDGET_FOCUSABLE) && (w->have_focus != FOCUS_LOST)) {
          widget_event_t  event;

          event.type  = WIDGET_EVENT_FOCUS;
          event.focus = FOCUS_LOST;
          w->event (w, &event, NULL);
          w->have_focus = FOCUS_LOST;
          event.type = WIDGET_EVENT_PAINT;
          w->event (w, &event, NULL);
        }
        if (w == w->wl->widget_focused)
          w->wl->widget_focused = NULL;
      }
    }
    w->enable = enable;
    if (w->type & WIDGET_GROUP) {
      widget_event_t  event;

      event.type = WIDGET_EVENT_ENABLE;
      w->event (w, &event, NULL);
    }
  }
}

static void xitk_widget_run (xitk_widget_t *w, int start) {
  w->running = start;
  w->state.running = start;
}


/*
 * Enable a widget.
 */
void xitk_enable_widget (xitk_widget_t *w) {
  if (!w) {
    XITK_WARNING ("widget is NULL\n");
    return;
  }
  xitk_widget_able (w, 1);
}

/*
 * Disable a widget.
 */
void xitk_disable_widget (xitk_widget_t *w) {
  if (!w)
    return;
  xitk_widget_able (w, 0);
}

/*
 * Destroy a widget.
 */
void xitk_destroy_widget(xitk_widget_t *w) {
  widget_event_t event;

  if (!w)
    return;

  xitk_clipboard_unregister_widget (w);
  xitk_widget_show_hide (w, 0);
  xitk_widget_run (w, 0);
  xitk_widget_able (w, 0);

  xitk_widget_rel_deinit (&w->parent);
  xitk_widget_rel_deinit (&w->focus_redirect);

  if (w->wl) {
    if (w == w->wl->widget_focused)
      w->wl->widget_focused = NULL;
    if (w == w->wl->widget_under_mouse)
      w->wl->widget_under_mouse = NULL;
    if (w == w->wl->widget_pressed)
      w->wl->widget_pressed = NULL;
  }

  xitk_dnode_remove (&w->node);

  event.type = WIDGET_EVENT_DESTROY;
  w->event (w, &event, NULL);

  XITK_FREE (w->tips_string);
  XITK_FREE (w);
}

/*
 * Destroy widgets from widget list.
 */
void xitk_destroy_widgets(xitk_widget_list_t *wl) {
  int again;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  /* do group leaders first, just in case they want to delete their members as well. */
  do {
    xitk_widget_t *w = (xitk_widget_t *)wl->list.tail.prev;
    again = 0;
    while (w->node.prev) {
      if (w->type & WIDGET_GROUP) {
        xitk_destroy_widget (w);
        again = 1;
        break;
      }
      w = (xitk_widget_t *)w->node.prev;
    }
  } while (again);

  while (1) {
    xitk_widget_t *mywidget = (xitk_widget_t *)wl->list.tail.prev;
    if (!mywidget->node.prev)
      break;
    xitk_destroy_widget(mywidget);
  }
}

#if 0
/*
 * Return the struct of color names/values.
 */
xitk_color_names_t *gui_get_color_names(void) {

  return xitk_color_names;
}
#endif

/*
 * Return a xitk_color_name_t type from a string color.
 */
xitk_color_names_t *xitk_get_color_name (xitk_color_names_t *cn, const char *color) {
  char lname[128];
  size_t nlen;

  if(color == NULL) {
    XITK_WARNING("color was NULL.\n");
    return NULL;
  }

  /* convert copied color to lowercase, this can avoid some problems
   * with _buggy_ sscanf(), who knows ;-) */
  {
    const char *p = color;
    char *q = lname, *e = q + sizeof (lname) - 1;

    while (*p && (q < e))
      *q++ = tolower (*p++);
    nlen = q - lname;
    *q = 0;
  }

  /* Hexa X triplet */
  if ((lname[0] == '#') && (nlen == 7)) {
    if (!cn) {
      cn = (xitk_color_names_t *)xitk_xmalloc (sizeof (*cn));
      if (!cn)
        return NULL;
    }
    if ((sscanf (lname + 1, "%2x%2x%2x", &cn->red, &cn->green, &cn->blue)) != 3) {
      XITK_WARNING ("sscanf () failed: %s\n", strerror (errno));
      cn->red = cn->green = cn->blue = 0;
    }
    strlcpy (cn->colorname, color, sizeof (cn->colorname));
    return cn;
  }
  {
    unsigned int b = 0, m, e = sizeof (xitk_sorted_color_names) / sizeof (xitk_sorted_color_names[0]);
    int d;
    do {
      m = (b + e) >> 1;
      d = strcmp (lname, xitk_sorted_color_names[m].colorname);
      if (d == 0)
        break;
      if (d < 0)
        e = m;
      else
        b = m + 1;
    } while (b != e);
    if (d == 0) {
      if (!cn)
        cn = (xitk_color_names_t *)xitk_xmalloc (sizeof (xitk_color_names_t));
      if (cn) {
        *cn = xitk_sorted_color_names[m];
        return cn;
      }
    }
  }
  return NULL;
}


/*
 * Free color object.
 */
void xitk_free_color_name(xitk_color_names_t *color) {

  if(!color) {
    XITK_WARNING("color is NULL\n");
    return;
  }

  XITK_FREE(color);
}

/*
 * Stop a widget.
 */
void xitk_stop_widget(xitk_widget_t *w) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  xitk_widget_run (w, 0);
}

/*
 * (Re)Start a widget.
 */
void xitk_start_widget(xitk_widget_t *w) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  xitk_widget_run (w, 1);
}

/*
 * Stop widgets from widget list.
 */
#ifdef YET_UNUSED
void xitk_stop_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  mywidget = (xitk_widget_t *)wl->list.head.next;

  while (mywidget->node.next) {

    xitk_stop_widget(mywidget);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}
#endif

/*
 * Show a widget.
 */
void xitk_show_widget (xitk_widget_t *w) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  xitk_widget_show_hide (w, 1);
}

/*
 * Show widgets from widget list.
 */
void xitk_show_widgets (xitk_widget_list_t *wl) {
  xitk_widget_t *w;

  if (!wl) {
    XITK_WARNING ("widget list was NULL.\n");
    return;
  }
  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if (w->visible == -1) {
      w->visible = 0;
    } else {
      xitk_widget_show_hide (w, 1);
    }
  }
}

void xitk_enable_and_show_widget(xitk_widget_t *w) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  xitk_widget_able (w, 1);
  xitk_widget_show_hide (w, 1);
}

/*
 * Hide a widget.
 */
void xitk_hide_widget (xitk_widget_t *w) {
  if (!w)
    return;
  xitk_widget_show_hide (w, 0);
}

/*
 * Hide widgets from widget list..
 */
void xitk_hide_widgets (xitk_widget_list_t *wl) {
  xitk_widget_t *w;

  if (!wl) {
    XITK_WARNING ("widget list was NULL.\n");
    return;
  }

  for (w = (xitk_widget_t *)wl->list.head.next; w->node.next; w = (xitk_widget_t *)w->node.next) {
    if (w->visible == 0) {
        w->visible = -1;
    } else {
      xitk_widget_show_hide (w, 0);
    }
  }
}

void xitk_disable_and_hide_widget (xitk_widget_t *w) {
  if (!w)
    return;
  xitk_widget_able (w, 0);
  xitk_widget_show_hide (w, 0);
}

/*
 *
 */
xitk_image_t *xitk_get_widget_foreground_skin(xitk_widget_t *w) {
  widget_event_t         event;
  widget_event_result_t  result;
  xitk_image_t          *image = NULL;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return NULL;
  }

  event.type = WIDGET_EVENT_GET_SKIN;
  event.skin_layer = FOREGROUND_SKIN;

  if(w->event(w, &event, &result))
    image = result.image;

  return image;
}

/*
 *
 */
#ifdef YET_UNUSED
xitk_image_t *xitk_get_widget_background_skin(xitk_widget_t *w) {
  widget_event_t         event;
  widget_event_result_t  result;
  xitk_image_t          *image = NULL;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return NULL;
  }

  event.type = WIDGET_EVENT_GET_SKIN;
  event.skin_layer = BACKGROUND_SKIN;

  if(w->event(w, &event, &result))
    image = result.image;

  return image;
}
#endif

/*
 *
 */
void xitk_set_widget_tips(xitk_widget_t *w, const char *str) {

  if(!w || !str) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_tips(w, str);
}

/*
 *
 */
void xitk_set_widget_tips_default(xitk_widget_t *w, const char *str) {

  if(!w || !str) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_tips(w, str);
  xitk_tips_set_timeout (w, xitk_get_cfg_num (w->wl->xitk, XITK_TIPS_TIMEOUT));
}

/*
 *
 */
void xitk_set_widget_tips_and_timeout(xitk_widget_t *w, const char *str, unsigned long timeout) {

  if(!w || !str) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_tips(w, str);
  xitk_tips_set_timeout(w, timeout);
}

/*
 *
 */
unsigned long xitk_get_widget_tips_timeout(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }

  return w->tips_timeout;
}

/*
 *
 */
void xitk_disable_widget_tips(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_timeout(w, 0);
}

/*
 *
 */
#ifdef YET_UNUSED
void xitk_enable_widget_tips(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_timeout (w, xitk_get_cfg_num (w->wl->xitk, XITK_TIPS_TIMEOUT));
}
#endif

/*
 *
 */
void xitk_disable_widgets_tips(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  mywidget = (xitk_widget_t *)wl->list.head.next;

  while (mywidget->node.next) {

    xitk_disable_widget_tips(mywidget);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}

/*
 *
 */
#ifdef YET_UNUSED
void xitk_enable_widgets_tips(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  mywidget = (xitk_widget_t *)wl->list.head.next;

  while (mywidget->node.next) {

    if(mywidget->tips_string)
      xitk_enable_widget_tips(mywidget);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}
#endif

/*
 *
 */
void xitk_set_widgets_tips_timeout(xitk_widget_list_t *wl, unsigned long timeout) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  xitk_set_tips_timeout(timeout);

  mywidget = (xitk_widget_t *)wl->list.head.next;

  while (mywidget->node.next) {

    if(mywidget->tips_string)
      xitk_tips_set_timeout(mywidget, timeout);

    mywidget = (xitk_widget_t *)mywidget->node.next;
  }
}

/*
 *
 */
void xitk_set_widget_tips_timeout(xitk_widget_t *w, unsigned long timeout) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_timeout(w, timeout);
}

int xitk_is_mouse_over_widget(xitk_widget_t *w) {
  int             win_x, win_y;
  Display        *display = xitk_x11_get_display(w->wl->xitk);
  Window          window = xitk_window_get_window(w->wl->xwin);

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }

  if (xitk_get_mouse_coords(display, window, &win_x, &win_y, NULL, NULL)) {
    if(((win_x >= w->x) && (win_x < (w->x + w->width))) &&
       ((win_y >= w->y) && (win_y < (w->y + w->height)))) {
      return 1;
    }
  }

  return 0;
}

int xitk_get_mouse_coords(Display *display, Window window, int *x, int *y, int *rx, int *ry) {
  Bool            ret;
  Window          root_window, child_window;
  int             root_x, root_y, win_x, win_y;
  unsigned int    mask;
  int             retval = 0;

  if(window == None) {
    XITK_WARNING("window is None\n");
    return 0;
  }

  XLOCK (xitk_x_lock_display, display);
  ret = XQueryPointer(display, window,
		      &root_window, &child_window, &root_x, &root_y, &win_x, &win_y, &mask);
  XUNLOCK (xitk_x_unlock_display, display);

  if((ret == False) ||
     ((child_window == None) && (win_x == 0) && (win_y == 0))) {
    retval = 0;
  }
  else {

    if(x)
      *x = win_x;
    if(y)
      *y = win_y;
    if(rx)
      *rx = root_x;
    if(ry)
      *ry = root_y;

    retval = 1;
  }

  return retval;
}

int xitk_widget_mode (xitk_widget_t *w, int mask, int mode) {
  if (!w)
    return 0;

  mask &= WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE;
  w->type = (w->type & ~mask) | (mode & mask);
  return w->type & (WIDGET_TABABLE | WIDGET_FOCUSABLE | WIDGET_CLICKABLE | WIDGET_KEEP_FOCUS | WIDGET_KEYABLE);
}

void xitk_add_widget (xitk_widget_list_t *wl, xitk_widget_t *wi) {
  if (wl && wi)
    xitk_dlist_add_tail (&wl->list, &wi->node);
}

int xitk_widget_key_event (xitk_widget_t *w, const char *string, int modifier) {
  widget_event_t event;
  int handled;

  if (!w || !string)
    return 0;
  if (!string[0])
    return 0;

  event.type = WIDGET_EVENT_KEY;
  event.string = string;
  event.modifier = modifier;
  handled = 0;

  if (w->type & WIDGET_KEYABLE)
    handled = w->event (w, &event, NULL);

  if (!handled && w->parent.group && (w->parent.group->w->type & WIDGET_KEYABLE))
    handled = w->parent.group->w->event (w->parent.group->w, &event, NULL);

  return handled;
}


xitk_widget_t *xitk_widget_new (xitk_widget_list_t *wl, size_t size) {
  xitk_widget_t *w;

  if (size < sizeof (*w))
    size = sizeof (*w);
  w = xitk_xmalloc (size);
  if (!w)
    return NULL;

  w->node.next = w->node.prev = NULL;

  w->wl = wl;
  xitk_widget_rel_init (&w->parent, w);
  xitk_widget_rel_init (&w->focus_redirect, w);

  w->x = w->y = w->width = w->height = 0;

  w->type = 0;
  w->enable = w->running = w->visible = 1;
  w->have_focus = FOCUS_LOST;

  w->event = NULL;

  w->tips_timeout = 0;
  w->tips_string = NULL;

  w->private_data = size > sizeof (*w) ? w : NULL;

  w->state.enable = w->state.visible = w->running = 0x7fffffff;

  return w;
};

void xitk_widget_set_parent (xitk_widget_t *w, xitk_widget_t *parent) {
  if (!w)
    return;
  xitk_widget_rel_join (&w->parent, parent ? &parent->parent : NULL);
}

void xitk_widget_set_focus_redirect (xitk_widget_t *w, xitk_widget_t *focus_redirect) {
  if (!w)
    return;
  xitk_widget_rel_join (&w->focus_redirect,
    focus_redirect && (focus_redirect->wl == w->wl) ? &focus_redirect->focus_redirect : NULL);
}
