/* 
 * Copyright (C) 2003 by Fredrik Noring
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
 */

typedef void(*fbxine_callback_t)(void);

struct fbxine_callback
{
	struct fbxine_callback *next;
	fbxine_callback_t func;
};

void fbxine_register_exit(struct fbxine_callback *callback,
			  fbxine_callback_t func);
void fbxine_register_abort(struct fbxine_callback *callback,
			   fbxine_callback_t func);
void fbxine_do_exit(void);
void fbxine_do_abort(void);
