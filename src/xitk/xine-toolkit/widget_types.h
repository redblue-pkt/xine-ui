/* 
 * Copyright (C) 2000-2002 the xine project
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

#ifndef HAVE_XITK_WIDGET_TYPES_H
#define HAVE_XITK_WIDGET_TYPES_H

/*
 *  1 <widget group >                         0x80000000   <mask 0x80000000> WIDGET_GROUP
 *   1 <The groupped widget>                  0x40000000   <mask 0x40000000> WIDGET_GROUP_WIDGET
 *    111111111111111 <group types>           <15 types>   <mask 0x3FFF8000> WIDGET_GROUP_(MASK)
 *                   111111111111111 <widget> <15 types>   <mask 0x00007FFF> WIDGET_TYPE_(MASK)
*/
/* Group of widgets widget */
#define WIDGET_GROUP              0x80000000
/* Grouped widget, itself */
#define WIDGET_GROUP_WIDGET       0x40000000

/* Grouped widgets */
#define WIDGET_GROUP_MASK         0x3FFF8000 
#define WIDGET_GROUP_BROWSER      0x00080000
#define WIDGET_GROUP_FILEBROWSER  0x00100000
#define WIDGET_GROUP_MRLBROWSER   0x00200000
#define WIDGET_GROUP_COMBO        0x00400000
#define WIDGET_GROUP_TABS         0x00800000
#define WIDGET_GROUP_INTBOX       0x01000000

/* Real widgets. */
#define WIDGET_TYPE_MASK          0x00007FFF
#define WIDGET_TYPE_BUTTON        0x00000001
#define WIDGET_TYPE_LABELBUTTON   0x00000002
#define WIDGET_TYPE_LABEL         0x00000003
#define WIDGET_TYPE_SLIDER        0x00000004
#define WIDGET_TYPE_CHECKBOX      0x00000005
#define WIDGET_TYPE_IMAGE         0x00000006
#define WIDGET_TYPE_INPUTTEXT     0x00000007

#endif
