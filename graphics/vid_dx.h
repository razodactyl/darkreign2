///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vid_dx.h     DirectX device handles, private to the DirectX code
//
// These used to sit in vid_public.h, which meant every translation unit that
// wanted a render state also got a Direct3D device pointer. They are declared
// here instead so that only the files that genuinely talk to DirectX see them:
//
//   vid_backend_d3d.cpp   the D3D rendering backend
//   vid.cpp               device and surface creation
//   vid_enumdx.cpp        driver and mode enumeration
//   viderror.cpp          HRESULT to string
//   bitmap.cpp            texture surfaces
//   bitmap_manager.cpp    texture memory reporting
//
// Nothing else may include this. Rendering goes through Vid::backend
// (vid_backend.h); see docs/research/graphics-backend-port.md.
//

#ifndef __VIDDX_H
#define __VIDDX_H

#include "vid_public.h"
//-----------------------------------------------------------------------------

extern HRESULT dxError;

#if	(defined DEVELOPMENT) || (defined DOLOGDXERROR)
#define LOG_DXERR( fmt)                             \
	if (dxError)                                      \
  {                                                 \
		LOG_ERR( fmt );                                 \
    LOG_ERR( ("...%s", GetErrorString( dxError)) ); \
  }
#else
#define LOG_DXERR( fmt)
#endif
//-----------------------------------------------------------------------------

namespace Vid
{
    extern SurfaceDD front;
    extern DirectDD ddx;
    extern Direct3D d3d;
    extern DeviceD3D device;
};

//-----------------------------------------------------------------------------

#endif // __VIDDX_H
