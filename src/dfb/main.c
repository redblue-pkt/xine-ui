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
 * xine main for DirectFB
 * Rich Wareham <richwareham@users.sourceforge.net>
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
#include <directfb.h>       

#include <xine.h>
#include <xine/xineutils.h>

#define FONT FONTDIR "/cetus.ttf"
#define POINTER FONTDIR "/pointer2.png"

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif
#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }

/*
 * global variables
 */
typedef struct {
  xine_t           *xine;
  int               ignore_status;
  vo_driver_t      *vo_driver;
  config_values_t  *config;
  ao_driver_t      *ao_driver;
  char             *mrl[1024];
  int               num_mrls;
  int               current_mrl;
  int               running;
  int               auto_quit;

  IDirectFB        *dfb;
  IDirectFBDisplayLayer  *layer;
  IDirectFBSurface *primary;
  IDirectFBSurface *bg_surface;
  IDirectFBWindow  *main_window;
  IDirectFBSurface *main_surface;
  IDirectFBSurface *pointer;

  IDirectFBFont    *font;
  int               fontheight;
  DFBDisplayLayerConfig  layer_config;
 
  int               screen_width;
  int               screen_height;

  struct {
    int                enable;
    int                caps;
    int                volume_mixer;
    int                volume_level;
    int                mute;
  } mixer;

  int               debug_messages;
} dfbxine_t;

static dfbxine_t dfbxine;

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

  printf("usage: dfbxine [aalib-options] [dfbxine-options] mrl ...\n");
  printf("\n");
  printf("  -v, --version                Display version.\n");
  printf("DFBXINE options:\n");
  printf("  -q, --auto-quit              Quit after playing all mrl's.\n");
  printf("  -V, --video-driver <drv>     Select video driver by id. Available drivers: \n");
  printf("                               ");
  driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_DFB);
  driver_id  = *driver_ids++;
  while (driver_id) {
    printf ("%s ", driver_id);
    driver_id  = *driver_ids++;
  }
  printf ("\n");
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
}

/*
 * Handling xine engine status changes.
 */
static void gui_status_callback (int nStatus) {

  if (dfbxine.ignore_status)
    return;
  
  if(nStatus == XINE_STOP) {
    dfbxine.current_mrl++;
   
    if (dfbxine.current_mrl < dfbxine.num_mrls)
      xine_play (dfbxine.xine, dfbxine.mrl[dfbxine.current_mrl], 0, 0 );
    else {
      if (dfbxine.auto_quit == 1) {
        dfbxine.running = 0;
      }
      dfbxine.current_mrl--;
    }
  }

}

/*
 * Return next available mrl to xine engine.
 */
static char *gui_next_mrl_callback (void ) {

  if(dfbxine.current_mrl >= (dfbxine.num_mrls - 1)) 
    return NULL;
  
  return dfbxine.mrl[dfbxine.current_mrl + 1];
}

/*
 * Xine engine branch success.
 */
static void gui_branched_callback (void ) {

  if(dfbxine.current_mrl < (dfbxine.num_mrls - 1)) {
    dfbxine.current_mrl++;
  }
}

/*
 * Seek in current stream.
 */
static void set_position (int pos) {

  dfbxine.ignore_status = 1;
  xine_play(dfbxine.xine, dfbxine.mrl[dfbxine.current_mrl], pos, 0);
  dfbxine.ignore_status = 0;
}

/*
 * Extract mrls from argv[] and store them to playlist.
 */
void extract_mrls(int num_mrls, char **mrls) {
  int i;

  for(i = 0; i < num_mrls; i++)
    dfbxine.mrl[i] = mrls[i];

  dfbxine.num_mrls = num_mrls;
  dfbxine.current_mrl = 0;

}

void vm_cb(int w, int h, int bpp, void* cb_data) {
  printf("Mode %ix%i @ %i bpp supported\n", w,h,bpp);
}

typedef struct {
  IDirectFB *dfb;
  IDirectFBSurface *primary;
} dfb_visual_info_t;

int main(int argc, char *argv[]) {
  int            c = '?';
  int            option_index    = 0;
  char          *configfile;
  char          *audio_driver_id = NULL;
  char          *video_driver_id = NULL;
  int            audio_channel   = 0;
  char          *driver_name;
  dfb_visual_info_t visual_info;
  DFBCardCapabilities    caps;
  IDirectFBInputBuffer *input_buf;
  IDirectFBInputDevice *keyboard = NULL;
  int window_grabbed = 0;
  int window_size = 0;
  int startx = 0;
  int starty = 0;
  int endx = 0;
  int endy = 0;

  /*
   * Check xine library version 
   */
  if(!xine_check_version(0, 9, 9)) {
    fprintf(stderr, "require xine library version 0.9.9, found %d.%d.%d.\n", 
	    xine_get_major_version(), xine_get_minor_version(),
	    xine_get_sub_version());
    goto failure;
  }

  dfbxine.num_mrls = 0;
  dfbxine.current_mrl = 0;
  dfbxine.auto_quit = 0; /* default: loop forever */
  dfbxine.debug_messages = 0;
  dfbxine.dfb = NULL;
  dfbxine.primary = NULL;

  /*
   * Initialize DirectFB and parse command line.
   */
  DFBCHECK (DirectFBInit (&argc, &argv));

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
      sscanf(optarg, "%i", &audio_channel);
      break;
      
    case 'q': /* Automatic quit option */
      dfbxine.auto_quit = 1;
      break;

    case 'A': /* Select audio driver */
      if(optarg != NULL) {
	audio_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strcpy (audio_driver_id, optarg);
      } else {
	fprintf (stderr, "audio driver id required for -A option\n");
	exit (1);
      }
      break;

    case 'V': /* Select video driver */
      if(optarg != NULL) {
	video_driver_id = xine_xmalloc (strlen (optarg) + 1);
	strcpy (video_driver_id, optarg);
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
    goto failure;
  }
  else
    extract_mrls((argc - optind), &argv[optind]);
  
  show_banner();

  /*
   * generate and init a config "object"
   */
  {
    char *cfgfile = ".xine/config";
    
    if (!(configfile = getenv("XINERC"))) {
      configfile = (char *) xine_xmalloc((strlen((xine_get_homedir())) + strlen(cfgfile))+2);
      sprintf(configfile, "%s/%s", (xine_get_homedir()), cfgfile);
    }
      
  }

  dfbxine.config = config_file_init (configfile);

  /*
   * Initialise primary surface
   */
  DFBCHECK(DirectFBCreate(&(dfbxine.dfb)));
  DFBCHECK(dfbxine.dfb->SetCooperativeLevel(dfbxine.dfb, 
					    DFSCL_NORMAL));
  DFBCHECK(dfbxine.dfb->EnumVideoModes(dfbxine.dfb, vm_cb, NULL));
   
  dfbxine.dfb->GetCardCapabilities( dfbxine.dfb, &caps );
  dfbxine.dfb->GetDisplayLayer( dfbxine.dfb, DLID_PRIMARY, &(dfbxine.layer) );
  dfbxine.layer->SetCooperativeLevel(dfbxine.layer, DLSCL_EXCLUSIVE);

 if (!((caps.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
       (caps.blitting_flags & DSBLIT_BLEND_COLORALPHA  )))
  {
   dfbxine.layer_config.flags = DLCONF_BUFFERMODE;
   dfbxine.layer_config.buffermode = DLBM_BACKSYSTEM;
   
   dfbxine.layer->SetConfiguration( dfbxine.layer, &(dfbxine.layer_config) );
  }

 dfbxine.layer->GetConfiguration( dfbxine.layer, &(dfbxine.layer_config) );
 /* dfbxine.layer->EnableCursor ( dfbxine.layer, 0 ); */
 dfbxine.layer->EnableCursor ( dfbxine.layer, 1 );

  {
   DFBSurfaceDescription dsc;
   IDirectFBImageProvider *provider;

   DFBCHECK(dfbxine.dfb->CreateImageProvider(dfbxine.dfb, POINTER,
					     &provider));
   DFBCHECK (provider->GetSurfaceDescription (provider, &dsc));
   DFBCHECK(dfbxine.dfb->CreateSurface(dfbxine.dfb, &dsc, &(dfbxine.pointer)));
   DFBCHECK(provider->RenderTo(provider, dfbxine.pointer));
   provider->Release(provider);
  }
 dfbxine.layer->SetCursorShape(dfbxine.layer, dfbxine.pointer, 1,1);

  {
   DFBFontDescription desc;
   
   desc.flags = DFDESC_HEIGHT;
   desc.height = 20;
   
   DFBCHECK(dfbxine.dfb->CreateFont( dfbxine.dfb, FONT, &desc, 
				     &(dfbxine.font) ));
   dfbxine.font->GetHeight( dfbxine.font, &(dfbxine.fontheight) );
  }

  {
   DFBSurfaceDescription desc;
   
   desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
   desc.width = dfbxine.layer_config.width;
   desc.height = dfbxine.layer_config.height;

   DFBCHECK(dfbxine.dfb->CreateSurface(dfbxine.dfb, &desc, 
				       &(dfbxine.primary)));
  }

 DFBCHECK(dfbxine.primary->GetSize(dfbxine.primary, &(dfbxine.screen_width),
				   &(dfbxine.screen_height)));
 DFBCHECK(dfbxine.primary->SetColor(dfbxine.primary, 0x5F, 0x5F, 0x5C, 0xFF));
 DFBCHECK(dfbxine.primary->FillRectangle(dfbxine.primary, 0,0, 
					 dfbxine.screen_width,
					 dfbxine.screen_height));
 DFBCHECK(dfbxine.primary->SetFont(dfbxine.primary, dfbxine.font));
 DFBCHECK(dfbxine.primary->SetColor(dfbxine.primary, 0xCF, 0xCF, 0xFF, 0xFF));

  {int i;
   for(i=5; i<dfbxine.screen_height-30; i+=25) {
     DFBCHECK(dfbxine.primary->DrawString(dfbxine.primary,
					  "This is the DirectFB output mode "
					  "for Xine (EXPERIMENTAL)",
					  -1,5,i, DSTF_LEFT | DSTF_TOP));
   }
  }
 DFBCHECK(dfbxine.layer->SetBackgroundImage(dfbxine.layer,dfbxine.primary));
 DFBCHECK(dfbxine.layer->SetBackgroundMode(dfbxine.layer, DLBM_IMAGE));
	  
  {
   DFBWindowDescription desc;
   
   desc.flags = ( DWDESC_POSX | DWDESC_POSY |
		  DWDESC_WIDTH | DWDESC_HEIGHT );

   desc.posx = 5;
   desc.posy = 5;
   desc.width = dfbxine.layer_config.width/2;
   desc.height = dfbxine.layer_config.height/2;

   DFBCHECK( dfbxine.layer->CreateWindow( dfbxine.layer, &desc, 
					  &(dfbxine.main_window) ) );
   dfbxine.main_window->GetSurface( dfbxine.main_window, 
				    &(dfbxine.main_surface) );
   dfbxine.main_window->SetOpacity(dfbxine.main_window, 0x9f);

   dfbxine.main_surface->SetColor( dfbxine.main_surface,
			      0x00, 0x30, 0x10, 0xc0 );
   dfbxine.main_surface->DrawRectangle( dfbxine.main_surface, 0, 0,
     				   desc.width, desc.height );
   dfbxine.main_surface->SetColor( dfbxine.main_surface,
			      0x80, 0xa0, 0x00, 0x90 );
   dfbxine.main_surface->FillRectangle( dfbxine.main_surface, 1, 1,
				   desc.width-2, desc.height-2 );
   
   dfbxine.main_surface->Flip(dfbxine.main_surface, NULL, 0);
  }
 dfbxine.main_window->RequestFocus(dfbxine.main_window);
 dfbxine.main_window->RaiseToTop(dfbxine.main_window);

 /*
  * init video output driver
  */
 if(!video_driver_id)
  video_driver_id = "directfb";
 
 visual_info.dfb = dfbxine.dfb;
 visual_info.primary = dfbxine.main_surface;
 dfbxine.vo_driver = xine_load_video_output_plugin(dfbxine.config, 
						   video_driver_id,
						   VISUAL_TYPE_DFB, 
						   (void *)(&visual_info));

  if (!dfbxine.vo_driver) {
    printf ("main: video driver failed\n");
    goto failure;
  }

  /*
   * init audio output driver
   */
  driver_name = dfbxine.config->register_string (dfbxine.config, "audio.driver", "oss",
						"audio driver to use",
						NULL, NULL, NULL);
  if(!audio_driver_id)
    audio_driver_id = driver_name;
  else
    dfbxine.config->update_string (dfbxine.config, "audio.driver", audio_driver_id);
  
  dfbxine.ao_driver = xine_load_audio_output_plugin(dfbxine.config, audio_driver_id);

  if (!dfbxine.ao_driver) {
    printf ("main: audio driver %s failed\n", audio_driver_id);
  }
 
  /*
   * xine init
   */
  dfbxine.xine = xine_init (dfbxine.vo_driver,
			   dfbxine.ao_driver, 
			   dfbxine.config);
 
  if(!dfbxine.xine) {
    fprintf(stderr, "xine_init() failed.\n");
    goto failure;
  }

  /* Init mixer control */
  dfbxine.mixer.enable = 0;
  dfbxine.mixer.caps = xine_get_audio_capabilities(dfbxine.xine);

  if(dfbxine.mixer.caps & AO_CAP_PCM_VOL)
    dfbxine.mixer.volume_mixer = AO_PROP_PCM_VOL;
  else if(dfbxine.mixer.caps & AO_CAP_MIXER_VOL)
    dfbxine.mixer.volume_mixer = AO_PROP_MIXER_VOL;
  
  if(dfbxine.mixer.caps & (AO_CAP_MIXER_VOL | AO_CAP_PCM_VOL)) { 
    dfbxine.mixer.enable = 1;
    dfbxine.mixer.volume_level = xine_get_audio_property(dfbxine.xine, dfbxine.mixer.volume_mixer);
  }
  
  if(dfbxine.mixer.caps & AO_CAP_MUTE_VOL) {
    dfbxine.mixer.mute = xine_get_audio_property(dfbxine.xine, AO_PROP_MUTE_VOL);
  }

  /* Select audio channel */
  xine_select_audio_channel (dfbxine.xine, audio_channel);
  
  /*
   * ui loop
   */

  xine_play (dfbxine.xine, dfbxine.mrl[dfbxine.current_mrl], 0, 0);
  
  dfbxine.running = 1;

  DFBCHECK (dfbxine.dfb->GetInputDevice (dfbxine.dfb,
					 DIDID_KEYBOARD, &keyboard));
  DFBCHECK (keyboard->CreateInputBuffer (keyboard, &input_buf));

  while (dfbxine.running) {
    DFBWindowEvent evt;
    DFBInputEvent event;

    input_buf->WaitForEventWithTimeout(input_buf, 0, 10000000);
    
    dfbxine.main_window->WaitForEventWithTimeout(dfbxine.main_window,
						 0, 10000000);
    
    while (dfbxine.main_window->GetEvent( dfbxine.main_window,
					  &evt ) == DFB_OK) {
      switch (evt.type) {
       case DWET_BUTTONDOWN: 
	if(!window_grabbed && evt.button == DIBI_LEFT) {
	  window_grabbed = 1;
	  dfbxine.layer->GetCursorPosition(dfbxine.layer, &startx,
					   &starty);
	  dfbxine.main_window->GrabPointer(dfbxine.main_window);
	}
	if(!window_size && evt.button == DIBI_MIDDLE) {
	  int w,h;
	  
	  window_size = 1;
	  dfbxine.layer->GetCursorPosition(dfbxine.layer, &startx,
					   &starty);
	  dfbxine.main_window->GrabPointer(dfbxine.main_window);
	  dfbxine.main_window->GetSize(dfbxine.main_window, &w, &h);
	  startx -= w;
	  starty -= h;
	}
	break;
       case DWET_BUTTONUP:
	if(evt.button == DIBI_LEFT) {
	  dfbxine.main_window->UngrabPointer(dfbxine.main_window);
	  window_grabbed = 0;
	}
	if(evt.button == DIBI_MIDDLE) {
	  dfbxine.main_window->UngrabPointer(dfbxine.main_window);
	  window_size = 0;
	}
	break;
       case DWET_MOTION:
	endx = evt.cx;
	endy = evt.cy;
	break;
       default:
      }
    }

    if(window_grabbed) {
      dfbxine.main_window->Move(dfbxine.main_window,
			    	endx-startx, endy-starty);
      startx = endx;
      starty = endy; 
    }

    if(window_size) {
      dfbxine.main_window->Resize(dfbxine.main_window,
				endx-startx, endy-starty);
    }
    
    while(input_buf->GetEvent(input_buf, &event) == DFB_OK) {
      switch(event.type) {
       case DIET_KEYPRESS:
	dfbxine.running = 0;
	break;
       default:
      }
    }
  }

failure:

  if(dfbxine.config) 
    dfbxine.config->save(dfbxine.config);
  
  if(dfbxine.xine)
    xine_exit(dfbxine.xine); 

  if(dfbxine.main_surface)
   DFBCHECK(dfbxine.main_surface->Release(dfbxine.main_surface));
  if(dfbxine.main_window)
   DFBCHECK(dfbxine.main_window->Release(dfbxine.main_window));
  if(dfbxine.primary)
   DFBCHECK(dfbxine.primary->Release(dfbxine.primary));
  if(dfbxine.pointer)
   DFBCHECK(dfbxine.pointer->Release(dfbxine.pointer));
  if(dfbxine.layer)
   DFBCHECK(dfbxine.layer->Release(dfbxine.layer));
  if(dfbxine.dfb) 
   DFBCHECK(dfbxine.dfb->Release(dfbxine.dfb));

  return 0;
}
