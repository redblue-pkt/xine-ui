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
#include "osd.h"
#include "post.h"

#define GUI_NEXT     1
#define GUI_PREV     2
#define GUI_RESET    3


static const struct
{
	char *description;
	char *name;
	int action;
} default_actions[] =
{
	{ "Select next sub picture (subtitle) channel.",
		  "SpuNext", ACTID_SPU_NEXT },
	{ "Select previous sub picture (subtitle) channel.",
		  "SpuPrior", ACTID_SPU_PRIOR },
	{ "Select default sub picture (subtitle) channel.",
		  "SpuDefault", ACTID_SPU_DEFAULT },
	{ "Select next audio channel.",
		  "AudioChannelNext", ACTID_AUDIOCHAN_NEXT },
	{ "Select previous audio channel.",
		  "AudioChannelPrior", ACTID_AUDIOCHAN_PRIOR },
	{ "Set default audio channel.",
		  "AudioChannelDefault", ACTID_AUDIOCHAN_DEFAULT },
	{ "Playback pause toggle.",
		  "Pause", ACTID_PAUSE },
	{ "Aspect ratio values toggle.",
		  "ToggleAspectRatio", ACTID_TOGGLE_ASPECT_RATIO },
	{ "Display stream information using OSD.",
		  "OSDStreamInfos", ACTID_OSD_SINFOS },
	
	{ "Interlaced mode toggle.",
		  "ToggleInterleave", ACTID_TOGGLE_INTERLEAVE },
	{ "Quit the program.",
		  "Quit", ACTID_QUIT },
	{ "Start playback.",
		  "Play", ACTID_PLAY },
	
	{ "Visibility toggle of the setup window.",
		  "SetupShow", ACTID_SETUP },
	{ "Stop playback.",
		  "Stop", ACTID_STOP },
	{ "Select and play next mrl in the playlist.",
		  "NextMrl", ACTID_MRL_NEXT },
	{ "Select and play previous mrl in the playlist.",
		  "PriorMrl", ACTID_MRL_PRIOR },
	{ "Visibility toggle of the event sender window.",
		  "EventSenderShow", ACTID_EVENT_SENDER },
	{ "Edit selected mediamark.",
		  "MediamarkEditor", ACTID_MMKEDITOR },
	{ "Eject the current medium.",
		  "Eject", ACTID_EJECT },
	
	{ "Set position to numeric-argument%% of current stream.",
		  "SetPosition%", ACTID_SET_CURPOS },
	{ "Set position to beginning of current stream.",
		  "SetPosition0%", ACTID_SET_CURPOS_0 },
	{ "Set position to 10%% of current stream.",
		  "SetPosition10%", ACTID_SET_CURPOS_10 },
	{ "Set position to 20%% of current stream.",
		  "SetPosition20%", ACTID_SET_CURPOS_20 },
	{ "Set position to 30%% of current stream.",
		  "SetPosition30%", ACTID_SET_CURPOS_30 },
	{ "Set position to 40%% of current stream.",
		  "SetPosition40%", ACTID_SET_CURPOS_40 },
	{ "Set position to 50%% of current stream.",
		  "SetPosition50%", ACTID_SET_CURPOS_50 },
	{ "Set position to 60%% of current stream.",
		  "SetPosition60%", ACTID_SET_CURPOS_60 },
	{ "Set position to 70%% of current stream.",
		  "SetPosition70%", ACTID_SET_CURPOS_70 },
	{ "Set position to 80%% of current stream.",
		  "SetPosition80%", ACTID_SET_CURPOS_80 },
	{ "Set position to 90%% of current stream.",
		  "SetPosition90%", ACTID_SET_CURPOS_90 },
	{ "Enter the number 0.",
		  "Number0", ACTID_EVENT_NUMBER_0 },
	{ "Enter the number 1.",
		  "Number1", ACTID_EVENT_NUMBER_1 },
	{ "Enter the number 2.",
		  "Number2", ACTID_EVENT_NUMBER_2 },
	{ "Enter the number 3.",
		  "Number3", ACTID_EVENT_NUMBER_3 },
	{ "Enter the number 4.",
		  "Number4", ACTID_EVENT_NUMBER_4 },
	{ "Enter the number 5.",
		  "Number5", ACTID_EVENT_NUMBER_5 },
	{ "Enter the number 6.",
		  "Number6", ACTID_EVENT_NUMBER_6 },
	{ "Enter the number 7.",
		  "Number7", ACTID_EVENT_NUMBER_7 },
	{ "Enter the number 8.",
		  "Number8", ACTID_EVENT_NUMBER_8 },
	{ "Enter the number 9.",
		  "Number9", ACTID_EVENT_NUMBER_9 },
	{ "Add 10 to the next entered number.",
		  "Number10add", ACTID_EVENT_NUMBER_10_ADD },
	{ "Set position back by numeric argument in current stream.",
		  "SeekRelative-", ACTID_SEEK_REL_m }, 
	{ "Set position to -60 seconds in current stream.",
		  "SeekRelative-60", ACTID_SEEK_REL_m60 }, 
	{ "Set position forward by numeric argument in current stream.",
		  "SeekRelative+", ACTID_SEEK_REL_p }, 
	{ "Set position to +60 seconds in current stream.",
		  "SeekRelative+60", ACTID_SEEK_REL_p60 },
	{ "Set position to -30 seconds in current stream.",
		  "SeekRelative-30", ACTID_SEEK_REL_m30 }, 
	{ "Set position to +30 seconds in current stream.",
		  "SeekRelative+30", ACTID_SEEK_REL_p30 },
	{ "Set position to -15 seconds in current stream.",
		  "SeekRelative-15", ACTID_SEEK_REL_m15 },
	{ "Set position to +15 seconds in current stream.",
		  "SeekRelative+15", ACTID_SEEK_REL_p15 },
	{ "Set position to -7 seconds in current stream.",
		  "SeekRelative-7", ACTID_SEEK_REL_m7 }, 
	{ "Set position to +7 seconds in current stream.",
		  "SeekRelative+7", ACTID_SEEK_REL_p7 },
	
	{ "Audio muting toggle.",
		  "Mute", ACTID_MUTE },
	{ "Change audio syncing.",
		  "AudioVideoDecay+", ACTID_AV_SYNC_p3600 },
	{ "Change audio syncing.",
		  "AudioVideoDecay-", ACTID_AV_SYNC_m3600 },
	{ "Reset audio video syncing offset.",
		  "AudioVideoDecayReset", ACTID_AV_SYNC_RESET },
	{ "Increment playback speed.",
		  "SpeedFaster", ACTID_SPEED_FAST },
	{ "Decrement playback speed.",
		  "SpeedSlower", ACTID_SPEED_SLOW },
	{ "Reset playback speed.",
		  "SpeedReset", ACTID_SPEED_RESET },
	{ "Increment audio volume.",
		  "Volume+", ACTID_pVOLUME },
	{ "Decrement audio volume.",
		  "Volume-", ACTID_mVOLUME },
	{ "Take a snapshot (Internal image fetch and save).",
		  "Snapshot", ACTID_SNAPSHOT },
		
	{ "Jump to Menu.",
		  "Menu", ACTID_EVENT_MENU1 },
	{ "Jump to Title Menu.",
		  "TitleMenu", ACTID_EVENT_MENU2 },
	{ "Jump to Root Menu.",
		  "RootMenu", ACTID_EVENT_MENU3 },
	{ "Jump to Subpicture Menu.",
		  "SubpictureMenu", ACTID_EVENT_MENU4 },
	{ "Jump to Audio Menu.",
		  "AudioMenu", ACTID_EVENT_MENU5 },
	{ "Jump to Angle Menu.",
		  "AngleMenu", ACTID_EVENT_MENU6 },
	{ "Jump to Part Menu.",
		  "PartMenu", ACTID_EVENT_MENU7 },
	{ "Up event.",
		  "EventUp", ACTID_EVENT_UP },
	{ "Down event.",
		  "EventDown", ACTID_EVENT_DOWN },
	{ "Left event.",
		  "EventLeft", ACTID_EVENT_LEFT },
	{ "Right event.",
		  "EventRight", ACTID_EVENT_RIGHT },
	{ "Previous event.",
		  "EventPrior", ACTID_EVENT_PRIOR },
	{ "Next event.",
		  "EventNext", ACTID_EVENT_NEXT },
	{ "Previous angle event.",
		  "EventAnglePrior", ACTID_EVENT_ANGLE_PRIOR },
	{ "Next angle event.",
		  "EventAngleNext", ACTID_EVENT_ANGLE_NEXT },
	{ "Select event.",
		  "EventSelect", ACTID_EVENT_SELECT },
		
	{ "Loop mode toggle.",
		  "ToggleLoopMode", ACTID_LOOPMODE },
		
	{ 0,
		  0, 0 }
};

int default_command_action(char *action_name)
{
	int i;

	for(i = 0; default_actions[i].name; i++)
		if(!strcasecmp(default_actions[i].name, action_name))
			return default_actions[i].action;

	return 0;   /* Unknown action */
};

static void action_pause(void)
{
	xine_set_param(fbxine.stream, XINE_PARAM_SPEED,
		       xine_get_param(fbxine.stream, XINE_PARAM_SPEED) ==
		       XINE_SPEED_PAUSE ? XINE_SPEED_NORMAL:XINE_SPEED_PAUSE);
	osd_update_status();
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
		osd_stream_position();
	}
	
	fbxine.ignore_next = 0;
	pthread_exit(0);
	return 0;
}

static void action_seek_relative(int off_sec)
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


static void change_audio_channel(void *data) 
{
        int dir = (int)data;
	int channel;
  
	channel = xine_get_param(fbxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
  
	if(dir == GUI_NEXT)
	     channel++;
	else if(dir == GUI_PREV)
	     channel--;
	else if(dir == GUI_RESET)
	     channel = -1;
  
        xine_set_param(fbxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, channel);
	osd_display_audio_lang();
}

static void change_spu(void *data) 
{
        int dir = (int)data;
	int channel;
  
	channel = xine_get_param(fbxine.stream, XINE_PARAM_SPU_CHANNEL);
  
	if(dir == GUI_NEXT)
	     channel++;
	else if(dir == GUI_PREV)
	     channel--;
	else if(dir == GUI_RESET)
	     channel = -1;
  
        xine_set_param(fbxine.stream, XINE_PARAM_SPU_CHANNEL, channel);
	osd_display_spu_lang();
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
		case ACTID_SEEK_REL_m600:
			action_seek_relative(-600);
			break;			
		case ACTID_SEEK_REL_p600:
			action_seek_relative(600);
			break;

		case ACTID_SEEK_REL_m60:
			action_seek_relative(-60);
			break;			
		case ACTID_SEEK_REL_p60:
			action_seek_relative(60);
			break;

		case ACTID_SEEK_REL_p30:
			action_seek_relative(30);
			break;
		case ACTID_SEEK_REL_m30:
			action_seek_relative(-30);
			break;

		case ACTID_SEEK_REL_p15:
			action_seek_relative(15);
			break;
		case ACTID_SEEK_REL_m15:
			action_seek_relative(-15);
			break;

		case ACTID_SEEK_REL_p10:
			action_seek_relative(10);
			break;
		case ACTID_SEEK_REL_m10:
			action_seek_relative(-10);
			break;

		case ACTID_SEEK_REL_m7:
			action_seek_relative(-7);
			break;
		case ACTID_SEEK_REL_p7:
			action_seek_relative(7);
			break;

	        case ACTID_AUDIOCHAN_NEXT:
		        change_audio_channel((void*)GUI_NEXT);
			break;

	        case ACTID_AUDIOCHAN_PRIOR:
		        change_audio_channel((void*)GUI_PREV);
			break;

	        case ACTID_AUDIOCHAN_DEFAULT:
		        change_audio_channel((void*)GUI_RESET);
			break;
			
	        case ACTID_SPU_NEXT:
		        change_spu((void*)GUI_NEXT);
			break;

	        case ACTID_SPU_PRIOR:
		        change_spu((void*)GUI_PREV);
			break;

	        case ACTID_SPU_DEFAULT:
		        change_spu((void*)GUI_RESET);
			break;
			
	        case ACTID_OSD_SINFOS:
		        osd_stream_infos();
		        break;
		  
		case ACTID_PAUSE:
			action_pause();
			break;
			
		case ACTID_PLAY:
			action_play();
			break;

		case ACTID_QUIT:
			fbxine_exit();
			break;
	        case ACTID_TOGGLE_INTERLEAVE:
		        fbxine.deinterlace_enable = !fbxine.deinterlace_enable;
			osd_display_info("Deinterlace: %s", (fbxine.deinterlace_enable) ? 
					 "enabled" : "disabled");
			post_deinterlace();
		        break;
	}
}
