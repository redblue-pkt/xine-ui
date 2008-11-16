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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"


#undef TRACE_REWIRE

#define DEFAULT_DEINTERLACER "tvtime:method=LinearBlend,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

#define WINDOW_WIDTH        530
#define WINDOW_HEIGHT       503

#define FRAME_WIDTH         (WINDOW_WIDTH - 50)
#define FRAME_HEIGHT        80

#define HELP_WINDOW_WIDTH   400
#define HELP_WINDOW_HEIGHT  402

#define MAX_DISPLAY_FILTERS 5

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
  xine_post_t                 *post;
  xine_post_api_t             *api;
  xine_post_api_descr_t       *descr;
  xine_post_api_parameter_t   *param;
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

typedef struct {
  xitk_window_t              *xwin;

  xitk_widget_list_t         *widget_list;

  xitk_widget_t              *slider;

  char                      **plugin_names;

  int                         first_displayed;
  post_object_t             **post_objects;
  int                         object_num;

  xitk_widget_t              *new_filter;
  xitk_widget_t              *enable;

  int                         x, y;
  
  int                         running;
  int                         visible;
  xitk_register_key_t         widget_key;

  /* help window stuff */
  xitk_window_t              *helpwin;
  xitk_widget_list_t         *help_widget_list;
  char                      **help_text;
  xitk_register_key_t         help_widget_key;
  int                         help_running;
  xitk_widget_t              *help_browser;
} _pplugin_t;

typedef struct {
  _pplugin_t                 *pplugin;
} _pp_wrapper_t;

static _pp_wrapper_t vpp_wrapper = { NULL };
static _pp_wrapper_t app_wrapper = { NULL };
static char        **post_audio_plugins;

/* Some functions prototype */
static void _vpplugin_unwire(void);
static void _applugin_unwire(void);
static void _pplugin_unwire(_pp_wrapper_t *pp_wrapper);
static post_element_t **pplugin_parse_and_load(_pp_wrapper_t *pp_wrapper, const char *, int *);
static void _vpplugin_rewire(void);
static void _applugin_rewire(void);
static void _pplugin_rewire(_pp_wrapper_t *pp_wrapper);
static void _pplugin_rewire_from_post_elements(_pp_wrapper_t *pp_wrapper, post_element_t **, int);
static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *);
static post_element_t **_pplugin_join_visualization_and_post_elements(int *);
static void _pplugin_save_chain(_pp_wrapper_t *pp_wrapper);
static void _vpplugin_close_help(xitk_widget_t *, void *);
static void _applugin_close_help(xitk_widget_t *, void *);
static void _pplugin_close_help(_pp_wrapper_t *pp_wrapper, xitk_widget_t *, void *);

static void post_deinterlace_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  post_element_t **posts = NULL;
  int              num, i;
  
  gGui->deinterlace_plugin = cfg->str_value;
  
  if(gGui->deinterlace_enable)
    _vpplugin_unwire();
  
  for(i = 0; i < gGui->deinterlace_elements_num; i++) {
    xine_post_dispose(__xineui_global_xine_instance, gGui->deinterlace_elements[i]->post);
    free(gGui->deinterlace_elements[i]->name);
    free(gGui->deinterlace_elements[i]);
  }
  
  SAFE_FREE(gGui->deinterlace_elements);
  gGui->deinterlace_elements_num = 0;
  
  if((posts = pplugin_parse_and_load(&vpp_wrapper, gGui->deinterlace_plugin, &num))) {
    gGui->deinterlace_elements     = posts;
    gGui->deinterlace_elements_num = num;
  }

  if(gGui->deinterlace_enable)
    _vpplugin_rewire();
}

static void post_audio_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui->visual_anim.post_plugin_num = cfg->num_value;  
  post_rewire_visual_anim();
}

const char **post_get_audio_plugins_names(void) {
  return (const char **) post_audio_plugins;
}

void post_init(void) {

  memset(&gGui->visual_anim.post_output_element, 0, sizeof (gGui->visual_anim.post_output_element));
  
  gGui->visual_anim.post_plugin_num = -1;
  gGui->visual_anim.post_changed = 0;

  if(gGui->ao_port) {
    const char *const *pol = xine_list_post_plugins_typed(__xineui_global_xine_instance, 
							  XINE_POST_TYPE_AUDIO_VISUALIZATION);
    
    if(pol) {
      int  num_plug = 0;
      
      while(pol[num_plug]) {
	
	post_audio_plugins = (char **) realloc(post_audio_plugins, sizeof(char *) * (num_plug + 2));
	
	post_audio_plugins[num_plug]     = strdup(pol[num_plug]);
	post_audio_plugins[num_plug + 1] = NULL;
	num_plug++;
      }
      
      if(num_plug) {
	
	gGui->visual_anim.post_plugin_num = 
	  xine_config_register_enum(__xineui_global_xine_instance, "gui.post_audio_plugin", 
				    0, post_audio_plugins,
				    _("Audio visualization plugin"),
				    _("Post audio plugin to used when playing streams without video"),
				    CONFIG_LEVEL_BEG,
				    post_audio_plugin_cb,
				    CONFIG_NO_DATA);
	
	gGui->visual_anim.post_output_element.post = 
	  xine_post_init(__xineui_global_xine_instance,
			 post_audio_plugins[gGui->visual_anim.post_plugin_num], 0,
			 &gGui->ao_port, &gGui->vo_port);
	
      }
    }
  }
}

void post_rewire_visual_anim(void) {

  if(gGui->visual_anim.post_output_element.post) {
    (void) post_rewire_audio_port_to_stream(gGui->stream);
    
    xine_post_dispose(__xineui_global_xine_instance, gGui->visual_anim.post_output_element.post);
  }
  
  gGui->visual_anim.post_output_element.post = 
    xine_post_init(__xineui_global_xine_instance,
		   post_audio_plugins[gGui->visual_anim.post_plugin_num], 0,
		   &gGui->ao_port, &gGui->vo_port);

  if(gGui->visual_anim.post_output_element.post && 
     (gGui->visual_anim.enabled == 1) && (gGui->visual_anim.running == 1)) {

    (void) post_rewire_audio_post_to_stream(gGui->stream);

  }
}

static int ignore_visual_anim = 1;

int post_rewire_audio_port_to_stream(xine_stream_t *stream) {

  _applugin_unwire();
  ignore_visual_anim = 1;
  _applugin_rewire();
/*  
  xine_post_out_t * audio_source;
  
  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gGui->ao_port);
*/
  return 1;
}

int post_rewire_audio_post_to_stream(xine_stream_t *stream) {

  _applugin_unwire();
  ignore_visual_anim = 0;
  _applugin_rewire();
/*
  xine_post_out_t * audio_source;

  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gGui->visual_anim.post_output->audio_input[0]);
*/
  return 1;
}

/* ================================================================ */

static void _vpplugin_unwire(void) {
  xine_post_out_t  *vo_source;
  
  vo_source = xine_get_video_source(gGui->stream);

  (void) xine_post_wire_video_port(vo_source, gGui->vo_port);
}

static void _applugin_unwire(void) {
  xine_post_out_t  *ao_source;
  
  ao_source = xine_get_audio_source(gGui->stream);

  (void) xine_post_wire_audio_port(ao_source, gGui->ao_port);
}

static void _pplugin_unwire(_pp_wrapper_t *pp_wrapper) {
  if (pp_wrapper == &vpp_wrapper)
    _vpplugin_unwire();
  else
    _applugin_unwire();
}

static void _vpplugin_rewire(void) {
  static post_element_t **post_elements;
  int post_elements_num;
  _pp_wrapper_t *pp_wrapper = &vpp_wrapper;
  
  _pplugin_save_chain(pp_wrapper);

  post_elements = _pplugin_join_deinterlace_and_post_elements(&post_elements_num);

  if( post_elements ) {
    _pplugin_rewire_from_post_elements(pp_wrapper, post_elements, post_elements_num);

    free(post_elements);
  }

#if 0
  if(pplugin && (gGui->post_enable && !gGui->deinterlace_enable)) {
    xine_post_out_t  *vo_source;
    int               post_num = pplugin->object_num;
    int               i = 0;
    
    if(!xitk_combo_get_current_selected(pplugin->post_objects[post_num - 1]->plugins))
      post_num--;
    
    if(post_num) {
      int paused = 0;
      
      if((paused = (xine_get_param(gGui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)))
	xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
      
      for(i = (post_num - 1); i >= 0; i--) {
	const char *const *outs = xine_post_list_outputs(pplugin->post_objects[i]->post);
	const xine_post_out_t *vo_out = xine_post_output(pplugin->post_objects[i]->post, (char *) *outs);
	if(i == (post_num - 1)) {
#ifdef TRACE_REWIRE
	  printf("  VIDEO_OUT .. OUT<-[PLUGIN %d]\n", i);
#endif
	  xine_post_wire_video_port((xine_post_out_t *) vo_out, gGui->vo_port);
	}
	else {
	  const xine_post_in_t *vo_in = xine_post_input(pplugin->post_objects[i+1]->post, "video");
	  int                   err;
	  
#ifdef TRACE_REWIRE
	  printf("  [PLUGIN %d]->IN ... OUT<-[PLUGIN %d]", i+1, i);
#endif
	  err = xine_post_wire((xine_post_out_t *) vo_out, (xine_post_in_t *) vo_in);	
#ifdef TRACE_REWIRE
	  printf(" (RESULT: %d)\n", err);
#endif
	}
      }
      
#ifdef TRACE_REWIRE
      printf("  [PLUGIN %d]->IN .. STREAM\n", 0);
#endif

      vo_source = xine_get_video_source(gGui->stream);
      xine_post_wire_video_port(vo_source, pplugin->post_objects[0]->post->video_input[0]);
      
      if(paused)
	xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);

    }
  }
#endif
}

static void _applugin_rewire(void) {
  static post_element_t **post_elements;
  int post_elements_num;
  _pp_wrapper_t *pp_wrapper = &app_wrapper;

  _pplugin_save_chain(pp_wrapper);

  post_elements = _pplugin_join_visualization_and_post_elements(&post_elements_num);
  
  if( post_elements ) {
    _pplugin_rewire_from_post_elements(pp_wrapper, post_elements, post_elements_num);

    free(post_elements);
  }
}

static void _pplugin_rewire(_pp_wrapper_t *pp_wrapper) {
  if (pp_wrapper == &vpp_wrapper)
    _vpplugin_rewire();
  else
    _applugin_rewire();
}

static int _pplugin_get_object_offset(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  int i;
  
  for(i = 0; i < pp_wrapper->pplugin->object_num; i++) {
    if(pp_wrapper->pplugin->post_objects[i] == pobj)
      return i;
  }
  
  return 0;
}

static int _pplugin_is_first_filter(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  if(pp_wrapper->pplugin)
    return (pobj == *pp_wrapper->pplugin->post_objects);
  
  return 0;
}

static int _pplugin_is_last_filter(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  
  if(pp_wrapper->pplugin) {
    post_object_t **po = pp_wrapper->pplugin->post_objects;
    
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

static void _pplugin_show_obj(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {

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
    
    if((!_pplugin_is_first_filter(pp_wrapper, pobj)) && (xitk_combo_get_current_selected(pobj->plugins)))
      ENABLE_ME(pobj->up);
    
    if((!_pplugin_is_last_filter(pp_wrapper, pobj) && (xitk_combo_get_current_selected(pobj->plugins))) && 
       (_pplugin_is_last_filter(pp_wrapper, pobj) 
	|| (!_pplugin_is_last_filter(pp_wrapper, pobj) 
	    && pp_wrapper->pplugin->post_objects[_pplugin_get_object_offset(pp_wrapper, pobj) + 1] 
	    && xitk_combo_get_current_selected(pp_wrapper->pplugin->post_objects[_pplugin_get_object_offset(pp_wrapper, pobj) + 1]->plugins))))
      ENABLE_ME(pobj->down);

  }
}

static void _pplugin_paint_widgets(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin) {
    int   i, x, y;
    int   last;
    int   slidmax, slidpos;
    
    last = pp_wrapper->pplugin->object_num <= (pp_wrapper->pplugin->first_displayed + MAX_DISPLAY_FILTERS)
      ? pp_wrapper->pplugin->object_num : (pp_wrapper->pplugin->first_displayed + MAX_DISPLAY_FILTERS);
    
    for(i = 0; i < pp_wrapper->pplugin->object_num; i++)
      _pplugin_hide_obj(pp_wrapper->pplugin->post_objects[i]);
    
    x = 15;
    y = 34 - (FRAME_HEIGHT + 4);
    
    for(i = pp_wrapper->pplugin->first_displayed; i < last; i++) {
      y += FRAME_HEIGHT + 4;
      pp_wrapper->pplugin->post_objects[i]->x = x;
      pp_wrapper->pplugin->post_objects[i]->y = y;
      _pplugin_show_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);
    }
    
    if(pp_wrapper->pplugin->object_num > MAX_DISPLAY_FILTERS) {
      slidmax = pp_wrapper->pplugin->object_num - MAX_DISPLAY_FILTERS;
      slidpos = slidmax - pp_wrapper->pplugin->first_displayed;
      xitk_enable_and_show_widget(pp_wrapper->pplugin->slider);
    }
    else {
      slidmax = 1;
      slidpos = slidmax;
      
      if(!pp_wrapper->pplugin->first_displayed)
	xitk_disable_and_hide_widget(pp_wrapper->pplugin->slider);
    }
    
    xitk_slider_set_max(pp_wrapper->pplugin->slider, slidmax);
    xitk_slider_set_pos(pp_wrapper->pplugin->slider, slidpos);
  }
}

static void _pplugin_send_expose(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin) {
    XLockDisplay(gGui->display);
    XClearWindow(gGui->display, (XITK_WIDGET_LIST_WINDOW(pp_wrapper->pplugin->widget_list)));
    XUnlockDisplay(gGui->display);
  }
}

static void _pplugin_destroy_widget(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w) {
  xitk_widget_t *cw;

  cw = (xitk_widget_t *) xitk_list_first_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)));
  while(cw) {
    
    if(cw == w) {
      xitk_hide_widget(cw);
      xitk_destroy_widget(cw);
      cw = NULL;
      xitk_list_delete_current((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)));
      return;
    }
    
    cw = (xitk_widget_t *) xitk_list_next_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)));
  }
}

static void _pplugin_update_parameter(post_object_t *pobj) {
  pobj->api->set_parameters(pobj->post, pobj->param_data);
  pobj->api->get_parameters(pobj->post, pobj->param_data);
}

static void _pplugin_set_param_int(xitk_widget_t *w, void *data, int value) {
  post_object_t *pobj = (post_object_t *) data;
  
  if(pobj->readonly)
    return;
  
  //can be int[]:
  //int num_of_int = pobj->param->size / sizeof(char);
  
  if(pobj->param->range_min && pobj->param->range_max && 
     (value < (int)pobj->param->range_min || value > (int)pobj->param->range_max)) {
    xine_error(_("Entered value is out of bounds (%d>%d<%d)."),
	       (int)pobj->param->range_min, value, (int)pobj->param->range_max);
  }
  else {
    *(int *)(pobj->param_data + pobj->param->offset) = value;
    
    _pplugin_update_parameter(pobj);
    
    if(xitk_get_widget_type(w) & WIDGET_GROUP_COMBO)
      xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
    else
      xitk_intbox_set_value(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
  }
}

static void _pplugin_set_param_double(xitk_widget_t *w, void *data, double value) {
  post_object_t *pobj = (post_object_t *) data;
  
  if(pobj->readonly)
    return;

  if(pobj->param->range_min && pobj->param->range_max && 
     (value < pobj->param->range_min || value > pobj->param->range_max)) {
    xine_error(_("Entered value is out of bounds (%e>%e<%e)."),
	       pobj->param->range_min, value, pobj->param->range_max);
  }
  else {
    *(double *)(pobj->param_data + pobj->param->offset) = value;
    
    _pplugin_update_parameter(pobj);
    
    xitk_doublebox_set_value(pobj->value, *(double *)(pobj->param_data + pobj->param->offset));
  }
}

static void _pplugin_set_param_char(xitk_widget_t *w, void *data, char *text) {
  post_object_t *pobj = (post_object_t *) data;

  if(pobj->readonly)
    return;
  
  // SUPPORT CHAR but no STRING yet
  if(pobj->param->type == POST_PARAM_TYPE_CHAR) {
    int maxlen = pobj->param->size / sizeof(char);
    
    strlcpy((char *)(pobj->param_data + pobj->param->offset), text, maxlen);
    _pplugin_update_parameter(pobj);
    xitk_inputtext_change_text(pobj->value, (char *)(pobj->param_data + pobj->param->offset));
  }
  else
    xine_error(_("parameter type POST_PARAM_TYPE_STRING not supported yet.\n"));

}

static void _pplugin_set_param_stringlist(xitk_widget_t *w, void *data, int value) {
  post_object_t *pobj = (post_object_t *) data;
  
  if(pobj->readonly)
    return;
  
  xine_error(_("parameter type POST_PARAM_TYPE_STRINGLIST not supported yet.\n"));
}

static void _pplugin_set_param_bool(xitk_widget_t *w, void *data, int state) {
  post_object_t *pobj = (post_object_t *) data;
  
  if(pobj->readonly)
    return;
  
  *(int *)(pobj->param_data + pobj->param->offset) = state;
  _pplugin_update_parameter(pobj);
  xitk_checkbox_set_state(pobj->value, 
			  (*(int *)(pobj->param_data + pobj->param->offset)));  
}

static void _pplugin_add_parameter_widget(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  
  if(pobj) {
    xitk_label_widget_t   lb;
    char                  buffer[2048];

    snprintf(buffer, sizeof(buffer), "%s:", (pobj->param->description) 
	     ? pobj->param->description : _("No description available"));

    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.window              = xitk_window_get_window(pp_wrapper->pplugin->xwin);
    lb.gc                  = (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->widget_list));
    lb.skin_element_name   = NULL;
    lb.label               = buffer;
    lb.callback            = NULL;
    lb.userdata            = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
     (pobj->comment = xitk_noskin_label_create(pp_wrapper->pplugin->widget_list, &lb,
					       0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname)));

    switch(pobj->param->type) {
    case POST_PARAM_TYPE_INT:
      {
	if(pobj->param->enum_values) {
	  xitk_combo_widget_t         cmb;

	  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
	  cmb.skin_element_name = NULL;
	  cmb.layer_above       = (is_layer_above());
	  cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
	  cmb.entries           = (const char **)pobj->param->enum_values;
	  cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
	  cmb.callback          = _pplugin_set_param_int;
	  cmb.userdata          = pobj;
	  xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
	   (pobj->value = xitk_noskin_combo_create(pp_wrapper->pplugin->widget_list, &cmb,
						   0, 0, 365, NULL, NULL)));

	  xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
	}
	else {
	  xitk_intbox_widget_t      ib;
	  
	  XITK_WIDGET_INIT(&ib, gGui->imlib_data);
	  ib.skin_element_name = NULL;
	  ib.value             = *(int *)(pobj->param_data + pobj->param->offset);
	  ib.step              = 1;
	  ib.parent_wlist      = pp_wrapper->pplugin->widget_list;
	  ib.callback          = _pplugin_set_param_int;
	  ib.userdata          = pobj;
	  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
	   (pobj->value = xitk_noskin_intbox_create(pp_wrapper->pplugin->widget_list, &ib,
						    0, 0, 50, 20, NULL, NULL, NULL)));
	}
      }
      break;
      
    case POST_PARAM_TYPE_DOUBLE:
      {
	xitk_doublebox_widget_t      ib;
	
	XITK_WIDGET_INIT(&ib, gGui->imlib_data);
	ib.skin_element_name = NULL;
	ib.value             = *(double *)(pobj->param_data + pobj->param->offset);
	ib.step              = .5;
	ib.parent_wlist      = pp_wrapper->pplugin->widget_list;
	ib.callback          = _pplugin_set_param_double;
	ib.userdata          = pobj;
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
	 (pobj->value = xitk_noskin_doublebox_create(pp_wrapper->pplugin->widget_list, &ib,
						     0, 0, 100, 20, NULL, NULL, NULL)));
      }
      break;
      
    case POST_PARAM_TYPE_CHAR:
    case POST_PARAM_TYPE_STRING:
      {
	xitk_inputtext_widget_t  inp;
	
	XITK_WIDGET_INIT(&inp, gGui->imlib_data);
	inp.skin_element_name = NULL;
	inp.text              = (char *)(pobj->param_data + pobj->param->offset);
	inp.max_length        = 256;
	inp.callback          = _pplugin_set_param_char;
	inp.userdata          = pobj;
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
	 (pobj->value = xitk_noskin_inputtext_create(pp_wrapper->pplugin->widget_list, &inp,
						     0, 0, 365, 20,
						     "Black", "Black", fontname)));
      }
      break;
      
    case POST_PARAM_TYPE_STRINGLIST:
      {
	xitk_combo_widget_t         cmb;
	
	XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
	cmb.skin_element_name = NULL;
	cmb.layer_above       = (is_layer_above());
	cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
	cmb.entries           = (const char **)(pobj->param_data + pobj->param->offset);
	cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
	cmb.callback          = _pplugin_set_param_stringlist;
	cmb.userdata          = pobj;
	xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
	 (pobj->value = xitk_noskin_combo_create(pp_wrapper->pplugin->widget_list, &cmb,
						 0, 0, 365, NULL, NULL)));
	
	xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
      }
      break;
      
    case POST_PARAM_TYPE_BOOL:
      {
	xitk_checkbox_widget_t    cb;
	
	XITK_WIDGET_INIT(&cb, gGui->imlib_data);
	cb.skin_element_name = NULL;
	cb.callback          = _pplugin_set_param_bool;
	cb.userdata          = pobj;
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
	  (pobj->value = xitk_noskin_checkbox_create(pp_wrapper->pplugin->widget_list, &cb,
						     0, 0, 12, 12)));
	xitk_checkbox_set_state(pobj->value, 
				(*(int *)(pobj->param_data + pobj->param->offset)));  
      }
      break;
    }
  }
}

static void _pplugin_change_parameter(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;

  if(pobj) {

    if(pobj->value)
      _pplugin_destroy_widget(pp_wrapper, pobj->value);
    
    if(pobj->comment)
      _pplugin_destroy_widget(pp_wrapper, pobj->comment);

    if(pobj->descr) {
      pobj->param    = pobj->descr->parameter;
      pobj->param    += select;
      pobj->readonly = pobj->param->readonly;
      _pplugin_add_parameter_widget(pp_wrapper, pobj);
    }

    _pplugin_paint_widgets(pp_wrapper);
  }
}

static void _vpplugin_change_parameter(xitk_widget_t *w, void *data, int select) {
  return _pplugin_change_parameter(&vpp_wrapper, w, data, select);
}

static void _applugin_change_parameter(xitk_widget_t *w, void *data, int select) {
  return _pplugin_change_parameter(&app_wrapper, w, data, select);
}

static void _pplugin_get_plugins(_pp_wrapper_t *pp_wrapper) {
  int plugin_type = (pp_wrapper == &vpp_wrapper) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  const char *const *pol = xine_list_post_plugins_typed(__xineui_global_xine_instance, plugin_type);
  
  if(pol) {
    int  i = 0;

    pp_wrapper->pplugin->plugin_names    = (char **) malloc(sizeof(char *) * 2);
    pp_wrapper->pplugin->plugin_names[i] = strdup(_("No Filter"));
    while(pol[i]) {
      pp_wrapper->pplugin->plugin_names = (char **) realloc(pp_wrapper->pplugin->plugin_names, sizeof(char *) * (i + 1 + 2));
      
      pp_wrapper->pplugin->plugin_names[i + 1] = strdup(pol[i]);
      i++;
    }
    pp_wrapper->pplugin->plugin_names[i + 1] = NULL;
  }
}

static void _pplugin_destroy_only_obj(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  if(pobj) {

    if(pobj->properties) {
      
      _pplugin_destroy_widget(pp_wrapper, pobj->properties);
      pobj->properties = NULL;
      
      free(pobj->param_data);
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
    
    if(pobj->comment) {
      _pplugin_destroy_widget(pp_wrapper, pobj->comment);
      pobj->comment = NULL;
    }
    
    if(pobj->value) {
      _pplugin_destroy_widget(pp_wrapper, pobj->value);
      pobj->value = NULL;
    }

    if(pobj->api && pobj->help) {
      if(pp_wrapper->pplugin->help_running)
	_pplugin_close_help(pp_wrapper, NULL,NULL);
      _pplugin_destroy_widget(pp_wrapper, pobj->help);
      pobj->help = NULL;
    }
    
  }
}

static void _pplugin_destroy_obj(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  if(pobj) {
    _pplugin_destroy_only_obj(pp_wrapper, pobj);
    
    if(pobj->post) {
      xine_post_dispose(__xineui_global_xine_instance, pobj->post);
      pobj->post = NULL;
    }
  }
}

static void _pplugin_destroy_base_obj(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  int i = 0;

  while(pp_wrapper->pplugin->post_objects && (pp_wrapper->pplugin->post_objects[i] != pobj)) 
    i++;
  
  if(pp_wrapper->pplugin->post_objects[i]) {
    _pplugin_destroy_widget(pp_wrapper, pp_wrapper->pplugin->post_objects[i]->plugins);
    _pplugin_destroy_widget(pp_wrapper, pp_wrapper->pplugin->post_objects[i]->frame);
    _pplugin_destroy_widget(pp_wrapper, pp_wrapper->pplugin->post_objects[i]->up);
    _pplugin_destroy_widget(pp_wrapper, pp_wrapper->pplugin->post_objects[i]->down);
    
    free(pp_wrapper->pplugin->post_objects[i]);
    pp_wrapper->pplugin->post_objects[i] = NULL;
    
    pp_wrapper->pplugin->object_num--;
    
    pp_wrapper->pplugin->x = 15;
    pp_wrapper->pplugin->y -= FRAME_HEIGHT + 4;
  }
}

static void _pplugin_kill_filters_from(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  int             num = pp_wrapper->pplugin->object_num;
  int             i = 0;
  
  while(pp_wrapper->pplugin->post_objects && (pp_wrapper->pplugin->post_objects[i] != pobj)) 
    i++;
  
  _pplugin_destroy_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);
  
  i++;
  
  if(pp_wrapper->pplugin->post_objects[i]) {

    while(pp_wrapper->pplugin->post_objects[i]) {

      _pplugin_destroy_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);
      
      _pplugin_destroy_base_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);

      i++;
    }
  }

  _pplugin_send_expose(pp_wrapper);

  if(num != pp_wrapper->pplugin->object_num) {
    pp_wrapper->pplugin->post_objects = (post_object_t **) realloc(pp_wrapper->pplugin->post_objects, sizeof(post_object_t *) * (pp_wrapper->pplugin->object_num + 2));
    pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num + 1] = NULL;
  }
}

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

static void _pplugin_close_help(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {

  pp_wrapper->pplugin->help_running = 0;
  
  xitk_unregister_event_handler(&pp_wrapper->pplugin->help_widget_key);
  
  xitk_destroy_widgets(pp_wrapper->pplugin->help_widget_list);
  xitk_window_destroy_window(gGui->imlib_data, pp_wrapper->pplugin->helpwin);
  
  pp_wrapper->pplugin->helpwin = NULL;
  xitk_list_free((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->help_widget_list)));
    
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->help_widget_list)));
  XUnlockDisplay(gGui->display);
    
  free(pp_wrapper->pplugin->help_widget_list);
}

static void _vpplugin_close_help(xitk_widget_t *w, void *data) {
  _pplugin_close_help(&vpp_wrapper, w, data);
}

static void _applugin_close_help(xitk_widget_t *w, void *data) {
  _pplugin_close_help(&app_wrapper, w, data);
}

static void _pplugin_help_handle_event(_pp_wrapper_t *pp_wrapper, XEvent *event, void *data) {

  if(event->type == KeyPress) {
    if(xitk_get_key_pressed(event) == XK_Escape)
      _pplugin_close_help(pp_wrapper, NULL, NULL);
    else
      gui_handle_event(event, data);
  }
}

static void _vpplugin_help_handle_event(XEvent *event, void *data) {
  _pplugin_help_handle_event(&vpp_wrapper, event, data);
}

static void _applugin_help_handle_event(XEvent *event, void *data) {
  _pplugin_help_handle_event(&app_wrapper, event, data);
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

static void _pplugin_show_help(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  GC                          gc;
  xitk_pixmap_t              *bg = NULL;
  int                         x, y, width, height;
  xitk_labelbutton_widget_t   lb;
  xitk_browser_widget_t       br;

  if(!pobj->api) {
    if(pp_wrapper->pplugin->help_running)
      _pplugin_close_help(pp_wrapper, NULL, NULL);
    return;
  }
  
  /* create help window if needed */

  if( !pp_wrapper->pplugin->help_running ) {
    x = y = 80;
    pp_wrapper->pplugin->helpwin = xitk_window_create_dialog_window(gGui->imlib_data, 
                                                                    _("Plugin Help"), x, y,
                                                                    HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT);

    set_window_states_start((xitk_window_get_window(pp_wrapper->pplugin->helpwin)));

    XLockDisplay (gGui->display);
    gc = XCreateGC(gGui->display, 
  		 (xitk_window_get_window(pp_wrapper->pplugin->helpwin)), None, None);
    XUnlockDisplay (gGui->display);
    
    pp_wrapper->pplugin->help_widget_list = xitk_widget_list_new();
    xitk_widget_list_set(pp_wrapper->pplugin->help_widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
    xitk_widget_list_set(pp_wrapper->pplugin->help_widget_list, 
                         WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(pp_wrapper->pplugin->helpwin)));
    xitk_widget_list_set(pp_wrapper->pplugin->help_widget_list, WIDGET_LIST_GC, gc);
    
    xitk_window_get_window_size(pp_wrapper->pplugin->helpwin, &width, &height);
    bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);
  
    XLockDisplay (gGui->display);
    XCopyArea(gGui->display, (xitk_window_get_background(pp_wrapper->pplugin->helpwin)), bg->pixmap,
  	    bg->gc, 0, 0, width, height, 0, 0);
    XUnlockDisplay (gGui->display);
  
    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Close");
    lb.align             = ALIGN_CENTER;
    lb.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_close_help : _applugin_close_help;
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->help_widget_list)), 
     (w = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->help_widget_list, 
				       &lb, HELP_WINDOW_WIDTH - (100 + 15), HELP_WINDOW_HEIGHT - (23 + 15), 100, 23,
  				       "Black", "Black", "White", btnfontname)));
    xitk_enable_and_show_widget(w);
  
  
    XITK_WIDGET_INIT(&br, gGui->imlib_data);
    br.arrow_up.skin_element_name    = NULL;
    br.slider.skin_element_name      = NULL;
    br.arrow_dn.skin_element_name    = NULL;
    br.browser.skin_element_name     = NULL;
    br.browser.max_displayed_entries = MAX_DISP_ENTRIES;
    br.browser.num_entries           = 0;
    br.browser.entries               = NULL;
    br.callback                      = NULL;
    br.dbl_click_callback            = NULL;
    br.parent_wlist                  = pp_wrapper->pplugin->help_widget_list;
    br.userdata                      = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->help_widget_list)), 
     (pp_wrapper->pplugin->help_browser = 
      xitk_noskin_browser_create(pp_wrapper->pplugin->help_widget_list, &br,
			       (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->help_widget_list)), 15, 34,
  			       HELP_WINDOW_WIDTH - (30 + 16), 20,
  			       16, br_fontname)));
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
      char **ohbuf = pp_wrapper->pplugin->help_text;
      int    i = 0;

      pp_wrapper->pplugin->help_text = hbuf;
      xitk_browser_update_list(pp_wrapper->pplugin->help_browser, (const char **)pp_wrapper->pplugin->help_text,
                               NULL, lines, 0);
      
      if(ohbuf) {
	while(ohbuf[i++])
	  free(ohbuf[i]);
	free(ohbuf);
      }
    }
  }

  if( !pp_wrapper->pplugin->help_running ) {

    xitk_enable_and_show_widget(pp_wrapper->pplugin->help_browser);
    xitk_browser_set_alignment(pp_wrapper->pplugin->help_browser, ALIGN_LEFT);
  
  
    xitk_window_change_background(gGui->imlib_data, pp_wrapper->pplugin->helpwin, bg->pixmap, width, height);
    xitk_image_destroy_xitk_pixmap(bg);
  
    pp_wrapper->pplugin->help_widget_key = xitk_register_event_handler((pp_wrapper == &vpp_wrapper) ? "vpplugin_help" : "applugin_help", 
                                                                       (xitk_window_get_window(pp_wrapper->pplugin->helpwin)),
								       (pp_wrapper == &vpp_wrapper) ? _vpplugin_help_handle_event : _applugin_help_handle_event,
                                                                       NULL,
                                                                       NULL,
                                                                       pp_wrapper->pplugin->help_widget_list,
                                                                       NULL);
  
    pp_wrapper->pplugin->help_running = 1;
  }

  raise_window(xitk_window_get_window(pp_wrapper->pplugin->helpwin), 1, pp_wrapper->pplugin->help_running);
  
  try_to_set_input_focus(xitk_window_get_window(pp_wrapper->pplugin->helpwin));
}

static void _vpplugin_show_help(xitk_widget_t *w, void *data) {
  _pplugin_show_help(&vpp_wrapper, w, data);
}

static void _applugin_show_help(xitk_widget_t *w, void *data) {
  _pplugin_show_help(&app_wrapper, w, data);
}

static void _pplugin_retrieve_parameters(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  xitk_combo_widget_t         cmb;
  xitk_labelbutton_widget_t   lb;
  
  if(__pplugin_retrieve_parameters(pobj)) {
    
    XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
    cmb.skin_element_name = NULL;
    cmb.layer_above       = (is_layer_above());
    cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
    cmb.entries           = (const char **)pobj->properties_names;
    cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
    cmb.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_change_parameter : _applugin_change_parameter;
    cmb.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
     (pobj->properties = xitk_noskin_combo_create(pp_wrapper->pplugin->widget_list, 
						  &cmb, 
						  0, 0, 175, NULL, NULL)));
    
    xitk_combo_set_select(pobj->properties, 0);
    xitk_combo_callback_exec(pobj->properties);

    if(pobj->api && pobj->api->get_help) {

      XITK_WIDGET_INIT(&lb, gGui->imlib_data);
      lb.button_type       = CLICK_BUTTON;
      lb.label             = _("Help");
      lb.align             = ALIGN_CENTER;
      lb.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_show_help : _applugin_show_help; 
      lb.state_callback    = NULL;
      lb.userdata          = pobj;
      lb.skin_element_name = NULL;
      xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
			       (pobj->help = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->widget_list, 
									    &lb, 0, 0, 63, 20,
									    "Black", "Black", "White", btnfontname)));
    }
  }
  else {
    xitk_label_widget_t   lb;
    
    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.window              = xitk_window_get_window(pp_wrapper->pplugin->xwin);
    lb.gc                  = (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->widget_list));
    lb.skin_element_name   = NULL;
    lb.label               = _("There is no parameter available for this plugin.");
    lb.callback            = NULL;
    lb.userdata            = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
     (pobj->comment = xitk_noskin_label_create(pp_wrapper->pplugin->widget_list, &lb,
					       0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname)));
    
    pobj->properties = NULL;
  }
}

static void _pplugin_select_filter(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;
  
  _pplugin_unwire(pp_wrapper);

  if(!select) {
    _pplugin_kill_filters_from(pp_wrapper, pobj);
    
    if(_pplugin_is_first_filter(pp_wrapper, pobj)) {
      if(xitk_is_widget_enabled(pp_wrapper->pplugin->new_filter))
	xitk_disable_widget(pp_wrapper->pplugin->new_filter);
    }
    else {
      _pplugin_destroy_base_obj(pp_wrapper, pobj);
      
      pp_wrapper->pplugin->post_objects = (post_object_t **) realloc(pp_wrapper->pplugin->post_objects, sizeof(post_object_t *) * (pp_wrapper->pplugin->object_num + 1));
      pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num] = NULL;
      
      if(!xitk_is_widget_enabled(pp_wrapper->pplugin->new_filter))
	xitk_enable_widget(pp_wrapper->pplugin->new_filter);
     
    }

    if(pp_wrapper->pplugin->object_num <= MAX_DISPLAY_FILTERS)
      pp_wrapper->pplugin->first_displayed = 0;
    else if((pp_wrapper->pplugin->object_num - pp_wrapper->pplugin->first_displayed) < MAX_DISPLAY_FILTERS)
      pp_wrapper->pplugin->first_displayed = pp_wrapper->pplugin->object_num - MAX_DISPLAY_FILTERS;

  }
  else {
    _pplugin_destroy_obj(pp_wrapper, pobj);

    pobj->post = xine_post_init(__xineui_global_xine_instance, 
				xitk_combo_get_current_entry_selected(w),
				0, &gGui->ao_port, &gGui->vo_port);
    
    _pplugin_retrieve_parameters(pp_wrapper, pobj);

    if(!xitk_is_widget_enabled(pp_wrapper->pplugin->new_filter))
      xitk_enable_widget(pp_wrapper->pplugin->new_filter);

  }
  
  if(pp_wrapper->pplugin->help_running)
    _pplugin_show_help(pp_wrapper, NULL, pobj);

  _pplugin_rewire(pp_wrapper);
  _pplugin_paint_widgets(pp_wrapper);
}

static void _vpplugin_select_filter(xitk_widget_t *w, void *data, int select) {
  _pplugin_select_filter(&vpp_wrapper, w, data, select);
}

static void _applugin_select_filter(xitk_widget_t *w, void *data, int select) {
  _pplugin_select_filter(&app_wrapper, w, data, select);
}

static void _pplugin_move_up(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_object_t **ppobj = pp_wrapper->pplugin->post_objects;

  _pplugin_unwire(pp_wrapper);

  while(*ppobj != pobj)
    ppobj++;
  
  *ppobj       = *(ppobj - 1);
  *(ppobj - 1) = pobj;
  
  _pplugin_rewire(pp_wrapper);
  _pplugin_paint_widgets(pp_wrapper);
}

static void _vpplugin_move_up(xitk_widget_t *w, void *data) {
  _pplugin_move_up(&vpp_wrapper, w, data);
}

static void _applugin_move_up(xitk_widget_t *w, void *data) {
  _pplugin_move_up(&app_wrapper, w, data);
}

static void _pplugin_move_down(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_object_t **ppobj = pp_wrapper->pplugin->post_objects;

  _pplugin_unwire(pp_wrapper);

  while(*ppobj != pobj)
    ppobj++;
  
  *ppobj       = *(ppobj + 1);
  *(ppobj + 1) = pobj;
  
  _pplugin_rewire(pp_wrapper);
  _pplugin_paint_widgets(pp_wrapper);
}

static void _vpplugin_move_down(xitk_widget_t *w, void *data) {
  _pplugin_move_down(&vpp_wrapper, w, data);
}

static void _applugin_move_down(xitk_widget_t *w, void *data) {
  _pplugin_move_down(&app_wrapper, w, data);
}

static void _pplugin_create_filter_object(_pp_wrapper_t *pp_wrapper) {
  xitk_combo_widget_t   cmb;
  post_object_t        *pobj;
  xitk_image_widget_t   im;
  xitk_image_t         *image;
  xitk_button_widget_t  b;
  
  pp_wrapper->pplugin->x = 15;
  pp_wrapper->pplugin->y += FRAME_HEIGHT + 4;
  
  xitk_disable_widget(pp_wrapper->pplugin->new_filter);

  pp_wrapper->pplugin->post_objects = (post_object_t **) realloc(pp_wrapper->pplugin->post_objects, sizeof(post_object_t *) * (pp_wrapper->pplugin->object_num + 2));
  pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num + 1] = NULL;
  
  pobj = pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num] = (post_object_t *) calloc(1, sizeof(post_object_t));
  pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num]->x = pp_wrapper->pplugin->x;
  pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num]->y = pp_wrapper->pplugin->y;
  pp_wrapper->pplugin->object_num++;

  image = xitk_image_create_image(gGui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);

  XLockDisplay(gGui->display);
  XSetForeground(gGui->display, (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->widget_list)),
		 xitk_get_pixel_color_gray(gGui->imlib_data));
  XFillRectangle(gGui->display, image->image->pixmap,
		 (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->widget_list)),
		 0, 0, image->width, image->height);
  XUnlockDisplay(gGui->display);

  /* Some decorations */
  draw_outter_frame(gGui->imlib_data, image->image, NULL, NULL,
		    0, 0, FRAME_WIDTH, FRAME_HEIGHT);
  draw_rectangular_outter_box_light(gGui->imlib_data, image->image,
		    27, 28, FRAME_WIDTH - 27 - 5, 1);
  draw_rectangular_outter_box_light(gGui->imlib_data, image->image,
		    26, 5, 1, FRAME_HEIGHT - 10);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, NULL,
		    5, 5, 16, 16);
  draw_rectangular_inner_box_light(gGui->imlib_data, image->image,
		    5, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(gGui->imlib_data, image->image,
		    10, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(gGui->imlib_data, image->image,
		    15, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(gGui->imlib_data, image->image,
		    20, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, NULL,
		    5, FRAME_HEIGHT - 16 - 5, 16, 16);

  XITK_WIDGET_INIT(&im, gGui->imlib_data);
  im.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
   (pobj->frame = xitk_noskin_image_create(pp_wrapper->pplugin->widget_list, &im, image, pobj->x - 5, pobj->y - 5)));
  DISABLE_ME(pobj->frame);
  
  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
  cmb.entries           = (const char **)pp_wrapper->pplugin->plugin_names;
  cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
  cmb.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_select_filter : _applugin_select_filter;
  cmb.userdata          = pobj;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
   (pobj->plugins = xitk_noskin_combo_create(pp_wrapper->pplugin->widget_list, 
					     &cmb, 0, 0, 175, NULL, NULL)));
  DISABLE_ME(pobj->plugins);
  
  xitk_combo_set_select(pobj->plugins, 0);

  {
    xitk_image_t *bimage;

    XITK_WIDGET_INIT(&b, gGui->imlib_data);
    b.skin_element_name = NULL;
    b.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_move_up : _applugin_move_up;
    b.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
     (pobj->up = xitk_noskin_button_create(pp_wrapper->pplugin->widget_list, &b, 
					   0, 0, 17, 17)));
    DISABLE_ME(pobj->up);

    if((bimage = xitk_get_widget_foreground_skin(pobj->up)) != NULL)
      draw_arrow_up(gGui->imlib_data, bimage);
    
    b.skin_element_name = NULL;
    b.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_move_down : _applugin_move_down;
    b.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list), 
     (pobj->down = xitk_noskin_button_create(pp_wrapper->pplugin->widget_list, &b, 
					     0, 0, 17, 17)));
    DISABLE_ME(pobj->down);

    if((bimage = xitk_get_widget_foreground_skin(pobj->down)) != NULL)
      draw_arrow_down(gGui->imlib_data, bimage);

  }
}

static void _pplugin_add_filter(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  _pplugin_create_filter_object(pp_wrapper);

  if(pp_wrapper->pplugin->object_num > MAX_DISPLAY_FILTERS)
    pp_wrapper->pplugin->first_displayed = (pp_wrapper->pplugin->object_num - MAX_DISPLAY_FILTERS);

  _pplugin_paint_widgets(pp_wrapper);
}

static void _vpplugin_add_filter(xitk_widget_t *w, void *data) {
  _pplugin_add_filter(&vpp_wrapper, w, data);
}

static void _applugin_add_filter(xitk_widget_t *w, void *data) {
  _pplugin_add_filter(&app_wrapper, w, data);
}

static int _pplugin_rebuild_filters(_pp_wrapper_t *pp_wrapper) {
  post_element_t  **pelem = (pp_wrapper == &vpp_wrapper) ? gGui->post_video_elements : gGui->post_audio_elements;
  int plugin_type = (pp_wrapper == &vpp_wrapper) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  int num_filters = 0;
  
  while(pelem && *pelem) {
    int i = 0;

    if ((*pelem)->post->type == plugin_type) {
      num_filters++;
      
      _pplugin_create_filter_object(pp_wrapper);
      pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num - 1]->post = (*pelem)->post;
      
      while(pp_wrapper->pplugin->plugin_names[i] && strcasecmp(pp_wrapper->pplugin->plugin_names[i], (*pelem)->name))
        i++;
      
      if(pp_wrapper->pplugin->plugin_names[i]) {
        xitk_combo_set_select(pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num - 1]->plugins, i);
        
        _pplugin_retrieve_parameters(pp_wrapper, pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num - 1]);
      }
    }
    
    pelem++;
  }
  
  xitk_enable_widget(pp_wrapper->pplugin->new_filter);

  _pplugin_paint_widgets(pp_wrapper);

  return num_filters;
}

static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num) {
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( gGui->post_video_enable )
    (*post_elements_num) += gGui->post_video_elements_num;
  if( gGui->deinterlace_enable )
    (*post_elements_num) += gGui->deinterlace_elements_num;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **) 
    calloc((*post_elements_num), sizeof(post_element_t *));

  for( i = 0; gGui->deinterlace_enable && i < gGui->deinterlace_elements_num; i++ ) {
    post_elements[i+j] = gGui->deinterlace_elements[i];
  }

  for( j = 0; gGui->post_video_enable && j < gGui->post_video_elements_num; j++ ) {
    post_elements[i+j] = gGui->post_video_elements[j];
  }
  
  return post_elements;
}


static post_element_t **_pplugin_join_visualization_and_post_elements(int *post_elements_num) {
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( gGui->post_audio_enable )
    (*post_elements_num) += gGui->post_audio_elements_num;
  if( !ignore_visual_anim )
    (*post_elements_num)++;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **) 
    calloc((*post_elements_num), sizeof(post_element_t *));
  
  for( j = 0; gGui->post_audio_enable && j < gGui->post_audio_elements_num; j++ ) {
    post_elements[i+j] = gGui->post_audio_elements[j];
  }

  for( i = 0; !ignore_visual_anim && i < 1; i++ ) {
    post_elements[i+j] = &gGui->visual_anim.post_output_element;
  }

  return post_elements;
}


static void _pplugin_save_chain(_pp_wrapper_t *pp_wrapper) {
  
  if(pp_wrapper->pplugin) {
    post_element_t ***_post_elements = (pp_wrapper == &vpp_wrapper) ? &gGui->post_video_elements : &gGui->post_audio_elements;
    int *_post_elements_num = (pp_wrapper == &vpp_wrapper) ? &gGui->post_video_elements_num : &gGui->post_audio_elements_num;
    int i = 0;
    int post_num = pp_wrapper->pplugin->object_num;
    
    if(!xitk_combo_get_current_selected(pp_wrapper->pplugin->post_objects[post_num - 1]->plugins))
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
	  free((*_post_elements)[j]);
	}
	
	*_post_elements = (post_element_t **) realloc(*_post_elements, sizeof(post_element_t *) * (post_num + 1));
	
      }
	
      for(i = 0; i < post_num; i++) {
	(*_post_elements)[i] = (post_element_t *) calloc(1, sizeof(post_element_t));
	(*_post_elements)[i]->post = pp_wrapper->pplugin->post_objects[i]->post;
	(*_post_elements)[i]->name = 
	  strdup(xitk_combo_get_current_entry_selected(pp_wrapper->pplugin->post_objects[i]->plugins));
      }

      (*_post_elements)[post_num] = NULL;
      *_post_elements_num         = post_num;
    }
    else {
      if(*_post_elements_num) {
	int j;
	
	for(j = 0; j < *_post_elements_num; j++) {
	  free((*_post_elements)[j]->name);
	  free((*_post_elements)[j]);
	}
	
	free((*_post_elements)[j]);
	free(*_post_elements);

	*_post_elements_num = 0;
	*_post_elements     = NULL;
      }
    }
  }
}

static void _pplugin_nextprev(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int pos) {
  int rpos = (xitk_slider_get_max(pp_wrapper->pplugin->slider)) - pos;

  if(rpos != pp_wrapper->pplugin->first_displayed) {
    pp_wrapper->pplugin->first_displayed = rpos;
    _pplugin_paint_widgets(pp_wrapper);
  }
}

static void _vpplugin_nextprev(xitk_widget_t *w, void *data, int pos) {
  _pplugin_nextprev(&vpp_wrapper, w, data, pos);
}

static void _applugin_nextprev(xitk_widget_t *w, void *data, int pos) {
  _pplugin_nextprev(&app_wrapper, w, data, pos);
}

static void pplugin_exit(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  
  if(pp_wrapper->pplugin) {
    window_info_t wi;
    int           i;
    
    if( pp_wrapper->pplugin->help_running )
     _pplugin_close_help(pp_wrapper, NULL,NULL);

    if(pp_wrapper->pplugin->help_text) {
      i = 0;
      while(pp_wrapper->pplugin->help_text[i++])
	free(pp_wrapper->pplugin->help_text[i]);
      free(pp_wrapper->pplugin->help_text);
    }
    
    _pplugin_save_chain(pp_wrapper);

    pp_wrapper->pplugin->running = 0;
    pp_wrapper->pplugin->visible = 0;
    
    if((xitk_get_window_info(pp_wrapper->pplugin->widget_key, &wi))) {
      config_update_num ((pp_wrapper == &vpp_wrapper) ? "gui.vpplugin_x" : "gui.applugin_x", wi.x);
      config_update_num ((pp_wrapper == &vpp_wrapper) ? "gui.vpplugin_y" : "gui.applugin_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    i = pp_wrapper->pplugin->object_num - 1;
    if(i > 0) {

      while(i > 0) {
	_pplugin_destroy_only_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);
	_pplugin_destroy_base_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[i]);
	free(pp_wrapper->pplugin->post_objects[i]);
	i--;
      }
      
      _pplugin_destroy_only_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[0]);
      _pplugin_destroy_base_obj(pp_wrapper, pp_wrapper->pplugin->post_objects[0]);
      free(pp_wrapper->pplugin->post_objects);
    }
    
    for(i = 0; pp_wrapper->pplugin->plugin_names[i]; i++)
      free(pp_wrapper->pplugin->plugin_names[i]);
    
    free(pp_wrapper->pplugin->plugin_names);
    
    xitk_unregister_event_handler(&pp_wrapper->pplugin->widget_key);

    xitk_destroy_widgets(pp_wrapper->pplugin->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, pp_wrapper->pplugin->xwin);

    pp_wrapper->pplugin->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(pp_wrapper->pplugin->widget_list);
   
    free(pp_wrapper->pplugin);
    pp_wrapper->pplugin = NULL;

    try_to_set_input_focus(gGui->video_window);
  }
}

static void vpplugin_exit(xitk_widget_t *w, void *data) {
  pplugin_exit(&vpp_wrapper, w, data);
}

static void applugin_exit(xitk_widget_t *w, void *data) {
  pplugin_exit(&app_wrapper, w, data);
}

static void _pplugin_handle_event(_pp_wrapper_t *pp_wrapper, XEvent *event, void *data) {
  
  switch(event->type) {

  case ButtonPress:
    if(event->xbutton.button == Button4) {
      xitk_slider_make_step(pp_wrapper->pplugin->slider);
      xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
    }
    else if(event->xbutton.button == Button5) {
      xitk_slider_make_backstep(pp_wrapper->pplugin->slider);
      xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
    }
    break;

  case KeyPress:
    {
      KeySym         mkey;
      int            modifier;
      
      xitk_get_key_modifier(event, &modifier);
      mkey = xitk_get_key_pressed(event);

      switch(mkey) {
	
      case XK_Up:
	if(xitk_is_widget_enabled(pp_wrapper->pplugin->slider) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_step(pp_wrapper->pplugin->slider);
	  xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
	  return;
	}
	break;
	
      case XK_Down:
	if(xitk_is_widget_enabled(pp_wrapper->pplugin->slider) &&
	   (modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_backstep(pp_wrapper->pplugin->slider);
	  xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
	  return;
	}
	break;
	
      case XK_Next:
	if(xitk_is_widget_enabled(pp_wrapper->pplugin->slider)) {
	  int pos, max = xitk_slider_get_max(pp_wrapper->pplugin->slider);
	  
	  pos = max - (pp_wrapper->pplugin->first_displayed + MAX_DISPLAY_FILTERS);
	  xitk_slider_set_pos(pp_wrapper->pplugin->slider, (pos >= 0) ? pos : 0);
	  xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
	  return;
	}
	break;
	
      case XK_Prior:
	if(xitk_is_widget_enabled(pp_wrapper->pplugin->slider)) {
	  int pos, max = xitk_slider_get_max(pp_wrapper->pplugin->slider);
	  
	  pos = max - (pp_wrapper->pplugin->first_displayed - MAX_DISPLAY_FILTERS);
	  xitk_slider_set_pos(pp_wrapper->pplugin->slider, (pos <= max) ? pos : max);
	  xitk_slider_callback_exec(pp_wrapper->pplugin->slider);
	  return;
	}
	break;
	
      case XK_Escape:
	pplugin_exit(pp_wrapper, NULL, NULL);
	return;
      }
      gui_handle_event(event, data);
    }
    break;
  }
}

static void _vpplugin_handle_event(XEvent *event, void *data) {
  _pplugin_handle_event(&vpp_wrapper, event, data);
}

static void _applugin_handle_event(XEvent *event, void *data) {
  _pplugin_handle_event(&app_wrapper, event, data);
}

static void _pplugin_enability(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int state) {
  if (pp_wrapper == &vpp_wrapper)
    gGui->post_video_enable = state;
  else
    gGui->post_audio_enable = state;
  
  _pplugin_unwire(pp_wrapper);
  _pplugin_rewire(pp_wrapper);
}

static void _vpplugin_enability(xitk_widget_t *w, void *data, int state) {
  _pplugin_enability(&vpp_wrapper, w, data, state);
}

static void _applugin_enability(xitk_widget_t *w, void *data, int state) {
  _pplugin_enability(&app_wrapper, w, data, state);
}

static int pplugin_is_post_selected(_pp_wrapper_t *pp_wrapper) {
  
  if(pp_wrapper->pplugin) {
    int post_num = pp_wrapper->pplugin->object_num;
    
    if(!xitk_combo_get_current_selected(pp_wrapper->pplugin->post_objects[post_num - 1]->plugins))
      post_num--;

    return (post_num > 0);
  }
  
  return (pp_wrapper == &vpp_wrapper) ? gGui->post_video_elements_num : gGui->post_audio_elements_num;
}

static void pplugin_rewire_from_posts_window(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin) {
    _pplugin_unwire(pp_wrapper);
    _pplugin_rewire(pp_wrapper);
  }
}

static void _vpplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {
  
  if(post_elements_num) {
    xine_post_out_t   *vo_source;
    int                i = 0;
    
    for(i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *vo_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_video_port((xine_post_out_t *) vo_out, gGui->vo_port);
      }
      else {
	const xine_post_in_t *vo_in;
	int                   err;
	
	/* look for standard input names */
	vo_in = xine_post_input(post_elements[i + 1]->post, "video");
	if( !vo_in )
	  vo_in = xine_post_input(post_elements[i + 1]->post, "video in");
	
	err = xine_post_wire((xine_post_out_t *) vo_out, (xine_post_in_t *) vo_in);	
      }
    }
    
    vo_source = xine_get_video_source(gGui->stream);
    xine_post_wire_video_port(vo_source, post_elements[0]->post->video_input[0]);
  }
}

static void _applugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {
  
  if(post_elements_num) {
    xine_post_out_t   *ao_source;
    int                i = 0;
    
    for(i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *ao_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_audio_port((xine_post_out_t *) ao_out, gGui->ao_port);
      }
      else {
	const xine_post_in_t *ao_in;
	int                   err;
	
	/* look for standard input names */
	ao_in = xine_post_input(post_elements[i + 1]->post, "audio");
	if( !ao_in )
	  ao_in = xine_post_input(post_elements[i + 1]->post, "audio in");
	
	err = xine_post_wire((xine_post_out_t *) ao_out, (xine_post_in_t *) ao_in);	
      }
    }
    
    ao_source = xine_get_audio_source(gGui->stream);
    xine_post_wire_audio_port(ao_source, post_elements[0]->post->audio_input[0]);
  }
}

static void _pplugin_rewire_from_post_elements(_pp_wrapper_t *pp_wrapper, post_element_t **post_elements, int post_elements_num) {
  if (pp_wrapper == &vpp_wrapper)
    _vpplugin_rewire_from_post_elements(post_elements, post_elements_num);
  else
    _applugin_rewire_from_post_elements(post_elements, post_elements_num);
}

static void pplugin_rewire_posts(_pp_wrapper_t *pp_wrapper) {
  _pplugin_unwire(pp_wrapper);
  _pplugin_rewire(pp_wrapper);

#if 0
  if(gGui->post_enable && !gGui->deinterlace_enable)
    _pplugin_rewire_from_post_elements(gGui->post_elements, gGui->post_elements_num);
#endif
}

static void pplugin_end(_pp_wrapper_t *pp_wrapper) {
  pplugin_exit(pp_wrapper, NULL, NULL);
}

static int pplugin_is_visible(_pp_wrapper_t *pp_wrapper) {
  
  if(pp_wrapper->pplugin) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(pp_wrapper->pplugin->xwin));
    else
      return pp_wrapper->pplugin->visible && xitk_is_window_visible(gGui->display, xitk_window_get_window(pp_wrapper->pplugin->xwin));
  }
  
  return 0;
}

static int pplugin_is_running(_pp_wrapper_t *pp_wrapper) {
  
  if(pp_wrapper->pplugin)
    return pp_wrapper->pplugin->running;
  
  return 0;
}

static void pplugin_raise_window(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin != NULL)
    raise_window(xitk_window_get_window(pp_wrapper->pplugin->xwin), pp_wrapper->pplugin->visible, pp_wrapper->pplugin->running);
}


static void pplugin_toggle_visibility(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  if(pp_wrapper->pplugin != NULL)
    toggle_window(xitk_window_get_window(pp_wrapper->pplugin->xwin), pp_wrapper->pplugin->widget_list,
		  &pp_wrapper->pplugin->visible, pp_wrapper->pplugin->running);
}

static void pplugin_update_enable_button(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin)
    xitk_labelbutton_set_state(pp_wrapper->pplugin->enable, (pp_wrapper == &vpp_wrapper) ? gGui->post_video_enable : gGui->post_audio_enable);
}

static void pplugin_reparent(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin)
    reparent_window((xitk_window_get_window(pp_wrapper->pplugin->xwin)));
}

static void pplugin_panel(_pp_wrapper_t *pp_wrapper) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  int                         x, y, width, height;
  xitk_slider_widget_t        sl;
  xitk_widget_t              *w;

  pp_wrapper->pplugin = (_pplugin_t *) calloc(1, sizeof(_pplugin_t));
  pp_wrapper->pplugin->first_displayed = 0;
  pp_wrapper->pplugin->help_text       = NULL;
  
  x = xine_config_register_num (__xineui_global_xine_instance, (pp_wrapper == &vpp_wrapper) ? "gui.vpplugin_x" : "gui.applugin_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, (pp_wrapper == &vpp_wrapper) ? "gui.vpplugin_y" : "gui.applugin_y",
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  pp_wrapper->pplugin->xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
						   (pp_wrapper == &vpp_wrapper) ? _("Video Chain Reaction") : _("Audio Chain Reaction"),
                                                   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start((xitk_window_get_window(pp_wrapper->pplugin->xwin)));

  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(pp_wrapper->pplugin->xwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  pp_wrapper->pplugin->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(pp_wrapper->pplugin->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(pp_wrapper->pplugin->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(pp_wrapper->pplugin->xwin)));
  xitk_widget_list_set(pp_wrapper->pplugin->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  xitk_window_get_window_size(pp_wrapper->pplugin->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);

  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(pp_wrapper->pplugin->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay (gGui->display);
  
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);
  
  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_nextprev : _applugin_nextprev;
  sl.motion_userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)),
   (pp_wrapper->pplugin->slider = xitk_noskin_slider_create(pp_wrapper->pplugin->widget_list, &sl,
							    (WINDOW_WIDTH - (16 + 15)), 34,
							    16, (MAX_DISPLAY_FILTERS * (FRAME_HEIGHT + 4) - 4),
							    XITK_VSLIDER)));

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("New Filter");
  lb.align             = ALIGN_CENTER;
  lb.callback          = (pp_wrapper == &vpp_wrapper) ? _vpplugin_add_filter : _applugin_add_filter; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
   (pp_wrapper->pplugin->new_filter = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->widget_list, 
								     &lb, x, y, 100, 23,
								     "Black", "Black", "White", btnfontname)));
  xitk_show_widget(pp_wrapper->pplugin->new_filter);

  x += (100 + 15);

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Enable");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = (pp_wrapper == &vpp_wrapper) ? _vpplugin_enability : _applugin_enability;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
   (pp_wrapper->pplugin->enable = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->widget_list, 
								 &lb, x, y, 100, 23,
								 "Black", "Black", "White", btnfontname)));

  xitk_labelbutton_set_state(pp_wrapper->pplugin->enable, (pp_wrapper == &vpp_wrapper) ? gGui->post_video_enable : gGui->post_audio_enable);
  xitk_enable_and_show_widget(pp_wrapper->pplugin->enable);

  /* IMPLEMENT ME
  x += (100 + 15);

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
   (w = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->widget_list, 
				       &lb, x, y, 100, 23,
				       "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(w);
  */

  x = WINDOW_WIDTH - (100 + 15);
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("OK");
  lb.align             = ALIGN_CENTER;
  lb.callback          = (pp_wrapper == &vpp_wrapper) ? vpplugin_exit : applugin_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pp_wrapper->pplugin->widget_list)), 
   (w = xitk_noskin_labelbutton_create(pp_wrapper->pplugin->widget_list, 
				       &lb, x, y, 100, 23,
				       "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(w);

  _pplugin_get_plugins(pp_wrapper);
  
  pp_wrapper->pplugin->x = 15;
  pp_wrapper->pplugin->y = 34 - (FRAME_HEIGHT + 4);
  
  if((pp_wrapper == &vpp_wrapper) ? gGui->post_video_elements : gGui->post_audio_elements)
    _pplugin_rebuild_filters(pp_wrapper);
  else
    _pplugin_add_filter(pp_wrapper, NULL, NULL);
  
  xitk_window_change_background(gGui->imlib_data, pp_wrapper->pplugin->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);
  
  pp_wrapper->pplugin->widget_key = xitk_register_event_handler((pp_wrapper == &vpp_wrapper) ? "vpplugin" : "applugin", 
						    (xitk_window_get_window(pp_wrapper->pplugin->xwin)),
						    (pp_wrapper == &vpp_wrapper) ? _vpplugin_handle_event : _applugin_handle_event,
						    NULL,
						    NULL,
						    pp_wrapper->pplugin->widget_list,
						    NULL);
  
  pp_wrapper->pplugin->visible = 1;
  pp_wrapper->pplugin->running = 1;

  _pplugin_paint_widgets(pp_wrapper);

  pplugin_raise_window(pp_wrapper);
  
  try_to_set_input_focus(xitk_window_get_window(pp_wrapper->pplugin->xwin));
}

/* pchain: "<post1>:option1=value1,option2=value2..;<post2>:...." */
static post_element_t **pplugin_parse_and_load(_pp_wrapper_t *pp_wrapper, const char *pchain, int *post_elements_num) {
  int plugin_type = (pp_wrapper == &vpp_wrapper) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  post_element_t **post_elements = NULL;

  *post_elements_num = 0;
  
  if(pchain && strlen(pchain)) {
    char *p;
    
    char *post_chain = strdup(pchain);
    
    while((p = xine_strsep(&post_chain, ";"))) {
      
      if(p && strlen(p)) {
	char          *plugin, *args = NULL;
	xine_post_t   *post;
	
	while(*p == ' ')
	  p++;
	
	plugin = strdup(p);
	
	if((p = strchr(plugin, ':')))
	  *p++ = '\0';
	
	if(p && (strlen(p) > 1))
	  args = p;
	
	post = xine_post_init(__xineui_global_xine_instance, plugin, 0, &gGui->ao_port, &gGui->vo_port);

        if (post && pp_wrapper) {
          if (post->type != plugin_type) {
            xine_post_dispose(__xineui_global_xine_instance, post);
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
		
		if(p && strlen(p)) {
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

static void pplugin_parse_and_store_post(_pp_wrapper_t *pp_wrapper, const char *post_chain) {
  post_element_t ***_post_elements = (pp_wrapper == &vpp_wrapper) ? &gGui->post_video_elements : &gGui->post_audio_elements;
  int *_post_elements_num = (pp_wrapper == &vpp_wrapper) ? &gGui->post_video_elements_num : &gGui->post_audio_elements_num;
  post_element_t **posts = NULL;
  int              num;

  if((posts = pplugin_parse_and_load(pp_wrapper, post_chain, &num))) {
    if(*_post_elements_num) {
      int i;
      int ptot = *_post_elements_num + num;
      
      *_post_elements = (post_element_t **) realloc(*_post_elements, sizeof(post_element_t *) * (ptot + 1));
      for(i = *_post_elements_num; i <  ptot; i++)
	(*_post_elements)[i] = posts[i - *_post_elements_num];

      (*_post_elements)[i]   = NULL;
      (*_post_elements_num) += num;

    }
    else {
      *_post_elements     = posts;
      *_post_elements_num = num;
    }
  }
  
}

static char *_pplugin_get_default_deinterlacer(void) {
  return DEFAULT_DEINTERLACER;
}

void post_deinterlace_init(const char *deinterlace_post) {
  post_element_t **posts = NULL;
  int              num;
  char            *deinterlace_default;

  deinterlace_default = _pplugin_get_default_deinterlacer();
  
  gGui->deinterlace_plugin = 
    (char *) xine_config_register_string (__xineui_global_xine_instance, "gui.deinterlace_plugin", 
					  deinterlace_default,
					  _("Deinterlace plugin."),
					  _("Plugin (with optional parameters) to use "
					    "when deinterlace is used (plugin separator is ';')."),
					  CONFIG_LEVEL_ADV,
					  post_deinterlace_plugin_cb,
					  CONFIG_NO_DATA);
  if((posts = pplugin_parse_and_load(0, (deinterlace_post && strlen(deinterlace_post)) ? 
				     deinterlace_post : gGui->deinterlace_plugin, &num))) {
    gGui->deinterlace_elements     = posts;
    gGui->deinterlace_elements_num = num;
  }
  
}

void post_deinterlace(void) {

  if( !gGui->deinterlace_elements_num ) {
    /* fallback to the old method */
    xine_set_param(gGui->stream, XINE_PARAM_VO_DEINTERLACE,
                   gGui->deinterlace_enable);
  }
  else {
    _pplugin_unwire(&vpp_wrapper);
    _pplugin_rewire(&vpp_wrapper);
  }

#if 0
  if(gGui->deinterlace_enable) {
    if(gGui->post_enable)
      _pplugin_unwire();
    
    _pplugin_rewire_from_post_elements(gGui->deinterlace_elements, gGui->deinterlace_elements_num);
  }
  else {
    _pplugin_unwire();
    
    if(gGui->post_enable) {
      if(pplugin_is_visible() && pplugin)
	_pplugin_rewire();
      else
	_pplugin_rewire_from_post_elements(gGui->post_elements, gGui->post_elements_num);
    }
  }
#endif
}

void vpplugin_end(void)
{
  pplugin_end(&vpp_wrapper);
}

int vpplugin_is_visible(void)
{
  return pplugin_is_visible(&vpp_wrapper);
}

int vpplugin_is_running(void)
{
  return pplugin_is_running(&vpp_wrapper);
}

void vpplugin_toggle_visibility(xitk_widget_t *w, void *data)
{
  pplugin_toggle_visibility(&vpp_wrapper, w, data);
}

void vpplugin_raise_window(void)
{
  pplugin_raise_window(&vpp_wrapper);
}

void vpplugin_update_enable_button(void)
{
  pplugin_update_enable_button(&vpp_wrapper);
}

void vpplugin_panel(void)
{
  pplugin_panel(&vpp_wrapper);
}

void vpplugin_parse_and_store_post(const char *post)
{
  pplugin_parse_and_store_post(&vpp_wrapper, post);
}

void vpplugin_rewire_from_posts_window(void)
{
  pplugin_rewire_from_posts_window(&vpp_wrapper);
}

void vpplugin_rewire_posts(void)
{
  pplugin_rewire_posts(&vpp_wrapper);
}

int vpplugin_is_post_selected(void)
{
  return pplugin_is_post_selected(&vpp_wrapper);
}

void vpplugin_reparent(void)
{
  pplugin_reparent(&vpp_wrapper);
}

void applugin_end(void)
{
  pplugin_end(&app_wrapper);
}

int applugin_is_visible(void)
{
  return pplugin_is_visible(&app_wrapper);
}

int applugin_is_running(void)
{
  return pplugin_is_running(&app_wrapper);
}

void applugin_toggle_visibility(xitk_widget_t *w, void *data)
{
  pplugin_toggle_visibility(&app_wrapper, w, data);
}

void applugin_raise_window(void)
{
  pplugin_raise_window(&app_wrapper);
}

void applugin_update_enable_button(void)
{
  pplugin_update_enable_button(&app_wrapper);
}

void applugin_panel(void)
{
  pplugin_panel(&app_wrapper);
}

void applugin_parse_and_store_post(const char *post)
{
  pplugin_parse_and_store_post(&app_wrapper, post);
}

void applugin_rewire_from_posts_window(void)
{
  pplugin_rewire_from_posts_window(&app_wrapper);
}

void applugin_rewire_posts(void)
{
  pplugin_rewire_posts(&app_wrapper);
}

int applugin_is_post_selected(void)
{
  return pplugin_is_post_selected(&app_wrapper);
}

void applugin_reparent(void)
{
  pplugin_reparent(&app_wrapper);
}
