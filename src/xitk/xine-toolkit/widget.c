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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "_xitk.h"

static xitk_color_names_t xitk_color_names[] = {
  { 255,  250,  250,  "snow" },
  { 248,  248,  255,  "GhostWhite" },
  { 245,  245,  245,  "WhiteSmoke" },
  { 220,  220,  220,  "gainsboro" },
  { 255,  250,  240,  "FloralWhite" },
  { 253,  245,  230,  "OldLace" },
  { 250,  240,  230,  "linen" },
  { 250,  235,  215,  "AntiqueWhite" },
  { 255,  239,  213,  "PapayaWhip" },
  { 255,  235,  205,  "BlanchedAlmond" },
  { 255,  228,  196,  "bisque" },
  { 255,  218,  185,  "PeachPuff" },
  { 255,  222,  173,  "NavajoWhite" },
  { 255,  228,  181,  "moccasin" },
  { 255,  248,  220,  "cornsilk" },
  { 255,  255,  240,  "ivory" },
  { 255,  250,  205,  "LemonChiffon" },
  { 255,  245,  238,  "seashell" },
  { 240,  255,  240,  "honeydew" },
  { 245,  255,  250,  "MintCream" },
  { 240,  255,  255,  "azure" },
  { 240,  248,  255,  "AliceBlue" },
  { 230,  230,  250,  "lavender" },
  { 255,  240,  245,  "LavenderBlush" },
  { 255,  228,  225,  "MistyRose" },
  { 255,  255,  255,  "white" },
  {   0,    0,    0,  "black" },
  {  47,   79,   79,  "DarkSlateGrey" },
  { 105,  105,  105,  "DimGray" },
  { 105,  105,  105,  "DimGrey" },
  { 112,  128,  144,  "SlateGray" },
  { 112,  128,  144,  "SlateGrey" },
  { 119,  136,  153,  "LightSlateGray" },
  { 119,  136,  153,  "LightSlateGrey" },
  { 190,  190,  190,  "gray" },
  { 190,  190,  190,  "grey" },
  { 211,  211,  211,  "LightGrey" },
  { 211,  211,  211,  "LightGray" },
  {  25,   25,  112,  "MidnightBlue" },
  {   0,    0,  128,  "navy" },
  {   0,    0,  128,  "NavyBlue" },
  { 100,  149,  237,  "CornflowerBlue" },
  {  72,   61,  139,  "DarkSlateBlue" },
  { 106,   90,  205,  "SlateBlue" },
  { 123,  104,  238,  "MediumSlateBlue" },
  { 132,  112,  255,  "LightSlateBlue" },
  {   0,    0,  205,  "MediumBlue" },
  {  65,  105,  225,  "RoyalBlue" },
  {   0,    0,  255,  "blue" },
  {  30,  144,  255,  "DodgerBlue" },
  {   0,  191,  255,  "DeepSkyBlue" },
  { 135,  206,  235,  "SkyBlue" },
  { 135,  206,  250,  "LightSkyBlue" },
  {  70,  130,  180,  "SteelBlue" },
  { 176,  196,  222,  "LightSteelBlue" },
  { 173,  216,  230,  "LightBlue" },
  { 176,  224,  230,  "PowderBlue" },
  { 175,  238,  238,  "PaleTurquoise" },
  {   0,  206,  209,  "DarkTurquoise" },
  {  72,  209,  204,  "MediumTurquoise" },
  {  64,  224,  208,  "turquoise" },
  {   0,  255,  255,  "cyan" },
  { 224,  255,  255,  "LightCyan" },
  {  95,  158,  160,  "CadetBlue" },
  { 102,  205,  170,  "MediumAquamarine" },
  { 127,  255,  212,  "aquamarine" },
  {   0,  100,    0,  "DarkGreen" },
  {  85,  107,   47,  "DarkOliveGreen" },
  { 143,  188,  143,  "DarkSeaGreen" },
  {  46,  139,   87,  "SeaGreen" },
  {  60,  179,  113,  "MediumSeaGreen" },
  {  32,  178,  170,  "LightSeaGreen" },
  { 152,  251,  152,  "PaleGreen" },
  {   0,  255,  127,  "SpringGreen" },
  { 124,  252,    0,  "LawnGreen" },
  { 127,  255,    0,  "chartreuse" },
  {   0,  250,  154,  "MediumSpringGreen" },
  { 173,  255,   47,  "GreenYellow" },
  {  50,  205,   50,  "LimeGreen" },
  { 154,  205,   50,  "YellowGreen" },
  {  34,  139,   34,  "ForestGreen" },
  { 107,  142,   35,  "OliveDrab" },
  { 189,  183,  107,  "DarkKhaki" },
  { 240,  230,  140,  "khaki" },
  { 238,  232,  170,  "PaleGoldenrod" },
  { 250,  250,  210,  "LightGoldenrodYellow" },
  { 255,  255,  224,  "LightYellow" },
  { 255,  255,    0,  "yellow" },
  { 255,  215,    0,  "gold" },
  { 238,  221,  130,  "LightGoldenrod" },
  { 218,  165,   32,  "goldenrod" },
  { 184,  134,   11,  "DarkGoldenrod" },
  { 188,  143,  143,  "RosyBrown" },
  { 205,   92,   92,  "IndianRed" },
  { 139,   69,   19,  "SaddleBrown" },
  { 160,   82,   45,  "sienna" },
  { 205,  133,   63,  "peru" },
  { 222,  184,  135,  "burlywood" },
  { 245,  245,  220,  "beige" },
  { 245,  222,  179,  "wheat" },
  { 244,  164,   96,  "SandyBrown" },
  { 210,  180,  140,  "tan" },
  { 210,  105,   30,  "chocolate" },
  { 178,   34,   34,  "firebrick" },
  { 165,   42,   42,  "brown" },
  { 233,  150,  122,  "DarkSalmon" },
  { 250,  128,  114,  "salmon" },
  { 255,  160,  122,  "LightSalmon" },
  { 255,  165,    0,  "orange" },
  { 255,  140,    0,  "DarkOrange" },
  { 255,  127,   80,  "coral" },
  { 240,  128,  128,  "LightCoral" },
  { 255,   99,   71,  "tomato" },
  { 255,   69,    0,  "OrangeRed" },
  { 255,    0,    0,  "red" },
  { 255,  105,  180,  "HotPink" },
  { 255,   20,  147,  "DeepPink" },
  { 255,  192,  203,  "pink" },
  { 255,  182,  193,  "LightPink" },
  { 219,  112,  147,  "PaleVioletRed" },
  { 176,   48,   96,  "maroon" },
  { 199,   21,  133,  "MediumVioletRed" },
  { 208,   32,  144,  "VioletRed" },
  { 255,    0,  255,  "magenta" },
  { 238,  130,  238,  "violet" },
  { 221,  160,  221,  "plum" },
  { 218,  112,  214,  "orchid" },
  { 186,   85,  211,  "MediumOrchid" },
  { 153,   50,  204,  "DarkOrchid" },
  { 148,    0,  211,  "DarkViolet" },
  { 138,   43,  226,  "BlueViolet" },
  { 160,   32,  240,  "purple" },
  { 147,  112,  219,  "MediumPurple" },
  { 216,  191,  216,  "thistle" },
  { 255,  250,  250,  "snow1" },
  { 238,  233,  233,  "snow2" },
  { 205,  201,  201,  "snow3" },
  { 139,  137,  137,  "snow4" },
  { 255,  245,  238,  "seashell1" },
  { 238,  229,  222,  "seashell2" },
  { 205,  197,  191,  "seashell3" },
  { 139,  134,  130,  "seashell4" },
  { 255,  239,  219,  "AntiqueWhite1" },
  { 238,  223,  204,  "AntiqueWhite2" },
  { 205,  192,  176,  "AntiqueWhite3" },
  { 139,  131,  120,  "AntiqueWhite4" },
  { 255,  228,  196,  "bisque1" },
  { 238,  213,  183,  "bisque2" },
  { 205,  183,  158,  "bisque3" },
  { 139,  125,  107,  "bisque4" },
  { 255,  218,  185,  "PeachPuff1" },
  { 238,  203,  173,  "PeachPuff2" },
  { 205,  175,  149,  "PeachPuff3" },
  { 139,  119,  101,  "PeachPuff4" },
  { 255,  222,  173,  "NavajoWhite1" },
  { 238,  207,  161,  "NavajoWhite2" },
  { 205,  179,  139,  "NavajoWhite3" },
  { 139,  121,   94,  "NavajoWhite4" },
  { 255,  250,  205,  "LemonChiffon1" },
  { 238,  233,  191,  "LemonChiffon2" },
  { 205,  201,  165,  "LemonChiffon3" },
  { 139,  137,  112,  "LemonChiffon4" },
  { 255,  248,  220,  "cornsilk1" },
  { 238,  232,  205,  "cornsilk2" },
  { 205,  200,  177,  "cornsilk3" },
  { 139,  136,  120,  "cornsilk4" },
  { 255,  255,  240,  "ivory1" },
  { 238,  238,  224,  "ivory2" },
  { 205,  205,  193,  "ivory3" },
  { 139,  139,  131,  "ivory4" },
  { 240,  255,  240,  "honeydew1" },
  { 224,  238,  224,  "honeydew2" },
  { 193,  205,  193,  "honeydew3" },
  { 131,  139,  131,  "honeydew4" },
  { 255,  240,  245,  "LavenderBlush1" },
  { 238,  224,  229,  "LavenderBlush2" },
  { 205,  193,  197,  "LavenderBlush3" },
  { 139,  131,  134,  "LavenderBlush4" },
  { 255,  228,  225,  "MistyRose1" },
  { 238,  213,  210,  "MistyRose2" },
  { 205,  183,  181,  "MistyRose3" },
  { 139,  125,  123,  "MistyRose4" },
  { 240,  255,  255,  "azure1" },
  { 224,  238,  238,  "azure2" },
  { 193,  205,  205,  "azure3" },
  { 131,  139,  139,  "azure4" },
  { 131,  111,  255,  "SlateBlue1" },
  { 122,  103,  238,  "SlateBlue2" },
  { 105,   89,  205,  "SlateBlue3" },
  {  71,   60,  139,  "SlateBlue4" },
  {  72,  118,  255,  "RoyalBlue1" },
  {  67,  110,  238,  "RoyalBlue2" },
  {  58,   95,  205,  "RoyalBlue3" },
  {  39,   64,  139,  "RoyalBlue4" },
  {   0,    0,  255,  "blue1" },
  {   0,    0,  238,  "blue2" },
  {   0,    0,  205,  "blue3" },
  {   0,    0,  139,  "blue4" },
  {  30,  144,  255,  "DodgerBlue1" },
  {  28,  134,  238,  "DodgerBlue2" },
  {  24,  116,  205,  "DodgerBlue3" },
  {  16,   78,  139,  "DodgerBlue4" },
  {  99,  184,  255,  "SteelBlue1" },
  {  92,  172,  238,  "SteelBlue2" },
  {  79,  148,  205,  "SteelBlue3" },
  {  54,  100,  139,  "SteelBlue4" },
  {   0,  191,  255,  "DeepSkyBlue1" },
  {   0,  178,  238,  "DeepSkyBlue2" },
  {   0,  154,  205,  "DeepSkyBlue3" },
  {   0,  104,  139,  "DeepSkyBlue4" },
  { 135,  206,  255,  "SkyBlue1" },
  { 126,  192,  238,  "SkyBlue2" },
  { 108,  166,  205,  "SkyBlue3" },
  {  74,  112,  139,  "SkyBlue4" },
  { 176,  226,  255,  "LightSkyBlue1" },
  { 164,  211,  238,  "LightSkyBlue2" },
  { 141,  182,  205,  "LightSkyBlue3" },
  {  96,  123,  139,  "LightSkyBlue4" },
  { 198,  226,  255,  "SlateGray1" },
  { 185,  211,  238,  "SlateGray2" },
  { 159,  182,  205,  "SlateGray3" },
  { 108,  123,  139,  "SlateGray4" },
  { 202,  225,  255,  "LightSteelBlue1" },
  { 188,  210,  238,  "LightSteelBlue2" },
  { 162,  181,  205,  "LightSteelBlue3" },
  { 110,  123,  139,  "LightSteelBlue4" },
  { 191,  239,  255,  "LightBlue1" },
  { 178,  223,  238,  "LightBlue2" },
  { 154,  192,  205,  "LightBlue3" },
  { 104,  131,  139,  "LightBlue4" },
  { 224,  255,  255,  "LightCyan1" },
  { 209,  238,  238,  "LightCyan2" },
  { 180,  205,  205,  "LightCyan3" },
  { 122,  139,  139,  "LightCyan4" },
  { 187,  255,  255,  "PaleTurquoise1" },
  { 174,  238,  238,  "PaleTurquoise2" },
  { 150,  205,  205,  "PaleTurquoise3" },
  { 102,  139,  139,  "PaleTurquoise4" },
  { 152,  245,  255,  "CadetBlue1" },
  { 142,  229,  238,  "CadetBlue2" },
  { 122,  197,  205,  "CadetBlue3" },
  {  83,  134,  139,  "CadetBlue4" },
  {   0,  245,  255,  "turquoise1" },
  {   0,  229,  238,  "turquoise2" },
  {   0,  197,  205,  "turquoise3" },
  {   0,  134,  139,  "turquoise4" },
  {   0,  255,  255,  "cyan1" },
  {   0,  238,  238,  "cyan2" },
  {   0,  205,  205,  "cyan3" },
  {   0,  139,  139,  "cyan4" },
  { 151,  255,  255,  "DarkSlateGray1" },
  { 141,  238,  238,  "DarkSlateGray2" },
  { 121,  205,  205,  "DarkSlateGray3" },
  {  82,  139,  139,  "DarkSlateGray4" },
  { 127,  255,  212,  "aquamarine1" },
  { 118,  238,  198,  "aquamarine2" },
  { 102,  205,  170,  "aquamarine3" },
  {  69,  139,  116,  "aquamarine4" },
  { 193,  255,  193,  "DarkSeaGreen1" },
  { 180,  238,  180,  "DarkSeaGreen2" },
  { 155,  205,  155,  "DarkSeaGreen3" },
  { 105,  139,  105,  "DarkSeaGreen4" },
  {  84,  255,  159,  "SeaGreen1" },
  {  78,  238,  148,  "SeaGreen2" },
  {  67,  205,  128,  "SeaGreen3" },
  {  46,  139,   87,  "SeaGreen4" },
  { 154,  255,  154,  "PaleGreen1" },
  { 144,  238,  144,  "PaleGreen2" },
  { 124,  205,  124,  "PaleGreen3" },
  {  84,  139,   84,  "PaleGreen4" },
  {   0,  255,  127,  "SpringGreen1" },
  {   0,  238,  118,  "SpringGreen2" },
  {   0,  205,  102,  "SpringGreen3" },
  {   0,  139,   69,  "SpringGreen4" },
  {   0,  255,    0,  "green1" },
  {   0,  238,    0,  "green2" },
  {   0,  205,    0,  "green3" },
  {   0,  139,    0,  "green4" },
  { 127,  255,    0,  "chartreuse1" },
  { 118,  238,    0,  "chartreuse2" },
  { 102,  205,    0,  "chartreuse3" },
  {  69,  139,    0,  "chartreuse4" },
  { 192,  255,   62,  "OliveDrab1" },
  { 179,  238,   58,  "OliveDrab2" },
  { 154,  205,   50,  "OliveDrab3" },
  { 105,  139,   34,  "OliveDrab4" },
  { 202,  255,  112,  "DarkOliveGreen1" },
  { 188,  238,  104,  "DarkOliveGreen2" },
  { 162,  205,   90,  "DarkOliveGreen3" },
  { 110,  139,   61,  "DarkOliveGreen4" },
  { 255,  246,  143,  "khaki1" },
  { 238,  230,  133,  "khaki2" },
  { 205,  198,  115,  "khaki3" },
  { 139,  134,   78,  "khaki4" },
  { 255,  236,  139,  "LightGoldenrod1" },
  { 238,  220,  130,  "LightGoldenrod2" },
  { 205,  190,  112,  "LightGoldenrod3" },
  { 139,  129,   76,  "LightGoldenrod4" },
  { 255,  255,  224,  "LightYellow1" },
  { 238,  238,  209,  "LightYellow2" },
  { 205,  205,  180,  "LightYellow3" },
  { 139,  139,  122,  "LightYellow4" },
  { 255,  255,    0,  "yellow1" },
  { 238,  238,    0,  "yellow2" },
  { 205,  205,    0,  "yellow3" },
  { 139,  139,    0,  "yellow4" },
  { 255,  215,    0,  "gold1" },
  { 238,  201,    0,  "gold2" },
  { 205,  173,    0,  "gold3" },
  { 139,  117,    0,  "gold4" },
  { 255,  193,   37,  "goldenrod1" },
  { 238,  180,   34,  "goldenrod2" },
  { 205,  155,   29,  "goldenrod3" },
  { 139,  105,   20,  "goldenrod4" },
  { 255,  185,   15,  "DarkGoldenrod1" },
  { 238,  173,   14,  "DarkGoldenrod2" },
  { 205,  149,   12,  "DarkGoldenrod3" },
  { 139,  101,    8,  "DarkGoldenrod4" },
  { 255,  193,  193,  "RosyBrown1" },
  { 238,  180,  180,  "RosyBrown2" },
  { 205,  155,  155,  "RosyBrown3" },
  { 139,  105,  105,  "RosyBrown4" },
  { 255,  106,  106,  "IndianRed1" },
  { 238,   99,   99,  "IndianRed2" },
  { 205,   85,   85,  "IndianRed3" },
  { 139,   58,   58,  "IndianRed4" },
  { 255,  130,   71,  "sienna1" },
  { 238,  121,   66,  "sienna2" },
  { 205,  104,   57,  "sienna3" },
  { 139,   71,   38,  "sienna4" },
  { 255,  211,  155,  "burlywood1" },
  { 238,  197,  145,  "burlywood2" },
  { 205,  170,  125,  "burlywood3" },
  { 139,  115,   85,  "burlywood4" },
  { 255,  231,  186,  "wheat1" },
  { 238,  216,  174,  "wheat2" },
  { 205,  186,  150,  "wheat3" },
  { 139,  126,  102,  "wheat4" },
  { 255,  165,   79,  "tan1" },
  { 238,  154,   73,  "tan2" },
  { 205,  133,   63,  "tan3" },
  { 139,   90,   43,  "tan4" },
  { 255,  127,   36,  "chocolate1" },
  { 238,  118,   33,  "chocolate2" },
  { 205,  102,   29,  "chocolate3" },
  { 139,   69,   19,  "chocolate4" },
  { 255,   48,   48,  "firebrick1" },
  { 238,   44,   44,  "firebrick2" },
  { 205,   38,   38,  "firebrick3" },
  { 139,   26,   26,  "firebrick4" },
  { 255,   64,   64,  "brown1" },
  { 238,   59,   59,  "brown2" },
  { 205,   51,   51,  "brown3" },
  { 139,   35,   35,  "brown4" },
  { 255,  140,  105,  "salmon1" },
  { 238,  130,   98,  "salmon2" },
  { 205,  112,   84,  "salmon3" },
  { 139,   76,   57,  "salmon4" },
  { 255,  160,  122,  "LightSalmon1" },
  { 238,  149,  114,  "LightSalmon2" },
  { 205,  129,   98,  "LightSalmon3" },
  { 139,   87,   66,  "LightSalmon4" },
  { 255,  165,    0,  "orange1" },
  { 238,  154,    0,  "orange2" },
  { 205,  133,    0,  "orange3" },
  { 139,   90,    0,  "orange4" },
  { 255,  127,    0,  "DarkOrange1" },
  { 238,  118,    0,  "DarkOrange2" },
  { 205,  102,    0,  "DarkOrange3" },
  { 139,   69,    0,  "DarkOrange4" },
  { 255,  114,   86,  "coral1" },
  { 238,  106,   80,  "coral2" },
  { 205,   91,   69,  "coral3" },
  { 139,   62,   47,  "coral4" },
  { 255,   99,   71,  "tomato1" },
  { 238,   92,   66,  "tomato2" },
  { 205,   79,   57,  "tomato3" },
  { 139,   54,   38,  "tomato4" },
  { 255,   69,    0,  "OrangeRed1" },
  { 238,   64,    0,  "OrangeRed2" },
  { 205,   55,    0,  "OrangeRed3" },
  { 139,   37,    0,  "OrangeRed4" },
  { 255,    0,    0,  "red1" },
  { 238,    0,    0,  "red2" },
  { 205,    0,    0,  "red3" },
  { 139,    0,    0,  "red4" },
  { 255,   20,  147,  "DeepPink1" },
  { 238,   18,  137,  "DeepPink2" },
  { 205,   16,  118,  "DeepPink3" },
  { 139,   10,   80,  "DeepPink4" },
  { 255,  110,  180,  "HotPink1" },
  { 238,  106,  167,  "HotPink2" },
  { 205,   96,  144,  "HotPink3" },
  { 139,   58,   98,  "HotPink4" },
  { 255,  181,  197,  "pink1" },
  { 238,  169,  184,  "pink2" },
  { 205,  145,  158,  "pink3" },
  { 139,   99,  108,  "pink4" },
  { 255,  174,  185,  "LightPink1" },
  { 238,  162,  173,  "LightPink2" },
  { 205,  140,  149,  "LightPink3" },
  { 139,   95,  101,  "LightPink4" },
  { 255,  130,  171,  "PaleVioletRed1" },
  { 238,  121,  159,  "PaleVioletRed2" },
  { 205,  104,  137,  "PaleVioletRed3" },
  { 139,   71,   93,  "PaleVioletRed4" },
  { 255,   52,  179,  "maroon1" },
  { 238,   48,  167,  "maroon2" },
  { 205,   41,  144,  "maroon3" },
  { 139,   28,   98,  "maroon4" },
  { 255,   62,  150,  "VioletRed1" },
  { 238,   58,  140,  "VioletRed2" },
  { 205,   50,  120,  "VioletRed3" },
  { 139,   34,   82,  "VioletRed4" },
  { 255,    0,  255,  "magenta1" },
  { 238,    0,  238,  "magenta2" },
  { 205,    0,  205,  "magenta3" },
  { 139,    0,  139,  "magenta4" },
  { 255,  131,  250,  "orchid1" },
  { 238,  122,  233,  "orchid2" },
  { 205,  105,  201,  "orchid3" },
  { 139,   71,  137,  "orchid4" },
  { 255,  187,  255,  "plum1" },
  { 238,  174,  238,  "plum2" },
  { 205,  150,  205,  "plum3" },
  { 139,  102,  139,  "plum4" },
  { 224,  102,  255,  "MediumOrchid1" },
  { 209,   95,  238,  "MediumOrchid2" },
  { 180,   82,  205,  "MediumOrchid3" },
  { 122,   55,  139,  "MediumOrchid4" },
  { 191,   62,  255,  "DarkOrchid1" },
  { 178,   58,  238,  "DarkOrchid2" },
  { 154,   50,  205,  "DarkOrchid3" },
  { 104,   34,  139,  "DarkOrchid4" },
  { 155,   48,  255,  "purple1" },
  { 145,   44,  238,  "purple2" },
  { 125,   38,  205,  "purple3" },
  {  85,   26,  139,  "purple4" },
  { 171,  130,  255,  "MediumPurple1" },
  { 159,  121,  238,  "MediumPurple2" },
  { 137,  104,  205,  "MediumPurple3" },
  {  93,   71,  139,  "MediumPurple4" },
  { 255,  225,  255,  "thistle1" },
  { 238,  210,  238,  "thistle2" },
  { 205,  181,  205,  "thistle3" },
  { 139,  123,  139,  "thistle4" },
  {   0,    0,    0,  "gray0" },
  {   0,    0,    0,  "grey0" },
  {   3,    3,    3,  "gray1" },
  {   3,    3,    3,  "grey1" },
  {   5,    5,    5,  "gray2" },
  {   5,    5,    5,  "grey2" },
  {   8,    8,    8,  "gray3" },
  {   8,    8,    8,  "grey3" },
  {  10,   10,   10,  "gray4" },
  {  10,   10,   10,  "grey4" },
  {  13,   13,   13,  "gray5" },
  {  13,   13,   13,  "grey5" },
  {  15,   15,   15,  "gray6" },
  {  15,   15,   15,  "grey6" },
  {  18,   18,   18,  "gray7" },
  {  18,   18,   18,  "grey7" },
  {  20,   20,   20,  "gray8" },
  {  20,   20,   20,  "grey8" },
  {  23,   23,   23,  "gray9" },
  {  23,   23,   23,  "grey9" },
  {  26,   26,   26,  "gray10" },
  {  26,   26,   26,  "grey10" },
  {  28,   28,   28,  "gray11" },
  {  28,   28,   28,  "grey11" },
  {  31,   31,   31,  "gray12" },
  {  31,   31,   31,  "grey12" },
  {  33,   33,   33,  "gray13" },
  {  33,   33,   33,  "grey13" },
  {  36,   36,   36,  "gray14" },
  {  36,   36,   36,  "grey14" },
  {  38,   38,   38,  "gray15" },
  {  38,   38,   38,  "grey15" },
  {  41,   41,   41,  "gray16" },
  {  41,   41,   41,  "grey16" },
  {  43,   43,   43,  "gray17" },
  {  43,   43,   43,  "grey17" },
  {  46,   46,   46,  "gray18" },
  {  46,   46,   46,  "grey18" },
  {  48,   48,   48,  "gray19" },
  {  48,   48,   48,  "grey19" },
  {  51,   51,   51,  "gray20" },
  {  51,   51,   51,  "grey20" },
  {  54,   54,   54,  "gray21" },
  {  54,   54,   54,  "grey21" },
  {  56,   56,   56,  "gray22" },
  {  56,   56,   56,  "grey22" },
  {  59,   59,   59,  "gray23" },
  {  59,   59,   59,  "grey23" },
  {  61,   61,   61,  "gray24" },
  {  61,   61,   61,  "grey24" },
  {  64,   64,   64,  "gray25" },
  {  64,   64,   64,  "grey25" },
  {  66,   66,   66,  "gray26" },
  {  66,   66,   66,  "grey26" },
  {  69,   69,   69,  "gray27" },
  {  69,   69,   69,  "grey27" },
  {  71,   71,   71,  "gray28" },
  {  71,   71,   71,  "grey28" },
  {  74,   74,   74,  "gray29" },
  {  74,   74,   74,  "grey29" },
  {  77,   77,   77,  "gray30" },
  {  77,   77,   77,  "grey30" },
  {  79,   79,   79,  "gray31" },
  {  79,   79,   79,  "grey31" },
  {  82,   82,   82,  "gray32" },
  {  82,   82,   82,  "grey32" },
  {  84,   84,   84,  "gray33" },
  {  84,   84,   84,  "grey33" },
  {  87,   87,   87,  "gray34" },
  {  87,   87,   87,  "grey34" },
  {  89,   89,   89,  "gray35" },
  {  89,   89,   89,  "grey35" },
  {  92,   92,   92,  "gray36" },
  {  92,   92,   92,  "grey36" },
  {  94,   94,   94,  "gray37" },
  {  94,   94,   94,  "grey37" },
  {  97,   97,   97,  "gray38" },
  {  97,   97,   97,  "grey38" },
  {  99,   99,   99,  "gray39" },
  {  99,   99,   99,  "grey39" },
  { 102,  102,  102,  "gray40" },
  { 102,  102,  102,  "grey40" },
  { 105,  105,  105,  "gray41" },
  { 105,  105,  105,  "grey41" },
  { 107,  107,  107,  "gray42" },
  { 107,  107,  107,  "grey42" },
  { 110,  110,  110,  "gray43" },
  { 110,  110,  110,  "grey43" },
  { 112,  112,  112,  "gray44" },
  { 112,  112,  112,  "grey44" },
  { 115,  115,  115,  "gray45" },
  { 115,  115,  115,  "grey45" },
  { 117,  117,  117,  "gray46" },
  { 117,  117,  117,  "grey46" },
  { 120,  120,  120,  "gray47" },
  { 120,  120,  120,  "grey47" },
  { 122,  122,  122,  "gray48" },
  { 122,  122,  122,  "grey48" },
  { 125,  125,  125,  "gray49" },
  { 125,  125,  125,  "grey49" },
  { 127,  127,  127,  "gray50" },
  { 127,  127,  127,  "grey50" },
  { 130,  130,  130,  "gray51" },
  { 130,  130,  130,  "grey51" },
  { 133,  133,  133,  "gray52" },
  { 133,  133,  133,  "grey52" },
  { 135,  135,  135,  "gray53" },
  { 135,  135,  135,  "grey53" },
  { 138,  138,  138,  "gray54" },
  { 138,  138,  138,  "grey54" },
  { 140,  140,  140,  "gray55" },
  { 140,  140,  140,  "grey55" },
  { 143,  143,  143,  "gray56" },
  { 143,  143,  143,  "grey56" },
  { 145,  145,  145,  "gray57" },
  { 145,  145,  145,  "grey57" },
  { 148,  148,  148,  "gray58" },
  { 148,  148,  148,  "grey58" },
  { 150,  150,  150,  "gray59" },
  { 150,  150,  150,  "grey59" },
  { 153,  153,  153,  "gray60" },
  { 153,  153,  153,  "grey60" },
  { 156,  156,  156,  "gray61" },
  { 156,  156,  156,  "grey61" },
  { 158,  158,  158,  "gray62" },
  { 158,  158,  158,  "grey62" },
  { 161,  161,  161,  "gray63" },
  { 161,  161,  161,  "grey63" },
  { 163,  163,  163,  "gray64" },
  { 163,  163,  163,  "grey64" },
  { 166,  166,  166,  "gray65" },
  { 166,  166,  166,  "grey65" },
  { 168,  168,  168,  "gray66" },
  { 168,  168,  168,  "grey66" },
  { 171,  171,  171,  "gray67" },
  { 171,  171,  171,  "grey67" },
  { 173,  173,  173,  "gray68" },
  { 173,  173,  173,  "grey68" },
  { 176,  176,  176,  "gray69" },
  { 176,  176,  176,  "grey69" },
  { 179,  179,  179,  "gray70" },
  { 179,  179,  179,  "grey70" },
  { 181,  181,  181,  "gray71" },
  { 181,  181,  181,  "grey71" },
  { 184,  184,  184,  "gray72" },
  { 184,  184,  184,  "grey72" },
  { 186,  186,  186,  "gray73" },
  { 186,  186,  186,  "grey73" },
  { 189,  189,  189,  "gray74" },
  { 189,  189,  189,  "grey74" },
  { 191,  191,  191,  "gray75" },
  { 191,  191,  191,  "grey75" },
  { 194,  194,  194,  "gray76" },
  { 194,  194,  194,  "grey76" },
  { 196,  196,  196,  "gray77" },
  { 196,  196,  196,  "grey77" },
  { 199,  199,  199,  "gray78" },
  { 199,  199,  199,  "grey78" },
  { 201,  201,  201,  "gray79" },
  { 201,  201,  201,  "grey79" },
  { 204,  204,  204,  "gray80" },
  { 204,  204,  204,  "grey80" },
  { 207,  207,  207,  "gray81" },
  { 207,  207,  207,  "grey81" },
  { 209,  209,  209,  "gray82" },
  { 209,  209,  209,  "grey82" },
  { 212,  212,  212,  "gray83" },
  { 212,  212,  212,  "grey83" },
  { 214,  214,  214,  "gray84" },
  { 214,  214,  214,  "grey84" },
  { 217,  217,  217,  "gray85" },
  { 217,  217,  217,  "grey85" },
  { 219,  219,  219,  "gray86" },
  { 219,  219,  219,  "grey86" },
  { 222,  222,  222,  "gray87" },
  { 222,  222,  222,  "grey87" },
  { 224,  224,  224,  "gray88" },
  { 224,  224,  224,  "grey88" },
  { 227,  227,  227,  "gray89" },
  { 227,  227,  227,  "grey89" },
  { 229,  229,  229,  "gray90" },
  { 229,  229,  229,  "grey90" },
  { 232,  232,  232,  "gray91" },
  { 232,  232,  232,  "grey91" },
  { 235,  235,  235,  "gray92" },
  { 235,  235,  235,  "grey92" },
  { 237,  237,  237,  "gray93" },
  { 237,  237,  237,  "grey93" },
  { 240,  240,  240,  "gray94" },
  { 240,  240,  240,  "grey94" },
  { 242,  242,  242,  "gray95" },
  { 242,  242,  242,  "grey95" },
  { 245,  245,  245,  "gray96" },
  { 245,  245,  245,  "grey96" },
  { 247,  247,  247,  "gray97" },
  { 247,  247,  247,  "grey97" },
  { 250,  250,  250,  "gray98" },
  { 250,  250,  250,  "grey98" },
  { 252,  252,  252,  "gray99" },
  { 252,  252,  252,  "grey99" },
  { 255,  255,  255,  "gray100" },
  { 255,  255,  255,  "grey100" },
  { 169,  169,  169,  "DarkGrey" },
  { 169,  169,  169,  "DarkGray" },
  {   0,    0,  139,  "DarkBlue" },
  {   0,  139,  139,  "DarkCyan" },
  { 139,    0,  139,  "DarkMagenta" },
  { 139,    0,    0,  "DarkRed" },
  { 144,  238,  144,  "LightGreen" },
  {   0,    0,    0,  NULL }
};

#if 0
void dump_widget_type(xitk_widget_t *w) {
  if(w->type & WIDGET_GROUP) {
    printf("WIDGET_TYPE_GROUP: ");
    if(w->type & WIDGET_GROUP_WIDGET)
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

/*
 * Alloc a memory area of size_t size.
 */
void *xitk_xmalloc(size_t size) {
  void *ptrmalloc;
  
  /* prevent xitk_xmalloc(0) of possibly returning NULL */
  if(!size)
    size++;
  
  if((ptrmalloc = calloc(1, size)) == NULL) {
    XITK_WARNING("calloc() failed: %s.\n", strerror(errno));
    return NULL;
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

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  event.type    = WIDGET_EVENT_CHANGE_SKIN;
  event.skonfig = skonfig;

  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);
  while (mywidget && wl && wl->l && wl->win && wl->gc && skonfig) {

    (void) mywidget->event(mywidget, &event, NULL);

    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l); 
  }
}

/*
 * (re)paint the specified widget list.
 */
int xitk_paint_widget_list (xitk_widget_list_t *wl) {
  xitk_widget_t   *mywidget;
  widget_event_t   event;

  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while (mywidget && wl && wl->l && wl->win && wl->gc) {
    
    if((mywidget->enable != WIDGET_ENABLE) && (mywidget->have_focus != FOCUS_LOST)) {

      event.type = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_LOST;
      (void) mywidget->event(mywidget, &event, NULL);
      
      event.type = WIDGET_EVENT_PAINT;
      (void) mywidget->event(mywidget, &event, NULL);
      
    }
    else {
      event.type = WIDGET_EVENT_PAINT;
      (void) mywidget->event(mywidget, &event, NULL);
    }
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l); 
  }
  return 1;
}

/*
 * Return 1 if the mouse poiter is in the visible area of widget.
 */
int xitk_is_cursor_out_mask(Display *display, xitk_widget_t *w, Pixmap mask, int x, int y) {
  XImage *xi;
  Pixel p;
  
  if(!display) {
    XITK_WARNING("display was unset.\n");
    return 0;
  }
  if(!w) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  if(mask) {
    int xx, yy;
    
    if((xx = (x - w->x)) == w->width) xx--;
    if((yy = (y - w->y)) == w->height) yy--;
    
    XLOCK(display);
    xi = XGetImage(display, mask, xx, yy, 1, 1, AllPlanes, ZPixmap);
    p = XGetPixel(xi, 0, 0);
    XDestroyImage(xi);
    XUNLOCK(display);

    return (int) p;
  }
  
  return 1;
}

/*
 * Return 1 if mouse pointer is in widget area.
 */
int xitk_is_inside_widget (xitk_widget_t *widget, int x, int y) {
  int inside = 0;
  
  if(!widget) {
    XITK_WARNING("widget was NULL.\n");
    return 0;
  }

  if(((x >= widget->x) && (x <= (widget->x + widget->width))) && 
     ((y >= widget->y) && (y <= (widget->y + widget->height)))) {
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

/*
 * Return widget present at specified coords.
 */
xitk_widget_t *xitk_get_widget_at (xitk_widget_list_t *wl, int x, int y) {
  xitk_widget_t *mywidget;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }

  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);
  while (mywidget) {
    if ((xitk_is_inside_widget (mywidget, x, y))
	&& (mywidget->enable == WIDGET_ENABLE) && mywidget->visible)
      return mywidget;

    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
  return NULL;
}

/*
 * Call notify_focus (with FOCUS_MOUSE_[IN|OUT] as focus state), 
 * function in right widget (the one who get, and the one who lose focus).
 */
void xitk_motion_notify_widget_list(xitk_widget_list_t *wl, int x, int y, unsigned int state) { 
  xitk_widget_t *mywidget;
  widget_event_t event;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
 
  /* Slider still pressed, mouse pointer outside widget */
  if(wl->widget_pressed && wl->widget_focused && (wl->widget_pressed == wl->widget_focused) &&
     ((wl->widget_pressed->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_SLIDER)) {
    widget_event_result_t   result;
    
    //    printf("slider already clicked -> send event\n");

    if(state & Button1Mask) {
      event.type           = WIDGET_EVENT_CLICK;
      event.x              = x;
      event.y              = y;
      event.button_pressed = LBUTTON_DOWN;
      event.button         = Button1;
      (void) wl->widget_focused->event(wl->widget_focused, &event, &result);
    }
    return;
  }

  mywidget = xitk_get_widget_at (wl, x, y);
  
  if (mywidget != wl->widget_under_mouse) {
    
    if (wl->widget_under_mouse) {
      
      /* Kill (hide) tips */
      xitk_tips_tips_kill(wl->widget_under_mouse);
      
      if((wl->widget_under_mouse->type & WIDGET_FOCUSABLE) && 
	 wl->widget_under_mouse->enable == WIDGET_ENABLE) {
	
	event.type  = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_MOUSE_OUT;
	(void) wl->widget_under_mouse->event(wl->widget_under_mouse, &event, NULL);

      event.type = WIDGET_EVENT_PAINT;
      (void) wl->widget_under_mouse->event(wl->widget_under_mouse, &event, NULL);
      }
    }
    
    wl->widget_under_mouse = mywidget;
    
    if (mywidget && (mywidget->enable == WIDGET_ENABLE) && mywidget->visible) {
      
#if 0
      dump_widget_type(mywidget);
#endif
      
      xitk_tips_create(mywidget);
      
      if((mywidget->type & WIDGET_FOCUSABLE) && mywidget->enable == WIDGET_ENABLE) {
	event.type  = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_MOUSE_IN;
	(void) mywidget->event(mywidget, &event, NULL);

	event.type  = WIDGET_EVENT_PAINT;
	(void) mywidget->event(mywidget, &event, NULL);
      }
    }
  }
  else {
    if(mywidget && (mywidget->type & WIDGET_TYPE_SLIDER)) {
      widget_event_result_t   result;
      
      if(state & Button1Mask) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = Button1;
	(void) mywidget->event(mywidget, &event, &result);
      }
    }
  }
}

/*
 * Call notify_focus (with FOCUS_[RECEIVED|LOST] as focus state), 
 * then call notify_click function, if exist, of widget at coords x/y.
 */
int xitk_click_notify_widget_list (xitk_widget_list_t *wl, int x, int y, int button, int bUp) {
  int                    bRepaint = 0;
  xitk_widget_t         *mywidget, *menu = NULL;
  widget_event_t         event;
  widget_event_result_t  result;

  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return 0;
  }
  
  mywidget = xitk_get_widget_at (wl, x, y);

  if(mywidget != wl->widget_focused) {
    if (wl->widget_focused) {
      
      /* Kill (hide) tips */
      xitk_tips_tips_kill(wl->widget_focused);
      
      if((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	 wl->widget_focused->enable == WIDGET_ENABLE) {
	
	if(wl->widget_focused && (wl->widget_focused->type & WIDGET_GROUP_MENU))
	  menu = xitk_menu_get_menu(wl->widget_focused);

	if((wl->widget_focused->type & WIDGET_GROUP_COMBO)) {
	  if(((wl->widget_focused->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)
	     || ((wl->widget_focused->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL)) {

	    if((!(mywidget && 
		  ((((mywidget->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_LABEL) || 
		    ((mywidget->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_CHECKBOX)) &&
		   (xitk_combo_is_same_parent(wl->widget_focused, mywidget) == 1)))) ||
	       (!mywidget)) {

	      if(xitk_checkbox_get_state(wl->widget_focused))
		xitk_combo_rollunroll(wl->widget_focused);
	      
	    }
	  }
	}
	else {
	  event.type  = WIDGET_EVENT_FOCUS;
	  event.focus = FOCUS_LOST;
	  (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
	  wl->widget_focused->have_focus = FOCUS_LOST;
	}
      }
      
      event.type = WIDGET_EVENT_PAINT;
      (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
    }
    
    
    wl->widget_focused = mywidget;
    
    if (mywidget) {
      
      if ((mywidget->type & WIDGET_FOCUSABLE) && mywidget->enable == WIDGET_ENABLE) {
	event.type = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_RECEIVED;
	(void) mywidget->event(mywidget, &event, NULL);
	mywidget->have_focus = FOCUS_RECEIVED;
      }
      else
	wl->widget_focused = NULL;
      
      event.type = WIDGET_EVENT_PAINT;
      (void) mywidget->event(mywidget, &event, NULL);
    }
    
  }
  else {
    if(wl->widget_under_mouse && mywidget && (wl->widget_under_mouse == mywidget)) {
      
      if ((mywidget->type & WIDGET_FOCUSABLE) && mywidget->enable == WIDGET_ENABLE) {
	event.type  = WIDGET_EVENT_FOCUS;
	event.focus = FOCUS_RECEIVED;
	(void) mywidget->event(mywidget, &event, NULL);
	mywidget->have_focus = FOCUS_RECEIVED;
      }
    }
    
  }
  
  if (!bUp) {

    wl->widget_pressed = mywidget;
    
    if (mywidget) {
      widget_event_result_t result;

      /* Kill (hide) tips */
      xitk_tips_tips_kill(mywidget);
      
      if((mywidget->type & WIDGET_CLICKABLE) && 
	 mywidget->enable == WIDGET_ENABLE && mywidget->running) {
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = x;
	event.y              = y;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = button;

	if(mywidget->event(mywidget, &event, &result))
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

	if(wl->widget_pressed->event(wl->widget_pressed, &event, &result))
	  bRepaint |= result.value;
      }
    }
  }
  
  if((wl->widget_focused == NULL) && menu)
    wl->widget_focused = menu;

  return((bRepaint == 1));
}

/*
 * Find the first focusable widget in wl, according to direction
 */
static xitk_widget_t *get_next_focusable_widget(xitk_widget_list_t *wl, int backward) {
  xitk_widget_t *widget = NULL;
  int found;
  
  if(!wl) {
    XITK_WARNING("widget list is NULL.\n");
    return NULL;
  }
  
  do {
    
    found = 0;

    if(backward)
      widget = (xitk_widget_t *) xitk_list_prev_content (wl->l);
    else
      widget = (xitk_widget_t *) xitk_list_next_content (wl->l);

    if(widget == NULL) {

      if(backward)
	widget = (xitk_widget_t *) xitk_list_last_content (wl->l);
      else
	widget = (xitk_widget_t *) xitk_list_first_content (wl->l);

      if(widget == NULL) {
	
	do {
	  
	  if(backward)
	    widget = (xitk_widget_t *) xitk_list_last_content (wl->l);
	  else
	    widget = (xitk_widget_t *) xitk_list_first_content (wl->l);
	  
	  if(widget && (widget->type & WIDGET_FOCUSABLE) && (widget->enable) && (widget->visible))
	    found = 1;
	  
	} while(!found);
	
	return widget;
      }
      else
	if((widget->type & WIDGET_FOCUSABLE) && (widget->enable) && (widget->visible))
	  found = 1;
      
    }
    else if((widget->type & WIDGET_FOCUSABLE) && (widget->enable) && (widget->visible))
      found = 1;
    
  } while(!found);
  
  return widget;
}

/*
 *
 */
void xitk_set_focus_to_next_widget(xitk_widget_list_t *wl, int backward) {
  xitk_widget_t   *first_widget, *widget;
  widget_event_t   event;

  if(!wl) {
    XITK_WARNING("widget list is NULL.\n");
    return;
  }

  if(backward) 
    first_widget = widget = (xitk_widget_t *) xitk_list_last_content (wl->l);
  else
    first_widget = widget = (xitk_widget_t *) xitk_list_first_content (wl->l);
  
  while (widget && wl && wl->l && wl->win && wl->gc) {
    
    /* There is no widget focused yet */
    if(!wl->widget_focused) {
      
      if((!(first_widget->type & WIDGET_FOCUSABLE)) || (!first_widget->enable) || (!first_widget->visible)) {
	first_widget = get_next_focusable_widget(wl, backward);
      }
      
      wl->widget_focused = first_widget;
      
    __focus_the_widget:
      
      /* Kill (hide) tips */
      xitk_tips_tips_kill(wl->widget_focused);

      if ((wl->widget_focused->type & WIDGET_FOCUSABLE) &&
	  (wl->widget_focused->enable == WIDGET_ENABLE)) {
	event.type  = WIDGET_EVENT_FOCUS;
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
      	  (void) wl->widget_focused->event(wl->widget_focused, &event, &result);

	  event.type = WIDGET_EVENT_PAINT;
	  (void) wl->widget_focused->event(wl->widget_focused, &event, NULL);
      	}
	
      }
      return;
    }
    
    /* next widget will be focused */
    if(widget == wl->widget_focused) {
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
    
    widget = get_next_focusable_widget(wl, backward);
  }

}

/*
 *
 */
void xitk_set_focus_to_widget(xitk_widget_t *w) {
  xitk_widget_t       *widget;
  widget_event_t       event;
  xitk_widget_list_t  *wl = w->wl;
  
  if(!wl) {
    XITK_WARNING("widget list is NULL.\n");
    return;
  }

  widget = (xitk_widget_t *) xitk_list_first_content (wl->l);
  
  while (widget && wl && wl->l && wl->win && wl->gc && (widget != w))
    widget = (xitk_widget_t *) xitk_list_next_content(wl->l);
  
  if(widget) {
    
    if(wl->widget_focused) {
      
      /* Kill (hide) tips */
      xitk_tips_tips_kill(wl->widget_focused);
      
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

      if(wl->widget_focused->type & WIDGET_CLICKABLE)
	wl->widget_under_mouse = wl->widget_focused;

      if((wl->widget_focused->type & WIDGET_CLICKABLE) &&
	 (wl->widget_focused->type & WIDGET_TYPE_MASK) == WIDGET_TYPE_INPUTTEXT) {
	widget_event_result_t  result;
	
	event.type           = WIDGET_EVENT_CLICK;
	event.x              = wl->widget_focused->x + 1;
	event.y              = wl->widget_focused->y + 1;
	event.button_pressed = LBUTTON_DOWN;
	event.button         = AnyButton;
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
 * Call notify_keyevent, if exist, of specified plugin. This pass an X11 XEvent.
 */
void xitk_send_key_event(xitk_widget_t *w, XEvent *xev) {
  widget_event_t event;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  event.type   = WIDGET_EVENT_KEY_EVENT;
  event.xevent = xev;
  
  (void) w->event(w, &event, NULL);
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

  w->x = x;
  w->y = y;

  return 1;
}

/*
 * Get position of a widget.
 */
int xitk_get_widget_pos(xitk_widget_t *w, int *x, int *y) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
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

/*
 * Enable a widget.
 */
void xitk_enable_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  w->enable = WIDGET_ENABLE;
  if(w->type & WIDGET_GROUP) {
    widget_event_t  event;

    event.type = WIDGET_EVENT_ENABLE;
    (void) w->event(w, &event, NULL);
  }
}

/*
 * Disable a widget.
 */
void xitk_disable_widget(xitk_widget_t *w) {
  widget_event_t  event;

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  
  if(w->wl != NULL) {
    
    if((w->wl->widget_under_mouse != NULL) && (w == w->wl->widget_under_mouse)) {
      /* Kill (hide) tips */
      xitk_tips_tips_kill(w);
    }
    
    if((w->type & WIDGET_FOCUSABLE) && 
       (w->enable == WIDGET_ENABLE) && (w->have_focus != FOCUS_LOST)) {

      event.type  = WIDGET_EVENT_FOCUS;
      event.focus = FOCUS_LOST;
      (void) w->event(w, &event, NULL);
      
      event.type = WIDGET_EVENT_PAINT;
      (void) w->event(w, &event, NULL);
    }
  }
  
  w->enable = !WIDGET_ENABLE;
  if(w->type & WIDGET_GROUP) {
    event.type = WIDGET_EVENT_ENABLE;
    (void) w->event(w, &event, NULL);
  }
}

/*
 *
 */
void xitk_free_widget(xitk_widget_t *w) {
  widget_event_t event;
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  
  event.type = WIDGET_EVENT_DESTROY;
  (void) w->event(w, &event, NULL);
  
  xitk_tips_tips_kill(w);
  XITK_FREE(w->tips_string);
  XITK_FREE(w);
  w = NULL;
  
}

/*
 * Destroy a widget.
 */
void xitk_destroy_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_hide_widget(w);
  xitk_stop_widget(w);
  xitk_disable_widget(w);

  xitk_free_widget(w);
}

/*
 * Destroy widgets from widget list.
 */
void xitk_destroy_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
  
  mywidget = (xitk_widget_t *) xitk_list_last_content (wl->l);

  while(mywidget) {
    
    xitk_destroy_widget(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_prev_content (wl->l);
  }
}

/*
 * Return the struct of color names/values.
 */
xitk_color_names_t *gui_get_color_names(void) {

  return xitk_color_names;
}

/*
 * Return a xitk_color_name_t type from a string color.
 */
xitk_color_names_t *xitk_get_color_name(char *color) {
  xitk_color_names_t *cn = NULL;
  xitk_color_names_t *pcn = NULL;

  if(color == NULL) {
    XITK_WARNING("color was NULL.\n");
    return NULL;
  }

  /* Hexa X triplet */
  if((strncasecmp(color, "#", 1) <= 0) && (strlen(color) == 7)) {
    char  *lowercolorname;
    char  *p;
    
    p = strdup((color + 1));

    /* 
     * convert copied color to lowercase, this can avoid some problems
     * with _buggy_ sscanf(), who knows ;-)
    */
    lowercolorname = p;
    while(*lowercolorname != '\0') {
      *lowercolorname = tolower(*lowercolorname);
      lowercolorname++;
    }
    lowercolorname = p;

    cn = (xitk_color_names_t *) xitk_xmalloc(sizeof(xitk_color_names_t));
    
    if((sscanf(lowercolorname, 
	       "%2x%2x%2x", &cn->red, &cn->green, &cn->blue)) != 3) {
      XITK_WARNING("sscanf() failed: %s\n", strerror(errno));
      cn->red = cn->green = cn->blue = 0;
    }

    cn->colorname = strdup(color);
    
    XITK_FREE(p);

    return cn;
  } 
  else {
    for(pcn = xitk_color_names; pcn->colorname != NULL; pcn++) {
      if(!strncasecmp(pcn->colorname, color, strlen(pcn->colorname))) {

	cn = (xitk_color_names_t *) xitk_xmalloc(sizeof(xitk_color_names_t));

	cn->red       = pcn->red;
	cn->green     = pcn->green;
	cn->blue      = pcn->blue;
	cn->colorname = strdup(pcn->colorname);

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

  XITK_FREE(color->colorname);
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

  w->running = 0;
  
}

/*
 * (Re)Start a widget.
 */
void xitk_start_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  w->running = 1;
  
}

/*
 * Stop widgets from widget list.
 */
void xitk_stop_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
    
  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {
    
    xitk_stop_widget(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
}

/*
 * Show a widget.
 */
void xitk_show_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  if(w->visible == 0) {
    widget_event_t  event;
    
    w->visible = 1;
    
    event.type = WIDGET_EVENT_PAINT;
    (void) w->event(w, &event, NULL);
  }
}

/*
 * Show widgets from widget list.
 */
void xitk_show_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
    
  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {
    
    if(mywidget->visible == -1) 
      mywidget->visible = 0;
    else
      xitk_show_widget(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
}

void xitk_enable_and_show_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_enable_widget(w);
  xitk_show_widget(w);
}

/*
 * Hide a widget.
 */
void xitk_hide_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  if(w->visible == 1) {
    widget_event_t event;
    
    w->visible = 0;

    xitk_tips_tips_kill(w);
    
    event.type = WIDGET_EVENT_PAINT;
    (void) w->event(w, &event, NULL);
  }
}

/*
 * Hide widgets from widget list..
 */
void xitk_hide_widgets(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
    
  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {

    if(mywidget->visible == 0)
      mywidget->visible = -1;
    else
      xitk_hide_widget(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
}

void xitk_disable_and_hide_widget(xitk_widget_t *w) {

  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_disable_widget(w);
  xitk_hide_widget(w);
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

/*
 *
 */
void xitk_set_widget_tips(xitk_widget_t *w, char *str) {

  if(!w || !str) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_tips(w, str);
}

/*
 *
 */
void xitk_set_widget_tips_default(xitk_widget_t *w, char *str) {

  if(!w || !str) {
    XITK_WARNING("widget is NULL\n");
    return;
  }

  xitk_tips_set_tips(w, str);
  xitk_tips_set_timeout(w, TIPS_TIMEOUT);
}

/*
 *
 */
void xitk_set_widget_tips_and_timeout(xitk_widget_t *w, char *str, unsigned int timeout) {

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
void xitk_enable_widget_tips(xitk_widget_t *w) {
  
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  
  xitk_tips_set_timeout(w, TIPS_TIMEOUT);
}

/*
 *
 */
void xitk_disable_widgets_tips(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
  
  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {
    
    xitk_disable_widget_tips(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
}

/*
 *
 */
void xitk_enable_widgets_tips(xitk_widget_list_t *wl) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }
  
  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {
    
    if(mywidget->tips_string)
      xitk_enable_widget_tips(mywidget);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
  }
}

/*
 *
 */
void xitk_set_widgets_tips_timeout(xitk_widget_list_t *wl, unsigned long timeout) {
  xitk_widget_t *mywidget;
  
  if(!wl) {
    XITK_WARNING("widget list was NULL.\n");
    return;
  }

  mywidget = (xitk_widget_t *) xitk_list_first_content (wl->l);

  while(mywidget) {
    
    if(mywidget->tips_string)
      xitk_tips_set_timeout(mywidget, timeout);
    
    mywidget = (xitk_widget_t *) xitk_list_next_content (wl->l);
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

int xitk_is_mouse_over_widget(Display *display, Window window, xitk_widget_t *w) {
  Bool            ret;
  Window          root_window, child_window;
  int             root_x, root_y, win_x, win_y;
  unsigned int    mask;
  int             retval = 0;
  
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return 0;
  }
  if(window == None) {
    XITK_WARNING("window is None\n");
    return 0;
  }
  
  XLOCK(display);
  ret = XQueryPointer(display, window, 
		      &root_window, &child_window, &root_x, &root_y, &win_x, &win_y, &mask);
  XUNLOCK(display);
  
  if((ret == False) ||
     ((child_window == None) && (win_x == 0) && (win_y == 0))) {
    retval = 0;
  }
  else {
    
    if(((win_x >= w->x) && (win_x <= (w->x + w->width))) && 
       ((win_y >= w->y) && (win_y <= (w->y + w->height)))) {
      retval = 1;
    }
    
  }
  
  return retval;
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
  
  XLOCK(display);
  ret = XQueryPointer(display, window, 
		      &root_window, &child_window, &root_x, &root_y, &win_x, &win_y, &mask);
  XUNLOCK(display);
  
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

int xitk_widget_list_set(xitk_widget_list_t *wl, int param, void *data) {

  if(!wl) {
    XITK_WARNING("List is NULL\n");
    return 0;
  }

  switch(param) {
  case WIDGET_LIST_GC:
    wl->gc = (GC) data;
    break;
  case WIDGET_LIST_WINDOW:
    wl->win = (Window) data;
    break;
  case WIDGET_LIST_LIST:
    wl->l = (xitk_list_t *) data;
    break;
  default:
    XITK_WARNING("Unknown param %d\n", param);
    return 0;
    break;
  }
  return 1;
}

void *xitk_widget_list_get(xitk_widget_list_t *wl, int param) {
  void *data = NULL;

  if(!wl) {
    XITK_WARNING("List is NULL\n");
    return NULL;
  }

  switch(param) {
  case WIDGET_LIST_GC:
    if(wl->gc)
      data = (void *) wl->gc;
    else
      XITK_WARNING("widget list GC unset\n");
    break;
  case WIDGET_LIST_WINDOW:
    if(wl->win)
      data = (void *) wl->win;
    else
      XITK_WARNING("widget list window unset\n");
    break;
  case WIDGET_LIST_LIST:
    if(wl->l)
      data = (void *) wl->l;
    else
      XITK_WARNING("widget list list unset\n");
    break;
  default:
    XITK_WARNING("Unknown param %d\n", param);
    break;
  }
  
  return data;
}

void xitk_widget_keyable(xitk_widget_t *w, int keyable) {
  if(!w) {
    XITK_WARNING("widget is NULL\n");
    return;
  }
  
  if(keyable)
    w->type |= WIDGET_KEYABLE;
  else
    w->type &= ~WIDGET_KEYABLE;
}
