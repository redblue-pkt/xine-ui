/*
 * Copyright (C) 2000-2021 the xine project
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
#include <strings.h>

#include "common.h"
#include "tvout.h"

#define TVOUT_DEBUG 1


struct xui_tvout_s {
  gGui_t *gui;
  int    (*init) (xui_tvout_t *tvout);
  int    (*setup)(void *private);
  void   (*get_size_and_aspect)(int *width, int *height, double *pixaspect, void *private);
  int    (*set_fullscreen)(int fullscreen, int width, int height, void *private);
  int    (*get_fullscreen)(void *private);
  void   (*deinit)(void *private);
  void   *private;
};

typedef xui_tvout_t *(*backend_init_t) (gGui_t *gui);

/* Backend init prototypes */
#ifdef HAVE_NVTVSIMPLE
static xui_tvout_t *nvtv_backend (gGui_t *gui);
#endif
static xui_tvout_t *ati_backend (gGui_t *gui);

static const char * const tvout_names[] = {
#ifdef HAVE_NVTVSIMPLE
  "nvtv",
#endif
  "ati",
  NULL
};

static const backend_init_t tvout_backends[] = {
#ifdef HAVE_NVTVSIMPLE
  nvtv_backend,
#endif
  ati_backend,
  NULL
};

#ifdef HAVE_NVTVSIMPLE

#include <nvtv/nvtv_simple.h>

typedef struct {
  nvtv_simple_tvsystem  tv_system;
  int                   xrandr;
  int                   fw, fh;
  double                pa;
} nvtv_private_t;


/* ===== NVTV ===== */
static int nvtv_tvout_init (xui_tvout_t *tvout) {
  int             ret;

  if((ret = nvtv_simple_init())) {
    static const char *const tv_systems[] = { "PAL", "NTSC", NULL };
    nvtv_private_t *private = (nvtv_private_t *) calloc(1, sizeof(nvtv_private_t));

    if (!private) {
      nvtv_simple_exit ();
      return 0;
    }
    tvout->private = private;

    private->tv_system = (int)
      xine_config_register_enum (tvout->gui->xine, "gui.tvout_nvtv_tv_system",
				0,
				tv_systems,
				_("nvtv TV output system"),
				_("Select which TV output system supported by your TV."),
				CONFIG_LEVEL_BEG,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
    private->xrandr =
      xine_config_register_bool (tvout->gui->xine, "gui.tvout_nvtv_xrandr",
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

static xui_tvout_t *nvtv_backend (gGui_t *gui) {
  xui_tvout_t *tvout = malloc (sizeof (*tvout);

  if (!tvout)
    return NULL;

  tvout->gui                 = gui;
  tvout->init                = nvtv_tvout_init;
  tvout->setup               = nvtv_tvout_setup;
  tvout->get_size_and_aspect = nvtv_get_size_and_aspect;
  tvout->set_fullscreen      = nvtv_tvout_set_fullscreen_mode;
  tvout->get_fullscreen      = nvtv_tvout_get_fullscreen_mode;
  tvout->deinit              = nvtv_tvout_deinit;
  tvout->private             = NULL;

  return tvout;
}
#endif

/* ===== ATI ===== */
typedef struct {
  char   *atitvout_cmds[2];
  int     fullscreen;
} ati_private_t;

static int ati_tvout_init (xui_tvout_t *tvout) {
  ati_private_t *private = (ati_private_t *) calloc(1, sizeof(ati_private_t));

  if (!private)
    return 0;

  tvout->private = private;

  private->atitvout_cmds[0] = (char *)
    xine_config_register_string (tvout->gui->xine, "gui.tvout_ati_cmd_off",
				 "sudo /usr/local/sbin/atitvout c",
				 _("Command to turn off TV out"),
				 _("atitvout command line used to turn on TV output."),
				 CONFIG_LEVEL_BEG,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA);

  private->atitvout_cmds[1] = (char *)
    xine_config_register_string (tvout->gui->xine, "gui.tvout_ati_cmd_on",
				 "sudo /usr/local/sbin/atitvout pal ct",
				 _("Command to turn on TV out"),
				 _("atitvout command line used to turn on TV output."),
				 CONFIG_LEVEL_BEG,
				 CONFIG_NO_CB,
				 CONFIG_NO_DATA);

  private->fullscreen = 0;

  return 1;
}

static int ati_tvout_setup (void *data) {
  (void)data;
  return 1;
}

static void ati_get_size_and_aspect (int *width, int *height, double *pixaspect, void *private) {
  (void)width;
  (void)height;
  (void)pixaspect;
  (void)private;
}

static int ati_tvout_set_fullscreen_mode(int fullscreen, int width, int height, void *data) {
  ati_private_t *private = (ati_private_t *) data;

  (void)width;
  (void)height;
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

static xui_tvout_t *ati_backend (gGui_t *gui) {
  xui_tvout_t *tvout = malloc (sizeof (*tvout));

  if (!tvout)
    return NULL;

  tvout->gui                 = gui;
  tvout->init                = ati_tvout_init;
  tvout->setup               = ati_tvout_setup;
  tvout->get_size_and_aspect = ati_get_size_and_aspect;
  tvout->set_fullscreen      = ati_tvout_set_fullscreen_mode;
  tvout->get_fullscreen      = ati_tvout_get_fullscreen_mode;
  tvout->deinit              = ati_tvout_deinit;
  tvout->private             = NULL;

  return tvout;
}
/* ===== ATI END ===== */


/* ===== Wrapper ===== */
xui_tvout_t *tvout_init (gGui_t *gui, const char *backend) {
  if(backend) {
    int i;

#ifdef TVOUT_DEBUG
    printf("Looking for %s tvout backend\n", backend);
#endif

    for (i = 0; tvout_backends[i]; i++) {
      if (!strcasecmp (tvout_names[i], backend)) {
        xui_tvout_t *tvout = tvout_backends[i] (gui);

	if(tvout) {
          if (!tvout->init (tvout)) {
            free (tvout);
            tvout = NULL;
          }
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

int tvout_setup(xui_tvout_t *tvout) {
  if(tvout)
    return (tvout->setup(tvout->private));

  return 0;
}

void tvout_get_size_and_aspect(xui_tvout_t *tvout, int *width, int *height, double *pix_aspect) {
  if(tvout)
    tvout->get_size_and_aspect(width, height, pix_aspect, tvout->private);
}

int tvout_set_fullscreen_mode(xui_tvout_t *tvout, int fullscreen, int width, int height) {
  if(tvout)
    return (tvout->set_fullscreen(fullscreen, width, height, tvout->private));

  return 0;
}

int tvout_get_fullscreen_mode(xui_tvout_t *tvout) {
  if(tvout)
    return (tvout->get_fullscreen(tvout->private));

  return 0;
}

void tvout_deinit(xui_tvout_t *tvout) {
  if (tvout) {
    tvout->deinit (tvout->private);
    free (tvout);
  }
}

const char * const *tvout_get_backend_names (void) {
  return tvout_names;
}
