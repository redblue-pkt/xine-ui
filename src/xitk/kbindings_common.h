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
  const char       *action;      /* Human readable action, used in config file too.
                                  * We currently only use the known ones, and thus can stick
                                  * to our default static const strings. */
  const char       *comment;     /* Comment automatically added in xbinding_display*() outputs */
  char             *key;         /* key binding */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  uint16_t          index;       /* into kbinding_t.entry */
  uint8_t           modifier;    /* Modifier key of binding (can be OR'ed) */
  uint8_t           is_alias:1;  /* is made from an alias entry ? */
  uint8_t           is_gui:1;    /* controls the gui (eg opening a dialogue) rather than the xine playback */
  uint8_t           is_default:1;/* still or again the preset mapping */
};

#define MAX_ENTRIES 301          /* Including terminating null entry */

kbinding_t *_kbindings_init_to_default (void);
kbinding_t *_kbindings_duplicate_kbindings (kbinding_t *kbt);

const kbinding_entry_t *_kbindings_get_entry (kbinding_t *kbt, int index);
int _kbindings_get_num_entries (kbinding_t *kbt);

/* key == "void" means delete.
 * return -1 (OK), -2 (unchanged), -3 (invalid), -4 (table full), >= 0 (index that already uses this key). */
int kbindings_entry_set (kbinding_t *kbt, int index, int modifier, const char *key);
int kbindings_alias_add (kbinding_t *kbt, int index, int modifier, const char *key);
int kbindings_reset (kbinding_t *kbt, int index);

kbinding_entry_t *kbindings_find_key (kbinding_t *kbt, const char *key, int modifier);

void _kbindings_free_bindings (kbinding_t *kbt);

#endif
