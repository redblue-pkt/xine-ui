/* 
 * Copyright (C) 2000-2020 the xine project
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

#include "xine_compat.h"

#ifndef HAVE_XINE_SOCKET_CLOEXEC

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

int
xine_socket_cloexec(int domain, int type, int protocol)
{
  int s = socket(domain, type, protocol);

  if (s >= 0) {
    fcntl(s, F_SETFD, FD_CLOEXEC);
  }

  return s;
}

#endif
