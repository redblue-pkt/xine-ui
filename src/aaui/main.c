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
 * - more using of aalib, many changes, new api port (daniel)
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
#include <aalib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#include <xine.h>
#include <xine/xineutils.h>

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

/*
 * global variables
 */
typedef struct {
  xine_t            *xine;
  xine_vo_driver_t  *vo_driver;
  xine_ao_driver_t  *ao_driver;
  xine_stream_t     *stream;

  int                ignore_status;
  char              *configfile;
  aa_context        *context;
  char              *mrl[1024];
  int                num_mrls;
  int                current_mrl;
  int                running;
  int                auto_quit;
  
  struct {
    int                enable;
    int                caps;
    int                volume_level;
    int                mute;
  } mixer;

  int               debug_messages;
} aaxine_t;

static aaxine_t aaxine;

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
  {"version"        , no_argument      , 0, 'v' },
  {0                , no_argument      , 0,  0  }
};

/*
 * Some config wrapper functions, because it's too much work for updating one entry.
 */
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
    {
      char *newv = string;
      char *oldv = entry->str_value;
      
      oldv = entry->str_value;
      entry->str_value = newv;
      
      if(oldv)
	free(oldv);
    }
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
  
  xine_config_update_entry(aaxine.xine, entry);
}

#if 0 /* No used yet */
static void config_update_range(char *key, int min, int max) {
  xine_cfg_entry_t entry;
  
  if(xine_config_lookup_entry(aaxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}
static void config_update_enum(char *key, int value) {
  xine_cfg_entry_t entry;
  
  if(xine_config_lookup_entry(aaxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

static void config_update_bool(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(aaxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

static void config_update_num(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(aaxine.xine, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}
#endif

static void config_update_string(char *key, char *string) {
  xine_cfg_entry_t entry;
  
  memset(&entry, 0, sizeof(entry));
  if (xine_config_lookup_entry(aaxine.xine, key, &entry) && string)
    config_update(&entry, XINE_CONFIG_TYPE_STRING, 0, 0, 0, string);
  else {
    if(string == NULL)
      fprintf(stderr, "string is NULL\n");
    else
      fprintf(stderr, "WOW, key %s isn't registered\n", key);
  }
}

/*
 * Display version.
 */
static void show_version(void) {
  
  printf("This is xine (aalib ui) - a free video player v%s\n"
	 "(c) 2000-2002 by G. Bartsch and the xine project team.\n", VERSION);
}

/*
 * Display an informative banner.
 */
static void show_banner(void) {
  int major, minor, sub;
  
  show_version();
  
  printf("Built with xine library %d.%d.%d (%s).\n", 
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, XINE_VERSION);

  xine_get_version (&major, &minor, &sub);
  printf("Found xine library version: %d.%d.%d (%s).\n", 
	 major, minor, sub, xine_get_version_string());
}

/*
 * Display full help.
 */
static void print_usage (void) {
  const char *const *driver_ids;
  char              *driver_id;
  xine_t            *xine;

  xine = (xine_t *)xine_new();
  
  printf("usage: aaxine [aalib-options] [aaxine-options] mrl ...\n"
	 "aalib-options:\n"
	 "%s", aa_help);
  printf("\n");
  printf("  -v, --version                Display version.\n");
  printf("AAXINE options:\n");
  printf("  -q, --auto-quit              Quit after playing all mrl's.\n");
  printf("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = xine_list_video_output_plugins (xine);
  driver_id  = (char *)*driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = (char *)*driver_ids++;
  }
  printf ("\n");
  printf("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = xine_list_audio_output_plugins (xine);
  driver_id  = (char *)*driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = (char *)*driver_ids++;
  }
  printf ("\n");
  printf("  -a, --audio-channel <#>      Select audio channel '#'.\n");
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

/*
 * Handling xine engine status changes.
 */
static void gui_status_callback (int nStatus) {

  if (aaxine.ignore_status)
    return;
  
  if(nStatus == XINE_STATUS_STOP) {
    aaxine.current_mrl++;
   
    if (aaxine.current_mrl < aaxine.num_mrls) {
      if(xine_open(aaxine.stream, aaxine.mrl[aaxine.current_mrl])) {
	if(!xine_play (aaxine.stream, 0, 0))
	  fprintf(stderr, "xine_play() failed.\n");
      }
      else
	fprintf(stderr, "xine_open() failed\n");
    }
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
  xine_play(aaxine.stream, pos, 0);
  aaxine.ignore_status = 0;
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
  int                      c = '?';
  int                      option_index    = 0;
  int                      key;
  char                    *audio_driver_id = NULL;
  char                    *video_driver_id = NULL;
  int                      audio_channel   = 0;
  char                    *driver_name;

  /*
   * Check xine library version 
   */
  if(!xine_check_version(0, 9, 14)) {
    int major, minor, sub;
    
    xine_get_version (&major, &minor, &sub);
    fprintf(stderr, "require xine library version 0.9.14, found %d.%d.%d.\n", major, minor, sub);
    goto failure;
  }
  
  aaxine.num_mrls = 0;
  aaxine.current_mrl = 0;

  aaxine.auto_quit = 0; /* default: loop forever */
  aaxine.debug_messages = 0;

  /* 
   * AALib help and option-parsing
   */
  if (!aa_parseoptions(NULL, NULL, &argc, argv)) {
    print_usage();
    goto failure;
  }

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(argc, argv, short_options, 
			 long_options, &option_index)) != EOF) {
    switch(c) {

    case 'd': /* Enable debug messages */
      aaxine.debug_messages = 1;
      break;

    case 'a': /* Select audio channel */
      sscanf(optarg, "%i", &audio_channel);
      break;
      
    case 'q': /* Automatic quit option */
      aaxine.auto_quit = 1;
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL)
	audio_driver_id = strdup(optarg);
      else {
	fprintf (stderr, "audio driver id required for -A option\n");
	exit (1);
      }
      break;

    case 'V': /* Select video driver */
      if(optarg != NULL)
	video_driver_id = strdup(optarg);
      else {
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
    goto failure;
  }
  else
    extract_mrls((argc - optind), &argv[optind]);
  
  show_banner();

  /*
   * generate and init a config "object"
   */
  {
    char *cfgfile = ".xine/config2";
    
    if (!(aaxine.configfile = getenv("XINERC"))) {
      aaxine.configfile = 
	(char *) xine_xmalloc((strlen((xine_get_homedir())) + strlen(cfgfile)) + 2);
      sprintf(aaxine.configfile, "%s/%s", (xine_get_homedir()), cfgfile);
    }
      
  }

  aaxine.xine = (xine_t *)xine_new();
  xine_config_load (aaxine.xine, aaxine.configfile);

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
  if(!video_driver_id)
    video_driver_id = "aa";
  
  aaxine.vo_driver = xine_open_video_driver(aaxine.xine,
					    video_driver_id,
					    XINE_VISUAL_TYPE_AA, 
					    (void *)aaxine.context);
  
  if (!aaxine.vo_driver) {
    aaxine.vo_driver = xine_open_video_driver(aaxine.xine, 
					      video_driver_id,
					      XINE_VISUAL_TYPE_FB, 
					      NULL);
  }
    
  if (!aaxine.vo_driver) {
    printf ("main: video driver %s failed\n", video_driver_id);
    goto failure;
  }

  /*
   * init audio output driver
   */
  driver_name = (char *)xine_config_register_string (aaxine.xine, 
						     "audio.driver",
						     "oss",
						     "audio driver to use",
						     NULL,
						     20,
						     NULL,
						     NULL);
  
  if(!audio_driver_id)
    audio_driver_id = driver_name;
  else
    config_update_string ("audio.driver", audio_driver_id);
  
  aaxine.ao_driver = xine_open_audio_driver(aaxine.xine, audio_driver_id, NULL);
  
  if (!aaxine.ao_driver)
    printf ("main: audio driver %s failed\n", audio_driver_id);
  
  /*
   * xine init
   */
  xine_init (aaxine.xine);
  
  if(!aaxine.xine) {
    fprintf(stderr, "xine_init() failed.\n");
    goto failure;
  }

  aaxine.stream = xine_stream_new(aaxine.xine, aaxine.ao_driver, aaxine.vo_driver);
  
  /* Init mixer control */
  aaxine.mixer.enable = 0;
  aaxine.mixer.caps = 0;
  
  if(xine_get_param(aaxine.stream, XINE_PARAM_AO_MIXER_VOL))
    aaxine.mixer.caps |= XINE_PARAM_AO_MIXER_VOL;
  if(xine_get_param(aaxine.stream, XINE_PARAM_AO_PCM_VOL))
    aaxine.mixer.caps |= XINE_PARAM_AO_PCM_VOL;
  if(xine_get_param(aaxine.stream, XINE_PARAM_AO_MUTE))
    aaxine.mixer.caps |= XINE_PARAM_AO_MUTE;

  if(aaxine.mixer.caps & (XINE_PARAM_AO_MIXER_VOL | XINE_PARAM_AO_PCM_VOL)) { 
    aaxine.mixer.enable = 1;
    aaxine.mixer.volume_level = xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME);
  }
  
  if(aaxine.mixer.caps & XINE_PARAM_AO_MUTE)
    aaxine.mixer.mute = xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_MUTE);
  
  /* Select audio channel */
  xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, audio_channel);

  /* Kick off terminal pollution */
  if((!aaxine.debug_messages)
     && ((!strcasecmp(aaxine.context->driver->shortname, "linux"))
         || (!strcasecmp(aaxine.context->driver->shortname, "X11")))) {
    int error_fd;
    
    if ((error_fd = open("/dev/null", O_WRONLY)) < 0)
         printf("cannot open /dev/null");
    else {
      if (dup2(error_fd, STDOUT_FILENO) < 0)
	printf("cannot dup2 stdout");
      if (dup2(error_fd, STDERR_FILENO) < 0)
	printf("cannot dup2 stderr");
    }
  }
  
  /*
   * ui loop
   */

  if(xine_open(aaxine.stream, aaxine.mrl[aaxine.current_mrl])) {
    if(!xine_play (aaxine.stream, 0, 0))
      fprintf(stderr, "xine_play() failed.\n");
  }
  else
    fprintf(stderr, "xine_open() failed.\n");

  aaxine.running = 1;

  while (aaxine.running) {
    
    key = AA_NONE;
    while(((key = aa_getevent(aaxine.context, 0)) == AA_NONE) && aaxine.running );
    
    if((key >= AA_UNKNOWN && key < AA_UNKNOWN) || (key >= AA_RELEASE)) 
      continue;

    switch (key) {

    case AA_UP:
      aaxine.ignore_status = 1;
      xine_stop(aaxine.stream);
      aaxine.current_mrl--;
      if((aaxine.current_mrl >= 0) && (aaxine.current_mrl < aaxine.num_mrls)) {
	if(xine_open(aaxine.stream, aaxine.mrl[aaxine.current_mrl])) {
	  if(!xine_play(aaxine.stream, 0, 0))
	    fprintf(stderr, "xine_play() failed.\n");
	}
	else
	  fprintf(stderr, "xine_open() failed.\n");
      } 
      else {
	aaxine.current_mrl = 0;
      }
      aaxine.ignore_status = 0;
      break;

    case AA_DOWN:
      aaxine.ignore_status = 1;
      xine_stop(aaxine.stream);
      aaxine.ignore_status = 0;
      gui_status_callback(XINE_STATUS_STOP);
      break;

    case AA_LEFT:
      if(xine_get_param(aaxine.stream, XINE_PARAM_SPEED) > XINE_SPEED_PAUSE)
        xine_set_param(aaxine.stream, XINE_PARAM_SPEED, 
		       (xine_get_param(aaxine.stream, XINE_PARAM_SPEED)) / 2);
      break;
      
    case AA_RIGHT:
      if(xine_get_param(aaxine.stream, XINE_PARAM_SPEED) < XINE_SPEED_FAST_4) {
        if(xine_get_param(aaxine.stream, XINE_PARAM_SPEED) > XINE_SPEED_PAUSE)
          xine_set_param(aaxine.stream, XINE_PARAM_SPEED, 
			 (xine_get_param(aaxine.stream, XINE_PARAM_SPEED)) * 2);
        else
          xine_set_param(aaxine.stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
      }
      break;
      
    case '+':
      xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
		     (xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL)) + 1);
      break;
      
    case '-':
      if(xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL))
	xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, 
		       (xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL)) - 1);
      break;
      
    case 'q':
    case 'Q':
      aaxine.running = 0;
      break;
      
    case 13:
    case 'r':
    case 'R':
      if(xine_open(aaxine.stream, aaxine.mrl[aaxine.current_mrl])) {
	if(!xine_play (aaxine.stream, 0, 0))
	  fprintf(stderr, "xine_play() failed.\n");
      }
      else
	fprintf(stderr, "xine_open() failed.\n");
      break;

    case ' ':
    case 'p':
    case 'P':
      if (xine_get_param (aaxine.stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
	xine_set_param(aaxine.stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
      else
	xine_set_param(aaxine.stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
      break;

    case 's':
    case 'S':
      xine_stop (aaxine.stream);
      break;

    case 'V':
      if(aaxine.mixer.enable) {
	if(aaxine.mixer.caps & (XINE_PARAM_AO_MIXER_VOL | XINE_PARAM_AO_PCM_VOL)) { 
	  if(aaxine.mixer.volume_level < 100) {
	    aaxine.mixer.volume_level++;
	    xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME, aaxine.mixer.volume_level);
	  }
	}
      }
      break;
      
    case 'v':
      if(aaxine.mixer.enable) {
	if(aaxine.mixer.caps & (XINE_PARAM_AO_MIXER_VOL | XINE_PARAM_AO_PCM_VOL)) { 
	  if(aaxine.mixer.volume_level > 0) {
	    aaxine.mixer.volume_level--;
	    xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME, aaxine.mixer.volume_level);
	  }
	}
      }
      break;
      
    case 'm':
    case 'M':
      if(aaxine.mixer.enable) {
	if(aaxine.mixer.caps & XINE_PARAM_AO_MUTE) {
	  aaxine.mixer.mute = !aaxine.mixer.mute;
	  xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_MUTE, aaxine.mixer.mute);
	}
      }
      break;

    case '1':
      set_position (6553);
      break;
    case '2':
      set_position (6553 * 2);
      break;
    case '3':
      set_position (6553 * 3);
      break;
    case '4':
      set_position (6553 * 4);
      break;
    case '5':
      set_position (6553 * 5);
      break;
    case '6':
      set_position (6553 * 6);
      break;
    case '7':
      set_position (6553 * 7);
      break;
    case '8':
      set_position (6553 * 8);
      break;
    case '9':
      set_position (6553 * 9);
      break;
    case '0':
      set_position (0);
      break;

    }
  }

 failure:
  
  if(aaxine.xine) 
    xine_config_save(aaxine.xine, aaxine.configfile);
  
  if(aaxine.xine)
    xine_exit(aaxine.xine); 

  xine_close_video_driver(aaxine.xine, aaxine.vo_driver);

  if(aaxine.ao_driver)
    xine_close_audio_driver(aaxine.xine, aaxine.ao_driver);
  
  if(aaxine.context) {
    aa_showcursor(aaxine.context);
    aa_uninitkbd(aaxine.context);
    aa_close(aaxine.context);
  }

  if(aaxine.configfile)
    free(aaxine.configfile);

  return 0;
}
