/** Copyright (C) 2000-2019 the xine project
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
 * xine main for X11
 *
 */
/* required for getsubopt(); the __sun test gives us strncasecmp() on solaris */
#if !defined(__sun) && ! defined(__FreeBSD__) && ! defined(__NetBSD__) && ! defined(__DragonFly__)
#define _XOPEN_SOURCE 500
#endif
/* required for strncasecmp() */
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
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
#include <X11/Xresource.h>

#include <locale.h>

#include <xine.h>
#include <xine/xineutils.h>

#include "common.h" 

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

#ifndef XINE_VERSION_CODE
#define XINE_VERSION_CODE  XINE_MAJOR_VERSION*10000+XINE_MINOR_VERSION*100+XINE_SUB_VERSION
#endif

/*
 * global variables
 */
gGui_t *gGui;

#define CONFIGFILE "config"
#define CONFIGDIR  ".xine"

typedef struct {
  FILE    *fd;
  char    *filename;
  char    *ln;
  char     buf[256];
} file_info_t;

#define	OPTION_VISUAL		1000
#define	OPTION_INSTALL_COLORMAP	1001
#define OPTION_DISPLAY_KEYMAP   1002
#define OPTION_SK_SERVER        1003
#define OPTION_ENQUEUE          1004
#define OPTION_VERBOSE          1005
#define OPTION_BROADCAST_PORT   1006
#define OPTION_NO_LOGO          1007
#define OPTION_POST             1008
#define OPTION_DISABLE_POST     1009
#define OPTION_NO_SPLASH        1010
#define OPTION_STDCTL           1011
#define OPTION_LIST_PLUGINS     1012
#define OPTION_BUG_REPORT       1013
#define OPTION_NETWORK_PORT     1014
#define OPTION_NO_MOUSE         1015


/* options args */
static const char short_options[] = "?hHgfvn"
#ifdef HAVE_XINERAMA
 "F"
#endif
#ifdef HAVE_LIRC
 "L"
#endif
#ifdef HAVE_XF86VIDMODE
 "F"
#endif
 "u:a:V:A:p::s:RW:G:BN:P:l::S:ZD::r:c:ET:I";

static const struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h'                      },
#ifdef HAVE_LIRC
  {"no-lirc"        , no_argument      , 0, 'L'                      },
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
#ifdef HAVE_XINERAMA
  {"xineramafull"   , no_argument      , 0, 'F'                      },
#endif
  {"visual"	    , required_argument, 0,  OPTION_VISUAL           },
  {"install"	    , no_argument      , 0,  OPTION_INSTALL_COLORMAP },
  {"keymap"         , optional_argument, 0,  OPTION_DISPLAY_KEYMAP   },
  {"network"        , no_argument      , 0, 'n'                      },
  {"network-port"   , required_argument, 0,  OPTION_NETWORK_PORT     },
  {"root"           , no_argument      , 0, 'R'                      },
  {"wid"            , required_argument, 0, 'W'                      },
  {"geometry"       , required_argument, 0, 'G'                      },
  {"borderless"     , no_argument      , 0, 'B'                      },
  {"animation"      , required_argument, 0, 'N'                      },
  {"playlist"       , required_argument, 0, 'P'                      },
  {"loop"           , optional_argument, 0, 'l'                      },
  {"skin-server-url", required_argument, 0, OPTION_SK_SERVER         },
  {"enqueue"        , no_argument      , 0, OPTION_ENQUEUE           },
  {"session"        , required_argument, 0, 'S'                      },
  {"version"        , no_argument      , 0, 'v'                      },
  {"deinterlace"    , optional_argument, 0, 'D'                      },
  {"aspect-ratio"   , required_argument, 0, 'r'                      },
  {"config"         , required_argument, 0, 'c'                      },
  {"verbose"        , optional_argument, 0, OPTION_VERBOSE           },
  {"stdctl"         , optional_argument, 0, OPTION_STDCTL            },
#ifdef XINE_PARAM_BROADCASTER_PORT
  {"broadcast-port" , required_argument, 0, OPTION_BROADCAST_PORT    },
#endif
  {"no-logo"        , no_argument      , 0, OPTION_NO_LOGO           },
  {"no-mouse"       , no_argument      , 0, OPTION_NO_MOUSE          },
  {"no-reload"      , no_argument      , 0, 'E'                      },
  {"post"           , required_argument, 0, OPTION_POST              },
  {"disable-post"   , no_argument      , 0, OPTION_DISABLE_POST      },
  {"no-splash"      , no_argument      , 0, OPTION_NO_SPLASH         },
  {"tvout"          , required_argument, 0, 'T'                      },
  {"list-plugins"   , optional_argument, 0, OPTION_LIST_PLUGINS      },
  {"bug-report"     , optional_argument, 0, OPTION_BUG_REPORT        },
  {"no-gui"         , no_argument      , 0, 'I'                      },
  {0                , no_argument      , 0, 0                        }
};

#undef TRACE_RC

/*
 *    RC file related functions
 */
#ifdef TRACE_RC
static void _rc_file_check_args(int argc, char **argv) {
  int i;
  
  printf("%s(): argc %d\n", __XINE_FUNCTION__, argc);

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
  int            i = 1, j;
  const char    *cfgdir = CONFIGDIR;
  const char    *xinerc = "xinerc";
  file_info_t   *rcfile;
  char        **_argv = NULL;
  
  _argv  = (char **) calloc((argc + 1), sizeof(char *));
  (*_argc) = argc;

  _argv[0] = strdup(argv[0]);
  
  rcfile           = (file_info_t *) calloc(1, sizeof(file_info_t));
  rcfile->filename = xitk_asprintf("%s/%s/%s", (xine_get_homedir()), cfgdir, xinerc);
  
  if((rcfile->fd = fopen(rcfile->filename, "r")) != NULL) {
    
    while(_rc_file_get_next_line(rcfile)) {
      _argv = (char **) realloc(_argv, sizeof(char **) * ((*_argc) + 2));
      _argv[i] = strdup(rcfile->ln);
      i++;
      (*_argc)++;
    }
    
    fclose(rcfile->fd);
  }
  
  for(j = 1; i < (*_argc); i++, j++)
    _argv[i] = strdup(argv[j]);

  _argv[(*_argc)] = NULL;
  
  free(rcfile->filename);
  free(rcfile);

  return _argv;
}

static void free_command_line_args(char ***argv, int argc) {
  int _argc = argc - 1;
  
  while(_argc >= 0) {
    free((*argv)[_argc]);
    _argc--;
  }
  
  free(*(argv));
}

static int parse_geometry(window_attributes_t *window_attribute, char *geomstr) {
  int xoff, yoff, ret;
  unsigned int width, height;

  if((ret = XParseGeometry(geomstr, &xoff, &yoff, &width, &height))) {
    if(ret & XValue)
      window_attribute->x      = xoff;
    if(ret & YValue)
      window_attribute->y      = yoff;
    if(ret & WidthValue)
      window_attribute->width  = width;
    if(ret & HeightValue)
      window_attribute->height = height;
    
    return 1;
  }
  return 0;
}
  
static int parse_visual(VisualID *vid, int *vclass, char *visual_str) {
  int ret = 0;
  unsigned int visual = 0;

  if(sscanf(visual_str, "%x", &visual) == 1) {
    *vid = visual;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "StaticGray") == 0) {
    *vclass = StaticGray;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "GrayScale") == 0) {
    *vclass = GrayScale;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "StaticColor") == 0) {
    *vclass = StaticColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "PseudoColor") == 0) {
    *vclass = PseudoColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "TrueColor") == 0) {
    *vclass = TrueColor;
    ret = 1;
  }
  else if (strcasecmp(visual_str, "DirectColor") == 0) {
    *vclass = DirectColor;
    ret = 1;
  }

  return ret;
}

static void xrm_parse(void) {
  gGui_t *gui = gGui;
  Display      *display;
  char          user_dbname[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  char          environement_buf[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  char          wide_dbname[XITK_PATH_MAX + XITK_NAME_MAX + 2];
  const char   *environment;
  const char   *classname = "xine";
  char         *str_type;
  XrmDatabase   rmdb, home_rmdb, server_rmdb, application_rmdb;
  XrmValue      value;
  
  XrmInitialize();

  rmdb = home_rmdb = server_rmdb = application_rmdb = NULL;

  snprintf(wide_dbname, sizeof(wide_dbname), "%s%s", "/usr/lib/X11/app-defaults/", classname);
  
  if((display = XOpenDisplay((getenv("DISPLAY")))) == NULL)
    return;
  
  application_rmdb = XrmGetFileDatabase(wide_dbname);
  (void) XrmMergeDatabases(application_rmdb, &rmdb);
  
  if(XResourceManagerString(display) != NULL)
    server_rmdb = XrmGetStringDatabase(XResourceManagerString(display));
  else {
    snprintf(user_dbname, sizeof(user_dbname), "%s%s", (xine_get_homedir()), "/.Xdefaults");
    server_rmdb = XrmGetFileDatabase(user_dbname);
  }
  
  (void) XrmMergeDatabases(server_rmdb, &rmdb);
  
  if((environment = getenv("XENVIRONMENT")) == NULL) {
    int len;
    
    environment = environement_buf;
    snprintf(environement_buf, sizeof(environement_buf), "%s%s", (xine_get_homedir()), "/.Xdefaults-");
    len = strlen(environement_buf);
    (void) gethostname(environement_buf + len, sizeof(environement_buf) - len);
  }
  
  home_rmdb = XrmGetFileDatabase(environment);
  (void) XrmMergeDatabases(home_rmdb, &rmdb);

  if(XrmGetResource(rmdb, "xine.geometry", "xine.Geometry", &str_type, &value) == True) {
    if (!parse_geometry (&gui->window_attribute, (char *)value.addr))
      printf(_("Bad geometry '%s'\n"), (char *)value.addr);
  } 
  if(XrmGetResource(rmdb, "xine.border", "xine.Border", &str_type, &value) == True) {
    gui->window_attribute.borderless = !get_bool_value((char *) value.addr);
  }
  if(XrmGetResource(rmdb, "xine.visual", "xine.Visual", &str_type, &value) == True) {
    if(!parse_visual(&gui->prefered_visual_id, 
		     &gui->prefered_visual_class, (char *)value.addr)) {
      printf(_("Bad visual '%s'\n"), (char *)value.addr);
    }
  }
  if(XrmGetResource(rmdb, "xine.colormap", "xine.Colormap", &str_type, &value) == True) {
    gui->install_colormap = !get_bool_value((char *) value.addr);
  }
  
  XrmDestroyDatabase(rmdb);
  
  if(display)
    XCloseDisplay(display);

}

static void main_change_logo_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->logo_mrl = cfg->str_value;
}
static void sub_autoload_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->subtitle_autoload = cfg->num_value;
}

/*
 *
 */
static void show_version(void) {
  
  printf(_("This is xine (X11 gui) - a free video player v%s"), VERSION);
#ifdef DEBUG
  printf("-[DEBUG]");
#endif
  printf(_(".\n(c) %s The xine Team.\n"), "2000-2019");
}

/*
 *
 */
static void show_banner(void) {
  int major, minor, sub;
  
  show_version();

  if(__xineui_global_verbosity)
    printf(_("Built with xine library %d.%d.%d (%s)\n"),
	    XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, XINE_VERSION);
  
  xine_get_version (&major, &minor, &sub);

  if(__xineui_global_verbosity)
    printf(_("Found xine library version: %d.%d.%d (%s).\n"), 
	    major, minor, sub, xine_get_version_string());
}

static void print_formatted(char *title, const char *const *plugins) {
  const char  *plugin;
  char         buffer[81];
  int          len;
  static const char blanks[] = "     ";

  printf("%s", title);
  
  strcpy(buffer, blanks);
  plugin = *plugins++;
  
  while(plugin) {
    
    len = strlen(buffer);
    
    if((len + (strlen(plugin) + 3)) < 80) {
      snprintf(buffer+len, sizeof(buffer)-len, "%s%s", (len == sizeof(blanks)-1) ? "" : ", ", plugin);
    }
    else {
      printf("%s,\n", buffer);
      snprintf(buffer, sizeof(buffer), "%s%s", blanks, plugin);
    }
    
    plugin = *plugins++;
  }
  
  printf("%s.\n\n", buffer);
}

static char *_config_file()
{
  const char *home       = xine_get_homedir();
  const char *cfgdir     = CONFIGDIR;
  const char *cfgfile    = CONFIGFILE;
  char       *configfile = NULL;

  if (!home)
    return NULL;

  configfile = (char *) malloc(strlen(home)
                               + strlen(cfgdir)
                               + strlen(cfgfile)
                               + 3);
  if (!configfile)
    return NULL;

  sprintf(configfile, "%s/%s", home, cfgdir);
  if (mkdir(configfile, 0755) < 0 && errno != EEXIST) {
    fprintf(stderr, "Error creating %s: %d (%s)\n", configfile, errno, strerror(errno));
  }
  sprintf(configfile + strlen(configfile), "/%s", cfgfile);

  return configfile;
}

static void _config_load(xine_t *xine)
{
  char *configfile = _config_file();
  if (configfile) {
    xine_config_load(xine, configfile);
    free(configfile);
  }
}

static void list_plugins(char *type) {
  const char   *const  *plugins;
  xine_t               *xine;
  int                   i;
  static const struct {
    const char *const *(*func)(xine_t *);
    char                name[24];
    char                type[16];
  } list_functions[] = {
    { xine_list_audio_output_plugins,  N_("   -Audio output:\n"),    "audio_out"     },
    { xine_list_video_output_plugins,  N_("   -Video output:\n"),    "video_out"     },
    { xine_list_demuxer_plugins,       N_("   -Demuxer:\n"),         "demux"         },
    { xine_list_input_plugins,         N_("   -Input:\n"),           "input"         },
    { xine_list_spu_plugins,           N_("   -Subpicture:\n"),      "sub"           },
    { xine_list_post_plugins,          N_("   -Post processing:\n"), "post"          },
    { xine_list_audio_decoder_plugins, N_("   -Audio decoder:\n"),   "audio_decoder" },
    { xine_list_video_decoder_plugins, N_("   -Video decoder:\n"),   "video_decoder" },
    { NULL,                            "",                           ""              }
  };
    

  xine = xine_new();
  _config_load(xine);
  xine_init(xine);

  show_version();

  if(type && strlen(type)) {
    i = 0;
    
    while(list_functions[i].func) {
      
      if(!strncasecmp(type, list_functions[i].type, strlen(type))) {
	if((plugins = list_functions[i].func(xine))) {
	  printf(_("\n Available xine's plugins:\n"));
	  print_formatted(gettext(list_functions[i].name), plugins);
	  goto __found;
	}
      }

      i++;
    }

    printf(_("No available plugins found of %s type!\n"), type);
  __found: ;
  }
  else {
    printf(_("\n Available xine's plugins:\n"));
    
    i = 0;
    
    while(list_functions[i].func) {

      if((plugins = list_functions[i].func(xine)))
	print_formatted(gettext(list_functions[i].name), plugins);
      
      i++;
    }
  }

  xine_exit(xine);
}

/*
 *
 */
static void show_usage (void) {
  const char   *const *driver_ids;
  const char   *driver_id;
  xine_t       *xine;
  const char  **backends, *backend;
  
  xine = xine_new();
  _config_load(xine);
  xine_init(xine);
  
  printf("\n");
  printf(_("Usage: xine [OPTIONS]... [MRL]\n"));
  printf("\n");
  printf(_("OPTIONS are:\n"));
  printf(_("  -v, --version                Display version.\n"));
  printf(_("      --verbose [=level]       Set verbosity level. Default is 1.\n"));
  printf(_("  -c, --config <file>          Use config file instead of default one.\n"));
  printf(_("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n"));
  printf("                               ");
  if((driver_ids = xine_list_video_output_plugins (xine))) {
    driver_id  = *driver_ids++;
    while (driver_id) {
      printf ("%s ", driver_id);
      driver_id  = *driver_ids++;
    }
  }
  printf ("\n");

  printf(_("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n"));
  printf("                               null ");
  if((driver_ids = xine_list_audio_output_plugins (xine))) {
    driver_id  = *driver_ids++;
    while (driver_id) {
      printf ("%s ", driver_id);
      driver_id  = *driver_ids++;
    }
  }
  printf ("\n");
  printf(_("  -u, --spu-channel <#>        Select SPU (subtitle) channel '#'.\n"));
  printf(_("  -a, --audio-channel <#>      Select audio channel '#'.\n"));
  printf(_("  -p, --auto-play [opt]        Play on start. Can be followed by:\n"
	   "                    'f': in fullscreen mode.\n"
	   "                    'h': hide GUI (panel, etc.).\n"
	   "                    'w': hide video window.\n"
	   "                    'q': quit when play is done.\n"
	   "                    'd': retrieve playlist from DVD. (deprecated. use -s DVD)\n"
	   "                    'v': retrieve playlist from VCD. (deprecated. use -s VCD)\n"));
#ifdef HAVE_XINERAMA
  printf(_("                    'F': in xinerama fullscreen mode.\n"));
#endif
  printf(_("  -s, --auto-scan <plugin>     auto-scan play list from <plugin>\n"));
  printf(_("  -f, --fullscreen             start in fullscreen mode,\n"));
#ifdef HAVE_XINERAMA
  printf(_("  -F, --xineramafull           start in xinerama fullscreen (display on several screens),\n"));
#endif
  printf(_("  -g, --hide-gui               hide GUI (panel, etc.)\n"));
  printf(_("  -I, --no-gui                 disable GUI\n"));
  printf(_("      --no-mouse               disable mouse event\n"));
  printf(_("  -H, --hide-video             hide video window\n"));
#ifdef HAVE_LIRC
  printf(_("  -L, --no-lirc                Turn off LIRC support.\n"));
#endif
  printf(_("      --visual <class-or-id>   Use the specified X11 visual. <class-or-id>\n"
	   "                               is either an X11 visual class, or a visual id.\n"));
  printf(_("      --install                Install a private colormap.\n"));
  printf(_("      --keymap [=option]       Display keymap. Option are:\n"
	   "                                 'default': display default keymap table,\n"
	   "                                 'lirc': display draft of a .lircrc config file.\n"
	   "                                 'remapped': user remapped keymap table.\n"
	   "                                 'file:<filename>': use <filename> as keymap.\n"
	   "                                 -if no option is given, 'default' is selected.\n"));
  printf(_("  -n, --network                Enable network remote control server.\n"));
  printf(_("      --network-port           Specify network server port number.\n"));
  printf(_("  -R, --root                   Use root window as video window.\n"));
  printf(_("  -W, --wid                    Use a window id.\n"));
  printf(_("  -G, --geometry <WxH[+X+Y]>   Set output window geometry (X style).\n"));
  printf(_("  -B, --borderless             Borderless video output window.\n"));
  printf(_("  -N, --animation <mrl>        Specify mrl to play when video output isn't used.\n"
	   "                                 -can be used more than one time.\n"));
  printf(_("  -P, --playlist <filename>    Load a playlist file.\n"));
  printf(_("  -l, --loop [=mode]           Set playlist loop mode (default: loop). Modes are:\n"
	   "                                 'loop': loop entire playlist.\n"
	   "                                 'repeat': repeat current playlist entry.\n"
	   "                                 'shuffle': select randomly a yet unplayed entry from playlist\n"
	   "                                 'shuffle+': same as 'shuffle', but indefinetely replay the playlist.\n"));
  printf(_("      --skin-server-url <url>  Define the skin server url.\n"));
  printf(_("      --enqueue <mrl> ...      Enqueue mrl of a running session (session 0)\n"));
  printf(_("  -S, --session <option1,option2,...>\n"
	   "                               Session managements. Options can be:\n"
	   "                    session=n   specify session <n> number,\n"
	   "                    mrl=m       add mrl <m> to the playlist,\n"
	   "                    audio=c     select audio channel (<c>: 'next' or 'prev'),\n"
	   "                    spu=c       select spu channel (<c>: 'next' or 'prev'),\n"
	   "                    volume=v    set audio volume,\n"
	   "                    amp=v       set amplification level,\n"
	   "                    loop=m      set loop mode (<m>: 'none' 'loop' 'repeat' 'shuffle' or 'shuffle+'),\n"
           "                    get_speed   get speed value (see man page for returned value).\n"
	   "                    (playlist|pl)=p\n"
	   "                                 <p> can be:\n"
	   "                                   'clear'  clear the playlist,\n"
	   "                                   'first'  play first entry in the playlist,\n"
	   "                                   'prev'   play previous playlist entry,\n"
	   "                                   'next'   play next playlist entry,\n"
	   "                                   'last'   play last entry in the playlist,\n"
	   "                                   'load:s' load playlist file <s>,\n"
	   "                                   'stop'   stop playback at the end of current playback.\n"
	   "                                   'cont'   continue playback at the end of current playback.\n"
	   "                    play, slow2, slow4, pause, fast2,\n"
	   "                    fast4, stop, quit, fullscreen, eject.\n"));
  printf(_("  -Z                           Don't automatically start playback (smart mode).\n"));
  printf(_("  -D, --deinterlace [post]...  Deinterlace video output. One or more post plugin\n"
	   "                                 can be specified, with optional parameters.\n"
	   "                                 Syntax is the same as --post option.\n"));
  printf(_("  -r, --aspect-ratio <mode>    Set aspect ratio of video output. Modes are:\n"
	   "                                 'auto', 'square', '4:3', 'anamorphic', 'dvb'.\n"));
#ifdef XINE_PARAM_BROADCASTER_PORT
  printf(_("      --broadcast-port <port>  Set port of xine broadcaster (master side)\n"
	   "                               Slave is started with 'xine slave://address:port'\n"));
#endif
  printf(_("      --no-logo                Don't display the logo.\n"));
  printf(_("  -E, --no-reload              Don't reload old playlist.\n"));
  printf(_("      --post <plugin>[:parameter=value][,...][;...]\n"
	   "                               Load one or many post plugin(s).\n"
	   "                               Parameters are comma separated.\n"
	   "                               This option can be used more than one time.\n"));
  printf(_("      --disable-post           Don't enable post plugin(s).\n"));
  printf(_("      --no-splash              Don't display the splash screen.\n"));
  printf(_("      --stdctl                 Control xine by the console, using lirc commands.\n"));
  printf(_("  -T, --tvout <backend>        Turn on tvout support. Backend can be:\n"));
  printf("                               ");
  if((backends = tvout_get_backend_names())) {
    backend = *backends++;
    while(backend) {
      printf("%s ", backend);
      backend = *backends++;
    }
  }
  printf("\n");
  printf(_("      --list-plugins [=type]   Display the list of all available plugins,\n"
	   "                               Optional <type> can be:\n"
	   "                    audio_out, video_out, demux, input, sub, post,\n"
	   "                    audio_decoder, video_decoder.\n"));
  printf(_("      --bug-report [=mrl]      Enable bug report mode:\n"
	   "                                 Turn on verbosity, gather all output\n"
	   "                                 messages and write them into a file named\n"
	   "                                 BUG-REPORT.TXT.\n"
	   "                                 If <mrl> is given, xine will play the mrl\n"
	   "                                 then quit (like -pq).\n"));
  printf("\n\n");
  printf(_("examples for valid MRLs (media resource locator):\n"
	   "  File:  'path/foo.vob'\n"
	   "         '/path/foo.vob'\n"
	   "         'file://path/foo.vob'\n"
	   "         'fifo://[[mpeg1|mpeg2]:/]path/foo'\n"
	   "         'stdin://[mpeg1|mpeg2]' or '-' (mpeg2 only)\n"
	   "  DVD:   'dvd://VTS_01_2.VOB'\n"
	   "  VCD:   'vcd://<track number>'\n"));
  printf("\n");

  xine_exit(xine);
}


/*
 * Try to load video output plugin, by stored name or probing
 */
static xine_video_port_t *load_video_out_driver(int driver_number) {
  gGui_t *gui = gGui;
  xine_video_port_t      *video_port = NULL;
  void                   *vis;
  int                     driver_num;

  vis = video_window_get_xine_visual(gui->vwin);

  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  driver_num = 
    xine_config_register_enum (gui->xine, "video.driver", 0, gui->video_driver_ids,
			      _("video driver to use"),
			      _("Choose video driver. "
				"NOTE: you may restart xine to use the new driver"),
			      CONFIG_LEVEL_ADV,
			      CONFIG_NO_CB, 
			      CONFIG_NO_DATA);
  
  if (driver_number < 0) {
    /* video output driver auto-probing */
    const char *const *driver_ids;
    int                i;
    
    if ((!strcasecmp (gui->video_driver_ids[driver_num], "none")) ||
        (!strcasecmp (gui->video_driver_ids[driver_num], "null"))) {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_num],
        XINE_VISUAL_TYPE_NONE, NULL);
      if (video_port)
	return video_port;
      
    }
    else if (!strcasecmp (gui->video_driver_ids[driver_num], "fb")) {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_num],
        XINE_VISUAL_TYPE_FB, NULL);
      if (video_port)
	return video_port;
      
    }
    else if (strcasecmp (gui->video_driver_ids[driver_num], "auto")) {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_num],
        XINE_VISUAL_TYPE_X11, vis);
      if (video_port)
	return video_port;
    }
    
    /* note: xine-lib can do auto-probing for us if we want.
     *       but doing it here should do no harm.
     */
    i = 0;
    driver_ids = xine_list_video_output_plugins (gui->xine);

    while (driver_ids[i]) {
      
      if(__xineui_global_verbosity)
	printf (_("main: probing <%s> video output plugin\n"), driver_ids[i]);
      
      video_port = xine_open_video_driver (gui->xine, 
					  driver_ids[i],
					  XINE_VISUAL_TYPE_X11, 
                                           vis);
      if (video_port) {
	return video_port;
      }
     
      i++;
    }
      
    if (!video_port) {
      printf (_("main: all available video drivers failed.\n"));
      exit (1);
    }
    
  }
  else {
    
    /* 'none' plugin is a special case, just change the visual type */
    if ((!strcasecmp (gui->video_driver_ids[driver_number], "none"))
      || (!strcasecmp (gui->video_driver_ids[driver_number], "null"))) {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_number],
        XINE_VISUAL_TYPE_NONE, NULL);
      
      /* do not save on config, otherwise user would never see images again... */
    }
    else if (!strcasecmp (gui->video_driver_ids[driver_number], "fb")) {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_number],
        XINE_VISUAL_TYPE_FB, NULL);

    }
    else {
      video_port = xine_open_video_driver (gui->xine, gui->video_driver_ids[driver_number],
        XINE_VISUAL_TYPE_X11, vis);
    }
    
    if(!video_port) {
      printf (_("main: video driver <%s> failed\n"), gui->video_driver_ids[driver_number]);
      exit (1);
    }
    
  }

  return video_port;
}

/*
 * Try to load audio output plugin, by stored name or probing
 */
static xine_audio_port_t *load_audio_out_driver(int driver_number) {
  gGui_t                 *gui = gGui;
  xine_audio_port_t      *audio_port = NULL;
  int                     driver_num;
  
  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  driver_num = 
    xine_config_register_enum (gui->xine, "video.driver", 0, gui->video_driver_ids,
			      _("video driver to use"),
			      _("Choose video driver. "
				"NOTE: you may restart xine to use the new driver"),
			      CONFIG_LEVEL_ADV,
			      CONFIG_NO_CB, 
			      CONFIG_NO_DATA);
  

  driver_num = 
    xine_config_register_enum (gui->xine, "audio.driver", 0, gui->audio_driver_ids,
			      _("audio driver to use"),
			      _("Choose audio driver. "
				"NOTE: you may restart xine to use the new driver"),
			      CONFIG_LEVEL_ADV,
			      CONFIG_NO_CB, 
			      CONFIG_NO_DATA);
  
  if (driver_number < 0) {
    const char *const *driver_ids;
    int    i;
    
    if (strcasecmp (gui->audio_driver_ids[driver_num], "auto")) {
      
      /* don't want to load an audio driver ? */
      if (!strncasecmp (gui->audio_driver_ids[driver_num], "NULL", 4)) {
	if(__xineui_global_verbosity)
	  printf(_("main: not using any audio driver (as requested).\n"));
        return NULL;
      }
      
      audio_port = xine_open_audio_driver (gui->xine, gui->audio_driver_ids[driver_num], NULL);
      if (audio_port)
	return audio_port;
    }
    
    /* note: xine-lib can do auto-probing for us if we want.
     *       but doing it here should do no harm.
     */
    i = 0;
    driver_ids = xine_list_audio_output_plugins (gui->xine);

    while (driver_ids[i]) {
      
      if(strcmp(driver_ids[i], "none")) {
	if(__xineui_global_verbosity)
	  printf(_("main: probing <%s> audio output plugin\n"), driver_ids[i]);
      }
      else {
	if(__xineui_global_verbosity)
	  printf(_("main: skipping <%s> audio output plugin from auto-probe\n"), driver_ids[i]);
	i++;
        continue;
      }
      
      audio_port = xine_open_audio_driver (gui->xine, 
					  driver_ids[i],
					  NULL);
      if (audio_port) {
	return audio_port;
      }
     
      i++;
    }
      
    printf(_("main: audio driver probing failed => no audio output\n"));
  }
  else {
    
    /* don't want to load an audio driver ? */
    if (!strncasecmp (gui->audio_driver_ids[driver_number], "NULL", 4)) {

      if(__xineui_global_verbosity)
	printf(_("main: not using any audio driver (as requested).\n"));

      /* calling -A null is useful to developers, but we should not save it at
       * config. if user doesn't have a sound card he may go to setup screen
       * changing audio.driver to NULL in order to make xine start a bit faster.
       */
    
    }
    else {
    
      audio_port = xine_open_audio_driver (gui->xine, gui->audio_driver_ids[driver_number], NULL);

      if (!audio_port) {
        printf (_("main: audio driver <%s> failed\n"), gui->audio_driver_ids[driver_number]);
        exit(1);
      }
    
    }
  
  }

  return audio_port;
}

/*
 *
 */
static void event_listener (void *user_data, const xine_event_t *event) {
  gGui_t *gui = user_data;
  struct timeval tv;
#if XINE_VERSION_CODE < 10200
  static int mrl_ext = 0; /* set if we get an MRL_REFERENCE_EXT */ 
#endif

  /*
   * Ignoring finished event logo is displayed (or played), that save us
   * from a loop of death
   */
  if(gui->logo_mode && (event->type == XINE_EVENT_UI_PLAYBACK_FINISHED))
    return;
  
  gettimeofday (&tv, NULL);
  
  if(labs(tv.tv_sec - event->tv.tv_sec) > 3) {
    fprintf(stderr, "Event too old, discarding\n");
    return;
  }
  
  if(gui->stdctl_enable)
    stdctl_event(event);

  switch(event->type) { 
    
  /* frontend can e.g. move on to next playlist entry */
  case XINE_EVENT_UI_PLAYBACK_FINISHED:
    if(event->stream == gui->stream) {
#ifdef XINE_PARAM_GAPLESS_SWITCH
      if( xine_check_version(1,1,1) )
        xine_set_param(gui->stream, XINE_PARAM_GAPLESS_SWITCH, 1);
#endif
      gui_playlist_start_next (gui);
    }
    else if(event->stream == gui->visual_anim.stream) {
      /* printf("xitk/main.c: restarting visual stream...\n"); */
      visual_anim_play_next();
    }
    break;
    
    /* inform ui that new channel info is available */
  case XINE_EVENT_UI_CHANNELS_CHANGED:
    if(event->stream == gui->stream)
      panel_update_channel_display (gui->panel);
    break;
    
    /* request title display change in ui */
  case XINE_EVENT_UI_SET_TITLE:
    if(event->stream == gui->stream) {
      xine_ui_data_t *uevent = (xine_ui_data_t *) event->data;
      
      pthread_mutex_lock (&gui->mmk_mutex);
      if(strcmp(gui->mmk.ident, uevent->str)) {
	
	free(gui->mmk.ident);
	if(gui->playlist.num && gui->playlist.cur >= 0 && gui->playlist.mmk &&
	   gui->playlist.mmk[gui->playlist.cur]) {
	  
	  free(gui->playlist.mmk[gui->playlist.cur]->ident);
	  
	  gui->playlist.mmk[gui->playlist.cur]->ident = strdup(uevent->str);
	}
	gui->mmk.ident = strdup(uevent->str);
        pthread_mutex_unlock (&gui->mmk_mutex);
	
        video_window_set_mrl (gui->vwin, uevent->str);
	playlist_mrlident_toggle();
        panel_update_mrl_display (gui->panel);
      } else {
        pthread_mutex_unlock (&gui->mmk_mutex);
      }
    }
    break;
    
    /* message (dialog) for the ui to display */
  case XINE_EVENT_UI_MESSAGE: 
    if(event->stream == gui->stream) {
      xine_ui_message_data_t *data = (xine_ui_message_data_t *) event->data;
      char                    buffer[8192];
      void                    (*report)(gGui_t *gui, const char *message, ...)
#if __GNUC__ >= 3
				__attribute__ ((format (printf, 2, 3)))
#endif
      ;
      
      report = xine_error_with_more;

      memset(&buffer, 0, sizeof(buffer));
      
      switch(data->type) {

	/* (messages to UI) */
      case XINE_MSG_NO_ERROR:
	{ /* copy strings, and replace '\0' separators by '\n' */
	  char *s, *d;
	    
	  report = xine_info;

	  s = data->messages;
	  d = buffer;
	  while(s && (*s != '\0') && ((*s + 1) != '\0')) {

	    switch(*s) {

	    case '\0':
	      *d = '\n';
	      break;
	      
	    default:
	      *d = *s;
	      break;
	    }
	    
	    s++;
	    d++;
	  }

	  *++d = '\0';
	}
	break;

	/* (warning message) */
      case XINE_MSG_GENERAL_WARNING:
	report = xine_info;
	snprintf(buffer, sizeof(buffer),
		 "%s:", _("*drum roll*, xine lib wants to take your attention "
			  "to deliver an important message to you ;-):"));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " %s %s", (char *) data + data->explanation, (char *) data + data->parameters);
	else
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " %s", _("No information available."));
	  
	break;

        /* (host name) */
      case XINE_MSG_UNKNOWN_HOST:
	strlcpy(buffer, _("The host you're trying to connect is unknown.\n"
			  "Check the validity of the specified hostname."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;
	
	/* (device name) */
      case XINE_MSG_UNKNOWN_DEVICE:
	strlcpy(buffer, _("The device name you specified seems invalid."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;

	/* none */
      case XINE_MSG_NETWORK_UNREACHABLE:
	strlcpy(buffer, _("The network looks unreachable.\nCheck your network "
			  "setup and/or the server name."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;

	/* (host name) */
      case XINE_MSG_CONNECTION_REFUSED:
	strlcpy(buffer, _("The connection was refused.\nCheck the host name."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;

	/* (file name or mrl) */
      case XINE_MSG_FILE_NOT_FOUND:
	strlcpy(buffer, _("The specified file or MRL could not be found. Please check it twice."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;

	/* (device/file/mrl) */
      case XINE_MSG_READ_ERROR:
	strlcpy(buffer, _("The source can't be read.\nMaybe you don't have enough "
			  "rights for this, or source doesn't contain data "
			  "(e.g: not disc in drive)."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;
	
	/* (library/decoder) */
      case XINE_MSG_LIBRARY_LOAD_ERROR:
	strlcpy(buffer, _("A problem occurred while loading a library or a decoder"), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), ": %s", (char *) data + data->parameters);
	break;

	/* none */
      case XINE_MSG_ENCRYPTED_SOURCE:
	strlcpy(buffer, _("The source seems encrypted, and can't be read."), sizeof(buffer));
        {
          int i;
          pthread_mutex_lock (&gui->mmk_mutex);
          i = strncasecmp (gui->mmk.mrl, "dvd:/", 5);
          pthread_mutex_unlock (&gui->mmk_mutex);
          if (!i) {
            strlcat (buffer,
              _("\nYour DVD is probably crypted. According to your country laws, you can or can't "
                "install/use libdvdcss to be able to read this disc, which you bought."), sizeof(buffer));
          }
        }
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s)", (char *) data + data->parameters);
	break;

	/* (warning message) */
      case XINE_MSG_SECURITY:
	report = xine_info;
	if(data->explanation)
	  snprintf(buffer, sizeof(buffer), "%s %s", (char *) data + data->explanation, (char *) data + data->parameters);
	else
	  strlcpy(buffer, _("No information available."), sizeof(buffer));
	break;

      case XINE_MSG_AUDIO_OUT_UNAVAILABLE:
        gui_stop (NULL, gui);
	strlcpy(buffer, _("The audio device is unavailable. "
			  "Please verify if another program already uses it."), sizeof(buffer));
	break;

	/* (file) */
      case 13 /* UNCOMMENTME: XINE_MSG_FILE_EMPTY */:
	snprintf(buffer, sizeof(buffer), "%s %s",
		 data->explanation ? (char *) data + data->explanation : "File is empty",
		 (char *) data + data->parameters);
	break;

#ifdef XINE_MSG_AUTHENTICATION_NEEDED
	/* mrl */
	/* FIXME: implement auth dialogue box */
      case XINE_MSG_AUTHENTICATION_NEEDED:
	strlcpy(buffer, _("Sorry, not implemented (authentication)"), sizeof(buffer));
	if (data->num_parameters)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), ": %s", (char *) data + data->parameters);
	break;
#endif

      default:
	strlcpy(buffer, _("*sight*, unknown error."), sizeof(buffer));
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), " (%s %s)", (char *) data + data->explanation, (char *) data + data->parameters);
	break;
      }
      
      if(__xineui_global_verbosity >= XINE_VERBOSITY_DEBUG) {
	strlcat(buffer, "\n\n[", sizeof(buffer));
	
	if(data->explanation)
	  snprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), "'%s' '%s'", (char *) data + data->explanation, (char *) data + data->parameters);
	
	strlcat(buffer, "]", sizeof(buffer));
      }
      
      if (buffer[0]) {
        struct timeval tv;
        pthread_mutex_lock (&gui->no_messages.mutex);
        tv = gui->no_messages.until;
        pthread_mutex_unlock (&gui->no_messages.mutex);
        if ((event->tv.tv_sec < tv.tv_sec)
          || ((event->tv.tv_sec == tv.tv_sec) && (event->tv.tv_usec < tv.tv_usec))) {
          if (__xineui_global_verbosity >= XINE_VERBOSITY_DEBUG)
            printf ("xine-ui: suppressed message:\n%s\n", buffer);
        } else {
          report (gui, "%s", buffer);
        }
      }
      
    }
    break;
    
    /* e.g. aspect ratio change during dvd playback */
  case XINE_EVENT_FRAME_FORMAT_CHANGE:
    break;

    /* report current audio vol level (l/r/mute) */
  case XINE_EVENT_AUDIO_LEVEL:
    if(event->stream == gui->stream) {
      xine_audio_level_data_t *aevent = (xine_audio_level_data_t *) event->data;

      gui->mixer.volume_level = (aevent->left + aevent->right) / 2;
      if(gui->mixer.method == SOUND_CARD_MIXER) {
	gui->mixer.mute = aevent->mute;
        panel_update_mixer_display (gui->panel);
      }
    }
    break;

#ifdef XINE_EVENT_AUDIO_AMP_LEVEL	/* Precaution for backward compatibility, will be removed some time */
    /* report current audio amp level (l/r/mute) */
  case XINE_EVENT_AUDIO_AMP_LEVEL:
    if(event->stream == gui->stream) {
      xine_audio_level_data_t *aevent = (xine_audio_level_data_t *) event->data;

      gui->mixer.amp_level = (aevent->left + aevent->right) / 2;
      if(gui->mixer.method == SOFTWARE_MIXER) {
	gui->mixer.mute = aevent->mute;
        panel_update_mixer_display (gui->panel);
      }
    }
    break;
#endif

    /* last event sent when stream is disposed */
  case XINE_EVENT_QUIT:
    break;
    
    /* index creation/network connections */
  case XINE_EVENT_PROGRESS:
    if(event->stream == gui->stream) {
      xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;
      char                  buffer[1024];
      
      snprintf(buffer, sizeof(buffer), "%s [%d%%]\n", pevent->description, pevent->percent);
      gui->mrl_overrided = 3;
      panel_set_title (gui->panel, buffer);
      osd_display_info("%s", buffer);
    }
    break;

#if XINE_VERSION_CODE < 10200
  case XINE_EVENT_MRL_REFERENCE:
    if(!mrl_ext && (event->stream == gui->stream) && gui->playlist.num) {
      xine_mrl_reference_data_t *ref = (xine_mrl_reference_data_t *) event->data;

      if(__xineui_global_verbosity)
	printf("XINE_EVENT_MRL_REFERENCE got mrl [%s] (alternative=%d)\n",
               ref->mrl, ref->alternative);

      if(ref->alternative == 0) {
        gui->playlist.ref_append++;
        mediamark_insert_entry(gui->playlist.ref_append, ref->mrl, ref->mrl, NULL, 0, -1, 0, 0);
      } else {
        pthread_mutex_lock (&gui->mmk_mutex);
	mediamark_t *mmk = mediamark_get_mmk_by_index(gui->playlist.ref_append);
	
	if(mmk) {
	  mediamark_append_alternate_mrl(mmk, ref->mrl);
	  mediamark_set_got_alternate(mmk);
	}
        pthread_mutex_unlock (&gui->mmk_mutex);

      }

    }
    break;
#endif

#ifndef XINE_EVENT_MRL_REFERENCE_EXT
/* present in 1.1.0 but not 1.0.2 */
#define XINE_EVENT_MRL_REFERENCE_EXT 13
typedef struct {
  int alternative, start_time, duration;
  const char mrl[1];
} xine_mrl_reference_data_ext_t;
#endif
  case XINE_EVENT_MRL_REFERENCE_EXT:
    if((event->stream == gui->stream) && gui->playlist.num) {
      xine_mrl_reference_data_ext_t *ref = (xine_mrl_reference_data_ext_t *) event->data;
      const char *title = ref->mrl + strlen (ref->mrl) + 1;
#if XINE_VERSION_CODE < 10200
      mrl_ext = 1; /* use this to ignore MRL_REFERENCE events */
#endif
      if(__xineui_global_verbosity)
	printf("XINE_EVENT_MRL_REFERENCE_EXT got mrl [%s] (alternative=%d)\n",
               ref->mrl, ref->alternative);

      if(ref->alternative == 0) {
        gui->playlist.ref_append++;
        /* FIXME: duration handled correctly? */
        mediamark_insert_entry(gui->playlist.ref_append, ref->mrl, *title ? title : ref->mrl, NULL, ref->start_time, ref->duration ? ref->start_time + ref->duration : -1, 0, 0);
      } else {
        /* FIXME: title? start? duration? */
        pthread_mutex_lock (&gui->mmk_mutex);
	mediamark_t *mmk = mediamark_get_mmk_by_index(gui->playlist.ref_append);

	if(mmk) {
	  mediamark_append_alternate_mrl(mmk, ref->mrl);
	  mediamark_set_got_alternate(mmk);
	}
        pthread_mutex_unlock (&gui->mmk_mutex);
      }
    }
    break;

  case XINE_EVENT_UI_NUM_BUTTONS:
    break;

  case XINE_EVENT_SPU_BUTTON:
    {
      xine_spu_button_t *spubtn = (xine_spu_button_t *) event->data;
      
      if(spubtn->direction)
        video_window_set_cursor (gui->vwin, CURSOR_HAND);
      else
        video_window_set_cursor (gui->vwin, CURSOR_ARROW);
    }
    break;

  case XINE_EVENT_DROPPED_FRAMES:
    if (xine_get_param(gui->stream, XINE_PARAM_SPEED) <= XINE_SPEED_NORMAL) {
      too_slow_window (gui);
    }
    break;

  }
}
  
/*
 *
 */
int main(int argc, char *argv[]) {
  gGui_t *gui;
  /* command line options will end up in these variables: */
  int                     c = '?', aos    = 0;
  int                     option_index    = 0;
  int                     audio_channel   = -1;
  int                     window_id       = 0;
  int                     spu_channel     = -1;
  char                   *audio_driver_id = NULL;
  char                   *video_driver_id = NULL;
  sigset_t                vo_mask;
  char                  **_argv;
  int                     _argc;
  int                     driver_num;
  int                     session         = -1;
  int                     aspect_ratio    = XINE_VO_ASPECT_AUTO ;
  int                     no_auto_start   = 0;
  int                     old_playlist_cfg, no_old_playlist = 0;
  char                  **pplugins        = NULL;
  int                     pplugins_num    = 0;
  char                   *tvout            = NULL;
  char                   *pdeinterlace     = NULL;
  int                     enable_deinterlace = 0;
  char                  **session_argv     = NULL;
  int                     session_argv_num = 0;
  int                     retval           = 0;
  pthread_mutexattr_t     mutexattr;
  
  /* Set stdout always line buffered to get every     */
  /* message line immediately, needed esp. for stdctl */
  setlinebuf(stdout);

#ifdef ENABLE_NLS
  if((xitk_set_locale()) != NULL) {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
  }
#endif

  bindtextdomain(PACKAGE, XITK_LOCALE);
  textdomain(PACKAGE);
  
  /* Check xine library version */
  if(!xine_check_version(1, 0, 0)) {
    int major, minor, sub;
    
    xine_get_version (&major, &minor, &sub);
    fprintf(stderr, _("Require xine library version 1.0.0, found %d.%d.%d.\n"),
	    major, minor,sub);
    exit(1);
  }

  sigemptyset(&vo_mask);
  sigaddset(&vo_mask, SIGALRM);
  if (sigprocmask (SIG_BLOCK,  &vo_mask, NULL))
    fprintf (stderr, "sigprocmask() failed.\n");

  gGui = (gGui_t *) calloc(1, sizeof(gGui_t));
  gui = gGui;
  
  gui->stream                 = NULL;
  gui->debug_level            = 0;

  gui->autoscan_plugin        = NULL;
  gui->prefered_visual_class  = -1;
  gui->prefered_visual_id     = None;
  gui->install_colormap       = 0;
  gui->cursor_grabbed         = 0;
  gui->network                = 0;
  gui->network_port           = 0;
  gui->video_window           = None;
  gui->use_root_window        = 0;
#ifdef HAVE_XF86VIDMODE
  gui->XF86VidMode_fullscreen = 0;
#endif
  gui->actions_on_start[aos]  = ACTID_NOKEY;
  gui->playlist.mmk           = NULL;
  gui->playlist.loop          = PLAYLIST_LOOP_NO_LOOP;
  gui->playlist.control       = 0;
  gui->skin_server_url        = NULL;
  __xineui_global_verbosity              = 0;
  gui->broadcast_port         = 0;
  gui->display_logo           = 1;
  gui->post_video_elements    = NULL;
  gui->post_video_elements_num = 0;
  gui->post_video_enable      = 1;
  gui->post_audio_elements    = NULL;
  gui->post_audio_elements_num = 0;
  gui->post_audio_enable      = 1;
  gui->splash                 = 1;
#ifdef HAVE_LIRC
  __xineui_global_lirc_enable            = 1;
#endif
  gui->deinterlace_enable     = 0;
  gui->report                 = stdout;
  gui->ssaver_enabled         = 1;
  gui->no_gui                 = 0;
  gui->no_mouse               = 0;
  gui->wid                    = 0;
  gui->nongui_error_msg       = NULL;
  gui->orig_stdout            = stdout;
  
  gui->panel                  = NULL;
  gui->vwin                   = NULL;
  gui->setup                  = NULL;
  gui->mrlb                   = NULL;
  gui->vctrl                  = NULL;

  gui->load_stream            = NULL;
  gui->load_sub               = NULL;

  gui->last_playback_speed    = XINE_FINE_SPEED_NORMAL;

  gui->window_attribute.x     =
  gui->window_attribute.y     = -8192;
  gui->window_attribute.width =
  gui->window_attribute.height = -1;
  gui->window_attribute.borderless = 0;

  _argv = build_command_line_args(argc, argv, &_argc);
#ifdef TRACE_RC
  _rc_file_check_args(_argc, _argv);
#endif

  {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init (&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&gui->mmk_mutex, &attr);
    pthread_mutexattr_destroy (&attr);
  }

  visual_anim_init();

  /*
   * Parse X Resource database
   */
  xrm_parse();

  /*
   * parse command line
   */
  opterr = 0;
  while((c = getopt_long(_argc, _argv, short_options,
			 long_options, &option_index)) != EOF) {
    switch(c) {

#ifdef HAVE_LIRC
    case 'L': /* Disable LIRC support */
      __xineui_global_lirc_enable = 0;
      break;
#endif

    case 'u': /* Select SPU channel */
      if (optarg) sscanf(optarg, "%i", &spu_channel);
      break;

    case 'a': /* Select audio channel */
      if (optarg) sscanf(optarg, "%i", &audio_channel);
      break;

    case 'V': /* select video driver by plugin id */
      if(optarg != NULL) {
        video_driver_id = strdup(optarg);
      } else {
	fprintf (stderr, _("video driver id required for -V option\n"));
	exit (1);
      }
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
        audio_driver_id = strdup(optarg);
      } else {
	fprintf (stderr, _("audio driver id required for -A option\n"));
	exit (1);
      }
      break;

    case 'p':/* Play [[in fullscreen][then quit]] on start */
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_PLAY;
      if(optarg != NULL) {
	if(strrchr(optarg, 'f')) {
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
	}
#ifdef HAVE_XINERAMA
	if(strrchr(optarg, 'F')) {
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_TOGGLE_XINERAMA_FULLSCREEN;
	}
#endif
	if(strrchr(optarg, 'h')) {
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_TOGGLE_VISIBLITY;
	}
	if(strrchr(optarg, 'q')) {
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_QUIT;
	}
	if(strrchr(optarg, 'w')) {
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
	}
	if(strrchr(optarg, 'd')) {
	  gui->autoscan_plugin = "DVD";
	}
	if(strrchr(optarg, 'v')) {
	  gui->autoscan_plugin = "VCD";
	}
      }
      break;

    case 'g': /* hide panel on start */
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_TOGGLE_VISIBLITY;
      break;

    case 'H': /* hide video on start */
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
      break;

    case 'f': /* full screen mode on start */
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
      break;

#ifdef HAVE_XINERAMA
    case 'F': /* xinerama full screen mode on start */
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_TOGGLE_XINERAMA_FULLSCREEN;
      break;
#endif

    case 'D':
      if(!pdeinterlace) {
	enable_deinterlace++;
	if(optarg) {
	  char *p = xine_chomp(optarg);
	  if(p && strlen(p))
	    pdeinterlace = strdup(p);
	}
      }
      break;
      
    case 'r':
      if(optarg != NULL) {
	char *p = xine_chomp(optarg);
	
	if(!strcasecmp(p, "auto"))
	  aspect_ratio = XINE_VO_ASPECT_AUTO;
	else if(!strcasecmp(p, "square"))
	  aspect_ratio = XINE_VO_ASPECT_SQUARE;
	else if(!strcasecmp(p, "4:3"))
	  aspect_ratio = XINE_VO_ASPECT_4_3;
	else if(!strcasecmp(p, "anamorphic"))
	  aspect_ratio = XINE_VO_ASPECT_ANAMORPHIC;
	else if(!strcasecmp(p, "dvb"))
	  aspect_ratio = XINE_VO_ASPECT_DVB;
	else {
	  fprintf(stderr, _("Bad aspect ratio mode '%s', see xine --help\n"), optarg);
	  exit(1);
	}
      }
      break;

    case 's': /* autoscan on start */
      gui->autoscan_plugin = xine_chomp(optarg);
      break;
       
    case OPTION_VISUAL:
      if(optarg != NULL) {
	if(!parse_visual(&gui->prefered_visual_id, &gui->prefered_visual_class, optarg)) {
	  show_usage();
	  exit(1);
	}
      }
      break;

    case OPTION_INSTALL_COLORMAP:
      gui->install_colormap = 1;
      break;

    case OPTION_DISPLAY_KEYMAP:
      if(optarg != NULL) {
	char *p = xine_chomp(optarg);

	if(!strcasecmp(p, "default"))
	  kbindings_display_default_bindings();
	else if(!strcasecmp(p, "lirc"))
	  kbindings_display_default_lirc_bindings();
	else if(!strcasecmp(p, "remapped"))
	  kbindings_display_current_bindings((kbindings_init_kbinding()));
	else if(!strncasecmp(p, "file:", 5)) {
	  char  *keymap_file = p + 5;
	  
	  if((gui->keymap_file == NULL) && keymap_file && strlen(keymap_file)) {
	    gui->keymap_file = xitk_filter_filename (keymap_file);
	    continue;
	  }
	}
	else
	  kbindings_display_default_bindings();
      }
      else
	kbindings_display_default_bindings();
      exit(1);
      break;

    case 'n': /* Enable remote control server */
      gui->network = 1;
      break;
      
    case OPTION_NETWORK_PORT:
      if(optarg) {
	int port = strtol(optarg, &optarg, 10);

	if((port >= 1) && (port <= 65535))
	  gui->network_port = port;
	else
	  fprintf(stderr, _("Bad port number: %d\n"), port);

      }
      break;

    case 'R': /* Use root window for video output */
      gui->use_root_window = 1;
      break;

    case 'G': /* Set geometry */
      if(optarg != NULL) {
        if (!parse_geometry (&gui->window_attribute, optarg)) {
	  fprintf(stderr, _("Bad geometry '%s', see xine --help\n"), optarg);
	  exit(1);
	}
      }
      break;

    case 'B':
      gui->window_attribute.borderless = 1;
      break;

    case 'N':
      if (optarg) {
        visual_anim_add_animation(optarg);
      }
      break;

    case 'P':
      if (optarg) {
      if((!actions_on_start(gui->actions_on_start, ACTID_PLAYLIST) && (aos < MAX_ACTIONS_ON_START)))
	gui->actions_on_start[aos++] = ACTID_PLAYLIST;

      pthread_mutex_lock (&gui->mmk_mutex);
      if(!gui->playlist.mmk)
	mediamark_load_mediamarks(optarg);
      else
	mediamark_concat_mediamarks(optarg);
      pthread_mutex_unlock (&gui->mmk_mutex);

      /* don't load original playlist when loading this one */
      no_old_playlist = 1;
      }
      break;

    case 'l':
      if(optarg != NULL) {
	char *p = xine_chomp(optarg);

	if(!strcasecmp(p, "loop"))
	  gui->playlist.loop = PLAYLIST_LOOP_LOOP;
	else if(!strcasecmp(p, "repeat"))
	  gui->playlist.loop = PLAYLIST_LOOP_REPEAT;
	else if(!strcasecmp(p, "shuffle+"))
	  gui->playlist.loop = PLAYLIST_LOOP_SHUF_PLUS;
	else if(!strcasecmp(p, "shuffle"))
	  gui->playlist.loop = PLAYLIST_LOOP_SHUFFLE;
	else {
	  fprintf(stderr, _("Bad loop mode '%s', see xine --help\n"), optarg);
	  exit(1);
	}
      }
      else
	gui->playlist.loop = PLAYLIST_LOOP_LOOP;
      break;

    case OPTION_SK_SERVER:
      if (optarg) {
        gui->skin_server_url = strdup(optarg);
      }
      break;
      
    case OPTION_ENQUEUE:
      if(is_remote_running(((session >= 0) ? session : 0))) {
	if(session < 0)
	  session = 0;
      }
      break;

    case OPTION_VERBOSE:
      if(optarg != NULL)
	__xineui_global_verbosity = strtol(optarg, &optarg, 10);
      else
	__xineui_global_verbosity = 1;
      break;

#ifdef XINE_PARAM_BROADCASTER_PORT    
    case OPTION_BROADCAST_PORT:
      if(optarg != NULL)
	gui->broadcast_port = strtol(optarg, &optarg, 10);
      break;
#endif
      
    case OPTION_NO_LOGO:
      gui->display_logo = 0;
      break;

    case OPTION_NO_MOUSE:
      gui->no_mouse = 1;
      break;
      
    case 'S':
      if (optarg) {
      if(is_remote_running(((session >= 0) ? session : 0)))
	retval = session_handle_subopt(optarg, NULL, &session);
      else {
	
	session_argv = (char **) realloc(session_argv, sizeof(char *) * (session_argv_num + 2));

	session_argv[session_argv_num++] = strdup(optarg);
	session_argv[session_argv_num]   = NULL;
      }
      }
      break;

    case 'Z':
      no_auto_start = 1;
      break;

    case 'c':
      {
        if (optarg) {
          char *cfg = xine_chomp(optarg);
          __xineui_global_config_file = xitk_filter_filename (cfg);
        }
      }
      break;

    case 'E':
      no_old_playlist = 1;
      break;

    case OPTION_POST:
      if (optarg) {
        pplugins = (char **) realloc(pplugins, sizeof(char *) * (pplugins_num + 2));
      
        pplugins[pplugins_num++] = optarg;
        pplugins[pplugins_num] = NULL;
      }
      break;

    case OPTION_DISABLE_POST:
      gui->post_video_enable = 0;
      gui->post_audio_enable = 0;
      break;

    case OPTION_NO_SPLASH:
      gui->splash = 0;
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

    case OPTION_STDCTL:
      gui->stdctl_enable = 1;
      break;

    case 'T':
      tvout = optarg;
      break;
      
    case OPTION_LIST_PLUGINS:
      {
	char *p = NULL;
	
	if(optarg)
	  p = xine_chomp(optarg);
	
	list_plugins(p);
	exit(1);
      }
      break;

    case OPTION_BUG_REPORT:
      {
	FILE   *f;

	if(!(f = fopen("BUG-REPORT.TXT", "w+")))
	  fprintf(stderr, "fopen(%s) failed: %s.\n", "BUG-REPORT.TXT", strerror(errno));
	else {
	  
	  printf(_("*** NOTE ***\n"));
	  printf(_(" Bug Report mode: All output messages will be added in BUG-REPORT.TXT file.\n"));

#ifdef ENABLE_NLS
	if((xitk_set_locale()) != NULL) {
	  setlocale(LC_ALL, "");
          setlocale(LC_NUMERIC, "C");
        }
#endif

	  if (dup2((fileno(f)), STDOUT_FILENO) < 0)
	    fprintf(stderr, "dup2() failed: %s.\n", strerror(errno));
	  else {
	    if (dup2((fileno(f)), STDERR_FILENO) < 0)
	      fprintf(stdout, "dup2() failed: %s.\n", strerror(errno));
	  }

	  gui->report = f;
	  __xineui_global_verbosity = 0xff;
	}
	
	if(optarg) {
	  char *p = xine_chomp(optarg);

	  session_argv = (char **) realloc(session_argv, sizeof(char *) * (session_argv_num + 2));
	  
          session_argv[session_argv_num] = xitk_asprintf("mrl=%s", p);
	  session_argv[++session_argv_num]   = NULL;

	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_PLAY;
	  if(aos < MAX_ACTIONS_ON_START)
	    gui->actions_on_start[aos++] = ACTID_QUIT;
	}
      }
      break;

    case 'I':
      gui->no_gui = 1;
      break;
    case 'W': /* Select wid */
      if(optarg != NULL) {
        sscanf(optarg, "%i", &window_id);
     	gui->wid = window_id;
      } 
      else {
	fprintf(stderr, _("window id required for -W option\n"));
	exit(1);
      }
      break;
      
    default:
      show_usage();
      fprintf(stderr, _("invalid argument %d => exit\n"), c);
      exit(1);
    }
  }
  
  if(session >= 0) {
    /* Session feature was used, say good bye */
    if(is_remote_running(session) && (_argc - optind)) {
      /* send all remaining MRL options to session */
      int i;

      if(_argv[optind]) {
	for(i = optind; i < _argc; i++) {
	  char enqueue_mrl[strlen(_argv[i]) + 256];
	  snprintf(enqueue_mrl, sizeof(enqueue_mrl), "session=%d", session);
	  (void) session_handle_subopt(enqueue_mrl, _argv[i], &session);
	}
      }
      else
	fprintf(stderr, _("You should specify at least one MRL to enqueue!\n"));

    }

    free_command_line_args(&_argv, _argc);

    return retval;
  }

  /* 
   * Using root window mode don't allow
   * window geometry, so, reset those params.
   */
  if(gui->use_root_window) {
    gui->window_attribute.x =
    gui->window_attribute.y = -8192;
    gui->window_attribute.width =
    gui->window_attribute.height = -1;
    gui->window_attribute.borderless = 0;
  }
  
  show_banner();
  
#ifndef DEBUG
  /* Make non-verbose stdout really quiet but keep a copy for special use, e.g. stdctl feedback */
  if(!__xineui_global_verbosity) {
    int   guiout_fd, stdout_fd;
    FILE *guiout_fp;
    
    if((guiout_fd = dup(STDOUT_FILENO)) < 0)
      fprintf(stderr, "cannot dup STDOUT_FILENO: %s.\n", strerror(errno));
    else if((guiout_fp = fdopen(guiout_fd, "w")) == NULL)
      fprintf(stderr, "cannot fdopen guiout_fd: %s.\n", strerror(errno));
    else if((stdout_fd = open("/dev/null", O_WRONLY)) < 0)
      fprintf(stderr, "cannot open /dev/null: %s.\n", strerror(errno));
    else {
      if (fcntl(guiout_fd, F_SETFD, FD_CLOEXEC) < 0) {
        fprintf(stderr, "cannot make guiout_fd uninheritable: %s.\n", strerror(errno));
      }

      if(dup2(stdout_fd, STDOUT_FILENO) < 0)
        fprintf(stderr, "cannot dup2 stdout_fd: %s.\n", strerror(errno));
      else {
        gui->orig_stdout = guiout_fp;
        setlinebuf(gui->orig_stdout);
      }

      close(stdout_fd); /* stdout_fd was intermediate, not needed any longer */
    }
  }
#endif

  /*
   * Initialize config
   */
  if(__xineui_global_config_file == NULL) {
    struct stat st;
    __xineui_global_config_file = _config_file();

    /* Popup setup window if there is no config file */
    if(!__xineui_global_config_file || stat(__xineui_global_config_file, &st) < 0) {
      if(aos < MAX_ACTIONS_ON_START)
	gui->actions_on_start[aos++] = ACTID_SETUP;
    }
    
  }
  
  /*
   * Initialize keymap
   */
  if(gui->keymap_file == NULL) {
    const char *cfgdir = CONFIGDIR;
    const char *keymap = "keymap";

    gui->keymap_file = xitk_asprintf("%s/%s/%s", xine_get_homedir(), cfgdir, keymap);
  }
  
  pthread_mutex_init (&gui->seek_mutex, NULL);
  gui->seek_running = 0;
  gui->seek_pos = -1;
  gui->seek_timestep = 0;

  pthread_mutex_init (&gui->no_messages.mutex, NULL);
  gui->no_messages.until.tv_sec = 0;
  gui->no_messages.until.tv_usec = 0;
  gui->no_messages.level = 0;

  __xineui_global_xine_instance = gui->xine = xine_new ();
  if (__xineui_global_config_file)
    xine_config_load (gui->xine, __xineui_global_config_file);
  xine_engine_set_param (gui->xine, XINE_ENGINE_PARAM_VERBOSITY, __xineui_global_verbosity);
  
  /* 
   * Playlist auto reload 
   */
  old_playlist_cfg = 
    xine_config_register_bool (gui->xine, "gui.playlist_auto_reload", 
			      0,
			      _("Automatically reload old playlist"),
			      _("If it's enabled and if you don't specify any MRL in command "
				"line, xine will automatically load previous playlist."), 
			      CONFIG_LEVEL_BEG,
			      dummy_config_cb,
			      gGui);

  if(old_playlist_cfg && (!(_argc - optind)) && (!no_old_playlist)) {
    char buffer[XITK_PATH_MAX + XITK_NAME_MAX + 2];
    
    snprintf(buffer, sizeof(buffer), "%s/.xine/xine-ui_old_playlist.tox", xine_get_homedir());
    mediamark_load_mediamarks(buffer);
  }

  gui->subtitle_autoload = 
    xine_config_register_bool (gui->xine, "gui.subtitle_autoload", 1,
			      _("Subtitle autoloading"),
			      _("Automatically load subtitles if they exist."), 
			      CONFIG_LEVEL_BEG,
			      sub_autoload_cb,
			      gGui);


  enable_deinterlace +=
    xine_config_register_bool (gui->xine, "gui.deinterlace_by_default", 0,
			      _("Enable deinterlacing by default"),
			      _("Deinterlace plugin will be enabled on "
			        "startup. Progressive streams are automatically "
			        "detected with no performance penalty."), 
			        CONFIG_LEVEL_BEG,
			        NULL,
			        CONFIG_NO_DATA);

  if( enable_deinterlace ) {
    if(aos < MAX_ACTIONS_ON_START)
      gui->actions_on_start[aos++] = ACTID_TOGGLE_INTERLEAVE;
  }

  /*
   * init gui
   */
  gui_init (gui, _argc - optind, &_argv[optind], &gui->window_attribute);

  pthread_mutex_init(&gui->download_mutex, NULL);

  pthread_mutexattr_init(&mutexattr);
  pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&gui->logo_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);

  /* Automatically start playback if new_mode is enabled and playlist is filled */
  if((gui->smart_mode && 
      (gui->playlist.num || actions_on_start(gui->actions_on_start, ACTID_PLAYLIST)) &&
      (!(actions_on_start(gui->actions_on_start, ACTID_PLAY)))) && (no_auto_start == 0)) {
    if(aos < MAX_ACTIONS_ON_START)
      gui->actions_on_start[aos++] = ACTID_PLAY;
  }
  
  /*
   * xine init
   */
  xine_init (gui->xine);

  /* Get old working path from input plugin */
  {
    xine_cfg_entry_t  cfg_entry;
    
    if(xine_config_lookup_entry (gui->xine, "media.files.origin_path", &cfg_entry))
      strlcpy(gui->curdir, cfg_entry.str_value, sizeof(gui->curdir));
    else
      if (!getcwd(&(gui->curdir[0]), XITK_PATH_MAX))
        gui->curdir[0] = 0;
  }

  /*
   * load and init output drivers
   */
  /* Video out plugin */
  driver_num = -1;
  {
    const char *const *vids = xine_list_video_output_plugins (gui->xine);
    int                i = 0;
    
    while(vids[i++]);
    
    gui->video_driver_ids = (char **)calloc ((i + 1), sizeof (char *));
    i = 0;
    gui->video_driver_ids[i] = strdup ("auto");
    while(vids[i]) {
      gui->video_driver_ids[i + 1] = strdup (vids[i]);
      i++;
    }
    
    gui->video_driver_ids[i + 1] = NULL;
    
    if(video_driver_id) {
      for (i = 0; gui->video_driver_ids[i] != NULL; i++) {
        if (!strcasecmp (video_driver_id, gui->video_driver_ids[i])) {
	  driver_num = i;
	  break;
	}
      }
    }
    gui->vo_port = load_video_out_driver(driver_num);
  }  
  
  {
    xine_cfg_entry_t  cfg_vo_entry;
    
    if(xine_config_lookup_entry (gui->xine, "video.driver", &cfg_vo_entry)) {

      if (!strcasecmp (gui->video_driver_ids[cfg_vo_entry.num_value], "dxr3")) {
	xine_cfg_entry_t  cfg_entry;
	
	if(xine_config_lookup_entry (gui->xine, "dxr3.output.mode", &cfg_entry)) {
	  if(((!strcmp(cfg_entry.enum_values[cfg_entry.num_value], "letterboxed tv")) ||
	      (!strcmp(cfg_entry.enum_values[cfg_entry.num_value], "widescreen tv"))) && 
	     (!(actions_on_start(gui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)))) {
	    if(aos < MAX_ACTIONS_ON_START)
	      gui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
	  }
	}
      }
      else if (!strcasecmp (gui->video_driver_ids[cfg_vo_entry.num_value], "fb")) {
	if(aos < MAX_ACTIONS_ON_START)
	  gui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
      }

    }
  }
  SAFE_FREE(video_driver_id);

  /* Audio out plugin */
  driver_num = -1;
  {
    const char *const *aids = xine_list_audio_output_plugins (gui->xine);
    int                i = 0;
    
    while(aids[i++]);
    
    gui->audio_driver_ids = (char **)calloc ((i + 2), sizeof (char *));
    i = 0;
    gui->audio_driver_ids[i] = strdup ("auto");
    gui->audio_driver_ids[i + 1] = strdup ("null");
    while(aids[i]) {
      gui->audio_driver_ids[i + 2] = strdup (aids[i]);
      i++;
    }
    
    gui->audio_driver_ids[i + 2] = NULL;
    
    if(audio_driver_id) {
      for (i = 0; gui->audio_driver_ids[i] != NULL; i++) {
        if (!strcasecmp (audio_driver_id, gui->audio_driver_ids[i])) {
	  driver_num = i;
	  break;
	}
      }
    }
    gui->ao_port = load_audio_out_driver(driver_num);    
  }
  SAFE_FREE(audio_driver_id);

  post_deinterlace_init(pdeinterlace);
  post_init();

  gui->stream = xine_stream_new (gui->xine, gui->ao_port, gui->vo_port);
#ifdef XINE_PARAM_EARLY_FINISHED_EVENT
  if( xine_check_version(1,1,1) )
      xine_set_param(gui->stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
#endif

  gui->vo_none = xine_open_video_driver (gui->xine, "none", XINE_VISUAL_TYPE_NONE, NULL);
  gui->ao_none = xine_open_audio_driver (gui->xine, "none", NULL);

  osd_init();

  /*
   * Setup logo.
   */
#ifdef XINE_LOGO2_MRL
#  define USE_XINE_LOGO_MRL XINE_LOGO2_MRL
#else
#  define USE_XINE_LOGO_MRL XINE_LOGO_MRL
#endif
  gui->logo_mode = 0;
  gui->logo_has_changed = 0;
  gui->logo_mrl = xine_config_register_string (gui->xine, "gui.logo_mrl", USE_XINE_LOGO_MRL,
    _("Logo MRL"), CONFIG_NO_HELP, CONFIG_LEVEL_EXP, main_change_logo_cb, gGui);

  gui->event_queue = xine_event_new_queue(gui->stream);
  xine_event_create_listener_thread (gui->event_queue, event_listener, gui);

  if(tvout && strlen(tvout)) {
    if((gui->tvout = tvout_init(gui->display, tvout)))
      tvout_setup(gui->tvout);
  }

  xine_set_param(gui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, audio_channel);
  xine_set_param(gui->stream, XINE_PARAM_SPU_CHANNEL, spu_channel);
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_REPORT_LEVEL, 0);
  xine_set_param(gui->stream, XINE_PARAM_AUDIO_AMP_LEVEL, gui->mixer.amp_level);
#ifdef XINE_PARAM_BROADCASTER_PORT
  xine_set_param(gui->stream, XINE_PARAM_BROADCASTER_PORT, gui->broadcast_port);
#endif

  /* Visual animation stream init */
  gui->visual_anim.stream = xine_stream_new (gui->xine, NULL, gui->vo_port);
  gui->visual_anim.event_queue = xine_event_new_queue(gui->visual_anim.stream);
  gui->visual_anim.current = 0;
  xine_event_create_listener_thread (gui->visual_anim.event_queue, event_listener, gui);
  xine_set_param(gui->visual_anim.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -2);
  xine_set_param(gui->visual_anim.stream, XINE_PARAM_SPU_CHANNEL, -2);
  xine_set_param(gui->visual_anim.stream, XINE_PARAM_AUDIO_REPORT_LEVEL, 0);

  /* subtitle stream */
  gui->spu_stream = xine_stream_new (gui->xine, NULL, gui->vo_port);
  xine_set_param(gui->spu_stream, XINE_PARAM_AUDIO_REPORT_LEVEL, 0);

  /* init the video window */
  video_window_select_visual (gui->vwin);

  xine_set_param(gui->stream, XINE_PARAM_VO_ASPECT_RATIO, aspect_ratio);
        
  /*
   * hand control over to gui
   */
  gui->actions_on_start[aos] = ACTID_NOKEY;
  
  /* Initialize posts, if required */
  if(pplugins_num) {
    char             **plugin = pplugins;
    
    while(*plugin) {
      vpplugin_parse_and_store_post((const char *) *plugin);
      applugin_parse_and_store_post((const char *) *plugin);
      plugin++;
    }
    
    vpplugin_rewire_posts();
    applugin_rewire_posts();
  }

  gui_run(session_argv);

  /*
   * Close X display before unloading modules linked against libGL.so
   * https://www.xfree86.org/4.3.0/DRI11.html
   *
   * Do not close the library with dlclose() until after XCloseDisplay() has
   * been called. When libGL.so initializes itself it registers several
   * callbacks functions with Xlib. When XCloseDisplay() is called those
   * callback functions are called. If libGL.so has already been unloaded
   * with dlclose() this will cause a segmentation fault.
   */
  gui->x_lock_display (gui->display);
  gui->x_unlock_display (gui->display);
  XCloseDisplay(gui->display);
  if( gui->video_display != gui->display ) {
    gui->x_lock_display (gui->video_display);
    gui->x_unlock_display (gui->video_display);
    XCloseDisplay(gui->video_display);
  }

  xine_exit (gui->xine);

  visual_anim_done();
  free(pplugins);

  if(session_argv_num) {
    int i = 0;
    
    while(session_argv[i])
      free(session_argv[i++]);
    
    free(session_argv);
  }

  pthread_mutex_destroy(&gui->mmk_mutex);
  pthread_mutex_destroy (&gui->no_messages.mutex);
  pthread_mutex_destroy (&gui->seek_mutex);
  pthread_mutex_destroy(&gui->download_mutex);
  pthread_mutex_destroy(&gui->logo_mutex);

  if(gui->report != stdout)
    fclose(gui->report);
  if(gui->orig_stdout != stdout)
    fclose(gui->orig_stdout);

  free_command_line_args(&_argv, _argc);

  {
    int ii;
    for (ii = 0; gui->video_driver_ids[ii]; ii++)
      free (gui->video_driver_ids[ii]);
    free (gui->video_driver_ids);
    for (ii = 0; gui->audio_driver_ids[ii]; ii++)
      free (gui->audio_driver_ids[ii]);
    free (gui->audio_driver_ids);
  }

  free (__xineui_global_config_file);

  return retval;
}

