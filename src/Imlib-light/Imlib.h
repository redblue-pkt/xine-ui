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
  int                 Imlib_render(ImlibData * id, ImlibImage * image, int width, int height);
  Pixmap              Imlib_move_image(ImlibData * id, ImlibImage * image);
  Pixmap              Imlib_move_mask(ImlibData * id, ImlibImage * im);
  ImlibImage         *Imlib_clone_image(ImlibData * id, ImlibImage * im);
  void                Imlib_destroy_image(ImlibData * id, ImlibImage * im);
  void                Imlib_kill_image(ImlibData * id, ImlibImage * im);
  void                Imlib_free_pixmap(ImlibData * id, Pixmap pmap);
  int                 Imlib_load_colors(ImlibData * id, char *file);
  Colormap            Imlib_get_colormap(ImlibData * id);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif

