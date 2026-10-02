#include "rnddx9/MultiMesh.h"
#include "obj/Object.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/Mesh.h"
#include "rnddx9/Utl.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"
#include "utl/Symbol.h"
#include "os/Debug.h"
#include "../../Memory.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"

DxMultiMesh::DxMultiMesh() : mGeomDirtyFlags(0), mBufferCycleIndex(0) {
    for (int i = 0; i < 3; i++) {
        mVertexBuffers[i] = mIndexBuffers[i] = nullptr;
    }
}

DxMultiMesh::~DxMultiMesh() {
    for (int i = 0; i < 3; i++) {
        DX_RELEASE(mIndexBuffers[i]);
        DX_RELEASE(mVertexBuffers[i]);
    }
}

// File-scope (internal linkage): retail addresses both tables off one base
// register, the mutable one as sVertexElement + 0x70.
static D3DVERTEXELEMENT9 sVertexElement[] = {
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 20, D3DDECLTYPE_DEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 24, D3DDECLTYPE_DEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    { 0, 28, D3DDECLTYPE_UDEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 32, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    { 1, 0, D3DDECLTYPE_UINT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    D3DDECL_END()
};
static D3DVERTEXELEMENT9 sMutableVertexElement[] = {
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 64, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 72, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    { 0, 80, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    { 1, 0, D3DDECLTYPE_UINT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    D3DDECL_END()
};

void DxMultiMesh::Init() {
    REGISTER_OBJ_FACTORY(DxMultiMesh);
    // Retail checks neither creation.
    sVertexDecl = D3DDevice_CreateVertexDeclaration(sVertexElement);
    sMutableVertexDecl = D3DDevice_CreateVertexDeclaration(sMutableVertexElement);
}

void DxMultiMesh::Shutdown() {
    if (sVertexDecl) {
        D3DResource_Release(sVertexDecl);
        sVertexDecl = nullptr;
    }
    if (sMutableVertexDecl) {
        D3DResource_Release(sMutableVertexDecl);
        sMutableVertexDecl = nullptr;
    }
}

// 0x8273F1E8 (called only from DrawBatchedNewGfx). Uploads the geometry
// owner's vertices into this frame's cycled vertex buffer and its faces as
// 32-bit indices; the face count is re-read every iteration.
void DxMultiMesh::UpdateGeometryBuffers() {
    PhysMemTypeTracker tracker("D3D(phys):Mesh");
    unsigned int idx = (unsigned int)mBufferCycleIndex % 3;
    RndMesh *owner = mMesh->GetGeomOwner();
    RndMesh::VertVector &verts = owner->Verts();
    if (!mVertexBuffers[idx]) {
        mVertexBuffers[idx] =
            D3DDevice_CreateVertexBuffer(verts.size() * 0x60, 0, (D3DPOOL)0);
    }
    {
        BufLock<D3DVertexBuffer> lock(mVertexBuffers[idx], 0);
        memcpy(lock.mDataAddr, &verts[0], verts.size() * 0x60);
    }
    int indexCount = owner->Faces().size() * 3;
    if (!mIndexBuffers[idx]) {
        mIndexBuffers[idx] =
            D3DDevice_CreateVertexBuffer(indexCount * 4, 0, (D3DPOOL)0);
    }
    unsigned int *dst =
        (unsigned int *)D3DVertexBuffer_Lock(mIndexBuffers[idx], 0, 0, 0);
    for (unsigned int i = 0; i != owner->Faces().size(); i++) {
        RndMesh::Face &face = owner->Faces()[i];
        *dst = face.v1;
        *++dst = face.v2;
        *++dst = face.v3;
        dst++;
    }
    D3DVertexBuffer_Unlock(mIndexBuffers[idx]);
}

// 0x8273F370, called only from DrawShowing. RB3's batching is simpler than the
// later engine's: the instance transform is a local identity, every instance
// is drawn (no per-instance visibility test), a batch fills vertex-shader
// registers 0x5c up to a fixed 0x81-register span, and no draw stats are kept.
void DxMultiMesh::DrawBatchedNewGfx() {
    RndMesh *mesh = mMesh;
    DxMesh *owner = static_cast<DxMesh *>(mesh->GetGeomOwner());
    bool fastBillboard =
        mesh->TransConstraint() == RndTransformable::kConstraintFastBillboardXYZ;
    RndMat *mat = mesh->Mat();
    if (owner->Mutable()) {
        UpdateGeometryBuffers();
    }
    int numFaces;
    if (owner->Mutable()) {
        D3DDevice_SetStreamSource(
            TheDxRnd.Device(),
            0,
            mVertexBuffers[(unsigned int)mBufferCycleIndex % 3],
            0,
            0x60,
            1
        );
        D3DDevice_SetStreamSource(
            TheDxRnd.Device(), 1, mIndexBuffers[(unsigned int)mBufferCycleIndex % 3], 0, 4, 1
        );
        D3DDevice_SetVertexDeclaration(TheDxRnd.Device(), sMutableVertexDecl);
        numFaces = owner->GetGeomOwner()->Faces().size();
    } else {
        D3DDevice_SetStreamSource(TheDxRnd.Device(), 0, owner->unk1a4.buffer, 0, 0x24, 1);
        D3DDevice_SetStreamSource(
            TheDxRnd.Device(), 1, owner->GetMultimeshFaces(), 0, 4, 1
        );
        D3DDevice_SetVertexDeclaration(TheDxRnd.Device(), sVertexDecl);
        numFaces = owner->mNumFaces;
    }
    int vertsPerInstance = numFaces * 3;
    Vector4 instanceVerts;
    instanceVerts.x = vertsPerInstance;
    instanceVerts.y = vertsPerInstance;
    instanceVerts.z = vertsPerInstance;
    instanceVerts.w = vertsPerInstance;
    TheShaderMgr.SetVConstant((VShaderConstant)0x59, instanceVerts);

    ShaderType shader = fastBillboard ? kMultimeshBBShader : kMultimeshShader;
    do {
        Transform xfm;
        xfm.Reset();
        TheShaderMgr.SetTransform(xfm);
        RndShader::SelectConfig(mat, shader, false);
        InstanceList::iterator it = mInstances.begin();
        while (it != mInstances.end()) {
            int reg;
            for (reg = 0; reg < 0x81 && it != mInstances.end(); reg += 3, ++it) {
                TheShaderMgr.SetVConstant4x3(
                    (VShaderConstant)(reg + 0x5C), Hmx::Matrix4(it->mXfm)
                );
            }
            D3DDevice_DrawVertices(
                TheDxRnd.Device(), D3DPT_TRIANGLELIST, 0, reg / 3 * vertsPerInstance
            );
        }
        if (mat) {
            mat = mat->NextPass();
        }
    } while (mat);
    mBufferCycleIndex++;
}

// 0x8273F610 (DxMultiMesh vtable slot 5).
void DxMultiMesh::DrawShowing() {
    if (mInstances.empty())
        return;
    RndMesh *mesh = mMesh;
    // retail tests the mesh with a signed compare (cmpwi cr6,r11,0)
    if ((int)mesh == 0)
        return;
    if (!static_cast<DxMesh *>(mesh->GetGeomOwner())->CanDraw())
        return;
    Rnd::Mode mode = TheRnd.DrawMode();
    if (mode == Rnd::kDrawOcclusion)
        return;
    if (mode != Rnd::kDrawNormal)
        return;
    DrawBatchedNewGfx();
}
