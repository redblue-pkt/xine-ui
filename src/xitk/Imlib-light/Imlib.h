#ifndef __IMLIB_H__
#define __IMLIB_H__

#include "Imlib_types.h"

#ifdef __cplusplus
extern              "C"
{
#endif				/* __cplusplus */

  ImlibData          *Imlib_init(Display * disp);
  ImlibData	     *Imlib_init_with_params(Display * disp, ImlibInitParams * p);
  ImlibImage         *Imlib_load_image(ImlibData * id, char *file);
  void                Imlib_apply_image(ImlibData * id, ImlibImage * im, Window p);
  Pixmap              Imlib_copy_image(ImlibData * id, ImlibImage * im);
  Pixmap              Imlib_copy_mask(ImlibData * id, ImlibImage * im);
  int                 Imlib_render(ImlibData * id, ImlibImage * image, int width, int height);
  Pixmap              Imlib_move_image(ImlibData * id, ImlibImage * image);
  Pixmap              Imlib_move_mask(ImlibData * id, ImlibImage * im);
  ImlibImage         *Imlib_clone_image(ImlibData * id, ImlibImage * im);
  void                Imlib_destroy_image(ImlibData * id, ImlibImage * im);
  void                Imlib_kill_image(ImlibData * id, ImlibImage * im);
  int                 Imlib_load_colors(ImlibData * id, char *file);
  int                 Imlib_best_color_match(ImlibData * id, int *r, int *g, int *b);
  void                Imlib_free_colors(ImlibData * id);
  int                 Imlib_load_default_colors(ImlibData * id);
  void                Imlib_free_pixmap(ImlibData * id, Pixmap pmap);
  Colormap            Imlib_get_colormap(ImlibData * id);
  int                 Imlib_get_render_type(ImlibData * id);
  void                Imlib_set_render_type(ImlibData * id, int rend_type);
  void                Imlib_set_image_border(ImlibData * id, ImlibImage * im, ImlibBorder * border);
  void                Imlib_get_image_border(ImlibData * id, ImlibImage * im, ImlibBorder * border);
  void                Imlib_get_image_shape(ImlibData * id, ImlibImage * im, ImlibColor * color);
  void                Imlib_set_image_shape(ImlibData * id, ImlibImage * im, ImlibColor * color);
  int                 Imlib_get_fallback(ImlibData * id);
  void                Imlib_set_fallback(ImlibData * id, int fallback);
  Visual             *Imlib_get_visual(ImlibData * id);
  char               *Imlib_get_sysconfig(ImlibData * id);



#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif

