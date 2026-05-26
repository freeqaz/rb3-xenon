#pragma once
#include "Utl.h"
#include "rnddx9/Rnd.h"
#include "xdk/D3D9.h"
#include <cstring>

struct D3DVertexBuffer *MakeVertexBuffer(int num, uint size, uint, bool) {
    MILO_ASSERT(num > 0, 19);
    MILO_ASSERT(size != 0, 20);

    struct D3DVertexBuffer *vb =
        D3DDevice_CreateVertexBuffer(num * size, 0, D3DPOOL_DEFAULT);
    DX_ASSERT(vb, 0x22);
    return vb;
}

struct D3DIndexBuffer *MakeIndexBuffer(int num, uint size, D3DFORMAT fmt) {
    MILO_ASSERT(num > 0, 60);
    MILO_ASSERT(size != 0, 61);
    MILO_ASSERT(fmt == D3DFMT_INDEX16 || fmt == D3DFMT_INDEX32, 62);

    struct D3DIndexBuffer *ib =
        D3DDevice_CreateIndexBuffer(num * size, 8, fmt, D3DPOOL_MANAGED);
    DX_ASSERT(ib, 0x47);
    return ib;
}

struct D3DVertexBuffer *CloneVertexBuffer(struct D3DVertexBuffer *in) {
    if (in == nullptr)
        return in;
    D3DVERTEXBUFFER_DESC desc;
    D3DVertexBuffer_GetDesc(in, &desc);
    struct D3DVertexBuffer *out =
        D3DDevice_CreateVertexBuffer(desc.Size, desc.Usage, desc.Pool);
    DX_ASSERT(out, 49);
    VBLock<> lock_in(in, 0);
    VBLock<> lock_out(out, 0);
    memcpy(lock_out.mDataAddr, lock_in.mDataAddr, desc.Size);
    return out;
}

struct D3DIndexBuffer *CloneIndexBuffer(struct D3DIndexBuffer *in) {
    if (in == nullptr)
        return in;
    D3DINDEXBUFFER_DESC desc;
    D3DIndexBuffer_GetDesc(in, &desc);
    struct D3DIndexBuffer *out =
        D3DDevice_CreateIndexBuffer(desc.Size, desc.Usage, desc.Format, desc.Pool);
    DX_ASSERT(out, 86);
    IBLock<> lock_in(in, 0);
    IBLock<> lock_out(out, 0);
    memcpy(lock_out.mDataAddr, lock_in.mDataAddr, desc.Size);
    return out;
}
