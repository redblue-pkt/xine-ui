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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <pthread.h>
#include <signal.h>

#include "../xine.h"
#include "../utils.h"
#include "xine-impl.c"
#include "corba-utils.h"

static int corba_ready = 0; /* Flag: CORBA initialized and ready */

static CORBA_Environment ev;
static CORBA_ORB orb;
static Xine xine_object;
static pthread_t server_thread;
static CORBA_char *objref;

/* Initialize CORBA, bind server, but do not publish the IOR yet (because
 * xine is not yet ready to accept commands). This function also accepts
 * and removes any ORBit-specific command line arguments. */
void xine_server_init(int *argc, char **argv)
{
	PortableServer_POA root_poa;
	PortableServer_POAManager pm;

	/* Initialize CORBA */
	CORBA_exception_init(&ev);
	orb = CORBA_ORB_init(argc, argv, "orbit-local-orb", &ev);
	if (!got_exception("initializing ORB", &ev)) {

		root_poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
		if (!got_exception("getting root POA", &ev)) {

			/* Get my IOR */
			xine_object = impl_Xine__create(root_poa, &ev);
			objref = CORBA_ORB_object_to_string(orb, xine_object, &ev);
			if (!got_exception("getting IOR", &ev)) {

				/* Activate the server */
				pm = PortableServer_POA__get_the_POAManager(root_poa, &ev);
				PortableServer_POAManager_activate(pm, &ev);
				if (!got_exception("activating CORBA server", &ev))
					corba_ready = 1;
			}
		}
	}
}

/* CORBA server thread main loop */
static void *corba_server_loop()
{
	/* Run ORB message loop */
	signal(SIGINT, SIG_DFL);
	CORBA_ORB_run(orb, &ev);
	return NULL;
}

/* Advertise and start the server */
void xine_server_start(void)
{
	CosNaming_NamingContext root;
	char filename[1024];
	FILE *f;

	if (!corba_ready)
		return;

	/* Find root name service */
	root = get_name_service_root(orb, &ev);
	if (!CORBA_Object_is_nil(root, &ev)) {

		/* Bind server name */
		CosNaming_NamingContext_bind(root, &xine_name, xine_object, &ev);
		if (!got_exception("binding CORBA server name", &ev))
			printf("xine registered with CORBA name service\n");
	}

	/* Because the ORBit name service is not set up by default and the above
	 * operation is very likely to fail, we also write the IOR to the user's
	 * home directory (or print it to stdout if that fails). */
	sprintf(filename, "%s/%s", get_homedir(), IOR_FILE_NAME);
	f = fopen(filename, "w");
	if (f) {
		fprintf(f, "%s", objref);
		fclose(f);
		printf("xine CORBA IOR saved to %s\n", filename);
	} else
		printf("xine %s\n", objref);

	/* Start CORBA server thread */
	pthread_create(&server_thread, NULL, corba_server_loop, NULL);
	printf("xine_server_init: CORBA server thread created\n");
}

void xine_server_exit(void)
{
}
