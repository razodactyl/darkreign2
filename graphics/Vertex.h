///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// vertex.h
//
// 18-JUN-1998
//

#ifndef _VERTEX_H
#define _VERTEX_H

#include "mathtypes.h"

typedef DWORD COLOR;

// Rendering vocabulary
//
// Nothing below refers to Direct3D. These are the game's own constants, and a
// backend translates them at its own boundary.
//
// The numbers happen to match the DirectX 7 values they were originally copied
// from, which lets the D3D backend pass them straight through. That is an
// optimisation, not an assumption: vid_backend_d3d.cpp static_asserts every one
// of them against the real D3D constant, so if the two ever diverge the build
// breaks instead of the renderer.

enum PRIMITIVE_TYPE
{
    PT_POINTLIST = 1,
    PT_LINELIST = 2,
    PT_LINESTRIP = 3,
    PT_TRIANGLELIST = 4,
    PT_TRIANGLESTRIP = 5,
    PT_TRIANGLEFAN = 6,
    PT_FORCE_DWORD = 0x7fffffff,
};

// vertex format component bits
enum VERTEX_FORMAT
{
    VF_XYZ = 0x002,
    VF_XYZRHW = 0x004,
    VF_NORMAL = 0x010,
    VF_RESERVED1 = 0x020,
    VF_DIFFUSE = 0x040,
    VF_SPECULAR = 0x080,
    VF_TEX1 = 0x100,
    VF_TEX2 = 0x200,
};

// vertex type flags
// what's with the TEX1 instead of TEX0 in LVERTEX and TLVERTEX?? HST 7/15/98
enum VERTEX_TYPE
{
    FVF_VERTEX = VF_XYZ | VF_NORMAL | VF_TEX1,
    FVF_T2VERTEX = VF_XYZ | VF_NORMAL | VF_TEX1 | VF_TEX2,
    FVF_CVERTEX = VF_XYZ | VF_NORMAL | VF_DIFFUSE | VF_TEX1,
    FVF_T2CVERTEX = VF_XYZ | VF_NORMAL | VF_DIFFUSE | VF_TEX1 | VF_TEX2,
    FVF_LVERTEX = VF_XYZ | VF_RESERVED1 | VF_DIFFUSE | VF_TEX1,
    FVF_TLVERTEX = VF_XYZRHW | VF_DIFFUSE | VF_SPECULAR | VF_TEX1,
    FVF_T2LVERTEX = VF_XYZRHW | VF_DIFFUSE | VF_SPECULAR | VF_TEX1 | VF_TEX2,
    FVF_DWORD = 0x7fffffff,
};

// texture address mode, stored in RS_ADD_MASK as (mode - 1)
enum TEXTURE_ADDRESS
{
    TA_WRAP = 1,
    TA_MIRROR = 2,
    TA_CLAMP = 3,
};

// blend factors, stored in RS_SRC_MASK / RS_DST_MASK
enum BLEND_FACTOR
{
    BLEND_ZERO = 1,
    BLEND_ONE = 2,
    BLEND_SRCCOLOR = 3,
    BLEND_INVSRCCOLOR = 4,
    BLEND_SRCALPHA = 5,
    BLEND_INVSRCALPHA = 6,
    BLEND_DSTALPHA = 7,
    BLEND_INVDSTALPHA = 8,
    BLEND_DSTCOLOR = 9,
    BLEND_INVDSTCOLOR = 10,
    BLEND_SRCALPHASAT = 11,
};

// DrawPrimitive flags

#define RS_ADD_SHIFT      14                        // texture addressing
#define RS_ADD_MASK       (0x3 << RS_ADD_SHIFT)

#define RS_SRC_SHIFT      16                        // source blending mode
#define RS_SRC_MASK       (0xf << RS_SRC_SHIFT)

#define RS_DST_SHIFT      20                        // destination blending mode
#define RS_DST_MASK       (0xf << RS_DST_SHIFT)

#define RS_TEX_SHIFT      28                        // texture blending mode
#define RS_TEX_MASK       (0x7 << RS_TEX_SHIFT)

enum DRAWPRIMITIVE_FLAGS
{
    DP_WAIT = 0x00000001,
    DP_DONOTCLIP = 0x00000004,
    DP_DONOTUPDATEEXTENTS = 0x00000008,
    DP_DONOTLIGHT = 0x00000010,
    DP_MASK = DP_WAIT,

    RS_NOINDEXED = 0x00000080,
    //  RS_NOLIGHTING         = 0x00000100,
    RS_2SIDED = 0x00000200,
    RS_HIDDEN = 0x00000400,
    RS_NOZBUFFER = 0x00000800,
    RS_WIREFRAME = 0x00001000,
    RS_NOSORT = 0x00002000,
    RS_FLAG_MASK = RS_WIREFRAME | RS_NOSORT,
    RS_MESH_MASK = RS_2SIDED,

    //  RS_TEXWRAP            = ((TA_WRAP - 1) << RS_ADD_SHIFT), // = 0,
    RS_TEXMIRROR = ((TA_MIRROR - 1) << RS_ADD_SHIFT), // 0x00004000,
    RS_TEXCLAMP = ((TA_CLAMP - 1) << RS_ADD_SHIFT), // 0x00008000,

    RS_SRC_ZERO = (BLEND_ZERO << RS_SRC_SHIFT),
    RS_SRC_ONE = (BLEND_ONE << RS_SRC_SHIFT),
    RS_SRC_SRCCOLOR = (BLEND_SRCCOLOR << RS_SRC_SHIFT),
    RS_SRC_INVSRCCOLOR = (BLEND_INVSRCCOLOR << RS_SRC_SHIFT),
    RS_SRC_SRCALPHA = (BLEND_SRCALPHA << RS_SRC_SHIFT),
    RS_SRC_INVSRCALPHA = (BLEND_INVSRCALPHA << RS_SRC_SHIFT),
    RS_SRC_DSTCOLOR = (BLEND_DSTCOLOR << RS_SRC_SHIFT),
    RS_SRC_INVDSTCOLOR = (BLEND_INVDSTCOLOR << RS_SRC_SHIFT),
    RS_SRC_DSTALPHA = (BLEND_DSTALPHA << RS_SRC_SHIFT),
    RS_SRC_INVDSTALPHA = (BLEND_INVDSTALPHA << RS_SRC_SHIFT),
    RS_SRC_SRCALPHASAT = (BLEND_SRCALPHASAT << RS_SRC_SHIFT),

    RS_DST_ZERO = (BLEND_ZERO << RS_DST_SHIFT),
    RS_DST_ONE = (BLEND_ONE << RS_DST_SHIFT),
    RS_DST_SRCCOLOR = (BLEND_SRCCOLOR << RS_DST_SHIFT),
    RS_DST_INVSRCCOLOR = (BLEND_INVSRCCOLOR << RS_DST_SHIFT),
    RS_DST_SRCALPHA = (BLEND_SRCALPHA << RS_DST_SHIFT),
    RS_DST_INVSRCALPHA = (BLEND_INVSRCALPHA << RS_DST_SHIFT),
    RS_DST_DSTCOLOR = (BLEND_DSTCOLOR << RS_DST_SHIFT),
    RS_DST_INVDSTCOLOR = (BLEND_INVDSTCOLOR << RS_DST_SHIFT),
    RS_DST_DSTALPHA = (BLEND_DSTALPHA << RS_DST_SHIFT),
    RS_DST_INVDSTALPHA = (BLEND_INVDSTALPHA << RS_DST_SHIFT),
    RS_DST_SRCALPHASAT = (BLEND_SRCALPHASAT << RS_DST_SHIFT),

    RS_TEX_DECAL = (1 << RS_TEX_SHIFT),
    RS_TEX_DECALALPHA = (2 << RS_TEX_SHIFT),
    RS_TEX_MODULATE = (3 << RS_TEX_SHIFT),
    RS_TEX_MODULATE2X = (4 << RS_TEX_SHIFT),
    RS_TEX_MODULATE4X = (5 << RS_TEX_SHIFT),
    RS_TEX_MODULATEALPHA = (6 << RS_TEX_SHIFT),
    RS_TEX_ADD = (7 << RS_TEX_SHIFT),

    RS_BLEND_MASK = RS_SRC_MASK | RS_DST_MASK | RS_TEX_MASK,
    RS_BUCKY_MASK = RS_SRC_MASK | RS_DST_MASK | RS_TEX_MASK | RS_ADD_MASK | RS_FLAG_MASK,
    RS_BLEND_GLOW = RS_SRC_SRCALPHA | RS_DST_ONE | RS_TEX_MODULATE2X,
    RS_BLEND_ADD = RS_SRC_SRCALPHA | RS_DST_ONE | RS_TEX_ADD,
    RS_BLEND_MODULATE = RS_SRC_SRCALPHA | RS_DST_INVSRCALPHA | RS_TEX_MODULATE,
    RS_BLEND_MODULATE2X = RS_SRC_SRCALPHA | RS_DST_INVSRCALPHA | RS_TEX_MODULATE2X,
    RS_BLEND_MODULATE4X = RS_SRC_SRCALPHA | RS_DST_INVSRCALPHA | RS_TEX_MODULATE4X,
    RS_BLEND_DECAL = RS_SRC_SRCALPHA | RS_DST_INVSRCALPHA | RS_TEX_DECAL,
    RS_BLEND_DECALALPHA = RS_SRC_SRCALPHA | RS_DST_INVSRCALPHA | RS_TEX_DECALALPHA,
    RS_BLEND_DEF = RS_BLEND_MODULATE,
};

//----------------------------------------------------------------------------

// all of the supported vertex data formats

// based on D3DVERTEX
struct Vertex
{
    Vector vv, nv;

    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    void ClearData()
    {
        vv.ClearData();
        nv.ClearData();
    }
};

//----------------------------------------------------------------------------

// vertex to state matrix indexing with multiweighting
//
#define MAXWEIGHTS  2

struct VertIndex
{
    U32 count;
    U16 index[MAXWEIGHTS];
    F32 weight[MAXWEIGHTS];

    Bool operator==(VertIndex& v1)
    {
        return (count == v1.count
                   && index[0] == v1.index[0]
                   && index[1] == v1.index[1]
                   && weight[0] == v1.weight[0]
                   && weight[1] == v1.weight[1])
                   ? TRUE
                   : FALSE;
    }
};

//----------------------------------------------------------------------------

struct VertexI : public Vertex
{
    VertIndex vi;
};

//----------------------------------------------------------------------------

struct VertexI2 : public Vertex
{
    U32 i0, i1;
    VertIndex vi;
};

//----------------------------------------------------------------------------

struct VertexT2
{
    Vector vv, nv;

    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    union
    {
        struct
        {
            F32 u1, v1;
        };

        UVPairS uv1;
    };


    void ClearData()
    {
        vv.ClearData();
        nv.ClearData();
    }
};

struct VertexC
{
    Vector vv, nv;
    Color diffuse;

    //  Color     specular;
    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    void ClearData()
    {
        vv.ClearData();
        nv.ClearData();
    }
};

struct VertexT2C
{
    Vector vv, nv;
    Color diffuse;

    //  Color     specular;
    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    union
    {
        struct
        {
            F32 u1, v1;
        };

        UVPairS uv1;
    };


    void ClearData()
    {
        vv.ClearData();
        nv.ClearData();
    }
};

/*
inline void Matrix::Transform( Vertex & dst, const Vertex & src) const
{
//  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z + posit.x;
//	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z + posit.y;
//	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z + posit.z;
  Transform_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//-----------------------------------------------------------------------------

inline void Matrix::Rotate( Vertex & dst, const Vertex & src) const
{
//  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z;
//	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z;
//	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z;
  Rotate_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//-----------------------------------------------------------------------------

inline void Matrix::RotateInv( Vertex & dst, const Vertex & src) const
{
// 	dst.vv.x = right.x * src.vv.x + right.y * src.vv.y + right.z * src.vv.z;
//  dst.vv.y =    up.x * src.vv.x +    up.y * src.vv.y +    up.z * src.vv.z;
//  dst.vv.z = front.x * src.vv.x + front.y * src.vv.y + front.z * src.vv.z;
  Unrotate_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//----------------------------------------------------------------------------

inline void Matrix::Transform( Vertex & dst, Vertex & src) const
{
//  Vertex temp = src;
//  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z + posit.x;
//	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z + posit.y;
//	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z + posit.z;
  Transform_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//-----------------------------------------------------------------------------

inline void Matrix::Rotate( Vertex & dst, Vertex & src) const
{
//  Vertex temp = src;
//  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z;
//	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z;
//	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z;
  Rotate_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//-----------------------------------------------------------------------------

inline void Matrix::RotateInv( Vertex & dst, Vertex &src) const
{
//  Vertex temp = src;
// 	dst.vv.x = right.x * temp.vv.x + right.y * temp.vv.y + right.z * temp.vv.z;
//  dst.vv.y =    up.x * temp.vv.x +    up.y * temp.vv.y +    up.z * temp.vv.z;
//  dst.vv.z = front.x * temp.vv.x + front.y * temp.vv.y + front.z * temp.vv.z;
  Unrotate_Vectors( (F32*)&(dst.vv), (F32*)&(src.vv), sizeof(Vertex), 1);
}
//----------------------------------------------------------------------------

inline void Matrix::Transform( Vertex * dst, const Vertex * src, U32 count) const
{
  Transform_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//-----------------------------------------------------------------------------

inline void Matrix::Rotate( Vertex * dst, const Vertex * src, U32 count) const
{
  Rotate_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//-----------------------------------------------------------------------------

inline void Matrix::RotateInv( Vertex * dst, const Vertex * src, U32 count) const
{
  Unrotate_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//----------------------------------------------------------------------------

inline void Matrix::Transform( Vertex  dst, Vertex * src, U32 count) const
{
  Transform_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//-----------------------------------------------------------------------------

inline void Matrix::Rotate( Vertex * dst, Vertex * src, U32 count) const
{
  Rotate_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//-----------------------------------------------------------------------------

inline void Matrix::RotateInv( Vertex * dst, Vertex * src, U32 count) const
{
  Unrotate_Vectors( (F32*)&(dst->vv), (F32*)&(src->vv), sizeof(Vertex), count);
}
//----------------------------------------------------------------------------
*/

// removing pad made DR2 slightly slower!!!
//
struct VertexL
{
    Vector vv;
    U32 pad;
    Color diffuse;
    Color specular;

    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    void ClearData()
    {
        vv.ClearData();
    }

    void operator=(Vertex& v)
    {
        vv = v.vv;
        uv = v.uv;
    }
};

inline void Matrix::Transform(VertexL& dst, const VertexL& src) const
{
    //  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z + posit.x;
    //	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z + posit.y;
    //	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z + posit.z;
    Transform_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexL& dst, const VertexL& src) const
{
    //  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z;
    //	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z;
    //	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z;
    Rotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexL& dst, const VertexL& src) const
{
    // 	dst.vv.x = right.x * src.vv.x + right.y * src.vv.y + right.z * src.vv.z;
    //  dst.vv.y =    up.x * src.vv.x +    up.y * src.vv.y +    up.z * src.vv.z;
    //  dst.vv.z = front.x * src.vv.x + front.y * src.vv.y + front.z * src.vv.z;
    Unrotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexL& dst, VertexL& src) const
{
    //  VertexL temp = src;
    //  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z + posit.x;
    //	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z + posit.y;
    //	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z + posit.z;
    Transform_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexL& dst, VertexL& src) const
{
    //  VertexL temp = src;
    //  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z;
    //	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z;
    //	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z;
    Rotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexL& dst, VertexL& src) const
{
    //  VertexL temp = src;
    // 	dst.vv.x = right.x * temp.vv.x + right.y * temp.vv.y + right.z * temp.vv.z;
    //  dst.vv.y =    up.x * temp.vv.x +    up.y * temp.vv.y +    up.z * temp.vv.z;
    //  dst.vv.z = front.x * temp.vv.x + front.y * temp.vv.y + front.z * temp.vv.z;
    Unrotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexL* dst, const VertexL* src, U32 count) const
{
    Transform_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexL* dst, const VertexL* src, U32 count) const
{
    Rotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexL* dst, const VertexL* src, U32 count) const
{
    Unrotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexL* dst, VertexL* src, U32 count) const
{
    Transform_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexL* dst, VertexL* src, U32 count) const
{
    Rotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexL* dst, VertexL* src, U32 count) const
{
    Unrotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexL), count);
}

//-----------------------------------------------------------------------------

struct VertexTL
{
    Vector vv;
    F32 rhw;
    Color diffuse;
    Color specular;

    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    void ClearData()
    {
        vv.ClearData();
    }

    void operator=(Vertex& v)
    {
        vv = v.vv;
        uv = v.uv;
    }

    void Interpolate(const VertexTL& v1, const VertexTL& v2, F32 t)
    {
        vv.x = v1.vv.x + t * (v2.vv.x - v1.vv.x);
        vv.y = v1.vv.y + t * (v2.vv.y - v1.vv.y);
        vv.z = v1.vv.z + t * (v2.vv.z - v1.vv.z);
        rhw = v1.rhw + t * (v2.rhw - v1.rhw);
        u = v1.u + t * (v2.u - v1.u);
        v = v1.v + t * (v2.v - v1.v);

        diffuse.Interpolate(v1.diffuse, v2.diffuse, t);
        specular.Interpolate(v1.specular, v2.specular, t);
    }

    // from fully projected vert
    void SetFog();      // in vidrend.cpp

    // from homogenous vert
    void SetFogH();      // in vidrend.cpp

    // from camera vert
    void SetFogX();      // in vidrend.cpp
};

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexTL& dst, const VertexTL& src) const
{
    //  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z + posit.x;
    //	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z + posit.y;
    //	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z + posit.z;
    Transform_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexTL& dst, const VertexTL& src) const
{
    //  dst.vv.x = right.x * src.vv.x + up.x * src.vv.y + front.x * src.vv.z;
    //	dst.vv.y = right.y * src.vv.x + up.y * src.vv.y + front.y * src.vv.z;
    //	dst.vv.z = right.z * src.vv.x + up.z * src.vv.y + front.z * src.vv.z;
    Rotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexTL& dst, const VertexTL& src) const
{
    // 	dst.vv.x = right.x * src.vv.x + right.y * src.vv.y + right.z * src.vv.z;
    //  dst.vv.y =    up.x * src.vv.x +    up.y * src.vv.y +    up.z * src.vv.z;
    //  dst.vv.z = front.x * src.vv.x + front.y * src.vv.y + front.z * src.vv.z;
    Unrotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexTL& dst, VertexTL& src) const
{
    //  VertexTL temp = src;
    //  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z + posit.x;
    //	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z + posit.y;
    //	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z + posit.z;
    Transform_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexTL& dst, VertexTL& src) const
{
    //  VertexTL temp = src;
    //  dst.vv.x = right.x * temp.vv.x + up.x * temp.vv.y + front.x * temp.vv.z;
    //	dst.vv.y = right.y * temp.vv.x + up.y * temp.vv.y + front.y * temp.vv.z;
    //	dst.vv.z = right.z * temp.vv.x + up.z * temp.vv.y + front.z * temp.vv.z;
    Rotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexTL& dst, VertexTL& src) const
{
    //  VertexTL temp = src;
    // 	dst.vv.x = right.x * temp.vv.x + right.y * temp.vv.y + right.z * temp.vv.z;
    //  dst.vv.y =    up.x * temp.vv.x +    up.y * temp.vv.y +    up.z * temp.vv.z;
    //  dst.vv.z = front.x * temp.vv.x + front.y * temp.vv.y + front.z * temp.vv.z;
    Unrotate_Vectors((F32*)&(dst.vv), (F32*)&(src.vv), sizeof(VertexTL), 1);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexTL* dst, const VertexTL* src, U32 count) const
{
    Transform_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexTL* dst, const VertexTL* src, U32 count) const
{
    Rotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexTL* dst, const VertexTL* src, U32 count) const
{
    Unrotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Transform(VertexTL* dst, VertexTL* src, U32 count) const
{
    Transform_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::Rotate(VertexTL* dst, VertexTL* src, U32 count) const
{
    Rotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

inline void Matrix::RotateInv(VertexTL* dst, VertexTL* src, U32 count) const
{
    Unrotate_Vectors((F32*)&(dst->vv), (F32*)&(src->vv), sizeof(VertexTL), count);
}

//-----------------------------------------------------------------------------

struct VertexT2L
{
    Vector vv;
    F32 rhw;
    Color diffuse, specular;

    union
    {
        struct
        {
            F32 u, v;
        };

        UVPairS uv;
    };

    union
    {
        struct
        {
            F32 u1, v1;
        };

        UVPairS uv1;
    };

    void ClearData()
    {
        vv.ClearData();
    }

    void operator=(Vertex& v)
    {
        vv = v.vv;
        uv = v.uv;
    }
};

//-----------------------------------------------------------------------------

// a single tri
//
struct FaceObj
{
    // indices
    //
    U16 vtx[3];
    U16 verts[3];
    U16 norms[3];
    U16 uvs[3];

    U16 index;        // face
    U16 buckyIndex;   // material group

    void ClearData()
    {
        Utils::Memset(this, 0, sizeof(*this));
    }

    FaceObj()
    {
        ClearData();
    }
};

//----------------------------------------------------------------------------

struct GeoCache
{
    //  void * vtx;     // raw vertex data
    //  VertIndex * vmap;

    U16* idx;     // index data

    struct Mrm
    {
        U8 vCount, rCount;
        U16 iCount;

        struct Rec
        {
            U16 index;
            U16 value[2];   // decreasing, increasing
        };

        Rec* rec;

        Mrm()
        {
            rec = NULL;
        }

        ~Mrm()
        {
            if (rec)
            {
                delete rec;
            }
        }
    };

    Mrm* mrm;

    U32 vCount, iCount, memSize;

    GeoCache()
    {
        ClearData();
    }

    ~GeoCache()
    {
        Release();
    }

    void ClearData()
    {
        //    vtx  = NULL;
        //    vmap = NULL;

        idx = NULL;
        mrm = NULL;
        vCount = iCount = memSize = 0;
    }

    void Release()
    {
        /*
            if (vtx)
            {
              delete [] vtx;
            }
            if (vmap)
            {
              delete [] vmap;
            }
        */
        if (idx)
        {
            delete[] idx;
        }
        if (mrm)
        {
            delete[] mrm;
        }
        ClearData();
    }

    void Alloc(U32 _vCount, U32 _iCount, U32 sizeofVertex = sizeof(VertexTL), U32 mrmCount = 0)
    {
        sizeofVertex;

        Release();

        vCount = _vCount;
        iCount = _iCount;
        /*
            memSize = vCount * sizeofVertex;
            vtx  = (void *) new char[memSize];

            memSize += vCount * sizeof(VertIndex);
            vmap = new VertIndex[vCount];
        */
        memSize = iCount << 1;
        idx = new U16[iCount];

        if (mrmCount)
        {
            memSize += mrmCount * sizeof(Mrm);
            mrm = new Mrm[mrmCount];
        }
    }

    inline U32 GetMem()
    {
        return memSize;
    }

    inline operator U16*() { return idx; }
    /*
      inline operator Vertex    *() { return (Vertex    *) vtx; }
      inline operator VertexC   *() { return (VertexC   *) vtx; }
      inline operator VertexT2  *() { return (VertexT2  *) vtx; }
      inline operator VertexT2C *() { return (VertexT2C *) vtx; }
      inline operator VertexL   *() { return (VertexL   *) vtx; }
      inline operator VertexTL  *() { return (VertexTL  *) vtx; }
      inline operator VertexT2L *() { return (VertexT2L *) vtx; }
    */
};

//----------------------------------------------------------------------------

#endif // _VERTEX_H
