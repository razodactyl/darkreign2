///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vid_backend.h     rendering backend dispatch
//
// The Vid layer talks to the graphics hardware through the table of function
// pointers below rather than calling a device directly. There is one table per
// backend; `Vid::backend` points at whichever was selected at startup.
//
// The public Vid API (vid_public.h) does not change shape: each Vid::SetXState
// still owns its renderState bookkeeping and return value, and only the part
// that actually touches the device goes through here.
//
// Backends:
//   vid_backend_d3d.cpp   DirectX 7, the default
//
// A backend is chosen once, at Vid::Init, and does not change for the lifetime
// of the process. See docs/research/graphics-backend-port.md.
//

#ifndef __VIDBACKEND_H
#define __VIDBACKEND_H

#include "vid_public.h"
//-----------------------------------------------------------------------------

namespace Vid
{
    // light.h is not pulled in here; SetLight only needs the reference
    namespace Light
    {
        class Obj;
    }

    //-----------------------------------------------------------------------------

    enum BackendId
    {
        backendDX7,
        backendOGL,
    };

    //-----------------------------------------------------------------------------

    struct Backend
    {
        // identity
        //
        const char* name;
        BackendId id;

        // frame
        //
        Bool (*BeginScene)();
        Bool (*EndScene)();
        void (*Clear)(U32 clearFlags, Color color, const Area<S32>& rect);

        // Returns FALSE if the backend is not ready to accept one yet, in which
        // case the caller must not update Vid::clipRect either.
        Bool (*SetViewport)(const ViewPort& desc);

        // the block of one-off device defaults applied by Vid::SetRenderState
        //
        void (*ResetState)(Bool wbuffer);

        // depth, blending, rasterization
        //
        void (*SetZBuffer)(Bool doZBuffer, Bool wbuffer);
        void (*SetZWrite)(Bool doZWrite);
        Bool (*GetZWrite)();
        void (*SetAlphaBlend)(Bool doAlpha);
        void (*SetCull)(Bool doCull);
        void (*SetSrcBlend)(U32 blend); // already shifted out of RS_SRC_MASK
        void (*SetDstBlend)(U32 blend); // already shifted out of RS_DST_MASK
        void (*SetClipping)(Bool doClip);
        void (*SetLighting)(Bool doLighting);
        void (*SetShade)(U32 shadeFlags); // Vid::ShadeFlags
        void (*SetDither)(Bool doDither);
        void (*SetSpecular)(Bool doSpecular);
        void (*SetAntiAlias)(Bool on);
        void (*SetEdgeAntiAlias)(Bool on);

        // these two report the state they replaced, so they cannot be void
        //
        Bool (*SetPerspective)(Bool on);
        Bool (*SetColorKey)(Bool on);

        // texture stages
        //
        void (*SetTexBlend)(U32 op, U32 stage); // already shifted out of RS_TEX_MASK
        void (*SetTexWrap)(U32 mode, U32 stage); // already shifted out of RS_ADD_MASK
        void (*SetTexFilter)(U32 filterFlags); // Vid::FilterFlags
        void (*SetTextureFactor)(Color color);
        Bool (*BindTexture)(const Bitmap* tex, U32 stage);
        Bool (*DisableTexStage)(U32 stage);

        // Texture storage.
        //
        // The Bitmap always owns its pixels in system memory; these manage
        // whatever the backend additionally needs to sample from them. The D3D
        // backend samples straight out of its DirectDraw surface and so has
        // nothing to do here - it returns 0 and ignores the rest.
        //
        // TextureCreate returns an opaque backend handle, 0 for "none".
        U32 (*TextureCreate)();
        void (*TextureDestroy)(U32 tex);
        void (*TextureUpload)
        (
            U32 tex,
            const void* pixels,
            S32 width,
            S32 height,
            S32 pitch,      // bytes per row, may exceed width * 4
            Bool mipmap
        );

        // probe which blend ops the hardware can actually do, and downgrade the
        // ones it cannot; run once after the device comes up
        //
        void (*ValidateBlends)();
        Bool (*ValidateBlend)(U32 blend, U32 stage);

        // fog and ambient light
        //
        void (*SetFog)(Bool fogOn);
        void (*SetFogColor)(U32 fogColor);
        void (*SetFogRange)(F32 min, F32 max);
        void (*SetAmbientColor)(U32 color);

        // transforms
        //
        void (*SetWorldTransform)(const Matrix& mat);
        void (*SetViewTransform)(const Matrix& mat);
        void (*SetProjTransform)(const Matrix& mat);

        // materials and lights
        //
        void (*SetMaterial)(const Material* mat);
        void (*SetLight)(U32 index, Light::Obj& light);
        void (*EnableLight)(U32 index, Bool on);

        // geometry
        //
        Bool (*DrawPrimitive)
        (
            PRIMITIVE_TYPE prim_type,
            VERTEX_TYPE vert_type,
            void* verts,
            U32 vert_count,
            U32 flags
        );

        Bool (*DrawIndexedPrimitive)
        (
            PRIMITIVE_TYPE prim_type,
            VERTEX_TYPE vert_type,
            void* verts,
            U32 vert_count,
            const U16* indices,
            U32 index_count,
            U32 flags
        );
    };

    //-----------------------------------------------------------------------------

    // the table in use; never NULL after SelectBackend
    extern const Backend* backend;

    // the DirectX 7 table, defined in vid_backend_d3d.cpp
    extern const Backend backendD3D;

    // the OpenGL 3.3 table, defined in vid_backend_ogl.cpp
    extern const Backend backendGL;

    // pick a backend; called once from Vid::Init before anything renders
    void SelectBackend(BackendId id);

    //-----------------------------------------------------------------------------

    // OpenGL context management. The context lives on the game's existing Win32
    // window - the window, the message pump and DirectInput are untouched.
    namespace OGL
    {
        Bool CreateContext(HWND hWnd);
        void DestroyContext();
        Bool Present();
    };

    //-----------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------

#endif // __VIDBACKEND_H
