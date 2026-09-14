#include "rnddx9/Tex.h"
#include "Rnd.h"
#include "Tex.h"
#include "os/Debug.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/TexMgr.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"

std::vector<DxTex *> gAllTextures;

// Scratch/destination bookkeeping for the DXT compression path.  File-local in
// retail (no map row of its own); layout taken from the dc3 oracle
// (../dc3-decomp/src/system/rnddx9/Tex.cpp:29).
struct CompressLevel {
    D3DSurface *scratchSurface; // 0x0
    D3DLOCKED_RECT scratchLock; // 0x4
    D3DSURFACE_DESC scratchDesc; // 0xc
    D3DSurface *textureSurface; // 0x2c
    D3DLOCKED_RECT textureLock; // 0x30
    D3DSURFACE_DESC textureDesc; // 0x38
};

struct CompressDesc {
    D3DTexture *texture; // 0x0
    // ⚠ dc3 declares this `RndTex::AlphaCompress alpha` -- an enum, so 4 bytes,
    // which compiles to `lwz` + signed `cmpwi`.  RETAIL LOADS A BYTE:
    // `lbz r11,0x4(r30); cmplwi r11,0x0` at 0x82734148+29.  The field is
    // byte-sized and compared UNSIGNED, so the oracle is wrong about its width.
    // It still holds three values (0/1/2 -- StartCompress tests `== 2`), so it
    // is a u8, not a bool.  3 pad bytes follow; `unk8` stays at 0x8 either way.
    u8 alpha; // 0x4
    int unk8; // 0x8
    D3DFORMAT format; // 0xc
    void *tiledBuffer; // 0x10
    CompressLevel levels[16]; // 0x14
};

DxTex::DxTex()
    : mFormat((D3DFORMAT)-1), mTexture(0), unk84(0), mRenderTarget(0), mDepthRT(0),
      mMovieBufIdx(0), mLockedRect(), unka4(0), unka8(0), unkac(0) {
    gAllTextures.push_back(this);
    for (int i = 0; i < 2; i++) {
        mMovieTextures[i] = 0;
    }
}

DxTex::~DxTex() {
    ResetSurfaces();
    auto it = std::find(gAllTextures.begin(), gAllTextures.end(), this);
    MILO_ASSERT(it != gAllTextures.end(), 0x2D7);
    gAllTextures.erase(it);
}

void DxTex::Compress(AlphaCompress a) {
    void *v = StartCompress(a);
    DoCompress(v);
    FinishCompress(v);
}

// Retail 0x82734148 (284 B), named in the map, previously fuzzy 0 for want of
// any definition to pair with: DECLARED at rnddx9/Tex.h and defined in NO
// translation unit.  StartCompress / FinishCompress are undefined the same way
// but carry no named retail row (objdiff pairs by NAME, so a `fn_` row cannot
// pair with our mangled symbol) -- they are deliberately left undefined so this
// change measures exactly one thing.
// Ported from ../dc3-decomp/src/system/rnddx9/Tex.cpp:150.
void DxTex::DoCompress(void *p) {
    CompressDesc *desc = (CompressDesc *)p;
    int numLevels = D3DBaseTexture_GetLevelCount(mTexture);
    for (int i = 0; i < numLevels; i++) {
        CompressLevel &level = desc->levels[i];
        int rowPitch = level.scratchDesc.Width * 4;
        XGUntileTextureLevel(
            level.scratchDesc.Width,
            level.scratchDesc.Height,
            desc->unk8,
            mFormat & 0x3f,
            1,
            desc->tiledBuffer,
            rowPitch,
            nullptr,
            level.scratchLock.pBits,
            nullptr
        );
        if ((int)desc->alpha == 0) {
            unsigned int *texel = (unsigned int *)desc->tiledBuffer;
            unsigned int *end =
                texel + level.scratchDesc.Width * level.scratchDesc.Height;
            for (; texel < end; texel++) {
                *texel |= 0xff000000;
            }
        }
        XGCompressSurface(
            level.textureLock.pBits,
            level.textureLock.Pitch,
            level.textureDesc.Width,
            level.textureDesc.Height,
            desc->format,
            0,
            desc->tiledBuffer,
            rowPitch,
            (D3DFORMAT)0x18280086,
            0,
            0,
            0.5f
        );
    }
}

void DxTex::SetDeviceTex(D3DTexture *tex) {
    mTexture = tex;
    mType = kDeviceTexture;
    if (tex) {
        D3DSURFACE_DESC desc;
        D3DTexture_GetLevelDesc(tex, 0, &desc);
        mNumMips = 0;
        mFormat = desc.Format;
        mWidth = desc.Width;
        mHeight = desc.Height;
        mBpp = D3DFORMAT_BitsPerPixel(desc.Format);
    }
}

D3DSurface *DxTex::GetRT() {
    if (!IsRenderTarget()) {
        return nullptr;
    } else {
        D3DResource_AddRef(mRenderTarget);
        return mRenderTarget;
    }
}

D3DSurface *DxTex::GetDepthRT() { return mDepthRT; }

void DxTex::PreDeviceReset() {
    if (IsBackBuffer() || IsRenderTarget()) {
        ResetSurfaces();
    }
}

void DxTex::PostDeviceReset() {
    if (IsBackBuffer()) {
        SetBitmap(TheRnd.Width(), TheRnd.Height(), TheRnd.Bpp(), mType, false, nullptr);
    }
    if (IsRenderTarget()) {
        SyncBitmap();
    }
}

D3DSurface *DxTex::GetSurfaceLevel(int x) {
    D3DSurface *ret = D3DTexture_GetSurfaceLevel(mTexture, x);
    DX_ASSERT(ret, 0xE6);
    return ret;
}

unsigned int DxTex::TexelsPitch() const {
    D3DLOCKED_RECT rect;
    D3DTexture_LockRect(mTexture, 0, &rect, nullptr, 0);
    D3DTexture_UnlockRect(mTexture, 0);
    return rect.Pitch;
}

D3DSurface *DxTex::GetMovieSurface() {
    if (!(mType & kMovie)) {
        return nullptr;
    } else {
        mTexture = mMovieTextures[mMovieBufIdx];
        return GetSurfaceLevel(0);
    }
}

void DxTex::SwapMovieSurface() {
    MILO_ASSERT((mType & kMovie) > 0, 0x2F5);
    mMovieBufIdx = (mMovieBufIdx + 1) % 2;
    mTexture = mMovieTextures[mMovieBufIdx];
}

void DxTex::ResetSurfaces() {
    // Clean up movie surface double-buffer
    for (int i = 0; i < 2; i++) {
        if (mTexture == mMovieTextures[i]) {
            mTexture = nullptr;
        }
        TheDxRnd.AutoRelease(mMovieTextures[i]);
        mMovieTextures[i] = nullptr;
    }

    // Delete main texture for certain types
    bool _bit0 = (mType & kRendered) != 0;
    if (((_bit0) && mNumMips) || ((mType & kMovie) && (mType & 0x20))
        || (mType & kScratch) || (mType & kRegularLinear)) {
        TheDxRnd.AutoDelete(mTexture);
        mTexture = nullptr;
    }

    // Clear texture pointer for movie/scratch/device types (0x1204 = kMovie | kScratch | kDeviceTexture)
    if (mType & 0x1204) {
        mTexture = nullptr;
    }

    // Release managed texture resource.
    // ⚠ `unk2c` is the DC3-ONLY Hmx::CRC resource key at RndTex+0x2c that retail
    // RB3-360 does NOT have -- see the note at rndobj/Tex.h:156, which gates the
    // member behind RB3_RNDTEX_DC3_CRC and claims "the only use is the
    // COPY_MEMBER in Tex.cpp".  That claim was WRONG: this second use site was
    // never gated, and nothing caught it because Tex.cpp was wired into no
    // build at all.  Retail has no CRC key, so it cannot perform this lookup;
    // the ungated path is the plain release.  Gated identically to
    // rndobj/Tex.cpp:119 so a future native build that defines the macro keeps
    // the DC3 behaviour.
#ifdef RB3_RNDTEX_DC3_CRC
    if (!TheDxTexMgr.ReleaseRes(unk2c)) {
        TheDxRnd.AutoRelease(mTexture);
    }
#else
    TheDxRnd.AutoRelease(mTexture);
#endif
    mTexture = nullptr;

    // Clean up render target surfaces
    TheDxRnd.AutoRelease(mRenderTarget);
    mRenderTarget = nullptr;
    TheDxRnd.AutoRelease(mDepthRT);
    mDepthRT = nullptr;
}
