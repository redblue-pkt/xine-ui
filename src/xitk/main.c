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

/*
 * global variables
 */
gGui_t                       *gGui;
#ifdef HAVE_LIRC
int                           no_lirc;
#endif

static char                 **video_driver_ids;
static char                 **audio_driver_ids;
static window_attributes_t    window_attribute;

#define CONFIGFILE "config"

typedef struct {
  FILE    *fd;
  char    *filename;
  char    *ln;
  char     buf[256];
} file_info_t;

#define	OPTION_VISUAL		1000
#define	OPTION_INSTALL_COLORMAP	1001
#define DISPLAY_KEYMAP          1002
#define OPTION_SK_SERVER        1003
#define OPTION_ENQUEUE          2000

/* options args */
static const char *short_options = "?hHgfvn"
#ifdef HAVE_XINERAMA
 "F"
#endif
#ifdef HAVE_LIRC
 "L"
#endif
#ifdef HAVE_XF86VIDMODE
 "F"
#endif
#ifdef DEBUG
 "d:"
#endif
 "u:a:V:A:p::s:RG:BN:P:l::S:";

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
#ifdef HAVE_XINERAMA
  {"xineramafull"   , no_argument      , 0, 'F'                      },
#endif
  {"visual"	    , required_argument, 0,  OPTION_VISUAL           },
  {"install"	    , no_argument      , 0,  OPTION_INSTALL_COLORMAP },
  {"keymap"         , optional_argument, 0,  DISPLAY_KEYMAP          },
  {"network"        , no_argument      , 0, 'n'                      },
  {"root"           , no_argument      , 0, 'R'                      },
  {"geometry"       , required_argument, 0, 'G'                      },
  {"borderless"     , no_argument      , 0, 'B'                      },
  {"animation"      , required_argument, 0, 'N'                      },
  {"playlist"       , required_argument, 0, 'P'                      },
  {"loop"           , optional_argument, 0, 'l'                      },
  {"skin-server-url", required_argument, 0, OPTION_SK_SERVER         },
  {"enqueue"        , required_argument, 0, OPTION_ENQUEUE           },
  {"session"        , required_argument, 0, 'S'                      },
  {"version"        , no_argument      , 0, 'v'                      },
  {0                , no_argument      , 0,  0                       }
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

static int parse_geometry(window_attributes_t *window_attribute, char *geomstr) {
  int width, height, xoff, yoff, ret;  
  
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
  int visual = 0;

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
  char         *display_name = ":0.0";
  Display      *display;
  char          user_dbname[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char          environement_buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char          wide_dbname[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  char         *environment;
  char         *classname = "xine";
  char         *str_type[20];
  XrmDatabase   rmdb, home_rmdb, server_rmdb, application_rmdb;
  XrmValue      value;
  
  XrmInitialize();
  
  sprintf(wide_dbname, "%s%s", "/usr/lib/X11/app-defaults/", classname);
  
  if(getenv("DISPLAY"))
    display_name = getenv("DISPLAY");
  
  display = XOpenDisplay(display_name);
  
  application_rmdb = XrmGetFileDatabase(wide_dbname);
  (void) XrmMergeDatabases(application_rmdb, &rmdb);
  
  if(XResourceManagerString(display) != NULL)
    server_rmdb = XrmGetStringDatabase(XResourceManagerString(display));
  else {
    sprintf(user_dbname, "%s%s", (xine_get_homedir()), "/.Xdefaults");
    server_rmdb = XrmGetFileDatabase(user_dbname);
  }
  
  (void) XrmMergeDatabases(server_rmdb, &rmdb);
  
  if((environment = getenv("XENVIRONMENT")) == NULL) {
    int len;
    
    environment = environement_buf;
    sprintf(environement_buf, "%s%s", (xine_get_homedir()), "/.Xdefaults-");
    len = strlen(environment);
    (void) gethostname(environment + len, (XITK_PATH_MAX + XITK_NAME_MAX) - len);
  }
  
  home_rmdb = XrmGetFileDatabase(environment);
  (void) XrmMergeDatabases(home_rmdb, &rmdb);

  if(XrmGetResource(rmdb, "xine.geometry", "xine.Geometry", str_type, &value) == True) {
    if(!parse_geometry(&window_attribute, (char *)value.addr))
      printf(_("Bad geometry '%s'\n"), (char *)value.addr);
  } 
  if(XrmGetResource(rmdb, "xine.border", "xine.Border", str_type, &value) == True) {
    window_attribute.borderless = !get_bool_value((char *) value.addr);
  }
  if(XrmGetResource(rmdb, "xine.visual", "xine.Visual", str_type, &value) == True) {
    if(!parse_visual(&gGui->prefered_visual_id, 
		     &gGui->prefered_visual_class, (char *)value.addr)) {
      printf(_("Bad visual '%s'\n"), (char *)value.addr);
    }
  }
  if(XrmGetResource(rmdb, "xine.colormap", "xine.Colormap", str_type, &value) == True) {
    gGui->install_colormap = !get_bool_value((char *) value.addr);
  }
  
  XrmDestroyDatabase(rmdb);
  
  if(display)
    XCloseDisplay(display);

}

static void main_change_logo_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->logo_mrl = cfg->str_value;
}

/*
 *
 */
void show_version(void) {

  printf(_("This is xine (X11 gui) - a free video player v%s\n"
	   "(c) 2000-2003 by G. Bartsch and the xine project team.\n"), VERSION);
}

/*
 *
 */
void show_banner(void) {
  int major, minor, sub;

  show_version();
  printf(_("Built with xine library %d.%d.%d (%s)\n"),
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION, XINE_VERSION);
  
  xine_get_version (&major, &minor, &sub);

  printf(_("Found xine library version: %d.%d.%d (%s).\n"), 
	 major, minor, sub, xine_get_version_string());

}

/*
 *
 */
void show_usage (void) {
  const char   *const *driver_ids;
  const char   *driver_id;
  xine_t       *xine;
  char         *cfgdir = ".xine";
  char         *cfgfile = CONFIGFILE;
  char         *configfile;
  
  if(!(configfile = getenv("XINERC"))) {
    configfile = (char *) xine_xmalloc(strlen(xine_get_homedir())
				       + strlen(cfgdir) 
				       + strlen(cfgfile)
				       + 3);
    sprintf(configfile, "%s/%s", xine_get_homedir(), cfgdir);
    mkdir(configfile, 0755);
    sprintf(configfile + strlen(configfile), "/%s", cfgfile);
  }
  
  xine = xine_new();
  xine_config_load(xine, configfile);
  xine_init(xine);
  
  printf("\n");
  printf(_("Usage: xine [OPTIONS]... [MRL]\n"));
  printf("\n");
  printf("OPTIONS are:\n");
  printf(_("  -v, --version                Display version.\n"));
  printf(_("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n"));
  printf("                               ");
  driver_ids = xine_list_video_output_plugins (xine);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");

  printf(_("  -A, --audio-driver <drv>     Select audio driver by id. Available drivers: \n"));
  printf("                               null ");
  driver_ids = xine_list_audio_output_plugins (xine);
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
#ifdef HAVE_XINERAMA
  printf(_("                    'F': in xinerama fullscreen mode.\n"));
#endif
  printf(_("                    'h': hide GUI (panel, etc.).\n"));
  printf(_("                    'w': hide video window.\n"));
  printf(_("                    'q': quit when play is done.\n"));
  printf(_("                    'd': retrieve playlist from DVD. (deprecated. use -s DVD)\n"));
  printf(_("                    'v': retrieve playlist from VCD. (deprecated. use -s VCD)\n"));
  printf(_("  -s, --auto-scan <plugin>     auto-scan play list from <plugin>\n"));
  printf(_("  -f, --fullscreen             start in fullscreen mode,\n"));
#ifdef HAVE_XINERAMA
  printf(_("  -F, --xineramafull           start in xinerama fullscreen (display on several screens),\n"));
#endif
  printf(_("  -g, --hide-gui               hide GUI (panel, etc.)\n"));
  printf(_("  -H, --hide-video             hide video window\n"));
#ifdef HAVE_LIRC
  printf(_("  -L, --no-lirc                Turn off LIRC support.\n"));
#endif
  printf(_("      --visual <class-or-id>   Use the specified X11 visual. <class-or-id>\n"));
  printf(_("                               is either an X11 visual class, or a visual id.\n"));
  printf(_("      --install                Install a private colormap.\n"));
  printf(_("      --keymap [=option]       Display keymap. Option are:\n"));
  printf(_("                                 'default': display default keymap table,\n"));
  printf(_("                                 'lirc': display draft of a .lircrc config file.\n"));
  printf(_("                                 'remapped': user remapped keymap table.\n"));
  printf(_("                                 -if no option is given, 'default' is selected.\n"));
  printf(_("  -n, --network                Enable network remote control server.\n"));
  printf(_("  -R, --root                   Use root window as video window.\n"));
  printf(_("  -G, --geometry <WxH[+X+Y]>   Set output window geometry (X style).\n"));
  printf(_("  -B, --borderless             Borderless video output window.\n"));
  printf(_("  -N, --animation <mrl>        Specify mrl to play when video output isn't used.\n"));
  printf(_("                                 -can be used more than one time.\n"));
  printf(_("  -P, --playlist <filename>    Load a playlist file.\n"));
  printf(_("  -l, --loop [=mode]           Set playlist loop mode (default: loop). Modes are:\n"));
  printf(_("                                 'loop': loop entire playlist.\n"));
  printf(_("                                 'repeat': repeat current playlist entry.\n"));
  printf(_("                                 'shuffle': select randomly a yet unplayed entry from playlist\n"));
  printf(_("                                 'shuffle+': same as 'shuffle', but indefinetely replay the playlist.\n"));
  printf(_("      --skin-server-url <url>  Define the skin server url.\n"));
  printf(_("      --enqueue <mrl>          Enqueue mrl of a running session (session 0)\n"));
  printf(_("  -S, --session <option1,option2,...>\n"));
  printf(_("                               Session managements. Options can be:\n"));
  printf(_("                    session=n   specify session <n> number,\n"));
  printf(_("                    mrl=m       add mrl <m> to the playlist,\n"));
  printf(_("                    audio=c     select audio channel (<c>: 'next' or 'prev'),\n"));
  printf(_("                    spu=c       select spu channel (<c>: 'next' or 'prev'),\n"));
  printf(_("                    (playlist|pl)=p\n"));
  printf(_("                                 <p> can be:\n"));
  printf(_("                                   'clear'  clear the playlist,\n"));
  printf(_("                                   'next'   play next playlist entry,\n"));
  printf(_("                                   'prev'   play previous playlist entry,\n"));
  printf(_("                                   'load:s' load playlist file <s>.\n"));
  printf(_("                    play, slow2, slow4, pause, fast2,\n"));
  printf(_("                    fast4, stop, quit, fullscreen, eject.\n"));
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

  xine_exit(xine);
}

/*
 * Try to load video output plugin, by stored name or probing
 */
static xine_video_port_t *load_video_out_driver(int driver_number) {
  xine_video_port_t      *video_port = NULL;
  double                  res_h, res_v;
  x11_visual_t            vis;
  int                     driver_num;

  vis.display           = gGui->display;
  vis.screen            = gGui->screen;
  vis.d                 = gGui->video_window;
  res_h                 = (DisplayWidth  (gGui->display, gGui->screen)*1000 
			   / DisplayWidthMM (gGui->display, gGui->screen));
  res_v                 = (DisplayHeight (gGui->display, gGui->screen)*1000
			   / DisplayHeightMM (gGui->display, gGui->screen));
  
  gGui->pixel_aspect    = res_v / res_h;
  
#ifdef DEBUG
  printf("pixel_aspect: %f\n", gGui->pixel_aspect);
#endif

  vis.dest_size_cb      = video_window_dest_size_cb;
  vis.frame_output_cb   = video_window_frame_output_cb;
  vis.user_data         = NULL;
  
  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  driver_num = 
    xine_config_register_enum(gGui->xine, "video.driver", 
			      0, video_driver_ids,
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
    
    if((!strcasecmp(video_driver_ids[driver_num], "none")) || 
       (!strcasecmp(video_driver_ids[driver_num], "null"))) {
      video_port = xine_open_video_driver(gGui->xine,
					  video_driver_ids[driver_num],
					  XINE_VISUAL_TYPE_NONE,
					  (void *) &vis);
      if (video_port)
	return video_port;
      
    }
    else if(strcasecmp(video_driver_ids[driver_num], "auto")) {
      
      video_port = xine_open_video_driver(gGui->xine, 
					  video_driver_ids[driver_num],
					  XINE_VISUAL_TYPE_X11,
					  (void *) &vis);
      if (video_port)
	return video_port;
    }
    
    /* note: xine-lib can do auto-probing for us if we want.
     *       but doing it here should do no harm.
     */
    i = 0;
    driver_ids = xine_list_video_output_plugins (gGui->xine);

    while (driver_ids[i]) {
      
      printf (_("main: probing <%s> video output plugin\n"), driver_ids[i]);
      
      video_port = xine_open_video_driver(gGui->xine, 
					  driver_ids[i],
					  XINE_VISUAL_TYPE_X11, 
					  (void *) &vis);
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
    if((!strcasecmp(video_driver_ids[driver_number], "none")) 
       || (!strcasecmp(video_driver_ids[driver_number], "null"))) {
      video_port = xine_open_video_driver(gGui->xine,
					  video_driver_ids[driver_number],
					  XINE_VISUAL_TYPE_NONE,
					  (void *) &vis);
      
      /* do not save on config, otherwise user would never see images again... */
    }
    else {
      video_port = xine_open_video_driver(gGui->xine,
					  video_driver_ids[driver_number],
					  XINE_VISUAL_TYPE_X11, 
					  (void *) &vis);
      
      /* save requested driver (-V) */ 
      if(video_port)
        config_update_num("video.driver", driver_number);
    }
    
    if(!video_port) {
      printf (_("main: video driver <%s> failed\n"), video_driver_ids[driver_number]);
      exit (1);
    }
    
  }

  return video_port;
}

/*
 * Try to load audio output plugin, by stored name or probing
 */
static xine_audio_port_t *load_audio_out_driver(int driver_number) {
  xine_audio_port_t      *audio_port = NULL;
  int                     driver_num;
  
  /*
   * Setting default (configfile stuff need registering before updating, etc...).
   */
  driver_num = 
    xine_config_register_enum(gGui->xine, "video.driver", 
			      0, video_driver_ids,
			      _("video driver to use"),
			      _("Choose video driver. "
				"NOTE: you may restart xine to use the new driver"),
			      CONFIG_LEVEL_ADV,
			      CONFIG_NO_CB, 
			      CONFIG_NO_DATA);
  

  driver_num = 
    xine_config_register_enum(gGui->xine, "audio.driver", 
			      0, audio_driver_ids,
			      _("audio driver to use"),
			      _("Choose audio driver. "
				"NOTE: you may restart xine to use the new driver"),
			      CONFIG_LEVEL_ADV,
			      CONFIG_NO_CB, 
			      CONFIG_NO_DATA);
  
  if (driver_number < 0) {
    const char *const *driver_ids;
    int    i;
    
    if (strcasecmp(audio_driver_ids[driver_num], "auto")) {
      
      /* don't want to load an audio driver ? */
      if (!strncasecmp(audio_driver_ids[driver_num], "NULL", 4)) {
        printf(_("main: not using any audio driver (as requested).\n"));
        return NULL;
      }
      
      audio_port = xine_open_audio_driver(gGui->xine, 
					  audio_driver_ids[driver_num],
					  NULL);
      if (audio_port)
	return audio_port;
    }
    
    /* note: xine-lib can do auto-probing for us if we want.
     *       but doing it here should do no harm.
     */
    i = 0;
    driver_ids = xine_list_audio_output_plugins (gGui->xine);

    while (driver_ids[i]) {
      
      printf (_("main: probing <%s> audio output plugin\n"), driver_ids[i]);
      
      audio_port = xine_open_audio_driver(gGui->xine, 
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
    if (!strncasecmp (audio_driver_ids[driver_number], "NULL", 4)) {

      printf(_("main: not using any audio driver (as requested).\n"));

      /* calling -A null is useful to developers, but we should not save it at
       * config. if user doesn't have a sound card he may go to setup screen
       * changing audio.driver to NULL in order to make xine start a bit faster.
       */
    
    }
    else {
    
      audio_port = xine_open_audio_driver(gGui->xine, audio_driver_ids[driver_number], NULL);

      if (!audio_port) {
        printf (_("main: audio driver <%s> failed\n"), audio_driver_ids[driver_number]);
        exit (1);
      }
    
      /* save requested driver (-A) */ 
      config_update_num("audio.driver", driver_number);
    }
  
  }

  return audio_port;
}

/*
 *
 */
static void event_listener(void *user_data, const xine_event_t *event) {
  struct timeval tv;

  /*
   * Ignoring finished event logo is displayed (or played), that save us
   * from a loop of death
   */
  if(gGui->logo_mode && (event->type == XINE_EVENT_UI_PLAYBACK_FINISHED))
    return;
  
  gettimeofday (&tv, NULL);
  
  if(abs(tv.tv_sec - event->tv.tv_sec) > 3) {
    printf("Event too old, discarding\n");
    return;
  }
  
  switch(event->type) { 
    
  /* frontend can e.g. move on to next playlist entry */
  case XINE_EVENT_UI_PLAYBACK_FINISHED:
    /*    printf("xitk/main.c: playback finished\n"
     *	   "             event->stream=%d,\n"
     *	   "gGui->stream=%d, gGui->visual_anim.stream=%d\n",
     *	   event->stream, gGui->stream, gGui->visual_anim.stream);
     */
    if(event->stream == gGui->stream) {
      /* printf("xitk/main.c: playing next stream...\n"); */
      gui_playlist_start_next();
    }
    else if(event->stream == gGui->visual_anim.stream) {
      /* printf("xitk/main.c: restarting visual stream...\n"); */
      visual_anim_play_next();
    }
    break;
    
    /* inform ui that new channel info is available */
  case XINE_EVENT_UI_CHANNELS_CHANGED:
    if(event->stream == gGui->stream)
      panel_update_channel_display();
    break;
    
    /* request title display change in ui */
  case XINE_EVENT_UI_SET_TITLE:
    if(event->stream == gGui->stream) {
      xine_ui_data_t *uevent = (xine_ui_data_t *) event->data;
      
      if(strcmp(gGui->mmk.ident, uevent->str)) {
	
	if(gGui->mmk.ident)
	  free(gGui->mmk.ident);
	if(gGui->playlist.mmk != NULL && 
	   gGui->playlist.mmk[gGui->playlist.cur] != NULL ) {
	  
	  if(gGui->playlist.mmk[gGui->playlist.cur]->ident)
	    free(gGui->playlist.mmk[gGui->playlist.cur]->ident);
	  
	  gGui->playlist.mmk[gGui->playlist.cur]->ident = strdup(uevent->str);
	}
	gGui->mmk.ident = strdup(uevent->str);
	
	playlist_mrlident_toggle();
	panel_update_mrl_display();
      }
    }
    break;
    
    /* message (dialog) for the ui to display */
  case XINE_EVENT_UI_MESSAGE: 
    if(event->stream == gGui->stream) {
      xine_ui_data_t *uevent = (xine_ui_data_t *) event->data;
      
      if(uevent->str) {
	xine_info((char *)uevent->str);
	osd_display_info((char *)uevent->str);
      }
    }
    break;
    
    /* e.g. aspect ratio change during dvd playback */
  case XINE_EVENT_FRAME_FORMAT_CHANGE:
    break;

    /* report current audio level (l/r) */
  case XINE_EVENT_AUDIO_LEVEL:
    if(event->stream == gGui->stream) {
      xine_audio_level_data_t *aevent = (xine_audio_level_data_t *) event->data;
      
      printf("XINE_EVENT_AUDIO_LEVEL: left 0>%d<255, right 0>%d<255\n", 
	     aevent->left, aevent->right);
    }
    break;

    /* last event sent when stream is disposed */
  case XINE_EVENT_QUIT:
    break;
    
    /* index creation/network connections */
  case XINE_EVENT_PROGRESS:
    if(event->stream == gGui->stream) {
      xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;
      char                  buffer[1024];
      
      memset(&buffer, 0, sizeof(buffer));
      sprintf(buffer, "%s [%d%%]\n", pevent->description, pevent->percent);
      gGui->mrl_overrided = 3;
      panel_set_title(buffer);
      osd_display_info(buffer);
    }
    break;

  case XINE_EVENT_MRL_REFERENCE:
    if((event->stream == gGui->stream) && gGui->playlist.num) {
      xine_mrl_reference_data_t *ref = (xine_mrl_reference_data_t *) event->data;
      
      if(ref->alternative == 0) {
	mediamark_t *mmk = mediamark_clone_mmk((mediamark_t *) mediamark_get_current_mmk());
	
	if(mmk) {
	  mediamark_replace_entry(&gGui->playlist.mmk[gGui->playlist.cur],
				  ref->mrl, 
				  ((mmk->ident && (strcmp(mmk->mrl, mmk->ident) == 0)) ?
				   ref->mrl : mmk->ident), 
				  mmk->sub, mmk->start, mmk->end);
	  mediamark_free_mmk(&mmk);
	  
	  mmk = (mediamark_t *) mediamark_get_current_mmk();
	  gui_set_current_mrl(mmk);
	}
      }
      else {
	mediamark_add_entry(ref->mrl, ref->mrl, NULL, 0, -1);
      }
      
      playlist_update_playlist();
      panel_update_mrl_display();

      /* We can't do anything more here, otherwise deadlock occur */
      gGui->got_reference_stream++;
    }
    break;
  }
}
  
/*
 *
 */
int main(int argc, char *argv[]) {
  /* command line options will end up in these variables: */
  int                     c = '?', aos = 0;
  int                     option_index = 0;
  int                     audio_channel = -1;
  int                     spu_channel = -1;
  char                   *audio_driver_id = NULL;
  char                   *video_driver_id = NULL;
  sigset_t                vo_mask;
  char                  **_argv;
  int                     _argc;
  int                     driver_num;
  int                     session = -1;
  char                   *session_mrl = NULL;

#ifdef HAVE_SETLOCALE
  if((xitk_set_locale()) != NULL)
    setlocale(LC_ALL, "");
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
  if (sigprocmask (SIG_BLOCK,  &vo_mask, NULL)) {
    printf (_("sigprocmask() failed.\n"));
  }

  gGui = (gGui_t *) xine_xmalloc(sizeof(gGui_t));
  
  gGui->stream                 = NULL;
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
  gGui->playlist.loop          = PLAYLIST_LOOP_NO_LOOP;
  gGui->playlist.on_start      = NULL;
  gGui->skin_server_url        = NULL;

  window_attribute.x     = window_attribute.y      = -8192;
  window_attribute.width = window_attribute.height = -1;
  window_attribute.borderless = 0;

  _argv = build_command_line_args(argc, argv, &_argc);
#ifdef TRACE_RC
  _rc_file_check_args(_argc, _argv);
#endif

  /*
   * initialize CORBA server
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_init (&_argc, _argv);
#endif

  visual_anim_init();

  /*
   * Parse X Ressource database
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
      gGui->actions_on_start[aos++] = ACTID_PLAY;
      if(optarg != NULL) {
	if(strrchr(optarg, 'f')) {
	  gGui->actions_on_start[aos++] = ACTID_TOGGLE_FULLSCREEN;
	}
#ifdef HAVE_XINERAMA
	if(strrchr(optarg, 'F')) {
	  gGui->actions_on_start[aos++] = ACTID_TOGGLE_XINERAMA_FULLSCR;
	}
#endif
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
#ifdef HAVE_XINERAMA
    case 'F': /* xinerama full screen mode on start */
      gGui->actions_on_start[aos++] = ACTID_TOGGLE_XINERAMA_FULLSCR;
      break;
#endif
    case 's': /* autoscan on start */
      gGui->autoscan_plugin = xine_chomp(optarg);
      break;
       
    case OPTION_VISUAL:
      if(optarg != NULL) {
	if(!parse_visual(&gGui->prefered_visual_id, &gGui->prefered_visual_class, optarg)) {
	  show_usage();
	  exit(1);
	}
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
      if(optarg != NULL) {
	if(!parse_geometry(&window_attribute, optarg)) {
	  printf(_("Bad geometry '%s', see xine --help\n"), optarg);
	  exit(1);
	}
      }
      break;

    case 'B':
      window_attribute.borderless = 1;
      break;

    case 'N':
      visual_anim_add_animation(optarg);
      break;

    case 'P':
      gGui->actions_on_start[aos++] = ACTID_PLAYLIST;
      gGui->playlist.on_start = strdup(optarg);
      break;

    case 'l':
      if(optarg != NULL) {
	char *p = xine_chomp(optarg);

	if(!strcasecmp(p, "loop"))
	  gGui->playlist.loop = PLAYLIST_LOOP_LOOP;
	else if(!strcasecmp(p, "repeat"))
	  gGui->playlist.loop = PLAYLIST_LOOP_REPEAT;
	else if(!strcasecmp(p, "shuffle+"))
	  gGui->playlist.loop = PLAYLIST_LOOP_SHUF_PLUS;
	else if(!strcasecmp(p, "shuffle"))
	  gGui->playlist.loop = PLAYLIST_LOOP_SHUFFLE;
	else {
	  printf(_("Bad loop mode '%s', see xine --help\n"), optarg);
	  exit(1);
	}
      }
      else
	gGui->playlist.loop = PLAYLIST_LOOP_LOOP;
      break;


    case OPTION_SK_SERVER:
      gGui->skin_server_url = strdup(optarg);
      break;

    case OPTION_ENQUEUE:
      {
	char eqmrl[strlen(optarg) + 256];

	session_mrl = strdup(optarg);

	if(session >= 0) 
	  sprintf(eqmrl, "session=%d,mrl=%s", session, optarg);
	else
	  sprintf(eqmrl, "mrl=%s", optarg);
	session_handle_subopt(eqmrl, &session);
      }
      break;

    case 'S':
      session_handle_subopt(optarg, &session);
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
  
  if(session >= 0) {
    /* Session feature was used, say good bye */
    if(is_remote_running(session))
      return 0;
    else {
      /* xine isn't running, create instance */
      if(session_mrl) {
	_argv = (char **) realloc(_argv, sizeof(char **) * ((_argc) + 2));
	_argv[_argc] = session_mrl;
	_argc++;
      }
      else
	return 0;
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
   * Initialize config
   */
  {
    char *cfgdir = ".xine";
    char *cfgfile = CONFIGFILE;
    struct stat st;
    
    if (!(gGui->configfile = getenv ("XINERC"))) {
      gGui->configfile = (char *) xine_xmalloc(strlen(xine_get_homedir())
					       + strlen(cfgdir) 
					       + strlen(cfgfile)
					       + 3);
      sprintf (gGui->configfile, "%s/%s", xine_get_homedir(), cfgdir);
      mkdir (gGui->configfile, 0755);
      sprintf (gGui->configfile + strlen(gGui->configfile), "/%s", cfgfile);
    }

    /* Popup setup window if there is no config file */
    if(stat(gGui->configfile, &st) < 0)
      gGui->actions_on_start[aos++] = ACTID_SETUP;

  }

  gGui->xine = xine_new();
  xine_config_load(gGui->xine, gGui->configfile);
  /*
   * init gui
   */
  gui_init(_argc - optind, &_argv[optind], &window_attribute);

  pthread_mutex_init(&gGui->download_mutex, NULL);
  
  /* Automatically start playback if new_mode is enabled and playlist is filled */
  if(gGui->smart_mode && 
     (gGui->playlist.num || actions_on_start(gGui->actions_on_start, ACTID_PLAYLIST)) &&
     (!(actions_on_start(gGui->actions_on_start, ACTID_PLAY))))
    gGui->actions_on_start[aos++] = ACTID_PLAY;
  
  /*
   * xine init
   */
  xine_init(gGui->xine);

  /*
   * load and init output drivers
   */
  /* Video out plugin */
  driver_num = -1;
  {
    const char *const *vids = xine_list_video_output_plugins(gGui->xine);
    int                i = 0;
    
    while(vids[i++]);
    
    video_driver_ids = (char **) xine_xmalloc(sizeof(char *) * (i + 1));
    i = 0;
    video_driver_ids[i] = strdup("auto");
    while(vids[i]) {
      video_driver_ids[i + 1] = strdup(vids[i]);
      i++;
    }
    
    video_driver_ids[i + 1] = NULL;
    
    if(video_driver_id) {
      for(i = 0; video_driver_ids[i] != NULL; i++) {
	if(!strcasecmp(video_driver_id, video_driver_ids[i])) {
	  driver_num = i;
	  break;
	}
      }
    }
    gGui->vo_port = load_video_out_driver(driver_num);
  }  
  
  {
    xine_cfg_entry_t  cfg_vo_entry;
    
    if(xine_config_lookup_entry(gGui->xine, "video.driver", &cfg_vo_entry)) {

      if(!strcasecmp(video_driver_ids[cfg_vo_entry.num_value], "dxr3")) {
	xine_cfg_entry_t  cfg_entry;
	
	if(xine_config_lookup_entry(gGui->xine, "dxr3.videoout_mode", &cfg_entry)) {
	  if(((!strcmp(cfg_entry.enum_values[cfg_entry.num_value], "letterboxed tv")) ||
	      (!strcmp(cfg_entry.enum_values[cfg_entry.num_value], "widescreen tv"))) && 
	     (!(actions_on_start(gGui->actions_on_start, ACTID_TOGGLE_WINOUT_VISIBLITY)))) {
	    gGui->actions_on_start[aos++] = ACTID_TOGGLE_WINOUT_VISIBLITY;
	  }
	}
      }
    }
  }
  SAFE_FREE(video_driver_id);

  /* Audio out plugin */
  driver_num = -1;
  {
    const char *const *aids = xine_list_audio_output_plugins(gGui->xine);
    int                i = 0;
    
    while(aids[i++]);
    
    audio_driver_ids = (char **) xine_xmalloc(sizeof(char *) * (i + 2));
    i = 0;
    audio_driver_ids[i] = strdup("auto");
    audio_driver_ids[i + 1] = strdup("null");
    while(aids[i]) {
      audio_driver_ids[i + 2] = strdup(aids[i]);
      i++;
    }
    
    audio_driver_ids[i + 2] = NULL;
    
    if(audio_driver_id) {
      for(i = 0; audio_driver_ids[i] != NULL; i++) {
	if(!strcasecmp(audio_driver_id, audio_driver_ids[i])) {
	  driver_num = i;
	  break;
	}
      }
    }
    gGui->ao_port = load_audio_out_driver(driver_num);
  }
  SAFE_FREE(audio_driver_id);
  post_init();

  gGui->stream = xine_stream_new(gGui->xine, gGui->ao_port, gGui->vo_port);
  gGui->spu_stream = xine_stream_new(gGui->xine, NULL, gGui->vo_port);

  osd_init();

  /*
   * Setup logo.
   */
  gGui->logo_mode = 0;
  gGui->logo_has_changed = 0;
  gGui->logo_mrl = xine_config_register_string (gGui->xine, "gui.logo_mrl", XINE_LOGO_MRL,
						_("Logo mrl"),
						CONFIG_NO_HELP, 
						CONFIG_LEVEL_EXP,
						main_change_logo_cb, 
						CONFIG_NO_DATA);

  gGui->event_queue = xine_event_new_queue(gGui->stream);
  xine_event_create_listener_thread(gGui->event_queue, event_listener, NULL);

  xine_tvmode_init(gGui->xine);
  
  xine_set_param(gGui->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, audio_channel);
  xine_set_param(gGui->stream, XINE_PARAM_SPU_CHANNEL, spu_channel);

  /* Visual animation stream init */
  gGui->visual_anim.stream = xine_stream_new(gGui->xine, NULL, gGui->vo_port);
  gGui->visual_anim.event_queue = xine_event_new_queue(gGui->visual_anim.stream);
  gGui->visual_anim.current = 0;
  xine_event_create_listener_thread(gGui->visual_anim.event_queue, event_listener, NULL);
  xine_set_param(gGui->visual_anim.stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -2);
  xine_set_param(gGui->visual_anim.stream, XINE_PARAM_SPU_CHANNEL, -2);

  /* Playlist scanning feature stream */
  gGui->playlist.scan_stream = xine_stream_new(gGui->xine, gGui->ao_port, gGui->vo_port);
  xine_set_param(gGui->playlist.scan_stream, XINE_PARAM_SPU_CHANNEL, -2);
  
  /* init the video window */
  video_window_select_visual ();
  
  /*
   * start CORBA server threadq
   */
#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_start (gGui->xine);
#endif

  /*
   * hand control over to gui
   */
  gGui->actions_on_start[aos] = ACTID_NOKEY;
  
  gui_run();

#ifdef HAVE_ORBIT
  if (!no_lirc)
    xine_server_exit(gGui->xine);
#endif

  return 0;
}
