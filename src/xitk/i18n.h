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

#ifndef _XITK_I18N_H
#define _XITK_I18N_H

#ifndef PACKAGE_NAME
#error config.h not included
#endif

#include <locale.h>

#ifdef ENABLE_NLS
#    include <libintl.h>
#      define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#    define XITK_GETTEXT_SEP "\004"
#    ifdef __GNUC__
#        define pgettext(Ctx, String) ({ \
             const char *msgid = Ctx XITK_GETTEXT_SEP String; \
             const char *trans = gettext (msgid); \
             trans == msgid ? String : trans; \
         })
#    else
#        define pgettext(Ctx, String) gettext(Ctx XITK_GETTEXT_SEP String) == Ctx XITK_GETTEXT_SEP String ? String : gettext(Ctx XITK_GETTEXT_SEP String)
#    endif

#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#    define pgettext(Ctx, String) (String)
#endif

#endif
