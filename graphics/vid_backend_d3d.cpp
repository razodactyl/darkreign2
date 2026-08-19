///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vid_backend_d3d.cpp     DirectX 7 rendering backend
//
// Everything in here used to live inline in vidrend.cpp, vid_cmd_dialog.cpp,
// vid_math.cpp and light.cpp. It is the same code; it is now reached through
// the Vid::Backend table so a second backend can be dropped in beside it.
//
// The Vid::SetXState functions still own their renderState bookkeeping and
// return values - only the part that talks to the device lives here.
//
// See docs/research/graphics-backend-port.md.
//

#include "vid_private.h"
#include "vid_backend.h"
#include "vid_dx.h"
#include "light.h"
#include "main.h"
#include "console.h"
#include "statistics.h"
//-----------------------------------------------------------------------------

namespace Vid
{
    //-------------------------------------------------------------------------
    //
    // The neutral rendering vocabulary in vertex.h and vid_public.h carries the
    // same numeric values as the DirectX 7 constants it was derived from, so
    // this backend can hand them to D3D untranslated. That is only safe while
    // it stays true - these assert it at compile time.
    //
    // A backend whose values differ (OpenGL) translates in its own boundary
    // functions instead; nothing outside this file may assume the identity.
    //

    static_assert(PT_POINTLIST == D3DPT_POINTLIST, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");
    static_assert(PT_LINELIST == D3DPT_LINELIST, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");
    static_assert(PT_LINESTRIP == D3DPT_LINESTRIP, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");
    static_assert(PT_TRIANGLELIST == D3DPT_TRIANGLELIST, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");
    static_assert(PT_TRIANGLESTRIP == D3DPT_TRIANGLESTRIP, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");
    static_assert(PT_TRIANGLEFAN == D3DPT_TRIANGLEFAN, "PRIMITIVE_TYPE drifted from D3DPRIMITIVETYPE");

    static_assert(VF_XYZ == D3DFVF_XYZ, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_XYZRHW == D3DFVF_XYZRHW, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_NORMAL == D3DFVF_NORMAL, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_RESERVED1 == D3DFVF_RESERVED1, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_DIFFUSE == D3DFVF_DIFFUSE, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_SPECULAR == D3DFVF_SPECULAR, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_TEX1 == D3DFVF_TEX1, "VERTEX_FORMAT drifted from D3DFVF");
    static_assert(VF_TEX2 == D3DFVF_TEX2, "VERTEX_FORMAT drifted from D3DFVF");

    static_assert(DP_WAIT == D3DDP_WAIT, "DRAWPRIMITIVE_FLAGS drifted from D3DDP");
    static_assert(DP_DONOTCLIP == D3DDP_DONOTCLIP, "DRAWPRIMITIVE_FLAGS drifted from D3DDP");
    static_assert(DP_DONOTUPDATEEXTENTS == D3DDP_DONOTUPDATEEXTENTS, "DRAWPRIMITIVE_FLAGS drifted from D3DDP");
    static_assert(DP_DONOTLIGHT == D3DDP_DONOTLIGHT, "DRAWPRIMITIVE_FLAGS drifted from D3DDP");

    static_assert(TA_WRAP == D3DTADDRESS_WRAP, "TEXTURE_ADDRESS drifted from D3DTEXTUREADDRESS");
    static_assert(TA_MIRROR == D3DTADDRESS_MIRROR, "TEXTURE_ADDRESS drifted from D3DTEXTUREADDRESS");
    static_assert(TA_CLAMP == D3DTADDRESS_CLAMP, "TEXTURE_ADDRESS drifted from D3DTEXTUREADDRESS");

    static_assert(BLEND_ZERO == D3DBLEND_ZERO, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_ONE == D3DBLEND_ONE, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_SRCCOLOR == D3DBLEND_SRCCOLOR, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_INVSRCCOLOR == D3DBLEND_INVSRCCOLOR, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_SRCALPHA == D3DBLEND_SRCALPHA, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_INVSRCALPHA == D3DBLEND_INVSRCALPHA, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_DSTALPHA == D3DBLEND_DESTALPHA, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_INVDSTALPHA == D3DBLEND_INVDESTALPHA, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_DSTCOLOR == D3DBLEND_DESTCOLOR, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_INVDSTCOLOR == D3DBLEND_INVDESTCOLOR, "BLEND_FACTOR drifted from D3DBLEND");
    static_assert(BLEND_SRCALPHASAT == D3DBLEND_SRCALPHASAT, "BLEND_FACTOR drifted from D3DBLEND");

    static_assert(clearBACK == D3DCLEAR_TARGET, "ClearFlags drifted from D3DCLEAR");
    static_assert(clearZBUFFER == D3DCLEAR_ZBUFFER, "ClearFlags drifted from D3DCLEAR");
    static_assert(clearSTENCIL == D3DCLEAR_STENCIL, "ClearFlags drifted from D3DCLEAR");

    //-------------------------------------------------------------------------

    namespace BackendD3D
    {
        //---------------------------------------------------------------------
        //
        // texture stage blend setup
        //
        // One function per RS_TEX_* op. ValidateBlends probes the device and
        // rewrites blendToOp to drop any the hardware cannot actually do.
        //
        void TexDecal(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: color"));

                //      dxError = device->SetTextureStageState( stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE); 
                //      dxError = device->SetTextureStageState( stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexDecalSimple(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexDecalAlpha(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexModulate(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexModulateAlpha(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexModulateAlpha2x(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexModulateAlpha4x(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE4X);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE4X);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexAddAlpha(U32 stage)
        {
            if (stage)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_ADD);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
            else
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: color"));

                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

                LOG_DXERR(("SetTextureStageState: alpha"));
            }
        }

        //----------------------------------------------------------------------------

        void TexAddSignedAlpha(U32 stage)
        {
            dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_ADDSIGNED);
            dxError = device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

            LOG_DXERR(("SetTextureStageState: color"));

            dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

            LOG_DXERR(("SetTextureStageState: alpha"));
        }

        //----------------------------------------------------------------------------

        typedef void (*TexBlendProcP)(U32 stage);    // texture blend setup function
        static TexBlendProcP blendToOp[] =
        {
            0,
            TexDecal,            //  RS_TEX_DECAL          
            TexDecalAlpha,       //  RS_TEX_DECALALPHA
            TexModulateAlpha,    //  RS_TEX_MODULATE
            TexModulateAlpha2x,  //  RS_TEX_MODULATE2X
            TexModulateAlpha4x,  //  RS_TEX_MODULATE4X
            TexModulateAlpha,    //  RS_TEX_MODULATEALPHA
            TexAddAlpha          //  RS_TEX_ADD
        };

        static Bool blendValidation[8][2];
        static char* blendName[8] =
        {
            "",
            "decal",
            "decalAlpha",
            "modulate",
            "modulate2x",
            "modulate4x",
            "modulateAlpha",
            "add"
        };

        //---------------------------------------------------------------------

        void SetTexBlend(U32 op, U32 stage)
        {
            blendToOp[op](stage);
        }

        //---------------------------------------------------------------------

        void ValidateBlends()
        {
            Bitmap* tex = Bitmap::Manager::FindCreate(Bitmap::reduceNONE, "library\\engine\\engine_pandemic.pic");

            // reset
            Utils::Memset(blendValidation, 0, sizeof(blendValidation));
            blendToOp[1] = TexDecal;            //  RS_TEX_DECAL          
            blendToOp[2] = TexDecalAlpha;       //  RS_TEX_DECALALPHA
            blendToOp[3] = TexModulateAlpha;    //  RS_TEX_MODULATE
            blendToOp[4] = TexModulateAlpha2x;  //  RS_TEX_MODULATE2X
            blendToOp[5] = TexModulateAlpha4x;  //  RS_TEX_MODULATE4X
            blendToOp[6] = TexModulateAlpha;    //  RS_TEX_MODULATEALPHA
            blendToOp[7] = TexAddAlpha;         //  RS_TEX_ADD

            Bool hit = FALSE;

            // TEX_MODULATE
            //
            Vid::SetTextureI(tex, 0, 3 << RS_TEX_SHIFT);
            DWORD passes;
            blendValidation[3][0] = device->ValidateDevice(&passes) == D3D_OK;

            if (!blendValidation[3][0])
            {
                // can't do D3DTSS_ALPHAOP D3DTOP_MODULATE
                //
                blendToOp[3] = TexModulate;
                Vid::SetTextureI(tex, 0, 3 << RS_TEX_SHIFT);
                blendValidation[3][0] = device->ValidateDevice(&passes) == D3D_OK;

                caps.noAlphaMod = TRUE;

                if (!hit)
                {
                    LOG_DIAG((""));
                    hit = TRUE;
                }
                LOG_DIAG(("tex : ! stage 0 TexModulateAlpha invalid replacing with TexModulate"));
            }

            for (U32 i = 1; i < 8; i++)
            {
                Vid::SetTextureI(tex, 0, i << RS_TEX_SHIFT);

                blendValidation[i][0] = device->ValidateDevice(&passes) == D3D_OK;
                if (!blendValidation[i][0])
                {
                    // find a good backup
                    //
                    S32 j = i - 1;
                    if (i == 7 || i == 1) // RS_TEX_ADD || RS_TEX_DECAL
                    {
                        j = 5;    // modulate4x
                    }
                    while (j > 0 && !blendValidation[j][0]) j--;
                    if (j == 0)
                    {
                        j = 3;
                    }
                    blendToOp[i] = blendToOp[j];

                    if (!hit)
                    {
                        LOG_DIAG((""));
                        hit = TRUE;
                    }
                    LOG_DIAG(("tex : ! stage 0 blend %s invalid replacing with %s", blendName[i], blendName[j]));
                }
            }

            if (caps.texMulti)
            {
                // check for useful texMultiure modes
                //
                Bool hhit = FALSE;
                for (U32 i = 1; i < 8; i++)
                {
                    Vid::SetTextureI(tex, 0, RS_BLEND_MODULATE);
                    Vid::SetTextureI(tex, 1, i << RS_TEX_SHIFT);

                    DWORD passes;
                    blendValidation[i][1] = device->ValidateDevice(&passes) == D3D_OK;
                    if (!blendValidation[i][1])
                    {
                        if (!hit)
                        {
                            LOG_DIAG((""));
                            hit = TRUE;
                        }
                        LOG_DIAG(("tex : ! stage 1 blend %s invalid", blendName[i]));
                    }
                    else if (!hhit)
                    {
                        hhit = TRUE;
                    }
                }

                if (!hhit)
                {
                    Vid::SetTextureI(tex, 0, RS_BLEND_MODULATE);
                    Vid::SetTextureI(tex, 1, RS_BLEND_ADD);
                    TexDecalSimple(0);
                    TexAddAlpha(1);
                    DWORD passes;
                    if (device->ValidateDevice(&passes) != D3D_OK)
                    {
                        if (!hit)
                        {
                            LOG_DIAG((""));
                            hit = TRUE;
                        }
                        LOG_DIAG(("tex : ! stage 1 blend decalsimple x add invalid"));
                    }
                }

                if (!blendValidation[RS_BLEND_DECAL >> RS_TEX_SHIFT][1]
                    || !blendValidation[RS_BLEND_ADD >> RS_TEX_SHIFT][1])
                {
                    // useless video trash
                    //
                    if (!hit)
                    {
                        LOG_DIAG((""));
                        hit = TRUE;
                    }
                    Var::varMultiTex = caps.texMulti = CurDD().texMulti = CurD3D().texMulti = FALSE;
                    LOG_DIAG(("tex : ! multitex doesn't handle required blends: canceling"));
                }

                SetTextureI(NULL, 1, RS_BLEND_DEF);
            }
            SetTextureI(NULL, 0, RS_BLEND_DEF);
            if (hit)
            {
                LOG_DIAG((""));
            }
        }

        //-----------------------------------------------------------------------------

        Bool ValidateBlend(U32 blend, U32 stage) // = 0)
        {
            blend = (blend & RS_TEX_MASK) >> RS_TEX_SHIFT;
            ASSERT(blend > 0);
            return blendValidation[blend][stage];
        }

        //---------------------------------------------------------------------
        //
        // frame
        //

        Bool BeginScene()
        {
            dxError = device->BeginScene();
            if (dxError)
            {
                LOG_DXERR(("BeginScene: device->BeginScene"));
            }
            return dxError == DD_OK;
        }

        //---------------------------------------------------------------------

        Bool EndScene()
        {
            dxError = device->EndScene();
            LOG_DXERR(("EndScene: device->EndScene"));

            return dxError == DD_OK;
        }

        //---------------------------------------------------------------------

        void Clear(U32 clearFlags, Color color, const Area<S32>& rect)
        {
            dxError = device->Clear(1UL, (LPD3DRECT)&rect, clearFlags, color, 1, 0);
            LOG_DXERR(("Vid::ClearD3D: viewport->Clear2"));
        }

        //---------------------------------------------------------------------

        Bool SetViewport(const ViewPort& desc)
        {
            if (!device)
            {
                return FALSE;
            }

            ViewPortDescD3D d3dDesc;
            d3dDesc.dwX = desc.x;
            d3dDesc.dwY = desc.y;
            d3dDesc.dwWidth = desc.width;
            d3dDesc.dwHeight = desc.height;
            d3dDesc.dvMinZ = desc.minZ;
            d3dDesc.dvMaxZ = desc.maxZ;

            device->SetViewport(&d3dDesc);

            return TRUE;
        }

        //---------------------------------------------------------------------
        //
        // one-off device defaults, applied by Vid::SetRenderState
        //

        void ResetState(Bool wbuffer)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_ZFUNC, wbuffer ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL);
            LOG_DXERR(("device->SetRenderState"));

            dxError = device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
            //	dxError = device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
            //  dxError = device->SetRenderState(D3DRENDERSTATE_ALPHAREF, (DWORD)0);
            LOG_DXERR(("device->SetRenderState: transparent"));

            dxError = device->SetRenderState(D3DRENDERSTATE_CLIPPING, 1);
            LOG_DXERR(("device->SetRenderState( CLIPPING)"));

            dxError = device->SetRenderState(D3DRENDERSTATE_EXTENTS, 0);
            LOG_DXERR(("device->SetRenderState( EXTENTS)"));

            dxError = device->SetRenderState(D3DRENDERSTATE_LIGHTING, 0);
            LOG_DXERR(("device->SetRenderState( LIGHTING)"));

            //    dxError = device->SetRenderState( D3DRENDERSTATE_COLORVERTEX, renderState.status.dxTL );
            //    LOG_DXERR( ("device->SetRenderState( COLORVERTEX)") );
        }

        //---------------------------------------------------------------------
        //
        // depth, blending, rasterization
        //

        void SetZBuffer(Bool doZBuffer, Bool wbuffer)
        {
            U32 flag = doZBuffer ? D3DZB_TRUE : D3DZB_FALSE;
            if (wbuffer)
            {
                flag |= D3DZB_USEW;
            }
            dxError = device->SetRenderState(D3DRENDERSTATE_ZENABLE, flag);
            dxError = device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, doZBuffer);
            LOG_DXERR(("SetZBufferState"));
        }

        //---------------------------------------------------------------------

        void SetZWrite(Bool doZWrite)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, doZWrite);
            LOG_DXERR(("SetZWriteState"));
        }

        //---------------------------------------------------------------------

        Bool GetZWrite()
        {
            U32 retValue;
            device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &retValue);

            return (Bool)retValue;
        }

        //---------------------------------------------------------------------

        void SetAlphaBlend(Bool doAlpha)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, doAlpha);
            LOG_DXERR(("device->SetRenderState"));
        }

        //---------------------------------------------------------------------

        void SetCull(Bool doCull)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_CULLMODE, doCull ? D3DCULL_CCW : D3DCULL_NONE);
            LOG_DXERR(("SetCullState"));
        }

        //---------------------------------------------------------------------

        void SetSrcBlend(U32 blend)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_SRCBLEND, blend);
            LOG_DXERR(("SetSrcBlendState"));
        }

        //---------------------------------------------------------------------

        void SetDstBlend(U32 blend)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_DESTBLEND, blend);
            LOG_DXERR(("SetDstBlendState"));
        }

        //---------------------------------------------------------------------

        void SetClipping(Bool doClip)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_CLIPPING, (DWORD)(doClip ? 1 : 0));
            LOG_DXERR(("device->SetRenderState( CLIPPING)"));
        }

        //---------------------------------------------------------------------

        void SetLighting(Bool doLighting)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_LIGHTING, DWORD(doLighting));
            LOG_DXERR(("device->SetRenderState( LIGHTING)"));
        }

        //---------------------------------------------------------------------

        void SetShade(U32 shadeFlags)
        {
            ASSERT(device);
            dxError = device->SetRenderState(D3DRENDERSTATE_FILLMODE, shadeFlags == shadeWIRE ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
            LOG_DXERR(("FILLSTATE"));
            dxError = device->SetRenderState(D3DRENDERSTATE_SHADEMODE, shadeFlags == shadeFLAT ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);
            LOG_DXERR(("SHADESTATE"));
        }

        //---------------------------------------------------------------------

        void SetDither(Bool doDither)
        {
            ASSERT(device);
            dxError = device->SetRenderState(D3DRENDERSTATE_DITHERENABLE, (DWORD)doDither);
            LOG_DXERR(("device->SetRenderState: dither"));
        }

        //---------------------------------------------------------------------

        // note: Vid::SetSpecularStateI keeps its own DOSPECULAR guard around the
        // call to this - SetRenderState calls it unguarded, and always has
        //
        void SetSpecular(Bool doSpecular)
        {
            ASSERT(device);
            dxError = device->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, doSpecular);
            LOG_DXERR(("device->SetRenderState"));
        }

        //---------------------------------------------------------------------

        void SetAntiAlias(Bool on)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_ANTIALIAS, on);
            LOG_DXERR(("device->SetRenderState"));
        }

        //---------------------------------------------------------------------

        void SetEdgeAntiAlias(Bool on)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_EDGEANTIALIAS, on);
            LOG_DXERR(("device->SetRenderState"));
        }

        //---------------------------------------------------------------------

        Bool SetPerspective(Bool on)
        {
            Bool retValue = 0;
            device->GetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, (DWORD*)&retValue);

            dxError = device->SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, on);
            LOG_DXERR(("device->SetRenderState"));

            return retValue;
        }

        //---------------------------------------------------------------------

        Bool SetColorKey(Bool on)
        {
            Bool retValue = 0;
            device->GetRenderState(D3DRENDERSTATE_COLORKEYENABLE, (DWORD*)&retValue);

            dxError = device->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, on);
            LOG_DXERR(("device->SetRenderState( COLORKEYENABLE)"));

            return retValue;
        }

        //---------------------------------------------------------------------
        //
        // texture stages
        //

        void SetTexWrap(U32 mode, U32 stage)
        {
            dxError = device->SetTextureStageState(stage, D3DTSS_ADDRESS, mode + 1);
            LOG_DXERR(("SetTexWrapState"));
        }

        //---------------------------------------------------------------------

        void SetTexFilter(U32 filterFlags)
        {
            D3DTEXTUREMAGFILTER magFilter;
            D3DTEXTUREMINFILTER minFilter;

            if (filterFlags & filterFILTER)
            {
                magFilter = D3DTFG_LINEAR;
                minFilter = D3DTFN_LINEAR;
            }
            else
            {
                magFilter = D3DTFG_POINT;
                minFilter = D3DTFN_POINT;
            }

            S32 stage = 1;    // set both stages (DR2 only uses 2)

            dxError = device->SetTextureStageState(stage, D3DTSS_MAGFILTER, magFilter);
            dxError = device->SetTextureStageState(stage, D3DTSS_MINFILTER, minFilter);
            LOG_DXERR(("SetFilterState"));

            dxError = device->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTFP_NONE);
            //        dxError = device->SetTextureStageState( stage, D3DTSS_MIPFILTER, D3DTFP_POINT);

            LOG_DXERR(("SetFilterState"));

            for (; stage >= 0; stage--)
            {
                dxError = device->SetTextureStageState(stage, D3DTSS_MAGFILTER, magFilter);
                dxError = device->SetTextureStageState(stage, D3DTSS_MINFILTER, minFilter);
                LOG_DXERR(("SetFilterState"));

                if (!(filterFlags & filterMIPMAP))
                {
                    dxError = device->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTFP_NONE);
                }
                else if (!(filterFlags & filterMIPFILTER))
                {
                    dxError = device->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTFP_POINT);
                }
                else
                {
                    dxError = device->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTFP_LINEAR);
                }
                LOG_DXERR(("SetFilterState"));
            }
        }

        //---------------------------------------------------------------------

        void SetTextureFactor(Color color)
        {
            dxError = device->SetRenderState
            (
                D3DRENDERSTATE_TEXTUREFACTOR,
                D3DRGBA(color.r, color.g, color.b, color.a)
            );
            LOG_DXERR(("SetTextureFactor"));
        }

        //---------------------------------------------------------------------

        Bool BindTexture(const Bitmap* tex, U32 stage)
        {
            TextureHandle texH = tex ? tex->GetTexture() : NULL;

            dxError = device->SetTexture(stage, texH);
            LOG_DXERR(("SetRenderState( mat): device->RenderState( TEXTUREHANDLE)"));

            return dxError == DD_OK;
        }

        //---------------------------------------------------------------------

        Bool DisableTexStage(U32 stage)
        {
            dxError = device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
            dxError = device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

            LOG_DXERR(("SetTextureStageState()"));

            return dxError == DD_OK;
        }

        //---------------------------------------------------------------------
        //
        // texture storage
        //
        // D3D samples out of the Bitmap's own DirectDraw surface, so there is
        // no separate texture object to manage.
        //

        U32 TextureCreate()
        {
            return 0;
        }

        //---------------------------------------------------------------------

        void TextureDestroy(U32)
        {
        }

        //---------------------------------------------------------------------

        void TextureUpload(U32, const void*, S32, S32, S32, Bool)
        {
        }

        //---------------------------------------------------------------------
        //
        // fog and ambient light
        //

        void SetFog(Bool fogOn)
        {
            ASSERT(device);
            dxError = device->SetRenderState(D3DRENDERSTATE_FOGENABLE, fogOn);
            LOG_DXERR(("SetFogState"));

#ifndef DODXLEANANDGRUMPY
            //    dxError = device->SetRenderState( D3DRENDERSTATE_FOGTABLEMODE,  D3DFOG_NONE);
            dxError = device->SetRenderState(D3DRENDERSTATE_FOGVERTEXMODE, fogOn ? D3DFOG_LINEAR : D3DFOG_NONE);
            LOG_DXERR(("SetFogState"));
#endif
        }

        //---------------------------------------------------------------------

        void SetFogColor(U32 fogColor)
        {
            dxError = device->SetRenderState(D3DRENDERSTATE_FOGCOLOR, fogColor);
            LOG_DXERR(("SetFogColorI"));
        }

        //---------------------------------------------------------------------

        void SetFogRange(F32 min, F32 max)
        {
#ifndef DODXLEANANDGRUMPY
            dxError = device->SetRenderState(D3DRENDERSTATE_FOGSTART, *((U32*)&min));
            LOG_DXERR(("SetFogRange"));
            dxError = device->SetRenderState(D3DRENDERSTATE_FOGEND, *((U32*)&max));
            LOG_DXERR(("SetFogRange"));

            /*
              //  if (caps.fogPixel)
                  dxError = device->SetRenderState( D3DRENDERSTATE_FOGTABLESTART, *((U32 *) &renderState.fogMin));
                  LOG_DXERR( ("SetFogRange") );
                  dxError = device->SetRenderState( D3DRENDERSTATE_FOGTABLEEND,   *((U32 *) &renderState.fogMax));
                  LOG_DXERR( ("SetFogRange") );
                    dxError = device->SetRenderState( D3DRENDERSTATE_FOGTABLEDENSITY, *((U32 *) &renderState.fogDensity));

              //  else if (caps.fogVertex)
            */
#else
            min;
            max;
#endif
        }

        //---------------------------------------------------------------------

        void SetAmbientColor(U32 color)
        {
            color;
#ifndef DODXLEANANDGRUMPY
            dxError = device->SetRenderState(D3DRENDERSTATE_AMBIENT, (D3DCOLOR)color);
            LOG_DXERR(("SetAmbientColor"));
#endif
        }

        //---------------------------------------------------------------------
        //
        // transforms
        //

        void SetWorldTransform(const Matrix& mat)
        {
            dxError = device->SetTransform(D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)&mat);
            LOG_DXERR(("device->SetTransform: world"));
        }

        //---------------------------------------------------------------------

        void SetViewTransform(const Matrix& mat)
        {
            dxError = device->SetTransform(D3DTRANSFORMSTATE_VIEW, (D3DMATRIX*)&mat);
            LOG_DXERR(("device->SetTransform: view."));
        }

        //---------------------------------------------------------------------

        void SetProjTransform(const Matrix& mat)
        {
            dxError = device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, (D3DMATRIX*)&mat);
            LOG_DXERR(("device->SetTransform: project."));
        }

        //---------------------------------------------------------------------
        //
        // materials and lights
        //

        void SetMaterial(const Material* mat)
        {
            const MaterialDescD3D& matD3D = mat->GetDesc();

            dxError = device->SetMaterial((MaterialDescD3D*)&matD3D);
            LOG_DXERR(("device->SetMaterial"));
        }

        //---------------------------------------------------------------------

        void SetLight(U32 index, Light::Obj& light)
        {
            dxError = device->SetLight(index, light.D3D());
            LOG_DXERR(("Light::SetActiveList: device->SetLight"));
        }

        //---------------------------------------------------------------------

        void EnableLight(U32 index, Bool on)
        {
            dxError = device->LightEnable(index, on);
            LOG_DXERR(("Light::SetActiveList: device->EnableLight"));
        }

        //---------------------------------------------------------------------
        //
        // geometry
        //

        Bool DrawPrimitive
        (
            PRIMITIVE_TYPE prim_type,
            VERTEX_TYPE vert_type,
            void* verts,
            U32 vert_count,
            U32 flags
        )
        {
            dxError = device->DrawPrimitive
            (
                (D3DPRIMITIVETYPE)prim_type,
                vert_type,
                verts,
                vert_count,
                flags
            );
            LOG_DXERR(("device->DrawPrimitive: trilist"));

            return (dxError == D3D_OK);
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
            dxError = device->DrawIndexedPrimitive
            (
                (D3DPRIMITIVETYPE)prim_type,
                vert_type,
                verts,
                vert_count,
                (LPWORD)indices,
                index_count,
                flags
            );
#ifdef DEVELOPMENT
            if (dxError)
            {
                LOG_DXERR(("device->DrawIndexedPrimitive: trilist"));
            }
#else
            LOG_DXERR(("device->DrawIndexedPrimitive: trilist"));
#endif

            return (dxError == D3D_OK);
        }

        //---------------------------------------------------------------------
    };

    //-------------------------------------------------------------------------

    const Backend backendD3D =
    {
        "DirectX 7",
        backendDX7,

        BackendD3D::BeginScene,
        BackendD3D::EndScene,
        BackendD3D::Clear,
        BackendD3D::SetViewport,

        BackendD3D::ResetState,

        BackendD3D::SetZBuffer,
        BackendD3D::SetZWrite,
        BackendD3D::GetZWrite,
        BackendD3D::SetAlphaBlend,
        BackendD3D::SetCull,
        BackendD3D::SetSrcBlend,
        BackendD3D::SetDstBlend,
        BackendD3D::SetClipping,
        BackendD3D::SetLighting,
        BackendD3D::SetShade,
        BackendD3D::SetDither,
        BackendD3D::SetSpecular,
        BackendD3D::SetAntiAlias,
        BackendD3D::SetEdgeAntiAlias,

        BackendD3D::SetPerspective,
        BackendD3D::SetColorKey,

        BackendD3D::SetTexBlend,
        BackendD3D::SetTexWrap,
        BackendD3D::SetTexFilter,
        BackendD3D::SetTextureFactor,
        BackendD3D::BindTexture,
        BackendD3D::DisableTexStage,

        BackendD3D::TextureCreate,
        BackendD3D::TextureDestroy,
        BackendD3D::TextureUpload,

        BackendD3D::ValidateBlends,
        BackendD3D::ValidateBlend,

        BackendD3D::SetFog,
        BackendD3D::SetFogColor,
        BackendD3D::SetFogRange,
        BackendD3D::SetAmbientColor,

        BackendD3D::SetWorldTransform,
        BackendD3D::SetViewTransform,
        BackendD3D::SetProjTransform,

        BackendD3D::SetMaterial,
        BackendD3D::SetLight,
        BackendD3D::EnableLight,

        BackendD3D::DrawPrimitive,
        BackendD3D::DrawIndexedPrimitive,
    };

    //-------------------------------------------------------------------------

    // default until SelectBackend says otherwise, so that anything running
    // before Vid::Init still has a valid table
    const Backend* backend = &backendD3D;

    //-------------------------------------------------------------------------

    void SelectBackend(BackendId id)
    {
        switch (id)
        {
            case backendDX7:
                backend = &backendD3D;
                break;

            case backendOGL:
                backend = &backendGL;
                break;

            default:
                ERR_FATAL(("SelectBackend: unknown backend %d", id));
        }

        LOG_DIAG(("Vid: %s backend selected", backend->name));
    }

    //-------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------
