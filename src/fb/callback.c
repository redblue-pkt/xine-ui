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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>

#include "callback.h"

static struct fbxine_callback *fbxine_fifo_exit = 0;
static struct fbxine_callback *fbxine_fifo_abort = 0;

static void fbxine_register_callback(struct fbxine_callback **top,
				     struct fbxine_callback *callback,
				     fbxine_callback_t func)
{
	callback->next = *top;
	*top = callback;
	callback->func = func;
}

static void fbxine_do_callbacks(struct fbxine_callback *callback)
{
	while(callback)
	{
		callback->func();
		callback = callback->next;
	}
}

void fbxine_register_exit(struct fbxine_callback *callback,
			  fbxine_callback_t func)
{
	fbxine_register_callback(&fbxine_fifo_exit, callback, func);
}

void fbxine_register_abort(struct fbxine_callback *callback,
			   fbxine_callback_t func)
{
	fbxine_register_callback(&fbxine_fifo_abort, callback, func);
}

void fbxine_do_exit(void)
{
	struct fbxine_callback *tmp;

	tmp = fbxine_fifo_exit;
	fbxine_fifo_exit = 0;
	fbxine_do_callbacks(tmp);
}

void __attribute__((noreturn)) fbxine_do_abort(void)
{
	struct fbxine_callback *tmp;

	tmp = fbxine_fifo_abort;
	fbxine_fifo_abort = 0;
	fbxine_do_callbacks(tmp);
	exit(1);   /* Panic. */
}
