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
 */

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
#include <pwd.h>
#include <aalib.h>
#include <termios.h>

#include "xine.h"

/*
 * global variables
 */

vo_driver_t      *vo_driver;
config_values_t  *config;
xine_t           *xine;
aa_context       *context;

void show_banner(void) {

  printf("This is xine (aalib ui) - a free video player v%s\n(c) 2000, 2001 by G. Bartsch and the xine project team.\n", VERSION);
  printf("Built with xine library %d.%d.%d [%s]-[%s]-[%s].\n", 
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, 
	 XINE_BUILD_DATE, XINE_BUILD_CC, XINE_BUILD_OS);
  printf("Found xine library version: %d.%d.%d (%s).\n", 
	 xine_get_major_version(), xine_get_minor_version(),
	 xine_get_sub_version(), xine_get_str_version());
}

void show_usage (void) {
  
  printf("\n");
  printf("Usage: aaxine MRL\n");
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

void gui_status_callback (int nStatus) {
}

const char *get_homedir(void) {
  struct passwd *pw = NULL;
  char *homedir = NULL;
#ifdef HAVE_GETPWUID_R
  int ret;
  struct passwd pwd;
  char *buffer = NULL;
  int bufsize = 128;

  buffer = (char *) xmalloc(bufsize);
  
  if((ret = getpwuid_r(getuid(), &pwd, buffer, bufsize, &pw)) < 0) {
#else
  if((pw = getpwuid(getuid())) == NULL) {
#endif
    if((homedir = getenv("HOME")) == NULL) {
      fprintf(stderr, "Unable to get home directory, set it to /tmp.\n");
      homedir = strdup("/tmp");
    }
  }
  else {
    if(pw) 
      homedir = strdup(pw->pw_dir);
  }
  
  
#ifdef HAVE_GETPWUID_R
  if(buffer) 
    free(buffer);
#endif
  
  return homedir;
}

int main(int argc, char *argv[]) {

  char          *mrl;
  char          *configfile;
  struct termios stored_settings;
  struct termios new_settings;
  char           c;
  int            running;

  /* Check xine library version */
  if(!xine_check_version(0, 5, 0)) {
    fprintf(stderr, "Require xine library version 0.5.0, found %d.%d.%d.\n", 
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    exit(1);
  }

  show_banner();

  /* aalib help and option-parsing */
 if(!aa_parseoptions(NULL, NULL, &argc, argv) || argc != 2) {
    printf("Usage: %s [options]\n"
           "Options:\n"
           "%s", argv[0], aa_help);
    exit(1);
  }

  /*
   * parse command line
   */
  if (argc<2) {
    show_usage();
    exit(1);
  }

  mrl = argv[1];


  /*
   * generate and init a config "object"
   */
  {
    char *homedir;

    homedir = strdup(get_homedir());
    configfile = (char *) malloc(strlen(homedir) + 8 + 1);

    sprintf (configfile, "%s/.xinerc", homedir);
  }

  config = config_file_init (configfile);


  /*
   * init video output driver
   */

  context = aa_autoinit(&aa_defparams);
  if(context == NULL) {
    fprintf(stderr,"Cannot initialize AA-lib. Sorry\n");
    exit(1);
  }

  vo_driver = xine_load_video_output_plugin(config, 
					    "aa",
					    VISUAL_TYPE_AA, 
					    (void *) context);
  
  if (!vo_driver) {
    printf ("main: video driver aa failed\n");
    exit (1);
  }

  /*
   * xine init
   */

  printf ("main: starting xine engine\n");

  xine = xine_init (vo_driver, NULL, 
		    gui_status_callback, config);

  xine_select_audio_channel (xine, 0);

  /*
   * ui loop
   */

  tcgetattr(0,&stored_settings);
  
  new_settings = stored_settings;
  
  /* Disable canonical mode, and set buffer size to 1 byte */
  new_settings.c_lflag &= (~ICANON);
  new_settings.c_lflag &= (~ECHO);
  new_settings.c_cc[VTIME] = 0;
  new_settings.c_cc[VMIN] = 1;
  
  tcsetattr(0,TCSANOW,&new_settings);

  xine_play (xine, mrl, 0);

  running = 1;

  while (running) {

    c = getc(stdin);

    switch (c) {
    case 'q':
    case 'Q':
      running = 0;
      break;
      
    case 13:
    case 'r':
    case 'R':
      printf ("PLAY\n");
      xine_play (xine, mrl, 0);
      break;

    case ' ':
    case 'p':
    case 'P':
      xine_pause (xine);
      break;

    case 's':
    case 'S':
      xine_stop (xine);
      break;
    }
  }

  tcsetattr(0,TCSANOW,&stored_settings);

  aa_close(context);

  return 0;
}
