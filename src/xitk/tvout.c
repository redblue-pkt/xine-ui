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
#include <strings.h>
#include <X11/Xlib.h>

#include "common.h"

#define TVOUT_DEBUG 1

extern gGui_t  *gGui;

struct tvout_s {
  int    (*init)(Display *display, void **private);
  int    (*setup)(void *private);
  void   (*get_size_and_aspect)(int *width, int *height, double *pixaspect, void *private);
  int    (*set_fullscreen)(int fullscreen, int width, int height, void *private);
  int    (*get_fullscreen)(void *private);
  void   (*deinit)(void *private);
  void   *private;
};

typedef tvout_t *(*backend_init_t)(Display *);

/* Backend init prototypes */
#ifdef HAVE_NVTVSIMPLE
static tvout_t *nvtv_backend(Display *);
#endif
static tvout_t *ati_backend(Display *);

static struct {
  char            *name;
  backend_init_t   init;
} backends[] = {
#ifdef HAVE_NVTVSIMPLE
  { "nvtv", nvtv_backend },
#endif
  { "ati",  ati_backend  },
  { NULL,   NULL         }
};


#ifdef HAVE_NVTVSIMPLE

#include <nvtv/nvtv_simple.h>

typedef struct {
  nvtv_simple_tvsystem  tv_system;
  int                   xrandr;
  int                   fw, fh;
  double                pa;
} nvtv_private_t;

static char *tv_systems[] = { "PAL", "NTSC", NULL };

/* ===== NVTV ===== */
static int nvtv_tvout_init(Display *display, void **data) {
  int             ret;
  
  if((ret = nvtv_simple_init())) {
    nvtv_private_t *private = (nvtv_private_t *) xine_xmalloc(sizeof(nvtv_private_t));

    *data = private;
    
    private->tv_system = (int) 
      xine_config_register_enum(gGui->xine, "gui.tvout_nvtv_tv_system", 
				0,
				tv_systems,
				_("nvtv TV output system"),
				_("Select which TV output system supported by your TV."), 
				CONFIG_LEVEL_BEG,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
    private->xrandr = 
      xine_config_register_bool(gGui->xine, "gui.tvout_nvtv_xrandr", 
				1,
				_("Enable XRandr extension"),
				_("Enable the use of xrandr to autofit the screen "
				  "to match the TV-resolution"),
				CONFIG_LEVEL_ADV,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  }

  return ret;
}

static int nvtv_tvout_setup(void *data) {
  nvtv_private_t *private = (nvtv_private_t *) data;
  double          fps;

  nvtv_simple_enable(1);

  if(private->xrandr)
    (void) nvtv_enable_autoresize(1);
  
  nvtv_simple_set_tvsystem((nvtv_simple_tvsystem) private->tv_system);

  /* Retrieve MAX width/height */
  (void) nvtv_simple_switch(NVTV_SIMPLE_TV_ON, 0, 0);
  nvtv_simple_size(&(private->fw), &(private->fh), &(private->pa), &fps);
  (void) nvtv_simple_switch(NVTV_SIMPLE_TV_OFF, 0, 0);
  
  return 1;
}

static void nvtv_get_size_and_aspect(int *width, int *height, double *pixaspect, void *data) {
  nvtv_private_t *private = (nvtv_private_t *) data;
  
  if(width)
    *width = private->fw;
  if(height)
    *height = private->fh;
  if(pixaspect)
    *pixaspect = private->pa;
}

static int nvtv_tvout_set_fullscreen_mode(int fullscreen, int width, int height, void *data) {
  nvtv_private_t *private = (nvtv_private_t *) data;

  if((width > private->fw) || (height > private->fh)) {
    width = private->fw;
    height = private->fh;
  }
  
  (void) nvtv_simple_switch(fullscreen ?
			    NVTV_SIMPLE_TV_ON : NVTV_SIMPLE_TV_OFF, 
			    width, height);
  
  return 1;
}

static int nvtv_tvout_get_fullscreen_mode(void *data) {
  int state = nvtv_simple_get_state();

  return (state == NVTV_SIMPLE_TV_ON ? 1 : 0);
}

static void nvtv_tvout_deinit(void *data) {
  nvtv_private_t *private = (nvtv_private_t *) data;

  if(nvtv_simple_get_state() == NVTV_SIMPLE_TV_ON) {
    nvtv_simple_enable(0);
    (void) nvtv_simple_switch(NVTV_SIMPLE_TV_OFF, 0, 0);
  }

  nvtv_simple_exit();
  free(private);
}

static tvout_t *nvtv_backend(Display *display) {
  static tvout_t tvout;
  
  tvout.init                = nvtv_tvout_init;
  tvout.setup               = nvtv_tvout_setup;
  tvout.get_size_and_aspect = nvtv_get_size_and_aspect;
  tvout.set_fullscreen      = nvtv_tvout_set_fullscreen_mode;
  tvout.get_fullscreen      = nvtv_tvout_get_fullscreen_mode;
  tvout.deinit              = nvtv_tvout_deinit;
  tvout.private             = NULL;

  return &tvout;
}
#endif

/* ===== ATI ===== */
typedef struct {
  char   *atitvout_cmds[2];
  int     fullscreen;
} ati_private_t;

static int ati_tvout_init(Display *display, void **data) {
  ati_private_t *private = (ati_private_t *) xine_xmalloc(sizeof(ati_private_t));
  
  *data = private;
  
  private->atitvout_cmds[0] = (char *) 
    xine_config_register_string (gGui->xine, "gui.tvout_ati_cmd_off", 
				 "sudo /usr/local/sbin/atitvout c",
				 _("Command to turn off TV out"),
				 _("atitvout command line used to turn on TV output."),
				 CONFIG_LEVEL_BEG,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA);

  private->atitvout_cmds[1] = (char *) 
    xine_config_register_string (gGui->xine, "gui.tvout_ati_cmd_on", 
				 "sudo /usr/local/sbin/atitvout pal ct",
				 _("Command to turn on TV out"),
				 _("atitvout command line used to turn on TV output."),
				 CONFIG_LEVEL_BEG,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA);
  
  private->fullscreen = 0;
  
  return 1;
}

static int ati_tvout_setup(void *data) {
  return 1;
}

static void ati_get_size_and_aspect(int *width, int *height, double *pixaspect, void *private) {
}

static int ati_tvout_set_fullscreen_mode(int fullscreen, int width, int height, void *data) {
  ati_private_t *private = (ati_private_t *) data;

  if(private->atitvout_cmds[fullscreen] && strlen(private->atitvout_cmds[fullscreen])) {
    int err;
    
    if((err = xine_system(0, private->atitvout_cmds[fullscreen])))
      fprintf(stderr, "command: '%s' failed with error code %d.\n", 
	      private->atitvout_cmds[fullscreen], err);
  }
    
  private->fullscreen = fullscreen;
  
  return 1;
}

static int ati_tvout_get_fullscreen_mode(void *data) {
  ati_private_t *private = (ati_private_t *) data;
  
  return (private->fullscreen);
}

static void ati_tvout_deinit(void *data) {
  ati_private_t *private = (ati_private_t *) data;

  if(private->fullscreen && private->atitvout_cmds[0] && strlen(private->atitvout_cmds[0])) {
    int err;
    
    if((err = xine_system(0, private->atitvout_cmds[0])))
      fprintf(stderr, "command: '%s' failed with error code %d.\n", 
	      private->atitvout_cmds[0], err);
  }
    
  free(private);
}

static tvout_t *ati_backend(Display *display) {
  static tvout_t tvout;

  tvout.init                = ati_tvout_init;
  tvout.setup               = ati_tvout_setup;
  tvout.get_size_and_aspect = ati_get_size_and_aspect;
  tvout.set_fullscreen      = ati_tvout_set_fullscreen_mode;
  tvout.get_fullscreen      = ati_tvout_get_fullscreen_mode;
  tvout.deinit              = ati_tvout_deinit;
  tvout.private             = NULL;
  
  return &tvout;
}
/* ===== ATI END ===== */


/* ===== Wrapper ===== */
tvout_t *tvout_init(Display *display, char *backend) {
  if(backend) {
    int i;
    
#ifdef TVOUT_DEBUG
    printf("Looking for %s tvout backend\n", backend);
#endif
    
    for(i = 0; backends[i].name; i++) {
      if(!strcasecmp(backends[i].name, backend)) {
	tvout_t *tvout = backends[i].init(display);
	
	if(tvout) {
	  if(!tvout->init(display, &(tvout->private)))
	    tvout = NULL;
#ifdef TVOUT_DEBUG
	  else
	    printf(" ***Initialized\n");
#endif
	}
	
	return tvout;
      }
    }
  }

  return NULL;
}

int tvout_setup(tvout_t *tvout) {
  if(tvout)
    return (tvout->setup(tvout->private));
  
  return 0;
}

void tvout_get_size_and_aspect(tvout_t *tvout, int *width, int *height, double *pix_aspect) {
  if(tvout)
    tvout->get_size_and_aspect(width, height, pix_aspect, tvout->private);
}

int tvout_set_fullscreen_mode(tvout_t *tvout, int fullscreen, int width, int height) {
  if(tvout)
    return (tvout->set_fullscreen(fullscreen, width, height, tvout->private));
  
  return 0;
}

int tvout_get_fullscreen_mode(tvout_t *tvout) {
  if(tvout)
    return (tvout->get_fullscreen(tvout->private));
  
  return 0;
}

void tvout_deinit(tvout_t *tvout) {
  if(tvout)
    tvout->deinit(tvout->private);
}

char **tvout_get_backend_names(void) {
  static char *bckends[(sizeof(backends) / sizeof(backends[0]))];
  int i = 0;
  
  while(backends[i].name) {
    bckends[i] = backends[i].name;
    i++;
  }
  
  bckends[i] = NULL;
  
  return bckends;
}
