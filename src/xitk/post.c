/* 
 * Copyright (C) 2000-2003 the xine project
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "common.h"

extern gGui_t    *gGui;

#undef TRACE_REWIRE

#define DEFAULT_DEINTERLACER "tvtime:method=Linear,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

#define WINDOW_WIDTH        500
#define WINDOW_HEIGHT       500

#define FRAME_WIDTH         (WINDOW_WIDTH - 40)
#define FRAME_HEIGHT        80

#define HELP_WINDOW_WIDTH   400
#define HELP_WINDOW_HEIGHT  300

#define MAX_DISPLAY_FILTERS 5

#define MAX_DISP_ENTRIES    11
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
 
static char                  *br_fontname      = "-misc-fixed-medium-r-normal-*-10-*-*-*-*-*-*-*";
static char                  *btnfontname      = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";
static char                  *fontname         = "-*-helvetica-medium-r-*-*-10-*-*-*-*-*-*-*";
static char                  *boldfontname     = "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";

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
  xitk_register_key_t         help_widget_key;
  int                         help_running;
  xitk_widget_t              *help_browser;
} _pplugin_t;

static _pplugin_t    *pplugin = NULL;
static char         **post_audio_plugins;

static pthread_t      rewire_thread;

/* Some functions prototype */
static void _pplugin_unwire(void);
static post_element_t **pplugin_parse_and_load(const char *pchain, int *post_elements_num);
static void _pplugin_rewire(void);
void _pplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num);
static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num);
static void _pplugin_save_chain(void);

static void *post_rewire_thread(void *data) {
  void (*rewire)(void) = data;
  
  pthread_detach(pthread_self());
  rewire();
  pthread_exit(NULL);
  return NULL;
}

static void post_deinterlace_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  post_element_t **posts = NULL;
  int              num, i;
  
  gGui->deinterlace_plugin = cfg->str_value;
  
  if(gGui->deinterlace_enable)
    _pplugin_unwire();
  
  for(i = 0; i < gGui->deinterlace_elements_num; i++) {
    xine_post_dispose(gGui->xine, gGui->deinterlace_elements[i]->post);
    free(gGui->deinterlace_elements[i]->name);
    free(gGui->deinterlace_elements[i]);
  }
  
  SAFE_FREE(gGui->deinterlace_elements);
  gGui->deinterlace_elements_num = 0;
  
  if((posts = pplugin_parse_and_load(gGui->deinterlace_plugin, &num))) {
    gGui->deinterlace_elements     = posts;
    gGui->deinterlace_elements_num = num;
  }

  if(gGui->deinterlace_enable)
    _pplugin_rewire();
}

static void post_audio_plugin_cb(void *data, xine_cfg_entry_t *cfg) {
  int err;
  
  gGui->visual_anim.post_plugin_num = cfg->num_value;
  
  if ((err = pthread_create(&rewire_thread,
			    NULL, post_rewire_thread, (void *)post_rewire_visual_anim)) != 0) {
    printf(_("%s(): can't create new thread (%s)\n"), __XINE_FUNCTION__, strerror(err));
    abort();
  }
}

const char **post_get_audio_plugins_names(void) {
  return (const char **) post_audio_plugins;
}

void post_init(void) {

  gGui->visual_anim.post_output = NULL;
  gGui->visual_anim.post_plugin_num = -1;
  gGui->visual_anim.post_changed = 0;

  if(gGui->ao_port) {
    const char *const *pol = xine_list_post_plugins_typed(gGui->xine, 
							  XINE_POST_TYPE_AUDIO_VISUALIZATION);
    
    if(pol) {
      int  num_plug = 0;
      
      while(pol[num_plug]) {
	
	if(!num_plug)
	  post_audio_plugins = (char **) xine_xmalloc(sizeof(char *) * 2);
	else
	  post_audio_plugins = (char **) realloc(post_audio_plugins, 
						 sizeof(char *) * (num_plug + 2));
	
	post_audio_plugins[num_plug]     = strdup(pol[num_plug]);
	post_audio_plugins[num_plug + 1] = NULL;
	num_plug++;
      }
      
      if(num_plug) {
	
	gGui->visual_anim.post_plugin_num = 
	  xine_config_register_enum(gGui->xine, "gui.post_audio_plugin", 
				    0, post_audio_plugins,
				    _("Audio visualization plugin"),
				    _("Post audio plugin to used when playing streams without video"),
				    CONFIG_LEVEL_BEG,
				    post_audio_plugin_cb,
				    CONFIG_NO_DATA);
	
	gGui->visual_anim.post_output = 
	  xine_post_init(gGui->xine,
			 post_audio_plugins[gGui->visual_anim.post_plugin_num], 0,
			 &gGui->ao_port, &gGui->vo_port);
	
      }
    }
  }
}

void post_rewire_visual_anim(void) {

  if(gGui->visual_anim.post_output) {
    xine_post_out_t *audio_source;
    
    audio_source = xine_get_audio_source(gGui->stream);
    (void) xine_post_wire_audio_port(audio_source, gGui->ao_port);
    
    xine_post_dispose(gGui->xine, gGui->visual_anim.post_output);
  }
  
  gGui->visual_anim.post_output = 
    xine_post_init(gGui->xine,
		   post_audio_plugins[gGui->visual_anim.post_plugin_num], 0,
		   &gGui->ao_port, &gGui->vo_port);

  if(gGui->visual_anim.post_output && 
     (gGui->visual_anim.enabled == 1) && (gGui->visual_anim.running == 1)) {

    (void) post_rewire_audio_post_to_stream(gGui->stream);

  }
}

int post_rewire_audio_port_to_stream(xine_stream_t *stream) {
  xine_post_out_t * audio_source;
  
  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gGui->ao_port);
}

int post_rewire_audio_post_to_stream(xine_stream_t *stream) {
  xine_post_out_t * audio_source;
  
  audio_source = xine_get_audio_source(stream);
  return xine_post_wire_audio_port(audio_source, gGui->visual_anim.post_output->audio_input[0]);
}

/* ================================================================ */

static void _pplugin_unwire(void) {
  xine_post_out_t  *vo_source;
  int               paused = 0;
  
  vo_source = xine_get_video_source(gGui->stream);

  if((paused = (xine_get_param(gGui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)))
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
  
  (void) xine_post_wire_video_port(vo_source, gGui->vo_port);
  
  if(paused)
    xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
  
  /* Waiting a small chunk of time helps to avoid crashing */
  xine_usec_sleep(500000);
  

}

static void _pplugin_rewire(void) {

  static post_element_t **post_elements;
  int post_elements_num;

  _pplugin_save_chain();

  post_elements = _pplugin_join_deinterlace_and_post_elements(&post_elements_num);

  if( post_elements ) {
    _pplugin_rewire_from_post_elements(post_elements, post_elements_num);

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

static int _pplugin_get_object_offset(post_object_t *pobj) {
  int i;
  
  for(i = 0; i < pplugin->object_num; i++) {
    if(pplugin->post_objects[i] == pobj)
      return i;
  }
  
  return 0;
}

static int _pplugin_is_first_filter(post_object_t *pobj) {
  if(pplugin)
    return (pobj == *pplugin->post_objects);
  
  return 0;
}

static int _pplugin_is_last_filter(post_object_t *pobj) {
  
  if(pplugin) {
    post_object_t **po = pplugin->post_objects;
    
    while(*po && (*po != pobj))
      *po++;
    
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
  }
}

static void _pplugin_show_obj(post_object_t *pobj) {

  if(pobj) {

    if(pobj->frame)
      xitk_set_widget_pos(pobj->frame, 10, pobj->y - 5);
    
    if(pobj->plugins)
      xitk_set_widget_pos(pobj->plugins, pobj->x + 30, pobj->y + 2);
    
    if(pobj->properties)
      xitk_set_widget_pos(pobj->properties, pobj->x + 200, pobj->y + 2);
    
    if(pobj->comment)
      xitk_set_widget_pos(pobj->comment, pobj->x + 30, (pobj->y + 27));
    
    if(pobj->value)
      xitk_set_widget_pos(pobj->value, pobj->x + 30, pobj->y + 50);

    if(pobj->up)
      xitk_set_widget_pos(pobj->up, pobj->x, pobj->y);

    if(pobj->down)
      xitk_set_widget_pos(pobj->down, pobj->x, pobj->y + (FRAME_HEIGHT - 26));
    
    ENABLE_ME(pobj->frame);
    ENABLE_ME(pobj->plugins);
    ENABLE_ME(pobj->properties);
    ENABLE_ME(pobj->value);
    ENABLE_ME(pobj->comment);

    if((!_pplugin_is_first_filter(pobj)) && (xitk_combo_get_current_selected(pobj->plugins)))
      ENABLE_ME(pobj->up);
    
    if((!_pplugin_is_last_filter(pobj) && (xitk_combo_get_current_selected(pobj->plugins))) && 
       (_pplugin_is_last_filter(pobj) 
	|| (!_pplugin_is_last_filter(pobj) 
	    && pplugin->post_objects[_pplugin_get_object_offset(pobj) + 1] 
	    && xitk_combo_get_current_selected(pplugin->post_objects[_pplugin_get_object_offset(pobj) + 1]->plugins))))
      ENABLE_ME(pobj->down);

  }
}

static void _pplugin_paint_widgets(void) {
  if(pplugin) {
    int   i, x, y;
    int   last;
    int   slidmax, slidpos;
    
    last = pplugin->object_num <= (pplugin->first_displayed + MAX_DISPLAY_FILTERS)
      ? pplugin->object_num : (pplugin->first_displayed + MAX_DISPLAY_FILTERS);
    
    for(i = 0; i < pplugin->object_num; i++)
      _pplugin_hide_obj(pplugin->post_objects[i]);
    
    x = 15;
    y = -45;
    
    for(i = pplugin->first_displayed; i < last; i++) {
      y += 20 + 5 + 20 + 5 + 23 + 10;
      pplugin->post_objects[i]->x = x;
      pplugin->post_objects[i]->y = y;
      _pplugin_show_obj(pplugin->post_objects[i]);
    }
    
    if(pplugin->object_num > MAX_DISPLAY_FILTERS) {
      slidmax = pplugin->object_num - MAX_DISPLAY_FILTERS;
      slidpos = slidmax - pplugin->first_displayed;
      xitk_enable_and_show_widget(pplugin->slider);
    }
    else {
      slidmax = 1;
      slidpos = slidmax;
      
      if(!pplugin->first_displayed)
	xitk_disable_and_hide_widget(pplugin->slider);
    }
    
    xitk_slider_set_max(pplugin->slider, slidmax);
    xitk_slider_set_pos(pplugin->slider, slidpos);
  }
}

static void _pplugin_send_expose(void) {
  if(pplugin) {
    XLockDisplay(gGui->display);
    XClearWindow(gGui->display, (XITK_WIDGET_LIST_WINDOW(pplugin->widget_list)));
    XUnlockDisplay(gGui->display);
  }
}

static void _pplugin_destroy_widget(xitk_widget_t *w) {
  xitk_widget_t *cw;

  cw = (xitk_widget_t *) xitk_list_first_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)));
  while(cw) {
    
    if(cw == w) {
      xitk_hide_widget(cw);
      xitk_destroy_widget(cw);
      cw = NULL;
      xitk_list_delete_current((XITK_WIDGET_LIST_LIST(pplugin->widget_list)));
      return;
    }
    
    cw = (xitk_widget_t *) xitk_list_next_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)));
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
    
    snprintf((char *)(pobj->param_data + pobj->param->offset), maxlen, "%s", text);
    _pplugin_update_parameter(pobj);
    xitk_inputtext_change_text(pobj->value, (char *)(pobj->param_data + pobj->param->offset));
  }
  else
    xine_error(_("parameter type POST_PARAM_TYPE_STRING) not supported yet.\n"));

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

static void _pplugin_add_parameter_widget(post_object_t *pobj) {
  
  if(pobj) {
    xitk_label_widget_t   lb;
    int                   w, width;
    char                  buffer[2048];
    xitk_font_t          *fs;

    memset(&buffer, 0, sizeof(buffer));
    snprintf(buffer, 2047, "%s:", (pobj->param->description) 
	     ? pobj->param->description : _("No description available"));

    fs = xitk_font_load_font(gGui->display, boldfontname);
    xitk_font_set_font(fs, (XITK_WIDGET_LIST_GC(pplugin->widget_list)));
    width = xitk_font_get_string_length(fs, buffer);
    xitk_font_unload_font(fs);
    
    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.window              = xitk_window_get_window(pplugin->xwin);
    lb.gc                  = (XITK_WIDGET_LIST_GC(pplugin->widget_list));
    lb.skin_element_name   = NULL;
    lb.label               = buffer;
    lb.callback            = NULL;
    lb.userdata            = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
     (pobj->comment = xitk_noskin_label_create(pplugin->widget_list, &lb,
					       pobj->x, (pobj->y + 25), 
					       width + 10, 20, boldfontname)));

    w = xitk_get_widget_width(pobj->comment);
    w += 5;

    switch(pobj->param->type) {
    case POST_PARAM_TYPE_INT:
      {
	
	if(pobj->param->enum_values) {
	  xitk_combo_widget_t         cmb;

	  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
	  cmb.skin_element_name = NULL;
	  cmb.layer_above       = (is_layer_above());
	  cmb.parent_wlist      = pplugin->widget_list;
	  cmb.entries           = pobj->param->enum_values;
	  cmb.parent_wkey       = &pplugin->widget_key;
	  cmb.callback          = _pplugin_set_param_int;
	  cmb.userdata          = pobj;
	  xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
	   (pobj->value = xitk_noskin_combo_create(pplugin->widget_list, 
						   &cmb, 
						   pobj->x + w, pobj->y + 25, 
						   180, NULL, NULL)));

	  xitk_combo_set_select(pobj->value, *(int *)(pobj->param_data + pobj->param->offset));
	}
	else {
	  xitk_intbox_widget_t      ib;
	  
	  XITK_WIDGET_INIT(&ib, gGui->imlib_data);
	  ib.skin_element_name = NULL;
	  ib.value             = *(int *)(pobj->param_data + pobj->param->offset);
	  ib.step              = 1;
	  ib.parent_wlist      = pplugin->widget_list;
	  ib.callback          = _pplugin_set_param_int;
	  ib.userdata          = pobj;
	  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
	   (pobj->value = xitk_noskin_intbox_create(pplugin->widget_list, &ib, 
						    pobj->x + w, pobj->y + 25, 
						    50, 20, NULL, NULL, NULL)));
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
	ib.parent_wlist      = pplugin->widget_list;
	ib.callback          = _pplugin_set_param_double;
	ib.userdata          = pobj;
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
	 (pobj->value = xitk_noskin_doublebox_create(pplugin->widget_list, &ib, 
						     pobj->x + w, pobj->y + 25, 
						     100, 20, NULL, NULL, NULL)));
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
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
	 (pobj->value = xitk_noskin_inputtext_create(pplugin->widget_list, &inp, 
						     pobj->x + w, pobj->y + 25, 
						     180, 20,
						     "Black", "Black", fontname)));
      }
      break;
      
    case POST_PARAM_TYPE_STRINGLIST:
      {
	xitk_combo_widget_t         cmb;
	
	XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
	cmb.skin_element_name = NULL;
	cmb.layer_above       = (is_layer_above());
	cmb.parent_wlist      = pplugin->widget_list;
	cmb.entries           = (char **)(pobj->param_data + pobj->param->offset);
	cmb.parent_wkey       = &pplugin->widget_key;
	cmb.callback          = _pplugin_set_param_stringlist;
	cmb.userdata          = pobj;
	xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
	 (pobj->value = xitk_noskin_combo_create(pplugin->widget_list, 
						 &cmb, 
						 pobj->x + w, pobj->y + 25, 
						 180, NULL, NULL)));
	
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
	xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
	  (pobj->value = xitk_noskin_checkbox_create(pplugin->widget_list, &cb,
						     pobj->x + w, pobj->y + 25, 15, 15)));
	xitk_checkbox_set_state(pobj->value, 
				(*(int *)(pobj->param_data + pobj->param->offset)));  
      }
      break;
    }
  }
}

static void _pplugin_change_parameter(xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;

  if(pobj) {

    if(pobj->value)
      _pplugin_destroy_widget(pobj->value);
    
    if(pobj->comment)
      _pplugin_destroy_widget(pobj->comment);

    if(pobj->descr) {
      pobj->param    = pobj->descr->parameter;
      pobj->param    += select;
      pobj->readonly = pobj->param->readonly;
      _pplugin_add_parameter_widget(pobj);
    }

    _pplugin_paint_widgets();
  }
}

static void _pplugin_get_plugins(void) {
  const char *const *pol = xine_list_post_plugins_typed(gGui->xine, XINE_POST_TYPE_VIDEO_FILTER);
  
  if(pol) {
    int  i = 0;

    pplugin->plugin_names    = (char **) xine_xmalloc(sizeof(char *) * 2);
    pplugin->plugin_names[i] = strdup(_("No Filter"));
    while(pol[i]) {
      pplugin->plugin_names = (char **) 
	realloc(pplugin->plugin_names, sizeof(char *) * (i + 1 + 2));
      
      pplugin->plugin_names[i + 1] = strdup(pol[i]);
      i++;
    }
    pplugin->plugin_names[i + 1] = NULL;
  }
}

static void _pplugin_destroy_only_obj(post_object_t *pobj) {
  if(pobj) {

    if(pobj->properties) {
      
      _pplugin_destroy_widget(pobj->properties);
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
      _pplugin_destroy_widget(pobj->comment);
      pobj->comment = NULL;
    }
    
    if(pobj->value) {
      _pplugin_destroy_widget(pobj->value);
      pobj->value = NULL;
    }
  }
}

static void _pplugin_destroy_obj(post_object_t *pobj) {
  if(pobj) {
    _pplugin_destroy_only_obj(pobj);
    
    if(pobj->post) {
      xine_post_dispose(gGui->xine, pobj->post);
      pobj->post = NULL;
    }
  }
}

static void _pplugin_destroy_base_obj(post_object_t *pobj) {
  int i = 0;

  while(pplugin->post_objects && (pplugin->post_objects[i] != pobj)) 
    i++;
  
  if(pplugin->post_objects[i]) {
    _pplugin_destroy_widget(pplugin->post_objects[i]->plugins);
    _pplugin_destroy_widget(pplugin->post_objects[i]->frame);
    _pplugin_destroy_widget(pplugin->post_objects[i]->up);
    _pplugin_destroy_widget(pplugin->post_objects[i]->down);
    
    free(pplugin->post_objects[i]);
    pplugin->post_objects[i] = NULL;
    
    pplugin->object_num--;
    
    pplugin->x = 5;
    pplugin->y -= 20 + 5 + 20 + 5 + 23 + 10;
  }
}

static void _pplugin_kill_filters_from(post_object_t *pobj) {
  int             num = pplugin->object_num;
  int             i = 0;
  
  while(pplugin->post_objects && (pplugin->post_objects[i] != pobj)) 
    i++;
  
  _pplugin_destroy_obj(pplugin->post_objects[i]);
  
  i++;
  
  if(pplugin->post_objects[i]) {

    while(pplugin->post_objects[i]) {

      _pplugin_destroy_obj(pplugin->post_objects[i]);
      
      _pplugin_destroy_base_obj(pplugin->post_objects[i]);

      i++;
    }
  }

  _pplugin_send_expose();

  if(num != pplugin->object_num) {
    pplugin->post_objects = (post_object_t **) 
      realloc(pplugin->post_objects, sizeof(post_object_t *) * (pplugin->object_num + 2));
    pplugin->post_objects[pplugin->object_num + 1] = NULL;
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

static void _pplugin_close_help(xitk_widget_t *w, void *data) {

  pplugin->help_running = 0;

  xitk_unregister_event_handler(&pplugin->help_widget_key);

  xitk_destroy_widgets(pplugin->help_widget_list);
  xitk_window_destroy_window(gGui->imlib_data, pplugin->helpwin);

  pplugin->helpwin = NULL;
  xitk_list_free((XITK_WIDGET_LIST_LIST(pplugin->help_widget_list)));
    
  XLockDisplay(gGui->display);
  XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(pplugin->help_widget_list)));
  XUnlockDisplay(gGui->display);
    
  free(pplugin->help_widget_list);
}

static int __line_wrap(char *s, int pos, int line_size)
{
  int word_size = 0;

  while( *s && *s != ' ' && *s != '\n' ) {
    s++;
    word_size++;
  }

  if( word_size >= line_size )
    return pos > line_size; 

  return word_size + pos > line_size;
}

static void _pplugin_show_help(xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  GC                          gc;
  xitk_pixmap_t              *bg;
  int                         x, y, width, height;
  xitk_labelbutton_widget_t   lb;
  xitk_browser_widget_t       br;

  x = y = 100;
  pplugin->helpwin = xitk_window_create_dialog_window(gGui->imlib_data, 
						 _("Plugin Help"), x, y,
						 HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT);
  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(pplugin->helpwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  pplugin->help_widget_list = xitk_widget_list_new();
  xitk_widget_list_set(pplugin->help_widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(pplugin->help_widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(pplugin->helpwin)));
  xitk_widget_list_set(pplugin->help_widget_list, WIDGET_LIST_GC, gc);
  
  xitk_window_get_window_size(pplugin->helpwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);

  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(pplugin->helpwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay (gGui->display);

  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Ok");
  lb.align             = ALIGN_CENTER;
  lb.callback          = _pplugin_close_help;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->help_widget_list)), 
   (w = xitk_noskin_labelbutton_create(pplugin->help_widget_list, 
				       &lb, 150, 270, 100, 23,
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
  br.parent_wlist                  = pplugin->help_widget_list;
  br.userdata                      = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->help_widget_list)), 
   (pplugin->help_browser = 
    xitk_noskin_browser_create(pplugin->help_widget_list, &br,
			       (XITK_WIDGET_LIST_GC(pplugin->help_widget_list)), 15, 25, 
			       HELP_WINDOW_WIDTH - (30 + 16), 20,
			       16, br_fontname)));

  {
    char  *p, **hbuf = NULL;
    int    lines = 0, i;
    
    p = pobj->api->get_help();
    
    do {
      if(!lines)
	hbuf = (char **) xine_xmalloc(sizeof(char *) * 2);
      else
	hbuf  = (char **) realloc(hbuf, sizeof(char *) * (lines + 2));

      hbuf[lines]    = malloc(BROWSER_LINE_WIDTH+1);
      hbuf[lines+1]  = NULL;

      for(i = 0; !__line_wrap(p,i,BROWSER_LINE_WIDTH) && *p && *p != '\n'; i++)
        hbuf[lines][i] = *p++;

      hbuf[lines][i] = '\0';
      if( *p && (*p == '\n' || *p == ' ') )
        p++;

      lines++;
    } while( *p );
    
    if(lines) {
      xitk_browser_update_list(pplugin->help_browser, (const char **)hbuf, 
			       lines, 0);
    }
  }


  xitk_enable_and_show_widget(pplugin->help_browser);
  xitk_browser_set_alignment(pplugin->help_browser, ALIGN_LEFT);

  xitk_window_change_background(gGui->imlib_data, pplugin->helpwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);

  pplugin->help_widget_key = xitk_register_event_handler("pplugin_help", 
						    (xitk_window_get_window(pplugin->helpwin)),
						    NULL,
						    NULL,
						    NULL,
						    pplugin->help_widget_list,
						    NULL);

  pplugin->help_running = 1;
  
  raise_window(xitk_window_get_window(pplugin->helpwin), 1, pplugin->help_running);
  
  try_to_set_input_focus(xitk_window_get_window(pplugin->helpwin));
}

static void _pplugin_retrieve_parameters(post_object_t *pobj) {
  xitk_combo_widget_t         cmb;
  xitk_labelbutton_widget_t   lb;
  
  if(__pplugin_retrieve_parameters(pobj)) {
    
    XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
    cmb.skin_element_name = NULL;
    cmb.layer_above       = (is_layer_above());
    cmb.parent_wlist      = pplugin->widget_list;
    cmb.entries           = pobj->properties_names;
    cmb.parent_wkey       = &pplugin->widget_key;
    cmb.callback          = _pplugin_change_parameter;
    cmb.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
     (pobj->properties = xitk_noskin_combo_create(pplugin->widget_list, 
						  &cmb, 
						  pobj->x + 180, pobj->y, 
						  150, NULL, NULL)));
    
    xitk_combo_set_select(pobj->properties, 0);
    xitk_combo_callback_exec(pobj->properties);


    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.button_type       = CLICK_BUTTON;
    lb.label             = _("Help");
    lb.align             = ALIGN_CENTER;
    lb.callback          = _pplugin_show_help; 
    lb.state_callback    = NULL;
    lb.userdata          = pobj;
    lb.skin_element_name = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
     (pobj->help = xitk_noskin_labelbutton_create(pplugin->widget_list, 
						  &lb, pobj->x + 390, pobj->y, 50, 20,
						  "Black", "Black", "White", btnfontname)));
    xitk_show_widget(pobj->help);
    xitk_enable_widget(pobj->help);
  }
  else {
    xitk_label_widget_t   lb;
    
    XITK_WIDGET_INIT(&lb, gGui->imlib_data);
    lb.window              = xitk_window_get_window(pplugin->xwin);
    lb.gc                  = (XITK_WIDGET_LIST_GC(pplugin->widget_list));
    lb.skin_element_name   = NULL;
    lb.label               = _("There is no parameter available for this plugin.");
    lb.callback            = NULL;
    lb.userdata            = NULL;
    xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
     (pobj->comment = xitk_noskin_label_create(pplugin->widget_list, &lb,
					       pobj->x, (pobj->y + 25), 
					       FRAME_WIDTH - 50, 20, boldfontname)));
    
    pobj->properties = NULL;
  }
}

static void _pplugin_select_filter(xitk_widget_t *w, void *data, int select) {
  post_object_t *pobj = (post_object_t *) data;
  
  _pplugin_unwire();

  if(!select) {
    _pplugin_kill_filters_from(pobj);
    
    if(_pplugin_is_first_filter(pobj)) {
      if(xitk_is_widget_enabled(pplugin->new_filter))
	xitk_disable_widget(pplugin->new_filter);
    }
    else {
      _pplugin_destroy_base_obj(pobj);
      
      pplugin->post_objects = (post_object_t **) 
	realloc(pplugin->post_objects, sizeof(post_object_t *) * (pplugin->object_num + 2));
      pplugin->post_objects[pplugin->object_num + 1] = NULL;
      
      if(!xitk_is_widget_enabled(pplugin->new_filter))
	xitk_enable_widget(pplugin->new_filter);
     
    }

    if(pplugin->object_num <= MAX_DISPLAY_FILTERS)
      pplugin->first_displayed = 0;
    else if((pplugin->object_num - pplugin->first_displayed) < MAX_DISPLAY_FILTERS)
      pplugin->first_displayed = pplugin->object_num - MAX_DISPLAY_FILTERS;

  }
  else {
    _pplugin_destroy_obj(pobj);

    pobj->post = xine_post_init(gGui->xine, 
				xitk_combo_get_current_entry_selected(w),
				0, &gGui->ao_port, &gGui->vo_port);
    
    _pplugin_retrieve_parameters(pobj);

    if(!xitk_is_widget_enabled(pplugin->new_filter))
      xitk_enable_widget(pplugin->new_filter);

  }

  _pplugin_rewire();
  _pplugin_paint_widgets();
}

static void _pplugin_move_up(xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_object_t *pobj_tmp;
  post_object_t **ppobj = pplugin->post_objects;

  _pplugin_unwire();

  while(*ppobj != pobj)
    *ppobj++;
  
  pobj_tmp = *--ppobj;
  *ppobj   = pobj;
  *++ppobj = pobj_tmp;
  
  _pplugin_rewire();
  _pplugin_paint_widgets();
}

static void _pplugin_move_down(xitk_widget_t *w, void *data) {
  post_object_t *pobj = (post_object_t *) data;
  post_object_t *pobj_tmp;
  post_object_t **ppobj = pplugin->post_objects;

  _pplugin_unwire();

  while(*ppobj != pobj)
    *ppobj++;
  
  pobj_tmp = *(ppobj + 1);
  *ppobj   = pobj_tmp;
  *++ppobj = pobj;
  
  _pplugin_rewire();
  _pplugin_paint_widgets();
}

static void _pplugin_create_filter_object(void) {
  xitk_combo_widget_t   cmb;
  post_object_t        *pobj;
  xitk_image_widget_t   im;
  xitk_image_t         *image;
  xitk_button_widget_t  b;
  
  pplugin->x = 15;
  pplugin->y += 20 + 5 + 20 + 5 + 23 + 10;
  
  xitk_disable_widget(pplugin->new_filter);

  if(!pplugin->object_num)
    pplugin->post_objects = (post_object_t **) xine_xmalloc(sizeof(post_object_t *) * 2);
  else
    pplugin->post_objects = (post_object_t **) 
      realloc(pplugin->post_objects, sizeof(post_object_t *) * (pplugin->object_num + 2));
  
  pplugin->post_objects[pplugin->object_num + 1] = NULL;
  
  pobj = pplugin->post_objects[pplugin->object_num] = (post_object_t *) xine_xmalloc(sizeof(post_object_t));
  pplugin->post_objects[pplugin->object_num]->x     = pplugin->x;
  pplugin->post_objects[pplugin->object_num]->y     = pplugin->y;
  pplugin->object_num++;

  image = xitk_image_create_image(gGui->imlib_data, FRAME_WIDTH, FRAME_HEIGHT);

  XLockDisplay(gGui->display);
  XSetForeground(gGui->display, (XITK_WIDGET_LIST_GC(pplugin->widget_list)),
		 xitk_get_pixel_color_gray(gGui->imlib_data));
  XFillRectangle(gGui->display, image->image->pixmap,
		 (XITK_WIDGET_LIST_GC(pplugin->widget_list)),
		 0, 0, image->width, image->height);
  XUnlockDisplay(gGui->display);

  /* Some decorations */
  draw_outter_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    0, 0, FRAME_WIDTH, FRAME_HEIGHT);
  draw_outter_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    26, 5, 1, FRAME_HEIGHT - 10);
  draw_outter_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    27, 28, FRAME_WIDTH - 40, 1);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    5, 5, 16, 16);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    5, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    10, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    15, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    20, 24, 1, FRAME_HEIGHT - 48);
  draw_inner_frame(gGui->imlib_data, image->image, NULL, boldfontname,
		    5, FRAME_HEIGHT - 16 - 5, 16, 16);

  XITK_WIDGET_INIT(&im, gGui->imlib_data);
  im.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
   (pobj->frame = xitk_noskin_image_create(pplugin->widget_list, &im, image, 10, pobj->y - 5)));
  DISABLE_ME(pobj->frame);
  
  XITK_WIDGET_INIT(&cmb, gGui->imlib_data);
  cmb.skin_element_name = NULL;
  cmb.layer_above       = (is_layer_above());
  cmb.parent_wlist      = pplugin->widget_list;
  cmb.entries           = pplugin->plugin_names;
  cmb.parent_wkey       = &pplugin->widget_key;
  cmb.callback          = _pplugin_select_filter;
  cmb.userdata          = pobj;
  xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
   (pobj->plugins = xitk_noskin_combo_create(pplugin->widget_list, 
					     &cmb, pobj->x + 20, pobj->y, 150, NULL, NULL)));
  DISABLE_ME(pobj->plugins);
  
  xitk_combo_set_select(pobj->plugins, 0);

  {
    xitk_image_t *bimage;

    XITK_WIDGET_INIT(&b, gGui->imlib_data);
    b.skin_element_name = NULL;
    b.callback          = _pplugin_move_up;
    b.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
     (pobj->up = xitk_noskin_button_create(pplugin->widget_list, &b, 
					   pobj->x, pobj->y, 17, 17)));
    DISABLE_ME(pobj->up);

    if((bimage = xitk_get_widget_foreground_skin(pobj->up)) != NULL)
      draw_arrow_up(gGui->imlib_data, bimage);
    
    b.skin_element_name = NULL;
    b.callback          = _pplugin_move_down;
    b.userdata          = pobj;
    xitk_list_append_content(XITK_WIDGET_LIST_LIST(pplugin->widget_list), 
     (pobj->down = xitk_noskin_button_create(pplugin->widget_list, &b, 
					     pobj->x, pobj->y + (FRAME_HEIGHT - 26), 17, 17)));
    DISABLE_ME(pobj->down);

    if((bimage = xitk_get_widget_foreground_skin(pobj->down)) != NULL)
      draw_arrow_down(gGui->imlib_data, bimage);

  }
}

static void _pplugin_add_filter(xitk_widget_t *w, void *data) {
  _pplugin_create_filter_object();

  if(pplugin->object_num > MAX_DISPLAY_FILTERS)
    pplugin->first_displayed = (pplugin->object_num - MAX_DISPLAY_FILTERS);

  _pplugin_paint_widgets();
}

static void _pplugin_rebuild_filters(void) {
  post_element_t  **pelem = gGui->post_elements;

  while(pelem && *pelem) {
    int i = 0;
    
    _pplugin_create_filter_object();
    pplugin->post_objects[pplugin->object_num - 1]->post = (*pelem)->post;
    
    while(pplugin->plugin_names[i] && strcasecmp(pplugin->plugin_names[i], (*pelem)->name))
      i++;
    
    if(pplugin->plugin_names[i]) {
      xitk_combo_set_select(pplugin->post_objects[pplugin->object_num - 1]->plugins, i);
      
      _pplugin_retrieve_parameters(pplugin->post_objects[pplugin->object_num - 1]);
    }
    
    *pelem++;
  }
  
  xitk_enable_widget(pplugin->new_filter);

  _pplugin_paint_widgets();
}

static post_element_t **_pplugin_join_deinterlace_and_post_elements(int *post_elements_num) {
  post_element_t **post_elements;
  int i, j;

  *post_elements_num = 0;
  if( gGui->post_enable )
    *post_elements_num += gGui->post_elements_num;
  if( gGui->deinterlace_enable )
    *post_elements_num += gGui->deinterlace_elements_num;

  if( *post_elements_num == 0 )
    return NULL;

  post_elements = (post_element_t **) 
    xine_xmalloc(sizeof(post_element_t *) * (*post_elements_num));

  for( i = 0; gGui->deinterlace_enable && i < gGui->deinterlace_elements_num; i++ ) {
    post_elements[i] = gGui->deinterlace_elements[i];
  }

  for( j = 0; gGui->post_enable && j < gGui->post_elements_num; j++ ) {
    post_elements[i+j] = gGui->post_elements[j];
  }
  
  return post_elements;
}


static void _pplugin_save_chain(void) {
  
  if(pplugin) {
    int i = 0;
    int post_num = pplugin->object_num;
    
    if(!xitk_combo_get_current_selected(pplugin->post_objects[post_num - 1]->plugins))
      post_num--;
    
    if(post_num) {
      if(!gGui->post_elements) {
	gGui->post_elements = (post_element_t **) 
	  xine_xmalloc(sizeof(post_element_t *) * (post_num + 1));
      }
      else {
	int j;
	
	for(j = 0; j < gGui->post_elements_num; j++) {
	  free(gGui->post_elements[j]->name);
	  free(gGui->post_elements[j]);
	}
	
	gGui->post_elements = (post_element_t **) 
	  realloc(gGui->post_elements, sizeof(post_element_t *) * (post_num + 1));
	
      }
	
      for(i = 0; i < post_num; i++) {
	gGui->post_elements[i] = (post_element_t *) xine_xmalloc(sizeof(post_element_t));
	gGui->post_elements[i]->post = pplugin->post_objects[i]->post;
	gGui->post_elements[i]->name = 
	  strdup(xitk_combo_get_current_entry_selected(pplugin->post_objects[i]->plugins));
      }

      gGui->post_elements[post_num] = NULL;
      gGui->post_elements_num       = post_num;
    }
    else {
      if(gGui->post_elements_num) {
	int j;
	
	for(j = 0; j < gGui->post_elements_num; j++) {
	  free(gGui->post_elements[j]->name);
	  free(gGui->post_elements[j]);
	}
	
	free(gGui->post_elements[j]);
	free(gGui->post_elements);

	gGui->post_elements_num = 0;
	gGui->post_elements     = NULL;
      }
    }
  }
}

static void _pplugin_nextprev(xitk_widget_t *w, void *data, int pos) {
  int rpos = (xitk_slider_get_max(pplugin->slider)) - pos;

  if(rpos != pplugin->first_displayed) {
    pplugin->first_displayed = rpos;
    _pplugin_paint_widgets();
  }
}

static void pplugin_exit(xitk_widget_t *w, void *data) {
  
  if(pplugin) {
    window_info_t wi;
    int           i;
    
    if( pplugin->help_running )
     _pplugin_close_help(NULL,NULL);

    _pplugin_save_chain();

    pplugin->running = 0;
    pplugin->visible = 0;
    
    if((xitk_get_window_info(pplugin->widget_key, &wi))) {
      config_update_num ("gui.pplugin_x", wi.x);
      config_update_num ("gui.pplugin_y", wi.y);
      WINDOW_INFO_ZERO(&wi);
    }
    
    i = pplugin->object_num - 1;
    if(i > 0) {

      while(i > 0) {
	_pplugin_destroy_only_obj(pplugin->post_objects[i]);
	_pplugin_destroy_base_obj(pplugin->post_objects[i]);
	free(pplugin->post_objects[i]);
	i--;
      }
      
      _pplugin_destroy_only_obj(pplugin->post_objects[0]);
      _pplugin_destroy_base_obj(pplugin->post_objects[0]);
      free(pplugin->post_objects);
    }
    
    for(i = 0; pplugin->plugin_names[i]; i++)
      free(pplugin->plugin_names[i]);
    
    free(pplugin->plugin_names);
    
    xitk_unregister_event_handler(&pplugin->widget_key);

    xitk_destroy_widgets(pplugin->widget_list);
    xitk_window_destroy_window(gGui->imlib_data, pplugin->xwin);

    pplugin->xwin = NULL;
    xitk_list_free((XITK_WIDGET_LIST_LIST(pplugin->widget_list)));
    
    XLockDisplay(gGui->display);
    XFreeGC(gGui->display, (XITK_WIDGET_LIST_GC(pplugin->widget_list)));
    XUnlockDisplay(gGui->display);
    
    free(pplugin->widget_list);
   
    free(pplugin);
    pplugin = NULL;
  }
}

static void _pplugin_handle_event(XEvent *event, void *data) {

  switch(event->type) {

  case ButtonPress:
    if(event->xbutton.button == Button4) {
      xitk_slider_make_step(pplugin->slider);
      xitk_slider_callback_exec(pplugin->slider);
    }
    else if(event->xbutton.button == Button5) {
      xitk_slider_make_backstep(pplugin->slider);
      xitk_slider_callback_exec(pplugin->slider);
    }
    break;

  case KeyPress: {
    XKeyEvent      mykeyevent;
    KeySym         mykey;
    char           kbuf[256];
    int            len;
    int            modifier;
    
    if(xitk_is_widget_enabled(pplugin->slider)) {
      
      mykeyevent = event->xkey;
      
      xitk_get_key_modifier(event, &modifier);
      
      XLockDisplay(gGui->display);
      len = XLookupString(&mykeyevent, kbuf, sizeof(kbuf), &mykey, NULL);
      XUnlockDisplay(gGui->display);
      
      switch(mykey) {
	
      case XK_Up:
	if((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) {
	  xitk_slider_make_step(pplugin->slider);
	  xitk_slider_callback_exec(pplugin->slider);
	}
	break;
	
      case XK_Down:
	if((modifier & 0xFFFFFFEF) == MODIFIER_NOMOD) { 
	  xitk_slider_make_backstep(pplugin->slider);
	  xitk_slider_callback_exec(pplugin->slider);
	}
	break;


      case XK_Next: {
	int pos, max = xitk_slider_get_max(pplugin->slider);
	
	pos = max - (pplugin->first_displayed + MAX_DISPLAY_FILTERS);
	xitk_slider_set_pos(pplugin->slider, (pos >= 0) ? pos : 0);
	xitk_slider_callback_exec(pplugin->slider);
      }
	break;

      case XK_Prior: {
	int pos, max = xitk_slider_get_max(pplugin->slider);
	
	pos = max - (pplugin->first_displayed - MAX_DISPLAY_FILTERS);
	xitk_slider_set_pos(pplugin->slider, (pos <= max) ? pos : max);
	xitk_slider_callback_exec(pplugin->slider);
      }
	break;
	
      }
    }
  }
    break;

  }
}

static void _pplugin_enability(xitk_widget_t *w, void *data, int state) {
  gGui->post_enable = state;
  _pplugin_unwire();
  _pplugin_rewire();
}

int pplugin_is_post_selected(void) {
  
  if(pplugin) {
    int post_num = pplugin->object_num;
    
    if(!xitk_combo_get_current_selected(pplugin->post_objects[post_num - 1]->plugins))
      post_num--;

    return (post_num > 0);
  }
  
  return gGui->post_elements_num;
}

void pplugin_rewire_from_posts_window(void) {
  if(pplugin) {
    _pplugin_unwire();
    _pplugin_rewire();
  }
}

void _pplugin_rewire_from_post_elements(post_element_t **post_elements, int post_elements_num) {
  
  if(post_elements_num) {
    xine_post_out_t   *vo_source;
    int                i = 0;
    int                paused = 0;
    
    if((paused = (xine_get_param(gGui->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)))
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_SLOW_4);
    
    for(i = (post_elements_num - 1); i >= 0; i--) {
      const char *const *outs = xine_post_list_outputs(post_elements[i]->post);
      const xine_post_out_t *vo_out = xine_post_output(post_elements[i]->post, (char *) *outs);
      if(i == (post_elements_num - 1)) {
	xine_post_wire_video_port((xine_post_out_t *) vo_out, gGui->vo_port);
      }
      else {
	const xine_post_in_t *vo_in = xine_post_input(post_elements[i + 1]->post, "video");
	int                   err;
	
	err = xine_post_wire((xine_post_out_t *) vo_out, (xine_post_in_t *) vo_in);	
      }
    }
    
    vo_source = xine_get_video_source(gGui->stream);
    xine_post_wire_video_port(vo_source, post_elements[0]->post->video_input[0]);
    
    if(paused)
      xine_set_param(gGui->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);

  }
}
void pplugin_rewire_posts(void) {
  
  _pplugin_unwire();
  _pplugin_rewire();

#if 0
  if(gGui->post_enable && !gGui->deinterlace_enable)
    _pplugin_rewire_from_post_elements(gGui->post_elements, gGui->post_elements_num);
#endif
}

void pplugin_end(void) {
  pplugin_exit(NULL, NULL);
}

int pplugin_is_visible(void) {
  
  if(pplugin) {
    if(gGui->use_root_window)
      return xitk_is_window_visible(gGui->display, xitk_window_get_window(pplugin->xwin));
    else
      return pplugin->visible;
  }
  
  return 0;
}

int pplugin_is_running(void) {
  
  if(pplugin)
    return pplugin->running;
  
  return 0;
}

void pplugin_raise_window(void) {
  if(pplugin != NULL)
    raise_window(xitk_window_get_window(pplugin->xwin), pplugin->visible, pplugin->running);
}


void pplugin_toggle_visibility(xitk_widget_t *w, void *data) {
  if(pplugin != NULL)
    toggle_window(xitk_window_get_window(pplugin->xwin), pplugin->widget_list,
		  &pplugin->visible, pplugin->running);
}

void pplugin_update_enable_button(void) {
  if(pplugin)
    xitk_labelbutton_set_state(pplugin->enable, gGui->post_enable);
}

void pplugin_panel(void) {
  GC                          gc;
  xitk_labelbutton_widget_t   lb;
  xitk_label_widget_t         lbl;
  xitk_checkbox_widget_t      cb;
  xitk_pixmap_t              *bg;
  int                         x, y, width, height;
  xitk_slider_widget_t        sl;
  xitk_widget_t              *w;

  pplugin = (_pplugin_t *) xine_xmalloc(sizeof(_pplugin_t));
  pplugin->first_displayed = 0;
  
  x = xine_config_register_num (gGui->xine, "gui.pplugin_x", 
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  y = xine_config_register_num (gGui->xine, "gui.pplugin_y",
				100,
				CONFIG_NO_DESC,
				CONFIG_NO_HELP,
				CONFIG_LEVEL_DEB,
				CONFIG_NO_CB,
				CONFIG_NO_DATA);
  
  pplugin->xwin = xitk_window_create_dialog_window(gGui->imlib_data, 
						   _("Chain Reaction"), x, y,
						   WINDOW_WIDTH, WINDOW_HEIGHT);
  
  XLockDisplay (gGui->display);
  gc = XCreateGC(gGui->display, 
		 (xitk_window_get_window(pplugin->xwin)), None, None);
  XUnlockDisplay (gGui->display);
  
  pplugin->widget_list = xitk_widget_list_new();
  xitk_widget_list_set(pplugin->widget_list, WIDGET_LIST_LIST, (xitk_list_new()));
  xitk_widget_list_set(pplugin->widget_list, 
		       WIDGET_LIST_WINDOW, (void *) (xitk_window_get_window(pplugin->xwin)));
  xitk_widget_list_set(pplugin->widget_list, WIDGET_LIST_GC, gc);
  
  XITK_WIDGET_INIT(&lb, gGui->imlib_data);
  XITK_WIDGET_INIT(&lbl, gGui->imlib_data);
  XITK_WIDGET_INIT(&cb, gGui->imlib_data);

  xitk_window_get_window_size(pplugin->xwin, &width, &height);
  bg = xitk_image_create_xitk_pixmap(gGui->imlib_data, width, height);

  XLockDisplay (gGui->display);
  XCopyArea(gGui->display, (xitk_window_get_background(pplugin->xwin)), bg->pixmap,
	    bg->gc, 0, 0, width, height, 0, 0);
  XUnlockDisplay (gGui->display);
  
  XITK_WIDGET_INIT(&sl, gGui->imlib_data);
  
  sl.min                      = 0;
  sl.max                      = 1;
  sl.step                     = 1;
  sl.skin_element_name        = NULL;
  sl.callback                 = NULL;
  sl.userdata                 = NULL;
  sl.motion_callback          = _pplugin_nextprev;
  sl.motion_userdata          = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)),
   (pplugin->slider = xitk_noskin_slider_create(pplugin->widget_list, &sl,
						(WINDOW_WIDTH - (16 + 10)), 33, 
						16, (WINDOW_HEIGHT - 88), XITK_VSLIDER)));

  y = WINDOW_HEIGHT - (23 + 15);
  x = 15;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("New Filter");
  lb.align             = ALIGN_CENTER;
  lb.callback          = _pplugin_add_filter; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
   (pplugin->new_filter = xitk_noskin_labelbutton_create(pplugin->widget_list, 
							 &lb, x, y, 100, 23,
							 "Black", "Black", "White", btnfontname)));
  xitk_show_widget(pplugin->new_filter);

  x += 115;

  lb.button_type       = RADIO_BUTTON;
  lb.label             = _("Enable");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = _pplugin_enability;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
   (pplugin->enable = xitk_noskin_labelbutton_create(pplugin->widget_list, 
						     &lb, x, y, 100, 23,
						     "Black", "Black", "White", btnfontname)));

  xitk_labelbutton_set_state(pplugin->enable, gGui->post_enable);
  xitk_enable_and_show_widget(pplugin->enable);

  /* IMPLEMENT ME
  x += 115;

  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Save");
  lb.align             = ALIGN_CENTER;
  lb.callback          = NULL;
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
   (w = xitk_noskin_labelbutton_create(pplugin->widget_list, 
							 &lb, x, y, 100, 23,
							 "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(w);
  */

  x = WINDOW_WIDTH - 115;
  
  lb.button_type       = CLICK_BUTTON;
  lb.label             = _("Ok");
  lb.align             = ALIGN_CENTER;
  lb.callback          = pplugin_exit; 
  lb.state_callback    = NULL;
  lb.userdata          = NULL;
  lb.skin_element_name = NULL;
  xitk_list_append_content((XITK_WIDGET_LIST_LIST(pplugin->widget_list)), 
   (w = xitk_noskin_labelbutton_create(pplugin->widget_list, 
				       &lb, x, y, 100, 23,
				       "Black", "Black", "White", btnfontname)));
  xitk_enable_and_show_widget(w);

  _pplugin_get_plugins();
  
  pplugin->x = 0;
  pplugin->y = -45;
  
  if(!gGui->post_elements)
    _pplugin_add_filter(NULL, NULL);
  else
    _pplugin_rebuild_filters();
  
  xitk_window_change_background(gGui->imlib_data, pplugin->xwin, bg->pixmap, width, height);
  xitk_image_destroy_xitk_pixmap(bg);
  
  pplugin->widget_key = xitk_register_event_handler("pplugin", 
						    (xitk_window_get_window(pplugin->xwin)),
						    _pplugin_handle_event,
						    NULL,
						    NULL,
						    pplugin->widget_list,
						    NULL);
  
  pplugin->visible = 1;
  pplugin->running = 1;

  _pplugin_paint_widgets();

  pplugin_raise_window();
  
  try_to_set_input_focus(xitk_window_get_window(pplugin->xwin));
}

/* pchain: "<post1>:option1=value1,option2=value2..;<post2>:...." */
static post_element_t **pplugin_parse_and_load(const char *pchain, int *post_elements_num) {
  post_element_t **post_elements = NULL;
  char            *post_chain;

  *post_elements_num = 0;
  
  if(pchain && strlen(pchain)) {
    char *p;
    
    xine_strdupa(post_chain, pchain);
    
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
	
	post = xine_post_init(gGui->xine, plugin, 0, &gGui->ao_port, &gGui->vo_port);
	
	if(post) {
	  post_object_t  pobj;
	 
	  if(!(*post_elements_num))
	    post_elements = (post_element_t **) xine_xmalloc(sizeof(post_element_t *) * 2);
	  else
	    post_elements = (post_element_t **) 
	      realloc(post_elements, sizeof(post_element_t *) * ((*post_elements_num) + 2));
	  
	  post_elements[(*post_elements_num)] = (post_element_t *) 
	    xine_xmalloc(sizeof(post_element_t));
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
			  
			  snprintf((char *)(pobj.param_data + pobj.param->offset), maxlen, "%s", p);
			  _pplugin_update_parameter(&pobj);
			}
			else
			  fprintf(stderr, "parameter type POST_PARAM_TYPE_STRING) not supported yet.\n");
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
  }

  return post_elements;
}

void pplugin_parse_and_store_post(const char *post_chain) {
  post_element_t **posts = NULL;
  int              num;

  if((posts = pplugin_parse_and_load(post_chain, &num))) {
    if(gGui->post_elements_num) {
      int i;
      int ptot = gGui->post_elements_num + num;
      
      gGui->post_elements = (post_element_t **) realloc(gGui->post_elements, 
							sizeof(post_element_t *) * (ptot + 1));
      for(i = gGui->post_elements_num; i <  ptot; i++)
	gGui->post_elements[i] = posts[i - gGui->post_elements_num];

      gGui->post_elements[i] = NULL;
      gGui->post_elements_num += num;

    }
    else {
      gGui->post_elements     = posts;
      gGui->post_elements_num = num;;
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
    (char *) xine_config_register_string (gGui->xine, "gui.deinterlace_plugin", 
					  deinterlace_default,
					  _("Deinterlace plugin."),
					  _("Plugin (with optional parameters) to use "
					    "when deinterlace is used (plugin separator is ';')."),
					  CONFIG_LEVEL_ADV,
					  post_deinterlace_plugin_cb,
					  CONFIG_NO_DATA);
  if((posts = pplugin_parse_and_load((deinterlace_post && strlen(deinterlace_post)) ? 
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
    _pplugin_unwire();
    _pplugin_rewire();
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
