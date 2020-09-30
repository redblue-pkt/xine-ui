/*
 * Copyright (C) 2003 by Dirk Meyer
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

#ifndef POST_HH
#define POST_HH

void vpplugin_rewire_posts(void);
void applugin_rewire_posts(void);
void vpplugin_parse_and_store_post(const char *post);
void applugin_parse_and_store_post(const char *post);
void post_deinterlace(void);
void post_deinterlace_init(const char *deinterlace_post);

#endif

/* end of post.h */
