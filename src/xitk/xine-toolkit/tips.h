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

#ifndef HAVE_XITK_TIPS_H
#define HAVE_XITK_TIPS_H

#define TIPS_TIMEOUT 500

void xitk_tips_create(xitk_widget_t *w, xitk_widget_list_t *wl);
void xitk_tips_tips_kill(xitk_widget_t *w);
void xitk_tips_set_timeout(xitk_widget_t *w, unsigned long timeout);
void xitk_tips_set_tips(xitk_widget_t *w, char *str);

#endif
