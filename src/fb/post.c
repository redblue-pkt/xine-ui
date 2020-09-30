/*
 * Copyright (C) 2008 by Dirk Meyer
 * Copyright (C) 2003-2020 the xine project
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
 * The code is taken from xine-ui/src/xitk/post.c at changed to work with fbxine
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "post.h"
#include "main.h"

#include "globals.h"
#include "utils.h"
#include "libcommon.h"  /* strsep replacement */

typedef struct {
  xine_post_t                 *post;
  const xine_post_api_t             *api;
  const xine_post_api_descr_t       *descr;
  const xine_post_api_parameter_t   *param;
  char                        *param_data;

  int                          x;
  int                          y;

  int                          readonly;

  char                       **properties_names;
} post_object_t;


static int __pplugin_retrieve_parameters(post_object_t *pobj) {
  const xine_post_in_t             *input_api;

  if((input_api = (xine_post_in_t *) xine_post_input(pobj->post, "parameters"))) {
    const xine_post_api_t            *post_api;
    const xine_post_api_descr_t      *api_descr;
    const xine_post_api_parameter_t  *parm;
    int                         pnum = 0;

    post_api = (xine_post_api_t *) input_api->data;

    api_descr = post_api->get_param_descr();

    parm = api_descr->parameter;
    pobj->param_data = malloc(api_descr->struct_size);

    while(parm->type != POST_PARAM_TYPE_LAST) {

      post_api->get_parameters(pobj->post, pobj->param_data);

      if(!pnum)
	pobj->properties_names = (char **) malloc(sizeof(char *) * 2);
      else
	pobj->properties_names = (char **)
	  realloc(pobj->properties_names, sizeof(char *) * (pnum + 2));

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


/* -post <name>:option1=value1,option2=value2... */
static post_element_t **pplugin_parse_and_load(int plugin_type, const char *pchain, int *post_elements_num) {
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

	post = xine_post_init(__xineui_global_xine_instance, plugin, 0, &fbxine.audio_port, &fbxine.video_port);

        if (post && plugin_type) {
          if (post->type != plugin_type) {
            xine_post_dispose(__xineui_global_xine_instance, post);
            post = NULL;
          }
        }

	if(post) {
	  post_object_t  pobj;

	  if(!(*post_elements_num))
	    post_elements = (post_element_t **) malloc(sizeof(post_element_t *) * 2);
	  else
	    post_elements = (post_element_t **)
	      realloc(post_elements, sizeof(post_element_t *) * ((*post_elements_num) + 2));

	  post_elements[(*post_elements_num)] = (post_element_t *)
	    malloc(sizeof(post_element_t));
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

	    free(pobj.param_data);
	  }
	}

	free(plugin);
      }
    }
    free(post_chain);
  }

  return post_elements;
}


static void pplugin_parse_and_store_post(int plugin_type, const char *post_chain) {
  post_element_t ***_post_elements = (plugin_type == XINE_POST_TYPE_VIDEO_FILTER) ? &fbxine.post_video_elements : &fbxine.post_audio_elements;
  int *_post_elements_num = (plugin_type == XINE_POST_TYPE_VIDEO_FILTER) ? &fbxine.post_video_elements_num : &fbxine.post_audio_elements_num;
  post_element_t **posts = NULL;
  int              num;

  if((posts = pplugin_parse_and_load(plugin_type, post_chain, &num))) {
    if(*_post_elements_num) {
      int i;
      int ptot = *_post_elements_num + num;

      *_post_elements = (post_element_t **) realloc(*_post_elements,
							sizeof(post_element_t *) * (ptot + 1));
      for(i = *_post_elements_num; i <  ptot; i++)
	(*_post_elements)[i] = posts[i - *_post_elements_num];

      (*_post_elements)[i] = NULL;
      (*_post_elements_num) += num;

      free(posts);
    }
    else {
      *_post_elements     = posts;
      *_post_elements_num = num;
    }
  }
}


void vpplugin_parse_and_store_post(const char *post_chain) {
  pplugin_parse_and_store_post(XINE_POST_TYPE_VIDEO_FILTER, post_chain);
}


void applugin_parse_and_store_post(const char *post_chain) {
  pplugin_parse_and_store_post(XINE_POST_TYPE_AUDIO_FILTER, post_chain);
}


static void _vpplugin_unwire(void) {
  xine_post_out_t  *vo_source;

  vo_source = xine_get_video_source(fbxine.stream);

  (void) xine_post_wire_video_port(vo_source, fbxine.video_port);
}


static void _applugin_unwire(void) {
  xine_post_out_t  *ao_source;

  ao_source = xine_get_audio_source(fbxine.stream);

  (void) xine_post_wire_audio_port(ao_source, fbxine.audio_port);
}


static void _vpplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {

  if(post_elements_num) {
    xine_post_out_t   *vo_source;
    int                i = 0;

    for(i = (post_elements_num - 1); i >= 0; i--) {

      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *vo_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_video_port((xine_post_out_t *) vo_out, fbxine.video_port);
      }
      else {
	const xine_post_in_t *vo_in;

	/* look for standard input names */
	vo_in = xine_post_input(post_elements[i + 1]->post, "video");
	if( !vo_in )
	  vo_in = xine_post_input(post_elements[i + 1]->post, "video in");

	xine_post_wire((xine_post_out_t *) vo_out, (xine_post_in_t *) vo_in);
      }
    }

    vo_source = xine_get_video_source(fbxine.stream);
    xine_post_wire_video_port(vo_source, post_elements[0]->post->video_input[0]);
  }
}


static void _applugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {

  if(post_elements_num) {
    xine_post_out_t   *ao_source;
    int                i = 0;

    for(i = (post_elements_num - 1); i >= 0; i--) {

      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *ao_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_audio_port((xine_post_out_t *) ao_out, fbxine.audio_port);
      }
      else {
	const xine_post_in_t *ao_in;

	/* look for standard input names */
	ao_in = xine_post_input(post_elements[i + 1]->post, "audio");
	if( !ao_in )
	  ao_in = xine_post_input(post_elements[i + 1]->post, "audio in");

	xine_post_wire((xine_post_out_t *) ao_out, (xine_post_in_t *) ao_in);
      }
    }

    ao_source = xine_get_audio_source(fbxine.stream);
    xine_post_wire_audio_port(ao_source, post_elements[0]->post->audio_input[0]);
  }
}


static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num) {
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( fbxine.post_video_enable )
    *post_elements_num += fbxine.post_video_elements_num;
  if( fbxine.deinterlace_enable )
    *post_elements_num += fbxine.deinterlace_elements_num;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **)
    malloc(sizeof(post_element_t *) * (*post_elements_num));

  for( i = 0; fbxine.deinterlace_enable && i < fbxine.deinterlace_elements_num; i++ ) {
    post_elements[i+j] = fbxine.deinterlace_elements[i];
  }

  for( j = 0; fbxine.post_video_enable && j < fbxine.post_video_elements_num; j++ ) {
    post_elements[i+j] = fbxine.post_video_elements[j];
  }

  return post_elements;
}

static post_element_t **_pplugin_join_visualization_and_post_elements(int *post_elements_num) {
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( fbxine.post_audio_enable )
    *post_elements_num += fbxine.post_audio_elements_num;
/*
  if( fbxine.visual_anim_enable )
    *post_elements_num++;
*/
  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **)
    malloc(sizeof(post_element_t *) * (*post_elements_num));

  for( j = 0; fbxine.post_audio_enable && j < fbxine.post_audio_elements_num; j++ ) {
    post_elements[i+j] = fbxine.post_audio_elements[j];
  }
/*
  for( i = 0; fbxine.visual_anim_enable && i < 1; i++ ) {
    post_elements[i+j] = fbxine.visual_anim_element;
  }
*/
  return post_elements;
}

static void _vpplugin_rewire(void) {

  static post_element_t **post_elements;
  int post_elements_num;

  post_elements = _pplugin_join_deinterlace_and_post_elements(&post_elements_num);

  if( post_elements ) {
    _vpplugin_rewire_from_post_elements(post_elements, post_elements_num);

    free(post_elements);
  }
}

static void _applugin_rewire(void) {

  static post_element_t **post_elements;
  int post_elements_num;

  post_elements = _pplugin_join_visualization_and_post_elements(&post_elements_num);

  if( post_elements ) {
    _applugin_rewire_from_post_elements(post_elements, post_elements_num);

    free(post_elements);
  }
}



#define DEFAULT_DEINTERLACER "tvtime:method=Linear,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

#if 0  /* FIXME: unused? */
static void post_deinterlace_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  post_element_t **posts = NULL;
  int              num, i;

  fbxine.deinterlace_plugin = cfg->str_value;

  if(fbxine.deinterlace_enable)
    _pplugin_unwire();

  for(i = 0; i < fbxine.deinterlace_elements_num; i++) {
    xine_post_dispose(__xineui_global_xine_instance, fbxine.deinterlace_elements[i]->post);
    free(fbxine.deinterlace_elements[i]->name);
    free(fbxine.deinterlace_elements[i]);
  }

  SAFE_FREE(fbxine.deinterlace_elements);
  fbxine.deinterlace_elements_num = 0;

  if((posts = pplugin_parse_and_load(fbxine.deinterlace_plugin, &num))) {
    fbxine.deinterlace_elements     = posts;
    fbxine.deinterlace_elements_num = num;
  }

  if(fbxine.deinterlace_enable)
    _pplugin_rewire();
}
#endif



void post_deinterlace_init(const char *deinterlace_post) {
  post_element_t **posts = NULL;
  int              num;

  fbxine.deinterlace_plugin = DEFAULT_DEINTERLACER;

  if((posts = pplugin_parse_and_load(0, (deinterlace_post && strlen(deinterlace_post)) ?
				     deinterlace_post : fbxine.deinterlace_plugin, &num))) {
    fbxine.deinterlace_elements     = posts;
    fbxine.deinterlace_elements_num = num;
  }
}


void post_deinterlace(void) {

  if( !fbxine.deinterlace_elements_num ) {
    /* fallback to the old method */
    xine_set_param(fbxine.stream, XINE_PARAM_VO_DEINTERLACE,
                   fbxine.deinterlace_enable);
  }
  else {
    _vpplugin_unwire();
    _vpplugin_rewire();
  }
}

void vpplugin_rewire_posts(void) {

  _vpplugin_unwire();
  _vpplugin_rewire();
}

void applugin_rewire_posts(void) {

  _applugin_unwire();
  _applugin_rewire();
}

/* end of post.c */

