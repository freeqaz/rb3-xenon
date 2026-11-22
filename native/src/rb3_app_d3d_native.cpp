// rb3-app: native D3D textures, backed by engine RndTex objects (lane profpic,
// 2026-11-22). Native only, rb3-app only.
//
// Who calls these: ProfilePicture's gamer-picture fetch, the only D3D texture
// user rb3-app links (rnddx9/ is not compiled natively). Retail clean TU5:
//   * FetchUserPicture (0x82B903B8) creates the texture UNCONDITIONALLY, before
//     it starts the read: D3DDevice_CreateTexture(64, 64, 1, 1, 0,
//     D3DFMT_LIN_A8R8G8B8 0x18280086, 0, D3DRTYPE_TEXTURE 3) (bl at 0x82B9040C),
//     then LockRect level 0 (0x82B9044C), zero-fills Pitch*64 bytes
//     (0x82B90460) and hands pBits/Pitch to XUserReadGamerPicture (0x82B9047C).
//     The read is written INTO the texture, so there is no texture-less path.
//     A null texture only skips the read (the E_OUTOFMEMORY test at
//     0x82B90428), and the next Poll's ReceiveUserPicture would then unlock it.
//   * ReceiveUserPicture (0x82B904E0) unlocks (0x82B90540), relocks read-only
//     (flags 0x10, 0x82B90560), copies the 64x64 32 bpp pixels into a
//     RndBitmap (0x82B90598), unlocks (0x82B905A4) and Releases (0x82B90600).
//   * FetchUserPicture's failed-read path unlocks (0x82B90490) and Releases
//     (0x82B904A0).
//
// The engine's native texture is an RndTex whose pixels are its RndBitmap;
// RndTex::SyncBitmap uploads that bitmap to the GPU (milo-native-engine
// Rnd_Wgpu_RB3.cpp / Tex_Wgpu.cpp). A D3D texture here is that same object:
// CreateTexture makes an RndTex holding a bitmap of the requested size,
// LockRect hands out the bitmap's pixels and row pitch, an UnlockRect that
// ends a writable lock calls SyncBitmap (the engine's "pixels changed" hook),
// and the last Release deletes the RndTex. There is no second pixel store.
//
// Only the shape retail's callers use is modelled: a 2D, one-level,
// linear A8R8G8B8 texture, whole-surface locks of level 0. Any other request
// aborts through rb3_app_stub_reached (ring xbox-d3d), as the generated stubs
// did, instead of answering with something retail never asked for.
#include <cstdio>
#include <cstring>
#include <map>
#include "obj/Object.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "xdk/d3d9i/d3d9.h"

extern "C" [[noreturn]] void rb3_app_stub_reached(const char *ring, const char *fn);

namespace {
const DWORD kD3DLockReadOnly = 0x10; // D3DLOCK_READONLY

struct NativeD3DTexture {
    D3DTexture header; // the pointer the caller holds (first member)
    RndTex *tex;       // the engine texture whose bitmap holds the pixels
    bool locked;
    bool writeLocked;
};

std::map<D3DResource *, NativeD3DTexture *> &Textures() {
    static std::map<D3DResource *, NativeD3DTexture *> sTextures;
    return sTextures;
}

NativeD3DTexture *Lookup(D3DResource *r, const char *fn) {
    auto it = Textures().find(r);
    if (it == Textures().end()) {
        fprintf(stderr, "rb3-app: %s on %p, which rb3_app_d3d_native.cpp did not create\n", fn,
                (void *)r);
        rb3_app_stub_reached("xbox-d3d", fn);
    }
    return it->second;
}
}

// Live textures, for the ProfilePicture gate (rb3_app_gates.cpp).
extern "C" int rb3_app_d3d_live_textures() { return (int)Textures().size(); } // ring: native-real

D3DBaseTexture *D3DDevice_CreateTexture( // ring: native-real
    UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT D3DFormat, UINT,
    D3DRESOURCETYPE D3DType
) {
    if (D3DType != D3DRTYPE_TEXTURE || Depth != 1 || Levels != 1 || Usage != 0
        || (DWORD)D3DFormat != 0x18280086u /* D3DFMT_LIN_A8R8G8B8 */ || Width == 0
        || Height == 0) {
        fprintf(stderr,
                "rb3-app: D3DDevice_CreateTexture(%u x %u, depth %u, levels %u, usage 0x%x, "
                "format 0x%x, type %d) is not the 2D linear A8R8G8B8 texture this models\n",
                Width, Height, Depth, Levels, (unsigned)Usage, (unsigned)D3DFormat, (int)D3DType);
        rb3_app_stub_reached("xbox-d3d", "D3DDevice_CreateTexture");
    }
    RndBitmap bmp;
    bmp.Create(Width, Height, 0, 32, 0, nullptr, nullptr, nullptr);
    memset(bmp.Pixels(), 0, bmp.PixelBytes());
    NativeD3DTexture *t = new NativeD3DTexture();
    memset(&t->header, 0, sizeof(t->header));
    t->header.ReferenceCount = 1;
    t->tex = Hmx::Object::New<RndTex>();
    t->tex->SetBitmap(bmp, nullptr, true); // keepFormat: 32 bpp, order 0, as created
    t->locked = false;
    t->writeLocked = false;
    Textures()[&t->header] = t;
    return &t->header;
}

VOID D3DTexture_LockRect( // ring: native-real
    D3DTexture *pTexture, UINT Level, D3DLOCKED_RECT *pLockedRect, const tagRECT *pRect,
    DWORD Flags
) {
    NativeD3DTexture *t = Lookup(pTexture, "D3DTexture_LockRect");
    if (Level != 0 || pRect || t->locked) {
        fprintf(stderr, "rb3-app: D3DTexture_LockRect(level %u, %s rect%s) is not modelled\n",
                Level, pRect ? "a sub" : "the whole", t->locked ? ", already locked" : "");
        rb3_app_stub_reached("xbox-d3d", "D3DTexture_LockRect");
    }
    const RndBitmap &bmp = t->tex->Bitmap();
    pLockedRect->Pitch = bmp.RowBytes();
    pLockedRect->pBits = bmp.Pixels();
    t->locked = true;
    t->writeLocked = !(Flags & kD3DLockReadOnly);
}

VOID D3DTexture_UnlockRect(D3DTexture *pTexture, UINT Level) { // ring: native-real
    NativeD3DTexture *t = Lookup(pTexture, "D3DTexture_UnlockRect");
    if (Level != 0 || !t->locked) {
        fprintf(stderr, "rb3-app: D3DTexture_UnlockRect(level %u) of a texture that is %s\n",
                Level, t->locked ? "locked" : "not locked");
        rb3_app_stub_reached("xbox-d3d", "D3DTexture_UnlockRect");
    }
    if (t->writeLocked) t->tex->SyncBitmap();
    t->locked = false;
    t->writeLocked = false;
}

ULONG D3DResource_Release(D3DResource *pResource) { // ring: native-real
    NativeD3DTexture *t = Lookup(pResource, "D3DResource_Release");
    ULONG refs = --t->header.ReferenceCount;
    if (refs == 0) {
        Textures().erase(pResource);
        delete t->tex;
        delete t;
    }
    return refs;
}
