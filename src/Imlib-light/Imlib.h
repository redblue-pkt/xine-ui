#ifndef __IMLIB_H__
#define __IMLIB_H__

#include <Imlib_types.h>

#ifdef __cplusplus
extern              "C"
{
#endif				/* __cplusplus */

  ImlibData          *Imlib_init(Display * disp);
  ImlibImage         *Imlib_load_image(ImlibData * id, char *file);
  void                Imlib_apply_image(ImlibData * id, ImlibImage * im, Window p);
  Pixmap              Imlib_copy_image(ImlibData * id, ImlibImage * im);
  int                 Imlib_render(ImlibData * id, ImlibImage * image, int width, int height);
  Pixmap              Imlib_move_image(ImlibData * id, ImlibImage * image);
  Pixmap              Imlib_move_mask(ImlibData * id, ImlibImage * im);
  void                Imlib_destroy_image(ImlibData * id, ImlibImage * im);
  void                Imlib_kill_image(ImlibData * id, ImlibImage * im);
  void                Imlib_free_pixmap(ImlibData * id, Pixmap pmap);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif

