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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <alloca.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>
#include <X11/Xutil.h>
#endif

#include <xine/xineutils.h>

#include "utils.h"

extern char **environ;

/*
 * Execute a shell command.
 */
int xine_system(int dont_run_as_root, char *command) {
  int pid, status;
  
  /* 
   * Don't permit run as root
   */
  if(dont_run_as_root) {
    if(getuid() == 0)
      return -1;
  }

  if(command == 0)
    return 1;

  pid = fork();

  if(pid == -1)
    return -1;
  
  if(pid == 0) {
    char *argv[4];
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = command;
    argv[3] = 0;
    execve("/bin/sh", argv, environ);
    exit(127);
  }

  do {
    if(waitpid(pid, &status, 0) == -1) {
      if (errno != EINTR)
	return -1;
    } 
    else {
      return WEXITSTATUS(status);
    }
  } while(1);
  
  return -1;
}

/*
 * cleanup the str string, take care about '
 */
char *atoa(char *str) {
  char *pbuf;
  int   quote = 0, dblquote = 0;

  pbuf = str;
  
  while(*pbuf == ' ') 
    pbuf++;

  if(*pbuf == '\'')
    quote = 1;
  else if(*pbuf == '"')
    dblquote = 1;
  
  pbuf = str;

  while(*pbuf != '\0')
    pbuf++;
  
  if(pbuf > str)
    pbuf--;

  while((pbuf > str) && (*pbuf == '\r' || *pbuf == '\n')) {
    *pbuf = '\0';
    pbuf--;
  }

  while((pbuf > str) && (*pbuf == ' ')) {
    *pbuf = '\0';
    pbuf--;
  }
  
  if((quote && (*pbuf == '\'')) || (dblquote && (*pbuf == '"'))) {
    *pbuf = '\0';
    pbuf--;
  }
  
  pbuf = str;

  while(*pbuf == ' ' || *pbuf == '\t')
    pbuf++;
  
  if((quote && (*pbuf == '\'')) || (dblquote && (*pbuf == '"')))
    pbuf++;
  
  return pbuf;
}

/*
 *
 */
static int _mkdir_safe(char *path) {
  struct stat  pstat;
  
  if(path == NULL)
    return 0;
  
  if((lstat(path, &pstat)) < 0) {
    /* file or directory no exist, create it */
    if(mkdir(path, 0755) < 0) {
      fprintf(stderr, "mkdir(%s) failed: %s\n", path, strerror(errno));
      return 0;
    }
  }
  else {
    /* Check of found file is a directory file */
    if(!S_ISDIR(pstat.st_mode)) {
      fprintf(stderr, "%s is not a directory.\n", path);
      errno = EEXIST;
      return 0;
    }
  }

  return 1;
}

/*
 *
 */
int mkdir_safe(char *path) {
  char *p, *pp;
  char  buf[_PATH_MAX + _NAME_MAX + 1];
  char  buf2[_PATH_MAX + _NAME_MAX + 1];
  
  if(path == NULL)
    return 0;
  
  memset(&buf, 0, sizeof(buf));
  memset(&buf2, 0, sizeof(buf2));
  
  sprintf(buf, "%s", path);
  pp = buf;
  while((p = xine_strsep(&pp, "/")) != NULL) {
    if(p && strlen(p)) {
      sprintf(buf2, "%s/%s", buf2, p);
      if(!_mkdir_safe(buf2))
	return 0;
    }
  }

  return 1;
}

int get_bool_value(const char *val) {
  static struct {
    const char *str;
    int value;
  } bools[] = {
    { "1",     1 }, { "true",  1 }, { "yes",   1 }, { "on",    1 },
    { "0",     0 }, { "false", 0 }, { "no",    0 }, { "off",   0 },
    { NULL,    0 }
  };
  int i;
  
  if(val) {
    for(i = 0; bools[i].str != NULL; i++) {
      if(!(strcasecmp(bools[i].str, val)))
	return bools[i].value;
    }
  }
  
  return 0;
}

const char *get_last_double_semicolon(const char *str) {
  int len;

  if(str && (len = strlen(str))) {
    const char *p = str + (len - 1);

    while(p > str) {

      if((*p == ':') && (*(p - 1) == ':'))
	return (p - 1);
      
      p--;
    }
  }

  return NULL;
}

int is_ipv6_last_double_semicolon(const char *str) {
  
  if(str && strlen(str)) {
    const char *d_semic = get_last_double_semicolon(str);

    if(d_semic) {
      const char *bracketl = NULL;
      const char *bracketr = NULL;
      const char *p        = d_semic + 2;
      
      while(*p && !bracketr) {
	if(*p == ']')
	  bracketr = p;

	p++;
      }
      
      if(bracketr) {
	p = d_semic;

	while((p >= str) && !bracketl) {
	  if(*p == '[')
	    bracketl = p;

	  p--;
	}
	
	if(bracketl) {

	  /* Look like an IPv6 address, check it */
	  p = d_semic + 2;

	  while(*p) {
	    switch(*p) {
	    case ':':
	    case '.':
	      break;

	    case ']': /* IPv6 delimiter end */

	      /* now go back to '[' */
	      p = d_semic;
	      
	      while(p >= str) {
		switch(*p) {
		case ':':
		  break;
		  
		case '.': /* This couldn't happen */
		  return 0;
		  break;

		case '[': /* We reach the beginning, it's a valid IPv6 */
		  return 1;
		  break;
		  
		default:
		  if(!isxdigit(*p))
		    return 0;
		  break;

		}

		p--;
	      }
	      break;

	    default:
	      if(!isxdigit(*p))
		return 0;
	      break;

	    }

	    p++;
	  }
	}
      }
    }
  }

  return 0;
}

inline int is_a_dir(char *filename) {
  struct stat  pstat;
  
  if((stat(filename, &pstat)) < 0)
    return 0;
  
  return (S_ISDIR(pstat.st_mode));
}

inline int is_a_file(char *filename) {
  struct stat  pstat;
  
  if((stat(filename, &pstat)) < 0)
    return 0;
  
  return (S_ISREG(pstat.st_mode));
}

void dump_cpu_infos(void) {
#if defined (__linux__)
  FILE *stream;
  char  buffer[2048];
  
  if((stream = fopen("/proc/cpuinfo", "r"))) {
    
    printf("   CPU Informations:\n");
    
    memset(&buffer, 0, sizeof(buffer));
    while(fread(&buffer, 1, 2047, stream)) {
      char *p, *pp;
      
      pp = buffer;
      while((p = xine_strsep(&pp, "\n"))) {
	if(p && strlen(p))
	  printf("\t%s\n", p);
      } 
      
      memset(&buffer, 0, sizeof(buffer));
    }
    
    printf("   -------\n");
    fclose(stream);
  }
  else
    printf("   Unable to open '/proc/cpuinfo': '%s'.\n", strerror(errno));
  
#else
  printf("   CPU informations unavailable.\n"); 
#endif
}

#ifdef HAVE_X11
/*
 * Code hardly based on xdyinfo.c, part of XFree86 
 * Copyright 1988, 1998  The Open Group
 * Author:  Jim Fulton, MIT X Consortium
 *
 * and xvinfo.c (no informations found)
 * 
 */
static int StrCmp(const void *a, const  void *b) {
  return strcmp(*(char **)a, *(char **)b);
}
void dump_xfree_info(Display *display, int screen, int complete) {
  char                 dummybuf[40];
  char                *cp;
  int                  i, n;
  long                 req_size;
  XPixmapFormatValues *pmf;
  Window               focuswin;
  int                  focusrevert, scr;
  char               **extlist;
  int                  nscreens, have_xv = 0;
  unsigned int         ver, rev, eventB, reqB, errorB; 

  nscreens = XScreenCount(display);
  
  printf("   Display Name:          %s,\n", DisplayString (display));
  printf("   XServer Vendor:        %s,\n", XServerVendor(display));
  printf("   Protocol Version:      %d, Revision: %d,\n", XProtocolVersion(display), XProtocolRevision(display));
  printf("   Available Screen(s):   %d,\n", nscreens);
  printf("   Default screen number: %d,\n", DefaultScreen (display));
  printf("   Using screen:          %d,\n", screen);
  printf("   Depth:                 %d,\n", XDisplayPlanes(display, screen));
  
#ifdef HAVE_SHM
  if(XShmQueryExtension(display)) {
    int major, minor, ignore;
    Bool       pixmaps;
    
    if(XQueryExtension(display, "MIT-SHM", &ignore, &ignore, &ignore)) {
      if(XShmQueryVersion(display, &major, &minor, &pixmaps ) == True) {
	printf("   XShmQueryVersion:      %d.%d,\n", major, minor);
      }
    }
  }
#endif
  
  if(complete) {

    if (strstr(XServerVendor (display), "XFree86")) {
      int vendrel = XVendorRelease(display);
      
      printf("   XFree86 version: ");
      if (vendrel < 336) {
	/*
	 * vendrel was set incorrectly for 3.3.4 and 3.3.5, so handle
	 * those cases here.
	 */
	printf("      %d.%d.%d", vendrel / 100, (vendrel / 10) % 10, vendrel % 10);
      } 
      else if (vendrel < 3900) {
	/* 3.3.x versions, other than the exceptions handled above */
	printf("      %d.%d", vendrel / 1000, (vendrel /  100) % 10);
	if (((vendrel / 10) % 10) || (vendrel % 10)) {
	  printf(".%d", (vendrel / 10) % 10);
	  if (vendrel % 10) {
	    printf(".%d", vendrel % 10);
	  }
	}
      } else if (vendrel < 40000000) {
	/* 4.0.x versions */
	printf("      %d.%d", vendrel / 1000, (vendrel /   10) % 10);
	if (vendrel % 10) {
	  printf(".%d", vendrel % 10);
	}
      } else {
	/* post-4.0.x */
	printf("      %d.%d.%d", vendrel / 10000000, (vendrel /   100000) % 100, (vendrel /     1000) % 100);
	if (vendrel % 1000) {
	  printf(".%d", vendrel % 1000);
	}
      }
      printf("\n");
    }
    
    req_size = XExtendedMaxRequestSize (display);
    if (!req_size)
      req_size = XMaxRequestSize (display);
    printf("   Maximum request size:  %ld bytes,\n", req_size * 4);
    printf("   Motion buffer size:    %ld,\n", XDisplayMotionBufferSize (display));
    
    switch (BitmapBitOrder (display)) {
    case LSBFirst:
      cp = "LSBFirst"; 
      break;
    case MSBFirst:
      cp = "MSBFirst"; 
      break;
    default:    
      snprintf(dummybuf, 40, "%s %d", "unknown order", BitmapBitOrder (display));
      cp = dummybuf;
      break;
    }
    printf("   Bitmap unit:           %d,\n", BitmapUnit(display));
    printf("     Bit order:           %s,\n", cp);
    printf("     Padding:             %d,\n", BitmapPad(display));
    
    switch (ImageByteOrder (display)) {
    case LSBFirst:
      cp = "LSBFirst";
      break;
    case MSBFirst:
      cp = "MSBFirst";
      break;
    default:    
      snprintf (dummybuf, 40, "%s %d", "unknown order", ImageByteOrder (display));
      cp = dummybuf;
      break;
    }
    
    printf("   Image byte order:      %s,\n", cp);
    
    pmf = XListPixmapFormats (display, &n);
    printf("   Number of supported pixmap formats: %d,\n", n);
    if (pmf) {
      printf("   Supported pixmap formats:\n");
      printf("     Depth        Bits_per_pixel        Scanline_pad\n");
      for (i = 0; i < n; i++) {
	printf("     %5d                 %5d               %5d\n", 
		 pmf[i].depth, pmf[i].bits_per_pixel, pmf[i].scanline_pad);
      }
      printf("     -----------------------------------------------\n\n");
      XFree ((char *) pmf);
    }
    
    /*
     * when we get interfaces to the PixmapFormat stuff, insert code here
     */
    
    XGetInputFocus (display, &focuswin, &focusrevert);
    printf("   Focus:                  ");
    switch (focuswin) {
    case PointerRoot:
      printf("PointerRoot\n");
      break;
    case None:
      printf("None\n");
      break;
    default:
      printf("Window 0x%lx, revert to ", focuswin);
      switch (focusrevert) {
      case RevertToParent:
	printf("Parent,\n");
	break;
      case RevertToNone:
	printf("None,\n");
	break;
      case RevertToPointerRoot:
	printf("PointerRoot,\n");
	
	break;
      default:			/* should not happen */
	printf("%d,\n", focusrevert);
	break;
      }
      break;
    }
    
    extlist = XListExtensions (display, &n);
    printf("   Number of extensions:   %d\n", n);
    if (extlist) {
      int   i, opcode, event, error;
      int   j, maxlen = 0;
      
      for(j = 0; j < n; j++) {
	if(maxlen < strlen(extlist[j]))
	  maxlen = strlen(extlist[j]);
      }
      maxlen += 2;
      
      qsort(extlist, n, sizeof(char *), StrCmp);
      for (i = 0; i < n; i++) {
	Bool retval;
	
	retval = XQueryExtension(display, extlist[i], &opcode, &event, &error);
	printf("     %s:", extlist[i]);
	
	for(j = strlen(extlist[i]); j < maxlen; j++)
	  printf(" ");
	
	if(retval == True) {
	  printf("[opcode: %d", opcode);
	  if(event || error) {
	    printf(", base (");
	    if (event) {
	      printf("event: %d", event);
	      if(error)
		printf(", ");
	    }
	    
	    if (error)
	      printf("error: %d", error);
	    printf(")");
	  }
	  printf("]\n");
	}
	else
	  printf("not present.\n"); 
	
      }
      /* Don't free it */
      /* XFreeExtensionList (extlist); */
    }

    if((Success != XvQueryExtension(display, &ver, &rev, &reqB, &eventB, &errorB))) {
      printf("   No X-Video Extension on %s\n", XDisplayName(NULL));
    } 
    else {
      printf("   X-Video Extension version: %i.%i\n", ver, rev);
      have_xv = 1;
    }
    
    for (scr = 0; scr < nscreens; scr++) {
      Screen      *s = ScreenOfDisplay (display, scr);  /* opaque structure */
      double       xres, yres;
      int          ndepths = 0, *depths = NULL;
      unsigned int width, height;
      
      xres = ((((double) DisplayWidth(display,scr)) * 25.4) / ((double) DisplayWidthMM(display,scr)));
      yres = ((((double) DisplayHeight(display,scr)) * 25.4) / ((double) DisplayHeightMM(display,scr)));
      
      printf("   Dimensions:             %dx%d pixels (%dx%d millimeters).\n",
	      DisplayWidth (display, scr), DisplayHeight (display, scr),
	      DisplayWidthMM(display, scr), DisplayHeightMM (display, scr));
      printf("   Resolution:             %dx%d dots per inch.\n", (int) (xres + 0.5), (int) (yres + 0.5));
      depths = XListDepths (display, scr, &ndepths);

      if (!depths)
	ndepths = 0;

      printf("   Depths (%d):             ", ndepths);
      
      for(i = 0; i < ndepths; i++) {
	printf("%d", depths[i]);
	
	if (i < ndepths - 1)
	  printf(", ");
	
      }
      printf("\n");

      if(depths)
	XFree ((char *) depths);
      
      printf("   Root window id:         0x%lx\n", RootWindow (display, scr));
      printf("   Depth of root window:   %d plane%s\n",
	     DisplayPlanes (display, scr), DisplayPlanes (display, scr) == 1 ? "" : "s");
      printf("   Number of colormaps:    min %d, max %d\n", MinCmapsOfScreen(s), MaxCmapsOfScreen(s));
      printf("   Default colormap:       0x%lx\n", DefaultColormap (display, scr));
      printf("   Default number of colormap cells:   %d\n", DisplayCells (display, scr));
      printf("   Preallocated pixels:    black %ld, white %ld\n", 
	     BlackPixel (display, scr), WhitePixel (display, scr));
      printf("   Options:                backing-store %s, save-unders %s\n",
	     (DoesBackingStore (s) == NotUseful) ? "no" : ((DoesBackingStore (s) == Always) ? "yes" : "when"),
	     DoesSaveUnders (s) ? "yes" : "no");

      XQueryBestSize (display, CursorShape, RootWindow (display, scr), 65535, 65535, &width, &height);
      printf("   Largest cursor:         ");
      if ((width == 65535) && (height == 65535))
	printf("unlimited\n");
      else
	printf("%dx%d\n", width, height);

      if(have_xv) {
	int                   n, k, adaptor; 
	unsigned int          nencode, nadaptors;
	int                   nattr, numImages;
	XvAdaptorInfo        *ainfo;
	XvAttribute          *attributes;
	XvEncodingInfo       *encodings;
	XvFormat             *format;
	XvImageFormatValues  *formats;
	
	printf("   Xv infos:\n");
	  
	  XvQueryAdaptors(display, RootWindow(display, scr), &nadaptors, &ainfo);
	  
	  if(!nadaptors) {
	    printf("     No adaptors present.\n");
	    continue;
	  } 
	  
	  for(adaptor = 0; adaptor < nadaptors; adaptor++) {
	    
	    printf("    Adaptor #%i:         \"%s\"\n", adaptor, ainfo[adaptor].name);
	    printf("       Number of ports:   %li\n", ainfo[adaptor].num_ports);
	    printf("       Port base:         %li\n", ainfo[adaptor].base_id);
	    printf("       Operations supported: ");
	    
	    switch(ainfo[adaptor].type & (XvInputMask | XvOutputMask)) {
	    case XvInputMask:
	      if(ainfo[adaptor].type & XvVideoMask) 
		printf("PutVideo ");
	      if(ainfo[adaptor].type & XvStillMask) 
		printf("PutStill ");
	      if(ainfo[adaptor].type & XvImageMask) 
		printf("PutImage ");
	      break;

	    case XvOutputMask:
	      if(ainfo[adaptor].type & XvVideoMask) 
		printf("GetVideo ");
	      if(ainfo[adaptor].type & XvStillMask) 
		printf("GetStill ");
	      break;

	    default:
	      printf("none ");
	      break;
	    }
	    printf("\n");
	    
	    format = ainfo[adaptor].formats;
	    
	    printf("     Supported visuals:\n");
	    for(k = 0; k < ainfo[adaptor].num_formats; k++, format++) {
	         printf("       - Depth %i, visualID 0x%2lx\n",
				 format->depth, format->visual_id);
	    }

	    attributes = XvQueryPortAttributes(display, ainfo[adaptor].base_id, &nattr);
	    
	    if(attributes && nattr) {
	      printf("     Number of attributes: %i\n", nattr);
	      
	      for(k = 0; k < nattr; k++)
		printf("       - %s\n", attributes[k].name);

	      XFree(attributes);
	    } else {
	      printf("     No port attributes defined.\n");
	    }
	    
	    XvQueryEncodings(display, ainfo[adaptor].base_id, &nencode, &encodings);
	    
	    if(encodings && nencode) {
	      int ImageEncodings = 0;
	      
	      for(n = 0; n < nencode; n++) {
		if(!strcmp(encodings[n].name, "XV_IMAGE"))
		  ImageEncodings++;
	      }
	      
	      if(nencode - ImageEncodings) {
		printf("     Number of encodings: %i\n", nencode - ImageEncodings);
		
		for(n = 0; n < nencode; n++) {
		  if(strcmp(encodings[n].name, "XV_IMAGE")) {
		    printf("       - encoding ID #%li: \"%s\"\n", encodings[n].encoding_id, encodings[n].name);
		    printf("         size:             %li x %li\n", encodings[n].width, encodings[n].height);
		    printf("         rate:             %f\n", 
			   (float)encodings[n].rate.numerator / (float)encodings[n].rate.denominator);
		  }
		}
	      }
	      
	      if(ImageEncodings && (ainfo[adaptor].type & XvImageMask)) {
		char imageName[5] = {0, 0, 0, 0, 0};
		
		for(n = 0; n < nencode; n++) {
		  if(!strcmp(encodings[n].name, "XV_IMAGE")) {
		    printf("     Maximum XvImage size: %li x %li\n", encodings[n].width, encodings[n].height);
		    break;
		  }
		}
		
		formats = XvListImageFormats(display, ainfo[adaptor].base_id, &numImages);
		
		printf("     Number of image formats: %i\n", numImages);
		
		for(n = 0; n < numImages; n++) {
		  memcpy(imageName, &(formats[n].id), 4);
		  printf("       - Id: 0x%x", formats[n].id);
		  if(isprint(imageName[0]) && isprint(imageName[1]) &&
		     isprint(imageName[2]) && isprint(imageName[3])) {
		    printf(" (%s):\n", imageName);
		  }
		  else
		    printf(":\n");
		  
		  printf("          Bits per pixel: %i\n", formats[n].bits_per_pixel);
		  printf("          Number of planes: %i\n", formats[n].num_planes);
		  printf("          Type: %s (%s)\n", 
			 (formats[n].type == XvRGB) ? "RGB" : "YUV",
			 (formats[n].format == XvPacked) ? "packed" : "planar");
		  
		  if(formats[n].type == XvRGB) {
		    printf("          Depth: %i\n", formats[n].depth);
		    
		    printf("          Red, green, blue masks: 0x%x, 0x%x, 0x%x\n", 
			   formats[n].red_mask, formats[n].green_mask, formats[n].blue_mask);
		  }
		}
		if(formats) XFree(formats);
	      }
	      
	      XvFreeEncodingInfo(encodings);
	    }
	    
	    printf("    End #%i.\n", adaptor);
	  }
	  
	  XvFreeAdaptorInfo(ainfo);
      }
    }
  }
}
#endif
