/*
 * Copyright (C) 2000-2020 the xine project
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
 * xine main for aalib and caca
 *
 * Changes:
 * - first working version (guenter)
 * - autoquit patch from Jens Viebig (siggi)
 * - more using of aalib, many changes, new api port (daniel)
 * - added caca (frantisek)
 *
 */

#ifndef __sun
#define _XOPEN_SOURCE 500
#endif
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <globals.h>

#include "../xitk/common.h"

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

#ifdef AA
#  include <aalib.h>
#endif

#ifdef CACA
#  include <caca.h>
#  ifdef CUCUL
#    include <cucul.h>
#    define caca_canvas_t cucul_canvas_t
#    define caca_create_canvas cucul_create_canvas
#    define caca_free_canvas cucul_free_canvas
#  endif
#endif

/* Sound mixer capabilities */
#define MIXER_CAP_NOTHING   0x00000000
#define MIXER_CAP_VOL       0x00000001
#define MIXER_CAP_MUTE      0x00000002

/*
 * global variables
 */
typedef struct {
  xine_video_port_t   *vo_port;
  xine_audio_port_t   *ao_port;
  xine_stream_t       *stream;
  xine_post_t         *post_plugin;
  xine_event_queue_t  *event_queue;

  int                  logo_mode;
  int                  ignore_next;
#ifdef AA
  aa_context          *context;
#endif
#ifdef CACA
  caca_canvas_t       *canvas;
  caca_display_t      *display;
#endif
  char                *mrl[1024];
  int                  num_mrls;
  int                  current_mrl;
  int                  running;
  int                  auto_quit;

  int                  no_post;
  char                *post_plugin_name;
  int                  post_running;

  struct {
    int                enable;
    int                caps;
    int                volume_level;
    int                mute;
  } mixer;

  int                  debug_messages;
} aaxine_t;

static aaxine_t  aaxine;
void            *xlib_handle = NULL;

#ifdef AA
#  define CONFIGFILE "aaxine_config"

#  define KEY_UP AA_UP
#  define KEY_DOWN AA_DOWN
#  define KEY_LEFT AA_LEFT
#  define KEY_RIGHT AA_RIGHT
#endif

#ifdef CACA
#  define CONFIGFILE "cacaxine_config"

#  define KEY_UP CACA_KEY_UP
#  define KEY_DOWN CACA_KEY_DOWN
#  define KEY_LEFT CACA_KEY_LEFT
#  define KEY_RIGHT CACA_KEY_RIGHT
#endif

/* options args */
static const char *short_options = "?h"
 "d::a:qA:V:R::NP:v";
static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h' },
  {"debug"          , optional_argument, 0, 'd' },
  {"audio-channel"  , required_argument, 0, 'a' },
  {"auto-quit"      , no_argument      , 0, 'q' },
  {"audio-driver"   , required_argument, 0, 'A' },
  {"video-driver"   , required_argument, 0, 'V' },
  {"no-post"        , no_argument      , 0, 'N' },
  {"post-plugin"    , required_argument, 0, 'P' },
  {"version"        , no_argument      , 0, 'v' },
  {0                , no_argument      , 0,  0  }
};


static void post_plugin_cb(void *data, xine_cfg_entry_t *entry) {
  (void)data;
  aaxine.post_plugin_name = entry->str_value;
}

#if 0 /* No used yet */
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

  xine_config_update_entry(__xineui_global_xine_instance, entry);
}

static void config_update_range(char *key, int min, int max) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_RANGE, min, max, 0, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}
static void config_update_enum(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_ENUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

static void config_update_bool(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_BOOL, 0, 0, ((value > 0) ? 1 : 0), NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

static void config_update_num(char *key, int value) {
  xine_cfg_entry_t entry;

  if(xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry))
    config_update(&entry, XINE_CONFIG_TYPE_NUM, 0, 0, value, NULL);
  else
    fprintf(stderr, "WOW, key %s isn't registered\n", key);
}

static void config_update_string(char *key, char *string) {
  xine_cfg_entry_t entry;

  memset(&entry, 0, sizeof(entry));
  if (xine_config_lookup_entry(__xineui_global_xine_instance, key, &entry) && string)
    config_update(&entry, XINE_CONFIG_TYPE_STRING, 0, 0, 0, string);
  else {
    if(string == NULL)
      fprintf(stderr, "string is NULL\n");
    else
      fprintf(stderr, "WOW, key %s isn't registered\n", key);
  }
}
#endif

/*
 * Display version.
 */
static void show_version(void) {
#ifdef AA
  printf("This is xine (aalib ui) - a free video player v%s\n"
	 "(c) 2000-2019 by G. Bartsch and the xine project team.\n", VERSION);
#endif
#ifdef CACA
  printf("This is xine (caca ui) - a free video player v%s\n"
	 "(c) 2000-2019 The xine Team.\n", VERSION);
#endif
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
  const char *const *post_ids;
  char              *post_id;
  xine_t            *xine;
  const char        *cfgdir = ".xine";
  const char        *cfgfile = CONFIGFILE;
  char              *configfile;

  if(!(configfile = getenv("XINERC"))) {
    configfile = (char *) malloc(strlen(xine_get_homedir())
				       + strlen(cfgdir)
				       + strlen(cfgfile)
				       + 3);
    if (configfile) {
      sprintf(configfile, "%s/%s", xine_get_homedir(), cfgdir);
      if (mkdir(configfile, 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "Error creating %s: %d (%s)\n", configfile, errno, strerror(errno));
      }
      sprintf(configfile + strlen(configfile), "/%s", cfgfile);
    }
  }

  xine = xine_new();
  if (configfile)
    xine_config_load(xine, configfile);
  xine_init(xine);

#ifdef AA
  printf("usage: aaxine [aalib-options] [aaxine-options] mrl ...\n"
	 "aalib-options:\n"
	 "%s", aa_help);
  printf("\n");
  printf("AAXINE options:\n");
#endif
#ifdef CACA
  printf("usage: cacaxine [cacaxine-options] mrl ...\n");
  printf("CACAXINE options:\n");
#endif
  printf("  -v, --version                Display version.\n");
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
  printf("  -P, --post-plugin <name>     Plugin <name> for video less stream animation:\n");
  printf("                                 (default is the first).\n");
  printf("                               ");
  post_ids = xine_list_post_plugins_typed(xine, XINE_POST_TYPE_AUDIO_VISUALIZATION);
  post_id  = (char *)*post_ids++;
  while (post_id) {
    printf ("%s ", post_id);
    post_id  = (char *)*post_ids++;
  }
  printf("\n");
  printf("  -N, --no-post                Don't use post effect at all\n");
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

static void aaxine_handle_xine_error(xine_stream_t *stream) {
  int err;

  err = xine_get_error(stream);

  switch(err) {

  case XINE_ERROR_NONE:
    /* noop */
    break;

  case XINE_ERROR_NO_INPUT_PLUGIN:
    fprintf(stderr, "- xine engine error -\n"
	    "There is no input plugin available to handle '%s'.\n"
	    "Maybe MRL syntax is wrong or file/stream source doesn't exist.\n",
	    aaxine.logo_mode ? XINE_LOGO_MRL : aaxine.mrl[aaxine.current_mrl]);
    break;

  case XINE_ERROR_NO_DEMUX_PLUGIN:
    fprintf(stderr, "- xine engine error -\n"
	    "There is no demuxer plugin available to handle '%s'.\n"
	    "Usually this means that the file format was not recognized.\n",
	    aaxine.logo_mode ? XINE_LOGO_MRL : aaxine.mrl[aaxine.current_mrl]);
    break;

  case XINE_ERROR_DEMUX_FAILED:
    fprintf(stderr, "- xine engine error -\n"
	    "Demuxer failed. Maybe '%s' is a broken file?\n",
	    aaxine.logo_mode ? XINE_LOGO_MRL : aaxine.mrl[aaxine.current_mrl]);
    break;

  case XINE_ERROR_MALFORMED_MRL:
    fprintf(stderr, "- xine engine error -\n"
	    "Malformed mrl. Mrl '%s' seems malformed/invalid.\n",
	    aaxine.logo_mode ? XINE_LOGO_MRL : aaxine.mrl[aaxine.current_mrl]);
    break;

  default:
    fprintf(stderr, "- xine engine error -\n!! Unhandled error !!\n");
    break;
  }

}

static int aaxine_xine_play(xine_stream_t *stream, int start_pos, int start_time_in_secs) {
  int ret;
  int has_video;

  if(start_time_in_secs)
    start_time_in_secs *= 1000;

  has_video = xine_get_stream_info(stream, XINE_STREAM_INFO_HAS_VIDEO);

  if(has_video)
    has_video = !xine_get_stream_info(stream, XINE_STREAM_INFO_IGNORE_VIDEO);

  if(has_video && aaxine.post_running) {
    xine_post_out_t * audio_source;

    audio_source = xine_get_audio_source(stream);
    if(xine_post_wire_audio_port(audio_source, aaxine.ao_port))
      aaxine.post_running = 0;

  }
  else if((!has_video) && (!aaxine.no_post) && (!aaxine.post_running)) {
    xine_post_out_t * audio_source;

    audio_source = xine_get_audio_source(stream);
    if(xine_post_wire_audio_port(audio_source, aaxine.post_plugin->audio_input[0]))
      aaxine.post_running = 1;

  }

  if((ret = xine_play(stream, start_pos, start_time_in_secs)) == 0)
    aaxine_handle_xine_error(stream);
  else {
    if(aaxine.logo_mode != 2)
      aaxine.logo_mode = 0;
  }

  return ret;
}

static int aaxine_xine_open_and_play(const char *mrl, int start_pos, int start_time) {


  if(!xine_open(aaxine.stream, mrl)) {
    aaxine_handle_xine_error(aaxine.stream);
    return 0;
  }

  if(!aaxine_xine_play(aaxine.stream, start_pos, start_time))
    return 0;

  return 1;
}

/*
 * playback logo mrl
 */
static void aaxine_play_logo(void) {

  if(xine_get_status(aaxine.stream) == XINE_STATUS_PLAY)
    return;

  aaxine.logo_mode = 2;

  if(xine_get_status(aaxine.stream) == XINE_STATUS_PLAY) {
    aaxine.ignore_next = 1;
    xine_stop(aaxine.stream);
    aaxine.ignore_next = 0;
  }

  aaxine_xine_open_and_play(XINE_LOGO_MRL, 0, 0);
  aaxine.logo_mode = 1;
}

/*
 * Handling xine engine status changes.
 */
static void aaxine_start_next(void) {

  if(aaxine.ignore_next)
    return;

  aaxine.current_mrl++;

  if (aaxine.current_mrl < aaxine.num_mrls) {
    if(!aaxine_xine_open_and_play(aaxine.mrl[aaxine.current_mrl], 0, 0))
      aaxine_play_logo();
  }
  else {
    aaxine.current_mrl--;
    aaxine_play_logo();

    if ((aaxine.auto_quit == 1) && (aaxine.logo_mode))
      aaxine.running = 0;
  }

}

static void aaxine_event_listener(void *user_data, const xine_event_t *event) {
  struct timeval tv;
  (void)user_data;

  if(aaxine.logo_mode)
    return;

  gettimeofday (&tv, NULL);

  if((tv.tv_sec - event->tv.tv_sec) > 3) {
    printf("Event too old, discarding\n");
    return;
  }

  switch(event->type) {

    /* frontend can e.g. move on to next playlist entry */
  case XINE_EVENT_UI_PLAYBACK_FINISHED:
    aaxine_start_next();
    break;

  case XINE_EVENT_QUIT:
    break;

  case XINE_EVENT_PROGRESS:
    {
      xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;

      fprintf(stderr, "XINE_EVENT_PROGRESS: %s [%d%%]\n", pevent->description, pevent->percent);
    }
    break;
  }
}

#ifdef AA
static void aaxine_resize_handler(aa_context *context) {
  aa_resize(context);
}
#endif

/*
 * Seek in current stream.
 */
static void set_position (int pos) {
  aaxine_xine_play(aaxine.stream, pos, 0);
}

/*
 * Extract mrls from argv[] and store them to playlist.
 */
static void extract_mrls(int num_mrls, char **mrls) {
  int i;

  for(i = 0; i < num_mrls; i++)
    aaxine.mrl[i] = mrls[i];

  aaxine.num_mrls = num_mrls;
  aaxine.current_mrl = 0;

}

static void xinit_thread(void) {
   int    (*x_init_threads)(void);

   dlerror();
   if((xlib_handle = dlopen("libX11.so", RTLD_LAZY)) != NULL) {

     if((x_init_threads = dlsym(xlib_handle, "XInitThreads")) != NULL)
       x_init_threads();
     else
       printf("dlsym() failed: %s\n", dlerror());
   }
   else
     printf("dlopen() failed: %s\n", dlerror());
}

static int aaxine_get_key_event(void) {
#ifdef AA
  int key;

  key = AA_NONE;
  while(((key = aa_getevent(aaxine.context, 0)) == AA_NONE) && aaxine.running)
    usleep(50000);

  if (key >= AA_RELEASE || key == AA_NONE) return 0;

  return key;
#endif

#ifdef CACA
  caca_event_t ev;

  while ( !caca_get_event(aaxine.display, CACA_EVENT_KEY_PRESS, &ev, 50000) && aaxine.running ) ;

  if (!aaxine.running) return 0;
#ifdef CACA_EVENT_OPAQUE
  return caca_get_event_key_ch(&ev);
#else
  return ev.data.key.ch;
#endif
#endif
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
  char                    *post_plugin_name = NULL;

  /*
   * Check xine library version
   */
  if(!xine_check_version(1, 0, 0)) {
    int major, minor, sub;

    xine_get_version (&major, &minor, &sub);
    fprintf(stderr, "require xine library version 1.0.0, found %d.%d.%d.\n", major, minor, sub);
    goto failure;
  }

  aaxine.num_mrls = 0;
  aaxine.current_mrl = 0;

  aaxine.auto_quit = 0; /* default: loop forever */
  aaxine.debug_messages = 0;
  aaxine.ignore_next = 0;
  aaxine.no_post = 0;
  aaxine.post_running = 0;
  aaxine.post_plugin_name = NULL;

#ifdef CACA
  aaxine.display = NULL;
#endif

#ifdef AA
  /*
   * AALib help and option-parsing
   */
  if (!aa_parseoptions(NULL, NULL, &argc, argv)) {
    print_usage();
    goto failure;
  }
#endif

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(argc, argv, short_options,
			 long_options, &option_index)) != EOF) {
    switch(c) {

    case 'd': /* Enable debug messages */
      if(optarg != NULL) {
	char *p = xine_chomp(optarg);

	if(p && strlen(p))
	  aaxine.debug_messages = strtol(p, &p, 10);

      }
      else
	aaxine.debug_messages = 1;
      break;

    case 'a': /* Select audio channel */
      if (optarg) {
        sscanf(optarg, "%i", &audio_channel);
      }
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

    case 'P':
      if(!post_plugin_name && optarg)
	post_plugin_name = strdup(optarg);
      break;

    case 'N':
      aaxine.no_post = 1;
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
    const char *cfgdir = ".xine";
    const char *cfgfile = CONFIGFILE;

    if (!(__xineui_global_config_file = getenv ("XINERC"))) {
      __xineui_global_config_file = (char *) malloc(strlen(xine_get_homedir())
						+ strlen(cfgdir)
						+ strlen(cfgfile)
						+ 3);
      if (__xineui_global_config_file) {
        sprintf(__xineui_global_config_file, "%s/%s", xine_get_homedir(), cfgdir);
        if (mkdir(__xineui_global_config_file, 0755) < 0 && errno != EEXIST) {
          fprintf(stderr, "Error creating %s: %d (%s)\n", __xineui_global_config_file, errno, strerror(errno));
        }
        sprintf(__xineui_global_config_file + strlen(__xineui_global_config_file), "/%s", cfgfile);
      }
    }
  }

  __xineui_global_xine_instance = (xine_t *)xine_new();
  if (__xineui_global_config_file)
    xine_config_load (__xineui_global_xine_instance, __xineui_global_config_file);
  xine_engine_set_param(__xineui_global_xine_instance, XINE_ENGINE_PARAM_VERBOSITY, aaxine.debug_messages);

  /*
   * xine init
   */
  xine_init (__xineui_global_xine_instance);

  if(!__xineui_global_xine_instance) {
    fprintf(stderr, "xine_init() failed.\n");
    goto failure;
  }

  /*
   * If X is running, init X thread support (avoid async errors)
   */
  if(getenv("DISPLAY"))
    xinit_thread();

#ifdef AA
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
  aa_resizehandler(aaxine.context, aaxine_resize_handler);
  /*
   * init aalib video output driver
   */
  if(!video_driver_id)
    video_driver_id = "aa";

  aaxine.vo_port = xine_open_video_driver(__xineui_global_xine_instance,
					  video_driver_id,
					  XINE_VISUAL_TYPE_AA,
					  (void *)aaxine.context);
#endif

#ifdef CACA
  /*
   * init caca video output driver
   */
  if(!video_driver_id)
    video_driver_id = "caca";

  aaxine.canvas = caca_create_canvas(0, 0);
  aaxine.display = caca_create_display(aaxine.canvas);
  aaxine.vo_port = xine_open_video_driver(__xineui_global_xine_instance,
					  video_driver_id,
					  XINE_VISUAL_TYPE_CACA,
					  aaxine.display);
#endif

  if (!aaxine.vo_port) {
    aaxine.vo_port = xine_open_video_driver(__xineui_global_xine_instance,
					    video_driver_id,
					    XINE_VISUAL_TYPE_FB,
					    NULL);
  }
  if (!aaxine.vo_port) {
    printf ("main: video driver %s failed\n", video_driver_id);
    goto failure;
  }

  /*
   * init audio output driver
   */
  driver_name = (char *)xine_config_register_string (__xineui_global_xine_instance,
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
    config_update_string (__xineui_global_xine_instance, "audio.driver", audio_driver_id);

  aaxine.ao_port = xine_open_audio_driver(__xineui_global_xine_instance, audio_driver_id, NULL);

  if(!aaxine.ao_port) {
    printf ("main: audio driver %s failed\n", audio_driver_id);
    aaxine.no_post = 1;
  }

  /* Init post plugin, if desired */
  if(!aaxine.no_post) {

    if(aaxine.ao_port) {
      const char *const *pol = xine_list_post_plugins_typed(__xineui_global_xine_instance,
							    XINE_POST_TYPE_AUDIO_VISUALIZATION);

      if(pol) {
	aaxine.post_plugin_name =
	  (char *) xine_config_register_string (__xineui_global_xine_instance, "aaxine.post_plugin",
						pol[0],
						"Post plugin name",
						NULL, 0, post_plugin_cb, NULL);

	if(post_plugin_name) {
	  if((aaxine.post_plugin = xine_post_init(__xineui_global_xine_instance, post_plugin_name,
						  0, &aaxine.ao_port, &aaxine.vo_port)) != NULL) {
	    config_update_string(__xineui_global_xine_instance, "aaxine.post_plugin", post_plugin_name);
	  }
	}
	else
	  aaxine.post_plugin = xine_post_init(__xineui_global_xine_instance, aaxine.post_plugin_name,
					      0, &aaxine.ao_port, &aaxine.vo_port);

	if(aaxine.post_plugin == NULL) {
	  printf("failed to initialize post plugins '%s'\n", aaxine.post_plugin_name);
	  aaxine.no_post = 1;
	}
	else {
	  /* reduce goom default values */
	  if(!strcmp(aaxine.post_plugin_name, "goom")) {
	    config_update_num(__xineui_global_xine_instance, "effects.goom.fps", 10);
	    config_update_num(__xineui_global_xine_instance, "effects.goom.height", 120);
	    config_update_num(__xineui_global_xine_instance, "effects.goom.width", 120);
	  }
	}
      }
    }
  }

  aaxine.stream = xine_stream_new(__xineui_global_xine_instance, aaxine.ao_port, aaxine.vo_port);

  /* Init mixer control */
  aaxine.mixer.enable = 0;
  aaxine.mixer.caps = MIXER_CAP_NOTHING;

  if(aaxine.ao_port) {
    if((xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME)) != -1)
      aaxine.mixer.caps |= MIXER_CAP_VOL;
    if((xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_MUTE)) != -1)
      aaxine.mixer.caps |= MIXER_CAP_MUTE;
  }

  if(aaxine.mixer.caps & MIXER_CAP_VOL) {
    aaxine.mixer.enable = 1;
    aaxine.mixer.volume_level = xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME);
  }

  if(aaxine.mixer.caps & MIXER_CAP_MUTE)
    aaxine.mixer.mute = xine_get_param(aaxine.stream, XINE_PARAM_AUDIO_MUTE);

  /* Select audio channel */
  xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, audio_channel);

#ifdef AA
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

      /* This was just a temporary so that we could redefine STDOUT */
      close(error_fd);
    }
  }
#endif

  aaxine.event_queue = xine_event_new_queue(aaxine.stream);
  xine_event_create_listener_thread(aaxine.event_queue, aaxine_event_listener, NULL);

  if(!aaxine_xine_open_and_play(aaxine.mrl[aaxine.current_mrl], 0, 0))
    goto failure;

  /*
   * ui loop
   */

  aaxine.running = 1;

  while (aaxine.running) {

    if ((key = aaxine_get_key_event()) == 0) continue;

    switch (key) {

    case KEY_UP:
      aaxine.ignore_next = 1;
      xine_stop(aaxine.stream);
      aaxine.current_mrl--;
      if((aaxine.current_mrl >= 0) && (aaxine.current_mrl < aaxine.num_mrls)) {
	if(!aaxine_xine_open_and_play(aaxine.mrl[aaxine.current_mrl], 0, 0))
	  goto failure;
      }
      else {
	aaxine.current_mrl = 0;
	aaxine_play_logo();
      }
      aaxine.ignore_next = 0;
      break;

    case KEY_DOWN:
      aaxine.ignore_next = 1;
      xine_stop(aaxine.stream);
      aaxine.ignore_next = 0;
      aaxine_start_next();
      break;

    case KEY_LEFT:
      if(xine_get_param(aaxine.stream, XINE_PARAM_SPEED) > XINE_SPEED_PAUSE)
        xine_set_param(aaxine.stream, XINE_PARAM_SPEED,
		       (xine_get_param(aaxine.stream, XINE_PARAM_SPEED)) / 2);
      break;

    case KEY_RIGHT:
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
      if(!aaxine_xine_open_and_play(aaxine.mrl[aaxine.current_mrl], 0, 0))
	goto failure;
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
      xine_stop(aaxine.stream);
      aaxine_play_logo();
      break;

    case 'V':
      if(aaxine.mixer.enable) {
	if(aaxine.mixer.caps & MIXER_CAP_VOL) {
	  if(aaxine.mixer.volume_level < 100) {
	    aaxine.mixer.volume_level++;
	    xine_set_param(aaxine.stream, XINE_PARAM_AUDIO_VOLUME, aaxine.mixer.volume_level);
	  }
	}
      }
      break;

    case 'v':
      if(aaxine.mixer.enable) {
	if(aaxine.mixer.caps & MIXER_CAP_VOL) {
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
	if(aaxine.mixer.caps & MIXER_CAP_MUTE) {
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

  if(__xineui_global_xine_instance && __xineui_global_config_file)
    xine_config_save(__xineui_global_xine_instance, __xineui_global_config_file);

  if(aaxine.stream) {
    xine_close(aaxine.stream);
    xine_event_dispose_queue(aaxine.event_queue);
    xine_dispose(aaxine.stream);
  }

  if(aaxine.vo_port)
    xine_close_video_driver(__xineui_global_xine_instance, aaxine.vo_port);

  if(aaxine.ao_port)
    xine_close_audio_driver(__xineui_global_xine_instance, aaxine.ao_port);

  if(__xineui_global_xine_instance)
    xine_exit(__xineui_global_xine_instance);

#ifdef AA
  if(aaxine.context) {
    aa_showcursor(aaxine.context);
    aa_uninitkbd(aaxine.context);
    aa_close(aaxine.context);
  }
#endif

#ifdef CACA
  if(aaxine.display) {
    caca_free_display(aaxine.display);
    caca_free_canvas(aaxine.canvas);
  }
#endif

  if(xlib_handle)
    dlclose(xlib_handle);

  free(__xineui_global_config_file);

  return 0;
}
