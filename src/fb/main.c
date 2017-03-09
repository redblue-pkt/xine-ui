/* 
 * Copyright (C) 2003 by Fredrik Noring
 * Copyright (C) 2003-2017 the xine project
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
 * Initial version by Fredrik Noring, January 2003, based on the xitk
 * and dfb sources.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>

#ifdef HAVE_LIRC
#include "lirc.h"
#endif

#include "main.h"
#include "keys.h"
#include "stdctl.h"
#include "options.h"
#include "osd.h"
#include "post.h"
#include "globals.h"
#include "config_wrapper.h"

#define XINE_CONFIG_DIR  ".xine"
#define XINE_CONFIG_FILE "config"

struct fbxine fbxine =
{
	.tty_fd =        -1,
	.video_port_id = "fb",

	.post_video_elements = NULL,
	.post_video_elements_num = 0,
	.post_video_enable = 1,

	.post_audio_elements = NULL,
	.post_audio_elements_num = 0,
	.post_audio_enable = 1,

	.deinterlace_plugin = NULL,
	.deinterlace_elements = NULL,
	.deinterlace_elements_num = 0,
	.deinterlace_enable = 0,

	.exit_cond = PTHREAD_COND_INITIALIZER
};

static void load_config(void)
{
	__xineui_global_config_file = getenv("XINERC");
	if(!__xineui_global_config_file)
	{
		__xineui_global_config_file = malloc(strlen(xine_get_homedir())
						 + strlen(XINE_CONFIG_DIR) 
						 + strlen(XINE_CONFIG_FILE)
						 + 3);
		sprintf(__xineui_global_config_file, "%s/%s", xine_get_homedir(),
			XINE_CONFIG_DIR);
		mkdir(__xineui_global_config_file, 0755);
		sprintf(__xineui_global_config_file + strlen(__xineui_global_config_file), "/%s",
			XINE_CONFIG_FILE);
	}
	xine_config_load(__xineui_global_xine_instance, __xineui_global_config_file);
        xine_engine_set_param(__xineui_global_xine_instance, XINE_ENGINE_PARAM_VERBOSITY, __xineui_global_verbosity);
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

static void event_listener(void *user_data, const xine_event_t *event)
{
	switch(event->type)
	{
		case XINE_EVENT_UI_PLAYBACK_FINISHED:
			pthread_mutex_lock(&fbxine.mutex);
			pthread_cond_signal(&fbxine.exit_cond);
			pthread_mutex_unlock(&fbxine.mutex);
			break;
		default:
			break;
	}
}

static int open_and_play(const char *mrl)
{

        if(!strncasecmp(mrl, "cfg:/", 5)) 
	{
		config_mrl(mrl);
		return 0;
	}

	if((!xine_open(fbxine.stream, fbxine.mrl[fbxine.current_mrl])) ||
	   (!xine_play(fbxine.stream, 0, 0)))
	{
		fprintf(stderr, "Unable to open MRL '%s'\n",
			fbxine.mrl[fbxine.current_mrl]);
		return 0;
	}

	return 1;
}

static void exit_video(void)
{
	xine_close_video_driver(__xineui_global_xine_instance, fbxine.video_port);
}

static int init_video(void)
{
	static struct fbxine_callback exit_callback;

	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_video);
	
	if (!strcmp(fbxine.video_port_id, "dxr3"))
	    fbxine.video_port =
	        xine_open_video_driver(__xineui_global_xine_instance, fbxine.video_port_id,
				       XINE_VISUAL_TYPE_X11, NULL);
	else if (!strcmp(fbxine.video_port_id, "none"))
	    fbxine.video_port =
	        xine_open_video_driver(__xineui_global_xine_instance, fbxine.video_port_id,
				       XINE_VISUAL_TYPE_NONE, NULL);
	else
	    fbxine.video_port =
	        xine_open_video_driver(__xineui_global_xine_instance, fbxine.video_port_id,
				       XINE_VISUAL_TYPE_FB, NULL);
	if(!fbxine.video_port)
	{
	    fprintf(stderr, "Video port failed.\n");
	    return 0;
	}
	

	return 1;
}

static void exit_audio(void)
{
	xine_close_audio_driver(__xineui_global_xine_instance, fbxine.audio_port);
}

static int init_audio(void)
{
	static struct fbxine_callback exit_callback;

	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_audio);

	if(!fbxine.audio_port_id)
		fbxine.audio_port_id =
			xine_config_register_string(__xineui_global_xine_instance, 
						    "audio.driver", "oss",
						    "audio driver to use",
						    0, 20, 0, 0);
	fbxine.audio_port =
		xine_open_audio_driver(__xineui_global_xine_instance, fbxine.audio_port_id, 0);
	if(!fbxine.audio_port)
	{
		fprintf(stderr, "Audio port failed.\n");
		return 0;
	}
	
	return 1;
}

static void exit_stream(void)
{
        xine_close(fbxine.stream);
	xine_event_dispose_queue(fbxine.event_queue);
	xine_dispose(fbxine.stream);
}

static int init_stream(void)
{
	static struct fbxine_callback exit_callback;

	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_stream);

	fbxine.stream = xine_stream_new(__xineui_global_xine_instance, fbxine.audio_port,
					fbxine.video_port);
	xine_set_param(fbxine.stream, XINE_PARAM_VERBOSITY, __xineui_global_verbosity);

	fbxine.event_queue = xine_event_new_queue(fbxine.stream);  
  
	xine_event_create_listener_thread(fbxine.event_queue, 
	                                  event_listener, NULL);

	return 1;
}

static void exit_xine(void)
{
	xine_exit(__xineui_global_xine_instance);
}

static int init_xine(void)
{
	static struct fbxine_callback exit_callback;

	fbxine_register_exit(&exit_callback, (fbxine_callback_t)exit_xine);
	
	__xineui_global_xine_instance = xine_new();
	if(!__xineui_global_xine_instance)
	{
		fprintf(stderr, "Failed to call xine_new.\n");
		return 0;
	}
	load_config();
	xine_init(__xineui_global_xine_instance);
	return 1;
}

static void wait_for_exit(void)
{
	pthread_cond_wait(&fbxine.exit_cond, &fbxine.mutex);
}

void fbxine_exit(void)
{
        osd_deinit();
	pthread_mutex_lock(&fbxine.mutex);
	fbxine.current_mrl = fbxine.num_mrls;
	pthread_cond_signal(&fbxine.exit_cond);
	pthread_mutex_unlock(&fbxine.mutex);
}

static int fbxine_init(int argc, char **argv)
{
	if(!check_version())
		return 0;
	if(!init_xine())
		return 0;
	switch(parse_options(argc, argv))
	{
		case 0:
		case -1:
			return 0;
	}

	if (stdctl)
	        fbxine_init_stdctl();
	else 
	        if(!fbxine_init_keyboard())
		        return 0;;
	if(!init_video())
		return 0;
	if(!init_audio())
		return 0;
	if(!init_stream())
		return 0;
#ifdef HAVE_LIRC
	if (!no_lirc)
	  fbxine_init_lirc();
#endif

	/* Initialize posts, if required */
	if(pplugins_num) {
	  char             **plugin = pplugins;
	  
	  while(*plugin) {
	    vpplugin_parse_and_store_post((const char *) *plugin);
	    applugin_parse_and_store_post((const char *) *plugin);
	    printf("1\n");
	    
	    plugin++;
	  }
    
	  vpplugin_rewire_posts();
	  applugin_rewire_posts();
	}

	post_deinterlace_init(NULL);

	if (fbxine.deinterlace_enable)
	  post_deinterlace();

	osd_init();
	fbxine.osd.enabled = 1;
	return 1;
}

static void install_abort(void)
{
	static const int trapped[] = {
		SIGINT, SIGQUIT, SIGILL,
		SIGFPE, SIGKILL, SIGBUS,
		SIGSEGV, SIGSYS, SIGPIPE,
		SIGTERM,
#ifdef SIGSTKFLT
		SIGSTKFLT,
#endif
	};
	size_t i;
	
	for(i = 0; i < sizeof(trapped)/sizeof(int); i++)
		signal(trapped[i], (void(*)(int))fbxine_do_abort);
}

int main(int argc, char *argv[])
{
	int exit_code = 1;
	
	install_abort();
	
	pthread_mutex_lock(&fbxine.mutex);
	if(fbxine_init(argc, argv))
	{
		while(fbxine.current_mrl < fbxine.num_mrls) 
		{
			if(open_and_play(fbxine.mrl[fbxine.current_mrl]))
			{
				wait_for_exit();
				xine_close(fbxine.stream);
			}
			fbxine.current_mrl++;
		}
		exit_code = 0;
	}
	fbxine_do_exit();
	pthread_mutex_unlock(&fbxine.mutex);
	
	return exit_code;
}
