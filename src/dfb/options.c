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
 * DirectFB UI command line handling 
 * Rich Wareham <richwareham@users.sourceforge.net>
 *
 */

#include "dfb.h"


/* options args */
static const char *short_options = "?h"
 "da:qA:V:R::v";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h' },
  {"debug"          , no_argument      , 0, 'd' },
  {"audio-channel"  , required_argument, 0, 'a' },
  {"auto-quit"      , no_argument      , 0, 'q' },
  {"audio-driver"   , required_argument, 0, 'A' },
  {"video-driver"   , required_argument, 0, 'V' },
  {"recognize-by"   , optional_argument, 0, 'R' },
  {"version"        , no_argument      , 0, 'v' },
  {0                , no_argument      , 0,  0  }
};

/*
 * Display version.
 */
static void show_version(void) {

  printf("This is xine (DirectFB UI) - a free video player v%s\n(c) 2000-2002 by G. Bartsch and the xine project team.\n", VERSION);
}

/*
 * Display an informative banner.
 */
static void show_banner(void) {
  int major, minor, sub;

  show_version();
  printf("Built with xine library %d.%d.%d..\n", 
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION);
  xine_get_version (&major, &minor, &sub);
  printf("Found xine library version: %d.%d.%d.\n", major, minor, sub);
}

/*
 * Display full help.
 */
static void print_usage (void) {
  char   **driver_ids;
  char    *driver_id;
  xine_t  *xine;

  xine = (xine_t *) xine_new();

  printf("usage: dfbxine [aalib-options] [dfbxine-options] mrl ...\n");
  printf("\n");
  printf("  -v, --version                Display version.\n");
  printf("DFBXINE options:\n");
  printf("  -q, --auto-quit              Quit after playing all mrl's.\n");
  printf("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = (char **)xine_list_video_output_plugins (xine);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");
  printf("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = (char **)xine_list_audio_output_plugins (xine);
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
  printf("  -d, --debug                  Show debug messages.\n");
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
  
  xine_exit(xine);
}

int do_command_line(int argc, char **argv) {
  int            c = '?';
  int            option_index    = 0;

  
  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, 
			 long_options, &option_index)) != EOF) {
    switch(c) {

    case 'd': /* Enable debug messages */
      dfbxine.debug_messages = 1;
      break;

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &(dfbxine.audio_channel));
      break;
      
    case 'q': /* Automatic quit option */
      dfbxine.auto_quit = 1;
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
	dfbxine.audio_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strcpy (dfbxine.audio_driver_id, optarg);
      } else {
	fprintf (stderr, "audio driver id required for -A option\n");
	exit (1);
      }
      break;

    case 'V': /* Select video driver */
      if(optarg != NULL) {
	dfbxine.video_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strcpy (dfbxine.video_driver_id, optarg);
      } else {
	fprintf (stderr, "video driver id required for -V option\n");
	exit (1);
      }
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
    return 0;
  }
  else
    extract_mrls((argc - optind), &argv[optind]);
 
 
  /*
   * Initialize DirectFB and parse command line.
   */
  DFBCHECK (DirectFBInit (&argc, &argv));    

  show_banner();
  return 1;
}
