/*
 * Copyright (C) 2000-2001 the xine project
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

/* required for getsubopt(); the __sun test gives us strncasecmp() on solaris */
#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
/* required for strncasecmp() */
#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "xitk.h"

#include "xine.h"
#include <xine/video_out_x11.h>
#include "utils.h"
#include "event.h"
#include "videowin.h"
#include "panel.h"

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
gGui_t   *gGui;
#ifdef HAVE_LIRC
int       no_lirc;
#endif

#define	OPTION_VISUAL		1000
#define	OPTION_INSTALL_COLORMAP	1001
#define DISPLAY_KEYMAP          1002

/* options args */
static const char *short_options = "?h"
#ifdef HAVE_LIRC
 "L"
#endif
#ifdef HAVE_XF86VIDMODE
 "F"
#endif
#ifdef DEBUG
 "d:"
#endif
 "R::u:a:V:A:D:p::s:g:f:v";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h'                      },
#ifdef HAVE_LIRC
  {"no-lirc"        , no_argument      , 0, 'L'                      },
#endif
#ifdef HAVE_XF86VIDMODE
  {"use-xvidext"    , no_argument      , 0, 'F'                      },
#endif
#ifdef debug
  {"debug"          , required_argument, 0, 'd'                      },
#endif
  {"recognize-by"   , optional_argument, 0, 'R'                      },
  {"spu-channel"    , required_argument, 0, 'u'                      },
  {"audio-channel"  , required_argument, 0, 'a'                      },
  {"video-driver"   , required_argument, 0, 'V'                      },
  {"audio-driver"   , required_argument, 0, 'A'                      },
  {"auto-play"      , optional_argument, 0, 'p'                      },
  {"auto-scan"      , required_argument, 0, 's'                      },
  {"hide-gui"       , no_argument,       0, 'g'                      },
  {"fullscreen"     , no_argument,       0, 'f'                      },
  {"visual"	    , required_argument, 0,  OPTION_VISUAL           },
  {"install"	    , no_argument      , 0,  OPTION_INSTALL_COLORMAP },
  {"keymap"         , optional_argument, 0,  DISPLAY_KEYMAP          },
  {"version"        , no_argument      , 0, 'v'                      },
  {0                , no_argument      , 0,  0                       }
};

/*
 *
 */
void show_version(void) {

  printf("This is xine (X11 gui) - a free video player v%s\n(c) 2000, 2001 by G. Bartsch and the xine project team.\n", VERSION);
}

/*
 *
 */
void show_banner(void) {

  show_version();
  printf("Built with xine library %d.%d.%d [%s]-[%s]-[%s].\n",
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, 
	 XINE_BUILD_DATE, XINE_BUILD_CC, XINE_BUILD_OS);
  printf("Found xine library version: %d.%d.%d (%s).\n",
	 xine_get_major_version(), xine_get_minor_version(),
	 xine_get_sub_version(), xine_get_str_version());

}

/*
 *
 */
void show_usage (void) {

  char **driver_ids;
  char  *driver_id;

  printf("\n");
  printf("Usage: xine [OPTIONS]... [MRL]\n");
  printf("\n");
  printf("  -v, --version                Display version.\n");
  printf("OPTIONS are:\n");
  printf("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_X11);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");
  printf("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n");
  printf("                               null ");
  driver_ids = xine_list_audio_output_plugins ();
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");

  printf("  -u, --spu-channel <#>        Select SPU (subtitle) channel '#'.\n");
  printf("  -a, --audio-channel <#>      Select audio channel '#'.\n");
  printf("  -p, --auto-play [opt]        Play on start. Can be followed by:\n");
  printf("                    'f': in fullscreen mode.\n");
  printf("                    'h': hide GUI (panel, etc.).\n");
  printf("                    'q': quit when play is done.\n");
  printf("                    'd': retrieve playlist from DVD. (deprecated. use -s DVD)\n");
  printf("                    'v': retrieve playlist from VCD. (deprecated. use -s VCD)\n");
  printf("  -s, --auto-scan <plugin>     auto-scan play list from <plugin>\n");
  printf("  -f, --fullscreen             start in fullscreen mode,\n");
#ifdef HAVE_XF86VIDMODE
  printf("  -F, --use-xvidext            Enable XF86VidMode Extension support\n");
#endif
  printf("  -g, --hide-gui               hide GUI (panel, etc.),\n");
#ifdef HAVE_LIRC
  printf("  -L, --no-lirc                Turn off LIRC support.\n");
#endif
  printf("  -R, --recognize-by [option]  Try to recognize stream type. Option are:\n");
  printf("                                 'default': by content, then by extension,\n");
  printf("                                 'revert': by extension, then by content,\n");
  printf("                                 'content': only by content,\n");
  printf("                                 'extension': only by extension.\n");
  printf("                                 -if no option is given, 'revert' is selected\n");
  printf("      --visual <class-or-id>   Use the specified X11 visual. <class-or-id>\n");
  printf("                               is either an X11 visual class, or a visual id.\n");
  printf("      --install                Install a private colormap.\n");
  printf("      --keymap [option]        Display keymap. Option are:\n");
  printf("                                 'default': display default keymap table,\n");
  printf("                                 'lirc': display draft of a .lircrc config file.\n");
  printf("                                 'remapped': user remapped keymap table.\n");
  printf("                                 -if no option is given, 'default' is selected.\n");
#ifdef DEBUG
  printf("  -d, --debug <flags>          Debug mode for <flags> ('help' for list).\n");
#endif

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

/*
 *
 */
#ifdef DEBUG
int handle_debug_subopt(char *sopt) {
  int i, subopt;
  char *str = sopt;
  char *val = NULL;
  char *debuglvl[] = {
    "verbose", "metronom", "audio", "demux",
    "input", "video", "pts", "mpeg", "avi",
    "ac3", "loop", "gui",
    NULL
  };

  while(*str) {
    subopt = getsubopt(&str, debuglvl, &val);
    switch(subopt) {
    case -1:
      i = 0;
      fprintf(stderr, "Valid debug flags are:\n\t");
      while(debuglvl[i] != NULL) {
	fprintf(stderr,"%s, ", debuglvl[i]);
	i++;
      }
      fprintf(stderr, "\b\b.\n");
      return 0;
    default:
      /* If debug feature is already enabled, with
       * XINE_DEBUG envvar, turn it off
       */
      if((gGui->debug_level & 0x8000>>(subopt + 1)))
	gGui->debug_level &= ~0x8000>>(subopt + 1);
      else
	gGui->debug_level |= 0x8000>>(subopt + 1);
      break;
    }
  }

  return 1;
}
#endif

/*
 * Handle sub-option of 'recognize-by' command line argument
 */
int handle_demux_strategy_subopt(char *sopt) {
  int subopt;
  char *str = sopt;
  char *val = NULL;
  char *ds_available[] = {
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

/*
 * Try to load video output plugin, by stored name or probing
 */
static void load_video_out_driver(char *video_driver_id) {
  double        res_h, res_v;
  x11_visual_t  vis;

  vis.display           = gGui->display;
  vis.screen            = gGui->screen;
  vis.d                 = gGui->video_window;
  res_h                 = (DisplayWidth  (gGui->display, gGui->screen)*1000 
			   / DisplayWidthMM (gGui->display, gGui->screen));
  res_v                 = (DisplayHeight (gGui->display, gGui->screen)*1000
			   / DisplayHeightMM (gGui->display, gGui->screen));
  vis.display_ratio     = res_h / res_v;
#ifdef	DEBUG
  printf("display_ratio: %f\n", vis.display_ratio);
#endif

  if (fabs(vis.display_ratio - 1.0) < 0.01) {
    /*
     * we have a display with *almost* square pixels (<1% error),
     * to avoid time consuming software scaling in video_out_xshm,
     * correct this to the exact value of 1.0 and pretend we have
     * perfect square pixels.
     */
    vis.display_ratio   = 1.0;
#ifdef	DEBUG
    printf("display_ratio: corrected to square pixels!\n");
#endif
  }

  vis.calc_dest_size    = video_window_calc_dest_size;
  vis.request_dest_size = video_window_adapt_size;
  vis.user_data         = NULL;
  
  if (!video_driver_id) {
    /* video output driver auto-probing */
    char **driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_X11);
    int    i;

    /* Try to init video with stored information */
    video_driver_id = config_lookup_str("video_driver_name", NULL);
    if(video_driver_id) {

      gGui->vo_driver = xine_load_video_output_plugin(gGui->config, 
						      video_driver_id,
						      VISUAL_TYPE_X11,
						      (void *) &vis);
      if (gGui->vo_driver) {
	if(driver_ids)
	  free(driver_ids);
	return;
      } 
    }
    
    i = 0;
    while (driver_ids[i]) {
      video_driver_id = driver_ids[i];
      
      printf ("main: probing <%s> video output plugin\n", video_driver_id);

      gGui->vo_driver = xine_load_video_output_plugin(gGui->config, 
						      video_driver_id,
						      VISUAL_TYPE_X11, 
						      (void *) &vis);
      if (gGui->vo_driver) {
	config_set_str("video_driver_name", video_driver_id);
	if(driver_ids)
	  free(driver_ids);
	return;
      }
     
      i++;
    }
      
    if (!gGui->vo_driver) {
      printf ("main: all available video drivers failed.\n");
      exit (1);
    }

  } 
  else {

    gGui->vo_driver = xine_load_video_output_plugin(gGui->config,
						    video_driver_id,
						    VISUAL_TYPE_X11, 
						    (void *) &vis);
    
    if (!gGui->vo_driver) {
      printf ("main: video driver <%s> failed\n", video_driver_id);
      exit (1);
    }
    
    config_set_str("video_driver_name", video_driver_id);
  }
}

/*
 * Try to load audio output plugin, by stored name or probing
 */
static void load_audio_out_driver(char *audio_driver_id, 
				  ao_driver_t **audio_driver) {
  
  if (!audio_driver_id) {
    char **driver_ids = xine_list_audio_output_plugins ();
    int i = 0;
    
    /* Try to init audio with stored information */
    audio_driver_id = config_lookup_str("audio_driver_name", NULL);
    if(audio_driver_id) {
      *audio_driver = xine_load_audio_output_plugin(gGui->config, 
						    audio_driver_id);
      if(*audio_driver) {
	if(driver_ids)
	    free(driver_ids);
	return;
      }
    }

    while(driver_ids[i] != NULL && *audio_driver == NULL) {
      audio_driver_id = driver_ids[i];

      printf("main: trying to autoload '%s' audio driver: ", driver_ids[i]);
      
      *audio_driver = xine_load_audio_output_plugin(gGui->config, 
						    driver_ids[i]);
      i++;
    }
  }
  else {

    /* Don't want to load an audio driver */
    if(!strncasecmp(audio_driver_id, "NULL", strlen(audio_driver_id))) {
      /* We don't store NULL driver name, i guess it's not very useful */
      *audio_driver = NULL;
      printf("main: not using any audio driver (as requested).\n");
      return;
    }
    else {
      *audio_driver = xine_load_audio_output_plugin(gGui->config, 
						    audio_driver_id);
      if (!*audio_driver) {
	printf ("main: the specified audio driver <%s> failed\n",
		audio_driver_id);
	exit(1);
      }
    }
  }

  if (*audio_driver)
    config_set_str("audio_driver_name", audio_driver_id);
  else
    printf ("main: audio driver <%s> failed\n", audio_driver_id);
}

/*
 *
 */
void event_listener (void *user_data, xine_event_t *event) {

  /* printf ("main: event listener, got event type %d\n", event->type); */
  
  switch(event->type) { 
  case XINE_EVENT_UI_CHANNELS_CHANGED:
    /* Update the panel */
    panel_update_channel_display ();
    break;

  case XINE_EVENT_UI_SET_TITLE: 
    {

      xine_ui_event_t *uevent = (xine_ui_event_t *)event;
      panel_set_title((char*)(uevent->data));
    }
    break;
  case XINE_EVENT_PLAYBACK_FINISHED:
    gui_status_callback (XINE_STOP);
    break;
  case XINE_EVENT_NEED_NEXT_MRL:
    {
      xine_next_mrl_event_t *uevent = (xine_next_mrl_event_t *)event;

      uevent->handled = 1;
      uevent->mrl = gui_next_mrl_callback ();
    }
    break;
  case XINE_EVENT_BRANCHED:
    gui_branched_callback ();
    break;
  }      
  
}

/*
 *
 */
int main(int argc, char *argv[]) {

  /* command line options will end up in these variables: */
  int              c = '?', aos = 0;
  int              option_index = 0;
  int              demux_strategy = DEMUX_DEFAULT_STRATEGY;
  int              audio_channel = 0;
  int              spu_channel = -1;
#ifdef HAVE_XF86VIDMODE
  int              use_xvidext = 0;
#endif
  /*  int              audio_options = 0; FIXME */
  int		   visual = 0;
  char            *audio_driver_id = NULL;
  char            *video_driver_id = NULL;
  ao_driver_t     *audio_driver = NULL ;
  sigset_t         vo_mask;

  /* Check xine library version */
  if(!xine_check_version(0, 9, 0)) {
    fprintf(stderr, "Require xine library version 0.9.0, found %d.%d.%d.\n",
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    exit(1);
  }

  sigemptyset(&vo_mask);
  sigaddset(&vo_mask, SIGALRM);
  if (sigprocmask (SIG_BLOCK,  &vo_mask, NULL)) {
    printf ("video_out: sigprocmask failed.\n");
  }

  gGui = (gGui_t *) xmalloc(sizeof(gGui_t));

  gGui->debug_level = 0;
  gGui->autoscan_plugin = NULL;
  gGui->prefered_visual_class = -1;
  gGui->prefered_visual_id = None;
  gGui->install_colormap = 0;
  gGui->cursor_grabbed = 0;
#ifdef HAVE_XF86VIDMODE
  gGui->XF86VidMode_fullscreen = 0;
#endif
  gGui->actions_on_start[aos] = ACTID_NOKEY;

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

#ifdef HAVE_LIRC
    case 'L': /* Disable LIRC support */
      no_lirc = 1;
      break;
#endif

#ifdef DEBUG
    case 'd': /* Select debug levels */
      if(optarg != NULL) {
	if(!(handle_debug_subopt(chomp(optarg))))
	  exit(1);
      }
      break;
#endif

    case 'R': /* Set a strategy to recognizing stream type */
      if(optarg != NULL)
	demux_strategy = handle_demux_strategy_subopt(chomp(optarg));
      else
	demux_strategy = DEMUX_REVERT_STRATEGY;
      break;

    case 'u': /* Select SPU channel */
      sscanf(optarg, "%i", &spu_channel);
      break;

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &audio_channel);
      break;

    case 'V': /* select video driver by plugin id */
      if(optarg != NULL) {
	video_driver_id = xmalloc (strlen (optarg) + 1);
	strncpy (video_driver_id, optarg, strlen (optarg));
	printf("video_driver_id = '%s'\n", video_driver_id);
      } else {
	fprintf (stderr, "video driver id required for -V option\n");
	exit (1);
      }
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
	audio_driver_id = xmalloc (strlen (optarg) + 1);
	strcpy (audio_driver_id, optarg);
      } else {
	fprintf (stderr, "audio driver id required for -A option\n");
	exit (1);
      }
      break;

    case 'p':/* Play [[in fullscreen][then quit]] on start */
      gGui->actions_on_start[aos++] = ACTID_PLAY;
      if(optarg != NULL) {
	if(strrchr(optarg, 'f')) {
	  gGui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
	}
	if(strrchr(optarg, 'h')) {
	  gGui->actions_on_start[aos++] = ACTID_TOGGLE_VISIBLITY;
	}
	if(strrchr(optarg, 'q')) {
	  gGui->actions_on_start[aos++] = ACTID_QUIT;
	}
	if(strrchr(optarg, 'd')) {
	  gGui->autoscan_plugin = "DVD";
	}
	if(strrchr(optarg, 'v')) {
	  gGui->autoscan_plugin = "VCD";
	}
      }
      break;

    case 'g': /* hide panel on start */
      gGui->actions_on_start[aos++] = ACTID_TOGGLE_VISIBLITY;
      break;

    case 'f': /* full screen mode on start */
      gGui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
      break;

    case 's': /* autoscan on start */
      gGui->autoscan_plugin = chomp(optarg);
      break;
       
#ifdef HAVE_XF86VIDMODE
    case 'F': /* Enable XF86VidMode Extension support */
      use_xvidext = 1;
      break;
#endif

    case OPTION_VISUAL:
      if (sscanf(optarg, "%x", &visual) == 1)
	gGui->prefered_visual_id = visual;
      else if (strcasecmp(optarg, "StaticGray") == 0)
	gGui->prefered_visual_class = StaticGray;
      else if (strcasecmp(optarg, "GrayScale") == 0)
	gGui->prefered_visual_class = GrayScale;
      else if (strcasecmp(optarg, "StaticColor") == 0)
	gGui->prefered_visual_class = StaticColor;
      else if (strcasecmp(optarg, "PseudoColor") == 0)
	gGui->prefered_visual_class = PseudoColor;
      else if (strcasecmp(optarg, "TrueColor") == 0)
	gGui->prefered_visual_class = TrueColor;
      else if (strcasecmp(optarg, "DirectColor") == 0)
	gGui->prefered_visual_class = DirectColor;
      else {
	show_usage();
	exit(1);
      }
      break;

    case OPTION_INSTALL_COLORMAP:
      gGui->install_colormap = 1;
      break;

    case DISPLAY_KEYMAP:
      if(optarg != NULL) {
	char *p = chomp(optarg);
	if(!strcasecmp(p, "default"))
	  kbindings_display_default_bindings();
	else if(!strcasecmp(p, "lirc"))
	  kbindings_display_default_lirc_bindings();
	else if(!strcasecmp(p, "remapped"))
	  kbindings_display_current_bindings((kbindings_init_kbinding()));
	else
	  kbindings_display_default_bindings();
      }
      else
	kbindings_display_default_bindings();
      exit(1);
      break;

    case 'v': /* Display version and exit*/
      show_version();
      exit(1);
      break;

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

  gGui->actions_on_start[aos] = ACTID_NOKEY;

  show_banner();

  /*
   * generate and init a config "object"
   */
  {
    char *homedir;

    if (!(gGui->configfile = getenv ("XINERC"))) {
      homedir = strdup(get_homedir());
      gGui->configfile = (char *) xmalloc(strlen(homedir) + 8 + 1);

      sprintf (gGui->configfile, "%s/.xinerc", homedir);
    }
  }

  gGui->config = config_file_init (gGui->configfile);

  config_set_int("demux_strategy", demux_strategy);
   
#ifdef HAVE_XF86VIDMODE
  if(use_xvidext) config_set_int("use_xvidext", 1);
#endif
   
  /*
   * init gui
   */

  gui_init(argc-optind, &argv[optind]);

  /*
   * load and init output drivers
   */

  /* Video out plugin */
  load_video_out_driver(video_driver_id);

  /* Audio out plugin */
  load_audio_out_driver(audio_driver_id, &audio_driver);


  /*
   * xine init
   */

  gGui->xine = xine_init (gGui->vo_driver, audio_driver,
			  gGui->config);

  xine_select_audio_channel (gGui->xine, audio_channel);
  xine_select_spu_channel (gGui->xine, spu_channel);

  /*
   * Register an event listener
   */
  xine_register_event_listener(gGui->xine, event_listener, gGui);

  /*
   * start CORBA server thread
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_start (gGui->xine);
#endif

  /*
   * hand control over to gui
   */

  gui_run();

#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_exit(gGui->xine);
#endif

  return 0;
}

