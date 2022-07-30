/*
 * Copyright (C) 2000-2022 the xine project
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
 * lirc-specific stuff
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>
#endif

#include "common.h"
#include "kbindings.h"
#include "panel.h"
#include "actions.h"
#include "lirc.h"
#include "event.h"
#include "playlist.h"

#ifdef DEBUG
#define LIRC_VERBOSE 1
#else
#define LIRC_VERBOSE 0
#endif


struct xui_lirc_s {
  struct lirc_config   *config;
  int                   fd;
  pthread_t             thread;
};

static void lirc_get_playlist (gGui_t *gui, char *from) {
  int    i;
  char **autoscan_plugins = (char **)xine_get_autoplay_input_plugin_ids (gui->xine);

  for(i = 0; autoscan_plugins[i] != NULL; ++i) {
    if(!strcasecmp(autoscan_plugins[i], from)) {
      int    num_mrls;
      int    j;
      char **autoplay_mrls = (char **)xine_get_autoplay_mrls (gui->xine, from, &num_mrls);
      if(autoplay_mrls) {

	/* First, free existing playlist */
	mediamark_free_mediamarks (gui);

	for (j = 0; j < num_mrls; j++)
	  mediamark_append_entry (gui, (const char *)autoplay_mrls[j],
				 (const char *)autoplay_mrls[j], NULL, 0, -1, 0, 0);

        gui->playlist.cur = 0;
	gui_set_current_mmk_by_index (gui, GUI_MMK_CURRENT);
        playlist_update_playlist (gui);
      }
    }
  }
}

static __attribute__((noreturn)) void *xine_lirc_loop (void *data) {
  gGui_t *gui = data;
  char             *code, *c;
  int               ret;
  const kbinding_entry_t *k;
  fd_set            set;
  struct timeval    tv;

  while (gui->running) {

    FD_ZERO(&set);
    FD_SET (gui->lirc->fd, &set);

    tv.tv_sec  = 0;
    tv.tv_usec = 500000;

    select (gui->lirc->fd + 1, &set, NULL, NULL, &tv);

    while((ret = lirc_nextcode(&code)) == 0) {

      if(code == NULL)
	break;

      while((ret = lirc_code2char (gui->lirc->config, code, &c)) == 0
	    && c != NULL) {
#if 0
	fprintf(stdout, "Command Received = '%s'\n", c);
#endif

        k = kbindings_lookup_action (gui->kbindings, c);

	if(k) {
          gui_execute_action_id (gui, (kbindings_get_action_id (k)));
	}
	else {
	  char from[256];

	  memset(&from, 0, sizeof(from));
          if(sscanf(c, "PlaylistFrom:%255s", &from[0]) == 1) {
	    if(strlen(from))
	      lirc_get_playlist (gui, from);
	  }
	}
      }

      if (panel_is_visible (gui->panel) > 1) {
        panel_paint (gui->panel);
      }

      free(code);

      if(ret == -1)
	break;

    }
    if(ret == -1)
      break;
  }

  pthread_exit(NULL);
}

void lirc_start (gGui_t *gui) {
  int flags;
  int err;

  gui->lirc = malloc (sizeof (*gui->lirc));
  if (!gui->lirc) {
    gui->lirc_enable = 0;
    return;
  }

  if ((gui->lirc->fd = lirc_init ("xine", LIRC_VERBOSE)) == -1) {
    gui->lirc_enable = 0;
    return;
  }

  fcntl (gui->lirc->fd, F_SETFD, FD_CLOEXEC);
  fcntl (gui->lirc->fd, F_SETOWN, getpid());
  flags = fcntl (gui->lirc->fd, F_GETFL, 0);
  if (flags != -1)
    fcntl (gui->lirc->fd, F_SETFL, flags|O_NONBLOCK);

  if (lirc_readconfig (NULL, &gui->lirc->config, NULL) != 0) {
    gui->lirc_enable = 0;
    lirc_deinit();
    return;
  }

  if ((err = pthread_create (&(gui->lirc->thread), NULL, xine_lirc_loop, gui)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    lirc_freeconfig (gui->lirc->config);
    lirc_deinit();
    free (gui->lirc);
    gui->lirc = NULL;
    gui->lirc_enable = 0;
  }
}

void lirc_stop (gGui_t *gui) {
  if (gui->lirc) {
    pthread_join (gui->lirc->thread, NULL);
    lirc_freeconfig (gui->lirc->config);
    lirc_deinit ();
    free (gui->lirc);
    gui->lirc = NULL;
  }
}

