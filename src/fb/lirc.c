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
 * lirc-specific stuff
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>
#endif

#include "main.h"
#include "keys.h"
#include "actions.h"

#ifdef HAVE_LIRC

#ifdef DEBUG
#define LIRC_VERBOSE 1
#else
#define LIRC_VERBOSE 0
#endif

typedef struct
{
	struct lirc_config   *config;
	int                   fd;
	pthread_t             thread;
} _lirc_t;

static _lirc_t lirc;

static void *xine_lirc_loop(void *dummy)
{
	kbinding_entry_t *k;
	char *code, *c;
	int ret;
	
	pthread_detach(pthread_self());
	
	while(fbxine.running && lirc_nextcode(&code) == 0)
	{
		pthread_testcancel();
		
		if(!code) 
			continue;
		
		pthread_testcancel();

		for(;;)
		{
			ret = lirc_code2char(lirc.config, code, &c);
			if(ret || !c)
				break;
			k = kbindings_lookup_action(c);
			if(k)
				do_action(k->action_id);
		}
		free(code);
		
		if(ret == -1) 
			break;
	}
	
	pthread_exit(0);
}

void init_lirc(void)
{
	if((lirc.fd = lirc_init("xine", LIRC_VERBOSE)) == -1)
	{
		fbxine.lirc_enable = 0;
		return;
	}

	if(lirc_readconfig(0, &lirc.config, 0) != 0)
	{
		fbxine.lirc_enable = 0;
		return;
	}
	
	fbxine.lirc_enable = 1;
	
	if(fbxine.lirc_enable)
		pthread_create(&lirc.thread, 0, xine_lirc_loop, 0);
}

void exit_lirc(void)
{
	pthread_cancel(lirc.thread);
	
	if(fbxine.lirc_enable)
	{
		lirc_freeconfig(lirc.config);
		lirc_deinit();
	}
}
#endif
