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
 * Initial version by Fredrik Noring January 2003.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include <xine.h>
#include <xine/xineutils.h>

#ifdef XINE_ORT
#define xine_cfg_entry_t xine_config_entry_t
#endif

#include "callback.h"

typedef struct {
  xine_post_t    *post;
  char           *name;
} post_element_t;


struct fbxine
{
	xine_t                   *xine;
	xine_stream_t            *stream;
	xine_video_port_t        *video_port;
        xine_audio_port_t        *audio_port;
	xine_event_queue_t       *event_queue;

	int tty_fd;

	int lirc_enable;

	char                     *configfile;
	
	int                      ignore_status;
	char                     *mrl[1024];
	int                      num_mrls;
	int                      current_mrl;

	const char               *audio_port_id;
	const char               *video_port_id;
	int                      audio_channel;
	
	int                      screen_width;
	int                      screen_height;

	int                      debug;

	int                      ignore_next;
        post_element_t           **post_elements;
        int                      post_elements_num;
  
#ifdef HAVE_LIRC
	struct
	{
		struct lirc_config *config;
		int                fd;
		pthread_t          thread;
	} lirc;
#endif /* HAVE_LIRC */
	
	pthread_mutex_t mutex;
	pthread_cond_t exit_cond;

	pthread_t keyboard_thread;
        pthread_t          osd_thread;


  struct {
    int                     enabled;
    int                     timeout;

    xine_osd_t             *sinfo;
    int                     sinfo_visible;

    xine_osd_t             *bar[2];
    int                     bar_visible;

    xine_osd_t             *status;
    int                     status_visible;

    xine_osd_t             *info;
    int                     info_visible;

  } osd;


};

extern struct fbxine fbxine;
void fbxine_exit(void);
