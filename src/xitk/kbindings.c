/* 
 * Copyright (C) 2000-2001 the xine project
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
 *
 * $Id$
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <xine/xineutils.h>

#include "event.h"
#include "kbindings.h"
#include "xitk.h"

extern gGui_t                 *gGui;

/*
 * Handled key modifier.
 */
#define KEYMOD_NOMOD           0x00000000
#define KEYMOD_CONTROL         0x00000001
#define KEYMOD_META            0x00000002
#define KEYMOD_MOD3            0x00000010
#define KEYMOD_MOD4            0x00000020
#define KEYMOD_MOD5            0x00000040

#define SAFE_FREE(X)           if(X) free(X)

#define DEFAULT_DISPLAY_MODE   1
#define LIRC_DISPLAY_MODE      2
/*
 * Key binding entry struct.
 */
struct kbinding_entry_s {
  char             *comment;     /* Comment automatically added in xbinding_display*() outputs */
  char             *action;      /* Human readable action, used in config file too */
  action_id_t       action_id;   /* The numerical action, handled in a case statement */
  char             *key;         /* key binding */
  int               modifier;    /* Modifier key of binding (can be OR'ed) */
};

#define MAX_ENTRIES 255
struct kbinding_s {
  int               num_entries;
  kbinding_entry_t *entry[MAX_ENTRIES];
};

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
/* Same as above, used on remapping */
typedef struct {
  char             *alias;
  char             *action;
  char             *key;
  char             *modifier;
} user_kbinding_t;

/*
 * keybinding file object.
 */
typedef struct {
  FILE             *fd;
  char             *bindingfile;
  char             *ln;
  char              buf[256];
} kbinding_file_t;


#ifndef	HAVE_STRSEP
static char *
strsep(char **stringp, char *delim)
{
    char *start = *stringp;
    char *cp;
    char ch;

    if (start == NULL)
	return NULL;

    for (cp = start; ch = *cp; cp++) {
	if (strchr(delim, ch)) {
	    *cp++ = 0;
	    *stringp = cp;
	    return start;
	}
    }
    *stringp = NULL;
    return start;
}
#endif

/* 
 * Default key mapping table.
 */
static kbinding_entry_t default_binding_table[] = {
  { "Reduce the output window size.",
    "WindowReduce",           ACTID_WINDOWREDUCE            , "less",     KEYMOD_NOMOD   },
  { "Enlarge the output window size.",
    "WindowEnlarge",          ACTID_WINDOWENLARGE           , "greater",  KEYMOD_NOMOD   },
  { "Select next sub picture (subtitle) channel.",
    "SpuNext",                ACTID_SPU_NEXT                , "period",   KEYMOD_NOMOD   },
  { "Select previous sub picture (subtitle) channel.",
    "SpuPrior",               ACTID_SPU_PRIOR               , "comma",    KEYMOD_NOMOD   },
  { "Visibility toggle of control window.",
    "ControlShow",            ACTID_CONTROLSHOW             , "c",        KEYMOD_META    },
  { "Visibility toggle of output window visibility.",
    "ToggleWindowVisibility", ACTID_TOGGLE_WINOUT_VISIBLITY , "h",        KEYMOD_NOMOD   },
  { "Select next audio channel.",
    "AudioChannelNext",       ACTID_AUDIOCHAN_NEXT          , "plus",     KEYMOD_NOMOD   },
  { "Select previous audio channel.",
    "AudioChannelPrior",      ACTID_AUDIOCHAN_PRIOR         , "minus",    KEYMOD_NOMOD   },
  { "Visibility toggle of playlist editor window.",
    "PlaylistEditor",         ACTID_PLAYLIST                , "p",        KEYMOD_META    },
  { "Playback pause toggle.",
    "Pause",                  ACTID_PAUSE                   , "space",    KEYMOD_NOMOD   },
  { "Visibility toggle of UI windows.",
    "ToggleVisiblity",        ACTID_TOGGLE_VISIBLITY        , "g",        KEYMOD_NOMOD   },
  { "Fullscreen toggle.",
    "ToggleFullscreen",       ACTID_TOGGLE_FULLSCREEN       , "f",        KEYMOD_NOMOD   },
  { "Aspect ratio values toggle.",
    "ToggleAspectRatio",      ACTID_TOGGLE_ASPECT_RATIO     , "a",        KEYMOD_NOMOD   },
  { "Interlaced mode toggle.",
    "ToggleInterleave",       ACTID_TOGGLE_INTERLEAVE       , "i",        KEYMOD_NOMOD   },
  { "Quit the program.",
    "Quit",                   ACTID_QUIT                    , "q",        KEYMOD_NOMOD   },
  { "Start playback.",
    "Play",                   ACTID_PLAY                    , "Return",   KEYMOD_NOMOD   },
  { "Visibility toggle of the setup window.",
    "SetupShow",              ACTID_SETUP                   , "s",        KEYMOD_META    },
  { "Stop playback.",
    "Stop",                   ACTID_STOP                    , "S",        KEYMOD_NOMOD   },
  { "Select and play next mrl in the playlist.",
    "NextMrl",                ACTID_MRL_NEXT                , "Next",     KEYMOD_NOMOD   },
  { "Select and play previous mrl in the playlist.",
    "PriorMrl",               ACTID_MRL_PRIOR               , "Prior",    KEYMOD_NOMOD   },
  { "Eject the current medium.",
    "Eject",                  ACTID_EJECT                   , "e",        KEYMOD_NOMOD   },
  { "Set position to beginning of current stream.",
    "SetPosition0%",          ACTID_SET_CURPOS_0            , "0",        KEYMOD_NOMOD   },
  { "Set position to 10%% of current stream.",
    "SetPosition10%",         ACTID_SET_CURPOS_10           , "1",        KEYMOD_NOMOD   },
  { "Set position to 20%% of current stream.",
    "SetPosition20%",         ACTID_SET_CURPOS_20           , "2",        KEYMOD_NOMOD   },
  { "Set position to 30%% of current stream.",
    "SetPosition30%",         ACTID_SET_CURPOS_30           , "3",        KEYMOD_NOMOD   },
  { "Set position to 40%% of current stream.",
    "SetPosition40%",         ACTID_SET_CURPOS_40           , "4",        KEYMOD_NOMOD   },
  { "Set position to 50%% of current stream.",
    "SetPosition50%",         ACTID_SET_CURPOS_50           , "5",        KEYMOD_NOMOD   },
  { "Set position to 60%% of current stream.",
    "SetPosition60%",         ACTID_SET_CURPOS_60           , "6",        KEYMOD_NOMOD   },
  { "Set position to 70%% of current stream.",
    "SetPosition70%",         ACTID_SET_CURPOS_70           , "7",        KEYMOD_NOMOD   },
  { "Set position to 80%% of current stream.",
    "SetPosition80%",         ACTID_SET_CURPOS_80           , "8",        KEYMOD_NOMOD   },
  { "Set position to 90%% of current stream.",
    "SetPosition90%",         ACTID_SET_CURPOS_90           , "9",        KEYMOD_NOMOD   },
  { "Set position to -60 seconds in current stream.",
    "SeekRelative-60",        ACTID_SEEK_REL_m60            , "Left",     KEYMOD_NOMOD   }, 
  { "Set position to +60 seconds in current stream.",
    "SeekRelative+60",        ACTID_SEEK_REL_p60            , "Right",    KEYMOD_NOMOD   },
  { "Set position to -15 seconds in current stream.",
    "SeekRelative-15",        ACTID_SEEK_REL_m15            , "Left",     KEYMOD_CONTROL },
  { "Set position to +15 seconds in current stream.",
    "SeekRelative+15",        ACTID_SEEK_REL_p15            , "Right",    KEYMOD_CONTROL },
  { "Visibility toggle of mrl browser window.",
    "MrlBrowser",             ACTID_MRLBROWSER              , "m",        KEYMOD_META    },
  { "Audio muting toggle.",
    "Mute",                   ACTID_MUTE                    , "m",        KEYMOD_CONTROL },
  { "Change audio syncing.",
    "AudioVideoDecay+",       ACTID_AV_SYNC_p3600           , "m",        KEYMOD_NOMOD   },
  { "Change audio syncing.",
    "AudioVideoDecay-",       ACTID_AV_SYNC_m3600           , "n",        KEYMOD_NOMOD   },
  { "Reset audio video syncing offset.",
    "AudioVideoDecayReset",   ACTID_AV_SYNC_RESET           , "Home",     KEYMOD_NOMOD   },
  { "Increment playback speed.",
    "SpeedFaster",            ACTID_SPEED_FAST              , "Up",       KEYMOD_NOMOD   },
  { "Decrement playback speed.",
    "SpeedSlower",            ACTID_SPEED_SLOW              , "Down",     KEYMOD_NOMOD   },
  { "Increment audio volume.",
    "Volume+",                ACTID_pVOLUME                 , "V",        KEYMOD_NOMOD   },
  { "Decrement audio volume.",
    "Volume-",                ACTID_mVOLUME                 , "v",        KEYMOD_NOMOD   },
  { "Take a snapshot (Internal image fetch and save).",
    "Snapshot",               ACTID_SNAPSHOT                , "t",        KEYMOD_NOMOD   },
  { "Resize output window to stream size1:1.",
    "Zoom1:1",                ACTID_ZOOM_1_1                , "s",        KEYMOD_NOMOD   },
  { "Grab pointer toggle.",
    "GrabPointer",            ACTID_GRAB_POINTER            , "Insert",   KEYMOD_NOMOD   },
  { "Menu 1 event.",
    "EventMenu1",             ACTID_EVENT_MENU1             , "Escape",   KEYMOD_NOMOD   },
  { "Menu 2 event.",
    "EventMenu2",             ACTID_EVENT_MENU2             , "F1",       KEYMOD_NOMOD   },
  { "Menu 3 event.",
    "EventMenu3",             ACTID_EVENT_MENU3             , "F2",       KEYMOD_NOMOD   },
  { "Up event.",
    "EventUp",                ACTID_EVENT_UP                , "KP_Up",    KEYMOD_NOMOD   },
  { "Down event.",
    "EventDown",              ACTID_EVENT_DOWN              , "KP_Down",  KEYMOD_NOMOD   },
  { "Left event.",
    "EventLeft",              ACTID_EVENT_LEFT              , "KP_Left",  KEYMOD_NOMOD   },
  { "Right event.",
    "EventRight",             ACTID_EVENT_RIGHT             , "KP_Right", KEYMOD_NOMOD   },
  { "Previous event.",
    "EventPrior",             ACTID_EVENT_PRIOR             , "KP_Prior", KEYMOD_NOMOD   },
  { "Next event.",
    "EventNext",              ACTID_EVENT_NEXT              , "KP_Next",  KEYMOD_NOMOD   },
  { "Previous angle event.",
    "EventAnglePrior",        ACTID_EVENT_ANGLE_PRIOR       , "KP_End",     KEYMOD_NOMOD   },
  { "Next angle event.",
    "EventAngleNext",         ACTID_EVENT_ANGLE_NEXT        , "KP_Home",     KEYMOD_NOMOD   },
  { "Select event.",
    "EventSelect",            ACTID_EVENT_SELECT            , "KP_Enter", KEYMOD_NOMOD   },
  { "Zoom into video.",
    "ZoomIn",                 ACTID_ZOOM_IN                 , "z",        KEYMOD_NOMOD   },
  { "Zoom out of video.",
    "ZoomOut",                ACTID_ZOOM_OUT                , "Z",        KEYMOD_NOMOD   },
  { "Reset zooming.",
    "ZoomReset",              ACTID_ZOOM_RESET              , "z",        KEYMOD_CONTROL | KEYMOD_META    },
  { "Toggle TV modes on the DXR3",
    "ToggleTVmode",           ACTID_TOGGLE_TVMODE	    , "o",	  KEYMOD_CONTROL },
  { "Visibility toggle of log viewer",
    "ViewlogShow",            ACTID_VIEWLOG	            , "l",	  KEYMOD_META    },
  { 0,
    0,                        0,                            0,            0              }
};

/*
 * Check if there some redundant entries in key binding table kbt.
 */
static void _kbindings_check_redundancy(kbinding_t *kbt) {
  int i, j, found = -1;

  if(kbt == NULL)
    return;

  for(i = 0; kbt->entry[i]->action != NULL; i++) {
    for(j = 0; kbt->entry[j]->action != NULL; j++) {
      if(i != j && j != found) {
	if((!strcmp(kbt->entry[i]->key, kbt->entry[j]->key)) &&
	   (kbt->entry[i]->modifier == kbt->entry[j]->modifier)) {
	  found = i;
	  xine_error(_("Key bindings of '%s' and '%s' are similar.\n"),
		     kbt->entry[i]->action, kbt->entry[j]->action);
	}
      }
    }
  }
  
}

/*
 * Free a keybindings object.
 */
static void _kbindings_free_bindings(kbinding_t *kbt) {
  kbinding_entry_t **k;
  int                i;
  
  if(kbt == NULL)
    return;

  k = kbt->entry;
  for(i = (kbt->num_entries - 1); i > 0; i--) {
    SAFE_FREE(k[i]->comment);
    SAFE_FREE(k[i]->action);
    SAFE_FREE(k[i]->key);
    SAFE_FREE(k[i]);
  }
  SAFE_FREE(kbt);
}

/*
 * Return position in str of char 'c'. -1 if not found
 */
static int _is_there_char(const char *str, int c) {
  char *p;

  if(str)
    if((p = strrchr(str, c)) != NULL) {
      return (p - str);
    }
  
  return -1;
}

/*
 * Return >= 0 if it's begin of section, otherwise -1
 */
static int _kbindings_begin_section(kbinding_file_t *kbdf) {

  assert(kbdf != NULL);

  return _is_there_char(kbdf->ln, '{');
}

/*
 * Return >= 0 if it's end of section, otherwise -1
 */
static int _kbindings_end_section(kbinding_file_t *kbdf) {

  assert(kbdf != NULL);

  return _is_there_char(kbdf->ln, '}');
}

/*
 * Cleanup end of line.
 */
static void _kbindings_clean_eol(kbinding_file_t *kbdf) {
  char *p;

  assert(kbdf != NULL);

  p = kbdf->ln;

  if(p) {
    while(*p != '\0') {
      if(*p == '\n' || *p == '\r') {
	*p = '\0';
	break;
      }
      p++;
    }

    while(p > kbdf->ln) {
      --p;
      
      if(*p == ' ' || *p == '\t') 
	*p = '\0';
      else
	break;
    }
  }
}

/*
 * Read the next line of the key binding file.
 */
static void _kbindings_get_next_line(kbinding_file_t *kbdf) {

  assert(kbdf != NULL && kbdf->fd != NULL);
  
 __get_next_line:

  kbdf->ln = fgets(kbdf->buf, 255, kbdf->fd);

  while(kbdf->ln && (*kbdf->ln == ' ' || *kbdf->ln == '\t')) ++kbdf->ln;

  if(kbdf->ln) {
    if((strncmp(kbdf->ln, "//", 2) == 0) ||
       (strncmp(kbdf->ln, "/*", 2) == 0) || /**/
       (strncmp(kbdf->ln, ";", 1) == 0) ||
       (strncmp(kbdf->ln, "#", 1) == 0) ||
       (*kbdf->ln == '\0')) {
      goto __get_next_line;
    }

  }

  _kbindings_clean_eol(kbdf);
}

/*
 * move forward p pointer till non space or tab.
 */
static void _kbindings_set_pos_to_next_char(char **p) {

  assert(*p != NULL);

  while(*(*p) != '\0' && (*(*p) == ' ' || *(*p) == '\t')) ++(*p);
}

/*
 * Position p pointer to value.
 */
static void _kbindings_set_pos_to_value(char **p) {
  
  assert(*p != NULL);

  while(*(*p) != '\0' && *(*p) != '=' && *(*p) != ':' && *(*p) != '{') ++(*p);
  while(*(*p) == '=' || *(*p) == ':' || *(*p) == ' ' || *(*p) == '\t') ++(*p);
}

/*
 * Add an entry in key binding table kbt. Called when an Alias entry
 * is found.
 */
static void _kbindings_add_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  kbinding_entry_t  *k;
  int                modifier;

  assert(kbt != NULL && ukb != NULL);

  k = kbindings_lookup_action(kbt, ukb->alias);
  if(k) {
    
    modifier = k->modifier;
    
    if(ukb->modifier) {
      char *p;
      
      modifier = KEYMOD_NOMOD;

      while((p = strsep(&ukb->modifier, ",")) != NULL) {
	
	_kbindings_set_pos_to_next_char(&p);
	
	if(p) {
	  
	  if(!strcasecmp(p, "none"))
	    modifier = KEYMOD_NOMOD;
	  else if(!strcasecmp(p, "control"))
	    modifier |= KEYMOD_CONTROL;
	  else if(!strcasecmp(p, "ctrl"))
	    modifier |= KEYMOD_CONTROL;
	  else if(!strcasecmp(p, "meta"))
	    modifier |= KEYMOD_META;
	  else if(!strcasecmp(p, "alt"))
	    modifier |= KEYMOD_META;
	  else if(!strcasecmp(p, "mod3"))
	    modifier |= KEYMOD_MOD3;
	  else if(!strcasecmp(p, "mod4"))
	    modifier |= KEYMOD_MOD4;
	  else if(!strcasecmp(p, "mod5"))
	    modifier |= KEYMOD_MOD5;
	}
      }
    }

    /*
     * Add new entry (struct memory already allocated)
     */
    kbt->entry[kbt->num_entries - 1]->comment = strdup(k->comment);
    kbt->entry[kbt->num_entries - 1]->action = strdup(k->action);
    kbt->entry[kbt->num_entries - 1]->action_id = k->action_id;
    kbt->entry[kbt->num_entries - 1]->key = strdup(ukb->key);
    kbt->entry[kbt->num_entries - 1]->modifier = modifier;
    
    /*
     * NULL terminate array.
     */
    kbt->entry[kbt->num_entries] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_t));
    kbt->entry[kbt->num_entries]->comment = NULL;
    kbt->entry[kbt->num_entries]->action = NULL;
    kbt->entry[kbt->num_entries]->action_id = 0;
    kbt->entry[kbt->num_entries]->key = NULL;
    kbt->entry[kbt->num_entries]->modifier = 0;
  
    kbt->num_entries++;

  }
}

/*
 * Change keystroke of modifier in entry.
 */
static void _kbindings_replace_entry(kbinding_t *kbt, user_kbinding_t *ukb) {
  int i;

  assert(kbt != NULL && ukb != NULL);

  for(i = 0; kbt->entry[i]->action != NULL; i++) {

    if(!strcmp(kbt->entry[i]->action, ukb->action)) {
      SAFE_FREE(kbt->entry[i]->key);
      kbt->entry[i]->key = strdup(ukb->key);

      if(ukb->modifier) {
	char *p;
	int   modifier = KEYMOD_NOMOD;
	
	while((p = strsep(&ukb->modifier, ",")) != NULL) {

	  _kbindings_set_pos_to_next_char(&p);
	  
	  if(p) {
	    
	    if(!strcasecmp(p, "none"))
	      modifier = KEYMOD_NOMOD;
	    else if(!strcasecmp(p, "control"))
	      modifier |= KEYMOD_CONTROL;
	    else if(!strcasecmp(p, "ctrl"))
	      modifier |= KEYMOD_CONTROL;
	    else if(!strcasecmp(p, "meta"))
	      modifier |= KEYMOD_META;
	    else if(!strcasecmp(p, "alt"))
	      modifier |= KEYMOD_META;
	    else if(!strcasecmp(p, "mod3"))
	      modifier |= KEYMOD_MOD3;
	    else if(!strcasecmp(p, "mod4"))
	      modifier |= KEYMOD_MOD4;
	    else if(!strcasecmp(p, "mod5"))
	      modifier |= KEYMOD_MOD5;
	  }
	}
	kbt->entry[i]->modifier = modifier;	
      }
      return;
    }
  }
}

/*
 * Read key remap section (try to).
 */
static void _kbindings_parse_section(kbinding_t *kbt, kbinding_file_t *kbdf) {
  int               brace_offset;
  user_kbinding_t  *ukb;
  char              *p;
  
  assert(kbt != NULL && kbdf != NULL);

  if((kbdf->ln != NULL) && (*kbdf->ln != '\0')) {
    
    ukb = (user_kbinding_t *) xine_xmalloc(sizeof(user_kbinding_t));
  
    p = kbdf->ln;
    
    if((brace_offset = _kbindings_begin_section(kbdf)) >= 0) {
      *(kbdf->ln + brace_offset) = '\0';
      _kbindings_clean_eol(kbdf);
      

      ukb->action = strdup(kbdf->ln);
  
      while(_kbindings_end_section(kbdf) < 0) {
	
	_kbindings_get_next_line(kbdf);
	p = kbdf->ln;
	if(kbdf->ln != NULL) {
	  if(!strncasecmp(kbdf->ln, "modifier", 8)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb->modifier = strdup(p);
	  }
	  if(!strncasecmp(kbdf->ln, "entry", 5)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb->alias = strdup(p);
	  }
	  else if(!strncasecmp(kbdf->ln, "key", 3)) {
	    _kbindings_set_pos_to_value(&p);
	    ukb->key = strdup(p);
	  }	  
	}
      }

      if(ukb && ukb->alias && ukb->action && ukb->key) {
	_kbindings_add_entry(kbt, ukb);
      }
      else if(ukb && ukb->action && ukb->key) {
	_kbindings_replace_entry(kbt, ukb);
      }

    }

    SAFE_FREE(ukb->alias);
    SAFE_FREE(ukb->action);
    SAFE_FREE(ukb->key);
    SAFE_FREE(ukb->modifier);
    SAFE_FREE(ukb);
  }
}

/*
 * Read and parse remap key binding file, if available.
 */
static void _kbinding_load_config(kbinding_t *kbt, char *file) {
  kbinding_file_t *kbdf;
  
  assert(kbt != NULL && file != NULL);

  kbdf = (kbinding_file_t *) xine_xmalloc(sizeof(kbinding_file_t));
  kbdf->bindingfile = strdup(file);

  if((kbdf->fd = fopen(kbdf->bindingfile, "r")) != NULL) {
    
    _kbindings_get_next_line(kbdf);

    while(kbdf->ln != NULL) {

      if(_kbindings_begin_section(kbdf)) {
	_kbindings_parse_section(kbt, kbdf);
      }
      
      _kbindings_get_next_line(kbdf);
    }
    fclose(kbdf->fd);
  }

  SAFE_FREE(kbdf->bindingfile);
  SAFE_FREE(kbdf);
}

static void _kbindings_display_kbindings_to_stream(kbinding_t *kbt, int mode, FILE *stream) {
  char               buf[256];
  kbinding_entry_t **k;
  int          i;

  if(kbt == NULL) {
    xine_error(_("OOCH: key binding table is NULL.\n"));
    return;
  }

  switch(mode) {
  case LIRC_DISPLAY_MODE:
    fprintf(stream, "##\n# Xine key bindings.\n");
    fprintf(stream, "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);
    
    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      sprintf(buf, "# %s\n", k[i]->comment);
      sprintf(buf, "%sbegin\n", buf);
      sprintf(buf, "%s\tremote = xxxxx\n", buf);
      sprintf(buf, "%s\tbutton = xxxxx\n", buf);
      sprintf(buf, "%s\tprog   = xine\n", buf);
      sprintf(buf, "%s\trepeat = 0\n", buf);
      sprintf(buf, "%s\tconfig = %s\n", buf, k[i]->action);
      sprintf(buf, "%send\n\n", buf);
      fprintf(stream, buf);
      memset(&buf, 0, sizeof(buf));
    }
    fprintf(stream, "##\n# End of Xine key bindings.\n##\n");
    break;
    
  case DEFAULT_DISPLAY_MODE:
  default:
    fprintf(stream, "##\n# Xine key bindings.\n");
    fprintf(stream, "# Automatically generated by %s version %s.\n##\n\n", PACKAGE, VERSION);

    for(k = kbt->entry, i = 0; k[i]->action != NULL; i++) {
      sprintf(buf, "# %s\n", k[i]->comment);
      sprintf(buf, "%s%s {\n", buf, k[i]->action);
      sprintf(buf, "%s\tkey = %s\n", buf, k[i]->key);
      sprintf(buf, "%s%s", buf, "\tmodifier = ");
      if(k[i]->modifier == KEYMOD_NOMOD)
	sprintf(buf, "%s%s, ", buf, "none");
      if(k[i]->modifier & KEYMOD_CONTROL)
	sprintf(buf, "%s%s, ", buf, "control");
      if(k[i]->modifier & KEYMOD_META)
	sprintf(buf, "%s%s, ", buf, "meta");
      if(k[i]->modifier & KEYMOD_MOD3)
	sprintf(buf, "%s%s, ", buf, "mod3");
      if(k[i]->modifier & KEYMOD_MOD4)
	sprintf(buf, "%s%s, ", buf, "mod4");
      if(k[i]->modifier & KEYMOD_MOD5)
	sprintf(buf, "%s%s, ", buf, "mod5");
      buf[strlen(buf) - 2] = '\n';
      buf[strlen(buf) - 1] = '\0';
      sprintf(buf, "%s%s", buf, "}\n\n");
      fprintf(stream, buf);
      memset(&buf, 0, sizeof(buf));
    }
    fprintf(stream, "##\n# End of Xine key bindings.\n##\n");
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
  *modifier = KEYMOD_NOMOD;
  
  assert(modifier != NULL);

  if(mod & MODIFIER_NOMOD)
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
 * Create a new key binding table from (static) default one.
 */
static kbinding_t *_kbindings_init_to_default(void) {
  kbinding_t  *kbt;
  int          i;
 
  kbt = (kbinding_t *) xine_xmalloc(sizeof(kbinding_t));

  for(i = 0; default_binding_table[i].action != NULL; i++) {
    kbt->entry[i] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_entry_t));
    kbt->entry[i]->comment = strdup(default_binding_table[i].comment);
    kbt->entry[i]->action = strdup(default_binding_table[i].action);
    kbt->entry[i]->action_id = default_binding_table[i].action_id;
    kbt->entry[i]->key = strdup(default_binding_table[i].key);
    kbt->entry[i]->modifier = default_binding_table[i].modifier;
  }
  kbt->entry[i] = (kbinding_entry_t *) xine_xmalloc(sizeof(kbinding_t));
  kbt->entry[i]->comment = NULL;
  kbt->entry[i]->action = NULL;
  kbt->entry[i]->action_id = 0;
  kbt->entry[i]->key = NULL;
  kbt->entry[i]->modifier = 0;
  
  kbt->num_entries = i+1;

  return kbt;
}

/*
 * Initialize a key binding table from default, then try
 * to remap this one with (system/user) remap files.
 */
kbinding_t *kbindings_init_kbinding(void) {
  kbinding_t *kbt = NULL;
  char        buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];

  kbt = _kbindings_init_to_default();

  /* read and parse user file */
  snprintf(buf, sizeof(buf), "%s/.xine/keymap", xine_get_homedir());
  _kbinding_load_config(kbt, buf);

  /* Just to check is there redundant entries, and inform user */
  _kbindings_check_redundancy(kbt);

  return kbt;
}

/*
 * Save current key binding table to keymap file.
 */
void kbindings_save_kbinding(kbinding_t *kbt) {
  FILE   *f;
  char    buf[XITK_PATH_MAX + XITK_NAME_MAX + 1];
  
  sprintf(buf, "%s/.xine/%s", xine_get_homedir(), "keymap");
  if((f = fopen(buf, "w")) == NULL) {
    return;
  }
  else {
    _kbindings_display_kbindings_to_stream(kbt, DEFAULT_DISPLAY_MODE, f);  
    fclose(f);
  }
}

/*
 * Free key binding table kbt, then set it to default table.
 */
void kbindings_reset_kbinding(kbinding_t **kbt) {
  kbinding_t *k;
  
  assert(*kbt != NULL);

  _kbindings_free_bindings(*kbt);
  k = _kbindings_init_to_default();
  *kbt = k;
}

/*
 * Freeing key binding table, then NULLify it.
 */
void kbindings_free_kbinding(kbinding_t **kbt) {

  assert(*kbt != NULL);

  _kbindings_free_bindings(*kbt);
  *kbt = NULL;
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

  assert(kbt != NULL);

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

/*
 * Return a key binding entry (if available) matching with action string.
 */
kbinding_entry_t *kbindings_lookup_action(kbinding_t *kbt, const char *action) {
  kbinding_entry_t  *k;
  int                i;

  if((action == NULL) || (kbt == NULL))
    return NULL;

  /* CHECKME: Not case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if(!strcasecmp(k->action, action))
      return k;
  }

  /* Unknown action */
  return NULL;
}

/*
 * Try to find and entry in key binding table matching with key and modifier value.
 */
static kbinding_entry_t *kbindings_lookup_binding(kbinding_t *kbt, const char *key, int modifier) {
  kbinding_entry_t *k;
  int               i;

  if((key == NULL) || (kbt == NULL))
    return NULL;

  /*
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
  */

  /* Be case sensitive */
  for(i = 0, k = kbt->entry[0]; kbt->entry[i]->action != NULL; i++, k = kbt->entry[i]) {
    if((!(strcmp(k->key, key))) && (modifier == k->modifier))
      return k;
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
    if((!(strcmp(k->key, key))) && (k->modifier == KEYMOD_NOMOD))
      return k;
  }

  /* Keybinding unknown */
  return NULL;
}

/*
 * Handle key event from an XEvent.
 */
void kbindings_handle_kbinding(kbinding_t *kbt, XEvent *event) {

  if((kbt == NULL) || (event == NULL))
    return;

  switch(event->type) {
    
  case KeyPress: {
    XKeyEvent            mykeyevent;
    KeySym               mykey;
    char                 kbuf[256];
    int                  len;
    int                  mod, modifier;
    kbinding_entry_t    *k;
    
    mykeyevent = event->xkey;
    
    (void) xitk_get_key_modifier(event, &mod);
    kbindings_convert_modifier(mod, &modifier);

    XLockDisplay (gGui->display);
    len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
    XUnlockDisplay (gGui->display);
    
    k = kbindings_lookup_binding(kbt, XKeysymToString(mykey), modifier);
    if(k) {
      gui_execute_action_id(k->action_id);
    }
#if 0  /* DEBUG */
    else
      printf("%s unhandled\n", kbuf);
#endif
    
  }
  break;
  }
}
