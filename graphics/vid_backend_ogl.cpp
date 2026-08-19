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

        static Bool InitShaders();    // defined below, called from CreateContext

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

            if (!InitShaders())
            {
                return FALSE;
            }

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
        // shaders
        //
        // One program for now, covering the pre-transformed (FVF_TLVERTEX)
        // geometry the interface, fonts and cursor are built from. Vertices
        // arrive already in screen pixels with the origin top-left, so the
        // vertex shader only has to fold them into clip space.
        //
        // The full RS_TEX_* stage-combine matrix is phase 5; this modulates
        // texture by vertex colour, which is what RS_TEX_MODULATE - the default
        // blend, and what almost all interface drawing asks for - means.
        //

        static const char* vertexShaderSrc =
            "#version 330 core\n"
            "layout (location = 0) in vec4 inPos;      // x, y in pixels; z depth; w rhw\n"
            "layout (location = 1) in vec4 inDiffuse;\n"
            "layout (location = 2) in vec4 inSpecular;\n"
            "layout (location = 3) in vec2 inUV;\n"
            "\n"
            "uniform vec2 screenSize;\n"
            "\n"
            "out vec4 vDiffuse;\n"
            "out vec4 vSpecular;\n"
            "out vec2 vUV;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    // pixels -> clip space, flipping y because GL puts the origin\n"
            "    // at the bottom left and these vertices assume the top left\n"
            "    float x = (inPos.x / screenSize.x) * 2.0 - 1.0;\n"
            "    float y = 1.0 - (inPos.y / screenSize.y) * 2.0;\n"
            "    float z = inPos.z * 2.0 - 1.0;\n"
            "\n"
            "    // These vertices are already projected, and inPos.w is rhw (1/w).\n"
            "    // Emitting them with w = 1 places them correctly but makes GL\n"
            "    // interpolate every varying in screen space - affine mapping, which\n"
            "    // is what makes textures zig-zag across a perspective surface.\n"
            "    // Restoring w and pre-multiplying x/y/z by it gives the same screen\n"
            "    // position after the perspective divide, while handing GL the w it\n"
            "    // needs to interpolate correctly. 2D geometry sets rhw = 1, so it\n"
            "    // comes through unchanged.\n"
            "    float w = (inPos.w != 0.0) ? (1.0 / inPos.w) : 1.0;\n"
            "    gl_Position = vec4(x * w, y * w, z * w, w);\n"
            "\n"
            "    vDiffuse = inDiffuse;\n"
            "    vSpecular = inSpecular;\n"
            "    vUV = inUV;\n"
            "}\n";

        static const char* fragmentShaderSrc =
            "#version 330 core\n"
            "in vec4 vDiffuse;\n"
            "in vec4 vSpecular;\n"
            "in vec2 vUV;\n"
            "\n"
            "uniform sampler2D texture0;\n"
            "uniform bool doTexture;\n"
            "uniform bool doFog;\n"
            "uniform vec3 fogColour;\n"
            "\n"
            "out vec4 fragColour;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    vec4 c = doTexture ? texture(texture0, vUV) * vDiffuse : vDiffuse;\n"
            "\n"
            "    // DR2 draws pre-transformed vertices, and for those D3D takes the\n"
            "    // fog factor from the specular alpha channel rather than from\n"
            "    // depth - 1 is unfogged, 0 is fully fogged.\n"
            "    if (doFog)\n"
            "    {\n"
            "        c.rgb = mix(fogColour, c.rgb, vSpecular.a);\n"
            "    }\n"
            "\n"
            "    fragColour = c;\n"
            "}\n";

        static GLuint program;
        static GLint uniScreenSize = -1;
        static GLint uniDoTexture = -1;
        static GLint uniDoFog = -1;
        static GLint uniFogColour = -1;

        static GLuint vao;
        static GLuint vbo;
        static GLuint ibo;

        //---------------------------------------------------------------------

        static GLuint CompileShader(GLenum type, const char* src, const char* what)
        {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            GLint ok = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char info[1024];
                GLsizei len = 0;
                glGetShaderInfoLog(shader, sizeof(info) - 1, &len, info);
                info[len < GLsizei(sizeof(info)) ? len : sizeof(info) - 1] = '\0';
                LOG_ERR(("OGL: %s shader failed to compile:", what));
                LOG_ERR(("OGL: %s", info));

                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        //---------------------------------------------------------------------

        static Bool InitShaders()
        {
            GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSrc, "vertex");
            if (!vs)
            {
                return FALSE;
            }

            GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc, "fragment");
            if (!fs)
            {
                glDeleteShader(vs);
                return FALSE;
            }

            program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);

            glDeleteShader(vs);
            glDeleteShader(fs);

            GLint ok = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char info[1024];
                GLsizei len = 0;
                glGetProgramInfoLog(program, sizeof(info) - 1, &len, info);
                info[len < GLsizei(sizeof(info)) ? len : sizeof(info) - 1] = '\0';
                LOG_ERR(("OGL: shader program failed to link:"));
                LOG_ERR(("OGL: %s", info));

                glDeleteProgram(program);
                program = 0;
                return FALSE;
            }

            uniScreenSize = glGetUniformLocation(program, "screenSize");
            uniDoTexture = glGetUniformLocation(program, "doTexture");
            uniDoFog = glGetUniformLocation(program, "doFog");
            uniFogColour = glGetUniformLocation(program, "fogColour");

            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "texture0"), 0);

            // VertexTL comes straight off the bucket memory, so the attribute
            // layout has to match it exactly - these keep that honest.
            static_assert(sizeof(VertexTL) == 32, "VertexTL layout changed; update the OGL vertex attributes");
            static_assert(offsetof(VertexTL, vv) == 0, "VertexTL layout changed");
            static_assert(offsetof(VertexTL, diffuse) == 16, "VertexTL layout changed");
            static_assert(offsetof(VertexTL, specular) == 20, "VertexTL layout changed");
            static_assert(offsetof(VertexTL, u) == 24, "VertexTL layout changed");

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ibo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

            const GLsizei stride = sizeof(VertexTL);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (const void*)0);

            // Color is b,g,r,a in memory; GL_BGRA as the size reorders it to
            // rgba for us rather than swizzling in the shader
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void*)16);

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void*)20);

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (const void*)24);

            glBindVertexArray(0);

            LOG_DIAG(("OGL: shaders ready"));

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

        //---------------------------------------------------------------------
        //
        // render state
        //

        static Bool zWrite = TRUE;

        static GLenum BlendFactor(U32 blend)
        {
            switch (blend)
            {
                case BLEND_ZERO: return GL_ZERO;
                case BLEND_ONE: return GL_ONE;
                case BLEND_SRCCOLOR: return GL_SRC_COLOR;
                case BLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
                case BLEND_SRCALPHA: return GL_SRC_ALPHA;
                case BLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
                case BLEND_DSTALPHA: return GL_DST_ALPHA;
                case BLEND_INVDSTALPHA: return GL_ONE_MINUS_DST_ALPHA;
                case BLEND_DSTCOLOR: return GL_DST_COLOR;
                case BLEND_INVDSTCOLOR: return GL_ONE_MINUS_DST_COLOR;
                case BLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
                default: return GL_ONE;
            }
        }

        static GLenum srcBlend = GL_SRC_ALPHA;
        static GLenum dstBlend = GL_ONE_MINUS_SRC_ALPHA;

        void ResetState(Bool)
        {
            glDisable(GL_SCISSOR_TEST);
            glDepthFunc(GL_LEQUAL);
            glDisable(GL_STENCIL_TEST);
            glBlendFunc(srcBlend, dstBlend);
        }

        void SetZBuffer(Bool doZBuffer, Bool)
        {
            if (doZBuffer)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
            zWrite = doZBuffer;
            glDepthMask(doZBuffer ? GL_TRUE : GL_FALSE);
        }

        void SetZWrite(Bool doZWrite)
        {
            zWrite = doZWrite;
            glDepthMask(doZWrite ? GL_TRUE : GL_FALSE);
        }

        Bool GetZWrite()
        {
            return zWrite;
        }

        void SetAlphaBlend(Bool doAlpha)
        {
            if (doAlpha)
            {
                glEnable(GL_BLEND);
            }
            else
            {
                glDisable(GL_BLEND);
            }
        }

        void SetCull(Bool doCull)
        {
            if (doCull)
            {
                glEnable(GL_CULL_FACE);
                // D3D culls counter-clockwise here, and the y flip in the
                // vertex shader reverses winding, so this lands on GL_BACK
                glCullFace(GL_BACK);
                glFrontFace(GL_CW);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
        }

        void SetSrcBlend(U32 blend)
        {
            srcBlend = BlendFactor(blend);
            glBlendFunc(srcBlend, dstBlend);
        }

        void SetDstBlend(U32 blend)
        {
            dstBlend = BlendFactor(blend);
            glBlendFunc(srcBlend, dstBlend);
        }

        // GL clips to the frustum unconditionally; there is no equivalent to
        // toggle, and D3DDP_DONOTCLIP was only ever a hint that the caller had
        // already done the work
        void SetClipping(Bool) {}

        // phase 5: lighting and shading become shader work
        void SetLighting(Bool) {}
        void SetShade(U32) {}

        // no equivalent in a core profile
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

        //---------------------------------------------------------------------
        //
        // fog
        //
        // The range is not needed here: the software transform has already
        // baked the per-vertex fog factor into the specular alpha channel, so
        // the shader only needs to know whether fog is on and what colour.
        //

        static Bool fogOn = FALSE;
        static F32 fogR = 0.0f, fogG = 0.0f, fogB = 0.0f;

        void SetFog(Bool on)
        {
            fogOn = on;
        }

        void SetFogColor(U32 color)
        {
            Color c;
            c.color = color;
            fogR = c.r / 255.0f;
            fogG = c.g / 255.0f;
            fogB = c.b / 255.0f;
        }

        void SetFogRange(F32, F32) {}

        // phase 5: ambient becomes a uniform once lighting moves to the shader
        void SetAmbientColor(U32) {}

        // phase 5: transforms become shader uniforms
        void SetWorldTransform(const Matrix&) {}
        void SetViewTransform(const Matrix&) {}
        void SetProjTransform(const Matrix&) {}

        // phase 5: materials and lights become shader uniforms
        void SetMaterial(const Material*) {}
        void SetLight(U32, Light::Obj&) {}
        void EnableLight(U32, Bool) {}

        //---------------------------------------------------------------------
        //
        // geometry
        //
        // Only the pre-transformed format is handled so far. The rest of the
        // vertex types need the transform and lighting pipeline, which is
        // phase 5; they return TRUE so the engine carries on as before.
        //

        static GLenum PrimitiveMode(PRIMITIVE_TYPE prim)
        {
            switch (prim)
            {
                case PT_POINTLIST: return GL_POINTS;
                case PT_LINELIST: return GL_LINES;
                case PT_LINESTRIP: return GL_LINE_STRIP;
                case PT_TRIANGLELIST: return GL_TRIANGLES;
                case PT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
                case PT_TRIANGLEFAN: return GL_TRIANGLE_FAN;
                default: return GL_TRIANGLES;
            }
        }

        // shared setup for both draw entry points; returns FALSE if this vertex
        // type is not handled yet
        static Bool BeginDraw(VERTEX_TYPE vert_type, const void* verts, U32 vert_count)
        {
            if (vert_type != FVF_TLVERTEX || !program || !hGLRC)
            {
                return FALSE;
            }

            glUseProgram(program);

            RECT client;
            GetClientRect(hWnd, &client);
            glUniform2f(uniScreenSize, F32(client.right), F32(client.bottom));
            glUniform1i(uniDoTexture, boundTexture[0] ? GL_TRUE : GL_FALSE);
            glUniform1i(uniDoFog, fogOn ? GL_TRUE : GL_FALSE);
            glUniform3f(uniFogColour, fogR, fogG, fogB);

            glBindVertexArray(vao);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // orphan and refill; a persistent ring buffer is a phase 6 concern
            glBufferData(GL_ARRAY_BUFFER, vert_count * sizeof(VertexTL), verts, GL_STREAM_DRAW);

            return TRUE;
        }

        static void EndDraw()
        {
            glBindVertexArray(0);
        }

        Bool DrawPrimitive
        (
            PRIMITIVE_TYPE prim_type,
            VERTEX_TYPE vert_type,
            void* verts,
            U32 vert_count,
            U32 flags
        )
        {
            flags;

            if (!BeginDraw(vert_type, verts, vert_count))
            {
                return TRUE;
            }

            glDrawArrays(PrimitiveMode(prim_type), 0, vert_count);

            EndDraw();

            return TRUE;
        }

        //---------------------------------------------------------------------

        Bool DrawIndexedPrimitive
        (
            PRIMITIVE_TYPE prim_type,
            VERTEX_TYPE vert_type,
            void* verts,
            U32 vert_count,
            const U16* indices,
            U32 index_count,
            U32 flags
        )
        {
            flags;

            if (!BeginDraw(vert_type, verts, vert_count))
            {
                return TRUE;
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(U16), indices, GL_STREAM_DRAW);

            glDrawElements(PrimitiveMode(prim_type), index_count, GL_UNSIGNED_SHORT, nullptr);

            EndDraw();

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
