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
 * Initial version by Fredrik Noring, January 2003, based on the xitk
 * and dfb sources.
 */

#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "actions.h"

void action_pause(void)
{
	xine_set_param(fbxine.stream, XINE_PARAM_SPEED,
		       xine_get_param(fbxine.stream, XINE_PARAM_SPEED) ==
		       XINE_SPEED_PAUSE ? XINE_SPEED_NORMAL:XINE_SPEED_PAUSE);
}

int get_pos_length(xine_stream_t *stream, int *pos, int *time, int *length)
{
	int t = 0, ret = 0;

	if(stream && xine_get_status(stream) == XINE_STATUS_PLAY)
		for(;;)
		{
			ret = xine_get_pos_length(stream, pos, time, length);
			if(ret || 10 < ++t)
				break;
			xine_usec_sleep(100000); /* wait before trying again */
		}
	
	return ret;
}

static int play(xine_stream_t *stream, 
		int start_pos, int start_time_in_secs, int update_mmk)
{
	return xine_play(stream, start_pos, start_time_in_secs * 1000);
}

static void action_play(void)
{
	play(fbxine.stream, 0, 0, 0);
}

static void *seek_relative_thread(void *data)
{
	int sec, off_sec = (int)data;
	
	pthread_detach(pthread_self());
	
	if(get_pos_length(fbxine.stream, 0, &sec, 0))
	{
		sec /= 1000;
		
		if(sec + off_sec < 0)
			sec = 0;
		else
			sec += off_sec;
		
		play(fbxine.stream, 0, sec, 1);
	}
	
	fbxine.ignore_next = 0;
	
	pthread_exit(0);
	return 0;
}

void action_seek_relative(int off_sec)
{
	static pthread_t seek_thread;
	int err;
	
	if(fbxine.ignore_next ||
	   !xine_get_stream_info(fbxine.stream, XINE_STREAM_INFO_SEEKABLE) ||
	   xine_get_status(fbxine.stream) != XINE_STATUS_PLAY)
		return;
    
	fbxine.ignore_next = 1;

	err = pthread_create(&seek_thread, 0, seek_relative_thread,
			     (void *)off_sec);
	if(!err)
		return;

	fprintf(stderr, "Failed to create action_seek_relative thread.\n");
	abort();
}

void do_action(int action)
{
	if(action & ACTID_IS_INPUT_EVENT)
	{
		xine_event_t xine_event;
		
		xine_event.type = action & ~ACTID_IS_INPUT_EVENT;
		xine_event.data_length = 0;
		xine_event.data = 0;
		xine_event.stream = fbxine.stream;
		gettimeofday(&xine_event.tv, NULL);
		
		xine_event_send(fbxine.stream, &xine_event);
		return;
	}
	
	switch(action)
	{
		case ACTID_SEEK_REL_m60:
			action_seek_relative(-60);
			break;
			
		case ACTID_SEEK_REL_m15:
			action_seek_relative(-15);
			break;
			
		case ACTID_SEEK_REL_p60:
			action_seek_relative(60);
			break;
			
		case ACTID_SEEK_REL_p15:
			action_seek_relative(15);
			break;
			
		case ACTID_SEEK_REL_m30:
			action_seek_relative(-30);
			break;
			
		case ACTID_SEEK_REL_m7:
			action_seek_relative(-7);
			break;
			
		case ACTID_SEEK_REL_p30:
			action_seek_relative(30);
			break;
			
		case ACTID_SEEK_REL_p7:
			action_seek_relative(7);
			break;
			
		case ACTID_PAUSE:
			action_pause();
			break;
			
		case ACTID_PLAY:
			action_play();
			break;
	}
}
