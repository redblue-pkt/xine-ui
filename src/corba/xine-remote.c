/* 
 * Copyright (C) 2001 the xine project
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
 */

#include <orb/orbit.h>
#include <stdio.h>
#include <stdlib.h>
#include "xine.h"
#include "corba-utils.h"

enum {
	CMD_PLAY,
	CMD_STOP,
	CMD_PAUSE,
	CMD_UNPAUSE
};

static char *prg_name;

static void usage(void)
{
	fprintf(stderr, "Usage: %s [ IOR ] <command>\n\nAvailable commands:\n  play <MRL>\n  stop\n  pause\n  unpause\n", prg_name);
	exit(1);
}

int main(int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;
	CORBA_Object server = CORBA_OBJECT_NIL;
	char *cmd_str, *mrl = NULL;
	int cmd;

	/* Get program name */
	prg_name = argv[0];

	/* Initialize CORBA */
	CORBA_exception_init(&ev);
	orb = CORBA_ORB_init(&argc, argv, "orbit-local-orb", &ev);
	if (got_exception("initializing ORB", &ev))
		exit(1);

	argc--; argv++; /* skip program name */
	if (argc < 1)
		usage();

	/* IOR supplied on command line? */
	if (strncmp(*argv, "IOR:", 4) == 0) {
		server = CORBA_ORB_string_to_object(orb, *argv, &ev);
		argc--; argv++;
		if (argc < 1)
			usage();
	}

	/* Check and parse command */
	cmd_str = *argv;
	argc--; argv++;

	if (strcmp(cmd_str, "play") == 0) {
		if (argc < 1)
			usage();
		mrl = *argv;
		cmd = CMD_PLAY;
	} else if (strcmp(cmd_str, "stop") == 0)
		cmd = CMD_STOP;
	else if (strcmp(cmd_str, "pause") == 0)
		cmd = CMD_PAUSE;
	else if (strcmp(cmd_str, "unpause") == 0)
		cmd = CMD_UNPAUSE;
	else
		usage();

	if (CORBA_Object_is_nil(server, &ev)) {

		/* Find xine server via name service */
		CosNaming_NamingContext root = get_name_service_root(orb, &ev);
		if (!CORBA_Object_is_nil(root, &ev)) {

			/* Find xine server object */
			server = CosNaming_NamingContext_resolve(root, &xine_name, &ev);
			got_exception("resolving xine server name", &ev);
		}
	}

	if (CORBA_Object_is_nil(server, &ev)) {

		/* Not found by name service, look for file containing IOR then */
		char filename[1024];
		FILE *f;

		sprintf(filename, "%s/%s", getenv("HOME"), IOR_FILE_NAME);
		f = fopen(filename, "r");
		if (f) {
			char ior[4096];

			printf("xine server not found by name, reading IOR from %s\n", filename);
			fgets(ior, sizeof(ior), f);
			fclose(f);
			server = CORBA_ORB_string_to_object(orb, ior, &ev);
			got_exception("resolving xine server IOR", &ev);
		}
	}

	if (CORBA_Object_is_nil(server, &ev)) {

		/* Still no luck, bail out */
		fprintf(stderr, "Can't locate xine server\n");
		exit(1);
	}

	/* Send command */
	switch (cmd) {
		case CMD_PLAY:
			Xine_stop(server, &ev);
			Xine_play(server, mrl, 0, &ev);
			break;
		case CMD_STOP:
			Xine_stop(server, &ev);
			break;
		case CMD_PAUSE:
			if (Xine__get_status(server, &ev) != Xine_statusPause)
				Xine_pause(server, &ev);
			break;
		case CMD_UNPAUSE:
			if (Xine__get_status(server, &ev) == Xine_statusPause)
				Xine_pause(server, &ev);
			break;
	}
	if (got_exception("sending command", &ev))
		return 1;

	CORBA_Object_release(server, &ev);
	return 0;
}
