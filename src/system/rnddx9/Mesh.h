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
    OBJ_SET_TYPE(Mesh)
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    // RndMesh
    virtual void DrawShowing();
    virtual void DrawFacesInRange(int, int);
    virtual int NumFaces() const { return mNumFaces; }
    virtual int NumVerts() const { return mNumVerts; }
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
    int mNumVerts; // 0x19c
    int mNumFaces; // 0x1a0
    VertexBufferData unk1a4;
    D3DResource *unk1ac;
    D3DResource *unk1b0;
};
