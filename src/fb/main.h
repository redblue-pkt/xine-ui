/*
 * Copyright (C) 2003 by Fredrik Noring
 * Copyright (C) 2003-2020 the xine project
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
 * Initial version by Fredrik Noring January 2003.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "callback.h"

typedef struct {
  xine_post_t    *post;
  char           *name;
} post_element_t;


struct fbxine
{
	xine_stream_t            *stream;
	xine_video_port_t        *video_port;
	xine_audio_port_t        *audio_port;
	xine_event_queue_t       *event_queue;

	int                      tty_fd;
	struct termios           ti_save;
	struct termios           ti_cur;

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

        post_element_t          **post_video_elements;
        int                       post_video_elements_num;
        int                       post_video_enable;

        post_element_t          **post_audio_elements;
        int                       post_audio_elements_num;
        int                       post_audio_enable;

        const char               *deinterlace_plugin;
        post_element_t          **deinterlace_elements;
        int                       deinterlace_elements_num;
        int                       deinterlace_enable;


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

#define SAFE_FREE(x)            do {           \
                                    free((x)); \
                                    x = NULL;  \
                                } while(0)
