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
 * xine main for aalib
 *
 * Changes: 
 * - first working version (guenter)
 * - autoquit patch from Jens Viebig (siggi)
 *
 */

#ifndef __sun
#define _XOPEN_SOURCE 500
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <aalib.h>

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

#include "utils.h"

#include "xine.h"

/*
 * global variables
 */
typedef struct {
  xine_t           *xine;
  int               ignore_status;
  vo_driver_t      *vo_driver;
  config_values_t  *config;
  aa_context       *context;
  ao_driver_t      *ao_driver;
  char             *mrl[1024];
  int               num_mrls;
  int               current_mrl;
  int               running;
  int               auto_quit;
#ifdef DEBUG
  int               debug_level;
#endif
} aaxine_t;

static aaxine_t aaxine;

/* options args */
static const char *short_options = "?h"
#ifdef DEBUG
 "d:"
#endif
 "a:qA:R::v";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h' },
#ifdef DEBUG
  {"debug"          , required_argument, 0, 'd' },
#endif
  {"audio-channel"  , required_argument, 0, 'a' },
  {"auto-quit"      , no_argument      , 0, 'q' },
  {"audio-driver"   , required_argument, 0, 'A' },
  {"recognize-by"   , optional_argument, 0, 'R' },
  {"version"        , optional_argument, 0, 'v' },
  {0                , no_argument      , 0,  0  }
};

/*
 * Display version.
 */
static void show_version(void) {

  printf("This is xine (aalib ui) - a free video player v%s\n(c) 2000, 2001 by G. Bartsch and the xine project team.\n", VERSION);
}

/*
 * Display an informative banner.
 */
static void show_banner(void) {

  show_version();
  printf("Built with xine library %d.%d.%d [%s]-[%s]-[%s].\n", 
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, 
	 XINE_BUILD_DATE, XINE_BUILD_CC, XINE_BUILD_OS);
  printf("Found xine library version: %d.%d.%d (%s).\n", 
	 xine_get_major_version(), xine_get_minor_version(),
	 xine_get_sub_version(), xine_get_str_version());
}

/*
 * Display full help.
 */
static void print_usage (void) {
  char **driver_ids;
  char  *driver_id;

  printf("usage: aaxine [aalib-options] [aaxine-options] mrl ...\n"
	 "aalib-options:\n"
	 "%s", aa_help);
  printf("\n");
  printf("  -v, --version                Display version.\n");
  printf("AAXINE options:\n");
  printf("  -q, --auto-quit              Quit after playing all mrl's.\n");
  printf("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = xine_list_audio_output_plugins ();
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");
  printf("  -a, --audio-channel <#>      Select audio channel '#'.\n");
  printf("  -R, --recognize-by [option]  Try to recognize stream type. Option are:\n");
  printf("                                 'default': by content, then by extension,\n");
  printf("                                 'revert': by extension, then by content,\n");
  printf("                                 'content': only by content,\n");
  printf("                                 'extension': only by extension.\n");
  printf("                                 -if no option is given, 'revert' is selected\n");
#ifdef DEBUG
  printf("  -d, --debug <flags>          Debug mode for <flags> ('help' for list).\n");
#endif
  printf("\n");
  printf("Examples for valid MRLs (media resource locator):\n");
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
 * Handling xine engine status changes.
 */
static void gui_status_callback (int nStatus) {

  if (aaxine.ignore_status)
    return;
  
  if(nStatus == XINE_STOP) {
    aaxine.current_mrl++;
   
    if (aaxine.current_mrl < aaxine.num_mrls)
      xine_play (aaxine.xine, aaxine.mrl[aaxine.current_mrl], 0, 0 );
    else {
      if (aaxine.auto_quit == 1) {
        aaxine.running = 0;
      }
      aaxine.current_mrl--;
    }
  }

}

/*
 * Return next available mrl to xine engine.
 */
static char *gui_next_mrl_callback (void ) {

  if(aaxine.current_mrl >= (aaxine.num_mrls - 1)) 
    return NULL;
  
  return aaxine.mrl[aaxine.current_mrl + 1];
}

/*
 * Xine engine branch success.
 */
static void gui_branched_callback (void ) {

  if(aaxine.current_mrl < (aaxine.num_mrls - 1)) {
    aaxine.current_mrl++;
  }
}

/*
 * Seek in current stream.
 */
static void set_position (int pos) {

  aaxine.ignore_status = 1;
  xine_play(aaxine.xine, aaxine.mrl[aaxine.current_mrl], pos, 0);
  aaxine.ignore_status = 0;
}

#ifdef DEBUG
/*
 * *FIXME* Turn on some debug info.
 */
static int handle_debug_subopt(char *sopt) {
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
      if((aaxine.debug_level & 0x8000>>(subopt + 1)))
	aaxine.debug_level &= ~0x8000>>(subopt + 1);
      else
	aaxine.debug_level |= 0x8000>>(subopt + 1);
      break;
    }
  }

  return 1;
}
#endif

/*
 * Handle demuxer strategy user choice here.
 */
static int handle_demux_strategy_subopt(char *sopt) {
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
 * Extract mrls from argv[] and store them to playlist.
 */
void extract_mrls(int num_mrls, char **mrls) {
  int i;

  for(i = 0; i < num_mrls; i++)
    aaxine.mrl[i] = mrls[i];

  aaxine.num_mrls = num_mrls;
  aaxine.current_mrl = 0;

}

/*
 * Errrr, i forget my mind ;-).
 */
int main(int argc, char *argv[]) {
  int            c = '?';
  int            option_index    = 0;
  int            key;
  char          *configfile;
  int            demux_strategy  = DEMUX_DEFAULT_STRATEGY;
  char          *audio_driver_id = NULL;
  int            audio_channel   = 0;

  /*
   * Check xine library version 
   */
  if(!xine_check_version(0, 9, 0)) {
    fprintf(stderr, "require xine library version 0.9.0, found %d.%d.%d.\n", 
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    goto failure;
  }

#ifdef DEBUG
  aaxine.debug_level = 0;
#endif
  aaxine.num_mrls = 0;
  aaxine.current_mrl = 0;
  aaxine.auto_quit = 0; /* default: loop forever */

  /* 
   * AALib help and option-parsing
   */
  if (!aa_parseoptions(NULL, NULL, &argc, argv)) {
    print_usage();
    goto failure;
  }

#ifdef DEBUG
  /* If XINE_DEBUG envvar is set, parse it */
  if(getenv("XINE_DEBUG") != NULL) {
    if(!(handle_debug_subopt(chomp((getenv("XINE_DEBUG"))))))
      exit(1);
  }
#endif

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, 
			 long_options, &option_index)) != EOF) {
    switch(c) {

#ifdef DEBUG      
    case 'd': /* Select debug levels */
      if(optarg != NULL) {
	if(!(handle_debug_subopt(chomp(optarg))))
	  exit(1);
      }
      break;
#endif

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &audio_channel);
      break;
      
    case 'q': /* Automatic quit option */
      aaxine.auto_quit = 1;
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

    case 'R': /* Set a strategy to recognizing stream type */
      if(optarg != NULL)
	demux_strategy = handle_demux_strategy_subopt(chomp(optarg));
      else
	demux_strategy = DEMUX_REVERT_STRATEGY;
      break;
     
    case 'v': /* Display version and exit*/
      show_version();
      exit(1);
      break;

    case 'h': /* Display usage */
    case '?':
      print_usage();
      exit(1);
      break;
      
    default:
      print_usage();
      fprintf (stderr, "invalid argument %d => exit\n",c);
      exit(1);
    }
  }

  if(!(argc - optind)) {
    fprintf(stderr, "You should specify at least one MRL.\n");
    goto failure;
  }
  else
    extract_mrls((argc - optind), &argv[optind]);
  
  show_banner();

  /*
   * generate and init a config "object"
   */
  {
    char *homedir;

    homedir = strdup(get_homedir());
    configfile = (char *) xmalloc(strlen(homedir) + 8 + 1);

    sprintf (configfile, "%s/.xinerc", homedir);
  }

  aaxine.config = config_file_init (configfile);
  aaxine.config->set_int(aaxine.config, "demux_strategy", demux_strategy);
  aaxine.config->save(aaxine.config);

  /*
   * Initialize AALib
   */
  aaxine.context = aa_autoinit(&aa_defparams);
  if(aaxine.context == NULL) {
    fprintf(stderr,"Cannot initialize AA-lib. Sorry\n");
    goto failure;
  }

  /*
   * Initialize AA keyboard support.
   * It seems there a little bug if you init
   * this with AA_SENDRELEASE, some keys aren't
   * correctly handled.
   */
  if(aa_autoinitkbd(aaxine.context, 0) < 1) {
    fprintf(stderr, "aa_autoinitkdb() failed.\n");
    goto failure;
  }
  
  aa_hidecursor(aaxine.context);

  /*
   * init video output driver
   */
  aaxine.vo_driver = xine_load_video_output_plugin(aaxine.config, 
						   "aa",
						   VISUAL_TYPE_AA, 
						   (void *) aaxine.context);
  
  if (!aaxine.vo_driver) {
    printf ("main: video driver aa failed\n");
    goto failure;
  }

  /*
   * init audio output driver
   */
  if(!audio_driver_id)
    audio_driver_id = aaxine.config->lookup_str(aaxine.config, 
						"audio_driver_name", "oss");
  else {
    aaxine.config->set_str(aaxine.config, 
			   "audio_driver_name", audio_driver_id);
    aaxine.config->save(aaxine.config);
  }
    
  aaxine.ao_driver = xine_load_audio_output_plugin(aaxine.config,
						   audio_driver_id);
		    
  if (!aaxine.ao_driver) {
    printf ("main: audio driver %s failed\n", audio_driver_id);
  }

  /*
   * xine init
   */
  aaxine.xine = xine_init (aaxine.vo_driver,
			   aaxine.ao_driver, 
			   aaxine.config);

  if(!aaxine.xine) {
    fprintf(stderr, "xine_init() failed.\n");
    goto failure;
  }

  xine_select_audio_channel (aaxine.xine, audio_channel);

  /*
   * ui loop
   */

  xine_play (aaxine.xine, aaxine.mrl[aaxine.current_mrl], 0, 0);

  aaxine.running = 1;

  while (aaxine.running) {
    
    key = AA_NONE;
    while( ((key = aa_getevent(aaxine.context, 0)) == AA_NONE)
	   && aaxine.running );
    
    if((key >= AA_UNKNOWN && key < AA_UNKNOWN) || (key >= AA_RELEASE)) 
      continue;

    switch (key) {

    case AA_UP:
      aaxine.ignore_status = 1;
      xine_stop(aaxine.xine);
      aaxine.current_mrl--;
      if((aaxine.current_mrl >= 0) && (aaxine.current_mrl < aaxine.num_mrls)) {
	xine_play(aaxine.xine, aaxine.mrl[aaxine.current_mrl], 0, 0);
      } 
      else {
	aaxine.current_mrl = 0;
      }
      aaxine.ignore_status = 0;
      break;

    case AA_DOWN:
      aaxine.ignore_status = 1;
      xine_stop(aaxine.xine);
      aaxine.ignore_status = 0;
      gui_status_callback(XINE_STOP);
      break;

    case AA_LEFT:
      if(xine_get_speed(aaxine.xine) > SPEED_PAUSE)
        xine_set_speed(aaxine.xine, xine_get_speed(aaxine.xine) / 2);
      break;
      
    case AA_RIGHT:
      if(xine_get_speed(aaxine.xine) < SPEED_FAST_4) {
        if(xine_get_speed(aaxine.xine) > SPEED_PAUSE)
          xine_set_speed(aaxine.xine, xine_get_speed (aaxine.xine) * 2);
        else
          xine_set_speed(aaxine.xine, SPEED_SLOW_4);
      }
      break;

    case '+':
      xine_select_audio_channel(aaxine.xine,
				(xine_get_audio_channel(aaxine.xine) + 1));
      break;
      
    case '-':
      if(xine_get_audio_channel(aaxine.xine))
	xine_select_audio_channel(aaxine.xine,
				  (xine_get_audio_channel(aaxine.xine) - 1));
      break;

    case 'q':
    case 'Q':
      xine_exit (aaxine.xine);
      aaxine.running = 0;
      break;
      
    case 13:
    case 'r':
    case 'R':
      xine_play (aaxine.xine, aaxine.mrl[aaxine.current_mrl], 0, 0);
      break;

    case ' ':
    case 'p':
    case 'P':
      if (xine_get_speed (aaxine.xine) != SPEED_PAUSE)
	xine_set_speed(aaxine.xine, SPEED_PAUSE);
      else
	xine_set_speed(aaxine.xine, SPEED_NORMAL);
      break;

    case 's':
    case 'S':
      xine_stop (aaxine.xine);
      break;

    case '1':
      set_position (6553);
      break;
    case '2':
      set_position (6553*2);
      break;
    case '3':
      set_position (6553*3);
      break;
    case '4':
      set_position (6553*4);
      break;
    case '5':
      set_position (6553*5);
      break;
    case '6':
      set_position (6553*6);
      break;
    case '7':
      set_position (6553*7);
      break;
    case '8':
      set_position (6553*8);
      break;
    case '9':
      set_position (6553*9);
      break;
    case '0':
      set_position (0);
      break;

    }
  }

 failure:
  
  if(aaxine.xine)
    xine_exit(aaxine.xine); 

  if(aaxine.config) 
    aaxine.config->save(aaxine.config);

  if(aaxine.context) {
    aa_showcursor(aaxine.context);
    aa_uninitkbd(aaxine.context);
    aa_close(aaxine.context);
  }

  return 0;
}
