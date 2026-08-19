///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vid_backend_ogl.cpp     OpenGL 3.3 core rendering backend
//
// Selected with -ogl on the command line. Still being built up: this phase
// brings up the context and owns the frame, and everything that actually draws
// is a stub. See docs/research/graphics-backend-port.md for the phase plan.
//
// The context is created on the game's existing Win32 window via WGL. The
// window, the message pump and DirectInput are deliberately left exactly as
// they are - the 2020 attempt at this replaced them with GLFW and broke
// multiplayer and game processing in the process.
//

#include "vid_private.h"
#include "vid_backend.h"
#include "main.h"
#include "console.h"
//-----------------------------------------------------------------------------

#include <glad/gl.h>
#include <glad/wgl.h>

#pragma comment(lib, "opengl32")
//-----------------------------------------------------------------------------

namespace Vid
{
    namespace BackendOGL
    {
        static HWND hWnd;
        static HDC hDC;
        static HGLRC hGLRC;

        static int glVersion;    // as returned by gladLoadGL, 0 until loaded

        //---------------------------------------------------------------------
        //
        // context creation
        //
        // Getting a core profile context needs a context to ask for one with,
        // so this is the usual two-step: a throwaway legacy context on a
        // throwaway window loads the WGL extensions, then the real context is
        // created on the game window with wglCreateContextAttribsARB.
        //

        static Bool LoadWGLExtensions()
        {
            // A window's pixel format can only be set once, so the probing is
            // done on a scratch window rather than the game's.
            WNDCLASSA wc;
            Utils::Memset(&wc, 0, sizeof(wc));
            wc.style = CS_OWNDC;
            wc.lpfnWndProc = DefWindowProcA;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = "DR2GLProbe";

            if (!RegisterClassA(&wc))
            {
                LOG_ERR(("OGL: could not register the probe window class"));
                return FALSE;
            }

            HWND probeWnd = CreateWindowExA
            (
                0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
                0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr
            );

            if (!probeWnd)
            {
                LOG_ERR(("OGL: could not create the probe window"));
                UnregisterClassA(wc.lpszClassName, wc.hInstance);
                return FALSE;
            }

            HDC probeDC = GetDC(probeWnd);

            PIXELFORMATDESCRIPTOR pfd;
            Utils::Memset(&pfd, 0, sizeof(pfd));
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = 32;
            pfd.cDepthBits = 24;
            pfd.cStencilBits = 8;

            Bool ok = FALSE;

            int format = ChoosePixelFormat(probeDC, &pfd);
            if (format && SetPixelFormat(probeDC, format, &pfd))
            {
                HGLRC probeRC = wglCreateContext(probeDC);
                if (probeRC && wglMakeCurrent(probeDC, probeRC))
                {
                    ok = gladLoaderLoadWGL(probeDC) != 0;
                    if (!ok)
                    {
                        LOG_ERR(("OGL: could not load the WGL extensions"));
                    }
                    wglMakeCurrent(nullptr, nullptr);
                }
                else
                {
                    LOG_ERR(("OGL: could not create the probe context"));
                }

                if (probeRC)
                {
                    wglDeleteContext(probeRC);
                }
            }
            else
            {
                LOG_ERR(("OGL: no pixel format for the probe window"));
            }

            ReleaseDC(probeWnd, probeDC);
            DestroyWindow(probeWnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);

            return ok;
        }

        //---------------------------------------------------------------------

        Bool CreateContext(HWND window)
        {
            ASSERT(window);

            if (hGLRC)
            {
                // already up
                return TRUE;
            }

            if (!LoadWGLExtensions())
            {
                return FALSE;
            }

            if (!wglCreateContextAttribsARB || !wglChoosePixelFormatARB)
            {
                LOG_ERR(("OGL: the driver has no ARB_create_context; need OpenGL 3.3"));
                return FALSE;
            }

            hWnd = window;
            hDC = GetDC(hWnd);

            const int formatAttribs[] =
            {
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB, 32,
                WGL_DEPTH_BITS_ARB, 24,
                WGL_STENCIL_BITS_ARB, 8,
                0
            };

            int format = 0;
            UINT formatCount = 0;

            if (!wglChoosePixelFormatARB(hDC, formatAttribs, nullptr, 1, &format, &formatCount)
                || formatCount == 0)
            {
                LOG_ERR(("OGL: no suitable pixel format for the game window"));
                ReleaseDC(hWnd, hDC);
                hDC = nullptr;
                return FALSE;
            }

            PIXELFORMATDESCRIPTOR pfd;
            DescribePixelFormat(hDC, format, sizeof(pfd), &pfd);

            if (!SetPixelFormat(hDC, format, &pfd))
            {
                // The window only gets one pixel format for its whole life. If
                // this fails, something already claimed it - most likely a
                // DirectDraw surface was set up on this window first.
                LOG_ERR(("OGL: could not set the pixel format on the game window"));
                ReleaseDC(hWnd, hDC);
                hDC = nullptr;
                return FALSE;
            }

            const int contextAttribs[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef DEVELOPMENT
                WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                0
            };

            hGLRC = wglCreateContextAttribsARB(hDC, nullptr, contextAttribs);
            if (!hGLRC)
            {
                LOG_ERR(("OGL: the driver would not give us an OpenGL 3.3 core context"));
                ReleaseDC(hWnd, hDC);
                hDC = nullptr;
                return FALSE;
            }

            if (!wglMakeCurrent(hDC, hGLRC))
            {
                LOG_ERR(("OGL: wglMakeCurrent failed"));
                wglDeleteContext(hGLRC);
                hGLRC = nullptr;
                ReleaseDC(hWnd, hDC);
                hDC = nullptr;
                return FALSE;
            }

            glVersion = gladLoaderLoadGL();
            if (!glVersion)
            {
                LOG_ERR(("OGL: could not load the OpenGL entry points"));
                return FALSE;
            }

            LOG_DIAG(("OGL: %s", (const char*)glGetString(GL_VERSION)));
            LOG_DIAG(("OGL: %s", (const char*)glGetString(GL_RENDERER)));
            LOG_DIAG(("OGL: GLSL %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION)));

            // vsync off by default; renderState.status.waitRetrace drives it
            // once the options plumbing lands
            if (wglSwapIntervalEXT)
            {
                wglSwapIntervalEXT(0);
            }

            return TRUE;
        }

        //---------------------------------------------------------------------

        void DestroyContext()
        {
            if (hGLRC)
            {
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(hGLRC);
                hGLRC = nullptr;
            }
            if (hDC)
            {
                ReleaseDC(hWnd, hDC);
                hDC = nullptr;
            }
            hWnd = nullptr;
            glVersion = 0;
        }

        //---------------------------------------------------------------------

        Bool Present()
        {
            if (!hDC)
            {
                return FALSE;
            }
            return SwapBuffers(hDC) ? TRUE : FALSE;
        }

        //---------------------------------------------------------------------
        //
        // frame
        //

        Bool BeginScene()
        {
            return hGLRC != nullptr;
        }

        //---------------------------------------------------------------------

        Bool EndScene()
        {
            return hGLRC != nullptr;
        }

        //---------------------------------------------------------------------

        void Clear(U32 clearFlags, Color color, const Area<S32>& rect)
        {
            rect;

            if (!hGLRC)
            {
                return;
            }

            GLbitfield mask = 0;

            if (clearFlags & clearBACK)
            {
                glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
                mask |= GL_COLOR_BUFFER_BIT;
            }
            if (clearFlags & clearZBUFFER)
            {
                // GL clamps to [0,1]; D3D clears z to 1 here
                glClearDepth(1.0);
                mask |= GL_DEPTH_BUFFER_BIT;
            }
            if (clearFlags & clearSTENCIL)
            {
                glClearStencil(0);
                mask |= GL_STENCIL_BUFFER_BIT;
            }

            // the D3D path clears a sub-rect; scissor is how GL does that, and
            // it arrives with the viewport work in the next phase
            glClear(mask);
        }

        //---------------------------------------------------------------------

        Bool SetViewport(const ViewPort& desc)
        {
            if (!hGLRC)
            {
                return FALSE;
            }

            // GL's origin is bottom-left, D3D's is top-left, so y is flipped
            // against the window height rather than passed straight through
            RECT client;
            GetClientRect(hWnd, &client);

            glViewport(desc.x, client.bottom - (desc.y + desc.height), desc.width, desc.height);
            glDepthRange(desc.minZ, desc.maxZ);

            return TRUE;
        }

        //---------------------------------------------------------------------
        //
        // textures
        //
        // Bitmaps under this backend keep their pixels in system memory (see
        // Bitmap::Create) in 32-bit A8R8G8B8, which is what Vid::PixNormal and
        // friends are set to when -ogl is given. That byte order is GL_BGRA
        // with GL_UNSIGNED_INT_8_8_8_8_REV, so no conversion is needed on the
        // way in.
        //

        static U32 boundTexture[MAX_TEXTURE_STAGES];

        U32 TextureCreate()
        {
            GLuint tex = 0;
            glGenTextures(1, &tex);

            return tex;
        }

        //---------------------------------------------------------------------

        void TextureDestroy(U32 tex)
        {
            if (tex)
            {
                GLuint t = tex;
                glDeleteTextures(1, &t);
            }
        }

        //---------------------------------------------------------------------

        void TextureUpload
        (
            U32 tex,
            const void* pixels,
            S32 width,
            S32 height,
            S32 pitch,
            Bool mipmap
        )
        {
            if (!tex || !pixels)
            {
                return;
            }

            glBindTexture(GL_TEXTURE_2D, tex);

            // rows are dword-aligned and the pitch can be wider than the image
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);

            glTexImage2D
            (
                GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels
            );

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            if (mipmap)
            {
                glGenerateMipmap(GL_TEXTURE_2D);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmap ? 1000 : 0);

            // the real filter and wrap state arrives with SetTexFilter /
            // SetTexWrap below; these are just sane defaults for first upload
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        //---------------------------------------------------------------------

        Bool BindTexture(const Bitmap* tex, U32 stage)
        {
            if (stage >= MAX_TEXTURE_STAGES)
            {
                return FALSE;
            }

            U32 name = tex ? tex->BackendTexture() : 0;

            boundTexture[stage] = name;

            glActiveTexture(GL_TEXTURE0 + stage);
            glBindTexture(GL_TEXTURE_2D, name);

            return TRUE;
        }

        //---------------------------------------------------------------------

        void SetTexWrap(U32 mode, U32 stage)
        {
            if (stage >= MAX_TEXTURE_STAGES || !boundTexture[stage])
            {
                return;
            }

            GLint wrap = GL_REPEAT;
            switch (mode + 1)    // stored as (mode - 1); see RS_ADD_MASK
            {
                case TA_MIRROR: wrap = GL_MIRRORED_REPEAT; break;
                case TA_CLAMP: wrap = GL_CLAMP_TO_EDGE; break;
                default: wrap = GL_REPEAT; break;
            }

            glActiveTexture(GL_TEXTURE0 + stage);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        }

        //---------------------------------------------------------------------

        void SetTexFilter(U32 filterFlags)
        {
            GLint mag = (filterFlags & filterFILTER) ? GL_LINEAR : GL_NEAREST;
            GLint min = mag;

            if (filterFlags & filterMIPMAP)
            {
                if (filterFlags & filterMIPFILTER)
                {
                    min = (filterFlags & filterFILTER) ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
                }
                else
                {
                    min = (filterFlags & filterFILTER) ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
                }
            }

            // applies to whatever is bound on each stage, matching the D3D path
            for (U32 stage = 0; stage < MAX_TEXTURE_STAGES; stage++)
            {
                if (!boundTexture[stage])
                {
                    continue;
                }
                glActiveTexture(GL_TEXTURE0 + stage);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
            }
        }

        //---------------------------------------------------------------------
        //
        // Everything below is not implemented yet. The game drives all of it on
        // every frame, so these have to exist and stay silent - logging here
        // would produce thousands of lines a second. Each is picked up by the
        // phase named against it in docs/research/graphics-backend-port.md.
        //

        // phase 5: render state
        void ResetState(Bool) {}
        void SetZBuffer(Bool, Bool) {}
        void SetZWrite(Bool) {}
        Bool GetZWrite() { return TRUE; }
        void SetAlphaBlend(Bool) {}
        void SetCull(Bool) {}
        void SetSrcBlend(U32) {}
        void SetDstBlend(U32) {}
        void SetClipping(Bool) {}
        void SetLighting(Bool) {}
        void SetShade(U32) {}
        void SetDither(Bool) {}
        void SetSpecular(Bool) {}
        void SetAntiAlias(Bool) {}
        void SetEdgeAntiAlias(Bool) {}
        Bool SetPerspective(Bool) { return TRUE; }
        Bool SetColorKey(Bool) { return FALSE; }

        // phase 5: the shader decides how the stages combine
        void SetTexBlend(U32, U32) {}
        void SetTextureFactor(Color) {}
        Bool DisableTexStage(U32) { return TRUE; }

        // phase 5: the uber-shader decides these, so every blend is available
        void ValidateBlends() {}
        Bool ValidateBlend(U32, U32) { return TRUE; }

        // phase 5: fog and ambient
        void SetFog(Bool) {}
        void SetFogColor(U32) {}
        void SetFogRange(F32, F32) {}
        void SetAmbientColor(U32) {}

        // phase 5: transforms become shader uniforms
        void SetWorldTransform(const Matrix&) {}
        void SetViewTransform(const Matrix&) {}
        void SetProjTransform(const Matrix&) {}

        // phase 5: materials and lights become shader uniforms
        void SetMaterial(const Material*) {}
        void SetLight(U32, Light::Obj&) {}
        void EnableLight(U32, Bool) {}

        // phase 4 (FVF_TLVERTEX) then phase 5 (the rest)
        Bool DrawPrimitive(PRIMITIVE_TYPE, VERTEX_TYPE, void*, U32, U32)
        {
            return TRUE;
        }

        Bool DrawIndexedPrimitive(PRIMITIVE_TYPE, VERTEX_TYPE, void*, U32, const U16*, U32, U32)
        {
            return TRUE;
        }

        //---------------------------------------------------------------------
    };

    //-------------------------------------------------------------------------

    const Backend backendGL =
    {
        "OpenGL 3.3",
        backendOGL,

        BackendOGL::BeginScene,
        BackendOGL::EndScene,
        BackendOGL::Clear,
        BackendOGL::SetViewport,

        BackendOGL::ResetState,

        BackendOGL::SetZBuffer,
        BackendOGL::SetZWrite,
        BackendOGL::GetZWrite,
        BackendOGL::SetAlphaBlend,
        BackendOGL::SetCull,
        BackendOGL::SetSrcBlend,
        BackendOGL::SetDstBlend,
        BackendOGL::SetClipping,
        BackendOGL::SetLighting,
        BackendOGL::SetShade,
        BackendOGL::SetDither,
        BackendOGL::SetSpecular,
        BackendOGL::SetAntiAlias,
        BackendOGL::SetEdgeAntiAlias,

        BackendOGL::SetPerspective,
        BackendOGL::SetColorKey,

        BackendOGL::SetTexBlend,
        BackendOGL::SetTexWrap,
        BackendOGL::SetTexFilter,
        BackendOGL::SetTextureFactor,
        BackendOGL::BindTexture,
        BackendOGL::DisableTexStage,

        BackendOGL::TextureCreate,
        BackendOGL::TextureDestroy,
        BackendOGL::TextureUpload,

        BackendOGL::ValidateBlends,
        BackendOGL::ValidateBlend,

        BackendOGL::SetFog,
        BackendOGL::SetFogColor,
        BackendOGL::SetFogRange,
        BackendOGL::SetAmbientColor,

        BackendOGL::SetWorldTransform,
        BackendOGL::SetViewTransform,
        BackendOGL::SetProjTransform,

        BackendOGL::SetMaterial,
        BackendOGL::SetLight,
        BackendOGL::EnableLight,

        BackendOGL::DrawPrimitive,
        BackendOGL::DrawIndexedPrimitive,
    };

    //-------------------------------------------------------------------------

    namespace OGL
    {
        Bool CreateContext(HWND hWnd)
        {
            return BackendOGL::CreateContext(hWnd);
        }

        void DestroyContext()
        {
            BackendOGL::DestroyContext();
        }

        Bool Present()
        {
            return BackendOGL::Present();
        }
    };

    //-------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------
