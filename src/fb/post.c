/* 
 * Copyright (C) 2003 by Dirk Meyer
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
 * The code is taken from xine-ui/src/xitk/post.c at changed to work with fbxine
 */

#include "post.h"
#include "main.h"


typedef struct {
  xine_post_t                 *post;
  xine_post_api_t             *api;
  xine_post_api_descr_t       *descr;
  xine_post_api_parameter_t   *param;
  char                        *param_data;

  int                          x;
  int                          y;

  int                          readonly;

  char                       **properties_names;
} post_object_t;


static int __pplugin_retrieve_parameters(post_object_t *pobj) {
  xine_post_in_t             *input_api;
  
  if((input_api = (xine_post_in_t *) xine_post_input(pobj->post, "parameters"))) {
    xine_post_api_t            *post_api;
    xine_post_api_descr_t      *api_descr;
    xine_post_api_parameter_t  *parm;
    int                         pnum = 0;
    
    post_api = (xine_post_api_t *) input_api->data;
    
    api_descr = post_api->get_param_descr();
    
    parm = api_descr->parameter;
    pobj->param_data = malloc(api_descr->struct_size);
    
    while(parm->type != POST_PARAM_TYPE_LAST) {
      
      post_api->get_parameters(pobj->post, pobj->param_data);
      
      if(!pnum)
	pobj->properties_names = (char **) xine_xmalloc(sizeof(char *) * 2);
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

void pplugin_parse_and_store_post(const char *post) {

  /* -post <name>:option1=value1,option2=value2... */
  if(post && strlen(post)) {
    char          *p = (char *) post;
    char          *plugin, *args = NULL;
    xine_post_t   *post;
    
    while(*p == ' ')
      p++;
    
    xine_strdupa(plugin, p);
    
    if((p = strchr(plugin, ':')))
      *p++ = '\0';
    
    if(p && (strlen(p) > 1))
      args = p;

    post = xine_post_init(fbxine.xine, plugin, 0, &fbxine.audio_port, 
			  &fbxine.video_port);

    if(post) {
      post_object_t  pobj;

      if(!fbxine.post_elements_num)
	fbxine.post_elements = (post_element_t **) xine_xmalloc(sizeof(post_element_t *) * 2);
      else
	fbxine.post_elements = (post_element_t **) 
	  realloc(fbxine.post_elements, sizeof(post_element_t *) * (fbxine.post_elements_num + 1));
      
      fbxine.post_elements[fbxine.post_elements_num] = (post_element_t *) 
	xine_xmalloc(sizeof(post_element_t));
      fbxine.post_elements[fbxine.post_elements_num]->post = post;
      fbxine.post_elements[fbxine.post_elements_num]->name = strdup(plugin);
      fbxine.post_elements_num++;
      fbxine.post_elements[fbxine.post_elements_num] = NULL;
      
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
	    
	    if(p && strlen(p)) {
	      int param_num = 0;
	      
	      *p++ = '\0';
	      
	      while(pobj.properties_names[param_num]
		    && strcmp(pobj.properties_names[param_num], param))
		param_num++;
	      
	      if(pobj.properties_names[param_num]) {
		
		pobj.param    = pobj.descr->parameter;
		pobj.param    += param_num;
		pobj.readonly = pobj.param->readonly;

		switch(pobj.param->type) {
		case POST_PARAM_TYPE_INT:
		  if(!pobj.readonly) {
		    *(int *)(pobj.param_data + pobj.param->offset) = (int) strtol(p, &p, 10);
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
		      
		      snprintf((char *)(pobj.param_data + pobj.param->offset), maxlen, "%s", p);
		      _pplugin_update_parameter(&pobj);
		    }
		    else
		     fprintf(stderr, "parameter type POST_PARAM_TYPE_STRING) not supported yet.\n");
		  }
		  break;

		case POST_PARAM_TYPE_STRINGLIST: // unsupported */
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
  }
}


static void _pplugin_unwire(void) {
  xine_post_out_t  *vo_source;

  vo_source = xine_get_video_source(fbxine.stream);
  (void) xine_post_wire_video_port(vo_source, fbxine.video_port);
  /* Waiting a small chunk of time helps to avoid crashing */
  xine_usec_sleep(500000);
}

void pplugin_rewire_posts(void) {

  _pplugin_unwire();

  if(fbxine.post_elements_num) {

    if(1) {
      xine_post_out_t   *vo_source;
      int                i = 0;
      
      for(i = (fbxine.post_elements_num - 1); i >= 0; i--) {
	const char *const *outs = xine_post_list_outputs(fbxine.post_elements[i]->post);
	const xine_post_out_t *vo_out = xine_post_output(fbxine.post_elements[i]->post, (char *) *outs);
	if(i == (fbxine.post_elements_num - 1)) {
	  xine_post_wire_video_port((xine_post_out_t *) vo_out, fbxine.video_port);
	}
	else {
	  const xine_post_in_t *vo_in = xine_post_input(fbxine.post_elements[i + 1]->post, "video");
	  int                   err;
	  
	  err = xine_post_wire((xine_post_out_t *) vo_out, (xine_post_in_t *) vo_in);	
	}
      }
      
      vo_source = xine_get_video_source(fbxine.stream);
      xine_post_wire_video_port(vo_source, fbxine.post_elements[0]->post->video_input[0]);
    }
  }
}



/* end of post.c */
  
