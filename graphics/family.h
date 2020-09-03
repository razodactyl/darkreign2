///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-2000 Pandemic Studios, Dark Reign II
//
// family.h
//
// 30-JUL-1998
//
// base classes for building jointed spatial hierarchy trees

#ifndef FAMILY_H
#define FAMILY_H

#include "mathtypes.h"
#include "array.h"
#include "utiltypes.h"
#include "fscope.h"
//----------------------------------------------------------------------------

// forward references
//
class Bitmap;

// clip flags
//
enum ClipType
{
    clipNONE = 0x0000,
    clipNEAR = 0x0001,
    clipFAR = 0x0002,
    clipLEFT = 0x0004,
    clipRIGHT = 0x0008,
    clipTOP = 0x0010,
    clipBOTTOM = 0x0020,
    clipPLANE0 = 0x0040,   // for arbitrary plane clip
    clipPLANE1 = 0x0080,   // for arbitrary plane clip
    clipGUARD = 0x0100,   // for guardband clipping

    clipOUTSIDE = 0xffff,   // not in frustum
    clipNEARFAR = clipNEAR | clipFAR,
    clip2D = clipLEFT | clipRIGHT | clipTOP | clipBOTTOM,
    clipALL = clipNEARFAR | clip2D,
    clipDEFAULT = clipNEARFAR,
};

//-----------------------------------------------------------------------------

//----------------------------------------------------------------------------

struct ShadowInfo
{
    Vector p, p1, p2;
    F32 z, radx, rady, radxRender, radyRender;
    U32 clipFlags;

    Color color;
    Bitmap* texture;
    U32 blend;

    UVPair uv0, uv1, uv2;

    Area<F32> size;
};

//----------------------------------------------------------------------------


class NodeIdent : public GameIdent
{
public:
    U32 index;    // index indentifies a node in MeshRoot's or MeshEnt's state array

public:
    // default constructor
    //
    NodeIdent()
    {
        *str = '\0';
        crc = 0;
        index = 0xffffffff;
    }

    // constructor with string
    NodeIdent(const char* newStr)
    {
        Set(newStr);
        index = 0xffffffff;
    }

    NodeIdent& operator=(const char* newStr)
    {
        Set(newStr);
        index = 0xffffffff;
        return (*this);
    }

    NodeIdent& operator=(GameIdent& ident)
    {
        Set(ident.str);
        index = 0xffffffff;
        return (*this);
    }

    NodeIdent& operator=(NodeIdent& ident)
    {
        Set(ident.str);
        index = ident.index;
        return (*this);
    }

    int operator==(const GameIdent& ident) const
    {
        return (crc == ident.crc);
    }

    int operator!=(const GameIdent& ident) const
    {
        return (crc != ident.crc);
    }

    int operator==(const NodeIdent& ident) const
    {
        return (crc == ident.crc);
    }

    int operator!=(NodeIdent& ident) const
    {
        return (crc != ident.crc);
    }

    Bool Valid() const
    {
        return index != 0xffffffff;
    }
};

//----------------------------------------------------------------------------

class RootObj
{
protected:
    friend class FamilyNode;
    friend class Mesh;
    friend class MeshRoot;
    friend class MeshObj;
    friend class MeshEnt;

    NodeIdent name;

public:
    void ClearData()
    {
        name.Set("");
        name.index = 0;
    }

    RootObj()
    {
        ClearData();
    }

    virtual void SetName(const char* _name)
    {
        name.Set(_name);
    }

    void SetIndex(U32 index)
    {
        name.index = index;
    }

    const char* GetName() const
    {
        return name.str;
    }

    U32 GetNameCrc() const
    {
        return name.crc;
    }

    U32 GetIndex() const
    {
        return name.index;
    }

    const NodeIdent& GetIdent() const
    {
        return name;
    }
};

//----------------------------------------------------------------------------
class Mesh;       // forward references
class MeshObj;
class MeshEnt;
class FamilyNode;
class FamilyState;
class Material;

enum AnimKeyEnum
{
    animNONE = 0x00,
    animPOSITION = 0x01,
    animQUATERNION = 0x02,
    animSCALE = 0x04,
    animMASK = 0x07,
    animNOANIM = 0x10,
    animCONDIRTY = 0x40,   // set via MeshEnt::SetSimTarget( NodeIdent &)
    animDIRTY = 0x80,
    animALLDIRTY = animCONDIRTY | animDIRTY,
};

typedef U32 AnimKeyType;

class AnimKey
{
public:
    F32 frame;
    AnimKeyType type;

    Quaternion quaternion;       // spatial info
    Vector position;
    Vector scale;

    AnimKey()
    {
        ClearData();
    }

    void ClearData();

    U32 GetAnimType() const
    {
        return type;
    }

    F32 GetFrame() const
    {
        return frame;
    }

    void SetAnimType(AnimKeyType _type)
    {
        type = _type;
    }

    F32 SetFrame(F32 _frame)
    {
        frame = _frame;
    }

    const Quaternion& GetRotation() const
    {
        return quaternion;
    }

    const Vector& GetPosition() const
    {
        return position;
    }

    const Vector& GetScale() const
    {
        return scale;
    }

    void Set(const Matrix& matrix)
    {
        quaternion.Set(matrix);
        position = matrix.posit;

        type |= animDIRTY;
    }

    void Set(const Quaternion& _quaternion, const Vector& _position)
    {
        //    ASSERT( _quaternion.Dot( _quaternion);> 0);

        quaternion = _quaternion;
        position = _position;

        type |= animDIRTY;
    }

    void Set(const Quaternion& _quaternion)
    {
        //    ASSERT( _quaternion.Dot( _quaternion);> 0);

        quaternion = _quaternion;

        type |= animDIRTY;
    }

    void Set(const Vector& _position)
    {
        position = _position;

        type |= animDIRTY;
    }

    void SetScale(const Vector& _scale)
    {
        scale = _scale;

        type |= animDIRTY | animSCALE;
    }

    void Set(const AnimKey& src)
    {
        ASSERT(src.quaternion.Dot(src.quaternion) > 0);

        quaternion = src.quaternion;
        position = src.position;
        scale = src.scale;

        type |= animDIRTY;
    }

    inline void operator=(const FamilyState& state);

    Bool operator==(const AnimKey& key)
    {
        // doesn't compare frame
        //
        return type == key.type && quaternion == key.quaternion && position == key.position && scale == key.scale;
    }

    void SaveState(FScope* fScope);
    void LoadState(FScope* fScope);
};

//----------------------------------------------------------------------------

// dynamic render feature control flags
enum ControlRenderFlags
{
    controlNONE = 0x00,
    controlOVERLAY = 0x01,
    controlOVERLAY1PASS = 0x02,
    controlOVERLAY2PASS = 0x04,
    controlMULTIWEIGHT = 0x08,
    controlENVMAP = 0x10,
    controlSHADOW = 0x20,
    controlBUMPMAP = 0x40,
    controlTRANSLUCENT = 0x80,

    controlDEF = 0x1f   // all
};

// node identifier type
enum FamilyNodeType
{
    nodeNone,
    nodeMesh,
    nodeMeshRoot,
    nodeMeshObj,
    nodeMeshEnt,
    nodeLight,
    nodeCamera,
};

// FamilyNode is the basic unit in attachable spatial hierarchies
// Camera, Light, and the Mesh classes are all based on FamilyNode

class FamilyNode : public RootObj
{
public:
    FamilyNodeType nodeType;	                // family node type identifier

    FamilyNode* parent;                      // family tree
    NList<FamilyNode> children;
    NList<FamilyNode>::Node childNode;

    // state data belongs to superclasses; mappable
    FamilyState* statePtr;                   // actual state data; object & world transforms

    void CalcWorldMatrix(const Matrix& world);
    void CalcWorldMatrix(const Matrix& world, FamilyState& state);

    Bool FindOffsetRecurse(const FamilyNode* node, Matrix& matrix, Bool local = FALSE);

    void ClearData();

    FamilyNode()
    {
        ClearData();
    }

    FamilyNode(FamilyState& _state)
    {
        ClearData();
        SetState(_state);
    }

    virtual ~FamilyNode();

    // FamilyNode's FamilyState (state) overloads
    //
    inline void SetState(const FamilyState& _state);
    inline void SetState(const FamilyState& _state, U32 index);
    inline void SetState(const char* _name, const FamilyState& _state, U32 index);
    inline void ClearState();

    inline const Matrix& ObjectMatrix() const;
    inline const Matrix& WorldMatrix() const;

    inline void SetObjectMatrix(const AnimKey& key);
    inline void SetObjectMatrix(const Matrix& matrix);
    inline void SetObjectMatrix(const Quaternion& _quaternion, const Vector& _position);
    inline void SetObjectMatrix(const Quaternion& _quaternion);
    inline void SetObjectMatrix(const Vector& _position);
    inline void SetObjectMatrixScale(const Vector& _scale);

    inline void SetWorldMatrix(const Matrix& matrix);
    inline void SetWorldMatrix(const Quaternion& _quaternion, const Vector& _position);
    inline void SetWorldMatrix(const Quaternion& _quaternion);
    inline void SetWorldMatrix(const Vector& _position);
    inline void SetWorldMatrixScale(const Vector& _scale);

    // node type functions
    //
    U32 GetNodeType() const
    {
        return nodeType;
    }

    Bool IsMesh() const
    {
        return nodeType == nodeMesh;
    }

    Bool IsMeshRoot() const
    {
        return nodeType == nodeMeshRoot;
    }

    Bool IsMeshObj() const
    {
        return nodeType == nodeMeshObj;
    }

    Bool IsMeshEnt() const
    {
        return nodeType == nodeMeshEnt;
    }

    Bool IsLight() const
    {
        return nodeType == nodeLight;
    }

    Bool IsCamera() const
    {
        return nodeType == nodeCamera;
    }

    Bool HasLight() const
    {
        return Child() && Child()->IsLight();
    }

    Bool HasCamera() const
    {
        return Child() && Child()->IsCamera();
    }

    Bool HasMeshEnt() const
    {
        return Child() && Child()->IsMeshEnt();
    }

    Bool HasAttachment() const
    {
        return HasMeshEnt();
    }

    // family heirarchy 
    //
    const FamilyNode* Parent() const
    {
        return parent;
    }

    const NList<FamilyNode>* Children() const
    {
        return &children;
    }

    const FamilyNode* Child() const
    {
        return children.GetHead();
    }

    // inserts 'node' as 'this's' primary/last child depending on 'insert' flag
    virtual void Attach(FamilyNode& node, Bool insert = TRUE);

    // removes 'this' and all its children from parent
    virtual void Detach();

    // remove 'this' from its parent leaving children
    void Extract();

    void Attach(const char* _name, FamilyNode& node, Bool local = FALSE);

    void AttachLocal(const char* _name, FamilyNode& node)
    {
        Attach(_name, node, TRUE);
    }

    virtual void SetWorldRecurse(const Matrix& world);
    virtual void SetWorldRecurseRender(const Matrix& world, FamilyState* stateArray);

    void SetWorldAll(const Matrix& world);
    void SetWorldAll();

    void SetWorld(const Matrix& matrix)
    {
        SetWorldMatrix(matrix);
        SetWorldAll();
    }

    void SetWorld(const Quaternion& _quaternion, const Vector& _position)
    {
        SetWorldMatrix(_quaternion, _position);
        SetWorldAll();
    }

    void SetWorld(const Quaternion& _quaternion)
    {
        SetWorldMatrix(_quaternion);
        SetWorldAll();
    }

    void SetWorld(const Vector& _position)
    {
        SetWorldMatrix(_position);
        SetWorldAll();
    }

    // display
    //
    virtual void Render()
    {
    }

    virtual void Render
    (
        const Array<FamilyState>& stateArray, Color teamColor, U32 clipFlags = clipALL,
        U32 _controlFlags = controlDEF
    );
    virtual void RenderColor
    (
        const Array<FamilyState>& stateArray, Color color, U32 clipFlags = clipALL,
        U32 _controlFlags = controlDEF
    );

    virtual void RenderSingle(Color teamColor = 0xffffffff, U32 _controlFlags = controlDEF)
    {
        teamColor;
        _controlFlags;
    }

    // search & info
    //
    //  if local == TRUE then find won't cross to attached objs'
    //
    MeshEnt* FindMeshEnt(Bool local = FALSE);

    //  if local == TRUE then find won't cross to attached objs'
    //
    FamilyNode* Find(U32 crc, Bool local = FALSE);

    Mesh* FindMesh(U32 crc, Bool local = FALSE)
    {
        FamilyNode* node = Find(crc, local);

        return node && (nodeType == nodeMesh || nodeType == nodeMeshRoot) ? (Mesh*)node : nullptr;
    }

    MeshObj* FindMeshObj(U32 crc, Bool local = FALSE)
    {
        FamilyNode* node = Find(crc, local);

        return node && (nodeType == nodeMeshObj || nodeType == nodeMeshEnt) ? (MeshObj*)node : nullptr;
    }

    FamilyNode* Find(const char* _name, Bool local = FALSE)
    {
        return Find(Crc::CalcStr(_name), local);
    }

    Mesh* FindMesh(const char* _name, Bool local = FALSE)
    {
        return FindMesh(Crc::CalcStr(_name), local);
    }

    MeshObj* FindMeshObj(const char* _name, Bool local = FALSE)
    {
        return FindMeshObj(Crc::CalcStr(_name), local);
    }

    // local = TRUE inline overloads
    //
    FamilyNode* FindLocal(U32 crc)
    {
        return Find(crc, TRUE);
    }

    FamilyNode* FindLocal(const char* _name)
    {
        return FindLocal(Crc::CalcStr(_name));
    }

    Mesh* FindMeshLocal(U32 crc)
    {
        return FindMesh(crc, TRUE);
    }

    Mesh* FindMeshLocal(const char* _name)
    {
        return FindMeshLocal(Crc::CalcStr(_name));
    }

    MeshObj* FindMeshObjLocal(U32 crc)
    {
        return FindMeshObj(crc, TRUE);
    }

    MeshObj* FindMeshObjLocal(const char* _name)
    {
        return FindMeshObjLocal(Crc::CalcStr(_name));
    }

    // must call matrix.ClearData() before calling
    //
    Bool FindOffset(const FamilyNode* node, Matrix& matrix, Bool local = FALSE);
    FamilyNode* FindOffset(const char* _name, Matrix& matrix, Bool local = FALSE);

    // local = TRUE inline overloads
    //
    Bool FindOffsetLocal(const FamilyNode* node, Matrix& matrix)
    {
        return FindOffset(node, matrix, TRUE);
    }

    FamilyNode* FindOffsetLocal(const char* _name, Matrix& matrix)
    {
        return FindOffset(_name, matrix, TRUE);
    }

    // debugging info
    //
    // MAXMESHPERGROUP is maxsize of 'names'
    //
    U32 GetHierarchy
    (
        BuffString* names, U32& count, Bool local = FALSE, U32 tabCount = 0,
        Matrix* matrix = nullptr
    ) const;

    U32 GetHierarchy(BuffString* names, Bool local = FALSE) const
    {
        U32 count = 0;
        return GetHierarchy(names, count, local);
    }

    void LogHierarchy(Bool local = FALSE) const
    {
        GetHierarchy(nullptr, local);
    }

    void DumpHierarchy(Bool local = FALSE) const
    {
        U32 count = 0;
        Matrix matrix;
        matrix.ClearData();
        GetHierarchy(nullptr, count, local, 0, &matrix);
    }
};

//----------------------------------------------------------------------------

//class FamilyState : public AnimKey
class FamilyState : protected AnimKey
{
protected:
    friend class MeshRoot;
    friend class MeshEnt;
    friend class MapObj;
    friend class Animation;
    friend class AnimList;

    friend const Matrix& FamilyNode::ObjectMatrix() const;
    friend const Matrix& FamilyNode::WorldMatrix() const;
    friend void FamilyNode::SetWorldRecurse(const Matrix& world);
    friend void FamilyNode::SetWorldRecurseRender(const Matrix& world, FamilyState* stateArray);
    friend inline void FamilyNode::CalcWorldMatrix(const Matrix& world);
    friend inline void FamilyNode::CalcWorldMatrix(const Matrix& world, FamilyState& state);

    Matrix objectMatrix;     // cached
    Matrix worldMatrix;

    FamilyNode* node;             // instance

    const Matrix& ObjectMatrix() const
    {
        return objectMatrix;
    }

public:

    void ClearData();

    FamilyState()
    {
        ClearData();
    }

    const Matrix& WorldMatrix() const
    {
        return worldMatrix;
    }

    const Matrix& ObjectMatrixPriv() const    // can't set everybody up as friends
    {
        return objectMatrix;
    }

    void operator=(const AnimKey& key)
    {
        *static_cast<AnimKey*>(this) = key;
    }

    // AnimKey overloads
    //
    U32 GetAnimType() const
    {
        return AnimKey::GetAnimType();
    }

    F32 GetFrame() const
    {
        return AnimKey::GetFrame();
    }

    void SetAnimType(AnimKeyType _type)
    {
        AnimKey::SetAnimType(_type);
    }

    F32 SetFrame(F32 _frame)
    {
        AnimKey::SetFrame(_frame);
    }

    const Quaternion& GetRotation() const
    {
        return AnimKey::GetRotation();
    }

    const Vector& GetPosition() const
    {
        return AnimKey::GetPosition();
    }

    const Vector& GetScale() const
    {
        return AnimKey::GetScale();
    }

    void Set(const Matrix& matrix)
    {
        AnimKey::Set(matrix);
    }

    void Set(const Quaternion& _quaternion, const Vector& _position)
    {
        AnimKey::Set(_quaternion, _position);
    }

    void Set(const Quaternion& _quaternion)
    {
        AnimKey::Set(_quaternion);
    }

    void Set(const Vector& _position)
    {
        AnimKey::Set(_position);
    }

    void SetScale(const Vector& _scale)
    {
        AnimKey::SetScale(_scale);
    }

    void Set(const AnimKey& src)
    {
        AnimKey::Set(src);
    }

    FamilyNode& GetNode() const
    {
        ASSERT(node);
        return *node;
    }

    const NodeIdent& GetIdent() const
    {
        ASSERT(node);
        return GetNode().GetIdent();
    }

    const char* GetName() const
    {
        ASSERT(node);
        return GetNode().GetName();
    }

    void SetNode(const FamilyNode& _node);

    void SetNode(const FamilyState& _state)
    {
        SetNode(_state.GetNode());
    }

    void SetState(FamilyState& _state) const
    {
        GetNode().SetState(_state);
    }

    void SetState(FamilyState& _state, U32 index) const
    {
        GetNode().SetState(_state, index);
    }

    //
    //
    void SetObject()
    {
        objectMatrix.Set(quaternion);
        objectMatrix.Set(position);
    }

    void SetObjectScale()
    {
        objectMatrix.SetScale(scale);
    }

    void SetObject(const AnimKey& src)
    {
        objectMatrix.Set(src.quaternion);
        objectMatrix.Set(src.position);
        objectMatrix.SetScale(src.scale);
    }

    void SetWorld()
    {
        worldMatrix.Set(quaternion);
        worldMatrix.Set(position);
    }

    void SetWorldScale()
    {
        worldMatrix.SetScale(scale);
    }

    void SetWorld(const AnimKey& src)
    {
        worldMatrix.Set(src.quaternion);
        worldMatrix.Set(src.position);
        worldMatrix.SetScale(src.scale);
    }

    void SetSlaveObject(const AnimKey& master)
    {
        quaternion = master.quaternion;
        position = master.position;
        scale = master.scale;

        SetObject();
        SetObjectScale();
    }

    void SetSlaveWorld(const AnimKey& master)
    {
        quaternion = master.quaternion;
        position = master.position;
        scale = master.scale;

        SetWorld();
        SetWorldScale();
    }

    FamilyNode* Node() const
    {
        return node;
    }

    Mesh* GetMeshFromRoot() const
    {
        return (Mesh*)node;
    }

    inline Bool IsTread() const;    // in mesh.h; only use on Mesh's
    inline Bool IsControl() const;

    MeshObj* GetMeshObj() const
    {
        return (MeshObj*)node;
    }

    inline Mesh* GetMesh() const;    // in meshent.h; only use on MeshEnts's

    void SetObjectMatrix(const Matrix& matrix)
    {
        AnimKey::Set(matrix);
        SetObject();
    }

    void SetObjectMatrix(const Quaternion& _quaternion)
    {
        AnimKey::Set(_quaternion);
        SetObject();
    }

    void SetObjectMatrix(const Vector& _position)
    {
        AnimKey::Set(_position);
        SetObject();
    }

    void SetObjectMatrix(const Quaternion& _quaternion, const Vector& _position)
    {
        AnimKey::Set(_quaternion, _position);
        SetObject();
    }

    void SetObjectMatrixScale(const Vector& _scale)
    {
        AnimKey::Set(_scale);
        SetObjectScale();
    }

    void SetObjectMatrix(const AnimKey& key)
    {
        AnimKey::Set(key);
        SetObject();
        SetObjectScale();
    }

    void PropagateObjectMatrix(const FamilyState& parentState)
    {
        SetObjectMatrix(ObjectMatrix() * parentState.ObjectMatrix());
    }

    void SetWorldMatrix(const Matrix& matrix)
    {
        quaternion.Set(matrix);
        position = matrix.posit;
        worldMatrix = matrix;
    }

    void SetWorldMatrix(const Quaternion& _quaternion, const Vector& _position)
    {
        quaternion = _quaternion;
        position = _position;
        worldMatrix.Set(quaternion);
        worldMatrix.posit = position;
    }

    void SetWorldMatrix(const Quaternion& _quaternion)
    {
        quaternion = _quaternion;
        worldMatrix.Set(quaternion);
    }

    void SetWorldMatrix(const Vector& _position)
    {
        position = _position;
        worldMatrix.posit = position;
    }

    void SetWorldMatrixScale(const Vector& _scale)
    {
        scale = _scale;
        worldMatrix.SetScale(scale);
    }
};

//----------------------------------------------------------------------------

// FamilyNode's FamilyState (state) overloads
//
inline void FamilyNode::CalcWorldMatrix(const Matrix& world)
{
    ASSERT(statePtr);
    statePtr->worldMatrix = parent ? statePtr->objectMatrix * world : world;   // FIXME
}

inline void FamilyNode::CalcWorldMatrix(const Matrix& world, FamilyState& state)
{
    ASSERT(statePtr);
    state.worldMatrix = parent ? state.objectMatrix * world : world;   // FIXME
}

inline void FamilyNode::SetState(const FamilyState& _state)
{
    statePtr = (FamilyState*)&_state;
    statePtr->SetNode(*this);
}

inline void FamilyNode::SetState(const FamilyState& _state, U32 index)
{
    statePtr = (FamilyState*)&_state;
    statePtr->SetNode(*this);
    SetIndex(index);
}

inline void FamilyNode::SetState(const char* _name, const FamilyState& _state, U32 index)
{
    statePtr = (FamilyState*)&_state;
    statePtr->SetNode(*this);
    SetName(_name);
    SetIndex(index);
}

inline void FamilyNode::ClearState()
{
    ASSERT(statePtr);
    statePtr->ClearData();
}

inline const Matrix& FamilyNode::ObjectMatrix() const
{
    ASSERT(statePtr);
    return statePtr->ObjectMatrix();
}

inline const Matrix& FamilyNode::WorldMatrix() const
{
    ASSERT(statePtr);
    return statePtr->WorldMatrix();
}

inline void FamilyNode::SetObjectMatrix(const AnimKey& key)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(key);
}

inline void FamilyNode::SetObjectMatrix(const Matrix& matrix)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(matrix);
}

inline void FamilyNode::SetObjectMatrix(const Quaternion& _quaternion, const Vector& _position)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(_quaternion, _position);
}

inline void FamilyNode::SetObjectMatrix(const Quaternion& _quaternion)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(_quaternion);
}

inline void FamilyNode::SetObjectMatrix(const Vector& _position)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(_position);
}

inline void FamilyNode::SetObjectMatrixScale(const Vector& _scale)
{
    ASSERT(statePtr);
    statePtr->SetObjectMatrix(_scale);
}

inline void FamilyNode::SetWorldMatrix(const Matrix& matrix)
{
    ASSERT(statePtr);
    statePtr->SetWorldMatrix(matrix);
}

inline void FamilyNode::SetWorldMatrix(const Quaternion& _quaternion, const Vector& _position)
{
    ASSERT(statePtr);
    statePtr->SetWorldMatrix(_quaternion, _position);
}

inline void FamilyNode::SetWorldMatrix(const Quaternion& _quaternion)
{
    ASSERT(statePtr);
    statePtr->SetWorldMatrix(_quaternion);
}

inline void FamilyNode::SetWorldMatrix(const Vector& _position)
{
    ASSERT(statePtr);
    statePtr->SetWorldMatrix(_position);
}

inline void FamilyNode::SetWorldMatrixScale(const Vector& _scale)
{
    ASSERT(statePtr);
    statePtr->SetWorldMatrixScale(_scale);
}

inline void AnimKey::operator=(const FamilyState& state)
{
    *static_cast<AnimKey*>(this) = *((AnimKey*)&state);
}

//----------------------------------------------------------------------------

#endif			// FAMILY_H
