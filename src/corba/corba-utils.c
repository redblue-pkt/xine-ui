/* 
 * corba-utils.c - Utility functions common to client and server
 *
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
#include "corba-utils.h"

/* Xine server name */
static CosNaming_NameComponent comp[1] = {{"Xine", "server"}};
CosNaming_Name xine_name = {1, 1, comp, CORBA_FALSE};

/* Check for exception, returns 0 if everything went OK */
int got_exception(const char *action, CORBA_Environment *ev)
{
	if (ev->_major != CORBA_NO_EXCEPTION) {
		fprintf(stderr, "Error while %s (%s)\n", action, CORBA_exception_id(ev));
		CORBA_exception_free(ev);
		return 1;
	} else
		return 0;
}

/* Find root naming context */
CosNaming_NamingContext get_name_service_root(CORBA_ORB orb, CORBA_Environment *ev)
{
	CosNaming_NamingContext root = CORBA_OBJECT_NIL;

	/* First try the official way */
	root = CORBA_ORB_resolve_initial_references(orb, "NameService", ev);
	if (got_exception("finding CORBA name service", ev) || CORBA_Object_is_nil(root, ev)) {

		/* No initial name service present, look for name server IOR in
		 * /tmp/name-service-ior (this is what the ORBit docs suggest) */
		FILE *f = fopen("/tmp/name-service-ior", "r");
		if (f) {
			char ior[4096];
			int i;

			fgets(ior, sizeof(ior), f);
			fclose(f);

			for (i=0; ior[i]; i++)
				if (ior[i] == '\n')
					ior[i] = 0;

			root = CORBA_ORB_string_to_object(orb, ior, ev);
			got_exception("resolving name service IOR", ev);
		}
	}

	return root;
}
