/* 
 * Copyright (C) 2003 by Fredrik Noring
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
 * Initial version by Fredrik Noring, January 2003, based on the xitk
 * and dfb sources.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "getopt.h"
#endif

#include "main.h"

#define OPTION_STDCTL           5000
int stdctl;

void extract_mrls(int num_mrls, char **mrls)
{
	int i;
	
	for(i = 0; i < num_mrls; i++)
		fbxine.mrl[i] = mrls[i];
	
	fbxine.num_mrls = num_mrls;
	fbxine.current_mrl = 0;
}

static void print_version(void)
{
	printf("fbxine %s is a frame buffer interface for the xine library.\n",
	       VERSION);
}

static void print_banner(void)
{
	int major, minor, sub;
	
	print_version();
	xine_get_version(&major, &minor, &sub);
	printf("Built with xine library %d.%d.%d. "
	       "Found xine library version: %d.%d.%d.\n", 
	       XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION,
	       major, minor, sub);
}

static void print_usage(void)
{
	const char *const *driver_id;
	
	printf("Usage: fbxine [options] <MRL> ...\n"
	       "\n"
	       "  -v, --version                  Display version.\n"
	       "  -V, --video-driver <drv>       Select video driver:\n");
	
	driver_id = xine_list_video_output_plugins(fbxine.xine);
	while(*driver_id)
		printf("                                   '%s'\n",
		       *driver_id++);
	
	printf("\n"
	       "  -A, --audio-driver <drv>       Select audio driver:\n");
	
	driver_id = xine_list_audio_output_plugins(fbxine.xine);
	while(*driver_id)
		printf("                                    '%s'\n",
		       *driver_id++);
	
	printf("\n"
	       "  -a, --audio-channel <number>   Select audio channel.\n"
	       "  -R, --recognize-by [option]    Try to recognize stream type. Option are:\n"
	       "                                 'default': by content, then by extension,\n"
	       "                                 'revert': by extension, then by content,\n"
	       "                                 'content': only by content,\n"
	       "                                 'extension': only by extension.\n"
	       "                                 -if no option is given, 'revert' is selected\n"
	       "  -d, --debug                    Print debug messages.\n"
	       "\n"
	       "Examples of valid MRL:s (media resource locators):\n"
	       "  File:  'path/foo.vob'\n"
	       "         '/path/foo.vob'\n"
	       "         'file://path/foo.vob'\n"
	       "         'fifo://[[mpeg1|mpeg2]:/]path/foo'\n"
	       "         'stdin://[mpeg1|mpeg2]' or '-' (mpeg2 only)\n"
	       "  DVD:   'dvd://VTS_01_2.VOB'\n"
	       "  VCD:   'vcd://<track number>'\n"
	       "\n");

	print_banner();
}

int parse_options(int argc, char **argv)
{
	const struct option long_options[] =
		{
			{ "help",          no_argument,       0, 'h' },
			{ "debug",         no_argument,       0, 'd' },
			{ "audio-channel", required_argument, 0, 'a' },
			{ "audio-driver",  required_argument, 0, 'A' },
			{ "video-driver",  required_argument, 0, 'V' },
			{ "version",       no_argument,       0, 'v' },
			{ "stdctl",        optional_argument, 0, OPTION_STDCTL },
			{ 0,               no_argument,       0,  0  }
		};
	const char *short_options = "?hda:qA:V:R::v";
	int c = '?', option_index = 0;
  
	opterr = 0;
	while((c = getopt_long(argc, argv, short_options, 
			       long_options, &option_index)) != EOF)
	{
		switch(c) {
			
			case 'a':
				sscanf(optarg, "%i", &fbxine.audio_channel);
				break;
				
			case 'd':
				fbxine.debug = 1;
				break;
				
			case 'A':
				if(!optarg)
				{
					fprintf(stderr, "Audio driver id is "
						"required for -A option.\n");
					return -1;
				}
				
				fbxine.audio_port_id = strdup(optarg);
				break;

			case 'V':
				if(!optarg)
				{
					fprintf(stderr, "Video driver id is "
						"required for -V option.\n");
					return -1;
				}
				
				fbxine.video_port_id = strdup(optarg);
				break;
				
			case 'v':
				print_version();
				return 0;
				
		        case OPTION_STDCTL:
			        stdctl  = 1;
				break;

			case 'h':
			case '?':
				print_usage();
				return 0;
				
			default:
				print_usage();
				fprintf (stderr, "Invalid argument %d.\n", c);
				return -1;
		}
	}
	
	if(argc - optind == 0)
	{
		fprintf(stderr, "You should specify at least one MRL.\n");
		return -1;
	}
	
	extract_mrls((argc - optind), &argv[optind]);
	
	return 1;
}
