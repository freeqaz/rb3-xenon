#pragma once
#include "obj/Object.h"
#include "rndobj/MultiMesh.h"
#include "xdk/D3D9.h"

class DxMultiMesh : public RndMultiMesh {
public:
    DxMultiMesh();
    virtual ~DxMultiMesh();
    OBJ_CLASSNAME(MultiMesh);
    OBJ_SET_TYPE(MultiMesh);
    virtual void DrawShowing();

    static void Init();
    static void Shutdown();
    NEW_OBJ(DxMultiMesh)

private:
    void UpdateGeometryBuffers();
    void DrawBatchedNewGfx();

    static D3DVertexDeclaration *sVertexDecl;
    static D3DVertexDeclaration *sMutableVertexDecl;

    int mGeomDirtyFlags;                  // 0x38
    int mBufferCycleIndex;                 // 0x3c - cycles through 3 buffers
    D3DVertexBuffer *mVertexBuffers[3];    // 0x40 - triple-buffered vertex data
    D3DVertexBuffer *mIndexBuffers[3];     // 0x4c - triple-buffered index data
};
