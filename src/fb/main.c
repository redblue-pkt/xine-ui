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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#ifdef HAVE_LIRC
#include "lirc.h"
#endif
#include "keys.h"
#include "options.h"

#define XINE_CONFIG_DIR  ".xine"
#define XINE_CONFIG_FILE "config2"

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

struct fbxine fbxine =
{
	tty_fd:        -1,
	video_port_id: "fb"
};

static void load_config(void)
{
	fbxine.configfile = getenv("XINERC");
	if(!fbxine.configfile)
	{
		fbxine.configfile = xine_xmalloc(strlen(xine_get_homedir())
						 + strlen(XINE_CONFIG_DIR) 
						 + strlen(XINE_CONFIG_FILE)
						 + 3);
		sprintf(fbxine.configfile, "%s/%s", xine_get_homedir(),
			XINE_CONFIG_DIR);
		mkdir(fbxine.configfile, 0755);
		sprintf(fbxine.configfile + strlen(fbxine.configfile), "/%s",
			XINE_CONFIG_FILE);
	}
	xine_config_load(fbxine.xine, fbxine.configfile);
}

static int check_version(void)
{
	int major, minor, sub;
	
	if(xine_check_version(1, 0, 0))
		return 1;

	xine_get_version(&major, &minor, &sub);
	fprintf(stderr, "Require at least xine library version 1.0.0, "
		"found %d.%d.%d.\n", major, minor, sub);
	return 0;
}

static int init_video(void)
{
	fbxine.video_port =
		xine_open_video_driver(fbxine.xine, fbxine.video_port_id,
				       XINE_VISUAL_TYPE_FB, (void *)&fbxine);
	if(!fbxine.video_port)
	{
		fprintf(stderr, "Video port failed.\n");
		return 0;
	}

	return 1;
}

static int init_audio(void)
{
	if(!fbxine.audio_port_id)
		fbxine.audio_port_id =
			xine_config_register_string(fbxine.xine, 
						    "audio.driver", "oss",
						    "audio driver to use",
						    0, 20, 0, 0);
	fbxine.audio_port =
		xine_open_audio_driver(fbxine.xine, fbxine.audio_port_id, 0);
	if(!fbxine.audio_port)
	{
		fprintf(stderr, "Audio port failed.\n");
		return 0;
	}
	
	return 1;
}

static int init_stream(void)
{
	fbxine.stream = xine_stream_new(fbxine.xine, fbxine.audio_port,
					fbxine.video_port);
	fbxine.event_queue = xine_event_new_queue(fbxine.stream);  

	if((!xine_open(fbxine.stream, fbxine.mrl[fbxine.current_mrl])) ||
	   (!xine_play(fbxine.stream, 0, 0)))
	{
		fprintf(stderr, "Unable to open MRL '%s'\n",
			fbxine.mrl[fbxine.current_mrl]);
		return 0;
	}

	return 1;
}

static int init_xine(void)
{
	fbxine.xine = xine_new();
	if(!fbxine.xine)
	{
		fprintf(stderr, "Failed to call xine_new.\n");
		return 0;
	}
	load_config();
	xine_init(fbxine.xine);
	return 1;
}

int main(int argc, char *argv[])
{
	int exit_code = 1;
	
	if(!check_version())
		goto err_version;
	if(!init_xine())
		goto err_xine;
	switch(parse_options(argc, argv))
	{
		case 0:
			exit_code = 0;
			goto exit_option;
		case -1:
			goto exit_option;
	}
	
	fbxine.running = 1;
	
	if(!init_video())
		goto err_video;
	if(!init_audio())
		goto err_audio;
	if(!init_stream())
		goto err_stream;
	if(!init_keyboard())
		goto err_keyboard;
#ifdef HAVE_LIRC
	init_lirc();
#endif

	wait_for_key();

	exit_code = 0;
	
#ifdef HAVE_LIRC
	exit_lirc();
#endif
	exit_keyboard();
err_keyboard:
	xine_close(fbxine.stream);
	xine_event_dispose_queue(fbxine.event_queue);
	xine_dispose(fbxine.stream);
err_stream:
	xine_close_audio_driver(fbxine.xine, fbxine.audio_port);
err_audio:
	xine_close_video_driver(fbxine.xine, fbxine.video_port);
err_video:
exit_option:
	xine_exit(fbxine.xine);
err_xine:
err_version:
	return exit_code;
}
