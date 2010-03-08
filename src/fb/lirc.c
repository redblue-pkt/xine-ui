/* 
 * Copyright (C) 2000-2003 the xine project and Fredrik Noring
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
 * lirc-specific stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>

#include <lirc/lirc_client.h>

#include "main.h"
#include "keys.h"
#include "actions.h"

int fbxine_init_lirc(void);

static __attribute__((noreturn)) void *lirc_loop(void *dummy)
{
	char *code, *c;
	int k, ret;
	
	pthread_detach(pthread_self());
	
	while(lirc_nextcode(&code) == 0)
	{
		pthread_testcancel();
		
		if(!code) 
			continue;

		for(;;)
		{
			ret = lirc_code2char(fbxine.lirc.config, code, &c);
			if(ret || !c)
				break;
			k = default_command_action(c);
			if(k)
				do_action(k);
		}
		free(code);
		
		if(ret == -1) 
			break;
	}
	
	pthread_exit(0);
}

static void exit_lirc(void)
{
	pthread_cancel(fbxine.lirc.thread);

	lirc_freeconfig(fbxine.lirc.config);
	lirc_deinit();
}

int fbxine_init_lirc(void)
{
	static struct fbxine_callback exit_callback;
	
	if((fbxine.lirc.fd = lirc_init("xine", 0)) == -1)
		return 0;
	if(lirc_readconfig(0, &fbxine.lirc.config, 0) != 0)
		return 0;
	
	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_lirc);
	
	pthread_create(&fbxine.lirc.thread, 0, lirc_loop, 0);
	return 1;
}
