/* 
 * corba-utils.h - Utility functions common to client and server
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

#include <orb/orbit.h>
#include <ORBitservices/CosNaming.h>

/* Xine server name */
extern CosNaming_Name xine_name;

/* File where server IOR gets written to */
#define IOR_FILE_NAME ".xine.ior"

/* Check for exception, returns 0 if everything went OK */
extern int got_exception(const char *action, CORBA_Environment *ev);

/* Find root naming context */
extern CosNaming_NamingContext get_name_service_root(CORBA_ORB orb, CORBA_Environment *ev);
