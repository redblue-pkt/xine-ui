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

#ifndef __OSD_H__
#define __OSD_H__

#define OSD_BAR_PROGRESS 1
#define OSD_BAR_POS      2
#define OSD_BAR_STEPPER  3


void osd_init(void);
void osd_deinit(void);
void osd_update(void);
void osd_stream_infos(void);
void osd_update_status(void);
void osd_stream_position(int pos);
  /* see OSD_BAR_* */
void osd_draw_bar(char *title, int min, int max, int val, int type);
#endif
