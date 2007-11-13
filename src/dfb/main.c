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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * $Id$
 *
 * xine main for DirectFB
 * Rich Wareham <richwareham@users.sourceforge.net>
 *
 */

#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
#define _BSD_SOURCE 1

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
#include <directfb.h>       

#include <xine.h>
#include <xine/xineutils.h>

#include "dfb.h" 

dfbxine_t dfbxine;

static void config_update(xine_cfg_entry_t *entry, 
			  int type, int min, int max, int value, char *string) {

  switch(type) {

  case XINE_CONFIG_TYPE_UNKNOWN:
    fprintf(stderr, "Config key '%s' isn't registered yet.\n", entry->key);
    return;
    break;

  case XINE_CONFIG_TYPE_RANGE:
    entry->range_min = min;
    entry->range_max = max;
    break;

  case XINE_CONFIG_TYPE_STRING: 
    entry->str_value = string;
    break;
    
  case XINE_CONFIG_TYPE_ENUM:
  case XINE_CONFIG_TYPE_NUM:
  case XINE_CONFIG_TYPE_BOOL:
    entry->num_value = value;
    break;

  default:
    fprintf(stderr, "Unknown config type %d\n", type);
    return;
    break;
  }
  
  xine_config_update_entry(dfbxine.xine, entry);
}

void config_update_range(char *key, int min, int max) {
  xine_cfg_entry_t entry;
  
  if(xine_config_lookup_entry(dfbxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

void config_update_string(char *key, char *string) {
  xine_cfg_entry_t entry;
  
  if((xine_config_lookup_entry(dfbxine.xine, key, &entry)) && string)
    config_update(&entry, XINE_CONFIG_TYPE_STRING, 0, 0, 0, string);
  else {
    if(string == NULL)
      fprintf(stderr, "string is NULL\n");
    else
      fprintf(stderr, "WOW, key %s isn't registered\n", key);
  }
}

void config_update_enum(char *key, int value) {
  xine_cfg_entry_t entry;
  
  if(xine_config_lookup_entry(dfbxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

void config_update_bool(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(dfbxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

void config_update_num(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(dfbxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

/*
 * Handling xine engine status changes.
 */
static void gui_status_callback (int nStatus) {

  if (dfbxine.ignore_status)
    return;
  
  if(nStatus == XINE_STATUS_STOP) {
    dfbxine.current_mrl++;
   
    if (dfbxine.current_mrl < dfbxine.num_mrls) {
      if(xine_open(dfbxine.xine, dfbxine.mrl[dfbxine.current_mrl]))
	xine_play (dfbxine.xine, 0, 0 );
    }
    else {
      if (dfbxine.auto_quit == 1) {
        dfbxine.running = 0;
      }
      dfbxine.current_mrl--;
    }
  }

}

/*
 * Return next available mrl to xine engine.
 */
static char *gui_next_mrl_callback (void ) {

  if(dfbxine.current_mrl >= (dfbxine.num_mrls - 1)) 
    return NULL;
  
  return dfbxine.mrl[dfbxine.current_mrl + 1];
}

/*
 * Xine engine branch success.
 */
static void gui_branched_callback (void ) {

  if(dfbxine.current_mrl < (dfbxine.num_mrls - 1)) {
    dfbxine.current_mrl++;
  }
}

/*
 * Seek in current stream.
 */
static void set_position (int pos) {

  dfbxine.ignore_status = 1;
  xine_play(dfbxine.xine, pos, 0);
  dfbxine.ignore_status = 0;
}

/*
 * Extract mrls from argv[] and store them to playlist.
 */
void extract_mrls(int num_mrls, char **mrls) {
  int i;

  for(i = 0; i < num_mrls; i++)
    dfbxine.mrl[i] = mrls[i];

  dfbxine.num_mrls = num_mrls;
  dfbxine.current_mrl = 0;

}

void vm_cb(int w, int h, int bpp, void* cb_data) {
  printf("Mode %ix%i @ %i bpp supported\n", w,h,bpp);
}

int main(int argc, char *argv[]) {
  char                 *configfile;
  char                 *driver_name;
  dfb_visual_info_t     visual_info;
  IDirectFBEventBuffer *input_buf;
  IDirectFBInputDevice *keyboard = NULL;
  int                   window_grabbed = 0;
  int                   window_size = 0;
  int                   startx = 0;
  int                   starty = 0;
  int                   endx = 0;
  int                   endy = 0;

  /*
   * Check xine library version 
   */
  if(!xine_check_version(0, 9, 14)) {
    int major, minor, sub;
    
    xine_get_version (&major, &minor, &sub);
    fprintf(stderr, "require xine library version 0.9.14, found %d.%d.%d.\n", major, minor, sub);
    goto failure;
  }
  
  dfbxine.num_mrls = 0;
  dfbxine.current_mrl = 0;
  dfbxine.auto_quit = 0; /* default: loop forever */
  dfbxine.debug_messages = 0;
  dfbxine.dfb = NULL;
  dfbxine.primary = NULL;
  dfbxine.audio_driver_id = dfbxine.video_driver_id = NULL;
  dfbxine.audio_channel = 0;
   
  if(!do_command_line(argc,argv)) {
    goto failure;
  }

  /*
   * generate and init a config "object"
   */
  {
    char *cfgfile = ".xine/config";
    if (!(configfile = getenv("XINERC"))) {
      configfile = (char *) xine_xmalloc((strlen((xine_get_homedir())) +
					  strlen(cfgfile))+2);
      sprintf(configfile, "%s/%s", (xine_get_homedir()), cfgfile);
    }
  }

  if(!init_dfb()) {
    fprintf(stderr, "Error initialising DirectFB\n");
    goto failure;
  }
  
  dfbxine.xine = (xine_t *) xine_new();
  xine_config_load (dfb.xine, configfile);
  
  xine_init (dfbxine.xine,  dfbxine.ao_driver, dfbxine.vo_driver);
  
  if(!dfbxine.xine) {
    fprintf(stderr, "xine_init() failed.\n");
    goto failure;
  }
  
  /*
   * init video output driver
   */
  if(!dfbxine.video_driver_id)
    dfbxine.video_driver_id = "directfb";
  
  visual_info.dfb = dfbxine.dfb;
  visual_info.layer = dfbxine.video_layer;
  dfbxine.vo_driver = xine_open_video_driver(dfbxine.xine, 
					     dfbxine.video_driver_id,
					     XINE_VISUAL_TYPE_DFB, 
					     (void *)(&visual_info));
  
  if (!dfbxine.vo_driver) {
    printf ("main: video driver failed\n");
    goto failure;
  }

  /*
   * init audio output driver
   */
  driver_name = (char *) xine_config_register_string(dfbxine.xine, 
						     "audio.driver", 
						     "oss",
						     "audio driver to use",
						     NULL,
						     20,
						     NULL,
						     NULL);
  
  if(!dfbxine.audio_driver_id)
    dfbxine.audio_driver_id = driver_name;
  else
    config_update_string (dfbxine.xine, "audio.driver", dfbxine.audio_driver_id);
  
  dfbxine.ao_driver = xine_open_audio_driver(dfbxine.xine, dfbxine.audio_driver_id, NULL);
  
  if (!dfbxine.ao_driver) {
    printf ("main: audio driver %s failed\n", dfbxine.audio_driver_id);
  }
  
  /*
   * xine init
   */
  /* Init mixer control */
  dfbxine.mixer.enable = 0;
  dfbxine.mixer.caps = 0;

  if(xine_get_param(dfbxine.xine, XINE_PARAM_AO_MIXER_VOL))
    dfbxine.mixer.caps |= XINE_PARAM_AO_MIXER_VOL;
  if(xine_get_param(dfbxine.xine, XINE_PARAM_AO_PCM_VOL))
    dfbxine.mixer.caps |= XINE_PARAM_AO_PCM_VOL;
  if(xine_get_param(dfbxine.xine, XINE_PARAM_AO_MUTE))
    dfbxine.mixer.caps |= XINE_PARAM_AO_MUTE;
  
  if(dfbxine.mixer.caps & (XINE_PARAM_AO_MIXER_VOL | XINE_PARAM_AO_PCM_VOL)) { 
    dfbxine.mixer.enable = 1;
    dfbxine.mixer.volume_level = xine_get_param(dfbxine.xine, XINE_PARAM_AUDIO_VOLUME);
  }
  
  if(dfbxine.mixer.caps & XINE_PARAM_AO_MUTE)
    dfbxine.mixer.mute = xine_get_param(dfbxine.xine, XINE_PARAM_AUDIO_MUTE);
  
  /* Select audio channel */
  xine_set_param(dfbxine.xine, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, dfbxine.audio_channel);
  
  /*
   * ui loop
   */

  if(xine_open(dfbxine.xine, dfbxine.mrl[dfbxine.current_mrl]))
    xine_play (dfbxine.xine, 0, 0);
  
  dfbxine.running = 1;
  
  DFBCHECK(dfbxine.dfb->GetInputDevice (dfbxine.dfb,
					DIDID_KEYBOARD, &keyboard));
  DFBCHECK(keyboard->CreateEventBuffer (keyboard, &input_buf));
  DFBCHECK(dfbxine.main_window->AttachEventBuffer(dfbxine.main_window,
						  input_buf));
  
  while (dfbxine.running) {
    DFBWindowEvent win_event;
    DFBInputEvent input_event;
    DFBEvent event;

    input_buf->WaitForEventWithTimeout(input_buf, 0, 10000000);
    
    while (input_buf->GetEvent(input_buf, &event) == DFB_OK) {
      if(event.clazz == DFEC_WINDOW) {
	win_event = event.window;
	switch (win_event.type) {
	 case DWET_BUTTONDOWN: 
	  if(!window_grabbed && win_event.button == DIBI_LEFT) {
	    window_grabbed = 1;
	    dfbxine.layer->GetCursorPosition(dfbxine.layer, &startx,
				  	     &starty);
	    dfbxine.main_window->GrabPointer(dfbxine.main_window);
	  }
	  if(!window_size && win_event.button == DIBI_MIDDLE) {
	    int w,h;
	    
	    window_size = 1;
	    dfbxine.layer->GetCursorPosition(dfbxine.layer, &startx,
		  			     &starty);
	    dfbxine.main_window->GrabPointer(dfbxine.main_window);
	    dfbxine.main_window->GetSize(dfbxine.main_window, &w, &h);
	    startx -= w;
	    starty -= h;
	  }
	  break;
	 case DWET_BUTTONUP:
	  if(win_event.button == DIBI_LEFT) {
	    dfbxine.main_window->UngrabPointer(dfbxine.main_window);
	    window_grabbed = 0;
	  }
	  if(win_event.button == DIBI_MIDDLE) {
	    dfbxine.main_window->UngrabPointer(dfbxine.main_window);
	    window_size = 0;
	  }
	  break;
	 case DWET_MOTION:
	  endx = win_event.cx;
	  endy = win_event.cy;
	  break;
	 default:
	  break;
	}
      }
      
      if(window_grabbed) {
	dfbxine.main_window->Move(dfbxine.main_window,
				  endx-startx, endy-starty);
  	startx = endx;
    	starty = endy; 
      }
      
      if(window_size) {
	dfbxine.main_window->Resize(dfbxine.main_window,
	    			    endx-startx, endy-starty);
      }

      if(event.clazz == DFEC_INPUT) {
	input_event = event.input;
	if(input_event.type == DIET_KEYPRESS) {
	  printf("USER QUITTED\n");
	  dfbxine.running = 0;
	}
      }
    } 
  }
  
failure:
    
  if(dfbxine.xine) 
    xine_config_save(dfbxine.xine, configfile);
 
  if(dfbxine.xine)
   xine_exit(dfbxine.xine); 
 
  if(dfbxine.video_layer)
   DFBCHECK(dfbxine.video_layer->Release(dfbxine.video_layer));
  if(dfbxine.main_surface)
   DFBCHECK(dfbxine.main_surface->Release(dfbxine.main_surface));
  if(dfbxine.main_window)
   DFBCHECK(dfbxine.main_window->Release(dfbxine.main_window));
  if(dfbxine.primary)
   DFBCHECK(dfbxine.primary->Release(dfbxine.primary));
  if(dfbxine.pointer)
   DFBCHECK(dfbxine.pointer->Release(dfbxine.pointer));
  if(dfbxine.layer)
   DFBCHECK(dfbxine.layer->Release(dfbxine.layer));
  if(dfbxine.dfb) 
   DFBCHECK(dfbxine.dfb->Release(dfbxine.dfb));

  return 0;
}
