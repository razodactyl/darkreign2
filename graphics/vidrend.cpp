///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vidrend.cpp      non-setup 3d rendering specific Vid stuff
//
// 22-APR-1998
//

#include "vid_private.h"
#include "vid_backend.h"
#include "main.h"
#include "console.h"
#include "mesh.h"
#include "perfstats.h"
#include "statistics.h"

//-----------------------------------------------------------------------------

namespace Vid
{
    const F32 ZBIAS = 1.0f;

    extern VarFloat farPlane;
    extern VarFloat fogRange;

    Bool AreVerticesInRange(const VertexTL* vertex, S32 vertexCount, const U16* index, S32 indexCount, U32 clipFlags, Bool guard) // = TRUE
    {
        int result = 1;

        Area<S32> range = viewRect;
        if (guard && Vid::renderState.status.clipGuard)
        {
            range.p0.x -= renderState.clipGuardSize;
            range.p0.y -= renderState.clipGuardSize;
            range.p1.x += renderState.clipGuardSize;
            range.p1.y += renderState.clipGuardSize;
        }

        const VertexTL *v, *ve = vertex + vertexCount;
        for (v = vertex; v < ve; v++)
        {
            if (clipFlags & DP_DONOTCLIP)
            {
                if (
                    S32(v->vv.x) >= range.p0.x && S32(v->vv.x) <= range.p1.x
                    && S32(v->vv.y) >= range.p0.y && S32(v->vv.y) <= range.p1.y
                    && S32(v->vv.z) >= 0 && S32(v->vv.z) <= 1
                )
                {
                    continue;
                }
                else
                {
                    result = 0;
                    LOG_DIAG(("vert out of range %f,%f,%f: cur %d; count %d", v->vv.x, v->vv.y, v->vv.z, v - vertex, vertexCount));
                    //          ERR_FATAL( ("vert out of range"));
                }
            }
            else if (S32(v->vv.z) >= 0 && S32(v->vv.z) <= 1.0f)
            {
                continue;
            }
            else
            {
                result = 0;

                LOG_DIAG(("vert z out of range %f: tex: %s", v->vv.z));
                //        ERR_FATAL( ("vert z out of range"));
            }
        }
        if (index)
        {
            const U16 *i, *ie = index + indexCount;
            for (i = index; i < ie; i++)
            {
                if (*i >= vertexCount)
                {
                    result = 0;

                    LOG_DIAG(("index out of range: tex"));
                    //          ERR_FATAL( ("index out of range"));
                }
            }
        }

        return result;
    }

    //----------------------------------------------------------------------------

    Bool SetZBufferState(Bool doZBuffer)
    {
        Bool retValue = renderState.status.zbuffer;

        renderState.status.zbuffer = doZBuffer;

        backend->SetZBuffer(doZBuffer, renderState.status.wbuffer);

        return retValue;
    }

    //----------------------------------------------------------------------------

    Bool SetZWriteState(Bool doZWrite)
    {
        Bool retValue = backend->GetZWrite();

        backend->SetZWrite(doZWrite);

        return retValue;
    }

    //----------------------------------------------------------------------------

    void SetCullState(Bool doCull)
    {
        backend->SetCull(doCull);
    }

    //----------------------------------------------------------------------------

    Bool SetAlphaState(Bool doAlpha)
    {
        Bool retValue = renderState.status.alpha;

        renderState.status.alpha = doAlpha;

        backend->SetAlphaBlend(doAlpha);

        return retValue;
    }

    //----------------------------------------------------------------------------

    U32 SetSrcBlendState(U32 flags)
    {
        U32 lastflags = renderState.renderFlags & RS_SRC_MASK;
        renderState.renderFlags &= ~RS_SRC_MASK;

        flags &= RS_SRC_MASK;
        renderState.renderFlags |= flags;
        flags >>= RS_SRC_SHIFT;

        backend->SetSrcBlend(flags);

        return lastflags;
    }

    //-----------------------------------------------------------------------------

    U32 SetDstBlendState(U32 flags)
    {
        U32 lastflags = renderState.renderFlags & RS_DST_MASK;
        renderState.renderFlags &= ~RS_DST_MASK;

        flags &= RS_DST_MASK;
        renderState.renderFlags |= flags;
        flags >>= RS_DST_SHIFT;

        backend->SetDstBlend(flags);

        return lastflags;
    }

    //-----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    U32 SetTexWrapState(U32 flags, U32 stage) // = 0
    {
        U32 lastFlags = renderState.renderFlags & RS_ADD_MASK;
        renderState.renderFlags &= ~RS_ADD_MASK;

        U32 flag = flags & RS_ADD_MASK;
        renderState.renderFlags |= flag;

        backend->SetTexWrap(flag >> RS_ADD_SHIFT, stage);

        return lastFlags;
    }

    //----------------------------------------------------------------------------

    U32 SetTexBlendState(U32 flags, U32 stage) // = 0
    {
        U32 lastflags = renderState.renderFlags & RS_TEX_MASK;
        renderState.renderFlags &= ~RS_TEX_MASK;

        flags &= RS_TEX_MASK;
        renderState.renderFlags |= flags;
        flags >>= RS_TEX_SHIFT;

#ifdef DEVELOPMENT
        if (flags == 0 || flags > 7)
        {
            ERR_FATAL(("bad tex blend %d", flags));
        }
#endif

        backend->SetTexBlend(flags, stage);

        return lastflags;
    }

    //-----------------------------------------------------------------------------

    void ClearBack(Color color, Area<S32>* rect) // = 0, = NULL
    {
        if (isStatus.ogl)
        {
            // no back surface to blit into; the backend clears its own buffer
            RenderClear(clearBACK, color, rect);
            return;
        }

        backBmp.Clear(color, rect);
    }

    //-----------------------------------------------------------------------------

    void RenderClear(U32 clearFlags, Color color, Area<S32>* rect) // = NULL
    {
        Area<S32> temp;
        if (!rect)
        {
            temp.SetSize(viewDesc.x, viewDesc.y, viewDesc.x + viewDesc.width, viewDesc.y + viewDesc.height);
            rect = &temp;
        }

        backend->Clear(clearFlags, color, *rect);
    }

    //-----------------------------------------------------------------------------

    void ValidateBlends()
    {
        backend->ValidateBlends();
    }

    //-----------------------------------------------------------------------------

    Bool ValidateBlend(U32 blend, U32 stage) // = 0)
    {
        return backend->ValidateBlend(blend, stage);
    }

    //-----------------------------------------------------------------------------

    void SetTextureFactor(Color color)
    {
        backend->SetTextureFactor(color);
    }

    //-----------------------------------------------------------------------------

    Bool SetCamera(Camera& cam, Bool force) // = FALSE)
    {
        if (!force && curCamera == &cam)
        {
            // already set
            return TRUE;
        }
        curCamera = &cam;

        Vid::clipRect.Set((F32)cam.ViewRect().p0.x, (F32)cam.ViewRect().p0.y, (F32)cam.ViewRect().p1.x, (F32)cam.ViewRect().p1.y);

        viewDesc.x = cam.ViewRect().p0.x;
        viewDesc.y = cam.ViewRect().p0.y;
        viewDesc.width = cam.ViewRect().Width();
        viewDesc.height = cam.ViewRect().Height();

        // clip to view rect
        //
        if (cam.ViewRect().p0.x < 0)
        {
            viewDesc.width += cam.ViewRect().p0.x;
            viewDesc.x = 0;
            Vid::clipRect.p0.x = 0;
        }
        else if (cam.ViewRect().p0.x >= viewRect.Width())
        {
            viewDesc.width = 0;
            viewDesc.x = viewRect.Width() - 1;
            Vid::clipRect.p0.x = (F32)viewDesc.x;
        }
        if (cam.ViewRect().p0.y < 0)
        {
            viewDesc.height += cam.ViewRect().p0.y;
            viewDesc.y = 0;
            Vid::clipRect.p0.y = 0;
        }
        else if (cam.ViewRect().p0.y >= viewRect.Height())
        {
            viewDesc.height = 0;
            viewDesc.y = viewRect.Height() - 1;
            Vid::clipRect.p0.y = (F32)viewDesc.y;
        }
        if (S32(viewDesc.x + viewDesc.width) > viewRect.p1.x)
        {
            S32 dx = viewRect.p1.x - (viewDesc.x + viewDesc.width);
            viewDesc.width += dx;
            Vid::clipRect.p1.x += (F32)dx;
        }
        if (S32(viewDesc.y + viewDesc.height) > viewRect.p1.y)
        {
            S32 dy = viewRect.p1.y - (viewDesc.y + viewDesc.height);
            viewDesc.height += dy;
            Vid::clipRect.p1.y += (F32)dy;
        }
        viewDesc.minZ = 0.0f;
        viewDesc.maxZ = 1.0f;

        backend->SetViewport(viewDesc);

        Setup(*curCamera);

        //  reset fog
        //
        Vid::SetFogDepth(Vid::renderState.fogDepth);

        // reset bucket sorting
        //
        Vid::SetTranBucketMinZ(curCamera->NearPlane());
        Vid::SetTranBucketMaxZ(curCamera->FarPlane() * curCamera->Zoom());

        return TRUE;
    }

    //-----------------------------------------------------------------------------

    void ClipScreen()
    {
        ViewPort desc = Vid::viewDesc;

        desc.width = Vid::viewRect.Width();
        desc.x = 0;
        desc.height = Vid::viewRect.Height();
        desc.y = 0;
        desc.minZ = 0.0f;
        desc.maxZ = 1.0f;

        if (backend->SetViewport(desc))
        {
            Vid::clipRect.Set((F32)viewRect.p0.x, (F32)viewRect.p0.y, (F32)viewRect.p1.x, (F32)viewRect.p1.y);
        }
    }

    //-----------------------------------------------------------------------------

    void ClipRestore()
    {
        if (backend->SetViewport(Vid::viewDesc))
        {
            Vid::clipRect.SetSize((F32)Vid::viewDesc.x, (F32)Vid::viewDesc.y, (F32)Vid::viewDesc.width, (F32)Vid::viewDesc.height);
        }
    }

    //-----------------------------------------------------------------------------

    void RenderState::ClearData()
    {
        status.filter = filterFILTER | filterMIPMAP | filterMIPFILTER;
        status.shade = shadeGOURAUD;
        status.texture = TRUE;
        status.zbuffer = TRUE;
        status.stencil = FALSE;
        //	  status.specular       = FALSE;
        status.specular = TRUE;

        status.fog = TRUE;
        status.alpha = TRUE;
        status.texWrap = TRUE;
        status.antiAlias = FALSE;
        status.texture = TRUE;
        status.dither = TRUE;
        status.texMovie2D = TRUE;
        status.texMovie3D = TRUE;
        status.clipGuard = FALSE;
        status.alphaFar = FALSE;
        status.alphaNear = FALSE;
        status.hardTL = FALSE;
        status.dxTL = FALSE;
        status.antiAlias = FALSE;
        status.antiAliasEdge = FALSE;
        status.texColorKey = FALSE;
        status.tex32 = FALSE;
        status.waitRetrace = FALSE;
        status.zbias = FALSE;
        status.xmm = FALSE;

        status.weather = TRUE;

        fogColorF32.Set(0, 0, 0, 1);
        fogColor.Set(fogColorF32.r, fogColorF32.g, fogColorF32.b, 1.0f);
        fogMin = STARTNEARPLANE;
        fogMax = STARTFARPLANE;
        fogDepth = 0.8f;
        fogDensity = 1.0;

        ambientColorF32.Set(0, 0, 0, 1);

        clearFlags = 0;

        renderFlags = RS_BLEND_DEF;

        zBias = ZBIAS;

        textureReduction = 0;

        cullSign = 0;

        alphaNear = alphaFar = 0;

        texMinimapSize = 64;
    }

    //----------------------------------------------------------------------------

    void SetFogColor(F32 r, F32 g, F32 b)
    {
        renderState.fogColorF32.Set(r, g, b, 1);
        renderState.fogColor.Set
        (
            renderState.fogColorF32.r,
            renderState.fogColorF32.g,
            renderState.fogColorF32.b,
            (U32)255
        );
        SetFogColorI(renderState.fogColor);
    }

    //----------------------------------------------------------------------------

    void SetFogColorI(U32 fogColor)
    {
        if (Vid::isStatus.initialized)
        {
            backend->SetFogColor(fogColor);
        }
    }

    //----------------------------------------------------------------------------

    void SetFogRange(F32 min, F32 max, F32 density)
    {
        renderState.fogMin = min;
        renderState.fogMax = max;
        renderState.fogDensity = density;

        renderState.fogMinZ = Vid::ProjectZ(min);
        renderState.fogMaxZ = 1.0f;

        //  ASSERT( renderState.fogMaxZ > renderState.fogMinZ);
        //  renderState.fogFactor = 255.0f / (renderState.fogMaxZ - renderState.fogMinZ); 
        renderState.fogFactor = 1.0f / (renderState.fogMaxZ - renderState.fogMinZ);

        renderState.fogMinZH = Vid::SetHomogeneousZ(min);
        renderState.fogMaxZH = Vid::SetHomogeneousZ(Vid::Math::farPlane);
        renderState.fogFactorH = 1.0f / (renderState.fogMaxZ - renderState.fogMinZ);

        renderState.fogMinZZ = min;
        renderState.fogMaxZZ = max;
        renderState.fogFactorZ = 1.0f / (renderState.fogMaxZZ - renderState.fogMinZZ);

        if (Vid::isStatus.initialized)
        {
            backend->SetFogRange(renderState.fogMin, renderState.fogMax);
        }
    }

    //----------------------------------------------------------------------------

    void SetAmbientColor(F32 r, F32 g, F32 b)
    {
        renderState.ambientColorF32.Set(r, g, b, 1);
        renderState.ambientColor.Set(r, g, b, (U32)255);

        if (Vid::isStatus.initialized)
        {
            backend->SetAmbientColor(renderState.ambientColor);
        }
    }

    //----------------------------------------------------------------------------

    void GetAmbientColor(F32& r, F32& g, F32& b)
    {
        r = renderState.ambientColorF32.r;
        g = renderState.ambientColorF32.g;
        b = renderState.ambientColorF32.b;
    }

    //----------------------------------------------------------------------------

    Bool SetRenderState(Bool checkInit) // TRUE
    {
        if (checkInit && !isStatus.initialized)
        {
            return FALSE;
        }

        // basic features
        //
        backend->ResetState(renderState.status.wbuffer);

        // vid system features
        //
        SetAmbientColor(renderState.ambientColorF32.r, renderState.ambientColorF32.g, renderState.ambientColorF32.b);

        SetShadeState(renderState.status.shade);
        SetFogState(renderState.status.fog);
        SetFogColor(renderState.fogColorF32.r, renderState.fogColorF32.g, renderState.fogColorF32.b);
        SetFogRange(renderState.fogMin, renderState.fogMax, renderState.fogDensity);

        SetZBufferState(renderState.status.zbuffer);
        SetSpecularState(renderState.status.specular);
        SetDitherState(renderState.status.dither);
        SetAlphaState(renderState.status.alpha);

        // Must stay on: with pre-transformed vertices D3D reads the fog factor
        // from the specular alpha channel, so this is what makes fog work at
        // all. SetSpecularStateI deliberately never turns it back off.
        backend->SetSpecular(TRUE);

        SetAntiAliasStateI(renderState.status.antiAlias);
        SetEdgeAntiAliasStateI(renderState.status.antiAliasEdge);
        SetColorKeyStateI(caps.noAlphaMod);
        SetPerspectiveStateI(TRUE);

        SetTextureFactor(Color(U32(255), U32(255), U32(255), U32(122)));

        SetSrcBlendState(renderState.renderFlags);
        SetDstBlendState(renderState.renderFlags);

        // texture stage related
        //
        SetTexBlendState(renderState.renderFlags);
        SetTexWrapState(renderState.renderFlags);
        SetFilterState(renderState.status.filter);

        //    SetBorderColor( 0);

        renderState.clearFlags = clearBACK;
        if (renderState.status.zbuffer)
        {
            renderState.clearFlags |= clearZBUFFER;
        }
        if (renderState.status.stencil)
        {
            renderState.clearFlags |= clearSTENCIL;
        }

        // reset view parameters
        // SetCamera checks current; trick it by setting NULL
        Camera* cam = curCamera;
        curCamera = NULL;
        SetCamera(*cam);

        return TRUE;
    }

    //----------------------------------------------------------------------------

    Bool RenderBegin()
    {
        bucket.Flush(FALSE);
        tranbucket.Flush(FALSE);

        Bool ok = backend->BeginScene();

        texMemPerFrame = 0;

        return ok;
    }

    //----------------------------------------------------------------------------

    Bool RenderEnd()
    {
        bucket.Flush(TRUE);
        tranbucket.Flush(TRUE);

        // Not on the OpenGL path: FreeVidMem has no DirectDraw to ask, so
        // totalTexMemory is 0 and this would fire every frame. GL drivers
        // manage texture residency themselves, so there is no swap to warn
        // about.
        if (!isStatus.ogl && showTexSwap && texMemPerFrame > totalTexMemory && turtleTex)
        {
            // texture swapping, render turtle
            //
            const Area<S32> rect
            (
                viewRect.p1.x - turtleTex->Width() - 4, viewRect.p0.y + 4,
                viewRect.p1.x, viewRect.p0.y + turtleTex->Height()
            );

            Vid::SetTexture(turtleTex, 0, RS_BLEND_ADD);

            // lock vertex memory
            VertexTL vertmem[4];

            vertmem[0].vv.x = (F32)rect.p0.x;
            vertmem[0].vv.y = (F32)rect.p0.y;
            vertmem[0].vv.z = 0;
            vertmem[0].rhw = 1;
            vertmem[0].diffuse = 0xffff0000;
            vertmem[0].specular = 0xff000000;
            vertmem[0].u = 0.0f;
            vertmem[0].v = 0.0f;

            vertmem[1].vv.x = (F32)rect.p1.x;
            vertmem[1].vv.y = (F32)rect.p0.y;
            vertmem[1].vv.z = vertmem[0].vv.z;
            vertmem[1].rhw = vertmem[0].rhw;
            vertmem[1].diffuse = 0xffff0000;
            vertmem[1].specular = vertmem[0].specular;
            vertmem[1].u = 1.0f;
            vertmem[1].v = 0.0f;

            vertmem[2].vv.x = (F32)rect.p1.x;
            vertmem[2].vv.y = (F32)rect.p1.y;
            vertmem[2].vv.z = vertmem[0].vv.z;
            vertmem[2].rhw = vertmem[0].rhw;
            vertmem[2].diffuse = 0xffff0000;
            vertmem[2].specular = vertmem[0].specular;
            vertmem[2].u = 1.0f;
            vertmem[2].v = 1.0f;

            vertmem[3].vv.x = (F32)rect.p0.x;
            vertmem[3].vv.y = (F32)rect.p1.y;
            vertmem[3].vv.z = vertmem[0].vv.z;
            vertmem[3].rhw = vertmem[0].rhw;
            vertmem[3].diffuse = 0xffff0000;
            vertmem[3].specular = vertmem[0].specular;
            vertmem[3].u = 0.0f;
            vertmem[3].v = 1.0f;

            Vid::DrawIndexedPrimitive
            (
                PT_TRIANGLELIST,
                FVF_TLVERTEX,
                vertmem, 4, Vid::rectIndices, 6,
                DP_DONOTUPDATEEXTENTS | DP_DONOTLIGHT | DP_DONOTCLIP | RS_BLEND_ADD
            );
        }


        return backend->EndScene();
    }

    //----------------------------------------------------------------------------

    void SetMaterialI(const Material* mat)
    {
        if (Material::Manager::GetMaterial() != mat)
        {
            if (!mat)
            {
                mat = defMaterial;
            }
            Material::Manager::SetMaterial(mat);

            backend->SetMaterial(mat);
        }
    }

    //----------------------------------------------------------------------------

    Bool SetTexture(Bitmap* tex, U32 stage, U32 blend) // = 0, = RS_BLEND_DEF
    {
        Bool ok = TRUE;

        if (Vid::renderState.status.texture)
        {
            if (Bitmap::Manager::GetTexture(stage) != tex)
            {
                Bitmap::Manager::SetTexture(tex, stage);

                ok = backend->BindTexture(tex, stage);
            }

            SetTexBlendState(blend, stage);

            if (tex)
            {
                SetTexWrapState(blend, stage);
            }
        }

        if (stage > 0)
        {
            // turn off texturing above this stage
            ok = backend->DisableTexStage(stage + 1);

            renderState.status.texStaged = TRUE;
        }
        else if (renderState.status.texStaged)
        {
            // turn off texturing above this stage
            ok = backend->DisableTexStage(stage + 1);

            renderState.status.texStaged = FALSE;
        }

        return ok;
    }

    //----------------------------------------------------------------------------

    Bool SetTextureI(const Bitmap* tex, U32 stage, U32 blend) // = 0, = RS_BLEND_DEF
    {
        backend->BindTexture(tex, stage);

        if (tex)
        {
            SetTexBlendState(blend, stage);
            SetTexWrapState(blend, stage);
        }

        // turn off texturing above this stage
        return backend->DisableTexStage(stage + 1);
    }

    //----------------------------------------------------------------------------

    void SetBucketMaterialProc(const Material* material)
    {
        BucketMan::SetMaterial(material);

        if (!renderState.status.dxTL)
        {
            return;
        }

        if (BucketMan::forceTranslucent || (material && material->GetStatus().translucent))
        {
            LOG_DIAG(("%s: force %d; mat %d", material->GetName(), BucketMan::forceTranslucent, material->GetStatus().translucent));

            currentBucketMan = &tranbucket;
        }
        else
        {
            currentBucketMan = &bucket;
        }
    }

    //----------------------------------------------------------------------------

    // stage 0 MUST be set first!!!
    //
    void SetBucketTexture(const Bitmap* texture, Bool translucent, U32 stage, U32 blend) // = FALSE, = 0, = RS_BLEND_DEF
    {
        if (stage == 0)
        {
            if (BucketMan::forceTranslucent)
            {
                translucent = TRUE;
            }
            if (translucent || (texture && texture->GetStatus().translucent))
            {
                currentBucketMan = &tranbucket;
            }
            else
            {
                currentBucketMan = &bucket;
            }
        }
        BucketMan::SetTexture(texture, stage, blend);
    }

    //----------------------------------------------------------------------------

    Bool DrawPrimitive
    (
        PRIMITIVE_TYPE prim_type,
        VERTEX_TYPE vert_type,
        LPVOID verts,
        DWORD vert_count,
        DWORD flags
    )
    {
        // sanity check vertex count
        ASSERT(vert_count >= 1);
        ASSERT
        (
            ((prim_type == PT_POINTLIST)) ||
            ((prim_type == PT_LINELIST) && (vert_count % 2 == 0)) ||
            ((prim_type == PT_LINESTRIP) && (vert_count >= 2)) ||
            ((prim_type == PT_TRIANGLELIST) && (vert_count % 3 == 0)) ||
            ((prim_type == PT_TRIANGLESTRIP) && (vert_count >= 3)) ||
            ((prim_type == PT_TRIANGLEFAN) && (vert_count >= 3))
        );

        // Check for errors
        if (renderState.status.checkVerts && !AreVerticesInRange((VertexTL*)verts, vert_count, NULL, 0, flags))
        {
            flags &= ~DP_DONOTCLIP;

            LOG_DIAG(("texture = %s", Bitmap::Manager::GetTexture(0) ? Bitmap::Manager::GetTexture(0)->GetName() : "null"));
        }

#ifndef DODXLEANANDGRUMPY
        if (renderState.status.dxTL)
        {
            SetCullState((flags & RS_2SIDED) ? FALSE : TRUE);

            backend->SetLighting(vert_type == FVF_VERTEX ? TRUE : FALSE);
        }
#endif

        SetSrcBlendState(flags);
        SetDstBlendState(flags);

        backend->SetClipping((flags & DP_DONOTCLIP) ? FALSE : TRUE);

        flags &= DP_MASK;
        //    flags &= ~(DP_DONOTUPDATEEXTENTS | DP_DONOTCLIP | DP_DONOTLIGHT);

        Bool ok = backend->DrawPrimitive(prim_type, vert_type, verts, vert_count, flags);

        indexCount += vert_count;

        return ok;
    }

    //----------------------------------------------------------------------------

    Bool DrawFanStripPrimitive
    (
        PRIMITIVE_TYPE prim_type,
        VERTEX_TYPE vert_type,
        LPVOID verts,
        DWORD vert_count,
        DWORD flags
    )
    {
        // sanity check vertex count
        ASSERT(vert_count >= 1);
        ASSERT
        (
            ((prim_type == PT_POINTLIST)) ||
            ((prim_type == PT_LINELIST) && (vert_count % 2 == 0)) ||
            ((prim_type == PT_LINESTRIP) && (vert_count >= 2)) ||
            ((prim_type == PT_TRIANGLELIST) && (vert_count % 3 == 0)) ||
            ((prim_type == PT_TRIANGLESTRIP) && (vert_count >= 3)) ||
            ((prim_type == PT_TRIANGLEFAN) && (vert_count >= 3))
        );

        // Check for errors
        if (renderState.status.checkVerts && !AreVerticesInRange((VertexTL*)verts, vert_count, NULL, 0, flags))
        {
            flags &= ~DP_DONOTCLIP;

            LOG_DIAG(("texture = %s", Bitmap::Manager::GetTexture(0) ? Bitmap::Manager::GetTexture(0)->GetName() : "null"));
        }

#ifndef DODXLEANANDGRUMPY
        if (renderState.status.dxTL)
        {
            SetCullState((flags & RS_2SIDED) ? FALSE : TRUE);

            backend->SetLighting(vert_type == FVF_VERTEX ? TRUE : FALSE);
        }
#endif

        SetSrcBlendState(flags);
        SetDstBlendState(flags);

        backend->SetClipping((flags & DP_DONOTCLIP) ? FALSE : TRUE);

        flags &= DP_MASK;
        //    flags &= ~(DP_DONOTUPDATEEXTENTS | DP_DONOTCLIP | DP_DONOTLIGHT);

        Bool ok = backend->DrawPrimitive(prim_type, vert_type, verts, vert_count, flags);

        indexCount += vert_count - 2;

        return ok;
    }

    //----------------------------------------------------------------------------

    Bool DrawIndexedPrimitive
    (
        PRIMITIVE_TYPE prim_type,
        VERTEX_TYPE vert_type,
        LPVOID verts,
        DWORD vert_count,
        LPWORD indices,
        DWORD index_count,
        DWORD flags
    )
    {
        // sanity check vertex count
        ASSERT(index_count >= 1);
        ASSERT
        (
            ((prim_type == PT_POINTLIST)) ||
            ((prim_type == PT_LINELIST) && (index_count % 2 == 0)) ||
            ((prim_type == PT_LINESTRIP) && (index_count >= 2)) ||
            ((prim_type == PT_TRIANGLELIST) && (index_count % 3 == 0)) ||
            ((prim_type == PT_TRIANGLESTRIP) && (index_count >= 3)) ||
            ((prim_type == PT_TRIANGLEFAN) && (index_count >= 3))
        );

        // Check for errors
        if (renderState.status.checkVerts && !AreVerticesInRange((VertexTL*)verts, vert_count, indices, index_count, flags))
        {
            flags &= ~DP_DONOTCLIP;

            LOG_DIAG(("texture = %s", Bitmap::Manager::GetTexture(0) ? Bitmap::Manager::GetTexture(0)->GetName() : "null"));
        }

#ifndef DODXLEANANDGRUMPY
        if (renderState.status.dxTL && vert_type != FVF_TLVERTEX)
        {
            SetCullState((flags & RS_2SIDED) ? FALSE : TRUE);

            backend->SetLighting(vert_type == FVF_VERTEX ? TRUE : FALSE);
        }
        else
        {
            SetCullState(FALSE);

            backend->SetLighting(TRUE);
        }
#endif

        SetSrcBlendState(flags);
        SetDstBlendState(flags);

        backend->SetClipping((flags & DP_DONOTCLIP) ? FALSE : TRUE);

        flags &= DP_MASK;
        //    flags &= ~(DP_DONOTUPDATEEXTENTS | DP_DONOTCLIP | DP_DONOTLIGHT);

        /*
            if (bucketdump)
            {
        //      LOG_DIAG( ("pt = %d, vt = %d, f = %d",
        //        prim_type,`
        //        /vert_type,
        //        flags) );

              U32 i;
              VertexTL *v = (VertexTL *) verts;
              for (i = 0; i < vert_count; i++, v++)
              {
        //        LOG_DIAG( ("vv = %f, %f, %f rhw = %f, d = %u s = %u, uv %f, %f",
        //          v->vv.x, v->vv.y, v->vv.z, v->rhw, v->diffuse, v->specular, v->u, v->v) );
                v->vv.x = 0.0f;
                v->vv.y = 0.0f;
              }
        //      for (i = 0; i < index_count; i++)
        //      {
        //        U32 in = indices[i];
        //        LOG_DIAG( ("i = %d", in) );
        //      }
            }
        */

        if (renderState.status.texStaged)
        {
        }

        Bool ok = backend->DrawIndexedPrimitive(prim_type, vert_type, verts, vert_count, indices, index_count, flags);

        indexCount += index_count;

        return ok;
    }

    //----------------------------------------------------------------------------
}

//----------------------------------------------------------------------------

// stage 0 MUST be set first!!!
//
void Bitmap::Manager::SetTexture(Bitmap* texture, U32 stage) // = 0)
{
    ASSERT(stage < MAX_TEXTURE_STAGES);

    if (stage == 0)
    {
        ClearTextures();
    }
    curTextureList[stage] = texture;

    stage += 1;
    if (stage > textureCount)
    {
        textureCount = stage;
    }

    // textures per frame usage monitoring
    //
    if (Vid::showTexSwap && texture && texture->frameNumber != Main::frameNumber)
    {
        Vid::texMemPerFrame += texture->MemSize();

        texture->frameNumber = Main::frameNumber;
    }
}

//----------------------------------------------------------------------------

void VertexTL::SetFog()
{
    // set vertex fog
    //
    S32 fogval = 255;

    if (vv.z > Vid::renderState.fogMinZ)
    {
        //    specular.a = Utils::FtoL( 255.f - ((vv.z - renderState.fogMinZ) * renderState.fogFactor));
        F32 fog = (vv.z - Vid::renderState.fogMinZ) * Vid::renderState.fogFactor;
        fog *= fog;   // non-linear
        fogval = Utils::FtoL(255.f - (fog * 255.0f));
    }
    fogval = Max<S32>(0, fogval - Vid::extraFog);
    specular.a = (U8)fogval;

    // set vertex transluceny
    //
    if (Vid::renderState.status.alphaFar && vv.z > Vid::renderState.alphaFar)
    {
        Float2Int fa((vv.z - Vid::renderState.alphaFar) / (1 - Vid::renderState.alphaFar) * diffuse.a + Float2Int::magic);
        diffuse.a = U8(diffuse.a - fa.i);
    }
    /*
      if (Vid::renderState.status.alphaNear && vv.z < Vid::renderState.alphaNear)
      {
        Float2Int fa( (1 - ((Vid::renderState.alphaNear - vv.z) / Vid::renderState.alphaNear)) * diffuse.a + Float2Int::magic);
        diffuse.a = U8(diffuse.a + fa.i);
      }
    */
}

//----------------------------------------------------------------------------

void VertexTL::SetFogX()
{
    // set vertex fog
    //
    S32 fogval = 255;

    if (vv.z > Vid::renderState.fogMinZ)
    {
        F32 fog = (vv.z - Vid::renderState.fogMinZ) * Vid::renderState.fogFactor;
        fog *= fog;   // non-linear
        fogval = Utils::FtoL(255.f - (fog * 255.0f));
    }
    fogval = Max<S32>(0, fogval - specular.a);
    specular.a = (U8)fogval;

    // set vertex transluceny
    //
    if (Vid::renderState.status.alphaFar && vv.z > Vid::renderState.alphaFar)
    {
        Float2Int fa((vv.z - Vid::renderState.alphaFar) / (1 - Vid::renderState.alphaFar) * diffuse.a + Float2Int::magic);
        diffuse.a = U8(diffuse.a - fa.i);
    }
    /*
      if (Vid::renderState.status.alphaNear && vv.z < Vid::renderState.alphaNear)
      {
        Float2Int fa( (vv.z - Vid::renderState.alphaFar) / Vid::renderState.alphaNear * diffuse.a + Float2Int::magic);
        diffuse.a = U8(diffuse.a + fa.i);
      }
    */
}

//----------------------------------------------------------------------------

void VertexTL::SetFogH()
{
    // set vertex fog
    if (vv.z > Vid::renderState.fogMinZH)
    {
        F32 fog = (vv.z - Vid::renderState.fogMinZH) * Vid::renderState.fogFactorH;
        fog *= fog;   // non-linear
        specular.a = (U8)Utils::FtoL(255.f - (fog * 255.0f));
    }
}

//----------------------------------------------------------------------------
