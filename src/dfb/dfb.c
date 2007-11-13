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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * $Id$
 *
 * DirectFB output utils
 * Rich Wareham <richwareham@users.sourceforge.net>
 * 
 */

#include "dfb.h"

DFBEnumerationResult enum_layers_callback( unsigned int                 id,
                                           DFBDisplayLayerCapabilities  caps,
                                           void                        *data )
{
     IDirectFBDisplayLayer **layer = (IDirectFBDisplayLayer **)data;

     printf( "\nLayer %d:\n", id );

     if (caps & DLCAPS_SURFACE)
          printf( "  - Has a surface.\n" );

     if (caps & DLCAPS_ALPHACHANNEL)
          printf( "  - Supports blending based on alpha channel.\n" );

     if (caps & DLCAPS_SRC_COLORKEY)
          printf( "  - Supports color keying.\n" );

     if (caps & DLCAPS_FLICKER_FILTERING)
          printf( "  - Supports flicker filtering.\n" );

     if (caps & DLCAPS_INTERLACED_VIDEO)
          printf( "  - Can natively display interlaced video.\n" );

     if (caps & DLCAPS_OPACITY)
          printf( "  - Supports blending based on global alpha factor.\n" );

     if (caps & DLCAPS_SCREEN_LOCATION)
          printf( "  - Can be positioned on the screen.\n" );

     if (caps & DLCAPS_BRIGHTNESS)
          printf( "  - Brightness can be adjusted.\n" );

     if (caps & DLCAPS_CONTRAST)
          printf( "  - Contrast can be adjusted.\n" );

     if (caps & DLCAPS_HUE)
          printf( "  - Hue can be adjusted.\n" );

     if (caps & DLCAPS_SATURATION)
          printf( "  - Saturation can be adjusted.\n" );

     printf("\n");

     /* We take the first layer not being the primary */
     if (id != DLID_PRIMARY) {
          DFBResult ret;

          ret = dfbxine.dfb->GetDisplayLayer( dfbxine.dfb, id, layer );
          if (ret)
               DirectFBError( "dfb->GetDisplayLayer failed", ret );
          else
               return DFENUM_CANCEL;
     }

     return DFENUM_OK;
}

int init_dfb() {
  DFBCardCapabilities     caps;
  DFBSurfaceDescription   desc;
  IDirectFBImageProvider *provider;
  DFBFontDescription      font_desc;
  DFBWindowDescription    win_desc;
  DFBResult               ret;

  /* Create global DFB instance */
  DFBCHECK(DirectFBCreate(&(dfbxine.dfb)));
  DFBCHECK(dfbxine.dfb->SetCooperativeLevel(dfbxine.dfb, 
					    DFSCL_NORMAL));
  dfbxine.dfb->GetCardCapabilities( dfbxine.dfb, &caps );
  dfbxine.dfb->GetDisplayLayer( dfbxine.dfb, DLID_PRIMARY, &(dfbxine.layer) );
  dfbxine.layer->SetCooperativeLevel(dfbxine.layer, DLSCL_EXCLUSIVE);

  if (!((caps.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
       (caps.blitting_flags & DSBLIT_BLEND_COLORALPHA  ))) {
    dfbxine.layer_config.flags = DLCONF_BUFFERMODE;
    dfbxine.layer_config.buffermode = DLBM_BACKSYSTEM; 
    dfbxine.layer->SetConfiguration( dfbxine.layer, &(dfbxine.layer_config) );
  }

  dfbxine.layer->GetConfiguration( dfbxine.layer, &(dfbxine.layer_config) );
  dfbxine.layer->EnableCursor ( dfbxine.layer, 1 );

  /* Set cursor shape */
  DFBCHECK(dfbxine.dfb->CreateImageProvider(dfbxine.dfb, POINTER,
					    &provider));
  DFBCHECK (provider->GetSurfaceDescription (provider, &desc));
  DFBCHECK(dfbxine.dfb->CreateSurface(dfbxine.dfb, &desc, &(dfbxine.pointer)));
  DFBCHECK(provider->RenderTo(provider, dfbxine.pointer,NULL));
  provider->Release(provider);
  dfbxine.layer->SetCursorShape(dfbxine.layer, dfbxine.pointer, 1,1);

  /* Load UI font */
  font_desc.flags = DFDESC_HEIGHT;
  font_desc.height = 20;
   
  DFBCHECK(dfbxine.dfb->CreateFont( dfbxine.dfb, FONT, &font_desc, 
				    &(dfbxine.font) ));
  dfbxine.font->GetHeight( dfbxine.font, &(dfbxine.fontheight) );

  /* Create background surface ('primary') */ 
  desc.flags = DSDESC_WIDTH | DSDESC_HEIGHT;
  desc.width = dfbxine.layer_config.width;
  desc.height = dfbxine.layer_config.height;
  
  DFBCHECK(dfbxine.dfb->CreateSurface(dfbxine.dfb, &desc, 
				      &(dfbxine.primary)));

  /* Render background */
  DFBCHECK(dfbxine.primary->GetSize(dfbxine.primary, &(dfbxine.screen_width),
				    &(dfbxine.screen_height)));
  DFBCHECK(dfbxine.primary->SetColor(dfbxine.primary, 0x5F, 0x5F, 0x5C, 0xFF));
  DFBCHECK(dfbxine.primary->FillRectangle(dfbxine.primary, 0,0, 
					  dfbxine.screen_width,
					  dfbxine.screen_height));
  DFBCHECK(dfbxine.primary->SetFont(dfbxine.primary, dfbxine.font));
  DFBCHECK(dfbxine.primary->SetColor(dfbxine.primary, 0xCF, 0xCF, 0xFF, 0xFF));
  
  DFBCHECK(dfbxine.primary->DrawString(dfbxine.primary,
				       "This is the DirectFB output mode "
				       "for Xine (EXPERIMENTAL)",
				       -1,5,5, DSTF_LEFT | DSTF_TOP));
  DFBCHECK(dfbxine.layer->SetBackgroundImage(dfbxine.layer,dfbxine.primary));
  DFBCHECK(dfbxine.layer->SetBackgroundMode(dfbxine.layer, DLBM_IMAGE));

  /* Create the Video layer */
  ret = dfbxine.dfb->EnumDisplayLayers(dfbxine.dfb, 
				       enum_layers_callback, 
				       &(dfbxine.video_layer));
  if (ret)
   DirectFBErrorFatal( "dfb->EnumDisplayLayers failed", ret );

  if (!dfbxine.video_layer) {
    printf( "\nNo additional layers have been found.\n" );
    return 0;
  }
  dfbxine.video_layer->SetScreenLocation(dfbxine.video_layer, 0.25,0.25,
					 0.5,0.5);

  
  win_desc.flags = ( DWDESC_POSX | DWDESC_POSY |
		     DWDESC_WIDTH | DWDESC_HEIGHT );
  win_desc.posx = 5;
  win_desc.posy = 5;
  win_desc.width = dfbxine.layer_config.width/2;
  win_desc.height = dfbxine.layer_config.height/2;

  DFBCHECK( dfbxine.layer->CreateWindow( dfbxine.layer, &win_desc, 
					 &(dfbxine.main_window) ) );
  dfbxine.main_window->GetSurface( dfbxine.main_window, 
				   &(dfbxine.main_surface) );
  dfbxine.main_window->SetOpacity(dfbxine.main_window, 0xff);
  
  dfbxine.main_surface->SetColor( dfbxine.main_surface,
				  0x00, 0x00, 0x00, 0xff );
  dfbxine.main_surface->FillRectangle( dfbxine.main_surface, 0, 0,
				       win_desc.width, win_desc.height );
   
  dfbxine.main_surface->Flip(dfbxine.main_surface, NULL, 0);
  dfbxine.main_window->RequestFocus(dfbxine.main_window);
  dfbxine.main_window->RaiseToTop(dfbxine.main_window);

  return 1;
}

