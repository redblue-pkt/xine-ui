/*
 * Copyright (C) 2000-2002 the xine project
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
#include <sys/types.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <locale.h>

#include <xine.h>
#include <xine/video_out_x11.h>
#include <xine/xineutils.h>

#include "event.h"
#include "videowin.h"
#include "panel.h"
#include "actions.h"
#include "i18n.h"

#include "xitk.h"
#include "errors.h"

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
int       unhandled_codec_mode; /* 0 = never, 1 = video, 2 = audio, 3 = always */

typedef struct {
  FILE    *fd;
  char    *filename;
  char    *ln;
  char     buf[256];
} file_info_t;


#define	OPTION_VISUAL		1000
#define	OPTION_INSTALL_COLORMAP	1001
#define DISPLAY_KEYMAP          1002

/* options args */
static const char *short_options = "?hHgfvn"
#ifdef HAVE_LIRC
 "L"
#endif
#ifdef HAVE_XF86VIDMODE
 "F"
#endif
#ifdef DEBUG
 "d:"
#endif
 "u:a:V:A:p::s:RG:B";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h'                      },
#ifdef HAVE_LIRC
  {"no-lirc"        , no_argument      , 0, 'L'                      },
#endif
#ifdef debug
  {"debug"          , required_argument, 0, 'd'                      },
#endif
  {"spu-channel"    , required_argument, 0, 'u'                      },
  {"audio-channel"  , required_argument, 0, 'a'                      },
  {"video-driver"   , required_argument, 0, 'V'                      },
  {"audio-driver"   , required_argument, 0, 'A'                      },
  {"auto-play"      , optional_argument, 0, 'p'                      },
  {"auto-scan"      , required_argument, 0, 's'                      },
  {"hide-gui"       , no_argument      , 0, 'g'                      },
  {"hide-video"     , no_argument      , 0, 'H'                      },
  {"fullscreen"     , no_argument      , 0, 'f'                      },
  {"visual"	    , required_argument, 0,  OPTION_VISUAL           },
  {"install"	    , no_argument      , 0,  OPTION_INSTALL_COLORMAP },
  {"keymap"         , optional_argument, 0,  DISPLAY_KEYMAP          },
  {"network"        , no_argument      , 0, 'n'                      },
  {"root"           , no_argument      , 0, 'R'                      },
  {"geometry"       , required_argument, 0, 'G'                      },
  {"borderless"     , no_argument      , 0, 'B'                      },
  {"version"        , no_argument      , 0, 'v'                      },
  {0                , no_argument      , 0,  0                       }
};


/*
 *    RC file related functions
 */
#if DEBUG
static void _rc_file_check_args(int argc, char **argv) {
  int i;
  
  printf("%s(): argc %d\n", __FUNCTION__, argc);

  for(i = 0; i < argc; i++)
    printf("argv[%d] = '%s'\n", i, argv[i]);
}
#endif
static void _rc_file_clean_eol(file_info_t *rcfile) {
  char *p;

  p = rcfile->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      
      p++;
    }
    
    while(p > rcfile->ln) {
      --p;
      
      if(*p == ' ') 
	*p = '\0';
      else
	break;
    }
  }
}
static int _rc_file_get_next_line(file_info_t *rcfile) {

 __get_next_line:
  rcfile->ln = fgets(rcfile->buf, 255, rcfile->fd);
  
  while(rcfile->ln && (*rcfile->ln == ' ' || *rcfile->ln == '\t')) ++rcfile->ln;
  
  if(rcfile->ln) {
    if(!strncmp(rcfile->ln, "#", 1))
      goto __get_next_line;
  }
  
  _rc_file_clean_eol(rcfile);
  
  if(rcfile->ln && !strlen(rcfile->ln))
    goto __get_next_line;

  return((rcfile->ln != NULL));
}
/*
 * This function duplicate argv[] to an char** array,, 
 * try to open and parse a "~/.xine/xinerc" rc file, 
 * and concatenate found entries to the char **array.
 * This allow user to always specify a command line argument.
 */
static char **build_command_line_args(int argc, char *argv[], int *_argc) {
  int            i;
  char          *xinerc = ".xine/xinerc";
  file_info_t   *rcfile;
  char        **_argv = NULL;
  
  _argv  = (char **) xine_xmalloc(sizeof(char **) * (argc + 1));
  (*_argc) = argc;
  for(i = 0; i < argc; i++)
    _argv[i] = strdup(argv[i]);

  rcfile           = (file_info_t *) xine_xmalloc(sizeof(file_info_t));
  rcfile->filename = (char *) xine_xmalloc((strlen((xine_get_homedir())) + strlen(xinerc)) + 2);
  sprintf(rcfile->filename, "%s/%s", (xine_get_homedir()), xinerc);
  
  if((rcfile->fd = fopen(rcfile->filename, "r")) != NULL) {
    
    while(_rc_file_get_next_line(rcfile)) {
      _argv = (char **) realloc(_argv, sizeof(char **) * ((*_argc) + 2));
      _argv[(*_argc)] = strdup(rcfile->ln);
      (*_argc)++;
    }
    
    fclose(rcfile->fd);
  }

  _argv[(*_argc)] = NULL;
  
  free(rcfile->filename);
  free(rcfile);

  return _argv;
}

/*
 *
 */
void show_version(void) {

  printf("This is xine (X11 gui) - a free video player v%s\n(c) 2000-2002 by G. Bartsch and the xine project team.\n", VERSION);
}

/*
 *
 */
void show_banner(void) {

  show_version();
  printf(_("Built with xine library %d.%d.%d [%s]-[%s]-[%s].\n"),
	   XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, 
	   XINE_BUILD_DATE, XINE_BUILD_CC, XINE_BUILD_OS);
  printf(_("Found xine library version: %d.%d.%d (%s).\n"),
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
  printf(_("Usage: xine [OPTIONS]... [MRL]\n"));
  printf("\n");
  printf("OPTIONS are:\n");
  printf(_("  -v, --version                Display version.\n"));
  printf(_("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n"));
  printf("                               ");
  driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_X11);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");
  printf(_("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n"));
  printf("                               null ");
  driver_ids = xine_list_audio_output_plugins ();
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");

  printf(_("  -u, --spu-channel <#>        Select SPU (subtitle) channel '#'.\n"));
  printf(_("  -a, --audio-channel <#>      Select audio channel '#'.\n"));
  printf(_("  -p, --auto-play [opt]        Play on start. Can be followed by:\n"));
  printf(_("                    'f': in fullscreen mode.\n"));
  printf(_("                    'h': hide GUI (panel, etc.).\n"));
  printf(_("                    'w': hide video window.\n"));
  printf(_("                    'q': quit when play is done.\n"));
  printf(_("                    'd': retrieve playlist from DVD. (deprecated. use -s DVD)\n"));
  printf(_("                    'v': retrieve playlist from VCD. (deprecated. use -s VCD)\n"));
  printf(_("  -s, --auto-scan <plugin>     auto-scan play list from <plugin>\n"));
  printf(_("  -f, --fullscreen             start in fullscreen mode,\n"));
  printf(_("  -g, --hide-gui               hide GUI (panel, etc.)\n"));
  printf(_("  -H, --hide-video             hide video window\n"));
#ifdef HAVE_LIRC
  printf(_("  -L, --no-lirc                Turn off LIRC support.\n"));
#endif
  printf(_("      --visual <class-or-id>   Use the specified X11 visual. <class-or-id>\n"));
  printf(_("                               is either an X11 visual class, or a visual id.\n"));
  printf(_("      --install                Install a private colormap.\n"));
  printf(_("      --keymap [option]        Display keymap. Option are:\n"));
  printf(_("                                 'default': display default keymap table,\n"));
  printf(_("                                 'lirc': display draft of a .lircrc config file.\n"));
  printf(_("                                 'remapped': user remapped keymap table.\n"));
  printf(_("                                 -if no option is given, 'default' is selected.\n"));
  printf(_("  -n, --network                Enable network remote control server.\n"));
  printf(_("  -R, --root                   Use root window as video window.\n"));
  printf(_("  -G, --geometry <WxH[+X+Y]>   Set output window geometry (X style).\n"));
  printf(_("  -B, --borderless             Borderless video output window.\n"));
  printf("\n");
  printf(_("examples for valid MRLs (media resource locator):\n"));
  printf(_("  File:  'path/foo.vob'\n"));
  printf(_("         '/path/foo.vob'\n"));
  printf(_("         'file://path/foo.vob'\n"));
  printf(_("         'fifo://[[mpeg1|mpeg2]:/]path/foo'\n"));
  printf(_("         'stdin://[mpeg1|mpeg2]' or '-' (mpeg2 only)\n"));
  printf(_("  DVD:   'dvd://VTS_01_2.VOB'\n"));
  printf(_("  VCD:   'vcd://<track number>'\n"));
  printf("\n");
}

/*
 * Try to load video output plugin, by stored name or probing
 */
static void load_video_out_driver(char *video_driver_id) {
  double         res_h, res_v;
  x11_visual_t   vis;
  char          *default_driver;

  vis.display           = gGui->display;
  vis.screen            = gGui->screen;
  vis.d                 = gGui->video_window;
  res_h                 = (DisplayWidth  (gGui->display, gGui->screen)*1000 
			   / DisplayWidthMM (gGui->display, gGui->screen));
  res_v                 = (DisplayHeight (gGui->display, gGui->screen)*1000
			   / DisplayHeightMM (gGui->display, gGui->screen));
  vis.display_ratio     = res_h / res_v;
#ifdef DEBUG
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
#ifdef DEBUG
    printf("display_ratio: corrected to square pixels!\n");
#endif
  }

  vis.dest_size_cb      = video_window_dest_size_cb;
  vis.frame_output_cb   = video_window_frame_output_cb;
  vis.user_data         = NULL;
  
  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  default_driver = gGui->config->register_string (gGui->config, "video.driver", "auto",
						  "video driver to use",
						  NULL, NULL, NULL);
  if (!video_driver_id) {
    /* video output driver auto-probing */
    char **driver_ids;
    int    i;
    
    /* Try to init video with stored information */
    video_driver_id = default_driver;
    
    if (strcmp (video_driver_id, "auto")) {

      gGui->vo_driver = xine_load_video_output_plugin(gGui->config, 
						      video_driver_id,
						      VISUAL_TYPE_X11,
						      (void *) &vis);
      if (gGui->vo_driver) {
	gGui->config->update_string (gGui->config, "video.driver", video_driver_id);
	return;
      } 
    }
    
    i = 0;
    driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_X11);

    while (driver_ids[i]) {
      video_driver_id = driver_ids[i];
      
      printf (_("main: probing <%s> video output plugin\n"), video_driver_id);

      gGui->vo_driver = xine_load_video_output_plugin(gGui->config, 
						      video_driver_id,
						      VISUAL_TYPE_X11, 
						      (void *) &vis);
      if (gGui->vo_driver) {
	if(driver_ids)
	  free(driver_ids);
	gGui->config->update_string (gGui->config, "video.driver", video_driver_id);
	return;
      }
     
      i++;
    }
      
    if (!gGui->vo_driver) {
      printf (_("main: all available video drivers failed.\n"));
      exit (1);
    }

  } else {

    gGui->vo_driver = xine_load_video_output_plugin(gGui->config,
						    video_driver_id,
						    VISUAL_TYPE_X11, 
						    (void *) &vis);
    
    if (!gGui->vo_driver) {
      printf (_("main: video driver <%s> failed\n"), video_driver_id);
      exit (1);
    }
    
    gGui->config->update_string (gGui->config, "video.driver", video_driver_id);
    
  }
}

/*
 * Try to load audio output plugin, by stored name or probing
 */
static ao_driver_t *load_audio_out_driver(char *audio_driver_id) {
  char        *default_driver;
  ao_driver_t *audio_driver = NULL;
  
  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  default_driver = gGui->config->register_string (gGui->config, "audio.driver", "auto",
						  "audio driver to use",
						  NULL, NULL, NULL);
  
  /*
   * if no audio driver was specified at the command line, 
   * look up audio driver id in the config file
   */
  if (!audio_driver_id) 
    audio_driver_id = default_driver;

  /* probe ? */

  if (!strncmp (audio_driver_id, "auto",4)) {
    char **driver_ids = xine_list_audio_output_plugins ();
    int i = 0;

    printf ("main: probing audio drivers...\n");
    
    while ( driver_ids[i] != NULL ) {
      audio_driver_id = driver_ids[i];

      printf(_("main: trying to autoload '%s' audio driver: "), driver_ids[i]);
      
      audio_driver = xine_load_audio_output_plugin(gGui->config, 
						    driver_ids[i]);

      if (audio_driver) {
	printf ("main: ...worked, using '%s' audio driver.\n",
		driver_ids[i]);

	gGui->config->update_string (gGui->config, "audio.driver", 
				     audio_driver_id);

	return audio_driver;
      }

      i++;
    }
    printf(_("main: audio driver probing failed => no audio output\n"));
	    
    gGui->config->update_string (gGui->config, "audio.driver", 
				 "null");

  } else {

    /* don't want to load an audio driver ? */
    if (!strncasecmp (audio_driver_id, "NULL", 4)) {

      printf("main: not using any audio driver (as requested).\n");
      gGui->config->update_string (gGui->config, "audio.driver", 
				   "null");

    } else {

      audio_driver = xine_load_audio_output_plugin(gGui->config, 
						   audio_driver_id);
      if (!audio_driver) {
	printf(_("main: the specified audio driver '%s' failed\n"),
		audio_driver_id);
	exit(1);
      }

      gGui->config->update_string (gGui->config, "audio.driver", 
				   audio_driver_id);
      
    }
  }

  return audio_driver;
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
  
  case XINE_EVENT_ASPECT_CHANGE:
    {
     xine_aspect_ratio_event_t *aevent = (xine_aspect_ratio_event_t*)event;
     gui_set_aspect_ratio(aevent->ratio_code);
    }
    break;
  }      
 
}

static void unhandled_codec_mode_cb( void *dummy, cfg_entry_t *entry)
{
  unhandled_codec_mode = entry->num_value;
}

static void codec_reporting( void *user_data, int codec_type,
                             uint32_t fourcc, char *description, int handled )
{
  char fourcc_txt[10];
  
  /* store fourcc as text */
  *(uint32_t *)fourcc_txt = fourcc;
  fourcc_txt[4] = '\0';
  
  /* report error for unknown/unhandled codecs */
  if( !handled ) {
    if( codec_type == XINE_CODEC_VIDEO &&
        (unhandled_codec_mode == 1 || unhandled_codec_mode == 3) ) {
      
      /* display fourcc if no description available */
      if( !description[0] )
        description = fourcc_txt;
        
      xine_error(_("No video plugin found to decode '%s'."), description);
    }
            
    if( codec_type == XINE_CODEC_AUDIO &&
        (unhandled_codec_mode == 2 || unhandled_codec_mode == 3) ) {
      
      /* display fourcc if no description available */
      if( !description[0] ) {
        description = fourcc_txt;
        
        /* show waveformattag in hexa */
        if( fourcc < 0x10000 ) {
          sprintf(fourcc_txt, "0x%x", fourcc );
        }  
      }
      xine_error(_("No audio plugin found to decode '%s'."), description);
    }
  }
}

/*
 *
 */
int main(int argc, char *argv[]) {

  /* command line options will end up in these variables: */
  int                   c = '?', aos = 0;
  int                   option_index = 0;
  int                   audio_channel = -1;
  int                   spu_channel = -1;
  window_attributes_t   window_attribute;
  int		        visual = 0;
  char                 *audio_driver_id = NULL;
  char                 *video_driver_id = NULL;
  ao_driver_t          *audio_driver = NULL ;
  sigset_t              vo_mask;
  char                **_argv;
  int                   _argc;
  static char          *warn_unhandled_codec[] = 
                       {"never", "video only", "audio only", "always", NULL};

  /* Check xine library version */
  if(!xine_check_version(0, 9, 10)) {
    fprintf(stderr, _("Require xine library version 0.9.10, found %d.%d.%d.\n"),
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    exit(1);
  }

  sigemptyset(&vo_mask);
  sigaddset(&vo_mask, SIGALRM);
  if (sigprocmask (SIG_BLOCK,  &vo_mask, NULL)) {
    printf ("video_out: sigprocmask failed.\n");
  }

#ifdef HAVE_SETLOCALE
  xine_set_locale();
  xitk_set_locale();
  setlocale (LC_ALL, "");
#endif

  bindtextdomain(PACKAGE, XITK_LOCALE);
  textdomain(PACKAGE);
  
  gGui = (gGui_t *) xine_xmalloc(sizeof(gGui_t));

  gGui->debug_level            = 0;
  gGui->autoscan_plugin        = NULL;
  gGui->prefered_visual_class  = -1;
  gGui->prefered_visual_id     = None;
  gGui->install_colormap       = 0;
  gGui->cursor_grabbed         = 0;
  gGui->network                = 0;
  gGui->use_root_window        = 0;
#ifdef HAVE_XF86VIDMODE
  gGui->XF86VidMode_fullscreen = 0;
#endif
  gGui->actions_on_start[aos]  = ACTID_NOKEY;

  window_attribute.x     = window_attribute.y      = -8192;
  window_attribute.width = window_attribute.height = -1;
  window_attribute.borderless = 0;

  _argv = build_command_line_args(argc, argv, &_argc);
#if DEBUG
  _rc_file_check_args(_argc, _argv);
#endif

  /*
   * initialize CORBA server
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_init (&_argc, _argv);
#endif

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(_argc, _argv, short_options,
			 long_options, &option_index)) != EOF) {
    switch(c) {

#ifdef HAVE_LIRC
    case 'L': /* Disable LIRC support */
      no_lirc = 1;
      break;
#endif

    case 'u': /* Select SPU channel */
      sscanf(optarg, "%i", &spu_channel);
      break;

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &audio_channel);
      break;

    case 'V': /* select video driver by plugin id */
      if(optarg != NULL) {
	video_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strncpy (video_driver_id, optarg, strlen (optarg));
	printf("video_driver_id = '%s'\n", video_driver_id);
      } else {
	fprintf (stderr, _("video driver id required for -V option\n"));
	exit (1);
      }
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
	audio_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strcpy (audio_driver_id, optarg);
      } else {
	fprintf (stderr, _("audio driver id required for -A option\n"));
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
	if(strrchr(optarg, 'w')) {
	  gGui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
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

    case 'H': /* hide video on start */
      gGui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
      break;

    case 'f': /* full screen mode on start */
      gGui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
      break;

    case 's': /* autoscan on start */
      gGui->autoscan_plugin = xine_chomp(optarg);
      break;
       
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
	char *p = xine_chomp(optarg);
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

    case 'n': /* Enable remote control server */
      gGui->network = 1;
      break;
      
    case 'R': /* Use root window for video output */
      gGui->use_root_window = 1;
      break;

    case 'G': /* Set geometry */
      {
	int width, height, xoff, yoff;
	
	if(optarg != NULL) {
	  if((sscanf(optarg, "%ix%i+%i+%i", &width, &height, &xoff, &yoff)) == 4) {
	    window_attribute.width  = width;
	    window_attribute.height = height;
	    window_attribute.x      = xoff;
	    window_attribute.y      = yoff;
	  }
	  else if((sscanf(optarg, "%ix%i", &width, &height)) == 2) {
	    window_attribute.width  = width;
	    window_attribute.height = height;
	  }
	  else {
	    printf(_("Bad geometry '%s', see xine --help\n"), optarg);
	    exit(1);
	  }
	}
      }
      break;

    case 'B':
      window_attribute.borderless = 1;
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
      fprintf (stderr, _("invalid argument %d => exit\n"), c);
      exit(1);
    }
  }
  
  /* 
   * Using root window mode don't allow
   * window geometry, so, reset those params.
   */
  if(gGui->use_root_window) {
    window_attribute.x     = window_attribute.y      = -8192;
    window_attribute.width = window_attribute.height = -1;
    window_attribute.borderless = 0;
  }

  show_banner();

  /*
   * generate and init a config "object"
   */
  {
    char *cfgfile = ".xine/config";
    struct stat st;

    if (!(gGui->configfile = getenv ("XINERC"))) {
      gGui->configfile = (char *) xine_xmalloc((strlen((xine_get_homedir())) + strlen(cfgfile))+2);
      sprintf (gGui->configfile, "%s/%s", (xine_get_homedir()), cfgfile);
    }

    /* Popup setup window if there is no config file */
    if(stat(gGui->configfile, &st) < 0)
      gGui->actions_on_start[aos++] = ACTID_SETUP;

  }

  gGui->actions_on_start[aos] = ACTID_NOKEY;

  gGui->config = xine_config_file_init (gGui->configfile);

  /*
   * init gui
   */
  
  gui_init(_argc - optind, &_argv[optind], &window_attribute);

  /*
   * load and init output drivers
   */

  /* Video out plugin */
  load_video_out_driver(video_driver_id);
  video_window_select_visual ();

  /* Audio out plugin */
  audio_driver = load_audio_out_driver(audio_driver_id);


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
  xine_register_event_listener(gGui->xine, event_listener, (void *) gGui);

  /*
   * Register codec reporting
   */
  xine_register_report_codec_cb(gGui->xine, codec_reporting, (void *) gGui );
  unhandled_codec_mode = gGui->config->register_enum (gGui->config,
                         "gui.warn_unhandled_codec", 3, warn_unhandled_codec,
                         "Display popup window on unhandled codecs",
                         NULL, unhandled_codec_mode_cb, NULL);
  
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
