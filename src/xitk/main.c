/* 
 * Copyright (C) 2000 the xine project
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
 * xine main for X11
 *
 */

/* required for getsubopt() */
#define _XOPEN_SOURCE 500

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>


#include "xine.h"
#include "utils.h"
#include "gui_main.h"
#include "gui_dnd.h"

#ifdef HAVE_ORBIT 
#include "../corba/xine-server.h"
#endif

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

/*
 * global variables
 */
Display  *gDisplay;
uint32_t  debug_level;
xine_t   *gXine;

/* options args */
static const char *short_options = "?hS4"
#ifdef HAVE_LIRC
 "L"
#endif
 "R::"
#ifdef DEBUG
 "d:"
#endif
 "u:a:V:A:p::";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h' },
  {"spdif"          , no_argument      , 0, 'S' },
  {"4-channel"      , no_argument      , 0, '4' },
  {"audio-channel"  , required_argument, 0, 'a' },
  {"video-driver"   , required_argument, 0, 'V' },
  {"audio-driver"   , required_argument, 0, 'A' },
  {"audio-id"       , required_argument, 0, 'i' },
  {"spu-channel"    , required_argument, 0, 'u' },
  {"auto-play"      , optional_argument, 0, 'p' },
#ifdef HAVE_LIRC
  {"no-lirc"        , no_argument      , 0, 'L' },
#endif
  {"recognize-by"   , optional_argument, 0, 'R' },
#ifdef debug
  {"debug"          , required_argument, 0, 'd' },
#endif
  {0                , no_argument      , 0,  0  }
};

/* ------------------------------------------------------------------------- */
/*
 *
 */
void show_banner(void) {

  printf("This is xine (X11 gui) - a free video player v%s\n(c) 2000, 2001 by G. Bartsch and the xine project team.\n", VERSION);
}
/* ------------------------------------------------------------------------- */
/*
 *
 */
void show_usage (void) {
  
  char **driver_ids;
  char  *driver_id;

  printf("\n");
  printf("Usage: %s [OPTIONS]... [MRL]\n", PACKAGE);
  printf("\n");
  printf("OPTIONS are:\n");
  printf("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = list_video_output_plugins (VISUAL_TYPE_X11);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");

/*    printf("  -u, --spu-channel <#>        Select SPU (subtitle) channel '#'.\n");
  printf("  -a, --audio-channel <#>      Select audio channel '#'.\n");
  printf("  -S, --spdif                  enable AC3 output via SPDIF Port.\n");
  printf("  -4, --4-channel              enable 4-channel surround audio.\n");
  printf("  -p, --auto-play [opt]        Play on start. Can be followed by:\n");
  printf("                                 'f': start in fullscreen mode,\n");
  printf("                                 'h': hide control panel,\n");
  printf("                                 'q': quit when play is done.\n");
  printf("                                 'd': retrieve playlist from DVD.\n");
  printf("                                 'v': retrieve playlist from VCD.\n");
#ifdef HAVE_LIRC
  printf("  -L, --no-lirc                Turn off LIRC support.\n");
#endif
  printf("  -R, --recognize-by [option]  Try to recognize stream type. Option are:\n");
  printf("                                 'default': by content, then by extension,\n");
  printf("                                 'revert': by extension, then by content,\n");
  printf("                                 'content': only by content,\n");
  printf("                                 'extension': only by extension.\n");
  printf("                                 -if no option is given, 'revert' is selected\n");
#ifdef DEBUG
  printf("  -d, --debug <flags>          Debug mode for <flags> ('help' for list).\n");
#endif
*/
  printf("\n");
  printf("examples for valid MRLs (media resource locator):\n");
  printf("  File:  'path/foo.vob'\n");
  printf("         '/path/foo.vob'\n");
  printf("         'file://path/foo.vob'\n");
  printf("         'fifo://[[mpeg1|mpeg2]:/]path/foo'\n");
  printf("         'stdin://[mpeg1|mpeg2]' or '-' (mpeg2 only)\n");
  printf("  DVD:   'dvd://VTS_01_2.VOB'\n");
  printf("  VCD:   'vcd://<track number>'\n");
  printf("\n");
}
/* ------------------------------------------------------------------------- */
/*
 *
 */
#ifdef DEBUG
int handle_debug_subopt(char *sopt) {
  int subopt;
  char *str = sopt;
  char *val = NULL;
  const char *debuglvl[] = {
    "verbose", "metronom", "audio", "demux", 
    "input", "video", "pts", "mpeg", "avi", 
    "ac3", "loop", "gui",
    NULL
  };

  while((subopt = getsubopt(&str, debuglvl, &val)) != -1) {
    
    switch(subopt) {
    default:
      /* If debug feature is already enabled, with 
       * XINE_DEBUG envvar, turn it off 
       */
      if((debug_level & 0x8000>>(subopt + 1)))
	debug_level &= ~0x8000>>(subopt + 1);
      else
	debug_level |= 0x8000>>(subopt + 1);
      break;
    }
  }

  if(val) {
    int i = 0;
    fprintf(stderr, "Valid debug flags are:\n\t");
    while(debuglvl[i] != NULL) {
      fprintf(stderr,"%s, ", debuglvl[i]);
      i++;
    }
    fprintf(stderr, "\b\b.\n");
    return 0;
  }

  return 1;
}
#endif
/* ------------------------------------------------------------------------- */
/*
 * Handle sub-option of 'recognize-by' command line argument
 */
int handle_demux_strategy_subopt(char *sopt) {
  int subopt;
  char *str = sopt;
  char *val = NULL;
  const char *ds_available[] = {
    "default",
    "revert",
    "content",
    "extension",
    NULL
  };
  enum DEMUX_S {
    DS_DEFAULT,
    DS_REVERT,
    DS_CONTENT,
    DS_EXT
  };

  while((subopt = getsubopt(&str, ds_available, &val)) != -1) {

    switch(subopt) {

    case DS_DEFAULT:
      return DEMUX_DEFAULT_STRATEGY;
      break;      
    case DS_REVERT:
      return DEMUX_REVERT_STRATEGY;
      break;      
    case DS_CONTENT:
      return DEMUX_CONTENT_STRATEGY;
      break;      
    case DS_EXT:
      return DEMUX_EXTENSION_STRATEGY;
      break;      
    }
  }

  if(val) {
    int i;

    fprintf(stderr, "Error: '%s' is a wrong recognition strategy option.\n", 
	    val);
    fprintf(stderr, "Available strategies are:");
    for(i = 0; ds_available[i] != NULL; i++)
      fprintf(stderr, " %s,", ds_available[i]);
    fprintf(stderr, "\b.\nStrategy forced to 'default'.\n");
  }
  
  return DEMUX_DEFAULT_STRATEGY;
}
/* ------------------------------------------------------------------------- */
/*
 *
 */
int main(int argc, char *argv[]) {

  /* command line options will end up in these variables: */
  int              c = '?';
  int              option_index = 0;
  int              demux_strategy = DEMUX_DEFAULT_STRATEGY;
  int              audio_channel = 0; 
  int              spu_channel = -1;
  int              no_lirc = 0;
  int              audio_options = 0;
  int              autoplay_options = 0; /* stuff like FULL_ON_START, QUIT_ON_STOP */
  char            *audio_driver_id = NULL;
  char            *video_driver_id = NULL;
  ao_functions_t  *audio_driver = NULL ;
  vo_driver_t     *video_driver = NULL;
  char            *display_name = ":0.0";
  char             filename[1024];
  config_values_t *cfg;

  show_banner();

  debug_level = 0;
#ifdef DEBUG
  /* If XINE_DEBUG envvar is set, parse it */
  if(getenv("XINE_DEBUG") != NULL) {
    if(!(handle_debug_subopt(chomp((getenv("XINE_DEBUG"))))))
      exit(1);
  }
#endif

  /*
   * initialize CORBA server
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_init (&argc, argv);
#endif

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, 
			 long_options, &option_index)) != EOF) {
    switch(c) {

    case 'S': /* Use SPDIF Port for AC3 Stream */
      /* audio_options |= AO_MODE_AC3; FIXME */
      break;

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &audio_channel);
      break;
      
    case 'u': /* Select SPU channel */
      sscanf(optarg, "%i", &spu_channel);
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
	audio_driver_id = malloc (strlen (optarg) + 1);
	strcpy (audio_driver_id, optarg);
      } else {
	fprintf (stderr, "audio driver id required for -A option\n");
	exit (1);
      }
      break;

    case 'V': /* select video driver by plugin id */
      if(optarg != NULL) {
	video_driver_id = malloc (strlen (optarg) + 1);
	strncpy (video_driver_id, optarg, strlen (optarg));
	printf("video_driver_id = '%s'\n", video_driver_id);
      } else {
	fprintf (stderr, "video driver id required for -V option\n");
	exit (1);
      }
      break;

    case 'p':/* Play [[in fullscreen][then quit]] on start */
      autoplay_options |= PLAY_ON_START;
      if(optarg != NULL) {
	if(strrchr(optarg, 'f')) {
	  autoplay_options |= FULL_ON_START;
	}
	if(strrchr(optarg, 'h')) {
	  autoplay_options |= HIDEGUI_ON_START;
	}
	if(strrchr(optarg, 'q')) {
	  autoplay_options |= QUIT_ON_STOP;
	}
	if(strrchr(optarg, 'd')) {
	  autoplay_options |= PLAY_FROM_DVD;
	}
	if(strrchr(optarg, 'v')) {
	  autoplay_options |= PLAY_FROM_VCD;
	}
      }
      break;

#ifdef HAVE_LIRC
    case 'L': /* Disable LIRC support */
      no_lirc = 1;
      break;
#endif
      
    case 'R': /* Set a strategy to recognizing stream type */
      if(optarg != NULL)
	demux_strategy = handle_demux_strategy_subopt(chomp(optarg));
      else
	demux_strategy = DEMUX_REVERT_STRATEGY;
      break;

#ifdef DEBUG      
    case 'd': /* Select debug levels */
      if(optarg != NULL) {
	if(!(handle_debug_subopt(chomp(optarg))))
	  exit(1);
      }
      break;
#endif
      
    case 'h': /* Display usage */
    case '?':
      show_usage();
      exit(1);
      break;
      
    default:
      show_usage();
      fprintf (stderr, "invalid argument %d => exit\n",c);
      exit(1);
    }
  }

  /*
   * generate and init a config "object"
   */

  sprintf (filename, "%s/.xinerc", get_homedir());
  cfg = config_file_init (filename);

  /*
   * init X11
   */
  
  if (!XInitThreads ()) {
    printf ("\nXInitThreads failed - looks like you don't have a thread-safe xlib.\n");
    exit (1);
  } 

  if(getenv("DISPLAY"))
    display_name = getenv("DISPLAY");

  gDisplay = XOpenDisplay(display_name);

  if (gDisplay == NULL) {
    fprintf(stderr,"Can not open display\n");
    exit(1);
  }

  /*
   * init output drivers
   */

  if (!video_driver_id) {
    char **driver_ids = list_video_output_plugins (VISUAL_TYPE_X11);
    video_driver_id = driver_ids[0];

    printf ("main: auto-selected %s video output plugin\n", video_driver_id);

  }

  video_driver = xine_load_video_output_plugin(cfg, video_driver_id,
					       VISUAL_TYPE_X11, 
					       (void *) gDisplay);
  audio_driver = xine_load_audio_output_plugin(cfg,
					       audio_driver_name,
					       audio_driver_id);


  

  /*
   * xine init
   */

  gXine = xine_init (video_driver, audio_driver, 
		     gui_status_callback, cfg);

  xine_select_audio_channel (gXine, audio_channel);
  xine_select_spu_channel (gXine, spu_channel);

  /*
   * start CORBA server thread
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_start (gXine);
#endif

  /*
   * hand control over to gui
   */

  gui_start(argc-optind, &argv[optind]);

#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_exit(gXine);
#endif

  return 0;
}		


