/* 
 * Copyright (C) 2000-2017 the xine project
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
#include <sys/utsname.h>

#include <xine/xineutils.h>

#include "utils.h"
#include "dump.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#ifdef HAVE_X11_EXTENSIONS_XSHM_H
#include <X11/extensions/XShm.h>
#endif
#ifdef HAVE_XV
#include <X11/extensions/Xvlib.h>
#endif /* !HAVE_XV */
#include <X11/Xutil.h>
#endif /* !HAVE_X11 */

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
  const char          *cp;
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
      size_t   j, maxlen = 0;
      
      for(i = 0; i < n; i++) {
	if(maxlen < strlen(extlist[i]))
	  maxlen = strlen(extlist[i]);
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

#ifdef HAVE_XV
    if((Success != XvQueryExtension(display, &ver, &rev, &reqB, &eventB, &errorB))) {
      printf("   No X-Video Extension on %s\n", XDisplayName(NULL));
    } 
    else {
      printf("   X-Video Extension version: %u.%u\n", ver, rev);
      have_xv = 1;
    }
#endif /* !HAVE_XV */

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
        printf("%ux%u\n", width, height);

#ifdef HAVE_XV
      if(have_xv) {
	unsigned int          nencode, adaptor, nadaptors, k;
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
	    
            printf("    Adaptor #%u:         \"%s\"\n", adaptor, ainfo[adaptor].name);
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
	      int j;
	      printf("     Number of attributes: %i\n", nattr);
	      
	      for(j = 0; j < nattr; j++)
		printf("       - %s\n", attributes[j].name);

	      XFree(attributes);
	    } else {
	      printf("     No port attributes defined.\n");
	    }
	    
	    XvQueryEncodings(display, ainfo[adaptor].base_id, &nencode, &encodings);
	    
	    if(encodings && nencode) {
	      unsigned int n;
	      unsigned int ImageEncodings = 0;
	      
	      for(n = 0; n < nencode; n++) {
		if(!strcmp(encodings[n].name, "XV_IMAGE"))
		  ImageEncodings++;
	      }
	      
	      if(nencode - ImageEncodings) {
                printf("     Number of encodings: %u\n", nencode - ImageEncodings);
		
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
		unsigned int j;
		int n;
		
		for(j = 0; j < nencode; j++) {
		  if(!strcmp(encodings[j].name, "XV_IMAGE")) {
		    printf("     Maximum XvImage size: %li x %li\n", encodings[j].width, encodings[j].height);
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
	    
            printf("    End #%u.\n", adaptor);
	  }
	  
	  XvFreeAdaptorInfo(ainfo);
      }
#endif /* !HAVE_XV */
    }
  }
}
