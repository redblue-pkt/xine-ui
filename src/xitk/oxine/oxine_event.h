/*
 * Copyright (C) 2002 Stefan Holst
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 *
 * datatypes and defines for events
 */

#ifndef HAVE_EVENT_H
#define HAVE_EVENT_H

/*
 * oxine global events and keycodes
 */

#define OXINE_EVENT_KEY             1
#define OXINE_EVENT_BUTTON          2
#define OXINE_EVENT_MOTION          3
#define OXINE_EVENT_FINISH          4
#define OXINE_EVENT_FORMAT_CHANGED  5
#define OXINE_EVENT_START           6

#define OXINE_KEY_NULL      0
#define OXINE_KEY_UP        1
#define OXINE_KEY_DOWN      2
#define OXINE_KEY_LEFT      3
#define OXINE_KEY_RIGHT     4
#define OXINE_KEY_PRIOR     5
#define OXINE_KEY_NEXT      6
#define OXINE_KEY_SELECT    7
#define OXINE_KEY_PLAY      8
#define OXINE_KEY_PAUSE     9
#define OXINE_KEY_STOP     10
#define OXINE_KEY_FORWARD  11
#define OXINE_KEY_REWIND   12
#define OXINE_KEY_0        13
#define OXINE_KEY_1        14
#define OXINE_KEY_2        15
#define OXINE_KEY_3        16
#define OXINE_KEY_4        17
#define OXINE_KEY_5        18
#define OXINE_KEY_6        19
#define OXINE_KEY_7        20
#define OXINE_KEY_8        21
#define OXINE_KEY_9        22
#define OXINE_KEY_MENU1    23
#define OXINE_KEY_MENU2    24
#define OXINE_KEY_MENU3    25
#define OXINE_KEY_MENU4    26
#define OXINE_KEY_MENU5    27
#define OXINE_KEY_MENU6    28
#define OXINE_KEY_MENU7    29
#define OXINE_KEY_ESCAPE   30
#define OXINE_KEY_FULLSCREEN 31
#define OXINE_KEY_VOLUP    32
#define OXINE_KEY_VOLDOWN  33
#define OXINE_KEY_VOLMUTE  34
#define OXINE_KEY_SEEK     35
#define OXINE_KEY_OSD      36
#define OXINE_KEY_EJECT    37
#define OXINE_KEY_PPLAY    38
#define OXINE_KEY_PL_NEXT  39
#define OXINE_KEY_PL_PREV  40
#define OXINE_KEY_SATURATION  41
#define OXINE_KEY_BRIGHTNESS  42
#define OXINE_KEY_CONTRAST    43
#define OXINE_KEY_HUE         44  
#define OXINE_KEY_SPU_OFFSET  45  
#define OXINE_KEY_SPU_CHANNEL   46          
#define OXINE_KEY_AUDIO_CHANNEL_LOGICAL  47
#define OXINE_KEY_AV_OFFSET     48          
#define OXINE_KEY_SPEED     49
#define OXINE_KEY_TOGGLE_ASPECT_RATIO     50  
#define OXINE_KEY_VO_DEINTERLACE      51   
#define OXINE_KEY_STREAM_POSITION     52

#define OXINE_BUTTON1       1
#define OXINE_BUTTON2       2
#define OXINE_BUTTON3       3
#define OXINE_BUTTON4       4
#define OXINE_BUTTON5       5

typedef struct {

  int              type;
  int              key, x, y;
  int              repeat;
  char             *data;

} oxine_event_t;

#endif
