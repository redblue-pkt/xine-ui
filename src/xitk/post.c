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

#include "common.h"
#include "post.h"
#include "videowin.h"
#include "actions.h"
#include "event.h"
#include "errors.h"
#include "xine-toolkit/combo.h"
#include "xine-toolkit/slider.h"
#include "xine-toolkit/inputtext.h"
#include "xine-toolkit/label.h"
#include "xine-toolkit/labelbutton.h"
#include "xine-toolkit/button.h"
#include "xine-toolkit/doublebox.h"
#include "xine-toolkit/intbox.h"
#include "xine-toolkit/checkbox.h"
#include "xine-toolkit/browser.h"

#define VFREE(vfp) do {memset (vfp, 0x55, sizeof (*(vfp))); free (vfp); } while (0)

#undef TRACE_REWIRE

#define DEFAULT_DEINTERLACER "tvtime:method=LinearBlend,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

#define WINDOW_WIDTH        530
#define WINDOW_HEIGHT       503

#define FRAME_WIDTH         (WINDOW_WIDTH - 50)
#define FRAME_HEIGHT        80

#define HELP_WINDOW_WIDTH   400
#define HELP_WINDOW_HEIGHT  402

#define MAX_DISPLAY_FILTERS 5
/* It hardly makes any sense to use infinitely many filters. */
#define MAX_USED_FILTERS    32

#define MAX_DISP_ENTRIES    15
#define BROWSER_LINE_WIDTH  55

#define DISABLE_ME(widget)                                                                      \
    do {                                                                                        \
      if(widget)                                                                                \
        xitk_disable_and_hide_widget(widget);                                                   \
    } while(0)

#define ENABLE_ME(widget)                                                                       \
    do {                                                                                        \
      if(widget)                                                                                \
        xitk_enable_and_show_widget(widget);                                                    \
    } while(0)

typedef struct {
  post_info_t                 *info;

  xine_post_t                 *post;
  const xine_post_api_t             *api;
  const xine_post_api_descr_t       *descr;
  const xine_post_api_parameter_t   *param;
  char                        *param_data;

  int                          x;
  int                          y;

  xitk_widget_t               *frame;
  xitk_widget_t               *plugins;
  xitk_widget_t               *properties;
  xitk_widget_t               *value;
  xitk_widget_t               *comment;

  xitk_widget_t               *up;
  xitk_widget_t               *down;

  xitk_widget_t               *help;

  int                          readonly;

  char                       **properties_names;
} post_object_t;

struct post_win_s {
  xitk_window_t              *xwin;

  xitk_widget_list_t         *widget_list;

  char                      **plugin_names;

  post_object_t              *post_objects[MAX_USED_FILTERS];
  int                         first_displayed, object_num;

  xitk_widget_t              *slider, *enable, *exit;

  int                         x, y;

  int                         running, visible;

  xitk_register_key_t         widget_key;

  /* help window stuff */
  xitk_window_t              *helpwin;
  char                      **help_text;
  xitk_register_key_t         help_widget_key;
  int                         help_running;
  xitk_widget_t              *help_browser;
};

static void post_info_init (gGui_t *gui, post_info_t *info, post_type_t type) {
  info->gui = gui;
  info->stream = NULL;
  info->win = NULL;
  info->elements = NULL;
  info->num_elements = 0;
  info->type = type;
  if (type == POST_AUDIO) {
    info->info.audio.audio_vis_plugins = NULL;
    info->info.audio.ignore_visual_anim = 1;
  } else if (type == POST_VIDEO) {
    info->info.video.deinterlace_plugin = NULL;
    info->info.video.deinterlace_elements = NULL;
    info->info.video.num_deinterlace_elements = 0;
  }
}

static void _vpplugin_unwire (gGui_t *gui) {
  xine_post_out_t  *vo_source;

  if (gui->stream) {
    vo_source = xine_get_video_source(gui->stream);

    if (gui->vo_port)
      (void) xine_post_wire_video_port(vo_source, gui->vo_port);
  }
}

static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num, post_info_t *vinfo) {
  gGui_t *gui = vinfo->gui;
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if (gui->post_video_enable)
    (*post_elements_num) += vinfo->num_elements;
  if (gui->deinterlace_enable)
    (*post_elements_num) += vinfo->info.video.num_deinterlace_elements;

  if (*post_elements_num == 0)
    return NULL;

  post_elements = (post_element_t **)
    calloc((*post_elements_num), sizeof(post_element_t *));

  for( i = 0; gui->deinterlace_enable && i < vinfo->info.video.num_deinterlace_elements; i++ ) {
    post_elements[i+j] = vinfo->info.video.deinterlace_elements[i];
  }

  for (j = 0; gui->post_video_enable && j < vinfo->num_elements; j++ ) {
    post_elements[i+j] = vinfo->elements[j];
  }

  return post_elements;
}

static void _pplugin_save_chain (post_info_t *info) {

  if(info->win) {
    // XXX simplify
    post_element_t ***_post_elements = &info->elements;
    int *_post_elements_num = &info->num_elements;
    int i = 0;
    int post_num = info->win->object_num;

    if(!xitk_combo_get_current_selected(info->win->post_objects[post_num - 1]->plugins))
      post_num--;

    if(post_num) {
      if(!*_post_elements) {
	*_post_elements = (post_element_t **)
	  calloc((post_num + 1), sizeof(post_element_t *));
      }
      else {
	int j;

	for(j = 0; j < *_post_elements_num; j++) {
	  free((*_post_elements)[j]->name);
	  VFREE((*_post_elements)[j]);
	}

	*_post_elements = (post_element_t **) realloc(*_post_elements, sizeof(post_element_t *) * (post_num + 1));

      }

      for(i = 0; i < post_num; i++) {
	(*_post_elements)[i] = (post_element_t *) calloc(1, sizeof(post_element_t));
	(*_post_elements)[i]->post = info->win->post_objects[i]->post;
	(*_post_elements)[i]->name =
	  strdup(xitk_combo_get_current_entry_selected(info->win->post_objects[i]->plugins));
      }

      (*_post_elements)[post_num] = NULL;
      *_post_elements_num         = post_num;
    }
    else {
      if(*_post_elements_num) {
	int j;

	for(j = 0; j < *_post_elements_num; j++) {
	  free((*_post_elements)[j]->name);
	  VFREE((*_post_elements)[j]);
	}

	/* free((*_post_elements)[j]); */
	free(*_post_elements);

	*_post_elements_num = 0;
	*_post_elements     = NULL;
      }
    }
  }
}

static void _vpplugin_rewire (post_info_t *vinfo) {
  static post_element_t **post_elements;
  int post_elements_num;

  _pplugin_save_chain (vinfo);

  post_elements = _pplugin_join_deinterlace_and_post_elements (&post_elements_num, vinfo);
  if (post_elements && post_elements_num) {
    xine_post_out_t   *vo_source;
    int                i = 0;

    for (i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char * const *outs = xine_post_list_outputs (post_elements[i]->post);
      const xine_post_out_t *vo_out = xine_post_output (post_elements[i]->post, (char *) *outs);
      if (i == (post_elements_num - 1)) {
        xine_post_wire_video_port ((xine_post_out_t *)vo_out, vinfo->gui->vo_port);
      } else {
        const xine_post_in_t *vo_in;

        /* look for standard input names */
        vo_in = xine_post_input (post_elements[i + 1]->post, "video");
        if (!vo_in)
          vo_in = xine_post_input (post_elements[i + 1]->post, "video in");
        xine_post_wire ((xine_post_out_t *)vo_out, (xine_post_in_t *)vo_in);
      }
    }
    vo_source = xine_get_video_source (vinfo->stream ? vinfo->stream : vinfo->gui->stream);
    xine_post_wire_video_port (vo_source, post_elements[0]->post->video_input[0]);
  }
  free (post_elements);
}

static void _applugin_unwire (gGui_t *gui) {
  xine_post_out_t  *ao_source;

  if (gui->stream) {
    ao_source = xine_get_audio_source(gui->stream);

    if (gui->ao_port)
      (void) xine_post_wire_audio_port(ao_source, gui->ao_port);
  }
}

static post_element_t **_pplugin_join_visualization_and_post_elements(int *post_elements_num, post_info_t *info) {
  gGui_t *gui = info->gui;
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if (gui->post_audio_enable)
    (*post_elements_num) += info->num_elements;
  if ((info->type == POST_AUDIO) && !info->info.audio.ignore_visual_anim)
    (*post_elements_num)++;

  if (*post_elements_num == 0)
    return NULL;

  post_elements = (post_element_t **)
    calloc((*post_elements_num), sizeof(post_element_t *));

  for (j = 0; gui->post_audio_enable && j < info->num_elements; j++ ) {
    post_elements[i+j] = info->elements[j];
  }

  if ((info->type == POST_AUDIO) && !info->info.audio.ignore_visual_anim) {
    for (i = 0; i < 1; i++)
      post_elements[i+j] = &gui->visual_anim.post_output_element;
  }

  return post_elements;
}

static void _applugin_rewire (post_info_t *info) {
  static post_element_t **post_elements;
  int post_elements_num;

  _pplugin_save_chain (info);

  post_elements = _pplugin_join_visualization_and_post_elements (&post_elements_num, info);
  if (post_elements && post_elements_num) {
    xine_post_out_t   *ao_source;
    int                i = 0;

    for (i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char *const *outs = xine_post_list_outputs (post_elements[i]->post);
      const xine_post_out_t *ao_out = xine_post_output (post_elements[i]->post, (char *) *outs);

      if (i == (post_elements_num - 1)) {
        xine_post_wire_audio_port ((xine_post_out_t *)ao_out, info->gui->ao_port);
      } else {
        const xine_post_in_t *ao_in;

        /* look for standard input names */
        ao_in = xine_post_input (post_elements[i + 1]->post, "audio");
        if (!ao_in)
          ao_in = xine_post_input (post_elements[i + 1]->post, "audio in");
        xine_post_wire ((xine_post_out_t *)ao_out, (xine_post_in_t *)ao_in);
      }
    }
    ao_source = xine_get_audio_source (info->stream ? info->stream : info->gui->stream);
    xine_post_wire_audio_port (ao_source, post_elements[0]->post->audio_input[0]);
  }
  free (post_elements);
}

static int __pplugin_retrieve_parameters(post_object_t *pobj) {
  xine_post_in_t             *input_api;

  if((input_api = (xine_post_in_t *) xine_post_input(pobj->post, "parameters"))) {
    const xine_post_api_t            *post_api;
    const xine_post_api_descr_t      *api_descr;
    const xine_post_api_parameter_t  *parm;
    int                         pnum = 0;

    post_api = (const xine_post_api_t *) input_api->data;

    api_descr = post_api->get_param_descr();

    parm = api_descr->parameter;
    pobj->param_data = malloc(api_descr->struct_size);
    post_api->get_parameters(pobj->post, pobj->param_data);

    while(parm->type != POST_PARAM_TYPE_LAST) {

      pobj->properties_names = (char **) realloc(pobj->properties_names, sizeof(char *) * (pnum + 2));

      pobj->properties_names[pnum]     = strdup(parm->name);
      pobj->properties_names[pnum + 1] = NULL;
      pnum++;
      parm++;
    }

    pobj->api      = post_api;
    pobj->descr    = api_descr;
    pobj->param    = api_descr->parameter;

    return 1;
  }

  return 0;
}

static void _pplugin_update_parameter(post_object_t *pobj) {
  pobj->api->set_parameters(pobj->post, pobj->param_data);
  pobj->api->get_parameters(pobj->post, pobj->param_data);
}

/* pchain: "<post1>:option1=value1,option2=value2..;<post2>:...." */
static post_element_t **pplugin_parse_and_load (post_info_t *info, const char *pchain, int *post_elements_num) {
  gGui_t *gui = info->gui;
  int plugin_type = info->type == POST_VIDEO ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  post_element_t **post_elements = NULL;

  *post_elements_num = 0;

  if(pchain && strlen(pchain)) {
    char *p, *post_chain, *ppost_chain;

    post_chain = strdup(pchain);

    ppost_chain = post_chain;
    while((p = xine_strsep(&ppost_chain, ";"))) {

      if(strlen(p)) {
	char          *plugin, *args = NULL;
	xine_post_t   *post;

	while(*p == ' ')
	  p++;

	plugin = strdup(p);

	if((p = strchr(plugin, ':')))
	  *p++ = '\0';

	if(p && (strlen(p) > 1))
	  args = p;

	post = xine_post_init (gui->xine, plugin, 0, &gui->ao_port, &gui->vo_port);

        if (post && info) {
          if (post->type != plugin_type) {
            xine_post_dispose (gui->xine, post);
            post = NULL;
          }
        }

	if(post) {
	  post_object_t  pobj;

	  post_elements = (post_element_t **) realloc(post_elements, sizeof(post_element_t *) * ((*post_elements_num) + 2));

	  post_elements[(*post_elements_num)] = (post_element_t *)
	    calloc(1, sizeof(post_element_t));
	  post_elements[(*post_elements_num)]->post = post;
	  post_elements[(*post_elements_num)]->name = strdup(plugin);
	  (*post_elements_num)++;
	  post_elements[(*post_elements_num)] = NULL;

	  memset(&pobj, 0, sizeof(post_object_t));
	  pobj.post = post;

	  if(__pplugin_retrieve_parameters(&pobj)) {
	    int   i;

	    if(pobj.properties_names && args) {
	      char *param;

	      while((param = xine_strsep(&args, ",")) != NULL) {

		p = param;

		while((*p != '\0') && (*p != '='))
		  p++;

		if(strlen(p)) {
		  int param_num = 0;

		  *p++ = '\0';

		  while(pobj.properties_names[param_num]
			&& strcasecmp(pobj.properties_names[param_num], param))
		    param_num++;

		  if(pobj.properties_names[param_num]) {

		    pobj.param    = pobj.descr->parameter;
		    pobj.param    += param_num;
		    pobj.readonly = pobj.param->readonly;

		    switch(pobj.param->type) {
		    case POST_PARAM_TYPE_INT:
		      if(!pobj.readonly) {
			if(pobj.param->enum_values) {
			  char **values = pobj.param->enum_values;
			  int    i = 0;

			  while(values[i]) {
			    if(!strcasecmp(values[i], p)) {
			      *(int *)(pobj.param_data + pobj.param->offset) = i;
			      break;
			    }
			    i++;
			  }

			  if( !values[i] )
			    *(int *)(pobj.param_data + pobj.param->offset) = (int) strtol(p, &p, 10);
			} else {
			  *(int *)(pobj.param_data + pobj.param->offset) = (int) strtol(p, &p, 10);
			}
			_pplugin_update_parameter(&pobj);
		      }
		      break;

		    case POST_PARAM_TYPE_DOUBLE:
		      if(!pobj.readonly) {
			*(double *)(pobj.param_data + pobj.param->offset) = strtod(p, &p);
			_pplugin_update_parameter(&pobj);
		      }
		      break;

		    case POST_PARAM_TYPE_CHAR:
		    case POST_PARAM_TYPE_STRING:
		      if(!pobj.readonly) {
			if(pobj.param->type == POST_PARAM_TYPE_CHAR) {
			  int maxlen = pobj.param->size / sizeof(char);

			  strlcpy((char *)(pobj.param_data + pobj.param->offset), p, maxlen);
			  _pplugin_update_parameter(&pobj);
			}
			else
			  fprintf(stderr, "parameter type POST_PARAM_TYPE_STRING not supported yet.\n");
		      }
		      break;

		    case POST_PARAM_TYPE_STRINGLIST: /* unsupported */
		      if(!pobj.readonly)
			fprintf(stderr, "parameter type POST_PARAM_TYPE_STRINGLIST not supported yet.\n");
		      break;

		    case POST_PARAM_TYPE_BOOL:
		      if(!pobj.readonly) {
			*(int *)(pobj.param_data + pobj.param->offset) = ((int) strtol(p, &p, 10)) ? 1 : 0;
			_pplugin_update_parameter(&pobj);
		      }
		      break;
		    }
		  }
		}
	      }

	      i = 0;

	      while(pobj.properties_names[i]) {
		free(pobj.properties_names[i]);
		i++;
	      }

	      free(pobj.properties_names);
	    }

	    VFREE(pobj.param_data);
	  }
	}

	VFREE(plugin);
      }
    }
    free(post_chain);
  }

  return post_elements;
}

static void post_deinterlace_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  post_element_t **posts = NULL;
  int              num, i;
  post_info_t *vinfo = &gui->post_video;

  vinfo->info.video.deinterlace_plugin = cfg->str_value;

  if(gui->deinterlace_enable)
    _vpplugin_unwire (gui);

  for(i = 0; i < vinfo->info.video.num_deinterlace_elements; i++) {
    xine_post_dispose (gui->xine, vinfo->info.video.deinterlace_elements[i]->post);
    free(vinfo->info.video.deinterlace_elements[i]->name);
    VFREE(vinfo->info.video.deinterlace_elements[i]);
  }

  SAFE_FREE(vinfo->info.video.deinterlace_elements);
  vinfo->info.video.num_deinterlace_elements = 0;

  if ((posts = pplugin_parse_and_load (vinfo, vinfo->info.video.deinterlace_plugin, &num))) {
    vinfo->info.video.deinterlace_elements     = posts;
    vinfo->info.video.num_deinterlace_elements = num;
  }

  if(gui->deinterlace_enable)
    _vpplugin_rewire (&gui->post_video);
}

static void post_audio_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->visual_anim.post_plugin_num = cfg->num_value;
  post_rewire_visual_anim (gui);
}

const char * const *post_get_audio_plugins_names (gGui_t *gui) {
  if (!gui)
    return NULL;
  return (const char * const *)gui->post_audio.info.audio.audio_vis_plugins;
}

void post_init (gGui_t *gui) {
  if (!gui)
    return;

  post_info_init (gui, &gui->post_audio, POST_AUDIO);
  post_info_init (gui, &gui->post_video, POST_VIDEO);

  memset (&gui->visual_anim.post_output_element, 0, sizeof (gui->visual_anim.post_output_element));
  gui->visual_anim.post_plugin_num = -1;
  gui->visual_anim.post_changed = 0;

  do {
    const char * const *names;
    int i, n;

    if (!gui->ao_port)
      break;
    names = xine_list_post_plugins_typed (gui->xine, XINE_POST_TYPE_AUDIO_VISUALIZATION);
    if (!names)
      break;
    for (n = 0; names[n]; n++) ;
    gui->post_audio.info.audio.audio_vis_plugins = (char **)malloc (sizeof (char *) * (n + 1));
    if (!gui->post_audio.info.audio.audio_vis_plugins)
      break;
    for (i = 0; i < n; i++)
      gui->post_audio.info.audio.audio_vis_plugins[i] = strdup (names[i]);
    gui->post_audio.info.audio.audio_vis_plugins[n] = NULL;
    if (!n)
      break;
    gui->visual_anim.post_plugin_num = xine_config_register_enum (gui->xine,
      "gui.post_audio_plugin", 0, gui->post_audio.info.audio.audio_vis_plugins,
      _("Audio visualization plugin"),
      _("Post audio plugin to used when playing streams without video"),
      CONFIG_LEVEL_BEG, post_audio_plugin_cb, gui);
    gui->visual_anim.post_output_element.post = xine_post_init (gui->xine,
        gui->post_audio.info.audio.audio_vis_plugins[gui->visual_anim.post_plugin_num],
        0, &gui->ao_port, &gui->vo_port);
  } while (0);
}

void post_rewire_visual_anim (gGui_t *gui) {
  if (!gui)
    return;
  if (gui->visual_anim.post_output_element.post) {
    post_rewire_audio_port_to_stream (gui, gui->stream);
    xine_post_dispose (gui->xine, gui->visual_anim.post_output_element.post);
  }
  gui->visual_anim.post_output_element.post = xine_post_init (gui->xine,
    gui->post_audio.info.audio.audio_vis_plugins[gui->visual_anim.post_plugin_num], 0,
    &gui->ao_port, &gui->vo_port);
  if (gui->visual_anim.post_output_element.post &&
     (gui->visual_anim.enabled == 1) && (gui->visual_anim.running == 1))
    post_rewire_audio_post_to_stream (gui, gui->stream);
}

int post_rewire_audio_port_to_stream (gGui_t *gui, xine_stream_t *stream) {

  _applugin_unwire (gui);
  gui->post_audio.info.audio.ignore_visual_anim = 1;
  gui->post_audio.stream = stream;
  _applugin_rewire (&gui->post_audio);
/*
  xine_post_out_t * audio_source;

  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gui->ao_port);
*/
  return 1;
}

int post_rewire_audio_post_to_stream (gGui_t *gui, xine_stream_t *stream) {

  _applugin_unwire (gui);
  gui->post_audio.info.audio.ignore_visual_anim = 0;
  gui->post_audio.stream = stream;
  _applugin_rewire (&gui->post_audio);
/*
  xine_post_out_t * audio_source;

  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gui->visual_anim.post_output->audio_input[0]);
*/
  return 1;
}

/* ================================================================ */

static void _pplugin_unwire(post_info_t *info) {
  if (info->type == POST_VIDEO)
    _vpplugin_unwire (info->gui);
  else
    _applugin_unwire (info->gui);
}

static void _pplugin_rewire (post_info_t *info) {
  if (info->type == POST_VIDEO)
    _vpplugin_rewire (info);
  else
    _applugin_rewire (info);
}

static int _pplugin_get_object_offset(post_info_t *info, post_object_t *pobj) {
  int i;

  if (!info->win)
    return 0;

  for (i = 0; i < info->win->object_num; i++) {
    if (info->win->post_objects[i] == pobj)
      return i;
  }

  return 0;
}

static int _pplugin_is_first_filter(post_info_t *info, post_object_t *pobj) {
  if (info->win)
    return (pobj == *info->win->post_objects);

  return 0;
}

static int _pplugin_is_last_filter(post_info_t *info, post_object_t *pobj) {

  if(info->win) {
    post_object_t **po = info->win->post_objects;

    while(*po && (*po != pobj))
      po++;

    if(*(po + 1) == NULL)
      return 1;
  }

  return 0;
}

static void _pplugin_hide_obj(post_object_t *pobj) {
  if(pobj) {
    DISABLE_ME(pobj->frame);
    DISABLE_ME(pobj->plugins);
    DISABLE_ME(pobj->properties);
    DISABLE_ME(pobj->value);
    DISABLE_ME(pobj->comment);
    DISABLE_ME(pobj->up);
    DISABLE_ME(pobj->down);
    DISABLE_ME(pobj->help);
  }
}

static void _pplugin_show_obj(post_info_t *info, post_object_t *pobj) {

  if(pobj) {

    if(pobj->frame)
      xitk_set_widget_pos(pobj->frame, pobj->x, pobj->y);

    if(pobj->plugins)
      xitk_set_widget_pos(pobj->plugins, pobj->x + 26 + 6, pobj->y + 5 + (20 - xitk_get_widget_height(pobj->plugins)) / 2);

    if(pobj->properties)
      xitk_set_widget_pos(pobj->properties, pobj->x + 26 + 196, pobj->y + 5 + (20 - xitk_get_widget_height(pobj->properties)) / 2);

    if(pobj->help)
      xitk_set_widget_pos(pobj->help, pobj->x + 26 + 386, pobj->y + 5);

    if(pobj->comment)
      xitk_set_widget_pos(pobj->comment, pobj->x + 26 + 6, pobj->y + 28 + 4);

    if(pobj->value)
      xitk_set_widget_pos(pobj->value, pobj->x + 26 + 6, pobj->y + (FRAME_HEIGHT - 5 + 1) - (20 + xitk_get_widget_height(pobj->value)) / 2);

    if(pobj->up)
      xitk_set_widget_pos(pobj->up, pobj->x + 5, pobj->y + 5);

    if(pobj->down)
      xitk_set_widget_pos(pobj->down, pobj->x + 5, pobj->y + (FRAME_HEIGHT - 16 - 5));

    ENABLE_ME(pobj->frame);
    ENABLE_ME(pobj->plugins);
    ENABLE_ME(pobj->properties);
    ENABLE_ME(pobj->value);
    ENABLE_ME(pobj->comment);
    ENABLE_ME(pobj->help);

    if((!_pplugin_is_first_filter(info, pobj)) && (xitk_combo_get_current_selected(pobj->plugins)))
      ENABLE_ME(pobj->up);

    if((!_pplugin_is_last_filter(info, pobj) && (xitk_combo_get_current_selected(pobj->plugins))) &&
       (_pplugin_is_last_filter(info, pobj)
	|| (!_pplugin_is_last_filter(info, pobj)
	    && info->win->post_objects[_pplugin_get_object_offset(info, pobj) + 1]
	    && xitk_combo_get_current_selected(info->win->post_objects[_pplugin_get_object_offset(info, pobj) + 1]->plugins))))
      ENABLE_ME(pobj->down);

  }
}

static void _pplugin_paint_widgets(post_info_t *info) {
  if(info->win) {
    int   i, x, y;
    int   last;
    int   slidmax, slidpos;

    last = info->win->object_num <= (info->win->first_displayed + MAX_DISPLAY_FILTERS)
      ? info->win->object_num : (info->win->first_displayed + MAX_DISPLAY_FILTERS);

    for(i = 0; i < info->win->object_num; i++)
      _pplugin_hide_obj(info->win->post_objects[i]);

    x = 15;
    y = 34 - (FRAME_HEIGHT + 4);

    for(i = info->win->first_displayed; i < last; i++) {
      y += FRAME_HEIGHT + 4;
      info->win->post_objects[i]->x = x;
      info->win->post_objects[i]->y = y;
      _pplugin_show_obj(info, info->win->post_objects[i]);
    }

    if(info->win->object_num > MAX_DISPLAY_FILTERS) {
      slidmax = info->win->object_num - MAX_DISPLAY_FILTERS;
      slidpos = slidmax - info->win->first_displayed;
      xitk_enable_and_show_widget(info->win->slider);
    }
    else {
      slidmax = 1;
      slidpos = slidmax;

      if(!info->win->first_displayed)
	xitk_disable_and_hide_widget(info->win->slider);
    }

    xitk_slider_set_max(info->win->slider, slidmax);
    xitk_slider_set_pos(info->win->slider, slidpos);
  }
}

static void _pplugin_destroy_widget (xitk_widget_t **w) {
  xitk_destroy_widget (*w);
  *w = NULL;
}

static void _pplugin_set_param_int(xitk_widget_t *w, void *data, int value) {
  post_object_t *pobj = (post_object_t *) data;

  if(pobj->readonly)
    return;

  //can be int[]:
  //int num_of_int = pobj->param->size / sizeof(char);

  if(pobj->param->range_min && pobj->param->range_max &&
     (value < (int)pobj->param->range_min || value > (int)pobj->param->range_max)) {
    xine_error (pobj->info->gui, _("Entered value is out of bounds (%d>%d<%d)."),
	       (int)pobj->param->range_min, value, (int)pobj->param->range_max);
  }
  else {
    *(int *)(pobj->param_data + pobj->param->offset) = value;

    _pplugin_update_parameter(pobj);

    if ((xitk_get_widget_type (w) & WIDGET_TYPE_MASK) == WIDGET_TYPE_COMBO)
      xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
    else
      xitk_intbox_set_value(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
  }
}

static void _pplugin_set_param_double(xitk_widget_t *w, void *data, double value) {
  post_object_t *pobj = (post_object_t *) data;

  (void)w;
  if(pobj->readonly)
    return;

  if(pobj->param->range_min && pobj->param->range_max &&
     (value < pobj->param->range_min || value > pobj->param->range_max)) {
    xine_error (pobj->info->gui, _("Entered value is out of bounds (%e>%e<%e)."),
	       pobj->param->range_min, value, pobj->param->range_max);
  }
  else {
    double *v = (double *)(pobj->param_data + pobj->param->offset);

    *v = value;
    _pplugin_update_parameter (pobj);
    xitk_doublebox_set_value (pobj->value, *v);
  }
}

static void _pplugin_set_param_char(xitk_widget_t *w, void *data, const char *text) {
  post_object_t *pobj = (post_object_t *) data;

  (void)w;
  if(pobj->readonly)
    return;

  // SUPPORT CHAR but no STRING yet
  if(pobj->param->type == POST_PARAM_TYPE_CHAR) {
    char *v;
    int maxlen = pobj->param->size / sizeof(char);

    v = (char *)(pobj->param_data + pobj->param->offset);
    strlcpy (v, text, maxlen);
    _pplugin_update_parameter (pobj);
    xitk_inputtext_change_text (pobj->value, v);
  }
  else
    xine_error (pobj->info->gui, _("parameter type POST_PARAM_TYPE_STRING not supported yet.\n"));

}

static void _pplugin_set_param_stringlist(xitk_widget_t *w, void *data, int value) {
  post_object_t *pobj = (post_object_t *) data;

  (void)w;
  (void)value;
  if(pobj->readonly)
    return;

  xine_error (pobj->info->gui, _("parameter type POST_PARAM_TYPE_STRINGLIST not supported yet.\n"));
}

static void _pplugin_set_param_bool(xitk_widget_t *w, void *data, int state) {
  post_object_t *pobj = (post_object_t *) data;
  int *v;

  (void)w;
  if(pobj->readonly)
    return;

  v = (int *)(pobj->param_data + pobj->param->offset);
  *v = state;
  _pplugin_update_parameter (pobj);
  xitk_checkbox_set_state (pobj->value, *v);
}

static void _pplugin_add_parameter_widget (post_object_t *pobj) {

  if (pobj) {
    post_info_t *info = pobj->info;
    xitk_label_widget_t   lb;
    char                  buffer[2048];

    snprintf(buffer, sizeof(buffer), "%s:", (pobj->param->description)
	     ? pobj->param->description : _("No description available"));

    XITK_WIDGET_INIT(&lb);
    lb.skin_element_name   = NULL;
    lb.label               = buffer;
    lb.callback            = NULL;
    lb.userdata            = NULL;
    pobj->comment =  xitk_noskin_label_create (info->win->widget_list, &lb,
      0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname);
    xitk_add_widget (info->win->widget_list, pobj->comment);

    switch(pobj->param->type) {
    case POST_PARAM_TYPE_INT:
      {
	if(pobj->param->enum_values) {
	  xitk_combo_widget_t         cmb;

          XITK_WIDGET_INIT(&cmb);
	  cmb.skin_element_name = NULL;
	  cmb.layer_above       = is_layer_above (info->gui);
	  cmb.entries           = (const char **)pobj->param->enum_values;
	  cmb.parent_wkey       = &info->win->widget_key;
	  cmb.callback          = _pplugin_set_param_int;
	  cmb.userdata          = pobj;
          pobj->value = xitk_noskin_combo_create (info->win->widget_list, &cmb, 0, 0, 365);
          xitk_add_widget (info->win->widget_list, pobj->value);
	  xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
	}
	else {
	  xitk_intbox_widget_t      ib;

          XITK_WIDGET_INIT(&ib);
          ib.fmt               = INTBOX_FMT_DECIMAL;
          ib.min               = 0;
          ib.max               = 0;
	  ib.skin_element_name = NULL;
	  ib.value             = *(int *)(pobj->param_data + pobj->param->offset);
	  ib.step              = 1;
	  ib.callback          = _pplugin_set_param_int;
	  ib.userdata          = pobj;
          pobj->value =  xitk_noskin_intbox_create (info->win->widget_list, &ib,
            0, 0, 50, 20);
          xitk_add_widget (info->win->widget_list, pobj->value);
	}
      }
      break;

    case POST_PARAM_TYPE_DOUBLE:
      {
	xitk_doublebox_widget_t      ib;

        XITK_WIDGET_INIT(&ib);
	ib.skin_element_name = NULL;
	ib.value             = *(double *)(pobj->param_data + pobj->param->offset);
	ib.step              = .5;
	ib.callback          = _pplugin_set_param_double;
	ib.userdata          = pobj;
        pobj->value =  xitk_noskin_doublebox_create (info->win->widget_list, &ib,
          0, 0, 100, 20);
        xitk_add_widget (info->win->widget_list, pobj->value);
      }
      break;

    case POST_PARAM_TYPE_CHAR:
    case POST_PARAM_TYPE_STRING:
      {
	xitk_inputtext_widget_t  inp;

        XITK_WIDGET_INIT(&inp);
	inp.skin_element_name = NULL;
	inp.text              = (char *)(pobj->param_data + pobj->param->offset);
	inp.max_length        = 256;
	inp.callback          = _pplugin_set_param_char;
	inp.userdata          = pobj;
        pobj->value =  xitk_noskin_inputtext_create (info->win->widget_list, &inp,
          0, 0, 365, 20, "Black", "Black", fontname);
        xitk_add_widget (info->win->widget_list, pobj->value);
      }
      break;

    case POST_PARAM_TYPE_STRINGLIST:
      {
	xitk_combo_widget_t         cmb;

        XITK_WIDGET_INIT(&cmb);
	cmb.skin_element_name = NULL;
	cmb.layer_above       = is_layer_above (info->gui);
	cmb.entries           = (const char **)(pobj->param_data + pobj->param->offset);
	cmb.parent_wkey       = &info->win->widget_key;
	cmb.callback          = _pplugin_set_param_stringlist;
	cmb.userdata          = pobj;
        pobj->value = xitk_noskin_combo_create (info->win->widget_list, &cmb, 0, 0, 365);
        xitk_add_widget (info->win->widget_list, pobj->value);
	xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
      }
      break;

    case POST_PARAM_TYPE_BOOL:
      {
	xitk_checkbox_widget_t    cb;

        XITK_WIDGET_INIT(&cb);
	cb.skin_element_name = NULL;
	cb.callback          = _pplugin_set_param_bool;
	cb.userdata          = pobj;
        pobj->value =  xitk_noskin_checkbox_create (info->win->widget_list, &cb, 0, 0, 12, 12);
        xitk_add_widget (info->win->widget_list, pobj->value);
	xitk_checkbox_set_state(pobj->value,
				(*(int *)(pobj->param_data + pobj->param->offset)));
      }
      break;
    }
  }
}

static void _pplugin_change_parameter (xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;

  (void)w;
  if(pobj) {

    _pplugin_destroy_widget (&pobj->value);
    _pplugin_destroy_widget (&pobj->comment);

    if(pobj->descr) {
      pobj->param    = pobj->descr->parameter;
      pobj->param    += select;
      pobj->readonly = pobj->param->readonly;
      _pplugin_add_parameter_widget (pobj);
    }

    _pplugin_paint_widgets (pobj->info);
  }
}

static void _pplugin_get_plugins(post_info_t *info) {
  do {
    int plugin_type = (info->type == POST_VIDEO) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
    const char * const *names = xine_list_post_plugins_typed (info->gui->xine, plugin_type);
    int n, i;

    if (!names)
      break;
    for (n = 0; names[n]; n++) ;
    info->win->plugin_names = (char **)malloc (sizeof (char *) * (n + 2));
    if (!info->win->plugin_names)
      break;
    info->win->plugin_names[0] = strdup (_("No Filter"));
    for (i = 0; i < n; i++)
      info->win->plugin_names[i + 1] = strdup (names[i]);
    info->win->plugin_names[i + 1] = NULL;
  } while (0);
}

static void _pplugin_close_help (xitk_widget_t *w, void *data, int state) {
  post_info_t *info = data;

  (void)w;
  (void)state;
  info->win->help_running = 0;
  xitk_unregister_event_handler (info->gui->xitk, &info->win->help_widget_key);
  xitk_window_destroy_window (info->win->helpwin);
  info->win->helpwin = NULL;
  /* xitk_dlist_init (&info->win->help_widget_list->list); */
}

static void _pplugin_destroy_only_obj(post_info_t *info, post_object_t *pobj) {
  if(pobj) {

    if(pobj->properties) {

      _pplugin_destroy_widget (&pobj->properties);

      VFREE(pobj->param_data);
      pobj->param_data = NULL;

      if(pobj->properties_names) {
	int pnum = 0;

	while(pobj->properties_names[pnum]) {
	  free(pobj->properties_names[pnum]);
	  pnum++;
	}

	free(pobj->properties_names);
	pobj->properties_names = NULL;
      }
    }

    _pplugin_destroy_widget (&pobj->comment);
    _pplugin_destroy_widget (&pobj->value);

    if(pobj->api && pobj->help) {
      if(info->win->help_running)
        _pplugin_close_help (NULL, info, 0);
      _pplugin_destroy_widget (&pobj->help);
    }

  }
}

static int pplugin_help_event (void *data, const xitk_be_event_t *e) {
  post_info_t *info = data;

  if (((e->type == XITK_EV_KEY_DOWN) && (e->utf8[0] == XITK_CTRL_KEY_PREFIX) && (e->utf8[1] == XITK_KEY_ESCAPE))
    || (e->type == XITK_EV_DEL_WIN)) {
    _pplugin_close_help (NULL, info, 0);
    return 1;
  }
  return gui_handle_be_event (info->gui, e);
}

static int __line_wrap(char *s, int pos, int line_size)
{
  int word_size = 0;

  while( *s && *s != '\t' && *s != ' ' && *s != '\n' ) {
    s++;
    word_size++;
  }

  if( word_size >= line_size )
    return pos > line_size;

  return word_size + pos > line_size;
}

static void _pplugin_show_help (xitk_widget_t *w, void *data, int state) {
  post_object_t *pobj = (post_object_t *) data;
  post_info_t *info = pobj->info;
  gGui_t *gui = info->gui;
  xitk_image_t *bg = NULL;
  int                         x, y;
  xitk_labelbutton_widget_t   lb;
  xitk_browser_widget_t       br;

  (void)w;
  (void)state;
  if(!pobj->api) {
    if(info->win->help_running)
      _pplugin_close_help (NULL, info, 0);
    return;
  }

  /* create help window if needed */

  if( !info->win->help_running ) {
    xitk_widget_list_t *help_widget_list;

    x = y = 80;
    info->win->helpwin = xitk_window_create_dialog_window(gui->xitk,
                                                                    _("Plugin Help"), x, y,
                                                                    HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT);

    set_window_states_start(gui, info->win->helpwin);

    help_widget_list = xitk_window_widget_list(info->win->helpwin);

    bg = xitk_window_get_background_image (info->win->helpwin);

    XITK_WIDGET_INIT(&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Close");
    lb.align             = ALIGN_CENTER;
    lb.callback          = _pplugin_close_help;
    lb.state_callback    = NULL;
    lb.userdata          = info;
    lb.skin_element_name = NULL;
    w =  xitk_noskin_labelbutton_create (help_widget_list,
      &lb, HELP_WINDOW_WIDTH - (100 + 15), HELP_WINDOW_HEIGHT - (23 + 15), 100, 23,
      "Black", "Black", "White", btnfontname);
    xitk_add_widget (help_widget_list, w);
    xitk_enable_and_show_widget(w);


    XITK_WIDGET_INIT(&br);
    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
    br.browser.num_entries           = 0;
    br.browser.entries               = NULL;
    br.callback                      = NULL;
    br.dbl_click_callback            = NULL;
    br.userdata                      = NULL;
    info->win->help_browser = xitk_noskin_browser_create (help_widget_list,
      &br, 15, 34, HELP_WINDOW_WIDTH - (30 + 16), 20, 16, br_fontname);
    xitk_add_widget (help_widget_list, info->win->help_browser);
  }

  /* load text to the browser widget */
  {
    char  *p, **hbuf = NULL;
    int    lines = 0, i;

    p = pobj->api->get_help();

    do {
      char c, *old_p = p, *new_p;
      int w;

      for(w = 0; !__line_wrap(p,w,BROWSER_LINE_WIDTH) && (c = *p++) != 0 && c != '\n'; w++)
	if (c == '\t') {
	  w = (w + 1) | 7; /* allow for loop increment */
	  if (w >= BROWSER_LINE_WIDTH)
	    w = BROWSER_LINE_WIDTH - 1;
        }

      hbuf  = (char **) realloc(hbuf, sizeof(char *) * (lines + 2));
      hbuf[lines]    = malloc(w + 2);
      hbuf[lines+1]  = NULL;

      new_p = p;
      p = old_p;
      for(i = 0; i < w; i++)
        switch (c = *p++) {
        case '\0':
        case '\n':
          hbuf[lines][i] = '\0';
          i = w;
          break;
        case '\t':
	  do {
	    hbuf[lines][i] = ' ';
	  } while (++i & 7 && i < BROWSER_LINE_WIDTH);
	  --i; /* allow for loop increment */
	  break;
        default:
	  hbuf[lines][i] = c;
        }
      hbuf[lines][i] = '\0';
      p = new_p;

      lines++;
    } while( *p );

    if(lines) {
      char **ohbuf = info->win->help_text;
      int    i = 0;

      info->win->help_text = hbuf;
      xitk_browser_update_list(info->win->help_browser, (const char **)info->win->help_text,
                               NULL, lines, 0);

      if(ohbuf) {
	while(ohbuf[i++])
	  free(ohbuf[i]);
	free(ohbuf);
      }
    }
  }

  if( !info->win->help_running ) {

    xitk_enable_and_show_widget(info->win->help_browser);
    xitk_browser_set_alignment(info->win->help_browser, ALIGN_LEFT);

    xitk_window_set_background_image (info->win->helpwin, bg);

    info->win->help_widget_key = xitk_be_register_event_handler (
      info->type == POST_VIDEO ? "vpplugin_help" : "applugin_help",
      info->win->helpwin, NULL, pplugin_help_event, info, NULL, NULL);

    info->win->help_running = 1;
  }

  raise_window (info->gui, info->win->helpwin, 1, info->win->help_running);

  xitk_window_try_to_set_input_focus(info->win->helpwin);
}

static void _pplugin_retrieve_parameters(post_info_t *info, post_object_t *pobj) {
  xitk_combo_widget_t         cmb;
  xitk_labelbutton_widget_t   lb;

  if(__pplugin_retrieve_parameters(pobj)) {

    XITK_WIDGET_INIT(&cmb);
    cmb.skin_element_name = NULL;
    cmb.layer_above       = is_layer_above (pobj->info->gui);
    cmb.entries           = (const char **)pobj->properties_names;
    cmb.parent_wkey       = &info->win->widget_key;
    cmb.callback          = _pplugin_change_parameter;
    cmb.userdata          = pobj;
    pobj->properties = xitk_noskin_combo_create (info->win->widget_list, &cmb, 0, 0, 175);
    xitk_add_widget (info->win->widget_list, pobj->properties);
    xitk_combo_set_select(pobj->properties, 0);
    cmb.callback (pobj->properties, pobj, 0);

    if(pobj->api && pobj->api->get_help) {

      XITK_WIDGET_INIT(&lb);
      lb.button_type       = CLICK_BUTTON;
      lb.label             = _("Help");
      lb.align             = ALIGN_CENTER;
      lb.callback          = _pplugin_show_help;
      lb.state_callback    = NULL;
      lb.userdata          = pobj;
      lb.skin_element_name = NULL;
      pobj->help =  xitk_noskin_labelbutton_create (info->win->widget_list,
        &lb, 0, 0, 63, 20, "Black", "Black", "White", btnfontname);
      xitk_add_widget (info->win->widget_list, pobj->help);
    }
  }
  else {
    xitk_label_widget_t   lb;

    XITK_WIDGET_INIT(&lb);
    lb.skin_element_name   = NULL;
    lb.label               = _("There is no parameter available for this plugin.");
    lb.callback            = NULL;
    lb.userdata            = NULL;
    pobj->comment =  xitk_noskin_label_create (info->win->widget_list, &lb,
      0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname);
    xitk_add_widget (info->win->widget_list, pobj->comment);
    pobj->properties = NULL;
  }
}

static post_object_t *_pplugin_create_filter_object (post_info_t *info);

static void _pplugin_select_filter (xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;
  post_info_t *info = pobj->info;
  gGui_t *gui = info->gui;
  post_object_t **p = info->win->post_objects;
  int n;

  (void)w;
  _pplugin_unwire(info);

  for (n = 0; p[n] && (p[n] != pobj); n++) ;
  if (!select) {
    /* User selected first line which is "No filter". Reset this one, and kill everything thereafter. */
    if (p[n]) {
      _pplugin_destroy_only_obj (info, pobj);
      if (pobj->post) {
        xine_post_dispose (gui->xine, pobj->post);
        pobj->post = NULL;
      }
      n++;
      info->win->object_num = n;
      while (p[n]) {
        _pplugin_destroy_only_obj (info, p[n]);
        if (p[n]->post) {
          xine_post_dispose (gui->xine, p[n]->post);
          p[n]->post = NULL;
        }
        _pplugin_destroy_widget (&p[n]->plugins);
        _pplugin_destroy_widget (&p[n]->frame);
        _pplugin_destroy_widget (&p[n]->up);
        _pplugin_destroy_widget (&p[n]->down);
        VFREE (p[n]);
        p[n] = NULL;
        n++;
      }
      xitk_hide_widget (info->win->enable);
      xitk_hide_widget (info->win->exit);
      xitk_window_clear_window (info->win->xwin);
      xitk_show_widget (info->win->enable);
      xitk_show_widget (info->win->exit);
    }
    if(info->win->object_num <= MAX_DISPLAY_FILTERS)
      info->win->first_displayed = 0;
    else if((info->win->object_num - info->win->first_displayed) < MAX_DISPLAY_FILTERS)
      info->win->first_displayed = info->win->object_num - MAX_DISPLAY_FILTERS;
  } else {
    /* Some filter has been selected. Kill old one and set new. */
    _pplugin_destroy_only_obj (info, pobj);
    if (pobj->post) {
      xine_post_dispose (gui->xine, pobj->post);
      pobj->post = NULL;
    }
    pobj->post = xine_post_init (gui->xine,
      xitk_combo_get_current_entry_selected (w), 0, &gui->ao_port, &gui->vo_port);
    _pplugin_retrieve_parameters (info, pobj);
    /* If this is the last filter, add a new "No filter" thereafter. */
    if ((n < MAX_USED_FILTERS - 1) && (n + 1 == info->win->object_num)) {
      _pplugin_create_filter_object (info);
    }
  }

  if(info->win->help_running)
    _pplugin_show_help (NULL, pobj, 0);

  _pplugin_rewire (info);
  _pplugin_paint_widgets(info);
}

static void _pplugin_move_up (xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_info_t *info = pobj->info;
  post_object_t **ppobj = info->win->post_objects;

  (void)w;
  _pplugin_unwire(info);

  while(*ppobj != pobj)
    ppobj++;

  *ppobj       = *(ppobj - 1);
  *(ppobj - 1) = pobj;

  _pplugin_rewire (info);
  _pplugin_paint_widgets(info);
}

static void _pplugin_move_down (xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_info_t *info = pobj->info;
  post_object_t **ppobj = info->win->post_objects;

  (void)w;
  _pplugin_unwire(info);

  while(*ppobj != pobj)
    ppobj++;

  *ppobj       = *(ppobj + 1);
  *(ppobj + 1) = pobj;

  _pplugin_rewire (info);
  _pplugin_paint_widgets(info);
}

static post_object_t *_pplugin_create_filter_object (post_info_t *info) {
  gGui_t *gui = gGui;
  xitk_combo_widget_t   cmb;
  post_object_t        *pobj;
  xitk_image_widget_t   im;
  xitk_image_t         *image;
  xitk_button_widget_t  b;

  if (info->win->object_num >= MAX_USED_FILTERS - 1)
    return NULL;
  pobj = calloc (1, sizeof (post_object_t));
  if (!pobj)
    return NULL;

  pobj->info = info;

  info->win->x = 15;
  info->win->y += FRAME_HEIGHT + 4;

  info->win->post_objects[info->win->object_num] = pobj;
  pobj->x = info->win->x;
  pobj->y = info->win->y;
  info->win->object_num++;

  image = xitk_image_new (gui->xitk, NULL, 0, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);

  xitk_image_fill_rectangle(image,
                            0, 0, FRAME_WIDTH + 1, FRAME_HEIGHT + 1,
                            xitk_get_cfg_num (gui->xitk, XITK_BG_COLOR));

  /* Some decorations */
  xitk_image_draw_outter_frame(image, NULL, NULL,
                               0, 0, FRAME_WIDTH, FRAME_HEIGHT);
  xitk_image_draw_rectangular_box (image, 27, 28, FRAME_WIDTH - 27 - 4, 2, DRAW_OUTTER | DRAW_LIGHT);
  xitk_image_draw_rectangular_box (image, 26, 5, 2, FRAME_HEIGHT - 9, DRAW_OUTTER | DRAW_LIGHT);
  xitk_image_draw_inner_frame(image, NULL, NULL, 5, 5, 16, 16);
  xitk_image_draw_rectangular_box (image, 5, 24, 2, FRAME_HEIGHT - 47, DRAW_INNER | DRAW_LIGHT);
  xitk_image_draw_rectangular_box (image, 10, 24, 2, FRAME_HEIGHT - 47, DRAW_INNER | DRAW_LIGHT);
  xitk_image_draw_rectangular_box (image, 15, 24, 2, FRAME_HEIGHT - 47, DRAW_INNER | DRAW_LIGHT);
  xitk_image_draw_rectangular_box (image, 20, 24, 2, FRAME_HEIGHT - 47, DRAW_INNER | DRAW_LIGHT);
  xitk_image_draw_inner_frame(image, NULL, NULL, 5, FRAME_HEIGHT - 16 - 5, 16, 16);

  XITK_WIDGET_INIT(&im);
  im.skin_element_name = NULL;
  pobj->frame =  xitk_noskin_image_create (info->win->widget_list, &im, image, pobj->x - 5, pobj->y - 5);
  xitk_add_widget (info->win->widget_list, pobj->frame);

  DISABLE_ME(pobj->frame);

  XITK_WIDGET_INIT(&cmb);
  cmb.skin_element_name = NULL;
  cmb.layer_above       = is_layer_above (info->gui);
  cmb.entries           = (const char **)info->win->plugin_names;
  cmb.parent_wkey       = &info->win->widget_key;
  cmb.callback          = _pplugin_select_filter;
  cmb.userdata          = pobj;
  pobj->plugins = xitk_noskin_combo_create (info->win->widget_list, &cmb, 0, 0, 175);
  xitk_add_widget (info->win->widget_list, pobj->plugins);
  DISABLE_ME(pobj->plugins);
  xitk_combo_set_select(pobj->plugins, 0);

  XITK_WIDGET_INIT(&b);
  b.skin_element_name = "XITK_NOSKIN_UP";
  b.callback          = _pplugin_move_up;
  b.userdata          = pobj;
  pobj->up = xitk_noskin_button_create (info->win->widget_list, &b, 0, 0, 17, 17);
  xitk_add_widget (info->win->widget_list, pobj->up);
  DISABLE_ME(pobj->up);

  b.skin_element_name = "XITK_NOSKIN_DOWN";
  b.callback          = _pplugin_move_down;
  b.userdata          = pobj;
  pobj->down = xitk_noskin_button_create (info->win->widget_list, &b, 0, 0, 17, 17);
  xitk_add_widget (info->win->widget_list, pobj->down);
  DISABLE_ME(pobj->down);

  return pobj;
}

static int _pplugin_rebuild_filters(post_info_t *info) {
  post_element_t  **pelem = info->elements;
  int plugin_type = info->type == POST_VIDEO ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  int num_filters = 0;

  while(pelem && *pelem) {
    int i = 0;

    if ((*pelem)->post->type == plugin_type) {
      num_filters++;

      if (!_pplugin_create_filter_object (info))
        break;
      info->win->post_objects[info->win->object_num - 1]->post = (*pelem)->post;

      while(info->win->plugin_names[i] && strcasecmp(info->win->plugin_names[i], (*pelem)->name))
        i++;

      if(info->win->plugin_names[i]) {
        xitk_combo_set_select(info->win->post_objects[info->win->object_num - 1]->plugins, i);

        _pplugin_retrieve_parameters(info, info->win->post_objects[info->win->object_num - 1]);
      }
    }

    pelem++;
  }
  /* Always have 1 "No filter" at the end for later extension. */
  _pplugin_create_filter_object (info);

  _pplugin_paint_widgets(info);

  return num_filters;
}

static void _pplugin_list_step (post_info_t *info, int step) {
  int max, newpos;

  max = info->win->object_num - MAX_DISPLAY_FILTERS;
  if (max < 0)
    max = 0;
  newpos = info->win->first_displayed + step;
  if (newpos > max)
    newpos = max;
  if (newpos < 0)
    newpos = 0;
  if (newpos == info->win->first_displayed)
    return;
  info->win->first_displayed = newpos;
  _pplugin_paint_widgets (info);
}

static void _pplugin_nextprev (xitk_widget_t *w, void *data, int pos) {
  post_info_t *info = data;
  int rpos = (xitk_slider_get_max(info->win->slider)) - pos;

  (void)w;
  if(rpos != info->win->first_displayed) {
    info->win->first_displayed = rpos;
    _pplugin_paint_widgets (info);
  }
}

static void _pplugin_exit (xitk_widget_t *w, void *data, int state) {
  post_info_t *info = data;
  gGui_t *gui = info->gui;

  (void)w;
  (void)state;
  if (info->win) {
    int           i;

    if (info->win->help_running)
     _pplugin_close_help (NULL, info, 0);

    if (info->win->help_text) {
      i = 0;
      while(info->win->help_text[i++])
	free(info->win->help_text[i]);
      free(info->win->help_text);
    }

    _pplugin_save_chain (info);

    info->win->running = 0;
    info->win->visible = 0;

    gui_save_window_pos (info->gui, info->type == POST_VIDEO ? "vpplugin" : "applugin", info->win->widget_key);

    {
      post_object_t **p = info->win->post_objects;
      while (*p) {
        _pplugin_destroy_only_obj (info, *p);
        _pplugin_destroy_widget (&(*p)->plugins);
        _pplugin_destroy_widget (&(*p)->frame);
        _pplugin_destroy_widget (&(*p)->up);
        _pplugin_destroy_widget (&(*p)->down);
        VFREE (*p);
        *p = NULL;
        p++;
      }
    }
    info->win->object_num = 0;

    for(i = 0; info->win->plugin_names[i]; i++)
      free(info->win->plugin_names[i]);

    free(info->win->plugin_names);

    xitk_unregister_event_handler (info->gui->xitk, &info->win->widget_key);

    xitk_window_destroy_window(info->win->xwin);
    /* xitk_dlist_init (&info->win->widget_list->list); */

    VFREE(info->win);
    info->win = NULL;

    video_window_set_input_focus(gui->vwin);
  }
}

static int pplugin_event (void *data, const xitk_be_event_t *e) {
  post_info_t *info = data;
  int step = 0;

  switch (e->type) {
    case XITK_EV_DEL_WIN:
      _pplugin_exit (NULL, info, 0);
      return 1;
    case XITK_EV_BUTTON_DOWN:
      if (e->code == 4)
        step = -1;
      else if (e->code == 5)
        step = 1;
      break;
    case XITK_EV_KEY_DOWN:
      if (e->utf8[0] == XITK_CTRL_KEY_PREFIX) {
        switch (e->utf8[1]) {
          case XITK_MOUSE_WHEEL_UP:
          case XITK_KEY_UP:
            step = -1;
            break;
          case XITK_MOUSE_WHEEL_DOWN:
          case XITK_KEY_DOWN:
            step = 1;
            break;
          case XITK_KEY_NEXT:
            step = MAX_DISPLAY_FILTERS;
            break;
          case XITK_KEY_PREV:
            step = -MAX_DISPLAY_FILTERS;
            break;
          case XITK_KEY_ESCAPE:
            _pplugin_exit (NULL, info, 0);
            return 1;
        }
      }
    default: ;
  }
  if (step && !(e->qual & (MODIFIER_SHIFT | MODIFIER_CTRL))) {
    _pplugin_list_step (info, step);
    return 1;
  }
  return gui_handle_be_event (info->gui, e);
}


static void _pplugin_enability (xitk_widget_t *w, void *data, int state, int modifier) {
  post_info_t *info = data;
  gGui_t *gui = info->gui;

  (void)w;
  (void)modifier;
  if (info->type == POST_VIDEO)
    gui->post_video_enable = state;
  else
    gui->post_audio_enable = state;

  _pplugin_unwire(info);
  _pplugin_rewire (info);
}

int pplugin_is_post_selected (post_info_t *info) {
  if (!info)
    return 0;

  if(info->win) {
    int post_num = info->win->object_num;

    if(!xitk_combo_get_current_selected(info->win->post_objects[post_num - 1]->plugins))
      post_num--;

    return (post_num > 0);
  }

  return info->num_elements;
}

void pplugin_rewire_from_posts_window (post_info_t *info) {
  if (info && info->win) {
    _pplugin_unwire (info);
    _pplugin_rewire (info);
  }
}

void pplugin_rewire_posts (post_info_t *info) {
  if (info) {
    _pplugin_unwire (info);
    _pplugin_rewire (info);
  }
}

void pplugin_end (post_info_t *info) {
  _pplugin_exit (NULL, info, 0);
}

int pplugin_is_visible (post_info_t *info) {
  if (info && info->win) {
    if (info->gui->use_root_window)
      return (xitk_window_flags (info->win->xwin, 0, 0) & XITK_WINF_VISIBLE);
    else
      return info->win->visible && (xitk_window_flags (info->win->xwin, 0, 0) & XITK_WINF_VISIBLE);
  }
  return 0;
}

int pplugin_is_running (post_info_t *info) {
  return (info && info->win) ? info->win->running : 0;
}

void pplugin_raise_window (post_info_t *info) {
  if (info && info->win)
    raise_window (info->gui, info->win->xwin, info->win->visible, info->win->running);
}


void pplugin_toggle_visibility (xitk_widget_t *w, void *data) {
  post_info_t *info = data;

  (void)w;
  if (info && info->win)
    toggle_window (info->gui, info->win->xwin, info->win->widget_list,
      &info->win->visible, info->win->running);
}

void pplugin_update_enable_button (post_info_t *info) {
  if (info && info->win) {
    gGui_t *gui = info->gui;
    xitk_labelbutton_set_state (info->win->enable, info->type == POST_VIDEO ? gui->post_video_enable : gui->post_audio_enable);
  }
}


void pplugin_panel (post_info_t *info) {
  gGui_t *gui;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_image_t               *bg;
  int                         x, y;
  xitk_slider_widget_t        sl;

  if (!info)
    return;
  if (info->win)
    return;
  gui = info->gui;

  info->win = (post_win_t *) calloc(1, sizeof (*info->win));
  info->win->first_displayed = 0;
  info->win->help_text       = NULL;

  x = y = 80;
  gui_load_window_pos (gui, info->type == POST_VIDEO ? "vpplugin" : "applugin", &x, &y);

  info->win->xwin = xitk_window_create_dialog_window (gui->xitk,
    info->type == POST_VIDEO ? _("Video Chain Reaction") : _("Audio Chain Reaction"),
    x, y, WINDOW_WIDTH, WINDOW_HEIGHT);

  set_window_states_start (gui, info->win->xwin);

  info->win->widget_list = xitk_window_widget_list (info->win->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&lbl);
  XITK_WIDGET_INIT(&cb);

  bg = xitk_window_get_background_image (info->win->xwin);

  XITK_WIDGET_INIT(&sl);

  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = _pplugin_nextprev;
  sl.motion_userdata          = info;
  info->win->slider =  xitk_noskin_slider_create (info->win->widget_list, &sl,
    (WINDOW_WIDTH - (16 + 15)), 34, 16, (MAX_DISPLAY_FILTERS * (FRAME_HEIGHT + 4) - 4), XITK_VSLIDER);
  xitk_add_widget (info->win->widget_list, info->win->slider);

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Enable");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = _pplugin_enability;
  lb.userdata          = info;
  lb.skin_element_name = NULL;
  info->win->enable =  xitk_noskin_labelbutton_create (info->win->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (info->win->widget_list, info->win->enable);

  xitk_labelbutton_set_state (info->win->enable, info->type == POST_VIDEO ? gui->post_video_enable : gui->post_audio_enable);
  xitk_enable_and_show_widget (info->win->enable);

  /* IMPLEMENT ME
  x += (100 + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  w =  xitk_noskin_labelbutton_create (info->win->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (info->win->widget_list, w);
  xitk_enable_and_show_widget(w);
  */

  x = WINDOW_WIDTH - (100 + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("OK");
  lb.align             = ALIGN_CENTER;
  lb.callback          = _pplugin_exit;
  lb.state_callback    = NULL;
  lb.userdata          = info;
  lb.skin_element_name = NULL;
  info->win->exit =  xitk_noskin_labelbutton_create (info->win->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (info->win->widget_list, info->win->exit);
  xitk_enable_and_show_widget (info->win->exit);

  _pplugin_get_plugins(info);

  info->win->x = 15;
  info->win->y = 34 - (FRAME_HEIGHT + 4);

  _pplugin_rebuild_filters (info);

  xitk_window_set_background_image (info->win->xwin, bg);

  info->win->widget_key = xitk_be_register_event_handler (
    info->type == POST_VIDEO ? "vpplugin" : "applugin", info->win->xwin, NULL, pplugin_event, info, NULL, NULL);

  info->win->visible = 1;
  info->win->running = 1;

  _pplugin_paint_widgets(info);

  pplugin_raise_window(info);

  xitk_window_try_to_set_input_focus(info->win->xwin);
}

void pplugin_parse_and_store_post (post_info_t *info, const char *post_chain) {
  post_element_t ***_post_elements;
  int *_post_elements_num;
  post_element_t **posts;
  int              num;

  if (!info)
    return;

  _post_elements = &info->elements;
  _post_elements_num = &info->num_elements;
  posts = NULL;

  if((posts = pplugin_parse_and_load(info, post_chain, &num))) {
    if(*_post_elements_num) {
      int i;
      int ptot = *_post_elements_num + num;

      *_post_elements = (post_element_t **) realloc(*_post_elements, sizeof(post_element_t *) * (ptot + 1));
      for(i = *_post_elements_num; i <  ptot; i++)
	(*_post_elements)[i] = posts[i - *_post_elements_num];

      (*_post_elements)[i]   = NULL;
      (*_post_elements_num) += num;

      free(posts);
    }
    else {
      *_post_elements     = posts;
      *_post_elements_num = num;
    }
  }
}

static void _pplugin_deinit (post_info_t *info) {
  int i;
#if 0
  /* requires <xine/xine_internal.h> */
  info->gui->xine->config->unregister_callback (info->gui->xine, "gui.deinterlace_plugin");
#endif
  for (i = 0; i < info->num_elements; i++) {
    xine_post_dispose (info->gui->xine, info->elements[i]->post);
    free (info->elements[i]->name);
    VFREE (info->elements[i]);
  }
  SAFE_FREE (info->elements);
  info->num_elements= 0;
}

static void _vpplugin_deinit (post_info_t *info) {
  int i;

  _pplugin_deinit (info);

  for (i = 0; i < info->info.video.num_deinterlace_elements; i++) {
    xine_post_dispose (info->gui->xine, info->info.video.deinterlace_elements[i]->post);
    free (info->info.video.deinterlace_elements[i]->name);
    VFREE (info->info.video.deinterlace_elements[i]);
  }
  SAFE_FREE (info->info.video.deinterlace_elements);
  info->info.video.num_deinterlace_elements = 0;
}

static void _applugin_deinit (post_info_t *info) {

  _pplugin_deinit (info);

  if (info->info.audio.audio_vis_plugins) {
    int i;
    for (i = 0; info->info.audio.audio_vis_plugins[i]; i++) {
      free (info->info.audio.audio_vis_plugins[i]);
      info->info.audio.audio_vis_plugins[i] = NULL;
    }
    free (info->info.audio.audio_vis_plugins);
    info->info.audio.audio_vis_plugins = NULL;
  }
}

void post_deinit (gGui_t *gui) {
  if (!gui)
    return;

  if (gui->post_video_enable || gui->deinterlace_enable)
    _vpplugin_unwire (gui);

  if (gui->post_audio_enable)
    _applugin_unwire (gui);

  _vpplugin_deinit (&gui->post_video);
  _applugin_deinit (&gui->post_audio);
}

static const char *_pplugin_get_default_deinterlacer(void) {
  return DEFAULT_DEINTERLACER;
}

void post_deinterlace_init (gGui_t *gui, const char *deinterlace_post) {
  post_element_t **posts = NULL;
  int              num;
  const char      *deinterlace_default;
  post_info_t *vinfo;

  if (!gui)
    return;

  vinfo = &gui->post_video;

  deinterlace_default = _pplugin_get_default_deinterlacer();

  vinfo->info.video.deinterlace_plugin =
    (char *) xine_config_register_string (gui->xine, "gui.deinterlace_plugin",
					  deinterlace_default,
					  _("Deinterlace plugin."),
					  _("Plugin (with optional parameters) to use "
					    "when deinterlace is used (plugin separator is ';')."),
					  CONFIG_LEVEL_ADV,
					  post_deinterlace_plugin_cb,
					  gui);
  if ((posts = pplugin_parse_and_load (vinfo,
    (deinterlace_post && strlen(deinterlace_post)) ? deinterlace_post : vinfo->info.video.deinterlace_plugin, &num))) {
    vinfo->info.video.deinterlace_elements     = posts;
    vinfo->info.video.num_deinterlace_elements = num;
  }

}

void post_deinterlace (gGui_t *gui) {
  post_info_t *vinfo;

  if (!gui)
    return;

  vinfo = &gui->post_video;

  if( !vinfo->info.video.num_deinterlace_elements ) {
    /* fallback to the old method */
    xine_set_param (gui->stream, XINE_PARAM_VO_DEINTERLACE, gui->deinterlace_enable);
  }
  else {
    _pplugin_unwire (vinfo);
    _pplugin_rewire (vinfo);
  }
}
