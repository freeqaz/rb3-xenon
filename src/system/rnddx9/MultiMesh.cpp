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
#include "Memory.h"

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

void DxMultiMesh::Init() {
    REGISTER_OBJ_FACTORY(DxMultiMesh);
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
    sVertexDecl = D3DDevice_CreateVertexDeclaration(sVertexElement);
    {
        HRESULT hr = sVertexDecl != nullptr ? 0 : 0x8007000E;
        if (hr) {
            MILO_FAIL("File: %s Line: %d Error: %s\n", __FILE__, 0x97, DxRnd::Error(hr));
        }
    }
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
    sMutableVertexDecl = D3DDevice_CreateVertexDeclaration(sMutableVertexElement);
    {
        HRESULT hr = sMutableVertexDecl != nullptr ? 0 : 0x8007000E;
        if (hr) {
            MILO_FAIL("File: %s Line: %d Error: %s\n", __FILE__, 0x9A, DxRnd::Error(hr));
        }
    }
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

void DxMultiMesh::UpdateGeometryBuffers() {
    // Register variables ordered to match calling conventions
    u32 var_r9;
    s32 var_r10;
    void *temp_r3_3;
    void *temp_r11;
    void *temp_r11_2;
    void *temp_r11_3;
    s32 temp_r24;
    s32 temp_r28_2;
    s32 temp_r3;
    DxMesh *temp_r30_ptr;
    s32 temp_r28;
    s32 temp_r10;
    void *temp_r11_4;
    s32 temp_r3_2;
    void *temp_r27;
    Symbol sym("D3D(phys)3Mesh");
    PhysMemTypeTracker tracker(sym);
    s32 temp_r8;
    void *var_r3;
    s32 temp_r23;
    void *temp_r30;

    u16 temp_r8_2;
    void *temp_r27_ptr;

    temp_r30_ptr = *(DxMesh **)((char *)this + 0x4C);
    temp_r30 = (void *)temp_r30_ptr;
    temp_r11 = (void *)((char *)temp_r30 + 0x150);
    temp_r27_ptr = *(void **)((char *)temp_r30 + 0x148);
    temp_r27 = temp_r27_ptr;

    MILO_ASSERT(!(*(u32 *)((char *)temp_r30 + 0x150) != *(u32 *)((char *)temp_r30 + 0x154)),
               0x21A);

    MILO_ASSERT(!(*(s32 *)((char *)temp_r27 + 0x160) == 0), 0x21B);

    temp_r24 = *(u32 *)((char *)this + 0x60) % 3;
    temp_r28 = (temp_r24 + 0x19) * 4;

    if (*(void **)((char *)this + temp_r28) == nullptr) {
        temp_r23 = *(s32 *)((char *)temp_r27 + 0x104);
        temp_r30_ptr->VertFVF();
        temp_r3 = (s32)D3DDevice_CreateVertexBuffer(temp_r23 * 0x60, 0, (D3DPOOL)0);
        *(s32 *)((char *)this + temp_r28) = temp_r3;
        temp_r10 = temp_r3 - 1;
        temp_r3_2 = ((temp_r10 - temp_r10) - (temp_r10 == 0 ? 1 : 0)) & 0x8007000E;
        if (temp_r3_2 != 0) {
            const char *errMsg = DxRnd::Error(temp_r3_2);
            MILO_FAIL("File: %s Line: %d Error: %s\n", __FILE__, 0x225, errMsg);
        }
    }

    void *bufPtr = *(void **)((char *)this + temp_r28);
    D3DVertexBuffer *vertBuf = (D3DVertexBuffer *)bufPtr;
    BufLock<D3DVertexBuffer> bufLock(vertBuf, 0);

    temp_r3 = *(s32 *)((char *)temp_r27 + 0x104);
    void *srcData = *(void **)((char *)temp_r27 + 0x100);
    void *dstData = bufLock.mDataAddr;

    memcpy(dstData, srcData, temp_r3 * 0x60);

    temp_r11_2 = *(void **)((char *)temp_r30 + 0x148);
    temp_r28_2 = (temp_r24 + 0x1C) * 4;
    temp_r11 = (void *)((char *)temp_r11_2 + 0x110);

    if (*(void **)((char *)this + temp_r28_2) == nullptr) {
        s32 indexSize = ((*(s32 *)((char *)temp_r11_2 + 0x114) -
                          *(s32 *)((char *)temp_r11_2 + 0x110)) / 6) * 0xC;
        void *vb2Ptr = D3DDevice_CreateVertexBuffer(indexSize, 0, (D3DPOOL)0);
        *(void **)((char *)this + temp_r28_2) = vb2Ptr;
    }

    auto _tmp0 = D3DVertexBuffer_Lock((D3DVertexBuffer *)*(void **)((char *)this + temp_r28_2), 0, 0, 0);
    var_r3 = _tmp0;

    temp_r11_3 = *(void **)((char *)temp_r30 + 0x148);
    var_r9 = 0;

    if ((*(s32 *)((char *)temp_r11_3 + 0x114) -
                     *(s32 *)((char *)temp_r11_3 + 0x110)) / 6 != 0) {
        var_r10 = 0;
        u32 indexCount = (u32)((*(s32 *)((char *)temp_r11_3 + 0x114) -
                                  *(s32 *)((char *)temp_r11_3 + 0x110)) / 6);
        do {
            temp_r8 = *(s32 *)((char *)temp_r11_3 + 0x110);
            var_r9++;
            temp_r11_4 = (void *)(var_r10 + temp_r8);
            temp_r8_2 = *(u16 *)((char *)temp_r11_4 + 0);
            var_r10 += 6;
            *(s32 *)((char *)var_r3 + 0) = (s32)temp_r8_2;
            temp_r3_3 = (void *)((char *)var_r3 + 4);
            *(s32 *)((char *)var_r3 + 4) = (s32)*(u16 *)((char *)temp_r11_4 + 2);
            *(s32 *)((char *)temp_r3_3 + 4) = (s32)*(u16 *)((char *)temp_r11_4 + 4);
            temp_r11_3 = *(void **)((char *)temp_r30 + 0x148);
            var_r3 = (void *)((char *)temp_r3_3 + 8);
        } while (var_r9 != indexCount);
    }

    D3DVertexBuffer_Unlock((D3DVertexBuffer *)*(void **)((char *)this + temp_r28_2));
}
