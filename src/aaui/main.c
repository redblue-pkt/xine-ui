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
ao_functions_t   *ao_driver;
char             *mrl;

void show_banner(void) {

  printf("This is xine (aalib ui) - a free video player v%s\n(c) 2000, 2001 by G. Bartsch and the xine project team.\n", VERSION);
  printf("Built with xine library %d.%d.%d [%s]-[%s]-[%s].\n", 
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, 
	 XINE_BUILD_DATE, XINE_BUILD_CC, XINE_BUILD_OS);
  printf("Found xine library version: %d.%d.%d (%s).\n", 
	 xine_get_major_version(), xine_get_minor_version(),
	 xine_get_sub_version(), xine_get_str_version());
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

void print_usage () {
  printf("usage: aaxine [aalib-options] [-A audio_driver] mrl\n"
	 "aalib-options:\n"
	 "%s", aa_help);
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

void set_position (int pos) {

  xine_stop (xine);
  xine_play (xine, mrl, pos);

}

int main(int argc, char *argv[]) {

  char          *configfile;
  struct termios stored_settings;
  struct termios new_settings;
  char           c;
  int            running;
  char          *ao_drv = "oss";

  /* Check xine library version */
  if(!xine_check_version(0, 5, 0)) {
    fprintf(stderr, "require xine library version 0.5.0, found %d.%d.%d.\n", 
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    exit(1);
  }

  show_banner();

  /* aalib help and option-parsing */
  if (!aa_parseoptions(NULL, NULL, &argc, argv)) {
    print_usage();
    exit(1);
  }

  /*
   * parse command line
   */
  if ((argc!=2) && (argc != 4)) {
    print_usage();
    exit(1);
  }

  if (argc==4) {

    if (strncmp (argv[1], "-A", 2)) {
      print_usage();
      exit(1);
    } 
    
    ao_drv = argv[2];

    mrl = argv[3];

  } else {
    mrl = argv[1];
  }


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
   * init audio output driver
   */
  ao_driver = xine_load_audio_output_plugin(config,
					    ao_drv);

  if (!ao_driver) {
    printf ("main: audio driver %s failed\n", ao_drv);
  }

  /*
   * xine init
   */

  printf ("main: starting xine engine\n");

  xine = xine_init (vo_driver, ao_driver, 
		    config, gui_status_callback,
		    NULL, NULL);

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
      xine_exit (xine);
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

  tcsetattr(0,TCSANOW,&stored_settings);

  aa_close(context);

  return 0;
}
