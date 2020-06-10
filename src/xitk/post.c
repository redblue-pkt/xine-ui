/* 
 * Copyright (C) 2000-2019 the xine project
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"

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
  int                          is_video;

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

typedef struct {
  xitk_window_t              *xwin;

  xitk_widget_list_t         *widget_list;

  xitk_widget_t              *slider;

  char                      **plugin_names;

  post_object_t              *post_objects[MAX_USED_FILTERS];
  int                         first_displayed;
  int                         object_num;

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
  int                         is_video;
  _pplugin_t                 *pplugin;
  post_element_t            **post_elements;
  int                         post_elements_num;
} _pp_wrapper_t;

typedef struct {
  _pp_wrapper_t   p;

  const char      *deinterlace_plugin;
  post_element_t **deinterlace_elements;
  int              deinterlace_elements_num;

} _vpp_wrapper_t;

typedef struct {
  _pp_wrapper_t   p;

  char          **post_audio_vis_plugins;

} _app_wrapper_t;

static _vpp_wrapper_t _vpp_wrapper = {{ 1, NULL, NULL, 0 }, NULL, NULL, 0 };
static _app_wrapper_t _app_wrapper = {{ 0, NULL, NULL, 0 }, NULL };

/* Some functions prototype */
static void _vpplugin_unwire(void);
static void _applugin_unwire(void);
static post_element_t **pplugin_parse_and_load(_pp_wrapper_t *pp_wrapper, const char *, int *);
static void _vpplugin_rewire(void);
static void _applugin_rewire(void);
static void _vpplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num);
static void _applugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num);
static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *, _vpp_wrapper_t *vpp_wrapper);
static post_element_t **_pplugin_join_visualization_and_post_elements(int *, _pp_wrapper_t *pp_wrapper);
static void _pplugin_save_chain(_pp_wrapper_t *pp_wrapper);
static void _pplugin_close_help(_pp_wrapper_t *pp_wrapper, xitk_widget_t *, void *);

static void post_deinterlace_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  post_element_t **posts = NULL;
  int              num, i;
  _vpp_wrapper_t *vpp_wrapper = &_vpp_wrapper;

  vpp_wrapper->deinterlace_plugin = cfg->str_value;
  
  if(gui->deinterlace_enable)
    _vpplugin_unwire();
  
  for(i = 0; i < vpp_wrapper->deinterlace_elements_num; i++) {
    xine_post_dispose(__xineui_global_xine_instance, vpp_wrapper->deinterlace_elements[i]->post);
    free(vpp_wrapper->deinterlace_elements[i]->name);
    VFREE(vpp_wrapper->deinterlace_elements[i]);
  }
  
  SAFE_FREE(vpp_wrapper->deinterlace_elements);
  vpp_wrapper->deinterlace_elements_num = 0;
  
  if((posts = pplugin_parse_and_load(&vpp_wrapper->p, vpp_wrapper->deinterlace_plugin, &num))) {
    vpp_wrapper->deinterlace_elements     = posts;
    vpp_wrapper->deinterlace_elements_num = num;
  }

  if(gui->deinterlace_enable)
    _vpplugin_rewire();
}

static void post_audio_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  gGui_t *gui = data;
  gui->visual_anim.post_plugin_num = cfg->num_value;  
  post_rewire_visual_anim();
}

const char **post_get_audio_plugins_names(void) {
  _app_wrapper_t *app_wrapper = &_app_wrapper;

  return (const char **) app_wrapper->post_audio_vis_plugins;
}

void post_init(void) {
  _app_wrapper_t *app_wrapper = &_app_wrapper;
  gGui_t *gui = gGui;

  memset(&gui->visual_anim.post_output_element, 0, sizeof (gui->visual_anim.post_output_element));
  
  gui->visual_anim.post_plugin_num = -1;
  gui->visual_anim.post_changed = 0;

  if(gui->ao_port) {
    const char *const *pol = xine_list_post_plugins_typed(__xineui_global_xine_instance, 
							  XINE_POST_TYPE_AUDIO_VISUALIZATION);
    
    if(pol) {
      int  num_plug = 0;
      
      while(pol[num_plug]) {

        app_wrapper->post_audio_vis_plugins = (char **) realloc(app_wrapper->post_audio_vis_plugins, sizeof(char *) * (num_plug + 2));

        app_wrapper->post_audio_vis_plugins[num_plug]     = strdup(pol[num_plug]);
        app_wrapper->post_audio_vis_plugins[num_plug + 1] = NULL;
	num_plug++;
      }
      
      if(num_plug) {
	
	gui->visual_anim.post_plugin_num = 
	  xine_config_register_enum(__xineui_global_xine_instance, "gui.post_audio_plugin", 
				    0, app_wrapper->post_audio_vis_plugins,
				    _("Audio visualization plugin"),
				    _("Post audio plugin to used when playing streams without video"),
				    CONFIG_LEVEL_BEG,
				    post_audio_plugin_cb,
				    gGui);
	
	gui->visual_anim.post_output_element.post = 
	  xine_post_init(__xineui_global_xine_instance,
                         app_wrapper->post_audio_vis_plugins[gui->visual_anim.post_plugin_num], 0,
			 &gui->ao_port, &gui->vo_port);
	
      }
    }
  }
}

void post_rewire_visual_anim(void) {
  _app_wrapper_t *app_wrapper = &_app_wrapper;
  gGui_t *gui = gGui;

  if(gui->visual_anim.post_output_element.post) {
    (void) post_rewire_audio_port_to_stream(gui->stream);
    
    xine_post_dispose(__xineui_global_xine_instance, gui->visual_anim.post_output_element.post);
  }
  
  gui->visual_anim.post_output_element.post = 
    xine_post_init(__xineui_global_xine_instance,
                   app_wrapper->post_audio_vis_plugins[gui->visual_anim.post_plugin_num], 0,
		   &gui->ao_port, &gui->vo_port);

  if(gui->visual_anim.post_output_element.post && 
     (gui->visual_anim.enabled == 1) && (gui->visual_anim.running == 1)) {

    (void) post_rewire_audio_post_to_stream(gui->stream);

  }
}

static int ignore_visual_anim = 1;

int post_rewire_audio_port_to_stream(xine_stream_t *stream) {

  _applugin_unwire();
  ignore_visual_anim = 1;
  _applugin_rewire();
/*  
  gGui_t *gui = gGui;
  xine_post_out_t * audio_source;
  
  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gui->ao_port);
*/
  return 1;
}

int post_rewire_audio_post_to_stream(xine_stream_t *stream) {

  _applugin_unwire();
  ignore_visual_anim = 0;
  _applugin_rewire();
/*
  xine_post_out_t * audio_source;
  gGui_t *gui = gGui;

  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gui->visual_anim.post_output->audio_input[0]);
*/
  return 1;
}

/* ================================================================ */

static void _vpplugin_unwire(void) {
  gGui_t *gui = gGui;
  xine_post_out_t  *vo_source;

  if (gui->stream) {
    vo_source = xine_get_video_source(gui->stream);

    if (gui->vo_port)
      (void) xine_post_wire_video_port(vo_source, gui->vo_port);
  }
}

static void _applugin_unwire(void) {
  gGui_t *gui = gGui;
  xine_post_out_t  *ao_source;

  if (gui->stream) {
    ao_source = xine_get_audio_source(gui->stream);

    if (gui->ao_port)
      (void) xine_post_wire_audio_port(ao_source, gui->ao_port);
  }
}

static void _pplugin_unwire(_pp_wrapper_t *pp_wrapper) {
  if (pp_wrapper == &_vpp_wrapper.p)
    _vpplugin_unwire();
  else
    _applugin_unwire();
}

static void _vpplugin_rewire(void) {
  static post_element_t **post_elements;
  int post_elements_num;
  _vpp_wrapper_t *vpp_wrapper = &_vpp_wrapper;
  
  _pplugin_save_chain(&vpp_wrapper->p);

  post_elements = _pplugin_join_deinterlace_and_post_elements(&post_elements_num, vpp_wrapper);

  if( post_elements ) {
    _vpplugin_rewire_from_post_elements(post_elements, post_elements_num);

    free(post_elements);
  }
}

static void _applugin_rewire(void) {
  static post_element_t **post_elements;
  int post_elements_num;
  _pp_wrapper_t *pp_wrapper = &_app_wrapper.p;

  _pplugin_save_chain(pp_wrapper);

  post_elements = _pplugin_join_visualization_and_post_elements(&post_elements_num, pp_wrapper);
  
  if( post_elements ) {
    _applugin_rewire_from_post_elements(post_elements, post_elements_num);

    free(post_elements);
  }
}

static void _pplugin_rewire(_pp_wrapper_t *pp_wrapper) {
  if (pp_wrapper == &_vpp_wrapper.p)
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
    xitk_window_clear_window(pp_wrapper->pplugin->xwin);
  }
}

static void _pplugin_destroy_widget (xitk_widget_t **w) {
  xitk_destroy_widget (*w);
  *w = NULL;
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
    xine_error (gGui, _("Entered value is out of bounds (%d>%d<%d)."),
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
    xine_error (gGui, _("Entered value is out of bounds (%e>%e<%e)."),
	       pobj->param->range_min, value, pobj->param->range_max);
  }
  else {
    *(double *)(pobj->param_data + pobj->param->offset) = value;
    
    _pplugin_update_parameter(pobj);
    
    xitk_doublebox_set_value(pobj->value, *(double *)(pobj->param_data + pobj->param->offset));
  }
}

static void _pplugin_set_param_char(xitk_widget_t *w, void *data, const char *text) {
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
    xine_error (gGui, _("parameter type POST_PARAM_TYPE_STRING not supported yet.\n"));

}

static void _pplugin_set_param_stringlist(xitk_widget_t *w, void *data, int value) {
  post_object_t *pobj = (post_object_t *) data;
  
  if(pobj->readonly)
    return;
  
  xine_error (gGui, _("parameter type POST_PARAM_TYPE_STRINGLIST not supported yet.\n"));
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

    XITK_WIDGET_INIT(&lb);
    lb.skin_element_name   = NULL;
    lb.label               = buffer;
    lb.callback            = NULL;
    lb.userdata            = NULL;
    pobj->comment =  xitk_noskin_label_create (pp_wrapper->pplugin->widget_list, &lb,
      0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname);
    xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->comment);

    switch(pobj->param->type) {
    case POST_PARAM_TYPE_INT:
      {
	if(pobj->param->enum_values) {
	  xitk_combo_widget_t         cmb;

          XITK_WIDGET_INIT(&cmb);
	  cmb.skin_element_name = NULL;
	  cmb.layer_above       = (is_layer_above());
	  cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
	  cmb.entries           = (const char **)pobj->param->enum_values;
	  cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
	  cmb.callback          = _pplugin_set_param_int;
	  cmb.userdata          = pobj;
          pobj->value = xitk_noskin_combo_create (pp_wrapper->pplugin->widget_list, &cmb,
            0, 0, 365, NULL, NULL);
          xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
	  xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
	}
	else {
	  xitk_intbox_widget_t      ib;
	  
          XITK_WIDGET_INIT(&ib);
	  ib.skin_element_name = NULL;
	  ib.value             = *(int *)(pobj->param_data + pobj->param->offset);
	  ib.step              = 1;
	  ib.parent_wlist      = pp_wrapper->pplugin->widget_list;
	  ib.callback          = _pplugin_set_param_int;
	  ib.userdata          = pobj;
          pobj->value =  xitk_noskin_intbox_create (pp_wrapper->pplugin->widget_list, &ib,
            0, 0, 50, 20, NULL, NULL, NULL);
          xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
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
	ib.parent_wlist      = pp_wrapper->pplugin->widget_list;
	ib.callback          = _pplugin_set_param_double;
	ib.userdata          = pobj;
        pobj->value =  xitk_noskin_doublebox_create (pp_wrapper->pplugin->widget_list, &ib,
          0, 0, 100, 20, NULL, NULL, NULL);
        xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
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
        pobj->value =  xitk_noskin_inputtext_create (pp_wrapper->pplugin->widget_list, &inp,
          0, 0, 365, 20, "Black", "Black", fontname);
        xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
      }
      break;
      
    case POST_PARAM_TYPE_STRINGLIST:
      {
	xitk_combo_widget_t         cmb;
	
        XITK_WIDGET_INIT(&cmb);
	cmb.skin_element_name = NULL;
	cmb.layer_above       = (is_layer_above());
	cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
	cmb.entries           = (const char **)(pobj->param_data + pobj->param->offset);
	cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
	cmb.callback          = _pplugin_set_param_stringlist;
	cmb.userdata          = pobj;
        pobj->value = xitk_noskin_combo_create (pp_wrapper->pplugin->widget_list, &cmb,
          0, 0, 365, NULL, NULL);
        xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
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
        pobj->value =  xitk_noskin_checkbox_create (pp_wrapper->pplugin->widget_list, &cb, 0, 0, 12, 12);
        xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->value);
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

    _pplugin_destroy_widget (&pobj->value);
    _pplugin_destroy_widget (&pobj->comment);

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
  return _pplugin_change_parameter(&_vpp_wrapper.p, w, data, select);
}

static void _applugin_change_parameter(xitk_widget_t *w, void *data, int select) {
  return _pplugin_change_parameter(&_app_wrapper.p, w, data, select);
}

static void _pplugin_get_plugins(_pp_wrapper_t *pp_wrapper) {
  int plugin_type = (pp_wrapper == &_vpp_wrapper.p) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
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
      if(pp_wrapper->pplugin->help_running)
	_pplugin_close_help(pp_wrapper, NULL,NULL);
      _pplugin_destroy_widget (&pobj->help);
    }
    
  }
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

static void _pplugin_close_help(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {

  pp_wrapper->pplugin->help_running = 0;
  
  xitk_unregister_event_handler(&pp_wrapper->pplugin->help_widget_key);

  xitk_window_destroy_window(pp_wrapper->pplugin->helpwin);
  pp_wrapper->pplugin->helpwin = NULL;
  /* xitk_dlist_init (&pp_wrapper->pplugin->help_widget_list->list); */
}

static void _vpplugin_close_help(xitk_widget_t *w, void *data) {
  _pplugin_close_help(&_vpp_wrapper.p, w, data);
}

static void _applugin_close_help(xitk_widget_t *w, void *data) {
  _pplugin_close_help(&_app_wrapper.p, w, data);
}

static void _pplugin_help_handle_event(XEvent *event, void *data) {
  _pp_wrapper_t *pp_wrapper = data;

  if(event->type == KeyPress) {
    if(xitk_get_key_pressed(event) == XK_Escape)
      _pplugin_close_help(pp_wrapper, NULL, NULL);
    else
      gui_handle_event (event, gGui);
  }
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
  gGui_t *gui = gGui;
  post_object_t *pobj = (post_object_t *) data;
  xitk_pixmap_t              *bg = NULL;
  int                         x, y;
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
    pp_wrapper->pplugin->helpwin = xitk_window_create_dialog_window(gui->imlib_data, 
                                                                    _("Plugin Help"), x, y,
                                                                    HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT);

    set_window_states_start(pp_wrapper->pplugin->helpwin);

    pp_wrapper->pplugin->help_widget_list = xitk_window_widget_list(pp_wrapper->pplugin->helpwin);

    bg = xitk_window_get_background_pixmap(pp_wrapper->pplugin->helpwin);

    XITK_WIDGET_INIT(&lb);
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Close");
    lb.align             = ALIGN_CENTER;
    lb.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_close_help : _applugin_close_help;
    lb.state_callback    = NULL;
    lb.userdata          = NULL;
    lb.skin_element_name = NULL;
    w =  xitk_noskin_labelbutton_create (pp_wrapper->pplugin->help_widget_list,
      &lb, HELP_WINDOW_WIDTH - (100 + 15), HELP_WINDOW_HEIGHT - (23 + 15), 100, 23,
      "Black", "Black", "White", btnfontname);
    xitk_add_widget (pp_wrapper->pplugin->help_widget_list, w);
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
    br.parent_wlist                  = pp_wrapper->pplugin->help_widget_list;
    br.userdata                      = NULL;
    pp_wrapper->pplugin->help_browser = xitk_noskin_browser_create (pp_wrapper->pplugin->help_widget_list,
      &br, (XITK_WIDGET_LIST_GC(pp_wrapper->pplugin->help_widget_list)), 15, 34,
      HELP_WINDOW_WIDTH - (30 + 16), 20, 16, br_fontname);
    xitk_add_widget (pp_wrapper->pplugin->help_widget_list, pp_wrapper->pplugin->help_browser);
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

    xitk_window_set_background(pp_wrapper->pplugin->helpwin, bg);

    pp_wrapper->pplugin->help_widget_key = xitk_register_event_handler((pp_wrapper == &_vpp_wrapper.p) ? "vpplugin_help" : "applugin_help", 
                                                                       pp_wrapper->pplugin->helpwin,
                                                                       _pplugin_help_handle_event,
                                                                       NULL,
                                                                       NULL,
                                                                       pp_wrapper->pplugin->help_widget_list,
                                                                       pp_wrapper);
  
    pp_wrapper->pplugin->help_running = 1;
  }

  raise_window(pp_wrapper->pplugin->helpwin, 1, pp_wrapper->pplugin->help_running);

  xitk_window_try_to_set_input_focus(pp_wrapper->pplugin->helpwin);
}

static void _vpplugin_show_help(xitk_widget_t *w, void *data) {
  _pplugin_show_help(&_vpp_wrapper.p, w, data);
}

static void _applugin_show_help(xitk_widget_t *w, void *data) {
  _pplugin_show_help(&_app_wrapper.p, w, data);
}

static void _pplugin_retrieve_parameters(_pp_wrapper_t *pp_wrapper, post_object_t *pobj) {
  xitk_combo_widget_t         cmb;
  xitk_labelbutton_widget_t   lb;
  
  if(__pplugin_retrieve_parameters(pobj)) {
    
    XITK_WIDGET_INIT(&cmb);
    cmb.skin_element_name = NULL;
    cmb.layer_above       = (is_layer_above());
    cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
    cmb.entries           = (const char **)pobj->properties_names;
    cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
    cmb.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_change_parameter : _applugin_change_parameter;
    cmb.userdata          = pobj;
    pobj->properties = xitk_noskin_combo_create (pp_wrapper->pplugin->widget_list,
      &cmb, 0, 0, 175, NULL, NULL);
    xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->properties);
    xitk_combo_set_select(pobj->properties, 0);
    xitk_combo_callback_exec(pobj->properties);

    if(pobj->api && pobj->api->get_help) {

      XITK_WIDGET_INIT(&lb);
      lb.button_type       = CLICK_BUTTON;
      lb.label             = _("Help");
      lb.align             = ALIGN_CENTER;
      lb.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_show_help : _applugin_show_help; 
      lb.state_callback    = NULL;
      lb.userdata          = pobj;
      lb.skin_element_name = NULL;
      pobj->help =  xitk_noskin_labelbutton_create (pp_wrapper->pplugin->widget_list,
        &lb, 0, 0, 63, 20, "Black", "Black", "White", btnfontname);
      xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->help);
    }
  }
  else {
    xitk_label_widget_t   lb;
    
    XITK_WIDGET_INIT(&lb);
    lb.skin_element_name   = NULL;
    lb.label               = _("There is no parameter available for this plugin.");
    lb.callback            = NULL;
    lb.userdata            = NULL;
    pobj->comment =  xitk_noskin_label_create (pp_wrapper->pplugin->widget_list, &lb,
      0, 0, FRAME_WIDTH - (26 + 6 + 6), 20, hboldfontname);
    xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->comment);
    pobj->properties = NULL;
  }
}

static post_object_t *_pplugin_create_filter_object (_pp_wrapper_t *pp_wrapper);

static void _pplugin_select_filter(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int select) {
  gGui_t *gui = gGui;
  post_object_t *pobj = (post_object_t *) data;
  post_object_t **p = pp_wrapper->pplugin->post_objects;
  int n;
  
  _pplugin_unwire(pp_wrapper);

  for (n = 0; p[n] && (p[n] != pobj); n++) ;
  if (!select) {
    /* User selected first line which is "No filter". Reset this one, and kill everything thereafter. */
    if (p[n]) {
      _pplugin_destroy_only_obj (pp_wrapper, pobj);
      if (pobj->post) {
        xine_post_dispose (__xineui_global_xine_instance, pobj->post);
        pobj->post = NULL;
      }
      n++;
      pp_wrapper->pplugin->object_num = n;
      while (p[n]) {
        _pplugin_destroy_only_obj (pp_wrapper, p[n]);
        if (p[n]->post) {
          xine_post_dispose (__xineui_global_xine_instance, p[n]->post);
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
      _pplugin_send_expose (pp_wrapper);
    }
    if(pp_wrapper->pplugin->object_num <= MAX_DISPLAY_FILTERS)
      pp_wrapper->pplugin->first_displayed = 0;
    else if((pp_wrapper->pplugin->object_num - pp_wrapper->pplugin->first_displayed) < MAX_DISPLAY_FILTERS)
      pp_wrapper->pplugin->first_displayed = pp_wrapper->pplugin->object_num - MAX_DISPLAY_FILTERS;
  } else {
    /* Some filter has been selected. Kill old one and set new. */
    _pplugin_destroy_only_obj (pp_wrapper, pobj);
    if (pobj->post) {
      xine_post_dispose (__xineui_global_xine_instance, pobj->post);
      pobj->post = NULL;
    }
    pobj->post = xine_post_init (__xineui_global_xine_instance,
      xitk_combo_get_current_entry_selected (w), 0, &gui->ao_port, &gui->vo_port);
    _pplugin_retrieve_parameters (pp_wrapper, pobj);
    /* If this is the last filter, add a new "No filter" thereafter. */
    if ((n < MAX_USED_FILTERS - 1) && (n + 1 == pp_wrapper->pplugin->object_num)) {
      _pplugin_create_filter_object (pp_wrapper);
    }
  }
  
  if(pp_wrapper->pplugin->help_running)
    _pplugin_show_help(pp_wrapper, NULL, pobj);

  _pplugin_rewire(pp_wrapper);
  _pplugin_paint_widgets(pp_wrapper);
}

static void _vpplugin_select_filter(xitk_widget_t *w, void *data, int select) {
  _pplugin_select_filter(&_vpp_wrapper.p, w, data, select);
}

static void _applugin_select_filter(xitk_widget_t *w, void *data, int select) {
  _pplugin_select_filter(&_app_wrapper.p, w, data, select);
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
  _pplugin_move_up(&_vpp_wrapper.p, w, data);
}

static void _applugin_move_up(xitk_widget_t *w, void *data) {
  _pplugin_move_up(&_app_wrapper.p, w, data);
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
  _pplugin_move_down(&_vpp_wrapper.p, w, data);
}

static void _applugin_move_down(xitk_widget_t *w, void *data) {
  _pplugin_move_down(&_app_wrapper.p, w, data);
}

static post_object_t *_pplugin_create_filter_object (_pp_wrapper_t *pp_wrapper) {
  gGui_t *gui = gGui;
  xitk_combo_widget_t   cmb;
  post_object_t        *pobj;
  xitk_image_widget_t   im;
  xitk_image_t         *image;
  xitk_button_widget_t  b;

  if (pp_wrapper->pplugin->object_num >= MAX_USED_FILTERS - 1)
    return NULL;
  pobj = calloc (1, sizeof (post_object_t));
  if (!pobj)
    return NULL;
  
  pp_wrapper->pplugin->x = 15;
  pp_wrapper->pplugin->y += FRAME_HEIGHT + 4;
  
  pp_wrapper->pplugin->post_objects[pp_wrapper->pplugin->object_num] = pobj;
  pobj->x = pp_wrapper->pplugin->x;
  pobj->y = pp_wrapper->pplugin->y;
  pp_wrapper->pplugin->object_num++;

  image = xitk_image_create_image(gui->imlib_data, FRAME_WIDTH + 1, FRAME_HEIGHT + 1);

  pixmap_fill_rectangle(image->image,
                        0, 0, image->width, image->height,
                        xitk_get_pixel_color_gray(gui->imlib_data));

  /* Some decorations */
  draw_outter_frame(image->image, NULL, NULL,
		    0, 0, FRAME_WIDTH, FRAME_HEIGHT);
  draw_rectangular_outter_box_light(image->image,
		    27, 28, FRAME_WIDTH - 27 - 5, 1);
  draw_rectangular_outter_box_light(image->image,
		    26, 5, 1, FRAME_HEIGHT - 10);
  draw_inner_frame(image->image, NULL, NULL,
		    5, 5, 16, 16);
  draw_rectangular_inner_box_light(image->image,
		    5, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(image->image,
		    10, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(image->image,
		    15, 24, 1, FRAME_HEIGHT - 48);
  draw_rectangular_inner_box_light(image->image,
		    20, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(image->image, NULL, NULL,
		    5, FRAME_HEIGHT - 16 - 5, 16, 16);

  XITK_WIDGET_INIT(&im);
  im.skin_element_name = NULL;
  pobj->frame =  xitk_noskin_image_create (pp_wrapper->pplugin->widget_list, &im, image, pobj->x - 5, pobj->y - 5);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->frame);

  DISABLE_ME(pobj->frame);
  
  XITK_WIDGET_INIT(&cmb);
  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.parent_wlist      = pp_wrapper->pplugin->widget_list;
  cmb.entries           = (const char **)pp_wrapper->pplugin->plugin_names;
  cmb.parent_wkey       = &pp_wrapper->pplugin->widget_key;
  cmb.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_select_filter : _applugin_select_filter;
  cmb.userdata          = pobj;
  pobj->plugins = xitk_noskin_combo_create (pp_wrapper->pplugin->widget_list,
    &cmb, 0, 0, 175, NULL, NULL);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->plugins);
  DISABLE_ME(pobj->plugins);
  xitk_combo_set_select(pobj->plugins, 0);

  {
    xitk_image_t *bimage;

    XITK_WIDGET_INIT(&b);
    b.skin_element_name = NULL;
    b.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_move_up : _applugin_move_up;
    b.userdata          = pobj;
    pobj->up = xitk_noskin_button_create (pp_wrapper->pplugin->widget_list, &b, 0, 0, 17, 17);
    xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->up);
    DISABLE_ME(pobj->up);

    if((bimage = xitk_get_widget_foreground_skin(pobj->up)) != NULL)
      draw_arrow_up(bimage);
    
    b.skin_element_name = NULL;
    b.callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_move_down : _applugin_move_down;
    b.userdata          = pobj;
    pobj->down = xitk_noskin_button_create (pp_wrapper->pplugin->widget_list, &b, 0, 0, 17, 17);
    xitk_add_widget (pp_wrapper->pplugin->widget_list, pobj->down);
    DISABLE_ME(pobj->down);

    if((bimage = xitk_get_widget_foreground_skin(pobj->down)) != NULL)
      draw_arrow_down(bimage);

  }
  return pobj;
}

static int _pplugin_rebuild_filters(_pp_wrapper_t *pp_wrapper) {
  post_element_t  **pelem = pp_wrapper->post_elements;
  int plugin_type = (pp_wrapper == &_vpp_wrapper.p) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
  int num_filters = 0;
  
  while(pelem && *pelem) {
    int i = 0;

    if ((*pelem)->post->type == plugin_type) {
      num_filters++;
      
      if (!_pplugin_create_filter_object (pp_wrapper))
        break;
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
  /* Always have 1 "No filter" at the end for later extension. */
  _pplugin_create_filter_object (pp_wrapper);
  
  _pplugin_paint_widgets(pp_wrapper);

  return num_filters;
}

static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num, _vpp_wrapper_t *vpp_wrapper) {
  gGui_t *gui = gGui;
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( gui->post_video_enable )
    (*post_elements_num) += vpp_wrapper->p.post_elements_num;
  if( gui->deinterlace_enable )
    (*post_elements_num) += vpp_wrapper->deinterlace_elements_num;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **) 
    calloc((*post_elements_num), sizeof(post_element_t *));

  for( i = 0; gui->deinterlace_enable && i < vpp_wrapper->deinterlace_elements_num; i++ ) {
    post_elements[i+j] = vpp_wrapper->deinterlace_elements[i];
  }

  for( j = 0; gui->post_video_enable && j < vpp_wrapper->p.post_elements_num; j++ ) {
    post_elements[i+j] = vpp_wrapper->p.post_elements[j];
  }
  
  return post_elements;
}


static post_element_t **_pplugin_join_visualization_and_post_elements(int *post_elements_num, _pp_wrapper_t *pp_wrapper) {
  gGui_t *gui = gGui;
  post_element_t **post_elements;
  int i = 0, j = 0;

  *post_elements_num = 0;
  if( gui->post_audio_enable )
    (*post_elements_num) += pp_wrapper->post_elements_num;
  if( !ignore_visual_anim )
    (*post_elements_num)++;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **) 
    calloc((*post_elements_num), sizeof(post_element_t *));
  
  for( j = 0; gui->post_audio_enable && j < pp_wrapper->post_elements_num; j++ ) {
    post_elements[i+j] = pp_wrapper->post_elements[j];
  }

  for( i = 0; !ignore_visual_anim && i < 1; i++ ) {
    post_elements[i+j] = &gui->visual_anim.post_output_element;
  }

  return post_elements;
}


static void _pplugin_save_chain(_pp_wrapper_t *pp_wrapper) {

  if(pp_wrapper->pplugin) {
    // XXX simplify
    post_element_t ***_post_elements = &pp_wrapper->post_elements;
    int *_post_elements_num = &pp_wrapper->post_elements_num;
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
	  VFREE((*_post_elements)[j]);
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

static void _pplugin_nextprev(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int pos) {
  int rpos = (xitk_slider_get_max(pp_wrapper->pplugin->slider)) - pos;

  if(rpos != pp_wrapper->pplugin->first_displayed) {
    pp_wrapper->pplugin->first_displayed = rpos;
    _pplugin_paint_widgets(pp_wrapper);
  }
}

static void _vpplugin_nextprev(xitk_widget_t *w, void *data, int pos) {
  _pplugin_nextprev(&_vpp_wrapper.p, w, data, pos);
}

static void _applugin_nextprev(xitk_widget_t *w, void *data, int pos) {
  _pplugin_nextprev(&_app_wrapper.p, w, data, pos);
}

static void pplugin_exit(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  gGui_t *gui = gGui;

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
      config_update_num ((pp_wrapper == &_vpp_wrapper.p) ? "gui.vpplugin_x" : "gui.applugin_x", wi.x);
      config_update_num ((pp_wrapper == &_vpp_wrapper.p) ? "gui.vpplugin_y" : "gui.applugin_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    {
      post_object_t **p = pp_wrapper->pplugin->post_objects;
      while (*p) {
        _pplugin_destroy_only_obj (pp_wrapper, *p);
        _pplugin_destroy_widget (&(*p)->plugins);
        _pplugin_destroy_widget (&(*p)->frame);
        _pplugin_destroy_widget (&(*p)->up);
        _pplugin_destroy_widget (&(*p)->down);
        VFREE (*p);
        *p = NULL;
        p++;
      }
    }
    pp_wrapper->pplugin->object_num = 0;

    for(i = 0; pp_wrapper->pplugin->plugin_names[i]; i++)
      free(pp_wrapper->pplugin->plugin_names[i]);
    
    free(pp_wrapper->pplugin->plugin_names);
    
    xitk_unregister_event_handler(&pp_wrapper->pplugin->widget_key);

    xitk_window_destroy_window(pp_wrapper->pplugin->xwin);
    /* xitk_dlist_init (&pp_wrapper->pplugin->widget_list->list); */

    VFREE(pp_wrapper->pplugin);
    pp_wrapper->pplugin = NULL;

    video_window_set_input_focus(gui->vwin);
  }
}

static void vpplugin_exit(xitk_widget_t *w, void *data) {
  pplugin_exit(&_vpp_wrapper.p, w, data);
}

static void applugin_exit(xitk_widget_t *w, void *data) {
  pplugin_exit(&_app_wrapper.p, w, data);
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
      gui_handle_event (event, gGui);
    }
    break;
  }
}

static void _vpplugin_handle_event(XEvent *event, void *data) {
  _pplugin_handle_event(&_vpp_wrapper.p, event, data);
}

static void _applugin_handle_event(XEvent *event, void *data) {
  _pplugin_handle_event(&_app_wrapper.p, event, data);
}

static void _pplugin_enability(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data, int state) {
  gGui_t *gui = gGui;
  if (pp_wrapper == &_vpp_wrapper.p)
    gui->post_video_enable = state;
  else
    gui->post_audio_enable = state;
  
  _pplugin_unwire(pp_wrapper);
  _pplugin_rewire(pp_wrapper);
}

static void _vpplugin_enability(xitk_widget_t *w, void *data, int state) {
  _pplugin_enability(&_vpp_wrapper.p, w, data, state);
}

static void _applugin_enability(xitk_widget_t *w, void *data, int state) {
  _pplugin_enability(&_app_wrapper.p, w, data, state);
}

static int pplugin_is_post_selected(_pp_wrapper_t *pp_wrapper) {

  if(pp_wrapper->pplugin) {
    int post_num = pp_wrapper->pplugin->object_num;
    
    if(!xitk_combo_get_current_selected(pp_wrapper->pplugin->post_objects[post_num - 1]->plugins))
      post_num--;

    return (post_num > 0);
  }
  
  return pp_wrapper->post_elements_num;
}

static void pplugin_rewire_from_posts_window(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin) {
    _pplugin_unwire(pp_wrapper);
    _pplugin_rewire(pp_wrapper);
  }
}

static void _vpplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {
  gGui_t *gui = gGui;
  
  if(post_elements_num) {
    xine_post_out_t   *vo_source;
    int                i = 0;
    
    for(i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *vo_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_video_port((xine_post_out_t *) vo_out, gui->vo_port);
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
    
    vo_source = xine_get_video_source(gui->stream);
    xine_post_wire_video_port(vo_source, post_elements[0]->post->video_input[0]);
  }
}

static void _applugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {
  gGui_t *gui = gGui;
  
  if(post_elements_num) {
    xine_post_out_t   *ao_source;
    int                i = 0;
    
    for(i = (post_elements_num - 1); i >= 0; i--) {
      /* use the first output from plugin */
      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *ao_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_audio_port((xine_post_out_t *) ao_out, gui->ao_port);
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
    
    ao_source = xine_get_audio_source(gui->stream);
    xine_post_wire_audio_port(ao_source, post_elements[0]->post->audio_input[0]);
  }
}

static void pplugin_rewire_posts(_pp_wrapper_t *pp_wrapper) {
  _pplugin_unwire(pp_wrapper);
  _pplugin_rewire(pp_wrapper);
}

static void pplugin_end(_pp_wrapper_t *pp_wrapper) {
  pplugin_exit(pp_wrapper, NULL, NULL);
}

static int pplugin_is_visible(_pp_wrapper_t *pp_wrapper) {
  gGui_t *gui = gGui;
  
  if(pp_wrapper->pplugin) {
    if(gui->use_root_window)
      return xitk_window_is_window_visible(pp_wrapper->pplugin->xwin);
    else
      return pp_wrapper->pplugin->visible && xitk_window_is_window_visible(pp_wrapper->pplugin->xwin);
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
    raise_window(pp_wrapper->pplugin->xwin, pp_wrapper->pplugin->visible, pp_wrapper->pplugin->running);
}


static void pplugin_toggle_visibility(_pp_wrapper_t *pp_wrapper, xitk_widget_t *w, void *data) {
  if(pp_wrapper->pplugin != NULL)
    toggle_window(pp_wrapper->pplugin->xwin, pp_wrapper->pplugin->widget_list,
		  &pp_wrapper->pplugin->visible, pp_wrapper->pplugin->running);
}

static void pplugin_update_enable_button(_pp_wrapper_t *pp_wrapper) {
  gGui_t *gui = gGui;
  if(pp_wrapper->pplugin)
    xitk_labelbutton_set_state(pp_wrapper->pplugin->enable, (pp_wrapper == &_vpp_wrapper.p) ? gui->post_video_enable : gui->post_audio_enable);
}

static void pplugin_reparent(_pp_wrapper_t *pp_wrapper) {
  if(pp_wrapper->pplugin)
    reparent_window(pp_wrapper->pplugin->xwin);
}

static void pplugin_panel(_pp_wrapper_t *pp_wrapper) {
  gGui_t *gui = gGui;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  int                         x, y;
  xitk_slider_widget_t        sl;
  xitk_widget_t              *w;

  pp_wrapper->pplugin = (_pplugin_t *) calloc(1, sizeof(_pplugin_t));
  pp_wrapper->pplugin->first_displayed = 0;
  pp_wrapper->pplugin->help_text       = NULL;
  
  x = xine_config_register_num (__xineui_global_xine_instance, (pp_wrapper == &_vpp_wrapper.p) ? "gui.vpplugin_x" : "gui.applugin_x", 
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (__xineui_global_xine_instance, (pp_wrapper == &_vpp_wrapper.p) ? "gui.vpplugin_y" : "gui.applugin_y",
				80,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  pp_wrapper->pplugin->xwin = xitk_window_create_dialog_window(gui->imlib_data, 
						   (pp_wrapper == &_vpp_wrapper.p) ? _("Video Chain Reaction") : _("Audio Chain Reaction"),
                                                   x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
  
  set_window_states_start(pp_wrapper->pplugin->xwin);

  pp_wrapper->pplugin->widget_list = xitk_window_widget_list(pp_wrapper->pplugin->xwin);

  XITK_WIDGET_INIT(&lb);
  XITK_WIDGET_INIT(&lbl);
  XITK_WIDGET_INIT(&cb);

  bg = xitk_window_get_background_pixmap(pp_wrapper->pplugin->xwin);

  XITK_WIDGET_INIT(&sl);
  
  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_nextprev : _applugin_nextprev;
  sl.motion_userdata          = NULL;
  pp_wrapper->pplugin->slider =  xitk_noskin_slider_create (pp_wrapper->pplugin->widget_list, &sl,
    (WINDOW_WIDTH - (16 + 15)), 34, 16, (MAX_DISPLAY_FILTERS * (FRAME_HEIGHT + 4) - 4), XITK_VSLIDER);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, pp_wrapper->pplugin->slider);

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Enable");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_enability : _applugin_enability;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  pp_wrapper->pplugin->enable =  xitk_noskin_labelbutton_create (pp_wrapper->pplugin->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, pp_wrapper->pplugin->enable);

  xitk_labelbutton_set_state(pp_wrapper->pplugin->enable, (pp_wrapper == &_vpp_wrapper.p) ? gui->post_video_enable : gui->post_audio_enable);
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
  w =  xitk_noskin_labelbutton_create (pp_wrapper->pplugin->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, w);
  xitk_enable_and_show_widget(w);
  */

  x = WINDOW_WIDTH - (100 + 15);
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("OK");
  lb.align             = ALIGN_CENTER;
  lb.callback          = (pp_wrapper == &_vpp_wrapper.p) ? vpplugin_exit : applugin_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  w =  xitk_noskin_labelbutton_create (pp_wrapper->pplugin->widget_list,
    &lb, x, y, 100, 23, "Black", "Black", "White", btnfontname);
  xitk_add_widget (pp_wrapper->pplugin->widget_list, w);
  xitk_enable_and_show_widget(w);

  _pplugin_get_plugins(pp_wrapper);
  
  pp_wrapper->pplugin->x = 15;
  pp_wrapper->pplugin->y = 34 - (FRAME_HEIGHT + 4);
  
  _pplugin_rebuild_filters (pp_wrapper);

  xitk_window_set_background(pp_wrapper->pplugin->xwin, bg);

  pp_wrapper->pplugin->widget_key = xitk_register_event_handler((pp_wrapper == &_vpp_wrapper.p) ? "vpplugin" : "applugin", 
                                                                pp_wrapper->pplugin->xwin,
						    (pp_wrapper == &_vpp_wrapper.p) ? _vpplugin_handle_event : _applugin_handle_event,
						    NULL,
						    NULL,
						    pp_wrapper->pplugin->widget_list,
						    NULL);
  
  pp_wrapper->pplugin->visible = 1;
  pp_wrapper->pplugin->running = 1;

  _pplugin_paint_widgets(pp_wrapper);

  pplugin_raise_window(pp_wrapper);

  xitk_window_try_to_set_input_focus(pp_wrapper->pplugin->xwin);
}

/* pchain: "<post1>:option1=value1,option2=value2..;<post2>:...." */
static post_element_t **pplugin_parse_and_load(_pp_wrapper_t *pp_wrapper, const char *pchain, int *post_elements_num) {
  gGui_t *gui = gGui;
  int plugin_type = (pp_wrapper == &_vpp_wrapper.p) ? XINE_POST_TYPE_VIDEO_FILTER : XINE_POST_TYPE_AUDIO_FILTER;
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
	
	post = xine_post_init(__xineui_global_xine_instance, plugin, 0, &gui->ao_port, &gui->vo_port);

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

static void pplugin_parse_and_store_post(_pp_wrapper_t *pp_wrapper, const char *post_chain) {
  post_element_t ***_post_elements = &pp_wrapper->post_elements;
  int *_post_elements_num = &pp_wrapper->post_elements_num;
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

      free(posts);
    }
    else {
      *_post_elements     = posts;
      *_post_elements_num = num;
    }
  }
}

static void _pplugin_deinit(_pp_wrapper_t *pp_wrapper) {
  int i;
#if 0
  /* requires <xine/xine_internal.h> */
  __xineui_global_xine_instance->config->unregister_callback (__xineui_global_xine_instance, "gui.deinterlace_plugin");
#endif
  for (i = 0; i < pp_wrapper->post_elements_num; i++) {
    xine_post_dispose (__xineui_global_xine_instance, pp_wrapper->post_elements[i]->post);
    free (pp_wrapper->post_elements[i]->name);
    VFREE (pp_wrapper->post_elements[i]);
  }
  SAFE_FREE (pp_wrapper->post_elements);
  pp_wrapper->post_elements_num = 0;
}

static void _vpplugin_deinit(_vpp_wrapper_t *vpp_wrapper) {
  int i;

  _pplugin_deinit(&vpp_wrapper->p);

  //if (pp_wrapper == &vpp_wrapper) {
  for (i = 0; i < vpp_wrapper->deinterlace_elements_num; i++) {
    xine_post_dispose (__xineui_global_xine_instance, vpp_wrapper->deinterlace_elements[i]->post);
    free (vpp_wrapper->deinterlace_elements[i]->name);
    VFREE (vpp_wrapper->deinterlace_elements[i]);
  }
  SAFE_FREE (vpp_wrapper->deinterlace_elements);
  vpp_wrapper->deinterlace_elements_num = 0;
  //}
}

static void _applugin_deinit(_app_wrapper_t *app_wrapper) {

  _pplugin_deinit(&app_wrapper->p);

  if (app_wrapper->post_audio_vis_plugins) {
    int i;
    for (i = 0; app_wrapper->post_audio_vis_plugins[i]; i++)
      free(app_wrapper->post_audio_vis_plugins[i]);
    free(app_wrapper->post_audio_vis_plugins[i]);
  }
}

void post_deinit (void) {
  gGui_t *gui = gGui;

  if (gui->post_video_enable || gui->deinterlace_enable)
    _vpplugin_unwire ();

  if (gui->post_audio_enable)
    _applugin_unwire ();

  _vpplugin_deinit(&_vpp_wrapper);
  _applugin_deinit(&_app_wrapper);
}

static const char *_pplugin_get_default_deinterlacer(void) {
  return DEFAULT_DEINTERLACER;
}

void post_deinterlace_init(const char *deinterlace_post) {
  post_element_t **posts = NULL;
  int              num;
  const char      *deinterlace_default;
  _vpp_wrapper_t *vpp_wrapper = &_vpp_wrapper;

  deinterlace_default = _pplugin_get_default_deinterlacer();
  
  vpp_wrapper->deinterlace_plugin = 
    (char *) xine_config_register_string (__xineui_global_xine_instance, "gui.deinterlace_plugin", 
					  deinterlace_default,
					  _("Deinterlace plugin."),
					  _("Plugin (with optional parameters) to use "
					    "when deinterlace is used (plugin separator is ';')."),
					  CONFIG_LEVEL_ADV,
					  post_deinterlace_plugin_cb,
					  gGui);
  if((posts = pplugin_parse_and_load(0, (deinterlace_post && strlen(deinterlace_post)) ? 
                                     deinterlace_post : vpp_wrapper->deinterlace_plugin, &num))) {
    vpp_wrapper->deinterlace_elements     = posts;
    vpp_wrapper->deinterlace_elements_num = num;
  }
  
}

void post_deinterlace(void) {
  gGui_t *gui = gGui;
  _vpp_wrapper_t *vpp_wrapper = &_vpp_wrapper;

  if( !vpp_wrapper->deinterlace_elements_num ) {
    /* fallback to the old method */
    xine_set_param(gui->stream, XINE_PARAM_VO_DEINTERLACE,
                   gui->deinterlace_enable);
  }
  else {
    _pplugin_unwire(&vpp_wrapper->p);
    _pplugin_rewire(&vpp_wrapper->p);
  }
}

void vpplugin_end(void)
{
  pplugin_end(&_vpp_wrapper.p);
}

int vpplugin_is_visible(void)
{
  return pplugin_is_visible(&_vpp_wrapper.p);
}

int vpplugin_is_running(void)
{
  return pplugin_is_running(&_vpp_wrapper.p);
}

void vpplugin_toggle_visibility(xitk_widget_t *w, void *data)
{
  pplugin_toggle_visibility(&_vpp_wrapper.p, w, data);
}

void vpplugin_raise_window(void)
{
  pplugin_raise_window(&_vpp_wrapper.p);
}

void vpplugin_update_enable_button(void)
{
  pplugin_update_enable_button(&_vpp_wrapper.p);
}

void vpplugin_panel(void)
{
  pplugin_panel(&_vpp_wrapper.p);
}

void vpplugin_parse_and_store_post(const char *post)
{
  pplugin_parse_and_store_post(&_vpp_wrapper.p, post);
}

void vpplugin_rewire_from_posts_window(void)
{
  pplugin_rewire_from_posts_window(&_vpp_wrapper.p);
}

void vpplugin_rewire_posts(void)
{
  pplugin_rewire_posts(&_vpp_wrapper.p);
}

int vpplugin_is_post_selected(void)
{
  return pplugin_is_post_selected(&_vpp_wrapper.p);
}

void vpplugin_reparent(void)
{
  pplugin_reparent(&_vpp_wrapper.p);
}

void applugin_end(void)
{
  pplugin_end(&_app_wrapper.p);
}

int applugin_is_visible(void)
{
  return pplugin_is_visible(&_app_wrapper.p);
}

int applugin_is_running(void)
{
  return pplugin_is_running(&_app_wrapper.p);
}

void applugin_toggle_visibility(xitk_widget_t *w, void *data)
{
  pplugin_toggle_visibility(&_app_wrapper.p, w, data);
}

void applugin_raise_window(void)
{
  pplugin_raise_window(&_app_wrapper.p);
}

void applugin_update_enable_button(void)
{
  pplugin_update_enable_button(&_app_wrapper.p);
}

void applugin_panel(void)
{
  pplugin_panel(&_app_wrapper.p);
}

void applugin_parse_and_store_post(const char *post)
{
  pplugin_parse_and_store_post(&_app_wrapper.p, post);
}

void applugin_rewire_from_posts_window(void)
{
  pplugin_rewire_from_posts_window(&_app_wrapper.p);
}

void applugin_rewire_posts(void)
{
  pplugin_rewire_posts(&_app_wrapper.p);
}

int applugin_is_post_selected(void)
{
  return pplugin_is_post_selected(&_app_wrapper.p);
}

void applugin_reparent(void)
{
  pplugin_reparent(&_app_wrapper.p);
}
