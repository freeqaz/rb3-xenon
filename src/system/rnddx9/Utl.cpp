#pragma once
#include "Utl.h"
#include "rnddx9/Rnd.h"
#include "xdk/D3D9.h"
#include <cstring>

// RB3 retail (TU5) has NO DX_ASSERT at the four allocation sites in this TU:
// MakeVertexBuffer/MakeIndexBuffer are bare 20 B tail calls (`b D3DDevice_Create*Buffer`,
// retail 0x8273D658 / 0x8273D670) and Clone*Buffer go straight from the Create call into
// the BufLock ctors (0x8273D688 / 0x8273D728). DC3 (newer) added the DX_ASSERTs; keeping
// them here expanded each function by ~30 instructions (DxRnd::Error + MakeString +
// Debug::Fail) and flipped the in/out callee-saved pair. Lane W16-AC, 2026-09-14.

struct D3DVertexBuffer *MakeVertexBuffer(int num, uint size, uint, bool) {
    MILO_ASSERT(num > 0, 19);
    MILO_ASSERT(size != 0, 20);

    struct D3DVertexBuffer *vb =
        D3DDevice_CreateVertexBuffer(num * size, 0, D3DPOOL_DEFAULT);
    return vb;
}

struct D3DIndexBuffer *MakeIndexBuffer(int num, uint size, D3DFORMAT fmt) {
    MILO_ASSERT(num > 0, 60);
    MILO_ASSERT(size != 0, 61);
    MILO_ASSERT(fmt == D3DFMT_INDEX16 || fmt == D3DFMT_INDEX32, 62);

    struct D3DIndexBuffer *ib =
        D3DDevice_CreateIndexBuffer(num * size, 8, fmt, D3DPOOL_MANAGED);
    return ib;
}

struct D3DVertexBuffer *CloneVertexBuffer(struct D3DVertexBuffer *in) {
    if (in == nullptr)
        return in;
    D3DVERTEXBUFFER_DESC desc;
    D3DVertexBuffer_GetDesc(in, &desc);
    struct D3DVertexBuffer *out =
        D3DDevice_CreateVertexBuffer(desc.Size, desc.Usage, desc.Pool);
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
    IBLock<> lock_in(in, 0);
    IBLock<> lock_out(out, 0);
    memcpy(lock_out.mDataAddr, lock_in.mDataAddr, desc.Size);
    return out;
}
