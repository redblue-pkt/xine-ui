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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <X11/keysym.h>

#include <xine.h>

#include "_xitk.h"
#include "mrlbrowser.h"
#include "browser.h"
#include "label.h"
#include "labelbutton.h"
#include "button.h"
#include "combo.h"

#include "utils.h"

#include "_xitk.h"
#include "button_list.h"

typedef struct {
  xine_mrl_t                 xmrl;
  char                      *disp, ext[8];
} _mrlb_item_t;

typedef struct {
  _mrlb_item_t              *items;
  char                     **f_list;
  int                       *f_indx, f_num, i_num;
} _mrlb_items_t;

typedef struct {
  xitk_widget_t              w;

  char                      *skin_element_name;
  char                      *skin_element_name_ip;

  xitk_t                    *xitk;
  xitk_register_key_t        widget_key;

  xitk_window_t             *xwin;

  xitk_widget_list_t        *widget_list; /* File browser widget list */

  xine_t                    *xine;

  int                        mrls_num;

  char                      *last_mrl_source;

  xitk_widget_t             *widget_origin; /* Current directory widget */
  char                      *current_origin; /* Current directory */

  int                        running; /* Boolean status */
  int                        visible; /* Boolean status */

  _mrlb_items_t              items;

  xitk_widget_t             *mrlb_list; /*  Browser list widget */
  xitk_widget_t             *autodir_buttons;

  xitk_widget_t             *combo_filter;
  const char               **filters;
  int                        filters_num;
  xitk_mrlbrowser_filter_t **mrl_filters;
  int                        filter_selected;

  xitk_mrl_callback_t        add_callback;
  void                      *add_userdata;
  xitk_mrl_callback_t        play_callback;
  void                      *play_userdata;
  xitk_simple_callback_t     kill_callback;
  void                      *kill_userdata;
  xitk_simple_callback_t     ip_callback;
  void                      *ip_userdata;
  xitk_key_event_callback_t  key_cb;
  void                      *key_cb_data;

  xitk_event_cbs_t           mrlbrowser_event_cbs;
} _mrlbrowser_private_t;


#undef DEBUG_MRLB

static const struct {
  const char *name;
  const char *ending;
} __mrl_filters[] = {
  { "All"                 , "*"            },
  { "*.vob"               , "vob"          }, /* mpeg block */
  { "*.mpv"               , "mpv"          }, /* elementary */
  { "*.mpg, *.mpeg, *.mpe", "mpg,mpeg,mpe" }, /* mpeg */
  { "*.avi"               , "avi"          }, /* avi */
  { "*.mp3"               , "mp3"          }, /* mp3 */
  { "*.asf, *.wmv"        , "asf,wmv"      }, /* asf */
  { "*.cpk, *.cak, *.film", "cpk,cak,film" }, /* film */
  { "*.ogg, *.ogm, *.ogv" , "ogg"          }, /* ogg */
  { "*.vdr"               , "vdr"          }, /* pes */
  { "*.mov, *.mp4, *.qt"  , "mov,mp4,qt"   }, /* QT */
  { "*.m4a, *.m4b"        , "mp4 audio"    }, /* QT (audio) */
  { "*.roq"               , "roq"          }, /* roq */
  { "*.m2t, *.ts, *.trp"  , "m2t,ts,trp"   }, /* ts */
  { "*.rmvb"              , "rmvb"         }, /* rmvb */
  { NULL                  , NULL           }
};

/*
 *
 */
static void _duplicate_mrl_filters (_mrlbrowser_private_t *wp, xitk_mrlbrowser_filter_t **mrl_f) {
  xitk_mrlbrowser_filter_t **mrl_filters = mrl_f;
  size_t                     i = 0;

  wp->filters_num = 0;

  if(mrl_f == NULL) {
    static const size_t len = sizeof(__mrl_filters) / sizeof(__mrl_filters[0]);

    wp->mrl_filters = (xitk_mrlbrowser_filter_t **)
      xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t *) * len);

    for(i = 0; i < (len - 1); i++) {

      wp->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *)
	xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
      wp->mrl_filters[i]->name   = strdup(__mrl_filters[i].name);
      wp->mrl_filters[i]->ending = strdup(__mrl_filters[i].ending);

    }
  }
  else {

    wp->mrl_filters = (xitk_mrlbrowser_filter_t **)
      xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t *));

    for(i = 0;
	mrl_filters[i] && (mrl_filters[i]->name && mrl_filters[i]->ending); i++) {

      wp->mrl_filters            = (xitk_mrlbrowser_filter_t **) realloc(wp->mrl_filters, sizeof(xitk_mrlbrowser_filter_t *) * (i + 2));
      wp->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *) xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
      wp->mrl_filters[i]->name   = strdup(mrl_filters[i]->name);
      wp->mrl_filters[i]->ending = strdup(mrl_filters[i]->ending);
    }
  }

  wp->filters_num            = i;
  wp->mrl_filters[i]         = (xitk_mrlbrowser_filter_t *)
    xitk_xmalloc(sizeof(xitk_mrlbrowser_filter_t));
  wp->mrl_filters[i]->name   = NULL;
  wp->mrl_filters[i]->ending = NULL;

  wp->filters = (const char **) calloc((wp->filters_num + 1), sizeof(char *));

  for(i = 0; wp->mrl_filters[i]->name != NULL; i++)
    wp->filters[i] = wp->mrl_filters[i]->name;

  wp->filters[i] = NULL;
  wp->filter_selected = 0;
}

static void _mrlb_items_free (_mrlb_items_t *items) {
  free (items->items);
  items->items = NULL;
  items->i_num = 0;
  free (items->f_list);
  items->f_list = NULL;
  free (items->f_indx);
  items->f_indx = NULL;
  items->f_num = 0;
}

/*
 * Destroy the mrlbrowser.
 */
static void _mrlbrowser_destroy (_mrlbrowser_private_t *wp) {
  wp->running = 0;
  wp->visible = 0;

  xitk_unregister_event_handler (wp->xitk, &wp->widget_key);

  xitk_window_destroy_window (wp->xwin);
  wp->xwin = NULL;

  _mrlb_items_free (&wp->items);

  {
    int i;

    for (i = 0; i <= wp->filters_num; i++) {
      XITK_FREE (wp->mrl_filters[i]->name);
      XITK_FREE (wp->mrl_filters[i]->ending);
      XITK_FREE (wp->mrl_filters[i]);
    }
    XITK_FREE (wp->mrl_filters);

    free (wp->filters);
  }

  XITK_FREE (wp->skin_element_name);
  XITK_FREE (wp->skin_element_name_ip);
  XITK_FREE (wp->last_mrl_source);
  XITK_FREE (wp->current_origin);
}

static int notify_event(xitk_widget_t *w, widget_event_t *event, widget_event_result_t *result) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return 0;

  (void)result;
  switch (event->type) {
    case WIDGET_EVENT_DESTROY:
      _mrlbrowser_destroy (wp);
      break;
    default: ;
  }
  return 0;
}

/*
 * Redisplay the displayed current origin (useful for file plugin).
 */
static void _update_current_origin (_mrlbrowser_private_t *wp) {
  XITK_FREE (wp->current_origin);
  if (wp->items.i_num && wp->items.items[0].xmrl.origin)
    wp->current_origin = strdup (wp->items.items[0].xmrl.origin);
  else
    wp->current_origin = strdup ("");
  xitk_label_change_label (wp->widget_origin, wp->current_origin);
}

/*
 *
 */
static int _mrlbrowser_filter_mrls (_mrlbrowser_private_t *wp) {
  int sel, nsel;

  if (!wp->items.f_list)
    return -1;

  sel = xitk_browser_get_current_selected (wp->mrlb_list);
  if ((sel >= 0) && (sel < wp->items.f_num))
    sel = wp->items.f_indx[sel];
  else
    sel = -1;
  nsel = -1;

  if (wp->filter_selected) { /* filtering is enabled */
    int i, j;
    size_t elen = strlen (wp->mrl_filters [wp->filter_selected]->ending) + 1;
    char *filter_ends = malloc (elen);
    if (filter_ends) {
      for (i = j = 0; i < wp->items.i_num; i++) {
        int keep = 1;
        if (!(wp->items.items[i].xmrl.type & XINE_MRL_TYPE_file_directory)) {
          char *m, *pfilter_ends;
          keep = 0;
          memcpy (filter_ends, wp->mrl_filters[wp->filter_selected]->ending, elen);
          pfilter_ends = filter_ends;
          while ((m = strsep (&pfilter_ends, ",")) != NULL) {
            if (!strcmp (wp->items.items[i].ext, m)) {
              keep = 1;
              break;
            }
          }
        }
        if (keep) {
          wp->items.f_list[j] = wp->items.items[i].disp;
          wp->items.f_indx[j] = i;
          if (sel == i)
            nsel = j;
          j++;
        }
      }
      wp->items.f_list[j] = NULL;
      wp->items.f_num = j;
      free (filter_ends);
    } else {
      wp->items.f_list[0] = NULL;
      wp->items.f_num = 0;
    }
  } else { /* no filtering */
    int i;
    for (i = 0; i < wp->items.i_num; i++) {
      wp->items.f_list[i] = wp->items.items[i].disp;
      wp->items.f_indx[i] = i;
    }
    wp->items.f_list[i] = NULL;
    wp->items.f_num = i;
    nsel = sel;
  }
  return nsel;
}

/*
 * Create *eye candy* entries in browser.
 */
static int _mrlbrowser_create_enlighted_entries (_mrlbrowser_private_t *wp) {
  return _mrlbrowser_filter_mrls (wp);
}

/*
 * Duplicate mrls from xine-engine's returned. We shouldn't play
 * directly with these ones.
 */
static void _mrlbrowser_duplicate_mrls (_mrlb_items_t *items, xine_mrl_t **mtmp, int num_mrls) {
  char *mem, *g_orig;
  size_t slen, g_olen;
  int i, g_onum;

  items->items = NULL;
  items->i_num = 0;
  items->f_list = NULL;
  items->f_indx = NULL;
  items->f_num = 0;

  if (!mtmp)
    return;

  items->f_list = malloc ((num_mrls + 1) * sizeof (*items->f_list));
  items->f_indx = malloc (num_mrls * sizeof (*items->f_indx));

  /* origin usually is the same for all items. */
  g_orig = (num_mrls > 0) ? mtmp[0]->origin : NULL;
  g_olen = g_orig ? strlen (g_orig) + 1 : 0;
  g_onum = 0;
  slen = 0;
  for (i = 0; i < num_mrls; i++) {
    xine_mrl_t *m = mtmp[i];
    size_t olen = (m->origin ? strlen (m->origin) : 0) + 1;
    size_t mlen = (m->mrl ? strlen (m->mrl) : 0) + 1;
    size_t llen = (m->link ? strlen (m->link) : 0) + 1;
    size_t dlen = (mlen > olen ? mlen - olen + 1 : mlen) + ((m->type & XINE_MRL_TYPE_file_symlink) ? llen + 5 : 1);
    slen += olen + mlen + llen + dlen;
    if ((olen == g_olen) && !memcmp (m->origin, g_orig, g_olen))
      g_onum += 1;
  }
  if (g_onum != num_mrls)
    g_orig = NULL;
  if (g_orig)
    slen -= (g_onum - 1) * g_olen;

  mem = malloc (num_mrls * sizeof (_mrlb_item_t) + slen);
  if (!mem || !items->f_list || !items->f_indx) {
    free (mem);
    free (items->f_list);
    items->f_list = NULL;
    free (items->f_indx);
    items->f_indx = NULL;
    return;
  }
  items->i_num = num_mrls;
  items->items = (_mrlb_item_t *)mem;
  mem += num_mrls * sizeof (_mrlb_item_t);

  if (g_orig) {
    memcpy (mem, g_orig, g_olen);
    g_orig = mem;
    mem += g_olen;
  }

  for (i = 0; i < num_mrls; i++) {
    xine_mrl_t *m = mtmp[i];
    _mrlb_item_t *item = items->items + i;
    size_t olen;
    size_t mlen = (m->mrl ? strlen (m->mrl) : 0) + 1;
    size_t llen = (m->link ? strlen (m->link) : 0) + 1;
    char *p, *q, *r;

    if (g_orig) {
      olen = g_olen;
      item->xmrl.origin = g_orig;
    } else {
      olen = (m->origin ? strlen (m->origin) : 0) + 1;
      item->xmrl.origin = mem;
      memcpy (mem, m->origin ? m->origin : "", olen);
      mem += olen;
    }
    item->xmrl.mrl = mem;
    memcpy (mem, m->mrl ? m->mrl : "", mlen);
    mem += mlen;
    item->xmrl.link = mem;
    memcpy (mem, m->link ? m->link : "", llen);
    mem += llen;
    item->xmrl.type = m->type;
    item->xmrl.size = m->size;

    item->disp = mem;
    if (mlen > olen) {
      if (m->mrl[olen - 1] != '/')
        olen -= 1;
      memcpy (mem, m->mrl + olen, mlen - olen - 1);
      mem += mlen - olen - 1;
    } else if (m->mrl) {
      memcpy (mem, m->mrl, mlen - 1);
      mem += mlen - 1;
    }
    if (m->type & XINE_MRL_TYPE_file_symlink) {
      memcpy (mem, "@ -> ", 5);
      mem += 5;
      memcpy (mem, m->link ? m->link : "", llen);
      mem += llen;
    } else if (m->type & XINE_MRL_TYPE_file_directory) {
      memcpy (mem, "/", 2);
      mem += 2;
    } else if (m->type & XINE_MRL_TYPE_file_fifo) {
      memcpy (mem, "|", 2);
      mem += 2;
    } else if (m->type & XINE_MRL_TYPE_file_sock) {
      memcpy (mem, "=", 2);
      mem += 2;
    } else if (m->type & XINE_MRL_TYPE_file_exec) {
      memcpy (mem, "*", 2);
      mem += 2;
    } else {
      *mem++ = 0;
    }
    p = mlen > olen ? m->mrl + mlen - olen : m->mrl;
    q = NULL;
    while (p[0] && (p[0] != '?') && (p[0] != '#')) {
      if (p[0] == '.') {
        q = p + 1;
      } else if (p[0] == '/') {
        q = NULL;
      }
      p += 1;
    }
    if (!q)
      q = p;
    if (p > q + sizeof (item->ext) - 1)
      p = q + sizeof (item->ext) - 1;
    r = item->ext;
    while (q < p)
      *r++ = *q++ | 0x20;
    *r = 0;
  }
}

/*
 * Grab mrls from xine-engine.
 */
static void mrlbrowser_grab_mrls (xitk_widget_t *w, void *data, int state) {
  _mrlbrowser_private_t *wp = (_mrlbrowser_private_t *)data;
  char *lbl = (char *) xitk_labelbutton_get_label(w);
  char *old_old_src;

  (void)state;
  if(lbl) {
    _mrlb_items_t items = wp->items;

    old_old_src = wp->last_mrl_source;
    wp->last_mrl_source = strdup (lbl);

    {
      int num_mrls;
      xine_mrl_t **mtmp = xine_get_browse_mrls (wp->xine, wp->last_mrl_source, NULL, &num_mrls);
#if 0
      if(!mtmp) {
        free (wp->last_mrl_source);
        wp->last_mrl_source = old_old_src;
	return;
      }
#endif
      _mrlbrowser_duplicate_mrls (&wp->items, mtmp, num_mrls);
    }

    free(old_old_src);

    _update_current_origin (wp);
    _mrlbrowser_create_enlighted_entries (wp);
    xitk_browser_update_list (wp->mrlb_list, (const char* const *)wp->items.f_list, NULL, wp->items.f_num, 0);
    _mrlb_items_free (&items);
  }
}

#ifdef DEBUG_MRLB
/*
 * Dump informations, on terminal, about selected mrl.
 */
static void mrlbrowser_dumpmrl(xitk_widget_t *w, void *data) {
  _mrlbrowser_private_t *wp= (_mrlbrowser_private_t *)data;
  int j = -1;

  if ((j = xitk_browser_get_current_selected (wp->mrlb_list)) >= 0) {
    xine_mrl_t *ms = wp->mc->mrls[j];

    printf("mrl '%s'\n\t+", ms->mrl);

    if(ms->type & XINE_MRL_TYPE_file_symlink)
      printf(" symlink to '%s' \n\t+ ", ms->link);

    printf("[");

    if(ms->type & XINE_MRL_TYPE_unknown)
      printf(" unknown ");

      if(ms->type & XINE_MRL_TYPE_dvd)
	printf(" dvd ");

      if(ms->type & XINE_MRL_TYPE_vcd)
	    printf(" vcd ");

      if(ms->type & XINE_MRL_TYPE_net)
	printf(" net ");

      if(ms->type & XINE_MRL_TYPE_rtp)
	printf(" rtp ");

      if(ms->type & XINE_MRL_TYPE_stdin)
	printf(" stdin ");

      if(ms->type & XINE_MRL_TYPE_file)
	printf(" file ");

      if(ms->type & XINE_MRL_TYPE_file_fifo)
	printf(" fifo ");

      if(ms->type & XINE_MRL_TYPE_file_chardev)
	printf(" chardev ");

      if(ms->type & XINE_MRL_TYPE_file_directory)
	printf(" directory ");

      if(ms->type & XINE_MRL_TYPE_file_blockdev)
	printf(" blockdev ");

      if(ms->type & XINE_MRL_TYPE_file_normal)
	printf(" normal ");

      if(ms->type & XINE_MRL_TYPE_file_sock)
	printf(" sock ");

      if(ms->type & XINE_MRL_TYPE_file_exec)
	printf(" exec ");

      if(ms->type & XINE_MRL_TYPE_file_backup)
	printf(" backup ");

      if(ms->type & XINE_MRL_TYPE_file_hidden)
	printf(" hidden ");

      printf("] (%Ld byte%c)\n", ms->size, (ms->size > 0) ?'s':'\0');
  }
}
#endif
/*
 *                                 END OF PRIVATES
 *****************************************************************************/

/*
 * Enable/Disable tips.
 */
void xitk_mrlbrowser_set_tips_timeout(xitk_widget_t *w, int enabled, unsigned long timeout) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return;

  if (enabled)
    xitk_set_widgets_tips_timeout (wp->widget_list, timeout);
  else
    xitk_disable_widgets_tips (wp->widget_list);
}

/*
 * Return window of widget.
 */
xitk_window_t *xitk_mrlbrowser_get_window(xitk_widget_t *w) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return NULL;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return NULL;

  return wp->xwin;
}


/*
 * Fill window information struct of given widget.
 */
int xitk_mrlbrowser_get_window_info(xitk_widget_t *w, window_info_t *inf) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return 0;

  return xitk_get_window_info (wp->xitk, wp->widget_key, inf);
}

/*
 * Boolean about running state.
 */
int xitk_mrlbrowser_is_running(xitk_widget_t *w) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return 0;

  return wp->running;
}

/*
 * Boolean about visible state.
 */
int xitk_mrlbrowser_is_visible(xitk_widget_t *w) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return 0;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return 0;

  return wp->visible;
}

/*
 * Hide mrlbrowser.
 */
void xitk_mrlbrowser_hide(xitk_widget_t *w) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return;

  if (wp->visible) {
    wp->visible = 0;
    xitk_hide_widgets (wp->widget_list);
    xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, 0);
  }
}

/*
 * Show mrlbrowser.
 */
void xitk_mrlbrowser_show(xitk_widget_t *w) {
  _mrlbrowser_private_t *wp;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return;

  wp->visible = 1;
  xitk_show_widgets (wp->widget_list);
  xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
  xitk_window_raise_window (wp->xwin);
}

/*
 * Leaving mrlbrowser.
 */
static void xitk_mrlbrowser_exit (xitk_widget_t *w, void *data, int state) {
  _mrlbrowser_private_t *wp = (_mrlbrowser_private_t *)data;

  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return;

  (void)w;
  (void)state;
  if (wp->kill_callback)
    wp->kill_callback (&wp->w, wp->kill_userdata);

  xitk_destroy_widget (&wp->w);
}


/*
 *
 */

void xitk_mrlbrowser_change_skins (xitk_widget_t *w, xitk_skin_config_t *skonfig) {
  _mrlbrowser_private_t *wp;
  xitk_image_t *bg_image;

  xitk_container (wp, w, w);
  if (!wp)
    return;
  if ((wp->w.type & WIDGET_TYPE_MASK) != WIDGET_TYPE_MRLBROWSER)
    return;

  xitk_skin_lock (skonfig);
  xitk_hide_widgets (wp->widget_list);

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (skonfig, wp->skin_element_name);
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image)
    XITK_DIE ("%s(): couldn't find image for background\n", __FUNCTION__);

  xitk_window_set_background_image (wp->xwin, bg_image);
  xitk_skin_unlock (skonfig);

  xitk_change_skins_widget_list (wp->widget_list, skonfig);

  xitk_paint_widget_list (wp->widget_list);
}

/*
 *
 */
static void _mrlbrowser_select_mrl (_mrlbrowser_private_t *wp, int j, int add_callback, int play_callback) {
  _mrlb_item_t *item;

  if ((j < 0) || (j >= wp->items.f_num))
    return;
  item = wp->items.items + wp->items.f_indx[j];

  if ((item->xmrl.type & (XINE_MRL_TYPE_file | XINE_MRL_TYPE_file_directory))
    == (XINE_MRL_TYPE_file | XINE_MRL_TYPE_file_directory)) {
    char *new_dir = item->xmrl.mrl, *temp = NULL;
    _mrlb_items_t items = wp->items;

    if (item->disp[0] == '.') {
      if (item->disp[1] == '/') { /* reload */
        new_dir = item->xmrl.origin;
      } else if ((item->disp[1] == '.') && (item->disp[2] == '/')) { /* parent */
        size_t l = strlen (item->xmrl.origin);
        new_dir = item->xmrl.origin;
        temp = malloc (l + 2);
        if (temp) {
          temp[0] = '/';
          memcpy (temp + 1, item->xmrl.origin, l + 1);
          new_dir = temp + 1 + l;
          if ((new_dir > temp + 1) && (new_dir[-1] == '/'))
            new_dir -= 1;
          do new_dir -= 1; while (new_dir[0] != '/');
          if (new_dir <= temp)
            new_dir = temp + 1;
          if ((new_dir == temp + 1) && (new_dir[0] == '/'))
            new_dir += 1;
          new_dir[0] = 0;
          new_dir = temp + 1;
        }
      }
    }

    {
      int num_mrls;
      xine_mrl_t **mtmp = xine_get_browse_mrls (wp->xine, wp->last_mrl_source, new_dir, &num_mrls);
      _mrlbrowser_duplicate_mrls (&wp->items, mtmp, num_mrls);
    }

    free (temp);

    _update_current_origin (wp);
    _mrlbrowser_create_enlighted_entries (wp);
    xitk_browser_update_list (wp->mrlb_list, (const char * const *)wp->items.f_list, NULL, wp->items.f_num, 0);
    _mrlb_items_free (&items);

  }
  else {

    xitk_browser_release_all_buttons (wp->mrlb_list);

    if (add_callback && wp->add_callback) {
      /* legacy HACK: give selected index for unset user data. */
      void *userdata = wp->add_userdata;
      if (!userdata)
        userdata = (void *)(uintptr_t)j;
      wp->add_callback (&wp->w, userdata, &item->xmrl);
    }

    if (play_callback && wp->play_callback) {
      void *userdata = wp->play_userdata;
      if (!userdata)
        userdata = (void *)(uintptr_t)j;
      wp->play_callback (&wp->w, userdata, &item->xmrl);
    }
  }
}

/*
 * Handle selection in mrlbrowser.
 */
static void mrlbrowser_select (xitk_widget_t *w, void *data, int state) {
  _mrlbrowser_private_t *wp= (_mrlbrowser_private_t *)data;
  int j = -1;

  (void)w;
  (void)state;
  if ((j = xitk_browser_get_current_selected (wp->mrlb_list)) >= 0)
    _mrlbrowser_select_mrl (wp, j, 1, 0);
}

/*
 * Handle selection in mrlbrowser, then
 */
static void mrlbrowser_play(xitk_widget_t *w, void *data) {
  _mrlbrowser_private_t *wp= (_mrlbrowser_private_t *)data;
  int j = -1;

  (void)w;
  if ((j = xitk_browser_get_current_selected (wp->mrlb_list)) >= 0)
    _mrlbrowser_select_mrl (wp, j, 0, 1);
}

/*
 * Handle double click in labelbutton list.
 */
static void handle_dbl_click(xitk_widget_t *w, void *data, int selected, int modifier) {
  _mrlbrowser_private_t *wp= (_mrlbrowser_private_t *)data;

  (void)w;
  if ((modifier & MODIFIER_CTRL) && (modifier & MODIFIER_SHIFT))
    _mrlbrowser_select_mrl (wp, selected, 1, 1);
  else if (modifier & MODIFIER_CTRL)
    _mrlbrowser_select_mrl (wp, selected, 1, 0);
  else
    _mrlbrowser_select_mrl (wp, selected, 0, 1);

}

/*
 * Refresh filtered list
 */
static void combo_filter_select(xitk_widget_t *w, void *data, int select) {
  _mrlbrowser_private_t *wp = (_mrlbrowser_private_t *)data;
  int nsel;

  (void)w;
  wp->filter_selected = select;
  /* Keep item selection across filter switch, if possible. */
  nsel = _mrlbrowser_create_enlighted_entries (wp);
  xitk_browser_update_list (wp->mrlb_list, (const char * const *)wp->items.f_list, NULL, wp->items.f_num, 0);
  xitk_browser_set_select (wp->mrlb_list, nsel);
}

/*
 * Handle here mrlbrowser events.
 */
static void mrlbrowser_handle_key_event(void *data, const xitk_key_event_t *ke) {
  _mrlbrowser_private_t *wp= (_mrlbrowser_private_t *)data;

  if (ke->event != XITK_KEY_PRESS)
    return;

  switch (ke->key_pressed) {

    case XK_d:
    case XK_D:
#ifdef DEBUG_MRLB
      /* This is for debugging purpose */
      if (ke->modifiers & MODIFIER_CTRL)
        mrlbrowser_dumpmrl (NULL, (void *)wp);
#endif
      break;

    case XK_s:
    case XK_S:
      if (ke->modifiers & MODIFIER_CTRL)
        mrlbrowser_select (NULL, (void *)wp, 0);
      break;

    case XK_Return: {
      int selected;

      if ((selected = xitk_browser_get_current_selected (wp->mrlb_list)) >= 0)
        _mrlbrowser_select_mrl (wp, selected, 0, 1);
    }
    break;

    case XK_Escape:
      xitk_mrlbrowser_exit (NULL, (void *)wp, 0);
      break;

    default:
      if (wp->key_cb)
        wp->key_cb (wp->key_cb_data, ke);
      break;
  }
}

/*
 * Create mrlbrowser window.
 */
xitk_widget_t *xitk_mrlbrowser_create(xitk_t *xitk, xitk_skin_config_t *skonfig, xitk_mrlbrowser_widget_t *mb) {
  char                       *title = mb->window_title;
  _mrlbrowser_private_t      *wp;
  xitk_widget_t              *default_source = NULL, *w;
  xitk_image_t               *bg_image;

  ABORT_IF_NULL(xitk);
  ABORT_IF_NULL(xitk->imlibdata);

  XITK_CHECK_CONSTITENCY(mb);

  if(mb->ip_availables == NULL) {
    XITK_DIE("Something's going wrong, there is no input plugin "
	     "available having INPUT_CAP_GET_DIR capability !!\nExiting.\n");
  }

  if(mb->xine == NULL) {
    XITK_DIE("Xine engine should be initialized first !!\nExiting.\n");
  }

  wp = (_mrlbrowser_private_t *) xitk_widget_new (NULL, sizeof (*wp));
  if (!wp)
    return NULL;

  wp->w.private_data    = wp;

  wp->xitk              = xitk;
  wp->visible           = 1;
  wp->running           = 1;
  wp->skin_element_name = mb->skin_element_name ? strdup (mb->skin_element_name) : NULL;
  wp->xine              = mb->xine;
  wp->key_cb            = mb->key_cb;
  wp->key_cb_data       = mb->key_cb_data;

  {
    const xitk_skin_element_info_t *info = xitk_skin_get_info (skonfig, wp->skin_element_name);
    bg_image = info ? info->pixmap_img.image : NULL;
  }
  if (!bg_image) {
    XITK_WARNING("%s(%d): couldn't find image for background\n", __FILE__, __LINE__);
    XITK_FREE (wp);
    return NULL;
  }

  wp->items.items = NULL;
  wp->items.i_num = 0;
  wp->items.f_list = NULL;
  wp->items.f_indx = NULL;
  wp->items.f_num = 0;

  wp->last_mrl_source = NULL;

  wp->xwin = xitk_window_create_window_ext (xitk, mb->x, mb->y, bg_image->width, bg_image->height,
    title, "xitk mrl browser", "xitk", 0, 0, mb->icon, bg_image);

  wp->widget_list = xitk_window_widget_list (wp->xwin);

  {
    xitk_labelbutton_widget_t lb;

    XITK_WIDGET_INIT (&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.align             = ALIGN_DEFAULT;
    lb.state_callback    = NULL;
    lb.userdata          = (void *)wp;

    lb.label             = mb->select.caption;
    lb.callback          = mrlbrowser_select;
    lb.skin_element_name = mb->select.skin_element_name;
    w = xitk_labelbutton_create (wp->widget_list, skonfig, &lb);
    if (w) {
      xitk_dlist_add_tail (&wp->widget_list->list, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
      xitk_set_widget_tips (w, _("Select current entry"));
    }

    lb.label             = mb->dismiss.caption;
    lb.callback          = xitk_mrlbrowser_exit;
    lb.skin_element_name = mb->dismiss.skin_element_name;
    w = xitk_labelbutton_create (wp->widget_list, skonfig, &lb);
    if (w) {
      xitk_dlist_add_tail (&wp->widget_list->list, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
      xitk_set_widget_tips (w, _("Close MRL browser window"));
    }
  }

  {
    xitk_button_widget_t pb;
    XITK_WIDGET_INIT (&pb);
    pb.skin_element_name = mb->play.skin_element_name;
    pb.callback          = mrlbrowser_play;
    pb.userdata          = (void *)wp;
    w = xitk_button_create (wp->widget_list, skonfig, &pb);
    if (w) {
      xitk_dlist_add_tail (&wp->widget_list->list, &w->node);
      w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
      xitk_set_widget_tips (w, _("Play selected entry"));
    }
  }

  wp->add_callback  = mb->select.callback;
  wp->add_userdata  = mb->select.data;
  wp->play_callback = mb->play.callback;
  wp->play_userdata = mb->play.data;
  wp->kill_callback = mb->kill.callback;
  wp->kill_userdata = mb->kill.data;

  XITK_WIDGET_INIT (&mb->browser);
  mb->browser.dbl_click_callback  = handle_dbl_click;
  mb->browser.userdata            = (void *)wp;
  wp->mrlb_list = xitk_browser_create (wp->widget_list, skonfig, &mb->browser);
  if (wp->mrlb_list) {
    xitk_dlist_add_tail (&wp->widget_list->list, &wp->mrlb_list->node);
    wp->mrlb_list->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
  }

  {
    xitk_label_widget_t lbl;

    XITK_WIDGET_INIT (&lbl);
    lbl.callback          = NULL;
    lbl.userdata          = NULL;

    lbl.label             = "";
    lbl.skin_element_name = mb->origin.skin_element_name;
    wp->widget_origin = xitk_label_create (wp->widget_list, skonfig, &lbl);
    if (wp->widget_origin) {
      xitk_dlist_add_tail (&wp->widget_list->list, &wp->widget_origin->node);
      wp->widget_origin->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
    }

    if (mb->ip_name.label.label_str) {
      lbl.label             = mb->ip_name.label.label_str;
      lbl.skin_element_name = mb->ip_name.label.skin_element_name;
      w = xitk_label_create (wp->widget_list, skonfig, &lbl);
      if (w) {
        xitk_dlist_add_tail (&wp->widget_list->list, &w->node);
        w->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
      }
    }
  }

  if (mb->origin.cur_origin)
    wp->current_origin = strdup (mb->origin.cur_origin);
  else
    wp->current_origin = strdup ("");

  /*
   * Create buttons with input plugins names.
   */
  wp->skin_element_name_ip = strdup (mb->ip_name.button.skin_element_name);
  do {
    const char *tips[64];
    const char * const *autodir_plugins = mb->ip_availables;
    unsigned int i;

    if (!autodir_plugins)
      break;

    for (i = 0; autodir_plugins[i]; i++) {
      if (i >= sizeof (tips) / sizeof (tips[0]))
        break;
      tips[i] = xine_get_input_plugin_description (wp->xine, autodir_plugins[i]);
    }

    wp->autodir_buttons = xitk_button_list_new (wp->widget_list, skonfig, wp->skin_element_name_ip,
      mrlbrowser_grab_mrls, wp, mb->ip_availables,
      tips, 5000, WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER);
    if (wp->autodir_buttons) {
      xitk_dlist_add_tail (&wp->widget_list->list, &wp->autodir_buttons->node);
      wp->autodir_buttons->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
    }
  } while (0);

  _duplicate_mrl_filters (wp, mb->mrl_filters);

  {
    xitk_combo_widget_t cmb;
    XITK_WIDGET_INIT (&cmb);
    cmb.skin_element_name = mb->combo.skin_element_name;
    cmb.layer_above       = mb->layer_above;
    cmb.entries           = wp->filters;
    cmb.parent_wkey       = &wp->widget_key;
    cmb.callback          = combo_filter_select;
    cmb.userdata          = (void *)wp;
    wp->combo_filter = xitk_combo_create (wp->widget_list, skonfig, &cmb);
    if (wp->combo_filter) {
      xitk_dlist_add_tail (&wp->widget_list->list, &wp->combo_filter->node);
      wp->combo_filter->type |= WIDGET_GROUP_MEMBER | WIDGET_GROUP_MRLBROWSER;
    }
  }

  wp->w.x            = mb->x;
  wp->w.y            = mb->y;
  wp->w.width        = bg_image->width;
  wp->w.height       = bg_image->height;
  wp->w.type         = WIDGET_GROUP | WIDGET_TYPE_MRLBROWSER;
  wp->w.event        = notify_event;

  xitk_browser_update_list (wp->mrlb_list, (const char * const *)wp->items.f_list, NULL, wp->items.f_num, 0);

  wp->mrlbrowser_event_cbs.dnd_cb = mb->dndcallback;
  wp->mrlbrowser_event_cbs.key_cb  = mrlbrowser_handle_key_event;

  wp->widget_key = xitk_window_register_event_handler ("mrl browser", wp->xwin, &wp->mrlbrowser_event_cbs, wp);

  if (mb->reparent_window) {
    mb->reparent_window (mb->rw_data, wp->xwin);
  } else {
    xitk_window_flags (wp->xwin, XITK_WINF_VISIBLE | XITK_WINF_ICONIFIED, XITK_WINF_VISIBLE);
    xitk_window_raise_window (wp->xwin);
  }

  xitk_window_try_to_set_input_focus (wp->xwin);

  /* xitk_mrlbrowser_change_skins (&wp->w, skonfig); */

  default_source = xitk_button_list_find (wp->autodir_buttons, "file");
  if (default_source && !wp->last_mrl_source)
    mrlbrowser_grab_mrls (default_source, wp, 0);

  return &wp->w;
}
