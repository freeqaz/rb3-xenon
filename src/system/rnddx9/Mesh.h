#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rnddx9/Object.h"
#include "rndobj/Mesh.h"
#include "utl/PoolAlloc.h"
#include "xdk/D3D9.h"

class DxMesh : public RndMesh, public DxObject {
public:
    struct VertexBufferData {
        VertexBufferData() : buffer(0), size(0) {}
        ~VertexBufferData() { Release(); }
        void Release();

        D3DVertexBuffer *buffer;
        unsigned int size;
    };
    // Hmx::Object
    virtual ~DxMesh();
    OBJ_CLASSNAME(Mesh)
    OBJ_SET_TYPE_ENGINE(Mesh)
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    // RndMesh
    virtual void DrawShowing();
    virtual void DrawFacesInRange(int, int);
    // ⚠ These MUST use MESH_DC3_VIRTUAL, not a bare `virtual`. rndobj/Mesh.h
    // already proved (three vcall-displacement anchors) that retail keeps
    // RndMesh::NumFaces/NumVerts NON-virtual, but leaving the keyword HERE
    // re-introduced them as two BRAND-NEW virtuals appended to DxMesh's tail:
    // retail's DxMesh RndDrawable-subobject vtable @0x82101b14 (COL.offset 0,
    // the primary) is SIXTEEN slots -- slot 16 would be 0x82101b54, which holds
    // 0xfffffffc and is not an image VA at all, so the bound is hard -- while
    // ours emitted EIGHTEEN, the extra two being ?NumFaces@DxMesh@@UBAHXZ and
    // ?NumVerts@DxMesh@@UBAHXZ.
    //
    // Slots 0..15 align one-for-one with retail by BODY, no map name involved:
    // [3] is the `li r3,0; blr` hub (CamOverride returns 0), [6]/[10]/[11] are
    // the bare-`blr` hub (the three empty void virtuals ListDrawChildren /
    // DrawPreClear / UpdatePreClearState), [5]/[14]/[15] are referenced by
    // exactly ONE vtable each (the genuine DxMesh overrides DrawShowing /
    // DrawFacesInRange / OnSync) and [0][1][2][4][7][8][12][13] by exactly two
    // (RndMesh's own bodies). So the surplus is unambiguously the tail pair.
    MESH_DC3_VIRTUAL int NumFaces() const { return mNumFaces; }
    MESH_DC3_VIRTUAL int NumVerts() const { return mNumVerts; }
    virtual void OnSync(int);

    D3DVertexBuffer *GetMultimeshFaces();
    u32 VertFVF() const;

    NEW_OBJ(DxMesh)

    POOL_OVERLOAD(DxMesh, 0x56);

protected:
    DxMesh();

    static D3DVertexDeclaration *sVertexDecl;
    static D3DVertexDeclaration *sMutableVertexDecl;
    static D3DVertexDeclaration *sMutableSkinnedVertexDecl;

    std::vector<Transform> unk190;
    int mNumVerts; // 0x15c
    int mNumFaces; // 0x160
    VertexBufferData unk1a4;
    D3DResource *unk1ac;
    D3DResource *unk1b0;
};
