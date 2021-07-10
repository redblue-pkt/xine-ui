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

#include "common.h"
#include "kbindings.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/backend.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/browser.h"
#include "kbindings_common.h"


#undef TRACE_KBINDINGS

#define  KBEDIT_NOOP           0
#define  KBEDIT_ALIASING       1
#define  KBEDIT_EDITING        2

struct xui_keyedit_s {
  gGui_t               *gui;

  kbinding_t           *kbt;

  xitk_window_t        *xwin;
  xitk_widget_list_t   *widget_list;
  int                   visible;

  int                   nsel;
  kbinding_entry_t     *ksel;

  xitk_widget_t        *browser;

  xitk_widget_t        *alias;
  xitk_widget_t        *edit;
  xitk_widget_t        *delete;
  xitk_widget_t        *save;
  xitk_widget_t        *done;
  xitk_widget_t        *grab;

  xitk_widget_t        *comment;
  xitk_widget_t        *key;

  xitk_widget_t        *ctrl;
  xitk_widget_t        *meta;
  xitk_widget_t        *mod3;
  xitk_widget_t        *mod4;
  xitk_widget_t        *mod5;

  int                   num_entries;
  char                **entries;
  char                **shortcuts;

  int                   grabbing;
  int                   action_wanted; /* See KBEDIT_ defines */
  kbinding_entry_t     *confirm_this;

  xitk_register_key_t   kreg;

  struct {
    xitk_register_key_t key;
    kbinding_entry_t   *entry;
    xitk_window_t      *xwin;
  }                     kbr;
};

#define WINDOW_WIDTH        530
#define WINDOW_HEIGHT       456
#define MAX_DISP_ENTRIES    11

#define DEFAULT_DISPLAY_MODE   1
#define LIRC_DISPLAY_MODE      2

/*
  Remap file entries syntax are:
  ...
  WindowReduce {
      key = less
      modifier = none
  }

  Alias {
      entry = WindowReduce
      key = w
      modifier = control
  }
  ...
  WindowReduce action key is <less>, with no modifier usage.
  There is an alias: C-w (control w> keystroke will have same
                     action than WindowReduce.

  modifier entry is optional.
  modifier key can be multiple:
 ...
  WindowReduce {
      key = less
      modifier = control, meta
  }
  ...
  Here, WindowReduce action keystroke will be CM-<

  Key modifier name are:
    none, control, ctrl, meta, alt, mod3, mod4, mod5.

  shift,lock and numlock modifier are voluntary not handled.

*/


static void _kbindings_display_kbindings_to_stream(kbinding_t *kbt, int mode, FILE *stream) {
  char               buf[256];
  kbinding_entry_t **k;
  int          i;

  if(kbt == NULL) {
    gui_msg (gGui, XUI_MSG_ERROR, _("OOCH: key binding table is NULL.\n"));
    return;
  }

  switch(mode) {
  case LIRC_DISPLAY_MODE:
    fprintf(stream, "##\n# xine key bindings.\n"
		    "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);

    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      if(!k[i]->is_alias) {
	snprintf(buf, sizeof(buf), "# %s\n", k[i]->comment);
	strlcat(buf, "begin\n"
		     "\tremote = xxxxx\n"
		     "\tbutton = xxxxx\n"
		     "\tprog   = xine\n"
		     "\trepeat = 0\n", sizeof(buf));
	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tconfig = %s\n", k[i]->action);
	strlcat(buf, "end\n\n", sizeof(buf));
	fputs(buf, stream);
      }
    }
    fprintf(stream, "##\n# End of xine key bindings.\n##\n");
    break;

  case DEFAULT_DISPLAY_MODE:
  default:
    fprintf(stream, "##\n# xine key bindings.\n"
		    "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);

    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      snprintf(buf, sizeof(buf), "# %s\n", k[i]->comment);

      if(k[i]->is_alias) {
	sprintf(buf+strlen(buf), "Alias {\n\tentry = %s\n", k[i]->action);
      }
      else
	sprintf(buf+strlen(buf), "%s {\n", k[i]->action);

      sprintf(buf+strlen(buf), "\tkey = %s\n\tmodifier = ", k[i]->key);
      if(k[i]->modifier == KEYMOD_NOMOD)
	strlcat(buf, "none, ", sizeof(buf));
      if(k[i]->modifier & KEYMOD_CONTROL)
	strlcat(buf, "control, ", sizeof(buf));
      if(k[i]->modifier & KEYMOD_META)
	strlcat(buf, "meta, ", sizeof(buf));
      if(k[i]->modifier & KEYMOD_MOD3)
	strlcat(buf, "mod3, ", sizeof(buf));
      if(k[i]->modifier & KEYMOD_MOD4)
	strlcat(buf, "mod4, ", sizeof(buf));
      if(k[i]->modifier & KEYMOD_MOD5)
	strlcat(buf, "mod5, ", sizeof(buf));
      buf[strlen(buf) - 2] = '\n';
      buf[strlen(buf) - 1] = '\0';
      strlcat(buf, "}\n\n", sizeof(buf));
      fputs(buf, stream);
    }
    fprintf(stream, "##\n# End of xine key bindings.\n##\n");
    break;
  }

}
/*
 * Display all key bindings from kbt key binding table.
 */
static void _kbindings_display_kbindings(kbinding_t *kbt, int mode) {
  _kbindings_display_kbindings_to_stream(kbt, mode, stdout);
}

/*
 * Convert a modifier to key binding modifier style.
 */
static void kbindings_convert_modifier(int mod, int *modifier) {

  ABORT_IF_NULL(modifier);

  *modifier = KEYMOD_NOMOD;

  if(mod & MODIFIER_CTRL)
    *modifier |= KEYMOD_CONTROL;
  if(mod & MODIFIER_META)
    *modifier |= KEYMOD_META;
  if(mod & MODIFIER_MOD3)
    *modifier |= KEYMOD_MOD3;
  if(mod & MODIFIER_MOD4)
    *modifier |= KEYMOD_MOD4;
  if(mod & MODIFIER_MOD5)
    *modifier |= KEYMOD_MOD5;
}

/*
 * Save current key binding table to keymap file.
 */
void kbindings_save_kbinding(kbinding_t *kbt) {
  gGui_t *gui = gGui;
  FILE   *f;

  if((f = fopen(gui->keymap_file, "w+")) == NULL) {
    return;
  }
  else {
    _kbindings_display_kbindings_to_stream(kbt, DEFAULT_DISPLAY_MODE, f);
    fclose(f);
  }
}

/*
 * Return a duplicated table from kbt.
 */
static kbinding_t *_kbindings_duplicate_kbindings(kbinding_t *kbt) {
  int         i;
  kbinding_t *k;

  ABORT_IF_NULL(kbt);

  k = (kbinding_t *) calloc(1, sizeof(kbinding_t));

  for(i = 0; kbt->entry[i]->action != NULL; i++) {
    k->entry[i]            = (kbinding_entry_t *) calloc(1, sizeof(kbinding_entry_t));
    k->entry[i]->comment   = strdup(kbt->entry[i]->comment);
    k->entry[i]->action    = strdup(kbt->entry[i]->action);
    k->entry[i]->action_id = kbt->entry[i]->action_id;
    k->entry[i]->key       = strdup(kbt->entry[i]->key);
    k->entry[i]->modifier  = kbt->entry[i]->modifier;
    k->entry[i]->is_alias  = kbt->entry[i]->is_alias;
    k->entry[i]->is_gui    = kbt->entry[i]->is_gui;
  }

  k->entry[i]            = (kbinding_entry_t *) calloc(1, sizeof(kbinding_entry_t));
  k->entry[i]->comment   = NULL;
  k->entry[i]->action    = NULL;
  k->entry[i]->action_id = 0;
  k->entry[i]->key       = NULL;
  k->entry[i]->modifier  = 0;
  k->entry[i]->is_alias  = 0;
  k->entry[i]->is_gui    = 0;
  k->num_entries         = i + 1;

  return k;
}

/*
 * Free key binding table kbt, then set it to default table.
 */
void kbindings_reset_kbinding(kbinding_t *kbt) {

  ABORT_IF_NULL(kbt);

  _kbindings_free_bindings_no_kbt(kbt);
  _kbindings_init_to_default_no_kbt(kbt);
}

/*
 * This could be used to create a default key binding file
 * with 'xine --keymap > $HOME/.xine_keymap'
 */
void kbindings_display_default_bindings(void) {
  kbinding_t *kbt = NULL;

  kbt = _kbindings_init_to_default();

  _kbindings_display_kbindings(kbt, DEFAULT_DISPLAY_MODE);
  _kbindings_free_bindings(kbt);
}

/*
 * This could be used to create a default key binding file
 * with 'xine --keymap=lirc > $HOME/.lircrc'
 */
void kbindings_display_default_lirc_bindings(void) {
  kbinding_t *kbt = NULL;

  kbt = _kbindings_init_to_default();

  _kbindings_display_kbindings(kbt, LIRC_DISPLAY_MODE);
  _kbindings_free_bindings(kbt);
}

/*
 * This could be used to create a key binding file from key binding
 * object k, with 'xine --keymap > $HOME/.xine_keymap'
 */
void kbindings_display_current_bindings(kbinding_t *kbt) {

  ABORT_IF_NULL(kbt);

  _kbindings_display_kbindings(kbt, DEFAULT_DISPLAY_MODE);
}

/*
 * Return action id from key binding kbt entry.
 */
action_id_t kbindings_get_action_id(kbinding_entry_t *kbt) {

  if(kbt == NULL)
    return ACTID_NOKEY;

  return kbt->action_id;
}

static size_t _kbindings_get_shortcut_from_kbe(const kbinding_entry_t *kbe, char *shortcut, size_t shortcut_size) {
  gGui_t *gui = gGui;

  if(kbe) {
    strlcpy(shortcut, "[", shortcut_size);

    if(gui->shortcut_style == 0) {
      if(kbe->modifier & KEYMOD_CONTROL)
        snprintf(shortcut+strlen(shortcut), shortcut_size-strlen(shortcut), "%s+", _("Ctrl"));
      if(kbe->modifier & KEYMOD_META)
        snprintf(shortcut+strlen(shortcut), shortcut_size-strlen(shortcut), "%s+", _("Alt"));
      if(kbe->modifier & KEYMOD_MOD3)
        strlcat(shortcut, "M3+", shortcut_size);
      if(kbe->modifier & KEYMOD_MOD4)
        strlcat(shortcut, "M4+", shortcut_size);
      if(kbe->modifier & KEYMOD_MOD5)
        strlcat(shortcut, "M5+", shortcut_size);
    }
    else {
      if(kbe->modifier & KEYMOD_CONTROL)
        strlcat(shortcut, "C-", shortcut_size);
      if(kbe->modifier & KEYMOD_META)
        strlcat(shortcut, "M-", shortcut_size);
      if(kbe->modifier & KEYMOD_MOD3)
        strlcat(shortcut, "M3-", shortcut_size);
      if(kbe->modifier & KEYMOD_MOD4)
        strlcat(shortcut, "M4-", shortcut_size);
      if(kbe->modifier & KEYMOD_MOD5)
        strlcat(shortcut, "M5-", shortcut_size);
    }

    strlcat(shortcut, kbe->key, shortcut_size);
    return strlcat(shortcut, "]", shortcut_size); /* result >= shortcut_size if buffer was too small */
  }
  return 0;
}

size_t kbindings_get_shortcut(kbinding_t *kbt, const char *action, char *buf, size_t buf_size) {
  kbinding_entry_t  *k;

  if(kbt) {
    if(action && (k = kbindings_lookup_action(kbt, action))) {
      if(strcmp(k->key, "VOID")) {
        return _kbindings_get_shortcut_from_kbe(k, buf, buf_size);
      }
    }
  }
  return 0;
}

/*
 * Try to find and entry in key binding table matching with key and modifier value.
 */
kbinding_entry_t *kbindings_lookup_binding (kbinding_t *kbt, const char *key, int modifier) {
  kbinding_entry_t *kret = NULL, *k;
  int               i;

  if((key == NULL) || (kbt == NULL))
    return NULL;

#ifdef TRACE_KBINDINGS
  printf("Looking for: '%s' [", key);
  if(modifier == KEYMOD_NOMOD)
    printf("none, ");
  if(modifier & KEYMOD_CONTROL)
    printf("control, ");
  if(modifier & KEYMOD_META)
    printf("meta, ");
  if(modifier & KEYMOD_MOD3)
    printf("mod3, ");
  if(modifier & KEYMOD_MOD4)
    printf("mod4, ");
  if(modifier & KEYMOD_MOD5)
    printf("mod5, ");
  printf("\b\b]\n");
#endif

  /* Be case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if(k && k->key && strlen(k->key) && ((!(strcmp(k->key, key))) && (modifier == k->modifier))) {
      kret = k;
      goto __found;
    }
  }

  /* Not case sensitive */
  /*
  for(i = 0, k = kbt[0]; kbt[i]->action != NULL; i++, k = kbt[i]) {
    if((!(strcasecmp(k->key, key))) && (modifier == k->modifier))
      return k;
  }
  */
  /* Last chance */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if(k && k->key && strlen(k->key) && ((!(strcmp(k->key, key))) && (k->modifier == KEYMOD_NOMOD))) {
      kret = k;
      break;
    }
  }

 __found:

  /* Keybinding unknown */
  return kret;
}

/* Convert X(Button|Key)(Press|Release) events into string identifier. */
static void event2id(unsigned long keysym, unsigned int keycode, int button, char *buf, size_t size) {
  if (button >= 0) {
    snprintf(buf, size, "XButton_%d", button);
    return;
  }
  switch (keysym) {
    default:
      if (xitk_keysym_to_string(keysym, buf, size) > 0)
        break;
      /* fall through */
    case 0: /* Key without assigned KeySymbol */
      snprintf(buf, size, "XKey_%u", keycode);
      break;
  }
}

/*
 * Handle key event from an XEvent.
 */
static kbinding_entry_t *_kbinding_entry_from_be_event (kbinding_t *kbt, const xitk_be_event_t *event) {
  char buf[64];
  int qual;

  if (!kbt || !event)
    return NULL;

  if (xitk_be_event_name (event, buf, sizeof(buf)) < 1)
    return NULL;
  /* printf ("_kbinding_entry_from_be_event (%s, 0x%x).\n", s, (unsigned int)event->qual); */
  kbindings_convert_modifier (event->qual, &qual);
  return kbindings_lookup_binding (kbt, buf, qual);
}

action_id_t kbinding_aid_from_be_event (kbinding_t *kbt, const xitk_be_event_t *event, int no_gui) {
  kbinding_entry_t *entry = _kbinding_entry_from_be_event (kbt, event);

  if (entry && (!entry->is_gui || !no_gui))
    return entry->action_id;
  return 0;
}

void kbindings_handle_kbinding(kbinding_t *kbt, unsigned long keysym, int keycode, int modifier, int button) {
  gGui_t *gui = gGui;
  char              buf[256];
  kbinding_entry_t *k;

  if (!gui->kbindings_enabled || (kbt == NULL))
    return;

  kbindings_convert_modifier(modifier, &modifier);
  event2id(keysym, keycode, button, buf, sizeof(buf));

  k = kbindings_lookup_binding(kbt, buf, modifier);
  if(k && !(gui->no_gui && k->is_gui))
    gui_execute_action_id (gui, k->action_id);
#if 0  /* DEBUG */
  else
    printf("%s unhandled\n", kbuf);
#endif
}

/*
 * ***** Key Binding Editor ******
 */
/*
 * Return 1 if setup panel is visible
 */
int kbedit_is_visible (xui_keyedit_t *kbedit) {
  if (kbedit) {
    if (kbedit->gui->use_root_window)
      return (xitk_window_flags (kbedit->xwin, 0, 0) & XITK_WINF_VISIBLE);
    else
      return kbedit->visible && (xitk_window_flags (kbedit->xwin, 0, 0) & XITK_WINF_VISIBLE);
  }
  return 0;
}

/*
 * Raise kbedit->xwin
 */
void kbedit_raise_window (xui_keyedit_t *kbedit) {
  if(kbedit != NULL)
    raise_window (kbedit->gui, kbedit->xwin, kbedit->visible, 1);
}

/*
 * Hide/show the kbedit panel
 */
void kbedit_toggle_visibility (xitk_widget_t *w, void *data) {
  xui_keyedit_t *kbedit = data;
  (void)w;
  if (kbedit)
    toggle_window (kbedit->gui, kbedit->xwin, kbedit->widget_list, &kbedit->visible, 1);
}

static void kbedit_create_browser_entries (xui_keyedit_t *kbedit) {
  int           i;

  if(kbedit->num_entries) {
    for(i = 0; i < kbedit->num_entries; i++) {
      free((char *)kbedit->entries[i]);
      free((char *)kbedit->shortcuts[i]);
    }
    free((char **)kbedit->entries);
    free((char **)kbedit->shortcuts);
  }

  kbedit->entries   = (char **) calloc(kbedit->kbt->num_entries, sizeof(char *));
  kbedit->shortcuts = (char **) calloc(kbedit->kbt->num_entries, sizeof(char *));
  kbedit->num_entries = kbedit->kbt->num_entries - 1;

  for(i = 0; i < kbedit->num_entries; i++) {
    char  buf[2048];
    char  shortcut[32];

    if (_kbindings_get_shortcut_from_kbe(kbedit->kbt->entry[i], shortcut, sizeof(shortcut)) < 1)
      strcpy(shortcut, "[VOID]");

    if(kbedit->kbt->entry[i]->is_alias)
      snprintf(buf, sizeof(buf), "@{%s}", kbedit->kbt->entry[i]->comment);
    else
      strlcpy(buf, kbedit->kbt->entry[i]->comment, sizeof(buf));

    kbedit->entries[i]   = strdup(buf);
    kbedit->shortcuts[i] = strdup(shortcut);
  }

  kbedit->entries[i]   = NULL;
  kbedit->shortcuts[i] = NULL;
}

static void kbedit_display_kbinding (xui_keyedit_t *kbedit, const char *action, kbinding_entry_t *kbe) {

  if(action && kbe) {

    xitk_label_change_label(kbedit->comment, action);
    xitk_label_change_label(kbedit->key, kbe->key);

    xitk_button_set_state (kbedit->ctrl, (kbe->modifier & KEYMOD_CONTROL) ? 1 : 0);
    xitk_button_set_state (kbedit->meta, (kbe->modifier & KEYMOD_META) ? 1 : 0);
    xitk_button_set_state (kbedit->mod3, (kbe->modifier & KEYMOD_MOD3) ? 1 : 0);
    xitk_button_set_state (kbedit->mod4, (kbe->modifier & KEYMOD_MOD4) ? 1 : 0);
    xitk_button_set_state (kbedit->mod5, (kbe->modifier & KEYMOD_MOD5) ? 1 : 0);
  }
}

/*
 *
 */
static void kbedit_select (xui_keyedit_t *kbedit, int s) {

  xitk_enable_and_show_widget(kbedit->alias);
  xitk_enable_and_show_widget(kbedit->edit);
  xitk_enable_and_show_widget(kbedit->delete);

  kbedit->nsel = s;
  kbedit->ksel = kbedit->kbt->entry[s];
  kbedit_display_kbinding (kbedit, kbedit->entries[s], kbedit->kbt->entry[s]);
}

/*
 *
 */
static void kbedit_unset (xui_keyedit_t *kbedit) {

  if(xitk_labelbutton_get_state(kbedit->alias))
    xitk_labelbutton_set_state(kbedit->alias, 0);

  if(xitk_labelbutton_get_state(kbedit->edit))
    xitk_labelbutton_set_state(kbedit->edit, 0);

  xitk_disable_widget(kbedit->alias);
  xitk_disable_widget(kbedit->edit);
  xitk_disable_widget(kbedit->delete);
  xitk_disable_widget(kbedit->grab);
  kbedit->ksel = NULL;
  kbedit->nsel = -1;

  xitk_label_change_label(kbedit->comment, _("Nothing selected"));
  xitk_label_change_label(kbedit->key, _("None"));

  xitk_button_set_state (kbedit->ctrl, 0);
  xitk_button_set_state (kbedit->meta, 0);
  xitk_button_set_state (kbedit->mod3, 0);
  xitk_button_set_state (kbedit->mod4, 0);
  xitk_button_set_state (kbedit->mod5, 0);
}

/*
 * Check for redundancy.
 * return: -2 on failure (null pointer passed)
 *         -1 on success
 *         >=0 if a redundant entry found (bkt array entry num).
 */
static int bkedit_check_redundancy(const kbinding_t *kbt, kbinding_entry_t *kbe) {
  int ret = -1;

  if(kbt && kbe) {
    int i;

    for(i = 0; kbt->entry[i]->action != NULL; i++) {
      if((!strcmp(kbt->entry[i]->key, kbe->key)) &&
	 (kbt->entry[i]->modifier == kbe->modifier) &&
	 (strcasecmp(kbt->entry[i]->key, "void"))) {
	return i;
      }
    }
  }
  else
    ret = -2;

  return ret;
}

static void _kbedit_free_entry (kbinding_entry_t **entry) {
  if (!*entry)
    return;
  SAFE_FREE ((*entry)->comment);
  SAFE_FREE ((*entry)->action);
  SAFE_FREE ((*entry)->key);
  SAFE_FREE (*entry);
}
  
static void _kbr_close (xui_keyedit_t *kbe) {
  if (kbe->kbr.key)
    xitk_unregister_event_handler (kbe->gui->xitk, &kbe->kbr.key);
  xitk_labelbutton_change_label (kbe->grab, _("Grab"));
  xitk_labelbutton_set_state (kbe->grab, 0);
  _kbedit_free_entry (&kbe->kbr.entry);
}

/*
 *
 */
static void kbedit_exit (xitk_widget_t *w, void *data, int state) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)state;
  if (kbedit) {
    _kbr_close (kbedit);
    kbedit->visible = 0;

    gui_save_window_pos (kbedit->gui, "kbedit", kbedit->kreg);
    xitk_unregister_event_handler (kbedit->gui->xitk, &kbedit->kreg);
    xitk_window_destroy_window (kbedit->xwin);
    kbedit->xwin = NULL;

    {
      int i;

      for(i = 0; i < kbedit->num_entries; i++) {
	free((char *)kbedit->entries[i]);
	free((char *)kbedit->shortcuts[i]);
      }

      free((char **)kbedit->entries);
      free((char **)kbedit->shortcuts);
    }

    kbindings_free_kbinding(&kbedit->kbt);

    kbedit->gui->ssaver_enabled = 1;

    video_window_set_input_focus (kbedit->gui->vwin);

    kbedit->gui->keyedit = NULL;
    free (kbedit);
  }
}

/*
 *
 */
static void kbedit_sel(xitk_widget_t *w, void *data, int s, int modifier) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)modifier;
  _kbr_close (kbedit);
  if(s >= 0)
    kbedit_select (kbedit, s);
}

/*
 * Create an alias from the selected entry.
 */
static void kbedit_alias(xitk_widget_t *w, void *data, int state, int modifier) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)modifier;
  _kbr_close (kbedit);
  xitk_labelbutton_set_state(kbedit->edit, 0);

  if(state) {
    xitk_enable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_ALIASING;
  }
  else {
    xitk_disable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_NOOP;
  }
}

/*
 * Change shortcut, should take care about reduncancy.
 */
static void kbedit_edit(xitk_widget_t *w, void *data, int state, int modifier) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)modifier;
  _kbr_close (kbedit);
  xitk_labelbutton_set_state(kbedit->alias, 0);

  if(state) {
    xitk_enable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_EDITING;
  }
  else {
    xitk_disable_widget(kbedit->grab);
    kbedit->action_wanted = KBEDIT_NOOP;
  }

}

/*
 * Remove selected entry, but alias ones only.
 */
static void kbedit_delete (xitk_widget_t *w, void *data, int state) {
  xui_keyedit_t *kbedit = data;
  int s = xitk_browser_get_current_selected(kbedit->browser);

  (void)w;
  (void)state;
  _kbr_close (kbedit);
  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);

  /* We can delete alias entries, only */
  if(s >= 0) {

    if(kbedit->kbt->entry[s]->is_alias) {
      xitk_browser_release_all_buttons(kbedit->browser);

      free(kbedit->kbt->entry[s]->comment);
      free(kbedit->kbt->entry[s]->action);
      free(kbedit->kbt->entry[s]->key);
      free(kbedit->kbt->entry[s]);


      for(; s < kbedit->num_entries; s++)
	kbedit->kbt->entry[s] = kbedit->kbt->entry[s + 1];

      kbedit->kbt->entry[s]->comment = NULL;
      kbedit->kbt->entry[s]->action  = NULL;
      kbedit->kbt->entry[s]->key     = NULL;

      kbedit->kbt->num_entries--;

      kbedit_create_browser_entries (kbedit);

      xitk_browser_update_list(kbedit->browser,
			       (const char* const*) kbedit->entries,
			       (const char* const*) kbedit->shortcuts, kbedit->num_entries, xitk_browser_get_current_start(kbedit->browser));
    }
    else
      gui_msg (gGui, XUI_MSG_ERROR, _("You can only delete alias entries."));

  }
}

/*
 * Reset to xine's default table.
 */
static void kbedit_reset (xitk_widget_t *w, void *data, int state) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)state;
  _kbr_close (kbedit);
  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);

  kbindings_reset_kbinding(kbedit->kbt);
  kbedit_create_browser_entries (kbedit);
  xitk_browser_update_list (kbedit->browser, (const char * const *) kbedit->entries,
    (const char * const *)kbedit->shortcuts, kbedit->num_entries, xitk_browser_get_current_start (kbedit->browser));
}

/*
 * Save keymap file, then set global table to hacked one
 */
static void kbedit_save (xitk_widget_t *w, void *data, int state) {
  xui_keyedit_t *kbedit = data;

  (void)w;
  (void)state;
  _kbr_close (kbedit);
  xitk_labelbutton_set_state(kbedit->alias, 0);
  xitk_labelbutton_set_state(kbedit->edit, 0);
  xitk_disable_widget(kbedit->grab);

  kbindings_free_kbinding (&kbedit->gui->kbindings);
  kbedit->gui->kbindings = _kbindings_duplicate_kbindings (kbedit->kbt);
  kbindings_save_kbinding (kbedit->gui->kbindings);
}

/*
 * Forget and dismiss kbeditor
 */
void kbedit_end (xui_keyedit_t *kbedit) {
  kbedit_exit (NULL, kbedit, 0);
}


static void _kbedit_store_1 (xui_keyedit_t *kbe) {
  switch (kbe->action_wanted) {
    case KBEDIT_ALIASING:
      if (kbe->kbt->num_entries >= MAX_ENTRIES) {
        gui_msg (gGui, XUI_MSG_ERROR, _("No more space for additional entries!"));
        return;
      }
      kbe->kbr.entry->is_alias = 1;
      kbe->kbt->entry[kbe->kbt->num_entries] = kbe->kbt->entry[kbe->kbt->num_entries - 1];
      kbe->kbt->entry[kbe->kbt->num_entries - 1] = kbe->kbr.entry;
      kbe->kbr.entry = NULL;
      kbe->kbt->num_entries++;
      kbedit_create_browser_entries (kbe);
      xitk_browser_update_list (kbe->browser, (const char * const *) kbe->entries,
        (const char * const *) kbe->shortcuts, kbe->num_entries, xitk_browser_get_current_start (kbe->browser));
      break;
    case KBEDIT_EDITING:
      kbe->kbr.entry->is_alias = kbe->ksel->is_alias;
      kbe->kbt->entry[kbe->nsel] = kbe->kbr.entry;
      _kbedit_free_entry (&kbe->ksel);
      kbe->ksel = kbe->kbr.entry;
      kbe->kbr.entry = NULL;
      kbedit_create_browser_entries (kbe);
      xitk_browser_update_list (kbe->browser, (const char * const *) kbe->entries,
        (const char * const *) kbe->shortcuts, kbe->num_entries, xitk_browser_get_current_start (kbe->browser));
      break;
  }
  kbe->action_wanted = KBEDIT_NOOP;
  kbedit_unset (kbe);
}


/*
 * Grab key binding.
 */

static int kbr_event (void *data, const xitk_be_event_t *e) {
  xui_keyedit_t *kbe = data;

  if (e->type == XITK_EV_DEL_WIN) {
    _kbr_close (kbe);
    kbe->grabbing = 0;
    return 1;
  }

  if (e->type == XITK_EV_KEY_DOWN) {
    kbe->grabbing = 2;
    return 1;
  }

  if (e->type == XITK_EV_KEY_UP) {
    int redundant;
    char name[64];

    /* 1. user hits grab button with ENTER/SPACE,
     * 2. grab window opens,
     * 3. we get the ENTER/SPACE key up here, and
     * 4. ignore it :-)) */
    if (kbe->grabbing < 2)
      return 0;
    if (xitk_be_event_name (e, name, sizeof(name)) < 1)
      return 0;

    _kbr_close (kbe);
    xitk_labelbutton_set_state (kbe->grab, 0);
    kbe->grabbing = 0;

    kbe->kbr.entry = calloc (1, sizeof (*kbe->kbr.entry));
    if (!kbe->kbr.entry)
        return 1;
    kbe->kbr.entry->comment   = strdup (kbe->ksel->comment);
    kbe->kbr.entry->action    = strdup (kbe->ksel->action);
    kbe->kbr.entry->action_id = kbe->ksel->action_id;
    kbe->kbr.entry->is_alias  = kbe->ksel->is_alias;
    kbe->kbr.entry->is_gui    = kbe->ksel->is_gui;
    kbe->kbr.entry->key       = strdup(name);
    kbindings_convert_modifier (e->qual, &kbe->kbr.entry->modifier);

    redundant = bkedit_check_redundancy (kbe->kbt, kbe->kbr.entry);
    if (redundant >= 0) {
      /* error, redundant */
      gui_msg (kbe->gui, XUI_MSG_ERROR, _("This key binding is redundant with action:\n\"%s\".\n"),
        kbe->kbt->entry[redundant]->comment);
      _kbedit_free_entry (&kbe->kbr.entry);
      return 1;
    }

    kbedit_display_kbinding (kbe, xitk_label_get_label (kbe->comment), kbe->kbr.entry);
    xitk_labelbutton_change_label (kbe->grab, _("Store new key binding"));
    return 1;
  }

  return 0;
}

static void kbr_delete (void *data) {
  xui_keyedit_t *kbe = data;

  xitk_window_destroy_window (kbe->kbr.xwin);
  kbe->kbr.xwin = NULL;
}

static void kbedit_grab (xitk_widget_t *w, void *data, int state, int qual) {
  xui_keyedit_t *kbe = data;

  (void)w;
  (void)qual;
  if (!kbe)
    return;

  if (kbe->kbr.key) {
    if (!state)
      _kbr_close (kbe);
    return;
  }

  if (kbe->kbr.entry) {
    if (state) {
      _kbedit_store_1 (kbe);
      _kbr_close (kbe);
    }
    return;
  }

  if (!state)
    return;

  kbe->grabbing = 1;
  /* xitk_labelbutton_change_label (kbe->grab, _("Press Keyboard Keys...")); */
  {
    int x, y, w, h;
    xitk_window_get_window_position (kbe->xwin, &x, &y, &w, &h);
    kbe->kbr.xwin = xitk_window_create_dialog_window (kbe->gui->xitk,
      _("Press keyboard keys to bind..."), x, y + h, w, 50);
  }
  set_window_states_start (kbe->gui, kbe->kbr.xwin);
  xitk_window_flags (kbe->kbr.xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
  xitk_window_raise_window (kbe->kbr.xwin);
  xitk_window_set_input_focus (kbe->kbr.xwin);
  kbe->kbr.key = xitk_be_register_event_handler ("keybinding recorder", kbe->kbr.xwin, kbr_event, kbe, kbr_delete, kbe);
}

/*
 *
 */

static void _kbedit_unset (xui_keyedit_t *kbedit) {
  xitk_widget_t *w;

  if (!kbedit || !kbedit->widget_list)
    return;

  w = xitk_get_focused_widget(kbedit->widget_list);

  if ((w && (w != kbedit->done) && (w != kbedit->save))
      && (xitk_browser_get_current_selected(kbedit->browser) < 0)) {
    kbedit_unset (kbedit);
  }
}

static int kbedit_event (void *data, const xitk_be_event_t *e) {
  xui_keyedit_t *kbedit = data;

  if ((e->type == XITK_EV_BUTTON_UP) || (e->type == XITK_EV_KEY_UP)) {
    _kbedit_unset (kbedit);
    return 1;
  }
  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    kbedit_exit (NULL, kbedit, 0);
    return 1;
  }
  return 0;
}

/* needed to turn button into checkbox */
static void kbedit_dummy (xitk_widget_t *w, void *data, int state) {
  (void)w;
  (void)data;
  (void)state;
}

/*
 *
 */
void kbedit_window (gGui_t *gui) {
  int                        x, y, y1;
  xitk_image_t              *bg;
  xitk_labelbutton_widget_t  lb;
  xitk_label_widget_t        l;
  xitk_browser_widget_t      br;
  xitk_button_widget_t       b;
  int                        btnw = 80;
  int                        fontheight;
  xitk_font_t               *fs;
  xitk_widget_t             *w;
  xui_keyedit_t             *kbedit;

  if (!gui)
    return;
  if (gui->keyedit)
    return;

  x = y = 80;
  gui_load_window_pos (gui, "kbedit", &x, &y);

  kbedit = (xui_keyedit_t *)calloc (1, sizeof (*kbedit));
  if (!kbedit)
    return;
  kbedit->gui = gui;
  kbedit->gui->keyedit = kbedit;

  kbedit->kbt           = _kbindings_duplicate_kbindings(gui->kbindings);
  kbedit->action_wanted = KBEDIT_NOOP;
  kbedit->xwin          = xitk_window_create_dialog_window(gui->xitk,
							   _("Key Binding Editor"),
							   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  set_window_states_start(gui, kbedit->xwin);

  kbedit->widget_list = xitk_window_widget_list(kbedit->xwin);

  bg = xitk_window_get_background_image (kbedit->xwin);

  x = 15;
  y = 34;

  xitk_image_draw_rectangular_box (bg, x, y, WINDOW_WIDTH - 30, MAX_DISP_ENTRIES * 20 + 16 + 10, DRAW_INNER);

  y += MAX_DISP_ENTRIES * 20 + 16 + 10 + 30;
  y1 = y; /* remember for later */
  xitk_image_draw_outter_frame (bg,
		    _("Binding Action"), hboldfontname,
		    x, y,
		    (WINDOW_WIDTH - 30), 45);

  y += 45 + 3;
  xitk_image_draw_outter_frame (bg,
		    _("Key"), hboldfontname,
		    x, y,
		    120, 45);

  xitk_image_draw_outter_frame (bg,
		    _("Modifiers"), hboldfontname,
		    x + 130, y,
		    (WINDOW_WIDTH - (x + 130) - 15), 45);


  xitk_window_set_background_image (kbedit->xwin, bg);

  kbedit_create_browser_entries (kbedit);

  y = 34;

  XITK_WIDGET_INIT(&br);

  br.arrow_up.skin_element_name    = NULL;
  br.slider.skin_element_name      = NULL;
  br.arrow_dn.skin_element_name    = NULL;
  br.browser.skin_element_name     = NULL;
  br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
  br.browser.num_entries           = kbedit->num_entries;
  br.browser.entries               = (const char* const*)kbedit->entries;
  br.callback                      = kbedit_sel;
  br.dbl_click_callback            = NULL;
  br.userdata                      = kbedit;
  kbedit->browser = xitk_noskin_browser_create (kbedit->widget_list, &br,
    x + 5, y + 5, WINDOW_WIDTH - (30 + 10 + 16), 20, 16, br_fontname);
  xitk_add_widget (kbedit->widget_list, kbedit->browser);
  xitk_enable_and_show_widget(kbedit->browser);

  xitk_browser_set_alignment(kbedit->browser, ALIGN_LEFT);
  xitk_browser_update_list(kbedit->browser,
			   (const char* const*) kbedit->entries,
			   (const char* const*) kbedit->shortcuts, kbedit->num_entries, xitk_browser_get_current_start(kbedit->browser));

  y = y1 - 30 + 4;

  XITK_WIDGET_INIT(&lb);

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Alias");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = kbedit_alias;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->alias = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->alias);
  xitk_enable_and_show_widget(kbedit->alias);

  x += btnw + 4;

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Edit");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = kbedit_edit;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->edit = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->edit);
  xitk_enable_and_show_widget(kbedit->edit);

  x += btnw + 4;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Delete");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_delete;
  lb.state_callback    = NULL;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->delete = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->delete);
  xitk_enable_and_show_widget(kbedit->delete);

  x += btnw + 4;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_save;
  lb.state_callback    = NULL;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->save = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->save);
  xitk_enable_and_show_widget(kbedit->save);

  x += btnw + 4;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Reset");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_reset;
  lb.state_callback    = NULL;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  w =  xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x += btnw + 4;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Done");
  lb.align             = ALIGN_CENTER;
  lb.callback          = kbedit_exit;
  lb.state_callback    = NULL;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->done = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, btnw, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->done);
  xitk_enable_and_show_widget(kbedit->done);

  x = 15;

  XITK_WIDGET_INIT(&l);

  fs = xitk_font_load_font(gui->xitk, hboldfontname);
  fontheight = xitk_font_get_string_height(fs, " ");
  xitk_font_unload_font(fs);

  y = y1 + (45 / 2);                /* Checkbox                     */
  y1 = y - ((fontheight - 10) / 2); /* Text, v-centered to ckeckbox */

  l.skin_element_name = NULL;
  l.label             = "Binding Action";
  l.callback          = NULL;
  kbedit->comment = xitk_noskin_label_create (kbedit->widget_list, &l, x + 10, y1,
    WINDOW_WIDTH - 50 - 2, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->comment);
  xitk_enable_and_show_widget(kbedit->comment);

  y += 45 + 3;
  y1 += 45 + 3;

  l.skin_element_name = NULL;
  l.label             = "THE Key";
  l.callback          = NULL;
  kbedit->key = xitk_noskin_label_create (kbedit->widget_list, &l, x + 10, y1,
    100, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->key);
  xitk_enable_and_show_widget(kbedit->key);

  x += 130 + 10;

  XITK_WIDGET_INIT (&b);
  b.callback          = NULL;
  b.state_callback    = kbedit_dummy;
  b.userdata          = NULL;
  b.skin_element_name = "XITK_NOSKIN_CHECK";

  kbedit->ctrl = xitk_noskin_button_create (kbedit->widget_list, &b, x, y, 10, 10);
  xitk_add_widget (kbedit->widget_list, kbedit->ctrl);
  xitk_show_widget(kbedit->ctrl);
  xitk_disable_widget(kbedit->ctrl);


  x += 15;

  l.skin_element_name = NULL;
  l.label             = _("ctrl");
  l.callback          = NULL;
  w =  xitk_noskin_label_create (kbedit->widget_list, &l, x, y1, 40, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x += 55;

  kbedit->meta = xitk_noskin_button_create (kbedit->widget_list, &b, x, y, 10, 10);
  xitk_add_widget (kbedit->widget_list, kbedit->meta);
  xitk_show_widget(kbedit->meta);
  xitk_disable_widget(kbedit->meta);


  x += 15;

  l.skin_element_name = NULL;
  l.label             = _("meta");
  l.callback          = NULL;
  w =  xitk_noskin_label_create (kbedit->widget_list, &l, x, y1, 40, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x += 55;

  kbedit->mod3 = xitk_noskin_button_create (kbedit->widget_list, &b, x, y, 10, 10);
  xitk_add_widget (kbedit->widget_list, kbedit->mod3);
  xitk_show_widget(kbedit->mod3);
  xitk_disable_widget(kbedit->mod3);


  x += 15;

  l.skin_element_name = NULL;
  l.label             = _("mod3");
  l.callback          = NULL;
  w =  xitk_noskin_label_create (kbedit->widget_list, &l, x, y1, 40, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x += 55;

  kbedit->mod4 = xitk_noskin_button_create (kbedit->widget_list, &b, x, y, 10, 10);
  xitk_add_widget (kbedit->widget_list, kbedit->mod4);
  xitk_show_widget(kbedit->mod4);
  xitk_disable_widget(kbedit->mod4);


  x += 15;

  l.skin_element_name = NULL;
  l.label             = _("mod4");
  l.callback          = NULL;
  w =  xitk_noskin_label_create (kbedit->widget_list, &l, x, y1, 40, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x += 55;

  kbedit->mod5 = xitk_noskin_button_create (kbedit->widget_list, &b, x, y, 10, 10);
  xitk_add_widget (kbedit->widget_list, kbedit->mod5);
  xitk_show_widget(kbedit->mod5);
  xitk_disable_widget(kbedit->mod5);


  x += 15;

  l.skin_element_name = NULL;
  l.label             = _("mod5");
  l.callback          = NULL;
  w =  xitk_noskin_label_create (kbedit->widget_list, &l, x, y1, 40, fontheight, hboldfontname);
  xitk_add_widget (kbedit->widget_list, w);
  xitk_enable_and_show_widget(w);

  x = 15;
  y = WINDOW_HEIGHT - (23 + 15);

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Grab");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = kbedit_grab;
  lb.userdata          = kbedit;
  lb.skin_element_name = NULL;
  kbedit->grab = xitk_noskin_labelbutton_create (kbedit->widget_list, &lb, x, y, WINDOW_WIDTH - 30, 23,
    "Black", "Black", "White", hboldfontname);
  xitk_add_widget (kbedit->widget_list, kbedit->grab);
  xitk_enable_and_show_widget(kbedit->grab);

  kbedit_unset (kbedit);

  kbedit->kreg = xitk_be_register_event_handler ("kbedit", kbedit->xwin, kbedit_event, kbedit, NULL, NULL);

  kbedit->kbr.key = 0;
  kbedit->kbr.entry = NULL;
  kbedit->kbr.xwin = NULL;

  kbedit->nsel = -1;
  kbedit->ksel = NULL;

  gui->ssaver_enabled = 0;
  kbedit->visible      = 1;
  kbedit_raise_window (kbedit);

  xitk_window_set_input_focus (kbedit->xwin);
}

