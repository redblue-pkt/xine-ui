/* 
 * Copyright (C) 2000-2008 the xine project
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
 * $Id$
 *
 */
#ifndef KBINDINGS_COMMON_H
#define KBINDINGS_COMMON_H

#include "kbindings.h"

/*
 * Handled key modifier.
 */
#define KEYMOD_NOMOD           0x00000000
#define KEYMOD_CONTROL         0x00000001
#define KEYMOD_META            0x00000002
#define KEYMOD_MOD3            0x00000010
#define KEYMOD_MOD4            0x00000020
#define KEYMOD_MOD5            0x00000040

/*
 * Key binding entry struct.
 */
struct kbinding_entry_s {
  char             *comment;     /* Comment automatically added in xbinding_display*() outputs */
  char             *action;      /* Human readable action, used in config file too */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  char             *key;         /* key binding */
  int               modifier;    /* Modifier key of binding (can be OR'ed) */
  int               is_alias;    /* is made from an alias entry ? */
  int               is_gui;
};

#define MAX_ENTRIES 301          /* Including terminating null entry */
struct kbinding_s {
  int               num_entries;
  kbinding_entry_t *entry[MAX_ENTRIES];
};

/*
 * keybinding file object.
 */
typedef struct {
  FILE             *fd;
  char             *bindingfile;
  char             *ln;
  char              buf[256];
} kbinding_file_t;

/* Same as above, used on remapping */
typedef struct {
  char             *alias;
  char             *action;
  char             *key;
  char             *modifier;
  int               is_alias;
  int               is_gui;
} user_kbinding_t;

void        _kbindings_free_bindings_no_kbt(kbinding_t *);
void        _kbindings_free_bindings(kbinding_t *);
void        _kbindings_init_to_default_no_kbt(kbinding_t *);
kbinding_t *_kbindings_init_to_default(void);

#endif

